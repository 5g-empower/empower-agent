#ifndef CLICK_RATEDUNQUEUE_HH
#define CLICK_RATEDUNQUEUE_HH
#include <click/element.hh>
#include <click/gaprate.hh>
#include <click/task.hh>
CLICK_DECLS

/*
 * =c
 * RatedUnqueue(RATE)
 * =s packet scheduling
 * pull-to-push converter
 * =d
 * 
 * Pulls packets at the given RATE in packets per second, and pushes them out
 * its single output.
 *
 * =h rate read/write
 *
 * =a BandwidthRatedUnqueue, Unqueue, Shaper, RatedSplitter */

class RatedUnqueue : public Element { public:
  
  RatedUnqueue();
  ~RatedUnqueue();
  
  const char *class_name() const		{ return "RatedUnqueue"; }
  const char *processing() const		{ return PULL_TO_PUSH; }
  
  int configure(Vector<String> &, ErrorHandler *);
  void configuration(Vector<String> &) const;
  int initialize(ErrorHandler *);
  void add_handlers();
  
  bool run_task();

  unsigned rate() const				{ return _rate.rate(); }
  void set_rate(unsigned, ErrorHandler * = 0);
  
 protected:

  GapRate _rate;
  Task _task;
  
};

CLICK_ENDDECLS
#endif
