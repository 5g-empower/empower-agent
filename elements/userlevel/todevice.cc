/*
 * todevice.{cc,hh} -- element writes packets to network via pcap library
 * Douglas S. J. De Couto, Eddie Kohler, John Jannotti
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>
#include "todevice.hh"
#include <click/error.hh>
#include <click/etheraddress.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/standard/scheduleinfo.hh>
#include <click/packet_anno.hh>

#include <stdio.h>
#include <unistd.h>

#if TODEVICE_BSD_DEV_BPF
# include <fcntl.h>
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
#elif TODEVICE_LINUX
# include <sys/socket.h>
# include <sys/ioctl.h>
# include <net/if.h>
# include <net/if_packet.h>
# include <features.h>
# if __GLIBC__ >= 2 && __GLIBC_MINOR__ >= 1
#  include <netpacket/packet.h>
# else
#  include <linux/if_packet.h>
# endif
#endif

CLICK_DECLS

ToDevice::ToDevice()
  : Element(1, 0), _task(this), _fd(-1), _my_fd(false), _set_error_anno(false),
    _ignore_q_errs(false), _printed_err(false)
{
  MOD_INC_USE_COUNT;
}

ToDevice::~ToDevice()
{
  MOD_DEC_USE_COUNT;
}

void
ToDevice::notify_noutputs(int n)
{
  set_noutputs(n <= 1 ? n : 1);
}

int
ToDevice::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (cp_va_parse(conf, this, errh,
		  cpString, "interface name", &_ifname,
		  cpKeywords,
		  "SET_ERROR_ANNO", cpBool, "set annotation on error packets?", &_set_error_anno,
		  "IGNORE_QUEUE_OVERFLOWS", cpBool, "ignore queue overflow errors?", &_ignore_q_errs,
		  cpEnd) < 0)
    return -1;
  if (!_ifname)
    return errh->error("interface not set");
  return 0;
}

int
ToDevice::initialize(ErrorHandler *errh)
{
  _fd = -1;

#if TODEVICE_BSD_DEV_BPF
  
  /* pcap_open_live() doesn't open for writing. */
  for (int i = 0; i < 16 && _fd < 0; i++) {
    char tmp[64];
    sprintf(tmp, "/dev/bpf%d", i);
    _fd = open(tmp, 1);
  }
  if (_fd < 0)
    return(errh->error("open /dev/bpf* for write: %s", strerror(errno)));

  struct ifreq ifr;
  strncpy(ifr.ifr_name, _ifname.c_str(), sizeof(ifr.ifr_name));
  ifr.ifr_name[sizeof(ifr.ifr_name) - 1] = 0;
  if (ioctl(_fd, BIOCSETIF, (caddr_t)&ifr) < 0)
    return errh->error("BIOCSETIF %s failed", ifr.ifr_name);
  _my_fd = true;

#elif TODEVICE_LINUX || TODEVICE_PCAP
  
  // find a FromDevice and reuse its socket if possible
  for (int ei = 0; ei < router()->nelements() && _fd < 0; ei++) {
    Element *e = router()->element(ei);
    FromDevice *fdev = (FromDevice *)e->cast("FromDevice");
    if (fdev && fdev->ifname() == _ifname && fdev->fd() >= 0) {
      _fd = fdev->fd();
      _my_fd = false;
    }
  }
  if (_fd < 0) {
# if TODEVICE_LINUX
    _fd = FromDevice::open_packet_socket(_ifname, errh);
    _my_fd = true;
# else
    return errh->error("ToDevice requires an initialized FromDevice on this platform") ;
# endif
  }
  if (_fd < 0)
    return -1;
  
#else
  
  return errh->error("ToDevice is not supported on this platform");
  
#endif

  // check for duplicate writers
  void *&used = router()->force_attachment("device_writer_" + _ifname);
  if (used)
    return errh->error("duplicate writer for device `%s'", _ifname.cc());
  used = this;

  if (input_is_pull(0)) {
    ScheduleInfo::join_scheduler(this, &_task, errh);
    _signal = Notifier::upstream_empty_signal(this, 0, &_task);
  }
  return 0;
}

void
ToDevice::cleanup(CleanupStage)
{
  if (_fd >= 0 && _my_fd)
    close(_fd);
  _fd = -1;
}

void
ToDevice::send_packet(Packet *p)
{
  int retval;
  const char *syscall;

#if TODEVICE_WRITE
  retval = (write(_fd, p->data(), p->length()) > 0 ? 0 : -1);
  syscall = "write";
#elif TODEVICE_SEND
  retval = send(_fd, p->data(), p->length(), 0);
  syscall = "send";
#else
  retval = 0;
#endif
  if (retval < 0) {
    int saved_errno = errno;
    if (!_ignore_q_errs || !_printed_err || (errno != ENOBUFS && errno != EAGAIN)) {
      _printed_err = true;
      click_chatter("ToDevice(%s) %s: %s", _ifname.cc(), syscall, strerror(errno));
    }
    if (_set_error_anno) {
      unsigned char c = saved_errno & 0xFF;
      if (c != saved_errno)
	click_chatter("ToDevice(%s) truncating errno to %d", _ifname.cc(), (int) c);
      SET_SEND_ERR_ANNO(p, c);
    }
    checked_output_push(0, p);
  }
  else
    p->kill();
}

void
ToDevice::push(int, Packet *p)
{
  assert(p->length() >= 14);	// XXX: sizeof()?
  send_packet(p);
}

bool
ToDevice::run_task()
{
  // XXX reduce tickets when idle
  Packet *p = input(0).pull();
  if (p)
    send_packet(p);
  else if (!_signal)
    return false;
  _task.fast_reschedule();
  return p != 0;
}

void
ToDevice::add_handlers()
{
  if (input_is_pull(0))
    add_task_handlers(&_task);
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(FromDevice userlevel)
EXPORT_ELEMENT(ToDevice)


