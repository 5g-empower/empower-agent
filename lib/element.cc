// -*- c-basic-offset: 2; related-file-name: "../include/click/element.hh" -*-
/*
 * element.{cc,hh} -- the Element base class
 * Eddie Kohler
 * statistics: Robert Morris
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software")
 * to deal in the Software without restriction, subject to the conditions
 * listed in the Click LICENSE file. These conditions include: you must
 * preserve this copyright notice, and you cannot mention the copyright
 * holders in advertising related to the Software without their permission.
 * The Software is provided WITHOUT ANY WARRANTY, EXPRESS OR IMPLIED. This
 * notice is a summary of the Click LICENSE file; the license in that file is
 * legally binding.
 */

#include <click/config.h>

#include <click/glue.hh>
#include <click/element.hh>
#include <click/bitvector.hh>
#include <click/confparse.hh>
#include <click/error.hh>
#include <click/router.hh>
#include <click/straccum.hh>
#if CLICK_LINUXMODULE
# include <click/cxxprotect.h>
CLICK_CXX_PROTECT
# include <asm/types.h>
# include <asm/uaccess.h>
CLICK_CXX_UNPROTECT
# include <click/cxxunprotect.h>
#endif
CLICK_DECLS

const char * const Element::AGNOSTIC = "a";
const char * const Element::PUSH = "h";
const char * const Element::PULL = "l";
const char * const Element::PUSH_TO_PULL = "h/l";
const char * const Element::PULL_TO_PUSH = "l/h";

const char * const Element::COMPLETE_FLOW = "x/x";

int Element::nelements_allocated = 0;

#if CLICK_STATS >= 2
# define ELEMENT_CTOR_STATS _calls(0), _self_cycles(0), _child_cycles(0),
#else
# define ELEMENT_CTOR_STATS
#endif

Element::Element()
  : ELEMENT_CTOR_STATS _inputs(&_ports0[0]), _outputs(&_ports0[0]),
    _ninputs(0), _noutputs(0), _router(0), _eindex(-1)
{
  nelements_allocated++;
}

Element::Element(int ninputs, int noutputs)
  : ELEMENT_CTOR_STATS _inputs(&_ports0[0]), _outputs(&_ports0[0]),
    _ninputs(0), _noutputs(0), _router(0), _eindex(-1)
{
  set_nports(ninputs, noutputs);
  nelements_allocated++;
}

Element::~Element()
{
  nelements_allocated--;
  if (_inputs != _ports0)
    delete[] _inputs;
  if (_outputs != _ports0 && _outputs != _ports0 + _ninputs)
    delete[] _outputs;
}

// CHARACTERISTICS

void *
Element::cast(const char *name)
{
  const char *my_name = class_name();
  if (my_name && name && strcmp(my_name, name) == 0)
    return this;
  else
    return 0;
}

String
Element::id() const
{
  String s;
  if (Router *r = router())
    s = r->ename(_eindex);
  return (s ? s : String("<unknown>"));
}

String
Element::declaration() const
{
  return id() + " :: " + class_name();
}

String
Element::landmark() const
{
  String s;
  if (Router *r = router())
    s = r->elandmark(_eindex);
  return (s ? s : String("<unknown>"));
}

// INPUTS AND OUTPUTS

void
Element::set_nports(int new_ninputs, int new_noutputs)
{
  // exit on bad counts, or if already initialized
  if (new_ninputs < 0 || new_noutputs < 0 || _ports0[0].initialized())
    return;
  
  // decide if inputs & outputs were inlined
  bool old_in_inline =
    (_inputs == _ports0);
  bool old_out_inline =
    (_outputs == _ports0 || _outputs == _ports0 + _ninputs);

  // decide if inputs & outputs should be inlined
  bool new_in_inline =
    (new_ninputs == 0
     || new_ninputs + new_noutputs <= INLINE_PORTS
     || (new_ninputs <= INLINE_PORTS && new_noutputs > INLINE_PORTS)
     || (new_ninputs <= INLINE_PORTS && new_ninputs > new_noutputs
	 && processing() == PULL));
  bool new_out_inline =
    (new_noutputs == 0
     || new_ninputs + new_noutputs <= INLINE_PORTS
     || (new_noutputs <= INLINE_PORTS && !new_in_inline));

  // create new port arrays
  Port *new_inputs =
    (new_in_inline ? _ports0 : new Port[new_ninputs]);
  if (!new_inputs)		// out of memory -- return
    return;

  Port *new_outputs =
    (new_out_inline ? (new_in_inline ? _ports0 + new_ninputs : _ports0)
     : new Port[new_noutputs]);
  if (!new_outputs) {		// out of memory -- return
    if (!new_in_inline)
      delete[] new_inputs;
    return;
  }

  // install information
  if (!old_in_inline)
    delete[] _inputs;
  if (!old_out_inline)
    delete[] _outputs;
  _inputs = new_inputs;
  _outputs = new_outputs;
  _ninputs = new_ninputs;
  _noutputs = new_noutputs;
}

void
Element::set_ninputs(int count)
{
  set_nports(count, _noutputs);
}

void
Element::set_noutputs(int count)
{
  set_nports(_ninputs, count);
}

bool
Element::ports_frozen() const
{
  return _ports0[0].initialized();
}

void
Element::notify_ninputs(int)
{
}

void
Element::notify_noutputs(int)
{
}

void
Element::initialize_ports(const int *in_v, const int *out_v)
{
  // always initialize _ports0[0] so set_nports will know whether to quit
  if (_inputs != _ports0 && _outputs != _ports0)
    _ports0[0] = Port(this, 0, -1);
  
  for (int i = 0; i < ninputs(); i++) {
    // allowed iff in_v[i] == VPULL
    int port = (in_v[i] == VPULL ? 0 : -1);
    _inputs[i] = Port(this, 0, port);
  }
  
  for (int o = 0; o < noutputs(); o++) {
    // allowed iff out_v[o] != VPULL
    int port = (out_v[o] == VPULL ? -1 : 0);
    _outputs[o] = Port(this, 0, port);
  }
}

int
Element::connect_input(int i, Element *f, int port)
{
  if (i >= 0 && i < ninputs() && _inputs[i].allowed()) {
    _inputs[i] = Port(this, f, port);
    return 0;
  } else
    return -1;
}

int
Element::connect_output(int o, Element *f, int port)
{
  if (o >= 0 && o < noutputs() && _outputs[o].allowed()) {
    _outputs[o] = Port(this, f, port);
    return 0;
  } else
    return -1;
}

// FLOW

const char *
Element::flow_code() const
{
  return COMPLETE_FLOW;
}

static void
skip_flow_code(const char *&p)
{
  if (*p != '/' && *p != 0) {
    if (*p == '[') {
      for (p++; *p != ']' && *p; p++)
	/* nada */;
      if (*p)
	p++;
    } else
      p++;
  }
}

static int
next_flow_code(const char *&p, int port, Bitvector &code,
	       ErrorHandler *errh, const Element *e)
{
  if (*p == '/' || *p == 0) {
    // back up to last code character
    if (p[-1] == ']') {
      for (p -= 2; *p != '['; p--)
	/* nada */;
    } else
      p--;
  }

  code.assign(256, false);

  if (*p == '[') {
    bool negated = false;
    if (p[1] == '^')
      negated = true, p++;
    for (p++; *p != ']' && *p; p++) {
      if (isalpha(*p))
	code[*p] = true;
      else if (*p == '#')
	code[port + 128] = true;
      else if (errh)
	errh->error("`%{element}' flow code: invalid character `%c'", e, *p);
    }
    if (negated)
      code.negate();
    if (!*p) {
      if (errh)
	errh->error("`%{element}' flow code: missing `]'", e);
      p--;			// don't skip over final '\0'
    }
  } else if (isalpha(*p))
    code[*p] = true;
  else if (*p == '#')
    code[port + 128] = true;
  else {
    if (errh)
      errh->error("`%{element}' flow code: invalid character `%c'", e, *p);
    p++;
    return -1;
  }

  p++;
  return 0;
}

void
Element::forward_flow(int input_port, Bitvector *bv) const
{
  const char *f = flow_code();
  if (input_port < 0 || input_port >= ninputs()) {
    bv->assign(noutputs(), false);
    return;
  } else if (!f || f == COMPLETE_FLOW) {
    bv->assign(noutputs(), true);
    return;
  }

  bv->assign(noutputs(), false);
  ErrorHandler *errh = ErrorHandler::default_handler();
  
  const char *f_in = f;
  const char *f_out = strchr(f, '/');
  if (!f_out || f_in == f_out || f_out[1] == 0 || f_out[1] == '/') {
    errh->error("`%{element}' flow code: missing or bad `/'", this);
    return;
  }
  f_out++;
  
  Bitvector in_code;
  for (int i = 0; i < input_port; i++)
    skip_flow_code(f_in);
  next_flow_code(f_in, input_port, in_code, errh, this);

  Bitvector out_code;
  for (int i = 0; i < noutputs(); i++) {
    next_flow_code(f_out, i, out_code, errh, this);
    if (in_code.nonzero_intersection(out_code))
      (*bv)[i] = true;
  }
}

void
Element::backward_flow(int output_port, Bitvector *bv) const
{
  const char *f = flow_code();
  if (output_port < 0 || output_port >= noutputs()) {
    bv->assign(ninputs(), false);
    return;
  } else if (!f || f == COMPLETE_FLOW) {
    bv->assign(ninputs(), true);
    return;
  }

  bv->assign(ninputs(), false);
  ErrorHandler *errh = ErrorHandler::default_handler();
  
  const char *f_in = f;
  const char *f_out = strchr(f, '/');
  if (!f_out || f_in == f_out || f_out[1] == 0 || f_out[1] == '/') {
    errh->error("`%{element}' flow code: missing or bad `/'", this);
    return;
  }
  f_out++;
  
  Bitvector out_code;
  for (int i = 0; i < output_port; i++)
    skip_flow_code(f_out);
  next_flow_code(f_out, output_port, out_code, errh, this);

  Bitvector in_code;
  for (int i = 0; i < ninputs(); i++) {
    next_flow_code(f_in, i, in_code, errh, this);
    if (in_code.nonzero_intersection(out_code))
      (*bv)[i] = true;
  }
}

// PUSH OR PULL PROCESSING

const char *
Element::processing() const
{
  return AGNOSTIC;
}

static int
next_processing_code(const char *&p, ErrorHandler *errh)
{
  switch (*p) {
    
   case 'h': case 'H':
    p++;
    return Element::VPUSH;
    
   case 'l': case 'L':
    p++;
    return Element::VPULL;
    
   case 'a': case 'A':
    p++;
    return Element::VAGNOSTIC;

   case '/': case 0:
    return -2;

   default:
    if (errh) errh->error("invalid character `%c' in processing code", *p);
    p++;
    return -1;
    
  }
}

void
Element::processing_vector(int *in_v, int *out_v, ErrorHandler *errh) const
{
  const char *p_in = processing();
  int val = 0;

  const char *p = p_in;
  int last_val = 0;
  for (int i = 0; i < ninputs(); i++) {
    if (last_val >= 0)
      last_val = next_processing_code(p, errh);
    if (last_val >= 0)
      val = last_val;
    in_v[i] = val;
  }

  while (*p && *p != '/')
    p++;
  if (!*p)
    p = p_in;
  else
    p++;

  last_val = 0;
  for (int i = 0; i < noutputs(); i++) {
    if (last_val >= 0)
      last_val = next_processing_code(p, errh);
    if (last_val >= 0)
      val = last_val;
    out_v[i] = val;
  }
}

const char *
Element::flags() const
{
  return "";
}

// CLONING AND CONFIGURING

int
Element::configure_phase() const
{
  return CONFIGURE_PHASE_DEFAULT;
}

int
Element::configure(Vector<String> &conf, ErrorHandler *errh)
{
  return cp_va_parse(conf, this, errh, cpEnd);
}

int
Element::initialize(ErrorHandler *)
{
  return 0;
}

void
Element::cleanup(CleanupStage)
{
}


// LIVE CONFIGURATION

bool
Element::can_live_reconfigure() const
{
  return false;
}

int
Element::live_reconfigure(Vector<String> &conf, ErrorHandler *errh)
{
  if (can_live_reconfigure())
    return configure(conf, errh);
  else
    return errh->error("cannot reconfigure %{element} live", this);
}

Element *
Element::hotswap_element() const
{
  if (Router *other = router()->hotswap_router())
    return other->find(id());
  else
    return 0;
}

void
Element::take_state(Element *, ErrorHandler *)
{
}


// used by configuration() and reconfigure_handler()
static int store_default_configuration;
static int was_default_configuration;

void
Element::configuration(Vector<String> &conf) const
{
  // Handle configuration(void) requests specially by preserving whitespace.
  String s = router()->default_configuration_string(eindex());
  if (store_default_configuration)
    conf.push_back(s);
  else
    cp_argvec(s, conf);
  was_default_configuration = 1;
}

String
Element::configuration() const
{
  store_default_configuration = 1;
  Vector<String> conf;
  configuration(conf);
  store_default_configuration = 0;
  // cp_unargvec(conf) will return conf[0] if conf has one element, so
  // store_default_configuration will work as expected.
  return cp_unargvec(conf);
}


// SELECT

#if CLICK_USERLEVEL

int
Element::add_select(int fd, int mask) const
{
  return router()->add_select(fd, eindex(), mask);
}

int
Element::remove_select(int fd, int mask) const
{
  return router()->remove_select(fd, eindex(), mask);
}

void
Element::selected(int)
{
}

#endif


// HANDLERS

void
Element::add_read_handler(const String &name, ReadHandler h, void *thunk)
{
  Router::add_read_handler(this, name, h, thunk);
}

void
Element::add_write_handler(const String &name, WriteHandler h, void *thunk)
{
  Router::add_write_handler(this, name, h, thunk);
}

static String
read_class_handler(Element *e, void *)
{
  return String(e->class_name()) + "\n";
}

static String
read_name_handler(Element *e, void *)
{
  return e->id() + "\n";
}

static String
read_config_handler(Element *e, void *)
{
    String s = e->configuration();
    if (s.length() && s.back() != '\n')
	return s + "\n";
    else
	return s;
}

static int
write_config_handler(const String &str, Element *e, void *,
		     ErrorHandler *errh)
{
  Vector<String> conf;
  cp_argvec(str, conf);
  if (e->live_reconfigure(conf, errh) >= 0) {
    e->router()->set_default_configuration_string(e->eindex(), str);
    return 0;
  } else
    return -EINVAL;
}

static String
read_ports_handler(Element *e, void *)
{
  return e->router()->element_ports_string(e->eindex());
}

static String
read_handlers_handler(Element *e, void *)
{
  Vector<int> handlers;
  Router *r = e->router();
  Router::element_hindexes(e, handlers);
  StringAccum sa;
  for (int i = 0; i < handlers.size(); i++) {
    const Router::Handler *h = r->handler(handlers[i]);
    if (h->read_visible() || h->write_visible())
      sa << h->name() << '\t' << (h->read_visible() ? "r" : "") << (h->write_visible() ? "w" : "") << '\n';
  }
  return sa.take_string();
}


#if CLICK_STATS >= 1

static String
read_icounts_handler(Element *f, void *)
{
  StringAccum sa;
  for (int i = 0; i < f->ninputs(); i++)
    if (f->input(i).allowed() || CLICK_STATS >= 2)
      sa << f->input(i).npackets() << "\n";
    else
      sa << "??\n";
  return sa.take_string();
}

static String
read_ocounts_handler(Element *f, void *)
{
  StringAccum sa;
  for (int i = 0; i < f->noutputs(); i++)
    if (f->output(i).allowed() || CLICK_STATS >= 2)
      sa << f->output(i).npackets() << "\n";
    else
      sa << "??\n";
  return sa.take_string();
}

#endif /* CLICK_STATS >= 1 */

#if CLICK_STATS >= 2
/*
 * cycles:
 * # of calls to this element (push or pull).
 * cycles spent in this element and elements it pulls or pushes.
 * cycles spent in the elements this one pulls and pushes.
 */
static String
read_cycles_handler(Element *f, void *)
{
  return(String(f->_calls) + "\n" +
         String(f->_self_cycles) + "\n" +
         String(f->_child_cycles) + "\n");
}
#endif

void
Element::add_default_handlers(bool allow_write_config)
{
  add_read_handler("class", read_class_handler, 0);
  add_read_handler("name", read_name_handler, 0);
  add_read_handler("config", read_config_handler, 0);
  if (allow_write_config && can_live_reconfigure())
    add_write_handler("config", write_config_handler, 0);
  add_read_handler("ports", read_ports_handler, 0);
  add_read_handler("handlers", read_handlers_handler, 0);
#if CLICK_STATS >= 1
  add_read_handler("icounts", read_icounts_handler, 0);
  add_read_handler("ocounts", read_ocounts_handler, 0);
# if CLICK_STATS >= 2
  add_read_handler("cycles", read_cycles_handler, 0);
# endif
#endif
}

#ifdef HAVE_STRIDE_SCHED
static String
read_task_tickets(Element *e, void *thunk)
{
  Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
  return String(task->tickets()) + "\n";
}

static int
write_task_tickets(const String &s, Element *e, void *thunk, ErrorHandler *errh)
{
  Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
  int tix;
  if (!cp_integer(cp_uncomment(s), &tix))
    return errh->error("'tickets' takes an integer between 1 and %d", Task::MAX_TICKETS);
  if (tix < 1) {
    errh->warning("tickets pinned at 1");
    tix = 1;
  } else if (tix > Task::MAX_TICKETS) {
    errh->warning("tickets pinned at %d", Task::MAX_TICKETS);
    tix = Task::MAX_TICKETS;
  }
  task->set_tickets(tix);
  return 0;
}
#endif

static String
read_task_scheduled(Element *e, void *thunk)
{
  Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
  return String(task->scheduled() ? "true\n" : "false\n");
}

#if __MTCLICK__
static String
read_task_thread_preference(Element *e, void *thunk)
{
  Task *task = (Task *)((uint8_t *)e + (intptr_t)thunk);
  return String(task->thread_preference())+String("\n");
}
#endif

void
Element::add_task_handlers(Task *task, const String &prefix)
{
  intptr_t task_offset = (uint8_t *)task - (uint8_t *)this;
  void *thunk = (void *)task_offset;
#ifdef HAVE_STRIDE_SCHED
  add_read_handler(prefix + "tickets", read_task_tickets, thunk);
  add_write_handler(prefix + "tickets", write_task_tickets, thunk);
#endif
  add_read_handler(prefix + "scheduled", read_task_scheduled, thunk);
#if __MTCLICK__
  add_read_handler(prefix + "thread_preference", read_task_thread_preference, thunk);
#endif
}

void
Element::add_handlers()
{
}

String
Element::read_positional_handler(Element *element, void *thunk)
{
  Vector<String> conf;
  element->configuration(conf);
  int no = (intptr_t)thunk;
  if (no >= conf.size())
    return String();
  String s = conf[no];
  // add trailing "\n" if appropriate
  if (s && s.back() != '\n')
    s += "\n";
  return s;
}

String
Element::read_keyword_handler(Element *element, void *thunk)
{
  Vector<String> conf;
  element->configuration(conf);
  const char *kw = (const char *)thunk;
  String s;
  for (int i = conf.size() - 1; i >= 0; i--)
    if (cp_va_parse_keyword(conf[i], element, ErrorHandler::silent_handler(),
			    kw, cpArgument, &s, 0) > 0)
      break;
  // add trailing "\n" if appropriate
  if (s && s.back() != '\n')
    s += "\n";
  return s;
}

static int
reconfigure_handler(const String &arg, Element *e,
		    int argno, const char *keyword, ErrorHandler *errh)
{
  Vector<String> conf;
  was_default_configuration = 0;
  e->configuration(conf);

  if (keyword) {
    if (was_default_configuration)
      return errh->error("can't use reconfigure_keyword_handler with default configuration() method");
    conf.push_back(String(keyword) + " " + arg);
  } else {
    while (conf.size() <= argno)
      conf.push_back(String());
    conf[argno] = cp_uncomment(arg);
  }

  // create new configuration before calling live_reconfigure(), in case it
  // mucks with the 'conf' array
  String new_config;
  if (keyword)
    new_config = String::stable_string("/* dynamically reconfigured */");
  else
    new_config = cp_unargvec(conf);
  
  if (e->live_reconfigure(conf, errh) < 0)
    return -EINVAL;
  else {
    e->router()->set_default_configuration_string(e->eindex(), new_config);
    return 0;
  }
}

int
Element::reconfigure_positional_handler(const String &arg, Element *e,
					void *thunk, ErrorHandler *errh)
{
  return reconfigure_handler(arg, e, (intptr_t)thunk, 0, errh);
}

int
Element::reconfigure_keyword_handler(const String &arg, Element *e,
				     void *thunk, ErrorHandler *errh)
{
  return reconfigure_handler(arg, e, -1, (const char *)thunk, errh);
}

int
Element::llrpc(unsigned, void *)
{
  return -EINVAL;
}

int
Element::local_llrpc(unsigned command, void *data)
{
#if CLICK_LINUXMODULE
  mm_segment_t old_fs = get_fs();
  set_fs(get_ds());

  int result = llrpc(command, data);

  set_fs(old_fs);
  return result;
#else
  return llrpc(command, data);
#endif
}

// RUNNING

void
Element::push(int, Packet *p)
{
  p = simple_action(p);
  if (p) output(0).push(p);
}

Packet *
Element::pull(int)
{
  Packet *p = input(0).pull();
  if (p) p = simple_action(p);
  return p;
}

Packet *
Element::simple_action(Packet *p)
{
  return p;
}

bool
Element::run_task()
{
  static int nwarn = 0;
  if (nwarn++ < 3)
    click_chatter("warning: calling deprecated run_scheduled() method;\nreplace with run_task() in your code");
  run_scheduled();
  return true;
}

void
Element::run_timer()
{
  static int nwarn = 0;
  if (nwarn++ < 3)
    click_chatter("warning: calling deprecated run_scheduled() method;\nreplace with run_timer() in your code");
  run_scheduled();
}

void
Element::run_scheduled()
{
  assert(0 && "bad run_task");
}

CLICK_ENDDECLS
