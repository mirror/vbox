/* $Id$ */
/** @file
 * innotek Portable Runtime - TCP/IP.
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
#include <winsock.h>
#else /* !RT_OS_WINDOWS */
#include <errno.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <netdb.h>
#include <unistd.h>
#endif /* !RT_OS_WINDOWS */

#include <iprt/tcp.h>
#include <iprt/thread.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/string.h>


/* non-standard linux stuff (it seems). */
#ifndef MSG_NOSIGNAL
# define MSG_NOSIGNAL 0
#endif
#ifndef SHUT_RDWR
# ifdef SD_BOTH
#  define SHUT_RDWR SD_BOTH
# else
#  define SHUT_RDWR 2
# endif
#endif

/* fixup backlevel OSes. */
#if defined(RT_OS_OS2) || defined(RT_OS_WINDOWS)
# define socklen_t  int
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define BACKLOG         10   /* how many pending connections queue will hold */


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
    /** Destroying signaling to the listener, listener will wait. */
    RTTCPSERVERSTATE_SIGNALING,
    /** Listener cleans up. */
    RTTCPSERVERSTATE_DESTROYING,
    /** Freed. */
    RTTCPSERVERSTATE_FREED
} RTTCPSERVERSTATE;

/*
 * Internal representation of the TCP Server handle.
 */
typedef struct RTTCPSERVER
{
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
static int rtTcpServerListen(PRTTCPSERVER pServer);
static void rcTcpServerListenCleanup(PRTTCPSERVER pServer);
static void rtTcpServerDestroyServerSock(RTSOCKET SockServer, const char *pszMsg);
static int rtTcpClose(RTSOCKET Sock, const char *pszMsg);



/**
 * Get the last error as an iprt status code.
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
 * Changes the TCP server state.
 */
DECLINLINE(bool) rtTcpServerSetState(PRTTCPSERVER pServer, RTTCPSERVERSTATE enmStateNew, RTTCPSERVERSTATE enmStateOld)
{
    bool fRc;
    ASMAtomicCmpXchgSize(&pServer->enmState, enmStateNew, enmStateOld, fRc);
    return fRc;
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
     * Do params checking
     */
    if (!uPort || !pfnServe || !pszThrdName || !ppServer)
    {
        AssertMsgFailed(("Invalid params\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Create the server.
     */
    PRTTCPSERVER    pServer;
    int rc = RTTcpServerCreateEx(pszAddress, uPort, &pServer);
    if (RT_SUCCESS(rc))
    {
        /*
         * Create the listener thread.
         */
        pServer->enmState   = RTTCPSERVERSTATE_STARTING;
        pServer->pvUser     = pvUser;
        pServer->pfnServe   = pfnServe;
        rc = RTThreadCreate(&pServer->Thread, rtTcpServerThread, pServer, 0, enmType, /*RTTHREADFLAGS_WAITABLE*/0, pszThrdName);
        if (RT_SUCCESS(rc))
        {
            /* done */
            if (ppServer)
                *ppServer = pServer;
            return rc;
        }

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
    if (rtTcpServerSetState(pServer, RTTCPSERVERSTATE_ACCEPTING, RTTCPSERVERSTATE_STARTING))
        return rtTcpServerListen(pServer);
    rcTcpServerListenCleanup(pServer);
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
     * Do params checking
     */
    if (!uPort || !ppServer)
    {
        AssertMsgFailed(("Invalid params\n"));
        return VERR_INVALID_PARAMETER;
    }

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
                rc = rtTcpError();
                AssertMsgFailed(("Could not get host address rc=%Vrc\n", rc));
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
                if (listen(WaitSock, BACKLOG) != -1)
                {
                    /*
                     * Create the server handle.
                     */
                    PRTTCPSERVER    pServer = (PRTTCPSERVER)RTMemAllocZ(sizeof(*pServer));
                    if (pServer)
                    {
                        pServer->SockServer = WaitSock;
                        pServer->SockClient = NIL_RTSOCKET;
                        pServer->Thread     = NIL_RTTHREAD;
                        pServer->enmState   = RTTCPSERVERSTATE_CREATED;
                        *ppServer = pServer;
                        return VINF_SUCCESS;
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
                else
                {
                    rc = rtTcpError();
                    AssertMsgFailed(("listen() %Vrc\n", rc));
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
            AssertMsgFailed(("setsockopt() %Vrc\n", rc));
        }
        rtTcpClose(WaitSock, "RTServerCreateEx");
    }
    else
    {
        rc = rtTcpError();
        AssertMsgFailed(("socket() %Vrc\n", rc));
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
 * @returns iprt status code.
 * @param   pServer         The server handle as returned from RTTcpServerCreateEx().
 * @param   pfnServe        The function which will serve a new client connection.
 * @param   pvUser          User argument passed to pfnServe.
 */
RTR3DECL(int) RTTcpServerListen(PRTTCPSERVER pServer, PFNRTTCPSERVE pfnServe, void *pvUser)
{
    /*
     * Validate input.
     */
    if (!pfnServe || !pServer)
    {
        AssertMsgFailed(("pfnServer=%p pServer=%p\n", pfnServe, pServer));
        return VERR_INVALID_PARAMETER;
    }
    if (rtTcpServerSetState(pServer, RTTCPSERVERSTATE_ACCEPTING, RTTCPSERVERSTATE_CREATED))
    {
        Assert(!pServer->pfnServe);
        Assert(!pServer->pvUser);
        Assert(pServer->Thread == NIL_RTTHREAD);
        Assert(pServer->SockClient == NIL_RTSOCKET);

        pServer->pfnServe = pfnServe;
        pServer->pvUser   = pvUser;
        pServer->Thread   = RTThreadSelf();
        Assert(pServer->Thread != NIL_RTTHREAD);
        return rtTcpServerListen(pServer);
    }
    AssertMsgFailed(("pServer->enmState=%d\n", pServer->enmState));
    return VERR_INVALID_PARAMETER;
}


/**
 * Closes the client socket.
 */
static int rtTcpServerDestroyClientSock(RTSOCKET volatile *pSock, const char *pszMsg)
{
    RTSOCKET Sock = rtTcpAtomicXchgSock(pSock, NIL_RTSOCKET);
    if (Sock != NIL_RTSOCKET)
        shutdown(Sock, SHUT_RDWR);
    return rtTcpClose(Sock, pszMsg);
}


/**
 * Internal worker common for RTTcpServerListen and the thread created by RTTcpServerCreate().
 */
static int rtTcpServerListen(PRTTCPSERVER pServer)
{
    /*
     * Accept connection loop.
     */
    int rc = VINF_SUCCESS;
    for (;;)
    {
        /*
         * Change state.
         */
        RTTCPSERVERSTATE enmState = pServer->enmState;
        if (    enmState != RTTCPSERVERSTATE_ACCEPTING
            &&  enmState != RTTCPSERVERSTATE_SERVING)
            break;
        if (!rtTcpServerSetState(pServer, RTTCPSERVERSTATE_ACCEPTING, enmState))
            continue;

        /*
         * Accept connection.
         */
        struct sockaddr_in RemoteAddr = {0};
        socklen_t Len = sizeof(RemoteAddr);
        RTSOCKET Socket = accept(pServer->SockServer, (struct sockaddr *)&RemoteAddr, &Len);
        if (Socket == -1)
        {
#ifndef RT_OS_WINDOWS
            /* These are typical for what can happen during destruction. */
            if (errno == EBADF || errno == EINVAL || errno == ENOTSOCK)
                break;
#endif
            continue;
        }

        /*
         * Run a pfnServe callback.
         */
        if (!rtTcpServerSetState(pServer, RTTCPSERVERSTATE_SERVING, RTTCPSERVERSTATE_ACCEPTING))
            break;
        rtTcpAtomicXchgSock(&pServer->SockClient, Socket);
        rc = pServer->pfnServe(Socket, pServer->pvUser);
        rtTcpServerDestroyClientSock(&pServer->SockClient, "Listener: client");

        /*
         * Stop the server?
         */
        if (rc == VERR_TCP_SERVER_STOP)
        {
            if (rtTcpServerSetState(pServer, RTTCPSERVERSTATE_STOPPING, RTTCPSERVERSTATE_SERVING))
            {
                /*
                 * Reset the server socket and change the state to stopped. After that state change
                 * we cannot safely access the handle so we'll have to return here.
                 */
                RTSOCKET SockServer = rtTcpAtomicXchgSock(&pServer->SockServer, NIL_RTSOCKET);
                rtTcpServerSetState(pServer, RTTCPSERVERSTATE_STOPPED, RTTCPSERVERSTATE_STOPPING);
                rtTcpClose(SockServer, "Listener: server stopped");
                return rc;
            }
            break;
        }
    }

    /*
     * Perform any pending clean and be gone.
     */
    rcTcpServerListenCleanup(pServer);
    return rc;
}


/**
 * Clean up after listener.
 */
static void rcTcpServerListenCleanup(PRTTCPSERVER pServer)
{
    /*
     * Wait for any destroyers to finish signaling us.
     */
    for (unsigned cTries = 99; cTries > 0; cTries--)
    {
        RTTCPSERVERSTATE enmState = pServer->enmState;
        switch (enmState)
        {
            /*
             * Intermediate state while the destroyer closes the client socket.
             */
            case RTTCPSERVERSTATE_SIGNALING:
                if (!RTThreadYield())
                    RTThreadSleep(1);
                break;

            /*
             * Free the handle.
             */
            case RTTCPSERVERSTATE_DESTROYING:
            {
                rtTcpClose(rtTcpAtomicXchgSock(&pServer->SockServer, NIL_RTSOCKET), "Listener-cleanup: server");
                rtTcpServerSetState(pServer, RTTCPSERVERSTATE_FREED, RTTCPSERVERSTATE_DESTROYING);
                RTMemFree(pServer);
                return;
            }

            /*
             * Everything else means failure.
             */
            default:
                AssertMsgFailed(("pServer=%p enmState=%d\n", pServer, enmState));
                return;
        }
    }
    AssertMsgFailed(("Timed out when trying to clean up after listener. pServer=%p enmState=%d\n", pServer, pServer->enmState));
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
     * Validate input.
     */
    if (    !pServer
        ||  pServer->enmState <= RTTCPSERVERSTATE_INVALID
        ||  pServer->enmState >= RTTCPSERVERSTATE_FREED)
    {
        AssertMsgFailed(("Invalid parameter!\n"));
        return VERR_INVALID_PARAMETER;
    }

    return rtTcpServerDestroyClientSock(&pServer->SockClient, "DisconnectClient: client");
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
     * Validate input.
     */
    if (    !pServer
        ||  pServer->enmState <= RTTCPSERVERSTATE_INVALID
        ||  pServer->enmState >= RTTCPSERVERSTATE_FREED)
    {
        AssertMsgFailed(("Invalid parameter!\n"));
        return VERR_INVALID_PARAMETER;
    }

/** @todo r=bird: Some of this horrible code can probably be exchanged with a RTThreadWait(). (It didn't exist when this code was written.) */

    /*
     * Move it to the destroying state.
     */
    RTSOCKET    SockServer = rtTcpAtomicXchgSock(&pServer->SockServer, NIL_RTSOCKET);
    for (unsigned cTries = 99; cTries > 0; cTries--)
    {
        RTTCPSERVERSTATE enmState = pServer->enmState;
        switch (enmState)
        {
            /*
             * Try move it to the destroying state.
             */
            case RTTCPSERVERSTATE_STARTING:
            case RTTCPSERVERSTATE_ACCEPTING:
            case RTTCPSERVERSTATE_SERVING:
            {
                if (rtTcpServerSetState(pServer, RTTCPSERVERSTATE_SIGNALING, enmState))
                {
                    /* client */
                    rtTcpServerDestroyClientSock(&pServer->SockClient, "Destroyer: client");

                    bool fRc = rtTcpServerSetState(pServer, RTTCPSERVERSTATE_DESTROYING, RTTCPSERVERSTATE_SIGNALING);
                    Assert(fRc); NOREF(fRc);

                    /* server */
                    rtTcpServerDestroyServerSock(SockServer, "Destroyer: server destroying");
                    RTThreadYield();

                    return VINF_SUCCESS;
                }
                break;
            }


            /*
             * Intermediate state.
             */
            case RTTCPSERVERSTATE_STOPPING:
                if (!RTThreadYield())
                    RTThreadSleep(1);
                break;

            /*
             * Just release the handle.
             */
            case RTTCPSERVERSTATE_CREATED:
            case RTTCPSERVERSTATE_STOPPED:
                if (rtTcpServerSetState(pServer, RTTCPSERVERSTATE_FREED, enmState))
                {
                    rtTcpServerDestroyServerSock(SockServer, "Destroyer: server freeing");
                    RTMemFree(pServer);
                    return VINF_TCP_SERVER_STOP;
                }
                break;

            /*
             * Everything else means failure.
             */
            default:
                AssertMsgFailed(("pServer=%p enmState=%d\n", pServer, enmState));
                return VERR_INTERNAL_ERROR;
        }
    }

    AssertMsgFailed(("Giving up! pServer=%p enmState=%d\n", pServer, pServer->enmState));
    rtTcpServerDestroyServerSock(SockServer, "Destroyer: server timeout");
    return VERR_INTERNAL_ERROR;
}


/**
 * Shutdowns the server socket.
 */
static void rtTcpServerDestroyServerSock(RTSOCKET SockServer, const char *pszMsg)
{
    if (SockServer == NIL_RTSOCKET)
        return;
    shutdown(SockServer, SHUT_RDWR);
    rtTcpClose(SockServer, "Destroyer: server destroying");
}



RTR3DECL(int)  RTTcpRead(RTSOCKET Sock, void *pvBuffer, size_t cbBuffer, size_t *pcbRead)
{
    /*
     * Do params checking
     */
    if (!pvBuffer || !cbBuffer)
    {
        AssertMsgFailed(("Invalid params\n"));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Read loop.
     * If pcbRead is NULL we have to fill the entire buffer!
     */
    size_t cbRead = 0;
    size_t cbToRead = cbBuffer;
    for (;;)
    {
        ssize_t cbBytesRead = recv(Sock, (char *)pvBuffer + cbRead, cbToRead, MSG_NOSIGNAL);
        if (cbBytesRead < 0)
            return rtTcpError();
        if (cbBytesRead == 0 && rtTcpError())
            return rtTcpError();
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
     * Do params checking
     */
    AssertReturn(uPort, VERR_INVALID_PARAMETER);
    AssertReturn(VALID_PTR(pszAddress), VERR_INVALID_PARAMETER);

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
            AssertMsgFailed(("Could not resolve '%s', rc=%Vrc\n", pszAddress, rc));
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
        rtTcpClose(Sock, "RTTcpClientConnect");
    }
    else
        rc = rtTcpError();
    return rc;
}


RTR3DECL(int) RTTcpClientClose(RTSOCKET Sock)
{
    return rtTcpClose(Sock, "RTTcpClientClose");
}


/**
 * Internal close function which does all the proper bitching.
 */
static int rtTcpClose(RTSOCKET Sock, const char *pszMsg)
{
    /* ignore nil handles. */
    if (Sock == NIL_RTSOCKET)
        return VINF_SUCCESS;

    /*
     * Attempt to close it.
     */
#ifdef RT_OS_WINDOWS
    int rc = closesocket(Sock);
#else
    int rc = close(Sock);
#endif
    if (!rc)
        return VINF_SUCCESS;
    rc = rtTcpError();
    AssertMsgFailed(("\"%s\": close(%d) -> %Vrc\n", pszMsg, Sock, rc));
    return rc;
}

