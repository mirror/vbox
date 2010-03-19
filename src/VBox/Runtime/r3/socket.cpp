/* $Id$ */
/** @file
 * IPRT - Network Sockets.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#ifdef RT_OS_WINDOWS
//# include <winsock.h>
# include <winsock2.h>
#else /* !RT_OS_WINDOWS */
# include <errno.h>
# include <sys/stat.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <netinet/tcp.h>
# include <arpa/inet.h>
# ifdef IPRT_WITH_TCPIP_V6
#  include <netinet6/in6.h>
# endif
# include <sys/un.h>
# include <netdb.h>
# include <unistd.h>
# include <fcntl.h>
#endif /* !RT_OS_WINDOWS */
#include <limits.h>

#include "internal/iprt.h"
#include <iprt/socket.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include "internal/magics.h"
#include "internal/socket.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* non-standard linux stuff (it seems). */
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL           0
#endif

/* Windows has different names for SHUT_XXX. */
#ifndef SHUT_RDWR
# ifdef SD_BOTH
#  define SHUT_RDWR             SD_BOTH
# else
#  define SHUT_RDWR             2
# endif
#endif
#ifndef SHUT_WR
# ifdef SD_SEND
#  define SHUT_WR               SD_SEND
# else
#  define SHUT_WR               1
# endif
#endif
#ifndef SHUT_RD
# ifdef SD_RECEIVE
#  define SHUT_RD               SD_RECEIVE
# else
#  define SHUT_RD               0
# endif
#endif

/* fixup backlevel OSes. */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define socklen_t              int
#endif

/** How many pending connection. */
#define RTTCP_SERVER_BACKLOG    10


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Socket handle data.
 *
 * This is mainly required for implementing RTPollSet on Windows.
 */
typedef struct RTSOCKETINT
{
    /** Magic number (RTTCPSOCKET_MAGIC). */
    uint32_t            u32Magic;
    /** Usage count.  This is used to prevent two threads from accessing the
     *  handle concurrently. */
    uint32_t volatile   cUsers;
#ifdef RT_OS_WINDOWS
    /** The native socket handle. */
    SOCKET              hNative;
    /** The event semaphore we've associated with the socket handle.
     * This is WSA_INVALID_EVENT if not done. */
    WSAEVENT            hEvent;
    /** The pollset currently polling this socket.  This is NIL if no one is
     * polling. */
    RTPOLLSET           hPollSet;
    /** The events we're polling for. */
    uint32_t            fPollEvts;
    /** The events we're currently subscribing to with WSAEventSelect.
     * This is ZERO if we're currently not subscribing to anything. */
    uint32_t            fSubscribedEvts;
#else
    /** The native socket handle. */
    int                 hNative;
#endif
} RTSOCKETINT;


/**
 * Address union used internally for things like getpeername and getsockname.
 */
typedef union RTSOCKADDRUNION
{
    struct sockaddr     Addr;
    struct sockaddr_in  Ipv4;
#ifdef IPRT_WITH_TCPIP_V6
    struct sockaddr_in6 Ipv6;
#endif
} RTSOCKADDRUNION;


/**
 * Get the last error as an iprt status code.
 *
 * @returns IPRT status code.
 */
DECLINLINE(int) rtSocketError(void)
{
#ifdef RT_OS_WINDOWS
    return RTErrConvertFromWin32(WSAGetLastError());
#else
    return RTErrConvertFromErrno(errno);
#endif
}


/**
 * Resets the last error.
 */
DECLINLINE(void) rtSocketErrorReset(void)
{
#ifdef RT_OS_WINDOWS
    WSASetLastError(0);
#else
    errno = 0;
#endif
}


/**
 * Get the last resolver error as an iprt status code.
 *
 * @returns iprt status code.
 */
int rtSocketResolverError(void)
{
#ifdef RT_OS_WINDOWS
    return RTErrConvertFromWin32(WSAGetLastError());
#else
    switch (h_errno)
    {
        case HOST_NOT_FOUND:
            return VERR_NET_HOST_NOT_FOUND;
        case NO_DATA:
            return VERR_NET_ADDRESS_NOT_AVAILABLE;
        case NO_RECOVERY:
            return VERR_IO_GEN_FAILURE;
        case TRY_AGAIN:
            return VERR_TRY_AGAIN;

        default:
            return VERR_UNRESOLVED_ERROR;
    }
#endif
}


/**
 * Tries to lock the socket for exclusive usage by the calling thread.
 *
 * Call rtSocketUnlock() to unlock.
 *
 * @returns @c true if locked, @c false if not.
 * @param   pThis               The socket structure.
 */
DECLINLINE(bool) rtSocketTryLock(RTSOCKETINT *pThis)
{
    return ASMAtomicCmpXchgU32(&pThis->cUsers, 1, 0);
}


/**
 * Unlocks the socket.
 *
 * @param   pThis               The socket structure.
 */
DECLINLINE(void) rtSocketUnlock(RTSOCKETINT *pThis)
{
    ASMAtomicCmpXchgU32(&pThis->cUsers, 0, 1);
}


/**
 * Creates an IPRT socket handle for a native one.
 *
 * @returns IPRT status code.
 * @param   ppSocket        Where to return the IPRT socket handle.
 * @param   hNative         The native handle.
 */
int rtSocketCreateForNative(RTSOCKETINT **ppSocket,
#ifdef RT_OS_WINDOWS
                            SOCKET hNative
#else
                            int hNative
#endif
                            )
{
    RTSOCKETINT *pThis = (RTSOCKETINT *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->u32Magic         = RTSOCKET_MAGIC;
    pThis->cUsers           = 0;
    pThis->hNative          = hNative;
#ifdef RT_OS_WINDOWS
    pThis->hEvent           = WSA_INVALID_EVENT;
    pThis->hPollSet         = NIL_RTPOLLSET;
    pThis->fPollEvts        = 0;
    pThis->fSubscribedEvts  = 0;
#endif
    *ppSocket = pThis;
    return VINF_SUCCESS;
}


/**
 * Wrapper around socket().
 *
 * @returns IPRT status code.
 * @param   phSocket            Where to store the handle to the socket on
 *                              success.
 * @param   iDomain             The protocol family (PF_XXX).
 * @param   iType               The socket type (SOCK_XXX).
 * @param   iProtocol           Socket parameter, usually 0.
 */
int rtSocketCreate(PRTSOCKET phSocket, int iDomain, int iType, int iProtocol)
{
    /*
     * Create the socket.
     */
#ifdef RT_OS_WINDOWS
    SOCKET  hNative = socket(iDomain, iType, iProtocol);
    if (hNative == INVALID_SOCKET)
        return rtSocketError();
#else
    int     hNative = socket(iDomain, iType, iProtocol);
    if (hNative == -1)
        return rtSocketError();
#endif

    /*
     * Wrap it.
     */
    int rc = rtSocketCreateForNative(phSocket, hNative);
    if (RT_FAILURE(rc))
    {
#ifdef RT_OS_WINDOWS
        closesocket(hNative);
#else
        close(hNative);
#endif
    }
    return rc;
}


RTDECL(int) RTSocketDestroy(RTSOCKET hSocket)
{
    RTSOCKETINT *pThis = hSocket;
    if (pThis == NIL_RTSOCKET)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);

    Assert(pThis->cUsers == 0);
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTSOCKET_MAGIC_DEAD, RTSOCKET_MAGIC), VERR_INVALID_HANDLE);

    /*
     * Do the cleanup.
     */
    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (pThis->hEvent == WSA_INVALID_EVENT)
    {
        WSACloseEvent(pThis->hEvent);
        pThis->hEvent = WSA_INVALID_EVENT;
    }

    if (pThis->hNative != INVALID_SOCKET)
    {
        rc = closesocket(pThis->hNative);
        if (!rc)
            rc = VINF_SUCCESS;
        else
        {
            rc = rtSocketError();
            AssertMsgFailed(("\"%s\": closesocket(%p) -> %Rrc\n", pThis->hNative, rc));
        }
        pThis->hNative = INVALID_SOCKET;
    }

#else
    if (pThis->hNative != -1)
    {
        if (close(pThis->hNative))
        {
            rc = rtSocketError();
            AssertMsgFailed(("\"%s\": close(%d) -> %Rrc\n", pThis->hNative, rc));
        }
        pThis->hNative = -1;
    }
#endif

    return rc;
}


RTDECL(RTHCUINTPTR) RTSocketToNative(RTSOCKET hSocket)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, RTHCUINTPTR_MAX);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, RTHCUINTPTR_MAX);
    return (RTHCUINTPTR)pThis->hNative;
}


RTDECL(int) RTSocketSetInheritance(RTSOCKET hSocket, bool fInheritable)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetHandleInformation((HANDLE)pThis->hNative, HANDLE_FLAG_INHERIT, fInheritable ? HANDLE_FLAG_INHERIT : 0))
        rc = RTErrConvertFromWin32(GetLastError());
#else
    if (fcntl(pThis->hNative, F_SETFD, fInheritable ? 0 : FD_CLOEXEC) < 0)
        rc = RTErrConvertFromErrno(errno);
#endif
    AssertRC(rc); /// @todo remove later.

    rtSocketUnlock(pThis);
    return rc;
}


RTDECL(int) RTSocketRead(RTSOCKET hSocket, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(cbBuffer > 0, VERR_INVALID_PARAMETER);
    AssertPtr(pvBuffer);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Read loop.
     * If pcbRead is NULL we have to fill the entire buffer!
     */
    int     rc       = VINF_SUCCESS;
    size_t  cbRead   = 0;
    size_t  cbToRead = cbBuffer;
    for (;;)
    {
        rtSocketErrorReset();
#ifdef RT_OS_WINDOWS
        int    cbNow = cbToRead >= INT_MAX/2 ? INT_MAX/2 : (int)cbToRead;
#else
        size_t cbNow = cbToRead;
#endif
        ssize_t cbBytesRead = recv(pThis->hNative, (char *)pvBuffer + cbRead, cbNow, MSG_NOSIGNAL);
        if (cbBytesRead <= 0)
        {
            rc = rtSocketError();
            Assert(RT_FAILURE_NP(rc) || cbBytesRead == 0);
            if (RT_SUCCESS_NP(rc))
            {
                if (!pcbRead)
                    rc = VERR_NET_SHUTDOWN;
                else
                {
                    *pcbRead = 0;
                    rc = VINF_SUCCESS;
                }
            }
            break;
        }
        if (pcbRead)
        {
            /* return partial data */
            *pcbRead = cbBytesRead;
            break;
        }

        /* read more? */
        cbRead += cbBytesRead;
        if (cbRead == cbBuffer)
            break;

        /* next */
        cbToRead = cbBuffer - cbRead;
    }

    rtSocketUnlock(pThis);
    return rc;
}


RTDECL(int) RTSocketWrite(RTSOCKET hSocket, const void *pvBuffer, size_t cbBuffer)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Try write all at once.
     */
    int     rc        = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    int     cbNow     = cbBuffer >= INT_MAX / 2 ? INT_MAX / 2 : (int)cbBuffer;
#else
    size_t  cbNow     = cbBuffer >= SSIZE_MAX   ? SSIZE_MAX   :      cbBuffer;
#endif
    ssize_t cbWritten = send(pThis->hNative, (const char *)pvBuffer, cbNow, MSG_NOSIGNAL);
    if (RT_LIKELY((size_t)cbWritten == cbBuffer && cbWritten >= 0))
        rc = VINF_SUCCESS;
    else if (cbWritten < 0)
        rc = rtSocketError();
    else
    {
        /*
         * Unfinished business, write the remainder of the request.  Must ignore
         * VERR_INTERRUPTED here if we've managed to send something.
         */
        size_t cbSentSoFar = 0;
        for (;;)
        {
            /* advance */
            cbBuffer    -= (size_t)cbWritten;
            if (!cbBuffer)
                break;
            cbSentSoFar += (size_t)cbWritten;
            pvBuffer     = (char const *)pvBuffer + cbWritten;

            /* send */
#ifdef RT_OS_WINDOWS
            cbNow = cbBuffer >= INT_MAX / 2 ? INT_MAX / 2 : (int)cbBuffer;
#else
            cbNow = cbBuffer >= SSIZE_MAX   ? SSIZE_MAX   :      cbBuffer;
#endif
            cbWritten = send(pThis->hNative, (const char *)pvBuffer, cbNow, MSG_NOSIGNAL);
            if (cbWritten >= 0)
                AssertMsg(cbBuffer >= (size_t)cbWritten, ("Wrote more than we requested!!! cbWritten=%zu cbBuffer=%zu rtSocketError()=%d\n",
                                                          cbWritten, cbBuffer, rtSocketError()));
            else
            {
                rc = rtSocketError();
                if (rc != VERR_INTERNAL_ERROR || cbSentSoFar == 0)
                    break;
                cbWritten = 0;
                rc = VINF_SUCCESS;
            }
        }
    }

    rtSocketUnlock(pThis);
    return rc;
}


RTDECL(int) RTSocketSelectOne(RTSOCKET hSocket, RTMSINTERVAL cMillies)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Set up the file descriptor sets and do the select.
     */
    fd_set fdsetR;
    FD_ZERO(&fdsetR);
    FD_SET(pThis->hNative, &fdsetR);

    fd_set fdsetE = fdsetR;

    int rc;
    if (cMillies == RT_INDEFINITE_WAIT)
        rc = select(pThis->hNative + 1, &fdsetR, NULL, &fdsetE, NULL);
    else
    {
        struct timeval timeout;
        timeout.tv_sec = cMillies / 1000;
        timeout.tv_usec = (cMillies % 1000) * 1000;
        rc = select(pThis->hNative + 1, &fdsetR, NULL, &fdsetE, &timeout);
    }
    if (rc > 0)
        rc = VINF_SUCCESS;
    else if (rc == 0)
        rc = VERR_TIMEOUT;
    else
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


RTDECL(int) RTSocketShutdown(RTSOCKET hSocket, bool fRead, bool fWrite)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(fRead || fWrite, VERR_INVALID_PARAMETER);
    //AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Do the job.
     */
    int rc = VINF_SUCCESS;
    int fHow;
    if (fRead && fWrite)
        fHow = SHUT_RDWR;
    else if (fRead)
        fHow = SHUT_RD;
    else
        fHow = SHUT_WR;
    if (shutdown(pThis->hNative, fHow) == -1)
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Converts from a native socket address to a generic IPRT network address.
 *
 * @returns IPRT status code.
 * @param   pSrc                The source address.
 * @param   cbSrc               The size of the source address.
 * @param   pAddr               Where to return the generic IPRT network
 *                              address.
 */
static int rtSocketConvertAddress(RTSOCKADDRUNION const *pSrc, size_t cbSrc, PRTNETADDR pAddr)
{
    /*
     * Convert the address.
     */
    if (   cbSrc == sizeof(struct sockaddr_in)
        && pSrc->Addr.sa_family == AF_INET)
    {
        RT_ZERO(*pAddr);
        pAddr->enmType      = RTNETADDRTYPE_IPV4;
        pAddr->uPort        = RT_N2H_U16(pSrc->Ipv4.sin_port);
        pAddr->uAddr.IPv4.u = pSrc->Ipv4.sin_addr.s_addr;
    }
#ifdef IPRT_WITH_TCPIP_V6
    else if (   cbSrc == sizeof(struct sockaddr_in6)
             && pSrc->Addr.sa_family == AF_INET6)
    {
        RT_ZERO(*pAddr);
        pAddr->enmType            = RTNETADDRTYPE_IPV6;
        pAddr->uPort              = RT_N2H_U16(pSrc->Ipv6.sin6_port);
        pAddr->uAddr.IPv6.au32[0] = pSrc->Ipv6.sin6_addr.s6_addr32[0];
        pAddr->uAddr.IPv6.au32[1] = pSrc->Ipv6.sin6_addr.s6_addr32[1];
        pAddr->uAddr.IPv6.au32[2] = pSrc->Ipv6.sin6_addr.s6_addr32[2];
        pAddr->uAddr.IPv6.au32[3] = pSrc->Ipv6.sin6_addr.s6_addr32[3];
    }
#endif
    else
        return VERR_NET_ADDRESS_FAMILY_NOT_SUPPORTED;
    return VINF_SUCCESS;
}


RTDECL(int) RTSocketGetLocalAddress(RTSOCKET hSocket, PRTNETADDR pAddr)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Get the address and convert it.
     */
    int             rc;
    RTSOCKADDRUNION u;
#ifdef RT_OS_WINDOWS
    int             cbAddr = sizeof(u);
#else
    socklen_t       cbAddr = sizeof(u);
#endif
    RT_ZERO(u);
    if (getsockname(pThis->hNative, &u.Addr, &cbAddr) == 0)
        rc = rtSocketConvertAddress(&u, cbAddr, pAddr);
    else
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


RTDECL(int) RTSocketGetPeerAddress(RTSOCKET hSocket, PRTNETADDR pAddr)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Get the address and convert it.
     */
    int             rc;
    RTSOCKADDRUNION u;
#ifdef RT_OS_WINDOWS
    int             cbAddr = sizeof(u);
#else
    socklen_t       cbAddr = sizeof(u);
#endif
    RT_ZERO(u);
    if (getpeername(pThis->hNative, &u.Addr, &cbAddr) == 0)
        rc = rtSocketConvertAddress(&u, cbAddr, pAddr);
    else
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}



/**
 * Wrapper around bind.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   pAddr               The socket address to bind to.
 * @param   cbAddr              The size of the address structure @a pAddr
 *                              points to.
 */
int rtSocketBind(RTSOCKET hSocket, const struct sockaddr *pAddr, int cbAddr)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
    if (bind(pThis->hNative, pAddr, cbAddr) != 0)
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Wrapper around listen.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   cMaxPending         The max number of pending connections.
 */
int rtSocketListen(RTSOCKET hSocket, int cMaxPending)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
    if (listen(pThis->hNative, cMaxPending) != 0)
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Wrapper around accept.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   phClient            Where to return the client socket handle on
 *                              success.
 * @param   pAddr               Where to return the client address.
 * @param   pcbAddr             On input this gives the size buffer size of what
 *                              @a pAddr point to.  On return this contains the
 *                              size of what's stored at @a pAddr.
 */
int rtSocketAccept(RTSOCKET hSocket, PRTSOCKET phClient, struct sockaddr *pAddr, size_t *pcbAddr)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    /*
     * Call accept().
     */
    rtSocketErrorReset();
    int         rc      = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    int         cbAddr  = (int)*pcbAddr;
    SOCKET      hNative = accept(pThis->hNative, pAddr, &cbAddr);
    if (hNative != INVALID_SOCKET)
#else
    socklen_t   cbAddr  = *pcbAddr;
    int         hNative = accept(pThis->hNative, pAddr, &cbAddr);
    if (hNative != -1)
#endif
    {
        *pcbAddr = cbAddr;

        /*
         * Wrap the client socket.
         */
        rc = rtSocketCreateForNative(phClient, hNative);
        if (RT_FAILURE(rc))
        {
#ifdef RT_OS_WINDOWS
            closesocket(hNative);
#else
            close(hNative);
#endif
        }
    }
    else
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Wrapper around connect.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   pAddr               The socket address to connect to.
 * @param   cbAddr              The size of the address structure @a pAddr
 *                              points to.
 */
int rtSocketConnect(RTSOCKET hSocket, const struct sockaddr *pAddr, int cbAddr)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
    if (connect(pThis->hNative, pAddr, cbAddr) != 0)
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Wrapper around setsockopt.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   iLevel              The protocol level, e.g. IPPORTO_TCP.
 * @param   iOption             The option, e.g. TCP_NODELAY.
 * @param   pvValue             The value buffer.
 * @param   cbValue             The size of the value pointed to by pvValue.
 */
int rtSocketSetOpt(RTSOCKET hSocket, int iLevel, int iOption, void const *pvValue, int cbValue)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
    if (setsockopt(pThis->hNative, iLevel, iOption, (const char *)pvValue, cbValue) != 0)
        rc = rtSocketError();

    rtSocketUnlock(pThis);
    return rc;
}

#ifdef RT_OS_WINDOWS

/**
 * Internal RTPollSetAdd helper that returns the handle that should be added to
 * the pollset.
 *
 * @returns Valid handle on success, INVALID_HANDLE_VALUE on failure.
 * @param   hSocket             The socket handle.
 * @param   fEvents             The events we're polling for.
 * @param   ph                  wher to put the primary handle.
 */
int rtSocketPollGetHandle(RTSOCKET hSocket, uint32_t fEvents, PHANDLE ph)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
    if (pThis->hEvent != WSA_INVALID_EVENT)
        *ph = pThis->hEvent;
    else
    {
        *ph = pThis->hEvent = WSACreateEvent();
        if (pThis->hEvent == WSA_INVALID_EVENT)
            rc = rtSocketError();
    }

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Undos the harm done by WSAEventSelect.
 *
 * @returns IPRT status code.
 * @param   pThis               The socket handle.
 */
static int rtSocketPollClearEventAndMakeBlocking(RTSOCKETINT *pThis)
{
    int rc = VINF_SUCCESS;
    if (pThis->fSubscribedEvts)
    {
        if (WSAEventSelect(pThis->hNative, WSA_INVALID_EVENT, 0) == 0)
        {
            pThis->fSubscribedEvts = 0;

            u_long fNonBlocking = 0;
            int rc2 = ioctlsocket(pThis->hNative, FIONBIO, &fNonBlocking);
            if (rc2 != 0)
            {
                rc = rtSocketError();
                AssertMsgFailed(("%Rrc; rc2=%d\n", rc, rc2));
            }
        }
        else
        {
            rc = rtSocketError();
            AssertMsgFailed(("%Rrc\n", rc));
        }
    }
    return rc;
}


/**
 * Updates the mask of events we're subscribing to.
 *
 * @returns IPRT status code.
 * @param   pThis               The socket handle.
 * @param   fEvents             The events we want to subscribe to.
 */
static int rtSocketPollUpdateEvents(RTSOCKETINT *pThis, uint32_t fEvents)
{
    LONG fNetworkEvents = 0;
    if (fEvents & RTPOLL_EVT_READ)
        fNetworkEvents |= FD_READ;
    if (fEvents & RTPOLL_EVT_WRITE)
        fNetworkEvents |= FD_WRITE;
    if (fEvents & RTPOLL_EVT_ERROR)
        fNetworkEvents |= FD_CLOSE;
    if (WSAEventSelect(pThis->hNative, pThis->hEvent, fNetworkEvents) == 0)
    {
        pThis->fSubscribedEvts = fEvents;
        return VINF_SUCCESS;
    }

    int rc = rtSocketError();
    AssertMsgFailed(("fNetworkEvents=%#x rc=%Rrc\n", fNetworkEvents, rtSocketError()));
    return rc;
}


/**
 * Checks for pending events.
 *
 * @returns Event mask or 0.
 * @param   pThis               The socket handle.
 * @param   fEvents             The desired events.
 */
static uint32_t rtSocketPollCheck(RTSOCKETINT *pThis, uint32_t fEvents)
{
    int         rc         = VINF_SUCCESS;
    uint32_t    fRetEvents = 0;

    /* Make sure WSAEnumNetworkEvents returns what we want. */
    if ((pThis->fSubscribedEvts & fEvents) != fEvents)
        rc = rtSocketPollUpdateEvents(pThis, pThis->fSubscribedEvts | fEvents);

    /* Get the event mask, ASSUMES that WSAEnumNetworkEvents doesn't clear stuff.  */
    WSANETWORKEVENTS NetEvts;
    RT_ZERO(NetEvts);
    if (WSAEnumNetworkEvents(pThis->hNative, pThis->hEvent, &NetEvts) == 0)
    {
        if (    (NetEvts.lNetworkEvents & FD_READ)
            &&  (fEvents & RTPOLL_EVT_READ)
            &&  NetEvts.iErrorCode[FD_READ_BIT] == 0)
            fRetEvents |= RTPOLL_EVT_READ;

        if (    (NetEvts.lNetworkEvents & FD_WRITE)
            &&  (fEvents & RTPOLL_EVT_WRITE)
            &&  NetEvts.iErrorCode[FD_WRITE_BIT] == 0)
            fRetEvents |= RTPOLL_EVT_WRITE;

        if (fEvents & RTPOLL_EVT_ERROR)
        {
            if (NetEvts.lNetworkEvents & FD_CLOSE)
                fRetEvents |= RTPOLL_EVT_ERROR;
            else
                for (uint32_t i = 0; i < FD_MAX_EVENTS; i++)
                    if (    (NetEvts.lNetworkEvents & (1L << i))
                        &&  NetEvts.iErrorCode[i] != 0)
                        fRetEvents |= RTPOLL_EVT_ERROR;
        }
    }
    else
        rc = rtSocketError();

    /* Fall back on select if we hit an error above. */
    if (RT_FAILURE(rc))
    {
        /** @todo  */
    }

    return fRetEvents;
}


/**
 * Internal RTPoll helper that polls the socket handle and, if @a fNoWait is
 * clear, starts whatever actions we've got running during the poll call.
 *
 * @returns 0 if no pending events, actions initiated if @a fNoWait is clear.
 *          Event mask (in @a fEvents) and no actions if the handle is ready
 *          already.
 *          UINT32_MAX (asserted) if the socket handle is busy in I/O or a
 *          different poll set.
 *
 * @param   hSocket             The socket handle.
 * @param   hPollSet            The poll set handle (for access checks).
 * @param   fEvents             The events we're polling for.
 * @param   fFinalEntry         Set if this is the final entry for this handle
 *                              in this poll set.  This can be used for dealing
 *                              with duplicate entries.
 * @param   fNoWait             Set if it's a zero-wait poll call.  Clear if
 *                              we'll wait for an event to occur.
 *
 * @remarks There is a potential race wrt duplicate handles when @a fNoWait is
 *          @c true, we don't currently care about that oddity...
 */
uint32_t rtSocketPollStart(RTSOCKET hSocket, RTPOLLSET hPollSet, uint32_t fEvents, bool fFinalEntry, bool fNoWait)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, UINT32_MAX);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, UINT32_MAX);
    if (rtSocketTryLock(pThis))
        pThis->hPollSet = hPollSet;
    else
    {
        AssertReturn(pThis->hPollSet == hPollSet, UINT32_MAX);
        ASMAtomicIncU32(&pThis->cUsers);
    }

    /* (rtSocketPollCheck will reset the event object). */
    uint32_t fRetEvents = rtSocketPollCheck(pThis, fEvents);
    if (   !fRetEvents
        && !fNoWait)
    {
        pThis->fPollEvts |= fEvents;
        if (   fFinalEntry
            && pThis->fSubscribedEvts != pThis->fPollEvts)
        {
            int rc = rtSocketPollUpdateEvents(pThis, pThis->fPollEvts);
            if (RT_FAILURE(rc))
            {
                pThis->fPollEvts = 0;
                fRetEvents       = UINT32_MAX;
            }
        }
    }

    if (fRetEvents || fNoWait)
    {
        if (pThis->cUsers == 1)
        {
            rtSocketPollClearEventAndMakeBlocking(pThis);
            pThis->hPollSet = NIL_RTPOLLSET;
        }
        ASMAtomicDecU32(&pThis->cUsers);
    }

    return fRetEvents;
}


/**
 * Called after a WaitForMultipleObjects returned in order to check for pending
 * events and stop whatever actions that rtSocketPollStart() initiated.
 *
 * @returns Event mask or 0.
 *
 * @param   hSocket             The socket handle.
 * @param   fEvents             The events we're polling for.
 * @param   fFinalEntry         Set if this is the final entry for this handle
 *                              in this poll set.  This can be used for dealing
 *                              with duplicate entries.  Only keep in mind that
 *                              this method is called in reverse order, so the
 *                              first call will have this set (when the entire
 *                              set was processed).
 */
uint32_t rtSocketPollDone(RTSOCKET hSocket, uint32_t fEvents, bool fFinalEntry)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, 0);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, 0);
    Assert(pThis->cUsers > 0);
    Assert(pThis->hPollSet != NIL_RTPOLLSET);

    /* Harvest events and clear the event mask for the next round of polling. */
    uint32_t fRetEvents = rtSocketPollCheck(pThis, fEvents);
    pThis->fPollEvts = 0;

    /* Make the socket blocking again and unlock the handle. */
    if (pThis->cUsers == 1)
    {
        rtSocketPollClearEventAndMakeBlocking(pThis);
        pThis->hPollSet = NIL_RTPOLLSET;
    }
    ASMAtomicDecU32(&pThis->cUsers);
    return fRetEvents;
}

#endif /* RT_OS_WINDOWS */
