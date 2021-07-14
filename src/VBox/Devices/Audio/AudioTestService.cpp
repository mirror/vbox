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
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_AUDIO_TEST

#include <iprt/alloca.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/critsect.h>
#include <iprt/crc.h>
#include <iprt/ctype.h>
#include <iprt/dir.h>
#include <iprt/env.h>
#include <iprt/err.h>
#include <iprt/file.h>
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

#include <VBox/log.h>

#include "AudioTestService.h"
#include "AudioTestServiceInternal.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
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


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/**
 * Transport layers.
 */
const PCATSTRANSPORT g_apTransports[] =
{
    &g_TcpTransport
};
/** Number of transport layers in \a g_apTransports. */
const size_t g_cTransports = RT_ELEMENTS(g_apTransports);

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
typedef struct ATSCLIENTINST
{
    /** List node for new clients. */
    RTLISTNODE             NdLst;
    /** The current client state. */
    ATSCLIENTSTATE         enmState;
    /** Transport backend specific data. */
    PATSTRANSPORTCLIENT    pTransportClient;
    /** Client hostname. */
    char                  *pszHostname;
} ATSCLIENTINST;
/** Pointer to a ATS client instance. */
typedef ATSCLIENTINST *PATSSERVERINST;

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
 * @param   pThis               The ATS instance.
 * @param   pInst               The ATS client structure.
 * @param   pPkt                The packet to send.  Must point to a correctly
 *                              aligned buffer.
 */
static int atsSendPkt(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPkt)
{
    Assert(pPkt->cb >= sizeof(*pPkt));
    pPkt->uCrc32 = RTCrc32(pPkt->achOpcode, pPkt->cb - RT_UOFFSETOF(ATSPKTHDR, achOpcode));
    if (pPkt->cb != RT_ALIGN_32(pPkt->cb, ATSPKT_ALIGNMENT))
        memset((uint8_t *)pPkt + pPkt->cb, '\0', RT_ALIGN_32(pPkt->cb, ATSPKT_ALIGNMENT) - pPkt->cb);

    LogFlowFunc(("cb=%RU32 (%#x), payload=%RU32 (%#x), opcode=%.8s\n",
                 pPkt->cb, pPkt->cb, pPkt->cb - sizeof(ATSPKTHDR), pPkt->cb - sizeof(ATSPKTHDR), pPkt->achOpcode));
    int rc = pThis->pTransport->pfnSendPkt(pThis->pTransportInst, pInst->pTransportClient, pPkt);
    while (RT_UNLIKELY(rc == VERR_INTERRUPTED) && !pThis->fTerminate)
        rc = pThis->pTransport->pfnSendPkt(pThis->pTransportInst, pInst->pTransportClient, pPkt);

    return rc;
}

/**
 * Sends a babble reply and disconnects the client (if applicable).
 *
 * @param   pThis               The ATS instance.
 * @param   pInst               The ATS server instance.
 * @param   pszOpcode           The BABBLE opcode.
 */
static void atsReplyBabble(PATSSERVER pThis, PATSSERVERINST pInst, const char *pszOpcode)
{
    ATSPKTHDR Reply;
    Reply.cb     = sizeof(Reply);
    Reply.uCrc32 = 0;
    memcpy(Reply.achOpcode, pszOpcode, sizeof(Reply.achOpcode));

    pThis->pTransport->pfnBabble(pThis->pTransportInst, pInst->pTransportClient, &Reply, 20*1000);
}

/**
 * Receive and validate a packet.
 *
 * Will send bable responses to malformed packets that results in a error status
 * code.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   ppPktHdr            Where to return the packet on success.  Free
 *                              with RTMemFree.
 * @param   fAutoRetryOnFailure Whether to retry on error.
 */
static int atsRecvPkt(PATSSERVER pThis, PATSSERVERINST pInst, PPATSPKTHDR ppPktHdr, bool fAutoRetryOnFailure)
{
    for (;;)
    {
        PATSPKTHDR pPktHdr;
        int rc = pThis->pTransport->pfnRecvPkt(pThis->pTransportInst, pInst->pTransportClient, &pPktHdr);
        if (RT_SUCCESS(rc))
        {
            /* validate the packet. */
            if (   pPktHdr->cb >= sizeof(ATSPKTHDR)
                && pPktHdr->cb < ATSPKT_MAX_SIZE)
            {
                Log2Func(("pPktHdr=%p cb=%#x crc32=%#x opcode=%.8s\n",
                          pPktHdr, pPktHdr->cb, pPktHdr->uCrc32, pPktHdr->achOpcode));
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
                        Log(("cb=%#x opcode=%.8s\n", pPktHdr->cb, pPktHdr->achOpcode));
                        *ppPktHdr = pPktHdr;
                        return rc;
                    }

                    rc = VERR_IO_BAD_COMMAND;
                }
                else
                {
                    Log(("cb=%#x opcode=%.8s crc32=%#x actual=%#x\n",
                         pPktHdr->cb, pPktHdr->achOpcode, pPktHdr->uCrc32, uCrc32Calc));
                    rc = VERR_IO_CRC;
                }
            }
            else
                rc = VERR_IO_BAD_LENGTH;

            /* Send babble reply and disconnect the client if the transport is
               connection oriented. */
            if (rc == VERR_IO_BAD_LENGTH)
                atsReplyBabble(pThis, pInst, "BABBLE L");
            else if (rc == VERR_IO_CRC)
                atsReplyBabble(pThis, pInst, "BABBLE C");
            else if (rc == VERR_IO_BAD_COMMAND)
                atsReplyBabble(pThis, pInst, "BABBLE O");
            else
                atsReplyBabble(pThis, pInst, "BABBLE  ");
            RTMemFree(pPktHdr);
        }

        /* Try again or return failure? */
        if (   pThis->fTerminate
            || rc != VERR_INTERRUPTED
            || !fAutoRetryOnFailure
            )
        {
            Log(("rc=%Rrc\n", rc));
            return rc;
        }
    }
}

/**
 * Make a simple reply, only status opcode.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pReply              The reply packet.
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 * @param   cbExtra             Bytes in addition to the header.
 */
static int atsReplyInternal(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pReply, const char *pszOpcode, size_t cbExtra)
{
    /* copy the opcode, don't be too strict in case of a padding screw up. */
    size_t cchOpcode = strlen(pszOpcode);
    if (RT_LIKELY(cchOpcode == sizeof(pReply->achOpcode)))
        memcpy(pReply->achOpcode, pszOpcode, sizeof(pReply->achOpcode));
    else
    {
        Assert(cchOpcode == sizeof(pReply->achOpcode));
        while (cchOpcode > 0 && pszOpcode[cchOpcode - 1] == ' ')
            cchOpcode--;
        AssertMsgReturn(cchOpcode < sizeof(pReply->achOpcode), ("%d/'%.8s'\n", cchOpcode, pszOpcode), VERR_INTERNAL_ERROR_4);
        memcpy(pReply->achOpcode, pszOpcode, cchOpcode);
        memset(&pReply->achOpcode[cchOpcode], ' ', sizeof(pReply->achOpcode) - cchOpcode);
    }

    pReply->cb     = (uint32_t)sizeof(ATSPKTHDR) + (uint32_t)cbExtra;
    pReply->uCrc32 = 0;

    return atsSendPkt(pThis, pInst, pReply);
}

/**
 * Make a simple reply, only status opcode.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The original packet (for future use).
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 */
static int atsReplySimple(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr, const char *pszOpcode)
{
    return atsReplyInternal(pThis, pInst, pPktHdr, pszOpcode, 0);
}

/**
 * Acknowledges a packet with success.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The original packet (for future use).
 */
static int atsReplyAck(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    return atsReplySimple(pThis, pInst, pPktHdr, "ACK     ");
}

/**
 * Replies with a failure.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The original packet (for future use).
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 * @param   rcReq               The status code of the request.
 * @param   pszDetailFmt        Longer description of the problem (format string).
 * @param   va                  Format arguments.
 */
static int atsReplyFailureV(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr,
                            const char *pszOpcode, int rcReq, const char *pszDetailFmt, va_list va)
{
    RT_NOREF(pPktHdr);
    union
    {
        ATSPKTHDR   Hdr;
        int         rc;
        char        ach[256];
    } uPkt;
    RT_ZERO(uPkt);

    size_t cchDetail = RTStrPrintfV(&uPkt.ach[sizeof(ATSPKTHDR)],
                                    sizeof(uPkt) - sizeof(ATSPKTHDR),
                                    pszDetailFmt, va);

    uPkt.rc = rcReq;

    return atsReplyInternal(pThis, pInst, &uPkt.Hdr, pszOpcode, sizeof(int) + cchDetail + 1);
}

/**
 * Replies with a failure.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The original packet (for future use).
 * @param   pszOpcode           The status opcode.  Exactly 8 chars long, padd
 *                              with space.
 * @param   rcReq               Status code.
 * @param   pszDetailFmt        Longer description of the problem (format string).
 * @param   ...                 Format arguments.
 */
static int atsReplyFailure(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr,
                           const char *pszOpcode, int rcReq, const char *pszDetailFmt, ...)
{
    va_list va;
    va_start(va, pszDetailFmt);
    int rc = atsReplyFailureV(pThis, pInst, pPktHdr, pszOpcode, rcReq, pszDetailFmt, va);
    va_end(va);
    return rc;
}

/**
 * Replies according to the return code.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The packet to reply to.
 * @param   rcOperation         The status code to report.
 * @param   pszOperationFmt     The operation that failed.  Typically giving the
 *                              function call with important arguments.
 * @param   ...                 Arguments to the format string.
 */
static int atsReplyRC(PATSSERVER pThis,
                      PATSSERVERINST pInst, PATSPKTHDR pPktHdr, int rcOperation, const char *pszOperationFmt, ...)
{
    if (RT_SUCCESS(rcOperation))
        return atsReplyAck(pThis, pInst, pPktHdr);

    char    szOperation[128];
    va_list va;
    va_start(va, pszOperationFmt);
    RTStrPrintfV(szOperation, sizeof(szOperation), pszOperationFmt, va);
    va_end(va);

    return atsReplyFailure(pThis, pInst, pPktHdr, "FAILED  ", rcOperation, "%s failed with rc=%Rrc (opcode '%.8s')",
                           szOperation, rcOperation, pPktHdr->achOpcode);
}

/**
 * Signal a bad packet exact size.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The packet to reply to.
 * @param   cb                  The wanted size.
 */
static int atsReplyBadSize(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr, size_t cb)
{
    return atsReplyFailure(pThis, pInst, pPktHdr, "BAD SIZE", VERR_INVALID_PARAMETER, "Expected at %zu bytes, got %u  (opcode '%.8s')",
                           cb, pPktHdr->cb, pPktHdr->achOpcode);
}

/**
 * Deals with a unknown command.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The packet to reply to.
 */
static int atsReplyUnknown(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    return atsReplyFailure(pThis, pInst, pPktHdr, "UNKNOWN ", VERR_NOT_FOUND, "Opcode '%.8s' is not known", pPktHdr->achOpcode);
}

/**
 * Deals with a command sent in an invalid client state.
 *
 * @returns IPRT status code of the send.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The packet containing the unterminated string.
 */
static int atsReplyInvalidState(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    return atsReplyFailure(pThis, pInst, pPktHdr, "INVSTATE", VERR_INVALID_STATE, "Opcode '%.8s' is not supported at client state '%s",
                           pPktHdr->achOpcode, atsClientStateStringify(pInst->enmState));
}

/**
 * Verifies and acknowledges a "BYE" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The bye packet.
 */
static int atsDoBye(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    int rc;
    if (pPktHdr->cb == sizeof(ATSPKTHDR))
    {
        rc = atsReplyAck(pThis, pInst, pPktHdr);
    }
    else
        rc = atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTHDR));
    return rc;
}

/**
 * Verifies and acknowledges a "HOWDY" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The howdy packet.
 */
static int atsDoHowdy(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    int rc = VINF_SUCCESS;

    if (pPktHdr->cb != sizeof(ATSPKTREQHOWDY))
        return atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTREQHOWDY));

    if (pInst->enmState != ATSCLIENTSTATE_INITIALISING)
        return atsReplyInvalidState(pThis, pInst, pPktHdr);

    PATSPKTREQHOWDY pReq = (PATSPKTREQHOWDY)pPktHdr;

    if (pReq->uVersion != ATS_PROTOCOL_VS)
        return atsReplyRC(pThis, pInst, pPktHdr, VERR_VERSION_MISMATCH, "The given version %#x is not supported", pReq->uVersion);

    ATSPKTREPHOWDY Rep;
    RT_ZERO(Rep);

    Rep.uVersion = ATS_PROTOCOL_VS;

    rc = atsReplyInternal(pThis, pInst, &Rep.Hdr, "ACK     ", sizeof(Rep) - sizeof(ATSPKTHDR));
    if (RT_SUCCESS(rc))
    {
        pThis->pTransport->pfnNotifyHowdy(pThis->pTransportInst, pInst->pTransportClient);
        pInst->enmState = ATSCLIENTSTATE_READY;
    }

    return rc;
}

/**
 * Verifies and acknowledges a "TSET BEG" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The test set begin packet.
 */
static int atsDoTestSetBegin(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    if (pPktHdr->cb != sizeof(ATSPKTREQTSETBEG))
        return atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTREQTSETBEG));

    PATSPKTREQTSETBEG pReq = (PATSPKTREQTSETBEG)pPktHdr;

    int rc = VINF_SUCCESS;

    if (pThis->Callbacks.pfnTestSetBegin)
    {
        rc = pThis->Callbacks.pfnTestSetBegin(pThis->Callbacks.pvUser, pReq->szTag);
        if (RT_FAILURE(rc))
            return atsReplyRC(pThis, pInst, pPktHdr, rc, "Beginning test set '%s' failed", pReq->szTag);
    }

    if (RT_SUCCESS(rc))
    {
        rc = atsReplyAck(pThis, pInst, pPktHdr);
    }
    else
        rc = atsReplyRC(pThis, pInst, pPktHdr, rc, "Beginning test set failed");

    return rc;
}

/**
 * Verifies and acknowledges a "TSET END" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The test set end packet.
 */
static int atsDoTestSetEnd(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    if (pPktHdr->cb != sizeof(ATSPKTREQTSETEND))
        return atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTREQTSETEND));

    PATSPKTREQTSETEND pReq = (PATSPKTREQTSETEND)pPktHdr;

    int rc = VINF_SUCCESS;

    if (pThis->Callbacks.pfnTestSetEnd)
    {
        rc = pThis->Callbacks.pfnTestSetEnd(pThis->Callbacks.pvUser, pReq->szTag);
        if (RT_FAILURE(rc))
            return atsReplyRC(pThis, pInst, pPktHdr, rc, "Ending test set '%s' failed", pReq->szTag);
    }
    if (RT_SUCCESS(rc))
    {
        rc = atsReplyAck(pThis, pInst, pPktHdr);
    }
    else
        rc = atsReplyRC(pThis, pInst, pPktHdr, rc, "Ending test set failed");

    return rc;
}

/**
 * Used by atsDoTestSetSend to wait for a reply ACK from the client.
 *
 * @returns VINF_SUCCESS on ACK, VERR_GENERAL_FAILURE on NACK,
 *          VERR_NET_NOT_CONNECTED on unknown response (sending a bable reply),
 *          or whatever atsRecvPkt returns.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The original packet (for future use).
 */
static int atsWaitForAck(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    RT_NOREF(pPktHdr);
    /** @todo timeout? */
    PATSPKTHDR pReply;
    int rc = atsRecvPkt(pThis, pInst, &pReply, false /*fAutoRetryOnFailure*/);
    if (RT_SUCCESS(rc))
    {
        if (atsIsSameOpcode(pReply, "ACK"))
            rc = VINF_SUCCESS;
        else if (atsIsSameOpcode(pReply, "NACK"))
            rc = VERR_GENERAL_FAILURE;
        else
        {
            atsReplyBabble(pThis, pInst, "BABBLE  ");
            rc = VERR_NET_NOT_CONNECTED;
        }
        RTMemFree(pReply);
    }
    return rc;
}

/**
 * Verifies and acknowledges a "TSET SND" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The test set end packet.
 */
static int atsDoTestSetSend(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    if (pPktHdr->cb != sizeof(ATSPKTREQTSETSND))
        return atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTREQTSETSND));

    PATSPKTREQTSETSND pReq = (PATSPKTREQTSETSND)pPktHdr;

    int rc = VINF_SUCCESS;

    if (!pThis->Callbacks.pfnTestSetSendRead)
        return atsReplyRC(pThis, pInst, pPktHdr, VERR_NOT_SUPPORTED, "Sending test set not implemented");

    if (pThis->Callbacks.pfnTestSetSendBegin)
    {
        rc = pThis->Callbacks.pfnTestSetSendBegin(pThis->Callbacks.pvUser, pReq->szTag);
        if (RT_FAILURE(rc))
            return atsReplyRC(pThis, pInst, pPktHdr, rc, "Beginning sending test set '%s' failed", pReq->szTag);
    }

    for (;;)
    {
        uint32_t uMyCrc32 = RTCrc32Start();
        struct
        {
            ATSPKTHDR   Hdr;
            uint32_t    uCrc32;
            char        ab[_64K];
            char        abPadding[ATSPKT_ALIGNMENT];
        }       Pkt;
#ifdef DEBUG
        RT_ZERO(Pkt);
#endif
        size_t  cbRead = 0;
        rc = pThis->Callbacks.pfnTestSetSendRead(pThis->Callbacks.pvUser, pReq->szTag, &Pkt.ab, sizeof(Pkt.ab), &cbRead);
        if (   RT_FAILURE(rc)
            || cbRead == 0)
        {
            if (    rc == VERR_EOF
                || (RT_SUCCESS(rc) && cbRead == 0))
            {
                Pkt.uCrc32 = RTCrc32Finish(uMyCrc32);
                rc = atsReplyInternal(pThis, pInst, &Pkt.Hdr, "DATA EOF", sizeof(uint32_t) /* uCrc32 */);
                if (RT_SUCCESS(rc))
                    rc = atsWaitForAck(pThis, pInst, &Pkt.Hdr);
            }
            else
                rc = atsReplyRC(pThis, pInst, pPktHdr, rc, "Sending data for test set '%s' failed", pReq->szTag);
            break;
        }

        uMyCrc32   = RTCrc32Process(uMyCrc32, &Pkt.ab[0], cbRead);
        Pkt.uCrc32 = RTCrc32Finish(uMyCrc32);

        Log2Func(("cbRead=%zu -> uCrc32=%#x\n", cbRead, Pkt.uCrc32));

        Assert(cbRead <= sizeof(Pkt.ab));

        rc = atsReplyInternal(pThis, pInst, &Pkt.Hdr, "DATA    ", sizeof(uint32_t) /* uCrc32 */ + cbRead);
        if (RT_FAILURE(rc))
            break;

        rc = atsWaitForAck(pThis, pInst, &Pkt.Hdr);
        if (RT_FAILURE(rc))
            break;
    }

    if (pThis->Callbacks.pfnTestSetSendEnd)
    {
        int rc2 = pThis->Callbacks.pfnTestSetSendEnd(pThis->Callbacks.pvUser, pReq->szTag);
        if (RT_FAILURE(rc2))
            return atsReplyRC(pThis, pInst, pPktHdr, rc2, "Ending sending test set '%s' failed", pReq->szTag);
    }

    return rc;
}

/**
 * Verifies and processes a "TN PLY" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The packet header.
 */
static int atsDoTonePlay(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    int rc = VINF_SUCCESS;

    if (pPktHdr->cb < sizeof(ATSPKTREQTONEPLAY))
        return atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTREQTONEPLAY));

    if (pInst->enmState != ATSCLIENTSTATE_READY)
        return atsReplyInvalidState(pThis, pInst, pPktHdr);

    if (!pThis->Callbacks.pfnTonePlay)
        return atsReplyRC(pThis, pInst, pPktHdr, VERR_NOT_SUPPORTED, "Playing tones not supported");

    PATSPKTREQTONEPLAY pReq = (PATSPKTREQTONEPLAY)pPktHdr;
    rc = pThis->Callbacks.pfnTonePlay(pThis->Callbacks.pvUser, &pReq->ToneParms);

    int rc2 = atsReplyAck(pThis, pInst, pPktHdr);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

/**
 * Verifies and processes a "TN REC" request.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst               The opaque ATS instance structure.
 * @param   pPktHdr             The packet header.
 */
static int atsDoToneRecord(PATSSERVER pThis, PATSSERVERINST pInst, PATSPKTHDR pPktHdr)
{
    int rc = VINF_SUCCESS;

    if (pPktHdr->cb < sizeof(ATSPKTREQTONEREC))
        return atsReplyBadSize(pThis, pInst, pPktHdr, sizeof(ATSPKTREQTONEREC));

    if (pInst->enmState != ATSCLIENTSTATE_READY)
        return atsReplyInvalidState(pThis, pInst, pPktHdr);

    if (!pThis->Callbacks.pfnToneRecord)
        return atsReplyRC(pThis, pInst, pPktHdr, VERR_NOT_SUPPORTED, "Recording tones not supported");

    PATSPKTREQTONEREC pReq = (PATSPKTREQTONEREC)pPktHdr;
    rc = pThis->Callbacks.pfnToneRecord(pThis->Callbacks.pvUser, &pReq->ToneParms);

    int rc2 = atsReplyAck(pThis, pInst, pPktHdr);
    if (RT_SUCCESS(rc))
        rc = rc2;

    return rc;
}

/**
 * Main request processing routine for each client.
 *
 * @returns IPRT status code.
 * @param   pThis               The ATS instance.
 * @param   pInst             The ATS client structure sending the request.
 */
static int atsClientReqProcess(PATSSERVER pThis, PATSSERVERINST pInst)
{
    /*
     * Read client command packet and process it.
     */
    PATSPKTHDR pPktHdr = NULL;
    int rc = atsRecvPkt(pThis, pInst, &pPktHdr, true /*fAutoRetryOnFailure*/);
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Do a string switch on the opcode bit.
     */
    /* Connection: */
    if (     atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_HOWDY))
        rc = atsDoHowdy(pThis, pInst, pPktHdr);
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_BYE))
        rc = atsDoBye(pThis, pInst, pPktHdr);
    /* Test set handling: */
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_TESTSET_BEGIN))
        rc = atsDoTestSetBegin(pThis, pInst, pPktHdr);
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_TESTSET_END))
        rc = atsDoTestSetEnd(pThis, pInst, pPktHdr);
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_TESTSET_SEND))
        rc = atsDoTestSetSend(pThis, pInst, pPktHdr);
    /* Audio testing: */
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_TONE_PLAY))
        rc = atsDoTonePlay(pThis, pInst, pPktHdr);
    else if (atsIsSameOpcode(pPktHdr, ATSPKT_OPCODE_TONE_RECORD))
        rc = atsDoToneRecord(pThis, pInst, pPktHdr);
    /* Misc: */
    else
        rc = atsReplyUnknown(pThis, pInst, pPktHdr);

    RTMemFree(pPktHdr);

    return rc;
}

/**
 * Destroys a client instance.
 *
 * @returns nothing.
 * @param   pInst               The opaque ATS instance structure.
 */
static void atsClientDestroy(PATSSERVERINST pInst)
{
    if (pInst->pszHostname)
        RTStrFree(pInst->pszHostname);
    RTMemFree(pInst);
}

/**
 * The main thread worker serving the clients.
 */
static DECLCALLBACK(int) atsClientWorker(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);

    PATSSERVER pThis = (PATSSERVER)pvUser;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    unsigned    cClientsMax = 0;
    unsigned    cClientsCur = 0;
    PATSSERVERINST *papInsts  = NULL;

    /* Add the pipe to the poll set. */
    int rc = RTPollSetAddPipe(pThis->hPollSet, pThis->hPipeR, RTPOLL_EVT_READ | RTPOLL_EVT_ERROR, 0);
    if (RT_SUCCESS(rc))
    {
        while (!pThis->fTerminate)
        {
            uint32_t fEvts;
            uint32_t uId;
            rc = RTPoll(pThis->hPollSet, RT_INDEFINITE_WAIT, &fEvts, &uId);
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
                    rc = RTPipeRead(pThis->hPipeR, &bRead, 1, &cbRead);
                    AssertRC(rc);

                    RTCritSectEnter(&pThis->CritSectClients);
                    /* Walk the list and add all new clients. */
                    PATSSERVERINST pIt, pItNext;
                    RTListForEachSafe(&pThis->LstClientsNew, pIt, pItNext, ATSCLIENTINST, NdLst)
                    {
                        RTListNodeRemove(&pIt->NdLst);
                        Assert(cClientsCur <= cClientsMax);
                        if (cClientsCur == cClientsMax)
                        {
                            /* Realloc to accommodate for the new clients. */
                            PATSSERVERINST *papInstsNew = (PATSSERVERINST *)RTMemRealloc(papInsts, (cClientsMax + 10) * sizeof(PATSSERVERINST));
                            if (RT_LIKELY(papInstsNew))
                            {
                                cClientsMax += 10;
                                papInsts = papInstsNew;
                            }
                        }
                        if (cClientsCur < cClientsMax)
                        {
                            /* Find a free slot in the client array. */
                            unsigned idxSlt = 0;
                            while (   idxSlt < cClientsMax
                                   && papInsts[idxSlt] != NULL)
                                idxSlt++;

                            rc = pThis->pTransport->pfnPollSetAdd(pThis->pTransportInst, pThis->hPollSet, pIt->pTransportClient, idxSlt + 1);
                            if (RT_SUCCESS(rc))
                            {
                                cClientsCur++;
                                papInsts[idxSlt] = pIt;
                            }
                            else
                            {
                                pThis->pTransport->pfnNotifyBye(pThis->pTransportInst, pIt->pTransportClient);
                                atsClientDestroy(pIt);
                            }
                        }
                        else
                        {
                            pThis->pTransport->pfnNotifyBye(pThis->pTransportInst, pIt->pTransportClient);
                            atsClientDestroy(pIt);
                        }
                    }
                    RTCritSectLeave(&pThis->CritSectClients);
                }
                else
                {
                    /* Client sends a request, pick the right client and process it. */
                    PATSSERVERINST pInst = papInsts[uId - 1];
                    AssertPtr(pInst);
                    if (fEvts & RTPOLL_EVT_READ)
                        rc = atsClientReqProcess(pThis, pInst);

                    if (   (fEvts & RTPOLL_EVT_ERROR)
                        || RT_FAILURE(rc))
                    {
                        /* Close connection and remove client from array. */
                        rc = pThis->pTransport->pfnPollSetRemove(pThis->pTransportInst, pThis->hPollSet, pInst->pTransportClient, uId);
                        AssertRC(rc);

                        pThis->pTransport->pfnNotifyBye(pThis->pTransportInst, pInst->pTransportClient);
                        papInsts[uId - 1] = NULL;
                        cClientsCur--;
                        atsClientDestroy(pInst);
                    }
                }
            }
        }
    }

    return rc;
}

/**
 * The main thread waiting for new client connections.
 *
 * @returns VBox status code.
 */
static DECLCALLBACK(int) atsMainThread(RTTHREAD hThread, void *pvUser)
{
    RT_NOREF(hThread);

    PATSSERVER pThis = (PATSSERVER)pvUser;
    AssertPtrReturn(pThis, VERR_INVALID_POINTER);

    int rc = RTThreadUserSignal(hThread);
    AssertRCReturn(rc, rc);

    while (!pThis->fTerminate)
    {
        /*
         * Wait for new connection and spin off a new thread
         * for every new client.
         */
        PATSTRANSPORTCLIENT pTransportClient;
        rc = pThis->pTransport->pfnWaitForConnect(pThis->pTransportInst, &pTransportClient);
        if (RT_FAILURE(rc))
            continue;

        /*
         * New connection, create new client structure and spin of
         * the request handling thread.
         */
        PATSSERVERINST pInst = (PATSSERVERINST)RTMemAllocZ(sizeof(ATSCLIENTINST));
        if (RT_LIKELY(pInst))
        {
            pInst->enmState         = ATSCLIENTSTATE_INITIALISING;
            pInst->pTransportClient = pTransportClient;
            pInst->pszHostname      = NULL;

            /* Add client to the new list and inform the worker thread. */
            RTCritSectEnter(&pThis->CritSectClients);
            RTListAppend(&pThis->LstClientsNew, &pInst->NdLst);
            RTCritSectLeave(&pThis->CritSectClients);

            size_t cbWritten = 0;
            rc = RTPipeWrite(pThis->hPipeW, "", 1, &cbWritten);
            if (RT_FAILURE(rc))
                RTMsgError("Failed to inform worker thread of a new client");
        }
        else
        {
            RTMsgError("Creating new client structure failed with out of memory error\n");
            pThis->pTransport->pfnNotifyBye(pThis->pTransportInst, pTransportClient);
        }
    }

    return rc;
}

/**
 * Creates an ATS instance.
 *
 * @returns VBox status code.
 * @param   pThis               The ATS instance to create.
 */
int AudioTestSvcCreate(PATSSERVER pThis)
{
    /*
     * The default transporter is the first one.
     */
    pThis->pTransport = g_apTransports[0]; /** @todo Make this dynamic. */

    return pThis->pTransport->pfnCreate(&pThis->pTransportInst);
}

/**
 * Initializes an ATS instance.
 *
 * @returns VBox status code.
 * @param   pThis               The ATS instance.
 * @param   pCallbacks          The callbacks table to use.
 */
int AudioTestSvcInit(PATSSERVER pThis, PCATSCALLBACKS pCallbacks)
{
    memcpy(&pThis->Callbacks, pCallbacks, sizeof(ATSCALLBACKS));

    pThis->fStarted   = false;
    pThis->fTerminate = false;

    pThis->hPipeR     = NIL_RTPIPE;
    pThis->hPipeW     = NIL_RTPIPE;

    RTListInit(&pThis->LstClientsNew);

    /*
     * Initialize the transport layer.
     */
    int rc = RTCritSectInit(&pThis->CritSectClients);
    if (RT_SUCCESS(rc))
    {
        rc = RTPollSetCreate(&pThis->hPollSet);
        if (RT_SUCCESS(rc))
        {
            rc = RTPipeCreate(&pThis->hPipeR, &pThis->hPipeW, 0);
            if (RT_SUCCESS(rc))
            {
                /* Spin off the thread serving connections. */
                rc = RTThreadCreate(&pThis->hThreadServing, atsClientWorker, pThis, 0, RTTHREADTYPE_IO, RTTHREADFLAGS_WAITABLE,
                                    "AUDTSTSRVC");
                if (RT_SUCCESS(rc))
                    return VINF_SUCCESS;
                else
                    RTMsgError("Creating the client worker thread failed with %Rrc\n", rc);

                RTPipeClose(pThis->hPipeR);
                RTPipeClose(pThis->hPipeW);
            }
            else
                RTMsgError("Creating communications pipe failed with %Rrc\n", rc);

            RTPollSetDestroy(pThis->hPollSet);
        }
        else
            RTMsgError("Creating pollset failed with %Rrc\n", rc);

        RTCritSectDelete(&pThis->CritSectClients);
    }
    else
        RTMsgError("Creating global critical section failed with %Rrc\n", rc);

    return rc;
}

/**
 * Handles a command line option.
 *
 * @returns VBox status code.
 * @param   pThis               The ATS instance to handle option for.
 * @param   ch                  Option (short) to handle.
 * @param   pVal                Option union to store the result in on success.
 */
int AudioTestSvcHandleOption(PATSSERVER pThis, int ch, PCRTGETOPTUNION pVal)
{
    AssertPtrReturn(pThis->pTransport, VERR_WRONG_ORDER); /* Must be creatd first. */
    if (!pThis->pTransport->pfnOption)
        return VERR_GETOPT_UNKNOWN_OPTION;
    return pThis->pTransport->pfnOption(pThis->pTransportInst, ch, pVal);
}

/**
 * Starts a formerly initialized ATS instance.
 *
 * @returns VBox status code.
 * @param   pThis               The ATS instance to start.
 */
int AudioTestSvcStart(PATSSERVER pThis)
{
    int rc = pThis->pTransport->pfnStart(pThis->pTransportInst);
    if (RT_SUCCESS(rc))
    {
        /* Spin off the connection thread. */
        rc = RTThreadCreate(&pThis->hThreadMain, atsMainThread, pThis, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "AUDTSTSRVM");
        if (RT_SUCCESS(rc))
        {
            rc = RTThreadUserWait(pThis->hThreadMain, RT_MS_30SEC);
            if (RT_SUCCESS(rc))
                pThis->fStarted = true;
        }
    }

    return rc;
}

/**
 * Shuts down a formerly started ATS instance.
 *
 * @returns VBox status code.
 * @param   pThis               The ATS instance.
 */
int AudioTestSvcShutdown(PATSSERVER pThis)
{
    if (!pThis->fStarted)
        return VINF_SUCCESS;

    ASMAtomicXchgBool(&pThis->fTerminate, true);

    if (pThis->pTransport)
        pThis->pTransport->pfnTerm(pThis->pTransportInst);

    size_t cbWritten;
    int rc = RTPipeWrite(pThis->hPipeW, "", 1, &cbWritten);
    AssertRCReturn(rc, rc);

    /* First close serving thread. */
    int rcThread;
    rc = RTThreadWait(pThis->hThreadServing, RT_MS_30SEC, &rcThread);
    if (RT_SUCCESS(rc))
    {
        rc = rcThread;
        if (RT_SUCCESS(rc))
        {
            /* Close the main thread last. */
            rc = RTThreadWait(pThis->hThreadMain, RT_MS_30SEC, &rcThread);
            if (RT_SUCCESS(rc))
                rc = rcThread;

            if (rc == VERR_TCP_SERVER_DESTROYED)
                rc = VINF_SUCCESS;
        }
    }

    if (RT_SUCCESS(rc))
        pThis->fStarted = false;

    return rc;
}

/**
 * Destroys an ATS instance, internal version.
 *
 * @returns VBox status code.
 * @param   pThis               ATS instance to destroy.
 */
static int audioTestSvcDestroyInternal(PATSSERVER pThis)
{
    int rc = VINF_SUCCESS;

    if (pThis->hPipeR != NIL_RTPIPE)
    {
        rc = RTPipeClose(pThis->hPipeR);
        AssertRCReturn(rc, rc);
        pThis->hPipeR = NIL_RTPIPE;
    }

    if (pThis->hPipeW != NIL_RTPIPE)
    {
        rc = RTPipeClose(pThis->hPipeW);
        AssertRCReturn(rc, rc);
        pThis->hPipeW = NIL_RTPIPE;
    }

    RTPollSetDestroy(pThis->hPollSet);
    pThis->hPollSet = NIL_RTPOLLSET;

    PATSSERVERINST pIt, pItNext;
    RTListForEachSafe(&pThis->LstClientsNew, pIt, pItNext, ATSCLIENTINST, NdLst)
    {
        RTListNodeRemove(&pIt->NdLst);

        RTMemFree(pIt);
        pIt = NULL;
    }

    if (RTCritSectIsInitialized(&pThis->CritSectClients))
    {
        rc = RTCritSectDelete(&pThis->CritSectClients);
        AssertRCReturn(rc, rc);
    }

    return rc;
}

/**
 * Destroys an ATS instance.
 *
 * @returns VBox status code.
 * @param   pThis               ATS instance to destroy.
 */
int AudioTestSvcDestroy(PATSSERVER pThis)
{
    int rc = audioTestSvcDestroyInternal(pThis);
    if (RT_SUCCESS(rc))
    {
        if (pThis->pTransport)
        {
            if (   pThis->pTransport->pfnDestroy
                && pThis->pTransportInst)
            {
                pThis->pTransport->pfnDestroy(pThis->pTransportInst);
                pThis->pTransportInst = NULL;
            }
        }
    }

    return rc;
}
