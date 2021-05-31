/* $Id$ */
/** @file
 * AudioTestServiceClient - Audio Test Service (ATS), Client helpers.
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

#include "AudioTestService.h"
#include "AudioTestServiceProtocol.h"
#include "AudioTestServiceClient.h"

/** @todo Use common defines between server protocol and this client. */

/**
 * A generic ATS reply, used by the client
 * to process the incoming packets.
 */
typedef struct ATSSRVREPLY
{
    char   szOp[8];
    void  *pvPayload;
    size_t cbPayload;
} ATSSRVREPLY;
/** Pointer to a generic ATS reply. */
typedef struct ATSSRVREPLY *PATSSRVREPLY;


/**
 * Initializes an ATS client, internal version.
 *
 * @param   pClient             Client to initialize.
 */
static void audioTestSvcClientInit(PATSCLIENT pClient)
{
    pClient->cbHdr = 0;
    pClient->hSock = NIL_RTSOCKET;
}

/**
 * Free's an ATS server reply.
 * @param   pReply              Reply to free. The pointer is invalid afterwards.
 */
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

/**
 * Receives a reply from an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to receive reply for.
 * @param   pReply              Where to store the reply.
 * @param   fNoDataOk           If it's okay that the reply is not expected to have any payload.
 */
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

/**
 * Receives a reply for an ATS server and checks if it is an acknowledge (success) one.
 *
 * @returns VBox status code.
 * @retval  VERR_NET_PROTOCOL_ERROR if the reply indicates a failure.
 * @param   pClient             Client to receive reply for.
 */
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

/**
 * Sends data over the transport to the server.
 * For now only TCP/IP is implemented.
 *
 * @returns VBox status code.
 * @param   pClient             Client to send data for.
 * @param   pvData              Pointer to data to send.
 * @param   cbData              Size (in bytes) of \a pvData to send.
 */
static int audioTestSvcClientSend(PATSCLIENT pClient, const void *pvData, size_t cbData)
{
    return RTTcpWrite(pClient->hSock, pvData, cbData);
}

/**
 * Sends a message plus optional payload to an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to send message for.
 * @param   pvHdr               Pointer to header data to send.
 * @param   cbHdr               Size (in bytes) of \a pvHdr to send.
 * @param   pvPayload           Pointer to payload to send. Optional.
 * @param   cbPayload           Size (in bytes) of \a pvPayload to send. Set to 0 if no payload needed.
 */
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

/**
 * Initializes a client request header.
 *
 * @returns VBox status code.
 * @param   pReqHdr             Request header to initialize.
 * @param   cbReq               Size (in bytes) the request will have (does *not* include payload).
 * @param   pszOp               Operation to perform with the request.
 * @param   cbPayload           Size (in bytes) of payload that will follow the header. Optional and can be 0.
 */
DECLINLINE (void) audioTestSvcClientReqHdrInit(PATSPKTHDR pReqHdr, size_t cbReq, const char *pszOp, size_t cbPayload)
{
    AssertReturnVoid(strlen(pszOp) >= 2);

    /** @todo Validate opcode. */

    memcpy(pReqHdr->achOpcode, pszOp, sizeof(pReqHdr->achOpcode));
    pReqHdr->uCrc32 = 0; /** @todo Do CRC-32 calculation. */
    pReqHdr->cb     = (uint32_t)cbReq + (uint32_t)cbPayload;
}

/**
 * Sends a greeting command (handshake) to an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to send command for.
 */
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

/**
 * Sends a disconnect command to an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to send command for.
 */
static int audioTestSvcClientDoBye(PATSCLIENT pClient)
{
    ATSPKTHDR Hdr;
    audioTestSvcClientReqHdrInit(&Hdr, sizeof(Hdr), ATSPKT_OPCODE_BYE, 0);
    int rc = audioTestSvcClientSendMsg(pClient, &Hdr, sizeof(Hdr), NULL, 0);
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

/**
 * Connects to an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to connect.
 * @param   pszAddr             Address to connect to. If NULL, 127.0.0.1 (localhost) will be used.
 * @param   uPort               Port to connect. If set to 0, port 6052 (ATS_DEFAULT_PORT) will be used.
 */
int AudioTestSvcClientConnect(PATSCLIENT pClient, const char *pszAddr, uint32_t uPort)
{
    audioTestSvcClientInit(pClient);

    /* For simplicity we always run on the same port, localhost only. */
    int rc = RTTcpClientConnect(pszAddr ? pszAddr : "127.0.0.1", uPort == 0 ? ATS_DEFAULT_PORT : uPort, &pClient->hSock);
    if (RT_SUCCESS(rc))
    {
        rc = audioTestSvcClientDoGreet(pClient);
    }

    return rc;
}

/**
 * Tells the server to play a (test) tone.
 *
 * @returns VBox status code.
 * @param   pClient             Client to issue command for.
 * @param   pStreamCfg          Audio stream configuration to use.
 * @param   pToneParms          Tone parameters to use.
 * @note    How (and if) the server plays a tone depends on the actual implementation side.
 */
int AudioTestSvcClientTonePlay(PATSCLIENT pClient, PPDMAUDIOSTREAMCFG pStreamCfg, PAUDIOTESTTONEPARMS pToneParms)
{
    ATSPKTREQTONEPLAY Req;

    memcpy(&Req.StreamCfg, pStreamCfg, sizeof(PDMAUDIOSTREAMCFG));
    memcpy(&Req.ToneParms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_TONE_PLAY, 0);

    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

/**
 * Disconnects from an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to disconnect.
 */
int AudioTestSvcClientClose(PATSCLIENT pClient)
{
    int rc = audioTestSvcClientDoBye(pClient);
    if (RT_SUCCESS(rc))
        rc = RTTcpClientClose(pClient->hSock);

    return rc;
}

