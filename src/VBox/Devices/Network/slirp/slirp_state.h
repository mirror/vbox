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
#include "ip_icmp.h"

/** Number of DHCP clients supported by NAT. */
#define NB_ADDR     16

/** Where to start DHCP IP number allocation. */
#define START_ADDR  15

/** DHCP Lease time. */
#define LEASE_TIME (24 * 3600)

/** Entry in the table of known DHCP clients. */
typedef struct
{
    bool allocated;
    uint8_t macaddr[6];
} BOOTPClient;


/** TFTP session entry. */
struct tftp_session
{
    int in_use;
    unsigned char filename[TFTP_FILENAME_MAX];

    struct in_addr client_ip;
    u_int16_t client_port;

    int timestamp;
};

#ifdef VBOX_WITH_MULTI_DNS
struct dns_entry
{
        struct in_addr de_addr;
        LIST_ENTRY(dns_entry) de_list; 
};
LIST_HEAD(dns_list_head, dns_entry);
#endif

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
    int if_thresh;
    struct mbuf if_fastq;
    struct mbuf if_batchq;
    struct mbuf *next_m;
    /* Stuff from icmp.c */
    struct icmpstat_t icmpstat;
    /* Stuff from ip_input.c */
    struct ipstat_t ipstat;
    struct ipqhead ipq[IPREASS_NHASH];
    int maxnipq;    /* Administrative limit on # of reass queues*/
    int maxfragsperpacket; /* Maximum number of IPv4 fragments allowed per packet */
    int nipq; /* total number of reass queues */
    uint16_t ip_currid;
    /* Stuff from mbuf.c */
    int mbuf_alloced, mbuf_max;
    int msize;
    struct mbuf m_freelist, m_usedlist;
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
#ifndef VBOX_WITH_MULTI_DNS
    struct in_addr dns_addr;
#else
    struct dns_list_head dns_list_head;
#endif
    struct in_addr tftp_server;
    struct in_addr loopback_addr;
    uint32_t netmask;
    uint8_t client_ethaddr[6];
    struct ex_list *exec_list;
    char slirp_hostname[33];
    bool fPassDomain;
    const char *pszDomain;
    /* Stuff from tcp_input.c */
    struct socket tcb;
#ifdef VBOX_WITH_SLIRP_MT
    RTSEMMUTEX      tcb_mutex;
#endif
    struct socket *tcp_last_so;
    tcp_seq tcp_iss;
    /* Stuff from tcp_timer.c */
    struct tcpstat_t tcpstat;
    uint32_t tcp_now;
    int tcp_reass_qsize;
    int tcp_reass_maxqlen;
    int tcp_reass_maxseg;
    int tcp_reass_overflows;
    /* Stuff from tftp.c */
    struct tftp_session tftp_sessions[TFTP_SESSIONS_MAX];
    const char *tftp_prefix;
    /* Stuff from udp.c */
    struct udpstat_t udpstat;
    struct socket udb;
#ifdef VBOX_WITH_SLIRP_MT
    RTSEMMUTEX      udb_mutex;
#endif
    struct socket *udp_last_so;
    struct socket icmp_socket;
    struct icmp_storage icmp_msg_head;
# ifdef RT_OS_WINDOWS
    void *pvIcmpBuffer;
    size_t szIcmpBuffer;
    /* Accordin MSDN specification IcmpParseReplies
     * function should be detected in runtime
     */
    long (WINAPI * pfIcmpParseReplies)(void *, long);
    BOOL (WINAPI * pfIcmpCloseHandle)(HANDLE);
    HMODULE hmIcmpLibrary;
# endif
#if defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
# define VBOX_SOCKET_EVENT (pData->phEvents[VBOX_SOCKET_EVENT_INDEX])
    HANDLE phEvents[VBOX_EVENT_COUNT];
#endif
#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
    int fIcmp;
#endif
    STAMPROFILE StatFill;
    STAMPROFILE StatPoll;
    STAMPROFILE StatFastTimer;
    STAMPROFILE StatSlowTimer;
    STAMCOUNTER StatTCP;
    STAMCOUNTER StatUDP;
    STAMCOUNTER StatTCPHot;
    STAMCOUNTER StatUDPHot;
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

#define maxfragsperpacket pData->maxfragsperpacket
#define maxnipq pData->maxnipq
#define nipq pData->nipq

#define tcp_reass_qsize pData->tcp_reass_qsize
#define tcp_reass_maxqlen pData->tcp_reass_maxqlen
#define tcp_reass_maxseg pData->tcp_reass_maxseg
#define tcp_reass_overflows pData->tcp_reass_overflows

#if !defined(VBOX_WITH_SIMPLIFIED_SLIRP_SYNC) && defined(RT_OS_WINDOWS)
# define fIcmp pData->fIcmp
#endif

#define queue_tcp_label tcb
#define queue_udp_label udb
#define __X(x) x
#define _X(x) __X(x)

#ifdef VBOX_WITH_SLIRP_MT
#define QSOCKET_LOCK(queue)                                                     \
do {                                                                            \
    int rc = RTSemMutexRequest(_X(queue) ## _mutex, RT_INDEFINITE_WAIT);        \
    AssertReleaseRC(rc);                                                        \
} while (0)
#define QSOCKET_UNLOCK(queue)                           \
do {                                                    \
    int rc = RTSemMutexRelease(_X(queue) ## _mutex);    \
    AssertReleaseRC(rc);                                \
} while (0)
#define QSOCKET_LOCK_CREATE(queue)                      \
do {                                                    \
    int rc = RTSemMutexCreate(&pData->queue ## _mutex); \
    AssertReleaseRC(rc);                                \
} while (0)
#define QSOCKET_LOCK_DESTROY(queue)                     \
do {                                                    \
    int rc = RTSemMutexDestroy(pData->queue ## _mutex); \
    AssertReleaseRC(rc);                                \
} while (0)

# define QSOCKET_FOREACH(so, sonext, label)                         \
        QSOCKET_LOCK(__X(queue_## label ## _label));                \
        (so) = (_X(queue_ ## label ## _label)).so_next;             \
        SOCKET_LOCK((so));                                          \
        for(;;)                                                     \
        {                                                           \
            if ((so) == &(_X(queue_## label ## _label)))            \
            {                                                       \
                QSOCKET_UNLOCK(__X(queue_## label ##_label));       \
                break;                                              \
            }                                                       \
            if ((so)->so_next != &(_X(queue_## label ## _label)))   \
            {                                                       \
                SOCKET_LOCK((so)->so_next);                         \
            }                                                       \
            (sonext) = (so)->so_next;                               \
            QSOCKET_UNLOCK(__X(queue_## label ##_label)); 
#else
#define QSOCKET_LOCK(queue) do {} while (0)
#define QSOCKET_UNLOCK(queue) do {} while (0)
#define QSOCKET_LOCK_CREATE(queue) do {} while (0)
#define QSOCKET_LOCK_DESTROY(queue) do {} while (0)
# define QSOCKET_FOREACH(so, sonext, label)                                                                         \
   for ((so) = __X(queue_ ## label ## _label).so_next; so != &(__X(queue_ ## label ## _label)); (so) = (sonext))    \
   {                                                                                                                \
            (sonext) = (so)->so_next;
#endif

#endif /* !_slirp_state_h_ */
