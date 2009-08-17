/* $Id$ */
/** @file
 * PDM Async I/O - Transport data asynchronous in R3 using EMT.
 * File data cache.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#define LOG_GROUP LOG_GROUP_PDM_ASYNC_COMPLETION
#include <iprt/types.h>
#include <iprt/mem.h>
#include <VBox/log.h>
#include <VBox/stam.h>

#include "PDMAsyncCompletionFileInternal.h"

static void pdmacFileCacheTaskCompleted(PPDMACTASKFILE pTask, void *pvUser);

static void pdmacFileCacheCheckList(PPDMACFILELRULIST pList, PPDMACFILECACHEENTRY pNotInList)
{
#ifdef PDMACFILECACHE_WITH_LRULIST_CHECKS
    PPDMACFILECACHEENTRY pCurr = pList->pHead;

    /* Check that there are no double entries and no cycles in the list. */
    while (pCurr)
    {
        PPDMACFILECACHEENTRY pNext = pCurr->pNext;

        while (pNext)
        {
            AssertMsg(pCurr != pNext,
                      ("Entry %#p is at least two times in list %#p or there is a cycle in the list\n",
                      pCurr, pList));
            pNext = pNext->pNext;
        }

        AssertMsg(pCurr != pNotInList, ("Not allowed entry %#p is in list\n", pCurr));

        if (!pCurr->pNext)
            AssertMsg(pCurr == pList->pTail, ("End of list reached but last element is not list tail\n"));

        pCurr = pCurr->pNext;
    }
#endif
}

static void pdmacFileCacheEntryRemoveFromList(PPDMACFILECACHEENTRY pEntry)
{
    PPDMACFILELRULIST pList = pEntry->pList;
    PPDMACFILECACHEENTRY pPrev, pNext;

    LogFlowFunc((": Deleting entry %#p from list %#p\n", pEntry, pList));

    AssertPtr(pList);
    pdmacFileCacheCheckList(pList, NULL);

    pPrev = pEntry->pPrev;
    pNext = pEntry->pNext;

    AssertMsg(pEntry != pPrev, ("Entry links to itself as previous element\n"));
    AssertMsg(pEntry != pNext, ("Entry links to itself as next element\n"));

    if (pPrev)
        pPrev->pNext = pNext;
    else
    {
        pList->pHead = pNext;

        if (pNext)
            pNext->pPrev = NULL;
    }

    if (pNext)
        pNext->pPrev = pPrev;
    else
    {
        pList->pTail = pPrev;

        if (pPrev)
            pPrev->pNext = NULL;
    }

    pEntry->pList    = NULL;
    pEntry->pPrev    = NULL;
    pEntry->pNext    = NULL;
    pList->cbCached -= pEntry->cbData;
    pdmacFileCacheCheckList(pList, pEntry);
}

static void pdmacFileCacheEntryAddToList(PPDMACFILELRULIST pList, PPDMACFILECACHEENTRY pEntry)
{
    LogFlowFunc((": Adding entry %#p to list %#p\n", pEntry, pList));
    pdmacFileCacheCheckList(pList, NULL);

    /* Remove from old list if needed */
    if (pEntry->pList)
        pdmacFileCacheEntryRemoveFromList(pEntry);

    pEntry->pNext = pList->pHead;
    if (pList->pHead)
        pList->pHead->pPrev = pEntry;
    else
    {
        Assert(!pList->pTail);
        pList->pTail = pEntry;
    }

    pEntry->pPrev    = NULL;
    pList->pHead     = pEntry;
    pList->cbCached += pEntry->cbData;
    pEntry->pList    = pList;
    pdmacFileCacheCheckList(pList, NULL);
}

/**
 * Destroys a LRU list freeing all entries.
 *
 * @returns nothing
 * @param   pList    Pointer to the LRU list to destroy.
 *
 * @note The caller must own the critical section of the cache.
 */
static void pdmacFileCacheDestroyList(PPDMACFILELRULIST pList)
{
    while (pList->pHead)
    {
        PPDMACFILECACHEENTRY pEntry = pList->pHead;

        pList->pHead = pEntry->pNext;

        AssertMsg(!(pEntry->fFlags & (PDMACFILECACHE_ENTRY_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DIRTY)),
                  ("Entry is dirty and/or still in progress fFlags=%#x\n", pEntry->fFlags));

        RTMemPageFree(pEntry->pbData);
        RTMemFree(pEntry);
    }
}

static size_t pdmacFileCacheEvictPagesFrom(PPDMACFILECACHEGLOBAL pCache, size_t cbData,
                                           PPDMACFILELRULIST pListSrc, PPDMACFILELRULIST pGhostListDst)
{
    size_t cbEvicted = 0;

    AssertMsg(cbData > 0, ("Evicting 0 bytes not possible\n"));
    AssertMsg(   !pGhostListDst
              || (pGhostListDst == &pCache->LruRecentlyGhost)
              || (pGhostListDst == &pCache->LruFrequentlyGhost),
              ("Destination list must be NULL or one of the ghost lists\n"));

    /* Start deleting from the tail. */
    PPDMACFILECACHEENTRY pEntry = pListSrc->pTail;

    while ((cbEvicted < cbData) && pEntry)
    {
        PPDMACFILECACHEENTRY pCurr = pEntry;

        pEntry = pEntry->pPrev;

        /* We can't evict pages which are currently in progress */
        if (!(pCurr->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS))
        {
            LogFlow(("Evicting entry %#p (%u bytes)\n", pCurr, pCurr->cbData));
            if (pCurr->pbData)
            {
                RTMemPageFree(pCurr->pbData);
                pCurr->pbData = NULL;
            }

            cbEvicted += pCurr->cbData;

            if (pGhostListDst)
            {
                pdmacFileCacheEntryAddToList(pGhostListDst, pCurr);
            }
            else
            {
                /* Delete the entry from the AVL tree it is assigned to. */
                STAM_PROFILE_ADV_START(&pCache->StatTreeRemove, Cache);
                RTAvlrFileOffsetRemove(pCurr->pEndpoint->DataCache.pTree, pCurr->Core.Key);
                STAM_PROFILE_ADV_STOP(&pCache->StatTreeRemove, Cache);

                pdmacFileCacheEntryRemoveFromList(pCurr);
                pCache->cbCached -= pCurr->cbData;
                RTMemFree(pCurr);
            }
        }
        else
            LogFlow(("Entry %#p (%u bytes) is still in progress and can't be evicted\n", pCurr, pCurr->cbData));
    }

    return cbEvicted;
}

static size_t pdmacFileCacheReplace(PPDMACFILECACHEGLOBAL pCache, size_t cbData, PPDMACFILELRULIST pEntryList)
{
    if (   (pCache->LruRecentlyUsed.cbCached)
        && (   (pCache->LruRecentlyUsed.cbCached > pCache->uAdaptVal)
            || (   (pEntryList == &pCache->LruFrequentlyGhost)
                && (pCache->LruRecentlyUsed.cbCached == pCache->uAdaptVal))))
    {
        /* We need to remove entry size pages from T1 and move the entries to B1 */
        return pdmacFileCacheEvictPagesFrom(pCache, cbData,
                                            &pCache->LruRecentlyUsed,
                                            &pCache->LruRecentlyGhost);
    }
    else
    {
        /* We need to remove entry size pages from T2 and move the entries to B2 */
        return pdmacFileCacheEvictPagesFrom(pCache, cbData,
                                            &pCache->LruFrequentlyUsed,
                                            &pCache->LruFrequentlyGhost);
    }
}

static size_t pdmacFileCacheEvict(PPDMACFILECACHEGLOBAL pCache, size_t cbData)
{
    size_t cbRemoved = ~0;

    if ((pCache->LruRecentlyUsed.cbCached + pCache->LruRecentlyGhost.cbCached) >= pCache->cbMax)
    {
        /* Delete desired pages from the cache. */
        if (pCache->LruRecentlyUsed.cbCached < pCache->cbMax)
        {
            cbRemoved = pdmacFileCacheEvictPagesFrom(pCache, cbData,
                                                     &pCache->LruRecentlyGhost,
                                                     NULL);
        }
        else
        {
            cbRemoved = pdmacFileCacheEvictPagesFrom(pCache, cbData,
                                                     &pCache->LruRecentlyUsed,
                                                     NULL);
        }
    }
    else
    {
        uint32_t cbUsed = pCache->LruRecentlyUsed.cbCached + pCache->LruRecentlyGhost.cbCached +
                          pCache->LruFrequentlyUsed.cbCached + pCache->LruFrequentlyGhost.cbCached;

        if (cbUsed >= pCache->cbMax)
        {
            if (cbUsed == 2*pCache->cbMax)
                cbRemoved = pdmacFileCacheEvictPagesFrom(pCache, cbData,
                                                         &pCache->LruFrequentlyGhost,
                                                         NULL);

            if (cbRemoved >= cbData)
                cbRemoved = pdmacFileCacheReplace(pCache, cbData, NULL);
        }
    }

    return cbRemoved;
}

static void pdmacFileCacheUpdate(PPDMACFILECACHEGLOBAL pCache, PPDMACFILECACHEENTRY pEntry)
{
    int32_t uUpdateVal = 0;

    /* Update parameters */
    if (pEntry->pList == &pCache->LruRecentlyGhost)
    {
        if (pCache->LruRecentlyGhost.cbCached >= pCache->LruFrequentlyGhost.cbCached)
            uUpdateVal = 1;
        else
            uUpdateVal = pCache->LruFrequentlyGhost.cbCached / pCache->LruRecentlyGhost.cbCached;

        pCache->uAdaptVal = RT_MIN(pCache->uAdaptVal + uUpdateVal, pCache->cbMax);
    }
    else if (pEntry->pList == &pCache->LruFrequentlyGhost)
    {
        if (pCache->LruFrequentlyGhost.cbCached >= pCache->LruRecentlyGhost.cbCached)
            uUpdateVal = 1;
        else
            uUpdateVal = pCache->LruRecentlyGhost.cbCached / pCache->LruFrequentlyGhost.cbCached;

        pCache->uAdaptVal = RT_MIN(pCache->uAdaptVal - uUpdateVal, 0);
    }
    else
        AssertMsgFailed(("Invalid list type\n"));
}

static void pdmacFileCacheReadFromEndpoint(PPDMACFILECACHEENTRY pEntry)
{
    /* Make sure no one evicts the entry while it is accessed. */
    pEntry->fFlags |= PDMACFILECACHE_ENTRY_IO_IN_PROGRESS;

    PPDMACTASKFILE pIoTask = pdmacFileTaskAlloc(pEntry->pEndpoint);
    AssertPtr(pIoTask);

    AssertMsg(pEntry->pbData, ("Entry is in ghost state\n"));

    pIoTask->pEndpoint       = pEntry->pEndpoint;
    pIoTask->enmTransferType = PDMACTASKFILETRANSFER_READ;
    pIoTask->Off             = pEntry->Core.Key;
    pIoTask->DataSeg.cbSeg   = pEntry->cbData;
    pIoTask->DataSeg.pvSeg   = pEntry->pbData;
    pIoTask->pvUser          = pEntry;
    pIoTask->pfnCompleted    = pdmacFileCacheTaskCompleted;

    /* Send it off to the I/O manager. */
    pdmacFileEpAddTask(pEntry->pEndpoint, pIoTask);
}

static void pdmacFileCacheWriteToEndpoint(PPDMACFILECACHEENTRY pEntry)
{
    /* Make sure no one evicts the entry while it is accessed. */
    pEntry->fFlags |= PDMACFILECACHE_ENTRY_IO_IN_PROGRESS;

    PPDMACTASKFILE pIoTask = pdmacFileTaskAlloc(pEntry->pEndpoint);
    AssertPtr(pIoTask);

    AssertMsg(pEntry->pbData, ("Entry is in ghost state\n"));

    pIoTask->pEndpoint       = pEntry->pEndpoint;
    pIoTask->enmTransferType = PDMACTASKFILETRANSFER_WRITE;
    pIoTask->Off             = pEntry->Core.Key;
    pIoTask->DataSeg.cbSeg   = pEntry->cbData;
    pIoTask->DataSeg.pvSeg   = pEntry->pbData;
    pIoTask->pvUser          = pEntry;
    pIoTask->pfnCompleted    = pdmacFileCacheTaskCompleted;

    /* Send it off to the I/O manager. */
    pdmacFileEpAddTask(pEntry->pEndpoint, pIoTask);
}

static void pdmacFileCacheTaskCompleted(PPDMACTASKFILE pTask, void *pvUser)
{
    PPDMACFILECACHEENTRY pEntry = (PPDMACFILECACHEENTRY)pvUser;
    PPDMACFILECACHEGLOBAL pCache = pEntry->pCache;

    RTCritSectEnter(&pCache->CritSect);

    pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IO_IN_PROGRESS;

    if (pTask->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
    {
        pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IS_DIRTY;

        /* Process waiting segment list. The data in entry might have changed inbetween. */
        PPDMACFILETASKSEG pCurr = pEntry->pHead;

        while (pCurr)
        {
            AssertMsg(pCurr->fWrite, ("Completed write entries should never have read tasks attached\n"));

            memcpy(pEntry->pbData + pCurr->uBufOffset, pCurr->pvBuf, pCurr->cbTransfer);
            pEntry->fFlags |= PDMACFILECACHE_ENTRY_IS_DIRTY;

            uint32_t uOld = ASMAtomicSubU32(&pCurr->pTask->cbTransferLeft, pCurr->cbTransfer);
            AssertMsg(uOld >= pCurr->cbTransfer, ("New value would overflow\n"));
            if (!(uOld - pCurr->cbTransfer)
                && !ASMAtomicXchgBool(&pCurr->pTask->fCompleted, true))
                pdmR3AsyncCompletionCompleteTask(&pCurr->pTask->Core);

            PPDMACFILETASKSEG pFree = pCurr;
            pCurr = pCurr->pNext;

            RTMemFree(pFree);
        }
    }
    else
    {
        AssertMsg(pTask->enmTransferType == PDMACTASKFILETRANSFER_READ, ("Invalid transfer type\n"));
        AssertMsg(!(pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY),("Invalid flags set\n"));

        /* Process waiting segment list. */
        PPDMACFILETASKSEG pCurr = pEntry->pHead;

        while (pCurr)
        {
            if (pCurr->fWrite)
            {
                memcpy(pEntry->pbData + pCurr->uBufOffset, pCurr->pvBuf, pCurr->cbTransfer);
                pEntry->fFlags |= PDMACFILECACHE_ENTRY_IS_DIRTY;
            }
            else
                memcpy(pCurr->pvBuf, pEntry->pbData + pCurr->uBufOffset, pCurr->cbTransfer);

            uint32_t uOld = ASMAtomicSubU32(&pCurr->pTask->cbTransferLeft, pCurr->cbTransfer);
            AssertMsg(uOld >= pCurr->cbTransfer, ("New value would overflow\n"));
            if (!(uOld - pCurr->cbTransfer)
                && !ASMAtomicXchgBool(&pCurr->pTask->fCompleted, true))
                pdmR3AsyncCompletionCompleteTask(&pCurr->pTask->Core);

            PPDMACFILETASKSEG pFree = pCurr;
            pCurr = pCurr->pNext;

            RTMemFree(pFree);
        }
    }

    pEntry->pHead = NULL;

    if (pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY)
        pdmacFileCacheWriteToEndpoint(pEntry);

    RTCritSectLeave(&pCache->CritSect);
}

int pdmacFileCacheInit(PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile, PCFGMNODE pCfgNode)
{
    int rc = VINF_SUCCESS;
    PPDMACFILECACHEGLOBAL pCache = &pClassFile->Cache;

    /* Initialize members */
    pCache->LruRecentlyUsed.pHead  = NULL;
    pCache->LruRecentlyUsed.pTail  = NULL;
    pCache->LruRecentlyUsed.cbCached = 0;

    pCache->LruFrequentlyUsed.pHead  = NULL;
    pCache->LruFrequentlyUsed.pTail  = NULL;
    pCache->LruFrequentlyUsed.cbCached = 0;

    pCache->LruRecentlyGhost.pHead  = NULL;
    pCache->LruRecentlyGhost.pTail  = NULL;
    pCache->LruRecentlyGhost.cbCached = 0;

    pCache->LruFrequentlyGhost.pHead  = NULL;
    pCache->LruFrequentlyGhost.pTail  = NULL;
    pCache->LruFrequentlyGhost.cbCached = 0;

    rc = CFGMR3QueryU32Def(pCfgNode, "CacheSize", &pCache->cbMax, 5 * _1M);
    AssertLogRelRCReturn(rc, rc);

    pCache->cbCached  = 0;
    pCache->uAdaptVal = 0;
    LogFlowFunc((": Maximum number of bytes cached %u\n", pCache->cbCached));

    STAMR3Register(pClassFile->Core.pVM, &pCache->cbMax,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbMax",
                   STAMUNIT_BYTES,
                   "Maximum cache size");
    STAMR3Register(pClassFile->Core.pVM, &pCache->cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCached",
                   STAMUNIT_BYTES,
                   "Currently used cache");
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruRecentlyUsed.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedMru",
                   STAMUNIT_BYTES,
                   "Number of bytes cached in Mru list");
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruFrequentlyUsed.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedFru",
                   STAMUNIT_BYTES,
                   "Number of bytes cached in Fru list");
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruRecentlyGhost.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedMruGhost",
                   STAMUNIT_BYTES,
                   "Number of bytes cached in Mru ghost list");
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruFrequentlyGhost.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedFruGhost",
                   STAMUNIT_BYTES, "Number of bytes cached in Fru ghost list");

#ifdef VBOX_WITH_STATISTICS
    STAMR3Register(pClassFile->Core.pVM, &pCache->cHits,
                   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheHits",
                   STAMUNIT_COUNT, "Number of hits in the cache");
    STAMR3Register(pClassFile->Core.pVM, &pCache->cPartialHits,
                   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CachePartialHits",
                   STAMUNIT_COUNT, "Number of partial hits in the cache");
    STAMR3Register(pClassFile->Core.pVM, &pCache->cMisses,
                   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheMisses",
                   STAMUNIT_COUNT, "Number of misses when accessing the cache");
    STAMR3Register(pClassFile->Core.pVM, &pCache->StatRead,
                   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheRead",
                   STAMUNIT_BYTES, "Number of bytes read from the cache");
    STAMR3Register(pClassFile->Core.pVM, &pCache->StatWritten,
                   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheWritten",
                   STAMUNIT_BYTES, "Number of bytes written to the cache");
    STAMR3Register(pClassFile->Core.pVM, &pCache->StatTreeGet,
                   STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheTreeGet",
                   STAMUNIT_TICKS_PER_CALL, "Time taken to access an entry in the tree");
    STAMR3Register(pClassFile->Core.pVM, &pCache->StatTreeInsert,
                   STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheTreeInsert",
                   STAMUNIT_TICKS_PER_CALL, "Time taken to insert an entry in the tree");
    STAMR3Register(pClassFile->Core.pVM, &pCache->StatTreeRemove,
                   STAMTYPE_PROFILE_ADV, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheTreeRemove",
                   STAMUNIT_TICKS_PER_CALL, "Time taken to remove an entry an the tree");
#endif

    /* Initialize the critical section */
    rc = RTCritSectInit(&pCache->CritSect);
    return rc;
}

void pdmacFileCacheDestroy(PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile)
{
    PPDMACFILECACHEGLOBAL pCache = &pClassFile->Cache;

    /* Make sure no one else uses the cache now */
    RTCritSectEnter(&pCache->CritSect);

    /* Cleanup deleting all cache entries waiting for in progress entries to finish. */
    pdmacFileCacheDestroyList(&pCache->LruRecentlyUsed);
    pdmacFileCacheDestroyList(&pCache->LruFrequentlyUsed);
    pdmacFileCacheDestroyList(&pCache->LruRecentlyGhost);
    pdmacFileCacheDestroyList(&pCache->LruFrequentlyGhost);

    RTCritSectLeave(&pCache->CritSect);

    RTCritSectDelete(&pCache->CritSect);
}

int pdmacFileEpCacheInit(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile)
{
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;

    pEndpointCache->pTree  = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
    pEndpointCache->pCache = &pClassFile->Cache;

    return VINF_SUCCESS;
}

static int pdmacFileEpCacheEntryDestroy(PAVLRFOFFNODECORE pNode, void *pvUser)
{
    PPDMACFILECACHEENTRY  pEntry = (PPDMACFILECACHEENTRY)pNode;
    PPDMACFILECACHEGLOBAL pCache = (PPDMACFILECACHEGLOBAL)pvUser;

    AssertMsg(!(pEntry->fFlags & (PDMACFILECACHE_ENTRY_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DIRTY)),
                ("Entry is dirty and/or still in progress fFlags=%#x\n", pEntry->fFlags));

    pdmacFileCacheEntryRemoveFromList(pEntry);
    pCache->cbCached -= pEntry->cbData;

    RTMemPageFree(pEntry->pbData);
    RTMemFree(pEntry);

    return VINF_SUCCESS;
}

void pdmacFileEpCacheDestroy(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;
    PPDMACFILECACHEGLOBAL   pCache         = pEndpointCache->pCache;

    /* Make sure nobody is accessing the cache while we delete the tree. */
    RTCritSectEnter(&pCache->CritSect);
    RTAvlrFileOffsetDestroy(pEndpointCache->pTree, pdmacFileEpCacheEntryDestroy, pCache);
    RTCritSectLeave(&pCache->CritSect);
}

/**
 * Advances the current segment buffer by the number of bytes transfered
 * or gets the next segment.
 */
#define ADVANCE_SEGMENT_BUFFER(BytesTransfered) \
    do \
    { \
        cbSegLeft -= BytesTransfered; \
        if (!cbSegLeft) \
        { \
            iSegCurr++; \
            cbSegLeft = paSegments[iSegCurr].cbSeg; \
            pbSegBuf  = (uint8_t *)paSegments[iSegCurr].pvSeg; \
        } \
        else \
            pbSegBuf += BytesTransfered; \
    } \
    while (0);

int pdmacFileEpCacheRead(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask,
                         RTFOFF off, PCPDMDATASEG paSegments, size_t cSegments,
                         size_t cbRead)
{
    int rc = VINF_SUCCESS;
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    PPDMACFILECACHEENTRY pEntry;

    LogFlowFunc((": pEndpoint=%#p{%s} pTask=%#p off=%RTfoff paSegments=%#p cSegments=%u cbRead=%u\n",
                 pEndpoint, pEndpoint->Core.pszUri, pTask, off, paSegments, cSegments, cbRead));

    pTask->cbTransferLeft = cbRead;
    /* Set to completed to make sure that the task is valid while we access it. */
    ASMAtomicWriteBool(&pTask->fCompleted, true);

    RTCritSectEnter(&pCache->CritSect);

    int iSegCurr       = 0;
    uint8_t *pbSegBuf  = (uint8_t *)paSegments[iSegCurr].pvSeg;
    size_t   cbSegLeft = paSegments[iSegCurr].cbSeg;

    while (cbRead)
    {
        size_t cbToRead;

        STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);
        pEntry = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetRangeGet(pEndpointCache->pTree, off);
        STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);

        /*
         * If there is no entry we try to create a new one eviciting unused pages
         * if the cache is full. If this is not possible we will pass the request through
         * and skip the caching (all entries may be still in progress so they can't
         * be evicted)
         * If we have an entry it can be in one of the LRU lists where the entry
         * contains data (recently used or frequently used LRU) so we can just read
         * the data we need and put the entry at the head of the frequently used LRU list.
         * In case the entry is in one of the ghost lists it doesn't contain any data.
         * We have to fetch it again evicting pages from either T1 or T2 to make room.
         */
        if (pEntry)
        {
            RTFOFF OffDiff = off - pEntry->Core.Key;

            AssertMsg(off >= pEntry->Core.Key,
                      ("Overflow in calculation off=%RTfoff OffsetAligned=%RTfoff\n",
                      off, pEntry->Core.Key));

            AssertPtr(pEntry->pList);

            cbToRead = RT_MIN(pEntry->cbData - OffDiff, cbRead);
            cbRead  -= cbToRead;

            if (!cbRead)
                STAM_COUNTER_INC(&pCache->cHits);
            else
                STAM_COUNTER_INC(&pCache->cPartialHits);

            STAM_COUNTER_ADD(&pCache->StatRead, cbToRead);

            /* Ghost lists contain no data. */
            if (   (pEntry->pList == &pCache->LruRecentlyUsed)
                || (pEntry->pList == &pCache->LruFrequentlyUsed))
            {
                if (   (pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS)
                    && !(pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY))
                {
                    /* Entry didn't completed yet. Append to the list */
                    while (cbToRead)
                    {
                        PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                        pSeg->pTask      = pTask;
                        pSeg->uBufOffset = OffDiff;
                        pSeg->cbTransfer = RT_MIN(cbToRead, cbSegLeft);
                        pSeg->pvBuf      = pbSegBuf;
                        pSeg->fWrite     = false;

                        ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                        pSeg->pNext = pEntry->pHead;
                        pEntry->pHead = pSeg;

                        off      += pSeg->cbTransfer;
                        cbToRead -= pSeg->cbTransfer;
                        OffDiff  += pSeg->cbTransfer;
                    }
                }
                else
                {
                    /* Read as much as we can from the entry. */
                    while (cbToRead)
                    {
                        size_t cbCopy = RT_MIN(cbSegLeft, cbToRead);

                        memcpy(pbSegBuf, pEntry->pbData + OffDiff, cbCopy);

                        ADVANCE_SEGMENT_BUFFER(cbCopy);

                        cbToRead -= cbCopy;
                        off      += cbCopy;
                        OffDiff  += cbCopy;
                        ASMAtomicSubS32(&pTask->cbTransferLeft, cbCopy);
                    }
                }

                /* Move this entry to the top position */
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
            }
            else
            {
                pdmacFileCacheUpdate(pCache, pEntry);
                pdmacFileCacheReplace(pCache, pEntry->cbData, pEntry->pList);

                /* Move the entry to T2 and fetch it to the cache. */
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);

                pEntry->pbData  = (uint8_t *)RTMemPageAlloc(pEntry->cbData);
                AssertPtr(pEntry->pbData);

                while (cbToRead)
                {
                    PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                    AssertMsg(off >= pEntry->Core.Key,
                                ("Overflow in calculation off=%RTfoff OffsetAligned=%RTfoff\n",
                                off, pEntry->Core.Key));

                    pSeg->pTask      = pTask;
                    pSeg->uBufOffset = OffDiff;
                    pSeg->cbTransfer = RT_MIN(cbToRead, cbSegLeft);
                    pSeg->pvBuf      = pbSegBuf;

                    ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                    pSeg->pNext = pEntry->pHead;
                    pEntry->pHead = pSeg;

                    off      += pSeg->cbTransfer;
                    OffDiff  += pSeg->cbTransfer;
                    cbToRead -= pSeg->cbTransfer;
                }

                pdmacFileCacheReadFromEndpoint(pEntry);
            }
        }
        else
        {
            /* No entry found for this offset. Get best fit entry and fetch the data to the cache. */
            STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);
            PPDMACFILECACHEENTRY pEntryBestFit = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetGetBestFit(pEndpointCache->pTree, off, true);
            STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);

            LogFlow(("%sbest fit entry for off=%RTfoff (BestFit=%RTfoff BestFitEnd=%RTfoff BestFitSize=%u)\n",
                     pEntryBestFit ? "" : "No ",
                     off,
                     pEntryBestFit ? pEntryBestFit->Core.Key : 0,
                     pEntryBestFit ? pEntryBestFit->Core.KeyLast : 0,
                     pEntryBestFit ? pEntryBestFit->cbData : 0));

            if (pEntryBestFit && ((off + (RTFOFF)cbRead) > pEntryBestFit->Core.Key))
                cbToRead = pEntryBestFit->Core.Key - off;
            else
                cbToRead = cbRead;

            cbRead -= cbToRead;

            if (!cbRead)
                STAM_COUNTER_INC(&pCache->cMisses);
            else
                STAM_COUNTER_INC(&pCache->cPartialHits);

            size_t cbRemoved = pdmacFileCacheEvict(pCache, cbToRead);

            if (cbRemoved >= cbToRead)
            {
                LogFlow(("Evicted %u bytes (%u requested). Creating new cache entry\n", cbRemoved, cbToRead));
                PPDMACFILECACHEENTRY pEntryNew = (PPDMACFILECACHEENTRY)RTMemAllocZ(sizeof(PDMACFILECACHEENTRY));
                AssertPtr(pEntryNew);

                pEntryNew->Core.Key     = off;
                pEntryNew->Core.KeyLast = off + cbToRead - 1;
                pEntryNew->pEndpoint    = pEndpoint;
                pEntryNew->pCache       = pCache;
                pEntryNew->fFlags       = 0;
                pEntryNew->pList        = NULL;
                pEntryNew->cbData       = cbToRead;
                pEntryNew->pHead        = NULL;
                pEntryNew->pbData       = (uint8_t *)RTMemPageAlloc(cbToRead);
                AssertPtr(pEntryNew->pbData);
                pdmacFileCacheEntryAddToList(&pCache->LruRecentlyUsed, pEntryNew);

                STAM_PROFILE_ADV_START(&pCache->StatTreeInsert, Cache);
                bool fInserted = RTAvlrFileOffsetInsert(pEndpoint->DataCache.pTree, &pEntryNew->Core);
                AssertMsg(fInserted, ("Node was not inserted into tree\n"));
                STAM_PROFILE_ADV_STOP(&pCache->StatTreeInsert, Cache);

                uint32_t uBufOffset = 0;

                pCache->cbCached += cbToRead;

                while (cbToRead)
                {
                    PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                    pSeg->pTask      = pTask;
                    pSeg->uBufOffset = uBufOffset;
                    pSeg->cbTransfer = RT_MIN(cbToRead, cbSegLeft);
                    pSeg->pvBuf      = pbSegBuf;

                    ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                    pSeg->pNext = pEntryNew->pHead;
                    pEntryNew->pHead = pSeg;

                    off        += pSeg->cbTransfer;
                    cbToRead   -= pSeg->cbTransfer;
                    uBufOffset += pSeg->cbTransfer;
                }

                pdmacFileCacheReadFromEndpoint(pEntryNew);
            }
            else
            {
                /*
                 * There is not enough free space in the cache.
                 * Pass the request directly to the I/O manager.
                 */
                LogFlow(("Couldn't evict %u bytes from the cache (%u actually removed). Remaining request will be passed through\n", cbToRead, cbRemoved));

                while (cbToRead)
                {
                    PPDMACTASKFILE pIoTask = pdmacFileTaskAlloc(pEndpoint);
                    AssertPtr(pIoTask);

                    pIoTask->pEndpoint       = pEndpoint;
                    pIoTask->enmTransferType = PDMACTASKFILETRANSFER_READ;
                    pIoTask->Off             = off;
                    pIoTask->DataSeg.cbSeg   = RT_MIN(cbToRead, cbSegLeft);
                    pIoTask->DataSeg.pvSeg   = pbSegBuf;
                    pIoTask->pvUser          = pTask;
                    pIoTask->pfnCompleted    = pdmacFileEpTaskCompleted;

                    off      += pIoTask->DataSeg.cbSeg;
                    cbToRead -= pIoTask->DataSeg.cbSeg;

                    ADVANCE_SEGMENT_BUFFER(pIoTask->DataSeg.cbSeg);

                    /* Send it off to the I/O manager. */
                    pdmacFileEpAddTask(pEndpoint, pIoTask);
                }
            }
        }
    }

    ASMAtomicWriteBool(&pTask->fCompleted, false);

    if (ASMAtomicReadS32(&pTask->cbTransferLeft) == 0
        && !ASMAtomicXchgBool(&pTask->fCompleted, true))
        pdmR3AsyncCompletionCompleteTask(&pTask->Core);

    RTCritSectLeave(&pCache->CritSect);

   return rc;
}

int pdmacFileEpCacheWrite(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask,
                          RTFOFF off, PCPDMDATASEG paSegments, size_t cSegments,
                          size_t cbWrite)
{
    int rc = VINF_SUCCESS;
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    PPDMACFILECACHEENTRY pEntry;

    LogFlowFunc((": pEndpoint=%#p{%s} pTask=%#p off=%RTfoff paSegments=%#p cSegments=%u cbWrite=%u\n",
                 pEndpoint, pEndpoint->Core.pszUri, pTask, off, paSegments, cSegments, cbWrite));

    pTask->cbTransferLeft = cbWrite;
    /* Set to completed to make sure that the task is valid while we access it. */
    ASMAtomicWriteBool(&pTask->fCompleted, true);

    RTCritSectEnter(&pCache->CritSect);

    int iSegCurr       = 0;
    uint8_t *pbSegBuf  = (uint8_t *)paSegments[iSegCurr].pvSeg;
    size_t   cbSegLeft = paSegments[iSegCurr].cbSeg;

    while (cbWrite)
    {
        size_t cbToWrite;

        STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);
        pEntry = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetRangeGet(pEndpointCache->pTree, off);
        STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);

        if (pEntry)
        {
            /* Write the data into the entry and mark it as dirty */
            AssertPtr(pEntry->pList);

            RTFOFF OffDiff = off - pEntry->Core.Key;

            AssertMsg(off >= pEntry->Core.Key,
                      ("Overflow in calculation off=%RTfoff OffsetAligned=%RTfoff\n",
                      off, pEntry->Core.Key));

            cbToWrite = RT_MIN(pEntry->cbData - OffDiff, cbWrite);
            cbWrite  -= cbToWrite;

            if (!cbWrite)
                STAM_COUNTER_INC(&pCache->cHits);
            else
                STAM_COUNTER_INC(&pCache->cPartialHits);

            STAM_COUNTER_ADD(&pCache->StatWritten, cbToWrite);

            /* Ghost lists contain no data. */
            if (   (pEntry->pList == &pCache->LruRecentlyUsed)
                || (pEntry->pList == &pCache->LruFrequentlyUsed))
            {
                if (pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY)
                {
                    AssertMsg(pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                              ("Entry is dirty but not in progress\n"));

                    /* The data isn't written to the file yet */
                    while (cbToWrite)
                    {
                        PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                        pSeg->pTask      = pTask;
                        pSeg->uBufOffset = OffDiff;
                        pSeg->cbTransfer = RT_MIN(cbToWrite, cbSegLeft);
                        pSeg->pvBuf      = pbSegBuf;
                        pSeg->fWrite     = true;

                        ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                        pSeg->pNext = pEntry->pHead;
                        pEntry->pHead = pSeg;

                        off       += pSeg->cbTransfer;
                        OffDiff   += pSeg->cbTransfer;
                        cbToWrite -= pSeg->cbTransfer;
                    }
                }
                else
                {
                    AssertMsg(!(pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS),
                              ("Entry is not dirty but in progress\n"));

                    /* Write as much as we can into the entry and update the file. */
                    while (cbToWrite)
                    {
                        size_t cbCopy = RT_MIN(cbSegLeft, cbToWrite);

                        memcpy(pEntry->pbData + OffDiff, pbSegBuf, cbCopy);

                        ADVANCE_SEGMENT_BUFFER(cbCopy);

                        cbToWrite-= cbCopy;
                        off      += cbCopy;
                        OffDiff  += cbCopy;
                        ASMAtomicSubS32(&pTask->cbTransferLeft, cbCopy);
                    }

                    pEntry->fFlags |= PDMACFILECACHE_ENTRY_IS_DIRTY;
                    pdmacFileCacheWriteToEndpoint(pEntry);
                }

                /* Move this entry to the top position */
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
            }
            else
            {
                pdmacFileCacheUpdate(pCache, pEntry);
                pdmacFileCacheReplace(pCache, pEntry->cbData, pEntry->pList);

                /* Move the entry to T2 and fetch it to the cache. */
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);

                pEntry->pbData  = (uint8_t *)RTMemPageAlloc(pEntry->cbData);
                AssertPtr(pEntry->pbData);

                while (cbToWrite)
                {
                    PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                    AssertMsg(off >= pEntry->Core.Key,
                                ("Overflow in calculation off=%RTfoff OffsetAligned=%RTfoff\n",
                                off, pEntry->Core.Key));

                    pSeg->pTask      = pTask;
                    pSeg->uBufOffset = OffDiff;
                    pSeg->cbTransfer = RT_MIN(cbToWrite, cbSegLeft);
                    pSeg->pvBuf      = pbSegBuf;
                    pSeg->fWrite     = true;

                    ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                    pSeg->pNext = pEntry->pHead;
                    pEntry->pHead = pSeg;

                    off       += pSeg->cbTransfer;
                    OffDiff   += pSeg->cbTransfer;
                    cbToWrite -= pSeg->cbTransfer;
                }

                pdmacFileCacheReadFromEndpoint(pEntry);
            }
        }
        else
        {
            /*
             * No entry found. Write directly into file.
             */
            STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);
            PPDMACFILECACHEENTRY pEntryBestFit = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetGetBestFit(pEndpointCache->pTree, off, true);
            STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);

            LogFlow(("%sbest fit entry for off=%RTfoff (BestFit=%RTfoff BestFitEnd=%RTfoff BestFitSize=%u)\n",
                     pEntryBestFit ? "" : "No ",
                     off,
                     pEntryBestFit ? pEntryBestFit->Core.Key : 0,
                     pEntryBestFit ? pEntryBestFit->Core.KeyLast : 0,
                     pEntryBestFit ? pEntryBestFit->cbData : 0));

            if (pEntryBestFit && ((off + (RTFOFF)cbWrite) > pEntryBestFit->Core.Key))
                cbToWrite = pEntryBestFit->Core.Key - off;
            else
                cbToWrite = cbWrite;

            cbWrite -= cbToWrite;

            while (cbToWrite)
            {
                PPDMACTASKFILE pIoTask = pdmacFileTaskAlloc(pEndpoint);
                AssertPtr(pIoTask);

                pIoTask->pEndpoint       = pEndpoint;
                pIoTask->enmTransferType = PDMACTASKFILETRANSFER_WRITE;
                pIoTask->Off             = off;
                pIoTask->DataSeg.cbSeg   = RT_MIN(cbToWrite, cbSegLeft);
                pIoTask->DataSeg.pvSeg   = pbSegBuf;
                pIoTask->pvUser          = pTask;
                pIoTask->pfnCompleted    = pdmacFileEpTaskCompleted;

                off       += pIoTask->DataSeg.cbSeg;
                cbToWrite -= pIoTask->DataSeg.cbSeg;

                ADVANCE_SEGMENT_BUFFER(pIoTask->DataSeg.cbSeg);

                /* Send it off to the I/O manager. */
                pdmacFileEpAddTask(pEndpoint, pIoTask);
            }
        }
    }

    ASMAtomicWriteBool(&pTask->fCompleted, false);

    if (ASMAtomicReadS32(&pTask->cbTransferLeft) == 0
        && !ASMAtomicXchgBool(&pTask->fCompleted, true))
        pdmR3AsyncCompletionCompleteTask(&pTask->Core);

    RTCritSectLeave(&pCache->CritSect);

    return VINF_SUCCESS;
}

#undef ADVANCE_SEGMENT_BUFFER

