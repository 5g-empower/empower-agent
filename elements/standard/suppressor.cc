/*
 * suppressor.{cc,hh} -- element for Ethernet switch
 * John Jannotti
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
#include "suppressor.hh"
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/error.hh>
CLICK_DECLS

Suppressor::Suppressor()
{
  MOD_INC_USE_COUNT;
}

Suppressor::~Suppressor()
{
  MOD_DEC_USE_COUNT;
}

void
Suppressor::notify_ninputs(int i)
{
  set_ninputs(i);
  set_noutputs(i);
}

bool
Suppressor::set(int output, bool sup)
{
  // Need to change anything?
  if (sup == suppressed(output))
    return false;
  
  if (sup)
    suppress(output);
  else
    allow(output);
  
  return true;
}

int
Suppressor::initialize(ErrorHandler *)
{
  allow_all();
  return 0;
}

void
Suppressor::push(int source, Packet *p)
{
  if (suppressed(source)) {
    p->kill();
  } else {			// forward	
    output(source).push(p);
  }
}

Packet *
Suppressor::pull(int source)
{
  if (suppressed(source)) {
    return 0;
  } else {
    return input(source).pull();
  }
}

static String
read_active(Element *e, void *thunk)
{
  Suppressor *sup = static_cast<Suppressor *>(e);
  int port = (int) reinterpret_cast<long>(thunk);
  return (sup->suppressed(port) ? "false\n" : "true\n");
}

static int
write_active(const String &in_s, Element *e, void *thunk, ErrorHandler *errh)
{
  Suppressor *sup = static_cast<Suppressor *>(e);
  int port = (int) reinterpret_cast<long>(thunk);
  bool active;
  if (!cp_bool(cp_uncomment(in_s), &active))
    return errh->error("active value must be boolean");
  else {
    sup->set(port, active);
    return 0;
  }
}

static int
write_reset(const String &, Element *e, void *, ErrorHandler *)
{
  Suppressor *sup = static_cast<Suppressor *>(e);
  sup->allow_all();
  return 0;
}

void
Suppressor::add_handlers()
{
  for (int i = 0; i < ninputs(); i++) {
    String s = "active" + String(i);
    add_read_handler(s, read_active, (void *)i);
    add_write_handler(s, write_active, (void *)i);
  }
  add_write_handler("reset", write_reset, 0);
}

CLICK_ENDDECLS
EXPORT_ELEMENT(Suppressor)
