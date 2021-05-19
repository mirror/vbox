/* $Id$ */
/** @file
 * AudioTestService - Audio test execution server.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/crc.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/getopt.h>
#include <iprt/handle.h>
#include <iprt/initterm.h>
#include <iprt/json.h>
#include <iprt/list.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/message.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/pipe.h>
#include <iprt/poll.h>
#include <iprt/process.h>
#include <iprt/stream.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "AudioTestService.h"
#include "AudioTestServiceInternal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Transport layers.
 */
static const PCATSTRANSPORT g_apTransports[] =
{
    &g_TcpTransport
};

/** The select transport layer. */
static PCATSTRANSPORT       g_pTransport;
/** Whether to terminate or not.
 * @todo implement signals and stuff.  */
static bool volatile        g_fTerminate = false;
/** Pipe for communicating with the serving thread about new clients. - read end */
static RTPIPE               g_hPipeR;
/** Pipe for communicating with the serving thread about new clients. - write end */
static RTPIPE               g_hPipeW;
/** Main thread waiting for connections. */
static RTTHREAD             g_hThreadMain;
/** Thread serving connected clients. */
static RTTHREAD             g_hThreadServing;
/** Critical section protecting the list of new clients. */
static RTCRITSECT           g_CritSectClients;
/** List of new clients waiting to be picked up by the client worker thread. */
static RTLISTANCHOR         g_LstClientsNew;


/**
 * ATS client state.
 */
typedef enum ATSCLIENTSTATE
{
    /** Invalid client state. */
    ATSCLIENTSTATE_INVALID = 0,
    /** Client is initialising, only the HOWDY and BYE packets are allowed. */
    ATSCLIENTSTATE_INITIALISING,
    /** Client is in fully cuntional state and ready to process all requests. */
    ATSCLIENTSTATE_READY,
    /** Client is destroying. */
    ATSCLIENTSTATE_DESTROYING,
    /** 32bit hack. */
    ATSCLIENTSTATE_32BIT_HACK = 0x7fffffff
} ATSCLIENTSTATE;

/**
 * ATS client instance.
 */
typedef struct ATSCLIENT
{
    /** List node for new clients. */
    RTLISTNODE             NdLst;
    /** The current client state. */
    ATSCLIENTSTATE         enmState;
    /** Transport backend specific data. */
    PATSTRANSPORTCLIENT    pTransportClient;
    /** Client hostname. */
    char                  *pszHostname;
} ATSCLIENT;
/** Pointer to a ATS client instance. */
typedef ATSCLIENT *PATSCLIENT;

/**
 * Returns the string represenation of the given state.
 */
static const char *atsClientStateStringify(ATSCLIENTSTATE enmState)
{
    switch (enmState)
    {
        case ATSCLIENTSTATE_INVALID:
            return "INVALID";
        case ATSCLIENTSTATE_INITIALISING:
            return "INITIALISING";
        case ATSCLIENTSTATE_READY:
            return "READY";
        case ATSCLIENTSTATE_DESTROYING:
            return "DESTROYING";
        case ATSCLIENTSTATE_32BIT_HACK:
        default:
            break;
    }

    AssertMsgFailed(("Unknown state %#x\n", enmState));
    return "UNKNOWN";
}

/**
 * Calculates the checksum value, zero any padding space and send the packet.
 *
 * @returns IPRT status code.
 * @param   pClient             The ATS client structure.
 * @param   pPkt                The packet to send.  Must point to a correctly
 *                              aligned buffer.
 */
static int atsSendPkt(PATSCLIENT pClient, PATSPKTHDR pPkt)
{
    Assert(pPkt->cb >= sizeof(*pPkt));
    pPkt->uCrc32 = RTCrc32(pPkt->achOpcode, pPkt->cb - RT_UOFFSETOF(ATSPKTHDR, achOpcode));
    if (pPkt->cb != RT_ALIGN_32(pPkt->cb, ATSPKT_ALIGNMENT))
        memset((uint8_t *)pPkt + pPkt->cb, '\0', RT_ALIGN_32(pPkt->cb, ATSPKT_ALIGNMENT) - pPkt->cb);

    Log(("atsSendPkt: cb=%#x opcode=%.8s\n", pPkt->cb, pPkt->achOpcode));
    Log2(("%.*Rhxd\n", RT_MIN(pPkt->cb, 256), pPkt));
    int rc = g_pTransport->pfnSendPkt(pClient->pTransportClient, pPkt);
    while (RT_UNLIKELY(rc == VERR_INTERRUPTED) && !g_fTerminate)
        rc = g_pTransport->pfnSendPkt(pClient->pTransportClient, pPkt);
    if (RT_FAILURE(rc))
        Log(("atsSendPkt: rc=%Rrc\n", rc));

    return rc;
}

/**
 * Sends a babble reply and disconnects the client (if applicable).
 *
 * @param   pClient             The ATS client structure.
 * @param   pszOpcode           The BABBLE opcode.
 */
static void atsReplyBabble(PATSCLIENT pClient, const char *pszOpcode)
{
    ATSPKTHDR Reply;
    Reply.cb     = sizeof(Reply);
    Reply.uCrc32 = 0;
    memcpy(Reply.achOpcode, pszOpcode, sizeof(Reply.achOpcode));

    g_pTransport->pfnBabble(pClient->pTransportClient, &Reply, 20*1000);
}

/**
 * Receive and validate a packet.
 *
 * Will send bable responses to malformed packets that results in a error status
 * code.
 *
 * @returns IPRT status code.
 * @param   pClient             The ATS client structure.
 * @param   ppPktHdr            Where to return the packet on success.  Free
 *                              with RTMemFree.
 * @param   fAutoRetryOnFailure Whether to retry on error.
 */
static int atsRecvPkt(PATSCLIENT pClient, PPATSPKTHDR ppPktHdr, bool fAutoRetryOnFailure)
{
    for (;;)
    {
        PATSPKTHDR pPktHdr;
        int rc = g_pTransport->pfnRecvPkt(pClient->pTransportClient, &pPktHdr);
        if (RT_SUCCESS(rc))
        {
            /* validate the packet. */
            if (   pPktHdr->cb >= sizeof(ATSPKTHDR)
                && pPktHdr->cb < ATSPKT_MAX_SIZE)
            {
                Log2(("atsRecvPkt: pPktHdr=%p cb=%#x crc32=%#x opcode=%.8s\n"
                      "%.*Rhxd\n",
                      pPktHdr, pPktHdr->cb, pPktHdr->uCrc32, pPktHdr->achOpcode, RT_MIN(pPktHdr->cb, 256), pPktHdr));
                uint32_t uCrc32Calc = pPktHdr->uCrc32 != 0
                                    ? RTCrc32(&pPktHdr->achOpcode[0], pPktHdr->cb - RT_UOFFSETOF(ATSPKTHDR, achOpcode))
                                    : 0;
                if (pPktHdr->uCrc32 == uCrc32Calc)
                {
                    AssertCompileMemberSize(ATSPKTHDR, achOpcode, 8);
                    if (   RT_C_IS_UPPER(pPktHdr->achOpcode[0])
                        && RT_C_IS_UPPER(pPktHdr->achOpcode[1])
                        && (RT_C_IS_UPPER(pPktHdr->achOpcode[2]) || pPktHdr->achOpcode[2] == ' ')
                        && (RT_C_IS_PRINT(pPktHdr->achOpcode[3]) || pPktHdr->achOpcode[3] == ' ')
                        && (RT_C_IS_PRINT(pPktHdr->achOpcode[4]) || pPktHdr->achOpcode[4] == ' ')
                        && (RT_C_IS_PRINT(pPktHdr->achOpcode[5]) || pPktHdr->achOpcode[5] == ' ')
                        && (RT_C_IS_PRINT(pPktHdr->achOpcode[6]) || pPktHdr->achOpcode[6] == ' ')
                        && (RT_C_IS_PRINT(pPktHdr->achOpcode[7]) || pPktHdr->achOpcode[7] == ' ')
                       )
                    {
                        Log(("atsRecvPkt: cb=%#x opcode=%.8s\n", pPktHdr->cb, pPktHdr->achOpcode));
                        *ppPktHdr = pPktHdr;
                        return rc;
                    }

                    rc = VERR_IO_BAD_COMMAND;
                }
                else
                {
                    Log(("atsRecvPkt: cb=%#x opcode=%.8s crc32=%#x actual=%#x\n",
                         pPktHdr->cb, pPktHdr->achOpcode, pPktHdr->uCrc32, uCrc32Calc));
                    rc = VERR_IO_CRC;
                }
            }
            else
                rc = VERR_IO_BAD_LENGTH;

            /* Send babble reply and disconnect the client if the transport is
               connection oriented. */
            if (rc == VERR_IO_BAD_LENGTH)
                atsReplyBabble(pClient, "BABBLE L");
            else if (rc == VERR_IO_CRC)
                atsReplyBabble(pClient, "BABBLE C");
            else if (rc == VERR_IO_BAD_COMMAND)
                atsReplyBabble(pClient, "BABBLE O");
            else
                atsReplyBabble(pClient, "BABBLE  ");
            RTMemFree(pPktHdr);
        }

        /* Try again or return failure? */
        if (   g_fTerminate
            || rc != VERR_INTERRUPTED
            || !fAutoRetryOnFailure
            )
        {
            Log(("atsRecvPkt: rc=%Rrc\n", rc));
            return rc;
        }
    }
}

/**
 * Make a simple reply, only status opcode.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pReply              The reply packet.
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 * @param   cbExtra             Bytes in addition to the header.
 */
static int atsReplyInternal(PATSCLIENT pClient, PATSPKTSTS pReply, const char *pszOpcode, size_t cbExtra)
{
    /* copy the opcode, don't be too strict in case of a padding screw up. */
    size_t cchOpcode = strlen(pszOpcode);
    if (RT_LIKELY(cchOpcode == sizeof(pReply->Hdr.achOpcode)))
        memcpy(pReply->Hdr.achOpcode, pszOpcode, sizeof(pReply->Hdr.achOpcode));
    else
    {
        Assert(cchOpcode == sizeof(pReply->Hdr.achOpcode));
        while (cchOpcode > 0 && pszOpcode[cchOpcode - 1] == ' ')
            cchOpcode--;
        AssertMsgReturn(cchOpcode < sizeof(pReply->Hdr.achOpcode), ("%d/'%.8s'\n", cchOpcode, pszOpcode), VERR_INTERNAL_ERROR_4);
        memcpy(pReply->Hdr.achOpcode, pszOpcode, cchOpcode);
        memset(&pReply->Hdr.achOpcode[cchOpcode], ' ', sizeof(pReply->Hdr.achOpcode) - cchOpcode);
    }

    pReply->Hdr.cb     = (uint32_t)sizeof(ATSPKTSTS) + (uint32_t)cbExtra;
    pReply->Hdr.uCrc32 = 0;

    return atsSendPkt(pClient, &pReply->Hdr);
}

/**
 * Make a simple reply, only status opcode.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The original packet (for future use).
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 */
static int atsReplySimple(PATSCLIENT pClient, PCATSPKTHDR pPktHdr, const char *pszOpcode)
{
    ATSPKTSTS Pkt;

    RT_ZERO(Pkt);
    Pkt.rcReq = VINF_SUCCESS;
    Pkt.cchStsMsg = 0;
    NOREF(pPktHdr);
    return atsReplyInternal(pClient, &Pkt, pszOpcode, 0);
}

/**
 * Acknowledges a packet with success.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The original packet (for future use).
 */
static int atsReplyAck(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    return atsReplySimple(pClient, pPktHdr, "ACK     ");
}

/**
 * Replies with a failure.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The original packet (for future use).
 * @param   rcReq               Status code.
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 * @param   rcReq               The status code of the request.
 * @param   pszDetailFmt        Longer description of the problem (format string).
 * @param   va                  Format arguments.
 */
static int atsReplyFailureV(PATSCLIENT pClient, PCATSPKTHDR pPktHdr, const char *pszOpcode, int rcReq, const char *pszDetailFmt, va_list va)
{
    NOREF(pPktHdr);
    union
    {
        ATSPKTSTS   Hdr;
        char        ach[256];
    } uPkt;

    RT_ZERO(uPkt);
    size_t cchDetail = RTStrPrintfV(&uPkt.ach[sizeof(ATSPKTSTS)],
                                    sizeof(uPkt) - sizeof(ATSPKTSTS),
                                    pszDetailFmt, va);
    uPkt.Hdr.rcReq = rcReq;
    uPkt.Hdr.cchStsMsg = cchDetail;
    return atsReplyInternal(pClient, &uPkt.Hdr, pszOpcode, cchDetail + 1);
}

/**
 * Replies with a failure.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The original packet (for future use).
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 * @param   rcReq               Status code.
 * @param   pszDetailFmt        Longer description of the problem (format string).
 * @param   ...                 Format arguments.
 */
static int atsReplyFailure(PATSCLIENT pClient, PCATSPKTHDR pPktHdr, const char *pszOpcode, int rcReq, const char *pszDetailFmt, ...)
{
    va_list va;
    va_start(va, pszDetailFmt);
    int rc = atsReplyFailureV(pClient, pPktHdr, pszOpcode, rcReq, pszDetailFmt, va);
    va_end(va);
    return rc;
}

/**
 * Replies according to the return code.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The packet to reply to.
 * @param   rcOperation         The status code to report.
 * @param   pszOperationFmt     The operation that failed.  Typically giving the
 *                              function call with important arguments.
 * @param   ...                 Arguments to the format string.
 */
static int atsReplyRC(PATSCLIENT pClient, PCATSPKTHDR pPktHdr, int rcOperation, const char *pszOperationFmt, ...)
{
    if (RT_SUCCESS(rcOperation))
        return atsReplyAck(pClient, pPktHdr);

    char    szOperation[128];
    va_list va;
    va_start(va, pszOperationFmt);
    RTStrPrintfV(szOperation, sizeof(szOperation), pszOperationFmt, va);
    va_end(va);

    return atsReplyFailure(pClient, pPktHdr, "FAILED  ", rcOperation, "%s failed with rc=%Rrc (opcode '%.8s')",
                           szOperation, rcOperation, pPktHdr->achOpcode);
}

/**
 * Signal a bad packet exact size.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The packet to reply to.
 * @param   cb                  The wanted size.
 */
static int atsReplyBadSize(PATSCLIENT pClient, PCATSPKTHDR pPktHdr, size_t cb)
{
    return atsReplyFailure(pClient, pPktHdr, "BAD SIZE", VERR_INVALID_PARAMETER, "Expected at %zu bytes, got %u  (opcode '%.8s')",
                           cb, pPktHdr->cb, pPktHdr->achOpcode);
}

/**
 * Deals with a unknown command.
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The packet to reply to.
 */
static int atsReplyUnknown(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    return atsReplyFailure(pClient, pPktHdr, "UNKNOWN ", VERR_NOT_FOUND, "Opcode '%.8s' is not known", pPktHdr->achOpcode);
}

#if 0
/**
 * Deals with a command which contains an unterminated string.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The packet containing the unterminated string.
 */
static int atsReplyBadStrTermination(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    return atsReplyFailure(pClient, pPktHdr, "BAD TERM", VERR_INVALID_PARAMETER, "Opcode '%.8s' contains an unterminated string", pPktHdr->achOpcode);
}
#endif

/**
 * Deals with a command sent in an invalid client state.
 *
 * @returns IPRT status code of the send.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The packet containing the unterminated string.
 */
static int atsReplyInvalidState(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    return atsReplyFailure(pClient, pPktHdr, "INVSTATE", VERR_INVALID_STATE, "Opcode '%.8s' is not supported at client state '%s",
                           pPktHdr->achOpcode, atsClientStateStringify(pClient->enmState));
}

/**
 * Verifies and acknowledges a "BYE" request.
 *
 * @returns IPRT status code.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The howdy packet.
 */
static int atsDoBye(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    int rc;
    if (pPktHdr->cb == sizeof(ATSPKTHDR))
        rc = atsReplyAck(pClient, pPktHdr);
    else
        rc = atsReplyBadSize(pClient, pPktHdr, sizeof(ATSPKTHDR));
    return rc;
}

/**
 * Verifies and acknowledges a "HOWDY" request.
 *
 * @returns IPRT status code.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The howdy packet.
 */
static int atsDoHowdy(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    int rc = VINF_SUCCESS;

    if (pPktHdr->cb != sizeof(ATSPKTREQHOWDY))
        return atsReplyBadSize(pClient, pPktHdr, sizeof(ATSPKTREQHOWDY));

    if (pClient->enmState != ATSCLIENTSTATE_INITIALISING)
        return atsReplyInvalidState(pClient, pPktHdr);

    PATSPKTREQHOWDY pReq = (PATSPKTREQHOWDY)pPktHdr;

    if (pReq->uVersion != ATS_PROTOCOL_VS)
        return atsReplyRC(pClient, pPktHdr, VERR_VERSION_MISMATCH, "The given version %#x is not supported", pReq->uVersion);

    return rc;
}

/**
 * Verifies and processes a "TONE PLAY" request.
 *
 * @returns IPRT status code.
 * @param   pClient             The ATS client structure.
 * @param   pPktHdr             The packet header.
 */
static int atsDoTonePlay(PATSCLIENT pClient, PCATSPKTHDR pPktHdr)
{
    int rc = VINF_SUCCESS;

    if (pPktHdr->cb < sizeof(ATSPKTREQTONEPLAY))
        return atsReplyBadSize(pClient, pPktHdr, sizeof(ATSPKTREQTONEPLAY));

    if (pClient->enmState != ATSCLIENTSTATE_READY)
        return atsReplyInvalidState(pClient, pPktHdr);

    return rc;
}

/**
 * Main request processing routine for each client.
 *
 * @returns IPRT status code.
 * @param   pClient             The ATS client structure sending the request.
 */
static int atsClientReqProcess(PATSCLIENT pClient)
{
    /*
     * Read client command packet and process it.
     */
    PATSPKTHDR pPktHdr = NULL;
    int rc = atsRecvPkt(pClient, &pPktHdr, true /*fAutoRetryOnFailure*/);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Do a string switch on the opcode bit.
     */
    /* Connection: */
    if (     atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_HOWDY))
        rc = atsDoHowdy(pClient, pPktHdr);
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_BYE))
        rc = atsDoBye(pClient, pPktHdr);
    /* Gadget API. */
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_TONE_PLAY))
        rc = atsDoTonePlay(pClient, pPktHdr);
    /* Misc: */
    else
        rc = atsReplyUnknown(pClient, pPktHdr);

    RTMemFree(pPktHdr);

    return rc;
}

/**
 * Destroys a client instance.
 *
 * @returns nothing.
 * @param   pClient             The ATS client structure.
 */
static void atsClientDestroy(PATSCLIENT pClient)
{
    if (pClient->pszHostname)
        RTStrFree(pClient->pszHostname);
    RTMemFree(pClient);
}

/**
 * The main thread worker serving the clients.
 */
static DECLCALLBACK(int) atsClientWorker(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF2(hThread, pvUser);
    unsigned    cClientsMax = 0;
    unsigned    cClientsCur = 0;
    PATSCLIENT *papClients  = NULL;
    RTPOLLSET   hPollSet;

    int rc = RTPollSetCreate(&hPollSet);
    if (RT_FAILURE(rc))
        return rc;

    /* Add the pipe to the poll set. */
    rc = RTPollSetAddPipe(hPollSet, g_hPipeR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 0);
    if (RT_SUCCESS(rc))
    {
        while (!g_fTerminate)
        {
            uint32_t fEvts;
            uint32_t uId;
            rc = RTPoll(hPollSet, RT_INDEFINITE_WAIT, &fEvts, &uId);
            if (RT_SUCCESS(rc))
            {
                if (uId == 0)
                {
                    if (fEvts & RTPOLL_EVT_ERROR)
                        break;

                    /* We got woken up because of a new client. */
                    Assert(fEvts & RTPOLL_EVT_READ);

                    uint8_t bRead;
                    size_t cbRead = 0;
                    rc = RTPipeRead(g_hPipeR, &bRead, 1, &cbRead);
                    AssertRC(rc);

                    RTCritSectEnter(&g_CritSectClients);
                    /* Walk the list and add all new clients. */
                    PATSCLIENT pIt, pItNext;
                    RTListForEachSafe(&g_LstClientsNew, pIt, pItNext, ATSCLIENT, NdLst)
                    {
                        RTListNodeRemove(&pIt->NdLst);
                        Assert(cClientsCur <= cClientsMax);
                        if (cClientsCur == cClientsMax)
                        {
                            /* Realloc to accommodate for the new clients. */
                            PATSCLIENT *papClientsNew = (PATSCLIENT *)RTMemRealloc(papClients, (cClientsMax + 10) * sizeof(PATSCLIENT));
                            if (RT_LIKELY(papClientsNew))
                            {
                                cClientsMax += 10;
                                papClients = papClientsNew;
                            }
                        }

                        if (cClientsCur < cClientsMax)
                        {
                            /* Find a free slot in the client array. */
                            unsigned idxSlt = 0;
                            while (   idxSlt < cClientsMax
                                   && papClients[idxSlt] != NULL)
                                idxSlt++;

                            rc = g_pTransport->pfnPollSetAdd(hPollSet, pIt->pTransportClient, idxSlt + 1);
                            if (RT_SUCCESS(rc))
                            {
                                cClientsCur++;
                                papClients[idxSlt] = pIt;
                            }
                            else
                            {
                                g_pTransport->pfnNotifyBye(pIt->pTransportClient);
                                atsClientDestroy(pIt);
                            }
                        }
                        else
                        {
                            g_pTransport->pfnNotifyBye(pIt->pTransportClient);
                            atsClientDestroy(pIt);
                        }
                    }
                    RTCritSectLeave(&g_CritSectClients);
                }
                else
                {
                    /* Client sends a request, pick the right client and process it. */
                    PATSCLIENT pClient = papClients[uId - 1];
                    AssertPtr(pClient);
                    if (fEvts & RTPOLL_EVT_READ)
                        rc = atsClientReqProcess(pClient);

                    if (   (fEvts & RTPOLL_EVT_ERROR)
                        || RT_FAILURE(rc))
                    {
                        /* Close connection and remove client from array. */
                        rc = g_pTransport->pfnPollSetRemove(hPollSet, pClient->pTransportClient, uId);
                        AssertRC(rc);

                        g_pTransport->pfnNotifyBye(pClient->pTransportClient);
                        papClients[uId - 1] = NULL;
                        cClientsCur--;
                        atsClientDestroy(pClient);
                    }
                }
            }
        }
    }

    RTPollSetDestroy(hPollSet);

    return rc;
}

/**
 * The main thread waiting for new client connections.
 *
 * @returns VBox status code.
 */
int atsMainThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread, pvUser);

    int rc = VINF_SUCCESS;

    while (!g_fTerminate)
    {
        /*
         * Wait for new connection and spin off a new thread
         * for every new client.
         */
        PATSTRANSPORTCLIENT pTransportClient;
        rc = g_pTransport->pfnWaitForConnect(&pTransportClient);
        if (RT_FAILURE(rc))
            continue;

        /*
         * New connection, create new client structure and spin of
         * the request handling thread.
         */
        PATSCLIENT pClient = (PATSCLIENT)RTMemAllocZ(sizeof(ATSCLIENT));
        if (RT_LIKELY(pClient))
        {
            pClient->enmState         = ATSCLIENTSTATE_INITIALISING;
            pClient->pTransportClient = pTransportClient;
            pClient->pszHostname      = NULL;

            /* Add client to the new list and inform the worker thread. */
            RTCritSectEnter(&g_CritSectClients);
            RTListAppend(&g_LstClientsNew, &pClient->NdLst);
            RTCritSectLeave(&g_CritSectClients);

            size_t cbWritten = 0;
            rc = RTPipeWrite(g_hPipeW, "", 1, &cbWritten);
            if (RT_FAILURE(rc))
                RTMsgError("Failed to inform worker thread of a new client");
        }
        else
        {
            RTMsgError("Creating new client structure failed with out of memory error\n");
            g_pTransport->pfnNotifyBye(pTransportClient);
        }
    }

    return rc;
}

/**
 * Initializes the global ATS state.
 *
 * @returns VBox status code.
 */
int atsInit(void)
{
    RTListInit(&g_LstClientsNew);

    /*
     * The default transporter is the first one.
     */
    g_pTransport = g_apTransports[0];

    /*
     * Initialize the transport layer.
     */
    int rc = g_pTransport->pfnInit();
    if (RT_SUCCESS(rc))
    {
        rc = RTCritSectInit(&g_CritSectClients);
        if (RT_SUCCESS(rc))
        {
            rc = RTPipeCreate(&g_hPipeR, &g_hPipeW, 0);
            if (RT_SUCCESS(rc))
            {
                /* Spin off the thread serving connections. */
                rc = RTThreadCreate(&g_hThreadServing, atsClientWorker, NULL, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE,
                                    "AUDTSTSRVC");
                if (RT_SUCCESS(rc))
                    return VINF_SUCCESS;
                else
                    RTMsgError("Creating the client worker thread failed with %Rrc\n", rc);

                RTPipeClose(g_hPipeR);
                RTPipeClose(g_hPipeW);
            }
            else
                RTMsgError("Creating communications pipe failed with %Rrc\n", rc);

            RTCritSectDelete(&g_CritSectClients);
        }
        else
            RTMsgError("Creating global critical section failed with %Rrc\n", rc);
    }
    else
        RTMsgError("Initializing the platform failed with %Rrc\n", rc);

    return rc;
}

int atsStart(void)
{
    /* Spin off the main thread. */
    int rc = RTThreadCreate(&g_hThreadMain, atsMainThread, NULL, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "AUDTSTSRVM");

    return rc;
}

