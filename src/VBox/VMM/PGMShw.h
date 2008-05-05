/* $Id$ */
/** @file
 * VBox - Page Manager / Monitor, Shadow Paging Template.
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
# define SHW_POOL_ROOT_IDX      PGMPOOL_IDX_PAE_PD
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
/* r3 */
PGM_SHW_DECL(int, InitData)(PVM pVM, PPGMMODEDATA pModeData, bool fResolveGCAndR0);
PGM_SHW_DECL(int, Enter)(PVM pVM);
PGM_SHW_DECL(int, Relocate)(PVM pVM, RTGCUINTPTR offDelta);
PGM_SHW_DECL(int, Exit)(PVM pVM);

/* all */
PGM_SHW_DECL(int, GetPage)(PVM pVM, RTGCUINTPTR GCPtr, uint64_t *pfFlags, PRTHCPHYS pHCPhys);
PGM_SHW_DECL(int, ModifyPage)(PVM pVM, RTGCUINTPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
__END_DECLS


/**
 * Initializes the guest bit of the paging mode data.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   fResolveGCAndR0 Indicate whether or not GC and Ring-0 symbols can be resolved now.
 *                          This is used early in the init process to avoid trouble with PDM
 *                          not being initialized yet.
 */
PGM_SHW_DECL(int, InitData)(PVM pVM, PPGMMODEDATA pModeData, bool fResolveGCAndR0)
{
    Assert(pModeData->uShwType == PGM_SHW_TYPE);

    /* Ring-3 */
    pModeData->pfnR3ShwRelocate          = PGM_SHW_NAME(Relocate);
    pModeData->pfnR3ShwExit              = PGM_SHW_NAME(Exit);
    pModeData->pfnR3ShwGetPage           = PGM_SHW_NAME(GetPage);
    pModeData->pfnR3ShwModifyPage        = PGM_SHW_NAME(ModifyPage);

    if (fResolveGCAndR0)
    {
        int rc;

#if PGM_SHW_TYPE != PGM_TYPE_AMD64 /* No AMD64 for traditional virtualization, only VT-x and AMD-V. */
        /* GC */
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_SHW_NAME_GC_STR(GetPage),  &pModeData->pfnGCShwGetPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_SHW_NAME_GC_STR(GetPage),  rc), rc);
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_SHW_NAME_GC_STR(ModifyPage),  &pModeData->pfnGCShwModifyPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_SHW_NAME_GC_STR(ModifyPage),  rc), rc);
#endif /* Not AMD64 shadow paging. */

        /* Ring-0 */
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_SHW_NAME_R0_STR(GetPage),  &pModeData->pfnR0ShwGetPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_SHW_NAME_R0_STR(GetPage),  rc), rc);
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_SHW_NAME_R0_STR(ModifyPage),  &pModeData->pfnR0ShwModifyPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_SHW_NAME_R0_STR(ModifyPage),  rc), rc);
    }
    return VINF_SUCCESS;
}

/**
 * Enters the shadow mode.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
PGM_SHW_DECL(int, Enter)(PVM pVM)
{
#if PGM_SHW_MODE == PGM_MODE_AMD64
    /*
     * Set the RW, US and A flags for the fixed PDPEs.
     */
#endif
    return VINF_SUCCESS;
}


/**
 * Relocate any GC pointers related to shadow mode paging.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   offDelta    The reloation offset.
 */
PGM_SHW_DECL(int, Relocate)(PVM pVM, RTGCUINTPTR offDelta)
{
    /* nothing special to do here - InitData does the job. */
    return VINF_SUCCESS;
}


/**
 * Exits the shadow mode.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
PGM_SHW_DECL(int, Exit)(PVM pVM)
{
#if PGM_SHW_MODE == PGM_MODE_AMD64
    /*
     * Clear the RW, US and A flags for the fixed PDPEs.
     */
#endif

    return VINF_SUCCESS;
}



