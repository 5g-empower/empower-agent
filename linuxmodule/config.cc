/*
 * config.cc -- parsing and installing configurations
 * Eddie Kohler
 *
 * Copyright (c) 1999-2000 Massachusetts Institute of Technology
 * Copyright (c) 2000-2002 Mazu Networks, Inc.
 * Copyright (c) 2001-2002 International Computer Science Institute
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
#include "modulepriv.hh"

#include <click/straccum.hh>
#include <click/router.hh>
#include <click/error.hh>
#include <click/lexer.hh>

static String *current_config = 0;
uint32_t click_config_generation;


/*************************** Parsing configurations **************************/

static Lexer *lexer = 0;

extern "C" int
click_add_element_type(const char *name, Element *e)
{
  if (name)
    return lexer->add_element_type(name, e);
  else
    return lexer->add_element_type(e);
}

extern "C" void
click_remove_element_type(int i)
{
  lexer->remove_element_type(i);
}

static String
read_classes(Element *, void *)
{
  Vector<String> v;
  lexer->element_type_names(v);
  StringAccum sa;
  for (int i = 0; i < v.size(); i++)
    sa << v[i] << "\n";
  return sa.take_string();
}


class LinuxModuleLexerExtra : public LexerExtra { public:
  LinuxModuleLexerExtra() { }
  void require(String, ErrorHandler *);
};

void
LinuxModuleLexerExtra::require(String r, ErrorHandler *errh)
{
  if (!click_has_provision(r.c_str()))
    errh->error("unsatisfied requirement `%s'", r.cc());
}

static Router *
parse_router(String s)
{
  LinuxModuleLexerExtra lextra;
  int cookie = lexer->begin_parse(s, "line ", &lextra, click_logged_errh);
  while (lexer->ystatement())
    /* do nothing */;
  Router *r = lexer->create_router();
  lexer->end_parse(cookie);
  return r;
}


/*********************** Installing and killing routers **********************/

#if __MTCLICK__
extern "C" int click_threads();
#endif

static void
set_current_config(const String &s)
{
  *current_config = s;
  click_config_generation++;
}

static void
install_router(Router *r)
{
  click_router = r;
  r->use();
#ifdef HAVE_PROC_CLICK
  init_router_element_procs();
#endif
#if __MTCLICK__
  if (r->initialized())
    click_start_sched(r, click_threads(), click_logged_errh);
#else
  if (r->initialized())
    click_start_sched(r, 1, click_logged_errh);
#endif
}

static void
kill_router()
{
  if (click_router) {
    click_router->please_stop_driver();
#ifdef HAVE_PROC_CLICK
    cleanup_router_element_procs();
#endif
    click_router->unuse();
    click_router = 0;
  }
}


/******************************* Handlers ************************************/

static int
swap_config(const String &s)
{
  set_current_config(s);
  kill_router();
  Router *router = parse_router(s);
  if (router) {
    router->preinitialize();
    router->initialize(click_logged_errh);
    install_router(router);
    return router->initialized() ? 0 : -EINVAL;
  } else
    return -EINVAL;
}

static int
hotswap_config(const String &s)
{
  int before_errors = click_logged_errh->nerrors();
  Router *r = parse_router(s);
  if (!r)
    return -EINVAL;

  // XXX should we lock the kernel?

  // register hotswap router on new router
  if (click_router && click_router->initialized())
    r->pre_take_state(click_router);
  
  if (click_logged_errh->nerrors() == before_errors
      && r->initialize(click_logged_errh) >= 0) {
    // perform hotswap
    if (click_router && click_router->initialized()) {
      // turn off all threads on current router before you take_state
      if (click_kill_router_threads() >= 0) {
	printk("<1>click: performing hotswap\n");
	r->take_state(click_logged_errh);
      }
    }
    // install
    kill_router();
    install_router(r);
    set_current_config(s);
  } else
    delete r;
  
  return 0;
}

static int
write_config(const String &s, Element *, void *thunk, ErrorHandler *)
{
  click_clear_error_log();
  int retval = (thunk ? hotswap_config(s) : swap_config(s));
  return retval;
}


/********************** Initialization and cleanup ***************************/

extern void click_export_elements();

void
click_init_config()
{
  lexer = new Lexer;
  click_export_elements();
  
  Router::add_read_handler(0, "classes", read_classes, 0);
  Router::add_write_handler(0, "config", write_config, 0);
  Router::add_write_handler(0, "hotconfig", write_config, (void *)1);
  Router::change_handler_flags(0, "config", 0, HANDLER_REREAD | HANDLER_WRITE_UNLIMITED);
  Router::change_handler_flags(0, "hotconfig", 0, HANDLER_WRITE_UNLIMITED);
  
  click_config_generation = 1;
  current_config = new String;
}

void
click_cleanup_config()
{
  kill_router();
  delete current_config;
  delete lexer;
}
