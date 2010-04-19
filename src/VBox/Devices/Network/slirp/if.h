/* $Id$ */
/** @file
 * NAT - if_*.
 */

/*
 * Copyright (C) 2006-2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*
 * This code is based on:
 *
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#ifndef _IF_H_
#define _IF_H_

#define IF_COMPRESS     0x01    /* We want compression */
#define IF_NOCOMPRESS   0x02    /* Do not do compression */
#define IF_AUTOCOMP     0x04    /* Autodetect (default) */
#define IF_NOCIDCOMP    0x08    /* CID compression */

/* Needed for FreeBSD */
#undef if_mtu
extern int      if_mtu;
extern int      if_mru;         /* MTU and MRU */
extern int      if_comp;        /* Flags for compression */
extern int      if_maxlinkhdr;
extern int      if_queued;      /* Number of packets queued so far */
extern int      if_thresh;      /* Number of packets queued before we start sending
                                 * (to prevent allocing too many mbufs) */

extern  struct mbuf if_fastq;   /* fast queue (for interactive data) */
extern  struct mbuf if_batchq;  /* queue for non-interactive data */
extern  struct mbuf *next_m;

#define ifs_init(ifm) ((ifm)->ifs_next = (ifm)->ifs_prev = (ifm))

#ifdef ETH_P_ARP
# undef ETH_P_ARP
#endif /* ETH_P_ARP*/
#define ETH_P_ARP       0x0806          /* Address Resolution packet    */

#ifdef ETH_P_IP
# undef ETH_P_IP
#endif /* ETH_P_IP */
#define ETH_P_IP        0x0800          /* Internet Protocol packet     */

#ifdef ETH_P_IPV6
# undef ETH_P_IPV6
#endif /* ETH_P_IPV6 */
#define ETH_P_IPV6      0x86DD          /* IPv6 */

#endif
