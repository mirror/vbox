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
#define LOG_GROUP RTLOGGROUP_DEFAULT
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
    RTSOCKET             hTcpClient;
    /** The size of the stashed data. */
    size_t               cbTcpStashed;
    /** The size of the stashed data allocation. */
    size_t               cbTcpStashedAlloced;
    /** The stashed data. */
    uint8_t             *pbTcpStashed;
} ATSTRANSPORTCLIENT;


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
        int rc = RTTcpServerDisconnectClient2(pClient->hTcpClient);
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
 * @interface_method_impl{ATSTRANSPORT,pfnWaitForConnect}
 */
static DECLCALLBACK(int) atsTcpWaitForConnect(PATSTRANSPORTINST pThis, PPATSTRANSPORTCLIENT ppClientNew)
{
    int rc;
    RTSOCKET hClientNew;

    rc = RTTcpServerListen2(pThis->pTcpServer, &hClientNew);
    Log(("atsTcpWaitForConnect: RTTcpServerListen2 -> %Rrc\n", rc));

    if (RT_SUCCESS(rc))
    {
        PATSTRANSPORTCLIENT pClient = (PATSTRANSPORTCLIENT)RTMemAllocZ(sizeof(ATSTRANSPORTCLIENT));
        if (RT_LIKELY(pClient))
        {
            pClient->hTcpClient          = hClientNew;
            pClient->cbTcpStashed        = 0;
            pClient->cbTcpStashedAlloced = 0;
            pClient->pbTcpStashed        = NULL;
            *ppClientNew = pClient;
        }
        else
        {
            RTTcpServerDisconnectClient2(hClientNew);
            rc = VERR_NO_MEMORY;
        }
    }

    return rc;
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnNotifyReboot}
 */
static DECLCALLBACK(void) atsTcpNotifyReboot(PATSTRANSPORTINST pThis)
{
    Log(("atsTcpNotifyReboot: RTTcpServerDestroy(%p)\n", pThis->pTcpServer));
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
    Log(("atsTcpNotifyBye: atsTcpDisconnectClient %RTsock\n", pClient->hTcpClient));
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
    Log(("atsTcpBabble: atsTcpDisconnectClient(%RTsock) (RTTcpWrite rc=%Rrc)\n", pClient->hTcpClient, rc));
    atsTcpDisconnectClient(pThis, pClient);
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnSendPkt}
 */
static DECLCALLBACK(int) atsTcpSendPkt(PATSTRANSPORTINST pThis, PATSTRANSPORTCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    Assert(pPktHdr->cb >= sizeof(ATSPKTHDR));

    /*
     * Write it.
     */
    size_t cbToSend = RT_ALIGN_Z(pPktHdr->cb, ATSPKT_ALIGNMENT);
    LogFlowFunc(("%RU32 -> %zu\n", pPktHdr->cb, cbToSend));
    int rc = RTTcpWrite(pClient->hTcpClient, pPktHdr, cbToSend);
    if (    RT_FAILURE(rc)
        &&  rc != VERR_INTERRUPTED)
    {
        /* assume fatal connection error. */
        Log(("RTTcpWrite -> %Rrc -> atsTcpDisconnectClient(%RTsock)\n", rc, pClient->hTcpClient));
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
     * Read and valid the length.
     */
    while (offData < sizeof(uint32_t))
    {
        size_t cbRead;
        rc = RTTcpRead(pClient->hTcpClient, pbData + offData, sizeof(uint32_t) - offData, &cbRead);
        if (RT_FAILURE(rc))
            break;
        if (cbRead == 0)
        {
            Log(("atsTcpRecvPkt: RTTcpRead -> %Rrc / cbRead=0 -> VERR_NET_NOT_CONNECTED (#1)\n", rc));
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
                        Log(("atsTcpRecvPkt: RTTcpRead -> %Rrc / cbRead=0 -> VERR_NET_NOT_CONNECTED (#2)\n", rc));
                        rc = VERR_NET_NOT_CONNECTED;
                        break;
                    }
                    offData += cbRead;
                }
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
            Log(("atsTcpRecvPkt: RTTcpRead -> %Rrc -> atsTcpDisconnectClient(%RTsock)\n", rc, pClient->hTcpClient));
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
    /* Shut down the server (will wake up thread). */
    if (pThis->pTcpServer)
    {
        Log(("atsTcpTerm: Destroying server...\n"));
        int rc = RTTcpServerDestroy(pThis->pTcpServer);
        if (RT_FAILURE(rc))
            RTMsgInfo("RTTcpServerDestroy failed in atsTcpTerm: %Rrc", rc);
        pThis->pTcpServer        = NULL;
    }

    Log(("atsTcpTerm: done\n"));
}

/**
 * @interface_method_impl{ATSTRANSPORT,pfnInit}
 */
static DECLCALLBACK(int) atsTcpInit(PATSTRANSPORTINST pThis, const char *pszBindAddr, uint32_t uBindPort)
{
    int rc = RTTcpServerCreateEx(pszBindAddr[0] ? pszBindAddr : NULL, uBindPort, &pThis->pTcpServer);
    if (RT_FAILURE(rc))
    {
        if (rc == VERR_NET_DOWN)
        {
            RTMsgInfo("RTTcpServerCreateEx(%s, %u,) failed: %Rrc, retrying for 20 seconds...\n",
                      pszBindAddr[0] ? pszBindAddr : NULL, uBindPort, rc);
            uint64_t StartMs = RTTimeMilliTS();
            do
            {
                RTThreadSleep(1000);
                rc = RTTcpServerCreateEx(pszBindAddr[0] ? pszBindAddr : NULL, uBindPort, &pThis->pTcpServer);
            } while (   rc == VERR_NET_DOWN
                     && RTTimeMilliTS() - StartMs < 20000);
            if (RT_SUCCESS(rc))
                RTMsgInfo("RTTcpServerCreateEx succceeded.\n");
        }
        if (RT_FAILURE(rc))
        {
            pThis->pTcpServer = NULL;
            RTMsgError("RTTcpServerCreateEx(%s, %u,) failed: %Rrc\n",
                       pszBindAddr[0] ? pszBindAddr : NULL, uBindPort, rc);
        }
    }

    return rc;
}

/** TCP/IP transport layer. */
const ATSTRANSPORT g_TcpTransport =
{
    /* .szName            = */ "tcp",
    /* .pszDesc           = */ "TCP/IP",
    /* .pfnInit           = */ atsTcpInit,
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

