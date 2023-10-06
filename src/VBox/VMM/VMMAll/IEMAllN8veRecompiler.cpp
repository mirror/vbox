/* $Id$ */
/** @file
 * IEM - Native Recompiler
 *
 * Logging group IEM_RE_NATIVE assignments:
 *      - Level 1  (Log)  : ...
 *      - Flow  (LogFlow) : ...
 *      - Level 2  (Log2) : ...
 *      - Level 3  (Log3) : ...
 *      - Level 4  (Log4) : ...
 *      - Level 5  (Log5) : ...
 *      - Level 6  (Log6) : ...
 *      - Level 7  (Log7) : ...
 *      - Level 8  (Log8) : ...
 *      - Level 9  (Log9) : ...
 *      - Level 10 (Log10): ...
 *      - Level 11 (Log11): ...
 *      - Level 12 (Log12): ...
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_IEM_RE_THREADED
#define IEM_WITH_OPAQUE_DECODER_STATE
#define VMCPU_INCL_CPUM_GST_CTX
#define VMM_INCLUDED_SRC_include_IEMMc_h /* block IEMMc.h inclusion. */
#include <VBox/vmm/iem.h>
#include <VBox/vmm/cpum.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <iprt/assert.h>
#include <iprt/heap.h>
#include <iprt/mem.h>
#include <iprt/string.h>
#if   defined(RT_ARCH_AMD64)
# include <iprt/x86.h>
#elif defined(RT_ARCH_ARM64)
# include <iprt/armv8.h>
#endif

#ifdef RT_OS_WINDOWS
# include <iprt/formats/pecoff.h> /* this is incomaptible with windows.h, thus: */
extern "C" DECLIMPORT(uint8_t) __cdecl RtlAddFunctionTable(void *pvFunctionTable, uint32_t cEntries, uintptr_t uBaseAddress);
extern "C" DECLIMPORT(uint8_t) __cdecl RtlDelFunctionTable(void *pvFunctionTable);
#else
# include <iprt/formats/dwarf.h>
# if defined(RT_OS_DARWIN)
#  include <libkern/OSCacheControl.h>
#  define IEMNATIVE_USE_LIBUNWIND
extern "C" void  __register_frame(const void *pvFde);
extern "C" void  __deregister_frame(const void *pvFde);
# else
extern "C" void  __register_frame_info(void *pvBegin, void *pvObj); /* found no header for these two */
extern "C" void *__deregister_frame_info(void *pvBegin);           /* (returns pvObj from __register_frame_info call) */
# endif
#endif

#include "IEMInline.h"
#include "IEMThreadedFunctions.h"
#include "IEMN8veRecompiler.h"
#include "IEMNativeFunctions.h"


/*
 * Narrow down configs here to avoid wasting time on unused configs here.
 * Note! Same checks in IEMAllThrdRecompiler.cpp.
 */

#ifndef IEM_WITH_CODE_TLB
# error The code TLB must be enabled for the recompiler.
#endif

#ifndef IEM_WITH_DATA_TLB
# error The data TLB must be enabled for the recompiler.
#endif

#ifndef IEM_WITH_SETJMP
# error The setjmp approach must be enabled for the recompiler.
#endif


/*********************************************************************************************************************************
*   Executable Memory Allocator                                                                                                  *
*********************************************************************************************************************************/
/** @def IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
 * Use an alternative chunk sub-allocator that does store internal data
 * in the chunk.
 *
 * Using the RTHeapSimple is not practial on newer darwin systems where
 * RTMEM_PROT_WRITE and RTMEM_PROT_EXEC are mutually exclusive in process
 * memory.  We would have to change the protection of the whole chunk for
 * every call to RTHeapSimple, which would be rather expensive.
 *
 * This alternative implemenation let restrict page protection modifications
 * to the pages backing the executable memory we just allocated.
 */
#define IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
/** The chunk sub-allocation unit size in bytes. */
#define IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE      128
/** The chunk sub-allocation unit size as a shift factor. */
#define IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT     7

#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
/**
 * Per-chunk unwind info for non-windows hosts.
 */
typedef struct IEMEXECMEMCHUNKEHFRAME
{
# ifdef IEMNATIVE_USE_LIBUNWIND
    /** The offset of the FDA into abEhFrame. */
    uintptr_t               offFda;
# else
    /** struct object storage area. */
    uint8_t                 abObject[1024];
# endif
    /** The dwarf ehframe data for the chunk. */
    uint8_t                 abEhFrame[512];
} IEMEXECMEMCHUNKEHFRAME;
/** Pointer to per-chunk info info for non-windows hosts. */
typedef IEMEXECMEMCHUNKEHFRAME *PIEMEXECMEMCHUNKEHFRAME;
#endif


/**
 * An chunk of executable memory.
 */
typedef struct IEMEXECMEMCHUNK
{
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    /** Number of free items in this chunk. */
    uint32_t                cFreeUnits;
    /** Hint were to start searching for free space in the allocation bitmap. */
    uint32_t                idxFreeHint;
#else
    /** The heap handle. */
    RTHEAPSIMPLE            hHeap;
#endif
    /** Pointer to the chunk. */
    void                   *pvChunk;
#ifdef IN_RING3
    /**
     * Pointer to the unwind information.
     *
     * This is used during C++ throw and longjmp (windows and probably most other
     * platforms).  Some debuggers (windbg) makes use of it as well.
     *
     * Windows: This is allocated from hHeap on windows because (at least for
     *          AMD64) the UNWIND_INFO structure address in the
     *          RUNTIME_FUNCTION entry is an RVA and the chunk is the "image".
     *
     * Others:  Allocated from the regular heap to avoid unnecessary executable data
     *          structures.  This points to an IEMEXECMEMCHUNKEHFRAME structure. */
    void                   *pvUnwindInfo;
#elif defined(IN_RING0)
    /** Allocation handle. */
    RTR0MEMOBJ              hMemObj;
#endif
} IEMEXECMEMCHUNK;
/** Pointer to a memory chunk. */
typedef IEMEXECMEMCHUNK *PIEMEXECMEMCHUNK;


/**
 * Executable memory allocator for the native recompiler.
 */
typedef struct IEMEXECMEMALLOCATOR
{
    /** Magic value (IEMEXECMEMALLOCATOR_MAGIC).  */
    uint32_t                uMagic;

    /** The chunk size. */
    uint32_t                cbChunk;
    /** The maximum number of chunks. */
    uint32_t                cMaxChunks;
    /** The current number of chunks. */
    uint32_t                cChunks;
    /** Hint where to start looking for available memory. */
    uint32_t                idxChunkHint;
    /** Statistics: Current number of allocations. */
    uint32_t                cAllocations;

    /** The total amount of memory available. */
    uint64_t                cbTotal;
    /** Total amount of free memory. */
    uint64_t                cbFree;
    /** Total amount of memory allocated. */
    uint64_t                cbAllocated;

#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    /** Pointer to the allocation bitmaps for all the chunks (follows aChunks).
     *
     * Since the chunk size is a power of two and the minimum chunk size is a lot
     * higher than the IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE, each chunk will always
     * require a whole number of uint64_t elements in the allocation bitmap.  So,
     * for sake of simplicity, they are allocated as one continous chunk for
     * simplicity/laziness. */
    uint64_t               *pbmAlloc;
    /** Number of units (IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE) per chunk. */
    uint32_t                cUnitsPerChunk;
    /** Number of bitmap elements per chunk (for quickly locating the bitmap
     * portion corresponding to an chunk). */
    uint32_t                cBitmapElementsPerChunk;
#else
    /** @name Tweaks to get 64 byte aligned allocats w/o unnecessary fragmentation.
     * @{ */
    /** The size of the heap internal block header.   This is used to adjust the
     * request memory size to make sure there is exacly enough room for a header at
     * the end of the blocks we allocate before the next 64 byte alignment line. */
    uint32_t                cbHeapBlockHdr;
    /** The size of initial heap allocation required make sure the first
     *  allocation is correctly aligned. */
    uint32_t                cbHeapAlignTweak;
    /** The alignment tweak allocation address. */
    void                   *pvAlignTweak;
    /** @} */
#endif

#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
    /** Pointer to the array of unwind info running parallel to aChunks (same
     * allocation as this structure, located after the bitmaps).
     * (For Windows, the structures must reside in 32-bit RVA distance to the
     * actual chunk, so they are allocated off the chunk.) */
    PIEMEXECMEMCHUNKEHFRAME paEhFrames;
#endif

    /** The allocation chunks. */
    RT_FLEXIBLE_ARRAY_EXTENSION
    IEMEXECMEMCHUNK         aChunks[RT_FLEXIBLE_ARRAY];
} IEMEXECMEMALLOCATOR;
/** Pointer to an executable memory allocator. */
typedef IEMEXECMEMALLOCATOR *PIEMEXECMEMALLOCATOR;

/** Magic value for IEMEXECMEMALLOCATOR::uMagic (Scott Frederick Turow). */
#define IEMEXECMEMALLOCATOR_MAGIC UINT32_C(0x19490412)


static int iemExecMemAllocatorGrow(PIEMEXECMEMALLOCATOR pExecMemAllocator);


/**
 * Worker for iemExecMemAllocatorAlloc that returns @a pvRet after updating
 * the heap statistics.
 */
static void * iemExecMemAllocatorAllocTailCode(PIEMEXECMEMALLOCATOR pExecMemAllocator, void *pvRet,
                                               uint32_t cbReq, uint32_t idxChunk)
{
    pExecMemAllocator->cAllocations += 1;
    pExecMemAllocator->cbAllocated  += cbReq;
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    pExecMemAllocator->cbFree       -= cbReq;
#else
    pExecMemAllocator->cbFree       -= RT_ALIGN_32(cbReq, 64);
#endif
    pExecMemAllocator->idxChunkHint  = idxChunk;

#ifdef RT_OS_DARWIN
    /*
     * Sucks, but RTMEM_PROT_EXEC and RTMEM_PROT_WRITE are mutually exclusive
     * on darwin.  So, we mark the pages returned as read+write after alloc and
     * expect the caller to call iemExecMemAllocatorReadyForUse when done
     * writing to the allocation.
     *
     * See also https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon
     * for details.
     */
    /** @todo detect if this is necessary... it wasn't required on 10.15 or
     *        whatever older version it was. */
    int rc = RTMemProtect(pvRet, cbReq, RTMEM_PROT_WRITE | RTMEM_PROT_READ);
    AssertRC(rc);
#endif

    return pvRet;
}


#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
static void *iemExecMemAllocatorAllocInChunkInt(PIEMEXECMEMALLOCATOR pExecMemAllocator, uint64_t *pbmAlloc, uint32_t idxFirst,
                                                uint32_t cToScan, uint32_t cReqUnits, uint32_t idxChunk)
{
    /*
     * Shift the bitmap to the idxFirst bit so we can use ASMBitFirstClear.
     */
    Assert(!(cToScan & 63));
    Assert(!(idxFirst & 63));
    Assert(cToScan + idxFirst <= pExecMemAllocator->cUnitsPerChunk);
    pbmAlloc += idxFirst / 64;

    /*
     * Scan the bitmap for cReqUnits of consequtive clear bits
     */
    /** @todo This can probably be done more efficiently for non-x86 systems. */
    int iBit = ASMBitFirstClear(pbmAlloc, cToScan);
    while (iBit >= 0 && (uint32_t)iBit <= cToScan - cReqUnits)
    {
        uint32_t idxAddBit = 1;
        while (idxAddBit < cReqUnits && !ASMBitTest(pbmAlloc, (uint32_t)iBit + idxAddBit))
            idxAddBit++;
        if (idxAddBit >= cReqUnits)
        {
            ASMBitSetRange(pbmAlloc, (uint32_t)iBit, (uint32_t)iBit + cReqUnits);

            PIEMEXECMEMCHUNK const pChunk = &pExecMemAllocator->aChunks[idxChunk];
            pChunk->cFreeUnits -= cReqUnits;
            pChunk->idxFreeHint = (uint32_t)iBit + cReqUnits;

            void * const pvRet  = (uint8_t *)pChunk->pvChunk
                                + ((idxFirst + (uint32_t)iBit) << IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT);

            return iemExecMemAllocatorAllocTailCode(pExecMemAllocator, pvRet,
                                                    cReqUnits << IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT, idxChunk);
        }

        iBit = ASMBitNextClear(pbmAlloc, cToScan, iBit + idxAddBit - 1);
    }
    return NULL;
}
#endif /* IEMEXECMEM_USE_ALT_SUB_ALLOCATOR */


static void *iemExecMemAllocatorAllocInChunk(PIEMEXECMEMALLOCATOR pExecMemAllocator, uint32_t idxChunk, uint32_t cbReq)
{
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    /*
     * Figure out how much to allocate.
     */
    uint32_t const cReqUnits = (cbReq + IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE - 1) >> IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;
    if (cReqUnits <= pExecMemAllocator->aChunks[idxChunk].cFreeUnits)
    {
        uint64_t * const pbmAlloc = &pExecMemAllocator->pbmAlloc[pExecMemAllocator->cBitmapElementsPerChunk * idxChunk];
        uint32_t const   idxHint  = pExecMemAllocator->aChunks[idxChunk].idxFreeHint & ~(uint32_t)63;
        if (idxHint + cReqUnits <= pExecMemAllocator->cUnitsPerChunk)
        {
            void *pvRet = iemExecMemAllocatorAllocInChunkInt(pExecMemAllocator, pbmAlloc, idxHint,
                                                             pExecMemAllocator->cUnitsPerChunk - idxHint, cReqUnits, idxChunk);
            if (pvRet)
                return pvRet;
        }
        return iemExecMemAllocatorAllocInChunkInt(pExecMemAllocator, pbmAlloc, 0,
                                                  RT_MIN(pExecMemAllocator->cUnitsPerChunk, RT_ALIGN_32(idxHint + cReqUnits, 64)),
                                                  cReqUnits, idxChunk);
    }
#else
    void *pvRet = RTHeapSimpleAlloc(pExecMemAllocator->aChunks[idxChunk].hHeap, cbReq, 32);
    if (pvRet)
        return iemExecMemAllocatorAllocTailCode(pExecMemAllocator, pvRet, cbReq, idxChunk);
#endif
    return NULL;

}


/**
 * Allocates @a cbReq bytes of executable memory.
 *
 * @returns Pointer to the memory, NULL if out of memory or other problem
 *          encountered.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   cbReq   How many bytes are required.
 */
static void *iemExecMemAllocatorAlloc(PVMCPU pVCpu, uint32_t cbReq)
{
    PIEMEXECMEMALLOCATOR pExecMemAllocator = pVCpu->iem.s.pExecMemAllocatorR3;
    AssertReturn(pExecMemAllocator && pExecMemAllocator->uMagic == IEMEXECMEMALLOCATOR_MAGIC, NULL);
    AssertMsgReturn(cbReq > 32 && cbReq < _512K, ("%#x\n", cbReq), NULL);

    /*
     * Adjust the request size so it'll fit the allocator alignment/whatnot.
     *
     * For the RTHeapSimple allocator this means to follow the logic described
     * in iemExecMemAllocatorGrow and attempt to allocate it from one of the
     * existing chunks if we think we've got sufficient free memory around.
     *
     * While for the alternative one we just align it up to a whole unit size.
     */
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    cbReq = RT_ALIGN_32(cbReq, IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE);
#else
    cbReq = RT_ALIGN_32(cbReq + pExecMemAllocator->cbHeapBlockHdr, 64) - pExecMemAllocator->cbHeapBlockHdr;
#endif
    if (cbReq <= pExecMemAllocator->cbFree)
    {
        uint32_t const cChunks      = pExecMemAllocator->cChunks;
        uint32_t const idxChunkHint = pExecMemAllocator->idxChunkHint < cChunks ? pExecMemAllocator->idxChunkHint : 0;
        for (uint32_t idxChunk = idxChunkHint; idxChunk < cChunks; idxChunk++)
        {
            void *pvRet = iemExecMemAllocatorAllocInChunk(pExecMemAllocator, idxChunk, cbReq);
            if (pvRet)
                return pvRet;
        }
        for (uint32_t idxChunk = 0; idxChunk < idxChunkHint; idxChunk++)
        {
            void *pvRet = iemExecMemAllocatorAllocInChunk(pExecMemAllocator, idxChunk, cbReq);
            if (pvRet)
                return pvRet;
        }
    }

    /*
     * Can we grow it with another chunk?
     */
    if (pExecMemAllocator->cChunks < pExecMemAllocator->cMaxChunks)
    {
        int rc = iemExecMemAllocatorGrow(pExecMemAllocator);
        AssertLogRelRCReturn(rc, NULL);

        uint32_t const idxChunk = pExecMemAllocator->cChunks - 1;
        void *pvRet = iemExecMemAllocatorAllocInChunk(pExecMemAllocator, idxChunk, cbReq);
        if (pvRet)
            return pvRet;
        AssertFailed();
    }

    /* What now? Prune native translation blocks from the cache? */
    AssertFailed();
    return NULL;
}


/** This is a hook that we may need later for changing memory protection back
 *  to readonly+exec */
static void iemExecMemAllocatorReadyForUse(PVMCPUCC pVCpu, void *pv, size_t cb)
{
#ifdef RT_OS_DARWIN
    /* See iemExecMemAllocatorAllocTailCode for the explanation. */
    int rc = RTMemProtect(pv, cb, RTMEM_PROT_EXEC | RTMEM_PROT_READ);
    AssertRC(rc); RT_NOREF(pVCpu);

    /*
     * Flush the instruction cache:
     *      https://developer.apple.com/documentation/apple-silicon/porting-just-in-time-compilers-to-apple-silicon
     */
    /* sys_dcache_flush(pv, cb); - not necessary */
    sys_icache_invalidate(pv, cb);
#else
    RT_NOREF(pVCpu, pv, cb);
#endif
}


/**
 * Frees executable memory.
 */
void iemExecMemAllocatorFree(PVMCPU pVCpu, void *pv, size_t cb)
{
    PIEMEXECMEMALLOCATOR pExecMemAllocator = pVCpu->iem.s.pExecMemAllocatorR3;
    Assert(pExecMemAllocator && pExecMemAllocator->uMagic == IEMEXECMEMALLOCATOR_MAGIC);
    Assert(pv);
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    Assert(!((uintptr_t)pv & (IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE - 1)));
#else
    Assert(!((uintptr_t)pv & 63));
#endif

    /* Align the size as we did when allocating the block. */
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    cb = RT_ALIGN_Z(cb, IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE);
#else
    cb = RT_ALIGN_Z(cb + pExecMemAllocator->cbHeapBlockHdr, 64) - pExecMemAllocator->cbHeapBlockHdr;
#endif

    /* Free it / assert sanity. */
#if defined(VBOX_STRICT) || defined(IEMEXECMEM_USE_ALT_SUB_ALLOCATOR)
    uint32_t const cChunks = pExecMemAllocator->cChunks;
    uint32_t const cbChunk = pExecMemAllocator->cbChunk;
    bool           fFound  = false;
    for (uint32_t idxChunk = 0; idxChunk < cChunks; idxChunk++)
    {
        uintptr_t const offChunk = (uintptr_t)pv - (uintptr_t)pExecMemAllocator->aChunks[idxChunk].pvChunk;
        fFound = offChunk < cbChunk;
        if (fFound)
        {
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
            uint32_t const idxFirst  = offChunk >> IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;
            uint32_t const cReqUnits = cb       >> IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;

            /* Check that it's valid and free it. */
            uint64_t * const pbmAlloc = &pExecMemAllocator->pbmAlloc[pExecMemAllocator->cBitmapElementsPerChunk * idxChunk];
            AssertReturnVoid(ASMBitTest(pbmAlloc, idxFirst));
            for (uint32_t i = 1; i < cReqUnits; i++)
                AssertReturnVoid(ASMBitTest(pbmAlloc, idxFirst + i));
            ASMBitClearRange(pbmAlloc, idxFirst, idxFirst + cReqUnits);

            pExecMemAllocator->aChunks[idxChunk].cFreeUnits  += cReqUnits;
            pExecMemAllocator->aChunks[idxChunk].idxFreeHint  = idxFirst;

            /* Update the stats. */
            pExecMemAllocator->cbAllocated  -= cb;
            pExecMemAllocator->cbFree       += cb;
            pExecMemAllocator->cAllocations -= 1;
            return;
#else
            Assert(RTHeapSimpleSize(pExecMemAllocator->aChunks[idxChunk].hHeap, pv) == cb);
            break;
#endif
        }
    }
# ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    AssertFailed();
# else
    Assert(fFound);
# endif
#endif

#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    /* Update stats while cb is freshly calculated.*/
    pExecMemAllocator->cbAllocated  -= cb;
    pExecMemAllocator->cbFree       += RT_ALIGN_Z(cb, 64);
    pExecMemAllocator->cAllocations -= 1;

    /* Free it. */
    RTHeapSimpleFree(NIL_RTHEAPSIMPLE, pv);
#endif
}



#ifdef IN_RING3
# ifdef RT_OS_WINDOWS

/**
 * Initializes the unwind info structures for windows hosts.
 */
static int
iemExecMemAllocatorInitAndRegisterUnwindInfoForChunk(PIEMEXECMEMALLOCATOR pExecMemAllocator, void *pvChunk, uint32_t idxChunk)
{
    /*
     * The AMD64 unwind opcodes.
     *
     * This is a program that starts with RSP after a RET instruction that
     * ends up in recompiled code, and the operations we describe here will
     * restore all non-volatile registers and bring RSP back to where our
     * RET address is.  This means it's reverse order from what happens in
     * the prologue.
     *
     * Note! Using a frame register approach here both because we have one
     *       and but mainly because the UWOP_ALLOC_LARGE argument values
     *       would be a pain to write initializers for.  On the positive
     *       side, we're impervious to changes in the the stack variable
     *       area can can deal with dynamic stack allocations if necessary.
     */
    static const IMAGE_UNWIND_CODE s_aOpcodes[] =
    {
        { { 16, IMAGE_AMD64_UWOP_SET_FPREG,     0 } },              /* RSP  = RBP - FrameOffset * 10 (0x60) */
        { { 16, IMAGE_AMD64_UWOP_ALLOC_SMALL,   0 } },              /* RSP += 8; */
        { { 14, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_x15 } },   /* R15  = [RSP]; RSP += 8; */
        { { 12, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_x14 } },   /* R14  = [RSP]; RSP += 8; */
        { { 10, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_x13 } },   /* R13  = [RSP]; RSP += 8; */
        { {  8, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_x12 } },   /* R12  = [RSP]; RSP += 8; */
        { {  7, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_xDI } },   /* RDI  = [RSP]; RSP += 8; */
        { {  6, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_xSI } },   /* RSI  = [RSP]; RSP += 8; */
        { {  5, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_xBX } },   /* RBX  = [RSP]; RSP += 8; */
        { {  4, IMAGE_AMD64_UWOP_PUSH_NONVOL,   X86_GREG_xBP } },   /* RBP  = [RSP]; RSP += 8; */
    };
    union
    {
        IMAGE_UNWIND_INFO Info;
        uint8_t abPadding[RT_UOFFSETOF(IMAGE_UNWIND_INFO, aOpcodes) + 16];
    } s_UnwindInfo =
    {
        {
            /* .Version = */        1,
            /* .Flags = */          0,
            /* .SizeOfProlog = */   16, /* whatever */
            /* .CountOfCodes = */   RT_ELEMENTS(s_aOpcodes),
            /* .FrameRegister = */  X86_GREG_xBP,
            /* .FrameOffset = */    (-IEMNATIVE_FP_OFF_LAST_PUSH + 8) / 16 /* we're off by one slot. sigh. */,
        }
    };
    AssertCompile(-IEMNATIVE_FP_OFF_LAST_PUSH < 240 && -IEMNATIVE_FP_OFF_LAST_PUSH > 0);
    AssertCompile((-IEMNATIVE_FP_OFF_LAST_PUSH & 0xf) == 8);

    /*
     * Calc how much space we need and allocate it off the exec heap.
     */
    unsigned const cFunctionEntries = 1;
    unsigned const cbUnwindInfo     = sizeof(s_aOpcodes) + RT_UOFFSETOF(IMAGE_UNWIND_INFO, aOpcodes);
    unsigned const cbNeeded         = sizeof(IMAGE_RUNTIME_FUNCTION_ENTRY) * cFunctionEntries + cbUnwindInfo;
#  ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    unsigned const cbNeededAligned  = RT_ALIGN_32(cbNeeded, IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE);
    PIMAGE_RUNTIME_FUNCTION_ENTRY const paFunctions
        = (PIMAGE_RUNTIME_FUNCTION_ENTRY)iemExecMemAllocatorAllocInChunk(pExecMemAllocator, idxChunk, cbNeededAligned);
#  else
    unsigned const cbNeededAligned  = RT_ALIGN_32(cbNeeded + pExecMemAllocator->cbHeapBlockHdr, 64)
                                    - pExecMemAllocator->cbHeapBlockHdr;
    PIMAGE_RUNTIME_FUNCTION_ENTRY const paFunctions = (PIMAGE_RUNTIME_FUNCTION_ENTRY)RTHeapSimpleAlloc(hHeap, cbNeededAligned,
                                                                                                       32 /*cbAlignment*/);
#  endif
    AssertReturn(paFunctions, VERR_INTERNAL_ERROR_5);
    pExecMemAllocator->aChunks[idxChunk].pvUnwindInfo = paFunctions;

    /*
     * Initialize the structures.
     */
    PIMAGE_UNWIND_INFO const pInfo = (PIMAGE_UNWIND_INFO)&paFunctions[cFunctionEntries];

    paFunctions[0].BeginAddress         = 0;
    paFunctions[0].EndAddress           = pExecMemAllocator->cbChunk;
    paFunctions[0].UnwindInfoAddress    = (uint32_t)((uintptr_t)pInfo - (uintptr_t)pvChunk);

    memcpy(pInfo, &s_UnwindInfo, RT_UOFFSETOF(IMAGE_UNWIND_INFO, aOpcodes));
    memcpy(&pInfo->aOpcodes[0], s_aOpcodes, sizeof(s_aOpcodes));

    /*
     * Register it.
     */
    uint8_t fRet = RtlAddFunctionTable(paFunctions, cFunctionEntries, (uintptr_t)pvChunk);
    AssertReturn(fRet, NULL); /* Nothing to clean up on failure, since its within the chunk itself. */

    return paFunctions;
}


# else /* !RT_OS_WINDOWS */

/**
 * Emits a LEB128 encoded value between -0x2000 and 0x2000 (both exclusive).
 */
DECLINLINE(RTPTRUNION) iemDwarfPutLeb128(RTPTRUNION Ptr, int32_t iValue)
{
    if (iValue >= 64)
    {
        Assert(iValue < 0x2000);
        *Ptr.pb++ = ((uint8_t)iValue & 0x7f) | 0x80;
        *Ptr.pb++ = (uint8_t)(iValue >> 7) & 0x3f;
    }
    else if (iValue >= 0)
        *Ptr.pb++ = (uint8_t)iValue;
    else if (iValue > -64)
        *Ptr.pb++ = ((uint8_t)iValue & 0x3f) | 0x40;
    else
    {
        Assert(iValue > -0x2000);
        *Ptr.pb++ = ((uint8_t)iValue & 0x7f)        | 0x80;
        *Ptr.pb++ = ((uint8_t)(iValue >> 7) & 0x3f) | 0x40;
    }
    return Ptr;
}


/**
 * Emits an ULEB128 encoded value (up to 64-bit wide).
 */
DECLINLINE(RTPTRUNION) iemDwarfPutUleb128(RTPTRUNION Ptr, uint64_t uValue)
{
    while (uValue >= 0x80)
    {
        *Ptr.pb++ = ((uint8_t)uValue & 0x7f) | 0x80;
        uValue  >>= 7;
    }
    *Ptr.pb++ = (uint8_t)uValue;
    return Ptr;
}


/**
 * Emits a CFA rule as register @a uReg + offset @a off.
 */
DECLINLINE(RTPTRUNION) iemDwarfPutCfaDefCfa(RTPTRUNION Ptr, uint32_t uReg, uint32_t off)
{
    *Ptr.pb++ = DW_CFA_def_cfa;
    Ptr = iemDwarfPutUleb128(Ptr, uReg);
    Ptr = iemDwarfPutUleb128(Ptr, off);
    return Ptr;
}


/**
 * Emits a register (@a uReg) save location:
 *      CFA + @a off * data_alignment_factor
 */
DECLINLINE(RTPTRUNION) iemDwarfPutCfaOffset(RTPTRUNION Ptr, uint32_t uReg, uint32_t off)
{
    if (uReg < 0x40)
        *Ptr.pb++ = DW_CFA_offset | uReg;
    else
    {
        *Ptr.pb++ = DW_CFA_offset_extended;
        Ptr = iemDwarfPutUleb128(Ptr, uReg);
    }
    Ptr = iemDwarfPutUleb128(Ptr, off);
    return Ptr;
}


#  if 0 /* unused */
/**
 * Emits a register (@a uReg) save location, using signed offset:
 *      CFA + @a offSigned * data_alignment_factor
 */
DECLINLINE(RTPTRUNION) iemDwarfPutCfaSignedOffset(RTPTRUNION Ptr, uint32_t uReg, int32_t offSigned)
{
    *Ptr.pb++ = DW_CFA_offset_extended_sf;
    Ptr = iemDwarfPutUleb128(Ptr, uReg);
    Ptr = iemDwarfPutLeb128(Ptr, offSigned);
    return Ptr;
}
#  endif


/**
 * Initializes the unwind info section for non-windows hosts.
 */
static int
iemExecMemAllocatorInitAndRegisterUnwindInfoForChunk(PIEMEXECMEMALLOCATOR pExecMemAllocator, void *pvChunk, uint32_t idxChunk)
{
    PIEMEXECMEMCHUNKEHFRAME const pEhFrame = &pExecMemAllocator->paEhFrames[idxChunk];
    pExecMemAllocator->aChunks[idxChunk].pvUnwindInfo = pEhFrame; /* not necessary, but whatever */

    RTPTRUNION Ptr = { pEhFrame->abEhFrame };

    /*
     * Generate the CIE first.
     */
#  ifdef IEMNATIVE_USE_LIBUNWIND /* libunwind (llvm, darwin) only supports v1 and v3. */
    uint8_t const iDwarfVer = 3;
#  else
    uint8_t const iDwarfVer = 4;
#  endif
    RTPTRUNION const PtrCie = Ptr;
    *Ptr.pu32++ = 123;                                      /* The CIE length will be determined later. */
    *Ptr.pu32++ = 0 /*UINT32_MAX*/;                         /* I'm a CIE in .eh_frame speak. */
    *Ptr.pb++   = iDwarfVer;                                /* DwARF version */
    *Ptr.pb++   = 0;                                        /* Augmentation. */
    if (iDwarfVer >= 4)
    {
        *Ptr.pb++   = sizeof(uintptr_t);                    /* Address size. */
        *Ptr.pb++   = 0;                                    /* Segment selector size. */
    }
#  ifdef RT_ARCH_AMD64
    Ptr = iemDwarfPutLeb128(Ptr, 1);                        /* Code alignment factor (LEB128 = 1). */
#  else
    Ptr = iemDwarfPutLeb128(Ptr, 4);                        /* Code alignment factor (LEB128 = 4). */
#  endif
    Ptr = iemDwarfPutLeb128(Ptr, -8);                       /* Data alignment factor (LEB128 = -8). */
#  ifdef RT_ARCH_AMD64
    Ptr = iemDwarfPutUleb128(Ptr, DWREG_AMD64_RA);          /* Return address column (ULEB128) */
#  elif defined(RT_ARCH_ARM64)
    Ptr = iemDwarfPutUleb128(Ptr, DWREG_ARM64_LR);          /* Return address column (ULEB128) */
#  else
#   error "port me"
#  endif
    /* Initial instructions: */
#  ifdef RT_ARCH_AMD64
    Ptr = iemDwarfPutCfaDefCfa(Ptr, DWREG_AMD64_RBP, 16);   /* CFA     = RBP + 0x10 - first stack parameter */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_RA,  1);    /* Ret RIP = [CFA + 1*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_RBP, 2);    /* RBP     = [CFA + 2*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_RBX, 3);    /* RBX     = [CFA + 3*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R12, 4);    /* R12     = [CFA + 4*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R13, 5);    /* R13     = [CFA + 5*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R14, 6);    /* R14     = [CFA + 6*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R15, 7);    /* R15     = [CFA + 7*-8] */
#  elif defined(RT_ARCH_ARM64)
#   if 1
    Ptr = iemDwarfPutCfaDefCfa(Ptr, DWREG_ARM64_BP,  16);   /* CFA     = BP + 0x10 - first stack parameter */
#   else
    Ptr = iemDwarfPutCfaDefCfa(Ptr, DWREG_ARM64_SP,  IEMNATIVE_FRAME_VAR_SIZE + IEMNATIVE_FRAME_SAVE_REG_SIZE);
#   endif
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_LR,   1);   /* Ret PC  = [CFA + 1*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_BP,   2);   /* Ret BP  = [CFA + 2*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X28,  3);   /* X28     = [CFA + 3*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X27,  4);   /* X27     = [CFA + 4*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X26,  5);   /* X26     = [CFA + 5*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X25,  6);   /* X25     = [CFA + 6*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X24,  7);   /* X24     = [CFA + 7*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X23,  8);   /* X23     = [CFA + 8*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X22,  9);   /* X22     = [CFA + 9*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X21, 10);   /* X21     = [CFA +10*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X20, 11);   /* X20     = [CFA +11*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_ARM64_X19, 12);   /* X19     = [CFA +12*-8] */
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE / 8 == 12);
    /** @todo we we need to do something about clearing DWREG_ARM64_RA_SIGN_STATE or something? */
#  else
#   error "port me"
#  endif
    while ((Ptr.u - PtrCie.u) & 3)
        *Ptr.pb++ = DW_CFA_nop;
    /* Finalize the CIE size. */
    *PtrCie.pu32 = Ptr.u - PtrCie.u - sizeof(uint32_t);

    /*
     * Generate an FDE for the whole chunk area.
     */
#  ifdef IEMNATIVE_USE_LIBUNWIND
    pEhFrame->offFda = Ptr.u - (uintptr_t)&pEhFrame->abEhFrame[0];
#  endif
    RTPTRUNION const PtrFde = Ptr;
    *Ptr.pu32++ = 123;                                      /* The CIE length will be determined later. */
    *Ptr.pu32   = Ptr.u - PtrCie.u;                         /* Negated self relative CIE address. */
    Ptr.pu32++;
    *Ptr.pu64++ = (uintptr_t)pvChunk;                       /* Absolute start PC of this FDE. */
    *Ptr.pu64++ = pExecMemAllocator->cbChunk;               /* PC range length for this PDE. */
#  if 0 /* not requried for recent libunwind.dylib nor recent libgcc/glib. */
    *Ptr.pb++ = DW_CFA_nop;
#  endif
    while ((Ptr.u - PtrFde.u) & 3)
        *Ptr.pb++ = DW_CFA_nop;
    /* Finalize the FDE size. */
    *PtrFde.pu32 = Ptr.u - PtrFde.u - sizeof(uint32_t);

    /* Terminator entry. */
    *Ptr.pu32++ = 0;
    *Ptr.pu32++ = 0;            /* just to be sure... */
    Assert(Ptr.u - (uintptr_t)&pEhFrame->abEhFrame[0] <= sizeof(pEhFrame->abEhFrame));

    /*
     * Register it.
     */
#  ifdef IEMNATIVE_USE_LIBUNWIND
    __register_frame(&pEhFrame->abEhFrame[pEhFrame->offFda]);
#  else
    memset(pEhFrame->abObject, 0xf6, sizeof(pEhFrame->abObject)); /* color the memory to better spot usage */
    __register_frame_info(pEhFrame->abEhFrame, pEhFrame->abObject);
#  endif

    return VINF_SUCCESS;
}

# endif /* !RT_OS_WINDOWS */
#endif /* IN_RING3 */


/**
 * Adds another chunk to the executable memory allocator.
 *
 * This is used by the init code for the initial allocation and later by the
 * regular allocator function when it's out of memory.
 */
static int iemExecMemAllocatorGrow(PIEMEXECMEMALLOCATOR pExecMemAllocator)
{
    /* Check that we've room for growth. */
    uint32_t const idxChunk = pExecMemAllocator->cChunks;
    AssertLogRelReturn(idxChunk < pExecMemAllocator->cMaxChunks, VERR_OUT_OF_RESOURCES);

    /* Allocate a chunk. */
#ifdef RT_OS_DARWIN
    void *pvChunk = RTMemPageAllocEx(pExecMemAllocator->cbChunk, 0);
#else
    void *pvChunk = RTMemPageAllocEx(pExecMemAllocator->cbChunk, RTMEMPAGEALLOC_F_EXECUTABLE);
#endif
    AssertLogRelReturn(pvChunk, VERR_NO_EXEC_MEMORY);

#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    int rc = VINF_SUCCESS;
#else
    /* Initialize the heap for the chunk. */
    RTHEAPSIMPLE hHeap = NIL_RTHEAPSIMPLE;
    int rc = RTHeapSimpleInit(&hHeap, pvChunk, pExecMemAllocator->cbChunk);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        /*
         * We want the memory to be aligned on 64 byte, so the first time thru
         * here we do some exploratory allocations to see how we can achieve this.
         * On subsequent runs we only make an initial adjustment allocation, if
         * necessary.
         *
         * Since we own the heap implementation, we know that the internal block
         * header is 32 bytes in size for 64-bit systems (see RTHEAPSIMPLEBLOCK),
         * so all we need to wrt allocation size adjustments is to add 32 bytes
         * to the size, align up by 64 bytes, and subtract 32 bytes.
         *
         * The heap anchor block is 8 * sizeof(void *) (see RTHEAPSIMPLEINTERNAL),
         * which mean 64 bytes on a 64-bit system, so we need to make a 64 byte
         * allocation to force subsequent allocations to return 64 byte aligned
         * user areas.
         */
        if (!pExecMemAllocator->cbHeapBlockHdr)
        {
            pExecMemAllocator->cbHeapBlockHdr   = sizeof(void *) * 4; /* See RTHEAPSIMPLEBLOCK. */
            pExecMemAllocator->cbHeapAlignTweak = 64;
            pExecMemAllocator->pvAlignTweak     = RTHeapSimpleAlloc(hHeap, pExecMemAllocator->cbHeapAlignTweak,
                                                                    32 /*cbAlignment*/);
            AssertStmt(pExecMemAllocator->pvAlignTweak, rc = VERR_INTERNAL_ERROR_2);

            void *pvTest1 = RTHeapSimpleAlloc(hHeap,
                                                RT_ALIGN_32(256 + pExecMemAllocator->cbHeapBlockHdr, 64)
                                              - pExecMemAllocator->cbHeapBlockHdr, 32 /*cbAlignment*/);
            AssertStmt(pvTest1, rc = VERR_INTERNAL_ERROR_2);
            AssertStmt(!((uintptr_t)pvTest1 & 63), rc = VERR_INTERNAL_ERROR_3);

            void *pvTest2 = RTHeapSimpleAlloc(hHeap,
                                                RT_ALIGN_32(687 + pExecMemAllocator->cbHeapBlockHdr, 64)
                                              - pExecMemAllocator->cbHeapBlockHdr, 32 /*cbAlignment*/);
            AssertStmt(pvTest2, rc = VERR_INTERNAL_ERROR_2);
            AssertStmt(!((uintptr_t)pvTest2 & 63), rc = VERR_INTERNAL_ERROR_3);

            RTHeapSimpleFree(hHeap, pvTest2);
            RTHeapSimpleFree(hHeap, pvTest1);
        }
        else
        {
            pExecMemAllocator->pvAlignTweak = RTHeapSimpleAlloc(hHeap,  pExecMemAllocator->cbHeapAlignTweak, 32 /*cbAlignment*/);
            AssertStmt(pExecMemAllocator->pvAlignTweak, rc = VERR_INTERNAL_ERROR_4);
        }
        if (RT_SUCCESS(rc))
#endif /* !IEMEXECMEM_USE_ALT_SUB_ALLOCATOR */
        {
            /*
             * Add the chunk.
             *
             * This must be done before the unwind init so windows can allocate
             * memory from the chunk when using the alternative sub-allocator.
             */
            pExecMemAllocator->aChunks[idxChunk].pvChunk      = pvChunk;
#ifdef IN_RING3
            pExecMemAllocator->aChunks[idxChunk].pvUnwindInfo = NULL;
#endif
#ifndef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
            pExecMemAllocator->aChunks[idxChunk].hHeap        = hHeap;
#else
            pExecMemAllocator->aChunks[idxChunk].cFreeUnits   = pExecMemAllocator->cUnitsPerChunk;
            pExecMemAllocator->aChunks[idxChunk].idxFreeHint  = 0;
            memset(&pExecMemAllocator->pbmAlloc[pExecMemAllocator->cBitmapElementsPerChunk * idxChunk],
                   0, sizeof(pExecMemAllocator->pbmAlloc[0]) * pExecMemAllocator->cBitmapElementsPerChunk);
#endif

            pExecMemAllocator->cChunks      = idxChunk + 1;
            pExecMemAllocator->idxChunkHint = idxChunk;

#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
            pExecMemAllocator->cbTotal     += pExecMemAllocator->cbChunk;
            pExecMemAllocator->cbFree      += pExecMemAllocator->cbChunk;
#else
            size_t const cbFree = RTHeapSimpleGetFreeSize(hHeap);
            pExecMemAllocator->cbTotal     += cbFree;
            pExecMemAllocator->cbFree      += cbFree;
#endif

#ifdef IN_RING3
            /*
             * Initialize the unwind information (this cannot really fail atm).
             * (This sets pvUnwindInfo.)
             */
            rc = iemExecMemAllocatorInitAndRegisterUnwindInfoForChunk(pExecMemAllocator, pvChunk, idxChunk);
            if (RT_SUCCESS(rc))
#endif
            {
                return VINF_SUCCESS;
            }

#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
            /* Just in case the impossible happens, undo the above up: */
            pExecMemAllocator->cbTotal -= pExecMemAllocator->cbChunk;
            pExecMemAllocator->cbFree  -= pExecMemAllocator->aChunks[idxChunk].cFreeUnits << IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;
            pExecMemAllocator->cChunks  = idxChunk;
            memset(&pExecMemAllocator->pbmAlloc[pExecMemAllocator->cBitmapElementsPerChunk * idxChunk],
                   0xff, sizeof(pExecMemAllocator->pbmAlloc[0]) * pExecMemAllocator->cBitmapElementsPerChunk);
            pExecMemAllocator->aChunks[idxChunk].pvChunk    = NULL;
            pExecMemAllocator->aChunks[idxChunk].cFreeUnits = 0;
#endif
        }
#ifndef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    }
#endif
    RTMemPageFree(pvChunk, pExecMemAllocator->cbChunk);
    return rc;
}


/**
 * Initializes the executable memory allocator for native recompilation on the
 * calling EMT.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure of the calling
 *                      thread.
 * @param   cbMax       The max size of the allocator.
 * @param   cbInitial   The initial allocator size.
 * @param   cbChunk     The chunk size, 0 or UINT32_MAX for default (@a cbMax
 *                      dependent).
 */
int iemExecMemAllocatorInit(PVMCPU pVCpu, uint64_t cbMax, uint64_t cbInitial, uint32_t cbChunk)
{
    /*
     * Validate input.
     */
    AssertLogRelMsgReturn(cbMax >= _1M && cbMax <= _4G+_4G, ("cbMax=%RU64 (%RX64)\n", cbMax, cbMax), VERR_OUT_OF_RANGE);
    AssertReturn(cbInitial <= cbMax, VERR_OUT_OF_RANGE);
    AssertLogRelMsgReturn(   cbChunk != UINT32_MAX
                          || cbChunk == 0
                          || (   RT_IS_POWER_OF_TWO(cbChunk)
                              && cbChunk >= _1M
                              && cbChunk <= _256M
                              && cbChunk <= cbMax),
                          ("cbChunk=%RU32 (%RX32) cbMax=%RU64\n", cbChunk, cbChunk, cbMax),
                          VERR_OUT_OF_RANGE);

    /*
     * Adjust/figure out the chunk size.
     */
    if (cbChunk == 0 || cbChunk == UINT32_MAX)
    {
        if (cbMax >= _256M)
            cbChunk = _64M;
        else
        {
            if (cbMax < _16M)
                cbChunk = cbMax >= _4M ? _4M : (uint32_t)cbMax;
            else
                cbChunk = (uint32_t)cbMax / 4;
            if (!RT_IS_POWER_OF_TWO(cbChunk))
                cbChunk = RT_BIT_32(ASMBitLastSetU32(cbChunk));
        }
    }

    if (cbChunk > cbMax)
        cbMax = cbChunk;
    else
        cbMax = (cbMax - 1 + cbChunk) / cbChunk * cbChunk;
    uint32_t const cMaxChunks = (uint32_t)(cbMax / cbChunk);
    AssertLogRelReturn((uint64_t)cMaxChunks * cbChunk == cbMax, VERR_INTERNAL_ERROR_3);

    /*
     * Allocate and initialize the allocatore instance.
     */
    size_t       cbNeeded   = RT_UOFFSETOF_DYN(IEMEXECMEMALLOCATOR, aChunks[cMaxChunks]);
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    size_t const offBitmaps = RT_ALIGN_Z(cbNeeded, RT_CACHELINE_SIZE);
    size_t const cbBitmap   = cbChunk >> (IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT + 3);
    cbNeeded += cbBitmap * cMaxChunks;
    AssertCompile(IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT <= 10);
    Assert(cbChunk > RT_BIT_32(IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT + 3));
#endif
#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
    size_t const offEhFrames = RT_ALIGN_Z(cbNeeded, RT_CACHELINE_SIZE);
    cbNeeded += sizeof(IEMEXECMEMCHUNKEHFRAME) * cMaxChunks;
#endif
    PIEMEXECMEMALLOCATOR pExecMemAllocator = (PIEMEXECMEMALLOCATOR)RTMemAllocZ(cbNeeded);
    AssertLogRelMsgReturn(pExecMemAllocator, ("cbNeeded=%zx cMaxChunks=%#x cbChunk=%#x\n", cbNeeded, cMaxChunks, cbChunk),
                          VERR_NO_MEMORY);
    pExecMemAllocator->uMagic       = IEMEXECMEMALLOCATOR_MAGIC;
    pExecMemAllocator->cbChunk      = cbChunk;
    pExecMemAllocator->cMaxChunks   = cMaxChunks;
    pExecMemAllocator->cChunks      = 0;
    pExecMemAllocator->idxChunkHint = 0;
    pExecMemAllocator->cAllocations = 0;
    pExecMemAllocator->cbTotal      = 0;
    pExecMemAllocator->cbFree       = 0;
    pExecMemAllocator->cbAllocated  = 0;
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    pExecMemAllocator->pbmAlloc                 = (uint64_t *)((uintptr_t)pExecMemAllocator + offBitmaps);
    pExecMemAllocator->cUnitsPerChunk           = cbChunk >> IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;
    pExecMemAllocator->cBitmapElementsPerChunk  = cbChunk >> (IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT + 6);
    memset(pExecMemAllocator->pbmAlloc, 0xff, cbBitmap); /* Mark everything as allocated. Clear when chunks are added. */
#endif
#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
    pExecMemAllocator->paEhFrames = (PIEMEXECMEMCHUNKEHFRAME)((uintptr_t)pExecMemAllocator + offEhFrames);
#endif
    for (uint32_t i = 0; i < cMaxChunks; i++)
    {
#ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
        pExecMemAllocator->aChunks[i].cFreeUnits   = 0;
        pExecMemAllocator->aChunks[i].idxFreeHint  = 0;
#else
        pExecMemAllocator->aChunks[i].hHeap        = NIL_RTHEAPSIMPLE;
#endif
        pExecMemAllocator->aChunks[i].pvChunk      = NULL;
#ifdef IN_RING0
        pExecMemAllocator->aChunks[i].hMemObj      = NIL_RTR0MEMOBJ;
#else
        pExecMemAllocator->aChunks[i].pvUnwindInfo = NULL;
#endif
    }
    pVCpu->iem.s.pExecMemAllocatorR3 = pExecMemAllocator;

    /*
     * Do the initial allocations.
     */
    while (cbInitial < (uint64_t)pExecMemAllocator->cChunks * pExecMemAllocator->cbChunk)
    {
        int rc = iemExecMemAllocatorGrow(pExecMemAllocator);
        AssertLogRelRCReturn(rc, rc);
    }

    pExecMemAllocator->idxChunkHint = 0;

    return VINF_SUCCESS;
}


/*********************************************************************************************************************************
*   Native Recompilation                                                                                                         *
*********************************************************************************************************************************/


/**
 * Used by TB code when encountering a non-zero status or rcPassUp after a call.
 */
IEM_DECL_IMPL_DEF(int, iemNativeHlpExecStatusCodeFiddling,(PVMCPUCC pVCpu, int rc, uint8_t idxInstr))
{
    pVCpu->iem.s.cInstructions += idxInstr;
    return VBOXSTRICTRC_VAL(iemExecStatusCodeFiddling(pVCpu, rc == VINF_IEM_REEXEC_BREAK ? VINF_SUCCESS : rc));
}


/**
 * Reinitializes the native recompiler state.
 *
 * Called before starting a new recompile job.
 */
static PIEMRECOMPILERSTATE iemNativeReInit(PIEMRECOMPILERSTATE pReNative, PCIEMTB pTb)
{
    pReNative->cLabels   = 0;
    pReNative->cFixups   = 0;
    pReNative->pTbOrg    = pTb;
    return pReNative;
}


/**
 * Allocates and initializes the native recompiler state.
 *
 * This is called the first time an EMT wants to recompile something.
 *
 * @returns Pointer to the new recompiler state.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The TB that's about to be recompiled.
 * @thread  EMT(pVCpu)
 */
static PIEMRECOMPILERSTATE iemNativeInit(PVMCPUCC pVCpu, PCIEMTB pTb)
{
    VMCPU_ASSERT_EMT(pVCpu);

    PIEMRECOMPILERSTATE pReNative = (PIEMRECOMPILERSTATE)RTMemAllocZ(sizeof(*pReNative));
    AssertReturn(pReNative, NULL);

    /*
     * Try allocate all the buffers and stuff we need.
     */
    pReNative->pInstrBuf = (PIEMNATIVEINSTR)RTMemAllocZ(_64K);
    pReNative->paLabels  = (PIEMNATIVELABEL)RTMemAllocZ(sizeof(IEMNATIVELABEL) * _8K);
    pReNative->paFixups  = (PIEMNATIVEFIXUP)RTMemAllocZ(sizeof(IEMNATIVEFIXUP) * _16K);
    if (RT_LIKELY(   pReNative->pInstrBuf
                  && pReNative->paLabels
                  && pReNative->paFixups))
    {
        /*
         * Set the buffer & array sizes on success.
         */
        pReNative->cInstrBufAlloc = _64K / sizeof(IEMNATIVEINSTR);
        pReNative->cLabelsAlloc   = _8K;
        pReNative->cFixupsAlloc   = _16K;

        /*
         * Done, just need to save it and reinit it.
         */
        pVCpu->iem.s.pNativeRecompilerStateR3 = pReNative;
        return iemNativeReInit(pReNative, pTb);
    }

    /*
     * Failed. Cleanup and return.
     */
    AssertFailed();
    RTMemFree(pReNative->pInstrBuf);
    RTMemFree(pReNative->paLabels);
    RTMemFree(pReNative->paFixups);
    RTMemFree(pReNative);
    return NULL;
}


/**
 * Defines a label.
 *
 * @returns Label ID.
 * @param   pReNative   The native recompile state.
 * @param   enmType     The label type.
 * @param   offWhere    The instruction offset of the label.  UINT32_MAX if the
 *                      label is not yet defined (default).
 * @param   uData       Data associated with the lable. Only applicable to
 *                      certain type of labels. Default is zero.
 */
DECLHIDDEN(uint32_t) iemNativeMakeLabel(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                                        uint32_t offWhere /*= UINT32_MAX*/, uint16_t uData /*= 0*/) RT_NOEXCEPT
{
    /*
     * Do we have the label already?
     */
    PIEMNATIVELABEL paLabels = pReNative->paLabels;
    uint32_t const  cLabels  = pReNative->cLabels;
    for (uint32_t i = 0; i < cLabels; i++)
        if (   paLabels[i].enmType == enmType
            && paLabels[i].uData   == uData)
        {
            if (paLabels[i].off == offWhere || offWhere == UINT32_MAX)
                return i;
            if (paLabels[i].off == UINT32_MAX)
            {
                paLabels[i].off = offWhere;
                return i;
            }
        }

    /*
     * Make sure we've got room for another label.
     */
    if (RT_LIKELY(cLabels < pReNative->cLabelsAlloc))
    { /* likely */ }
    else
    {
        uint32_t cNew = pReNative->cLabelsAlloc;
        AssertReturn(cNew, UINT32_MAX);
        AssertReturn(cLabels == cNew, UINT32_MAX);
        cNew *= 2;
        AssertReturn(cNew <= _64K, UINT32_MAX); /* IEMNATIVEFIXUP::idxLabel type restrict this */
        paLabels = (PIEMNATIVELABEL)RTMemRealloc(paLabels, cNew * sizeof(paLabels[0]));
        AssertReturn(paLabels, UINT32_MAX);
        pReNative->paLabels     = paLabels;
        pReNative->cLabelsAlloc = cNew;
    }

    /*
     * Define a new label.
     */
    paLabels[cLabels].off     = offWhere;
    paLabels[cLabels].enmType = enmType;
    paLabels[cLabels].uData   = uData;
    pReNative->cLabels = cLabels + 1;
    return cLabels;
}


/**
 * Looks up a lable.
 *
 * @returns Label ID if found, UINT32_MAX if not.
 */
static uint32_t iemNativeFindLabel(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                                   uint32_t offWhere = UINT32_MAX, uint16_t uData = 0) RT_NOEXCEPT
{
    PIEMNATIVELABEL paLabels = pReNative->paLabels;
    uint32_t const  cLabels  = pReNative->cLabels;
    for (uint32_t i = 0; i < cLabels; i++)
        if (   paLabels[i].enmType == enmType
            && paLabels[i].uData   == uData
            && (   paLabels[i].off == offWhere
                || offWhere        == UINT32_MAX
                || paLabels[i].off == UINT32_MAX))
            return i;
    return UINT32_MAX;
}



/**
 * Adds a fixup.
 *
 * @returns Success indicator.
 * @param   pReNative   The native recompile state.
 * @param   offWhere    The instruction offset of the fixup location.
 * @param   idxLabel    The target label ID for the fixup.
 * @param   enmType     The fixup type.
 * @param   offAddend   Fixup addend if applicable to the type. Default is 0.
 */
DECLHIDDEN(bool) iemNativeAddFixup(PIEMRECOMPILERSTATE pReNative, uint32_t offWhere, uint32_t idxLabel,
                                   IEMNATIVEFIXUPTYPE enmType, int8_t offAddend /*= 0*/) RT_NOEXCEPT
{
    Assert(idxLabel <= UINT16_MAX);
    Assert((unsigned)enmType <= UINT8_MAX);

    /*
     * Make sure we've room.
     */
    PIEMNATIVEFIXUP paFixups = pReNative->paFixups;
    uint32_t const  cFixups  = pReNative->cFixups;
    if (RT_LIKELY(cFixups < pReNative->cFixupsAlloc))
    { /* likely */ }
    else
    {
        uint32_t cNew = pReNative->cFixupsAlloc;
        AssertReturn(cNew, false);
        AssertReturn(cFixups == cNew, false);
        cNew *= 2;
        AssertReturn(cNew <= _128K, false);
        paFixups = (PIEMNATIVEFIXUP)RTMemRealloc(paFixups, cNew * sizeof(paFixups[0]));
        AssertReturn(paFixups, false);
        pReNative->paFixups     = paFixups;
        pReNative->cFixupsAlloc = cNew;
    }

    /*
     * Add the fixup.
     */
    paFixups[cFixups].off       = offWhere;
    paFixups[cFixups].idxLabel  = (uint16_t)idxLabel;
    paFixups[cFixups].enmType   = enmType;
    paFixups[cFixups].offAddend = offAddend;
    pReNative->cFixups = cFixups + 1;
    return true;
}

/**
 * Slow code path for iemNativeInstrBufEnsure.
 */
DECLHIDDEN(PIEMNATIVEINSTR) iemNativeInstrBufEnsureSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                        uint32_t cInstrReq) RT_NOEXCEPT
{
    /* Double the buffer size till we meet the request. */
    uint32_t cNew = pReNative->cInstrBufAlloc;
    AssertReturn(cNew > 0, NULL);
    do
        cNew *= 2;
    while (cNew < off + cInstrReq);

    uint32_t const cbNew = cNew * sizeof(IEMNATIVEINSTR);
    AssertReturn(cbNew <= _2M, NULL);

    void *pvNew = RTMemRealloc(pReNative->pInstrBuf, cbNew);
    AssertReturn(pvNew, NULL);

    pReNative->cInstrBufAlloc   = cNew;
    return pReNative->pInstrBuf = (PIEMNATIVEINSTR)pvNew;
}


/**
 * Emits a code for checking the return code of a call and rcPassUp, returning
 * from the code if either are non-zero.
 */
DECLHIDDEN(uint32_t) iemNativeEmitCheckCallRetAndPassUp(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                        uint8_t idxInstr) RT_NOEXCEPT
{
#ifdef RT_ARCH_AMD64
    /*
     * AMD64: eax = call status code.
     */

    /* edx = rcPassUp */
    off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, X86_GREG_xDX, RT_UOFFSETOF(VMCPUCC, iem.s.rcPassUp));
    AssertReturn(off != UINT32_MAX, UINT32_MAX);

    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /* edx = eax | rcPassUp*/
    pbCodeBuf[off++] = 0x0b;                    /* or edx, eax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xDX, X86_GREG_xAX);

    /* Jump to non-zero status return path, loading cl with the instruction number. */
    pbCodeBuf[off++] = 0xb0 + X86_GREG_xCX;     /* mov cl, imm8 (pCallEntry->idxInstr) */
    pbCodeBuf[off++] = idxInstr;

    pbCodeBuf[off++] = 0x0f;                    /* jnz rel32 */
    pbCodeBuf[off++] = 0x85;
    uint32_t const idxLabel = iemNativeMakeLabel(pReNative, kIemNativeLabelType_NonZeroRetOrPassUp);
    AssertReturn(idxLabel != UINT32_MAX, UINT32_MAX);
    AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_Rel32, -4), UINT32_MAX);
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;

    /* done. */

#elif RT_ARCH_ARM64
    /*
     * ARM64: w0 = call status code.
     */
    off = iemNativeEmitLoadGprImm64(pReNative, off, ARMV8_A64_REG_X2, idxInstr); /** @todo 32-bit imm load? Fixed counter register? */
    off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, ARMV8_A64_REG_X3, RT_UOFFSETOF(VMCPUCC, iem.s.rcPassUp));

    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pu32CodeBuf, UINT32_MAX);

    pu32CodeBuf[off++] = Armv8A64MkInstrOrr(ARMV8_A64_REG_X4, ARMV8_A64_REG_X3, ARMV8_A64_REG_X0, false /*f64Bit*/);

    uint32_t const idxLabel = iemNativeMakeLabel(pReNative, kIemNativeLabelType_NonZeroRetOrPassUp);
    AssertReturn(idxLabel != UINT32_MAX, UINT32_MAX);
    AssertReturn(iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5), UINT32_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(true /*fJmpIfNotZero*/, ARMV8_A64_REG_X4, false /*f64Bit*/);

#else
# error "port me"
#endif
    return off;
}


/**
 * Emits a call to a CImpl function or something similar.
 */
static int32_t iemNativeEmitCImplCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                      uintptr_t pfnCImpl, uint8_t cbInstr, uint8_t cAddParams,
                                      uint64_t uParam0, uint64_t uParam1, uint64_t uParam2)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
#endif

    /*
     * Load the parameters.
     */
#if defined(RT_OS_WINDOWS) && defined(VBOXSTRICTRC_STRICT_ENABLED)
    /* Special code the hidden VBOXSTRICTRC pointer. */
    off = iemNativeEmitLoadGprFromGpr(  pReNative, off, IEMNATIVE_CALL_ARG1_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
    off = iemNativeEmitLoadGprImm64(    pReNative, off, IEMNATIVE_CALL_ARG2_GREG, cbInstr); /** @todo 8-bit reg load opt for amd64 */
    if (cAddParams > 0)
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG3_GREG, uParam0);
    if (cAddParams > 1)
        off = iemNativeEmitStoreImm64ByBp(pReNative, off, IEMNATIVE_FP_OFF_STACK_ARG0, uParam1);
    if (cAddParams > 2)
        off = iemNativeEmitStoreImm64ByBp(pReNative, off, IEMNATIVE_FP_OFF_STACK_ARG1, uParam2);
    off = iemNativeEmitLeaGrpByBp(pReNative, off, X86_GREG_xCX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict */

#else
    AssertCompile(IEMNATIVE_CALL_ARG_GREG_COUNT >= 4);
    off = iemNativeEmitLoadGprFromGpr(  pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
    off = iemNativeEmitLoadGprImm64(    pReNative, off, IEMNATIVE_CALL_ARG1_GREG, cbInstr); /** @todo 8-bit reg load opt for amd64 */
    if (cAddParams > 0)
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, uParam0);
    if (cAddParams > 1)
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG3_GREG, uParam1);
    if (cAddParams > 2)
# if IEMNATIVE_CALL_ARG_GREG_COUNT >= 5
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG4_GREG, uParam2);
# else
        off = iemNativeEmitStoreImm64ByBp(pReNative, off, IEMNATIVE_FP_OFF_STACK_ARG0, uParam2);
# endif
#endif
    AssertReturn(off != UINT32_MAX, off);

    /*
     * Make the call.
     */
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xAX, pfnCImpl);

    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0xff;                    /* call rax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

# if defined(VBOXSTRICTRC_STRICT_ENABLED) && defined(RT_OS_WINDOWS)
    off = iemNativeEmitLoadGprByBpU32(pReNative, off, X86_GREG_xAX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict (see above) */
# endif

#elif defined(RT_ARCH_ARM64)
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, pfnCImpl);

    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    pu32CodeBuf[off++] = Armv8A64MkInstrBlr(IEMNATIVE_REG_FIXED_TMP0);

#else
# error "Port me!"
#endif

    /*
     * Check the status code.
     */
    return iemNativeEmitCheckCallRetAndPassUp(pReNative, off, idxInstr);
}


/**
 * Emits a call to a threaded worker function.
 */
static int32_t iemNativeEmitThreadedCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pReNative, off);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
#endif
    uint8_t const cParams = g_acIemThreadedFunctionUsedArgs[pCallEntry->enmFunction];

#ifdef RT_ARCH_AMD64
    /* Load the parameters and emit the call. */
# ifdef RT_OS_WINDOWS
#  ifndef VBOXSTRICTRC_STRICT_ENABLED
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xCX, IEMNATIVE_REG_FIXED_PVMCPU);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    if (cParams > 0)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xDX, pCallEntry->auParams[0]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    if (cParams > 1)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x8, pCallEntry->auParams[1]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    if (cParams > 2)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x9, pCallEntry->auParams[2]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
#  else  /* VBOXSTRICTRC: Returned via hidden parameter. Sigh. */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDX, IEMNATIVE_REG_FIXED_PVMCPU);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    if (cParams > 0)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x8, pCallEntry->auParams[0]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    if (cParams > 1)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x9, pCallEntry->auParams[1]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    if (cParams > 2)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x10, pCallEntry->auParams[2]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    off = iemNativeEmitStoreGprByBp(pReNative, off, IEMNATIVE_FP_OFF_STACK_ARG0, X86_GREG_x10);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLeaGrpByBp(pReNative, off, X86_GREG_xCX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict */
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
#  endif /* VBOXSTRICTRC_STRICT_ENABLED */
# else
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDI, IEMNATIVE_REG_FIXED_PVMCPU);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    if (cParams > 0)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xSI, pCallEntry->auParams[0]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    if (cParams > 1)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xDX, pCallEntry->auParams[1]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
    if (cParams > 2)
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xCX, pCallEntry->auParams[2]);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
    }
# endif
    off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xAX, (uintptr_t)g_apfnIemThreadedFunctions[pCallEntry->enmFunction]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);

    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0xff;                    /* call rax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

# if defined(VBOXSTRICTRC_STRICT_ENABLED) && defined(RT_OS_WINDOWS)
    off = iemNativeEmitLoadGprByBpU32(pReNative, off, X86_GREG_xAX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict (see above) */
# endif

#elif RT_ARCH_ARM64
    /*
     * ARM64:
     */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
    if (cParams > 0)
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, pCallEntry->auParams[0]);
    if (cParams > 1)
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, pCallEntry->auParams[1]);
    if (cParams > 2)
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_ARG3_GREG, pCallEntry->auParams[2]);
    off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0,
                                    (uintptr_t)g_apfnIemThreadedFunctions[pCallEntry->enmFunction]);

    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    AssertReturn(pu32CodeBuf, UINT32_MAX);

    pu32CodeBuf[off++] = Armv8A64MkInstrBlr(IEMNATIVE_REG_FIXED_TMP0);

#else
# error "port me"
#endif

    /*
     * Check the status code.
     */
    off = iemNativeEmitCheckCallRetAndPassUp(pReNative, off, pCallEntry->idxInstr);
    AssertReturn(off != UINT32_MAX, off);

    return off;
}


/**
 * Emits the RC fiddling code for handling non-zero return code or rcPassUp.
 */
static uint32_t iemNativeEmitRcFiddling(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    /*
     * Generate the rc + rcPassUp fiddling code if needed.
     */
    uint32_t idxLabel = iemNativeFindLabel(pReNative, kIemNativeLabelType_NonZeroRetOrPassUp);
    if (idxLabel != UINT32_MAX)
    {
        Assert(pReNative->paLabels[idxLabel].off == UINT32_MAX);
        pReNative->paLabels[idxLabel].off = off;

        /* iemNativeHlpExecStatusCodeFiddling(PVMCPUCC pVCpu, int rc, uint8_t idxInstr) */
#ifdef RT_ARCH_AMD64
        /*
         * AMD64:
         */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 20);
        AssertReturn(pbCodeBuf, UINT32_MAX);

        /* Call helper and jump to return point. */
# ifdef RT_OS_WINDOWS
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_x8,  X86_GREG_xCX); /* cl = instruction number */
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xCX, IEMNATIVE_REG_FIXED_PVMCPU);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDX, X86_GREG_xAX);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
# else
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDI, IEMNATIVE_REG_FIXED_PVMCPU);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xSI, X86_GREG_xAX);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDX, X86_GREG_xCX); /* cl = instruction number */
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
# endif
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xAX, (uintptr_t)iemNativeHlpExecStatusCodeFiddling);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);

        pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        pbCodeBuf[off++] = 0xff;                    /* call rax */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

        /* Jump to common return point. */
        uint32_t offRel = pReNative->paLabels[idxReturnLabel].off - (off + 2);
        if (-(int32_t)offRel <= 127)
        {
            pbCodeBuf[off++] = 0xeb;                /* jmp rel8 */
            pbCodeBuf[off++] = (uint8_t)offRel;
            off++;
        }
        else
        {
            offRel -= 3;
            pbCodeBuf[off++] = 0xe9;                /* jmp rel32 */
            pbCodeBuf[off++] = RT_BYTE1(offRel);
            pbCodeBuf[off++] = RT_BYTE2(offRel);
            pbCodeBuf[off++] = RT_BYTE3(offRel);
            pbCodeBuf[off++] = RT_BYTE4(offRel);
        }
        pbCodeBuf[off++] = 0xcc;                    /*  int3 poison */

#elif RT_ARCH_ARM64
        /*
         * ARM64:
         */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, IEMNATIVE_CALL_RET_GREG);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        /* IEMNATIVE_CALL_ARG2_GREG is already set. */
        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, (uintptr_t)iemNativeHlpExecStatusCodeFiddling);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);

        uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
        AssertReturn(pu32CodeBuf, UINT32_MAX);
        pu32CodeBuf[off++] = Armv8A64MkInstrBlr(IEMNATIVE_REG_FIXED_TMP0);

        /* Jump back to the common return point. */
        int32_t const offRel = pReNative->paLabels[idxReturnLabel].off - off;
        pu32CodeBuf[off++] = Armv8A64MkInstrB(offRel);
#else
# error "port me"
#endif
    }
    return off;
}


/**
 * Emits a standard epilog.
 */
static uint32_t iemNativeEmitEpilog(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    /*
     * Successful return, so clear the return register (eax, w0).
     */
    off = iemNativeEmitGprZero(pReNative,off, IEMNATIVE_CALL_RET_GREG);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);

    /*
     * Define label for common return point.
     */
    uint32_t const idxReturn = iemNativeMakeLabel(pReNative, kIemNativeLabelType_Return, off);
    AssertReturn(idxReturn != UINT32_MAX, UINT32_MAX);

    /*
     * Restore registers and return.
     */
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 20);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /* Reposition esp at the r15 restore point. */
    pbCodeBuf[off++] = X86_OP_REX_W;
    pbCodeBuf[off++] = 0x8d;                    /* lea rsp, [rbp - (gcc ? 5 : 7) * 8] */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, X86_GREG_xSP, X86_GREG_xBP);
    pbCodeBuf[off++] = (uint8_t)IEMNATIVE_FP_OFF_LAST_PUSH;

    /* Pop non-volatile registers and return */
    pbCodeBuf[off++] = X86_OP_REX_B;            /* pop r15 */
    pbCodeBuf[off++] = 0x58 + X86_GREG_x15 - 8;
    pbCodeBuf[off++] = X86_OP_REX_B;            /* pop r14 */
    pbCodeBuf[off++] = 0x58 + X86_GREG_x14 - 8;
    pbCodeBuf[off++] = X86_OP_REX_B;            /* pop r13 */
    pbCodeBuf[off++] = 0x58 + X86_GREG_x13 - 8;
    pbCodeBuf[off++] = X86_OP_REX_B;            /* pop r12 */
    pbCodeBuf[off++] = 0x58 + X86_GREG_x12 - 8;
# ifdef RT_OS_WINDOWS
    pbCodeBuf[off++] = 0x58 + X86_GREG_xDI;     /* pop rdi */
    pbCodeBuf[off++] = 0x58 + X86_GREG_xSI;     /* pop rsi */
# endif
    pbCodeBuf[off++] = 0x58 + X86_GREG_xBX;     /* pop rbx */
    pbCodeBuf[off++] = 0xc9;                    /* leave */
    pbCodeBuf[off++] = 0xc3;                    /* ret */
    pbCodeBuf[off++] = 0xcc;                    /* int3 poison */

#elif RT_ARCH_ARM64
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
    AssertReturn(pu32CodeBuf, UINT32_MAX);

    /* ldp x19, x20, [sp #IEMNATIVE_FRAME_VAR_SIZE]! ; Unallocate the variable space and restore x19+x20. */
    AssertCompile(IEMNATIVE_FRAME_VAR_SIZE < 64*8);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kPreIndex,
                                                 ARMV8_A64_REG_X19, ARMV8_A64_REG_X20, ARMV8_A64_REG_SP,
                                                 IEMNATIVE_FRAME_VAR_SIZE / 8);
    /* Restore x21 thru x28 + BP and LR (ret address) (SP remains unchanged in the kSigned variant). */
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X21, ARMV8_A64_REG_X22, ARMV8_A64_REG_SP, 2);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X23, ARMV8_A64_REG_X24, ARMV8_A64_REG_SP, 4);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X25, ARMV8_A64_REG_X26, ARMV8_A64_REG_SP, 6);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X27, ARMV8_A64_REG_X28, ARMV8_A64_REG_SP, 8);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_BP,  ARMV8_A64_REG_LR,  ARMV8_A64_REG_SP, 10);
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE / 8 == 12);

    /* add sp, sp, IEMNATIVE_FRAME_SAVE_REG_SIZE ;  */
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE < 4096);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSub(false /*fSub*/, ARMV8_A64_REG_SP, ARMV8_A64_REG_SP, IEMNATIVE_FRAME_SAVE_REG_SIZE);

    /* retab / ret */
# ifdef RT_OS_DARWIN /** @todo See todo on pacibsp in the prolog. */
    if (1)
        pu32CodeBuf[off++] = ARMV8_A64_INSTR_RETAB;
    else
# endif
        pu32CodeBuf[off++] = ARMV8_A64_INSTR_RET;

#else
# error "port me"
#endif

    return iemNativeEmitRcFiddling(pReNative, off, idxReturn);
}


/**
 * Emits a standard prolog.
 */
static uint32_t iemNativeEmitProlog(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
#ifdef RT_ARCH_AMD64
    /*
     * Set up a regular xBP stack frame, pushing all non-volatile GPRs,
     * reserving 64 bytes for stack variables plus 4 non-register argument
     * slots.  Fixed register assignment: xBX = pReNative;
     *
     * Since we always do the same register spilling, we can use the same
     * unwind description for all the code.
     */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0x50 + X86_GREG_xBP;     /* push rbp */
    pbCodeBuf[off++] = X86_OP_REX_W;            /* mov rbp, rsp */
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xBP, X86_GREG_xSP);
    pbCodeBuf[off++] = 0x50 + X86_GREG_xBX;     /* push rbx */
    AssertCompile(IEMNATIVE_REG_FIXED_PVMCPU == X86_GREG_xBX);
# ifdef RT_OS_WINDOWS
    pbCodeBuf[off++] = X86_OP_REX_W;            /* mov rbx, rcx ; RBX = pVCpu */
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xBX, X86_GREG_xCX);
    pbCodeBuf[off++] = 0x50 + X86_GREG_xSI;     /* push rsi */
    pbCodeBuf[off++] = 0x50 + X86_GREG_xDI;     /* push rdi */
# else
    pbCodeBuf[off++] = X86_OP_REX_W;            /* mov rbx, rdi ; RBX = pVCpu */
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xBX, X86_GREG_xDI);
# endif
    pbCodeBuf[off++] = X86_OP_REX_B;            /* push r12 */
    pbCodeBuf[off++] = 0x50 + X86_GREG_x12 - 8;
    pbCodeBuf[off++] = X86_OP_REX_B;            /* push r13 */
    pbCodeBuf[off++] = 0x50 + X86_GREG_x13 - 8;
    pbCodeBuf[off++] = X86_OP_REX_B;            /* push r14 */
    pbCodeBuf[off++] = 0x50 + X86_GREG_x14 - 8;
    pbCodeBuf[off++] = X86_OP_REX_B;            /* push r15 */
    pbCodeBuf[off++] = 0x50 + X86_GREG_x15 - 8;

    off = iemNativeEmitSubGprImm(pReNative, off,    /* sub rsp, byte 28h */
                                 X86_GREG_xSP,
                                   IEMNATIVE_FRAME_ALIGN_SIZE
                                 + IEMNATIVE_FRAME_VAR_SIZE
                                 + IEMNATIVE_FRAME_STACK_ARG_COUNT * 8
                                 + IEMNATIVE_FRAME_SHADOW_ARG_COUNT * 8);
    AssertCompile(!(IEMNATIVE_FRAME_VAR_SIZE & 0xf));
    AssertCompile(!(IEMNATIVE_FRAME_STACK_ARG_COUNT & 0x1));
    AssertCompile(!(IEMNATIVE_FRAME_SHADOW_ARG_COUNT & 0x1));

#elif RT_ARCH_ARM64
    /*
     * We set up a stack frame exactly like on x86, only we have to push the
     * return address our selves here.  We save all non-volatile registers.
     */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
    AssertReturn(pu32CodeBuf, UINT32_MAX);

# ifdef RT_OS_DARWIN /** @todo This seems to be requirement by libunwind for JIT FDEs. Investigate further as been unable
                      * to figure out where the BRK following AUTHB*+XPACB* stuff comes from in libunwind.  It's
                      * definitely the dwarf stepping code, but till found it's very tedious to figure out whether it's
                      * in any way conditional, so just emitting this instructions now and hoping for the best... */
    /* pacibsp */
    pu32CodeBuf[off++] = ARMV8_A64_INSTR_PACIBSP;
# endif

    /* stp x19, x20, [sp, #-IEMNATIVE_FRAME_SAVE_REG_SIZE] ; Allocate space for saving registers and place x19+x20 at the bottom. */
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE < 64*8);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kPreIndex,
                                                 ARMV8_A64_REG_X19, ARMV8_A64_REG_X20, ARMV8_A64_REG_SP,
                                                 -IEMNATIVE_FRAME_SAVE_REG_SIZE / 8);
    /* Save x21 thru x28 (SP remains unchanged in the kSigned variant). */
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X21, ARMV8_A64_REG_X22, ARMV8_A64_REG_SP, 2);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X23, ARMV8_A64_REG_X24, ARMV8_A64_REG_SP, 4);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X25, ARMV8_A64_REG_X26, ARMV8_A64_REG_SP, 6);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_X27, ARMV8_A64_REG_X28, ARMV8_A64_REG_SP, 8);
    /* Save the BP and LR (ret address) registers at the top of the frame. */
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_kSigned,
                                                 ARMV8_A64_REG_BP,  ARMV8_A64_REG_LR,  ARMV8_A64_REG_SP, 10);
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE / 8 == 12);
    /* add bp, sp, IEMNATIVE_FRAME_SAVE_REG_SIZE - 16 ; Set BP to point to the old BP stack address. */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSub(false /*fSub*/, ARMV8_A64_REG_BP,
                                               ARMV8_A64_REG_SP, IEMNATIVE_FRAME_SAVE_REG_SIZE - 16);

    /* sub sp, sp, IEMNATIVE_FRAME_VAR_SIZE ;  Allocate the variable area from SP. */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSub(true /*fSub*/, ARMV8_A64_REG_SP, ARMV8_A64_REG_SP, IEMNATIVE_FRAME_VAR_SIZE);

    /* mov r28, r0  */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_REG_FIXED_PVMCPU, IEMNATIVE_CALL_ARG0_GREG);

#else
# error "port me"
#endif
    return off;
}


DECLINLINE(uint32_t) iemNativeEmitCImplCall1(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                             uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, pfnCImpl, cbInstr, 1, uArg0, 0, 0);
}


DECLINLINE(uint32_t) iemNativeEmitCImplCall2(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                             uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0, uint64_t uArg1)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, pfnCImpl, cbInstr, 2, uArg0, uArg1, 0);
}


DECLINLINE(uint32_t) iemNativeEmitCImplCall3(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                             uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0, uint64_t uArg1, uint64_t uArg2)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, pfnCImpl, cbInstr, 3, uArg0, uArg1, uArg2);
}


/** Same as iemRegFinishClearingRF. */
DECLINLINE(uint32_t) iemNativeEmitFinishClearingRF(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    uint32_t const fFlags = pReNative->pTbOrg->fFlags;
    if (fFlags & IEMTB_F_INHIBIT_SHADOW)
    {
    }
    IEMTB_F_IEM_F_MASK

    //
        if (RT_LIKELY(!(  pVCpu->cpum.GstCtx.eflags.uBoth
                        & (X86_EFL_TF | X86_EFL_RF | CPUMCTX_INHIBIT_SHADOW | CPUMCTX_DBG_HIT_DRX_MASK | CPUMCTX_DBG_DBGF_MASK)) ))
            return VINF_SUCCESS;
        return iemFinishInstructionWithFlagsSet(pVCpu);
}


/** Same as iemRegAddToEip32AndFinishingClearingRF. */
DECLINLINE(uint32_t) iemNativeEmitAddToEip32AndFinishingClearingRF(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Increment RIP. */
    pVCpu->cpum.GstCtx.rip = (uint32_t)(pVCpu->cpum.GstCtx.eip + cbInstr);

    /* Consider flags. */
    return iemNativeEmitFinishClearingRF(pReNative, off);
}


/*
 * MC definitions for the native recompiler.
 */

#define IEM_MC_DEFER_TO_CIMPL_0_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl) \
    return iemNativeEmitCImplCall0(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr) /** @todo not used ... */

#define IEM_MC_DEFER_TO_CIMPL_1_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl, a0) \
    return iemNativeEmitCImplCall1(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr, a0)

#define IEM_MC_DEFER_TO_CIMPL_2_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl, a0, a1) \
    return iemNativeEmitCImplCall2(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr, a0, a1)

#define IEM_MC_DEFER_TO_CIMPL_3_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl, a0, a1, a2) \
    return iemNativeEmitCImplCall3(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr, a0, a1, a2)


#define IEM_MC_BEGIN(a_cArgs, a_cLocals, a_fFlags)      {

#define IEM_MC_END()                                    } AssertFailedReturn(UINT32_MAX /* shouldn't be reached! */)

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16(a_cbInstr) \
    return iemNativeEmitAddToIp16AndFinishingClearingRF(pReNative, off, a_cbInstr)

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32(a_cbInstr) \
    return iemNativeEmitAddToEip32AndFinishingClearingRF(pReNative, off, a_cbInstr)

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64(a_cbInstr) \
    return iemNativeEmitAddToRip64AndFinishingClearingRF(pReNative, off, a_cbInstr)


/*
 * Builtin functions.
 */

/**
 * Built-in function that calls a C-implemention function taking zero arguments.
 */
static IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_DeferToCImpl0)
{
    PFNIEMCIMPL0 const pfnCImpl = (PFNIEMCIMPL0)(uintptr_t)pCallEntry->auParams[0];
    uint8_t const      cbInstr  = (uint8_t)pCallEntry->auParams[1];
    return iemNativeEmitCImplCall(pReNative, off, pCallEntry->idxInstr, (uintptr_t)pfnCImpl, cbInstr, 0, 0, 0, 0);
}



/*
 * Include g_apfnIemNativeRecompileFunctions and associated functions.
 *
 * This should probably live in it's own file later, but lets see what the
 * compile times turn out to be first.
 */
#include "IEMNativeFunctions.cpp.h"


/**
 * Recompiles the given threaded TB into a native one.
 *
 * In case of failure the translation block will be returned as-is.
 *
 * @returns pTb.
 * @param   pVCpu   The cross context virtual CPU structure of the calling
 *                  thread.
 * @param   pTb     The threaded translation to recompile to native.
 */
PIEMTB iemNativeRecompile(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /*
     * The first time thru, we allocate the recompiler state, the other times
     * we just need to reset it before using it again.
     */
    PIEMRECOMPILERSTATE pReNative = pVCpu->iem.s.pNativeRecompilerStateR3;
    if (RT_LIKELY(pReNative))
        iemNativeReInit(pReNative, pTb);
    else
    {
        pReNative = iemNativeInit(pVCpu, pTb);
        AssertReturn(pReNative, pTb);
    }

    /*
     * Emit prolog code (fixed).
     */
    uint32_t off = iemNativeEmitProlog(pReNative, 0);
    AssertReturn(off != UINT32_MAX, pTb);

    /*
     * Convert the calls to native code.
     */
    PCIEMTHRDEDCALLENTRY pCallEntry = pTb->Thrd.paCalls;
    uint32_t             cCallsLeft = pTb->Thrd.cCalls;
    while (cCallsLeft-- > 0)
    {
        PFNIEMNATIVERECOMPFUNC const pfnRecom = g_apfnIemNativeRecompileFunctions[pCallEntry->enmFunction];
        if (pfnRecom) /** @todo stats on this.   */
            off = pfnRecom(pReNative, off, pCallEntry);
        else
            off = iemNativeEmitThreadedCall(pReNative, off, pCallEntry);
        AssertReturn(off != UINT32_MAX, pTb);

        pCallEntry++;
    }

    /*
     * Emit the epilog code.
     */
    off = iemNativeEmitEpilog(pReNative, off);
    AssertReturn(off != UINT32_MAX, pTb);

    /*
     * Make sure all labels has been defined.
     */
    PIEMNATIVELABEL const paLabels = pReNative->paLabels;
#ifdef VBOX_STRICT
    uint32_t const        cLabels  = pReNative->cLabels;
    for (uint32_t i = 0; i < cLabels; i++)
        AssertMsgReturn(paLabels[i].off < off, ("i=%d enmType=%d\n", i, paLabels[i].enmType), pTb);
#endif

    /*
     * Allocate executable memory, copy over the code we've generated.
     */
    PIEMTBALLOCATOR const pTbAllocator = pVCpu->iem.s.pTbAllocatorR3;
    if (pTbAllocator->pDelayedFreeHead)
        iemTbAllocatorProcessDelayedFrees(pVCpu, pVCpu->iem.s.pTbAllocatorR3);

    PIEMNATIVEINSTR const paFinalInstrBuf = (PIEMNATIVEINSTR)iemExecMemAllocatorAlloc(pVCpu, off * sizeof(IEMNATIVEINSTR));
    AssertReturn(paFinalInstrBuf, pTb);
    memcpy(paFinalInstrBuf, pReNative->pInstrBuf, off * sizeof(paFinalInstrBuf[0]));

    /*
     * Apply fixups.
     */
    PIEMNATIVEFIXUP const paFixups   = pReNative->paFixups;
    uint32_t const        cFixups    = pReNative->cFixups;
    for (uint32_t i = 0; i < cFixups; i++)
    {
        Assert(paFixups[i].off < off);
        Assert(paFixups[i].idxLabel < cLabels);
        RTPTRUNION const Ptr = { &paFinalInstrBuf[paFixups[i].off] };
        switch (paFixups[i].enmType)
        {
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
            case kIemNativeFixupType_Rel32:
                Assert(paFixups[i].off + 4 <= off);
                *Ptr.pi32 = paLabels[paFixups[i].idxLabel].off - paFixups[i].off + paFixups[i].offAddend;
                continue;

#elif defined(RT_ARCH_ARM64)
            case kIemNativeFixupType_RelImm19At5:
            {
                Assert(paFixups[i].off < off);
                int32_t const offDisp = paLabels[paFixups[i].idxLabel].off - paFixups[i].off + paFixups[i].offAddend;
                Assert(offDisp >= -262144 && offDisp < 262144);
                *Ptr.pu32 = (*Ptr.pu32 & UINT32_C(0xff00001f)) | (offDisp << 5);
                continue;
            }
#endif
            case kIemNativeFixupType_Invalid:
            case kIemNativeFixupType_End:
                break;
        }
        AssertFailed();
    }

    iemExecMemAllocatorReadyForUse(pVCpu, paFinalInstrBuf, off * sizeof(IEMNATIVEINSTR));

    /*
     * Convert the translation block.
     */
    //RT_BREAKPOINT();
    RTMemFree(pTb->Thrd.paCalls);
    pTb->Native.paInstructions  = paFinalInstrBuf;
    pTb->Native.cInstructions   = off;
    pTb->fFlags                 = (pTb->fFlags & ~IEMTB_F_TYPE_MASK) | IEMTB_F_TYPE_NATIVE;

    Assert(pTbAllocator->cThreadedTbs > 0);
    pTbAllocator->cThreadedTbs -= 1;
    pTbAllocator->cNativeTbs   += 1;
    Assert(pTbAllocator->cNativeTbs <= pTbAllocator->cTotalTbs);

    return pTb;
}

