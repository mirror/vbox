/* $Id$ */
/** @file
 * innotek Portable Runtime - Memory Allocation, electric fence.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "alloc-ef.h"
#include <iprt/log.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include <VBox/sup.h>
#include <iprt/err.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef RTALLOC_EFENCE_TRACE
/** Spinlock protecting the allthe blocks globals. */
static volatile uint32_t    g_BlocksLock;
/** Tree tracking the allocations. */
static AVLPVTREE            g_BlocksTree;
#ifdef RTALLOC_EFENCE_FREE_DELAYED
/** Tail of the delayed blocks. */
static volatile PRTMEMBLOCK g_pBlocksDelayHead;
/** Tail of the delayed blocks. */
static volatile PRTMEMBLOCK g_pBlocksDelayTail;
/** Number of bytes in the delay list (includes fences). */
static volatile size_t      g_cbBlocksDelay;
#endif
#endif
/** Array of pointers free watches for. */
void   *gapvRTMemFreeWatch[4] = {0};
/** Enable logging of all freed memory. */
bool    gfRTMemFreeLog = false;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
/**
 * Complains about something.
 */
static void rtmemComplain(const char *pszOp, const char *pszFormat, ...)
{
    va_list args;
    fprintf(stderr, "RTMem error: %s: ", pszOp);
    va_start(args, pszFormat);
    vfprintf(stderr, pszFormat, args);
    va_end(args);
    AssertReleaseBreakpoint();
}

/**
 * Log an event.
 */
static inline void rtmemLog(const char *pszOp, const char *pszFormat, ...)
{
#if 0
    va_list args;
    fprintf(stderr, "RTMem info: %s: ", pszOp);
    va_start(args, pszFormat);
    vfprintf(stderr, pszFormat, args);
    va_end(args);
#endif
}


#ifdef RTALLOC_EFENCE_TRACE

/**
 * Aquires the lock.
 */
static inline void rtmemBlockLock(void)
{
    unsigned c = 0;
    while (!ASMAtomicCmpXchgU32(&g_BlocksLock, 1, 0))
        RTThreadSleep(((++c) >> 2) & 31);
}


/**
 * Releases the lock.
 */
static inline void rtmemBlockUnlock(void)
{
    Assert(g_BlocksLock == 1);
    ASMAtomicXchgU32(&g_BlocksLock, 0);
}


/**
 * Creates a block.
 */
static inline PRTMEMBLOCK rtmemBlockCreate(RTMEMTYPE enmType, size_t cb, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction)
{
    PRTMEMBLOCK pBlock = (PRTMEMBLOCK)malloc(sizeof(*pBlock));
    if (pBlock)
    {
        pBlock->enmType = enmType;
        pBlock->cb = cb;
        pBlock->pvCaller = pvCaller;
        pBlock->iLine = iLine;
        pBlock->pszFile = pszFile;
        pBlock->pszFunction = pszFunction;
    }
    return pBlock;
}


/**
 * Frees a block.
 */
static inline void rtmemBlockFree(PRTMEMBLOCK pBlock)
{
    free(pBlock);
}


/**
 * Insert a block from the tree.
 */
static inline void rtmemBlockInsert(PRTMEMBLOCK pBlock, void *pv)
{
    pBlock->Core.Key = pv;
    rtmemBlockLock();
    bool fRc = RTAvlPVInsert(&g_BlocksTree, &pBlock->Core);
    rtmemBlockUnlock();
    AssertRelease(fRc);
}


/**
 * Remove a block from the tree and returns it to the caller.
 */
static inline PRTMEMBLOCK rtmemBlockRemove(void *pv)
{
    rtmemBlockLock();
    PRTMEMBLOCK pBlock = (PRTMEMBLOCK)RTAvlPVRemove(&g_BlocksTree, pv);
    rtmemBlockUnlock();
    return pBlock;
}

/**
 * Gets a block.
 */
static inline PRTMEMBLOCK rtmemBlockGet(void *pv)
{
    rtmemBlockLock();
    PRTMEMBLOCK pBlock = (PRTMEMBLOCK)RTAvlPVGet(&g_BlocksTree, pv);
    rtmemBlockUnlock();
    return pBlock;
}

/**
 * Dumps one allocation.
 */
static DECLCALLBACK(int) RTMemDumpOne(PAVLPVNODECORE pNode, void *pvUser)
{
    PRTMEMBLOCK pBlock = (PRTMEMBLOCK)pNode;
    fprintf(stderr, "%p %08lx %p\n",
            pBlock->Core.Key,
            (long)pBlock->cb,
            pBlock->pvCaller);
    return 0;
}

/**
 * Dumps the allocated blocks.
 * This is something which you should call from gdb.
 */
extern "C" void RTMemDump(void);
void RTMemDump(void)
{
    fprintf(stderr, "address  size     caller\n");
    RTAvlPVDoWithAll(&g_BlocksTree, true, RTMemDumpOne, NULL);
}


#ifdef RTALLOC_EFENCE_FREE_DELAYED
/**
 * Insert a delayed block.
 */
static inline void rtmemBlockDelayInsert(PRTMEMBLOCK pBlock)
{
    size_t cbBlock = RT_ALIGN_Z(pBlock->cb, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
    pBlock->Core.pRight = NULL;
    pBlock->Core.pLeft = NULL;
    rtmemBlockLock();
    if (g_pBlocksDelayHead)
    {
        g_pBlocksDelayHead->Core.pLeft = (PAVLPVNODECORE)pBlock;
        pBlock->Core.pRight = (PAVLPVNODECORE)g_pBlocksDelayHead;
        g_pBlocksDelayHead = pBlock;
    }
    else
    {
        g_pBlocksDelayTail = pBlock;
        g_pBlocksDelayHead = pBlock;
    }
    g_cbBlocksDelay += cbBlock;
    rtmemBlockUnlock();
}

/**
 * Removes a delayed block.
 */
static inline PRTMEMBLOCK rtmemBlockDelayRemove(void)
{
    PRTMEMBLOCK pBlock = NULL;
    rtmemBlockLock();
    if (g_cbBlocksDelay > RTALLOC_EFENCE_FREE_DELAYED)
    {
        pBlock = g_pBlocksDelayTail;
        if (pBlock)
        {
            g_pBlocksDelayTail = (PRTMEMBLOCK)pBlock->Core.pLeft;
            if (pBlock->Core.pLeft)
                pBlock->Core.pLeft->pRight = NULL;
            else
                g_pBlocksDelayHead = NULL;
            g_cbBlocksDelay -= RT_ALIGN_Z(pBlock->cb, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
        }
    }
    rtmemBlockUnlock();
    return pBlock;
}


#endif  /* DELAY */

#endif /* RTALLOC_EFENCE_TRACE */


/**
 * Internal allocator.
 */
RTDECL(void *) rtMemAlloc(const char *pszOp, RTMEMTYPE enmType, size_t cb, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction)
{
    /*
     * Sanity.
     */
    if (    RT_ALIGN_Z(RTALLOC_EFENCE_SIZE, PAGE_SIZE) != RTALLOC_EFENCE_SIZE
        &&  RTALLOC_EFENCE_SIZE <= 0)
    {
        rtmemComplain(pszOp, "Invalid E-fence size! %#x\n", RTALLOC_EFENCE_SIZE);
        return NULL;
    }
    if (!cb)
    {
#if 0
        rtmemComplain(pszOp, "Request of ZERO bytes allocation!\n");
        return NULL;
#else
        cb = 1;
#endif
    }

#ifdef RTALLOC_EFENCE_TRACE
    /*
     * Allocate the trace block.
     */
    PRTMEMBLOCK pBlock = rtmemBlockCreate(enmType, cb, pvCaller, iLine, pszFile, pszFunction);
    if (!pBlock)
    {
        rtmemComplain(pszOp, "Failed to allocate trace block!\n");
        return NULL;
    }
#endif

    /*
     * Allocate a block with page alignment space + the size of the E-fence.
     */
    size_t  cbBlock = RT_ALIGN_Z(cb, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
    void   *pvBlock = RTMemPageAlloc(cbBlock);
    if (pvBlock)
    {
        /*
         * Calc the start of the fence and the user block
         * and then change the page protection of the fence.
         */
        #ifdef RTALLOC_EFENCE_IN_FRONT
        void *pvEFence = pvBlock;
        void *pv = (char *)pvEFence + RTALLOC_EFENCE_SIZE;
        #else
        void *pvEFence = (char *)pvBlock + (cbBlock - RTALLOC_EFENCE_SIZE);
        void *pv = (char *)pvEFence - cb;
        #endif
        int rc = RTMemProtect(pvEFence, RTALLOC_EFENCE_SIZE, RTMEM_PROT_NONE);
        if (!rc)
        {
            #ifdef RTALLOC_EFENCE_TRACE
            rtmemBlockInsert(pBlock, pv);
            #endif
            if (enmType == RTMEMTYPE_RTMEMALLOCZ)
                memset(pv, 0, cb);
#ifdef RTALLOC_EFENCE_FILLER
            else
                memset(pv, RTALLOC_EFENCE_FILLER, cb);
#endif

            rtmemLog(pszOp, "returns %p (pvBlock=%p cbBlock=%#x pvEFence=%p cb=%#x)\n", pv, pvBlock, cbBlock, pvEFence, cb);
            return pv;
        }
        rtmemComplain(pszOp, "RTMemProtect failed, pvEFence=%p size %d, rc=%d\n", pvEFence, RTALLOC_EFENCE_SIZE, rc);
        RTMemPageFree(pvBlock);
    }
    else
        rtmemComplain(pszOp, "Failed to allocated %d bytes.\n", cb);

#ifdef RTALLOC_EFENCE_TRACE
    rtmemBlockFree(pBlock);
#endif
    return NULL;
}


/**
 * Internal free.
 */
RTDECL(void) rtMemFree(const char *pszOp, RTMEMTYPE enmType, void *pv, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction)
{
    /*
     * Simple case.
     */
    if (!pv)
        return;

    /*
     * Check watch points.
     */
    for (unsigned i = 0; i < ELEMENTS(gapvRTMemFreeWatch); i++)
        if (gapvRTMemFreeWatch[i] == pv)
            AssertReleaseBreakpoint();

#ifdef RTALLOC_EFENCE_TRACE
    /*
     * Find the block.
     */
    PRTMEMBLOCK pBlock = rtmemBlockRemove(pv);
    if (pBlock)
    {
        if (gfRTMemFreeLog)
            RTLogPrintf("RTMem %s: pv=%p pvCaller=%p cb=%#x\n", pszOp, pv, pvCaller, pBlock->cb);

    #ifdef RTALLOC_EFENCE_FREE_FILL
        /*
         * Fill the user part of the block.
         */
        memset(pv, RTALLOC_EFENCE_FREE_FILL, pBlock->cb);
    #endif

    #if defined(RTALLOC_EFENCE_FREE_DELAYED) && RTALLOC_EFENCE_FREE_DELAYED > 0
        /*
         * We're doing delayed freeing.
         * That means we'll expand the E-fence to cover the entire block.
         */
        int rc = RTMemProtect(pv, pBlock->cb, RTMEM_PROT_NONE);
        if (RT_SUCCESS(rc))
        {
            /*
             * Insert it into the free list and process pending frees.
             */
            rtmemBlockDelayInsert(pBlock);
            while ((pBlock = rtmemBlockDelayRemove()) != NULL)
            {
                pv = pBlock->Core.Key;
                #ifdef RTALLOC_EFENCE_IN_FRONT
                void *pvBlock = (char *)pv - RTALLOC_EFENCE_SIZE;
                #else
                void *pvBlock = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);
                #endif
                size_t cbBlock = RT_ALIGN_Z(pBlock->cb, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
                rc = RTMemProtect(pvBlock, cbBlock, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
                if (RT_SUCCESS(rc))
                    RTMemPageFree(pvBlock);
                else
                    rtmemComplain(pszOp, "RTMemProtect(%p, %#x, RTMEM_PROT_READ | RTMEM_PROT_WRITE) -> %d\n", pvBlock, cbBlock, rc);
                rtmemBlockFree(pBlock);
            }
        }
        else
            rtmemComplain(pszOp, "Failed to expand the efence of pv=%p cb=%d, rc=%d.\n", pv, pBlock, rc);

    #else /* !RTALLOC_EFENCE_FREE_DELAYED */

        /*
         * Turn of the E-fence and free it.
         */
        #ifdef RTALLOC_EFENCE_IN_FRONT
        void *pvBlock = (char *)pv - RTALLOC_EFENCE_SIZE;
        void *pvEFence = pvBlock;
        #else
        void *pvBlock = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);
        void *pvEFence = (char *)pv + pBlock->cb;
        #endif
        int rc = RTMemProtect(pvEFence, RTALLOC_EFENCE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
        if (RT_SUCCESS(rc))
            RTMemPageFree(pvBlock);
        else
            rtmemComplain(pszOp, "RTMemProtect(%p, %#x, RTMEM_PROT_READ | RTMEM_PROT_WRITE) -> %d\n", pvEFence, RTALLOC_EFENCE_SIZE, rc);
        rtmemBlockFree(pBlock);

    #endif /* !RTALLOC_EFENCE_FREE_DELAYED */
    }
    else
        rtmemComplain(pszOp, "pv=%p not found! Incorrect free!\n", pv);

#else /* !RTALLOC_EFENCE_TRACE */

    /*
     * We have no size tracking, so we're not doing any freeing because
     * we cannot if the E-fence is after the block.
     * Let's just expand the E-fence to the first page of the user bit
     * since we know that it's around.
     */
    int rc = RTMemProtect((void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK), PAGE_SIZE, RTMEM_PROT_NONE);
    if (RT_FAILURE(rc))
        rtmemComplain(pszOp, "RTMemProtect(%p, PAGE_SIZE, RTMEM_PROT_NONE) -> %d\n", (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK), rc);
#endif /* !RTALLOC_EFENCE_TRACE */
}

/**
 * Internal realloc.
 */
RTDECL(void *) rtMemRealloc(const char *pszOp, RTMEMTYPE enmType, void *pvOld, size_t cbNew, void *pvCaller, unsigned iLine, const char *pszFile, const char *pszFunction)
{
    /*
     * Allocate new and copy.
     */
    if (!pvOld)
        return rtMemAlloc(pszOp, enmType, cbNew, pvCaller, iLine, pszFile, pszFunction);
    if (!cbNew)
    {
        rtMemFree(pszOp, RTMEMTYPE_RTMEMREALLOC, pvOld, pvCaller, iLine, pszFile, pszFunction);
        return NULL;
    }

#ifdef RTALLOC_EFENCE_TRACE

    /*
     * Get the block, allocate the new, copy the data, free the old one.
     */
    PRTMEMBLOCK pBlock = rtmemBlockGet(pvOld);
    if (pBlock)
    {
        void *pvRet = rtMemAlloc(pszOp, enmType, cbNew, pvCaller, iLine, pszFile, pszFunction);
        if (pvRet)
        {
            memcpy(pvRet, pvOld, RT_MIN(cbNew, pBlock->cb));
            rtMemFree(pszOp, RTMEMTYPE_RTMEMREALLOC, pvOld, pvCaller, iLine, pszFile, pszFunction);
        }
        return pvRet;
    }
    else
        rtmemComplain(pszOp, "pvOld=%p was not found!\n", pvOld);
    return NULL;

#else /* !RTALLOC_EFENCE_TRACE */

    rtmemComplain(pszOp, "Not supported if RTALLOC_EFENCE_TRACE isn't defined!\n");
    return NULL;

#endif /* !RTALLOC_EFENCE_TRACE */
}




/**
 * Same as RTMemTmpAlloc() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfTmpAlloc(size_t cb)
{
    return RTMemEfAlloc(cb);
}


/**
 * Same as RTMemTmpAllocZ() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfTmpAllocZ(size_t cb)
{
    return RTMemEfAllocZ(cb);
}


/**
 * Same as RTMemTmpFree() except that it's for fenced memory.
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemEfTmpFree(void *pv)
{
    RTMemEfFree(pv);
}


/**
 * Same as RTMemAlloc() except that it's fenced.
 *
 * @returns Pointer to the allocated memory. Free with RTMemEfFree().
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfAlloc(size_t cb)
{
    return rtMemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cb, ((void **)&cb)[-1], 0, NULL, NULL);
}


/**
 * Same as RTMemAllocZ() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   cb      Size in bytes of the memory block to allocate.
 */
RTDECL(void *)  RTMemEfAllocZ(size_t cb)
{
    return rtMemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cb, ((void **)&cb)[-1], 0, NULL, NULL);
}


/**
 * Same as RTMemRealloc() except that it's fenced.
 *
 * @returns Pointer to the allocated memory.
 * @returns NULL on failure.
 * @param   pvOld   The memory block to reallocate.
 * @param   cbNew   The new block size (in bytes).
 */
RTDECL(void *)  RTMemEfRealloc(void *pvOld, size_t cbNew)
{
    return rtMemRealloc("Realloc", RTMEMTYPE_RTMEMREALLOC, pvOld, cbNew, ((void **)&pvOld)[-1], 0, NULL, NULL);
}


/**
 * Free memory allocated by any of the RTMemEf* allocators.
 *
 * @param   pv      Pointer to memory block.
 */
RTDECL(void)    RTMemEfFree(void *pv)
{
    if (pv)
        rtMemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ((void **)&pv)[-1], 0, NULL, NULL);
}


/**
 * Same as RTMemDup() except that it's fenced.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cb      The amount of memory to duplicate.
 */
RTDECL(void *) RTMemEfDup(const void *pvSrc, size_t cb)
{
    void *pvDst = RTMemEfAlloc(cb);
    if (pvDst)
        memcpy(pvDst, pvSrc, cb);
    return pvDst;
}


/**
 * Same as RTMemDupEx except that it's fenced.
 *
 * @returns New heap block with the duplicate data.
 * @returns NULL if we're out of memory.
 * @param   pvSrc   The memory to duplicate.
 * @param   cbSrc   The amount of memory to duplicate.
 * @param   cbExtra The amount of extra memory to allocate and zero.
 */
RTDECL(void *) RTMemEfDupEx(const void *pvSrc, size_t cbSrc, size_t cbExtra)
{
    void *pvDst = RTMemEfAlloc(cbSrc + cbExtra);
    if (pvDst)
    {
        memcpy(pvDst, pvSrc, cbSrc);
        memset((uint8_t *)pvDst + cbSrc, 0, cbExtra);
    }
    return pvDst;
}

