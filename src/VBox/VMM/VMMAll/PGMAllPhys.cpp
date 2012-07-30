/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Physical Memory Addressing.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_PGM_PHYS
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/em.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include "PGMInternal.h"
#include <VBox/vmm/vm.h>
#include "PGMInline.h"
#include <VBox/param.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/asm-amd64-x86.h>
#include <VBox/log.h>
#ifdef IN_RING3
# include <iprt/thread.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Enable the physical TLB. */
#define PGM_WITH_PHYS_TLB



#ifndef IN_RING3

/**
 * \#PF Handler callback for physical memory accesses without a RC/R0 handler.
 * This simply pushes everything to the HC handler.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         Pointer to the VM.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
VMMDECL(int) pgmPhysHandlerRedirectToHC(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    NOREF(pVM); NOREF(uErrorCode); NOREF(pRegFrame); NOREF(pvFault); NOREF(GCPhysFault); NOREF(pvUser);
    return (uErrorCode & X86_TRAP_PF_RW) ? VINF_IOM_R3_MMIO_WRITE : VINF_IOM_R3_MMIO_READ;
}


/**
 * \#PF Handler callback for Guest ROM range write access.
 * We simply ignore the writes or fall back to the recompiler if we don't support the instruction.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         Pointer to the VM.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument. Pointer to the ROM range structure.
 */
VMMDECL(int) pgmPhysRomWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    int             rc;
    PPGMROMRANGE    pRom = (PPGMROMRANGE)pvUser;
    uint32_t        iPage = (GCPhysFault - pRom->GCPhys) >> PAGE_SHIFT;
    PVMCPU          pVCpu = VMMGetCpu(pVM);
    NOREF(uErrorCode); NOREF(pvFault);

    Assert(uErrorCode & X86_TRAP_PF_RW); /* This shall not be used for read access! */

    Assert(iPage < (pRom->cb >> PAGE_SHIFT));
    switch (pRom->aPages[iPage].enmProt)
    {
        case PGMROMPROT_READ_ROM_WRITE_IGNORE:
        case PGMROMPROT_READ_RAM_WRITE_IGNORE:
        {
            /*
             * If it's a simple instruction which doesn't change the cpu state
             * we will simply skip it. Otherwise we'll have to defer it to REM.
             */
            uint32_t     cbOp;
            PDISCPUSTATE pDis = &pVCpu->pgm.s.DisState;
            rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
            if (     RT_SUCCESS(rc)
                &&   pDis->uCpuMode == DISCPUMODE_32BIT  /** @todo why does this matter? */
                &&  !(pDis->fPrefix & (DISPREFIX_REPNE | DISPREFIX_REP | DISPREFIX_SEG)))
            {
                switch (pDis->bOpCode)
                {
                    /** @todo Find other instructions we can safely skip, possibly
                     * adding this kind of detection to DIS or EM. */
                    case OP_MOV:
                        pRegFrame->rip += cbOp;
                        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZGuestROMWriteHandled);
                        return VINF_SUCCESS;
                }
            }
            else if (RT_UNLIKELY(rc == VERR_EM_INTERNAL_DISAS_ERROR))
                return rc;
            break;
        }

        case PGMROMPROT_READ_RAM_WRITE_RAM:
            pRom->aPages[iPage].LiveSave.fWrittenTo = true;
            rc = PGMHandlerPhysicalPageTempOff(pVM, pRom->GCPhys, GCPhysFault & X86_PTE_PG_MASK);
            AssertRC(rc);
            break; /** @todo Must edit the shadow PT and restart the instruction, not use the interpreter! */

        case PGMROMPROT_READ_ROM_WRITE_RAM:
            /* Handle it in ring-3 because it's *way* easier there. */
            pRom->aPages[iPage].LiveSave.fWrittenTo = true;
            break;

        default:
            AssertMsgFailedReturn(("enmProt=%d iPage=%d GCPhysFault=%RGp\n",
                                   pRom->aPages[iPage].enmProt, iPage, GCPhysFault),
                                  VERR_IPE_NOT_REACHED_DEFAULT_CASE);
    }

    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZGuestROMWriteUnhandled);
    return VINF_EM_RAW_EMULATE_INSTR;
}

#endif /* IN_RING3 */

/**
 * Invalidates the RAM range TLBs.
 *
 * @param   pVM                 Pointer to the VM.
 */
void pgmPhysInvalidRamRangeTlbs(PVM pVM)
{
    pgmLock(pVM);
    for (uint32_t i = 0; i < PGM_RAMRANGE_TLB_ENTRIES; i++)
    {
        pVM->pgm.s.apRamRangesTlbR3[i] = NIL_RTR3PTR;
        pVM->pgm.s.apRamRangesTlbR0[i] = NIL_RTR0PTR;
        pVM->pgm.s.apRamRangesTlbRC[i] = NIL_RTRCPTR;
    }
    pgmUnlock(pVM);
}


/**
 * Tests if a value of type RTGCPHYS is negative if the type had been signed
 * instead of unsigned.
 *
 * @returns @c true if negative, @c false if positive or zero.
 * @param   a_GCPhys        The value to test.
 * @todo    Move me to iprt/types.h.
 */
#define RTGCPHYS_IS_NEGATIVE(a_GCPhys)  ((a_GCPhys) & ((RTGCPHYS)1 << (sizeof(RTGCPHYS)*8 - 1)))


/**
 * Slow worker for pgmPhysGetRange.
 *
 * @copydoc pgmPhysGetRange
 */
PPGMRAMRANGE pgmPhysGetRangeSlow(PVM pVM, RTGCPHYS GCPhys)
{
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,RamRangeTlbMisses));

    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangeTree);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            pVM->pgm.s.CTX_SUFF(apRamRangesTlb)[PGM_RAMRANGE_TLB_IDX(GCPhys)] = pRam;
            return pRam;
        }
        if (RTGCPHYS_IS_NEGATIVE(off))
            pRam = pRam->CTX_SUFF(pLeft);
        else
            pRam = pRam->CTX_SUFF(pRight);
    }
    return NULL;
}


/**
 * Slow worker for pgmPhysGetRangeAtOrAbove.
 *
 * @copydoc pgmPhysGetRangeAtOrAbove
 */
PPGMRAMRANGE pgmPhysGetRangeAtOrAboveSlow(PVM pVM, RTGCPHYS GCPhys)
{
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,RamRangeTlbMisses));

    PPGMRAMRANGE pLastLeft = NULL;
    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangeTree);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            pVM->pgm.s.CTX_SUFF(apRamRangesTlb)[PGM_RAMRANGE_TLB_IDX(GCPhys)] = pRam;
            return pRam;
        }
        if (RTGCPHYS_IS_NEGATIVE(off))
        {
            pLastLeft = pRam;
            pRam = pRam->CTX_SUFF(pLeft);
        }
        else
            pRam = pRam->CTX_SUFF(pRight);
    }
    return pLastLeft;
}


/**
 * Slow worker for pgmPhysGetPage.
 *
 * @copydoc pgmPhysGetPage
 */
PPGMPAGE pgmPhysGetPageSlow(PVM pVM, RTGCPHYS GCPhys)
{
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,RamRangeTlbMisses));

    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangeTree);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            pVM->pgm.s.CTX_SUFF(apRamRangesTlb)[PGM_RAMRANGE_TLB_IDX(GCPhys)] = pRam;
            return &pRam->aPages[off >> PAGE_SHIFT];
        }

        if (RTGCPHYS_IS_NEGATIVE(off))
            pRam = pRam->CTX_SUFF(pLeft);
        else
            pRam = pRam->CTX_SUFF(pRight);
    }
    return NULL;
}


/**
 * Slow worker for pgmPhysGetPageEx.
 *
 * @copydoc pgmPhysGetPageEx
 */
int pgmPhysGetPageExSlow(PVM pVM, RTGCPHYS GCPhys, PPPGMPAGE ppPage)
{
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,RamRangeTlbMisses));

    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangeTree);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            pVM->pgm.s.CTX_SUFF(apRamRangesTlb)[PGM_RAMRANGE_TLB_IDX(GCPhys)] = pRam;
            *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
            return VINF_SUCCESS;
        }

        if (RTGCPHYS_IS_NEGATIVE(off))
            pRam = pRam->CTX_SUFF(pLeft);
        else
            pRam = pRam->CTX_SUFF(pRight);
    }

    *ppPage = NULL;
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Slow worker for pgmPhysGetPageAndRangeEx.
 *
 * @copydoc pgmPhysGetPageAndRangeEx
 */
int pgmPhysGetPageAndRangeExSlow(PVM pVM, RTGCPHYS GCPhys, PPPGMPAGE ppPage, PPGMRAMRANGE *ppRam)
{
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,RamRangeTlbMisses));

    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRangeTree);
    while (pRam)
    {
        RTGCPHYS off = GCPhys - pRam->GCPhys;
        if (off < pRam->cb)
        {
            pVM->pgm.s.CTX_SUFF(apRamRangesTlb)[PGM_RAMRANGE_TLB_IDX(GCPhys)] = pRam;
            *ppRam  = pRam;
            *ppPage = &pRam->aPages[off >> PAGE_SHIFT];
            return VINF_SUCCESS;
        }

        if (RTGCPHYS_IS_NEGATIVE(off))
            pRam = pRam->CTX_SUFF(pLeft);
        else
            pRam = pRam->CTX_SUFF(pRight);
    }

    *ppRam  = NULL;
    *ppPage = NULL;
    return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
}


/**
 * Checks if Address Gate 20 is enabled or not.
 *
 * @returns true if enabled.
 * @returns false if disabled.
 * @param   pVCpu    Pointer to the VMCPU.
 */
VMMDECL(bool) PGMPhysIsA20Enabled(PVMCPU pVCpu)
{
    LogFlow(("PGMPhysIsA20Enabled %d\n", pVCpu->pgm.s.fA20Enabled));
    return pVCpu->pgm.s.fA20Enabled;
}


/**
 * Validates a GC physical address.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pVM     Pointer to the VM.
 * @param   GCPhys  The physical address to validate.
 */
VMMDECL(bool) PGMPhysIsGCPhysValid(PVM pVM, RTGCPHYS GCPhys)
{
    PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhys);
    return pPage != NULL;
}


/**
 * Checks if a GC physical address is a normal page,
 * i.e. not ROM, MMIO or reserved.
 *
 * @returns true if normal.
 * @returns false if invalid, ROM, MMIO or reserved page.
 * @param   pVM     Pointer to the VM.
 * @param   GCPhys  The physical address to check.
 */
VMMDECL(bool) PGMPhysIsGCPhysNormal(PVM pVM, RTGCPHYS GCPhys)
{
    PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhys);
    return pPage
        && PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM;
}


/**
 * Converts a GC physical address to a HC physical address.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 *
 * @param   pVM     Pointer to the VM.
 * @param   GCPhys  The GC physical address to convert.
 * @param   pHCPhys Where to store the HC physical address on success.
 */
VMMDECL(int) PGMPhysGCPhys2HCPhys(PVM pVM, RTGCPHYS GCPhys, PRTHCPHYS pHCPhys)
{
    pgmLock(pVM);
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageEx(pVM, GCPhys, &pPage);
    if (RT_SUCCESS(rc))
        *pHCPhys = PGM_PAGE_GET_HCPHYS(pPage) | (GCPhys & PAGE_OFFSET_MASK);
    pgmUnlock(pVM);
    return rc;
}


/**
 * Invalidates all page mapping TLBs.
 *
 * @param   pVM     Pointer to the VM.
 */
void pgmPhysInvalidatePageMapTLB(PVM pVM)
{
    pgmLock(pVM);
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->StatPageMapTlbFlushes);

    /* Clear the shared R0/R3 TLB completely. */
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->pgm.s.PhysTlbHC.aEntries); i++)
    {
        pVM->pgm.s.PhysTlbHC.aEntries[i].GCPhys = NIL_RTGCPHYS;
        pVM->pgm.s.PhysTlbHC.aEntries[i].pPage = 0;
        pVM->pgm.s.PhysTlbHC.aEntries[i].pMap = 0;
        pVM->pgm.s.PhysTlbHC.aEntries[i].pv = 0;
    }

    /** @todo clear the RC TLB whenever we add it. */

    pgmUnlock(pVM);
}


/**
 * Invalidates a page mapping TLB entry
 *
 * @param   pVM     Pointer to the VM.
 * @param   GCPhys  GCPhys entry to flush
 */
void pgmPhysInvalidatePageMapTLBEntry(PVM pVM, RTGCPHYS GCPhys)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->StatPageMapTlbFlushEntry);

#ifdef IN_RC
    unsigned idx = PGM_PAGER3MAPTLB_IDX(GCPhys);
    pVM->pgm.s.PhysTlbHC.aEntries[idx].GCPhys = NIL_RTGCPHYS;
    pVM->pgm.s.PhysTlbHC.aEntries[idx].pPage = 0;
    pVM->pgm.s.PhysTlbHC.aEntries[idx].pMap = 0;
    pVM->pgm.s.PhysTlbHC.aEntries[idx].pv = 0;
#else
    /* Clear the shared R0/R3 TLB entry. */
    PPGMPAGEMAPTLBE pTlbe = &pVM->pgm.s.CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    pTlbe->GCPhys = NIL_RTGCPHYS;
    pTlbe->pPage  = 0;
    pTlbe->pMap   = 0;
    pTlbe->pv     = 0;
#endif

    /** @todo clear the RC TLB whenever we add it. */
}

/**
 * Makes sure that there is at least one handy page ready for use.
 *
 * This will also take the appropriate actions when reaching water-marks.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_EM_NO_MEMORY if we're really out of memory.
 *
 * @param   pVM     Pointer to the VM.
 *
 * @remarks Must be called from within the PGM critical section. It may
 *          nip back to ring-3/0 in some cases.
 */
static int pgmPhysEnsureHandyPage(PVM pVM)
{
    AssertMsg(pVM->pgm.s.cHandyPages <= RT_ELEMENTS(pVM->pgm.s.aHandyPages), ("%d\n", pVM->pgm.s.cHandyPages));

    /*
     * Do we need to do anything special?
     */
#ifdef IN_RING3
    if (pVM->pgm.s.cHandyPages <= RT_MAX(PGM_HANDY_PAGES_SET_FF, PGM_HANDY_PAGES_R3_ALLOC))
#else
    if (pVM->pgm.s.cHandyPages <= RT_MAX(PGM_HANDY_PAGES_SET_FF, PGM_HANDY_PAGES_RZ_TO_R3))
#endif
    {
        /*
         * Allocate pages only if we're out of them, or in ring-3, almost out.
         */
#ifdef IN_RING3
        if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_R3_ALLOC)
#else
        if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_RZ_ALLOC)
#endif
        {
            Log(("PGM: cHandyPages=%u out of %u -> allocate more; VM_FF_PGM_NO_MEMORY=%RTbool\n",
                 pVM->pgm.s.cHandyPages, RT_ELEMENTS(pVM->pgm.s.aHandyPages), VM_FF_ISSET(pVM, VM_FF_PGM_NO_MEMORY) ));
#ifdef IN_RING3
            int rc = PGMR3PhysAllocateHandyPages(pVM);
#else
            int rc = VMMRZCallRing3NoCpu(pVM, VMMCALLRING3_PGM_ALLOCATE_HANDY_PAGES, 0);
#endif
            if (RT_UNLIKELY(rc != VINF_SUCCESS))
            {
                if (RT_FAILURE(rc))
                    return rc;
                AssertMsgReturn(rc == VINF_EM_NO_MEMORY, ("%Rrc\n", rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                if (!pVM->pgm.s.cHandyPages)
                {
                    LogRel(("PGM: no more handy pages!\n"));
                    return VERR_EM_NO_MEMORY;
                }
                Assert(VM_FF_ISSET(pVM, VM_FF_PGM_NEED_HANDY_PAGES));
                Assert(VM_FF_ISSET(pVM, VM_FF_PGM_NO_MEMORY));
#ifdef IN_RING3
# ifdef VBOX_WITH_REM
                 REMR3NotifyFF(pVM);
# endif
#else
                VMCPU_FF_SET(VMMGetCpu(pVM), VMCPU_FF_TO_R3); /* paranoia */
#endif
            }
            AssertMsgReturn(    pVM->pgm.s.cHandyPages > 0
                            &&  pVM->pgm.s.cHandyPages <= RT_ELEMENTS(pVM->pgm.s.aHandyPages),
                            ("%u\n", pVM->pgm.s.cHandyPages),
                            VERR_PGM_HANDY_PAGE_IPE);
        }
        else
        {
            if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_SET_FF)
                VM_FF_SET(pVM, VM_FF_PGM_NEED_HANDY_PAGES);
#ifndef IN_RING3
            if (pVM->pgm.s.cHandyPages <= PGM_HANDY_PAGES_RZ_TO_R3)
            {
                Log(("PGM: VM_FF_TO_R3 - cHandyPages=%u out of %u\n", pVM->pgm.s.cHandyPages, RT_ELEMENTS(pVM->pgm.s.aHandyPages)));
                VMCPU_FF_SET(VMMGetCpu(pVM), VMCPU_FF_TO_R3);
            }
#endif
        }
    }

    return VINF_SUCCESS;
}


/**
 * Replace a zero or shared page with new page that we can write to.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success, pPage is modified.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_EM_NO_MEMORY if we're totally out of memory.
 *
 * @todo    Propagate VERR_EM_NO_MEMORY up the call tree.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure. This will
 *                      be modified on success.
 * @param   GCPhys      The address of the page.
 *
 * @remarks Must be called from within the PGM critical section. It may
 *          nip back to ring-3/0 in some cases.
 *
 * @remarks This function shouldn't really fail, however if it does
 *          it probably means we've screwed up the size of handy pages and/or
 *          the low-water mark. Or, that some device I/O is causing a lot of
 *          pages to be allocated while while the host is in a low-memory
 *          condition. This latter should be handled elsewhere and in a more
 *          controlled manner, it's on the @bugref{3170} todo list...
 */
int pgmPhysAllocPage(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    LogFlow(("pgmPhysAllocPage: %R[pgmpage] %RGp\n", pPage, GCPhys));

    /*
     * Prereqs.
     */
    PGM_LOCK_ASSERT_OWNER(pVM);
    AssertMsg(PGM_PAGE_IS_ZERO(pPage) || PGM_PAGE_IS_SHARED(pPage), ("%R[pgmpage] %RGp\n", pPage, GCPhys));
    Assert(!PGM_PAGE_IS_MMIO(pPage));

# ifdef PGM_WITH_LARGE_PAGES
    /*
     * Try allocate a large page if applicable.
     */
    if (    PGMIsUsingLargePages(pVM)
        &&  PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_RAM)
    {
        RTGCPHYS GCPhysBase = GCPhys & X86_PDE2M_PAE_PG_MASK;
        PPGMPAGE pBasePage;

        int rc = pgmPhysGetPageEx(pVM, GCPhysBase, &pBasePage);
        AssertRCReturn(rc, rc);     /* paranoia; can't happen. */
        if (PGM_PAGE_GET_PDE_TYPE(pBasePage) == PGM_PAGE_PDE_TYPE_DONTCARE)
        {
            rc = pgmPhysAllocLargePage(pVM, GCPhys);
            if (rc == VINF_SUCCESS)
                return rc;
        }
        /* Mark the base as type page table, so we don't check over and over again. */
        PGM_PAGE_SET_PDE_TYPE(pVM, pBasePage, PGM_PAGE_PDE_TYPE_PT);

        /* fall back to 4KB pages. */
    }
# endif

    /*
     * Flush any shadow page table mappings of the page.
     * When VBOX_WITH_NEW_LAZY_PAGE_ALLOC isn't defined, there shouldn't be any.
     */
    bool fFlushTLBs = false;
    int rc = pgmPoolTrackUpdateGCPhys(pVM, GCPhys, pPage, true /*fFlushTLBs*/, &fFlushTLBs);
    AssertMsgReturn(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3, ("%Rrc\n", rc), RT_FAILURE(rc) ? rc : VERR_IPE_UNEXPECTED_STATUS);

    /*
     * Ensure that we've got a page handy, take it and use it.
     */
    int rc2 = pgmPhysEnsureHandyPage(pVM);
    if (RT_FAILURE(rc2))
    {
        if (fFlushTLBs)
            PGM_INVL_ALL_VCPU_TLBS(pVM);
        Assert(rc2 == VERR_EM_NO_MEMORY);
        return rc2;
    }
    /* re-assert preconditions since pgmPhysEnsureHandyPage may do a context switch. */
    PGM_LOCK_ASSERT_OWNER(pVM);
    AssertMsg(PGM_PAGE_IS_ZERO(pPage) || PGM_PAGE_IS_SHARED(pPage), ("%R[pgmpage] %RGp\n", pPage, GCPhys));
    Assert(!PGM_PAGE_IS_MMIO(pPage));

    uint32_t iHandyPage = --pVM->pgm.s.cHandyPages;
    AssertMsg(iHandyPage < RT_ELEMENTS(pVM->pgm.s.aHandyPages), ("%d\n", iHandyPage));
    Assert(pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys != NIL_RTHCPHYS);
    Assert(!(pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys & ~X86_PTE_PAE_PG_MASK));
    Assert(pVM->pgm.s.aHandyPages[iHandyPage].idPage != NIL_GMM_PAGEID);
    Assert(pVM->pgm.s.aHandyPages[iHandyPage].idSharedPage == NIL_GMM_PAGEID);

    /*
     * There are one or two action to be taken the next time we allocate handy pages:
     *      - Tell the GMM (global memory manager) what the page is being used for.
     *        (Speeds up replacement operations - sharing and defragmenting.)
     *      - If the current backing is shared, it must be freed.
     */
    const RTHCPHYS HCPhys = pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys;
    pVM->pgm.s.aHandyPages[iHandyPage].HCPhysGCPhys = GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK;

    void const *pvSharedPage = NULL;
    if (PGM_PAGE_IS_SHARED(pPage))
    {
        /* Mark this shared page for freeing/dereferencing. */
        pVM->pgm.s.aHandyPages[iHandyPage].idSharedPage = PGM_PAGE_GET_PAGEID(pPage);
        Assert(PGM_PAGE_GET_PAGEID(pPage) != NIL_GMM_PAGEID);

        Log(("PGM: Replaced shared page %#x at %RGp with %#x / %RHp\n", PGM_PAGE_GET_PAGEID(pPage),
             GCPhys, pVM->pgm.s.aHandyPages[iHandyPage].idPage, HCPhys));
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PageReplaceShared));
        pVM->pgm.s.cSharedPages--;

        /* Grab the address of the page so we can make a copy later on. (safe) */
        rc = pgmPhysPageMapReadOnly(pVM, pPage, GCPhys, &pvSharedPage);
        AssertRC(rc);
    }
    else
    {
        Log2(("PGM: Replaced zero page %RGp with %#x / %RHp\n", GCPhys, pVM->pgm.s.aHandyPages[iHandyPage].idPage, HCPhys));
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->StatRZPageReplaceZero);
        pVM->pgm.s.cZeroPages--;
    }

    /*
     * Do the PGMPAGE modifications.
     */
    pVM->pgm.s.cPrivatePages++;
    PGM_PAGE_SET_HCPHYS(pVM, pPage, HCPhys);
    PGM_PAGE_SET_PAGEID(pVM, pPage, pVM->pgm.s.aHandyPages[iHandyPage].idPage);
    PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
    PGM_PAGE_SET_PDE_TYPE(pVM, pPage, PGM_PAGE_PDE_TYPE_PT);
    pgmPhysInvalidatePageMapTLBEntry(pVM, GCPhys);

    /* Copy the shared page contents to the replacement page. */
    if (pvSharedPage)
    {
        /* Get the virtual address of the new page. */
        PGMPAGEMAPLOCK  PgMpLck;
        void           *pvNewPage;
        rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvNewPage, &PgMpLck); AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            memcpy(pvNewPage, pvSharedPage, PAGE_SIZE); /** @todo todo write ASMMemCopyPage */
            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
        }
    }

    if (    fFlushTLBs
        &&  rc != VINF_PGM_GCPHYS_ALIASED)
        PGM_INVL_ALL_VCPU_TLBS(pVM);
    return rc;
}

#ifdef PGM_WITH_LARGE_PAGES

/**
 * Replace a 2 MB range of zero pages with new pages that we can write to.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success, pPage is modified.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_EM_NO_MEMORY if we're totally out of memory.
 *
 * @todo    Propagate VERR_EM_NO_MEMORY up the call tree.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The address of the page.
 *
 * @remarks Must be called from within the PGM critical section. It may
 *          nip back to ring-3/0 in some cases.
 */
int pgmPhysAllocLargePage(PVM pVM, RTGCPHYS GCPhys)
{
    RTGCPHYS GCPhysBase = GCPhys & X86_PDE2M_PAE_PG_MASK;
    LogFlow(("pgmPhysAllocLargePage: %RGp base %RGp\n", GCPhys, GCPhysBase));

    /*
     * Prereqs.
     */
    PGM_LOCK_ASSERT_OWNER(pVM);
    Assert(PGMIsUsingLargePages(pVM));

    PPGMPAGE pFirstPage;
    int rc = pgmPhysGetPageEx(pVM, GCPhysBase, &pFirstPage);
    if (    RT_SUCCESS(rc)
        &&  PGM_PAGE_GET_TYPE(pFirstPage) == PGMPAGETYPE_RAM)
    {
        unsigned uPDEType = PGM_PAGE_GET_PDE_TYPE(pFirstPage);

        /* Don't call this function for already allocated pages. */
        Assert(uPDEType != PGM_PAGE_PDE_TYPE_PDE);

        if (   uPDEType == PGM_PAGE_PDE_TYPE_DONTCARE
            && PGM_PAGE_GET_STATE(pFirstPage) == PGM_PAGE_STATE_ZERO)
        {
            /* Lazy approach: check all pages in the 2 MB range.
             * The whole range must be ram and unallocated. */
            GCPhys = GCPhysBase;
            unsigned iPage;
            for (iPage = 0; iPage < _2M/PAGE_SIZE; iPage++)
            {
                PPGMPAGE pSubPage;
                rc = pgmPhysGetPageEx(pVM, GCPhys, &pSubPage);
                if  (   RT_FAILURE(rc)
                     || PGM_PAGE_GET_TYPE(pSubPage)  != PGMPAGETYPE_RAM      /* Anything other than ram implies monitoring. */
                     || PGM_PAGE_GET_STATE(pSubPage) != PGM_PAGE_STATE_ZERO) /* Allocated, monitored or shared means we can't use a large page here */
                {
                    LogFlow(("Found page %RGp with wrong attributes (type=%d; state=%d); cancel check. rc=%d\n", GCPhys, PGM_PAGE_GET_TYPE(pSubPage), PGM_PAGE_GET_STATE(pSubPage), rc));
                    break;
                }
                Assert(PGM_PAGE_GET_PDE_TYPE(pSubPage) == PGM_PAGE_PDE_TYPE_DONTCARE);
                GCPhys += PAGE_SIZE;
            }
            if (iPage != _2M/PAGE_SIZE)
            {
                /* Failed. Mark as requiring a PT so we don't check the whole thing again in the future. */
                STAM_REL_COUNTER_INC(&pVM->pgm.s.StatLargePageRefused);
                PGM_PAGE_SET_PDE_TYPE(pVM, pFirstPage, PGM_PAGE_PDE_TYPE_PT);
                return VERR_PGM_INVALID_LARGE_PAGE_RANGE;
            }

            /*
             * Do the allocation.
             */
# ifdef IN_RING3
            rc = PGMR3PhysAllocateLargeHandyPage(pVM, GCPhysBase);
# else
            rc = VMMRZCallRing3NoCpu(pVM, VMMCALLRING3_PGM_ALLOCATE_LARGE_HANDY_PAGE, GCPhysBase);
# endif
            if (RT_SUCCESS(rc))
            {
                Assert(PGM_PAGE_GET_STATE(pFirstPage) == PGM_PAGE_STATE_ALLOCATED);
                pVM->pgm.s.cLargePages++;
                return VINF_SUCCESS;
            }

            /* If we fail once, it most likely means the host's memory is too
               fragmented; don't bother trying again. */
            LogFlow(("pgmPhysAllocLargePage failed with %Rrc\n", rc));
            PGMSetLargePageUsage(pVM, false);
            return rc;
        }
    }
    return VERR_PGM_INVALID_LARGE_PAGE_RANGE;
}


/**
 * Recheck the entire 2 MB range to see if we can use it again as a large page.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success, the large page can be used again
 * @retval  VERR_PGM_INVALID_LARGE_PAGE_RANGE if it can't be reused
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The address of the page.
 * @param   pLargePage  Page structure of the base page
 */
int pgmPhysRecheckLargePage(PVM pVM, RTGCPHYS GCPhys, PPGMPAGE pLargePage)
{
    STAM_REL_COUNTER_INC(&pVM->pgm.s.StatLargePageRecheck);

    GCPhys &= X86_PDE2M_PAE_PG_MASK;

    /* Check the base page. */
    Assert(PGM_PAGE_GET_PDE_TYPE(pLargePage) == PGM_PAGE_PDE_TYPE_PDE_DISABLED);
    if (    PGM_PAGE_GET_STATE(pLargePage) != PGM_PAGE_STATE_ALLOCATED
        ||  PGM_PAGE_GET_TYPE(pLargePage) != PGMPAGETYPE_RAM
        ||  PGM_PAGE_GET_HNDL_PHYS_STATE(pLargePage) != PGM_PAGE_HNDL_PHYS_STATE_NONE)
    {
        LogFlow(("pgmPhysRecheckLargePage: checks failed for base page %x %x %x\n", PGM_PAGE_GET_STATE(pLargePage), PGM_PAGE_GET_TYPE(pLargePage), PGM_PAGE_GET_HNDL_PHYS_STATE(pLargePage)));
        return VERR_PGM_INVALID_LARGE_PAGE_RANGE;
    }

    STAM_PROFILE_START(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,IsValidLargePage), a);
    /* Check all remaining pages in the 2 MB range. */
    unsigned i;
    GCPhys += PAGE_SIZE;
    for (i = 1; i < _2M/PAGE_SIZE; i++)
    {
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageEx(pVM, GCPhys, &pPage);
        AssertRCBreak(rc);

        if (    PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED
            ||  PGM_PAGE_GET_PDE_TYPE(pPage) != PGM_PAGE_PDE_TYPE_PDE
            ||  PGM_PAGE_GET_TYPE(pPage) != PGMPAGETYPE_RAM
            ||  PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) != PGM_PAGE_HNDL_PHYS_STATE_NONE)
        {
            LogFlow(("pgmPhysRecheckLargePage: checks failed for page %d; %x %x %x\n", i, PGM_PAGE_GET_STATE(pPage), PGM_PAGE_GET_TYPE(pPage), PGM_PAGE_GET_HNDL_PHYS_STATE(pPage)));
            break;
        }

        GCPhys += PAGE_SIZE;
    }
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,IsValidLargePage), a);

    if (i == _2M/PAGE_SIZE)
    {
        PGM_PAGE_SET_PDE_TYPE(pVM, pLargePage, PGM_PAGE_PDE_TYPE_PDE);
        pVM->pgm.s.cLargePagesDisabled--;
        Log(("pgmPhysRecheckLargePage: page %RGp can be reused!\n", GCPhys - _2M));
        return VINF_SUCCESS;
    }

    return VERR_PGM_INVALID_LARGE_PAGE_RANGE;
}

#endif /* PGM_WITH_LARGE_PAGES */

/**
 * Deal with a write monitored page.
 *
 * @returns VBox strict status code.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure.
 *
 * @remarks Called from within the PGM critical section.
 */
void pgmPhysPageMakeWriteMonitoredWritable(PVM pVM, PPGMPAGE pPage)
{
    Assert(PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_WRITE_MONITORED);
    PGM_PAGE_SET_WRITTEN_TO(pVM, pPage);
    PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
    Assert(pVM->pgm.s.cMonitoredPages > 0);
    pVM->pgm.s.cMonitoredPages--;
    pVM->pgm.s.cWrittenToPages++;
}


/**
 * Deal with pages that are not writable, i.e. not in the ALLOCATED state.
 *
 * @returns VBox strict status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 *
 * @remarks Called from within the PGM critical section.
 */
int pgmPhysPageMakeWritable(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    PGM_LOCK_ASSERT_OWNER(pVM);
    switch (PGM_PAGE_GET_STATE(pPage))
    {
        case PGM_PAGE_STATE_WRITE_MONITORED:
            pgmPhysPageMakeWriteMonitoredWritable(pVM, pPage);
            /* fall thru */
        default: /* to shut up GCC */
        case PGM_PAGE_STATE_ALLOCATED:
            return VINF_SUCCESS;

        /*
         * Zero pages can be dummy pages for MMIO or reserved memory,
         * so we need to check the flags before joining cause with
         * shared page replacement.
         */
        case PGM_PAGE_STATE_ZERO:
            if (PGM_PAGE_IS_MMIO(pPage))
                return VERR_PGM_PHYS_PAGE_RESERVED;
            /* fall thru */
        case PGM_PAGE_STATE_SHARED:
            return pgmPhysAllocPage(pVM, pPage, GCPhys);

        /* Not allowed to write to ballooned pages. */
        case PGM_PAGE_STATE_BALLOONED:
            return VERR_PGM_PHYS_PAGE_BALLOONED;
    }
}


/**
 * Internal usage: Map the page specified by its GMM ID.
 *
 * This is similar to pgmPhysPageMap
 *
 * @returns VBox status code.
 *
 * @param   pVM         Pointer to the VM.
 * @param   idPage      The Page ID.
 * @param   HCPhys      The physical address (for RC).
 * @param   ppv         Where to store the mapping address.
 *
 * @remarks Called from within the PGM critical section.  The mapping is only
 *          valid while you are inside this section.
 */
int pgmPhysPageMapByPageID(PVM pVM, uint32_t idPage, RTHCPHYS HCPhys, void **ppv)
{
    /*
     * Validation.
     */
    PGM_LOCK_ASSERT_OWNER(pVM);
    AssertReturn(HCPhys && !(HCPhys & PAGE_OFFSET_MASK), VERR_INVALID_PARAMETER);
    const uint32_t idChunk = idPage >> GMM_CHUNKID_SHIFT;
    AssertReturn(idChunk != NIL_GMM_CHUNKID, VERR_INVALID_PARAMETER);

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /*
     * Map it by HCPhys.
     */
    return pgmRZDynMapHCPageInlined(VMMGetCpu(pVM), HCPhys, ppv  RTLOG_COMMA_SRC_POS);

#else
    /*
     * Find/make Chunk TLB entry for the mapping chunk.
     */
    PPGMCHUNKR3MAP pMap;
    PPGMCHUNKR3MAPTLBE pTlbe = &pVM->pgm.s.ChunkR3Map.Tlb.aEntries[PGM_CHUNKR3MAPTLB_IDX(idChunk)];
    if (pTlbe->idChunk == idChunk)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,ChunkR3MapTlbHits));
        pMap = pTlbe->pChunk;
    }
    else
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,ChunkR3MapTlbMisses));

        /*
         * Find the chunk, map it if necessary.
         */
        pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
        if (pMap)
            pMap->iLastUsed = pVM->pgm.s.ChunkR3Map.iNow;
        else
        {
# ifdef IN_RING0
            int rc = VMMRZCallRing3NoCpu(pVM, VMMCALLRING3_PGM_MAP_CHUNK, idChunk);
            AssertRCReturn(rc, rc);
            pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
            Assert(pMap);
# else
            int rc = pgmR3PhysChunkMap(pVM, idChunk, &pMap);
            if (RT_FAILURE(rc))
                return rc;
# endif
        }

        /*
         * Enter it into the Chunk TLB.
         */
        pTlbe->idChunk = idChunk;
        pTlbe->pChunk = pMap;
    }

    *ppv = (uint8_t *)pMap->pv + ((idPage &GMM_PAGEID_IDX_MASK) << PAGE_SHIFT);
    return VINF_SUCCESS;
#endif
}


/**
 * Maps a page into the current virtual address space so it can be accessed.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 * @param   ppMap       Where to store the address of the mapping tracking structure.
 * @param   ppv         Where to store the mapping address of the page. The page
 *                      offset is masked off!
 *
 * @remarks Called from within the PGM critical section.
 */
static int pgmPhysPageMapCommon(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, PPPGMPAGEMAP ppMap, void **ppv)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /*
     * Just some sketchy GC/R0-darwin code.
     */
    *ppMap = NULL;
    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(pPage);
    Assert(HCPhys != pVM->pgm.s.HCPhysZeroPg);
    pgmRZDynMapHCPageInlined(VMMGetCpu(pVM), HCPhys, ppv RTLOG_COMMA_SRC_POS);
    NOREF(GCPhys);
    return VINF_SUCCESS;

#else /* IN_RING3 || IN_RING0 */


    /*
     * Special case: ZERO and MMIO2 pages.
     */
    const uint32_t idChunk = PGM_PAGE_GET_CHUNKID(pPage);
    if (idChunk == NIL_GMM_CHUNKID)
    {
        AssertMsgReturn(PGM_PAGE_GET_PAGEID(pPage) == NIL_GMM_PAGEID, ("pPage=%R[pgmpage]\n", pPage), VERR_PGM_PHYS_PAGE_MAP_IPE_1);
        if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2)
        {
            /* Lookup the MMIO2 range and use pvR3 to calc the address. */
            PPGMRAMRANGE pRam = pgmPhysGetRange(pVM, GCPhys);
            AssertMsgReturn(pRam || !pRam->pvR3, ("pRam=%p pPage=%R[pgmpage]\n", pRam, pPage), VERR_PGM_PHYS_PAGE_MAP_IPE_2);
            *ppv = (void *)((uintptr_t)pRam->pvR3 + (uintptr_t)((GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK) - pRam->GCPhys));
        }
        else if (PGM_PAGE_GET_TYPE(pPage) == PGMPAGETYPE_MMIO2_ALIAS_MMIO)
        {
            /** @todo deal with aliased MMIO2 pages somehow...
             * One solution would be to seed MMIO2 pages to GMM and get unique Page IDs for
             * them, that would also avoid this mess. It would actually be kind of
             * elegant... */
            AssertLogRelMsgFailedReturn(("%RGp\n", GCPhys), VERR_PGM_MAP_MMIO2_ALIAS_MMIO);
        }
        else
        {
            /** @todo handle MMIO2 */
            AssertMsgReturn(PGM_PAGE_IS_ZERO(pPage), ("pPage=%R[pgmpage]\n", pPage), VERR_PGM_PHYS_PAGE_MAP_IPE_3);
            AssertMsgReturn(PGM_PAGE_GET_HCPHYS(pPage) == pVM->pgm.s.HCPhysZeroPg,
                            ("pPage=%R[pgmpage]\n", pPage),
                            VERR_PGM_PHYS_PAGE_MAP_IPE_4);
            *ppv = pVM->pgm.s.CTXALLSUFF(pvZeroPg);
        }
        *ppMap = NULL;
        return VINF_SUCCESS;
    }

    /*
     * Find/make Chunk TLB entry for the mapping chunk.
     */
    PPGMCHUNKR3MAP pMap;
    PPGMCHUNKR3MAPTLBE pTlbe = &pVM->pgm.s.ChunkR3Map.Tlb.aEntries[PGM_CHUNKR3MAPTLB_IDX(idChunk)];
    if (pTlbe->idChunk == idChunk)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,ChunkR3MapTlbHits));
        pMap = pTlbe->pChunk;
        AssertPtr(pMap->pv);
    }
    else
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,ChunkR3MapTlbMisses));

        /*
         * Find the chunk, map it if necessary.
         */
        pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
        if (pMap)
        {
            AssertPtr(pMap->pv);
            pMap->iLastUsed = pVM->pgm.s.ChunkR3Map.iNow;
        }
        else
        {
#ifdef IN_RING0
            int rc = VMMRZCallRing3NoCpu(pVM, VMMCALLRING3_PGM_MAP_CHUNK, idChunk);
            AssertRCReturn(rc, rc);
            pMap = (PPGMCHUNKR3MAP)RTAvlU32Get(&pVM->pgm.s.ChunkR3Map.pTree, idChunk);
            Assert(pMap);
#else
            int rc = pgmR3PhysChunkMap(pVM, idChunk, &pMap);
            if (RT_FAILURE(rc))
                return rc;
#endif
            AssertPtr(pMap->pv);
        }

        /*
         * Enter it into the Chunk TLB.
         */
        pTlbe->idChunk = idChunk;
        pTlbe->pChunk = pMap;
    }

    *ppv = (uint8_t *)pMap->pv + (PGM_PAGE_GET_PAGE_IN_CHUNK(pPage) << PAGE_SHIFT);
    *ppMap = pMap;
    return VINF_SUCCESS;
#endif /* IN_RING3 */
}


/**
 * Combination of pgmPhysPageMakeWritable and pgmPhysPageMapWritable.
 *
 * This is typically used is paths where we cannot use the TLB methods (like ROM
 * pages) or where there is no point in using them since we won't get many hits.
 *
 * @returns VBox strict status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_SYNC_CR3 on success and a page pool flush is pending.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 * @param   ppv         Where to store the mapping address of the page. The page
 *                      offset is masked off!
 *
 * @remarks Called from within the PGM critical section.  The mapping is only
 *          valid while you are inside section.
 */
int pgmPhysPageMakeWritableAndMap(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void **ppv)
{
    int rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
    if (RT_SUCCESS(rc))
    {
        AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* returned */, ("%Rrc\n", rc));
        PPGMPAGEMAP pMapIgnore;
        int rc2 = pgmPhysPageMapCommon(pVM, pPage, GCPhys, &pMapIgnore, ppv);
        if (RT_FAILURE(rc2)) /* preserve rc */
            rc = rc2;
    }
    return rc;
}


/**
 * Maps a page into the current virtual address space so it can be accessed for
 * both writing and reading.
 *
 * This is typically used is paths where we cannot use the TLB methods (like ROM
 * pages) or where there is no point in using them since we won't get many hits.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure. Must be in the
 *                      allocated state.
 * @param   GCPhys      The address of the page.
 * @param   ppv         Where to store the mapping address of the page. The page
 *                      offset is masked off!
 *
 * @remarks Called from within the PGM critical section.  The mapping is only
 *          valid while you are inside section.
 */
int pgmPhysPageMap(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void **ppv)
{
    Assert(PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_ALLOCATED);
    PPGMPAGEMAP pMapIgnore;
    return pgmPhysPageMapCommon(pVM, pPage, GCPhys, &pMapIgnore, ppv);
}


/**
 * Maps a page into the current virtual address space so it can be accessed for
 * reading.
 *
 * This is typically used is paths where we cannot use the TLB methods (like ROM
 * pages) or where there is no point in using them since we won't get many hits.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The physical page tracking structure.
 * @param   GCPhys      The address of the page.
 * @param   ppv         Where to store the mapping address of the page. The page
 *                      offset is masked off!
 *
 * @remarks Called from within the PGM critical section.  The mapping is only
 *          valid while you are inside this section.
 */
int pgmPhysPageMapReadOnly(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void const **ppv)
{
    PPGMPAGEMAP pMapIgnore;
    return pgmPhysPageMapCommon(pVM, pPage, GCPhys, &pMapIgnore, (void **)ppv);
}

#if !defined(IN_RC) && !defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

/**
 * Load a guest page into the ring-3 physical TLB.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 * @param   pPGM        The PGM instance pointer.
 * @param   GCPhys      The guest physical address in question.
 */
int pgmPhysPageLoadIntoTlb(PVM pVM, RTGCPHYS GCPhys)
{
    PGM_LOCK_ASSERT_OWNER(pVM);

    /*
     * Find the ram range and page and hand it over to the with-page function.
     * 99.8% of requests are expected to be in the first range.
     */
    PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhys);
    if (!pPage)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PageMapTlbMisses));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    return pgmPhysPageLoadIntoTlbWithPage(pVM, pPage, GCPhys);
}


/**
 * Load a guest page into the ring-3 physical TLB.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       Pointer to the PGMPAGE structure corresponding to
 *                      GCPhys.
 * @param   GCPhys      The guest physical address in question.
 */
int pgmPhysPageLoadIntoTlbWithPage(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys)
{
    PGM_LOCK_ASSERT_OWNER(pVM);
    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PageMapTlbMisses));

    /*
     * Map the page.
     * Make a special case for the zero page as it is kind of special.
     */
    PPGMPAGEMAPTLBE pTlbe = &pVM->pgm.s.CTXSUFF(PhysTlb).aEntries[PGM_PAGEMAPTLB_IDX(GCPhys)];
    if (    !PGM_PAGE_IS_ZERO(pPage)
        &&  !PGM_PAGE_IS_BALLOONED(pPage))
    {
        void *pv;
        PPGMPAGEMAP pMap;
        int rc = pgmPhysPageMapCommon(pVM, pPage, GCPhys, &pMap, &pv);
        if (RT_FAILURE(rc))
            return rc;
        pTlbe->pMap = pMap;
        pTlbe->pv = pv;
        Assert(!((uintptr_t)pTlbe->pv & PAGE_OFFSET_MASK));
    }
    else
    {
        AssertMsg(PGM_PAGE_GET_HCPHYS(pPage) == pVM->pgm.s.HCPhysZeroPg, ("%RGp/%R[pgmpage]\n", GCPhys, pPage));
        pTlbe->pMap = NULL;
        pTlbe->pv = pVM->pgm.s.CTXALLSUFF(pvZeroPg);
    }
#ifdef PGM_WITH_PHYS_TLB
    if (    PGM_PAGE_GET_TYPE(pPage) < PGMPAGETYPE_ROM_SHADOW
        ||  PGM_PAGE_GET_TYPE(pPage) > PGMPAGETYPE_ROM)
        pTlbe->GCPhys = GCPhys & X86_PTE_PAE_PG_MASK;
    else
        pTlbe->GCPhys = NIL_RTGCPHYS; /* ROM: Problematic because of the two pages. :-/ */
#else
    pTlbe->GCPhys = NIL_RTGCPHYS;
#endif
    pTlbe->pPage = pPage;
    return VINF_SUCCESS;
}

#endif /* !IN_RC && !VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */

/**
 * Internal version of PGMPhysGCPhys2CCPtr that expects the caller to
 * own the PGM lock and therefore not need to lock the mapped page.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   pPage       Pointer to the PGMPAGE structure for the page.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 *
 * @internal
 * @deprecated Use pgmPhysGCPhys2CCPtrInternalEx.
 */
int pgmPhysGCPhys2CCPtrInternalDepr(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void **ppv)
{
    int rc;
    AssertReturn(pPage, VERR_PGM_PHYS_NULL_PAGE_PARAM);
    PGM_LOCK_ASSERT_OWNER(pVM);
    pVM->pgm.s.cDeprecatedPageLocks++;

    /*
     * Make sure the page is writable.
     */
    if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
    {
        rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
        if (RT_FAILURE(rc))
            return rc;
        AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
    }
    Assert(PGM_PAGE_GET_HCPHYS(pPage) != 0);

    /*
     * Get the mapping address.
     */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    void *pv;
    rc = pgmRZDynMapHCPageInlined(VMMGetCpu(pVM),
                                  PGM_PAGE_GET_HCPHYS(pPage),
                                  &pv
                                  RTLOG_COMMA_SRC_POS);
    if (RT_FAILURE(rc))
        return rc;
    *ppv = (void *)((uintptr_t)pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
#else
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbeWithPage(pVM, pPage, GCPhys, &pTlbe);
    if (RT_FAILURE(rc))
        return rc;
    *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
#endif
    return VINF_SUCCESS;
}

#if !defined(IN_RC) && !defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

/**
 * Locks a page mapping for writing.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   pPage               The page.
 * @param   pTlbe               The mapping TLB entry for the page.
 * @param   pLock               The lock structure (output).
 */
DECLINLINE(void) pgmPhysPageMapLockForWriting(PVM pVM, PPGMPAGE pPage, PPGMPAGEMAPTLBE pTlbe, PPGMPAGEMAPLOCK pLock)
{
    PPGMPAGEMAP pMap = pTlbe->pMap;
    if (pMap)
        pMap->cRefs++;

    unsigned cLocks = PGM_PAGE_GET_WRITE_LOCKS(pPage);
    if (RT_LIKELY(cLocks < PGM_PAGE_MAX_LOCKS - 1))
    {
        if (cLocks == 0)
            pVM->pgm.s.cWriteLockedPages++;
        PGM_PAGE_INC_WRITE_LOCKS(pPage);
    }
    else if (cLocks != PGM_PAGE_MAX_LOCKS)
    {
        PGM_PAGE_INC_WRITE_LOCKS(pPage);
        AssertMsgFailed(("%R[pgmpage] is entering permanent write locked state!\n", pPage));
        if (pMap)
            pMap->cRefs++; /* Extra ref to prevent it from going away. */
    }

    pLock->uPageAndType = (uintptr_t)pPage | PGMPAGEMAPLOCK_TYPE_WRITE;
    pLock->pvMap = pMap;
}

/**
 * Locks a page mapping for reading.
 *
 * @param   pVM                 Pointer to the VM.
 * @param   pPage               The page.
 * @param   pTlbe               The mapping TLB entry for the page.
 * @param   pLock               The lock structure (output).
 */
DECLINLINE(void) pgmPhysPageMapLockForReading(PVM pVM, PPGMPAGE pPage, PPGMPAGEMAPTLBE pTlbe, PPGMPAGEMAPLOCK pLock)
{
    PPGMPAGEMAP pMap = pTlbe->pMap;
    if (pMap)
        pMap->cRefs++;

    unsigned cLocks = PGM_PAGE_GET_READ_LOCKS(pPage);
    if (RT_LIKELY(cLocks < PGM_PAGE_MAX_LOCKS - 1))
    {
        if (cLocks == 0)
            pVM->pgm.s.cReadLockedPages++;
        PGM_PAGE_INC_READ_LOCKS(pPage);
    }
    else if (cLocks != PGM_PAGE_MAX_LOCKS)
    {
        PGM_PAGE_INC_READ_LOCKS(pPage);
        AssertMsgFailed(("%R[pgmpage] is entering permanent read locked state!\n", pPage));
        if (pMap)
            pMap->cRefs++; /* Extra ref to prevent it from going away. */
    }

    pLock->uPageAndType = (uintptr_t)pPage | PGMPAGEMAPLOCK_TYPE_READ;
    pLock->pvMap = pMap;
}

#endif /* !IN_RC && !VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */


/**
 * Internal version of PGMPhysGCPhys2CCPtr that expects the caller to
 * own the PGM lock and have access to the page structure.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   pPage       Pointer to the PGMPAGE structure for the page.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that
 *                      pgmPhysReleaseInternalPageMappingLock needs.
 *
 * @internal
 */
int pgmPhysGCPhys2CCPtrInternal(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    int rc;
    AssertReturn(pPage, VERR_PGM_PHYS_NULL_PAGE_PARAM);
    PGM_LOCK_ASSERT_OWNER(pVM);

    /*
     * Make sure the page is writable.
     */
    if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
    {
        rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
        if (RT_FAILURE(rc))
            return rc;
        AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
    }
    Assert(PGM_PAGE_GET_HCPHYS(pPage) != 0);

    /*
     * Do the job.
     */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    void *pv;
    PVMCPU pVCpu = VMMGetCpu(pVM);
    rc = pgmRZDynMapHCPageInlined(pVCpu,
                                  PGM_PAGE_GET_HCPHYS(pPage),
                                  &pv
                                  RTLOG_COMMA_SRC_POS);
    if (RT_FAILURE(rc))
        return rc;
    *ppv = (void *)((uintptr_t)pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
    pLock->pvPage = pv;
    pLock->pVCpu  = pVCpu;

#else
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbeWithPage(pVM, pPage, GCPhys, &pTlbe);
    if (RT_FAILURE(rc))
        return rc;
    pgmPhysPageMapLockForWriting(pVM, pPage, pTlbe, pLock);
    *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
#endif
    return VINF_SUCCESS;
}


/**
 * Internal version of PGMPhysGCPhys2CCPtrReadOnly that expects the caller to
 * own the PGM lock and have access to the page structure.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   pPage       Pointer to the PGMPAGE structure for the page.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that
 *                      pgmPhysReleaseInternalPageMappingLock needs.
 *
 * @internal
 */
int pgmPhysGCPhys2CCPtrInternalReadOnly(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, const void **ppv, PPGMPAGEMAPLOCK pLock)
{
    AssertReturn(pPage, VERR_PGM_PHYS_NULL_PAGE_PARAM);
    PGM_LOCK_ASSERT_OWNER(pVM);
    Assert(PGM_PAGE_GET_HCPHYS(pPage) != 0);

    /*
     * Do the job.
     */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    void *pv;
    PVMCPU pVCpu = VMMGetCpu(pVM);
    int rc = pgmRZDynMapHCPageInlined(pVCpu,
                                      PGM_PAGE_GET_HCPHYS(pPage),
                                      &pv
                                      RTLOG_COMMA_SRC_POS); /** @todo add a read only flag? */
    if (RT_FAILURE(rc))
        return rc;
    *ppv = (void *)((uintptr_t)pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
    pLock->pvPage = pv;
    pLock->pVCpu  = pVCpu;

#else
    PPGMPAGEMAPTLBE pTlbe;
    int rc = pgmPhysPageQueryTlbeWithPage(pVM, pPage, GCPhys, &pTlbe);
    if (RT_FAILURE(rc))
        return rc;
    pgmPhysPageMapLockForReading(pVM, pPage, pTlbe, pLock);
    *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
#endif
    return VINF_SUCCESS;
}


/**
 * Requests the mapping of a guest page into the current context.
 *
 * This API should only be used for very short term, as it will consume scarse
 * resources (R0 and GC) in the mapping cache. When you're done with the page,
 * call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify
 * the page, use the PGMPhysGCPhys2CCPtrReadOnly() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The guest physical address of the page that should be
 *                      mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that
 *                      PGMPhysReleasePageMappingLock needs.
 *
 * @remarks The caller is responsible for dealing with access handlers.
 * @todo    Add an informational return code for pages with access handlers?
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk. External threads may
 *          need to delegate jobs to the EMTs.
 * @remarks Only one page is mapped!  Make no assumption about what's after or
 *          before the returned page!
 * @thread  Any thread.
 */
VMMDECL(int) PGMPhysGCPhys2CCPtr(PVM pVM, RTGCPHYS GCPhys, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /*
     * Find the page and make sure it's writable.
     */
    PPGMPAGE pPage;
    rc = pgmPhysGetPageEx(pVM, GCPhys, &pPage);
    if (RT_SUCCESS(rc))
    {
        if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
            rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
        if (RT_SUCCESS(rc))
        {
            AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));

            PVMCPU pVCpu = VMMGetCpu(pVM);
            void  *pv;
            rc = pgmRZDynMapHCPageInlined(pVCpu,
                                          PGM_PAGE_GET_HCPHYS(pPage),
                                          &pv
                                          RTLOG_COMMA_SRC_POS);
            if (RT_SUCCESS(rc))
            {
                AssertRCSuccess(rc);

                pv = (void *)((uintptr_t)pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
                *ppv = pv;
                pLock->pvPage = pv;
                pLock->pVCpu  = pVCpu;
            }
        }
    }

#else  /* IN_RING3 || IN_RING0 */
    /*
     * Query the Physical TLB entry for the page (may fail).
     */
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbe(pVM, GCPhys, &pTlbe);
    if (RT_SUCCESS(rc))
    {
        /*
         * If the page is shared, the zero page, or being write monitored
         * it must be converted to a page that's writable if possible.
         */
        PPGMPAGE pPage = pTlbe->pPage;
        if (RT_UNLIKELY(PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED))
        {
            rc = pgmPhysPageMakeWritable(pVM, pPage, GCPhys);
            if (RT_SUCCESS(rc))
            {
                AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 /* not returned */, ("%Rrc\n", rc));
                rc = pgmPhysPageQueryTlbeWithPage(pVM, pPage, GCPhys, &pTlbe);
            }
        }
        if (RT_SUCCESS(rc))
        {
            /*
             * Now, just perform the locking and calculate the return address.
             */
            pgmPhysPageMapLockForWriting(pVM, pPage, pTlbe, pLock);
            *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
        }
    }

#endif /* IN_RING3 || IN_RING0 */
    pgmUnlock(pVM);
    return rc;
}


/**
 * Requests the mapping of a guest page into the current context.
 *
 * This API should only be used for very short term, as it will consume scarse
 * resources (R0 and GC) in the mapping cache.  When you're done with the page,
 * call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The guest physical address of the page that should be
 *                      mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that
 *                      PGMPhysReleasePageMappingLock needs.
 *
 * @remarks The caller is responsible for dealing with access handlers.
 * @todo    Add an informational return code for pages with access handlers?
 *
 * @remarks Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @remarks Only one page is mapped!  Make no assumption about what's after or
 *          before the returned page!
 * @thread  Any thread.
 */
VMMDECL(int) PGMPhysGCPhys2CCPtrReadOnly(PVM pVM, RTGCPHYS GCPhys, void const **ppv, PPGMPAGEMAPLOCK pLock)
{
    int rc = pgmLock(pVM);
    AssertRCReturn(rc, rc);

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    /*
     * Find the page and make sure it's readable.
     */
    PPGMPAGE pPage;
    rc = pgmPhysGetPageEx(pVM, GCPhys, &pPage);
    if (RT_SUCCESS(rc))
    {
        if (RT_UNLIKELY(PGM_PAGE_IS_MMIO(pPage)))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
        else
        {
            PVMCPU pVCpu = VMMGetCpu(pVM);
            void  *pv;
            rc = pgmRZDynMapHCPageInlined(pVCpu,
                                          PGM_PAGE_GET_HCPHYS(pPage),
                                          &pv
                                          RTLOG_COMMA_SRC_POS); /** @todo add a read only flag? */
            if (RT_SUCCESS(rc))
            {
                AssertRCSuccess(rc);

                pv = (void *)((uintptr_t)pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
                *ppv = pv;
                pLock->pvPage = pv;
                pLock->pVCpu  = pVCpu;
            }
        }
    }

#else  /* IN_RING3 || IN_RING0 */
    /*
     * Query the Physical TLB entry for the page (may fail).
     */
    PPGMPAGEMAPTLBE pTlbe;
    rc = pgmPhysPageQueryTlbe(pVM, GCPhys, &pTlbe);
    if (RT_SUCCESS(rc))
    {
        /* MMIO pages doesn't have any readable backing. */
        PPGMPAGE pPage = pTlbe->pPage;
        if (RT_UNLIKELY(PGM_PAGE_IS_MMIO(pPage)))
            rc = VERR_PGM_PHYS_PAGE_RESERVED;
        else
        {
            /*
             * Now, just perform the locking and calculate the return address.
             */
            pgmPhysPageMapLockForReading(pVM, pPage, pTlbe, pLock);
            *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
        }
    }

#endif /* IN_RING3 || IN_RING0 */
    pgmUnlock(pVM);
    return rc;
}


/**
 * Requests the mapping of a guest page given by virtual address into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * This API will assume your intention is to write to the page, and will
 * therefore replace shared and zero pages. If you do not intend to modify
 * the page, use the PGMPhysGCPtr2CCPtrReadOnly() API.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT if the page directory for the virtual address isn't present.
 * @retval  VERR_PAGE_NOT_PRESENT if the page at the virtual address isn't present.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  EMT
 */
VMMDECL(int) PGMPhysGCPtr2CCPtr(PVMCPU pVCpu, RTGCPTR GCPtr, void **ppv, PPGMPAGEMAPLOCK pLock)
{
    VM_ASSERT_EMT(pVCpu->CTX_SUFF(pVM));
    RTGCPHYS GCPhys;
    int rc = PGMPhysGCPtr2GCPhys(pVCpu, GCPtr, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2CCPtr(pVCpu->CTX_SUFF(pVM), GCPhys, ppv, pLock);
    return rc;
}


/**
 * Requests the mapping of a guest page given by virtual address into the current context.
 *
 * This API should only be used for very short term, as it will consume
 * scarse resources (R0 and GC) in the mapping cache. When you're done
 * with the page, call PGMPhysReleasePageMappingLock() ASAP to release it.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_PAGE_TABLE_NOT_PRESENT if the page directory for the virtual address isn't present.
 * @retval  VERR_PAGE_NOT_PRESENT if the page at the virtual address isn't present.
 * @retval  VERR_PGM_PHYS_PAGE_RESERVED it it's a valid page but has no physical backing.
 * @retval  VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid physical address.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCPhys      The guest physical address of the page that should be mapped.
 * @param   ppv         Where to store the address corresponding to GCPhys.
 * @param   pLock       Where to store the lock information that PGMPhysReleasePageMappingLock needs.
 *
 * @remark  Avoid calling this API from within critical sections (other than
 *          the PGM one) because of the deadlock risk.
 * @thread  EMT
 */
VMMDECL(int) PGMPhysGCPtr2CCPtrReadOnly(PVMCPU pVCpu, RTGCPTR GCPtr, void const **ppv, PPGMPAGEMAPLOCK pLock)
{
    VM_ASSERT_EMT(pVCpu->CTX_SUFF(pVM));
    RTGCPHYS GCPhys;
    int rc = PGMPhysGCPtr2GCPhys(pVCpu, GCPtr, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2CCPtrReadOnly(pVCpu->CTX_SUFF(pVM), GCPhys, ppv, pLock);
    return rc;
}


/**
 * Release the mapping of a guest page.
 *
 * This is the counter part of PGMPhysGCPhys2CCPtr, PGMPhysGCPhys2CCPtrReadOnly
 * PGMPhysGCPtr2CCPtr and PGMPhysGCPtr2CCPtrReadOnly.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pLock       The lock structure initialized by the mapping function.
 */
VMMDECL(void) PGMPhysReleasePageMappingLock(PVM pVM, PPGMPAGEMAPLOCK pLock)
{
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    Assert(pLock->pvPage != NULL);
    Assert(pLock->pVCpu == VMMGetCpu(pVM));
    PGM_DYNMAP_UNUSED_HINT(pLock->pVCpu, pLock->pvPage);
    pLock->pVCpu  = NULL;
    pLock->pvPage = NULL;

#else
    PPGMPAGEMAP pMap       = (PPGMPAGEMAP)pLock->pvMap;
    PPGMPAGE    pPage      = (PPGMPAGE)(pLock->uPageAndType & ~PGMPAGEMAPLOCK_TYPE_MASK);
    bool        fWriteLock = (pLock->uPageAndType & PGMPAGEMAPLOCK_TYPE_MASK) == PGMPAGEMAPLOCK_TYPE_WRITE;

    pLock->uPageAndType = 0;
    pLock->pvMap = NULL;

    pgmLock(pVM);
    if (fWriteLock)
    {
        unsigned cLocks = PGM_PAGE_GET_WRITE_LOCKS(pPage);
        Assert(cLocks > 0);
        if (RT_LIKELY(cLocks > 0 && cLocks < PGM_PAGE_MAX_LOCKS))
        {
            if (cLocks == 1)
            {
                Assert(pVM->pgm.s.cWriteLockedPages > 0);
                pVM->pgm.s.cWriteLockedPages--;
            }
            PGM_PAGE_DEC_WRITE_LOCKS(pPage);
        }

        if (PGM_PAGE_GET_STATE(pPage) == PGM_PAGE_STATE_WRITE_MONITORED)
        {
            PGM_PAGE_SET_WRITTEN_TO(pVM, pPage);
            PGM_PAGE_SET_STATE(pVM, pPage, PGM_PAGE_STATE_ALLOCATED);
            Assert(pVM->pgm.s.cMonitoredPages > 0);
            pVM->pgm.s.cMonitoredPages--;
            pVM->pgm.s.cWrittenToPages++;
        }
    }
    else
    {
        unsigned cLocks = PGM_PAGE_GET_READ_LOCKS(pPage);
        Assert(cLocks > 0);
        if (RT_LIKELY(cLocks > 0 && cLocks < PGM_PAGE_MAX_LOCKS))
        {
            if (cLocks == 1)
            {
                Assert(pVM->pgm.s.cReadLockedPages > 0);
                pVM->pgm.s.cReadLockedPages--;
            }
            PGM_PAGE_DEC_READ_LOCKS(pPage);
        }
    }

    if (pMap)
    {
        Assert(pMap->cRefs >= 1);
        pMap->cRefs--;
    }
    pgmUnlock(pVM);
#endif /* IN_RING3 */
}


/**
 * Release the internal mapping of a guest page.
 *
 * This is the counter part of pgmPhysGCPhys2CCPtrInternalEx and
 * pgmPhysGCPhys2CCPtrInternalReadOnly.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pLock       The lock structure initialized by the mapping function.
 *
 * @remarks Caller must hold the PGM lock.
 */
void pgmPhysReleaseInternalPageMappingLock(PVM pVM, PPGMPAGEMAPLOCK pLock)
{
    PGM_LOCK_ASSERT_OWNER(pVM);
    PGMPhysReleasePageMappingLock(pVM, pLock); /* lazy for now */
}


/**
 * Converts a GC physical address to a HC ring-3 pointer.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @returns VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY if the range crosses
 *          a dynamic ram chunk boundary
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The GC physical address to convert.
 * @param   pR3Ptr      Where to store the R3 pointer on success.
 *
 * @deprecated  Avoid when possible!
 */
int pgmPhysGCPhys2R3Ptr(PVM pVM, RTGCPHYS GCPhys, PRTR3PTR pR3Ptr)
{
/** @todo this is kind of hacky and needs some more work. */
#ifndef DEBUG_sandervl
    VM_ASSERT_EMT(pVM); /* no longer safe for use outside the EMT thread! */
#endif

    Log(("pgmPhysGCPhys2R3Ptr(,%RGp,): dont use this API!\n", GCPhys)); /** @todo eliminate this API! */
#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    NOREF(pVM); NOREF(pR3Ptr);
    AssertFailedReturn(VERR_NOT_IMPLEMENTED);
#else
    pgmLock(pVM);

    PPGMRAMRANGE pRam;
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageAndRangeEx(pVM, GCPhys, &pPage, &pRam);
    if (RT_SUCCESS(rc))
        rc = pgmPhysGCPhys2CCPtrInternalDepr(pVM, pPage, GCPhys, (void **)pR3Ptr);

    pgmUnlock(pVM);
    Assert(rc <= VINF_SUCCESS);
    return rc;
#endif
}

#if 0 /*defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)*/

/**
 * Maps and locks a guest CR3 or PD (PAE) page.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_PGM_PHYS_PAGE_RESERVED it it's a valid GC physical
 *          page but has no physical backing.
 * @returns VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS if it's not a valid
 *          GC physical address.
 * @returns VERR_PGM_GCPHYS_RANGE_CROSSES_BOUNDARY if the range crosses
 *          a dynamic ram chunk boundary
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The GC physical address to convert.
 * @param   pR3Ptr      Where to store the R3 pointer on success.  This may or
 *                      may not be valid in ring-0 depending on the
 *                      VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 build option.
 *
 * @remarks The caller must own the PGM lock.
 */
int pgmPhysCr3ToHCPtr(PVM pVM, RTGCPHYS GCPhys, PRTR3PTR pR3Ptr)
{

    PPGMRAMRANGE pRam;
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageAndRangeEx(pVM, GCPhys, &pPage, &pRam);
    if (RT_SUCCESS(rc))
        rc = pgmPhysGCPhys2CCPtrInternalDepr(pVM, pPage, GCPhys, (void **)pR3Ptr);
    Assert(rc <= VINF_SUCCESS);
    return rc;
}


int pgmPhysCr3ToHCPtr(PVM pVM, RTGCPHYS GCPhys, PRTR3PTR pR3Ptr)
{

}

#endif

/**
 * Converts a guest pointer to a GC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCPtr       The guest pointer to convert.
 * @param   pGCPhys     Where to store the GC physical address.
 */
VMMDECL(int) PGMPhysGCPtr2GCPhys(PVMCPU pVCpu, RTGCPTR GCPtr, PRTGCPHYS pGCPhys)
{
    int rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, (RTGCUINTPTR)GCPtr, NULL, pGCPhys);
    if (pGCPhys && RT_SUCCESS(rc))
        *pGCPhys |= (RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK;
    return rc;
}


/**
 * Converts a guest pointer to a HC physical address.
 *
 * This uses the current CR3/CR0/CR4 of the guest.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   GCPtr       The guest pointer to convert.
 * @param   pHCPhys     Where to store the HC physical address.
 */
VMMDECL(int) PGMPhysGCPtr2HCPhys(PVMCPU pVCpu, RTGCPTR GCPtr, PRTHCPHYS pHCPhys)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    RTGCPHYS GCPhys;
    int rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, (RTGCUINTPTR)GCPtr, NULL, &GCPhys);
    if (RT_SUCCESS(rc))
        rc = PGMPhysGCPhys2HCPhys(pVM, GCPhys | ((RTGCUINTPTR)GCPtr & PAGE_OFFSET_MASK), pHCPhys);
    return rc;
}



#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_PGM_PHYS_ACCESS


#if defined(IN_RING3) && defined(SOME_UNUSED_FUNCTION)
/**
 * Cache PGMPhys memory access
 *
 * @param   pVM             Pointer to the VM.
 * @param   pCache          Cache structure pointer
 * @param   GCPhys          GC physical address
 * @param   pbHC            HC pointer corresponding to physical page
 *
 * @thread  EMT.
 */
static void pgmPhysCacheAdd(PVM pVM, PGMPHYSCACHE *pCache, RTGCPHYS GCPhys, uint8_t *pbR3)
{
    uint32_t iCacheIndex;

    Assert(VM_IS_EMT(pVM));

    GCPhys = PHYS_PAGE_ADDRESS(GCPhys);
    pbR3   = (uint8_t *)PAGE_ADDRESS(pbR3);

    iCacheIndex = ((GCPhys >> PAGE_SHIFT) & PGM_MAX_PHYSCACHE_ENTRIES_MASK);

    ASMBitSet(&pCache->aEntries, iCacheIndex);

    pCache->Entry[iCacheIndex].GCPhys = GCPhys;
    pCache->Entry[iCacheIndex].pbR3   = pbR3;
}
#endif /* IN_RING3 */


/**
 * Deals with reading from a page with one or more ALL access handlers.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The page descriptor.
 * @param   GCPhys      The physical address to start reading at.
 * @param   pvBuf       Where to put the bits we read.
 * @param   cb          How much to read - less or equal to a page.
 */
static int pgmPhysReadHandler(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void *pvBuf, size_t cb)
{
    /*
     * The most frequent access here is MMIO and shadowed ROM.
     * The current code ASSUMES all these access handlers covers full pages!
     */

    /*
     * Whatever we do we need the source page, map it first.
     */
    PGMPAGEMAPLOCK PgMpLck;
    const void    *pvSrc = NULL;
    int rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pPage, GCPhys, &pvSrc, &PgMpLck);
    if (RT_FAILURE(rc))
    {
        AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternalReadOnly failed on %RGp / %R[pgmpage] -> %Rrc\n",
                               GCPhys, pPage, rc));
        memset(pvBuf, 0xff, cb);
        return VINF_SUCCESS;
    }
    rc = VINF_PGM_HANDLER_DO_DEFAULT;

    /*
     * Deal with any physical handlers.
     */
#ifdef IN_RING3
    PPGMPHYSHANDLER pPhys = NULL;
#endif
    if (PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) == PGM_PAGE_HNDL_PHYS_STATE_ALL)
    {
#ifdef IN_RING3
        pPhys = pgmHandlerPhysicalLookup(pVM, GCPhys);
        AssertReleaseMsg(pPhys, ("GCPhys=%RGp cb=%#x\n", GCPhys, cb));
        Assert(GCPhys >= pPhys->Core.Key && GCPhys <= pPhys->Core.KeyLast);
        Assert((pPhys->Core.Key     & PAGE_OFFSET_MASK) == 0);
        Assert((pPhys->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK);
        Assert(pPhys->CTX_SUFF(pfnHandler));

        PFNPGMR3PHYSHANDLER pfnHandler = pPhys->CTX_SUFF(pfnHandler);
        void *pvUser = pPhys->CTX_SUFF(pvUser);

        Log5(("pgmPhysReadHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cb, pPage, R3STRING(pPhys->pszDesc) ));
        STAM_PROFILE_START(&pPhys->Stat, h);
        PGM_LOCK_ASSERT_OWNER(pVM);
        /* Release the PGM lock as MMIO handlers take the IOM lock. (deadlock prevention) */
        pgmUnlock(pVM);
        rc = pfnHandler(pVM, GCPhys, (void *)pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, pvUser);
        pgmLock(pVM);
# ifdef VBOX_WITH_STATISTICS
        pPhys = pgmHandlerPhysicalLookup(pVM, GCPhys);
        if (pPhys)
            STAM_PROFILE_STOP(&pPhys->Stat, h);
# else
        pPhys = NULL; /* might not be valid anymore. */
# endif
        AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp\n", rc, GCPhys));
#else
        /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
        //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cb=%#x\n", GCPhys, cb));
        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
        return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
    }

    /*
     * Deal with any virtual handlers.
     */
    if (PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) == PGM_PAGE_HNDL_VIRT_STATE_ALL)
    {
        unsigned        iPage;
        PPGMVIRTHANDLER pVirt;

        int rc2 = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pVirt, &iPage);
        AssertReleaseMsg(RT_SUCCESS(rc2), ("GCPhys=%RGp cb=%#x rc2=%Rrc\n", GCPhys, cb, rc2));
        Assert((pVirt->Core.Key     & PAGE_OFFSET_MASK) == 0);
        Assert((pVirt->Core.KeyLast & PAGE_OFFSET_MASK) == PAGE_OFFSET_MASK);
        Assert(GCPhys >= pVirt->aPhysToVirt[iPage].Core.Key && GCPhys <= pVirt->aPhysToVirt[iPage].Core.KeyLast);

#ifdef IN_RING3
        if (pVirt->pfnHandlerR3)
        {
            if (!pPhys)
                Log5(("pgmPhysReadHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] virt %s\n", GCPhys, cb, pPage, R3STRING(pVirt->pszDesc) ));
            else
                Log(("pgmPhysReadHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] phys/virt %s/%s\n", GCPhys, cb, pPage, R3STRING(pVirt->pszDesc), R3STRING(pPhys->pszDesc) ));
            RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirt->Core.Key & PAGE_BASE_GC_MASK)
                              + (iPage << PAGE_SHIFT)
                              + (GCPhys & PAGE_OFFSET_MASK);

            STAM_PROFILE_START(&pVirt->Stat, h);
            rc2 = pVirt->CTX_SUFF(pfnHandler)(pVM, GCPtr, (void *)pvSrc, pvBuf, cb, PGMACCESSTYPE_READ, /*pVirt->CTX_SUFF(pvUser)*/ NULL);
            STAM_PROFILE_STOP(&pVirt->Stat, h);
            if (rc2 == VINF_SUCCESS)
                rc = VINF_SUCCESS;
            AssertLogRelMsg(rc2 == VINF_SUCCESS || rc2 == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc2, GCPhys, pPage, pVirt->pszDesc));
        }
        else
            Log5(("pgmPhysReadHandler: GCPhys=%RGp cb=%#x pPage=%R[pgmpage] virt %s [no handler]\n", GCPhys, cb, pPage, R3STRING(pVirt->pszDesc) ));
#else
        /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
        //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cb=%#x\n", GCPhys, cb));
        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
        return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
    }

    /*
     * Take the default action.
     */
    if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
        memcpy(pvBuf, pvSrc, cb);
    pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
    return rc;
}


/**
 * Read physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleReadGCPhys() if you
 * want to ignore those.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM             Pointer to the VM.
 * @param   GCPhys          Physical address start reading from.
 * @param   pvBuf           Where to put the read bits.
 * @param   cbRead          How many bytes to read.
 */
VMMDECL(int) PGMPhysRead(PVM pVM, RTGCPHYS GCPhys, void *pvBuf, size_t cbRead)
{
    AssertMsgReturn(cbRead > 0, ("don't even think about reading zero bytes!\n"), VINF_SUCCESS);
    LogFlow(("PGMPhysRead: %RGp %d\n", GCPhys, cbRead));

    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysRead));
    STAM_COUNTER_ADD(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysReadBytes), cbRead);

    pgmLock(pVM);

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, GCPhys);
    for (;;)
    {
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPHYS off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                unsigned iPage = off >> PAGE_SHIFT;
                PPGMPAGE pPage = &pRam->aPages[iPage];
                size_t   cb    = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                if (cb > cbRead)
                    cb = cbRead;

                /*
                 * Any ALL access handlers?
                 */
                if (RT_UNLIKELY(PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)))
                {
                    int rc = pgmPhysReadHandler(pVM, pPage, pRam->GCPhys + off, pvBuf, cb);
                    if (RT_FAILURE(rc))
                    {
                        pgmUnlock(pVM);
                        return rc;
                    }
                }
                else
                {
                    /*
                     * Get the pointer to the page.
                     */
                    PGMPAGEMAPLOCK PgMpLck;
                    const void    *pvSrc;
                    int rc = pgmPhysGCPhys2CCPtrInternalReadOnly(pVM, pPage, pRam->GCPhys + off, &pvSrc, &PgMpLck);
                    if (RT_SUCCESS(rc))
                    {
                        memcpy(pvBuf, pvSrc, cb);
                        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                    }
                    else
                    {
                        AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternalReadOnly failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                               pRam->GCPhys + off, pPage, rc));
                        memset(pvBuf, 0xff, cb);
                    }
                }

                /* next page */
                if (cb >= cbRead)
                {
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }
                cbRead -= cb;
                off    += cb;
                pvBuf   = (char *)pvBuf + cb;
            } /* walk pages in ram range. */

            GCPhys = pRam->GCPhysLast + 1;
        }
        else
        {
            LogFlow(("PGMPhysRead: Unassigned %RGp size=%u\n", GCPhys, cbRead));

            /*
             * Unassigned address space.
             */
            size_t cb = pRam ? pRam->GCPhys - GCPhys : ~(size_t)0;
            if (cb >= cbRead)
            {
                memset(pvBuf, 0xff, cbRead);
                break;
            }
            memset(pvBuf, 0xff, cb);

            cbRead -= cb;
            pvBuf   = (char *)pvBuf + cb;
            GCPhys += cb;
        }

        /* Advance range if necessary. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
    } /* Ram range walk */

    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Deals with writing to a page with one or more WRITE or ALL access handlers.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pPage       The page descriptor.
 * @param   GCPhys      The physical address to start writing at.
 * @param   pvBuf       What to write.
 * @param   cbWrite     How much to write - less or equal to a page.
 */
static int pgmPhysWriteHandler(PVM pVM, PPGMPAGE pPage, RTGCPHYS GCPhys, void const *pvBuf, size_t cbWrite)
{
    PGMPAGEMAPLOCK  PgMpLck;
    void           *pvDst = NULL;
    int             rc;

    /*
     * Give priority to physical handlers (like #PF does).
     *
     * Hope for a lonely physical handler first that covers the whole
     * write area. This should be a pretty frequent case with MMIO and
     * the heavy usage of full page handlers in the page pool.
     */
    if (    !PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage)
        ||  PGM_PAGE_IS_MMIO(pPage) /* screw virtual handlers on MMIO pages */)
    {
        PPGMPHYSHANDLER pCur = pgmHandlerPhysicalLookup(pVM, GCPhys);
        if (pCur)
        {
            Assert(GCPhys >= pCur->Core.Key && GCPhys <= pCur->Core.KeyLast);
            Assert(pCur->CTX_SUFF(pfnHandler));

            size_t cbRange = pCur->Core.KeyLast - GCPhys + 1;
            if (cbRange > cbWrite)
                cbRange = cbWrite;

#ifndef IN_RING3
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;

#else  /* IN_RING3 */
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cbRange, pPage, R3STRING(pCur->pszDesc) ));
            if (!PGM_PAGE_IS_MMIO(pPage))
                rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDst, &PgMpLck);
            else
                rc = VINF_SUCCESS;
            if (RT_SUCCESS(rc))
            {
                PFNPGMR3PHYSHANDLER pfnHandler = pCur->CTX_SUFF(pfnHandler);
                void *pvUser = pCur->CTX_SUFF(pvUser);

                STAM_PROFILE_START(&pCur->Stat, h);
                PGM_LOCK_ASSERT_OWNER(pVM);
                /* Release the PGM lock as MMIO handlers take the IOM lock. (deadlock prevention) */
                pgmUnlock(pVM);
                rc = pfnHandler(pVM, GCPhys, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, pvUser);
                pgmLock(pVM);
# ifdef VBOX_WITH_STATISTICS
                pCur = pgmHandlerPhysicalLookup(pVM, GCPhys);
                if (pCur)
                    STAM_PROFILE_STOP(&pCur->Stat, h);
# else
                pCur = NULL; /* might not be valid anymore. */
# endif
                if (rc == VINF_PGM_HANDLER_DO_DEFAULT && pvDst)
                {
                    if (pvDst)
                        memcpy(pvDst, pvBuf, cbRange);
                }
                else
                    AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, (pCur) ? pCur->pszDesc : ""));
            }
            else
                AssertLogRelMsgFailedReturn(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                             GCPhys, pPage, rc), rc);
            if (RT_LIKELY(cbRange == cbWrite))
            {
                if (pvDst)
                    pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                return VINF_SUCCESS;
            }

            /* more fun to be had below */
            cbWrite -= cbRange;
            GCPhys  += cbRange;
            pvBuf    = (uint8_t *)pvBuf + cbRange;
            pvDst    = (uint8_t *)pvDst + cbRange;
#endif /* IN_RING3 */
        }
        /* else: the handler is somewhere else in the page, deal with it below. */
        Assert(!PGM_PAGE_IS_MMIO(pPage)); /* MMIO handlers are all PAGE_SIZEed! */
    }
    /*
     * A virtual handler without any interfering physical handlers.
     * Hopefully it'll cover the whole write.
     */
    else if (!PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage))
    {
        unsigned        iPage;
        PPGMVIRTHANDLER pCur;
        rc = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pCur, &iPage);
        if (RT_SUCCESS(rc))
        {
            size_t cbRange = (PAGE_OFFSET_MASK & pCur->Core.KeyLast) - (PAGE_OFFSET_MASK & GCPhys) + 1;
            if (cbRange > cbWrite)
                cbRange = cbWrite;

#ifndef IN_RING3
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            return VERR_PGM_PHYS_WR_HIT_HANDLER;

#else  /* IN_RING3 */

            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] virt %s\n", GCPhys, cbRange, pPage, R3STRING(pCur->pszDesc) ));
            rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDst, &PgMpLck);
            if (RT_SUCCESS(rc))
            {
                rc = VINF_PGM_HANDLER_DO_DEFAULT;
                if (pCur->pfnHandlerR3)
                {
                    RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pCur->Core.Key & PAGE_BASE_GC_MASK)
                                      + (iPage << PAGE_SHIFT)
                                      + (GCPhys & PAGE_OFFSET_MASK);

                    STAM_PROFILE_START(&pCur->Stat, h);
                    rc = pCur->CTX_SUFF(pfnHandler)(pVM, GCPtr, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, /*pCur->CTX_SUFF(pvUser)*/ NULL);
                    STAM_PROFILE_STOP(&pCur->Stat, h);
                }
                if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    memcpy(pvDst, pvBuf, cbRange);
                else
                    AssertLogRelMsg(rc == VINF_SUCCESS, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pCur->pszDesc));
            }
            else
                AssertLogRelMsgFailedReturn(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                             GCPhys, pPage, rc), rc);
            if (RT_LIKELY(cbRange == cbWrite))
            {
                pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                return VINF_SUCCESS;
            }

            /* more fun to be had below */
            cbWrite -= cbRange;
            GCPhys  += cbRange;
            pvBuf    = (uint8_t *)pvBuf + cbRange;
            pvDst    = (uint8_t *)pvDst + cbRange;
#endif
        }
        /* else: the handler is somewhere else in the page, deal with it below. */
    }

    /*
     * Deal with all the odd ends.
     */

    /* We need a writable destination page. */
    if (!pvDst)
    {
        rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, &pvDst, &PgMpLck);
        AssertLogRelMsgReturn(RT_SUCCESS(rc),
                              ("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                               GCPhys, pPage, rc), rc);
    }

    /* The loop state (big + ugly). */
    unsigned        iVirtPage   = 0;
    PPGMVIRTHANDLER pVirt       = NULL;
    uint32_t        offVirt     = PAGE_SIZE;
    uint32_t        offVirtLast = PAGE_SIZE;
    bool            fMoreVirt   = PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage);

    PPGMPHYSHANDLER pPhys       = NULL;
    uint32_t        offPhys     = PAGE_SIZE;
    uint32_t        offPhysLast = PAGE_SIZE;
    bool            fMorePhys   = PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage);

    /* The loop. */
    for (;;)
    {
        /*
         * Find the closest handler at or above GCPhys.
         */
        if (fMoreVirt && !pVirt)
        {
            rc = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys, &pVirt, &iVirtPage);
            if (RT_SUCCESS(rc))
            {
                offVirt = 0;
                offVirtLast = (pVirt->aPhysToVirt[iVirtPage].Core.KeyLast & PAGE_OFFSET_MASK) - (GCPhys & PAGE_OFFSET_MASK);
            }
            else
            {
                PPGMPHYS2VIRTHANDLER pVirtPhys;
                pVirtPhys = (PPGMPHYS2VIRTHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysToVirtHandlers,
                                                                          GCPhys, true /* fAbove */);
                if (    pVirtPhys
                    &&  (pVirtPhys->Core.Key >> PAGE_SHIFT) == (GCPhys >> PAGE_SHIFT))
                {
                    /* ASSUME that pVirtPhys only covers one page. */
                    Assert((pVirtPhys->Core.Key >> PAGE_SHIFT) == (pVirtPhys->Core.KeyLast >> PAGE_SHIFT));
                    Assert(pVirtPhys->Core.Key > GCPhys);

                    pVirt       = (PPGMVIRTHANDLER)((uintptr_t)pVirtPhys + pVirtPhys->offVirtHandler);
                    iVirtPage   = pVirtPhys - &pVirt->aPhysToVirt[0]; Assert(iVirtPage == 0);
                    offVirt     = (pVirtPhys->Core.Key     & PAGE_OFFSET_MASK) - (GCPhys & PAGE_OFFSET_MASK);
                    offVirtLast = (pVirtPhys->Core.KeyLast & PAGE_OFFSET_MASK) - (GCPhys & PAGE_OFFSET_MASK);
                }
                else
                {
                    pVirt       = NULL;
                    fMoreVirt   = false;
                    offVirt     = offVirtLast = PAGE_SIZE;
                }
            }
        }

        if (fMorePhys && !pPhys)
        {
            pPhys = pgmHandlerPhysicalLookup(pVM, GCPhys);
            if (pPhys)
            {
                offPhys = 0;
                offPhysLast = pPhys->Core.KeyLast - GCPhys; /* ASSUMES < 4GB handlers... */
            }
            else
            {
                pPhys = (PPGMPHYSHANDLER)RTAvlroGCPhysGetBestFit(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers,
                                                                 GCPhys, true /* fAbove */);
                if (    pPhys
                    &&  pPhys->Core.Key <= GCPhys + (cbWrite - 1))
                {
                    offPhys     = pPhys->Core.Key     - GCPhys;
                    offPhysLast = pPhys->Core.KeyLast - GCPhys; /* ASSUMES < 4GB handlers... */
                }
                else
                {
                    pPhys     = NULL;
                    fMorePhys = false;
                    offPhys   = offPhysLast = PAGE_SIZE;
                }
            }
        }

        /*
         * Handle access to space without handlers (that's easy).
         */
        rc = VINF_PGM_HANDLER_DO_DEFAULT;
        uint32_t cbRange = (uint32_t)cbWrite;
        if (offPhys && offVirt)
        {
            if (cbRange > offPhys)
                cbRange = offPhys;
            if (cbRange > offVirt)
                cbRange = offVirt;
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] miss\n", GCPhys, cbRange, pPage));
        }
        /*
         * Physical handler.
         */
        else if (!offPhys && offVirt)
        {
            if (cbRange > offPhysLast + 1)
                cbRange = offPhysLast + 1;
            if (cbRange > offVirt)
                cbRange = offVirt;
#ifdef IN_RING3
            PFNPGMR3PHYSHANDLER pfnHandler = pPhys->CTX_SUFF(pfnHandler);
            void *pvUser = pPhys->CTX_SUFF(pvUser);

            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cbRange, pPage, R3STRING(pPhys->pszDesc) ));
            STAM_PROFILE_START(&pPhys->Stat, h);
            PGM_LOCK_ASSERT_OWNER(pVM);
            /* Release the PGM lock as MMIO handlers take the IOM lock. (deadlock prevention) */
            pgmUnlock(pVM);
            rc = pfnHandler(pVM, GCPhys, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, pvUser);
            pgmLock(pVM);
# ifdef VBOX_WITH_STATISTICS
            pPhys = pgmHandlerPhysicalLookup(pVM, GCPhys);
            if (pPhys)
                STAM_PROFILE_STOP(&pPhys->Stat, h);
# else
            pPhys = NULL; /* might not be valid anymore. */
# endif
            AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, (pPhys) ? pPhys->pszDesc : ""));
#else
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
            return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
        }
        /*
         * Virtual handler.
         */
        else if (offPhys && !offVirt)
        {
            if (cbRange > offVirtLast + 1)
                cbRange = offVirtLast + 1;
            if (cbRange > offPhys)
                cbRange = offPhys;
#ifdef IN_RING3
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys %s\n", GCPhys, cbRange, pPage, R3STRING(pVirt->pszDesc) ));
            if (pVirt->pfnHandlerR3)
            {
                RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirt->Core.Key & PAGE_BASE_GC_MASK)
                                  + (iVirtPage << PAGE_SHIFT)
                                  + (GCPhys & PAGE_OFFSET_MASK);
                STAM_PROFILE_START(&pVirt->Stat, h);
                rc = pVirt->CTX_SUFF(pfnHandler)(pVM, GCPtr, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, /*pCur->CTX_SUFF(pvUser)*/ NULL);
                STAM_PROFILE_STOP(&pVirt->Stat, h);
                AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pVirt->pszDesc));
            }
            pVirt = NULL;
#else
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
            return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
        }
        /*
         * Both... give the physical one priority.
         */
        else
        {
            Assert(!offPhys && !offVirt);
            if (cbRange > offVirtLast + 1)
                cbRange = offVirtLast + 1;
            if (cbRange > offPhysLast + 1)
                cbRange = offPhysLast + 1;

#ifdef IN_RING3
            if (pVirt->pfnHandlerR3)
                Log(("pgmPhysWriteHandler: overlapping phys and virt handlers at %RGp %R[pgmpage]; cbRange=%#x\n", GCPhys, pPage, cbRange));
            Log5(("pgmPhysWriteHandler: GCPhys=%RGp cbRange=%#x pPage=%R[pgmpage] phys/virt %s/%s\n", GCPhys, cbRange, pPage, R3STRING(pPhys->pszDesc), R3STRING(pVirt->pszDesc) ));

            PFNPGMR3PHYSHANDLER pfnHandler = pPhys->CTX_SUFF(pfnHandler);
            void *pvUser = pPhys->CTX_SUFF(pvUser);

            STAM_PROFILE_START(&pPhys->Stat, h);
            PGM_LOCK_ASSERT_OWNER(pVM);
            /* Release the PGM lock as MMIO handlers take the IOM lock. (deadlock prevention) */
            pgmUnlock(pVM);
            rc = pfnHandler(pVM, GCPhys, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, pvUser);
            pgmLock(pVM);
# ifdef VBOX_WITH_STATISTICS
            pPhys = pgmHandlerPhysicalLookup(pVM, GCPhys);
            if (pPhys)
                STAM_PROFILE_STOP(&pPhys->Stat, h);
# else
            pPhys = NULL; /* might not be valid anymore. */
# endif
            AssertLogRelMsg(rc == VINF_SUCCESS || rc == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, (pPhys) ? pPhys->pszDesc : ""));
            if (pVirt->pfnHandlerR3)
            {

                RTGCUINTPTR GCPtr = ((RTGCUINTPTR)pVirt->Core.Key & PAGE_BASE_GC_MASK)
                                  + (iVirtPage << PAGE_SHIFT)
                                  + (GCPhys & PAGE_OFFSET_MASK);
                STAM_PROFILE_START(&pVirt->Stat, h2);
                int rc2 = pVirt->CTX_SUFF(pfnHandler)(pVM, GCPtr, pvDst, (void *)pvBuf, cbRange, PGMACCESSTYPE_WRITE, /*pCur->CTX_SUFF(pvUser)*/ NULL);
                STAM_PROFILE_STOP(&pVirt->Stat, h2);
                if (rc2 == VINF_SUCCESS && rc == VINF_PGM_HANDLER_DO_DEFAULT)
                    rc = VINF_SUCCESS;
                else
                    AssertLogRelMsg(rc2 == VINF_SUCCESS || rc2 == VINF_PGM_HANDLER_DO_DEFAULT, ("rc=%Rrc GCPhys=%RGp pPage=%R[pgmpage] %s\n", rc, GCPhys, pPage, pVirt->pszDesc));
            }
            pPhys = NULL;
            pVirt = NULL;
#else
            /* In R0 and RC the callbacks cannot handle this context, so we'll fail. */
            NOREF(cbRange);
            //AssertReleaseMsgFailed(("Wrong API! GCPhys=%RGp cbRange=%#x\n", GCPhys, cbRange));
            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
            return VERR_PGM_PHYS_WR_HIT_HANDLER;
#endif
        }
        if (rc == VINF_PGM_HANDLER_DO_DEFAULT)
            memcpy(pvDst, pvBuf, cbRange);

        /*
         * Advance if we've got more stuff to do.
         */
        if (cbRange >= cbWrite)
        {
            pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
            return VINF_SUCCESS;
        }

        cbWrite         -= cbRange;
        GCPhys          += cbRange;
        pvBuf            = (uint8_t *)pvBuf + cbRange;
        pvDst            = (uint8_t *)pvDst + cbRange;

        offPhys         -= cbRange;
        offPhysLast     -= cbRange;
        offVirt         -= cbRange;
        offVirtLast     -= cbRange;
    }
}


/**
 * Write to physical memory.
 *
 * This API respects access handlers and MMIO. Use PGMPhysSimpleWriteGCPhys() if you
 * want to ignore those.
 *
 * @returns VBox status code. Can be ignored in ring-3.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVM             Pointer to the VM.
 * @param   GCPhys          Physical address to write to.
 * @param   pvBuf           What to write.
 * @param   cbWrite         How many bytes to write.
 */
VMMDECL(int) PGMPhysWrite(PVM pVM, RTGCPHYS GCPhys, const void *pvBuf, size_t cbWrite)
{
    AssertMsg(!pVM->pgm.s.fNoMorePhysWrites, ("Calling PGMPhysWrite after pgmR3Save()!\n"));
    AssertMsgReturn(cbWrite > 0, ("don't even think about writing zero bytes!\n"), VINF_SUCCESS);
    LogFlow(("PGMPhysWrite: %RGp %d\n", GCPhys, cbWrite));

    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysWrite));
    STAM_COUNTER_ADD(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysWriteBytes), cbWrite);

    pgmLock(pVM);

    /*
     * Copy loop on ram ranges.
     */
    PPGMRAMRANGE pRam = pgmPhysGetRangeAtOrAbove(pVM, GCPhys);
    for (;;)
    {
        /* Inside range or not? */
        if (pRam && GCPhys >= pRam->GCPhys)
        {
            /*
             * Must work our way thru this page by page.
             */
            RTGCPTR off = GCPhys - pRam->GCPhys;
            while (off < pRam->cb)
            {
                RTGCPTR     iPage = off >> PAGE_SHIFT;
                PPGMPAGE    pPage = &pRam->aPages[iPage];
                size_t      cb    = PAGE_SIZE - (off & PAGE_OFFSET_MASK);
                if (cb > cbWrite)
                    cb = cbWrite;

                /*
                 * Any active WRITE or ALL access handlers?
                 */
                if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                {
                    int rc = pgmPhysWriteHandler(pVM, pPage, pRam->GCPhys + off, pvBuf, cb);
                    if (RT_FAILURE(rc))
                    {
                        pgmUnlock(pVM);
                        return rc;
                    }
                }
                else
                {
                    /*
                     * Get the pointer to the page.
                     */
                    PGMPAGEMAPLOCK PgMpLck;
                    void          *pvDst;
                    int rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, pRam->GCPhys + off, &pvDst, &PgMpLck);
                    if (RT_SUCCESS(rc))
                    {
                        Assert(!PGM_PAGE_IS_BALLOONED(pPage));
                        memcpy(pvDst, pvBuf, cb);
                        pgmPhysReleaseInternalPageMappingLock(pVM, &PgMpLck);
                    }
                    /* Ignore writes to ballooned pages. */
                    else if (!PGM_PAGE_IS_BALLOONED(pPage))
                        AssertLogRelMsgFailed(("pgmPhysGCPhys2CCPtrInternal failed on %RGp / %R[pgmpage] -> %Rrc\n",
                                                pRam->GCPhys + off, pPage, rc));
                }

                /* next page */
                if (cb >= cbWrite)
                {
                    pgmUnlock(pVM);
                    return VINF_SUCCESS;
                }

                cbWrite -= cb;
                off     += cb;
                pvBuf    = (const char *)pvBuf + cb;
            } /* walk pages in ram range */

            GCPhys = pRam->GCPhysLast + 1;
        }
        else
        {
            /*
             * Unassigned address space, skip it.
             */
            if (!pRam)
                break;
            size_t cb = pRam->GCPhys - GCPhys;
            if (cb >= cbWrite)
                break;
            cbWrite -= cb;
            pvBuf   = (const char *)pvBuf + cb;
            GCPhys += cb;
        }

        /* Advance range if necessary. */
        while (pRam && GCPhys > pRam->GCPhysLast)
            pRam = pRam->CTX_SUFF(pNext);
    } /* Ram range walk */

    pgmUnlock(pVM);
    return VINF_SUCCESS;
}


/**
 * Read from guest physical memory by GC physical address, bypassing
 * MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         Pointer to the VM.
 * @param   pvDst       The destination address.
 * @param   GCPhysSrc   The source address (GC physical address).
 * @param   cb          The number of bytes to read.
 */
VMMDECL(int) PGMPhysSimpleReadGCPhys(PVM pVM, void *pvDst, RTGCPHYS GCPhysSrc, size_t cb)
{
    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void const *pvSrc;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhysSrc, &pvSrc, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - (GCPhysSrc & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPhysSrc += cbPage;
    pvDst = (uint8_t *)pvDst + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhysSrc, &pvSrc, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPhysSrc += PAGE_SIZE;
        pvDst = (uint8_t *)pvDst + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Write to guest physical memory referenced by GC pointer.
 * Write memory to GC physical address in guest physical memory.
 *
 * This will bypass MMIO and access handlers.
 *
 * @returns VBox status.
 * @param   pVM         Pointer to the VM.
 * @param   GCPhysDst   The GC physical address of the destination.
 * @param   pvSrc       The source buffer.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysSimpleWriteGCPhys(PVM pVM, RTGCPHYS GCPhysDst, const void *pvSrc, size_t cb)
{
    LogFlow(("PGMPhysSimpleWriteGCPhys: %RGp %zu\n", GCPhysDst, cb));

    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void *pvDst;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPhys2CCPtr(pVM, GCPhysDst, &pvDst, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - (GCPhysDst & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPhysDst += cbPage;
    pvSrc = (const uint8_t *)pvSrc + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPhys2CCPtr(pVM, GCPhysDst, &pvDst, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPhysDst += PAGE_SIZE;
        pvSrc = (const uint8_t *)pvSrc + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set any accessed bits.
 *
 * @returns VBox status.
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 */
VMMDECL(int) PGMPhysSimpleReadGCPtr(PVMCPU pVCpu, void *pvDst, RTGCPTR GCPtrSrc, size_t cb)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
/** @todo fix the macro / state handling: VMCPU_ASSERT_EMT_OR_GURU(pVCpu); */

    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysSimpleRead));
    STAM_COUNTER_ADD(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysSimpleReadBytes), cb);

    /* Take the PGM lock here, because many called functions take the lock for a very short period. That's counter-productive
     * when many VCPUs are fighting for the lock.
     */
    pgmLock(pVM);

    /* map the 1st page */
    void const *pvSrc;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPtr2CCPtrReadOnly(pVCpu, GCPtrSrc, &pvSrc, &Lock);
    if (RT_FAILURE(rc))
    {
        pgmUnlock(pVM);
        return rc;
    }

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - ((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        pgmUnlock(pVM);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPtrSrc = (RTGCPTR)((RTGCUINTPTR)GCPtrSrc + cbPage);
    pvDst = (uint8_t *)pvDst + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPtr2CCPtrReadOnly(pVCpu, GCPtrSrc, &pvSrc, &Lock);
        if (RT_FAILURE(rc))
        {
            pgmUnlock(pVM);
            return rc;
        }

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            pgmUnlock(pVM);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPtrSrc = (RTGCPTR)((RTGCUINTPTR)GCPtrSrc + PAGE_SIZE);
        pvDst = (uint8_t *)pvDst + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers and not set dirty or accessed bits.
 *
 * @returns VBox status.
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysSimpleWriteGCPtr(PVMCPU pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Treat the first page as a special case.
     */
    if (!cb)
        return VINF_SUCCESS;

    STAM_COUNTER_INC(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysSimpleWrite));
    STAM_COUNTER_ADD(&pVM->pgm.s.CTX_SUFF(pStats)->CTX_MID_Z(Stat,PhysSimpleWriteBytes), cb);

    /* map the 1st page */
    void *pvDst;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrDst, &pvDst, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + cbPage);
    pvSrc = (const uint8_t *)pvSrc + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrDst, &pvDst, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + PAGE_SIZE);
        pvSrc = (const uint8_t *)pvSrc + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Write to guest physical memory referenced by GC pointer and update the PTE.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * bypass access handlers but will set any dirty and accessed bits in the PTE.
 *
 * If you don't want to set the dirty bit, use PGMPhysSimpleWriteGCPtr().
 *
 * @returns VBox status.
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysSimpleDirtyWriteGCPtr(PVMCPU pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Treat the first page as a special case.
     * Btw. this is the same code as in PGMPhyssimpleWriteGCPtr excep for the PGMGstModifyPage.
     */
    if (!cb)
        return VINF_SUCCESS;

    /* map the 1st page */
    void *pvDst;
    PGMPAGEMAPLOCK Lock;
    int rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrDst, &pvDst, &Lock);
    if (RT_FAILURE(rc))
        return rc;

    /* optimize for the case where access is completely within the first page. */
    size_t cbPage = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
    if (RT_LIKELY(cb <= cbPage))
    {
        memcpy(pvDst, pvSrc, cb);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
        return VINF_SUCCESS;
    }

    /* copy to the end of the page. */
    memcpy(pvDst, pvSrc, cbPage);
    PGMPhysReleasePageMappingLock(pVM, &Lock);
    rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
    GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + cbPage);
    pvSrc = (const uint8_t *)pvSrc + cbPage;
    cb -= cbPage;

    /*
     * Page by page.
     */
    for (;;)
    {
        /* map the page */
        rc = PGMPhysGCPtr2CCPtr(pVCpu, GCPtrDst, &pvDst, &Lock);
        if (RT_FAILURE(rc))
            return rc;

        /* last page? */
        if (cb <= PAGE_SIZE)
        {
            memcpy(pvDst, pvSrc, cb);
            PGMPhysReleasePageMappingLock(pVM, &Lock);
            rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
            return VINF_SUCCESS;
        }

        /* copy the entire page and advance */
        memcpy(pvDst, pvSrc, PAGE_SIZE);
        PGMPhysReleasePageMappingLock(pVM, &Lock);
        rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D)); AssertRC(rc);
        GCPtrDst = (RTGCPTR)((RTGCUINTPTR)GCPtrDst + PAGE_SIZE);
        pvSrc = (const uint8_t *)pvSrc + PAGE_SIZE;
        cb -= PAGE_SIZE;
    }
    /* won't ever get here. */
}


/**
 * Read from guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * respect access handlers and set accessed bits.
 *
 * @returns VBox status.
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   pvDst       The destination address.
 * @param   GCPtrSrc    The source address (GC pointer).
 * @param   cb          The number of bytes to read.
 * @thread  The vCPU EMT.
 */
VMMDECL(int) PGMPhysReadGCPtr(PVMCPU pVCpu, void *pvDst, RTGCPTR GCPtrSrc, size_t cb)
{
    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int         rc;
    PVM         pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    LogFlow(("PGMPhysReadGCPtr: %RGv %zu\n", GCPtrSrc, cb));

    /*
     * Optimize reads within a single page.
     */
    if (((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, (RTGCUINTPTR)GCPtrSrc, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrSrc), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK;

        /* mark the guest page as accessed. */
        if (!(fFlags & X86_PTE_A))
        {
            rc = PGMGstModifyPage(pVCpu, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)(X86_PTE_A));
            AssertRC(rc);
        }

        return PGMPhysRead(pVM, GCPhys, pvDst, cb);
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, (RTGCUINTPTR)GCPtrSrc, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrSrc), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK;

        /* mark the guest page as accessed. */
        if (!(fFlags & X86_PTE_A))
        {
            rc = PGMGstModifyPage(pVCpu, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)(X86_PTE_A));
            AssertRC(rc);
        }

        /* copy */
        size_t cbRead = PAGE_SIZE - ((RTGCUINTPTR)GCPtrSrc & PAGE_OFFSET_MASK);
        rc = PGMPhysRead(pVM, GCPhys, pvDst, cbRead);
        if (cbRead >= cb || RT_FAILURE(rc))
            return rc;

        /* next */
        cb         -= cbRead;
        pvDst       = (uint8_t *)pvDst + cbRead;
        GCPtrSrc   += cbRead;
    }
}


/**
 * Write to guest physical memory referenced by GC pointer.
 *
 * This function uses the current CR3/CR0/CR4 of the guest and will
 * respect access handlers and set dirty and accessed bits.
 *
 * @returns VBox status.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_PGM_PHYS_WR_HIT_HANDLER in R0 and GC, NEVER in R3.
 *
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   GCPtrDst    The destination address (GC pointer).
 * @param   pvSrc       The source address.
 * @param   cb          The number of bytes to write.
 */
VMMDECL(int) PGMPhysWriteGCPtr(PVMCPU pVCpu, RTGCPTR GCPtrDst, const void *pvSrc, size_t cb)
{
    RTGCPHYS    GCPhys;
    uint64_t    fFlags;
    int         rc;
    PVM         pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * Anything to do?
     */
    if (!cb)
        return VINF_SUCCESS;

    LogFlow(("PGMPhysWriteGCPtr: %RGv %zu\n", GCPtrDst, cb));

    /*
     * Optimize writes within a single page.
     */
    if (((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK) + cb <= PAGE_SIZE)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, (RTGCUINTPTR)GCPtrDst, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrDst), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK;

        /* Mention when we ignore X86_PTE_RW... */
        if (!(fFlags & X86_PTE_RW))
            Log(("PGMPhysGCPtr2GCPhys: Writing to RO page %RGv %#x\n", GCPtrDst, cb));

        /* Mark the guest page as accessed and dirty if necessary. */
        if ((fFlags & (X86_PTE_A | X86_PTE_D)) != (X86_PTE_A | X86_PTE_D))
        {
            rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
            AssertRC(rc);
        }

        return PGMPhysWrite(pVM, GCPhys, pvSrc, cb);
    }

    /*
     * Page by page.
     */
    for (;;)
    {
        /* Convert virtual to physical address + flags */
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, (RTGCUINTPTR)GCPtrDst, &fFlags, &GCPhys);
        AssertMsgRCReturn(rc, ("GetPage failed with %Rrc for %RGv\n", rc, GCPtrDst), rc);
        GCPhys |= (RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK;

        /* Mention when we ignore X86_PTE_RW... */
        if (!(fFlags & X86_PTE_RW))
            Log(("PGMPhysGCPtr2GCPhys: Writing to RO page %RGv %#x\n", GCPtrDst, cb));

        /* Mark the guest page as accessed and dirty if necessary. */
        if ((fFlags & (X86_PTE_A | X86_PTE_D)) != (X86_PTE_A | X86_PTE_D))
        {
            rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
            AssertRC(rc);
        }

        /* copy */
        size_t cbWrite = PAGE_SIZE - ((RTGCUINTPTR)GCPtrDst & PAGE_OFFSET_MASK);
        rc = PGMPhysWrite(pVM, GCPhys, pvSrc, cbWrite);
        if (cbWrite >= cb || RT_FAILURE(rc))
            return rc;

        /* next */
        cb         -= cbWrite;
        pvSrc       = (uint8_t *)pvSrc + cbWrite;
        GCPtrDst   += cbWrite;
    }
}


/**
 * Performs a read of guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the access bits.
 *
 * The current implementation will bypass all access handlers. It may later be
 * changed to at least respect MMIO.
 *
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   pCtxCore    The context core.
 * @param   pvDst       Where to put the bytes we've read.
 * @param   GCPtrSrc    The source address.
 * @param   cb          The number of bytes to read. Not more than a page.
 *
 * @remark  This function will dynamically map physical pages in GC. This may unmap
 *          mappings done by the caller. Be careful!
 */
VMMDECL(int) PGMPhysInterpretedRead(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(cb <= PAGE_SIZE);
    VMCPU_ASSERT_EMT(pVCpu);

/** @todo r=bird: This isn't perfect!
 *  -# It's not checking for reserved bits being 1.
 *  -# It's not correctly dealing with the access bit.
 *  -# It's not respecting MMIO memory or any other access handlers.
 */
    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the read operation.
     * 4. Set access bits if required.
     */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS GCPhys;
        uint64_t fFlags;
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrSrc, &fFlags, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            /** @todo we should check reserved bits ... */
            PGMPAGEMAPLOCK PgMpLck;
            void const    *pvSrc;
            rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, &pvSrc, &PgMpLck);
            switch (rc)
            {
                case VINF_SUCCESS:
                    Log(("PGMPhysInterpretedRead: pvDst=%p pvSrc=%p cb=%d\n", pvDst, (uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb));
                    memcpy(pvDst, (uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb);
                    PGMPhysReleasePageMappingLock(pVM, &PgMpLck);
                    break;
                case VERR_PGM_PHYS_PAGE_RESERVED:
                case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                    memset(pvDst, 0xff, cb);
                    break;
                default:
                    Assert(RT_FAILURE_NP(rc));
                    return rc;
            }

            /** @todo access bit emulation isn't 100% correct. */
            if (!(fFlags & X86_PTE_A))
            {
                rc = PGMGstModifyPage(pVCpu, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                AssertRC(rc);
            }
            return VINF_SUCCESS;
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        size_t cb2 = cb - cb1;
        uint64_t fFlags1;
        RTGCPHYS GCPhys1;
        uint64_t fFlags2;
        RTGCPHYS GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrSrc, &fFlags1, &GCPhys1);
        if (RT_SUCCESS(rc))
        {
            rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrSrc + cb1, &fFlags2, &GCPhys2);
            if (RT_SUCCESS(rc))
            {
                /** @todo we should check reserved bits ... */
                AssertMsgFailed(("cb=%d cb1=%d cb2=%d GCPtrSrc=%RGv\n", cb, cb1, cb2, GCPtrSrc));
                PGMPAGEMAPLOCK PgMpLck;
                void const *pvSrc1;
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys1, &pvSrc1, &PgMpLck);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        memcpy(pvDst, (uint8_t *)pvSrc1 + (GCPtrSrc & PAGE_OFFSET_MASK), cb1);
                        PGMPhysReleasePageMappingLock(pVM, &PgMpLck);
                        break;
                    case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                        memset(pvDst, 0xff, cb1);
                        break;
                    default:
                        Assert(RT_FAILURE_NP(rc));
                        return rc;
                }

                void const *pvSrc2;
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys2, &pvSrc2, &PgMpLck);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        memcpy((uint8_t *)pvDst + cb1, pvSrc2, cb2);
                        PGMPhysReleasePageMappingLock(pVM, &PgMpLck);
                        break;
                    case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                        memset((uint8_t *)pvDst + cb1, 0xff, cb2);
                        break;
                    default:
                        Assert(RT_FAILURE_NP(rc));
                        return rc;
                }

                if (!(fFlags1 & X86_PTE_A))
                {
                    rc = PGMGstModifyPage(pVCpu, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                    AssertRC(rc);
                }
                if (!(fFlags2 & X86_PTE_A))
                {
                    rc = PGMGstModifyPage(pVCpu, GCPtrSrc + cb1, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                    AssertRC(rc);
                }
                return VINF_SUCCESS;
            }
        }
    }

    /*
     * Raise a #PF.
     */
    uint32_t uErr;

    /* Get the current privilege level. */
    uint32_t cpl = CPUMGetGuestCPL(pVCpu);
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Rrc GCPtrSrc=%RGv cb=%#x\n", rc, GCPtrSrc, cb));
            return rc;
    }
    Log(("PGMPhysInterpretedRead: GCPtrSrc=%RGv cb=%#x -> #PF(%#x)\n", GCPtrSrc, cb, uErr));
    return TRPMRaiseXcptErrCR2(pVCpu, pCtxCore, X86_XCPT_PF, uErr, GCPtrSrc);
}


/**
 * Performs a read of guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the access bits.
 *
 * The current implementation will bypass all access handlers. It may later be
 * changed to at least respect MMIO.
 *
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   pCtxCore    The context core.
 * @param   pvDst       Where to put the bytes we've read.
 * @param   GCPtrSrc    The source address.
 * @param   cb          The number of bytes to read. Not more than a page.
 * @param   fRaiseTrap  If set the trap will be raised on as per spec, if clear
 *                      an appropriate error status will be returned (no
 *                      informational at all).
 *
 *
 * @remarks Takes the PGM lock.
 * @remarks A page fault on the 2nd page of the access will be raised without
 *          writing the bits on the first page since we're ASSUMING that the
 *          caller is emulating an instruction access.
 * @remarks This function will dynamically map physical pages in GC. This may
 *          unmap mappings done by the caller. Be careful!
 */
VMMDECL(int) PGMPhysInterpretedReadNoHandlers(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, void *pvDst, RTGCUINTPTR GCPtrSrc, size_t cb,
                                              bool fRaiseTrap)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    Assert(cb <= PAGE_SIZE);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the read operation.
     * 4. Set access bits if required.
     */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrSrc & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS    GCPhys;
        uint64_t    fFlags;
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrSrc, &fFlags, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            if (1) /** @todo we should check reserved bits ... */
            {
                const void *pvSrc;
                PGMPAGEMAPLOCK Lock;
                rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys, &pvSrc, &Lock);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        Log(("PGMPhysInterpretedReadNoHandlers: pvDst=%p pvSrc=%p (%RGv) cb=%d\n",
                               pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), GCPtrSrc, cb));
                        memcpy(pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb);
                        PGMPhysReleasePageMappingLock(pVM, &Lock);
                        break;
                    case VERR_PGM_PHYS_PAGE_RESERVED:
                    case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                        memset(pvDst, 0xff, cb);
                        break;
                    default:
                        AssertMsgFailed(("%Rrc\n", rc));
                        AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                        return rc;
                }

                if (!(fFlags & X86_PTE_A))
                {
                    /** @todo access bit emulation isn't 100% correct. */
                    rc = PGMGstModifyPage(pVCpu, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                    AssertRC(rc);
                }
                return VINF_SUCCESS;
            }
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        size_t      cb2 = cb - cb1;
        uint64_t    fFlags1;
        RTGCPHYS    GCPhys1;
        uint64_t    fFlags2;
        RTGCPHYS    GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrSrc, &fFlags1, &GCPhys1);
        if (RT_SUCCESS(rc))
        {
            rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrSrc + cb1, &fFlags2, &GCPhys2);
            if (RT_SUCCESS(rc))
            {
                if (1) /** @todo we should check reserved bits ... */
                {
                    const void *pvSrc;
                    PGMPAGEMAPLOCK Lock;
                    rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys1, &pvSrc, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            Log(("PGMPhysInterpretedReadNoHandlers: pvDst=%p pvSrc=%p (%RGv) cb=%d [2]\n",
                                   pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), GCPtrSrc, cb1));
                            memcpy(pvDst, (const uint8_t *)pvSrc + (GCPtrSrc & PAGE_OFFSET_MASK), cb1);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            memset(pvDst, 0xff, cb1);
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    rc = PGMPhysGCPhys2CCPtrReadOnly(pVM, GCPhys2, &pvSrc, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            memcpy((uint8_t *)pvDst + cb1, pvSrc, cb2);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            memset((uint8_t *)pvDst + cb1, 0xff, cb2);
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    if (!(fFlags1 & X86_PTE_A))
                    {
                        rc = PGMGstModifyPage(pVCpu, GCPtrSrc, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                        AssertRC(rc);
                    }
                    if (!(fFlags2 & X86_PTE_A))
                    {
                        rc = PGMGstModifyPage(pVCpu, GCPtrSrc + cb1, 1, X86_PTE_A, ~(uint64_t)X86_PTE_A);
                        AssertRC(rc);
                    }
                    return VINF_SUCCESS;
                }
                /* sort out which page */
            }
            else
                GCPtrSrc += cb1; /* fault on 2nd page */
        }
    }

    /*
     * Raise a #PF if we're allowed to do that.
     */
    /* Calc the error bits. */
    uint32_t cpl = CPUMGetGuestCPL(pVCpu);
    uint32_t uErr;
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            rc = VERR_ACCESS_DENIED;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Rrc GCPtrSrc=%RGv cb=%#x\n", rc, GCPtrSrc, cb));
            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
            return rc;
    }
    if (fRaiseTrap)
    {
        Log(("PGMPhysInterpretedReadNoHandlers: GCPtrSrc=%RGv cb=%#x -> Raised #PF(%#x)\n", GCPtrSrc, cb, uErr));
        return TRPMRaiseXcptErrCR2(pVCpu, pCtxCore, X86_XCPT_PF, uErr, GCPtrSrc);
    }
    Log(("PGMPhysInterpretedReadNoHandlers: GCPtrSrc=%RGv cb=%#x -> #PF(%#x) [!raised]\n", GCPtrSrc, cb, uErr));
    return rc;
}


/**
 * Performs a write to guest virtual memory for instruction emulation.
 *
 * This will check permissions, raise exceptions and update the dirty and access
 * bits.
 *
 * @returns VBox status code suitable to scheduling.
 * @retval  VINF_SUCCESS if the read was performed successfully.
 * @retval  VINF_EM_RAW_GUEST_TRAP if an exception was raised but not dispatched yet.
 * @retval  VINF_TRPM_XCPT_DISPATCHED if an exception was raised and dispatched.
 *
 * @param   pVCpu       Handle to the current virtual CPU.
 * @param   pCtxCore    The context core.
 * @param   GCPtrDst    The destination address.
 * @param   pvSrc       What to write.
 * @param   cb          The number of bytes to write. Not more than a page.
 * @param   fRaiseTrap  If set the trap will be raised on as per spec, if clear
 *                      an appropriate error status will be returned (no
 *                      informational at all).
 *
 * @remarks Takes the PGM lock.
 * @remarks A page fault on the 2nd page of the access will be raised without
 *          writing the bits on the first page since we're ASSUMING that the
 *          caller is emulating an instruction access.
 * @remarks This function will dynamically map physical pages in GC. This may
 *          unmap mappings done by the caller. Be careful!
 */
VMMDECL(int) PGMPhysInterpretedWriteNoHandlers(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, RTGCPTR GCPtrDst, const void *pvSrc,
                                               size_t cb, bool fRaiseTrap)
{
    Assert(cb <= PAGE_SIZE);
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    VMCPU_ASSERT_EMT(pVCpu);

    /*
     * 1. Translate virtual to physical. This may fault.
     * 2. Map the physical address.
     * 3. Do the write operation.
     * 4. Set access bits if required.
     */
    /** @todo Since this method is frequently used by EMInterpret or IOM
     *        upon a write fault to an write access monitored page, we can
     *        reuse the guest page table walking from the \#PF code. */
    int rc;
    unsigned cb1 = PAGE_SIZE - (GCPtrDst & PAGE_OFFSET_MASK);
    if (cb <= cb1)
    {
        /*
         * Not crossing pages.
         */
        RTGCPHYS    GCPhys;
        uint64_t    fFlags;
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrDst, &fFlags, &GCPhys);
        if (RT_SUCCESS(rc))
        {
            if (    (fFlags & X86_PTE_RW)                   /** @todo Also check reserved bits. */
                ||  (   !(CPUMGetGuestCR0(pVCpu) & X86_CR0_WP)
                     &&   CPUMGetGuestCPL(pVCpu) <= 2) ) /** @todo it's 2, right? Check cpl check below as well. */
            {
                void *pvDst;
                PGMPAGEMAPLOCK Lock;
                rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys, &pvDst, &Lock);
                switch (rc)
                {
                    case VINF_SUCCESS:
                        Log(("PGMPhysInterpretedWriteNoHandlers: pvDst=%p (%RGv) pvSrc=%p cb=%d\n",
                               (uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), GCPtrDst, pvSrc,  cb));
                        memcpy((uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), pvSrc, cb);
                        PGMPhysReleasePageMappingLock(pVM, &Lock);
                        break;
                    case VERR_PGM_PHYS_PAGE_RESERVED:
                    case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                        /* bit bucket */
                        break;
                    default:
                        AssertMsgFailed(("%Rrc\n", rc));
                        AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                        return rc;
                }

                if (!(fFlags & (X86_PTE_A | X86_PTE_D)))
                {
                    /** @todo dirty & access bit emulation isn't 100% correct. */
                    rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, X86_PTE_A | X86_PTE_D, ~(uint64_t)(X86_PTE_A | X86_PTE_D));
                    AssertRC(rc);
                }
                return VINF_SUCCESS;
            }
            rc = VERR_ACCESS_DENIED;
        }
    }
    else
    {
        /*
         * Crosses pages.
         */
        size_t      cb2 = cb - cb1;
        uint64_t    fFlags1;
        RTGCPHYS    GCPhys1;
        uint64_t    fFlags2;
        RTGCPHYS    GCPhys2;
        rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrDst, &fFlags1, &GCPhys1);
        if (RT_SUCCESS(rc))
        {
            rc = PGM_GST_PFN(GetPage,pVCpu)(pVCpu, GCPtrDst + cb1, &fFlags2, &GCPhys2);
            if (RT_SUCCESS(rc))
            {
                if (    (   (fFlags1 & X86_PTE_RW)  /** @todo Also check reserved bits. */
                         && (fFlags2 & X86_PTE_RW))
                    ||  (   !(CPUMGetGuestCR0(pVCpu) & X86_CR0_WP)
                         &&   CPUMGetGuestCPL(pVCpu) <= 2) )
                {
                    void *pvDst;
                    PGMPAGEMAPLOCK Lock;
                    rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys1, &pvDst, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            Log(("PGMPhysInterpretedWriteNoHandlers: pvDst=%p (%RGv) pvSrc=%p cb=%d\n",
                                   (uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), GCPtrDst, pvSrc, cb1));
                            memcpy((uint8_t *)pvDst + (GCPtrDst & PAGE_OFFSET_MASK), pvSrc, cb1);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            /* bit bucket */
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    rc = PGMPhysGCPhys2CCPtr(pVM, GCPhys2, &pvDst, &Lock);
                    switch (rc)
                    {
                        case VINF_SUCCESS:
                            memcpy(pvDst, (const uint8_t *)pvSrc + cb1, cb2);
                            PGMPhysReleasePageMappingLock(pVM, &Lock);
                            break;
                        case VERR_PGM_PHYS_PAGE_RESERVED:
                        case VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS:
                            /* bit bucket */
                            break;
                        default:
                            AssertMsgFailed(("%Rrc\n", rc));
                            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
                            return rc;
                    }

                    if (!(fFlags1 & (X86_PTE_A | X86_PTE_RW)))
                    {
                        rc = PGMGstModifyPage(pVCpu, GCPtrDst, 1, (X86_PTE_A | X86_PTE_RW), ~(uint64_t)(X86_PTE_A | X86_PTE_RW));
                        AssertRC(rc);
                    }
                    if (!(fFlags2 & (X86_PTE_A | X86_PTE_RW)))
                    {
                        rc = PGMGstModifyPage(pVCpu, GCPtrDst + cb1, 1, (X86_PTE_A | X86_PTE_RW), ~(uint64_t)(X86_PTE_A | X86_PTE_RW));
                        AssertRC(rc);
                    }
                    return VINF_SUCCESS;
                }
                if ((fFlags1 & (X86_PTE_RW)) == X86_PTE_RW)
                    GCPtrDst += cb1; /* fault on the 2nd page. */
                rc = VERR_ACCESS_DENIED;
            }
            else
                GCPtrDst += cb1; /* fault on the 2nd page. */
        }
    }

    /*
     * Raise a #PF if we're allowed to do that.
     */
    /* Calc the error bits. */
    uint32_t uErr;
    uint32_t cpl = CPUMGetGuestCPL(pVCpu);
    switch (rc)
    {
        case VINF_SUCCESS:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RSVD | X86_TRAP_PF_US : X86_TRAP_PF_RSVD;
            rc = VERR_ACCESS_DENIED;
            break;

        case VERR_ACCESS_DENIED:
            uErr = (cpl >= 2) ? X86_TRAP_PF_RW | X86_TRAP_PF_US : X86_TRAP_PF_RW;
            break;

        case VERR_PAGE_NOT_PRESENT:
        case VERR_PAGE_TABLE_NOT_PRESENT:
            uErr = (cpl >= 2) ? X86_TRAP_PF_US : 0;
            break;

        default:
            AssertMsgFailed(("rc=%Rrc GCPtrDst=%RGv cb=%#x\n", rc, GCPtrDst, cb));
            AssertReturn(RT_FAILURE(rc), VERR_IPE_UNEXPECTED_INFO_STATUS);
            return rc;
    }
    if (fRaiseTrap)
    {
        Log(("PGMPhysInterpretedWriteNoHandlers: GCPtrDst=%RGv cb=%#x -> Raised #PF(%#x)\n", GCPtrDst, cb, uErr));
        return TRPMRaiseXcptErrCR2(pVCpu, pCtxCore, X86_XCPT_PF, uErr, GCPtrDst);
    }
    Log(("PGMPhysInterpretedWriteNoHandlers: GCPtrDst=%RGv cb=%#x -> #PF(%#x) [!raised]\n", GCPtrDst, cb, uErr));
    return rc;
}


/**
 * Return the page type of the specified physical address.
 *
 * @returns The page type.
 * @param   pVM             Pointer to the VM.
 * @param   GCPhys          Guest physical address
 */
VMMDECL(PGMPAGETYPE) PGMPhysGetPageType(PVM pVM, RTGCPHYS GCPhys)
{
    pgmLock(pVM);
    PPGMPAGE pPage = pgmPhysGetPage(pVM, GCPhys);
    PGMPAGETYPE enmPgType = pPage ? (PGMPAGETYPE)PGM_PAGE_GET_TYPE(pPage) : PGMPAGETYPE_INVALID;
    pgmUnlock(pVM);

    return enmPgType;
}




/**
 * Converts a GC physical address to a HC ring-3 pointer, with some
 * additional checks.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_PHYS_TLB_CATCH_WRITE and *ppv set if the page has a write
 *          access handler of some kind.
 * @retval  VERR_PGM_PHYS_TLB_CATCH_ALL if the page has a handler catching all
 *          accesses or is odd in any way.
 * @retval  VERR_PGM_PHYS_TLB_UNASSIGNED if the page doesn't exist.
 *
 * @param   pVM         Pointer to the VM.
 * @param   GCPhys      The GC physical address to convert.  Since this is only
 *                      used for filling the REM TLB, the A20 mask must be
 *                      applied before calling this API.
 * @param   fWritable   Whether write access is required.
 * @param   ppv         Where to store the pointer corresponding to GCPhys on
 *                      success.
 * @param   pLock
 *
 * @remarks This is more or a less a copy of PGMR3PhysTlbGCPhys2Ptr.
 */
VMM_INT_DECL(int) PGMPhysIemGCPhys2Ptr(PVM pVM, RTGCPHYS GCPhys, bool fWritable, bool fByPassHandlers,
                                       void **ppv, PPGMPAGEMAPLOCK pLock)
{
    pgmLock(pVM);
    PGM_A20_ASSERT_MASKED(VMMGetCpu(pVM), GCPhys);

    PPGMRAMRANGE pRam;
    PPGMPAGE pPage;
    int rc = pgmPhysGetPageAndRangeEx(pVM, GCPhys, &pPage, &pRam);
    if (RT_SUCCESS(rc))
    {
        if (PGM_PAGE_IS_BALLOONED(pPage))
            rc = VINF_PGM_PHYS_TLB_CATCH_WRITE;
        else if (   !PGM_PAGE_HAS_ANY_HANDLERS(pPage)
                 || (fByPassHandlers && !PGM_PAGE_IS_MMIO(pPage)) )
            rc = VINF_SUCCESS;
        else
        {
            if (PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)) /* catches MMIO */
            {
                Assert(!fByPassHandlers || PGM_PAGE_IS_MMIO(pPage));
                rc = VERR_PGM_PHYS_TLB_CATCH_ALL;
            }
            else if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage) && fWritable)
            {
                Assert(!fByPassHandlers);
                rc = VINF_PGM_PHYS_TLB_CATCH_WRITE;
            }
        }
        if (RT_SUCCESS(rc))
        {
            int rc2;

            /* Make sure what we return is writable. */
            if (fWritable)
                switch (PGM_PAGE_GET_STATE(pPage))
                {
                    case PGM_PAGE_STATE_ALLOCATED:
                        break;
                    case PGM_PAGE_STATE_BALLOONED:
                        AssertFailed();
                        break;
                    case PGM_PAGE_STATE_ZERO:
                    case PGM_PAGE_STATE_SHARED:
                        if (rc == VINF_PGM_PHYS_TLB_CATCH_WRITE)
                            break;
                    case PGM_PAGE_STATE_WRITE_MONITORED:
                        rc2 = pgmPhysPageMakeWritable(pVM, pPage, GCPhys & ~(RTGCPHYS)PAGE_OFFSET_MASK);
                        AssertLogRelRCReturn(rc2, rc2);
                        break;
                }

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
            PVMCPU pVCpu = VMMGetCpu(pVM);
            void *pv;
            rc = pgmRZDynMapHCPageInlined(pVCpu,
                                          PGM_PAGE_GET_HCPHYS(pPage),
                                          &pv
                                          RTLOG_COMMA_SRC_POS);
            if (RT_FAILURE(rc))
                return rc;
            *ppv = (void *)((uintptr_t)pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
            pLock->pvPage = pv;
            pLock->pVCpu  = pVCpu;

#else
            /* Get a ring-3 mapping of the address. */
            PPGMPAGER3MAPTLBE pTlbe;
            rc2 = pgmPhysPageQueryTlbeWithPage(pVM, pPage, GCPhys, &pTlbe);
            AssertLogRelRCReturn(rc2, rc2);

            /* Lock it and calculate the address. */
            if (fWritable)
                pgmPhysPageMapLockForWriting(pVM, pPage, pTlbe, pLock);
            else
                pgmPhysPageMapLockForReading(pVM, pPage, pTlbe, pLock);
            *ppv = (void *)((uintptr_t)pTlbe->pv | (uintptr_t)(GCPhys & PAGE_OFFSET_MASK));
#endif


            Log6(("PGMPhysIemGCPhys2Ptr: GCPhys=%RGp rc=%Rrc pPage=%R[pgmpage] *ppv=%p\n", GCPhys, rc, pPage, *ppv));
        }
        else
            Log6(("PGMPhysIemGCPhys2Ptr: GCPhys=%RGp rc=%Rrc pPage=%R[pgmpage]\n", GCPhys, rc, pPage));

        /* else: handler catching all access, no pointer returned. */
    }
    else
        rc = VERR_PGM_PHYS_TLB_UNASSIGNED;

    pgmUnlock(pVM);
    return rc;
}

