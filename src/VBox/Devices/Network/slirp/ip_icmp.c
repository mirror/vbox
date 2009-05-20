/*
 * Copyright (c) 1982, 1986, 1988, 1993
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
 *      @(#)ip_icmp.c   8.2 (Berkeley) 1/4/94
 * ip_icmp.c,v 1.7 1995/05/30 08:09:42 rgrimes Exp
 */

#include "slirp.h"
#include "ip_icmp.h"
#ifdef RT_OS_WINDOWS
#include <Icmpapi.h>
#include <Iphlpapi.h>
#endif

#ifdef RT_OS_WINDOWS
# define ICMP_SEND_ECHO(event, routine, addr, data, datasize, ipopt)                                        \
                IcmpSendEcho2(pData->icmp_socket.sh, (event), NULL, NULL, (addr), (data), (datasize),       \
                               (ipopt), pData->pvIcmpBuffer, pData->szIcmpBuffer, 1)
#endif /* RT_OS_WINDOWS */

/* The message sent when emulating PING */
/* Be nice and tell them it's just a psuedo-ping packet */
static const char icmp_ping_msg[] = "This is a psuedo-PING packet used by Slirp to emulate ICMP ECHO-REQUEST packets.\n";

/* list of actions for icmp_error() on RX of an icmp message */
static const int icmp_flush[19] =
{
/* ECHO REPLY (0) */         0,
                             1,
                             1,
/* DEST UNREACH (3) */       1,
/* SOURCE QUENCH (4)*/       1,
/* REDIRECT (5) */           1,
                             1,
                             1,
/* ECHO (8) */               0,
/* ROUTERADVERT (9) */       1,
/* ROUTERSOLICIT (10) */     1,
/* TIME EXCEEDED (11) */     1,
/* PARAMETER PROBLEM (12) */ 1,
/* TIMESTAMP (13) */         0,
/* TIMESTAMP REPLY (14) */   0,
/* INFO (15) */              0,
/* INFO REPLY (16) */        0,
/* ADDR MASK (17) */         0,
/* ADDR MASK REPLY (18) */   0
};

int
icmp_init(PNATState pData)
{
    pData->icmp_socket.so_type = IPPROTO_ICMP;
    pData->icmp_socket.so_state = SS_ISFCONNECTED;
#ifndef RT_OS_WINDOWS
# ifndef RT_OS_DARWIN
    pData->icmp_socket.s = socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
# else /* !RT_OS_DARWIN */
    pData->icmp_socket.s = socket(AF_INET, SOCK_DGRAM, IPPROTO_ICMP);
# endif /* RT_OS_DARWIN */
    if (pData->icmp_socket.s == -1)
    {
        int rc = RTErrConvertFromErrno(errno);
        LogRel(("NAT: ICMP/ping not available (could open ICMP socket, error %Rrc)\n", rc));
        return 1;
    }
    fd_nonblock(pData->icmp_socket.s);
    NSOCK_INC();
#else /* RT_OS_WINDOWS */
    pData->hmIcmpLibrary = LoadLibrary("Iphlpapi.dll");
    if (pData->hmIcmpLibrary != NULL)
    {
        pData->pfIcmpParseReplies = (long (WINAPI *)(void *, long))
                                    GetProcAddress(pData->hmIcmpLibrary, "IcmpParseReplies");
        pData->pfIcmpCloseHandle = (BOOL (WINAPI *)(HANDLE))
                                    GetProcAddress(pData->hmIcmpLibrary, "IcmpCloseHandle");
# ifdef VBOX_WITH_MULTI_DNS 
        pData->pfGetAdaptersAddresses = (ULONG (WINAPI *)(HANDLE))
                                    GetProcAddress(pData->hmIcmpLibrary, "GetAdaptersAddresses");
        if (pData->pfGetAdaptersAddresses == NULL) 
        {
            LogRel(("NAT: Can't find GetAdapterAddresses in Iphlpapi.dll"));
        }
# endif
    }

    if (pData->pfIcmpParseReplies == NULL)
    {
# ifdef VBOX_WITH_MULTI_DNS 
        if(pData->pfGetAdaptersAddresses == NULL) 
            FreeLibrary(pData->hmIcmpLibrary);
# else
        FreeLibrary(pData->hmIcmpLibrary);
# endif
        pData->hmIcmpLibrary = LoadLibrary("Icmp.dll");
        if (pData->hmIcmpLibrary == NULL)
        {
            LogRel(("NAT: Icmp.dll could not be loaded\n"));
            return 1;
        }
        pData->pfIcmpParseReplies = (long (WINAPI *)(void *, long))
                                    GetProcAddress(pData->hmIcmpLibrary, "IcmpParseReplies");
        pData->pfIcmpCloseHandle = (BOOL (WINAPI *)(HANDLE))
                                    GetProcAddress(pData->hmIcmpLibrary, "IcmpCloseHandle");
    }
    if (pData->pfIcmpParseReplies == NULL)
    {
        LogRel(("NAT: Can't find IcmpParseReplies symbol\n"));
        FreeLibrary(pData->hmIcmpLibrary);
        return 1;
    }
    if (pData->pfIcmpCloseHandle == NULL)
    {
        LogRel(("NAT: Can't find IcmpCloseHandle symbol\n"));
        FreeLibrary(pData->hmIcmpLibrary);
        return 1;
    }
    pData->icmp_socket.sh = IcmpCreateFile();
    pData->phEvents[VBOX_ICMP_EVENT_INDEX] = CreateEvent(NULL, FALSE, FALSE, NULL);
    pData->szIcmpBuffer = sizeof(ICMP_ECHO_REPLY) * 10;
    pData->pvIcmpBuffer = RTMemAlloc(pData->szIcmpBuffer);
#endif /* RT_OS_WINDOWS */
    LIST_INIT(&pData->icmp_msg_head);
    return 0;
}

/*
 * ip here is ip header + 64bytes readed from ICMP packet
 */
struct icmp_msg *
icmp_find_original_mbuf(PNATState pData, struct ip *ip)
{
    struct mbuf *m0;
    struct ip *ip0;
    struct icmp *icp, *icp0;
    struct icmp_msg *icm = NULL;
    int found = 0;
    struct udphdr *udp;
    struct tcphdr *tcp;
    struct socket *head_socket = NULL;
    struct socket *last_socket = NULL;
    struct socket *so = NULL;
    struct in_addr laddr, faddr;
    u_short lport, fport;

    laddr.s_addr = ~0;
    faddr.s_addr = ~0;

    lport = ~0;
    fport = ~0;


    Log(("%s: processing (proto:%d)\n", __FUNCTION__, ip->ip_p));
    switch (ip->ip_p)
    {
        case IPPROTO_ICMP:
            icp = (struct icmp *)((char *)ip + (ip->ip_hl << 2));
            LIST_FOREACH(icm, &pData->icmp_msg_head, im_list)
            {
                m0 = icm->im_m;
                ip0 = mtod(m0, struct ip *);
                AssertRelease(ip0->ip_p == IPPROTO_ICMP);
                icp0 = (struct icmp *)((char *)ip0 + (ip0->ip_hl << 2));
                if (  (   (icp->icmp_type != ICMP_ECHO && ip->ip_src.s_addr == ip0->ip_dst.s_addr)
                       || (icp->icmp_type == ICMP_ECHO && ip->ip_dst.s_addr == ip0->ip_dst.s_addr))
                    && icp->icmp_id == icp0->icmp_id
                    && icp->icmp_seq == icp0->icmp_seq)
                {
                    found = 1;
                    Log(("Have found %R[natsock]\n", icm->im_so));
                    break;
                }
                Log(("Have found nothing\n"));
            }
            break;

        /*
         *  for TCP and UDP logic little bit reverted, we try to find the HOST socket
         *  from which the IP package has been sent.
         */
        case IPPROTO_UDP:
            head_socket = &udb;
            udp = (struct udphdr *)((char *)ip + (ip->ip_hl << 2));
            faddr.s_addr = ip->ip_dst.s_addr;
            fport = udp->uh_dport;
            laddr.s_addr = ip->ip_src.s_addr;
            lport = udp->uh_sport;
            last_socket = udp_last_so;
            /* fall through */

        case IPPROTO_TCP:
            if (head_socket == NULL)
            {
                tcp = (struct tcphdr *)((char *)ip + (ip->ip_hl << 2));
                head_socket = &tcb; /* head_socket could be initialized with udb*/
                faddr.s_addr = ip->ip_dst.s_addr;
                fport = tcp->th_dport;
                laddr.s_addr = ip->ip_src.s_addr;
                lport = tcp->th_sport;
                last_socket = tcp_last_so;
            }
            /* check last socket first */
            if (   last_socket->so_faddr.s_addr == faddr.s_addr
                && last_socket->so_fport == fport
                && last_socket->so_hlport == lport)
            {
                found = 1;
                so = last_socket;
                goto sofound;
            }
            for (so = head_socket->so_prev; so != head_socket; so = so->so_prev)
            {
                /* Should be reaplaced by hash here */
                Log(("trying:%R[natsock] against %R[IP4]:%d lport=%d hlport=%d\n", so, &faddr, fport, lport, so->so_hlport));
                if (   so->so_faddr.s_addr == faddr.s_addr
                    && so->so_fport == fport
                    && so->so_hlport == lport)
                {
                    found = 1;
                    break;
                }
            }
            break;

        default:
            LogRel(("%s:ICMP: unsupported protocol(%d)\n", __FUNCTION__, ip->ip_p));
    }
    sofound:
    if (found == 1 && icm == NULL)
    {
        if (so->so_state == SS_NOFDREF) 
        {
            /* socket is shutdowning we've already sent ICMP on it.*/
            LogRel(("NAT: Received icmp on shutdowning socket (probably corresponding ICMP socket has been already sent)\n"));
            return NULL;
        }
        icm = RTMemAlloc(sizeof(struct icmp_msg));
        icm->im_m = so->so_m;
        icm->im_so = so;
        found = 1;
        Log(("hit:%R[natsock]\n", so));
        /*XXX: this storage not very long,
         * better add flag if it should removed from lis
         */
        LIST_INSERT_HEAD(&pData->icmp_msg_head, icm, im_list);
        return (icm);
    }
    if (found == 1)
        return icm;

    return NULL;
}

static int
icmp_attach(PNATState pData, struct mbuf *m)
{
    struct icmp_msg *icm;
    struct ip *ip;
    ip = mtod(m, struct ip *);
    Assert(ip->ip_p == IPPROTO_ICMP);
    icm = RTMemAlloc(sizeof(struct icmp_msg));
    icm->im_m = m;
    icm->im_so = m->m_so;
    LIST_INSERT_HEAD(&pData->icmp_msg_head, icm, im_list);
    return 0;
}

/*
 * Process a received ICMP message.
 */
void
icmp_input(PNATState pData, struct mbuf *m, int hlen)
{
    register struct icmp *icp;
    register struct ip *ip = mtod(m, struct ip *);
    int icmplen = ip->ip_len;
    int status;
    uint32_t dst;
#if !defined(RT_OS_WINDOWS)
    int ttl;
#endif

    /* int code; */

    DEBUG_CALL("icmp_input");
    DEBUG_ARG("m = %lx", (long )m);
    DEBUG_ARG("m_len = %d", m->m_len);

    icmpstat.icps_received++;

    /*
     * Locate icmp structure in mbuf, and check
     * that its not corrupted and of at least minimum length.
     */
    if (icmplen < ICMP_MINLEN)
    {
        /* min 8 bytes payload */
        icmpstat.icps_tooshort++;
freeit:
        m_freem(pData, m);
        goto end_error;
    }

    m->m_len -= hlen;
    m->m_data += hlen;
    icp = mtod(m, struct icmp *);
    if (cksum(m, icmplen))
    {
        icmpstat.icps_checksum++;
        goto freeit;
    }
    m->m_len += hlen;
    m->m_data -= hlen;

    /* icmpstat.icps_inhist[icp->icmp_type]++; */
    /* code = icp->icmp_code; */

    DEBUG_ARG("icmp_type = %d", icp->icmp_type);
    switch (icp->icmp_type)
    {
        case ICMP_ECHO:
            ip->ip_len += hlen;              /* since ip_input subtracts this */
            dst = ip->ip_dst.s_addr;
            if (dst == alias_addr.s_addr)
            {
                icp->icmp_type = ICMP_ECHOREPLY;
                ip->ip_dst.s_addr = ip->ip_src.s_addr;
                ip->ip_src.s_addr = dst;
                icmp_reflect(pData, m);
            }
            else
            {
                struct sockaddr_in addr;
#ifdef RT_OS_WINDOWS
                IP_OPTION_INFORMATION ipopt;
                int error;
#endif
                addr.sin_family = AF_INET;
                if ((ip->ip_dst.s_addr & htonl(pData->netmask)) == special_addr.s_addr)
                {
                    /* It's an alias */
                    switch (ntohl(ip->ip_dst.s_addr) & ~pData->netmask)
                    {
                        case CTL_DNS:
#ifndef VBOX_WITH_MULTI_DNS
                            addr.sin_addr = dns_addr;
                            break;
#endif
                        case CTL_ALIAS:
                        default:
                            addr.sin_addr = loopback_addr;
                            break;
                    }
                }
                else
                    addr.sin_addr.s_addr = ip->ip_dst.s_addr;
#ifndef RT_OS_WINDOWS
                if (pData->icmp_socket.s != -1)
                {
                    m->m_so = &pData->icmp_socket;
                    icmp_attach(pData, m);
                    ttl = ip->ip_ttl;
                    Log(("NAT/ICMP: try to set TTL(%d)\n", ttl));
                    status = setsockopt(pData->icmp_socket.s, IPPROTO_IP, IP_TTL,
                                        (void *)&ttl, sizeof(ttl));
                    if (status < 0)
                        LogRel(("NAT: Error (%s) occurred while setting TTL attribute of IP packet\n",
                                strerror(errno)));
                    if (sendto(pData->icmp_socket.s, icp, icmplen, 0,
                              (struct sockaddr *)&addr, sizeof(addr)) == -1)
                    {
                        Log((dfd,"icmp_input udp sendto tx errno = %d-%s\n",
                                    errno, strerror(errno)));
                        icmp_error(pData, m, ICMP_UNREACH, ICMP_UNREACH_NET, 0, strerror(errno));
                    }
                }
                else
                {
                    /*
                     * We're freeing the ICMP message, which unable sent or process.
                     * That behavior described in rfc 793, we shouldn't notify sender about
                     * fail of processing it's ICMP packets
                     */
                    m_free(pData, m);
                    return;
                }
#else /* RT_OS_WINDOWS */
                icmp_attach(pData, m);
                pData->icmp_socket.so_laddr.s_addr = ip->ip_src.s_addr; /* XXX: hack*/
                pData->icmp_socket.so_icmp_id = icp->icmp_id;
                pData->icmp_socket.so_icmp_seq = icp->icmp_seq;
                m->m_so = &pData->icmp_socket;
                memset(&ipopt, 0, sizeof(IP_OPTION_INFORMATION));
                ipopt.Ttl = ip->ip_ttl;
                status = ICMP_SEND_ECHO(pData->phEvents[VBOX_ICMP_EVENT_INDEX], notify_slirp, addr.sin_addr.s_addr,
                                icp->icmp_data, icmplen - ICMP_MINLEN, &ipopt);
                if (status == 0 && (error = GetLastError()) != ERROR_IO_PENDING)
                {
                    error = GetLastError();
                    LogRel(("NAT: Error (%d) occurred while sending ICMP (", error));
                    switch (error)
                    {
                        case ERROR_INVALID_PARAMETER:
                            LogRel(("icmp_socket:%lx is invalid)\n", pData->icmp_socket.s));
                            break;
                        case ERROR_NOT_SUPPORTED:
                            LogRel(("operation is unsupported)\n"));
                            break;
                        case ERROR_NOT_ENOUGH_MEMORY:
                            LogRel(("OOM!!!)\n"));
                            break;
                        case IP_BUF_TOO_SMALL:
                            LogRel(("Buffer too small)\n"));
                            break;
                        default:
                            LogRel(("Other error!!!)\n"));
                            break;
                    }
                }
#endif /* RT_OS_WINDOWS */
            } /* if ip->ip_dst.s_addr == alias_addr.s_addr */
            break;
        case ICMP_UNREACH:
            /* XXX? report error? close socket? */
        case ICMP_TIMXCEED:
        case ICMP_PARAMPROB:
        case ICMP_SOURCEQUENCH:
        case ICMP_TSTAMP:
        case ICMP_MASKREQ:
        case ICMP_REDIRECT:
            icmpstat.icps_notsupp++;
            m_freem(pData, m);
            break;

        default:
            icmpstat.icps_badtype++;
            m_freem(pData, m);
    } /* switch */

end_error:
    /* m is m_free()'d xor put in a socket xor or given to ip_send */
    ;
}


/*
 * Send an ICMP message in response to a situation
 *
 * RFC 1122: 3.2.2 MUST send at least the IP header and 8 bytes of header. MAY send more (we do).
 *                 MUST NOT change this header information.
 *                 MUST NOT reply to a multicast/broadcast IP address.
 *                 MUST NOT reply to a multicast/broadcast MAC address.
 *                 MUST reply to only the first fragment.
 */
/*
 * Send ICMP_UNREACH back to the source regarding msrc.
 * mbuf *msrc is used as a template, but is NOT m_free()'d.
 * It is reported as the bad ip packet.  The header should
 * be fully correct and in host byte order.
 * ICMP fragmentation is illegal.  All machines must accept 576 bytes in one
 * packet.  The maximum payload is 576-20(ip hdr)-8(icmp hdr)=548
 */

#define ICMP_MAXDATALEN (IP_MSS-28)
void icmp_error(PNATState pData, struct mbuf *msrc, u_char type, u_char code, int minsize, const char *message)
{
    unsigned hlen, shlen, s_ip_len;
    register struct ip *ip;
    register struct icmp *icp;
    register struct mbuf *m;

    DEBUG_CALL("icmp_error");
    DEBUG_ARG("msrc = %lx", (long )msrc);
    DEBUG_ARG("msrc_len = %d", msrc->m_len);

    if (type!=ICMP_UNREACH && type!=ICMP_TIMXCEED)
        goto end_error;

    /* check msrc */
    if (!msrc)
        goto end_error;

    ip = mtod(msrc, struct ip *);
#if DEBUG
    {
        char bufa[20], bufb[20];
        strcpy(bufa, inet_ntoa(ip->ip_src));
        strcpy(bufb, inet_ntoa(ip->ip_dst));
        DEBUG_MISC((dfd, " %.16s to %.16s\n", bufa, bufb));
    }
#endif
    if (ip->ip_off & IP_OFFMASK)
        goto end_error;    /* Only reply to fragment 0 */

    shlen = ip->ip_hl << 2;
    s_ip_len = ip->ip_len;
    if (ip->ip_p == IPPROTO_ICMP)
    {
        icp = (struct icmp *)((char *)ip + shlen);
        /*
         *  Assume any unknown ICMP type is an error. This isn't
         *  specified by the RFC, but think about it..
         */
        if (icp->icmp_type>18 || icmp_flush[icp->icmp_type])
            goto end_error;
    }

    /* make a copy */
    if (!(m = m_get(pData)))
        goto end_error;                    /* get mbuf */
    {
        int new_m_size;
        m_adj(m, if_maxlinkhdr);
        new_m_size = sizeof(struct ip) + ICMP_MINLEN + msrc->m_len + ICMP_MAXDATALEN;
        if (new_m_size>m->m_size)
            m_inc(m, new_m_size);
    }

    memcpy(m->m_data, msrc->m_data, msrc->m_len);
    m->m_len = msrc->m_len;                /* copy msrc to m */

    /* make the header of the reply packet */
    ip   = mtod(m, struct ip *);
    hlen = sizeof(struct ip );             /* no options in reply */

    /* fill in icmp */
    m->m_data += hlen;
    m->m_len  -= hlen;

    icp = mtod(m, struct icmp *);

    if (minsize)
        s_ip_len = shlen+ICMP_MINLEN;      /* return header+8b only */
    else if (s_ip_len > ICMP_MAXDATALEN)   /* maximum size */
        s_ip_len = ICMP_MAXDATALEN;

    m->m_len = ICMP_MINLEN + s_ip_len;     /* 8 bytes ICMP header */

    /* min. size = 8+sizeof(struct ip)+8 */

    icp->icmp_type = type;
    icp->icmp_code = code;
    icp->icmp_id = 0;
    icp->icmp_seq = 0;

    memcpy(&icp->icmp_ip, msrc->m_data, s_ip_len);   /* report the ip packet */
    HTONS(icp->icmp_ip.ip_len);
    HTONS(icp->icmp_ip.ip_id);
    HTONS(icp->icmp_ip.ip_off);

#if DEBUG
    if (message)
    {
        /* DEBUG : append message to ICMP packet */
        int message_len;
        char *cpnt;
        message_len = strlen(message);
        if (message_len > ICMP_MAXDATALEN)
            message_len = ICMP_MAXDATALEN;
        cpnt = (char *)m->m_data+m->m_len;
        memcpy(cpnt, message, message_len);
        m->m_len += message_len;
    }
#endif

    icp->icmp_cksum = 0;
    icp->icmp_cksum = cksum(m, m->m_len);

    m->m_data -= hlen;
    m->m_len += hlen;

    /* fill in ip */
    ip->ip_hl = hlen >> 2;
    ip->ip_len = m->m_len;

    ip->ip_tos = ((ip->ip_tos & 0x1E) | 0xC0);  /* high priority for errors */

    ip->ip_ttl = MAXTTL;
    ip->ip_p = IPPROTO_ICMP;
    ip->ip_dst = ip->ip_src;    /* ip adresses */
    ip->ip_src = alias_addr;

    (void ) ip_output(pData, (struct socket *)NULL, m);

    icmpstat.icps_reflect++;

    return;

end_error:
    LogRel(("NAT: error occured while sending ICMP error message \n"));
}
#undef ICMP_MAXDATALEN

/*
 * Reflect the ip packet back to the source
 */
void
icmp_reflect(PNATState pData, struct mbuf *m)
{
    register struct ip *ip = mtod(m, struct ip *);
    int hlen = ip->ip_hl << 2;
    int optlen = hlen - sizeof(struct ip );
    register struct icmp *icp;

    /*
     * Send an icmp packet back to the ip level,
     * after supplying a checksum.
     */
    m->m_data += hlen;
    m->m_len -= hlen;
    icp = mtod(m, struct icmp *);

    icp->icmp_cksum = 0;
    icp->icmp_cksum = cksum(m, ip->ip_len - hlen);

    m->m_data -= hlen;
    m->m_len += hlen;

    (void ) ip_output(pData, (struct socket *)NULL, m);

    icmpstat.icps_reflect++;
}
