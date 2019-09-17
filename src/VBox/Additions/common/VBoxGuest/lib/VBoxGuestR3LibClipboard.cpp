/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, Shared Clipboard.
 */

/*
 * Copyright (C) 2007-2019 Oracle Corporation
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
#include <VBox/GuestHost/SharedClipboard.h>
#include <VBox/GuestHost/clipboard-helper.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <VBox/GuestHost/SharedClipboard-uri.h>
#endif
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
# include <iprt/dir.h>
# include <iprt/file.h>
# include <iprt/path.h>
#endif
#include <iprt/string.h>
#include <iprt/cpp/ministring.h>

#include "VBoxGuestR3LibInternal.h"


/*********************************************************************************************************************************
*   Prototypes                                                                                                                   *
*********************************************************************************************************************************/


/**
 * Connects to the Shared Clipboard service, legacy version, do not use anymore.
 *
 * @returns VBox status code
 * @param   pidClient       Where to put the client id on success. The client id
 *                          must be passed to all the other clipboard calls.
 */
VBGLR3DECL(int) VbglR3ClipboardConnect(HGCMCLIENTID *pidClient)
{
    int rc = VbglR3HGCMConnect("VBoxSharedClipboard", pidClient);
    if (rc == VERR_HGCM_SERVICE_NOT_FOUND)
        rc = VINF_PERMISSION_DENIED;

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Connects to the Shared Clipboard service, extended version.
 *
 * @returns VBox status code.
 * @param   pCtx                Shared Clipboard command context to use for the connection.
 */
VBGLR3DECL(int) VbglR3ClipboardConnectEx(PVBGLR3SHCLCMDCTX pCtx)
{
    int rc = VbglR3ClipboardConnect(&pCtx->uClientID);
    if (RT_SUCCESS(rc))
    {
        VBoxShClConnect Msg;
        RT_ZERO(Msg);

        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                           VBOX_SHCL_GUEST_FN_CONNECT, VBOX_SHCL_CPARMS_CONNECT);

        VbglHGCMParmUInt32Set(&Msg.uProtocolVer, 0);
        VbglHGCMParmUInt32Set(&Msg.uProtocolFlags, 0);
        VbglHGCMParmUInt32Set(&Msg.cbChunkSize, 0);
        VbglHGCMParmUInt32Set(&Msg.enmCompression, 0);
        VbglHGCMParmUInt32Set(&Msg.enmChecksumType, 0);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            rc = VbglHGCMParmUInt32Get(&Msg.uProtocolVer, &pCtx->uProtocolVer);
            if (RT_SUCCESS(rc))
                rc = VbglHGCMParmUInt32Get(&Msg.uProtocolFlags, &pCtx->uProtocolFlags);
            if (RT_SUCCESS(rc))
                rc = VbglHGCMParmUInt32Get(&Msg.cbChunkSize, &pCtx->cbChunkSize);

            /** @todo Add / handle checksum + compression type. */
        }
        else
        {
            /* If the above call fails, make sure to use some sane defaults for
             * the old (legacy) protocol. */
            pCtx->uProtocolVer   = 0;
            pCtx->uProtocolFlags = 0;
            pCtx->cbChunkSize    = _64K;

            rc = VINF_SUCCESS; /* Failing above is not fatal. */
        }

        LogFlowFunc(("uProtocolVer=%RU32, cbChunkSize=%RU32\n", pCtx->uProtocolVer, pCtx->cbChunkSize));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Disconnects from the Shared Clipboard service, legacy version, do not use anymore.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 */
VBGLR3DECL(int) VbglR3ClipboardDisconnect(HGCMCLIENTID idClient)
{
    return VbglR3HGCMDisconnect(idClient);
}


/**
 * Disconnects from the Shared Clipboard service, extended version.
 *
 * @returns VBox status code.
 * @param   pCtx                Shared Clipboard command context to use for the connection.
 */
VBGLR3DECL(int) VbglR3ClipboardDisconnectEx(PVBGLR3SHCLCMDCTX pCtx)
{
    int rc = VbglR3ClipboardDisconnect(pCtx->uClientID);
    if (RT_SUCCESS(rc))
    {
        pCtx->uClientID = 0;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


VBGLR3DECL(int) VbglR3ClipboardFormatsReportRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLFORMATDATA pFormats)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pFormats, VERR_INVALID_POINTER);

    VBoxShClFormatsMsg Msg;
    RT_ZERO(Msg);

    if (pCtx->uProtocolVer >= 1)
    {
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                           VBOX_SHCL_GUEST_FN_MSG_GET, 3);

        Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_FORMATS_REPORT);
        Msg.uFormats.SetUInt32(0);
        Msg.fFlags.SetUInt32(0);
    }

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.uFormats.GetUInt32(&pFormats->uFormats);
        if (RT_SUCCESS(rc))
            rc = Msg.fFlags.GetUInt32(&pFormats->fFlags);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


VBGLR3DECL(int) VbglR3ClipboardReadDataRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLDATAREQ pDataReq)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pDataReq, VERR_INVALID_POINTER);

    VBoxShClReadDataReqMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_READ_DATA);

    Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_READ_DATA);
    Msg.uFormat.SetUInt32(0);
    Msg.cbSize.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.uFormat.GetUInt32(&pDataReq->uFmt);
        if (RT_SUCCESS(rc))
            rc = Msg.cbSize.GetUInt32(&pDataReq->cbSize);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}


/**
 * Get a host message, legacy version (protocol v0). Do not use anymore.
 *
 * Note: This is the old message which still is being used for the non-URI Shared Clipboard transfers,
 *       to not break compatibility with older additions / VBox versions.
 *
 * This will block until a message becomes available.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   pidMsg          Where to store the message id.
 * @param   pfFormats       Where to store the format(s) the message applies to.
 */
VBGLR3DECL(int) VbglR3ClipboardGetHostMsgOld(HGCMCLIENTID idClient, uint32_t *pidMsg, uint32_t *pfFormats)
{
    VBoxShClGetHostMsgOld Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHCL_GUEST_FN_GET_HOST_MSG_OLD, VBOX_SHCL_CPARMS_GET_HOST_MSG_OLD);

    VbglHGCMParmUInt32Set(&Msg.msg, 0);
    VbglHGCMParmUInt32Set(&Msg.formats, 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        int rc2 = VbglHGCMParmUInt32Get(&Msg.msg, pidMsg);
        if (RT_SUCCESS(rc))
        {
            rc2 = VbglHGCMParmUInt32Get(&Msg.formats, pfFormats);
            if (RT_SUCCESS(rc2))
                return rc;
        }
        rc = rc2;
    }
    *pidMsg    = UINT32_MAX - 1;
    *pfFormats = UINT32_MAX;
    return rc;
}


/**
 * Reads data from the host clipboard.
 *
 * @returns VBox status code.
 * @retval  VINF_BUFFER_OVERFLOW    If there is more data available than the caller provided buffer space for.
 *
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormat         The format we're requesting the data in.
 * @param   pv              Where to store the data.
 * @param   cb              The size of the buffer pointed to by pv.
 * @param   pcb             The actual size of the host clipboard data. May be larger than cb.
 */
VBGLR3DECL(int) VbglR3ClipboardReadData(HGCMCLIENTID idClient, uint32_t fFormat, void *pv, uint32_t cb, uint32_t *pcb)
{
    VBoxShClReadDataMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHCL_GUEST_FN_DATA_READ, VBOX_SHCL_CPARMS_READ_DATA);
    VbglHGCMParmUInt32Set(&Msg.format, fFormat);
    VbglHGCMParmPtrSet(&Msg.ptr, pv, cb);
    VbglHGCMParmUInt32Set(&Msg.size, 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        uint32_t cbActual;
        int rc2 = VbglHGCMParmUInt32Get(&Msg.size, &cbActual);
        if (RT_SUCCESS(rc2))
        {
            *pcb = cbActual;
            if (cbActual > cb)
                return VINF_BUFFER_OVERFLOW;
            return rc;
        }
        rc = rc2;
    }
    return rc;
}

/**
 * Peeks at the next host message, waiting for one to turn up.
 *
 * @returns VBox status code.
 * @retval  VERR_INTERRUPTED if interrupted.  Does the necessary cleanup, so
 *          caller just have to repeat this call.
 * @retval  VERR_VM_RESTORED if the VM has been restored (idRestoreCheck).
 *
 * @param   pCtx            Shared Clipboard command context to use for the connection.
 * @param   pidMsg          Where to store the message id.
 * @param   pcParameters    Where to store the number  of parameters which will
 *                          be received in a second call to the host.
 * @param   pidRestoreCheck Pointer to the VbglR3GetSessionId() variable to use
 *                          for the VM restore check.  Optional.
 *
 * @note    Restore check is only performed optimally with a 6.0 host.
 */
int VbglR3ClipboardMsgPeekWait(PVBGLR3SHCLCMDCTX pCtx, uint32_t *pidMsg, uint32_t *pcParameters, uint64_t *pidRestoreCheck)
{
    AssertPtrReturn(pidMsg, VERR_INVALID_POINTER);
    AssertPtrReturn(pcParameters, VERR_INVALID_POINTER);

    int rc;

    struct
    {
        VBGLIOCHGCMCALL Hdr;
        HGCMFunctionParameter idMsg;       /* Doubles as restore check on input. */
        HGCMFunctionParameter cParameters;
    } Msg;
    VBGL_HGCM_HDR_INIT(&Msg.Hdr, pCtx->uClientID, VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT, 2);
    VbglHGCMParmUInt64Set(&Msg.idMsg, pidRestoreCheck ? *pidRestoreCheck : 0);
    VbglHGCMParmUInt32Set(&Msg.cParameters, 0);
    rc = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
    LogFlowFunc(("VbglR3HGCMCall -> %Rrc\n", rc));
    if (RT_SUCCESS(rc))
    {
        AssertMsgReturn(   Msg.idMsg.type       == VMMDevHGCMParmType_64bit
                        && Msg.cParameters.type == VMMDevHGCMParmType_32bit,
                        ("msg.type=%d num_parms.type=%d\n", Msg.idMsg.type, Msg.cParameters.type),
                        VERR_INTERNAL_ERROR_3);

        *pidMsg       = (uint32_t)Msg.idMsg.u.value64;
        *pcParameters = Msg.cParameters.u.value32;
        return rc;
    }

    /*
     * If interrupted we must cancel the call so it doesn't prevent us from making another one.
     */
    if (rc == VERR_INTERRUPTED)
    {
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, pCtx->uClientID, VBOX_SHCL_GUEST_FN_CANCEL, 0);
        int rc2 = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg.Hdr));
        AssertRC(rc2);
    }

    /*
     * If restored, update pidRestoreCheck.
     */
    if (rc == VERR_VM_RESTORED && pidRestoreCheck)
        *pidRestoreCheck = Msg.idMsg.u.value64;

    *pidMsg       = UINT32_MAX - 1;
    *pcParameters = UINT32_MAX - 2;
    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static int vbglR3ClipboardRootListHdrRead(PVBGLR3SHCLCMDCTX pCtx, PSHCLROOTLISTHDR pRootListHdr)
{
    AssertPtrReturn(pCtx,         VERR_INVALID_POINTER);
    AssertPtrReturn(pRootListHdr, VERR_INVALID_POINTER);

    VBoxShClRootListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_READ, VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ);

    Msg.ReqParms.uContext.SetUInt32(pCtx->uContextID);
    Msg.ReqParms.fRoots.SetUInt32(0);

    Msg.cRoots.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.ReqParms.fRoots.GetUInt32(&pRootListHdr->fRoots); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.cRoots.GetUInt32(&pRootListHdr->cRoots); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vbglR3ClipboardRootListEntryRead(PVBGLR3SHCLCMDCTX pCtx, uint32_t uIndex, PSHCLROOTLISTENTRY pRootListEntry)
{
    AssertPtrReturn(pCtx,           VERR_INVALID_POINTER);
    AssertPtrReturn(pRootListEntry, VERR_INVALID_POINTER);

    VBoxShClRootListEntryMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_READ, VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY);

    Msg.Parms.uContext.SetUInt32(pCtx->uContextID);
    Msg.Parms.fInfo.SetUInt32(pRootListEntry->fInfo);
    Msg.Parms.uIndex.SetUInt32(uIndex);

    Msg.szName.SetPtr(pRootListEntry->pszName, pRootListEntry->cbName);
    Msg.cbInfo.SetUInt32(pRootListEntry->cbInfo);
    Msg.pvInfo.SetPtr(pRootListEntry->pvInfo, pRootListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.Parms.fInfo.GetUInt32(&pRootListEntry->fInfo); AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            uint32_t cbInfo = 0;
            rc = Msg.cbInfo.GetUInt32(&cbInfo); AssertRC(rc);
            if (pRootListEntry->cbInfo != cbInfo)
                rc = VERR_INVALID_PARAMETER;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardRootListRead(PVBGLR3SHCLCMDCTX pCtx, PSHCLROOTLIST *ppRootList)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(ppRootList, VERR_INVALID_POINTER);

    int rc;

    PSHCLROOTLIST pRootList = SharedClipboardURIRootListAlloc();
    if (pRootList)
    {
        SHCLROOTLISTHDR srcRootListHdr;
        rc = vbglR3ClipboardRootListHdrRead(pCtx, &srcRootListHdr);
        if (RT_SUCCESS(rc))
        {
            pRootList->Hdr.cRoots = srcRootListHdr.cRoots;
            pRootList->Hdr.fRoots = 0; /** @todo Implement this. */

            if (srcRootListHdr.cRoots)
            {
                pRootList->paEntries =
                    (PSHCLROOTLISTENTRY)RTMemAllocZ(srcRootListHdr.cRoots * sizeof(SHCLROOTLISTENTRY));
                if (pRootList->paEntries)
                {
                    for (uint32_t i = 0; i < srcRootListHdr.cRoots; i++)
                    {
                        rc = vbglR3ClipboardRootListEntryRead(pCtx, i, &pRootList->paEntries[i]);
                        if (RT_FAILURE(rc))
                            break;
                    }
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }

        if (RT_SUCCESS(rc))
        {
            *ppRootList = pRootList;
        }
        else
            SharedClipboardURIRootListFree(pRootList);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboarTransferStatusRecv(PVBGLR3SHCLCMDCTX pCtx,
                                                 PSHCLURITRANSFERDIR pEnmDir, PSHCLURITRANSFERREPORT pReport)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pReport, VERR_INVALID_POINTER);
    AssertPtrReturn(pEnmDir, VERR_INVALID_POINTER);

    VBoxShClTransferStatusMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_TRANSFER_STATUS);

    Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_TRANSFER_STATUS);
    Msg.enmDir.SetUInt32(0);
    Msg.enmStatus.SetUInt32(0);
    Msg.rc.SetUInt32(0);
    Msg.fFlags.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.enmDir.GetUInt32((uint32_t *)pEnmDir); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.enmStatus.GetUInt32(&pReport->uStatus); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.rc.GetUInt32((uint32_t *)&pReport->rc); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.fFlags.GetUInt32(&pReport->fFlags); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardTransferStatusReply(PVBGLR3SHCLCMDCTX pCtx, PSHCLURITRANSFER pTransfer,
                                                   SHCLURITRANSFERSTATUS uStatus, int rcTransfer)
{
    AssertPtrReturn(pCtx,      VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    RT_NOREF(pTransfer);

    VBoxShClReplyMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_REPLY, VBOX_SHCL_CPARMS_REPLY_MIN + 1);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.enmType.SetUInt32(VBOX_SHCL_REPLYMSGTYPE_TRANSFER_STATUS);
    Msg.rc.SetUInt32((uint32_t )rcTransfer); /* int vs. uint32_t */
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(NULL, 0);

    Msg.u.TransferStatus.enmStatus.SetUInt32((uint32_t)uStatus);

    LogFlowFunc(("%s\n", VBoxShClTransferStatusToStr(uStatus)));

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardRootListHdrReadReq(PVBGLR3SHCLCMDCTX pCtx, uint32_t *pfRoots)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(pfRoots, VERR_INVALID_POINTER);

    VBoxShClRootListReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_ROOT_LIST_HDR_READ);

    Msg.ReqParms.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_ROOT_LIST_HDR_READ);
    Msg.ReqParms.fRoots.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.ReqParms.uContext.GetUInt32(&pCtx->uContextID); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.fRoots.GetUInt32(pfRoots); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardRootListHdrReadReply(PVBGLR3SHCLCMDCTX pCtx, PSHCLROOTLISTHDR pRootListHdr)
{
    AssertPtrReturn(pCtx,         VERR_INVALID_POINTER);
    AssertPtrReturn(pRootListHdr, VERR_INVALID_POINTER);

    VBoxShClRootListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_ROOT_LIST_HDR_WRITE, VBOX_SHCL_CPARMS_ROOT_LIST_HDR);

    Msg.ReqParms.uContext.SetUInt32(pCtx->uContextID);
    Msg.ReqParms.fRoots.SetUInt32(pRootListHdr->fRoots);

    Msg.cRoots.SetUInt32(pRootListHdr->cRoots);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardRootListEntryReadReq(PVBGLR3SHCLCMDCTX pCtx, uint32_t *puIndex, uint32_t *pfInfo)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(puIndex, VERR_INVALID_POINTER);
    AssertPtrReturn(pfInfo,  VERR_INVALID_POINTER);

    VBoxShClRootListEntryReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY_READ_REQ);

    Msg.Parms.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_ROOT_LIST_ENTRY_READ);
    Msg.Parms.fInfo.SetUInt32(0);
    Msg.Parms.uIndex.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.Parms.uContext.GetUInt32(&pCtx->uContextID); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.Parms.fInfo.GetUInt32(pfInfo); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.Parms.uIndex.GetUInt32(puIndex); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardRootListEntryReadReply(PVBGLR3SHCLCMDCTX pCtx, uint32_t uIndex, PSHCLROOTLISTENTRY pEntry)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pEntry, VERR_INVALID_POINTER);

    VBoxShClRootListEntryMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_ROOT_LIST_ENTRY_WRITE, VBOX_SHCL_CPARMS_ROOT_LIST_ENTRY);

    Msg.Parms.uContext.SetUInt32(pCtx->uContextID);
    Msg.Parms.fInfo.SetUInt32(0);
    Msg.Parms.uIndex.SetUInt32(uIndex);

    Msg.szName.SetPtr(pEntry->pszName, pEntry->cbName);
    Msg.cbInfo.SetUInt32(pEntry->cbInfo);
    Msg.pvInfo.SetPtr(pEntry->pvInfo, pEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListOpenSend(PVBGLR3SHCLCMDCTX pCtx, PSHCLLISTOPENPARMS pOpenParms,
                                            PSHCLLISTHANDLE phList)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,     VERR_INVALID_POINTER);

    VBoxShClListOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_LIST_OPEN, VBOX_SHCL_CPARMS_LIST_OPEN);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.fList.SetUInt32(0);
    Msg.cbFilter.SetUInt32(pOpenParms->cbFilter);
    Msg.pvFilter.SetPtr(pOpenParms->pszFilter, pOpenParms->cbFilter);
    Msg.cbPath.SetUInt32(pOpenParms->cbPath);
    Msg.pvFilter.SetPtr(pOpenParms->pszPath, pOpenParms->cbPath);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uHandle.GetUInt64(phList); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListOpenRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLLISTOPENPARMS pOpenParms)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);

    VBoxShClListOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_LIST_OPEN);

    Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_LIST_OPEN);
    Msg.fList.SetUInt32(0);
    Msg.cbPath.SetUInt32(pOpenParms->cbPath);
    Msg.pvPath.SetPtr(pOpenParms->pszPath, pOpenParms->cbPath);
    Msg.cbFilter.SetUInt32(pOpenParms->cbFilter);
    Msg.pvFilter.SetPtr(pOpenParms->pszFilter, pOpenParms->cbFilter);
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.fList.GetUInt32(&pOpenParms->fList);
        if (RT_SUCCESS(rc))
            rc = Msg.cbFilter.GetUInt32(&pOpenParms->cbFilter);
        if (RT_SUCCESS(rc))
            rc = Msg.cbPath.GetUInt32(&pOpenParms->cbPath);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListOpenReply(PVBGLR3SHCLCMDCTX pCtx, int rcReply, SHCLLISTHANDLE hList)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBoxShClReplyMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_REPLY, VBOX_SHCL_CPARMS_REPLY_MIN + 1);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.enmType.SetUInt32(VBOX_SHCL_REPLYMSGTYPE_LIST_OPEN);
    Msg.rc.SetUInt32((uint32_t)rcReply); /** int vs. uint32_t */
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(NULL, 0);

    Msg.u.ListOpen.uHandle.SetUInt64(hList);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListCloseRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLLISTHANDLE phList)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(phList, VERR_INVALID_POINTER);

    VBoxShClListCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_LIST_CLOSE);

    Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_LIST_CLOSE);
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.uHandle.GetUInt64(phList); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListCloseReply(PVBGLR3SHCLCMDCTX pCtx, int rcReply, SHCLLISTHANDLE hList)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBoxShClReplyMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_REPLY, VBOX_SHCL_CPARMS_REPLY_MIN + 1);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.enmType.SetUInt32(VBOX_SHCL_REPLYMSGTYPE_LIST_CLOSE);
    Msg.rc.SetUInt32((uint32_t)rcReply); /** int vs. uint32_t */
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(NULL, 0);

    Msg.u.ListOpen.uHandle.SetUInt64(hList);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListCloseSend(PVBGLR3SHCLCMDCTX pCtx, SHCLLISTHANDLE hList)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBoxShClListCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_HOST_MSG_URI_LIST_CLOSE, VBOX_SHCL_CPARMS_LIST_CLOSE);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.uHandle.SetUInt64(hList);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}


VBGLR3DECL(int) VbglR3ClipboardListHdrRead(PVBGLR3SHCLCMDCTX pCtx, SHCLLISTHANDLE hList, uint32_t fFlags,
                                           PSHCLLISTHDR pListHdr)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxShClListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_LIST_HDR_READ, VBOX_SHCL_CPARMS_LIST_HDR);

    Msg.ReqParms.uContext.SetUInt32(pCtx->uContextID);
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fFlags.SetUInt32(fFlags);

    Msg.fFeatures.SetUInt32(0);
    Msg.cbTotalSize.SetUInt32(0);
    Msg.cTotalObjects.SetUInt64(0);
    Msg.cbTotalSize.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.fFeatures.GetUInt32(&pListHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = Msg.cTotalObjects.GetUInt64(&pListHdr->cTotalObjects);
        if (RT_SUCCESS(rc))
            rc = Msg.cbTotalSize.GetUInt64(&pListHdr->cbTotalSize);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListHdrReadRecvReq(PVBGLR3SHCLCMDCTX pCtx, PSHCLLISTHANDLE phList, uint32_t *pfFlags)
{
    AssertPtrReturn(pCtx,    VERR_INVALID_POINTER);
    AssertPtrReturn(phList,  VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    VBoxShClListHdrReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_LIST_HDR_READ_REQ);

    Msg.ReqParms.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_LIST_HDR_READ);
    Msg.ReqParms.uHandle.SetUInt64(0);
    Msg.ReqParms.fFlags.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.ReqParms.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.uHandle.GetUInt64(phList);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.fFlags.GetUInt32(pfFlags);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListHdrWrite(PVBGLR3SHCLCMDCTX pCtx, SHCLLISTHANDLE hList,
                                            PSHCLLISTHDR pListHdr)
{
    AssertPtrReturn(pCtx,     VERR_INVALID_POINTER);
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxShClListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE, VBOX_SHCL_CPARMS_LIST_HDR);

    Msg.ReqParms.uContext.SetUInt32(pCtx->uContextID);
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fFlags.SetUInt32(0);

    Msg.fFeatures.SetUInt32(0);
    Msg.cbTotalSize.SetUInt32(pListHdr->fFeatures);
    Msg.cTotalObjects.SetUInt64(pListHdr->cTotalObjects);
    Msg.cbTotalSize.SetUInt64(pListHdr->cbTotalSize);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListEntryRead(PVBGLR3SHCLCMDCTX pCtx, SHCLLISTHANDLE hList,
                                             PSHCLLISTENTRY pListEntry)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);

    VBoxShClListEntryMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ, VBOX_SHCL_CPARMS_LIST_ENTRY);

    Msg.ReqParms.uContext.SetUInt32(pCtx->uContextID);
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fInfo.SetUInt32(0);

    Msg.szName.SetPtr(pListEntry->pszName, pListEntry->cbName);
    Msg.cbInfo.SetUInt32(pListEntry->cbInfo);
    Msg.pvInfo.SetPtr(pListEntry->pvInfo, pListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.cbInfo.GetUInt32(&pListEntry->cbInfo); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListEntryReadRecvReq(PVBGLR3SHCLCMDCTX pCtx, PSHCLLISTHANDLE phList, uint32_t *pfInfo)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(phList, VERR_INVALID_POINTER);
    AssertPtrReturn(pfInfo, VERR_INVALID_POINTER);

    VBoxShClListEntryReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_LIST_ENTRY_READ);

    Msg.ReqParms.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_LIST_ENTRY_READ);
    Msg.ReqParms.uHandle.SetUInt64(0);
    Msg.ReqParms.fInfo.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.ReqParms.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.uHandle.GetUInt64(phList); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.fInfo.GetUInt32(pfInfo); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListEntryWrite(PVBGLR3SHCLCMDCTX pCtx, SHCLLISTHANDLE hList,
                                              PSHCLLISTENTRY pListEntry)
{
    AssertPtrReturn(pCtx,       VERR_INVALID_POINTER);
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);

    VBoxShClListEntryMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE, VBOX_SHCL_CPARMS_LIST_ENTRY);

    Msg.ReqParms.uContext.SetUInt32(pCtx->uContextID);
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fInfo.SetUInt32(pListEntry->fInfo);

    Msg.szName.SetPtr(pListEntry->pszName, pListEntry->cbName);
    Msg.cbInfo.SetUInt32(pListEntry->cbInfo);
    Msg.pvInfo.SetPtr(pListEntry->pvInfo, pListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjOpenRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms)
{
    AssertPtrReturn(pCtx,         VERR_INVALID_POINTER);
    AssertPtrReturn(pCreateParms, VERR_INVALID_POINTER);

    VBoxShClObjOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_OBJ_OPEN);

    Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_OBJ_OPEN);
    Msg.uHandle.SetUInt64(0);
    Msg.cbPath.SetUInt32(pCreateParms->cbPath);
    Msg.szPath.SetPtr(pCreateParms->pszPath, pCreateParms->cbPath);
    Msg.fCreate.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.cbPath.GetUInt32(&pCreateParms->cbPath);
        if (RT_SUCCESS(rc))
            rc = Msg.fCreate.GetUInt32(&pCreateParms->fCreate);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjOpenReply(PVBGLR3SHCLCMDCTX pCtx, int rcReply, SHCLOBJHANDLE hObj)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBoxShClReplyMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_REPLY, VBOX_SHCL_CPARMS_REPLY_MIN + 1);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.enmType.SetUInt32(VBOX_SHCL_REPLYMSGTYPE_OBJ_OPEN);
    Msg.rc.SetUInt32((uint32_t)rcReply); /** int vs. uint32_t */
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(NULL, 0);

    Msg.u.ObjOpen.uHandle.SetUInt64(hObj);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjOpenSend(PVBGLR3SHCLCMDCTX pCtx, PSHCLOBJOPENCREATEPARMS pCreateParms,
                                           PSHCLOBJHANDLE phObj)
{
    AssertPtrReturn(pCtx,         VERR_INVALID_POINTER);
    AssertPtrReturn(pCreateParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phObj,        VERR_INVALID_POINTER);

    VBoxShClObjOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_OBJ_OPEN, VBOX_SHCL_CPARMS_OBJ_OPEN);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.uHandle.SetUInt64(0);
    Msg.cbPath.SetUInt32(pCreateParms->cbPath);
    Msg.szPath.SetPtr((void *)pCreateParms->pszPath, pCreateParms->cbPath + 1 /* Include terminating zero */);
    Msg.fCreate.SetUInt32(pCreateParms->fCreate);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        Msg.uHandle.GetUInt64(phObj);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjCloseRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLOBJHANDLE phObj)
{
    AssertPtrReturn(pCtx,  VERR_INVALID_POINTER);
    AssertPtrReturn(phObj, VERR_INVALID_POINTER);

    VBoxShClObjCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_OBJ_CLOSE);

    Msg.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_OBJ_CLOSE);
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.uHandle.GetUInt64(phObj);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjCloseReply(PVBGLR3SHCLCMDCTX pCtx, int rcReply, SHCLOBJHANDLE hObj)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBoxShClReplyMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_REPLY, VBOX_SHCL_CPARMS_REPLY_MIN + 1);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.enmType.SetUInt32(VBOX_SHCL_REPLYMSGTYPE_OBJ_CLOSE);
    Msg.rc.SetUInt32((uint32_t)rcReply); /** int vs. uint32_t */
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(NULL, 0);

    Msg.u.ObjClose.uHandle.SetUInt64(hObj);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjCloseSend(PVBGLR3SHCLCMDCTX pCtx, SHCLOBJHANDLE hObj)
{
    AssertPtrReturn(pCtx, VERR_INVALID_POINTER);

    VBoxShClObjCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_OBJ_CLOSE, VBOX_SHCL_CPARMS_OBJ_CLOSE);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.uHandle.SetUInt64(hObj);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjReadRecv(PVBGLR3SHCLCMDCTX pCtx, PSHCLOBJHANDLE phObj, uint32_t *pcbToRead,
                                           uint32_t *pfFlags)
{
    AssertPtrReturn(pCtx,      VERR_INVALID_POINTER);
    AssertPtrReturn(phObj,     VERR_INVALID_POINTER);
    AssertPtrReturn(pcbToRead, VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags,   VERR_INVALID_POINTER);

    VBoxShClObjReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_MSG_GET, VBOX_SHCL_CPARMS_OBJ_READ_REQ);

    Msg.ReqParms.uContext.SetUInt32(VBOX_SHCL_HOST_MSG_URI_OBJ_READ);
    Msg.ReqParms.uHandle.SetUInt64(0);
    Msg.ReqParms.cbToRead.SetUInt32(0);
    Msg.ReqParms.fRead.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.ReqParms.uContext.GetUInt32(&pCtx->uContextID);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.uHandle.GetUInt64(phObj);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.cbToRead.GetUInt32(pcbToRead);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.fRead.GetUInt32(pfFlags);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjRead(PVBGLR3SHCLCMDCTX pCtx, SHCLOBJHANDLE hObj,
                                       void *pvData, uint32_t cbData, uint32_t *pcbRead)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData,    VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    VBoxShClObjReadWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_OBJ_READ, VBOX_SHCL_CPARMS_OBJ_READ);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.uHandle.SetUInt64(hObj);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(cbData);
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Add checksum support. */

        if (pcbRead)
        {
            rc = Msg.cbData.GetUInt32(pcbRead); AssertRC(rc);
            AssertReturn(cbData >= *pcbRead, VERR_TOO_MUCH_DATA);
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjWrite(PVBGLR3SHCLCMDCTX pCtx, SHCLOBJHANDLE hObj,
                                        void *pvData, uint32_t cbData, uint32_t *pcbWritten)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    /* cbData can be 0. */
    /* pcbWritten is optional. */

    VBoxShClObjReadWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                       VBOX_SHCL_GUEST_FN_OBJ_WRITE, VBOX_SHCL_CPARMS_OBJ_WRITE);

    Msg.uContext.SetUInt32(pCtx->uContextID);
    Msg.uHandle.SetUInt64(hObj);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(cbData);
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Add checksum support. */

        if (pcbWritten)
            *pcbWritten = cbData; /** @todo For now return all as being written. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Starts a transfer on the guest side.
 *
 * @returns VBox status code.
 * @param   pCmdCtx             Command context to use.
 * @param   pTransferCtx        Transfer context to create transfer for.
 * @param   uTransferID         ID to use for transfer to start.
 * @param   enmDir              Direction of transfer to start.
 * @param   enmSource           Source of transfer to start.
 * @param   ppTransfer          Where to return the transfer object on success. Optional.
 */
static int vbglR3ClipboardTransferStart(PVBGLR3SHCLCMDCTX pCmdCtx, PSHCLURICTX pTransferCtx,
                                        SHCLURITRANSFERID uTransferID, SHCLURITRANSFERDIR enmDir, SHCLSOURCE enmSource,
                                        PSHCLURITRANSFER *ppTransfer)
{
    PSHCLURITRANSFER pTransfer;
    int rc = SharedClipboardURITransferCreate(&pTransfer);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardURICtxTransferRegisterByIndex(pTransferCtx, pTransfer, uTransferID);
        if (RT_SUCCESS(rc))
        {
            rc = SharedClipboardURITransferInit(pTransfer, uTransferID, enmDir, enmSource);
            if (RT_FAILURE(rc))
                SharedClipboardURICtxTransferUnregister(pTransferCtx, uTransferID);
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (ppTransfer)
            *ppTransfer = pTransfer;

        LogRel2(("Shared Clipboard: Transfer ID=%RU16 successfully started\n", uTransferID));
    }
    else
        LogRel(("Shared Clipboard: Unable to start transfer ID=%RU16, rc=%Rrc\n", uTransferID, rc));

    /* Send a reply in any case. */
    int rc2 = VbglR3ClipboardTransferStatusReply(pCmdCtx, pTransfer,
                                                   RT_SUCCESS(rc)
                                                 ? SHCLURITRANSFERSTATUS_STARTED : SHCLURITRANSFERSTATUS_ERROR, rc);
    AssertRC(rc2);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Stops a transfer on the guest side.
 *
 * @returns VBox status code, or VERR_NOT_FOUND if transfer has not been found.
 * @param   pCmdCtx             Command context to use.
 * @param   pTransferCtx        Transfer context to stop transfer for.
 * @param   uTransferID         ID of transfer to stop.
 */
static int vbglR3ClipboardTransferStop(PVBGLR3SHCLCMDCTX pCmdCtx, PSHCLURICTX pTransferCtx,
                                       SHCLURITRANSFERID uTransferID)
{
    int rc;

    PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx, uTransferID);
    if (pTransfer)
    {
        rc = SharedClipboardURITransferClose(pTransfer);
        if (RT_SUCCESS(rc))
            rc = SharedClipboardURICtxTransferUnregister(pTransferCtx, uTransferID);

        if (RT_SUCCESS(rc))
        {
            LogRel2(("Shared Clipboard: Transfer ID=%RU16 successfully stopped\n", uTransferID));
        }
        else
            LogRel(("Shared Clipboard: Unable to stop transfer ID=%RU16, rc=%Rrc\n", uTransferID, rc));

        /* Send a reply in any case. */
        int rc2 = VbglR3ClipboardTransferStatusReply(pCmdCtx, pTransfer,
                                                       RT_SUCCESS(rc)
                                                     ? SHCLURITRANSFERSTATUS_STOPPED : SHCLURITRANSFERSTATUS_ERROR, rc);
        AssertRC(rc2);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardEventGetNextEx(uint32_t idMsg, uint32_t cParms,
                                              PVBGLR3SHCLCMDCTX pCmdCtx, PSHCLURICTX pTransferCtx,
                                              PVBGLR3CLIPBOARDEVENT pEvent)
{
    AssertPtrReturn(pCmdCtx,      VERR_INVALID_POINTER);
    AssertPtrReturn(pTransferCtx, VERR_INVALID_POINTER);
    AssertPtrReturn(pEvent,       VERR_INVALID_POINTER);

    LogFunc(("Handling idMsg=%RU32 (%s), cParms=%RU32\n", idMsg, VBoxShClHostMsgToStr(idMsg), cParms));

    int rc;

    switch (idMsg)
    {
        case VBOX_SHCL_HOST_MSG_URI_TRANSFER_STATUS:
        {
            SHCLURITRANSFERDIR    enmDir;
            SHCLURITRANSFERREPORT transferReport;
            rc = VbglR3ClipboarTransferStatusRecv(pCmdCtx, &enmDir, &transferReport);
            if (RT_SUCCESS(rc))
            {
                const SHCLURITRANSFERID uTransferID = VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID);

                LogFlowFunc(("[Transfer %RU16] %s\n", uTransferID, VBoxShClTransferStatusToStr(transferReport.uStatus)));

                switch (transferReport.uStatus)
                {
                    case SHCLURITRANSFERSTATUS_READY:
                        RT_FALL_THROUGH();
                    case SHCLURITRANSFERSTATUS_STARTED:
                    {
                        SHCLSOURCE enmSource = enmDir == SHCLURITRANSFERDIR_READ
                                             ? SHCLSOURCE_LOCAL
                                             : SHCLSOURCE_REMOTE;

                        rc = vbglR3ClipboardTransferStart(pCmdCtx, pTransferCtx, uTransferID,
                                                          enmDir, enmSource, NULL /* ppTransfer */);
                        if (RT_SUCCESS(rc))
                        {

                        }
                        break;
                    }

                    case SHCLURITRANSFERSTATUS_CANCELED:
                        RT_FALL_THROUGH();
                    case SHCLURITRANSFERSTATUS_KILLED:
                        RT_FALL_THROUGH();
                    case SHCLURITRANSFERSTATUS_ERROR:
                    {
                        rc = vbglR3ClipboardTransferStop(pCmdCtx, pTransferCtx,
                                                         VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                        break;
                    }

                    default:
                        rc = VERR_NOT_SUPPORTED;
                        break;
                }

                if (RT_SUCCESS(rc))
                {
                    pEvent->u.TransferStatus.enmDir = enmDir;
                    pEvent->u.TransferStatus.Report = transferReport;
                    pEvent->u.TransferStatus.uID    = VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID);

                    pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_TRANSFER_STATUS;

                    LogRel2(("Shared Clipboard: Received status %RU32 (%Rrc) for transfer ID=%RU16\n",
                             pEvent->u.TransferStatus.Report.uStatus, pEvent->u.TransferStatus.Report.rc,
                             pEvent->u.TransferStatus.uID));
                }
            }
            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_ROOT_LIST_HDR_READ:
        {
            uint32_t fRoots;
            rc = VbglR3ClipboardRootListHdrReadReq(pCmdCtx, &fRoots);

            /** @todo Validate / handle fRoots. */

            if (RT_SUCCESS(rc))
            {
                PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                              VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                SHCLROOTLISTHDR rootListHdr;
                RT_ZERO(rootListHdr);

                rootListHdr.cRoots = SharedClipboardURILTransferRootsCount(pTransfer);

                LogFlowFunc(("cRoots=%RU32\n", rootListHdr.cRoots));

                rc = VbglR3ClipboardRootListHdrReadReply(pCmdCtx, &rootListHdr);
            }
            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_ROOT_LIST_ENTRY_READ:
        {
            uint32_t uIndex;
            uint32_t fInfo;
            rc = VbglR3ClipboardRootListEntryReadReq(pCmdCtx, &uIndex, &fInfo);
            if (RT_SUCCESS(rc))
            {
                PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                              VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                SHCLROOTLISTENTRY rootListEntry;
                rc = SharedClipboardURILTransferRootsEntry(pTransfer, uIndex, &rootListEntry);
                if (RT_SUCCESS(rc))
                    rc = VbglR3ClipboardRootListEntryReadReply(pCmdCtx, uIndex, &rootListEntry);
            }
            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_LIST_OPEN:
        {
            SHCLLISTOPENPARMS openParmsList;
            rc = SharedClipboardURIListOpenParmsInit(&openParmsList);
            if (RT_SUCCESS(rc))
            {
                rc = VbglR3ClipboardListOpenRecv(pCmdCtx, &openParmsList);
                if (RT_SUCCESS(rc))
                {
                    PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                                  VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                    AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                    LogFlowFunc(("pszPath=%s\n", openParmsList.pszPath));

                    SHCLLISTHANDLE hList = SHCLLISTHANDLE_INVALID;
                    rc = SharedClipboardURITransferListOpen(pTransfer, &openParmsList, &hList);

                    /* Reply in any case. */
                    int rc2 = VbglR3ClipboardListOpenReply(pCmdCtx, rc, hList);
                    AssertRC(rc2);
                }

                SharedClipboardURIListOpenParmsDestroy(&openParmsList);
            }

            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_LIST_CLOSE:
        {
            SHCLLISTHANDLE hList;
            rc = VbglR3ClipboardListCloseRecv(pCmdCtx, &hList);
            if (RT_SUCCESS(rc))
            {
                PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                              VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                rc = SharedClipboardURITransferListClose(pTransfer, hList);

                /* Reply in any case. */
                int rc2 = VbglR3ClipboardListCloseReply(pCmdCtx, rc, hList);
                AssertRC(rc2);
            }

            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_LIST_HDR_READ:
        {
            /** @todo Handle filter + list features. */

            SHCLLISTHANDLE hList  = SHCLLISTHANDLE_INVALID;
            uint32_t                  fFlags = 0;
            rc = VbglR3ClipboardListHdrReadRecvReq(pCmdCtx, &hList, &fFlags);
            if (RT_SUCCESS(rc))
            {
                PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                              VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                SHCLLISTHDR hdrList;
                rc = SharedClipboardURITransferListGetHeader(pTransfer, hList, &hdrList);
                if (RT_SUCCESS(rc))
                {
                    rc = VbglR3ClipboardListHdrWrite(pCmdCtx, hList, &hdrList);

                    SharedClipboardURIListHdrDestroy(&hdrList);
                }
            }

            break;
        }

    #if 0
        case VBOX_SHCL_HOST_MSG_URI_LIST_HDR_WRITE:
        {
            LogFlowFunc(("VBOX_SHCL_HOST_MSG_URI_LIST_HDR_WRITE\n"));

            SHCLLISTHDR hdrList;
            rc = SharedClipboardURIListHdrInit(&hdrList);
            if (RT_SUCCESS(rc))
            {
                rc = VBglR3ClipboardListHdrRecv(pCtx, )
            }
            break;
        }
    #endif

        case VBOX_SHCL_HOST_MSG_URI_LIST_ENTRY_READ:
        {
            LogFlowFunc(("VBOX_SHCL_HOST_MSG_URI_LIST_ENTRY_READ\n"));

            SHCLLISTENTRY entryList;
            rc = SharedClipboardURIListEntryInit(&entryList);
            if (RT_SUCCESS(rc))
            {
                SHCLLISTHANDLE hList;
                uint32_t                  fInfo;
                rc = VbglR3ClipboardListEntryReadRecvReq(pCmdCtx, &hList, &fInfo);
                if (RT_SUCCESS(rc))
                {
                    PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                                  VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                    AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                    rc = SharedClipboardURITransferListRead(pTransfer, hList, &entryList);
                    if (RT_SUCCESS(rc))
                    {
                        PSHCLFSOBJINFO pObjInfo = (PSHCLFSOBJINFO)entryList.pvInfo;
                        Assert(entryList.cbInfo == sizeof(SHCLFSOBJINFO));

                        RT_NOREF(pObjInfo);

                        LogFlowFunc(("\t%s (%RU64 bytes)\n", entryList.pszName, pObjInfo->cbObject));

                        rc = VbglR3ClipboardListEntryWrite(pCmdCtx, hList, &entryList);
                    }
                }

                SharedClipboardURIListEntryDestroy(&entryList);
            }

            break;
        }

    #if 0
        case VBOX_SHCL_HOST_MSG_URI_LIST_ENTRY_WRITE:
        {
            LogFlowFunc(("VBOX_SHCL_HOST_MSG_URI_LIST_ENTRY_WRITE\n"));
            pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_LIST_ENTRY_WRITE;
            break;
        }
    #endif

        case VBOX_SHCL_HOST_MSG_URI_OBJ_OPEN:
        {
            SHCLOBJOPENCREATEPARMS openParms;
            rc = SharedClipboardURIObjectOpenParmsInit(&openParms);
            if (RT_SUCCESS(rc))
            {
                rc = VbglR3ClipboardObjOpenRecv(pCmdCtx, &openParms);
                if (RT_SUCCESS(rc))
                {
                    PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                                  VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                    AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                    SHCLOBJHANDLE hObj;
                    rc = SharedClipboardURIObjectOpen(pTransfer, &openParms, &hObj);

                    /* Reply in any case. */
                    int rc2 = VbglR3ClipboardObjOpenReply(pCmdCtx, rc, hObj);
                    AssertRC(rc2);
                }

                SharedClipboardURIObjectOpenParmsDestroy(&openParms);
            }

            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_OBJ_CLOSE:
        {
            SHCLOBJHANDLE hObj;
            rc = VbglR3ClipboardObjCloseRecv(pCmdCtx, &hObj);
            if (RT_SUCCESS(rc))
            {
                PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                              VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                rc = SharedClipboardURIObjectClose(pTransfer, hObj);

                /* Reply in any case. */
                int rc2 = VbglR3ClipboardObjCloseReply(pCmdCtx, rc, hObj);
                AssertRC(rc2);
            }

            break;
        }

        case VBOX_SHCL_HOST_MSG_URI_OBJ_READ:
        {
            SHCLOBJHANDLE hObj;
            uint32_t cbBuf;
            uint32_t fFlags;
            rc = VbglR3ClipboardObjReadRecv(pCmdCtx, &hObj, &cbBuf, &fFlags);
            if (RT_SUCCESS(rc))
            {
                PSHCLURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(pTransferCtx,
                                                                              VBO_SHARED_CLIPBOARD_CONTEXTID_GET_TRANSFER(pCmdCtx->uContextID));
                AssertPtrBreakStmt(pTransfer, VERR_NOT_FOUND);

                AssertBreakStmt(pCmdCtx->cbChunkSize, rc = VERR_INVALID_PARAMETER);

                const uint32_t cbToRead = RT_MIN(cbBuf, pCmdCtx->cbChunkSize);

                LogFlowFunc(("hObj=%RU64, cbBuf=%RU32, fFlags=0x%x -> cbChunkSize=%RU32, cbToRead=%RU32\n",
                             hObj, cbBuf, fFlags, pCmdCtx->cbChunkSize, cbToRead));

                void *pvBuf = RTMemAlloc(cbToRead);
                if (pvBuf)
                {
                    uint32_t cbRead;
                    rc = SharedClipboardURIObjectRead(pTransfer, hObj, pvBuf, cbToRead, &cbRead, fFlags);
                    if (RT_SUCCESS(rc))
                        rc = VbglR3ClipboardObjWrite(pCmdCtx, hObj, pvBuf, cbRead, NULL /* pcbWritten */);

                    RTMemFree(pvBuf);
                }
                else
                    rc = VERR_NO_MEMORY;
            }

            break;
        }

    #if 0
        case VBOX_SHCL_HOST_MSG_URI_OBJ_WRITE:
        {
            LogFlowFunc(("VBOX_SHCL_HOST_MSG_URI_OBJ_WRITE\n"));
            break;
        }
    #endif

        default:
        {
            rc = VbglR3ClipboardEventGetNext(idMsg, cParms, pCmdCtx, pEvent);
            break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

VBGLR3DECL(int) VbglR3ClipboardEventGetNext(uint32_t idMsg, uint32_t cParms,
                                            PVBGLR3SHCLCMDCTX pCtx, PVBGLR3CLIPBOARDEVENT pEvent)
{
    AssertPtrReturn(pCtx,   VERR_INVALID_POINTER);
    AssertPtrReturn(pEvent, VERR_INVALID_POINTER);

    RT_NOREF(cParms);

    int rc;

#ifdef LOG_ENABLED
    LogFunc(("Handling idMsg=%RU32 (%s), protocol v%RU32\n", idMsg, VBoxShClHostMsgToStr(idMsg), pCtx->uProtocolVer));
#endif
    switch (idMsg)
    {
        case VBOX_SHCL_HOST_MSG_FORMATS_REPORT:
        {
            rc = VbglR3ClipboardFormatsReportRecv(pCtx, &pEvent->u.ReportedFormats);
            if (RT_SUCCESS(rc))
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_REPORT_FORMATS;
            break;
        }

        case VBOX_SHCL_HOST_MSG_READ_DATA:
        {
            rc = VbglR3ClipboardReadDataRecv(pCtx, &pEvent->u.ReadData);
            if (RT_SUCCESS(rc))
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_READ_DATA;
            break;
        }

        default:
        {
            rc = VERR_NOT_SUPPORTED;
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        /* Copy over our command context to the event. */
        pEvent->cmdCtx = *pCtx;
    }
    else
    {
        /* Report error back to the host. */
        int rc2 = VbglR3ClipboardWriteError(pCtx->uClientID, rc);
        AssertRC(rc2);

   }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Frees (destroys) a formerly allocated Shared Clipboard event.
 *
 * @returns IPRT status code.
 * @param   pEvent              Event to free (destroy).
 */
VBGLR3DECL(void) VbglR3ClipboardEventFree(PVBGLR3CLIPBOARDEVENT pEvent)
{
    if (!pEvent)
        return;

    /* Some messages require additional cleanup. */
    switch (pEvent->enmType)
    {
        default:
            break;
    }

    RTMemFree(pEvent);
    pEvent = NULL;
}

#if 0
VBGLR3DECL(int) VbglR3ClipboardListHdrReadRecv(HGCMCLIENTID idClient, PSHCLLISTHANDLE phList)
{
    AssertPtrReturn(phList, VERR_INVALID_POINTER);

    VBoxShClListHdrReadMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHCL_GUEST_FN_LIST_HDR_READ, VBOX_SHCL_CPARMS_LIST_HDR_READ);

    Msg.uContext.SetUInt32(0); /** @todo Not used yet. */
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        Msg.uHandle.GetUInt32(phList);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sends a list header to the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   hList           List handle to send header for.
 * @param   pListHdr        List header to send.
 */
VBGLR3DECL(int) VbglR3ClipboardListHdrSend(HGCMCLIENTID idClient,
                                           SHCLLISTHANDLE hList, PSHCLLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxShClListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHCL_GUEST_FN_LIST_HDR_WRITE, VBOX_SHCL_CPARMS_LIST_HDR);

    Msg.uContext.SetUInt32(0); /** @todo Not used yet. */
    Msg.fFeatures.SetUInt32(pListHdr->fFeatures);
    Msg.cTotalObjects.SetUInt64(pListHdr->cTotalObjects);
    Msg.cbTotalSize.SetUInt64(pListHdr->cbTotalSize);
    Msg.enmCompression.SetUInt32(pListHdr->enmCompression);
    Msg.enmChecksumType.SetUInt32(RTDIGESTTYPE_INVALID);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sends a list entry to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect()
 * @param   hList               List handle to send entry for.
 * @param   pListEntry          List entry to send.
 */
VBGLR3DECL(int) VbglR3ClipboardSendListEntryWrite(HGCMCLIENTID idClient,
                                                  SHCLLISTHANDLE hList, PSHCLLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);

    VBoxShClListEntryWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHCL_GUEST_FN_LIST_ENTRY_WRITE, VBOX_SHCL_CPARMS_LIST_ENTRY_WRITE);

    Msg.uContext.SetUInt32(0); /** @todo Not used yet. */
    Msg.uHandle.SetUInt64(hList);
    Msg.fInfo.SetUInt32(pListEntry->fInfo);
    Msg.cbInfo.SetUInt32(pListEntry->cbInfo);
    Msg.pvInfo.SetPtr(pListEntry->pvInfo, pListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif

/**
 * Sends (reports) guest clipboard formats to the host.
 *
 * @returns VBox status code.
 * @param   pCtx                The command context returned by VbglR3ClipboardConnect().
 * @param   pFormats            The formats to send (report).
 */
VBGLR3DECL(int) VbglR3ClipboardFormatsSend(PVBGLR3SHCLCMDCTX pCtx, PSHCLFORMATDATA pFormats)
{
    VBoxShClFormatsMsg Msg;

    if (pCtx->uProtocolVer == 0)
    {
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, VBOX_SHCL_GUEST_FN_FORMATS_REPORT, 1);
        VbglHGCMParmUInt32Set(&Msg.uFormats, pFormats->uFormats);
    }
    else
    {
        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID, VBOX_SHCL_GUEST_FN_FORMATS_REPORT, 3);

        Msg.uContext.SetUInt32(pCtx->uContextID);
        Msg.uFormats.SetUInt32(pFormats->uFormats);
        Msg.fFlags.SetUInt32(pFormats->fFlags);
    }

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reports (advertises) guest clipboard formats to the host.
 *
 * Legacy function, do not use anymore.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormats        The formats to advertise.
 */
VBGLR3DECL(int) VbglR3ClipboardReportFormats(HGCMCLIENTID idClient, uint32_t fFormats)
{
    VBoxShClFormatsMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHCL_GUEST_FN_FORMATS_REPORT, 1);
    VbglHGCMParmUInt32Set(&Msg.uFormats, fFormats);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Sends guest clipboard data to the host. Legacy function kept for compatibility, do not use anymore.
 *
 * This is usually called in reply to a VBOX_SHCL_HOST_MSG_READ_DATA message
 * from the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormat         The format of the data.
 * @param   pv              The data.
 * @param   cb              The size of the data.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteData(HGCMCLIENTID idClient, uint32_t fFormat, void *pv, uint32_t cb)
{
    VBoxShClWriteDataMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHCL_GUEST_FN_DATA_WRITE, 2);

    VbglHGCMParmUInt32Set(&Msg.u.v0.format, fFormat);
    VbglHGCMParmPtrSet(&Msg.u.v0.ptr, pv, cb);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sends guest clipboard data to the host.
 *
 * This is usually called in reply to a VBOX_SHCL_HOST_MSG_READ_DATA message
 * from the host.
 *
 * @returns VBox status code.
 * @param   pCtx                The command context returned by VbglR3ClipboardConnect().
 * @param   pData               Clipboard data to send.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteDataEx(PVBGLR3SHCLCMDCTX pCtx, PSHCLDATABLOCK pData)
{
    int rc;

    if (pCtx->uProtocolVer == 0)
    {
        rc = VbglR3ClipboardWriteData(pCtx->uClientID, pData->uFormat, pData->pvData, pData->cbData);
    }
    else
    {
        VBoxShClWriteDataMsg Msg;
        RT_ZERO(Msg);

        VBGL_HGCM_HDR_INIT(&Msg.hdr, pCtx->uClientID,
                           VBOX_SHCL_GUEST_FN_DATA_WRITE, VBOX_SHCL_CPARMS_WRITE_DATA);

        LogFlowFunc(("CID=%RU32\n", pCtx->uContextID));

        Msg.u.v1.uContext.SetUInt32(pCtx->uContextID);
        Msg.u.v1.uFormat.SetUInt32(pData->uFormat);
        Msg.u.v1.cbData.SetUInt32(pData->cbData);
        Msg.u.v1.pvData.SetPtr(pData->pvData, pData->cbData);

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes an error to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   rcErr               Error (IPRT-style) to send.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteError(HGCMCLIENTID idClient, int rcErr)
{
    VBoxShClWriteErrorMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHCL_GUEST_FN_ERROR, VBOX_SHCL_CPARMS_ERROR);

    /** @todo Context ID not used yet. */
    Msg.uContext.SetUInt32(0);
    Msg.rc.SetUInt32((uint32_t)rcErr); /* uint32_t vs. int. */

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    if (RT_FAILURE(rc))
        LogFlowFunc(("Sending error %Rrc failed with rc=%Rrc\n", rcErr, rc));
    if (rc == VERR_NOT_SUPPORTED)
        rc = VINF_SUCCESS;

    if (RT_FAILURE(rc))
        LogRel(("Shared Clipboard: Reporting error %Rrc to the host failed with %Rrc\n", rcErr, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

