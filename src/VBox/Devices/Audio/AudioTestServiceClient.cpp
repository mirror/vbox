/* $Id$ */
/** @file
 * AudioTestServiceClient - Audio test execution server, Client helpers.
 *
 * Note: Only does TCP/IP as transport layer for now.
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
#include <iprt/crc.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/tcp.h>

#include "AudioTestServiceProtocol.h"
#include "AudioTestServiceClient.h"

/** @todo Use common defines between server protocol and this client. */

typedef struct ATSSRVREPLY
{
    char   szOp[8];
    void  *pvPayload;
    size_t cbPayload;

} ATSSRVREPLY;
typedef struct ATSSRVREPLY *PATSSRVREPLY;


static void audioTestSvcClientConnInit(PATSCLIENT pClient)
{
    pClient->cbHdr = 0;
    pClient->hSock = NIL_RTSOCKET;
}

static void audioTestSvcClientReplyFree(PATSSRVREPLY pReply)
{
    if (!pReply)
        return;

    if (pReply->pvPayload)
    {
        Assert(pReply->cbPayload);
        RTMemFree(pReply->pvPayload);
        pReply->pvPayload = NULL;
    }

    pReply->cbPayload = 0;
}

static int audioTestSvcClientRecvReply(PATSCLIENT pClient, PATSSRVREPLY pReply, bool fNoDataOk)
{
    int rc;

    ATSPKTHDR Hdr;
    size_t  cbHdr = 0;
    if (pClient->cbHdr)
    {
        memcpy(&Hdr, &pClient->abHdr, sizeof(Hdr));
        cbHdr = pClient->cbHdr;
        pClient->cbHdr = 0;
        rc = VINF_SUCCESS;
    }
    else
        rc = RTTcpRead(pClient->hSock, &Hdr, sizeof(Hdr), &cbHdr);

    /** @todo Use defines for all those numbers below. */

    if (cbHdr != 16)
        return VERR_NET_PROTOCOL_ERROR;

    if (RT_SUCCESS(rc))
    {
        if (Hdr.cb < 16)
            return VERR_NET_PROTOCOL_ERROR;

        if (Hdr.cb > 1024 * 1024)
            return VERR_NET_PROTOCOL_ERROR;

        /** @todo Check opcode encoding. */

        if (Hdr.cb > 16)
        {
            uint32_t cbPadding;
            if (Hdr.cb % 16)
                cbPadding = 16 - (Hdr.cb % 16);
            else
                cbPadding = 0;

            pReply->pvPayload = RTMemAlloc(Hdr.cb - 16);
            pReply->cbPayload = Hdr.cb - 16;

            size_t cbRead = 0;
            rc = RTTcpRead(pClient->hSock, pReply->pvPayload, RT_MIN(Hdr.cb - 16 + cbPadding, pReply->cbPayload), &cbRead);
            if (RT_SUCCESS(rc))
            {
                if (!cbRead)
                {
                    memcpy(&pClient->abHdr, &Hdr, sizeof(pClient->abHdr));
                    if (!fNoDataOk)
                        rc = VERR_NET_PROTOCOL_ERROR;
                }
                else
                {
                    while (cbPadding--)
                    {
                        Assert(cbRead);
                        cbRead--;
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    /** @todo Check CRC-32. */

                    memcpy(pReply->szOp, Hdr.achOpcode, sizeof(pReply->szOp));
                    pReply->cbPayload = cbRead;

                    /** @todo Re-allocate pvPayload to not store padding? */
                }
            }

            if (RT_FAILURE(rc))
                audioTestSvcClientReplyFree(pReply);
        }
    }

    return rc;
}

static int audioTestSvcClientRecvAck(PATSCLIENT pClient)
{
    ATSSRVREPLY Reply;
    RT_ZERO(Reply);

    int rc = audioTestSvcClientRecvReply(pClient, &Reply, true /* fNoDataOk */);
    if (RT_SUCCESS(rc))
    {
        if (RTStrNCmp(Reply.szOp, "ACK     ", 8) != 0) /** @todo Use protocol define. */
            rc = VERR_NET_PROTOCOL_ERROR;

        audioTestSvcClientReplyFree(&Reply);
    }

    return rc;
}

static int audioTestSvcClientSend(PATSCLIENT pClient, const void *pvData, size_t cbData)
{
    return RTTcpWrite(pClient->hSock, pvData, cbData);
}

static int audioTestSvcClientSendMsg(PATSCLIENT pClient,
                                     void *pvHdr, size_t cbHdr, const void *pvPayload, size_t cbPayload)
{
    int rc = audioTestSvcClientSend(pClient, pvHdr, cbHdr);
    if (   RT_SUCCESS(rc)
        && cbPayload)
    {
        rc = audioTestSvcClientSend(pClient, (uint8_t *)pvPayload, cbPayload);
    }

    return rc;
}

DECLINLINE (void) audioTestSvcClientReqHdrInit(PATSPKTHDR pReqHdr, size_t cbReq, const char *pszOp, size_t cbPayload)
{
    AssertReturnVoid(strlen(pszOp) >= 2);

    /** @todo Validate opcode. */

    memcpy(pReqHdr->achOpcode, pszOp, sizeof(pReqHdr->achOpcode));
    pReqHdr->uCrc32 = 0; /** @todo Do CRC-32 calculation. */
    pReqHdr->cb     = (uint32_t)cbReq + (uint32_t)cbPayload;
}

static int audioTestSvcClientDoGreet(PATSCLIENT pClient)
{
    ATSPKTREQHOWDY Req;
    Req.uVersion = ATS_PROTOCOL_VS;
    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_HOWDY, 0);
    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);
    return rc;
}

static int audioTestSvcClientDoBye(PATSCLIENT pClient)
{
    ATSPKTHDR Hdr;
    audioTestSvcClientReqHdrInit(&Hdr, sizeof(Hdr), ATSPKT_OPCODE_BYE, 0);
    int rc = audioTestSvcClientSendMsg(pClient, &Hdr, sizeof(Hdr), NULL, 0);
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

int AudioTestSvcClientConnect(PATSCLIENT pClient, const char *pszAddr)
{
    audioTestSvcClientConnInit(pClient);

    /* For simplicity we always run on the same port, localhost only. */
    int rc = RTTcpClientConnect(pszAddr ? pszAddr : "127.0.0.1", 6052, &pClient->hSock);
    if (RT_SUCCESS(rc))
    {
        rc = audioTestSvcClientDoGreet(pClient);
    }

    return rc;
}

int AudioTestSvcClientClose(PATSCLIENT pClient)
{
    int rc = audioTestSvcClientDoBye(pClient);
    if (RT_SUCCESS(rc))
        rc = RTTcpClientClose(pClient->hSock);

    return rc;
}

