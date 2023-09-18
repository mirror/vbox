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
#ifdef RT_OS_WINDOWS
/** @todo */
#else
# include <iprt/formats/dwarf.h>
extern "C" void __register_frame_info(void *begin, void *ob);
#endif

#include "IEMInline.h"
#include "IEMThreadedFunctions.h"


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

#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
/**
 * Per-chunk unwind info for non-windows hosts.
 */
typedef struct IEMEXECMEMCHUNKEHFRAME
{
    /** struct object storage area. */
    uint8_t                 abObject[1024];
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
    /** The heap handle. */
    RTHEAPSIMPLE            hHeap;
    /** Pointer to the chunk. */
    void                   *pvChunk;
#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
    /** Exception handling frame information for proper unwinding during C++
     *  throws and (possibly) longjmp(). */
    PIEMEXECMEMCHUNKEHFRAME pEhFrame;
#endif
#ifdef IN_RING0
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

#if defined(IN_RING3) && defined(RT_OS_WINDOWS) && (defined(RT_ARCH_AMD64) || defined(RT_ARCH_ARM64))
    PRUNTIME_FUNCTION       paUnwindFunctions;
#endif

    /** The allocation chunks. */
    RT_FLEXIBLE_ARRAY_EXTENSION
    IEMEXECMEMCHUNK         aChunks[RT_FLEXIBLE_ARRAY];
} IEMEXECMEMALLOCATOR;
/** Pointer to an executable memory allocator. */
typedef IEMEXECMEMALLOCATOR *PIEMEXECMEMALLOCATOR;

/** Magic value for IEMEXECMEMALLOCATOR::uMagic (Scott Frederick Turow). */
#define IEMEXECMEMALLOCATOR_MAGIC UINT32_C(0x19490412)


#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)

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


/**
 * Initalizes the unwind info section for non-windows hosts.
 */
static void iemExecMemAllocatorInitEhFrameForChunk(PIEMEXECMEMALLOCATOR pExecMemAllocator,
                                                   PIEMEXECMEMCHUNKEHFRAME pEhFrame, void *pvChunk)
{
    RTPTRUNION Ptr = { pEhFrame };

    /*
     * Generate the CIE first.
     */
    RTPTRUNION const PtrCie = Ptr;
    *Ptr.pu32++ = 123;                                      /* The CIE length will be determined later. */
    *Ptr.pu32++ = 0 /*UINT32_MAX*/;                         /* I'm a CIE in .eh_frame speak. */
    *Ptr.pb++   = 4;                                        /* DwARF v4. */
    *Ptr.pb++   = 0;                                        /* Augmentation. */
    *Ptr.pb++   = sizeof(uintptr_t);                        /* Address size. */
    *Ptr.pb++   = 0;                                        /* Segment selector size. */
    Ptr = iemDwarfPutLeb128(Ptr, 1);                        /* Code alignment factor (LEB128 = 1). */
    Ptr = iemDwarfPutLeb128(Ptr, -8);                       /* Data alignment factor (LEB128 = -8). */
    Ptr = iemDwarfPutUleb128(Ptr, DWREG_AMD64_RA);          /* Return address column (ULEB128) */
    /* Initial instructions: */
    Ptr = iemDwarfPutCfaDefCfa(Ptr, DWREG_AMD64_RBP, 16);   /* CFA     = RBP + 0x10 - first stack parameter */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_RA,  1);    /* Ret RIP = [CFA + 1*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_RBP, 2);    /* RBP     = [CFA + 2*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_RBX, 3);    /* RBX     = [CFA + 3*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R12, 4);    /* R12     = [CFA + 4*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R13, 5);    /* R13     = [CFA + 5*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R14, 6);    /* R14     = [CFA + 6*-8] */
    Ptr = iemDwarfPutCfaOffset(Ptr, DWREG_AMD64_R15, 7);    /* R15     = [CFA + 7*-8] */
    while ((Ptr.u - PtrCie.u) & 3)
        *Ptr.pb++ = DW_CFA_nop;
    /* Finalize the CIE size. */
    *PtrCie.pu32 = Ptr.u - PtrCie.u - sizeof(uint32_t);

    /*
     * Generate an FDE for the whole chunk area.
     */
    RTPTRUNION const PtrFde = Ptr;
    *Ptr.pu32++ = 123;                                      /* The CIE length will be determined later. */
    *Ptr.pu32   = Ptr.u - PtrCie.u;                         /* Negated self relative CIE address. */
    Ptr.pu32++;
    *Ptr.pu64++ = (uintptr_t)pvChunk;                       /* Absolute start PC of this FDE. */
    *Ptr.pu64++ = pExecMemAllocator->cbChunk;               /* PC range length for this PDE. */
    //*Ptr.pb++ = DW_CFA_nop; - not required for recent libgcc/glibc.
    while ((Ptr.u - PtrFde.u) & 3)
        *Ptr.pb++ = DW_CFA_nop;
    /* Finalize the FDE size. */
    *PtrFde.pu32 = Ptr.u - PtrFde.u - sizeof(uint32_t);

    /* Terminator entry. */
    *Ptr.pu32++ = 0;
    *Ptr.pu32++ = 0;            /* just to be sure... */
    Assert(Ptr.u - (uintptr_t)&pEhFrame->abEhFrame[0] <= sizeof(pEhFrame->abEhFrame));
}

#endif /* IN_RING3 && !RT_OS_WINDOWS */


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
    void *pvChunk = RTMemPageAllocEx(pExecMemAllocator->cbChunk, RTMEMPAGEALLOC_F_EXECUTABLE);
    AssertLogRelReturn(pvChunk, VERR_NO_EXEC_MEMORY);

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
        {
#ifdef IN_RING3
# ifdef RT_OS_WINDOWS
            /*
             * Register the runtime function table for this chunk.
             * We can share the data structure on windows.
             */
            /** @todo */
# else
            /*
             * Generate an .eh_frame section for the chunk and register it so
             * the unwinding code works (required for C++ exceptions and
             * probably also for longjmp()).
             */
            PIEMEXECMEMCHUNKEHFRAME pEhFrame = (PIEMEXECMEMCHUNKEHFRAME)RTMemAllocZ(sizeof(IEMEXECMEMCHUNKEHFRAME));
            if (pEhFrame)
            {
                iemExecMemAllocatorInitEhFrameForChunk(pExecMemAllocator, pEhFrame, pvChunk);
                memset(pEhFrame->abObject, 0xf6, sizeof(pEhFrame->abObject)); /* color the memory to better spot usage */
                __register_frame_info(pEhFrame->abEhFrame, pEhFrame->abObject);
            }
            else
                rc = VERR_NO_MEMORY;
# endif
            if (RT_SUCCESS(rc))
#endif
            {
                /*
                 * Finalize the adding of the chunk.
                 */
                pExecMemAllocator->aChunks[idxChunk].pvChunk  = pvChunk;
                pExecMemAllocator->aChunks[idxChunk].hHeap    = hHeap;
#if defined(IN_RING3) && !defined(RT_OS_WINDOWS)
                pExecMemAllocator->aChunks[idxChunk].pEhFrame = pEhFrame;
#endif

                pExecMemAllocator->cChunks      = idxChunk + 1;
                pExecMemAllocator->idxChunkHint = idxChunk;

                size_t const cbFree = RTHeapSimpleGetFreeSize(hHeap);
                pExecMemAllocator->cbTotal     += cbFree;
                pExecMemAllocator->cbFree      += cbFree;

                return VINF_SUCCESS;
            }
        }
    }
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
#if defined(IN_RING3) && defined(RT_OS_WINDOWS) && (defined(RT_ARCH_AMD64) || defined(RT_ARCH_ARM64))
    size_t const cbExtra = sizeof(RUNTIME_FUNCTION) + 0; /** @todo */
#else
    size_t const cbExtra = 0;
#endif
    PIEMEXECMEMALLOCATOR pExecMemAllocator = (PIEMEXECMEMALLOCATOR)RTMemAllocZ(  RT_UOFFSETOF_DYN(IEMEXECMEMALLOCATOR,
                                                                                                  aChunks[cMaxChunks])
                                                                               + cbExtra);
    AssertReturn(pExecMemAllocator, VERR_NO_MEMORY);
    pExecMemAllocator->uMagic       = IEMEXECMEMALLOCATOR_MAGIC;
    pExecMemAllocator->cbChunk      = cbChunk;
    pExecMemAllocator->cMaxChunks   = cMaxChunks;
    pExecMemAllocator->cChunks      = 0;
    pExecMemAllocator->idxChunkHint = 0;
    pExecMemAllocator->cAllocations = 0;
    pExecMemAllocator->cbTotal      = 0;
    pExecMemAllocator->cbFree       = 0;
    pExecMemAllocator->cbAllocated  = 0;
#if defined(IN_RING3) && defined(RT_OS_WINDOWS) && (defined(RT_ARCH_AMD64) || defined(RT_ARCH_ARM64))
    pExecMemAllocator->paUnwindFunctions = (PRUNTIME_FUNCTION)&pExecMemAllocator->aChunks[cMaxChunks];
    /** @todo */
#endif
    for (uint32_t i = 0; i < cMaxChunks; i++)
    {
        pExecMemAllocator->aChunks[i].hHeap    = NIL_RTHEAPSIMPLE;
        pExecMemAllocator->aChunks[i].pvChunk  = NULL;
#ifdef IN_RING0
        pExecMemAllocator->aChunks[i].hMemObj  = NIL_RTR0MEMOBJ;
#elif !defined(RT_OS_WINDOWS)
        pExecMemAllocator->aChunks[i].pEhFrame = NULL;
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

/**
 * Worker for iemExecMemAllocatorAlloc that returns @a pvRet after updating
 * the heap statistics.
 */
DECL_FORCE_INLINE(void *) iemExecMemAllocatorAllocTailCode(PIEMEXECMEMALLOCATOR pExecMemAllocator, void *pvRet,
                                                           uint32_t cbReq, uint32_t idxChunk)
{
    pExecMemAllocator->cAllocations += 1;
    pExecMemAllocator->cbAllocated  += cbReq;
    pExecMemAllocator->cbFree       -= RT_ALIGN_32(cbReq, 64);
    pExecMemAllocator->idxChunkHint  = idxChunk;
    return pvRet;
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
     * Adjust the request size as per the logic described in
     * iemExecMemAllocatorGrow and attempt to allocate it from one of the
     * existing chunks if we think we've got sufficient free memory around.
     */
    cbReq = RT_ALIGN_32(cbReq + pExecMemAllocator->cbHeapBlockHdr, 64) - pExecMemAllocator->cbHeapBlockHdr;
    if (cbReq <= pExecMemAllocator->cbFree)
    {
        uint32_t const cChunks      = pExecMemAllocator->cChunks;
        uint32_t const idxChunkHint = pExecMemAllocator->idxChunkHint < cChunks ? pExecMemAllocator->idxChunkHint : 0;
        for (uint32_t idxChunk = idxChunkHint; idxChunk < cChunks; idxChunk++)
        {
            void *pvRet = RTHeapSimpleAlloc(pExecMemAllocator->aChunks[idxChunk].hHeap, cbReq, 32);
            if (pvRet)
                return iemExecMemAllocatorAllocTailCode(pExecMemAllocator, pvRet, cbReq, idxChunk);
        }
        for (uint32_t idxChunk = 0; idxChunk < idxChunkHint; idxChunk++)
        {
            void *pvRet = RTHeapSimpleAlloc(pExecMemAllocator->aChunks[idxChunk].hHeap, cbReq, 32);
            if (pvRet)
                return iemExecMemAllocatorAllocTailCode(pExecMemAllocator, pvRet, cbReq, idxChunk);
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
        void *pvRet = RTHeapSimpleAlloc(pExecMemAllocator->aChunks[idxChunk].hHeap, cbReq, 32);
        if (pvRet)
            return iemExecMemAllocatorAllocTailCode(pExecMemAllocator, pvRet, cbReq, idxChunk);
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
    RT_NOREF(pVCpu, pv, cb);
}


/**
 * Frees executable memory.
 */
void iemExecMemAllocatorFree(PVMCPU pVCpu, void *pv, size_t cb)
{
    PIEMEXECMEMALLOCATOR pExecMemAllocator = pVCpu->iem.s.pExecMemAllocatorR3;
    Assert(pExecMemAllocator && pExecMemAllocator->uMagic == IEMEXECMEMALLOCATOR_MAGIC);
    Assert(pv);

    /* Align the size as we did when allocating the block. */
    cb = RT_ALIGN_Z(cb + pExecMemAllocator->cbHeapBlockHdr, 64) - pExecMemAllocator->cbHeapBlockHdr;

    /* Assert sanity if strict build. */
#ifdef VBOX_STRICT
    uint32_t const cChunks = pExecMemAllocator->cChunks;
    uint32_t const cbChunk = pExecMemAllocator->cbChunk;
    bool           fFound  = false;
    for (uint32_t idxChunk = 0; idxChunk < cChunks; idxChunk++)
    {
        fFound = (uintptr_t)pv - (uintptr_t)pExecMemAllocator->aChunks[idxChunk].pvChunk < cbChunk;
        if (fFound)
        {
            Assert(RTHeapSimpleSize(pExecMemAllocator->aChunks[idxChunk].hHeap, pv) == cb);
            break;
        }
    }
    Assert(fFound);
#endif

    /* Update stats while cb is freshly calculated.*/
    pExecMemAllocator->cbAllocated  -= cb;
    pExecMemAllocator->cbFree       += RT_ALIGN_Z(cb, 64);
    pExecMemAllocator->cAllocations -= 1;

    /* Do the actual freeing. */
    RTHeapSimpleFree(NIL_RTHEAPSIMPLE, pv);
}


/*********************************************************************************************************************************
*   Native Recompilation                                                                                                         *
*********************************************************************************************************************************/

/** Native code generator label types. */
typedef enum
{
    kIemNativeLabelType_Invalid = 0,
    kIemNativeLabelType_Return,
    kIemNativeLabelType_NonZeroRetOrPassUp,
    kIemNativeLabelType_End
} IEMNATIVELABELTYPE;

/** Native code generator label definition. */
typedef struct IEMNATIVELABEL
{
    /** Code offset if defined, UINT32_MAX if it needs to be generated after/in
     * the epilog. */
    uint32_t    off;
    /** The type of label (IEMNATIVELABELTYPE). */
    uint16_t    enmType;
    /** Additional label data, type specific. */
    uint16_t    uData;
} IEMNATIVELABEL;
/** Pointer to a label. */
typedef IEMNATIVELABEL *PIEMNATIVELABEL;


/** Native code generator fixup types.  */
typedef enum
{
    kIemNativeFixupType_Invalid = 0,
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
    /** AMD64 fixup: PC relative 32-bit with addend in bData. */
    kIemNativeFixupType_Rel32,
#elif defined(RT_ARCH_ARM64)
#endif
    kIemNativeFixupType_End
} IEMNATIVEFIXUPTYPE;

/** Native code generator fixup. */
typedef struct IEMNATIVEFIXUP
{
    /** Code offset of the fixup location. */
    uint32_t    off;
    /** The IEMNATIVELABEL this is a fixup for. */
    uint16_t    idxLabel;
    /** The fixup type (IEMNATIVEFIXUPTYPE). */
    uint8_t     enmType;
    /** Addend or other data. */
    int8_t      offAddend;
} IEMNATIVEFIXUP;

typedef IEMNATIVEFIXUP *PIEMNATIVEFIXUP;


/**
 * Used by TB code when encountering a non-zero status or rcPassUp after a call.
 */
IEM_DECL_IMPL_DEF(int, iemNativeHlpExecStatusCodeFiddling,(PVMCPUCC pVCpu, int rc, uint8_t idxInstr))
{
    pVCpu->iem.s.cInstructions += idxInstr;
    return VBOXSTRICTRC_VAL(iemExecStatusCodeFiddling(pVCpu, rc == VINF_IEM_REEXEC_BREAK ? VINF_SUCCESS : rc));
}


static void iemNativeReInit(PVMCPUCC pVCpu)
{
    pVCpu->iem.s.Native.cLabels   = 0;
    pVCpu->iem.s.Native.cFixups   = 0;
}


static bool iemNativeInit(PVMCPUCC pVCpu)
{
    /*
     * Try allocate all the buffers and stuff we need.
     */
    pVCpu->iem.s.Native.pInstrBuf = (PIEMNATIVEINSTR)RTMemAllocZ(_64K);
    pVCpu->iem.s.Native.paLabels  = (PIEMNATIVELABEL)RTMemAllocZ(sizeof(IEMNATIVELABEL) * _8K);
    pVCpu->iem.s.Native.paFixups  = (PIEMNATIVEFIXUP)RTMemAllocZ(sizeof(IEMNATIVEFIXUP) * _16K);
    if (RT_LIKELY(   pVCpu->iem.s.Native.pInstrBuf
                  && pVCpu->iem.s.Native.paLabels
                  && pVCpu->iem.s.Native.paFixups))
    {
        /*
         * Set the buffer & array sizes on success.
         */
        pVCpu->iem.s.Native.cInstrBufAlloc = _64K / sizeof(IEMNATIVEINSTR);
        pVCpu->iem.s.Native.cLabelsAlloc   = _8K;
        pVCpu->iem.s.Native.cFixupsAlloc   = _16K;
        iemNativeReInit(pVCpu);
        return true;
    }

    /*
     * Failed. Cleanup and the reset state.
     */
    AssertFailed();
    RTMemFree(pVCpu->iem.s.Native.pInstrBuf);
    RTMemFree(pVCpu->iem.s.Native.paLabels);
    RTMemFree(pVCpu->iem.s.Native.paFixups);
    pVCpu->iem.s.Native.pInstrBuf = NULL;
    pVCpu->iem.s.Native.paLabels  = NULL;
    pVCpu->iem.s.Native.paFixups  = NULL;
    return false;
}


static uint32_t iemNativeMakeLabel(PVMCPUCC pVCpu, IEMNATIVELABELTYPE enmType,
                                   uint32_t offWhere = UINT32_MAX, uint16_t uData = 0)
{
    /*
     * Do we have the label already?
     */
    PIEMNATIVELABEL paLabels = pVCpu->iem.s.Native.paLabels;
    uint32_t const  cLabels  = pVCpu->iem.s.Native.cLabels;
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
    if (RT_LIKELY(cLabels < pVCpu->iem.s.Native.cLabelsAlloc))
    { /* likely */ }
    else
    {
        uint32_t cNew = pVCpu->iem.s.Native.cLabelsAlloc;
        AssertReturn(cNew, UINT32_MAX);
        AssertReturn(cLabels == cNew, UINT32_MAX);
        cNew *= 2;
        AssertReturn(cNew <= _64K, UINT32_MAX); /* IEMNATIVEFIXUP::idxLabel type restrict this */
        paLabels = (PIEMNATIVELABEL)RTMemRealloc(paLabels, cNew * sizeof(paLabels[0]));
        AssertReturn(paLabels, UINT32_MAX);
        pVCpu->iem.s.Native.paLabels     = paLabels;
        pVCpu->iem.s.Native.cLabelsAlloc = cNew;
    }

    /*
     * Define a new label.
     */
    paLabels[cLabels].off     = offWhere;
    paLabels[cLabels].enmType = enmType;
    paLabels[cLabels].uData   = uData;
    pVCpu->iem.s.Native.cLabels = cLabels + 1;
    return cLabels;
}


static uint32_t iemNativeFindLabel(PVMCPUCC pVCpu, IEMNATIVELABELTYPE enmType,
                                   uint32_t offWhere = UINT32_MAX, uint16_t uData = 0)
{
    PIEMNATIVELABEL paLabels = pVCpu->iem.s.Native.paLabels;
    uint32_t const  cLabels  = pVCpu->iem.s.Native.cLabels;
    for (uint32_t i = 0; i < cLabels; i++)
        if (   paLabels[i].enmType == enmType
            && paLabels[i].uData   == uData
            && (   paLabels[i].off == offWhere
                || offWhere        == UINT32_MAX
                || paLabels[i].off == UINT32_MAX))
            return i;
    return UINT32_MAX;
}



static bool iemNativeAddFixup(PVMCPUCC pVCpu, uint32_t offWhere, uint32_t idxLabel,
                              IEMNATIVEFIXUPTYPE enmType, int8_t offAddend = 0)
{
    Assert(idxLabel <= UINT16_MAX);
    Assert((unsigned)enmType <= UINT8_MAX);

    /*
     * Make sure we've room.
     */
    PIEMNATIVEFIXUP paFixups = pVCpu->iem.s.Native.paFixups;
    uint32_t const  cFixups  = pVCpu->iem.s.Native.cFixups;
    if (RT_LIKELY(cFixups < pVCpu->iem.s.Native.cFixupsAlloc))
    { /* likely */ }
    else
    {
        uint32_t cNew = pVCpu->iem.s.Native.cFixupsAlloc;
        AssertReturn(cNew, false);
        AssertReturn(cFixups == cNew, false);
        cNew *= 2;
        AssertReturn(cNew <= _128K, false);
        paFixups = (PIEMNATIVEFIXUP)RTMemRealloc(paFixups, cNew * sizeof(paFixups[0]));
        AssertReturn(paFixups, false);
        pVCpu->iem.s.Native.paFixups     = paFixups;
        pVCpu->iem.s.Native.cFixupsAlloc = cNew;
    }

    /*
     * Add the fixup.
     */
    paFixups[cFixups].off       = offWhere;
    paFixups[cFixups].idxLabel  = (uint16_t)idxLabel;
    paFixups[cFixups].enmType   = enmType;
    paFixups[cFixups].offAddend = offAddend;
    pVCpu->iem.s.Native.cFixups = cFixups + 1;
    return true;
}


static PIEMNATIVEINSTR iemNativeInstrBufEnsureSlow(PVMCPUCC pVCpu, uint32_t off, uint32_t cInstrReq)
{
    /* Double the buffer size till we meet the request. */
    uint32_t cNew = pVCpu->iem.s.Native.cInstrBufAlloc;
    AssertReturn(cNew > 0, NULL);
    do
        cNew *= 2;
    while (cNew < off + cInstrReq);

    uint32_t const cbNew = cNew * sizeof(IEMNATIVEINSTR);
    AssertReturn(cbNew <= _2M, NULL);

    void *pvNew = RTMemRealloc(pVCpu->iem.s.Native.pInstrBuf, cbNew);
    AssertReturn(pvNew, NULL);

    pVCpu->iem.s.Native.cInstrBufAlloc   = cNew;
    return pVCpu->iem.s.Native.pInstrBuf = (PIEMNATIVEINSTR)pvNew;
}


DECL_FORCE_INLINE(PIEMNATIVEINSTR) iemNativeInstrBufEnsure(PVMCPUCC pVCpu, uint32_t off, uint32_t cInstrReq)
{
    if (RT_LIKELY(off + cInstrReq <= pVCpu->iem.s.Native.cInstrBufAlloc))
        return pVCpu->iem.s.Native.pInstrBuf;
    return iemNativeInstrBufEnsureSlow(pVCpu, off, cInstrReq);
}


/**
 * Emit a simple marker instruction to more easily tell where something starts
 * in the disassembly.
 */
uint32_t iemNativeEmitMarker(PVMCPUCC pVCpu, uint32_t off)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 1);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0x90;                    /* nop */

#elif RT_ARCH_ARM64
    uint32_t *pu32CodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 1);
    pu32CodeBuf[off++] = 0xe503201f;            /* nop? */

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitGprZero(PVMCPUCC pVCpu, uint32_t off, uint8_t iGpr)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if (iGpr >= 8)                          /* xor gpr32, gpr32 */
        pbCodeBuf[off++] = X86_OP_REX_R | X86_OP_REX_B;
    pbCodeBuf[off++] = 0x33;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGpr & 7, iGpr & 7);

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu, iGpr, uImm64);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    RT_NOREF(pVCpu);
    return off;
}


static uint32_t iemNativeEmitLoadGprImm64(PVMCPUCC pVCpu, uint32_t off, uint8_t iGpr, uint64_t uImm64)
{
    if (!uImm64)
        return iemNativeEmitGprZero(pVCpu, off, iGpr);

#ifdef RT_ARCH_AMD64
    if (uImm64 <= UINT32_MAX)
    {
        /* mov gpr, imm32 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 6);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        if (iGpr >= 8)
            pbCodeBuf[off++] = X86_OP_REX_R;
        pbCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pbCodeBuf[off++] = RT_BYTE1(uImm64);
        pbCodeBuf[off++] = RT_BYTE2(uImm64);
        pbCodeBuf[off++] = RT_BYTE3(uImm64);
        pbCodeBuf[off++] = RT_BYTE4(uImm64);
    }
    else
    {
        /* mov gpr, imm64 */
        uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 10);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        if (iGpr < 8)
            pbCodeBuf[off++] = X86_OP_REX_W;
        else
            pbCodeBuf[off++] = X86_OP_REX_W | X86_OP_REX_R;
        pbCodeBuf[off++] = 0xb8 + (iGpr & 7);
        pbCodeBuf[off++] = RT_BYTE1(uImm64);
        pbCodeBuf[off++] = RT_BYTE2(uImm64);
        pbCodeBuf[off++] = RT_BYTE3(uImm64);
        pbCodeBuf[off++] = RT_BYTE4(uImm64);
        pbCodeBuf[off++] = RT_BYTE5(uImm64);
        pbCodeBuf[off++] = RT_BYTE6(uImm64);
        pbCodeBuf[off++] = RT_BYTE7(uImm64);
        pbCodeBuf[off++] = RT_BYTE8(uImm64);
    }

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu, iGpr, uImm64);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitLoadGprFromVCpuU32(PVMCPUCC pVCpu, uint32_t off, uint8_t iGpr, uint32_t offVCpu)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 7);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /* mov reg32, mem32 */
    if (iGpr >= 8)
        pbCodeBuf[off++] = X86_OP_REX_R;
    pbCodeBuf[off++] = 0x8b;
    if (offVCpu < 128)
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, iGpr & 7, X86_GREG_xBX);
        pbCodeBuf[off++] = (uint8_t)offVCpu;
    }
    else
    {
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM4, iGpr & 7, X86_GREG_xBX);
        pbCodeBuf[off++] = RT_BYTE1(offVCpu);
        pbCodeBuf[off++] = RT_BYTE2(offVCpu);
        pbCodeBuf[off++] = RT_BYTE3(offVCpu);
        pbCodeBuf[off++] = RT_BYTE4(offVCpu);
    }

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu, idxInstr);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitLoadGprFromGpr(PVMCPUCC pVCpu, uint32_t off, uint8_t iGprDst, uint8_t iGprSrc)
{
#ifdef RT_ARCH_AMD64
    /* mov gprdst, gprsrc */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 3);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    if ((iGprDst | iGprSrc) >= 8)
        pbCodeBuf[off++] = iGprDst < 8  ? X86_OP_REX_W | X86_OP_REX_B
                         : iGprSrc >= 8 ? X86_OP_REX_W | X86_OP_REX_R | X86_OP_REX_B
                         :                X86_OP_REX_W | X86_OP_REX_R;
    else
        pbCodeBuf[off++] = X86_OP_REX_W;
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, iGprDst & 7, iGprSrc & 7);

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu, iGprDst, iGprSrc);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitCheckCallRetAndPassUp(PVMCPUCC pVCpu, uint32_t off, uint8_t idxInstr)
{
#ifdef RT_ARCH_AMD64
    /* eax = call status code.*/

    /* edx = rcPassUp */
    off = iemNativeEmitLoadGprFromVCpuU32(pVCpu, off, X86_GREG_xDX, RT_UOFFSETOF(VMCPUCC, iem.s.rcPassUp));
    AssertReturn(off != UINT32_MAX, UINT32_MAX);

    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 10);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /* edx = eax | rcPassUp*/
    pbCodeBuf[off++] = 0x0b;                    /* or edx, eax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xDX, X86_GREG_xAX);

    /* Jump to non-zero status return path, loading cl with the instruction number. */
    pbCodeBuf[off++] = 0xb0 + X86_GREG_xCX;     /* mov cl, imm8 (pCallEntry->idxInstr) */
    pbCodeBuf[off++] = idxInstr;

    pbCodeBuf[off++] = 0x0f;                    /* jnz rel32 */
    pbCodeBuf[off++] = 0x85;
    uint32_t const idxLabel = iemNativeMakeLabel(pVCpu, kIemNativeLabelType_NonZeroRetOrPassUp);
    AssertReturn(idxLabel != UINT32_MAX, UINT32_MAX);
    AssertReturn(iemNativeAddFixup(pVCpu, off, idxLabel, kIemNativeFixupType_Rel32, -4), UINT32_MAX);
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;
    pbCodeBuf[off++] = 0x00;

    /* done. */

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu, idxInstr);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitThreadedCall(PVMCPUCC pVCpu, uint32_t off, PCIEMTHRDEDCALLENTRY pCallEntry)
{
#ifdef VBOX_STRICT
    off = iemNativeEmitMarker(pVCpu, off);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
#endif

#ifdef RT_ARCH_AMD64
    /* Load the parameters and emit the call. */
# ifdef RT_OS_WINDOWS
    off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xCX, X86_GREG_xBX);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_xDX, pCallEntry->auParams[0]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_x8, pCallEntry->auParams[1]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_x9, pCallEntry->auParams[2]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
# else
    off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xDI, X86_GREG_xBX);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_xSI, pCallEntry->auParams[0]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_xDX, pCallEntry->auParams[1]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_xCX, pCallEntry->auParams[2]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);
# endif
    off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_xAX, (uintptr_t)g_apfnIemThreadedFunctions[pCallEntry->enmFunction]);
    AssertReturn(off != UINT32_MAX, UINT32_MAX);

    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 2);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0xff;                    /* call rax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

    /* Check the status code. */
    off = iemNativeEmitCheckCallRetAndPassUp(pVCpu, off, pCallEntry->idxInstr);
    AssertReturn(off != UINT32_MAX, off);


#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu, pCallEntry);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitEpilog(PVMCPUCC pVCpu, uint32_t off)
{
#ifdef RT_ARCH_AMD64
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 20);
    AssertReturn(pbCodeBuf, UINT32_MAX);

    /*
     * Successful return, so clear eax.
     */
    pbCodeBuf[off++] = 0x33;                    /* xor eax, eax */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xAX, X86_GREG_xAX);

    /*
     * Define label for common return point.
     */
    uint32_t const idxReturn = iemNativeMakeLabel(pVCpu, kIemNativeLabelType_Return, off);
    AssertReturn(idxReturn != UINT32_MAX, UINT32_MAX);

    /* Reposition esp at the r15 restore point. */
    pbCodeBuf[off++] = X86_OP_REX_W;
    pbCodeBuf[off++] = 0x8d;                    /* lea rsp, [rbp - (gcc ? 5 : 7) * 8] */
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_MEM1, X86_GREG_xSP, X86_GREG_xBP);
# ifdef RT_OS_WINDOWS
    pbCodeBuf[off++] = (uint8_t)(-7 * 8);
# else
    pbCodeBuf[off++] = (uint8_t)(-5 * 8);
# endif

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

    /*
     * Generate the rc + rcPassUp fiddling code if needed.
     */
    uint32_t idxLabel = iemNativeFindLabel(pVCpu, kIemNativeLabelType_NonZeroRetOrPassUp);
    if (idxLabel != UINT32_MAX)
    {
        Assert(pVCpu->iem.s.Native.paLabels[idxLabel].off == UINT32_MAX);
        pVCpu->iem.s.Native.paLabels[idxLabel].off = off;

        /* Call helper and jump to return point. */
# ifdef RT_OS_WINDOWS
        off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xR8, X86_GREG_xCX); /* cl = instruction number */
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xCX, X86_GREG_xBX);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xDX, X86_GREG_xAX);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
# else
        off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xDI, X86_GREG_xBX);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xSI, X86_GREG_xAX);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
        off = iemNativeEmitLoadGprFromGpr(pVCpu, off, X86_GREG_xDX, X86_GREG_xCX); /* cl = instruction number */
        AssertReturn(off != UINT32_MAX, UINT32_MAX);
# endif
        off = iemNativeEmitLoadGprImm64(pVCpu, off, X86_GREG_xAX, (uintptr_t)iemNativeHlpExecStatusCodeFiddling);
        AssertReturn(off != UINT32_MAX, UINT32_MAX);

        pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 10);
        AssertReturn(pbCodeBuf, UINT32_MAX);
        pbCodeBuf[off++] = 0xff;                    /* call rax */
        pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 2, X86_GREG_xAX);

        /* Jump to common return point. */
        uint32_t offRel = pVCpu->iem.s.Native.paLabels[idxReturn].off - (off + 2);
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
    }

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
}


static uint32_t iemNativeEmitProlog(PVMCPUCC pVCpu, uint32_t off)
{
#ifdef RT_ARCH_AMD64
    /*
     * Set up a regular xBP stack frame, pushing all non-volatile GPRs,
     * reserving 64 bytes for stack variables plus 4 non-register argument
     * slots.  Fixed register assignment: xBX = pVCpu;
     *
     * Since we always do the same register spilling, we can use the same
     * unwind description for all the code.
     */
    uint8_t *pbCodeBuf = iemNativeInstrBufEnsure(pVCpu, off, 32);
    AssertReturn(pbCodeBuf, UINT32_MAX);
    pbCodeBuf[off++] = 0x50 + X86_GREG_xBP;     /* push rbp */
    pbCodeBuf[off++] = X86_OP_REX_W;            /* mov rbp, rsp */
    pbCodeBuf[off++] = 0x8b;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, X86_GREG_xBP, X86_GREG_xSP);
    pbCodeBuf[off++] = 0x50 + X86_GREG_xBX;     /* push rbx */
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

    pbCodeBuf[off++] = X86_OP_REX_W;            /* sub rsp, byte 28h */
    pbCodeBuf[off++] = 0x83;
    pbCodeBuf[off++] = X86_MODRM_MAKE(X86_MOD_REG, 5, X86_GREG_xSP);
    pbCodeBuf[off++] = 0x40  /* for variables */
                     + 8     /* stack alignment correction */
                     + 4 * 8 /* 4 non-register arguments */
# ifdef RT_OS_WINDOWS
                     + 0x20  /* register argument spill area for windows calling convention */
# endif
                     ;

#elif RT_ARCH_ARM64
    RT_NOREF(pVCpu);
    off = UINT32_MAX;

#else
# error "port me"
#endif
    return off;
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
PIEMTB iemNativeRecompile(PVMCPUCC pVCpu, PIEMTB pTb)
{
    /*
     * The first time thru, we allocate the recompiler state, the other times
     * we just need to reset it before using it again.
     */
    if (RT_LIKELY(pVCpu->iem.s.Native.pInstrBuf))
        iemNativeReInit(pVCpu);
    else
        AssertReturn(iemNativeInit(pVCpu), pTb);

    /*
     * Emit prolog code (fixed atm).
     */
    uint32_t off = iemNativeEmitProlog(pVCpu, 0);
    AssertReturn(off != UINT32_MAX, pTb);

    /*
     * Convert the calls to native code.
     */
    PCIEMTHRDEDCALLENTRY pCallEntry = pTb->Thrd.paCalls;
    uint32_t             cCallsLeft = pTb->Thrd.cCalls;
    while (cCallsLeft-- > 0)
    {
        off = iemNativeEmitThreadedCall(pVCpu, off, pCallEntry);
        AssertReturn(off != UINT32_MAX, pTb);

        pCallEntry++;
    }

    /*
     * Emit the epilog code.
     */
    off = iemNativeEmitEpilog(pVCpu, off);
    AssertReturn(off != UINT32_MAX, pTb);

    /*
     * Make sure all labels has been defined.
     */
    PIEMNATIVELABEL const paLabels = pVCpu->iem.s.Native.paLabels;
#ifdef VBOX_STRICT
    uint32_t const        cLabels  = pVCpu->iem.s.Native.cLabels;
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
    memcpy(paFinalInstrBuf, pVCpu->iem.s.Native.pInstrBuf, off * sizeof(paFinalInstrBuf[0]));

    /*
     * Apply fixups.
     */
    PIEMNATIVEFIXUP const paFixups   = pVCpu->iem.s.Native.paFixups;
    uint32_t const        cFixups    = pVCpu->iem.s.Native.cFixups;
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

