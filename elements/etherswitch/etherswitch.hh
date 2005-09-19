#ifndef CLICK_ETHERSWITCH_HH
#define CLICK_ETHERSWITCH_HH
#include <click/element.hh>
#include <click/etheraddress.hh>
#include <click/hashmap.hh>
CLICK_DECLS

class EtherSwitch : public Element {
  
 public:
  
  EtherSwitch();
  ~EtherSwitch();
  
  const char *class_name() const		{ return "EtherSwitch"; }
  const char *port_count() const		{ return "-/="; }
  const char *processing() const		{ return PUSH; }
  const char *flow_code() const			{ return "#/[^#]"; }

  void push(int port, Packet* p);

  static String read_table(Element* f, void *);
  void add_handlers();

  void set_timeout(int seconds)			{ _timeout = seconds; }

  struct AddrInfo {
    int port;
    timeval stamp;
    AddrInfo(int p, const timeval& s);
  };

private:

  typedef HashMap<EtherAddress, AddrInfo *> Table;
  Table _table;
  int _timeout;
  
  void broadcast(int source, Packet*);
};

CLICK_ENDDECLS
#endif
