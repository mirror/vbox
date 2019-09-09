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
#include <iprt/list.h>
#include <iprt/path.h>
#include <iprt/semaphore.h>

#include <VBox/err.h>
#include <VBox/HostServices/VBoxClipboardSvc.h>
#include <VBox/GuestHost/SharedClipboard-uri.h>


static int sharedClipboardURITransferThreadCreate(PSHCLURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser);
static int sharedClipboardURITransferThreadDestroy(PSHCLURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs);
static PSHCLURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHCLURICTX pURI, uint32_t uIdx);
static int sharedClipboardConvertFileCreateFlags(bool fWritable, unsigned fShClFlags, RTFMODE fMode,
                                                 SHCLOBJHANDLE handleInitial, uint64_t *pfOpen);

/** @todo Split this file up in different modules. */

/**
 * Allocates a new URI root list.
 *
 * @returns Allocated URI root list on success, or NULL on failure.
 */
PSHCLROOTLIST SharedClipboardURIRootListAlloc(void)
{
    PSHCLROOTLIST pRootList = (PSHCLROOTLIST)RTMemAllocZ(sizeof(SHCLROOTLIST));

    return pRootList;
}

/**
 * Frees an URI root list.
 *
 * @param   pRootList           URI root list to free. The pointer will be
 *                              invalid after returning from this function.
 */
void SharedClipboardURIRootListFree(PSHCLROOTLIST pRootList)
{
    if (!pRootList)
        return;

    for (uint32_t i = 0; i < pRootList->Hdr.cRoots; i++)
        SharedClipboardURIListEntryDestroy(&pRootList->paEntries[i]);

    RTMemFree(pRootList);
    pRootList = NULL;
}

/**
 * Initializes an URI root list header.
 *
 * @returns VBox status code.
 * @param   pRootLstHdr         Root list header to initialize.
 */
int SharedClipboardURIRootListHdrInit(PSHCLROOTLISTHDR pRootLstHdr)
{
    AssertPtrReturn(pRootLstHdr, VERR_INVALID_POINTER);

    RT_BZERO(pRootLstHdr, sizeof(SHCLROOTLISTHDR));

    return VINF_SUCCESS;
}

/**
 * Destroys an URI root list header.
 *
 * @param   pRootLstHdr         Root list header to destroy.
 */
void SharedClipboardURIRootListHdrDestroy(PSHCLROOTLISTHDR pRootLstHdr)
{
    if (!pRootLstHdr)
        return;

    pRootLstHdr->fRoots = 0;
    pRootLstHdr->cRoots = 0;
}

/**
 * Duplicates an URI list header.
 *
 * @returns Duplicated URI list header on success, or NULL on failure.
 * @param   pRootLstHdr         Root list header to duplicate.
 */
PSHCLROOTLISTHDR SharedClipboardURIRootListHdrDup(PSHCLROOTLISTHDR pRootLstHdr)
{
    AssertPtrReturn(pRootLstHdr, NULL);

    int rc = VINF_SUCCESS;

    PSHCLROOTLISTHDR pRootsDup = (PSHCLROOTLISTHDR)RTMemAllocZ(sizeof(SHCLROOTLISTHDR));
    if (pRootsDup)
    {
        *pRootsDup = *pRootLstHdr;
    }
    else
        rc = VERR_NO_MEMORY;

    if (RT_FAILURE(rc))
    {
        SharedClipboardURIRootListHdrDestroy(pRootsDup);
        pRootsDup = NULL;
    }

    return pRootsDup;
}

/**
 * (Deep) Copies a clipboard root list entry structure.
 *
 * @returns VBox status code.
 * @param   pListEntry          Clipboard root list entry to copy.
 */
int SharedClipboardURIRootListEntryCopy(PSHCLROOTLISTENTRY pDst, PSHCLROOTLISTENTRY pSrc)
{
    return SharedClipboardURIListEntryCopy(pDst, pSrc);
}

/**
 * Duplicates (allocates) a clipboard root list entry structure.
 *
 * @returns Duplicated clipboard root list entry structure on success.
 * @param   pListEntry          Clipboard root list entry to duplicate.
 */
PSHCLROOTLISTENTRY SharedClipboardURIRootListEntryDup(PSHCLROOTLISTENTRY pRootListEntry)
{
    return SharedClipboardURIListEntryDup(pRootListEntry);
}

/**
 * Destroys a clipboard root list entry structure.
 *
 * @param   pListEntry          Clipboard root list entry structure to destroy.
 */
void SharedClipboardURIRootListEntryDestroy(PSHCLROOTLISTENTRY pRootListEntry)
{
    return SharedClipboardURIListEntryDestroy(pRootListEntry);
}

/**
 * Destroys a list handle info structure.
 *
 * @param   pInfo               List handle info structure to destroy.
 */
void SharedClipboardURIListHandleInfoDestroy(PSHCLURILISTHANDLEINFO pInfo)
{
    if (!pInfo)
        return;

    if (pInfo->pszPathLocalAbs)
    {
        RTStrFree(pInfo->pszPathLocalAbs);
        pInfo->pszPathLocalAbs = NULL;
    }
}

/**
 * Allocates a URI list header structure.
 *
 * @returns VBox status code.
 * @param   ppListHdr           Where to store the allocated URI list header structure on success.
 */
int SharedClipboardURIListHdrAlloc(PSHCLLISTHDR *ppListHdr)
{
    int rc;

    PSHCLLISTHDR pListHdr = (PSHCLLISTHDR)RTMemAllocZ(sizeof(SHCLLISTHDR));
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
 * Frees an URI list header structure.
 *
 * @param   pListEntry          URI list header structure to free.
 */
void SharedClipboardURIListHdrFree(PSHCLLISTHDR pListHdr)
{
    if (!pListHdr)
        return;

    LogFlowFuncEnter();

    SharedClipboardURIListHdrDestroy(pListHdr);

    RTMemFree(pListHdr);
    pListHdr = NULL;
}

/**
 * Duplicates (allocates) an URI list header structure.
 *
 * @returns Duplicated URI list header structure on success.
 * @param   pListHdr            URI list header to duplicate.
 */
PSHCLLISTHDR SharedClipboardURIListHdrDup(PSHCLLISTHDR pListHdr)
{
    AssertPtrReturn(pListHdr, NULL);

    PSHCLLISTHDR pListHdrDup = (PSHCLLISTHDR)RTMemAlloc(sizeof(SHCLLISTHDR));
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
int SharedClipboardURIListHdrInit(PSHCLLISTHDR pListHdr)
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
void SharedClipboardURIListHdrDestroy(PSHCLLISTHDR pListHdr)
{
    if (!pListHdr)
        return;

    LogFlowFuncEnter();
}

/**
 * Resets a SHCLListHdr structture.
 *
 * @returns VBox status code.
 * @param   pListHdr            SHCLListHdr structture to reset.
 */
void SharedClipboardURIListHdrReset(PSHCLLISTHDR pListHdr)
{
    AssertPtrReturnVoid(pListHdr);

    LogFlowFuncEnter();

    RT_BZERO(pListHdr, sizeof(SHCLLISTHDR));
}

/**
 * Returns whether a given clipboard data header is valid or not.
 *
 * @returns \c true if valid, \c false if not.
 * @param   pListHdr            Clipboard data header to validate.
 */
bool SharedClipboardURIListHdrIsValid(PSHCLLISTHDR pListHdr)
{
    RT_NOREF(pListHdr);
    return true; /** @todo Implement this. */
}

int SharedClipboardURIListOpenParmsCopy(PSHCLLISTOPENPARMS pDst, PSHCLLISTOPENPARMS pSrc)
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

/**
 * Duplicates an URI list open parameters structure.
 *
 * @returns Duplicated URI list open parameters structure on success, or NULL on failure.
 * @param   pParms              URI list open parameters structure to duplicate.
 */
PSHCLLISTOPENPARMS SharedClipboardURIListOpenParmsDup(PSHCLLISTOPENPARMS pParms)
{
    AssertPtrReturn(pParms, NULL);

    PSHCLLISTOPENPARMS pParmsDup = (PSHCLLISTOPENPARMS)RTMemAllocZ(sizeof(SHCLLISTOPENPARMS));
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

/**
 * Initializes an URI list open parameters structure.
 *
 * @returns VBox status code.
 * @param   pParms              URI list open parameters structure to initialize.
 */
int SharedClipboardURIListOpenParmsInit(PSHCLLISTOPENPARMS pParms)
{
    AssertPtrReturn(pParms, VERR_INVALID_POINTER);

    RT_BZERO(pParms, sizeof(SHCLLISTOPENPARMS));

    pParms->cbFilter  = 64; /** @todo Make this dynamic. */
    pParms->pszFilter = RTStrAlloc(pParms->cbFilter);

    pParms->cbPath    = RTPATH_MAX;
    pParms->pszPath   = RTStrAlloc(pParms->cbPath);

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

/**
 * Destroys an URI list open parameters structure.
 *
 * @param   pParms              URI list open parameters structure to destroy.
 */
void SharedClipboardURIListOpenParmsDestroy(PSHCLLISTOPENPARMS pParms)
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
int SharedClipboardURIListEntryAlloc(PSHCLLISTENTRY *ppListEntry)
{
    PSHCLLISTENTRY pListEntry = (PSHCLLISTENTRY)RTMemAlloc(sizeof(SHCLLISTENTRY));
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
void SharedClipboardURIListEntryFree(PSHCLLISTENTRY pListEntry)
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
int SharedClipboardURIListEntryCopy(PSHCLLISTENTRY pDst, PSHCLLISTENTRY pSrc)
{
    AssertPtrReturn(pDst, VERR_INVALID_POINTER);
    AssertPtrReturn(pSrc, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    *pDst = *pSrc;

    if (pSrc->pszName)
    {
        pDst->pszName = RTStrDup(pSrc->pszName);
        if (!pDst->pszName)
            rc = VERR_NO_MEMORY;
    }

    if (   RT_SUCCESS(rc)
        && pSrc->pvInfo)
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
PSHCLLISTENTRY SharedClipboardURIListEntryDup(PSHCLLISTENTRY pListEntry)
{
    AssertPtrReturn(pListEntry, NULL);

    int rc = VINF_SUCCESS;

    PSHCLLISTENTRY pListEntryDup = (PSHCLLISTENTRY)RTMemAllocZ(sizeof(SHCLLISTENTRY));
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
 * @returns VBox status code.
 * @param   pListEntry          Clipboard list entry structure to initialize.
 */
int SharedClipboardURIListEntryInit(PSHCLLISTENTRY pListEntry)
{
    RT_BZERO(pListEntry, sizeof(SHCLLISTENTRY));

    pListEntry->pszName = RTStrAlloc(SHCLLISTENTRY_MAX_NAME);
    if (!pListEntry->pszName)
        return VERR_NO_MEMORY;

    pListEntry->cbName = SHCLLISTENTRY_MAX_NAME;
    pListEntry->pvInfo = NULL;
    pListEntry->cbInfo = 0;
    pListEntry->fInfo  = 0;

    return VINF_SUCCESS;
}

/**
 * Destroys a clipboard list entry structure.
 *
 * @param   pListEntry          Clipboard list entry structure to destroy.
 */
void SharedClipboardURIListEntryDestroy(PSHCLLISTENTRY pListEntry)
{
    if (!pListEntry)
        return;

    if (pListEntry->pszName)
    {
        RTStrFree(pListEntry->pszName);

        pListEntry->pszName = NULL;
        pListEntry->cbName  = 0;
    }

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
bool SharedClipboardURIListEntryIsValid(PSHCLLISTENTRY pListEntry)
{
    RT_NOREF(pListEntry);

    /** @todo Verify checksum. */

    return true; /** @todo Implement this. */
}

/**
 * Initializes an URI object context.
 *
 * @returns VBox status code.
 * @param   pObjCtx             URI object context to initialize.
 */
int SharedClipboardURIObjCtxInit(PSHCLCLIENTURIOBJCTX pObjCtx)
{
    AssertPtrReturn(pObjCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    pObjCtx->uHandle  = SHCLOBJHANDLE_INVALID;

    return VINF_SUCCESS;
}

/**
 * Destroys an URI object context.
 *
 * @param   pObjCtx             URI object context to destroy.
 */
void SharedClipboardURIObjCtxDestroy(PSHCLCLIENTURIOBJCTX pObjCtx)
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
bool SharedClipboardURIObjCtxIsValid(PSHCLCLIENTURIOBJCTX pObjCtx)
{
    return (   pObjCtx
            && pObjCtx->uHandle != SHCLOBJHANDLE_INVALID);
}

/**
 * Destroys an object handle info structure.
 *
 * @param   pInfo               Object handle info structure to destroy.
 */
void SharedClipboardURIObjectHandleInfoDestroy(PSHCLURIOBJHANDLEINFO pInfo)
{
    if (!pInfo)
        return;

    if (pInfo->pszPathLocalAbs)
    {
        RTStrFree(pInfo->pszPathLocalAbs);
        pInfo->pszPathLocalAbs = NULL;
    }
}

/**
 * Initializes an URI object open parameters structure.
 *
 * @returns VBox status code.
 * @param   pParms              URI object open parameters structure to initialize.
 */
int SharedClipboardURIObjectOpenParmsInit(PSHCLOBJOPENCREATEPARMS pParms)
{
    AssertPtrReturn(pParms, VERR_INVALID_POINTER);

    int rc;

    RT_BZERO(pParms, sizeof(SHCLOBJOPENCREATEPARMS));

    pParms->cbPath    = RTPATH_MAX; /** @todo Make this dynamic. */
    pParms->pszPath   = RTStrAlloc(pParms->cbPath);
    if (pParms->pszPath)
    {
        rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NO_MEMORY;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Copies an URI object open parameters structure from source to destination.
 *
 * @returns VBox status code.
 * @param   pParmsDst           Where to copy the source URI object open parameters to.
 * @param   pParmsSrc           Which source URI object open parameters to copy.
 */
int SharedClipboardURIObjectOpenParmsCopy(PSHCLOBJOPENCREATEPARMS pParmsDst, PSHCLOBJOPENCREATEPARMS pParmsSrc)
{
    int rc;

    *pParmsDst = *pParmsSrc;

    if (pParmsSrc->pszPath)
    {
        Assert(pParmsSrc->cbPath);
        pParmsDst->pszPath = RTStrDup(pParmsSrc->pszPath);
        if (pParmsDst->pszPath)
        {
            rc = VINF_SUCCESS;
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else
        rc = VINF_SUCCESS;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Destroys an URI object open parameters structure.
 *
 * @param   pParms              URI object open parameters structure to destroy.
 */
void SharedClipboardURIObjectOpenParmsDestroy(PSHCLOBJOPENCREATEPARMS pParms)
{
    if (!pParms)
        return;

    if (pParms->pszPath)
    {
        RTStrFree(pParms->pszPath);
        pParms->pszPath = NULL;
    }
}

/**
 * Returns a specific object handle info of a transfer.
 *
 * @returns Pointer to object handle info if found, or NULL if not found.
 * @param   pTransfer           URI clipboard transfer to get object handle info from.
 * @param   hObj                Object handle of the object to get handle info for.
 */
inline PSHCLURIOBJHANDLEINFO sharedClipboardURIObjectGet(PSHCLURITRANSFER pTransfer,
                                                                    SHCLOBJHANDLE hObj)
{
    PSHCLURIOBJHANDLEINFO pIt;
    RTListForEach(&pTransfer->lstObj, pIt, SHCLURIOBJHANDLEINFO, Node)
    {
        if (pIt->hObj == hObj)
            return pIt;
    }

    return NULL;
}

/**
 * Opens an URI object.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to open the object for.
 * @param   pOpenCreateParms    Open / create parameters of URI object to open / create.
 * @param   phObj               Where to store the handle of URI object opened on success.
 */
int SharedClipboardURIObjectOpen(PSHCLURITRANSFER pTransfer, PSHCLOBJOPENCREATEPARMS pOpenCreateParms,
                                 PSHCLOBJHANDLE phObj)
{
    AssertPtrReturn(pTransfer,        VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenCreateParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phObj,            VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    LogFlowFunc(("pszPath=%s, fCreate=0x%x\n", pOpenCreateParms->pszPath, pOpenCreateParms->fCreate));

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURIOBJHANDLEINFO pInfo
            = (PSHCLURIOBJHANDLEINFO)RTMemAlloc(sizeof(SHCLURIOBJHANDLEINFO));
        if (pInfo)
        {
            const bool fWritable = true; /** @todo Fix this. */

            uint64_t fOpen;
            rc = sharedClipboardConvertFileCreateFlags(fWritable,
                                                       pOpenCreateParms->fCreate, pOpenCreateParms->ObjInfo.Attr.fMode,
                                                       SHCLOBJHANDLE_INVALID, &fOpen);
            if (RT_SUCCESS(rc))
            {
                char *pszPathAbs = RTStrAPrintf2("%s/%s", pTransfer->pszPathRootAbs, pOpenCreateParms->pszPath);
                if (pszPathAbs)
                {
                    LogFlowFunc(("%s\n", pszPathAbs));

                    rc = RTFileOpen(&pInfo->u.Local.hFile, pszPathAbs, fOpen);
                    RTStrFree(pszPathAbs);
                }
                else
                    rc = VERR_NO_MEMORY;
            }

            if (RT_SUCCESS(rc))
            {
                pInfo->hObj    = pTransfer->uObjHandleNext++;
                pInfo->enmType = SHCLURIOBJTYPE_FILE;

                RTListAppend(&pTransfer->lstObj, &pInfo->Node);

                *phObj = pInfo->hObj;
            }

            if (RT_FAILURE(rc))
                RTMemFree(pInfo);
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnObjOpen)
        {
            rc = pTransfer->ProviderIface.pfnObjOpen(&pTransfer->ProviderCtx, pOpenCreateParms, phObj);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Closes an URI object.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer that contains the object to close.
 * @param   hObj                Handle of URI object to close.
 */
int SharedClipboardURIObjectClose(PSHCLURITRANSFER pTransfer, SHCLOBJHANDLE hObj)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURIOBJHANDLEINFO pInfo = sharedClipboardURIObjectGet(pTransfer, hObj);
        if (pInfo)
        {
            switch (pInfo->enmType)
            {
                case SHCLURIOBJTYPE_DIRECTORY:
                {
                    rc = RTDirClose(pInfo->u.Local.hDir);
                    if (RT_SUCCESS(rc))
                        pInfo->u.Local.hDir = NIL_RTDIR;
                    break;
                }

                case SHCLURIOBJTYPE_FILE:
                {
                    rc = RTFileClose(pInfo->u.Local.hFile);
                    if (RT_SUCCESS(rc))
                        pInfo->u.Local.hFile = NIL_RTFILE;
                    break;
                }

                default:
                    rc = VERR_NOT_IMPLEMENTED;
                    break;
            }

            RTMemFree(pInfo);

            RTListNodeRemove(&pInfo->Node);
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnObjClose)
        {
            rc = pTransfer->ProviderIface.pfnObjClose(&pTransfer->ProviderCtx, hObj);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Reads from an URI object.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer that contains the object to read from.
 * @param   hObj                Handle of URI object to read from.
 * @param   pvBuf               Buffer for where to store the read data.
 * @param   cbBuf               Size (in bytes) of buffer.
 * @param   pcbRead             How much bytes were read on success. Optional.
 */
int SharedClipboardURIObjectRead(PSHCLURITRANSFER pTransfer,
                                 SHCLOBJHANDLE hObj, void *pvBuf, uint32_t cbBuf, uint32_t *pcbRead, uint32_t fFlags)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,     VERR_INVALID_POINTER);
    AssertReturn   (cbBuf,     VERR_INVALID_PARAMETER);
    /* pcbRead is optional. */
    /** @todo Validate fFlags. */

    int rc = VINF_SUCCESS;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURIOBJHANDLEINFO pInfo = sharedClipboardURIObjectGet(pTransfer, hObj);
        if (pInfo)
        {
            switch (pInfo->enmType)
            {
                case SHCLURIOBJTYPE_FILE:
                {
                    size_t cbRead;
                    rc = RTFileRead(pInfo->u.Local.hFile, pvBuf, cbBuf, &cbRead);
                    if (RT_SUCCESS(rc))
                    {
                        if (pcbRead)
                            *pcbRead = (uint32_t)cbRead;
                    }
                    break;
                }

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnObjRead)
        {
            rc = pTransfer->ProviderIface.pfnObjRead(&pTransfer->ProviderCtx, hObj, pvBuf, cbBuf, fFlags, pcbRead);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Writes to an URI object.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer that contains the object to write to.
 * @param   hObj                Handle of URI object to write to.
 * @param   pvBuf               Buffer of data to write.
 * @param   cbBuf               Size (in bytes) of buffer to write.
 * @param   pcbWritten          How much bytes were writtenon success. Optional.
 */
int SharedClipboardURIObjectWrite(PSHCLURITRANSFER pTransfer,
                                  SHCLOBJHANDLE hObj, void *pvBuf, uint32_t cbBuf, uint32_t *pcbWritten,
                                  uint32_t fFlags)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pvBuf,     VERR_INVALID_POINTER);
    AssertReturn   (cbBuf,     VERR_INVALID_PARAMETER);
    /* pcbWritten is optional. */

    int rc = VINF_SUCCESS;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURIOBJHANDLEINFO pInfo = sharedClipboardURIObjectGet(pTransfer, hObj);
        if (pInfo)
        {
            switch (pInfo->enmType)
            {
                case SHCLURIOBJTYPE_FILE:
                {
                    rc = RTFileWrite(pInfo->u.Local.hFile, pvBuf, cbBuf, (size_t *)pcbWritten);
                    break;
                }

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnObjWrite)
        {
            rc = pTransfer->ProviderIface.pfnObjWrite(&pTransfer->ProviderCtx, hObj, pvBuf, cbBuf, fFlags, pcbWritten);
        }
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Duplicaates an URI object data chunk.
 *
 * @returns Duplicated object data chunk on success, or NULL on failure.
 * @param   pDataChunk          URI object data chunk to duplicate.
 */
PSHCLOBJDATACHUNK SharedClipboardURIObjectDataChunkDup(PSHCLOBJDATACHUNK pDataChunk)
{
    if (!pDataChunk)
        return NULL;

    PSHCLOBJDATACHUNK pDataChunkDup = (PSHCLOBJDATACHUNK)RTMemAllocZ(sizeof(SHCLOBJDATACHUNK));
    if (!pDataChunkDup)
        return NULL;

    if (pDataChunk->pvData)
    {
        Assert(pDataChunk->cbData);

        pDataChunkDup->uHandle = pDataChunk->uHandle;
        pDataChunkDup->pvData  = RTMemDup(pDataChunk->pvData, pDataChunk->cbData);
        pDataChunkDup->cbData  = pDataChunk->cbData;
    }

    return pDataChunkDup;
}

/**
 * Destroys an URI object data chunk.
 *
 * @param   pDataChunk          URI object data chunk to destroy.
 */
void SharedClipboardURIObjectDataChunkDestroy(PSHCLOBJDATACHUNK pDataChunk)
{
    if (!pDataChunk)
        return;

    if (pDataChunk->pvData)
    {
        Assert(pDataChunk->cbData);

        RTMemFree(pDataChunk->pvData);

        pDataChunk->pvData = NULL;
        pDataChunk->cbData = 0;
    }

    pDataChunk->uHandle = 0;
}

/**
 * Frees an URI object data chunk.
 *
 * @param   pDataChunk          URI object data chunk to free. The handed-in pointer will
 *                              be invalid after calling this function.
 */
void SharedClipboardURIObjectDataChunkFree(PSHCLOBJDATACHUNK pDataChunk)
{
    if (!pDataChunk)
        return;

    SharedClipboardURIObjectDataChunkDestroy(pDataChunk);

    RTMemFree(pDataChunk);
    pDataChunk = NULL;
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
int SharedClipboardURITransferCreate(SHCLURITRANSFERDIR enmDir, SHCLSOURCE enmSource,
                                     PSHCLURITRANSFER *ppTransfer)
{
    AssertPtrReturn(ppTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    PSHCLURITRANSFER pTransfer = (PSHCLURITRANSFER)RTMemAlloc(sizeof(SHCLURITRANSFER));
    if (!pTransfer)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    pTransfer->State.uID       = 0;
    pTransfer->State.enmStatus = SHCLURITRANSFERSTATUS_NONE;
    pTransfer->State.enmDir    = enmDir;
    pTransfer->State.enmSource = enmSource;

    LogFlowFunc(("enmDir=%RU32, enmSource=%RU32\n", pTransfer->State.enmDir, pTransfer->State.enmSource));

    pTransfer->pArea = NULL; /* Will be created later if needed. */

    pTransfer->Thread.hThread    = NIL_RTTHREAD;
    pTransfer->Thread.fCancelled = false;
    pTransfer->Thread.fStarted   = false;
    pTransfer->Thread.fStop      = false;

    pTransfer->pszPathRootAbs    = NULL;

    pTransfer->uListHandleNext   = 1;
    pTransfer->uObjHandleNext    = 1;

    pTransfer->uTimeoutMs     = 30 * 1000; /* 30s timeout by default. */
    pTransfer->cbMaxChunkSize = _64K; /** @todo Make this configurable. */

    pTransfer->pvUser = NULL;
    pTransfer->cbUser = 0;

    RT_ZERO(pTransfer->Callbacks);

    RTListInit(&pTransfer->lstList);
    RTListInit(&pTransfer->lstObj);

    pTransfer->cRoots = 0;
    RTListInit(&pTransfer->lstRoots);

    *ppTransfer = pTransfer;

    if (RT_FAILURE(rc))
    {
        if (pTransfer)
        {
            SharedClipboardURITransferDestroy(pTransfer);
            RTMemFree(pTransfer);
        }
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
int SharedClipboardURITransferDestroy(PSHCLURITRANSFER pTransfer)
{
    if (!pTransfer)
        return VINF_SUCCESS;

    LogFlowFuncEnter();

    int rc = sharedClipboardURITransferThreadDestroy(pTransfer, 30 * 1000 /* Timeout in ms */);
    if (RT_FAILURE(rc))
        return rc;

    RTStrFree(pTransfer->pszPathRootAbs);

    SharedClipboardEventSourceDestroy(&pTransfer->Events);

    PSHCLURILISTHANDLEINFO pItList, pItListNext;
    RTListForEachSafe(&pTransfer->lstList, pItList, pItListNext, SHCLURILISTHANDLEINFO, Node)
    {
        SharedClipboardURIListHandleInfoDestroy(pItList);

        RTListNodeRemove(&pItList->Node);

        RTMemFree(pItList);
    }

    PSHCLURIOBJHANDLEINFO pItObj, pItObjNext;
    RTListForEachSafe(&pTransfer->lstObj, pItObj, pItObjNext, SHCLURIOBJHANDLEINFO, Node)
    {
        SharedClipboardURIObjectHandleInfoDestroy(pItObj);

        RTListNodeRemove(&pItObj->Node);

        RTMemFree(pItObj);
    }

    LogFlowFuncLeave();
    return VINF_SUCCESS;
}

int SharedClipboardURITransferOpen(PSHCLURITRANSFER pTransfer)
{
    int rc = VINF_SUCCESS;

    if (pTransfer->ProviderIface.pfnTransferOpen)
        rc = pTransfer->ProviderIface.pfnTransferOpen(&pTransfer->ProviderCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferClose(PSHCLURITRANSFER pTransfer)
{
    int rc = VINF_SUCCESS;

    if (pTransfer->ProviderIface.pfnTransferClose)
        rc = pTransfer->ProviderIface.pfnTransferClose(&pTransfer->ProviderCtx);

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns a specific list handle info of a transfer.
 *
 * @returns Pointer to list handle info if found, or NULL if not found.
 * @param   pTransfer           URI clipboard transfer to get list handle info from.
 * @param   hList               List handle of the list to get handle info for.
 */
inline PSHCLURILISTHANDLEINFO sharedClipboardURITransferListGet(PSHCLURITRANSFER pTransfer,
                                                                           SHCLLISTHANDLE hList)
{
    PSHCLURILISTHANDLEINFO pIt;
    RTListForEach(&pTransfer->lstList, pIt, SHCLURILISTHANDLEINFO, Node)
    {
        if (pIt->hList == hList)
            return pIt;
    }

    return NULL;
}

/**
 * Creates a new list handle (local only).
 *
 * @returns New List handle on success, or SHCLLISTHANDLE_INVALID on error.
 * @param   pTransfer           URI clipboard transfer to create new list handle for.
 */
inline SHCLLISTHANDLE sharedClipboardURITransferListHandleNew(PSHCLURITRANSFER pTransfer)
{
    return pTransfer->uListHandleNext++; /** @todo Good enough for now. Improve this later. */
}

/**
 * Opens a list.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to handle.
 * @param   pOpenParms          List open parameters to use for opening.
 * @param   phList              Where to store the List handle of opened list on success.
 */
int SharedClipboardURITransferListOpen(PSHCLURITRANSFER pTransfer, PSHCLLISTOPENPARMS pOpenParms,
                                       PSHCLLISTHANDLE phList)
{
    AssertPtrReturn(pTransfer,  VERR_INVALID_POINTER);
    AssertPtrReturn(pOpenParms, VERR_INVALID_POINTER);
    AssertPtrReturn(phList,     VERR_INVALID_POINTER);

    int rc;

    SHCLLISTHANDLE hList = SHCLLISTHANDLE_INVALID;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURILISTHANDLEINFO pInfo
            = (PSHCLURILISTHANDLEINFO)RTMemAlloc(sizeof(SHCLURILISTHANDLEINFO));
        if (pInfo)
        {
            LogFlowFunc(("pszPath=%s\n", pOpenParms->pszPath));

            RTFSOBJINFO objInfo;
            rc = RTPathQueryInfo(pOpenParms->pszPath, &objInfo, RTFSOBJATTRADD_NOTHING);
            if (RT_SUCCESS(rc))
            {
                switch (pInfo->enmType)
                {
                    case SHCLURIOBJTYPE_DIRECTORY:
                    {
                        rc = RTDirOpen(&pInfo->u.Local.hDir, pOpenParms->pszPath);
                        break;
                    }

                    case SHCLURIOBJTYPE_FILE:
                    {
                        rc = RTFileOpen(&pInfo->u.Local.hFile, pOpenParms->pszPath,
                                        RTFILE_O_OPEN | RTFILE_O_READ | RTFILE_O_DENY_WRITE);
                        break;
                    }

                    default:
                        rc = VERR_NOT_SUPPORTED;
                        break;
                }

                if (RT_SUCCESS(rc))
                {
                    pInfo->hList = sharedClipboardURITransferListHandleNew(pTransfer);

                    RTListAppend(&pTransfer->lstList, &pInfo->Node);
                }
                else
                {
                    if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
                    {
                        if (RTDirIsValid(pInfo->u.Local.hDir))
                            RTDirClose(pInfo->u.Local.hDir);
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
        }
        else
            rc = VERR_NO_MEMORY;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
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

/**
 * Closes a list.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to handle.
 * @param   hList               Handle of list to close.
 */
int SharedClipboardURITransferListClose(PSHCLURITRANSFER pTransfer, SHCLLISTHANDLE hList)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    if (hList == SHCLLISTHANDLE_INVALID)
        return VINF_SUCCESS;

    int rc = VINF_SUCCESS;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURILISTHANDLEINFO pInfo = sharedClipboardURITransferListGet(pTransfer, hList);
        if (pInfo)
        {
            switch (pInfo->enmType)
            {
                case SHCLURIOBJTYPE_DIRECTORY:
                {
                    if (RTDirIsValid(pInfo->u.Local.hDir))
                        RTDirClose(pInfo->u.Local.hDir);
                    break;
                }

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }

            RTListNodeRemove(&pInfo->Node);

            RTMemFree(pInfo);
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
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

/**
 * Adds a file to a list heaer.
 *
 * @returns VBox status code.
 * @param   pHdr                List header to add file to.
 * @param   pszPath             Path of file to add.
 */
static int sharedClipboardURITransferListHdrAddFile(PSHCLLISTHDR pHdr, const char *pszPath)
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

/**
 * Builds a list header, internal version.
 *
 * @returns VBox status code.
 * @param   pHdr                Where to store the build list header.
 * @param   pcszSrcPath         Source path of list.
 * @param   pcszDstPath         Destination path of list.
 * @param   pcszDstBase         Destination base path.
 * @param   cchDstBase          Number of charaters of destination base path.
 */
static int sharedClipboardURITransferListHdrFromDir(PSHCLLISTHDR pHdr,
                                                    const char *pcszSrcPath, const char *pcszDstPath,
                                                    const char *pcszDstBase)
{
    AssertPtrReturn(pcszSrcPath, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDstBase, VERR_INVALID_POINTER);
    AssertPtrReturn(pcszDstPath, VERR_INVALID_POINTER);

    LogFlowFunc(("pcszSrcPath=%s, pcszDstPath=%s, pcszDstBase=%s\n",
                 pcszSrcPath, pcszDstPath, pcszDstBase));

    RTFSOBJINFO objInfo;
    int rc = RTPathQueryInfo(pcszSrcPath, &objInfo, RTFSOBJATTRADD_NOTHING);
    if (RT_SUCCESS(rc))
    {
        if (RTFS_IS_DIRECTORY(objInfo.Attr.fMode))
        {
            pHdr->cTotalObjects++;

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
                #if 0 /* No recursion here (yet). */
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
                #endif
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

/**
 * Translates an absolute path to a relative one.
 *
 * @returns Translated, allocated path on success, or NULL on failure.
 *          Must be free'd with RTStrFree().
 * @param   pszPath             Absolute path to translate.
 */
static char *sharedClipboardPathTranslate(const char *pszPath)
{
    char *pszPathTranslated = NULL;

    char *pszSrcPath = RTStrDup(pszPath);
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

                pszPathTranslated = RTStrDup(&pszSrcPath[cchDstBase]);

                LogFlowFunc(("pszSrcPath=%s, pszFileName=%s -> pszPathTranslated=%s\n",
                             pszSrcPath, pszFileName, pszPathTranslated));
            }
        }

        RTStrFree(pszSrcPath);
    }

    return pszPathTranslated;
}

/**
 * Retrieves the header of a Shared Clipboard list.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to handle.
 * @param   hList               Handle of list to get header for.
 * @param   pHdr                Where to store the returned list header information.
 */
int SharedClipboardURITransferListGetHeader(PSHCLURITRANSFER pTransfer, SHCLLISTHANDLE hList,
                                            PSHCLLISTHDR pHdr)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pHdr,      VERR_INVALID_POINTER);

    int rc;

    LogFlowFunc(("hList=%RU64\n", hList));

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURILISTHANDLEINFO pInfo = sharedClipboardURITransferListGet(pTransfer, hList);
        if (pInfo)
        {
            rc = SharedClipboardURIListHdrInit(pHdr);
            if (RT_SUCCESS(rc))
            {
                switch (pInfo->enmType)
                {
                    case SHCLURIOBJTYPE_DIRECTORY:
                    {
                        char *pszPathRel = sharedClipboardPathTranslate(pInfo->pszPathLocalAbs);
                        if (pszPathRel)
                        {
                            rc = sharedClipboardURITransferListHdrFromDir(pHdr,
                                                                          pszPathRel, pszPathRel, pszPathRel);
                            RTStrFree(pszPathRel);
                        }
                        else
                            rc = VERR_NO_MEMORY;
                        break;
                    }

                    case SHCLURIOBJTYPE_FILE:
                    {
                        pHdr->cTotalObjects = 1;

                        RTFSOBJINFO objInfo;
                        rc = RTFileQueryInfo(pInfo->u.Local.hFile, &objInfo, RTFSOBJATTRADD_NOTHING);
                        if (RT_SUCCESS(rc))
                        {
                            pHdr->cbTotalSize = objInfo.cbObject;
                        }
                        break;
                    }

                    default:
                        rc = VERR_NOT_SUPPORTED;
                        break;
                }
            }

            LogFlowFunc(("cTotalObj=%RU64, cbTotalSize=%RU64\n", pHdr->cTotalObjects, pHdr->cbTotalSize));
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
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
 * Currently not implemented and wil return NULL.
 *
 * @returns Pointer to URI object, or NULL if not found / invalid.
 * @param   pTransfer           URI clipboard transfer to return URI object for.
 * @param   hList               Handle of URI transfer list to get object for.
 * @param   uIdx                Index of object to get.
 */
PSHCLURITRANSFEROBJ SharedClipboardURITransferListGetObj(PSHCLURITRANSFER pTransfer,
                                                                    SHCLLISTHANDLE hList, uint64_t uIdx)
{
    AssertPtrReturn(pTransfer, NULL);

    RT_NOREF(hList, uIdx);

    LogFlowFunc(("hList=%RU64\n", hList));

    return NULL;
}

/**
 * Reads a single Shared Clipboard list entry.
 *
 * @returns VBox status code or VERR_NO_MORE_FILES if the end of the list has been reached.
 * @param   pTransfer           URI clipboard transfer to handle.
 * @param   hList               List handle of list to read from.
 * @param   pEntry              Where to store the read information.
 */
int SharedClipboardURITransferListRead(PSHCLURITRANSFER pTransfer, SHCLLISTHANDLE hList,
                                       PSHCLLISTENTRY pEntry)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pEntry,    VERR_INVALID_POINTER);

    int rc = VINF_SUCCESS;

    LogFlowFunc(("hList=%RU64\n", hList));

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLURILISTHANDLEINFO pInfo = sharedClipboardURITransferListGet(pTransfer, hList);
        if (pInfo)
        {
            switch (pInfo->enmType)
            {
                case SHCLURIOBJTYPE_DIRECTORY:
                {
                    LogFlowFunc(("\tDirectory: %s\n", pInfo->pszPathLocalAbs));

                    for (;;)
                    {
                        bool fSkipEntry = false; /* Whether to skip an entry in the enumeration. */

                        size_t        cbDirEntry = 0;
                        PRTDIRENTRYEX pDirEntry  = NULL;
                        rc = RTDirReadExA(pInfo->u.Local.hDir, &pDirEntry, &cbDirEntry, RTFSOBJATTRADD_NOTHING, RTPATH_F_ON_LINK);
                        if (RT_SUCCESS(rc))
                        {
                            switch (pDirEntry->Info.Attr.fMode & RTFS_TYPE_MASK)
                            {
                                case RTFS_TYPE_DIRECTORY:
                                {
                                    /* Skip "." and ".." entries. */
                                    if (RTDirEntryExIsStdDotLink(pDirEntry))
                                    {
                                        fSkipEntry = true;
                                        break;
                                    }

                                    LogFlowFunc(("Directory: %s\n", pDirEntry->szName));
                                    break;
                                }

                                case RTFS_TYPE_FILE:
                                {
                                    LogFlowFunc(("File: %s\n", pDirEntry->szName));
                                    break;
                                }

                                case RTFS_TYPE_SYMLINK:
                                {
                                    rc = VERR_NOT_IMPLEMENTED; /** @todo Not implemented yet. */
                                    break;
                                }

                                default:
                                    break;
                            }

                            if (   RT_SUCCESS(rc)
                                && !fSkipEntry)
                            {
                                pEntry->pvInfo = (PSHCLFSOBJINFO)RTMemAlloc(sizeof(SHCLFSOBJINFO));
                                if (pEntry->pvInfo)
                                {
                                    rc = RTStrCopy(pEntry->pszName, pEntry->cbName, pDirEntry->szName);
                                    if (RT_SUCCESS(rc))
                                    {
                                        SharedClipboardFsObjFromIPRT(PSHCLFSOBJINFO(pEntry->pvInfo), &pDirEntry->Info);

                                        pEntry->cbInfo = sizeof(SHCLFSOBJINFO);
                                        pEntry->fInfo  = VBOX_SHCL_INFO_FLAG_FSOBJINFO;
                                    }
                                }
                                else
                                    rc = VERR_NO_MEMORY;
                            }

                            RTDirReadExAFree(&pDirEntry, &cbDirEntry);
                        }

                        if (   !fSkipEntry /* Do we have a valid entry? Bail out. */
                            || RT_FAILURE(rc))
                        {
                            break;
                        }
                    }

                    break;
                }

                case SHCLURIOBJTYPE_FILE:
                {
                    LogFlowFunc(("\tSingle file: %s\n", pInfo->pszPathLocalAbs));

                    RTFSOBJINFO objInfo;
                    rc = RTFileQueryInfo(pInfo->u.Local.hFile, &objInfo, RTFSOBJATTRADD_NOTHING);
                    if (RT_SUCCESS(rc))
                    {
                        pEntry->pvInfo = (PSHCLFSOBJINFO)RTMemAlloc(sizeof(SHCLFSOBJINFO));
                        if (pEntry->pvInfo)
                        {
                            rc = RTStrCopy(pEntry->pszName, pEntry->cbName, pInfo->pszPathLocalAbs);
                            if (RT_SUCCESS(rc))
                            {
                                SharedClipboardFsObjFromIPRT(PSHCLFSOBJINFO(pEntry->pvInfo), &objInfo);

                                pEntry->cbInfo = sizeof(SHCLFSOBJINFO);
                                pEntry->fInfo  = VBOX_SHCL_INFO_FLAG_FSOBJINFO;
                            }
                        }
                        else
                            rc = VERR_NO_MEMORY;
                    }

                    break;
                }

                default:
                    rc = VERR_NOT_SUPPORTED;
                    break;
            }
        }
        else
            rc = VERR_NOT_FOUND;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnListEntryRead)
            rc = pTransfer->ProviderIface.pfnListEntryRead(&pTransfer->ProviderCtx, hList, pEntry);
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

int SharedClipboardURITransferListWrite(PSHCLURITRANSFER pTransfer, SHCLLISTHANDLE hList,
                                        PSHCLLISTENTRY pEntry)
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

/**
 * Returns whether a given list handle is valid or not.
 *
 * @returns \c true if list handle is valid, \c false if not.
 * @param   pTransfer           URI clipboard transfer to handle.
 * @param   hList               List handle to check.
 */
bool SharedClipboardURITransferListHandleIsValid(PSHCLURITRANSFER pTransfer, SHCLLISTHANDLE hList)
{
    bool fIsValid = false;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        fIsValid = sharedClipboardURITransferListGet(pTransfer, hList) != NULL;
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
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
int SharedClipboardURITransferPrepare(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    AssertMsgReturn(pTransfer->State.enmStatus == SHCLURITRANSFERSTATUS_NONE,
                    ("Transfer has wrong state (%RU32)\n", pTransfer->State.enmStatus), VERR_WRONG_ORDER);

    LogFlowFunc(("pTransfer=%p, enmDir=%RU32\n", pTransfer, pTransfer->State.enmDir));

    if (pTransfer->Callbacks.pfnTransferPrepare)
    {
        SHCLURITRANSFERCALLBACKDATA callbackData = { pTransfer, pTransfer->Callbacks.pvUser };
        pTransfer->Callbacks.pfnTransferPrepare(&callbackData);
    }

    if (RT_SUCCESS(rc))
    {
        pTransfer->State.enmStatus = SHCLURITRANSFERSTATUS_READY;

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
int SharedClipboardURITransferSetInterface(PSHCLURITRANSFER pTransfer,
                                           PSHCLPROVIDERCREATIONCTX pCreationCtx)
{
    AssertPtrReturn(pTransfer,    VERR_INVALID_POINTER);
    AssertPtrReturn(pCreationCtx, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    pTransfer->ProviderIface         = pCreationCtx->Interface;

#ifdef DEBUG
# define LOG_IFACE_PTR(a_Name) \
    LogFlowFunc(( #a_Name "=%p\n", pTransfer->ProviderIface.a_Name));

    LOG_IFACE_PTR(pfnTransferOpen);
    LOG_IFACE_PTR(pfnTransferClose);
    LOG_IFACE_PTR(pfnGetRoots);
    LOG_IFACE_PTR(pfnListOpen);
    LOG_IFACE_PTR(pfnListClose);

# undef LOG_IFACE_PTR
#endif

    pTransfer->ProviderCtx.pTransfer = pTransfer;
    pTransfer->ProviderCtx.pvUser    = pCreationCtx->pvUser;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Clears (resets) the root list of an URI transfer.
 *
 * @param   pTransfer           Transfer to clear URI root list for.
 */
static void sharedClipboardURIListTransferRootsClear(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    if (pTransfer->pszPathRootAbs)
    {
        RTStrFree(pTransfer->pszPathRootAbs);
        pTransfer->pszPathRootAbs = NULL;
    }

    PSHCLURILISTROOT pListRoot, pListRootNext;
    RTListForEachSafe(&pTransfer->lstRoots, pListRoot, pListRootNext, SHCLURILISTROOT, Node)
    {
        RTStrFree(pListRoot->pszPathAbs);

        RTListNodeRemove(&pListRoot->Node);

        RTMemFree(pListRoot);
        pListRoot = NULL;
    }

    pTransfer->cRoots = 0;
}

/**
 * Sets URI root list entries for a given transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           Transfer to set URI list entries for.
 * @param   pszRoots            String list (separated by CRLF) of root entries to set.
 *                              All entries must have the same root path.
 * @param   cbRoots             Size (in bytes) of string list.
 */
int SharedClipboardURILTransferSetRoots(PSHCLURITRANSFER pTransfer, const char *pszRoots, size_t cbRoots)
{
    AssertPtrReturn(pTransfer,      VERR_INVALID_POINTER);
    AssertPtrReturn(pszRoots,       VERR_INVALID_POINTER);
    AssertReturn(cbRoots,           VERR_INVALID_PARAMETER);

    if (!RTStrIsValidEncoding(pszRoots))
        return VERR_INVALID_PARAMETER;

    int rc = VINF_SUCCESS;

    sharedClipboardURIListTransferRootsClear(pTransfer);

    char  *pszPathRootAbs = NULL;

    RTCList<RTCString> lstRootEntries = RTCString(pszRoots, cbRoots - 1).split("\r\n");
    for (size_t i = 0; i < lstRootEntries.size(); ++i)
    {
        PSHCLURILISTROOT pListRoot = (PSHCLURILISTROOT)RTMemAlloc(sizeof(SHCLURILISTROOT));
        AssertPtrBreakStmt(pListRoot, rc = VERR_NO_MEMORY);

        pListRoot->pszPathAbs = RTStrDup(lstRootEntries.at(i).c_str());
        AssertPtrBreakStmt(pListRoot->pszPathAbs, rc = VERR_NO_MEMORY);

        if (!pszPathRootAbs)
        {
            pszPathRootAbs = RTStrDup(pListRoot->pszPathAbs);
            if (pszPathRootAbs)
            {
                RTPathStripFilename(pszPathRootAbs);
                LogFlowFunc(("pszPathRootAbs=%s\n", pszPathRootAbs));
            }
            else
                rc = VERR_NO_MEMORY;
        }

        if (RT_FAILURE(rc))
            break;

        /* Make sure all entries have the same root path. */
        if (!RTStrStartsWith(pListRoot->pszPathAbs, pszPathRootAbs))
        {
            rc = VERR_INVALID_PARAMETER;
            break;
        }

        RTListAppend(&pTransfer->lstRoots, &pListRoot->Node);

        pTransfer->cRoots++;
    }

    /** @todo Entry rollback on failure? */

    if (RT_SUCCESS(rc))
    {
        pTransfer->pszPathRootAbs = pszPathRootAbs;
        LogFlowFunc(("pszPathRootAbs=%s, cRoots=%zu\n", pTransfer->pszPathRootAbs, pTransfer->cRoots));
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Resets an clipboard URI transfer.
 *
 * @param   pTransfer           URI clipboard transfer to reset.
 */
void SharedClipboardURITransferReset(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturnVoid(pTransfer);

    LogFlowFuncEnter();

    sharedClipboardURIListTransferRootsClear(pTransfer);
}

/**
 * Returns the clipboard area for a clipboard URI transfer.
 *
 * @returns Current clipboard area, or NULL if none.
 * @param   pTransfer           URI clipboard transfer to return clipboard area for.
 */
SharedClipboardArea *SharedClipboardURITransferGetArea(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, NULL);

    return pTransfer->pArea;
}

/**
 * Returns the number of URI root list entries.
 *
 * @returns Root list entry count.
 * @param   pTransfer           URI clipboard transfer to return root entry count for.
 */
uint32_t SharedClipboardURILTransferRootsCount(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, 0);

    return (uint32_t)pTransfer->cRoots;
}

/**
 * Returns a specific root list entry of a transfer.
 *
 * @returns Pointer to root list entry if found, or NULL if not found.
 * @param   pTransfer           URI clipboard transfer to get root list entry from.
 * @param   uIdx                Index of root list entry to return.
 */
inline PSHCLURILISTROOT sharedClipboardURILTransferRootsGet(PSHCLURITRANSFER pTransfer, uint32_t uIdx)
{
    if (uIdx >= pTransfer->cRoots)
        return NULL;

    PSHCLURILISTROOT pIt = RTListGetFirst(&pTransfer->lstRoots, SHCLURILISTROOT, Node);
    while (uIdx--)
        pIt = RTListGetNext(&pTransfer->lstRoots, pIt, SHCLURILISTROOT, Node);

    return pIt;
}

/**
 * Get a specific root list entry.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to get root list entry of.
 * @param   uIndex              Index (zero-based) of entry to get.
 * @param   pEntry              Where to store the returned entry on success.
 */
int SharedClipboardURILTransferRootsEntry(PSHCLURITRANSFER pTransfer,
                                          uint64_t uIndex, PSHCLROOTLISTENTRY pEntry)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);
    AssertPtrReturn(pEntry,    VERR_INVALID_POINTER);

    if (uIndex >= pTransfer->cRoots)
        return VERR_INVALID_PARAMETER;

    int rc;

    PSHCLURILISTROOT pRoot = sharedClipboardURILTransferRootsGet(pTransfer, uIndex);
    AssertPtrReturn(pRoot, VERR_INVALID_PARAMETER);

    /* Make sure that we only advertise relative source paths, not absolute ones. */
    const char *pcszSrcPath = pRoot->pszPathAbs;

    char *pszFileName = RTPathFilename(pcszSrcPath);
    if (pszFileName)
    {
        Assert(pszFileName >= pcszSrcPath);
        size_t cchDstBase = pszFileName - pcszSrcPath;
        const char *pszDstPath = &pcszSrcPath[cchDstBase];

        LogFlowFunc(("pcszSrcPath=%s, pszDstPath=%s\n", pcszSrcPath, pszDstPath));

        rc = SharedClipboardURIListEntryInit(pEntry);
        if (RT_SUCCESS(rc))
        {
            rc = RTStrCopy(pEntry->pszName, pEntry->cbName, pszDstPath);
            if (RT_SUCCESS(rc))
            {
                pEntry->cbInfo = sizeof(SHCLFSOBJINFO);
                pEntry->pvInfo = (PSHCLFSOBJINFO)RTMemAlloc(pEntry->cbInfo);
                if (pEntry->pvInfo)
                {
                    RTFSOBJINFO fsObjInfo;
                    rc = RTPathQueryInfo(pcszSrcPath, & fsObjInfo, RTFSOBJATTRADD_NOTHING);
                    if (RT_SUCCESS(rc))
                    {
                        SharedClipboardFsObjFromIPRT(PSHCLFSOBJINFO(pEntry->pvInfo), &fsObjInfo);

                        pEntry->fInfo  = VBOX_SHCL_INFO_FLAG_FSOBJINFO;
                    }
                }
                else
                    rc = VERR_NO_MEMORY;
            }
        }
    }
    else
        rc = VERR_INVALID_POINTER;

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns the root entries of an URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to return root entries for.
 * @param   ppRootList          Where to store the root list on success.
 */
int SharedClipboardURILTransferRootsAsList(PSHCLURITRANSFER pTransfer, PSHCLROOTLIST *ppRootList)
{
    AssertPtrReturn(pTransfer,  VERR_INVALID_POINTER);
    AssertPtrReturn(ppRootList, VERR_INVALID_POINTER);

    LogFlowFuncEnter();

    int rc = VINF_SUCCESS;

    if (pTransfer->State.enmSource == SHCLSOURCE_LOCAL)
    {
        PSHCLROOTLIST pRootList = SharedClipboardURIRootListAlloc();
        if (!pRootList)
            return VERR_NO_MEMORY;

        const uint64_t cRoots = (uint32_t)pTransfer->cRoots;

        LogFlowFunc(("cRoots=%RU64\n", cRoots));

        if (cRoots)
        {
            PSHCLROOTLISTENTRY paRootListEntries
                = (PSHCLROOTLISTENTRY)RTMemAllocZ(cRoots * sizeof(SHCLROOTLISTENTRY));
            if (paRootListEntries)
            {
                for (uint64_t i = 0; i < cRoots; ++i)
                {
                    rc = SharedClipboardURILTransferRootsEntry(pTransfer, i, &paRootListEntries[i]);
                    if (RT_FAILURE(rc))
                        break;
                }

                if (RT_SUCCESS(rc))
                    pRootList->paEntries = paRootListEntries;
            }
            else
                rc = VERR_NO_MEMORY;
        }
        else
            rc = VERR_NOT_FOUND;

        if (RT_SUCCESS(rc))
        {
            pRootList->Hdr.cRoots = cRoots;
            pRootList->Hdr.fRoots = 0; /** @todo Implement this. */

            *ppRootList = pRootList;
        }
    }
    else if (pTransfer->State.enmSource == SHCLSOURCE_REMOTE)
    {
        if (pTransfer->ProviderIface.pfnGetRoots)
            rc = pTransfer->ProviderIface.pfnGetRoots(&pTransfer->ProviderCtx, ppRootList);
        else
            rc = VERR_NOT_SUPPORTED;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

/**
 * Returns the transfer's source.
 *
 * @returns The transfer's source.
 * @param   pTransfer           URI clipboard transfer to return source for.
 */
SHCLSOURCE SharedClipboardURITransferGetSource(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHCLSOURCE_INVALID);

    return pTransfer->State.enmSource;
}

/**
 * Returns the current transfer status.
 *
 * @returns Current transfer status.
 * @param   pTransfer           URI clipboard transfer to return status for.
 */
SHCLURITRANSFERSTATUS SharedClipboardURITransferGetStatus(PSHCLURITRANSFER pTransfer)
{
    AssertPtrReturn(pTransfer, SHCLURITRANSFERSTATUS_NONE);

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
int SharedClipboardURITransferRun(PSHCLURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser)
{
    AssertPtrReturn(pTransfer, VERR_INVALID_POINTER);

    AssertMsgReturn(pTransfer->State.enmStatus == SHCLURITRANSFERSTATUS_READY,
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
void SharedClipboardURITransferSetCallbacks(PSHCLURITRANSFER pTransfer,
                                            PSHCLURITRANSFERCALLBACKS pCallbacks)
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

/**
 * Creates a thread for a clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pTransfer           URI clipboard transfer to create thread for.
 * @param   pfnThreadFunc       Thread function to use for this transfer.
 * @param   pvUser              Pointer to user-provided data.
 */
static int sharedClipboardURITransferThreadCreate(PSHCLURITRANSFER pTransfer, PFNRTTHREAD pfnThreadFunc, void *pvUser)

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
            pTransfer->State.enmStatus = SHCLURITRANSFERSTATUS_RUNNING;
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
static int sharedClipboardURITransferThreadDestroy(PSHCLURITRANSFER pTransfer, RTMSINTERVAL uTimeoutMs)
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
int SharedClipboardURICtxInit(PSHCLURICTX pURI)
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
void SharedClipboardURICtxDestroy(PSHCLURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFunc(("%p\n", pURI));

    RTCritSectDelete(&pURI->CritSect);

    PSHCLURITRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pURI->List, pTransfer, pTransferNext, SHCLURITRANSFER, Node)
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
void SharedClipboardURICtxReset(PSHCLURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFuncEnter();

    PSHCLURITRANSFER pTransfer;
    RTListForEach(&pURI->List, pTransfer, SHCLURITRANSFER, Node)
        SharedClipboardURITransferReset(pTransfer);
}

/**
 * Adds a new URI transfer to an clipboard URI transfer.
 *
 * @returns VBox status code.
 * @param   pURI                URI clipboard context to add transfer to.
 * @param   pTransfer           Pointer to URI clipboard transfer to add.
 */
int SharedClipboardURICtxTransferAdd(PSHCLURICTX pURI, PSHCLURITRANSFER pTransfer)
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
int SharedClipboardURICtxTransferRemove(PSHCLURICTX pURI, PSHCLURITRANSFER pTransfer)
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
static PSHCLURITRANSFER sharedClipboardURICtxGetTransferInternal(PSHCLURICTX pURI, uint32_t uIdx)
{
    AssertReturn(uIdx == 0, NULL); /* Only one transfer allowed at the moment. */
    return RTListGetFirst(&pURI->List, SHCLURITRANSFER, Node);
}

/**
 * Returns a specific URI transfer.
 *
 * @returns URI transfer, or NULL if not found.
 * @param   pURI                URI clipboard context to return transfer for.
 * @param   uIdx                Index of the transfer to return.
 */
PSHCLURITRANSFER SharedClipboardURICtxGetTransfer(PSHCLURICTX pURI, uint32_t uIdx)
{
    return sharedClipboardURICtxGetTransferInternal(pURI, uIdx);
}

/**
 * Returns the number of running URI transfers.
 *
 * @returns Number of running transfers.
 * @param   pURI                URI clipboard context to return number for.
 */
uint32_t SharedClipboardURICtxGetRunningTransfers(PSHCLURICTX pURI)
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
uint32_t SharedClipboardURICtxGetTotalTransfers(PSHCLURICTX pURI)
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
void SharedClipboardURICtxTransfersCleanup(PSHCLURICTX pURI)
{
    AssertPtrReturnVoid(pURI);

    LogFlowFunc(("cTransfers=%RU32, cRunning=%RU32\n", pURI->cTransfers, pURI->cRunning));

    /* Remove all transfers which are not in a running state (e.g. only announced). */
    PSHCLURITRANSFER pTransfer, pTransferNext;
    RTListForEachSafe(&pURI->List, pTransfer, pTransferNext, SHCLURITRANSFER, Node)
    {
        if (SharedClipboardURITransferGetStatus(pTransfer) != SHCLURITRANSFERSTATUS_RUNNING)
        {
            SharedClipboardURITransferDestroy(pTransfer);
            RTListNodeRemove(&pTransfer->Node);

            RTMemFree(pTransfer);
            pTransfer = NULL;

            Assert(pURI->cTransfers);
            pURI->cTransfers--;
        }
    }
}

/**
 * Returns whether the maximum of concurrent transfers of a specific URI context has been reached or not.
 *
 * @returns \c if maximum has been reached, \c false if not.
 * @param   pURI                URI clipboard context to determine value for.
 */
bool SharedClipboardURICtxTransfersMaximumReached(PSHCLURICTX pURI)
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
void SharedClipboardFsObjFromIPRT(PSHCLFSOBJINFO pDst, PCRTFSOBJINFO pSrc)
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
            pDst->Attr.enmAdditional        = SHCLFSOBJATTRADD_NOTHING;
            break;

        case RTFSOBJATTRADD_UNIX:
            pDst->Attr.enmAdditional        = SHCLFSOBJATTRADD_UNIX;
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
            pDst->Attr.enmAdditional        = SHCLFSOBJATTRADD_EASIZE;
            pDst->Attr.u.EASize.cb          = pSrc->Attr.u.EASize.cb;
            break;
    }
}

/**
 * Converts Shared Clipboard create flags (see SharedClipboard-uri.) into IPRT create flags.
 *
 * @returns IPRT status code.
 * @param  fWritable            Whether the shared folder is writable
 * @param  fShClFlags           Shared clipboard create flags.
 * @param  fMode                File attributes.
 * @param  handleInitial        Initial handle.
 * @retval pfOpen               Where to store the IPRT creation / open flags.
 *
 * @sa Initially taken from vbsfConvertFileOpenFlags().
 */
static int sharedClipboardConvertFileCreateFlags(bool fWritable, unsigned fShClFlags, RTFMODE fMode,
                                                 SHCLOBJHANDLE handleInitial, uint64_t *pfOpen)
{
    uint64_t fOpen = 0;
    int rc = VINF_SUCCESS;

    if (   (fMode & RTFS_DOS_MASK) != 0
        && (fMode & RTFS_UNIX_MASK) == 0)
    {
        /* A DOS/Windows guest, make RTFS_UNIX_* from RTFS_DOS_*.
         * @todo this is based on rtFsModeNormalize/rtFsModeFromDos.
         *       May be better to use RTFsModeNormalize here.
         */
        fMode |= RTFS_UNIX_IRUSR | RTFS_UNIX_IRGRP | RTFS_UNIX_IROTH;
        /* x for directories. */
        if (fMode & RTFS_DOS_DIRECTORY)
            fMode |= RTFS_TYPE_DIRECTORY | RTFS_UNIX_IXUSR | RTFS_UNIX_IXGRP | RTFS_UNIX_IXOTH;
        /* writable? */
        if (!(fMode & RTFS_DOS_READONLY))
            fMode |= RTFS_UNIX_IWUSR | RTFS_UNIX_IWGRP | RTFS_UNIX_IWOTH;

        /* Set the requested mode using only allowed bits. */
        fOpen |= ((fMode & RTFS_UNIX_MASK) << RTFILE_O_CREATE_MODE_SHIFT) & RTFILE_O_CREATE_MODE_MASK;
    }
    else
    {
        /* Old linux and solaris additions did not initialize the Info.Attr.fMode field
         * and it contained random bits from stack. Detect this using the handle field value
         * passed from the guest: old additions set it (incorrectly) to 0, new additions
         * set it to SHCLOBJHANDLE_INVALID(~0).
         */
        if (handleInitial == 0)
        {
            /* Old additions. Do nothing, use default mode. */
        }
        else
        {
            /* New additions or Windows additions. Set the requested mode using only allowed bits.
             * Note: Windows guest set RTFS_UNIX_MASK bits to 0, which means a default mode
             *       will be set in fOpen.
             */
            fOpen |= ((fMode & RTFS_UNIX_MASK) << RTFILE_O_CREATE_MODE_SHIFT) & RTFILE_O_CREATE_MODE_MASK;
        }
    }

    switch ((fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_RW))
    {
        default:
        case SHCL_OBJ_CF_ACCESS_NONE:
        {
#ifdef RT_OS_WINDOWS
            if ((fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_ATTR) != SHCL_OBJ_CF_ACCESS_ATTR_NONE)
                fOpen |= RTFILE_O_ATTR_ONLY;
            else
#endif
                fOpen |= RTFILE_O_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_NONE\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_READ:
        {
            fOpen |= RTFILE_O_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_READ\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_WRITE:
        {
            fOpen |= RTFILE_O_WRITE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_WRITE\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_READWRITE:
        {
            fOpen |= RTFILE_O_READWRITE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_READWRITE\n"));
            break;
        }
    }

    if (fShClFlags & SHCL_OBJ_CF_ACCESS_APPEND)
    {
        fOpen |= RTFILE_O_APPEND;
    }

    switch ((fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_ATTR))
    {
        default:
        case SHCL_OBJ_CF_ACCESS_ATTR_NONE:
        {
            fOpen |= RTFILE_O_ACCESS_ATTR_DEFAULT;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_ATTR_NONE\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_ATTR_READ:
        {
            fOpen |= RTFILE_O_ACCESS_ATTR_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_ATTR_READ\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_ATTR_WRITE:
        {
            fOpen |= RTFILE_O_ACCESS_ATTR_WRITE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_ATTR_WRITE\n"));
            break;
        }

        case SHCL_OBJ_CF_ACCESS_ATTR_READWRITE:
        {
            fOpen |= RTFILE_O_ACCESS_ATTR_READWRITE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_ATTR_READWRITE\n"));
            break;
        }
    }

    /* Sharing mask */
    switch ((fShClFlags & SHCL_OBJ_CF_ACCESS_MASK_DENY))
    {
        default:
        case SHCL_OBJ_CF_ACCESS_DENYNONE:
            fOpen |= RTFILE_O_DENY_NONE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_DENYNONE\n"));
            break;

        case SHCL_OBJ_CF_ACCESS_DENYREAD:
            fOpen |= RTFILE_O_DENY_READ;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_DENYREAD\n"));
            break;

        case SHCL_OBJ_CF_ACCESS_DENYWRITE:
            fOpen |= RTFILE_O_DENY_WRITE;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_DENYWRITE\n"));
            break;

        case SHCL_OBJ_CF_ACCESS_DENYALL:
            fOpen |= RTFILE_O_DENY_ALL;
            LogFlowFunc(("SHCL_OBJ_CF_ACCESS_DENYALL\n"));
            break;
    }

    /* Open/Create action mask */
    switch ((fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_EXISTS))
    {
        case SHCL_OBJ_CF_ACT_OPEN_IF_EXISTS:
            if (SHCL_OBJ_CF_ACT_CREATE_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_OPEN_CREATE;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_OPEN_IF_EXISTS and SHCL_OBJ_CF_ACT_CREATE_IF_NEW\n"));
            }
            else if (SHCL_OBJ_CF_ACT_FAIL_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_OPEN;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_OPEN_IF_EXISTS and SHCL_OBJ_CF_ACT_FAIL_IF_NEW\n"));
            }
            else
            {
                LogFlowFunc(("invalid open/create action combination\n"));
                rc = VERR_INVALID_PARAMETER;
            }
            break;
        case SHCL_OBJ_CF_ACT_FAIL_IF_EXISTS:
            if (SHCL_OBJ_CF_ACT_CREATE_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_CREATE;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_FAIL_IF_EXISTS and SHCL_OBJ_CF_ACT_CREATE_IF_NEW\n"));
            }
            else
            {
                LogFlowFunc(("invalid open/create action combination\n"));
                rc = VERR_INVALID_PARAMETER;
            }
            break;
        case SHCL_OBJ_CF_ACT_REPLACE_IF_EXISTS:
            if (SHCL_OBJ_CF_ACT_CREATE_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_CREATE_REPLACE;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_REPLACE_IF_EXISTS and SHCL_OBJ_CF_ACT_CREATE_IF_NEW\n"));
            }
            else if (SHCL_OBJ_CF_ACT_FAIL_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_OPEN | RTFILE_O_TRUNCATE;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_REPLACE_IF_EXISTS and SHCL_OBJ_CF_ACT_FAIL_IF_NEW\n"));
            }
            else
            {
                LogFlowFunc(("invalid open/create action combination\n"));
                rc = VERR_INVALID_PARAMETER;
            }
            break;
        case SHCL_OBJ_CF_ACT_OVERWRITE_IF_EXISTS:
            if (SHCL_OBJ_CF_ACT_CREATE_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_CREATE_REPLACE;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_OVERWRITE_IF_EXISTS and SHCL_OBJ_CF_ACT_CREATE_IF_NEW\n"));
            }
            else if (SHCL_OBJ_CF_ACT_FAIL_IF_NEW == (fShClFlags & SHCL_OBJ_CF_ACT_MASK_IF_NEW))
            {
                fOpen |= RTFILE_O_OPEN | RTFILE_O_TRUNCATE;
                LogFlowFunc(("SHCL_OBJ_CF_ACT_OVERWRITE_IF_EXISTS and SHCL_OBJ_CF_ACT_FAIL_IF_NEW\n"));
            }
            else
            {
                LogFlowFunc(("invalid open/create action combination\n"));
                rc = VERR_INVALID_PARAMETER;
            }
            break;
        default:
        {
            rc = VERR_INVALID_PARAMETER;
            LogFlowFunc(("SHCL_OBJ_CF_ACT_MASK_IF_EXISTS - invalid parameter\n"));
            break;
        }
    }

    if (RT_SUCCESS(rc))
    {
        if (!fWritable)
            fOpen &= ~RTFILE_O_WRITE;

        *pfOpen = fOpen;
    }

    LogFlowFuncLeaveRC(rc);
    return rc;
}

