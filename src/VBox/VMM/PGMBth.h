/* $Id$ */
/** @file
 * VBox - Page Manager / Monitor, Shadow+Guest Paging Template.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
PGM_BTH_DECL(int, InitData)(PVM pVM, PPGMMODEDATA pModeData, bool fResolveGCAndR0);
PGM_BTH_DECL(int, Enter)(PVM pVM, RTGCPHYS GCPhysCR3);
PGM_BTH_DECL(int, Relocate)(PVM pVM, RTGCUINTPTR offDelta);

PGM_BTH_DECL(int, Trap0eHandler)(PVM pVM, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
PGM_BTH_DECL(int, SyncCR3)(PVM pVM, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal);
PGM_BTH_DECL(int, SyncPage)(PVM pVM, X86PDE PdeSrc, RTGCUINTPTR GCPtrPage, unsigned cPages, unsigned uError);
PGM_BTH_DECL(int, VerifyAccessSyncPage)(PVM pVM, RTGCUINTPTR Addr, unsigned fPage, unsigned uError);
PGM_BTH_DECL(int, InvalidatePage)(PVM pVM, RTGCPTR GCPtrPage);
PGM_BTH_DECL(int, PrefetchPage)(PVM pVM, RTGCUINTPTR GCPtrPage);
PGM_BTH_DECL(unsigned, AssertCR3)(PVM pVM, uint64_t cr3, uint64_t cr4, RTGCUINTPTR GCPtr = 0, RTGCUINTPTR cb = ~(RTGCUINTPTR)0);
__END_DECLS


/**
 * Initializes the both bit of the paging mode data.
 *
 * @returns VBox status code.
 * @param   pVM             The VM handle.
 * @param   fResolveGCAndR0 Indicate whether or not GC and Ring-0 symbols can be resolved now.
 *                          This is used early in the init process to avoid trouble with PDM
 *                          not being initialized yet.
 */
PGM_BTH_DECL(int, InitData)(PVM pVM, PPGMMODEDATA pModeData, bool fResolveGCAndR0)
{
    Assert(pModeData->uShwType == PGM_SHW_TYPE); Assert(pModeData->uGstType == PGM_GST_TYPE);

    /* Ring 3 */
    pModeData->pfnR3BthRelocate          = PGM_BTH_NAME(Relocate);
    pModeData->pfnR3BthSyncCR3           = PGM_BTH_NAME(SyncCR3);
    pModeData->pfnR3BthTrap0eHandler     = PGM_BTH_NAME(Trap0eHandler);
    pModeData->pfnR3BthInvalidatePage    = PGM_BTH_NAME(InvalidatePage);
    pModeData->pfnR3BthSyncPage          = PGM_BTH_NAME(SyncPage);
    pModeData->pfnR3BthPrefetchPage      = PGM_BTH_NAME(PrefetchPage);
    pModeData->pfnR3BthVerifyAccessSyncPage = PGM_BTH_NAME(VerifyAccessSyncPage);
#ifdef VBOX_STRICT
    PGM_BTH_PFN(AssertCR3, pVM)         = PGM_BTH_NAME(AssertCR3);
#endif

    if (fResolveGCAndR0)
    {
        int rc;

        /* GC */
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(Trap0eHandler),  &pModeData->pfnGCBthTrap0eHandler);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(Trap0eHandler),  rc), rc);
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(InvalidatePage), &pModeData->pfnGCBthInvalidatePage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(InvalidatePage), rc), rc);
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(SyncCR3), &pModeData->pfnGCBthSyncCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(SyncPage), rc), rc);
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(SyncPage), &pModeData->pfnGCBthSyncPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(SyncPage), rc), rc);
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(PrefetchPage), &pModeData->pfnGCBthPrefetchPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(PrefetchPage), rc), rc);
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(VerifyAccessSyncPage), &pModeData->pfnGCBthVerifyAccessSyncPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(VerifyAccessSyncPage), rc), rc);
#ifdef VBOX_STRICT
        rc = PDMR3GetSymbolGC(pVM, NULL, PGM_BTH_NAME_GC_STR(AssertCR3), &pModeData->pfnGCBthAssertCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_GC_STR(AssertCR3), rc), rc);
#endif

        /* Ring 0 */
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(Trap0eHandler),  &pModeData->pfnR0BthTrap0eHandler);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(Trap0eHandler),  rc), rc);
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(InvalidatePage), &pModeData->pfnR0BthInvalidatePage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(InvalidatePage), rc), rc);
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(SyncCR3), &pModeData->pfnR0BthSyncCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(SyncCR3), rc), rc);
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(SyncPage), &pModeData->pfnR0BthSyncPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(SyncPage), rc), rc);
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(PrefetchPage), &pModeData->pfnR0BthPrefetchPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(PrefetchPage), rc), rc);
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(VerifyAccessSyncPage), &pModeData->pfnR0BthVerifyAccessSyncPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(VerifyAccessSyncPage), rc), rc);
#ifdef VBOX_STRICT
        rc = PDMR3GetSymbolR0(pVM, NULL, PGM_BTH_NAME_R0_STR(AssertCR3), &pModeData->pfnR0BthAssertCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Vrc\n", PGM_BTH_NAME_R0_STR(AssertCR3), rc), rc);
#endif
    }
    return VINF_SUCCESS;
}


/**
 * Enters the shadow+guest mode.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPhysCR3   The physical address from the CR3 register.
 */
PGM_BTH_DECL(int, Enter)(PVM pVM, RTGCPHYS GCPhysCR3)
{
    /* nothing special to do here - InitData does the job. */
    return VINF_SUCCESS;
}


/**
 * Relocate any GC pointers related to shadow mode paging.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   offDelta    The reloation offset.
 */
PGM_BTH_DECL(int, Relocate)(PVM pVM, RTGCUINTPTR offDelta)
{
    /* nothing special to do here - InitData does the job. */
    return VINF_SUCCESS;
}

