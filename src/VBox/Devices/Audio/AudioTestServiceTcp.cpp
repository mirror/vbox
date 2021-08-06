/* $Id$ */
/** @file
 * AudioTestServiceTcp - Audio test execution server, TCP/IP Transport Layer.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_AUDIO_TEST
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/poll.h>
#include <iprt/string.h>
#include <iprt/tcp.h>
#include <iprt/thread.h>
#include <iprt/time.h>

#include <VBox/log.h>

#include "AudioTestService.h"
#include "AudioTestServiceInternal.h"


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * TCP specific client data.
 */
typedef struct ATSTRANSPORTCLIENT
{
    /** Socket of the current client. */
    RTSOCKET                           hTcpClient;
    /** Indicates whether \a hTcpClient comes from the server or from a client
     * connect (relevant when closing it). */
    bool                               fFromServer;
    /** The size of the stashed data. */
    size_t                             cbTcpStashed;
    /** The size of the stashed data allocation. */
    size_t                             cbTcpStashedAlloced;
    /** The stashed data. */
    uint8_t                           *pbTcpStashed;
} ATSTRANSPORTCLIENT;

/**
 * Enumeration for the TCP/IP connection mode.
 */
typedef enum ATSTCPMODE
{
    /** Both: Uses parallel client and server connection methods (via threads). */
    ATSTCPMODE_BOTH = 0,
    /** Client only: Connects to a server. */
    ATSTCPMODE_CLIENT,
    /** Server only: Listens for new incoming client connections. */
    ATSTCPMODE_SERVER
} ATSTCPMODE;

/**
 * Structure for keeping Audio Test Service (ATS) transport instance-specific data.
 */
typedef struct ATSTRANSPORTINST
{
    /** Critical section for serializing access. */
    RTCRITSECT                         CritSect;
    /** Connection mode to use. */
    ATSTCPMODE                         enmMode;
    /** The addresses to bind to.  Empty string means any. */
    char                               szBindAddr[256];
    /** The TCP port to listen to. */
    uint32_t                           uBindPort;
    /** The addresses to connect to if running in reversed (VM NATed) mode. */
    char                               szConnectAddr[256];
    /** The TCP port to connect to if running in reversed (VM NATed) mode. */
    uint32_t                           uConnectPort;
    /** Pointer to the TCP server instance. */
    PRTTCPSERVER                       pTcpServer;
    /** Thread calling RTTcpServerListen2. */
    RTTHREAD                           hThreadServer;
    /** Thread calling RTTcpClientConnect. */
    RTTHREAD                           hThreadConnect;
    /** The main thread handle (for signalling). */
    RTTHREAD                           hThreadMain;
    /** Stop connecting attempts when set. */
    bool                               fStopConnecting;
    /** Connect cancel cookie. */
    PRTTCPCLIENTCONNECTCANCEL volatile pConnectCancelCookie;
} ATSTRANSPORTINST;
/** Pointer to an Audio Test Service (ATS) TCP/IP transport instance. */
typedef ATSTRANSPORTINST *PATSTRANSPORTINST;

/**
 * Structure holding an ATS connection context, which is
 * required when connecting a client via server (listening) or client (connecting).
 */
typedef struct ATSCONNCTX
{
    /** Pointer to transport instance to use. */
    PATSTRANSPORTINST                  pInst;
    /** Pointer to transport client to connect. */
    PATSTRANSPORTCLIENT                pClient;
} ATSCONNCTX;
/** Pointer to an Audio Test Service (ATS) TCP/IP connection context. */
typedef ATSCONNCTX *PATSCONNCTX;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/

/**
 * Disconnects the current client and frees all stashed data.
 */
static void atsTcpDisconnectClient(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient)
{
    RT_NOREF(pThis);

    if (pClient->hTcpClient != NIL_RTSOCKET)
    {
        int rc;
        if (pClient->fFromServer)
            rc = RTTcpServerDisconnectClient2(pClient->hTcpClient);
        else
            rc = RTTcpClientClose(pClient->hTcpClient);
        pClient->hTcpClient = NIL_RTSOCKET;
        AssertRCSuccess(rc);
    }

    if (pClient->pbTcpStashed)
    {
        RTMemFree(pClient->pbTcpStashed);
        pClient->pbTcpStashed = NULL;
    }
}

/**
 * Sets the current client socket in a safe manner.
 *
 * @returns NIL_RTSOCKET if consumed, other wise hTcpClient.
 * @param   pThis           Transport instance.
 * @param   pClient         Client to set the socket for.
 * @param   fFromServer     Whether the socket is from a server (listening) or client (connecting) call.
 *                          Important when closing / disconnecting.
 * @param   hTcpClient      The client socket.
 */
static RTSOCKET atsTcpSetClient(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, bool fFromServer, RTSOCKET hTcpClient)
{
    RTCritSectEnter(&pThis->CritSect);
    if (   pClient->hTcpClient  == NIL_RTSOCKET
        && !pThis->fStopConnecting)
    {
        LogFunc(("New client connected\n"));

        pClient->fFromServer = fFromServer;
        pClient->hTcpClient = hTcpClient;
        hTcpClient = NIL_RTSOCKET; /* Invalidate, as pClient has now ownership. */
    }
    RTCritSectLeave(&pThis->CritSect);
    return hTcpClient;
}

/**
 * Checks if it's a fatal RTTcpClientConnect return code.
 *
 * @returns true / false.
 * @param   rc              The IPRT status code.
 */
static bool atsTcpIsFatalClientConnectStatus(int rc)
{
    return rc != VERR_NET_UNREACHABLE
        && rc != VERR_NET_HOST_DOWN
        && rc != VERR_NET_HOST_UNREACHABLE
        && rc != VERR_NET_CONNECTION_REFUSED
        && rc != VERR_TIMEOUT
        && rc != VERR_NET_CONNECTION_TIMED_OUT;
}

/**
 * Server mode connection thread.
 *
 * @returns iprt status code.
 * @param   hSelf           Thread handle. Ignored.
 * @param   pvUser          Pointer to ATSTRANSPORTINST the thread is bound to.
 */
static DECLCALLBACK(int) atsTcpServerConnectThread(RTTHREAD hSelf, void *pvUser)
{
    RT_NOREF(hSelf);

    PATSCONNCTX         pConnCtx = (PATSCONNCTX)pvUser;
    PATSTRANSPORTINST   pThis    = pConnCtx->pInst;
    PATSTRANSPORTCLIENT pClient  = pConnCtx->pClient;

    RTSOCKET hTcpClient;
    int rc = RTTcpServerListen2(pThis->pTcpServer, &hTcpClient);
    if (RT_SUCCESS(rc))
    {
        hTcpClient = atsTcpSetClient(pThis, pClient, true /* fFromServer */, hTcpClient);
        RTTcpServerDisconnectClient2(hTcpClient);
    }

    return rc;
}

/**
 * Client mode connection thread.
 *
 * @returns iprt status code.
 * @param   hSelf           Thread handle. Use to sleep on. The main thread will
 *                          signal it to speed up thread shutdown.
 * @param   pvUser          Pointer to a connection context (PATSCONNCTX) the thread is bound to.
 */
static DECLCALLBACK(int) atsTcpClientConnectThread(RTTHREAD hSelf, void *pvUser)
{
    PATSCONNCTX         pConnCtx = (PATSCONNCTX)pvUser;
    PATSTRANSPORTINST   pThis    = pConnCtx->pInst;
    PATSTRANSPORTCLIENT pClient  = pConnCtx->pClient;

    for (;;)
    {
        /* Stop? */
        RTCritSectEnter(&pThis->CritSect);
        bool fStop = pThis->fStopConnecting;
        RTCritSectLeave(&pThis->CritSect);
        if (fStop)
            return VINF_SUCCESS;

        /* Try connect. */ /** @todo make cancelable! */
        RTSOCKET hTcpClient;
        int rc = RTTcpClientConnectEx(pThis->szConnectAddr, pThis->uConnectPort, &hTcpClient,
                                      RT_SOCKETCONNECT_DEFAULT_WAIT, &pThis->pConnectCancelCookie);
        if (RT_SUCCESS(rc))
        {
            hTcpClient = atsTcpSetClient(pThis, pClient, false /* fFromServer */, hTcpClient);
            RTTcpClientCloseEx(hTcpClient, true /* fGracefulShutdown*/);
            break;
        }

        if (atsTcpIsFatalClientConnectStatus(rc))
            return rc;

        /* Delay a wee bit before retrying. */
        RTThreadUserWait(hSelf, 1536);
    }
    return VINF_SUCCESS;
}

/**
 * Wait on the threads to complete.
 *
 * @returns Thread status (if collected), otherwise VINF_SUCCESS.
 * @param   pThis           Transport instance.
 * @param   cMillies        The period to wait on each thread.
 */
static int atsTcpConnectWaitOnThreads(PATSTRANSPORTINST pThis, RTMSINTERVAL cMillies)
{
    int rcRet = VINF_SUCCESS;

    if (pThis->hThreadConnect != NIL_RTTHREAD)
    {
        int rcThread;
        int rc2 = RTThreadWait(pThis->hThreadConnect, cMillies, &rcThread);
        if (RT_SUCCESS(rc2))
        {
            pThis->hThreadConnect = NIL_RTTHREAD;
            rcRet = rcThread;
        }
    }

    if (pThis->hThreadServer != NIL_RTTHREAD)
    {
        int rcThread;
        int rc2 = RTThreadWait(pThis->hThreadServer, cMillies, &rcThread);
        if (RT_SUCCESS(rc2))
        {
            pThis->hThreadServer = NIL_RTTHREAD;
            if (RT_SUCCESS(rc2))
                rcRet = rcThread;
        }
    }
    return rcRet;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnWaitForConnect}
 */
static DECLCALLBACK(int) atsTcpWaitForConnect(PATSTRANSPORTINST pThis, PPATSTRANSPORTCLIENT ppClientNew)
{
    PATSTRANSPORTCLIENT pClient = (PATSTRANSPORTCLIENT)RTMemAllocZ(sizeof(ATSTRANSPORTCLIENT));
    AssertPtrReturn(pClient, VERR_NO_MEMORY);

    int rc;

    if (pThis->enmMode == ATSTCPMODE_SERVER)
    {
        pClient->fFromServer = true;
        rc = RTTcpServerListen2(pThis->pTcpServer, &pClient->hTcpClient);
        LogFunc(("RTTcpServerListen2 -> %Rrc\n", rc));
    }
    else if (pThis->enmMode == ATSTCPMODE_CLIENT)
    {
        pClient->fFromServer = false;
        for (;;)
        {
            Log2Func(("Calling RTTcpClientConnect(%s, %u,)...\n", pThis->szConnectAddr, pThis->uConnectPort));
            rc = RTTcpClientConnect(pThis->szConnectAddr, pThis->uConnectPort, &pClient->hTcpClient);
            LogFunc(("RTTcpClientConnect -> %Rrc\n", rc));
            if (RT_SUCCESS(rc) || atsTcpIsFatalClientConnectStatus(rc))
                break;

            /* Delay a wee bit before retrying. */
            RTThreadSleep(1536);
        }
    }
    else
    {
        Assert(pThis->enmMode == ATSTCPMODE_BOTH);

        /*
         * Create client threads.
         */
        RTCritSectEnter(&pThis->CritSect);

        pThis->fStopConnecting = false;
        RTCritSectLeave(&pThis->CritSect);

        atsTcpConnectWaitOnThreads(pThis, 32 /* cMillies */);

        ATSCONNCTX ConnCtx;
        RT_ZERO(ConnCtx);
        ConnCtx.pInst   = pThis;
        ConnCtx.pClient = pClient;

        rc = VINF_SUCCESS;
        if (pThis->hThreadConnect == NIL_RTTHREAD)
        {
            pThis->pConnectCancelCookie = NULL;
            rc = RTThreadCreate(&pThis->hThreadConnect, atsTcpClientConnectThread, &ConnCtx, 0, RTTHREADTYPE_DEFAULT,
                                RTTHREADFLAGS_WAITABLE, "tcpconn");
        }
        if (pThis->hThreadServer == NIL_RTTHREAD && RT_SUCCESS(rc))
            rc = RTThreadCreate(&pThis->hThreadServer, atsTcpServerConnectThread, &ConnCtx, 0, RTTHREADTYPE_DEFAULT,
                                RTTHREADFLAGS_WAITABLE, "tcpserv");

        RTCritSectEnter(&pThis->CritSect);

        /*
         * Wait for connection to be established.
         */
        while (   RT_SUCCESS(rc)
               && pClient->hTcpClient == NIL_RTSOCKET)
        {
            RTCritSectLeave(&pThis->CritSect);
            rc = atsTcpConnectWaitOnThreads(pThis, 10 /* cMillies */);
            RTCritSectEnter(&pThis->CritSect);
        }

        /*
         * Cancel the threads.
         */
        pThis->fStopConnecting = true;

        RTCritSectLeave(&pThis->CritSect);
        RTTcpClientCancelConnect(&pThis->pConnectCancelCookie);
    }

    if (RT_SUCCESS(rc))
    {
        *ppClientNew = pClient;
    }
    else
    {
        if (pClient)
        {
            RTTcpServerDisconnectClient2(pClient->hTcpClient);

            RTMemFree(pClient);
            pClient = NULL;
        }
    }

    return rc;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnNotifyReboot}
 */
static DECLCALLBACK(void) atsTcpNotifyReboot(PATSTRANSPORTINST pThis)
{
    LogFunc(("RTTcpServerDestroy(%p)\n", pThis->pTcpServer));
    if (pThis->pTcpServer)
    {
        int rc = RTTcpServerDestroy(pThis->pTcpServer);
        if (RT_FAILURE(rc))
            RTMsgInfo("RTTcpServerDestroy failed in atsTcpNotifyReboot: %Rrc", rc);
        pThis->pTcpServer = NULL;
    }
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnNotifyBye}
 */
static DECLCALLBACK(void) atsTcpNotifyBye(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient)
{
    LogFunc(("atsTcpDisconnectClient %RTsock\n", pClient->hTcpClient));
    atsTcpDisconnectClient(pThis, pClient);
    RTMemFree(pClient);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnNotifyHowdy}
 */
static DECLCALLBACK(void) atsTcpNotifyHowdy(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient)
{
    /* nothing to do here */
    RT_NOREF(pThis, pClient);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnBabble}
 */
static DECLCALLBACK(void) atsTcpBabble(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr, RTMSINTERVAL cMsSendTimeout)
{
    /*
     * Try send the babble reply.
     */
    NOREF(cMsSendTimeout); /** @todo implement the timeout here; non-blocking write + select-on-write. */
    int     rc;
    size_t  cbToSend = RT_ALIGN_Z(pPktHdr->cb, ATSPKT_ALIGNMENT);
    do  rc = RTTcpWrite(pClient->hTcpClient, pPktHdr, cbToSend);
    while (rc == VERR_INTERRUPTED);

    /*
     * Disconnect the client.
     */
    LogFunc(("atsTcpDisconnectClient(%RTsock) (RTTcpWrite rc=%Rrc)\n", pClient->hTcpClient, rc));
    atsTcpDisconnectClient(pThis, pClient);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnSendPkt}
 */
static DECLCALLBACK(int) atsTcpSendPkt(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    AssertReturn(pPktHdr->cb >= sizeof(ATSPKTHDR), VERR_INVALID_PARAMETER);

    /*
     * Write it.
     */
    size_t cbToSend = RT_ALIGN_Z(pPktHdr->cb, ATSPKT_ALIGNMENT);

    Log3Func(("%RU32 -> %zu\n", pPktHdr->cb, cbToSend));

    Log3Func(("Header:\n"
              "%.*Rhxd\n", RT_MIN(sizeof(ATSPKTHDR), cbToSend), pPktHdr));

    if (cbToSend > sizeof(ATSPKTHDR))
        Log3Func(("Payload:\n"
                  "%.*Rhxd\n",
                  RT_MIN(64, cbToSend - sizeof(ATSPKTHDR)), (uint8_t *)pPktHdr + sizeof(ATSPKTHDR)));

    int rc = RTTcpWrite(pClient->hTcpClient, pPktHdr, cbToSend);
    if (    RT_FAILURE(rc)
        &&  rc != VERR_INTERRUPTED)
    {
        /* assume fatal connection error. */
        LogFunc(("RTTcpWrite -> %Rrc -> atsTcpDisconnectClient(%RTsock)\n", rc, pClient->hTcpClient));
        atsTcpDisconnectClient(pThis, pClient);
    }

    return rc;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnRecvPkt}
 */
static DECLCALLBACK(int) atsTcpRecvPkt(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PPATSPKTHDR ppPktHdr)
{
    int rc = VINF_SUCCESS;
    *ppPktHdr = NULL;

    /*
     * Read state.
     */
    size_t      offData       = 0;
    size_t      cbData        = 0;
    size_t      cbDataAlloced;
    uint8_t    *pbData        = NULL;

    /*
     * Any stashed data?
     */
    if (pClient->cbTcpStashedAlloced)
    {
        offData               = pClient->cbTcpStashed;
        cbDataAlloced         = pClient->cbTcpStashedAlloced;
        pbData                = pClient->pbTcpStashed;

        pClient->cbTcpStashed        = 0;
        pClient->cbTcpStashedAlloced = 0;
        pClient->pbTcpStashed        = NULL;
    }
    else
    {
        cbDataAlloced = RT_ALIGN_Z(64,  ATSPKT_ALIGNMENT);
        pbData = (uint8_t *)RTMemAlloc(cbDataAlloced);
        if (!pbData)
            return VERR_NO_MEMORY;
    }

    /*
     * Read and validate the length.
     */
    while (offData < sizeof(uint32_t))
    {
        size_t cbRead;
        rc = RTTcpRead(pClient->hTcpClient, pbData + offData, sizeof(uint32_t) - offData, &cbRead);
        if (RT_FAILURE(rc))
            break;
        if (cbRead == 0)
        {
            LogFunc(("RTTcpRead -> %Rrc / cbRead=0 -> VERR_NET_NOT_CONNECTED (#1)\n", rc));
            rc = VERR_NET_NOT_CONNECTED;
            break;
        }
        offData += cbRead;
    }
    if (RT_SUCCESS(rc))
    {
        ASMCompilerBarrier(); /* paranoia^3 */
        cbData = *(uint32_t volatile *)pbData;
        if (cbData >= sizeof(ATSPKTHDR) && cbData <= ATSPKT_MAX_SIZE)
        {
            /*
             * Align the length and reallocate the return packet it necessary.
             */
            cbData = RT_ALIGN_Z(cbData, ATSPKT_ALIGNMENT);
            if (cbData > cbDataAlloced)
            {
                void *pvNew = RTMemRealloc(pbData, cbData);
                if (pvNew)
                {
                    pbData = (uint8_t *)pvNew;
                    cbDataAlloced = cbData;
                }
                else
                    rc = VERR_NO_MEMORY;
            }
            if (RT_SUCCESS(rc))
            {
                /*
                 * Read the remainder of the data.
                 */
                while (offData < cbData)
                {
                    size_t cbRead;
                    rc = RTTcpRead(pClient->hTcpClient, pbData + offData, cbData - offData, &cbRead);
                    if (RT_FAILURE(rc))
                        break;
                    if (cbRead == 0)
                    {
                        LogFunc(("RTTcpRead -> %Rrc / cbRead=0 -> VERR_NET_NOT_CONNECTED (#2)\n", rc));
                        rc = VERR_NET_NOT_CONNECTED;
                        break;
                    }

                    offData += cbRead;
                }

                Log3Func(("Header:\n"
                          "%.*Rhxd\n", sizeof(ATSPKTHDR), pbData));

                if (   RT_SUCCESS(rc)
                    && cbData > sizeof(ATSPKTHDR))
                    Log3Func(("Payload:\n"
                              "%.*Rhxd\n", RT_MIN(64, cbData - sizeof(ATSPKTHDR)), (uint8_t *)pbData + sizeof(ATSPKTHDR)));
            }
        }
        else
            rc = VERR_NET_PROTOCOL_ERROR;
    }
    if (RT_SUCCESS(rc))
        *ppPktHdr = (PATSPKTHDR)pbData;
    else
    {
        /*
         * Deal with errors.
         */
        if (rc == VERR_INTERRUPTED)
        {
            /* stash it away for the next call. */
            pClient->cbTcpStashed        = cbData;
            pClient->cbTcpStashedAlloced = cbDataAlloced;
            pClient->pbTcpStashed        = pbData;
        }
        else
        {
            RTMemFree(pbData);

            /* assume fatal connection error. */
            LogFunc(("RTTcpRead -> %Rrc -> atsTcpDisconnectClient(%RTsock)\n", rc, pClient->hTcpClient));
            atsTcpDisconnectClient(pThis, pClient);
        }
    }

    return rc;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnPollSetAdd}
 */
static DECLCALLBACK(int) atsTcpPollSetAdd(PATSTRANSPORTINST pThis, RTPOLLSET hPollSet, PATSTRANSPORTCLIENT pClient, uint32_t idStart)
{
    RT_NOREF(pThis);
    return RTPollSetAddSocket(hPollSet, pClient->hTcpClient, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, idStart);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnPollSetRemove}
 */
static DECLCALLBACK(int) atsTcpPollSetRemove(PATSTRANSPORTINST pThis, RTPOLLSET hPollSet, PATSTRANSPORTCLIENT pClient, uint32_t idStart)
{
    RT_NOREF(pThis, pClient);
    return RTPollSetRemove(hPollSet, idStart);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnPollIn}
 */
static DECLCALLBACK(bool) atsTcpPollIn(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient)
{
    RT_NOREF(pThis);
    int rc = RTTcpSelectOne(pClient->hTcpClient, 0/*cMillies*/);
    return RT_SUCCESS(rc);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnTerm}
 */
static DECLCALLBACK(void) atsTcpTerm(PATSTRANSPORTINST pThis)
{
    /* Signal thread */
    if (RTCritSectIsInitialized(&pThis->CritSect))
    {
        RTCritSectEnter(&pThis->CritSect);
        pThis->fStopConnecting = true;
        RTCritSectLeave(&pThis->CritSect);
    }

    if (pThis->hThreadConnect != NIL_RTTHREAD)
    {
        RTThreadUserSignal(pThis->hThreadConnect);
        RTTcpClientCancelConnect(&pThis->pConnectCancelCookie);
    }

    /* Shut down the server (will wake up thread). */
    if (pThis->pTcpServer)
    {
        LogFunc(("Destroying server...\n"));
        int rc = RTTcpServerDestroy(pThis->pTcpServer);
        if (RT_FAILURE(rc))
            RTMsgInfo("RTTcpServerDestroy failed in atsTcpTerm: %Rrc", rc);
        pThis->pTcpServer        = NULL;
    }

    /* Wait for the thread (they should've had some time to quit by now). */
    atsTcpConnectWaitOnThreads(pThis, 15000);

    LogFunc(("Done\n"));
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnCreate}
 */
static DECLCALLBACK(int) atsTcpCreate(PATSTRANSPORTINST *ppThis)
{
    PATSTRANSPORTINST pThis = (PATSTRANSPORTINST)RTMemAllocZ(sizeof(ATSTRANSPORTINST));
    AssertPtrReturn(pThis, VERR_NO_MEMORY);

    int rc = RTCritSectInit(&pThis->CritSect);
    if (RT_SUCCESS(rc))
    {
        *ppThis = pThis;
    }

    return rc;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnDestroy}
 */
static DECLCALLBACK(int) atsTcpDestroy(PATSTRANSPORTINST pThis)
{
    /* Finally, clean up the critical section. */
    if (RTCritSectIsInitialized(&pThis->CritSect))
        RTCritSectDelete(&pThis->CritSect);

    RTMemFree(pThis);

    return VINF_SUCCESS;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnStart}
 */
static DECLCALLBACK(int) atsTcpStart(PATSTRANSPORTINST pThis)
{
    int rc = VINF_SUCCESS;

    if (pThis->enmMode != ATSTCPMODE_CLIENT)
    {
        rc = RTTcpServerCreateEx(pThis->szBindAddr[0] ? pThis->szBindAddr : NULL, pThis->uBindPort, &pThis->pTcpServer);
        if (RT_FAILURE(rc))
        {
            if (rc == VERR_NET_DOWN)
            {
                RTMsgInfo("RTTcpServerCreateEx(%s, %u,) failed: %Rrc, retrying for 20 seconds...\n",
                          pThis->szBindAddr[0] ? pThis->szBindAddr : NULL, pThis->uBindPort, rc);
                uint64_t StartMs = RTTimeMilliTS();
                do
                {
                    RTThreadSleep(1000);
                    rc = RTTcpServerCreateEx(pThis->szBindAddr[0] ? pThis->szBindAddr : NULL, pThis->uBindPort, &pThis->pTcpServer);
                } while (   rc == VERR_NET_DOWN
                         && RTTimeMilliTS() - StartMs < 20000);
                if (RT_SUCCESS(rc))
                    RTMsgInfo("RTTcpServerCreateEx succceeded.\n");
            }

            if (RT_FAILURE(rc))
            {
                RTMsgError("RTTcpServerCreateEx(%s, %u,) failed: %Rrc\n",
                           pThis->szBindAddr[0] ? pThis->szBindAddr : NULL, pThis->uBindPort, rc);
            }
        }
    }

    return rc;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnOption}
 */
static DECLCALLBACK(int) atsTcpOption(PATSTRANSPORTINST pThis, int ch, PCRTGETOPTUNION pVal)
{
    int rc;

    switch (ch)
    {
        case ATSTCPOPT_MODE:
            if (!strcmp(pVal->psz, "both"))
                pThis->enmMode = ATSTCPMODE_BOTH;
            else if (!strcmp(pVal->psz, "client"))
                pThis->enmMode = ATSTCPMODE_CLIENT;
            else if (!strcmp(pVal->psz, "server"))
                pThis->enmMode = ATSTCPMODE_SERVER;
            else
                return RTMsgErrorRc(VERR_INVALID_PARAMETER, "Invalid TCP mode: '%s'\n", pVal->psz);
            return VINF_SUCCESS;

        case ATSTCPOPT_BIND_ADDRESS:
            rc = RTStrCopy(pThis->szBindAddr, sizeof(pThis->szBindAddr), pVal->psz);
            if (RT_FAILURE(rc))
                return RTMsgErrorRc(VERR_INVALID_PARAMETER, "TCP bind address is too long (%Rrc)", rc);
            return VINF_SUCCESS;

        case ATSTCPOPT_BIND_PORT:
            pThis->uBindPort = pVal->u16 == 0 ? ATS_TCP_DEF_BIND_PORT_GUEST : pVal->u16;
            return VINF_SUCCESS;

        case ATSTCPOPT_CONNECT_ADDRESS:
            rc = RTStrCopy(pThis->szConnectAddr, sizeof(pThis->szConnectAddr), pVal->psz);
            if (RT_FAILURE(rc))
                return RTMsgErrorRc(VERR_INVALID_PARAMETER, "TCP connect address is too long (%Rrc)", rc);
            if (!pThis->szConnectAddr[0])
                strcpy(pThis->szConnectAddr, ATS_TCP_DEF_CONNECT_GUEST_STR);
            return VINF_SUCCESS;

        case ATSTCPOPT_CONNECT_PORT:
            pThis->uConnectPort = pVal->u16 == 0 ? ATS_TCP_DEF_BIND_PORT_GUEST : pVal->u16;
            return VINF_SUCCESS;

        default:
            break;
    }
    return VERR_TRY_AGAIN;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnUsage}
 */
DECLCALLBACK(void) atsTcpUsage(PRTSTREAM pStream)
{
    RTStrmPrintf(pStream,
                 "  --tcp-mode <both|client|server>\n"
                 "       Selects the mode of operation.\n"
                 "       Default: both\n"
                 "  --tcp-bind-address <address>\n"
                 "       The address(es) to listen to TCP connection on.  Empty string\n"
                 "       means any address, this is the default.\n"
                 "  --tcp-bind-port <port>\n"
                 "       The port to listen to TCP connections on.\n"
                 "       Default: %u\n"
                 "  --tcp-connect-address <address>\n"
                 "       The address of the server to try connect to in client mode.\n"
                 "       Default: " ATS_TCP_DEF_CONNECT_GUEST_STR "\n"
                 "  --tcp-connect-port <port>\n"
                 "       The port on the server to connect to in client mode.\n"
                 "       Default: %u\n"
                 , ATS_TCP_DEF_BIND_PORT_GUEST, ATS_TCP_DEF_CONNECT_PORT_GUEST);
}

/** Command line options for the TCP/IP transport layer. */
static const RTGETOPTDEF  g_TcpOpts[] =
{
    { "--tcp-mode",             ATSTCPOPT_MODE,             RTGETOPT_REQ_STRING },
    { "--tcp-bind-address",     ATSTCPOPT_BIND_ADDRESS,     RTGETOPT_REQ_STRING },
    { "--tcp-bind-port",        ATSTCPOPT_BIND_PORT,        RTGETOPT_REQ_UINT16 },
    { "--tcp-connect-address",  ATSTCPOPT_CONNECT_ADDRESS,  RTGETOPT_REQ_STRING },
    { "--tcp-connect-port",     ATSTCPOPT_CONNECT_PORT,     RTGETOPT_REQ_UINT16 }
};

/** TCP/IP transport layer. */
const ATSTRANSPORT g_TcpTransport =
{
    /* .szName            = */ "tcp",
    /* .pszDesc           = */ "TCP/IP",
    /* .cOpts             = */ &g_TcpOpts[0],
    /* .paOpts            = */ RT_ELEMENTS(g_TcpOpts),
    /* .pfnUsage          = */ atsTcpUsage,
    /* .pfnCreate         = */ atsTcpCreate,
    /* .pfnDestroy        = */ atsTcpDestroy,
    /* .pfnOption         = */ atsTcpOption,
    /* .pfnStart          = */ atsTcpStart,
    /* .pfnTerm           = */ atsTcpTerm,
    /* .pfnWaitForConnect = */ atsTcpWaitForConnect,
    /* .pfnPollIn         = */ atsTcpPollIn,
    /* .pfnPollSetAdd     = */ atsTcpPollSetAdd,
    /* .pfnPollSetRemove  = */ atsTcpPollSetRemove,
    /* .pfnRecvPkt        = */ atsTcpRecvPkt,
    /* .pfnSendPkt        = */ atsTcpSendPkt,
    /* .pfnBabble         = */ atsTcpBabble,
    /* .pfnNotifyHowdy    = */ atsTcpNotifyHowdy,
    /* .pfnNotifyBye      = */ atsTcpNotifyBye,
    /* .pfnNotifyReboot   = */ atsTcpNotifyReboot,
    /* .u32EndMarker      = */ UINT32_C(0x12345678)
};

