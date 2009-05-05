/*
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#define WANT_SYS_IOCTL_H
#include <slirp.h>
#include "ip_icmp.h"
#include "main.h"
#ifdef __sun__
#include <sys/filio.h>
#endif
#if defined (RT_OS_WINDOWS)
#include <iphlpapi.h>
#include <icmpapi.h>
#endif


static void send_icmp_to_guest(PNATState, char *, size_t, struct socket *, const struct sockaddr_in *);
#ifdef RT_OS_WINDOWS
static void sorecvfrom_icmp_win(PNATState, struct socket *);
#else /* RT_OS_WINDOWS */
static void sorecvfrom_icmp_unix(PNATState, struct socket *);
#endif /* !RT_OS_WINDOWS */

void
so_init()
{
}


struct socket *
solookup(struct socket *head, struct in_addr laddr,
         u_int lport, struct in_addr faddr, u_int fport)
{
    struct socket *so;

    for (so = head->so_next; so != head; so = so->so_next)
    {
        if (   so->so_lport        == lport
            && so->so_laddr.s_addr == laddr.s_addr
            && so->so_faddr.s_addr == faddr.s_addr
            && so->so_fport        == fport)
            return so;
    }

    return (struct socket *)NULL;
}

/*
 * Create a new socket, initialise the fields
 * It is the responsibility of the caller to
 * insque() it into the correct linked-list
 */
struct socket *
socreate()
{
    struct socket *so;

    so = (struct socket *)RTMemAllocZ(sizeof(struct socket));
    if(so)
    {
        so->so_state = SS_NOFDREF;
        so->s = -1;
#if !defined(RT_OS_WINDOWS)
        so->so_poll_index = -1;
#endif
    }
    return so;
}

/*
 * remque and free a socket, clobber cache
 * VBOX_WITH_SLIRP_MT: before sofree queue should be locked, because
 *      in sofree we don't know from which queue item beeing removed.
 */
void
sofree(PNATState pData, struct socket *so)
{
    struct socket *so_prev = NULL;
    if (so == tcp_last_so)
        tcp_last_so = &tcb;
    else if (so == udp_last_so)
        udp_last_so = &udb;

    /* check if mbuf haven't been already freed  */
    if (so->so_m != NULL)
        m_free(pData, so->so_m);
#ifndef VBOX_WITH_SLIRP_MT
    if(so->so_next && so->so_prev)
    {
        remque(pData, so);  /* crashes if so is not in a queue */
        NSOCK_DEC();
    }

    RTMemFree(so);
#else
    so->so_deleted = 1;
#endif
}

#ifdef VBOX_WITH_SLIRP_MT
void
soread_queue(PNATState pData, struct socket *so, int *ret)
{
    *ret = soread(pData, so);
}
#endif

/*
 * Read from so's socket into sb_snd, updating all relevant sbuf fields
 * NOTE: This will only be called if it is select()ed for reading, so
 * a read() of 0 (or less) means it's disconnected
 */
int
soread(PNATState pData, struct socket *so)
{
    int n, nn, lss, total;
    struct sbuf *sb = &so->so_snd;
    size_t len = sb->sb_datalen - sb->sb_cc;
    struct iovec iov[2];
    int mss = so->so_tcpcb->t_maxseg;
    QSOCKET_LOCK(tcb);
    SOCKET_LOCK(so);
    QSOCKET_UNLOCK(tcb);

    DEBUG_CALL("soread");
    DEBUG_ARG("so = %lx", (long )so);

    /*
     * No need to check if there's enough room to read.
     * soread wouldn't have been called if there weren't
     */

    len = sb->sb_datalen - sb->sb_cc;

    iov[0].iov_base = sb->sb_wptr;
    iov[1].iov_base = 0;
    iov[1].iov_len  = 0;
    if (sb->sb_wptr < sb->sb_rptr)
    {
        iov[0].iov_len = sb->sb_rptr - sb->sb_wptr;
        /* Should never succeed, but... */
        if (iov[0].iov_len > len)
            iov[0].iov_len = len;
        if (iov[0].iov_len > mss)
            iov[0].iov_len -= iov[0].iov_len%mss;
        n = 1;
    }
    else
    {
        iov[0].iov_len = (sb->sb_data + sb->sb_datalen) - sb->sb_wptr;
        /* Should never succeed, but... */
        if (iov[0].iov_len > len)
            iov[0].iov_len = len;
        len -= iov[0].iov_len;
        if (len)
        {
            iov[1].iov_base = sb->sb_data;
            iov[1].iov_len = sb->sb_rptr - sb->sb_data;
            if(iov[1].iov_len > len)
                iov[1].iov_len = len;
            total = iov[0].iov_len + iov[1].iov_len;
            if (total > mss)
            {
                lss = total % mss;
                if (iov[1].iov_len > lss)
                {
                    iov[1].iov_len -= lss;
                    n = 2;
                }
                else
                {
                    lss -= iov[1].iov_len;
                    iov[0].iov_len -= lss;
                    n = 1;
                }
            }
            else
                n = 2;
        }
        else
        {
            if (iov[0].iov_len > mss)
                iov[0].iov_len -= iov[0].iov_len%mss;
            n = 1;
        }
    }

#ifdef HAVE_READV
    nn = readv(so->s, (struct iovec *)iov, n);
    DEBUG_MISC((dfd, " ... read nn = %d bytes\n", nn));
#else
    nn = recv(so->s, iov[0].iov_base, iov[0].iov_len, 0);
#endif
    if (nn <= 0)
    {
#if defined(RT_OS_WINDOWS)
        /*
         * Special case for WSAEnumNetworkEvents: If we receive 0 bytes that
         * _could_ mean that the connection is closed. But we will receive an
         * FD_CLOSE event later if the connection was _really_ closed. With
         * www.youtube.com I see this very often. Closing the socket too early
         * would be dangerous.
         */
        int status, ignored;
        unsigned long pending = 0;
        status = WSAIoctl(so->s, FIONREAD, NULL, 0, &pending, sizeof(unsigned long), &ignored, NULL, NULL);
        if (status < 0)
            LogRel(("NAT:error in WSAIoctl: %d\n", WSAGetLastError()));
        if (nn == 0 && (pending != 0))
        {
            SOCKET_UNLOCK(so);
            return 0;
        }
#endif
        if (nn < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK))
        {
            SOCKET_UNLOCK(so);
            return 0;
        }
        else
        {
            /* nn == 0 means peer has performed an orderly shutdown */
            DEBUG_MISC((dfd, " --- soread() disconnected, nn = %d, errno = %d-%s\n",
                        nn, errno, strerror(errno)));
            sofcantrcvmore(so);
            tcp_sockclosed(pData, sototcpcb(so));
            SOCKET_UNLOCK(so);
            return -1;
        }
    }

#ifndef HAVE_READV
    /*
     * If there was no error, try and read the second time round
     * We read again if n = 2 (ie, there's another part of the buffer)
     * and we read as much as we could in the first read
     * We don't test for <= 0 this time, because there legitimately
     * might not be any more data (since the socket is non-blocking),
     * a close will be detected on next iteration.
     * A return of -1 wont (shouldn't) happen, since it didn't happen above
     */
    if (n == 2 && nn == iov[0].iov_len)
    {
        int ret;
        ret = recv(so->s, iov[1].iov_base, iov[1].iov_len, 0);
        if (ret > 0)
            nn += ret;
    }

    DEBUG_MISC((dfd, " ... read nn = %d bytes\n", nn));
#endif

    /* Update fields */
    sb->sb_cc += nn;
    sb->sb_wptr += nn;
    if (sb->sb_wptr >= (sb->sb_data + sb->sb_datalen))
        sb->sb_wptr -= sb->sb_datalen;
    SOCKET_UNLOCK(so);
    return nn;
}

/*
 * Get urgent data
 *
 * When the socket is created, we set it SO_OOBINLINE,
 * so when OOB data arrives, we soread() it and everything
 * in the send buffer is sent as urgent data
 */
void
sorecvoob(PNATState pData, struct socket *so)
{
    struct tcpcb *tp = sototcpcb(so);

    DEBUG_CALL("sorecvoob");
    DEBUG_ARG("so = %lx", (long)so);

    /*
     * We take a guess at how much urgent data has arrived.
     * In most situations, when urgent data arrives, the next
     * read() should get all the urgent data.  This guess will
     * be wrong however if more data arrives just after the
     * urgent data, or the read() doesn't return all the
     * urgent data.
     */
    soread(pData, so);
    tp->snd_up = tp->snd_una + so->so_snd.sb_cc;
    tp->t_force = 1;
    tcp_output(pData, tp);
    tp->t_force = 0;
}

/*
 * Send urgent data
 * There's a lot duplicated code here, but...
 */
int
sosendoob(struct socket *so)
{
    struct sbuf *sb = &so->so_rcv;
    char buff[2048]; /* XXX Shouldn't be sending more oob data than this */

    int n, len;

    DEBUG_CALL("sosendoob");
    DEBUG_ARG("so = %lx", (long)so);
    DEBUG_ARG("sb->sb_cc = %d", sb->sb_cc);

    if (so->so_urgc > sizeof(buff))
        so->so_urgc = sizeof(buff); /* XXX */

    if (sb->sb_rptr < sb->sb_wptr)
    {
        /* We can send it directly */
        n = send(so->s, sb->sb_rptr, so->so_urgc, (MSG_OOB)); /* |MSG_DONTWAIT)); */
        so->so_urgc -= n;

        DEBUG_MISC((dfd, " --- sent %d bytes urgent data, %d urgent bytes left\n",
                    n, so->so_urgc));
    }
    else
    {
        /*
         * Since there's no sendv or sendtov like writev,
         * we must copy all data to a linear buffer then
         * send it all
         */
        len = (sb->sb_data + sb->sb_datalen) - sb->sb_rptr;
        if (len > so->so_urgc)
            len = so->so_urgc;
        memcpy(buff, sb->sb_rptr, len);
        so->so_urgc -= len;
        if (so->so_urgc)
        {
            n = sb->sb_wptr - sb->sb_data;
            if (n > so->so_urgc)
                n = so->so_urgc;
            memcpy(buff + len, sb->sb_data, n);
            so->so_urgc -= n;
            len += n;
        }
        n = send(so->s, buff, len, (MSG_OOB)); /* |MSG_DONTWAIT)); */
#ifdef DEBUG
        if (n != len)
            DEBUG_ERROR((dfd, "Didn't send all data urgently XXXXX\n"));
#endif
        DEBUG_MISC((dfd, " ---2 sent %d bytes urgent data, %d urgent bytes left\n",
                    n, so->so_urgc));
    }

    sb->sb_cc -= n;
    sb->sb_rptr += n;
    if (sb->sb_rptr >= (sb->sb_data + sb->sb_datalen))
        sb->sb_rptr -= sb->sb_datalen;

    return n;
}

/*
 * Write data from so_rcv to so's socket,
 * updating all sbuf field as necessary
 */
int
sowrite(PNATState pData, struct socket *so)
{
    int n, nn;
    struct sbuf *sb = &so->so_rcv;
    size_t len = sb->sb_cc;
    struct iovec iov[2];

    DEBUG_CALL("sowrite");
    DEBUG_ARG("so = %lx", (long)so);
    QSOCKET_LOCK(tcb);
    SOCKET_LOCK(so);
    QSOCKET_UNLOCK(tcb);
    if (so->so_urgc)
    {
        sosendoob(so);
        if (sb->sb_cc == 0)
        {
            SOCKET_UNLOCK(so);
            return 0;
        }
    }

    /*
     * No need to check if there's something to write,
     * sowrite wouldn't have been called otherwise
     */

    len = sb->sb_cc;

    iov[0].iov_base = sb->sb_rptr;
    iov[1].iov_base = 0;
    iov[1].iov_len  = 0;
    if (sb->sb_rptr < sb->sb_wptr)
    {
        iov[0].iov_len = sb->sb_wptr - sb->sb_rptr;
        /* Should never succeed, but... */
        if (iov[0].iov_len > len)
            iov[0].iov_len = len;
        n = 1;
    }
    else
    {
        iov[0].iov_len = (sb->sb_data + sb->sb_datalen) - sb->sb_rptr;
        if (iov[0].iov_len > len)
            iov[0].iov_len = len;
        len -= iov[0].iov_len;
        if (len)
        {
            iov[1].iov_base = sb->sb_data;
            iov[1].iov_len = sb->sb_wptr - sb->sb_data;
            if (iov[1].iov_len > len)
                iov[1].iov_len = len;
            n = 2;
        }
        else
            n = 1;
    }
    /* Check if there's urgent data to send, and if so, send it */
#ifdef HAVE_READV
    nn = writev(so->s, (const struct iovec *)iov, n);
    DEBUG_MISC((dfd, "  ... wrote nn = %d bytes\n", nn));
#else
    nn = send(so->s, iov[0].iov_base, iov[0].iov_len, 0);
#endif
    /* This should never happen, but people tell me it does *shrug* */
    if (nn < 0 && (errno == EAGAIN || errno == EINTR || errno == EWOULDBLOCK))
    {
        SOCKET_UNLOCK(so);
        return 0;
    }

    if (nn < 0 || (nn == 0 && iov[0].iov_len > 0))
    {
        DEBUG_MISC((dfd, " --- sowrite disconnected, so->so_state = %x, errno = %d\n",
                   so->so_state, errno));
        sofcantsendmore(so);
        tcp_sockclosed(pData, sototcpcb(so));
        SOCKET_UNLOCK(so);
        return -1;
    }

#ifndef HAVE_READV
    if (n == 2 && nn == iov[0].iov_len)
    {
        int ret;
        ret = send(so->s, iov[1].iov_base, iov[1].iov_len, 0);
        if (ret > 0)
            nn += ret;
    }
    DEBUG_MISC((dfd, "  ... wrote nn = %d bytes\n", nn));
#endif

    /* Update sbuf */
    sb->sb_cc -= nn;
    sb->sb_rptr += nn;
    if (sb->sb_rptr >= (sb->sb_data + sb->sb_datalen))
        sb->sb_rptr -= sb->sb_datalen;

    /*
     * If in DRAIN mode, and there's no more data, set
     * it CANTSENDMORE
     */
    if ((so->so_state & SS_FWDRAIN) && sb->sb_cc == 0)
        sofcantsendmore(so);

    SOCKET_UNLOCK(so);
    return nn;
}

/*
 * recvfrom() a UDP socket
 */
void
sorecvfrom(PNATState pData, struct socket *so)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    DEBUG_CALL("sorecvfrom");
    DEBUG_ARG("so = %lx", (long)so);

    if (so->so_type == IPPROTO_ICMP)
    {
        /* This is a "ping" reply */
#ifdef RT_OS_WINDOWS
        sorecvfrom_icmp_win(pData, so);
#else /* RT_OS_WINDOWS */
        sorecvfrom_icmp_unix(pData, so);
#endif /* !RT_OS_WINDOWS */
        udp_detach(pData, so);
    }
    else
    {
        /* A "normal" UDP packet */
        struct mbuf *m;
        struct ethhdr *eh;
        size_t len;
        u_long n;

        QSOCKET_LOCK(udb);
        SOCKET_LOCK(so);
        QSOCKET_UNLOCK(udb);

        if (!(m = m_get(pData)))
        {
            SOCKET_UNLOCK(so);
            return;
        }
        m->m_data += if_maxlinkhdr;
        m->m_data += sizeof(struct udphdr)
                   + sizeof(struct ip); /*XXX: no options atm*/

        /*
         * XXX Shouldn't FIONREAD packets destined for port 53,
         * but I don't know the max packet size for DNS lookups
         */
#if 0
        len = M_FREEROOM(m);
        /* if (so->so_fport != htons(53)) */
        {
            ioctlsocket(so->s, FIONREAD, &n);

            if (n > len)
            {
                n = (m->m_data - m->m_dat) + m->m_len + n + 1;
                m_inc(m, n);
                len = M_FREEROOM(m);
            }
        }
#else
        len = m->m_size - (if_maxlinkhdr + sizeof(struct udpiphdr)); /* get max free room here*/
#endif
        m->m_len = recvfrom(so->s, m->m_data, len, 0,
                            (struct sockaddr *)&addr, &addrlen);
        Log2((" did recvfrom %d, errno = %d-%s\n",
                    m->m_len, errno, strerror(errno)));
        if(m->m_len < 0)
        {
            u_char code = ICMP_UNREACH_PORT;

            if (errno == EHOSTUNREACH)
                code = ICMP_UNREACH_HOST;
            else if(errno == ENETUNREACH)
                code = ICMP_UNREACH_NET;

            Log2((dfd," rx error, tx icmp ICMP_UNREACH:%i\n", code));
            icmp_error(pData, so->so_m, ICMP_UNREACH, code, 0, strerror(errno));
            so->so_m = NULL;
            m_free(pData, m);
        }
        else
        {
            /*
             * Hack: domain name lookup will be used the most for UDP,
             * and since they'll only be used once there's no need
             * for the 4 minute (or whatever) timeout... So we time them
             * out much quicker (10 seconds  for now...)
             */
#ifndef VBOX_WITH_SLIRP_DNS_PROXY
            if (so->so_expire)
            {
                if (so->so_fport == htons(53))
                    so->so_expire = curtime + SO_EXPIREFAST;
                else
                    so->so_expire = curtime + SO_EXPIRE;
            }
#else
            if (so->so_expire)
            {
                if (so->so_fport != htons(53))
                    so->so_expire = curtime + SO_EXPIRE;
            }
            /* 
             *  last argument should be changed if Slirp will inject IP attributes
             *  Note: Here we can't check if dnsproxy's sent initial request
             */
            if (so->so_fport == htons(53))
                dnsproxy_answer(pData, so, m);  
#endif

#if 0
            if (m->m_len == len)
            {
                m_inc(m, MINCSIZE);
                m->m_len = 0;
            }
#endif

            /*
             * If this packet was destined for CTL_ADDR,
             * make it look like that's where it came from, done by udp_output
             */
#ifdef VBOX_WITH_NAT_SERVICE
            {
                struct ethhdr *eh0;
                struct ethhdr *eh;
                Assert(so->so_m);
                eh0 = (struct ethhdr *)so->so_m->m_dat;
                eh = (struct ethhdr *)m->m_dat;

                memcpy(eh->h_source, eh0->h_source, ETH_ALEN);
            }
#endif
            udp_output(pData, so, m, &addr);
            SOCKET_UNLOCK(so);
        } /* rx error */
    } /* if ping packet */
}

/*
 * sendto() a socket
 */
int
sosendto(PNATState pData, struct socket *so, struct mbuf *m)
{
    int ret;
    struct sockaddr_in addr;
#if 0
    struct sockaddr_in host_addr;
#endif

    DEBUG_CALL("sosendto");
    DEBUG_ARG("so = %lx", (long)so);
    DEBUG_ARG("m = %lx", (long)m);

    addr.sin_family = AF_INET;
    if ((so->so_faddr.s_addr & htonl(pData->netmask)) == special_addr.s_addr)
    {
        /* It's an alias */
        uint32_t last_byte = ntohl(so->so_faddr.s_addr) & ~pData->netmask;
        switch(last_byte)
        {
#if 0
            /* handle this case at 'default:' */
            case CTL_BROADCAST:
                addr.sin_addr.s_addr = INADDR_BROADCAST;
                /* Send the packet to host to fully emulate broadcast */
                /** @todo r=klaus: on Linux host this causes the host to receive
                 * the packet twice for some reason. And I cannot find any place
                 * in the man pages which states that sending a broadcast does not
                 * reach the host itself. */
                host_addr.sin_family = AF_INET;
                host_addr.sin_port = so->so_fport;
                host_addr.sin_addr = our_addr;
                sendto(so->s, m->m_data, m->m_len, 0,
                        (struct sockaddr *)&host_addr, sizeof (struct sockaddr));
                break;
#endif
            case CTL_DNS:
#ifndef VBOX_WITH_MULTI_DNS
                if (!get_dns_addr(pData, &dns_addr))
                    addr.sin_addr = dns_addr;
                else
                    addr.sin_addr = loopback_addr;
                break;
#endif
            case CTL_ALIAS:
            default:
                if (last_byte == ~pData->netmask)
                    addr.sin_addr.s_addr = INADDR_BROADCAST;
                else
                    addr.sin_addr = loopback_addr;
                break;
        }
    }
    else
        addr.sin_addr = so->so_faddr;
    addr.sin_port = so->so_fport;

    DEBUG_MISC((dfd, " sendto()ing, addr.sin_port=%d, addr.sin_addr.s_addr=%.16s\n",
                ntohs(addr.sin_port), inet_ntoa(addr.sin_addr)));

    /* Don't care what port we get */
    ret = sendto(so->s, m->m_data, m->m_len, 0,
                 (struct sockaddr *)&addr, sizeof (struct sockaddr));
    if (ret < 0)
    {
        LogRel(("UDP: sendto fails (%s)\n", strerror(errno)));
        return -1;
    }

    /*
     * Kill the socket if there's no reply in 4 minutes,
     * but only if it's an expirable socket
     */
    if (so->so_expire)
        so->so_expire = curtime + SO_EXPIRE;
    so->so_state = SS_ISFCONNECTED; /* So that it gets select()ed */
    return 0;
}

/*
 * XXX This should really be tcp_listen
 */
struct socket *
solisten(PNATState pData, u_int port, u_int32_t laddr, u_int lport, int flags)
{
    struct sockaddr_in addr;
    struct socket *so;
    socklen_t addrlen = sizeof(addr);
    int s, opt = 1;

    DEBUG_CALL("solisten");
    DEBUG_ARG("port = %d", port);
    DEBUG_ARG("laddr = %x", laddr);
    DEBUG_ARG("lport = %d", lport);
    DEBUG_ARG("flags = %x", flags);

    if ((so = socreate()) == NULL)
    {
        /* RTMemFree(so);      Not sofree() ??? free(NULL) == NOP */
        return NULL;
    }

    /* Don't tcp_attach... we don't need so_snd nor so_rcv */
    if ((so->so_tcpcb = tcp_newtcpcb(pData, so)) == NULL)
    {
        RTMemFree(so);
        return NULL;
    }

    SOCKET_LOCK_CREATE(so);
    SOCKET_LOCK(so);
    QSOCKET_LOCK(tcb);
    insque(pData, so,&tcb);
    NSOCK_INC();
    QSOCKET_UNLOCK(tcb);

    /*
     * SS_FACCEPTONCE sockets must time out.
     */
    if (flags & SS_FACCEPTONCE)
        so->so_tcpcb->t_timer[TCPT_KEEP] = TCPTV_KEEP_INIT*2;

    so->so_state = (SS_FACCEPTCONN|flags);
    so->so_lport = lport; /* Kept in network format */
    so->so_laddr.s_addr = laddr; /* Ditto */

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = port;

    if (   ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        || (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,(char *)&opt, sizeof(int)) < 0)
        || (bind(s,(struct sockaddr *)&addr, sizeof(addr)) < 0)
        || (listen(s, 1) < 0))
    {
#ifdef RT_OS_WINDOWS
        int tmperrno = WSAGetLastError(); /* Don't clobber the real reason we failed */
        closesocket(s);
        QSOCKET_LOCK(tcb);
        sofree(pData, so);
        QSOCKET_UNLOCK(tcb);
        /* Restore the real errno */
        WSASetLastError(tmperrno);
#else
        int tmperrno = errno; /* Don't clobber the real reason we failed */
        close(s);
        QSOCKET_LOCK(tcb);
        sofree(pData, so);
        QSOCKET_UNLOCK(tcb);
        /* Restore the real errno */
        errno = tmperrno;
#endif
        return NULL;
    }
    setsockopt(s, SOL_SOCKET, SO_OOBINLINE,(char *)&opt, sizeof(int));

    getsockname(s,(struct sockaddr *)&addr,&addrlen);
    so->so_fport = addr.sin_port;
    if (addr.sin_addr.s_addr == 0 || addr.sin_addr.s_addr == loopback_addr.s_addr)
        so->so_faddr = alias_addr;
    else
        so->so_faddr = addr.sin_addr;

    so->s = s;
    SOCKET_UNLOCK(so);
    return so;
}

/*
 * Data is available in so_rcv
 * Just write() the data to the socket
 * XXX not yet...
 */
void
sorwakeup(struct socket *so)
{
#if 0
    sowrite(so);
    FD_CLR(so->s,&writefds);
#endif
}

/*
 * Data has been freed in so_snd
 * We have room for a read() if we want to
 * For now, don't read, it'll be done in the main loop
 */
void
sowwakeup(struct socket *so)
{
}

/*
 * Various session state calls
 * XXX Should be #define's
 * The socket state stuff needs work, these often get call 2 or 3
 * times each when only 1 was needed
 */
void
soisfconnecting(struct socket *so)
{
    so->so_state &= ~(SS_NOFDREF|SS_ISFCONNECTED|SS_FCANTRCVMORE|
                      SS_FCANTSENDMORE|SS_FWDRAIN);
    so->so_state |= SS_ISFCONNECTING; /* Clobber other states */
}

void
soisfconnected(struct socket *so)
{
    so->so_state &= ~(SS_ISFCONNECTING|SS_FWDRAIN|SS_NOFDREF);
    so->so_state |= SS_ISFCONNECTED; /* Clobber other states */
}

void
sofcantrcvmore(struct  socket *so)
{
    if ((so->so_state & SS_NOFDREF) == 0)
    {
        shutdown(so->s, 0);
    }
    so->so_state &= ~(SS_ISFCONNECTING);
    if (so->so_state & SS_FCANTSENDMORE)
        so->so_state = SS_NOFDREF; /* Don't select it */
                                   /* XXX close() here as well? */
    else
        so->so_state |= SS_FCANTRCVMORE;
}

void
sofcantsendmore(struct socket *so)
{
    if ((so->so_state & SS_NOFDREF) == 0)
        shutdown(so->s, 1);           /* send FIN to fhost */

    so->so_state &= ~(SS_ISFCONNECTING);
    if (so->so_state & SS_FCANTRCVMORE)
        so->so_state = SS_NOFDREF; /* as above */
    else
        so->so_state |= SS_FCANTSENDMORE;
}

void
soisfdisconnected(struct socket *so)
{
#if 0
    so->so_state &= ~(SS_ISFCONNECTING|SS_ISFCONNECTED);
    close(so->s);
    so->so_state = SS_ISFDISCONNECTED;
    /*
     * XXX Do nothing ... ?
     */
#endif
}

/*
 * Set write drain mode
 * Set CANTSENDMORE once all data has been write()n
 */
void
sofwdrain(struct socket *so)
{
    if (so->so_rcv.sb_cc)
        so->so_state |= SS_FWDRAIN;
    else
        sofcantsendmore(so);
}

static void
send_icmp_to_guest(PNATState pData, char *buff, size_t len, struct socket *so, const struct sockaddr_in *addr)
{
    struct ip *ip;
    uint32_t dst, src;
    char ip_copy[256];
    struct icmp *icp;
    int old_ip_len = 0;
    int hlen, original_hlen = 0;
    struct mbuf *m;
    struct icmp_msg *icm;
    uint8_t proto;
    int type = 0;

    ip = (struct ip *)buff;
    hlen = (ip->ip_hl << 2);
    icp = (struct icmp *)((char *)ip + hlen);

    Log(("ICMP:received msg(t:%d, c:%d)\n", icp->icmp_type, icp->icmp_code));
    if (   icp->icmp_type != ICMP_ECHOREPLY
        && icp->icmp_type != ICMP_TIMXCEED
        && icp->icmp_type != ICMP_UNREACH)
    {
        return;
    }

    type = icp->icmp_type;
    if (   type == ICMP_TIMXCEED
        || type == ICMP_UNREACH)
    {
        ip = &icp->icmp_ip;
        DO_ALIAS(&ip->ip_dst);
    }
    else
    {
        DO_ALIAS(&ip->ip_src);
    }

    icm = icmp_find_original_mbuf(pData, ip);

    if (icm == NULL)
    {
        Log(("NAT: Can't find the corresponding packet for the received ICMP\n"));
        return;
    }

    m = icm->im_m;
    Assert(m != NULL);

    src = addr->sin_addr.s_addr;

    ip = mtod(m, struct ip *);
    proto = ip->ip_p;
    /* Now ip is pointing on header we've sent from guest */
    if (   icp->icmp_type == ICMP_TIMXCEED
        || icp->icmp_type == ICMP_UNREACH)
    {
        old_ip_len = (ip->ip_hl << 2) + 64;
        if (old_ip_len > sizeof(ip_copy))
            old_ip_len = sizeof(ip_copy);
        memcpy(ip_copy, ip, old_ip_len);
    }

    /* source address from original IP packet*/
    dst = ip->ip_src.s_addr;

    /* overide ther tail of old packet */
    ip = mtod(m, struct ip *); /* ip is from mbuf we've overrided */
    original_hlen = ip->ip_hl << 2;
    /* saves original ip header and options */
    memcpy(m->m_data + original_hlen, buff + hlen, len - hlen);
    m->m_len = len - hlen + original_hlen;
    ip->ip_len = m->m_len;
    ip->ip_p = IPPROTO_ICMP; /* the original package could be whatever, but we're response via ICMP*/

    icp = (struct icmp *)((char *)ip + (ip->ip_hl << 2));
    type = icp->icmp_type;
    if (   type == ICMP_TIMXCEED
        || type == ICMP_UNREACH)
    {
        /* according RFC 793 error messages required copy of initial IP header + 64 bit */
        memcpy(&icp->icmp_ip, ip_copy, old_ip_len);
        ip->ip_tos = ((ip->ip_tos & 0x1E) | 0xC0);  /* high priority for errors */
    }

    ip->ip_src.s_addr = src;
    ip->ip_dst.s_addr = dst;
    icmp_reflect(pData, m);
    LIST_REMOVE(icm, im_list);
    /* Don't call m_free here*/

    if (   type == ICMP_TIMXCEED
        || type == ICMP_UNREACH)
    {
        icm->im_so->so_m = NULL;
        switch (proto)
        {
            case  IPPROTO_UDP:
                /*XXX: so->so_m already freed so we shouldn't call sofree */
                udp_detach(pData, icm->im_so);
            break;
            case  IPPROTO_TCP:
                /*close tcp should be here */
            break;
            default:
            /* do nothing */
            break;
        }
    }
    RTMemFree(icm);
}

#ifdef RT_OS_WINDOWS
static void
sorecvfrom_icmp_win(PNATState pData, struct socket *so)
{
    int len;
    int i;
    struct ip *ip;
    struct mbuf *m;
    struct icmp *icp;
    struct icmp_msg *icm;
    struct ip *ip_broken; /* ICMP returns header + 64 bit of packet */
    uint32_t src;
    ICMP_ECHO_REPLY *icr;
    int hlen = 0;
    int data_len = 0;
    int nbytes = 0;
    u_char code = ~0;

    len = pData->pfIcmpParseReplies(pData->pvIcmpBuffer, pData->szIcmpBuffer);
    if (len < 0)
    {
        LogRel(("NAT: Error (%d) occurred on ICMP receiving\n", GetLastError()));
        return;
    }
    if (len == 0)
        return; /* no error */

    icr = (ICMP_ECHO_REPLY *)pData->pvIcmpBuffer;
    for (i = 0; i < len; ++i)
    {
        switch(icr[i].Status)
        {
            case IP_DEST_HOST_UNREACHABLE:
                code = (code != ~0 ? code : ICMP_UNREACH_HOST);
            case IP_DEST_NET_UNREACHABLE:
                code = (code != ~0 ? code : ICMP_UNREACH_NET);
            case IP_DEST_PROT_UNREACHABLE:
                code = (code != ~0 ? code : ICMP_UNREACH_PROTOCOL);
                /* UNREACH error inject here */
            case IP_DEST_PORT_UNREACHABLE:
                code = (code != ~0 ? code : ICMP_UNREACH_PORT);
                icmp_error(pData, so->so_m, ICMP_UNREACH, code, 0, "Error occurred!!!");
                so->so_m = NULL;
                break;
            case IP_SUCCESS: /* echo replied */
                m = m_get(pData);
                m->m_data += if_maxlinkhdr;
                ip = mtod(m, struct ip *);
                ip->ip_src.s_addr = icr[i].Address;
                DO_ALIAS(&ip->ip_src);
                ip->ip_p = IPPROTO_ICMP;
                ip->ip_dst.s_addr = so->so_laddr.s_addr; /*XXX: still the hack*/
                data_len = sizeof(struct ip);
                ip->ip_hl =  data_len >> 2; /* requiered for icmp_reflect, no IP options */
                ip->ip_ttl = icr[i].Options.Ttl;

                icp = (struct icmp *)&ip[1]; /* no options */
                icp->icmp_type = ICMP_ECHOREPLY;
                icp->icmp_code = 0;
                icp->icmp_id = so->so_icmp_id;
                icp->icmp_seq = so->so_icmp_seq;

                data_len += ICMP_MINLEN;

                nbytes = (data_len + icr[i].DataSize > m->m_size? m->m_size - data_len: icr[i].DataSize);
                memcpy(icp->icmp_data, icr[i].Data, nbytes);

                data_len += icr[i].DataSize;

                ip->ip_len = data_len;
                m->m_len = ip->ip_len;

                icmp_reflect(pData, m);
                break;
            case IP_TTL_EXPIRED_TRANSIT: /* TTL expired */

                ip_broken = icr[i].Data;
                icm = icmp_find_original_mbuf(pData, ip_broken);
                if (icm == NULL) {
                    Log(("ICMP: can't find original package (first double word %x)\n", *(uint32_t *)ip_broken));
                    return;
                }
                m = icm->im_m;
                ip = mtod(m, struct ip *);
                ip->ip_ttl = icr[i].Options.Ttl;
                src = ip->ip_src.s_addr;
                ip->ip_dst.s_addr = src;
                ip->ip_dst.s_addr = icr[i].Address;

                hlen = (ip->ip_hl << 2);
                icp = (struct icmp *)((char *)ip + hlen);
                ip_broken->ip_src.s_addr = src; /*it packet sent from host not from guest*/
                data_len = (ip_broken->ip_hl << 2) + 64;

                nbytes =(hlen + ICMP_MINLEN + data_len > m->m_size? m->m_size - (hlen + ICMP_MINLEN): data_len);
                memcpy(icp->icmp_data, ip_broken,  nbytes);
                icmp_reflect(pData, m);
                break;
            default:
                Log(("ICMP(default): message with Status: %x was received from %x\n", icr[i].Status, icr[i].Address));
                break;
        }
    }
}
#else /* RT_OS_WINDOWS */
static void sorecvfrom_icmp_unix(PNATState pData, struct socket *so)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    char buff[1500];
    int len;
    len = recvfrom(so->s, buff, 1500, 0,
                   (struct sockaddr *)&addr, &addrlen);
    /* XXX Check if reply is "correct"? */

    if (len == -1 || len == 0)
    {
        u_char code = ICMP_UNREACH_PORT;

        if (errno == EHOSTUNREACH)
            code = ICMP_UNREACH_HOST;
        else if(errno == ENETUNREACH)
            code = ICMP_UNREACH_NET;

        DEBUG_MISC((dfd," udp icmp rx errno = %d-%s\n",
                    errno, strerror(errno)));
        icmp_error(pData, so->so_m, ICMP_UNREACH, code, 0, strerror(errno));
        so->so_m = NULL;
    }
    else
    {
        send_icmp_to_guest(pData, buff, len, so, &addr);
    }
}
#endif /* !RT_OS_WINDOWS */
