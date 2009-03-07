/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor, Ring-0.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PGM
#include <VBox/pgm.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <iprt/assert.h>

__BEGIN_DECLS
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

__END_DECLS


/**
 * Worker function for PGMR3PhysAllocateHandyPages and pgmPhysEnsureHandyPage.
 *
 * @returns The following VBox status codes.
 * @retval  VINF_SUCCESS on success. FF cleared.
 * @retval  VINF_EM_NO_MEMORY if we're out of memory. The FF is set in this case.
 *
 * @param   pVM         The VM handle.
 *
 * @remarks Must be called from within the PGM critical section. The caller
 *          must clear the new pages.
 */
VMMR0DECL(int) PGMR0PhysAllocateHandyPages(PVM pVM)
{
    Assert(PDMCritSectIsOwner(&pVM->pgm.s.CritSect));

    uint32_t iFirst = pVM->pgm.s.cHandyPages;
    if (iFirst > RT_ELEMENTS(pVM->pgm.s.aHandyPages))
    {
        LogRel(("%d", iFirst));
        return VERR_INTERNAL_ERROR;
    }

    uint32_t cPages = RT_ELEMENTS(pVM->pgm.s.aHandyPages) - iFirst;
    if (!cPages)
        return VINF_SUCCESS;

    int rc = GMMR0AllocateHandyPages(pVM, cPages, cPages, &pVM->pgm.s.aHandyPages[iFirst]);
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
        LogRel(("PGMR0PhysAllocateHandyPages: rc=%Rrc iFirst=%d cPages=%d\n", rc, iFirst, cPages));
    LogFlow(("PGMR0PhysAllocateHandyPages: cPages=%d rc=%Rrc\n", cPages, rc));
    return rc;
}


/**
 * #PF Handler for nested paging.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM                 VM Handle.
 * @param   enmShwPagingMode    Paging mode for the nested page tables
 * @param   uErr                The trap error code.
 * @param   pRegFrame           Trap register frame.
 * @param   pvFault             The fault address.
 */
VMMR0DECL(int) PGMR0Trap0eHandlerNestedPaging(PVM pVM, PGMMODE enmShwPagingMode, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPHYS pvFault)
{
    int rc;

    LogFlow(("PGMTrap0eHandler: uErr=%#x pvFault=%RGp eip=%RGv\n", uErr, pvFault, (RTGCPTR)pRegFrame->rip));
    STAM_PROFILE_START(&pVM->pgm.s.StatRZTrap0e, a);
    STAM_STATS({ pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = NULL; } );

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
                STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eUSNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eUSNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eUSWrite);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eUSReserved);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eUSNXE);
        else
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eUSRead);
    }
    else
    {   /* Supervisor */
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eSVNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eSVNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eSVWrite);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eSNXE);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eSVReserved);
    }
#endif

    /*
     * Call the worker.
     *
     * We pretend the guest is in protected mode without paging, so we can use existing code to build the
     * nested page tables.
     */
    switch(enmShwPagingMode)
    {
    case PGMMODE_32_BIT:
        rc = PGM_BTH_NAME_32BIT_PROT(Trap0eHandler)(pVM, uErr, pRegFrame, pvFault);
        break;
    case PGMMODE_PAE:
    case PGMMODE_PAE_NX:
        rc = PGM_BTH_NAME_PAE_PROT(Trap0eHandler)(pVM, uErr, pRegFrame, pvFault);
        break;
    case PGMMODE_AMD64:
    case PGMMODE_AMD64_NX:
        rc = PGM_BTH_NAME_AMD64_PROT(Trap0eHandler)(pVM, uErr, pRegFrame, pvFault);
        break;
    case PGMMODE_EPT:
        rc = PGM_BTH_NAME_EPT_PROT(Trap0eHandler)(pVM, uErr, pRegFrame, pvFault);
        break;
    default:
        AssertFailed();
        rc = VERR_INVALID_PARAMETER;
        break;
    }
    if (rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE)
        rc = VINF_SUCCESS;
    STAM_STATS({ if (!pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution))
                    pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatRZTrap0eTime2Misc; });
    STAM_PROFILE_STOP_EX(&pVM->pgm.s.StatRZTrap0e, pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution), a);
    return rc;
}

