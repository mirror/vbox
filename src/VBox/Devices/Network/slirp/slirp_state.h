/** @file
 * NAT state/configuration.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

#ifndef _slirp_state_h_
#define _slirp_state_h_
#ifdef VBOX_WITH_SYNC_SLIRP
#include <iprt/semaphore.h>
#endif

/** Number of DHCP clients supported by NAT. */
#define NB_ADDR     16

/** Where to start DHCP IP number allocation. */
#define START_ADDR  15

/** DHCP Lease time. */
#define LEASE_TIME (24 * 3600)

/** Entry in the table of known DHCP clients. */
typedef struct {
    bool allocated;
    uint8_t macaddr[6];
} BOOTPClient;


/** TFTP session entry. */
struct tftp_session {
    int in_use;
    unsigned char filename[TFTP_FILENAME_MAX];

    struct in_addr client_ip;
    u_int16_t client_port;

    int timestamp;
};


/** Main state/configuration structure for slirp NAT. */
typedef struct NATState
{
    /* Stuff from boot.c */
    BOOTPClient bootp_clients[NB_ADDR];
    const char *bootp_filename;
    /* Stuff from if.c */
    int if_mtu, if_mru;
    int if_comp;
    int if_maxlinkhdr;
    int if_queued;
#ifdef VBOX_WITH_SYNC_SLIRP
    /* mutex for accessing if_queued flag which used in if_start function and
     * and understanding that we need call if_start to send anything we have to
     * send.
     */
    RTSEMMUTEX  if_queued_mutex;
#endif
    int if_thresh;
    struct mbuf if_fastq;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * if_fastq_mutex prevent concurrent adding/removing mbufs on
     * fast queue (mbufs) from this queue are processed in first order
     */
    RTSEMMUTEX  if_fastq_mutex;
#endif
    struct mbuf if_batchq;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * if_batchq_mutex prevent concurent adding/removing mbufs on
     * batch queue mbufs from this queue used if no mbufs on fast queue
     * and next_m doesn't point on mbuf scheduled to be processesed
     */
    RTSEMMUTEX  if_batchq_mutex;
#endif
    struct mbuf *next_m;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * next_m_mutex prevent concurrent assigning/reading
     * from pointer next_m, used for pointing next mbuf to be processed
     * it readed if no messages are not in fast queue, usually it assigned with
     * mbuf from batch queue
     */
    RTSEMMUTEX  next_m_mutex;
#endif
    /* Stuff from icmp.c */
    struct icmpstat_t icmpstat;
    /* Stuff from ip_input.c */
    struct ipstat_t ipstat;
    struct ipq_t ipq;
    uint16_t ip_currid;
    /* Stuff from mbuf.c */
    int mbuf_alloced, mbuf_max;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * mbuf_alloced_mutex used to prevent concurent access to mbuf_alloced counter
     * which ticks on every allocation and readed to check it against limits
     */
    RTSEMMUTEX  mbuf_alloced_mutex;
#endif
    int msize;
    struct mbuf m_freelist, m_usedlist;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * m_freelist_mutex and m_usedlist_mutex are used to prevent concurrent access and modifications
     * of corresponded queues controlling allocation/utilization of mbufs
     */
    RTSEMMUTEX  m_freelist_mutex;
    RTSEMMUTEX  m_usedlist_mutex;
#endif
    /* Stuff from slirp.c */
    void *pvUser;
    uint32_t curtime;
    uint32_t time_fasttimo;
    uint32_t last_slowtimo;
    bool do_slowtimo;
    bool link_up;
    struct timeval tt;
    struct in_addr our_addr;
    struct in_addr alias_addr;
    struct in_addr special_addr;
    struct in_addr dns_addr;
    struct in_addr loopback_addr;
    uint32_t netmask;
    uint8_t client_ethaddr[6];
    struct ex_list *exec_list;
    char slirp_hostname[33];
    bool fPassDomain;
    const char *pszDomain;
    /* Stuff from tcp_input.c */
    struct socket tcb;
    struct socket *tcp_last_so;
    tcp_seq tcp_iss;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * tcp_last_so_mutex used for control access to tcp_last_so pointer
     */
    RTSEMMUTEX tcp_last_so_mutex;
    /*
     * tcb_mutex used for control access to tcb queue of sockets
     * servising TCP connections
     */
    RTSEMMUTEX tcb_mutex;
#endif
#if ARCH_BITS == 64
    /* Stuff from tcp_subr.c */
    void *apvHash[16384];
    uint32_t cpvHashUsed;
    uint32_t cpvHashCollisions;
    uint64_t cpvHashInserts;
    uint64_t cpvHashDone;
#endif
    /* Stuff from tcp_timer.c */
    struct tcpstat_t tcpstat;
    uint32_t tcp_now;
    /* Stuff from tftp.c */
    struct tftp_session tftp_sessions[TFTP_SESSIONS_MAX];
    const char *tftp_prefix;
    /* Stuff from udp.c */
    struct udpstat_t udpstat;
    struct socket udb;
    struct socket *udp_last_so;
#ifdef VBOX_WITH_SYNC_SLIRP
    /*
     * udb_mutex used in similar to tcb_mutex way, but for handling udp connections
     */
    RTSEMMUTEX udb_mutex;
    /*
     * used for access udp_last_so global pointer avoiding overusing of udb_mutex.
     */
    RTSEMMUTEX udp_last_so_mutex;
#endif
} NATState;


/** Default IP time to live. */
#define ip_defttl IPDEFTTL

/** Number of permanent buffers in mbuf. */
#define mbuf_thresh 30

/** Use a fixed time before sending keepalive. */
#define tcp_keepidle TCPTV_KEEP_IDLE

/** Use a fixed interval between keepalive. */
#define tcp_keepintvl TCPTV_KEEPINTVL

/** Maximum idle time before timing out a connection. */
#define tcp_maxidle (TCPTV_KEEPCNT * tcp_keepintvl)

/** Default TCP socket options. */
#define so_options DO_KEEPALIVE

/** Default TCP MSS value. */
#define tcp_mssdflt TCP_MSS

/** Default TCP round trip time. */
#define tcp_rttdflt (TCPTV_SRTTDFLT / PR_SLOWHZ)

/** Enable RFC1323 performance enhancements.
 * @todo check if it really works, it was turned off before. */
#define tcp_do_rfc1323 1

/** TCP receive buffer size. */
#define tcp_rcvspace TCP_RCVSPACE

/** TCP receive buffer size. */
#define tcp_sndspace TCP_SNDSPACE

/* TCP duplicate ACK retransmit threshold. */
#define tcprexmtthresh 3


#define bootp_filename pData->bootp_filename
#define bootp_clients pData->bootp_clients

#define if_mtu pData->if_mtu
#define if_mru pData->if_mru
#define if_comp pData->if_comp
#define if_maxlinkhdr pData->if_maxlinkhdr
#define if_queued pData->if_queued
#define if_thresh pData->if_thresh
#define if_fastq pData->if_fastq
#define if_batchq pData->if_batchq
#define next_m pData->next_m

#define icmpstat pData->icmpstat

#define ipstat pData->ipstat
#define ipq pData->ipq
#define ip_currid pData->ip_currid

#define mbuf_alloced pData->mbuf_alloced
#define mbuf_max pData->mbuf_max
#define msize pData->msize
#define m_freelist pData->m_freelist
#define m_usedlist pData->m_usedlist

#define curtime pData->curtime
#define time_fasttimo pData->time_fasttimo
#define last_slowtimo pData->last_slowtimo
#define do_slowtimo pData->do_slowtimo
#define link_up pData->link_up
#define cUsers pData->cUsers
#define tt pData->tt
#define our_addr pData->our_addr
#define alias_addr pData->alias_addr
#define special_addr pData->special_addr
#define dns_addr pData->dns_addr
#define loopback_addr pData->loopback_addr
#define client_ethaddr pData->client_ethaddr
#define exec_list pData->exec_list
#define slirp_hostname pData->slirp_hostname

#define tcb pData->tcb
#define tcp_last_so pData->tcp_last_so
#define tcp_iss pData->tcp_iss

#define tcpstat pData->tcpstat
#define tcp_now pData->tcp_now

#define tftp_sessions pData->tftp_sessions
#define tftp_prefix pData->tftp_prefix

#define udpstat pData->udpstat
#define udb pData->udb
#define udp_last_so pData->udp_last_so


#if SIZEOF_CHAR_P != 4
    extern void     VBoxU32PtrDone(PNATState pData, void *pv, uint32_t iHint);
    extern uint32_t VBoxU32PtrHashSlow(PNATState pData, void *pv);

    /** Hash the pointer, inserting it if need be. */
    DECLINLINE(uint32_t) VBoxU32PtrHash(PNATState pData, void *pv)
    {
        uint32_t i = ((uintptr_t)pv >> 3) % RT_ELEMENTS(pData->apvHash);
        if (RT_LIKELY(pData->apvHash[i] == pv && pv))
            return i;
        return VBoxU32PtrHashSlow(pData, pv);
    }
    /** Lookup the hash value. */
    DECLINLINE(void *) VBoxU32PtrLookup(PNATState pData, uint32_t i)
    {
        void *pv;
        Assert(i < RT_ELEMENTS(pData->apvHash));
        pv = pData->apvHash[i];
        Assert(pv || !i);
        return pv;
    }
#endif


#endif /* !_slirp_state_h_ */
