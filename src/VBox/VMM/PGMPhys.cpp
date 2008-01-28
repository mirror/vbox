/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include <VBox/cpum.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/stam.h>
#include <VBox/rem.h>
#include <VBox/csam.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/dbg.h>
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <VBox/log.h>
#include <iprt/thread.h>
#include <iprt/string.h>



/*
 * PGMR3PhysReadByte/Word/Dword
 * PGMR3PhysWriteByte/Word/Dword
 */

#define PGMPHYSFN_READNAME  PGMR3PhysReadByte
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteByte
#define PGMPHYS_DATASIZE    1
#define PGMPHYS_DATATYPE    uint8_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadWord
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteWord
#define PGMPHYS_DATASIZE    2
#define PGMPHYS_DATATYPE    uint16_t
#include "PGMPhys.h"

#define PGMPHYSFN_READNAME  PGMR3PhysReadDword
#define PGMPHYSFN_WRITENAME PGMR3PhysWriteDword
#define PGMPHYS_DATASIZE    4
#define PGMPHYS_DATATYPE    uint32_t
#include "PGMPhys.h"




/**
 * Interface that the MMR3RamRegister(), MMR3RomRegister() and MMIO handler
 * registration APIs calls to inform PGM about memory registrations.
 *
 * It registers the physical memory range with PGM. MM is responsible
 * for the toplevel things - allocation and locking - while PGM is taking
 * care of all the details and implements the physical address space virtualization.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pvRam           HC virtual address of the RAM range. (page aligned)
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          Flags, MM_RAM_*.
 * @param   paPages         Pointer an array of physical page descriptors.
 * @param   pszDesc         Description string.
 */
PGMR3DECL(int) PGMR3PhysRegister(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc)
{
    /*
     * Validate input.
     * (Not so important because callers are only MMR3PhysRegister()
     *  and PGMR3HandlerPhysicalRegisterEx(), but anyway...)
     */
    Log(("PGMR3PhysRegister %08X %x bytes flags %x %s\n", GCPhys, cb, fFlags, pszDesc));

    Assert((fFlags & (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_DYNAMIC_ALLOC)) || paPages);
    /*Assert(!(fFlags & MM_RAM_FLAGS_RESERVED) || !paPages);*/
    Assert((fFlags == (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO)) || (fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC) || pvRam);
    /*Assert(!(fFlags & MM_RAM_FLAGS_RESERVED) || !pvRam);*/
    Assert(!(fFlags & ~0xfff));
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_P(pvRam, PAGE_SIZE) == pvRam);
    Assert(!(fFlags & ~(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_DYNAMIC_ALLOC)));
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    if (GCPhysLast < GCPhys)
    {
        AssertMsgFailed(("The range wraps! GCPhys=%VGp cb=%#x\n", GCPhys, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find range location and check for conflicts.
     */
    PPGMRAMRANGE    pPrev = NULL;
    PPGMRAMRANGE    pCur = pVM->pgm.s.pRamRangesHC;
    while (pCur)
    {
        if (GCPhys <= pCur->GCPhysLast && GCPhysLast >= pCur->GCPhys)
        {
            AssertMsgFailed(("Conflict! This cannot happen!\n"));
            return VERR_PGM_RAM_CONFLICT;
        }
        if (GCPhysLast < pCur->GCPhys)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextHC;
    }

    /*
     * Allocate RAM range.
     * Small ranges are allocated from the heap, big ones have separate mappings.
     */
    size_t          cbRam = RT_OFFSETOF(PGMRAMRANGE, aPages[cb >> PAGE_SHIFT]);
    PPGMRAMRANGE    pNew;
    RTGCPTR         GCPtrNew;
    int             rc = VERR_NO_MEMORY;
    if (cbRam > PAGE_SIZE / 2)
    {   /* large */
        cbRam = RT_ALIGN_Z(cbRam, PAGE_SIZE);
        rc = SUPPageAlloc(cbRam >> PAGE_SHIFT, (void **)&pNew);
        if (VBOX_SUCCESS(rc))
        {
            rc = MMR3HyperMapHCRam(pVM, pNew, cbRam, true, pszDesc, &GCPtrNew);
            if (VBOX_SUCCESS(rc))
            {
                Assert(MMHyperHC2GC(pVM, pNew) == GCPtrNew);
                rc = MMR3HyperReserve(pVM, PAGE_SIZE, "fence", NULL);
            }
            else
            {
                AssertMsgFailed(("MMR3HyperMapHCRam(,,%#x,,,) -> %Vrc\n", cbRam, rc));
                SUPPageFree(pNew, cbRam >> PAGE_SHIFT);
            }
        }
        else
            AssertMsgFailed(("SUPPageAlloc(%#x,,) -> %Vrc\n", cbRam >> PAGE_SHIFT, rc));

    }
/** @todo Make VGA and VMMDev register their memory at init time before the hma size is fixated. */
    if (RT_FAILURE(rc))
    {   /* small + fallback (vga) */
        rc = MMHyperAlloc(pVM, cbRam, 16, MM_TAG_PGM, (void **)&pNew);
        if (VBOX_SUCCESS(rc))
            GCPtrNew = MMHyperHC2GC(pVM, pNew);
        else
            AssertMsgFailed(("MMHyperAlloc(,%#x,,,) -> %Vrc\n", cbRam, cb));
    }
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Initialize the range.
         */
        pNew->pvHC          = pvRam;
        pNew->GCPhys        = GCPhys;
        pNew->GCPhysLast    = GCPhysLast;
        pNew->cb            = cb;
        pNew->fFlags        = fFlags;
        pNew->pavHCChunkHC  = NULL;
        pNew->pavHCChunkGC  = 0;

        unsigned iPage = cb >> PAGE_SHIFT;
        if (paPages)
        {
            while (iPage-- > 0)
            {
                pNew->aPages[iPage].HCPhys = (paPages[iPage].Phys & X86_PTE_PAE_PG_MASK) | fFlags; /** @todo PAGE FLAGS */
                pNew->aPages[iPage].u2State = PGM_PAGE_STATE_ALLOCATED;
                pNew->aPages[iPage].fWrittenTo = 0;
                pNew->aPages[iPage].fSomethingElse = 0;
                pNew->aPages[iPage].idPage = 0;
                pNew->aPages[iPage].u32B = 0;
            }
        }
        else if (fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)
        {
            /* Allocate memory for chunk to HC ptr lookup array. */
            rc = MMHyperAlloc(pVM, (cb >> PGM_DYNAMIC_CHUNK_SHIFT) * sizeof(void *), 16, MM_TAG_PGM, (void **)&pNew->pavHCChunkHC);
            AssertMsgReturn(rc == VINF_SUCCESS, ("MMHyperAlloc(,%#x,,,) -> %Vrc\n", cbRam, cb), rc);

            pNew->pavHCChunkGC = MMHyperHC2GC(pVM, pNew->pavHCChunkHC);
            Assert(pNew->pavHCChunkGC);

            /* Physical memory will be allocated on demand. */
            while (iPage-- > 0)
            {
                pNew->aPages[iPage].HCPhys = fFlags; /** @todo PAGE FLAGS */
                pNew->aPages[iPage].u2State = PGM_PAGE_STATE_ZERO;
                pNew->aPages[iPage].fWrittenTo = 0;
                pNew->aPages[iPage].fSomethingElse = 0;
                pNew->aPages[iPage].idPage = 0;
                pNew->aPages[iPage].u32B = 0;
            }
        }
        else
        {
            Assert(fFlags == (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO));
            RTHCPHYS HCPhysDummyPage = (MMR3PageDummyHCPhys(pVM) & X86_PTE_PAE_PG_MASK) | fFlags; /** @todo PAGE FLAGS */
            while (iPage-- > 0)
            {
                pNew->aPages[iPage].HCPhys = HCPhysDummyPage; /** @todo PAGE FLAGS */
                pNew->aPages[iPage].u2State = PGM_PAGE_STATE_ZERO;
                pNew->aPages[iPage].fWrittenTo = 0;
                pNew->aPages[iPage].fSomethingElse = 0;
                pNew->aPages[iPage].idPage = 0;
                pNew->aPages[iPage].u32B = 0;
            }
        }

        /*
         * Insert the new RAM range.
         */
        pgmLock(pVM);
        pNew->pNextHC = pCur;
        pNew->pNextGC = pCur ? MMHyperHC2GC(pVM, pCur) : 0;
        if (pPrev)
        {
            pPrev->pNextHC = pNew;
            pPrev->pNextGC = GCPtrNew;
        }
        else
        {
            pVM->pgm.s.pRamRangesHC = pNew;
            pVM->pgm.s.pRamRangesGC = GCPtrNew;
        }
        pgmUnlock(pVM);
    }
    return rc;
}

#ifndef VBOX_WITH_NEW_PHYS_CODE

/**
 * Register a chunk of a the physical memory range with PGM. MM is responsible
 * for the toplevel things - allocation and locking - while PGM is taking
 * care of all the details and implements the physical address space virtualization.
 *
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pvRam           HC virtual address of the RAM range. (page aligned)
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          Flags, MM_RAM_*.
 * @param   paPages         Pointer an array of physical page descriptors.
 * @param   pszDesc         Description string.
 */
PGMR3DECL(int) PGMR3PhysRegisterChunk(PVM pVM, void *pvRam, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, const SUPPAGE *paPages, const char *pszDesc)
{
    NOREF(pszDesc);

    /*
     * Validate input.
     * (Not so important because callers are only MMR3PhysRegister()
     *  and PGMR3HandlerPhysicalRegisterEx(), but anyway...)
     */
    Log(("PGMR3PhysRegisterChunk %08X %x bytes flags %x %s\n", GCPhys, cb, fFlags, pszDesc));

    Assert(paPages);
    Assert(pvRam);
    Assert(!(fFlags & ~0xfff));
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_P(pvRam, PAGE_SIZE) == pvRam);
    Assert(!(fFlags & ~(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2 | MM_RAM_FLAGS_DYNAMIC_ALLOC)));
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    Assert(VM_IS_EMT(pVM));
    Assert(!(GCPhys & PGM_DYNAMIC_CHUNK_OFFSET_MASK));
    Assert(cb == PGM_DYNAMIC_CHUNK_SIZE);

    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    if (GCPhysLast < GCPhys)
    {
        AssertMsgFailed(("The range wraps! GCPhys=%VGp cb=%#x\n", GCPhys, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find existing range location.
     */
    PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (    off < pRam->cb
            &&  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
            break;

        pRam = CTXSUFF(pRam->pNext);
    }
    AssertReturn(pRam, VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS);

    unsigned off = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
    unsigned iPage = cb >> PAGE_SHIFT;
    if (paPages)
    {
        while (iPage-- > 0)
            pRam->aPages[off + iPage].HCPhys = (paPages[iPage].Phys & X86_PTE_PAE_PG_MASK) | fFlags;  /** @todo PAGE FLAGS */
    }
    off >>= (PGM_DYNAMIC_CHUNK_SHIFT - PAGE_SHIFT);
    pRam->pavHCChunkHC[off] = pvRam;

    /* Notify the recompiler. */
    REMR3NotifyPhysRamChunkRegister(pVM, GCPhys, PGM_DYNAMIC_CHUNK_SIZE, (RTHCUINTPTR)pvRam, fFlags);

    return VINF_SUCCESS;
}


/**
 * Allocate missing physical pages for an existing guest RAM range.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 */
PGMR3DECL(int) PGM3PhysGrowRange(PVM pVM, RTGCPHYS GCPhys)
{
    /*
     * Walk range list.
     */
    pgmLock(pVM);

    PPGMRAMRANGE pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (    off < pRam->cb
            &&  (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC))
        {
            bool     fRangeExists = false;
            unsigned off = (GCPhys - pRam->GCPhys) >> PGM_DYNAMIC_CHUNK_SHIFT;

            /** @note A request made from another thread may end up in EMT after somebody else has already allocated the range. */
            if (pRam->pavHCChunkHC[off])
                fRangeExists = true;

            pgmUnlock(pVM);
            if (fRangeExists)
                return VINF_SUCCESS;
            return pgmr3PhysGrowRange(pVM, GCPhys);
        }

        pRam = CTXSUFF(pRam->pNext);
    }
    pgmUnlock(pVM);
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Allocate missing physical pages for an existing guest RAM range.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   pRamRange       RAM range
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 */
int pgmr3PhysGrowRange(PVM pVM, RTGCPHYS GCPhys)
{
    void *pvRam;
    int   rc;

    /* We must execute this function in the EMT thread, otherwise we'll run into problems. */
    if (!VM_IS_EMT(pVM))
    {
        PVMREQ pReq;

        AssertMsg(!PDMCritSectIsOwner(&pVM->pgm.s.CritSect), ("We own the PGM lock -> deadlock danger!!\n"));

        rc = VMR3ReqCall(pVM, &pReq, RT_INDEFINITE_WAIT, (PFNRT)PGM3PhysGrowRange, 2, pVM, GCPhys);
        if (VBOX_SUCCESS(rc))
        {
            rc = pReq->iStatus;
            VMR3ReqFree(pReq);
        }
        return rc;
    }

    /* Round down to chunk boundary */
    GCPhys = GCPhys & PGM_DYNAMIC_CHUNK_BASE_MASK;

    STAM_COUNTER_INC(&pVM->pgm.s.StatDynRamGrow);
    STAM_COUNTER_ADD(&pVM->pgm.s.StatDynRamTotal, PGM_DYNAMIC_CHUNK_SIZE/(1024*1024));

    Log(("pgmr3PhysGrowRange: allocate chunk of size 0x%X at %VGp\n", PGM_DYNAMIC_CHUNK_SIZE, GCPhys));

    unsigned cPages = PGM_DYNAMIC_CHUNK_SIZE >> PAGE_SHIFT;

    for (;;)
    {
        rc = SUPPageAlloc(cPages, &pvRam);
        if (VBOX_SUCCESS(rc))
        {

            rc = MMR3PhysRegisterEx(pVM, pvRam, GCPhys, PGM_DYNAMIC_CHUNK_SIZE, 0, MM_PHYS_TYPE_DYNALLOC_CHUNK, "Main Memory");
            if (VBOX_SUCCESS(rc))
                return rc;

            SUPPageFree(pvRam, cPages);
        }

        VMSTATE enmVMState = VMR3GetState(pVM);
        if (enmVMState != VMSTATE_RUNNING)
        {
            AssertMsgFailed(("Out of memory while trying to allocate a guest RAM chunk at %VGp!\n", GCPhys));
            LogRel(("PGM: Out of memory while trying to allocate a guest RAM chunk at %VGp (VMstate=%s)!\n", GCPhys, VMR3GetStateName(enmVMState)));
            return rc;
        }

        LogRel(("pgmr3PhysGrowRange: out of memory. pause until the user resumes execution.\n"));

        /* Pause first, then inform Main. */
        rc = VMR3SuspendNoSave(pVM);
        AssertRC(rc);

        VMSetRuntimeError(pVM, false, "HostMemoryLow", "Unable to allocate and lock memory. The virtual machine will be paused. Please close applications to free up memory or close the VM.");

        /* Wait for resume event; will only return in that case. If the VM is stopped, the EMT thread will be destroyed. */
        rc = VMR3WaitForResume(pVM);

        /* Retry */
        LogRel(("pgmr3PhysGrowRange: VM execution resumed -> retry.\n"));
    }
}

#endif /* !VBOX_WITH_NEW_PHYS_CODE */

/**
 * Interface MMIO handler relocation calls.
 *
 * It relocates an existing physical memory range with PGM.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhysOld       Previous GC physical address of the RAM range. (page aligned)
 * @param   GCPhysNew       New GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 */
PGMR3DECL(int) PGMR3PhysRelocate(PVM pVM, RTGCPHYS GCPhysOld, RTGCPHYS GCPhysNew, size_t cb)
{
    /*
     * Validate input.
     * (Not so important because callers are only MMR3PhysRelocate(),
     *  but anyway...)
     */
    Log(("PGMR3PhysRelocate Old %VGp New %VGp (%#x bytes)\n", GCPhysOld, GCPhysNew, cb));

    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_T(GCPhysOld, PAGE_SIZE, RTGCPHYS) == GCPhysOld);
    Assert(RT_ALIGN_T(GCPhysNew, PAGE_SIZE, RTGCPHYS) == GCPhysNew);
    RTGCPHYS GCPhysLast;
    GCPhysLast = GCPhysOld + (cb - 1);
    if (GCPhysLast < GCPhysOld)
    {
        AssertMsgFailed(("The old range wraps! GCPhys=%VGp cb=%#x\n", GCPhysOld, cb));
        return VERR_INVALID_PARAMETER;
    }
    GCPhysLast = GCPhysNew + (cb - 1);
    if (GCPhysLast < GCPhysNew)
    {
        AssertMsgFailed(("The new range wraps! GCPhys=%VGp cb=%#x\n", GCPhysNew, cb));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Find and remove old range location.
     */
    pgmLock(pVM);
    PPGMRAMRANGE    pPrev = NULL;
    PPGMRAMRANGE    pCur = pVM->pgm.s.pRamRangesHC;
    while (pCur)
    {
        if (pCur->GCPhys == GCPhysOld && pCur->cb == cb)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextHC;
    }
    if (pPrev)
    {
        pPrev->pNextHC = pCur->pNextHC;
        pPrev->pNextGC = pCur->pNextGC;
    }
    else
    {
        pVM->pgm.s.pRamRangesHC = pCur->pNextHC;
        pVM->pgm.s.pRamRangesGC = pCur->pNextGC;
    }

    /*
     * Update the range.
     */
    pCur->GCPhys    = GCPhysNew;
    pCur->GCPhysLast= GCPhysLast;
    PPGMRAMRANGE    pNew = pCur;

    /*
     * Find range location and check for conflicts.
     */
    pPrev = NULL;
    pCur = pVM->pgm.s.pRamRangesHC;
    while (pCur)
    {
        if (GCPhysNew <= pCur->GCPhysLast && GCPhysLast >= pCur->GCPhys)
        {
            AssertMsgFailed(("Conflict! This cannot happen!\n"));
            pgmUnlock(pVM);
            return VERR_PGM_RAM_CONFLICT;
        }
        if (GCPhysLast < pCur->GCPhys)
            break;

        /* next */
        pPrev = pCur;
        pCur = pCur->pNextHC;
    }

    /*
     * Reinsert the RAM range.
     */
    pNew->pNextHC = pCur;
    pNew->pNextGC = pCur ? MMHyperHC2GC(pVM, pCur) : 0;
    if (pPrev)
    {
        pPrev->pNextHC = pNew;
        pPrev->pNextGC = MMHyperHC2GC(pVM, pNew);
    }
    else
    {
        pVM->pgm.s.pRamRangesHC = pNew;
        pVM->pgm.s.pRamRangesGC = MMHyperHC2GC(pVM, pNew);
    }

    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Interface MMR3RomRegister() and MMR3PhysReserve calls to update the
 * flags of existing RAM ranges.
 *
 * @returns VBox status.
 * @param   pVM             The VM handle.
 * @param   GCPhys          GC physical address of the RAM range. (page aligned)
 * @param   cb              Size of the RAM range. (page aligned)
 * @param   fFlags          The Or flags, MM_RAM_* \#defines.
 * @param   fMask           The and mask for the flags.
 */
PGMR3DECL(int) PGMR3PhysSetFlags(PVM pVM, RTGCPHYS GCPhys, size_t cb, unsigned fFlags, unsigned fMask)
{
    Log(("PGMR3PhysSetFlags %08X %x %x %x\n", GCPhys, cb, fFlags, fMask));

    /*
     * Validate input.
     * (Not so important because caller is always MMR3RomRegister() and MMR3PhysReserve(), but anyway...)
     */
    Assert(!(fFlags & ~(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)));
    Assert(RT_ALIGN_Z(cb, PAGE_SIZE) == cb && cb);
    Assert(RT_ALIGN_T(GCPhys, PAGE_SIZE, RTGCPHYS) == GCPhys);
    RTGCPHYS GCPhysLast = GCPhys + (cb - 1);
    AssertReturn(GCPhysLast > GCPhys, VERR_INVALID_PARAMETER);

    /*
     * Lookup the range.
     */
    PPGMRAMRANGE    pRam = CTXSUFF(pVM->pgm.s.pRamRanges);
    while (pRam && GCPhys > pRam->GCPhysLast)
        pRam = CTXSUFF(pRam->pNext);
    if (    !pRam
        ||  GCPhys > pRam->GCPhysLast
        ||  GCPhysLast < pRam->GCPhys)
    {
        AssertMsgFailed(("No RAM range for %VGp-%VGp\n", GCPhys, GCPhysLast));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Update the requested flags.
     */
    RTHCPHYS fFullMask = ~(RTHCPHYS)(MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_ROM | MM_RAM_FLAGS_MMIO | MM_RAM_FLAGS_MMIO2)
                         | fMask;
    unsigned iPageEnd = (GCPhysLast - pRam->GCPhys + 1) >> PAGE_SHIFT;
    unsigned iPage    = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
    for ( ; iPage < iPageEnd; iPage++)
        pRam->aPages[iPage].HCPhys = (pRam->aPages[iPage].HCPhys & fFullMask) | fFlags; /** @todo PAGE FLAGS */

    return VINF_SUCCESS;
}


/**
 * Sets the Address Gate 20 state.
 *
 * @param   pVM         VM handle.
 * @param   fEnable     True if the gate should be enabled.
 *                      False if the gate should be disabled.
 */
PGMDECL(void) PGMR3PhysSetA20(PVM pVM, bool fEnable)
{
    LogFlow(("PGMR3PhysSetA20 %d (was %d)\n", fEnable, pVM->pgm.s.fA20Enabled));
    if (pVM->pgm.s.fA20Enabled != (RTUINT)fEnable)
    {
        pVM->pgm.s.fA20Enabled = fEnable;
        pVM->pgm.s.GCPhysA20Mask = ~(RTGCPHYS)(!fEnable << 20);
        REMR3A20Set(pVM, fEnable);
    }
}


/**
 * Tree enumeration callback for dealing with age rollover.
 * It will perform a simple compression of the current age.
 */
static DECLCALLBACK(int) pgmR3PhysChunkAgeingRolloverCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    /* Age compression - ASSUMES iNow == 4. */
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)pNode;
    if (pChunk->iAge >= UINT32_C(0xffffff00))
        pChunk->iAge = 3;
    else if (pChunk->iAge >= UINT32_C(0xfffff000))
        pChunk->iAge = 2;
    else if (pChunk->iAge)
        pChunk->iAge = 1;
    else /* iAge = 0 */
        pChunk->iAge = 4;

    /* reinsert */
    PVM pVM = (PVM)pvUser;
    RTAvllU32Remove(&pVM->pgm.s.ChunkR3Map.pAgeTree, pChunk->AgeCore.Key);
    pChunk->AgeCore.Key = pChunk->iAge;
    RTAvllU32Insert(&pVM->pgm.s.ChunkR3Map.pAgeTree, &pChunk->AgeCore);
    return 0;
}


/**
 * Tree enumeration callback that updates the chunks that have
 * been used since the last
 */
static DECLCALLBACK(int) pgmR3PhysChunkAgeingCallback(PAVLU32NODECORE pNode, void *pvUser)
{
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)pNode;
    if (!pChunk->iAge)
    {
        PVM pVM = (PVM)pvUser;
        RTAvllU32Remove(&pVM->pgm.s.ChunkR3Map.pAgeTree, pChunk->AgeCore.Key);
        pChunk->AgeCore.Key = pChunk->iAge = pVM->pgm.s.ChunkR3Map.iNow;
        RTAvllU32Insert(&pVM->pgm.s.ChunkR3Map.pAgeTree, &pChunk->AgeCore);
    }

    return 0;
}


/**
 * Performs ageing of the ring-3 chunk mappings.
 *
 * @param   pVM         The VM handle.
 */
PGMR3DECL(void) PGMR3PhysChunkAgeing(PVM pVM)
{
    pVM->pgm.s.ChunkR3Map.AgeingCountdown = RT_MIN(pVM->pgm.s.ChunkR3Map.cMax / 4, 1024);
    pVM->pgm.s.ChunkR3Map.iNow++;
    if (pVM->pgm.s.ChunkR3Map.iNow == 0)
    {
        pVM->pgm.s.ChunkR3Map.iNow = 4;
        RTAvlU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pTree, true /*fFromLeft*/, pgmR3PhysChunkAgeingRolloverCallback, pVM);
    }
    else
        RTAvlU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pTree, true /*fFromLeft*/, pgmR3PhysChunkAgeingCallback, pVM);
}


/**
 * The structure passed in the pvUser argument of pgmR3PhysChunkUnmapCandidateCallback().
 */
typedef struct PGMR3PHYSCHUNKUNMAPCB
{
    PVM                 pVM;            /**< The VM handle. */
    PPGMCHUNKR3MAP      pChunk;         /**< The chunk to unmap. */
} PGMR3PHYSCHUNKUNMAPCB, *PPGMR3PHYSCHUNKUNMAPCB;


/**
 * Callback used to find the mapping that's been unused for
 * the longest time.
 */
static DECLCALLBACK(int) pgmR3PhysChunkUnmapCandidateCallback(PAVLLU32NODECORE pNode, void *pvUser)
{
    do
    {
        PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)((uint8_t *)pNode - RT_OFFSETOF(PGMCHUNKR3MAP, AgeCore));
        if (    pChunk->iAge
            &&  !pChunk->cRefs)
        {
            /*
             * Check that it's not in any of the TLBs.
             */
            PVM pVM = ((PPGMR3PHYSCHUNKUNMAPCB)pvUser)->pVM;
            for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
                if (pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].pChunk == pChunk)
                {
                    pChunk = NULL;
                    break;
                }
            if (pChunk)
                for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.PhysTlbHC.aEntries); i++)
                    if (pVM->pgm.s.PhysTlbHC.aEntries[i].pMap == pChunk)
                    {
                        pChunk = NULL;
                        break;
                    }
            if (pChunk)
            {
                ((PPGMR3PHYSCHUNKUNMAPCB)pvUser)->pChunk = pChunk;
                return 1; /* done */
            }
        }

        /* next with the same age - this version of the AVL API doesn't enumerate the list, so we have to do it. */
        pNode = pNode->pList;
    } while (pNode);
    return 0;
}


/**
 * Finds a good candidate for unmapping when the ring-3 mapping cache is full.
 *
 * The candidate will not be part of any TLBs, so no need to flush
 * anything afterwards.
 *
 * @returns Chunk id.
 * @param   pVM         The VM handle.
 */
static int32_t pgmR3PhysChunkFindUnmapCandidate(PVM pVM)
{
    /*
     * Do tree ageing first?
     */
    if (pVM->pgm.s.ChunkR3Map.AgeingCountdown-- == 0)
        PGMR3PhysChunkAgeing(pVM);

    /*
     * Enumerate the age tree starting with the left most node.
     */
    PGMR3PHYSCHUNKUNMAPCB Args;
    Args.pVM = pVM;
    Args.pChunk = NULL;
    if (RTAvllU32DoWithAll(&pVM->pgm.s.ChunkR3Map.pAgeTree, true /*fFromLeft*/, pgmR3PhysChunkUnmapCandidateCallback, pVM))
        return Args.pChunk->Core.Key;
    return INT32_MAX;
}


/**
 * Maps the given chunk into the ring-3 mapping cache.
 *
 * This will call ring-0.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk in question.
 * @param   ppChunk     Where to store the chunk tracking structure.
 *
 * @remarks Called from within the PGM critical section.
 */
int pgmR3PhysChunkMap(PVM pVM, uint32_t idChunk, PPPGMCHUNKR3MAP ppChunk)
{
    int rc;
    /*
     * Allocate a new tracking structure first.
     */
#if 0 /* for later when we've got a separate mapping method for ring-0. */
    PPGMCHUNKR3MAP pChunk = (PPGMCHUNKR3MAP)MMR3HeapAlloc(pVM, MM_TAG_PGM_CHUNK_MAPPING, sizeof(*pChunk));
    AssertReturn(pChunk, VERR_NO_MEMORY);
#else
    PPGMCHUNKR3MAP pChunk;
    rc = MMHyperAlloc(pVM, sizeof(*pChunk), 0, MM_TAG_PGM_CHUNK_MAPPING, (void **)&pChunk);
    AssertRCReturn(rc, rc);
#endif
    pChunk->Core.Key = idChunk;
    pChunk->AgeCore.Key = pVM->pgm.s.ChunkR3Map.iNow;
    pChunk->iAge = 0;
    pChunk->cRefs = 0;
    pChunk->cPermRefs = 0;
    pChunk->pv = NULL;

    /*
     * Request the ring-0 part to map the chunk in question and if
     * necessary unmap another one to make space in the mapping cache.
     */
    GMMMAPUNMAPCHUNKREQ Req;
    Req.Hdr.u32Magic = SUPVMMR0REQHDR_MAGIC;
    Req.Hdr.cbReq = sizeof(Req);
    Req.pvR3 = NULL;
    Req.idChunkMap = idChunk;
    Req.idChunkUnmap = INT32_MAX;
    if (pVM->pgm.s.ChunkR3Map.c >= pVM->pgm.s.ChunkR3Map.cMax)
        Req.idChunkUnmap = pgmR3PhysChunkFindUnmapCandidate(pVM);
    rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_MAP_UNMAP_CHUNK, 0, &Req.Hdr);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Update the tree.
         */
        /* insert the new one. */
        AssertPtr(Req.pvR3);
        pChunk->pv = Req.pvR3;
        bool fRc = RTAvlU32Insert(&pVM->pgm.s.ChunkR3Map.pTree, &pChunk->Core);
        AssertRelease(fRc);
        pVM->pgm.s.ChunkR3Map.c++;

        fRc = RTAvllU32Insert(&pVM->pgm.s.ChunkR3Map.pAgeTree, &pChunk->AgeCore);
        AssertRelease(fRc);

        /* remove the unmapped one. */
        if (Req.idChunkUnmap != INT32_MAX)
        {
            PPGMCHUNKR3MAP pUnmappedChunk = (PPGMCHUNKR3MAP)RTAvlU32Remove(&pVM->pgm.s.ChunkR3Map.pTree, Req.idChunkUnmap);
            AssertRelease(pUnmappedChunk);
            pUnmappedChunk->pv = NULL;
            pUnmappedChunk->Core.Key = UINT32_MAX;
#if 0 /* for later when we've got a separate mapping method for ring-0. */
            MMR3HeapFree(pUnmappedChunk);
#else
            MMHyperFree(pVM, pUnmappedChunk);
#endif
            pVM->pgm.s.ChunkR3Map.c--;
        }
    }
    else
    {
        AssertRC(rc);
#if 0 /* for later when we've got a separate mapping method for ring-0. */
        MMR3HeapFree(pChunk);
#else
        MMHyperFree(pVM, pChunk);
#endif
        pChunk = NULL;
    }

    *ppChunk = pChunk;
    return rc;
}


/**
 * For VMMCALLHOST_PGM_MAP_CHUNK, considered internal.
 *
 * @returns see pgmR3PhysChunkMap.
 * @param   pVM         The VM handle.
 * @param   idChunk     The chunk to map.
 */
PDMR3DECL(int) PGMR3PhysChunkMap(PVM pVM, uint32_t idChunk)
{
    PPGMCHUNKR3MAP pChunk;
    return pgmR3PhysChunkMap(pVM, idChunk, &pChunk);
}


/**
 * Invalidates the TLB for the ring-3 mapping cache.
 *
 * @param   pVM         The VM handle.
 */
PGMR3DECL(void) PGMR3PhysChunkInvalidateTLB(PVM pVM)
{
    pgmLock(pVM);
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.ChunkR3Map.Tlb.aEntries); i++)
    {
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].idChunk = NIL_GMM_CHUNKID;
        pVM->pgm.s.ChunkR3Map.Tlb.aEntries[i].pChunk = NULL;
    }
    pgmUnlock(pVM);
}


/**
 * Response to VM_FF_PGM_NEED_HANDY_PAGES and VMMCALLHOST_PGM_ALLOCATE_HANDY_PAGES.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is not cleared in this case.
 *
 * @param   pVM         The VM handle.
 */
PDMR3DECL(int) PGMR3PhysAllocateHandyPages(PVM pVM)
{
    pgmLock(pVM);
    int rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_PGM_ALLOCATE_HANDY_PAGES, 0, NULL);
    if (rc == VERR_GMM_SEED_ME)
    {
        void *pvChunk;
        rc = SUPPageAlloc(GMM_CHUNK_SIZE >> PAGE_SHIFT, &pvChunk);
        if (VBOX_SUCCESS(rc))
            rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_GMM_SEED_CHUNK, (uintptr_t)pvChunk, NULL);
        if (VBOX_FAILURE(rc))
        {
            LogRel(("PGM: GMM Seeding failed, rc=%Vrc\n", rc));
            rc = VINF_EM_NO_MEMORY;
        }
    }
    pgmUnlock(pVM);
    Assert(rc == VINF_SUCCESS || rc == VINF_EM_NO_MEMORY);
    return rc;
}

