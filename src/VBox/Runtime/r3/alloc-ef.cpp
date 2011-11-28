/* $Id$ */
/** @file
 * IPRT - Memory Allocation, electric fence.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <iprt/mem.h>
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
/** Spinlock protecting the all the block's globals. */
static volatile uint32_t    g_BlocksLock;
/** Tree tracking the allocations. */
static AVLPVTREE            g_BlocksTree;
# ifdef RTALLOC_EFENCE_FREE_DELAYED
/** Tail of the delayed blocks. */
static volatile PRTMEMBLOCK g_pBlocksDelayHead;
/** Tail of the delayed blocks. */
static volatile PRTMEMBLOCK g_pBlocksDelayTail;
/** Number of bytes in the delay list (includes fences). */
static volatile size_t      g_cbBlocksDelay;
# endif /* RTALLOC_EFENCE_FREE_DELAYED */
#endif /* RTALLOC_EFENCE_TRACE */
/** Array of pointers free watches for. */
void   *gapvRTMemFreeWatch[4] = {NULL, NULL, NULL, NULL};
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
    RTAssertDoPanic();
}

/**
 * Log an event.
 */
DECLINLINE(void) rtmemLog(const char *pszOp, const char *pszFormat, ...)
{
#if 0
    va_list args;
    fprintf(stderr, "RTMem info: %s: ", pszOp);
    va_start(args, pszFormat);
    vfprintf(stderr, pszFormat, args);
    va_end(args);
#else
    NOREF(pszOp); NOREF(pszFormat);
#endif
}


#ifdef RTALLOC_EFENCE_TRACE

/**
 * Acquires the lock.
 */
DECLINLINE(void) rtmemBlockLock(void)
{
    unsigned c = 0;
    while (!ASMAtomicCmpXchgU32(&g_BlocksLock, 1, 0))
        RTThreadSleepNoLog(((++c) >> 2) & 31);
}


/**
 * Releases the lock.
 */
DECLINLINE(void) rtmemBlockUnlock(void)
{
    Assert(g_BlocksLock == 1);
    ASMAtomicXchgU32(&g_BlocksLock, 0);
}


/**
 * Creates a block.
 */
DECLINLINE(PRTMEMBLOCK) rtmemBlockCreate(RTMEMTYPE enmType, size_t cbUnaligned, size_t cbAligned,
                                         const char *pszTag, void *pvCaller, RT_SRC_POS_DECL)
{
    PRTMEMBLOCK pBlock = (PRTMEMBLOCK)malloc(sizeof(*pBlock));
    if (pBlock)
    {
        pBlock->enmType     = enmType;
        pBlock->cbUnaligned = cbUnaligned;
        pBlock->cbAligned   = cbAligned;
        pBlock->pszTag      = pszTag;
        pBlock->pvCaller    = pvCaller;
        pBlock->iLine       = iLine;
        pBlock->pszFile     = pszFile;
        pBlock->pszFunction = pszFunction;
    }
    return pBlock;
}


/**
 * Frees a block.
 */
DECLINLINE(void) rtmemBlockFree(PRTMEMBLOCK pBlock)
{
    free(pBlock);
}


/**
 * Insert a block from the tree.
 */
DECLINLINE(void) rtmemBlockInsert(PRTMEMBLOCK pBlock, void *pv)
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
DECLINLINE(PRTMEMBLOCK) rtmemBlockRemove(void *pv)
{
    rtmemBlockLock();
    PRTMEMBLOCK pBlock = (PRTMEMBLOCK)RTAvlPVRemove(&g_BlocksTree, pv);
    rtmemBlockUnlock();
    return pBlock;
}

/**
 * Gets a block.
 */
DECLINLINE(PRTMEMBLOCK) rtmemBlockGet(void *pv)
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
    fprintf(stderr, "%p %08lx(+%02lx) %p\n",
            pBlock->Core.Key,
            (unsigned long)pBlock->cbUnaligned,
            (unsigned long)(pBlock->cbAligned - pBlock->cbUnaligned),
            pBlock->pvCaller);
    NOREF(pvUser);
    return 0;
}

/**
 * Dumps the allocated blocks.
 * This is something which you should call from gdb.
 */
extern "C" void RTMemDump(void);
void RTMemDump(void)
{
    fprintf(stderr, "address  size(alg)     caller\n");
    RTAvlPVDoWithAll(&g_BlocksTree, true, RTMemDumpOne, NULL);
}

# ifdef RTALLOC_EFENCE_FREE_DELAYED

/**
 * Insert a delayed block.
 */
DECLINLINE(void) rtmemBlockDelayInsert(PRTMEMBLOCK pBlock)
{
    size_t cbBlock = RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
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
DECLINLINE(PRTMEMBLOCK) rtmemBlockDelayRemove(void)
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
            g_cbBlocksDelay -= RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
        }
    }
    rtmemBlockUnlock();
    return pBlock;
}

# endif  /* RTALLOC_EFENCE_FREE_DELAYED */

#endif /* RTALLOC_EFENCE_TRACE */


/**
 * Internal allocator.
 */
RTDECL(void *) rtR3MemAlloc(const char *pszOp, RTMEMTYPE enmType, size_t cbUnaligned, size_t cbAligned,
                            const char *pszTag, void *pvCaller, RT_SRC_POS_DECL)
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
    if (!cbUnaligned)
    {
#if 0
        rtmemComplain(pszOp, "Request of ZERO bytes allocation!\n");
        return NULL;
#else
        cbAligned = cbUnaligned = 1;
#endif
    }

#ifndef RTALLOC_EFENCE_IN_FRONT
    /* Alignment decreases fence accuracy, but this is at least partially
     * counteracted by filling and checking the alignment padding. When the
     * fence is in front then then no extra alignment is needed. */
    cbAligned = RT_ALIGN_Z(cbAligned, RTALLOC_EFENCE_ALIGNMENT);
#endif

#ifdef RTALLOC_EFENCE_TRACE
    /*
     * Allocate the trace block.
     */
    PRTMEMBLOCK pBlock = rtmemBlockCreate(enmType, cbUnaligned, cbAligned, pszTag, pvCaller, RT_SRC_POS_ARGS);
    if (!pBlock)
    {
        rtmemComplain(pszOp, "Failed to allocate trace block!\n");
        return NULL;
    }
#endif

    /*
     * Allocate a block with page alignment space + the size of the E-fence.
     */
    size_t  cbBlock = RT_ALIGN_Z(cbAligned, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
    void   *pvBlock = RTMemPageAlloc(cbBlock);
    if (pvBlock)
    {
        /*
         * Calc the start of the fence and the user block
         * and then change the page protection of the fence.
         */
#ifdef RTALLOC_EFENCE_IN_FRONT
        void *pvEFence = pvBlock;
        void *pv       = (char *)pvEFence + RTALLOC_EFENCE_SIZE;
# ifdef RTALLOC_EFENCE_NOMAN_FILLER
        memset((char *)pv + cbUnaligned, RTALLOC_EFENCE_NOMAN_FILLER, cbBlock - RTALLOC_EFENCE_SIZE - cbUnaligned);
# endif
#else
        void *pvEFence = (char *)pvBlock + (cbBlock - RTALLOC_EFENCE_SIZE);
        void *pv       = (char *)pvEFence - cbAligned;
# ifdef RTALLOC_EFENCE_NOMAN_FILLER
        memset(pvBlock, RTALLOC_EFENCE_NOMAN_FILLER, cbBlock - RTALLOC_EFENCE_SIZE - cbAligned);
        memset((char *)pv + cbUnaligned, RTALLOC_EFENCE_NOMAN_FILLER, cbAligned - cbUnaligned);
# endif
#endif

#ifdef RTALLOC_EFENCE_FENCE_FILLER
        memset(pvEFence, RTALLOC_EFENCE_FENCE_FILLER, RTALLOC_EFENCE_SIZE);
#endif
        int rc = RTMemProtect(pvEFence, RTALLOC_EFENCE_SIZE, RTMEM_PROT_NONE);
        if (!rc)
        {
#ifdef RTALLOC_EFENCE_TRACE
            rtmemBlockInsert(pBlock, pv);
#endif
            if (enmType == RTMEMTYPE_RTMEMALLOCZ)
                memset(pv, 0, cbUnaligned);
#ifdef RTALLOC_EFENCE_FILLER
            else
                memset(pv, RTALLOC_EFENCE_FILLER, cbUnaligned);
#endif

            rtmemLog(pszOp, "returns %p (pvBlock=%p cbBlock=%#x pvEFence=%p cbUnaligned=%#x)\n", pv, pvBlock, cbBlock, pvEFence, cbUnaligned);
            return pv;
        }
        rtmemComplain(pszOp, "RTMemProtect failed, pvEFence=%p size %d, rc=%d\n", pvEFence, RTALLOC_EFENCE_SIZE, rc);
        RTMemPageFree(pvBlock, cbBlock);
    }
    else
        rtmemComplain(pszOp, "Failed to allocated %lu (%lu) bytes.\n", (unsigned long)cbBlock, (unsigned long)cbUnaligned);

#ifdef RTALLOC_EFENCE_TRACE
    rtmemBlockFree(pBlock);
#endif
    return NULL;
}


/**
 * Internal free.
 */
RTDECL(void) rtR3MemFree(const char *pszOp, RTMEMTYPE enmType, void *pv, void *pvCaller, RT_SRC_POS_DECL)
{
    NOREF(enmType); RT_SRC_POS_NOREF();

    /*
     * Simple case.
     */
    if (!pv)
        return;

    /*
     * Check watch points.
     */
    for (unsigned i = 0; i < RT_ELEMENTS(gapvRTMemFreeWatch); i++)
        if (gapvRTMemFreeWatch[i] == pv)
            RTAssertDoPanic();

#ifdef RTALLOC_EFENCE_TRACE
    /*
     * Find the block.
     */
    PRTMEMBLOCK pBlock = rtmemBlockRemove(pv);
    if (pBlock)
    {
        if (gfRTMemFreeLog)
            RTLogPrintf("RTMem %s: pv=%p pvCaller=%p cbUnaligned=%#x\n", pszOp, pv, pvCaller, pBlock->cbUnaligned);

# ifdef RTALLOC_EFENCE_NOMAN_FILLER
        /*
         * Check whether the no man's land is untouched.
         */
#  ifdef RTALLOC_EFENCE_IN_FRONT
        void *pvWrong = ASMMemIsAll8((char *)pv + pBlock->cbUnaligned,
                                     RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) - pBlock->cbUnaligned,
                                     RTALLOC_EFENCE_NOMAN_FILLER);
#  else
        /* Alignment must match allocation alignment in rtMemAlloc(). */
        void  *pvWrong   = ASMMemIsAll8((char *)pv + pBlock->cbUnaligned,
                                        pBlock->cbAligned - pBlock->cbUnaligned,
                                        RTALLOC_EFENCE_NOMAN_FILLER);
        if (pvWrong)
            RTAssertDoPanic();
        pvWrong = ASMMemIsAll8((void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK),
                               RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) - pBlock->cbAligned,
                               RTALLOC_EFENCE_NOMAN_FILLER);
#  endif
        if (pvWrong)
            RTAssertDoPanic();
# endif

# ifdef RTALLOC_EFENCE_FREE_FILL
        /*
         * Fill the user part of the block.
         */
        memset(pv, RTALLOC_EFENCE_FREE_FILL, pBlock->cbUnaligned);
# endif

# if defined(RTALLOC_EFENCE_FREE_DELAYED) && RTALLOC_EFENCE_FREE_DELAYED > 0
        /*
         * We're doing delayed freeing.
         * That means we'll expand the E-fence to cover the entire block.
         */
        int rc = RTMemProtect(pv, pBlock->cbAligned, RTMEM_PROT_NONE);
        if (RT_SUCCESS(rc))
        {
            /*
             * Insert it into the free list and process pending frees.
             */
            rtmemBlockDelayInsert(pBlock);
            while ((pBlock = rtmemBlockDelayRemove()) != NULL)
            {
                pv = pBlock->Core.Key;
#  ifdef RTALLOC_EFENCE_IN_FRONT
                void  *pvBlock = (char *)pv - RTALLOC_EFENCE_SIZE;
#  else
                void  *pvBlock = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);
#  endif
                size_t cbBlock = RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) + RTALLOC_EFENCE_SIZE;
                rc = RTMemProtect(pvBlock, cbBlock, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
                if (RT_SUCCESS(rc))
                    RTMemPageFree(pvBlock, RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) + RTALLOC_EFENCE_SIZE);
                else
                    rtmemComplain(pszOp, "RTMemProtect(%p, %#x, RTMEM_PROT_READ | RTMEM_PROT_WRITE) -> %d\n", pvBlock, cbBlock, rc);
                rtmemBlockFree(pBlock);
            }
        }
        else
            rtmemComplain(pszOp, "Failed to expand the efence of pv=%p cb=%d, rc=%d.\n", pv, pBlock, rc);

# else /* !RTALLOC_EFENCE_FREE_DELAYED */

        /*
         * Turn of the E-fence and free it.
         */
#  ifdef RTALLOC_EFENCE_IN_FRONT
        void *pvBlock = (char *)pv - RTALLOC_EFENCE_SIZE;
        void *pvEFence = pvBlock;
#  else
        void *pvBlock = (void *)((uintptr_t)pv & ~PAGE_OFFSET_MASK);
        void *pvEFence = (char *)pv + pBlock->cb;
#  endif
        int rc = RTMemProtect(pvEFence, RTALLOC_EFENCE_SIZE, RTMEM_PROT_READ | RTMEM_PROT_WRITE);
        if (RT_SUCCESS(rc))
            RTMemPageFree(pvBlock, RT_ALIGN_Z(pBlock->cbAligned, PAGE_SIZE) + RTALLOC_EFENCE_SIZE);
        else
            rtmemComplain(pszOp, "RTMemProtect(%p, %#x, RTMEM_PROT_READ | RTMEM_PROT_WRITE) -> %d\n", pvEFence, RTALLOC_EFENCE_SIZE, rc);
        rtmemBlockFree(pBlock);

# endif /* !RTALLOC_EFENCE_FREE_DELAYED */
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
RTDECL(void *) rtR3MemRealloc(const char *pszOp, RTMEMTYPE enmType, void *pvOld, size_t cbNew,
                              const char *pszTag, void *pvCaller, RT_SRC_POS_DECL)
{
    /*
     * Allocate new and copy.
     */
    if (!pvOld)
        return rtR3MemAlloc(pszOp, enmType, cbNew, cbNew, pszTag, pvCaller, RT_SRC_POS_ARGS);
    if (!cbNew)
    {
        rtR3MemFree(pszOp, RTMEMTYPE_RTMEMREALLOC, pvOld, pvCaller, RT_SRC_POS_ARGS);
        return NULL;
    }

#ifdef RTALLOC_EFENCE_TRACE

    /*
     * Get the block, allocate the new, copy the data, free the old one.
     */
    PRTMEMBLOCK pBlock = rtmemBlockGet(pvOld);
    if (pBlock)
    {
        void *pvRet = rtR3MemAlloc(pszOp, enmType, cbNew, cbNew, pszTag, pvCaller, RT_SRC_POS_ARGS);
        if (pvRet)
        {
            memcpy(pvRet, pvOld, RT_MIN(cbNew, pBlock->cbUnaligned));
            rtR3MemFree(pszOp, RTMEMTYPE_RTMEMREALLOC, pvOld, pvCaller, RT_SRC_POS_ARGS);
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




RTDECL(void *)  RTMemEfTmpAlloc(size_t cb, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    return rtR3MemAlloc("TmpAlloc", RTMEMTYPE_RTMEMALLOC, cb, cb, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *)  RTMemEfTmpAllocZ(size_t cb, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    return rtR3MemAlloc("TmpAlloc", RTMEMTYPE_RTMEMALLOCZ, cb, cb, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void)    RTMemEfTmpFree(void *pv, RT_SRC_POS_DECL) RT_NO_THROW
{
    if (pv)
        rtR3MemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *)  RTMemEfAlloc(size_t cb, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    return rtR3MemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cb, cb, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *)  RTMemEfAllocZ(size_t cb, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    return rtR3MemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cb, cb, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *)  RTMemEfAllocVar(size_t cbUnaligned, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
    return rtR3MemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cbUnaligned, cbAligned, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *)  RTMemEfAllocZVar(size_t cbUnaligned, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
    return rtR3MemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cbUnaligned, cbAligned, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *)  RTMemEfRealloc(void *pvOld, size_t cbNew, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    return rtR3MemRealloc("Realloc", RTMEMTYPE_RTMEMREALLOC, pvOld, cbNew, pszTag, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void)    RTMemEfFree(void *pv, RT_SRC_POS_DECL) RT_NO_THROW
{
    if (pv)
        rtR3MemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ASMReturnAddress(), RT_SRC_POS_ARGS);
}


RTDECL(void *) RTMemEfDup(const void *pvSrc, size_t cb, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    void *pvDst = RTMemEfAlloc(cb, pszTag, RT_SRC_POS_ARGS);
    if (pvDst)
        memcpy(pvDst, pvSrc, cb);
    return pvDst;
}


RTDECL(void *) RTMemEfDupEx(const void *pvSrc, size_t cbSrc, size_t cbExtra, const char *pszTag, RT_SRC_POS_DECL) RT_NO_THROW
{
    void *pvDst = RTMemEfAlloc(cbSrc + cbExtra, pszTag, RT_SRC_POS_ARGS);
    if (pvDst)
    {
        memcpy(pvDst, pvSrc, cbSrc);
        memset((uint8_t *)pvDst + cbSrc, 0, cbExtra);
    }
    return pvDst;
}




/*
 *
 * The NP (no position) versions.
 *
 */



RTDECL(void *)  RTMemEfTmpAllocNP(size_t cb, const char *pszTag) RT_NO_THROW
{
    return rtR3MemAlloc("TmpAlloc", RTMEMTYPE_RTMEMALLOC, cb, cb, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *)  RTMemEfTmpAllocZNP(size_t cb, const char *pszTag) RT_NO_THROW
{
    return rtR3MemAlloc("TmpAllocZ", RTMEMTYPE_RTMEMALLOCZ, cb, cb, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void)    RTMemEfTmpFreeNP(void *pv) RT_NO_THROW
{
    if (pv)
        rtR3MemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *)  RTMemEfAllocNP(size_t cb, const char *pszTag) RT_NO_THROW
{
    return rtR3MemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cb, cb, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *)  RTMemEfAllocZNP(size_t cb, const char *pszTag) RT_NO_THROW
{
    return rtR3MemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cb, cb, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *)  RTMemEfAllocVarNP(size_t cbUnaligned, const char *pszTag) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
    return rtR3MemAlloc("Alloc", RTMEMTYPE_RTMEMALLOC, cbUnaligned, cbAligned, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *)  RTMemEfAllocZVarNP(size_t cbUnaligned, const char *pszTag) RT_NO_THROW
{
    size_t cbAligned;
    if (cbUnaligned >= 16)
        cbAligned = RT_ALIGN_Z(cbUnaligned, 16);
    else
        cbAligned = RT_ALIGN_Z(cbUnaligned, sizeof(void *));
    return rtR3MemAlloc("AllocZ", RTMEMTYPE_RTMEMALLOCZ, cbUnaligned, cbAligned, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *)  RTMemEfReallocNP(void *pvOld, size_t cbNew, const char *pszTag) RT_NO_THROW
{
    return rtR3MemRealloc("Realloc", RTMEMTYPE_RTMEMREALLOC, pvOld, cbNew, pszTag, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void)    RTMemEfFreeNP(void *pv) RT_NO_THROW
{
    if (pv)
        rtR3MemFree("Free", RTMEMTYPE_RTMEMFREE, pv, ASMReturnAddress(), NULL, 0, NULL);
}


RTDECL(void *) RTMemEfDupNP(const void *pvSrc, size_t cb, const char *pszTag) RT_NO_THROW
{
    void *pvDst = RTMemEfAlloc(cb, pszTag, NULL, 0, NULL);
    if (pvDst)
        memcpy(pvDst, pvSrc, cb);
    return pvDst;
}


RTDECL(void *) RTMemEfDupExNP(const void *pvSrc, size_t cbSrc, size_t cbExtra, const char *pszTag) RT_NO_THROW
{
    void *pvDst = RTMemEfAlloc(cbSrc + cbExtra, pszTag, NULL, 0, NULL);
    if (pvDst)
    {
        memcpy(pvDst, pvSrc, cbSrc);
        memset((uint8_t *)pvDst + cbSrc, 0, cbExtra);
    }
    return pvDst;
}

