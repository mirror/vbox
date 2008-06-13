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
 * @remarks Must be called from within the PGM critical section.
 */
PGMR0DECL(int) PGMR0PhysAllocateHandyPages(PVM pVM)
{
    return VERR_NOT_IMPLEMENTED;
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
PGMR0DECL(int) PGMR0Trap0eHandlerNestedPaging(PVM pVM, PGMMODE enmShwPagingMode, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPHYS pvFault)
{
    int rc;

    LogFlow(("PGMTrap0eHandler: uErr=%#x pvFault=%VGp eip=%VGv\n", uErr, pvFault, pRegFrame->rip));
    STAM_PROFILE_START(&pVM->pgm.s.StatGCTrap0e, a);
    STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = NULL; } );

    /* AMD uses the host's paging mode; Intel's version is on the todo list */
    Assert(enmShwPagingMode == PGMMODE_32_BIT || enmShwPagingMode == PGMMODE_PAE || enmShwPagingMode == PGMMODE_AMD64);

#ifdef VBOX_WITH_STATISTICS
    /*
     * Error code stats.
     */
    if (uErr & X86_TRAP_PF_US)
    {
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUSNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUSNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUSWrite);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUSReserved);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUSNXE);
        else
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUSRead);
    }
    else
    {   /* Supervisor */
        if (!(uErr & X86_TRAP_PF_P))
        {
            if (uErr & X86_TRAP_PF_RW)
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eSVNotPresentWrite);
            else
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eSVNotPresentRead);
        }
        else if (uErr & X86_TRAP_PF_RW)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eSVWrite);
        else if (uErr & X86_TRAP_PF_ID)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eSNXE);
        else if (uErr & X86_TRAP_PF_RSVD)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eSVReserved);
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
        rc = PGM_BTH_NAME_PAE_PROT(Trap0eHandler)(pVM, uErr, pRegFrame, pvFault);
        break;
    case PGMMODE_AMD64:
        rc = PGM_BTH_NAME_AMD64_PROT(Trap0eHandler)(pVM, uErr, pRegFrame, pvFault);
        break;
    }
    if (rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE)
        rc = VINF_SUCCESS;
    STAM_STATS({ if (!pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution))
                    pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eMisc; });
    STAM_PROFILE_STOP_EX(&pVM->pgm.s.StatGCTrap0e, pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution), a);
    return rc;
}
