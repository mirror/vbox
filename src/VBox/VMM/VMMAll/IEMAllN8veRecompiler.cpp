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
#ifdef VBOX_STRICT
static uint32_t iemNativeEmitGuestRegValueCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                uint8_t idxReg, IEMNATIVEGSTREG enmGstReg);
static void iemNativeRegAssertSanity(PIEMRECOMPILERSTATE pReNative);
#endif
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
static void iemNativeDbgInfoAddNativeOffset(PIEMRECOMPILERSTATE pReNative, uint32_t off);
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


    for (unsigned iIteration = 0;; iIteration++)
    {
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

        /*
         * Try prune native TBs once.
         */
        if (iIteration == 0)
            iemTbAllocatorFreeupNativeSpace(pVCpu, cbReq / sizeof(IEMNATIVEINSTR));
        else
        {
            /** @todo stats...   */
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

    pReNative->Core.bmHstRegs              = IEMNATIVE_REG_FIXED_MASK
#if IEMNATIVE_HST_GREG_COUNT < 32
                                           | ~(RT_BIT(IEMNATIVE_HST_GREG_COUNT) - 1U)
#endif
                                           ;
    pReNative->Core.bmHstRegsWithGstShadow = 0;
    pReNative->Core.bmGstRegShadows        = 0;
    pReNative->Core.bmVars                 = 0;
    pReNative->Core.bmStack                = 0;
    AssertCompile(sizeof(pReNative->Core.bmStack) * 8 == IEMNATIVE_FRAME_VAR_SLOTS); /* Must set reserved slots to 1 otherwise. */
    pReNative->Core.u64ArgVars             = UINT64_MAX;

    AssertCompile(RT_ELEMENTS(pReNative->aidxUniqueLabels) == 9);
    pReNative->aidxUniqueLabels[0]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[1]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[2]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[3]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[4]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[5]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[6]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[7]         = UINT32_MAX;
    pReNative->aidxUniqueLabels[8]         = UINT32_MAX;

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
    AssertReturn(cNew > 0, NULL);
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
static void iemNativeDbgInfoAddNativeOffset(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    PIEMTBDBG pDbgInfo = pReNative->pDbgInfo;

    /*
     * Search backwards to see if we've got a similar record already.
     */
    uint32_t idx     = pDbgInfo->cEntries;
    uint32_t idxStop = idx > 8 ? idx - 8 : 0;
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
static void iemNativeDbgInfoAddGuestRegShadowing(PIEMRECOMPILERSTATE pReNative, IEMNATIVEGSTREG enmGstReg,
                                                 uint8_t idxHstReg = UINT8_MAX, uint8_t idxHstRegPrev = UINT8_MAX)
{
    PIEMTBDBGENTRY const pEntry = iemNativeDbgInfoAddNewEntry(pReNative, pReNative->pDbgInfo);
    pEntry->GuestRegShadowing.uType         = kIemTbDbgEntryType_GuestRegShadowing;
    pEntry->GuestRegShadowing.uUnused       = 0;
    pEntry->GuestRegShadowing.idxGstReg     = enmGstReg;
    pEntry->GuestRegShadowing.idxHstReg     = idxHstReg;
    pEntry->GuestRegShadowing.idxHstRegPrev = idxHstRegPrev;
}

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
    /* [kIemNativeGstReg_LivenessPadding17] = */        { UINT32_MAX / 4, 0,                        "pad17", },
    /* [kIemNativeGstReg_LivenessPadding18] = */        { UINT32_MAX / 4, 0,                        "pad18", },
    /* [kIemNativeGstReg_LivenessPadding19] = */        { UINT32_MAX / 4, 0,                        "pad19", },
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


DECL_FORCE_INLINE(uint8_t) iemNativeRegMarkAllocated(PIEMRECOMPILERSTATE pReNative, unsigned idxReg,
                                                     IEMNATIVEWHAT enmWhat, uint8_t idxVar = UINT8_MAX) RT_NOEXCEPT
{
    pReNative->Core.bmHstRegs |= RT_BIT_32(idxReg);

    pReNative->Core.aHstRegs[idxReg].enmWhat        = enmWhat;
    pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
    pReNative->Core.aHstRegs[idxReg].idxVar         = idxVar;
    return (uint8_t)idxReg;
}


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
    Assert(!(fRegMask & ~IEMNATIVE_HST_GREG_MASK));
    Assert(!(fRegMask & IEMNATIVE_REG_FIXED_MASK));

    /*
     * Try a freed register that's shadowing a guest register
     */
    uint32_t fRegs = ~pReNative->Core.bmHstRegs & fRegMask;
    if (fRegs)
    {
        unsigned const idxReg = (fPreferVolatile
                                 ? ASMBitFirstSetU32(fRegs)
                                 : ASMBitLastSetU32(  fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK
                                                    ? fRegs & ~IEMNATIVE_CALL_VOLATILE_GREG_MASK: fRegs))
                              - 1;

        Assert(pReNative->Core.aHstRegs[idxReg].fGstRegShadows != 0);
        Assert(   (pReNative->Core.aHstRegs[idxReg].fGstRegShadows & pReNative->Core.bmGstRegShadows)
               == pReNative->Core.aHstRegs[idxReg].fGstRegShadows);
        Assert(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxReg));

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
    for (uint32_t iLoop = 0; iLoop < 2; iLoop++)
    {
        uint32_t fVars = pReNative->Core.bmVars;
        while (fVars)
        {
            uint32_t const idxVar = ASMBitFirstSetU32(fVars) - 1;
            uint8_t const  idxReg = pReNative->Core.aVars[idxVar].idxReg;
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

                if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                {
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
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
    Assert(pReNative->Core.aVars[idxVar].idxReg == idxRegOld);
    RT_NOREF(pszCaller);

    iemNativeRegClearGstRegShadowing(pReNative, idxRegNew, off);

    uint64_t fGstRegShadows = pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows;
    Log12(("%s: moving idxVar=%d from %s to %s (fGstRegShadows=%RX64)\n",
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

    pReNative->Core.aVars[idxVar].idxReg = (uint8_t)idxRegNew;
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
static uint32_t iemNativeRegMoveOrSpillStackVar(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVar,
                                                uint32_t fForbiddenRegs = IEMNATIVE_CALL_VOLATILE_GREG_MASK)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack);
    Assert(!pReNative->Core.aVars[idxVar].fRegAcquired);

    uint8_t const idxRegOld = pReNative->Core.aVars[idxVar].idxReg;
    Assert(idxRegOld < RT_ELEMENTS(pReNative->Core.aHstRegs));
    Assert(pReNative->Core.bmHstRegs & RT_BIT_32(idxRegOld));
    Assert(pReNative->Core.aHstRegs[idxRegOld].enmWhat == kIemNativeWhat_Var);
    Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows)
           == pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows);
    Assert(pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(   RT_BOOL(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxRegOld))
           == RT_BOOL(pReNative->Core.aHstRegs[idxRegOld].fGstRegShadows));


    /** @todo Add statistics on this.*/
    /** @todo Implement basic variable liveness analysis (python) so variables
     * can be freed immediately once no longer used.  This has the potential to
     * be trashing registers and stack for dead variables. */

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
    Log12(("iemNativeRegMoveOrSpillStackVar: spilling idxVar=%d/idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
           idxVar, idxRegOld, idxStackSlot, iemNativeStackCalcBpDisp(idxStackSlot), off));
    off = iemNativeEmitStoreGprByBp(pReNative, off, iemNativeStackCalcBpDisp(idxStackSlot), idxRegOld);

    pReNative->Core.aVars[idxVar].idxReg    = UINT8_MAX;
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
    }
    else
    {
        idxReg = iemNativeRegAllocFindFree(pReNative, poff, fPreferVolatile);
        AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_TMP));
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
    }
    else
    {
        idxReg = iemNativeRegAllocFindFree(pReNative, poff, fPreferVolatile, fRegMask);
        AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_TMP));
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
 * Marks host register @a idxHstReg as containing a shadow copy of guest
 * register @a enmGstReg.
 *
 * ASSUMES that caller has made sure @a enmGstReg is not associated with any
 * host register before calling.
 */
DECL_FORCE_INLINE(void)
iemNativeRegMarkAsGstRegShadow(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg, uint32_t off)
{
    Assert(!(pReNative->Core.bmGstRegShadows & RT_BIT_64(enmGstReg)));
    Assert(!pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows);
    Assert((unsigned)enmGstReg < (unsigned)kIemNativeGstReg_End);

    pReNative->Core.aidxGstRegShadows[enmGstReg]       = idxHstReg;
    pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = RT_BIT_64(enmGstReg); /** @todo why? not OR? */
    pReNative->Core.bmGstRegShadows                   |= RT_BIT_64(enmGstReg);
    pReNative->Core.bmHstRegsWithGstShadow            |= RT_BIT_32(idxHstReg);
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    iemNativeDbgInfoAddNativeOffset(pReNative, off);
    iemNativeDbgInfoAddGuestRegShadowing(pReNative, enmGstReg, idxHstReg);
#else
    RT_NOREF(off);
#endif
}


/**
 * Clear any guest register shadow claims from @a idxHstReg.
 *
 * The register does not need to be shadowing any guest registers.
 */
DECL_FORCE_INLINE(void)
iemNativeRegClearGstRegShadowing(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, uint32_t off)
{
    Assert(      (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows)
              == pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows
           && pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(   RT_BOOL(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxHstReg))
           == RT_BOOL(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows));

#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    uint64_t fGstRegs = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
    if (fGstRegs)
    {
        Assert(fGstRegs < RT_BIT_64(kIemNativeGstReg_End));
        iemNativeDbgInfoAddNativeOffset(pReNative, off);
        while (fGstRegs)
        {
            unsigned const iGstReg = ASMBitFirstSetU64(fGstRegs) - 1;
            fGstRegs &= ~RT_BIT_64(iGstReg);
            iemNativeDbgInfoAddGuestRegShadowing(pReNative, (IEMNATIVEGSTREG)iGstReg, UINT8_MAX, idxHstReg);
        }
    }
#else
    RT_NOREF(off);
#endif

    pReNative->Core.bmHstRegsWithGstShadow            &= ~RT_BIT_32(idxHstReg);
    pReNative->Core.bmGstRegShadows                   &= ~pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
    pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = 0;
}


/**
 * Clear guest register shadow claim regarding @a enmGstReg from @a idxHstReg
 * and global overview flags.
 */
DECL_FORCE_INLINE(void)
iemNativeRegClearGstRegShadowingOne(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstReg, IEMNATIVEGSTREG enmGstReg, uint32_t off)
{
    Assert(pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(      (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows)
              == pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows
           && pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(pReNative->Core.bmGstRegShadows                    & RT_BIT_64(enmGstReg));
    Assert(pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & RT_BIT_64(enmGstReg));
    Assert(pReNative->Core.bmHstRegsWithGstShadow             & RT_BIT_32(idxHstReg));

#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    iemNativeDbgInfoAddNativeOffset(pReNative, off);
    iemNativeDbgInfoAddGuestRegShadowing(pReNative, enmGstReg, UINT8_MAX, idxHstReg);
#else
    RT_NOREF(off);
#endif

    uint64_t const fGstRegShadowsNew = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows & ~RT_BIT_64(enmGstReg);
    pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows = fGstRegShadowsNew;
    if (!fGstRegShadowsNew)
        pReNative->Core.bmHstRegsWithGstShadow        &= ~RT_BIT_32(idxHstReg);
    pReNative->Core.bmGstRegShadows                   &= ~RT_BIT_64(enmGstReg);
}


/**
 * Clear any guest register shadow claim for @a enmGstReg.
 */
DECL_FORCE_INLINE(void)
iemNativeRegClearGstRegShadowingByGstReg(PIEMRECOMPILERSTATE pReNative, IEMNATIVEGSTREG enmGstReg, uint32_t off)
{
    Assert(pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    if (pReNative->Core.bmGstRegShadows & RT_BIT_64(enmGstReg))
    {
        Assert(pReNative->Core.aidxGstRegShadows[enmGstReg] < RT_ELEMENTS(pReNative->Core.aHstRegs));
        iemNativeRegClearGstRegShadowingOne(pReNative, pReNative->Core.aidxGstRegShadows[enmGstReg], enmGstReg, off);
    }
}


/**
 * Clear any guest register shadow claim for @a enmGstReg and mark @a idxHstRegNew
 * as the new shadow of it.
 */
DECL_FORCE_INLINE(void)
iemNativeRegClearAndMarkAsGstRegShadow(PIEMRECOMPILERSTATE pReNative, uint8_t idxHstRegNew,
                                       IEMNATIVEGSTREG enmGstReg, uint32_t off)
{
    Assert(pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    if (pReNative->Core.bmGstRegShadows & RT_BIT_64(enmGstReg))
    {
        Assert(pReNative->Core.aidxGstRegShadows[enmGstReg] < RT_ELEMENTS(pReNative->Core.aHstRegs));
        if (pReNative->Core.aidxGstRegShadows[enmGstReg] == idxHstRegNew)
            return;
        iemNativeRegClearGstRegShadowingOne(pReNative, pReNative->Core.aidxGstRegShadows[enmGstReg], enmGstReg, off);
    }
    iemNativeRegMarkAsGstRegShadow(pReNative, idxHstRegNew, enmGstReg, off);
}


/**
 * Transfers the guest register shadow claims of @a enmGstReg from @a idxRegFrom
 * to @a idxRegTo.
 */
DECL_FORCE_INLINE(void)
iemNativeRegTransferGstRegShadowing(PIEMRECOMPILERSTATE pReNative, uint8_t idxRegFrom, uint8_t idxRegTo,
                                    IEMNATIVEGSTREG enmGstReg, uint32_t off)
{
    Assert(pReNative->Core.aHstRegs[idxRegFrom].fGstRegShadows & RT_BIT_64(enmGstReg));
    Assert(pReNative->Core.aidxGstRegShadows[enmGstReg] == idxRegFrom);
    Assert(      (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxRegFrom].fGstRegShadows)
              == pReNative->Core.aHstRegs[idxRegFrom].fGstRegShadows
           && pReNative->Core.bmGstRegShadows < RT_BIT_64(kIemNativeGstReg_End));
    Assert(   (pReNative->Core.bmGstRegShadows & pReNative->Core.aHstRegs[idxRegTo].fGstRegShadows)
           == pReNative->Core.aHstRegs[idxRegTo].fGstRegShadows);
    Assert(   RT_BOOL(pReNative->Core.bmHstRegsWithGstShadow & RT_BIT_32(idxRegFrom))
           == RT_BOOL(pReNative->Core.aHstRegs[idxRegFrom].fGstRegShadows));

    uint64_t const fGstRegShadowsFrom = pReNative->Core.aHstRegs[idxRegFrom].fGstRegShadows & ~RT_BIT_64(enmGstReg);
    pReNative->Core.aHstRegs[idxRegFrom].fGstRegShadows  = fGstRegShadowsFrom;
    if (!fGstRegShadowsFrom)
        pReNative->Core.bmHstRegsWithGstShadow          &= ~RT_BIT_32(idxRegFrom);
    pReNative->Core.bmHstRegsWithGstShadow              |= RT_BIT_32(idxRegTo);
    pReNative->Core.aHstRegs[idxRegTo].fGstRegShadows   |= RT_BIT_64(enmGstReg);
    pReNative->Core.aidxGstRegShadows[enmGstReg]         = idxRegTo;
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    iemNativeDbgInfoAddNativeOffset(pReNative, off);
    iemNativeDbgInfoAddGuestRegShadowing(pReNative, enmGstReg, idxRegTo, idxRegFrom);
#else
    RT_NOREF(off);
#endif
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
 * @sa      iemNativeRegAllocTmpForGuestRegIfAlreadyPresent
 */
DECL_HIDDEN_THROW(uint8_t)
iemNativeRegAllocTmpForGuestReg(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, IEMNATIVEGSTREG enmGstReg,
                                IEMNATIVEGSTREGUSE enmIntendedUse /*= kIemNativeGstRegUse_ReadOnly*/,
                                bool fNoVolatileRegs /*= false*/)
{
    Assert(enmGstReg < kIemNativeGstReg_End && g_aGstShadowInfo[enmGstReg].cb != 0);
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
                uint8_t const idxRegNew = iemNativeRegAllocTmpEx(pReNative, poff, fRegMask,
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


DECL_HIDDEN_THROW(uint8_t) iemNativeRegAllocVar(PIEMRECOMPILERSTATE pReNative, uint32_t *poff, uint8_t idxVar);


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
                        AssertStmt(idxVar < RT_ELEMENTS(pReNative->Core.aVars),
                                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_IPE_5));
                        Assert(pReNative->Core.aVars[idxVar].idxReg == idxReg);
                        Assert(pReNative->Core.bmVars & RT_BIT_32(idxVar));

                        if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Stack)
                            pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
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
    Assert(pReNative->Core.aVars[idxVar].idxReg == idxHstReg);

    pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
    pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
    if (!fFlushShadows)
        Log12(("iemNativeRegFreeVar: %s (gst: %#RX64) idxVar=%d\n",
               g_apszIemNativeHstRegNames[idxHstReg], pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows, idxVar));
    else
    {
        pReNative->Core.bmHstRegsWithGstShadow &= ~RT_BIT_32(idxHstReg);
        uint64_t const fGstRegShadowsOld        = pReNative->Core.aHstRegs[idxHstReg].fGstRegShadows;
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
        Log12(("iemNativeRegFreeVar: %s (gst: %#RX64 -> 0) idxVar=%d\n",
               g_apszIemNativeHstRegNames[idxHstReg], fGstRegShadowsOld, idxVar));
    }
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
                    uint8_t const idxVar = pReNative->Core.aHstRegs[idxReg].idxVar;
                    Assert(idxVar < RT_ELEMENTS(pReNative->Core.aVars));
                    Assert(pReNative->Core.bmVars & RT_BIT_32(idxVar));
                    Assert(pReNative->Core.aVars[idxVar].idxReg == idxReg);
                    if (!(RT_BIT_32(idxVar) & fKeepVars))
                    {
                        Log12(("iemNativeRegMoveAndFreeAndFlushAtCall: idxVar=%d enmKind=%d idxReg=%d\n",
                               idxVar, pReNative->Core.aVars[idxVar].enmKind, pReNative->Core.aVars[idxVar].idxReg));
                        if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Stack)
                            pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
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
            pReNative->Core.bmGstRegShadows &= ~pReNative->Core.aHstRegs[idxReg].fGstRegShadows;
            pReNative->Core.aHstRegs[idxReg].fGstRegShadows = 0;
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


/**
 * Flushes delayed write of a specific guest register.
 *
 * This must be called prior to calling CImpl functions and any helpers that use
 * the guest state (like raising exceptions) and such.
 *
 * This optimization has not yet been implemented.  The first target would be
 * RIP updates, since these are the most common ones.
 */
DECL_HIDDEN_THROW(uint32_t) iemNativeRegFlushPendingSpecificWrite(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                                  IEMNATIVEGSTREGREF enmClass, uint8_t idxReg)
{
    RT_NOREF(pReNative, enmClass, idxReg);
    return off;
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
DECL_HIDDEN_THROW(uint32_t) iemNativeRegFlushPendingWrites(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    RT_NOREF(pReNative, off);
    return off;
}


#ifdef VBOX_STRICT
/**
 * Does internal register allocator sanity checks.
 */
static void iemNativeRegAssertSanity(PIEMRECOMPILERSTATE pReNative)
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
#endif


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
            AssertFailedStmt(IEMNATIVE_DO_LONGJMP(pReNative, VERR_IPE_NOT_REACHED_DEFAULT_CASE));
    }
}


#ifdef VBOX_STRICT
/**
 * Emitting code that checks that the value of @a idxReg is UINT32_MAX or less.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
static uint32_t
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
#endif /* VBOX_STRICT */


#ifdef VBOX_STRICT
/**
 * Emitting code that checks that the content of register @a idxReg is the same
 * as what's in the guest register @a enmGstReg, resulting in a breakpoint
 * instruction if that's not the case.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
static uint32_t
iemNativeEmitGuestRegValueCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxReg, IEMNATIVEGSTREG enmGstReg)
{
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
#endif /* VBOX_STRICT */


#ifdef VBOX_STRICT
/**
 * Emitting code that checks that IEMCPU::fExec matches @a fExec for all
 * important bits.
 *
 * @note May of course trash IEMNATIVE_REG_FIXED_TMP0.
 *       Trashes EFLAGS on AMD64.
 */
static uint32_t
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
 * Emits code to check if the content of @a idxAddrReg is within the limit of
 * idxSegReg, raising a \#GP(0) if it isn't.
 *
 * @returns New code buffer offset; throws VBox status code on error.
 * @param   pReNative       The native recompile state.
 * @param   off             The code buffer offset.
 * @param   idxAddrReg      The host register (32-bit) with the address to
 *                          check.
 * @param   idxSegReg       The segment register (X86_SREG_XXX) to check
 *                          against.
 * @param   idxInstr        The current instruction.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCheckGpr32AgainstSegLimitMaybeRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                    uint8_t idxAddrReg, uint8_t idxSegReg, uint8_t idxInstr)
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

    /** @todo implement expand down/whatnot checking */
    AssertStmt(idxSegReg == X86_SREG_CS, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_EMIT_CASE_NOT_IMPLEMENTED_1));

    uint8_t const iTmpLimReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off,
                                                               (IEMNATIVEGSTREG)(kIemNativeGstReg_SegLimitFirst + idxSegReg),
                                                               kIemNativeGstRegUse_ForUpdate);

    off = iemNativeEmitCmpGpr32WithGpr(pReNative, off, idxAddrReg, iTmpLimReg);
    off = iemNativeEmitJaToNewLabel(pReNative, off, kIemNativeLabelType_RaiseGp0);

    iemNativeRegFreeTmp(pReNative, iTmpLimReg);
    return off;
}


/**
 * Converts IEM_CIMPL_F_XXX flags into a guest register shadow copy flush mask.
 *
 * @returns The flush mask.
 * @param   fCImpl          The IEM_CIMPL_F_XXX flags.
 * @param   fGstShwFlush    The starting flush mask.
 */
DECL_FORCE_INLINE(uint64_t) iemNativeCImplFlagsToGuestShadowFlushMask(uint32_t fCImpl, uint64_t fGstShwFlush)
{
    if (fCImpl & IEM_CIMPL_F_BRANCH_FAR)
        fGstShwFlush |= RT_BIT_64(kIemNativeGstReg_SegSelFirst   + X86_SREG_CS)
                     |  RT_BIT_64(kIemNativeGstReg_SegBaseFirst  + X86_SREG_CS)
                     |  RT_BIT_64(kIemNativeGstReg_SegLimitFirst + X86_SREG_CS);
    if (fCImpl & IEM_CIMPL_F_BRANCH_STACK_FAR)
        fGstShwFlush |= RT_BIT_64(kIemNativeGstReg_GprFirst + X86_GREG_xSP)
                     |  RT_BIT_64(kIemNativeGstReg_SegSelFirst   + X86_SREG_SS)
                     |  RT_BIT_64(kIemNativeGstReg_SegBaseFirst  + X86_SREG_SS)
                     |  RT_BIT_64(kIemNativeGstReg_SegLimitFirst + X86_SREG_SS);
    else if (fCImpl & IEM_CIMPL_F_BRANCH_STACK)
        fGstShwFlush |= RT_BIT_64(kIemNativeGstReg_GprFirst + X86_GREG_xSP);
    if (fCImpl & (IEM_CIMPL_F_RFLAGS | IEM_CIMPL_F_STATUS_FLAGS | IEM_CIMPL_F_INHIBIT_SHADOW))
        fGstShwFlush |= RT_BIT_64(kIemNativeGstReg_EFlags);
    return fGstShwFlush;
}


/**
 * Emits a call to a CImpl function or something similar.
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCImplCall(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr, uint64_t fGstShwFlush, uintptr_t pfnCImpl,
                       uint8_t cbInstr, uint8_t cAddParams, uint64_t uParam0, uint64_t uParam1, uint64_t uParam2)
{
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


/**
 * Emits the code at the CheckBranchMiss label.
 */
static uint32_t iemNativeEmitCheckBranchMiss(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_CheckBranchMiss);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

        /* int iemNativeHlpCheckBranchMiss(PVMCPUCC pVCpu) */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpCheckBranchMiss);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
    }
    return off;
}


/**
 * Emits the code at the NeedCsLimChecking label.
 */
static uint32_t iemNativeEmitNeedCsLimChecking(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_NeedCsLimChecking);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

        /* int iemNativeHlpNeedCsLimChecking(PVMCPUCC pVCpu) */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpNeedCsLimChecking);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
    }
    return off;
}


/**
 * Emits the code at the ObsoleteTb label.
 */
static uint32_t iemNativeEmitObsoleteTb(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_ObsoleteTb);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

        /* int iemNativeHlpObsoleteTb(PVMCPUCC pVCpu) */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpObsoleteTb);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
    }
    return off;
}


/**
 * Emits the code at the RaiseGP0 label.
 */
static uint32_t iemNativeEmitRaiseGp0(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t idxReturnLabel)
{
    uint32_t const idxLabel = iemNativeLabelFind(pReNative, kIemNativeLabelType_RaiseGp0);
    if (idxLabel != UINT32_MAX)
    {
        iemNativeLabelDefine(pReNative, idxLabel, off);

        /* iemNativeHlpExecRaiseGp0(PVMCPUCC pVCpu) */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
        off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)iemNativeHlpExecRaiseGp0);

        /* jump back to the return sequence. */
        off = iemNativeEmitJmpToLabel(pReNative, off, idxReturnLabel);
    }
    return off;
}


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

    /*
     * Successful return, so clear the return register (eax, w0).
     */
    off = iemNativeEmitGprZero(pReNative,off, IEMNATIVE_CALL_RET_GREG);

    /*
     * Define label for common return point.
     */
    uint32_t const idxReturn = iemNativeLabelCreate(pReNative, kIemNativeLabelType_Return, off);
    *pidxReturnLabel = idxReturn;

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
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);

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
*   Emitters for IEM_MC_BEGIN and IEM_MC_END.                                                                                    *
*********************************************************************************************************************************/

#define IEM_MC_BEGIN(a_cArgs, a_cLocals, a_fMcFlags, a_fCImplFlags) \
    { \
        Assert(pReNative->Core.bmVars     == 0); \
        Assert(pReNative->Core.u64ArgVars == UINT64_MAX); \
        Assert(pReNative->Core.bmStack    == 0); \
        pReNative->fMc    = (a_fMcFlags); \
        pReNative->fCImpl = (a_fCImplFlags); \
        pReNative->cArgs  = ((a_cArgs) + iemNativeArgGetHiddenArgCount(pReNative))

/** We have to get to the end in recompilation mode, as otherwise we won't
 * generate code for all the IEM_MC_IF_XXX branches. */
#define IEM_MC_END() \
        iemNativeVarFreeAll(pReNative); \
    } return off



/*********************************************************************************************************************************
*   Emitters for standalone C-implementation deferals (IEM_MC_DEFER_TO_CIMPL_XXXX)                                               *
*********************************************************************************************************************************/

#define IEM_MC_DEFER_TO_CIMPL_0_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl) \
    pReNative->fMc    = 0; \
    pReNative->fCImpl = (a_fFlags); \
    return iemNativeEmitCImplCall0(pReNative, off, pCallEntry->idxInstr, a_fGstShwFlush, (uintptr_t)a_pfnCImpl, a_cbInstr) /** @todo not used ... */


#define IEM_MC_DEFER_TO_CIMPL_1_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0) \
    pReNative->fMc    = 0; \
    pReNative->fCImpl = (a_fFlags); \
    return iemNativeEmitCImplCall1(pReNative, off, pCallEntry->idxInstr, a_fGstShwFlush, (uintptr_t)a_pfnCImpl, a_cbInstr, a0)

DECL_INLINE_THROW(uint32_t) iemNativeEmitCImplCall1(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                    uint8_t idxInstr, uint64_t a_fGstShwFlush,
                                                    uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, a_fGstShwFlush, pfnCImpl, cbInstr, 1, uArg0, 0, 0);
}


#define IEM_MC_DEFER_TO_CIMPL_2_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1) \
    pReNative->fMc    = 0; \
    pReNative->fCImpl = (a_fFlags); \
    return iemNativeEmitCImplCall2(pReNative, off, pCallEntry->idxInstr, a_fGstShwFlush, \
                                   (uintptr_t)a_pfnCImpl, a_cbInstr, a0, a1)

DECL_INLINE_THROW(uint32_t) iemNativeEmitCImplCall2(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                    uint8_t idxInstr, uint64_t a_fGstShwFlush,
                                                    uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0, uint64_t uArg1)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, a_fGstShwFlush, pfnCImpl, cbInstr, 2, uArg0, uArg1, 0);
}


#define IEM_MC_DEFER_TO_CIMPL_3_RET_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2) \
    pReNative->fMc    = 0; \
    pReNative->fCImpl = (a_fFlags); \
    return iemNativeEmitCImplCall3(pReNative, off, pCallEntry->idxInstr, a_fGstShwFlush, \
                                   (uintptr_t)a_pfnCImpl, a_cbInstr, a0, a1, a2)

DECL_INLINE_THROW(uint32_t) iemNativeEmitCImplCall3(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                    uint8_t idxInstr, uint64_t a_fGstShwFlush,
                                                    uintptr_t pfnCImpl, uint8_t cbInstr, uint64_t uArg0, uint64_t uArg1,
                                                    uint64_t uArg2)
{
    return iemNativeEmitCImplCall(pReNative, off, idxInstr, a_fGstShwFlush, pfnCImpl, cbInstr, 3, uArg0, uArg1, uArg2);
}



/*********************************************************************************************************************************
*   Emitters for advancing PC/RIP/EIP/IP (IEM_MC_ADVANCE_RIP_AND_FINISH_XXX)                                                     *
*********************************************************************************************************************************/

/** Emits the flags check for IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64_WITH_FLAGS
 *  and the other _WITH_FLAGS MCs, see iemRegFinishClearingRF. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFinishInstructionFlagsCheck(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    /*
     * If its not just X86_EFL_RF and CPUMCTX_INHIBIT_SHADOW that are set, we
     * return with special status code and make the execution loop deal with
     * this.  If TF or CPUMCTX_DBG_HIT_DRX_MASK triggers, we have to raise an
     * exception and won't continue execution.  While CPUMCTX_DBG_DBGF_MASK
     * could continue w/o interruption, it probably will drop into the
     * debugger, so not worth the effort of trying to services it here and we
     * just lump it in with the handling of the others.
     *
     * To simplify the code and the register state management even more (wrt
     * immediate in AND operation), we always update the flags and skip the
     * extra check associated conditional jump.
     */
    AssertCompile(   (X86_EFL_TF | X86_EFL_RF | CPUMCTX_INHIBIT_SHADOW | CPUMCTX_DBG_HIT_DRX_MASK | CPUMCTX_DBG_DBGF_MASK)
                  <= UINT32_MAX);
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                             kIemNativeGstRegUse_ForUpdate);
    off = iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfAnySet(pReNative, off, idxEflReg,
                                                             X86_EFL_TF | CPUMCTX_DBG_HIT_DRX_MASK | CPUMCTX_DBG_DBGF_MASK,
                                                             iemNativeLabelCreate(pReNative, kIemNativeLabelType_ReturnWithFlags));
    off = iemNativeEmitAndGpr32ByImm(pReNative, off, idxEflReg, ~(uint32_t)(X86_EFL_RF | CPUMCTX_INHIBIT_SHADOW));
    off = iemNativeEmitStoreGprToVCpuU32(pReNative, off, idxEflReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.eflags));

    /* Free but don't flush the EFLAGS register. */
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    return off;
}


/** The VINF_SUCCESS dummy. */
template<int const a_rcNormal>
DECL_FORCE_INLINE(uint32_t)
iemNativeEmitFinishInstructionWithStatus(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxInstr)
{
    AssertCompile(a_rcNormal == VINF_SUCCESS || a_rcNormal == VINF_IEM_REEXEC_BREAK);
    if (a_rcNormal != VINF_SUCCESS)
    {
#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
        off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
        RT_NOREF_PV(idxInstr);
#endif
        return iemNativeEmitJmpToNewLabel(pReNative, off, kIemNativeLabelType_ReturnBreak);
    }
    return off;
}


#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64(a_cbInstr, a_rcNormal) \
    off = iemNativeEmitAddToRip64AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_cbInstr, a_rcNormal) \
    off = iemNativeEmitAddToRip64AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

/** Same as iemRegAddToRip64AndFinishingNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddToRip64AndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGprImm8(pReNative, off, idxPcReg, cbInstr);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32(a_cbInstr, a_rcNormal) \
    off = iemNativeEmitAddToEip32AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_cbInstr, a_rcNormal) \
    off = iemNativeEmitAddToEip32AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

/** Same as iemRegAddToEip32AndFinishingNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddToEip32AndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGpr32Imm8(pReNative, off, idxPcReg, cbInstr);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}


#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16(a_cbInstr, a_rcNormal) \
    off = iemNativeEmitAddToIp16AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_ADVANCE_RIP_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_cbInstr, a_rcNormal) \
    off = iemNativeEmitAddToIp16AndFinishingNoFlags(pReNative, off, (a_cbInstr)); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

/** Same as iemRegAddToIp16AndFinishingNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddToIp16AndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr)
{
    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

    /* Perform the addition and store the result. */
    off = iemNativeEmitAddGpr32Imm8(pReNative, off, idxPcReg, cbInstr);
    off = iemNativeEmitClear16UpGpr(pReNative, off, idxPcReg);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}



/*********************************************************************************************************************************
*   Emitters for changing PC/RIP/EIP/IP with a relative jump (IEM_MC_REL_JMP_XXX_AND_FINISH_XXX).                                *
*********************************************************************************************************************************/

#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal) \
    off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), \
                                                            (a_enmEffOpSize), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal) \
    off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), \
                                                            (a_enmEffOpSize), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64(a_i16, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), \
                                                            IEMMODE_16BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_i16, a_cbInstr, a_rcNormal) \
        off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), \
                                                                IEMMODE_16BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64(a_i32, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), \
                                                            IEMMODE_64BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_i32, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), \
                                                            IEMMODE_64BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

/** Same as iemRegRip64RelativeJumpS8AndFinishNoFlags,
 *  iemRegRip64RelativeJumpS16AndFinishNoFlags and
 *  iemRegRip64RelativeJumpS32AndFinishNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitRip64RelativeJumpAndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr,
                                                  int32_t offDisp, IEMMODE enmEffOpSize, uint8_t idxInstr)
{
    Assert(enmEffOpSize == IEMMODE_64BIT || enmEffOpSize == IEMMODE_16BIT);

    /* We speculatively modify PC and may raise #GP(0), so make sure the right values are in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

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


#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), \
                                                            (a_enmEffOpSize), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_i8, a_cbInstr, a_enmEffOpSize, a_rcNormal) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), \
                                                            (a_enmEffOpSize), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32(a_i16, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), \
                                                            IEMMODE_16BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_i16, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), \
                                                            IEMMODE_16BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32(a_i32, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), \
                                                            IEMMODE_32BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_i32, a_cbInstr, a_rcNormal) \
            off = iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), \
                                                                    IEMMODE_32BIT, pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

/** Same as iemRegEip32RelativeJumpS8AndFinishNoFlags,
 *  iemRegEip32RelativeJumpS16AndFinishNoFlags and
 *  iemRegEip32RelativeJumpS32AndFinishNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitEip32RelativeJumpAndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr,
                                                  int32_t offDisp, IEMMODE enmEffOpSize, uint8_t idxInstr)
{
    Assert(enmEffOpSize == IEMMODE_32BIT || enmEffOpSize == IEMMODE_16BIT);

    /* We speculatively modify PC and may raise #GP(0), so make sure the right values are in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

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


#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16(a_i8, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S8_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_i8, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int8_t)(a_i8), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16(a_i16, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S16_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_i16, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (int16_t)(a_i16), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC16(a_i32, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

#define IEM_MC_REL_JMP_S32_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_i32, a_cbInstr, a_rcNormal) \
    off = iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(pReNative, off, (a_cbInstr), (a_i32), pCallEntry->idxInstr); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off); \
    off = iemNativeEmitFinishInstructionWithStatus<a_rcNormal>(pReNative, off, pCallEntry->idxInstr)

/** Same as iemRegIp16RelativeJumpS8AndFinishNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitIp16RelativeJumpAndFinishingNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                                 uint8_t cbInstr, int32_t offDisp, uint8_t idxInstr)
{
    /* We speculatively modify PC and may raise #GP(0), so make sure the right values are in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Allocate a temporary PC register. */
    uint8_t const idxPcReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc, kIemNativeGstRegUse_ForUpdate);

    /* Perform the addition, clamp the result, check limit (may #GP(0) + exit TB) and store the result. */
    off = iemNativeEmitAddGpr32Imm(pReNative, off, idxPcReg, offDisp + cbInstr);
    off = iemNativeEmitClear16UpGpr(pReNative, off, idxPcReg);
    off = iemNativeEmitCheckGpr32AgainstSegLimitMaybeRaiseGp0(pReNative, off, idxPcReg, X86_SREG_CS, idxInstr);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    /* Free but don't flush the PC register. */
    iemNativeRegFreeTmp(pReNative, idxPcReg);

    return off;
}



/*********************************************************************************************************************************
*   Emitters for changing PC/RIP/EIP/IP with a indirect jump (IEM_MC_SET_RIP_UXX_AND_FINISH).                                    *
*********************************************************************************************************************************/

/** Variant of IEM_MC_SET_RIP_U16_AND_FINISH for pre-386 targets. */
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC16(a_u16NewIP) \
    off = iemNativeEmitRipJumpNoFlags(pReNative, off, (a_u16NewIP), false /*f64Bit*/, pCallEntry->idxInstr, sizeof(uint16_t))

/** Variant of IEM_MC_SET_RIP_U16_AND_FINISH for 386+ targets. */
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC32(a_u16NewIP) \
    off = iemNativeEmitRipJumpNoFlags(pReNative, off, (a_u16NewIP), false /*f64Bit*/, pCallEntry->idxInstr, sizeof(uint16_t))

/** Variant of IEM_MC_SET_RIP_U16_AND_FINISH for use in 64-bit code. */
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC64(a_u16NewIP) \
    off = iemNativeEmitRipJumpNoFlags(pReNative, off, (a_u16NewIP),  true /*f64Bit*/, pCallEntry->idxInstr, sizeof(uint16_t))

/** Variant of IEM_MC_SET_RIP_U16_AND_FINISH for pre-386 targets that checks and
 *  clears flags. */
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC16_WITH_FLAGS(a_u16NewIP) \
    IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC16(a_u16NewIP); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off)

/** Variant of IEM_MC_SET_RIP_U16_AND_FINISH for 386+ targets that checks and
 *  clears flags. */
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_u16NewIP) \
    IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC32(a_u16NewIP); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off)

/** Variant of IEM_MC_SET_RIP_U16_AND_FINISH for use in 64-bit code that checks and
 *  clears flags. */
#define IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_u16NewIP) \
    IEM_MC_SET_RIP_U16_AND_FINISH_THREADED_PC64(a_u16NewIP); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off)

#undef IEM_MC_SET_RIP_U16_AND_FINISH


/** Variant of IEM_MC_SET_RIP_U32_AND_FINISH for 386+ targets. */
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC32(a_u32NewEIP) \
    off = iemNativeEmitRipJumpNoFlags(pReNative, off, (a_u32NewEIP), false /*f64Bit*/, pCallEntry->idxInstr, sizeof(uint32_t))

/** Variant of IEM_MC_SET_RIP_U32_AND_FINISH for use in 64-bit code. */
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC64(a_u32NewEIP) \
    off = iemNativeEmitRipJumpNoFlags(pReNative, off, (a_u32NewEIP),  true /*f64Bit*/, pCallEntry->idxInstr, sizeof(uint32_t))

/** Variant of IEM_MC_SET_RIP_U32_AND_FINISH for 386+ targets that checks and
 *  clears flags. */
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC32_WITH_FLAGS(a_u32NewEIP) \
    IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC32(a_u32NewEIP); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off)

/** Variant of IEM_MC_SET_RIP_U32_AND_FINISH for use in 64-bit code that checks
 *  and clears flags. */
#define IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_u32NewEIP) \
    IEM_MC_SET_RIP_U32_AND_FINISH_THREADED_PC64(a_u32NewEIP); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off)

#undef IEM_MC_SET_RIP_U32_AND_FINISH


/** Variant of IEM_MC_SET_RIP_U64_AND_FINISH for use in 64-bit code. */
#define IEM_MC_SET_RIP_U64_AND_FINISH_THREADED_PC64(a_u64NewEIP) \
    off = iemNativeEmitRipJumpNoFlags(pReNative, off, (a_u64NewEIP),  true /*f64Bit*/, pCallEntry->idxInstr, sizeof(uint64_t))

/** Variant of IEM_MC_SET_RIP_U64_AND_FINISH for use in 64-bit code that checks
 *  and clears flags. */
#define IEM_MC_SET_RIP_U64_AND_FINISH_THREADED_PC64_WITH_FLAGS(a_u64NewEIP) \
    IEM_MC_SET_RIP_U64_AND_FINISH_THREADED_PC64(a_u64NewEIP); \
    off = iemNativeEmitFinishInstructionFlagsCheck(pReNative, off)

#undef IEM_MC_SET_RIP_U64_AND_FINISH


/** Same as iemRegRipJumpU16AndFinishNoFlags,
 *  iemRegRipJumpU32AndFinishNoFlags and iemRegRipJumpU64AndFinishNoFlags. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitRipJumpNoFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarPc, bool f64Bit,
                            uint8_t idxInstr, uint8_t cbVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarPc);
    Assert(pReNative->Core.aVars[idxVarPc].cbVar == cbVar);

    /* We speculatively modify PC and may raise #GP(0), so make sure the right values are in CPUMCTX. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /* Get a register with the new PC loaded from idxVarPc.
       Note! This ASSUMES that the high bits of the GPR is zeroed. */
    uint8_t const idxPcReg = iemNativeVarRegisterAcquireForGuestReg(pReNative, idxVarPc, kIemNativeGstReg_Pc, &off);

    /* Check limit (may #GP(0) + exit TB). */
    if (!f64Bit)
        off = iemNativeEmitCheckGpr32AgainstSegLimitMaybeRaiseGp0(pReNative, off, idxPcReg, X86_SREG_CS, idxInstr);
    /* Check that the address is canonical, raising #GP(0) + exit TB if it isn't. */
    else if (cbVar > sizeof(uint32_t))
        off = iemNativeEmitCheckGprCanonicalMaybeRaiseGp0(pReNative, off, idxPcReg, idxInstr);

    /* Store the result. */
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxPcReg, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rip));

    iemNativeVarRegisterRelease(pReNative, idxVarPc);
    /** @todo implictly free the variable? */

    return off;
}



/*********************************************************************************************************************************
*   Emitters for conditionals (IEM_MC_IF_XXX, IEM_MC_ELSE, IEM_MC_ENDIF)                                                         *
*********************************************************************************************************************************/

/**
 * Pushes an IEM_MC_IF_XXX onto the condition stack.
 *
 * @returns Pointer to the condition stack entry on success, NULL on failure
 *          (too many nestings)
 */
DECL_INLINE_THROW(PIEMNATIVECOND) iemNativeCondPushIf(PIEMRECOMPILERSTATE pReNative)
{
    uint32_t const idxStack = pReNative->cCondDepth;
    AssertStmt(idxStack < RT_ELEMENTS(pReNative->aCondStack), IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_COND_TOO_DEEPLY_NESTED));

    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[idxStack];
    pReNative->cCondDepth = (uint8_t)(idxStack + 1);

    uint16_t const uCondSeqNo = ++pReNative->uCondSeqNo;
    pEntry->fInElse       = false;
    pEntry->idxLabelElse  = iemNativeLabelCreate(pReNative, kIemNativeLabelType_Else, UINT32_MAX /*offWhere*/, uCondSeqNo);
    pEntry->idxLabelEndIf = iemNativeLabelCreate(pReNative, kIemNativeLabelType_Endif, UINT32_MAX /*offWhere*/, uCondSeqNo);

    return pEntry;
}


/**
 * Start of the if-block, snapshotting the register and variable state.
 */
DECL_INLINE_THROW(void)
iemNativeCondStartIfBlock(PIEMRECOMPILERSTATE pReNative, uint32_t offIfBlock, uint32_t idxLabelIf = UINT32_MAX)
{
    Assert(offIfBlock != UINT32_MAX);
    Assert(pReNative->cCondDepth > 0 && pReNative->cCondDepth <= RT_ELEMENTS(pReNative->aCondStack));
    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[pReNative->cCondDepth - 1];
    Assert(!pEntry->fInElse);

    /* Define the start of the IF block if request or for disassembly purposes. */
    if (idxLabelIf != UINT32_MAX)
        iemNativeLabelDefine(pReNative, idxLabelIf, offIfBlock);
#ifdef IEMNATIVE_WITH_TB_DEBUG_INFO
    else
        iemNativeLabelCreate(pReNative, kIemNativeLabelType_If, offIfBlock, pReNative->paLabels[pEntry->idxLabelElse].uData);
#else
    RT_NOREF(offIfBlock);
#endif

    /* Copy the initial state so we can restore it in the 'else' block. */
    pEntry->InitialState = pReNative->Core;
}


#define IEM_MC_ELSE() } while (0); \
        off = iemNativeEmitElse(pReNative, off); \
        do {

/** Emits code related to IEM_MC_ELSE. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitElse(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    /* Check sanity and get the conditional stack entry. */
    Assert(off != UINT32_MAX);
    Assert(pReNative->cCondDepth > 0 && pReNative->cCondDepth <= RT_ELEMENTS(pReNative->aCondStack));
    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[pReNative->cCondDepth - 1];
    Assert(!pEntry->fInElse);

    /* Jump to the endif */
    off = iemNativeEmitJmpToLabel(pReNative, off, pEntry->idxLabelEndIf);

    /* Define the else label and enter the else part of the condition. */
    iemNativeLabelDefine(pReNative, pEntry->idxLabelElse, off);
    pEntry->fInElse = true;

    /* Snapshot the core state so we can do a merge at the endif and restore
       the snapshot we took at the start of the if-block. */
    pEntry->IfFinalState = pReNative->Core;
    pReNative->Core = pEntry->InitialState;

    return off;
}


#define IEM_MC_ENDIF() } while (0); \
        off = iemNativeEmitEndIf(pReNative, off)

/** Emits code related to IEM_MC_ENDIF. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitEndIf(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    /* Check sanity and get the conditional stack entry. */
    Assert(off != UINT32_MAX);
    Assert(pReNative->cCondDepth > 0 && pReNative->cCondDepth <= RT_ELEMENTS(pReNative->aCondStack));
    PIEMNATIVECOND const pEntry = &pReNative->aCondStack[pReNative->cCondDepth - 1];

    /*
     * Now we have find common group with the core state at the end of the
     * if-final.  Use the smallest common denominator and just drop anything
     * that isn't the same in both states.
     */
    /** @todo We could, maybe, shuffle registers around if we thought it helpful,
     *        which is why we're doing this at the end of the else-block.
     *        But we'd need more info about future for that to be worth the effort. */
    PCIEMNATIVECORESTATE const pOther = pEntry->fInElse ? &pEntry->IfFinalState : &pEntry->InitialState;
    if (memcmp(&pReNative->Core, pOther, sizeof(*pOther)) != 0)
    {
        /* shadow guest stuff first. */
        uint64_t fGstRegs = pReNative->Core.bmGstRegShadows;
        if (fGstRegs)
        {
            Assert(pReNative->Core.bmHstRegsWithGstShadow != 0);
            do
            {
                unsigned idxGstReg = ASMBitFirstSetU64(fGstRegs) - 1;
                fGstRegs &= ~RT_BIT_64(idxGstReg);

                uint8_t const idxHstReg = pReNative->Core.aidxGstRegShadows[idxGstReg];
                if (  !(pOther->bmGstRegShadows & RT_BIT_64(idxGstReg))
                    || idxHstReg != pOther->aidxGstRegShadows[idxGstReg])
                {
                    Log12(("iemNativeEmitEndIf: dropping gst %s from hst %s\n",
                           g_aGstShadowInfo[idxGstReg].pszName, g_apszIemNativeHstRegNames[idxHstReg]));
                    iemNativeRegClearGstRegShadowing(pReNative, idxHstReg, off);
                }
            } while (fGstRegs);
        }
        else
            Assert(pReNative->Core.bmHstRegsWithGstShadow == 0);

        /* Check variables next. For now we must require them to be identical
           or stuff we can recreate. */
        Assert(pReNative->Core.u64ArgVars == pOther->u64ArgVars);
        uint32_t fVars = pReNative->Core.bmVars | pOther->bmVars;
        if (fVars)
        {
            uint32_t const fVarsMustRemove = pReNative->Core.bmVars ^ pOther->bmVars;
            do
            {
                unsigned idxVar = ASMBitFirstSetU32(fVars) - 1;
                fVars &= ~RT_BIT_32(idxVar);

                if (!(fVarsMustRemove & RT_BIT_32(idxVar)))
                {
                    if (pReNative->Core.aVars[idxVar].idxReg == pOther->aVars[idxVar].idxReg)
                        continue;
                    if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Stack)
                    {
                        uint8_t const idxHstReg = pReNative->Core.aVars[idxVar].idxReg;
                        if (idxHstReg != UINT8_MAX)
                        {
                            pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
                            pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
                            Log12(("iemNativeEmitEndIf: Dropping hst reg %s for var #%u\n",
                                   g_apszIemNativeHstRegNames[idxHstReg], idxVar));
                        }
                        continue;
                    }
                }
                else if (!(pReNative->Core.bmVars & RT_BIT_32(idxVar)))
                    continue;

                /* Irreconcilable, so drop it. */
                uint8_t const idxHstReg = pReNative->Core.aVars[idxVar].idxReg;
                if (idxHstReg != UINT8_MAX)
                {
                    pReNative->Core.bmHstRegs &= ~RT_BIT_32(idxHstReg);
                    pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
                    Log12(("iemNativeEmitEndIf: Dropping hst reg %s for var #%u (also dropped)\n",
                           g_apszIemNativeHstRegNames[idxHstReg], idxVar));
                }
                Log11(("iemNativeEmitEndIf: Freeing variable #%u\n", idxVar));
                pReNative->Core.bmVars &= ~RT_BIT_32(idxVar);
            } while (fVars);
        }

        /* Finally, check that the host register allocations matches. */
        AssertMsgStmt(pReNative->Core.bmHstRegs == pOther->bmHstRegs,
                      ("Core.bmHstRegs=%#x pOther->bmHstRegs=%#x - %#x\n",
                       pReNative->Core.bmHstRegs, pOther->bmHstRegs, pReNative->Core.bmHstRegs ^ pOther->bmHstRegs),
                      IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_COND_ENDIF_RECONCILIATION_FAILED));
    }

    /*
     * Define the endif label and maybe the else one if we're still in the 'if' part.
     */
    if (!pEntry->fInElse)
        iemNativeLabelDefine(pReNative, pEntry->idxLabelElse, off);
    else
        Assert(pReNative->paLabels[pEntry->idxLabelElse].off <= off);
    iemNativeLabelDefine(pReNative, pEntry->idxLabelEndIf, off);

    /* Pop the conditional stack.*/
    pReNative->cCondDepth -= 1;

    return off;
}


#define IEM_MC_IF_EFL_ANY_BITS_SET(a_fBits) \
        off = iemNativeEmitIfEflagAnysBitsSet(pReNative, off, (a_fBits)); \
        do {

/** Emits code for IEM_MC_IF_EFL_ANY_BITS_SET. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfEflagAnysBitsSet(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitsInEfl)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    /* Test and jump. */
    off = iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfNoneSet(pReNative, off, idxEflReg, fBitsInEfl, pEntry->idxLabelElse);

    /* Free but don't flush the EFlags register. */
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    /* Make a copy of the core state now as we start the if-block. */
    iemNativeCondStartIfBlock(pReNative, off);

    return off;
}


#define IEM_MC_IF_EFL_NO_BITS_SET(a_fBits) \
        off = iemNativeEmitIfEflagNoBitsSet(pReNative, off, (a_fBits)); \
        do {

/** Emits code for IEM_MC_IF_EFL_NO_BITS_SET. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfEflagNoBitsSet(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitsInEfl)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    /* Test and jump. */
    off = iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfAnySet(pReNative, off, idxEflReg, fBitsInEfl, pEntry->idxLabelElse);

    /* Free but don't flush the EFlags register. */
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    /* Make a copy of the core state now as we start the if-block. */
    iemNativeCondStartIfBlock(pReNative, off);

    return off;
}


#define IEM_MC_IF_EFL_BIT_SET(a_fBit) \
        off = iemNativeEmitIfEflagsBitSet(pReNative, off, (a_fBit)); \
        do {

/** Emits code for IEM_MC_IF_EFL_BIT_SET. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfEflagsBitSet(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitInEfl)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    unsigned const iBitNo = ASMBitFirstSetU32(fBitInEfl) - 1;
    Assert(RT_BIT_32(iBitNo) == fBitInEfl);

    /* Test and jump. */
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfNotSet(pReNative, off, idxEflReg, iBitNo, pEntry->idxLabelElse);

    /* Free but don't flush the EFlags register. */
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    /* Make a copy of the core state now as we start the if-block. */
    iemNativeCondStartIfBlock(pReNative, off);

    return off;
}


#define IEM_MC_IF_EFL_BIT_NOT_SET(a_fBit) \
        off = iemNativeEmitIfEflagsBitNotSet(pReNative, off, (a_fBit)); \
        do {

/** Emits code for IEM_MC_IF_EFL_BIT_NOT_SET. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfEflagsBitNotSet(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitInEfl)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    unsigned const iBitNo = ASMBitFirstSetU32(fBitInEfl) - 1;
    Assert(RT_BIT_32(iBitNo) == fBitInEfl);

    /* Test and jump. */
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfSet(pReNative, off, idxEflReg, iBitNo, pEntry->idxLabelElse);

    /* Free but don't flush the EFlags register. */
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    /* Make a copy of the core state now as we start the if-block. */
    iemNativeCondStartIfBlock(pReNative, off);

    return off;
}


#define IEM_MC_IF_EFL_BITS_EQ(a_fBit1, a_fBit2)         \
    off = iemNativeEmitIfEflagsTwoBitsEqual(pReNative, off, a_fBit1, a_fBit2, false /*fInverted*/); \
    do {

#define IEM_MC_IF_EFL_BITS_NE(a_fBit1, a_fBit2)         \
    off = iemNativeEmitIfEflagsTwoBitsEqual(pReNative, off, a_fBit1, a_fBit2, true /*fInverted*/); \
    do {

/** Emits code for IEM_MC_IF_EFL_BITS_EQ and IEM_MC_IF_EFL_BITS_NE. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitIfEflagsTwoBitsEqual(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                  uint32_t fBit1InEfl, uint32_t fBit2InEfl, bool fInverted)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    unsigned const iBitNo1 = ASMBitFirstSetU32(fBit1InEfl) - 1;
    Assert(RT_BIT_32(iBitNo1) == fBit1InEfl);

    unsigned const iBitNo2 = ASMBitFirstSetU32(fBit2InEfl) - 1;
    Assert(RT_BIT_32(iBitNo2) == fBit2InEfl);
    Assert(iBitNo1 != iBitNo2);

#ifdef RT_ARCH_AMD64
    uint8_t const idxTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBit1InEfl);

    off = iemNativeEmitAndGpr32ByGpr32(pReNative, off, idxTmpReg, idxEflReg);
    if (iBitNo1 > iBitNo2)
        off = iemNativeEmitShiftGpr32Right(pReNative, off, idxTmpReg, iBitNo1 - iBitNo2);
    else
        off = iemNativeEmitShiftGpr32Left(pReNative, off, idxTmpReg, iBitNo2 - iBitNo1);
    off = iemNativeEmitXorGpr32ByGpr32(pReNative, off, idxTmpReg, idxEflReg);

#elif defined(RT_ARCH_ARM64)
    uint8_t const    idxTmpReg   = iemNativeRegAllocTmp(pReNative, &off);
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);

    /* and tmpreg, eflreg, #1<<iBitNo1 */
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(idxTmpReg, idxEflReg, 0 /*uImm7SizeLen -> 32*/, 32 - iBitNo1, false /*f64Bit*/);

    /* eeyore tmpreg, eflreg, tmpreg, LSL/LSR, #abs(iBitNo2 - iBitNo1) */
    if (iBitNo1 > iBitNo2)
        pu32CodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxEflReg, idxTmpReg, false /*64bit*/,
                                                iBitNo1 - iBitNo2, kArmv8A64InstrShift_Lsr);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxEflReg, idxTmpReg, false /*64bit*/,
                                                iBitNo2 - iBitNo1, kArmv8A64InstrShift_Lsl);

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif

    /* Test (bit #2 is set in tmpreg if not-equal) and jump. */
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, idxTmpReg, iBitNo2,
                                                     pEntry->idxLabelElse, !fInverted /*fJmpIfSet*/);

    /* Free but don't flush the EFlags and tmp registers. */
    iemNativeRegFreeTmp(pReNative, idxTmpReg);
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    /* Make a copy of the core state now as we start the if-block. */
    iemNativeCondStartIfBlock(pReNative, off);

    return off;
}


#define IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ(a_fBit, a_fBit1, a_fBit2) \
    off = iemNativeEmitIfEflagsBitNotSetAndTwoBitsEqual(pReNative, off, a_fBit, a_fBit1, a_fBit2, false /*fInverted*/); \
    do {

#define IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE(a_fBit, a_fBit1, a_fBit2) \
    off = iemNativeEmitIfEflagsBitNotSetAndTwoBitsEqual(pReNative, off, a_fBit, a_fBit1, a_fBit2, true /*fInverted*/); \
    do {

/** Emits code for IEM_MC_IF_EFL_BIT_NOT_SET_AND_BITS_EQ and
 *  IEM_MC_IF_EFL_BIT_SET_OR_BITS_NE. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitIfEflagsBitNotSetAndTwoBitsEqual(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitInEfl,
                                              uint32_t fBit1InEfl, uint32_t fBit2InEfl, bool fInverted)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* We need an if-block label for the non-inverted variant. */
    uint32_t const idxLabelIf = fInverted ? iemNativeLabelCreate(pReNative, kIemNativeLabelType_If, UINT32_MAX,
                                                                 pReNative->paLabels[pEntry->idxLabelElse].uData) : UINT32_MAX;

    /* Get the eflags. */
    uint8_t const idxEflReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                              kIemNativeGstRegUse_ReadOnly);

    /* Translate the flag masks to bit numbers. */
    unsigned const iBitNo = ASMBitFirstSetU32(fBitInEfl) - 1;
    Assert(RT_BIT_32(iBitNo) == fBitInEfl);

    unsigned const iBitNo1 = ASMBitFirstSetU32(fBit1InEfl) - 1;
    Assert(RT_BIT_32(iBitNo1) == fBit1InEfl);
    Assert(iBitNo1 != iBitNo);

    unsigned const iBitNo2 = ASMBitFirstSetU32(fBit2InEfl) - 1;
    Assert(RT_BIT_32(iBitNo2) == fBit2InEfl);
    Assert(iBitNo2 != iBitNo);
    Assert(iBitNo2 != iBitNo1);

#ifdef RT_ARCH_AMD64
    uint8_t const idxTmpReg = iemNativeRegAllocTmpImm(pReNative, &off, fBit1InEfl); /* This must come before we jump anywhere! */
#elif defined(RT_ARCH_ARM64)
    uint8_t const idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);
#endif

    /* Check for the lone bit first. */
    if (!fInverted)
        off = iemNativeEmitTestBitInGprAndJmpToLabelIfSet(pReNative, off, idxEflReg, iBitNo, pEntry->idxLabelElse);
    else
        off = iemNativeEmitTestBitInGprAndJmpToLabelIfSet(pReNative, off, idxEflReg, iBitNo, idxLabelIf);

    /* Then extract and compare the other two bits. */
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitAndGpr32ByGpr32(pReNative, off, idxTmpReg, idxEflReg);
    if (iBitNo1 > iBitNo2)
        off = iemNativeEmitShiftGpr32Right(pReNative, off, idxTmpReg, iBitNo1 - iBitNo2);
    else
        off = iemNativeEmitShiftGpr32Left(pReNative, off, idxTmpReg, iBitNo2 - iBitNo1);
    off = iemNativeEmitXorGpr32ByGpr32(pReNative, off, idxTmpReg, idxEflReg);

#elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);

    /* and tmpreg, eflreg, #1<<iBitNo1 */
    pu32CodeBuf[off++] = Armv8A64MkInstrAndImm(idxTmpReg, idxEflReg, 0 /*uImm7SizeLen -> 32*/, 32 - iBitNo1, false /*f64Bit*/);

    /* eeyore tmpreg, eflreg, tmpreg, LSL/LSR, #abs(iBitNo2 - iBitNo1) */
    if (iBitNo1 > iBitNo2)
        pu32CodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxEflReg, idxTmpReg, false /*64bit*/,
                                                iBitNo1 - iBitNo2, kArmv8A64InstrShift_Lsr);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrEor(idxTmpReg, idxEflReg, idxTmpReg, false /*64bit*/,
                                                iBitNo2 - iBitNo1, kArmv8A64InstrShift_Lsl);

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

#else
# error "Port me"
#endif

    /* Test (bit #2 is set in tmpreg if not-equal) and jump. */
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, idxTmpReg, iBitNo2,
                                                     pEntry->idxLabelElse, !fInverted /*fJmpIfSet*/);

    /* Free but don't flush the EFlags and tmp registers. */
    iemNativeRegFreeTmp(pReNative, idxTmpReg);
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    /* Make a copy of the core state now as we start the if-block. */
    iemNativeCondStartIfBlock(pReNative, off, idxLabelIf);

    return off;
}


#define IEM_MC_IF_CX_IS_NZ() \
    off = iemNativeEmitIfCxIsNotZero(pReNative, off); \
    do {

/** Emits code for IEM_MC_IF_CX_IS_NZ. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfCxIsNotZero(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    uint8_t const idxGstRcxReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xCX),
                                                                 kIemNativeGstRegUse_ReadOnly);
    off = iemNativeEmitTestAnyBitsInGprAndJmpToLabelIfNoneSet(pReNative, off, idxGstRcxReg, UINT16_MAX, pEntry->idxLabelElse);
    iemNativeRegFreeTmp(pReNative, idxGstRcxReg);

    iemNativeCondStartIfBlock(pReNative, off);
    return off;
}


#define IEM_MC_IF_ECX_IS_NZ() \
    off = iemNativeEmitIfRcxEcxIsNotZero(pReNative, off, false /*f64Bit*/); \
    do {

#define IEM_MC_IF_RCX_IS_NZ() \
    off = iemNativeEmitIfRcxEcxIsNotZero(pReNative, off, true /*f64Bit*/); \
    do {

/** Emits code for IEM_MC_IF_ECX_IS_NZ and IEM_MC_IF_RCX_IS_NZ. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfRcxEcxIsNotZero(PIEMRECOMPILERSTATE pReNative, uint32_t off, bool f64Bit)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    uint8_t const idxGstRcxReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xCX),
                                                                 kIemNativeGstRegUse_ReadOnly);
    off = iemNativeEmitTestIfGprIsZeroAndJmpToLabel(pReNative, off, idxGstRcxReg, f64Bit, pEntry->idxLabelElse);
    iemNativeRegFreeTmp(pReNative, idxGstRcxReg);

    iemNativeCondStartIfBlock(pReNative, off);
    return off;
}


#define IEM_MC_IF_CX_IS_NOT_ONE() \
    off = iemNativeEmitIfCxIsNotOne(pReNative, off); \
    do {

/** Emits code for IEM_MC_IF_CX_IS_NOT_ONE. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfCxIsNotOne(PIEMRECOMPILERSTATE pReNative, uint32_t off)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    uint8_t const idxGstRcxReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xCX),
                                                                 kIemNativeGstRegUse_ReadOnly);
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitTestIfGpr16EqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse);
#else
    uint8_t const idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);
    off = iemNativeEmitTestIfGpr16EqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse, idxTmpReg);
    iemNativeRegFreeTmp(pReNative, idxTmpReg);
#endif
    iemNativeRegFreeTmp(pReNative, idxGstRcxReg);

    iemNativeCondStartIfBlock(pReNative, off);
    return off;
}


#define IEM_MC_IF_ECX_IS_NOT_ONE() \
    off = iemNativeEmitIfRcxEcxIsNotOne(pReNative, off, false /*f64Bit*/); \
    do {

#define IEM_MC_IF_RCX_IS_NOT_ONE() \
    off = iemNativeEmitIfRcxEcxIsNotOne(pReNative, off, true /*f64Bit*/); \
    do {

/** Emits code for IEM_MC_IF_ECX_IS_NOT_ONE and IEM_MC_IF_RCX_IS_NOT_ONE. */
DECL_INLINE_THROW(uint32_t) iemNativeEmitIfRcxEcxIsNotOne(PIEMRECOMPILERSTATE pReNative, uint32_t off, bool f64Bit)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    uint8_t const idxGstRcxReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xCX),
                                                                 kIemNativeGstRegUse_ReadOnly);
    if (f64Bit)
        off = iemNativeEmitTestIfGprEqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse);
    else
        off = iemNativeEmitTestIfGpr32EqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse);
    iemNativeRegFreeTmp(pReNative, idxGstRcxReg);

    iemNativeCondStartIfBlock(pReNative, off);
    return off;
}


#define IEM_MC_IF_CX_IS_NOT_ONE_AND_EFL_BIT_SET(a_fBit) \
    off = iemNativeEmitIfCxIsNotOneAndTestEflagsBit(pReNative, off, a_fBit, true /*fCheckIfSet*/); \
    do {

#define IEM_MC_IF_CX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET(a_fBit) \
    off = iemNativeEmitIfCxIsNotOneAndTestEflagsBit(pReNative, off, a_fBit, false /*fCheckIfSet*/); \
    do {

/** Emits code for IEM_MC_IF_CX_IS_NOT_ONE_AND_EFL_BIT_SET and
 *  IEM_MC_IF_CX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitIfCxIsNotOneAndTestEflagsBit(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint32_t fBitInEfl, bool fCheckIfSet)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* We have to load both RCX and EFLAGS before we can start branching,
       otherwise we'll end up in the else-block with an inconsistent
       register allocator state.
       Doing EFLAGS first as it's more likely to be loaded, right? */
    uint8_t const idxEflReg    = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                                 kIemNativeGstRegUse_ReadOnly);
    uint8_t const idxGstRcxReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xCX),
                                                                 kIemNativeGstRegUse_ReadOnly);

    /** @todo we could reduce this to a single branch instruction by spending a
     *        temporary register and some setnz stuff.  Not sure if loops are
     *        worth it. */
    /* Check CX. */
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitTestIfGpr16EqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse);
#else
    uint8_t const idxTmpReg = iemNativeRegAllocTmp(pReNative, &off);
    off = iemNativeEmitTestIfGpr16EqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse, idxTmpReg);
    iemNativeRegFreeTmp(pReNative, idxTmpReg);
#endif

    /* Check the EFlags bit. */
    unsigned const iBitNo = ASMBitFirstSetU32(fBitInEfl) - 1;
    Assert(RT_BIT_32(iBitNo) == fBitInEfl);
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, idxEflReg, iBitNo, pEntry->idxLabelElse,
                                                     !fCheckIfSet /*fJmpIfSet*/);

    iemNativeRegFreeTmp(pReNative, idxGstRcxReg);
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    iemNativeCondStartIfBlock(pReNative, off);
    return off;
}


#define IEM_MC_IF_ECX_IS_NOT_ONE_AND_EFL_BIT_SET(a_fBit) \
    off = iemNativeEmitIfRcxEcxIsNotOneAndTestEflagsBit(pReNative, off, a_fBit, true /*fCheckIfSet*/, false /*f64Bit*/); \
    do {

#define IEM_MC_IF_ECX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET(a_fBit) \
    off = iemNativeEmitIfRcxEcxIsNotOneAndTestEflagsBit(pReNative, off, a_fBit, false /*fCheckIfSet*/, false /*f64Bit*/); \
    do {

#define IEM_MC_IF_RCX_IS_NOT_ONE_AND_EFL_BIT_SET(a_fBit) \
    off = iemNativeEmitIfRcxEcxIsNotOneAndTestEflagsBit(pReNative, off, a_fBit, true /*fCheckIfSet*/, true /*f64Bit*/); \
    do {

#define IEM_MC_IF_RCX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET(a_fBit) \
    off = iemNativeEmitIfRcxEcxIsNotOneAndTestEflagsBit(pReNative, off, a_fBit, false /*fCheckIfSet*/, true /*f64Bit*/); \
    do {

/** Emits code for IEM_MC_IF_ECX_IS_NOT_ONE_AND_EFL_BIT_SET,
 *  IEM_MC_IF_ECX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET,
 *  IEM_MC_IF_RCX_IS_NOT_ONE_AND_EFL_BIT_SET and
 *  IEM_MC_IF_RCX_IS_NOT_ONE_AND_EFL_BIT_NOT_SET. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitIfRcxEcxIsNotOneAndTestEflagsBit(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                               uint32_t fBitInEfl, bool fCheckIfSet, bool f64Bit)
{
    PIEMNATIVECOND const pEntry = iemNativeCondPushIf(pReNative);

    /* We have to load both RCX and EFLAGS before we can start branching,
       otherwise we'll end up in the else-block with an inconsistent
       register allocator state.
       Doing EFLAGS first as it's more likely to be loaded, right? */
    uint8_t const idxEflReg    = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_EFlags,
                                                                 kIemNativeGstRegUse_ReadOnly);
    uint8_t const idxGstRcxReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xCX),
                                                                 kIemNativeGstRegUse_ReadOnly);

    /** @todo we could reduce this to a single branch instruction by spending a
     *        temporary register and some setnz stuff.  Not sure if loops are
     *        worth it. */
    /* Check RCX/ECX. */
    if (f64Bit)
        off = iemNativeEmitTestIfGprEqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse);
    else
        off = iemNativeEmitTestIfGpr32EqualsImmAndJmpToLabel(pReNative, off, idxGstRcxReg, 1, pEntry->idxLabelElse);

    /* Check the EFlags bit. */
    unsigned const iBitNo = ASMBitFirstSetU32(fBitInEfl) - 1;
    Assert(RT_BIT_32(iBitNo) == fBitInEfl);
    off = iemNativeEmitTestBitInGprAndJmpToLabelIfCc(pReNative, off, idxEflReg, iBitNo, pEntry->idxLabelElse,
                                                     !fCheckIfSet /*fJmpIfSet*/);

    iemNativeRegFreeTmp(pReNative, idxGstRcxReg);
    iemNativeRegFreeTmp(pReNative, idxEflReg);

    iemNativeCondStartIfBlock(pReNative, off);
    return off;
}



/*********************************************************************************************************************************
*   Emitters for IEM_MC_ARG_XXX, IEM_MC_LOCAL, IEM_MC_LOCAL_CONST, ++                                                            *
*********************************************************************************************************************************/
/** Number of hidden arguments for CIMPL calls.
 * @note We're sufferning from the usual VBOXSTRICTRC fun on Windows. */
#if defined(VBOXSTRICTRC_STRICT_ENABLED) && defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64)
# define IEM_CIMPL_HIDDEN_ARGS 3
#else
# define IEM_CIMPL_HIDDEN_ARGS 2
#endif

#define IEM_MC_NOREF(a_Name) \
    RT_NOREF_PV(a_Name)

#define IEM_MC_ARG(a_Type, a_Name, a_iArg) \
    uint8_t const a_Name = iemNativeArgAlloc(pReNative, (a_iArg), sizeof(a_Type))

#define IEM_MC_ARG_CONST(a_Type, a_Name, a_Value, a_iArg) \
    uint8_t const a_Name = iemNativeArgAllocConst(pReNative, (a_iArg), sizeof(a_Type), (a_Value))

#define IEM_MC_ARG_LOCAL_REF(a_Type, a_Name, a_Local, a_iArg) \
    uint8_t const a_Name = iemNativeArgAllocLocalRef(pReNative, (a_iArg), (a_Local))

#define IEM_MC_LOCAL(a_Type, a_Name) \
    uint8_t const a_Name = iemNativeVarAlloc(pReNative, sizeof(a_Type))

#define IEM_MC_LOCAL_CONST(a_Type, a_Name, a_Value) \
    uint8_t const a_Name = iemNativeVarAllocConst(pReNative, sizeof(a_Type), (a_Value))


/**
 * Gets the number of hidden arguments for an expected IEM_MC_CALL statement.
 */
DECLINLINE(uint8_t) iemNativeArgGetHiddenArgCount(PIEMRECOMPILERSTATE pReNative)
{
    if (pReNative->fCImpl & IEM_CIMPL_F_CALLS_CIMPL)
        return IEM_CIMPL_HIDDEN_ARGS;
    if (pReNative->fCImpl & IEM_CIMPL_F_CALLS_AIMPL_WITH_FXSTATE)
        return 1;
    return 0;
}


/**
 * Internal work that allocates a variable with kind set to
 * kIemNativeVarKind_Invalid and no current stack allocation.
 *
 * The kind will either be set by the caller or later when the variable is first
 * assigned a value.
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
    return idxVar;
}


/**
 * Internal work that allocates an argument variable w/o setting enmKind.
 */
static uint8_t iemNativeArgAllocInt(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t cbType)
{
    iArgNo += iemNativeArgGetHiddenArgCount(pReNative);
    AssertStmt(iArgNo < RT_ELEMENTS(pReNative->Core.aidxArgVars), IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_1));
    AssertStmt(pReNative->Core.aidxArgVars[iArgNo] == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_DUP_ARG_NO));

    uint8_t const idxVar = iemNativeVarAllocInt(pReNative, cbType);
    pReNative->Core.aidxArgVars[iArgNo]  = idxVar;
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
    Assert(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack);

    /* Already got a slot? */
    uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
    if (idxStackSlot != UINT8_MAX)
    {
        Assert(idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS);
        return idxStackSlot;
    }

    /*
     * A single slot is easy to allocate.
     * Allocate them from the top end, closest to BP, to reduce the displacement.
     */
    if (pReNative->Core.aVars[idxVar].cbVar <= sizeof(uint64_t))
    {
        unsigned const iSlot = ASMBitLastSetU32(~pReNative->Core.bmStack) - 1;
        AssertStmt(iSlot < IEMNATIVE_FRAME_VAR_SLOTS, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_OUT_OF_STACK_SLOTS));
        pReNative->Core.bmStack |= RT_BIT_32(iSlot);
        pReNative->Core.aVars[idxVar].idxStackSlot = (uint8_t)iSlot;
        Log11(("iemNativeVarSetKindToStack: idxVar=%d iSlot=%#x\n", idxVar, iSlot));
        return (uint8_t)iSlot;
    }

    /*
     * We need more than one stack slot.
     *
     * cbVar -> fBitAlignMask: 16 -> 1; 32 -> 3; 64 -> 7;
     */
    AssertCompile(RT_IS_POWER_OF_TWO(IEMNATIVE_FRAME_VAR_SLOTS)); /* If not we have to add an overflow check. */
    Assert(pReNative->Core.aVars[idxVar].cbVar <= 64);
    uint32_t const fBitAlignMask = RT_BIT_32(ASMBitLastSetU32(pReNative->Core.aVars[idxVar].cbVar) - 4) - 1;
    uint32_t       fBitAllocMask = RT_BIT_32((pReNative->Core.aVars[idxVar].cbVar + 7) >> 3) - 1;
    uint32_t       bmStack       = ~pReNative->Core.bmStack;
    while (bmStack != UINT32_MAX)
    {
/** @todo allocate from the top to reduce BP displacement. */
        unsigned const iSlot = ASMBitFirstSetU32(bmStack) - 1;
        AssertStmt(iSlot < IEMNATIVE_FRAME_VAR_SLOTS, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_OUT_OF_STACK_SLOTS));
        if (!(iSlot & fBitAlignMask))
        {
            if ((bmStack & (fBitAllocMask << iSlot)) == (fBitAllocMask << iSlot))
            {
                pReNative->Core.bmStack |= (fBitAllocMask << iSlot);
                pReNative->Core.aVars[idxVar].idxStackSlot = (uint8_t)iSlot;
                Log11(("iemNativeVarSetKindToStack: idxVar=%d iSlot=%#x/%#x (cbVar=%#x)\n",
                       idxVar, iSlot, fBitAllocMask, pReNative->Core.aVars[idxVar].cbVar));
                return (uint8_t)iSlot;
            }
        }
        bmStack |= fBitAlignMask << (iSlot & ~fBitAlignMask);
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
static void iemNativeVarSetKindToStack(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Stack)
    {
        /* We could in theory transition from immediate to stack as well, but it
           would involve the caller doing work storing the value on the stack. So,
           till that's required we only allow transition from invalid. */
        AssertStmt(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Invalid,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        AssertStmt(pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pReNative->Core.aVars[idxVar].enmKind = kIemNativeVarKind_Stack;

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
static void iemNativeVarSetKindToConst(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, uint64_t uValue)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_Immediate)
    {
        /* Only simple transitions for now. */
        AssertStmt(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Invalid,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pReNative->Core.aVars[idxVar].enmKind = kIemNativeVarKind_Immediate;
    }
    AssertStmt(pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));

    pReNative->Core.aVars[idxVar].u.uValue = uValue;
    AssertMsg(   pReNative->Core.aVars[idxVar].cbVar >= sizeof(uint64_t)
              || pReNative->Core.aVars[idxVar].u.uValue < RT_BIT_64(pReNative->Core.aVars[idxVar].cbVar * 8),
              ("idxVar=%d cbVar=%u uValue=%#RX64\n", idxVar, pReNative->Core.aVars[idxVar].cbVar, uValue));
}


/**
 * Sets the variable to a reference (pointer) to @a idxOtherVar.
 *
 * This does not require stack storage as we know the value and can always
 * reload it.  Loading is postponed till needed.
 *
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @param   idxOtherVar The variable to take the (stack) address of.
 *
 * @throws  VERR_IEM_VAR_OUT_OF_STACK_SLOTS, VERR_IEM_VAR_IPE_2
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

    pReNative->Core.aVars[idxVar].u.idxRefVar = idxOtherVar;

    /* Update the other variable, ensure it's a stack variable. */
    /** @todo handle variables with const values... that'll go boom now. */
    pReNative->Core.aVars[idxOtherVar].idxReferrerVar = idxVar;
    iemNativeVarSetKindToStack(pReNative, idxOtherVar);
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
static void iemNativeVarSetKindToGstRegRef(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar,
                                           IEMNATIVEGSTREGREF enmRegClass, uint8_t idxReg)
{
    Assert(idxVar < RT_ELEMENTS(pReNative->Core.aVars) && (pReNative->Core.bmVars & RT_BIT_32(idxVar)));

    if (pReNative->Core.aVars[idxVar].enmKind != kIemNativeVarKind_GstRegRef)
    {
        /* Only simple transitions for now. */
        AssertStmt(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Invalid,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));
        pReNative->Core.aVars[idxVar].enmKind = kIemNativeVarKind_GstRegRef;
    }
    AssertStmt(pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_2));

    pReNative->Core.aVars[idxVar].u.GstRegRef.enmClass = enmRegClass;
    pReNative->Core.aVars[idxVar].u.GstRegRef.idx      = idxReg;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeArgAlloc(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t cbType)
{
    return iemNativeArgAllocInt(pReNative, iArgNo, cbType);
}


DECL_HIDDEN_THROW(uint8_t) iemNativeArgAllocConst(PIEMRECOMPILERSTATE pReNative, uint8_t iArgNo, uint8_t cbType, uint64_t uValue)
{
    uint8_t const idxVar = iemNativeArgAllocInt(pReNative, iArgNo, cbType);

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
    AssertStmt(   idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars)
               && (pReNative->Core.bmVars & RT_BIT_32(idxOtherVar))
               && pReNative->Core.aVars[idxOtherVar].uArgNo == UINT8_MAX,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_1));

    uint8_t const idxArgVar = iemNativeArgAlloc(pReNative, iArgNo, sizeof(uintptr_t));
    iemNativeVarSetKindToLocalRef(pReNative, idxArgVar, idxOtherVar);
    return idxArgVar;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeVarAlloc(PIEMRECOMPILERSTATE pReNative, uint8_t cbType)
{
    uint8_t const idxVar = iemNativeVarAllocInt(pReNative, cbType);
    /* Don't set to stack now, leave that to the first use as for instance
       IEM_MC_CALC_RM_EFF_ADDR may produce a const/immediate result (esp. in DOS). */
    return idxVar;
}


DECL_HIDDEN_THROW(uint8_t) iemNativeVarAllocConst(PIEMRECOMPILERSTATE pReNative, uint8_t cbType, uint64_t uValue)
{
    uint8_t const idxVar = iemNativeVarAllocInt(pReNative, cbType);

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
    Assert(pReNative->Core.aVars[idxVar].cbVar <= 8);
    Assert(!pReNative->Core.aVars[idxVar].fRegAcquired);

    uint8_t idxReg = pReNative->Core.aVars[idxVar].idxReg;
    if (idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        Assert(   pReNative->Core.aVars[idxVar].enmKind > kIemNativeVarKind_Invalid
               && pReNative->Core.aVars[idxVar].enmKind < kIemNativeVarKind_End);
        pReNative->Core.aVars[idxVar].fRegAcquired = true;
        return idxReg;
    }

    /*
     * If the kind of variable has not yet been set, default to 'stack'.
     */
    Assert(   pReNative->Core.aVars[idxVar].enmKind >= kIemNativeVarKind_Invalid
           && pReNative->Core.aVars[idxVar].enmKind < kIemNativeVarKind_End);
    if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Invalid)
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

    uint8_t const uArgNo = pReNative->Core.aVars[idxVar].uArgNo;
    if (   uArgNo < RT_ELEMENTS(g_aidxIemNativeCallRegs)
        && !(pReNative->Core.bmHstRegs & RT_BIT_32(g_aidxIemNativeCallRegs[uArgNo])))
    {
        idxReg = g_aidxIemNativeCallRegs[uArgNo];
        iemNativeRegClearGstRegShadowing(pReNative, idxReg, *poff);
        Log11(("iemNativeVarRegisterAcquire: idxVar=%u idxReg=%u (matching arg %u)\n", idxVar, idxReg, uArgNo));
    }
    else if (   idxRegPref >= RT_ELEMENTS(pReNative->Core.aHstRegs)
             || (pReNative->Core.bmHstRegs & RT_BIT_32(idxRegPref)))
    {
        uint32_t const fNotArgsMask = ~g_afIemNativeCallRegs[RT_MIN(pReNative->cArgs, IEMNATIVE_CALL_ARG_GREG_COUNT)];
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
            Log11(("iemNativeVarRegisterAcquire: idxVar=%u idxReg=%u (uArgNo=%u)\n", idxVar, idxReg, uArgNo));
        }
        else
        {
            idxReg = iemNativeRegAllocFindFree(pReNative, poff, false /*fPreferVolatile*/,
                                               IEMNATIVE_HST_GREG_MASK & ~IEMNATIVE_REG_FIXED_MASK & fNotArgsMask);
            AssertStmt(idxReg != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_REG_ALLOCATOR_NO_FREE_VAR));
            Log11(("iemNativeVarRegisterAcquire: idxVar=%u idxReg=%u (slow, uArgNo=%u)\n", idxVar, idxReg, uArgNo));
        }
    }
    else
    {
        idxReg = idxRegPref;
        iemNativeRegClearGstRegShadowing(pReNative, idxReg, *poff);
        Log11(("iemNativeVarRegisterAcquire: idxVar=%u idxReg=%u (preferred)\n", idxVar, idxReg));
    }
    iemNativeRegMarkAllocated(pReNative, idxReg, kIemNativeWhat_Var, idxVar);
    pReNative->Core.aVars[idxVar].idxReg = idxReg;

    /*
     * Load it off the stack if we've got a stack slot.
     */
    uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
    if (idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS)
    {
        Assert(fInitialized);
        int32_t const offDispBp = iemNativeStackCalcBpDisp(idxStackSlot);
        switch (pReNative->Core.aVars[idxVar].cbVar)
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
    pReNative->Core.aVars[idxVar].fRegAcquired = true;
    return idxReg;
}


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
    Assert(!pReNative->Core.aVars[idxVar].fRegAcquired);
    AssertMsgStmt(   pReNative->Core.aVars[idxVar].cbVar <= 8
                  && (   pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Immediate
                      || pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack),
                  ("idxVar=%d cbVar=%d enmKind=%d enmGstReg=%s\n", idxVar, pReNative->Core.aVars[idxVar].cbVar,
                   pReNative->Core.aVars[idxVar].enmKind, g_aGstShadowInfo[enmGstReg].pszName),
                  IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_6));

    /*
     * This shouldn't ever be used for arguments, unless it's in a weird else
     * branch that doesn't do any calling and even then it's questionable.
     *
     * However, in case someone writes crazy wrong MC code and does register
     * updates before making calls, just use the regular register allocator to
     * ensure we get a register suitable for the intended argument number.
     */
    AssertStmt(pReNative->Core.aVars[idxVar].uArgNo == UINT8_MAX, iemNativeVarRegisterAcquire(pReNative, idxVar, poff));

    /*
     * If there is already a register for the variable, we transfer/set the
     * guest shadow copy assignment to it.
     */
    uint8_t idxReg = pReNative->Core.aVars[idxVar].idxReg;
    if (idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
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
        pReNative->Core.aVars[idxVar].enmGstReg    = enmGstReg;
        pReNative->Core.aVars[idxVar].fRegAcquired = true;
        return idxReg;
    }
    Assert(pReNative->Core.aVars[idxVar].uArgNo == UINT8_MAX);

    /*
     * Because this is supposed to be the commit stage, we're just tag along with the
     * temporary register allocator and upgrade it to a variable register.
     */
    idxReg = iemNativeRegAllocTmpForGuestReg(pReNative, poff, enmGstReg, kIemNativeGstRegUse_ForFullWrite);
    Assert(pReNative->Core.aHstRegs[idxReg].enmWhat == kIemNativeWhat_Tmp);
    Assert(pReNative->Core.aHstRegs[idxReg].idxVar  == UINT8_MAX);
    pReNative->Core.aHstRegs[idxReg].enmWhat = kIemNativeWhat_Var;
    pReNative->Core.aHstRegs[idxReg].idxVar  = idxVar;
    pReNative->Core.aVars[idxVar].idxReg     = idxReg;

    /*
     * Now we need to load the register value.
     */
    if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Immediate)
        *poff = iemNativeEmitLoadGprImm64(pReNative, *poff, idxReg, pReNative->Core.aVars[idxVar].u.uValue);
    else
    {
        uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
        int32_t const offDispBp    = iemNativeStackCalcBpDisp(idxStackSlot);
        switch (pReNative->Core.aVars[idxVar].cbVar)
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

    pReNative->Core.aVars[idxVar].fRegAcquired = true;
    return idxReg;
}


/**
 * Sets the host register for @a idxVarRc to @a idxReg.
 *
 * The register must not be allocated. Any guest register shadowing will be
 * implictly dropped by this call.
 *
 * The variable must not have any register associated with it (causes
 * VERR_IEM_VAR_IPE_10 to be raised).  Conversion to a stack variable is
 * implied.
 *
 * @returns idxReg
 * @param   pReNative   The recompiler state.
 * @param   idxVar      The variable.
 * @param   idxReg      The host register (typically IEMNATIVE_CALL_RET_GREG).
 * @param   off         For recording in debug info.
 *
 * @throws  VERR_IEM_VAR_IPE_10, VERR_IEM_VAR_IPE_11
 */
DECL_INLINE_THROW(uint8_t) iemNativeVarRegisterSet(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar, uint8_t idxReg, uint32_t off)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(!pReNative->Core.aVars[idxVar].fRegAcquired);
    Assert(idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs));
    AssertStmt(pReNative->Core.aVars[idxVar].idxReg == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_10));
    AssertStmt(!(pReNative->Core.bmHstRegs & RT_BIT_32(idxReg)), IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_11));

    iemNativeRegClearGstRegShadowing(pReNative, idxReg, off);
    iemNativeRegMarkAllocated(pReNative, idxReg, kIemNativeWhat_Var, idxVar);

    iemNativeVarSetKindToStack(pReNative, idxVar);
    pReNative->Core.aVars[idxVar].idxReg = idxReg;

    return idxReg;
}


/**
 * A convenient helper function.
 */
DECL_INLINE_THROW(uint8_t) iemNativeVarRegisterSetAndAcquire(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar,
                                                             uint8_t idxReg, uint32_t *poff)
{
    idxReg = iemNativeVarRegisterSet(pReNative, idxVar, idxReg, *poff);
    pReNative->Core.aVars[idxVar].fRegAcquired = true;
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
                AssertStmt(   idxVar < RT_ELEMENTS(pReNative->Core.aVars)
                           && (pReNative->Core.bmVars & RT_BIT_32(idxVar))
                           && pReNative->Core.aVars[idxVar].idxReg == idxHstReg,
                           IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_12));
                switch (pReNative->Core.aVars[idxVar].enmKind)
                {
                    case kIemNativeVarKind_Stack:
                    {
                        /* Temporarily spill the variable register. */
                        uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                        Log12(("iemNativeVarSaveVolatileRegsPreHlpCall: spilling idxVar=%d/idxReg=%d onto the stack (slot %#x bp+%d, off=%#x)\n",
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
                AssertStmt(   idxVar < RT_ELEMENTS(pReNative->Core.aVars)
                           && (pReNative->Core.bmVars & RT_BIT_32(idxVar))
                           && pReNative->Core.aVars[idxVar].idxReg == idxHstReg,
                           IEMNATIVE_DO_LONGJMP(pReNative,  VERR_IEM_VAR_IPE_12));
                switch (pReNative->Core.aVars[idxVar].enmKind)
                {
                    case kIemNativeVarKind_Stack:
                    {
                        /* Unspill the variable register. */
                        uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                        Log12(("iemNativeVarRestoreVolatileRegsPostHlpCall: unspilling idxVar=%d/idxReg=%d (slot %#x bp+%d, off=%#x)\n",
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
    return off;
}


/**
 * Worker that frees the stack slots for variable @a idxVar if any allocated.
 *
 * This is used both by iemNativeVarFreeOneWorker and iemNativeEmitCallCommon.
 */
DECL_FORCE_INLINE(void) iemNativeVarFreeStackSlots(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
    if (idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS)
    {
        uint8_t const  cbVar      = pReNative->Core.aVars[idxVar].cbVar;
        uint8_t const  cSlots     = (cbVar + sizeof(uint64_t) - 1) / sizeof(uint64_t);
        uint32_t const fAllocMask = (uint32_t)(RT_BIT_32(cSlots) - 1U);
        Assert(cSlots > 0);
        Assert(((pReNative->Core.bmStack >> idxStackSlot) & fAllocMask) == fAllocMask);
        Log11(("iemNativeVarFreeStackSlots: idxVar=%d iSlot=%#x/%#x (cbVar=%#x)\n", idxVar, idxStackSlot, fAllocMask, cbVar));
        pReNative->Core.bmStack &= ~(fAllocMask << idxStackSlot);
        pReNative->Core.aVars[idxVar].idxStackSlot = UINT8_MAX;
    }
    else
        Assert(idxStackSlot == UINT8_MAX);
}


/**
 * Worker that frees a single variable.
 *
 * ASSUMES that @a idxVar is valid.
 */
DECLINLINE(void) iemNativeVarFreeOneWorker(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    Assert(   pReNative->Core.aVars[idxVar].enmKind >= kIemNativeVarKind_Invalid  /* Including invalid as we may have unused */
           && pReNative->Core.aVars[idxVar].enmKind <  kIemNativeVarKind_End);    /* variables in conditional branches. */
    Assert(!pReNative->Core.aVars[idxVar].fRegAcquired);

    /* Free the host register first if any assigned. */
    uint8_t const idxHstReg = pReNative->Core.aVars[idxVar].idxReg;
    if (idxHstReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        Assert(pReNative->Core.aHstRegs[idxHstReg].idxVar == idxVar);
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
DECLINLINE(void) iemNativeVarFreeAllSlow(PIEMRECOMPILERSTATE pReNative, uint32_t bmVars)
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
            Assert(pReNative->Core.aHstRegs[idxHstReg].idxVar == idxVar);
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


/**
 * This is called by IEM_MC_END() to clean up all variables.
 */
DECL_FORCE_INLINE(void) iemNativeVarFreeAll(PIEMRECOMPILERSTATE pReNative)
{
    uint32_t const bmVars = pReNative->Core.bmVars;
    if (bmVars != 0)
        iemNativeVarFreeAllSlow(pReNative, bmVars);
    Assert(pReNative->Core.u64ArgVars == UINT64_MAX);
    Assert(pReNative->Core.bmStack    == 0);
}


#define IEM_MC_FREE_LOCAL(a_Name)   iemNativeVarFreeLocal(pReNative, a_Name)

/**
 * This is called by IEM_MC_FREE_LOCAL.
 */
DECLINLINE(void) iemNativeVarFreeLocal(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[idxVar].uArgNo == UINT8_MAX);
    iemNativeVarFreeOneWorker(pReNative, idxVar);
}


#define IEM_MC_FREE_ARG(a_Name)     iemNativeVarFreeArg(pReNative, a_Name)

/**
 * This is called by IEM_MC_FREE_ARG.
 */
DECLINLINE(void) iemNativeVarFreeArg(PIEMRECOMPILERSTATE pReNative, uint8_t idxVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVar);
    Assert(pReNative->Core.aVars[idxVar].uArgNo < RT_ELEMENTS(pReNative->Core.aidxArgVars));
    iemNativeVarFreeOneWorker(pReNative, idxVar);
}


#define IEM_MC_ASSIGN_TO_SMALLER(a_VarDst, a_VarSrcEol) off = iemNativeVarAssignToSmaller(pReNative, off, a_VarDst, a_VarSrcEol)

/**
 * This is called by IEM_MC_ASSIGN_TO_SMALLER.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeVarAssignToSmaller(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarDst, uint8_t idxVarSrc)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarDst);
    AssertStmt(pReNative->Core.aVars[idxVarDst].enmKind == kIemNativeVarKind_Invalid,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));
    Assert(   pReNative->Core.aVars[idxVarDst].cbVar == sizeof(uint16_t)
           || pReNative->Core.aVars[idxVarDst].cbVar == sizeof(uint32_t));

    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarSrc);
    AssertStmt(   pReNative->Core.aVars[idxVarSrc].enmKind == kIemNativeVarKind_Stack
               || pReNative->Core.aVars[idxVarSrc].enmKind == kIemNativeVarKind_Immediate,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    Assert(pReNative->Core.aVars[idxVarDst].cbVar < pReNative->Core.aVars[idxVarSrc].cbVar);

    /*
     * Special case for immediates.
     */
    if (pReNative->Core.aVars[idxVarSrc].enmKind == kIemNativeVarKind_Immediate)
    {
        switch (pReNative->Core.aVars[idxVarDst].cbVar)
        {
            case sizeof(uint16_t):
                iemNativeVarSetKindToConst(pReNative, idxVarDst, (uint16_t)pReNative->Core.aVars[idxVarSrc].u.uValue);
                break;
            case sizeof(uint32_t):
                iemNativeVarSetKindToConst(pReNative, idxVarDst, (uint32_t)pReNative->Core.aVars[idxVarSrc].u.uValue);
                break;
            default: AssertFailed(); break;
        }
    }
    else
    {
        /*
         * The generic solution for now.
         */
        /** @todo optimize this by having the python script make sure the source
         *        variable passed to IEM_MC_ASSIGN_TO_SMALLER is not used after the
         *        statement.   Then we could just transfer the register assignments. */
        uint8_t const idxRegDst = iemNativeVarRegisterAcquire(pReNative, idxVarDst, &off);
        uint8_t const idxRegSrc = iemNativeVarRegisterAcquire(pReNative, idxVarSrc, &off);
        switch (pReNative->Core.aVars[idxVarDst].cbVar)
        {
            case sizeof(uint16_t):
                off = iemNativeEmitLoadGprFromGpr16(pReNative, off, idxRegDst, idxRegSrc);
                break;
            case sizeof(uint32_t):
                off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxRegDst, idxRegSrc);
                break;
            default: AssertFailed(); break;
        }
        iemNativeVarRegisterRelease(pReNative, idxVarSrc);
        iemNativeVarRegisterRelease(pReNative, idxVarDst);
    }
    return off;
}



/*********************************************************************************************************************************
*   Emitters for IEM_MC_CALL_CIMPL_XXX                                                                                           *
*********************************************************************************************************************************/

/**
 * Emits code to load a reference to the given guest register into @a idxGprDst.
  */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitLeaGprByGstRegRef(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxGprDst,
                               IEMNATIVEGSTREGREF enmClass, uint8_t idxRegInClass)
{
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
 */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCallCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cArgs, uint8_t cHiddenArgs)
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
                if (idxRegOld < RT_ELEMENTS(pReNative->Core.aHstRegs))
                {
                    uint8_t const idxStackSlot = iemNativeVarGetStackSlot(pReNative, idxVar);
                    Log12(("iemNativeEmitCallCommon: spilling idxVar=%d/idxReg=%d (referred to by %d) onto the stack (slot %#x bp+%d, off=%#x)\n",
                           idxVar, idxRegOld, pReNative->Core.aVars[idxVar].idxReferrerVar,
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
                    uint8_t const idxVar = pReNative->Core.aHstRegs[idxArgReg].idxVar;
                    Assert(idxVar < RT_ELEMENTS(pReNative->Core.aVars));
                    Assert(pReNative->Core.aVars[idxVar].idxReg == idxArgReg);
                    uint8_t const uArgNo = pReNative->Core.aVars[idxVar].uArgNo;
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

                        if (pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack)
                            off = iemNativeRegMoveOrSpillStackVar(pReNative, off, idxVar);
                        else
                        {
                            /* just free it, can be reloaded if used again */
                            pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
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
            uint8_t const idxVar    = pReNative->Core.aidxArgVars[i];
            int32_t const offBpDisp = g_aoffIemNativeCallStackArgBpDisp[i - IEMNATIVE_CALL_ARG_GREG_COUNT];
            if (pReNative->Core.aVars[idxVar].idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
            {
                Assert(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack); /* Imm as well? */
                off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, pReNative->Core.aVars[idxVar].idxReg);
                pReNative->Core.bmHstRegs &= ~RT_BIT_32(pReNative->Core.aVars[idxVar].idxReg);
                pReNative->Core.aVars[idxVar].idxReg = UINT8_MAX;
            }
            else
            {
                /* Use ARG0 as temp for stuff we need registers for. */
                switch (pReNative->Core.aVars[idxVar].enmKind)
                {
                    case kIemNativeVarKind_Stack:
                    {
                        uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
                        AssertStmt(idxStackSlot != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
                        off = iemNativeEmitLoadGprByBp(pReNative, off, IEMNATIVE_CALL_ARG0_GREG /* is free */,
                                                       iemNativeStackCalcBpDisp(idxStackSlot));
                        off = iemNativeEmitStoreGprByBp(pReNative, off, offBpDisp, IEMNATIVE_CALL_ARG0_GREG);
                        continue;
                    }

                    case kIemNativeVarKind_Immediate:
                        off = iemNativeEmitStoreImm64ByBp(pReNative, off, offBpDisp, pReNative->Core.aVars[idxVar].u.uValue);
                        continue;

                    case kIemNativeVarKind_VarRef:
                    {
                        uint8_t const idxOtherVar    = pReNative->Core.aVars[idxVar].u.idxRefVar;
                        Assert(idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars));
                        uint8_t const idxStackSlot   = iemNativeVarGetStackSlot(pReNative, idxOtherVar);
                        int32_t const offBpDispOther = iemNativeStackCalcBpDisp(idxStackSlot);
                        uint8_t const idxRegOther    = pReNative->Core.aVars[idxOtherVar].idxReg;
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
                                                             pReNative->Core.aVars[idxVar].u.GstRegRef.enmClass,
                                                             pReNative->Core.aVars[idxVar].u.GstRegRef.idx);
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
                Assert(   pReNative->Core.aHstRegs[idxArgReg].idxVar == pReNative->Core.aidxArgVars[i]
                       && pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]].uArgNo == i
                       && pReNative->Core.aVars[pReNative->Core.aidxArgVars[i]].idxReg == idxArgReg);
            else
            {
                uint8_t const idxVar = pReNative->Core.aidxArgVars[i];
                if (pReNative->Core.aVars[idxVar].idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
                {
                    Assert(pReNative->Core.aVars[idxVar].enmKind == kIemNativeVarKind_Stack);
                    off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxArgReg, pReNative->Core.aVars[idxVar].idxReg);
                    pReNative->Core.bmHstRegs = (pReNative->Core.bmHstRegs & ~RT_BIT_32(pReNative->Core.aVars[idxVar].idxReg))
                                              | RT_BIT_32(idxArgReg);
                    pReNative->Core.aVars[idxVar].idxReg = idxArgReg;
                }
                else
                {
                    /* Use ARG0 as temp for stuff we need registers for. */
                    switch (pReNative->Core.aVars[idxVar].enmKind)
                    {
                        case kIemNativeVarKind_Stack:
                        {
                            uint8_t const idxStackSlot = pReNative->Core.aVars[idxVar].idxStackSlot;
                            AssertStmt(idxStackSlot != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
                            off = iemNativeEmitLoadGprByBp(pReNative, off, idxArgReg, iemNativeStackCalcBpDisp(idxStackSlot));
                            continue;
                        }

                        case kIemNativeVarKind_Immediate:
                            off = iemNativeEmitLoadGprImm64(pReNative, off, idxArgReg, pReNative->Core.aVars[idxVar].u.uValue);
                            continue;

                        case kIemNativeVarKind_VarRef:
                        {
                            uint8_t const idxOtherVar    = pReNative->Core.aVars[idxVar].u.idxRefVar;
                            Assert(idxOtherVar < RT_ELEMENTS(pReNative->Core.aVars));
                            uint8_t const idxStackSlot   = iemNativeVarGetStackSlot(pReNative, idxOtherVar);
                            int32_t const offBpDispOther = iemNativeStackCalcBpDisp(idxStackSlot);
                            uint8_t const idxRegOther    = pReNative->Core.aVars[idxOtherVar].idxReg;
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
                                                                 pReNative->Core.aVars[idxVar].u.GstRegRef.enmClass,
                                                                 pReNative->Core.aVars[idxVar].u.GstRegRef.idx);
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
        uint8_t const idxVar = pReNative->Core.aidxArgVars[i];
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


/** Common emit function for IEM_MC_CALL_CIMPL_XXXX. */
DECL_HIDDEN_THROW(uint32_t)
iemNativeEmitCallCImplCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr, uint8_t idxInstr,
                             uint64_t fGstShwFlush, uintptr_t pfnCImpl, uint8_t cArgs)

{
    /*
     * Do all the call setup and cleanup.
     */
    off = iemNativeEmitCallCommon(pReNative, off, cArgs + IEM_CIMPL_HIDDEN_ARGS, IEM_CIMPL_HIDDEN_ARGS);

    /*
     * Load the two or three hidden arguments.
     */
#if defined(VBOXSTRICTRC_STRICT_ENABLED) && defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64)
    off = iemNativeEmitLeaGprByBp(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, cbInstr);
#else
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);
    off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, cbInstr);
#endif

    /*
     * Make the call and check the return code.
     *
     * Shadow PC copies are always flushed here, other stuff depends on flags.
     * Segment and general purpose registers are explictily flushed via the
     * IEM_MC_HINT_FLUSH_GUEST_SHADOW_GREG and IEM_MC_HINT_FLUSH_GUEST_SHADOW_SREG
     * macros.
     */
    off = iemNativeEmitCallImm(pReNative, off, (uintptr_t)pfnCImpl);
#if defined(VBOXSTRICTRC_STRICT_ENABLED) && defined(RT_OS_WINDOWS) && defined(RT_ARCH_AMD64)
    off = iemNativeEmitLoadGprByBpU32(pReNative, off, X86_GREG_xAX, IEMNATIVE_FP_OFF_IN_SHADOW_ARG0); /* rcStrict (see above) */
#endif
    fGstShwFlush = iemNativeCImplFlagsToGuestShadowFlushMask(pReNative->fCImpl, fGstShwFlush | RT_BIT_64(kIemNativeGstReg_Pc));
    if (!(pReNative->fMc & IEM_MC_F_WITHOUT_FLAGS)) /** @todo We don't emit with-flags/without-flags variations for CIMPL calls.  */
        fGstShwFlush |= RT_BIT_64(kIemNativeGstReg_EFlags);
    iemNativeRegFlushGuestShadows(pReNative, fGstShwFlush);

    return iemNativeEmitCheckCallRetAndPassUp(pReNative, off, idxInstr);
}


#define IEM_MC_CALL_CIMPL_1_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0) \
    off = iemNativeEmitCallCImpl1(pReNative, off, a_cbInstr, pCallEntry->idxInstr, a_fGstShwFlush, (uintptr_t)a_pfnCImpl, a0)

/** Emits code for IEM_MC_CALL_CIMPL_1. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallCImpl1(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr, uint8_t idxInstr, uint64_t fGstShwFlush,
                        uintptr_t pfnCImpl, uint8_t idxArg0)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0 + IEM_CIMPL_HIDDEN_ARGS);
    return iemNativeEmitCallCImplCommon(pReNative, off, cbInstr, idxInstr, fGstShwFlush, pfnCImpl, 1);
}


#define IEM_MC_CALL_CIMPL_2_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1) \
    off = iemNativeEmitCallCImpl2(pReNative, off, a_cbInstr, pCallEntry->idxInstr, a_fGstShwFlush, (uintptr_t)a_pfnCImpl, a0, a1)

/** Emits code for IEM_MC_CALL_CIMPL_2. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallCImpl2(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr, uint8_t idxInstr, uint64_t fGstShwFlush,
                        uintptr_t pfnCImpl, uint8_t idxArg0, uint8_t idxArg1)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1 + IEM_CIMPL_HIDDEN_ARGS);
    return iemNativeEmitCallCImplCommon(pReNative, off, cbInstr, idxInstr, fGstShwFlush, pfnCImpl, 2);
}


#define IEM_MC_CALL_CIMPL_3_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2) \
    off = iemNativeEmitCallCImpl3(pReNative, off, a_cbInstr, pCallEntry->idxInstr, a_fGstShwFlush, \
                                  (uintptr_t)a_pfnCImpl, a0, a1, a2)

/** Emits code for IEM_MC_CALL_CIMPL_3. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallCImpl3(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr, uint8_t idxInstr, uint64_t fGstShwFlush,
                        uintptr_t pfnCImpl, uint8_t idxArg0, uint8_t idxArg1, uint8_t idxArg2)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg2, 2 + IEM_CIMPL_HIDDEN_ARGS);
    return iemNativeEmitCallCImplCommon(pReNative, off, cbInstr, idxInstr, fGstShwFlush, pfnCImpl, 3);
}


#define IEM_MC_CALL_CIMPL_4_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2, a3) \
    off = iemNativeEmitCallCImpl4(pReNative, off, a_cbInstr, pCallEntry->idxInstr, a_fGstShwFlush, \
                                  (uintptr_t)a_pfnCImpl, a0, a1, a2, a3)

/** Emits code for IEM_MC_CALL_CIMPL_4. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallCImpl4(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr, uint8_t idxInstr, uint64_t fGstShwFlush,
                        uintptr_t pfnCImpl, uint8_t idxArg0, uint8_t idxArg1, uint8_t idxArg2, uint8_t idxArg3)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg2, 2 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg3, 3 + IEM_CIMPL_HIDDEN_ARGS);
    return iemNativeEmitCallCImplCommon(pReNative, off, cbInstr, idxInstr, fGstShwFlush, pfnCImpl, 4);
}


#define IEM_MC_CALL_CIMPL_5_THREADED(a_cbInstr, a_fFlags, a_fGstShwFlush, a_pfnCImpl, a0, a1, a2, a3, a4) \
    off = iemNativeEmitCallCImpl5(pReNative, off, a_cbInstr, pCallEntry->idxInstr, a_fGstShwFlush, \
                                  (uintptr_t)a_pfnCImpl, a0, a1, a2, a3, a4)

/** Emits code for IEM_MC_CALL_CIMPL_4. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallCImpl5(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t cbInstr, uint8_t idxInstr, uint64_t fGstShwFlush,
                        uintptr_t pfnCImpl, uint8_t idxArg0, uint8_t idxArg1, uint8_t idxArg2, uint8_t idxArg3, uint8_t idxArg4)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg2, 2 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg3, 3 + IEM_CIMPL_HIDDEN_ARGS);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg4, 4 + IEM_CIMPL_HIDDEN_ARGS);
    return iemNativeEmitCallCImplCommon(pReNative, off, cbInstr, idxInstr, fGstShwFlush, pfnCImpl, 5);
}


/** Recompiler debugging: Flush guest register shadow copies. */
#define IEM_MC_HINT_FLUSH_GUEST_SHADOW(g_fGstShwFlush) iemNativeRegFlushGuestShadows(pReNative, g_fGstShwFlush)



/*********************************************************************************************************************************
*   Emitters for IEM_MC_CALL_VOID_AIMPL_XXX and IEM_MC_CALL_AIMPL_XXX                                                            *
*********************************************************************************************************************************/

/**
 * Common worker for IEM_MC_CALL_VOID_AIMPL_XXX and IEM_MC_CALL_AIMPL_XXX.
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallAImplCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRc,
                             uintptr_t pfnAImpl, uint8_t cArgs)
{
    if (idxVarRc != UINT8_MAX)
    {
        IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRc);
        AssertStmt(pReNative->Core.aVars[idxVarRc].uArgNo == UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_8));
        AssertStmt(pReNative->Core.aVars[idxVarRc].cbVar <= sizeof(uint64_t), IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_IPE_9));
    }

    /*
     * Do all the call setup and cleanup.
     */
    off = iemNativeEmitCallCommon(pReNative, off, cArgs, 0 /*cHiddenArgs*/);

    /*
     * Make the call and update the return code variable if we've got one.
     */
    off = iemNativeEmitCallImm(pReNative, off, pfnAImpl);
    if (idxVarRc < RT_ELEMENTS(pReNative->Core.aVars))
    {
pReNative->pInstrBuf[off++] = 0xcc; /** @todo test IEM_MC_CALL_AIMPL_3 and IEM_MC_CALL_AIMPL_4 return codes. */
        iemNativeVarRegisterSet(pReNative, idxVarRc, IEMNATIVE_CALL_RET_GREG, off);
    }

    return off;
}



#define IEM_MC_CALL_VOID_AIMPL_0(a_pfn) \
    off = iemNativeEmitCallAImpl0(pReNative, off, UINT8_MAX /*idxVarRc*/, (uintptr_t)(a_pfn))

#define IEM_MC_CALL_AIMPL_0(a_rc, a_pfn) \
    off = iemNativeEmitCallAImpl0(pReNative, off, a_rc,                   (uintptr_t)(a_pfn))

/** Emits code for IEM_MC_CALL_VOID_AIMPL_0 and IEM_MC_CALL_AIMPL_0. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallAImpl0(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRc, uintptr_t pfnAImpl)
{
    return iemNativeEmitCallAImplCommon(pReNative, off, idxVarRc, pfnAImpl, 0);
}


#define IEM_MC_CALL_VOID_AIMPL_1(a_pfn, a0) \
    off = iemNativeEmitCallAImpl1(pReNative, off, UINT8_MAX /*idxVarRc*/, (uintptr_t)(a_pfn), a0)

#define IEM_MC_CALL_AIMPL_1(a_rc, a_pfn, a0) \
    off = iemNativeEmitCallAImpl1(pReNative, off, a_rc,                   (uintptr_t)(a_pfn), a0)

/** Emits code for IEM_MC_CALL_VOID_AIMPL_1 and IEM_MC_CALL_AIMPL_1. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallAImpl1(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRc, uintptr_t pfnAImpl, uint8_t idxArg0)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0);
    return iemNativeEmitCallAImplCommon(pReNative, off, idxVarRc, pfnAImpl, 1);
}


#define IEM_MC_CALL_VOID_AIMPL_2(a_pfn, a0, a1) \
    off = iemNativeEmitCallAImpl2(pReNative, off, UINT8_MAX /*idxVarRc*/, (uintptr_t)(a_pfn), a0, a1)

#define IEM_MC_CALL_AIMPL_2(a_rc, a_pfn, a0, a1) \
    off = iemNativeEmitCallAImpl2(pReNative, off, a_rc,                   (uintptr_t)(a_pfn), a0, a1)

/** Emits code for IEM_MC_CALL_VOID_AIMPL_2 and IEM_MC_CALL_AIMPL_2. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallAImpl2(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRc,
                        uintptr_t pfnAImpl, uint8_t idxArg0, uint8_t idxArg1)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1);
    return iemNativeEmitCallAImplCommon(pReNative, off, idxVarRc, pfnAImpl, 2);
}


#define IEM_MC_CALL_VOID_AIMPL_3(a_pfn, a0, a1, a2) \
    off = iemNativeEmitCallAImpl3(pReNative, off, UINT8_MAX /*idxVarRc*/, (uintptr_t)(a_pfn), a0, a1, a2)

#define IEM_MC_CALL_AIMPL_3(a_rc, a_pfn, a0, a1, a2) \
    off = iemNativeEmitCallAImpl3(pReNative, off, a_rc,                   (uintptr_t)(a_pfn), a0, a1, a2)

/** Emits code for IEM_MC_CALL_VOID_AIMPL_3 and IEM_MC_CALL_AIMPL_3. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallAImpl3(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRc,
                        uintptr_t pfnAImpl, uint8_t idxArg0, uint8_t idxArg1, uint8_t idxArg2)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg2, 2);
    return iemNativeEmitCallAImplCommon(pReNative, off, idxVarRc, pfnAImpl, 3);
}


#define IEM_MC_CALL_VOID_AIMPL_4(a_pfn, a0, a1, a2, a3) \
    off = iemNativeEmitCallAImpl4(pReNative, off, UINT8_MAX /*idxVarRc*/, (uintptr_t)(a_pfn), a0, a1, a2, a3)

#define IEM_MC_CALL_AIMPL_4(a_rc, a_pfn, a0, a1, a2, a3) \
    off = iemNativeEmitCallAImpl4(pReNative, off, a_rc,                   (uintptr_t)(a_pfn), a0, a1, a2, a3)

/** Emits code for IEM_MC_CALL_VOID_AIMPL_4 and IEM_MC_CALL_AIMPL_4. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCallAImpl4(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRc,
                        uintptr_t pfnAImpl, uint8_t idxArg0, uint8_t idxArg1, uint8_t idxArg2, uint8_t idxArg3)
{
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg0, 0);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg1, 1);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg2, 2);
    IEMNATIVE_ASSERT_ARG_VAR_IDX(pReNative, idxArg3, 3);
    return iemNativeEmitCallAImplCommon(pReNative, off, idxVarRc, pfnAImpl, 4);
}



/*********************************************************************************************************************************
*   Emitters for general purpose register fetches (IEM_MC_FETCH_GREG_XXX).                                                       *
*********************************************************************************************************************************/

#define IEM_MC_FETCH_GREG_U8_THREADED(a_u8Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8(pReNative, off, a_u8Dst,  a_iGRegEx, sizeof(uint8_t) /*cbZeroExtended*/)

#define IEM_MC_FETCH_GREG_U8_ZX_U16_THREADED(a_u16Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8(pReNative, off, a_u16Dst, a_iGRegEx, sizeof(uint16_t) /*cbZeroExtended*/)

#define IEM_MC_FETCH_GREG_U8_ZX_U32_THREADED(a_u32Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8(pReNative, off, a_u32Dst, a_iGRegEx, sizeof(uint32_t) /*cbZeroExtended*/)

#define IEM_MC_FETCH_GREG_U8_ZX_U64_THREADED(a_u64Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8(pReNative, off, a_u64Dst, a_iGRegEx, sizeof(uint64_t) /*cbZeroExtended*/)


/** Emits code for IEM_MC_FETCH_GREG_U8_THREADED and
 *  IEM_MC_FETCH_GREG_U8_ZX_U16/32/64_THREADED. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGRegEx, int8_t cbZeroExtended)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == cbZeroExtended); RT_NOREF(cbZeroExtended);
    Assert(iGRegEx < 20);

    /* Same discussion as in iemNativeEmitFetchGregU16 */
    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGRegEx & 15),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);

    /* The value is zero-extended to the full 64-bit host register width. */
    if (iGRegEx < 16)
        off = iemNativeEmitLoadGprFromGpr8(pReNative, off, idxVarReg, idxGstFullReg);
    else
        off = iemNativeEmitLoadGprFromGpr8Hi(pReNative, off, idxVarReg, idxGstFullReg);

    iemNativeVarRegisterRelease(pReNative, idxDstVar);
    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}


#define IEM_MC_FETCH_GREG_U8_SX_U16_THREADED(a_u16Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8Sx(pReNative, off, a_u16Dst, a_iGRegEx, sizeof(uint16_t))

#define IEM_MC_FETCH_GREG_U8_SX_U32_THREADED(a_u32Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8Sx(pReNative, off, a_u32Dst, a_iGRegEx, sizeof(uint32_t))

#define IEM_MC_FETCH_GREG_U8_SX_U64_THREADED(a_u64Dst, a_iGRegEx) \
    off = iemNativeEmitFetchGregU8Sx(pReNative, off, a_u64Dst, a_iGRegEx, sizeof(uint64_t))

/** Emits code for IEM_MC_FETCH_GREG_U8_SX_U16/32/64_THREADED. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU8Sx(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGRegEx, uint8_t cbSignExtended)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == cbSignExtended);
    Assert(iGRegEx < 20);

    /* Same discussion as in iemNativeEmitFetchGregU16 */
    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGRegEx & 15),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);

    if (iGRegEx < 16)
    {
        switch (cbSignExtended)
        {
            case sizeof(uint16_t):
                off = iemNativeEmitLoadGpr16SignExtendedFromGpr8(pReNative, off, idxVarReg, idxGstFullReg);
                break;
            case sizeof(uint32_t):
                off = iemNativeEmitLoadGpr32SignExtendedFromGpr8(pReNative, off, idxVarReg, idxGstFullReg);
                break;
            case sizeof(uint64_t):
                off = iemNativeEmitLoadGprSignExtendedFromGpr8(pReNative, off, idxVarReg, idxGstFullReg);
                break;
            default: AssertFailed(); break;
        }
    }
    else
    {
        off = iemNativeEmitLoadGprFromGpr8Hi(pReNative, off, idxVarReg, idxGstFullReg);
        switch (cbSignExtended)
        {
            case sizeof(uint16_t):
                off = iemNativeEmitLoadGpr16SignExtendedFromGpr8(pReNative, off, idxVarReg, idxVarReg);
                break;
            case sizeof(uint32_t):
                off = iemNativeEmitLoadGpr32SignExtendedFromGpr8(pReNative, off, idxVarReg, idxVarReg);
                break;
            case sizeof(uint64_t):
                off = iemNativeEmitLoadGprSignExtendedFromGpr8(pReNative, off, idxVarReg, idxVarReg);
                break;
            default: AssertFailed(); break;
        }
    }

    iemNativeVarRegisterRelease(pReNative, idxDstVar);
    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}



#define IEM_MC_FETCH_GREG_U16(a_u16Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU16(pReNative, off, a_u16Dst, a_iGReg, sizeof(uint16_t))

#define IEM_MC_FETCH_GREG_U16_ZX_U32(a_u16Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU16(pReNative, off, a_u16Dst, a_iGReg, sizeof(uint32_t))

#define IEM_MC_FETCH_GREG_U16_ZX_U64(a_u16Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU16(pReNative, off, a_u16Dst, a_iGReg, sizeof(uint64_t))

/** Emits code for IEM_MC_FETCH_GREG_U16 and IEM_MC_FETCH_GREG_U16_ZX_U32/64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGReg, uint8_t cbZeroExtended)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == cbZeroExtended); RT_NOREF(cbZeroExtended);
    Assert(iGReg < 16);

    /*
     * We can either just load the low 16-bit of the GPR into a host register
     * for the variable, or we can do so via a shadow copy host register. The
     * latter will avoid having to reload it if it's being stored later, but
     * will waste a host register if it isn't touched again.  Since we don't
     * know what going to happen, we choose the latter for now.
     */
    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);
    off = iemNativeEmitLoadGprFromGpr16(pReNative, off, idxVarReg, idxGstFullReg);
    iemNativeVarRegisterRelease(pReNative, idxDstVar);

    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}


#define IEM_MC_FETCH_GREG_U16_SX_U32(a_u16Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU16Sx(pReNative, off, a_u16Dst, a_iGReg, sizeof(uint32_t))

#define IEM_MC_FETCH_GREG_U16_SX_U64(a_u16Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU16Sx(pReNative, off, a_u16Dst, a_iGReg, sizeof(uint64_t))

/** Emits code for IEM_MC_FETCH_GREG_U16_SX_U32/64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU16Sx(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGReg, uint8_t cbSignExtended)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == cbSignExtended);
    Assert(iGReg < 16);

    /*
     * We can either just load the low 16-bit of the GPR into a host register
     * for the variable, or we can do so via a shadow copy host register. The
     * latter will avoid having to reload it if it's being stored later, but
     * will waste a host register if it isn't touched again.  Since we don't
     * know what going to happen, we choose the latter for now.
     */
    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);
    if (cbSignExtended == sizeof(uint32_t))
        off = iemNativeEmitLoadGpr32SignExtendedFromGpr16(pReNative, off, idxVarReg, idxGstFullReg);
    else
    {
        Assert(cbSignExtended == sizeof(uint64_t));
        off = iemNativeEmitLoadGprSignExtendedFromGpr16(pReNative, off, idxVarReg, idxGstFullReg);
    }
    iemNativeVarRegisterRelease(pReNative, idxDstVar);

    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}


#define IEM_MC_FETCH_GREG_U32(a_u32Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU32(pReNative, off, a_u32Dst, a_iGReg, sizeof(uint32_t))

#define IEM_MC_FETCH_GREG_U32_ZX_U64(a_u32Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU32(pReNative, off, a_u32Dst, a_iGReg, sizeof(uint64_t))

/** Emits code for IEM_MC_FETCH_GREG_U32. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGReg, uint8_t cbZeroExtended)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == cbZeroExtended); RT_NOREF_PV(cbZeroExtended);
    Assert(iGReg < 16);

    /*
     * We can either just load the low 16-bit of the GPR into a host register
     * for the variable, or we can do so via a shadow copy host register. The
     * latter will avoid having to reload it if it's being stored later, but
     * will waste a host register if it isn't touched again.  Since we don't
     * know what going to happen, we choose the latter for now.
     */
    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);
    off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxVarReg, idxGstFullReg);
    iemNativeVarRegisterRelease(pReNative, idxDstVar);

    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}


#define IEM_MC_FETCH_GREG_U32_SX_U64(a_u32Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU32SxU64(pReNative, off, a_u32Dst, a_iGReg)

/** Emits code for IEM_MC_FETCH_GREG_U32. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU32SxU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGReg)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == sizeof(uint64_t));
    Assert(iGReg < 16);

    /*
     * We can either just load the low 32-bit of the GPR into a host register
     * for the variable, or we can do so via a shadow copy host register. The
     * latter will avoid having to reload it if it's being stored later, but
     * will waste a host register if it isn't touched again.  Since we don't
     * know what going to happen, we choose the latter for now.
     */
    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);
    off = iemNativeEmitLoadGprSignExtendedFromGpr32(pReNative, off, idxVarReg, idxGstFullReg);
    iemNativeVarRegisterRelease(pReNative, idxDstVar);

    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}


#define IEM_MC_FETCH_GREG_U64(a_u64Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU64(pReNative, off, a_u64Dst, a_iGReg)

#define IEM_MC_FETCH_GREG_U64_ZX_U64(a_u64Dst, a_iGReg) \
    off = iemNativeEmitFetchGregU64(pReNative, off, a_u64Dst, a_iGReg)

/** Emits code for IEM_MC_FETCH_GREG_U64 (and the
 *  IEM_MC_FETCH_GREG_U64_ZX_U64 alias). */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchGregU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iGReg)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == sizeof(uint64_t));
    Assert(iGReg < 16);

    uint8_t const idxGstFullReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                  kIemNativeGstRegUse_ReadOnly);

    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxVarReg, idxGstFullReg);
    /** @todo name the register a shadow one already? */
    iemNativeVarRegisterRelease(pReNative, idxDstVar);

    iemNativeRegFreeTmp(pReNative, idxGstFullReg);
    return off;
}



/*********************************************************************************************************************************
*   Emitters for general purpose register stores (IEM_MC_STORE_GREG_XXX).                                                        *
*********************************************************************************************************************************/

#define IEM_MC_STORE_GREG_U8_CONST_THREADED(a_iGRegEx, a_u8Value) \
    off = iemNativeEmitStoreGregU8Const(pReNative, off, a_iGRegEx, a_u8Value)

/** Emits code for IEM_MC_STORE_GREG_U8_CONST_THREADED. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU8Const(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGRegEx, uint8_t u8Value)
{
    Assert(iGRegEx < 20);
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGRegEx & 15),
                                                                 kIemNativeGstRegUse_ForUpdate);
#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 12);

    /* To the lowest byte of the register: mov r8, imm8 */
    if (iGRegEx < 16)
    {
        if (idxGstTmpReg >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
        else if (idxGstTmpReg >= 4)
            pbCodeBuf[off++] = X86_OP_REX;
        pbCodeBuf[off++] = 0xb0 + (idxGstTmpReg & 7);
        pbCodeBuf[off++] = u8Value;
    }
    /* Otherwise it's to ah, ch, dh or bh: use mov r8, imm8 if we can, otherwise, we rotate. */
    else if (idxGstTmpReg < 4)
    {
        pbCodeBuf[off++] = 0xb4 + idxGstTmpReg;
        pbCodeBuf[off++] = u8Value;
    }
    else
    {
        /* ror reg64, 8 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxGstTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 1, idxGstTmpReg & 7);
        pbCodeBuf[off++] = 8;

        /* mov reg8, imm8  */
        if (idxGstTmpReg >= 8)
            pbCodeBuf[off++] = X86_OP_REX_B;
        else if (idxGstTmpReg >= 4)
            pbCodeBuf[off++] = X86_OP_REX;
        pbCodeBuf[off++] = 0xb0 + (idxGstTmpReg & 7);
        pbCodeBuf[off++] = u8Value;

        /* rol reg64, 8 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxGstTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
        pbCodeBuf[off++] = 8;
    }

#elif defined(RT_ARCH_ARM64)
    uint8_t const    idxImmReg   = iemNativeRegAllocTmpImm(pReNative, &off, u8Value);
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);
    if (iGRegEx < 16)
        /* bfi w1, w2, 0, 8 - moves bits 7:0 from idxImmReg to idxGstTmpReg bits 7:0. */
        pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxImmReg, 0, 8);
    else
        /* bfi w1, w2, 8, 8 - moves bits 7:0 from idxImmReg to idxGstTmpReg bits 15:8. */
        pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxImmReg, 8, 8);
    iemNativeRegFreeTmp(pReNative, idxImmReg);

#else
# error "Port me!"
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGRegEx & 15]));

    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_STORE_GREG_U8_THREADED(a_iGRegEx, a_u8Value) \
    off = iemNativeEmitStoreGregU8(pReNative, off, a_iGRegEx, a_u8Value)

/** Emits code for IEM_MC_STORE_GREG_U8_THREADED. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGRegEx, uint8_t idxValueVar)
{
    Assert(iGRegEx < 20);
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxValueVar);

    /*
     * If it's a constant value (unlikely) we treat this as a
     * IEM_MC_STORE_GREG_U8_CONST statement.
     */
    if (pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Stack)
    { /* likely */ }
    else
    {
        AssertStmt(pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Immediate,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));
        return iemNativeEmitStoreGregU8Const(pReNative, off, iGRegEx, (uint8_t)pReNative->Core.aVars[idxValueVar].u.uValue);
    }

    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGRegEx & 15),
                                                                 kIemNativeGstRegUse_ForUpdate);
    uint8_t const    idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxValueVar, &off, true /*fInitialized*/);

#ifdef RT_ARCH_AMD64
    /* To the lowest byte of the register: mov reg8, reg8(r/m) */
    if (iGRegEx < 16)
    {
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 3);
        if (idxGstTmpReg >= 8 || idxVarReg >= 8)
            pbCodeBuf[off++] = (idxGstTmpReg >= 8 ? X86_OP_REX_R : 0) | (idxVarReg >= 8 ? X86_OP_REX_B : 0);
        else if (idxGstTmpReg >= 4 || idxVarReg >= 4)
            pbCodeBuf[off++] = X86_OP_REX;
        pbCodeBuf[off++] = 0x8a;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxGstTmpReg & 7, idxVarReg & 7);
    }
    /* Otherwise it's to ah, ch, dh or bh from al, cl, dl or bl: use mov r8, r8 if we can, otherwise, we rotate. */
    else if (idxGstTmpReg < 4 && idxVarReg < 4)
    {
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2+1);
        pbCodeBuf[off++] = 0x8a;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxGstTmpReg + 4, idxVarReg);
    }
    else
    {
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 15);

        /* ror reg64, 8 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxGstTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 1, idxGstTmpReg & 7);
        pbCodeBuf[off++] = 8;

        /* mov reg8, reg8(r/m)  */
        if (idxGstTmpReg >= 8 || idxVarReg >= 8)
            pbCodeBuf[off++] = (idxGstTmpReg >= 8 ? X86_OP_REX_R : 0) | (idxVarReg >= 8 ? X86_OP_REX_B : 0);
        else if (idxGstTmpReg >= 4 || idxVarReg >= 4)
            pbCodeBuf[off++] = X86_OP_REX;
        pbCodeBuf[off++] = 0x8a;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxGstTmpReg & 7, idxVarReg & 7);

        /* rol reg64, 8 */
        pbCodeBuf[off++] = X86_OP_REX_W | (idxGstTmpReg < 8 ? 0 : X86_OP_REX_B);
        pbCodeBuf[off++] = 0xc1;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
        pbCodeBuf[off++] = 8;
    }

#elif defined(RT_ARCH_ARM64)
    /* bfi w1, w2, 0, 8 - moves bits 7:0 from idxVarReg to idxGstTmpReg bits 7:0.
            or
       bfi w1, w2, 8, 8 - moves bits 7:0 from idxVarReg to idxGstTmpReg bits 15:8. */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    if (iGRegEx < 16)
        pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxVarReg, 0, 8);
    else
        pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxVarReg, 8, 8);

#else
# error "Port me!"
#endif
    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    iemNativeVarRegisterRelease(pReNative, idxValueVar);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGRegEx & 15]));
    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}



#define IEM_MC_STORE_GREG_U16_CONST(a_iGReg, a_u16Const) \
    off = iemNativeEmitStoreGregU16Const(pReNative, off, a_iGReg, a_u16Const)

/** Emits code for IEM_MC_STORE_GREG_U16. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU16Const(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint16_t uValue)
{
    Assert(iGReg < 16);
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);
#ifdef RT_ARCH_AMD64
    /* mov reg16, imm16 */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (idxGstTmpReg >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    pbCodeBuf[off++] = 0xb8 + (idxGstTmpReg & 7);
    pbCodeBuf[off++] = RT_BYTE1(uValue);
    pbCodeBuf[off++] = RT_BYTE2(uValue);

#elif defined(RT_ARCH_ARM64)
    /* movk xdst, #uValue, lsl #0 */
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrMovK(idxGstTmpReg, uValue);

#else
# error "Port me!"
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_STORE_GREG_U16(a_iGReg, a_u16Value) \
    off = iemNativeEmitStoreGregU16(pReNative, off, a_iGReg, a_u16Value)

/** Emits code for IEM_MC_STORE_GREG_U16. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t idxValueVar)
{
    Assert(iGReg < 16);
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxValueVar);

    /*
     * If it's a constant value (unlikely) we treat this as a
     * IEM_MC_STORE_GREG_U16_CONST statement.
     */
    if (pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Stack)
    { /* likely */ }
    else
    {
        AssertStmt(pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Immediate,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));
        return iemNativeEmitStoreGregU16Const(pReNative, off, iGReg, (uint16_t)pReNative->Core.aVars[idxValueVar].u.uValue);
    }

    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);

#ifdef RT_ARCH_AMD64
    /* mov reg16, reg16 or [mem16] */
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 12);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (pReNative->Core.aVars[idxValueVar].idxReg < RT_ELEMENTS(pReNative->Core.aHstRegs))
    {
        if (idxGstTmpReg >= 8 || pReNative->Core.aVars[idxValueVar].idxReg >= 8)
            pbCodeBuf[off++] = (idxGstTmpReg >= 8                              ? X86_OP_REX_R : 0)
                             | (pReNative->Core.aVars[idxValueVar].idxReg >= 8 ? X86_OP_REX_B : 0);
        pbCodeBuf[off++] = 0x8b;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, idxGstTmpReg & 7, pReNative->Core.aVars[idxValueVar].idxReg & 7);
    }
    else
    {
        uint8_t const idxStackSlot = pReNative->Core.aVars[idxValueVar].idxStackSlot;
        AssertStmt(idxStackSlot != UINT8_MAX, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_NOT_INITIALIZED));
        if (idxGstTmpReg >= 8)
            pbCodeBuf[off++] = X86_OP_REX_R;
        pbCodeBuf[off++] = 0x8b;
        off = iemNativeEmitGprByBpDisp(pbCodeBuf, off, idxGstTmpReg, iemNativeStackCalcBpDisp(idxStackSlot), pReNative);
    }

#elif defined(RT_ARCH_ARM64)
    /* bfi w1, w2, 0, 16 - moves bits 15:0 from idxVarReg to idxGstTmpReg bits 15:0. */
    uint8_t const    idxVarReg   = iemNativeVarRegisterAcquire(pReNative, idxValueVar, &off, true /*fInitialized*/);
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxVarReg, 0, 16);
    iemNativeVarRegisterRelease(pReNative, idxValueVar);

#else
# error "Port me!"
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_STORE_GREG_U32_CONST(a_iGReg, a_u32Const) \
    off = iemNativeEmitStoreGregU32Const(pReNative, off, a_iGReg, a_u32Const)

/** Emits code for IEM_MC_STORE_GREG_U32_CONST. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU32Const(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint32_t uValue)
{
    Assert(iGReg < 16);
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForFullWrite);
    off = iemNativeEmitLoadGprImm64(pReNative, off, idxGstTmpReg, uValue);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_STORE_GREG_U32(a_iGReg, a_u32Value) \
    off = iemNativeEmitStoreGregU32(pReNative, off, a_iGReg, a_u32Value)

/** Emits code for IEM_MC_STORE_GREG_U32. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU32(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t idxValueVar)
{
    Assert(iGReg < 16);
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxValueVar);

    /*
     * If it's a constant value (unlikely) we treat this as a
     * IEM_MC_STORE_GREG_U32_CONST statement.
     */
    if (pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Stack)
    { /* likely */ }
    else
    {
        AssertStmt(pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Immediate,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));
        return iemNativeEmitStoreGregU32Const(pReNative, off, iGReg, (uint32_t)pReNative->Core.aVars[idxValueVar].u.uValue);
    }

    /*
     * For the rest we allocate a guest register for the variable and writes
     * it to the CPUMCTX structure.
     */
    uint8_t const idxVarReg = iemNativeVarRegisterAcquireForGuestReg(pReNative, idxValueVar, IEMNATIVEGSTREG_GPR(iGReg), &off);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxVarReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
#ifdef VBOX_STRICT
    off = iemNativeEmitTop32BitsClearCheck(pReNative, off, idxVarReg);
#endif
    iemNativeVarRegisterRelease(pReNative, idxValueVar);
    return off;
}


#define IEM_MC_STORE_GREG_U64_CONST(a_iGReg, a_u64Const) \
    off = iemNativeEmitStoreGregU64Const(pReNative, off, a_iGReg, a_u64Const)

/** Emits code for IEM_MC_STORE_GREG_U64_CONST. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU64Const(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint64_t uValue)
{
    Assert(iGReg < 16);
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForFullWrite);
    off = iemNativeEmitLoadGprImm64(pReNative, off, idxGstTmpReg, uValue);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_STORE_GREG_U64(a_iGReg, a_u64Value) \
    off = iemNativeEmitStoreGregU64(pReNative, off, a_iGReg, a_u64Value)

/** Emits code for IEM_MC_STORE_GREG_U64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStoreGregU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t idxValueVar)
{
    Assert(iGReg < 16);
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxValueVar);

    /*
     * If it's a constant value (unlikely) we treat this as a
     * IEM_MC_STORE_GREG_U64_CONST statement.
     */
    if (pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Stack)
    { /* likely */ }
    else
    {
        AssertStmt(pReNative->Core.aVars[idxValueVar].enmKind == kIemNativeVarKind_Immediate,
                   IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));
        return iemNativeEmitStoreGregU64Const(pReNative, off, iGReg, pReNative->Core.aVars[idxValueVar].u.uValue);
    }

    /*
     * For the rest we allocate a guest register for the variable and writes
     * it to the CPUMCTX structure.
     */
    uint8_t const idxVarReg = iemNativeVarRegisterAcquireForGuestReg(pReNative, idxValueVar, IEMNATIVEGSTREG_GPR(iGReg), &off);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxVarReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
    iemNativeVarRegisterRelease(pReNative, idxValueVar);
    return off;
}


#define IEM_MC_CLEAR_HIGH_GREG_U64(a_iGReg) \
    off = iemNativeEmitClearHighGregU64(pReNative, off, a_iGReg)

/** Emits code for IEM_MC_CLEAR_HIGH_GREG_U64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitClearHighGregU64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg)
{
    Assert(iGReg < 16);
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);
    off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxGstTmpReg, idxGstTmpReg);
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));
    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


/*********************************************************************************************************************************
*   General purpose register manipulation (add, sub).                                                                            *
*********************************************************************************************************************************/

#define IEM_MC_ADD_GREG_U16(a_iGReg, a_u8SubtrahendConst) \
    off = iemNativeEmitAddGregU16(pReNative, off, a_iGReg, a_u8SubtrahendConst)

/** Emits code for IEM_MC_ADD_GREG_U16. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGregU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t uAddend)
{
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);

#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (idxGstTmpReg >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (uAddend == 1)
    {
        pbCodeBuf[off++] = 0xff; /* inc */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
    }
    else
    {
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
        pbCodeBuf[off++] = uAddend;
        pbCodeBuf[off++] = 0;
    }

#else
    uint8_t const    idxTmpReg   = iemNativeRegAllocTmp(pReNative, &off);
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);

    /* sub tmp, gstgrp, uAddend */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxTmpReg, idxGstTmpReg, uAddend, false /*f64Bit*/);

    /* bfi w1, w2, 0, 16 - moves bits 15:0 from tmpreg2 to tmpreg. */
    pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxTmpReg, 0, 16);

    iemNativeRegFreeTmp(pReNative, idxTmpReg);
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));

    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_ADD_GREG_U32(a_iGReg, a_u8Const) \
    off = iemNativeEmitAddGregU32U64(pReNative, off, a_iGReg, a_u8Const, false /*f64Bit*/)

#define IEM_MC_ADD_GREG_U64(a_iGReg, a_u8Const) \
    off = iemNativeEmitAddGregU32U64(pReNative, off, a_iGReg, a_u8Const, true /*f64Bit*/)

/** Emits code for IEM_MC_ADD_GREG_U32 and IEM_MC_ADD_GREG_U64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitAddGregU32U64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t uAddend, bool f64Bit)
{
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);

#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (f64Bit)
        pbCodeBuf[off++] = X86_OP_REX_W | (idxGstTmpReg >= 8 ? X86_OP_REX_B : 0);
    else if (idxGstTmpReg >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (uAddend == 1)
    {
        pbCodeBuf[off++] = 0xff; /* inc */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
    }
    else if (uAddend < 128)
    {
        pbCodeBuf[off++] = 0x83; /* add */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
        pbCodeBuf[off++] = RT_BYTE1(uAddend);
    }
    else
    {
        pbCodeBuf[off++] = 0x81; /* add */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 0, idxGstTmpReg & 7);
        pbCodeBuf[off++] = RT_BYTE1(uAddend);
        pbCodeBuf[off++] = 0;
        pbCodeBuf[off++] = 0;
        pbCodeBuf[off++] = 0;
    }

#else
    /* sub tmp, gstgrp, uAddend */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxGstTmpReg, idxGstTmpReg, uAddend, f64Bit);

#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));

    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}



#define IEM_MC_SUB_GREG_U16(a_iGReg, a_u8SubtrahendConst) \
    off = iemNativeEmitSubGregU16(pReNative, off, a_iGReg, a_u8SubtrahendConst)

/** Emits code for IEM_MC_SUB_GREG_U16. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitSubGregU16(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t uSubtrahend)
{
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);

#ifdef RT_ARCH_AMD64
    uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 6);
    pbCodeBuf[off++] = X86_OP_PRF_SIZE_OP;
    if (idxGstTmpReg >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (uSubtrahend == 1)
    {
        pbCodeBuf[off++] = 0xff; /* dec */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 1, idxGstTmpReg & 7);
    }
    else
    {
        pbCodeBuf[off++] = 0x81;
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, idxGstTmpReg & 7);
        pbCodeBuf[off++] = uSubtrahend;
        pbCodeBuf[off++] = 0;
    }

#else
    uint8_t const    idxTmpReg   = iemNativeRegAllocTmp(pReNative, &off);
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 2);

    /* sub tmp, gstgrp, uSubtrahend */
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, idxTmpReg, idxGstTmpReg, uSubtrahend, false /*f64Bit*/);

    /* bfi w1, w2, 0, 16 - moves bits 15:0 from tmpreg2 to tmpreg. */
    pu32CodeBuf[off++] = Armv8A64MkInstrBfi(idxGstTmpReg, idxTmpReg, 0, 16);

    iemNativeRegFreeTmp(pReNative, idxTmpReg);
#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));

    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}


#define IEM_MC_SUB_GREG_U32(a_iGReg, a_u8Const) \
    off = iemNativeEmitSubGregU32U64(pReNative, off, a_iGReg, a_u8Const, false /*f64Bit*/)

#define IEM_MC_SUB_GREG_U64(a_iGReg, a_u8Const) \
    off = iemNativeEmitSubGregU32U64(pReNative, off, a_iGReg, a_u8Const, true /*f64Bit*/)

/** Emits code for IEM_MC_SUB_GREG_U32 and IEM_MC_SUB_GREG_U64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitSubGregU32U64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t iGReg, uint8_t uSubtrahend, bool f64Bit)
{
    uint8_t const idxGstTmpReg = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(iGReg),
                                                                 kIemNativeGstRegUse_ForUpdate);

#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 7);
    if (f64Bit)
        pbCodeBuf[off++] = X86_OP_REX_W | (idxGstTmpReg >= 8 ? X86_OP_REX_B : 0);
    else if (idxGstTmpReg >= 8)
        pbCodeBuf[off++] = X86_OP_REX_B;
    if (uSubtrahend == 1)
    {
        pbCodeBuf[off++] = 0xff; /* dec */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 1, idxGstTmpReg & 7);
    }
    else if (uSubtrahend < 128)
    {
        pbCodeBuf[off++] = 0x83; /* sub */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, idxGstTmpReg & 7);
        pbCodeBuf[off++] = RT_BYTE1(uSubtrahend);
    }
    else
    {
        pbCodeBuf[off++] = 0x81; /* sub */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, idxGstTmpReg & 7);
        pbCodeBuf[off++] = RT_BYTE1(uSubtrahend);
        pbCodeBuf[off++] = 0;
        pbCodeBuf[off++] = 0;
        pbCodeBuf[off++] = 0;
    }

#else
    /* sub tmp, gstgrp, uSubtrahend */
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
    pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, idxGstTmpReg, idxGstTmpReg, uSubtrahend, f64Bit);

#endif

    IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxGstTmpReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[iGReg]));

    iemNativeRegFreeTmp(pReNative, idxGstTmpReg);
    return off;
}



/*********************************************************************************************************************************
*   EFLAGS                                                                                                                       *
*********************************************************************************************************************************/

#if !defined(VBOX_WITH_STATISTICS) || !defined(IEMNATIVE_WITH_LIVENESS_ANALYSIS)
# define IEMNATIVE_EFLAGS_OPTIMIZATION_STATS(a_fEflInput, a_fEflOutput)     ((void)0)
#else
# define IEMNATIVE_EFLAGS_OPTIMIZATION_STATS(a_fEflInput, a_fEflOutput) \
    iemNativeEFlagsOptimizationStats(pReNative, a_fEflInput, a_fEflOutput)

DECLINLINE(void) iemNativeEFlagsOptimizationStats(PIEMRECOMPILERSTATE pReNative, uint32_t fEflInput, uint32_t fEflOutput)
{
    if (fEflOutput)
    {
        IEMLIVENESSPART2 const LivenessInfo2 = pReNative->paLivenessEntries[pReNative->idxCurCall].s2;
        PVMCPUCC const pVCpu = pReNative->pVCpu;
# define CHECK_FLAG_AND_UPDATE_STATS(a_fEfl, a_u2LivenessMember, a_CoreStatName) \
            if (fEflOutput & (a_fEfl)) \
            { \
                if (LivenessInfo2.a_u2LivenessMember != IEMLIVENESS_STATE_CLOBBERED) \
                    STAM_COUNTER_INC(&pVCpu->iem.s. a_CoreStatName ## Required); \
                else \
                    STAM_COUNTER_INC(&pVCpu->iem.s. a_CoreStatName ## Skippable); \
            } else do { } while (0)
        CHECK_FLAG_AND_UPDATE_STATS(X86_EFL_CF, u2EflCf, StatNativeLivenessEflCf);
        CHECK_FLAG_AND_UPDATE_STATS(X86_EFL_PF, u2EflPf, StatNativeLivenessEflPf);
        CHECK_FLAG_AND_UPDATE_STATS(X86_EFL_AF, u2EflAf, StatNativeLivenessEflAf);
        CHECK_FLAG_AND_UPDATE_STATS(X86_EFL_ZF, u2EflZf, StatNativeLivenessEflZf);
        CHECK_FLAG_AND_UPDATE_STATS(X86_EFL_SF, u2EflSf, StatNativeLivenessEflSf);
        CHECK_FLAG_AND_UPDATE_STATS(X86_EFL_OF, u2EflOf, StatNativeLivenessEflOf);
        CHECK_FLAG_AND_UPDATE_STATS(~X86_EFL_STATUS_BITS, u2EflOther, StatNativeLivenessEflOther);
# undef CHECK_FLAG_AND_UPDATE_STATS
    }
    RT_NOREF(fEflInput);
}
#endif /* VBOX_WITH_STATISTICS */

#undef  IEM_MC_FETCH_EFLAGS /* should not be used */
#define IEM_MC_FETCH_EFLAGS_EX(a_EFlags, a_fEflInput, a_fEflOutput) \
    off = iemNativeEmitFetchEFlags(pReNative, off, a_EFlags)

/** Handles IEM_MC_FETCH_EFLAGS_EX. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchEFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarEFlags)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarEFlags);
    Assert(pReNative->Core.aVars[idxVarEFlags].cbVar == sizeof(uint32_t));

    uint8_t const idxReg = iemNativeVarRegisterAcquire(pReNative, idxVarEFlags, &off, false /*fInitialized*/);
    iemNativeRegClearAndMarkAsGstRegShadow(pReNative, idxReg, kIemNativeGstReg_EFlags, off);
    off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, idxReg, RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.eflags));
    iemNativeVarRegisterRelease(pReNative, idxVarEFlags);
    return off;
}



/** @todo emit strict build assertions for IEM_MC_COMMIT_EFLAGS_EX when we
 * start using it with custom native code emission (inlining assembly
 * instruction helpers). */
#undef  IEM_MC_COMMIT_EFLAGS /* should not be used */
#define IEM_MC_COMMIT_EFLAGS_EX(a_EFlags, a_fEflInput, a_fEflOutput) \
    IEMNATIVE_EFLAGS_OPTIMIZATION_STATS(a_fEflInput, a_fEflOutput); \
    off = iemNativeEmitCommitEFlags(pReNative, off, a_EFlags)

/** Handles IEM_MC_COMMIT_EFLAGS_EX. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCommitEFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarEFlags)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarEFlags);
    Assert(pReNative->Core.aVars[idxVarEFlags].cbVar == sizeof(uint32_t));

    uint8_t const idxReg = iemNativeVarRegisterAcquire(pReNative, idxVarEFlags, &off, true /*fInitialized*/);

#ifdef VBOX_STRICT
    off = iemNativeEmitTestAnyBitsInGpr(pReNative, off, idxReg, X86_EFL_RA1_MASK);
    uint32_t offFixup = off;
    off = iemNativeEmitJnzToFixed(pReNative, off, off);
    off = iemNativeEmitBrk(pReNative, off, UINT32_C(0x2001));
    iemNativeFixupFixedJump(pReNative, offFixup, off);

    off = iemNativeEmitTestAnyBitsInGpr(pReNative, off, idxReg, X86_EFL_RAZ_MASK & CPUMX86EFLAGS_HW_MASK_32);
    offFixup = off;
    off = iemNativeEmitJzToFixed(pReNative, off, off);
    off = iemNativeEmitBrk(pReNative, off, UINT32_C(0x2002));
    iemNativeFixupFixedJump(pReNative, offFixup, off);
#endif

    iemNativeRegClearAndMarkAsGstRegShadow(pReNative, idxReg, kIemNativeGstReg_EFlags, off);
    off = iemNativeEmitStoreGprToVCpuU32(pReNative, off, idxReg, RT_UOFFSETOF_DYN(VMCPUCC, cpum.GstCtx.eflags));
    iemNativeVarRegisterRelease(pReNative, idxVarEFlags);
    return off;
}



/*********************************************************************************************************************************
*   Emitters for segment register fetches (IEM_MC_FETCH_SREG_XXX).
*********************************************************************************************************************************/

#define IEM_MC_FETCH_SREG_U16(a_u16Dst, a_iSReg) \
    off = iemNativeEmitFetchSReg(pReNative, off, a_u16Dst, a_iSReg, sizeof(uint16_t))

#define IEM_MC_FETCH_SREG_ZX_U32(a_u32Dst, a_iSReg) \
    off = iemNativeEmitFetchSReg(pReNative, off, a_u32Dst, a_iSReg, sizeof(uint32_t))

#define IEM_MC_FETCH_SREG_ZX_U64(a_u64Dst, a_iSReg) \
    off = iemNativeEmitFetchSReg(pReNative, off, a_u64Dst, a_iSReg, sizeof(uint64_t))


/** Emits code for IEM_MC_FETCH_SREG_U16, IEM_MC_FETCH_SREG_ZX_U32 and
 *  IEM_MC_FETCH_SREG_ZX_U64. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitFetchSReg(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxDstVar, uint8_t iSReg, int8_t cbVar)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxDstVar);
    Assert(pReNative->Core.aVars[idxDstVar].cbVar == cbVar); RT_NOREF(cbVar);
    Assert(iSReg < X86_SREG_COUNT);

    /*
     * For now, we will not create a shadow copy of a selector.  The rational
     * is that since we do not recompile the popping and loading of segment
     * registers and that the the IEM_MC_FETCH_SREG_U* MCs are only used for
     * pushing and moving to registers, there is only a small chance that the
     * shadow copy will be accessed again before the register is reloaded.  One
     * scenario would be nested called in 16-bit code, but I doubt it's worth
     * the extra register pressure atm.
     *
     * What we really need first, though, is to combine iemNativeRegAllocTmpForGuestReg
     * and iemNativeVarRegisterAcquire for a load scenario. We only got the
     * store scencario covered at present (r160730).
     */
    iemNativeVarSetKindToStack(pReNative, idxDstVar);
    uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxDstVar, &off);
    off = iemNativeEmitLoadGprFromVCpuU16(pReNative, off, idxVarReg, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aSRegs[iSReg].Sel));
    iemNativeVarRegisterRelease(pReNative, idxDstVar);
    return off;
}



/*********************************************************************************************************************************
*   Register references.                                                                                                         *
*********************************************************************************************************************************/

#define IEM_MC_REF_GREG_U8_THREADED(a_pu8Dst, a_iGRegEx) \
    off = iemNativeEmitRefGregU8(pReNative, off, a_pu8Dst, a_iGRegEx, false /*fConst*/)

#define IEM_MC_REF_GREG_U8_CONST_THREADED(a_pu8Dst, a_iGReg) \
    off = iemNativeEmitRefGregU8(pReNative, off, a_pu8Dst, a_iGRegEx, true /*fConst*/)

/** Handles IEM_MC_REF_GREG_U8[_CONST]. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitRefGregU8(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRef, uint8_t iGRegEx, bool fConst)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRef);
    Assert(pReNative->Core.aVars[idxVarRef].cbVar == sizeof(void *));
    Assert(iGRegEx < 20);

    if (iGRegEx < 16)
        iemNativeVarSetKindToGstRegRef(pReNative, idxVarRef, kIemNativeGstRegRef_Gpr, iGRegEx & 15);
    else
        iemNativeVarSetKindToGstRegRef(pReNative, idxVarRef, kIemNativeGstRegRef_GprHighByte, iGRegEx & 15);

    /* If we've delayed writing back the register value, flush it now. */
    off = iemNativeRegFlushPendingSpecificWrite(pReNative, off, kIemNativeGstRegRef_Gpr, iGRegEx & 15);

    /* If it's not a const reference we need to flush the shadow copy of the register now. */
    if (!fConst)
        iemNativeRegFlushGuestShadows(pReNative, RT_BIT_64(IEMNATIVEGSTREG_GPR(iGRegEx & 15)));

    return off;
}

#define IEM_MC_REF_GREG_U16(a_pu16Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pu16Dst, a_iGReg, false /*fConst*/)

#define IEM_MC_REF_GREG_U16_CONST(a_pu16Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pu16Dst, a_iGReg, true /*fConst*/)

#define IEM_MC_REF_GREG_U32(a_pu32Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pu32Dst, a_iGReg, false /*fConst*/)

#define IEM_MC_REF_GREG_U32_CONST(a_pu32Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pu32Dst, a_iGReg, true /*fConst*/)

#define IEM_MC_REF_GREG_I32(a_pi32Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pi32Dst, a_iGReg, false /*fConst*/)

#define IEM_MC_REF_GREG_I32_CONST(a_pi32Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pi32Dst, a_iGReg, true /*fConst*/)

#define IEM_MC_REF_GREG_U64(a_pu64Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pu64Dst, a_iGReg, false /*fConst*/)

#define IEM_MC_REF_GREG_U64_CONST(a_pu64Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pu64Dst, a_iGReg, true /*fConst*/)

#define IEM_MC_REF_GREG_I64(a_pi64Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pi64Dst, a_iGReg, false /*fConst*/)

#define IEM_MC_REF_GREG_I64_CONST(a_pi64Dst, a_iGReg) \
    off = iemNativeEmitRefGregUxx(pReNative, off, a_pi64Dst, a_iGReg, true /*fConst*/)

/** Handles IEM_MC_REF_GREG_Uxx[_CONST] and IEM_MC_REF_GREG_Ixx[_CONST]. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitRefGregUxx(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRef, uint8_t iGReg, bool fConst)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRef);
    Assert(pReNative->Core.aVars[idxVarRef].cbVar == sizeof(void *));
    Assert(iGReg < 16);

    iemNativeVarSetKindToGstRegRef(pReNative, idxVarRef, kIemNativeGstRegRef_Gpr, iGReg);

    /* If we've delayed writing back the register value, flush it now. */
    off = iemNativeRegFlushPendingSpecificWrite(pReNative, off, kIemNativeGstRegRef_Gpr, iGReg);

    /* If it's not a const reference we need to flush the shadow copy of the register now. */
    if (!fConst)
        iemNativeRegFlushGuestShadows(pReNative, RT_BIT_64(IEMNATIVEGSTREG_GPR(iGReg)));

    return off;
}


#undef  IEM_MC_REF_EFLAGS /* should not be used. */
#define IEM_MC_REF_EFLAGS_EX(a_pEFlags, a_fEflInput, a_fEflOutput) \
    IEMNATIVE_EFLAGS_OPTIMIZATION_STATS(a_fEflInput, a_fEflOutput); \
    off = iemNativeEmitRefEFlags(pReNative, off, a_pEFlags)

/** Handles IEM_MC_REF_EFLAGS. */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitRefEFlags(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarRef)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRef);
    Assert(pReNative->Core.aVars[idxVarRef].cbVar == sizeof(void *));

    iemNativeVarSetKindToGstRegRef(pReNative, idxVarRef, kIemNativeGstRegRef_EFlags, 0);

    /* If we've delayed writing back the register value, flush it now. */
    off = iemNativeRegFlushPendingSpecificWrite(pReNative, off, kIemNativeGstRegRef_EFlags, 0);

    /* If there is a shadow copy of guest EFLAGS, flush it now. */
    iemNativeRegFlushGuestShadows(pReNative, RT_BIT_64(kIemNativeGstReg_EFlags));

    return off;
}


/* @todo Emit code for IEM_MC_ASSERT_EFLAGS in strict builds?  Once we emit
 * different code from threaded recompiler, maybe it would be helpful. For now
 * we assume the threaded recompiler catches any incorrect EFLAGS delcarations. */
#define IEM_MC_ASSERT_EFLAGS(a_fEflInput, a_fEflOutput) ((void)0)



/*********************************************************************************************************************************
*   Effective Address Calculation                                                                                                *
*********************************************************************************************************************************/
#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_16(a_GCPtrEff, a_bRm, a_u16Disp) \
    off = iemNativeEmitCalcRmEffAddrThreadedAddr16(pReNative, off, a_bRm, a_u16Disp, a_GCPtrEff)

/** Emit code for IEM_MC_CALC_RM_EFF_ADDR_THREADED_16.
 * @sa iemOpHlpCalcRmEffAddrThreadedAddr16  */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCalcRmEffAddrThreadedAddr16(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                         uint8_t bRm, uint16_t u16Disp, uint8_t idxVarRet)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRet);

    /*
     * Handle the disp16 form with no registers first.
     *
     * Convert to an immediate value, as that'll delay the register allocation
     * and assignment till the memory access / call / whatever and we can use
     * a more appropriate register (or none at all).
     */
    if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 6)
    {
        iemNativeVarSetKindToConst(pReNative, idxVarRet, u16Disp);
        return off;
    }

    /* Determin the displacment. */
    uint16_t u16EffAddr;
    switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
    {
        case 0:  u16EffAddr = 0;                        break;
        case 1:  u16EffAddr = (int16_t)(int8_t)u16Disp; break;
        case 2:  u16EffAddr = u16Disp;                  break;
        default: AssertFailedStmt(u16EffAddr = 0);
    }

    /* Determine the registers involved. */
    uint8_t idxGstRegBase;
    uint8_t idxGstRegIndex;
    switch (bRm & X86_MODRM_RM_MASK)
    {
        case 0:
            idxGstRegBase  = X86_GREG_xBX;
            idxGstRegIndex = X86_GREG_xSI;
            break;
        case 1:
            idxGstRegBase  = X86_GREG_xBX;
            idxGstRegIndex = X86_GREG_xDI;
            break;
        case 2:
            idxGstRegBase  = X86_GREG_xBP;
            idxGstRegIndex = X86_GREG_xSI;
            break;
        case 3:
            idxGstRegBase  = X86_GREG_xBP;
            idxGstRegIndex = X86_GREG_xDI;
            break;
        case 4:
            idxGstRegBase  = X86_GREG_xSI;
            idxGstRegIndex = UINT8_MAX;
            break;
        case 5:
            idxGstRegBase  = X86_GREG_xDI;
            idxGstRegIndex = UINT8_MAX;
            break;
        case 6:
            idxGstRegBase  = X86_GREG_xBP;
            idxGstRegIndex = UINT8_MAX;
            break;
#ifdef _MSC_VER  /* lazy compiler, thinks idxGstRegBase and idxGstRegIndex may otherwise be used uninitialized. */
        default:
#endif
        case 7:
            idxGstRegBase  = X86_GREG_xBX;
            idxGstRegIndex = UINT8_MAX;
            break;
    }

    /*
     * Now emit code that calculates: idxRegRet = (uint16_t)(u16EffAddr + idxGstRegBase [+ idxGstRegIndex])
     */
    uint8_t const idxRegRet   = iemNativeVarRegisterAcquire(pReNative, idxVarRet, &off);
    uint8_t const idxRegBase  = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGstRegBase),
                                                               kIemNativeGstRegUse_ReadOnly);
    uint8_t const idxRegIndex = idxGstRegIndex != UINT8_MAX
                              ? iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGstRegIndex),
                                                               kIemNativeGstRegUse_ReadOnly)
                              : UINT8_MAX;
#ifdef RT_ARCH_AMD64
    if (idxRegIndex == UINT8_MAX)
    {
        if (u16EffAddr == 0)
        {
            /* movxz ret, base */
            off = iemNativeEmitLoadGprFromGpr16(pReNative, off, idxRegRet, idxRegBase);
        }
        else
        {
            /* lea ret32, [base64 + disp32] */
            Assert(idxRegBase != X86_GREG_xSP /*SIB*/);
            uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
            if (idxRegRet >= 8 || idxRegBase >= 8)
                pbCodeBuf[off++] = (idxRegRet >= 8 ? X86_OP_REX_R : 0) | (idxRegBase >= 8 ? X86_OP_REX_B : 0);
            pbCodeBuf[off++] = 0x8d;
            if (idxRegBase != X86_GREG_x12 /*SIB*/)
                pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, idxRegRet & 7, idxRegBase & 7);
            else
            {
                pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, idxRegRet & 7, 4 /*SIB*/);
                pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_x12 & 7, 4 /*no index*/, 0);
            }
            pbCodeBuf[off++] = RT_BYTE1(u16EffAddr);
            pbCodeBuf[off++] = RT_BYTE2(u16EffAddr);
            pbCodeBuf[off++] = 0;
            pbCodeBuf[off++] = 0;
            IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);

            off = iemNativeEmitClear16UpGpr(pReNative, off, idxRegRet);
        }
    }
    else
    {
        /* lea ret32, [index64 + base64 (+ disp32)] */
        Assert(idxRegIndex != X86_GREG_xSP /*no-index*/);
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
        if (idxRegRet >= 8 || idxRegBase >= 8 || idxRegIndex >= 8)
            pbCodeBuf[off++] = (idxRegRet   >= 8 ? X86_OP_REX_R : 0)
                             | (idxRegBase  >= 8 ? X86_OP_REX_B : 0)
                             | (idxRegIndex >= 8 ? X86_OP_REX_X : 0);
        pbCodeBuf[off++] = 0x8d;
        uint8_t const bMod = u16EffAddr == 0 && (idxRegBase & 7) != X86_GREG_xBP ? X86_MOD_MEM0 : X86_MOD_MEM4;
        pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, 4 /*SIB*/);
        pbCodeBuf[off++] = X86_SIB_MAKE(idxRegBase & 7, idxRegIndex & 7, 0);
        if (bMod == X86_MOD_MEM4)
        {
            pbCodeBuf[off++] = RT_BYTE1(u16EffAddr);
            pbCodeBuf[off++] = RT_BYTE2(u16EffAddr);
            pbCodeBuf[off++] = 0;
            pbCodeBuf[off++] = 0;
        }
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        off = iemNativeEmitClear16UpGpr(pReNative, off, idxRegRet);
    }

#elif defined(RT_ARCH_ARM64)
    uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 5);
    if (u16EffAddr == 0)
    {
        if (idxRegIndex == UINT8_MAX)
            pu32CodeBuf[off++] = Armv8A64MkInstrUxth(idxRegRet, idxRegBase);
        else
        {
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegBase, idxRegIndex, false /*f64Bit*/);
            pu32CodeBuf[off++] = Armv8A64MkInstrUxth(idxRegRet, idxRegRet);
        }
    }
    else
    {
        if ((int16_t)u16EffAddr < 4096 && (int16_t)u16EffAddr >= 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxRegRet, idxRegBase, u16EffAddr, false /*f64Bit*/);
        else if ((int16_t)u16EffAddr > -4096 && (int16_t)u16EffAddr < 0)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, idxRegRet, idxRegBase,
                                                             (uint16_t)-(int16_t)u16EffAddr, false /*f64Bit*/);
        else
        {
            pu32CodeBuf[off++] = Armv8A64MkInstrMovZ(idxRegRet, u16EffAddr);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegRet, idxRegBase, false /*f64Bit*/);
        }
        if (idxRegIndex != UINT8_MAX)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegRet, idxRegIndex, false /*f64Bit*/);
        pu32CodeBuf[off++] = Armv8A64MkInstrUxth(idxRegRet, idxRegRet);
    }

#else
# error "port me"
#endif

    if (idxRegIndex != UINT8_MAX)
        iemNativeRegFreeTmp(pReNative, idxRegIndex);
    iemNativeRegFreeTmp(pReNative, idxRegBase);
    iemNativeVarRegisterRelease(pReNative, idxVarRet);
    return off;
}


#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_32(a_GCPtrEff, a_bRm, a_uSibAndRspOffset, a_u32Disp) \
    off = iemNativeEmitCalcRmEffAddrThreadedAddr32(pReNative, off, a_bRm, a_uSibAndRspOffset, a_u32Disp, a_GCPtrEff)

/** Emit code for IEM_MC_CALC_RM_EFF_ADDR_THREADED_32.
 * @see iemOpHlpCalcRmEffAddrThreadedAddr32  */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCalcRmEffAddrThreadedAddr32(PIEMRECOMPILERSTATE pReNative, uint32_t off,
                                         uint8_t bRm, uint32_t uSibAndRspOffset, uint32_t u32Disp, uint8_t idxVarRet)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRet);

    /*
     * Handle the disp32 form with no registers first.
     *
     * Convert to an immediate value, as that'll delay the register allocation
     * and assignment till the memory access / call / whatever and we can use
     * a more appropriate register (or none at all).
     */
    if ((bRm & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
    {
        iemNativeVarSetKindToConst(pReNative, idxVarRet, u32Disp);
        return off;
    }

    /* Calculate the fixed displacement (more down in SIB.B=4 and SIB.B=5 on this). */
    uint32_t u32EffAddr = 0;
    switch ((bRm >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
    {
        case 0: break;
        case 1: u32EffAddr = (int8_t)u32Disp; break;
        case 2: u32EffAddr = u32Disp; break;
        default: AssertFailed();
    }

    /* Get the register (or SIB) value. */
    uint8_t idxGstRegBase  = UINT8_MAX;
    uint8_t idxGstRegIndex = UINT8_MAX;
    uint8_t cShiftIndex    = 0;
    switch (bRm & X86_MODRM_RM_MASK)
    {
        case 0: idxGstRegBase = X86_GREG_xAX; break;
        case 1: idxGstRegBase = X86_GREG_xCX; break;
        case 2: idxGstRegBase = X86_GREG_xDX; break;
        case 3: idxGstRegBase = X86_GREG_xBX; break;
        case 4: /* SIB */
        {
            /* index /w scaling . */
            cShiftIndex = (uSibAndRspOffset >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;
            switch ((uSibAndRspOffset >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK)
            {
                case 0: idxGstRegIndex = X86_GREG_xAX; break;
                case 1: idxGstRegIndex = X86_GREG_xCX; break;
                case 2: idxGstRegIndex = X86_GREG_xDX; break;
                case 3: idxGstRegIndex = X86_GREG_xBX; break;
                case 4: cShiftIndex    = 0; /*no index*/ break;
                case 5: idxGstRegIndex = X86_GREG_xBP; break;
                case 6: idxGstRegIndex = X86_GREG_xSI; break;
                case 7: idxGstRegIndex = X86_GREG_xDI; break;
            }

            /* base */
            switch (uSibAndRspOffset & X86_SIB_BASE_MASK)
            {
                case 0: idxGstRegBase = X86_GREG_xAX; break;
                case 1: idxGstRegBase = X86_GREG_xCX; break;
                case 2: idxGstRegBase = X86_GREG_xDX; break;
                case 3: idxGstRegBase = X86_GREG_xBX; break;
                case 4:
                    idxGstRegBase     = X86_GREG_xSP;
                    u32EffAddr       += uSibAndRspOffset >> 8;
                    break;
                case 5:
                    if ((bRm & X86_MODRM_MOD_MASK) != 0)
                        idxGstRegBase = X86_GREG_xBP;
                    else
                    {
                        Assert(u32EffAddr == 0);
                        u32EffAddr    = u32Disp;
                    }
                    break;
                case 6: idxGstRegBase = X86_GREG_xSI; break;
                case 7: idxGstRegBase = X86_GREG_xDI; break;
            }
            break;
        }
        case 5: idxGstRegBase = X86_GREG_xBP; break;
        case 6: idxGstRegBase = X86_GREG_xSI; break;
        case 7: idxGstRegBase = X86_GREG_xDI; break;
    }

    /*
     * If no registers are involved (SIB.B=5, SIB.X=4) repeat what we did at
     * the start of the function.
     */
    if (idxGstRegBase == UINT8_MAX && idxGstRegIndex == UINT8_MAX)
    {
        iemNativeVarSetKindToConst(pReNative, idxVarRet, u32EffAddr);
        return off;
    }

    /*
     * Now emit code that calculates: idxRegRet = (uint32_t)(u32EffAddr [+ idxGstRegBase] [+ (idxGstRegIndex << cShiftIndex)])
     */
    uint8_t const idxRegRet   = iemNativeVarRegisterAcquire(pReNative, idxVarRet, &off);
    uint8_t       idxRegBase  = idxGstRegBase == UINT8_MAX ? UINT8_MAX
                              : iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGstRegBase),
                                                                kIemNativeGstRegUse_ReadOnly);
    uint8_t       idxRegIndex = idxGstRegIndex == UINT8_MAX ? UINT8_MAX
                              : iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGstRegIndex),
                                                               kIemNativeGstRegUse_ReadOnly);

    /* If base is not given and there is no shifting, swap the registers to avoid code duplication. */
    if (idxRegBase == UINT8_MAX && cShiftIndex == 0)
    {
        idxRegBase  = idxRegIndex;
        idxRegIndex = UINT8_MAX;
    }

#ifdef RT_ARCH_AMD64
    if (idxRegIndex == UINT8_MAX)
    {
        if (u32EffAddr == 0)
        {
            /* mov ret, base */
            off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxRegRet, idxRegBase);
        }
        else
        {
            /* lea ret32, [base64 + disp32] */
            Assert(idxRegBase != X86_GREG_xSP /*SIB*/);
            uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
            if (idxRegRet >= 8 || idxRegBase >= 8)
                pbCodeBuf[off++] = (idxRegRet >= 8 ? X86_OP_REX_R : 0) | (idxRegBase >= 8 ? X86_OP_REX_B : 0);
            pbCodeBuf[off++] = 0x8d;
            uint8_t const bMod = (int8_t)u32EffAddr == (int32_t)u32EffAddr ? X86_MOD_MEM1 : X86_MOD_MEM4;
            if (idxRegBase != X86_GREG_x12 /*SIB*/)
                pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, idxRegBase & 7);
            else
            {
                pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, 4 /*SIB*/);
                pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_x12 & 7, 4 /*no index*/, 0);
            }
            pbCodeBuf[off++] = RT_BYTE1(u32EffAddr);
            if (bMod == X86_MOD_MEM4)
            {
                pbCodeBuf[off++] = RT_BYTE2(u32EffAddr);
                pbCodeBuf[off++] = RT_BYTE3(u32EffAddr);
                pbCodeBuf[off++] = RT_BYTE4(u32EffAddr);
            }
            IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        }
    }
    else
    {
        Assert(idxRegIndex != X86_GREG_xSP /*no-index*/);
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
        if (idxRegBase == UINT8_MAX)
        {
            /* lea ret32, [(index64 << cShiftIndex) + disp32] */
            if (idxRegRet >= 8 || idxRegIndex >= 8)
                pbCodeBuf[off++] = (idxRegRet   >= 8 ? X86_OP_REX_R : 0)
                                 | (idxRegIndex >= 8 ? X86_OP_REX_X : 0);
            pbCodeBuf[off++] = 0x8d;
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM0, idxRegRet & 7, 4 /*SIB*/);
            pbCodeBuf[off++] = X86_SIB_MAKE(5 /*nobase/bp*/, idxRegIndex & 7, cShiftIndex);
            pbCodeBuf[off++] = RT_BYTE1(u32EffAddr);
            pbCodeBuf[off++] = RT_BYTE2(u32EffAddr);
            pbCodeBuf[off++] = RT_BYTE3(u32EffAddr);
            pbCodeBuf[off++] = RT_BYTE4(u32EffAddr);
        }
        else
        {
            /* lea ret32, [(index64 << cShiftIndex) + base64 (+ disp32)] */
            if (idxRegRet >= 8 || idxRegBase >= 8 || idxRegIndex >= 8)
                pbCodeBuf[off++] = (idxRegRet   >= 8 ? X86_OP_REX_R : 0)
                                 | (idxRegBase  >= 8 ? X86_OP_REX_B : 0)
                                 | (idxRegIndex >= 8 ? X86_OP_REX_X : 0);
            pbCodeBuf[off++] = 0x8d;
            uint8_t const bMod = u32EffAddr == 0 && (idxRegBase & 7) != X86_GREG_xBP ? X86_MOD_MEM0
                               : (int8_t)u32EffAddr == (int32_t)u32EffAddr           ? X86_MOD_MEM1 : X86_MOD_MEM4;
            pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, 4 /*SIB*/);
            pbCodeBuf[off++] = X86_SIB_MAKE(idxRegBase & 7, idxRegIndex & 7, cShiftIndex);
            if (bMod != X86_MOD_MEM0)
            {
                pbCodeBuf[off++] = RT_BYTE1(u32EffAddr);
                if (bMod == X86_MOD_MEM4)
                {
                    pbCodeBuf[off++] = RT_BYTE2(u32EffAddr);
                    pbCodeBuf[off++] = RT_BYTE3(u32EffAddr);
                    pbCodeBuf[off++] = RT_BYTE4(u32EffAddr);
                }
            }
        }
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }

#elif defined(RT_ARCH_ARM64)
    if (u32EffAddr == 0)
    {
        if (idxRegIndex == UINT8_MAX)
            off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxRegRet, idxRegBase);
        else if (idxRegBase == UINT8_MAX)
        {
            if (cShiftIndex == 0)
                off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxRegRet, idxRegIndex);
            else
            {
                uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
                pu32CodeBuf[off++] = Armv8A64MkInstrLslImm(idxRegRet, idxRegIndex, cShiftIndex, false /*f64Bit*/);
            }
        }
        else
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegBase, idxRegIndex,
                                                          false /*f64Bit*/, false /*fSetFlags*/, cShiftIndex);
        }
    }
    else
    {
        if ((int32_t)u32EffAddr < 4096 && (int32_t)u32EffAddr >= 0 && idxRegBase != UINT8_MAX)
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxRegRet, idxRegBase, u32EffAddr, false /*f64Bit*/);
        }
        else if ((int32_t)u32EffAddr > -4096 && (int32_t)u32EffAddr < 0 && idxRegBase != UINT8_MAX)
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, idxRegRet, idxRegBase,
                                                             (uint32_t)-(int32_t)u32EffAddr, false /*f64Bit*/);
        }
        else
        {
            off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegRet, u32EffAddr);
            if (idxRegBase != UINT8_MAX)
            {
                uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
                pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegRet, idxRegBase, false /*f64Bit*/);
            }
        }
        if (idxRegIndex != UINT8_MAX)
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegRet, idxRegIndex,
                                                          false /*f64Bit*/, false /*fSetFlags*/, cShiftIndex);
        }
    }

#else
# error "port me"
#endif

    if (idxRegIndex != UINT8_MAX)
        iemNativeRegFreeTmp(pReNative, idxRegIndex);
    if (idxRegBase != UINT8_MAX)
        iemNativeRegFreeTmp(pReNative, idxRegBase);
    iemNativeVarRegisterRelease(pReNative, idxVarRet);
    return off;
}


#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_64(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm) \
    off = iemNativeEmitCalcRmEffAddrThreadedAddr64(pReNative, off, a_bRmEx, a_uSibAndRspOffset, \
                                                   a_u32Disp, a_cbImm, a_GCPtrEff, true /*f64Bit*/)

#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_FSGS(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm) \
    off = iemNativeEmitCalcRmEffAddrThreadedAddr64(pReNative, off, a_bRmEx, a_uSibAndRspOffset, \
                                                   a_u32Disp, a_cbImm, a_GCPtrEff, true /*f64Bit*/)

#define IEM_MC_CALC_RM_EFF_ADDR_THREADED_64_ADDR32(a_GCPtrEff, a_bRmEx, a_uSibAndRspOffset, a_u32Disp, a_cbImm) \
    off = iemNativeEmitCalcRmEffAddrThreadedAddr64(pReNative, off, a_bRmEx, a_uSibAndRspOffset, \
                                                   a_u32Disp, a_cbImm, a_GCPtrEff, false /*f64Bit*/)

/**
 * Emit code for IEM_MC_CALC_RM_EFF_ADDR_THREADED_64*.
 *
 * @returns New off.
 * @param   pReNative           .
 * @param   off                 .
 * @param   bRmEx               The ModRM byte but with bit 3 set to REX.B and
 *                              bit 4 to REX.X.  The two bits are part of the
 *                              REG sub-field, which isn't needed in this
 *                              function.
 * @param   uSibAndRspOffset    Two parts:
 *                                - The first 8 bits make up the SIB byte.
 *                                - The next 8 bits are the fixed RSP/ESP offset
 *                                  in case of a pop [xSP].
 * @param   u32Disp             The displacement byte/word/dword, if any.
 * @param   cbInstr             The size of the fully decoded instruction. Used
 *                              for RIP relative addressing.
 * @param   idxVarRet           The result variable number.
 * @param   f64Bit              Whether to use a 64-bit or 32-bit address size
 *                              when calculating the address.
 *
 * @see iemOpHlpCalcRmEffAddrThreadedAddr64
 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitCalcRmEffAddrThreadedAddr64(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t bRmEx, uint32_t uSibAndRspOffset,
                                         uint32_t u32Disp, uint8_t cbInstr, uint8_t idxVarRet, bool f64Bit)
{
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarRet);

    /*
     * Special case the rip + disp32 form first.
     */
    if ((bRmEx & (X86_MODRM_MOD_MASK | X86_MODRM_RM_MASK)) == 5)
    {
        uint8_t const idxRegRet = iemNativeVarRegisterAcquire(pReNative, idxVarRet, &off);
        uint8_t const idxRegPc  = iemNativeRegAllocTmpForGuestReg(pReNative, &off, kIemNativeGstReg_Pc,
                                                                  kIemNativeGstRegUse_ReadOnly);
#ifdef RT_ARCH_AMD64
        if (f64Bit)
        {
            int64_t const offFinalDisp = (int64_t)(int32_t)u32Disp + cbInstr;
            if ((int32_t)offFinalDisp == offFinalDisp)
                off = iemNativeEmitLoadGprFromGprWithAddendMaybeZero(pReNative, off, idxRegRet, idxRegPc, (int32_t)offFinalDisp);
            else
            {
                off = iemNativeEmitLoadGprFromGprWithAddend(pReNative, off, idxRegRet, idxRegPc, (int32_t)u32Disp);
                off = iemNativeEmitAddGprImm8(pReNative, off, idxRegRet, cbInstr);
            }
        }
        else
            off = iemNativeEmitLoadGprFromGpr32WithAddendMaybeZero(pReNative, off, idxRegRet, idxRegPc, (int32_t)u32Disp + cbInstr);

#elif defined(RT_ARCH_ARM64)
        if (f64Bit)
            off = iemNativeEmitLoadGprFromGprWithAddendMaybeZero(pReNative, off, idxRegRet, idxRegPc,
                                                                 (int64_t)(int32_t)u32Disp + cbInstr);
        else
            off = iemNativeEmitLoadGprFromGpr32WithAddendMaybeZero(pReNative, off, idxRegRet, idxRegPc,
                                                                   (int32_t)u32Disp + cbInstr);

#else
# error "Port me!"
#endif
        iemNativeRegFreeTmp(pReNative, idxRegPc);
        iemNativeVarRegisterRelease(pReNative, idxVarRet);
        return off;
    }

    /* Calculate the fixed displacement (more down in SIB.B=4 and SIB.B=5 on this). */
    int64_t i64EffAddr = 0;
    switch ((bRmEx >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK)
    {
        case 0: break;
        case 1: i64EffAddr = (int8_t)u32Disp; break;
        case 2: i64EffAddr = (int32_t)u32Disp; break;
        default: AssertFailed();
    }

    /* Get the register (or SIB) value. */
    uint8_t idxGstRegBase  = UINT8_MAX;
    uint8_t idxGstRegIndex = UINT8_MAX;
    uint8_t cShiftIndex    = 0;
    if ((bRmEx & X86_MODRM_RM_MASK) != 4)
        idxGstRegBase = bRmEx & (X86_MODRM_RM_MASK | 0x8); /* bRmEx[bit 3] = REX.B */
    else /* SIB: */
    {
        /* index /w scaling . */
        cShiftIndex    = (uSibAndRspOffset >> X86_SIB_SCALE_SHIFT) & X86_SIB_SCALE_SMASK;
        idxGstRegIndex = ((uSibAndRspOffset >> X86_SIB_INDEX_SHIFT) & X86_SIB_INDEX_SMASK)
                       | ((bRmEx & 0x10) >> 1); /* bRmEx[bit 4] = REX.X */
        if (idxGstRegIndex == 4)
        {
            /* no index */
            cShiftIndex    = 0;
            idxGstRegIndex = UINT8_MAX;
        }

        /* base */
        idxGstRegBase = (uSibAndRspOffset & X86_SIB_BASE_MASK) | (bRmEx & 0x8); /* bRmEx[bit 3] = REX.B */
        if (idxGstRegBase == 4)
        {
            /* pop [rsp] hack */
            i64EffAddr += uSibAndRspOffset >> 8; /* (this is why i64EffAddr must be 64-bit) */
        }
        else if (   (idxGstRegBase & X86_SIB_BASE_MASK) == 5
                 && (bRmEx & X86_MODRM_MOD_MASK) == 0)
        {
            /* mod=0 and base=5 -> disp32, no base reg. */
            Assert(i64EffAddr == 0);
            i64EffAddr    = (int32_t)u32Disp;
            idxGstRegBase = UINT8_MAX;
        }
    }

    /*
     * If no registers are involved (SIB.B=5, SIB.X=4) repeat what we did at
     * the start of the function.
     */
    if (idxGstRegBase == UINT8_MAX && idxGstRegIndex == UINT8_MAX)
    {
        if (f64Bit)
            iemNativeVarSetKindToConst(pReNative, idxVarRet, (uint64_t)i64EffAddr);
        else
            iemNativeVarSetKindToConst(pReNative, idxVarRet, (uint32_t)i64EffAddr);
        return off;
    }

    /*
     * Now emit code that calculates:
     *      idxRegRet = (uint64_t)(i64EffAddr [+ idxGstRegBase] [+ (idxGstRegIndex << cShiftIndex)])
     * or if !f64Bit:
     *      idxRegRet = (uint32_t)(i64EffAddr [+ idxGstRegBase] [+ (idxGstRegIndex << cShiftIndex)])
     */
    uint8_t const idxRegRet   = iemNativeVarRegisterAcquire(pReNative, idxVarRet, &off);
    uint8_t       idxRegBase  = idxGstRegBase == UINT8_MAX ? UINT8_MAX
                              : iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGstRegBase),
                                                                kIemNativeGstRegUse_ReadOnly);
    uint8_t       idxRegIndex = idxGstRegIndex == UINT8_MAX ? UINT8_MAX
                              : iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGstRegIndex),
                                                               kIemNativeGstRegUse_ReadOnly);

    /* If base is not given and there is no shifting, swap the registers to avoid code duplication. */
    if (idxRegBase == UINT8_MAX && cShiftIndex == 0)
    {
        idxRegBase  = idxRegIndex;
        idxRegIndex = UINT8_MAX;
    }

#ifdef RT_ARCH_AMD64
    uint8_t bFinalAdj;
    if (!f64Bit || (int32_t)i64EffAddr == i64EffAddr)
        bFinalAdj = 0; /* likely */
    else
    {
        /* pop [rsp] with a problematic disp32 value.  Split out the
           RSP offset and add it separately afterwards (bFinalAdj). */
        /** @todo testcase: pop [rsp] with problematic disp32 (mod4).   */
        Assert(idxGstRegBase == X86_GREG_xSP);
        Assert(((bRmEx >> X86_MODRM_MOD_SHIFT) & X86_MODRM_MOD_SMASK) == X86_MOD_MEM4);
        bFinalAdj   = (uint8_t)(uSibAndRspOffset >> 8);
        Assert(bFinalAdj != 0);
        i64EffAddr -= bFinalAdj;
        Assert((int32_t)i64EffAddr == i64EffAddr);
    }
    uint32_t const u32EffAddr = (uint32_t)i64EffAddr;
//pReNative->pInstrBuf[off++] = 0xcc;

    if (idxRegIndex == UINT8_MAX)
    {
        if (u32EffAddr == 0)
        {
            /* mov ret, base */
            if (f64Bit)
                off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegRet, idxRegBase);
            else
                off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxRegRet, idxRegBase);
        }
        else
        {
            /* lea ret, [base + disp32] */
            Assert(idxRegBase != X86_GREG_xSP /*SIB*/);
            uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
            if (f64Bit || idxRegRet >= 8 || idxRegBase >= 8)
                pbCodeBuf[off++] = (idxRegRet  >= 8 ? X86_OP_REX_R : 0)
                                 | (idxRegBase >= 8 ? X86_OP_REX_B : 0)
                                 | (f64Bit          ? X86_OP_REX_W : 0);
            pbCodeBuf[off++] = 0x8d;
            uint8_t const bMod = (int8_t)u32EffAddr == (int32_t)u32EffAddr ? X86_MOD_MEM1 : X86_MOD_MEM4;
            if (idxRegBase != X86_GREG_x12 /*SIB*/)
                pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, idxRegBase & 7);
            else
            {
                pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, 4 /*SIB*/);
                pbCodeBuf[off++] = X86_SIB_MAKE(X86_GREG_x12 & 7, 4 /*no index*/, 0);
            }
            pbCodeBuf[off++] = RT_BYTE1(u32EffAddr);
            if (bMod == X86_MOD_MEM4)
            {
                pbCodeBuf[off++] = RT_BYTE2(u32EffAddr);
                pbCodeBuf[off++] = RT_BYTE3(u32EffAddr);
                pbCodeBuf[off++] = RT_BYTE4(u32EffAddr);
            }
            IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
        }
    }
    else
    {
        Assert(idxRegIndex != X86_GREG_xSP /*no-index*/);
        uint8_t * const pbCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 8);
        if (idxRegBase == UINT8_MAX)
        {
            /* lea ret, [(index64 << cShiftIndex) + disp32] */
            if (f64Bit || idxRegRet >= 8 || idxRegIndex >= 8)
                pbCodeBuf[off++] = (idxRegRet   >= 8 ? X86_OP_REX_R : 0)
                                 | (idxRegIndex >= 8 ? X86_OP_REX_X : 0)
                                 | (f64Bit           ? X86_OP_REX_W : 0);
            pbCodeBuf[off++] = 0x8d;
            pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM0, idxRegRet & 7, 4 /*SIB*/);
            pbCodeBuf[off++] = X86_SIB_MAKE(5 /*nobase/bp*/, idxRegIndex & 7, cShiftIndex);
            pbCodeBuf[off++] = RT_BYTE1(u32EffAddr);
            pbCodeBuf[off++] = RT_BYTE2(u32EffAddr);
            pbCodeBuf[off++] = RT_BYTE3(u32EffAddr);
            pbCodeBuf[off++] = RT_BYTE4(u32EffAddr);
        }
        else
        {
            /* lea ret, [(index64 << cShiftIndex) + base64 (+ disp32)] */
            if (f64Bit || idxRegRet >= 8 || idxRegBase >= 8 || idxRegIndex >= 8)
                pbCodeBuf[off++] = (idxRegRet   >= 8 ? X86_OP_REX_R : 0)
                                 | (idxRegBase  >= 8 ? X86_OP_REX_B : 0)
                                 | (idxRegIndex >= 8 ? X86_OP_REX_X : 0)
                                 | (f64Bit           ? X86_OP_REX_W : 0);
            pbCodeBuf[off++] = 0x8d;
            uint8_t const bMod = u32EffAddr == 0 && (idxRegBase & 7) != X86_GREG_xBP ? X86_MOD_MEM0
                               : (int8_t)u32EffAddr == (int32_t)u32EffAddr           ? X86_MOD_MEM1 : X86_MOD_MEM4;
            pbCodeBuf[off++] = X86_MODRM_MAKE(bMod, idxRegRet & 7, 4 /*SIB*/);
            pbCodeBuf[off++] = X86_SIB_MAKE(idxRegBase & 7, idxRegIndex & 7, cShiftIndex);
            if (bMod != X86_MOD_MEM0)
            {
                pbCodeBuf[off++] = RT_BYTE1(u32EffAddr);
                if (bMod == X86_MOD_MEM4)
                {
                    pbCodeBuf[off++] = RT_BYTE2(u32EffAddr);
                    pbCodeBuf[off++] = RT_BYTE3(u32EffAddr);
                    pbCodeBuf[off++] = RT_BYTE4(u32EffAddr);
                }
            }
        }
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }

    if (!bFinalAdj)
    { /* likely */ }
    else
    {
        Assert(f64Bit);
        off = iemNativeEmitAddGprImm8(pReNative, off, idxRegRet, bFinalAdj);
    }

#elif defined(RT_ARCH_ARM64)
    if (i64EffAddr == 0)
    {
        uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
        if (idxRegIndex == UINT8_MAX)
            pu32CodeBuf[off++] = Armv8A64MkInstrMov(idxRegRet, idxRegBase, f64Bit);
        else if (idxRegBase != UINT8_MAX)
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegBase, idxRegIndex,
                                                          f64Bit, false /*fSetFlags*/, cShiftIndex);
        else
        {
            Assert(cShiftIndex != 0); /* See base = index swap above when shift is 0 and we have no base reg. */
            pu32CodeBuf[off++] = Armv8A64MkInstrLslImm(idxRegRet, idxRegIndex, cShiftIndex, f64Bit);
        }
    }
    else
    {
        if (f64Bit)
        { /* likely */ }
        else
            i64EffAddr = (int32_t)i64EffAddr;

        if (i64EffAddr < 4096 && i64EffAddr >= 0 && idxRegBase != UINT8_MAX)
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(false /*fSub*/, idxRegRet, idxRegBase, i64EffAddr, f64Bit);
        }
        else if (i64EffAddr > -4096 && i64EffAddr < 0 && idxRegBase != UINT8_MAX)
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubUImm12(true /*fSub*/, idxRegRet, idxRegBase, (uint32_t)-i64EffAddr, f64Bit);
        }
        else
        {
            if (f64Bit)
                off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegRet, i64EffAddr);
            else
                off = iemNativeEmitLoadGprImm64(pReNative, off, idxRegRet, (uint32_t)i64EffAddr);
            if (idxRegBase != UINT8_MAX)
            {
                uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
                pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegRet, idxRegBase, f64Bit);
            }
        }
        if (idxRegIndex != UINT8_MAX)
        {
            uint32_t * const pu32CodeBuf = iemNativeInstrBufEnsure(pReNative, off, 1);
            pu32CodeBuf[off++] = Armv8A64MkInstrAddSubReg(false /*fSub*/, idxRegRet, idxRegRet, idxRegIndex,
                                                          f64Bit, false /*fSetFlags*/, cShiftIndex);
        }
    }

#else
# error "port me"
#endif

    if (idxRegIndex != UINT8_MAX)
        iemNativeRegFreeTmp(pReNative, idxRegIndex);
    if (idxRegBase != UINT8_MAX)
        iemNativeRegFreeTmp(pReNative, idxRegBase);
    iemNativeVarRegisterRelease(pReNative, idxVarRet);
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
*   Memory fetches and stores common                                                                                             *
*********************************************************************************************************************************/

typedef enum IEMNATIVEMITMEMOP
{
    kIemNativeEmitMemOp_Store = 0,
    kIemNativeEmitMemOp_Fetch,
    kIemNativeEmitMemOp_Fetch_Zx_U16,
    kIemNativeEmitMemOp_Fetch_Zx_U32,
    kIemNativeEmitMemOp_Fetch_Zx_U64,
    kIemNativeEmitMemOp_Fetch_Sx_U16,
    kIemNativeEmitMemOp_Fetch_Sx_U32,
    kIemNativeEmitMemOp_Fetch_Sx_U64
} IEMNATIVEMITMEMOP;

/** Emits code for IEM_MC_FETCH_MEM_U8/16/32/64 and IEM_MC_STORE_MEM_U8/16/32/64,
 * and IEM_MC_FETCH_MEM_FLAT_U8/16/32/64 and IEM_MC_STORE_MEM_FLAT_U8/16/32/64
 * (with iSegReg = UINT8_MAX). */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitMemFetchStoreDataCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off,  uint8_t idxVarValue, uint8_t iSegReg,
                                     uint8_t idxVarGCPtrMem, uint8_t cbMem, uint8_t fAlignMask, IEMNATIVEMITMEMOP enmOp,
                                     uintptr_t pfnFunction, uint8_t idxInstr, uint8_t offDisp = 0)
{
    /*
     * Assert sanity.
     */
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarValue);
    Assert(   enmOp != kIemNativeEmitMemOp_Store
           || pReNative->Core.aVars[idxVarValue].enmKind == kIemNativeVarKind_Immediate
           || pReNative->Core.aVars[idxVarValue].enmKind == kIemNativeVarKind_Stack);
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarGCPtrMem);
    AssertStmt(   pReNative->Core.aVars[idxVarGCPtrMem].enmKind == kIemNativeVarKind_Immediate
               || pReNative->Core.aVars[idxVarGCPtrMem].enmKind == kIemNativeVarKind_Stack,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));
    Assert(iSegReg < 6 || iSegReg == UINT8_MAX);
    Assert(cbMem == 1 || cbMem == 2 || cbMem == 4 || cbMem == 8);
    AssertCompile(IEMNATIVE_CALL_ARG_GREG_COUNT >= 4);
#ifdef VBOX_STRICT
    if (iSegReg == UINT8_MAX)
    {
        Assert(   (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_64BIT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_PROT_FLAT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_FLAT);
        switch (cbMem)
        {
            case 1:
                Assert(   pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemFlatStoreDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U16 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U32 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U64 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U16 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8_Sx_U16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U32 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8_Sx_U32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U64 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU8_Sx_U64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
            case 2:
                Assert(   pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemFlatStoreDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFlatFetchDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U32 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U64 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U32 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU16_Sx_U32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U64 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU16_Sx_U64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
            case 4:
                Assert(   pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemFlatStoreDataU32
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFlatFetchDataU32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U64 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U64 ? (uintptr_t)iemNativeHlpMemFlatFetchDataU32_Sx_U64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
            case 8:
                Assert(    pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemFlatStoreDataU64
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFlatFetchDataU64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
        }
    }
    else
    {
        Assert(iSegReg < 6);
        switch (cbMem)
        {
            case 1:
                Assert(   pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemStoreDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U16 ? (uintptr_t)iemNativeHlpMemFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U32 ? (uintptr_t)iemNativeHlpMemFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U64 ? (uintptr_t)iemNativeHlpMemFetchDataU8
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U16 ? (uintptr_t)iemNativeHlpMemFetchDataU8_Sx_U16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U32 ? (uintptr_t)iemNativeHlpMemFetchDataU8_Sx_U32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U64 ? (uintptr_t)iemNativeHlpMemFetchDataU8_Sx_U64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
            case 2:
                Assert(   pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemStoreDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFetchDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U32 ? (uintptr_t)iemNativeHlpMemFetchDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U64 ? (uintptr_t)iemNativeHlpMemFetchDataU16
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U32 ? (uintptr_t)iemNativeHlpMemFetchDataU16_Sx_U32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U64 ? (uintptr_t)iemNativeHlpMemFetchDataU16_Sx_U64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
            case 4:
                Assert(   pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemStoreDataU32
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFetchDataU32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Zx_U64 ? (uintptr_t)iemNativeHlpMemFetchDataU32
                           : enmOp == kIemNativeEmitMemOp_Fetch_Sx_U64 ? (uintptr_t)iemNativeHlpMemFetchDataU32_Sx_U64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
            case 8:
                Assert(    pfnFunction
                       == (  enmOp == kIemNativeEmitMemOp_Store        ? (uintptr_t)iemNativeHlpMemStoreDataU64
                           : enmOp == kIemNativeEmitMemOp_Fetch        ? (uintptr_t)iemNativeHlpMemFetchDataU64
                           : UINT64_C(0xc000b000a0009000) ));
                break;
        }
    }
#endif

#ifdef VBOX_STRICT
    /*
     * Check that the fExec flags we've got make sense.
     */
    off = iemNativeEmitExecFlagsCheck(pReNative, off, pReNative->fExec);
#endif

    /*
     * To keep things simple we have to commit any pending writes first as we
     * may end up making calls.
     */
    /** @todo we could postpone this till we make the call and reload the
     * registers after returning from the call. Not sure if that's sensible or
     * not, though. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

#ifdef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
    /*
     * Move/spill/flush stuff out of call-volatile registers.
     * This is the easy way out. We could contain this to the tlb-miss branch
     * by saving and restoring active stuff here.
     */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 0 /* vacate all non-volatile regs */);
#endif

    /*
     * Define labels and allocate the result register (trying for the return
     * register if we can).
     */
    uint16_t const uTlbSeqNo         = pReNative->uTlbSeqNo++;
    uint8_t  const idxRegValueFetch  = enmOp == kIemNativeEmitMemOp_Store ? UINT8_MAX
                                     : !(pReNative->Core.bmHstRegs & RT_BIT_32(IEMNATIVE_CALL_RET_GREG))
                                     ? iemNativeVarRegisterSetAndAcquire(pReNative, idxVarValue, IEMNATIVE_CALL_RET_GREG, &off)
                                     : iemNativeVarRegisterAcquire(pReNative, idxVarValue, &off);
    IEMNATIVEEMITTLBSTATE const TlbState(pReNative, &off, idxVarGCPtrMem, iSegReg, cbMem, offDisp);
    uint8_t  const idxRegValueStore  =    !TlbState.fSkip
                                       && enmOp == kIemNativeEmitMemOp_Store
                                       && pReNative->Core.aVars[idxVarValue].enmKind != kIemNativeVarKind_Immediate
                                     ? iemNativeVarRegisterAcquire(pReNative, idxVarValue, &off)
                                     : UINT8_MAX;
    uint32_t const idxRegMemResult   = !TlbState.fSkip ? iemNativeRegAllocTmp(pReNative, &off) : UINT8_MAX;
    uint32_t const idxLabelTlbLookup = !TlbState.fSkip
                                     ? iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbLookup, UINT32_MAX, uTlbSeqNo)
                                     : UINT32_MAX;

    /*
     * Jump to the TLB lookup code.
     */
    if (!TlbState.fSkip)
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbLookup); /** @todo short jump */

    /*
     * TlbMiss:
     *
     * Call helper to do the fetching.
     * We flush all guest register shadow copies here.
     */
    uint32_t const idxLabelTlbMiss = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbMiss, off, uTlbSeqNo);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

#ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
    /* Save variables in volatile registers. */
    uint32_t const fHstRegsNotToSave = TlbState.getRegsNotToSave()
                                     | (idxRegMemResult  != UINT8_MAX ? RT_BIT_32(idxRegMemResult)  : 0)
                                     | (idxRegValueFetch != UINT8_MAX ? RT_BIT_32(idxRegValueFetch) : 0);
    off = iemNativeVarSaveVolatileRegsPreHlpCall(pReNative, off, fHstRegsNotToSave);
#endif

    /* IEMNATIVE_CALL_ARG2/3_GREG = uValue (idxVarValue) - if store */
    uint32_t fVolGregMask = IEMNATIVE_CALL_VOLATILE_GREG_MASK;
    if (enmOp == kIemNativeEmitMemOp_Store)
    {
        uint8_t const idxRegArgValue = iSegReg == UINT8_MAX ? IEMNATIVE_CALL_ARG2_GREG : IEMNATIVE_CALL_ARG3_GREG;
        off = iemNativeEmitLoadArgGregFromImmOrStackVar(pReNative, off, idxRegArgValue, idxVarValue, 0 /*cbAppend*/,
#ifdef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
                                                        IEMNATIVE_CALL_VOLATILE_GREG_MASK);
#else
                                                        IEMNATIVE_CALL_VOLATILE_GREG_MASK, true /*fSpilledVarsInvolatileRegs*/);
        fVolGregMask &= ~RT_BIT_32(idxRegArgValue);
#endif
    }

    /* IEMNATIVE_CALL_ARG1_GREG = GCPtrMem */
    off = iemNativeEmitLoadArgGregFromImmOrStackVar(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxVarGCPtrMem, offDisp /*cbAppend*/,
#ifdef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
                                                    fVolGregMask);
#else
                                                    fVolGregMask, true /*fSpilledVarsInvolatileRegs*/);
#endif

    if (iSegReg != UINT8_MAX)
    {
        /* IEMNATIVE_CALL_ARG2_GREG = iSegReg */
        AssertStmt(iSegReg < 6, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_EMIT_BAD_SEG_REG_NO));
        off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, iSegReg);
    }

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, pfnFunction);

    /*
     * Put the result in the right register if this is a fetch.
     */
    if (enmOp != kIemNativeEmitMemOp_Store)
    {
        Assert(idxRegValueFetch == pReNative->Core.aVars[idxVarValue].idxReg);
        if (idxRegValueFetch != IEMNATIVE_CALL_RET_GREG)
            off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegValueFetch, IEMNATIVE_CALL_RET_GREG);
    }

#ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
    /* Restore variables and guest shadow registers to volatile registers. */
    off = iemNativeVarRestoreVolatileRegsPostHlpCall(pReNative, off, fHstRegsNotToSave);
    off = iemNativeRegRestoreGuestShadowsInVolatileRegs(pReNative, off, TlbState.getActiveRegsWithShadows());
#endif

#ifdef IEMNATIVE_WITH_TLB_LOOKUP
    if (!TlbState.fSkip)
    {
        /* end of TlbMiss - Jump to the done label. */
        uint32_t const idxLabelTlbDone = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbDone, UINT32_MAX, uTlbSeqNo);
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbDone);

        /*
         * TlbLookup:
         */
        off = iemNativeEmitTlbLookup<true>(pReNative, off, &TlbState, iSegReg, cbMem, fAlignMask,
                                           enmOp == kIemNativeEmitMemOp_Store ? IEM_ACCESS_TYPE_WRITE : IEM_ACCESS_TYPE_READ,
                                           idxLabelTlbLookup, idxLabelTlbMiss, idxRegMemResult, offDisp);

        /*
         * Emit code to do the actual storing / fetching.
         */
        PIEMNATIVEINSTR pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 64);
# ifdef VBOX_WITH_STATISTICS
        off = iemNativeEmitIncStamCounterInVCpuEx(pCodeBuf, off, TlbState.idxReg1, TlbState.idxReg2,
                                                  enmOp == kIemNativeEmitMemOp_Store
                                                  ? RT_UOFFSETOF(VMCPUCC, iem.s.StatNativeTlbHitsForFetch)
                                                  : RT_UOFFSETOF(VMCPUCC, iem.s.StatNativeTlbHitsForStore));
# endif
        switch (enmOp)
        {
            case kIemNativeEmitMemOp_Store:
                if (pReNative->Core.aVars[idxVarValue].enmKind != kIemNativeVarKind_Immediate)
                {
                    switch (cbMem)
                    {
                        case 1:
                            off = iemNativeEmitStoreGpr8ByGprEx(pCodeBuf, off, idxRegValueStore, idxRegMemResult);
                            break;
                        case 2:
                            off = iemNativeEmitStoreGpr16ByGprEx(pCodeBuf, off, idxRegValueStore, idxRegMemResult);
                            break;
                        case 4:
                            off = iemNativeEmitStoreGpr32ByGprEx(pCodeBuf, off, idxRegValueStore, idxRegMemResult);
                            break;
                        case 8:
                            off = iemNativeEmitStoreGpr64ByGprEx(pCodeBuf, off, idxRegValueStore, idxRegMemResult);
                            break;
                        default:
                            AssertFailed();
                    }
                }
                else
                {
                    switch (cbMem)
                    {
                        case 1:
                            off = iemNativeEmitStoreImm8ByGprEx(pCodeBuf, off,
                                                                (uint8_t)pReNative->Core.aVars[idxVarValue].u.uValue,
                                                                idxRegMemResult, TlbState.idxReg1);
                            break;
                        case 2:
                            off = iemNativeEmitStoreImm16ByGprEx(pCodeBuf, off,
                                                                 (uint16_t)pReNative->Core.aVars[idxVarValue].u.uValue,
                                                                 idxRegMemResult, TlbState.idxReg1);
                            break;
                        case 4:
                            off = iemNativeEmitStoreImm32ByGprEx(pCodeBuf, off,
                                                                 (uint32_t)pReNative->Core.aVars[idxVarValue].u.uValue,
                                                                 idxRegMemResult, TlbState.idxReg1);
                            break;
                        case 8:
                            off = iemNativeEmitStoreImm64ByGprEx(pCodeBuf, off, pReNative->Core.aVars[idxVarValue].u.uValue,
                                                                 idxRegMemResult, TlbState.idxReg1);
                            break;
                        default:
                            AssertFailed();
                    }
                }
                break;

            case kIemNativeEmitMemOp_Fetch:
            case kIemNativeEmitMemOp_Fetch_Zx_U16:
            case kIemNativeEmitMemOp_Fetch_Zx_U32:
            case kIemNativeEmitMemOp_Fetch_Zx_U64:
                switch (cbMem)
                {
                    case 1:
                        off = iemNativeEmitLoadGprByGprU8Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    case 2:
                        off = iemNativeEmitLoadGprByGprU16Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    case 4:
                        off = iemNativeEmitLoadGprByGprU32Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    case 8:
                        off = iemNativeEmitLoadGprByGprU64Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    default:
                        AssertFailed();
                }
                break;

            case kIemNativeEmitMemOp_Fetch_Sx_U16:
                Assert(cbMem == 1);
                off = iemNativeEmitLoadGprByGprU16SignExtendedFromS8Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                break;

            case kIemNativeEmitMemOp_Fetch_Sx_U32:
                Assert(cbMem == 1 || cbMem == 2);
                if (cbMem == 1)
                    off = iemNativeEmitLoadGprByGprU32SignExtendedFromS8Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                else
                    off = iemNativeEmitLoadGprByGprU32SignExtendedFromS16Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                break;

            case kIemNativeEmitMemOp_Fetch_Sx_U64:
                switch (cbMem)
                {
                    case 1:
                        off = iemNativeEmitLoadGprByGprU64SignExtendedFromS8Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    case 2:
                        off = iemNativeEmitLoadGprByGprU64SignExtendedFromS16Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    case 4:
                        off = iemNativeEmitLoadGprByGprU64SignExtendedFromS32Ex(pCodeBuf, off, idxRegValueFetch, idxRegMemResult);
                        break;
                    default:
                        AssertFailed();
                }
                break;

            default:
                AssertFailed();
        }

        iemNativeRegFreeTmp(pReNative, idxRegMemResult);

        /*
         * TlbDone:
         */
        iemNativeLabelDefine(pReNative, idxLabelTlbDone, off);

        TlbState.freeRegsAndReleaseVars(pReNative, idxVarGCPtrMem);

# ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
        /* Temp Hack: Flush all guest shadows in volatile registers in case of TLB miss. */
        iemNativeRegFlushGuestShadowsByHostMask(pReNative, IEMNATIVE_CALL_VOLATILE_GREG_MASK);
# endif
    }
#else
    RT_NOREF(fAlignMask, idxLabelTlbMiss);
#endif

    if (idxRegValueFetch != UINT8_MAX || idxRegValueStore != UINT8_MAX)
        iemNativeVarRegisterRelease(pReNative, idxVarValue);
    return off;
}



/*********************************************************************************************************************************
*   Memory fetches (IEM_MEM_FETCH_XXX).                                                                                          *
*********************************************************************************************************************************/

/* 8-bit segmented: */
#define IEM_MC_FETCH_MEM_U8(a_u8Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u8Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U8_ZX_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Zx_U16, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U8_ZX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Zx_U32, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U8_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Zx_U64, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U8_SX_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Sx_U16, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8_Sx_U16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U8_SX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Sx_U32, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8_Sx_U32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U8_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Sx_U64, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU8_Sx_U64, pCallEntry->idxInstr)

/* 16-bit segmented: */
#define IEM_MC_FETCH_MEM_U16(a_u16Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U16_DISP(a_u16Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU16, pCallEntry->idxInstr, a_offDisp)

#define IEM_MC_FETCH_MEM_U16_ZX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Zx_U32, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U16_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Zx_U64, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U16_SX_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Sx_U32, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU16_Sx_U32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U16_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Sx_U64, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU16_Sx_U64, pCallEntry->idxInstr)


/* 32-bit segmented: */
#define IEM_MC_FETCH_MEM_U32(a_u32Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U32_DISP(a_u32Dst, a_iSeg, a_GCPtrMem, a_offDisp) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU32, pCallEntry->idxInstr, a_offDisp)

#define IEM_MC_FETCH_MEM_U32_ZX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch_Zx_U64, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_U32_SX_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch_Sx_U64, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU32_Sx_U64, pCallEntry->idxInstr)


/* 64-bit segmented: */
#define IEM_MC_FETCH_MEM_U64(a_u64Dst, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint64_t), sizeof(uint64_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFetchDataU64, pCallEntry->idxInstr)



/* 8-bit flat: */
#define IEM_MC_FETCH_MEM_FLAT_U8(a_u8Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u8Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U8_ZX_U16(a_u16Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Zx_U16, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U8_ZX_U32(a_u32Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Zx_U32, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U8_ZX_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Zx_U64, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U8_SX_U16(a_u16Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Sx_U16, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8_Sx_U16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U8_SX_U32(a_u32Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Sx_U32, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8_Sx_U32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U8_SX_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Fetch_Sx_U64, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU8_Sx_U64, pCallEntry->idxInstr)


/* 16-bit flat: */
#define IEM_MC_FETCH_MEM_FLAT_U16(a_u16Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U16_DISP(a_u16Dst, a_GCPtrMem, a_offDisp) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU16, pCallEntry->idxInstr, a_offDisp)

#define IEM_MC_FETCH_MEM_FLAT_U16_ZX_U32(a_u32Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Zx_U32, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U16_ZX_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Zx_U64, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU16, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U16_SX_U32(a_u32Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Sx_U32, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU16_Sx_U32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U16_SX_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Fetch_Sx_U64, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU16_Sx_U64, pCallEntry->idxInstr)

/* 32-bit flat: */
#define IEM_MC_FETCH_MEM_FLAT_U32(a_u32Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U32_DISP(a_u32Dst, a_GCPtrMem, a_offDisp) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU32, pCallEntry->idxInstr, a_offDisp)

#define IEM_MC_FETCH_MEM_FLAT_U32_ZX_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch_Zx_U64, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU32, pCallEntry->idxInstr)

#define IEM_MC_FETCH_MEM_FLAT_U32_SX_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Fetch_Sx_U64, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU32_Sx_U64, pCallEntry->idxInstr)

/* 64-bit flat: */
#define IEM_MC_FETCH_MEM_FLAT_U64(a_u64Dst, a_GCPtrMem) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Dst, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint64_t), sizeof(uint64_t) - 1, kIemNativeEmitMemOp_Fetch, \
                                               (uintptr_t)iemNativeHlpMemFlatFetchDataU64, pCallEntry->idxInstr)



/*********************************************************************************************************************************
*   Memory stores (IEM_MEM_STORE_XXX).                                                                                           *
*********************************************************************************************************************************/

#define IEM_MC_STORE_MEM_U8(a_iSeg, a_GCPtrMem, a_u8Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u8Value, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemStoreDataU8, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_U16(a_iSeg, a_GCPtrMem, a_u16Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Value, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemStoreDataU16, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_U32(a_iSeg, a_GCPtrMem, a_u32Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Value, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemStoreDataU32, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_U64(a_iSeg, a_GCPtrMem, a_u64Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Value, a_iSeg, a_GCPtrMem, \
                                               sizeof(uint64_t), sizeof(uint64_t) - 1, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemStoreDataU64, pCallEntry->idxInstr)


#define IEM_MC_STORE_MEM_FLAT_U8(a_GCPtrMem, a_u8Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u8Value, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint8_t), 0 /*fAlignMask*/, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU8, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_FLAT_U16(a_GCPtrMem, a_u16Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u16Value, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint16_t), sizeof(uint16_t) - 1, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU16, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_FLAT_U32(a_GCPtrMem, a_u32Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u32Value, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint32_t), sizeof(uint32_t) - 1, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU32, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_FLAT_U64(a_GCPtrMem, a_u64Value) \
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, a_u64Value, UINT8_MAX, a_GCPtrMem, \
                                               sizeof(uint64_t), sizeof(uint64_t) - 1, kIemNativeEmitMemOp_Store, \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU64, pCallEntry->idxInstr)


#define IEM_MC_STORE_MEM_U8_CONST(a_iSeg, a_GCPtrMem, a_u8ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u8ConstValue, a_iSeg, a_GCPtrMem, sizeof(uint8_t), \
                                               (uintptr_t)iemNativeHlpMemStoreDataU8, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_U16_CONST(a_iSeg, a_GCPtrMem, a_u16ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u16ConstValue, a_iSeg, a_GCPtrMem, sizeof(uint16_t), \
                                               (uintptr_t)iemNativeHlpMemStoreDataU16, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_U32_CONST(a_iSeg, a_GCPtrMem, a_u32ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u32ConstValue, a_iSeg, a_GCPtrMem, sizeof(uint32_t), \
                                               (uintptr_t)iemNativeHlpMemStoreDataU32, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_U64_CONST(a_iSeg, a_GCPtrMem, a_u64ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u64ConstValue, a_iSeg, a_GCPtrMem, sizeof(uint64_t), \
                                               (uintptr_t)iemNativeHlpMemStoreDataU64, pCallEntry->idxInstr)


#define IEM_MC_STORE_MEM_FLAT_U8_CONST(a_GCPtrMem, a_u8ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u8ConstValue, UINT8_MAX, a_GCPtrMem, sizeof(uint8_t), \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU8, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_FLAT_U16_CONST(a_GCPtrMem, a_u16ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u16ConstValue, UINT8_MAX, a_GCPtrMem, sizeof(uint16_t), \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU16, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_FLAT_U32_CONST(a_GCPtrMem, a_u32ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u32ConstValue, UINT8_MAX, a_GCPtrMem, sizeof(uint32_t), \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU32, pCallEntry->idxInstr)

#define IEM_MC_STORE_MEM_FLAT_U64_CONST(a_GCPtrMem, a_u64ConstValue) \
    off = iemNativeEmitMemStoreConstDataCommon(pReNative, off, a_u64ConstValue, UINT8_MAX, a_GCPtrMem, sizeof(uint64_t), \
                                               (uintptr_t)iemNativeHlpMemFlatStoreDataU64, pCallEntry->idxInstr)

/** Emits code for IEM_MC_STORE_MEM_U8/16/32/64_CONST and
 *  IEM_MC_STORE_MEM_FLAT_U8/16/32/64_CONST (with iSegReg = UINT8_MAX). */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitMemStoreConstDataCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint64_t uValueConst, uint8_t iSegReg,
                                    uint8_t idxVarGCPtrMem, uint8_t cbMem, uintptr_t pfnFunction, uint8_t idxInstr)
{
    /*
     * Create a temporary const variable and call iemNativeEmitMemFetchStoreDataCommon
     * to do the grunt work.
     */
    uint8_t const idxVarConstValue = iemNativeVarAllocConst(pReNative, cbMem, uValueConst);
    off = iemNativeEmitMemFetchStoreDataCommon(pReNative, off, idxVarConstValue, iSegReg, idxVarGCPtrMem,
                                               cbMem, cbMem - 1, kIemNativeEmitMemOp_Store,
                                               pfnFunction, idxInstr);
    iemNativeVarFreeLocal(pReNative, idxVarConstValue);
    return off;
}



/*********************************************************************************************************************************
*   Stack Accesses.                                                                                                              *
*********************************************************************************************************************************/
/*                                                     RT_MAKE_U32_FROM_U8(cBitsVar, cBitsFlat, fSReg, 0) */
#define IEM_MC_PUSH_U16(a_u16Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u16Value, RT_MAKE_U32_FROM_U8(16,  0, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackStoreU16, pCallEntry->idxInstr)
#define IEM_MC_PUSH_U32(a_u32Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u32Value, RT_MAKE_U32_FROM_U8(32,  0, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackStoreU32, pCallEntry->idxInstr)
#define IEM_MC_PUSH_U32_SREG(a_uSegVal) \
    off = iemNativeEmitStackPush(pReNative, off, a_uSegVal,  RT_MAKE_U32_FROM_U8(32,  0, 1, 0), \
                                 (uintptr_t)iemNativeHlpStackStoreU32SReg, pCallEntry->idxInstr)
#define IEM_MC_PUSH_U64(a_u64Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u64Value, RT_MAKE_U32_FROM_U8(64,  0, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackStoreU64, pCallEntry->idxInstr)

#define IEM_MC_FLAT32_PUSH_U16(a_u16Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u16Value, RT_MAKE_U32_FROM_U8(16, 32, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackFlatStoreU16, pCallEntry->idxInstr)
#define IEM_MC_FLAT32_PUSH_U32(a_u32Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u32Value, RT_MAKE_U32_FROM_U8(32, 32, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackFlatStoreU32, pCallEntry->idxInstr)
#define IEM_MC_FLAT32_PUSH_U32_SREG(a_u32Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u32Value, RT_MAKE_U32_FROM_U8(32, 32, 1, 0), \
                                 (uintptr_t)iemNativeHlpStackFlatStoreU32SReg, pCallEntry->idxInstr)

#define IEM_MC_FLAT64_PUSH_U16(a_u16Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u16Value, RT_MAKE_U32_FROM_U8(16, 64, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackFlatStoreU16, pCallEntry->idxInstr)
#define IEM_MC_FLAT64_PUSH_U64(a_u64Value) \
    off = iemNativeEmitStackPush(pReNative, off, a_u64Value, RT_MAKE_U32_FROM_U8(64, 64, 0, 0), \
                                 (uintptr_t)iemNativeHlpStackFlatStoreU64, pCallEntry->idxInstr)


DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitStackPushUse16Sp(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t idxRegRsp, uint8_t idxRegEffSp, uint8_t cbMem)
{
    /* Use16BitSp: */
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitSubGpr16ImmEx(pCodeBuf, off, idxRegRsp, cbMem); /* ASSUMES this does NOT modify bits [63:16]! */
    off = iemNativeEmitLoadGprFromGpr16Ex(pCodeBuf, off, idxRegEffSp, idxRegRsp);
#else
    /* sub regeff, regrsp, #cbMem */
    pCodeBuf[off++] = Armv8A64MkInstrSubUImm12(idxRegEffSp, idxRegRsp, cbMem, false /*f64Bit*/);
    /* and regeff, regeff, #0xffff */
    Assert(Armv8A64ConvertImmRImmS2Mask32(15, 0) == 0xffff);
    pCodeBuf[off++] = Armv8A64MkInstrAndImm(idxRegEffSp, idxRegEffSp, 15, 0,  false /*f64Bit*/);
    /* bfi regrsp, regeff, #0, #16 - moves bits 15:0 from idxVarReg to idxGstTmpReg bits 15:0. */
    pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegRsp, idxRegEffSp, 0, 16, false /*f64Bit*/);
#endif
    return off;
}


DECL_FORCE_INLINE(uint32_t)
iemNativeEmitStackPushUse32Sp(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t idxRegRsp, uint8_t idxRegEffSp, uint8_t cbMem)
{
    /* Use32BitSp: */
    off = iemNativeEmitSubGpr32ImmEx(pCodeBuf, off, idxRegRsp, cbMem);
    off = iemNativeEmitLoadGprFromGpr32Ex(pCodeBuf, off, idxRegEffSp, idxRegRsp);
    return off;
}


/** IEM_MC[|_FLAT32|_FLAT64]_PUSH_U16/32/32_SREG/64 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStackPush(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarValue,
                       uint32_t cBitsVarAndFlat, uintptr_t pfnFunction, uint8_t idxInstr)
{
    /*
     * Assert sanity.
     */
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarValue);
#ifdef VBOX_STRICT
    if (RT_BYTE2(cBitsVarAndFlat) != 0)
    {
        Assert(   (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_64BIT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_PROT_FLAT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_FLAT);
        Assert(   pfnFunction
               == (  cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(16, 32, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatStoreU16
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(32, 32, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatStoreU32
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(32, 32, 1, 0) ? (uintptr_t)iemNativeHlpStackFlatStoreU32SReg
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(16, 64, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatStoreU16
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(64, 64, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatStoreU64
                   : UINT64_C(0xc000b000a0009000) ));
    }
    else
        Assert(   pfnFunction
               == (  cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(16, 0, 0, 0) ? (uintptr_t)iemNativeHlpStackStoreU16
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(32, 0, 0, 0) ? (uintptr_t)iemNativeHlpStackStoreU32
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(32, 0, 1, 0) ? (uintptr_t)iemNativeHlpStackStoreU32SReg
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(64, 0, 0, 0) ? (uintptr_t)iemNativeHlpStackStoreU64
                   : UINT64_C(0xc000b000a0009000) ));
#endif

#ifdef VBOX_STRICT
    /*
     * Check that the fExec flags we've got make sense.
     */
    off = iemNativeEmitExecFlagsCheck(pReNative, off, pReNative->fExec);
#endif

    /*
     * To keep things simple we have to commit any pending writes first as we
     * may end up making calls.
     */
    /** @todo we could postpone this till we make the call and reload the
     * registers after returning from the call. Not sure if that's sensible or
     * not, though. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /*
     * First we calculate the new RSP and the effective stack pointer value.
     * For 64-bit mode and flat 32-bit these two are the same.
     * (Code structure is very similar to that of PUSH)
     */
    uint8_t const cbMem       = RT_BYTE1(cBitsVarAndFlat) / 8;
    bool const    fIsSegReg   = RT_BYTE3(cBitsVarAndFlat) != 0;
    bool const    fIsIntelSeg = fIsSegReg && IEM_IS_GUEST_CPU_INTEL(pReNative->pVCpu);
    uint8_t const cbMemAccess = !fIsIntelSeg || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_16BIT
                              ? cbMem : sizeof(uint16_t);
    uint8_t const cBitsFlat   = RT_BYTE2(cBitsVarAndFlat);      RT_NOREF(cBitsFlat);
    uint8_t const idxRegRsp   = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xSP),
                                                                kIemNativeGstRegUse_ForUpdate, true /*fNoVolatileRegs*/);
    uint8_t const idxRegEffSp = cBitsFlat != 0 ? idxRegRsp : iemNativeRegAllocTmp(pReNative, &off);
    uint32_t      offFixupJumpToUseOtherBitSp = UINT32_MAX;
    if (cBitsFlat != 0)
    {
        Assert(idxRegEffSp == idxRegRsp);
        Assert(cBitsFlat == 32 || cBitsFlat == 64);
        Assert(IEM_F_MODE_X86_IS_FLAT(pReNative->fExec));
        if (cBitsFlat == 64)
            off = iemNativeEmitSubGprImm(pReNative, off, idxRegRsp, cbMem);
        else
            off = iemNativeEmitSubGpr32Imm(pReNative, off, idxRegRsp, cbMem);
    }
    else /** @todo We can skip the test if we're targeting pre-386 CPUs. */
    {
        Assert(idxRegEffSp != idxRegRsp);
        uint8_t const idxRegSsAttr = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_ATTRIB(X86_SREG_SS),
                                                                     kIemNativeGstRegUse_ReadOnly);
#ifdef RT_ARCH_AMD64
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
#else
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
#endif
        off = iemNativeEmitTestAnyBitsInGpr32Ex(pCodeBuf, off, idxRegSsAttr, X86DESCATTR_D);
        iemNativeRegFreeTmp(pReNative, idxRegSsAttr);
        offFixupJumpToUseOtherBitSp = off;
        if ((pReNative->fExec & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_32BIT)
        {
            off = iemNativeEmitJccToFixedEx(pCodeBuf, off, off /*8-bit suffices*/, kIemNativeInstrCond_e); /* jump if zero */
            off = iemNativeEmitStackPushUse32Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem);
        }
        else
        {
            off = iemNativeEmitJccToFixedEx(pCodeBuf, off, off /*8-bit suffices*/, kIemNativeInstrCond_ne); /* jump if not zero */
            off = iemNativeEmitStackPushUse16Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem);
        }
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
    /* SpUpdateEnd: */
    uint32_t const offLabelSpUpdateEnd = off;

    /*
     * Okay, now prepare for TLB lookup and jump to code (or the TlbMiss if
     * we're skipping lookup).
     */
    uint8_t const  iSegReg           = cBitsFlat != 0 ? UINT8_MAX : X86_SREG_SS;
    IEMNATIVEEMITTLBSTATE const TlbState(pReNative, idxRegEffSp, &off, iSegReg, cbMemAccess);
    uint16_t const uTlbSeqNo         = pReNative->uTlbSeqNo++;
    uint32_t const idxLabelTlbMiss   = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbMiss, UINT32_MAX, uTlbSeqNo);
    uint32_t const idxLabelTlbLookup = !TlbState.fSkip
                                     ? iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbLookup, UINT32_MAX, uTlbSeqNo)
                                     : UINT32_MAX;
    uint8_t const  idxRegValue       =    !TlbState.fSkip
                                       && pReNative->Core.aVars[idxVarValue].enmKind != kIemNativeVarKind_Immediate
                                     ? iemNativeVarRegisterAcquire(pReNative, idxVarValue, &off, true /*fInitialized*/,
                                                                   IEMNATIVE_CALL_ARG2_GREG /*idxRegPref*/)
                                     : UINT8_MAX;
    uint8_t const  idxRegMemResult   = !TlbState.fSkip ? iemNativeRegAllocTmp(pReNative, &off) : UINT8_MAX;


    if (!TlbState.fSkip)
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbLookup); /** @todo short jump */
    else
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbMiss); /** @todo short jump */

    /*
     * Use16BitSp:
     */
    if (cBitsFlat == 0)
    {
#ifdef RT_ARCH_AMD64
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
#else
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
#endif
        iemNativeFixupFixedJump(pReNative, offFixupJumpToUseOtherBitSp, off);
        if ((pReNative->fExec & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_32BIT)
            off = iemNativeEmitStackPushUse16Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem);
        else
            off = iemNativeEmitStackPushUse32Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem);
        off = iemNativeEmitJmpToFixedEx(pCodeBuf, off, offLabelSpUpdateEnd);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }

    /*
     * TlbMiss:
     *
     * Call helper to do the pushing.
     */
    iemNativeLabelDefine(pReNative, idxLabelTlbMiss, off);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

    /* Save variables in volatile registers. */
    uint32_t const fHstRegsNotToSave = TlbState.getRegsNotToSave()
                                     | (idxRegMemResult < RT_ELEMENTS(pReNative->Core.aHstRegs) ? RT_BIT_32(idxRegMemResult) : 0)
                                     | (idxRegEffSp != idxRegRsp ? RT_BIT_32(idxRegEffSp) : 0)
                                     | (idxRegValue < RT_ELEMENTS(pReNative->Core.aHstRegs) ? RT_BIT_32(idxRegValue) : 0);
    off = iemNativeVarSaveVolatileRegsPreHlpCall(pReNative, off, fHstRegsNotToSave);

    if (   idxRegValue == IEMNATIVE_CALL_ARG1_GREG
        && idxRegEffSp == IEMNATIVE_CALL_ARG2_GREG)
    {
        /* Swap them using ARG0 as temp register: */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_CALL_ARG1_GREG);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, IEMNATIVE_CALL_ARG2_GREG);
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, IEMNATIVE_CALL_ARG0_GREG);
    }
    else if (idxRegEffSp != IEMNATIVE_CALL_ARG2_GREG)
    {
        /* IEMNATIVE_CALL_ARG2_GREG = idxVarValue (first!) */
        off = iemNativeEmitLoadArgGregFromImmOrStackVar(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, idxVarValue,
                                                        0 /*offAddend*/, IEMNATIVE_CALL_VOLATILE_GREG_MASK);

        /* IEMNATIVE_CALL_ARG1_GREG = idxRegEffSp */
        if (idxRegEffSp != IEMNATIVE_CALL_ARG1_GREG)
            off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxRegEffSp);
    }
    else
    {
        /* IEMNATIVE_CALL_ARG1_GREG = idxRegEffSp (first!) */
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxRegEffSp);

        /* IEMNATIVE_CALL_ARG2_GREG = idxVarValue */
        off = iemNativeEmitLoadArgGregFromImmOrStackVar(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, idxVarValue, 0 /*offAddend*/,
                                                        IEMNATIVE_CALL_VOLATILE_GREG_MASK & ~IEMNATIVE_CALL_ARG1_GREG);
    }

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, pfnFunction);

    /* Restore variables and guest shadow registers to volatile registers. */
    off = iemNativeVarRestoreVolatileRegsPostHlpCall(pReNative, off, fHstRegsNotToSave);
    off = iemNativeRegRestoreGuestShadowsInVolatileRegs(pReNative, off, TlbState.getActiveRegsWithShadows());

#ifdef IEMNATIVE_WITH_TLB_LOOKUP
    if (!TlbState.fSkip)
    {
        /* end of TlbMiss - Jump to the done label. */
        uint32_t const idxLabelTlbDone = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbDone, UINT32_MAX, uTlbSeqNo);
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbDone);

        /*
         * TlbLookup:
         */
        off = iemNativeEmitTlbLookup<true>(pReNative, off, &TlbState, iSegReg, cbMemAccess, cbMemAccess - 1,
                                           IEM_ACCESS_TYPE_WRITE, idxLabelTlbLookup, idxLabelTlbMiss, idxRegMemResult);

        /*
         * Emit code to do the actual storing / fetching.
         */
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 64);
# ifdef VBOX_WITH_STATISTICS
        off = iemNativeEmitIncStamCounterInVCpuEx(pCodeBuf, off, TlbState.idxReg1, TlbState.idxReg2,
                                                  RT_UOFFSETOF(VMCPUCC, iem.s.StatNativeTlbHitsForStack));
# endif
        if (idxRegValue != UINT8_MAX)
        {
            switch (cbMemAccess)
            {
                case 2:
                    off = iemNativeEmitStoreGpr16ByGprEx(pCodeBuf, off, idxRegValue, idxRegMemResult);
                    break;
                case 4:
                    if (!fIsIntelSeg)
                        off = iemNativeEmitStoreGpr32ByGprEx(pCodeBuf, off, idxRegValue, idxRegMemResult);
                    else
                    {
                        /* intel real mode segment push. 10890XE adds the 2nd of half EFLAGS to a
                           PUSH FS in real mode, so we have to try emulate that here.
                           We borrow the now unused idxReg1 from the TLB lookup code here. */
                        uint8_t idxRegEfl = iemNativeRegAllocTmpForGuestRegIfAlreadyPresent(pReNative, &off,
                                                                                            kIemNativeGstReg_EFlags);
                        if (idxRegEfl != UINT8_MAX)
                        {
#ifdef ARCH_AMD64
                            off = iemNativeEmitLoadGprFromGpr32(pReNative, off, TlbState.idxReg1, idxRegEfl);
                            off = iemNativeEmitAndGpr32ByImm(pReNative, off, TlbState.idxReg1,
                                                             UINT32_C(0xffff0000) & ~X86_EFL_RAZ_MASK);
#else
                            off = iemNativeEmitGpr32EqGprAndImmEx(iemNativeInstrBufEnsure(pReNative, off, 3),
                                                                  off, TlbState.idxReg1, idxRegEfl,
                                                                  UINT32_C(0xffff0000) & ~X86_EFL_RAZ_MASK);
#endif
                            iemNativeRegFreeTmp(pReNative, idxRegEfl);
                        }
                        else
                        {
                            off = iemNativeEmitLoadGprFromVCpuU32(pReNative, off, TlbState.idxReg1,
                                                                  RT_UOFFSETOF(VMCPUCC, cpum.GstCtx.eflags));
                            off = iemNativeEmitAndGpr32ByImm(pReNative, off, TlbState.idxReg1,
                                                             UINT32_C(0xffff0000) & ~X86_EFL_RAZ_MASK);
                        }
                        /* ASSUMES the upper half of idxRegValue is ZERO. */
                        off = iemNativeEmitOrGpr32ByGpr(pReNative, off, TlbState.idxReg1, idxRegValue);
                        off = iemNativeEmitStoreGpr32ByGprEx(pCodeBuf, off, TlbState.idxReg1, idxRegMemResult);
                    }
                    break;
                case 8:
                    off = iemNativeEmitStoreGpr64ByGprEx(pCodeBuf, off, idxRegValue, idxRegMemResult);
                    break;
                default:
                    AssertFailed();
            }
        }
        else
        {
            switch (cbMemAccess)
            {
                case 2:
                    off = iemNativeEmitStoreImm16ByGprEx(pCodeBuf, off,
                                                         (uint16_t)pReNative->Core.aVars[idxVarValue].u.uValue,
                                                         idxRegMemResult, TlbState.idxReg1);
                    break;
                case 4:
                    Assert(!fIsSegReg);
                    off = iemNativeEmitStoreImm32ByGprEx(pCodeBuf, off,
                                                         (uint32_t)pReNative->Core.aVars[idxVarValue].u.uValue,
                                                         idxRegMemResult, TlbState.idxReg1);
                    break;
                case 8:
                    off = iemNativeEmitStoreImm64ByGprEx(pCodeBuf, off, pReNative->Core.aVars[idxVarValue].u.uValue,
                                                         idxRegMemResult, TlbState.idxReg1);
                    break;
                default:
                    AssertFailed();
            }
        }

        iemNativeRegFreeTmp(pReNative, idxRegMemResult);
        TlbState.freeRegsAndReleaseVars(pReNative);

        /*
         * TlbDone:
         *
         * Commit the new RSP value.
         */
        iemNativeLabelDefine(pReNative, idxLabelTlbDone, off);
    }
#endif /* IEMNATIVE_WITH_TLB_LOOKUP */

    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxRegRsp, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.rsp));
    iemNativeRegFreeTmp(pReNative, idxRegRsp);
    if (idxRegEffSp != idxRegRsp)
        iemNativeRegFreeTmp(pReNative, idxRegEffSp);

    /* The value variable is implictly flushed. */
    if (idxRegValue != UINT8_MAX)
        iemNativeVarRegisterRelease(pReNative, idxVarValue);
    iemNativeVarFreeLocal(pReNative, idxVarValue);

    return off;
}



/*                                                     RT_MAKE_U32_FROM_U8(cBitsVar, cBitsFlat, 0, 0) */
#define IEM_MC_POP_GREG_U16(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(16,  0, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFetchU16, pCallEntry->idxInstr)
#define IEM_MC_POP_GREG_U32(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(32,  0, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFetchU32, pCallEntry->idxInstr)
#define IEM_MC_POP_GREG_U64(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(64,  0, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFetchU64, pCallEntry->idxInstr)

#define IEM_MC_FLAT32_POP_GREG_U16(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(16, 32, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFlatFetchU16, pCallEntry->idxInstr)
#define IEM_MC_FLAT32_POP_GREG_U32(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(32, 32, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFlatFetchU32, pCallEntry->idxInstr)

#define IEM_MC_FLAT64_POP_GREG_U16(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(16, 64, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFlatFetchU16, pCallEntry->idxInstr)
#define IEM_MC_FLAT64_POP_GREG_U64(a_iGReg) \
    off = iemNativeEmitStackPopGReg(pReNative, off, a_iGReg, RT_MAKE_U32_FROM_U8(64, 64, 0, 0), \
                                    (uintptr_t)iemNativeHlpStackFlatFetchU64, pCallEntry->idxInstr)


DECL_FORCE_INLINE_THROW(uint32_t)
iemNativeEmitStackPopUse16Sp(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t idxRegRsp, uint8_t idxRegEffSp, uint8_t cbMem,
                             uint8_t idxRegTmp)
{
    /* Use16BitSp: */
#ifdef RT_ARCH_AMD64
    off = iemNativeEmitLoadGprFromGpr16Ex(pCodeBuf, off, idxRegEffSp, idxRegRsp);
    off = iemNativeEmitAddGpr16ImmEx(pCodeBuf, off, idxRegRsp, cbMem); /* ASSUMES this does NOT modify bits [63:16]! */
    RT_NOREF(idxRegTmp);
#else
    /* ubfiz regeff, regrsp, #0, #16 - copies bits 15:0 from RSP to EffSp bits 15:0, zeroing bits 63:16. */
    pCodeBuf[off++] = Armv8A64MkInstrUbfiz(idxRegEffSp, idxRegRsp, 0, 16, false /*f64Bit*/);
    /* add tmp, regrsp, #cbMem */
    pCodeBuf[off++] = Armv8A64MkInstrAddUImm12(idxRegTmp, idxRegRsp, cbMem, false /*f64Bit*/);
    /* and tmp, tmp, #0xffff */
    Assert(Armv8A64ConvertImmRImmS2Mask32(15, 0) == 0xffff);
    pCodeBuf[off++] = Armv8A64MkInstrAndImm(idxRegTmp, idxRegTmp, 15, 0,  false /*f64Bit*/);
    /* bfi regrsp, regeff, #0, #16 - moves bits 15:0 from tmp to RSP bits 15:0, keeping the other RSP bits as is. */
    pCodeBuf[off++] = Armv8A64MkInstrBfi(idxRegRsp, idxRegTmp, 0, 16, false /*f64Bit*/);
#endif
    return off;
}


DECL_FORCE_INLINE(uint32_t)
iemNativeEmitStackPopUse32Sp(PIEMNATIVEINSTR pCodeBuf, uint32_t off, uint8_t idxRegRsp, uint8_t idxRegEffSp, uint8_t cbMem)
{
    /* Use32BitSp: */
    off = iemNativeEmitLoadGprFromGpr32Ex(pCodeBuf, off, idxRegEffSp, idxRegRsp);
    off = iemNativeEmitAddGpr32ImmEx(pCodeBuf, off, idxRegRsp, cbMem);
    return off;
}


/** IEM_MC[|_FLAT32|_FLAT64]_POP_GREG_U16/32/64 */
DECL_INLINE_THROW(uint32_t)
iemNativeEmitStackPopGReg(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxGReg,
                          uint32_t cBitsVarAndFlat, uintptr_t pfnFunction, uint8_t idxInstr)
{
    /*
     * Assert sanity.
     */
    Assert(idxGReg < 16);
#ifdef VBOX_STRICT
    if (RT_BYTE2(cBitsVarAndFlat) != 0)
    {
        Assert(   (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_64BIT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_PROT_FLAT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_FLAT);
        Assert(   pfnFunction
               == (  cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(16, 32, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatFetchU16
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(32, 32, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatFetchU32
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(16, 64, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatFetchU16
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(64, 64, 0, 0) ? (uintptr_t)iemNativeHlpStackFlatFetchU64
                   : UINT64_C(0xc000b000a0009000) ));
    }
    else
        Assert(   pfnFunction
               == (  cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(16, 0, 0, 0) ? (uintptr_t)iemNativeHlpStackFetchU16
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(32, 0, 0, 0) ? (uintptr_t)iemNativeHlpStackFetchU32
                   : cBitsVarAndFlat == RT_MAKE_U32_FROM_U8(64, 0, 0, 0) ? (uintptr_t)iemNativeHlpStackFetchU64
                   : UINT64_C(0xc000b000a0009000) ));
#endif

#ifdef VBOX_STRICT
    /*
     * Check that the fExec flags we've got make sense.
     */
    off = iemNativeEmitExecFlagsCheck(pReNative, off, pReNative->fExec);
#endif

    /*
     * To keep things simple we have to commit any pending writes first as we
     * may end up making calls.
     */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /*
     * Determine the effective stack pointer, for non-FLAT modes we also update RSP.
     * For FLAT modes we'll do this in TlbDone as we'll be using the incoming RSP
     * directly as the effective stack pointer.
     * (Code structure is very similar to that of PUSH)
     */
    uint8_t const cbMem           = RT_BYTE1(cBitsVarAndFlat) / 8;
    uint8_t const cBitsFlat       = RT_BYTE2(cBitsVarAndFlat);      RT_NOREF(cBitsFlat);
    uint8_t const idxRegRsp       = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(X86_GREG_xSP),
                                                                    kIemNativeGstRegUse_ForUpdate, true /*fNoVolatileRegs*/);
    uint8_t const idxRegEffSp     = cBitsFlat != 0 ? idxRegRsp : iemNativeRegAllocTmp(pReNative, &off);
    /** @todo can do a better job picking the register here. For cbMem >= 4 this
     *        will be the resulting register value. */
    uint8_t const idxRegMemResult = iemNativeRegAllocTmp(pReNative, &off); /* pointer then value; arm64 SP += 2/4 helper too.  */

    uint32_t      offFixupJumpToUseOtherBitSp = UINT32_MAX;
    if (cBitsFlat != 0)
    {
        Assert(idxRegEffSp == idxRegRsp);
        Assert(cBitsFlat == 32 || cBitsFlat == 64);
        Assert(IEM_F_MODE_X86_IS_FLAT(pReNative->fExec));
    }
    else /** @todo We can skip the test if we're targeting pre-386 CPUs. */
    {
        Assert(idxRegEffSp != idxRegRsp);
        uint8_t const idxRegSsAttr = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_SEG_ATTRIB(X86_SREG_SS),
                                                                     kIemNativeGstRegUse_ReadOnly);
#ifdef RT_ARCH_AMD64
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
#else
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
#endif
        off = iemNativeEmitTestAnyBitsInGpr32Ex(pCodeBuf, off, idxRegSsAttr, X86DESCATTR_D);
        iemNativeRegFreeTmp(pReNative, idxRegSsAttr);
        offFixupJumpToUseOtherBitSp = off;
        if ((pReNative->fExec & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_32BIT)
        {
/** @todo can skip idxRegRsp updating when popping ESP.   */
            off = iemNativeEmitJccToFixedEx(pCodeBuf, off, off /*8-bit suffices*/, kIemNativeInstrCond_e); /* jump if zero */
            off = iemNativeEmitStackPopUse32Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem);
        }
        else
        {
            off = iemNativeEmitJccToFixedEx(pCodeBuf, off, off /*8-bit suffices*/, kIemNativeInstrCond_ne); /* jump if not zero */
            off = iemNativeEmitStackPopUse16Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem, idxRegMemResult);
        }
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
    /* SpUpdateEnd: */
    uint32_t const offLabelSpUpdateEnd = off;

    /*
     * Okay, now prepare for TLB lookup and jump to code (or the TlbMiss if
     * we're skipping lookup).
     */
    uint8_t const  iSegReg           = cBitsFlat != 0 ? UINT8_MAX : X86_SREG_SS;
    IEMNATIVEEMITTLBSTATE const TlbState(pReNative, idxRegEffSp, &off, iSegReg, cbMem);
    uint16_t const uTlbSeqNo         = pReNative->uTlbSeqNo++;
    uint32_t const idxLabelTlbMiss   = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbMiss, UINT32_MAX, uTlbSeqNo);
    uint32_t const idxLabelTlbLookup = !TlbState.fSkip
                                     ? iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbLookup, UINT32_MAX, uTlbSeqNo)
                                     : UINT32_MAX;

    if (!TlbState.fSkip)
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbLookup); /** @todo short jump */
    else
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbMiss); /** @todo short jump */

    /*
     * Use16BitSp:
     */
    if (cBitsFlat == 0)
    {
#ifdef RT_ARCH_AMD64
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
#else
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 10);
#endif
        iemNativeFixupFixedJump(pReNative, offFixupJumpToUseOtherBitSp, off);
        if ((pReNative->fExec & IEM_F_MODE_CPUMODE_MASK) == IEMMODE_32BIT)
            off = iemNativeEmitStackPopUse16Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem, idxRegMemResult);
        else
            off = iemNativeEmitStackPopUse32Sp(pCodeBuf, off, idxRegRsp, idxRegEffSp, cbMem);
        off = iemNativeEmitJmpToFixedEx(pCodeBuf, off, offLabelSpUpdateEnd);
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }

    /*
     * TlbMiss:
     *
     * Call helper to do the pushing.
     */
    iemNativeLabelDefine(pReNative, idxLabelTlbMiss, off);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

    uint32_t const fHstRegsNotToSave = TlbState.getRegsNotToSave()
                                     | (idxRegMemResult < RT_ELEMENTS(pReNative->Core.aHstRegs) ? RT_BIT_32(idxRegMemResult) : 0)
                                     | (idxRegEffSp != idxRegRsp ? RT_BIT_32(idxRegEffSp) : 0);
    off = iemNativeVarSaveVolatileRegsPreHlpCall(pReNative, off, fHstRegsNotToSave);


    /* IEMNATIVE_CALL_ARG1_GREG = EffSp/RSP */
    if (idxRegEffSp != IEMNATIVE_CALL_ARG1_GREG)
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxRegEffSp);

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, pfnFunction);

    /* Move the return register content to idxRegMemResult. */
    if (idxRegMemResult != IEMNATIVE_CALL_RET_GREG)
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegMemResult, IEMNATIVE_CALL_RET_GREG);

    /* Restore variables and guest shadow registers to volatile registers. */
    off = iemNativeVarRestoreVolatileRegsPostHlpCall(pReNative, off, fHstRegsNotToSave);
    off = iemNativeRegRestoreGuestShadowsInVolatileRegs(pReNative, off, TlbState.getActiveRegsWithShadows());

#ifdef IEMNATIVE_WITH_TLB_LOOKUP
    if (!TlbState.fSkip)
    {
        /* end of TlbMiss - Jump to the done label. */
        uint32_t const idxLabelTlbDone = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbDone, UINT32_MAX, uTlbSeqNo);
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbDone);

        /*
         * TlbLookup:
         */
        off = iemNativeEmitTlbLookup<true>(pReNative, off, &TlbState, iSegReg, cbMem, cbMem - 1, IEM_ACCESS_TYPE_READ,
                                           idxLabelTlbLookup, idxLabelTlbMiss, idxRegMemResult);

        /*
         * Emit code to load the value (from idxRegMemResult into idxRegMemResult).
         */
        PIEMNATIVEINSTR const pCodeBuf = iemNativeInstrBufEnsure(pReNative, off, 32);
# ifdef VBOX_WITH_STATISTICS
        off = iemNativeEmitIncStamCounterInVCpuEx(pCodeBuf, off, TlbState.idxReg1, TlbState.idxReg2,
                                                  RT_UOFFSETOF(VMCPUCC, iem.s.StatNativeTlbHitsForStack));
# endif
        switch (cbMem)
        {
            case 2:
                off = iemNativeEmitLoadGprByGprU16Ex(pCodeBuf, off, idxRegMemResult, idxRegMemResult);
                break;
            case 4:
                off = iemNativeEmitLoadGprByGprU32Ex(pCodeBuf, off, idxRegMemResult, idxRegMemResult);
                break;
            case 8:
                off = iemNativeEmitLoadGprByGprU64Ex(pCodeBuf, off, idxRegMemResult, idxRegMemResult);
                break;
            default:
                AssertFailed();
        }

        TlbState.freeRegsAndReleaseVars(pReNative);

        /*
         * TlbDone:
         *
         * Set the new RSP value (FLAT accesses needs to calculate it first) and
         * commit the popped register value.
         */
        iemNativeLabelDefine(pReNative, idxLabelTlbDone, off);
    }
#endif /* IEMNATIVE_WITH_TLB_LOOKUP */

    if (idxGReg != X86_GREG_xSP)
    {
        /* Set the register. */
        if (cbMem >= sizeof(uint32_t))
        {
            iemNativeRegClearAndMarkAsGstRegShadow(pReNative, idxRegMemResult,  IEMNATIVEGSTREG_GPR(idxGReg), off);
            off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxRegMemResult,
                                                 RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[idxGReg]));
        }
        else
        {
            Assert(cbMem == sizeof(uint16_t));
            uint8_t const idxRegDst = iemNativeRegAllocTmpForGuestReg(pReNative, &off, IEMNATIVEGSTREG_GPR(idxGReg),
                                                                      kIemNativeGstRegUse_ForUpdate);
            off = iemNativeEmitGprMergeInGpr16(pReNative, off, idxRegDst, idxRegMemResult);
            off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxRegDst, RT_UOFFSETOF_DYN(VMCPU, cpum.GstCtx.aGRegs[idxGReg]));
            iemNativeRegFreeTmp(pReNative, idxRegDst);
        }

        /* Complete RSP calculation for FLAT mode. */
        if (idxRegEffSp == idxRegRsp)
        {
            if (cBitsFlat == 64)
                off = iemNativeEmitAddGprImm8(pReNative, off, idxRegRsp, sizeof(uint64_t));
            else
                off = iemNativeEmitAddGpr32Imm8(pReNative, off, idxRegRsp, sizeof(uint32_t));
        }
    }
    else
    {
        /* We're popping RSP, ESP or SP. Only the is a bit extra work, of course. */
        if (cbMem == sizeof(uint64_t))
            off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegRsp, idxRegMemResult);
        else if (cbMem == sizeof(uint32_t))
            off = iemNativeEmitLoadGprFromGpr32(pReNative, off, idxRegRsp, idxRegMemResult);
        else
        {
            if (idxRegEffSp == idxRegRsp)
            {
                if (cBitsFlat == 64)
                    off = iemNativeEmitAddGprImm8(pReNative, off, idxRegRsp, sizeof(uint64_t));
                else
                    off = iemNativeEmitAddGpr32Imm8(pReNative, off, idxRegRsp, sizeof(uint32_t));
            }
            off = iemNativeEmitGprMergeInGpr16(pReNative, off, idxRegRsp, idxRegMemResult);
        }
    }
    off = iemNativeEmitStoreGprToVCpuU64(pReNative, off, idxRegRsp, RT_UOFFSETOF(VMCPU, cpum.GstCtx.rsp));

    iemNativeRegFreeTmp(pReNative, idxRegRsp);
    if (idxRegEffSp != idxRegRsp)
        iemNativeRegFreeTmp(pReNative, idxRegEffSp);
    iemNativeRegFreeTmp(pReNative, idxRegMemResult);

    return off;
}



/*********************************************************************************************************************************
*   Memory mapping (IEM_MEM_MAP_XXX, IEM_MEM_FLAT_MAP_XXX).                                                                      *
*********************************************************************************************************************************/

#define IEM_MC_MEM_MAP_U8_ATOMIC(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_ATOMIC,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU8Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U8_RW(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_RW,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU8Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U8_WO(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_W,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU8Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_MAP_U8_RO(a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_R,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU8Ro, pCallEntry->idxInstr)


#define IEM_MC_MEM_MAP_U16_ATOMIC(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_ATOMIC,  sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU16Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U16_RW(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_RW,  sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU16Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U16_WO(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU16Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_MAP_U16_RO(a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_R,  sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU16Ro, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_I16_WO(a_pi16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pi16Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(int16_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU16Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_MAP_U32_ATOMIC(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_ATOMIC, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU32Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U32_RW(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_RW, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU32Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U32_WO(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU32Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_MAP_U32_RO(a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_R,  sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU32Ro, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_I32_WO(a_pi32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pi32Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(int32_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU32Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_MAP_U64_ATOMIC(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_ATOMIC, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU64Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U64_RW(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_RW, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU64Rw, pCallEntry->idxInstr)
#define IEM_MC_MEM_MAP_U64_WO(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU64Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_MAP_U64_RO(a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_R,  sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU64Ro, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_I64_WO(a_pi64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pi64Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(int64_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU64Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_MAP_R80_WO(a_pr80Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pr80Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(RTFLOAT80U), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataR80Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_MAP_D80_WO(a_pd80Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pd80Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(RTFLOAT80U), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, /** @todo check BCD align */ \
                                    (uintptr_t)iemNativeHlpMemMapDataD80Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_MAP_U128_ATOMIC(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_ATOMIC, sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU128Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U128_RW(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_RW, sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU128Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_MAP_U128_WO(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_W, sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU128Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_MAP_U128_RO(a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, a_iSeg, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_R,  sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemMapDataU128Ro, pCallEntry->idxInstr)



#define IEM_MC_MEM_FLAT_MAP_U8_ATOMIC(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_ATOMIC,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU8Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U8_RW(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_RW,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU8Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U8_WO(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_W,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU8Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_FLAT_MAP_U8_RO(a_pu8Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu8Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint8_t), \
                                    IEM_ACCESS_DATA_R,  0 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU8Ro, pCallEntry->idxInstr)


#define IEM_MC_MEM_FLAT_MAP_U16_ATOMIC(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_ATOMIC,  sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU16Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U16_RW(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_RW,  sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU16Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U16_WO(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU16Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_FLAT_MAP_U16_RO(a_pu16Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu16Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint16_t), \
                                    IEM_ACCESS_DATA_R,  sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU16Ro, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_I16_WO(a_pi16Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pi16Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(int16_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint16_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU16Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_FLAT_MAP_U32_ATOMIC(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_ATOMIC, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU32Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U32_RW(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_RW, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU32Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U32_WO(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU32Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_FLAT_MAP_U32_RO(a_pu32Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu32Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint32_t), \
                                    IEM_ACCESS_DATA_R,  sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU32Ro, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_I32_WO(a_pi32Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pi32Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(int32_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint32_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU32Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_FLAT_MAP_U64_ATOMIC(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_ATOMIC, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU64Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U64_RW(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_RW, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU64Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U64_WO(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU64Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_FLAT_MAP_U64_RO(a_pu64Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu64Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(uint64_t), \
                                    IEM_ACCESS_DATA_R,  sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU64Ro, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_I64_WO(a_pi64Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pi64Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(int64_t), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU64Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_FLAT_MAP_R80_WO(a_pr80Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pr80Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(RTFLOAT80U), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataR80Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_FLAT_MAP_D80_WO(a_pd80Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pd80Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(RTFLOAT80U), \
                                    IEM_ACCESS_DATA_W, sizeof(uint64_t) - 1 /*fAlignMask*/, /** @todo check BCD align */ \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataD80Wo, pCallEntry->idxInstr) \


#define IEM_MC_MEM_FLAT_MAP_U128_ATOMIC(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_ATOMIC, sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU128Atomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U128_RW(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_RW, sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU128Rw, pCallEntry->idxInstr)

#define IEM_MC_MEM_FLAT_MAP_U128_WO(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_W, sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU128Wo, pCallEntry->idxInstr) \

#define IEM_MC_MEM_FLAT_MAP_U128_RO(a_pu128Mem, a_bUnmapInfo, a_GCPtrMem) \
    off = iemNativeEmitMemMapCommon(pReNative, off, a_pu128Mem, a_bUnmapInfo, UINT8_MAX, a_GCPtrMem, sizeof(RTUINT128U), \
                                    IEM_ACCESS_DATA_R,  sizeof(RTUINT128U) - 1 /*fAlignMask*/, \
                                    (uintptr_t)iemNativeHlpMemFlatMapDataU128Ro, pCallEntry->idxInstr)


DECL_INLINE_THROW(uint32_t)
iemNativeEmitMemMapCommon(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarMem, uint8_t idxVarUnmapInfo,
                          uint8_t iSegReg, uint8_t idxVarGCPtrMem, uint8_t cbMem, uint32_t fAccess, uint8_t fAlignMask,
                          uintptr_t pfnFunction, uint8_t idxInstr)
{
    /*
     * Assert sanity.
     */
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarMem);
    AssertStmt(   pReNative->Core.aVars[idxVarMem].enmKind == kIemNativeVarKind_Invalid
               && pReNative->Core.aVars[idxVarMem].cbVar   == sizeof(void *),
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarUnmapInfo);
    AssertStmt(   pReNative->Core.aVars[idxVarUnmapInfo].enmKind == kIemNativeVarKind_Invalid
               && pReNative->Core.aVars[idxVarUnmapInfo].cbVar   == sizeof(uint8_t),
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarGCPtrMem);
    AssertStmt(   pReNative->Core.aVars[idxVarGCPtrMem].enmKind == kIemNativeVarKind_Immediate
               || pReNative->Core.aVars[idxVarGCPtrMem].enmKind == kIemNativeVarKind_Stack,
               IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_VAR_UNEXPECTED_KIND));

    Assert(iSegReg < 6 || iSegReg == UINT8_MAX);

    AssertCompile(IEMNATIVE_CALL_ARG_GREG_COUNT >= 4);

#ifdef VBOX_STRICT
# define IEM_MAP_HLP_FN_NO_AT(a_fAccess, a_fnBase) \
        (  ((a_fAccess) & (IEM_ACCESS_TYPE_MASK | IEM_ACCESS_ATOMIC)) == (IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_TYPE_READ) \
         ? (uintptr_t)RT_CONCAT(a_fnBase,Rw) \
         : ((a_fAccess) & (IEM_ACCESS_TYPE_MASK | IEM_ACCESS_ATOMIC)) == IEM_ACCESS_TYPE_READ \
         ? (uintptr_t)RT_CONCAT(a_fnBase,Ro) : (uintptr_t)RT_CONCAT(a_fnBase,Wo) )
# define IEM_MAP_HLP_FN(a_fAccess, a_fnBase) \
        (  ((a_fAccess) & (IEM_ACCESS_TYPE_MASK | IEM_ACCESS_ATOMIC)) == (IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_TYPE_READ | IEM_ACCESS_ATOMIC) \
         ? (uintptr_t)RT_CONCAT(a_fnBase,Atomic) \
         : IEM_MAP_HLP_FN_NO_AT(a_fAccess, a_fnBase) )

    if (iSegReg == UINT8_MAX)
    {
        Assert(   (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_64BIT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_PROT_FLAT
               || (pReNative->fExec & IEM_F_MODE_MASK) == IEM_F_MODE_X86_32BIT_FLAT);
        switch (cbMem)
        {
            case 1:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemFlatMapDataU8)); break;
            case 2:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemFlatMapDataU16)); break;
            case 4:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemFlatMapDataU32)); break;
            case 8:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemFlatMapDataU64)); break;
            case 10:
                Assert(   pfnFunction == (uintptr_t)iemNativeHlpMemFlatMapDataR80Wo
                       || pfnFunction == (uintptr_t)iemNativeHlpMemFlatMapDataD80Wo);
                Assert((fAccess & IEM_ACCESS_TYPE_MASK) == IEM_ACCESS_TYPE_WRITE);
                break;
            case 16: Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemFlatMapDataU128)); break;
# if 0
            case 32: Assert(pfnFunction == IEM_MAP_HLP_FN_NO_AT(fAccess, iemNativeHlpMemFlatMapDataU256)); break;
            case 64: Assert(pfnFunction == IEM_MAP_HLP_FN_NO_AT(fAccess, iemNativeHlpMemFlatMapDataU512)); break;
# endif
            default: AssertFailed(); break;
        }
    }
    else
    {
        Assert(iSegReg < 6);
        switch (cbMem)
        {
            case 1:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemMapDataU8)); break;
            case 2:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemMapDataU16)); break;
            case 4:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemMapDataU32)); break;
            case 8:  Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemMapDataU64)); break;
            case 10:
                Assert(   pfnFunction == (uintptr_t)iemNativeHlpMemMapDataR80Wo
                       || pfnFunction == (uintptr_t)iemNativeHlpMemMapDataD80Wo);
                Assert((fAccess & IEM_ACCESS_TYPE_MASK) == IEM_ACCESS_TYPE_WRITE);
                break;
            case 16: Assert(pfnFunction == IEM_MAP_HLP_FN(fAccess, iemNativeHlpMemMapDataU128)); break;
# if 0
            case 32: Assert(pfnFunction == IEM_MAP_HLP_FN_NO_AT(fAccess, iemNativeHlpMemMapDataU256)); break;
            case 64: Assert(pfnFunction == IEM_MAP_HLP_FN_NO_AT(fAccess, iemNativeHlpMemMapDataU512)); break;
# endif
            default: AssertFailed(); break;
        }
    }
# undef IEM_MAP_HLP_FN
# undef IEM_MAP_HLP_FN_NO_AT
#endif

#ifdef VBOX_STRICT
    /*
     * Check that the fExec flags we've got make sense.
     */
    off = iemNativeEmitExecFlagsCheck(pReNative, off, pReNative->fExec);
#endif

    /*
     * To keep things simple we have to commit any pending writes first as we
     * may end up making calls.
     */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

#ifdef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
    /*
     * Move/spill/flush stuff out of call-volatile registers.
     * This is the easy way out. We could contain this to the tlb-miss branch
     * by saving and restoring active stuff here.
     */
    /** @todo save+restore active registers and maybe guest shadows in tlb-miss.  */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 0 /* vacate all non-volatile regs */);
#endif

    /* The bUnmapInfo variable will get a register in the tlb-hit code path,
       while the tlb-miss codepath will temporarily put it on the stack.
       Set the the type to stack here so we don't need to do it twice below. */
    iemNativeVarSetKindToStack(pReNative, idxVarUnmapInfo);
    uint8_t const idxRegUnmapInfo   = iemNativeVarRegisterAcquire(pReNative, idxVarUnmapInfo, &off);
    /** @todo use a tmp register from TlbState, since they'll be free after tlb
     *        lookup is done. */

    /*
     * Define labels and allocate the result register (trying for the return
     * register if we can).
     */
    uint16_t const uTlbSeqNo         = pReNative->uTlbSeqNo++;
    uint8_t  const idxRegMemResult   = !(pReNative->Core.bmHstRegs & RT_BIT_32(IEMNATIVE_CALL_RET_GREG))
                                     ? iemNativeVarRegisterSetAndAcquire(pReNative, idxVarMem, IEMNATIVE_CALL_RET_GREG, &off)
                                     : iemNativeVarRegisterAcquire(pReNative, idxVarMem, &off);
    IEMNATIVEEMITTLBSTATE const TlbState(pReNative, &off, idxVarGCPtrMem, iSegReg, cbMem);
    uint32_t const idxLabelTlbLookup = !TlbState.fSkip
                                     ? iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbLookup, UINT32_MAX, uTlbSeqNo)
                                     : UINT32_MAX;
//off=iemNativeEmitBrk(pReNative, off, 0);
    /*
     * Jump to the TLB lookup code.
     */
    if (!TlbState.fSkip)
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbLookup); /** @todo short jump */

    /*
     * TlbMiss:
     *
     * Call helper to do the fetching.
     * We flush all guest register shadow copies here.
     */
    uint32_t const idxLabelTlbMiss = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbMiss, off, uTlbSeqNo);

#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

#ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
    /* Save variables in volatile registers. */
    uint32_t const fHstRegsNotToSave = TlbState.getRegsNotToSave() | RT_BIT_32(idxRegMemResult) | RT_BIT_32(idxRegUnmapInfo);
    off = iemNativeVarSaveVolatileRegsPreHlpCall(pReNative, off, fHstRegsNotToSave);
#endif

    /* IEMNATIVE_CALL_ARG2_GREG = GCPtrMem - load first as it is from a variable. */
    off = iemNativeEmitLoadArgGregFromImmOrStackVar(pReNative, off, IEMNATIVE_CALL_ARG2_GREG, idxVarGCPtrMem, 0 /*cbAppend*/,
#ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
                                                    IEMNATIVE_CALL_VOLATILE_GREG_MASK, true /*fSpilledVarsInvolatileRegs*/);
#else
                                                    IEMNATIVE_CALL_VOLATILE_GREG_MASK);
#endif

    /* IEMNATIVE_CALL_ARG3_GREG = iSegReg */
    if (iSegReg != UINT8_MAX)
    {
        AssertStmt(iSegReg < 6, IEMNATIVE_DO_LONGJMP(pReNative, VERR_IEM_EMIT_BAD_SEG_REG_NO));
        off = iemNativeEmitLoadGpr8Imm(pReNative, off, IEMNATIVE_CALL_ARG3_GREG, iSegReg);
    }

    /* IEMNATIVE_CALL_ARG1_GREG = &idxVarUnmapInfo; stackslot address, load any register with result after the call. */
    int32_t const offBpDispVarUnmapInfo = iemNativeStackCalcBpDisp(iemNativeVarGetStackSlot(pReNative, idxVarUnmapInfo));
    off = iemNativeEmitLeaGprByBp(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, offBpDispVarUnmapInfo);

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, pfnFunction);

    /*
     * Put the output in the right registers.
     */
    Assert(idxRegMemResult == pReNative->Core.aVars[idxVarMem].idxReg);
    if (idxRegMemResult != IEMNATIVE_CALL_RET_GREG)
        off = iemNativeEmitLoadGprFromGpr(pReNative, off, idxRegMemResult, IEMNATIVE_CALL_RET_GREG);

#ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
    /* Restore variables and guest shadow registers to volatile registers. */
    off = iemNativeVarRestoreVolatileRegsPostHlpCall(pReNative, off, fHstRegsNotToSave);
    off = iemNativeRegRestoreGuestShadowsInVolatileRegs(pReNative, off, TlbState.getActiveRegsWithShadows());
#endif

    Assert(pReNative->Core.aVars[idxVarUnmapInfo].idxReg == idxRegUnmapInfo);
    off = iemNativeEmitLoadGprByBpU8(pReNative, off, idxRegUnmapInfo, offBpDispVarUnmapInfo);

#ifdef IEMNATIVE_WITH_TLB_LOOKUP
    if (!TlbState.fSkip)
    {
        /* end of tlbsmiss - Jump to the done label. */
        uint32_t const idxLabelTlbDone = iemNativeLabelCreate(pReNative, kIemNativeLabelType_TlbDone, UINT32_MAX, uTlbSeqNo);
        off = iemNativeEmitJmpToLabel(pReNative, off, idxLabelTlbDone);

        /*
         * TlbLookup:
         */
        off = iemNativeEmitTlbLookup<true>(pReNative, off, &TlbState, iSegReg, cbMem, fAlignMask, fAccess,
                                           idxLabelTlbLookup, idxLabelTlbMiss, idxRegMemResult);
# ifdef VBOX_WITH_STATISTICS
        off = iemNativeEmitIncStamCounterInVCpu(pReNative, off, TlbState.idxReg1, TlbState.idxReg2,
                                                RT_UOFFSETOF(VMCPUCC,  iem.s.StatNativeTlbHitsForMapped));
# endif

        /* [idxVarUnmapInfo] = 0; */
        off = iemNativeEmitLoadGprImm32(pReNative, off, idxRegUnmapInfo, 0);

        /*
         * TlbDone:
         */
        iemNativeLabelDefine(pReNative, idxLabelTlbDone, off);

        TlbState.freeRegsAndReleaseVars(pReNative, idxVarGCPtrMem);

# ifndef IEMNATIVE_WITH_FREE_AND_FLUSH_VOLATILE_REGS_AT_TLB_LOOKUP
        /* Temp Hack: Flush all guest shadows in volatile registers in case of TLB miss. */
        iemNativeRegFlushGuestShadowsByHostMask(pReNative, IEMNATIVE_CALL_VOLATILE_GREG_MASK);
# endif
    }
#else
    RT_NOREF(fAccess, fAlignMask, idxLabelTlbMiss);
#endif

    iemNativeVarRegisterRelease(pReNative, idxVarUnmapInfo);
    iemNativeVarRegisterRelease(pReNative, idxVarMem);

    return off;
}


#define IEM_MC_MEM_COMMIT_AND_UNMAP_ATOMIC(a_bMapInfo) \
    off = iemNativeEmitMemCommitAndUnmap(pReNative, off, (a_bMapInfo), IEM_ACCESS_DATA_ATOMIC, \
                                         (uintptr_t)iemNativeHlpMemCommitAndUnmapAtomic, pCallEntry->idxInstr)

#define IEM_MC_MEM_COMMIT_AND_UNMAP_RW(a_bMapInfo) \
    off = iemNativeEmitMemCommitAndUnmap(pReNative, off, (a_bMapInfo), IEM_ACCESS_DATA_RW, \
                                         (uintptr_t)iemNativeHlpMemCommitAndUnmapRw, pCallEntry->idxInstr)

#define IEM_MC_MEM_COMMIT_AND_UNMAP_WO(a_bMapInfo) \
    off = iemNativeEmitMemCommitAndUnmap(pReNative, off, (a_bMapInfo), IEM_ACCESS_DATA_W, \
                                         (uintptr_t)iemNativeHlpMemCommitAndUnmapWo, pCallEntry->idxInstr)

#define IEM_MC_MEM_COMMIT_AND_UNMAP_RO(a_bMapInfo) \
    off = iemNativeEmitMemCommitAndUnmap(pReNative, off, (a_bMapInfo), IEM_ACCESS_DATA_R, \
                                         (uintptr_t)iemNativeHlpMemCommitAndUnmapRo, pCallEntry->idxInstr)

DECL_INLINE_THROW(uint32_t)
iemNativeEmitMemCommitAndUnmap(PIEMRECOMPILERSTATE pReNative, uint32_t off, uint8_t idxVarUnmapInfo,
                               uint32_t fAccess, uintptr_t pfnFunction, uint8_t idxInstr)
{
    /*
     * Assert sanity.
     */
    IEMNATIVE_ASSERT_VAR_IDX(pReNative, idxVarUnmapInfo);
    Assert(pReNative->Core.aVars[idxVarUnmapInfo].enmKind == kIemNativeVarKind_Stack);
    Assert(   pReNative->Core.aVars[idxVarUnmapInfo].idxReg       < RT_ELEMENTS(pReNative->Core.aHstRegs)
           || pReNative->Core.aVars[idxVarUnmapInfo].idxStackSlot < IEMNATIVE_FRAME_VAR_SLOTS); /* must be initialized */
#ifdef VBOX_STRICT
    switch (fAccess & (IEM_ACCESS_TYPE_MASK | IEM_ACCESS_ATOMIC))
    {
        case IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_WRITE | IEM_ACCESS_ATOMIC:
            Assert(pfnFunction == (uintptr_t)iemNativeHlpMemCommitAndUnmapAtomic); break;
        case IEM_ACCESS_TYPE_READ | IEM_ACCESS_TYPE_WRITE:
            Assert(pfnFunction == (uintptr_t)iemNativeHlpMemCommitAndUnmapRw); break;
        case IEM_ACCESS_TYPE_WRITE:
            Assert(pfnFunction == (uintptr_t)iemNativeHlpMemCommitAndUnmapWo); break;
        case IEM_ACCESS_TYPE_READ:
            Assert(pfnFunction == (uintptr_t)iemNativeHlpMemCommitAndUnmapRo); break;
        default: AssertFailed();
    }
#else
    RT_NOREF(fAccess);
#endif

    /*
     * To keep things simple we have to commit any pending writes first as we
     * may end up making calls (there shouldn't be any at this point, so this
     * is just for consistency).
     */
    /** @todo we could postpone this till we make the call and reload the
     * registers after returning from the call. Not sure if that's sensible or
     * not, though. */
    off = iemNativeRegFlushPendingWrites(pReNative, off);

    /*
     * Move/spill/flush stuff out of call-volatile registers.
     *
     * We exclude any register holding the bUnmapInfo variable, as we'll be
     * checking it after returning from the call and will free it afterwards.
     */
    /** @todo save+restore active registers and maybe guest shadows in miss
     *        scenario. */
    off = iemNativeRegMoveAndFreeAndFlushAtCall(pReNative, off, 0 /* vacate all non-volatile regs */, RT_BIT_32(idxVarUnmapInfo));

    /*
     * If idxVarUnmapInfo is zero, we can skip all this. Otherwise we'll have
     * to call the unmap helper function.
     *
     * The likelyhood of it being zero is higher than for the TLB hit when doing
     * the mapping, as a TLB miss for an well aligned and unproblematic memory
     * access should also end up with a mapping that won't need special unmapping.
     */
    /** @todo Go over iemMemMapJmp and implement the no-unmap-needed case!  That
     *        should speed up things for the pure interpreter as well when TLBs
     *        are enabled. */
#ifdef RT_ARCH_AMD64
    if (pReNative->Core.aVars[idxVarUnmapInfo].idxReg == UINT8_MAX)
    {
        /* test byte [rbp - xxx], 0ffh  */
        uint8_t * const pbCodeBuf    = iemNativeInstrBufEnsure(pReNative, off, 7);
        pbCodeBuf[off++] = 0xf6;
        uint8_t const   idxStackSlot = pReNative->Core.aVars[idxVarUnmapInfo].idxStackSlot;
        off = iemNativeEmitGprByBpDisp(pbCodeBuf, off, 0, iemNativeStackCalcBpDisp(idxStackSlot), pReNative);
        pbCodeBuf[off++] = 0xff;
        IEMNATIVE_ASSERT_INSTR_BUF_ENSURE(pReNative, off);
    }
    else
#endif
    {
        uint8_t const idxVarReg = iemNativeVarRegisterAcquire(pReNative, idxVarUnmapInfo, &off,
                                                              true /*fInitialized*/, IEMNATIVE_CALL_ARG1_GREG /*idxRegPref*/);
        off = iemNativeEmitTestAnyBitsInGpr8(pReNative, off, idxVarReg, 0xff);
        iemNativeVarRegisterRelease(pReNative, idxVarUnmapInfo);
    }
    uint32_t const offJmpFixup = off;
    off = iemNativeEmitJzToFixed(pReNative, off, off /* ASSUME jz rel8 suffices*/);

    /*
     * Call the unmap helper function.
     */
#ifdef IEMNATIVE_WITH_INSTRUCTION_COUNTING /** @todo This should be unnecessary, the mapping call will already have set it! */
    off = iemNativeEmitStoreImmToVCpuU8(pReNative, off, idxInstr, RT_UOFFSETOF(VMCPUCC, iem.s.idxTbCurInstr));
#else
    RT_NOREF(idxInstr);
#endif

    /* IEMNATIVE_CALL_ARG1_GREG = idxVarUnmapInfo (first!) */
    off = iemNativeEmitLoadArgGregFromStackVar(pReNative, off, IEMNATIVE_CALL_ARG1_GREG, idxVarUnmapInfo,
                                               0 /*offAddend*/, IEMNATIVE_CALL_VOLATILE_GREG_MASK);

    /* IEMNATIVE_CALL_ARG0_GREG = pVCpu */
    off = iemNativeEmitLoadGprFromGpr(pReNative, off, IEMNATIVE_CALL_ARG0_GREG, IEMNATIVE_REG_FIXED_PVMCPU);

    /* Done setting up parameters, make the call. */
    off = iemNativeEmitCallImm(pReNative, off, pfnFunction);

    /* The bUnmapInfo variable is implictly free by these MCs. */
    iemNativeVarFreeLocal(pReNative, idxVarUnmapInfo);

    /*
     * Done, just fixup the jump for the non-call case.
     */
    iemNativeFixupFixedJump(pReNative, offJmpFixup, off);

    return off;
}



/*********************************************************************************************************************************
*   State and Exceptions                                                                                                         *
*********************************************************************************************************************************/

#define IEM_MC_ACTUALIZE_FPU_STATE_FOR_CHANGE()     off = iemNativeEmitPrepareFpuForUse(pReNative, off, true /*fForChange*/)
#define IEM_MC_ACTUALIZE_FPU_STATE_FOR_READ()       off = iemNativeEmitPrepareFpuForUse(pReNative, off, false /*fForChange*/)

#define IEM_MC_PREPARE_SSE_USAGE()                  off = iemNativeEmitPrepareFpuForUse(pReNative, off, true /*fForChange*/)
#define IEM_MC_ACTUALIZE_SSE_STATE_FOR_CHANGE()     off = iemNativeEmitPrepareFpuForUse(pReNative, off, true /*fForChange*/)
#define IEM_MC_ACTUALIZE_SSE_STATE_FOR_READ()       off = iemNativeEmitPrepareFpuForUse(pReNative, off, false /*fForChange*/)

#define IEM_MC_PREPARE_AVX_USAGE()                  off = iemNativeEmitPrepareFpuForUse(pReNative, off, true /*fForChange*/)
#define IEM_MC_ACTUALIZE_AVX_STATE_FOR_CHANGE()     off = iemNativeEmitPrepareFpuForUse(pReNative, off, true /*fForChange*/)
#define IEM_MC_ACTUALIZE_AVX_STATE_FOR_READ()       off = iemNativeEmitPrepareFpuForUse(pReNative, off, false /*fForChange*/)


DECL_INLINE_THROW(uint32_t) iemNativeEmitPrepareFpuForUse(PIEMRECOMPILERSTATE pReNative, uint32_t off, bool fForChange)
{
    /** @todo this needs a lot more work later. */
    RT_NOREF(pReNative, fForChange);
    return off;
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


/** @callback_method_impl{FNDISREADBYTES, Dummy.} */
static DECLCALLBACK(int) iemNativeDisasReadBytesDummy(PDISSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead)
{
    RT_BZERO(&pDis->Instr.ab[offInstr], cbMaxRead);
    pDis->cbCachedInstr += cbMaxRead;
    RT_NOREF(cbMinRead);
    return VERR_NO_DATA;
}


/**
 * Formats TB flags (IEM_F_XXX and IEMTB_F_XXX) to string.
 * @returns pszBuf.
 * @param   fFlags  The flags.
 * @param   pszBuf  The output buffer.
 * @param   cbBuf   The output buffer size.  At least 32 bytes.
 */
DECLHIDDEN(const char *) iemTbFlagsToString(uint32_t fFlags, char *pszBuf, size_t cbBuf) RT_NOEXCEPT
{
    Assert(cbBuf >= 32);
    static RTSTRTUPLE const s_aModes[] =
    {
        /* [00] = */ { RT_STR_TUPLE("16BIT") },
        /* [01] = */ { RT_STR_TUPLE("32BIT") },
        /* [02] = */ { RT_STR_TUPLE("!2!") },
        /* [03] = */ { RT_STR_TUPLE("!3!") },
        /* [04] = */ { RT_STR_TUPLE("16BIT_PRE_386") },
        /* [05] = */ { RT_STR_TUPLE("32BIT_FLAT") },
        /* [06] = */ { RT_STR_TUPLE("!6!") },
        /* [07] = */ { RT_STR_TUPLE("!7!") },
        /* [08] = */ { RT_STR_TUPLE("16BIT_PROT") },
        /* [09] = */ { RT_STR_TUPLE("32BIT_PROT") },
        /* [0a] = */ { RT_STR_TUPLE("64BIT") },
        /* [0b] = */ { RT_STR_TUPLE("!b!") },
        /* [0c] = */ { RT_STR_TUPLE("16BIT_PROT_PRE_386") },
        /* [0d] = */ { RT_STR_TUPLE("32BIT_PROT_FLAT") },
        /* [0e] = */ { RT_STR_TUPLE("!e!") },
        /* [0f] = */ { RT_STR_TUPLE("!f!") },
        /* [10] = */ { RT_STR_TUPLE("!10!") },
        /* [11] = */ { RT_STR_TUPLE("!11!") },
        /* [12] = */ { RT_STR_TUPLE("!12!") },
        /* [13] = */ { RT_STR_TUPLE("!13!") },
        /* [14] = */ { RT_STR_TUPLE("!14!") },
        /* [15] = */ { RT_STR_TUPLE("!15!") },
        /* [16] = */ { RT_STR_TUPLE("!16!") },
        /* [17] = */ { RT_STR_TUPLE("!17!") },
        /* [18] = */ { RT_STR_TUPLE("16BIT_PROT_V86") },
        /* [19] = */ { RT_STR_TUPLE("32BIT_PROT_V86") },
        /* [1a] = */ { RT_STR_TUPLE("!1a!") },
        /* [1b] = */ { RT_STR_TUPLE("!1b!") },
        /* [1c] = */ { RT_STR_TUPLE("!1c!") },
        /* [1d] = */ { RT_STR_TUPLE("!1d!") },
        /* [1e] = */ { RT_STR_TUPLE("!1e!") },
        /* [1f] = */ { RT_STR_TUPLE("!1f!") },
    };
    AssertCompile(RT_ELEMENTS(s_aModes) == IEM_F_MODE_MASK + 1);
    memcpy(pszBuf, s_aModes[fFlags & IEM_F_MODE_MASK].psz, s_aModes[fFlags & IEM_F_MODE_MASK].cch);
    size_t off = s_aModes[fFlags & IEM_F_MODE_MASK].cch;

    pszBuf[off++] = ' ';
    pszBuf[off++] = 'C';
    pszBuf[off++] = 'P';
    pszBuf[off++] = 'L';
    pszBuf[off++] = '0' + ((fFlags >> IEM_F_X86_CPL_SHIFT) & IEM_F_X86_CPL_SMASK);
    Assert(off < 32);

    fFlags &= ~(IEM_F_MODE_MASK | IEM_F_X86_CPL_SMASK);

    static struct { const char *pszName; uint32_t cchName; uint32_t fFlag; } const s_aFlags[] =
    {
        { RT_STR_TUPLE("BYPASS_HANDLERS"),      IEM_F_BYPASS_HANDLERS    },
        { RT_STR_TUPLE("PENDING_BRK_INSTR"),    IEM_F_PENDING_BRK_INSTR  },
        { RT_STR_TUPLE("PENDING_BRK_DATA"),     IEM_F_PENDING_BRK_DATA   },
        { RT_STR_TUPLE("PENDING_BRK_X86_IO"),   IEM_F_PENDING_BRK_X86_IO },
        { RT_STR_TUPLE("X86_DISREGARD_LOCK"),   IEM_F_X86_DISREGARD_LOCK },
        { RT_STR_TUPLE("X86_CTX_VMX"),          IEM_F_X86_CTX_VMX        },
        { RT_STR_TUPLE("X86_CTX_SVM"),          IEM_F_X86_CTX_SVM        },
        { RT_STR_TUPLE("X86_CTX_IN_GUEST"),     IEM_F_X86_CTX_IN_GUEST   },
        { RT_STR_TUPLE("X86_CTX_SMM"),          IEM_F_X86_CTX_SMM        },
        { RT_STR_TUPLE("INHIBIT_SHADOW"),       IEMTB_F_INHIBIT_SHADOW   },
        { RT_STR_TUPLE("INHIBIT_NMI"),          IEMTB_F_INHIBIT_NMI      },
        { RT_STR_TUPLE("CS_LIM_CHECKS"),        IEMTB_F_CS_LIM_CHECKS    },
        { RT_STR_TUPLE("TYPE_THREADED"),        IEMTB_F_TYPE_THREADED    },
        { RT_STR_TUPLE("TYPE_NATIVE"),          IEMTB_F_TYPE_NATIVE      },
    };
    if (fFlags)
        for (unsigned i = 0; i < RT_ELEMENTS(s_aFlags); i++)
            if (s_aFlags[i].fFlag & fFlags)
            {
                AssertReturnStmt(off + 1 + s_aFlags[i].cchName + 1 <= cbBuf, pszBuf[off] = '\0', pszBuf);
                pszBuf[off++] = ' ';
                memcpy(&pszBuf[off], s_aFlags[i].pszName, s_aFlags[i].cchName);
                off += s_aFlags[i].cchName;
                fFlags &= ~s_aFlags[i].fFlag;
                if (!fFlags)
                    break;
            }
    pszBuf[off] = '\0';

    return pszBuf;
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
                                pHlp->pfnPrintf(pHlp, "  Guest register %s == host register %s\n", pszGstReg,
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstReg]);
                            else
                                pHlp->pfnPrintf(pHlp, "  Guest register %s == host register %s (previously in %s)\n", pszGstReg,
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstReg],
                                                g_apszIemNativeHstRegNames[pEntry->GuestRegShadowing.idxHstRegPrev]);
                            continue;
                        }

                        case kIemTbDbgEntryType_Label:
                        {
                            const char *pszName    = "what_the_fudge";
                            const char *pszComment = "";
                            bool        fNumbered  = pDbgInfo->aEntries[iDbgEntry].Label.uData != 0;
                            switch ((IEMNATIVELABELTYPE)pDbgInfo->aEntries[iDbgEntry].Label.enmLabel)
                            {
                                case kIemNativeLabelType_Return:
                                    pszName = "Return";
                                    break;
                                case kIemNativeLabelType_ReturnBreak:
                                    pszName = "ReturnBreak";
                                    break;
                                case kIemNativeLabelType_ReturnWithFlags:
                                    pszName = "ReturnWithFlags";
                                    break;
                                case kIemNativeLabelType_NonZeroRetOrPassUp:
                                    pszName = "NonZeroRetOrPassUp";
                                    break;
                                case kIemNativeLabelType_RaiseGp0:
                                    pszName = "RaiseGp0";
                                    break;
                                case kIemNativeLabelType_ObsoleteTb:
                                    pszName = "ObsoleteTb";
                                    break;
                                case kIemNativeLabelType_NeedCsLimChecking:
                                    pszName = "NeedCsLimChecking";
                                    break;
                                case kIemNativeLabelType_CheckBranchMiss:
                                    pszName = "CheckBranchMiss";
                                    break;
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
        paLivenessEntries[idxCall].s1.bm64 = IEMLIVENESSPART1_ALL_UNUSED;
        paLivenessEntries[idxCall].s2.bm64 = IEMLIVENESSPART2_ALL_UNUSED;

        /* Loop backwards thru the calls and fill in the other entries. */
        PCIEMTHRDEDCALLENTRY pCallEntry = &pTb->Thrd.paCalls[idxCall];
        while (idxCall > 0)
        {
            PFNIEMNATIVELIVENESSFUNC const pfnLiveness = g_apfnIemNativeLivenessFunctions[pCallEntry->enmFunction];
            if (pfnLiveness)
                pfnLiveness(pCallEntry, &paLivenessEntries[idxCall - 1], &paLivenessEntries[idxCall]);
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
             * Debug info and assembly markup.
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
        if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_RaiseGp0))
            off = iemNativeEmitRaiseGp0(pReNative, off, idxReturnLabel);
        if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_ObsoleteTb))
            off = iemNativeEmitObsoleteTb(pReNative, off, idxReturnLabel);
        if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_NeedCsLimChecking))
            off = iemNativeEmitNeedCsLimChecking(pReNative, off, idxReturnLabel);
        if (pReNative->bmLabelTypes & RT_BIT_64(kIemNativeLabelType_CheckBranchMiss))
            off = iemNativeEmitCheckBranchMiss(pReNative, off, idxReturnLabel);
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
# ifdef DEBUG_bird
        RTLogFlush(NULL);
# endif
    }
#endif
    /*iemNativeDisassembleTb(pTb, DBGFR3InfoLogRelHlp());*/

    STAM_REL_PROFILE_STOP(&pVCpu->iem.s.StatNativeRecompilation, a);
    return pTb;
}

