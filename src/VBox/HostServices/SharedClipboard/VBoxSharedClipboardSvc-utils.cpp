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
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_CHUNK:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DIR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_FILE_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_FILE_DATA:
            fAllowed = fGuestToHost;
            break;

        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DATA_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DATA_CHUNK:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DIR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_FILE_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_FILE_DATA:
            fAllowed = fHostToGuest;
            break;

        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ERROR:
            fAllowed = fHostToGuest || fGuestToHost;
            break;

        default:
            break;
    }

    LogFlowFunc(("uMsg=%RU32, uMode=%RU32 -> fAllowed=%RTbool\n", uMsg, uMode, fAllowed));
    return fAllowed;
}

int vboxSvcClipboardURIReportMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t uMsg, uint32_t uFormats)
{
    AssertPtrReturn(pClientData, VERR_INVALID_POINTER);

    RT_NOREF(uFormats);

    if (!vboxSvcClipboardURIMsgIsAllowed(vboxSvcClipboardGetMode(), uMsg))
        return VERR_ACCESS_DENIED;

    int rc = VINF_SUCCESS;

    switch (uMsg)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_HDR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_HDR\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DATA_HDR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DATA_HDR\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_CHUNK:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DATA_CHUNK\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DATA_CHUNK:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DATA_CHUNK\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DIR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_DIR\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DIR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_DIR\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_FILE_HDR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_FILE_HDR\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_FILE_HDR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_FILE_HDR\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_FILE_DATA:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_READ_FILE_DATA\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_FILE_DATA:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_WRITE_FILE_DATA\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_CANCEL:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_CANCEL\n"));
            break;
        case VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ERROR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_URI_ERROR\n"));
            break;

        default:
            AssertMsgFailed(("Invalid message %RU32\n", uMsg));
            rc = VERR_INVALID_PARAMETER;
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static bool s_fReqHdr = false;

bool vboxSvcClipboardURIReturnMsg(PVBOXCLIPBOARDCLIENTDATA pClientData, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(cParms);

    bool fHandled = false;

    PSHAREDCLIPBOARDURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(&pClientData->URI, 0 /* Index */);
    if (pTransfer)
    {
        if (   !s_fReqHdr
            && pTransfer->State.Header.cObjects == 0) /** @todo For now we ASSUME that a header has been received when cObject > 0. */
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA\n"));

            HGCMSvcSetU32(&paParms[0], VBOX_SHARED_CLIPBOARD_HOST_MSG_READ_DATA);
            HGCMSvcSetU32(&paParms[1], VBOX_SHARED_CLIPBOARD_FMT_URI_LIST /* fFormats */);
            fHandled = true;

            s_fReqHdr = true;
        }
        else
        {
        }
    }

    LogFlowFunc(("fHandled=%RTbool\n", fHandled));
    return fHandled;
}
#endif /* VBOX_WITH_SHARED_CLIPBOARD_URI_LIST */

