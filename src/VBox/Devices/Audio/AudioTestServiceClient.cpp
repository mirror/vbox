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
#include <VBox/log.h>

#include <iprt/crc.h>
#include <iprt/err.h>
#include <iprt/file.h>
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
    char   szOp[ATSPKT_OPCODE_MAX_LEN];
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
    int rc;

    ATSPKTHDR Hdr;
    size_t    cbHdr = 0;
    if (pClient->cbHdr)
    {
        memcpy(&Hdr, &pClient->abHdr, sizeof(Hdr));
        cbHdr = pClient->cbHdr;
        pClient->cbHdr = 0;
        rc = VINF_SUCCESS;
    }
    else
    {
        rc = RTTcpRead(pClient->hSock, &Hdr, sizeof(Hdr), &cbHdr);
        LogFlowFunc(("rc=%Rrc, hdr=%zu, hdr.cb=%zu, %.8s -> %.*Rhxd\n", rc, sizeof(Hdr), Hdr.cb, Hdr.achOpcode, RT_MIN(cbHdr, sizeof(ATSPKTHDR)), &Hdr));
    }

    if (cbHdr != sizeof(Hdr)) /* Check for bad packet sizes. */
        return VERR_NET_PROTOCOL_ERROR;

    if (RT_SUCCESS(rc))
    {
        if (Hdr.cb < sizeof(ATSPKTHDR))
            return VERR_NET_PROTOCOL_ERROR;

        if (Hdr.cb > ATSPKT_MAX_SIZE)
            return VERR_NET_PROTOCOL_ERROR;

        /** @todo Check opcode encoding. */

        uint32_t cbPadding;
        if (Hdr.cb % ATSPKT_ALIGNMENT)
            cbPadding = ATSPKT_ALIGNMENT - (Hdr.cb % ATSPKT_ALIGNMENT);
        else
            cbPadding = 0;

        if (Hdr.cb > sizeof(ATSPKTHDR))
        {
            pReply->cbPayload = (Hdr.cb - sizeof(ATSPKTHDR)) + cbPadding;
            Log3Func(("cbPadding=%RU32, cbPayload: %RU32 -> %zu\n", cbPadding, Hdr.cb - sizeof(ATSPKTHDR), pReply->cbPayload));
            AssertReturn(pReply->cbPayload <= ATSPKT_MAX_SIZE, VERR_BUFFER_OVERFLOW); /* Paranoia. */
        }

        if (pReply->cbPayload)
        {
            pReply->pvPayload = RTMemAlloc(pReply->cbPayload);
            if (pReply->pvPayload)
            {
                size_t cbRead = 0;
                rc = RTTcpRead(pClient->hSock, pReply->pvPayload, pReply->cbPayload, &cbRead);
                if (RT_SUCCESS(rc))
                {
                    Log3Func(("cbPayload=%zu -> cbRead=%zu\n", pReply->cbPayload, cbRead));
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
                }

                if (RT_SUCCESS(rc))
                {
                    /** @todo Check CRC-32. */
                    pReply->cbPayload = cbRead;
                    /** @todo Re-allocate pvPayload to not store padding? */
                }
                else
                    audioTestSvcClientReplyDestroy(pReply);
            }
            else
                rc = VERR_NO_MEMORY;
        }

        if (RT_SUCCESS(rc))
        {
            memcpy(pReply->szOp, Hdr.achOpcode, sizeof(pReply->szOp));
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
        if (RTStrNCmp(Reply.szOp, "ACK     ", ATSPKT_OPCODE_MAX_LEN) != 0)
            rc = VERR_NET_PROTOCOL_ERROR;

        audioTestSvcClientReplyDestroy(&Reply);
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

    return audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
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

    int rc = RTTcpClientConnect(pszAddr ? pszAddr : ATS_TCP_HOST_DEFAULT_ADDR_STR, uPort == 0 ? ATS_TCP_HOST_DEFAULT_PORT : uPort, &pClient->hSock);
    if (RT_SUCCESS(rc))
    {
        rc = audioTestSvcClientDoGreet(pClient);
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

    rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
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

    rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
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

    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
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

    int rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
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

    rc = audioTestSvcClientSendMsg(pClient, &Req, sizeof(Req), NULL, 0);
    while (RT_SUCCESS(rc))
    {
        ATSSRVREPLY Reply;
        RT_ZERO(Reply);

        rc = audioTestSvcClientRecvReply(pClient, &Reply, false /* fNoDataOk */);
        AssertRCBreak(rc);

        if (   RTStrNCmp(Reply.szOp, "DATA    ", ATSPKT_OPCODE_MAX_LEN) == 0
            && Reply.pvPayload
            && Reply.cbPayload)
        {
            /** @todo Skip uCrc32 for now. */
            rc = RTFileWrite(hFile, (uint8_t *)Reply.pvPayload + 4, Reply.cbPayload - 4, NULL);
        }
        else if (RTStrNCmp(Reply.szOp, "DATA EOF", ATSPKT_OPCODE_MAX_LEN) == 0)
        {
            rc = VINF_EOF;
        }
        else
        {
            AssertMsgFailed(("Got unexpected reply '%s'", Reply.szOp));
            rc = VERR_NET_PROTOCOL_ERROR;
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
    int rc = audioTestSvcClientDoBye(pClient);
    if (RT_SUCCESS(rc))
        rc = RTTcpClientClose(pClient->hSock);

    return rc;
}

