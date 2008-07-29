/** @file
 * IPRT - Network Protocols.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___iprt_netprotfmt_h
#define ___iprt_netprotfmt_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <VBox/types.h> /** @todo switch around PDMMAC and NETMAC. */
#include <iprt/assert.h>


__BEGIN_DECLS

/** @defgroup grp_rt_net     RTNet - Network Protocols
 * @ingroup grp_rt
 * @{
 */

/** A Ethernet MAC address. */
typedef PDMMAC RTNETMAC;
/** Pointer to an ethernet MAC address. */
typedef RTNETMAC *PRTNETMAC;
/** Pointer to a const ethernet MAC address. */
typedef RTNETMAC const *PCRTNETMAC;

/**
 * IPv4 address.
 */
typedef RTUINT32U RTNETADDRIPV4;
AssertCompileSize(RTNETADDRIPV4, 4);
/** Pointer to a IPv4 address. */
typedef RTNETADDRIPV4 *PRTNETADDRIPV4;
/** Pointer to a const IPv4 address. */
typedef RTNETADDRIPV4 const *PCRTNETADDRIPV4;

/**
 * IPv6 address.
 */
typedef RTUINT128U RTNETADDRIPV6;
AssertCompileSize(RTNETADDRIPV6, 16);
/** Pointer to a IPv4 address. */
typedef RTNETADDRIPV6 *PRTNETADDRIPV6;
/** Pointer to a const IPv4 address. */
typedef RTNETADDRIPV6 const *PCRTNETADDRIPV6;

/**
 * IPX address.
 */
#pragma pack(1)
typedef struct RTNETADDRIPX
{
    /** The network ID. */
    uint32_t Network;
    /** The node ID. (Defaults to the MAC address apparently.) */
    RTNETMAC Node;
} RTNETADDRIPX;
#pragma pack(0)
AssertCompileSize(RTNETADDRIPX, 4+6);
/** Pointer to an IPX address. */
typedef RTNETADDRIPX *PRTNETADDRIPX;
/** Pointer to a const IPX address. */
typedef RTNETADDRIPX const *PCRTNETADDRIPX;

/**
 * Address union.
 */
typedef union RTNETADDRU
{
    /** 64-bit view. */
    uint64_t au64[2];
    /** 32-bit view. */
    uint32_t au32[4];
    /** 16-bit view. */
    uint16_t au16[8];
    /** 8-bit view. */
    uint8_t  au8[16];
    /** IPv4 view. */
    RTNETADDRIPV4 IPv4;
    /** IPv6 view. */
    RTNETADDRIPV6 IPv6;
    /** IPX view. */
    RTNETADDRIPX Ipx;
    /** MAC address view. */
    RTNETMAC Mac;
} RTNETADDRU;
AssertCompileSize(RTNETADDRU, 16);
/** Pointer to an address union. */
typedef RTNETADDRU *PRTNETADDRU;
/** Pointer to a const address union. */
typedef RTNETADDRU const *PCRTNETADDRU;


/**
 * Ethernet header.
 */
#pragma pack(1)
typedef struct RTNETETHERHDR
{
    RTNETMAC    MacDst;
    RTNETMAC    MacSrc;
    /** Ethernet frame type or frame size, depending on the kind of ethernet.
     * This is big endian on the wire. */
    uint16_t    EtherType;
} RTNETETHERHDR;
#pragma pack()
AssertCompileSize(RTNETETHERHDR, 14);
/** Pointer to an ethernet header. */
typedef RTNETETHERHDR *PRTNETETHERHDR;
/** Pointer to a const ethernet header. */
typedef RTNETETHERHDR const *PCRTNETETHERHDR;

/** @name EtherType (RTNETETHERHDR::EtherType)
 * @{ */
#define RTNET_ETHERTYPE_IPV4   UINT16_C(0x0800)
#define RTNET_ETHERTYPE_ARP    UINT16_C(0x0806)
#define RTNET_ETHERTYPE_IPV6   UINT16_C(0x86dd)
/** @} */


/**
 * IPv4 header.
 * All is bigendian on the wire.
 */
#pragma pack(1)
typedef struct RTNETIPV4
{
#ifdef RT_BIG_ENDIAN
    unsigned int    ip_v : 4;
    unsigned int    ip_hl : 4;
    unsigned int    ip_tos : 8;
    unsigned int    ip_len : 16;
#else
    /** Header length given as a 32-bit word count. */
    unsigned int    ip_hl : 4;
    /** Header version. */
    unsigned int    ip_v : 4;
    /** Type of service. */
    unsigned int    ip_tos : 8;
    /** Total length (header + data). */
    unsigned int    ip_len : 16;
#endif
    /** Packet idenficiation. */
    uint16_t        ip_id;
    /** Offset if fragmented. */
    uint16_t        ip_off;
    /** Time to live. */
    uint8_t         ip_ttl;
    /** Protocol. */
    uint8_t         ip_p;
    /** Header check sum. */
    uint16_t        ip_sum;
    /** Source address. */
    uint32_t        ip_src;
    /** Destination address. */
    uint32_t        ip_dst;
    /** Options (optional). */
    uint32_t        ip_options[1];
} RTNETIPV4;
#pragma pack(0)
AssertCompileSize(RTNETIPV4, 6 * 4);
/** Pointer to a IPv4 header. */
typedef RTNETIPV4 *PRTNETIPV4;
/** Pointer to a const IPv4 header. */
typedef RTNETIPV4 const *PCRTNETIPV4;

/** The minimum IPv4 header length (in bytes).
 * Up to and including RTNETIPV4::ip_dst. */
#define RTNETIPV4_MIN_LEN   (20)


/** @name IPv4 Protocol Numbers
 * @{ */
/** IPv4: ICMP */
#define RTNETIPV4_PROT_ICMP (0)
/** IPv4: TCP */
#define RTNETIPV4_PROT_TCP  (6)
/** IPv4: UDP */
#define RTNETIPV4_PROT_UDP  (17)
/** @} */


/**
 * UDP header.
 */
#pragma pack(1)
typedef struct RTNETUDP
{
    /** The source port. */
    uint16_t    uh_sport;
    /** The destination port. */
    uint16_t    uh_dport;
    /** The length of the UDP header and associated data. */
    uint16_t    uh_ulen;
    /** The checksum of the pseudo header, the UDP header and the data. */
    uint16_t    uh_sum;
} RTNETUDP;
#pragma pack(0)
AssertCompileSize(RTNETUDP, 8);
typedef RTNETUDP *PRTNETUDP;
typedef RTNETUDP const *PCRTNETUDP;

/** The minimum IPv4 packet length (in bytes). (RTNETUDP::uh_ulen) */
#define RTNETUDP_MIN_LEN   (8)


/**
 * IPv4 DHCP packet.
 */
#pragma pack(1)
typedef struct RTNETDHCP
{
    uint8_t         Op;
    /** Hardware address type. */
    uint8_t         HType;
    /** Hardware address length. */
    uint8_t         HLen;
    uint8_t         Hops;
    uint32_t        XID;
    uint16_t        Secs;
    uint16_t        Flags;
    /** Client IPv4 address. */
    RTNETADDRIPV4   CIAddr;
    /** Your IPv4 address. */
    RTNETADDRIPV4   YIAddr;
    /** Server IPv4 address. */
    RTNETADDRIPV4   SIAddr;
    /** Gateway IPv4 address. */
    RTNETADDRIPV4   GIAddr;
    /** Client hardware address. */
    uint8_t         CHAddr[16];
    /** Server name. */
    uint8_t         SName[64];
    uint8_t         File[128];
    uint8_t         abMagic[4];
    uint8_t         DhcpOpt;
    uint8_t         DhcpLen; /* 1 */
    uint8_t         DhcpReq;
    uint8_t         abOptions[57];
} RTNETDHCP;
#pragma pack(0)
/// @todo AssertCompileSize(RTNETDHCP, );
/** Pointer to a DHCP packet. */
typedef RTNETDHCP *PRTNETDHCP;
/** Pointer to a const DHCP packet. */
typedef RTNETDHCP const *PCRTNETDHCP;


/**
 * Ethernet ARP header.
 */
#pragma pack(1)
typedef struct RTNETARPHDR
{
    /** The hardware type. */
    uint16_t    ar_htype;
    /** The protocol type (ethertype). */
    uint16_t    ar_ptype;
    /** The hardware address length. */
    uint8_t     ar_hlen;
    /** The protocol address length. */
    uint8_t     ar_plen;
    /** The operation. */
    uint16_t    ar_oper;
} RTNETARPHDR;
#pragma pack(0)
AssertCompileSize(RTNETARPHDR, 8);
/** Pointer to an ethernet ARP header. */
typedef RTNETARPHDR *PRTNETARPHDR;
/** Pointer to a const ethernet ARP header. */
typedef RTNETARPHDR const *PCRTNETARPHDR;

/** ARP hardware type - ethernet. */
#define RTNET_ARP_ETHER            UINT16_C(1)

/** @name ARP operations
 * @{ */
#define RTNET_ARPOP_REQUEST        UINT16_C(1) /**< Request hardward address given a protocol address (ARP). */
#define RTNET_ARPOP_REPLY          UINT16_C(2)
#define RTNET_ARPOP_REVREQUEST     UINT16_C(3) /**< Request protocol address given a hardware address (RARP). */
#define RTNET_ARPOP_REVREPLY       UINT16_C(4)
#define RTNET_ARPOP_INVREQUEST     UINT16_C(8) /**< Inverse ARP.  */
#define RTNET_ARPOP_INVREPLY       UINT16_C(9)
/** Check if an ARP operation is a request or not. */
#define RTNET_ARPOP_IS_REQUEST(Op) ((Op) & 1)
/** Check if an ARP operation is a reply or not. */
#define RTNET_ARPOP_IS_REPLY(Op)   (!RTNET_ARPOP_IS_REQUEST(Op))
/** @} */


/**
 * Ethernet IPv4 + 6-byte MAC ARP request packet.
 */
#pragma pack(1)
typedef struct RTNETARPIPV4
{
    /** ARP header. */
    RTNETARPHDR     Hdr;
    /** The sender hardware address. */
    RTNETMAC        ar_sha;
    /** The sender protocol address. */
    RTNETADDRIPV4   ar_spa;
    /** The target hardware address. */
    RTNETMAC        ar_tha;
    /** The arget protocol address. */
    RTNETADDRIPV4   ar_tpa;
} RTNETARPIPV4;
#pragma pack(0)
AssertCompileSize(RTNETARPIPV4, 8+6+4+6+4);
/** Pointer to an ethernet IPv4+MAC ARP request packet. */
typedef RTNETARPIPV4 *PRTNETARPIPV4;
/** Pointer to a const ethernet IPv4+MAC ARP request packet. */
typedef RTNETARPIPV4 const *PCRTNETARPIPV4;


/** @todo RTNETNDP (IPv6)*/


/** @} */

__END_DECLS

#endif



