/*
 * Copyright (c) 1982, 1986, 1993
 *      The Regents of the University of California.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *      This product includes software developed by the University of
 *      California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *      @(#)ip.h        8.1 (Berkeley) 6/10/93
 * ip.h,v 1.3 1994/08/21 05:27:30 paul Exp
 */

#ifndef _IP_H_
#define _IP_H_

#ifdef VBOX_WITH_BSD_REASS
# include "queue.h"
#endif

#ifdef WORDS_BIGENDIAN
# ifndef NTOHL
#  define NTOHL(d)
# endif
# ifndef NTOHS
#  define NTOHS(d)
# endif
# ifndef HTONL
#  define HTONL(d)
# endif
# ifndef HTONS
#  define HTONS(d)
# endif
#else
# ifndef NTOHL
#  define NTOHL(d) ((d) = ntohl((d)))
# endif
# ifndef NTOHS
#  define NTOHS(d) ((d) = ntohs((u_int16_t)(d)))
# endif
# ifndef HTONL
#  define HTONL(d) ((d) = htonl((d)))
# endif
# ifndef HTONS
#  define HTONS(d) ((d) = htons((u_int16_t)(d)))
# endif
#endif

/*
 * Definitions for internet protocol version 4.
 * Per RFC 791, September 1981.
 */
#define IPVERSION       4

/*
 * Structure of an internet header, naked of options.
 */
struct ip
{
#ifdef WORDS_BIGENDIAN
# ifdef _MSC_VER
    uint8_t        ip_v:4;     /* version */
    uint8_t        ip_hl:4;    /* header length */
# else
    unsigned       ip_v:4;     /* version */
    unsigned       ip_hl:4;    /* header length */
# endif
#else
# ifdef _MSC_VER
    uint8_t        ip_hl:4;    /* header length */
    uint8_t        ip_v:4;     /* version */
# else
    unsigned       ip_hl:4;    /* header length */
    unsigned       ip_v:4;     /* version */
# endif
#endif
    uint8_t        ip_tos;     /* type of service */
    uint16_t       ip_len;     /* total length */
    uint16_t       ip_id;      /* identification */
    uint16_t       ip_off;     /* fragment offset field */
#define IP_DF       0x4000     /* don't fragment flag */
#define IP_MF       0x2000     /* more fragments flag */
#define IP_OFFMASK  0x1fff     /* mask for fragmenting bits */
    uint8_t        ip_ttl;     /* time to live */
    uint8_t        ip_p;       /* protocol */
    uint16_t       ip_sum;     /* checksum */
    struct in_addr ip_src;     /* source address */
    struct in_addr ip_dst;     /* destination address */
};
AssertCompileSize(struct ip, 20);

#define IP_MAXPACKET    65535  /* maximum packet size */

/*
 * Definitions for IP type of service (ip_tos)
 */
#define IPTOS_LOWDELAY          0x10
#define IPTOS_THROUGHPUT        0x08
#define IPTOS_RELIABILITY       0x04

/*
 * Definitions for options.
 */
#define IPOPT_COPIED(o)         ((o)&0x80)
#define IPOPT_CLASS(o)          ((o)&0x60)
#define IPOPT_NUMBER(o)         ((o)&0x1f)

#define IPOPT_CONTROL           0x00
#define IPOPT_RESERVED1         0x20
#define IPOPT_DEBMEAS           0x40
#define IPOPT_RESERVED2         0x60

#define IPOPT_EOL               0     /* end of option list */
#define IPOPT_NOP               1     /* no operation */

#define IPOPT_RR                7     /* record packet route */
#define IPOPT_TS                68    /* timestamp */
#define IPOPT_SECURITY          130   /* provide s,c,h,tcc */
#define IPOPT_LSRR              131   /* loose source route */
#define IPOPT_SATID             136   /* satnet id */
#define IPOPT_SSRR              137   /* strict source route */

/*
 * Offsets to fields in options other than EOL and NOP.
 */
#define IPOPT_OPTVAL            0     /* option ID */
#define IPOPT_OLEN              1     /* option length */
#define IPOPT_OFFSET            2     /* offset within option */
#define IPOPT_MINOFF            4     /* min value of above */

/*
 * Time stamp option structure.
 */
struct  ip_timestamp
{
    uint8_t        ipt_code;          /* IPOPT_TS */
    uint8_t        ipt_len;           /* size of structure (variable) */
    uint8_t        ipt_ptr;           /* index of current entry */
#ifdef WORDS_BIGENDIAN
# ifdef _MSC_VER
    uint8_t        ipt_oflw:4;        /* overflow counter */
    uint8_t        ipt_flg:4;         /* flags, see below */
# else
    unsigned       ipt_oflw:4;        /* overflow counter */
    unsigned       ipt_flg:4;         /* flags, see below */
# endif
#else
# ifdef _MSC_VER
    uint8_t        ipt_flg:4;         /* flags, see below */
    uint8_t        ipt_oflw:4;        /* overflow counter */
# else
    unsigned       ipt_flg:4;         /* flags, see below */
    unsigned       ipt_oflw:4;        /* overflow counter */
# endif
#endif
    union ipt_timestamp
    {
        uint32_t           ipt_time[1];
        struct ipt_ta
        {
            struct in_addr ipt_addr;
            uint32_t       ipt_time;
        } ipt_ta[1];
    } ipt_timestamp;
};
AssertCompileSize(struct ip_timestamp, 12);

/* flag bits for ipt_flg */
#define IPOPT_TS_TSONLY         0     /* timestamps only */
#define IPOPT_TS_TSANDADDR      1     /* timestamps and addresses */
#define IPOPT_TS_PRESPEC        3     /* specified modules only */

/* bits for security (not byte swapped) */
#define IPOPT_SECUR_UNCLASS     0x0000
#define IPOPT_SECUR_CONFID      0xf135
#define IPOPT_SECUR_EFTO        0x789a
#define IPOPT_SECUR_MMMM        0xbc4d
#define IPOPT_SECUR_RESTR       0xaf13
#define IPOPT_SECUR_SECRET      0xd788
#define IPOPT_SECUR_TOPSECRET   0x6bc5

/*
 * Internet implementation parameters.
 */
#define MAXTTL          255           /* maximum time to live (seconds) */
#define IPDEFTTL        64            /* default ttl, from RFC 1340 */
#define IPFRAGTTL       60            /* time to live for frags, slowhz */
#define IPTTLDEC        1             /* subtracted when forwarding */

#define IP_MSS          576           /* default maximum segment size */

#ifdef HAVE_SYS_TYPES32_H  /* Overcome some Solaris 2.x junk */
# include <sys/types32.h>
#else
# if SIZEOF_CHAR_P == 4
typedef caddr_t caddr32_t;
# else
#  if !defined(VBOX_WITH_BSD_REASS)
typedef u_int32_t caddr32_t;
#  else /* VBOX_WITH_BSD_REASS */
typedef caddr_t caddr32_t;
#  endif /* VBOX_WITH_BSD_REASS */
# endif
#endif

#if SIZEOF_CHAR_P == 4
typedef struct ipq_t *ipqp_32;
typedef struct ipasfrag *ipasfragp_32;
#else
typedef caddr32_t ipqp_32;
typedef caddr32_t ipasfragp_32;
#endif

/*
 * Overlay for ip header used by other protocols (tcp, udp).
 */
struct ipovly
{
#if !defined(VBOX_WITH_BSD_REASS)
    caddr32_t       ih_next;
    caddr32_t       ih_prev;          /* for protocol sequence q's */
    u_int8_t        ih_x1;            /* (unused) */
#else /* VBOX_WITH_BSD_REASS */
    u_int8_t        ih_x1[9];         /* (unused) */
#endif /* VBOX_WITH_BSD_REASS */
    u_int8_t        ih_pr;            /* protocol */
    u_int16_t       ih_len;           /* protocol length */
    struct  in_addr ih_src;           /* source internet address */
    struct  in_addr ih_dst;           /* destination internet address */
};
AssertCompileSize(struct ipovly, 20);

/*
 * Ip reassembly queue structure.  Each fragment being reassembled is
 * attached to one of these structures. They are timed out after ipq_ttl
 * drops to 0, and may also be reclaimed if memory becomes tight.
 * size 28 bytes
 */
struct ipq_t
{
#ifndef VBOX_WITH_BSD_REASS
    ipqp_32         next;
    ipqp_32         prev;            /* to other reass headers */
#else  /* VBOX_WITH_BSD_REASS */
    TAILQ_ENTRY(ipq_t) ipq_list;
#endif /* VBOX_WITH_BSD_REASS */
    u_int8_t        ipq_ttl;         /* time for reass q to live */
    u_int8_t        ipq_p;           /* protocol of this fragment */
    u_int16_t       ipq_id;          /* sequence id for reassembly */
#ifndef VBOX_WITH_BSD_REASS
    ipasfragp_32    ipq_next;
    ipasfragp_32    ipq_prev;        /* to ip headers of fragments */
#else  /* VBOX_WITH_BSD_REASS */
    struct mbuf     *ipq_frags;      /* to ip headers of fragments */
    uint8_t         ipq_nfrags;      /* # of fragments in this packet */
#endif /* VBOX_WITH_BSD_REASS */
    struct in_addr  ipq_src;
    struct in_addr  ipq_dst;
};

#ifdef VBOX_WITH_BSD_REASS
/*
* IP datagram reassembly.
*/
#define IPREASS_NHASH_LOG2      6
#define IPREASS_NHASH           (1 << IPREASS_NHASH_LOG2)
#define IPREASS_HMASK           (IPREASS_NHASH - 1)
#define IPREASS_HASH(x,y) \
(((((x) & 0xF) | ((((x) >> 8) & 0xF) << 4)) ^ (y)) & IPREASS_HMASK)
TAILQ_HEAD(ipqhead,ipq_t);
#endif /* VBOX_WITH_BSD_REASS */

/*
 * Ip header, when holding a fragment.
 *
 * Note: ipf_next must be at same offset as ipq_next above
 */
struct  ipasfrag
{
#ifdef WORDS_BIGENDIAN
# ifdef _MSC_VER
    uint8_t      ip_v:4;
    uint8_t      ip_hl:4;
# else
    unsigned     ip_v:4;
    unsigned     ip_hl:4;
# endif
#else
# ifdef _MSC_VER
    uint8_t      ip_hl:4;
    uint8_t      ip_v:4;
# else
    unsigned     ip_hl:4;
    unsigned     ip_v:4;
# endif
#endif
    u_int8_t     ipf_mff;           /* XXX overlays ip_tos: use low bit
                                     * to avoid destroying tos (PPPDTRuu);
                                     * copied from (ip_off & IP_MF) */
    u_int16_t    ip_len;
    u_int16_t    ip_id;
    u_int16_t    ip_off;
    u_int8_t     ip_ttl;
    u_int8_t     ip_p;
    u_int16_t    ip_sum;
    ipasfragp_32 ipf_next;          /* next fragment */
    ipasfragp_32 ipf_prev;          /* previous fragment */
};
AssertCompileSize(struct ipasfrag, 20);

/*
 * Structure stored in mbuf in inpcb.ip_options
 * and passed to ip_output when ip options are in use.
 * The actual length of the options (including ipopt_dst)
 * is in m_len.
 */
#define MAX_IPOPTLEN    40

struct ipoption
{
    struct in_addr ipopt_dst;      /* first-hop dst if source routed */
    int8_t         ipopt_list[MAX_IPOPTLEN]; /* options proper */
};

/*
 * Structure attached to inpcb.ip_moptions and
 * passed to ip_output when IP multicast options are in use.
 */

struct  ipstat_t
{
    u_long  ips_total;              /* total packets received */
    u_long  ips_badsum;             /* checksum bad */
    u_long  ips_tooshort;           /* packet too short */
    u_long  ips_toosmall;           /* not enough data */
    u_long  ips_badhlen;            /* ip header length < data size */
    u_long  ips_badlen;             /* ip length < ip header length */
    u_long  ips_fragments;          /* fragments received */
    u_long  ips_fragdropped;        /* frags dropped (dups, out of space) */
    u_long  ips_fragtimeout;        /* fragments timed out */
    u_long  ips_forward;            /* packets forwarded */
    u_long  ips_cantforward;        /* packets rcvd for unreachable dest */
    u_long  ips_redirectsent;       /* packets forwarded on same net */
    u_long  ips_noproto;            /* unknown or unsupported protocol */
    u_long  ips_delivered;          /* datagrams delivered to upper level*/
    u_long  ips_localout;           /* total ip packets generated here */
    u_long  ips_odropped;           /* lost packets due to nobufs, etc. */
    u_long  ips_reassembled;        /* total packets reassembled ok */
    u_long  ips_fragmented;         /* datagrams successfully fragmented */
    u_long  ips_ofragments;         /* output fragments created */
    u_long  ips_cantfrag;           /* don't fragment flag was set, etc. */
    u_long  ips_badoptions;         /* error in option processing */
    u_long  ips_noroute;            /* packets discarded due to no route */
    u_long  ips_badvers;            /* ip version != 4 */
    u_long  ips_rawout;             /* total raw ip packets generated */
    u_long  ips_unaligned;          /* times the ip packet was not aligned */
};

#endif
