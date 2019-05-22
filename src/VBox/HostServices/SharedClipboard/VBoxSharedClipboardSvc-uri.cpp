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

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "VBoxSharedClipboardSvc-uri.h"


/**
 * Initializes an URI object context.
 *
 * @returns VBox status code.
 * @param   pObjCtx             URI object context to initialize.
 */
int vboxClipboardSvcURIObjCtxInit(PVBOXCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);

    pObjCtx->pObj = NULL;

    return VINF_SUCCESS;
}

/**
 * Uninitializes an URI object context.
 *
 * @param   pObjCtx             URI object context to uninitialize.
 */
void vboxClipboardSvcURIObjCtxUninit(PVBOXCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturnVoid(pObjCtx);

    if (pObjCtx->pObj)
    {
        pObjCtx->pObj->Close();
        delete pObjCtx->pObj;
    }

    pObjCtx->pObj = NULL;
}

/**
 * Returns the URI object context's URI object.
 *
 * @returns Pointer to the URI object context's URI object.
 * @param   pObjCtx             URI object context to return the URI object for.
 */
SharedClipboardURIObject *vboxClipboardSvcURIObjCtxGetObj(PVBOXCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, NULL);
    return pObjCtx->pObj;
}

/**
 * Returns if an URI object context is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pObjCtx             URI object context to check.
 */
bool vboxClipboardSvcURIObjCtxIsValid(PVBOXCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    return (   pObjCtx
            && pObjCtx->pObj
            && pObjCtx->pObj->IsComplete() == false
            && pObjCtx->pObj->IsOpen());
}

/**
 * Creates a URI transfer object.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer object to destroy.
 * @param   uClientID           Client ID to assign to the transfer object.
 */
int vboxClipboardSvcURITransferCreate(PVBOXCLIPBOARDCLIENTURITRANSFER pTransfer, uint32_t uClientID)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    RT_NOREF(uClientID);

    int rc = pTransfer->Cache.OpenTemp(SHAREDCLIPBOARDCACHE_FLAGS_NONE);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardMetaDataInit(&pTransfer->Meta);
        if (RT_SUCCESS(rc))
            vboxClipboardSvcURIObjCtxInit(&pTransfer->ObjCtx);
    }

    return rc;
}

/**
 * Destroys a URI transfer object.
 *
 * @param   pTransfer           Transfer object to destroy.
 */
void vboxClipboardSvcURITransferDestroy(PVBOXCLIPBOARDCLIENTURITRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    vboxClipboardSvcURITransferReset(pTransfer);

    int rc2 = pTransfer->Cache.Close();
    AssertRC(rc2);
}

/**
 * Resets a URI transfer object.
 *
 * @param   pTransfer           Transfer object to reset.
 */
void vboxClipboardSvcURITransferReset(PVBOXCLIPBOARDCLIENTURITRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    vboxClipboardSvcURIObjCtxUninit(&pTransfer->ObjCtx);

    int rc2 = pTransfer->Cache.Rollback();
    AssertRC(rc2);

    SharedClipboardMetaDataDestroy(&pTransfer->Meta);
}

/**
 * URI client (guest) handler for the Shared Clipboard host service.
 *
 * @returns VBox status code.
 * @param   u32ClientID         Client ID of the client which called this handler.
 * @param   pvClient            Pointer to client-specific data block.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters  supplied.
 * @param   tsArrival           Timestamp of arrival.
 * @param   pfAsync             Returns \c true if the response needs to be being asynchronous or \c false if not,
 */
int vboxClipboardSvcURIHandler(uint32_t u32ClientID,
                               void *pvClient,
                               uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[],
                               uint64_t tsArrival,
                               bool *pfAsync)
{
    RT_NOREF(u32ClientID, paParms, tsArrival, pfAsync);

    const PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;

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

    const PVBOXCLIPBOARDCLIENTURITRANSFER pTransfer = &pClientData->Transfer;

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
            if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA_HDR)
            {
                if (pClientData->cTransfers == 0) /* At the moment we only support on transfer per client at a time. */
                {
                    RT_ZERO(pTransfer->Hdr);
                    /* Note: Context ID (paParms[0]) not used yet. */
                    rc = HGCMSvcGetU32(&paParms[1], &pTransfer->Hdr.uFlags);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU32(&paParms[2], &pTransfer->Hdr.uScreenId);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU64(&paParms[3], &pTransfer->Hdr.cbTotal);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU32(&paParms[4], &pTransfer->Hdr.cbMeta);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetPv(&paParms[5], &pTransfer->Hdr.pvMetaFmt, &pTransfer->Hdr.cbMetaFmt);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU32(&paParms[6], &pTransfer->Hdr.cbMetaFmt);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU64(&paParms[7], &pTransfer->Hdr.cObjects);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU32(&paParms[8], &pTransfer->Hdr.enmCompression);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU32(&paParms[9], (uint32_t *)&pTransfer->Hdr.enmChecksumType);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetPv(&paParms[10], &pTransfer->Hdr.pvChecksum, &pTransfer->Hdr.cbChecksum);
                    if (RT_SUCCESS(rc))
                        rc = HGCMSvcGetU32(&paParms[11], &pTransfer->Hdr.cbChecksum);

                    LogFlowFunc(("fFlags=0x%x, cbTotalSize=%RU64, cObj=%RU64\n",
                                 pTransfer->Hdr.uFlags, pTransfer->Hdr.cbTotal, pTransfer->Hdr.cObjects));

                    if (RT_SUCCESS(rc))
                    {
                        /** @todo Validate pvMetaFmt + cbMetaFmt. */
                        /** @todo Validate checksum. */
                        rc = SharedClipboardMetaDataResize(&pTransfer->Meta, pTransfer->Hdr.cbMeta);
                        if (RT_SUCCESS(rc))
                            pClientData->cTransfers++;
                    }
                }
                else
                    LogFunc(("Another transfer in progress, cTransfers=%RU32\n", pClientData->cTransfers));
            }

            break;
        }

        case VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK\n"));
            if (   cParms == VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA_CHUNK
                && pClientData->cTransfers) /* Some transfer in-flight? */
            {
                VBOXCLIPBOARDDATACHUNK data;
                RT_ZERO(data);
                /* Note: Context ID (paParms[0]) not used yet. */
                rc = HGCMSvcGetPv(&paParms[1], (void**)&data.pvData, &data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[3], (void**)&data.pvChecksum, &data.cbChecksum);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[4], &data.cbChecksum);

                if (RT_SUCCESS(rc))
                {
                    if (!VBoxSvcClipboardDataChunkIsValid(&data))
                        rc = VERR_INVALID_PARAMETER;
                }

                if (RT_SUCCESS(rc))
                {
                    /** @todo Validate checksum. */
                    rc = SharedClipboardMetaDataAdd(&pTransfer->Meta, data.pvData, data.cbData);
                    if (   RT_SUCCESS(rc)
                        && SharedClipboardMetaDataGetUsed(&pTransfer->Meta) == pTransfer->Hdr.cbMeta) /* Meta data transfer complete? */
                    {
                        rc = pTransfer->List.SetFromURIData(SharedClipboardMetaDataRaw(&pTransfer->Meta),
                                                            SharedClipboardMetaDataGetSize(&pTransfer->Meta),
                                                            SHAREDCLIPBOARDURILIST_FLAGS_NONE);

                        /* We're done processing the meta data, so just destroy it. */
                        SharedClipboardMetaDataDestroy(&pTransfer->Meta);

                        /*
                         * As we now have valid meta data, other parties can now acquire those and
                         * request additional data as well.
                         *
                         * In case of file data, receiving and requesting now depends on the speed of
                         * the actual source and target, i.e. the source might not be finished yet in receiving
                         * the current object, while the target already requests (and maybe waits) for the data to arrive.
                         */
                    }
                }
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
            if (   cParms == VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DIR
                && pClientData->cTransfers) /* Some transfer in-flight? */
            {
                VBOXCLIPBOARDDIRDATA data;
                RT_ZERO(data);
                /* Note: Context ID (paParms[0]) not used yet. */
                rc = HGCMSvcGetPv(&paParms[1], (void**)&data.pszPath, &data.cbPath);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.cbPath);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[3], &data.fMode);

                LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x\n", data.pszPath, data.cbPath, data.fMode));

                if (RT_SUCCESS(rc))
                {
                    if (!VBoxSvcClipboardDirDataIsValid(&data))
                        rc = VERR_INVALID_PARAMETER;
                }

                if (RT_SUCCESS(rc))
                {
                    const char *pszCacheDir = pTransfer->Cache.GetDirAbs();
                    char *pszDir = RTPathJoinA(pszCacheDir, data.pszPath);
                    if (pszDir)
                    {
                        rc = RTDirCreateFullPath(pszDir, data.fMode);
                        if (RT_SUCCESS(rc))
                        {
                            /* Add for having a proper rollback. */
                            int rc2 = pTransfer->Cache.AddDir(pszDir);
                            AssertRC(rc2);
                        }
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
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
            if (   cParms == VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_FILE_HDR
                && pClientData->cTransfers) /* Some transfer in-flight? */
            {
                VBOXCLIPBOARDFILEHDR data;
                RT_ZERO(data);
                /* Note: Context ID (paParms[0]) not used yet. */
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

                if (RT_SUCCESS(rc))
                {
                    if (!VBoxSvcClipboardFileHdrIsValid(&data, &pTransfer->Hdr))
                        rc = VERR_INVALID_PARAMETER;

                    if (pTransfer->ObjCtx.pObj == NULL)
                    {
                        pTransfer->ObjCtx.pObj = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File);
                        if (!pTransfer->ObjCtx.pObj) /** @todo Can this throw? */
                            rc = VERR_NO_MEMORY;
                    }
                    else /* There still is another object being processed? */
                       rc = VERR_WRONG_ORDER;
                }

                if (RT_SUCCESS(rc))
                {
                    const char *pszCacheDir = pTransfer->Cache.GetDirAbs();

                    char pszPathAbs[RTPATH_MAX];
                    rc = RTPathJoin(pszPathAbs, sizeof(pszPathAbs), pszCacheDir, data.pszFilePath);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardPathSanitize(pszPathAbs, sizeof(pszPathAbs));
                        if (RT_SUCCESS(rc))
                        {
                            SharedClipboardURIObject *pObj = vboxClipboardSvcURIObjCtxGetObj(&pTransfer->ObjCtx);

                            /** @todo Add sparse file support based on fFlags? (Use Open(..., fFlags | SPARSE). */
                            rc = pObj->OpenEx(pszPathAbs, SharedClipboardURIObject::View_Target,
                                              RTFILE_O_CREATE_REPLACE | RTFILE_O_WRITE | RTFILE_O_DENY_WRITE,
                                              (data.fMode & RTFS_UNIX_MASK) | RTFS_UNIX_IRUSR | RTFS_UNIX_IWUSR);
                            if (RT_SUCCESS(rc))
                            {
                                rc = pObj->SetSize(data.cbSize);

                                /** @todo Unescape path before printing. */
                                LogRel2(("Clipboard: Transferring guest file '%s' to host (%RU64 bytes, mode 0x%x)\n",
                                         pObj->GetDestPathAbs().c_str(), pObj->GetSize(), pObj->GetMode()));

                                if (pObj->IsComplete()) /* 0-byte file? We're done already. */
                                {
                                    /** @todo Sanitize path. */
                                    LogRel2(("Clipboard: Transferring guest file '%s' (0 bytes) to host complete\n",
                                             pObj->GetDestPathAbs().c_str()));

                                    vboxClipboardSvcURIObjCtxUninit(&pTransfer->ObjCtx);
                                }

                                /* Add for having a proper rollback. */
                                int rc2 = pTransfer->Cache.AddFile(pszPathAbs);
                                AssertRC(rc2);
                            }
                            else
                                LogRel(("Clipboard: Error opening/creating guest file '%s' on host, rc=%Rrc\n", pszPathAbs, rc));
                        }
                    }
                }
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
            if (   cParms == VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_FILE_DATA
                && pClientData->cTransfers) /* Some transfer in-flight? */
            {
                VBOXCLIPBOARDFILEDATA data;
                RT_ZERO(data);
                /* Note: Context ID (paParms[0]) not used yet. */
                rc = HGCMSvcGetPv(&paParms[1], (void**)&data.pvData, &data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[2], &data.cbData);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetPv(&paParms[3], (void**)&data.pvChecksum, &data.cbChecksum);
                if (RT_SUCCESS(rc))
                    rc = HGCMSvcGetU32(&paParms[4], &data.cbChecksum);

                LogFlowFunc(("pvData=0x%p, cbData=%RU32\n", data.pvData, data.cbData));

                if (RT_SUCCESS(rc))
                {
                    if (!VBoxSvcClipboardFileDataIsValid(&data, &pTransfer->Hdr))
                        rc = VERR_INVALID_PARAMETER;

                    if (!vboxClipboardSvcURIObjCtxIsValid(&pTransfer->ObjCtx))
                        rc = VERR_WRONG_ORDER;
                }

                if (RT_SUCCESS(rc))
                {
                    SharedClipboardURIObject *pObj = vboxClipboardSvcURIObjCtxGetObj(&pTransfer->ObjCtx);

                    uint32_t cbWritten;
                    rc = pObj->Write(data.pvData, data.cbData, &cbWritten);
                    if (RT_SUCCESS(rc))
                    {
                        Assert(cbWritten <= data.cbData);
                        if (cbWritten < data.cbData)
                        {
                            /** @todo What to do when the host's disk is full? */
                            rc = VERR_DISK_FULL;
                        }

                        if (   pObj->IsComplete()
                            || RT_FAILURE(rc))
                            vboxClipboardSvcURIObjCtxUninit(&pTransfer->ObjCtx);
                    }
                    else
                        LogRel(("Clipboard: Error writing guest file data for '%s', rc=%Rrc\n", pObj->GetDestPathAbs().c_str(), rc));
                }
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

#ifdef DEBUG_andy
    AssertRC(rc);
#endif
    return rc;
}

/**
 * URI host handler for the Shared Clipboard host service.
 *
 * @returns VBox status code.
 * @param   u32Function         Function number being called.
 * @param   cParms              Number of function parameters supplied.
 * @param   paParms             Array function parameters  supplied.
 */
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

