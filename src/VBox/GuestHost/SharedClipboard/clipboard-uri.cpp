/* $Id$ */
/** @file
 * Shared Clipboard: Common URI transfer handling code.
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

#define LOG_GROUP LOG_GROUP_SHARED_CLIPBOARD
#include <VBox/log.h>

#include <iprt/path.h>

#include <VBox/err.h>
#include <VBox/GuestHost/SharedClipboard-uri.h>


#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static int sharedClipboardURITransferThreadCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer);
static int sharedClipboardURITransferThreadDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs);
static int sharedClipboardURITransferReadThread(RTTHREAD hThread, void *pvUser);
static int sharedClipboardURITransferWriteThread(RTTHREAD hThread, void *pvUser);
static PSHAREDCLIPBOARDURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx);

static void sharedClipboardURITransferMetaDataDestroyInternal(PSHAREDCLIPBOARDURITRANSFER pTransfer);
#endif


int SharedClipboardURIDataHdrAlloc(PVBOXCLIPBOARDDATAHDR *ppDataHdr)
{
    int rc;

    PVBOXCLIPBOARDDATAHDR pDataHdr = (PVBOXCLIPBOARDDATAHDR)RTMemAllocZ(sizeof(VBOXCLIPBOARDDATAHDR));
    if (pDataHdr)
    {
        PSHAREDCLIPBOARDMETADATAFMTDATA pMetaDataFmt
            = (PSHAREDCLIPBOARDMETADATAFMTDATA)RTMemAllocZ(sizeof(SHAREDCLIPBOARDMETADATAFMTDATA));
        if (pMetaDataFmt)
        {
            char *pszFmt = NULL;
            rc = RTStrAAppend(&pszFmt, "VBoxShClURIList");
            if (RT_SUCCESS(rc))
            {
                pMetaDataFmt->uVer  = 1;
                pMetaDataFmt->pvFmt = pszFmt;
                pMetaDataFmt->cbFmt = (uint32_t)strlen(pszFmt) + 1 /* Include terminating zero */;

                pDataHdr->pvMetaFmt = pMetaDataFmt;
                pDataHdr->cbMetaFmt = sizeof(SHAREDCLIPBOARDMETADATAFMTDATA) + pMetaDataFmt->cbFmt;

                *ppDataHdr = pDataHdr;
            }
        }
        else
            rc = VERR_NO_MEMORY;

        if (RT_FAILURE(rc))
        {
            RTMemFree(pDataHdr);
            pDataHdr = NULL;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Frees a VBOXCLIPBOARDDATAHDR structure.
 *
 * @param   pDataChunk          VBOXCLIPBOARDDATAHDR structure to free.
 */
void SharedClipboardURIDataHdrFree(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (!pDataHdr)
        return;

    LogFlowFuncEnter();

    SharedClipboardURIDataHdrDestroy(pDataHdr);

    RTMemFree(pDataHdr);
    pDataHdr = NULL;
}

/**
 * Duplicates (allocates) a VBOXCLIPBOARDDATAHDR structure.
 *
 * @returns Duplicated VBOXCLIPBOARDDATAHDR structure on success.
 * @param   pDataHdr            VBOXCLIPBOARDDATAHDR to duplicate.
 */
PVBOXCLIPBOARDDATAHDR SharedClipboardURIDataHdrDup(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, NULL);

    PVBOXCLIPBOARDDATAHDR pDataHdrDup = (PVBOXCLIPBOARDDATAHDR)RTMemAlloc(sizeof(VBOXCLIPBOARDDATAHDR));
    if (pDataHdrDup)
    {
        *pDataHdrDup = *pDataHdr;

        if (pDataHdr->pvMetaFmt)
        {
            pDataHdrDup->pvMetaFmt = RTMemDup(pDataHdr->pvMetaFmt, pDataHdr->cbMetaFmt);
            pDataHdrDup->cbMetaFmt = pDataHdr->cbMetaFmt;
        }

        if (pDataHdr->pvChecksum)
        {
            pDataHdrDup->pvChecksum = RTMemDup(pDataHdr->pvChecksum, pDataHdr->cbChecksum);
            pDataHdrDup->cbChecksum = pDataHdr->cbChecksum;
        }
    }

    return pDataHdrDup;
}

/**
 * Returns the size (in bytes) of the announced meta data.
 *
 * @returns Announced meta data size in bytes.
 * @param   pDataHdr            Data header struct to get announced meta data size for.
 */
uint32_t SharedClipboardURIDataHdrGetMetaDataSize(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, 0);

    return pDataHdr->cbMeta;
}

/**
 * Initializes an URI data header struct.
 *
 * @returns VBox status code.
 * @param   pDataHdr            Data header struct to initialize.
 */
int SharedClipboardURIDataHdrInit(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturn(pDataHdr, VERR_INVALID_POINTER);

    SharedClipboardURIDataHdrReset(pDataHdr);

    return VINF_SUCCESS;
}

/**
 * Destroys an URI data header struct.
 *
 * @param   pDataHdr            Data header struct to destroy.
 */
void SharedClipboardURIDataHdrDestroy(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (!pDataHdr)
        return;

    LogFlowFuncEnter();

    if (pDataHdr->pvMetaFmt)
    {
        Assert(pDataHdr->cbMetaFmt);

        RTMemFree(pDataHdr->pvMetaFmt);
        pDataHdr->pvMetaFmt = NULL;
        pDataHdr->cbMetaFmt = 0;
    }

    if (pDataHdr->pvChecksum)
    {
        Assert(pDataHdr->cbChecksum);

        RTMemFree(pDataHdr->pvChecksum);
        pDataHdr->pvChecksum = NULL;
        pDataHdr->cbChecksum = 0;
    }
}

/**
 * Resets a VBOXCLIPBOARDDATAHDR structture.
 *
 * @returns VBox status code.
 * @param   pDataHdr            VBOXCLIPBOARDDATAHDR structture to reset.
 */
void SharedClipboardURIDataHdrReset(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertPtrReturnVoid(pDataHdr);

    LogFlowFuncEnter();

    RT_BZERO(pDataHdr, sizeof(VBOXCLIPBOARDDATAHDR));
}

/**
 * Returns whether a given clipboard data header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDataHdr            Clipboard data header to validate.
 */
bool SharedClipboardURIDataHdrIsValid(PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pDataHdr);
    return true; /** @todo Implement this. */
}

/**
 * Creates (allocates) and initializes a VBOXCLIPBOARDDATACHUNK structure.
 *
 * @param   ppDirData           Where to return the created VBOXCLIPBOARDDATACHUNK structure on success.
 */
int SharedClipboardURIDataChunkAlloc(PVBOXCLIPBOARDDATACHUNK *ppDataChunk)
{
    PVBOXCLIPBOARDDATACHUNK pDataChunk = (PVBOXCLIPBOARDDATACHUNK)RTMemAlloc(sizeof(VBOXCLIPBOARDDATACHUNK));
    if (!pDataChunk)
        return VERR_NO_MEMORY;

    int rc = SharedClipboardURIDataChunkInit(pDataChunk);
    if (RT_SUCCESS(rc))
        *ppDataChunk = pDataChunk;

    return rc;
}

/**
 * Frees a VBOXCLIPBOARDDATACHUNK structure.
 *
 * @param   pDataChunk         VBOXCLIPBOARDDATACHUNK structure to free.
 */
void SharedClipboardURIDataChunkFree(PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    if (!pDataChunk)
        return;

    SharedClipboardURIDataChunkDestroy(pDataChunk);
    RTMemFree(pDataChunk);
}

/**
 * Initializes a VBOXCLIPBOARDDATACHUNK structure.
 *
 * @param   pDataChunk          VBOXCLIPBOARDDATACHUNK structure to initialize.
 */
int SharedClipboardURIDataChunkInit(PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    RT_BZERO(pDataChunk, sizeof(VBOXCLIPBOARDDATACHUNK));

    return VINF_SUCCESS;
}

/**
 * Initializes a VBOXCLIPBOARDDATACHUNK structure.
 *
 * @param   pDataChunk          VBOXCLIPBOARDDATACHUNK structure to destroy.
 */
void SharedClipboardURIDataChunkDestroy(PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    if (pDataChunk->pvData)
    {
        RTMemFree(pDataChunk->pvData);
        pDataChunk->pvData = NULL;
        pDataChunk->cbData = 0;
    }

    if (pDataChunk->pvChecksum)
    {
        RTMemFree(pDataChunk->pvChecksum);
        pDataChunk->pvChecksum = NULL;
        pDataChunk->cbChecksum = 0;
    }
}

/**
 * Returns whether a given clipboard data chunk is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDataChunk          Clipboard data chunk to validate.
 */
bool SharedClipboardURIDataChunkIsValid(PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    RT_NOREF(pDataChunk);

    /** @todo Verify checksum. */

    return true; /** @todo Implement this. */
}

/**
 * Creates (allocates) and initializes a VBOXCLIPBOARDDIRDATA structure.
 *
 * @param   ppDirData           Where to return the created VBOXCLIPBOARDDIRDATA structure on success.
 */
int SharedClipboardURIDirDataAlloc(PVBOXCLIPBOARDDIRDATA *ppDirData)
{
    PVBOXCLIPBOARDDIRDATA pDirData = (PVBOXCLIPBOARDDIRDATA)RTMemAlloc(sizeof(VBOXCLIPBOARDDIRDATA));
    if (!pDirData)
        return VERR_NO_MEMORY;

    int rc = SharedClipboardURIDirDataInit(pDirData);
    if (RT_SUCCESS(rc))
        *ppDirData = pDirData;

    return rc;
}

/**
 * Frees a VBOXCLIPBOARDDIRDATA structure.
 *
 * @param   pDirData           Where to return the created VBOXCLIPBOARDDIRDATA structure on success.
 */
void SharedClipboardURIDirDataFree(PVBOXCLIPBOARDDIRDATA pDirData)
{
    if (!pDirData)
        return;

    SharedClipboardURIDirDataDestroy(pDirData);
    RTMemFree(pDirData);
}

/**
 * Initializes a VBOXCLIPBOARDDIRDATA structure.
 *
 * @param   pDirData            VBOXCLIPBOARDDIRDATA structure to initialize.
 */
int SharedClipboardURIDirDataInit(PVBOXCLIPBOARDDIRDATA pDirData)
{
    RT_BZERO(pDirData, sizeof(VBOXCLIPBOARDDIRDATA));

    return VINF_SUCCESS;
}

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
 * Duplicates (allocates) a VBOXCLIPBOARDDIRDATA structure.
 *
 * @returns Duplicated VBOXCLIPBOARDDIRDATA structure on success.
 * @param   pDirData            VBOXCLIPBOARDDIRDATA to duplicate.
 */
PVBOXCLIPBOARDDIRDATA SharedClipboardURIDirDataDup(PVBOXCLIPBOARDDIRDATA pDirData)
{
    AssertPtrReturn(pDirData, NULL);

    PVBOXCLIPBOARDDIRDATA pDirDataDup = (PVBOXCLIPBOARDDIRDATA)RTMemAllocZ(sizeof(VBOXCLIPBOARDDIRDATA));
    if (pDirDataDup)
    {
        *pDirDataDup = *pDirData;

        if (pDirData->pszPath)
        {
            pDirDataDup->pszPath = RTStrDup(pDirData->pszPath);
            if (pDirDataDup->pszPath)
                pDirDataDup->cbPath = pDirData->cbPath;
        }
    }

    return pDirDataDup;
}

/**
 * Returns whether given clipboard directory data is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pDirData            Clipboard directory data to validate.
 */
bool SharedClipboardURIDirDataIsValid(PVBOXCLIPBOARDDIRDATA pDirData)
{
    if (   !pDirData->cbPath
        || pDirData->cbPath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pDirData->pszPath))
        return false;

    return true;
}

/**
 * Initializes a VBOXCLIPBOARDFILEHDR structure.
 *
 * @param   pDirData            VBOXCLIPBOARDFILEHDR structure to initialize.
 */
int SharedClipboardURIFileHdrInit(PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    RT_BZERO(pFileHdr, sizeof(VBOXCLIPBOARDFILEHDR));

    return VINF_SUCCESS;
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
 * Frees a VBOXCLIPBOARDFILEHDR structure.
 *
 * @param   pFileHdr            VBOXCLIPBOARDFILEHDR structure to free.
 */
void SharedClipboardURIFileHdrFree(PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    if (!pFileHdr)
        return;

    SharedClipboardURIFileHdrDestroy(pFileHdr);

    RTMemFree(pFileHdr);
    pFileHdr = NULL;
}

/**
 * Duplicates (allocates) a VBOXCLIPBOARDFILEHDR structure.
 *
 * @returns Duplicated VBOXCLIPBOARDFILEHDR structure on success.
 * @param   pFileHdr            VBOXCLIPBOARDFILEHDR to duplicate.
 */
PVBOXCLIPBOARDFILEHDR SharedClipboardURIFileHdrDup(PVBOXCLIPBOARDFILEHDR pFileHdr)
{
    AssertPtrReturn(pFileHdr, NULL);

    PVBOXCLIPBOARDFILEHDR pFileHdrDup = (PVBOXCLIPBOARDFILEHDR)RTMemAllocZ(sizeof(VBOXCLIPBOARDFILEHDR));
    if (pFileHdrDup)
    {
        *pFileHdrDup = *pFileHdr;

        if (pFileHdr->pszFilePath)
        {
            pFileHdrDup->pszFilePath = RTStrDup(pFileHdr->pszFilePath);
            if (pFileHdrDup->pszFilePath)
                pFileHdrDup->cbFilePath = pFileHdrDup->cbFilePath;
        }
    }

    return pFileHdrDup;
}

/**
 * Returns whether a given clipboard file header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pFileHdr            Clipboard file header to validate.
 * @param   pDataHdr            Data header to use for validation.
 */
bool SharedClipboardURIFileHdrIsValid(PVBOXCLIPBOARDFILEHDR pFileHdr, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    if (   !pFileHdr->cbFilePath
        || pFileHdr->cbFilePath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pFileHdr->pszFilePath))
        return false;

    if (pFileHdr->cbSize > pDataHdr->cbTotal)
        return false;

    return true;
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
 * Duplicates (allocates) a VBOXCLIPBOARDFILEDATA structure.
 *
 * @returns Duplicated VBOXCLIPBOARDFILEDATA structure on success.
 * @param   pFileData           VBOXCLIPBOARDFILEDATA to duplicate.
 */
PVBOXCLIPBOARDFILEDATA SharedClipboardURIFileDataDup(PVBOXCLIPBOARDFILEDATA pFileData)
{
    AssertPtrReturn(pFileData, NULL);

    PVBOXCLIPBOARDFILEDATA pFileDataDup = (PVBOXCLIPBOARDFILEDATA)RTMemAllocZ(sizeof(VBOXCLIPBOARDFILEDATA));
    if (pFileDataDup)
    {
        *pFileDataDup = *pFileData;

        if (pFileData->pvData)
        {
            pFileDataDup->pvData = RTMemDup(pFileData->pvData, pFileData->cbData);
            if (pFileDataDup->pvData)
                pFileDataDup->cbData = pFileDataDup->cbData;
        }

        if (pFileData->pvChecksum)
        {
            pFileDataDup->pvChecksum = RTMemDup(pFileData->pvChecksum, pFileData->cbChecksum);
            if (pFileDataDup->pvChecksum)
                pFileDataDup->cbChecksum = pFileData->cbChecksum;
        }
    }

    return pFileDataDup;
}

/**
 * Returns whether given clipboard file data is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pFileData           Clipboard file data to validate.
 * @param   pDataHdr            Data header to use for validation.
 */
bool SharedClipboardURIFileDataIsValid(PVBOXCLIPBOARDFILEDATA pFileData, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    RT_NOREF(pFileData, pDataHdr);
    return true;
}

/**
 * Initializes an URI object context.
 *
 * @returns VBox status code.
 * @param   pObjCtx             URI object context to initialize.
 */
int SharedClipboardURIObjCtxInit(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    pObjCtx->pObj = NULL;

    return VINF_SUCCESS;
}

/**
 * Destroys an URI object context.
 *
 * @param   pObjCtx             URI object context to destroy.
 */
void SharedClipboardURIObjCtxDestroy(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturnVoid(pObjCtx);

    LogFlowFuncEnter();

    if (pObjCtx->pObj)
    {
        pObjCtx->pObj->Close();
        /* Note: Do *not* delete pObj here -- the associated URI list will do this. */
    }

    pObjCtx->pObj = NULL;
}

/**
 * Returns the URI object context's URI object.
 *
 * @returns Pointer to the URI object context's URI object.
 * @param   pObjCtx             URI object context to return the URI object for.
 */
SharedClipboardURIObject *SharedClipboardURIObjCtxGetObj(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
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
bool SharedClipboardURIObjCtxIsValid(PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    return (   pObjCtx
            && pObjCtx->pObj
            && pObjCtx->pObj->IsComplete() == false
            && pObjCtx->pObj->IsOpen());
}

/**
 * Initializes an URI clipboard transfer struct.
 *
 * @returns VBox status code.
 * @param   enmDir              Specifies the transfer direction of this transfer.
 * @param   enmSource           Specifies the data source of the transfer.
 * @param   ppTransfer          Where to return the created URI transfer struct.
 *                              Must be destroyed by SharedClipboardURITransferDestroy().
 */
int SharedClipboardURITransferCreate(SHAREDCLIPBOARDURITRANSFERDIR enmDir, SHAREDCLIPBOARDSOURCE enmSource,
                                     PSHAREDCLIPBOARDURITRANSFER *ppTransfer)
{
    AssertPtrReturn(ppTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PSHAREDCLIPBOARDURITRANSFER pTransfer = (PSHAREDCLIPBOARDURITRANSFER)RTMemAlloc(sizeof(SHAREDCLIPBOARDURITRANSFER));
    if (!pTransfer)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_NONE;
    pTransfer->State.enmDir    = enmDir;
    pTransfer->State.enmSource = enmSource;

    LogFlowFunc(("enmDir=%RU32, enmSource=%RU32\n", pTransfer->State.enmDir, pTransfer->State.enmSource));

    pTransfer->State.pHeader = NULL;
    pTransfer->State.pMeta   = NULL;
    pTransfer->pArea         = NULL; /* Will be created later if needed. */

    pTransfer->Thread.hThread    = NIL_RTTHREAD;
    pTransfer->Thread.fCancelled = false;
    pTransfer->Thread.fStarted   = false;

    pTransfer->pvUser = NULL;
    pTransfer->cbUser = 0;

    RT_ZERO(pTransfer->Callbacks);

    pTransfer->pURIList = new SharedClipboardURIList();
    if (!pTransfer->pURIList)
    {
        RTMemFree(pTransfer);
        return VERR_NO_MEMORY;
    }

    rc = SharedClipboardURIObjCtxInit(&pTransfer->State.ObjCtx);
    if (RT_SUCCESS(rc))
    {
        *ppTransfer = pTransfer;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys an URI clipboard transfer context struct.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard transfer to destroy.
 */
int SharedClipboardURITransferDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    if (!pTransfer)
        return VINF_SUCCESS;

    LogFlowFuncEnter();

    int rc = sharedClipboardURITransferThreadDestroy(pTransfer, 30 * 1000 /* Timeout in ms */);
    if (RT_FAILURE(rc))
        return rc;

    SharedClipboardURIDataHdrDestroy(pTransfer->State.pHeader);
    SharedClipboardMetaDataDestroy(pTransfer->State.pMeta);

    if (pTransfer->pURIList)
    {
        delete pTransfer->pURIList;
        pTransfer->pURIList = NULL;
    }

    SharedClipboardURIObjCtxDestroy(&pTransfer->State.ObjCtx);

    RTMemFree(pTransfer);
    pTransfer = NULL;

    LogFlowFuncLeave();

    return VINF_SUCCESS;
}

/**
 * Prepares everything needed for a read / write transfer to begin.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to prepare.
 */
int SharedClipboardURITransferPrepare(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    LogFlowFunc(("pTransfer=%p, enmDir=%RU32\n", pTransfer, pTransfer->State.enmDir));

    if (pTransfer->Callbacks.pfnTransferPrepare)
    {
        SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
        pTransfer->Callbacks.pfnTransferPrepare(&callbackData);
    }

    if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ)
    {
        if (pTransfer->ProviderIface.pfnPrepare)
            rc = pTransfer->ProviderIface.pfnPrepare(&pTransfer->ProviderCtx);
    #if 0
        rc = pTransfer->pURIList->SetFromURIData(SharedClipboardMetaDataRaw(pMeta),
                                                 SharedClipboardMetaDataGetUsed(pMeta),
                                                 SHAREDCLIPBOARDURILIST_FLAGS_NONE);
        /** @todo Verify pvMetaFmt. */
    #endif

        sharedClipboardURITransferMetaDataDestroyInternal(pTransfer);
    }
    else if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_WRITE)
    {
        if (   pTransfer->State.pMeta
            && pTransfer->pURIList)
        {
            const PSHAREDCLIPBOARDMETADATA pMeta = pTransfer->State.pMeta;

            rc = pTransfer->pURIList->AppendURIPathsFromList((char *)SharedClipboardMetaDataRaw(pMeta),
                                                             SharedClipboardMetaDataGetUsed(pMeta),
                                                             SHAREDCLIPBOARDURILIST_FLAGS_KEEP_OPEN);
            if (RT_SUCCESS(rc))
            {
                PVBOXCLIPBOARDDATAHDR pHeader;
                rc = SharedClipboardURIDataHdrAlloc(&pHeader);
                if (RT_SUCCESS(rc))
                {
                    /* The total size also contains the size of the meta data. */
                    uint64_t cbTotal  = pMeta->cbUsed;
                             cbTotal += pTransfer->pURIList->GetTotalBytes();

                    pHeader->cbTotal  = cbTotal;
                    pHeader->cbMeta   = (uint32_t)SharedClipboardMetaDataGetUsed(pMeta);
                    pHeader->cObjects = pTransfer->pURIList->GetTotalCount();

                    SharedClipboardURIDataHdrDestroy(pTransfer->State.pHeader);

                    if (RT_SUCCESS(rc))
                    {
                        LogFlowFunc(("Writing cbTotal=%RU64, cbMeta=%RU32, cObj=%RU64\n",
                                     pHeader->cbTotal, pHeader->cbMeta, pHeader->cObjects));

                        pTransfer->State.pHeader = pHeader;
                    }
                    else
                        SharedClipboardURIDataHdrFree(pHeader);
                }
            }
        }
        else
            rc = VERR_WRONG_ORDER;
    }
    else
    {
        rc = VERR_NOT_IMPLEMENTED;
        AssertFailed();
    }

    if (RT_SUCCESS(rc))
    {
        /** @todo Add checksum support. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates an URI provider for a given transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to create URI provider for.
 * @param   pCreationCtx        Provider creation context to use for provider creation.
 */
int SharedClipboardURITransferProviderCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                             PSHAREDCLIPBOARDPROVIDERCREATIONCTX pCreationCtx)
{
    AssertPtrReturn(pTransfer,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCreationCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    pTransfer->ProviderIface         = pCreationCtx->Interface;

    pTransfer->ProviderCtx.pTransfer = pTransfer;
    pTransfer->ProviderCtx.pvUser    = pCreationCtx->pvUser;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Resets an clipboard URI transfer.
 *
 * @param   pTransfer           URI clipboard transfer to reset.
 */
void SharedClipboardURITransferReset(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    LogFlowFuncEnter();

    /** @todo Anything else to do here? */

    if (pTransfer->pURIList)
        pTransfer->pURIList->Clear();

    SharedClipboardURIObjCtxDestroy(&pTransfer->State.ObjCtx);
}

/**
 * Returns the clipboard area for a clipboard URI transfer.
 *
 * @returns Current clipboard area, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return clipboard area for.
 */
SharedClipboardArea *SharedClipboardURITransferGetArea(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    return pTransfer->pArea;
}

/**
 * Returns the current object context of a clipboard URI transfer.
 *
 * @returns Current object context, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return object context for.
 */
PSHAREDCLIPBOARDCLIENTURIOBJCTX SharedClipboardURITransferGetCurrentObjCtx(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    /* At the moment we only have one object context per transfer at a time. */
    return &pTransfer->State.ObjCtx;
}

/**
 * Returns the current URI object for a clipboard URI transfer.
 *
 * @returns Current URI object, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return current URI object for.
 */
const SharedClipboardURIObject *SharedClipboardURITransferGetCurrentObject(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    if (pTransfer->pURIList)
        return pTransfer->pURIList->First();

    return NULL;
}

/**
 * Returns the URI list for a clipboard URI transfer.
 *
 * @returns Pointer to URI list.
 * @param   pTransfer           URI clipboard transfer to return URI list for.
 */
SharedClipboardURIList *SharedClipboardURITransferGetList(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    return pTransfer->pURIList;
}

/**
 * Returns the current URI object for a clipboard URI transfer.
 *
 * @returns Pointer to URI object.
 * @param   pTransfer           URI clipboard transfer to return URI object for.
 */
SharedClipboardURIObject *SharedClipboardURITransferGetObject(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint64_t uIdx)
{
    AssertPtrReturn(pTransfer, NULL);

    if (!pTransfer->pURIList)
        return NULL;

    return pTransfer->pURIList->At(uIdx);
}

SHAREDCLIPBOARDSOURCE SharedClipboardURITransferGetSource(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHAREDCLIPBOARDSOURCE_INVALID);

    return pTransfer->State.enmSource;
}

/**
 * Returns the current transfer status.
 *
 * @returns Current transfer status.
 * @param   pTransfer           URI clipboard transfer to return status for.
 */
SHAREDCLIPBOARDURITRANSFERSTATUS SharedClipboardURITransferGetStatus(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHAREDCLIPBOARDURITRANSFERSTATUS_NONE);

    return pTransfer->State.enmStatus;
}

/**
 * Runs (starts) an URI transfer, either in synchronous or asynchronous (threaded) mode.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to run.
 * @param   fAsync              Whether to run the transfer synchronously or asynchronously.
 */
int SharedClipboardURITransferRun(PSHAREDCLIPBOARDURITRANSFER pTransfer, bool fAsync)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("fAsync=%RTbool\n", fAsync));

    if (fAsync)
    {
        rc = sharedClipboardURITransferThreadCreate(pTransfer);
    }
    else
    {
        if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ)
            rc = SharedClipboardURITransferRead(pTransfer);
        else if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_WRITE)
            rc = SharedClipboardURITransferWrite(pTransfer);
        else
            rc = VERR_NOT_IMPLEMENTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets or unsets the callback table to be used for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to set callbacks for.
 * @param   pCallbacks          Pointer to callback table to set.
 */
void SharedClipboardURITransferSetCallbacks(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                            PSHAREDCLIPBOARDURITRANSFERCALLBACKS pCallbacks)
{
    AssertPtrReturnVoid(pTransfer);
    AssertPtrReturnVoid(pCallbacks);

    LogFlowFunc(("pCallbacks=%p\n", pCallbacks));

#define SET_CALLBACK(a_pfnCallback)             \
    if (pCallbacks->a_pfnCallback)              \
        pTransfer->Callbacks.a_pfnCallback = pCallbacks->a_pfnCallback

    SET_CALLBACK(pfnTransferPrepare);
    SET_CALLBACK(pfnTransferStarted);
    SET_CALLBACK(pfnDataHeaderComplete);
    SET_CALLBACK(pfnDataComplete);
    SET_CALLBACK(pfnTransferCanceled);
    SET_CALLBACK(pfnTransferError);
    SET_CALLBACK(pfnTransferStarted);

#undef SET_CALLBACK

    pTransfer->Callbacks.pvUser = pCallbacks->pvUser;
}

int SharedClipboardURITransferSetDataHeader(PSHAREDCLIPBOARDURITRANSFER pTransfer, PVBOXCLIPBOARDDATAHDR pDataHdr)
{
    AssertReturn(pTransfer->State.pHeader == NULL, VERR_WRONG_ORDER);

    LogFlowFuncEnter();

    int rc;

    pTransfer->State.pHeader = SharedClipboardURIDataHdrDup(pDataHdr);
    if (pTransfer->State.pHeader)
    {
        LogFlowFunc(("Meta data size is %RU32, total size is %RU64, %RU64 objects\n",
                     pTransfer->State.pHeader->cbMeta, pTransfer->State.pHeader->cbTotal,
                     pTransfer->State.pHeader->cObjects));

        rc = VINF_SUCCESS;

        if (pTransfer->Callbacks.pfnDataHeaderComplete)
        {
            SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
            pTransfer->Callbacks.pfnDataHeaderComplete(&callbackData);
        }
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferSetDataChunk(PSHAREDCLIPBOARDURITRANSFER pTransfer, PVBOXCLIPBOARDDATACHUNK pDataChunk)
{
    LogFlowFuncEnter();

    AssertPtrReturn(pTransfer->State.pHeader, VERR_WRONG_ORDER);

    int rc = SharedClipboardURITransferMetaDataAdd(pTransfer, pDataChunk->pvData, pDataChunk->cbData);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a thread for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to create thread for.
 */
static int sharedClipboardURITransferThreadCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    PFNRTTHREAD pfnRTThread = NULL;

    if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_READ)
        pfnRTThread = sharedClipboardURITransferReadThread;
    else if (pTransfer->State.enmDir == SHAREDCLIPBOARDURITRANSFERDIR_WRITE)
        pfnRTThread = sharedClipboardURITransferWriteThread;

    AssertPtrReturn(pfnRTThread, VERR_NOT_SUPPORTED);

    /* Spawn a worker thread, so that we don't block the window thread for too long. */
    int rc = RTThreadCreate(&pTransfer->Thread.hThread, pfnRTThread,
                            pTransfer /* pvUser */, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "shclp");
    if (RT_SUCCESS(rc))
    {
        int rc2 = RTThreadUserWait(pTransfer->Thread.hThread, 30 * 1000 /* Timeout in ms */);
        AssertRC(rc2);

        if (!pTransfer->Thread.fStarted) /* Did the thread fail to start? */
            rc = VERR_GENERAL_FAILURE; /** @todo Find a better rc. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys a thread of a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to destroy thread for.
 * @param   uTimeoutMs          Timeout (in ms) to wait for thread creation.
 */
static int sharedClipboardURITransferThreadDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    if (pTransfer->Thread.hThread == NIL_RTTHREAD)
        return VINF_SUCCESS;

    int rcThread = VERR_WRONG_ORDER;
    int rc = RTThreadWait(pTransfer->Thread.hThread, uTimeoutMs, &rcThread);

    LogFlowFunc(("Waiting for thread resulted in %Rrc (thread exited with %Rrc)\n", rc, rcThread));

    return rc;
}

/**
 * Reads all URI objects using the connected provider.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to read objects for.
 */
int SharedClipboardURITransferRead(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->State.enmStatus == SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING,
                    ("Transfer already is running\n"), VERR_WRONG_ORDER);

    pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING;

    if (pTransfer->Callbacks.pfnTransferStarted)
    {
        SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
        pTransfer->Callbacks.pfnTransferStarted(&callbackData);
    }

    int rc = SharedClipboardURITransferMetaDataRead(pTransfer, NULL /* pcbRead */);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardURITransferReadObjects(pTransfer);
        if (RT_SUCCESS(rc))
        {
            if (pTransfer->Callbacks.pfnTransferComplete)
            {
                SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
                pTransfer->Callbacks.pfnTransferComplete(&callbackData, rc);
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pTransfer->Callbacks.pfnTransferError)
        {
            SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
            pTransfer->Callbacks.pfnTransferError(&callbackData, rc);
        }
    }

    pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_NONE;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferReadDir(PSHAREDCLIPBOARDURITRANSFER pTransfer, PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    RT_NOREF(pObjCtx);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDDIRDATA pDirData;
    int rc = pTransfer->ProviderIface.pfnReadDirectory(&pTransfer->ProviderCtx, &pDirData);
    if (RT_SUCCESS(rc))
    {
        SharedClipboardURIDirDataFree(pDirData);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferReadFile(PSHAREDCLIPBOARDURITRANSFER pTransfer, PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    RT_NOREF(pObjCtx);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDFILEHDR pFileHdr;
    int rc = pTransfer->ProviderIface.pfnReadFileHdr(&pTransfer->ProviderCtx, &pFileHdr);
    if (RT_SUCCESS(rc))
    {
        uint64_t cbToRead = pFileHdr->cbSize;
        uint32_t cbRead;

        uint8_t  pvBuf[_64K]; /** @todo Improve. */
        uint32_t cbBuf = _64K;

        while (cbToRead)
        {
            rc = pTransfer->ProviderIface.pfnReadFileData(&pTransfer->ProviderCtx,
                                                          pvBuf, cbBuf, 0 /* fFlags */, &cbRead);
            if (RT_FAILURE(rc))
                break;

            /** @todo Validate data block checksum. */

            Assert(cbToRead >= cbRead);
            cbToRead -= cbRead;
        }

        SharedClipboardURIFileHdrFree(pFileHdr);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferReadObject(PSHAREDCLIPBOARDURITRANSFER pTransfer, PSHAREDCLIPBOARDCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    RT_NOREF(pObjCtx);

    LogFlowFuncEnter();

    PVBOXCLIPBOARDOBJHDR pObjHdr;
    int rc = pTransfer->ProviderIface.pfnReadObjHdr(&pTransfer->ProviderCtx, &pObjHdr);
    if (RT_SUCCESS(rc))
    {
        switch (pObjHdr->enmType)
        {
#if 0
            case Dir:
            {
                SHAREDCLIPBOARDCLIENTURIOBJCTX objCtx;
                rc = SharedClipboardURITransferReadDir(pTransfer, &objCtx);
                break;
            }

            case File:
            {
                SHAREDCLIPBOARDCLIENTURIOBJCTX objCtx;
                rc = SharedClipboardURITransferReadFile(pTransfer, &objCtx);
                break;
            }
#endif
            default:
                /** @todo Skip -- how? */
                break;
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads all URI objects using the connected provider.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to read objects for.
 */
int SharedClipboardURITransferReadObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    const PVBOXCLIPBOARDDATAHDR pHdr = pTransfer->State.pHeader;
    AssertPtrReturn(pHdr, VERR_WRONG_ORDER);

    int rc = VINF_SUCCESS;

    for (uint64_t i = 0; i < pHdr->cObjects; i++)
    {
        SHAREDCLIPBOARDCLIENTURIOBJCTX objCtx;
        rc = SharedClipboardURITransferReadObject(pTransfer, &objCtx);
        if (RT_FAILURE(rc))
            break;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Thread for transferring (reading) URI objects from source to the target.
 * For target to source transfers we utilize our own IDataObject / IStream implementations.
 *
 * @returns VBox status code.
 * @param   hThread             Thread handle.
 * @param   pvUser              User arguments; is PSHAREDCLIPBOARDURITRANSFER.
 */
static int sharedClipboardURITransferReadThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* At the moment we only support one transfer at a time. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer = (PSHAREDCLIPBOARDURITRANSFER)pvUser;
    AssertPtr(pTransfer);

    int rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        pTransfer->Thread.fStarted = true;

    int rc2 = RTThreadUserSignal(hThread);
    const bool fSignalled = RT_SUCCESS(rc2);

    if (RT_SUCCESS(rc))
        rc = SharedClipboardURITransferRead(pTransfer);

    if (!fSignalled)
    {
        rc2 = RTThreadUserSignal(hThread);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates the internal meta data buffer of an URI clipboard transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to create internal meta data for.
 * @param   cbSize              Size (in bytes) of meta data buffer to create. An existing meta data buffer
 *                              will be resized accordingly.
 */
static int sharedClipboardURITransferMetaDataCreateInternal(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t cbSize)
{
    int rc;

    LogFlowFuncEnter();

    if (pTransfer->State.pMeta == NULL)
    {
        pTransfer->State.pMeta = (PSHAREDCLIPBOARDMETADATA)RTMemAlloc(sizeof(SHAREDCLIPBOARDMETADATA));
        if (pTransfer->State.pMeta)
        {
            /** @todo Make meta data format handling more flexible. */
            rc = SharedClipboardMetaDataInit(pTransfer->State.pMeta, SHAREDCLIPBOARDMETADATAFMT_URI_LIST);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = SharedClipboardMetaDataResize(pTransfer->State.pMeta, cbSize);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys a clipboard URI transfer's internal meta data.
 *
 * @param   pTransfer           URI clipboard transfer to destroy internal meta data of.
 */
static void sharedClipboardURITransferMetaDataDestroyInternal(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    if (!pTransfer->State.pMeta)
        return;

    LogFlowFuncEnter();

    /* We're done processing the meta data, so just destroy it. */
    SharedClipboardMetaDataDestroy(pTransfer->State.pMeta);

    RTMemFree(pTransfer->State.pMeta);
    pTransfer->State.pMeta = NULL;
}

/**
 * Adds meta data for a clipboard URI transfer, internal version.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to set meta data for.
 * @param   pvMeta              Pointer to meta data buffer.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 */
static int sharedClipboardURITransferMetaDataAddInternal(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                                         const void *pvMeta, uint32_t cbMeta)
{
    LogFlowFunc(("pvMeta=%p, cbMeta=%RU32\n", pvMeta, cbMeta));

    int rc = SharedClipboardMetaDataAdd(pTransfer->State.pMeta, pvMeta, cbMeta);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Adds meta data for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to set meta data for.
 * @param   pvMeta              Pointer to meta data buffer.
 * @param   cbMeta              Size (in bytes) of meta data buffer.
 */
int SharedClipboardURITransferMetaDataAdd(PSHAREDCLIPBOARDURITRANSFER pTransfer, const void *pvMeta, uint32_t cbMeta)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = sharedClipboardURITransferMetaDataCreateInternal(pTransfer, cbMeta);
    if (RT_SUCCESS(rc))
        rc = sharedClipboardURITransferMetaDataAddInternal(pTransfer, pvMeta, cbMeta);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns whether the meta data is in a complete state (e.g. completetely read / written) or not.
 *
 * @returns \c true if meta data is complete, \c false if not.
 * @param   pTransfer           URI clipboard transfer to get completion status of meta data for.
 */
bool SharedClipboardURITransferMetaDataIsComplete(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer->State.pHeader, false);
    AssertPtrReturn(pTransfer->State.pMeta,   false);

    return SharedClipboardMetaDataGetUsed(pTransfer->State.pMeta) == pTransfer->State.pHeader->cbMeta;
}

/**
 * Reads meta for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to read meta data for.
 * @param   pcbRead             How much meta data (in bytes) was read on success.
 */
int SharedClipboardURITransferMetaDataRead(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t *pcbRead)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    /* pcbRead is optional. */

    LogFlowFuncEnter();

    /* Destroy any former meta data. */
    SharedClipboardMetaDataDestroy(pTransfer->State.pMeta);

    uint32_t cbReadTotal = 0;

    int rc = pTransfer->ProviderIface.pfnReadDataHdr(&pTransfer->ProviderCtx, &pTransfer->State.pHeader);
    if (RT_SUCCESS(rc))
    {
        uint32_t cbMeta = _4K; /** @todo Improve. */
        void    *pvMeta = RTMemAlloc(cbMeta);

        if (pvMeta)
        {
            AssertPtr(pTransfer->State.pHeader);
            uint32_t cbMetaToRead = pTransfer->State.pHeader->cbMeta;
            while (cbMetaToRead)
            {
                uint32_t cbMetaRead;
                rc = pTransfer->ProviderIface.pfnReadDataChunk(&pTransfer->ProviderCtx,
                                                               pTransfer->State.pHeader, pvMeta, cbMeta, 0 /* fFlags */,
                                                               &cbMetaRead);
                if (RT_SUCCESS(rc))
                    rc = sharedClipboardURITransferMetaDataAddInternal(pTransfer, pvMeta, cbMeta);

                if (RT_FAILURE(rc))
                    break;

                Assert(cbMetaToRead >= cbMetaRead);
                cbMetaToRead -= cbMetaRead;

                cbReadTotal += cbReadTotal;
            }

            RTMemFree(pvMeta);

            if (RT_SUCCESS(rc))
            {
                if (pcbRead)
                    *pcbRead = cbReadTotal;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes the actual meta data.
 *
 * @returns IPRT status code.
 * @param   pTransfer           Transfer to write meta data for.
 * @param   pcbWritten          How much bytes were written on success. Optional.
 */
int SharedClipboardURITransferMetaDataWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t *pcbWritten)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    /* pcbWritten is optional. */

    LogFlowFuncEnter();

    AssertPtrReturn(pTransfer->State.pHeader, VERR_WRONG_ORDER);
    AssertPtrReturn(pTransfer->State.pMeta,   VERR_WRONG_ORDER);

    uint32_t cbWrittenTotal = 0;

    int rc = pTransfer->ProviderIface.pfnWriteDataHdr(&pTransfer->ProviderCtx, pTransfer->State.pHeader);
    if (RT_SUCCESS(rc))
    {
        /* Sanity. */
        Assert(pTransfer->State.pHeader->cbMeta == pTransfer->State.pMeta->cbUsed);

        uint32_t cbMetaToWrite = pTransfer->State.pHeader->cbMeta;
        while (cbMetaToWrite)
        {
            uint32_t cbMetaWritten;
            rc = pTransfer->ProviderIface.pfnWriteDataChunk(&pTransfer->ProviderCtx, pTransfer->State.pHeader,
                                                            (uint8_t *)pTransfer->State.pMeta->pvMeta + cbWrittenTotal,
                                                            cbMetaToWrite, 0 /* fFlags */, &cbMetaWritten);
            if (RT_FAILURE(rc))
                break;

            Assert(cbMetaToWrite >= cbMetaWritten);
            cbMetaToWrite -= cbMetaWritten;

            cbWrittenTotal += cbMetaWritten;
            Assert(cbWrittenTotal <= pTransfer->State.pHeader->cbMeta);
        }

        if (RT_SUCCESS(rc))
        {
            if (pcbWritten)
                *pcbWritten = cbWrittenTotal;

            if (pTransfer->Callbacks.pfnDataComplete)
            {
                SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
                pTransfer->Callbacks.pfnDataComplete(&callbackData);
            }
        }
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes all URI objects using the connected provider.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to write objects for.
 */
int SharedClipboardURITransferWriteObjects(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    AssertPtrReturn(pTransfer->pURIList, VERR_WRONG_ORDER);

    while (!pTransfer->pURIList->IsEmpty())
    {
        SharedClipboardURIObject *pObj = pTransfer->pURIList->First();
        AssertPtrBreakStmt(pObj, rc = VERR_INVALID_POINTER);

        switch (pObj->GetType())
        {
            case SharedClipboardURIObject::Type_Directory:
            {
                RTCString strPath = pObj->GetDestPathAbs();
                LogFlowFunc(("strDir=%s (%zu), fMode=0x%x\n",
                             strPath.c_str(), strPath.length(), pObj->GetMode()));

                VBOXCLIPBOARDDIRDATA dirData;
                SharedClipboardURIDirDataInit(&dirData);

                dirData.pszPath = RTStrDup(strPath.c_str());
                dirData.cbPath  = (uint32_t)strlen(dirData.pszPath);

                rc = pTransfer->ProviderIface.pfnWriteDirectory(&pTransfer->ProviderCtx, &dirData);

                SharedClipboardURIDirDataDestroy(&dirData);
                break;
            }

            case SharedClipboardURIObject::Type_File:
            {
                AssertBreakStmt(pObj->IsOpen(), rc = VERR_INVALID_STATE);

                RTCString strPath = pObj->GetDestPathAbs();

                LogFlowFunc(("strFile=%s (%zu), cbSize=%RU64, fMode=0x%x\n", strPath.c_str(), strPath.length(),
                             pObj->GetSize(), pObj->GetMode()));

                VBOXCLIPBOARDFILEHDR fileHdr;
                SharedClipboardURIFileHdrInit(&fileHdr);

                fileHdr.pszFilePath = RTStrDup(strPath.c_str());
                fileHdr.cbFilePath  = (uint32_t)strlen(fileHdr.pszFilePath);
                fileHdr.cbSize      = pObj->GetSize();
                fileHdr.fFlags      = 0;
                fileHdr.fMode       = pObj->GetMode();

                rc = pTransfer->ProviderIface.pfnWriteFileHdr(&pTransfer->ProviderCtx, &fileHdr);
                SharedClipboardURIFileHdrDestroy(&fileHdr);

                if (RT_FAILURE(rc))
                    break;

                uint32_t cbData = _64K; /** @todo Improve. */
                void    *pvData = RTMemAlloc(cbData);

                AssertPtrBreakStmt(pvData, rc = VERR_NO_MEMORY);

                while (!pObj->IsComplete())
                {
                    uint32_t cbRead;
                    rc = pObj->Read(pvData, cbData, &cbRead);
                    if (RT_SUCCESS(rc))
                    {
                        uint32_t cbToRead = cbRead;
                        rc = pTransfer->ProviderIface.pfnWriteFileData(&pTransfer->ProviderCtx,
                                                                       pvData, cbToRead, 0 /* fFlags */, &cbRead);
                    }

                    if (RT_FAILURE(rc))
                        break;
                }

                RTMemFree(pvData);
                pvData = NULL;
                break;
            }

            default:
                AssertFailed();
                break;
        }

        if (RT_FAILURE(rc))
            break;

        /* Only remove current object on success. */
        pTransfer->pURIList->RemoveFirst();
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Thread for transferring (writing) URI objects from source to the target.
 * For target to source transfers we utilize our own IDataObject / IStream implementations.
 *
 * @returns VBox status code.
 * @param   hThread             Thread handle.
 * @param   pvUser              User arguments; is PSHAREDCLIPBOARDURITRANSFER.
 */
static int sharedClipboardURITransferWriteThread(RTTHREAD hThread, void *pvUser)
{
    AssertPtrReturn(pvUser, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    /* At the moment we only support one transfer at a time. */
    PSHAREDCLIPBOARDURITRANSFER pTransfer = (PSHAREDCLIPBOARDURITRANSFER)pvUser;

    int rc = VINF_SUCCESS;

    if (RT_SUCCESS(rc))
        pTransfer->Thread.fStarted = true;

    int rc2 = RTThreadUserSignal(hThread);
    const bool fSignalled = RT_SUCCESS(rc2);

    if (RT_SUCCESS(rc))
        rc = SharedClipboardURITransferWrite(pTransfer);

    if (!fSignalled)
    {
        rc2 = RTThreadUserSignal(hThread);
        AssertRC(rc2);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Main function to write a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to write.
 */
int SharedClipboardURITransferWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    AssertMsgReturn(pTransfer->State.enmStatus == SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING,
                    ("Transfer already is running\n"), VERR_WRONG_ORDER);

    pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING;

    if (pTransfer->Callbacks.pfnTransferStarted)
    {
        SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
        pTransfer->Callbacks.pfnTransferStarted(&callbackData);
    }

    int rc = SharedClipboardURITransferMetaDataWrite(pTransfer, NULL /* pcbWritten */);
    if (RT_SUCCESS(rc))
    {
        rc = SharedClipboardURITransferWriteObjects(pTransfer);
        if (RT_SUCCESS(rc))
        {
            if (pTransfer->Callbacks.pfnTransferComplete)
            {
                SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
                pTransfer->Callbacks.pfnTransferComplete(&callbackData, rc);
            }
        }
    }

    if (RT_FAILURE(rc))
    {
        if (pTransfer->Callbacks.pfnTransferError)
        {
            SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
            pTransfer->Callbacks.pfnTransferError(&callbackData, rc);
        }
    }

    pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_NONE;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Initializes a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to initialize.
 */
int SharedClipboardURICtxInit(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = RTCritSectInit(&pURI->CritSect);
    if (RT_SUCCESS(rc))
    {
        RTListInit(&pURI->List);

        pURI->cRunning    = 0;
        pURI->cMaxRunning = 1; /* For now we only support one transfer per client at a time. */

#ifdef DEBUG_andy
        pURI->cMaxRunning = UINT32_MAX;
#endif
        SharedClipboardURICtxReset(pURI);
    }

    return VINF_SUCCESS;
}

/**
 * Destroys an URI clipboard information context struct.
 *
 * @param   pURI                URI clipboard context to destroy.
 */
void SharedClipboardURICtxDestroy(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFuncEnter();

    RTCritSectDelete(&pURI->CritSect);

    PSHAREDCLIPBOARDURITRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pURI->List, pTransfer, pTransferNext, SHAREDCLIPBOARDURITRANSFER, Node)
    {
        SharedClipboardURITransferDestroy(pTransfer);
        RTListNodeRemove(&pTransfer->Node);
    }

    pURI->cRunning  = 0;
#ifdef DEBUG
    pURI->cTransfer = 0;
#endif
}

/**
 * Resets an clipboard URI transfer.
 *
 * @param   pURI                URI clipboard context to reset.
 */
void SharedClipboardURICtxReset(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFuncEnter();

    PSHAREDCLIPBOARDURITRANSFER pTransfer;
    RTListForEach(&pURI->List, pTransfer, SHAREDCLIPBOARDURITRANSFER, Node)
        SharedClipboardURITransferReset(pTransfer);
}

/**
 * Adds a new URI transfer to an clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to add transfer to.
 * @param   pTransfer           Pointer to URI clipboard transfer to add.
 */
int SharedClipboardURICtxTransferAdd(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pURI,      VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    if (pURI->cRunning == pURI->cMaxRunning)
        return VERR_SHCLPB_MAX_TRANSFERS_REACHED;

    RTListAppend(&pURI->List, &pTransfer->Node);

#ifdef DEBUG
    pURI->cTransfer++;
    LogFlowFunc(("cTransfers=%RU32, cRunning=%RU32\n", pURI->cTransfer, pURI->cRunning));
#endif

    return VINF_SUCCESS;
}

/**
 * Removes an URI transfer from a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to remove transfer from.
 * @param   pTransfer           Pointer to URI clipboard transfer to remove.
 */
int SharedClipboardURICtxTransferRemove(PSHAREDCLIPBOARDURICTX pURI, PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    AssertPtrReturn(pURI,      VERR_INVALID_POINTER);
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();


    int rc = SharedClipboardURITransferDestroy(pTransfer);
    if (RT_SUCCESS(rc))
    {
        RTListNodeRemove(&pTransfer->Node);
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns a specific URI transfer, internal version.
 *
 * @returns URI transfer, or NULL if not found.
 * @param   pURI                URI clipboard context to return transfer for.
 * @param   uIdx                Index of the transfer to return.
 */
static PSHAREDCLIPBOARDURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx)
{
    AssertReturn(uIdx == 0, NULL); /* Only one transfer allowed at the moment. */
    return RTListGetFirst(&pURI->List, SHAREDCLIPBOARDURITRANSFER, Node);
}

/**
 * Returns a specific URI transfer.
 *
 * @returns URI transfer, or NULL if not found.
 * @param   pURI                URI clipboard context to return transfer for.
 * @param   uIdx                Index of the transfer to return.
 */
PSHAREDCLIPBOARDURITRANSFER SharedClipboardURICtxGetTransfer(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx)
{
    return sharedClipboardURICtxGetTransferInternal(pURI, uIdx);
}

/**
 * Returns the number of running URI transfers.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to return number for.
 */
uint32_t SharedClipboardURICtxGetRunningTransfers(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, 0);
    return pURI->cRunning;
}

/**
 * Cleans up all associated transfers which are not needed (anymore).
 * This can be due to transfers which only have been announced but not / never being run.
 *
 * @param   pURI                URI clipboard context to cleanup transfers for.
 */
void SharedClipboardURICtxTransfersCleanup(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFunc(("cRunning=%RU32\n", pURI->cRunning));

    /* Remove all transfers which are not in a running state (e.g. only announced). */
    PSHAREDCLIPBOARDURITRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pURI->List, pTransfer, pTransferNext, SHAREDCLIPBOARDURITRANSFER, Node)
    {
        if (SharedClipboardURITransferGetStatus(pTransfer) != SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING)
        {
            SharedClipboardURITransferDestroy(pTransfer);
            RTListNodeRemove(&pTransfer->Node);

#ifdef DEBUG
            Assert(pURI->cTransfer);
            pURI->cTransfer--;

            LogFlowFunc(("cTransfers=%RU32\n", pURI->cTransfer));
#endif
        }
    }
}

/**
 * Returns whether the maximum of concurrent transfers of a specific URI context has been reached or not.
 *
 * @returns \c if maximum has been reached, \c false if not.
 * @param   pURI                URI clipboard context to determine value for.
 */
bool SharedClipboardURICtxTransfersMaximumReached(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, true);

    LogFlowFunc(("cRunning=%RU32\n", pURI->cRunning));

    Assert(pURI->cRunning <= pURI->cMaxRunning);
    return pURI->cRunning == pURI->cMaxRunning;
}

