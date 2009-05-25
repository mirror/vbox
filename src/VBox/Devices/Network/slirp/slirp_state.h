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
#include <iprt/req.h>
#include "ip_icmp.h"
#ifdef VBOX_WITH_SLIRP_DNS_PROXY
# include "dnsproxy/dnsproxy.h"
#endif

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
#ifdef VBOX_WITHOUT_SLIRP_CLIENT_ETHER
    struct in_addr addr;
#endif
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
struct dns_domain_entry
{
        char *dd_pszDomain;
        LIST_ENTRY(dns_domain_entry) dd_list;
};
LIST_HEAD(dns_domain_list_head, dns_domain_entry);

struct dns_entry
{
        struct in_addr de_addr;
        TAILQ_ENTRY(dns_entry) de_list;
};
TAILQ_HEAD(dns_list_head, dns_entry);
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
#ifdef VBOX_WITH_SLIRP_MT
    PRTREQQUEUE pReqQueue;
#endif
#ifndef VBOX_WITH_MULTI_DNS
    struct in_addr dns_addr;
#else
# ifdef RT_OS_WINDOWS
    ULONG (WINAPI * pfGetAdaptersAddresses)(ULONG, ULONG, PVOID, PIP_ADAPTER_ADDRESSES, PULONG);
# endif
    struct dns_list_head dns_list_head;
    struct dns_domain_list_head dns_domain_list_head;
#endif
    struct in_addr tftp_server;
    struct in_addr loopback_addr;
    uint32_t netmask;
#ifndef VBOX_WITHOUT_SLIRP_CLIENT_ETHER
    uint8_t client_ethaddr[6];
#else
    uint8_t *slirp_ethaddr;
#endif
    struct ex_list *exec_list;
    char slirp_hostname[33];
    bool fPassDomain;
#ifndef VBOX_WITH_MULTI_DNS
    const char *pszDomain;
#endif
    /* Stuff from tcp_input.c */
    struct socket tcb;
#ifdef VBOX_WITH_SLIRP_MT
    RTCRITSECT      tcb_mutex;
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
    RTCRITSECT      udb_mutex;
#endif
    struct socket *udp_last_so;
    struct socket icmp_socket;
    struct icmp_storage icmp_msg_head;
# ifndef RT_OS_WINDOWS
    /* counter of sockets needed for allocation enough room to
     * process sockets with poll/epoll
     *
     * NSOCK_INC/DEC should be injected before every
     * operation on socket queue (tcb, udb)
     */
    int nsock;
#  define NSOCK_INC() do {pData->nsock++;} while (0)
#  define NSOCK_DEC() do {pData->nsock--;} while (0)
# else
#  define NSOCK_INC() do {} while (0)
#  define NSOCK_DEC() do {} while (0)
# endif
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
#if defined(RT_OS_WINDOWS)
# define VBOX_SOCKET_EVENT (pData->phEvents[VBOX_SOCKET_EVENT_INDEX])
    HANDLE phEvents[VBOX_EVENT_COUNT];
#endif
#ifdef VBOX_WITH_SLIRP_DNS_PROXY
    /* from dnsproxy/dnsproxy.h*/
    unsigned int authoritative_port;
    unsigned int authoritative_timeout;
    unsigned int recursive_port;
    unsigned int recursive_timeout;
    unsigned int stats_timeout;
    unsigned int port;

    unsigned long active_queries;
    unsigned long all_queries;
    unsigned long authoritative_queries;
    unsigned long recursive_queries;
    unsigned long removed_queries;
    unsigned long dropped_queries;
    unsigned long answered_queries;
    unsigned long dropped_answers;
    unsigned long late_answers;
    unsigned long hash_collisions;
    /*dnsproxy/dnsproxy.c*/
    unsigned short queryid;
    struct sockaddr_in authoritative_addr;
    struct sockaddr_in recursive_addr;
    int sock_query;
    int sock_answer;
    /* dnsproxy/hash.c */
    #define HASHSIZE 10
    #define HASH(id) (id & ((1 << HASHSIZE) - 1))
    struct request *request_hash[1 << HASHSIZE];
    /* this field control behaviour of DHCP server */
    bool use_dns_proxy;
#endif
#ifdef VBOX_WITH_SLIRP_ALIAS
    LIST_HEAD(, libalias) instancehead;
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
#ifndef VBOX_SLIRP_ALIAS
# define alias_addr pData->alias_addr
#endif
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

#define queue_tcp_label tcb
#define queue_udp_label udb
#define __X(x) x
#define _X(x) __X(x)
#define _str(x) #x
#define str(x) _str(x)

#ifdef VBOX_WITH_SLIRP_MT

# define QSOCKET_LOCK(queue)                                          \
    do {                                                              \
        int rc;                                                       \
        /* Assert(strcmp(RTThreadSelfName(), "EMT") != 0); */         \
        rc = RTCritSectEnter(&_X(queue) ## _mutex);                   \
        AssertReleaseRC(rc);                                          \
    } while (0)
# define QSOCKET_UNLOCK(queue)                                        \
    do {                                                              \
        int rc;                                                       \
        rc = RTCritSectLeave(&_X(queue) ## _mutex);                   \
        AssertReleaseRC(rc);                                          \
    } while (0)
# define QSOCKET_LOCK_CREATE(queue)                                   \
    do {                                                              \
        int rc;                                                       \
        rc = RTCritSectInit(&pData->queue ## _mutex);                 \
        AssertReleaseRC(rc);                                          \
    } while (0)
# define QSOCKET_LOCK_DESTROY(queue)                                  \
    do {                                                              \
        int rc = RTCritSectDelete(&pData->queue ## _mutex);           \
        AssertReleaseRC(rc);                                          \
    } while (0)

# define QSOCKET_FOREACH(so, sonext, label)                           \
    QSOCKET_LOCK(__X(queue_## label ## _label));                      \
    (so) = (_X(queue_ ## label ## _label)).so_next;                   \
    QSOCKET_UNLOCK(__X(queue_## label ##_label));                     \
    if ((so) != &(_X(queue_## label ## _label))) SOCKET_LOCK((so));   \
    for(;;)                                                           \
    {                                                                 \
        if ((so) == &(_X(queue_## label ## _label)))                  \
        {                                                             \
            break;                                                    \
        }                                                             \
        Log2(("%s:%d Processing so:%R[natsock]\n", __FUNCTION__, __LINE__, (so)));

# define CONTINUE_NO_UNLOCK(label) goto loop_end_ ## label ## _mt_nounlock
# define CONTINUE(label) goto loop_end_ ## label ## _mt
/* @todo replace queue parameter with macrodinition */
/* _mt_nounlock - user should lock so_next before calling CONTINUE_NO_UNLOCK */
# define LOOP_LABEL(label, so, sonext) loop_end_ ## label ## _mt:       \
    (sonext) = (so)->so_next;                                           \
    SOCKET_UNLOCK(so);                                                  \
    QSOCKET_LOCK(_X(queue_ ## label ## _label));                        \
    if ((sonext) != &(_X(queue_## label ## _label)))                    \
    {                                                                   \
        SOCKET_LOCK((sonext));                                          \
        QSOCKET_UNLOCK(_X(queue_ ## label ## _label));                  \
    }                                                                   \
    else                                                                \
    {                                                                   \
        so = &_X(queue_ ## label ## _label);                            \
        QSOCKET_UNLOCK(_X(queue_ ## label ## _label));                  \
        break;                                                          \
    }                                                                   \
    (so) = (sonext);                                                    \
    continue;                                                           \
    loop_end_ ## label ## _mt_nounlock:                                 \
    (so) = (sonext)

# define DO_TCP_OUTPUT(data, sotcb)                                     \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)tcp_output;                  \
        pReq->u.Internal.cArgs    = 2;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(sotcb);                 \
        pReq->fFlags              = RTREQFLAGS_VOID;                    \
        rc = RTReqQueue(pReq, 0);                                       \
        if (RT_LIKELY(rc) == VERR_TIMEOUT)                              \
        {                                                               \
            SOCKET_UNLOCK(so);                                          \
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);                   \
            AssertReleaseRC(rc);                                        \
            SOCKET_LOCK(so);                                            \
            RTReqFree(pReq);                                            \
        }                                                               \
        else                                                            \
            AssertReleaseRC(rc);                                        \
} while(0)

# define DO_TCP_INPUT(data, mbuf, size, so)                             \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)tcp_input;                   \
        pReq->u.Internal.cArgs    = 4;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(mbuf);                  \
        pReq->u.Internal.aArgs[2] = (uintptr_t)(size);                  \
        pReq->u.Internal.aArgs[3] = (uintptr_t)(so);                    \
        pReq->fFlags              = RTREQFLAGS_VOID|RTREQFLAGS_NO_WAIT; \
        rc = RTReqQueue(pReq, 0);                                       \
        AssertReleaseRC(rc);                                            \
    } while(0)

# define DO_TCP_CONNECT(data, so)                                       \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)tcp_connect;                 \
        pReq->u.Internal.cArgs    = 2;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(so);                    \
        pReq->fFlags              = RTREQFLAGS_VOID;                    \
        rc = RTReqQueue(pReq, 0); /* don't wait, we have to release lock before*/ \
        if (RT_LIKELY(rc) == VERR_TIMEOUT)                              \
        {                                                               \
            SOCKET_UNLOCK(so);                                          \
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);                   \
            AssertReleaseRC(rc);                                        \
            SOCKET_LOCK(so);                                            \
            RTReqFree(pReq);                                            \
        }                                                               \
        else                                                            \
            AssertReleaseRC(rc);                                        \
    } while(0)

# define DO_SOREAD(ret, data, so, ifclose)                              \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)soread_queue;                \
        pReq->u.Internal.cArgs    = 4;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(so);                    \
        pReq->u.Internal.aArgs[2] = (uintptr_t)(ifclose);               \
        pReq->u.Internal.aArgs[3] = (uintptr_t)&(ret);                  \
        pReq->fFlags              = RTREQFLAGS_VOID;                    \
        rc = RTReqQueue(pReq, 0); /* don't wait, we have to release lock before*/ \
        if (RT_LIKELY(rc) == VERR_TIMEOUT)                              \
        {                                                               \
            SOCKET_UNLOCK(so);                                          \
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);                   \
            AssertReleaseRC(rc);                                        \
            SOCKET_LOCK(so);                                            \
            RTReqFree(pReq);                                            \
        }                                                               \
        else                                                            \
            AssertReleaseRC(rc);                                        \
    } while(0)

# define DO_SOWRITE(ret, data, so)                                      \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)sowrite;                     \
        pReq->u.Internal.cArgs    = 2;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(so);                    \
        pReq->fFlags              = RTREQFLAGS_RETURN_MASK;             \
        rc = RTReqQueue(pReq, 0); /* don't wait, we have to release lock before*/ \
        if (RT_LIKELY(rc) == VERR_TIMEOUT)                              \
        {                                                               \
            SOCKET_UNLOCK(so);                                          \
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);                   \
            SOCKET_LOCK(so);                                            \
            ret = pReq->iStatus;                                        \
            RTReqFree(pReq);                                            \
        }                                                               \
        else                                                            \
            AssertReleaseRC(rc);                                        \
    } while(0)

# define DO_SORECFROM(data, so)                                         \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)sorecvfrom;                  \
        pReq->u.Internal.cArgs    = 2;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(so);                    \
        pReq->fFlags              = RTREQFLAGS_VOID;                    \
        rc = RTReqQueue(pReq, 0);                                       \
        if (RT_LIKELY(rc) == VERR_TIMEOUT)                              \
        {                                                               \
            SOCKET_UNLOCK(so);                                          \
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);                   \
            AssertReleaseRC(rc);                                        \
            SOCKET_LOCK(so);                                            \
            RTReqFree(pReq);                                            \
        }                                                               \
        else                                                            \
            AssertReleaseRC(rc);                                        \
    } while(0)

# define DO_UDP_DETACH(data, so, so_next)                               \
    do {                                                                \
        PRTREQ pReq = NULL;                                             \
        int rc;                                                         \
        rc = RTReqAlloc((data)->pReqQueue, &pReq, RTREQTYPE_INTERNAL);  \
        AssertReleaseRC(rc);                                            \
        pReq->u.Internal.pfn      = (PFNRT)udp_detach;                  \
        pReq->u.Internal.cArgs    = 2;                                  \
        pReq->u.Internal.aArgs[0] = (uintptr_t)(data);                  \
        pReq->u.Internal.aArgs[1] = (uintptr_t)(so);                    \
        pReq->fFlags              = RTREQFLAGS_VOID;                    \
        rc = RTReqQueue(pReq, 0); /* don't wait, we have to release lock before*/ \
        if (RT_LIKELY(rc) == VERR_TIMEOUT)                              \
        {                                                               \
            SOCKET_UNLOCK(so);                                          \
            rc = RTReqWait(pReq, RT_INDEFINITE_WAIT);                   \
            AssertReleaseRC(rc);                                        \
            if ((so_next) != &udb) SOCKET_LOCK((so_next));              \
            RTReqFree(pReq);                                            \
        }                                                               \
        else                                                            \
            AssertReleaseRC(rc);                                        \
    } while(0)

# define SOLOOKUP(so, label, src, sport, dst, dport)                    \
    do {                                                                \
        struct socket *sonxt;                                           \
        (so) = NULL;                                                    \
        QSOCKET_FOREACH(so, sonxt, label)                               \
        /* { */                                                         \
            if (   so->so_lport        == (sport)                       \
                && so->so_laddr.s_addr == (src).s_addr                  \
                && so->so_faddr.s_addr == (dst).s_addr                  \
                && so->so_fport        == (dport))                      \
                {                                                       \
                    if (sonxt != &__X(queue_ ## label ## _label))       \
                        SOCKET_UNLOCK(sonxt);                           \
                    break; /*so is locked*/                             \
                }                                                       \
            LOOP_LABEL(so, sonxt, label);                               \
            }                                                           \
        }                                                               \
    } while (0)

#else /* !VBOX_WITH_SLIRP_MT */

# define QSOCKET_LOCK(queue) do {} while (0)
# define QSOCKET_UNLOCK(queue) do {} while (0)
# define QSOCKET_LOCK_CREATE(queue) do {} while (0)
# define QSOCKET_LOCK_DESTROY(queue) do {} while (0)
# define QSOCKET_FOREACH(so, sonext, label)                              \
    for ((so)  = __X(queue_ ## label ## _label).so_next;                 \
         (so) != &(__X(queue_ ## label ## _label));                      \
         (so) = (sonext))                                                \
    {                                                                    \
        (sonext) = (so)->so_next;
# define CONTINUE(label) continue
# define CONTINUE_NO_UNLOCK(label) continue
# define LOOP_LABEL(label, so, sonext) /* empty*/
# define DO_TCP_OUTPUT(data, sotcb) tcp_output((data), (sotcb))
# define DO_TCP_INPUT(data, mbuf, size, so) tcp_input((data), (mbuf), (size), (so))
# define DO_TCP_CONNECT(data, so) tcp_connect((data), (so))
# define DO_SOREAD(ret, data, so, ifclose)                               \
    do {                                                                 \
        (ret) = soread((data), (so), (ifclose));                         \
    } while(0)
# define DO_SOWRITE(ret, data, so)                                       \
    do {                                                                 \
        (ret) = sowrite((data), (so));                                   \
    } while(0)
# define DO_SORECFROM(data, so) sorecvfrom((data), (so))
# define SOLOOKUP(so, label, src, sport, dst, dport)                                      \
    do {                                                                                  \
        (so) = solookup(&__X(queue_ ## label ## _label), (src), (sport), (dst), (dport)); \
    } while (0)
# define DO_UDP_DETACH(data, so, ignored) udp_detach((data), (so))

#endif /* !VBOX_WITH_SLIRP_MT */

#define TCP_OUTPUT(data, sotcb) DO_TCP_OUTPUT((data), (sotcb))
#define TCP_INPUT(data, mbuf, size, so) DO_TCP_INPUT((data), (mbuf), (size), (so))
#define TCP_CONNECT(data, so) DO_TCP_CONNECT((data), (so))
#define SOREAD(ret, data, so, ifclose) DO_SOREAD((ret), (data), (so), (ifclose))
#define SOWRITE(ret, data, so) DO_SOWRITE((ret), (data), (so))
#define SORECVFROM(data, so) DO_SORECFROM((data), (so))
#define UDP_DETACH(data, so, so_next) DO_UDP_DETACH((data), (so), (so_next))

#ifdef VBOX_WITH_SLIRP_DNS_PROXY
/* dnsproxy/dnsproxy.c */
# define authoritative_port pData->authoritative_port
# define authoritative_timeout pData->authoritative_timeout
# define recursive_port pData->recursive_port
# define recursive_timeout pData->recursive_timeout
# define stats_timeout pData->stats_timeout
/* dnsproxy/hash.c */
# define dns_port pData->port
# define request_hash pData->request_hash
# define hash_collisions pData->hash_collisions
# define active_queries pData->active_queries
# define all_queries pData->all_queries
# define authoritative_queries pData->authoritative_queries
# define recursive_queries pData->recursive_queries
# define removed_queries pData->removed_queries
# define dropped_queries pData->dropped_queries
# define answered_queries pData->answered_queries
# define dropped_answers pData->dropped_answers
# define late_answers pData->late_answers

/* dnsproxy/dnsproxy.c */
# define queryid pData->queryid
# define authoritative_addr pData->authoritative_addr
# define recursive_addr pData->recursive_addr
# define sock_query pData->sock_query
# define sock_answer pData->sock_answer
#endif

#ifdef VBOX_WITH_SLIRP_ALIAS
# define instancehead pData->instancehead
#endif

#endif /* !_slirp_state_h_ */
