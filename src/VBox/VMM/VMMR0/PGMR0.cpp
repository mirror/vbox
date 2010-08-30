/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Ring-0.
 */

/*
 * Copyright (C) 2007-2010 Oracle Corporation
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
#include <VBox/gmm.h>
#include "../PGMInternal.h"
#include <VBox/vm.h>
#include "../PGMInline.h"
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/mem.h>


/*
 * Instantiate the ring-0 header/code templates.
 */
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
 * @param   enmShwPagingMode    Paging mode for the nested page tables.
 * @param   uErr                The trap error code.
 * @param   pRegFrame           Trap register frame.
 * @param   GCPhysFault         The fault address.
 */
VMMR0DECL(int) PGMR0Trap0eHandlerNestedPaging(PVM pVM, PVMCPU pVCpu, PGMMODE enmShwPagingMode, RTGCUINT uErr,
                                              PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault)
{
    int rc;

    LogFlow(("PGMTrap0eHandler: uErr=%RGx GCPhysFault=%RGp eip=%RGv\n", uErr, GCPhysFault, (RTGCPTR)pRegFrame->rip));
    STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0e, a);
    STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = NULL; } );

    /* AMD uses the host's paging mode; Intel has a single mode (EPT). */
    AssertMsg(   enmShwPagingMode == PGMMODE_32_BIT || enmShwPagingMode == PGMMODE_PAE      || enmShwPagingMode == PGMMODE_PAE_NX
              || enmShwPagingMode == PGMMODE_AMD64  || enmShwPagingMode == PGMMODE_AMD64_NX || enmShwPagingMode == PGMMODE_EPT,
              ("enmShwPagingMode=%d\n", enmShwPagingMode));

    /* Reserved shouldn't end up here. */
    Assert(!(uErr & X86_TRAP_PF_RSVD));

#ifdef VBOX_WITH_STATISTICS
    /*
     * Error code stats.
     */
    if (uErr & X86_TRAP_PF_US)
    {
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eUSNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eUSNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eUSWrite);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eUSReserved);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eUSNXE);
        else
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eUSRead);
    }
    else
    {   /* Supervisor */
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eSVNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eSVNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eSVWrite);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eSNXE);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eSVReserved);
    }
#endif

    /*
     * Call the worker.
     *
     * Note! We pretend the guest is in protected mode without paging, so we
     *       can use existing code to build the nested page tables.
     */
    bool fLockTaken = false;
    switch(enmShwPagingMode)
    {
        case PGMMODE_32_BIT:
            rc = PGM_BTH_NAME_32BIT_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, GCPhysFault, &fLockTaken);
            break;
        case PGMMODE_PAE:
        case PGMMODE_PAE_NX:
            rc = PGM_BTH_NAME_PAE_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, GCPhysFault, &fLockTaken);
            break;
        case PGMMODE_AMD64:
        case PGMMODE_AMD64_NX:
            rc = PGM_BTH_NAME_AMD64_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, GCPhysFault, &fLockTaken);
            break;
        case PGMMODE_EPT:
            rc = PGM_BTH_NAME_EPT_PROT(Trap0eHandler)(pVCpu, uErr, pRegFrame, GCPhysFault, &fLockTaken);
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
    /* Note: hack alert for difficult to reproduce problem. */
    else if (   rc == VERR_PAGE_NOT_PRESENT                 /* SMP only ; disassembly might fail. */
             || rc == VERR_PAGE_TABLE_NOT_PRESENT           /* seen with UNI & SMP */
             || rc == VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT   /* seen with SMP */
             || rc == VERR_PAGE_MAP_LEVEL4_NOT_PRESENT)     /* precaution */
    {
        Log(("WARNING: Unexpected VERR_PAGE_TABLE_NOT_PRESENT (%d) for page fault at %RGp error code %x (rip=%RGv)\n", rc, GCPhysFault, uErr, pRegFrame->rip));
        /* Some kind of inconsistency in the SMP case; it's safe to just execute the instruction again; not sure about
           single VCPU VMs though. */
        rc = VINF_SUCCESS;
    }

    STAM_STATS({ if (!pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution))
                    pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0eTime2Misc; });
    STAM_PROFILE_STOP_EX(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatRZTrap0e, pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution), a);
    return rc;
}


/**
 * #PF Handler for deliberate nested paging misconfiguration (/reserved bit)
 * employed for MMIO pages.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM                 The VM Handle.
 * @param   pVCpu               The current CPU.
 * @param   enmShwPagingMode    Paging mode for the nested page tables.
 * @param   pRegFrame           Trap register frame.
 * @param   GCPhysFault         The fault address.
 * @param   uErr                The error code, UINT32_MAX if not available
 *                              (VT-x).
 */
VMMR0DECL(VBOXSTRICTRC) PGMR0Trap0eHandlerNPMisconfig(PVM pVM, PVMCPU pVCpu, PGMMODE enmShwPagingMode,
                                                      PCPUMCTXCORE pRegFrame, RTGCPHYS GCPhysFault, uint32_t uErr)
{
#ifdef PGM_WITH_MMIO_OPTIMIZATIONS
    STAM_PROFILE_START(&pVCpu->CTX_SUFF(pStats)->StatR0NpMiscfg, a);
    VBOXSTRICTRC rc;

    /*
     * Try lookup the all access physical handler for the address.
     */
    pgmLock(pVM);
    PPGMPHYSHANDLER pHandler = pgmHandlerPhysicalLookup(pVM, GCPhysFault);
    if (RT_LIKELY(pHandler && pHandler->enmType != PGMPHYSHANDLERTYPE_PHYSICAL_WRITE))
    {
        /*
         * If the handle has aliases page or pages that have been temporarily
         * disabled, we'll have to take a detour to make sure we resync them
         * to avoid lots of unnecessary exits.
         */
        PPGMPAGE pPage;
        if (   (   pHandler->cAliasedPages
                || pHandler->cTmpOffPages)
            && (   (pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhysFault)) == NULL
                || PGM_PAGE_GET_HNDL_PHYS_STATE(pPage) == PGM_PAGE_HNDL_PHYS_STATE_DISABLED)
           )
        {
            Log(("PGMR0Trap0eHandlerNPMisconfig: Resyncing aliases / tmp-off page at %RGp (uErr=%#x) %R[pgmpage]\n", GCPhysFault, uErr, pPage));
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatR0NpMiscfgSyncPage);
            rc = pgmShwSyncNestedPageLocked(pVCpu, GCPhysFault, 1 /*cPages*/, enmShwPagingMode);
            pgmUnlock(pVM);
        }
        else
        {
            if (pHandler->CTX_SUFF(pfnHandler))
            {
                CTX_MID(PFNPGM,PHYSHANDLER) pfnHandler = pHandler->CTX_SUFF(pfnHandler);
                void                       *pvUser     = pHandler->CTX_SUFF(pvUser);
                STAM_PROFILE_START(&pHandler->Stat, h);
                pgmUnlock(pVM);

                Log6(("PGMR0Trap0eHandlerNPMisconfig: calling %p(,%#x,,%RGp,%p)\n", pfnHandler, uErr, GCPhysFault, pvUser));
                rc = pfnHandler(pVM, uErr == UINT32_MAX ? RTGCPTR_MAX : uErr, pRegFrame, GCPhysFault, GCPhysFault, pvUser);

#ifdef VBOX_WITH_STATISTICS
                pgmLock(pVM);
                pHandler = pgmHandlerPhysicalLookup(pVM, GCPhysFault);
                if (pHandler)
                    STAM_PROFILE_STOP(&pHandler->Stat, h);
                pgmUnlock(pVM);
#endif
            }
            else
            {
                pgmUnlock(pVM);
                Log(("PGMR0Trap0eHandlerNPMisconfig: %RGp (uErr=%#x) -> R3\n", GCPhysFault, uErr));
                rc = VINF_EM_RAW_EMULATE_INSTR;
            }
        }
    }
    else
    {
        /*
         * Must be out of sync, so do a SyncPage and restart the instruction.
         *
         * ASSUMES that ALL handlers are page aligned and covers whole pages
         * (assumption asserted in PGMHandlerPhysicalRegisterEx).
         */
        Log(("PGMR0Trap0eHandlerNPMisconfig: Out of sync page at %RGp (uErr=%#x)\n", GCPhysFault, uErr));
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatR0NpMiscfgSyncPage);
        rc = pgmShwSyncNestedPageLocked(pVCpu, GCPhysFault, 1 /*cPages*/, enmShwPagingMode);
        pgmUnlock(pVM);
    }

    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_SUFF(pStats)->StatR0NpMiscfg, a);
    return rc;

#else
    AssertLogRelFailed();
    return VERR_INTERNAL_ERROR_4;
#endif
}


