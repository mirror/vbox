/* $Id$ */
/** @file
 * VBox - Page Manager / Monitor, Guest Paging Template.
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
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
/* r3 */
PGM_GST_DECL(int, InitData)(PVM pVM, PPGMMODEDATA pModeData, bool fResolveGCAndR0);
PGM_GST_DECL(int, Enter)(PVM pVM, RTGCPHYS GCPhysCR3);
PGM_GST_DECL(int, Relocate)(PVM pVM, RTGCPTR offDelta);
PGM_GST_DECL(int, Exit)(PVM pVM);

#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
static DECLCALLBACK(int) pgmR3Gst32BitWriteHandlerCR3(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
static DECLCALLBACK(int) pgmR3GstPAEWriteHandlerCR3(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser);
#endif

/* all */
PGM_GST_DECL(int, GetPage)(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);
PGM_GST_DECL(int, ModifyPage)(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGM_GST_DECL(int, GetPDE)(PVM pVM, RTGCPTR GCPtr, PX86PDEPAE pPDE);
#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
PGM_GST_DECL(int, MonitorCR3)(PVM pVM, RTGCPHYS GCPhysCR3);
PGM_GST_DECL(int, UnmonitorCR3)(PVM pVM);
#endif
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
PGM_GST_DECL(int, InitData)(PVM pVM, PPGMMODEDATA pModeData, bool fResolveGCAndR0)
{
    Assert(pModeData->uGstType == PGM_GST_TYPE);

    /* Ring-3 */
    pModeData->pfnR3GstRelocate           = PGM_GST_NAME(Relocate);
    pModeData->pfnR3GstExit               = PGM_GST_NAME(Exit);
    pModeData->pfnR3GstGetPDE             = PGM_GST_NAME(GetPDE);
    pModeData->pfnR3GstGetPage            = PGM_GST_NAME(GetPage);
    pModeData->pfnR3GstModifyPage         = PGM_GST_NAME(ModifyPage);
#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
    pModeData->pfnR3GstMonitorCR3         = PGM_GST_NAME(MonitorCR3);
    pModeData->pfnR3GstUnmonitorCR3       = PGM_GST_NAME(UnmonitorCR3);
#endif

#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
# if PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_PAE
    pModeData->pfnR3GstWriteHandlerCR3    = PGM_GST_NAME(WriteHandlerCR3);
    pModeData->pszR3GstWriteHandlerCR3    = "Guest CR3 Write access handler";
    pModeData->pfnR3GstPAEWriteHandlerCR3 = PGM_GST_NAME(WriteHandlerCR3);
    pModeData->pszR3GstPAEWriteHandlerCR3 = "Guest CR3 Write access handler (PAE)";
# else
    pModeData->pfnR3GstWriteHandlerCR3    = NULL;
    pModeData->pszR3GstWriteHandlerCR3    = NULL;
    pModeData->pfnR3GstPAEWriteHandlerCR3 = NULL;
    pModeData->pszR3GstPAEWriteHandlerCR3 = NULL;
# endif
#endif

    if (fResolveGCAndR0)
    {
        int rc;

#if PGM_SHW_TYPE != PGM_TYPE_AMD64 /* No AMD64 for traditional virtualization, only VT-x and AMD-V. */
        /* GC */
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(GetPage),          &pModeData->pfnRCGstGetPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(GetPage),  rc), rc);
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(ModifyPage),       &pModeData->pfnRCGstModifyPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(ModifyPage),  rc), rc);
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(GetPDE),           &pModeData->pfnRCGstGetPDE);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(GetPDE), rc), rc);
# ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(MonitorCR3),       &pModeData->pfnRCGstMonitorCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(MonitorCR3), rc), rc);
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(UnmonitorCR3),     &pModeData->pfnRCGstUnmonitorCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(UnmonitorCR3), rc), rc);
# endif
# ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
#  if PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_PAE
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(WriteHandlerCR3),  &pModeData->pfnRCGstWriteHandlerCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(WriteHandlerCR3), rc), rc);
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       PGM_GST_NAME_RC_STR(WriteHandlerCR3),  &pModeData->pfnRCGstPAEWriteHandlerCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_RC_STR(WriteHandlerCR3), rc), rc);
#  endif
# endif
#endif /* Not AMD64 shadow paging. */

        /* Ring-0 */
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(GetPage),          &pModeData->pfnR0GstGetPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(GetPage),  rc), rc);
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(ModifyPage),       &pModeData->pfnR0GstModifyPage);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(ModifyPage),  rc), rc);
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(GetPDE),           &pModeData->pfnR0GstGetPDE);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(GetPDE), rc), rc);
#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(MonitorCR3),       &pModeData->pfnR0GstMonitorCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(MonitorCR3), rc), rc);
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(UnmonitorCR3),     &pModeData->pfnR0GstUnmonitorCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(UnmonitorCR3), rc), rc);
#endif
#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY
# if PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_PAE
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(WriteHandlerCR3),  &pModeData->pfnR0GstWriteHandlerCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(WriteHandlerCR3), rc), rc);
        rc = PDMR3LdrGetSymbolR0(pVM, NULL,       PGM_GST_NAME_R0_STR(WriteHandlerCR3),  &pModeData->pfnR0GstPAEWriteHandlerCR3);
        AssertMsgRCReturn(rc, ("%s -> rc=%Rrc\n", PGM_GST_NAME_R0_STR(WriteHandlerCR3), rc), rc);
# endif
#endif
    }

    return VINF_SUCCESS;
}


/**
 * Enters the guest mode.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPhysCR3   The physical address from the CR3 register.
 */
PGM_GST_DECL(int, Enter)(PVM pVM, RTGCPHYS GCPhysCR3)
{
    /*
     * Map and monitor CR3
     */
#ifdef VBOX_WITH_PGMPOOL_PAGING_ONLY
    int rc = PGM_BTH_PFN(MapCR3, pVM)(pVM, GCPhysCR3);
#else
    int rc = PGM_BTH_NAME(MapCR3)(pVM, GCPhysCR3);
    if (RT_SUCCESS(rc) && !pVM->pgm.s.fMappingsFixed)
        rc = PGM_GST_NAME(MonitorCR3)(pVM, GCPhysCR3);
#endif
    return rc;
}


/**
 * Relocate any GC pointers related to guest mode paging.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 * @param   offDelta    The reloation offset.
 */
PGM_GST_DECL(int, Relocate)(PVM pVM, RTGCPTR offDelta)
{
    /* nothing special to do here - InitData does the job. */
    return VINF_SUCCESS;
}


/**
 * Exits the guest mode.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
PGM_GST_DECL(int, Exit)(PVM pVM)
{
    int rc;

#ifdef VBOX_WITH_PGMPOOL_PAGING_ONLY
    rc = PGM_BTH_PFN(UnmapCR3, pVM)(pVM);
#else
    rc = PGM_GST_NAME(UnmonitorCR3)(pVM);
    if (RT_SUCCESS(rc))
        rc = PGM_BTH_NAME(UnmapCR3)(pVM);
#endif
    return rc;
}


#ifndef VBOX_WITH_PGMPOOL_PAGING_ONLY

#if PGM_GST_TYPE == PGM_TYPE_32BIT
/**
 * Physical write access for the Guest CR3 in 32-bit mode.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) pgmR3Gst32BitWriteHandlerCR3(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    AssertMsg(!pVM->pgm.s.fMappingsFixed, ("Shouldn't be registered when mappings are fixed!\n"));
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    Log2(("pgmR3Gst32BitWriteHandlerCR3: ff=%#x GCPhys=%RGp pvPhys=%p cbBuf=%d pvBuf={%.*Rhxs}\n", pVM->fForcedActions, GCPhys, pvPhys, cbBuf, cbBuf, pvBuf));

    /*
     * Do the write operation.
     */
    memcpy(pvPhys, pvBuf, cbBuf);
    if (    !pVM->pgm.s.fMappingsFixed
        &&  !VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL))
    {
        /*
         * Check for conflicts.
         */
        const RTGCPTR   offPD = GCPhys & PAGE_OFFSET_MASK;
        const unsigned  iPD1  = offPD / sizeof(X86PDE);
        const unsigned  iPD2  = (unsigned)(offPD + cbBuf - 1) / sizeof(X86PDE);
        Assert(iPD1 - iPD2 <= 1);
        if (    (   pVM->pgm.s.pGst32BitPdR3->a[iPD1].n.u1Present
                 && pgmGetMapping(pVM, iPD1 << X86_PD_SHIFT) )
            ||  (   iPD1 != iPD2
                 && pVM->pgm.s.pGst32BitPdR3->a[iPD2].n.u1Present
                 && pgmGetMapping(pVM, iPD2 << X86_PD_SHIFT) )
           )
        {
            Log(("pgmR3Gst32BitWriteHandlerCR3: detected conflict. iPD1=%#x iPD2=%#x GCPhys=%RGp\n", iPD1, iPD2, GCPhys));
            STAM_COUNTER_INC(&pVM->pgm.s.StatR3GuestPDWriteConflict);
            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
        }
    }

    STAM_COUNTER_INC(&pVM->pgm.s.StatR3GuestPDWrite);
    return VINF_SUCCESS;
}
#endif /* 32BIT */

#if PGM_GST_TYPE == PGM_TYPE_PAE

/**
 * Physical write access handler for the Guest CR3 in PAE mode.
 *
 * @returns VINF_SUCCESS if the handler have carried out the operation.
 * @returns VINF_PGM_HANDLER_DO_DEFAULT if the caller should carry out the access operation.
 * @param   pVM             VM Handle.
 * @param   GCPhys          The physical address the guest is writing to.
 * @param   pvPhys          The HC mapping of that address.
 * @param   pvBuf           What the guest is reading/writing.
 * @param   cbBuf           How much it's reading/writing.
 * @param   enmAccessType   The access type.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(int) pgmR3GstPAEWriteHandlerCR3(PVM pVM, RTGCPHYS GCPhys, void *pvPhys, void *pvBuf, size_t cbBuf, PGMACCESSTYPE enmAccessType, void *pvUser)
{
    AssertMsg(!pVM->pgm.s.fMappingsFixed, ("Shouldn't be registered when mappings are fixed!\n"));
    Assert(enmAccessType == PGMACCESSTYPE_WRITE);
    Log2(("pgmR3GstPAEWriteHandlerCR3: ff=%#x GCPhys=%RGp pvPhys=%p cbBuf=%d pvBuf={%.*Rhxs}\n", pVM->fForcedActions, GCPhys, pvPhys, cbBuf, cbBuf, pvBuf));

    /*
     * Do the write operation.
     */
    memcpy(pvPhys, pvBuf, cbBuf);
    if (    !pVM->pgm.s.fMappingsFixed
        &&  !VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL))
    {
        /*
         * Check if any of the PDs have changed.
         * We'll simply check all of them instead of figuring out which one/two to check.
         */
        for (unsigned i = 0; i < 4; i++)
        {
            if (    pVM->pgm.s.pGstPaePdptR3->a[i].n.u1Present
                &&  (pVM->pgm.s.pGstPaePdptR3->a[i].u & X86_PDPE_PG_MASK) != pVM->pgm.s.aGCPhysGstPaePDsMonitored[i])
            {
                Log(("pgmR3GstPAEWriteHandlerCR3: detected updated PDPE; [%d] = %#llx, Old GCPhys=%RGp\n",
                     i, pVM->pgm.s.pGstPaePdptR3->a[i].u, pVM->pgm.s.aGCPhysGstPaePDsMonitored[i]));
                /*
                 * The PD has changed.
                 * We will schedule a monitoring update for the next TLB Flush,
                 * InvalidatePage or SyncCR3.
                 *
                 * This isn't perfect, because a lazy page sync might be dealing with an half
                 * updated PDPE. However, we assume that the guest OS is disabling interrupts
                 * and being extremely careful (cmpxchg8b) when updating a PDPE where it's
                 * executing.
                 */
                pVM->pgm.s.fSyncFlags |= PGM_SYNC_MONITOR_CR3;
            }
        }
    }
    /*
     * Flag a updating of the monitor at the next crossroad so we don't monitor the
     * wrong pages for soo long that they can be reused as code pages and freak out
     * the recompiler or something.
     */
    else
        pVM->pgm.s.fSyncFlags |= PGM_SYNC_MONITOR_CR3;


    STAM_COUNTER_INC(&pVM->pgm.s.StatR3GuestPDWrite);
    return VINF_SUCCESS;
}

#endif /* PAE */
#endif /* !VBOX_WITH_PGMPOOL_PAGING_ONLY */
