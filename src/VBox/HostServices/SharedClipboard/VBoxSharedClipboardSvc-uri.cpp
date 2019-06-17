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
#include <VBox/log.h>

#include <VBox/err.h>

#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/HostServices/VBoxClipboardExt.h>

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>

#include "VBoxSharedClipboardSvc-internal.h"
#include "VBoxSharedClipboardSvc-uri.h"


/*********************************************************************************************************************************
*   Externals                                                                                                                    *
*********************************************************************************************************************************/
extern PFNHGCMSVCEXT g_pfnExtension;
extern void *g_pvExtension;


/**
 * Destroys a VBOXCLIPBOARDDIRDATA structure.
 *
 * @param   pDirData            VBOXCLIPBOARDDIRDATA structure to destroy.
 */
void SharedClipboardURIDirDataDestroy(PVBOXCLIPBOARDDIRDATA pDirData)
{
    if (!pDirData)
        return;

    if (pDirData->pszPath)
    {
        Assert(pDirData->cbPath);
        RTStrFree(pDirData->pszPath);
        pDirData->pszPath = NULL;
    }
}

/**
 * Destroys a VBOXCLIPBOARDFILEHDR structure.
 *
 * @param   pFileHdr            VBOXCLIPBOARDFILEHDR structure to destroy.
 */
void SharedClipboardURIFileHdrDestroy(PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    if (!pFileHdr)
        return;

    if (pFileHdr->pszFilePath)
    {
        Assert(pFileHdr->pszFilePath);
        RTStrFree(pFileHdr->pszFilePath);
        pFileHdr->pszFilePath = NULL;
    }
}

/**
 * Destroys a VBOXCLIPBOARDFILEDATA structure.
 *
 * @param   pFileData           VBOXCLIPBOARDFILEDATA structure to destroy.
 */
void SharedClipboardURIFileDataDestroy(PVBOXCLIPBOARDFILEDATA pFileData)
{
    if (!pFileData)
        return;

    if (pFileData->pvData)
    {
        Assert(pFileData->cbData);
        RTMemFree(pFileData->pvData);
        pFileData->pvData = NULL;
    }
}

/**
 * Reads an URI data header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pDataHdr            Where to store the result.
 */
int VBoxSvcClipboardURIReadDataHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA_HDR)
        return VERR_INVALID_PARAMETER;

    RT_BZERO(pDataHdr, sizeof(VBOXCLIPBOARDDATAHDR));

    /* Note: Context ID (paParms[0]) not used yet. */
    int rc = HGCMSvcGetU32(&paParms[1], &pDataHdr->uFlags);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[2], &pDataHdr->uScreenId);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU64(&paParms[3], &pDataHdr->cbTotal);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[4], &pDataHdr->cbMeta);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetPv(&paParms[5], &pDataHdr->pvMetaFmt, &pDataHdr->cbMetaFmt);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[6], &pDataHdr->cbMetaFmt);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU64(&paParms[7], &pDataHdr->cObjects);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[8], &pDataHdr->enmCompression);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[9], (uint32_t *)&pDataHdr->enmChecksumType);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetPv(&paParms[10], &pDataHdr->pvChecksum, &pDataHdr->cbChecksum);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[11], &pDataHdr->cbChecksum);

    LogFlowFunc(("fFlags=0x%x, cbMeta=%RU32, cbTotalSize=%RU64, cObj=%RU64\n",
                 pDataHdr->uFlags, pDataHdr->cbMeta, pDataHdr->cbTotal, pDataHdr->cObjects));

    if (RT_SUCCESS(rc))
    {
        /** @todo Validate pvMetaFmt + cbMetaFmt. */
        /** @todo Validate header checksum. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads an URI data chunk from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pDataChunk          Where to store the result.
 */
int VBoxSvcClipboardURIReadDataChunk(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DATA_CHUNK)
        return VERR_INVALID_PARAMETER;

    RT_BZERO(pDataChunk, sizeof(VBOXCLIPBOARDDATACHUNK));

    /* Note: Context ID (paParms[0]) not used yet. */
    int rc = HGCMSvcGetPv(&paParms[1], (void**)&pDataChunk->pvData, &pDataChunk->cbData);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[2], &pDataChunk->cbData);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetPv(&paParms[3], (void**)&pDataChunk->pvChecksum, &pDataChunk->cbChecksum);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[4], &pDataChunk->cbChecksum);

    if (RT_SUCCESS(rc))
    {
        if (!SharedClipboardURIDataChunkIsValid(pDataChunk))
            rc = VERR_INVALID_PARAMETER;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads an URI directory entry from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pDirData            Where to store the result.
 */
int VBoxSvcClipboardURIReadDir(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDDIRDATA pDirData)
{
    if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_DIR)
        return VERR_INVALID_PARAMETER;

    RT_BZERO(pDirData, sizeof(VBOXCLIPBOARDDIRDATA));

    /* Note: Context ID (paParms[0]) not used yet. */
    int rc = HGCMSvcGetPv(&paParms[1], (void**)&pDirData->pszPath, &pDirData->cbPath);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[2], &pDirData->cbPath);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[3], &pDirData->fMode);

    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x\n", pDirData->pszPath, pDirData->cbPath, pDirData->fMode));

    if (RT_SUCCESS(rc))
    {
        if (!SharedClipboardURIDirDataIsValid(pDirData))
            rc = VERR_INVALID_PARAMETER;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads an URI file header from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pFileHdr            Where to store the result.
 */
int VBoxSvcClipboardURIReadFileHdr(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_FILE_HDR)
        return VERR_INVALID_PARAMETER;

    RT_BZERO(pFileHdr, sizeof(VBOXCLIPBOARDFILEHDR));

    /* Note: Context ID (paParms[0]) not used yet. */
    int rc = HGCMSvcGetPv(&paParms[1], (void**)&pFileHdr->pszFilePath, &pFileHdr->cbFilePath);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[2], &pFileHdr->cbFilePath);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[3], &pFileHdr->fFlags);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[4], &pFileHdr->fMode);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU64(&paParms[5], &pFileHdr->cbSize);

    LogFlowFunc(("pszPath=%s, cbPath=%RU32, fMode=0x%x, cbSize=%RU64\n",
                 pFileHdr->pszFilePath, pFileHdr->cbFilePath, pFileHdr->fMode, pFileHdr->cbSize));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads an URI file data chunk from HGCM service parameters.
 *
 * @returns VBox status code.
 * @param   cParms              Number of HGCM parameters supplied in \a paParms.
 * @param   paParms             Array of HGCM parameters.
 * @param   pFileData           Where to store the result.
 */
int VBoxSvcClipboardURIReadFileData(uint32_t cParms, VBOXHGCMSVCPARM paParms[], PVBOXCLIPBOARDFILEDATA pFileData)
{
    if (cParms != VBOX_SHARED_CLIPBOARD_CPARMS_WRITE_FILE_DATA)
        return VERR_INVALID_PARAMETER;

    RT_BZERO(pFileData, sizeof(VBOXCLIPBOARDFILEDATA));

    /* Note: Context ID (paParms[0]) not used yet. */
    int rc = HGCMSvcGetPv(&paParms[1], (void**)&pFileData->pvData, &pFileData->cbData);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[2], &pFileData->cbData);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetPv(&paParms[3], (void**)&pFileData->pvChecksum, &pFileData->cbChecksum);
    if (RT_SUCCESS(rc))
        rc = HGCMSvcGetU32(&paParms[4], &pFileData->cbChecksum);

    LogFlowFunc(("pvData=0x%p, cbData=%RU32\n", pFileData->pvData, pFileData->cbData));

    LogFlowFuncLeaveRC(rc);
    return rc;
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
int vboxSvcClipboardURIHandler(uint32_t u32ClientID,
                               void *pvClient,
                               uint32_t u32Function,
                               uint32_t cParms,
                               VBOXHGCMSVCPARM paParms[],
                               uint64_t tsArrival,
                               bool *pfAsync)
{
    RT_NOREF(u32ClientID, paParms, tsArrival, pfAsync);

    LogFlowFunc(("u32ClientID=%RU32, u32Function=%RU32, cParms=%RU32, g_pfnExtension=%p\n",
                 u32ClientID, u32Function, cParms, g_pfnExtension));

    const PVBOXCLIPBOARDCLIENTDATA pClientData = (PVBOXCLIPBOARDCLIENTDATA)pvClient;
    AssertPtrReturn(pClientData, VERR_INVALID_POINTER);

    /* Check if we've the right mode set. */
    int rc = VERR_ACCESS_DENIED; /* Play safe. */

    if (!vboxSvcClipboardURIMsgIsAllowed(vboxSvcClipboardGetMode(), u32Function))
        return rc;

    /* A (valid) service extension is needed because VBoxSVC needs to keep track of the
     * clipboard areas cached on the host. */
    if (!g_pfnExtension)
    {
#ifdef DEBUG_andy
        AssertPtr(g_pfnExtension);
#endif
        LogFunc(("Invalid / no service extension set, skipping URI handling\n"));
        rc = VERR_NOT_SUPPORTED;
    }

    if (RT_FAILURE(rc))
        return rc;

    const PSHAREDCLIPBOARDURITRANSFER pTransfer = SharedClipboardURICtxGetTransfer(&pClientData->URI, 0 /* Index */);

    bool fWrite = false;

    rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DATA_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_DATA_HDR\n"));
            if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_READ_DATA_HDR)
            {
                bool fDetach = false;

                if (RT_SUCCESS(rc))
                {
                    // pTransfer->Area. parms.uID

                    /** @todo Detach if header / meta size is 0. */
                }

                /* Do we need to detach again because we're done? */
                if (fDetach)
                    vboxSvcClipboardURIAreaDetach(&pClientData->State, pTransfer);
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR:
        {

            RT_BREAKPOINT();

            if (   u32Function == VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_HDR
                && SharedClipboardURICtxMaximumTransfersReached(&pClientData->URI))
            {
                rc = VERR_SHCLPB_MAX_TRANSFERS_REACHED;
                break;
            }

            fWrite = true;
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DATA_CHUNK:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DATA_CHUNK\n"));

            if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
            {
                rc = VERR_WRONG_ORDER;
                break;
            }

           fWrite = true;
#if 0
            if (RT_SUCCESS(rc))
            {
                /** @todo Validate checksum. */
                rc = SharedClipboardMetaDataAdd(&pTransfer->Meta, data.pvData, data.cbData);
                if (   RT_SUCCESS(rc)
                    && SharedClipboardMetaDataGetUsed(&pTransfer->Meta) == pTransfer->Header.cbMeta) /* Meta data transfer complete? */
                {
                    if (RT_SUCCESS(rc))
                    {

                    }

                    /* We're done processing the meta data, so just destroy it. */
                    SharedClipboardMetaDataDestroy(&pTransfer->Meta);
                }
            }
#endif
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_DIR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_DIR\n"));
            if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_READ_DIR)
            {
                if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
                {
                    rc = VERR_WRONG_ORDER;
                    break;
                }

                VBOXCLIPBOARDDIRDATA data;
                rc = VBoxClipboardSvcImplURIReadDir(pClientData, &data);
                if (RT_SUCCESS(rc))
                {
                    /* Note: Context ID (paParms[0]) not used yet. */
                    HGCMSvcSetPv (&paParms[1], data.pszPath, data.cbPath);
                    HGCMSvcSetU32(&paParms[2], data.cbPath);
                    HGCMSvcSetU32(&paParms[3], data.fMode);

                    SharedClipboardURIDirDataDestroy(&data);
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_DIR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_DIR\n"));

            if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
            {
                rc = VERR_WRONG_ORDER;
                break;
            }

#if 0
            if (RT_SUCCESS(rc))
            {
                SharedClipboardArea *pArea = SharedClipboardURITransferGetArea(pTransfer);
                AssertPtrBreakStmt(pArea, rc = VERR_INVALID_POINTER);

                const char *pszCacheDir = pArea->GetDirAbs();
                char *pszDir = RTPathJoinA(pszCacheDir, data.pszPath);
                if (pszDir)
                {
                    LogFlowFunc(("pszDir=%s\n", pszDir));

                    rc = RTDirCreateFullPath(pszDir, data.fMode);
                    if (RT_SUCCESS(rc))
                    {
                        /* Add for having a proper rollback. */
                        int rc2 = pArea->AddDir(pszDir);
                        AssertRC(rc2);
                    }

                    RTStrFree(pszDir);
                }
                else
                    rc = VERR_NO_MEMORY;
            }
#endif
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_FILE_HDR\n"));
            if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_READ_FILE_HDR)
            {
                if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
                {
                    rc = VERR_WRONG_ORDER;
                    break;
                }

                VBOXCLIPBOARDFILEHDR hdr;
                rc = VBoxClipboardSvcImplURIReadFileHdr(pClientData, &hdr);
                if (RT_SUCCESS(rc))
                {
                    /* Note: Context ID (paParms[0]) not used yet. */
                    HGCMSvcSetPv (&paParms[1], hdr.pszFilePath, hdr.cbFilePath);
                    HGCMSvcSetU32(&paParms[2], hdr.cbFilePath);
                    HGCMSvcSetU32(&paParms[3], hdr.fFlags);
                    HGCMSvcSetU32(&paParms[4], hdr.fMode);
                    HGCMSvcSetU64(&paParms[5], hdr.cbSize);

                    SharedClipboardURIFileHdrDestroy(&hdr);
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_HDR:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_HDR\n"));

                if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
                {
                    rc = VERR_WRONG_ORDER;
                    break;
                }

#if 0
                if (RT_SUCCESS(rc))
                {
                    if (!vboxSvcClipboardURIFileHdrIsValid(&data, &pTransfer->Header))
                    {
                        rc = VERR_INVALID_PARAMETER;
                    }
                    else
                    {
                        if (pTransfer->ObjCtx.pObj == NULL)
                        {
                            pTransfer->ObjCtx.pObj = new SharedClipboardURIObject(SharedClipboardURIObject::Type_File);
                            if (!pTransfer->ObjCtx.pObj) /** @todo Can this throw? */
                                rc = VERR_NO_MEMORY;
                        }
                        else /* There still is another object being processed? */
                           rc = VERR_WRONG_ORDER;
                    }
                }

                if (RT_SUCCESS(rc))
                {
                    SharedClipboardArea *pArea = SharedClipboardURITransferGetArea(pTransfer);
                    AssertPtrBreakStmt(pArea, rc = VERR_INVALID_POINTER);

                    const char *pszCacheDir = pArea->GetDirAbs();

                    char pszPathAbs[RTPATH_MAX];
                    rc = RTPathJoin(pszPathAbs, sizeof(pszPathAbs), pszCacheDir, data.pszFilePath);
                    if (RT_SUCCESS(rc))
                    {
                        rc = SharedClipboardPathSanitize(pszPathAbs, sizeof(pszPathAbs));
                        if (RT_SUCCESS(rc))
                        {
                            SharedClipboardURIObject *pObj = SharedClipboardURITransferGetObject(pTransfer, 0 /* Index */);
                            AssertPtrBreakStmt(pObj, VERR_INVALID_POINTER);

                            /** @todo Add sparse file support based on fFlags? (Use Open(..., fFlags | SPARSE). */
                            rc = pObj->OpenFileEx(pszPathAbs, SharedClipboardURIObject::View_Target,
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

                                    SharedClipboardURIObjCtxUninit(&pTransfer->ObjCtx);
                                }

                                /* Add for having a proper rollback. */
                                int rc2 = pArea->AddFile(pszPathAbs);
                                AssertRC(rc2);
                            }
                            else
                                LogRel(("Clipboard: Error opening/creating guest file '%s' on host, rc=%Rrc\n", pszPathAbs, rc));
                        }
                    }
            }
#endif
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_READ_FILE_DATA:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_READ_FILE_DATA\n"));
            if (cParms == VBOX_SHARED_CLIPBOARD_CPARMS_READ_FILE_DATA)
            {
                if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
                {
                    rc = VERR_WRONG_ORDER;
                    break;
                }

                VBOXCLIPBOARDFILEDATA data;
                rc = VBoxClipboardSvcImplURIReadFileData(pClientData, &data);
                if (RT_SUCCESS(rc))
                {
                    /* Note: Context ID (paParms[0]) not used yet. */
                    HGCMSvcSetPv (&paParms[1], data.pvData, data.cbData);
                    HGCMSvcSetU32(&paParms[2], data.cbData);
                    HGCMSvcSetPv (&paParms[3], data.pvChecksum, data.cbChecksum);
                    HGCMSvcSetU32(&paParms[4], data.cbChecksum);

                    SharedClipboardURIFileDataDestroy(&data);
                }
            }
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_FILE_DATA:
        {
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_FILE_DATA\n"));

            if (!SharedClipboardURICtxGetActiveTransfers(&pClientData->URI))
            {
                rc = VERR_WRONG_ORDER;
                break;
            }

#if 0
            if (RT_SUCCESS(rc))
            {
                if (!vboxSvcClipboardURIFileDataIsValid(&data, &pTransfer->Header))
                    rc = VERR_INVALID_PARAMETER;

                if (!SharedClipboardURIObjCtxIsValid(&pTransfer->ObjCtx))
                    rc = VERR_WRONG_ORDER;
            }

            if (RT_SUCCESS(rc))
            {
                SharedClipboardURIObject *pObj = SharedClipboardURIObjCtxGetObj(&pTransfer->ObjCtx);
                AssertPtrBreakStmt(pObj, VERR_INVALID_POINTER);

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
                        SharedClipboardURIObjCtxUninit(&pTransfer->ObjCtx);
                }
                else
                    LogRel(("Clipboard: Error writing guest file data for '%s', rc=%Rrc\n", pObj->GetDestPathAbs().c_str(), rc));
            }
#endif
            break;
        }

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_CANCEL:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_CANCEL\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;

        case VBOX_SHARED_CLIPBOARD_GUEST_FN_WRITE_ERROR:
            LogFlowFunc(("VBOX_SHARED_CLIPBOARD_FN_WRITE_ERROR\n"));
            AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);
            break;

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;
    }

    if (RT_SUCCESS(rc))
    {
        if (fWrite)
        {
            SHAREDCLIPBOARDPROVIDERWRITEPARMS writeParms;
            RT_ZERO(writeParms);

            writeParms.u.HostService.uMsg    = u32Function;
            writeParms.u.HostService.cParms  = cParms;
            writeParms.u.HostService.paParms = paParms;

            rc = pTransfer->pProvider->OnWrite(&writeParms);
        }
    }

    LogFlowFuncLeaveRC(rc);
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
int vboxSvcClipboardURIHostHandler(uint32_t u32Function,
                                   uint32_t cParms,
                                   VBOXHGCMSVCPARM paParms[])
{
    RT_NOREF(cParms, paParms);

    int rc = VERR_INVALID_PARAMETER; /* Play safe. */

    switch (u32Function)
    {
        case VBOX_SHARED_CLIPBOARD_HOST_FN_CANCEL:
            AssertFailed(); /** @todo Implement this. */
            break;

        case VBOX_SHARED_CLIPBOARD_HOST_FN_ERROR:
            AssertFailed(); /** @todo Implement this. */
            break;

        default:
            AssertMsgFailed(("Not implemented\n"));
            break;

    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Registers an URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to register a clipboard area for.
 */
int vboxSvcClipboardURIAreaRegister(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is registered for this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    VBOXCLIPBOARDEXTAREAPARMS parms;
    RT_ZERO(parms);

    parms.uID                  = NIL_SHAREDCLIPBOARDAREAID;
    parms.u.fn_register.pvData = SharedClipboardMetaDataMutableRaw(&pTransfer->State.Meta);
    parms.u.fn_register.cbData = (uint32_t)SharedClipboardMetaDataGetUsed(&pTransfer->State.Meta);

    /* As the meta data is now complete, register a new clipboard on the host side. */
    int rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_REGISTER, &parms, sizeof(parms));
    if (RT_SUCCESS(rc))
    {
        /* Note: Do *not* specify SHAREDCLIPBOARDAREA_OPEN_FLAGS_MUST_NOT_EXIST as flags here, as VBoxSVC took care of the
         *       clipboard area creation already. */
        rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */,
                                        SHAREDCLIPBOARDAREA_OPEN_FLAGS_NONE);
    }

    LogFlowFunc(("Registered new clipboard area (%RU32) by client %RU32 with rc=%Rrc\n",
                 parms.uID, pClientState->u32ClientID, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Unregisters an URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to unregister a clipboard area from.
 */
int vboxSvcClipboardURIAreaUnregister(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    VBOXCLIPBOARDEXTAREAPARMS parms;
    RT_ZERO(parms);

    parms.uID = pTransfer->pArea->GetID();

    int rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_UNREGISTER, &parms, sizeof(parms));
    if (RT_SUCCESS(rc))
    {
        rc = pTransfer->pArea->Close();
        if (RT_SUCCESS(rc))
        {
            delete pTransfer->pArea;
            pTransfer->pArea = NULL;
        }
    }

    LogFlowFunc(("Unregistered clipboard area (%RU32) by client %RU32 with rc=%Rrc\n",
                 parms.uID, pClientState->u32ClientID, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Attaches to an existing (registered) URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to attach a clipboard area to.
 */
int vboxSvcClipboardURIAreaAttach(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->pArea == NULL, ("An area already is attached to this transfer\n"),
                    VERR_WRONG_ORDER);

    pTransfer->pArea = new SharedClipboardArea();
    if (!pTransfer->pArea)
        return VERR_NO_MEMORY;

    VBOXCLIPBOARDEXTAREAPARMS parms;
    RT_ZERO(parms);

    parms.uID = 0; /* 0 means most recent clipboard area. */

    /* The client now needs to attach to the most recent clipboard area
     * to keep a reference to it. The host does the actual book keeping / cleanup then.
     *
     * This might fail if the host does not have a most recent clipboard area (yet). */
    int rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_ATTACH, &parms, sizeof(parms));
    if (RT_SUCCESS(rc))
        rc = pTransfer->pArea->OpenTemp(parms.uID /* Area ID */);

    LogFlowFunc(("Attached client %RU32 to clipboard area %RU32 with rc=%Rrc\n",
                 pClientState->u32ClientID, parms.uID, rc));

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Detaches from an URI clipboard area.
 *
 * @returns VBox status code.
 * @param   pClientState        Client state to use.
 * @param   pTransfer           URI transfer to detach a clipboard area from.
 */
int vboxSvcClipboardURIAreaDetach(PVBOXCLIPBOARDCLIENTSTATE pClientState, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    LogFlowFuncEnter();

    if (!pTransfer->pArea)
        return VINF_SUCCESS;

    const uint32_t uAreaID = pTransfer->pArea->GetID();

    int rc = VINF_SUCCESS;

    if (g_pfnExtension)
    {
        VBOXCLIPBOARDEXTAREAPARMS parms;
        RT_ZERO(parms);
        parms.uID = uAreaID;

        rc = g_pfnExtension(g_pvExtension, VBOX_CLIPBOARD_EXT_FN_AREA_DETACH, &parms, sizeof(parms));

        LogFlowFunc(("Detached client %RU32 from clipboard area %RU32 with rc=%Rrc\n",
                     pClientState->u32ClientID, uAreaID, rc));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

