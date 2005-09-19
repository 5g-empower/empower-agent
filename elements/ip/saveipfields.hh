#ifndef CLICK_SAVEIPFIELDS_HH
#define CLICK_SAVEIPFIELDS_HH
#include <click/element.hh>
CLICK_DECLS

/*
 * =c
 * SaveIPFields()
 * =s IP, annotations
 * save IP header fields into annotations
 * =d
 * Expects IP packets. Copies the IP header's TOS, TTL,
 * and offset fields into the Click packet annotation.
 *
 * These annotations are used by the IPEncap element.
 *
 * =a IPEncap
 */

class SaveIPFields : public Element {
  
 public:
  
  SaveIPFields();
  
  const char *class_name() const		{ return "SaveIPFields"; }
  const char *port_count() const		{ return PORTS_1_1; }
  const char *processing() const		{ return AGNOSTIC; }
  
  Packet *simple_action(Packet *);
  
};

CLICK_ENDDECLS
#endif
