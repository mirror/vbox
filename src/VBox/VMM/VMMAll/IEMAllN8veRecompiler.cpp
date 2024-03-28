/* $Id$ */
/** @file
 * IEM - Native Recompiler
 *
 * Logging group IEM_RE_NATIVE assignments:
 *      - Level 1  (Log)  : ...
 *      - Flow  (LogFlow) : ...
 *      - Level 2  (Log2) : Details calls as they're recompiled.
 *      - Level 3  (Log3) : Disassemble native code after recompiling.
 *      - Level 4  (Log4) : ...
 *      - Level 5  (Log5) : ...
 *      - Level 6  (Log6) : ...
 *      - Level 7  (Log7) : ...
 *      - Level 8  (Log8) : ...
 *      - Level 9  (Log9) : ...
 *      - Level 10 (Log10): ...
 *      - Level 11 (Log11): Variable allocator.
 *      - Level 12 (Log12): Register allocator.
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
#include <VBox/vmm/dbgf.h>
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/dis.h>
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
#ifdef VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER
# include "/opt/local/include/capstone/capstone.h"
#endif

#include "IEMInline.h"
#include "IEMThreadedFunctions.h"
#include "IEMN8veRecompiler.h"
#include "IEMN8veRecompilerEmit.h"
#include "IEMN8veRecompilerTlbLookup.h"
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

/** @todo eliminate this clang build hack. */
#if RT_CLANG_PREREQ(4, 0)
# pragma GCC diagnostic ignored "-Wunused-function"
#endif


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
static void iemNativeDbgInfoAddLabel(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType, uint16_t uData);
#endif
DECL_FORCE_INLINE(void) iemNativeRegClearGstRegShadowing(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, uint32_t off);
DECL_FORCE_INLINE(void) iemNativeRegClearGstRegShadowingOne(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg,
                                                            IEMNATIVEGSTREG enmGstReg, uint32_t off);
DECL_INLINE_THROW(void) iemNativeVarRegisterRelease(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar);


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

    for (unsigned iIteration = 0;; iIteration++)
    {
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

        /*
         * Try prune native TBs once.
         */
        if (iIteration == 0)
            iemTbAllocatorFreeupNativeSpace(pVCpu, cbReq / sizeof(IEMNATIVEINSTR));
        else
        {
            STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatNativeExecMemInstrBufAllocFailed);
            return NULL;
        }
    }

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
            uint32_t const idxFirst  = (uint32_t)offChunk >> IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;
            uint32_t const cReqUnits = (uint32_t)cb       >> IEMEXECMEM_ALT_SUB_ALLOC_UNIT_SHIFT;

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
    AssertReturn(fRet, VERR_INTERNAL_ERROR_3); /* Nothing to clean up on failure, since its within the chunk itself. */

    return VINF_SUCCESS;
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
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecStatusCodeFiddling,(PVMCPUCC pVCpu, int rc, uint8_t idxInstr))
{
    pVCpu->iem.s.cInstructions += idxInstr;
    return VBOXSTRICTRC_VAL(iemExecStatusCodeFiddling(pVCpu, rc == VINF_IEM_REEXEC_BREAK ? VINF_SUCCESS : rc));
}


/**
 * Used by TB code when it wants to raise a \#DE.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseDe,(PVMCPUCC pVCpu))
{
    iemRaiseDivideErrorJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise a \#UD.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseUd,(PVMCPUCC pVCpu))
{
    iemRaiseUndefinedOpcodeJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise an SSE related \#UD or \#NM.
 *
 * See IEM_MC_MAYBE_RAISE_SSE_RELATED_XCPT.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseSseRelated,(PVMCPUCC pVCpu))
{
    if (   (pVCpu->cpum.GstCtx.cr0 & X86_CR0_EM)
        || !(pVCpu->cpum.GstCtx.cr4 & X86_CR4_OSFXSR))
        iemRaiseUndefinedOpcodeJmp(pVCpu);
    else
        iemRaiseDeviceNotAvailableJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise an AVX related \#UD or \#NM.
 *
 * See IEM_MC_MAYBE_RAISE_AVX_RELATED_XCPT.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseAvxRelated,(PVMCPUCC pVCpu))
{
    if (   (pVCpu->cpum.GstCtx.aXcr[0] & (XSAVE_C_YMM | XSAVE_C_SSE)) != (XSAVE_C_YMM | XSAVE_C_SSE)
        || !(pVCpu->cpum.GstCtx.cr4 & X86_CR4_OSXSAVE))
        iemRaiseUndefinedOpcodeJmp(pVCpu);
    else
        iemRaiseDeviceNotAvailableJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise an SSE/AVX floating point exception related \#UD or \#XF.
 *
 * See IEM_MC_MAYBE_RAISE_SSE_AVX_SIMD_FP_OR_UD_XCPT.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseSseAvxFpRelated,(PVMCPUCC pVCpu))
{
    if (pVCpu->cpum.GstCtx.cr4 & X86_CR4_OSXMMEEXCPT)
        iemRaiseSimdFpExceptionJmp(pVCpu);
    else
        iemRaiseUndefinedOpcodeJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise a \#NM.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseNm,(PVMCPUCC pVCpu))
{
    iemRaiseDeviceNotAvailableJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise a \#GP(0).
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseGp0,(PVMCPUCC pVCpu))
{
    iemRaiseGeneralProtectionFault0Jmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise a \#MF.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseMf,(PVMCPUCC pVCpu))
{
    iemRaiseMathFaultJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when it wants to raise a \#XF.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpExecRaiseXf,(PVMCPUCC pVCpu))
{
    iemRaiseSimdFpExceptionJmp(pVCpu);
#ifndef _MSC_VER
    return VINF_IEM_RAISED_XCPT; /* not reached */
#endif
}


/**
 * Used by TB code when detecting opcode changes.
 * @see iemThreadeFuncWorkerObsoleteTb
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpObsoleteTb,(PVMCPUCC pVCpu))
{
    /* We set fSafeToFree to false where as we're being called in the context
       of a TB callback function, which for native TBs means we cannot release
       the executable memory till we've returned our way back to iemTbExec as
       that return path codes via the native code generated for the TB. */
    Log7(("TB obsolete: %p at %04x:%08RX64\n", pVCpu->iem.s.pCurTbR3, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip));
    iemThreadedTbObsolete(pVCpu, pVCpu->iem.s.pCurTbR3, false /*fSafeToFree*/);
    return VINF_IEM_REEXEC_BREAK;
}


/**
 * Used by TB code when we need to switch to a TB with CS.LIM checking.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpNeedCsLimChecking,(PVMCPUCC pVCpu))
{
    Log7(("TB need CS.LIM: %p at %04x:%08RX64; offFromLim=%#RX64 CS.LIM=%#RX32 CS.BASE=%#RX64\n",
          pVCpu->iem.s.pCurTbR3, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
          (int64_t)pVCpu->cpum.GstCtx.cs.u32Limit - (int64_t)pVCpu->cpum.GstCtx.rip,
          pVCpu->cpum.GstCtx.cs.u32Limit, pVCpu->cpum.GstCtx.cs.u64Base));
    STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatCheckNeedCsLimChecking);
    return VINF_IEM_REEXEC_BREAK;
}


/**
 * Used by TB code when we missed a PC check after a branch.
 */
IEM_DECL_NATIVE_HLP_DEF(int, iemNativeHlpCheckBranchMiss,(PVMCPUCC pVCpu))
{
    Log7(("TB jmp miss: %p at %04x:%08RX64; GCPhysWithOffset=%RGp, pbInstrBuf=%p\n",
          pVCpu->iem.s.pCurTbR3, pVCpu->cpum.GstCtx.cs.Sel, pVCpu->cpum.GstCtx.rip,
          pVCpu->iem.s.GCPhysInstrBuf + pVCpu->cpum.GstCtx.rip + pVCpu->cpum.GstCtx.cs.u64Base - pVCpu->iem.s.uInstrBufPc,
          pVCpu->iem.s.pbInstrBuf));
    STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatCheckBranchMisses);
    return VINF_IEM_REEXEC_BREAK;
}



/*********************************************************************************************************************************
*   Helpers: Segmented memory fetches and stores.                                                                                *
*********************************************************************************************************************************/

/**
 * Used by TB code to load unsigned 8-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU8,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)iemMemFetchDataU8SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)iemMemFetchDataU8Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 8-bit data w/ segmentation, sign extending it
 * to 16 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU8_Sx_U16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(uint16_t)(int16_t)(int8_t)iemMemFetchDataU8SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)(uint16_t)(int16_t)(int8_t)iemMemFetchDataU8Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 8-bit data w/ segmentation, sign extending it
 * to 32 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU8_Sx_U32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(uint32_t)(int32_t)(int8_t)iemMemFetchDataU8SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)(uint32_t)(int32_t)(int8_t)iemMemFetchDataU8Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}

/**
 * Used by TB code to load signed 8-bit data w/ segmentation, sign extending it
 * to 64 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU8_Sx_U64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(int64_t)(int8_t)iemMemFetchDataU8SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)(int64_t)(int8_t)iemMemFetchDataU8Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 16-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)iemMemFetchDataU16SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)iemMemFetchDataU16Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 16-bit data w/ segmentation, sign extending it
 * to 32 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU16_Sx_U32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(uint32_t)(int32_t)(int16_t)iemMemFetchDataU16SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)(uint32_t)(int32_t)(int16_t)iemMemFetchDataU16Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 16-bit data w/ segmentation, sign extending it
 * to 64 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU16_Sx_U64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(int64_t)(int16_t)iemMemFetchDataU16SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)(int64_t)(int16_t)iemMemFetchDataU16Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 32-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)iemMemFetchDataU32SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)iemMemFetchDataU32Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 32-bit data w/ segmentation, sign extending it
 * to 64 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU32_Sx_U64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(int64_t)(int32_t)iemMemFetchDataU32SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return (uint64_t)(int64_t)(int32_t)iemMemFetchDataU32Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 64-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFetchDataU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU64SafeJmp(pVCpu, iSegReg, GCPtrMem);
#else
    return iemMemFetchDataU64Jmp(pVCpu, iSegReg, GCPtrMem);
#endif
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Used by TB code to load 128-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFetchDataU128,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PRTUINT128U pu128Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    iemMemFetchDataU128SafeJmp(pVCpu, pu128Dst, iSegReg, GCPtrMem);
#else
    iemMemFetchDataU128Jmp(pVCpu, pu128Dst, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load 128-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFetchDataU128AlignedSse,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PRTUINT128U pu128Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    iemMemFetchDataU128AlignedSseSafeJmp(pVCpu, pu128Dst, iSegReg, GCPtrMem);
#else
    iemMemFetchDataU128AlignedSseJmp(pVCpu, pu128Dst, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load 128-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFetchDataU128NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PRTUINT128U pu128Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    iemMemFetchDataU128NoAcSafeJmp(pVCpu, pu128Dst, iSegReg, GCPtrMem);
#else
    iemMemFetchDataU128NoAcJmp(pVCpu, pu128Dst, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load 256-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFetchDataU256NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PRTUINT256U pu256Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    iemMemFetchDataU256NoAcSafeJmp(pVCpu, pu256Dst, iSegReg, GCPtrMem);
#else
    iemMemFetchDataU256NoAcJmp(pVCpu, pu256Dst, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to load 256-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFetchDataU256AlignedAvx,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PRTUINT256U pu256Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    iemMemFetchDataU256AlignedAvxSafeJmp(pVCpu, pu256Dst, iSegReg, GCPtrMem);
#else
    iemMemFetchDataU256AlignedAvxJmp(pVCpu, pu256Dst, iSegReg, GCPtrMem);
#endif
}
#endif


/**
 * Used by TB code to store unsigned 8-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU8,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, uint8_t u8Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU8SafeJmp(pVCpu, iSegReg, GCPtrMem, u8Value);
#else
    iemMemStoreDataU8Jmp(pVCpu, iSegReg, GCPtrMem, u8Value);
#endif
}


/**
 * Used by TB code to store unsigned 16-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, uint16_t u16Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU16SafeJmp(pVCpu, iSegReg, GCPtrMem, u16Value);
#else
    iemMemStoreDataU16Jmp(pVCpu, iSegReg, GCPtrMem, u16Value);
#endif
}


/**
 * Used by TB code to store unsigned 32-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, uint32_t u32Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU32SafeJmp(pVCpu, iSegReg, GCPtrMem, u32Value);
#else
    iemMemStoreDataU32Jmp(pVCpu, iSegReg, GCPtrMem, u32Value);
#endif
}


/**
 * Used by TB code to store unsigned 64-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, uint64_t u64Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU64SafeJmp(pVCpu, iSegReg, GCPtrMem, u64Value);
#else
    iemMemStoreDataU64Jmp(pVCpu, iSegReg, GCPtrMem, u64Value);
#endif
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Used by TB code to store unsigned 128-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU128AlignedSse,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PCRTUINT128U pu128Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU128AlignedSseSafeJmp(pVCpu, iSegReg, GCPtrMem, pu128Src);
#else
    iemMemStoreDataU128AlignedSseJmp(pVCpu, iSegReg, GCPtrMem, pu128Src);
#endif
}


/**
 * Used by TB code to store unsigned 128-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU128NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PCRTUINT128U pu128Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU128NoAcSafeJmp(pVCpu, iSegReg, GCPtrMem, pu128Src);
#else
    iemMemStoreDataU128NoAcJmp(pVCpu, iSegReg, GCPtrMem, pu128Src);
#endif
}


/**
 * Used by TB code to store unsigned 256-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU256NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PCRTUINT256U pu256Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU256NoAcSafeJmp(pVCpu, iSegReg, GCPtrMem, pu256Src);
#else
    iemMemStoreDataU256NoAcJmp(pVCpu, iSegReg, GCPtrMem, pu256Src);
#endif
}


/**
 * Used by TB code to store unsigned 256-bit data w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemStoreDataU256AlignedAvx,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t iSegReg, PCRTUINT256U pu256Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU256AlignedAvxSafeJmp(pVCpu, iSegReg, GCPtrMem, pu256Src);
#else
    iemMemStoreDataU256AlignedAvxcJmp(pVCpu, iSegReg, GCPtrMem, pu256Src);
#endif
}
#endif



/**
 * Used by TB code to store an unsigned 16-bit value onto a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackStoreU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint16_t u16Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU16SafeJmp(pVCpu, GCPtrMem, u16Value);
#else
    iemMemStoreStackU16Jmp(pVCpu, GCPtrMem, u16Value);
#endif
}


/**
 * Used by TB code to store an unsigned 32-bit value onto a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackStoreU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t u32Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU32SafeJmp(pVCpu, GCPtrMem, u32Value);
#else
    iemMemStoreStackU32Jmp(pVCpu, GCPtrMem, u32Value);
#endif
}


/**
 * Used by TB code to store an 32-bit selector value onto a generic stack.
 *
 * Intel CPUs doesn't do write a whole dword, thus the special function.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackStoreU32SReg,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t u32Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU32SRegSafeJmp(pVCpu, GCPtrMem, u32Value);
#else
    iemMemStoreStackU32SRegJmp(pVCpu, GCPtrMem, u32Value);
#endif
}


/**
 * Used by TB code to push unsigned 64-bit value onto a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackStoreU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint64_t u64Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU64SafeJmp(pVCpu, GCPtrMem, u64Value);
#else
    iemMemStoreStackU64Jmp(pVCpu, GCPtrMem, u64Value);
#endif
}


/**
 * Used by TB code to fetch an unsigned 16-bit item off a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t, iemNativeHlpStackFetchU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_POP
    return iemMemFetchStackU16SafeJmp(pVCpu, GCPtrMem);
#else
    return iemMemFetchStackU16Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to fetch an unsigned 32-bit item off a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t, iemNativeHlpStackFetchU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_POP
    return iemMemFetchStackU32SafeJmp(pVCpu, GCPtrMem);
#else
    return iemMemFetchStackU32Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to fetch an unsigned 64-bit item off a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpStackFetchU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_POP
    return iemMemFetchStackU64SafeJmp(pVCpu, GCPtrMem);
#else
    return iemMemFetchStackU64Jmp(pVCpu, GCPtrMem);
#endif
}



/*********************************************************************************************************************************
*   Helpers: Flat memory fetches and stores.                                                                                     *
*********************************************************************************************************************************/

/**
 * Used by TB code to load unsigned 8-bit data w/ flat address.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU8,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)iemMemFetchDataU8SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)iemMemFlatFetchDataU8Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 8-bit data w/ flat address, sign extending it
 * to 16 bits.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU8_Sx_U16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(uint16_t)(int16_t)(int8_t)iemMemFetchDataU8SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)(uint16_t)(int16_t)(int8_t)iemMemFlatFetchDataU8Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 8-bit data w/ flat address, sign extending it
 * to 32 bits.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU8_Sx_U32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(uint32_t)(int32_t)(int8_t)iemMemFetchDataU8SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)(uint32_t)(int32_t)(int8_t)iemMemFlatFetchDataU8Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 8-bit data w/ flat address, sign extending it
 * to 64 bits.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU8_Sx_U64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(int64_t)(int8_t)iemMemFetchDataU8SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)(int64_t)(int8_t)iemMemFlatFetchDataU8Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 16-bit data w/ flat address.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)iemMemFetchDataU16SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)iemMemFlatFetchDataU16Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 16-bit data w/ flat address, sign extending it
 * to 32 bits.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU16_Sx_U32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(uint32_t)(int32_t)(int16_t)iemMemFetchDataU16SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)(uint32_t)(int32_t)(int16_t)iemMemFlatFetchDataU16Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 16-bit data w/ flat address, sign extending it
 * to 64 bits.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU16_Sx_U64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(int64_t)(int16_t)iemMemFetchDataU16SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)(int64_t)(int16_t)iemMemFlatFetchDataU16Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 32-bit data w/ flat address.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)iemMemFetchDataU32SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)iemMemFlatFetchDataU32Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load signed 32-bit data w/ flat address, sign extending it
 * to 64 bits.
 * @note Zero extending the value to 64-bit to simplify assembly.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU32_Sx_U64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return (uint64_t)(int64_t)(int32_t)iemMemFetchDataU32SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return (uint64_t)(int64_t)(int32_t)iemMemFlatFetchDataU32Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 64-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpMemFlatFetchDataU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU64SafeJmp(pVCpu, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatFetchDataU64Jmp(pVCpu, GCPtrMem);
#endif
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Used by TB code to load unsigned 128-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatFetchDataU128,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PRTUINT128U pu128Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU128SafeJmp(pVCpu, pu128Dst, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatFetchDataU128Jmp(pVCpu, pu128Dst, UINT8_MAX, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 128-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatFetchDataU128AlignedSse,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PRTUINT128U pu128Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU128AlignedSseSafeJmp(pVCpu, pu128Dst, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatFetchDataU128AlignedSseJmp(pVCpu, pu128Dst, UINT8_MAX, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 128-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatFetchDataU128NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PRTUINT128U pu128Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU128NoAcSafeJmp(pVCpu, pu128Dst, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatFetchDataU128NoAcJmp(pVCpu, pu128Dst, UINT8_MAX, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 256-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatFetchDataU256NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PRTUINT256U pu256Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU256NoAcSafeJmp(pVCpu, pu256Dst, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatFetchDataU256NoAcJmp(pVCpu, pu256Dst, UINT8_MAX, GCPtrMem);
#endif
}


/**
 * Used by TB code to load unsigned 256-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatFetchDataU256AlignedAvx,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PRTUINT256U pu256Dst))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_FETCH
    return iemMemFetchDataU256AlignedAvxSafeJmp(pVCpu, pu256Dst, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatFetchDataU256AlignedAvxJmp(pVCpu, pu256Dst, UINT8_MAX, GCPtrMem);
#endif
}
#endif


/**
 * Used by TB code to store unsigned 8-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU8,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint8_t u8Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU8SafeJmp(pVCpu, UINT8_MAX, GCPtrMem, u8Value);
#else
    iemMemFlatStoreDataU8Jmp(pVCpu, GCPtrMem, u8Value);
#endif
}


/**
 * Used by TB code to store unsigned 16-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint16_t u16Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU16SafeJmp(pVCpu, UINT8_MAX, GCPtrMem, u16Value);
#else
    iemMemFlatStoreDataU16Jmp(pVCpu, GCPtrMem, u16Value);
#endif
}


/**
 * Used by TB code to store unsigned 32-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t u32Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU32SafeJmp(pVCpu, UINT8_MAX, GCPtrMem, u32Value);
#else
    iemMemFlatStoreDataU32Jmp(pVCpu, GCPtrMem, u32Value);
#endif
}


/**
 * Used by TB code to store unsigned 64-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint64_t u64Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU64SafeJmp(pVCpu, UINT8_MAX, GCPtrMem, u64Value);
#else
    iemMemFlatStoreDataU64Jmp(pVCpu, GCPtrMem, u64Value);
#endif
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Used by TB code to store unsigned 128-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU128AlignedSse,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PCRTUINT128U pu128Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU128AlignedSseSafeJmp(pVCpu, UINT8_MAX, GCPtrMem, pu128Src);
#else
    iemMemFlatStoreDataU128AlignedSseJmp(pVCpu, GCPtrMem, pu128Src);
#endif
}


/**
 * Used by TB code to store unsigned 128-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU128NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PCRTUINT128U pu128Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU128NoAcSafeJmp(pVCpu, UINT8_MAX, GCPtrMem, pu128Src);
#else
    iemMemFlatStoreDataU128NoAcJmp(pVCpu, GCPtrMem, pu128Src);
#endif
}


/**
 * Used by TB code to store unsigned 256-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU256NoAc,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PCRTUINT256U pu256Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU256NoAcSafeJmp(pVCpu, UINT8_MAX, GCPtrMem, pu256Src);
#else
    iemMemFlatStoreDataU256NoAcJmp(pVCpu, GCPtrMem, pu256Src);
#endif
}


/**
 * Used by TB code to store unsigned 256-bit data w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemFlatStoreDataU256AlignedAvx,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, PCRTUINT256U pu256Src))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_STORE
    iemMemStoreDataU256AlignedAvxSafeJmp(pVCpu, UINT8_MAX, GCPtrMem, pu256Src);
#else
    iemMemFlatStoreDataU256AlignedAvxJmp(pVCpu, GCPtrMem, pu256Src);
#endif
}
#endif



/**
 * Used by TB code to store an unsigned 16-bit value onto a flat stack.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackFlatStoreU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint16_t u16Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU16SafeJmp(pVCpu, GCPtrMem, u16Value);
#else
    iemMemFlatStoreStackU16Jmp(pVCpu, GCPtrMem, u16Value);
#endif
}


/**
 * Used by TB code to store an unsigned 32-bit value onto a flat stack.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackFlatStoreU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t u32Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU32SafeJmp(pVCpu, GCPtrMem, u32Value);
#else
    iemMemFlatStoreStackU32Jmp(pVCpu, GCPtrMem, u32Value);
#endif
}


/**
 * Used by TB code to store a segment selector value onto a flat stack.
 *
 * Intel CPUs doesn't do write a whole dword, thus the special function.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackFlatStoreU32SReg,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint32_t u32Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU32SRegSafeJmp(pVCpu, GCPtrMem, u32Value);
#else
    iemMemFlatStoreStackU32SRegJmp(pVCpu, GCPtrMem, u32Value);
#endif
}


/**
 * Used by TB code to store an unsigned 64-bit value onto a flat stack.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpStackFlatStoreU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem, uint64_t u64Value))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_PUSH
    iemMemStoreStackU64SafeJmp(pVCpu, GCPtrMem, u64Value);
#else
    iemMemFlatStoreStackU64Jmp(pVCpu, GCPtrMem, u64Value);
#endif
}


/**
 * Used by TB code to fetch an unsigned 16-bit item off a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t, iemNativeHlpStackFlatFetchU16,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_POP
    return iemMemFetchStackU16SafeJmp(pVCpu, GCPtrMem);
#else
    return iemMemFlatFetchStackU16Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to fetch an unsigned 32-bit item off a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t, iemNativeHlpStackFlatFetchU32,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_POP
    return iemMemFetchStackU32SafeJmp(pVCpu, GCPtrMem);
#else
    return iemMemFlatFetchStackU32Jmp(pVCpu, GCPtrMem);
#endif
}


/**
 * Used by TB code to fetch an unsigned 64-bit item off a generic stack.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t, iemNativeHlpStackFlatFetchU64,(PVMCPUCC pVCpu, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_POP
    return iemMemFetchStackU64SafeJmp(pVCpu, GCPtrMem);
#else
    return iemMemFlatFetchStackU64Jmp(pVCpu, GCPtrMem);
#endif
}



/*********************************************************************************************************************************
*   Helpers: Segmented memory mapping.                                                                                           *
*********************************************************************************************************************************/

/**
 * Used by TB code to map unsigned 8-bit data for atomic read-write w/
 * segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t *, iemNativeHlpMemMapDataU8Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                   RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8AtSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU8AtJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 8-bit data read-write w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t *, iemNativeHlpMemMapDataU8Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                               RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8RwSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU8RwJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 8-bit data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t *, iemNativeHlpMemMapDataU8Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                               RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU8WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 8-bit data readonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t const *, iemNativeHlpMemMapDataU8Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                     RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8RoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU8RoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data for atomic read-write w/
 * segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t *, iemNativeHlpMemMapDataU16Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                     RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16AtSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU16AtJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data read-write w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t *, iemNativeHlpMemMapDataU16Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                 RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16RwSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU16RwJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t *, iemNativeHlpMemMapDataU16Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                 RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU16WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data readonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t const *, iemNativeHlpMemMapDataU16Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                       RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16RoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU16RoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data for atomic read-write w/
 * segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t *, iemNativeHlpMemMapDataU32Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                     RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32AtSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU32AtJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data read-write w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t *, iemNativeHlpMemMapDataU32Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                 RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32RwSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU32RwJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t *, iemNativeHlpMemMapDataU32Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                 RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU32WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data readonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t const *, iemNativeHlpMemMapDataU32Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                       RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32RoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU32RoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data for atomic read-write w/
 * segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t *, iemNativeHlpMemMapDataU64Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                     RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64AtSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU64AtJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data read-write w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t *, iemNativeHlpMemMapDataU64Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                 RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64RwSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU64RwJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t *, iemNativeHlpMemMapDataU64Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                 RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU64WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data readonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t const *, iemNativeHlpMemMapDataU64Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                       RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64RoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU64RoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map 80-bit float data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(RTFLOAT80U *, iemNativeHlpMemMapDataR80Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                   RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataR80WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataR80WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map 80-bit BCD data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(RTPBCD80U *, iemNativeHlpMemMapDataD80Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                  RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataD80WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataD80WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data for atomic read-write w/
 * segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U *, iemNativeHlpMemMapDataU128Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                        RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128AtSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU128AtJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data read-write w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U *, iemNativeHlpMemMapDataU128Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                    RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128RwSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU128RwJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data writeonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U *, iemNativeHlpMemMapDataU128Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                    RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128WoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU128WoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data readonly w/ segmentation.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U const *, iemNativeHlpMemMapDataU128Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo,
                                                                          RTGCPTR GCPtrMem, uint8_t iSegReg))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128RoSafeJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#else
    return iemMemMapDataU128RoJmp(pVCpu, pbUnmapInfo, iSegReg, GCPtrMem);
#endif
}


/*********************************************************************************************************************************
*   Helpers: Flat memory mapping.                                                                                                *
*********************************************************************************************************************************/

/**
 * Used by TB code to map unsigned 8-bit data for atomic read-write w/ flat
 * address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t *, iemNativeHlpMemFlatMapDataU8Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8AtSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU8AtJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 8-bit data read-write w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t *, iemNativeHlpMemFlatMapDataU8Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8RwSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU8RwJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 8-bit data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t *, iemNativeHlpMemFlatMapDataU8Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU8WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 8-bit data readonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint8_t const *, iemNativeHlpMemFlatMapDataU8Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU8RoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU8RoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data for atomic read-write w/ flat
 * address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t *, iemNativeHlpMemFlatMapDataU16Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16AtSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU16AtJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data read-write w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t *, iemNativeHlpMemFlatMapDataU16Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16RwSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU16RwJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t *, iemNativeHlpMemFlatMapDataU16Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU16WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 16-bit data readonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint16_t const *, iemNativeHlpMemFlatMapDataU16Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU16RoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU16RoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data for atomic read-write w/ flat
 * address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t *, iemNativeHlpMemFlatMapDataU32Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32AtSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU32AtJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data read-write w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t *, iemNativeHlpMemFlatMapDataU32Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32RwSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU32RwJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t *, iemNativeHlpMemFlatMapDataU32Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU32WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 32-bit data readonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint32_t const *, iemNativeHlpMemFlatMapDataU32Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU32RoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU32RoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data for atomic read-write w/ flat
 * address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t *, iemNativeHlpMemFlatMapDataU64Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64AtSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU64AtJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data read-write w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t *, iemNativeHlpMemFlatMapDataU64Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64RwSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU64RwJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t *, iemNativeHlpMemFlatMapDataU64Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU64WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 64-bit data readonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(uint64_t const *, iemNativeHlpMemFlatMapDataU64Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU64RoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU64RoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map 80-bit float data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(RTFLOAT80U *, iemNativeHlpMemFlatMapDataR80Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataR80WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataR80WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map 80-bit BCD data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(RTPBCD80U *, iemNativeHlpMemFlatMapDataD80Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataD80WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataD80WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data for atomic read-write w/ flat
 * address.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U *, iemNativeHlpMemFlatMapDataU128Atomic,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128AtSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU128AtJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data read-write w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U *, iemNativeHlpMemFlatMapDataU128Rw,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128RwSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU128RwJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data writeonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U *, iemNativeHlpMemFlatMapDataU128Wo,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128WoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU128WoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/**
 * Used by TB code to map unsigned 128-bit data readonly w/ flat address.
 */
IEM_DECL_NATIVE_HLP_DEF(RTUINT128U const *, iemNativeHlpMemFlatMapDataU128Ro,(PVMCPUCC pVCpu, uint8_t *pbUnmapInfo, RTGCPTR GCPtrMem))
{
#ifdef IEMNATIVE_WITH_TLB_LOOKUP_MAPPED
    return iemMemMapDataU128RoSafeJmp(pVCpu, pbUnmapInfo, UINT8_MAX, GCPtrMem);
#else
    return iemMemFlatMapDataU128RoJmp(pVCpu, pbUnmapInfo, GCPtrMem);
#endif
}


/*********************************************************************************************************************************
*   Helpers: Commit, rollback & unmap                                                                                            *
*********************************************************************************************************************************/

/**
 * Used by TB code to commit and unmap a read-write memory mapping.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemCommitAndUnmapAtomic,(PVMCPUCC pVCpu, uint8_t bUnmapInfo))
{
    return iemMemCommitAndUnmapAtSafeJmp(pVCpu, bUnmapInfo);
}


/**
 * Used by TB code to commit and unmap a read-write memory mapping.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemCommitAndUnmapRw,(PVMCPUCC pVCpu, uint8_t bUnmapInfo))
{
    return iemMemCommitAndUnmapRwSafeJmp(pVCpu, bUnmapInfo);
}


/**
 * Used by TB code to commit and unmap a write-only memory mapping.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemCommitAndUnmapWo,(PVMCPUCC pVCpu, uint8_t bUnmapInfo))
{
    return iemMemCommitAndUnmapWoSafeJmp(pVCpu, bUnmapInfo);
}


/**
 * Used by TB code to commit and unmap a read-only memory mapping.
 */
IEM_DECL_NATIVE_HLP_DEF(void, iemNativeHlpMemCommitAndUnmapRo,(PVMCPUCC pVCpu, uint8_t bUnmapInfo))
{
    return iemMemCommitAndUnmapRoSafeJmp(pVCpu, bUnmapInfo);
}


/**
 * Reinitializes the native recompiler state.
 *
 * Called before starting a new recompile job.
 */
static PIEMRECOMPILERSTATE iemNativeReInit(PIEMRECOMPILERSTATE pReNative, PCIEMTB pTb)
{
    pReNative->cLabels                     = 0;
    pReNative->bmLabelTypes                = 0;
    pReNative->cFixups                     = 0;
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    pReNative->pDbgInfo->cEntries          = 0;
#endif
    pReNative->pTbOrg                      = pTb;
    pReNative->cCondDepth                  = 0;
    pReNative->uCondSeqNo                  = 0;
    pReNative->uCheckIrqSeqNo              = 0;
    pReNative->uTlbSeqNo                   = 0;

#ifdef IEMNATIVE_WITH_DELAYED_PC_UPDATING
    pReNative->Core.offPc                  = 0;
    pReNative->Core.cInstrPcUpdateSkipped  = 0;
#endif
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    pReNative->fSimdRaiseXcptChecksEmitted = 0;
#endif
    pReNative->Core.bmHstRegs              = IEMNATIVE_REG_FIXED_MASK
#if IEMNATIVE_HST_GREG_COUNT < 32
                                           | ~(RT_BIT(IEMNATIVE_HST_GREG_COUNT) - 1U)
#endif
                                           ;
    pReNative->Core.bmHstRegsWithGstShadow = 0;
    pReNative->Core.bmGstRegShadows        = 0;
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    pReNative->Core.bmGstRegShadowDirty    = 0;
#endif
    pReNative->Core.bmVars                 = 0;
    pReNative->Core.bmStack                = 0;
    AssertCompile(sizeof(pReNative->Core.bmStack) * 8 == IEMNATIVE_FRAME_VAR_SLOTS); /* Must set reserved slots to 1 otherwise. */
    pReNative->Core.u64ArgVars             = UINT64_MAX;

    AssertCompile(RT_ELEMENTS(pReNative->aidxUniqueLabels) == 17);
    pReNative->aidxUniqueLabels[0]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[1]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[2]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[3]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[4]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[5]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[6]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[7]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[8]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[9]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[10]        = UINT32_MAX;
    pReNative->aidxUniqueLabels[11]        = UINT32_MAX;
    pReNative->aidxUniqueLabels[12]        = UINT32_MAX;
    pReNative->aidxUniqueLabels[13]        = UINT32_MAX;
    pReNative->aidxUniqueLabels[14]        = UINT32_MAX;
    pReNative->aidxUniqueLabels[15]        = UINT32_MAX;
    pReNative->aidxUniqueLabels[16]        = UINT32_MAX;

    /* Full host register reinit: */
    for (unsigned i = 0; i < RT_ELEMENTS(pReNative->Core.aHstRegs); i++)
    {
        pReNative->Core.aHstRegs[i].fGstRegShadows = 0;
        pReNative->Core.aHstRegs[i].enmWhat        = kIemNativeWhat_Invalid;
        pReNative->Core.aHstRegs[i].idxVar         = UINT8_MAX;
    }

    uint32_t fRegs = IEMNATIVE_REG_FIXED_MASK
                   & ~(  RT_BIT_32(IEMNATIVE_REG_FIXED_PVMCPU)
#ifdef IEMNATIVE_REG_FIXED_PCPUMCTX
                       | RT_BIT_32(IEMNATIVE_REG_FIXED_PCPUMCTX)
#endif
#ifdef IEMNATIVE_REG_FIXED_PCPUMCTX
                       | RT_BIT_32(IEMNATIVE_REG_FIXED_TMP0)
#endif
#ifdef IEMNATIVE_REG_FIXED_TMP1
                       | RT_BIT_32(IEMNATIVE_REG_FIXED_TMP1)
#endif
#ifdef IEMNATIVE_REG_FIXED_PC_DBG
                       | RT_BIT_32(IEMNATIVE_REG_FIXED_PC_DBG)
#endif
                      );
    for (uint32_t idxReg = ASMBitFirstSetU32(fRegs) - 1; fRegs != 0; idxReg = ASMBitFirstSetU32(fRegs) - 1)
    {
        fRegs &= ~RT_BIT_32(idxReg);
        pReNative->Core.aHstRegs[IEMNATIVE_REG_FIXED_PVMCPU].enmWhat = kIemNativeWhat_FixedReserved;
    }

    pReNative->Core.aHstRegs[IEMNATIVE_REG_FIXED_PVMCPU].enmWhat     = kIemNativeWhat_pVCpuFixed;
#ifdef IEMNATIVE_REG_FIXED_PCPUMCTX
    pReNative->Core.aHstRegs[IEMNATIVE_REG_FIXED_PCPUMCTX].enmWhat   = kIemNativeWhat_pCtxFixed;
#endif
#ifdef IEMNATIVE_REG_FIXED_TMP0
    pReNative->Core.aHstRegs[IEMNATIVE_REG_FIXED_TMP0].enmWhat       = kIemNativeWhat_FixedTmp;
#endif
#ifdef IEMNATIVE_REG_FIXED_TMP1
    pReNative->Core.aHstRegs[IEMNATIVE_REG_FIXED_TMP1].enmWhat       = kIemNativeWhat_FixedTmp;
#endif
#ifdef IEMNATIVE_REG_FIXED_PC_DBG
    pReNative->Core.aHstRegs[IEMNATIVE_REG_FIXED_PC_DBG].enmWhat     = kIemNativeWhat_PcShadow;
#endif

#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    pReNative->Core.bmHstSimdRegs          = IEMNATIVE_SIMD_REG_FIXED_MASK
# if IEMNATIVE_HST_SIMD_REG_COUNT < 32
                                           | ~(RT_BIT(IEMNATIVE_HST_SIMD_REG_COUNT) - 1U)
# endif
                                           ;
    pReNative->Core.bmHstSimdRegsWithGstShadow   = 0;
    pReNative->Core.bmGstSimdRegShadows          = 0;
    pReNative->Core.bmGstSimdRegShadowDirtyLo128 = 0;
    pReNative->Core.bmGstSimdRegShadowDirtyHi128 = 0;

    /* Full host register reinit: */
    for (unsigned i = 0; i < RT_ELEMENTS(pReNative->Core.aHstSimdRegs); i++)
    {
        pReNative->Core.aHstSimdRegs[i].fGstRegShadows = 0;
        pReNative->Core.aHstSimdRegs[i].enmWhat        = kIemNativeWhat_Invalid;
        pReNative->Core.aHstSimdRegs[i].idxVar         = UINT8_MAX;
        pReNative->Core.aHstSimdRegs[i].enmLoaded      = kIemNativeGstSimdRegLdStSz_Invalid;
    }

    fRegs = IEMNATIVE_SIMD_REG_FIXED_MASK;
    for (uint32_t idxReg = ASMBitFirstSetU32(fRegs) - 1; fRegs != 0; idxReg = ASMBitFirstSetU32(fRegs) - 1)
    {
        fRegs &= ~RT_BIT_32(idxReg);
        pReNative->Core.aHstSimdRegs[idxReg].enmWhat = kIemNativeWhat_FixedReserved;
    }

#ifdef IEMNATIVE_SIMD_REG_FIXED_TMP0
    pReNative->Core.aHstSimdRegs[IEMNATIVE_SIMD_REG_FIXED_TMP0].enmWhat = kIemNativeWhat_FixedTmp;
#endif

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
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    pReNative->pDbgInfo  = (PIEMTBDBG)RTMemAllocZ(RT_UOFFSETOF_DYN(IEMTBDBG, aEntries[_16K]));
#endif
    if (RT_LIKELY(   pReNative->pInstrBuf
                  && pReNative->paLabels
                  && pReNative->paFixups)
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        && pReNative->pDbgInfo
#endif
       )
    {
        /*
         * Set the buffer & array sizes on success.
         */
        pReNative->cInstrBufAlloc = _64K / sizeof(IEMNATIVEINSTR);
        pReNative->cLabelsAlloc   = _8K;
        pReNative->cFixupsAlloc   = _16K;
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        pReNative->cDbgInfoAlloc  = _16K;
#endif

        /* Other constant stuff: */
        pReNative->pVCpu          = pVCpu;

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
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    RTMemFree(pReNative->pDbgInfo);
#endif
    RTMemFree(pReNative);
    return NULL;
}


/**
 * Creates a label
 *
 * If the label does not yet have a defined position,
 * call iemNativeLabelDefine() later to set it.
 *
 * @returns Label ID. Throws VBox status code on failure, so no need to check
 *          the return value.
 * @param   pReNative   The native recompile state.
 * @param   enmType     The label type.
 * @param   offWhere    The instruction offset of the label.  UINT32_MAX if the
 *                      label is not yet defined (default).
 * @param   uData       Data associated with the lable. Only applicable to
 *                      certain type of labels. Default is zero.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeLabelCreate(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                     uint32_t offWhere /*= UINT32_MAX*/, uint16_t uData /*= 0*/)
{
    Assert(uData == 0 || enmType >= kIemNativeLabelType_FirstWithMultipleInstances);

    /*
     * Locate existing label definition.
     *
     * This is only allowed for forward declarations where offWhere=UINT32_MAX
     * and uData is zero.
     */
    PIEMNATIVELABEL paLabels = pReNative->paLabels;
    uint32_t const  cLabels  = pReNative->cLabels;
    if (   pReNative->bmLabelTypes & RT_BIT_64(enmType)
#ifndef VBOX_STRICT
        && enmType  <  kIemNativeLabelType_FirstWithMultipleInstances
        && offWhere == UINT32_MAX
        && uData    == 0
#endif
        )
    {
#ifndef VBOX_STRICT
        AssertStmt(enmType > kIemNativeLabelType_Invalid && enmType < kIemNativeLabelType_FirstWithMultipleInstances,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_1));
        uint32_t const idxLabel = pReNative->aidxUniqueLabels[enmType];
        if (idxLabel < pReNative->cLabels)
            return idxLabel;
#else
        for (uint32_t i = 0; i < cLabels; i++)
            if (   paLabels[i].enmType == enmType
                && paLabels[i].uData   == uData)
            {
                AssertStmt(uData == 0, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_1));
                AssertStmt(offWhere == UINT32_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_1));
                AssertStmt(paLabels[i].off == UINT32_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_2));
                AssertStmt(enmType < kIemNativeLabelType_FirstWithMultipleInstances && pReNative->aidxUniqueLabels[enmType] == i,
                           IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_1));
                return i;
            }
        AssertStmt(   enmType >= kIemNativeLabelType_FirstWithMultipleInstances
                   || pReNative->aidxUniqueLabels[enmType] == UINT32_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_1));
#endif
    }

    /*
     * Make sure we've got room for another label.
     */
    if (RT_LIKELY(cLabels < pReNative->cLabelsAlloc))
    { /* likely */ }
    else
    {
        uint32_t cNew = pReNative->cLabelsAlloc;
        AssertStmt(cNew, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_3));
        AssertStmt(cLabels == cNew, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_3));
        cNew *= 2;
        AssertStmt(cNew <= _64K, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_TOO_MANY)); /* IEMNATIVEFIXUP::idxLabel type restrict this */
        paLabels = (PIEMNATIVELABEL)RTMemRealloc(paLabels, cNew * sizeof(paLabels[0]));
        AssertStmt(paLabels, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_OUT_OF_MEMORY));
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

    Assert((unsigned)enmType < 64);
    pReNative->bmLabelTypes |= RT_BIT_64(enmType);

    if (enmType < kIemNativeLabelType_FirstWithMultipleInstances)
    {
        Assert(uData == 0);
        pReNative->aidxUniqueLabels[enmType] = cLabels;
    }

    if (offWhere != UINT32_MAX)
    {
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        iemNativeDbgInfoAddNativeOffset(pReNative, offWhere);
        iemNativeDbgInfoAddLabel(pReNative, enmType, uData);
#endif
    }
    return cLabels;
}


/**
 * Defines the location of an existing label.
 *
 * @param   pReNative   The native recompile state.
 * @param   idxLabel    The label to define.
 * @param   offWhere    The position.
 */
DECL_HIDDEN_THROW(void) iemNativeLabelDefine(PIEMRECOMPILERSTATE pReNative, uint32_t idxLabel, uint32_t offWhere)
{
    AssertStmt(idxLabel < pReNative->cLabels, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_4));
    PIEMNATIVELABEL const pLabel = &pReNative->paLabels[idxLabel];
    AssertStmt(pLabel->off == UINT32_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_5));
    pLabel->off = offWhere;
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    iemNativeDbgInfoAddNativeOffset(pReNative, offWhere);
    iemNativeDbgInfoAddLabel(pReNative, (IEMNATIVELABELTYPE)pLabel->enmType, pLabel->uData);
#endif
}


/**
 * Looks up a lable.
 *
 * @returns Label ID if found, UINT32_MAX if not.
 */
static uint32_t iemNativeLabelFind(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType,
                                   uint32_t offWhere = UINT32_MAX, uint16_t uData = 0) RT_NOEXCEPT
{
    Assert((unsigned)enmType < 64);
    if (RT_BIT_64(enmType) & pReNative->bmLabelTypes)
    {
        if (enmType < kIemNativeLabelType_FirstWithMultipleInstances)
            return pReNative->aidxUniqueLabels[enmType];

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
 * @throws  VBox status code (int) on failure.
 * @param   pReNative   The native recompile state.
 * @param   offWhere    The instruction offset of the fixup location.
 * @param   idxLabel    The target label ID for the fixup.
 * @param   enmType     The fixup type.
 * @param   offAddend   Fixup addend if applicable to the type. Default is 0.
 */
DECL_HIDDEN_THROW(void)
iemNativeAddFixup(PIEMRECOMPILERSTATE pReNative, uint32_t offWhere, uint32_t idxLabel,
                  IEMNATIVEFIXUPTYPE enmType, int8_t offAddend /*= 0*/)
{
    Assert(idxLabel <= UINT16_MAX);
    Assert((unsigned)enmType <= UINT8_MAX);
#ifdef RT_ARCH_ARM64
    AssertStmt(   enmType != kIemNativeFixupType_RelImm14At5
               || pReNative->paLabels[idxLabel].enmType >= kIemNativeLabelType_LastWholeTbBranch,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_FIXUP_SHORT_JMP_TO_TAIL_LABEL));
#endif

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
        AssertStmt(cNew, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_FIXUP_IPE_1));
        AssertStmt(cFixups == cNew, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_FIXUP_IPE_1));
        cNew *= 2;
        AssertStmt(cNew <= _128K, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_FIXUP_TOO_MANY));
        paFixups = (PIEMNATIVEFIXUP)RTMemRealloc(paFixups, cNew * sizeof(paFixups[0]));
        AssertStmt(paFixups, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_FIXUP_OUT_OF_MEMORY));
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
}


/**
 * Slow code path for iemNativeInstrBufEnsure.
 */
DECL_HIDDEN_THROW(PIEMNATIVEINSTR) iemNativeInstrBufEnsureSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t cInstrReq)
{
    /* Double the buffer size till we meet the request. */
    uint32_t cNew = pReNative->cInstrBufAlloc;
    AssertStmt(cNew > 0, IEMNATIVE_DO_LONGJMP(pReNative, VERR_INTERNAL_ERROR_5)); /* impossible */
    do
        cNew *= 2;
    while (cNew < off + cInstrReq);

    uint32_t const cbNew = cNew * sizeof(IEMNATIVEINSTR);
#ifdef RT_ARCH_ARM64
    uint32_t const cbMaxInstrBuf = _1M; /* Limited by the branch instruction range (18+2 bits). */
#else
    uint32_t const cbMaxInstrBuf = _2M;
#endif
    AssertStmt(cbNew <= cbMaxInstrBuf, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_INSTR_BUF_TOO_LARGE));

    void *pvNew = RTMemRealloc(pReNative->pInstrBuf, cbNew);
    AssertStmt(pvNew, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_INSTR_BUF_OUT_OF_MEMORY));

#ifdef VBOX_STRICT
    pReNative->offInstrBufChecked = off + cInstrReq;
#endif
    pReNative->cInstrBufAlloc     = cNew;
    return pReNative->pInstrBuf   = (PIEMNATIVEINSTR)pvNew;
}

#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO

/**
 * Grows the static debug info array used during recompilation.
 *
 * @returns Pointer to the new debug info block; throws VBox status code on
 *          failure, so no need to check the return value.
 */
DECL_NO_INLINE(static, PIEMTBDBG) iemNativeDbgInfoGrow(PIEMRECOMPILERSTATE pReNative, PIEMTBDBG pDbgInfo)
{
    uint32_t cNew = pReNative->cDbgInfoAlloc * 2;
    AssertStmt(cNew < _1M && cNew != 0, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_DBGINFO_IPE_1));
    pDbgInfo = (PIEMTBDBG)RTMemRealloc(pDbgInfo, RT_UOFFSETOF_DYN(IEMTBDBG, aEntries[cNew]));
    AssertStmt(pDbgInfo, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_DBGINFO_OUT_OF_MEMORY));
    pReNative->pDbgInfo      = pDbgInfo;
    pReNative->cDbgInfoAlloc = cNew;
    return pDbgInfo;
}


/**
 * Adds a new debug info uninitialized entry, returning the pointer to it.
 */
DECL_INLINE_THROW(PIEMTBDBGENTRY) iemNativeDbgInfoAddNewEntry(PIEMRECOMPILERSTATE pReNative, PIEMTBDBG pDbgInfo)
{
    if (RT_LIKELY(pDbgInfo->cEntries < pReNative->cDbgInfoAlloc))
    { /* likely */ }
    else
        pDbgInfo = iemNativeDbgInfoGrow(pReNative, pDbgInfo);
    return &pDbgInfo->aEntries[pDbgInfo->cEntries++];
}


/**
 * Debug Info: Adds a native offset record, if necessary.
 */
DECL_HIDDEN_THROW(void) iemNativeDbgInfoAddNativeOffset(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    PIEMTBDBG pDbgInfo = pReNative->pDbgInfo;

    /*
     * Search backwards to see if we've got a similar record already.
     */
    uint32_t idx     = pDbgInfo->cEntries;
    uint32_t idxStop = idx > 16 ? idx - 16 : 0;
    while (idx-- > idxStop)
        if (pDbgInfo->aEntries[idx].Gen.uType == kIemTbDbgEntryType_NativeOffset)
        {
            if (pDbgInfo->aEntries[idx].NativeOffset.offNative == off)
                return;
            AssertStmt(pDbgInfo->aEntries[idx].NativeOffset.offNative < off,
                       IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_DBGINFO_IPE_2));
            break;
        }

    /*
     * Add it.
     */
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pDbgInfo);
    pEntry->NativeOffset.uType     = kIemTbDbgEntryType_NativeOffset;
    pEntry->NativeOffset.offNative = off;
}


/**
 * Debug Info: Record info about a label.
 */
static void iemNativeDbgInfoAddLabel(PIEMRECOMPILERSTATE pReNative, IEMNATIVELABELTYPE enmType, uint16_t uData)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->Label.uType    = kIemTbDbgEntryType_Label;
    pEntry->Label.uUnused  = 0;
    pEntry->Label.enmLabel = (uint8_t)enmType;
    pEntry->Label.uData    = uData;
}


/**
 * Debug Info: Record info about a threaded call.
 */
static void iemNativeDbgInfoAddThreadedCall(PIEMRECOMPILERSTATE pReNative, IEMTHREADEDFUNCS enmCall, bool fRecompiled)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->ThreadedCall.uType       = kIemTbDbgEntryType_ThreadedCall;
    pEntry->ThreadedCall.fRecompiled = fRecompiled;
    pEntry->ThreadedCall.uUnused     = 0;
    pEntry->ThreadedCall.enmCall     = (uint16_t)enmCall;
}


/**
 * Debug Info: Record info about a new guest instruction.
 */
static void iemNativeDbgInfoAddGuestInstruction(PIEMRECOMPILERSTATE pReNative, uint32_t fExec)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->GuestInstruction.uType   = kIemTbDbgEntryType_GuestInstruction;
    pEntry->GuestInstruction.uUnused = 0;
    pEntry->GuestInstruction.fExec   = fExec;
}


/**
 * Debug Info: Record info about guest register shadowing.
 */
DECL_HIDDEN_THROW(void)
iemNativeDbgInfoAddGuestRegShadowing(PIEMRECOMPILERSTATE pReNative, IEMNATIVEGSTREG enmGstReg,
                                     uint8_t idxHstReg /*= UINT8_MAX*/, uint8_t idxHstRegPrev /*= UINT8_MAX*/)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->GuestRegShadowing.uType         = kIemTbDbgEntryType_GuestRegShadowing;
    pEntry->GuestRegShadowing.uUnused       = 0;
    pEntry->GuestRegShadowing.idxGstReg     = enmGstReg;
    pEntry->GuestRegShadowing.idxHstReg     = idxHstReg;
    pEntry->GuestRegShadowing.idxHstRegPrev = idxHstRegPrev;
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    Assert(   idxHstReg != UINT8_MAX
           || !(pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(enmGstReg)));
#endif
}


# ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Debug Info: Record info about guest register shadowing.
 */
DECL_HIDDEN_THROW(void)
iemNativeDbgInfoAddGuestSimdRegShadowing(PIEMRECOMPILERSTATE pReNative, IEMNATIVEGSTSIMDREG enmGstSimdReg,
                                         uint8_t idxHstSimdReg /*= UINT8_MAX*/, uint8_t idxHstSimdRegPrev /*= UINT8_MAX*/)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->GuestSimdRegShadowing.uType             = kIemTbDbgEntryType_GuestSimdRegShadowing;
    pEntry->GuestSimdRegShadowing.uUnused           = 0;
    pEntry->GuestSimdRegShadowing.idxGstSimdReg     = enmGstSimdReg;
    pEntry->GuestSimdRegShadowing.idxHstSimdReg     = idxHstSimdReg;
    pEntry->GuestSimdRegShadowing.idxHstSimdRegPrev = idxHstSimdRegPrev;
}
# endif


# ifdef IEMNATIVE_WITH_DELAYED_PC_UPDATING
/**
 * Debug Info: Record info about delayed RIP updates.
 */
DECL_HIDDEN_THROW(void) iemNativeDbgInfoAddDelayedPcUpdate(PIEMRECOMPILERSTATE pReNative, uint32_t offPc, uint32_t cInstrSkipped)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->DelayedPcUpdate.uType         = kIemTbDbgEntryType_DelayedPcUpdate;
    pEntry->DelayedPcUpdate.offPc         = offPc;
    pEntry->DelayedPcUpdate.cInstrSkipped = cInstrSkipped;
}
# endif

# if defined(IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK) || defined(IEMNATIVE_WITH_SIMD_REG_ALLOCATOR)

/**
 * Debug Info: Record info about a dirty guest register.
 */
DECL_HIDDEN_THROW(void) iemNativeDbgInfoAddGuestRegDirty(PIEMRECOMPILERSTATE pReNative, bool fSimdReg,
                                                         uint8_t idxGstReg, uint8_t idxHstReg)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->GuestRegDirty.uType         = kIemTbDbgEntryType_GuestRegDirty;
    pEntry->GuestRegDirty.fSimdReg      = fSimdReg ? 1 : 0;
    pEntry->GuestRegDirty.idxGstReg     = idxGstReg;
    pEntry->GuestRegDirty.idxHstReg     = idxHstReg;
}


/**
 * Debug Info: Record info about a dirty guest register writeback operation.
 */
DECL_HIDDEN_THROW(void) iemNativeDbgInfoAddGuestRegWriteback(PIEMRECOMPILERSTATE pReNative, bool fSimdReg, uint64_t fGstReg)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->GuestRegWriteback.uType         = kIemTbDbgEntryType_GuestRegWriteback;
    pEntry->GuestRegWriteback.fSimdReg      = fSimdReg ? 1 : 0;
    pEntry->GuestRegWriteback.fGstReg       = (uint32_t)fGstReg;
    /** @todo r=aeichner Can't fit the whole register mask in the debug info entry, deal with it when it becomes necessary. */
    Assert((uint64_t)pEntry->GuestRegWriteback.fGstReg == fGstReg);
}

# endif /* defined(IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK) || defined(IEMNATIVE_WITH_SIMD_REG_ALLOCATOR) */

#endif /* IEMNATIVE_WITH_TB_DEBUG_INFO */


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
AssertCompile(RT_ELEMENTS(g_aidxIemNativeCallRegs) == IEMNATIVE_CALL_ARG_GREG_COUNT);

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

#ifdef IEMNATIVE_FP_OFF_STACK_ARG0
/**
 * BP offset of the stack argument slots.
 *
 * This array is indexed by \#argument - IEMNATIVE_CALL_ARG_GREG_COUNT and has
 * IEMNATIVE_FRAME_STACK_ARG_COUNT entries.
 */
DECL_HIDDEN_CONST(int32_t) const g_aoffIemNativeCallStackArgBpDisp[] =
{
    IEMNATIVE_FP_OFF_STACK_ARG0,
# ifdef IEMNATIVE_FP_OFF_STACK_ARG1
    IEMNATIVE_FP_OFF_STACK_ARG1,
# endif
# ifdef IEMNATIVE_FP_OFF_STACK_ARG2
    IEMNATIVE_FP_OFF_STACK_ARG2,
# endif
# ifdef IEMNATIVE_FP_OFF_STACK_ARG3
    IEMNATIVE_FP_OFF_STACK_ARG3,
# endif
};
AssertCompile(RT_ELEMENTS(g_aoffIemNativeCallStackArgBpDisp) == IEMNATIVE_FRAME_STACK_ARG_COUNT);
#endif /* IEMNATIVE_FP_OFF_STACK_ARG0 */

/**
 * Info about shadowed guest register values.
 * @see IEMNATIVEGSTREG
 */
DECL_HIDDEN_CONST(IEMANTIVEGSTREGINFO const) g_aGstShadowInfo[] =
{
#define CPUMCTX_OFF_AND_SIZE(a_Reg) (uint32_t)RT_UOFFSETOF(VMCPU, cpum.GstCtx. a_Reg), RT_SIZEOFMEMB(VMCPU, cpum.GstCtx. a_Reg)
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
    /* [kIemNativeGstReg_Cr0] = */                      { CPUMCTX_OFF_AND_SIZE(cr0),                "cr0", },
    /* [kIemNativeGstReg_FpuFcw] = */                   { CPUMCTX_OFF_AND_SIZE(XState.x87.FCW),     "fcw", },
    /* [kIemNativeGstReg_FpuFsw] = */                   { CPUMCTX_OFF_AND_SIZE(XState.x87.FSW),     "fsw", },
    /* [kIemNativeGstReg_SegBaseFirst + 0] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[0].u64Base),  "es_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 1] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[1].u64Base),  "cs_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 2] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[2].u64Base),  "ss_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 3] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[3].u64Base),  "ds_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 4] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[4].u64Base),  "fs_base", },
    /* [kIemNativeGstReg_SegBaseFirst + 5] = */         { CPUMCTX_OFF_AND_SIZE(aSRegs[5].u64Base),  "gs_base", },
    /* [kIemNativeGstReg_SegAttribFirst + 0] = */       { CPUMCTX_OFF_AND_SIZE(aSRegs[0].Attr.u),   "es_attrib", },
    /* [kIemNativeGstReg_SegAttribFirst + 1] = */       { CPUMCTX_OFF_AND_SIZE(aSRegs[1].Attr.u),   "cs_attrib", },
    /* [kIemNativeGstReg_SegAttribFirst + 2] = */       { CPUMCTX_OFF_AND_SIZE(aSRegs[2].Attr.u),   "ss_attrib", },
    /* [kIemNativeGstReg_SegAttribFirst + 3] = */       { CPUMCTX_OFF_AND_SIZE(aSRegs[3].Attr.u),   "ds_attrib", },
    /* [kIemNativeGstReg_SegAttribFirst + 4] = */       { CPUMCTX_OFF_AND_SIZE(aSRegs[4].Attr.u),   "fs_attrib", },
    /* [kIemNativeGstReg_SegAttribFirst + 5] = */       { CPUMCTX_OFF_AND_SIZE(aSRegs[5].Attr.u),   "gs_attrib", },
    /* [kIemNativeGstReg_SegLimitFirst + 0] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[0].u32Limit), "es_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 1] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[1].u32Limit), "cs_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 2] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[2].u32Limit), "ss_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 3] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[3].u32Limit), "ds_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 4] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[4].u32Limit), "fs_limit", },
    /* [kIemNativeGstReg_SegLimitFirst + 5] = */        { CPUMCTX_OFF_AND_SIZE(aSRegs[5].u32Limit), "gs_limit", },
    /* [kIemNativeGstReg_SegSelFirst + 0] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[0].Sel),      "es", },
    /* [kIemNativeGstReg_SegSelFirst + 1] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[1].Sel),      "cs", },
    /* [kIemNativeGstReg_SegSelFirst + 2] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[2].Sel),      "ss", },
    /* [kIemNativeGstReg_SegSelFirst + 3] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[3].Sel),      "ds", },
    /* [kIemNativeGstReg_SegSelFirst + 4] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[4].Sel),      "fs", },
    /* [kIemNativeGstReg_SegSelFirst + 5] = */          { CPUMCTX_OFF_AND_SIZE(aSRegs[5].Sel),      "gs", },
    /* [kIemNativeGstReg_Cr4] = */                      { CPUMCTX_OFF_AND_SIZE(cr4),                "cr4", },
    /* [kIemNativeGstReg_Xcr0] = */                     { CPUMCTX_OFF_AND_SIZE(aXcr[0]),            "xcr0", },
    /* [kIemNativeGstReg_MxCsr] = */                    { CPUMCTX_OFF_AND_SIZE(XState.x87.MXCSR),   "mxcsr", },
    /* [kIemNativeGstReg_EFlags] = */                   { CPUMCTX_OFF_AND_SIZE(eflags),             "eflags", },
#undef CPUMCTX_OFF_AND_SIZE
};
AssertCompile(RT_ELEMENTS(g_aGstShadowInfo) == kIemNativeGstReg_End);


/** Host CPU general purpose register names. */
DECL_HIDDEN_CONST(const char * const) g_apszIemNativeHstRegNames[] =
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


#if 0 /* unused */
/**
 * Tries to locate a suitable register in the given register mask.
 *
 * This ASSUMES the caller has done the minimal/optimal allocation checks and
 * failed.
 *
 * @returns Host register number on success, returns UINT8_MAX on failure.
 */
static uint8_t iemNativeRegTryAllocFree(PIEMRECOMPILERSTATE pReNative, uint32_t fRegMask)
{
    Assert(!(fRegMask & ~IEMNATIVE_HST_GREG_MASK));
    uint32_t fRegs = ~pReNative->Core.bmHstRegs & fRegMask;
    if (fRegs)
    {
        /** @todo pick better here:    */
        unsigned const idxReg = ASMBitFirstSetU32(fRegs) - 1;

        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows != 0);
        Assert(   (pReNative->Core.aHstRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstRegShadows)
               == pReNative->Core.aHstRegs[idxReg].fGstRegShadows);
        Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

        pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
        pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);
        pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
        return idxReg;
    }
    return UINT8_MAX;
}
#endif /* unused */


#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
/**
 * Stores the host reg @a idxHstReg into guest shadow register @a enmGstReg.
 *
 * @returns New code buffer offset on success, UINT32_MAX on failure.
 * @param   pReNative   .
 * @param   off         The current code buffer position.
 * @param   enmGstReg   The guest register to store to.
 * @param   idxHstReg   The host register to store from.
 */
DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitStoreGprWithGstShadowReg(PIEMRECOMPILERSTATE pReNative, uint32_t off, IEMNATIVEGSTREG enmGstReg, uint8_t idxHstReg)
{
    Assert((unsigned)enmGstReg < (unsigned)kIemNativeGstReg_End);
    Assert(g_aGstShadowInfo[enmGstReg].cb != 0);

    switch (g_aGstShadowInfo[enmGstReg].cb)
    {
        case sizeof(uint64_t):
            return iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
        case sizeof(uint32_t):
            return iemNativeEmitStoreGprToVCpuU32(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
        case sizeof(uint16_t):
            return iemNativeEmitStoreGprToVCpuU16(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
#if 0 /* not present in the table. */
        case sizeof(uint8_t):
            return iemNativeEmitStoreGprToVCpuU8(pReNative, off, idxHstReg, g_aGstShadowInfo[enmGstReg].off);
#endif
        default:
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IPE_NOT_REACHED_DEFAULT_CASE));
    }
}


/**
 * Emits code to flush a pending write of the given guest register if any.
 *
 * @returns New code buffer offset.
 * @param   pReNative       The native recompile state.
 * @param   off             Current code buffer position.
 * @param   enmGstReg       The guest register to flush.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeRegFlushPendingWrite(PIEMRECOMPILERSTATE pReNative, uint32_t off, IEMNATIVEGSTREG enmGstReg)
{
    uint8_t const idxHstReg = pReNative->Core.aidxGstRegShadows[enmGstReg];

    Assert(enmGstReg >= kIemNativeGstReg_GprFirst && enmGstReg <= kIemNativeGstReg_GprLast);
    Assert(   idxHstReg != UINT8_MAX
           && pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(enmGstReg));
    Log12(("iemNativeRegFlushPendingWrite: Clearing guest register %s shadowed by host %s\n",
           g_aGstShadowInfo[enmGstReg].pszName, g_apszIemNativeHstRegNames[idxHstReg]));

    off = iemNativeEmitStoreGprWithGstShadowReg(pReNative, off, enmGstReg, idxHstReg);

    pReNative->Core.bmGstRegShadowDirty &= ~RT_BIT_64(enmGstReg);
    return off;
}


/**
 * Flush the given set of guest registers if marked as dirty.
 *
 * @returns New code buffer offset.
 * @param   pReNative       The native recompile state.
 * @param   off             Current code buffer position.
 * @param   fFlushGstReg    The guest register set to flush (default is flush everything).
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeRegFlushDirtyGuest(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint64_t fFlushGstReg /*= UINT64_MAX*/)
{
    if (pReNative->Core.bmGstRegShadowDirty & fFlushGstReg)
    {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        iemNativeDbgInfoAddNativeOffset(pReNative, off);
        iemNativeDbgInfoAddGuestRegWriteback(pReNative, false /*fSimdReg*/, pReNative->Core.bmGstRegShadowDirty & fFlushGstReg);
# endif

        uint64_t bmGstRegShadowDirty = pReNative->Core.bmGstRegShadowDirty & fFlushGstReg;
        uint32_t idxGstReg = 0;

        do
        {
            if (bmGstRegShadowDirty & 0x1)
            {
                off = iemNativeRegFlushPendingWrite(pReNative, off, (IEMNATIVEGSTREG)idxGstReg);
                Assert(!(pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(idxGstReg)));
            }
            idxGstReg++;
            bmGstRegShadowDirty >>= 1;
        } while (bmGstRegShadowDirty);
    }

    return off;
}


/**
 * Flush all shadowed guest registers marked as dirty for the given host register.
 *
 * @returns New code buffer offset.
 * @param   pReNative       The native recompile state.
 * @param   off             Current code buffer position.
 * @param   idxHstReg       The host register.
 *
 * @note This doesn't do any unshadowing of guest registers from the host register.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeRegFlushDirtyGuestByHostRegShadow(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxHstReg)
{
    /* We need to flush any pending guest register writes this host register shadows. */
    uint64_t fGstRegShadows = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
    if (pReNative->Core.bmGstRegShadowDirty & fGstRegShadows)
    {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        iemNativeDbgInfoAddNativeOffset(pReNative, off);
        iemNativeDbgInfoAddGuestRegWriteback(pReNative, false /*fSimdReg*/, pReNative->Core.bmGstRegShadowDirty & fGstRegShadows);
# endif

        uint64_t bmGstRegShadowDirty = pReNative->Core.bmGstRegShadowDirty & fGstRegShadows;
        uint32_t idxGstReg = 0;
        do
        {
            if (bmGstRegShadowDirty & 0x1)
            {
                off = iemNativeRegFlushPendingWrite(pReNative, off, (IEMNATIVEGSTREG)idxGstReg);
                Assert(!(pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(idxGstReg)));
            }
            idxGstReg++;
            bmGstRegShadowDirty >>= 1;
        } while (bmGstRegShadowDirty);
    }

    return off;
}
#endif


/**
 * Locate a register, possibly freeing one up.
 *
 * This ASSUMES the caller has done the minimal/optimal allocation checks and
 * failed.
 *
 * @returns Host register number on success. Returns UINT8_MAX if no registers
 *          found, the caller is supposed to deal with this and raise a
 *          allocation type specific status code (if desired).
 *
 * @throws  VBox status code if we're run into trouble spilling a variable of
 *          recording debug info.  Does NOT throw anything if we're out of
 *          registers, though.
 */
static uint8_t iemNativeRegAllocFindFree(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, bool fPreferVolatile,
                                         uint32_t fRegMask = IEMNATIVE_HST_GREG_MASK & ~IEMNATIVE_REG_FIXED_MASK)
{
    STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeRegFindFree);
    Assert(!(fRegMask & ~IEMNATIVE_HST_GREG_MASK));
    Assert(!(fRegMask & IEMNATIVE_REG_FIXED_MASK));

    /*
     * Try a freed register that's shadowing a guest register.
     */
    uint32_t fRegs = ~pReNative->Core.bmHstRegs & fRegMask;
    if (fRegs)
    {
        STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeRegFindFreeNoVar);

#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
        /*
         * When we have livness information, we use it to kick out all shadowed
         * guest register that will not be needed any more in this TB.  If we're
         * lucky, this may prevent us from ending up here again.
         *
         * Note! We must consider the previous entry here so we don't free
         *       anything that the current threaded function requires (current
         *       entry is produced by the next threaded function).
         */
        uint32_t const idxCurCall = pReNative->idxCurCall;
        if (idxCurCall > 0)
        {
            PCIEMLIVENESSENTRY const pLivenessEntry = &pReNative->paLivenessEntries[idxCurCall - 1];

# ifndef IEMLIVENESS_EXTENDED_LAYOUT
            /* Construct a mask of the guest registers in the UNUSED and XCPT_OR_CALL state. */
            AssertCompile(IEMLIVENESS_STATE_UNUSED == 1 && IEMLIVENESS_STATE_XCPT_OR_CALL == 2);
            uint64_t fToFreeMask = pLivenessEntry->Bit0.bm64 ^ pLivenessEntry->Bit1.bm64; /* mask of regs in either UNUSED */
#else
            /* Construct a mask of the registers not in the read or write state.
               Note! We could skips writes, if they aren't from us, as this is just
                     a hack to prevent trashing registers that have just been written
                     or will be written when we retire the current instruction. */
            uint64_t fToFreeMask = ~pLivenessEntry->aBits[IEMLIVENESS_BIT_READ].bm64
                                 & ~pLivenessEntry->aBits[IEMLIVENESS_BIT_WRITE].bm64
                                 & IEMLIVENESSBIT_MASK;
#endif
            /* Merge EFLAGS. */
            uint64_t fTmp = fToFreeMask & (fToFreeMask >> 3);   /* AF2,PF2,CF2,Other2 = AF,PF,CF,Other & OF,SF,ZF,AF */
            fTmp &= fTmp >> 2;                                  /*         CF3,Other3 = AF2,PF2 & CF2,Other2  */
            fTmp &= fTmp >> 1;                                  /*             Other4 = CF3 & Other3 */
            fToFreeMask &= RT_BIT_64(kIemNativeGstReg_EFlags) - 1;
            fToFreeMask |= fTmp & RT_BIT_64(kIemNativeGstReg_EFlags);

            /* If it matches any shadowed registers. */
            if (pReNative->Core.bmGstRegShadows & fToFreeMask)
            {
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                /* Writeback any dirty shadow registers we are about to unshadow. */
                *poff = iemNativeRegFlushDirtyGuest(pReNative, *poff, fToFreeMask);
#endif

                STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeRegFindFreeLivenessUnshadowed);
                iemNativeRegFlushGuestShadows(pReNative, fToFreeMask);
                Assert(fRegs == (~pReNative->Core.bmHstRegs & fRegMask)); /* this shall not change. */

                /* See if we've got any unshadowed registers we can return now. */
                uint32_t const fUnshadowedRegs = fRegs & ~pReNative->Core.bmHstRegsWithGstShadow;
                if (fUnshadowedRegs)
                {
                    STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeRegFindFreeLivenessHelped);
                    return (fPreferVolatile
                            ? ASMBitFirstSetU32(fUnshadowedRegs)
                            : ASMBitLastSetU32(  fUnshadowedRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                               ? fUnshadowedRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fUnshadowedRegs))
                         - 1;
                }
            }
        }
#endif /* IEMNATIVE_WITH_LIVENESS_ANALYSIS */

        unsigned const idxReg = (fPreferVolatile
                                 ? ASMBitFirstSetU32(fRegs)
                                 : ASMBitLastSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                    ? fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs))
                              - 1;

        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows != 0);
        Assert(   (pReNative->Core.aHstRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstRegShadows)
               == pReNative->Core.aHstRegs[idxReg].fGstRegShadows);
        Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
        /* We need to flush any pending guest register writes this host register shadows. */
        *poff = iemNativeRegFlushDirtyGuestByHostRegShadow(pReNative, *poff, idxReg);
#endif

        pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);
        pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
        pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
        return idxReg;
    }

    /*
     * Try free up a variable that's in a register.
     *
     * We do two rounds here, first evacuating variables we don't need to be
     * saved on the stack, then in the second round move things to the stack.
     */
    STAM_REL_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeRegFindFreeVar);
    for (uint32_t iLoop = 0; iLoop < 2; iLoop++)
    {
        uint32_t fVars = pReNative->Core.bmVars;
        while (fVars)
        {
            uint32_t const idxVar = ASMBitFirstSetU32(fVars) - 1;
            uint8_t const  idxReg = pReNative->Core.aVars[idxVar].idxReg;
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
            if (pReNative->Core.aVars[idxVar].fSimdReg) /* Need to ignore SIMD variables here or we end up freeing random registers. */
                continue;
#endif

            if (   idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs)
                && (RT_BIT_32(idxReg) & fRegMask)
                && (  iLoop == 0
                    ? pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Stack
                    : pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                && !pReNative->Core.aVars[idxVar].fRegAcquired)
            {
                Assert(pReNative->Core.bmHstRegs & RT_BIT_32(idxReg));
                Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxReg].fGstRegShadows)
                       == pReNative->Core.aHstRegs[idxReg].fGstRegShadows);
                Assert(pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
                Assert(   RT_BOOL(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg))
                       == RT_BOOL(pReNative->Core.aHstRegs[idxReg].fGstRegShadows));
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                Assert(!(pReNative->Core.aHstRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstRegShadowDirty));
#endif

                if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                {
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, IEMNATIVE_VAR_IDX_PACK(idxVar));
                    *poff = iemNativeEmitStoreGprByBp(pReNative, *poff, iemNativeStackCalcBpDisp(idxStackSlot), idxReg);
                }

                pReNative->Core.aVars[idxVar].idxReg    = UINT8_MAX;
                pReNative->Core.bmHstRegs              &= ~RT_BIT_32(idxReg);

                pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);
                pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
                pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
                return idxReg;
            }
            fVars &= ~RT_BIT_32(idxVar);
        }
    }

    return UINT8_MAX;
}


/**
 * Reassigns a variable to a different register specified by the caller.
 *
 * @returns The new code buffer position.
 * @param   pReNative       The native recompile state.
 * @param   off             The current code buffer position.
 * @param   idxVar          The variable index.
 * @param   idxRegOld       The old host register number.
 * @param   idxRegNew       The new host register number.
 * @param   pszCaller       The caller for logging.
 */
static uint32_t iemNativeRegMoveVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVar,
                                    uint8_t idxRegOld, uint8_t idxRegNew, const char *pszCaller)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxRegOld);
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    Assert(!pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg);
#endif
    RT_NOREF(pszCaller);

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    Assert(!(pReNative->Core.aHstRegs[idxRegNew].fGstRegShadows & pReNative->Core.bmGstRegShadowDirty));
#endif
    iemNativeRegClearGstRegShadowing(pReNative, idxRegNew, off);

    uint64_t fGstRegShadows = pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows;
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    Assert(!(fGstRegShadows & pReNative->Core.bmGstRegShadowDirty));
#endif
    Log12(("%s: moving idxVar=%#x from %s to %s (fGstRegShadows=%RX64)\n",
           pszCaller, idxVar, g_apszIemNativeHstRegNames[idxRegOld], g_apszIemNativeHstRegNames[idxRegNew], fGstRegShadows));
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegNew, idxRegOld);

    pReNative->Core.aHstRegs[idxRegNew].fGstRegShadows = fGstRegShadows;
    pReNative->Core.aHstRegs[idxRegNew].enmWhat        = kIemNativeWhat_Var;
    pReNative->Core.aHstRegs[idxRegNew].idxVar         = idxVar;
    if (fGstRegShadows)
    {
        pReNative->Core.bmHstRegsWithGstShadow = (pReNative->Core.bmHstRegsWithGstShadow & ~RT_BIT_32(idxRegOld))
                                               | RT_BIT_32(idxRegNew);
        while (fGstRegShadows)
        {
            unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegShadows) - 1;
            fGstRegShadows &= ~RT_BIT_64(idxGstReg);

            Assert(pReNative->Core.aidxGstRegShadows[idxGstReg] == idxRegOld);
            pReNative->Core.aidxGstRegShadows[idxGstReg] = idxRegNew;
        }
    }

    pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg = (uint8_t)idxRegNew;
    pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows = 0;
    pReNative->Core.bmHstRegs = RT_BIT_32(idxRegNew) | (pReNative->Core.bmHstRegs & ~RT_BIT_32(idxRegOld));
    return off;
}


/**
 * Moves a variable to a different register or spills it onto the stack.
 *
 * This must be a stack variable (kIemNativeVarKind_Stack) because the other
 * kinds can easily be recreated if needed later.
 *
 * @returns The new code buffer position.
 * @param   pReNative       The native recompile state.
 * @param   off             The current code buffer position.
 * @param   idxVar          The variable index.
 * @param   fForbiddenRegs  Mask of the forbidden registers.  Defaults to
 *                          call-volatile registers.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeRegMoveOrSpillStackVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVar,
                                                            uint32_t fForbiddenRegs /*= IEMNATIVE_CALL_VOLATILE_GREG_MASK*/)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    Assert(pVar->enmKind == kIemNativeVarKind_Stack);
    Assert(!pVar->fRegAcquired);

    uint8_t const idxRegOld = pVar->idxReg;
    Assert(idxRegOld < RT_ELEMENTS(pReNative->Core.aHstRegs));
    Assert(pReNative->Core.bmHstRegs & RT_BIT_32(idxRegOld));
    Assert(pReNative->Core.aHstRegs[idxRegOld].enmWhat == kIemNativeWhat_Var);
    Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows)
           == pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows);
    Assert(pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(   RT_BOOL(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxRegOld))
           == RT_BOOL(pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows));
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    Assert(!(pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows & pReNative->Core.bmGstRegShadowDirty));
#endif


    /** @todo Add statistics on this.*/
    /** @todo Implement basic variable liveness analysis (python) so variables
     * can be freed immediately once no longer used.  This has the potential to
     * be trashing registers and stack for dead variables.
     * Update: This is mostly done. (Not IEMNATIVE_WITH_LIVENESS_ANALYSIS.) */

    /*
     * First try move it to a different register, as that's cheaper.
     */
    fForbiddenRegs |= RT_BIT_32(idxRegOld);
    fForbiddenRegs |= IEMNATIVE_REG_FIXED_MASK;
    uint32_t fRegs = ~pReNative->Core.bmHstRegs & ~fForbiddenRegs;
    if (fRegs)
    {
        /* Avoid using shadow registers, if possible. */
        if (fRegs & ~pReNative->Core.bmHstRegsWithGstShadow)
            fRegs &= ~pReNative->Core.bmHstRegsWithGstShadow;
        unsigned const idxRegNew = ASMBitFirstSetU32(fRegs) - 1;
        return iemNativeRegMoveVar(pReNative, off, idxVar, idxRegOld, idxRegNew, "iemNativeRegMoveOrSpillStackVar");
    }

    /*
     * Otherwise we must spill the register onto the stack.
     */
    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
    Log12(("iemNativeRegMoveOrSpillStackVar: spilling idxVar=%#x/idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
           idxVar, idxRegOld, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
    off = iemNativeEmitStoreGprByBp(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);

    pVar->idxReg                            = UINT8_MAX;
    pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxRegOld);
    pReNative->Core.bmHstRegs              &= ~RT_BIT_32(idxRegOld);
    pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows;
    pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows = 0;
    return off;
}


/**
 * Allocates a temporary host general purpose register.
 *
 * This may emit code to save register content onto the stack in order to free
 * up a register.
 *
 * @returns The host register number; throws VBox status code on failure,
 *          so no need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 *                          This will be update if we need to move a variable from
 *                          register to stack in order to satisfy the request.
 * @param   fPreferVolatile Whether to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false, for iemNativeRegAllocTmpForGuestReg()).
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeRegAllocTmp(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, bool fPreferVolatile /*= true*/)
{
    /*
     * Try find a completely unused register, preferably a call-volatile one.
     */
    uint8_t  idxReg;
    uint32_t fRegs = ~pReNative->Core.bmHstRegs
                   & ~pReNative->Core.bmHstRegsWithGstShadow
                   & (~IEMNATIVE_REG_FIXED_MASK & IEMNATIVE_HST_GREG_MASK);
    if (fRegs)
    {
        if (fPreferVolatile)
            idxReg = (uint8_t)ASMBitFirstSetU32(  fRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                ? fRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
        else
            idxReg = (uint8_t)ASMBitFirstSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                ? fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows == 0);
        Assert(!(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg)));
        Log12(("iemNativeRegAllocTmp: %s\n", g_apszIemNativeHstRegNames[idxReg]));
    }
    else
    {
        idxReg = iemNativeRegAllocFindFree(pReNative, poff, fPreferVolatile);
        AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_TMP));
        Log12(("iemNativeRegAllocTmp: %s (slow)\n", g_apszIemNativeHstRegNames[idxReg]));
    }
    return iemNativeRegMarkAllocated(pReNative, idxReg, kIemNativeWhat_Tmp);
}


/**
 * Alternative version of iemNativeRegAllocTmp that takes mask with acceptable
 * registers.
 *
 * @returns The host register number; throws VBox status code on failure,
 *          so no need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 *                          This will be update if we need to move a variable from
 *                          register to stack in order to satisfy the request.
 * @param   fRegMask        Mask of acceptable registers.
 * @param   fPreferVolatile Whether to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false, for iemNativeRegAllocTmpForGuestReg()).
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeRegAllocTmpEx(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint32_t fRegMask,
                                                  bool fPreferVolatile /*= true*/)
{
    Assert(!(fRegMask & ~IEMNATIVE_HST_GREG_MASK));
    Assert(!(fRegMask & IEMNATIVE_REG_FIXED_MASK));

    /*
     * Try find a completely unused register, preferably a call-volatile one.
     */
    uint8_t  idxReg;
    uint32_t fRegs = ~pReNative->Core.bmHstRegs
                   & ~pReNative->Core.bmHstRegsWithGstShadow
                   & (~IEMNATIVE_REG_FIXED_MASK & IEMNATIVE_HST_GREG_MASK)
                   & fRegMask;
    if (fRegs)
    {
        if (fPreferVolatile)
            idxReg = (uint8_t)ASMBitFirstSetU32(  fRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                ? fRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
        else
            idxReg = (uint8_t)ASMBitFirstSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                ? fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows == 0);
        Assert(!(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg)));
        Log12(("iemNativeRegAllocTmpEx: %s\n", g_apszIemNativeHstRegNames[idxReg]));
    }
    else
    {
        idxReg = iemNativeRegAllocFindFree(pReNative, poff, fPreferVolatile, fRegMask);
        AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_TMP));
        Log12(("iemNativeRegAllocTmpEx: %s (slow)\n", g_apszIemNativeHstRegNames[idxReg]));
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
 * @returns The host register number; throws VBox status code on failure, so no
 *          need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 * @param   uImm            The immediate value that the register must hold upon
 *                          return.
 * @param   fPreferVolatile Whether to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false).
 *
 * @note    Reusing immediate values has not been implemented yet.
 */
DECL_HIDDEN_THROW(uint8_t)
iemNativeRegAllocTmpImm(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint64_t uImm, bool fPreferVolatile /*= true*/)
{
    uint8_t const idxReg = iemNativeRegAllocTmp(pReNative, poff, fPreferVolatile);
    *poff = iemNativeEmitLoadGprImm64(pReNative, *poff, idxReg, uImm);
    return idxReg;
}


/**
 * Allocates a temporary host general purpose register for keeping a guest
 * register value.
 *
 * Since we may already have a register holding the guest register value,
 * code will be emitted to do the loading if that's not the case. Code may also
 * be emitted if we have to free up a register to satify the request.
 *
 * @returns The host register number; throws VBox status code on failure, so no
 *          need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer
 *                          position. This will be update if we need to move a
 *                          variable from register to stack in order to satisfy
 *                          the request.
 * @param   enmGstReg       The guest register that will is to be updated.
 * @param   enmIntendedUse  How the caller will be using the host register.
 * @param   fNoVolatileRegs Set if no volatile register allowed, clear if any
 *                          register is okay (default).  The ASSUMPTION here is
 *                          that the caller has already flushed all volatile
 *                          registers, so this is only applied if we allocate a
 *                          new register.
 * @param   fSkipLivenessAssert     Hack for liveness input validation of EFLAGS.
 * @sa      iemNativeRegAllocTmpForGuestRegIfAlreadyPresent
 */
DECL_HIDDEN_THROW(uint8_t)
iemNativeRegAllocTmpForGuestReg(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, IEMNATIVEGSTREG enmGstReg,
                                IEMNATIVEGSTREGUSE enmIntendedUse /*= kIemNativeGstRegUse_ReadOnly*/,
                                bool fNoVolatileRegs /*= false*/, bool fSkipLivenessAssert /*= false*/)
{
    Assert(enmGstReg < kIemNativeGstReg_End && g_aGstShadowInfo[enmGstReg].cb != 0);
#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
    AssertMsg(   fSkipLivenessAssert
              || pReNative->idxCurCall == 0
              || enmGstReg == kIemNativeGstReg_Pc
              || (enmIntendedUse == kIemNativeGstRegUse_ForFullWrite
                  ? IEMLIVENESS_STATE_IS_CLOBBER_EXPECTED(iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstReg))
                  : enmIntendedUse == kIemNativeGstRegUse_ForUpdate
                  ? IEMLIVENESS_STATE_IS_MODIFY_EXPECTED( iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstReg))
                  : IEMLIVENESS_STATE_IS_INPUT_EXPECTED(  iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstReg)) ),
              ("%s - %u\n", g_aGstShadowInfo[enmGstReg].pszName, iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstReg)));
#endif
    RT_NOREF(fSkipLivenessAssert);
#if defined(LOG_ENABLED) || defined(VBOX_STRICT)
    static const char * const s_pszIntendedUse[] = { "fetch", "update", "full write", "destructive calc" };
#endif
    uint32_t const fRegMask = !fNoVolatileRegs
                            ? IEMNATIVE_HST_GREG_MASK & ~IEMNATIVE_REG_FIXED_MASK
                            : IEMNATIVE_HST_GREG_MASK & ~IEMNATIVE_REG_FIXED_MASK & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK;

    /*
     * First check if the guest register value is already in a host register.
     */
    if (pReNative->Core.bmGstRegShadows & RT_BIT_64(enmGstReg))
    {
        uint8_t idxReg = pReNative->Core.aidxGstRegShadows[enmGstReg];
        Assert(idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs));
        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows & RT_BIT_64(enmGstReg));
        Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

        /* It's not supposed to be allocated... */
        if (!(pReNative->Core.bmHstRegs & RT_BIT_32(idxReg)))
        {
            /*
             * If the register will trash the guest shadow copy, try find a
             * completely unused register we can use instead.  If that fails,
             * we need to disassociate the host reg from the guest reg.
             */
            /** @todo would be nice to know if preserving the register is in any way helpful. */
            /* If the purpose is calculations, try duplicate the register value as
               we'll be clobbering the shadow. */
            if (   enmIntendedUse == kIemNativeGstRegUse_Calculation
                && (  ~pReNative->Core.bmHstRegs
                    & ~pReNative->Core.bmHstRegsWithGstShadow
                    & (~IEMNATIVE_REG_FIXED_MASK & IEMNATIVE_HST_GREG_MASK)))
            {
                uint8_t const idxRegNew = iemNativeRegAllocTmpEx(pReNative, poff, fRegMask);

                *poff = iemNativeEmitLoadGprFromGpr(pReNative, *poff, idxRegNew, idxReg);

                Log12(("iemNativeRegAllocTmpForGuestReg: Duplicated %s for guest %s into %s for destructive calc\n",
                       g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                       g_apszIemNativeHstRegNames[idxRegNew]));
                idxReg = idxRegNew;
            }
            /* If the current register matches the restrictions, go ahead and allocate
               it for the caller. */
            else if (fRegMask & RT_BIT_32(idxReg))
            {
                pReNative->Core.bmHstRegs |= RT_BIT_32(idxReg);
                pReNative->Core.aHstRegs[idxReg].enmWhat = kIemNativeWhat_Tmp;
                pReNative->Core.aHstRegs[idxReg].idxVar  = UINT8_MAX;
                if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
                    Log12(("iemNativeRegAllocTmpForGuestReg: Reusing %s for guest %s %s\n",
                           g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName, s_pszIntendedUse[enmIntendedUse]));
                else
                {
                    iemNativeRegClearGstRegShadowing(pReNative, idxReg, *poff);
                    Log12(("iemNativeRegAllocTmpForGuestReg: Grabbing %s for guest %s - destructive calc\n",
                           g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName));
                }
            }
            /* Otherwise, allocate a register that satisfies the caller and transfer
               the shadowing if compatible with the intended use.  (This basically
               means the call wants a non-volatile register (RSP push/pop scenario).) */
            else
            {
                Assert(fNoVolatileRegs);
                uint8_t const idxRegNew = iemNativeRegAllocTmpEx(pReNative, poff, fRegMask & ~RT_BIT_32(idxReg),
                                                                    !fNoVolatileRegs
                                                                 && enmIntendedUse == kIemNativeGstRegUse_Calculation);
                *poff = iemNativeEmitLoadGprFromGpr(pReNative, *poff, idxRegNew, idxReg);
                if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
                {
                    iemNativeRegTransferGstRegShadowing(pReNative, idxReg, idxRegNew, enmGstReg, *poff);
                    Log12(("iemNativeRegAllocTmpForGuestReg: Transfering %s to %s for guest %s %s\n",
                           g_apszIemNativeHstRegNames[idxReg], g_apszIemNativeHstRegNames[idxRegNew],
                           g_aGstShadowInfo[enmGstReg].pszName, s_pszIntendedUse[enmIntendedUse]));
                }
                else
                    Log12(("iemNativeRegAllocTmpForGuestReg: Duplicated %s for guest %s into %s for destructive calc\n",
                           g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                           g_apszIemNativeHstRegNames[idxRegNew]));
                idxReg = idxRegNew;
            }
        }
        else
        {
            /*
             * Oops. Shadowed guest register already allocated!
             *
             * Allocate a new register, copy the value and, if updating, the
             * guest shadow copy assignment to the new register.
             */
            AssertMsg(   enmIntendedUse != kIemNativeGstRegUse_ForUpdate
                      && enmIntendedUse != kIemNativeGstRegUse_ForFullWrite,
                      ("This shouldn't happen: idxReg=%d enmGstReg=%d enmIntendedUse=%s\n",
                       idxReg, enmGstReg, s_pszIntendedUse[enmIntendedUse]));

            /** @todo share register for readonly access. */
            uint8_t const idxRegNew = iemNativeRegAllocTmpEx(pReNative, poff, fRegMask,
                                                             enmIntendedUse == kIemNativeGstRegUse_Calculation);

            if (enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
                *poff = iemNativeEmitLoadGprFromGpr(pReNative, *poff, idxRegNew, idxReg);

            if (   enmIntendedUse != kIemNativeGstRegUse_ForUpdate
                && enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
                Log12(("iemNativeRegAllocTmpForGuestReg: Duplicated %s for guest %s into %s for %s\n",
                       g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                       g_apszIemNativeHstRegNames[idxRegNew], s_pszIntendedUse[enmIntendedUse]));
            else
            {
                iemNativeRegTransferGstRegShadowing(pReNative, idxReg, idxRegNew, enmGstReg, *poff);
                Log12(("iemNativeRegAllocTmpForGuestReg: Moved %s for guest %s into %s for %s\n",
                       g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName,
                       g_apszIemNativeHstRegNames[idxRegNew], s_pszIntendedUse[enmIntendedUse]));
            }
            idxReg = idxRegNew;
        }
        Assert(RT_BIT_32(idxReg) & fRegMask); /* See assumption in fNoVolatileRegs docs. */

#ifdef VBOX_STRICT
        /* Strict builds: Check that the value is correct. */
        *poff = iemNativeEmitGuestRegValueCheck(pReNative, *poff, idxReg, enmGstReg);
#endif

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
        /** @todo r=aeichner Implement for registers other than GPR as well. */
        if (   (   enmIntendedUse == kIemNativeGstRegUse_ForFullWrite
                || enmIntendedUse == kIemNativeGstRegUse_ForUpdate)
            && enmGstReg >= kIemNativeGstReg_GprFirst
            && enmGstReg <= kIemNativeGstReg_GprLast
            )
        {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
            iemNativeDbgInfoAddNativeOffset(pReNative, *poff);
            iemNativeDbgInfoAddGuestRegDirty(pReNative, false /*fSimdReg*/, enmGstReg, idxReg);
# endif

            pReNative->Core.bmGstRegShadowDirty |= RT_BIT_64(enmGstReg);
        }
#endif

        return idxReg;
    }

    /*
     * Allocate a new register, load it with the guest value and designate it as a copy of the
     */
    uint8_t const idxRegNew = iemNativeRegAllocTmpEx(pReNative, poff, fRegMask, enmIntendedUse == kIemNativeGstRegUse_Calculation);

    if (enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
        *poff = iemNativeEmitLoadGprWithGstShadowReg(pReNative, *poff, idxRegNew, enmGstReg);

    if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
        iemNativeRegMarkAsGstRegShadow(pReNative, idxRegNew, enmGstReg, *poff);
    Log12(("iemNativeRegAllocTmpForGuestReg: Allocated %s for guest %s %s\n",
           g_apszIemNativeHstRegNames[idxRegNew], g_aGstShadowInfo[enmGstReg].pszName, s_pszIntendedUse[enmIntendedUse]));

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    /** @todo r=aeichner Implement for registers other than GPR as well. */
    if (   (   enmIntendedUse == kIemNativeGstRegUse_ForFullWrite
            || enmIntendedUse == kIemNativeGstRegUse_ForUpdate)
        && enmGstReg >= kIemNativeGstReg_GprFirst
        && enmGstReg <= kIemNativeGstReg_GprLast
        )
    {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        iemNativeDbgInfoAddNativeOffset(pReNative, *poff);
        iemNativeDbgInfoAddGuestRegDirty(pReNative, false /*fSimdReg*/, enmGstReg, idxRegNew);
# endif

        pReNative->Core.bmGstRegShadowDirty |= RT_BIT_64(enmGstReg);
    }
#endif

    return idxRegNew;
}


/**
 * Allocates a temporary host general purpose register that already holds the
 * given guest register value.
 *
 * The use case for this function is places where the shadowing state cannot be
 * modified due to branching and such.  This will fail if the we don't have a
 * current shadow copy handy or if it's incompatible.  The only code that will
 * be emitted here is value checking code in strict builds.
 *
 * The intended use can only be readonly!
 *
 * @returns The host register number, UINT8_MAX if not present.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the instruction buffer offset.
 *                          Will be updated in strict builds if a register is
 *                          found.
 * @param   enmGstReg       The guest register that will is to be updated.
 * @note    In strict builds, this may throw instruction buffer growth failures.
 *          Non-strict builds will not throw anything.
 * @sa iemNativeRegAllocTmpForGuestReg
 */
DECL_HIDDEN_THROW(uint8_t)
iemNativeRegAllocTmpForGuestRegIfAlreadyPresent(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, IEMNATIVEGSTREG enmGstReg)
{
    Assert(enmGstReg < kIemNativeGstReg_End && g_aGstShadowInfo[enmGstReg].cb != 0);
#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
    AssertMsg(   pReNative->idxCurCall == 0
              || IEMLIVENESS_STATE_IS_INPUT_EXPECTED(iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstReg))
              || enmGstReg == kIemNativeGstReg_Pc,
              ("%s - %u\n", g_aGstShadowInfo[enmGstReg].pszName, iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstReg)));
#endif

    /*
     * First check if the guest register value is already in a host register.
     */
    if (pReNative->Core.bmGstRegShadows & RT_BIT_64(enmGstReg))
    {
        uint8_t idxReg = pReNative->Core.aidxGstRegShadows[enmGstReg];
        Assert(idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs));
        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows & RT_BIT_64(enmGstReg));
        Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

        if (!(pReNative->Core.bmHstRegs & RT_BIT_32(idxReg)))
        {
            /*
             * We only do readonly use here, so easy compared to the other
             * variant of this code.
             */
            pReNative->Core.bmHstRegs |= RT_BIT_32(idxReg);
            pReNative->Core.aHstRegs[idxReg].enmWhat = kIemNativeWhat_Tmp;
            pReNative->Core.aHstRegs[idxReg].idxVar  = UINT8_MAX;
            Log12(("iemNativeRegAllocTmpForGuestRegIfAlreadyPresent: Reusing %s for guest %s readonly\n",
                   g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName));

#ifdef VBOX_STRICT
            /* Strict builds: Check that the value is correct. */
            *poff = iemNativeEmitGuestRegValueCheck(pReNative, *poff, idxReg, enmGstReg);
#else
            RT_NOREF(poff);
#endif
            return idxReg;
        }
    }

    return UINT8_MAX;
}


/**
 * Allocates argument registers for a function call.
 *
 * @returns New code buffer offset on success; throws VBox status code on failure, so no
 *          need to check the return value.
 * @param   pReNative   The native recompile state.
 * @param   off         The current code buffer offset.
 * @param   cArgs       The number of arguments the function call takes.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeRegAllocArgs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs)
{
    AssertStmt(cArgs <= IEMNATIVE_CALL_ARG_GREG_COUNT + IEMNATIVE_FRAME_STACK_ARG_COUNT,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_4));
    Assert(RT_ELEMENTS(g_aidxIemNativeCallRegs) == IEMNATIVE_CALL_ARG_GREG_COUNT);
    Assert(RT_ELEMENTS(g_afIemNativeCallRegs) == IEMNATIVE_CALL_ARG_GREG_COUNT);

    if (cArgs > RT_ELEMENTS(g_aidxIemNativeCallRegs))
        cArgs = RT_ELEMENTS(g_aidxIemNativeCallRegs);
    else if (cArgs == 0)
        return true;

    /*
     * Do we get luck and all register are free and not shadowing anything?
     */
    if (((pReNative->Core.bmHstRegs | pReNative->Core.bmHstRegsWithGstShadow) & g_afIemNativeCallRegs[cArgs]) == 0)
        for (uint32_t i = 0; i < cArgs; i++)
        {
            uint8_t const idxReg = g_aidxIemNativeCallRegs[i];
            pReNative->Core.aHstRegs[idxReg].enmWhat = kIemNativeWhat_Arg;
            pReNative->Core.aHstRegs[idxReg].idxVar  = UINT8_MAX;
            Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows == 0);
        }
    /*
     * Okay, not lucky so we have to free up the registers.
     */
    else
        for (uint32_t i = 0; i < cArgs; i++)
        {
            uint8_t const idxReg = g_aidxIemNativeCallRegs[i];
            if (pReNative->Core.bmHstRegs & RT_BIT_32(idxReg))
            {
                switch (pReNative->Core.aHstRegs[idxReg].enmWhat)
                {
                    case kIemNativeWhat_Var:
                    {
                        uint8_t const idxVar = pReNative->Core.aHstRegs[idxReg].idxVar;
                        IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
                        AssertStmt(IEMNATIVE_VAR_IDX_UNPACK(idxVar) < RT_ELEMENTS(pReNative->Core.aVars),
                                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_5));
                        Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxReg);
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
                        Assert(!pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg);
#endif

                        if (pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].enmKind != kIemNativeVarKind_Stack)
                            pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg = UINT8_MAX;
                        else
                        {
                            off = iemNativeRegMoveOrSpillStackVar(pReNative, off, idxVar);
                            Assert(!(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg)));
                        }
                        break;
                    }

                    case kIemNativeWhat_Tmp:
                    case kIemNativeWhat_Arg:
                    case kIemNativeWhat_rc:
                        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_5));
                    default:
                        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_6));
                }

            }
            if (pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg))
            {
                Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows != 0);
                Assert(   (pReNative->Core.aHstRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstRegShadows)
                       == pReNative->Core.aHstRegs[idxReg].fGstRegShadows);
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                Assert(!(pReNative->Core.aHstRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstRegShadowDirty));
#endif
                pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);
                pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
                pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
            }
            else
                Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows == 0);
            pReNative->Core.aHstRegs[idxReg].enmWhat = kIemNativeWhat_Arg;
            pReNative->Core.aHstRegs[idxReg].idxVar  = UINT8_MAX;
        }
    pReNative->Core.bmHstRegs |= g_afIemNativeCallRegs[cArgs];
    return true;
}


DECL_HIDDEN_THROW(uint8_t)  iemNativeRegAssignRc(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg);


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
    Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aHstRegs));
    Assert(pReNative->Core.bmHstRegs & RT_BIT_32(idxHstReg));
    Assert(!(IEMNATIVE_REG_FIXED_MASK & RT_BIT_32(idxHstReg)));
    Assert(   pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Var
           || pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Tmp
           || pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Arg
           || pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_rc);
    Assert(   pReNative->Core.aHstRegs[idxHstReg].enmWhat != kIemNativeWhat_Var
           || pReNative->Core.aVars[pReNative->Core.aHstRegs[idxHstReg].idxVar].idxReg == UINT8_MAX
           || (pReNative->Core.bmVars & RT_BIT_32(pReNative->Core.aHstRegs[idxHstReg].idxVar)));
    Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows)
           == pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows);
    Assert(   RT_BOOL(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg))
           == RT_BOOL(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows));

    pReNative->Core.bmHstRegs              &= ~RT_BIT_32(idxHstReg);
    /* no flushing, right:
    pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
    pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
    pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = 0;
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
    Assert(pReNative->Core.bmHstRegs & RT_BIT_32(idxHstReg));
    Assert(pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Tmp);
    pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
    Log12(("iemNativeRegFreeTmp: %s (gst: %#RX64)\n",
           g_apszIemNativeHstRegNames[idxHstReg], pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows));
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
 * Frees a register assigned to a variable.
 *
 * The register will be disassociated from the variable.
 */
DECLHIDDEN(void) iemNativeRegFreeVar(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, bool fFlushShadows) RT_NOEXCEPT
{
    Assert(pReNative->Core.bmHstRegs & RT_BIT_32(idxHstReg));
    Assert(pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Var);
    uint8_t const idxVar = pReNative->Core.aHstRegs[idxHstReg].idxVar;
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxHstReg);
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    Assert(!pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg);
#endif

    pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg = UINT8_MAX;
    pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
    if (!fFlushShadows)
        Log12(("iemNativeRegFreeVar: %s (gst: %#RX64) idxVar=%#x\n",
               g_apszIemNativeHstRegNames[idxHstReg], pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows, idxVar));
    else
    {
        pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
        uint64_t const fGstRegShadowsOld        = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
        Assert(!(pReNative->Core.bmGstRegShadowDirty & fGstRegShadowsOld));
#endif
        pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = 0;
        pReNative->Core.bmGstRegShadows        &= ~fGstRegShadowsOld;
        uint64_t       fGstRegShadows           = fGstRegShadowsOld;
        while (fGstRegShadows)
        {
            unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegShadows) - 1;
            fGstRegShadows &= ~RT_BIT_64(idxGstReg);

            Assert(pReNative->Core.aidxGstRegShadows[idxGstReg] == idxHstReg);
            pReNative->Core.aidxGstRegShadows[idxGstReg] = UINT8_MAX;
        }
        Log12(("iemNativeRegFreeVar: %s (gst: %#RX64 -> 0) idxVar=%#x\n",
               g_apszIemNativeHstRegNames[idxHstReg], fGstRegShadowsOld, idxVar));
    }
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
# ifdef LOG_ENABLED
/** Host CPU SIMD register names. */
DECL_HIDDEN_CONST(const char * const) g_apszIemNativeHstSimdRegNames[] =
{
#  ifdef RT_ARCH_AMD64
    "ymm0", "ymm1", "ymm2", "ymm3", "ymm4", "ymm5", "ymm6", "ymm7", "ymm8", "ymm9", "ymm10", "ymm11", "ymm12", "ymm13", "ymm14", "ymm15"
#  elif RT_ARCH_ARM64
    "v0",  "v1",  "v2",  "v3",  "v4",  "v5",  "v6",  "v7",  "v8",  "v9",  "v10", "v11", "v12", "v13", "v14", "v15",
    "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31",
#  else
#   error "port me"
#  endif
};
# endif


/**
 * Frees a SIMD register assigned to a variable.
 *
 * The register will be disassociated from the variable.
 */
DECLHIDDEN(void) iemNativeSimdRegFreeVar(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, bool fFlushShadows) RT_NOEXCEPT
{
    Assert(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxHstReg));
    Assert(pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_Var);
    uint8_t const idxVar = pReNative->Core.aHstSimdRegs[idxHstReg].idxVar;
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxHstReg);
    Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg);

    pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg = UINT8_MAX;
    pReNative->Core.bmHstSimdRegs &= ~RT_BIT_32(idxHstReg);
    if (!fFlushShadows)
        Log12(("iemNativeSimdRegFreeVar: %s (gst: %#RX64) idxVar=%#x\n",
               g_apszIemNativeHstSimdRegNames[idxHstReg], pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows, idxVar));
    else
    {
        pReNative->Core.bmHstSimdRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
        uint64_t const fGstRegShadowsOld        = pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows;
        pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows = 0;
        pReNative->Core.bmGstSimdRegShadows    &= ~fGstRegShadowsOld;
        uint64_t       fGstRegShadows           = fGstRegShadowsOld;
        while (fGstRegShadows)
        {
            unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegShadows) - 1;
            fGstRegShadows &= ~RT_BIT_64(idxGstReg);

            Assert(pReNative->Core.aidxGstSimdRegShadows[idxGstReg] == idxHstReg);
            pReNative->Core.aidxGstSimdRegShadows[idxGstReg] = UINT8_MAX;
        }
        Log12(("iemNativeSimdRegFreeVar: %s (gst: %#RX64 -> 0) idxVar=%#x\n",
               g_apszIemNativeHstSimdRegNames[idxHstReg], fGstRegShadowsOld, idxVar));
    }
}


/**
 * Reassigns a variable to a different SIMD register specified by the caller.
 *
 * @returns The new code buffer position.
 * @param   pReNative       The native recompile state.
 * @param   off             The current code buffer position.
 * @param   idxVar          The variable index.
 * @param   idxRegOld       The old host register number.
 * @param   idxRegNew       The new host register number.
 * @param   pszCaller       The caller for logging.
 */
static uint32_t iemNativeSimdRegMoveVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVar,
                                        uint8_t idxRegOld, uint8_t idxRegNew, const char *pszCaller)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxRegOld);
    Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg);
    RT_NOREF(pszCaller);

    Assert(!(  (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
             & pReNative->Core.aHstSimdRegs[idxRegNew].fGstRegShadows));
    iemNativeSimdRegClearGstSimdRegShadowing(pReNative, idxRegNew, off);

    uint64_t fGstRegShadows = pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows;
    Assert(!(  (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
             & pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows));

    Log12(("%s: moving idxVar=%#x from %s to %s (fGstRegShadows=%RX64)\n",
           pszCaller, idxVar, g_apszIemNativeHstSimdRegNames[idxRegOld], g_apszIemNativeHstSimdRegNames[idxRegNew], fGstRegShadows));
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegNew, idxRegOld);

    if (pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar == sizeof(RTUINT128U))
        off = iemNativeEmitSimdLoadVecRegFromVecRegU128(pReNative, off, idxRegNew, idxRegOld);
    else
    {
        Assert(pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar == sizeof(RTUINT256U));
        off = iemNativeEmitSimdLoadVecRegFromVecRegU256(pReNative, off, idxRegNew, idxRegOld);
    }

    pReNative->Core.aHstSimdRegs[idxRegNew].fGstRegShadows = fGstRegShadows;
    pReNative->Core.aHstSimdRegs[idxRegNew].enmWhat        = kIemNativeWhat_Var;
    pReNative->Core.aHstSimdRegs[idxRegNew].idxVar         = idxVar;
    if (fGstRegShadows)
    {
        pReNative->Core.bmHstSimdRegsWithGstShadow = (pReNative->Core.bmHstSimdRegsWithGstShadow & ~RT_BIT_32(idxRegOld))
                                                   | RT_BIT_32(idxRegNew);
        while (fGstRegShadows)
        {
            unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegShadows) - 1;
            fGstRegShadows &= ~RT_BIT_64(idxGstReg);

            Assert(pReNative->Core.aidxGstSimdRegShadows[idxGstReg] == idxRegOld);
            pReNative->Core.aidxGstSimdRegShadows[idxGstReg] = idxRegNew;
        }
    }

    pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg = (uint8_t)idxRegNew;
    pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows = 0;
    pReNative->Core.bmHstSimdRegs = RT_BIT_32(idxRegNew) | (pReNative->Core.bmHstSimdRegs & ~RT_BIT_32(idxRegOld));
    return off;
}


/**
 * Moves a variable to a different register or spills it onto the stack.
 *
 * This must be a stack variable (kIemNativeVarKind_Stack) because the other
 * kinds can easily be recreated if needed later.
 *
 * @returns The new code buffer position.
 * @param   pReNative       The native recompile state.
 * @param   off             The current code buffer position.
 * @param   idxVar          The variable index.
 * @param   fForbiddenRegs  Mask of the forbidden registers.  Defaults to
 *                          call-volatile registers.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeSimdRegMoveOrSpillStackVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVar,
                                                                uint32_t fForbiddenRegs /*= IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK*/)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    Assert(pVar->enmKind == kIemNativeVarKind_Stack);
    Assert(!pVar->fRegAcquired);
    Assert(!pVar->fSimdReg);

    uint8_t const idxRegOld = pVar->idxReg;
    Assert(idxRegOld < RT_ELEMENTS(pReNative->Core.aHstSimdRegs));
    Assert(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxRegOld));
    Assert(pReNative->Core.aHstSimdRegs[idxRegOld].enmWhat == kIemNativeWhat_Var);
    Assert(   (pReNative->Core.bmGstSimdRegShadows & pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows)
           == pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows);
    Assert(pReNative->Core.bmGstSimdRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(   RT_BOOL(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxRegOld))
           == RT_BOOL(pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows));
    Assert(!(  (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
             & pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows));

    /** @todo Add statistics on this.*/
    /** @todo Implement basic variable liveness analysis (python) so variables
     * can be freed immediately once no longer used.  This has the potential to
     * be trashing registers and stack for dead variables.
     * Update: This is mostly done. (Not IEMNATIVE_WITH_LIVENESS_ANALYSIS.) */

    /*
     * First try move it to a different register, as that's cheaper.
     */
    fForbiddenRegs |= RT_BIT_32(idxRegOld);
    fForbiddenRegs |= IEMNATIVE_SIMD_REG_FIXED_MASK;
    uint32_t fRegs = ~pReNative->Core.bmHstSimdRegs & ~fForbiddenRegs;
    if (fRegs)
    {
        /* Avoid using shadow registers, if possible. */
        if (fRegs & ~pReNative->Core.bmHstSimdRegsWithGstShadow)
            fRegs &= ~pReNative->Core.bmHstSimdRegsWithGstShadow;
        unsigned const idxRegNew = ASMBitFirstSetU32(fRegs) - 1;
        return iemNativeSimdRegMoveVar(pReNative, off, idxVar, idxRegOld, idxRegNew, "iemNativeSimdRegMoveOrSpillStackVar");
    }

    /*
     * Otherwise we must spill the register onto the stack.
     */
    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
    Log12(("iemNativeSimdRegMoveOrSpillStackVar: spilling idxVar=%#x/idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
           idxVar, idxRegOld, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));

    if (pVar->cbVar == sizeof(RTUINT128U))
        off = iemNativeEmitStoreVecRegByBpU128(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);
    else
    {
        Assert(pVar->cbVar == sizeof(RTUINT256U));
        off = iemNativeEmitStoreVecRegByBpU256(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);
    }

    pVar->idxReg                                = UINT8_MAX;
    pReNative->Core.bmHstSimdRegsWithGstShadow &= ~RT_BIT_32(idxRegOld);
    pReNative->Core.bmHstSimdRegs              &= ~RT_BIT_32(idxRegOld);
    pReNative->Core.bmGstSimdRegShadows        &= ~pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows;
    pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows = 0;
    return off;
}


/**
 * Called right before emitting a call instruction to move anything important
 * out of call-volatile SIMD registers, free and flush the call-volatile SIMD registers,
 * optionally freeing argument variables.
 *
 * @returns New code buffer offset, UINT32_MAX on failure.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   cArgs           The number of arguments the function call takes.
 *                          It is presumed that the host register part of these have
 *                          been allocated as such already and won't need moving,
 *                          just freeing.
 * @param   fKeepVars       Mask of variables that should keep their register
 *                          assignments.  Caller must take care to handle these.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeSimdRegMoveAndFreeAndFlushAtCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs, uint32_t fKeepVars /*= 0*/)
{
    Assert(!cArgs); RT_NOREF(cArgs);

    /* fKeepVars will reduce this mask. */
    uint32_t fSimdRegsToFree = IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK;

    /*
     * Move anything important out of volatile registers.
     */
    uint32_t fSimdRegsToMove = IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
#ifdef IEMNATIVE_SIMD_REG_FIXED_TMP0
                             & ~RT_BIT_32(IEMNATIVE_SIMD_REG_FIXED_TMP0)
#endif
                             ;

    fSimdRegsToMove &= pReNative->Core.bmHstSimdRegs;
    if (!fSimdRegsToMove)
    { /* likely */ }
    else
    {
        Log12(("iemNativeSimdRegMoveAndFreeAndFlushAtCall: fSimdRegsToMove=%#x\n", fSimdRegsToMove));
        while (fSimdRegsToMove != 0)
        {
            unsigned const idxSimdReg = ASMBitFirstSetU32(fSimdRegsToMove) - 1;
            fSimdRegsToMove &= ~RT_BIT_32(idxSimdReg);

            switch (pReNative->Core.aHstSimdRegs[idxSimdReg].enmWhat)
            {
                case kIemNativeWhat_Var:
                {
                    uint8_t const       idxVar = pReNative->Core.aHstRegs[idxSimdReg].idxVar;
                    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
                    PIEMNATIVEVAR const pVar   = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
                    Assert(pVar->idxReg == idxSimdReg);
                    Assert(pVar->fSimdReg);
                    if (!(RT_BIT_32(IEMNATIVE_VAR_IDX_UNPACK(idxVar)) & fKeepVars))
                    {
                        Log12(("iemNativeSimdRegMoveAndFreeAndFlushAtCall: idxVar=%#x enmKind=%d idxSimdReg=%d\n",
                               idxVar, pVar->enmKind, pVar->idxReg));
                        if (pVar->enmKind != kIemNativeVarKind_Stack)
                            pVar->idxReg = UINT8_MAX;
                        else
                            off = iemNativeSimdRegMoveOrSpillStackVar(pReNative, off, idxVar);
                    }
                    else
                        fSimdRegsToFree &= ~RT_BIT_32(idxSimdReg);
                    continue;
                }

                case kIemNativeWhat_Arg:
                    AssertMsgFailed(("What?!?: %u\n", idxSimdReg));
                    continue;

                case kIemNativeWhat_rc:
                case kIemNativeWhat_Tmp:
                    AssertMsgFailed(("Missing free: %u\n", idxSimdReg));
                    continue;

                case kIemNativeWhat_FixedReserved:
#ifdef RT_ARCH_ARM64
                    continue; /* On ARM the upper half of the virtual 256-bit register. */
#endif

                case kIemNativeWhat_FixedTmp:
                case kIemNativeWhat_pVCpuFixed:
                case kIemNativeWhat_pCtxFixed:
                case kIemNativeWhat_PcShadow:
                case kIemNativeWhat_Invalid:
                case kIemNativeWhat_End:
                    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_1));
            }
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_2));
        }
    }

    /*
     * Do the actual freeing.
     */
    if (pReNative->Core.bmHstSimdRegs & fSimdRegsToFree)
        Log12(("iemNativeSimdRegMoveAndFreeAndFlushAtCall: bmHstSimdRegs %#x -> %#x\n",
               pReNative->Core.bmHstSimdRegs, pReNative->Core.bmHstSimdRegs & ~fSimdRegsToFree));
    pReNative->Core.bmHstSimdRegs &= ~fSimdRegsToFree;

    /* If there are guest register shadows in any call-volatile register, we
       have to clear the corrsponding guest register masks for each register. */
    uint32_t fHstSimdRegsWithGstShadow = pReNative->Core.bmHstSimdRegsWithGstShadow & fSimdRegsToFree;
    if (fHstSimdRegsWithGstShadow)
    {
        Log12(("iemNativeSimdRegMoveAndFreeAndFlushAtCall: bmHstSimdRegsWithGstShadow %#RX32 -> %#RX32; removed %#RX32\n",
               pReNative->Core.bmHstSimdRegsWithGstShadow, pReNative->Core.bmHstSimdRegsWithGstShadow & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK, fHstSimdRegsWithGstShadow));
        pReNative->Core.bmHstSimdRegsWithGstShadow &= ~fHstSimdRegsWithGstShadow;
        do
        {
            unsigned const idxSimdReg = ASMBitFirstSetU32(fHstSimdRegsWithGstShadow) - 1;
            fHstSimdRegsWithGstShadow &= ~RT_BIT_32(idxSimdReg);

            AssertMsg(pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows != 0, ("idxSimdReg=%#x\n", idxSimdReg));

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
            /*
             * Flush any pending writes now (might have been skipped earlier in iemEmitCallCommon() but it doesn't apply
             * to call volatile registers).
             */
            if (  (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                & pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows)
                off = iemNativeSimdRegFlushDirtyGuestByHostSimdRegShadow(pReNative, off, idxSimdReg);
#endif
            Assert(!(  (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                     & pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows));

            pReNative->Core.bmGstSimdRegShadows &= ~pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows;
            pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows = 0;
        } while (fHstSimdRegsWithGstShadow != 0);
    }

    return off;
}
#endif


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
 * @param   fKeepVars       Mask of variables that should keep their register
 *                          assignments.  Caller must take care to handle these.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeRegMoveAndFreeAndFlushAtCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs, uint32_t fKeepVars /*= 0*/)
{
    Assert(cArgs <= IEMNATIVE_CALL_MAX_ARG_COUNT);

    /* fKeepVars will reduce this mask. */
    uint32_t fRegsToFree = IEMNATIVE_CALL_VOLATILE_GREG_MASK;

    /*
     * Move anything important out of volatile registers.
     */
    if (cArgs > RT_ELEMENTS(g_aidxIemNativeCallRegs))
        cArgs = RT_ELEMENTS(g_aidxIemNativeCallRegs);
    uint32_t fRegsToMove = IEMNATIVE_CALL_VOLATILE_GREG_MASK
#ifdef IEMNATIVE_REG_FIXED_TMP0
                         & ~RT_BIT_32(IEMNATIVE_REG_FIXED_TMP0)
#endif
#ifdef IEMNATIVE_REG_FIXED_TMP1
                         & ~RT_BIT_32(IEMNATIVE_REG_FIXED_TMP1)
#endif
#ifdef IEMNATIVE_REG_FIXED_PC_DBG
                         & ~RT_BIT_32(IEMNATIVE_REG_FIXED_PC_DBG)
#endif
                         & ~g_afIemNativeCallRegs[cArgs];

    fRegsToMove &= pReNative->Core.bmHstRegs;
    if (!fRegsToMove)
    { /* likely */ }
    else
    {
        Log12(("iemNativeRegMoveAndFreeAndFlushAtCall: fRegsToMove=%#x\n", fRegsToMove));
        while (fRegsToMove != 0)
        {
            unsigned const idxReg = ASMBitFirstSetU32(fRegsToMove) - 1;
            fRegsToMove &= ~RT_BIT_32(idxReg);

            switch (pReNative->Core.aHstRegs[idxReg].enmWhat)
            {
                case kIemNativeWhat_Var:
                {
                    uint8_t const       idxVar = pReNative->Core.aHstRegs[idxReg].idxVar;
                    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
                    PIEMNATIVEVAR const pVar   = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
                    Assert(pVar->idxReg == idxReg);
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
                    Assert(!pVar->fSimdReg);
#endif
                    if (!(RT_BIT_32(IEMNATIVE_VAR_IDX_UNPACK(idxVar)) & fKeepVars))
                    {
                        Log12(("iemNativeRegMoveAndFreeAndFlushAtCall: idxVar=%#x enmKind=%d idxReg=%d\n",
                               idxVar, pVar->enmKind, pVar->idxReg));
                        if (pVar->enmKind != kIemNativeVarKind_Stack)
                            pVar->idxReg = UINT8_MAX;
                        else
                            off = iemNativeRegMoveOrSpillStackVar(pReNative, off, idxVar);
                    }
                    else
                        fRegsToFree &= ~RT_BIT_32(idxReg);
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
                case kIemNativeWhat_PcShadow:
                case kIemNativeWhat_FixedReserved:
                case kIemNativeWhat_Invalid:
                case kIemNativeWhat_End:
                    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_1));
            }
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_2));
        }
    }

    /*
     * Do the actual freeing.
     */
    if (pReNative->Core.bmHstRegs & fRegsToFree)
        Log12(("iemNativeRegMoveAndFreeAndFlushAtCall: bmHstRegs %#x -> %#x\n",
               pReNative->Core.bmHstRegs, pReNative->Core.bmHstRegs & ~fRegsToFree));
    pReNative->Core.bmHstRegs &= ~fRegsToFree;

    /* If there are guest register shadows in any call-volatile register, we
       have to clear the corrsponding guest register masks for each register. */
    uint32_t fHstRegsWithGstShadow = pReNative->Core.bmHstRegsWithGstShadow & fRegsToFree;
    if (fHstRegsWithGstShadow)
    {
        Log12(("iemNativeRegMoveAndFreeAndFlushAtCall: bmHstRegsWithGstShadow %#RX32 -> %#RX32; removed %#RX32\n",
               pReNative->Core.bmHstRegsWithGstShadow, pReNative->Core.bmHstRegsWithGstShadow & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK, fHstRegsWithGstShadow));
        pReNative->Core.bmHstRegsWithGstShadow &= ~fHstRegsWithGstShadow;
        do
        {
            unsigned const idxReg = ASMBitFirstSetU32(fHstRegsWithGstShadow) - 1;
            fHstRegsWithGstShadow &= ~RT_BIT_32(idxReg);

            AssertMsg(pReNative->Core.aHstRegs[idxReg].fGstRegShadows != 0, ("idxReg=%#x\n", idxReg));

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
            /*
             * Flush any pending writes now (might have been skipped earlier in iemEmitCallCommon() but it doesn't apply
             * to call volatile registers).
             */
            if (pReNative->Core.bmGstRegShadowDirty & pReNative->Core.aHstRegs[idxReg].fGstRegShadows)
                off = iemNativeRegFlushDirtyGuestByHostRegShadow(pReNative, off, idxReg);
            Assert(!(pReNative->Core.bmGstRegShadowDirty & pReNative->Core.aHstRegs[idxReg].fGstRegShadows));
#endif

            pReNative->Core.bmGstRegShadows &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
            pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
        } while (fHstRegsWithGstShadow != 0);
    }

#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    /* Now for the SIMD registers, no argument support for now. */
    off = iemNativeSimdRegMoveAndFreeAndFlushAtCall(pReNative, off, 0 /*cArgs*/, fKeepVars);
#endif

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
    uint64_t const bmGstRegShadowsOld = pReNative->Core.bmGstRegShadows;
    fGstRegs &= bmGstRegShadowsOld;
    if (fGstRegs)
    {
        uint64_t const bmGstRegShadowsNew = bmGstRegShadowsOld & ~fGstRegs;
        Log12(("iemNativeRegFlushGuestShadows: flushing %#RX64 (%#RX64 -> %#RX64)\n", fGstRegs, bmGstRegShadowsOld, bmGstRegShadowsNew));
        pReNative->Core.bmGstRegShadows = bmGstRegShadowsNew;
        if (bmGstRegShadowsNew)
        {
            /*
             * Partial.
             */
            do
            {
                unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegs) - 1;
                uint8_t const  idxHstReg = pReNative->Core.aidxGstRegShadows[idxGstReg];
                Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aidxGstRegShadows));
                Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg));
                Assert(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg));
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                Assert(!(pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(idxGstReg)));
#endif

                uint64_t const fInThisHstReg = (pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & fGstRegs) | RT_BIT_64(idxGstReg);
                fGstRegs &= ~fInThisHstReg;
                uint64_t const fGstRegShadowsNew = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & ~fInThisHstReg;
                pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = fGstRegShadowsNew;
                if (!fGstRegShadowsNew)
                    pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
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
                uint8_t const  idxHstReg = pReNative->Core.aidxGstRegShadows[idxGstReg];
                Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aidxGstRegShadows));
                Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg));
                Assert(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg));
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                Assert(!(pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(idxGstReg)));
#endif

                fGstRegs &= ~(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows | RT_BIT_64(idxGstReg));
                pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = 0;
            } while (fGstRegs != 0);
            pReNative->Core.bmHstRegsWithGstShadow = 0;
        }
    }
}


/**
 * Flushes guest register shadow copies held by a set of host registers.
 *
 * This is used with the TLB lookup code for ensuring that we don't carry on
 * with any guest shadows in volatile registers, as these will get corrupted by
 * a TLB miss.
 *
 * @param   pReNative       The native recompile state.
 * @param   fHstRegs        Set of host registers to flush guest shadows for.
 */
DECLHIDDEN(void) iemNativeRegFlushGuestShadowsByHostMask(PIEMRECOMPILERSTATE pReNative, uint32_t fHstRegs) RT_NOEXCEPT
{
    /*
     * Reduce the mask by what's currently shadowed.
     */
    uint32_t const bmHstRegsWithGstShadowOld = pReNative->Core.bmHstRegsWithGstShadow;
    fHstRegs &= bmHstRegsWithGstShadowOld;
    if (fHstRegs)
    {
        uint32_t const bmHstRegsWithGstShadowNew = bmHstRegsWithGstShadowOld & ~fHstRegs;
        Log12(("iemNativeRegFlushGuestShadowsByHostMask: flushing %#RX32 (%#RX32 -> %#RX32)\n",
               fHstRegs, bmHstRegsWithGstShadowOld, bmHstRegsWithGstShadowNew));
        pReNative->Core.bmHstRegsWithGstShadow = bmHstRegsWithGstShadowNew;
        if (bmHstRegsWithGstShadowNew)
        {
            /*
             * Partial (likely).
             */
            uint64_t fGstShadows = 0;
            do
            {
                unsigned const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;
                Assert(!(pReNative->Core.bmHstRegs & RT_BIT_32(idxHstReg)));
                Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows)
                       == pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows);
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                Assert(!(pReNative->Core.bmGstRegShadowDirty & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows));
#endif

                fGstShadows |= pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
                pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = 0;
                fHstRegs &= ~RT_BIT_32(idxHstReg);
            } while (fHstRegs != 0);
            pReNative->Core.bmGstRegShadows &= ~fGstShadows;
        }
        else
        {
            /*
             * Clear all.
             */
            do
            {
                unsigned const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;
                Assert(!(pReNative->Core.bmHstRegs & RT_BIT_32(idxHstReg)));
                Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows)
                       == pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows);
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                Assert(!(pReNative->Core.bmGstRegShadowDirty & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows));
#endif

                pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = 0;
                fHstRegs &= ~RT_BIT_32(idxHstReg);
            } while (fHstRegs != 0);
            pReNative->Core.bmGstRegShadows = 0;
        }
    }
}


/**
 * Restores guest shadow copies in volatile registers.
 *
 * This is used after calling a helper function (think TLB miss) to restore the
 * register state of volatile registers.
 *
 * @param   pReNative               The native recompile state.
 * @param   off                     The code buffer offset.
 * @param   fHstRegsActiveShadows   Set of host registers which are allowed to
 *                                  be active (allocated) w/o asserting. Hack.
 * @see     iemNativeVarSaveVolatileRegsPreHlpCall(),
 *          iemNativeVarRestoreVolatileRegsPostHlpCall()
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeRegRestoreGuestShadowsInVolatileRegs(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fHstRegsActiveShadows)
{
    uint32_t fHstRegs = pReNative->Core.bmHstRegsWithGstShadow & IEMNATIVE_CALL_VOLATILE_GREG_MASK;
    if (fHstRegs)
    {
        Log12(("iemNativeRegRestoreGuestShadowsInVolatileRegs: %#RX32\n", fHstRegs));
        do
        {
            unsigned const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;

            /* It's not fatal if a register is active holding a variable that
               shadowing a guest register, ASSUMING all pending guest register
               writes were flushed prior to the helper call. However, we'll be
               emitting duplicate restores, so it wasts code space. */
            Assert(!(pReNative->Core.bmHstRegs & ~fHstRegsActiveShadows & RT_BIT_32(idxHstReg)));
            RT_NOREF(fHstRegsActiveShadows);

            uint64_t const fGstRegShadows = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
            Assert(!(pReNative->Core.bmGstRegShadowDirty & fGstRegShadows));
#endif
            Assert((pReNative->Core.bmGstRegShadows & fGstRegShadows) == fGstRegShadows);
            AssertStmt(fGstRegShadows != 0 && fGstRegShadows < RT_BIT_64(kIemNativeGstReg_End),
                       IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_12));

            unsigned const idxGstReg = ASMBitFirstSetU64(fGstRegShadows) - 1;
            off = iemNativeEmitLoadGprWithGstShadowReg(pReNative, off, idxHstReg, (IEMNATIVEGSTREG)idxGstReg);

            fHstRegs &= ~RT_BIT_32(idxHstReg);
        } while (fHstRegs != 0);
    }
    return off;
}




/*********************************************************************************************************************************
*   SIMD register allocator (largely code duplication of the GPR allocator for now but might diverge)                            *
*********************************************************************************************************************************/
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR

/**
 * Info about shadowed guest SIMD register values.
 * @see IEMNATIVEGSTSIMDREG
 */
static struct
{
    /** Offset in VMCPU of XMM (low 128-bit) registers. */
    uint32_t    offXmm;
    /** Offset in VMCPU of YmmHi (high 128-bit) registers. */
    uint32_t    offYmm;
    /** Name (for logging). */
    const char *pszName;
} const g_aGstSimdShadowInfo[] =
{
#define CPUMCTX_OFF_AND_SIZE(a_iSimdReg) (uint32_t)RT_UOFFSETOF(VMCPU, cpum.GstCtx.XState.x87.aXMM[a_iSimdReg]), \
                                         (uint32_t)RT_UOFFSETOF(VMCPU, cpum.GstCtx.XState.u.YmmHi.aYmmHi[a_iSimdReg])
    /* [kIemNativeGstSimdReg_SimdRegFirst +  0] = */  { CPUMCTX_OFF_AND_SIZE(0),  "ymm0",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  1] = */  { CPUMCTX_OFF_AND_SIZE(1),  "ymm1",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  2] = */  { CPUMCTX_OFF_AND_SIZE(2),  "ymm2",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  3] = */  { CPUMCTX_OFF_AND_SIZE(3),  "ymm3",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  4] = */  { CPUMCTX_OFF_AND_SIZE(4),  "ymm4",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  5] = */  { CPUMCTX_OFF_AND_SIZE(5),  "ymm5",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  6] = */  { CPUMCTX_OFF_AND_SIZE(6),  "ymm6",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  7] = */  { CPUMCTX_OFF_AND_SIZE(7),  "ymm7",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  8] = */  { CPUMCTX_OFF_AND_SIZE(8),  "ymm8",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst +  9] = */  { CPUMCTX_OFF_AND_SIZE(9),  "ymm9",  },
    /* [kIemNativeGstSimdReg_SimdRegFirst + 10] = */  { CPUMCTX_OFF_AND_SIZE(10), "ymm10", },
    /* [kIemNativeGstSimdReg_SimdRegFirst + 11] = */  { CPUMCTX_OFF_AND_SIZE(11), "ymm11", },
    /* [kIemNativeGstSimdReg_SimdRegFirst + 12] = */  { CPUMCTX_OFF_AND_SIZE(12), "ymm12", },
    /* [kIemNativeGstSimdReg_SimdRegFirst + 13] = */  { CPUMCTX_OFF_AND_SIZE(13), "ymm13", },
    /* [kIemNativeGstSimdReg_SimdRegFirst + 14] = */  { CPUMCTX_OFF_AND_SIZE(14), "ymm14", },
    /* [kIemNativeGstSimdReg_SimdRegFirst + 15] = */  { CPUMCTX_OFF_AND_SIZE(15), "ymm15", },
#undef CPUMCTX_OFF_AND_SIZE
};
AssertCompile(RT_ELEMENTS(g_aGstSimdShadowInfo) == kIemNativeGstSimdReg_End);


/**
 * Frees a temporary SIMD register.
 *
 * Any shadow copies of guest registers assigned to the host register will not
 * be flushed by this operation.
 */
DECLHIDDEN(void) iemNativeSimdRegFreeTmp(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstSimdReg) RT_NOEXCEPT
{
    Assert(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxHstSimdReg));
    Assert(pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmWhat == kIemNativeWhat_Tmp);
    pReNative->Core.bmHstSimdRegs &= ~RT_BIT_32(idxHstSimdReg);
    Log12(("iemNativeSimdRegFreeTmp: %s (gst: %#RX64)\n",
           g_apszIemNativeHstSimdRegNames[idxHstSimdReg], pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows));
}


/**
 * Emits code to flush a pending write of the given SIMD register if any, also flushes the guest to host SIMD register association.
 *
 * @returns New code bufferoffset.
 * @param   pReNative       The native recompile state.
 * @param   off             Current code buffer position.
 * @param   enmGstSimdReg   The guest SIMD register to flush.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeSimdRegFlushPendingWrite(PIEMRECOMPILERSTATE pReNative, uint32_t off, IEMNATIVEGSTSIMDREG enmGstSimdReg)
{
    uint8_t const idxHstSimdReg = pReNative->Core.aidxGstSimdRegShadows[enmGstSimdReg];

    Log12(("iemNativeSimdRegFlushPendingWrite: Clearing guest register %s shadowed by host %s with state DirtyLo:%u DirtyHi:%u\n",
           g_aGstSimdShadowInfo[enmGstSimdReg].pszName, g_apszIemNativeHstSimdRegNames[idxHstSimdReg],
           IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_LO_U128(pReNative, enmGstSimdReg),
           IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_HI_U128(pReNative, enmGstSimdReg)));

    if (IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_LO_U128(pReNative, enmGstSimdReg))
    {
        Assert(   pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_256
               || pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_Low128);
        off = iemNativeEmitSimdStoreVecRegToVCpuLowU128(pReNative, off, idxHstSimdReg, g_aGstSimdShadowInfo[enmGstSimdReg].offXmm);
    }

    if (IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_HI_U128(pReNative, enmGstSimdReg))
    {
        Assert(   pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_256
               || pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_High128);
        off = iemNativeEmitSimdStoreVecRegToVCpuHighU128(pReNative, off, idxHstSimdReg, g_aGstSimdShadowInfo[enmGstSimdReg].offYmm);
    }

    IEMNATIVE_SIMD_REG_STATE_CLR_DIRTY(pReNative, enmGstSimdReg);
    return off;
}


/**
 * Flush the given set of guest SIMD registers if marked as dirty.
 *
 * @returns New code buffer offset.
 * @param   pReNative           The native recompile state.
 * @param   off                 Current code buffer position.
 * @param   fFlushGstSimdReg    The guest SIMD register set to flush (default is flush everything).
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeSimdRegFlushDirtyGuest(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint64_t fFlushGstSimdReg /*= UINT64_MAX*/)
{
    uint64_t bmGstSimdRegShadowDirty =   (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                                       & fFlushGstSimdReg;
    if (bmGstSimdRegShadowDirty)
    {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        iemNativeDbgInfoAddNativeOffset(pReNative, off);
        iemNativeDbgInfoAddGuestRegWriteback(pReNative, true /*fSimdReg*/, bmGstSimdRegShadowDirty);
# endif

        uint32_t idxGstSimdReg = 0;
        do
        {
            if (bmGstSimdRegShadowDirty & 0x1)
                off = iemNativeSimdRegFlushPendingWrite(pReNative, off, IEMNATIVEGSTSIMDREG_SIMD(idxGstSimdReg));

            idxGstSimdReg++;
            bmGstSimdRegShadowDirty >>= 1;
        } while (bmGstSimdRegShadowDirty);
    }

    return off;
}


#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
/**
 * Flush all shadowed guest SIMD registers marked as dirty for the given host SIMD register.
 *
 * @returns New code buffer offset.
 * @param   pReNative       The native recompile state.
 * @param   off             Current code buffer position.
 * @param   idxHstSimdReg   The host SIMD register.
 *
 * @note This doesn't do any unshadowing of guest registers from the host register.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeSimdRegFlushDirtyGuestByHostSimdRegShadow(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t const idxHstSimdReg)
{
    /* We need to flush any pending guest register writes this host register shadows. */
    uint64_t bmGstSimdRegShadowDirty =   (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                                       & pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows;
    if (bmGstSimdRegShadowDirty)
    {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        iemNativeDbgInfoAddNativeOffset(pReNative, off);
        iemNativeDbgInfoAddGuestRegWriteback(pReNative, true /*fSimdReg*/, bmGstSimdRegShadowDirty);
# endif

        uint32_t idxGstSimdReg = 0;
        do
        {
            if (bmGstSimdRegShadowDirty & 0x1)
                off = iemNativeSimdRegFlushPendingWrite(pReNative, off, IEMNATIVEGSTSIMDREG_SIMD(idxGstSimdReg));

            idxGstSimdReg++;
            bmGstSimdRegShadowDirty >>= 1;
        } while (bmGstSimdRegShadowDirty);
    }

    return off;
}
#endif


/**
 * Locate a register, possibly freeing one up.
 *
 * This ASSUMES the caller has done the minimal/optimal allocation checks and
 * failed.
 *
 * @returns Host register number on success. Returns UINT8_MAX if no registers
 *          found, the caller is supposed to deal with this and raise a
 *          allocation type specific status code (if desired).
 *
 * @throws  VBox status code if we're run into trouble spilling a variable of
 *          recording debug info.  Does NOT throw anything if we're out of
 *          registers, though.
 */
static uint8_t iemNativeSimdRegAllocFindFree(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, bool fPreferVolatile,
                                             uint32_t fRegMask = IEMNATIVE_HST_SIMD_REG_MASK & ~IEMNATIVE_SIMD_REG_FIXED_MASK)
{
    STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeSimdRegFindFree);
    Assert(!(fRegMask & ~IEMNATIVE_HST_SIMD_REG_MASK));
    Assert(!(fRegMask & IEMNATIVE_SIMD_REG_FIXED_MASK));

    /*
     * Try a freed register that's shadowing a guest register.
     */
    uint32_t fRegs = ~pReNative->Core.bmHstSimdRegs & fRegMask;
    if (fRegs)
    {
        STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeSimdRegFindFreeNoVar);

#if 0 /** @todo def IEMNATIVE_WITH_LIVENESS_ANALYSIS */
        /*
         * When we have livness information, we use it to kick out all shadowed
         * guest register that will not be needed any more in this TB.  If we're
         * lucky, this may prevent us from ending up here again.
         *
         * Note! We must consider the previous entry here so we don't free
         *       anything that the current threaded function requires (current
         *       entry is produced by the next threaded function).
         */
        uint32_t const idxCurCall = pReNative->idxCurCall;
        if (idxCurCall > 0)
        {
            PCIEMLIVENESSENTRY const pLivenessEntry = &pReNative->paLivenessEntries[idxCurCall - 1];

# ifndef IEMLIVENESS_EXTENDED_LAYOUT
            /* Construct a mask of the guest registers in the UNUSED and XCPT_OR_CALL state. */
            AssertCompile(IEMLIVENESS_STATE_UNUSED == 1 && IEMLIVENESS_STATE_XCPT_OR_CALL == 2);
            uint64_t fToFreeMask = pLivenessEntry->Bit0.bm64 ^ pLivenessEntry->Bit1.bm64; /* mask of regs in either UNUSED */
#else
            /* Construct a mask of the registers not in the read or write state.
               Note! We could skips writes, if they aren't from us, as this is just
                     a hack to prevent trashing registers that have just been written
                     or will be written when we retire the current instruction. */
            uint64_t fToFreeMask = ~pLivenessEntry->aBits[IEMLIVENESS_BIT_READ].bm64
                                 & ~pLivenessEntry->aBits[IEMLIVENESS_BIT_WRITE].bm64
                                 & IEMLIVENESSBIT_MASK;
#endif
            /* If it matches any shadowed registers. */
            if (pReNative->Core.bmGstRegShadows & fToFreeMask)
            {
                STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeSimdRegFindFreeLivenessUnshadowed);
                iemNativeRegFlushGuestShadows(pReNative, fToFreeMask);
                Assert(fRegs == (~pReNative->Core.bmHstRegs & fRegMask)); /* this shall not change. */

                /* See if we've got any unshadowed registers we can return now. */
                uint32_t const fUnshadowedRegs = fRegs & ~pReNative->Core.bmHstRegsWithGstShadow;
                if (fUnshadowedRegs)
                {
                    STAM_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeSimdRegFindFreeLivenessHelped);
                    return (fPreferVolatile
                            ? ASMBitFirstSetU32(fUnshadowedRegs)
                            : ASMBitLastSetU32(  fUnshadowedRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                               ? fUnshadowedRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fUnshadowedRegs))
                         - 1;
                }
            }
        }
#endif /* IEMNATIVE_WITH_LIVENESS_ANALYSIS */

        unsigned const idxReg = (fPreferVolatile
                                 ? ASMBitFirstSetU32(fRegs)
                                 : ASMBitLastSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
                                                    ? fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK : fRegs))
                              - 1;

        Assert(pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows != 0);
        Assert(   (pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstSimdRegShadows)
               == pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows);
        Assert(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxReg));

        /* We need to flush any pending guest register writes this host SIMD register shadows. */
        uint32_t fGstRegShadows = pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows;
        uint32_t idxGstSimdReg = 0;
        do
        {
            if (fGstRegShadows & 0x1)
            {
                *poff = iemNativeSimdRegFlushPendingWrite(pReNative, *poff, IEMNATIVEGSTSIMDREG_SIMD(idxGstSimdReg));
                Assert(!IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_U256(pReNative, idxGstSimdReg));
            }
            idxGstSimdReg++;
            fGstRegShadows >>= 1;
        } while (fGstRegShadows);

        pReNative->Core.bmHstSimdRegsWithGstShadow &= ~RT_BIT_32(idxReg);
        pReNative->Core.bmGstSimdRegShadows        &= ~pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows;
        pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows = 0;
        pReNative->Core.aHstSimdRegs[idxReg].enmLoaded      = kIemNativeGstSimdRegLdStSz_Invalid;
        return idxReg;
    }

    AssertFailed(); /** @todo The following needs testing when it actually gets hit. */

    /*
     * Try free up a variable that's in a register.
     *
     * We do two rounds here, first evacuating variables we don't need to be
     * saved on the stack, then in the second round move things to the stack.
     */
    STAM_REL_COUNTER_INC(&pReNative->pVCpu->iem.s.StatNativeSimdRegFindFreeVar);
    for (uint32_t iLoop = 0; iLoop < 2; iLoop++)
    {
        uint32_t fVars = pReNative->Core.bmVars;
        while (fVars)
        {
            uint32_t const idxVar = ASMBitFirstSetU32(fVars) - 1;
            uint8_t const  idxReg = pReNative->Core.aVars[idxVar].idxReg;
            if (!pReNative->Core.aVars[idxVar].fSimdReg) /* Ignore non SIMD variables here. */
                continue;

            if (   idxReg < RT_ELEMENTS(pReNative->Core.aHstSimdRegs)
                && (RT_BIT_32(idxReg) & fRegMask)
                && (  iLoop == 0
                    ? pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Stack
                    : pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                && !pReNative->Core.aVars[idxVar].fRegAcquired)
            {
                Assert(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxReg));
                Assert(   (pReNative->Core.bmGstSimdRegShadows & pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows)
                       == pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows);
                Assert(pReNative->Core.bmGstSimdRegShadows < RT_BIT_64(kIemNativeGstSimdReg_End));
                Assert(   RT_BOOL(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxReg))
                       == RT_BOOL(pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows));

                if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                {
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, IEMNATIVE_VAR_IDX_PACK(idxVar));
                    *poff = iemNativeEmitStoreGprByBp(pReNative, *poff, iemNativeStackCalcBpDisp(idxStackSlot), idxReg);
                }

                pReNative->Core.aVars[idxVar].idxReg        = UINT8_MAX;
                pReNative->Core.bmHstSimdRegs              &= ~RT_BIT_32(idxReg);

                pReNative->Core.bmHstSimdRegsWithGstShadow &= ~RT_BIT_32(idxReg);
                pReNative->Core.bmGstSimdRegShadows        &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
                pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows = 0;
                return idxReg;
            }
            fVars &= ~RT_BIT_32(idxVar);
        }
    }

    AssertFailed();
    return UINT8_MAX;
}


/**
 * Flushes a set of guest register shadow copies.
 *
 * This is usually done after calling a threaded function or a C-implementation
 * of an instruction.
 *
 * @param   pReNative       The native recompile state.
 * @param   fGstSimdRegs    Set of guest SIMD registers to flush.
 */
DECLHIDDEN(void) iemNativeSimdRegFlushGuestShadows(PIEMRECOMPILERSTATE pReNative, uint64_t fGstSimdRegs) RT_NOEXCEPT
{
    /*
     * Reduce the mask by what's currently shadowed
     */
    uint64_t const bmGstSimdRegShadows = pReNative->Core.bmGstSimdRegShadows;
    fGstSimdRegs &= bmGstSimdRegShadows;
    if (fGstSimdRegs)
    {
        uint64_t const bmGstSimdRegShadowsNew = bmGstSimdRegShadows & ~fGstSimdRegs;
        Log12(("iemNativeSimdRegFlushGuestShadows: flushing %#RX64 (%#RX64 -> %#RX64)\n", fGstSimdRegs, bmGstSimdRegShadows, bmGstSimdRegShadowsNew));
        pReNative->Core.bmGstSimdRegShadows = bmGstSimdRegShadowsNew;
        if (bmGstSimdRegShadowsNew)
        {
            /*
             * Partial.
             */
            do
            {
                unsigned const idxGstReg = ASMBitFirstSetU64(fGstSimdRegs) - 1;
                uint8_t const  idxHstReg = pReNative->Core.aidxGstSimdRegShadows[idxGstReg];
                Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aidxGstSimdRegShadows));
                Assert(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxHstReg));
                Assert(pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg));
                Assert(!IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_U256(pReNative, idxGstReg));

                uint64_t const fInThisHstReg = (pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows & fGstSimdRegs) | RT_BIT_64(idxGstReg);
                fGstSimdRegs &= ~fInThisHstReg;
                uint64_t const fGstRegShadowsNew = pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows & ~fInThisHstReg;
                pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows = fGstRegShadowsNew;
                if (!fGstRegShadowsNew)
                {
                    pReNative->Core.bmHstSimdRegsWithGstShadow        &= ~RT_BIT_32(idxHstReg);
                    pReNative->Core.aHstSimdRegs[idxHstReg].enmLoaded  = kIemNativeGstSimdRegLdStSz_Invalid;
                }
            } while (fGstSimdRegs != 0);
        }
        else
        {
            /*
             * Clear all.
             */
            do
            {
                unsigned const idxGstReg = ASMBitFirstSetU64(fGstSimdRegs) - 1;
                uint8_t const  idxHstReg = pReNative->Core.aidxGstSimdRegShadows[idxGstReg];
                Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aidxGstSimdRegShadows));
                Assert(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxHstReg));
                Assert(pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg));
                Assert(!IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_U256(pReNative, idxGstReg));

                fGstSimdRegs &= ~(pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows | RT_BIT_64(idxGstReg));
                pReNative->Core.aHstSimdRegs[idxHstReg].fGstRegShadows = 0;
                pReNative->Core.aHstSimdRegs[idxHstReg].enmLoaded      = kIemNativeGstSimdRegLdStSz_Invalid;
            } while (fGstSimdRegs != 0);
            pReNative->Core.bmHstSimdRegsWithGstShadow = 0;
        }
    }
}


/**
 * Allocates a temporary host SIMD register.
 *
 * This may emit code to save register content onto the stack in order to free
 * up a register.
 *
 * @returns The host register number; throws VBox status code on failure,
 *          so no need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 *                          This will be update if we need to move a variable from
 *                          register to stack in order to satisfy the request.
 * @param   fPreferVolatile Whether to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false, for iemNativeRegAllocTmpForGuestReg()).
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeSimdRegAllocTmp(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, bool fPreferVolatile /*= true*/)
{
    /*
     * Try find a completely unused register, preferably a call-volatile one.
     */
    uint8_t  idxSimdReg;
    uint32_t fRegs = ~pReNative->Core.bmHstRegs
                   & ~pReNative->Core.bmHstRegsWithGstShadow
                   & (~IEMNATIVE_SIMD_REG_FIXED_MASK & IEMNATIVE_HST_SIMD_REG_MASK);
    if (fRegs)
    {
        if (fPreferVolatile)
            idxSimdReg = (uint8_t)ASMBitFirstSetU32(  fRegs & IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
                                                    ? fRegs & IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK : fRegs) - 1;
        else
            idxSimdReg = (uint8_t)ASMBitFirstSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
                                                    ? fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK : fRegs) - 1;
        Assert(pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows == 0);
        Assert(!(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxSimdReg)));

        pReNative->Core.aHstSimdRegs[idxSimdReg].enmLoaded = kIemNativeGstSimdRegLdStSz_Invalid;
        Log12(("iemNativeSimdRegAllocTmp: %s\n", g_apszIemNativeHstSimdRegNames[idxSimdReg]));
    }
    else
    {
        idxSimdReg = iemNativeSimdRegAllocFindFree(pReNative, poff, fPreferVolatile);
        AssertStmt(idxSimdReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_TMP));
        Log12(("iemNativeSimdRegAllocTmp: %s (slow)\n", g_apszIemNativeHstSimdRegNames[idxSimdReg]));
    }

    Assert(pReNative->Core.aHstSimdRegs[idxSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_Invalid);
    return iemNativeSimdRegMarkAllocated(pReNative, idxSimdReg, kIemNativeWhat_Tmp);
}


/**
 * Alternative version of iemNativeSimdRegAllocTmp that takes mask with acceptable
 * registers.
 *
 * @returns The host register number; throws VBox status code on failure,
 *          so no need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer position.
 *                          This will be update if we need to move a variable from
 *                          register to stack in order to satisfy the request.
 * @param   fRegMask        Mask of acceptable registers.
 * @param   fPreferVolatile Whether to prefer volatile over non-volatile
 *                          registers (@c true, default) or the other way around
 *                          (@c false, for iemNativeRegAllocTmpForGuestReg()).
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeSimdRegAllocTmpEx(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint32_t fRegMask,
                                                      bool fPreferVolatile /*= true*/)
{
    Assert(!(fRegMask & ~IEMNATIVE_HST_SIMD_REG_MASK));
    Assert(!(fRegMask & IEMNATIVE_SIMD_REG_FIXED_MASK));

    /*
     * Try find a completely unused register, preferably a call-volatile one.
     */
    uint8_t  idxSimdReg;
    uint32_t fRegs = ~pReNative->Core.bmHstSimdRegs
                   & ~pReNative->Core.bmHstSimdRegsWithGstShadow
                   & (~IEMNATIVE_SIMD_REG_FIXED_MASK & IEMNATIVE_HST_SIMD_REG_MASK)
                   & fRegMask;
    if (fRegs)
    {
        if (fPreferVolatile)
            idxSimdReg = (uint8_t)ASMBitFirstSetU32(  fRegs & IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
                                                    ? fRegs & IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK : fRegs) - 1;
        else
            idxSimdReg = (uint8_t)ASMBitFirstSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
                                                    ? fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK : fRegs) - 1;
        Assert(pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows == 0);
        Assert(!(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxSimdReg)));

        pReNative->Core.aHstSimdRegs[idxSimdReg].enmLoaded = kIemNativeGstSimdRegLdStSz_Invalid;
        Log12(("iemNativeSimdRegAllocTmpEx: %s\n", g_apszIemNativeHstSimdRegNames[idxSimdReg]));
    }
    else
    {
        idxSimdReg = iemNativeSimdRegAllocFindFree(pReNative, poff, fPreferVolatile, fRegMask);
        AssertStmt(idxSimdReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_TMP));
        Log12(("iemNativeSimdRegAllocTmpEx: %s (slow)\n", g_apszIemNativeHstSimdRegNames[idxSimdReg]));
    }

    Assert(pReNative->Core.aHstSimdRegs[idxSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_Invalid);
    return iemNativeSimdRegMarkAllocated(pReNative, idxSimdReg, kIemNativeWhat_Tmp);
}


/**
 * Sets the indiactor for which part of the given SIMD register has valid data loaded.
 *
 * @param   pReNative       The native recompile state.
 * @param   idxHstSimdReg   The host SIMD register to update the state for.
 * @param   enmLoadSz       The load size to set.
 */
DECL_FORCE_INLINE(void) iemNativeSimdRegSetValidLoadFlag(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstSimdReg,
                                                         IEMNATIVEGSTSIMDREGLDSTSZ enmLoadSz)
{
    /* Everything valid already? -> nothing to do. */
    if (pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_256)
        return;

    if (pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_Invalid)
        pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded = enmLoadSz;
    else if (pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded != enmLoadSz)
    {
        Assert(   (   pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_Low128
                   && enmLoadSz == kIemNativeGstSimdRegLdStSz_High128)
               || (   pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded == kIemNativeGstSimdRegLdStSz_High128
                   && enmLoadSz == kIemNativeGstSimdRegLdStSz_Low128));
        pReNative->Core.aHstSimdRegs[idxHstSimdReg].enmLoaded = kIemNativeGstSimdRegLdStSz_256;
    }
}


static uint32_t iemNativeSimdRegAllocLoadVecRegFromVecRegSz(PIEMRECOMPILERSTATE pReNative, uint32_t off, IEMNATIVEGSTSIMDREG enmGstSimdRegDst,
                                                            uint8_t idxHstSimdRegDst, uint8_t idxHstSimdRegSrc, IEMNATIVEGSTSIMDREGLDSTSZ enmLoadSzDst)
{
    /* Easy case first, either the destination loads the same range as what the source has already loaded or the source has loaded everything. */
    if (   pReNative->Core.aHstSimdRegs[idxHstSimdRegSrc].enmLoaded == enmLoadSzDst
        || pReNative->Core.aHstSimdRegs[idxHstSimdRegSrc].enmLoaded == kIemNativeGstSimdRegLdStSz_256)
    {
# ifdef RT_ARCH_ARM64
        /* ASSUMES that there are two adjacent 128-bit registers available for the 256-bit value. */
        Assert(!(idxHstSimdRegDst & 0x1)); Assert(!(idxHstSimdRegSrc & 0x1));
# endif

        if (idxHstSimdRegDst != idxHstSimdRegSrc)
        {
            switch (enmLoadSzDst)
            {
                case kIemNativeGstSimdRegLdStSz_256:
                    off = iemNativeEmitSimdLoadVecRegFromVecRegU256(pReNative, off, idxHstSimdRegDst, idxHstSimdRegSrc);
                    break;
                case kIemNativeGstSimdRegLdStSz_Low128:
                    off = iemNativeEmitSimdLoadVecRegFromVecRegU128(pReNative, off, idxHstSimdRegDst, idxHstSimdRegSrc);
                    break;
                case kIemNativeGstSimdRegLdStSz_High128:
                    off = iemNativeEmitSimdLoadVecRegHighU128FromVecRegHighU128(pReNative, off, idxHstSimdRegDst, idxHstSimdRegSrc);
                    break;
                default:
                    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IPE_NOT_REACHED_DEFAULT_CASE));
            }

            iemNativeSimdRegSetValidLoadFlag(pReNative, idxHstSimdRegDst, enmLoadSzDst);
        }
    }
    else
    {
        /* The source doesn't has the part loaded, so load the register from CPUMCTX. */
        Assert(enmLoadSzDst == kIemNativeGstSimdRegLdStSz_Low128 || enmLoadSzDst == kIemNativeGstSimdRegLdStSz_High128);
        off = iemNativeEmitLoadSimdRegWithGstShadowSimdReg(pReNative, off, idxHstSimdRegDst, enmGstSimdRegDst, enmLoadSzDst);
    }

    return off;
}


/**
 * Allocates a temporary host SIMD register for keeping a guest
 * SIMD register value.
 *
 * Since we may already have a register holding the guest register value,
 * code will be emitted to do the loading if that's not the case. Code may also
 * be emitted if we have to free up a register to satify the request.
 *
 * @returns The host register number; throws VBox status code on failure, so no
 *          need to check the return value.
 * @param   pReNative       The native recompile state.
 * @param   poff            Pointer to the variable with the code buffer
 *                          position. This will be update if we need to move a
 *                          variable from register to stack in order to satisfy
 *                          the request.
 * @param   enmGstSimdReg   The guest SIMD register that will is to be updated.
 * @param   enmIntendedUse  How the caller will be using the host register.
 * @param   fNoVolatileRegs Set if no volatile register allowed, clear if any
 *                          register is okay (default).  The ASSUMPTION here is
 *                          that the caller has already flushed all volatile
 *                          registers, so this is only applied if we allocate a
 *                          new register.
 * @sa      iemNativeRegAllocTmpForGuestRegIfAlreadyPresent
 */
DECL_HIDDEN_THROW(uint8_t)
iemNativeSimdRegAllocTmpForGuestSimdReg(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, IEMNATIVEGSTSIMDREG enmGstSimdReg,
                                        IEMNATIVEGSTSIMDREGLDSTSZ enmLoadSz, IEMNATIVEGSTREGUSE enmIntendedUse /*= kIemNativeGstRegUse_ReadOnly*/,
                                        bool fNoVolatileRegs /*= false*/)
{
    Assert(enmGstSimdReg < kIemNativeGstSimdReg_End);
#if defined(IEMNATIVE_WITH_LIVENESS_ANALYSIS) && 0 /** @todo r=aeichner */
    AssertMsg(   pReNative->idxCurCall == 0
              || (enmIntendedUse == kIemNativeGstRegUse_ForFullWrite
                  ? IEMLIVENESS_STATE_IS_CLOBBER_EXPECTED(iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstSimdReg))
                  : enmIntendedUse == kIemNativeGstRegUse_ForUpdate
                  ? IEMLIVENESS_STATE_IS_MODIFY_EXPECTED( iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstSimdReg))
                  : IEMLIVENESS_STATE_IS_INPUT_EXPECTED(  iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstSimdReg)) ),
              ("%s - %u\n", g_aGstSimdShadowInfo[enmGstSimdReg].pszName, iemNativeLivenessGetPrevStateByGstReg(pReNative, enmGstSimdReg)));
#endif
#if defined(LOG_ENABLED) || defined(VBOX_STRICT)
    static const char * const s_pszIntendedUse[] = { "fetch", "update", "full write", "destructive calc" };
#endif
    uint32_t const fRegMask = !fNoVolatileRegs
                            ? IEMNATIVE_HST_SIMD_REG_MASK & ~IEMNATIVE_SIMD_REG_FIXED_MASK
                            : IEMNATIVE_HST_SIMD_REG_MASK & ~IEMNATIVE_SIMD_REG_FIXED_MASK & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK;

    /*
     * First check if the guest register value is already in a host register.
     */
    if (pReNative->Core.bmGstSimdRegShadows & RT_BIT_64(enmGstSimdReg))
    {
        uint8_t idxSimdReg = pReNative->Core.aidxGstSimdRegShadows[enmGstSimdReg];
        Assert(idxSimdReg < RT_ELEMENTS(pReNative->Core.aHstSimdRegs));
        Assert(pReNative->Core.aHstSimdRegs[idxSimdReg].fGstRegShadows & RT_BIT_64(enmGstSimdReg));
        Assert(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxSimdReg));

        /* It's not supposed to be allocated... */
        if (!(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxSimdReg)))
        {
            /*
             * If the register will trash the guest shadow copy, try find a
             * completely unused register we can use instead.  If that fails,
             * we need to disassociate the host reg from the guest reg.
             */
            /** @todo would be nice to know if preserving the register is in any way helpful. */
            /* If the purpose is calculations, try duplicate the register value as
               we'll be clobbering the shadow. */
            if (   enmIntendedUse == kIemNativeGstRegUse_Calculation
                && (  ~pReNative->Core.bmHstSimdRegs
                    & ~pReNative->Core.bmHstSimdRegsWithGstShadow
                    & (~IEMNATIVE_SIMD_REG_FIXED_MASK & IEMNATIVE_HST_SIMD_REG_MASK)))
            {
                uint8_t const idxRegNew = iemNativeSimdRegAllocTmpEx(pReNative, poff, fRegMask);

                *poff = iemNativeSimdRegAllocLoadVecRegFromVecRegSz(pReNative, *poff, enmGstSimdReg, idxRegNew, idxSimdReg, enmLoadSz);

                Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Duplicated %s for guest %s into %s for destructive calc\n",
                       g_apszIemNativeHstSimdRegNames[idxSimdReg], g_aGstSimdShadowInfo[enmGstSimdReg].pszName,
                       g_apszIemNativeHstSimdRegNames[idxRegNew]));
                idxSimdReg = idxRegNew;
            }
            /* If the current register matches the restrictions, go ahead and allocate
               it for the caller. */
            else if (fRegMask & RT_BIT_32(idxSimdReg))
            {
                pReNative->Core.bmHstSimdRegs |= RT_BIT_32(idxSimdReg);
                pReNative->Core.aHstSimdRegs[idxSimdReg].enmWhat = kIemNativeWhat_Tmp;
                if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
                {
                    if (enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
                        *poff = iemNativeSimdRegAllocLoadVecRegFromVecRegSz(pReNative, *poff, enmGstSimdReg, idxSimdReg, idxSimdReg, enmLoadSz);
                    else
                        iemNativeSimdRegSetValidLoadFlag(pReNative, idxSimdReg, enmLoadSz);
                    Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Reusing %s for guest %s %s\n",
                           g_apszIemNativeHstSimdRegNames[idxSimdReg], g_aGstSimdShadowInfo[enmGstSimdReg].pszName, s_pszIntendedUse[enmIntendedUse]));
                }
                else
                {
                    iemNativeSimdRegClearGstSimdRegShadowing(pReNative, idxSimdReg, *poff);
                    Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Grabbing %s for guest %s - destructive calc\n",
                           g_apszIemNativeHstSimdRegNames[idxSimdReg], g_aGstSimdShadowInfo[enmGstSimdReg].pszName));
                }
            }
            /* Otherwise, allocate a register that satisfies the caller and transfer
               the shadowing if compatible with the intended use.  (This basically
               means the call wants a non-volatile register (RSP push/pop scenario).) */
            else
            {
                Assert(fNoVolatileRegs);
                uint8_t const idxRegNew = iemNativeSimdRegAllocTmpEx(pReNative, poff, fRegMask & ~RT_BIT_32(idxSimdReg),
                                                                    !fNoVolatileRegs
                                                                 && enmIntendedUse == kIemNativeGstRegUse_Calculation);
                *poff = iemNativeSimdRegAllocLoadVecRegFromVecRegSz(pReNative, *poff, enmGstSimdReg, idxRegNew, idxSimdReg, enmLoadSz);
                if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
                {
                    iemNativeSimdRegTransferGstSimdRegShadowing(pReNative, idxSimdReg, idxRegNew, enmGstSimdReg, *poff);
                    Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Transfering %s to %s for guest %s %s\n",
                           g_apszIemNativeHstSimdRegNames[idxSimdReg], g_apszIemNativeHstSimdRegNames[idxRegNew],
                           g_aGstSimdShadowInfo[enmGstSimdReg].pszName, s_pszIntendedUse[enmIntendedUse]));
                }
                else
                    Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Duplicated %s for guest %s into %s for destructive calc\n",
                           g_apszIemNativeHstSimdRegNames[idxSimdReg], g_aGstSimdShadowInfo[enmGstSimdReg].pszName,
                           g_apszIemNativeHstSimdRegNames[idxRegNew]));
                idxSimdReg = idxRegNew;
            }
        }
        else
        {
            /*
             * Oops. Shadowed guest register already allocated!
             *
             * Allocate a new register, copy the value and, if updating, the
             * guest shadow copy assignment to the new register.
             */
            AssertMsg(   enmIntendedUse != kIemNativeGstRegUse_ForUpdate
                      && enmIntendedUse != kIemNativeGstRegUse_ForFullWrite,
                      ("This shouldn't happen: idxSimdReg=%d enmGstSimdReg=%d enmIntendedUse=%s\n",
                       idxSimdReg, enmGstSimdReg, s_pszIntendedUse[enmIntendedUse]));

            /** @todo share register for readonly access. */
            uint8_t const idxRegNew = iemNativeSimdRegAllocTmpEx(pReNative, poff, fRegMask,
                                                                 enmIntendedUse == kIemNativeGstRegUse_Calculation);

            if (enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
                *poff = iemNativeSimdRegAllocLoadVecRegFromVecRegSz(pReNative, *poff, enmGstSimdReg, idxRegNew, idxSimdReg, enmLoadSz);
            else
                iemNativeSimdRegSetValidLoadFlag(pReNative, idxRegNew, enmLoadSz);

            if (   enmIntendedUse != kIemNativeGstRegUse_ForUpdate
                && enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
                Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Duplicated %s for guest %s into %s for %s\n",
                       g_apszIemNativeHstSimdRegNames[idxSimdReg], g_aGstSimdShadowInfo[enmGstSimdReg].pszName,
                       g_apszIemNativeHstSimdRegNames[idxRegNew], s_pszIntendedUse[enmIntendedUse]));
            else
            {
                iemNativeSimdRegTransferGstSimdRegShadowing(pReNative, idxSimdReg, idxRegNew, enmGstSimdReg, *poff);
                Log12(("iemNativeSimdRegAllocTmpForGuestSimdReg: Moved %s for guest %s into %s for %s\n",
                       g_apszIemNativeHstSimdRegNames[idxSimdReg], g_aGstSimdShadowInfo[enmGstSimdReg].pszName,
                       g_apszIemNativeHstSimdRegNames[idxRegNew], s_pszIntendedUse[enmIntendedUse]));
            }
            idxSimdReg = idxRegNew;
        }
        Assert(RT_BIT_32(idxSimdReg) & fRegMask); /* See assumption in fNoVolatileRegs docs. */

#ifdef VBOX_STRICT
        /* Strict builds: Check that the value is correct. */
        if (enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
            *poff = iemNativeEmitGuestSimdRegValueCheck(pReNative, *poff, idxSimdReg, enmGstSimdReg, enmLoadSz);
#endif

        if (   enmIntendedUse == kIemNativeGstRegUse_ForFullWrite
            || enmIntendedUse == kIemNativeGstRegUse_ForUpdate)
        {
# if defined(IEMNATIVE_WITH_TB_DEBUG_INFO) && defined(IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK)
            iemNativeDbgInfoAddNativeOffset(pReNative, *poff);
            iemNativeDbgInfoAddGuestRegDirty(pReNative, true /*fSimdReg*/, enmGstSimdReg, idxSimdReg);
# endif

            if (enmLoadSz == kIemNativeGstSimdRegLdStSz_Low128)
                IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_LO_U128(pReNative, enmGstSimdReg);
            else if (enmLoadSz == kIemNativeGstSimdRegLdStSz_High128)
                IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_HI_U128(pReNative, enmGstSimdReg);
            else
            {
                Assert(enmLoadSz == kIemNativeGstSimdRegLdStSz_256);
                IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_LO_U128(pReNative, enmGstSimdReg);
                IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_HI_U128(pReNative, enmGstSimdReg);
            }
        }

        return idxSimdReg;
    }

    /*
     * Allocate a new register, load it with the guest value and designate it as a copy of the
     */
    uint8_t const idxRegNew = iemNativeSimdRegAllocTmpEx(pReNative, poff, fRegMask, enmIntendedUse == kIemNativeGstRegUse_Calculation);

    if (enmIntendedUse != kIemNativeGstRegUse_ForFullWrite)
        *poff = iemNativeEmitLoadSimdRegWithGstShadowSimdReg(pReNative, *poff, idxRegNew, enmGstSimdReg, enmLoadSz);
    else
        iemNativeSimdRegSetValidLoadFlag(pReNative, idxRegNew, enmLoadSz);

    if (enmIntendedUse != kIemNativeGstRegUse_Calculation)
        iemNativeSimdRegMarkAsGstSimdRegShadow(pReNative, idxRegNew, enmGstSimdReg, *poff);

    if (   enmIntendedUse == kIemNativeGstRegUse_ForFullWrite
        || enmIntendedUse == kIemNativeGstRegUse_ForUpdate)
    {
# if defined(IEMNATIVE_WITH_TB_DEBUG_INFO) && defined(IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK)
        iemNativeDbgInfoAddNativeOffset(pReNative, *poff);
        iemNativeDbgInfoAddGuestRegDirty(pReNative, true /*fSimdReg*/, enmGstSimdReg, idxRegNew);
# endif

        if (enmLoadSz == kIemNativeGstSimdRegLdStSz_Low128)
            IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_LO_U128(pReNative, enmGstSimdReg);
        else if (enmLoadSz == kIemNativeGstSimdRegLdStSz_High128)
            IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_HI_U128(pReNative, enmGstSimdReg);
        else
        {
            Assert(enmLoadSz == kIemNativeGstSimdRegLdStSz_256);
            IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_LO_U128(pReNative, enmGstSimdReg);
            IEMNATIVE_SIMD_REG_STATE_SET_DIRTY_HI_U128(pReNative, enmGstSimdReg);
        }
    }

    Log12(("iemNativeRegAllocTmpForGuestSimdReg: Allocated %s for guest %s %s\n",
           g_apszIemNativeHstSimdRegNames[idxRegNew], g_aGstSimdShadowInfo[enmGstSimdReg].pszName, s_pszIntendedUse[enmIntendedUse]));

    return idxRegNew;
}


/**
 * Flushes guest SIMD register shadow copies held by a set of host registers.
 *
 * This is used whenever calling an external helper for ensuring that we don't carry on
 * with any guest shadows in volatile registers, as these will get corrupted by the caller.
 *
 * @param   pReNative       The native recompile state.
 * @param   fHstSimdRegs    Set of host SIMD registers to flush guest shadows for.
 */
DECLHIDDEN(void) iemNativeSimdRegFlushGuestShadowsByHostMask(PIEMRECOMPILERSTATE pReNative, uint32_t fHstSimdRegs) RT_NOEXCEPT
{
    /*
     * Reduce the mask by what's currently shadowed.
     */
    uint32_t const bmHstSimdRegsWithGstShadowOld = pReNative->Core.bmHstSimdRegsWithGstShadow;
    fHstSimdRegs &= bmHstSimdRegsWithGstShadowOld;
    if (fHstSimdRegs)
    {
        uint32_t const bmHstSimdRegsWithGstShadowNew = bmHstSimdRegsWithGstShadowOld & ~fHstSimdRegs;
        Log12(("iemNativeSimdRegFlushGuestShadowsByHostMask: flushing %#RX32 (%#RX32 -> %#RX32)\n",
               fHstSimdRegs, bmHstSimdRegsWithGstShadowOld, bmHstSimdRegsWithGstShadowNew));
        pReNative->Core.bmHstSimdRegsWithGstShadow = bmHstSimdRegsWithGstShadowNew;
        if (bmHstSimdRegsWithGstShadowNew)
        {
            /*
             * Partial (likely).
             */
            uint64_t fGstShadows = 0;
            do
            {
                unsigned const idxHstSimdReg = ASMBitFirstSetU32(fHstSimdRegs) - 1;
                Assert(!(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxHstSimdReg)));
                Assert(   (pReNative->Core.bmGstSimdRegShadows & pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows)
                       == pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows);
                Assert(!((  pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                          & pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows));

                fGstShadows |= pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows;
                pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows = 0;
                fHstSimdRegs &= ~RT_BIT_32(idxHstSimdReg);
            } while (fHstSimdRegs != 0);
            pReNative->Core.bmGstSimdRegShadows &= ~fGstShadows;
        }
        else
        {
            /*
             * Clear all.
             */
            do
            {
                unsigned const idxHstSimdReg = ASMBitFirstSetU32(fHstSimdRegs) - 1;
                Assert(!(pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxHstSimdReg)));
                Assert(   (pReNative->Core.bmGstSimdRegShadows & pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows)
                       == pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows);
                Assert(!(  (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                         & pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows));

                pReNative->Core.aHstSimdRegs[idxHstSimdReg].fGstRegShadows = 0;
                fHstSimdRegs &= ~RT_BIT_32(idxHstSimdReg);
            } while (fHstSimdRegs != 0);
            pReNative->Core.bmGstSimdRegShadows = 0;
        }
    }
}
#endif /* IEMNATIVE_WITH_SIMD_REG_ALLOCATOR */



/*********************************************************************************************************************************
*   Code emitters for flushing pending guest register writes and sanity checks                                                   *
*********************************************************************************************************************************/

#ifdef VBOX_STRICT
/**
 * Does internal register allocator sanity checks.
 */
DECLHIDDEN(void) iemNativeRegAssertSanity(PIEMRECOMPILERSTATE pReNative)
{
    /*
     * Iterate host registers building a guest shadowing set.
     */
    uint64_t bmGstRegShadows        = 0;
    uint32_t bmHstRegsWithGstShadow = pReNative->Core.bmHstRegsWithGstShadow;
    AssertMsg(!(bmHstRegsWithGstShadow & IEMNATIVE_REG_FIXED_MASK), ("%#RX32\n", bmHstRegsWithGstShadow));
    while (bmHstRegsWithGstShadow)
    {
        unsigned const idxHstReg = ASMBitFirstSetU32(bmHstRegsWithGstShadow) - 1;
        Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aHstRegs));
        bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);

        uint64_t fThisGstRegShadows = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
        AssertMsg(fThisGstRegShadows != 0, ("idxHstReg=%d\n", idxHstReg));
        AssertMsg(fThisGstRegShadows < RT_BIT_64(kIemNativeGstReg_End), ("idxHstReg=%d %#RX64\n", idxHstReg, fThisGstRegShadows));
        bmGstRegShadows |= fThisGstRegShadows;
        while (fThisGstRegShadows)
        {
            unsigned const idxGstReg = ASMBitFirstSetU64(fThisGstRegShadows) - 1;
            fThisGstRegShadows &= ~RT_BIT_64(idxGstReg);
            AssertMsg(pReNative->Core.aidxGstRegShadows[idxGstReg] == idxHstReg,
                      ("idxHstReg=%d aidxGstRegShadows[idxGstReg=%d]=%d\n",
                       idxHstReg, idxGstReg, pReNative->Core.aidxGstRegShadows[idxGstReg]));
        }
    }
    AssertMsg(bmGstRegShadows == pReNative->Core.bmGstRegShadows,
              ("%RX64 vs %RX64; diff %RX64\n", bmGstRegShadows, pReNative->Core.bmGstRegShadows,
               bmGstRegShadows ^ pReNative->Core.bmGstRegShadows));

    /*
     * Now the other way around, checking the guest to host index array.
     */
    bmHstRegsWithGstShadow = 0;
    bmGstRegShadows        = pReNative->Core.bmGstRegShadows;
    Assert(bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    while (bmGstRegShadows)
    {
        unsigned const idxGstReg = ASMBitFirstSetU64(bmGstRegShadows) - 1;
        Assert(idxGstReg < RT_ELEMENTS(pReNative->Core.aidxGstRegShadows));
        bmGstRegShadows &= ~RT_BIT_64(idxGstReg);

        uint8_t const idxHstReg = pReNative->Core.aidxGstRegShadows[idxGstReg];
        AssertMsg(idxHstReg < RT_ELEMENTS(pReNative->Core.aHstRegs), ("aidxGstRegShadows[%d]=%d\n", idxGstReg, idxHstReg));
        AssertMsg(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & RT_BIT_64(idxGstReg),
                  ("idxGstReg=%d idxHstReg=%d fGstRegShadows=%RX64\n",
                   idxGstReg, idxHstReg, pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows));
        bmHstRegsWithGstShadow |= RT_BIT_32(idxHstReg);
    }
    AssertMsg(bmHstRegsWithGstShadow == pReNative->Core.bmHstRegsWithGstShadow,
              ("%RX64 vs %RX64; diff %RX64\n", bmHstRegsWithGstShadow, pReNative->Core.bmHstRegsWithGstShadow,
               bmHstRegsWithGstShadow ^ pReNative->Core.bmHstRegsWithGstShadow));
}
#endif /* VBOX_STRICT */


/**
 * Flushes any delayed guest register writes.
 *
 * This must be called prior to calling CImpl functions and any helpers that use
 * the guest state (like raising exceptions) and such.
 *
 * @note This function does not flush any shadowing information for guest registers. This needs to be done by
 *       the caller if it wishes to do so.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeRegFlushPendingWritesSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint64_t fGstShwExcept, uint64_t fGstSimdShwExcept)
{
#ifdef IEMNATIVE_WITH_DELAYED_PC_UPDATING
    if (!(fGstShwExcept & kIemNativeGstReg_Pc))
        off = iemNativeEmitPcWriteback(pReNative, off);
#else
    RT_NOREF(pReNative, fGstShwExcept);
#endif

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    off = iemNativeRegFlushDirtyGuest(pReNative, off, ~fGstShwExcept);
#endif

#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    off = iemNativeSimdRegFlushDirtyGuest(pReNative, off, ~fGstSimdShwExcept);
#endif

    return off;
}


#ifdef IEMNATIVE_WITH_DELAYED_PC_UPDATING
/**
 * Emits code to update the guest RIP value by adding the current offset since the start of the last RIP update.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeEmitPcWritebackSlow(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    Assert(pReNative->Core.offPc);
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    iemNativeDbgInfoAddNativeOffset(pReNative, off);
    iemNativeDbgInfoAddDelayedPcUpdate(pReNative, pReNative->Core.offPc, pReNative->Core.cInstrPcUpdateSkipped);
# endif

# ifndef IEMNATIVE_REG_FIXED_PC_DBG
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGprImm(pReNative, off, idxPcReg, pReNative->Core.offPc);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);
# else
    /* Compare the shadow with the context value, they should match. */
    off = iemNativeEmitAddGprImm(pReNative, off, IEMNATIVE_REG_FIXED_PC_DBG, pReNative->Core.offPc);
    off = iemNativeEmitGuestRegValueCheck(pReNative, off, IEMNATIVE_REG_FIXED_PC_DBG, kIemNativeGstReg_Pc);
# endif

    STAM_COUNTER_ADD(&pReNative->pVCpu->iem.s.StatNativePcUpdateDelayed, pReNative->Core.cInstrPcUpdateSkipped);
    pReNative->Core.offPc                 = 0;
    pReNative->Core.cInstrPcUpdateSkipped = 0;

    return off;
}
#endif /* IEMNATIVE_WITH_DELAYED_PC_UPDATING */


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
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitLoadGprWithGstShadowReg(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg)
{
    Assert((unsigned)enmGstReg < (unsigned)kIemNativeGstReg_End);
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
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IPE_NOT_REACHED_DEFAULT_CASE));
    }
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Loads the guest shadow SIMD register @a enmGstSimdReg into host SIMD reg @a idxHstSimdReg.
 *
 * @returns New code buffer offset on success, UINT32_MAX on failure.
 * @param   pReNative       The recompiler state.
 * @param   off             The current code buffer position.
 * @param   idxHstSimdReg   The host register to load the guest register value into.
 * @param   enmGstSimdReg   The guest register to load.
 * @param   enmLoadSz       The load size of the register.
 *
 * @note This does not mark @a idxHstReg as having a shadow copy of @a enmGstReg,
 *       that is something the caller needs to do if applicable.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitLoadSimdRegWithGstShadowSimdReg(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxHstSimdReg,
                                             IEMNATIVEGSTSIMDREG enmGstSimdReg, IEMNATIVEGSTSIMDREGLDSTSZ enmLoadSz)
{
    Assert((unsigned)enmGstSimdReg < RT_ELEMENTS(g_aGstSimdShadowInfo));

    iemNativeSimdRegSetValidLoadFlag(pReNative, idxHstSimdReg, enmLoadSz);
    switch (enmLoadSz)
    {
        case kIemNativeGstSimdRegLdStSz_256:
            off = iemNativeEmitSimdLoadVecRegFromVCpuLowU128(pReNative, off, idxHstSimdReg, g_aGstSimdShadowInfo[enmGstSimdReg].offXmm);
            return iemNativeEmitSimdLoadVecRegFromVCpuHighU128(pReNative, off, idxHstSimdReg, g_aGstSimdShadowInfo[enmGstSimdReg].offYmm);
        case kIemNativeGstSimdRegLdStSz_Low128:
            return iemNativeEmitSimdLoadVecRegFromVCpuLowU128(pReNative, off, idxHstSimdReg, g_aGstSimdShadowInfo[enmGstSimdReg].offXmm);
        case kIemNativeGstSimdRegLdStSz_High128:
            return iemNativeEmitSimdLoadVecRegFromVCpuHighU128(pReNative, off, idxHstSimdReg, g_aGstSimdShadowInfo[enmGstSimdReg].offYmm);
        default:
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IPE_NOT_REACHED_DEFAULT_CASE));
    }
}
#endif /* IEMNATIVE_WITH_SIMD_REG_ALLOCATOR */

#ifdef VBOX_STRICT

/**
 * Emitting code that checks that the value of @a idxReg is UINT32_MAX or less.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitTop32BitsClearCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxReg)
{
# ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 20);

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

# elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    /* lsr tmp0, reg64, #32 */
    pu32CodeBuf[off++] = Armv8A64MkInstrLsrImm(IEMNATIVE_REG_FIXED_TMP0, idxReg, 32);
    /* cbz tmp0, +1 */
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 2, IEMNATIVE_REG_FIXED_TMP0);
    /* brk #0x1100 */
    pu32CodeBuf[off++] = Armv8A64MkInstrBrk(UINT32_C(0x1100));

# else
#  error "Port me!"
# endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/**
 * Emitting code that checks that the content of register @a idxReg is the same
 * as what's in the guest register @a enmGstReg, resulting in a breakpoint
 * instruction if that's not the case.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitGuestRegValueCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxReg, IEMNATIVEGSTREG enmGstReg)
{
#if defined(IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK)
    /* We can't check the value against whats in CPUMCTX if the register is already marked as dirty, so skip the check. */
    if (pReNative->Core.bmGstRegShadowDirty & RT_BIT_64(enmGstReg))
        return off;
#endif

# ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);

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
                AssertStmt(g_aGstShadowInfo[enmGstReg].cb == sizeof(uint32_t),
                           IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_LABEL_IPE_6));
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
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
    else
    {
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        if (g_aGstShadowInfo[enmGstReg].cb == sizeof(uint32_t))
            iemNativeEmitTop32BitsClearCheck(pReNative, off, idxReg);
    }

# elif defined(RT_ARCH_ARM64)
    /* mov TMP0, [gstreg] */
    off = iemNativeEmitLoadGprWithGstShadowReg(pReNative, off, IEMNATIVE_REG_FIXED_TMP0, enmGstReg);

    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
    /* sub tmp0, tmp0, idxReg */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(true /*fSub*/, IEMNATIVE_REG_FIXED_TMP0, IEMNATIVE_REG_FIXED_TMP0, idxReg);
    /* cbz tmp0, +1 */
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 2, IEMNATIVE_REG_FIXED_TMP0);
    /* brk #0x1000+enmGstReg */
    pu32CodeBuf[off++] = Armv8A64MkInstrBrk((uint32_t)enmGstReg | UINT32_C(0x1000));
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

# else
#  error "Port me!"
# endif
    return off;
}


# ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
#  ifdef RT_ARCH_AMD64
/**
 * Helper for AMD64 to emit code which checks the low 128-bits of the given SIMD register against the given vCPU offset.
 */
DECL_FORCE_INLINE_THROW(uint32_t) iemNativeEmitGuestSimdRegValueCheckVCpuU128(uint8_t * const pbCodeBuf, uint32_t off, uint8_t idxSimdReg, uint32_t offVCpu)
{
    /* pcmpeqq vectmp0, [gstreg] (ASSUMES SSE4.1) */
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (idxSimdReg >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0x38;
    pbCodeBuf[off++] = 0x29;
    off = iemNativeEmitGprByVCpuDisp(pbCodeBuf, off, idxSimdReg, offVCpu);

    /* pextrq tmp0, vectmp0, #0 (ASSUMES SSE4.1). */
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    pbCodeBuf[off++] =   X86_OP_REX_W
                       | (idxSimdReg < 8 ? 0 : X86_OP_REX_R)
                       | (IEMNATIVE_REG_FIXED_TMP0 < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0x3a;
    pbCodeBuf[off++] = 0x16;
    pbCodeBuf[off++] = 0xeb;
    pbCodeBuf[off++] = 0x00;

    /* cmp tmp0, 0xffffffffffffffff. */
    pbCodeBuf[off++] = X86_OP_REX_W | (IEMNATIVE_REG_FIXED_TMP0 < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x83;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, IEMNATIVE_REG_FIXED_TMP0 & 7);
    pbCodeBuf[off++] = 0xff;

    /* je/jz +1 */
    pbCodeBuf[off++] = 0x74;
    pbCodeBuf[off++] = 0x01;

    /* int3 */
    pbCodeBuf[off++] = 0xcc;

    /* pextrq tmp0, vectmp0, #1 (ASSUMES SSE4.1). */
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    pbCodeBuf[off++] =   X86_OP_REX_W
                       | (idxSimdReg < 8 ? 0 : X86_OP_REX_R)
                       | (IEMNATIVE_REG_FIXED_TMP0 < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x0f;
    pbCodeBuf[off++] = 0x3a;
    pbCodeBuf[off++] = 0x16;
    pbCodeBuf[off++] = 0xeb;
    pbCodeBuf[off++] = 0x01;

    /* cmp tmp0, 0xffffffffffffffff. */
    pbCodeBuf[off++] = X86_OP_REX_W | (IEMNATIVE_REG_FIXED_TMP0 < 8 ? 0 : X86_OP_REX_B);
    pbCodeBuf[off++] = 0x83;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 7, IEMNATIVE_REG_FIXED_TMP0 & 7);
    pbCodeBuf[off++] = 0xff;

    /* je/jz +1 */
    pbCodeBuf[off++] = 0x74;
    pbCodeBuf[off++] = 0x01;

    /* int3 */
    pbCodeBuf[off++] = 0xcc;

    return off;
}
#  endif


/**
 * Emitting code that checks that the content of SIMD register @a idxSimdReg is the same
 * as what's in the guest register @a enmGstSimdReg, resulting in a breakpoint
 * instruction if that's not the case.
 *
 * @note May of course trash IEMNATIVE_SIMD_REG_FIXED_TMP0 and IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitGuestSimdRegValueCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxSimdReg,
                                    IEMNATIVEGSTSIMDREG enmGstSimdReg, IEMNATIVEGSTSIMDREGLDSTSZ enmLoadSz)
{
    /* We can't check the value against whats in CPUMCTX if the register is already marked as dirty, so skip the check. */
    if (   (   enmLoadSz == kIemNativeGstSimdRegLdStSz_256
            && (   IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_LO_U128(pReNative, enmGstSimdReg)
                || IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_HI_U128(pReNative, enmGstSimdReg)))
        || (   enmLoadSz == kIemNativeGstSimdRegLdStSz_Low128
            && IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_LO_U128(pReNative, enmGstSimdReg))
        || (   enmLoadSz == kIemNativeGstSimdRegLdStSz_High128
            && IEMNATIVE_SIMD_REG_STATE_IS_DIRTY_HI_U128(pReNative, enmGstSimdReg)))
        return off;

#  ifdef RT_ARCH_AMD64
    if (enmLoadSz == kIemNativeGstSimdRegLdStSz_Low128 || enmLoadSz == kIemNativeGstSimdRegLdStSz_256)
    {
        /* movdqa vectmp0, idxSimdReg */
        off = iemNativeEmitSimdLoadVecRegFromVecRegU128(pReNative, off, IEMNATIVE_SIMD_REG_FIXED_TMP0, idxSimdReg);

        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 44);

        off = iemNativeEmitGuestSimdRegValueCheckVCpuU128(pbCodeBuf, off, IEMNATIVE_SIMD_REG_FIXED_TMP0,
                                                          g_aGstSimdShadowInfo[enmGstSimdReg].offXmm);
    }

    if (enmLoadSz == kIemNativeGstSimdRegLdStSz_High128 || enmLoadSz == kIemNativeGstSimdRegLdStSz_256)
    {
        /* Due to the fact that CPUMCTX stores the high 128-bit separately we need to do this all over again for the high part. */
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 50);

        /* vextracti128 vectmp0, idxSimdReg, 1 */
        pbCodeBuf[off++] = X86_OP_VEX3;
        pbCodeBuf[off++] =   (idxSimdReg < 8 ? X86_OP_VEX3_BYTE1_R : 0)
                           | X86_OP_VEX3_BYTE1_X
                           | (IEMNATIVE_SIMD_REG_FIXED_TMP0 < 8 ? X86_OP_VEX3_BYTE1_B : 0)
                           | 0x03; /* Opcode map */
        pbCodeBuf[off++] = X86_OP_VEX3_BYTE2_MAKE_NO_VVVV(false /*f64BitOpSz*/, true /*f256BitAvx*/, X86_OP_VEX3_BYTE2_P_066H);
        pbCodeBuf[off++] = 0x39;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxSimdReg & 7, IEMNATIVE_SIMD_REG_FIXED_TMP0 & 7);
        pbCodeBuf[off++] = 0x01;

        off = iemNativeEmitGuestSimdRegValueCheckVCpuU128(pbCodeBuf, off, IEMNATIVE_SIMD_REG_FIXED_TMP0,
                                                          g_aGstSimdShadowInfo[enmGstSimdReg].offYmm);
    }
#  elif defined(RT_ARCH_ARM64)
    /* mov vectmp0, [gstreg] */
    off = iemNativeEmitLoadSimdRegWithGstShadowSimdReg(pReNative, off, IEMNATIVE_SIMD_REG_FIXED_TMP0, enmGstSimdReg, enmLoadSz);

    if (enmLoadSz == kIemNativeGstSimdRegLdStSz_Low128 || enmLoadSz == kIemNativeGstSimdRegLdStSz_256)
    {
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
        /* eor vectmp0, vectmp0, idxSimdReg */
        pu32CodeBuf[off++] = Armv8A64MkVecInstrEor(IEMNATIVE_SIMD_REG_FIXED_TMP0, IEMNATIVE_SIMD_REG_FIXED_TMP0, idxSimdReg);
        /* uaddlv vectmp0, vectmp0.16B */
        pu32CodeBuf[off++] = Armv8A64MkVecInstrUAddLV(IEMNATIVE_SIMD_REG_FIXED_TMP0, IEMNATIVE_SIMD_REG_FIXED_TMP0, kArmv8InstrUAddLVSz_16B);
        /* umov tmp0, vectmp0.H[0] */
        pu32CodeBuf[off++] = Armv8A64MkVecInstrUmov(IEMNATIVE_REG_FIXED_TMP0, IEMNATIVE_SIMD_REG_FIXED_TMP0,
                                                    0 /*idxElem*/, kArmv8InstrUmovInsSz_U16, false /*f64Bit*/);
        /* cbz tmp0, +1 */
        pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 2, IEMNATIVE_REG_FIXED_TMP0);
        /* brk #0x1000+enmGstReg */
        pu32CodeBuf[off++] = Armv8A64MkInstrBrk((uint32_t)enmGstSimdReg | UINT32_C(0x1000));
    }

    if (enmLoadSz == kIemNativeGstSimdRegLdStSz_High128 || enmLoadSz == kIemNativeGstSimdRegLdStSz_256)
    {
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
        /* eor vectmp0 + 1, vectmp0 + 1, idxSimdReg */
        pu32CodeBuf[off++] = Armv8A64MkVecInstrEor(IEMNATIVE_SIMD_REG_FIXED_TMP0 + 1, IEMNATIVE_SIMD_REG_FIXED_TMP0 + 1, idxSimdReg + 1);
        /* uaddlv vectmp0 + 1, (vectmp0 + 1).16B */
        pu32CodeBuf[off++] = Armv8A64MkVecInstrUAddLV(IEMNATIVE_SIMD_REG_FIXED_TMP0 + 1, IEMNATIVE_SIMD_REG_FIXED_TMP0 + 1, kArmv8InstrUAddLVSz_16B);
        /* umov tmp0, (vectmp0 + 1).H[0] */
        pu32CodeBuf[off++] = Armv8A64MkVecInstrUmov(IEMNATIVE_REG_FIXED_TMP0, IEMNATIVE_SIMD_REG_FIXED_TMP0 + 1,
                                                    0 /*idxElem*/, kArmv8InstrUmovInsSz_U16, false /*f64Bit*/);
        /* cbz tmp0, +1 */
        pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(false /*fJmpIfNotZero*/, 2, IEMNATIVE_REG_FIXED_TMP0);
        /* brk #0x1000+enmGstReg */
        pu32CodeBuf[off++] = Armv8A64MkInstrBrk((uint32_t)enmGstSimdReg | UINT32_C(0x1000));
    }

#  else
#   error "Port me!"
#  endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}
# endif /* IEMNATIVE_WITH_SIMD_REG_ALLOCATOR */


/**
 * Emitting code that checks that IEMCPU::fExec matches @a fExec for all
 * important bits.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitExecFlagsCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fExec)
{
    uint8_t const idxRegTmp = iemNativeRegAllocTmp(pReNative, &off);
    off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, idxRegTmp, RT_UOFFSETOF(VMCPUCC, iem.s.fExec));
    off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxRegTmp, IEMTB_F_IEM_F_MASK & IEMTB_F_KEY_MASK);
    off = iemNativeEmitCmpGpr32WithImm(pReNative, off, idxRegTmp, fExec & IEMTB_F_KEY_MASK);

#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);

    /* je/jz +1 */
    pbCodeBuf[off++] = 0x74;
    pbCodeBuf[off++] = 0x01;

    /* int3 */
    pbCodeBuf[off++] = 0xcc;

# elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);

    /* b.eq +1 */
    pu32CodeBuf[off++] = Armv8A64MkInstrBCond(kArmv8InstrCond_Eq, 2);
    /* brk #0x2000 */
    pu32CodeBuf[off++] = Armv8A64MkInstrBrk(UINT32_C(0x2000));

# else
#  error "Port me!"
# endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeRegFreeTmp(pReNative, idxRegTmp);
    return off;
}

#endif /* VBOX_STRICT */


#ifdef IEMNATIVE_STRICT_EFLAGS_SKIPPING
/**
 * Worker for IEMNATIVE_STRICT_EFLAGS_SKIPPING_EMIT_CHECK.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitEFlagsSkippingCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fEflNeeded)
{
    uint32_t const offVCpu = RT_UOFFSETOF(VMCPU, iem.s.fSkippingEFlags);

    fEflNeeded &= X86_EFL_STATUS_BITS;
    if (fEflNeeded)
    {
# ifdef RT_ARCH_AMD64
        /* test dword [pVCpu + offVCpu], imm32 */
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
        if (fEflNeeded <= 0xff)
        {
            pCodeBuf[off++] = 0xf6;
            off = iemNativeEmitGprByVCpuDisp(pCodeBuf, off, 0, offVCpu);
            pCodeBuf[off++] = RT_BYTE1(fEflNeeded);
        }
        else
        {
            pCodeBuf[off++] = 0xf7;
            off = iemNativeEmitGprByVCpuDisp(pCodeBuf, off, 0, offVCpu);
            pCodeBuf[off++] = RT_BYTE1(fEflNeeded);
            pCodeBuf[off++] = RT_BYTE2(fEflNeeded);
            pCodeBuf[off++] = RT_BYTE3(fEflNeeded);
            pCodeBuf[off++] = RT_BYTE4(fEflNeeded);
        }
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

# else
        uint8_t const idxRegTmp = iemNativeRegAllocTmp(pReNative, &off);
        off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, idxRegTmp, offVCpu);
        off = iemNativeEmitTestAnyBitsInGpr(pReNative, off, idxRegTmp, fEflNeeded);
#  ifdef RT_ARCH_ARM64
        off = iemNativeEmitJzToFixed(pReNative, off, off + 2);
        off = iemNativeEmitBrk(pReNative, off, 0x7777);
#  else
#   error "Port me!"
#  endif
        iemNativeRegFreeTmp(pReNative, idxRegTmp);
# endif
    }
    return off;
}
#endif /* IEMNATIVE_STRICT_EFLAGS_SKIPPING */


/**
 * Emits a code for checking the return code of a call and rcPassUp, returning
 * from the code if either are non-zero.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCheckCallRetAndPassUp(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr)
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
# ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitLoadGprImm64(pReNative, off, ARMV8_A64_REG_X2, idxInstr);
# endif
    off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, ARMV8_A64_REG_X3, RT_UOFFSETOF(VMCPUCC, iem.s.rcPassUp));

    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);

    pu32CodeBuf[off++] = Armv8A64MkInstrOrr(ARMV8_A64_REG_X4, ARMV8_A64_REG_X3, ARMV8_A64_REG_X0, false /*f64Bit*/);

    uint32_t const idxLabel = iemNativeLabelCreate(pReNative, kIemNativeLabelType_NonZeroRetOrPassUp);
    iemNativeAddFixup(pReNative, off, idxLabel, kIemNativeFixupType_RelImm19At5);
    pu32CodeBuf[off++] = Armv8A64MkInstrCbzCbnz(true /*fJmpIfNotZero*/, 0, ARMV8_A64_REG_X4, false /*f64Bit*/);

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    RT_NOREF_PV(idxInstr);
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
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCheckGprCanonicalMaybeRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxAddrReg, uint8_t idxInstr)
{
    /*
     * Make sure we don't have any outstanding guest register writes as we may
     * raise an #GP(0) and all guest register must be up to date in CPUMCTX.
     */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

#ifdef RT_ARCH_AMD64
    /*
     * if ((((uint32_t)(a_u64Addr >> 32) + UINT32_C(0x8000)) >> 16) != 0)
     *     return raisexcpt();
     * ---- this wariant avoid loading a 64-bit immediate, but is an instruction longer.
     */
    uint8_t const iTmpReg = iemNativeRegAllocTmp(pReNative, &off);

    off = iemNativeEmitLoadGprFromGpr(pReNative, off, iTmpReg, idxAddrReg);
    off = iemNativeEmitShiftGprRight(pReNative, off, iTmpReg, 32);
    off = iemNativeEmitAddGpr32Imm(pReNative, off, iTmpReg, (int32_t)0x8000);
    off = iemNativeEmitShiftGprRight(pReNative, off, iTmpReg, 16);
    off = iemNativeEmitJnzToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);

    iemNativeRegFreeTmp(pReNative, iTmpReg);

#elif defined(RT_ARCH_ARM64)
    /*
     * if ((((uint64_t)(a_u64Addr) + UINT64_C(0x800000000000)) >> 48) != 0)
     *     return raisexcpt();
     * ----
     *     mov     x1, 0x800000000000
     *     add     x1, x0, x1
     *     cmp     xzr, x1, lsr 48
     *     b.ne    .Lraisexcpt
     */
    uint8_t const iTmpReg = iemNativeRegAllocTmp(pReNative, &off);

    off = iemNativeEmitLoadGprImm64(pReNative, off, iTmpReg, UINT64_C(0x800000000000));
    off = iemNativeEmitAddTwoGprs(pReNative, off, iTmpReg, idxAddrReg);
    off = iemNativeEmitCmpArm64(pReNative, off, ARMV8_A64_REG_XZR, iTmpReg, true /*f64Bit*/, 48 /*cShift*/, kArmv8A64InstrShift_Lsr);
    off = iemNativeEmitJnzToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);

    iemNativeRegFreeTmp(pReNative, iTmpReg);

#else
# error "Port me"
#endif
    return off;
}


/**
 * Emits code to check if that the content of @a idxAddrReg is within the limit
 * of CS, raising a \#GP(0) if it isn't.
 *
 * @returns New code buffer offset; throws VBox status code on error.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   idxAddrReg      The host register (32-bit) with the address to
 *                          check.
 * @param   idxInstr        The current instruction.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCheckGpr32AgainstCsSegLimitMaybeRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                      uint8_t idxAddrReg, uint8_t idxInstr)
{
    /*
     * Make sure we don't have any outstanding guest register writes as we may
     * raise an #GP(0) and all guest register must be up to date in CPUMCTX.
     */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

    uint8_t const idxRegCsLim = iemNativeRegAllocTmpForGuestReg(pReNative, &off,
                                                                (IEMNATIVEGSTREG)(kIemNativeGstReg_SegLimitFirst + X86_SREG_CS),
                                                                kIemNativeGstRegUse_ReadOnly);

    off = iemNativeEmitCmpGpr32WithGpr(pReNative, off, idxAddrReg, idxRegCsLim);
    off = iemNativeEmitJaToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);

    iemNativeRegFreeTmp(pReNative, idxRegCsLim);
    return off;
}


/**
 * Emits a call to a CImpl function or something similar.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCImplCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr, uint64_t fGstShwFlush, uintptr_t pfnCImpl,
                       uint8_t cbInstr, uint8_t cAddParams, uint64_t uParam0, uint64_t uParam1, uint64_t uParam2)
{
    /* Writeback everything. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /*
     * Flush stuff. PC and EFlags are implictly flushed, the latter because we
     * don't do with/without flags variants of defer-to-cimpl stuff at the moment.
     */
    fGstShwFlush = iemNativeCImplFlagsToGuestShadowFlushMask(pReNative->fCImpl,
                                                             fGstShwFlush
                                                             | RT_BIT_64(kIemNativeGstReg_Pc)
                                                             | RT_BIT_64(kIemNativeGstReg_EFlags));
    iemNativeRegFlushGuestShadows(pReNative, fGstShwFlush);

    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 4);

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
    off = iemNativeEmitLeaGprByBp(pReNative, off, X86_GREG_xCX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict */

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
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitThreadedCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)
{
    IEMNATIVE_STRICT_EFLAGS_SKIPPING_EMIT_CHECK(pReNative, off, X86_EFL_STATUS_BITS);

    /* We don't know what the threaded function is doing so we must flush all pending writes. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    iemNativeRegFlushGuestShadows(pReNative, UINT64_MAX); /** @todo optimize this */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 4);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    /* The threaded function may throw / long jmp, so set current instruction
       number if we're counting. */
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, pCallEntry->idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#endif

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
    {
        off = iemNativeEmitLoadGprImm64(pReNative, off, X86_GREG_x10, pCallEntry->auParams[2]);
        off = iemNativeEmitStoreGprByBp(pReNative, off, IEMNATIVE_FP_OFF_STACK_ARG0, X86_GREG_x10);
    }
    off = iemNativeEmitLeaGprByBp(pReNative, off, X86_GREG_xCX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict */
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

    return off;
}

#ifdef VBOX_WITH_STATISTICS
/**
 * Emits code to update the thread call statistics.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitThreadCallStats(PIEMRECOMPILERSTATE pReNative, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)
{
    /*
     * Update threaded function stats.
     */
    uint32_t const offVCpu = RT_UOFFSETOF_DYN(VMCPUCC, iem.s.acThreadedFuncStats[pCallEntry->enmFunction]);
    AssertCompile(sizeof(pReNative->pVCpu->iem.s.acThreadedFuncStats[pCallEntry->enmFunction]) == sizeof(uint32_t));
# if defined(RT_ARCH_ARM64)
    uint8_t const idxTmp1 = iemNativeRegAllocTmp(pReNative, &off);
    uint8_t const idxTmp2 = iemNativeRegAllocTmp(pReNative, &off);
    off = iemNativeEmitIncU32CounterInVCpu(pReNative, off, idxTmp1, idxTmp2, offVCpu);
    iemNativeRegFreeTmp(pReNative, idxTmp1);
    iemNativeRegFreeTmp(pReNative, idxTmp2);
# else
    off = iemNativeEmitIncU32CounterInVCpu(pReNative, off, UINT8_MAX, UINT8_MAX, offVCpu);
# endif
    return off;
}
#endif /* VBOX_WITH_STATISTICS */


/**
 * Emits the code at the ReturnWithFlags label (returns
 * VINF_IEM_REEXEC_FINISH_WITH_FLAGS).
 */
static uint32_t iemNativeEmitReturnWithFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_ReturnWithFlags);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_RET_GREG, VINF_IEM_REEXEC_FINISH_WITH_FLAGS);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
    }
    return off;
}


/**
 * Emits the code at the ReturnBreak label (returns VINF_IEM_REEXEC_BREAK).
 */
static uint32_t iemNativeEmitReturnBreak(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_ReturnBreak);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

        off = iemNativeEmitLoadGprImm64(pReNative, off, IEMNATIVE_CALL_RET_GREG, VINF_IEM_REEXEC_BREAK);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
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
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_NonZeroRetOrPassUp);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

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
static uint32_t iemNativeEmitEpilog(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t *pidxReturnLabel)
{
    *pidxReturnLabel = UINT32_MAX;

    /* Flush any pending writes before returning from the last instruction (RIP updates, etc.). */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /*
     * Successful return, so clear the return register (eax, w0).
     */
    off = iemNativeEmitGprZero(pReNative,off, IEMNATIVE_CALL_RET_GREG);

    /*
     * Define label for common return point.
     */
    uint32_t const idxReturn = iemNativeLabelCreate(pReNative, kIemNativeLabelType_Return, off);
    *pidxReturnLabel = idxReturn;

    IEMNATIVE_STRICT_EFLAGS_SKIPPING_EMIT_CHECK(pReNative, off, X86_EFL_STATUS_BITS);

    /*
     * Restore registers and return.
     */
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 20);

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
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);

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
    uint8_t *const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
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

# ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER_LONGJMP
    /* Save the frame pointer. */
    off = iemNativeEmitStoreGprToVCpuU64Ex(pbCodeBuf, off, X86_GREG_xBP, RT_UOFFSETOF(VMCPUCC, iem.s.pvTbFramePointerR3));
# endif

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
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 16);

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
    off = iemNativeEmitLoadGprFromGprEx(pu32CodeBuf, off, IEMNATIVE_REG_FIXED_PVMCPU, IEMNATIVE_CALL_ARG0_GREG);
    /* mov r27, r1  */
    off = iemNativeEmitLoadGprFromGprEx(pu32CodeBuf, off, IEMNATIVE_REG_FIXED_PCPUMCTX, IEMNATIVE_CALL_ARG1_GREG);

# ifdef VBOX_WITH_IEM_NATIVE_RECOMPILER_LONGJMP
    /* Save the frame pointer. */
    off = iemNativeEmitStoreGprToVCpuU64Ex(pu32CodeBuf, off, ARMV8_A64_REG_BP, RT_UOFFSETOF(VMCPUCC, iem.s.pvTbFramePointerR3),
                                           ARMV8_A64_REG_X2);
# endif

#else
# error "port me"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    return off;
}


/*********************************************************************************************************************************
*   Emitters for IEM_MC_ARG_XXX, IEM_MC_LOCAL, IEM_MC_LOCAL_CONST, ++                                                            *
*********************************************************************************************************************************/

/**
 * Internal work that allocates a variable with kind set to
 * kIemNativeVarKind_Invalid and no current stack allocation.
 *
 * The kind will either be set by the caller or later when the variable is first
 * assigned a value.
 *
 * @returns Unpacked index.
 * @internal
 */
static uint8_t iemNativeVarAllocInt(PIEMRECOMPILERSTATE pReNative, uint8_t cbType)
{
    Assert(cbType > 0 && cbType <= 64);
    unsigned const idxVar = ASMBitFirstSetU32(~pReNative->Core.bmVars) - 1;
    AssertStmt(idxVar < RT_ELEMENTS(pReNative->Core.aVars), IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_EXHAUSTED));
    pReNative->Core.bmVars |= RT_BIT_32(idxVar);
    pReNative->Core.aVars[idxVar].enmKind        = kIemNativeVarKind_Invalid;
    pReNative->Core.aVars[idxVar].cbVar          = cbType;
    pReNative->Core.aVars[idxVar].idxStackSlot   = UINT8_MAX;
    pReNative->Core.aVars[idxVar].idxReg         = UINT8_MAX;
    pReNative->Core.aVars[idxVar].uArgNo         = UINT8_MAX;
    pReNative->Core.aVars[idxVar].idxReferrerVar = UINT8_MAX;
    pReNative->Core.aVars[idxVar].enmGstReg      = kIemNativeGstReg_End;
    pReNative->Core.aVars[idxVar].fRegAcquired   = false;
    pReNative->Core.aVars[idxVar].u.uValue       = 0;
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    pReNative->Core.aVars[idxVar].fSimdReg       = false;
#endif
    return idxVar;
}


/**
 * Internal work that allocates an argument variable w/o setting enmKind.
 *
 * @returns Unpacked index.
 * @internal
 */
static uint8_t iemNativeArgAllocInt(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t cbType)
{
    iArgNo += iemNativeArgGetHiddenArgCount(pReNative);
    AssertStmt(iArgNo < RT_ELEMENTS(pReNative->Core.aidxArgVars), IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_1));
    AssertStmt(pReNative->Core.aidxArgVars[iArgNo] == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_DUP_ARG_NO));

    uint8_t const idxVar = iemNativeVarAllocInt(pReNative, cbType);
    pReNative->Core.aidxArgVars[iArgNo]  = idxVar; /* (unpacked) */
    pReNative->Core.aVars[idxVar].uArgNo = iArgNo;
    return idxVar;
}


/**
 * Gets the stack slot for a stack variable, allocating one if necessary.
 *
 * Calling this function implies that the stack slot will contain a valid
 * variable value.  The caller deals with any register currently assigned to the
 * variable, typically by spilling it into the stack slot.
 *
 * @returns The stack slot number.
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @throws  VERR_IEM_VAR_OUT_OF_STACK_SLOTS
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeVarGetStackSlot(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    Assert(pVar->enmKind == kIemNativeVarKind_Stack);

    /* Already got a slot? */
    uint8_t const idxStackSlot = pVar->idxStackSlot;
    if (idxStackSlot != UINT8_MAX)
    {
        Assert(idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS);
        return idxStackSlot;
    }

    /*
     * A single slot is easy to allocate.
     * Allocate them from the top end, closest to BP, to reduce the displacement.
     */
    if (pVar->cbVar <= sizeof(uint64_t))
    {
        unsigned const iSlot = ASMBitLastSetU32(~pReNative->Core.bmStack) - 1;
        AssertStmt(iSlot < IEMNATIVE_FRAME_VAR_SLOTS, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_OUT_OF_STACK_SLOTS));
        pReNative->Core.bmStack |= RT_BIT_32(iSlot);
        pVar->idxStackSlot       = (uint8_t)iSlot;
        Log11(("iemNativeVarGetStackSlot: idxVar=%#x iSlot=%#x\n", idxVar, iSlot));
        return (uint8_t)iSlot;
    }

    /*
     * We need more than one stack slot.
     *
     * cbVar -> fBitAlignMask: 16 -> 1; 32 -> 3; 64 -> 7;
     */
    AssertCompile(RT_IS_POWER_OF_TWO(IEMNATIVE_FRAME_VAR_SLOTS)); /* If not we have to add an overflow check. */
    Assert(pVar->cbVar <= 64);
    uint32_t const fBitAlignMask = RT_BIT_32(ASMBitLastSetU32(pVar->cbVar) - 4) - 1;
    uint32_t       fBitAllocMask = RT_BIT_32((pVar->cbVar + 7) >> 3) - 1;
    uint32_t       bmStack       = pReNative->Core.bmStack;
    while (bmStack != UINT32_MAX)
    {
        unsigned iSlot = ASMBitLastSetU32(~bmStack);
        AssertStmt(iSlot, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_OUT_OF_STACK_SLOTS));
        iSlot = (iSlot - 1) & ~fBitAlignMask;
        if ((bmStack & ~(fBitAllocMask << iSlot)) == bmStack)
        {
            pReNative->Core.bmStack |= (fBitAllocMask << iSlot);
            pVar->idxStackSlot       = (uint8_t)iSlot;
            Log11(("iemNativeVarGetStackSlot: idxVar=%#x iSlot=%#x/%#x (cbVar=%#x)\n",
                   idxVar, iSlot, fBitAllocMask, pVar->cbVar));
            return (uint8_t)iSlot;
        }

        bmStack |= (fBitAllocMask << iSlot);
    }
    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_OUT_OF_STACK_SLOTS));
}


/**
 * Changes the variable to a stack variable.
 *
 * Currently this is s only possible to do the first time the variable is used,
 * switching later is can be implemented but not done.
 *
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @throws  VERR_IEM_VAR_IPE_2
 */
DECL_HIDDEN_THROW(void) iemNativeVarSetKindToStack(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    if (pVar->enmKind != kIemNativeVarKind_Stack)
    {
        /* We could in theory transition from immediate to stack as well, but it
           would involve the caller doing work storing the value on the stack. So,
           till that's required we only allow transition from invalid. */
        AssertStmt(pVar->enmKind == kIemNativeVarKind_Invalid, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        AssertStmt(pVar->idxReg  == UINT8_MAX,                 IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pVar->enmKind = kIemNativeVarKind_Stack;

        /* Note! We don't allocate a stack slot here, that's only done when a
                 slot is actually needed to hold a variable value. */
    }
}


/**
 * Sets it to a variable with a constant value.
 *
 * This does not require stack storage as we know the value and can always
 * reload it, unless of course it's referenced.
 *
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @param   uValue      The immediate value.
 * @throws  VERR_IEM_VAR_OUT_OF_STACK_SLOTS, VERR_IEM_VAR_IPE_2
 */
DECL_HIDDEN_THROW(void) iemNativeVarSetKindToConst(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, uint64_t uValue)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    if (pVar->enmKind != kIemNativeVarKind_Immediate)
    {
        /* Only simple transitions for now. */
        AssertStmt(pVar->enmKind == kIemNativeVarKind_Invalid, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pVar->enmKind = kIemNativeVarKind_Immediate;
    }
    AssertStmt(pVar->idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));

    pVar->u.uValue = uValue;
    AssertMsg(   pVar->cbVar >= sizeof(uint64_t)
              || pVar->u.uValue < RT_BIT_64(pVar->cbVar * 8),
              ("idxVar=%d cbVar=%u uValue=%#RX64\n", idxVar, pVar->cbVar, uValue));
}


/**
 * Sets the variable to a reference (pointer) to @a idxOtherVar.
 *
 * This does not require stack storage as we know the value and can always
 * reload it.  Loading is postponed till needed.
 *
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable. Unpacked.
 * @param   idxOtherVar The variable to take the (stack) address of. Unpacked.
 *
 * @throws  VERR_IEM_VAR_OUT_OF_STACK_SLOTS, VERR_IEM_VAR_IPE_2
 * @internal
 */
static void iemNativeVarSetKindToLocalRef(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, uint8_t idxOtherVar)
{
    Assert(idxVar      < RT_ELEMENTS(pReNative->Core.aVars) && (pReNative->Core.bmVars & RT_BIT_32(idxVar)));
    Assert(idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars) && (pReNative->Core.bmVars & RT_BIT_32(idxOtherVar)));

    if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_VarRef)
    {
        /* Only simple transitions for now. */
        AssertStmt(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Invalid,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pReNative->Core.aVars[idxVar].enmKind = kIemNativeVarKind_VarRef;
    }
    AssertStmt(pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));

    pReNative->Core.aVars[idxVar].u.idxRefVar = idxOtherVar; /* unpacked */

    /* Update the other variable, ensure it's a stack variable. */
    /** @todo handle variables with const values... that'll go boom now. */
    pReNative->Core.aVars[idxOtherVar].idxReferrerVar = idxVar;
    iemNativeVarSetKindToStack(pReNative, IEMNATIVE_VAR_IDX_PACK(idxOtherVar));
}


/**
 * Sets the variable to a reference (pointer) to a guest register reference.
 *
 * This does not require stack storage as we know the value and can always
 * reload it.  Loading is postponed till needed.
 *
 * @param   pReNative       The recompiler state.
 * @param   idxVar          The variable.
 * @param   enmRegClass     The class guest registers to reference.
 * @param   idxReg          The register within @a enmRegClass to reference.
 *
 * @throws  VERR_IEM_VAR_IPE_2
 */
DECL_HIDDEN_THROW(void) iemNativeVarSetKindToGstRegRef(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar,
                                                       IEMNATIVEGSTREGREF enmRegClass, uint8_t idxReg)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];

    if (pVar->enmKind != kIemNativeVarKind_GstRegRef)
    {
        /* Only simple transitions for now. */
        AssertStmt(pVar->enmKind == kIemNativeVarKind_Invalid, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pVar->enmKind = kIemNativeVarKind_GstRegRef;
    }
    AssertStmt(pVar->idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));

    pVar->u.GstRegRef.enmClass = enmRegClass;
    pVar->u.GstRegRef.idx      = idxReg;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeArgAlloc(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t cbType)
{
    return IEMNATIVE_VAR_IDX_PACK(iemNativeArgAllocInt(pReNative, iArgNo, cbType));
}


DECL_HIDDEN_THROW(uint8_t) iemNativeArgAllocConst(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t cbType, uint64_t uValue)
{
    uint8_t const idxVar = IEMNATIVE_VAR_IDX_PACK(iemNativeArgAllocInt(pReNative, iArgNo, cbType));

    /* Since we're using a generic uint64_t value type, we must truncate it if
       the variable is smaller otherwise we may end up with too large value when
       scaling up a imm8 w/ sign-extension.

       This caused trouble with a "add bx, 0xffff" instruction (around f000:ac60
       in the bios, bx=1) when running on arm, because clang expect 16-bit
       register parameters to have bits 16 and up set to zero.  Instead of
       setting x1 = 0xffff we ended up with x1 = 0xffffffffffffff and the wrong
       CF value in the result.  */
    switch (cbType)
    {
        case sizeof(uint8_t):   uValue &= UINT64_C(0xff); break;
        case sizeof(uint16_t):  uValue &= UINT64_C(0xffff); break;
        case sizeof(uint32_t):  uValue &= UINT64_C(0xffffffff); break;
    }
    iemNativeVarSetKindToConst(pReNative, idxVar, uValue);
    return idxVar;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeArgAllocLocalRef(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t idxOtherVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxOtherVar);
    idxOtherVar = IEMNATIVE_VAR_IDX_UNPACK(idxOtherVar);
    AssertStmt(   idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars)
               && (pReNative->Core.bmVars & RT_BIT_32(idxOtherVar))
               && pReNative->Core.aVars[idxOtherVar].uArgNo == UINT8_MAX,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_1));

    uint8_t const idxArgVar = iemNativeArgAlloc(pReNative, iArgNo, sizeof(uintptr_t));
    iemNativeVarSetKindToLocalRef(pReNative, IEMNATIVE_VAR_IDX_UNPACK(idxArgVar), idxOtherVar);
    return idxArgVar;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeVarAlloc(PIEMRECOMPILERSTATE pReNative, uint8_t cbType)
{
    uint8_t const idxVar = IEMNATIVE_VAR_IDX_PACK(iemNativeVarAllocInt(pReNative, cbType));
    /* Don't set to stack now, leave that to the first use as for instance
       IEM_MC_CALC_RM_EFF_ADDR may produce a const/immediate result (esp. in DOS). */
    return idxVar;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeVarAllocConst(PIEMRECOMPILERSTATE pReNative, uint8_t cbType, uint64_t uValue)
{
    uint8_t const idxVar = IEMNATIVE_VAR_IDX_PACK(iemNativeVarAllocInt(pReNative, cbType));

    /* Since we're using a generic uint64_t value type, we must truncate it if
       the variable is smaller otherwise we may end up with too large value when
       scaling up a imm8 w/ sign-extension. */
    switch (cbType)
    {
        case sizeof(uint8_t):   uValue &= UINT64_C(0xff); break;
        case sizeof(uint16_t):  uValue &= UINT64_C(0xffff); break;
        case sizeof(uint32_t):  uValue &= UINT64_C(0xffffffff); break;
    }
    iemNativeVarSetKindToConst(pReNative, idxVar, uValue);
    return idxVar;
}


DECL_HIDDEN_THROW(uint8_t)  iemNativeVarAllocAssign(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint8_t cbType, uint8_t idxVarOther)
{
    uint8_t const idxVar = IEMNATIVE_VAR_IDX_PACK(iemNativeVarAllocInt(pReNative, cbType));
    iemNativeVarSetKindToStack(pReNative, IEMNATIVE_VAR_IDX_PACK(idxVar));

    uint8_t const idxVarOtherReg = iemNativeVarRegisterAcquire(pReNative, idxVarOther, poff, true /*fInitialized*/);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxVar, poff);

    *poff = iemNativeEmitLoadGprFromGpr(pReNative, *poff, idxVarReg, idxVarOtherReg);

    /* Truncate the value to this variables size. */
    switch (cbType)
    {
        case sizeof(uint8_t):   *poff = iemNativeEmitAndGpr32ByImm(pReNative, *poff, idxVarReg, UINT64_C(0xff)); break;
        case sizeof(uint16_t):  *poff = iemNativeEmitAndGpr32ByImm(pReNative, *poff, idxVarReg, UINT64_C(0xffff)); break;
        case sizeof(uint32_t):  *poff = iemNativeEmitAndGpr32ByImm(pReNative, *poff, idxVarReg, UINT64_C(0xffffffff)); break;
    }

    iemNativeVarRegisterRelease(pReNative, idxVarOther);
    iemNativeVarRegisterRelease(pReNative, idxVar);
    return idxVar;
}


/**
 * Makes sure variable @a idxVar has a register assigned to it and that it stays
 * fixed till we call iemNativeVarRegisterRelease.
 *
 * @returns The host register number.
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @param   poff        Pointer to the instruction buffer offset.
 *                      In case a register needs to be freed up or the value
 *                      loaded off the stack.
 * @param  fInitialized Set if the variable must already have been initialized.
 *                      Will throw VERR_IEM_VAR_NOT_INITIALIZED if this is not
 *                      the case.
 * @param  idxRegPref   Preferred register number or UINT8_MAX.
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeVarRegisterAcquire(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, uint32_t *poff,
                                                       bool fInitialized /*= false*/, uint8_t idxRegPref /*= UINT8_MAX*/)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    Assert(pVar->cbVar <= 8);
    Assert(!pVar->fRegAcquired);

    uint8_t idxReg = pVar->idxReg;
    if (idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        Assert(   pVar->enmKind > kIemNativeVarKind_Invalid
               && pVar->enmKind < kIemNativeVarKind_End);
        pVar->fRegAcquired = true;
        return idxReg;
    }

    /*
     * If the kind of variable has not yet been set, default to 'stack'.
     */
    Assert(   pVar->enmKind >= kIemNativeVarKind_Invalid
           && pVar->enmKind < kIemNativeVarKind_End);
    if (pVar->enmKind == kIemNativeVarKind_Invalid)
        iemNativeVarSetKindToStack(pReNative, idxVar);

    /*
     * We have to allocate a register for the variable, even if its a stack one
     * as we don't know if there are modification being made to it before its
     * finalized (todo: analyze and insert hints about that?).
     *
     * If we can, we try get the correct register for argument variables. This
     * is assuming that most argument variables are fetched as close as possible
     * to the actual call, so that there aren't any interfering hidden calls
     * (memory accesses, etc) inbetween.
     *
     * If we cannot or it's a variable, we make sure no argument registers
     * that will be used by this MC block will be allocated here, and we always
     * prefer non-volatile registers to avoid needing to spill stuff for internal
     * call.
     */
    /** @todo Detect too early argument value fetches and warn about hidden
     * calls causing less optimal code to be generated in the python script. */

    uint8_t const uArgNo = pVar->uArgNo;
    if (   uArgNo < RT_ELEMENTS(g_aidxIemNativeCallRegs)
        && !(pReNative->Core.bmHstRegs & RT_BIT_32(g_aidxIemNativeCallRegs[uArgNo])))
    {
        idxReg = g_aidxIemNativeCallRegs[uArgNo];

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
        /* Writeback any dirty shadow registers we are about to unshadow. */
        *poff = iemNativeRegFlushDirtyGuestByHostRegShadow(pReNative, *poff, idxReg);
#endif

        iemNativeRegClearGstRegShadowing(pReNative, idxReg, *poff);
        Log11(("iemNativeVarRegisterAcquire: idxVar=%#x idxReg=%u (matching arg %u)\n", idxVar, idxReg, uArgNo));
    }
    else if (   idxRegPref >= RT_ELEMENTS(pReNative->Core.aHstRegs)
             || (pReNative->Core.bmHstRegs & RT_BIT_32(idxRegPref)))
    {
        /** @todo there must be a better way for this and boot cArgsX?   */
        uint32_t const fNotArgsMask = ~g_afIemNativeCallRegs[RT_MIN(pReNative->cArgsX, IEMNATIVE_CALL_ARG_GREG_COUNT)];
        uint32_t const fRegs        = ~pReNative->Core.bmHstRegs
                                    & ~pReNative->Core.bmHstRegsWithGstShadow
                                    & (~IEMNATIVE_REG_FIXED_MASK & IEMNATIVE_HST_GREG_MASK)
                                    & fNotArgsMask;
        if (fRegs)
        {
            /* Pick from the top as that both arm64 and amd64 have a block of non-volatile registers there. */
            idxReg = (uint8_t)ASMBitLastSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                               ? fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK : fRegs) - 1;
            Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows == 0);
            Assert(!(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg)));
            Log11(("iemNativeVarRegisterAcquire: idxVar=%#x idxReg=%u (uArgNo=%u)\n", idxVar, idxReg, uArgNo));
        }
        else
        {
            idxReg = iemNativeRegAllocFindFree(pReNative, poff, false /*fPreferVolatile*/,
                                               IEMNATIVE_HST_GREG_MASK & ~IEMNATIVE_REG_FIXED_MASK & fNotArgsMask);
            AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_VAR));
            Log11(("iemNativeVarRegisterAcquire: idxVar=%#x idxReg=%u (slow, uArgNo=%u)\n", idxVar, idxReg, uArgNo));
        }
    }
    else
    {
        idxReg = idxRegPref;
        iemNativeRegClearGstRegShadowing(pReNative, idxReg, *poff);
        Log11(("iemNativeVarRegisterAcquire: idxVar=%#x idxReg=%u (preferred)\n", idxVar, idxReg));
    }
    iemNativeRegMarkAllocated(pReNative, idxReg, kIemNativeWhat_Var, idxVar);
    pVar->idxReg = idxReg;

#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    pVar->fSimdReg = false;
#endif

    /*
     * Load it off the stack if we've got a stack slot.
     */
    uint8_t const idxStackSlot = pVar->idxStackSlot;
    if (idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS)
    {
        Assert(fInitialized);
        int32_t const offDispBp = iemNativeStackCalcBpDisp(idxStackSlot);
        switch (pVar->cbVar)
        {
            case 1: *poff = iemNativeEmitLoadGprByBpU8( pReNative, *poff, idxReg, offDispBp); break;
            case 2: *poff = iemNativeEmitLoadGprByBpU16(pReNative, *poff, idxReg, offDispBp); break;
            case 3: AssertFailed(); RT_FALL_THRU();
            case 4: *poff = iemNativeEmitLoadGprByBpU32(pReNative, *poff, idxReg, offDispBp); break;
            default: AssertFailed(); RT_FALL_THRU();
            case 8: *poff = iemNativeEmitLoadGprByBp(   pReNative, *poff, idxReg, offDispBp); break;
        }
    }
    else
    {
        Assert(idxStackSlot == UINT8_MAX);
        AssertStmt(!fInitialized, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
    }
    pVar->fRegAcquired = true;
    return idxReg;
}


#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
/**
 * Makes sure variable @a idxVar has a SIMD register assigned to it and that it stays
 * fixed till we call iemNativeVarRegisterRelease.
 *
 * @returns The host register number.
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @param   poff        Pointer to the instruction buffer offset.
 *                      In case a register needs to be freed up or the value
 *                      loaded off the stack.
 * @param  fInitialized Set if the variable must already have been initialized.
 *                      Will throw VERR_IEM_VAR_NOT_INITIALIZED if this is not
 *                      the case.
 * @param  idxRegPref   Preferred SIMD register number or UINT8_MAX.
 */
DECL_HIDDEN_THROW(uint8_t) iemNativeVarSimdRegisterAcquire(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, uint32_t *poff,
                                                           bool fInitialized /*= false*/, uint8_t idxRegPref /*= UINT8_MAX*/)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    Assert(   pVar->cbVar == sizeof(RTUINT128U)
           || pVar->cbVar == sizeof(RTUINT256U));
    Assert(!pVar->fRegAcquired);

    uint8_t idxReg = pVar->idxReg;
    if (idxReg < RT_ELEMENTS(pReNative->Core.aHstSimdRegs))
    {
        Assert(   pVar->enmKind > kIemNativeVarKind_Invalid
               && pVar->enmKind < kIemNativeVarKind_End);
        pVar->fRegAcquired = true;
        return idxReg;
    }

    /*
     * If the kind of variable has not yet been set, default to 'stack'.
     */
    Assert(   pVar->enmKind >= kIemNativeVarKind_Invalid
           && pVar->enmKind < kIemNativeVarKind_End);
    if (pVar->enmKind == kIemNativeVarKind_Invalid)
        iemNativeVarSetKindToStack(pReNative, idxVar);

    /*
     * We have to allocate a register for the variable, even if its a stack one
     * as we don't know if there are modification being made to it before its
     * finalized (todo: analyze and insert hints about that?).
     *
     * If we can, we try get the correct register for argument variables. This
     * is assuming that most argument variables are fetched as close as possible
     * to the actual call, so that there aren't any interfering hidden calls
     * (memory accesses, etc) inbetween.
     *
     * If we cannot or it's a variable, we make sure no argument registers
     * that will be used by this MC block will be allocated here, and we always
     * prefer non-volatile registers to avoid needing to spill stuff for internal
     * call.
     */
    /** @todo Detect too early argument value fetches and warn about hidden
     * calls causing less optimal code to be generated in the python script. */

    uint8_t const uArgNo = pVar->uArgNo;
    Assert(uArgNo == UINT8_MAX); RT_NOREF(uArgNo); /* No SIMD registers as arguments for now. */

    /* SIMD is bit simpler for now because there is no support for arguments. */
    if (   idxRegPref >= RT_ELEMENTS(pReNative->Core.aHstSimdRegs)
        || (pReNative->Core.bmHstSimdRegs & RT_BIT_32(idxRegPref)))
    {
        uint32_t const fNotArgsMask = UINT32_MAX; //~g_afIemNativeCallRegs[RT_MIN(pReNative->cArgs, IEMNATIVE_CALL_ARG_GREG_COUNT)];
        uint32_t const fRegs        = ~pReNative->Core.bmHstSimdRegs
                                    & ~pReNative->Core.bmHstSimdRegsWithGstShadow
                                    & (~IEMNATIVE_SIMD_REG_FIXED_MASK & IEMNATIVE_HST_SIMD_REG_MASK)
                                    & fNotArgsMask;
        if (fRegs)
        {
            idxReg = (uint8_t)ASMBitLastSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK
                                               ? fRegs & ~IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK : fRegs) - 1;
            Assert(pReNative->Core.aHstSimdRegs[idxReg].fGstRegShadows == 0);
            Assert(!(pReNative->Core.bmHstSimdRegsWithGstShadow & RT_BIT_32(idxReg)));
            Log11(("iemNativeVarSimdRegisterAcquire: idxVar=%#x idxReg=%u (uArgNo=%u)\n", idxVar, idxReg, uArgNo));
        }
        else
        {
            idxReg = iemNativeSimdRegAllocFindFree(pReNative, poff, false /*fPreferVolatile*/,
                                                   IEMNATIVE_HST_SIMD_REG_MASK & ~IEMNATIVE_SIMD_REG_FIXED_MASK & fNotArgsMask);
            AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_VAR));
            Log11(("iemNativeVarSimdRegisterAcquire: idxVar=%#x idxReg=%u (slow, uArgNo=%u)\n", idxVar, idxReg, uArgNo));
        }
    }
    else
    {
        idxReg = idxRegPref;
        AssertReleaseFailed(); //iemNativeRegClearGstRegShadowing(pReNative, idxReg, *poff);
        Log11(("iemNativeVarSimdRegisterAcquire: idxVar=%#x idxReg=%u (preferred)\n", idxVar, idxReg));
    }
    iemNativeSimdRegMarkAllocated(pReNative, idxReg, kIemNativeWhat_Var, idxVar);

    pVar->fSimdReg = true;
    pVar->idxReg = idxReg;

    /*
     * Load it off the stack if we've got a stack slot.
     */
    uint8_t const idxStackSlot = pVar->idxStackSlot;
    if (idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS)
    {
        Assert(fInitialized);
        int32_t const offDispBp = iemNativeStackCalcBpDisp(idxStackSlot);
        switch (pVar->cbVar)
        {
            case sizeof(RTUINT128U): *poff = iemNativeEmitLoadVecRegByBpU128(pReNative, *poff, idxReg, offDispBp); break;
            default: AssertFailed(); RT_FALL_THRU();
            case sizeof(RTUINT256U): *poff = iemNativeEmitLoadVecRegByBpU256(pReNative, *poff, idxReg, offDispBp); break;
        }
    }
    else
    {
        Assert(idxStackSlot == UINT8_MAX);
        AssertStmt(!fInitialized, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
    }
    pVar->fRegAcquired = true;
    return idxReg;
}
#endif


/**
 * The value of variable @a idxVar will be written in full to the @a enmGstReg
 * guest register.
 *
 * This function makes sure there is a register for it and sets it to be the
 * current shadow copy of @a enmGstReg.
 *
 * @returns The host register number.
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @param   enmGstReg   The guest register this variable will be written to
 *                      after this call.
 * @param   poff        Pointer to the instruction buffer offset.
 *                      In case a register needs to be freed up or if the
 *                      variable content needs to be loaded off the stack.
 *
 * @note    We DO NOT expect @a idxVar to be an argument variable,
 *          because we can only in the commit stage of an instruction when this
 *          function is used.
 */
DECL_HIDDEN_THROW(uint8_t)
iemNativeVarRegisterAcquireForGuestReg(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, IEMNATIVEGSTREG enmGstReg, uint32_t *poff)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
    Assert(!pVar->fRegAcquired);
    AssertMsgStmt(   pVar->cbVar <= 8
                  && (   pVar->enmKind == kIemNativeVarKind_Immediate
                      || pVar->enmKind == kIemNativeVarKind_Stack),
                  ("idxVar=%#x cbVar=%d enmKind=%d enmGstReg=%s\n", idxVar, pVar->cbVar,
                   pVar->enmKind, g_aGstShadowInfo[enmGstReg].pszName),
                  IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_6));

    /*
     * This shouldn't ever be used for arguments, unless it's in a weird else
     * branch that doesn't do any calling and even then it's questionable.
     *
     * However, in case someone writes crazy wrong MC code and does register
     * updates before making calls, just use the regular register allocator to
     * ensure we get a register suitable for the intended argument number.
     */
    AssertStmt(pVar->uArgNo == UINT8_MAX, iemNativeVarRegisterAcquire(pReNative, idxVar, poff));

    /*
     * If there is already a register for the variable, we transfer/set the
     * guest shadow copy assignment to it.
     */
    uint8_t idxReg = pVar->idxReg;
    if (idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
        if (enmGstReg >= kIemNativeGstReg_GprFirst && enmGstReg <= kIemNativeGstReg_GprLast)
        {
# ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
            iemNativeDbgInfoAddNativeOffset(pReNative, *poff);
            iemNativeDbgInfoAddGuestRegDirty(pReNative, false /*fSimdReg*/, enmGstReg, idxReg);
# endif

            pReNative->Core.bmGstRegShadowDirty |= RT_BIT_64(enmGstReg);
        }
#endif

        if (pReNative->Core.bmGstRegShadows & RT_BIT_64(enmGstReg))
        {
            uint8_t const idxRegOld = pReNative->Core.aidxGstRegShadows[enmGstReg];
            iemNativeRegTransferGstRegShadowing(pReNative, idxRegOld, idxReg, enmGstReg, *poff);
            Log12(("iemNativeVarRegisterAcquireForGuestReg: Moved %s for guest %s into %s for full write\n",
                   g_apszIemNativeHstRegNames[idxRegOld], g_aGstShadowInfo[enmGstReg].pszName, g_apszIemNativeHstRegNames[idxReg]));
        }
        else
        {
            iemNativeRegMarkAsGstRegShadow(pReNative, idxReg, enmGstReg, *poff);
            Log12(("iemNativeVarRegisterAcquireForGuestReg: Marking %s as copy of guest %s (full write)\n",
                   g_apszIemNativeHstRegNames[idxReg], g_aGstShadowInfo[enmGstReg].pszName));
        }
        /** @todo figure this one out. We need some way of making sure the register isn't
         * modified after this point, just in case we start writing crappy MC code. */
        pVar->enmGstReg    = enmGstReg;
        pVar->fRegAcquired = true;
        return idxReg;
    }
    Assert(pVar->uArgNo == UINT8_MAX);

    /*
     * Because this is supposed to be the commit stage, we're just tag along with the
     * temporary register allocator and upgrade it to a variable register.
     */
    idxReg = iemNativeRegAllocTmpForGuestReg(pReNative, poff, enmGstReg, kIemNativeGstRegUse_ForFullWrite);
    Assert(pReNative->Core.aHstRegs[idxReg].enmWhat == kIemNativeWhat_Tmp);
    Assert(pReNative->Core.aHstRegs[idxReg].idxVar  == UINT8_MAX);
    pReNative->Core.aHstRegs[idxReg].enmWhat = kIemNativeWhat_Var;
    pReNative->Core.aHstRegs[idxReg].idxVar  = idxVar;
    pVar->idxReg                             = idxReg;

    /*
     * Now we need to load the register value.
     */
    if (pVar->enmKind == kIemNativeVarKind_Immediate)
        *poff = iemNativeEmitLoadGprImm64(pReNative, *poff, idxReg, pVar->u.uValue);
    else
    {
        uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
        int32_t const offDispBp    = iemNativeStackCalcBpDisp(idxStackSlot);
        switch (pVar->cbVar)
        {
            case sizeof(uint64_t):
                *poff = iemNativeEmitLoadGprByBp(pReNative, *poff, idxReg, offDispBp);
                break;
            case sizeof(uint32_t):
                *poff = iemNativeEmitLoadGprByBpU32(pReNative, *poff, idxReg, offDispBp);
                break;
            case sizeof(uint16_t):
                *poff = iemNativeEmitLoadGprByBpU16(pReNative, *poff, idxReg, offDispBp);
                break;
            case sizeof(uint8_t):
                *poff = iemNativeEmitLoadGprByBpU8(pReNative, *poff, idxReg, offDispBp);
                break;
            default:
                AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_6));
        }
    }

    pVar->fRegAcquired = true;
    return idxReg;
}


/**
 * Emit code to save volatile registers prior to a call to a helper (TLB miss).
 *
 * This is used together with iemNativeVarRestoreVolatileRegsPostHlpCall() and
 * optionally iemNativeRegRestoreGuestShadowsInVolatileRegs() to bypass the
 * requirement of flushing anything in volatile host registers when making a
 * call.
 *
 * @returns New @a off value.
 * @param   pReNative           The recompiler state.
 * @param   off                 The code buffer position.
 * @param   fHstRegsNotToSave   Set of registers not to save & restore.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeVarSaveVolatileRegsPreHlpCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fHstRegsNotToSave)
{
    uint32_t fHstRegs = pReNative->Core.bmHstRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK & ~fHstRegsNotToSave;
    if (fHstRegs)
    {
        do
        {
            unsigned int const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;
            fHstRegs &= ~RT_BIT_32(idxHstReg);

            if (pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Var)
            {
                uint8_t const idxVar = pReNative->Core.aHstRegs[idxHstReg].idxVar;
                IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
                AssertStmt(   IEMNATIVE_VAR_IDX_UNPACK(idxVar) < RT_ELEMENTS(pReNative->Core.aVars)
                           && (pReNative->Core.bmVars & RT_BIT_32(IEMNATIVE_VAR_IDX_UNPACK(idxVar)))
                           && pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxHstReg,
                           IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_12));
                switch (pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].enmKind)
                {
                    case kIemNativeVarKind_Stack:
                    {
                        /* Temporarily spill the variable register. */
                        uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                        Log12(("iemNativeVarSaveVolatileRegsPreHlpCall: spilling idxVar=%#x/idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
                               idxVar, idxHstReg, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
                        off = iemNativeEmitStoreGprByBp(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxHstReg);
                        continue;
                    }

                    case kIemNativeVarKind_Immediate:
                    case kIemNativeVarKind_VarRef:
                    case kIemNativeVarKind_GstRegRef:
                        /* It is weird to have any of these loaded at this point. */
                        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_13));
                        continue;

                    case kIemNativeVarKind_End:
                    case kIemNativeVarKind_Invalid:
                        break;
                }
                AssertFailed();
            }
            else
            {
                /*
                 * Allocate a temporary stack slot and spill the register to it.
                 */
                unsigned const idxStackSlot = ASMBitLastSetU32(~pReNative->Core.bmStack) - 1;
                AssertStmt(idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS,
                           IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_OUT_OF_STACK_SLOTS));
                pReNative->Core.bmStack |= RT_BIT_32(idxStackSlot);
                pReNative->Core.aHstRegs[idxHstReg].idxStackSlot = (uint8_t)idxStackSlot;
                Log12(("iemNativeVarSaveVolatileRegsPreHlpCall: spilling idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
                       idxHstReg, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
                off = iemNativeEmitStoreGprByBp(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxHstReg);
            }
        } while (fHstRegs);
    }
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR

    /*
     * Guest register shadows are flushed to CPUMCTX at the moment and don't need allocating a stack slot
     * which would be more difficult due to spanning multiple stack slots and different sizes
     * (besides we only have a limited amount of slots at the moment).
     *
     * However the shadows need to be flushed out as the guest SIMD register might get corrupted by
     * the callee. This asserts that the registers were written back earlier and are not in the dirty state.
     */
    iemNativeSimdRegFlushGuestShadowsByHostMask(pReNative, IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK);

    fHstRegs = pReNative->Core.bmHstSimdRegs & IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK;
    if (fHstRegs)
    {
        do
        {
            unsigned int const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;
            fHstRegs &= ~RT_BIT_32(idxHstReg);

            /* Fixed reserved and temporary registers don't need saving. */
            if (   pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_FixedReserved
                || pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_FixedTmp)
                continue;

            Assert(pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_Var);

            uint8_t const idxVar = pReNative->Core.aHstSimdRegs[idxHstReg].idxVar;
            IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
            AssertStmt(   IEMNATIVE_VAR_IDX_UNPACK(idxVar) < RT_ELEMENTS(pReNative->Core.aVars)
                       && (pReNative->Core.bmVars & RT_BIT_32(IEMNATIVE_VAR_IDX_UNPACK(idxVar)))
                       && pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxHstReg
                       && pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg
                       && (   pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar == sizeof(RTUINT128U)
                           || pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar == sizeof(RTUINT256U)),
                       IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_12));
            switch (pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].enmKind)
            {
                case kIemNativeVarKind_Stack:
                {
                    /* Temporarily spill the variable register. */
                    uint8_t const cbVar        = pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar;
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                    Log12(("iemNativeVarSaveVolatileRegsPreHlpCall: spilling idxVar=%#x/idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
                           idxVar, idxHstReg, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
                    if (cbVar == sizeof(RTUINT128U))
                        off = iemNativeEmitStoreVecRegByBpU128(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxHstReg);
                    else
                        off = iemNativeEmitStoreVecRegByBpU256(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxHstReg);
                    continue;
                }

                case kIemNativeVarKind_Immediate:
                case kIemNativeVarKind_VarRef:
                case kIemNativeVarKind_GstRegRef:
                    /* It is weird to have any of these loaded at this point. */
                    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_13));
                    continue;

                case kIemNativeVarKind_End:
                case kIemNativeVarKind_Invalid:
                    break;
            }
            AssertFailed();
        } while (fHstRegs);
    }
#endif
    return off;
}


/**
 * Emit code to restore volatile registers after to a call to a helper.
 *
 * @returns New @a off value.
 * @param   pReNative           The recompiler state.
 * @param   off                 The code buffer position.
 * @param   fHstRegsNotToSave   Set of registers not to save & restore.
 * @see     iemNativeVarSaveVolatileRegsPreHlpCall(),
 *          iemNativeRegRestoreGuestShadowsInVolatileRegs()
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeVarRestoreVolatileRegsPostHlpCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fHstRegsNotToSave)
{
    uint32_t fHstRegs = pReNative->Core.bmHstRegs & IEMNATIVE_CALL_VOLATILE_GREG_MASK & ~fHstRegsNotToSave;
    if (fHstRegs)
    {
        do
        {
            unsigned int const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;
            fHstRegs &= ~RT_BIT_32(idxHstReg);

            if (pReNative->Core.aHstRegs[idxHstReg].enmWhat == kIemNativeWhat_Var)
            {
                uint8_t const idxVar = pReNative->Core.aHstRegs[idxHstReg].idxVar;
                IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
                AssertStmt(   IEMNATIVE_VAR_IDX_UNPACK(idxVar) < RT_ELEMENTS(pReNative->Core.aVars)
                           && (pReNative->Core.bmVars & RT_BIT_32(IEMNATIVE_VAR_IDX_UNPACK(idxVar)))
                           && pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxHstReg,
                           IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_12));
                switch (pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].enmKind)
                {
                    case kIemNativeVarKind_Stack:
                    {
                        /* Unspill the variable register. */
                        uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                        Log12(("iemNativeVarRestoreVolatileRegsPostHlpCall: unspilling idxVar=%#x/idxReg=%d (slot %#x bp+%d, off=%#x)\n",
                               idxVar, idxHstReg, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
                        off = iemNativeEmitLoadGprByBp(pReNative, off, idxHstReg, iemNativeStackCalcBpDisp(idxStackSlot));
                        continue;
                    }

                    case kIemNativeVarKind_Immediate:
                    case kIemNativeVarKind_VarRef:
                    case kIemNativeVarKind_GstRegRef:
                        /* It is weird to have any of these loaded at this point. */
                        AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_13));
                        continue;

                    case kIemNativeVarKind_End:
                    case kIemNativeVarKind_Invalid:
                        break;
                }
                AssertFailed();
            }
            else
            {
                /*
                 * Restore from temporary stack slot.
                 */
                uint8_t const idxStackSlot = pReNative->Core.aHstRegs[idxHstReg].idxStackSlot;
                AssertContinue(idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS && (pReNative->Core.bmStack & RT_BIT_32(idxStackSlot)));
                pReNative->Core.bmStack &= ~RT_BIT_32(idxStackSlot);
                pReNative->Core.aHstRegs[idxHstReg].idxStackSlot = UINT8_MAX;

                off = iemNativeEmitLoadGprByBp(pReNative, off, idxHstReg, iemNativeStackCalcBpDisp(idxStackSlot));
            }
        } while (fHstRegs);
    }
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    fHstRegs = pReNative->Core.bmHstSimdRegs & IEMNATIVE_CALL_VOLATILE_SIMD_REG_MASK;
    if (fHstRegs)
    {
        do
        {
            unsigned int const idxHstReg = ASMBitFirstSetU32(fHstRegs) - 1;
            fHstRegs &= ~RT_BIT_32(idxHstReg);

            if (   pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_FixedTmp
                || pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_FixedReserved)
                continue;
            Assert(pReNative->Core.aHstSimdRegs[idxHstReg].enmWhat == kIemNativeWhat_Var);

            uint8_t const idxVar = pReNative->Core.aHstSimdRegs[idxHstReg].idxVar;
            IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
            AssertStmt(   IEMNATIVE_VAR_IDX_UNPACK(idxVar) < RT_ELEMENTS(pReNative->Core.aVars)
                       && (pReNative->Core.bmVars & RT_BIT_32(IEMNATIVE_VAR_IDX_UNPACK(idxVar)))
                       && pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].idxReg == idxHstReg
                       && pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].fSimdReg
                       && (   pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar == sizeof(RTUINT128U)
                           || pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar == sizeof(RTUINT256U)),
                       IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_12));
            switch (pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].enmKind)
            {
                case kIemNativeVarKind_Stack:
                {
                    /* Unspill the variable register. */
                    uint8_t const cbVar        = pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)].cbVar;
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                    Log12(("iemNativeVarRestoreVolatileRegsPostHlpCall: unspilling idxVar=%#x/idxReg=%d (slot %#x bp+%d, off=%#x)\n",
                           idxVar, idxHstReg, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));

                    if (cbVar == sizeof(RTUINT128U))
                        off = iemNativeEmitLoadVecRegByBpU128(pReNative, off, idxHstReg, iemNativeStackCalcBpDisp(idxStackSlot));
                    else
                        off = iemNativeEmitLoadVecRegByBpU256(pReNative, off, idxHstReg, iemNativeStackCalcBpDisp(idxStackSlot));
                    continue;
                }

                case kIemNativeVarKind_Immediate:
                case kIemNativeVarKind_VarRef:
                case kIemNativeVarKind_GstRegRef:
                    /* It is weird to have any of these loaded at this point. */
                    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_13));
                    continue;

                case kIemNativeVarKind_End:
                case kIemNativeVarKind_Invalid:
                    break;
            }
            AssertFailed();
        } while (fHstRegs);
    }
#endif
    return off;
}


/**
 * Worker that frees the stack slots for variable @a idxVar if any allocated.
 *
 * This is used both by iemNativeVarFreeOneWorker and iemNativeEmitCallCommon.
 *
 * ASSUMES that @a idxVar is valid and unpacked.
 */
DECL_FORCE_INLINE(void) iemNativeVarFreeStackSlots(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    Assert(idxVar < RT_ELEMENTS(pReNative->Core.aVars)); /* unpacked! */
    uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
    if (idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS)
    {
        uint8_t const  cbVar      = pReNative->Core.aVars[idxVar].cbVar;
        uint8_t const  cSlots     = (cbVar + sizeof(uint64_t) - 1) / sizeof(uint64_t);
        uint32_t const fAllocMask = (uint32_t)(RT_BIT_32(cSlots) - 1U);
        Assert(cSlots > 0);
        Assert(((pReNative->Core.bmStack >> idxStackSlot) & fAllocMask) == fAllocMask);
        Log11(("iemNativeVarFreeStackSlots: idxVar=%d/%#x iSlot=%#x/%#x (cbVar=%#x)\n",
               idxVar, IEMNATIVE_VAR_IDX_PACK(idxVar), idxStackSlot, fAllocMask, cbVar));
        pReNative->Core.bmStack &= ~(fAllocMask << idxStackSlot);
        pReNative->Core.aVars[idxVar].idxStackSlot = UINT8_MAX;
    }
    else
        Assert(idxStackSlot == UINT8_MAX);
}


/**
 * Worker that frees a single variable.
 *
 * ASSUMES that @a idxVar is valid and unpacked.
 */
DECLHIDDEN(void) iemNativeVarFreeOneWorker(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    Assert(   pReNative->Core.aVars[idxVar].enmKind >= kIemNativeVarKind_Invalid  /* Including invalid as we may have unused */
           && pReNative->Core.aVars[idxVar].enmKind <  kIemNativeVarKind_End);    /* variables in conditional branches. */
    Assert(!pReNative->Core.aVars[idxVar].fRegAcquired);

    /* Free the host register first if any assigned. */
    uint8_t const idxHstReg = pReNative->Core.aVars[idxVar].idxReg;
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
    if (   idxHstReg != UINT8_MAX
        && pReNative->Core.aVars[idxVar].fSimdReg)
    {
        Assert(idxHstReg < RT_ELEMENTS(pReNative->Core.aHstSimdRegs));
        Assert(pReNative->Core.aHstSimdRegs[idxHstReg].idxVar == IEMNATIVE_VAR_IDX_PACK(idxVar));
        pReNative->Core.aHstSimdRegs[idxHstReg].idxVar = UINT8_MAX;
        pReNative->Core.bmHstSimdRegs &= ~RT_BIT_32(idxHstReg);
    }
    else
#endif
    if (idxHstReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        Assert(pReNative->Core.aHstRegs[idxHstReg].idxVar == IEMNATIVE_VAR_IDX_PACK(idxVar));
        pReNative->Core.aHstRegs[idxHstReg].idxVar = UINT8_MAX;
        pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
    }

    /* Free argument mapping. */
    uint8_t const uArgNo = pReNative->Core.aVars[idxVar].uArgNo;
    if (uArgNo < RT_ELEMENTS(pReNative->Core.aidxArgVars))
        pReNative->Core.aidxArgVars[uArgNo] = UINT8_MAX;

    /* Free the stack slots. */
    iemNativeVarFreeStackSlots(pReNative, idxVar);

    /* Free the actual variable. */
    pReNative->Core.aVars[idxVar].enmKind = kIemNativeVarKind_Invalid;
    pReNative->Core.bmVars &= ~RT_BIT_32(idxVar);
}


/**
 * Worker for iemNativeVarFreeAll that's called when there is anything to do.
 */
DECLHIDDEN(void) iemNativeVarFreeAllSlow(PIEMRECOMPILERSTATE pReNative, uint32_t bmVars)
{
    while (bmVars != 0)
    {
        uint8_t const idxVar = ASMBitFirstSetU32(bmVars) - 1;
        bmVars &= ~RT_BIT_32(idxVar);

#if 1 /** @todo optimize by simplifying this later... */
        iemNativeVarFreeOneWorker(pReNative, idxVar);
#else
        /* Only need to free the host register, the rest is done as bulk updates below. */
        uint8_t const idxHstReg = pReNative->Core.aVars[idxVar].idxReg;
        if (idxHstReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
        {
            Assert(pReNative->Core.aHstRegs[idxHstReg].idxVar == IEMNATIVE_VAR_IDX_PACK(idxVar));
            pReNative->Core.aHstRegs[idxHstReg].idxVar = UINT8_MAX;
            pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
        }
#endif
    }
#if 0 /** @todo optimize by simplifying this later... */
    pReNative->Core.bmVars     = 0;
    pReNative->Core.bmStack    = 0;
    pReNative->Core.u64ArgVars = UINT64_MAX;
#endif
}



/*********************************************************************************************************************************
*   Emitters for IEM_MC_CALL_CIMPL_XXX                                                                                           *
*********************************************************************************************************************************/

/**
 * Emits code to load a reference to the given guest register into @a idxGprDst.
  */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitLeaGprByGstRegRef(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxGprDst,
                               IEMNATIVEGSTREGREF enmClass, uint8_t idxRegInClass)
{
#ifdef IEMNATIVE_WITH_DELAYED_PC_UPDATING
    /** @todo If we ever gonna allow referencing the RIP register we need to update guest value here. */
#endif

    /*
     * Get the offset relative to the CPUMCTX structure.
     */
    uint32_t offCpumCtx;
    switch (enmClass)
    {
        case kIemNativeGstRegRef_Gpr:
            Assert(idxRegInClass < 16);
            offCpumCtx = RT_UOFFSETOF_DYN(CPUMCTX, aGRegs[idxRegInClass]);
            break;

        case kIemNativeGstRegRef_GprHighByte:    /**< AH, CH, DH, BH*/
            Assert(idxRegInClass < 4);
            offCpumCtx = RT_UOFFSETOF_DYN(CPUMCTX, aGRegs[0].bHi) + idxRegInClass * sizeof(CPUMCTXGREG);
            break;

        case kIemNativeGstRegRef_EFlags:
            Assert(idxRegInClass == 0);
            offCpumCtx = RT_UOFFSETOF(CPUMCTX, eflags);
            break;

        case kIemNativeGstRegRef_MxCsr:
            Assert(idxRegInClass == 0);
            offCpumCtx = RT_UOFFSETOF(CPUMCTX, XState.x87.MXCSR);
            break;

        case kIemNativeGstRegRef_FpuReg:
            Assert(idxRegInClass < 8);
            AssertFailed(); /** @todo what kind of indexing? */
            offCpumCtx = RT_UOFFSETOF_DYN(CPUMCTX, XState.x87.aRegs[idxRegInClass]);
            break;

        case kIemNativeGstRegRef_MReg:
            Assert(idxRegInClass < 8);
            AssertFailed(); /** @todo what kind of indexing? */
            offCpumCtx = RT_UOFFSETOF_DYN(CPUMCTX, XState.x87.aRegs[idxRegInClass]);
            break;

        case kIemNativeGstRegRef_XReg:
            Assert(idxRegInClass < 16);
            offCpumCtx = RT_UOFFSETOF_DYN(CPUMCTX, XState.x87.aXMM[idxRegInClass]);
            break;

        case kIemNativeGstRegRef_X87: /* Not a register actually but we would just duplicate code otherwise. */
            Assert(idxRegInClass == 0);
            offCpumCtx = RT_UOFFSETOF(CPUMCTX, XState.x87);
            break;

        case kIemNativeGstRegRef_XState: /* Not a register actually but we would just duplicate code otherwise. */
            Assert(idxRegInClass == 0);
            offCpumCtx = RT_UOFFSETOF(CPUMCTX, XState);
            break;

        default:
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_5));
    }

    /*
     * Load the value into the destination register.
     */
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLeaGprByVCpu(pReNative, off, idxGprDst, offCpumCtx + RT_UOFFSETOF(VMCPUCC, cpum.GstCtx));

#elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    Assert(offCpumCtx < 4096);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxGprDst, IEMNATIVE_REG_FIXED_PCPUMCTX, offCpumCtx);

#else
# error "Port me!"
#endif

    return off;
}


/**
 * Common code for CIMPL and AIMPL calls.
 *
 * These are calls that uses argument variables and such.  They should not be
 * confused with internal calls required to implement an MC operation,
 * like a TLB load and similar.
 *
 * Upon return all that is left to do is to load any hidden arguments and
 * perform the call. All argument variables are freed.
 *
 * @returns New code buffer offset; throws VBox status code on error.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   cArgs           The total nubmer of arguments (includes hidden
 *                          count).
 * @param   cHiddenArgs     The number of hidden arguments.  The hidden
 *                          arguments must not have any variable declared for
 *                          them, whereas all the regular arguments must
 *                          (tstIEMCheckMc ensures this).
 * @param   fFlushPendingWrites Flag whether to flush pending writes (default true),
 *                              this will still flush pending writes in call volatile registers if false.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCallCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs, uint8_t cHiddenArgs,
                        bool fFlushPendingWrites /*= true*/)
{
#ifdef VBOX_STRICT
    /*
     * Assert sanity.
     */
    Assert(cArgs <= IEMNATIVE_CALL_MAX_ARG_COUNT);
    Assert(cHiddenArgs < IEMNATIVE_CALL_ARG_GREG_COUNT);
    for (unsigned i = 0; i < cHiddenArgs; i++)
        Assert(pReNative->Core.aidxArgVars[i] == UINT8_MAX);
    for (unsigned i = cHiddenArgs; i < cArgs; i++)
    {
        Assert(pReNative->Core.aidxArgVars[i] != UINT8_MAX); /* checked by tstIEMCheckMc.cpp */
        Assert(pReNative->Core.bmVars & RT_BIT_32(pReNative->Core.aidxArgVars[i]));
    }
    iemNativeRegAssertSanity(pReNative);
#endif

    /* We don't know what the called function makes use of, so flush any pending register writes. */
    RT_NOREF(fFlushPendingWrites);
#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    if (fFlushPendingWrites)
#endif
        off = iemNativeRegFlushPendingWrites(pReNative, off);

    /*
     * Before we do anything else, go over variables that are referenced and
     * make sure they are not in a register.
     */
    uint32_t bmVars = pReNative->Core.bmVars;
    if (bmVars)
    {
        do
        {
            uint8_t const idxVar = ASMBitFirstSetU32(bmVars) - 1;
            bmVars &= ~RT_BIT_32(idxVar);

            if (pReNative->Core.aVars[idxVar].idxReferrerVar != UINT8_MAX)
            {
                uint8_t const idxRegOld = pReNative->Core.aVars[idxVar].idxReg;
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
                if (   idxRegOld != UINT8_MAX
                    && pReNative->Core.aVars[idxVar].fSimdReg)
                {
                    Assert(idxRegOld < RT_ELEMENTS(pReNative->Core.aHstSimdRegs));
                    Assert(pReNative->Core.aVars[idxVar].cbVar == sizeof(RTUINT128U) || pReNative->Core.aVars[idxVar].cbVar == sizeof(RTUINT256U));

                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, IEMNATIVE_VAR_IDX_PACK(idxVar));
                    Log12(("iemNativeEmitCallCommon: spilling idxVar=%d/%#x/idxReg=%d (referred to by %d) onto the stack (slot %#x bp+%d, off=%#x)\n",
                           idxVar, IEMNATIVE_VAR_IDX_PACK(idxVar), idxRegOld, pReNative->Core.aVars[idxVar].idxReferrerVar,
                           idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
                    if (pReNative->Core.aVars[idxVar].cbVar == sizeof(RTUINT128U))
                        off = iemNativeEmitStoreVecRegByBpU128(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);
                    else
                        off = iemNativeEmitStoreVecRegByBpU256(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);

                    Assert(!(   (pReNative->Core.bmGstSimdRegShadowDirtyLo128 | pReNative->Core.bmGstSimdRegShadowDirtyHi128)
                              & pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows));

                    pReNative->Core.aVars[idxVar].idxReg       = UINT8_MAX;
                    pReNative->Core.bmHstSimdRegs              &= ~RT_BIT_32(idxRegOld);
                    pReNative->Core.bmHstSimdRegsWithGstShadow &= ~RT_BIT_32(idxRegOld);
                    pReNative->Core.bmGstSimdRegShadows        &= ~pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows;
                    pReNative->Core.aHstSimdRegs[idxRegOld].fGstRegShadows = 0;
                }
                else
#endif
                if (idxRegOld < RT_ELEMENTS(pReNative->Core.aHstRegs))
                {
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, IEMNATIVE_VAR_IDX_PACK(idxVar));
                    Log12(("iemNativeEmitCallCommon: spilling idxVar=%d/%#x/idxReg=%d (referred to by %d) onto the stack (slot %#x bp+%d, off=%#x)\n",
                           idxVar, IEMNATIVE_VAR_IDX_PACK(idxVar), idxRegOld, pReNative->Core.aVars[idxVar].idxReferrerVar,
                           idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
                    off = iemNativeEmitStoreGprByBp(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);

                    pReNative->Core.aVars[idxVar].idxReg    = UINT8_MAX;
                    pReNative->Core.bmHstRegs              &= ~RT_BIT_32(idxRegOld);
                    pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxRegOld);
                    pReNative->Core.bmGstRegShadows        &= ~pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows;
                    pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows = 0;
                }
            }
        } while (bmVars != 0);
#if 0 //def VBOX_STRICT
        iemNativeRegAssertSanity(pReNative);
#endif
    }

    uint8_t const cRegArgs = RT_MIN(cArgs, RT_ELEMENTS(g_aidxIemNativeCallRegs));

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
    /*
     * At the very first step go over the host registers that will be used for arguments
     * don't shadow anything which needs writing back first.
     */
    for (uint32_t i = 0; i < cRegArgs; i++)
    {
        uint8_t const idxArgReg = g_aidxIemNativeCallRegs[i];

        /* Writeback any dirty guest shadows before using this register. */
        if (pReNative->Core.bmGstRegShadowDirty & pReNative->Core.aHstRegs[idxArgReg].fGstRegShadows)
            off = iemNativeRegFlushDirtyGuestByHostRegShadow(pReNative, off, idxArgReg);
        Assert(!(pReNative->Core.bmGstRegShadowDirty & pReNative->Core.aHstRegs[idxArgReg].fGstRegShadows));
    }
#endif

    /*
     * First, go over the host registers that will be used for arguments and make
     * sure they either hold the desired argument or are free.
     */
    if (pReNative->Core.bmHstRegs & g_afIemNativeCallRegs[cRegArgs])
    {
        for (uint32_t i = 0; i < cRegArgs; i++)
        {
            uint8_t const idxArgReg = g_aidxIemNativeCallRegs[i];
            if (pReNative->Core.bmHstRegs & RT_BIT_32(idxArgReg))
            {
                if (pReNative->Core.aHstRegs[idxArgReg].enmWhat == kIemNativeWhat_Var)
                {
                    uint8_t const       idxVar = pReNative->Core.aHstRegs[idxArgReg].idxVar;
                    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
                    PIEMNATIVEVAR const pVar   = &pReNative->Core.aVars[IEMNATIVE_VAR_IDX_UNPACK(idxVar)];
                    Assert(pVar->idxReg == idxArgReg);
                    uint8_t const       uArgNo = pVar->uArgNo;
                    if (uArgNo == i)
                    { /* prefect */ }
                    /* The variable allocator logic should make sure this is impossible,
                       except for when the return register is used as a parameter (ARM,
                       but not x86). */
#if RT_BIT_32(IEMNATIVE_CALL_RET_GREG) & IEMNATIVE_CALL_ARGS_GREG_MASK
                    else if (idxArgReg == IEMNATIVE_CALL_RET_GREG && uArgNo != UINT8_MAX)
                    {
# ifdef IEMNATIVE_FP_OFF_STACK_ARG0
#  error "Implement this"
# endif
                        Assert(uArgNo < IEMNATIVE_CALL_ARG_GREG_COUNT);
                        uint8_t const idxFinalArgReg = g_aidxIemNativeCallRegs[uArgNo];
                        AssertStmt(!(pReNative->Core.bmHstRegs & RT_BIT_32(idxFinalArgReg)),
                                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_10));
                        off = iemNativeRegMoveVar(pReNative, off, idxVar, idxArgReg, idxFinalArgReg, "iemNativeEmitCallCommon");
                    }
#endif
                    else
                    {
                        AssertStmt(uArgNo == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_10));

                        if (pVar->enmKind == kIemNativeVarKind_Stack)
                            off = iemNativeRegMoveOrSpillStackVar(pReNative, off, idxVar);
                        else
                        {
                            /* just free it, can be reloaded if used again */
                            pVar->idxReg               = UINT8_MAX;
                            pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxArgReg);
                            iemNativeRegClearGstRegShadowing(pReNative, idxArgReg, off);
                        }
                    }
                }
                else
                    AssertStmt(pReNative->Core.aHstRegs[idxArgReg].enmWhat == kIemNativeWhat_Arg,
                               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_8));
            }
        }
#if 0 //def VBOX_STRICT
        iemNativeRegAssertSanity(pReNative);
#endif
    }

    Assert(!(pReNative->Core.bmHstRegs & g_afIemNativeCallRegs[cHiddenArgs])); /* No variables for hidden arguments. */

#ifdef IEMNATIVE_FP_OFF_STACK_ARG0
    /*
     * If there are any stack arguments, make sure they are in their place as well.
     *
     * We can use IEMNATIVE_CALL_ARG0_GREG as temporary register since we'll (or
     * the caller) be loading it later and it must be free (see first loop).
     */
    if (cArgs > IEMNATIVE_CALL_ARG_GREG_COUNT)
    {
        for (unsigned i = IEMNATIVE_CALL_ARG_GREG_COUNT; i < cArgs; i++)
        {
            PIEMNATIVEVAR const pVar      = &pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]]; /* unpacked */
            int32_t const       offBpDisp = g_aoffIemNativeCallStackArgBpDisp[i - IEMNATIVE_CALL_ARG_GREG_COUNT];
            if (pVar->idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
            {
                Assert(pVar->enmKind == kIemNativeVarKind_Stack); /* Imm as well? */
                off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, pVar->idxReg);
                pReNative->Core.bmHstRegs &= ~RT_BIT_32(pVar->idxReg);
                pVar->idxReg = UINT8_MAX;
            }
            else
            {
                /* Use ARG0 as temp for stuff we need registers for. */
                switch (pVar->enmKind)
                {
                    case kIemNativeVarKind_Stack:
                    {
                        uint8_t const idxStackSlot = pVar->idxStackSlot;
                        AssertStmt(idxStackSlot != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
                        off = iemNativeEmitLoadGprByBp(pReNative, off, IEMNATIVE_CALL_ARG0_GREG /* is free */,
                                                       iemNativeStackCalcBpDisp(idxStackSlot));
                        off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, IEMNATIVE_CALL_ARG0_GREG);
                        continue;
                    }

                    case kIemNativeVarKind_Immediate:
                        off = iemNativeEmitStoreImm64ByBp(pReNative, off, offBpDisp, pVar->u.uValue);
                        continue;

                    case kIemNativeVarKind_VarRef:
                    {
                        uint8_t const idxOtherVar    = pVar->u.idxRefVar; /* unpacked */
                        Assert(idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars));
                        uint8_t const idxStackSlot   = iemNativeVarGetStackSlot(pReNative, IEMNATIVE_VAR_IDX_PACK(idxOtherVar));
                        int32_t const offBpDispOther = iemNativeStackCalcBpDisp(idxStackSlot);
                        uint8_t const idxRegOther    = pReNative->Core.aVars[idxOtherVar].idxReg;
# ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
                        bool    const fSimdReg       = pReNative->Core.aVars[idxOtherVar].fSimdReg;
                        uint8_t const cbVar          = pReNative->Core.aVars[idxOtherVar].cbVar;
                        if (   fSimdReg
                            && idxRegOther != UINT8_MAX)
                        {
                            Assert(idxRegOther < RT_ELEMENTS(pReNative->Core.aHstSimdRegs));
                            if (cbVar == sizeof(RTUINT128U))
                                off = iemNativeEmitStoreVecRegByBpU128(pReNative, off, offBpDispOther, idxRegOther);
                            else
                                off = iemNativeEmitStoreVecRegByBpU256(pReNative, off, offBpDispOther, idxRegOther);
                            iemNativeSimdRegFreeVar(pReNative, idxRegOther, true); /** @todo const ref? */
                            Assert(pReNative->Core.aVars[idxOtherVar].idxReg == UINT8_MAX);
                        }
                        else
# endif
                        if (idxRegOther < RT_ELEMENTS(pReNative->Core.aHstRegs))
                        {
                            off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDispOther, idxRegOther);
                            iemNativeRegFreeVar(pReNative, idxRegOther, true); /** @todo const ref? */
                            Assert(pReNative->Core.aVars[idxOtherVar].idxReg == UINT8_MAX);
                        }
                        Assert(   pReNative->Core.aVars[idxOtherVar].idxStackSlot != UINT8_MAX
                               && pReNative->Core.aVars[idxOtherVar].idxReg       == UINT8_MAX);
                        off = iemNativeEmitLeaGprByBp(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, offBpDispOther);
                        off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, IEMNATIVE_CALL_ARG0_GREG);
                        continue;
                    }

                    case kIemNativeVarKind_GstRegRef:
                        off = iemNativeEmitLeaGprByGstRegRef(pReNative, off, IEMNATIVE_CALL_ARG0_GREG,
                                                             pVar->u.GstRegRef.enmClass, pVar->u.GstRegRef.idx);
                        off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, IEMNATIVE_CALL_ARG0_GREG);
                        continue;

                    case kIemNativeVarKind_Invalid:
                    case kIemNativeVarKind_End:
                        break;
                }
                AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_3));
            }
        }
# if 0 //def VBOX_STRICT
        iemNativeRegAssertSanity(pReNative);
# endif
    }
#else
    AssertCompile(IEMNATIVE_CALL_MAX_ARG_COUNT <= IEMNATIVE_CALL_ARG_GREG_COUNT);
#endif

    /*
     * Make sure the argument variables are loaded into their respective registers.
     *
     * We can optimize this by ASSUMING that any register allocations are for
     * registeres that have already been loaded and are ready.  The previous step
     * saw to that.
     */
    if (~pReNative->Core.bmHstRegs & (g_afIemNativeCallRegs[cRegArgs] & ~g_afIemNativeCallRegs[cHiddenArgs]))
    {
        for (unsigned i = cHiddenArgs; i < cRegArgs; i++)
        {
            uint8_t const idxArgReg = g_aidxIemNativeCallRegs[i];
            if (pReNative->Core.bmHstRegs & RT_BIT_32(idxArgReg))
                Assert(   pReNative->Core.aHstRegs[idxArgReg].idxVar == IEMNATIVE_VAR_IDX_PACK(pReNative->Core.aidxArgVars[i])
                       && pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]].uArgNo == i
                       && pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]].idxReg == idxArgReg);
            else
            {
                PIEMNATIVEVAR const pVar = &pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]]; /* unpacked */
                if (pVar->idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
                {
                    Assert(pVar->enmKind == kIemNativeVarKind_Stack);
                    off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxArgReg, pVar->idxReg);
                    pReNative->Core.bmHstRegs = (pReNative->Core.bmHstRegs & ~RT_BIT_32(pVar->idxReg))
                                              | RT_BIT_32(idxArgReg);
                    pVar->idxReg = idxArgReg;
                }
                else
                {
                    /* Use ARG0 as temp for stuff we need registers for. */
                    switch (pVar->enmKind)
                    {
                        case kIemNativeVarKind_Stack:
                        {
                            uint8_t const idxStackSlot = pVar->idxStackSlot;
                            AssertStmt(idxStackSlot != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
                            off = iemNativeEmitLoadGprByBp(pReNative, off, idxArgReg, iemNativeStackCalcBpDisp(idxStackSlot));
                            continue;
                        }

                        case kIemNativeVarKind_Immediate:
                            off = iemNativeEmitLoadGprImm64(pReNative, off, idxArgReg, pVar->u.uValue);
                            continue;

                        case kIemNativeVarKind_VarRef:
                        {
                            uint8_t const idxOtherVar    = pVar->u.idxRefVar; /* unpacked */
                            Assert(idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars));
                            uint8_t const idxStackSlot   = iemNativeVarGetStackSlot(pReNative,
                                                                                    IEMNATIVE_VAR_IDX_PACK(idxOtherVar));
                            int32_t const offBpDispOther = iemNativeStackCalcBpDisp(idxStackSlot);
                            uint8_t const idxRegOther    = pReNative->Core.aVars[idxOtherVar].idxReg;
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
                            bool    const fSimdReg       = pReNative->Core.aVars[idxOtherVar].fSimdReg;
                            uint8_t const cbVar          = pReNative->Core.aVars[idxOtherVar].cbVar;
                            if (   fSimdReg
                                && idxRegOther != UINT8_MAX)
                            {
                                Assert(idxRegOther < RT_ELEMENTS(pReNative->Core.aHstSimdRegs));
                                if (cbVar == sizeof(RTUINT128U))
                                    off = iemNativeEmitStoreVecRegByBpU128(pReNative, off, offBpDispOther, idxRegOther);
                                else
                                    off = iemNativeEmitStoreVecRegByBpU256(pReNative, off, offBpDispOther, idxRegOther);
                                iemNativeSimdRegFreeVar(pReNative, idxRegOther, true); /** @todo const ref? */
                                Assert(pReNative->Core.aVars[idxOtherVar].idxReg == UINT8_MAX);
                            }
                            else
#endif
                            if (idxRegOther < RT_ELEMENTS(pReNative->Core.aHstRegs))
                            {
                                off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDispOther, idxRegOther);
                                iemNativeRegFreeVar(pReNative, idxRegOther, true); /** @todo const ref? */
                                Assert(pReNative->Core.aVars[idxOtherVar].idxReg == UINT8_MAX);
                            }
                            Assert(   pReNative->Core.aVars[idxOtherVar].idxStackSlot != UINT8_MAX
                                   && pReNative->Core.aVars[idxOtherVar].idxReg       == UINT8_MAX);
                            off = iemNativeEmitLeaGprByBp(pReNative, off, idxArgReg, offBpDispOther);
                            continue;
                        }

                        case kIemNativeVarKind_GstRegRef:
                            off = iemNativeEmitLeaGprByGstRegRef(pReNative, off, idxArgReg,
                                                                 pVar->u.GstRegRef.enmClass, pVar->u.GstRegRef.idx);
                            continue;

                        case kIemNativeVarKind_Invalid:
                        case kIemNativeVarKind_End:
                            break;
                    }
                    AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_3));
                }
            }
        }
#if 0 //def VBOX_STRICT
        iemNativeRegAssertSanity(pReNative);
#endif
    }
#ifdef VBOX_STRICT
    else
        for (unsigned i = cHiddenArgs; i < cRegArgs; i++)
        {
            Assert(pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]].uArgNo == i);
            Assert(pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]].idxReg == g_aidxIemNativeCallRegs[i]);
        }
#endif

    /*
     * Free all argument variables (simplified).
     * Their lifetime always expires with the call they are for.
     */
    /** @todo Make the python script check that arguments aren't used after
     *        IEM_MC_CALL_XXXX. */
    /** @todo There is a special with IEM_MC_MEM_MAP_U16_RW and friends requiring
     *        a IEM_MC_MEM_COMMIT_AND_UNMAP_RW after a AIMPL call typically with
     *        an argument value.  There is also some FPU stuff. */
    for (uint32_t i = cHiddenArgs; i < cArgs; i++)
    {
        uint8_t const idxVar = pReNative->Core.aidxArgVars[i]; /* unpacked */
        Assert(idxVar < RT_ELEMENTS(pReNative->Core.aVars));

        /* no need to free registers: */
        AssertMsg(i < IEMNATIVE_CALL_ARG_GREG_COUNT
                  ?    pReNative->Core.aVars[idxVar].idxReg == g_aidxIemNativeCallRegs[i]
                    || pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX
                  : pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX,
                  ("i=%d idxVar=%d idxReg=%d, expected %d\n", i, idxVar, pReNative->Core.aVars[idxVar].idxReg,
                   i < IEMNATIVE_CALL_ARG_GREG_COUNT ? g_aidxIemNativeCallRegs[i] : UINT8_MAX));

        pReNative->Core.aidxArgVars[i] = UINT8_MAX;
        pReNative->Core.bmVars        &= ~RT_BIT_32(idxVar);
        iemNativeVarFreeStackSlots(pReNative, idxVar);
    }
    Assert(pReNative->Core.u64ArgVars == UINT64_MAX);

    /*
     * Flush volatile registers as we make the call.
     */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, cRegArgs);

    return off;
}



/*********************************************************************************************************************************
*   TLB Lookup.                                                                                                                  *
*********************************************************************************************************************************/

/**
 * This is called via iemNativeHlpAsmSafeWrapCheckTlbLookup.
 */
DECLASM(void) iemNativeHlpCheckTlbLookup(PVMCPU pVCpu, uintptr_t uResult, uint64_t GCPtr, uint32_t uSegAndSizeAndAccess)
{
    uint8_t const  iSegReg = RT_BYTE1(uSegAndSizeAndAccess);
    uint8_t const  cbMem   = RT_BYTE2(uSegAndSizeAndAccess);
    uint32_t const fAccess = uSegAndSizeAndAccess >> 16;
    Log(("iemNativeHlpCheckTlbLookup: %x:%#RX64 LB %#x fAccess=%#x -> %#RX64\n", iSegReg, GCPtr, cbMem, fAccess, uResult));

    /* Do the lookup manually. */
    RTGCPTR const      GCPtrFlat = iSegReg == UINT8_MAX ? GCPtr : GCPtr + pVCpu->cpum.GstCtx.aSRegs[iSegReg].u64Base;
    uint64_t const     uTag      = IEMTLB_CALC_TAG(    &pVCpu->iem.s.DataTlb, GCPtrFlat);
    PIEMTLBENTRY const pTlbe     = IEMTLB_TAG_TO_ENTRY(&pVCpu->iem.s.DataTlb, uTag);
    if (RT_LIKELY(pTlbe->uTag == uTag))
    {
        /*
         * Check TLB page table level access flags.
         */
        AssertCompile(IEMTLBE_F_PT_NO_USER == 4);
        uint64_t const fNoUser          = (IEM_GET_CPL(pVCpu) + 1) & IEMTLBE_F_PT_NO_USER;
        uint64_t const fNoWriteNoDirty  = !(fAccess & IEM_ACCESS_TYPE_WRITE) ? 0
                                        : IEMTLBE_F_PT_NO_WRITE | IEMTLBE_F_PT_NO_DIRTY | IEMTLBE_F_PG_NO_WRITE;
        uint64_t const fFlagsAndPhysRev = pTlbe->fFlagsAndPhysRev & (  IEMTLBE_F_PHYS_REV       | IEMTLBE_F_NO_MAPPINGR3
                                                                     | IEMTLBE_F_PG_UNASSIGNED
                                                                     | IEMTLBE_F_PT_NO_ACCESSED
                                                                     | fNoWriteNoDirty          | fNoUser);
        uint64_t const uTlbPhysRev      = pVCpu->iem.s.DataTlb.uTlbPhysRev;
        if (RT_LIKELY(fFlagsAndPhysRev == uTlbPhysRev))
        {
            /*
             * Return the address.
             */
            uint8_t const * const pbAddr = &pTlbe->pbMappingR3[GCPtrFlat & GUEST_PAGE_OFFSET_MASK];
            if ((uintptr_t)pbAddr == uResult)
                return;
            RT_NOREF(cbMem);
            AssertFailed();
        }
        else
            AssertMsgFailed(("fFlagsAndPhysRev=%#RX64 vs uTlbPhysRev=%#RX64: %#RX64\n",
                             fFlagsAndPhysRev, uTlbPhysRev, fFlagsAndPhysRev ^ uTlbPhysRev));
    }
    else
        AssertFailed();
    RT_BREAKPOINT();
}

/* The rest of the code is in IEMN8veRecompilerTlbLookup.h. */



/*********************************************************************************************************************************
*   Recompiler Core.                                                                                                             *
*********************************************************************************************************************************/

/** @callback_method_impl{FNDISREADBYTES, Dummy.} */
static DECLCALLBACK(int) iemNativeDisasReadBytesDummy(PDISSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead)
{
    RT_BZERO(&pDis->Instr.ab[offInstr], cbMaxRead);
    pDis->cbCachedInstr += cbMaxRead;
    RT_NOREF(cbMinRead);
    return VERR_NO_DATA;
}


DECLHIDDEN(const char *) iemNativeDbgVCpuOffsetToName(uint32_t off)
{
    static struct { uint32_t off; const char *pszName; } const s_aMembers[] =
    {
#define ENTRY(a_Member) { (uint32_t)RT_UOFFSETOF(VMCPUCC, a_Member), #a_Member } /* cast is for stupid MSC */
        ENTRY(fLocalForcedActions),
        ENTRY(iem.s.rcPassUp),
        ENTRY(iem.s.fExec),
        ENTRY(iem.s.pbInstrBuf),
        ENTRY(iem.s.uInstrBufPc),
        ENTRY(iem.s.GCPhysInstrBuf),
        ENTRY(iem.s.cbInstrBufTotal),
        ENTRY(iem.s.idxTbCurInstr),
#ifdef VBOX_WITH_STATISTICS
        ENTRY(iem.s.StatNativeTlbHitsForFetch),
        ENTRY(iem.s.StatNativeTlbHitsForStore),
        ENTRY(iem.s.StatNativeTlbHitsForStack),
        ENTRY(iem.s.StatNativeTlbHitsForMapped),
        ENTRY(iem.s.StatNativeCodeTlbMissesNewPage),
        ENTRY(iem.s.StatNativeCodeTlbHitsForNewPage),
        ENTRY(iem.s.StatNativeCodeTlbMissesNewPageWithOffset),
        ENTRY(iem.s.StatNativeCodeTlbHitsForNewPageWithOffset),
#endif
        ENTRY(iem.s.DataTlb.aEntries),
        ENTRY(iem.s.DataTlb.uTlbRevision),
        ENTRY(iem.s.DataTlb.uTlbPhysRev),
        ENTRY(iem.s.DataTlb.cTlbHits),
        ENTRY(iem.s.CodeTlb.aEntries),
        ENTRY(iem.s.CodeTlb.uTlbRevision),
        ENTRY(iem.s.CodeTlb.uTlbPhysRev),
        ENTRY(iem.s.CodeTlb.cTlbHits),
        ENTRY(pVMR3),
        ENTRY(cpum.GstCtx.rax),
        ENTRY(cpum.GstCtx.ah),
        ENTRY(cpum.GstCtx.rcx),
        ENTRY(cpum.GstCtx.ch),
        ENTRY(cpum.GstCtx.rdx),
        ENTRY(cpum.GstCtx.dh),
        ENTRY(cpum.GstCtx.rbx),
        ENTRY(cpum.GstCtx.bh),
        ENTRY(cpum.GstCtx.rsp),
        ENTRY(cpum.GstCtx.rbp),
        ENTRY(cpum.GstCtx.rsi),
        ENTRY(cpum.GstCtx.rdi),
        ENTRY(cpum.GstCtx.r8),
        ENTRY(cpum.GstCtx.r9),
        ENTRY(cpum.GstCtx.r10),
        ENTRY(cpum.GstCtx.r11),
        ENTRY(cpum.GstCtx.r12),
        ENTRY(cpum.GstCtx.r13),
        ENTRY(cpum.GstCtx.r14),
        ENTRY(cpum.GstCtx.r15),
        ENTRY(cpum.GstCtx.es.Sel),
        ENTRY(cpum.GstCtx.es.u64Base),
        ENTRY(cpum.GstCtx.es.u32Limit),
        ENTRY(cpum.GstCtx.es.Attr),
        ENTRY(cpum.GstCtx.cs.Sel),
        ENTRY(cpum.GstCtx.cs.u64Base),
        ENTRY(cpum.GstCtx.cs.u32Limit),
        ENTRY(cpum.GstCtx.cs.Attr),
        ENTRY(cpum.GstCtx.ss.Sel),
        ENTRY(cpum.GstCtx.ss.u64Base),
        ENTRY(cpum.GstCtx.ss.u32Limit),
        ENTRY(cpum.GstCtx.ss.Attr),
        ENTRY(cpum.GstCtx.ds.Sel),
        ENTRY(cpum.GstCtx.ds.u64Base),
        ENTRY(cpum.GstCtx.ds.u32Limit),
        ENTRY(cpum.GstCtx.ds.Attr),
        ENTRY(cpum.GstCtx.fs.Sel),
        ENTRY(cpum.GstCtx.fs.u64Base),
        ENTRY(cpum.GstCtx.fs.u32Limit),
        ENTRY(cpum.GstCtx.fs.Attr),
        ENTRY(cpum.GstCtx.gs.Sel),
        ENTRY(cpum.GstCtx.gs.u64Base),
        ENTRY(cpum.GstCtx.gs.u32Limit),
        ENTRY(cpum.GstCtx.gs.Attr),
        ENTRY(cpum.GstCtx.rip),
        ENTRY(cpum.GstCtx.eflags),
        ENTRY(cpum.GstCtx.uRipInhibitInt),
        ENTRY(cpum.GstCtx.cr0),
        ENTRY(cpum.GstCtx.cr4),
        ENTRY(cpum.GstCtx.aXcr[0]),
        ENTRY(cpum.GstCtx.aXcr[1]),
#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
        ENTRY(cpum.GstCtx.XState.x87.aXMM[0]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[1]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[2]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[3]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[4]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[5]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[6]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[7]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[8]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[9]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[10]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[11]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[12]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[13]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[14]),
        ENTRY(cpum.GstCtx.XState.x87.aXMM[15]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[0]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[1]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[2]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[3]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[4]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[5]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[6]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[7]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[8]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[9]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[10]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[11]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[12]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[13]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[14]),
        ENTRY(cpum.GstCtx.XState.u.YmmHi.aYmmHi[15])
#endif
#undef ENTRY
    };
#ifdef VBOX_STRICT
    static bool s_fOrderChecked = false;
    if (!s_fOrderChecked)
    {
        s_fOrderChecked = true;
        uint32_t offPrev = s_aMembers[0].off;
        for (unsigned i = 1; i < RT_ELEMENTS(s_aMembers); i++)
        {
            Assert(s_aMembers[i].off > offPrev);
            offPrev = s_aMembers[i].off;
        }
    }
#endif

    /*
     * Binary lookup.
     */
    unsigned iStart = 0;
    unsigned iEnd   = RT_ELEMENTS(s_aMembers);
    for (;;)
    {
        unsigned const iCur   = iStart + (iEnd - iStart) / 2;
        uint32_t const offCur = s_aMembers[iCur].off;
        if (off < offCur)
        {
            if (iCur != iStart)
                iEnd = iCur;
            else
                break;
        }
        else if (off > offCur)
        {
            if (iCur + 1 < iEnd)
                iStart = iCur + 1;
            else
                break;
        }
        else
            return s_aMembers[iCur].pszName;
    }
#ifdef VBOX_WITH_STATISTICS
    if (off - RT_UOFFSETOF(VMCPUCC, iem.s.acThreadedFuncStats) < RT_SIZEOFMEMB(VMCPUCC, iem.s.acThreadedFuncStats))
        return "iem.s.acThreadedFuncStats[iFn]";
#endif
    return NULL;
}


DECLHIDDEN(void) iemNativeDisassembleTb(PCIEMTB pTb, PCDBGFINFOHLP pHlp) RT_NOEXCEPT
{
    AssertReturnVoid((pTb->fFlags & IEMTB_F_TYPE_MASK) == IEMTB_F_TYPE_NATIVE);
#if defined(RT_ARCH_AMD64)
    static const char * const a_apszMarkers[] =
    {
        /*[0]=*/ "unknown0",        "CheckCsLim",           "ConsiderLimChecking",  "CheckOpcodes",
        /*[4]=*/ "PcAfterBranch",   "LoadTlbForNewPage",    "LoadTlbAfterBranch"
    };
#endif

    char                    szDisBuf[512];
    DISSTATE                Dis;
    PCIEMNATIVEINSTR const  paNative      = pTb->Native.paInstructions;
    uint32_t const          cNative       = pTb->Native.cInstructions;
    uint32_t                offNative     = 0;
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    PCIEMTBDBG const        pDbgInfo      = pTb->pDbgInfo;
#endif
    DISCPUMODE              enmGstCpuMode = (pTb->fFlags & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_16BIT ? DISCPUMODE_16BIT
                                          : (pTb->fFlags & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_32BIT ? DISCPUMODE_32BIT
                                          :                                                            DISCPUMODE_64BIT;
#if   defined(RT_ARCH_AMD64) && !defined(VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER)
    DISCPUMODE const        enmHstCpuMode = DISCPUMODE_64BIT;
#elif defined(RT_ARCH_ARM64) && !defined(VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER)
    DISCPUMODE const        enmHstCpuMode = DISCPUMODE_ARMV8_A64;
#elif !defined(VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER)
# error "Port me"
#else
    csh                     hDisasm       = ~(size_t)0;
# if defined(RT_ARCH_AMD64)
    cs_err                  rcCs          = cs_open(CS_ARCH_X86, CS_MODE_LITTLE_ENDIAN | CS_MODE_64, &hDisasm);
# elif defined(RT_ARCH_ARM64)
    cs_err                  rcCs          = cs_open(CS_ARCH_ARM64, CS_MODE_LITTLE_ENDIAN, &hDisasm);
# else
#  error "Port me"
# endif
    AssertMsgReturnVoid(rcCs == CS_ERR_OK, ("%d (%#x)\n", rcCs, rcCs));

    //rcCs = cs_option(hDisasm, CS_OPT_DETAIL, CS_OPT_ON);  - not needed as pInstr->detail doesn't provide full memory detail.
    //Assert(rcCs == CS_ERR_OK);
#endif

    /*
     * Print TB info.
     */
    pHlp->pfnPrintf(pHlp,
                    "pTb=%p: GCPhysPc=%RGp cInstructions=%u LB %#x cRanges=%u\n"
                    "pTb=%p: cUsed=%u msLastUsed=%u fFlags=%#010x %s\n",
                    pTb, pTb->GCPhysPc, pTb->cInstructions, pTb->cbOpcodes, pTb->cRanges,
                    pTb, pTb->cUsed, pTb->msLastUsed, pTb->fFlags, iemTbFlagsToString(pTb->fFlags, szDisBuf, sizeof(szDisBuf)));
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    if (pDbgInfo && pDbgInfo->cEntries > 1)
    {
        Assert(pDbgInfo->aEntries[0].Gen.uType == kIemTbDbgEntryType_NativeOffset);

        /*
         * This disassembly is driven by the debug info which follows the native
         * code and indicates when it starts with the next guest instructions,
         * where labels are and such things.
         */
        uint32_t                idxThreadedCall  = 0;
        uint32_t                fExec            = pTb->fFlags & UINT32_C(0x00ffffff);
        uint8_t                 idxRange         = UINT8_MAX;
        uint8_t const           cRanges          = RT_MIN(pTb->cRanges, RT_ELEMENTS(pTb->aRanges));
        uint32_t                offRange         = 0;
        uint32_t                offOpcodes       = 0;
        uint32_t const          cbOpcodes        = pTb->cbOpcodes;
        RTGCPHYS                GCPhysPc         = pTb->GCPhysPc;
        uint32_t const          cDbgEntries      = pDbgInfo->cEntries;
        uint32_t                iDbgEntry        = 1;
        uint32_t                offDbgNativeNext = pDbgInfo->aEntries[0].NativeOffset.offNative;

        while (offNative < cNative)
        {
            /* If we're at or have passed the point where the next chunk of debug
               info starts, process it. */
            if (offDbgNativeNext <= offNative)
            {
                offDbgNativeNext = UINT32_MAX;
                for (; iDbgEntry < cDbgEntries; iDbgEntry++)
                {
                    switch (pDbgInfo->aEntries[iDbgEntry].Gen.uType)
                    {
                        case kIemTbDbgEntryType_GuestInstruction:
                        {
                            /* Did the exec flag change? */
                            if (fExec != pDbgInfo->aEntries[iDbgEntry].GuestInstruction.fExec)
                            {
                                pHlp->pfnPrintf(pHlp,
                                                "  fExec change %#08x -> %#08x %s\n",
                                                fExec, pDbgInfo->aEntries[iDbgEntry].GuestInstruction.fExec,
                                                iemTbFlagsToString(pDbgInfo->aEntries[iDbgEntry].GuestInstruction.fExec,
                                                                   szDisBuf, sizeof(szDisBuf)));
                                fExec = pDbgInfo->aEntries[iDbgEntry].GuestInstruction.fExec;
                                enmGstCpuMode = (fExec & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_16BIT ? DISCPUMODE_16BIT
                                              : (fExec & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_32BIT ? DISCPUMODE_32BIT
                                              :                                                      DISCPUMODE_64BIT;
                            }

                            /* New opcode range? We need to fend up a spurious debug info entry here for cases
                               where the compilation was aborted before the opcode was recorded and the actual
                               instruction was translated to a threaded call.  This may happen when we run out
                               of ranges, or when some complicated interrupts/FFs are found to be pending or
                               similar.  So, we just deal with it here rather than in the compiler code as it
                               is a lot simpler to do here. */
                            if (   idxRange == UINT8_MAX
                                || idxRange >= cRanges
                                || offRange >= pTb->aRanges[idxRange].cbOpcodes)
                            {
                                idxRange += 1;
                                if (idxRange < cRanges)
                                    offRange = !idxRange ? 0 : offRange - pTb->aRanges[idxRange - 1].cbOpcodes;
                                else
                                    continue;
                                Assert(offOpcodes == pTb->aRanges[idxRange].offOpcodes + offRange);
                                GCPhysPc = pTb->aRanges[idxRange].offPhysPage
                                         + (pTb->aRanges[idxRange].idxPhysPage == 0
                                            ? pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK
                                            : pTb->aGCPhysPages[pTb->aRanges[idxRange].idxPhysPage - 1]);
                                pHlp->pfnPrintf(pHlp, "  Range #%u: GCPhysPc=%RGp LB %#x [idxPg=%d]\n",
                                                idxRange, GCPhysPc, pTb->aRanges[idxRange].cbOpcodes,
                                                pTb->aRanges[idxRange].idxPhysPage);
                                GCPhysPc += offRange;
                            }

                            /* Disassemble the instruction. */
                            //uint8_t const cbInstrMax = RT_MIN(pTb->aRanges[idxRange].cbOpcodes - offRange, 15);
                            uint8_t const cbInstrMax = RT_MIN(cbOpcodes - offOpcodes, 15);
                            uint32_t      cbInstr    = 1;
                            int rc = DISInstrWithPrefetchedBytes(GCPhysPc, enmGstCpuMode, DISOPTYPE_ALL,
                                                                 &pTb->pabOpcodes[offOpcodes], cbInstrMax,
                                                                 iemNativeDisasReadBytesDummy, NULL, &Dis, &cbInstr);
                            if (RT_SUCCESS(rc))
                            {
                                size_t cch = DISFormatYasmEx(&Dis, szDisBuf, sizeof(szDisBuf),
                                                             DIS_FMT_FLAGS_BYTES_WIDTH_MAKE(10) | DIS_FMT_FLAGS_BYTES_LEFT
                                                             | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_C_HEX,
                                                             NULL /*pfnGetSymbol*/, NULL /*pvUser*/);

                                static unsigned const s_offMarker  = 55;
                                static char const     s_szMarker[] = " ; <--- guest";
                                if (cch < s_offMarker)
                                {
                                    memset(&szDisBuf[cch], ' ', s_offMarker - cch);
                                    cch = s_offMarker;
                                }
                                if (cch + sizeof(s_szMarker) <= sizeof(szDisBuf))
                                    memcpy(&szDisBuf[cch], s_szMarker, sizeof(s_szMarker));

                                pHlp->pfnPrintf(pHlp, "  %%%%%RGp: %s\n", GCPhysPc, szDisBuf);
                            }
                            else
                            {
                                pHlp->pfnPrintf(pHlp, "  %%%%%RGp: %.*Rhxs - guest disassembly failure %Rrc\n",
                                                GCPhysPc, cbInstrMax, &pTb->pabOpcodes[offOpcodes], rc);
                                cbInstr = 1;
                            }
                            GCPhysPc   += cbInstr;
                            offOpcodes += cbInstr;
                            offRange   += cbInstr;
                            continue;
                        }

                        case kIemTbDbgEntryType_ThreadedCall:
                            pHlp->pfnPrintf(pHlp,
                                            "  Call #%u to %s (%u args) - %s\n",
                                            idxThreadedCall,
                                            g_apszIemThreadedFunctions[pDbgInfo->aEntries[iDbgEntry].ThreadedCall.enmCall],
                                            g_acIemThreadedFunctionUsedArgs[pDbgInfo->aEntries[iDbgEntry].ThreadedCall.enmCall],
                                            pDbgInfo->aEntries[iDbgEntry].ThreadedCall.fRecompiled ? "recompiled" : "todo");
                            idxThreadedCall++;
                            continue;

                        case kIemTbDbgEntryType_GuestRegShadowing:
                        {
                            PCIEMTBDBGENTRY const pEntry    = &pDbgInfo->aEntries[iDbgEntry];
                            const char * const    pszGstReg = g_aGstShadowInfo[pEntry->GuestRegShadowing.idxGstReg].pszName;
                            if (pEntry->GuestRegShadowing.idxHstReg == UINT8_MAX)
                                pHlp->pfnPrintf(pHlp, "  Guest register %s != host register %s\n", pszGstReg,
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstRegPrev]);
                            else if (pEntry->GuestRegShadowing.idxHstRegPrev == UINT8_MAX)
                                pHlp->pfnPrintf(pHlp, "  Guest register %s == host register %s \n", pszGstReg,
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstReg]);
                            else
                                pHlp->pfnPrintf(pHlp, "  Guest register %s == host register %s (previously in %s)\n", pszGstReg,
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstReg],
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstRegPrev]);
                            continue;
                        }

#ifdef IEMNATIVE_WITH_SIMD_REG_ALLOCATOR
                        case kIemTbDbgEntryType_GuestSimdRegShadowing:
                        {
                            PCIEMTBDBGENTRY const pEntry    = &pDbgInfo->aEntries[iDbgEntry];
                            const char * const    pszGstReg = g_aGstSimdShadowInfo[pEntry->GuestSimdRegShadowing.idxGstSimdReg].pszName;
                            if (pEntry->GuestSimdRegShadowing.idxHstSimdReg == UINT8_MAX)
                                pHlp->pfnPrintf(pHlp, "  Guest SIMD register %s != host SIMD register %s\n", pszGstReg,
                                                g_apszIemNativeHstSimdRegNames[pEntry->GuestSimdRegShadowing.idxHstSimdRegPrev]);
                            else if (pEntry->GuestSimdRegShadowing.idxHstSimdRegPrev == UINT8_MAX)
                                pHlp->pfnPrintf(pHlp, "  Guest SIMD register %s == host SIMD register %s\n", pszGstReg,
                                                g_apszIemNativeHstSimdRegNames[pEntry->GuestSimdRegShadowing.idxHstSimdReg]);
                            else
                                pHlp->pfnPrintf(pHlp, "  Guest SIMD register %s == host SIMD register %s (previously in %s)\n", pszGstReg,
                                                g_apszIemNativeHstSimdRegNames[pEntry->GuestSimdRegShadowing.idxHstSimdReg],
                                                g_apszIemNativeHstSimdRegNames[pEntry->GuestSimdRegShadowing.idxHstSimdRegPrev]);
                            continue;
                        }
#endif

                        case kIemTbDbgEntryType_Label:
                        {
                            const char *pszName    = "what_the_fudge";
                            const char *pszComment = "";
                            bool        fNumbered  = pDbgInfo->aEntries[iDbgEntry].Label.uData != 0;
                            switch ((IEMNATIVELABELTYPE)pDbgInfo->aEntries[iDbgEntry].Label.enmLabel)
                            {
                                case kIemNativeLabelType_Return:                pszName = "Return"; break;
                                case kIemNativeLabelType_ReturnBreak:           pszName = "ReturnBreak"; break;
                                case kIemNativeLabelType_ReturnWithFlags:       pszName = "ReturnWithFlags"; break;
                                case kIemNativeLabelType_NonZeroRetOrPassUp:    pszName = "NonZeroRetOrPassUp"; break;
                                case kIemNativeLabelType_RaiseDe:               pszName = "RaiseDe"; break;
                                case kIemNativeLabelType_RaiseUd:               pszName = "RaiseUd"; break;
                                case kIemNativeLabelType_RaiseSseRelated:       pszName = "RaiseSseRelated"; break;
                                case kIemNativeLabelType_RaiseAvxRelated:       pszName = "RaiseAvxRelated"; break;
                                case kIemNativeLabelType_RaiseSseAvxFpRelated:  pszName = "RaiseSseAvxFpRelated"; break;
                                case kIemNativeLabelType_RaiseNm:               pszName = "RaiseNm"; break;
                                case kIemNativeLabelType_RaiseGp0:              pszName = "RaiseGp0"; break;
                                case kIemNativeLabelType_RaiseMf:               pszName = "RaiseMf"; break;
                                case kIemNativeLabelType_RaiseXf:               pszName = "RaiseXf"; break;
                                case kIemNativeLabelType_ObsoleteTb:            pszName = "ObsoleteTb"; break;
                                case kIemNativeLabelType_NeedCsLimChecking:     pszName = "NeedCsLimChecking"; break;
                                case kIemNativeLabelType_CheckBranchMiss:       pszName = "CheckBranchMiss"; break;
                                case kIemNativeLabelType_If:
                                    pszName = "If";
                                    fNumbered = true;
                                    break;
                                case kIemNativeLabelType_Else:
                                    pszName = "Else";
                                    fNumbered = true;
                                    pszComment = "   ; regs state restored pre-if-block";
                                    break;
                                case kIemNativeLabelType_Endif:
                                    pszName = "Endif";
                                    fNumbered = true;
                                    break;
                                case kIemNativeLabelType_CheckIrq:
                                    pszName = "CheckIrq_CheckVM";
                                    fNumbered = true;
                                    break;
                                case kIemNativeLabelType_TlbLookup:
                                    pszName = "TlbLookup";
                                    fNumbered = true;
                                    break;
                                case kIemNativeLabelType_TlbMiss:
                                    pszName = "TlbMiss";
                                    fNumbered = true;
                                    break;
                                case kIemNativeLabelType_TlbDone:
                                    pszName = "TlbDone";
                                    fNumbered = true;
                                    break;
                                case kIemNativeLabelType_Invalid:
                                case kIemNativeLabelType_End:
                                    break;
                            }
                            if (fNumbered)
                                pHlp->pfnPrintf(pHlp, "  %s_%u:%s\n", pszName, pDbgInfo->aEntries[iDbgEntry].Label.uData, pszComment);
                            else
                                pHlp->pfnPrintf(pHlp, "  %s:\n", pszName);
                            continue;
                        }

                        case kIemTbDbgEntryType_NativeOffset:
                            offDbgNativeNext = pDbgInfo->aEntries[iDbgEntry].NativeOffset.offNative;
                            Assert(offDbgNativeNext > offNative);
                            break;

#ifdef IEMNATIVE_WITH_DELAYED_PC_UPDATING
                        case kIemTbDbgEntryType_DelayedPcUpdate:
                            pHlp->pfnPrintf(pHlp,
                                            "  Updating guest PC value by %u (cInstrSkipped=%u)\n",
                                            pDbgInfo->aEntries[iDbgEntry].DelayedPcUpdate.offPc,
                                            pDbgInfo->aEntries[iDbgEntry].DelayedPcUpdate.cInstrSkipped);
                            continue;
#endif

#ifdef IEMNATIVE_WITH_DELAYED_REGISTER_WRITEBACK
                        case kIemTbDbgEntryType_GuestRegDirty:
                        {
                            PCIEMTBDBGENTRY const pEntry    = &pDbgInfo->aEntries[iDbgEntry];
                            const char * const    pszGstReg =   pEntry->GuestRegDirty.fSimdReg
                                                              ? g_aGstSimdShadowInfo[pEntry->GuestRegDirty.idxGstReg].pszName
                                                              : g_aGstShadowInfo[pEntry->GuestRegDirty.idxGstReg].pszName;
                            const char * const    pszHstReg =   pEntry->GuestRegDirty.fSimdReg
                                                              ? g_apszIemNativeHstSimdRegNames[pEntry->GuestRegDirty.idxHstReg]
                                                              : g_apszIemNativeHstRegNames[pEntry->GuestRegDirty.idxHstReg];
                            pHlp->pfnPrintf(pHlp,
                                            "  Guest register %s (shadowed by %s) is now dirty\n",
                                            pszGstReg, pszHstReg);
                            continue;
                        }

                        case kIemTbDbgEntryType_GuestRegWriteback:
                            pHlp->pfnPrintf(pHlp,
                                            "  Writing dirty %s registers (gst %#RX64)\n",
                                            pDbgInfo->aEntries[iDbgEntry].GuestRegWriteback.fSimdReg ? "SIMD" : "general",
                                            pDbgInfo->aEntries[iDbgEntry].GuestRegWriteback.fGstReg);
                            continue;
#endif

                        default:
                            AssertFailed();
                    }
                    iDbgEntry++;
                    break;
                }
            }

            /*
             * Disassemble the next native instruction.
             */
            PCIEMNATIVEINSTR const pNativeCur = &paNative[offNative];
# ifndef VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER
            uint32_t               cbInstr    = sizeof(paNative[0]);
            int const              rc         = DISInstr(pNativeCur, enmHstCpuMode, &Dis, &cbInstr);
            if (RT_SUCCESS(rc))
            {
#  if defined(RT_ARCH_AMD64)
                if (Dis.pCurInstr->uOpcode == OP_NOP && cbInstr == 7) /* iemNativeEmitMarker */
                {
                    uint32_t const uInfo = *(uint32_t const *)&Dis.Instr.ab[3];
                    if (RT_HIWORD(uInfo) < kIemThreadedFunc_End)
                        pHlp->pfnPrintf(pHlp, "    %p: nop ; marker: call #%u to %s (%u args) - %s\n",
                                        pNativeCur, uInfo & 0x7fff, g_apszIemThreadedFunctions[RT_HIWORD(uInfo)],
                                        g_acIemThreadedFunctionUsedArgs[RT_HIWORD(uInfo)],
                                        uInfo & 0x8000 ? "recompiled" : "todo");
                    else if ((uInfo & ~RT_BIT_32(31)) < RT_ELEMENTS(a_apszMarkers))
                        pHlp->pfnPrintf(pHlp, "    %p: nop ; marker: %s\n", pNativeCur, a_apszMarkers[uInfo & ~RT_BIT_32(31)]);
                    else
                        pHlp->pfnPrintf(pHlp, "    %p: nop ; unknown marker: %#x (%d)\n", pNativeCur, uInfo, uInfo);
                }
                else
#  endif
                {
                    const char *pszAnnotation = NULL;
#  ifdef RT_ARCH_AMD64
                    DISFormatYasmEx(&Dis, szDisBuf, sizeof(szDisBuf),
                                    DIS_FMT_FLAGS_BYTES_WIDTH_MAKE(10) | DIS_FMT_FLAGS_BYTES_LEFT
                                    | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_C_HEX,
                                    NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
                    PCDISOPPARAM pMemOp;
                    if (DISUSE_IS_EFFECTIVE_ADDR(Dis.Param1.fUse))
                        pMemOp = &Dis.Param1;
                    else if (DISUSE_IS_EFFECTIVE_ADDR(Dis.Param2.fUse))
                        pMemOp = &Dis.Param2;
                    else if (DISUSE_IS_EFFECTIVE_ADDR(Dis.Param3.fUse))
                        pMemOp = &Dis.Param3;
                    else
                        pMemOp = NULL;
                    if (   pMemOp
                        && pMemOp->x86.Base.idxGenReg == IEMNATIVE_REG_FIXED_PVMCPU
                        && (pMemOp->fUse & (DISUSE_BASE | DISUSE_REG_GEN64)) == (DISUSE_BASE | DISUSE_REG_GEN64))
                        pszAnnotation = iemNativeDbgVCpuOffsetToName(pMemOp->fUse & DISUSE_DISPLACEMENT32
                                                                     ? pMemOp->x86.uDisp.u32 : pMemOp->x86.uDisp.u8);

#elif defined(RT_ARCH_ARM64)
                    DISFormatArmV8Ex(&Dis, szDisBuf, sizeof(szDisBuf),
                                     DIS_FMT_FLAGS_BYTES_LEFT | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_C_HEX,
                                     NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
#  else
#   error "Port me"
#  endif
                    if (pszAnnotation)
                    {
                        static unsigned const s_offAnnotation = 55;
                        size_t const          cchAnnotation   = strlen(pszAnnotation);
                        size_t                cchDis          = strlen(szDisBuf);
                        if (RT_MAX(cchDis, s_offAnnotation) + sizeof(" ; ") + cchAnnotation <= sizeof(szDisBuf))
                        {
                            if (cchDis < s_offAnnotation)
                            {
                                memset(&szDisBuf[cchDis], ' ', s_offAnnotation - cchDis);
                                cchDis = s_offAnnotation;
                            }
                            szDisBuf[cchDis++] = ' ';
                            szDisBuf[cchDis++] = ';';
                            szDisBuf[cchDis++] = ' ';
                            memcpy(&szDisBuf[cchDis], pszAnnotation, cchAnnotation + 1);
                        }
                    }
                    pHlp->pfnPrintf(pHlp, "    %p: %s\n", pNativeCur, szDisBuf);
                }
            }
            else
            {
#  if defined(RT_ARCH_AMD64)
                pHlp->pfnPrintf(pHlp, "    %p:  %.*Rhxs - disassembly failure %Rrc\n",
                                pNativeCur, RT_MIN(cNative - offNative, 16), pNativeCur, rc);
#  elif defined(RT_ARCH_ARM64)
                pHlp->pfnPrintf(pHlp, "    %p:  %#010RX32 - disassembly failure %Rrc\n", pNativeCur, *pNativeCur, rc);
#  else
#   error "Port me"
#  endif
                cbInstr = sizeof(paNative[0]);
            }
            offNative += cbInstr / sizeof(paNative[0]);

#  else  /* VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER */
            cs_insn *pInstr;
            size_t   cInstrs = cs_disasm(hDisasm, (const uint8_t *)pNativeCur, (cNative - offNative) * sizeof(*pNativeCur),
                                         (uintptr_t)pNativeCur, 1, &pInstr);
            if (cInstrs > 0)
            {
                Assert(cInstrs == 1);
                const char *pszAnnotation = NULL;
#  if defined(RT_ARCH_ARM64)
                if (   (pInstr->id >= ARM64_INS_LD1 && pInstr->id < ARM64_INS_LSL)
                    || (pInstr->id >= ARM64_INS_ST1 && pInstr->id < ARM64_INS_SUB))
                {
                    /* This is bit crappy, but the disassembler provides incomplete addressing details. */
                    AssertCompile(IEMNATIVE_REG_FIXED_PVMCPU == 28 && IEMNATIVE_REG_FIXED_PCPUMCTX == 27);
                    char *psz = strchr(pInstr->op_str, '[');
                    if (psz && psz[1] == 'x' && psz[2] == '2' && (psz[3] == '7' || psz[3] == '8'))
                    {
                        uint32_t const offVCpu = psz[3] == '8'? 0 : RT_UOFFSETOF(VMCPU, cpum.GstCtx);
                        int32_t        off     = -1;
                        psz += 4;
                        if (*psz == ']')
                            off = 0;
                        else if (*psz == ',')
                        {
                            psz = RTStrStripL(psz + 1);
                            if (*psz == '#')
                                off = RTStrToInt32(&psz[1]);
                            /** @todo deal with index registers and LSL as well... */
                        }
                        if (off >= 0)
                            pszAnnotation = iemNativeDbgVCpuOffsetToName(offVCpu + (uint32_t)off);
                    }
                }
#  endif

                size_t const cchOp = strlen(pInstr->op_str);
#  if defined(RT_ARCH_AMD64)
                if (pszAnnotation)
                    pHlp->pfnPrintf(pHlp, "    %p: %.*Rhxs %-7s %s%*s ; %s\n",
                                    pNativeCur, pInstr->size, pNativeCur, pInstr->mnemonic, pInstr->op_str,
                                    cchOp < 55 ? 55 - cchOp : 0, "", pszAnnotation);
                else
                    pHlp->pfnPrintf(pHlp, "    %p: %.*Rhxs %-7s %s\n",
                                    pNativeCur, pInstr->size, pNativeCur, pInstr->mnemonic, pInstr->op_str);

#  else
                if (pszAnnotation)
                    pHlp->pfnPrintf(pHlp, "    %p: %#010RX32 %-7s %s%*s ; %s\n",
                                    pNativeCur, *pNativeCur, pInstr->mnemonic, pInstr->op_str,
                                    cchOp < 55 ? 55 - cchOp : 0, "", pszAnnotation);
                else
                    pHlp->pfnPrintf(pHlp, "    %p: %#010RX32 %-7s %s\n",
                                    pNativeCur, *pNativeCur, pInstr->mnemonic, pInstr->op_str);
#  endif
                offNative += pInstr->size / sizeof(*pNativeCur);
                cs_free(pInstr, cInstrs);
            }
            else
            {
#  if defined(RT_ARCH_AMD64)
                pHlp->pfnPrintf(pHlp, "    %p:  %.*Rhxs - disassembly failure %d\n",
                                pNativeCur, RT_MIN(cNative - offNative, 16), pNativeCur, cs_errno(hDisasm)));
#  else
                pHlp->pfnPrintf(pHlp, "    %p:  %#010RX32 - disassembly failure %d\n", pNativeCur, *pNativeCur, cs_errno(hDisasm));
#  endif
                offNative++;
            }
# endif /* VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER */
        }
    }
    else
#endif /* IEMNATIVE_WITH_TB_DEBUG_INFO */
    {
        /*
         * No debug info, just disassemble the x86 code and then the native code.
         *
         * First the guest code:
         */
        for (unsigned i = 0; i < pTb->cRanges; i++)
        {
            RTGCPHYS GCPhysPc = pTb->aRanges[i].offPhysPage
                              + (pTb->aRanges[i].idxPhysPage == 0
                                 ? pTb->GCPhysPc & ~(RTGCPHYS)GUEST_PAGE_OFFSET_MASK
                                 : pTb->aGCPhysPages[pTb->aRanges[i].idxPhysPage - 1]);
            pHlp->pfnPrintf(pHlp, "  Range #%u: GCPhysPc=%RGp LB %#x [idxPg=%d]\n",
                            i, GCPhysPc, pTb->aRanges[i].cbOpcodes, pTb->aRanges[i].idxPhysPage);
            unsigned       off       = pTb->aRanges[i].offOpcodes;
            /** @todo this ain't working when crossing pages!   */
            unsigned const cbOpcodes = pTb->aRanges[i].cbOpcodes + off;
            while (off < cbOpcodes)
            {
                uint32_t cbInstr = 1;
                int rc = DISInstrWithPrefetchedBytes(GCPhysPc, enmGstCpuMode, DISOPTYPE_ALL,
                                                     &pTb->pabOpcodes[off], cbOpcodes - off,
                                                     iemNativeDisasReadBytesDummy, NULL, &Dis, &cbInstr);
                if (RT_SUCCESS(rc))
                {
                    DISFormatYasmEx(&Dis, szDisBuf, sizeof(szDisBuf),
                                    DIS_FMT_FLAGS_BYTES_WIDTH_MAKE(10) | DIS_FMT_FLAGS_BYTES_LEFT
                                    | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_C_HEX,
                                    NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
                    pHlp->pfnPrintf(pHlp, "    %RGp: %s\n", GCPhysPc, szDisBuf);
                    GCPhysPc += cbInstr;
                    off      += cbInstr;
                }
                else
                {
                    pHlp->pfnPrintf(pHlp, "    %RGp: %.*Rhxs - disassembly failure %Rrc\n",
                                    GCPhysPc, cbOpcodes - off, &pTb->pabOpcodes[off], rc);
                    break;
                }
            }
        }

        /*
         * Then the native code:
         */
        pHlp->pfnPrintf(pHlp, "  Native code %p L %#x\n", paNative, cNative);
        while (offNative < cNative)
        {
            PCIEMNATIVEINSTR const pNativeCur = &paNative[offNative];
# ifndef VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER
            uint32_t               cbInstr    = sizeof(paNative[0]);
            int const              rc         = DISInstr(pNativeCur, enmHstCpuMode, &Dis, &cbInstr);
            if (RT_SUCCESS(rc))
            {
#  if defined(RT_ARCH_AMD64)
                if (Dis.pCurInstr->uOpcode == OP_NOP && cbInstr == 7) /* iemNativeEmitMarker */
                {
                    uint32_t const uInfo = *(uint32_t const *)&Dis.Instr.ab[3];
                    if (RT_HIWORD(uInfo) < kIemThreadedFunc_End)
                        pHlp->pfnPrintf(pHlp, "\n    %p: nop ; marker: call #%u to %s (%u args) - %s\n",
                                        pNativeCur, uInfo & 0x7fff, g_apszIemThreadedFunctions[RT_HIWORD(uInfo)],
                                        g_acIemThreadedFunctionUsedArgs[RT_HIWORD(uInfo)],
                                        uInfo & 0x8000 ? "recompiled" : "todo");
                    else if ((uInfo & ~RT_BIT_32(31)) < RT_ELEMENTS(a_apszMarkers))
                        pHlp->pfnPrintf(pHlp, "    %p: nop ; marker: %s\n", pNativeCur, a_apszMarkers[uInfo & ~RT_BIT_32(31)]);
                    else
                        pHlp->pfnPrintf(pHlp, "    %p: nop ; unknown marker: %#x (%d)\n", pNativeCur, uInfo, uInfo);
                }
                else
#  endif
                {
#  ifdef RT_ARCH_AMD64
                    DISFormatYasmEx(&Dis, szDisBuf, sizeof(szDisBuf),
                                    DIS_FMT_FLAGS_BYTES_WIDTH_MAKE(10) | DIS_FMT_FLAGS_BYTES_LEFT
                                    | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_C_HEX,
                                    NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
#  elif defined(RT_ARCH_ARM64)
                    DISFormatArmV8Ex(&Dis, szDisBuf, sizeof(szDisBuf),
                                     DIS_FMT_FLAGS_BYTES_LEFT | DIS_FMT_FLAGS_RELATIVE_BRANCH | DIS_FMT_FLAGS_C_HEX,
                                     NULL /*pfnGetSymbol*/, NULL /*pvUser*/);
#  else
#   error "Port me"
#  endif
                    pHlp->pfnPrintf(pHlp, "    %p: %s\n", pNativeCur, szDisBuf);
                }
            }
            else
            {
#  if defined(RT_ARCH_AMD64)
                pHlp->pfnPrintf(pHlp, "    %p:  %.*Rhxs - disassembly failure %Rrc\n",
                                pNativeCur, RT_MIN(cNative - offNative, 16), pNativeCur, rc);
#  else
                pHlp->pfnPrintf(pHlp, "    %p:  %#010RX32 - disassembly failure %Rrc\n", pNativeCur, *pNativeCur, rc);
#  endif
                cbInstr = sizeof(paNative[0]);
            }
            offNative += cbInstr / sizeof(paNative[0]);

# else  /* VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER */
            cs_insn *pInstr;
            size_t   cInstrs = cs_disasm(hDisasm, (const uint8_t *)pNativeCur, (cNative - offNative) * sizeof(*pNativeCur),
                                         (uintptr_t)pNativeCur, 1, &pInstr);
            if (cInstrs > 0)
            {
                Assert(cInstrs == 1);
#  if defined(RT_ARCH_AMD64)
                pHlp->pfnPrintf(pHlp, "    %p: %.*Rhxs %-7s %s\n",
                                pNativeCur, pInstr->size, pNativeCur, pInstr->mnemonic, pInstr->op_str);
#  else
                pHlp->pfnPrintf(pHlp, "    %p: %#010RX32 %-7s %s\n",
                                pNativeCur, *pNativeCur, pInstr->mnemonic, pInstr->op_str);
#  endif
                offNative += pInstr->size / sizeof(*pNativeCur);
                cs_free(pInstr, cInstrs);
            }
            else
            {
#  if defined(RT_ARCH_AMD64)
                pHlp->pfnPrintf(pHlp, "    %p:  %.*Rhxs - disassembly failure %d\n",
                                pNativeCur, RT_MIN(cNative - offNative, 16), pNativeCur, cs_errno(hDisasm)));
#  else
                pHlp->pfnPrintf(pHlp, "    %p:  %#010RX32 - disassembly failure %d\n", pNativeCur, *pNativeCur, cs_errno(hDisasm));
#  endif
                offNative++;
            }
# endif /* VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER */
        }
    }

#ifdef VBOX_WITH_IEM_USING_CAPSTONE_DISASSEMBLER
    /* Cleanup. */
    cs_close(&hDisasm);
#endif
}


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
DECLHIDDEN(PIEMTB) iemNativeRecompile(PVMCPUCC pVCpu, PIEMTB pTb) RT_NOEXCEPT
{
    STAM_REL_PROFILE_START(&pVCpu->iem.s.StatNativeRecompilation, a);

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

#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
    /*
     * First do liveness analysis.  This is done backwards.
     */
    {
        uint32_t idxCall = pTb->Thrd.cCalls;
        if (idxCall <= pReNative->cLivenessEntriesAlloc)
        { /* likely */ }
        else
        {
            uint32_t cAlloc = RT_MAX(pReNative->cLivenessEntriesAlloc, _4K);
            while (idxCall > cAlloc)
                cAlloc *= 2;
            void *pvNew = RTMemRealloc(pReNative->paLivenessEntries, sizeof(pReNative->paLivenessEntries[0]) * cAlloc);
            AssertReturn(pvNew, pTb);
            pReNative->paLivenessEntries     = (PIEMLIVENESSENTRY)pvNew;
            pReNative->cLivenessEntriesAlloc = cAlloc;
        }
        AssertReturn(idxCall > 0, pTb);
        PIEMLIVENESSENTRY const paLivenessEntries = pReNative->paLivenessEntries;

        /* The initial (final) entry. */
        idxCall--;
        IEM_LIVENESS_RAW_INIT_AS_UNUSED(&paLivenessEntries[idxCall]);

        /* Loop backwards thru the calls and fill in the other entries. */
        PCIEMTHRDEDCALLENTRY pCallEntry = &pTb->Thrd.paCalls[idxCall];
        while (idxCall > 0)
        {
            PFNIEMNATIVELIVENESSFUNC const pfnLiveness = g_apfnIemNativeLivenessFunctions[pCallEntry->enmFunction];
            if (pfnLiveness)
                pfnLiveness(pCallEntry, &paLivenessEntries[idxCall], &paLivenessEntries[idxCall - 1]);
            else
                IEM_LIVENESS_RAW_INIT_WITH_XCPT_OR_CALL(&paLivenessEntries[idxCall - 1], &paLivenessEntries[idxCall]);
            pCallEntry--;
            idxCall--;
        }

# ifdef VBOX_WITH_STATISTICS
        /* Check if there are any EFLAGS optimization to be had here.  This requires someone settings them
           to 'clobbered' rather that 'input'.  */
        /** @todo */
# endif
    }
#endif

    /*
     * Recompiling and emitting code is done using try/throw/catch or setjmp/longjmp
     * for aborting if an error happens.
     */
    uint32_t        cCallsLeft = pTb->Thrd.cCalls;
#ifdef LOG_ENABLED
    uint32_t const  cCallsOrg  = cCallsLeft;
#endif
    uint32_t        off        = 0;
    int             rc         = VINF_SUCCESS;
    IEMNATIVE_TRY_SETJMP(pReNative, rc)
    {
        /*
         * Emit prolog code (fixed).
         */
        off = iemNativeEmitProlog(pReNative, off);

        /*
         * Convert the calls to native code.
         */
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
        int32_t              iGstInstr        = -1;
#endif
#ifndef VBOX_WITHOUT_RELEASE_STATISTICS
        uint32_t             cThreadedCalls   = 0;
        uint32_t             cRecompiledCalls = 0;
#endif
#if defined(IEMNATIVE_WITH_LIVENESS_ANALYSIS) || defined(VBOX_STRICT) || defined(LOG_ENABLED)
        uint32_t             idxCurCall       = 0;
#endif
        PCIEMTHRDEDCALLENTRY pCallEntry       = pTb->Thrd.paCalls;
        pReNative->fExec                      = pTb->fFlags & IEMTB_F_IEM_F_MASK;
        while (cCallsLeft-- > 0)
        {
            PFNIEMNATIVERECOMPFUNC const pfnRecom = g_apfnIemNativeRecompileFunctions[pCallEntry->enmFunction];
#ifdef IEMNATIVE_WITH_LIVENESS_ANALYSIS
            pReNative->idxCurCall                 = idxCurCall;
#endif

            /*
             * Debug info, assembly markup and statistics.
             */
#if defined(IEMNATIVE_WITH_TB_DEBUG_INFO) || !defined(IEMNATIVE_WITH_BLTIN_CHECKMODE)
            if (pCallEntry->enmFunction == kIemThreadedFunc_BltIn_CheckMode)
                pReNative->fExec = pCallEntry->auParams[0] & IEMTB_F_IEM_F_MASK;
#endif
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
            iemNativeDbgInfoAddNativeOffset(pReNative, off);
            if (iGstInstr < (int32_t)pCallEntry->idxInstr)
            {
                if (iGstInstr < (int32_t)pTb->cInstructions)
                    iemNativeDbgInfoAddGuestInstruction(pReNative, pReNative->fExec);
                else
                    Assert(iGstInstr == pTb->cInstructions);
                iGstInstr = pCallEntry->idxInstr;
            }
            iemNativeDbgInfoAddThreadedCall(pReNative, (IEMTHREADEDFUNCS)pCallEntry->enmFunction, pfnRecom != NULL);
#endif
#if defined(VBOX_STRICT)
            off = iemNativeEmitMarker(pReNative, off,
                                      RT_MAKE_U32(idxCurCall | (pfnRecom ? 0x8000 : 0), pCallEntry->enmFunction));
#endif
#if defined(VBOX_STRICT)
            iemNativeRegAssertSanity(pReNative);
#endif
#ifdef VBOX_WITH_STATISTICS
            off = iemNativeEmitThreadCallStats(pReNative, off, pCallEntry);
#endif

            /*
             * Actual work.
             */
            Log2(("%u[%u]: %s%s\n", idxCurCall, pCallEntry->idxInstr, g_apszIemThreadedFunctions[pCallEntry->enmFunction],
                  pfnRecom ? "(recompiled)" : "(todo)"));
            if (pfnRecom) /** @todo stats on this.   */
            {
                off = pfnRecom(pReNative, off, pCallEntry);
                STAM_REL_STATS({cRecompiledCalls++;});
            }
            else
            {
                off = iemNativeEmitThreadedCall(pReNative, off, pCallEntry);
                STAM_REL_STATS({cThreadedCalls++;});
            }
            Assert(off <= pReNative->cInstrBufAlloc);
            Assert(pReNative->cCondDepth == 0);

#if defined(LOG_ENABLED) && defined(IEMNATIVE_WITH_LIVENESS_ANALYSIS)
            if (LogIs2Enabled())
            {
                PCIEMLIVENESSENTRY pLivenessEntry = &pReNative->paLivenessEntries[idxCurCall];
# ifndef IEMLIVENESS_EXTENDED_LAYOUT
                static const char s_achState[] = "CUXI";
# else
                static const char s_achState[] = "UxRrWwMmCcQqKkNn";
# endif

                char szGpr[17];
                for (unsigned i = 0; i < 16; i++)
                    szGpr[i] = s_achState[iemNativeLivenessGetStateByGstRegEx(pLivenessEntry, i + kIemNativeGstReg_GprFirst)];
                szGpr[16] = '\0';

                char szSegBase[X86_SREG_COUNT + 1];
                char szSegLimit[X86_SREG_COUNT + 1];
                char szSegAttrib[X86_SREG_COUNT + 1];
                char szSegSel[X86_SREG_COUNT + 1];
                for (unsigned i = 0; i < X86_SREG_COUNT; i++)
                {
                    szSegBase[i]   = s_achState[iemNativeLivenessGetStateByGstRegEx(pLivenessEntry, i + kIemNativeGstReg_SegBaseFirst)];
                    szSegAttrib[i] = s_achState[iemNativeLivenessGetStateByGstRegEx(pLivenessEntry, i + kIemNativeGstReg_SegAttribFirst)];
                    szSegLimit[i]  = s_achState[iemNativeLivenessGetStateByGstRegEx(pLivenessEntry, i + kIemNativeGstReg_SegLimitFirst)];
                    szSegSel[i]    = s_achState[iemNativeLivenessGetStateByGstRegEx(pLivenessEntry, i + kIemNativeGstReg_SegSelFirst)];
                }
                szSegBase[X86_SREG_COUNT] = szSegAttrib[X86_SREG_COUNT] = szSegLimit[X86_SREG_COUNT]
                    = szSegSel[X86_SREG_COUNT] = '\0';

                char szEFlags[8];
                for (unsigned i = 0; i < 7; i++)
                    szEFlags[i] = s_achState[iemNativeLivenessGetStateByGstRegEx(pLivenessEntry, i + kIemNativeGstReg_EFlags)];
                szEFlags[7] = '\0';

                Log2(("liveness: grp=%s segbase=%s segattr=%s seglim=%s segsel=%s efl=%s\n",
                      szGpr, szSegBase, szSegAttrib, szSegLimit, szSegSel, szEFlags));
            }
#endif

            /*
             * Advance.
             */
            pCallEntry++;
#if defined(IEMNATIVE_WITH_LIVENESS_ANALYSIS) || defined(VBOX_STRICT) || defined(LOG_ENABLED)
            idxCurCall++;
#endif
        }

        STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatNativeCallsRecompiled, cRecompiledCalls);
        STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatNativeCallsThreaded,   cThreadedCalls);
        if (!cThreadedCalls)
            STAM_REL_COUNTER_INC(&pVCpu->iem.s.StatNativeFullyRecompiledTbs);

        /*
         * Emit the epilog code.
         */
        uint32_t idxReturnLabel;
        off = iemNativeEmitEpilog(pReNative, off, &idxReturnLabel);

        /*
         * Generate special jump labels.
         */
        if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_ReturnBreak))
            off = iemNativeEmitReturnBreak(pReNative, off, idxReturnLabel);
        if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_ReturnWithFlags))
            off = iemNativeEmitReturnWithFlags(pReNative, off, idxReturnLabel);

        /*
         * Generate simple TB tail labels that just calls a help with a pVCpu
         * arg and either return or longjmps/throws a non-zero status.
         *
         * The array entries must be ordered by enmLabel value so we can index
         * using fTailLabels bit numbers.
         */
        typedef IEM_DECL_NATIVE_HLP_PTR(int, PFNIEMNATIVESIMPLETAILLABELCALL,(PVMCPUCC pVCpu));
        static struct
        {
            IEMNATIVELABELTYPE              enmLabel;
            PFNIEMNATIVESIMPLETAILLABELCALL pfnCallback;
        } const g_aSimpleTailLabels[] =
        {
            {   kIemNativeLabelType_Invalid,                NULL },
            {   kIemNativeLabelType_RaiseDe,                iemNativeHlpExecRaiseDe },
            {   kIemNativeLabelType_RaiseUd,                iemNativeHlpExecRaiseUd },
            {   kIemNativeLabelType_RaiseSseRelated,        iemNativeHlpExecRaiseSseRelated },
            {   kIemNativeLabelType_RaiseAvxRelated,        iemNativeHlpExecRaiseAvxRelated },
            {   kIemNativeLabelType_RaiseSseAvxFpRelated,   iemNativeHlpExecRaiseSseAvxFpRelated },
            {   kIemNativeLabelType_RaiseNm,                iemNativeHlpExecRaiseNm },
            {   kIemNativeLabelType_RaiseGp0,               iemNativeHlpExecRaiseGp0 },
            {   kIemNativeLabelType_RaiseMf,                iemNativeHlpExecRaiseMf },
            {   kIemNativeLabelType_RaiseXf,                iemNativeHlpExecRaiseXf },
            {   kIemNativeLabelType_ObsoleteTb,             iemNativeHlpObsoleteTb },
            {   kIemNativeLabelType_NeedCsLimChecking,      iemNativeHlpNeedCsLimChecking },
            {   kIemNativeLabelType_CheckBranchMiss,        iemNativeHlpCheckBranchMiss },
        };
        AssertCompile(RT_ELEMENTS(g_aSimpleTailLabels) == (unsigned)kIemNativeLabelType_LastSimple + 1U);
        AssertCompile(kIemNativeLabelType_Invalid == 0);
        uint64_t fTailLabels = pReNative->bmLabelTypes & (RT_BIT_64(kIemNativeLabelType_LastSimple + 1U) - 2U);
        if (fTailLabels)
        {
            do
            {
                IEMNATIVELABELTYPE const enmLabel = (IEMNATIVELABELTYPE)(ASMBitFirstSetU64(fTailLabels) - 1U);
                fTailLabels &= ~RT_BIT_64(enmLabel);
                Assert(g_aSimpleTailLabels[enmLabel].enmLabel == enmLabel);

                uint32_t const idxLabel = iemNativeLabelFind(pReNative, enmLabel);
                Assert(idxLabel != UINT32_MAX);
                if (idxLabel != UINT32_MAX)
                {
                    iemNativeLabelDefine(pReNative, idxLabel, off);

                    /* int pfnCallback(PVMCPUCC pVCpu) */
                    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
                    off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)g_aSimpleTailLabels[enmLabel].pfnCallback);

                    /* jump back to the return sequence. */
                    off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
                }

            } while (fTailLabels);
        }
    }
    IEMNATIVE_CATCH_LONGJMP_BEGIN(pReNative, rc);
    {
        Log(("iemNativeRecompile: Caught %Rrc while recompiling!\n", rc));
        return pTb;
    }
    IEMNATIVE_CATCH_LONGJMP_END(pReNative);
    Assert(off <= pReNative->cInstrBufAlloc);

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
        AssertMsg(paLabels[paFixups[i].idxLabel].off < off,
                  ("idxLabel=%d enmType=%d off=%#x (max %#x)\n", paFixups[i].idxLabel,
                   paLabels[paFixups[i].idxLabel].enmType, paLabels[paFixups[i].idxLabel].off, off));
        RTPTRUNION const Ptr = { &paFinalInstrBuf[paFixups[i].off] };
        switch (paFixups[i].enmType)
        {
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
            case kIemNativeFixupType_Rel32:
                Assert(paFixups[i].off + 4 <= off);
                *Ptr.pi32 = paLabels[paFixups[i].idxLabel].off - paFixups[i].off + paFixups[i].offAddend;
                continue;

#elif defined(RT_ARCH_ARM64)
            case kIemNativeFixupType_RelImm26At0:
            {
                Assert(paFixups[i].off < off);
                int32_t const offDisp = paLabels[paFixups[i].idxLabel].off - paFixups[i].off + paFixups[i].offAddend;
                Assert(offDisp >= -262144 && offDisp < 262144);
                *Ptr.pu32 = (*Ptr.pu32 & UINT32_C(0xfc000000)) | ((uint32_t)offDisp & UINT32_C(0x03ffffff));
                continue;
            }

            case kIemNativeFixupType_RelImm19At5:
            {
                Assert(paFixups[i].off < off);
                int32_t const offDisp = paLabels[paFixups[i].idxLabel].off - paFixups[i].off + paFixups[i].offAddend;
                Assert(offDisp >= -262144 && offDisp < 262144);
                *Ptr.pu32 = (*Ptr.pu32 & UINT32_C(0xff00001f)) | (((uint32_t)offDisp & UINT32_C(0x0007ffff)) << 5);
                continue;
            }

            case kIemNativeFixupType_RelImm14At5:
            {
                Assert(paFixups[i].off < off);
                int32_t const offDisp = paLabels[paFixups[i].idxLabel].off - paFixups[i].off + paFixups[i].offAddend;
                Assert(offDisp >= -8192 && offDisp < 8192);
                *Ptr.pu32 = (*Ptr.pu32 & UINT32_C(0xfff8001f)) | (((uint32_t)offDisp & UINT32_C(0x00003fff)) << 5);
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
    STAM_REL_PROFILE_ADD_PERIOD(&pVCpu->iem.s.StatTbNativeCode, off * sizeof(IEMNATIVEINSTR));

    /*
     * Convert the translation block.
     */
    RTMemFree(pTb->Thrd.paCalls);
    pTb->Native.paInstructions  = paFinalInstrBuf;
    pTb->Native.cInstructions   = off;
    pTb->fFlags                 = (pTb->fFlags & ~IEMTB_F_TYPE_MASK) | IEMTB_F_TYPE_NATIVE;
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    pTb->pDbgInfo               = (PIEMTBDBG)RTMemDup(pReNative->pDbgInfo, /* non-fatal, so not return check. */
                                                      RT_UOFFSETOF_DYN(IEMTBDBG, aEntries[pReNative->pDbgInfo->cEntries]));
#endif

    Assert(pTbAllocator->cThreadedTbs > 0);
    pTbAllocator->cThreadedTbs -= 1;
    pTbAllocator->cNativeTbs   += 1;
    Assert(pTbAllocator->cNativeTbs <= pTbAllocator->cTotalTbs);

#ifdef LOG_ENABLED
    /*
     * Disassemble to the log if enabled.
     */
    if (LogIs3Enabled())
    {
        Log3(("----------------------------------------- %d calls ---------------------------------------\n", cCallsOrg));
        iemNativeDisassembleTb(pTb, DBGFR3InfoLogHlp());
# if defined(DEBUG_bird) || defined(DEBUG_aeichner)
        RTLogFlush(NULL);
# endif
    }
#endif
    /*iemNativeDisassembleTb(pTb, DBGFR3InfoLogRelHlp());*/

    STAM_REL_PROFILE_STOP(&pVCpu->iem.s.StatNativeRecompilation, a);
    return pTb;
}

