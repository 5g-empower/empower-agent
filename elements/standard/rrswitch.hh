#ifndef CLICK_RRSWITCH_HH
#define CLICK_RRSWITCH_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * RoundRobinSwitch
 * =s classification
 * sends packets to round-robin outputs
 * =io
 * One input, one or more outputs
 * =d
 * Pushes each arriving packet to one of the N outputs. The next packet
 * will be pushed to the following output in round-robin order.
 *
 * =a StrideSwitch, Switch, HashSwitch, RandomSwitch, RoundRobinSched
 */

class RoundRobinSwitch : public Element {

  atomic_uint32_t _next;
  
 public:
  
  RoundRobinSwitch();
  ~RoundRobinSwitch();
  
  const char *class_name() const	{ return "RoundRobinSwitch"; }
  const char *processing() const	{ return PUSH; }
  void notify_noutputs(int);
  
  RoundRobinSwitch *clone() const	{ return new RoundRobinSwitch; }
  
  void push(int, Packet *);
  
};

CLICK_ENDDECLS
#endif
