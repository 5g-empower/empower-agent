#ifndef CLICK_INFINITESOURCE_HH
#define CLICK_INFINITESOURCE_HH
#include <click/element.hh>
#include <click/task.hh>
CLICK_DECLS

/*
=c

InfiniteSource([DATA, LIMIT, BURST, ACTIVE?, I<KEYWORDS>])

=s sources

generates packets whenever scheduled

=d

Creates packets consisting of DATA. Pushes BURST such packets out its single
output every time it is scheduled (which will be often). Stops sending after
LIMIT packets are generated; but if LIMIT is negative, sends packets forever.
Will send packets only if ACTIVE is true. (ACTIVE is true by default.) Default
DATA is at least 64 bytes long. Default LIMIT is -1 (send packets forever).
Default BURST is 1.

Keyword arguments are:

=over 8

=item DATA

String. Same as the DATA argument.

=item DATASIZE

Integer. If set, ensures the outgoing packet contains at least this many bytes.

=item LIMIT

Integer. Same as the LIMIT argument.

=item BURST

Integer. Same as the BURST argument.

=item ACTIVE

Boolean. Same as the ACTIVE? argument.

=item STOP

Boolean. If true, then stop the driver once LIMIT packets are sent. Default is
false.

=back

To generate a particular traffic pattern, use this element and RatedSource
in conjunction with PokeHandlers.

=e

  InfiniteSource(\<0800>) -> Queue -> ...

=n

Useful for profiling and experiments.  Packets' timestamp annotations are set
to the current time.

=h count read-only
Returns the total number of packets that have been generated.
=h reset write-only
Resets the number of generated packets to 0. The InfiniteSource will then
generate another LIMIT packets (if it is active).
=h data read/write
Returns or sets the DATA parameter.
=h datasize read/write
Returns or sets the DATASIZE parameter.
=h limit read/write
Returns or sets the LIMIT parameter.
=h burstsize read/write
Returns or sets the BURSTSIZE parameter.
=h active read/write
Makes the element active or inactive.

=a

RatedSource, PokeHandlers */

class InfiniteSource : public Element { public:
  
  InfiniteSource();
  ~InfiniteSource();
  
  const char *class_name() const		{ return "InfiniteSource"; }
  const char *processing() const		{ return AGNOSTIC; }
  const char *flags() const			{ return "S1"; }
  void add_handlers();
  
  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  bool can_live_reconfigure() const		{ return true; }
  void cleanup(CleanupStage);

  bool run_task();
  Packet *pull(int);

 protected:

  void setup_packet();
  
  Packet *_packet;
  int _burstsize;
  int _limit;
  int _count;
  int _datasize;
  bool _active : 1;
  bool _stop : 1;
  Task _task;
  String _data;
  
  static String read_param(Element *, void *);
  static int change_param(const String &, Element *, void *, ErrorHandler *);

};

CLICK_ENDDECLS
#endif
