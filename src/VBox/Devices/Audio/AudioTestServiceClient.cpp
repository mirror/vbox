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
#define LOG_GROUP LOG_GROUP_DRV_HOST_AUDIO /** @todo Add an own log group for this? */

#include <iprt/crc.h>
#include <iprt/err.h>
#include <iprt/file.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/tcp.h>

#include <VBox/log.h>

#include "AudioTestService.h"
#include "AudioTestServiceInternal.h"
#include "AudioTestServiceClient.h"

/** @todo Use common defines between server protocol and this client. */

/**
 * A generic ATS reply, used by the client
 * to process the incoming packets.
 */
typedef struct ATSSRVREPLY
{
    char   szOp[ATSPKT_OPCODE_MAX_LEN];
    /** Pointer to payload data.
     *  This does *not* include the header! */
    void  *pvPayload;
    /** Size (in bytes) of the payload data.
     *  This does *not* include the header! */
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
    RT_BZERO(pClient, sizeof(ATSCLIENT));
}

/**
 * Destroys an ATS server reply.
 *
 * @param   pReply              Reply to destroy.
 */
static void audioTestSvcClientReplyDestroy(PATSSRVREPLY pReply)
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
 *                              The reply must be destroyed with audioTestSvcClientReplyDestroy() then.
 * @param   fNoDataOk           If it's okay that the reply is not expected to have any payload.
 */
static int audioTestSvcClientRecvReply(PATSCLIENT pClient, PATSSRVREPLY pReply, bool fNoDataOk)
{
    LogFlowFuncEnter();

    PATSPKTHDR pPktHdr;
    int rc = pClient->pTransport->pfnRecvPkt(pClient->pTransportInst, pClient->pTransportClient, &pPktHdr);
    if (RT_SUCCESS(rc))
    {
        AssertReturn(pPktHdr->cb >= sizeof(ATSPKTHDR), VERR_NET_PROTOCOL_ERROR);
        pReply->cbPayload = pPktHdr->cb - sizeof(ATSPKTHDR);
        Log3Func(("szOp=%.8s, cb=%RU32\n", pPktHdr->achOpcode, pPktHdr->cb));
        if (pReply->cbPayload)
        {
            pReply->pvPayload = RTMemDup((uint8_t *)pPktHdr + sizeof(ATSPKTHDR), pReply->cbPayload);
        }
        else
            pReply->pvPayload = NULL;

        if (   !pReply->cbPayload
            && !fNoDataOk)
        {
            rc = VERR_NET_PROTOCOL_ERROR;
        }
        else
        {
            memcpy(&pReply->szOp, &pPktHdr->achOpcode, sizeof(pReply->szOp));
        }

        RTMemFree(pPktHdr);
        pPktHdr = NULL;
    }

    LogFlowFuncLeaveRC(rc);
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
        if (RTStrNCmp(Reply.szOp, "ACK     ", ATSPKT_OPCODE_MAX_LEN) != 0)
            rc = VERR_NET_PROTOCOL_ERROR;

        audioTestSvcClientReplyDestroy(&Reply);
    }

    return rc;
}

/**
 * Sends a message plus optional payload to an ATS server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to send message for.
 * @param   pvHdr               Pointer to header data to send.
 * @param   cbHdr               Size (in bytes) of \a pvHdr to send.
 */
static int audioTestSvcClientSendMsg(PATSCLIENT pClient, void *pvHdr, size_t cbHdr)
{
    RT_NOREF(cbHdr);
    AssertPtrReturn(pClient->pTransport,       VERR_WRONG_ORDER);
    AssertPtrReturn(pClient->pTransportInst,   VERR_WRONG_ORDER);
    AssertPtrReturn(pClient->pTransportClient, VERR_NET_NOT_CONNECTED);
    return pClient->pTransport->pfnSendPkt(pClient->pTransportInst, pClient->pTransportClient, (PCATSPKTHDR)pvHdr);
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
    AssertReturnVoid(strlen(pszOp) <= ATSPKT_OPCODE_MAX_LEN);

    /** @todo Validate opcode. */

    RT_BZERO(pReqHdr, sizeof(ATSPKTHDR));

    memcpy(pReqHdr->achOpcode, pszOp, strlen(pszOp));
    pReqHdr->uCrc32 = 0; /** @todo Do CRC-32 calculation. */
    pReqHdr->cb     = (uint32_t)cbReq + (uint32_t)cbPayload;

    Assert(pReqHdr->cb <= ATSPKT_MAX_SIZE);
}

/**
 * Sends an acknowledege response back to the server.
 *
 * @returns VBox status code.
 * @param   pClient             Client to send command for.
 */
static int audioTestSvcClientSendAck(PATSCLIENT pClient)
{
    ATSPKTHDR Req;
    audioTestSvcClientReqHdrInit(&Req, sizeof(Req), "ACK     ", 0);

    return audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
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
    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);
    return rc;
}

/**
 * Creates an ATS client.
 *
 * @returns VBox status code.
 * @param   pClient             Client to create.
 */
int AudioTestSvcClientCreate(PATSCLIENT pClient)
{
    audioTestSvcClientInit(pClient);

    /*
     * The default transporter is the first one.
     */
    pClient->pTransport = g_apTransports[0]; /** @todo Make this dynamic. */

    return pClient->pTransport->pfnCreate(&pClient->pTransportInst);
}

/**
 * Destroys an ATS client.
 *
 * @returns VBox status code.
 * @param   pClient             Client to destroy.
 */
void AudioTestSvcClientDestroy(PATSCLIENT pClient)
{
    if (pClient->pTransport)
        pClient->pTransport->pfnTerm(pClient->pTransportInst);
}

/**
 * Handles a command line option.
 *
 * @returns VBox status code.
 * @param   pClient             Client to handle option for.
 * @param   ch                  Option (short) to handle.
 * @param   pVal                Option union to store the result in on success.
 */
int AudioTestSvcClientHandleOption(PATSCLIENT pClient, int ch, PCRTGETOPTUNION pVal)
{
    AssertPtrReturn(pClient->pTransport, VERR_WRONG_ORDER); /* Must be created first via AudioTestSvcClientCreate(). */
    if (!pClient->pTransport->pfnOption)
        return VERR_GETOPT_UNKNOWN_OPTION;
    return pClient->pTransport->pfnOption(pClient->pTransportInst, ch, pVal);
}

/**
 * Connects to an ATS peer.
 *
 * @returns VBox status code.
 * @param   pClient             Client to connect.
 */
int AudioTestSvcClientConnect(PATSCLIENT pClient)
{
    int rc = pClient->pTransport->pfnStart(pClient->pTransportInst);
    if (RT_SUCCESS(rc))
    {
        rc = pClient->pTransport->pfnWaitForConnect(pClient->pTransportInst, &pClient->pTransportClient);
        if (RT_SUCCESS(rc))
        {
            rc = audioTestSvcClientDoGreet(pClient);
        }
    }

    return rc;
}

/**
 * Tells the server to begin a new test set.
 *
 * @returns VBox status code.
 * @param   pClient             Client to issue command for.
 * @param   pszTag              Tag to use for the test set to begin.
 */
int AudioTestSvcClientTestSetBegin(PATSCLIENT pClient, const char *pszTag)
{
    ATSPKTREQTSETBEG Req;

    int rc = RTStrCopy(Req.szTag, sizeof(Req.szTag), pszTag);
    AssertRCReturn(rc, rc);

    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_TESTSET_BEGIN, 0);

    rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

/**
 * Tells the server to end a runing test set.
 *
 * @returns VBox status code.
 * @param   pClient             Client to issue command for.
 * @param   pszTag              Tag of test set to end.
 */
int AudioTestSvcClientTestSetEnd(PATSCLIENT pClient, const char *pszTag)
{
    ATSPKTREQTSETEND Req;

    int rc = RTStrCopy(Req.szTag, sizeof(Req.szTag), pszTag);
    AssertRCReturn(rc, rc);

    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_TESTSET_END, 0);

    rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

/**
 * Tells the server to play a (test) tone.
 *
 * @returns VBox status code.
 * @param   pClient             Client to issue command for.
 * @param   pToneParms          Tone parameters to use.
 * @note    How (and if) the server plays a tone depends on the actual implementation side.
 */
int AudioTestSvcClientTonePlay(PATSCLIENT pClient, PAUDIOTESTTONEPARMS pToneParms)
{
    ATSPKTREQTONEPLAY Req;

    memcpy(&Req.ToneParms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_TONE_PLAY, 0);

    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

/**
 * Tells the server to record a (test) tone.
 *
 * @returns VBox status code.
 * @param   pClient             Client to issue command for.
 * @param   pToneParms          Tone parameters to use.
 * @note    How (and if) the server plays a tone depends on the actual implementation side.
 */
int AudioTestSvcClientToneRecord(PATSCLIENT pClient, PAUDIOTESTTONEPARMS pToneParms)
{
    ATSPKTREQTONEREC Req;

    memcpy(&Req.ToneParms, pToneParms, sizeof(AUDIOTESTTONEPARMS));

    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_TONE_RECORD, 0);

    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
    if (RT_SUCCESS(rc))
        rc = audioTestSvcClientRecvAck(pClient);

    return rc;
}

/**
 * Tells the server to send (download) a (packed up) test set archive.
 * The test set must not be running / open anymore.
 *
 * @returns VBox status code.
 * @param   pClient             Client to issue command for.
 * @param   pszTag              Tag of test set to send.
 * @param   pszPathOutAbs       Absolute path where to store the downloaded test set archive.
 */
int AudioTestSvcClientTestSetDownload(PATSCLIENT pClient, const char *pszTag, const char *pszPathOutAbs)
{
    ATSPKTREQTSETSND Req;

    int rc = RTStrCopy(Req.szTag, sizeof(Req.szTag), pszTag);
    AssertRCReturn(rc, rc);

    audioTestSvcClientReqHdrInit(&Req.Hdr, sizeof(Req), ATSPKT_OPCODE_TESTSET_SEND, 0);

    RTFILE hFile;
    rc = RTFileOpen(&hFile, pszPathOutAbs, RTFILE_O_WRITE | RTFILE_O_CREATE | RTFILE_O_DENY_WRITE);
    AssertRCReturn(rc, rc);

    rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req));
    while (RT_SUCCESS(rc))
    {
        ATSSRVREPLY Reply;
        RT_ZERO(Reply);

        rc = audioTestSvcClientRecvReply(pClient, &Reply, false /* fNoDataOk */);
        if (RT_SUCCESS(rc))
        {
            /* Extract received CRC32 checksum. */
            const size_t cbCrc32 = sizeof(uint32_t); /* Skip CRC32 in payload for actual CRC verification. */

            uint32_t uSrcCrc32;
            memcpy(&uSrcCrc32, Reply.pvPayload, cbCrc32);

            if (uSrcCrc32)
            {
                const uint32_t uDstCrc32 = RTCrc32((uint8_t *)Reply.pvPayload + cbCrc32, Reply.cbPayload - cbCrc32);

                Log2Func(("uSrcCrc32=%#x, cbRead=%zu -> uDstCrc32=%#x\n"
                          "%.*Rhxd\n",
                          uSrcCrc32, Reply.cbPayload - cbCrc32, uDstCrc32,
                          RT_MIN(64, Reply.cbPayload - cbCrc32), (uint8_t *)Reply.pvPayload + cbCrc32));

                if (uSrcCrc32 != uDstCrc32)
                    rc = VERR_TAR_CHKSUM_MISMATCH; /** @todo Fudge! */
            }

            if (RT_SUCCESS(rc))
            {
                if (   RTStrNCmp(Reply.szOp, "DATA    ", ATSPKT_OPCODE_MAX_LEN) == 0
                    && Reply.pvPayload
                    && Reply.cbPayload)
                {
                    rc = RTFileWrite(hFile, (uint8_t *)Reply.pvPayload + cbCrc32, Reply.cbPayload - cbCrc32, NULL);
                }
                else if (RTStrNCmp(Reply.szOp, "DATA EOF", ATSPKT_OPCODE_MAX_LEN) == 0)
                {
                    rc = VINF_EOF;
                }
                else
                {
                    AssertMsgFailed(("Got unexpected reply '%s'", Reply.szOp));
                    rc = VERR_NOT_SUPPORTED;
                }
            }
        }

        audioTestSvcClientReplyDestroy(&Reply);

        int rc2 = audioTestSvcClientSendAck(pClient);
        if (rc == VINF_SUCCESS) /* Might be VINF_EOF already. */
            rc = rc2;

        if (rc == VINF_EOF)
            break;
    }

    int rc2 = RTFileClose(hFile);
    if (RT_SUCCESS(rc))
        rc = rc2;

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
    pClient->pTransport->pfnNotifyBye(pClient->pTransportInst, pClient->pTransportClient);

    return VINF_SUCCESS;
}

