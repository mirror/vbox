/* $Id$ */
/** @file
 * VBoxGuestLibR0 - Physical memory heap.
 */

/*
 * Copyright (C) 2006-2022 Oracle and/or its affiliates.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "VBoxGuestR0LibInternal.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/semaphore.h>

/** @page pg_vbglr0_phys_heap   VBoxGuestLibR0 - Physical memory heap.
 *
 * The physical memory heap consists of a doubly linked list of large chunks
 * (VBGLDATA::pChunkHead), memory blocks are allocated within these chunks and
 * are members of allocated (VBGLDATA::pAllocBlocksHead) and free
 * (VBGLDATA::pFreeBlocksHead) doubly linked lists.
 *
 * When allocating a block, we search the free list for a suitable free block.
 * If there is no such block, a new chunk is allocated and the new block is
 * taken from the new chunk as the only chunk-sized free block. The allocated
 * block is unlinked from the free list and goes to alloc list.
 *
 * When freeing block, we check the pointer and then unlink the block from the
 * alloc list and move it to the free list.
 *
 * For each chunk we maintain the allocated blocks counter (as well as a count
 * of free blocks).  If 2 (or more) entire chunks are free they are immediately
 * deallocated, so we always have at most 1 free chunk.
 *
 * When freeing blocks, two subsequent free blocks are always merged together.
 * Current implementation merges blocks only when there is a free block after
 * the just freed one, never when there is one before it as that's too
 * expensive.
 */


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
#define VBGL_PH_ASSERT      Assert
#define VBGL_PH_ASSERT_MSG  AssertMsg

// #define DUMPHEAP

#ifdef DUMPHEAP
# define VBGL_PH_dprintf(a) RTAssertMsg2Weak a
#else
# define VBGL_PH_dprintf(a)
#endif

/* Heap block signature */
#define VBGL_PH_BLOCKSIGNATURE (0xADDBBBBB)


/* Heap chunk signature */
#define VBGL_PH_CHUNKSIGNATURE (0xADDCCCCC)
/* Heap chunk allocation unit */
#define VBGL_PH_CHUNKSIZE (0x10000)

/* Heap block bit flags */
#define VBGL_PH_BF_ALLOCATED (0x1)

/** Threshold at which to split out a tail free block when allocating.
 *
 * The value gives the amount of user space, i.e. excluding the header.
 *
 * Using 32 bytes based on VMMDev.h request sizes.  The smallest requests are 24
 * bytes, i.e. only the header, at least 4 of these.  There are at least 10 with
 * size 28 bytes and at least 11 with size 32 bytes.  So, 32 bytes would fit
 * some 25 requests out of about 60, which is reasonable.
 */
#define VBGL_PH_MIN_SPLIT_FREE_BLOCK    32


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
/**
 * A heap block (within a chunk).
 *
 * This is used to track a part of a heap chunk that's either free or
 * allocated.  The VBGLPHYSHEAPBLOCK::fAllocated member indicates which it is.
 */
struct VBGLPHYSHEAPBLOCK
{
    /** Magic value (VBGL_PH_BLOCKSIGNATURE). */
    uint32_t u32Signature;

    /** Size of user data in the block. Does not include this block header. */
    uint32_t cbDataSize : 31;
    /** The top bit indicates whether it's allocated or free. */
    uint32_t fAllocated : 1;

    /** Pointer to the next block on the list. */
    VBGLPHYSHEAPBLOCK  *pNext;
    /** Pointer to the previous block on the list. */
    VBGLPHYSHEAPBLOCK  *pPrev;
    /** Pointer back to the chunk. */
    VBGLPHYSHEAPCHUNK  *pChunk;
};

/**
 * A chunk of memory used by the heap for sub-allocations.
 *
 * There is a list of these.
 */
struct VBGLPHYSHEAPCHUNK
{
    /** Magic value (VBGL_PH_CHUNKSIGNATURE). */
    uint32_t u32Signature;

    /** Size of the chunk. Includes the chunk header. */
    uint32_t cbSize;

    /** Physical address of the chunk (contiguous). */
    uint32_t physAddr;

    uint32_t uPadding1;

    /** Indexed by VBGLPHYSHEAPBLOCK::fAllocated, so zero is the free ones and
     *  one the allocated ones. */
    int32_t acBlocks[2];

    /** Pointer to the next chunk. */
    VBGLPHYSHEAPCHUNK  *pNext;
    /** Pointer to the previous chunk. */
    VBGLPHYSHEAPCHUNK  *pPrev;
#if ARCH_BITS == 64
    /** Pad the size up to 64 bytes. */
    uintptr_t           auPadding2[3];
#endif
};
#if ARCH_BITS == 64
AssertCompileSize(VBGLPHYSHEAPCHUNK, 64);
#endif


#ifndef DUMPHEAP
# define dumpheap(pszWhere) do { } while (0)
#else
void dumpheap(const char *pszWhere)
{
   VBGL_PH_dprintf(("VBGL_PH dump at '%s'\n", pszWhere));

   VBGL_PH_dprintf(("Chunks:\n"));

   VBGLPHYSHEAPCHUNK *pChunk = g_vbgldata.pChunkHead;

   while (pChunk)
   {
       VBGL_PH_dprintf(("%p: pNext = %p, pPrev = %p, sign = %08X, size = %8d, allocated = %8d, phys = %08X\n",
                        pChunk, pChunk->pNext, pChunk->pPrev, pChunk->u32Signature, pChunk->cbSize, pChunk->cAllocatedBlocks, pChunk->physAddr));

       pChunk = pChunk->pNext;
   }

   VBGL_PH_dprintf(("Allocated blocks:\n"));

   VBGLPHYSHEAPBLOCK *pBlock = g_vbgldata.pAllocBlocksHead;

   while (pBlock)
   {
       VBGL_PH_dprintf(("%p: pNext = %p, pPrev = %p, sign = %08X, size = %8d, %s, pChunk = %p\n",
                        pBlock, pBlock->pNext, pBlock->pPrev, pBlock->u32Signature, pBlock->cbDataSize,
                        pBlock->fAllocated ? "allocated" : "free", pBlock->pChunk));

       pBlock = pBlock->pNext;
   }

   VBGL_PH_dprintf(("Free blocks:\n"));

   pBlock = g_vbgldata.pFreeBlocksHead;

   while (pBlock)
   {
       VBGL_PH_dprintf(("%p: pNext = %p, pPrev = %p, sign = %08X, size = %8d, %s, pChunk = %p\n",
                        pBlock, pBlock->pNext, pBlock->pPrev, pBlock->u32Signature, pBlock->cbDataSize,
                        pBlock->fAllocated ? "allocated" : "free", pBlock->pChunk));

       pBlock = pBlock->pNext;
   }

   VBGL_PH_dprintf(("VBGL_PH dump at '%s' done\n", pszWhere));
}
#endif


DECLINLINE(void) vbglPhysHeapLeave(void)
{
    RTSemFastMutexRelease(g_vbgldata.mutexHeap);
}


static void vbglPhysHeapInitFreeBlock(VBGLPHYSHEAPBLOCK *pBlock, VBGLPHYSHEAPCHUNK *pChunk, uint32_t cbDataSize)
{
    VBGL_PH_ASSERT(pBlock != NULL);
    VBGL_PH_ASSERT(pChunk != NULL);

    pBlock->u32Signature = VBGL_PH_BLOCKSIGNATURE;
    pBlock->cbDataSize   = cbDataSize;
    pBlock->fAllocated   = false;
    pBlock->pNext        = NULL;
    pBlock->pPrev        = NULL;
    pBlock->pChunk       = pChunk;
}


/**
 * Links @a pBlock onto the appropriate chain accoring to @a pInsertAfter or @a
 * pBlock->fAllocated.
 *
 * This also update the per-chunk block counts.
 */
static void vbglPhysHeapInsertBlock(VBGLPHYSHEAPBLOCK *pInsertAfter, VBGLPHYSHEAPBLOCK *pBlock)
{
    bool const fAllocated = pBlock->fAllocated;

    VBGL_PH_ASSERT_MSG(pBlock->pNext == NULL, ("pBlock->pNext = %p\n", pBlock->pNext));
    VBGL_PH_ASSERT_MSG(pBlock->pPrev == NULL, ("pBlock->pPrev = %p\n", pBlock->pPrev));

    if (pInsertAfter)
    {
        pBlock->pNext = pInsertAfter->pNext;
        pBlock->pPrev = pInsertAfter;

        if (pInsertAfter->pNext)
            pInsertAfter->pNext->pPrev = pBlock;

        pInsertAfter->pNext = pBlock;
    }
    else
    {
        /* inserting to head of list */
        pBlock->pPrev = NULL;

        if (fAllocated)
        {
            pBlock->pNext = g_vbgldata.pAllocBlocksHead;

            if (g_vbgldata.pAllocBlocksHead)
                g_vbgldata.pAllocBlocksHead->pPrev = pBlock;

            g_vbgldata.pAllocBlocksHead = pBlock;
        }
        else
        {
            pBlock->pNext = g_vbgldata.pFreeBlocksHead;

            if (g_vbgldata.pFreeBlocksHead)
                g_vbgldata.pFreeBlocksHead->pPrev = pBlock;

            g_vbgldata.pFreeBlocksHead = pBlock;
        }
    }

    /* Update the block counts: */
    g_vbgldata.acBlocks[fAllocated]      += 1;
    pBlock->pChunk->acBlocks[fAllocated] += 1;
    AssertMsg(   (uint32_t)pBlock->pChunk->acBlocks[fAllocated]
              <= pBlock->pChunk->cbSize / (sizeof(*pBlock) + sizeof(void *)),
              ("pChunk=%p: cbSize=%#x acBlocks[%u]=%d\n",
               pBlock->pChunk, pBlock->pChunk->cbSize, fAllocated, pBlock->pChunk->acBlocks[fAllocated]));
}


/**
 * Unlinks @a pBlock from the chain it's on.
 *
 * This also update the per-chunk block counts.
 */
static void vbglPhysHeapUnlinkBlock(VBGLPHYSHEAPBLOCK *pBlock)
{
    bool const fAllocated = pBlock->fAllocated;

    if (pBlock->pNext)
        pBlock->pNext->pPrev = pBlock->pPrev;
    /* else: this is tail of list but we do not maintain tails of block lists. so nothing to do. */

    if (pBlock->pPrev)
        pBlock->pPrev->pNext = pBlock->pNext;
    else if (fAllocated)
    {
        Assert(g_vbgldata.pAllocBlocksHead == pBlock);
        g_vbgldata.pAllocBlocksHead = pBlock->pNext;
    }
    else
    {
        Assert(g_vbgldata.pFreeBlocksHead == pBlock);
        g_vbgldata.pFreeBlocksHead = pBlock->pNext;
    }

    pBlock->pNext = NULL;
    pBlock->pPrev = NULL;

    /* Update the per-chunk counts: */
    g_vbgldata.acBlocks[fAllocated]      -= 1;
    pBlock->pChunk->acBlocks[fAllocated] -= 1;
    AssertMsg(pBlock->pChunk->acBlocks[fAllocated] >= 0,
              ("pChunk=%p: cbSize=%#x acBlocks[%u]=%d\n",
               pBlock->pChunk, pBlock->pChunk->cbSize, fAllocated, pBlock->pChunk->acBlocks[fAllocated]));
    Assert(g_vbgldata.acBlocks[fAllocated] >= 0);
}


static VBGLPHYSHEAPBLOCK *vbglPhysHeapChunkAlloc(uint32_t cbMinBlock)
{
    RTCCPHYS           PhysAddr = NIL_RTHCPHYS;
    VBGLPHYSHEAPCHUNK *pChunk;
    uint32_t           cbChunk;
    VBGL_PH_dprintf(("Allocating new chunk for %#x byte allocation\n", cbMinBlock));
    AssertReturn(cbMinBlock < _128M, NULL); /* paranoia */

    /* Compute the size of the new chunk, rounding up to next chunk size,
       which must be power of 2. */
    Assert(RT_IS_POWER_OF_TWO(VBGL_PH_CHUNKSIZE));
    cbChunk = cbMinBlock + sizeof(VBGLPHYSHEAPCHUNK) + sizeof(VBGLPHYSHEAPBLOCK);
    cbChunk = RT_ALIGN_32(cbChunk, VBGL_PH_CHUNKSIZE);

    /* This function allocates physical contiguous memory below 4 GB.  This 4GB
       limitation stems from using a 32-bit OUT instruction to pass a block
       physical address to the host. */
    pChunk = (VBGLPHYSHEAPCHUNK *)RTMemContAlloc(&PhysAddr, cbChunk);
    /** @todo retry with smaller size if it fails, treating VBGL_PH_CHUNKSIZE as
     *        a guideline rather than absolute minimum size. */
    if (pChunk)
    {
        VBGLPHYSHEAPCHUNK *pOldHeadChunk;
        VBGLPHYSHEAPBLOCK *pBlock;
        AssertRelease(PhysAddr < _4G && PhysAddr + cbChunk <= _4G);

        /* Init the new chunk. */
        pChunk->u32Signature     = VBGL_PH_CHUNKSIGNATURE;
        pChunk->cbSize           = cbChunk;
        pChunk->physAddr         = (uint32_t)PhysAddr;
        pChunk->acBlocks[0]      = 0;
        pChunk->acBlocks[1]      = 0;
        pChunk->pNext            = NULL;
        pChunk->pPrev            = NULL;
        pChunk->uPadding1        = UINT32_C(0xADDCAAA1);
#if ARCH_BITS == 64
        pChunk->auPadding2[0]    = UINT64_C(0xADDCAAA3ADDCAAA2);
        pChunk->auPadding2[1]    = UINT64_C(0xADDCAAA5ADDCAAA4);
        pChunk->auPadding2[2]    = UINT64_C(0xADDCAAA7ADDCAAA6);
#endif

        /* Initialize the free block, which now occupies entire chunk. */
        pBlock = (VBGLPHYSHEAPBLOCK *)(pChunk + 1);
        vbglPhysHeapInitFreeBlock(pBlock, pChunk, cbChunk - sizeof(VBGLPHYSHEAPCHUNK) - sizeof(VBGLPHYSHEAPBLOCK));
        vbglPhysHeapInsertBlock(NULL, pBlock);

        /* Add the chunk to the list. */
        pOldHeadChunk = g_vbgldata.pChunkHead;
        pChunk->pNext = pOldHeadChunk;
        if (pOldHeadChunk)
            pOldHeadChunk->pPrev = pChunk;
        g_vbgldata.pChunkHead    = pChunk;

        VBGL_PH_dprintf(("Allocated chunk %p LB %#x, block %p LB %#x\n", pChunk, cbChunk, pBlock, pBlock->cbDataSize));
        return pBlock;
    }
    LogRel(("vbglPhysHeapChunkAlloc: failed to alloc %u (%#x) contiguous bytes.\n", cbChunk, cbChunk));
    return NULL;
}


static void vbglPhysHeapChunkDelete(VBGLPHYSHEAPCHUNK *pChunk)
{
    uintptr_t  uEnd, uCur;
    VBGL_PH_ASSERT(pChunk != NULL);
    VBGL_PH_ASSERT_MSG(pChunk->u32Signature == VBGL_PH_CHUNKSIGNATURE, ("pChunk->u32Signature = %08X\n", pChunk->u32Signature));

    VBGL_PH_dprintf(("Deleting chunk %p size %x\n", pChunk, pChunk->cbSize));

    /* first scan the chunk and unlink all blocks from the lists */

    uEnd = (uintptr_t)pChunk + pChunk->cbSize;
    uCur = (uintptr_t)(pChunk + 1);

    while (uCur < uEnd)
    {
        VBGLPHYSHEAPBLOCK *pBlock = (VBGLPHYSHEAPBLOCK *)uCur;

        uCur += pBlock->cbDataSize + sizeof(VBGLPHYSHEAPBLOCK);

        vbglPhysHeapUnlinkBlock(pBlock);
    }

    VBGL_PH_ASSERT_MSG(uCur == uEnd, ("uCur = %p, uEnd = %p, pChunk->cbSize = %08X\n", uCur, uEnd, pChunk->cbSize));

    /* Unlink the chunk from the chunk list. */
    if (pChunk->pNext)
        pChunk->pNext->pPrev = pChunk->pPrev;
    /* else: we do not maintain tail pointer. */

    if (pChunk->pPrev)
        pChunk->pPrev->pNext = pChunk->pNext;
    else
    {
        Assert(g_vbgldata.pChunkHead == pChunk);
        g_vbgldata.pChunkHead = pChunk->pNext;
    }

    RTMemContFree(pChunk, pChunk->cbSize);
}


DECLR0VBGL(void *) VbglR0PhysHeapAlloc(uint32_t cbSize)
{
    VBGLPHYSHEAPBLOCK *pBlock, *pIter;
    int rc;

    /*
     * Align the size to a pointer size to avoid getting misaligned header pointers and whatnot.
     */
    cbSize = RT_ALIGN_32(cbSize, sizeof(void *));
    AssertStmt(cbSize > 0, cbSize = sizeof(void *));  /* avoid allocating zero bytes */


    rc = RTSemFastMutexRequest(g_vbgldata.mutexHeap);
    AssertRCReturn(rc, NULL);

    dumpheap("pre alloc");

    /*
     * Search the free list.  We do this in linear fashion as we don't expect
     * there to be many blocks in the heap.
     */
    /** @todo r=bird: Don't walk these lists for ever, use the block count
     *        statistics to limit the walking to the first X or something. */
    pBlock = NULL;
    if (cbSize <= PAGE_SIZE / 4 * 3)
    {
        /* Smaller than 3/4 page:  Prefer a free block that can keep the request within a single page,
           so HGCM processing in VMMDev can use page locks instead of several reads and writes. */
        VBGLPHYSHEAPBLOCK *pFallback = NULL;
        for (pIter = g_vbgldata.pFreeBlocksHead; pIter != NULL; pIter = pIter->pNext)
        {
            AssertBreak(pIter->u32Signature == VBGL_PH_BLOCKSIGNATURE);
            if (pIter->cbDataSize >= cbSize)
            {
                if (pIter->cbDataSize == cbSize)
                {
                    if (PAGE_SIZE - ((uintptr_t)(pIter + 1) & PAGE_OFFSET_MASK) >= cbSize)
                    {
                        pBlock = pIter;
                        break;
                    }
                    pFallback = pIter;
                }
                else
                {
                    if (!pFallback || pIter->cbDataSize < pFallback->cbDataSize)
                        pFallback = pIter;
                    if (PAGE_SIZE - ((uintptr_t)(pIter + 1) & PAGE_OFFSET_MASK) >= cbSize)
                        if (!pBlock || pIter->cbDataSize < pBlock->cbDataSize)
                            pBlock = pIter;
                }
            }
        }

        if (!pBlock)
            pBlock = pFallback;
    }
    else
    {
        /* Large than 3/4 page:  Find closest free list match. */
        for (pIter = g_vbgldata.pFreeBlocksHead; pIter != NULL; pIter = pIter->pNext)
        {
            AssertBreak(pIter->u32Signature == VBGL_PH_BLOCKSIGNATURE);
            if (pIter->cbDataSize >= cbSize)
            {
                if (pIter->cbDataSize == cbSize)
                {
                    /* Exact match - we're done! */
                    pBlock = pIter;
                    break;
                }

                /* Looking for a free block with nearest size. */
                if (!pBlock || pIter->cbDataSize < pBlock->cbDataSize)
                    pBlock = pIter;
            }
        }
    }

    if (!pBlock)
    {
        /* No free blocks, allocate a new chunk, the only free block of the
           chunk will be returned. */
        pBlock = vbglPhysHeapChunkAlloc(cbSize);
    }

    if (pBlock)
    {
        /* We have a free block, either found or allocated. */
        VBGL_PH_ASSERT_MSG(pBlock->u32Signature == VBGL_PH_BLOCKSIGNATURE,
                           ("pBlock = %p, pBlock->u32Signature = %08X\n", pBlock, pBlock->u32Signature));
        VBGL_PH_ASSERT_MSG(!pBlock->fAllocated, ("pBlock = %p\n", pBlock));

        /*
         * If the block is too large, split off a free block with the unused space.
         *
         * We do this before unlinking the block so we can preserve the location
         * in the free list.
         */
        if (pBlock->cbDataSize >= sizeof(VBGLPHYSHEAPBLOCK) * 2 + VBGL_PH_MIN_SPLIT_FREE_BLOCK + cbSize)
        {
            pIter = (VBGLPHYSHEAPBLOCK *)((uintptr_t)(pBlock + 1) + cbSize);
            vbglPhysHeapInitFreeBlock(pIter, pBlock->pChunk, pBlock->cbDataSize - cbSize - sizeof(VBGLPHYSHEAPBLOCK));

            pBlock->cbDataSize = cbSize;

            /* Insert the new 'pIter' block after the 'pBlock' in the free list. */
            vbglPhysHeapInsertBlock(pBlock, pIter);
        }

        /*
         * Unlink the block from the free list, mark it as allocated and insert
         * it in the allocated list.
         */
        vbglPhysHeapUnlinkBlock(pBlock);
        pBlock->fAllocated = true;
        vbglPhysHeapInsertBlock(NULL, pBlock);

        dumpheap("post alloc");

        rc = RTSemFastMutexRelease(g_vbgldata.mutexHeap);

        VBGL_PH_dprintf(("VbglR0PhysHeapAlloc: returns %p size %x\n", pBlock + 1, pBlock->cbDataSize));
        return pBlock + 1;
    }

    rc = RTSemFastMutexRelease(g_vbgldata.mutexHeap);
    AssertRC(rc);

    VBGL_PH_dprintf(("VbglR0PhysHeapAlloc: returns NULL (requested %#x bytes)\n", cbSize));
    return NULL;
}


DECLR0VBGL(uint32_t) VbglR0PhysHeapGetPhysAddr(void *pv)
{
    /*
     * Validate the incoming pointer.
     */
    if (pv != NULL)
    {
        VBGLPHYSHEAPBLOCK *pBlock = (VBGLPHYSHEAPBLOCK *)pv - 1;
        if (   pBlock->u32Signature == VBGL_PH_BLOCKSIGNATURE
            && pBlock->fAllocated)
        {
            /*
             * Calculate and return its physical address.
             */
            return pBlock->pChunk->physAddr + (uint32_t)((uintptr_t)pv - (uintptr_t)pBlock->pChunk);
        }

        AssertMsgFailed(("Use after free or corrupt pointer variable: pv=%p pBlock=%p: u32Signature=%#x cb=%#x fAllocated=%d\n",
                         pv, pBlock, pBlock->u32Signature, pBlock->cbDataSize, pBlock->fAllocated));
    }
    else
        AssertMsgFailed(("Unexpected NULL pointer\n"));
    return 0;
}


DECLR0VBGL(void) VbglR0PhysHeapFree(void *pv)
{
    if (pv != NULL)
    {
        VBGLPHYSHEAPBLOCK *pBlock;

        int rc = RTSemFastMutexRequest(g_vbgldata.mutexHeap);
        AssertRCReturnVoid(rc);

        dumpheap("pre free");

        /*
         * Validate the block header.
         */
        pBlock = (VBGLPHYSHEAPBLOCK *)pv - 1;
        if (   pBlock->u32Signature == VBGL_PH_BLOCKSIGNATURE
            && pBlock->fAllocated)
        {
            VBGLPHYSHEAPCHUNK *pChunk;
            VBGLPHYSHEAPBLOCK *pNeighbour;

            /*
             * Move the block from the allocated list to the free list.
             */
            VBGL_PH_dprintf(("VbglR0PhysHeapFree: %p size %#x\n", pv, pBlock->cbDataSize));
            vbglPhysHeapUnlinkBlock(pBlock);

            dumpheap("post unlink");

            pBlock->fAllocated = false;
            vbglPhysHeapInsertBlock(NULL, pBlock);

            dumpheap("post insert");

            /*
             * Check if the block after this one is also free and we can merge it into
             * this one.
             *
             * Because the free list isn't sorted by address we cannot cheaply do the
             * same for the block before us, so we have to hope for the best for now.
             */
            /** @todo When the free:used ration in chunk is too skewed, scan the whole
             *        chunk and merge adjacent blocks that way every so often.  Always do so
             *        when this is the last used one and we end up with more than 1 free
             *        node afterwards. */
            pChunk = pBlock->pChunk;
            pNeighbour = (VBGLPHYSHEAPBLOCK *)((uintptr_t)(pBlock + 1) + pBlock->cbDataSize);
            if (   (uintptr_t)pNeighbour <= (uintptr_t)pChunk + pChunk->cbSize - sizeof(*pNeighbour)
                && !pNeighbour->fAllocated)
            {
                /* Adjust size of current memory block */
                pBlock->cbDataSize += pNeighbour->cbDataSize + sizeof(VBGLPHYSHEAPBLOCK);

                /* Unlink the following node and invalid it. */
                vbglPhysHeapUnlinkBlock(pNeighbour);

                pNeighbour->u32Signature = ~VBGL_PH_BLOCKSIGNATURE;
                pNeighbour->cbDataSize   = UINT32_MAX / 4;

                dumpheap("post merge");
            }

            /*
             * If this chunk is now completely unused, delete it if there are
             * more completely free ones.
             */
            if (   pChunk->acBlocks[1 /*allocated*/] == 0
                && (pChunk->pPrev || pChunk->pNext))
            {
                VBGLPHYSHEAPCHUNK *pCurChunk;
                uint32_t           cUnusedChunks = 0;
                for (pCurChunk = g_vbgldata.pChunkHead; pCurChunk; pCurChunk = pCurChunk->pNext)
                {
                    AssertBreak(pCurChunk->u32Signature == VBGL_PH_CHUNKSIGNATURE);
                    if (pCurChunk->acBlocks[1 /*allocated*/] == 0)
                    {
                        cUnusedChunks++;
                        if (cUnusedChunks > 1)
                        {
                            /* Delete current chunk, it will also unlink all free blocks
                             * remaining in the chunk from the free list, so the pBlock
                             * will also be invalid after this.
                             */
                            vbglPhysHeapChunkDelete(pChunk);
                            pNeighbour = pBlock = NULL; /* invalid */
                            break;
                        }
                    }
                }
            }

            dumpheap("post free");
        }
        else
            AssertMsgFailed(("pBlock: %p: u32Signature=%#x cb=%#x fAllocated=%d - double free?\n",
                             pBlock, pBlock->u32Signature, pBlock->cbDataSize, pBlock->fAllocated));

        rc = RTSemFastMutexRelease(g_vbgldata.mutexHeap);
        AssertRC(rc);
    }
}

#ifdef IN_TESTCASE /* For the testcase only */
# include <iprt/err.h>

/**
 * Returns the sum of all free heap blocks.
 *
 * This is the amount of memory you can theoretically allocate if you do
 * allocations exactly matching the free blocks.
 *
 * @returns The size of the free blocks.
 * @returns 0 if heap was safely detected as being bad.
 */
DECLVBGL(size_t) VbglR0PhysHeapGetFreeSize(void)
{
    int rc = RTSemFastMutexRequest(g_vbgldata.mutexHeap);
    AssertRCReturn(rc, 0);

    size_t cbTotal = 0;
    for (VBGLPHYSHEAPBLOCK *pCurBlock = g_vbgldata.pFreeBlocksHead; pCurBlock; pCurBlock = pCurBlock->pNext)
    {
        Assert(pCurBlock->u32Signature == VBGL_PH_BLOCKSIGNATURE);
        cbTotal += pCurBlock->cbDataSize;
    }

    RTSemFastMutexRelease(g_vbgldata.mutexHeap);
    return cbTotal;
}


/**
 * Checks the heap, caller responsible for locking.
 *
 * @returns VINF_SUCCESS if okay, error status if not.
 * @param   pErrInfo    Where to return more error details, optional.
 */
static int vbglR0PhysHeapCheckLocked(PRTERRINFO pErrInfo)
{
    /*
     * Scan the blocks in each chunk.
     */
    unsigned acTotalBlocks[2] = { 0, 0 };
    for (VBGLPHYSHEAPCHUNK *pCurChunk = g_vbgldata.pChunkHead, *pPrevChunk = NULL; pCurChunk; pCurChunk = pCurChunk->pNext)
    {
        AssertReturn(pCurChunk->u32Signature == VBGL_PH_CHUNKSIGNATURE,
                     RTErrInfoSetF(pErrInfo, VERR_INVALID_MAGIC, "pCurChunk=%p: magic=%#x", pCurChunk, pCurChunk->u32Signature));
        AssertReturn(pCurChunk->pPrev == pPrevChunk,
                     RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_2,
                                   "pCurChunk=%p: pPrev=%p, expected %p", pCurChunk, pCurChunk->pPrev, pPrevChunk));

        uintptr_t const          uEnd        = (uintptr_t)pCurChunk + pCurChunk->cbSize;
        const VBGLPHYSHEAPBLOCK *pCurBlock   = (const VBGLPHYSHEAPBLOCK *)(pCurChunk + 1);
        unsigned                 acBlocks[2] = { 0, 0 };
        while ((uintptr_t)pCurBlock < uEnd)
        {
            AssertReturn(pCurBlock->u32Signature == VBGL_PH_BLOCKSIGNATURE,
                         RTErrInfoSetF(pErrInfo, VERR_INVALID_MAGIC,
                                       "pCurBlock=%p: magic=%#x", pCurBlock, pCurBlock->u32Signature));
            AssertReturn(pCurBlock->pChunk == pCurChunk,
                         RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_2,
                                       "pCurBlock=%p: pChunk=%p, expected %p", pCurBlock, pCurBlock->pChunk, pCurChunk));
            AssertReturn(   pCurBlock->cbDataSize >= sizeof(void *)
                         && pCurBlock->cbDataSize < _128M
                         && RT_ALIGN_32(pCurBlock->cbDataSize, sizeof(void *)) == pCurBlock->cbDataSize,
                         RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_3,
                                       "pCurBlock=%p: cbDataSize=%#x", pCurBlock, pCurBlock->cbDataSize));
            acBlocks[pCurBlock->fAllocated] += 1;

            /* advance */
            pCurBlock = (const VBGLPHYSHEAPBLOCK *)((uintptr_t)(pCurBlock + 1) + pCurBlock->cbDataSize);
        }
        AssertReturn((uintptr_t)pCurBlock == uEnd,
                     RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_4,
                                   "pCurBlock=%p uEnd=%p", pCurBlock, uEnd));

        acTotalBlocks[1] += acBlocks[1];
        AssertReturn(acBlocks[1] == (uint32_t)pCurChunk->acBlocks[1],
                     RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_4,
                                   "pCurChunk=%p: cAllocatedBlocks=%u, expected %u",
                                   pCurChunk, pCurChunk->acBlocks[1], acBlocks[1]));

        acTotalBlocks[0] += acBlocks[0];
        AssertReturn(acBlocks[0] == (uint32_t)pCurChunk->acBlocks[0],
                     RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_5,
                                   "pCurChunk=%p: cFreeBlocks=%u, expected %u",
                                   pCurChunk, pCurChunk->acBlocks[0], acBlocks[0]));

        pPrevChunk = pCurChunk;
    }

    AssertReturn(acTotalBlocks[0] == (uint32_t)g_vbgldata.acBlocks[0],
                 RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR,
                               "g_vbgldata.acBlocks[0]=%u, expected %u", g_vbgldata.acBlocks[0], acTotalBlocks[0]));
    AssertReturn(acTotalBlocks[1] == (uint32_t)g_vbgldata.acBlocks[1],
                 RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR,
                               "g_vbgldata.acBlocks[1]=%u, expected %u", g_vbgldata.acBlocks[1], acTotalBlocks[1]));

    /*
     * Count each list and check that they contain the same number of nodes.
     */
    VBGLPHYSHEAPBLOCK *apHeads[2] = { g_vbgldata.pFreeBlocksHead, g_vbgldata.pAllocBlocksHead };
    for (unsigned iType = 0; iType < RT_ELEMENTS(apHeads); iType++)
    {
        unsigned           cBlocks    = 0;
        VBGLPHYSHEAPBLOCK *pPrevBlock = NULL;
        for (VBGLPHYSHEAPBLOCK *pCurBlock = apHeads[iType]; pCurBlock; pCurBlock = pCurBlock->pNext)
        {
            AssertReturn(pCurBlock->u32Signature == VBGL_PH_BLOCKSIGNATURE,
                         RTErrInfoSetF(pErrInfo, VERR_INVALID_MAGIC,
                                       "pCurBlock=%p/%u: magic=%#x", pCurBlock, iType, pCurBlock->u32Signature));
            AssertReturn(pCurBlock->pPrev == pPrevBlock,
                         RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_2,
                                       "pCurBlock=%p/%u: pPrev=%p, expected %p", pCurBlock, iType, pCurBlock->pPrev, pPrevBlock));
            AssertReturn(pCurBlock->pChunk->u32Signature == VBGL_PH_CHUNKSIGNATURE,
                         RTErrInfoSetF(pErrInfo, VERR_INVALID_MAGIC, "pCurBlock=%p/%u: chunk (%p) magic=%#x",
                                       pCurBlock, iType, pCurBlock->pChunk, pCurBlock->pChunk->u32Signature));
            cBlocks++;
            pPrevBlock = pCurBlock;
        }

        AssertReturn(cBlocks == acTotalBlocks[iType],
                     RTErrInfoSetF(pErrInfo, VERR_INTERNAL_ERROR_3,
                                   "iType=%u: Found %u in list, expected %u", iType, cBlocks, acTotalBlocks[iType]));
    }

    return VINF_SUCCESS;
}


/**
 * Performs a heap check.
 *
 * @returns Problem description on failure, NULL on success.
 * @param   pErrInfo    Where to return more error details, optional.
 */
DECLVBGL(int) VbglR0PhysHeapCheck(PRTERRINFO pErrInfo)
{
    int rc = RTSemFastMutexRequest(g_vbgldata.mutexHeap);
    AssertRCReturn(rc, 0);

    rc = vbglR0PhysHeapCheckLocked(pErrInfo);

    RTSemFastMutexRelease(g_vbgldata.mutexHeap);
    return rc;
}

#endif /* IN_TESTCASE */

DECLR0VBGL(int) VbglR0PhysHeapInit(void)
{
    g_vbgldata.mutexHeap = NIL_RTSEMFASTMUTEX;

    /* Allocate the first chunk of the heap. */
    VBGLPHYSHEAPBLOCK *pBlock = vbglPhysHeapChunkAlloc(0);
    if (pBlock)
        return RTSemFastMutexCreate(&g_vbgldata.mutexHeap);
    return VERR_NO_CONT_MEMORY;
}

DECLR0VBGL(void) VbglR0PhysHeapTerminate(void)
{
    while (g_vbgldata.pChunkHead)
        vbglPhysHeapChunkDelete(g_vbgldata.pChunkHead);

    RTSemFastMutexDestroy(g_vbgldata.mutexHeap);
}

