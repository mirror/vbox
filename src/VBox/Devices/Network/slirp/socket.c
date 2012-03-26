/* $Id$ */
/** @file
 * NAT - socket handling.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*
 * This code is based on:
 *
 * Copyright (c) 1995 Danny Gasparovski.
 *
 * Please read the file COPYRIGHT for the
 * terms and conditions of the copyright.
 */

#include <slirp.h>
#include "ip_icmp.h"
#include "main.h"
#ifdef __sun__
#include <sys/filio.h>
#endif
#include <VBox/vmm/pdmdrv.h>
#if defined (RT_OS_WINDOWS)
#include <iphlpapi.h>
#include <icmpapi.h>
#endif

#ifdef VBOX_WITH_NAT_UDP_SOCKET_CLONE
/**
 *
 */
struct socket * soCloneUDPSocketWithForegnAddr(PNATState pData, bool fBindSocket, struct socket *pSo, uint32_t u32ForeignAddr)
{
    struct socket *pNewSocket = NULL;
    LogFlowFunc(("Enter: fBindSocket:%RTbool, so:%R[natsock], u32ForeignAddr:%RTnaipv4\n", fBindSocket, pSo, u32ForeignAddr));
    pNewSocket = socreate();
    if (!pNewSocket)
    {
        LogFunc(("Can't create socket\n"));
        LogFlowFunc(("Leave: NULL\n"));
        return NULL;
    }
    if (fBindSocket)
    {
        if (udp_attach(pData, pNewSocket, 0) <= 0)
        {
            sofree(pData, pNewSocket);
            LogFunc(("Can't attach fresh created socket\n"));
            return NULL;
        }
    }
    else
    {
        pNewSocket->so_cloneOf = (struct socket *)pSo;
        pNewSocket->s = pSo->s;
        insque(pData, pNewSocket, &udb);
    }
    pNewSocket->so_laddr = pSo->so_laddr;
    pNewSocket->so_lport = pSo->so_lport;
    pNewSocket->so_faddr.s_addr = u32ForeignAddr;
    pNewSocket->so_fport = pSo->so_fport;
    pSo->so_cCloneCounter++;
    LogFlowFunc(("Leave: %R[natsock]\n", pNewSocket));
    return pNewSocket;
}

struct socket *soLookUpClonedUDPSocket(PNATState pData, const struct socket *pcSo, uint32_t u32ForeignAddress)
{
    struct socket *pSoClone = NULL;
    LogFlowFunc(("Enter: pcSo:%R[natsock], u32ForeignAddress:%RTnaipv4\n", pcSo, u32ForeignAddress));
    for (pSoClone = udb.so_next; pSoClone != &udb; pSoClone = pSoClone->so_next)
    {
        if (   pSoClone->so_cloneOf
            && pSoClone->so_cloneOf == pcSo
            && pSoClone->so_lport == pcSo->so_lport
            && pSoClone->so_fport == pcSo->so_fport
            && pSoClone->so_laddr.s_addr == pcSo->so_laddr.s_addr
            && pSoClone->so_faddr.s_addr == u32ForeignAddress)
            goto done;
    }
    pSoClone = NULL;
done:
    LogFlowFunc(("Leave: pSoClone: %R[natsock]\n", pSoClone));
    return pSoClone;
}
#endif

#ifdef VBOX_WITH_NAT_SEND2HOME
DECLINLINE(bool) slirpSend2Home(PNATState pData, struct socket *pSo, const void *pvBuf, uint32_t cbBuf, int iFlags)
{
    int idxAddr;
    int ret = 0;
    bool fSendDone = false;
    LogFlowFunc(("Enter pSo:%R[natsock] pvBuf: %p, cbBuf: %d, iFlags: %d\n", pSo, pvBuf, cbBuf, iFlags));
    for (idxAddr = 0; idxAddr < pData->cInHomeAddressSize; ++idxAddr)
    {

        struct socket *pNewSocket = soCloneUDPSocketWithForegnAddr(pData, pSo, pData->pInSockAddrHomeAddress[idxAddr].sin_addr);
        AssertReturn((pNewSocket, false));
        pData->pInSockAddrHomeAddress[idxAddr].sin_port = pSo->so_fport;
        /* @todo: more verbose on errors,
         * @note: we shouldn't care if this send fail or not (we're in broadcast).
         */
        LogFunc(("send %d bytes to %RTnaipv4 from %R[natsock]\n", cbBuf, pData->pInSockAddrHomeAddress[idxAddr].sin_addr.s_addr, pNewSocket));
        ret = sendto(pNewSocket->s, pvBuf, cbBuf, iFlags, (struct sockaddr *)&pData->pInSockAddrHomeAddress[idxAddr], sizeof(struct sockaddr_in));
        if (ret < 0)
            LogFunc(("Failed to send %d bytes to %RTnaipv4\n", cbBuf, pData->pInSockAddrHomeAddress[idxAddr].sin_addr.s_addr));
        fSendDone |= ret > 0;
    }
    LogFlowFunc(("Leave %RTbool\n", fSendDone));
    return fSendDone;
}
#endif /* !VBOX_WITH_NAT_SEND2HOME */
static void send_icmp_to_guest(PNATState, char *, size_t, const struct sockaddr_in *);
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
    if (so)
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
    if (so == tcp_last_so)
        tcp_last_so = &tcb;
    else if (so == udp_last_so)
        udp_last_so = &udb;
    /* libalias notification */
    if (so->so_pvLnk)
        slirpDeleteLinkSocket(so->so_pvLnk);

    /* check if mbuf haven't been already freed  */
    if (so->so_m != NULL)
        m_freem(pData, so->so_m);
#ifndef VBOX_WITH_SLIRP_MT
    if (so->so_next && so->so_prev)
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
#ifndef VBOX_WITH_SLIRP_BSD_SBUF
int
soread(PNATState pData, struct socket *so)
{
    int n, nn, lss, total;
    struct sbuf *sb = &so->so_snd;
    size_t len = sb->sb_datalen - sb->sb_cc;
    struct iovec iov[2];
    int mss = so->so_tcpcb->t_maxseg;

    STAM_PROFILE_START(&pData->StatIOread, a);
    STAM_COUNTER_RESET(&pData->StatIORead_in_1);
    STAM_COUNTER_RESET(&pData->StatIORead_in_2);

    QSOCKET_LOCK(tcb);
    SOCKET_LOCK(so);
    QSOCKET_UNLOCK(tcb);

    LogFlow(("soread: so = %R[natsock]\n", so));
    Log2(("%s: so = %R[natsock] so->so_snd = %R[sbuf]\n", __PRETTY_FUNCTION__, so, sb));

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
            if (iov[1].iov_len > len)
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
#else
    nn = recv(so->s, iov[0].iov_base, iov[0].iov_len, (so->so_tcpcb->t_force? MSG_OOB:0));
#endif
    Log2(("%s: read(1) nn = %d bytes\n", __PRETTY_FUNCTION__, nn));
    Log2(("%s: so = %R[natsock] so->so_snd = %R[sbuf]\n", __PRETTY_FUNCTION__, so, sb));
    if (nn <= 0)
    {
        /*
         * Special case for WSAEnumNetworkEvents: If we receive 0 bytes that
         * _could_ mean that the connection is closed. But we will receive an
         * FD_CLOSE event later if the connection was _really_ closed. With
         * www.youtube.com I see this very often. Closing the socket too early
         * would be dangerous.
         */
        int status;
        unsigned long pending = 0;
        status = ioctlsocket(so->s, FIONREAD, &pending);
        if (status < 0)
            Log(("NAT:%s: error in WSAIoctl: %d\n", __PRETTY_FUNCTION__, errno));
        if (nn == 0 && (pending != 0))
        {
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOread, a);
            return 0;
        }
        if (   nn < 0
            && (   errno == EINTR
                || errno == EAGAIN
                || errno == EWOULDBLOCK))
        {
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOread, a);
            return 0;
        }
        else
        {
            /* nn == 0 means peer has performed an orderly shutdown */
            Log2(("%s: disconnected, nn = %d, errno = %d (%s)\n",
                  __PRETTY_FUNCTION__, nn, errno, strerror(errno)));
            sofcantrcvmore(so);
            tcp_sockclosed(pData, sototcpcb(so));
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOread, a);
            return -1;
        }
    }
    STAM_STATS(
        if (n == 1)
        {
            STAM_COUNTER_INC(&pData->StatIORead_in_1);
            STAM_COUNTER_ADD(&pData->StatIORead_in_1_bytes, nn);
        }
        else
        {
            STAM_COUNTER_INC(&pData->StatIORead_in_2);
            STAM_COUNTER_ADD(&pData->StatIORead_in_2_1st_bytes, nn);
        }
    );

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
        STAM_STATS(
            if (ret > 0)
            {
                STAM_COUNTER_INC(&pData->StatIORead_in_2);
                STAM_COUNTER_ADD(&pData->StatIORead_in_2_2nd_bytes, ret);
            }
        );
    }

    Log2(("%s: read(2) nn = %d bytes\n", __PRETTY_FUNCTION__, nn));
#endif

    /* Update fields */
    sb->sb_cc += nn;
    sb->sb_wptr += nn;
    Log2(("%s: update so_snd (readed nn = %d) %R[sbuf]\n", __PRETTY_FUNCTION__, nn, sb));
    if (sb->sb_wptr >= (sb->sb_data + sb->sb_datalen))
    {
        sb->sb_wptr -= sb->sb_datalen;
        Log2(("%s: alter sb_wptr  so_snd = %R[sbuf]\n", __PRETTY_FUNCTION__, sb));
    }
    STAM_PROFILE_STOP(&pData->StatIOread, a);
    SOCKET_UNLOCK(so);
    return nn;
}
#else /* VBOX_WITH_SLIRP_BSD_SBUF */
int
soread(PNATState pData, struct socket *so)
{
    int n;
    char *buf;
    struct sbuf *sb = &so->so_snd;
    size_t len = sbspace(sb);
    int mss = so->so_tcpcb->t_maxseg;

    STAM_PROFILE_START(&pData->StatIOread, a);
    STAM_COUNTER_RESET(&pData->StatIORead_in_1);
    STAM_COUNTER_RESET(&pData->StatIORead_in_2);

    QSOCKET_LOCK(tcb);
    SOCKET_LOCK(so);
    QSOCKET_UNLOCK(tcb);

    LogFlowFunc(("soread: so = %lx\n", (long)so));

    if (len > mss)
        len -= len % mss;
    buf = RTMemAlloc(len);
    if (buf == NULL)
    {
        Log(("NAT: can't alloc enough memory\n"));
        return -1;
    }

    n = recv(so->s, buf, len, (so->so_tcpcb->t_force? MSG_OOB:0));
    if (n <= 0)
    {
        /*
         * Special case for WSAEnumNetworkEvents: If we receive 0 bytes that
         * _could_ mean that the connection is closed. But we will receive an
         * FD_CLOSE event later if the connection was _really_ closed. With
         * www.youtube.com I see this very often. Closing the socket too early
         * would be dangerous.
         */
        int status;
        unsigned long pending = 0;
        status = ioctlsocket(so->s, FIONREAD, &pending);
        if (status < 0)
            Log(("NAT:error in WSAIoctl: %d\n", errno));
        if (n == 0 && (pending != 0))
        {
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOread, a);
            RTMemFree(buf);
            return 0;
        }
        if (   n < 0
            && (   errno == EINTR
                || errno == EAGAIN
                || errno == EWOULDBLOCK))
        {
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOread, a);
            RTMemFree(buf);
            return 0;
        }
        else
        {
            Log2((" --- soread() disconnected, n = %d, errno = %d (%s)\n",
                  n, errno, strerror(errno)));
            sofcantrcvmore(so);
            tcp_sockclosed(pData, sototcpcb(so));
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOread, a);
            RTMemFree(buf);
            return -1;
        }
    }

    sbuf_bcat(sb, buf, n);
    RTMemFree(buf);
    return n;
}
#endif

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
    ssize_t ret;

    LogFlowFunc(("sorecvoob: so = %R[natsock]\n", so));

    /*
     * We take a guess at how much urgent data has arrived.
     * In most situations, when urgent data arrives, the next
     * read() should get all the urgent data.  This guess will
     * be wrong however if more data arrives just after the
     * urgent data, or the read() doesn't return all the
     * urgent data.
     */
    ret = soread(pData, so);
    if (RT_LIKELY(ret > 0))
    {
        tp->snd_up = tp->snd_una + SBUF_LEN(&so->so_snd);
        tp->t_force = 1;
        tcp_output(pData, tp);
        tp->t_force = 0;
    }
}
#ifndef VBOX_WITH_SLIRP_BSD_SBUF
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

    LogFlowFunc(("sosendoob so = %R[natsock]\n", so));

    if (so->so_urgc > sizeof(buff))
        so->so_urgc = sizeof(buff); /* XXX */

    if (sb->sb_rptr < sb->sb_wptr)
    {
        /* We can send it directly */
        n = send(so->s, sb->sb_rptr, so->so_urgc, (MSG_OOB)); /* |MSG_DONTWAIT)); */
        so->so_urgc -= n;

        Log2((" --- sent %d bytes urgent data, %d urgent bytes left\n",
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
            Log(("Didn't send all data urgently XXXXX\n"));
#endif
        Log2((" ---2 sent %d bytes urgent data, %d urgent bytes left\n",
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

    STAM_PROFILE_START(&pData->StatIOwrite, a);
    STAM_COUNTER_RESET(&pData->StatIOWrite_in_1);
    STAM_COUNTER_RESET(&pData->StatIOWrite_in_1_bytes);
    STAM_COUNTER_RESET(&pData->StatIOWrite_in_2);
    STAM_COUNTER_RESET(&pData->StatIOWrite_in_2_1st_bytes);
    STAM_COUNTER_RESET(&pData->StatIOWrite_in_2_2nd_bytes);
    STAM_COUNTER_RESET(&pData->StatIOWrite_no_w);
    STAM_COUNTER_RESET(&pData->StatIOWrite_rest);
    STAM_COUNTER_RESET(&pData->StatIOWrite_rest_bytes);
    LogFlowFunc(("so = %R[natsock]\n", so));
    Log2(("%s: so = %R[natsock] so->so_rcv = %R[sbuf]\n", __PRETTY_FUNCTION__, so, sb));
    QSOCKET_LOCK(tcb);
    SOCKET_LOCK(so);
    QSOCKET_UNLOCK(tcb);
    if (so->so_urgc)
    {
        sosendoob(so);
        if (sb->sb_cc == 0)
        {
            SOCKET_UNLOCK(so);
            STAM_PROFILE_STOP(&pData->StatIOwrite, a);
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
    STAM_STATS({
        if (n == 1)
        {
            STAM_COUNTER_INC(&pData->StatIOWrite_in_1);
            STAM_COUNTER_ADD(&pData->StatIOWrite_in_1_bytes, iov[0].iov_len);
        }
        else
        {
            STAM_COUNTER_INC(&pData->StatIOWrite_in_2);
            STAM_COUNTER_ADD(&pData->StatIOWrite_in_2_1st_bytes, iov[0].iov_len);
            STAM_COUNTER_ADD(&pData->StatIOWrite_in_2_2nd_bytes, iov[1].iov_len);
        }
    });
    /* Check if there's urgent data to send, and if so, send it */
#ifdef HAVE_READV
    nn = writev(so->s, (const struct iovec *)iov, n);
#else
    nn = send(so->s, iov[0].iov_base, iov[0].iov_len, 0);
#endif
    Log2(("%s: wrote(1) nn = %d bytes\n", __PRETTY_FUNCTION__, nn));
    /* This should never happen, but people tell me it does *shrug* */
    if (   nn < 0
        && (   errno == EAGAIN
            || errno == EINTR
            || errno == EWOULDBLOCK))
    {
        SOCKET_UNLOCK(so);
        STAM_PROFILE_STOP(&pData->StatIOwrite, a);
        return 0;
    }

    if (nn < 0 || (nn == 0 && iov[0].iov_len > 0))
    {
        Log2(("%s: disconnected, so->so_state = %x, errno = %d\n",
              __PRETTY_FUNCTION__, so->so_state, errno));
        sofcantsendmore(so);
        tcp_sockclosed(pData, sototcpcb(so));
        SOCKET_UNLOCK(so);
        STAM_PROFILE_STOP(&pData->StatIOwrite, a);
        return -1;
    }

#ifndef HAVE_READV
    if (n == 2 && nn == iov[0].iov_len)
    {
        int ret;
        ret = send(so->s, iov[1].iov_base, iov[1].iov_len, 0);
        if (ret > 0)
            nn += ret;
        STAM_STATS({
            if (ret > 0 && ret != iov[1].iov_len)
            {
                STAM_COUNTER_INC(&pData->StatIOWrite_rest);
                STAM_COUNTER_ADD(&pData->StatIOWrite_rest_bytes, (iov[1].iov_len - ret));
            }
        });
    }
    Log2(("%s: wrote(2) nn = %d bytes\n", __PRETTY_FUNCTION__, nn));
#endif

    /* Update sbuf */
    sb->sb_cc -= nn;
    sb->sb_rptr += nn;
    Log2(("%s: update so_rcv (written nn = %d) %R[sbuf]\n", __PRETTY_FUNCTION__, nn, sb));
    if (sb->sb_rptr >= (sb->sb_data + sb->sb_datalen))
    {
        sb->sb_rptr -= sb->sb_datalen;
        Log2(("%s: alter sb_rptr of so_rcv %R[sbuf]\n", __PRETTY_FUNCTION__, sb));
    }

    /*
     * If in DRAIN mode, and there's no more data, set
     * it CANTSENDMORE
     */
    if ((so->so_state & SS_FWDRAIN) && sb->sb_cc == 0)
        sofcantsendmore(so);

    SOCKET_UNLOCK(so);
    STAM_PROFILE_STOP(&pData->StatIOwrite, a);
    return nn;
}
#else /* VBOX_WITH_SLIRP_BSD_SBUF */
static int
do_sosend(struct socket *so, int fUrg)
{
    struct sbuf *sb = &so->so_rcv;

    int n, len;

    LogFlowFunc(("sosendoob: so = %R[natsock]\n", so));

    len = sbuf_len(sb);

    n = send(so->s, sbuf_data(sb), len, (fUrg ? MSG_OOB : 0));
    if (n < 0)
        Log(("NAT: Can't sent sbuf via socket.\n"));
    if (fUrg)
        so->so_urgc -= n;
    if (n > 0 && n < len)
    {
        char *ptr;
        char *buff;
        buff = RTMemAlloc(len);
        if (buff == NULL)
        {
            Log(("NAT: No space to allocate temporal buffer\n"));
            return -1;
        }
        ptr = sbuf_data(sb);
        memcpy(buff, &ptr[n], len - n);
        sbuf_bcpy(sb, buff, len - n);
        RTMemFree(buff);
        return n;
    }
    sbuf_clear(sb);
    return n;
}
int
sosendoob(struct socket *so)
{
    return do_sosend(so, 1);
}

/*
 * Write data from so_rcv to so's socket,
 * updating all sbuf field as necessary
 */
int
sowrite(PNATState pData, struct socket *so)
{
    return do_sosend(so, 0);
}
#endif

/*
 * recvfrom() a UDP socket
 */
void
sorecvfrom(PNATState pData, struct socket *so)
{
    ssize_t ret = 0;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);

    LogFlowFunc(("sorecvfrom: so = %lx\n", (long)so));

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
        ssize_t len;
        u_long n = 0;
        int rc = 0;
        static int signalled = 0;
        char *pchBuffer = NULL;
        bool fWithTemporalBuffer = false;

        QSOCKET_LOCK(udb);
        SOCKET_LOCK(so);
        QSOCKET_UNLOCK(udb);

        /*How many data has been received ?*/
        /*
        * 1. calculate how much we can read
        * 2. read as much as possible
        * 3. attach buffer to allocated header mbuf
        */
        rc = ioctlsocket(so->s, FIONREAD, &n);
        if (rc == -1)
        {
            if (  errno == EAGAIN
               || errno == EWOULDBLOCK
               || errno == EINPROGRESS
               || errno == ENOTCONN)
                return;
            else if (signalled == 0)
            {
                LogRel(("NAT: can't fetch amount of bytes on socket %R[natsock], so message will be truncated.\n", so));
                signalled = 1;
            }
            return;
        }

        len = sizeof(struct udpiphdr);
        m = m_getjcl(pData, M_NOWAIT, MT_HEADER, M_PKTHDR, slirp_size(pData));
        if (m == NULL)
            return;

        len += n;
        m->m_data += ETH_HLEN;
        m->m_pkthdr.header = mtod(m, void *);
        m->m_data += sizeof(struct udpiphdr);

        pchBuffer = mtod(m, char *);
        fWithTemporalBuffer = false;
        /*
         * Even if amounts of bytes on socket is greater than MTU value
         * Slirp will able fragment it, but we won't create temporal location
         * here.
         */
        if (n > (slirp_size(pData) - sizeof(struct udpiphdr)))
        {
            pchBuffer = RTMemAlloc((n) * sizeof(char));
            if (!pchBuffer)
            {
                m_freem(pData, m);
                return;
            }
            fWithTemporalBuffer = true;
        }
        ret = recvfrom(so->s, pchBuffer, n, 0,
                       (struct sockaddr *)&addr, &addrlen);
        if (fWithTemporalBuffer)
        {
            if (ret > 0)
            {
                m_copyback(pData, m, 0, ret, pchBuffer);
                /*
                 * If we've met comporison below our size prediction was failed
                 * it's not fatal just we've allocated for nothing. (@todo add counter here
                 * to calculate how rare we here)
                 */
                if(ret < slirp_size(pData) && !m->m_next)
                    Log(("NAT:udp: Expected size(%d) lesser than real(%d) and less minimal mbuf size(%d)\n",
                         n, ret, slirp_size(pData)));
            }
            /* we're freeing buffer anyway */
            RTMemFree(pchBuffer);
        }
        else
            m->m_len = ret;

        if (ret < 0)
        {
            u_char code = ICMP_UNREACH_PORT;

            if (errno == EHOSTUNREACH)
                code = ICMP_UNREACH_HOST;
            else if (errno == ENETUNREACH)
                code = ICMP_UNREACH_NET;

            m_freem(pData, m);
            if (   errno == EAGAIN
                || errno == EWOULDBLOCK
                || errno == EINPROGRESS
                || errno == ENOTCONN)
            {
                return;
            }

            Log2((" rx error, tx icmp ICMP_UNREACH:%i\n", code));
            icmp_error(pData, so->so_m, ICMP_UNREACH, code, 0, strerror(errno));
            so->so_m = NULL;
        }
        else
        {
            Assert((m_length(m,NULL) == ret));
            /*
             * Hack: domain name lookup will be used the most for UDP,
             * and since they'll only be used once there's no need
             * for the 4 minute (or whatever) timeout... So we time them
             * out much quicker (10 seconds  for now...)
             */
            if (so->so_expire)
            {
                if (so->so_fport != RT_H2N_U16_C(53))
                    so->so_expire = curtime + SO_EXPIRE;
            }
            /*
             *  last argument should be changed if Slirp will inject IP attributes
             *  Note: Here we can't check if dnsproxy's sent initial request
             */
            if (   pData->fUseDnsProxy
                && so->so_fport == RT_H2N_U16_C(53))
                dnsproxy_answer(pData, so, m);

#if 0
            if (m->m_len == len)
            {
                m_inc(m, MINCSIZE);
                m->m_len = 0;
            }
#endif

            /* packets definetly will be fragmented, could confuse receiver peer. */
            if (m_length(m, NULL) > if_mtu)
                m->m_flags |= M_SKIP_FIREWALL;
            /*
             * If this packet was destined for CTL_ADDR,
             * make it look like that's where it came from, done by udp_output
             */
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
    struct sockaddr_in *paddr;
    struct sockaddr addr;
#if 0
    struct sockaddr_in host_addr;
#endif
    caddr_t buf = 0;
    int mlen;

    LogFlowFunc(("sosendto: so = %R[natsock], m = %lx\n", so, (long)m));

    memset(&addr, 0, sizeof(struct sockaddr));
#ifdef RT_OS_DARWIN
    addr.sa_len = sizeof(struct sockaddr_in);
#endif
    paddr = (struct sockaddr_in *)&addr;
    paddr->sin_family = AF_INET;
    if ((so->so_faddr.s_addr & RT_H2N_U32(pData->netmask)) == pData->special_addr.s_addr)
    {
        /* It's an alias */
        uint32_t last_byte = RT_N2H_U32(so->so_faddr.s_addr) & ~pData->netmask;
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
            case CTL_ALIAS:
            default:
                if (last_byte == ~pData->netmask)
                    paddr->sin_addr.s_addr = INADDR_BROADCAST;
                else
                    paddr->sin_addr = loopback_addr;
                break;
        }
    }
    else
        paddr->sin_addr = so->so_faddr;
    paddr->sin_port = so->so_fport;

    Log2((" sendto()ing, addr.sin_port=%d, addr.sin_addr.s_addr=%.16s\n",
          RT_N2H_U16(paddr->sin_port), inet_ntoa(paddr->sin_addr)));

    /* Don't care what port we get */
    /*
     * > nmap -sV -T4 -O -A -v -PU3483 255.255.255.255
     * generates bodyless messages, annoying memmory management system.
     */
    mlen = m_length(m, NULL);
    if (mlen > 0)
    {
        buf = RTMemAlloc(mlen);
        if (buf == NULL)
        {
            return -1;
        }
        m_copydata(m, 0, mlen, buf);
    }
    ret = sendto(so->s, buf, mlen, 0,
                 (struct sockaddr *)&addr, sizeof (struct sockaddr));
#ifdef VBOX_WITH_NAT_SEND2HOME
    if (slirpIsWideCasting(pData, so->so_faddr.s_addr))
    {
        slirpSend2Home(pData, so, buf, mlen, 0);
    }
#endif
    if (buf)
        RTMemFree(buf);
    if (ret < 0)
    {
        Log2(("UDP: sendto fails (%s)\n", strerror(errno)));
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
solisten(PNATState pData, u_int32_t bind_addr, u_int port, u_int32_t laddr, u_int lport, int flags)
{
    struct sockaddr_in addr;
    struct socket *so;
    socklen_t addrlen = sizeof(addr);
    int s, opt = 1;
    int status;

    LogFlowFunc(("solisten: port = %d, laddr = %x, lport = %d, flags = %x\n", port, laddr, lport, flags));

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

    memset(&addr, 0, sizeof(addr));
#ifdef RT_OS_DARWIN
    addr.sin_len = sizeof(addr);
#endif
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = bind_addr;
    addr.sin_port = port;

    /**
     * changing listen(,1->SOMAXCONN) shouldn't be harmful for NAT's TCP/IP stack,
     * kernel will choose the optimal value for requests queue length.
     * @note: MSDN recommends low (2-4) values for bluetooth networking devices.
     */
    if (   ((s = socket(AF_INET, SOCK_STREAM, 0)) < 0)
        || (setsockopt(s, SOL_SOCKET, SO_REUSEADDR,(char *)&opt, sizeof(int)) < 0)
        || (bind(s,(struct sockaddr *)&addr, sizeof(addr)) < 0)
        || (listen(s, pData->soMaxConn) < 0))
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
    fd_nonblock(s);
    setsockopt(s, SOL_SOCKET, SO_OOBINLINE,(char *)&opt, sizeof(int));

    getsockname(s,(struct sockaddr *)&addr,&addrlen);
    so->so_fport = addr.sin_port;
    /* set socket buffers */
    opt = pData->socket_rcv;
    status = setsockopt(s, SOL_SOCKET, SO_RCVBUF, (char *)&opt, sizeof(int));
    if (status < 0)
    {
        LogRel(("NAT: Error(%d) while setting RCV capacity to (%d)\n", errno, opt));
        goto no_sockopt;
    }
    opt = pData->socket_snd;
    status = setsockopt(s, SOL_SOCKET, SO_SNDBUF, (char *)&opt, sizeof(int));
    if (status < 0)
    {
        LogRel(("NAT: Error(%d) while setting SND capacity to (%d)\n", errno, opt));
        goto no_sockopt;
    }
no_sockopt:
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
 * @todo do we really need this function, what it's intended to do?
 */
void
sorwakeup(struct socket *so)
{
    NOREF(so);
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
    NOREF(so);
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
    LogFlowFunc(("ENTER: so:%R[natsock]\n", so));
    so->so_state &= ~(SS_ISFCONNECTING|SS_FWDRAIN|SS_NOFDREF);
    so->so_state |= SS_ISFCONNECTED; /* Clobber other states */
    LogFlowFunc(("LEAVE: so:%R[natsock]\n", so));
}

void
sofcantrcvmore(struct  socket *so)
{
    LogFlowFunc(("ENTER: so:%R[natsock]\n", so));
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
    LogFlowFuncLeave();
}

void
sofcantsendmore(struct socket *so)
{
    LogFlowFunc(("ENTER: so:%R[natsock]\n", so));
    if ((so->so_state & SS_NOFDREF) == 0)
        shutdown(so->s, 1);           /* send FIN to fhost */

    so->so_state &= ~(SS_ISFCONNECTING);
    if (so->so_state & SS_FCANTRCVMORE)
        so->so_state = SS_NOFDREF; /* as above */
    else
        so->so_state |= SS_FCANTSENDMORE;
    LogFlowFuncLeave();
}

void
soisfdisconnected(struct socket *so)
{
    NOREF(so);
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
    if (SBUF_LEN(&so->so_rcv))
        so->so_state |= SS_FWDRAIN;
    else
        sofcantsendmore(so);
}

static void
send_icmp_to_guest(PNATState pData, char *buff, size_t len, const struct sockaddr_in *addr)
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
    /* Fix ip->ip_len to  contain the total packet length including the header
     * in _host_ byte order for all OSes. On Darwin, that value already is in
     * host byte order. Solaris and Darwin report only the payload. */
#ifndef RT_OS_DARWIN
    ip->ip_len = RT_N2H_U16(ip->ip_len);
#endif
    hlen = (ip->ip_hl << 2);
#if defined(RT_OS_SOLARIS) || defined(RT_OS_DARWIN)
    ip->ip_len += hlen;
#endif
    if (ip->ip_len < hlen + ICMP_MINLEN)
    {
       Log(("send_icmp_to_guest: ICMP header is too small to understand which type/subtype of the datagram\n"));
       return;
    }
    icp = (struct icmp *)((char *)ip + hlen);

    Log(("ICMP:received msg(t:%d, c:%d)\n", icp->icmp_type, icp->icmp_code));
    if (   icp->icmp_type != ICMP_ECHOREPLY
        && icp->icmp_type != ICMP_TIMXCEED
        && icp->icmp_type != ICMP_UNREACH)
    {
        return;
    }

    /*
     * ICMP_ECHOREPLY, ICMP_TIMXCEED, ICMP_UNREACH minimal header size is
     * ICMP_ECHOREPLY assuming data 0
     * icmp_{type(8), code(8), cksum(16),identifier(16),seqnum(16)}
     */
    if (ip->ip_len < hlen + 8)
    {
        Log(("send_icmp_to_guest: NAT accept ICMP_{ECHOREPLY, TIMXCEED, UNREACH} the minimum size is 64 (see rfc792)\n"));
        return;
    }

    type = icp->icmp_type;
    if (   type == ICMP_TIMXCEED
        || type == ICMP_UNREACH)
    {
        /*
         * ICMP_TIMXCEED, ICMP_UNREACH minimal header size is
         * icmp_{type(8), code(8), cksum(16),unused(32)} + IP header + 64 bit of original datagram
         */
        if (ip->ip_len < hlen + 2*8 + sizeof(struct ip))
        {
            Log(("send_icmp_to_guest: NAT accept ICMP_{TIMXCEED, UNREACH} the minimum size of ipheader + 64 bit of data (see rfc792)\n"));
            return;
        }
        ip = &icp->icmp_ip;
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
    if (type == ICMP_ECHOREPLY)
    {
        struct ip *ip0 = mtod(m, struct ip *);
        struct icmp *icp0 = (struct icmp *)((char *)ip0 + (ip0->ip_hl << 2));
        if (icp0->icmp_type != ICMP_ECHO)
        {
            Log(("NAT: we haven't found echo for this reply\n"));
            return;
        }
        /*
         * while combining buffer to send (see ip_icmp.c) we control ICMP header only,
         * IP header combined by OS network stack, our local copy of IP header contians values
         * in host byte order so no byte order conversion is required. IP headers fields are converting
         * in ip_output0 routine only.
         */
        if (   (ip->ip_len - hlen)
            != (ip0->ip_len - (ip0->ip_hl << 2)))
        {
            Log(("NAT: ECHO(%d) lenght doesn't match ECHOREPLY(%d)\n",
                (ip->ip_len - hlen), (ip0->ip_len - (ip0->ip_hl << 2))));
            return;
        }
    }

    /* ip points on origianal ip header */
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
    m_copyback(pData, m, original_hlen, len - hlen, buff + hlen);
    ip->ip_len = m_length(m, NULL);
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
    pData->cIcmpCacheSize--;
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
    int nbytes = 0;
    u_char code = ~0;
    int out_len;
    int size;

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
        LogFunc(("icr[%d] Data:%p, DataSize:%d\n",
                 i, icr[i].Data, icr[i].DataSize));
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
                out_len = ETH_HLEN + sizeof(struct ip) +  8;
                size;
                size = MCLBYTES;
                if (out_len < MSIZE)
                    size = MCLBYTES;
                else if (out_len < MCLBYTES)
                    size = MCLBYTES;
                else if (out_len < MJUM9BYTES)
                    size = MJUM9BYTES;
                else if (out_len < MJUM16BYTES)
                    size = MJUM16BYTES;
                else
                    AssertMsgFailed(("Unsupported size"));

                m = m_getjcl(pData, M_NOWAIT, MT_HEADER, M_PKTHDR, size);
                LogFunc(("m_getjcl returns m: %p\n", m));
                if (m == NULL)
                    return;
                m->m_len = 0;
                m->m_data += if_maxlinkhdr;
                m->m_pkthdr.header = mtod(m, void *);

                ip = mtod(m, struct ip *);
                ip->ip_src.s_addr = icr[i].Address;
                ip->ip_p = IPPROTO_ICMP;
                ip->ip_dst.s_addr = so->so_laddr.s_addr; /*XXX: still the hack*/
                ip->ip_hl =  sizeof(struct ip) >> 2; /* requiered for icmp_reflect, no IP options */
                ip->ip_ttl = icr[i].Options.Ttl;

                icp = (struct icmp *)&ip[1]; /* no options */
                icp->icmp_type = ICMP_ECHOREPLY;
                icp->icmp_code = 0;
                icp->icmp_id = so->so_icmp_id;
                icp->icmp_seq = so->so_icmp_seq;

                icm = icmp_find_original_mbuf(pData, ip);
                if (icm)
                {
                    /* on this branch we don't need stored variant */
                    m_freem(pData, icm->im_m);
                    LIST_REMOVE(icm, im_list);
                    pData->cIcmpCacheSize--;
                    RTMemFree(icm);
                }


                hlen = (ip->ip_hl << 2);
                Assert((hlen >= sizeof(struct ip)));

                m->m_data += hlen + ICMP_MINLEN;
                if (!RT_VALID_PTR(icr[i].Data))
                {
                    m_freem(pData, m);
                    break;
                }
                m_copyback(pData, m, 0, icr[i].DataSize, icr[i].Data);
                m->m_data -= hlen + ICMP_MINLEN;
                m->m_len += hlen + ICMP_MINLEN;


                ip->ip_len = m_length(m, NULL);
                Assert((ip->ip_len == hlen + ICMP_MINLEN + icr[i].DataSize));

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
                Assert(((ip_broken->ip_hl >> 2) >= sizeof(struct ip)));
                ip->ip_ttl = icr[i].Options.Ttl;
                src = ip->ip_src.s_addr;
                ip->ip_dst.s_addr = src;
                ip->ip_dst.s_addr = icr[i].Address;

                hlen = (ip->ip_hl << 2);
                icp = (struct icmp *)((char *)ip + hlen);
                ip_broken->ip_src.s_addr = src; /*it packet sent from host not from guest*/

                m->m_len = (ip_broken->ip_hl << 2) + 64;
                m->m_pkthdr.header = mtod(m, void *);
                m_copyback(pData, m, ip->ip_hl >> 2, icr[i].DataSize, icr[i].Data);
                icmp_reflect(pData, m);
                /* Here is different situation from Unix world, where we can receive icmp in response on TCP/UDP */
                LIST_REMOVE(icm, im_list);
                pData->cIcmpCacheSize--;
                RTMemFree(icm);
                break;
            default:
                Log(("ICMP(default): message with Status: %x was received from %x\n", icr[i].Status, icr[i].Address));
                break;
        }
    }
}
#else /* !RT_OS_WINDOWS */
static void sorecvfrom_icmp_unix(PNATState pData, struct socket *so)
{
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(struct sockaddr_in);
    struct ip ip;
    char *buff;
    int len = 0;

    /* 1- step: read the ip header */
    len = recvfrom(so->s, &ip, sizeof(struct ip), MSG_PEEK,
                   (struct sockaddr *)&addr, &addrlen);
    if (   len < 0
        && (   errno == EAGAIN
            || errno == EWOULDBLOCK
            || errno == EINPROGRESS
            || errno == ENOTCONN))
    {
        Log(("sorecvfrom_icmp_unix: 1 - step can't read IP datagramm (would block)\n"));
        return;
    }

    if (   len < sizeof(struct ip)
        || len < 0
        || len == 0)
    {
        u_char code;
        code = ICMP_UNREACH_PORT;

        if (errno == EHOSTUNREACH)
            code = ICMP_UNREACH_HOST;
        else if (errno == ENETUNREACH)
            code = ICMP_UNREACH_NET;

        LogRel((" udp icmp rx errno = %d (%s)\n", errno, strerror(errno)));
        icmp_error(pData, so->so_m, ICMP_UNREACH, code, 0, strerror(errno));
        so->so_m = NULL;
        Log(("sorecvfrom_icmp_unix: 1 - step can't read IP datagramm\n"));
        return;
    }
    /* basic check of IP header */
    if (   ip.ip_v != IPVERSION
# ifndef RT_OS_DARWIN
        || ip.ip_p != IPPROTO_ICMP
# endif
        )
    {
        Log(("sorecvfrom_icmp_unix: 1 - step IP isn't IPv4\n"));
        return;
    }
# ifndef RT_OS_DARWIN
    /* Darwin reports the IP length already in host byte order. */
    ip.ip_len = RT_N2H_U16(ip.ip_len);
# endif
# if defined(RT_OS_SOLARIS) || defined(RT_OS_DARWIN)
    /* Solaris and Darwin report the payload only */
    ip.ip_len += (ip.ip_hl << 2);
# endif
    /* Note: ip->ip_len in host byte order (all OS) */
    len = ip.ip_len;
    buff = RTMemAlloc(len);
    if (buff == NULL)
    {
        Log(("sorecvfrom_icmp_unix: 1 - step can't allocate enought room for datagram\n"));
        return;
    }
    /* 2 - step: we're reading rest of the datagramm to the buffer */
    addrlen = sizeof(struct sockaddr_in);
    memset(&addr, 0, addrlen);
    len = recvfrom(so->s, buff, len, 0,
                   (struct sockaddr *)&addr, &addrlen);
    if (   len < 0
        && (   errno == EAGAIN
            || errno == EWOULDBLOCK
            || errno == EINPROGRESS
            || errno == ENOTCONN))
    {
        Log(("sorecvfrom_icmp_unix: 2 - step can't read IP body (would block expected:%d)\n",
            ip.ip_len));
        RTMemFree(buff);
        return;
    }
    if (   len < 0
        || len == 0)
    {
        Log(("sorecvfrom_icmp_unix: 2 - step read of the rest of datagramm is fallen (errno:%d, len:%d expected: %d)\n",
             errno, len, (ip.ip_len - sizeof(struct ip))));
        RTMemFree(buff);
        return;
    }
    /* len is modified in 2nd read, when the rest of the datagramm was read */
    send_icmp_to_guest(pData, buff, len, &addr);
    RTMemFree(buff);
}
#endif /* !RT_OS_WINDOWS */
