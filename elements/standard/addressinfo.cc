// -*- c-basic-offset: 2; related-file-name: "../../include/click/standard/addressinfo.hh" -*-
/*
 * addressinfo.{cc,hh} -- element stores address information
 * Eddie Kohler
 *
 * Copyright (c) 2000 Mazu Networks, Inc.
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
#include <click/standard/addressinfo.hh>
#include <click/glue.hh>
#include <click/confparse.hh>
#include <click/router.hh>
#include <click/error.hh>
#if CLICK_USERLEVEL
# include <unistd.h>
#endif
#if defined(__linux__) && CLICK_USERLEVEL
# include <net/if.h>
# include <sys/ioctl.h>
# include <net/if_arp.h>
#endif
CLICK_DECLS

AddressInfo::AddressInfo()
  : _map(-1)
{
  MOD_INC_USE_COUNT;
}

AddressInfo::~AddressInfo()
{
  MOD_DEC_USE_COUNT;
}

int
AddressInfo::add_info(const Vector<String> &conf, const String &prefix,
		      ErrorHandler *errh)
{
  Info scrap;
  int before = errh->nerrors();
  
  for (int i = 0; i < conf.size(); i++) {
    Vector<String> parts;
    cp_spacevec(conf[i], parts);
    if (parts.size() == 0)
      // allow empty arguments
      continue;
    else if (parts.size() < 2)
      errh->error("expected `NAME [ADDRS]', got `%s'", conf[i].c_str());
    else {
      String name = prefix + parts[0];
      if (_map[name] < 0) {
	_map.insert(name, _as.size());
	_as.push_back(Info());
      }
      Info &a = _as[_map[name]];
#ifdef CLICK_NS
      // Maybe get info from the simulator...
      String simif = parts[1];
      String simip;
      String simeth;
      const char* simsuffix = ":simnet";
      Router* myrouter = router();
      simclick_sim mysiminst = myrouter->get_siminst();

      int colon = simif.find_right(':');
      if ((colon >= 0) && (simif.substring(colon).lower() == simsuffix)) {
	parts.pop_back();
	char tmp[255];
	
	simif = simif.substring(0,colon);
	simclick_sim_ipaddr_from_name(mysiminst, simif.cc(), tmp, 255);
	simip = tmp;
	simclick_sim_macaddr_from_name(mysiminst, simif.cc(), tmp, 255);
	simeth = tmp;

	if (simip.length()) {
	  parts.push_back(simip);
	}

	if (simeth.length()) {
	  parts.push_back(simeth);
	}
      }
#endif
      for (int j = 1; j < parts.size(); j++)
	if (cp_ip_address(parts[j], &scrap.ip.c[0])) {
	  if ((a.have & INFO_IP) && scrap.ip.u != a.ip.u)
	    errh->warning("\"%s\" IP addresses conflict", name.cc());
	  else if ((a.have & INFO_IP_PREFIX) && (scrap.ip.u & a.ip_mask.u) != (a.ip.u & a.ip_mask.u))
	    errh->warning("\"%s\" IP address and IP address prefix conflict", name.cc());
	  a.have |= INFO_IP;
	  a.ip.u = scrap.ip.u;
	  
	} else if (cp_ip_prefix(parts[j], &scrap.ip.c[0], &scrap.ip_mask.c[0], false)) {
	  if ((a.have & (INFO_IP | INFO_IP_PREFIX))
	      && (scrap.ip.u & scrap.ip_mask.u) != (a.ip.u & scrap.ip_mask.u))
	    errh->warning("\"%s\" IP address and IP address prefix conflict", name.cc());
	  else if ((a.have & INFO_IP_PREFIX) && scrap.ip_mask.u != a.ip_mask.u)
	    errh->warning("\"%s\" IP address prefixes conflict", name.cc());
	  a.have |= INFO_IP_PREFIX;
	  if (!(a.have & INFO_IP)) {
	    a.ip.u = scrap.ip.u;
	    if ((a.ip.u & ~scrap.ip_mask.u) != 0)
	      a.have |= INFO_IP;
	  }
	  a.ip_mask.u = scrap.ip_mask.u;

#ifdef HAVE_IP6
	} else if (cp_ip6_address(parts[j], scrap.ip6.data())) {
	  if ((a.have & INFO_IP6) && scrap.ip6 != a.ip6)
	    errh->warning("\"%s\" IPv6 addresses conflict", name.cc());
	  else if (a.have & INFO_IP6_PREFIX) {
	    IP6Address m = IP6Address::make_prefix(a.ip6_prefix);
	    if ((scrap.ip6 & m) != (a.ip6 & m))
	      errh->warning("\"%s\" IPv6 address and IPv6 address prefix conflict", name.cc());
	  }
	  a.have |= INFO_IP6;
	  a.ip6 = scrap.ip6;
	  
	} else if (cp_ip6_prefix(parts[j], scrap.ip6.data(), &scrap.ip6_prefix, false)) {
	  IP6Address m = IP6Address::make_prefix(scrap.ip6_prefix);
	  if ((a.have & (INFO_IP6 | INFO_IP6_PREFIX))
	      && (scrap.ip6 & m) != (a.ip6 & m))
	    errh->warning("\"%s\" IPv6 address and IPv6 address prefix conflict", name.cc());
	  else if ((a.have & INFO_IP6_PREFIX) && scrap.ip6_prefix != a.ip6_prefix)
	    errh->warning("\"%s\" IPv6 address prefixes conflict", name.cc());
	  a.have |= INFO_IP6_PREFIX;
	  if (!(a.have & INFO_IP6))
	    a.ip6 = scrap.ip6;
	  a.ip6_prefix = scrap.ip6_prefix;
#endif /* HAVE_IP6 */
	  
	} else if (cp_ethernet_address(parts[j], scrap.ether)) {
	  if ((a.have & INFO_ETHER) && memcmp(scrap.ether, a.ether, 6) != 0)
	    errh->warning("\"%s\" Ethernet addresses conflict", name.cc());
	  a.have |= INFO_ETHER;
	  memcpy(a.ether, scrap.ether, 6);
	  
	} else
	  errh->error("\"%s\" `%s' is not a recognizable address", name.cc(), parts[j].cc());
      
    }
  }

  return (errh->nerrors() == before ? 0 : -1);
}

int
AddressInfo::configure(Vector<String> &conf, ErrorHandler *errh)
{
  // find prefix, which includes slash
  String prefix;
  int last_slash = id().find_right('/');
  if (last_slash >= 0)
    prefix = id().substring(0, last_slash + 1);
  else
    prefix = String();

  // put everything in the first AddressInfo
  const Vector<Element *> &ev = router()->elements();
  for (int i = 0; i <= eindex(); i++)
    if (AddressInfo *si = (AddressInfo *)ev[i]->cast("AddressInfo")) {
      if (si == this)
	router()->set_attachment("AddressInfo", si);
      return si->add_info(conf, prefix, errh);
    }

  // should never get here
  return -1;
}

const AddressInfo::Info *
AddressInfo::query(const String &name, unsigned have_mask, const String &eid) const
{
  String prefix = eid;
  int slash = prefix.find_right('/');
  prefix = prefix.substring(0, (slash < 0 ? 0 : slash + 1));
  
  while (1) {
    int e = _map[prefix + name];
    if (e >= 0 && (_as[e].have & have_mask))
      return &_as[e];
    else if (!prefix)
      return 0;

    slash = prefix.find_right('/', prefix.length() - 2);
    prefix = prefix.substring(0, (slash < 0 ? 0 : slash + 1));
  }
}

AddressInfo *
AddressInfo::find_element(Element *e)
{
  if (!e)
    return 0;
  else
    return static_cast<AddressInfo *>(e->router()->attachment("AddressInfo"));
}

bool
AddressInfo::query_ip(String s, unsigned char *store, Element *e)
{
  int colon = s.find_right(':');
  if (colon >= 0 && s.substring(colon).lower() != ":ip"
      && s.substring(colon).lower() != ":ip4")
    return false;
  else if (colon >= 0)
    s = s.substring(0, colon);
  
  if (AddressInfo *infoe = find_element(e))
    if (const Info *info = infoe->query(s, INFO_IP, e->id())) {
      memcpy(store, info->ip.c, 4);
      return true;
    }
  return false;
}

bool
AddressInfo::query_ip_prefix(String s, unsigned char *store,
			     unsigned char *mask_store, Element *e)
{
  int colon = s.find_right(':');
  if (colon >= 0 && s.substring(colon).lower() != ":ipnet"
      && s.substring(colon).lower() != ":ip4net")
    return false;
  else if (colon >= 0)
    s = s.substring(0, colon);
  
  if (AddressInfo *infoe = find_element(e))
    if (const Info *info = infoe->query(s, INFO_IP_PREFIX, e->id())) {
      memcpy(store, info->ip.c, 4);
      memcpy(mask_store, info->ip_mask.c, 4);
      return true;
    }
  return false;
}


#ifdef HAVE_IP6

bool
AddressInfo::query_ip6(String s, unsigned char *store, Element *e)
{
  int colon = s.find_right(':');
  if (colon >= 0 && s.substring(colon).lower() != ":ip6")
    return false;
  else if (colon >= 0)
    s = s.substring(0, colon);
  
  if (AddressInfo *infoe = find_element(e))
    if (const Info *info = infoe->query(s, INFO_IP6, e->id())) {
      memcpy(store, info->ip6.data(), 16);
      return true;
    }
  return false;
}

bool
AddressInfo::query_ip6_prefix(String s, unsigned char *store,
			      int *bits_store, Element *e)
{
  int colon = s.find_right(':');
  if (colon >= 0 && s.substring(colon).lower() != ":ip6net")
    return false;
  else if (colon >= 0)
    s = s.substring(0, colon);
  
  if (AddressInfo *infoe = find_element(e))
    if (const Info *info = infoe->query(s, INFO_IP6_PREFIX, e->id())) {
      memcpy(store, info->ip6.data(), 16);
      *bits_store = info->ip6_prefix;
      return true;
    }
  return false;
}

#endif /* HAVE_IP6 */


bool
AddressInfo::query_ethernet(String s, unsigned char *store, Element *e)
{
  int colon = s.find_right(':');
  if (colon >= 0 && s.substring(colon).lower() != ":eth"
      && s.substring(colon).lower() != ":ethernet")
    return false;
  else if (colon >= 0)
    s = s.substring(0, colon);
  
  if (AddressInfo *infoe = find_element(e))
    if (const Info *info = infoe->query(s, INFO_ETHER, e->id())) {
      memcpy(store, info->ether, 6);
      return true;
    }

  // if it's a device name, return its Ethernet address
#ifdef CLICK_LINUXMODULE
  // in the Linux kernel, just look at the device list
# if LINUX_VERSION_CODE < KERNEL_VERSION(2, 4, 0)
#  define dev_put(dev) /* nada */
# endif
  net_device *dev = dev_get_by_name(s.cc());
  if (dev && dev->type == ARPHRD_ETHER) {
    memcpy(store, dev->dev_addr, 6);
    dev_put(dev);
    return true;
  } else if (dev)
    dev_put(dev);
#elif defined(__linux__) && CLICK_USERLEVEL
  // on Linux userlevel, call ioctl on a socket opened for the purpose
  // -- based on patch from Jose Vasconcellos <jvasco@bellatlantic.net>
  struct ifreq ifr;
  if ((size_t) s.length() < sizeof(ifr.ifr_name)) {
    strcpy(ifr.ifr_name, s.cc());
    int fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd >= 0) {
      int ret = ioctl(fd, SIOCGIFHWADDR, &ifr);
      close(fd);
      if (ret >= 0 && ifr.ifr_hwaddr.sa_family == ARPHRD_ETHER) {
	memcpy(store, ifr.ifr_hwaddr.sa_data, 6);
	return true;
      }
    }
  }
#endif
  
  return false;
}

EXPORT_ELEMENT(AddressInfo)
ELEMENT_HEADER(<click/standard/addressinfo.hh>)

// template instance
#include <click/vector.cc>
CLICK_ENDDECLS
