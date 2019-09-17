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


#ifdef VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS
/**
 * Returns whether a HGCM message is allowed in a certain service mode or not.
 *
 * @returns \c true if message is allowed, \c false if not.
 * @param   uMode               Service mode to check allowance for.
 * @param   uMsg                HGCM message to check allowance for.
 */
bool vboxSvcClipboardURIMsgIsAllowed(uint32_t uMode, uint32_t uMsg)
{
    const bool fHostToGuest =    uMode == VBOX_SHCL_MODE_HOST_TO_GUEST
                              || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL;

    const bool fGuestToHost =    uMode == VBOX_SHCL_MODE_GUEST_TO_HOST
                              || uMode == VBOX_SHCL_MODE_BIDIRECTIONAL;

    bool fAllowed = false; /* If in doubt, don't allow. */

    switch (uMsg)
    {
        case VBOX_SHCL_GUEST_FN_LIST_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_CLOSE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_HDR_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_LIST_ENTRY_READ:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_OPEN:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_CLOSE:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_OBJ_READ:
            fAllowed = fHostToGuest;
            break;

        case VBOX_SHCL_GUEST_FN_MSG_PEEK_WAIT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_PEEK_NOWAIT:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_MSG_GET:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_STATUS:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHCL_GUEST_FN_ERROR:
            fAllowed = fHostToGuest || fGuestToHost;
            break;

        default:
            break;
    }

    fAllowed = true; /** @todo FIX !!!! */

    LogFlowFunc(("uMsg=%RU32, uMode=%RU32 -> fAllowed=%RTbool\n", uMsg, uMode, fAllowed));
    return fAllowed;
}

#if 0
int vboxSvcClipboardURIReportMsg(PSHCLCLIENT pClient, uint32_t uMsg, uint32_t uParm)
{
    AssertPtrReturn(pClient, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    switch (uMsg)
    {
        case VBOX_SHCL_HOST_MSG_URI_TRANSFER_START:
        {
            Assert(pClient->State.URI.fTransferStart == false);

            LogFlowFunc(("VBOX_SHCL_HOST_MSG_URI_TRANSFER_START\n"));

            if (   sharedClipboardSvcGetMode() != VBOX_SHCL_MODE_HOST_TO_GUEST
                && sharedClipboardSvcGetMode() != VBOX_SHCL_MODE_BIDIRECTIONAL)
            {
                LogFlowFunc(("Wrong clipboard mode, skipping\n"));
                break;
            }

            pClient->State.URI.fTransferStart = true;
            pClient->State.URI.enmTransferDir = (SHCLURITRANSFERDIR)uParm;
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

bool sharedClipboardSvcURIReturnMsg(PSHCLCLIENT pClient, uint32_t cParms, VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(pClient, cParms, paParms);

    bool fHandled = false;

    if (   pClient->State.URI.fTransferStart
        && cParms >= 2)
    {
        HGCMSvcSetU32(&paParms[0], VBOX_SHCL_HOST_MSG_URI_TRANSFER_START);
        HGCMSvcSetU32(&paParms[1], pClient->State.URI.enmTransferDir);
        pClient->State.URI.fTransferStart = false;

        fHandled = true;
    }

    LogFlowFunc(("fHandled=%RTbool\n", fHandled));
    return fHandled;
}
#endif
#endif /* VBOX_WITH_SHARED_CLIPBOARD_TRANSFERS */

