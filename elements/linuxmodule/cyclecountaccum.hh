#ifndef CLICK_CYCLECOUNTACCUM_HH
#define CLICK_CYCLECOUNTACCUM_HH

/*
=c
CycleCountAccum()

=s measurement

collects differences in cycle counters

=d

Expects incoming packets to have their cycle counter annotation set.
Measures the current value of the cycle counter, and keeps track of the
total accumulated difference.

=n

A packet has room for either exactly one cycle count or exactly one
performance metric.

=h count read-only
Returns the number of packets that have passed.

=h cycles read-only
Returns the accumulated cycles for all passing packets.

=h reset_counts write-only
Resets C<count> and C<cycles> counters to zero when written.

=a SetCycleCount, RoundTripCycleCount, SetPerfCount, PerfCountAccum */

#include <click/element.hh>

class CycleCountAccum : public Element { public:
  
  CycleCountAccum();
  ~CycleCountAccum();
  
  const char *class_name() const		{ return "CycleCountAccum"; }
  const char *processing() const		{ return AGNOSTIC; }

  int initialize(ErrorHandler *);
  void add_handlers();

  inline void smaction(Packet *);
  void push(int, Packet *p);
  Packet *pull(int);

 private:
  
  uint64_t _accum;
  uint64_t _count;

  static String read_handler(Element *, void *);
  static int reset_handler(const String &, Element *, void *, ErrorHandler *);
  
};

#endif
