/* $Id$ */
/** @file
 * VBox - Page Manager, Shadow Paging Template - All context code.
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
#undef SHWPT
#undef PSHWPT
#undef SHWPTE
#undef PSHWPTE
#undef SHWPD
#undef PSHWPD
#undef SHWPDE
#undef PSHWPDE
#undef SHW_PDE_PG_MASK
#undef SHW_PD_SHIFT
#undef SHW_PD_MASK
#undef SHW_PTE_PG_MASK
#undef SHW_PT_SHIFT
#undef SHW_PT_MASK
#undef SHW_TOTAL_PD_ENTRIES
#undef SHW_PDPT_SHIFT
#undef SHW_PDPT_MASK
#undef SHW_POOL_ROOT_IDX

#if PGM_SHW_TYPE == PGM_TYPE_32BIT
# define SHWPT                  X86PT
# define PSHWPT                 PX86PT
# define SHWPTE                 X86PTE
# define PSHWPTE                PX86PTE
# define SHWPD                  X86PD
# define PSHWPD                 PX86PD
# define SHWPDE                 X86PDE
# define PSHWPDE                PX86PDE
# define SHW_PDE_PG_MASK        X86_PDE_PG_MASK
# define SHW_PD_SHIFT           X86_PD_SHIFT
# define SHW_PD_MASK            X86_PD_MASK
# define SHW_TOTAL_PD_ENTRIES   X86_PG_ENTRIES
# define SHW_PTE_PG_MASK        X86_PTE_PG_MASK
# define SHW_PT_SHIFT           X86_PT_SHIFT
# define SHW_PT_MASK            X86_PT_MASK
# define SHW_POOL_ROOT_IDX      PGMPOOL_IDX_PD
#else
# define SHWPT                  X86PTPAE
# define PSHWPT                 PX86PTPAE
# define SHWPTE                 X86PTEPAE
# define PSHWPTE                PX86PTEPAE
# define SHWPD                  X86PDPAE
# define PSHWPD                 PX86PDPAE
# define SHWPDE                 X86PDEPAE
# define PSHWPDE                PX86PDEPAE
# define SHW_PDE_PG_MASK        X86_PDE_PAE_PG_MASK
# define SHW_PD_SHIFT           X86_PD_PAE_SHIFT
# define SHW_PD_MASK            X86_PD_PAE_MASK
# define SHW_PTE_PG_MASK        X86_PTE_PAE_PG_MASK
# define SHW_PT_SHIFT           X86_PT_PAE_SHIFT
# define SHW_PT_MASK            X86_PT_PAE_MASK
#if PGM_SHW_TYPE == PGM_TYPE_AMD64
# define SHW_PDPT_SHIFT        X86_PDPT_SHIFT
# define SHW_PDPT_MASK         X86_PDPT_MASK_AMD64
# define SHW_TOTAL_PD_ENTRIES   (X86_PG_AMD64_ENTRIES*X86_PG_AMD64_PDPE_ENTRIES)
# define SHW_POOL_ROOT_IDX      PGMPOOL_IDX_PAE_PD
#else /* 32 bits PAE mode */
# define SHW_PDPT_SHIFT        X86_PDPT_SHIFT
# define SHW_PDPT_MASK         X86_PDPT_MASK_PAE
# define SHW_TOTAL_PD_ENTRIES   (X86_PG_PAE_ENTRIES*X86_PG_PAE_PDPE_ENTRIES)
# define SHW_POOL_ROOT_IDX      PGMPOOL_IDX_PAE_PD
#endif
#endif



/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
PGM_SHW_DECL(int, GetPage)(PVM pVM, RTGCUINTPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys);
PGM_SHW_DECL(int, ModifyPage)(PVM pVM, RTGCUINTPTR GCPtr, size_t cbPages, uint64_t fFlags, uint64_t fMask);
__END_DECLS



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
PGM_SHW_DECL(int, GetPage)(PVM pVM, RTGCUINTPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys)
{
#if PGM_SHW_TYPE == PGM_TYPE_NESTED
    return VERR_PAGE_TABLE_NOT_PRESENT;

#else /* PGM_SHW_TYPE != PGM_TYPE_NESTED */
    /*
     * Get the PDE.
     */
# if PGM_SHW_TYPE == PGM_TYPE_AMD64
    bool      fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVM) & MSR_K6_EFER_NXE);
    X86PDEPAE Pde;

    /* PML4 */
    const unsigned iPml4  = ((RTGCUINTPTR64)GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
    X86PML4E Pml4e = CTXMID(pVM->pgm.s.p,PaePML4)->a[iPml4];
    if (!Pml4e.n.u1Present)
        return VERR_PAGE_TABLE_NOT_PRESENT;

    /* PDPT */
    PX86PDPT pPDPT;
    int rc = PGM_HCPHYS_2_PTR(pVM, Pml4e.u & X86_PML4E_PG_MASK, &pPDPT);
    if (VBOX_FAILURE(rc))
        return rc;
    const unsigned iPDPT = (GCPtr >> SHW_PDPT_SHIFT) & SHW_PDPT_MASK;
    X86PDPE Pdpe = pPDPT->a[iPDPT];
    if (!Pdpe.n.u1Present)
        return VERR_PAGE_TABLE_NOT_PRESENT;

    /* PD */
    PX86PDPAE pPd;
    rc = PGM_HCPHYS_2_PTR(pVM, Pdpe.u & X86_PDPE_PG_MASK, &pPd);
    if (VBOX_FAILURE(rc))
        return rc;
    const unsigned iPd = (GCPtr >> SHW_PD_SHIFT) & SHW_PD_MASK;
    Pde = pPd->a[iPd];

    /* Merge accessed, write, user and no-execute bits into the PDE. */
    Pde.n.u1Accessed  &= Pml4e.n.u1Accessed & Pdpe.lm.u1Accessed;
    Pde.n.u1Write     &= Pml4e.n.u1Write & Pdpe.lm.u1Write;
    Pde.n.u1User      &= Pml4e.n.u1User & Pdpe.lm.u1User;
    Pde.n.u1NoExecute &= Pml4e.n.u1NoExecute & Pdpe.lm.u1NoExecute;

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    bool           fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVM) & MSR_K6_EFER_NXE);
    const unsigned iPDPT = (GCPtr >> SHW_PDPT_SHIFT) & SHW_PDPT_MASK;
    const unsigned iPd = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
    X86PDEPAE      Pde = CTXMID(pVM->pgm.s.ap,PaePDs)[iPDPT]->a[iPd];

# else /* PGM_TYPE_32BIT */
    const unsigned iPd = (GCPtr >> X86_PD_SHIFT) & X86_PD_MASK;
    X86PDE Pde = CTXMID(pVM->pgm.s.p,32BitPD)->a[iPd];
# endif
    if (!Pde.n.u1Present)
        return VERR_PAGE_TABLE_NOT_PRESENT;

    Assert(!Pde.b.u1Size);

    /*
     * Get PT entry.
     */
    PSHWPT pPT;
    if (!(Pde.u & PGM_PDFLAGS_MAPPING))
    {
        int rc = PGM_HCPHYS_2_PTR(pVM, Pde.u & SHW_PDE_PG_MASK, &pPT);
        if (VBOX_FAILURE(rc))
            return rc;
    }
    else /* mapping: */
    {
# if PGM_SHW_TYPE == PGM_TYPE_AMD64
        AssertFailed(); /* can't happen */
# else
        Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));

        PPGMMAPPING pMap = pgmGetMapping(pVM, (RTGCPTR)GCPtr);
        AssertMsgReturn(pMap, ("GCPtr=%VGv\n", GCPtr), VERR_INTERNAL_ERROR);
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
        pPT = pMap->aPTs[(GCPtr - pMap->GCPtr) >> X86_PD_SHIFT].CTXALLSUFF(pPT);
#  else /* PAE */
        pPT = pMap->aPTs[(GCPtr - pMap->GCPtr) >> X86_PD_SHIFT].CTXALLSUFF(paPaePTs);
#  endif
# endif
    }
    const unsigned iPt = (GCPtr >> SHW_PT_SHIFT) & SHW_PT_MASK;
    SHWPTE Pte = pPT->a[iPt];
    if (!Pte.n.u1Present)
        return VERR_PAGE_NOT_PRESENT;

    /*
     * Store the results.
     * RW and US flags depend on the entire page translation hierarchy - except for
     * legacy PAE which has a simplified PDPE.
     */
    if (pfFlags)
    {
        *pfFlags = (Pte.u & ~SHW_PTE_PG_MASK)
                 & ((Pde.u & (X86_PTE_RW | X86_PTE_US)) | ~(uint64_t)(X86_PTE_RW | X86_PTE_US));
# if PGM_WITH_NX(PGM_SHW_TYPE)
        /* The NX bit is determined by a bitwise OR between the PT and PD */
        if (fNoExecuteBitValid)
            *pfFlags |= (Pte.u & Pde.u & X86_PTE_PAE_NX);
# endif
    }

    if (pHCPhys)
        *pHCPhys = Pte.u & SHW_PTE_PG_MASK;

    return VINF_SUCCESS;
#endif /* PGM_SHW_TYPE != PGM_TYPE_NESTED */
}


/**
 * Modify page flags for a range of pages in the shadow context.
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range. Page aligned!
 * @param   cb          Size (in bytes) of the range to apply the modification to. Page aligned!
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*.
 *                      Be extremely CAREFUL with ~'ing values because they can be 32-bit!
 * @remark  You must use PGMMapModifyPage() for pages in a mapping.
 */
PGM_SHW_DECL(int, ModifyPage)(PVM pVM, RTGCUINTPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
# if PGM_SHW_TYPE == PGM_TYPE_NESTED
    return VERR_PAGE_TABLE_NOT_PRESENT;

# else /* PGM_SHW_TYPE != PGM_TYPE_NESTED */
    int rc;

    /*
     * Walk page tables and pages till we're done.
     */
    for (;;)
    {
        /*
         * Get the PDE.
         */
# if PGM_SHW_TYPE == PGM_TYPE_AMD64
        X86PDEPAE Pde;
        /* PML4 */
        const unsigned iPml4  = ((RTGCUINTPTR64)GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;
        X86PML4E Pml4e = CTXMID(pVM->pgm.s.p,PaePML4)->a[iPml4];
        if (!Pml4e.n.u1Present)
            return VERR_PAGE_TABLE_NOT_PRESENT;

        /* PDPT */
        PX86PDPT pPDPT;
        rc = PGM_HCPHYS_2_PTR(pVM, Pml4e.u & X86_PML4E_PG_MASK, &pPDPT);
        if (VBOX_FAILURE(rc))
            return rc;
        const unsigned iPDPT = (GCPtr >> SHW_PDPT_SHIFT) & SHW_PDPT_MASK;
        X86PDPE Pdpe = pPDPT->a[iPDPT];
        if (!Pdpe.n.u1Present)
            return VERR_PAGE_TABLE_NOT_PRESENT;

        /* PD */
        PX86PDPAE pPd;
        rc = PGM_HCPHYS_2_PTR(pVM, Pdpe.u & X86_PDPE_PG_MASK, &pPd);
        if (VBOX_FAILURE(rc))
            return rc;
        const unsigned iPd = (GCPtr >> SHW_PD_SHIFT) & SHW_PD_MASK;
        Pde = pPd->a[iPd];

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
        const unsigned iPDPT = (GCPtr >> SHW_PDPT_SHIFT) & SHW_PDPT_MASK;
        const unsigned iPd = (GCPtr >> X86_PD_PAE_SHIFT) & X86_PD_PAE_MASK;
        X86PDEPAE Pde = CTXMID(pVM->pgm.s.ap,PaePDs)[iPDPT]->a[iPd];

# else /* PGM_TYPE_32BIT */
        const unsigned iPd = (GCPtr >> X86_PD_SHIFT) & X86_PD_MASK;
        X86PDE Pde = CTXMID(pVM->pgm.s.p,32BitPD)->a[iPd];
# endif
        if (!Pde.n.u1Present)
            return VERR_PAGE_TABLE_NOT_PRESENT;

        /*
         * Map the page table.
         */
        PSHWPT pPT;
        rc = PGM_HCPHYS_2_PTR(pVM, Pde.u & SHW_PDE_PG_MASK, &pPT);
        if (VBOX_FAILURE(rc))
            return rc;

        unsigned iPTE = (GCPtr >> SHW_PT_SHIFT) & SHW_PT_MASK;
        while (iPTE < ELEMENTS(pPT->a))
        {
            if (pPT->a[iPTE].n.u1Present)
            {
                pPT->a[iPTE].u = (pPT->a[iPTE].u & (fMask | SHW_PTE_PG_MASK)) | (fFlags & ~SHW_PTE_PG_MASK);
                Assert(pPT->a[iPTE].n.u1Present);
                PGM_INVL_PG(GCPtr);
            }

            /* next page */
            cb -= PAGE_SIZE;
            if (!cb)
                return VINF_SUCCESS;
            GCPtr += PAGE_SIZE;
            iPTE++;
        }
    }
# endif /* PGM_SHW_TYPE != PGM_TYPE_NESTED */
}

