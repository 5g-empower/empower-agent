// -*- mode: c++; c-basic-offset: 4 -*-
/*
 * fakepcap.{cc,hh} -- a faked-up pcap-like interface
 * Eddie Kohler
 *
 * Copyright (c) 2001 International Computer Science Institute
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
#include "fakepcap.hh"
#include <clicknet/ip.h>
#include <clicknet/ip6.h>
#include <clicknet/ether.h>
#include <clicknet/fddi.h>
#include <clicknet/rfc1483.h>
CLICK_DECLS

int
fake_pcap_parse_dlt(const String &str)
{
    if (str == "IP")
	return FAKE_DLT_RAW;
    else if (str == "ETHER")
	return FAKE_DLT_EN10MB;
    else if (str == "FDDI")
	return FAKE_DLT_FDDI;
    else if (str == "ATM" || str == "RFC1483" || str == "ATM-RFC1483")
	return FAKE_DLT_ATM_RFC1483;
    else if (str == "802.11")
	return FAKE_DLT_IEEE802_11;
    else if (str == "SLL")
	return FAKE_DLT_LINUX_SLL;
    else if (str == "AIRONET")
	return FAKE_DLT_AIRONET_HEADER;
    else if (str == "HDLC")
	return FAKE_DLT_C_HDLC;
    else
	return -1;
}

const char *
fake_pcap_unparse_dlt(int dlt)
{
    switch (dlt) {
      case FAKE_DLT_RAW:
	return "IP";
      case FAKE_DLT_EN10MB:
	return "ETHER";
      case FAKE_DLT_FDDI:
	return "FDDI";
      case FAKE_DLT_ATM_RFC1483:
	return "ATM";
      case FAKE_DLT_IEEE802_11:
	return "802.11";
      case FAKE_DLT_LINUX_SLL:
	return "SLL";
      case FAKE_DLT_AIRONET_HEADER:
	return "AIRONET";
      case FAKE_DLT_C_HDLC:
	return "HDLC";
      default:
	return "??";
    }
}

// Handling FORCE_IP.

bool
fake_pcap_dlt_force_ipable(int dlt)
{
    return (dlt == FAKE_DLT_RAW || dlt == FAKE_DLT_EN10MB
	    || dlt == FAKE_DLT_FDDI || dlt == FAKE_DLT_ATM_RFC1483
	    || dlt == FAKE_DLT_LINUX_SLL || dlt == FAKE_DLT_C_HDLC);
}

#if HAVE_INDIFFERENT_ALIGNMENT
#define UNALIGNED_NET_SHORT_EQ(x, y) ((x) == htons((y)))
#else
static inline uint16_t
unaligned_net_short(const void *v)
{
    const uint8_t *d = reinterpret_cast<const uint8_t *>(v);
    return (d[0] << 8) | d[1];
}
#define UNALIGNED_NET_SHORT_EQ(x, y) (unaligned_net_short(&(x)) == (y))
#endif

#define IP_ETHERTYPE(et)	(UNALIGNED_NET_SHORT_EQ((et), ETHERTYPE_IP) || UNALIGNED_NET_SHORT_EQ((et), ETHERTYPE_IP6))

// NB: May change 'p', but will never free it.
bool
fake_pcap_force_ip(Packet *&p, int dlt)
{
    const click_ip *iph = 0;
    switch (dlt) {

      case FAKE_DLT_RAW: {
	  iph = (const click_ip *) p->data();
	  break;
      }

      case FAKE_DLT_EN10MB: {
	  const click_ether *ethh = (const click_ether *) p->data();
	  if (p->length() >= sizeof(click_ether)
	      && IP_ETHERTYPE(ethh->ether_type))
	      iph = (const click_ip *)(ethh + 1);
	  break;
      }

      case FAKE_DLT_FDDI: {
	  const click_fddi *fh = (const click_fddi *) p->data();
	  if (p->length() < sizeof(click_fddi_snap)
	      || (fh->fc & FDDI_FC_LLCMASK) != FDDI_FC_LLC_ASYNC)
	      break;
	  const click_fddi_snap *fsh = (const click_fddi_snap *) fh;
	  if (memcmp(&fsh->dsap, FDDI_SNAP_EXPECTED, FDDI_SNAP_EXPECTED_LEN) == 0
	      && IP_ETHERTYPE(fsh->ether_type))
	      iph = (const click_ip *) (fsh + 1);
	  break;
      }

      case FAKE_DLT_ATM_RFC1483: {
	  const click_rfc1483 *rh = (const click_rfc1483 *) p->data();
	  if (p->length() >= sizeof(click_rfc1483)
	      && memcmp(rh->snap, RFC1483_SNAP_EXPECTED, RFC1483_SNAP_EXPECTED_LEN) == 0
	      && IP_ETHERTYPE(rh->ether_type))
	      iph = (const click_ip *) (rh + 1);
	  break;
      }

      case FAKE_DLT_LINUX_SLL: {
	  CLICK_SIZE_PACKED_STRUCTURE(
	  struct linux_sll {,
	      uint16_t sll_pkttype;
	      uint16_t sll_hatype;
	      uint16_t sll_halen;
	      uint8_t sll_addr[8];
	      uint16_t sll_protocol;
	  });
	  const linux_sll *sllh = (const linux_sll *) p->data();
	  if (p->length() >= sizeof(linux_sll) &&
	      IP_ETHERTYPE(sllh->sll_protocol))
	      iph = (const click_ip *)(sllh + 1);
	  break;
      }

      case FAKE_DLT_C_HDLC: {
	  struct click_pcap_hdlc {
	      uint16_t hdlc_address;
	      uint16_t hdlc_protocol;
	  };
	  const click_pcap_hdlc *hdlch = (const click_pcap_hdlc *) p->data();
	  if (p->length() >= sizeof(click_pcap_hdlc) &&
	      IP_ETHERTYPE(hdlch->hdlc_protocol))
	      iph = (const click_ip *)(hdlch + 1);
	  break;
      }
      
      default:
	break;

    }

    if (!iph)
	return false;

#if !HAVE_INDIFFERENT_ALIGNMENT
    // Machine may crash if we try to access 'iph'. Align it on a word
    // boundary.
    uintptr_t header_ptr = reinterpret_cast<uintptr_t>(iph);
    if (header_ptr & 3) {
	int header_off = header_ptr - reinterpret_cast<uintptr_t>(p->data());
	if (Packet *q = p->shift_data(-(header_ptr & 3), false)) {
	    p = q;
	    iph = reinterpret_cast<const click_ip *>(q->data() + header_off);
	} else			// cannot align; return it as a non-IP packet
	    return false;
    }
#endif
    
    if (iph->ip_v == 4) {
	int offset = (const uint8_t *) iph - p->data();
	if (iph->ip_hl >= 5
	    && p->length() - offset >= (iph->ip_hl << 2)) {
	    p->set_ip_header(iph, iph->ip_hl << 2);
	    p->set_dst_ip_anno(iph->ip_dst);
	    return true;
	}
    } else if (iph->ip_v == 6) {
	int offset = (const uint8_t *) iph - p->data();
	if (p->length() - offset >= sizeof(click_ip6)) {
	    p->set_ip6_header((const click_ip6 *) iph);
	    return true;
	}
    }

    return false;
}

CLICK_ENDDECLS
ELEMENT_REQUIRES(userlevel|ns)
ELEMENT_PROVIDES(FakePcap)
