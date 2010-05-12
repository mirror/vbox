/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Ring-0.
 */

/*
 * Copyright (C) 2007 Oracle Corporation
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
#include "../PGMInternal.h"
#include <VBox/vm.h>
#include "../PGMInline.h"
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/mem.h>

RT_C_DECLS_BEGIN
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_PROT(name)
#include "PGMR0Bth.h"
#undef PGM_BTH_NAME

#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PROT(name)
#include "PGMR0Bth.h"
#undef PGM_BTH_NAME

#define PGM_BTH_NAME(name)          PGM_BTH_NAME_AMD64_PROT(name)
#include "PGMR0Bth.h"
#undef PGM_BTH_NAME

#define PGM_BTH_NAME(name)          PGM_BTH_NAME_EPT_PROT(name)
#include "PGMR0Bth.h"
#undef PGM_BTH_NAME

RT_C_DECLS_END


/**
 * Worker function for PGMR3PhysAllocateHandyPages and pgmPhysEnsureHandyPage.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is set in this case.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 *
 * @remarks Must be called from within the PGM critical section. The caller
 *          must clear the new pages.
 */
VMMR0DECL(int) PGMR0PhysAllocateHandyPages(PVM pVM, PVMCPU pVCpu)
{
    Assert(PDMCritSectIsOwnerEx(&pVM->pgm.s.CritSect, pVCpu->idCpu));

    /*
     * Check for error injection.
     */
    if (RT_UNLIKELY(pVM->pgm.s.fErrInjHandyPages))
        return VERR_NO_MEMORY;

    /*
     * Try allocate a full set of handy pages.
     */
    uint32_t iFirst = pVM->pgm.s.cHandyPages;
    AssertReturn(iFirst <= RT_ELEMENTS(pVM->pgm.s.aHandyPages), VERR_INTERNAL_ERROR);
    uint32_t cPages = RT_ELEMENTS(pVM->pgm.s.aHandyPages) - iFirst;
    if (!cPages)
        return VINF_SUCCESS;
    int rc = GMMR0AllocateHandyPages(pVM, pVCpu->idCpu, cPages, cPages, &pVM->pgm.s.aHandyPages[iFirst]);
    if (RT_SUCCESS(rc))
    {
        for (uint32_t i = 0; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
        {
            Assert(pVM->pgm.s.aHandyPages[i].idPage != NIL_GMM_PAGEID);
            Assert(pVM->pgm.s.aHandyPages[i].idPage <= GMM_PAGEID_LAST);
            Assert(pVM->pgm.s.aHandyPages[i].idSharedPage == NIL_GMM_PAGEID);
            Assert(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys != NIL_RTHCPHYS);
            Assert(!(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys & ~X86_PTE_PAE_PG_MASK));
        }

        pVM->pgm.s.cHandyPages = RT_ELEMENTS(pVM->pgm.s.aHandyPages);
    }
    else if (rc != VERR_GMM_SEED_ME)
    {
        if (    (   rc == VERR_GMM_HIT_GLOBAL_LIMIT
                 || rc == VERR_GMM_HIT_VM_ACCOUNT_LIMIT)
            &&  iFirst < PGM_HANDY_PAGES_MIN)
        {

#ifdef VBOX_STRICT
            /* We're ASSUMING that GMM has updated all the entires before failing us. */
            uint32_t i;
            for (i = iFirst; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
            {
                Assert(pVM->pgm.s.aHandyPages[i].idPage == NIL_GMM_PAGEID);
                Assert(pVM->pgm.s.aHandyPages[i].idSharedPage == NIL_GMM_PAGEID);
                Assert(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys == NIL_RTHCPHYS);
            }
#endif

            /*
             * Reduce the number of pages until we hit the minimum limit.
             */
            do
            {
                cPages >>= 2;
                if (cPages + iFirst < PGM_HANDY_PAGES_MIN)
                    cPages = PGM_HANDY_PAGES_MIN - iFirst;
                rc = GMMR0AllocateHandyPages(pVM, pVCpu->idCpu, cPages, cPages, &pVM->pgm.s.aHandyPages[iFirst]);
            } while (   (   rc == VERR_GMM_HIT_GLOBAL_LIMIT
                         || rc == VERR_GMM_HIT_VM_ACCOUNT_LIMIT)
                     && cPages + iFirst > PGM_HANDY_PAGES_MIN);
            if (RT_SUCCESS(rc))
            {
#ifdef VBOX_STRICT
                i = iFirst + cPages;
                while (i-- > 0)
                {
                    Assert(pVM->pgm.s.aHandyPages[i].idPage != NIL_GMM_PAGEID);
                    Assert(pVM->pgm.s.aHandyPages[i].idPage <= GMM_PAGEID_LAST);
                    Assert(pVM->pgm.s.aHandyPages[i].idSharedPage == NIL_GMM_PAGEID);
                    Assert(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys != NIL_RTHCPHYS);
                    Assert(!(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys & ~X86_PTE_PAE_PG_MASK));
                }

                for (i = cPages + iFirst; i < RT_ELEMENTS(pVM->pgm.s.aHandyPages); i++)
                {
                    Assert(pVM->pgm.s.aHandyPages[i].idPage == NIL_GMM_PAGEID);
                    Assert(pVM->pgm.s.aHandyPages[i].idSharedPage == NIL_GMM_PAGEID);
                    Assert(pVM->pgm.s.aHandyPages[i].HCPhysGCPhys == NIL_RTHCPHYS);
                }
#endif

                pVM->pgm.s.cHandyPages = iFirst + cPages;
            }
        }

        if (RT_FAILURE(rc) && rc != VERR_GMM_SEED_ME)
        {
            LogRel(("PGMR0PhysAllocateHandyPages: rc=%Rrc iFirst=%d cPages=%d\n", rc, iFirst, cPages));
            VM_FF_SET(pVM, VM_FF_PGM_NO_MEMORY);
        }
    }


    LogFlow(("PGMR0PhysAllocateHandyPages: cPages=%d rc=%Rrc\n", cPages, rc));
    return rc;
}

/**
 * Worker function for PGMR3PhysAllocateLargeHandyPage
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 *
 * @remarks Must be called from within the PGM critical section. The caller
 *          must clear the new pages.
 */
VMMR0DECL(int) PGMR0PhysAllocateLargeHandyPage(PVM pVM, PVMCPU pVCpu)
{
    Assert(PDMCritSectIsOwnerEx(&pVM->pgm.s.CritSect, pVCpu->idCpu));

    Assert(!pVM->pgm.s.cLargeHandyPages);
    int rc = GMMR0AllocateLargePage(pVM, pVCpu->idCpu, _2M, &pVM->pgm.s.aLargeHandyPage[0].idPage, &pVM->pgm.s.aLargeHandyPage[0].HCPhysGCPhys);
    if (RT_SUCCESS(rc))
        pVM->pgm.s.cLargeHandyPages = 1;

    return rc;
}

/**
 * #PF Handler for nested paging.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM                 VM Handle.
 * @param   pVCpu               VMCPU Handle.
 * @param   enmShwPagingMode    Paging mode for the nested page tables
 * @param   uErr                The trap error code.
 * @param   pRegFrame           Trap register frame.
 * @param   pvFault             The fault address.
 */
VMMR0DECL(int) PGMR0Trap0eHandlerNestedPaging(PVM pVM, PVMCPU pVCpu, PGMMODE enmShwPagingMode, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPHYS pvFault)
{
    int rc;

    LogFlow(("PGMTrap0eHandler: uErr=%RGx pvFault=%RGp eip=%RGv\n", uErr, pvFault, (RTGCPTR)pRegFrame->rip));
    STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0e, a);
    STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = NULL; } );

    /* AMD uses the host's paging mode; Intel has a single mode (EPT). */
    AssertMsg(enmShwPagingMode == PGMMODE_32_BIT || enmShwPagingMode == PGMMODE_PAE || enmShwPagingMode == PGMMODE_PAE_NX || enmShwPagingMode == PGMMODE_AMD64 || enmShwPagingMode == PGMMODE_AMD64_NX || enmShwPagingMode == PGMMODE_EPT, ("enmShwPagingMode=%d\n", enmShwPagingMode));

#ifdef VBOX_WITH_STATISTICS
    /*
     * Error code stats.
     */
    if (uErr & X86_TRAP_PF_US)
    {
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eUSNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eUSNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eUSWrite);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eUSReserved);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eUSNXE);
        else
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eUSRead);
    }
    else
    {   /* Supervisor */
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eSVNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eSVNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eSVWrite);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eSNXE);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eSVReserved);
    }
#endif

    /*
     * Call the worker.
     *
     * We pretend the guest is in protected mode without paging, so we can use existing code to build the
     * nested page tables.
     */
    bool fLockTaken = false;
    switch(enmShwPagingMode)
    {
    case PGMMODE_32_BIT:
        rc = PGM_BTH_NAME_32BIT_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, pvFault, &fLockTaken);
        break;
    case PGMMODE_PAE:
    case PGMMODE_PAE_NX:
        rc = PGM_BTH_NAME_PAE_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, pvFault, &fLockTaken);
        break;
    case PGMMODE_AMD64:
    case PGMMODE_AMD64_NX:
        rc = PGM_BTH_NAME_AMD64_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, pvFault, &fLockTaken);
        break;
    case PGMMODE_EPT:
        rc = PGM_BTH_NAME_EPT_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, pvFault, &fLockTaken);
        break;
    default:
        AssertFailed();
        rc = VERR_INVALID_PARAMETER;
        break;
    }
    if (fLockTaken)
    {
        Assert(PGMIsLockOwner(pVM));
        pgmUnlock(pVM);
    }
    if (rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE)
        rc = VINF_SUCCESS;
    else
    /* Note: hack alert for difficult to reproduce problem. */
    if (    rc == VERR_PAGE_NOT_PRESENT                 /* SMP only ; disassembly might fail. */
        ||  rc == VERR_PAGE_TABLE_NOT_PRESENT           /* seen with UNI & SMP */
        ||  rc == VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT   /* seen with SMP */
        ||  rc == VERR_PAGE_MAP_LEVEL4_NOT_PRESENT)     /* precaution */
    {
        Log(("WARNING: Unexpected VERR_PAGE_TABLE_NOT_PRESENT (%d) for page fault at %RGp error code %x (rip=%RGv)\n", rc, pvFault, uErr, pRegFrame->rip));
        /* Some kind of inconsistency in the SMP case; it's safe to just execute the instruction again; not sure about single VCPU VMs though. */
        rc = VINF_SUCCESS;
    }

    STAM_STATS({ if (!pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution))
                    pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2Misc; });
    STAM_PROFILE_STOP_EX(&pVCpu->pgm.s.StatRZTrap0e, pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution), a);
    return rc;
}

#ifdef VBOX_WITH_PAGE_SHARING
/**
 * Check a registered module for shared page changes
 *
 * @returns The following VBox status codes.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The VMCPU handle.
 * @param   pReq        Module request packet
 */
VMMR0DECL(int) PGMR0SharedModuleCheck(PVM pVM, PVMCPU pVCpu, PGMMREGISTERSHAREDMODULEREQ pReq)
{
    int                rc = VINF_SUCCESS;
    PGMMSHAREDPAGEDESC paPageDesc = NULL;
    uint32_t           cbPreviousRegion  = 0;
    bool               fFlushTLBs = false;

    /*
     * Validate input.
     */
    AssertPtrReturn(pReq, VERR_INVALID_POINTER);
    AssertMsgReturn(pReq->Hdr.cbReq >= sizeof(*pReq) && pReq->Hdr.cbReq == RT_UOFFSETOF(GMMREGISTERSHAREDMODULEREQ, aRegions[pReq->cRegions]), ("%#x != %#x\n", pReq->Hdr.cbReq, RT_UOFFSETOF(GMMREGISTERSHAREDMODULEREQ, aRegions[pReq->cRegions])), VERR_INVALID_PARAMETER);

    pgmLock(pVM);

    /* Check every region of the shared module. */
    for (unsigned i = 0; i < pReq->cRegions; i++)
    {
        Assert((pReq->aRegions[i].cbRegion & 0xfff) == 0);
        Assert((pReq->aRegions[i].GCRegionAddr & 0xfff) == 0);

        RTGCPTR  GCRegion  = pReq->aRegions[i].GCRegionAddr;
        unsigned cbRegion = pReq->aRegions[i].cbRegion & ~0xfff;
        unsigned idxPage = 0;
        bool     fValidChanges = false;

        if (cbPreviousRegion < cbRegion)
        {
            if (paPageDesc)
                RTMemFree(paPageDesc);

            paPageDesc = (PGMMSHAREDPAGEDESC)RTMemAlloc((cbRegion >> PAGE_SHIFT) * sizeof(*paPageDesc));
            if (!paPageDesc)
            {
                AssertFailed();
                rc = VERR_NO_MEMORY;
                goto end;
            }
            cbPreviousRegion  = cbRegion;
        }

        while (cbRegion)
        {
            RTGCPHYS GCPhys;
            uint64_t fFlags;

            rc = PGMGstGetPage(pVCpu, GCRegion, &GCPhys, &fFlags);
            if (    rc == VINF_SUCCESS
                &&  !(fFlags & X86_PTE_RW)) /* important as we make assumptions about this below! */
            {
                PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
                if (    pPage
                    &&  !PGM_PAGE_IS_SHARED(pPage))
                {
                    fValidChanges = true;
                    paPageDesc[idxPage].uHCPhysPageId = PGM_PAGE_GET_PAGEID(pPage);
                    paPageDesc[idxPage].HCPhys        = PGM_PAGE_GET_HCPHYS(pPage);
                    paPageDesc[idxPage].GCPhys        = GCPhys;
                }
                else
                    paPageDesc[idxPage].uHCPhysPageId = NIL_GMM_PAGEID;
            }
            else
                paPageDesc[idxPage].uHCPhysPageId = NIL_GMM_PAGEID;

            idxPage++;
            GCRegion += PAGE_SIZE;
            cbRegion -= PAGE_SIZE;
        }

        if (fValidChanges)
        {
            rc = GMMR0SharedModuleCheckRange(pVM, pVCpu->idCpu, pReq, i, idxPage, paPageDesc);
            AssertRC(rc);
            if (RT_FAILURE(rc))
                break;
                
            for (unsigned i = 0; i < idxPage; i++)
            {
                /* Any change for this page? */
                if (paPageDesc[i].uHCPhysPageId != NIL_GMM_PAGEID)
                {
                    /** todo: maybe cache these to prevent the nth lookup. */
                    PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, paPageDesc[i].GCPhys);
                    if (!pPage)
                    {
                        /* Should never happen. */
                        AssertFailed();
                        rc = VERR_PGM_PHYS_INVALID_PAGE_ID;
                        goto end;
                    }
                    Assert(!PGM_PAGE_IS_SHARED(pPage));

                    if (paPageDesc[i].HCPhys != PGM_PAGE_GET_HCPHYS(pPage))
                    {
                        bool fFlush = false;

                        /* Page was replaced by an existing shared version of it; clear all references first. */
                        rc = pgmPoolTrackUpdateGCPhys(pVM, paPageDesc[i].GCPhys, pPage, true /* clear the entries */, &fFlush);
                        if (RT_FAILURE(rc))
                        {
                            AssertRC(rc);
                            goto end;
                        }
                        Assert(rc == VINF_SUCCESS || (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3) && (pVCpu->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL)));
                        if (rc = VINF_SUCCESS)
                            fFlushTLBs |= fFlush;

                        /* Update the physical address and page id now. */
                        PGM_PAGE_SET_HCPHYS(pPage, paPageDesc[i].HCPhys);
                        PGM_PAGE_SET_PAGEID(pPage, paPageDesc[i].uHCPhysPageId);

                        /* Invalidate page map TLB entry for this page too. */
                        PGMPhysInvalidatePageMapTLBEntry(pVM, paPageDesc[i].GCPhys);
                    }
                    /* else nothing changed (== this page is now a shared page), so no need to flush anything. */

                    PGM_PAGE_SET_STATE(pPage, PGM_PAGE_STATE_SHARED);
                }
            }
        }
    }

end:
    pgmUnlock(pVM);
    if (fFlushTLBs)
        PGM_INVL_ALL_VCPU_TLBS(pVM);

    if (paPageDesc)
        RTMemFree(paPageDesc);

    return rc;
}
#endif
