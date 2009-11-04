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
 * This component implements an I/O cache for file endpoints based on the ARC algorithm.
 * http://en.wikipedia.org/wiki/Adaptive_Replacement_Cache
 *
 * The algorithm uses four LRU (Least frequently used) lists to store data in the cache.
 * Two of them contain data where one stores entries which were accessed recently and one
 * which is used for frequently accessed data.
 * The other two lists are called ghost lists and store information about the accessed range
 * but do not contain data. They are used to track data access. If these entries are accessed
 * they will push the data to a higher position in the cache preventing it from getting removed
 * quickly again.
 *
 * The algorithm needs to be modified to meet our requirements. Like the implementation
 * for the ZFS filesystem we need to handle pages with a variable size. It would
 * be possible to use a fixed size but would increase the computational
 * and memory overhead.
 * Because we do I/O asynchronously we also need to mark entries which are currently accessed
 * as non evictable to prevent removal of the entry while the data is being accessed.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_ASYNC_COMPLETION
#define RT_STRICT
#include <iprt/types.h>
#include <iprt/mem.h>
#include <iprt/path.h>
#include <VBox/log.h>
#include <VBox/stam.h>

#include "PDMAsyncCompletionFileInternal.h"

#ifdef VBOX_STRICT
# define PDMACFILECACHE_IS_CRITSECT_OWNER(Cache) \
    do \
    { \
     AssertMsg(RTCritSectIsOwner(&pCache->CritSect), \
               ("Thread does not own critical section\n"));\
    } while(0);
#else
# define PDMACFILECACHE_IS_CRITSECT_OWNER(Cache) do { } while(0);
#endif

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void pdmacFileCacheTaskCompleted(PPDMACTASKFILE pTask, void *pvUser);

DECLINLINE(void) pdmacFileEpCacheEntryRelease(PPDMACFILECACHEENTRY pEntry)
{
    AssertMsg(pEntry->cRefs > 0, ("Trying to release a not referenced entry\n"));
    ASMAtomicDecU32(&pEntry->cRefs);
}

DECLINLINE(void) pdmacFileEpCacheEntryRef(PPDMACFILECACHEENTRY pEntry)
{
    ASMAtomicIncU32(&pEntry->cRefs);
}

/**
 * Checks consistency of a LRU list.
 *
 * @returns nothing
 * @param    pList         The LRU list to check.
 * @param    pNotInList    Element which is not allowed to occur in the list.
 */
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
 *
 * @notes This function may return fewer bytes than requested because entries
 *        may be marked as non evictable if they are used for I/O at the moment.
 */
static size_t pdmacFileCacheEvictPagesFrom(PPDMACFILECACHEGLOBAL pCache, size_t cbData,
                                           PPDMACFILELRULIST pListSrc, PPDMACFILELRULIST pGhostListDst)
{
    size_t cbEvicted = 0;

    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);

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
        if (!(pCurr->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS)
            && (ASMAtomicReadU32(&pCurr->cRefs) == 0))
        {
            /* Ok eviction candidate. Grab the endpoint semaphore and check again
             * because somebody else might have raced us. */
            PPDMACFILEENDPOINTCACHE pEndpointCache = &pCurr->pEndpoint->DataCache;
            RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);

            if (!(pCurr->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS)
                && (ASMAtomicReadU32(&pCurr->cRefs) == 0))
            {
                AssertMsg(!(pCurr->fFlags & PDMACFILECACHE_ENTRY_IS_DEPRECATED),
                          ("This entry is deprecated so it should have the I/O in progress flag set\n"));
                Assert(!pCurr->pbDataReplace);

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
            RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
        }
        else
            LogFlow(("Entry %#p (%u bytes) is still in progress and can't be evicted\n", pCurr, pCurr->cbData));
    }

    return cbEvicted;
}

static size_t pdmacFileCacheReplace(PPDMACFILECACHEGLOBAL pCache, size_t cbData, PPDMACFILELRULIST pEntryList)
{
    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);

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

/**
 * Tries to evict the given amount of the data from the cache.
 *
 * @returns Bytes removed.
 * @param    pCache    The global cache data.
 * @param    cbData    Number of bytes to evict.
 */
static size_t pdmacFileCacheEvict(PPDMACFILECACHEGLOBAL pCache, size_t cbData)
{
    size_t cbRemoved = ~0;

    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);

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

/**
 * Updates the cache parameters
 *
 * @returns nothing.
 * @param    pCache    The global cache data.
 * @param    pEntry    The entry usign for the update.
 */
static void pdmacFileCacheUpdate(PPDMACFILECACHEGLOBAL pCache, PPDMACFILECACHEENTRY pEntry)
{
    int32_t uUpdateVal = 0;

    PDMACFILECACHE_IS_CRITSECT_OWNER(pCache);

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

    /* Send it off to the I/O manager. */
    pdmacFileEpAddTask(pEntry->pEndpoint, pIoTask);
}

/**
 * Completion callback for I/O tasks.
 *
 * @returns nothing.
 * @param    pTask     The completed task.
 * @param    pvUser    Opaque user data.
 */
static void pdmacFileCacheTaskCompleted(PPDMACTASKFILE pTask, void *pvUser)
{
    PPDMACFILECACHEENTRY pEntry = (PPDMACFILECACHEENTRY)pvUser;
    PPDMACFILECACHEGLOBAL pCache = pEntry->pCache;
    PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint = pEntry->pEndpoint;

    /* Reference the entry now as we are clearing the I/O in progres flag
     * which protects the entry till now. */
    pdmacFileEpCacheEntryRef(pEntry);

    RTSemRWRequestWrite(pEndpoint->DataCache.SemRWEntries, RT_INDEFINITE_WAIT);
    pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IO_IN_PROGRESS;

    /* Process waiting segment list. The data in entry might have changed inbetween. */
    PPDMACFILETASKSEG pCurr = pEntry->pWaitingHead;

    AssertMsg((pCurr && pEntry->pWaitingTail) || (!pCurr && !pEntry->pWaitingTail),
                ("The list tail was not updated correctly\n"));
    pEntry->pWaitingTail = NULL;
    pEntry->pWaitingHead = NULL;

    if (pTask->enmTransferType == PDMACTASKFILETRANSFER_WRITE)
    {
        if (pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DEPRECATED)
        {
            AssertMsg(!pCurr, ("The entry is deprecated but has waiting write segments attached\n"));

            RTMemPageFree(pEntry->pbData);
            pEntry->pbData = pEntry->pbDataReplace;
            pEntry->pbDataReplace = NULL;
            pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IS_DEPRECATED;
        }
        else
        {
            pEntry->fFlags &= ~PDMACFILECACHE_ENTRY_IS_DIRTY;

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
    }
    else
    {
        AssertMsg(pTask->enmTransferType == PDMACTASKFILETRANSFER_READ, ("Invalid transfer type\n"));
        AssertMsg(!(pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY),("Invalid flags set\n"));

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

    if (pEntry->fFlags & PDMACFILECACHE_ENTRY_IS_DIRTY)
        pdmacFileCacheWriteToEndpoint(pEntry);

    RTSemRWReleaseWrite(pEndpoint->DataCache.SemRWEntries);

    /* Dereference so that it isn't protected anymore except we issued anyother write for it. */
    pdmacFileEpCacheEntryRelease(pEntry);
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
    LogFlowFunc((": Maximum number of bytes cached %u\n", pCache->cbMax));

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

    if (RT_SUCCESS(rc))
        LogRel(("AIOMgr: Cache successfully initialised. Cache size is %u bytes\n", pCache->cbMax));

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
    RTCritSectEnter(&pCache->CritSect);

    /* Cleanup deleting all cache entries waiting for in progress entries to finish. */
    pdmacFileCacheDestroyList(&pCache->LruRecentlyUsed);
    pdmacFileCacheDestroyList(&pCache->LruFrequentlyUsed);
    pdmacFileCacheDestroyList(&pCache->LruRecentlyGhost);
    pdmacFileCacheDestroyList(&pCache->LruFrequentlyGhost);

    RTCritSectLeave(&pCache->CritSect);

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

    int rc = RTSemRWCreate(&pEndpointCache->SemRWEntries);
    if (RT_SUCCESS(rc))
    {
        pEndpointCache->pTree  = (PAVLRFOFFTREE)RTMemAllocZ(sizeof(AVLRFOFFTREE));
        if (!pEndpointCache->pTree)
        {
            rc = VERR_NO_MEMORY;
            RTSemRWDestroy(pEndpointCache->SemRWEntries);
        }
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

    while (pEntry->fFlags & (PDMACFILECACHE_ENTRY_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DIRTY))
    {
        RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
        RTThreadSleep(250);
        RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    }

    AssertMsg(!(pEntry->fFlags & (PDMACFILECACHE_ENTRY_IO_IN_PROGRESS | PDMACFILECACHE_ENTRY_IS_DIRTY)),
                ("Entry is dirty and/or still in progress fFlags=%#x\n", pEntry->fFlags));

    pdmacFileCacheEntryRemoveFromList(pEntry);
    pCache->cbCached -= pEntry->cbData;

    RTMemPageFree(pEntry->pbData);
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
    RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    RTCritSectEnter(&pCache->CritSect);
    RTAvlrFileOffsetDestroy(pEndpointCache->pTree, pdmacFileEpCacheEntryDestroy, pCache);
    RTCritSectLeave(&pCache->CritSect);
    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);

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

static PPDMACFILECACHEENTRY pdmacFileEpCacheGetCacheBestFitEntryByOffset(PPDMACFILEENDPOINTCACHE pEndpointCache, RTFOFF off)
{
    PPDMACFILECACHEGLOBAL pCache = pEndpointCache->pCache;
    PPDMACFILECACHEENTRY pEntry = NULL;

    STAM_PROFILE_ADV_START(&pCache->StatTreeGet, Cache);

    RTSemRWRequestRead(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);
    pEntry = (PPDMACFILECACHEENTRY)RTAvlrFileOffsetGetBestFit(pEndpointCache->pTree, off, true);
    if (pEntry)
        pdmacFileEpCacheEntryRef(pEntry);
    RTSemRWReleaseRead(pEndpointCache->SemRWEntries);

    STAM_PROFILE_ADV_STOP(&pCache->StatTreeGet, Cache);

    return pEntry;
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
 */
static PPDMACFILECACHEENTRY pdmacFileCacheEntryAlloc(PPDMACFILECACHEGLOBAL pCache,
                                                     PPDMASYNCCOMPLETIONENDPOINTFILE pEndpoint,
                                                     RTFOFF off, size_t cbData)
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
    pEntryNew->pbDataReplace = NULL;
    pEntryNew->pbData        = (uint8_t *)RTMemPageAlloc(cbData);

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
static void pdmacFileEpCacheEntryAddWaitingSegment(PPDMACFILECACHEENTRY pEntry, PPDMACFILETASKSEG pSeg)
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
    bool fPassed = ((pEntry->fFlags & fSet) && !(pEntry->fFlags & fClear));

    if (fPassed)
    {
        /* Acquire the lock and check again becuase the completion callback might have raced us. */
        RTSemRWRequestWrite(pEndpointCache->SemRWEntries, RT_INDEFINITE_WAIT);

        fPassed = ((pEntry->fFlags & fSet) && !(pEntry->fFlags & fClear));

        /* Drop the lock if we didn't passed the test. */
        if (!fPassed)
            RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
    }

    return fPassed;
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
    while (0)

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

    int iSegCurr       = 0;
    uint8_t *pbSegBuf  = (uint8_t *)paSegments[iSegCurr].pvSeg;
    size_t   cbSegLeft = paSegments[iSegCurr].cbSeg;

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
                if(pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                  PDMACFILECACHE_ENTRY_IS_DEPRECATED,
                                                                  0))
                {
                    /* Entry is deprecated. Read data from the new buffer. */
                    while (cbToRead)
                    {
                        size_t cbCopy = RT_MIN(cbSegLeft, cbToRead);

                        memcpy(pbSegBuf, pEntry->pbDataReplace + OffDiff, cbCopy);

                        ADVANCE_SEGMENT_BUFFER(cbCopy);

                        cbToRead -= cbCopy;
                        off      += cbCopy;
                        OffDiff  += cbCopy;
                        ASMAtomicSubS32(&pTask->cbTransferLeft, cbCopy);
                    }
                }
                else
                {
                    if (pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                       PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                                                                       PDMACFILECACHE_ENTRY_IS_DIRTY))
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

                            pdmacFileEpCacheEntryAddWaitingSegment(pEntry, pSeg);

                            off      += pSeg->cbTransfer;
                            cbToRead -= pSeg->cbTransfer;
                            OffDiff  += pSeg->cbTransfer;
                        }
                        RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
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
                }

                /* Move this entry to the top position */
                RTCritSectEnter(&pCache->CritSect);
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                RTCritSectLeave(&pCache->CritSect);
            }
            else
            {
                LogFlow(("Fetching data for ghost entry %#p from file\n", pEntry));

                RTCritSectEnter(&pCache->CritSect);
                pdmacFileCacheUpdate(pCache, pEntry);
                pdmacFileCacheReplace(pCache, pEntry->cbData, pEntry->pList);

                /* Move the entry to T2 and fetch it to the cache. */
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                RTCritSectLeave(&pCache->CritSect);

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

                    pdmacFileEpCacheEntryAddWaitingSegment(pEntry, pSeg);

                    off      += pSeg->cbTransfer;
                    OffDiff  += pSeg->cbTransfer;
                    cbToRead -= pSeg->cbTransfer;
                }

                pdmacFileCacheReadFromEndpoint(pEntry);
            }
            pdmacFileEpCacheEntryRelease(pEntry);
        }
        else
        {
            /* No entry found for this offset. Get best fit entry and fetch the data to the cache. */
            size_t cbToReadAligned;
            PPDMACFILECACHEENTRY pEntryBestFit = pdmacFileEpCacheGetCacheBestFitEntryByOffset(pEndpointCache, off);

            LogFlow(("%sbest fit entry for off=%RTfoff (BestFit=%RTfoff BestFitEnd=%RTfoff BestFitSize=%u)\n",
                     pEntryBestFit ? "" : "No ",
                     off,
                     pEntryBestFit ? pEntryBestFit->Core.Key : 0,
                     pEntryBestFit ? pEntryBestFit->Core.KeyLast : 0,
                     pEntryBestFit ? pEntryBestFit->cbData : 0));

            if (pEntryBestFit && ((off + (RTFOFF)cbRead) > pEntryBestFit->Core.Key))
            {
                cbToRead = pEntryBestFit->Core.Key - off;
                pdmacFileEpCacheEntryRelease(pEntryBestFit);
                cbToReadAligned = cbToRead;
            }
            else
            {
                /*
                 * Align the size to a 4KB boundary.
                 * Memory size is aligned to a page boundary
                 * and memory is wasted if the size is rahter small.
                 * (For example reads with a size of 512 bytes.
                 */
                cbToRead = cbRead; 
                cbToReadAligned = RT_ALIGN_Z(cbRead, PAGE_SIZE);

                /* Clip read to file size */
                cbToReadAligned = RT_MIN(pEndpoint->cbFile - off, cbToReadAligned);
                if (pEntryBestFit)
                    cbToReadAligned = RT_MIN(cbToReadAligned, pEntryBestFit->Core.Key - off);
            }

            cbRead -= cbToRead;

            if (!cbRead)
                STAM_COUNTER_INC(&pCache->cMisses);
            else
                STAM_COUNTER_INC(&pCache->cPartialHits);

            RTCritSectEnter(&pCache->CritSect);
            size_t cbRemoved = pdmacFileCacheEvict(pCache, cbToReadAligned);
            RTCritSectLeave(&pCache->CritSect);

            if (cbRemoved >= cbToReadAligned)
            {
                LogFlow(("Evicted %u bytes (%u requested). Creating new cache entry\n", cbRemoved, cbToReadAligned));
                PPDMACFILECACHEENTRY pEntryNew = pdmacFileCacheEntryAlloc(pCache, pEndpoint, off, cbToReadAligned);
                AssertPtr(pEntryNew);

                RTCritSectEnter(&pCache->CritSect);
                pdmacFileCacheEntryAddToList(&pCache->LruRecentlyUsed, pEntryNew);
                pCache->cbCached += cbToReadAligned;
                RTCritSectLeave(&pCache->CritSect);

                pdmacFileEpCacheInsertEntry(pEndpointCache, pEntryNew);
                uint32_t uBufOffset = 0;

                while (cbToRead)
                {
                    PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                    pSeg->pTask      = pTask;
                    pSeg->uBufOffset = uBufOffset;
                    pSeg->cbTransfer = RT_MIN(cbToRead, cbSegLeft);
                    pSeg->pvBuf      = pbSegBuf;

                    ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                    pdmacFileEpCacheEntryAddWaitingSegment(pEntryNew, pSeg);

                    off        += pSeg->cbTransfer;
                    cbToRead   -= pSeg->cbTransfer;
                    uBufOffset += pSeg->cbTransfer;
                }

                pdmacFileCacheReadFromEndpoint(pEntryNew);
                pdmacFileEpCacheEntryRelease(pEntryNew); /* it is protected by the I/O in progress flag now. */
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

    int iSegCurr       = 0;
    uint8_t *pbSegBuf  = (uint8_t *)paSegments[iSegCurr].pvSeg;
    size_t   cbSegLeft = paSegments[iSegCurr].cbSeg;

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
            if (   (pEntry->pList == &pCache->LruRecentlyUsed)
                || (pEntry->pList == &pCache->LruFrequentlyUsed))
            {
                /* Check if the buffer is deprecated. */
                if(pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                  PDMACFILECACHE_ENTRY_IS_DEPRECATED,
                                                                  0))
                {
                    AssertMsg(pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                                ("Entry is deprecated but not in progress\n"));
                    AssertPtr(pEntry->pbDataReplace);

                    LogFlow(("Writing to deprecated buffer of entry %#p\n", pEntry));

                    /* Update the data from the write. */
                    while (cbToWrite)
                    {
                        size_t cbCopy = RT_MIN(cbSegLeft, cbToWrite);

                        memcpy(pEntry->pbDataReplace + OffDiff, pbSegBuf, cbCopy);

                        ADVANCE_SEGMENT_BUFFER(cbCopy);

                        cbToWrite-= cbCopy;
                        off      += cbCopy;
                        OffDiff  += cbCopy;
                        ASMAtomicSubS32(&pTask->cbTransferLeft, cbCopy);
                    }
                    RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                }
                else
                {
                    /* If the entry is dirty it must be also in progress now and we have to defer updating it again. */
                    if(pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                      PDMACFILECACHE_ENTRY_IS_DIRTY,
                                                                      0))
                    {
                        AssertMsg(pEntry->fFlags & PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                                    ("Entry is dirty but not in progress\n"));
                        Assert(!pEntry->pbDataReplace);

                        /* Deprecate the current buffer. */
                        if (!pEntry->pWaitingHead)
                            pEntry->pbDataReplace = (uint8_t *)RTMemPageAlloc(pEntry->cbData);

                        /* If we are out of memory or have waiting segments
                         * defer the write. */
                        if (!pEntry->pbDataReplace || pEntry->pWaitingHead)
                        {
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

                                pdmacFileEpCacheEntryAddWaitingSegment(pEntry, pSeg);

                                off       += pSeg->cbTransfer;
                                OffDiff   += pSeg->cbTransfer;
                                cbToWrite -= pSeg->cbTransfer;
                            }
                            STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                        }
                        else
                        {
                            LogFlow(("Deprecating buffer for entry %#p\n", pEntry));
                            pEntry->fFlags |= PDMACFILECACHE_ENTRY_IS_DEPRECATED;

#if 1
                            /* Copy the data before the update. */
                            if (OffDiff)
                                memcpy(pEntry->pbDataReplace, pEntry->pbData, OffDiff);

                            /* Copy data behind the update. */
                            if ((pEntry->cbData - OffDiff - cbToWrite) > 0)
                                memcpy(pEntry->pbDataReplace + OffDiff + cbToWrite,
                                       pEntry->pbData + OffDiff + cbToWrite,
                                       (pEntry->cbData - OffDiff - cbToWrite));
#else
                            /* A safer method but probably slower. */
                            memcpy(pEntry->pbDataReplace, pEntry->pbData, pEntry->cbData);
#endif

                            /* Update the data from the write. */
                            while (cbToWrite)
                            {
                                size_t cbCopy = RT_MIN(cbSegLeft, cbToWrite);

                                memcpy(pEntry->pbDataReplace + OffDiff, pbSegBuf, cbCopy);

                                ADVANCE_SEGMENT_BUFFER(cbCopy);

                                cbToWrite-= cbCopy;
                                off      += cbCopy;
                                OffDiff  += cbCopy;
                                ASMAtomicSubS32(&pTask->cbTransferLeft, cbCopy);
                            }

                            /* We are done here. A new write is initiated if the current request completes. */
                        }

                        RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                    }
                    else
                    {
                        /*
                         * Check if a read is in progress for this entry.
                         * We have to defer processing in that case.
                         */
                        if(pdmacFileEpCacheEntryFlagIsSetClearAcquireLock(pEndpointCache, pEntry,
                                                                          PDMACFILECACHE_ENTRY_IO_IN_PROGRESS,
                                                                          0))
                        {
                            while (cbToWrite)
                            {
                                PPDMACFILETASKSEG pSeg = (PPDMACFILETASKSEG)RTMemAllocZ(sizeof(PDMACFILETASKSEG));

                                pSeg->pTask      = pTask;
                                pSeg->uBufOffset = OffDiff;
                                pSeg->cbTransfer = RT_MIN(cbToWrite, cbSegLeft);
                                pSeg->pvBuf      = pbSegBuf;
                                pSeg->fWrite     = true;

                                ADVANCE_SEGMENT_BUFFER(pSeg->cbTransfer);

                                pdmacFileEpCacheEntryAddWaitingSegment(pEntry, pSeg);

                                off       += pSeg->cbTransfer;
                                OffDiff   += pSeg->cbTransfer;
                                cbToWrite -= pSeg->cbTransfer;
                            }
                            STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                            RTSemRWReleaseWrite(pEndpointCache->SemRWEntries);
                        }
                        else
                        {
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
                    }

                    /* Move this entry to the top position */
                    RTCritSectEnter(&pCache->CritSect);
                    pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                    RTCritSectLeave(&pCache->CritSect);
                }
            }
            else
            {
                RTCritSectEnter(&pCache->CritSect);
                pdmacFileCacheUpdate(pCache, pEntry);
                pdmacFileCacheReplace(pCache, pEntry->cbData, pEntry->pList);

                /* Move the entry to T2 and fetch it to the cache. */
                pdmacFileCacheEntryAddToList(&pCache->LruFrequentlyUsed, pEntry);
                RTCritSectLeave(&pCache->CritSect);

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

                    pdmacFileEpCacheEntryAddWaitingSegment(pEntry, pSeg);

                    off       += pSeg->cbTransfer;
                    OffDiff   += pSeg->cbTransfer;
                    cbToWrite -= pSeg->cbTransfer;
                }

                STAM_COUNTER_INC(&pEndpointCache->StatWriteDeferred);
                pdmacFileCacheReadFromEndpoint(pEntry);
            }

            /* Release the reference. If it is still needed the I/O in progress flag should protect it now. */
            pdmacFileEpCacheEntryRelease(pEntry);
        }
        else
        {
            /*
             * No entry found. Try to create a new cache entry to store the data in and if that fails
             * write directly to the file.
             */
            PPDMACFILECACHEENTRY pEntryBestFit = pdmacFileEpCacheGetCacheBestFitEntryByOffset(pEndpointCache, off);

            LogFlow(("%sest fit entry for off=%RTfoff (BestFit=%RTfoff BestFitEnd=%RTfoff BestFitSize=%u)\n",
                     pEntryBestFit ? "B" : "No b",
                     off,
                     pEntryBestFit ? pEntryBestFit->Core.Key : 0,
                     pEntryBestFit ? pEntryBestFit->Core.KeyLast : 0,
                     pEntryBestFit ? pEntryBestFit->cbData : 0));

            if (pEntryBestFit && ((off + (RTFOFF)cbWrite) > pEntryBestFit->Core.Key))
            {
                cbToWrite = pEntryBestFit->Core.Key - off;
                pdmacFileEpCacheEntryRelease(pEntryBestFit);
            }
            else
                cbToWrite = cbWrite;

            cbWrite -= cbToWrite;

            STAM_COUNTER_INC(&pCache->cMisses);
            STAM_COUNTER_ADD(&pCache->StatWritten, cbToWrite);

            RTCritSectEnter(&pCache->CritSect);
            size_t cbRemoved = pdmacFileCacheEvict(pCache, cbToWrite);
            RTCritSectLeave(&pCache->CritSect);

            if (cbRemoved >= cbToWrite)
            {
                uint8_t *pbBuf;
                PPDMACFILECACHEENTRY pEntryNew;

                LogFlow(("Evicted %u bytes (%u requested). Creating new cache entry\n", cbRemoved, cbToWrite));

                pEntryNew = pdmacFileCacheEntryAlloc(pCache, pEndpoint, off, cbToWrite);
                AssertPtr(pEntryNew);

                RTCritSectEnter(&pCache->CritSect);
                pdmacFileCacheEntryAddToList(&pCache->LruRecentlyUsed, pEntryNew);
                pCache->cbCached += cbToWrite;
                RTCritSectLeave(&pCache->CritSect);

                pdmacFileEpCacheInsertEntry(pEndpointCache, pEntryNew);

                off   += cbToWrite;
                pbBuf  = pEntryNew->pbData;

                while (cbToWrite)
                {
                    size_t cbCopy = RT_MIN(cbSegLeft, cbToWrite);

                    memcpy(pbBuf, pbSegBuf, cbCopy);

                    ADVANCE_SEGMENT_BUFFER(cbCopy);

                    cbToWrite -= cbCopy;
                    pbBuf     += cbCopy;
                    ASMAtomicSubS32(&pTask->cbTransferLeft, cbCopy);
                }

                pEntryNew->fFlags |= PDMACFILECACHE_ENTRY_IS_DIRTY;
                pdmacFileCacheWriteToEndpoint(pEntryNew);
                pdmacFileEpCacheEntryRelease(pEntryNew); /* it is protected by the I/O in progress flag now. */
            }
            else
            {
                /*
                 * There is not enough free space in the cache.
                 * Pass the request directly to the I/O manager.
                 */
                LogFlow(("Couldn't evict %u bytes from the cache (%u actually removed). Remaining request will be passed through\n", cbToWrite, cbRemoved));

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
    }

    ASMAtomicWriteBool(&pTask->fCompleted, false);

    if (ASMAtomicReadS32(&pTask->cbTransferLeft) == 0
        && !ASMAtomicXchgBool(&pTask->fCompleted, true))
        pdmR3AsyncCompletionCompleteTask(&pTask->Core);

    return VINF_SUCCESS;
}

#undef ADVANCE_SEGMENT_BUFFER

