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

#include <iprt/dir.h>
#include <iprt/file.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>

#include <VBox/err.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/GuestHost/SharedClipboard-uri.h>


#ifdef VBOX_WITH_SHARED_CLIPBOARD_URI_LIST
static int sharedClipboardURITransferThreadCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser);
static int sharedClipboardURITransferThreadDestroy(PSHAREDCLIPBOARDURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs);
static int sharedClipboardURITransferWriteThread(RTTHREAD hThread, void *pvUser);
static PSHAREDCLIPBOARDURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHAREDCLIPBOARDURICTX pURI, uint32_t uIdx);
#endif


/**
 * Allocates a VBOXCLIPBOARDListHdr structure.
 *
 * @returns VBox status code.
 * @param   ppListEntry         Where to store the allocated VBOXCLIPBOARDListHdr structure on success.
 */
int SharedClipboardURIListHdrAlloc(PVBOXCLIPBOARDLISTHDR *ppListHdr)
{
    int rc;

    PVBOXCLIPBOARDLISTHDR pListHdr = (PVBOXCLIPBOARDLISTHDR)RTMemAllocZ(sizeof(VBOXCLIPBOARDLISTHDR));
    if (pListHdr)
    {
        *ppListHdr = pListHdr;
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Frees a VBOXCLIPBOARDListHdr structure.
 *
 * @param   pListEntry          VBOXCLIPBOARDListHdr structure to free.
 */
void SharedClipboardURIListHdrFree(PVBOXCLIPBOARDLISTHDR pListHdr)
{
    if (!pListHdr)
        return;

    LogFlowFuncEnter();

    SharedClipboardURIListHdrDestroy(pListHdr);

    RTMemFree(pListHdr);
    pListHdr = NULL;
}

/**
 * Duplicates (allocates) a VBOXCLIPBOARDListHdr structure.
 *
 * @returns Duplicated VBOXCLIPBOARDListHdr structure on success.
 * @param   pListHdr            VBOXCLIPBOARDListHdr to duplicate.
 */
PVBOXCLIPBOARDLISTHDR SharedClipboardURIListHdrDup(PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, NULL);

    PVBOXCLIPBOARDLISTHDR pListHdrDup = (PVBOXCLIPBOARDLISTHDR)RTMemAlloc(sizeof(VBOXCLIPBOARDLISTHDR));
    if (pListHdrDup)
    {
        *pListHdrDup = *pListHdr;
    }

    return pListHdrDup;
}

/**
 * Initializes an URI data header struct.
 *
 * @returns VBox status code.
 * @param   pListHdr            Data header struct to initialize.
 */
int SharedClipboardURIListHdrInit(PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    SharedClipboardURIListHdrReset(pListHdr);

    return VINF_SUCCESS;
}

/**
 * Destroys an URI data header struct.
 *
 * @param   pListHdr            Data header struct to destroy.
 */
void SharedClipboardURIListHdrDestroy(PVBOXCLIPBOARDLISTHDR pListHdr)
{
    if (!pListHdr)
        return;

    LogFlowFuncEnter();
}

/**
 * Resets a VBOXCLIPBOARDListHdr structture.
 *
 * @returns VBox status code.
 * @param   pListHdr            VBOXCLIPBOARDListHdr structture to reset.
 */
void SharedClipboardURIListHdrReset(PVBOXCLIPBOARDLISTHDR pListHdr)
{
    AssertPtrReturnVoid(pListHdr);

    LogFlowFuncEnter();

    RT_BZERO(pListHdr, sizeof(VBOXCLIPBOARDLISTHDR));
}

/**
 * Returns whether a given clipboard data header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pListHdr            Clipboard data header to validate.
 */
bool SharedClipboardURIListHdrIsValid(PVBOXCLIPBOARDLISTHDR pListHdr)
{
    RT_NOREF(pListHdr);
    return true; /** @todo Implement this. */
}

int SharedClipboardURIListOpenParmsCopy(PVBOXCLIPBOARDLISTOPENPARMS pDst, PVBOXCLIPBOARDLISTOPENPARMS pSrc)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    if (pSrc->pszFilter)
    {
        pDst->pszFilter = RTStrDup(pSrc->pszFilter);
        if (!pDst->pszFilter)
            rc = VERR_NO_MEMORY;
    }

    if (   RT_SUCCESS(rc)
        && pSrc->pszPath)
    {
        pDst->pszPath = RTStrDup(pSrc->pszPath);
        if (!pDst->pszPath)
            rc = VERR_NO_MEMORY;
    }

    if (RT_SUCCESS(rc))
    {
        pDst->fList    = pDst->fList;
        pDst->cbFilter = pSrc->cbFilter;
        pDst->cbPath   = pSrc->cbPath;
    }

    return rc;
}

PVBOXCLIPBOARDLISTOPENPARMS SharedClipboardURIListOpenParmsDup(PVBOXCLIPBOARDLISTOPENPARMS pParms)
{
    AssertPtrReturn(pParms, NULL);

    PVBOXCLIPBOARDLISTOPENPARMS pParmsDup = (PVBOXCLIPBOARDLISTOPENPARMS)RTMemAllocZ(sizeof(VBOXCLIPBOARDLISTOPENPARMS));
    if (!pParmsDup)
        return NULL;

    int rc = SharedClipboardURIListOpenParmsCopy(pParmsDup, pParms);
    if (RT_FAILURE(rc))
    {
        SharedClipboardURIListOpenParmsDestroy(pParmsDup);

        RTMemFree(pParmsDup);
        pParmsDup = NULL;
    }

    return pParmsDup;
}

int SharedClipboardURIListOpenParmsInit(PVBOXCLIPBOARDLISTOPENPARMS pParms)
{
    AssertPtrReturn(pParms, VERR_INVALID_POINTER);

    RT_BZERO(pParms, sizeof(VBOXCLIPBOARDLISTOPENPARMS));

    pParms->cbFilter  = 64;
    pParms->pszFilter = RTStrAlloc(pParms->cbFilter);

    pParms->cbPath    = RTPATH_MAX;
    pParms->pszPath   = RTStrAlloc(pParms->cbPath);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

void SharedClipboardURIListOpenParmsDestroy(PVBOXCLIPBOARDLISTOPENPARMS pParms)
{
    if (!pParms)
        return;

    if (pParms->pszFilter)
    {
        RTStrFree(pParms->pszFilter);
        pParms->pszFilter = NULL;
    }

    if (pParms->pszPath)
    {
        RTStrFree(pParms->pszPath);
        pParms->pszPath = NULL;
    }
}

/**
 * Creates (allocates) and initializes a clipboard list entry structure.
 *
 * @param   ppDirData           Where to return the created clipboard list entry structure on success.
 */
int SharedClipboardURIListEntryAlloc(PVBOXCLIPBOARDLISTENTRY *ppListEntry)
{
    PVBOXCLIPBOARDLISTENTRY pListEntry = (PVBOXCLIPBOARDLISTENTRY)RTMemAlloc(sizeof(VBOXCLIPBOARDLISTENTRY));
    if (!pListEntry)
        return VERR_NO_MEMORY;

    int rc = SharedClipboardURIListEntryInit(pListEntry);
    if (RT_SUCCESS(rc))
        *ppListEntry = pListEntry;

    return rc;
}

/**
 * Frees a clipboard list entry structure.
 *
 * @param   pListEntry         Clipboard list entry structure to free.
 */
void SharedClipboardURIListEntryFree(PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    if (!pListEntry)
        return;

    SharedClipboardURIListEntryDestroy(pListEntry);
    RTMemFree(pListEntry);
}

/**
 * (Deep) Copies a clipboard list entry structure.
 *
 * @returns VBox status code.
 * @param   pListEntry          Clipboard list entry to copy.
 */
int SharedClipboardURIListEntryCopy(PVBOXCLIPBOARDLISTENTRY pDst, PVBOXCLIPBOARDLISTENTRY pSrc)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    *pDst = *pSrc;

    if (pSrc->pvInfo)
    {
        pDst->pvInfo = RTMemDup(pSrc->pvInfo, pSrc->cbInfo);
        if (pDst->pvInfo)
        {
            pDst->cbInfo = pSrc->cbInfo;
        }
        else
            rc = VERR_NO_MEMORY;
    }

    if (RT_FAILURE(rc))
    {
        if (pDst->pvInfo)
        {
            RTMemFree(pDst->pvInfo);
            pDst->pvInfo = NULL;
            pDst->cbInfo = 0;
        }
    }

    return rc;
}

/**
 * Duplicates (allocates) a clipboard list entry structure.
 *
 * @returns Duplicated clipboard list entry structure on success.
 * @param   pListEntry          Clipboard list entry to duplicate.
 */
PVBOXCLIPBOARDLISTENTRY SharedClipboardURIListEntryDup(PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, NULL);

    int rc = VINF_SUCCESS;

    PVBOXCLIPBOARDLISTENTRY pListEntryDup = (PVBOXCLIPBOARDLISTENTRY)RTMemAllocZ(sizeof(VBOXCLIPBOARDLISTENTRY));
    if (pListEntryDup)
        rc = SharedClipboardURIListEntryCopy(pListEntryDup, pListEntry);

    if (RT_FAILURE(rc))
    {
        SharedClipboardURIListEntryDestroy(pListEntryDup);

        RTMemFree(pListEntryDup);
        pListEntryDup = NULL;
    }

    return pListEntryDup;
}

/**
 * Initializes a clipboard list entry structure.
 *
 * @param   pListEntry          clipboard list entry structure to initialize.
 */
int SharedClipboardURIListEntryInit(PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    RT_BZERO(pListEntry, sizeof(VBOXCLIPBOARDLISTENTRY));

    return VINF_SUCCESS;
}

/**
 * Initializes a clipboard list entry structure.
 *
 * @param   pListEntry          clipboard list entry structure to destroy.
 */
void SharedClipboardURIListEntryDestroy(PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    if (!pListEntry)
        return;

    if (pListEntry->pvInfo)
    {
        RTMemFree(pListEntry->pvInfo);
        pListEntry->pvInfo = NULL;
        pListEntry->cbInfo = 0;
    }
}

/**
 * Returns whether a given clipboard data chunk is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pListEntry          Clipboard data chunk to validate.
 */
bool SharedClipboardURIListEntryIsValid(PVBOXCLIPBOARDLISTENTRY pListEntry)
{
    RT_NOREF(pListEntry);

    /** @todo Verify checksum. */

    return true; /** @todo Implement this. */
}

#if 0
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
 * @param   pListHdr            Data header to use for validation.
 */
bool SharedClipboardURIFileHdrIsValid(PVBOXCLIPBOARDFILEHDR pFileHdr, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    if (   !pFileHdr->cbFilePath
        || pFileHdr->cbFilePath > RTPATH_MAX)
        return false;

    if (!RTStrIsValidEncoding(pFileHdr->pszFilePath))
        return false;

    if (pFileHdr->cbSize > pListHdr->cbTotalSize)
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
 * @param   pListHdr            Data header to use for validation.
 */
bool SharedClipboardURIFileDataIsValid(PVBOXCLIPBOARDFILEDATA pFileData, PVBOXCLIPBOARDLISTHDR pListHdr)
{
    RT_NOREF(pFileData, pListHdr);
    return true;
}
#endif

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

    pObjCtx->uHandle  = SHAREDCLIPBOARDOBJHANDLE_INVALID;

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
            && pObjCtx->uHandle != SHAREDCLIPBOARDOBJHANDLE_INVALID);
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

    pTransfer->pArea = NULL; /* Will be created later if needed. */

    pTransfer->Thread.hThread    = NIL_RTTHREAD;
    pTransfer->Thread.fCancelled = false;
    pTransfer->Thread.fStarted   = false;
    pTransfer->Thread.fStop      = false;

    pTransfer->pvUser = NULL;
    pTransfer->cbUser = 0;

    RT_ZERO(pTransfer->Callbacks);

    pTransfer->pMapEvents = new SharedClipboardURITransferEventMap();
    if (pTransfer->pMapEvents)
    {
        pTransfer->pMapLists = new SharedClipboardURIListMap();
        if (pTransfer->pMapLists)
        {
            *ppTransfer = pTransfer;
        }
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
    {
        RTMemFree(pTransfer);
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

    if (pTransfer->pMapEvents)
    {
        delete pTransfer->pMapEvents;
        pTransfer->pMapEvents = NULL;
    }

    if (pTransfer->pMapLists)
    {
        delete pTransfer->pMapLists;
        pTransfer->pMapLists = NULL;
    }

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

int SharedClipboardURITransferOpen(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    int rc = VINF_SUCCESS;

    if (pTransfer->ProviderIface.pfnTransferOpen)
        rc = pTransfer->ProviderIface.pfnTransferOpen(&pTransfer->ProviderCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferClose(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    int rc = VINF_SUCCESS;

    if (pTransfer->ProviderIface.pfnTransferClose)
        rc = pTransfer->ProviderIface.pfnTransferClose(&pTransfer->ProviderCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static SHAREDCLIPBOARDLISTHANDLE sharedClipboardURITransferListHandleNew(PSHAREDCLIPBOARDURITRANSFER pTransfer)
{
    RT_NOREF(pTransfer);

    return 42; /** @todo FIX !!!!! */
}

int SharedClipboardURITransferListOpen(PSHAREDCLIPBOARDURITRANSFER pTransfer, PVBOXCLIPBOARDLISTOPENPARMS pOpenParms,
                                       PSHAREDCLIPBOARDLISTHANDLE phList)
{
    AssertPtrReturn(pTransfer,  VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,     VERR_INVALID_POINTER);

    int rc;

    SHAREDCLIPBOARDLISTHANDLE hList = SHAREDCLIPBOARDLISTHANDLE_INVALID;

    if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_LOCAL)
    {
        PSHAREDCLIPBOARDURILISTHANDLEINFO pInfo
            = (PSHAREDCLIPBOARDURILISTHANDLEINFO)RTMemAlloc(sizeof(SHAREDCLIPBOARDURILISTHANDLEINFO));
        if (pInfo)
        {
            if (   !pOpenParms->pszPath
                || !strlen(pOpenParms->pszPath))
                RTStrAPrintf(&pOpenParms->pszPath, "C:\\Temp"); /** @todo FIX !!!! */

            RTFSOBJINFO objInfo;
            rc = RTPathQueryInfo(pOpenParms->pszPath, &objInfo, RTFSOBJATTRADD_NOTHING);
            if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
            {
                rc = RTDirOpen(&pInfo->u.Local.hDirRoot, pOpenParms->pszPath);
            }
            else if (RTFS_IS_FILE(objInfo.Attr.fMode))
            {
                rc = RTFileOpen(&pInfo->u.Local.hFile, pOpenParms->pszPath, RTFILE_O_READ | RTFILE_O_DENY_WRITE);
            }
            else if (RTFS_IS_SYMLINK(objInfo.Attr.fMode))
            {
                rc = VERR_NOT_IMPLEMENTED; /** @todo */
            }
            else
                AssertFailedStmt(rc = VERR_NOT_SUPPORTED);

            if (RT_SUCCESS(rc))
                rc = SharedClipboardURIListOpenParmsCopy(&pInfo->OpenParms, pOpenParms);

            if (RT_SUCCESS(rc))
            {
                pInfo->fMode = objInfo.Attr.fMode;

                hList = sharedClipboardURITransferListHandleNew(pTransfer);

                pTransfer->pMapLists->insert(
                    std::pair<SHAREDCLIPBOARDLISTHANDLE, PSHAREDCLIPBOARDURILISTHANDLEINFO>(hList, pInfo));
            }
            else
            {
                if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                {
                    if (RTDirIsValid(pInfo->u.Local.hDirRoot))
                        RTDirClose(pInfo->u.Local.hDirRoot);
                }
                else if (RTFS_IS_FILE(objInfo.Attr.fMode))
                {
                    if (RTFileIsValid(pInfo->u.Local.hFile))
                        RTFileClose(pInfo->u.Local.hFile);
                }

                RTMemFree(pInfo);
                pInfo = NULL;
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnListOpen)
        {
            rc = pTransfer->ProviderIface.pfnListOpen(&pTransfer->ProviderCtx, pOpenParms, &hList);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

    if (RT_SUCCESS(rc))
        *phList = hList;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferListClose(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    if (hList == SHAREDCLIPBOARDLISTHANDLE_INVALID)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_LOCAL)
    {
        SharedClipboardURIListMap::iterator itList = pTransfer->pMapLists->find(hList);
        if (itList != pTransfer->pMapLists->end())
        {
            PSHAREDCLIPBOARDURILISTHANDLEINFO pInfo = itList->second;
            AssertPtr(pInfo);

            if (RTDirIsValid(pInfo->u.Local.hDirRoot))
                RTDirClose(pInfo->u.Local.hDirRoot);

            RTMemFree(pInfo);

            pTransfer->pMapLists->erase(itList);
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnListClose)
        {
            rc = pTransfer->ProviderIface.pfnListClose(&pTransfer->ProviderCtx, hList);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int sharedClipboardURITransferListHdrAddFile(PVBOXCLIPBOARDLISTHDR pHdr, const char *pszPath)
{
    uint64_t cbSize = 0;
    int rc = RTFileQuerySize(pszPath, &cbSize);
    if (RT_SUCCESS(rc))
    {
        pHdr->cbTotalSize  += cbSize;
        pHdr->cTotalObjects++;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

static int sharedClipboardURITransferListHdrFromDir(PVBOXCLIPBOARDLISTHDR pHdr,
                                                    const char *pcszSrcPath, const char *pcszDstPath,
                                                    const char *pcszDstBase, size_t cchDstBase)
{
    AssertPtrReturn(pcszSrcPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDstBase, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDstPath, VERR_INVALID_POINTER);

    LogFlowFunc(("pcszSrcPath=%s, pcszDstPath=%s, pcszDstBase=%s, cchDstBase=%zu\n",
                 pcszSrcPath, pcszDstPath, pcszDstBase, cchDstBase));

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszSrcPath, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
        {
            pHdr->cTotalObjects++; /* Add directory itself. */

            if (RT_SUCCESS(rc))
            {
                RTDIR hDir;
                rc = RTDirOpen(&hDir, pcszSrcPath);
                if (RT_SUCCESS(rc))
                {
                    size_t        cbDirEntry = 0;
                    PRTDIRENTRYEX pDirEntry  = NULL;
                    do
                    {
                        /* Retrieve the next directory entry. */
                        rc = RTDirReadExA(hDir, &pDirEntry, &cbDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                        if (RT_FAILURE(rc))
                        {
                            if (rc == VERR_NO_MORE_FILES)
                                rc = VINF_SUCCESS;
                            break;
                        }

                        switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
                        {
                            case RTFS_TYPE_DIRECTORY:
                            {
                                /* Skip "." and ".." entries. */
                                if (RTDirEntryExIsStdDotLink(pDirEntry))
                                    break;

                                char *pszSrc = RTPathJoinA(pcszSrcPath, pDirEntry->szName);
                                if (pszSrc)
                                {
                                    char *pszDst = RTPathJoinA(pcszDstPath, pDirEntry->szName);
                                    if (pszDst)
                                    {
                                        rc = sharedClipboardURITransferListHdrFromDir(pHdr, pszSrc, pszDst,
                                                                                      pcszDstBase, cchDstBase);
                                        RTStrFree(pszDst);
                                    }
                                    else
                                        rc = VERR_NO_MEMORY;

                                    RTStrFree(pszSrc);
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                                break;
                            }

                            case RTFS_TYPE_FILE:
                            {
                                char *pszSrc = RTPathJoinA(pcszSrcPath, pDirEntry->szName);
                                if (pszSrc)
                                {
                                    rc = sharedClipboardURITransferListHdrAddFile(pHdr, pszSrc);
                                    RTStrFree(pszSrc);
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                                break;
                            }
                            case RTFS_TYPE_SYMLINK:
                            {
                                /** @todo Not implemented yet. */
                            }

                            default:
                                break;
                        }

                    } while (RT_SUCCESS(rc));

                    RTDirReadExAFree(&pDirEntry, &cbDirEntry);
                    RTDirClose(hDir);
                }
            }
        }
        else if (RTFS_IS_FILE(objInfo.Attr.fMode))
        {
            rc = sharedClipboardURITransferListHdrAddFile(pHdr, pcszSrcPath);
        }
        else if (RTFS_IS_SYMLINK(objInfo.Attr.fMode))
        {
            /** @todo Not implemented yet. */
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferListGetHeader(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList,
                                            PVBOXCLIPBOARDLISTHDR pHdr)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pHdr,      VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("hList=%RU64\n", hList));

    if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_LOCAL)
    {
        SharedClipboardURIListMap::iterator itList = pTransfer->pMapLists->find(hList);
        if (itList != pTransfer->pMapLists->end())
        {
            rc = SharedClipboardURIListHdrInit(pHdr);
            if (RT_SUCCESS(rc))
            {
                PSHAREDCLIPBOARDURILISTHANDLEINFO pInfo = itList->second;
                AssertPtr(pInfo);

                if (RTFS_IS_DIRECTORY(pInfo->fMode))
                {
                    char *pszSrcPath = RTStrDup(pInfo->OpenParms.pszPath);
                    if (pszSrcPath)
                    {
                        size_t cbSrcPathLen = RTPathStripTrailingSlash(pszSrcPath);
                        if (cbSrcPathLen)
                        {
                            char *pszFileName = RTPathFilename(pszSrcPath);
                            if (pszFileName)
                            {
                                Assert(pszFileName >= pszSrcPath);
                                size_t cchDstBase = pszFileName - pszSrcPath;
                                char *pszDstPath  = &pszSrcPath[cchDstBase];

                                LogFlowFunc(("pszSrcPath=%s, pszFileName=%s, pszDstPath=%s\n", pszSrcPath, pszFileName, pszDstPath));
                                rc = sharedClipboardURITransferListHdrFromDir(pHdr,
                                                                              pszSrcPath, pszSrcPath, pszSrcPath, cchDstBase);
                            }
                            else
                                rc = VERR_PATH_NOT_FOUND;
                        }
                        else
                            rc = VERR_INVALID_PARAMETER;

                        RTStrFree(pszSrcPath);
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
                else if (RTFS_IS_FILE(pInfo->fMode))
                {
                    pHdr->cTotalObjects = 1;

                    RTFSOBJINFO objInfo;
                    rc = RTFileQueryInfo(pInfo->u.Local.hFile, &objInfo, RTFSOBJATTRADD_NOTHING);
                    if (RT_SUCCESS(rc))
                    {
                        pHdr->cbTotalSize = objInfo.cbObject;
                    }
                }
                else if (RTFS_IS_SYMLINK(pInfo->fMode))
                {
                    rc = VERR_NOT_IMPLEMENTED; /** @todo */
                }
                else
                    AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
            }
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnListHdrRead)
        {
            rc = pTransfer->ProviderIface.pfnListHdrRead(&pTransfer->ProviderCtx, hList, pHdr);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }
    else
        AssertFailedStmt(rc = VERR_NOT_IMPLEMENTED);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns the current URI object for a clipboard URI transfer list.
 *
 * @returns Pointer to URI object.
 * @param   pTransfer           URI clipboard transfer to return URI object for.
 */
PSHAREDCLIPBOARDURITRANSFEROBJ SharedClipboardURITransferListGetObj(PSHAREDCLIPBOARDURITRANSFER pTransfer,
                                                                    SHAREDCLIPBOARDLISTHANDLE hList, uint64_t uIdx)
{
    AssertPtrReturn(pTransfer, NULL);

    RT_NOREF(hList, uIdx);

    LogFlowFunc(("hList=%RU64\n", hList));

    return NULL;
}

int SharedClipboardURITransferListRead(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList,
                                       PVBOXCLIPBOARDLISTENTRY pEntry)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pEntry,    VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    LogFlowFunc(("hList=%RU64\n", hList));

    if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_LOCAL)
    {
        SharedClipboardURIListMap::iterator itList = pTransfer->pMapLists->find(hList);
        if (itList != pTransfer->pMapLists->end())
        {
            PSHAREDCLIPBOARDURILISTHANDLEINFO pInfo = itList->second;
            AssertPtr(pInfo);

            if (RTFS_IS_DIRECTORY(pInfo->fMode))
            {
                size_t        cbDirEntry = 0;
                PRTDIRENTRYEX pDirEntry  = NULL;
                rc = RTDirReadExA(pInfo->u.Local.hDirRoot, &pDirEntry, &cbDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                if (RT_SUCCESS(rc))
                {
                    switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
                    {
                        case RTFS_TYPE_DIRECTORY:
                        {
                            /* Skip "." and ".." entries. */
                            if (RTDirEntryExIsStdDotLink(pDirEntry))
                                break;

                            RT_FALL_THROUGH();
                        }

                        case RTFS_TYPE_FILE:
                        {
                            pEntry->pvInfo = (PSHAREDCLIPBOARDFSOBJINFO)RTMemAlloc(sizeof(SHAREDCLIPBOARDFSOBJINFO));
                            if (pEntry->pvInfo)
                            {
                                SharedClipboardFsObjFromIPRT(PSHAREDCLIPBOARDFSOBJINFO(pEntry->pvInfo), &pDirEntry->Info);

                                pEntry->cbInfo = sizeof(SHAREDCLIPBOARDFSOBJINFO);
                                pEntry->fInfo  = 0; /** @todo Implement. */
                            }
                            else
                                rc = VERR_NO_MEMORY;
                            break;
                        }

                        case RTFS_TYPE_SYMLINK:
                        {
                            /** @todo Not implemented yet. */
                            break;
                        }

                        default:
                            break;
                    }

                    RTDirReadExAFree(&pDirEntry, &cbDirEntry);
                }
            }
            else if (RTFS_IS_FILE(pInfo->fMode))
            {
                RTFSOBJINFO objInfo;
                rc = RTFileQueryInfo(pInfo->u.Local.hFile, &objInfo, RTFSOBJATTRADD_NOTHING);
                if (RT_SUCCESS(rc))
                {
                    pEntry->pvInfo = (PSHAREDCLIPBOARDFSOBJINFO)RTMemAlloc(sizeof(SHAREDCLIPBOARDFSOBJINFO));
                    if (pEntry->pvInfo)
                    {
                        SharedClipboardFsObjFromIPRT(PSHAREDCLIPBOARDFSOBJINFO(pEntry->pvInfo), &objInfo);

                        pEntry->cbInfo = sizeof(SHAREDCLIPBOARDFSOBJINFO);
                        pEntry->fInfo  = 0; /** @todo Implement. */
                    }
                    else
                        rc = VERR_NO_MEMORY;
                }
            }
            else if (RTFS_IS_SYMLINK(pInfo->fMode))
            {
                rc = VERR_NOT_IMPLEMENTED;
            }
            else
                AssertFailedStmt(rc = VERR_NOT_SUPPORTED);
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnListEntryRead)
            rc = pTransfer->ProviderIface.pfnListEntryRead(&pTransfer->ProviderCtx, hList, pEntry);
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferListWrite(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList,
                                        PVBOXCLIPBOARDLISTENTRY pEntry)
{
    RT_NOREF(pTransfer, hList, pEntry);

    int rc = VINF_SUCCESS;

#if 0
    if (pTransfer->ProviderIface.pfnListEntryWrite)
        rc = pTransfer->ProviderIface.pfnListEntryWrite(&pTransfer->ProviderCtx, hList, pEntry);
#endif

    LogFlowFuncLeaveRC(rc);
    return rc;
}

bool SharedClipboardURITransferListHandleIsValid(PSHAREDCLIPBOARDURITRANSFER pTransfer, SHAREDCLIPBOARDLISTHANDLE hList)
{
    bool fIsValid = false;

    if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_LOCAL)
    {
        SharedClipboardURIListMap::iterator itList = pTransfer->pMapLists->find(hList);
        fIsValid = itList != pTransfer->pMapLists->end();
    }
    else if (pTransfer->State.enmSource == SHAREDCLIPBOARDSOURCE_REMOTE)
    {
        AssertFailed(); /** @todo Implement. */
    }

    return fIsValid;
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

    AssertMsgReturn(pTransfer->State.enmStatus == SHAREDCLIPBOARDURITRANSFERSTATUS_NONE,
                    ("Transfer has wrong state (%RU32)\n", pTransfer->State.enmStatus), VERR_WRONG_ORDER);

    LogFlowFunc(("pTransfer=%p, enmDir=%RU32\n", pTransfer, pTransfer->State.enmDir));

    if (pTransfer->Callbacks.pfnTransferPrepare)
    {
        SHAREDCLIPBOARDURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
        pTransfer->Callbacks.pfnTransferPrepare(&callbackData);
    }

    if (RT_SUCCESS(rc))
    {
        pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_READY;

        /** @todo Add checksum support. */
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Sets the URI provider interface for a given transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to create URI provider for.
 * @param   pCreationCtx        Provider creation context to use for provider creation.
 */
int SharedClipboardURITransferSetInterface(PSHAREDCLIPBOARDURITRANSFER pTransfer,
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
 * Runs (starts) an URI transfer thread.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to run.
 * @param   pfnThreadFunc       Pointer to thread function to use.
 * @param   pvUser              Pointer to user-provided data.
 */
int SharedClipboardURITransferRun(PSHAREDCLIPBOARDURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    AssertMsgReturn(pTransfer->State.enmStatus == SHAREDCLIPBOARDURITRANSFERSTATUS_READY,
                    ("Wrong status (currently is %RU32)\n", pTransfer->State.enmStatus), VERR_WRONG_ORDER);

    int rc = sharedClipboardURITransferThreadCreate(pTransfer, pfnThreadFunc, pvUser);

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
    SET_CALLBACK(pfnListHeaderComplete);
    SET_CALLBACK(pfnListEntryComplete);
    SET_CALLBACK(pfnTransferCanceled);
    SET_CALLBACK(pfnTransferError);
    SET_CALLBACK(pfnTransferStarted);

#undef SET_CALLBACK

    pTransfer->Callbacks.pvUser = pCallbacks->pvUser;
}

int SharedClipboardURITransferPayloadAlloc(uint32_t uID, const void *pvData, uint32_t cbData,
                                           PSHAREDCLIPBOARDURITRANSFERPAYLOAD *ppPayload)
{
    PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload =
        (PSHAREDCLIPBOARDURITRANSFERPAYLOAD)RTMemAlloc(sizeof(SHAREDCLIPBOARDURITRANSFERPAYLOAD));
    if (!pPayload)
        return VERR_NO_MEMORY;

    pPayload->pvData = RTMemAlloc(cbData);
    if (pPayload->pvData)
    {
        memcpy(pPayload->pvData, pvData, cbData);

        pPayload->cbData = cbData;
        pPayload->uID    = uID;

        *ppPayload = pPayload;

        return VINF_SUCCESS;
    }

    RTMemFree(pPayload);
    return VERR_NO_MEMORY;
}

void SharedClipboardURITransferPayloadFree(PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload)
{
    if (!pPayload)
        return;

    if (pPayload->pvData)
    {
        Assert(pPayload->cbData);
        RTMemFree(pPayload->pvData);
        pPayload->pvData = NULL;
    }

    pPayload->cbData = 0;

    RTMemFree(pPayload);
    pPayload = NULL;
}

int SharedClipboardURITransferEventRegister(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID)
{
    int rc;

    SharedClipboardURITransferEventMap::iterator itEvent = pTransfer->pMapEvents->find(uID);
    if (itEvent == pTransfer->pMapEvents->end())
    {
        PSHAREDCLIPBOARDURITRANSFEREVENT pEvent
            = (PSHAREDCLIPBOARDURITRANSFEREVENT)RTMemAllocZ(sizeof(SHAREDCLIPBOARDURITRANSFEREVENT));
        if (pEvent)
        {
            rc = RTSemEventCreate(&pEvent->hEventSem);
            if (RT_SUCCESS(rc))
            {
                pTransfer->pMapEvents->insert(std::pair<uint32_t, PSHAREDCLIPBOARDURITRANSFEREVENT>(uID, pEvent)); /** @todo Can this throw? */
            }
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VERR_ALREADY_EXISTS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferEventUnregister(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID)
{
    int rc;

    SharedClipboardURITransferEventMap::const_iterator itEvent = pTransfer->pMapEvents->find(uID);
    if (itEvent != pTransfer->pMapEvents->end())
    {
        SharedClipboardURITransferPayloadFree(itEvent->second->pPayload);

        RTSemEventDestroy(itEvent->second->hEventSem);

        RTMemFree(itEvent->second);

        pTransfer->pMapEvents->erase(itEvent);

        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferEventWait(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID, RTMSINTERVAL uTimeoutMs,
                                        PSHAREDCLIPBOARDURITRANSFERPAYLOAD *ppPayload)
{
    LogFlowFuncEnter();

    int rc;

    SharedClipboardURITransferEventMap::const_iterator itEvent = pTransfer->pMapEvents->find(uID);
    if (itEvent != pTransfer->pMapEvents->end())
    {
        rc = RTSemEventWait(itEvent->second->hEventSem, uTimeoutMs);
        if (RT_SUCCESS(rc))
        {
            *ppPayload = itEvent->second->pPayload;

            itEvent->second->pPayload = NULL;
        }
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferEventSignal(PSHAREDCLIPBOARDURITRANSFER pTransfer, uint32_t uID,
                                          PSHAREDCLIPBOARDURITRANSFERPAYLOAD pPayload)
{
    int rc;

    SharedClipboardURITransferEventMap::const_iterator itEvent = pTransfer->pMapEvents->find(uID);
    if (itEvent != pTransfer->pMapEvents->end())
    {
        Assert(itEvent->second->pPayload == NULL);

        itEvent->second->pPayload = pPayload;

        rc = RTSemEventSignal(itEvent->second->hEventSem);
    }
    else
        rc = VERR_NOT_FOUND;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Creates a thread for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to create thread for.
 * @param   pfnThreadFunc       Thread function to use for this transfer.
 * @param   pvUser              Pointer to user-provided data.
 */
static int sharedClipboardURITransferThreadCreate(PSHAREDCLIPBOARDURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser)

{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    /* Spawn a worker thread, so that we don't block the window thread for too long. */
    int rc = RTThreadCreate(&pTransfer->Thread.hThread, pfnThreadFunc,
                            pvUser, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE,
                            "shclp");
    if (RT_SUCCESS(rc))
    {
        int rc2 = RTThreadUserWait(pTransfer->Thread.hThread, 30 * 1000 /* Timeout in ms */);
        AssertRC(rc2);

        if (pTransfer->Thread.fStarted) /* Did the thread indicate that it started correctly? */
        {
            pTransfer->State.enmStatus = SHAREDCLIPBOARDURITRANSFERSTATUS_RUNNING;
        }
        else
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

    LogFlowFuncEnter();

    /* Set stop indicator. */
    pTransfer->Thread.fStop = true;

    int rcThread = VERR_WRONG_ORDER;
    int rc = RTThreadWait(pTransfer->Thread.hThread, uTimeoutMs, &rcThread);

    LogFlowFunc(("Waiting for thread resulted in %Rrc (thread exited with %Rrc)\n", rc, rcThread));

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

    LogFlowFunc(("%p\n", pURI));

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

    LogFlowFunc(("%p\n", pURI));

    RTCritSectDelete(&pURI->CritSect);

    PSHAREDCLIPBOARDURITRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pURI->List, pTransfer, pTransferNext, SHAREDCLIPBOARDURITRANSFER, Node)
    {
        SharedClipboardURITransferDestroy(pTransfer);

        RTListNodeRemove(&pTransfer->Node);

        RTMemFree(pTransfer);
        pTransfer = NULL;
    }

    pURI->cRunning   = 0;
    pURI->cTransfers = 0;
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

    pURI->cTransfers++;
    LogFlowFunc(("cTransfers=%RU32, cRunning=%RU32\n", pURI->cTransfers, pURI->cRunning));

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

        RTMemFree(pTransfer);
        pTransfer = NULL;
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
 * @returns Number of running transfers.
 * @param   pURI                URI clipboard context to return number for.
 */
uint32_t SharedClipboardURICtxGetRunningTransfers(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, 0);
    return pURI->cRunning;
}

/**
 * Returns the number of total URI transfers.
 *
 * @returns Number of total transfers.
 * @param   pURI                URI clipboard context to return number for.
 */
uint32_t SharedClipboardURICtxGetTotalTransfers(PSHAREDCLIPBOARDURICTX pURI)
{
    AssertPtrReturn(pURI, 0);
    return pURI->cTransfers;
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

            RTMemFree(pTransfer);
            pTransfer = NULL;

            Assert(pURI->cTransfers);
            pURI->cTransfers--;

            LogFlowFunc(("cTransfers=%RU32\n", pURI->cTransfers));
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

    LogFlowFunc(("cRunning=%RU32, cMaxRunning=%RU32\n", pURI->cRunning, pURI->cMaxRunning));

    Assert(pURI->cRunning <= pURI->cMaxRunning);
    return pURI->cRunning == pURI->cMaxRunning;
}

/**
 * Copies file system objinfo from IPRT to Shared Clipboard format.
 *
 * @param   pDst                The Shared Clipboard structure to convert data to.
 * @param   pSrc                The IPRT structure to convert data from.
 */
void SharedClipboardFsObjFromIPRT(PSHAREDCLIPBOARDFSOBJINFO pDst, PCRTFSOBJINFO pSrc)
{
    pDst->cbObject          = pSrc->cbObject;
    pDst->cbAllocated       = pSrc->cbAllocated;
    pDst->AccessTime        = pSrc->AccessTime;
    pDst->ModificationTime  = pSrc->ModificationTime;
    pDst->ChangeTime        = pSrc->ChangeTime;
    pDst->BirthTime         = pSrc->BirthTime;
    pDst->Attr.fMode        = pSrc->Attr.fMode;
    /* Clear bits which we don't pass through for security reasons. */
    pDst->Attr.fMode       &= ~(RTFS_UNIX_ISUID | RTFS_UNIX_ISGID | RTFS_UNIX_ISTXT);
    RT_ZERO(pDst->Attr.u);
    switch (pSrc->Attr.enmAdditional)
    {
        default:
        case RTFSOBJATTRADD_NOTHING:
            pDst->Attr.enmAdditional        = SHAREDCLIPBOARDFSOBJATTRADD_NOTHING;
            break;

        case RTFSOBJATTRADD_UNIX:
            pDst->Attr.enmAdditional        = SHAREDCLIPBOARDFSOBJATTRADD_UNIX;
            pDst->Attr.u.Unix.uid           = pSrc->Attr.u.Unix.uid;
            pDst->Attr.u.Unix.gid           = pSrc->Attr.u.Unix.gid;
            pDst->Attr.u.Unix.cHardlinks    = pSrc->Attr.u.Unix.cHardlinks;
            pDst->Attr.u.Unix.INodeIdDevice = pSrc->Attr.u.Unix.INodeIdDevice;
            pDst->Attr.u.Unix.INodeId       = pSrc->Attr.u.Unix.INodeId;
            pDst->Attr.u.Unix.fFlags        = pSrc->Attr.u.Unix.fFlags;
            pDst->Attr.u.Unix.GenerationId  = pSrc->Attr.u.Unix.GenerationId;
            pDst->Attr.u.Unix.Device        = pSrc->Attr.u.Unix.Device;
            break;

        case RTFSOBJATTRADD_EASIZE:
            pDst->Attr.enmAdditional        = SHAREDCLIPBOARDFSOBJATTRADD_EASIZE;
            pDst->Attr.u.EASize.cb          = pSrc->Attr.u.EASize.cb;
            break;
    }
}

