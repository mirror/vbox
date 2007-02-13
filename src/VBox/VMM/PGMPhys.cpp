/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#include <VBox/pdm.h>
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




/**
 * Interface MMR3RamRegister(), MMR3RomRegister() and MMIO handler
 * registration calls.
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
    size_t          cbRam = RT_OFFSETOF(PGMRAMRANGE, aHCPhys[cb >> PAGE_SHIFT]);
    PPGMRAMRANGE    pNew;
    RTGCPTR         GCPtrNew;
    int             rc;
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
                SUPPageFree(pNew);
            }
        }
        else
            AssertMsgFailed(("SUPPageAlloc(%#x,,) -> %Vrc\n", cbRam >> PAGE_SHIFT, rc));
    }
    else
    {   /* small */
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
                pNew->aHCPhys[iPage] = (paPages[iPage].Phys & X86_PTE_PAE_PG_MASK) | fFlags;
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
                pNew->aHCPhys[iPage] = fFlags;
        }
        else
        {
            Assert(fFlags == (MM_RAM_FLAGS_RESERVED | MM_RAM_FLAGS_MMIO));
            RTHCPHYS HCPhysDummyPage = (MMR3PageDummyHCPhys(pVM) & X86_PTE_PAE_PG_MASK) | fFlags;
            while (iPage-- > 0)
                pNew->aHCPhys[iPage] = HCPhysDummyPage;
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
#ifdef PGM_DYNAMIC_RAM_ALLOC
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
            pRam->aHCPhys[off + iPage] = (paPages[iPage].Phys & X86_PTE_PAE_PG_MASK) | fFlags;
    }
    off >>= (PGM_DYNAMIC_CHUNK_SHIFT - PAGE_SHIFT);
    pRam->pavHCChunkHC[off] = pvRam;

    /* Notify the recompiler. */
    REMR3NotifyPhysRamChunkRegister(pVM, GCPhys, PGM_DYNAMIC_CHUNK_SIZE, (RTHCUINTPTR)pvRam, fFlags);

    return VINF_SUCCESS;
#else /* !PGM_DYNAMIC_RAM_ALLOC */
    AssertReleaseMsgFailed(("Shouldn't ever get here when PGM_DYNAMIC_RAM_ALLOC isn't defined!\n"));
    return VERR_INTERNAL_ERROR;
#endif /* !PGM_DYNAMIC_RAM_ALLOC */
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

    unsigned    cPages = PGM_DYNAMIC_CHUNK_SIZE >> PAGE_SHIFT;
    rc = SUPPageAlloc(cPages, &pvRam);
    if (VBOX_SUCCESS(rc))
    {
        VMSTATE enmVMState = VMR3GetState(pVM);

        rc = MMR3PhysRegisterEx(pVM, pvRam, GCPhys, PGM_DYNAMIC_CHUNK_SIZE, 0, MM_PHYS_TYPE_DYNALLOC_CHUNK, "Main Memory");
        if (    VBOX_SUCCESS(rc)
            ||  enmVMState != VMSTATE_RUNNING)
        {
            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("Out of memory while trying to allocate a guest RAM chunk at %VGp!\n", GCPhys));
                LogRel(("PGM: Out of memory while trying to allocate a guest RAM chunk at %VGp (VMstate=%s)!\n", GCPhys, VMR3GetStateName(enmVMState)));
            }
            return rc;
        }

        SUPPageFree(pvRam);

        LogRel(("pgmr3PhysGrowRange: out of memory. pause until the user resumes execution.\n"));
        VMSetRuntimeError(pVM, false, "HostMemoryLow", "Unable to allocate and lock memory. The virtual machine will be paused. Please close applications to free up memory or close the VM.");
 
        rc = VMR3SuspendNoSave(pVM);
        AssertRC(rc);

        /* Wait for resume event; will only return in that case. If the VM is stopped, the EMT thread will be destroyed. */
        rc = VMR3WaitForResume(pVM);

        /* Retry */
        LogRel(("pgmr3PhysGrowRange: VM execution resumed -> retry.\n"));
        return pgmr3PhysGrowRange(pVM, GCPhys);
    }
    return rc;
}


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
        pRam->aHCPhys[iPage] = (pRam->aHCPhys[iPage] & fFullMask) | fFlags;

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
