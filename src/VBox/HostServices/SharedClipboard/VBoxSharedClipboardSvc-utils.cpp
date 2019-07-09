/* $Id$ */
/** @file
 * Shared Clipboard Service - Host service utility functions.
 */

/*
 * Copyright (C) 2019 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/log.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/errcore.h>
#include <iprt/path.h>

#include "VBoxSharedClipboardSvc-internal.h"


#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
/**
 * Returns whether a HGCM message is allowed in a certain service mode or not.
 *
 * @returns \c true if message is allowed, \c false if not.
 * @param   uMode               Service mode to check allowance for.
 * @param   uMsg                HGCM message to check allowance for.
 */
bool vboxSvcClipboardURIMsgIsAllowed(uint32_t uMode, uint32_t uMsg)
{
    const bool fHostToGuest =    uMode == VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                              || uMode == VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL;

    const bool fGuestToHost =    uMode == VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                              || uMode == VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL;

    bool fAllowed = false; /* If in doubt, don't allow. */

    switch (uMsg)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_CLOSE:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_HDR_READ:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_LIST_ENTRY_READ:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_CLOSE:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_OBJ_READ:
            fAllowed = fHostToGuest;
            break;

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_WAIT:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_PEEK_NOWAIT:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_MSG_GET:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_STATUS:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_ERROR:
            fAllowed = fHostToGuest || fGuestToHost;
            break;

        default:
            break;
    }

    fAllowed = true; /** @todo FIX !!!! */

    LogFlowFunc(("uMsg=%RU32, uMode=%RU32 -> fAllowed=%RTbool\n", uMsg, uMode, fAllowed));
    return fAllowed;
}

int vboxSvcClipboardURIReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t uMsg, uint32_t uParm)
{
    AssertPtrReturn(pClientData, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    switch (uMsg)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START\n"));

            if (   vboxSvcClipboardGetMode() != VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                && vboxSvcClipboardGetMode() != VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
            {
                LogFlowFunc(("Wrong clipboard mode, skipping\n"));
                break;
            }

            pClientData->State.URI.fTransferStart = true;
            pClientData->State.URI.enmTransferDir = (SHAREDCLIPBOARDURITRANSFERDIR)uParm;
            break;

        }

        default:
            AssertMsgFailed(("Invalid message %RU32\n", uMsg));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

bool vboxSvcClipboardURIReturnMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pClientData, cParms, paParms);

    bool fHandled = false;

    if (   pClientData->State.URI.fTransferStart
        && cParms >= 2)
    {
        HGCMSvcSetU32(&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_TRANSFER_START);
        HGCMSvcSetU32(&paParms[1], pClientData->State.URI.enmTransferDir);
        pClientData->State.URI.fTransferStart = false;

        fHandled = true;
    }

#if 1
    int rc = 0;
#else
    /* Sanity. */
    Assert(pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ);

    int rc;

    /* Note: Message priority / order is taken into account here. */
    if (pTransfer->State.pHeader)
    {
        LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA_HDR\n"));
        rc = VBoxSvcClipboardURIWriteListHdr(cParms, paParms, pTransfer->State.pHeader);
        if (RT_SUCCESS(rc))
        {
            /* We're done witht the data header, destroy it. */
            SharedClipboardURIListHdrFree(pTransfer->State.pHeader);
            pTransfer->State.pHeader = NULL;

            fHandled = true;
        }
    }
    else if (pTransfer->State.pMeta)
    {
        LogFlowFunc(("VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA_CHUNK\n"));

        uint32_t cbBuf = _64K;
        uint8_t  pvBuf[_64K]; /** @todo Improve */

        uint32_t cbRead;
        rc = SharedClipboardMetaDataRead(pTransfer->State.pMeta, pvBuf, cbBuf, &cbRead);
        if (RT_SUCCESS(rc))
        {
            Assert(cbRead <= cbBuf);

            VBOXCLIPBOARDListEntry ListEntry;
            RT_ZERO(ListEntry);
            ListEntry.pvData = pvBuf;
            ListEntry.cbData = cbRead;

            rc = VBoxSvcClipboardURIWriteListEntry(cParms, paParms, &ListEntry);
        }

        /* Has all meta data been read? */
        if (RT_SUCCESS(rc))
        {
            if (SharedClipboardMetaDataGetUsed(pTransfer->State.pMeta) == 0)
            {
                SharedClipboardMetaDataFree(pTransfer->State.pMeta);
                pTransfer->State.pMeta = NULL;
            }

            fHandled = true;
        }
    }
    else if (pTransfer->pURIList)
    {
        PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx = SharedClipboardURITransferGetCurrentObjCtx(pTransfer);
        if (!SharedClipboardURIObjCtxIsValid(pObjCtx))
        {
            if (!pTransfer->pURIList->IsEmpty())
                pObjCtx->pObj = pTransfer->pURIList->First();
        }

        if (   pObjCtx
            && pObjCtx->pObj)
        {
            switch (pObjCtx->pObj->GetType())
            {
                case SharedClipboardURIObject::Type_Directory:
                {
                    rc = VBoxSvcClipboardURIWriteDir(cParms, paParms, &ListEntry);
                    break;
                }
            }
        }

        if (0)
        {
            delete pTransfer->pURIList;
            pTransfer->pURIList = NULL;
        }

        fHandled = true;
    }
    else
        rc = VERR_WRONG_ORDER;
#endif

    LogFlowFunc(("rc=%Rrc, fHandled=%RTbool\n", rc, fHandled));
    return fHandled;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

