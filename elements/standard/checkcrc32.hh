#ifndef CLICK_CHECKCRC32_HH
#define CLICK_CHECKCRC32_HH
#include <click/element.hh>
#include <click/atomic.hh>
CLICK_DECLS

/*
 * =c
 * CheckCRC32()
 * =s checking
 * checks packet CRC32s
 * =d
 * Check that the CRC32 appended by SetCRC32 is OK.
 * If so, delete the CRC from the packet.
 * Otherwise, drop the packet.
 * =a SetCRC32
 */

class EtherAddress;

class CheckCRC32 : public Element {
public:
  CheckCRC32();
  ~CheckCRC32();

  const char *class_name() const		{ return "CheckCRC32"; }
  const char *processing() const		{ return AGNOSTIC; }
  
  CheckCRC32 *clone() const			{ return new CheckCRC32; }
  
  Packet *simple_action(Packet *);

private:

  atomic_uint32_t _drops;

};

CLICK_ENDDECLS
#endif
