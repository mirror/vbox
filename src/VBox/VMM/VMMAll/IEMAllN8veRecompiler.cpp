/* $Id$ */
/** @file
 * IEM - Native Recompiler
 *
 * Logging group IEM_RE_NATIVE assignments:
 *      - Level 1  (Log)  : ...
 *      - Flow  (LogFlow) : ...
 *      - Level 2  (Log2) : ...
 *      - Level 3  (Log3) : Disassemble native code after recompiling.
 *      - Level 4  (Log4) : ...
 *      - Level 5  (Log5) : ...
 *      - Level 6  (Log6) : ...
 *      - Level 7  (Log7) : ...
 *      - Level 8  (Log8) : ...
 *      - Level 9  (Log9) : ...
 *      - Level 10 (Log10): ...
 *      - Level 11 (Log11): ...
 *      - Level 12 (Log12): Register allocator
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
#define LOG_GROUP LOG_GROUP_IEM_RE_NATIVE
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
#  ifdef DEBUG_bird /** @todo not thread safe yet */
#   define IEMNATIVE_USE_GDB_JIT
#  endif
#  ifdef IEMNATIVE_USE_GDB_JIT
#   include <iprt/critsect.h>
#   include <iprt/once.h>
#   include <iprt/formats/elf64.h>
#  endif
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
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Always count instructions for now. */
#define IEMNATIVE_WITH_INSTRUCTION_COUNTING


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
static uint32_t iemNativeEmitGuestRegValueCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                uint8_t idxReg, IEMNATIVEGSTREG enmGstReg);


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
# ifdef IEMNATIVE_USE_GDB_JIT
#   define IEMNATIVE_USE_GDB_JIT_ET_DYN

/** GDB JIT: Code entry.   */
typedef struct GDBJITCODEENTRY
{
    struct GDBJITCODEENTRY *pNext;
    struct GDBJITCODEENTRY *pPrev;
    uint8_t                *pbSymFile;
    uint64_t                cbSymFile;
} GDBJITCODEENTRY;

/** GDB JIT: Actions. */
typedef enum GDBJITACTIONS : uint32_t
{
    kGdbJitaction_NoAction = 0, kGdbJitaction_Register, kGdbJitaction_Unregister
} GDBJITACTIONS;

/** GDB JIT: Descriptor. */
typedef struct GDBJITDESCRIPTOR
{
    uint32_t            uVersion;
    GDBJITACTIONS       enmAction;
    GDBJITCODEENTRY    *pRelevant;
    GDBJITCODEENTRY    *pHead;
    /** Our addition: */
    GDBJITCODEENTRY    *pTail;
} GDBJITDESCRIPTOR;

/** GDB JIT: Our simple symbol file data. */
typedef struct GDBJITSYMFILE
{
    Elf64_Ehdr          EHdr;
#  ifndef IEMNATIVE_USE_GDB_JIT_ET_DYN
    Elf64_Shdr          aShdrs[5];
#  else
    Elf64_Shdr          aShdrs[7];
    Elf64_Phdr          aPhdrs[2];
#  endif
    /** The dwarf ehframe data for the chunk. */
    uint8_t             abEhFrame[512];
    char                szzStrTab[128];
    Elf64_Sym           aSymbols[3];
#  ifdef IEMNATIVE_USE_GDB_JIT_ET_DYN
    Elf64_Sym           aDynSyms[2];
    Elf64_Dyn           aDyn[6];
#  endif
} GDBJITSYMFILE;

extern "C" GDBJITDESCRIPTOR __jit_debug_descriptor;
extern "C" DECLEXPORT(void) __jit_debug_register_code(void);

/** Init once for g_IemNativeGdbJitLock. */
static RTONCE     g_IemNativeGdbJitOnce = RTONCE_INITIALIZER;
/** Init once for the critical section. */
static RTCRITSECT g_IemNativeGdbJitLock;

/** GDB reads the info here. */
GDBJITDESCRIPTOR __jit_debug_descriptor = { 1, kGdbJitaction_NoAction, NULL, NULL };

/** GDB sets a breakpoint on this and checks __jit_debug_descriptor when hit. */
DECL_NO_INLINE(RT_NOTHING, DECLEXPORT(void)) __jit_debug_register_code(void)
{
    ASMNopPause();
}

/** @callback_method_impl{FNRTONCE} */
static DECLCALLBACK(int32_t) iemNativeGdbJitInitOnce(void *pvUser)
{
    RT_NOREF(pvUser);
    return RTCritSectInit(&g_IemNativeGdbJitLock);
}


# endif /* IEMNATIVE_USE_GDB_JIT */

/**
 * Per-chunk unwind info for non-windows hosts.
 */
typedef struct IEMEXECMEMCHUNKEHFRAME
{
# ifdef IEMNATIVE_USE_LIBUNWIND
    /** The offset of the FDA into abEhFrame. */
    uintptr_t               offFda;
# else
    /** 'struct object' storage area. */
    uint8_t                 abObject[1024];
# endif
#  ifdef IEMNATIVE_USE_GDB_JIT
#   if 0
    /** The GDB JIT 'symbol file' data. */
    GDBJITSYMFILE           GdbJitSymFile;
#   endif
    /** The GDB JIT list entry. */
    GDBJITCODEENTRY         GdbJitEntry;
#  endif
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


static int iemExecMemAllocatorGrow(PVMCPUCC pVCpu, PIEMEXECMEMALLOCATOR pExecMemAllocator);


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
        int rc = iemExecMemAllocatorGrow(pVCpu, pExecMemAllocator);
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
iemExecMemAllocatorInitAndRegisterUnwindInfoForChunk(PVMCPUCC pVCpu, PIEMEXECMEMALLOCATOR pExecMemAllocator,
                                                     void *pvChunk, uint32_t idxChunk)
{
    RT_NOREF(pVCpu);

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
iemExecMemAllocatorInitAndRegisterUnwindInfoForChunk(PVMCPUCC pVCpu, PIEMEXECMEMALLOCATOR pExecMemAllocator,
                                                     void *pvChunk, uint32_t idxChunk)
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

#  ifdef IEMNATIVE_USE_GDB_JIT
    /*
     * Now for telling GDB about this (experimental).
     *
     * This seems to work best with ET_DYN.
     */
    unsigned const cbNeeded        = sizeof(GDBJITSYMFILE);
#   ifdef IEMEXECMEM_USE_ALT_SUB_ALLOCATOR
    unsigned const cbNeededAligned = RT_ALIGN_32(cbNeeded, IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SIZE);
    GDBJITSYMFILE * const pSymFile = (GDBJITSYMFILE *)iemExecMemAllocatorAllocInChunk(pExecMemAllocator, idxChunk, cbNeededAligned);
#   else
    unsigned const cbNeededAligned = RT_ALIGN_32(cbNeeded + pExecMemAllocator->cbHeapBlockHdr, 64)
                                   - pExecMemAllocator->cbHeapBlockHdr;
    GDBJITSYMFILE * const pSymFile = (PIMAGE_RUNTIME_FUNCTION_ENTRY)RTHeapSimpleAlloc(hHeap, cbNeededAligned, 32 /*cbAlignment*/);
#   endif
    AssertReturn(pSymFile, VERR_INTERNAL_ERROR_5);
    unsigned const offSymFileInChunk = (uintptr_t)pSymFile - (uintptr_t)pvChunk;

    RT_ZERO(*pSymFile);

    /*
     * The ELF header:
     */
    pSymFile->EHdr.e_ident[0]           = ELFMAG0;
    pSymFile->EHdr.e_ident[1]           = ELFMAG1;
    pSymFile->EHdr.e_ident[2]           = ELFMAG2;
    pSymFile->EHdr.e_ident[3]           = ELFMAG3;
    pSymFile->EHdr.e_ident[EI_VERSION]  = EV_CURRENT;
    pSymFile->EHdr.e_ident[EI_CLASS]    = ELFCLASS64;
    pSymFile->EHdr.e_ident[EI_DATA]     = ELFDATA2LSB;
    pSymFile->EHdr.e_ident[EI_OSABI]    = ELFOSABI_NONE;
#   ifdef IEMNATIVE_USE_GDB_JIT_ET_DYN
    pSymFile->EHdr.e_type               = ET_DYN;
#   else
    pSymFile->EHdr.e_type               = ET_REL;
#   endif
#   ifdef RT_ARCH_AMD64
    pSymFile->EHdr.e_machine            = EM_AMD64;
#   elif defined(RT_ARCH_ARM64)
    pSymFile->EHdr.e_machine            = EM_AARCH64;
#   else
#    error "port me"
#   endif
    pSymFile->EHdr.e_version            = 1; /*?*/
    pSymFile->EHdr.e_entry              = 0;
#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN)
    pSymFile->EHdr.e_phoff              = RT_UOFFSETOF(GDBJITSYMFILE, aPhdrs);
#   else
    pSymFile->EHdr.e_phoff              = 0;
#   endif
    pSymFile->EHdr.e_shoff              = sizeof(pSymFile->EHdr);
    pSymFile->EHdr.e_flags              = 0;
    pSymFile->EHdr.e_ehsize             = sizeof(pSymFile->EHdr);
#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN)
    pSymFile->EHdr.e_phentsize          = sizeof(pSymFile->aPhdrs[0]);
    pSymFile->EHdr.e_phnum              = RT_ELEMENTS(pSymFile->aPhdrs);
#   else
    pSymFile->EHdr.e_phentsize          = 0;
    pSymFile->EHdr.e_phnum              = 0;
#   endif
    pSymFile->EHdr.e_shentsize          = sizeof(pSymFile->aShdrs[0]);
    pSymFile->EHdr.e_shnum              = RT_ELEMENTS(pSymFile->aShdrs);
    pSymFile->EHdr.e_shstrndx           = 0; /* set later */

    uint32_t offStrTab = 0;
#define APPEND_STR(a_szStr) do { \
        memcpy(&pSymFile->szzStrTab[offStrTab], a_szStr, sizeof(a_szStr)); \
        offStrTab += sizeof(a_szStr); \
        Assert(offStrTab < sizeof(pSymFile->szzStrTab)); \
    } while (0)
#define APPEND_STR_FMT(a_szStr, ...) do { \
        offStrTab += RTStrPrintf(&pSymFile->szzStrTab[offStrTab], sizeof(pSymFile->szzStrTab) - offStrTab, a_szStr, __VA_ARGS__); \
        offStrTab++; \
        Assert(offStrTab < sizeof(pSymFile->szzStrTab)); \
    } while (0)

    /*
     * Section headers.
     */
    /* Section header #0: NULL */
    unsigned i = 0;
    APPEND_STR("");
    RT_ZERO(pSymFile->aShdrs[i]);
    i++;

    /* Section header: .eh_frame */
    pSymFile->aShdrs[i].sh_name         = offStrTab;
    APPEND_STR(".eh_frame");
    pSymFile->aShdrs[i].sh_type         = SHT_PROGBITS;
    pSymFile->aShdrs[i].sh_flags        = SHF_ALLOC | SHF_EXECINSTR;
#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN) || defined(IEMNATIVE_USE_GDB_JIT_ELF_RVAS)
    pSymFile->aShdrs[i].sh_offset
        = pSymFile->aShdrs[i].sh_addr   = RT_UOFFSETOF(GDBJITSYMFILE, abEhFrame);
#   else
    pSymFile->aShdrs[i].sh_addr         = (uintptr_t)&pSymFile->abEhFrame[0];
    pSymFile->aShdrs[i].sh_offset       = 0;
#   endif

    pSymFile->aShdrs[i].sh_size         = sizeof(pEhFrame->abEhFrame);
    pSymFile->aShdrs[i].sh_link         = 0;
    pSymFile->aShdrs[i].sh_info         = 0;
    pSymFile->aShdrs[i].sh_addralign    = 1;
    pSymFile->aShdrs[i].sh_entsize      = 0;
    memcpy(pSymFile->abEhFrame, pEhFrame->abEhFrame, sizeof(pEhFrame->abEhFrame));
    i++;

    /* Section header: .shstrtab */
    unsigned const iShStrTab = i;
    pSymFile->EHdr.e_shstrndx           = iShStrTab;
    pSymFile->aShdrs[i].sh_name         = offStrTab;
    APPEND_STR(".shstrtab");
    pSymFile->aShdrs[i].sh_type         = SHT_STRTAB;
    pSymFile->aShdrs[i].sh_flags        = SHF_ALLOC;
#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN) || defined(IEMNATIVE_USE_GDB_JIT_ELF_RVAS)
    pSymFile->aShdrs[i].sh_offset
        = pSymFile->aShdrs[i].sh_addr   = RT_UOFFSETOF(GDBJITSYMFILE, szzStrTab);
#   else
    pSymFile->aShdrs[i].sh_addr         = (uintptr_t)&pSymFile->szzStrTab[0];
    pSymFile->aShdrs[i].sh_offset       = 0;
#   endif
    pSymFile->aShdrs[i].sh_size         = sizeof(pSymFile->szzStrTab);
    pSymFile->aShdrs[i].sh_link         = 0;
    pSymFile->aShdrs[i].sh_info         = 0;
    pSymFile->aShdrs[i].sh_addralign    = 1;
    pSymFile->aShdrs[i].sh_entsize      = 0;
    i++;

    /* Section header: .symbols */
    pSymFile->aShdrs[i].sh_name         = offStrTab;
    APPEND_STR(".symtab");
    pSymFile->aShdrs[i].sh_type         = SHT_SYMTAB;
    pSymFile->aShdrs[i].sh_flags        = SHF_ALLOC;
    pSymFile->aShdrs[i].sh_offset
        = pSymFile->aShdrs[i].sh_addr   = RT_UOFFSETOF(GDBJITSYMFILE, aSymbols);
    pSymFile->aShdrs[i].sh_size         = sizeof(pSymFile->aSymbols);
    pSymFile->aShdrs[i].sh_link         = iShStrTab;
    pSymFile->aShdrs[i].sh_info         = RT_ELEMENTS(pSymFile->aSymbols);
    pSymFile->aShdrs[i].sh_addralign    = sizeof(pSymFile->aSymbols[0].st_value);
    pSymFile->aShdrs[i].sh_entsize      = sizeof(pSymFile->aSymbols[0]);
    i++;

#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN)
    /* Section header: .symbols */
    pSymFile->aShdrs[i].sh_name         = offStrTab;
    APPEND_STR(".dynsym");
    pSymFile->aShdrs[i].sh_type         = SHT_DYNSYM;
    pSymFile->aShdrs[i].sh_flags        = SHF_ALLOC;
    pSymFile->aShdrs[i].sh_offset
        = pSymFile->aShdrs[i].sh_addr   = RT_UOFFSETOF(GDBJITSYMFILE, aDynSyms);
    pSymFile->aShdrs[i].sh_size         = sizeof(pSymFile->aDynSyms);
    pSymFile->aShdrs[i].sh_link         = iShStrTab;
    pSymFile->aShdrs[i].sh_info         = RT_ELEMENTS(pSymFile->aDynSyms);
    pSymFile->aShdrs[i].sh_addralign    = sizeof(pSymFile->aDynSyms[0].st_value);
    pSymFile->aShdrs[i].sh_entsize      = sizeof(pSymFile->aDynSyms[0]);
    i++;
#   endif

#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN)
    /* Section header: .dynamic */
    pSymFile->aShdrs[i].sh_name         = offStrTab;
    APPEND_STR(".dynamic");
    pSymFile->aShdrs[i].sh_type         = SHT_DYNAMIC;
    pSymFile->aShdrs[i].sh_flags        = SHF_ALLOC;
    pSymFile->aShdrs[i].sh_offset
        = pSymFile->aShdrs[i].sh_addr   = RT_UOFFSETOF(GDBJITSYMFILE, aDyn);
    pSymFile->aShdrs[i].sh_size         = sizeof(pSymFile->aDyn);
    pSymFile->aShdrs[i].sh_link         = iShStrTab;
    pSymFile->aShdrs[i].sh_info         = 0;
    pSymFile->aShdrs[i].sh_addralign    = 1;
    pSymFile->aShdrs[i].sh_entsize      = sizeof(pSymFile->aDyn[0]);
    i++;
#   endif

    /* Section header: .text */
    unsigned const iShText = i;
    pSymFile->aShdrs[i].sh_name         = offStrTab;
    APPEND_STR(".text");
    pSymFile->aShdrs[i].sh_type         = SHT_PROGBITS;
    pSymFile->aShdrs[i].sh_flags        = SHF_ALLOC | SHF_EXECINSTR;
#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN) || defined(IEMNATIVE_USE_GDB_JIT_ELF_RVAS)
    pSymFile->aShdrs[i].sh_offset
        = pSymFile->aShdrs[i].sh_addr   = sizeof(GDBJITSYMFILE);
#   else
    pSymFile->aShdrs[i].sh_addr         = (uintptr_t)(pSymFile + 1);
    pSymFile->aShdrs[i].sh_offset       = 0;
#   endif
    pSymFile->aShdrs[i].sh_size         = pExecMemAllocator->cbChunk - offSymFileInChunk - sizeof(GDBJITSYMFILE);
    pSymFile->aShdrs[i].sh_link         = 0;
    pSymFile->aShdrs[i].sh_info         = 0;
    pSymFile->aShdrs[i].sh_addralign    = 1;
    pSymFile->aShdrs[i].sh_entsize      = 0;
    i++;

    Assert(i == RT_ELEMENTS(pSymFile->aShdrs));

#   if defined(IEMNATIVE_USE_GDB_JIT_ET_DYN)
    /*
     * The program headers:
     */
    /* Everything in a single LOAD segment: */
    i = 0;
    pSymFile->aPhdrs[i].p_type          = PT_LOAD;
    pSymFile->aPhdrs[i].p_flags         = PF_X | PF_R;
    pSymFile->aPhdrs[i].p_offset
        = pSymFile->aPhdrs[i].p_vaddr
        = pSymFile->aPhdrs[i].p_paddr   = 0;
    pSymFile->aPhdrs[i].p_filesz         /* Size of segment in file. */
        = pSymFile->aPhdrs[i].p_memsz   = pExecMemAllocator->cbChunk - offSymFileInChunk;
    pSymFile->aPhdrs[i].p_align         = HOST_PAGE_SIZE;
    i++;
    /* The .dynamic segment. */
    pSymFile->aPhdrs[i].p_type          = PT_DYNAMIC;
    pSymFile->aPhdrs[i].p_flags         = PF_R;
    pSymFile->aPhdrs[i].p_offset
        = pSymFile->aPhdrs[i].p_vaddr
        = pSymFile->aPhdrs[i].p_paddr   = RT_UOFFSETOF(GDBJITSYMFILE, aDyn);
    pSymFile->aPhdrs[i].p_filesz         /* Size of segment in file. */
        = pSymFile->aPhdrs[i].p_memsz   = sizeof(pSymFile->aDyn);
    pSymFile->aPhdrs[i].p_align         = sizeof(pSymFile->aDyn[0].d_tag);
    i++;

    Assert(i == RT_ELEMENTS(pSymFile->aPhdrs));

    /*
     * The dynamic section:
     */
    i = 0;
    pSymFile->aDyn[i].d_tag             = DT_SONAME;
    pSymFile->aDyn[i].d_un.d_val        = offStrTab;
    APPEND_STR_FMT("iem-exec-chunk-%u-%u", pVCpu->idCpu, idxChunk);
    i++;
    pSymFile->aDyn[i].d_tag             = DT_STRTAB;
    pSymFile->aDyn[i].d_un.d_ptr        = RT_UOFFSETOF(GDBJITSYMFILE, szzStrTab);
    i++;
    pSymFile->aDyn[i].d_tag             = DT_STRSZ;
    pSymFile->aDyn[i].d_un.d_val        = sizeof(pSymFile->szzStrTab);
    i++;
    pSymFile->aDyn[i].d_tag             = DT_SYMTAB;
    pSymFile->aDyn[i].d_un.d_ptr        = RT_UOFFSETOF(GDBJITSYMFILE, aDynSyms);
    i++;
    pSymFile->aDyn[i].d_tag             = DT_SYMENT;
    pSymFile->aDyn[i].d_un.d_val        = sizeof(pSymFile->aDynSyms[0]);
    i++;
    pSymFile->aDyn[i].d_tag             = DT_NULL;
    i++;
    Assert(i == RT_ELEMENTS(pSymFile->aDyn));
#   endif /* IEMNATIVE_USE_GDB_JIT_ET_DYN */

    /*
     * Symbol tables:
     */
    /** @todo gdb doesn't seem to really like this ...   */
    i = 0;
    pSymFile->aSymbols[i].st_name       = 0;
    pSymFile->aSymbols[i].st_shndx      = SHN_UNDEF;
    pSymFile->aSymbols[i].st_value      = 0;
    pSymFile->aSymbols[i].st_size       = 0;
    pSymFile->aSymbols[i].st_info       = ELF64_ST_INFO(STB_LOCAL, STT_NOTYPE);
    pSymFile->aSymbols[i].st_other      = 0 /* STV_DEFAULT */;
#   ifdef IEMNATIVE_USE_GDB_JIT_ET_DYN
    pSymFile->aDynSyms[0] = pSymFile->aSymbols[i];
#   endif
    i++;

    pSymFile->aSymbols[i].st_name       = 0;
    pSymFile->aSymbols[i].st_shndx      = SHN_ABS;
    pSymFile->aSymbols[i].st_value      = 0;
    pSymFile->aSymbols[i].st_size       = 0;
    pSymFile->aSymbols[i].st_info       = ELF64_ST_INFO(STB_LOCAL, STT_FILE);
    pSymFile->aSymbols[i].st_other      = 0 /* STV_DEFAULT */;
    i++;

    pSymFile->aSymbols[i].st_name       = offStrTab;
    APPEND_STR_FMT("iem_exec_chunk_%u_%u", pVCpu->idCpu, idxChunk);
#   if 0
    pSymFile->aSymbols[i].st_shndx      = iShText;
    pSymFile->aSymbols[i].st_value      = 0;
#   else
    pSymFile->aSymbols[i].st_shndx      = SHN_ABS;
    pSymFile->aSymbols[i].st_value      = (uintptr_t)(pSymFile + 1);
#   endif
    pSymFile->aSymbols[i].st_size       = pSymFile->aShdrs[iShText].sh_size;
    pSymFile->aSymbols[i].st_info       = ELF64_ST_INFO(STB_GLOBAL, STT_FUNC);
    pSymFile->aSymbols[i].st_other      = 0 /* STV_DEFAULT */;
#   ifdef IEMNATIVE_USE_GDB_JIT_ET_DYN
    pSymFile->aDynSyms[1] = pSymFile->aSymbols[i];
    pSymFile->aDynSyms[1].st_value      = (uintptr_t)(pSymFile + 1);
#   endif
    i++;

    Assert(i == RT_ELEMENTS(pSymFile->aSymbols));
    Assert(offStrTab < sizeof(pSymFile->szzStrTab));

    /*
     * The GDB JIT entry and informing GDB.
     */
    pEhFrame->GdbJitEntry.pbSymFile = (uint8_t *)pSymFile;
#   if 1
    pEhFrame->GdbJitEntry.cbSymFile = pExecMemAllocator->cbChunk - ((uintptr_t)pSymFile - (uintptr_t)pvChunk);
#   else
    pEhFrame->GdbJitEntry.cbSymFile = sizeof(GDBJITSYMFILE);
#   endif

    RTOnce(&g_IemNativeGdbJitOnce, iemNativeGdbJitInitOnce, NULL);
    RTCritSectEnter(&g_IemNativeGdbJitLock);
    pEhFrame->GdbJitEntry.pNext      = NULL;
    pEhFrame->GdbJitEntry.pPrev      = __jit_debug_descriptor.pTail;
    if (__jit_debug_descriptor.pTail)
        __jit_debug_descriptor.pTail->pNext = &pEhFrame->GdbJitEntry;
    else
        __jit_debug_descriptor.pHead = &pEhFrame->GdbJitEntry;
    __jit_debug_descriptor.pTail     = &pEhFrame->GdbJitEntry;
    __jit_debug_descriptor.pRelevant = &pEhFrame->GdbJitEntry;

    /* Notify GDB: */
    __jit_debug_descriptor.enmAction = kGdbJitaction_Register;
    __jit_debug_register_code();
    __jit_debug_descriptor.enmAction = kGdbJitaction_NoAction;
    RTCritSectLeave(&g_IemNativeGdbJitLock);

#  else  /* !IEMNATIVE_USE_GDB_JIT */
    RT_NOREF(pVCpu);
#  endif /* !IEMNATIVE_USE_GDB_JIT */

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
static int iemExecMemAllocatorGrow(PVMCPUCC pVCpu, PIEMEXECMEMALLOCATOR pExecMemAllocator)
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
            rc = iemExecMemAllocatorInitAndRegisterUnwindInfoForChunk(pVCpu, pExecMemAllocator, pvChunk, idxChunk);
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
    RT_NOREF(pVCpu);
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
        int rc = iemExecMemAllocatorGrow(pVCpu, pExecMemAllocator);
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
 * Used by TB code when it wants to raise a \#GP(0).
 */
IEM_DECL_IMPL_DEF(int, iemNativeHlpExecRaiseGp0,(PVMCPUCC pVCpu, uint8_t idxInstr))
{
    pVCpu->iem.s.cInstructions += idxInstr;
    iemRaiseGeneralProtectionFault0Jmp(pVCpu);
    return VINF_IEM_RAISED_XCPT; /* not reached */
}


/**
 * Reinitializes the native recompiler state.
 *
 * Called before starting a new recompile job.
 */
static PIEMRECOMPILERSTATE iemNativeReInit(PIEMRECOMPILERSTATE pReNative, PCIEMTB pTb)
{
    pReNative->cLabels                = 0;
    pReNative->bmLabelTypes           = 0;
    pReNative->cFixups                = 0;
    pReNative->pTbOrg                 = pTb;

    pReNative->bmHstRegs              = IEMNATIVE_REG_FIXED_MASK
#if IEMNATIVE_HST_GREG_COUNT < 32
                                      | ~(RT_BIT(IEMNATIVE_HST_GREG_COUNT) - 1U)
#endif
                                      ;
    pReNative->bmHstRegsWithGstShadow = 0;
    pReNative->bmGstRegShadows        = 0;
    pReNative->cCondDepth             = 0;
    pReNative->uCondSeqNo             = 0;
    pReNative->bmVars                 = 0;
    pReNative->u64ArgVars             = UINT64_MAX;

    /* Full host register reinit: */
    for (unsigned i = 0; i < RT_ELEMENTS(pReNative->aHstRegs); i++)
    {
        pReNative->aHstRegs[i].fGstRegShadows = 0;
        pReNative->aHstRegs[i].enmWhat        = kIemNativeWhat_Invalid;
        pReNative->aHstRegs[i].idxVar         = UINT8_MAX;
    }

    uint32_t fRegs = IEMNATIVE_REG_FIXED_MASK
                   & ~(  RT_BIT_32(IEMNATIVE_REG_FIXED_PVMCPU)
#ifdef IEMNATIVE_REG_FIXED_PCPUMCTX
                       | RT_BIT_32(IEMNATIVE_REG_FIXED_PCPUMCTX)
#endif
#ifdef IEMNATIVE_REG_FIXED_PCPUMCTX
                       | RT_BIT_32(IEMNATIVE_REG_FIXED_TMP0)
#endif
                      );
    for (uint32_t idxReg = ASMBitFirstSetU32(fRegs) - 1; fRegs != 0; idxReg = ASMBitFirstSetU32(fRegs) - 1)
    {
        fRegs &= ~RT_BIT_32(idxReg);
        pReNative->aHstRegs[IEMNATIVE_REG_FIXED_PVMCPU].enmWhat = kIemNativeWhat_FixedReserved;
    }

    pReNative->aHstRegs[IEMNATIVE_REG_FIXED_PVMCPU].enmWhat     = kIemNativeWhat_pVCpuFixed;
#ifdef IEMNATIVE_REG_FIXED_PCPUMCTX
    pReNative->aHstRegs[IEMNATIVE_REG_FIXED_PCPUMCTX].enmWhat   = kIemNativeWhat_pCtxFixed;
#endif
#ifdef IEMNATIVE_REG_FIXED_TMP0
    pReNative->aHstRegs[IEMNATIVE_REG_FIXED_TMP0].enmWhat       = kIemNativeWhat_FixedTmp;
#endif
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

    Assert(enmType >= 0 && enmType < 64);
    pReNative->bmLabelTypes |= RT_BIT_64(enmType);
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
    Assert(enmType >= 0 && enmType < 64);
    if (RT_BIT_64(enmType) & pReNative->bmLabelTypes)
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
    }
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
#if RT_ARCH_ARM64
    AssertReturn(cbNew <= _1M, NULL); /* Limited by the branch instruction range (18+2 bits). */
#else
    AssertReturn(cbNew <= _2M, NULL);
#endif

    void *pvNew = RTMemRealloc(pReNative->pInstrBuf, cbNew);
    AssertReturn(pvNew, NULL);

    pReNative->cInstrBufAlloc   = cNew;
    return pReNative->pInstrBuf = (PIEMNATIVEINSTR)pvNew;
}



/*********************************************************************************************************************************
*   Register Allocator                                                                                                           *
*********************************************************************************************************************************/

/**
 * Register parameter indexes (indexed by argument number).
 */
DECL_HIDDEN_CONST(uint8_t) const g_aidxIemNativeCallRegs[] =
{
    IEMNATIVE_CALL_ARG0_GREG,
    IEMNATIVE_CALL_ARG1_GREG,
    IEMNATIVE_CALL_ARG2_GREG,
    IEMNATIVE_CALL_ARG3_GREG,
#if defined(IEMNATIVE_CALL_ARG4_GREG)
    IEMNATIVE_CALL_ARG4_GREG,
# if defined(IEMNATIVE_CALL_ARG5_GREG)
    IEMNATIVE_CALL_ARG5_GREG,
#  if defined(IEMNATIVE_CALL_ARG6_GREG)
    IEMNATIVE_CALL_ARG6_GREG,
#   if defined(IEMNATIVE_CALL_ARG7_GREG)
    IEMNATIVE_CALL_ARG7_GREG,
#   endif
#  endif
# endif
#endif
};

/**
 * Call register masks indexed by argument count.
 */
DECL_HIDDEN_CONST(uint32_t) const g_afIemNativeCallRegs[] =
{
    0,
    RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG),
    RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG),
    RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG2_GREG),
      RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG2_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG3_GREG),
#if defined(IEMNATIVE_CALL_ARG4_GREG)
      RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG2_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG3_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG4_GREG),
# if defined(IEMNATIVE_CALL_ARG5_GREG)
      RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG2_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG3_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG4_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG5_GREG),
#  if defined(IEMNATIVE_CALL_ARG6_GREG)
      RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG2_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG3_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG4_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG5_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG6_GREG),
#   if defined(IEMNATIVE_CALL_ARG7_GREG)
      RT_BIT_32(IEMNATIVE_CALL_ARG0_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG1_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG2_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG3_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG4_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG5_GREG)
    | RT_BIT_32(IEMNATIVE_CALL_ARG6_GREG) | RT_BIT_32(IEMNATIVE_CALL_ARG7_GREG),
#   endif
#  endif
# endif
#endif
};

/**
 * Info about shadowed guest register values.
 * @see IEMNATIVEGSTREG
 */
static struct
{
    /** Offset in VMCPU. */
    uint32_t    off;
    /** The field size. */
    uint8_t     cb;
    /** Name (for logging). */
    const char *pszName;
} const g_aGstShadowInfo[] =
{
#define CPUMCTX_OFF_AND_SIZE(a_Reg) RT_UOFFSETOF(VMCPU, cpum.GstCtx. a_Reg), RT_SIZEOFMEMB(VMCPU, cpum.GstCtx. a_Reg)
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xAX] = */  { CPUMCTX_OFF_AND_SIZE(rax),                "rax", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xCX] = */  { CPUMCTX_OFF_AND_SIZE(rcx),                "rcx", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xDX] = */  { CPUMCTX_OFF_AND_SIZE(rdx),                "rdx", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xBX] = */  { CPUMCTX_OFF_AND_SIZE(rbx),                "rbx", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xSP] = */  { CPUMCTX_OFF_AND_SIZE(rsp),                "rsp", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xBP] = */  { CPUMCTX_OFF_AND_SIZE(rbp),                "rbp", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xSI] = */  { CPUMCTX_OFF_AND_SIZE(rsi),                "rsi", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_xDI] = */  { CPUMCTX_OFF_AND_SIZE(rdi),                "rdi", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x8 ] = */  { CPUMCTX_OFF_AND_SIZE(r8),                 "r8", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x9 ] = */  { CPUMCTX_OFF_AND_SIZE(r9),                 "r9", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x10] = */  { CPUMCTX_OFF_AND_SIZE(r10),                "r10", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x11] = */  { CPUMCTX_OFF_AND_SIZE(r11),                "r11", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x12] = */  { CPUMCTX_OFF_AND_SIZE(r12),                "r12", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x13] = */  { CPUMCTX_OFF_AND_SIZE(r13),                "r13", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x14] = */  { CPUMCTX_OFF_AND_SIZE(r14),                "r14", },
    /* [kIemNativeGstReg_GprFirst + X86_GREG_x15] = */  { CPUMCTX_OFF_AND_SIZE(r15),                "r15", },
    /* [kIemNativeGstReg_Pc] = */                       { CPUMCTX_OFF_AND_SIZE(rip),                "rip", },
    /* [kIemNativeGstReg_EFlags] = */                   { CPUMCTX_OFF_AND_SIZE(eflags),             "eflags", },
    /* [18] = */                                        { UINT32_C(0xfffffff7),                  0, NULL, },
    /* [19] = */                                        { UINT32_C(0xfffffff5),                  0, NULL, },
    /* [20] = */                                        { UINT32_C(0xfffffff3),                  0, NULL, },
    /* [21] = */                                        { UINT32_C(0xfffffff1),                  0, NULL, },
    /* [22] = */                                        { UINT32_C(0xffffffef),                  0, NULL, },
    /* [23] = */                                        { UINT32_C(0xffffffed),                  0, NULL, },
    /* [kIemNativeGstReg_SegSelFirst + 0] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[0].Sel),      "es", },
    /* [kIemNativeGstReg_SegSelFirst + 1] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[1].Sel),      "cs", },
    /* [kIemNativeGstReg_SegSelFirst + 2] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[2].Sel),      "ss", },
    /* [kIemNativeGstReg_SegSelFirst + 3] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[3].Sel),      "ds", },
    /* [kIemNativeGstReg_SegSelFirst + 4] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[4].Sel),      "fs", },
    /* [kIemNativeGstReg_SegSelFirst + 5] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[5].Sel),      "gs", },
    /* [kIemNativeGstReg_SegBaseFirst + 0] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[0].u64Base),  "es_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 1] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[1].u64Base),  "cs_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 2] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[2].u64Base),  "ss_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 3] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[3].u64Base),  "ds_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 4] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[4].u64Base),  "fs_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 5] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[5].u64Base),  "gs_base", },
    /* [kIemNativeGstReg_SegLimitFirst + 0] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[0].u32Limit), "es_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 1] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[1].u32Limit), "cs_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 2] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[2].u32Limit), "ss_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 3] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[3].u32Limit), "ds_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 4] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[4].u32Limit), "fs_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 5] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[5].u32Limit), "gs_limit", },
#undef CPUMCTX_OFF_AND_SIZE
};
AssertCompile(RT_ELEMENTS(g_aGstShadowInfo) == kIemNativeGstReg_End);


/** Host CPU general purpose register names. */
const char * const g_apszIemNativeHstRegNames[] =
{
#ifdef RT_ARCH_AMD64
    "rax", "rcx", "rdx", "rbx", "rsp", "rbp", "rsi", "rdi", "r8", "r9", "r10", "r11", "r12", "r13", "r14", "r15"
#elif RT_ARCH_ARM64
    "x0",  "x1",  "x2",  "x3",  "x4",  "x5",  "x6",  "x7",  "x8",  "x9",  "x10", "x11", "x12", "x13", "x14", "x15",
    "x16", "x17", "x18", "x19", "x20", "x21", "x22", "x23", "x24", "x25", "x26", "x27", "x28", "bp",  "lr",  "sp/xzr",
#else
# error "port me"
#endif
};


DECL_FORCE_INLINE(uint8_t) iemNativeRegMarkAllocated(PIEMRECOMPILERSTATE pReNative, unsigned idxReg,
                                                     IEMNATIVEWHAT enmWhat, uint8_t idxVar = UINT8_MAX) RT_NOEXCEPT
{
    pReNative->bmHstRegs |= RT_BIT_32(idxReg);

    pReNative->aHstRegs[idxReg].enmWhat        = enmWhat;
    pReNative->aHstRegs[idxReg].fGstRegShadows = 0;
    pReNative->aHstRegs[idxReg].idxVar         = idxVar;
    return (uint8_t)idxReg;
}


/**
 * Locate a register, possibly freeing one up.
 *
 * This ASSUMES the caller has done the minimal/optimal allocation checks and
 * failed.
 */
static uint8_t iemNativeRegAllocFindFree(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, bool fAllowVolatile) RT_NOEXCEPT
{
    uint32_t fRegMask = fAllowVolatile
                      ? IEMNATIVE_HST_GREG_MASK & ~IEMNATIVE_REG_FIXED_MASK
                      : IEMNATIVE_HST_GREG_MASK & ~(IEMNATIVE_REG_FIXED_MASK | IEMNATIVE_CALL_VOLATILE_GREG_MASK);

    /*
     * Try a freed register that's shadowing a guest register
     */
    uint32_t fRegs = ~pReNative->bmHstRegs & fRegMask;
    if (fRegs)
    {
        /** @todo pick better here:    */
        unsigned const idxReg = ASMBitFirstSetU32(fRegs) - 1;

        Assert(pReNative->aHstRegs[idxReg].fGstRegShadows != 0);
        Assert(   (pReNative->aHstRegs[idxReg].fGstRegShadows & pReNative->bmGstRegShadows)
               == pReNative->aHstRegs[idxReg].fGstRegShadows);
        Assert(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

        pReNative->bmGstRegShadows        &= ~pReNative->aHstRegs[idxReg].fGstRegShadows;
        pReNative->bmHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);
        pReNative->aHstRegs[idxReg].fGstRegShadows = 0;
        return idxReg;
    }

    /*
     * Try free up a variable that's in a register.
     *
     * We do two rounds here, first evacuating variables we don't need to be
     * saved on the stack, then in the second round move things to the stack.
     */
    for (uint32_t iLoop = 0; iLoop < 2; iLoop++)
    {
        uint32_t fVars = pReNative->bmVars;
        while (fVars)
        {
            uint32_t const idxVar = ASMBitFirstSetU32(fVars) - 1;
            uint8_t const  idxReg = pReNative->aVars[idxVar].idxReg;
            if (   idxReg < RT_ELEMENTS(pReNative->aHstRegs)
                && (RT_BIT_32(idxReg) & fRegMask)
                && (  iLoop == 0
                    ? pReNative->aVars[idxVar].enmKind != kIemNativeVarKind_Stack
                    : pReNative->aVars[idxVar].enmKind == kIemNativeVarKind_Stack))
            {
                Assert(pReNative->bmHstRegs & RT_BIT_32(idxReg));
                Assert(   (pReNative->bmGstRegShadows & pReNative->aHstRegs[idxReg].fGstRegShadows)
                       == pReNative->aHstRegs[idxReg].fGstRegShadows);
                Assert(   RT_BOOL(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxReg))
                       == RT_BOOL(pReNative->aHstRegs[idxReg].fGstRegShadows));

                if (pReNative->aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                {
                    AssertReturn(pReNative->aVars[idxVar].idxStackSlot != UINT8_MAX, UINT8_MAX);
                    uint32_t off = *poff;
                    *poff = off = iemNativeEmitStoreGprByBp(pReNative, off,
                                                              pReNative->aVars[idxVar].idxStackSlot * sizeof(uint64_t)
                                                            - IEMNATIVE_FP_OFF_STACK_VARS,
                                                            idxReg);
                    AssertReturn(off != UINT32_MAX, UINT8_MAX);
                }

                pReNative->aVars[idxVar].idxReg    = UINT8_MAX;
                pReNative->bmGstRegShadows        &= ~pReNative->aHstRegs[idxReg].fGstRegShadows;
                pReNative->bmHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);
                pReNative->bmHstRegs              &= ~RT_BIT_32(idxReg);
                return idxReg;
            }
            fVars &= ~RT_BIT_32(idxVar);
        }
    }

    AssertFailedReturn(UINT8_MAX);
}


/**
 * Moves a variable to a different register or spills it onto the stack.
 *
 * This must be a stack variable (kIemNativeVarKind_Stack) because the other
 * kinds can easily be recreated if needed later.
 *
 * @returns The new code buffer position, UINT32_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   off             The current code buffer position.
 * @param   idxVar          The variable index.
 * @param   fForbiddenRegs  Mask of the forbidden registers.  Defaults to
 *                          call-volatile registers.
 */
static uint32_t iemNativeRegMoveOrSpillStackVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVar,
                                                uint32_t fForbiddenRegs = IEMNATIVE_CALL_VOLATILE_GREG_MASK)
{
    Assert(idxVar < RT_ELEMENTS(pReNative->aVars));
    Assert(pReNative->aVars[idxVar].enmKind == kIemNativeVarKind_Stack);

    uint8_t const idxRegOld = pReNative->aVars[idxVar].idxReg;
    Assert(idxRegOld < RT_ELEMENTS(pReNative->aHstRegs));
    Assert(pReNative->bmHstRegs & RT_BIT_32(idxRegOld));
    Assert(pReNative->aHstRegs[idxRegOld].enmWhat == kIemNativeWhat_Var);
    Assert(   (pReNative->bmGstRegShadows & pReNative->aHstRegs[idxRegOld].fGstRegShadows)
           == pReNative->aHstRegs[idxRegOld].fGstRegShadows);
    Assert(   RT_BOOL(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxRegOld))
           == RT_BOOL(pReNative->aHstRegs[idxRegOld].fGstRegShadows));


    /** @todo Add statistics on this.*/
    /** @todo Implement basic variable liveness analysis (python) so variables
     * can be freed immediately once no longer used.  This has the potential to
     * be trashing registers and stack for dead variables. */

    /*
     * First try move it to a different register, as that's cheaper.
     */
    fForbiddenRegs |= RT_BIT_32(idxRegOld);
    fForbiddenRegs |= IEMNATIVE_REG_FIXED_MASK;
    uint32_t fRegs = ~pReNative->bmHstRegs & ~fForbiddenRegs;
    if (fRegs)
    {
        /* Avoid using shadow registers, if possible. */
        if (fRegs & ~pReNative->bmHstRegsWithGstShadow)
            fRegs &= ~pReNative->bmHstRegsWithGstShadow;
        unsigned const idxRegNew = ASMBitFirstSetU32(fRegs) - 1;

        uint64_t fGstRegShadows = pReNative->aHstRegs[idxRegOld].fGstRegShadows;
        pReNative->aHstRegs[idxRegNew].fGstRegShadows = fGstRegShadows;
        pReNative->aHstRegs[idxRegNew].enmWhat        = kIemNativeWhat_Var;
        pReNative->aHstRegs[idxRegNew].idxVar         = idxVar;
        if (fGstRegShadows)
        {
            pReNative->bmHstRegsWithGstShadow |= RT_BIT_32(idxRegNew);
            while (fGstRegShadows)
            {
                unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegShadows);
                fGstRegShadows &= ~RT_BIT_64(idxGstReg);

                Assert(pReNative->aidxGstRegShadows[idxGstReg] == idxRegOld);
                pReNative->aidxGstRegShadows[idxGstReg] = idxRegNew;
            }
        }

        pReNative->aVars[idxVar].idxReg = (uint8_t)idxRegNew;
        pReNative->bmHstRegs           |= RT_BIT_32(idxRegNew);
    }
    /*
     * Otherwise we must spill the register onto the stack.
     */
    else
    {
        AssertReturn(pReNative->aVars[idxVar].idxStackSlot != UINT8_MAX, UINT32_MAX);
        off = iemNativeEmitStoreGprByBp(pReNative, off,
                                        pReNative->aVars[idxVar].idxStackSlot * sizeof(uint64_t) - IEMNATIVE_FP_OFF_STACK_VARS,
                                        idxRegOld);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);

        pReNative->bmHstRegsWithGstShadow &= ~RT_BIT_32(idxRegOld);
        pReNative->bmGstRegShadows        &= ~pReNative->aHstRegs[idxRegOld].fGstRegShadows;
    }

    pReNative->bmHstRegs &= ~RT_BIT_32(idxRegOld);
    pReNative->aHstRegs[idxRegOld].fGstRegShadows = 0;
    return off;
}


/**
 * Allocates a temporary host general purpose register.
 *
 * This may emit code to save register content onto the stack in order to free
 * up a register.
 *
 * @returns The host register number, UINT8_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 *                          This will be update if we need to move a variable from
 *                          register to stack in order to satisfy the request.
 * @param   fPreferVolatile Wheter to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false, for iemNativeRegAllocTmpForGuestReg()).
 */
DECLHIDDEN(uint8_t) iemNativeRegAllocTmp(PIEMRECOMPILERSTATE pReNative, uint32_t *poff,
                                         bool fPreferVolatile /*= true*/) RT_NOEXCEPT
{
    /*
     * Try find a completely unused register, preferably a call-volatile one.
     */
    uint8_t  idxReg;
    uint32_t fRegs = ~pReNative->bmHstRegs
                   & ~pReNative->bmHstRegsWithGstShadow
                   & (~IEMNATIVE_REG_FIXED_MASK & IEMNATIVE_HST_GREG_MASK);
    if (fRegs)
    {
        if (fPreferVolatile)
            idxReg = (uint8_t)ASMBitFirstSetU32(  fRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                ? fRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
        else
            idxReg = (uint8_t)ASMBitFirstSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                ? fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
        Assert(pReNative->aHstRegs[idxReg].fGstRegShadows == 0);
        Assert(!(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxReg)));
    }
    else
    {
        idxReg = iemNativeRegAllocFindFree(pReNative, poff, true /*fAllowVolatile*/);
        AssertReturn(idxReg != UINT8_MAX, UINT8_MAX);
    }
    return iemNativeRegMarkAllocated(pReNative, idxReg, kIemNativeWhat_Tmp);
}


/**
 * Allocates a temporary register for loading an immediate value into.
 *
 * This will emit code to load the immediate, unless there happens to be an
 * unused register with the value already loaded.
 *
 * The caller will not modify the returned register, it must be considered
 * read-only.  Free using iemNativeRegFreeTmpImm.
 *
 * @returns The host register number, UINT8_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 * @param   uImm            The immediate value that the register must hold upon
 *                          return.
 * @param   fPreferVolatile Wheter to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false).
 *
 * @note    Reusing immediate values has not been implemented yet.
 */
DECLHIDDEN(uint8_t) iemNativeRegAllocTmpImm(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint64_t uImm,
                                            bool fPreferVolatile /*= true*/) RT_NOEXCEPT
{
    uint8_t idxReg = iemNativeRegAllocTmp(pReNative, poff, fPreferVolatile);
    if (idxReg < RT_ELEMENTS(pReNative->aHstRegs))
    {
        uint32_t off = *poff;
        *poff = off = iemNativeEmitLoadGprImm64(pReNative, off, idxReg, uImm);
        AssertReturnStmt(off != UINT32_MAX, iemNativeRegFreeTmp(pReNative, idxReg), UINT8_MAX);
    }
    return idxReg;
}


/**
 * Marks host register @a idxHstReg as containing a shadow copy of guest
 * register @a enmGstReg.
 *
 * ASSUMES that caller has made sure @a enmGstReg is not associated with any
 * host register before calling.
 */
DECL_FORCE_INLINE(void)
iemNativeRegMarkAsGstRegShadow(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg)
{
    Assert(!(pReNative->bmGstRegShadows & RT_BIT_64(enmGstReg)));

    pReNative->aidxGstRegShadows[enmGstReg]       = idxHstReg;
    pReNative->aHstRegs[idxHstReg].fGstRegShadows = RT_BIT_64(enmGstReg);
    pReNative->bmGstRegShadows                   |= RT_BIT_64(enmGstReg);
    pReNative->bmHstRegsWithGstShadow            |= RT_BIT_32(idxHstReg);
}


/**
 * Clear any guest register shadow claims from @a idxHstReg.
 *
 * The register does not need to be shadowing any guest registers.
 */
DECL_FORCE_INLINE(void)
iemNativeRegClearGstRegShadowing(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg)
{
    Assert(   (pReNative->bmGstRegShadows & pReNative->aHstRegs[idxHstReg].fGstRegShadows)
           == pReNative->aHstRegs[idxHstReg].fGstRegShadows);
    Assert(   RT_BOOL(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg))
           == RT_BOOL(pReNative->aHstRegs[idxHstReg].fGstRegShadows));

    pReNative->bmHstRegsWithGstShadow            &= ~RT_BIT_32(idxHstReg);
    pReNative->bmGstRegShadows                   &= ~pReNative->aHstRegs[idxHstReg].fGstRegShadows;
    pReNative->aHstRegs[idxHstReg].fGstRegShadows = 0;
}


/**
 * Transfers the guest register shadow claims of @a enmGstReg from @a idxRegFrom
 * to @a idxRegTo.
 */
DECL_FORCE_INLINE(void)
iemNativeRegTransferGstRegShadowing(PIEMRECOMPILERSTATE pReNative, uint8_t idxRegFrom, uint8_t idxRegTo, IEMNATIVEGSTREG enmGstReg)
{
    Assert(pReNative->aHstRegs[idxRegFrom].fGstRegShadows & RT_BIT_64(enmGstReg));
    Assert(   (pReNative->bmGstRegShadows & pReNative->aHstRegs[idxRegFrom].fGstRegShadows)
           == pReNative->aHstRegs[idxRegFrom].fGstRegShadows);
    Assert(   RT_BOOL(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxRegFrom))
           == RT_BOOL(pReNative->aHstRegs[idxRegFrom].fGstRegShadows));

    pReNative->aHstRegs[idxRegFrom].fGstRegShadows &= ~RT_BIT_64(enmGstReg);
    pReNative->aHstRegs[idxRegTo].fGstRegShadows    = RT_BIT_64(enmGstReg);
    pReNative->aidxGstRegShadows[enmGstReg]         = idxRegTo;
}



/**
 * Intended use statement for iemNativeRegAllocTmpForGuestReg().
 */
typedef enum IEMNATIVEGSTREGUSE
{
    /** The usage is read-only, the register holding the guest register
     * shadow copy will not be modified by the caller. */
    kIemNativeGstRegUse_ReadOnly = 0,
    /** The caller will update the guest register (think: PC += cbInstr).
     * The guest shadow copy will follow the returned register. */
    kIemNativeGstRegUse_ForUpdate,
    /** The caller will use the guest register value as input in a calculation
     * and the host register will be modified.
     * This means that the returned host register will not be marked as a shadow
     * copy of the guest register. */
    kIemNativeGstRegUse_Calculation
} IEMNATIVEGSTREGUSE;

/**
 * Allocates a temporary host general purpose register for updating a guest
 * register value.
 *
 * Since we may already have a register holding the guest register value,
 * code will be emitted to do the loading if that's not the case. Code may also
 * be emitted if we have to free up a register to satify the request.
 *
 * @returns The host register number, UINT8_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer
 *                          position. This will be update if we need to move a
 *                          variable from register to stack in order to satisfy
 *                          the request.
 * @param   enmGstReg       The guest register that will is to be updated.
 * @param   enmIntendedUse  How the caller will be using the host register.
 */
DECLHIDDEN(uint8_t) iemNativeRegAllocTmpForGuestReg(PIEMRECOMPILERSTATE pReNative, uint32_t *poff,
                                                    IEMNATIVEGSTREG enmGstReg, IEMNATIVEGSTREGUSE enmIntendedUse) RT_NOEXCEPT
{
    Assert(enmGstReg < kIemNativeGstReg_End && g_aGstShadowInfo[enmGstReg].cb != 0);
#ifdef LOG_ENABLED
    static const char * const s_pszIntendedUse[] = { "fetch", "update", "destructive calc" };
#endif

    /*
     * First check if the guest register value is already in a host register.
     */
    if (pReNative->bmGstRegShadows & RT_BIT_64(enmGstReg))
    {
        uint8_t idxReg = pReNative->aidxGstRegShadows[enmGstReg];
        Assert(idxReg < RT_ELEMENTS(pReNative->aHstRegs));
        Assert(pReNative->aHstRegs[idxReg].fGstRegShadows & RT_BIT_64(enmGstReg));
        Assert(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

        if (!(pReNative->bmHstRegs & RT_BIT_32(idxReg)))
        {
            /*
             * If the register will trash the guest shadow copy, try find a
             * completely unused register we can use instead.  If that fails,
             * we need to disassociate the host reg from the guest reg.
             */
            /** @todo would be nice to know if preserving the register is in any way helpful. */
            if (   enmIntendedUse == kIemNativeGstRegUse_Calculation
                && (  ~pReNative->bmHstRegs
                    & ~pReNative->bmHstRegsWithGstShadow
                    & (~IEMNATIVE_REG_FIXED_MASK & IEMNATIVE_HST_GREG_MASK)))
            {
                uint8_t const idxRegNew = iemNativeRegAllocTmp(pReNative, poff);
                Assert(idxRegNew < RT_ELEMENTS(pReNative->aHstRegs));

                uint32_t off = *poff;
                *poff = off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegNew, idxReg);
                AssertReturn(off != UINT32_MAX, UINT8_MAX);

                Log12(("iemNativeRegAllocTmpForGuestReg: Duplicated %s for guest %s into %s for destructive calc\n",
                       g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                       g_apszIemNativeHstRegNames[idxRegNew]));
                idxReg = idxRegNew;
            }
            else
            {
                pReNative->bmHstRegs |= RT_BIT_32(idxReg);
                pReNative->aHstRegs[idxReg].enmWhat = kIemNativeWhat_Tmp;
                pReNative->aHstRegs[idxReg].idxVar  = UINT8_MAX;
                if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
                    Log12(("iemNativeRegAllocTmpForGuestReg: Reusing %s for guest %s %s\n",
                           g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName, s_pszIntendedUse[enmIntendedUse]));
                else
                {
                    iemNativeRegClearGstRegShadowing(pReNative, idxReg);
                    Log12(("iemNativeRegAllocTmpForGuestReg: Grabbing %s for guest %s - destructive calc\n",
                           g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName));
                }
            }
        }
        else
        {
            AssertMsg(enmIntendedUse != kIemNativeGstRegUse_ForUpdate,
                      ("This shouldn't happen: idxReg=%d enmGstReg=%d\n", idxReg, enmGstReg));

            /*
             * Allocate a new register, copy the value and, if updating, the
             * guest shadow copy assignment to the new register.
             */
            /** @todo share register for readonly access. */
            uint8_t const idxRegNew = iemNativeRegAllocTmp(pReNative, poff, enmIntendedUse == kIemNativeGstRegUse_Calculation);
            AssertReturn(idxRegNew < RT_ELEMENTS(pReNative->aHstRegs), UINT8_MAX);

            uint32_t off = *poff;
            *poff = off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegNew, idxReg);
            AssertReturn(off != UINT32_MAX, UINT8_MAX);

            if (enmIntendedUse != kIemNativeGstRegUse_ForUpdate)
                Log12(("iemNativeRegAllocTmpForGuestReg: Duplicated %s for guest %s into %s for %s\n",
                       g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                       g_apszIemNativeHstRegNames[idxRegNew], s_pszIntendedUse[enmIntendedUse]));
            else
            {
                iemNativeRegTransferGstRegShadowing(pReNative, idxReg, idxRegNew, enmGstReg);
                Log12(("iemNativeRegAllocTmpForGuestReg: Moved %s for guest %s into %s for update\n",
                       g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                       g_apszIemNativeHstRegNames[idxRegNew]));
            }
            idxReg = idxRegNew;
        }

#ifdef VBOX_STRICT
        /* Strict builds: Check that the value is correct. */
        uint32_t off = *poff;
        *poff = off = iemNativeEmitGuestRegValueCheck(pReNative, off, idxReg, enmGstReg);
        AssertReturn(off != UINT32_MAX, UINT8_MAX);
#endif

        return idxReg;
    }

    /*
     * Allocate a new register, load it with the guest value and designate it as a copy of the
     */
    uint8_t const idxRegNew = iemNativeRegAllocTmp(pReNative, poff, enmIntendedUse == kIemNativeGstRegUse_Calculation);
    AssertReturn(idxRegNew < RT_ELEMENTS(pReNative->aHstRegs), UINT8_MAX);

    uint32_t off = *poff;
    *poff = off = iemNativeEmitLoadGprWithGstShadowReg(pReNative, off, idxRegNew, enmGstReg);
    AssertReturn(off != UINT32_MAX, UINT8_MAX);

    if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
        iemNativeRegMarkAsGstRegShadow(pReNative, idxRegNew, enmGstReg);
    Log12(("iemNativeRegAllocTmpForGuestReg: Allocated %s for guest %s %s\n",
           g_apszIemNativeHstRegNames[idxRegNew], g_aGstShadowInfo[enmGstReg].pszName, s_pszIntendedUse[enmIntendedUse]));

    return idxRegNew;
}


DECLHIDDEN(uint8_t)         iemNativeRegAllocVar(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint8_t idxVar) RT_NOEXCEPT;


/**
 * Allocates argument registers for a function call.
 *
 * @returns New code buffer offset on success, UINT32_MAX on failure.
 * @param   pReNative   The native recompile state.
 * @param   off         The current code buffer offset.
 * @param   cArgs       The number of arguments the function call takes.
 */
DECLHIDDEN(uint32_t) iemNativeRegAllocArgs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs) RT_NOEXCEPT
{
    AssertReturn(cArgs <= IEMNATIVE_CALL_ARG_GREG_COUNT + IEMNATIVE_FRAME_STACK_ARG_COUNT, false);
    Assert(RT_ELEMENTS(g_aidxIemNativeCallRegs) == IEMNATIVE_CALL_ARG_GREG_COUNT);
    Assert(RT_ELEMENTS(g_afIemNativeCallRegs) == IEMNATIVE_CALL_ARG_GREG_COUNT);

    if (cArgs > RT_ELEMENTS(g_aidxIemNativeCallRegs))
        cArgs = RT_ELEMENTS(g_aidxIemNativeCallRegs);
    else if (cArgs == 0)
        return true;

    /*
     * Do we get luck and all register are free and not shadowing anything?
     */
    if (((pReNative->bmHstRegs | pReNative->bmHstRegsWithGstShadow) & g_afIemNativeCallRegs[cArgs]) == 0)
        for (uint32_t i = 0; i < cArgs; i++)
        {
            uint8_t const idxReg = g_aidxIemNativeCallRegs[i];
            pReNative->aHstRegs[idxReg].enmWhat = kIemNativeWhat_Arg;
            pReNative->aHstRegs[idxReg].idxVar  = UINT8_MAX;
            Assert(pReNative->aHstRegs[idxReg].fGstRegShadows == 0);
        }
    /*
     * Okay, not lucky so we have to free up the registers.
     */
    else
        for (uint32_t i = 0; i < cArgs; i++)
        {
            uint8_t const idxReg = g_aidxIemNativeCallRegs[i];
            if (pReNative->bmHstRegs & RT_BIT_32(idxReg))
            {
                switch (pReNative->aHstRegs[idxReg].enmWhat)
                {
                    case kIemNativeWhat_Var:
                    {
                        uint8_t const idxVar = pReNative->aHstRegs[idxReg].idxVar;
                        AssertReturn(idxVar < RT_ELEMENTS(pReNative->aVars), false);
                        Assert(pReNative->aVars[idxVar].idxReg == idxReg);
                        Assert(pReNative->bmVars & RT_BIT_32(idxVar));

                        if (pReNative->aVars[idxVar].enmKind != kIemNativeVarKind_Stack)
                            pReNative->aVars[idxVar].idxReg = UINT8_MAX;
                        else
                        {
                            off = iemNativeRegMoveOrSpillStackVar(pReNative, off, idxVar);
                            AssertReturn(off != UINT32_MAX, false);
                            Assert(!(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxReg)));
                        }
                        break;
                    }

                    case kIemNativeWhat_Tmp:
                    case kIemNativeWhat_Arg:
                    case kIemNativeWhat_rc:
                        AssertFailedReturn(false);
                    default:
                        AssertFailedReturn(false);
                }

            }
            if (pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxReg))
            {
                Assert(pReNative->aHstRegs[idxReg].fGstRegShadows != 0);
                Assert(   (pReNative->aHstRegs[idxReg].fGstRegShadows & pReNative->bmGstRegShadows)
                       == pReNative->aHstRegs[idxReg].fGstRegShadows);
                pReNative->bmGstRegShadows &= ~pReNative->aHstRegs[idxReg].fGstRegShadows;
                pReNative->aHstRegs[idxReg].fGstRegShadows = 0;
            }
            else
                Assert(pReNative->aHstRegs[idxReg].fGstRegShadows == 0);
            pReNative->aHstRegs[idxReg].enmWhat = kIemNativeWhat_Arg;
            pReNative->aHstRegs[idxReg].idxVar  = UINT8_MAX;
        }
    pReNative->bmHstRegs |= g_afIemNativeCallRegs[cArgs];
    return true;
}


DECLHIDDEN(uint8_t)         iemNativeRegAssignRc(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT;


#if 0
/**
 * Frees a register assignment of any type.
 *
 * @param   pReNative       The native recompile state.
 * @param   idxHstReg       The register to free.
 *
 * @note    Does not update variables.
 */
DECLHIDDEN(void) iemNativeRegFree(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT
{
    Assert(idxHstReg < RT_ELEMENTS(pReNative->aHstRegs));
    Assert(pReNative->bmHstRegs & RT_BIT_32(idxHstReg));
    Assert(!(IEMNATIVE_REG_FIXED_MASK & RT_BIT_32(idxHstReg)));
    Assert(   pReNative->aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Var
           || pReNative->aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Tmp
           || pReNative->aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Arg
           || pReNative->aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_rc);
    Assert(   pReNative->aHstRegs[idxHstReg].enmWhat != kIemNativeWhat_Var
           || pReNative->aVars[pReNative->aHstRegs[idxHstReg].idxVar].idxReg == UINT8_MAX
           || (pReNative->bmVars & RT_BIT_32(pReNative->aHstRegs[idxHstReg].idxVar)));
    Assert(   (pReNative->bmGstRegShadows & pReNative->aHstRegs[idxHstReg].fGstRegShadows)
           == pReNative->aHstRegs[idxHstReg].fGstRegShadows);
    Assert(   RT_BOOL(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg))
           == RT_BOOL(pReNative->aHstRegs[idxHstReg].fGstRegShadows));

    pReNative->bmHstRegs              &= ~RT_BIT_32(idxHstReg);
    /* no flushing, right:
    pReNative->bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
    pReNative->bmGstRegShadows        &= ~pReNative->aHstRegs[idxHstReg].fGstRegShadows;
    pReNative->aHstRegs[idxHstReg].fGstRegShadows = 0;
    */
}
#endif


/**
 * Frees a temporary register.
 *
 * Any shadow copies of guest registers assigned to the host register will not
 * be flushed by this operation.
 */
DECLHIDDEN(void) iemNativeRegFreeTmp(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT
{
    Assert(pReNative->bmHstRegs & RT_BIT_32(idxHstReg));
    Assert(pReNative->aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Tmp);
    pReNative->bmHstRegs &= ~RT_BIT_32(idxHstReg);
    Log12(("iemNativeRegFreeTmp: %s (gst: %#RX64)\n",
           g_apszIemNativeHstRegNames[idxHstReg], pReNative->aHstRegs[idxHstReg].fGstRegShadows));
}


/**
 * Frees a temporary immediate register.
 *
 * It is assumed that the call has not modified the register, so it still hold
 * the same value as when it was allocated via iemNativeRegAllocTmpImm().
 */
DECLHIDDEN(void) iemNativeRegFreeTmpImm(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg) RT_NOEXCEPT
{
    iemNativeRegFreeTmp(pReNative, idxHstReg);
}


/**
 * Called right before emitting a call instruction to move anything important
 * out of call-volatile registers, free and flush the call-volatile registers,
 * optionally freeing argument variables.
 *
 * @returns New code buffer offset, UINT32_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   cArgs           The number of arguments the function call takes.
 *                          It is presumed that the host register part of these have
 *                          been allocated as such already and won't need moving,
 *                          just freeing.
 * @param   fFreeArgVars    Whether to free argument variables for the call.
 */
DECLHIDDEN(uint32_t) iemNativeRegMoveAndFreeAndFlushAtCall(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                           uint8_t cArgs, bool fFreeArgVars) RT_NOEXCEPT
{
    /*
     * Free argument variables first (simplified).
     */
    AssertReturn(cArgs <= RT_ELEMENTS(pReNative->aidxArgVars), UINT32_MAX);
    if (fFreeArgVars && cArgs > 0)
    {
        for (uint32_t i = 0; i < cArgs; i++)
        {
            uint8_t idxVar = pReNative->aidxArgVars[i];
            if (idxVar < RT_ELEMENTS(pReNative->aVars))
            {
                pReNative->aidxArgVars[i] = UINT8_MAX;
                pReNative->bmVars        &= ~RT_BIT_32(idxVar);
                Assert(   pReNative->aVars[idxVar].idxReg
                       == (i < RT_ELEMENTS(g_aidxIemNativeCallRegs) ? g_aidxIemNativeCallRegs[i] : UINT8_MAX));
            }
        }
        Assert(pReNative->u64ArgVars == UINT64_MAX);
    }

    /*
     * Move anything important out of volatile registers.
     */
    if (cArgs > RT_ELEMENTS(g_aidxIemNativeCallRegs))
        cArgs = RT_ELEMENTS(g_aidxIemNativeCallRegs);
    uint32_t fRegsToMove = IEMNATIVE_CALL_VOLATILE_GREG_MASK
#ifdef IEMNATIVE_REG_FIXED_TMP0
                         & ~RT_BIT_32(IEMNATIVE_REG_FIXED_TMP0)
#endif
                         & ~g_afIemNativeCallRegs[cArgs];

    fRegsToMove &= pReNative->bmHstRegs;
    if (!fRegsToMove)
    { /* likely */ }
    else
        while (fRegsToMove != 0)
        {
            unsigned const idxReg = ASMBitFirstSetU32(fRegsToMove) - 1;
            fRegsToMove &= ~RT_BIT_32(idxReg);

            switch (pReNative->aHstRegs[idxReg].enmWhat)
            {
                case kIemNativeWhat_Var:
                {
                    uint8_t const idxVar = pReNative->aHstRegs[idxReg].idxVar;
                    Assert(idxVar < RT_ELEMENTS(pReNative->aVars));
                    Assert(pReNative->bmVars & RT_BIT_32(idxVar));
                    Assert(pReNative->aVars[idxVar].idxReg == idxReg);
                    if (pReNative->aVars[idxVar].enmKind != kIemNativeVarKind_Stack)
                        pReNative->aVars[idxVar].idxReg = UINT8_MAX;
                    else
                    {
                        off = iemNativeRegMoveOrSpillStackVar(pReNative, off, idxVar);
                        AssertReturn(off != UINT32_MAX, UINT32_MAX);
                    }
                    continue;
                }

                case kIemNativeWhat_Arg:
                    AssertMsgFailed(("What?!?: %u\n", idxReg));
                    continue;

                case kIemNativeWhat_rc:
                case kIemNativeWhat_Tmp:
                    AssertMsgFailed(("Missing free: %u\n", idxReg));
                    continue;

                case kIemNativeWhat_FixedTmp:
                case kIemNativeWhat_pVCpuFixed:
                case kIemNativeWhat_pCtxFixed:
                case kIemNativeWhat_FixedReserved:
                case kIemNativeWhat_Invalid:
                case kIemNativeWhat_End:
                    AssertFailedReturn(UINT32_MAX);
            }
            AssertFailedReturn(UINT32_MAX);
        }

    /*
     * Do the actual freeing.
     */
    pReNative->bmHstRegs &= ~IEMNATIVE_CALL_VOLATILE_GREG_MASK;

    /* If there are guest register shadows in any call-volatile register, we
       have to clear the corrsponding guest register masks for each register. */
    uint32_t fHstRegsWithGstShadow = pReNative->bmHstRegsWithGstShadow & IEMNATIVE_CALL_VOLATILE_GREG_MASK;
    if (fHstRegsWithGstShadow)
    {
        pReNative->bmHstRegsWithGstShadow &= ~fHstRegsWithGstShadow;
        do
        {
            unsigned const idxReg = ASMBitFirstSetU32(fHstRegsWithGstShadow) - 1;
            fHstRegsWithGstShadow = ~RT_BIT_32(idxReg);

            Assert(pReNative->aHstRegs[idxReg].fGstRegShadows != 0);
            pReNative->bmGstRegShadows &= ~pReNative->aHstRegs[idxReg].fGstRegShadows;
            pReNative->aHstRegs[idxReg].fGstRegShadows = 0;
        } while (fHstRegsWithGstShadow != 0);
    }

    return off;
}


/**
 * Flushes a set of guest register shadow copies.
 *
 * This is usually done after calling a threaded function or a C-implementation
 * of an instruction.
 *
 * @param   pReNative       The native recompile state.
 * @param   fGstRegs        Set of guest registers to flush.
 */
DECLHIDDEN(void) iemNativeRegFlushGuestShadows(PIEMRECOMPILERSTATE pReNative, uint64_t fGstRegs) RT_NOEXCEPT
{
    /*
     * Reduce the mask by what's currently shadowed
     */
    fGstRegs &= pReNative->bmGstRegShadows;
    if (fGstRegs)
    {
        pReNative->bmGstRegShadows &= ~fGstRegs;
        if (pReNative->bmGstRegShadows)
        {
            /*
             * Partial.
             */
            do
            {
                unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegs) - 1;
                uint8_t const  idxHstReg = pReNative->aidxGstRegShadows[idxGstReg];
                Assert(idxHstReg < RT_ELEMENTS(pReNative->aidxGstRegShadows));
                Assert(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg));
                Assert(pReNative->aHstRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg));

                uint64_t const fInThisHstReg = (pReNative->aHstRegs[idxHstReg].fGstRegShadows & fGstRegs) | RT_BIT_64(idxGstReg);
                fGstRegs &= ~fInThisHstReg;
                pReNative->aHstRegs[idxHstReg].fGstRegShadows &= fInThisHstReg;
                if (!pReNative->aHstRegs[idxHstReg].fGstRegShadows)
                    pReNative->bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
            } while (fGstRegs != 0);
        }
        else
        {
            /*
             * Clear all.
             */
            do
            {
                unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegs) - 1;
                uint8_t const  idxHstReg = pReNative->aidxGstRegShadows[idxGstReg];
                Assert(idxHstReg < RT_ELEMENTS(pReNative->aidxGstRegShadows));
                Assert(pReNative->bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg));
                Assert(pReNative->aHstRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg));

                fGstRegs &= ~(pReNative->aHstRegs[idxHstReg].fGstRegShadows | RT_BIT_64(idxGstReg));
                pReNative->aHstRegs[idxHstReg].fGstRegShadows = 0;
            } while (fGstRegs != 0);
            pReNative->bmHstRegsWithGstShadow = 0;
        }
    }
}


/**
 * Flushes any delayed guest register writes.
 *
 * This must be called prior to calling CImpl functions and any helpers that use
 * the guest state (like raising exceptions) and such.
 *
 * This optimization has not yet been implemented.  The first target would be
 * RIP updates, since these are the most common ones.
 */
DECLHIDDEN(uint32_t) iemNativeRegFlushPendingWrites(PIEMRECOMPILERSTATE pReNative, uint32_t off) RT_NOEXCEPT
{
    RT_NOREF(pReNative, off);
    return off;
}


/*********************************************************************************************************************************
*   Code Emitters (larger snippets)                                                                                              *
*********************************************************************************************************************************/

/**
 * Loads the guest shadow register @a enmGstReg into host reg @a idxHstReg, zero
 * extending to 64-bit width.
 *
 * @returns New code buffer offset on success, UINT32_MAX on failure.
 * @param   pReNative   .
 * @param   off         The current code buffer position.
 * @param   idxHstReg   The host register to load the guest register value into.
 * @param   enmGstReg   The guest register to load.
 *
 * @note This does not mark @a idxHstReg as having a shadow copy of @a enmGstReg,
 *       that is something the caller needs to do if applicable.
 */
DECLHIDDEN(uint32_t) iemNativeEmitLoadGprWithGstShadowReg(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                          uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg) RT_NOEXCEPT
{
    Assert((unsigned)enmGstReg < RT_ELEMENTS(g_aGstShadowInfo));
    Assert(g_aGstShadowInfo[enmGstReg].cb != 0);

    switch (g_aGstShadowInfo[enmGstReg].cb)
    {
        case sizeof(uint64_t):
            return iemNativeEmitLoadGprFromVCpuU64(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
        case sizeof(uint32_t):
            return iemNativeEmitLoadGprFromVCpuU32(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
        case sizeof(uint16_t):
            return iemNativeEmitLoadGprFromVCpuU16(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
#if 0 /* not present in the table. */
        case sizeof(uint8_t):
            return iemNativeEmitLoadGprFromVCpuU8(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
#endif
        default:
            AssertFailedReturn(UINT32_MAX);
    }
}


#ifdef VBOX_STRICT
/**
 * Emitting code that checks that the content of register @a idxReg is the same
 * as what's in the guest register @a enmGstReg, resulting in a breakpoint
 * instruction if that's not the case.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
static uint32_t iemNativeEmitGuestRegValueCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                uint8_t idxReg, IEMNATIVEGSTREG enmGstReg)
{
# ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /* cmp reg, [mem] */
    if (g_aGstShadowInfo[enmGstReg].cb == sizeof(uint8_t))
    {
        if (idxReg >= 8)
            pbCodeBuf[off++] = X86_OP_REX_R;
        pbCodeBuf[off++] = 0x38;
    }
    else
    {
        if (g_aGstShadowInfo[enmGstReg].cb == sizeof(uint64_t))
            pbCodeBuf[off++] = X86_OP_REX_W | (idxReg < 8 ? 0 : X86_OP_REX_R);
        else
        {
            if (g_aGstShadowInfo[enmGstReg].cb == sizeof(uint16_t))
                pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
            else
                AssertReturn(g_aGstShadowInfo[enmGstReg].cb == sizeof(uint32_t), UINT32_MAX);
            if (idxReg >= 8)
                pbCodeBuf[off++] = X86_OP_REX_R;
        }
        pbCodeBuf[off++] = 0x39;
    }
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, idxReg, g_aGstShadowInfo[enmGstReg].off);

    /* je/jz +1 */
    pbCodeBuf[off++] = 0x74;
    pbCodeBuf[off++] = 0x01;

    /* int3 */
    pbCodeBuf[off++] = 0xcc;

    /* For values smaller than the register size, we must check that the rest
       of the register is all zeros. */
    if (g_aGstShadowInfo[enmGstReg].cb < sizeof(uint32_t))
    {
        /* test reg64, imm32 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xf7;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxReg & 7);
        pbCodeBuf[off++] = 0;
        pbCodeBuf[off++] = g_aGstShadowInfo[enmGstReg].cb > sizeof(uint8_t) ? 0 : 0xff;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;

        /* je/jz +1 */
        pbCodeBuf[off++] = 0x74;
        pbCodeBuf[off++] = 0x01;

        /* int3 */
        pbCodeBuf[off++] = 0xcc;
    }
    else if (g_aGstShadowInfo[enmGstReg].cb == sizeof(uint32_t))
    {
        /* rol reg64, 32 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxReg & 7);
        pbCodeBuf[off++] = 32;

        /* test reg32, ffffffffh */
        if (idxReg >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
        pbCodeBuf[off++] = 0xf7;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxReg & 7);
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;
        pbCodeBuf[off++] = 0xff;

        /* je/jz +1 */
        pbCodeBuf[off++] = 0x74;
        pbCodeBuf[off++] = 0x01;

        /* int3 */
        pbCodeBuf[off++] = 0xcc;

        /* rol reg64, 32 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxReg & 7);
        pbCodeBuf[off++] = 32;
    }

# elif defined(RT_ARCH_ARM64)
    /* mov TMP0, [gstreg] */
    off = iemNativeEmitLoadGprWithGstShadowReg(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, enmGstReg);

    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    AssertReturn(pu32CodeBuf, UINT32_MAX);
    /* sub tmp0, tmp0, idxReg */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(true /*fSub*/, IEMNATIVE_REG_FIXED_TMP0, IEMNATIVE_REG_FIXED_TMP0, idxReg);
    /* cbz tmp0, +1 */
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 1, IEMNATIVE_REG_FIXED_TMP0);
    /* brk #0x1000+enmGstReg */
    pu32CodeBuf[off++] = Armv8A64MkInstrBrk((uint32_t)enmGstReg | UINT32_C(0x1000));

# else
#  error "Port me!"
# endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
#endif /* VBOX_STRICT */



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
# ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, X86_GREG_xCX, idxInstr);
# endif

    /* edx = eax | rcPassUp */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0x0b;                    /* or edx, eax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xDX, X86_GREG_xAX);
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    /* Jump to non-zero status return path. */
    off = iemNativeEmitJnzToNewLabel(pReNative, off, kIemNativeLabelType_NonZeroRetOrPassUp);

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
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emits code to check if the content of @a idxAddrReg is a canonical address,
 * raising a \#GP(0) if it isn't.
 *
 * @returns New code buffer offset, UINT32_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   idxAddrReg      The host register with the address to check.
 * @param   idxInstr        The current instruction.
 */
DECLHIDDEN(uint32_t) iemNativeEmitCheckGprCanonicalMaybeRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                 uint8_t idxAddrReg, uint8_t idxInstr)
{
    RT_NOREF(idxInstr);

    /*
     * Make sure we don't have any outstanding guest register writes as we may
     * raise an #GP(0) and all guest register must be up to date in CPUMCTX.
     */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

#ifdef RT_ARCH_AMD64
    /*
     * if ((((uint32_t)(a_u64Addr >> 32) + UINT32_C(0x8000)) >> 16) != 0)
     *     return raisexcpt();
     * ---- this wariant avoid loading a 64-bit immediate, but is an instruction longer.
     */
    uint8_t const iTmpReg = iemNativeRegAllocTmp(pReNative, &off);
    AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->aHstRegs), UINT32_MAX);

    off = iemNativeEmitLoadGprFromGpr(pReNative, off, iTmpReg, idxAddrReg);
    off = iemNativeEmitShiftGprRight(pReNative, off, iTmpReg, 32);
    off = iemNativeEmitAddGpr32Imm(pReNative, off, iTmpReg, (int32_t)0x8000);
    off = iemNativeEmitShiftGprRight(pReNative, off, iTmpReg, 16);

# ifndef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitJnzToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);
# else
    uint32_t const offFixup = off;
    off = iemNativeEmitJzToFixed(pReNative, off, 0);
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxInstr);
    off = iemNativeEmitJmpToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);
    iemNativeFixupFixedJump(pReNative, offFixup, off /*offTarget*/);
# endif

    iemNativeRegFreeTmp(pReNative, iTmpReg);

#elif defined(RT_ARCH_ARM64)
    /*
     * if ((((uint64_t)(a_u64Addr) + UINT64_C(0x800000000000)) >> 48) != 0)
     *     return raisexcpt();
     * ----
     *     mov     x1, 0x800000000000
     *     add     x1, x0, x1
     *     cmp     xzr, x1, lsr 48
     * and either:
     *     b.ne    .Lraisexcpt
     * or:
     *     b.eq    .Lnoexcept
     *     movz    x1, #instruction-number
     *     b       .Lraisexcpt
     * .Lnoexcept:
     */
    uint8_t const iTmpReg = iemNativeRegAllocTmp(pReNative, &off);
    AssertReturn(iTmpReg < RT_ELEMENTS(pReNative->aHstRegs), UINT32_MAX);

    off = iemNativeEmitLoadGprImm64(pReNative, off, iTmpReg, UINT64_C(0x800000000000));
    off = iemNativeEmitAddTwoGprs(pReNative, off, iTmpReg, idxAddrReg);
    off = iemNativeEmitCmpArm64(pReNative, off, ARMV8_A64_REG_XZR, idxAddrReg, true /*f64Bit*/, 48 /*cShift*/, kArmv8A64InstrShift_Lsr);

# ifndef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitJnzToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);
# else
    uint32_t const offFixup = off;
    off = iemNativeEmitJzToFixed(pReNative, off, 0);
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxInstr);
    off = iemNativeEmitJmpToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);
    iemNativeFixupFixedJump(pReNative, offFixup, off /*offTarget*/);
# endif

    iemNativeRegFreeTmp(pReNative, iTmpReg);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code to check if the content of @a idxAddrReg is within the limit of
 * idxSegReg, raising a \#GP(0) if it isn't.
 *
 * @returns New code buffer offset, UINT32_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   idxAddrReg      The host register (32-bit) with the address to
 *                          check.
 * @param   idxSegReg       The segment register (X86_SREG_XXX) to check
 *                          against.
 * @param   idxInstr        The current instruction.
 */
DECLHIDDEN(uint32_t) iemNativeEmitCheckGpr32AgainstSegLimitMaybeRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                         uint8_t idxAddrReg, uint8_t idxSegReg, uint8_t idxInstr)
{
    /*
     * Make sure we don't have any outstanding guest register writes as we may
     * raise an #GP(0) and all guest register must be up to date in CPUMCTX.
     */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /** @todo implement expand down/whatnot checking */
    AssertReturn(idxSegReg == X86_SREG_CS, UINT32_MAX);

    uint8_t const iTmpLimReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off,
                                                               (IEMNATIVEGSTREG)(kIemNativeGstReg_SegLimitFirst + idxSegReg),
                                                               kIemNativeGstRegUse_ForUpdate);
    AssertReturn(iTmpLimReg < RT_ELEMENTS(pReNative->aHstRegs), UINT32_MAX);

    off = iemNativeEmitCmpGpr32WithGpr(pReNative, off, idxAddrReg, iTmpLimReg);

#ifndef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitJaToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);
    RT_NOREF(idxInstr);
#else
    uint32_t const offFixup = off;
    off = iemNativeEmitJbeToFixed(pReNative, off, 0);
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxInstr);
    off = iemNativeEmitJmpToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);
    iemNativeFixupFixedJump(pReNative, offFixup, off /*offTarget*/);
#endif

    iemNativeRegFreeTmp(pReNative, iTmpLimReg);
    return off;
}


/**
 * Emits a call to a CImpl function or something similar.
 */
static int32_t iemNativeEmitCImplCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                      uintptr_t pfnCImpl, uint8_t cbInstr, uint8_t cAddParams,
                                      uint64_t uParam0, uint64_t uParam1, uint64_t uParam2)
{
    iemNativeRegFlushGuestShadows(pReNative, UINT64_MAX); /** @todo optimize this */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 4, false /*fFreeArgVars*/);

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
    off = iemNativeEmitCallImm(pReNative, off, pfnCImpl);

#if defined(RT_ARCH_AMD64) && defined(VBOXSTRICTRC_STRICT_ENABLED) && defined(RT_OS_WINDOWS)
    off = iemNativeEmitLoadGprByBpU32(pReNative, off, X86_GREG_xAX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict (see above) */
#endif

    /*
     * Check the status code.
     */
    return iemNativeEmitCheckCallRetAndPassUp(pReNative, off, idxInstr);
}


/**
 * Emits a call to a threaded worker function.
 */
static uint32_t iemNativeEmitThreadedCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)
{
    iemNativeRegFlushGuestShadows(pReNative, UINT64_MAX); /** @todo optimize this */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 4, false /*fFreeArgVars*/);
    uint8_t const cParams = g_acIemThreadedFunctionUsedArgs[pCallEntry->enmFunction];

#ifdef RT_ARCH_AMD64
    /* Load the parameters and emit the call. */
# ifdef RT_OS_WINDOWS
#  ifndef VBOXSTRICTRC_STRICT_ENABLED
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xCX, IEMNATIVE_REG_FIXED_PVMCPU);
    if (cParams > 0)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xDX, pCallEntry->auParams[0]);
    if (cParams > 1)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x8, pCallEntry->auParams[1]);
    if (cParams > 2)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x9, pCallEntry->auParams[2]);
#  else  /* VBOXSTRICTRC: Returned via hidden parameter. Sigh. */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDX, IEMNATIVE_REG_FIXED_PVMCPU);
    if (cParams > 0)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x8, pCallEntry->auParams[0]);
    if (cParams > 1)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x9, pCallEntry->auParams[1]);
    if (cParams > 2)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x10, pCallEntry->auParams[2]);
    off = iemNativeEmitStoreGprByBp(pReNative, off, IEMNATIVE_FP_OFF_STACK_ARG0, X86_GREG_x10);
    off = iemNativeEmitLeaGrpByBp(pReNative, off, X86_GREG_xCX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict */
#  endif /* VBOXSTRICTRC_STRICT_ENABLED */
# else
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDI, IEMNATIVE_REG_FIXED_PVMCPU);
    if (cParams > 0)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xSI, pCallEntry->auParams[0]);
    if (cParams > 1)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xDX, pCallEntry->auParams[1]);
    if (cParams > 2)
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_xCX, pCallEntry->auParams[2]);
# endif

    off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)g_apfnIemThreadedFunctions[pCallEntry->enmFunction]);

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

    off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)g_apfnIemThreadedFunctions[pCallEntry->enmFunction]);

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
 * Emits the code at the RaiseGP0 label.
 */
static uint32_t iemNativeEmitRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    uint32_t idxLabel = iemNativeFindLabel(pReNative, kIemNativeLabelType_RaiseGp0);
    if (idxLabel != UINT32_MAX)
    {
        Assert(pReNative->paLabels[idxLabel].off == UINT32_MAX);
        pReNative->paLabels[idxLabel].off = off;

        /* iemNativeHlpExecRaiseGp0(PVMCPUCC pVCpu, uint8_t idxInstr) */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
#ifndef IEMNATIVE_WITH_INSTRUCTION_COUNTING
        off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, 0);
#endif
        off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpExecRaiseGp0);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, iemNativeFindLabel(pReNative, kIemNativeLabelType_Return));
    }
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
# ifdef RT_OS_WINDOWS
#  ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_x8,  X86_GREG_xCX); /* cl = instruction number */
#  endif
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xCX, IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDX, X86_GREG_xAX);
# else
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDI, IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xSI, X86_GREG_xAX);
#  ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, X86_GREG_xDX, X86_GREG_xCX); /* cl = instruction number */
#  endif
# endif
# ifndef IEMNATIVE_WITH_INSTRUCTION_COUNTING
        off = iemNativeEmitLoadGpr8Imm(pReNative, off, X86_GREG_xCX, 0);
# endif

#else
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, IEMNATIVE_CALL_RET_GREG);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
        /* IEMNATIVE_CALL_ARG2_GREG is already set. */
#endif

        off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpExecStatusCodeFiddling);
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
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
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_PreIndex,
                                                 ARMV8_A64_REG_X19, ARMV8_A64_REG_X20, ARMV8_A64_REG_SP,
                                                 IEMNATIVE_FRAME_VAR_SIZE / 8);
    /* Restore x21 thru x28 + BP and LR (ret address) (SP remains unchanged in the kSigned variant). */
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X21, ARMV8_A64_REG_X22, ARMV8_A64_REG_SP, 2);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X23, ARMV8_A64_REG_X24, ARMV8_A64_REG_SP, 4);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X25, ARMV8_A64_REG_X26, ARMV8_A64_REG_SP, 6);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X27, ARMV8_A64_REG_X28, ARMV8_A64_REG_SP, 8);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(true /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_BP,  ARMV8_A64_REG_LR,  ARMV8_A64_REG_SP, 10);
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE / 8 == 12);

    /* add sp, sp, IEMNATIVE_FRAME_SAVE_REG_SIZE ;  */
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE < 4096);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, ARMV8_A64_REG_SP, ARMV8_A64_REG_SP,
                                                     IEMNATIVE_FRAME_SAVE_REG_SIZE);

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
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

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
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_PreIndex,
                                                 ARMV8_A64_REG_X19, ARMV8_A64_REG_X20, ARMV8_A64_REG_SP,
                                                 -IEMNATIVE_FRAME_SAVE_REG_SIZE / 8);
    /* Save x21 thru x28 (SP remains unchanged in the kSigned variant). */
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X21, ARMV8_A64_REG_X22, ARMV8_A64_REG_SP, 2);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X23, ARMV8_A64_REG_X24, ARMV8_A64_REG_SP, 4);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X25, ARMV8_A64_REG_X26, ARMV8_A64_REG_SP, 6);
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_X27, ARMV8_A64_REG_X28, ARMV8_A64_REG_SP, 8);
    /* Save the BP and LR (ret address) registers at the top of the frame. */
    pu32CodeBuf[off++] = Armv8A64MkInstrStLdPair(false /*fLoad*/, 2 /*64-bit*/, kArm64InstrStLdPairType_Signed,
                                                 ARMV8_A64_REG_BP,  ARMV8_A64_REG_LR,  ARMV8_A64_REG_SP, 10);
    AssertCompile(IEMNATIVE_FRAME_SAVE_REG_SIZE / 8 == 12);
    /* add bp, sp, IEMNATIVE_FRAME_SAVE_REG_SIZE - 16 ; Set BP to point to the old BP stack address. */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, ARMV8_A64_REG_BP,
                                                     ARMV8_A64_REG_SP, IEMNATIVE_FRAME_SAVE_REG_SIZE - 16);

    /* sub sp, sp, IEMNATIVE_FRAME_VAR_SIZE ;  Allocate the variable area from SP. */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, ARMV8_A64_REG_SP, ARMV8_A64_REG_SP, IEMNATIVE_FRAME_VAR_SIZE);

    /* mov r28, r0  */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_REG_FIXED_PVMCPU, IEMNATIVE_CALL_ARG0_GREG);
    /* mov r27, r1  */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_REG_FIXED_PCPUMCTX, IEMNATIVE_CALL_ARG1_GREG);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}



/*********************************************************************************************************************************
*   Emitters for IEM_MC_XXXX and the associated IEM_MC_XXXX recompiler definitions                                               *
*********************************************************************************************************************************/

#define IEM_MC_BEGIN(a_cArgs, a_cLocals, a_fMcFlags, a_fCImplFlags) \
    {

/** We have to get to the end in recompilation mode, as otherwise we won't
 * generate code for all the IEM_MC_IF_XXX branches. */
#define IEM_MC_END() \
    } return off


/*
 * Standalone CImpl deferals.
 */

#define IEM_MC_DEFER_TO_CIMPL_0_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl) \
    return iemNativeEmitCImplCall0(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr) /** @todo not used ... */


#define IEM_MC_DEFER_TO_CIMPL_1_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl, a0) \
    return iemNativeEmitCImplCall1(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr, a0)

DECLINLINE(uint32_t) iemNativeEmitCImplCall1(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                             uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, pfnCImpl, cbInstr, 1, uArg0, 0, 0);
}


#define IEM_MC_DEFER_TO_CIMPL_2_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl, a0, a1) \
    return iemNativeEmitCImplCall2(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr, a0, a1)

DECLINLINE(uint32_t) iemNativeEmitCImplCall2(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                             uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0, uint64_t uArg1)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, pfnCImpl, cbInstr, 2, uArg0, uArg1, 0);
}


#define IEM_MC_DEFER_TO_CIMPL_3_RET_THREADED(a_cbInstr, a_fFlags, a_pfnCImpl, a0, a1, a2) \
    return iemNativeEmitCImplCall3(pReNative, off, pCallEntry->idxInstr, (uintptr_t)a_pfnCImpl, a_cbInstr, a0, a1, a2)

DECLINLINE(uint32_t) iemNativeEmitCImplCall3(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr,
                                             uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0, uint64_t uArg1, uint64_t uArg2)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, pfnCImpl, cbInstr, 3, uArg0, uArg1, uArg2);
}


/*
 * Advancing PC/RIP/EIP/IP.
 */

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64(a_cbInstr) \
    off = iemNativeEmitAddToRip64AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Same as iemRegAddToRip64AndFinishingNoFlags. */
DECLINLINE(uint32_t) iemNativeEmitAddToRip64AndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);
    AssertReturn(idxPcReg != UINT8_MAX, UINT32_MAX);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGprImm8(pReNative, off, idxPcReg, cbInstr);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32(a_cbInstr) \
    off = iemNativeEmitAddToEip32AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Same as iemRegAddToEip32AndFinishingNoFlags. */
DECLINLINE(uint32_t) iemNativeEmitAddToEip32AndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);
    AssertReturn(idxPcReg != UINT8_MAX, UINT32_MAX);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGpr32Imm8(pReNative, off, idxPcReg, cbInstr);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16(a_cbInstr) \
    off = iemNativeEmitAddToIp16AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Same as iemRegAddToIp16AndFinishingNoFlags. */
DECLINLINE(uint32_t) iemNativeEmitAddToIp16AndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);
    AssertReturn(idxPcReg != UINT8_MAX, UINT32_MAX);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGpr32Imm8(pReNative, off, idxPcReg, cbInstr);
    off = iemNativeEmitClear16UpGpr(pReNative, off, idxPcReg);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


/*
 * Changing PC/RIP/EIP/IP with a relative jump.
 */

#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64(a_i8, a_cbInstr, a_enmEffOpSize) \
    off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), \
                                                            (a_enmEffOpSize), pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)


#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64(a_i16, a_cbInstr) \
    off =  iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), \
                                                             IEMMODE_16BIT, pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64(a_i32, a_cbInstr) \
    off =  iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), \
                                                             IEMMODE_64BIT, pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Same as iemRegRip64RelativeJumpS8AndFinishNoFlags,
 *  iemRegRip64RelativeJumpS16AndFinishNoFlags and
 *  iemRegRip64RelativeJumpS32AndFinishNoFlags. */
DECLINLINE(uint32_t) iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                       uint8_t cbInstr, int32_t offDisp, IEMMODE enmEffOpSize,
                                                                       uint8_t idxInstr)
{
    Assert(enmEffOpSize == IEMMODE_64BIT || enmEffOpSize == IEMMODE_16BIT);

    /* We speculatively modify PC and may raise #GP(0), so make sure the right value is in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);
    AssertReturn(idxPcReg != UINT8_MAX, UINT32_MAX);

    /* Perform the addition. */
    off = iemNativeEmitAddGprImm(pReNative, off, idxPcReg, (int64_t)offDisp + cbInstr);

    if (RT_LIKELY(enmEffOpSize == IEMMODE_64BIT))
    {
        /* Check that the address is canonical, raising #GP(0) + exit TB if it isn't. */
        off = iemNativeEmitCheckGprCanonicalMaybeRaiseGp0(pReNative, off, idxPcReg, idxInstr);
    }
    else
    {
        /* Just truncate the result to 16-bit IP. */
        Assert(enmEffOpSize == IEMMODE_16BIT);
        off = iemNativeEmitClear16UpGpr(pReNative, off, idxPcReg);
    }
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32(a_i8, a_cbInstr, a_enmEffOpSize) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), \
                                                            (a_enmEffOpSize), pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32(a_i16, a_cbInstr) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), \
                                                            IEMMODE_16BIT, pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32(a_i32, a_cbInstr) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), \
                                                            IEMMODE_32BIT, pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Same as iemRegEip32RelativeJumpS8AndFinishNoFlags,
 *  iemRegEip32RelativeJumpS16AndFinishNoFlags and
 *  iemRegEip32RelativeJumpS32AndFinishNoFlags. */
DECLINLINE(uint32_t) iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                       uint8_t cbInstr, int32_t offDisp, IEMMODE enmEffOpSize,
                                                                       uint8_t idxInstr)
{
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);

    /* We speculatively modify PC and may raise #GP(0), so make sure the right value is in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);
    AssertReturn(idxPcReg != UINT8_MAX, UINT32_MAX);

    /* Perform the addition. */
    off = iemNativeEmitAddGpr32Imm(pReNative, off, idxPcReg, offDisp + cbInstr);

    /* Truncate the result to 16-bit IP if the operand size is 16-bit. */
    if (enmEffOpSize == IEMMODE_16BIT)
        off = iemNativeEmitClear16UpGpr(pReNative, off, idxPcReg);

    /* Perform limit checking, potentially raising #GP(0) and exit the TB. */
    off = iemNativeEmitCheckGpr32AgainstSegLimitMaybeRaiseGp0(pReNative, off, idxPcReg, X86_SREG_CS, idxInstr);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16(a_i8, a_cbInstr) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16(a_i16, a_cbInstr) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC16(a_i32, a_cbInstr) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), pCallEntry->idxInstr); \
    AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Same as iemRegIp16RelativeJumpS8AndFinishNoFlags. */
DECLINLINE(uint32_t) iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                      uint8_t cbInstr, int32_t offDisp, uint8_t idxInstr)
{
    /* We speculatively modify PC and may raise #GP(0), so make sure the right value is in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);
    AssertReturn(idxPcReg != UINT8_MAX, UINT32_MAX);

    /* Perform the addition, clamp the result, check limit (may #GP(0) + exit TB) and store the result. */
    off = iemNativeEmitAddGpr32Imm(pReNative, off, idxPcReg, offDisp + cbInstr);
    off = iemNativeEmitClear16UpGpr(pReNative, off, idxPcReg);
    off = iemNativeEmitCheckGpr32AgainstSegLimitMaybeRaiseGp0(pReNative, off, idxPcReg, X86_SREG_CS, idxInstr);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


/*
 * Conditionals.
 */

/**
 * Pushes an IEM_MC_IF_XXX onto the condition stack.
 *
 * @returns Pointer to the condition stack entry on success, NULL on failure
 *          (too many nestings)
 */
DECLINLINE(PIEMNATIVECOND) iemNativeCondPushIf(PIEMRECOMPILERSTATE pReNative)
{
    uint32_t const idxStack = pReNative->cCondDepth;
    AssertReturn(idxStack < RT_ELEMENTS(pReNative->aCondStack), NULL);

    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[idxStack];
    pReNative->cCondDepth = (uint8_t)(idxStack + 1);

    uint16_t const uCondSeqNo = ++pReNative->uCondSeqNo;
    pEntry->fInElse       = false;
    pEntry->idxLabelElse  = iemNativeMakeLabel(pReNative, kIemNativeLabelType_Else, UINT32_MAX /*offWhere*/, uCondSeqNo);
    AssertReturn(pEntry->idxLabelElse != UINT32_MAX, NULL);
    pEntry->idxLabelEndIf = iemNativeMakeLabel(pReNative, kIemNativeLabelType_Endif, UINT32_MAX /*offWhere*/, uCondSeqNo);
    AssertReturn(pEntry->idxLabelEndIf != UINT32_MAX, NULL);

    return pEntry;
}


#define IEM_MC_ELSE() } while (0); \
        off = iemNativeEmitElse(pReNative, off); \
        AssertReturn(off != UINT32_MAX, UINT32_MAX); \
        do {

/** Emits code related to IEM_MC_ELSE. */
DECLINLINE(uint32_t) iemNativeEmitElse(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    /* Check sanity and get the conditional stack entry. */
    Assert(off != UINT32_MAX);
    Assert(pReNative->cCondDepth > 0 && pReNative->cCondDepth <= RT_ELEMENTS(pReNative->aCondStack));
    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[pReNative->cCondDepth - 1];
    Assert(!pEntry->fInElse);

    /* Jump to the endif */
    off = iemNativeEmitJmpToLabel(pReNative, off, pEntry->idxLabelEndIf);

    /* Define the else label and enter the else part of the condition. */
    pReNative->paLabels[pEntry->idxLabelElse].off = off;
    pEntry->fInElse = true;

    return off;
}


#define IEM_MC_ENDIF() } while (0); \
        off = iemNativeEmitEndIf(pReNative, off); \
        AssertReturn(off != UINT32_MAX, UINT32_MAX)

/** Emits code related to IEM_MC_ENDIF. */
DECLINLINE(uint32_t) iemNativeEmitEndIf(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    /* Check sanity and get the conditional stack entry. */
    Assert(off != UINT32_MAX);
    Assert(pReNative->cCondDepth > 0 && pReNative->cCondDepth <= RT_ELEMENTS(pReNative->aCondStack));
    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[pReNative->cCondDepth - 1];

    /* Define the endif label and maybe the else one if we're still in the 'if' part. */
    if (!pEntry->fInElse)
        pReNative->paLabels[pEntry->idxLabelElse].off = off;
    else
        Assert(pReNative->paLabels[pEntry->idxLabelElse].off <= off);
    pReNative->paLabels[pEntry->idxLabelEndIf].off    = off;

    /* Pop the conditional stack.*/
    pReNative->cCondDepth -= 1;

    return off;
}


#define IEM_MC_IF_EFL_BIT_SET(a_fBit) \
        off = iemNativeEmitIfEflagsBitSet(pReNative, off, (a_fBit)); \
        AssertReturn(off != UINT32_MAX, UINT32_MAX); \
        do {

/** Emits code for IEM_MC_IF_EFL_BIT_SET. */
DECLINLINE(uint32_t) iemNativeEmitIfEflagsBitSet(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitInEfl)
{
    PIEMNATIVECOND pEntry = iemNativeCondPushIf(pReNative);
    AssertReturn(pEntry, UINT32_MAX);

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);
    AssertReturn(idxEflReg != UINT8_MAX, UINT32_MAX);

    unsigned const iBitNo = ASMBitFirstSetU32(fBitInEfl) - 1;
    Assert(RT_BIT_32(iBitNo) == fBitInEfl);

    /* Test and jump. */
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfSet(pReNative, off, idxEflReg, iBitNo, pEntry->idxLabelElse);

    /* Free but don't flush the EFlags register. */
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    return off;
}



/*********************************************************************************************************************************
*   Builtin functions                                                                                                            *
*********************************************************************************************************************************/

/**
 * Built-in function that calls a C-implemention function taking zero arguments.
 */
static IEM_DECL_IEMNATIVERECOMPFUNC_DEF(iemNativeRecompFunc_BltIn_DeferToCImpl0)
{
    PFNIEMCIMPL0 const pfnCImpl = (PFNIEMCIMPL0)(uintptr_t)pCallEntry->auParams[0];
    uint8_t const      cbInstr  = (uint8_t)pCallEntry->auParams[1];
    return iemNativeEmitCImplCall(pReNative, off, pCallEntry->idxInstr, (uintptr_t)pfnCImpl, cbInstr, 0, 0, 0, 0);
}



/*********************************************************************************************************************************
*   The native code generator functions for each MC block.                                                                       *
*********************************************************************************************************************************/


/*
 * Include g_apfnIemNativeRecompileFunctions and associated functions.
 *
 * This should probably live in it's own file later, but lets see what the
 * compile times turn out to be first.
 */
#include "IEMNativeFunctions.cpp.h"



/*********************************************************************************************************************************
*   Recompiler Core.                                                                                                             *
*********************************************************************************************************************************/


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
#ifdef VBOX_STRICT
        off = iemNativeEmitMarker(pReNative, off, RT_MAKE_U32(pTb->Thrd.cCalls - cCallsLeft - 1, pCallEntry->enmFunction));
        AssertReturn(off != UINT32_MAX, pTb);
#endif
        PFNIEMNATIVERECOMPFUNC const pfnRecom = g_apfnIemNativeRecompileFunctions[pCallEntry->enmFunction];
        if (pfnRecom) /** @todo stats on this.   */
        {
            //STAM_COUNTER_INC()
            off = pfnRecom(pReNative, off, pCallEntry);
        }
        else
            off = iemNativeEmitThreadedCall(pReNative, off, pCallEntry);
        AssertReturn(off != UINT32_MAX, pTb);
        Assert(pReNative->cCondDepth == 0);

        pCallEntry++;
    }

    /*
     * Emit the epilog code.
     */
    off = iemNativeEmitEpilog(pReNative, off);
    AssertReturn(off != UINT32_MAX, pTb);

    /*
     * Generate special jump labels.
     */
    if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_RaiseGp0))
    {
        off = iemNativeEmitRaiseGp0(pReNative, off);
        AssertReturn(off != UINT32_MAX, pTb);
    }

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
#ifdef LOG_ENABLED
    if (LogIs3Enabled())
    {

    }
#endif

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

