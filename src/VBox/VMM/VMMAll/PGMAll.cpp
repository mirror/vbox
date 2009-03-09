/* $Id$ */
/** @file
 * PGM - Page Manager and Monitor - All context code.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <VBox/cpum.h>
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <VBox/sup.h>
#include <VBox/mm.h>
#include <VBox/stam.h>
#include <VBox/csam.h>
#include <VBox/patm.h>
#include <VBox/trpm.h>
#include <VBox/rem.h>
#include <VBox/em.h>
#include <VBox/hwaccm.h>
#include <VBox/hwacc_vmx.h>
#include "PGMInternal.h"
#include <VBox/vm.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <VBox/log.h>
#include <VBox/param.h>
#include <VBox/err.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Stated structure for PGM_GST_NAME(HandlerVirtualUpdate) that's
 * passed to PGM_GST_NAME(VirtHandlerUpdateOne) during enumeration.
 */
typedef struct PGMHVUSTATE
{
    /** The VM handle. */
    PVM         pVM;
    /** The todo flags. */
    RTUINT      fTodo;
    /** The CR4 register value. */
    uint32_t    cr4;
} PGMHVUSTATE,  *PPGMHVUSTATE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
DECLINLINE(int) pgmShwGetLongModePDPtr(PVM pVM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e, PX86PDPT *ppPdpt, PX86PDPAE *ppPD);
DECLINLINE(int) pgmShwGetPAEPDPtr(PVM pVM, RTGCPTR GCPtr, PX86PDPT *ppPdpt, PX86PDPAE *ppPD);
DECLINLINE(int) pgmShwGetPaePoolPagePD(PPGM pPGM, RTGCPTR GCPtr, PPGMPOOLPAGE *ppShwPde);

/*
 * Shadow - 32-bit mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_32BIT
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_32BIT(name)
#include "PGMAllShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_REAL(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_32BIT_PD_PHYS
#include "PGMGstDefs.h"
#include "PGMAllGst.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_PROT(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_32BIT_PD_PHYS
#include "PGMGstDefs.h"
#include "PGMAllGst.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_32BIT_32BIT(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_32BIT_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_32BIT_PT_FOR_32BIT_4MB
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_32BIT_PD
#include "PGMGstDefs.h"
#include "PGMAllGst.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME


/*
 * Shadow - PAE mode
 */
#define PGM_SHW_TYPE                PGM_TYPE_PAE
#define PGM_SHW_NAME(name)          PGM_SHW_NAME_PAE(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#include "PGMAllShw.h"

/* Guest - real mode */
#define PGM_GST_TYPE                PGM_TYPE_REAL
#define PGM_GST_NAME(name)          PGM_GST_NAME_REAL(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_REAL(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT_PHYS
#include "PGMGstDefs.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - protected mode */
#define PGM_GST_TYPE                PGM_TYPE_PROT
#define PGM_GST_NAME(name)          PGM_GST_NAME_PROT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PROT(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PHYS
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT_PHYS
#include "PGMGstDefs.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

/* Guest - 32-bit mode */
#define PGM_GST_TYPE                PGM_TYPE_32BIT
#define PGM_GST_NAME(name)          PGM_GST_NAME_32BIT(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_32BIT(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_32BIT_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_32BIT_4MB
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT_FOR_32BIT
#include "PGMGstDefs.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME


/* Guest - PAE mode */
#define PGM_GST_TYPE                PGM_TYPE_PAE
#define PGM_GST_NAME(name)          PGM_GST_NAME_PAE(name)
#define PGM_BTH_NAME(name)          PGM_BTH_NAME_PAE_PAE(name)
#define BTH_PGMPOOLKIND_PT_FOR_PT   PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#define BTH_PGMPOOLKIND_PT_FOR_BIG  PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#define BTH_PGMPOOLKIND_ROOT        PGMPOOLKIND_PAE_PDPT
#include "PGMGstDefs.h"
#include "PGMAllGst.h"
#include "PGMAllBth.h"
#undef BTH_PGMPOOLKIND_PT_FOR_BIG
#undef BTH_PGMPOOLKIND_PT_FOR_PT
#undef BTH_PGMPOOLKIND_ROOT
#undef PGM_BTH_NAME
#undef PGM_GST_TYPE
#undef PGM_GST_NAME

#undef PGM_SHW_TYPE
#undef PGM_SHW_NAME


#ifndef IN_RC /* AMD64 implies VT-x/AMD-V */
/*
 * Shadow - AMD64 mode
 */
# define PGM_SHW_TYPE               PGM_TYPE_AMD64
# define PGM_SHW_NAME(name)         PGM_SHW_NAME_AMD64(name)
# include "PGMAllShw.h"

/* Guest - protected mode (only used for AMD-V nested paging in 64 bits mode) */
# define PGM_GST_TYPE               PGM_TYPE_PROT
# define PGM_GST_NAME(name)         PGM_GST_NAME_PROT(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_AMD64_PROT(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_PAE_PT_FOR_PHYS
# define BTH_PGMPOOLKIND_ROOT       PGMPOOLKIND_PAE_PD_PHYS
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef BTH_PGMPOOLKIND_ROOT
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

# ifdef VBOX_WITH_64_BITS_GUESTS
/* Guest - AMD64 mode */
#  define PGM_GST_TYPE              PGM_TYPE_AMD64
#  define PGM_GST_NAME(name)        PGM_GST_NAME_AMD64(name)
#  define PGM_BTH_NAME(name)        PGM_BTH_NAME_AMD64_AMD64(name)
#  define BTH_PGMPOOLKIND_PT_FOR_PT PGMPOOLKIND_PAE_PT_FOR_PAE_PT
#  define BTH_PGMPOOLKIND_PT_FOR_BIG PGMPOOLKIND_PAE_PT_FOR_PAE_2MB
#  define BTH_PGMPOOLKIND_ROOT      PGMPOOLKIND_64BIT_PML4
#  include "PGMGstDefs.h"
#  include "PGMAllGst.h"
#  include "PGMAllBth.h"
#  undef BTH_PGMPOOLKIND_PT_FOR_BIG
#  undef BTH_PGMPOOLKIND_PT_FOR_PT
#  undef BTH_PGMPOOLKIND_ROOT
#  undef PGM_BTH_NAME
#  undef PGM_GST_TYPE
#  undef PGM_GST_NAME
# endif /* VBOX_WITH_64_BITS_GUESTS */

# undef PGM_SHW_TYPE
# undef PGM_SHW_NAME


/*
 * Shadow - Nested paging mode
 */
# define PGM_SHW_TYPE               PGM_TYPE_NESTED
# define PGM_SHW_NAME(name)         PGM_SHW_NAME_NESTED(name)
# include "PGMAllShw.h"

/* Guest - real mode */
# define PGM_GST_TYPE               PGM_TYPE_REAL
# define PGM_GST_NAME(name)         PGM_GST_NAME_REAL(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_NESTED_REAL(name)
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

/* Guest - protected mode */
# define PGM_GST_TYPE               PGM_TYPE_PROT
# define PGM_GST_NAME(name)         PGM_GST_NAME_PROT(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_NESTED_PROT(name)
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

/* Guest - 32-bit mode */
# define PGM_GST_TYPE               PGM_TYPE_32BIT
# define PGM_GST_NAME(name)         PGM_GST_NAME_32BIT(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_NESTED_32BIT(name)
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

/* Guest - PAE mode */
# define PGM_GST_TYPE               PGM_TYPE_PAE
# define PGM_GST_NAME(name)         PGM_GST_NAME_PAE(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_NESTED_PAE(name)
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

# ifdef VBOX_WITH_64_BITS_GUESTS
/* Guest - AMD64 mode */
#  define PGM_GST_TYPE              PGM_TYPE_AMD64
#  define PGM_GST_NAME(name)        PGM_GST_NAME_AMD64(name)
#  define PGM_BTH_NAME(name)        PGM_BTH_NAME_NESTED_AMD64(name)
#  include "PGMGstDefs.h"
#  include "PGMAllBth.h"
#  undef PGM_BTH_NAME
#  undef PGM_GST_TYPE
#  undef PGM_GST_NAME
# endif /* VBOX_WITH_64_BITS_GUESTS */

# undef PGM_SHW_TYPE
# undef PGM_SHW_NAME


/*
 * Shadow - EPT
 */
# define PGM_SHW_TYPE               PGM_TYPE_EPT
# define PGM_SHW_NAME(name)         PGM_SHW_NAME_EPT(name)
# include "PGMAllShw.h"

/* Guest - real mode */
# define PGM_GST_TYPE               PGM_TYPE_REAL
# define PGM_GST_NAME(name)         PGM_GST_NAME_REAL(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_EPT_REAL(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_EPT_PT_FOR_PHYS
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

/* Guest - protected mode */
# define PGM_GST_TYPE               PGM_TYPE_PROT
# define PGM_GST_NAME(name)         PGM_GST_NAME_PROT(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_EPT_PROT(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_EPT_PT_FOR_PHYS
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

/* Guest - 32-bit mode */
# define PGM_GST_TYPE               PGM_TYPE_32BIT
# define PGM_GST_NAME(name)         PGM_GST_NAME_32BIT(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_EPT_32BIT(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_EPT_PT_FOR_PHYS
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

/* Guest - PAE mode */
# define PGM_GST_TYPE               PGM_TYPE_PAE
# define PGM_GST_NAME(name)         PGM_GST_NAME_PAE(name)
# define PGM_BTH_NAME(name)         PGM_BTH_NAME_EPT_PAE(name)
# define BTH_PGMPOOLKIND_PT_FOR_PT  PGMPOOLKIND_EPT_PT_FOR_PHYS
# include "PGMGstDefs.h"
# include "PGMAllBth.h"
# undef BTH_PGMPOOLKIND_PT_FOR_PT
# undef PGM_BTH_NAME
# undef PGM_GST_TYPE
# undef PGM_GST_NAME

# ifdef VBOX_WITH_64_BITS_GUESTS
/* Guest - AMD64 mode */
#  define PGM_GST_TYPE              PGM_TYPE_AMD64
#  define PGM_GST_NAME(name)        PGM_GST_NAME_AMD64(name)
#  define PGM_BTH_NAME(name)        PGM_BTH_NAME_EPT_AMD64(name)
#  define BTH_PGMPOOLKIND_PT_FOR_PT PGMPOOLKIND_EPT_PT_FOR_PHYS
#  include "PGMGstDefs.h"
#  include "PGMAllBth.h"
#  undef BTH_PGMPOOLKIND_PT_FOR_PT
#  undef PGM_BTH_NAME
#  undef PGM_GST_TYPE
#  undef PGM_GST_NAME
# endif /* VBOX_WITH_64_BITS_GUESTS */

# undef PGM_SHW_TYPE
# undef PGM_SHW_NAME

#endif /* !IN_RC */


#ifndef IN_RING3
/**
 * #PF Handler.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErr        The trap error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address.
 */
VMMDECL(int)     PGMTrap0eHandler(PVM pVM, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault)
{
    LogFlow(("PGMTrap0eHandler: uErr=%RGu pvFault=%RGv eip=%RGv\n", uErr, pvFault, (RTGCPTR)pRegFrame->rip));
    STAM_PROFILE_START(&pVM->pgm.s.StatRZTrap0e, a);
    STAM_STATS({ pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = NULL; } );


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
#endif /* VBOX_WITH_STATISTICS */

    /*
     * Call the worker.
     */
    int rc = PGM_BTH_PFN(Trap0eHandler, pVM)(pVM, uErr, pRegFrame, pvFault);
    if (rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE)
        rc = VINF_SUCCESS;
    STAM_STATS({ if (rc == VINF_EM_RAW_GUEST_TRAP) STAM_COUNTER_INC(&pVM->pgm.s.StatRZTrap0eGuestPF); });
    STAM_STATS({ if (!pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution))
                    pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatRZTrap0eTime2Misc; });
    STAM_PROFILE_STOP_EX(&pVM->pgm.s.StatRZTrap0e, pVM->pgm.s.CTX_SUFF(pStatTrap0eAttribution), a);
    return rc;
}
#endif /* !IN_RING3 */


/**
 * Prefetch a page
 *
 * Typically used to sync commonly used pages before entering raw mode
 * after a CR3 reload.
 *
 * @returns VBox status code suitable for scheduling.
 * @retval  VINF_SUCCESS on success.
 * @retval  VINF_PGM_SYNC_CR3 if we're out of shadow pages or something like that.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 */
VMMDECL(int) PGMPrefetchPage(PVM pVM, RTGCPTR GCPtrPage)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,Prefetch), a);
    int rc = PGM_BTH_PFN(PrefetchPage, pVM)(pVM, GCPtrPage);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,Prefetch), a);
    AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 || RT_FAILURE(rc), ("rc=%Rrc\n", rc));
    return rc;
}


/**
 * Gets the mapping corresponding to the specified address (if any).
 *
 * @returns Pointer to the mapping.
 * @returns NULL if not
 *
 * @param   pVM         The virtual machine.
 * @param   GCPtr       The guest context pointer.
 */
PPGMMAPPING pgmGetMapping(PVM pVM, RTGCPTR GCPtr)
{
    PPGMMAPPING pMapping = pVM->pgm.s.CTX_SUFF(pMappings);
    while (pMapping)
    {
        if ((uintptr_t)GCPtr < (uintptr_t)pMapping->GCPtr)
            break;
        if ((uintptr_t)GCPtr - (uintptr_t)pMapping->GCPtr < pMapping->cb)
            return pMapping;
        pMapping = pMapping->CTX_SUFF(pNext);
    }
    return NULL;
}


/**
 * Verifies a range of pages for read or write access
 *
 * Only checks the guest's page tables
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   Addr        Guest virtual address to check
 * @param   cbSize      Access size
 * @param   fAccess     Access type (r/w, user/supervisor (X86_PTE_*))
 * @remarks Current not in use.
 */
VMMDECL(int) PGMIsValidAccess(PVM pVM, RTGCPTR Addr, uint32_t cbSize, uint32_t fAccess)
{
    /*
     * Validate input.
     */
    if (fAccess & ~(X86_PTE_US | X86_PTE_RW))
    {
        AssertMsgFailed(("PGMIsValidAccess: invalid access type %08x\n", fAccess));
        return VERR_INVALID_PARAMETER;
    }

    uint64_t fPage;
    int rc = PGMGstGetPage(pVM, (RTGCPTR)Addr, &fPage, NULL);
    if (RT_FAILURE(rc))
    {
        Log(("PGMIsValidAccess: access violation for %RGv rc=%d\n", Addr, rc));
        return VINF_EM_RAW_GUEST_TRAP;
    }

    /*
     * Check if the access would cause a page fault
     *
     * Note that hypervisor page directories are not present in the guest's tables, so this check
     * is sufficient.
     */
    bool fWrite = !!(fAccess & X86_PTE_RW);
    bool fUser  = !!(fAccess & X86_PTE_US);
    if (  !(fPage & X86_PTE_P)
        || (fWrite && !(fPage & X86_PTE_RW))
        || (fUser  && !(fPage & X86_PTE_US)) )
    {
        Log(("PGMIsValidAccess: access violation for %RGv attr %#llx vs %d:%d\n", Addr, fPage, fWrite, fUser));
        return VINF_EM_RAW_GUEST_TRAP;
    }
    if (    RT_SUCCESS(rc)
        &&  PAGE_ADDRESS(Addr) != PAGE_ADDRESS(Addr + cbSize))
        return PGMIsValidAccess(pVM, Addr + PAGE_SIZE, (cbSize > PAGE_SIZE) ? cbSize - PAGE_SIZE : 1, fAccess);
    return rc;
}


/**
 * Verifies a range of pages for read or write access
 *
 * Supports handling of pages marked for dirty bit tracking and CSAM
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   Addr        Guest virtual address to check
 * @param   cbSize      Access size
 * @param   fAccess     Access type (r/w, user/supervisor (X86_PTE_*))
 */
VMMDECL(int) PGMVerifyAccess(PVM pVM, RTGCPTR Addr, uint32_t cbSize, uint32_t fAccess)
{
    AssertMsg(!(fAccess & ~(X86_PTE_US | X86_PTE_RW)), ("PGMVerifyAccess: invalid access type %08x\n", fAccess));

    /*
     * Get going.
     */
    uint64_t fPageGst;
    int rc = PGMGstGetPage(pVM, (RTGCPTR)Addr, &fPageGst, NULL);
    if (RT_FAILURE(rc))
    {
        Log(("PGMVerifyAccess: access violation for %RGv rc=%d\n", Addr, rc));
        return VINF_EM_RAW_GUEST_TRAP;
    }

    /*
     * Check if the access would cause a page fault
     *
     * Note that hypervisor page directories are not present in the guest's tables, so this check
     * is sufficient.
     */
    const bool fWrite = !!(fAccess & X86_PTE_RW);
    const bool fUser  = !!(fAccess & X86_PTE_US);
    if (  !(fPageGst & X86_PTE_P)
        || (fWrite  && !(fPageGst & X86_PTE_RW))
        || (fUser   && !(fPageGst & X86_PTE_US)) )
    {
        Log(("PGMVerifyAccess: access violation for %RGv attr %#llx vs %d:%d\n", Addr, fPageGst, fWrite, fUser));
        return VINF_EM_RAW_GUEST_TRAP;
    }

    if (!HWACCMIsNestedPagingActive(pVM))
    {
        /*
         * Next step is to verify if we protected this page for dirty bit tracking or for CSAM scanning
         */
        rc = PGMShwGetPage(pVM, (RTGCPTR)Addr, NULL, NULL);
        if (    rc == VERR_PAGE_NOT_PRESENT
            ||  rc == VERR_PAGE_TABLE_NOT_PRESENT)
        {
            /*
            * Page is not present in our page tables.
            * Try to sync it!
            */
            Assert(X86_TRAP_PF_RW == X86_PTE_RW && X86_TRAP_PF_US == X86_PTE_US);
            uint32_t uErr = fAccess & (X86_TRAP_PF_RW | X86_TRAP_PF_US);
            rc = PGM_BTH_PFN(VerifyAccessSyncPage, pVM)(pVM, Addr, fPageGst, uErr);
            if (rc != VINF_SUCCESS)
                return rc;
        }
        else
            AssertMsg(rc == VINF_SUCCESS, ("PGMShwGetPage %RGv failed with %Rrc\n", Addr, rc));
    }

#if 0 /* def VBOX_STRICT; triggers too often now */
    /*
     * This check is a bit paranoid, but useful.
     */
    /** @note this will assert when writing to monitored pages (a bit annoying actually) */
    uint64_t fPageShw;
    rc = PGMShwGetPage(pVM, (RTGCPTR)Addr, &fPageShw, NULL);
    if (    (rc == VERR_PAGE_NOT_PRESENT || RT_FAILURE(rc))
        || (fWrite && !(fPageShw & X86_PTE_RW))
        || (fUser  && !(fPageShw & X86_PTE_US)) )
    {
        AssertMsgFailed(("Unexpected access violation for %RGv! rc=%Rrc write=%d user=%d\n",
                         Addr, rc, fWrite && !(fPageShw & X86_PTE_RW), fUser && !(fPageShw & X86_PTE_US)));
        return VINF_EM_RAW_GUEST_TRAP;
    }
#endif

    if (    RT_SUCCESS(rc)
        &&  (   PAGE_ADDRESS(Addr) != PAGE_ADDRESS(Addr + cbSize - 1)
             || Addr + cbSize < Addr))
    {
        /* Don't recursively call PGMVerifyAccess as we might run out of stack. */
        for (;;)
        {
            Addr += PAGE_SIZE;
            if (cbSize > PAGE_SIZE)
                cbSize -= PAGE_SIZE;
            else
                cbSize = 1;
            rc = PGMVerifyAccess(pVM, Addr, 1, fAccess);
            if (rc != VINF_SUCCESS)
                break;
            if (PAGE_ADDRESS(Addr) == PAGE_ADDRESS(Addr + cbSize - 1))
                break;
        }
    }
    return rc;
}


/**
 * Emulation of the invlpg instruction (HC only actually).
 *
 * @returns VBox status code, special care required.
 * @retval  VINF_PGM_SYNC_CR3 - handled.
 * @retval  VINF_EM_RAW_EMULATE_INSTR - not handled (RC only).
 * @retval  VERR_REM_FLUSHED_PAGES_OVERFLOW - not handled.
 *
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 *
 * @remark  ASSUMES the page table entry or page directory is valid. Fairly
 *          safe, but there could be edge cases!
 *
 * @todo    Flush page or page directory only if necessary!
 */
VMMDECL(int) PGMInvalidatePage(PVM pVM, RTGCPTR GCPtrPage)
{
    int rc;
    Log3(("PGMInvalidatePage: GCPtrPage=%RGv\n", GCPtrPage));

#ifndef IN_RING3
    /*
     * Notify the recompiler so it can record this instruction.
     * Failure happens when it's out of space. We'll return to HC in that case.
     */
    rc = REMNotifyInvalidatePage(pVM, GCPtrPage);
    if (rc != VINF_SUCCESS)
        return rc;
#endif /* !IN_RING3 */


#ifdef IN_RC
    /*
     * Check for conflicts and pending CR3 monitoring updates.
     */
    if (!pVM->pgm.s.fMappingsFixed)
    {
        if (    pgmGetMapping(pVM, GCPtrPage)
            &&  PGMGstGetPage(pVM, GCPtrPage, NULL, NULL) != VERR_PAGE_TABLE_NOT_PRESENT)
        {
            LogFlow(("PGMGCInvalidatePage: Conflict!\n"));
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            STAM_COUNTER_INC(&pVM->pgm.s.StatRCInvlPgConflict);
            return VINF_PGM_SYNC_CR3;
        }

        if (pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3)
        {
            LogFlow(("PGMGCInvalidatePage: PGM_SYNC_MONITOR_CR3 -> reinterpret instruction in R3\n"));
            STAM_COUNTER_INC(&pVM->pgm.s.StatRCInvlPgSyncMonCR3);
            return VINF_EM_RAW_EMULATE_INSTR;
        }
    }
#endif /* IN_RC */

    /*
     * Call paging mode specific worker.
     */
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,InvalidatePage), a);
    rc = PGM_BTH_PFN(InvalidatePage, pVM)(pVM, GCPtrPage);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,InvalidatePage), a);

#ifdef IN_RING3
    /*
     * Check if we have a pending update of the CR3 monitoring.
     */
    if (    RT_SUCCESS(rc)
        &&  (pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3))
    {
        pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_MONITOR_CR3;
        Assert(!pVM->pgm.s.fMappingsFixed);
    }

    /*
     * Inform CSAM about the flush
     *
     * Note: This is to check if monitored pages have been changed; when we implement
     *       callbacks for virtual handlers, this is no longer required.
     */
    CSAMR3FlushPage(pVM, GCPtrPage);
#endif /* IN_RING3 */
    return rc;
}


/**
 * Executes an instruction using the interpreter.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM handle.
 * @param   pRegFrame   Register frame.
 * @param   pvFault     Fault address.
 */
VMMDECL(int) PGMInterpretInstruction(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault)
{
    uint32_t cb;
    int rc = EMInterpretInstruction(pVM, pRegFrame, pvFault, &cb);
    if (rc == VERR_EM_INTERPRETER)
        rc = VINF_EM_RAW_EMULATE_INSTR;
    if (rc != VINF_SUCCESS)
        Log(("PGMInterpretInstruction: returns %Rrc (pvFault=%RGv)\n", rc, pvFault));
    return rc;
}


/**
 * Gets effective page information (from the VMM page directory).
 *
 * @returns VBox status.
 * @param   pVM         VM Handle.
 * @param   GCPtr       Guest Context virtual address of the page.
 * @param   pfFlags     Where to store the flags. These are X86_PTE_*.
 * @param   pHCPhys     Where to store the HC physical address of the page.
 *                      This is page aligned.
 * @remark  You should use PGMMapGetPage() for pages in a mapping.
 */
VMMDECL(int) PGMShwGetPage(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys)
{
    return PGM_SHW_PFN(GetPage,pVM)(pVM, GCPtr, pfFlags, pHCPhys);
}


/**
 * Sets (replaces) the page flags for a range of pages in the shadow context.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address of the first page.
 * @param   cb          The size of the range in bytes.
 * @param   fFlags      Page flags X86_PTE_*, excluding the page mask of course.
 * @remark  You must use PGMMapSetPage() for pages in a mapping.
 */
VMMDECL(int) PGMShwSetPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags)
{
    return PGMShwModifyPage(pVM, GCPtr, cb, fFlags, 0);
}


/**
 * Modify page flags for a range of pages in the shadow context.
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*.
 *                      Be very CAREFUL when ~'ing constants which could be 32-bit!
 * @remark  You must use PGMMapModifyPage() for pages in a mapping.
 */
VMMDECL(int) PGMShwModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
    AssertMsg(!(fFlags & X86_PTE_PAE_PG_MASK), ("fFlags=%#llx\n", fFlags));
    Assert(cb);

    /*
     * Align the input.
     */
    cb     += GCPtr & PAGE_OFFSET_MASK;
    cb      = RT_ALIGN_Z(cb, PAGE_SIZE);
    GCPtr   = (GCPtr & PAGE_BASE_GC_MASK); /** @todo this ain't necessary, right... */

    /*
     * Call worker.
     */
    return PGM_SHW_PFN(ModifyPage, pVM)(pVM, GCPtr, cb, fFlags, fMask);
}


/**
 * Gets the SHADOW page directory pointer for the specified address.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address.
 * @param   ppPdpt      Receives address of pdpt
 * @param   ppPD        Receives address of page directory
 * @remarks Unused.
 */
DECLINLINE(int) pgmShwGetPAEPDPtr(PVM pVM, RTGCPTR GCPtr, PX86PDPT *ppPdpt, PX86PDPAE *ppPD)
{
    PPGM           pPGM   = &pVM->pgm.s;
    PPGMPOOL       pPool  = pPGM->CTX_SUFF(pPool);
    PPGMPOOLPAGE   pShwPage;

    Assert(!HWACCMIsNestedPagingActive(pVM));

    const unsigned iPdPt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    PX86PDPT  pPdpt = pgmShwGetPaePDPTPtr(&pVM->pgm.s);
    PX86PDPE  pPdpe = &pPdpt->a[iPdPt];

    *ppPdpt = pPdpt;
    if (!pPdpe->n.u1Present)
        return VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT;

    Assert(pPdpe->u & X86_PDPE_PG_MASK);
    pShwPage = pgmPoolGetPage(pPool, pPdpe->u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPage, VERR_INTERNAL_ERROR);

    *ppPD = (PX86PDPAE)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    return VINF_SUCCESS;
}

/**
 * Gets the shadow page directory for the specified address, PAE.
 *
 * @returns Pointer to the shadow PD.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address.
 * @param   pGstPdpe    Guest PDPT entry
 * @param   ppPD        Receives address of page directory
 */
int pgmShwSyncPaePDPtr(PVM pVM, RTGCPTR GCPtr, PX86PDPE pGstPdpe, PX86PDPAE *ppPD)
{
    const unsigned iPdPt    = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    PX86PDPT       pPdpt    = pgmShwGetPaePDPTPtr(&pVM->pgm.s);
    PX86PDPE       pPdpe    = &pPdpt->a[iPdPt];
    PPGMPOOL       pPool    = pVM->pgm.s.CTX_SUFF(pPool);
    PPGMPOOLPAGE   pShwPage;
    int            rc;

    /* Allocate page directory if not present. */
    if (    !pPdpe->n.u1Present
        &&  !(pPdpe->u & X86_PDPE_PG_MASK))
    {
        bool        fNestedPaging = HWACCMIsNestedPagingActive(pVM);
        bool        fPaging       = !!(CPUMGetGuestCR0(pVM) & X86_CR0_PG);
        RTGCPTR64   GCPdPt;
        PGMPOOLKIND enmKind;

# if defined(IN_RC)
        /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
        PGMDynLockHCPage(pVM, (uint8_t *)pPdpe);
# endif

        if (fNestedPaging || !fPaging)
        {
            /* AMD-V nested paging or real/protected mode without paging */
            GCPdPt  = (RTGCPTR64)iPdPt << X86_PDPT_SHIFT;
            enmKind = PGMPOOLKIND_PAE_PD_PHYS;
        }
        else
        {
            Assert(pGstPdpe);

            if (CPUMGetGuestCR4(pVM) & X86_CR4_PAE)
            {
                if (!pGstPdpe->n.u1Present)
                {
                    /* PD not present; guest must reload CR3 to change it.
                     * No need to monitor anything in this case.
                     */
                    Assert(!HWACCMIsEnabled(pVM));

                    GCPdPt  = pGstPdpe->u & X86_PDPE_PG_MASK;
                    enmKind = PGMPOOLKIND_PAE_PD_PHYS;
                    pGstPdpe->n.u1Present = 1;
                }
                else
                {
                    GCPdPt  = pGstPdpe->u & X86_PDPE_PG_MASK;
                    enmKind = PGMPOOLKIND_PAE_PD_FOR_PAE_PD;
                }
            }
            else
            {
                GCPdPt  = CPUMGetGuestCR3(pVM);
                enmKind = (PGMPOOLKIND)(PGMPOOLKIND_PAE_PD0_FOR_32BIT_PD + iPdPt);
            }
        }

        /* Create a reference back to the PDPT by using the index in its shadow page. */
        rc = pgmPoolAlloc(pVM, GCPdPt, enmKind, pVM->pgm.s.CTX_SUFF(pShwPageCR3)->idx, iPdPt, &pShwPage);
        if (rc == VERR_PGM_POOL_FLUSHED)
        {
            Log(("pgmShwSyncPaePDPtr: PGM pool flushed -> signal sync cr3\n"));
            Assert(pVM->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL);
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
# if defined(IN_RC)
            PGMDynUnlockHCPage(pVM, (uint8_t *)pPdpe);
# endif
            return VINF_PGM_SYNC_CR3;
        }
        AssertRCReturn(rc, rc);

        /* The PD was cached or created; hook it up now. */
        pPdpe->u |= pShwPage->Core.Key
                 | (pGstPdpe->u & ~(X86_PDPE_PG_MASK | X86_PDPE_AVL_MASK | X86_PDPE_PCD | X86_PDPE_PWT));

# if defined(IN_RC)
        /* In 32 bits PAE mode we *must* invalidate the TLB when changing a PDPT entry; the CPU fetches them only during cr3 load, so any
         * non-present PDPT will continue to cause page faults.
         */
        ASMReloadCR3();
        PGMDynUnlockHCPage(pVM, (uint8_t *)pPdpe);
# endif
    }
    else
    {
        pShwPage = pgmPoolGetPage(pPool, pPdpe->u & X86_PDPE_PG_MASK);
        AssertReturn(pShwPage, VERR_INTERNAL_ERROR);

        Assert((pPdpe->u & X86_PDPE_PG_MASK) == pShwPage->Core.Key);
    }
    *ppPD = (PX86PDPAE)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    return VINF_SUCCESS;
}


/**
 * Gets the pointer to the shadow page directory entry for an address, PAE.
 *
 * @returns Pointer to the PDE.
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   GCPtr       The address.
 * @param   ppShwPde    Receives the address of the pgm pool page for the shadow page directory
 */
DECLINLINE(int) pgmShwGetPaePoolPagePD(PPGM pPGM, RTGCPTR GCPtr, PPGMPOOLPAGE *ppShwPde)
{
    const unsigned  iPdPt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_PAE;
    PX86PDPT        pPdpt = pgmShwGetPaePDPTPtr(pPGM);
    AssertReturn(pPdpt, VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT);    /* can't happen */
    if (!pPdpt->a[iPdPt].n.u1Present)
    {
        LogFlow(("pgmShwGetPaePoolPagePD: PD %d not present (%RX64)\n", iPdPt, pPdpt->a[iPdPt].u));
        return VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT;
    }

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE pShwPde = pgmPoolGetPageByHCPhys(PGM2VM(pPGM), pPdpt->a[iPdPt].u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPde, VERR_INTERNAL_ERROR);

    *ppShwPde = pShwPde;
    return VINF_SUCCESS;
}

#ifndef IN_RC

/**
 * Syncs the SHADOW page directory pointer for the specified address.
 *
 * Allocates backing pages in case the PDPT or PML4 entry is missing.
 *
 * The caller is responsible for making sure the guest has a valid PD before
 * calling this function.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address.
 * @param   pGstPml4e   Guest PML4 entry
 * @param   pGstPdpe    Guest PDPT entry
 * @param   ppPD        Receives address of page directory
 */
int pgmShwSyncLongModePDPtr(PVM pVM, RTGCPTR64 GCPtr, PX86PML4E pGstPml4e, PX86PDPE pGstPdpe, PX86PDPAE *ppPD)
{
    PPGM           pPGM          = &pVM->pgm.s;
    PPGMPOOL       pPool         = pPGM->CTX_SUFF(pPool);
    const unsigned iPml4         = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PX86PML4E      pPml4e        = pgmShwGetLongModePML4EPtr(pPGM, iPml4);
    bool           fNestedPaging = HWACCMIsNestedPagingActive(pVM);
    bool           fPaging      = !!(CPUMGetGuestCR0(pVM) & X86_CR0_PG);
    PPGMPOOLPAGE   pShwPage;
    int            rc;

    /* Allocate page directory pointer table if not present. */
    if (    !pPml4e->n.u1Present
        &&  !(pPml4e->u & X86_PML4E_PG_MASK))
    {
        RTGCPTR64   GCPml4;
        PGMPOOLKIND enmKind;

        Assert(pVM->pgm.s.CTX_SUFF(pShwPageCR3));

        if (fNestedPaging || !fPaging)
        {
            /* AMD-V nested paging or real/protected mode without paging */
            GCPml4  = (RTGCPTR64)iPml4 << X86_PML4_SHIFT;
            enmKind = PGMPOOLKIND_64BIT_PDPT_FOR_PHYS;
        }
        else
        {
            Assert(pGstPml4e && pGstPdpe);

            GCPml4  = pGstPml4e->u & X86_PML4E_PG_MASK;
            enmKind = PGMPOOLKIND_64BIT_PDPT_FOR_64BIT_PDPT;
        }

        /* Create a reference back to the PDPT by using the index in its shadow page. */
        rc = pgmPoolAlloc(pVM, GCPml4, enmKind, pVM->pgm.s.CTX_SUFF(pShwPageCR3)->idx, iPml4, &pShwPage);
        if (rc == VERR_PGM_POOL_FLUSHED)
        {
            Log(("PGMShwSyncLongModePDPtr: PGM pool flushed (1) -> signal sync cr3\n"));
            Assert(pVM->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL);
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            return VINF_PGM_SYNC_CR3;
        }
        AssertRCReturn(rc, rc);
    }
    else
    {
        pShwPage = pgmPoolGetPage(pPool, pPml4e->u & X86_PML4E_PG_MASK);
        AssertReturn(pShwPage, VERR_INTERNAL_ERROR);
    }
    /* The PDPT was cached or created; hook it up now. */
    pPml4e->u |= pShwPage->Core.Key
              | (pGstPml4e->u & ~(X86_PML4E_PG_MASK | X86_PML4E_AVL_MASK | X86_PML4E_PCD | X86_PML4E_PWT));

    const unsigned iPdPt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
    PX86PDPT  pPdpt = (PX86PDPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    PX86PDPE  pPdpe = &pPdpt->a[iPdPt];

    /* Allocate page directory if not present. */
    if (    !pPdpe->n.u1Present
        &&  !(pPdpe->u & X86_PDPE_PG_MASK))
    {
        RTGCPTR64   GCPdPt;
        PGMPOOLKIND enmKind;

        if (fNestedPaging || !fPaging)
        {
            /* AMD-V nested paging or real/protected mode without paging */
            GCPdPt  = (RTGCPTR64)iPdPt << X86_PDPT_SHIFT;
            enmKind = PGMPOOLKIND_64BIT_PD_FOR_PHYS;
        }
        else
        {
            Assert(pGstPdpe);

            GCPdPt  = pGstPdpe->u & X86_PDPE_PG_MASK;
            enmKind = PGMPOOLKIND_64BIT_PD_FOR_64BIT_PD;
        }

        /* Create a reference back to the PDPT by using the index in its shadow page. */
        rc = pgmPoolAlloc(pVM, GCPdPt, enmKind, pShwPage->idx, iPdPt, &pShwPage);
        if (rc == VERR_PGM_POOL_FLUSHED)
        {
            Log(("PGMShwSyncLongModePDPtr: PGM pool flushed (2) -> signal sync cr3\n"));
            Assert(pVM->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL);
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            return VINF_PGM_SYNC_CR3;
        }
        AssertRCReturn(rc, rc);
    }
    else
    {
        pShwPage = pgmPoolGetPage(pPool, pPdpe->u & X86_PDPE_PG_MASK);
        AssertReturn(pShwPage, VERR_INTERNAL_ERROR);
    }
    /* The PD was cached or created; hook it up now. */
    pPdpe->u |= pShwPage->Core.Key
             | (pGstPdpe->u & ~(X86_PDPE_PG_MASK | X86_PDPE_AVL_MASK | X86_PDPE_PCD | X86_PDPE_PWT));

    *ppPD = (PX86PDPAE)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    return VINF_SUCCESS;
}


/**
 * Gets the SHADOW page directory pointer for the specified address (long mode).
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address.
 * @param   ppPdpt      Receives address of pdpt
 * @param   ppPD        Receives address of page directory
 */
DECLINLINE(int) pgmShwGetLongModePDPtr(PVM pVM, RTGCPTR64 GCPtr, PX86PML4E *ppPml4e, PX86PDPT *ppPdpt, PX86PDPAE *ppPD)
{
    PPGM            pPGM = &pVM->pgm.s;
    const unsigned  iPml4 = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    PCX86PML4E      pPml4e = pgmShwGetLongModePML4EPtr(pPGM, iPml4);
    AssertReturn(pPml4e, VERR_INTERNAL_ERROR);
    if (ppPml4e)
        *ppPml4e = (PX86PML4E)pPml4e;
    if (!pPml4e->n.u1Present)
        return VERR_PAGE_MAP_LEVEL4_NOT_PRESENT;

    PPGMPOOL        pPool = pPGM->CTX_SUFF(pPool);
    PPGMPOOLPAGE    pShwPage = pgmPoolGetPage(pPool, pPml4e->u & X86_PML4E_PG_MASK);
    AssertReturn(pShwPage, VERR_INTERNAL_ERROR);

    const unsigned  iPdPt = (GCPtr >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
    PCX86PDPT       pPdpt = *ppPdpt = (PX86PDPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    if (!pPdpt->a[iPdPt].n.u1Present)
        return VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT;

    pShwPage = pgmPoolGetPage(pPool, pPdpt->a[iPdPt].u & X86_PDPE_PG_MASK);
    AssertReturn(pShwPage, VERR_INTERNAL_ERROR);

    *ppPD = (PX86PDPAE)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    return VINF_SUCCESS;
}


/**
 * Syncs the SHADOW EPT page directory pointer for the specified address. Allocates
 * backing pages in case the PDPT or PML4 entry is missing.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address.
 * @param   ppPdpt      Receives address of pdpt
 * @param   ppPD        Receives address of page directory
 */
int pgmShwGetEPTPDPtr(PVM pVM, RTGCPTR64 GCPtr, PEPTPDPT *ppPdpt, PEPTPD *ppPD)
{
    PPGM           pPGM  = &pVM->pgm.s;
    const unsigned iPml4 = (GCPtr >> EPT_PML4_SHIFT) & EPT_PML4_MASK;
    PPGMPOOL       pPool = pPGM->CTX_SUFF(pPool);
    PEPTPML4       pPml4;
    PEPTPML4E      pPml4e;
    PPGMPOOLPAGE   pShwPage;
    int            rc;

    Assert(HWACCMIsNestedPagingActive(pVM));

    pPml4 = (PEPTPML4)PGMPOOL_PAGE_2_PTR_BY_PGM(pPGM, pPGM->CTX_SUFF(pShwPageCR3));
    Assert(pPml4);

    /* Allocate page directory pointer table if not present. */
    pPml4e = &pPml4->a[iPml4];
    if (    !pPml4e->n.u1Present
        &&  !(pPml4e->u & EPT_PML4E_PG_MASK))
    {
        Assert(!(pPml4e->u & EPT_PML4E_PG_MASK));
        RTGCPTR64 GCPml4 = (RTGCPTR64)iPml4 << EPT_PML4_SHIFT;

        rc = pgmPoolAlloc(pVM, GCPml4, PGMPOOLKIND_EPT_PDPT_FOR_PHYS, PGMPOOL_IDX_NESTED_ROOT, iPml4, &pShwPage);
        if (rc == VERR_PGM_POOL_FLUSHED)
        {
            Log(("PGMShwSyncEPTPDPtr: PGM pool flushed (1) -> signal sync cr3\n"));
            Assert(pVM->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL);
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            return VINF_PGM_SYNC_CR3;
        }
        AssertRCReturn(rc, rc);
    }
    else
    {
        pShwPage = pgmPoolGetPage(pPool, pPml4e->u & EPT_PML4E_PG_MASK);
        AssertReturn(pShwPage, VERR_INTERNAL_ERROR);
    }
    /* The PDPT was cached or created; hook it up now and fill with the default value. */
    pPml4e->u           = pShwPage->Core.Key;
    pPml4e->n.u1Present = 1;
    pPml4e->n.u1Write   = 1;
    pPml4e->n.u1Execute = 1;

    const unsigned iPdPt = (GCPtr >> EPT_PDPT_SHIFT) & EPT_PDPT_MASK;
    PEPTPDPT  pPdpt = (PEPTPDPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    PEPTPDPTE pPdpe = &pPdpt->a[iPdPt];

    if (ppPdpt)
        *ppPdpt = pPdpt;

    /* Allocate page directory if not present. */
    if (    !pPdpe->n.u1Present
        &&  !(pPdpe->u & EPT_PDPTE_PG_MASK))
    {
        RTGCPTR64 GCPdPt = (RTGCPTR64)iPdPt << EPT_PDPT_SHIFT;

        rc = pgmPoolAlloc(pVM, GCPdPt, PGMPOOLKIND_64BIT_PD_FOR_PHYS, pShwPage->idx, iPdPt, &pShwPage);
        if (rc == VERR_PGM_POOL_FLUSHED)
        {
            Log(("PGMShwSyncEPTPDPtr: PGM pool flushed (2) -> signal sync cr3\n"));
            Assert(pVM->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL);
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
            return VINF_PGM_SYNC_CR3;
        }
        AssertRCReturn(rc, rc);
    }
    else
    {
        pShwPage = pgmPoolGetPage(pPool, pPdpe->u & EPT_PDPTE_PG_MASK);
        AssertReturn(pShwPage, VERR_INTERNAL_ERROR);
    }
    /* The PD was cached or created; hook it up now and fill with the default value. */
    pPdpe->u            = pShwPage->Core.Key;
    pPdpe->n.u1Present  = 1;
    pPdpe->n.u1Write    = 1;
    pPdpe->n.u1Execute  = 1;

    *ppPD = (PEPTPD)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    return VINF_SUCCESS;
}

#endif /* IN_RC */

/**
 * Gets effective Guest OS page information.
 *
 * When GCPtr is in a big page, the function will return as if it was a normal
 * 4KB page. If the need for distinguishing between big and normal page becomes
 * necessary at a later point, a PGMGstGetPage() will be created for that
 * purpose.
 *
 * @returns VBox status.
 * @param   pVM         VM Handle.
 * @param   GCPtr       Guest Context virtual address of the page.
 * @param   pfFlags     Where to store the flags. These are X86_PTE_*, even for big pages.
 * @param   pGCPhys     Where to store the GC physical address of the page.
 *                      This is page aligned. The fact that the
 */
VMMDECL(int) PGMGstGetPage(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys)
{
    return PGM_GST_PFN(GetPage,pVM)(pVM, GCPtr, pfFlags, pGCPhys);
}


/**
 * Checks if the page is present.
 *
 * @returns true if the page is present.
 * @returns false if the page is not present.
 * @param   pVM         The VM handle.
 * @param   GCPtr       Address within the page.
 */
VMMDECL(bool) PGMGstIsPagePresent(PVM pVM, RTGCPTR GCPtr)
{
    int rc = PGMGstGetPage(pVM, GCPtr, NULL, NULL);
    return RT_SUCCESS(rc);
}


/**
 * Sets (replaces) the page flags for a range of pages in the guest's tables.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPtr       The address of the first page.
 * @param   cb          The size of the range in bytes.
 * @param   fFlags      Page flags X86_PTE_*, excluding the page mask of course.
 */
VMMDECL(int)  PGMGstSetPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags)
{
    return PGMGstModifyPage(pVM, GCPtr, cb, fFlags, 0);
}


/**
 * Modify page flags for a range of pages in the guest's tables
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range.
 * @param   cb          Size (in bytes) of the range to apply the modification to.
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*, excluding the page mask of course.
 *                      Be very CAREFUL when ~'ing constants which could be 32-bit!
 */
VMMDECL(int)  PGMGstModifyPage(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,GstModifyPage), a);

    /*
     * Validate input.
     */
    AssertMsg(!(fFlags & X86_PTE_PAE_PG_MASK), ("fFlags=%#llx\n", fFlags));
    Assert(cb);

    LogFlow(("PGMGstModifyPage %RGv %d bytes fFlags=%08llx fMask=%08llx\n", GCPtr, cb, fFlags, fMask));

    /*
     * Adjust input.
     */
    cb     += GCPtr & PAGE_OFFSET_MASK;
    cb      = RT_ALIGN_Z(cb, PAGE_SIZE);
    GCPtr   = (GCPtr & PAGE_BASE_GC_MASK);

    /*
     * Call worker.
     */
    int rc = PGM_GST_PFN(ModifyPage, pVM)(pVM, GCPtr, cb, fFlags, fMask);

    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,GstModifyPage), a);
    return rc;
}


/**
 * Gets the specified page directory pointer table entry.
 *
 * @returns PDP entry
 * @param   pPGM        Pointer to the PGM instance data.
 * @param   iPdpt       PDPT index
 */
VMMDECL(X86PDPE) PGMGstGetPaePDPtr(PVM pVM, unsigned iPdpt)
{
    Assert(iPdpt <= 3);
    return pgmGstGetPaePDPTPtr(&pVM->pgm.s)->a[iPdpt & 3];
}


/**
 * Gets the current CR3 register value for the shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetHyperCR3(PVM pVM)
{
    Assert(pVM->pgm.s.CTX_SUFF(pShwPageCR3));
    return pVM->pgm.s.CTX_SUFF(pShwPageCR3)->Core.Key;
}


/**
 * Gets the current CR3 register value for the nested memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetNestedCR3(PVM pVM, PGMMODE enmShadowMode)
{
    Assert(pVM->pgm.s.CTX_SUFF(pShwPageCR3));
    return pVM->pgm.s.CTX_SUFF(pShwPageCR3)->Core.Key;
}


/**
 * Gets the CR3 register value for the 32-Bit shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetHyper32BitCR3(PVM pVM)
{
    Assert(pVM->pgm.s.CTX_SUFF(pShwPageCR3));
    return pVM->pgm.s.CTX_SUFF(pShwPageCR3)->Core.Key;
}


/**
 * Gets the CR3 register value for the PAE shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetHyperPaeCR3(PVM pVM)
{
    Assert(pVM->pgm.s.CTX_SUFF(pShwPageCR3));
    return pVM->pgm.s.CTX_SUFF(pShwPageCR3)->Core.Key;
}


/**
 * Gets the CR3 register value for the AMD64 shadow memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetHyperAmd64CR3(PVM pVM)
{
    Assert(pVM->pgm.s.CTX_SUFF(pShwPageCR3));
    return pVM->pgm.s.CTX_SUFF(pShwPageCR3)->Core.Key;
}


/**
 * Gets the current CR3 register value for the HC intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetInterHCCR3(PVM pVM)
{
    switch (pVM->pgm.s.enmHostMode)
    {
        case SUPPAGINGMODE_32_BIT:
        case SUPPAGINGMODE_32_BIT_GLOBAL:
            return pVM->pgm.s.HCPhysInterPD;

        case SUPPAGINGMODE_PAE:
        case SUPPAGINGMODE_PAE_GLOBAL:
        case SUPPAGINGMODE_PAE_NX:
        case SUPPAGINGMODE_PAE_GLOBAL_NX:
            return pVM->pgm.s.HCPhysInterPaePDPT;

        case SUPPAGINGMODE_AMD64:
        case SUPPAGINGMODE_AMD64_GLOBAL:
        case SUPPAGINGMODE_AMD64_NX:
        case SUPPAGINGMODE_AMD64_GLOBAL_NX:
            return pVM->pgm.s.HCPhysInterPaePDPT;

        default:
            AssertMsgFailed(("enmHostMode=%d\n", pVM->pgm.s.enmHostMode));
            return ~0;
    }
}


/**
 * Gets the current CR3 register value for the RC intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetInterRCCR3(PVM pVM)
{
    switch (pVM->pgm.s.enmShadowMode)
    {
        case PGMMODE_32_BIT:
            return pVM->pgm.s.HCPhysInterPD;

        case PGMMODE_PAE:
        case PGMMODE_PAE_NX:
            return pVM->pgm.s.HCPhysInterPaePDPT;

        case PGMMODE_AMD64:
        case PGMMODE_AMD64_NX:
            return pVM->pgm.s.HCPhysInterPaePML4;

        case PGMMODE_EPT:
        case PGMMODE_NESTED:
            return 0; /* not relevant */

        default:
            AssertMsgFailed(("enmShadowMode=%d\n", pVM->pgm.s.enmShadowMode));
            return ~0;
    }
}


/**
 * Gets the CR3 register value for the 32-Bit intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetInter32BitCR3(PVM pVM)
{
    return pVM->pgm.s.HCPhysInterPD;
}


/**
 * Gets the CR3 register value for the PAE intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetInterPaeCR3(PVM pVM)
{
    return pVM->pgm.s.HCPhysInterPaePDPT;
}


/**
 * Gets the CR3 register value for the AMD64 intermediate memory context.
 * @returns CR3 value.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTHCPHYS) PGMGetInterAmd64CR3(PVM pVM)
{
    return pVM->pgm.s.HCPhysInterPaePML4;
}


/**
 * Performs and schedules necessary updates following a CR3 load or reload.
 *
 * This will normally involve mapping the guest PD or nPDPT
 *
 * @returns VBox status code.
 * @retval  VINF_PGM_SYNC_CR3 if monitoring requires a CR3 sync. This can
 *          safely be ignored and overridden since the FF will be set too then.
 * @param   pVM         VM handle.
 * @param   cr3         The new cr3.
 * @param   fGlobal     Indicates whether this is a global flush or not.
 */
VMMDECL(int) PGMFlushTLB(PVM pVM, uint64_t cr3, bool fGlobal)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,FlushTLB), a);

    /*
     * Always flag the necessary updates; necessary for hardware acceleration
     */
    /** @todo optimize this, it shouldn't always be necessary. */
    VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL);
    if (fGlobal)
        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
    LogFlow(("PGMFlushTLB: cr3=%RX64 OldCr3=%RX64 fGlobal=%d\n", cr3, pVM->pgm.s.GCPhysCR3, fGlobal));

    /*
     * Remap the CR3 content and adjust the monitoring if CR3 was actually changed.
     */
    int rc = VINF_SUCCESS;
    RTGCPHYS GCPhysCR3;
    switch (pVM->pgm.s.enmGuestMode)
    {
    case PGMMODE_PAE:
    case PGMMODE_PAE_NX:
        GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_PAE_PAGE_MASK);
        break;
    case PGMMODE_AMD64:
    case PGMMODE_AMD64_NX:
        GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_AMD64_PAGE_MASK);
        break;
    default:
        GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_PAGE_MASK);
        break;
    }

    if (pVM->pgm.s.GCPhysCR3 != GCPhysCR3)
    {
        RTGCPHYS GCPhysOldCR3 = pVM->pgm.s.GCPhysCR3;
        pVM->pgm.s.GCPhysCR3  = GCPhysCR3;
        rc = PGM_BTH_PFN(MapCR3, pVM)(pVM, GCPhysCR3);
        if (RT_LIKELY(rc == VINF_SUCCESS))
        {
            if (!pVM->pgm.s.fMappingsFixed)
            {
                pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_MONITOR_CR3;
            }
        }
        else
        {
            AssertMsg(rc == VINF_PGM_SYNC_CR3, ("%Rrc\n", rc));
            Assert(VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL | VM_FF_PGM_SYNC_CR3));
            pVM->pgm.s.GCPhysCR3 = GCPhysOldCR3;
            pVM->pgm.s.fSyncFlags |= PGM_SYNC_MAP_CR3;
            if (!pVM->pgm.s.fMappingsFixed)
                pVM->pgm.s.fSyncFlags |= PGM_SYNC_MONITOR_CR3;
        }

        if (fGlobal)
            STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,FlushTLBNewCR3Global));
        else
            STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,FlushTLBNewCR3));
    }
    else
    {
        /*
         * Check if we have a pending update of the CR3 monitoring.
         */
        if (pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3)
        {
            pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_MONITOR_CR3;
            Assert(!pVM->pgm.s.fMappingsFixed);
        }
        if (fGlobal)
            STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,FlushTLBSameCR3Global));
        else
            STAM_COUNTER_INC(&pVM->pgm.s.CTX_MID_Z(Stat,FlushTLBSameCR3));
    }

    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,FlushTLB), a);
    return rc;
}


/**
 * Performs and schedules necessary updates following a CR3 load or reload when
 * using nested or extended paging.
 *
 * This API is an alterantive to PDMFlushTLB that avoids actually flushing the
 * TLB and triggering a SyncCR3.
 *
 * This will normally involve mapping the guest PD or nPDPT
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  (If applied when not in nested mode: VINF_PGM_SYNC_CR3 if monitoring
 *          requires a CR3 sync. This can safely be ignored and overridden since
 *          the FF will be set too then.)
 * @param   pVM         VM handle.
 * @param   cr3         The new cr3.
 */
VMMDECL(int) PGMUpdateCR3(PVM pVM, uint64_t cr3)
{
    LogFlow(("PGMUpdateCR3: cr3=%RX64 OldCr3=%RX64\n", cr3, pVM->pgm.s.GCPhysCR3));

    /* We assume we're only called in nested paging mode. */
    Assert(pVM->pgm.s.fMappingsFixed);
    Assert(!(pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3));
    Assert(pVM->pgm.s.enmShadowMode == PGMMODE_NESTED || pVM->pgm.s.enmShadowMode == PGMMODE_EPT);

    /*
     * Remap the CR3 content and adjust the monitoring if CR3 was actually changed.
     */
    int rc = VINF_SUCCESS;
    RTGCPHYS GCPhysCR3;
    switch (pVM->pgm.s.enmGuestMode)
    {
    case PGMMODE_PAE:
    case PGMMODE_PAE_NX:
        GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_PAE_PAGE_MASK);
        break;
    case PGMMODE_AMD64:
    case PGMMODE_AMD64_NX:
        GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_AMD64_PAGE_MASK);
        break;
    default:
        GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_PAGE_MASK);
        break;
    }
    if (pVM->pgm.s.GCPhysCR3 != GCPhysCR3)
    {
        pVM->pgm.s.GCPhysCR3 = GCPhysCR3;
        rc = PGM_BTH_PFN(MapCR3, pVM)(pVM, GCPhysCR3);
        AssertRCSuccess(rc); /* Assumes VINF_PGM_SYNC_CR3 doesn't apply to nested paging. */ /** @todo this isn't true for the mac, but we need hw to test/fix this. */
    }
    return rc;
}


/**
 * Synchronize the paging structures.
 *
 * This function is called in response to the VM_FF_PGM_SYNC_CR3 and
 * VM_FF_PGM_SYNC_CR3_NONGLOBAL. Those two force action flags are set
 * in several places, most importantly whenever the CR3 is loaded.
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   cr0         Guest context CR0 register
 * @param   cr3         Guest context CR3 register
 * @param   cr4         Guest context CR4 register
 * @param   fGlobal     Including global page directories or not
 */
VMMDECL(int) PGMSyncCR3(PVM pVM, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal)
{
    int rc;

    /*
     * We might be called when we shouldn't.
     *
     * The mode switching will ensure that the PD is resynced
     * after every mode switch. So, if we find ourselves here
     * when in protected or real mode we can safely disable the
     * FF and return immediately.
     */
    if (pVM->pgm.s.enmGuestMode <= PGMMODE_PROTECTED)
    {
        Assert((cr0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE));
        VM_FF_CLEAR(pVM, VM_FF_PGM_SYNC_CR3);
        VM_FF_CLEAR(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL);
        return VINF_SUCCESS;
    }

    /* If global pages are not supported, then all flushes are global. */
    if (!(cr4 & X86_CR4_PGE))
        fGlobal = true;
    LogFlow(("PGMSyncCR3: cr0=%RX64 cr3=%RX64 cr4=%RX64 fGlobal=%d[%d,%d]\n", cr0, cr3, cr4, fGlobal,
             VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3), VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL)));

#ifdef PGMPOOL_WITH_MONITORING
    /*
     * The pool may have pending stuff and even require a return to ring-3 to
     * clear the whole thing.
     */
    rc = pgmPoolSyncCR3(pVM);
    if (rc != VINF_SUCCESS)
        return rc;
#endif

    /*
     * Check if we need to finish an aborted MapCR3 call (see PGMFlushTLB).
     * This should be done before SyncCR3.
     */
    if (pVM->pgm.s.fSyncFlags & PGM_SYNC_MAP_CR3)
    {
        pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_MAP_CR3;

        RTGCPHYS GCPhysCR3Old = pVM->pgm.s.GCPhysCR3;
        RTGCPHYS GCPhysCR3;
        switch (pVM->pgm.s.enmGuestMode)
        {
        case PGMMODE_PAE:
        case PGMMODE_PAE_NX:
            GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_PAE_PAGE_MASK);
            break;
        case PGMMODE_AMD64:
        case PGMMODE_AMD64_NX:
            GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_AMD64_PAGE_MASK);
            break;
        default:
            GCPhysCR3 = (RTGCPHYS)(cr3 & X86_CR3_PAGE_MASK);
            break;
        }

        if (pVM->pgm.s.GCPhysCR3 != GCPhysCR3)
        {
            pVM->pgm.s.GCPhysCR3 = GCPhysCR3;
            rc = PGM_BTH_PFN(MapCR3, pVM)(pVM, GCPhysCR3);
        }
#ifdef IN_RING3
        if (rc == VINF_PGM_SYNC_CR3)
            rc = pgmPoolSyncCR3(pVM);
#else
        if (rc == VINF_PGM_SYNC_CR3)
        {
            pVM->pgm.s.GCPhysCR3 = GCPhysCR3Old;
            return rc;
        }
#endif
        AssertRCReturn(rc, rc);
        AssertRCSuccessReturn(rc, VERR_INTERNAL_ERROR);
    }

    /*
     * Let the 'Bth' function do the work and we'll just keep track of the flags.
     */
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3), a);
    rc = PGM_BTH_PFN(SyncCR3, pVM)(pVM, cr0, cr3, cr4, fGlobal);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3), a);
    AssertMsg(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3 || RT_FAILURE(rc), ("rc=%Rrc\n", rc));
    if (rc == VINF_SUCCESS)
    {
        if (!(pVM->pgm.s.fSyncFlags & PGM_SYNC_ALWAYS))
        {
            VM_FF_CLEAR(pVM, VM_FF_PGM_SYNC_CR3);
            VM_FF_CLEAR(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL);
        }

        /*
         * Check if we have a pending update of the CR3 monitoring.
         */
        if (pVM->pgm.s.fSyncFlags & PGM_SYNC_MONITOR_CR3)
        {
            pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_MONITOR_CR3;
            Assert(!pVM->pgm.s.fMappingsFixed);
        }
    }

    /*
     * Now flush the CR3 (guest context).
     */
    if (rc == VINF_SUCCESS)
        PGM_INVL_GUEST_TLBS();
    return rc;
}


/**
 * Called whenever CR0 or CR4 in a way which may change
 * the paging mode.
 *
 * @returns VBox status code fit for scheduling in GC and R0.
 * @retval  VINF_SUCCESS if the was no change, or it was successfully dealt with.
 * @retval  VINF_PGM_CHANGE_MODE if we're in GC or R0 and the mode changes.
 * @param   pVM         VM handle.
 * @param   cr0         The new cr0.
 * @param   cr4         The new cr4.
 * @param   efer        The new extended feature enable register.
 */
VMMDECL(int) PGMChangeMode(PVM pVM, uint64_t cr0, uint64_t cr4, uint64_t efer)
{
    PGMMODE enmGuestMode;

    /*
     * Calc the new guest mode.
     */
    if (!(cr0 & X86_CR0_PE))
        enmGuestMode = PGMMODE_REAL;
    else if (!(cr0 & X86_CR0_PG))
        enmGuestMode = PGMMODE_PROTECTED;
    else if (!(cr4 & X86_CR4_PAE))
        enmGuestMode = PGMMODE_32_BIT;
    else if (!(efer & MSR_K6_EFER_LME))
    {
        if (!(efer & MSR_K6_EFER_NXE))
            enmGuestMode = PGMMODE_PAE;
        else
            enmGuestMode = PGMMODE_PAE_NX;
    }
    else
    {
        if (!(efer & MSR_K6_EFER_NXE))
            enmGuestMode = PGMMODE_AMD64;
        else
            enmGuestMode = PGMMODE_AMD64_NX;
    }

    /*
     * Did it change?
     */
    if (pVM->pgm.s.enmGuestMode == enmGuestMode)
        return VINF_SUCCESS;

    /* Flush the TLB */
    PGM_INVL_GUEST_TLBS();

#ifdef IN_RING3
    return PGMR3ChangeMode(pVM, enmGuestMode);
#else
    LogFlow(("PGMChangeMode: returns VINF_PGM_CHANGE_MODE.\n"));
    return VINF_PGM_CHANGE_MODE;
#endif
}


/**
 * Gets the current guest paging mode.
 *
 * If you just need the CPU mode (real/protected/long), use CPUMGetGuestMode().
 *
 * @returns The current paging mode.
 * @param   pVM             The VM handle.
 */
VMMDECL(PGMMODE) PGMGetGuestMode(PVM pVM)
{
    return pVM->pgm.s.enmGuestMode;
}


/**
 * Gets the current shadow paging mode.
 *
 * @returns The current paging mode.
 * @param   pVM             The VM handle.
 */
VMMDECL(PGMMODE) PGMGetShadowMode(PVM pVM)
{
    return pVM->pgm.s.enmShadowMode;
}

/**
 * Gets the current host paging mode.
 *
 * @returns The current paging mode.
 * @param   pVM             The VM handle.
 */
VMMDECL(PGMMODE) PGMGetHostMode(PVM pVM)
{
    switch (pVM->pgm.s.enmHostMode)
    {
        case SUPPAGINGMODE_32_BIT:
        case SUPPAGINGMODE_32_BIT_GLOBAL:
            return PGMMODE_32_BIT;

        case SUPPAGINGMODE_PAE:
        case SUPPAGINGMODE_PAE_GLOBAL:
            return PGMMODE_PAE;

        case SUPPAGINGMODE_PAE_NX:
        case SUPPAGINGMODE_PAE_GLOBAL_NX:
            return PGMMODE_PAE_NX;

        case SUPPAGINGMODE_AMD64:
        case SUPPAGINGMODE_AMD64_GLOBAL:
            return PGMMODE_AMD64;

        case SUPPAGINGMODE_AMD64_NX:
        case SUPPAGINGMODE_AMD64_GLOBAL_NX:
            return PGMMODE_AMD64_NX;

        default: AssertMsgFailed(("enmHostMode=%d\n", pVM->pgm.s.enmHostMode)); break;
    }

    return PGMMODE_INVALID;
}


/**
 * Get mode name.
 *
 * @returns read-only name string.
 * @param   enmMode     The mode which name is desired.
 */
VMMDECL(const char *) PGMGetModeName(PGMMODE enmMode)
{
    switch (enmMode)
    {
        case PGMMODE_REAL:      return "Real";
        case PGMMODE_PROTECTED: return "Protected";
        case PGMMODE_32_BIT:    return "32-bit";
        case PGMMODE_PAE:       return "PAE";
        case PGMMODE_PAE_NX:    return "PAE+NX";
        case PGMMODE_AMD64:     return "AMD64";
        case PGMMODE_AMD64_NX:  return "AMD64+NX";
        case PGMMODE_NESTED:    return "Nested";
        case PGMMODE_EPT:       return "EPT";
        default:                return "unknown mode value";
    }
}


/**
 * Acquire the PGM lock.
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 */
int pgmLock(PVM pVM)
{
    int rc = PDMCritSectEnter(&pVM->pgm.s.CritSect, VERR_SEM_BUSY);
#ifdef IN_RC
    if (rc == VERR_SEM_BUSY)
        rc = VMMGCCallHost(pVM, VMMCALLHOST_PGM_LOCK, 0);
#elif defined(IN_RING0)
    if (rc == VERR_SEM_BUSY)
        rc = VMMR0CallHost(pVM, VMMCALLHOST_PGM_LOCK, 0);
#endif
    AssertMsg(rc == VINF_SUCCESS, ("%Rrc\n", rc));
    return rc;
}


/**
 * Release the PGM lock.
 *
 * @returns VBox status code
 * @param   pVM         The VM to operate on.
 */
void pgmUnlock(PVM pVM)
{
    PDMCritSectLeave(&pVM->pgm.s.CritSect);
}

#if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)

/**
 * Temporarily maps one guest page specified by GC physical address.
 * These pages must have a physical mapping in HC, i.e. they cannot be MMIO pages.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhys      GC Physical address of the page.
 * @param   ppv         Where to store the address of the mapping.
 */
VMMDECL(int) PGMDynMapGCPage(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    AssertMsg(!(GCPhys & PAGE_OFFSET_MASK), ("GCPhys=%RGp\n", GCPhys));

    /*
     * Get the ram range.
     */
    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
    while (pRam && GCPhys - pRam->GCPhys >= pRam->cb)
        pRam = pRam->CTX_SUFF(pNext);
    if (!pRam)
    {
        AssertMsgFailed(("Invalid physical address %RGp!\n", GCPhys));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    /*
     * Pass it on to PGMDynMapHCPage.
     */
    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT]);
    //Log(("PGMDynMapGCPage: GCPhys=%RGp HCPhys=%RHp\n", GCPhys, HCPhys));
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    pgmR0DynMapHCPageInlined(&pVM->pgm.s, HCPhys, ppv);
#else
    PGMDynMapHCPage(pVM, HCPhys, ppv);
#endif
    return VINF_SUCCESS;
}


/**
 * Temporarily maps one guest page specified by unaligned GC physical address.
 * These pages must have a physical mapping in HC, i.e. they cannot be MMIO pages.
 *
 * Be WARNED that the dynamic page mapping area is small, 8 pages, thus the space is
 * reused after 8 mappings (or perhaps a few more if you score with the cache).
 *
 * The caller is aware that only the speicifed page is mapped and that really bad things
 * will happen if writing beyond the page!
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   GCPhys      GC Physical address within the page to be mapped.
 * @param   ppv         Where to store the address of the mapping address corresponding to GCPhys.
 */
VMMDECL(int) PGMDynMapGCPageOff(PVM pVM, RTGCPHYS GCPhys, void **ppv)
{
    /*
     * Get the ram range.
     */
    PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
    while (pRam && GCPhys - pRam->GCPhys >= pRam->cb)
        pRam = pRam->CTX_SUFF(pNext);
    if (!pRam)
    {
        AssertMsgFailed(("Invalid physical address %RGp!\n", GCPhys));
        return VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
    }

    /*
     * Pass it on to PGMDynMapHCPage.
     */
    RTHCPHYS HCPhys = PGM_PAGE_GET_HCPHYS(&pRam->aPages[(GCPhys - pRam->GCPhys) >> PAGE_SHIFT]);
#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0
    pgmR0DynMapHCPageInlined(&pVM->pgm.s, HCPhys, ppv);
#else
    PGMDynMapHCPage(pVM, HCPhys, ppv);
#endif
    *ppv = (void *)((uintptr_t)*ppv | (GCPhys & PAGE_OFFSET_MASK));
    return VINF_SUCCESS;
}

# ifdef IN_RC

/**
 * Temporarily maps one host page specified by HC physical address.
 *
 * Be WARNED that the dynamic page mapping area is small, 16 pages, thus the space is
 * reused after 16 mappings (or perhaps a few more if you score with the cache).
 *
 * @returns VINF_SUCCESS, will bail out to ring-3 on failure.
 * @param   pVM         VM handle.
 * @param   HCPhys      HC Physical address of the page.
 * @param   ppv         Where to store the address of the mapping. This is the
 *                      address of the PAGE not the exact address corresponding
 *                      to HCPhys. Use PGMDynMapHCPageOff if you care for the
 *                      page offset.
 */
VMMDECL(int) PGMDynMapHCPage(PVM pVM, RTHCPHYS HCPhys, void **ppv)
{
    AssertMsg(!(HCPhys & PAGE_OFFSET_MASK), ("HCPhys=%RHp\n", HCPhys));

    /*
     * Check the cache.
     */
    register unsigned iCache;
    for (iCache = 0;iCache < RT_ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache);iCache++)
    {
        static const uint8_t au8Trans[MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT][RT_ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache)] =
        {
            { 0,  9, 10, 11, 12, 13, 14, 15},
            { 0,  1, 10, 11, 12, 13, 14, 15},
            { 0,  1,  2, 11, 12, 13, 14, 15},
            { 0,  1,  2,  3, 12, 13, 14, 15},
            { 0,  1,  2,  3,  4, 13, 14, 15},
            { 0,  1,  2,  3,  4,  5, 14, 15},
            { 0,  1,  2,  3,  4,  5,  6, 15},
            { 0,  1,  2,  3,  4,  5,  6,  7},
            { 8,  1,  2,  3,  4,  5,  6,  7},
            { 8,  9,  2,  3,  4,  5,  6,  7},
            { 8,  9, 10,  3,  4,  5,  6,  7},
            { 8,  9, 10, 11,  4,  5,  6,  7},
            { 8,  9, 10, 11, 12,  5,  6,  7},
            { 8,  9, 10, 11, 12, 13,  6,  7},
            { 8,  9, 10, 11, 12, 13, 14,  7},
            { 8,  9, 10, 11, 12, 13, 14, 15},
        };
        AssertCompile(RT_ELEMENTS(au8Trans) == 16);
        AssertCompile(RT_ELEMENTS(au8Trans[0]) == 8);

        if (pVM->pgm.s.aHCPhysDynPageMapCache[iCache] == HCPhys)
        {
            int iPage = au8Trans[pVM->pgm.s.iDynPageMapLast][iCache];

            /* The cache can get out of sync with locked entries. (10 locked, 2 overwrites its cache position, last = 11, lookup 2 -> page 10 instead of 2) */
            if ((pVM->pgm.s.paDynPageMap32BitPTEsGC[iPage].u & X86_PTE_PG_MASK) == HCPhys)
            {
                void *pv = pVM->pgm.s.pbDynPageMapBaseGC + (iPage << PAGE_SHIFT);
                *ppv = pv;
                STAM_COUNTER_INC(&pVM->pgm.s.StatRCDynMapCacheHits);
                Log4(("PGMGCDynMapHCPage: HCPhys=%RHp pv=%p iPage=%d iCache=%d\n", HCPhys, pv, iPage, iCache));
                return VINF_SUCCESS;
            }
        }
    }
    AssertCompile(RT_ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache) == 8);
    AssertCompile((MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT) == 16);
    STAM_COUNTER_INC(&pVM->pgm.s.StatRCDynMapCacheMisses);

    /*
     * Update the page tables.
     */
    register unsigned iPage = pVM->pgm.s.iDynPageMapLast;
    unsigned i;
    for (i=0;i<(MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT);i++)
    {
        pVM->pgm.s.iDynPageMapLast = iPage = (iPage + 1) & ((MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT) - 1);
        if (!pVM->pgm.s.aLockedDynPageMapCache[iPage])
            break;
        iPage++;
    }
    AssertRelease(i != (MM_HYPER_DYNAMIC_SIZE >> PAGE_SHIFT));

    pVM->pgm.s.aHCPhysDynPageMapCache[iPage & (RT_ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache) - 1)] = HCPhys;
    pVM->pgm.s.paDynPageMap32BitPTEsGC[iPage].u = (uint32_t)HCPhys | X86_PTE_P | X86_PTE_A | X86_PTE_D;
    pVM->pgm.s.paDynPageMapPaePTEsGC[iPage].u   =           HCPhys | X86_PTE_P | X86_PTE_A | X86_PTE_D;
    pVM->pgm.s.aLockedDynPageMapCache[iPage]    = 0;

    void *pv = pVM->pgm.s.pbDynPageMapBaseGC + (iPage << PAGE_SHIFT);
    *ppv = pv;
    ASMInvalidatePage(pv);
    Log4(("PGMGCDynMapHCPage: HCPhys=%RHp pv=%p iPage=%d\n", HCPhys, pv, iPage));
    return VINF_SUCCESS;
}


/**
 * Temporarily lock a dynamic page to prevent it from being reused.
 *
 * @param   pVM         VM handle.
 * @param   GCPage      GC address of page
 */
VMMDECL(void) PGMDynLockHCPage(PVM pVM, RCPTRTYPE(uint8_t *) GCPage)
{
    unsigned iPage;

    Assert(GCPage >= pVM->pgm.s.pbDynPageMapBaseGC && GCPage < (pVM->pgm.s.pbDynPageMapBaseGC + MM_HYPER_DYNAMIC_SIZE));
    iPage = ((uintptr_t)(GCPage - pVM->pgm.s.pbDynPageMapBaseGC)) >> PAGE_SHIFT;
    ASMAtomicIncU32(&pVM->pgm.s.aLockedDynPageMapCache[iPage]);
}


/**
 * Unlock a dynamic page
 *
 * @param   pVM         VM handle.
 * @param   GCPage      GC address of page
 */
VMMDECL(void) PGMDynUnlockHCPage(PVM pVM, RCPTRTYPE(uint8_t *) GCPage)
{
    unsigned iPage;

    AssertCompile(RT_ELEMENTS(pVM->pgm.s.aHCPhysDynPageMapCache) == RT_ELEMENTS(pVM->pgm.s.aLockedDynPageMapCache));

    Assert(GCPage >= pVM->pgm.s.pbDynPageMapBaseGC && GCPage < (pVM->pgm.s.pbDynPageMapBaseGC + MM_HYPER_DYNAMIC_SIZE));
    iPage = ((uintptr_t)(GCPage - pVM->pgm.s.pbDynPageMapBaseGC)) >> PAGE_SHIFT;
    Assert(pVM->pgm.s.aLockedDynPageMapCache[iPage]);
    ASMAtomicDecU32(&pVM->pgm.s.aLockedDynPageMapCache[iPage]);
}


#  ifdef VBOX_STRICT
/**
 * Check for lock leaks.
 *
 * @param   pVM         VM handle.
 */
VMMDECL(void) PGMDynCheckLocks(PVM pVM)
{
    for (unsigned i=0;i<RT_ELEMENTS(pVM->pgm.s.aLockedDynPageMapCache);i++)
        Assert(!pVM->pgm.s.aLockedDynPageMapCache[i]);
}
#  endif /* VBOX_STRICT */

# endif /* IN_RC */
#endif /* IN_RC || VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0 */

#if !defined(IN_R0) || defined(LOG_ENABLED)

/** Format handler for PGMPAGE.
 * @copydoc FNRTSTRFORMATTYPE */
static DECLCALLBACK(size_t) pgmFormatTypeHandlerPage(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                    const char *pszType, void const *pvValue,
                                                    int cchWidth, int cchPrecision, unsigned fFlags,
                                                    void *pvUser)
{
    size_t    cch;
    PCPGMPAGE pPage = (PCPGMPAGE)pvValue;
    if (VALID_PTR(pPage))
    {
        char szTmp[64+80];

        cch = 0;

        /* The single char state stuff. */
        static const char s_achPageStates[4]    = { 'Z', 'A', 'W', 'S' };
        szTmp[cch++] = s_achPageStates[PGM_PAGE_GET_STATE(pPage)];

#define IS_PART_INCLUDED(lvl) ( !(fFlags & RTSTR_F_PRECISION) || cchPrecision == (lvl) || cchPrecision >= (lvl)+10 )
        if (IS_PART_INCLUDED(5))
        {
            static const char s_achHandlerStates[4] = { '-', 't', 'w', 'a' };
            szTmp[cch++] = s_achHandlerStates[PGM_PAGE_GET_HNDL_PHYS_STATE(pPage)];
            szTmp[cch++] = s_achHandlerStates[PGM_PAGE_GET_HNDL_VIRT_STATE(pPage)];
        }

        /* The type. */
        if (IS_PART_INCLUDED(4))
        {
            szTmp[cch++] = ':';
            static const char s_achPageTypes[8][4]  = { "INV", "RAM", "MI2", "M2A", "SHA", "ROM", "MIO", "BAD" };
            szTmp[cch++] = s_achPageTypes[PGM_PAGE_GET_TYPE(pPage)][0];
            szTmp[cch++] = s_achPageTypes[PGM_PAGE_GET_TYPE(pPage)][1];
            szTmp[cch++] = s_achPageTypes[PGM_PAGE_GET_TYPE(pPage)][2];
        }

        /* The numbers. */
        if (IS_PART_INCLUDED(3))
        {
            szTmp[cch++] = ':';
            cch += RTStrFormatNumber(&szTmp[cch], PGM_PAGE_GET_HCPHYS(pPage), 16, 12, 0, RTSTR_F_ZEROPAD | RTSTR_F_64BIT);
        }

        if (IS_PART_INCLUDED(2))
        {
            szTmp[cch++] = ':';
            cch += RTStrFormatNumber(&szTmp[cch], PGM_PAGE_GET_PAGEID(pPage), 16, 7, 0, RTSTR_F_ZEROPAD | RTSTR_F_32BIT);
        }

        if (IS_PART_INCLUDED(6))
        {
            szTmp[cch++] = ':';
            static const char s_achRefs[4] = { '-', 'U', '!', 'L' };
            szTmp[cch++] = s_achRefs[PGM_PAGE_GET_TD_CREFS(pPage)];
            cch += RTStrFormatNumber(&szTmp[cch], PGM_PAGE_GET_TD_IDX(pPage), 16, 4, 0, RTSTR_F_ZEROPAD | RTSTR_F_16BIT);
        }
#undef IS_PART_INCLUDED

        cch = pfnOutput(pvArgOutput, szTmp, cch);
    }
    else
        cch = pfnOutput(pvArgOutput, "<bad-pgmpage-ptr>", sizeof("<bad-pgmpage-ptr>") - 1);
    return cch;
}


/** Format handler for PGMRAMRANGE.
 * @copydoc FNRTSTRFORMATTYPE */
static DECLCALLBACK(size_t) pgmFormatTypeHandlerRamRange(PFNRTSTROUTPUT pfnOutput, void *pvArgOutput,
                                                         const char *pszType, void const *pvValue,
                                                         int cchWidth, int cchPrecision, unsigned fFlags,
                                                         void *pvUser)
{
    size_t              cch;
    PGMRAMRANGE const  *pRam = (PGMRAMRANGE const *)pvValue;
    if (VALID_PTR(pRam))
    {
        char szTmp[80];
        cch = RTStrPrintf(szTmp, sizeof(szTmp), "%RGp-%RGp", pRam->GCPhys, pRam->GCPhysLast);
        cch = pfnOutput(pvArgOutput, szTmp, cch);
    }
    else
        cch = pfnOutput(pvArgOutput, "<bad-pgmramrange-ptr>", sizeof("<bad-pgmramrange-ptr>") - 1);
    return cch;
}

/** Format type andlers to be registered/deregistered. */
static const struct
{
    char                szType[24];
    PFNRTSTRFORMATTYPE  pfnHandler;
} g_aPgmFormatTypes[] =
{
    { "pgmpage",        pgmFormatTypeHandlerPage },
    { "pgmramrange",    pgmFormatTypeHandlerRamRange }
};

#endif /* !IN_R0 || LOG_ENABLED */


/**
 * Registers the global string format types.
 *
 * This should be called at module load time or in some other manner that ensure
 * that it's called exactly one time.
 *
 * @returns IPRT status code on RTStrFormatTypeRegister failure.
 */
VMMDECL(int) PGMRegisterStringFormatTypes(void)
{
#if !defined(IN_R0) || defined(LOG_ENABLED)
    int         rc = VINF_SUCCESS;
    unsigned    i;
    for (i = 0; RT_SUCCESS(rc) && i < RT_ELEMENTS(g_aPgmFormatTypes); i++)
    {
        rc = RTStrFormatTypeRegister(g_aPgmFormatTypes[i].szType, g_aPgmFormatTypes[i].pfnHandler, NULL);
# ifdef IN_RING0
        if (rc == VERR_ALREADY_EXISTS)
        {
            /* in case of cleanup failure in ring-0 */
            RTStrFormatTypeDeregister(g_aPgmFormatTypes[i].szType);
            rc = RTStrFormatTypeRegister(g_aPgmFormatTypes[i].szType, g_aPgmFormatTypes[i].pfnHandler, NULL);
        }
# endif
    }
    if (RT_FAILURE(rc))
        while (i-- > 0)
            RTStrFormatTypeDeregister(g_aPgmFormatTypes[i].szType);

    return rc;
#else
    return VINF_SUCCESS;
#endif
}


/**
 * Deregisters the global string format types.
 *
 * This should be called at module unload time or in some other manner that
 * ensure that it's called exactly one time.
 */
VMMDECL(void) PGMDeregisterStringFormatTypes(void)
{
#if !defined(IN_R0) || defined(LOG_ENABLED)
    for (unsigned i = 0; i < RT_ELEMENTS(g_aPgmFormatTypes); i++)
        RTStrFormatTypeDeregister(g_aPgmFormatTypes[i].szType);
#endif
}

#ifdef VBOX_STRICT

/**
 * Asserts that there are no mapping conflicts.
 *
 * @returns Number of conflicts.
 * @param   pVM     The VM Handle.
 */
VMMDECL(unsigned) PGMAssertNoMappingConflicts(PVM pVM)
{
    unsigned cErrors = 0;

    /*
     * Check for mapping conflicts.
     */
    for (PPGMMAPPING pMapping = pVM->pgm.s.CTX_SUFF(pMappings);
         pMapping;
         pMapping = pMapping->CTX_SUFF(pNext))
    {
        /** @todo This is slow and should be optimized, but since it's just assertions I don't care now. */
        for (RTGCPTR GCPtr = pMapping->GCPtr;
              GCPtr <= pMapping->GCPtrLast;
              GCPtr += PAGE_SIZE)
        {
            int rc = PGMGstGetPage(pVM, (RTGCPTR)GCPtr, NULL, NULL);
            if (rc != VERR_PAGE_TABLE_NOT_PRESENT)
            {
                AssertMsgFailed(("Conflict at %RGv with %s\n", GCPtr, R3STRING(pMapping->pszDesc)));
                cErrors++;
                break;
            }
        }
    }

    return cErrors;
}


/**
 * Asserts that everything related to the guest CR3 is correctly shadowed.
 *
 * This will call PGMAssertNoMappingConflicts() and PGMAssertHandlerAndFlagsInSync(),
 * and assert the correctness of the guest CR3 mapping before asserting that the
 * shadow page tables is in sync with the guest page tables.
 *
 * @returns Number of conflicts.
 * @param   pVM     The VM Handle.
 * @param   cr3     The current guest CR3 register value.
 * @param   cr4     The current guest CR4 register value.
 */
VMMDECL(unsigned) PGMAssertCR3(PVM pVM, uint64_t cr3, uint64_t cr4)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3), a);
    unsigned cErrors = PGM_BTH_PFN(AssertCR3, pVM)(pVM, cr3, cr4, 0, ~(RTGCPTR)0);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3), a);
    return cErrors;
}

#endif /* VBOX_STRICT */
