/* $Id$ */
/** @file
 * IPRT - TCP/IP.
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
# include <sys/un.h>
# include <netdb.h>
# include <unistd.h>
#endif /* !RT_OS_WINDOWS */

#include "internal/iprt.h"
#include <iprt/tcp.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mempool.h>
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
static int  rcTcpServerListenCleanup(PRTTCPSERVER pServer);
static int  rtTcpServerDestroySocket(RTSOCKET volatile *pSockClient, const char *pszMsg);
static int  rtTcpClose(RTSOCKET Sock, const char *pszMsg, bool fTryGracefulShutdown);



/**
 * Get the last error as an iprt status code.
 *
 * @returns iprt status code.
 */
DECLINLINE(int) rtTcpError(void)
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
DECLINLINE(void) rtTcpErrorReset(void)
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
DECLINLINE(int) rtTcpResolverError(void)
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
 * Atomicly updates a socket variable.
 * @returns The old value.
 * @param   pSock   The socket variable to update.
 * @param   Sock    The new value.
 */
DECLINLINE(RTSOCKET) rtTcpAtomicXchgSock(RTSOCKET volatile *pSock, const RTSOCKET Sock)
{
    switch (sizeof(RTSOCKET))
    {
        case 4: return (RTSOCKET)ASMAtomicXchgS32((int32_t volatile *)pSock, (int32_t)Sock);
        default:
            AssertReleaseFailed();
            return NIL_RTSOCKET;
    }
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
    RTSOCKET Sock = rtTcpAtomicXchgSock(pSock, NIL_RTSOCKET);
    if (Sock != NIL_RTSOCKET)
    {
        if (!fTryGracefulShutdown)
            shutdown(Sock, SHUT_RDWR);
        return rtTcpClose(Sock, pszMsg, fTryGracefulShutdown);
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
        rc = rcTcpServerListenCleanup(pServer);
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
                rc = rtTcpResolverError();
                AssertMsgFailed(("Could not get host address rc=%Rrc\n", rc));
                return rc;
            }
        }
    }

    /*
     * Setting up socket.
     */
    RTSOCKET WaitSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (WaitSock != -1)
    {
        /*
         * Set socket options.
         */
        int fFlag = 1;
        if (!setsockopt(WaitSock, SOL_SOCKET, SO_REUSEADDR, (const char *)&fFlag, sizeof(fFlag)))
        {
            /*
             * Set socket family, address and port.
             */
            struct sockaddr_in LocalAddr = {0};
            LocalAddr.sin_family = AF_INET;
            LocalAddr.sin_port = htons(uPort);
            /* if address not specified, use INADDR_ANY. */
            if (!pHostEnt)
                LocalAddr.sin_addr.s_addr = INADDR_ANY;
            else
                LocalAddr.sin_addr = *((struct in_addr *)pHostEnt->h_addr);

            /*
             * Bind a name to a socket.
             */
            if (bind(WaitSock, (struct sockaddr *)&LocalAddr, sizeof(LocalAddr)) != -1)
            {
                /*
                 * Listen for connections on a socket.
                 */
                if (listen(WaitSock, RTTCP_SERVER_BACKLOG) != -1)
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
                else
                {
                    rc = rtTcpError();
                    AssertMsgFailed(("listen() %Rrc\n", rc));
                }
            }
            else
            {
                rc = rtTcpError();
            }
        }
        else
        {
            rc = rtTcpError();
            AssertMsgFailed(("setsockopt() %Rrc\n", rc));
        }
        rtTcpClose(WaitSock, "RTServerCreateEx", false /*fTryGracefulShutdown*/);
    }
    else
    {
        rc = rtTcpError();
        AssertMsgFailed(("socket() %Rrc\n", rc));
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
            return rcTcpServerListenCleanup(pServer);
        if (!rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_ACCEPTING, enmState))
            continue;

        /*
         * Accept connection.
         */
        struct sockaddr_in  RemoteAddr   = {0};
        socklen_t           cbRemoteAddr = sizeof(RemoteAddr);
        RTSOCKET Socket = accept(SockServer, (struct sockaddr *)&RemoteAddr, &cbRemoteAddr);
        if (Socket == -1)
        {
#ifndef RT_OS_WINDOWS
            /* These are typical for what can happen during destruction. */
            if (errno == EBADF || errno == EINVAL || errno == ENOTSOCK)
                return rcTcpServerListenCleanup(pServer);
#endif
            continue;
        }

        /*
         * Run a pfnServe callback.
         */
        if (!rtTcpServerTrySetState(pServer, RTTCPSERVERSTATE_SERVING, RTTCPSERVERSTATE_ACCEPTING))
        {
            rtTcpClose(Socket, "rtTcpServerListen", true /*fTryGracefulShutdown*/);
            return rcTcpServerListenCleanup(pServer);
        }
        rtTcpAtomicXchgSock(&pServer->SockClient, Socket);
        int rc = pServer->pfnServe(Socket, pServer->pvUser);
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
                RTSOCKET SockServer = rtTcpAtomicXchgSock(&pServer->SockServer, NIL_RTSOCKET);
                rtTcpServerSetState(pServer, RTTCPSERVERSTATE_STOPPED, RTTCPSERVERSTATE_STOPPING);
                rtTcpClose(SockServer, "Listener: server stopped", false /*fTryGracefulShutdown*/);
            }
            else
                rcTcpServerListenCleanup(pServer); /* ignore rc */
            return rc;
        }
    }
}


/**
 * Clean up after listener.
 */
static int rcTcpServerListenCleanup(PRTTCPSERVER pServer)
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


RTR3DECL(int) RTTcpRead(RTSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    /*
     * Validate input.
     */
    AssertReturn(cbBuffer > 0, VERR_INVALID_PARAMETER);
    AssertPtr(pvBuffer);

    /*
     * Read loop.
     * If pcbRead is NULL we have to fill the entire buffer!
     */
    size_t cbRead = 0;
    size_t cbToRead = cbBuffer;
    for (;;)
    {
        rtTcpErrorReset();
        ssize_t cbBytesRead = recv(Sock, (char *)pvBuffer + cbRead, cbToRead, MSG_NOSIGNAL);
        if (cbBytesRead <= 0)
        {
            int rc = rtTcpError();
            Assert(RT_FAILURE_NP(rc) || cbBytesRead == 0);
            if (RT_FAILURE_NP(rc))
                return rc;
            if (pcbRead)
            {
                *pcbRead = 0;
                return VINF_SUCCESS;
            }
            return VERR_NET_SHUTDOWN;
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

    return VINF_SUCCESS;
}


RTR3DECL(int)  RTTcpWrite(RTSOCKET Sock, const void *pvBuffer, size_t cbBuffer)
{
    do
    {
        ssize_t cbWritten = send(Sock, (const char *)pvBuffer, cbBuffer, MSG_NOSIGNAL);
        if (cbWritten < 0)
            return rtTcpError();
        AssertMsg(cbBuffer >= (size_t)cbWritten, ("Wrote more than we requested!!! cbWritten=%d cbBuffer=%d rtTcpError()=%d\n",
                                                  cbWritten, cbBuffer, rtTcpError()));
        cbBuffer -= cbWritten;
        pvBuffer = (char *)pvBuffer + cbWritten;
    } while (cbBuffer);

    return VINF_SUCCESS;
}


RTR3DECL(int)  RTTcpFlush(RTSOCKET Sock)
{
    int fFlag = 1;
    setsockopt(Sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&fFlag, sizeof(fFlag));
    fFlag = 0;
    setsockopt(Sock, IPPROTO_TCP, TCP_NODELAY, (const char *)&fFlag, sizeof(fFlag));

    return VINF_SUCCESS;
}


RTR3DECL(int)  RTTcpSelectOne(RTSOCKET Sock, unsigned cMillies)
{
    fd_set fdsetR;
    FD_ZERO(&fdsetR);
    FD_SET(Sock, &fdsetR);

    fd_set fdsetE = fdsetR;

    int rc;
    if (cMillies == RT_INDEFINITE_WAIT)
        rc = select(Sock + 1, &fdsetR, NULL, &fdsetE, NULL);
    else
    {
        struct timeval timeout;
        timeout.tv_sec = cMillies / 1000;
        timeout.tv_usec = (cMillies % 1000) * 1000;
        rc = select(Sock + 1, &fdsetR, NULL, &fdsetE, &timeout);
    }
    if (rc > 0)
        return VINF_SUCCESS;
    if (rc == 0)
        return VERR_TIMEOUT;
    return rtTcpError();
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
            rc = rtTcpError();
            AssertMsgFailed(("Could not resolve '%s', rc=%Rrc\n", pszAddress, rc));
            return rc;
        }
    }

    /*
     * Create the socket and connect.
     */
    RTSOCKET Sock = socket(PF_INET, SOCK_STREAM, 0);
    if (Sock != -1)
    {
        struct sockaddr_in InAddr = {0};
        InAddr.sin_family = AF_INET;
        InAddr.sin_port = htons(uPort);
        InAddr.sin_addr = *((struct in_addr *)pHostEnt->h_addr);
        if (!connect(Sock, (struct sockaddr *)&InAddr, sizeof(InAddr)))
        {
            *pSock = Sock;
            return VINF_SUCCESS;
        }
        rc = rtTcpError();
        rtTcpClose(Sock, "RTTcpClientConnect", false /*fTryGracefulShutdown*/);
    }
    else
        rc = rtTcpError();
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
        rc = shutdown(Sock, SHUT_WR);
        if (!rc)
        {
            uint64_t u64Start = RTTimeMilliTS();
            for (;;)
            {
                rc = RTTcpSelectOne(Sock, 1000);
                if (rc == VERR_TIMEOUT)
                {
                    if (RTTimeMilliTS() - u64Start > 30000)
                        break;
                }
                else if (rc != VINF_SUCCESS)
                    break;
                {
                    uint8_t abBitBucket[16*_1KB];
                    ssize_t cbBytesRead = recv(Sock, abBitBucket, sizeof(abBitBucket), MSG_NOSIGNAL);
                    if (cbBytesRead == 0)
                        break; /* orderly shutdown in progress */
                    if (cbBytesRead < 0)
                        break; /* some kind of error, never mind which... */
                }
            }  /* forever */
        }
    }

    /*
     * Attempt to close it.
     */
#ifdef RT_OS_WINDOWS
    rc = closesocket(Sock);
#else
    rc = close(Sock);
#endif
    if (!rc)
        return VINF_SUCCESS;
    rc = rtTcpError();
    AssertMsgFailed(("\"%s\": close(%d) -> %Rrc\n", pszMsg, Sock, rc));
    return rc;
}

