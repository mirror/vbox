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

/** @page pg_pdm_async_completion_cache     PDM Async Completion Cache - The file I/O cache
 * This component implements an I/O cache for file endpoints based on the 2Q cache algorithm.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_ASYNC_COMPLETION
#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <VBox/log.h>
#include <VBox/stam.h>

#include "PDMAsyncCompletionFileInternal.h"

/**
 * A I/O memory context.
 */
typedef struct PDMIOMEMCTX
{
    /** Pointer to the scatter/gather list. */
    PCRTSGSEG      paDataSeg;
    /** Number of segments. */
    size_t         cSegments;
    /** Current segment we are in. */
    unsigned       iSegIdx;
    /** Pointer to the current buffer. */
    uint8_t       *pbBuf;
    /** Number of bytes left in the current buffer. */
    size_t         cbBufLeft;
} PDMIOMEMCTX, *PPDMIOMEMCTX;

#ifdef VBOX_STRICT
# define PDMACFILECACHE_IS_CRITSECT_OWNER(Cache) \
    do \
    { \
     AssertMsg(RTCritSectIsOwner(&Cache->CritSect), \
               ("Thread does not own critical section\n"));\
    } while(0)

# define PDMACFILECACHE_EP_IS_SEMRW_WRITE_OWNER(pEpCache) \
    do \
    { \
        AssertMsg(RTSemRWIsWriteOwner(pEpCache->SemRWEntries), \
                  ("Thread is not exclusive owner of the per endpoint RW semaphore\n")); \
    } while(0)

# define PDMACFILECACHE_EP_IS_SEMRW_READ_OWNER(pEpCache) \
    do \
    { \
        AssertMsg(RTSemRWIsReadOwner(pEpCache->SemRWEntries), \
                  ("Thread is not read owner of the per endpoint RW semaphore\n")); \
    } while(0)

#else
# define PDMACFILECACHE_IS_CRITSECT_OWNER(Cache) do { } while(0)
# define PDMACFILECACHE_EP_IS_SEMRW_WRITE_OWNER(pEpCache) do { } while(0)
# define PDMACFILECACHE_EP_IS_SEMRW_READ_OWNER(pEpCache) do { } while(0)
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void pdmacFileCacheTaskCompleted(PPDMACTASKFILE pTask, void *pvUser, int rc);

/**
 * Decrement the reference counter of the given cache entry.
 *
 * @returns nothing.
 * @param   pEntry    The entry to release.
 */
DECLINLINE(void) pdmacFileEpCacheEntryRelease(PPDMACFILECACHEENTRY pEntry)
{
    AssertMsg(pEntry->cRefs > 0, ("Trying to release a not referenced entry\n"));
    ASMAtomicDecU32(&pEntry->cRefs);
}

/**
 * Increment the reference counter of the given cache entry.
 *
 * @returns nothing.
 * @param   pEntry    The entry to reference.
 */
DECLINLINE(void) pdmacFileEpCacheEntryRef(PPDMACFILECACHEENTRY pEntry)
{
    ASMAtomicIncU32(&pEntry->cRefs);
}

/**
 * Initialize a I/O memory context.
 *
 * @returns nothing
 * @param   pIoMemCtx    Pointer to a unitialized I/O memory context.
 * @param   paDataSeg    Pointer to the S/G list.
 * @param   cSegments    Number of segments in the S/G list.
 */
DECLINLINE(void) pdmIoMemCtxInit(PPDMIOMEMCTX pIoMemCtx, PCRTSGSEG paDataSeg, size_t cSegments)
{
    AssertMsg((cSegments > 0) && paDataSeg, ("Trying to initialize a I/O memory context without a S/G list\n"));

    pIoMemCtx->paDataSeg = paDataSeg;
    pIoMemCtx->cSegments = cSegments;
    pIoMemCtx->iSegIdx   = 0;
    pIoMemCtx->pbBuf     = (uint8_t *)paDataSeg[0].pvSeg;
    pIoMemCtx->cbBufLeft = paDataSeg[0].cbSeg;
}

/**
 * Return a buffer from the I/O memory context.
 *
 * @returns Pointer to the buffer
 * @param   pIoMemCtx    Pointer to the I/O memory context.
 * @param   pcbData      Pointer to the amount of byte requested.
 *                       If the current buffer doesn't have enough bytes left
 *                       the amount is returned in the variable.
 */
DECLINLINE(uint8_t *) pdmIoMemCtxGetBuffer(PPDMIOMEMCTX pIoMemCtx, size_t *pcbData)
{
    size_t cbData = RT_MIN(*pcbData, pIoMemCtx->cbBufLeft);
    uint8_t *pbBuf = pIoMemCtx->pbBuf;

    pIoMemCtx->cbBufLeft -= cbData;

    /* Advance to the next segment if required. */
    if (!pIoMemCtx->cbBufLeft)
    {
        pIoMemCtx->iSegIdx++;

        if (RT_UNLIKELY(pIoMemCtx->iSegIdx == pIoMemCtx->cSegments))
        {
            pIoMemCtx->cbBufLeft = 0;
            pIoMemCtx->pbBuf     = NULL;
        }
        else
        {
            pIoMemCtx->pbBuf     = (uint8_t *)pIoMemCtx->paDataSeg[pIoMemCtx->iSegIdx].pvSeg;
            pIoMemCtx->cbBufLeft = pIoMemCtx->paDataSeg[pIoMemCtx->iSegIdx].cbSeg;
        }

        *pcbData = cbData;
    }
    else
        pIoMemCtx->pbBuf += cbData;

    return pbBuf;
}

#ifdef DEBUG
static void pdmacFileCacheValidate(PPDMACFILECACHEGLOBAL pCache)
{
    /* Amount of cached data should never exceed the maximum amount. */
    AssertMsg(pCache->cbCached <= pCache->cbMax,
              ("Current amount of cached data exceeds maximum\n"));

    /* The amount of cached data in the LRU and FRU list should match cbCached */
    AssertMsg(pCache->LruRecentlyUsedIn.cbCached + pCache->LruFrequentlyUsed.cbCached == pCache->cbCached,
              ("Amount of cached data doesn't match\n"));

    AssertMsg(pCache->LruRecentlyUsedOut.cbCached <= pCache->cbRecentlyUsedOutMax,
              ("Paged out list exceeds maximum\n"));
}
#endif

DECLINLINE(void) pdmacFileCacheLockEnter(PPDMACFILECACHEGLOBAL pCache)
{
    RTCritSectEnter(&pCache->CritSect);
#ifdef DEBUG
    pdmacFileCacheValidate(pCache);
#endif
}

DECLINLINE(void) pdmacFileCacheLockLeave(PPDMACFILECACHEGLOBAL pCache)
{
#ifdef DEBUG
    pdmacFileCacheValidate(pCache);
#endif
    RTCritSectLeave(&pCache->CritSect);
}

DECLINLINE(void) pdmacFileCacheSub(PPDMACFILECACHEGLOBAL pCache, uint32_t cbAmount)
{
    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);
    pCache->cbCached -= cbAmount;
}

DECLINLINE(void) pdmacFileCacheAdd(PPDMACFILECACHEGLOBAL pCache, uint32_t cbAmount)
{
    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);
    pCache->cbCached += cbAmount;
}

DECLINLINE(void) pdmacFileCacheListAdd(PPDMACFILELRULIST pList, uint32_t cbAmount)
{
    pList->cbCached += cbAmount;
}

DECLINLINE(void) pdmacFileCacheListSub(PPDMACFILELRULIST pList, uint32_t cbAmount)
{
    pList->cbCached -= cbAmount;
}

#ifdef PDMACFILECACHE_WITH_LRULIST_CHECKS
/**
 * Checks consistency of a LRU list.
 *
 * @returns nothing
 * @param    pList         The LRU list to check.
 * @param    pNotInList    Element which is not allowed to occur in the list.
 */
static void pdmacFileCacheCheckList(PPDMACFILELRULIST pList, PPDMACFILECACHEENTRY pNotInList)
{
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
}
#endif

/**
 * Unlinks a cache entry from the LRU list it is assigned to.
 *
 * @returns nothing.
 * @param   pEntry    The entry to unlink.
 */
static void pdmacFileCacheEntryRemoveFromList(PPDMACFILECACHEENTRY pEntry)
{
    PPDMACFILELRULIST pList = pEntry->pList;
    PPDMACFILECACHEENTRY pPrev, pNext;

    LogFlowFunc((": Deleting entry %#p from list %#p\n", pEntry, pList));

    AssertPtr(pList);

#ifdef PDMACFILECACHE_WITH_LRULIST_CHECKS
    pdmacFileCacheCheckList(pList, NULL);
#endif

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
    pdmacFileCacheListSub(pList, pEntry->cbData);
#ifdef PDMACFILECACHE_WITH_LRULIST_CHECKS
    pdmacFileCacheCheckList(pList, pEntry);
#endif
}

/**
 * Adds a cache entry to the given LRU list unlinking it from the currently
 * assigned list if needed.
 *
 * @returns nothing.
 * @param    pList    List to the add entry to.
 * @param    pEntry   Entry to add.
 */
static void pdmacFileCacheEntryAddToList(PPDMACFILELRULIST pList, PPDMACFILECACHEENTRY pEntry)
{
    LogFlowFunc((": Adding entry %#p to list %#p\n", pEntry, pList));
#ifdef PDMACFILECACHE_WITH_LRULIST_CHECKS
    pdmacFileCacheCheckList(pList, NULL);
#endif

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
    pdmacFileCacheListAdd(pList, pEntry->cbData);
    pEntry->pList    = pList;
#ifdef PDMACFILECACHE_WITH_LRULIST_CHECKS
    pdmacFileCacheCheckList(pList, NULL);
#endif
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

        RTMemPageFree(pEntry->pbData, pEntry->cbData);
        RTMemFree(pEntry);
    }
}

/**
 * Tries to remove the given amount of bytes from a given list in the cache
 * moving the entries to one of the given ghosts lists
 *
 * @returns Amount of data which could be freed.
 * @param    pCache           Pointer to the global cache data.
 * @param    cbData           The amount of the data to free.
 * @param    pListSrc         The source list to evict data from.
 * @param    pGhostListSrc    The ghost list removed entries should be moved to
 *                            NULL if the entry should be freed.
 * @param    fReuseBuffer     Flag whether a buffer should be reused if it has the same size
 * @param    ppbBuf           Where to store the address of the buffer if an entry with the
 *                            same size was found and fReuseBuffer is true.
 *
 * @note    This function may return fewer bytes than requested because entries
 *          may be marked as non evictable if they are used for I/O at the
 *          moment.
 */
static size_t pdmacFileCacheEvictPagesFrom(PPDMACFILECACHEGLOBAL pCache, size_t cbData,
                                           PPDMACFILELRULIST pListSrc, PPDMACFILELRULIST pGhostListDst,
                                           bool fReuseBuffer, uint8_t **ppbBuffer)
{
    size_t cbEvicted = 0;

    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);

    AssertMsg(cbData > 0, ("Evicting 0 bytes not possible\n"));
    AssertMsg(   !pGhostListDst
              || (pGhostListDst == &pCache->LruRecentlyUsedOut),
              ("Destination list must be NULL or the recently used but paged out list\n"));

    if (fReuseBuffer)
    {
        AssertPtr(ppbBuffer);
        *ppbBuffer = NULL;
    }

    /* Start deleting from the tail. */
    PPDMACFILECACHEENTRY pEntry = pListSrc->pTail;

    while ((cbEvicted < cbData) && pEntry)
    {
        PPDMACFILECACHEENTRY pCurr = pEntry;

        pEntry = pEntry->pPrev;

        /* We can't evict pages which are currently in progress or dirty but not in progress */
        if (   !(pCurr->fFlags & PDMACFILECACHE_NOT_EVICTABLE)
            && (ASMAtomicReadU32(&pCurr->cRefs) == 0))
        {
            /* Ok eviction candidate. Grab the endpoint semaphore and check again
             * because somebody else might have raced us. */
            PPDMACFILEENDPOINTCACHE pEndpointCache = &pCurr->pEndpoint->DataCache;
            RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);

            if (!(pCurr->fFlags & PDMACFILECACHE_NOT_EVICTABLE)
                && (ASMAtomicReadU32(&pCurr->cRefs) == 0))
            {
                LogFlow(("Evicting entry %#p (%u bytes)\n", pCurr, pCurr->cbData));

                if (fReuseBuffer && (pCurr->cbData == cbData))
                {
                    STAM_COUNTER_INC(&pCache->StatBuffersReused);
                    *ppbBuffer = pCurr->pbData;
                }
                else if (pCurr->pbData)
                    RTMemPageFree(pCurr->pbData, pCurr->cbData);

                pCurr->pbData = NULL;
                cbEvicted += pCurr->cbData;

                pdmacFileCacheEntryRemoveFromList(pCurr);
                pdmacFileCacheSub(pCache, pCurr->cbData);

                if (pGhostListDst)
                {
                    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);

                    PPDMACFILECACHEENTRY pGhostEntFree = pGhostListDst->pTail;

                    /* We have to remove the last entries from the paged out list. */
                    while (   ((pGhostListDst->cbCached + pCurr->cbData) > pCache->cbRecentlyUsedOutMax)
                           && pGhostEntFree)
                    {
                        PPDMACFILECACHEENTRY pFree = pGhostEntFree;
                        PPDMACFILEENDPOINTCACHE pEndpointCacheFree = &pFree->pEndpoint->DataCache;

                        pGhostEntFree = pGhostEntFree->pPrev;

                        RTSemRWRequestWrite(pEndpointCacheFree->SemRWEntries, RT_INDEFINITE_WAIT);

                        if (ASMAtomicReadU32(&pFree->cRefs) == 0)
                        {
                            pdmacFileCacheEntryRemoveFromList(pFree);

                            STAM_PROFILE_ADV_START(&pCache->StatTreeRemove, Cache);
                            RTAvlrFileOffsetRemove(pEndpointCacheFree->pTree, pFree->Core.Key);
                            STAM_PROFILE_ADV_STOP(&pCache->StatTreeRemove, Cache);

                            RTMemFree(pFree);
                        }

                        RTSemRWReleaseWrite(pEndpointCacheFree->SemRWEntries);
                    }

                    if (pGhostListDst->cbCached + pCurr->cbData > pCache->cbRecentlyUsedOutMax)
                    {
                        /* Couldn't remove enough entries. Delete */
                        STAM_PROFILE_ADV_START(&pCache->StatTreeRemove, Cache);
                        RTAvlrFileOffsetRemove(pCurr->pEndpoint->DataCache.pTree, pCurr->Core.Key);
                        STAM_PROFILE_ADV_STOP(&pCache->StatTreeRemove, Cache);

                        RTMemFree(pCurr);
                    }
                    else
                        pdmacFileCacheEntryAddToList(pGhostListDst, pCurr);
                }
                else
                {
                    /* Delete the entry from the AVL tree it is assigned to. */
                    STAM_PROFILE_ADV_START(&pCache->StatTreeRemove, Cache);
                    RTAvlrFileOffsetRemove(pCurr->pEndpoint->DataCache.pTree, pCurr->Core.Key);
                    STAM_PROFILE_ADV_STOP(&pCache->StatTreeRemove, Cache);

                    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                    RTMemFree(pCurr);
                }
            }

        }
        else
            LogFlow(("Entry %#p (%u bytes) is still in progress and can't be evicted\n", pCurr, pCurr->cbData));
    }

    return cbEvicted;
}

static bool pdmacFileCacheReclaim(PPDMACFILECACHEGLOBAL pCache, size_t cbData, bool fReuseBuffer, uint8_t **ppbBuffer)
{
    size_t cbRemoved = 0;

    if ((pCache->cbCached + cbData) < pCache->cbMax)
        return true;
    else if ((pCache->LruRecentlyUsedIn.cbCached + cbData) > pCache->cbRecentlyUsedInMax)
    {
        /* Try to evict as many bytes as possible from A1in */
        cbRemoved = pdmacFileCacheEvictPagesFrom(pCache, cbData, &pCache->LruRecentlyUsedIn,
                                                 &pCache->LruRecentlyUsedOut, fReuseBuffer, ppbBuffer);

        /*
         * If it was not possible to remove enough entries
         * try the frequently accessed cache.
         */
        if (cbRemoved < cbData)
        {
            Assert(!fReuseBuffer || !*ppbBuffer); /* It is not possible that we got a buffer with the correct size but we didn't freed enough data. */

            /*
             * If we removed something we can't pass the reuse buffer flag anymore because
             * we don't need to evict that much data
             */
            if (!cbRemoved)
                cbRemoved += pdmacFileCacheEvictPagesFrom(pCache, cbData, &pCache->LruFrequentlyUsed,
                                                          NULL, fReuseBuffer, ppbBuffer);
            else
                cbRemoved += pdmacFileCacheEvictPagesFrom(pCache, cbData - cbRemoved, &pCache->LruFrequentlyUsed,
                                                          NULL, false, NULL);
        }
    }
    else
    {
        /* We have to remove entries from frequently access list. */
        cbRemoved = pdmacFileCacheEvictPagesFrom(pCache, cbData, &pCache->LruFrequentlyUsed,
                                                 NULL, fReuseBuffer, ppbBuffer);
    }

    LogFlowFunc((": removed %u bytes, requested %u\n", cbRemoved, cbData));
    return (cbRemoved >= cbData);
}

/**
 * Initiates a read I/O task for the given entry.
 *
 * @returns nothing.
 * @param   pEntry    The entry to fetch the data to.
 */
static void pdmacFileCacheReadFromEndpoint(PPDMACFILECACHEENTRY pEntry)
{
    LogFlowFunc((": Reading data into cache entry %#p\n", pEntry));

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

/**
 * Initiates a write I/O task for the given entry.
 *
 * @returns nothing.
 * @param    pEntry The entry to read the data from.
 */
static void pdmacFileCacheWriteToEndpoint(PPDMACFILECACHEENTRY pEntry)
{
    LogFlowFunc((": Writing data from cache entry %#p\n", pEntry));

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
    ASMAtomicIncU32(&pEntry->pEndpoint->DataCache.cWritesOutstanding);

    /* Send it off to the I/O manager. */
    pdmacFileEpAddTask(pEntry->pEndpoint, pIoTask);
}

/**
 * Commit a single dirty entry to the endpoint
 *
 * @returns nothing
 * @param   pEntry    The entry to commit.
 */
static void pdmacFileCacheEntryCommit(PPDMACFILEENDPOINTCACHE pEndpointCache, PPDMACFILECACHEENTRY pEntry)
{
    NOREF(pEndpointCache);
    AssertMsg(   (pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY)
              && !(pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS),
              ("Invalid flags set for entry %#p\n", pEntry));

    pdmacFileCacheWriteToEndpoint(pEntry);
}

/**
 * Commit all dirty entries for a single endpoint.
 *
 * @returns nothing.
 * @param   pEndpointCache    The endpoint cache to commit.
 */
static void pdmacFileCacheEndpointCommit(PPDMACFILEENDPOINTCACHE pEndpointCache)
{
    uint32_t cbCommitted = 0;
    RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);

    /* The list is moved to a new header to reduce locking overhead. */
    RTLISTNODE ListDirtyNotCommitted;
    RTSPINLOCKTMP Tmp;

    RTListInit(&ListDirtyNotCommitted);
    RTSpinlockAcquire(pEndpointCache->LockList, &Tmp);
    RTListMove(&ListDirtyNotCommitted, &pEndpointCache->ListDirtyNotCommitted);
    RTSpinlockRelease(pEndpointCache->LockList, &Tmp);

    if (!RTListIsEmpty(&ListDirtyNotCommitted))
    {
        PPDMACFILECACHEENTRY pEntry = RTListNodeGetFirst(&ListDirtyNotCommitted,
                                                         PDMACFILECACHEENTRY,
                                                         NodeNotCommitted);

        while (!RTListNodeIsLast(&ListDirtyNotCommitted, &pEntry->NodeNotCommitted))
        {
            PPDMACFILECACHEENTRY pNext = RTListNodeGetNext(&pEntry->NodeNotCommitted, PDMACFILECACHEENTRY,
                                                           NodeNotCommitted);
            pdmacFileCacheEntryCommit(pEndpointCache, pEntry);
            cbCommitted += pEntry->cbData;
            RTListNodeRemove(&pEntry->NodeNotCommitted);
            pEntry = pNext;
        }

        /* Commit the last endpoint */
        Assert(RTListNodeIsLast(&ListDirtyNotCommitted, &pEntry->NodeNotCommitted));
        pdmacFileCacheEntryCommit(pEndpointCache, pEntry);
        RTListNodeRemove(&pEntry->NodeNotCommitted);
        AssertMsg(RTListIsEmpty(&ListDirtyNotCommitted),
                  ("Committed all entries but list is not empty\n"));
    }

    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
    AssertMsg(pEndpointCache->pCache->cbDirty >= cbCommitted,
              ("Number of committed bytes exceeds number of dirty bytes\n"));
    ASMAtomicSubU32(&pEndpointCache->pCache->cbDirty, cbCommitted);
}

/**
 * Commit all dirty entries in the cache.
 *
 * @returns nothing.
 * @param   pCache    The global cache instance.
 */
static void pdmacFileCacheCommitDirtyEntries(PPDMACFILECACHEGLOBAL pCache)
{
    bool fCommitInProgress = ASMAtomicXchgBool(&pCache->fCommitInProgress, true);

    if (!fCommitInProgress)
    {
        pdmacFileCacheLockEnter(pCache);
        Assert(!RTListIsEmpty(&pCache->ListEndpoints));

        PPDMACFILEENDPOINTCACHE pEndpointCache = RTListNodeGetFirst(&pCache->ListEndpoints,
                                                                    PDMACFILEENDPOINTCACHE,
                                                                    NodeCacheEndpoint);
        AssertPtr(pEndpointCache);

        while (!RTListNodeIsLast(&pCache->ListEndpoints, &pEndpointCache->NodeCacheEndpoint))
        {
            pdmacFileCacheEndpointCommit(pEndpointCache);

            pEndpointCache = RTListNodeGetNext(&pEndpointCache->NodeCacheEndpoint, PDMACFILEENDPOINTCACHE,
                                               NodeCacheEndpoint);
        }

        /* Commit the last endpoint */
        Assert(RTListNodeIsLast(&pCache->ListEndpoints, &pEndpointCache->NodeCacheEndpoint));
        pdmacFileCacheEndpointCommit(pEndpointCache);

        pdmacFileCacheLockLeave(pCache);
        ASMAtomicWriteBool(&pCache->fCommitInProgress, false);
    }
}

/**
 * Adds the given entry as a dirty to the cache.
 *
 * @returns Flag whether the amount of dirty bytes in the cache exceeds the threshold
 * @param   pEndpointCache    The endpoint cache the entry belongs to.
 * @param   pEntry            The entry to add.
 */
static bool pdmacFileCacheAddDirtyEntry(PPDMACFILEENDPOINTCACHE pEndpointCache, PPDMACFILECACHEENTRY pEntry)
{
    bool fDirtyBytesExceeded = false;
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;

    /* If the commit timer is disabled we commit right away. */
    if (pCache->u32CommitTimeoutMs == 0)
    {
        pEntry->fFlags |= PDMACFILECACHE_ENTRY_IS_DIRTY;
        pdmacFileCacheEntryCommit(pEndpointCache, pEntry);
    }
    else if (!(pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY))
    {
        pEntry->fFlags |= PDMACFILECACHE_ENTRY_IS_DIRTY;

        RTSPINLOCKTMP Tmp;
        RTSpinlockAcquire(pEndpointCache->LockList, &Tmp);
        RTListAppend(&pEndpointCache->ListDirtyNotCommitted, &pEntry->NodeNotCommitted);
        RTSpinlockRelease(pEndpointCache->LockList, &Tmp);

        uint32_t cbDirty = ASMAtomicAddU32(&pCache->cbDirty, pEntry->cbData);

        fDirtyBytesExceeded = (cbDirty >= pCache->cbCommitDirtyThreshold);
    }

    return fDirtyBytesExceeded;
}


/**
 * Completes a task segment freeing all ressources and completes the task handle
 * if everything was transfered.
 *
 * @returns Next task segment handle.
 * @param   pTaskSeg          Task segment to complete.
 * @param   rc                Status code to set.
 */
static PPDMACFILETASKSEG pdmacFileCacheTaskComplete(PPDMACFILETASKSEG pTaskSeg, int rc)
{
    PPDMACFILETASKSEG pNext = pTaskSeg->pNext;
    PPDMASYNCCOMPLETIONTASKFILE pTaskFile = pTaskSeg->pTask;

    if (RT_FAILURE(rc))
        ASMAtomicCmpXchgS32(&pTaskFile->rc, rc, VINF_SUCCESS);

    uint32_t uOld = ASMAtomicSubS32(&pTaskFile->cbTransferLeft, pTaskSeg->cbTransfer);
    AssertMsg(uOld >= pTaskSeg->cbTransfer, ("New value would overflow\n"));
    if (!(uOld - pTaskSeg->cbTransfer)
        && !ASMAtomicXchgBool(&pTaskFile->fCompleted, true))
        pdmR3AsyncCompletionCompleteTask(&pTaskFile->Core, pTaskFile->rc, true);

    RTMemFree(pTaskSeg);

    return pNext;
}

/**
 * Completion callback for I/O tasks.
 *
 * @returns nothing.
 * @param    pTask     The completed task.
 * @param    pvUser    Opaque user data.
 * @param    rc        Status code of the completed request.
 */
static void pdmacFileCacheTaskCompleted(PPDMACTASKFILE pTask, void *pvUser, int rc)
{
    PPDMACFILECACHEENTRY pEntry = (PPDMACFILECACHEENTRY)pvUser;
    PPDMACFILECACHEGLOBAL pCache = pEntry->pCache;
    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = pEntry->pEndpoint;
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;

    /* Reference the entry now as we are clearing the I/O in progres flag
     * which protects the entry till now. */
    pdmacFileEpCacheEntryRef(pEntry);

    RTSemRWRequestWrite(pEndpoint->DataCache.SemRWEntries, RT_INDEFINITE_WAIT);
    pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IO_IN_PROGRESS;

    /* Process waiting segment list. The data in entry might have changed inbetween. */
    bool fDirty = false;
    PPDMACFILETASKSEG pComplete = pEntry->pWaitingHead;
    PPDMACFILETASKSEG pCurr     = pComplete;

    AssertMsg((pCurr && pEntry->pWaitingTail) || (!pCurr && !pEntry->pWaitingTail),
                ("The list tail was not updated correctly\n"));
    pEntry->pWaitingTail = NULL;
    pEntry->pWaitingHead = NULL;

    if (pTask->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
    {
        AssertMsg(pEndpointCache->cWritesOutstanding > 0, ("Completed write request but outstanding task count is 0\n"));
        ASMAtomicDecU32(&pEndpointCache->cWritesOutstanding);

        /*
         * An error here is difficult to handle as the original request completed already.
         * The error is logged for now and the VM is paused.
         * If the user continues the entry is written again in the hope
         * the user fixed the problem and the next write succeeds.
         */
        /** @todo r=aeichner: This solution doesn't work
         * The user will get the message but the VM will hang afterwards
         * VMR3Suspend() returns when the VM is suspended but suspending
         * the VM will reopen the images readonly in DrvVD. They are closed first
         * which will close the endpoints. This will block EMT while the
         * I/O manager processes the close request but the IO manager is stuck
         * in the VMR3Suspend call and can't process the request.
         * Another problem is that closing the VM means flushing the cache
         * but the entry failed and will probably fail again.
         * No idea so far how to solve this problem... but the user gets informed
         * at least.
         */
        if (RT_FAILURE(rc))
        {
            LogRel(("I/O cache: Error while writing entry at offset %RTfoff (%u bytes) to file \"%s\"\n",
                    pEntry->Core.Key, pEntry->cbData, pEndpoint->Core.pszUri));

            rc = VMSetRuntimeError(pEndpoint->Core.pEpClass->pVM, 0, "CACHE_IOERR",
                                   N_("The I/O cache encountered an error while updating data in file \"%s\" (rc=%Rrc). Make sure there is enough free space on the disk and that the disk is working properly. Operation can be resumed afterwards."), pEndpoint->Core.pszUri, rc);
            AssertRC(rc);
            rc = VMR3Suspend(pEndpoint->Core.pEpClass->pVM);
        }
        else
        {
            pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IS_DIRTY;

            while (pCurr)
            {
                AssertMsg(pCurr->fWrite, ("Completed write entries should never have read tasks attached\n"));

                memcpy(pEntry->pbData + pCurr->uBufOffset, pCurr->pvBuf, pCurr->cbTransfer);
                fDirty = true;

                pCurr = pCurr->pNext;
            }
        }
    }
    else
    {
        AssertMsg(pTask->enmTransferType == PDMACTASKFILETRANSFER_READ, ("Invalid transfer type\n"));
        AssertMsg(!(pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY),
                  ("Invalid flags set\n"));

        while (pCurr)
        {
            if (pCurr->fWrite)
            {
                memcpy(pEntry->pbData + pCurr->uBufOffset, pCurr->pvBuf, pCurr->cbTransfer);
                fDirty = true;
            }
            else
                memcpy(pCurr->pvBuf, pEntry->pbData + pCurr->uBufOffset, pCurr->cbTransfer);

            pCurr = pCurr->pNext;
        }
    }

    bool fCommit = false;
    if (fDirty)
        fCommit = pdmacFileCacheAddDirtyEntry(pEndpointCache, pEntry);

    /* Complete a pending flush if all writes have completed */
    if (!ASMAtomicReadU32(&pEndpointCache->cWritesOutstanding))
    {
        PPDMASYNCCOMPLETIONTASKFILE pTaskFlush = (PPDMASYNCCOMPLETIONTASKFILE)ASMAtomicXchgPtr((void * volatile *)&pEndpointCache->pTaskFlush, NULL);
        if (pTaskFlush)
            pdmR3AsyncCompletionCompleteTask(&pTaskFlush->Core, VINF_SUCCESS, true);
    }

    RTSemRWReleaseWrite(pEndpoint->DataCache.SemRWEntries);

    /* Dereference so that it isn't protected anymore except we issued anyother write for it. */
    pdmacFileEpCacheEntryRelease(pEntry);

    if (fCommit)
        pdmacFileCacheCommitDirtyEntries(pCache);

    /* Complete waiters now. */
    while (pComplete)
        pComplete = pdmacFileCacheTaskComplete(pComplete, rc);
}

/**
 * Commit timer callback.
 */
static void pdmacFileCacheCommitTimerCallback(PVM pVM, PTMTIMER pTimer, void *pvUser)
{
    PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pvUser;
    PPDMACFILECACHEGLOBAL          pCache = &pClassFile->Cache;

    LogFlowFunc(("Commit interval expired, commiting dirty entries\n"));

    if (ASMAtomicReadU32(&pCache->cbDirty) > 0)
        pdmacFileCacheCommitDirtyEntries(pCache);

    TMTimerSetMillies(pTimer, pCache->u32CommitTimeoutMs);
    LogFlowFunc(("Entries committed, going to sleep\n"));
}

/**
 * Initializies the I/O cache.
 *
 * returns VBox status code.
 * @param    pClassFile    The global class data for file endpoints.
 * @param    pCfgNode      CFGM node to query configuration data from.
 */
int pdmacFileCacheInit(PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile, PCFGMNODE pCfgNode)
{
    int rc = VINF_SUCCESS;
    PPDMACFILECACHEGLOBAL pCache = &pClassFile->Cache;

    rc = CFGMR3QueryU32Def(pCfgNode, "CacheSize", &pCache->cbMax, 5 * _1M);
    AssertLogRelRCReturn(rc, rc);

    RTListInit(&pCache->ListEndpoints);
    pCache->cRefs = 0;
    pCache->cbCached  = 0;
    pCache->fCommitInProgress = 0;
    LogFlowFunc((": Maximum number of bytes cached %u\n", pCache->cbMax));

    /* Initialize members */
    pCache->LruRecentlyUsedIn.pHead    = NULL;
    pCache->LruRecentlyUsedIn.pTail    = NULL;
    pCache->LruRecentlyUsedIn.cbCached = 0;

    pCache->LruRecentlyUsedOut.pHead    = NULL;
    pCache->LruRecentlyUsedOut.pTail    = NULL;
    pCache->LruRecentlyUsedOut.cbCached = 0;

    pCache->LruFrequentlyUsed.pHead    = NULL;
    pCache->LruFrequentlyUsed.pTail    = NULL;
    pCache->LruFrequentlyUsed.cbCached = 0;

    pCache->cbRecentlyUsedInMax  = (pCache->cbMax / 100) * 25; /* 25% of the buffer size */
    pCache->cbRecentlyUsedOutMax = (pCache->cbMax / 100) * 50; /* 50% of the buffer size */
    LogFlowFunc((": cbRecentlyUsedInMax=%u cbRecentlyUsedOutMax=%u\n", pCache->cbRecentlyUsedInMax, pCache->cbRecentlyUsedOutMax));

    /** @todo r=aeichner: Experiment to find optimal default values */
    rc = CFGMR3QueryU32Def(pCfgNode, "CacheCommitIntervalMs", &pCache->u32CommitTimeoutMs, 10000 /* 10sec */);
    AssertLogRelRCReturn(rc, rc);
    rc = CFGMR3QueryU32(pCfgNode, "CacheCommitThreshold", &pCache->cbCommitDirtyThreshold);
    if (   rc == VERR_CFGM_VALUE_NOT_FOUND
        || rc == VERR_CFGM_NO_PARENT)
    {
        /* Start committing after 50% of the cache are dirty */
        pCache->cbCommitDirtyThreshold = pCache->cbMax / 2;
    }
    else
        return rc;

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
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruRecentlyUsedIn.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedMruIn",
                   STAMUNIT_BYTES,
                   "Number of bytes cached in MRU list");
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruRecentlyUsedOut.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedMruOut",
                   STAMUNIT_BYTES,
                   "Number of bytes cached in FRU list");
    STAMR3Register(pClassFile->Core.pVM, &pCache->LruFrequentlyUsed.cbCached,
                   STAMTYPE_U32, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/cbCachedFru",
                   STAMUNIT_BYTES,
                   "Number of bytes cached in FRU ghost list");

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
    STAMR3Register(pClassFile->Core.pVM, &pCache->StatBuffersReused,
                   STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                   "/PDM/AsyncCompletion/File/CacheBuffersReused",
                   STAMUNIT_COUNT, "Number of times a buffer could be reused");
#endif

    /* Initialize the critical section */
    rc = RTCritSectInit(&pCache->CritSect);

    if (RT_SUCCESS(rc))
    {
        /* Create the commit timer */
        if (pCache->u32CommitTimeoutMs > 0)
            rc = TMR3TimerCreateInternal(pClassFile->Core.pVM, TMCLOCK_REAL,
                                         pdmacFileCacheCommitTimerCallback,
                                         pClassFile,
                                         "Cache-Commit",
                                         &pClassFile->Cache.pTimerCommit);

        if (RT_SUCCESS(rc))
        {
            LogRel(("AIOMgr: Cache successfully initialised. Cache size is %u bytes\n", pCache->cbMax));
            LogRel(("AIOMgr: Cache commit interval is %u ms\n", pCache->u32CommitTimeoutMs));
            LogRel(("AIOMgr: Cache commit threshold is %u bytes\n", pCache->cbCommitDirtyThreshold));
            return VINF_SUCCESS;
        }

        RTCritSectDelete(&pCache->CritSect);
    }

    return rc;
}

/**
 * Destroysthe cache freeing all data.
 *
 * returns nothing.
 * @param    pClassFile    The global class data for file endpoints.
 */
void pdmacFileCacheDestroy(PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile)
{
    PPDMACFILECACHEGLOBAL pCache = &pClassFile->Cache;

    /* Make sure no one else uses the cache now */
    pdmacFileCacheLockEnter(pCache);

    /* Cleanup deleting all cache entries waiting for in progress entries to finish. */
    pdmacFileCacheDestroyList(&pCache->LruRecentlyUsedIn);
    pdmacFileCacheDestroyList(&pCache->LruRecentlyUsedOut);
    pdmacFileCacheDestroyList(&pCache->LruFrequentlyUsed);

    pdmacFileCacheLockLeave(pCache);

    RTCritSectDelete(&pCache->CritSect);
}

/**
 * Initializes per endpoint cache data
 * like the AVL tree used to access cached entries.
 *
 * @returns VBox status code.
 * @param    pEndpoint     The endpoint to init the cache for,
 * @param    pClassFile    The global class data for file endpoints.
 */
int pdmacFileEpCacheInit(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONEPCLASSFILE pClassFile)
{
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;

    pEndpointCache->pCache = &pClassFile->Cache;
    RTListInit(&pEndpointCache->ListDirtyNotCommitted);
    int rc = RTSpinlockCreate(&pEndpointCache->LockList);

    if (RT_SUCCESS(rc))
    {
        rc = RTSemRWCreate(&pEndpointCache->SemRWEntries);
        if (RT_SUCCESS(rc))
        {
            pEndpointCache->pTree  = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
            if (pEndpointCache->pTree)
            {
                pClassFile->Cache.cRefs++;
                RTListAppend(&pClassFile->Cache.ListEndpoints, &pEndpointCache->NodeCacheEndpoint);

                /* Arm the timer if this is the first endpoint. */
                if (   pClassFile->Cache.cRefs == 1
                    && pClassFile->Cache.u32CommitTimeoutMs > 0)
                    rc = TMTimerSetMillies(pClassFile->Cache.pTimerCommit, pClassFile->Cache.u32CommitTimeoutMs);
            }
            else
                rc = VERR_NO_MEMORY;

            if (RT_FAILURE(rc))
                RTSemRWDestroy(pEndpointCache->SemRWEntries);
        }

        if (RT_FAILURE(rc))
            RTSpinlockDestroy(pEndpointCache->LockList);
    }

#ifdef VBOX_WITH_STATISTICS
    if (RT_SUCCESS(rc))
    {
        STAMR3RegisterF(pClassFile->Core.pVM, &pEndpointCache->StatWriteDeferred,
                       STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS,
                       STAMUNIT_COUNT, "Number of deferred writes",
                       "/PDM/AsyncCompletion/File/%s/Cache/DeferredWrites", RTPathFilename(pEndpoint->Core.pszUri));
    }
#endif

    LogFlowFunc(("Leave rc=%Rrc\n", rc));
    return rc;
}

/**
 * Callback for the AVL destroy routine. Frees a cache entry for this endpoint.
 *
 * @returns IPRT status code.
 * @param    pNode     The node to destroy.
 * @param    pvUser    Opaque user data.
 */
static int pdmacFileEpCacheEntryDestroy(PAVLRFOFFNODECORE pNode, void *pvUser)
{
    PPDMACFILECACHEENTRY  pEntry = (PPDMACFILECACHEENTRY)pNode;
    PPDMACFILECACHEGLOBAL pCache = (PPDMACFILECACHEGLOBAL)pvUser;
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEntry->pEndpoint->DataCache;

    while (ASMAtomicReadU32(&pEntry->fFlags) & (PDMACFILECACHE_ENTRY_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DIRTY))
    {
        RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
        RTThreadSleep(250);
        RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    }

    AssertMsg(!(pEntry->fFlags & (PDMACFILECACHE_ENTRY_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DIRTY)),
                ("Entry is dirty and/or still in progress fFlags=%#x\n", pEntry->fFlags));

    bool fUpdateCache =    pEntry->pList == &pCache->LruFrequentlyUsed
                        || pEntry->pList == &pCache->LruRecentlyUsedIn;

    pdmacFileCacheEntryRemoveFromList(pEntry);

    if (fUpdateCache)
        pdmacFileCacheSub(pCache, pEntry->cbData);

    RTMemPageFree(pEntry->pbData, pEntry->cbData);
    RTMemFree(pEntry);

    return VINF_SUCCESS;
}

/**
 * Destroys all cache ressources used by the given endpoint.
 *
 * @returns nothing.
 * @param    pEndpoint    The endpoint to the destroy.
 */
void pdmacFileEpCacheDestroy(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint)
{
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;
    PPDMACFILECACHEGLOBAL   pCache         = pEndpointCache->pCache;

    /* Make sure nobody is accessing the cache while we delete the tree. */
    pdmacFileCacheLockEnter(pCache);
    RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    RTAvlrFileOffsetDestroy(pEndpointCache->pTree, pdmacFileEpCacheEntryDestroy, pCache);
    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);

    RTSpinlockDestroy(pEndpointCache->LockList);

    pCache->cRefs--;
    RTListNodeRemove(&pEndpointCache->NodeCacheEndpoint);

    if (   !pCache->cRefs
        && pCache->u32CommitTimeoutMs > 0)
        TMTimerStop(pCache->pTimerCommit);

    pdmacFileCacheLockLeave(pCache);

    RTSemRWDestroy(pEndpointCache->SemRWEntries);

#ifdef VBOX_WITH_STATISTICS
    PPDMASYNCCOMPLETIONEPCLASSFILE pEpClassFile = (PPDMASYNCCOMPLETIONEPCLASSFILE)pEndpoint->Core.pEpClass;

    STAMR3Deregister(pEpClassFile->Core.pVM, &pEndpointCache->StatWriteDeferred);
#endif
}

static PPDMACFILECACHEENTRY pdmacFileEpCacheGetCacheEntryByOffset(PPDMACFILEENDPOINTCACHE pEndpointCache, RTFOFF off)
{
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    PPDMACFILECACHEENTRY pEntry = NULL;

    STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);

    RTSemRWRequestRead(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    pEntry = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetRangeGet(pEndpointCache->pTree, off);
    if (pEntry)
        pdmacFileEpCacheEntryRef(pEntry);
    RTSemRWReleaseRead(pEndpointCache->SemRWEntries);

    STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);

    return pEntry;
}

/**
 * Return the best fit cache entries for the given offset.
 *
 * @returns nothing.
 * @param   pEndpointCache    The endpoint cache.
 * @param   off               The offset.
 * @param   pEntryAbove       Where to store the pointer to the best fit entry above the
 *                            the given offset. NULL if not required.
 * @param   pEntryBelow       Where to store the pointer to the best fit entry below the
 *                            the given offset. NULL if not required.
 */
static void pdmacFileEpCacheGetCacheBestFitEntryByOffset(PPDMACFILEENDPOINTCACHE pEndpointCache, RTFOFF off,
                                                         PPDMACFILECACHEENTRY *ppEntryAbove,
                                                         PPDMACFILECACHEENTRY *ppEntryBelow)
{
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;

    STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);

    RTSemRWRequestRead(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    if (ppEntryAbove)
    {
        *ppEntryAbove = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetGetBestFit(pEndpointCache->pTree, off, true /*fAbove*/);
        if (*ppEntryAbove)
            pdmacFileEpCacheEntryRef(*ppEntryAbove);
    }

    if (ppEntryBelow)
    {
        *ppEntryBelow = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetGetBestFit(pEndpointCache->pTree, off, false /*fAbove*/);
        if (*ppEntryBelow)
            pdmacFileEpCacheEntryRef(*ppEntryBelow);
    }
    RTSemRWReleaseRead(pEndpointCache->SemRWEntries);

    STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);
}

static void pdmacFileEpCacheInsertEntry(PPDMACFILEENDPOINTCACHE pEndpointCache, PPDMACFILECACHEENTRY pEntry)
{
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;

    STAM_PROFILE_ADV_START(&pCache->StatTreeInsert, Cache);
    RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    bool fInserted = RTAvlrFileOffsetInsert(pEndpointCache->pTree, &pEntry->Core);
    AssertMsg(fInserted, ("Node was not inserted into tree\n"));
    STAM_PROFILE_ADV_STOP(&pCache->StatTreeInsert, Cache);
    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
}

/**
 * Allocates and initializes a new entry for the cache.
 * The entry has a reference count of 1.
 *
 * @returns Pointer to the new cache entry or NULL if out of memory.
 * @param   pCache    The cache the entry belongs to.
 * @param   pEndoint  The endpoint the entry holds data for.
 * @param   off       Start offset.
 * @param   cbData    Size of the cache entry.
 * @param   pbBuffer  Pointer to the buffer to use.
 *                    NULL if a new buffer should be allocated.
 *                    The buffer needs to have the same size of the entry.
 */
static PPDMACFILECACHEENTRY pdmacFileCacheEntryAlloc(PPDMACFILECACHEGLOBAL pCache,
                                                     PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                                                     RTFOFF off, size_t cbData, uint8_t *pbBuffer)
{
    PPDMACFILECACHEENTRY pEntryNew = (PPDMACFILECACHEENTRY)RTMemAllocZ(sizeof(PDMACFILECACHEENTRY));

    if (RT_UNLIKELY(!pEntryNew))
        return NULL;

    pEntryNew->Core.Key      = off;
    pEntryNew->Core.KeyLast  = off + cbData - 1;
    pEntryNew->pEndpoint     = pEndpoint;
    pEntryNew->pCache        = pCache;
    pEntryNew->fFlags        = 0;
    pEntryNew->cRefs         = 1; /* We are using it now. */
    pEntryNew->pList         = NULL;
    pEntryNew->cbData        = cbData;
    pEntryNew->pWaitingHead  = NULL;
    pEntryNew->pWaitingTail  = NULL;
    if (pbBuffer)
        pEntryNew->pbData    = pbBuffer;
    else
        pEntryNew->pbData    = (uint8_t *)RTMemPageAlloc(cbData);

    if (RT_UNLIKELY(!pEntryNew->pbData))
    {
        RTMemFree(pEntryNew);
        return NULL;
    }

    return pEntryNew;
}

/**
 * Adds a segment to the waiting list for a cache entry
 * which is currently in progress.
 *
 * @returns nothing.
 * @param   pEntry    The cache entry to add the segment to.
 * @param   pSeg      The segment to add.
 */
DECLINLINE(void) pdmacFileEpCacheEntryAddWaitingSegment(PPDMACFILECACHEENTRY pEntry, PPDMACFILETASKSEG pSeg)
{
    pSeg->pNext = NULL;

    if (pEntry->pWaitingHead)
    {
        AssertPtr(pEntry->pWaitingTail);

        pEntry->pWaitingTail->pNext = pSeg;
        pEntry->pWaitingTail = pSeg;
    }
    else
    {
        Assert(!pEntry->pWaitingTail);

        pEntry->pWaitingHead = pSeg;
        pEntry->pWaitingTail = pSeg;
    }
}

/**
 * Checks that a set of flags is set/clear acquiring the R/W semaphore
 * in exclusive mode.
 *
 * @returns true if the flag in fSet is set and the one in fClear is clear.
 *          false othwerise.
 *          The R/W semaphore is only held if true is returned.
 *
 * @param   pEndpointCache   The endpoint cache instance data.
 * @param   pEntry           The entry to check the flags for.
 * @param   fSet             The flag which is tested to be set.
 * @param   fClear           The flag which is tested to be clear.
 */
DECLINLINE(bool) pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(PPDMACFILEENDPOINTCACHE pEndpointCache,
                                                                PPDMACFILECACHEENTRY pEntry,
                                                                uint32_t fSet, uint32_t fClear)
{
    uint32_t fFlags = ASMAtomicReadU32(&pEntry->fFlags);
    bool fPassed = ((fFlags & fSet) && !(fFlags & fClear));

    if (fPassed)
    {
        /* Acquire the lock and check again becuase the completion callback might have raced us. */
        RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);

        fFlags = ASMAtomicReadU32(&pEntry->fFlags);
        fPassed = ((fFlags & fSet) && !(fFlags & fClear));

        /* Drop the lock if we didn't passed the test. */
        if (!fPassed)
            RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
    }

    return fPassed;
}

/**
 * Copies data to a buffer described by a I/O memory context.
 *
 * @returns nothing.
 * @param   pIoMemCtx    The I/O memory context to copy the data into.
 * @param   pbData       Pointer to the data data to copy.
 * @param   cbData       Amount of data to copy.
 */
static void pdmacFileEpCacheCopyToIoMemCtx(PPDMIOMEMCTX pIoMemCtx,
                                           uint8_t *pbData,
                                           size_t cbData)
{
    while (cbData)
    {
        size_t cbCopy = cbData;
        uint8_t *pbBuf = pdmIoMemCtxGetBuffer(pIoMemCtx, &cbCopy);

        AssertPtr(pbBuf);

        memcpy(pbBuf, pbData, cbCopy);

        cbData -= cbCopy;
        pbData += cbCopy;
    }
}

/**
 * Copies data from a buffer described by a I/O memory context.
 *
 * @returns nothing.
 * @param   pIoMemCtx    The I/O memory context to copy the data from.
 * @param   pbData       Pointer to the destination buffer.
 * @param   cbData       Amount of data to copy.
 */
static void pdmacFileEpCacheCopyFromIoMemCtx(PPDMIOMEMCTX pIoMemCtx,
                                             uint8_t *pbData,
                                             size_t cbData)
{
    while (cbData)
    {
        size_t cbCopy = cbData;
        uint8_t *pbBuf = pdmIoMemCtxGetBuffer(pIoMemCtx, &cbCopy);

        AssertPtr(pbBuf);

        memcpy(pbData, pbBuf, cbCopy);

        cbData -= cbCopy;
        pbData += cbCopy;
    }
}

/**
 * Add a buffer described by the I/O memory context
 * to the entry waiting for completion.
 *
 * @returns nothing.
 * @param   pEntry    The entry to add the buffer to.
 * @param   pTask     Task associated with the buffer.
 * @param   pIoMemCtx The memory context to use.
 * @param   OffDiff   Offset from the start of the buffer
 *                    in the entry.
 * @param   cbData    Amount of data to wait for onthis entry.
 * @param   fWrite    Flag whether the task waits because it wants to write
 *                    to the cache entry.
 */
static void pdmacFileEpCacheEntryWaitersAdd(PPDMACFILECACHEENTRY pEntry,
                                            PPDMASYNCCOMPLETIONTASKFILE pTask,
                                            PPDMIOMEMCTX pIoMemCtx,
                                            RTFOFF OffDiff,
                                            size_t cbData,
                                            bool fWrite)
{
    while (cbData)
    {
        PPDMACFILETASKSEG pSeg  = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));
        size_t            cbSeg = cbData;
        uint8_t          *pbBuf = pdmIoMemCtxGetBuffer(pIoMemCtx, &cbSeg);

        pSeg->pTask      = pTask;
        pSeg->uBufOffset = OffDiff;
        pSeg->cbTransfer = cbSeg;
        pSeg->pvBuf      = pbBuf;
        pSeg->fWrite     = fWrite;

        pdmacFileEpCacheEntryAddWaitingSegment(pEntry, pSeg);

        cbData   -= cbSeg;
        OffDiff  += cbSeg;
    }
}

/**
 * Passthrough a part of a request directly to the I/O manager
 * handling the endpoint.
 *
 * @returns nothing.
 * @param   pEndpoint          The endpoint.
 * @param   pTask              The task.
 * @param   pIoMemCtx          The I/O memory context to use.
 * @param   offStart           Offset to start transfer from.
 * @param   cbData             Amount of data to transfer.
 * @param   enmTransferType    The transfer type (read/write)
 */
static void pdmacFileEpCacheRequestPassthrough(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                                               PPDMASYNCCOMPLETIONTASKFILE pTask,
                                               PPDMIOMEMCTX pIoMemCtx,
                                               RTFOFF offStart, size_t cbData,
                                               PDMACTASKFILETRANSFER enmTransferType)
{
    while (cbData)
    {
        size_t         cbSeg = cbData;
        uint8_t       *pbBuf = pdmIoMemCtxGetBuffer(pIoMemCtx, &cbSeg);
        PPDMACTASKFILE pIoTask = pdmacFileTaskAlloc(pEndpoint);
        AssertPtr(pIoTask);

        pIoTask->pEndpoint       = pEndpoint;
        pIoTask->enmTransferType = enmTransferType;
        pIoTask->Off             = offStart;
        pIoTask->DataSeg.cbSeg   = cbSeg;
        pIoTask->DataSeg.pvSeg   = pbBuf;
        pIoTask->pvUser          = pTask;
        pIoTask->pfnCompleted    = pdmacFileEpTaskCompleted;

        offStart += cbSeg;
        cbData   -= cbSeg;

        /* Send it off to the I/O manager. */
        pdmacFileEpAddTask(pEndpoint, pIoTask);
    }
}

/**
 * Calculate aligned offset and size for a new cache entry
 * which do not intersect with an already existing entry and the
 * file end.
 *
 * @returns The number of bytes the entry can hold of the requested amount
 *          of byte.
 * @param   pEndpoint        The endpoint.
 * @param   pEndpointCache   The endpoint cache.
 * @param   off              The start offset.
 * @param   cb               The number of bytes the entry needs to hold at least.
 * @param   uAlignment       Alignment of the boundary sizes.
 * @param   poffAligned      Where to store the aligned offset.
 * @param   pcbAligned       Where to store the aligned size of the entry.
 */
static size_t pdmacFileEpCacheEntryBoundariesCalc(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                                                  PPDMACFILEENDPOINTCACHE pEndpointCache,
                                                  RTFOFF off, size_t cb,
                                                  unsigned uAlignment,
                                                  RTFOFF *poffAligned, size_t *pcbAligned)
{
    size_t cbAligned;
    size_t cbInEntry = 0;
    RTFOFF offAligned;
    PPDMACFILECACHEENTRY pEntryAbove = NULL;
    PPDMACFILECACHEENTRY pEntryBelow = NULL;

    /* Get the best fit entries around the offset */
    pdmacFileEpCacheGetCacheBestFitEntryByOffset(pEndpointCache, off,
                                                 &pEntryAbove, &pEntryBelow);

    /* Log the info */
    LogFlow(("%sest fit entry below off=%RTfoff (BestFit=%RTfoff BestFitEnd=%RTfoff BestFitSize=%u)\n",
             pEntryBelow ? "B" : "No b",
             off,
             pEntryBelow ? pEntryBelow->Core.Key : 0,
             pEntryBelow ? pEntryBelow->Core.KeyLast : 0,
             pEntryBelow ? pEntryBelow->cbData : 0));

    LogFlow(("%sest fit entry above off=%RTfoff (BestFit=%RTfoff BestFitEnd=%RTfoff BestFitSize=%u)\n",
             pEntryAbove ? "B" : "No b",
             off,
             pEntryAbove ? pEntryAbove->Core.Key : 0,
             pEntryAbove ? pEntryAbove->Core.KeyLast : 0,
             pEntryAbove ? pEntryAbove->cbData : 0));

    /* Align the offset first. */
    offAligned = off & ~(RTFOFF)(512-1);
    if (   pEntryBelow
        && offAligned <= pEntryBelow->Core.KeyLast)
        offAligned = pEntryBelow->Core.KeyLast;

    if (    pEntryAbove
        &&  off + (RTFOFF)cb > pEntryAbove->Core.Key)
    {
        cbInEntry = pEntryAbove->Core.Key - off;
        cbAligned = pEntryAbove->Core.Key - offAligned;
    }
    else
    {
        /*
         * Align the size to a 4KB boundary.
         * Memory size is aligned to a page boundary
         * and memory is wasted if the size is rather small.
         * (For example reads with a size of 512 bytes).
         */
        cbInEntry = cb;
        cbAligned = RT_ALIGN_Z(cb + (off - offAligned), uAlignment);

        /*
         * Clip to file size if the original request doesn't
         * exceed the file (not an appending write)
         */
        uint64_t cbReq = off + (RTFOFF)cb;
        if (cbReq >= pEndpoint->cbFile)
            cbAligned = cbReq - offAligned;
        else
            cbAligned = RT_MIN(pEndpoint->cbFile - offAligned, cbAligned);
        if (pEntryAbove)
        {
            Assert(pEntryAbove->Core.Key >= off);
            cbAligned = RT_MIN(cbAligned, (uint64_t)pEntryAbove->Core.Key - offAligned);
        }
    }

    /* A few sanity checks */
    AssertMsg(!pEntryBelow || pEntryBelow->Core.KeyLast < offAligned,
              ("Aligned start offset intersects with another cache entry\n"));
    AssertMsg(!pEntryAbove || (offAligned + (RTFOFF)cbAligned) <= pEntryAbove->Core.Key,
              ("Aligned size intersects with another cache entry\n"));
    Assert(cbInEntry <= cbAligned);
    AssertMsg(   (   offAligned + (RTFOFF)cbAligned <= (RTFOFF)pEndpoint->cbFile
                  && off + (RTFOFF)cb <= (RTFOFF)pEndpoint->cbFile)
              || (offAligned + (RTFOFF)cbAligned <= off + (RTFOFF)cb),
               ("Unwanted file size increase\n"));

    if (pEntryBelow)
        pdmacFileEpCacheEntryRelease(pEntryBelow);
    if (pEntryAbove)
        pdmacFileEpCacheEntryRelease(pEntryAbove);

    LogFlow(("offAligned=%RTfoff cbAligned=%u\n", offAligned, cbAligned));

    *poffAligned = offAligned;
    *pcbAligned  = cbAligned;

    return cbInEntry;
}

/**
 * Create a new cache entry evicting data from the cache if required.
 *
 * @returns Pointer to the new cache entry or NULL
 *          if not enough bytes could be evicted from the cache.
 * @param   pEndpoint         The endpoint.
 * @param   pEndpointCache    The endpoint cache.
 * @param   off               The offset.
 * @param   cb                Number of bytes the cache entry should have.
 * @param   uAlignment        Alignment the size of the entry should have.
 * @param   pcbData           Where to store the number of bytes the new
 *                            entry can hold. May be lower than actually requested
 *                            due to another entry intersecting the access range.
 */
static PPDMACFILECACHEENTRY pdmacFileEpCacheEntryCreate(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                                                        PPDMACFILEENDPOINTCACHE pEndpointCache,
                                                        RTFOFF off, size_t cb,
                                                        unsigned uAlignment,
                                                        size_t *pcbData)
{
    RTFOFF offStart = 0;
    size_t cbEntry = 0;
    PPDMACFILECACHEENTRY pEntryNew = NULL;
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    uint8_t *pbBuffer = NULL;

    *pcbData = pdmacFileEpCacheEntryBoundariesCalc(pEndpoint,
                                                   pEndpointCache,
                                                   off, cb,
                                                   uAlignment,
                                                   &offStart, &cbEntry);

    pdmacFileCacheLockEnter(pCache);
    bool fEnough = pdmacFileCacheReclaim(pCache, cbEntry, true, &pbBuffer);

    if (fEnough)
    {
        LogFlow(("Evicted enough bytes (%u requested). Creating new cache entry\n", cbEntry));

        pEntryNew = pdmacFileCacheEntryAlloc(pCache, pEndpoint,
                                             offStart, cbEntry,
                                             pbBuffer);
        if (RT_LIKELY(pEntryNew))
        {
            pdmacFileCacheEntryAddToList(&pCache->LruRecentlyUsedIn, pEntryNew);
            pdmacFileCacheAdd(pCache, cbEntry);
            pdmacFileCacheLockLeave(pCache);

            pdmacFileEpCacheInsertEntry(pEndpointCache, pEntryNew);

            AssertMsg(   (off >= pEntryNew->Core.Key)
                      && (off + (RTFOFF)*pcbData <= pEntryNew->Core.KeyLast + 1),
                      ("Overflow in calculation off=%RTfoff OffsetAligned=%RTfoff\n",
                       off, pEntryNew->Core.Key));
        }
        else
            pdmacFileCacheLockLeave(pCache);
    }
    else
        pdmacFileCacheLockLeave(pCache);

    return pEntryNew;
}

/**
 * Reads the specified data from the endpoint using the cache if possible.
 *
 * @returns VBox status code.
 * @param    pEndpoint     The endpoint to read from.
 * @param    pTask         The task structure used as identifier for this request.
 * @param    off           The offset to start reading from.
 * @param    paSegments    Pointer to the array holding the destination buffers.
 * @param    cSegments     Number of segments in the array.
 * @param    cbRead        Number of bytes to read.
 */
int pdmacFileEpCacheRead(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask,
                         RTFOFF off, PCRTSGSEG paSegments, size_t cSegments,
                         size_t cbRead)
{
    int rc = VINF_SUCCESS;
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    PPDMACFILECACHEENTRY pEntry;

    LogFlowFunc((": pEndpoint=%#p{%s} pTask=%#p off=%RTfoff paSegments=%#p cSegments=%u cbRead=%u\n",
                 pEndpoint, pEndpoint->Core.pszUri, pTask, off, paSegments, cSegments, cbRead));

    pTask->cbTransferLeft = cbRead;
    pTask->rc             = VINF_SUCCESS;
    /* Set to completed to make sure that the task is valid while we access it. */
    ASMAtomicWriteBool(&pTask->fCompleted, true);

    /* Init the I/O memory context */
    PDMIOMEMCTX IoMemCtx;
    pdmIoMemCtxInit(&IoMemCtx, paSegments, cSegments);

    while (cbRead)
    {
        size_t cbToRead;

        pEntry = pdmacFileEpCacheGetCacheEntryByOffset(pEndpointCache, off);

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

            AssertMsg(off + (RTFOFF)cbToRead <= pEntry->Core.Key + pEntry->Core.KeyLast + 1,
                      ("Buffer of cache entry exceeded off=%RTfoff cbToRead=%d\n",
                       off, cbToRead));

            cbRead  -= cbToRead;

            if (!cbRead)
                STAM_COUNTER_INC(&pCache->cHits);
            else
                STAM_COUNTER_INC(&pCache->cPartialHits);

            STAM_COUNTER_ADD(&pCache->StatRead, cbToRead);

            /* Ghost lists contain no data. */
            if (   (pEntry->pList == &pCache->LruRecentlyUsedIn)
                || (pEntry->pList == &pCache->LruFrequentlyUsed))
            {
                if (pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                    PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                                                                    PDMACFILECACHE_ENTRY_IS_DIRTY))
                {
                    /* Entry didn't completed yet. Append to the list */
                    pdmacFileEpCacheEntryWaitersAdd(pEntry, pTask,
                                                    &IoMemCtx,
                                                    OffDiff, cbToRead,
                                                    false /* fWrite */);
                    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                }
                else
                {
                    /* Read as much as we can from the entry. */
                    pdmacFileEpCacheCopyToIoMemCtx(&IoMemCtx, pEntry->pbData + OffDiff, cbToRead);
                    ASMAtomicSubS32(&pTask->cbTransferLeft, cbToRead);
                }

                /* Move this entry to the top position */
                if (pEntry->pList == &pCache->LruFrequentlyUsed)
                {
                    pdmacFileCacheLockEnter(pCache);
                    pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                    pdmacFileCacheLockLeave(pCache);
                }
                /* Release the entry */
                pdmacFileEpCacheEntryRelease(pEntry);
            }
            else
            {
                uint8_t *pbBuffer = NULL;

                LogFlow(("Fetching data for ghost entry %#p from file\n", pEntry));

                pdmacFileCacheLockEnter(pCache);
                pdmacFileCacheEntryRemoveFromList(pEntry); /* Remove it before we remove data, otherwise it may get freed when evicting data. */
                bool fEnough = pdmacFileCacheReclaim(pCache, pEntry->cbData, true, &pbBuffer);

                /* Move the entry to Am and fetch it to the cache. */
                if (fEnough)
                {
                    pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                    pdmacFileCacheAdd(pCache, pEntry->cbData);
                    pdmacFileCacheLockLeave(pCache);

                    if (pbBuffer)
                        pEntry->pbData = pbBuffer;
                    else
                        pEntry->pbData = (uint8_t *)RTMemPageAlloc(pEntry->cbData);
                    AssertPtr(pEntry->pbData);

                    pdmacFileEpCacheEntryWaitersAdd(pEntry, pTask,
                                                    &IoMemCtx,
                                                    OffDiff, cbToRead,
                                                    false /* fWrite */);
                    pdmacFileCacheReadFromEndpoint(pEntry);
                    /* Release the entry */
                    pdmacFileEpCacheEntryRelease(pEntry);
                }
                else
                {
                    RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
                    STAM_PROFILE_ADV_START(&pCache->StatTreeRemove, Cache);
                    RTAvlrFileOffsetRemove(pEndpointCache->pTree, pEntry->Core.Key);
                    STAM_PROFILE_ADV_STOP(&pCache->StatTreeRemove, Cache);
                    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);

                    pdmacFileCacheLockLeave(pCache);

                    RTMemFree(pEntry);

                    pdmacFileEpCacheRequestPassthrough(pEndpoint, pTask,
                                                       &IoMemCtx, off, cbToRead,
                                                       PDMACTASKFILETRANSFER_READ);
                }
            }
        }
        else
        {
#ifdef VBOX_WITH_IO_READ_CACHE
            /* No entry found for this offset. Create a new entry and fetch the data to the cache. */
            PPDMACFILECACHEENTRY pEntryNew = pdmacFileEpCacheEntryCreate(pEndpoint,
                                                                         pEndpointCache,
                                                                         off, cbRead,
                                                                         PAGE_SIZE,
                                                                         &cbToRead);

            cbRead -= cbToRead;

            if (pEntryNew)
            {
                if (!cbRead)
                    STAM_COUNTER_INC(&pCache->cMisses);
                else
                    STAM_COUNTER_INC(&pCache->cPartialHits);

                pdmacFileEpCacheEntryWaitersAdd(pEntryNew, pTask,
                                                &IoMemCtx,
                                                off - pEntryNew->Core.Key,
                                                cbToRead,
                                                false /* fWrite */);
                pdmacFileCacheReadFromEndpoint(pEntryNew);
                pdmacFileEpCacheEntryRelease(pEntryNew); /* it is protected by the I/O in progress flag now. */
            }
            else
            {
                /*
                 * There is not enough free space in the cache.
                 * Pass the request directly to the I/O manager.
                 */
                LogFlow(("Couldn't evict %u bytes from the cache. Remaining request will be passed through\n", cbToRead));

                pdmacFileEpCacheRequestPassthrough(pEndpoint, pTask,
                                                   &IoMemCtx, off, cbToRead,
                                                   PDMACTASKFILETRANSFER_READ);
            }
#else
            /* Clip read size if neccessary. */
            PPDMACFILECACHEENTRY pEntryAbove;
            pdmacFileEpCacheGetCacheBestFitEntryByOffset(pEndpointCache, off,
                                                         &pEntryAbove, NULL);

            if (pEntryAbove)
            {
                if (off + (RTFOFF)cbRead > pEntryAbove->Core.Key)
                    cbToRead = pEntryAbove->Core.Key - off;
                else
                    cbToRead = cbRead;

                pdmacFileEpCacheEntryRelease(pEntryAbove);
            }
            else
                cbToRead = cbRead;

            cbRead -= cbToRead;
            pdmacFileEpCacheRequestPassthrough(pEndpoint, pTask,
                                                &IoMemCtx, off, cbToRead,
                                                PDMACTASKFILETRANSFER_READ);
#endif
        }
        off += cbToRead;
    }

    ASMAtomicWriteBool(&pTask->fCompleted, false);

    if (ASMAtomicReadS32(&pTask->cbTransferLeft) == 0
        && !ASMAtomicXchgBool(&pTask->fCompleted, true))
        pdmR3AsyncCompletionCompleteTask(&pTask->Core, VINF_SUCCESS, false);
    else
        rc = VINF_AIO_TASK_PENDING;

    LogFlowFunc((": Leave rc=%Rrc\n", rc));

   return rc;
}

/**
 * Writes the given data to the endpoint using the cache if possible.
 *
 * @returns VBox status code.
 * @param    pEndpoint     The endpoint to write to.
 * @param    pTask         The task structure used as identifier for this request.
 * @param    off           The offset to start writing to
 * @param    paSegments    Pointer to the array holding the source buffers.
 * @param    cSegments     Number of segments in the array.
 * @param    cbWrite       Number of bytes to write.
 */
int pdmacFileEpCacheWrite(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask,
                          RTFOFF off, PCRTSGSEG paSegments, size_t cSegments,
                          size_t cbWrite)
{
    int rc = VINF_SUCCESS;
    PPDMACFILEENDPOINTCACHE pEndpointCache = &pEndpoint->DataCache;
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    PPDMACFILECACHEENTRY pEntry;

    LogFlowFunc((": pEndpoint=%#p{%s} pTask=%#p off=%RTfoff paSegments=%#p cSegments=%u cbWrite=%u\n",
                 pEndpoint, pEndpoint->Core.pszUri, pTask, off, paSegments, cSegments, cbWrite));

    pTask->cbTransferLeft = cbWrite;
    pTask->rc             = VINF_SUCCESS;
    /* Set to completed to make sure that the task is valid while we access it. */
    ASMAtomicWriteBool(&pTask->fCompleted, true);

    /* Init the I/O memory context */
    PDMIOMEMCTX IoMemCtx;
    pdmIoMemCtxInit(&IoMemCtx, paSegments, cSegments);

    while (cbWrite)
    {
        size_t cbToWrite;

        pEntry = pdmacFileEpCacheGetCacheEntryByOffset(pEndpointCache, off);

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
            if (   (pEntry->pList == &pCache->LruRecentlyUsedIn)
                || (pEntry->pList == &pCache->LruFrequentlyUsed))
            {
                /* Check if the entry is dirty. */
                if(pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                    PDMACFILECACHE_ENTRY_IS_DIRTY,
                                                                    0))
                {
                    /* If it is dirty but not in progrss just update the data. */
                    if (!(pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS))
                    {
                        pdmacFileEpCacheCopyFromIoMemCtx(&IoMemCtx,
                                                            pEntry->pbData + OffDiff,
                                                            cbToWrite);
                        ASMAtomicSubS32(&pTask->cbTransferLeft, cbToWrite);
                    }
                    else
                    {
                            /* The data isn't written to the file yet */
                            pdmacFileEpCacheEntryWaitersAdd(pEntry, pTask,
                                                            &IoMemCtx,
                                                            OffDiff, cbToWrite,
                                                            true /* fWrite */);
                            STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                    }

                        RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                }
                else /* Dirty bit not set */
                {
                    /*
                     * Check if a read is in progress for this entry.
                     * We have to defer processing in that case.
                     */
                    if(pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                        PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                                                                        0))
                    {
                        pdmacFileEpCacheEntryWaitersAdd(pEntry, pTask,
                                                        &IoMemCtx,
                                                        OffDiff, cbToWrite,
                                                        true /* fWrite */);
                        STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                        RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                    }
                    else /* I/O in progress flag not set */
                    {
                        /* Write as much as we can into the entry and update the file. */
                        pdmacFileEpCacheCopyFromIoMemCtx(&IoMemCtx,
                                                            pEntry->pbData + OffDiff,
                                                            cbToWrite);
                        ASMAtomicSubS32(&pTask->cbTransferLeft, cbToWrite);

                        bool fCommit = pdmacFileCacheAddDirtyEntry(pEndpointCache, pEntry);
                        if (fCommit)
                            pdmacFileCacheCommitDirtyEntries(pCache);
                    }
                } /* Dirty bit not set */

                /* Move this entry to the top position */
                if (pEntry->pList == &pCache->LruFrequentlyUsed)
                {
                    pdmacFileCacheLockEnter(pCache);
                    pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                    pdmacFileCacheLockLeave(pCache);
                }

                pdmacFileEpCacheEntryRelease(pEntry);
            }
            else /* Entry is on the ghost list */
            {
                uint8_t *pbBuffer = NULL;

                pdmacFileCacheLockEnter(pCache);
                pdmacFileCacheEntryRemoveFromList(pEntry); /* Remove it before we remove data, otherwise it may get freed when evicting data. */
                bool fEnough = pdmacFileCacheReclaim(pCache, pEntry->cbData, true, &pbBuffer);

                if (fEnough)
                {
                    /* Move the entry to Am and fetch it to the cache. */
                    pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                    pdmacFileCacheAdd(pCache, pEntry->cbData);
                    pdmacFileCacheLockLeave(pCache);

                    if (pbBuffer)
                        pEntry->pbData = pbBuffer;
                    else
                        pEntry->pbData = (uint8_t *)RTMemPageAlloc(pEntry->cbData);
                    AssertPtr(pEntry->pbData);

                    pdmacFileEpCacheEntryWaitersAdd(pEntry, pTask,
                                                    &IoMemCtx,
                                                    OffDiff, cbToWrite,
                                                    true /* fWrite */);
                    STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                    pdmacFileCacheReadFromEndpoint(pEntry);

                    /* Release the reference. If it is still needed the I/O in progress flag should protect it now. */
                    pdmacFileEpCacheEntryRelease(pEntry);
                }
                else
                {
                    RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
                    STAM_PROFILE_ADV_START(&pCache->StatTreeRemove, Cache);
                    RTAvlrFileOffsetRemove(pEndpointCache->pTree, pEntry->Core.Key);
                    STAM_PROFILE_ADV_STOP(&pCache->StatTreeRemove, Cache);
                    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);

                    pdmacFileCacheLockLeave(pCache);

                    RTMemFree(pEntry);
                    pdmacFileEpCacheRequestPassthrough(pEndpoint, pTask,
                                                       &IoMemCtx, off, cbToWrite,
                                                       PDMACTASKFILETRANSFER_WRITE);
                }
            }
        }
        else /* No entry found */
        {
            /*
             * No entry found. Try to create a new cache entry to store the data in and if that fails
             * write directly to the file.
             */
            PPDMACFILECACHEENTRY pEntryNew = pdmacFileEpCacheEntryCreate(pEndpoint,
                                                                         pEndpointCache,
                                                                         off, cbWrite,
                                                                         512,
                                                                         &cbToWrite);

            cbWrite -= cbToWrite;

            if (pEntryNew)
            {
                RTFOFF offDiff = off - pEntryNew->Core.Key;

                STAM_COUNTER_INC(&pCache->cHits);

                /*
                 * Check if it is possible to just write the data without waiting
                 * for it to get fetched first.
                 */
                if (!offDiff && pEntryNew->cbData == cbToWrite)
                {
                    pdmacFileEpCacheCopyFromIoMemCtx(&IoMemCtx,
                                                     pEntryNew->pbData,
                                                     cbToWrite);
                    ASMAtomicSubS32(&pTask->cbTransferLeft, cbToWrite);

                    bool fCommit = pdmacFileCacheAddDirtyEntry(pEndpointCache, pEntryNew);
                    if (fCommit)
                        pdmacFileCacheCommitDirtyEntries(pCache);
                    STAM_COUNTER_ADD(&pCache->StatWritten, cbToWrite);
                }
                else
                {
                    /* Defer the write and fetch the data from the endpoint. */
                    pdmacFileEpCacheEntryWaitersAdd(pEntryNew, pTask,
                                                    &IoMemCtx,
                                                    offDiff, cbToWrite,
                                                    true /* fWrite */);
                    STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                    pdmacFileCacheReadFromEndpoint(pEntryNew);
                }

                pdmacFileEpCacheEntryRelease(pEntryNew);
            }
            else
            {
                /*
                 * There is not enough free space in the cache.
                 * Pass the request directly to the I/O manager.
                 */
                LogFlow(("Couldn't evict %u bytes from the cache. Remaining request will be passed through\n", cbToWrite));

                STAM_COUNTER_INC(&pCache->cMisses);

                pdmacFileEpCacheRequestPassthrough(pEndpoint, pTask,
                                                   &IoMemCtx, off, cbToWrite,
                                                   PDMACTASKFILETRANSFER_WRITE);
            }
        }

        off += cbToWrite;
    }

    ASMAtomicWriteBool(&pTask->fCompleted, false);

    if (ASMAtomicReadS32(&pTask->cbTransferLeft) == 0
        && !ASMAtomicXchgBool(&pTask->fCompleted, true))
        pdmR3AsyncCompletionCompleteTask(&pTask->Core, VINF_SUCCESS, false);
    else
        rc = VINF_AIO_TASK_PENDING;

    LogFlowFunc((": Leave rc=%Rrc\n", rc));

    return rc;
}

int pdmacFileEpCacheFlush(PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint, PPDMASYNCCOMPLETIONTASKFILE pTask)
{
    int rc = VINF_SUCCESS;

    LogFlowFunc((": pEndpoint=%#p{%s} pTask=%#p\n",
                 pEndpoint, pEndpoint->Core.pszUri, pTask));

    pTask->rc = VINF_SUCCESS;

    if (ASMAtomicReadPtr((void * volatile *)&pEndpoint->DataCache.pTaskFlush))
        rc = VERR_RESOURCE_BUSY;
    else
    {
        /* Check for dirty entries in the cache. */
        pdmacFileCacheEndpointCommit(&pEndpoint->DataCache);
        if (ASMAtomicReadU32(&pEndpoint->DataCache.cWritesOutstanding) > 0)
        {
            ASMAtomicWritePtr((void * volatile *)&pEndpoint->DataCache.pTaskFlush, pTask);
            rc = VINF_AIO_TASK_PENDING;
        }
        else
            pdmR3AsyncCompletionCompleteTask(&pTask->Core, VINF_SUCCESS, false);
    }

    LogFlowFunc((": Leave rc=%Rrc\n", rc));
    return rc;
}

