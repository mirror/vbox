/*
 * Copyright (c) 1982, 1986, 1988, 1990, 1993
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
 *      @(#)udp_usrreq.c        8.4 (Berkeley) 1/21/94
 * udp_usrreq.c,v 1.4 1994/10/02 17:48:45 phk Exp
 */

/*
 * Changes and additions relating to SLiRP
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#include <slirp.h>
#include "ip_icmp.h"
#include "ctl.h"


/*
 * UDP protocol implementation.
 * Per RFC 768, August, 1980.
 */
#define udpcksum 1

void
udp_init(PNATState pData)
{
    udp_last_so = &udb;
    udb.so_next = udb.so_prev = &udb;
}

/* m->m_data  points at ip packet header
 * m->m_len   length ip packet
 * ip->ip_len length data (IPDU)
 */
void
udp_input(PNATState pData, register struct mbuf *m, int iphlen)
{
    register struct ip *ip;
    register struct udphdr *uh;
    int len;
    struct ip save_ip;
    struct socket *so;
    int ret;
    int ttl;

    DEBUG_CALL("udp_input");
    DEBUG_ARG("m = %lx", (long)m);
    ip = mtod(m, struct ip *);
    DEBUG_ARG("iphlen = %d", iphlen);
    Log2(("%R[IP4] iphlen = %d\n", &ip->ip_dst, iphlen));

    udpstat.udps_ipackets++;

    /*
     * Strip IP options, if any; should skip this,
     * make available to user, and use on returned packets,
     * but we don't yet have a way to check the checksum
     * with options still present.
     */
    if (iphlen > sizeof(struct ip))
    {
        ip_stripoptions(m, (struct mbuf *)0);
        iphlen = sizeof(struct ip);
    }

    /*
     * Get IP and UDP header together in first mbuf.
     */
    ip = mtod(m, struct ip *);
    uh = (struct udphdr *)((caddr_t)ip + iphlen);

    /*
     * Make mbuf data length reflect UDP length.
     * If not enough data to reflect UDP length, drop.
     */
    len = ntohs((u_int16_t)uh->uh_ulen);
    Assert((ip->ip_len == len));
    Assert((ip->ip_len + iphlen == m->m_len));

    if (ip->ip_len != len)
    {
        if (len > ip->ip_len)
        {
            udpstat.udps_badlen++;
            Log3(("NAT: IP(id: %hd) has bad size\n", ip->ip_id));
        }
        m_adj(m, len - ip->ip_len);
        ip->ip_len = len;
    }

    /*
     * Save a copy of the IP header in case we want restore it
     * for sending an ICMP error message in response.
     */
    save_ip = *ip;
    save_ip.ip_len+= iphlen;         /* tcp_input subtracts this */

    /*
     * Checksum extended UDP header and data.
     */
    if (udpcksum && uh->uh_sum)
    {
        memset(((struct ipovly *)ip)->ih_x1, 0, 9);
        ((struct ipovly *)ip)->ih_len = uh->uh_ulen;
#if 0
        /* keep uh_sum for ICMP reply */
        uh->uh_sum = cksum(m, len + sizeof (struct ip));
        if (uh->uh_sum)
        {

#endif
            if(cksum(m, len + iphlen))
            {
                udpstat.udps_badsum++;
                Log3(("NAT: IP(id: %hd) has bad (udp) cksum\n", ip->ip_id));
                goto bad;
            }
        }
#if 0
    }
#endif

    /*
     *  handle DHCP/BOOTP
     */
    if (ntohs(uh->uh_dport) == BOOTP_SERVER)
    {
        bootp_input(pData, m);
        goto done;
    }

    if (   ntohs(uh->uh_dport) == 53
        && CTL_CHECK(ntohl(ip->ip_dst.s_addr), CTL_DNS))
    {
        struct sockaddr_in dst, src;
        src.sin_addr.s_addr = ip->ip_dst.s_addr;
        src.sin_port = uh->uh_dport;
        dst.sin_addr.s_addr = ip->ip_src.s_addr;
        dst.sin_port = uh->uh_sport;
        /* udp_output2 will do opposite operations on mbuf*/
        
        m->m_data += sizeof(struct udpiphdr);
        m->m_len -= sizeof(struct udpiphdr);
        udp_output2(pData, NULL, m, &src, &dst, IPTOS_LOWDELAY);
        goto done;
    }
    /*
     *  handle TFTP
     */
#ifndef VBOX_WITH_SLIRP_BSD_MBUF
    if (   ntohs(uh->uh_dport) == TFTP_SERVER
        && CTL_CHECK(ntohl(ip->ip_dst.s_addr), CTL_TFTP))
    {
        tftp_input(pData, m);
        goto done;
    }
#endif

    /*
     * Locate pcb for datagram.
     */
    so = udp_last_so;
    if (   so->so_lport != uh->uh_sport
        || so->so_laddr.s_addr != ip->ip_src.s_addr)
    {
        struct socket *tmp;

        for (tmp = udb.so_next; tmp != &udb; tmp = tmp->so_next)
        {
            if (   tmp->so_lport        == uh->uh_sport
                && tmp->so_laddr.s_addr == ip->ip_src.s_addr)
            {
                so = tmp;
                break;
            }
        }
        if (tmp == &udb)
            so = NULL;
        else
        {
            udpstat.udpps_pcbcachemiss++;
            udp_last_so = so;
        }
    }

    if (so == NULL)
    {
        /*
         * If there's no socket for this packet,
         * create one
         */
        if ((so = socreate()) == NULL)
        {
            Log3(("NAT: IP(id: %hd) failed to create socket\n", ip->ip_id));
            goto bad;
        }
        if (udp_attach(pData, so, 0) == -1)
        {
            Log3(("NAT: IP(id: %hd) udp_attach errno = %d-%s\n",
                        ip->ip_id, errno, strerror(errno)));
            sofree(pData, so);
            goto bad;
        }

        /*
         * Setup fields
         */
        /* udp_last_so = so; */
        so->so_laddr = ip->ip_src;
        so->so_lport = uh->uh_sport;

        if ((so->so_iptos = udp_tos(so)) == 0)
            so->so_iptos = ip->ip_tos;

        /*
         * XXXXX Here, check if it's in udpexec_list,
         * and if it is, do the fork_exec() etc.
         */
    }

    so->so_faddr = ip->ip_dst;   /* XXX */
    so->so_fport = uh->uh_dport; /* XXX */

    /*
     * DNS proxy
     */
    if (   pData->use_dns_proxy
        && (ip->ip_dst.s_addr == htonl(ntohl(special_addr.s_addr) | CTL_DNS))
        && (ntohs(uh->uh_dport) == 53)) 
    {
        dnsproxy_query(pData, so, m, iphlen);
        goto done;
    }

    iphlen += sizeof(struct udphdr);
    m->m_len -= iphlen;
    m->m_data += iphlen;

    /*
     * Now we sendto() the packet.
     */
    if (so->so_emu)
        udp_emu(pData, so, m);

    ttl = ip->ip_ttl = save_ip.ip_ttl;
    ret = setsockopt(so->s, IPPROTO_IP, IP_TTL, (const char*)&ttl, sizeof(ttl));
    if (ret < 0)
        LogRel(("NAT: Error (%s) occurred while setting TTL(%d) attribute "
                "of IP packet to socket %R[natsock]\n", strerror(errno), ip->ip_ttl, so));

    if (sosendto(pData, so, m) == -1)
    {
        m->m_len += iphlen;
        m->m_data -= iphlen;
        *ip = save_ip;
        DEBUG_MISC((dfd,"NAT: UDP tx errno = %d-%s (on sent to %R[IP4])\n", errno, 
                strerror(errno), &ip->ip_dst));
        icmp_error(pData, m, ICMP_UNREACH, ICMP_UNREACH_NET, 0, strerror(errno));
        /* in case we receive ICMP on this socket we'll aware that ICMP has been already sent to host*/
        so->so_m = NULL; 
    }

    if (so->so_m)
        m_free(pData, so->so_m);   /* used for ICMP if error on sorecvfrom */

    /* restore the orig mbuf packet */
    m->m_len += iphlen;
    m->m_data -= iphlen;
    *ip = save_ip;
    so->so_m = m;         /* ICMP backup */

    return;

bad:
    Log2(("NAT: UDP(id: %hd) datagram to %R[IP4] with size(%d) claimed as bad\n", 
        ip->ip_id, &ip->ip_dst, ip->ip_len));
done: 
    /* some services like bootp(built-in), dns(buildt-in) and dhcp don't need sockets
     * and create new m'buffers to send them to guest, so we'll free their incomming 
     * buffers here. 
     */
    m_freem(pData, m);
    return;
}

int udp_output2(PNATState pData, struct socket *so, struct mbuf *m,
                struct sockaddr_in *saddr, struct sockaddr_in *daddr,
                int iptos)
{
    register struct udpiphdr *ui;
    int error = 0;

    DEBUG_CALL("udp_output");
    DEBUG_ARG("so = %lx", (long)so);
    DEBUG_ARG("m = %lx", (long)m);
    DEBUG_ARG("saddr = %lx", (long)saddr->sin_addr.s_addr);
    DEBUG_ARG("daddr = %lx", (long)daddr->sin_addr.s_addr);

    /*
     * Adjust for header
     */
    m->m_data -= sizeof(struct udpiphdr);
    m->m_len += sizeof(struct udpiphdr);

    /*
     * Fill in mbuf with extended UDP header
     * and addresses and length put into network format.
     */
    ui = mtod(m, struct udpiphdr *);
    memset(ui->ui_x1, 0, 9);
    ui->ui_pr = IPPROTO_UDP;
    ui->ui_len = htons(m->m_len - sizeof(struct ip)); 
    /* XXXXX Check for from-one-location sockets, or from-any-location sockets */
    ui->ui_src = saddr->sin_addr;
    ui->ui_dst = daddr->sin_addr;
    ui->ui_sport = saddr->sin_port;
    ui->ui_dport = daddr->sin_port;
    ui->ui_ulen = ui->ui_len;

    /*
     * Stuff checksum and output datagram.
     */
    ui->ui_sum = 0;
    if (udpcksum)
    {
        if ((ui->ui_sum = cksum(m, /* sizeof (struct udpiphdr) + */ m->m_len)) == 0)
            ui->ui_sum = 0xffff;
    }
    ((struct ip *)ui)->ip_len = m->m_len;
    ((struct ip *)ui)->ip_ttl = ip_defttl;
    ((struct ip *)ui)->ip_tos = iptos;

    udpstat.udps_opackets++;

    error = ip_output(pData, so, m);

    return error;
}

int udp_output(PNATState pData, struct socket *so, struct mbuf *m,
               struct sockaddr_in *addr)
{
    struct sockaddr_in saddr, daddr;

    saddr = *addr;
    if ((so->so_faddr.s_addr & htonl(pData->netmask)) == special_addr.s_addr)
    {
        saddr.sin_addr.s_addr = so->so_faddr.s_addr;
        if ((so->so_faddr.s_addr & htonl(~pData->netmask)) == htonl(~pData->netmask))
            saddr.sin_addr.s_addr = alias_addr.s_addr;
    }

    /* Any UDP packet to the loopback address must be translated to be from
     * the forwarding address, i.e. 10.0.2.2. */
    if (   (saddr.sin_addr.s_addr & htonl(IN_CLASSA_NET))
        == htonl(INADDR_LOOPBACK & IN_CLASSA_NET))
        saddr.sin_addr.s_addr = alias_addr.s_addr;

    daddr.sin_addr = so->so_laddr;
    daddr.sin_port = so->so_lport;

    return udp_output2(pData, so, m, &saddr, &daddr, so->so_iptos);
}

int
udp_attach(PNATState pData, struct socket *so, int service_port)
{
    struct sockaddr_in *addr;
    struct sockaddr sa_addr;
    socklen_t socklen = sizeof(struct sockaddr);
    int status;
    int opt = 1;

    if ((so->s = socket(AF_INET, SOCK_DGRAM, 0)) == -1)
        goto error;
    /*
     * Here, we bind() the socket.  Although not really needed
     * (sendto() on an unbound socket will bind it), it's done
     * here so that emulation of ytalk etc. don't have to do it
     */
    memset(&sa_addr, 0, sizeof(struct sockaddr));
    addr = (struct sockaddr_in *)&sa_addr;
#ifdef RT_OS_DARWIN
    addr->sin_len = sizeof(struct sockaddr_in);
#endif
    addr->sin_family = AF_INET;
    addr->sin_port = service_port;
    addr->sin_addr.s_addr = pData->bindIP.s_addr;
    fd_nonblock(so->s);
    if (bind(so->s, &sa_addr, sizeof(struct sockaddr_in)) < 0)
    {
        int lasterrno = errno;
        closesocket(so->s);
        so->s = -1;
#ifdef RT_OS_WINDOWS
        WSASetLastError(lasterrno);
#else
        errno = lasterrno;
#endif
        goto error;
    }
    /* success, insert in queue */
    so->so_expire = curtime + SO_EXPIRE;
    /* enable broadcast for later use */
    setsockopt(so->s, SOL_SOCKET, SO_BROADCAST, (const char *)&opt, sizeof(opt));
    status = getsockname(so->s, &sa_addr, &socklen);
    Assert(status == 0 && sa_addr.sa_family == AF_INET);
    so->so_hlport = ((struct sockaddr_in *)&sa_addr)->sin_port;
    so->so_hladdr.s_addr = ((struct sockaddr_in *)&sa_addr)->sin_addr.s_addr;
    SOCKET_LOCK_CREATE(so);
    QSOCKET_LOCK(udb);
    insque(pData, so, &udb);
    NSOCK_INC();
    QSOCKET_UNLOCK(udb);
    return so->s;
error:
    LogRel(("NAT: can't create datagramm socket\n"));
    return -1;
}

void
udp_detach(PNATState pData, struct socket *so)
{
    if (so != &pData->icmp_socket)
    {
        QSOCKET_LOCK(udb);
        SOCKET_LOCK(so);
        QSOCKET_UNLOCK(udb);
        closesocket(so->s);
        sofree(pData, so);
        SOCKET_UNLOCK(so);
    }
}

static const struct tos_t udptos[] =
{
    {   0,    53, IPTOS_LOWDELAY, 0           }, /* DNS */
    { 517,   517, IPTOS_LOWDELAY, EMU_TALK    }, /* talk */
    { 518,   518, IPTOS_LOWDELAY, EMU_NTALK   }, /* ntalk */
    {   0,  7648, IPTOS_LOWDELAY, EMU_CUSEEME }, /* Cu-Seeme */
    {   0,     0, 0,              0           }
};

u_int8_t
udp_tos(struct socket *so)
{
    int i = 0;

    while(udptos[i].tos)
    {
        if (   (udptos[i].fport && ntohs(so->so_fport) == udptos[i].fport)
            || (udptos[i].lport && ntohs(so->so_lport) == udptos[i].lport))
        {
            so->so_emu = udptos[i].emu;
            return udptos[i].tos;
        }
        i++;
    }

    return 0;
}

#ifdef EMULATE_TALK
#include "talkd.h"
#endif

/*
 * Here, talk/ytalk/ntalk requests must be emulated
 */
void
udp_emu(PNATState pData, struct socket *so, struct mbuf *m)
{
    so->so_emu = 0;
}

struct socket *
udp_listen(PNATState pData, u_int32_t bind_addr, u_int port, u_int32_t laddr, u_int lport, int flags)
{
    struct sockaddr_in addr;
    struct socket *so;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    int opt = 1;

    if ((so = socreate()) == NULL)
        return NULL;

    so->s = socket(AF_INET, SOCK_DGRAM, 0);
    if (so->s == -1)
    {
        LogRel(("NAT: can't create datagram socket\n"));
        RTMemFree(so);
        return NULL;
    }
    so->so_expire = curtime + SO_EXPIRE;
    fd_nonblock(so->s);
    SOCKET_LOCK_CREATE(so);
    QSOCKET_LOCK(udb);
    insque(pData, so,&udb);
    NSOCK_INC();
    QSOCKET_UNLOCK(udb);

    memset(&addr, 0, sizeof(addr));
#ifdef RT_OS_DARWIN
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = bind_addr;
    addr.sin_port = port;

    if (bind(so->s,(struct sockaddr *)&addr, addrlen) < 0)
    {
        LogRel(("NAT: bind to %R[IP4] has been failed\n", &addr.sin_addr));
        udp_detach(pData, so);
        return NULL;
    }
    setsockopt(so->s, SOL_SOCKET, SO_REUSEADDR,(char *)&opt, sizeof(int));
/*  setsockopt(so->s, SOL_SOCKET, SO_OOBINLINE,(char *)&opt, sizeof(int)); */

    getsockname(so->s,(struct sockaddr *)&addr,&addrlen);
    so->so_fport = addr.sin_port;
    /* The original check was completely broken, as the commented out
     * if statement was always true (INADDR_ANY=0). */
    /* if (addr.sin_addr.s_addr == 0 || addr.sin_addr.s_addr == loopback_addr.s_addr) */
    if (1 == 0)                 /* always use the else part */
        so->so_faddr = alias_addr;
    else
        so->so_faddr = addr.sin_addr;

    so->so_lport = lport;
    so->so_laddr.s_addr = laddr;
    if (flags != SS_FACCEPTONCE)
        so->so_expire = 0;

    so->so_state = SS_ISFCONNECTED;

    return so;
}
