// -*- c-basic-offset: 4; related-file-name: "../include/click/timer.hh" -*-
/*
 * timer.{cc,hh} -- portable timers
 * Eddie Kohler
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
#include <click/timer.hh>
#include <click/element.hh>
#include <click/router.hh>
#include <click/routerthread.hh>
#include <click/task.hh>
#include <click/master.hh>
CLICK_DECLS

/*
 * element_hook is a callback that gets called when a Timer,
 * constructed with just an Element instance, expires. 
 * 
 * When used in userlevel or kernel polling mode, timer is maintained by
 * Click, so element_hook is called within Click.
 */

static void
element_hook(Timer *, void *thunk)
{
    Element *e = (Element *)thunk;
    e->run_timer();
}

static void
task_hook(Timer *, void *thunk)
{
    Task *task = (Task *)thunk;
    task->reschedule();
}

static void
list_hook(Timer *, void *)
{
    assert(0);
}


Timer::Timer(TimerHook hook, void *thunk)
    : _prev(0), _next(0), _hook(hook), _thunk(thunk), _head(0)
{
}

Timer::Timer(Element *e)
    : _prev(0), _next(0), _hook(element_hook), _thunk(e), _head(0)
{
}

Timer::Timer(Task *t)
    : _prev(0), _next(0), _hook(task_hook), _thunk(t), _head(0)
{
}

TimerList::TimerList()
    : Timer(list_hook, 0)
{
    _prev = _next = _head = this;
}

void
Timer::initialize(Router *r)
{
    initialize(r->master()->timer_list());
}

void
Timer::initialize(Element *e)
{
    initialize(e->router()->master()->timer_list());
}

void
Timer::schedule_at(const struct timeval &when)
{
  // acquire lock, unschedule
  assert(!is_list() && initialized());
  _head->acquire_lock();
  if (scheduled())
    unschedule();

  // set expiration timer
  _expiry = when;

  // manipulate list
  Timer *prev = _head;
  Timer *trav = prev->_next;
  while (trav != _head && timercmp(&_expiry, &trav->_expiry, >)) {
    prev = trav;
    trav = trav->_next;
  }
  _prev = prev;
  _next = trav;
  _prev->_next = this;
  trav->_prev = this;

  // done
  _head->release_lock();
}

void
Timer::schedule_after_s(uint32_t s)
{
  struct timeval t;
  click_gettimeofday(&t);
  t.tv_sec += s;
  schedule_at(t);
}

void
Timer::schedule_after_ms(uint32_t ms)
{
  struct timeval t, interval;
  click_gettimeofday(&t);
  interval.tv_sec = ms / 1000;
  interval.tv_usec = (ms % 1000) * 1000;
  timeradd(&t, &interval, &t);
  schedule_at(t);
}

void
Timer::reschedule_after_s(uint32_t s)
{
  struct timeval t = _expiry;
  t.tv_sec += s;
  schedule_at(t);
}

void
Timer::reschedule_after_ms(uint32_t ms)
{
  struct timeval t = _expiry;
  struct timeval interval;
  interval.tv_sec = ms / 1000;
  interval.tv_usec = (ms % 1000) * 1000;
  timeradd(&t, &interval, &t);
  schedule_at(t);
}

void
Timer::unschedule()
{
  if (scheduled()) {
    _head->acquire_lock();
    _prev->_next = _next;
    _next->_prev = _prev;
    _prev = _next = 0;
    _head->release_lock();
  }
}

void
TimerList::run(const volatile int *runcount)
{
  static int one = 1;
  if (attempt_lock()) {
    struct timeval now;
    click_gettimeofday(&now);
    if (!runcount)
      runcount = &one;
    while (_next != this
	   && !timercmp(&_next->_expiry, &now, >)
	   && *runcount > 0) {
      Timer *t = _next;
      _next = t->_next;
      _next->_prev = this;
      t->_prev = 0;
      t->_hook(t, t->_thunk);
    }
    release_lock();
  }
}

// How long select() should wait until next timer expires.

int
TimerList::get_next_delay(struct timeval *tv)
{
  int retval;
  acquire_lock();
  if (_next == this) {
    tv->tv_sec = 1000;
    tv->tv_usec = 0;
    retval = 0;
  } else {
    struct timeval now;
    click_gettimeofday(&now);
    if (timercmp(&_next->_expiry, &now, >)) {
      timersub(&_next->_expiry, &now, tv);
    } else {
      tv->tv_sec = 0;
      tv->tv_usec = 0;
    }
    retval = 1;
  }
  release_lock();
  return retval;
}

void
TimerList::unschedule_all()
{
  acquire_lock();
  while (_next != this) {
    Timer *t = _next;
    _next = t->_next;
    _next->_prev = this;
    t->_prev = 0;
  }
  release_lock();
}

CLICK_ENDDECLS
