/* $Id$ */
/** @file
 * Shared Clipboard Service - Internal code for URI (list) handling.
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
#include <VBox/HostServices/VBoxClipboardSvc.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "VBoxSharedClipboardSvc-uri.h"


int vboxClipboardSvcURICreate(PVBOXCLIPBOARDCLIENTURIDATA pURIData, uint32_t uClientID)
{
    RT_NOREF(pURIData, uClientID);

    int rc = pURIData->Cache.OpenTemp(SHAREDCLIPBOARDCACHE_FLAGS_NONE);
    if (RT_SUCCESS(rc))
    {
    }

    return rc;
}

void vboxClipboardSvcURIDestroy(PVBOXCLIPBOARDCLIENTURIDATA pURIData)
{
    int rc = pURIData->Cache.Rollback();
    if (RT_SUCCESS(rc))
        rc = pURIData->Cache.Close();
}

int vboxClipboardSvcURIHandler(uint32_t u32ClientID,
                               void *pvClient,
                               uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[],
                               uint64_t tsArrival,
                               bool *pfAsync)
{
    RT_NOREF(u32ClientID, pvClient, paParms, tsArrival, pfAsync);

    PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;
    RT_NOREF(pClientData);

    /* Check if we've the right mode set. */
    int rc = VERR_ACCESS_DENIED; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_FN_READ_DATA_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_READ_DIR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_READ_FILE_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_READ_FILE_DATA:
        {
            if (   vboxSvcClipboardGetMode() == VBOX_SHARED_CLIPBOARD_MODE_GUEST_TO_HOST
                || vboxSvcClipboardGetMode() == VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
            {
                rc = VINF_SUCCESS;
            }

            if (RT_FAILURE(rc))
                LogFunc(("Guest -> Host Shared Clipboard mode disabled, ignoring request\n"));
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_HDR:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_WRITE_CANCEL:
            RT_FALL_THROUGH();
        case VBOX_SHARED_CLIPBOARD_FN_WRITE_ERROR:
        {
            if (   vboxSvcClipboardGetMode() == VBOX_SHARED_CLIPBOARD_MODE_HOST_TO_GUEST
                || vboxSvcClipboardGetMode() == VBOX_SHARED_CLIPBOARD_MODE_BIDIRECTIONAL)
            {
                rc = VINF_SUCCESS;
            }

            if (RT_FAILURE(rc))
                LogFunc(("Clipboard: Host -> Guest Shared Clipboard mode disabled, ignoring request\n"));
            break;
        }

        default:
            break;
    }

    if (RT_FAILURE(rc))
        return rc;

    rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_FN_READ_DATA_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_DATA_HDR\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_HDR\n"));
            if (cParms == 12)
            {
                VBOXCLIPBOARDCBWRITEDATAHDRDATA data;
                RT_ZERO(data);
                data.hdr.uMagic = CB_MAGIC_CLIPBOARD_WRITE_DATA_HDR;
                rc = HGCMSvcGetU32(&paParms[0], &data.hdr.uContextID);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[1], &data.data.uFlags);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.data.uScreenId);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU64(&paParms[3], &data.data.cbTotal);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[4], &data.data.cbMeta);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[5], &data.data.pvMetaFmt, &data.data.cbMetaFmt);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[6], &data.data.cbMetaFmt);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU64(&paParms[7], &data.data.cObjects);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[8], &data.data.enmCompression);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[9], (uint32_t *)&data.data.enmChecksumType);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[10], &data.data.pvChecksum, &data.data.cbChecksum);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[11], &data.data.cbChecksum);

                LogFlowFunc(("fFlags=0x%x, cbTotalSize=%RU64, cObj=%RU64\n",
                             data.data.uFlags, data.data.cbTotal, data.data.cObjects));
            }

            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK\n"));
            if (cParms == 5)
            {
                VBOXCLIPBOARDCBDATADATA data;
                RT_ZERO(data);
                data.hdr.uMagic = CB_MAGIC_CLIPBOARD_WRITE_DATA_CHUNK;
                rc = HGCMSvcGetU32(&paParms[0], &data.hdr.uContextID);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[1], (void**)&data.data.pvData, &data.data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[3], (void**)&data.data.pvChecksum, &data.data.cbChecksum);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[4], &data.data.cbChecksum);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_READ_DIR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_DIR\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DIR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DIR\n"));
            if (cParms == 4)
            {
                VBOXCLIPBOARDCBWRITEDIRDATA data;
                RT_ZERO(data);
                data.hdr.uMagic = CB_MAGIC_CLIPBOARD_WRITE_DIR;

                rc = HGCMSvcGetU32(&paParms[0], &data.hdr.uContextID);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[1], (void**)&data.pszPath, &data.cbPath);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.cbPath);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[3], &data.fMode);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_READ_FILE_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_FILE_HDR\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_HDR\n"));
            if (cParms == 6)
            {
                VBOXCLIPBOARDCBWRITEFILEHDRDATA data;
                RT_ZERO(data);
                data.hdr.uMagic = CB_MAGIC_CLIPBOARD_WRITE_FILE_HDR;

                rc = HGCMSvcGetU32(&paParms[0], &data.hdr.uContextID);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[1], (void**)&data.pszFilePath, &data.cbFilePath);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.cbFilePath);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[3], &data.fFlags);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[4], &data.fMode);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU64(&paParms[5], &data.cbSize);

                LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x, cbSize=%RU64\n",
                             data.pszFilePath, data.cbFilePath, data.fMode, data.cbSize));
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_READ_FILE_DATA:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_FILE_DATA\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA\n"));
            if (cParms == 5)
            {
                VBOXCLIPBOARDCBWRITEFILEDATADATA data;
                RT_ZERO(data);
                data.hdr.uMagic = CB_MAGIC_CLIPBOARD_WRITE_FILE_DATA;

                rc = HGCMSvcGetU32(&paParms[0], &data.hdr.uContextID);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[1], (void**)&data.pvData, &data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[3], (void**)&data.pvChecksum, &data.cbChecksum);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[4], &data.cbChecksum);

                LogFlowFunc(("pvData=0x%p, cbData=%RU32\n", data.pvData, data.cbData));
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_CANCEL:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_CANCEL\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_ERROR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_ERROR\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    return rc;
}

int vboxClipboardSvcURIHostHandler(uint32_t u32Function,
                                   uint32_t cParms,
                                   VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(cParms, paParms);

    int rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_FN_CANCEL:
            break;

        case VBOX_SHARED_CLIPBOARD_HOST_FN_ERROR:
            break;

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;

    }

    return rc;
}

