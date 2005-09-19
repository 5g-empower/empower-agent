/*
 * storeipaddress.{cc,hh} -- element stores IP destination annotation into
 * packet
 * Robert Morris
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
#include "storeipaddress.hh"
#include <click/confparse.hh>
CLICK_DECLS

StoreIPAddress::StoreIPAddress()
{
}

StoreIPAddress::~StoreIPAddress()
{
}

int
StoreIPAddress::configure(Vector<String> &conf, ErrorHandler *errh)
{
  if (conf.size() == 1) {
    _use_address = false;
    return cp_va_parse(conf, this, errh,
		       cpUnsigned, "byte offset of IP address", &_offset,
		       cpEnd);
  } else {
    _use_address = true;
    return cp_va_parse(conf, this, errh,
		       cpIPAddress, "IP address", &_address,
		       cpUnsigned, "byte offset of IP address", &_offset,
		       cpEnd);
  }
}

Packet *
StoreIPAddress::simple_action(Packet *p)
{
  // XXX error reporting?
  IPAddress ipa = (_use_address ? _address : p->dst_ip_anno());
  if ((ipa || _use_address) && _offset + 4 <= p->length()) {
    WritablePacket *q = p->uniqueify();
    memcpy(q->data() + _offset, &ipa, 4);
    return q;
  } else
    return p;
}

CLICK_ENDDECLS
EXPORT_ELEMENT(StoreIPAddress)
ELEMENT_MT_SAFE(StoreIPAddress)
