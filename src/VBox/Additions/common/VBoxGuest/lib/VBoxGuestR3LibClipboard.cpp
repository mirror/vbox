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
static int vbglR3ClipboardGetNextMsgType(HGCMCLIENTID idClient, uint32_t *puMsg, uint32_t *pcParms, bool fWait)
{
    AssertPtrReturn(puMsg,   VERR_INVALID_POINTER);
    AssertPtrReturn(pcParms, VERR_INVALID_POINTER);

    VBoxClipboardGetHostMsg Msg;
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

static int vbglR3ClipboardRecvListOpen(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    VBoxClipboardListOpenMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_OPEN);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.uHandle.SetUInt64(0);
    Msg.fList.SetUInt32(0);
    Msg.fFeatures.SetUInt32(0);
    Msg.cbFilter.SetUInt32(0);
    Msg.pvFilter.SetPtr(pListHdr->pszFilter, pListHdr->cbFilter);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.fList.GetUInt32(&pListHdr->fList);         AssertRC(rc);
        rc = Msg.fFeatures.GetUInt32(&pListHdr->fFeatures); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vbglR3ClipboardRecvListClose(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTHANDLE phList)
{
    AssertPtrReturn(phList, VERR_INVALID_POINTER);

    VBoxClipboardListCloseMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_CLOSE);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.uHandle.SetUInt64(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uHandle.GetUInt64(phList); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vbglR3ClipboardRecvListHdrRead(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTHANDLE phList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,   VERR_INVALID_POINTER);

    VBoxClipboardListHdrReadMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_READ, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_READ);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.uHandle.SetUInt64(0);
    Msg.cTotalObjects.SetUInt64(0);
    Msg.cbTotalSize.SetUInt64(0);
    Msg.enmCompression.SetUInt32(0);
    Msg.enmChecksumType.SetUInt32(0);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uHandle.GetUInt64(phList); AssertRC(rc);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int vbglR3ClipboardRecvListEntryRead(HGCMCLIENTID idClient, PVBOXCLIPBOARDLISTHANDLE phList,
                                            PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,     VERR_INVALID_POINTER);

    VBoxClipboardListEntryReadMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_ENTRY_READ);

    Msg.uContext.SetUInt32(0); /** @todo Context ID not used yet. */
    Msg.uHandle.SetUInt64(0);
    Msg.fInfo.SetUInt32(0);
    Msg.cbInfo.SetUInt32(0);
    Msg.pvInfo.SetPtr(pListEntry->pvInfo, pListEntry->cbInfo);

    int rc = VbglR3HGCMCall(&Msg.hdr, sizeof(Msg));
    if (RT_SUCCESS(rc))
    {
        rc = Msg.uHandle.GetUInt64(phList);             AssertRC(rc);
        rc = Msg.fInfo.GetUInt32(&pListEntry->fInfo);   AssertRC(rc);
        rc = Msg.cbInfo.GetUInt32(&pListEntry->cbInfo); AssertRC(rc);
    }

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
    int rc = vbglR3ClipboardGetNextMsgType(idClient, &uMsg, &cParms, true /* fWait */);
    if (RT_SUCCESS(rc))
    {
        /** @todo Check for VM session change. */
    }

#if 0
    typedef struct _
    {
        union
        {
            struct Dir
            {
                RTDIR hDir;
            };
        } u;
    };
#endif

    if (RT_SUCCESS(rc))
    {
        LogFunc(("Handling uMsg=%RU32\n", uMsg));

        switch (uMsg)
        {
#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_OPEN\n"));

                VBOXCLIPBOARDLISTHDR listHdr;
                rc = SharedClipboardURIListHdrInit(&listHdr);
                if (RT_SUCCESS(rc))
                {
                    rc = vbglR3ClipboardRecvListOpen(idClient, &listHdr);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardURITransferListOpen(pTransfer, &listHdr, NULL /* phList */);
                    }

                    SharedClipboardURIListHdrDestroy(&listHdr);
                }

                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_CLOSE\n"));

                VBOXCLIPBOARDLISTHANDLE hList;
                rc = vbglR3ClipboardRecvListClose(idClient, &hList);
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

                VBOXCLIPBOARDLISTHDR listHdr;
                rc = SharedClipboardURIListHdrInit(&listHdr);
                if (RT_SUCCESS(rc))
                {
                    VBOXCLIPBOARDLISTHANDLE hList;
                    rc = vbglR3ClipboardRecvListHdrRead(idClient, &hList, &listHdr);
                    if (RT_SUCCESS(rc))
                    {
                        if (SharedClipboardURITransferListHandleIsValid(pTransfer, hList))
                        {
                            rc = VbglR3ClipboardSendListHdrWrite(idClient, hList, &listHdr);
                        }
                        else
                            rc = VERR_INVALID_HANDLE;
                    }

                    SharedClipboardURIListHdrDestroy(&listHdr);
                }

                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_HDR_WRITE\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_LIST_HDR_WRITE;
                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_READ\n"));

                VBOXCLIPBOARDLISTENTRY listEntry;
                rc = SharedClipboardURIListEntryInit(&listEntry);
                if (RT_SUCCESS(rc))
                {
                    VBOXCLIPBOARDLISTHANDLE hList;
                    rc = vbglR3ClipboardRecvListEntryRead(idClient, &hList, &listEntry);
                    if (RT_SUCCESS(rc))
                    {
                        if (SharedClipboardURITransferListHandleIsValid(pTransfer, hList))
                        {
                            rc = VbglR3ClipboardSendListEntryWrite(idClient, hList, &listEntry);
                        }
                        else
                            rc = VERR_INVALID_HANDLE;
                    }

                    SharedClipboardURIListEntryDestroy(&listEntry);
                }
                break;
            }

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_LIST_ENTRY_WRITE\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_LIST_ENTRY_WRITE;
                break;
            }

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

            case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE:
            {
                LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_OBJ_WRITE\n"));
                pEvent->enmType = VBGLR3CLIPBOARDEVENTTYPE_URI_OBJ_WRITE;
                break;
            }
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
        VbglR3ClipboardWriteError(idClient, rc);

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

/**
 * Sends a list header to the host.
 *
 * @returns VBox status code.
 * @param   idClient        The client id returned by VbglR3ClipboardConnect().
 * @param   hList           List handle to send header for.
 * @param   pListHdr        List header to send.
 */
VBGLR3DECL(int) VbglR3ClipboardSendListHdrWrite(HGCMCLIENTID idClient,
                                                VBOXCLIPBOARDLISTHANDLE hList, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    VBoxClipboardListHdrWriteMsg Msg;
    RT_ZERO(Msg);

    VBGL_HGCM_HDR_INIT(&Msg.hdr, idClient,
                       VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_WRITE, VBOX_SHARED_CLIPBOARD_CPARMS_LIST_HDR_WRITE);

    Msg.uContext.SetUInt32(0); /** @todo Not used yet. */
    Msg.uHandle.SetUInt64(hList);
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
    Msg.pvChecksum.SetPtr(NULL, 0);
    Msg.cbChecksum.SetUInt32(0);

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

