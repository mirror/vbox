/* $Id$ */
/** @file
 * IPRT - TCP/IP.
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
# include <winsock.h>
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
#include <iprt/tcp.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mempool.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include "internal/magics.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/* non-standard linux stuff (it seems). */
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL           0
#endif
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
     * This is INVALID_HANDLE_VALUE if not done. */
    WSAEVENT            hEvent;
    /** The pollset currently polling this socket.  This is NIL if no one is
     * polling. */
    RTPOLLSET           hPollSet;
    /** The events we're polling for. */
    uint32_t            fPollEvts;
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
 * TCP Server state.
 */
typedef enum RTTCPSERVERSTATE
{
    /** Invalid. */
    RTTCPSERVERSTATE_INVALID = 0,
    /** Created. */
    RTTCPSERVERSTATE_CREATED,
    /** Listener thread is starting up. */
    RTTCPSERVERSTATE_STARTING,
    /** Accepting client connections. */
    RTTCPSERVERSTATE_ACCEPTING,
    /** Serving a client. */
    RTTCPSERVERSTATE_SERVING,
    /** Listener terminating. */
    RTTCPSERVERSTATE_STOPPING,
    /** Listener terminated. */
    RTTCPSERVERSTATE_STOPPED,
    /** Listener cleans up. */
    RTTCPSERVERSTATE_DESTROYING
} RTTCPSERVERSTATE;

/*
 * Internal representation of the TCP Server handle.
 */
typedef struct RTTCPSERVER
{
    /** The magic value (RTTCPSERVER_MAGIC). */
    uint32_t volatile           u32Magic;
    /** The server state. */
    RTTCPSERVERSTATE volatile   enmState;
    /** The server thread. */
    RTTHREAD                    Thread;
    /** The server socket. */
    RTSOCKET volatile           SockServer;
    /** The socket to the client currently being serviced.
     * This is NIL_RTSOCKET when no client is serviced. */
    RTSOCKET volatile           SockClient;
    /** The connection function. */
    PFNRTTCPSERVE               pfnServe;
    /** Argument to pfnServer. */
    void                       *pvUser;
} RTTCPSERVER;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int)  rtTcpServerThread(RTTHREAD ThreadSelf, void *pvServer);
static int  rtTcpServerListen(PRTTCPSERVER pServer);
static int  rtTcpServerListenCleanup(PRTTCPSERVER pServer);
static int  rtTcpServerDestroySocket(RTSOCKET volatile *pSockClient, const char *pszMsg);
static int  rtTcpClose(RTSOCKET Sock, const char *pszMsg, bool fTryGracefulShutdown);



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
DECLINLINE(int) rtSocketResolverError(void)
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
                            HANDLE hNative
#else
                            int hNative
#endif
                            )
{
    RTSOCKETINT *pThis = (RTSOCKETINT *)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->u32Magic     = RTSOCKET_MAGIC;
    pThis->cUsers       = 0;
    pThis->hNative      = hNative;
#ifdef RT_OS_WINDOWS
    pThis->hEvent       = INVALID_HANDLE_VALUE;
    pThis->hPollSet     = 0;
    pThis->fPollEvts    = 0;
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
static int rtSocketCreate(PRTSOCKET phSocket, int iDomain, int iType, int iProtocol)
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


/**
 * Destroys the specified handle, freeing associated resources and closing the
 * socket.
 *
 * @returns IPRT status code.
 * @param   hSocket         The socket handle.  NIL is ignored.
 *
 * @remarks This will not perform a graceful shutdown of the socket, it will
 *          just destroy it.  Use the protocol specific close method if this is
 *          desired.
 */
int rtSocketDestroy(RTSOCKET hSocket)
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
    if (pThis->hEvent == INVALID_HANDLE_VALUE)
    {
        CloseHandle(pThis->hEvent);
        pThis->hEvent = INVALID_HANDLE_VALUE;
    }

    if (pThis->hNative != INVALID_HANDLE_VALUE)
    {
        rc = closesocket(pThis->hNative);
        if (!rc)
            rc = VINF_SUCCESS;
        else
        {
            rc = rtSocketError();
            AssertMsgFailed(("\"%s\": closesocket(%p) -> %Rrc\n", pThis->hNative, rc));
        }
        pThis->hNative = INVALID_HANDLE_VALUE;
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


/**
 * Gets the native socket handle.
 *
 * @returns The native socket handle or RTHCUINTPTR_MAX if not invalid.
 * @param   hSocket             The socket handle.
 */
RTHCUINTPTR rtSocketNative(RTSOCKET hSocket)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, RTHCUINTPTR_MAX);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, RTHCUINTPTR_MAX);
    return (RTHCUINTPTR)pThis->hNative;
}


/**
 * Helper that ensures the correct inheritability of a socket.
 *
 * We're currently ignoring failures.
 *
 * @returns IPRT status code
 * @param   hSocket         The socket handle.
 * @param   fInheritable    The desired inheritability state.
 */
int rtSocketSetInheritance(RTSOCKET hSocket, bool fInheritable)
{
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

    int rc = VINF_SUCCESS;
#ifdef RT_OS_WINDOWS
    if (!SetHandleInformation(pThis->hNative, HANDLE_FLAG_INHERIT, fInheritable ? HANDLE_FLAG_INHERIT : 0))
        rc = RTErrConvertFromWin32(GetLastError());
#else
    if (fcntl(pThis->hNative, F_SETFD, fInheritable ? 0 : FD_CLOEXEC) < 0)
        rc = RTErrConvertFromErrno(errno);
#endif
    AssertRC(rc); /// @todo remove later.

    rtSocketUnlock(pThis);
    return rc;
}


/**
 * Receive data from a socket.
 *
 * @returns IPRT status code.
 * @param   hSocket         The socket handle.
 * @param   pvBuffer        Where to put the data we read.
 * @param   cbBuffer        Read buffer size.
 * @param   pcbRead         Number of bytes read. If NULL the entire buffer
 *                          will be filled upon successful return. If not NULL a
 *                          partial read can be done successfully.
 */
int rtSocketRead(RTSOCKET hSocket, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
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


/**
 * Send data to a socket.
 *
 * @returns IPRT status code.
 * @retval  VERR_INTERRUPTED if interrupted before anything was written.
 *
 * @param   hSocket         The socket handle.
 * @param   pvBuffer        Buffer to write data to socket.
 * @param   cbBuffer        How much to write.
 */
int rtSocketWrite(RTSOCKET hSocket, const void *pvBuffer, size_t cbBuffer)
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


/**
 * Checks if the socket is ready for reading (for I/O multiplexing).
 *
 * @returns IPRT status code.
 * @param   hSocket         The socket handle.
 * @param   cMillies        Number of milliseconds to wait for the socket.  Use
 *                          RT_INDEFINITE_WAIT to wait for ever.
 */
int rtSocketSelectOne(RTSOCKET hSocket, RTMSINTERVAL cMillies)
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


/**
 * Shuts down one or both directions of communciation.
 *
 * @returns IPRT status code.
 * @param   hSocket             The socket handle.
 * @param   fRead               Whether to shutdown our read direction.
 * @param   fWrite              Whether to shutdown our write direction.
 */
static int rtSocketShutdown(RTSOCKET hSocket, bool fRead, bool fWrite)
{
    /*
     * Validate input.
     */
    RTSOCKETINT *pThis = hSocket;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSOCKET_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(fRead || fWrite, VERR_INVALID_PARAMETER);
    AssertReturn(rtSocketTryLock(pThis), VERR_CONCURRENT_ACCESS);

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


/**
 * Gets the address of the local side.
 *
 * @returns IPRT status code.
 * @param   Sock            Socket descriptor.
 * @param   pAddr           Where to store the local address on success.
 */
int rtSocketGetLocalAddress(RTSOCKET hSocket, PRTNETADDR pAddr)
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


/**
 * Gets the address of the other party.
 *
 * @returns IPRT status code.
 * @param   Sock            Socket descriptor.
 * @param   pAddr           Where to store the peer address on success.
 */
int rtSocketGetPeerAddress(RTSOCKET hSocket, PRTNETADDR pAddr)
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


/////////////////////////////////////////////////////////////////////////////////


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



/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////



/**
 * Atomicly updates a socket variable.
 * @returns The old handle value.
 * @param   phSock          The socket handle variable to update.
 * @param   hSock           The new socket handle value.
 */
DECLINLINE(RTSOCKET) rtTcpAtomicXchgSock(RTSOCKET volatile *phSock, const RTSOCKET hNew)
{
    RTSOCKET hRet;
    ASMAtomicXchgHandle(phSock, hNew, &hRet);
    return hRet;
}


/**
 * Tries to change the TCP server state.
 */
DECLINLINE(bool) rtTcpServerTrySetState(PRTTCPSERVER pServer, RTTCPSERVERSTATE enmStateNew, RTTCPSERVERSTATE enmStateOld)
{
    bool fRc;
    ASMAtomicCmpXchgSize(&pServer->enmState, enmStateNew, enmStateOld, fRc);
    return fRc;
}

/**
 * Changes the TCP server state.
 */
DECLINLINE(void) rtTcpServerSetState(PRTTCPSERVER pServer, RTTCPSERVERSTATE enmStateNew, RTTCPSERVERSTATE enmStateOld)
{
    bool fRc;
    ASMAtomicCmpXchgSize(&pServer->enmState, enmStateNew, enmStateOld, fRc);
    Assert(fRc); NOREF(fRc);
}


/**
 * Closes the a socket (client or server).
 *
 * @returns IPRT status code.
 */
static int rtTcpServerDestroySocket(RTSOCKET volatile *pSock, const char *pszMsg, bool fTryGracefulShutdown)
{
    RTSOCKET hSocket = rtTcpAtomicXchgSock(pSock, NIL_RTSOCKET);
    if (hSocket != NIL_RTSOCKET)
    {
        if (!fTryGracefulShutdown)
            rtSocketShutdown(hSocket, true /*fRead*/, true /*fWrite*/);
        return rtTcpClose(hSocket, pszMsg, fTryGracefulShutdown);
    }
    return VINF_TCP_SERVER_NO_CLIENT;
}


/**
 * Create single connection at a time TCP Server in a separate thread.
 *
 * The thread will loop accepting connections and call pfnServe for
 * each of the incoming connections in turn. The pfnServe function can
 * return VERR_TCP_SERVER_STOP too terminate this loop. RTTcpServerDestroy()
 * should be used to terminate the server.
 *
 * @returns iprt status code.
 * @param   pszAddress      The address for creating a listening socket.
 *                          If NULL or empty string the server is bound to all interfaces.
 * @param   uPort           The port for creating a listening socket.
 * @param   enmType         The thread type.
 * @param   pszThrdName     The name of the worker thread.
 * @param   pfnServe        The function which will serve a new client connection.
 * @param   pvUser          User argument passed to pfnServe.
 * @param   ppServer        Where to store the serverhandle.
 */
RTR3DECL(int)  RTTcpServerCreate(const char *pszAddress, unsigned uPort, RTTHREADTYPE enmType, const char *pszThrdName,
                                 PFNRTTCPSERVE pfnServe, void *pvUser, PPRTTCPSERVER ppServer)
{
    /*
     * Validate input.
     */
    AssertReturn(uPort > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfnServe, VERR_INVALID_POINTER);
    AssertPtrReturn(pszThrdName, VERR_INVALID_POINTER);
    AssertPtrReturn(ppServer, VERR_INVALID_POINTER);

    /*
     * Create the server.
     */
    PRTTCPSERVER pServer;
    int rc = RTTcpServerCreateEx(pszAddress, uPort, &pServer);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the listener thread.
         */
        RTMemPoolRetain(pServer);
        pServer->enmState   = RTTCPSERVERSTATE_STARTING;
        pServer->pvUser     = pvUser;
        pServer->pfnServe   = pfnServe;
        rc = RTThreadCreate(&pServer->Thread, rtTcpServerThread, pServer, 0, enmType, /*RTTHREADFLAGS_WAITABLE*/0, pszThrdName);
        if (RT_SUCCESS(rc))
        {
            /* done */
            if (ppServer)
                *ppServer = pServer;
            else
                RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
            return rc;
        }
        RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);

        /*
         * Destroy the server.
         */
        rtTcpServerSetState(pServer, RTTCPSERVERSTATE_CREATED, RTTCPSERVERSTATE_STARTING);
        RTTcpServerDestroy(pServer);
    }

    return rc;
}


/**
 * Server thread, loops accepting connections until it's terminated.
 *
 * @returns iprt status code. (ignored).
 * @param   ThreadSelf      Thread handle.
 * @param   pvServer        Server handle.
 */
static DECLCALLBACK(int)  rtTcpServerThread(RTTHREAD ThreadSelf, void *pvServer)
{
    PRTTCPSERVER    pServer = (PRTTCPSERVER)pvServer;
    int             rc;
    if (rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_ACCEPTING, RTTCPSERVERSTATE_STARTING))
        rc = rtTcpServerListen(pServer);
    else
        rc = rtTcpServerListenCleanup(pServer);
    RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
    NOREF(ThreadSelf);
    return VINF_SUCCESS;
}


/**
 * Create single connection at a time TCP Server.
 * The caller must call RTTcpServerListen() to actually start the server.
 *
 * @returns iprt status code.
 * @param   pszAddress      The address for creating a listening socket.
 *                          If NULL the server is bound to all interfaces.
 * @param   uPort           The port for creating a listening socket.
 * @param   ppServer        Where to store the serverhandle.
 */
RTR3DECL(int) RTTcpServerCreateEx(const char *pszAddress, uint32_t uPort, PPRTTCPSERVER ppServer)
{
    int rc;

    /*
     * Validate input.
     */
    AssertReturn(uPort > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(ppServer, VERR_INVALID_PARAMETER);

#ifdef RT_OS_WINDOWS
    /*
     * Initialize WinSock and check version.
     */
    WORD    wVersionRequested = MAKEWORD(1, 1);
    WSADATA wsaData;
    rc = WSAStartup(wVersionRequested, &wsaData);
    if (wsaData.wVersion != wVersionRequested)
    {
        AssertMsgFailed(("Wrong winsock version\n"));
        return VERR_NOT_SUPPORTED;
    }
#endif

    /*
     * Get host listening address.
     */
    struct hostent *pHostEnt = NULL;
    if (pszAddress != NULL && *pszAddress)
    {
        pHostEnt = gethostbyname(pszAddress);
        if (!pHostEnt)
        {
            struct in_addr InAddr;
            InAddr.s_addr = inet_addr(pszAddress);
            pHostEnt = gethostbyaddr((char *)&InAddr, 4, AF_INET);
            if (!pHostEnt)
            {
                rc = rtSocketResolverError();
                AssertMsgFailed(("Could not get host address rc=%Rrc\n", rc));
                return rc;
            }
        }
    }

    /*
     * Setting up socket.
     */
    RTSOCKET WaitSock;
    rc = rtSocketCreate(&WaitSock, AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (RT_SUCCESS(rc))
    {
        rtSocketSetInheritance(WaitSock, false /*fInheritable*/);

        /*
         * Set socket options.
         */
        int fFlag = 1;
        if (!rtSocketSetOpt(WaitSock, SOL_SOCKET, SO_REUSEADDR, &fFlag, sizeof(fFlag)))
        {
            /*
             * Set socket family, address and port.
             */
            struct sockaddr_in LocalAddr;
            RT_ZERO(LocalAddr);
            LocalAddr.sin_family = AF_INET;
            LocalAddr.sin_port = htons(uPort);
            /* if address not specified, use INADDR_ANY. */
            if (!pHostEnt)
                LocalAddr.sin_addr.s_addr = INADDR_ANY;
            else
                LocalAddr.sin_addr = *((struct in_addr *)pHostEnt->h_addr);

            /*
             * Bind a name to a socket and set it listening for connections.
             */
            rc = rtSocketBind(WaitSock, (struct sockaddr *)&LocalAddr, sizeof(LocalAddr));
            if (RT_SUCCESS(rc))
                rc = rtSocketListen(WaitSock, RTTCP_SERVER_BACKLOG);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Create the server handle.
                 */
                PRTTCPSERVER pServer = (PRTTCPSERVER)RTMemPoolAlloc(RTMEMPOOL_DEFAULT, sizeof(*pServer));
                if (pServer)
                {
                    pServer->u32Magic   = RTTCPSERVER_MAGIC;
                    pServer->enmState   = RTTCPSERVERSTATE_CREATED;
                    pServer->Thread     = NIL_RTTHREAD;
                    pServer->SockServer = WaitSock;
                    pServer->SockClient = NIL_RTSOCKET;
                    pServer->pfnServe   = NULL;
                    pServer->pvUser     = NULL;
                    *ppServer = pServer;
                    return VINF_SUCCESS;
                }

                /* bail out */
                rc = VERR_NO_MEMORY;
            }
        }
        else
            AssertMsgFailed(("rtSocketSetOpt: %Rrc\n", rc));
        rtTcpClose(WaitSock, "RTServerCreateEx", false /*fTryGracefulShutdown*/);
    }

    return rc;
}


/**
 * Listen for incoming connections.
 *
 * The function will loop accepting connections and call pfnServe for
 * each of the incoming connections in turn. The pfnServe function can
 * return VERR_TCP_SERVER_STOP too terminate this loop. A stopped server
 * can only be destroyed.
 *
 * @returns IPRT status code.
 * @retval  VERR_TCP_SERVER_STOP if stopped by pfnServe.
 * @retval  VERR_TCP_SERVER_SHUTDOWN if shut down by RTTcpServerShutdown.
 *
 * @param   pServer         The server handle as returned from RTTcpServerCreateEx().
 * @param   pfnServe        The function which will serve a new client connection.
 * @param   pvUser          User argument passed to pfnServe.
 */
RTR3DECL(int) RTTcpServerListen(PRTTCPSERVER pServer, PFNRTTCPSERVE pfnServe, void *pvUser)
{
    /*
     * Validate input and retain the instance.
     */
    AssertPtrReturn(pfnServe, VERR_INVALID_POINTER);
    AssertPtrReturn(pServer, VERR_INVALID_HANDLE);
    AssertReturn(pServer->u32Magic == RTTCPSERVER_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(RTMemPoolRetain(pServer) != UINT32_MAX, VERR_INVALID_HANDLE);

    int rc = VERR_INVALID_STATE;
    if (rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_ACCEPTING, RTTCPSERVERSTATE_CREATED))
    {
        Assert(!pServer->pfnServe);
        Assert(!pServer->pvUser);
        Assert(pServer->Thread == NIL_RTTHREAD);
        Assert(pServer->SockClient == NIL_RTSOCKET);

        pServer->pfnServe = pfnServe;
        pServer->pvUser   = pvUser;
        pServer->Thread   = RTThreadSelf();
        Assert(pServer->Thread != NIL_RTTHREAD);
        rc = rtTcpServerListen(pServer);
    }
    else
    {
        AssertMsgFailed(("enmState=%d\n", pServer->enmState));
        rc = VERR_INVALID_STATE;
    }
    RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
    return rc;
}


/**
 * Internal worker common for RTTcpServerListen and the thread created by
 * RTTcpServerCreate().
 *
 * The caller makes sure it has its own memory reference and releases it upon
 * return.
 */
static int rtTcpServerListen(PRTTCPSERVER pServer)
{
    /*
     * Accept connection loop.
     */
    for (;;)
    {
        /*
         * Change state.
         */
        RTTCPSERVERSTATE    enmState   = pServer->enmState;
        RTSOCKET            SockServer = pServer->SockServer;
        if (    enmState != RTTCPSERVERSTATE_ACCEPTING
            &&  enmState != RTTCPSERVERSTATE_SERVING)
            return rtTcpServerListenCleanup(pServer);
        if (!rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_ACCEPTING, enmState))
            continue;

        /*
         * Accept connection.
         */
        struct sockaddr_in  RemoteAddr;
        size_t              cbRemoteAddr = sizeof(RemoteAddr);
        RTSOCKET            Socket;
        RT_ZERO(RemoteAddr);
        int rc = rtSocketAccept(SockServer, &Socket, (struct sockaddr *)&RemoteAddr, &cbRemoteAddr);
        if (RT_FAILURE(rc))
        {
            /* These are typical for what can happen during destruction. */
            if (   rc == VERR_INVALID_HANDLE
                || rc == VERR_INVALID_PARAMETER
                || rc == VERR_NET_NOT_SOCKET)
                return rtTcpServerListenCleanup(pServer);
            continue;
        }
        rtSocketSetInheritance(Socket, false /*fInheritable*/);

        /*
         * Run a pfnServe callback.
         */
        if (!rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_SERVING, RTTCPSERVERSTATE_ACCEPTING))
        {
            rtTcpClose(Socket, "rtTcpServerListen", true /*fTryGracefulShutdown*/);
            return rtTcpServerListenCleanup(pServer);
        }
        rtTcpAtomicXchgSock(&pServer->SockClient, Socket);
        rc = pServer->pfnServe(Socket, pServer->pvUser);
        rtTcpServerDestroySocket(&pServer->SockClient, "Listener: client", true /*fTryGracefulShutdown*/);

        /*
         * Stop the server?
         */
        if (rc == VERR_TCP_SERVER_STOP)
        {
            if (rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_STOPPING, RTTCPSERVERSTATE_SERVING))
            {
                /*
                 * Reset the server socket and change the state to stopped. After that state change
                 * we cannot safely access the handle so we'll have to return here.
                 */
                SockServer = rtTcpAtomicXchgSock(&pServer->SockServer, NIL_RTSOCKET);
                rtTcpServerSetState(pServer, RTTCPSERVERSTATE_STOPPED, RTTCPSERVERSTATE_STOPPING);
                rtTcpClose(SockServer, "Listener: server stopped", false /*fTryGracefulShutdown*/);
            }
            else
                rtTcpServerListenCleanup(pServer); /* ignore rc */
            return rc;
        }
    }
}


/**
 * Clean up after listener.
 */
static int rtTcpServerListenCleanup(PRTTCPSERVER pServer)
{
    /*
     * Close the server socket, the client one shouldn't be set.
     */
    rtTcpServerDestroySocket(&pServer->SockServer, "ListenCleanup", false /*fTryGracefulShutdown*/);
    Assert(pServer->SockClient == NIL_RTSOCKET);

    /*
     * Figure the return code and make sure the state is OK.
     */
    RTTCPSERVERSTATE enmState = pServer->enmState;
    switch (enmState)
    {
        case RTTCPSERVERSTATE_STOPPING:
        case RTTCPSERVERSTATE_STOPPED:
            return VERR_TCP_SERVER_SHUTDOWN;

        case RTTCPSERVERSTATE_ACCEPTING:
            rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_STOPPED, enmState);
            return VERR_TCP_SERVER_DESTROYED;

        case RTTCPSERVERSTATE_DESTROYING:
            return VERR_TCP_SERVER_DESTROYED;

        case RTTCPSERVERSTATE_STARTING:
        case RTTCPSERVERSTATE_SERVING:
        default:
            AssertMsgFailedReturn(("pServer=%p enmState=%d\n", pServer, enmState), VERR_INTERNAL_ERROR_4);
    }
}


/**
 * Listen and accept one incomming connection.
 *
 * This is an alternative to RTTcpServerListen for the use the callbacks are not
 * possible.
 *
 * @returns IPRT status code.
 * @retval  VERR_TCP_SERVER_SHUTDOWN if shut down by RTTcpServerShutdown.
 * @retval  VERR_INTERRUPTED if the listening was interrupted.
 *
 * @param   pServer         The server handle as returned from RTTcpServerCreateEx().
 * @param   pSockClient     Where to return the socket handle to the client
 *                          connection (on success only).  Use
 *                          RTTcpServerDisconnectClient() to clean it, this must
 *                          be done before the next call to RTTcpServerListen2.
 *
 * @todo    This can easily be extended to support multiple connections by
 *          adding a new state and a RTTcpServerDisconnectClient variant for
 *          closing client sockets.
 */
RTR3DECL(int) RTTcpServerListen2(PRTTCPSERVER pServer, PRTSOCKET pSockClient)
{
    /*
     * Validate input and retain the instance.
     */
    AssertPtrReturn(pSockClient, VERR_INVALID_HANDLE);
    *pSockClient = NIL_RTSOCKET;
    AssertReturn(pServer->u32Magic == RTTCPSERVER_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(RTMemPoolRetain(pServer) != UINT32_MAX, VERR_INVALID_HANDLE);

    int rc = VERR_INVALID_STATE;
    for (;;)
    {
        /*
         * Change state to accepting.
         */
        RTTCPSERVERSTATE    enmState   = pServer->enmState;
        RTSOCKET            SockServer = pServer->SockServer;
        if (    enmState != RTTCPSERVERSTATE_SERVING
            &&  enmState != RTTCPSERVERSTATE_CREATED)
        {
            rc = rtTcpServerListenCleanup(pServer);
            break;
        }
        if (!rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_ACCEPTING, enmState))
            continue;
        Assert(!pServer->pfnServe);
        Assert(!pServer->pvUser);
        Assert(pServer->Thread == NIL_RTTHREAD);
        Assert(pServer->SockClient == NIL_RTSOCKET);

        /*
         * Accept connection.
         */
        struct sockaddr_in  RemoteAddr;
        size_t              cbRemoteAddr = sizeof(RemoteAddr);
        RTSOCKET            Socket;
        RT_ZERO(RemoteAddr);
        rc = rtSocketAccept(SockServer, &Socket, (struct sockaddr *)&RemoteAddr, &cbRemoteAddr);
        if (RT_FAILURE(rc))
        {
            if (!rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_CREATED, RTTCPSERVERSTATE_ACCEPTING))
                rc = rtTcpServerListenCleanup(pServer);
            if (RT_FAILURE(rc))
                break;
            continue;
        }
        rtSocketSetInheritance(Socket, false /*fInheritable*/);

        /*
         * Chance to the 'serving' state and return the socket.
         */
        if (rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_SERVING, RTTCPSERVERSTATE_ACCEPTING))
        {
            RTSOCKET OldSocket = rtTcpAtomicXchgSock(&pServer->SockClient, Socket);
            Assert(OldSocket == NIL_RTSOCKET); NOREF(OldSocket);
            *pSockClient = Socket;
            rc = VINF_SUCCESS;
        }
        else
        {
            rtTcpClose(Socket, "RTTcpServerListen2", true /*fTryGracefulShutdown*/);
            rc = rtTcpServerListenCleanup(pServer);
        }
        break;
    }

    RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
    return rc;
}


/**
 * Terminate the open connection to the server.
 *
 * @returns iprt status code.
 * @param   pServer         Handle to the server.
 */
RTR3DECL(int) RTTcpServerDisconnectClient(PRTTCPSERVER pServer)
{
    /*
     * Validate input and retain the instance.
     */
    AssertPtrReturn(pServer, VERR_INVALID_HANDLE);
    AssertReturn(pServer->u32Magic == RTTCPSERVER_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(RTMemPoolRetain(pServer) != UINT32_MAX, VERR_INVALID_HANDLE);

    int rc = rtTcpServerDestroySocket(&pServer->SockClient, "DisconnectClient: client", true /*fTryGracefulShutdown*/);

    RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
    return rc;
}


/**
 * Shuts down the server, leaving client connections open.
 *
 * @returns IPRT status code.
 * @param   pServer         Handle to the server.
 */
RTR3DECL(int) RTTcpServerShutdown(PRTTCPSERVER pServer)
{
    /*
     * Validate input and retain the instance.
     */
    AssertPtrReturn(pServer, VERR_INVALID_HANDLE);
    AssertReturn(pServer->u32Magic == RTTCPSERVER_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(RTMemPoolRetain(pServer) != UINT32_MAX, VERR_INVALID_HANDLE);

    /*
     * Try change the state to stopping, then replace and destroy the server socket.
     */
    for (;;)
    {
        RTTCPSERVERSTATE enmState = pServer->enmState;
        if (    enmState != RTTCPSERVERSTATE_ACCEPTING
            &&  enmState != RTTCPSERVERSTATE_SERVING)
        {
            RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
            switch (enmState)
            {
                case RTTCPSERVERSTATE_CREATED:
                case RTTCPSERVERSTATE_STARTING:
                default:
                    AssertMsgFailed(("%d\n", enmState));
                    return VERR_INVALID_STATE;

                case RTTCPSERVERSTATE_STOPPING:
                case RTTCPSERVERSTATE_STOPPED:
                    return VINF_SUCCESS;

                case RTTCPSERVERSTATE_DESTROYING:
                    return VERR_TCP_SERVER_DESTROYED;
            }
        }
        if (rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_STOPPING, enmState))
        {
            rtTcpServerDestroySocket(&pServer->SockServer, "RTTcpServerShutdown", false /*fTryGracefulShutdown*/);
            rtTcpServerSetState(pServer, RTTCPSERVERSTATE_STOPPED, RTTCPSERVERSTATE_STOPPING);

            RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
            return VINF_SUCCESS;
        }
    }
}


/**
 * Closes down and frees a TCP Server.
 * This will also terminate any open connections to the server.
 *
 * @returns iprt status code.
 * @param   pServer         Handle to the server.
 */
RTR3DECL(int) RTTcpServerDestroy(PRTTCPSERVER pServer)
{
    /*
     * Validate input and retain the instance.
     */
    AssertPtrReturn(pServer, VERR_INVALID_HANDLE);
    AssertReturn(pServer->u32Magic == RTTCPSERVER_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(RTMemPoolRetain(pServer) != UINT32_MAX, VERR_INVALID_HANDLE); /* paranoia */

    /*
     * Move the state along so the listener can figure out what's going on.
     */
    for (;;)
    {
        bool             fDestroyable;
        RTTCPSERVERSTATE enmState = pServer->enmState;
        switch (enmState)
        {
            case RTTCPSERVERSTATE_STARTING:
            case RTTCPSERVERSTATE_ACCEPTING:
            case RTTCPSERVERSTATE_SERVING:
            case RTTCPSERVERSTATE_CREATED:
            case RTTCPSERVERSTATE_STOPPED:
                fDestroyable = rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_DESTROYING, enmState);
                break;

            /* destroyable states */
            case RTTCPSERVERSTATE_STOPPING:
                fDestroyable = true;
                break;

            /*
             * Everything else means user or internal misbehavior.
             */
            default:
                AssertMsgFailed(("pServer=%p enmState=%d\n", pServer, enmState));
                RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
                return VERR_INTERNAL_ERROR;
        }
        if (fDestroyable)
            break;
    }

    /*
     * Destroy it.
     */
    ASMAtomicWriteU32(&pServer->u32Magic, ~RTTCPSERVER_MAGIC);
    rtTcpServerDestroySocket(&pServer->SockServer, "Destroyer: server", false /*fTryGracefulShutdown*/);
    rtTcpServerDestroySocket(&pServer->SockClient, "Destroyer: client", true  /*fTryGracefulShutdown*/);

    /*
     * Release it.
     */
    RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
    RTMemPoolRelease(RTMEMPOOL_DEFAULT, pServer);
    return VINF_SUCCESS;
}


RTR3DECL(int) RTTcpClientConnect(const char *pszAddress, uint32_t uPort, PRTSOCKET pSock)
{
    int rc;

    /*
     * Validate input.
     */
    AssertReturn(uPort > 0, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszAddress, VERR_INVALID_POINTER);

#ifdef RT_OS_WINDOWS
    /*
     * Initialize WinSock and check version.
     */
    WORD    wVersionRequested = MAKEWORD(1, 1);
    WSADATA wsaData;
    rc = WSAStartup(wVersionRequested, &wsaData);
    if (wsaData.wVersion != wVersionRequested)
    {
        AssertMsgFailed(("Wrong winsock version\n"));
        return VERR_NOT_SUPPORTED;
    }
#endif

    /*
     * Resolve the address.
     */
    struct hostent *pHostEnt = NULL;
    pHostEnt = gethostbyname(pszAddress);
    if (!pHostEnt)
    {
        struct in_addr InAddr;
        InAddr.s_addr = inet_addr(pszAddress);
        pHostEnt = gethostbyaddr((char *)&InAddr, 4, AF_INET);
        if (!pHostEnt)
        {
            rc = rtSocketError();
            AssertMsgFailed(("Could not resolve '%s', rc=%Rrc\n", pszAddress, rc));
            return rc;
        }
    }

    /*
     * Create the socket and connect.
     */
    RTSOCKET Sock;
    rc = rtSocketCreate(&Sock, PF_INET, SOCK_STREAM, 0);
    if (RT_SUCCESS(rc))
    {
        rtSocketSetInheritance(Sock, false /*fInheritable*/);

        struct sockaddr_in InAddr;
        RT_ZERO(InAddr);
        InAddr.sin_family = AF_INET;
        InAddr.sin_port = htons(uPort);
        InAddr.sin_addr = *((struct in_addr *)pHostEnt->h_addr);
        rc = rtSocketConnect(Sock, (struct sockaddr *)&InAddr, sizeof(InAddr));
        if (RT_SUCCESS(rc))
        {
            *pSock = Sock;
            return VINF_SUCCESS;
        }

        rtTcpClose(Sock, "RTTcpClientConnect", false /*fTryGracefulShutdown*/);
    }
    return rc;
}


RTR3DECL(int) RTTcpClientClose(RTSOCKET Sock)
{
    return rtTcpClose(Sock, "RTTcpClientClose", true /*fTryGracefulShutdown*/);
}


/**
 * Internal close function which does all the proper bitching.
 */
static int rtTcpClose(RTSOCKET Sock, const char *pszMsg, bool fTryGracefulShutdown)
{
    int rc;

    /* ignore nil handles. */
    if (Sock == NIL_RTSOCKET)
        return VINF_SUCCESS;

    /*
     * Try to gracefully shut it down.
     */
    if (fTryGracefulShutdown)
    {
        rc = rtSocketShutdown(Sock, false /*fRead*/, true /*fWrite*/);
        if (RT_SUCCESS(rc))
        {
            uint64_t u64Start = RTTimeMilliTS();
            for (;;)
            {
                rc = rtSocketSelectOne(Sock, 1000);
                if (rc == VERR_TIMEOUT)
                {
                    if (RTTimeMilliTS() - u64Start > 30000)
                        break;
                }
                else if (rc != VINF_SUCCESS)
                    break;
                {
                    char abBitBucket[16*_1K];
                    ssize_t cbBytesRead = recv(rtSocketNative(Sock), &abBitBucket[0], sizeof(abBitBucket), MSG_NOSIGNAL);
                    if (cbBytesRead == 0)
                        break; /* orderly shutdown in progress */
                    if (cbBytesRead < 0)
                        break; /* some kind of error, never mind which... */
                }
            }  /* forever */
        }
    }

    /*
     * Destroy the socket handle.
     */
    return rtSocketDestroy(Sock);
}


RTR3DECL(int) RTTcpRead(RTSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    return rtSocketRead(Sock, pvBuffer, cbBuffer, pcbRead);
}


RTR3DECL(int)  RTTcpWrite(RTSOCKET Sock, const void *pvBuffer, size_t cbBuffer)
{
    return rtSocketWrite(Sock, pvBuffer, cbBuffer);
}


RTR3DECL(int)  RTTcpFlush(RTSOCKET Sock)
{

    int fFlag = 1;
    int rc = rtSocketSetOpt(Sock, IPPROTO_TCP, TCP_NODELAY, &fFlag, sizeof(fFlag));
    if (RT_SUCCESS(rc))
    {
        fFlag = 0;
        rc = rtSocketSetOpt(Sock, IPPROTO_TCP, TCP_NODELAY, &fFlag, sizeof(fFlag));
    }
    return rc;
}


RTR3DECL(int)  RTTcpSelectOne(RTSOCKET Sock, RTMSINTERVAL cMillies)
{
    return rtSocketSelectOne(Sock, cMillies);
}


RTR3DECL(int) RTTcpGetLocalAddress(RTSOCKET Sock, PRTNETADDR pAddr)
{
    return rtSocketGetLocalAddress(Sock, pAddr);
}


RTR3DECL(int) RTTcpGetPeerAddress(RTSOCKET Sock, PRTNETADDR pAddr)
{
    return rtSocketGetPeerAddress(Sock, pAddr);
}

