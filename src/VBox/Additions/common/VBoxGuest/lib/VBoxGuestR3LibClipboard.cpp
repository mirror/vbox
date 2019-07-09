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
 * Connects to the clipboard service.
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
 * Disconnect from the clipboard service.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 */
VBGLR3DECL(int) VbglR3ClipboardDisconnect(HGCMCLIENTID idClient)
{
    return VbglR3HGCMDisconnect(idClient);
}


/**
 * Get a host message, old version.
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
    VBoxClipboardGetHostMsgOld Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG_OLD, VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG_OLD);

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
    VBoxClipboardReadDataMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA, VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA);
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

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
#if 0
static int vbglR3ClipboardPeekMsg(HGCMCLIENTID idClient, uint32_t *puMsg, uint32_t *pcParms, bool fWait)
{
    AssertPtrReturn(puMsg,   VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    VBoxClipboardPeekMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_GET_HOST_MSG, VBOX_SHARED_CLIPBOARD_CPARMS_GET_HOST_MSG);

    Msg.uMsg.SetUInt32(0);
    Msg.cParms.SetUInt32(0);
    Msg.fBlock.SetUInt32(fWait ? 1 : 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uMsg.GetUInt32(puMsg);
        if (RT_SUCCESS(rc))
            rc = Msg.cParms.GetUInt32(pcParms);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#else
/**
 * Peeks at the next host message, waiting for one to turn up.
 *
 * @returns VBox status code.
 * @retval  VERR_INTERRUPTED if interrupted.  Does the necessary cleanup, so
 *          caller just have to repeat this call.
 * @retval  VERR_VM_RESTORED if the VM has been restored (idRestoreCheck).
 *
 * @param   idClient        The client ID returned by VbglR3GuestCtrlConnect().
 * @param   pidMsg          Where to store the message id.
 * @param   pcParameters    Where to store the number  of parameters which will
 *                          be received in a second call to the host.
 * @param   pidRestoreCheck Pointer to the VbglR3GetSessionId() variable to use
 *                          for the VM restore check.  Optional.
 *
 * @note    Restore check is only performed optimally with a 6.0 host.
 */
static int vbglR3ClipboardMsgPeekWait(uint32_t idClient, uint32_t *pidMsg, uint32_t *pcParameters, uint64_t *pidRestoreCheck)
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
    VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_WAIT, 2);
    VbglHGCMParmUInt64Set(&Msg.idMsg, pidRestoreCheck ? *pidRestoreCheck : 0);
    VbglHGCMParmUInt32Set(&Msg.cParameters, 0);
    rc = VbglR3HGCMCall(&Msg.Hdr, sizeof(Msg));
    LogRel2(("VbglR3GuestCtrlMsgPeekWait -> %Rrc\n", rc));
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
        VBGL_HGCM_HDR_INIT(&Msg.Hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_CANCEL, 0);
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
#endif

VBGLR3DECL(int) VbglR3ClipboardTransferSendStatus(HGCMCLIENTID idClient, SHAREDCLIPBOARDURITRANSFERSTATUS uStatus)
{
    VBoxClipboardStatusMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS, VBOX_SHARED_CLIPBOARD_CPARMS_STATUS);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.uStatus.SetUInt32(uStatus);
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(NULL, 0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListOpenSend(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTOPENPARMS pOpenParms,
                                            PSHAREDCLIPBOARDLISTHANDLE phList)
{
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,     VERR_INVALID_POINTER);

    VBoxClipboardListOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
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

VBGLR3DECL(int) VbglR3ClipboardListOpenRecv(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTOPENPARMS pOpenParms)
{
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);

    VBoxClipboardListOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN);

    Msg.uContext.SetUInt32(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN);
    Msg.fList.SetUInt32(0);
    Msg.cbPath.SetUInt32(pOpenParms->cbPath);
    Msg.pvPath.SetPtr(pOpenParms->pszPath, pOpenParms->cbPath);
    Msg.cbFilter.SetUInt32(pOpenParms->cbFilter);
    Msg.pvFilter.SetPtr(pOpenParms->pszFilter, pOpenParms->cbFilter);
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.fList.GetUInt32(&pOpenParms->fList);
        if (RT_SUCCESS(rc))
            rc = Msg.cbFilter.GetUInt32(&pOpenParms->cbFilter);
        if (RT_SUCCESS(rc))
            rc = Msg.cbPath.GetUInt32(&pOpenParms->cbPath);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListOpenReply(HGCMCLIENTID idClient, int rcReply, SHAREDCLIPBOARDLISTHANDLE hList)
{
    VBoxClipboardReplyMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_REPLY, 6);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.enmType.SetUInt32(VBOX_SHAREDCLIPBOARD_REPLYMSGTYPE_LIST_OPEN);
    Msg.rc.SetUInt32((uint32_t)rcReply); /** int vs. uint32_t */
    Msg.cbPayload.SetUInt32(0);
    Msg.pvPayload.SetPtr(0, NULL);

    Msg.u.ListOpen.uHandle.SetUInt64(hList);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListCloseSend(HGCMCLIENTID idClient, SHAREDCLIPBOARDLISTHANDLE hList)
{
    VBoxClipboardListCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.uHandle.SetUInt64(hList);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListCloseRecv(HGCMCLIENTID idClient, PSHAREDCLIPBOARDLISTHANDLE phList)
{
    AssertPtrReturn(phList, VERR_INVALID_POINTER);

    VBoxClipboardListCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE);

    Msg.uContext.SetUInt32(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE);
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uHandle.GetUInt64(phList); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListHdrRead(HGCMCLIENTID idClient, SHAREDCLIPBOARDLISTHANDLE hList, uint32_t fFlags,
                                           PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxClipboardListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR);

    Msg.ReqParms.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fFlags.SetUInt32(fFlags);

    Msg.fFeatures.SetUInt32(0);
    Msg.cbTotalSize.SetUInt32(0);
    Msg.cTotalObjects.SetUInt64(0);
    Msg.cbTotalSize.SetUInt64(0);
    Msg.enmCompression.SetUInt32(0);
    Msg.enmChecksumType.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.fFeatures.GetUInt32(&pListHdr->fFeatures);
        if (RT_SUCCESS(rc))
            rc = Msg.cTotalObjects.GetUInt64(&pListHdr->cTotalObjects);
        if (RT_SUCCESS(rc))
            rc = Msg.cbTotalSize.GetUInt64(&pListHdr->cbTotalSize);
        if (RT_SUCCESS(rc))
            rc = Msg.enmCompression.GetUInt32(&pListHdr->enmCompression);
        if (RT_SUCCESS(rc))
            rc = Msg.enmChecksumType.GetUInt32(&pListHdr->enmChecksumType);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListHdrReadRecvReq(HGCMCLIENTID idClient, PSHAREDCLIPBOARDLISTHANDLE phList, uint32_t *pfFlags)
{
    AssertPtrReturn(phList,  VERR_INVALID_POINTER);
    AssertPtrReturn(pfFlags, VERR_INVALID_POINTER);

    VBoxClipboardListHdrReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ_REQ);

    Msg.ReqParms.uContext.SetUInt32(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ);
    Msg.ReqParms.uHandle.SetUInt64(0);
    Msg.ReqParms.fFlags.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.uHandle.GetUInt64(phList);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.fFlags.GetUInt32(pfFlags);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListHdrWrite(HGCMCLIENTID idClient, SHAREDCLIPBOARDLISTHANDLE hList,
                                            PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxClipboardListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR);

    Msg.ReqParms.uContext.SetUInt32(0);
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fFlags.SetUInt32(0);

    Msg.fFeatures.SetUInt32(0);
    Msg.cbTotalSize.SetUInt32(pListHdr->fFeatures);
    Msg.cTotalObjects.SetUInt64(pListHdr->cTotalObjects);
    Msg.cbTotalSize.SetUInt64(pListHdr->cbTotalSize);
    Msg.enmCompression.SetUInt32(pListHdr->enmCompression);
    Msg.enmChecksumType.SetUInt32(pListHdr->enmChecksumType);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListEntryRead(HGCMCLIENTID idClient, SHAREDCLIPBOARDLISTHANDLE hList,
                                             PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);

    VBoxClipboardListEntryMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY);

    Msg.ReqParms.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fInfo.SetUInt32(0);

    Msg.cbInfo.SetUInt32(0);
    Msg.pvInfo.SetPtr(pListEntry->pvInfo, pListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.cbInfo.GetUInt32(&pListEntry->cbInfo); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListEntryReadRecvReq(HGCMCLIENTID idClient, PSHAREDCLIPBOARDLISTHANDLE phList, uint32_t *pfInfo)
{
    AssertPtrReturn(phList, VERR_INVALID_POINTER);
    AssertPtrReturn(pfInfo, VERR_INVALID_POINTER);

    VBoxClipboardListEntryReadReqMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ_REQ);

    Msg.ReqParms.uContext.SetUInt32(VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ);
    Msg.ReqParms.uHandle.SetUInt64(0);
    Msg.ReqParms.fInfo.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.ReqParms.uHandle.GetUInt64(phList); AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = Msg.ReqParms.fInfo.GetUInt32(pfInfo); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardListEntryWrite(HGCMCLIENTID idClient, SHAREDCLIPBOARDLISTHANDLE hList,
                                              PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);

    VBoxClipboardListEntryMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY);

    Msg.ReqParms.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.ReqParms.uHandle.SetUInt64(hList);
    Msg.ReqParms.fInfo.SetUInt32(pListEntry->fInfo);

    Msg.cbInfo.SetUInt32(pListEntry->cbInfo);
    Msg.pvInfo.SetPtr(pListEntry->pvInfo, pListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardEventGetNext(HGCMCLIENTID idClient, PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                            PVBGLR3CLIPBOARDEVENT *ppEvent)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(ppEvent,   VERR_INVALID_POINTER);

    PVBGLR3CLIPBOARDEVENT pEvent = (PVBGLR3CLIPBOARDEVENT)RTMemAllocZ(sizeof(VBGLR3CLIPBOARDEVENT));
    if (!pEvent)
        return VERR_NO_MEMORY;

    uint32_t uMsg   = 0;
    uint32_t cParms = 0;
    int rc = vbglR3ClipboardMsgPeekWait(idClient, &uMsg, &cParms, NULL /* pidRestoreCheck */);
    if (RT_SUCCESS(rc))
    {
        LogFunc(("Handling uMsg=%RU32\n", uMsg));

        switch (uMsg)
        {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN\n"));

                VBOXCLIPBOARDLISTOPENPARMS openParmsList;
                rc = SharedClipboardURIListOpenParmsInit(&openParmsList);
                if (RT_SUCCESS(rc))
                {
                    rc = VbglR3ClipboardListOpenRecv(idClient, &openParmsList);
                    if (RT_SUCCESS(rc))
                    {
                        SHAREDCLIPBOARDLISTHANDLE hList = SHAREDCLIPBOARDLISTHANDLE_INVALID;
                        rc = SharedClipboardURITransferListOpen(pTransfer, &openParmsList, &hList);

                        /* Reply in any case. */
                        int rc2 = VbglR3ClipboardListOpenReply(idClient, rc, hList);
                        AssertRC(rc2);
                    }

                    SharedClipboardURIListOpenParmsDestroy(&openParmsList);
                }
                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE\n"));

                SHAREDCLIPBOARDLISTHANDLE hList;
                rc = VbglR3ClipboardListCloseRecv(idClient, &hList);
                if (RT_SUCCESS(rc))
                {
                    rc = SharedClipboardURITransferListClose(pTransfer, hList);
                }

                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ\n"));

                /** @todo Handle filter + list features. */

                SHAREDCLIPBOARDLISTHANDLE hList  = SHAREDCLIPBOARDLISTHANDLE_INVALID;
                uint32_t                  fFlags = 0;
                rc = VbglR3ClipboardListHdrReadRecvReq(idClient, &hList, &fFlags);
                if (RT_SUCCESS(rc))
                {
                    VBOXCLIPBOARDLISTHDR hdrList;
                    rc = SharedClipboardURITransferListGetHeader(pTransfer, hList, &hdrList);
                    if (RT_SUCCESS(rc))
                    {
                        rc = VbglR3ClipboardListHdrWrite(idClient, hList, &hdrList);
                        SharedClipboardURIListHdrDestroy(&hdrList);
                    }
                }

                break;
            }

        #if 0
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE\n"));

                VBOXCLIPBOARDLISTHDR hdrList;
                rc = SharedClipboardURIListHdrInit(&hdrList);
                if (RT_SUCCESS(rc))
                {
                    rc = VBglR3ClipboardListHdrRecv(idClient, )
                }
                break;
            }
        #endif

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ\n"));

                VBOXCLIPBOARDLISTENTRY listEntry;
                rc = SharedClipboardURIListEntryInit(&listEntry);
                if (RT_SUCCESS(rc))
                {
                    SHAREDCLIPBOARDLISTHANDLE hList;
                    uint32_t                  fInfo;
                    rc = VbglR3ClipboardListEntryReadRecvReq(idClient, &hList, &fInfo);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardURITransferListRead(pTransfer, hList, &listEntry);
                        if (RT_SUCCESS(rc))
                        {
                            rc = VbglR3ClipboardListEntryWrite(idClient, hList, &listEntry);
                        }
                    }

                    SharedClipboardURIListEntryDestroy(&listEntry);
                }

                break;
            }

        #if 0
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_LIST_ENTRY_WRITE;
                break;
            }
        #endif

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_OPEN:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_OPEN\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_OBJ_OPEN;
                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_CLOSE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_CLOSE\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_OBJ_CLOSE;
                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_READ:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_READ\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_OBJ_READ;
                break;
            }

        #if 0
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_OBJ_WRITE;
                break;
            }
        #endif
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

            default:
                rc = VERR_NOT_SUPPORTED;
                break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (pEvent->enmType != VBGLR3CLIPBOARDEVENTTYPE_INVALID)
        {
            *ppEvent = pEvent;
        }
        else
            VbglR3ClipboardEventFree(pEvent);
    }
    else
    {
        /* Report error back to the host. */
        //VbglR3ClipboardWriteError(idClient, rc);

        VbglR3ClipboardEventFree(pEvent);
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
VBGLR3DECL(int) VbglR3ClipboardListHdrReadRecv(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTHANDLE phList)
{
    AssertPtrReturn(phList, VERR_INVALID_POINTER);

    VBoxClipboardListHdrReadMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ);

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
                                           VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxClipboardListHdrMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR);

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
                                                  VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);

    VBoxClipboardListEntryWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_WRITE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_WRITE);

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

VBGLR3DECL(int) VbglR3ClipboardObjOpen(HGCMCLIENTID idClient,
                                       const char *pszPath, PVBOXCLIPBOARDCREATEPARMS pCreateParms,
                                       PSHAREDCLIPBOARDOBJHANDLE phObj)
{
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pCreateParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phObj, VERR_INVALID_POINTER);

    VBoxClipboardObjOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_OPEN, VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_OPEN);

    int rc;

    char *pszPathTmp = RTStrDup(pszPath);
    if (pszPathTmp)
    {
        Msg.szPath.SetPtr((void *)pszPathTmp, (uint32_t)strlen(pszPathTmp) + 1 /* Include terminating zero */);
        Msg.parms.SetPtr(pCreateParms, sizeof(VBOXCLIPBOARDCREATEPARMS));

        rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
        if (RT_SUCCESS(rc))
        {
            *phObj = pCreateParms->uHandle;
        }

        RTStrFree(pszPathTmp);
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjClose(HGCMCLIENTID idClient, SHAREDCLIPBOARDOBJHANDLE hObj)
{
    VBoxClipboardObjCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_CLOSE, VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_CLOSE);

    Msg.uHandle.SetUInt64(hObj);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

VBGLR3DECL(int) VbglR3ClipboardObjRead(HGCMCLIENTID idClient, SHAREDCLIPBOARDOBJHANDLE hObj,
                                       void *pvData, uint32_t cbData, uint32_t *pcbRead)
{
    AssertPtrReturn(pvData,  VERR_INVALID_POINTER);
    AssertReturn(cbData,     VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */

    VBoxClipboardObjReadWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_READ, VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_READ);

    Msg.uContext.SetUInt32(0);
    Msg.uHandle.SetUInt64(hObj);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(0);
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        /** @todo Context ID not used yet. */
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

VBGLR3DECL(int) VbglR3ClipboardObjWrite(HGCMCLIENTID idClient,
                                        SHAREDCLIPBOARDOBJHANDLE hObj,
                                        void *pvData, uint32_t cbData, uint32_t *pcbWritten)
{
    AssertPtrReturn(pvData, VERR_INVALID_POINTER);
    AssertReturn(cbData, VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    VBoxClipboardObjReadWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_WRITE, VBOX_SHARED_CLIPBOARD_CPARMS_OBJ_WRITE);

    Msg.uContext.SetUInt32(0);
    Msg.uHandle.SetUInt64(hObj);
    Msg.pvData.SetPtr(pvData, cbData);
    Msg.cbData.SetUInt32(cbData);
    Msg.pvChecksum.SetPtr(NULL, 0); /** @todo Implement this. */
    Msg.cbChecksum.SetUInt32(0); /** @todo Implement this. */

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        if (pcbWritten)
            *pcbWritten = cbData;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

/**
 * Reports (advertises) guest clipboard formats to the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormats        The formats to advertise.
 */
VBGLR3DECL(int) VbglR3ClipboardReportFormats(HGCMCLIENTID idClient, uint32_t fFormats)
{
    VBoxClipboardReportFormatsMsg Msg;

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_REPORT_FORMATS, VBOX_SHARED_CLIPBOARD_CPARMS_REPORT_FORMATS);
    VbglHGCMParmUInt32Set(&Msg.formats, fFormats);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Sends guest clipboard data to the host.
 *
 * This is usually called in reply to a VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA message
 * from the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   fFormat         The format of the data.
 * @param   pv              The data.
 * @param   cb              The size of the data.
 */
static int vbglR3ClipboardWriteDataRaw(HGCMCLIENTID idClient, uint32_t fFormat, void *pv, uint32_t cb)
{
    VBoxClipboardWriteDataMsg Msg;
    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA, VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA);
    VbglHGCMParmUInt32Set(&Msg.format, fFormat);
    VbglHGCMParmPtrSet(&Msg.ptr, pv, cb);

    return VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
}

/**
 * Send guest clipboard data to the host.
 *
 * This is usually called in reply to a VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA message
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
    int rc  = vbglR3ClipboardWriteDataRaw(idClient, fFormat, pv, cb);

    return rc;
}

#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Writes an error to the host.
 *
 * @returns IPRT status code.
 * @param   idClient            The client id returned by VbglR3ClipboardConnect().
 * @param   rcErr               Error (IPRT-style) to send.
 */
VBGLR3DECL(int) VbglR3ClipboardWriteError(HGCMCLIENTID idClient, int rcErr)
{
    VBoxClipboardWriteErrorMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient, VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR, VBOX_SHARED_CLIPBOARD_CPARMS_ERROR);

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
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

