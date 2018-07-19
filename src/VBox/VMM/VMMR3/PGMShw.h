/* $Id$ */
/** @file
 * VBox - Page Manager / Monitor, Shadow Paging Template.
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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
#undef SHW_PDPE_PG_MASK

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

#elif PGM_SHW_TYPE == PGM_TYPE_EPT
# define SHWPT                  EPTPT
# define PSHWPT                 PEPTPT
# define SHWPTE                 EPTPTE
# define PSHWPTE                PEPTPTE
# define SHWPD                  EPTPD
# define PSHWPD                 PEPTPD
# define SHWPDE                 EPTPDE
# define PSHWPDE                PEPTPDE
# define SHW_PDE_PG_MASK        EPT_PDE_PG_MASK
# define SHW_PD_SHIFT           EPT_PD_SHIFT
# define SHW_PD_MASK            EPT_PD_MASK
# define SHW_PTE_PG_MASK        EPT_PTE_PG_MASK
# define SHW_PT_SHIFT           EPT_PT_SHIFT
# define SHW_PT_MASK            EPT_PT_MASK
# define SHW_PDPT_SHIFT         EPT_PDPT_SHIFT
# define SHW_PDPT_MASK          EPT_PDPT_MASK
# define SHW_PDPE_PG_MASK       EPT_PDPE_PG_MASK
# define SHW_TOTAL_PD_ENTRIES   (EPT_PG_AMD64_ENTRIES*EPT_PG_AMD64_PDPE_ENTRIES)

#else
# define SHWPT                  PGMSHWPTPAE
# define PSHWPT                 PPGMSHWPTPAE
# define SHWPTE                 PGMSHWPTEPAE
# define PSHWPTE                PPGMSHWPTEPAE
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

# if PGM_SHW_TYPE == PGM_TYPE_AMD64
#  define SHW_PDPT_SHIFT        X86_PDPT_SHIFT
#  define SHW_PDPT_MASK         X86_PDPT_MASK_AMD64
#  define SHW_PDPE_PG_MASK      X86_PDPE_PG_MASK
#  define SHW_TOTAL_PD_ENTRIES  (X86_PG_AMD64_ENTRIES*X86_PG_AMD64_PDPE_ENTRIES)

# else /* 32 bits PAE mode */
#  define SHW_PDPT_SHIFT        X86_PDPT_SHIFT
#  define SHW_PDPT_MASK         X86_PDPT_MASK_PAE
#  define SHW_PDPE_PG_MASK      X86_PDPE_PG_MASK
#  define SHW_TOTAL_PD_ENTRIES  (X86_PG_PAE_ENTRIES*X86_PG_PAE_PDPE_ENTRIES)
# endif
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN
/* r3 */
PGM_SHW_DECL(int, Enter)(PVMCPU pVCpu, bool fIs64BitsPagingMode);
PGM_SHW_DECL(int, Relocate)(PVMCPU pVCpu, RTGCPTR offDelta);
PGM_SHW_DECL(int, Exit)(PVMCPU pVCpu);

/* all */
PGM_SHW_DECL(int, GetPage)(PVMCPU pVCpu, RTGCPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys);
PGM_SHW_DECL(int, ModifyPage)(PVMCPU pVCpu, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask, uint32_t fOpFlags);
RT_C_DECLS_END


/**
 * Enters the shadow mode.
 *
 * @returns VBox status code.
 * @param   pVCpu                   The cross context virtual CPU structure.
 * @param   fIs64BitsPagingMode     New shadow paging mode is for 64 bits? (only relevant for 64 bits guests on a 32 bits AMD-V nested paging host)
 */
PGM_SHW_DECL(int, Enter)(PVMCPU pVCpu, bool fIs64BitsPagingMode)
{
#if PGM_TYPE_IS_NESTED_OR_EPT(PGM_SHW_TYPE)

# if PGM_TYPE_IS_NESTED(PGM_SHW_TYPE) && HC_ARCH_BITS == 32
    /* Must distinguish between 32 and 64 bits guest paging modes as we'll use
       a different shadow paging root/mode in both cases. */
    RTGCPHYS     GCPhysCR3 = (fIs64BitsPagingMode) ? RT_BIT_64(63) : RT_BIT_64(62);
# else
    RTGCPHYS     GCPhysCR3 = RT_BIT_64(63); NOREF(fIs64BitsPagingMode);
# endif
    PPGMPOOLPAGE pNewShwPageCR3;
    PVM          pVM       = pVCpu->pVMR3;

    Assert((HMIsNestedPagingActive(pVM) || VM_IS_NEM_ENABLED(pVM)) == pVM->pgm.s.fNestedPaging);
    Assert(pVM->pgm.s.fNestedPaging);
    Assert(!pVCpu->pgm.s.pShwPageCR3R3);

    pgmLock(pVM);

    int rc = pgmPoolAlloc(pVM, GCPhysCR3, PGMPOOLKIND_ROOT_NESTED, PGMPOOLACCESS_DONTCARE, PGM_A20_IS_ENABLED(pVCpu),
                          NIL_PGMPOOL_IDX, UINT32_MAX, true /*fLockPage*/,
                          &pNewShwPageCR3);
    AssertFatalRC(rc);

    pVCpu->pgm.s.pShwPageCR3R3 = pNewShwPageCR3;

    pVCpu->pgm.s.pShwPageCR3RC = MMHyperCCToRC(pVM, pVCpu->pgm.s.pShwPageCR3R3);
    pVCpu->pgm.s.pShwPageCR3R0 = MMHyperCCToR0(pVM, pVCpu->pgm.s.pShwPageCR3R3);

    pgmUnlock(pVM);

    Log(("Enter nested shadow paging mode: root %RHv phys %RHp\n", pVCpu->pgm.s.pShwPageCR3R3, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3)->Core.Key));
#else
    NOREF(pVCpu); NOREF(fIs64BitsPagingMode);
#endif
    return VINF_SUCCESS;
}


/**
 * Relocate any GC pointers related to shadow mode paging.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   offDelta    The relocation offset.
 */
PGM_SHW_DECL(int, Relocate)(PVMCPU pVCpu, RTGCPTR offDelta)
{
    pVCpu->pgm.s.pShwPageCR3RC += offDelta;
    return VINF_SUCCESS;
}


/**
 * Exits the shadow mode.
 *
 * @returns VBox status code.
 * @param   pVCpu       The cross context virtual CPU structure.
 */
PGM_SHW_DECL(int, Exit)(PVMCPU pVCpu)
{
#if PGM_TYPE_IS_NESTED_OR_EPT(PGM_SHW_TYPE)
    PVM pVM = pVCpu->pVMR3;
    if (pVCpu->pgm.s.CTX_SUFF(pShwPageCR3))
    {
        PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

        pgmLock(pVM);

        /* Do *not* unlock this page as we have two of them floating around in the 32-bit host & 64-bit guest case.
         * We currently assert when you try to free one of them; don't bother to really allow this.
         *
         * Note that this is two nested paging root pages max. This isn't a leak. They are reused.
         */
        /* pgmPoolUnlockPage(pPool, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3)); */

        pgmPoolFreeByPage(pPool, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3), NIL_PGMPOOL_IDX, UINT32_MAX);
        pVCpu->pgm.s.pShwPageCR3R3 = 0;
        pVCpu->pgm.s.pShwPageCR3R0 = 0;
        pVCpu->pgm.s.pShwPageCR3RC = 0;

        pgmUnlock(pVM);

        Log(("Leave nested shadow paging mode\n"));
    }
#else
    RT_NOREF_PV(pVCpu);
#endif
    return VINF_SUCCESS;
}

