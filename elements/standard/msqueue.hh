#ifndef CLICK_MSQUEUE_HH
#define CLICK_MSQUEUE_HH
#include <click/element.hh>
#include <click/bitvector.hh>
#include <click/glue.hh>
#include <click/sync.hh>
#include <click/standard/storage.hh>
CLICK_DECLS

/*
 * =c
 * MSQueue
 * MSQueue(CAPACITY)
 * =s smpclick
 * stores packets in a FIFO queue
 * =d
 * Stores incoming packets in a multiple producer single consumer
 * first-in-first-out queue. Enqueue operations are synchronized, dequeue
 * operations are not. Drops incoming packets if the queue already holds
 * CAPACITY packets. The default for CAPACITY is 1000.
 *
 * =h length read-only
 * Returns the current number of packets in the queue.
 * =h dropd read-only
 * Returns the number of packets dropped by the queue so far.
 * =h capacity read/write
 * Returns or sets the queue's capacity.
 * =a Queue
 */

class MSQueue : public Element {

  int _capacity;
  atomic_uint32_t _head;
  atomic_uint32_t _tail;
  atomic_uint32_t _drops;
  Packet **_q;
  Spinlock _lock;

  int _pad[8];
  bool _can_pull;
  int _pulls;

  int next_i(int i) const { return (i!=_capacity ? i+1 : 0); }
  int prev_i(int i) const { return (i!=0 ? i-1 : _capacity); }

  static String read_handler(Element *, void *);
  
 public:
  
  MSQueue();
  virtual ~MSQueue();
  
  const char *class_name() const		{ return "MSQueue"; }
  void *cast(const char *);
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return PUSH_TO_PULL; }
  
  int size() const; 
  int capacity() const                          { return _capacity; }
  uint32_t drops() const			{ return _drops; }

  Packet *head() const;

  int configure(Vector<String> &, ErrorHandler *);
  int initialize(ErrorHandler *);
  void cleanup(CleanupStage);
  void add_handlers();
  
  void push(int port, Packet *);
  Packet *pull(int port);

#ifdef CLICK_LINUXMODULE
#if __i386__ && HAVE_INTEL_CPU
  static void prefetch_packet(Packet *p);
#endif
#endif
};

#ifdef CLICK_LINUXMODULE
#if __i386__ && HAVE_INTEL_CPU
inline void
MSQueue::prefetch_packet(Packet *p)
{
  struct sk_buff *skb = p->skb();
  asm volatile("prefetcht0 %0" : : "m" (skb->data));
}
#endif
#endif

inline int
MSQueue::size() const
{ 
  register int x = _tail.value() - _head.value(); 
  return (x >= 0 ? x : _capacity + x + 1);
}

inline Packet *
MSQueue::head() const
{
  return (_head.value() != _tail.value() ? _q[_head.value()] : 0);
}

CLICK_ENDDECLS
#endif
