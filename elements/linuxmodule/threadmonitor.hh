#ifndef THREADMONITOR_HH
#define THREADMONITOR_HH

/*
 * =c
 * ThreadMonitor([INTERVAL, THRESH])
 * =s IP
 * print out thread status
 * =d
 *
 * Every INTERVAL number of ms, print out tasks scheduled on each thread if
 * tasks are busy. INTERVAL by default is 1000 ms. Only tasks with cycle count
 * above THRESH are printed. By default THRESH is 1000 cycles.
 */

#include <click/element.hh>
#include <click/timer.hh>

class ThreadMonitor : public Element {

  Timer _timer;
  unsigned _interval;
  unsigned _thresh;

 public:

  ThreadMonitor();
  ~ThreadMonitor();
  
  const char *class_name() const	{ return "ThreadMonitor"; }
  int configure(Vector<String> &, ErrorHandler *);

  int initialize(ErrorHandler *);
  void run_timer();
};

#endif

