/* $Id$ */
/** @file
 * VBox - Page Manager, Shadow+Guest Paging Template - All context code.
 *
 * This file is a big challenge!
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
PGM_BTH_DECL(int, Trap0eHandler)(PVM pVM, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
PGM_BTH_DECL(int, InvalidatePage)(PVM pVM, RTGCUINTPTR GCPtrPage);
PGM_BTH_DECL(int, SyncPage)(PVM pVM, GSTPDE PdeSrc, RTGCUINTPTR GCPtrPage, unsigned cPages, unsigned uErr);
PGM_BTH_DECL(int, CheckPageFault)(PVM pVM, uint32_t uErr, PSHWPDE pPdeDst, PGSTPDE pPdeSrc, RTGCUINTPTR GCPtrPage);
PGM_BTH_DECL(int, SyncPT)(PVM pVM, unsigned iPD, PGSTPD pPDSrc, RTGCUINTPTR GCPtrPage);
PGM_BTH_DECL(int, VerifyAccessSyncPage)(PVM pVM, RTGCUINTPTR Addr, unsigned fPage, unsigned uErr);
PGM_BTH_DECL(int, PrefetchPage)(PVM pVM, RTGCUINTPTR GCPtrPage);
PGM_BTH_DECL(int, SyncCR3)(PVM pVM, uint32_t cr0, uint32_t cr3, uint32_t cr4, bool fGlobal);
#ifdef VBOX_STRICT
PGM_BTH_DECL(unsigned, AssertCR3)(PVM pVM, uint32_t cr3, uint32_t cr4, RTGCUINTPTR GCPtr = 0, RTGCUINTPTR cb = ~(RTGCUINTPTR)0);
#endif
#ifdef PGMPOOL_WITH_USER_TRACKING
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorkerTrackDeref)(PVM pVM, PPGMPOOLPAGE pShwPage, RTHCPHYS HCPhys);
#endif
__END_DECLS


/**
 * #PF Handler for raw-mode guest execution.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErr        The trap error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address.
 */
PGM_BTH_DECL(int, Trap0eHandler)(PVM pVM, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault)
{
#if (PGM_GST_TYPE == PGM_TYPE_32BIT ||  PGM_GST_TYPE == PGM_TYPE_REAL ||  PGM_GST_TYPE == PGM_TYPE_PROT || PGM_GST_TYPE == PGM_TYPE_PAE) && PGM_SHW_TYPE != PGM_TYPE_AMD64

# if PGM_SHW_TYPE != PGM_TYPE_32BIT && PGM_SHW_TYPE != PGM_TYPE_PAE
#  error "32-bit guest mode is only implemented for 32-bit and PAE shadow modes."
# endif

# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE != PGM_TYPE_PAE
    /*
     * Hide the instruction fetch trap indicator for now.
     */
    /** @todo NXE will change this and we must fix NXE in the switcher too! */
    if (uErr & X86_TRAP_PF_ID)
    {
        uErr &= ~X86_TRAP_PF_ID;
        TRPMSetErrorCode(pVM, uErr);
    }
# endif

    /*
     * Get PDs.
     */
    int       rc;
# if PGM_WITH_PAGING(PGM_GST_TYPE)
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDSrc = (RTGCUINTPTR)pvFault >> GST_PD_SHIFT;
    PGSTPD          pPDSrc = CTXSUFF(pVM->pgm.s.pGuestPD);
#  else /* PAE */
    unsigned        iPDSrc;
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVM->pgm.s, (RTGCUINTPTR)pvFault, &iPDSrc);
#  endif
# else
    PGSTPD          pPDSrc = NULL;
    const unsigned  iPDSrc = 0;
# endif

    const unsigned  iPDDst = (RTGCUINTPTR)pvFault >> SHW_PD_SHIFT;
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PD          pPDDst = pVM->pgm.s.CTXMID(p,32BitPD);
# else /* PAE */
    PX86PDPAE       pPDDst = pVM->pgm.s.CTXMID(ap,PaePDs)[0]; /* We treat this as a PD with 2048 entries. */
# endif

# if PGM_WITH_PAGING(PGM_GST_TYPE)
#  ifdef PGM_SYNC_DIRTY_BIT
    /*
     * If we successfully correct the write protection fault due to dirty bit
     * tracking, or this page fault is a genuine one, then return immediately.
     */
    STAM_PROFILE_START(&pVM->pgm.s.StatCheckPageFault, e);
    rc = PGM_BTH_NAME(CheckPageFault)(pVM, uErr, &pPDDst->a[iPDDst], &pPDSrc->a[iPDSrc], (RTGCUINTPTR)pvFault);
    STAM_PROFILE_STOP(&pVM->pgm.s.StatCheckPageFault, e);
    if (    rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT
        ||  rc == VINF_EM_RAW_GUEST_TRAP)
    {
        STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution)
                     = rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT ? &pVM->pgm.s.StatTrap0eDirtyAndAccessedBits : &pVM->pgm.s.StatTrap0eGuestTrap; });
        LogBird(("Trap0eHandler: returns %s\n", rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT ? "VINF_SUCCESS" : "VINF_EM_RAW_GUEST_TRAP"));
        return rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT ? VINF_SUCCESS : rc;
    }
#  endif

    STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0ePD[iPDSrc]);
# endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */

    /*
     * A common case is the not-present error caused by lazy page table syncing.
     *
     * It is IMPORTANT that we weed out any access to non-present shadow PDEs here
     * so we can safely assume that the shadow PT is present when calling SyncPage later.
     *
     * On failure, we ASSUME that SyncPT is out of memory or detected some kind
     * of mapping conflict and defer to SyncCR3 in R3.
     * (Again, we do NOT support access handlers for non-present guest pages.)
     *
     */
# if PGM_WITH_PAGING(PGM_GST_TYPE)
    GSTPDE PdeSrc = pPDSrc->a[iPDSrc];
# else
    GSTPDE PdeSrc;
    PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
    PdeSrc.n.u1Present  = 1;
    PdeSrc.n.u1Write    = 1;
    PdeSrc.n.u1Accessed = 1;
    PdeSrc.n.u1User     = 1;
# endif
    if (    !(uErr & X86_TRAP_PF_P) /* not set means page not present instead of page protection violation */
        &&  !pPDDst->a[iPDDst].n.u1Present
        &&  PdeSrc.n.u1Present
       )

    {
        STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eSyncPT; });
        STAM_PROFILE_START(&pVM->pgm.s.StatLazySyncPT, f);
        LogFlow(("=>SyncPT %04x = %08x\n", iPDSrc, PdeSrc.au32[0]));
        rc = PGM_BTH_NAME(SyncPT)(pVM, iPDSrc, pPDSrc, (RTGCUINTPTR)pvFault);
        if (VBOX_SUCCESS(rc))
        {
            STAM_PROFILE_STOP(&pVM->pgm.s.StatLazySyncPT, f);
            return rc;
        }
        Log(("SyncPT: %d failed!! rc=%d\n", iPDSrc, rc));
        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3); /** @todo no need to do global sync, right? */
        STAM_PROFILE_STOP(&pVM->pgm.s.StatLazySyncPT, f);
        return VINF_PGM_SYNC_CR3;
    }

# if PGM_WITH_PAGING(PGM_GST_TYPE)
    /*
     * Check if this address is within any of our mappings.
     *
     * This is *very* fast and it's gonna save us a bit of effort below and prevent
     * us from screwing ourself with MMIO2 pages which have a GC Mapping (VRam).
     * (BTW, it's impossible to have physical access handlers in a mapping.)
     */
    if (pgmMapAreMappingsEnabled(&pVM->pgm.s))
    {
        STAM_PROFILE_START(&pVM->pgm.s.StatMapping, a);
        PPGMMAPPING pMapping = CTXALLSUFF(pVM->pgm.s.pMappings);
        for ( ; pMapping; pMapping = CTXALLSUFF(pMapping->pNext))
        {
            if ((RTGCUINTPTR)pvFault < (RTGCUINTPTR)pMapping->GCPtr)
                break;
            if ((RTGCUINTPTR)pvFault - (RTGCUINTPTR)pMapping->GCPtr < pMapping->cb)
            {
                /*
                 * The first thing we check is if we've got an undetected conflict.
                 */
                if (!pVM->pgm.s.fMappingsFixed)
                {
                    unsigned iPT = pMapping->cPTs;
                    while (iPT-- > 0)
                        if (pPDSrc->a[iPDSrc + iPT].n.u1Present)
                        {
                            STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eConflicts);
                            Log(("Trap0e: Detected Conflict %VGv-%VGv\n", pMapping->GCPtr, pMapping->GCPtrLast));
                            VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3); /** @todo no need to do global sync,right? */
                            STAM_PROFILE_STOP(&pVM->pgm.s.StatMapping, a);
                            return VINF_PGM_SYNC_CR3;
                        }
                }

                /*
                 * Check if the fault address is in a virtual page access handler range.
                 */
                PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRangeGet(&CTXSUFF(pVM->pgm.s.pTrees)->HyperVirtHandlers, pvFault);
                if (    pCur
                    &&  (RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr < pCur->cb
                    &&  uErr & X86_TRAP_PF_RW)
                {
#  ifdef IN_GC
                    STAM_PROFILE_START(&pCur->Stat, h);
                    rc = CTXSUFF(pCur->pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->GCPtr, (RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr);
                    STAM_PROFILE_STOP(&pCur->Stat, h);
#  else
                    AssertFailed();
                    rc = VINF_EM_RAW_EMULATE_INSTR; /* can't happen with VMX */
#  endif
                    STAM_COUNTER_INC(&pVM->pgm.s.StatTrap0eMapHandler);
                    STAM_PROFILE_STOP(&pVM->pgm.s.StatMapping, a);
                    return rc;
                }

                /*
                 * Pretend we're not here and let the guest handle the trap.
                 */
                TRPMSetErrorCode(pVM, uErr & ~X86_TRAP_PF_P);
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eMap);
                LogFlow(("PGM: Mapping access -> route trap to recompiler!\n"));
                STAM_PROFILE_STOP(&pVM->pgm.s.StatMapping, a);
                return VINF_EM_RAW_GUEST_TRAP;
            }
        }
        STAM_PROFILE_STOP(&pVM->pgm.s.StatMapping, a);
    } /* pgmAreMappingsEnabled(&pVM->pgm.s) */
# endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */

    /*
     * Check if this fault address is flagged for special treatment,
     * which means we'll have to figure out the physical address and
     * check flags associated with it.
     *
     * ASSUME that we can limit any special access handling to pages
     * in page tables which the guest believes to be present.
     */
    if (PdeSrc.n.u1Present)
    {
        RTGCPHYS    GCPhys = ~0U;

# if PGM_WITH_PAGING(PGM_GST_TYPE)
        uint32_t    cr4 = CPUMGetGuestCR4(pVM);
        if (    PdeSrc.b.u1Size
            &&  (cr4 & X86_CR4_PSE))
            GCPhys = (PdeSrc.u & GST_PDE4M_PG_MASK)
                    | ((RTGCPHYS)pvFault & (GST_BIG_PAGE_OFFSET_MASK ^ PAGE_OFFSET_MASK));
        else
        {
            PVBOXPT pPTSrc;
#  ifdef IN_GC
            rc = PGMGCDynMapGCPage(pVM, PdeSrc.u & GST_PDE_PG_MASK, (void **)&pPTSrc);
#  else
            pPTSrc = (PVBOXPT)MMPhysGCPhys2HCVirt(pVM, PdeSrc.u & GST_PDE_PG_MASK, sizeof(*pPTSrc));
            if (pPTSrc == 0)
                rc = VERR_PGM_INVALID_GC_PHYSICAL_ADDRESS;
#  endif
            if (VBOX_SUCCESS(rc))
            {
                unsigned iPTESrc = ((RTGCUINTPTR)pvFault >> PAGE_SHIFT) & GST_PT_MASK;
                if (pPTSrc->a[iPTESrc].n.u1Present)
                    GCPhys = pPTSrc->a[iPTESrc].u & GST_PTE_PG_MASK;
            }
        }
# else
        /* No paging so the fault address is the physical address */
        GCPhys = (RTGCPHYS)((RTGCUINTPTR)pvFault & ~PAGE_OFFSET_MASK);
# endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */

        /*
         * If we have a GC address we'll check if it has any flags set.
         */
        if (GCPhys != ~0U)
        {
            STAM_PROFILE_START(&pVM->pgm.s.StatHandlers, b);

            PPGMPAGE pPage;
            rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
            if (VBOX_SUCCESS(rc))
            {
                if (PGM_PAGE_HAVE_ANY_HANDLERS(pPage))
                {
                    if (PGM_PAGE_HAVE_ANY_PHYSICAL_HANDLERS(pPage))
                    {
                        /*
                         * Physical page access handler.
                         */
                        const RTGCPHYS  GCPhysFault = GCPhys | ((RTGCUINTPTR)pvFault & PAGE_OFFSET_MASK);
                        PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&CTXSUFF(pVM->pgm.s.pTrees)->PhysHandlers, GCPhysFault);
                        if (pCur)
                        {
# ifdef PGM_SYNC_N_PAGES
                            /*
                             * If the region is write protected and we got a page not present fault, then sync
                             * the pages. If the fault was caused by a read, then restart the instruction.
                             * In case of write access continue to the GC write handler.
                             *
                             * ASSUMES that there is only one handler per page or that they have similar write properties.
                             */
                            if (    pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE
                                && !(uErr & X86_TRAP_PF_P))
                            {
                                rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)pvFault, PGM_SYNC_NR_PAGES, uErr);
                                if (    VBOX_FAILURE(rc)
                                    || !(uErr & X86_TRAP_PF_RW)
                                    || rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE)
                                {
                                    AssertRC(rc);
                                    STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersOutOfSync);
                                    STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                                    STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eOutOfSyncHndPhys; });
                                    return rc;
                                }
                            }
# endif

                            AssertMsg(   pCur->enmType != PGMPHYSHANDLERTYPE_PHYSICAL_WRITE
                                      || (pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE && (uErr & X86_TRAP_PF_RW)),
                                      ("Unexpected trap for physical handler: %08X (phys=%08x) HCPhys=%X uErr=%X, enum=%d\n", pvFault, GCPhys, pPage->HCPhys, uErr, pCur->enmType));

#if defined(IN_GC) || defined(IN_RING0)
                            if (CTXALLSUFF(pCur->pfnHandler))
                            {
                                STAM_PROFILE_START(&pCur->Stat, h);
                                rc = pCur->CTXALLSUFF(pfnHandler)(pVM, uErr, pRegFrame, pvFault, GCPhysFault, CTXALLSUFF(pCur->pvUser));
                                STAM_PROFILE_STOP(&pCur->Stat, h);
                            }
                            else
#endif
                                rc = VINF_EM_RAW_EMULATE_INSTR;
                            STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersPhysical);
                            STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                            STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eHndPhys; });
                            return rc;
                        }
                    }
# if PGM_WITH_PAGING(PGM_GST_TYPE)
                    else
                    {
# ifdef PGM_SYNC_N_PAGES
                        /*
                         * If the region is write protected and we got a page not present fault, then sync
                         * the pages. If the fault was caused by a read, then restart the instruction.
                         * In case of write access continue to the GC write handler.
                         */
                        if (    PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) < PGM_PAGE_HNDL_PHYS_STATE_ALL
                            && !(uErr & X86_TRAP_PF_P))
                        {
                            rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)pvFault, PGM_SYNC_NR_PAGES, uErr);
                            if (    VBOX_FAILURE(rc)
                                ||  rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE
                                ||  !(uErr & X86_TRAP_PF_RW))
                            {
                                AssertRC(rc);
                                STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersOutOfSync);
                                STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                                STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eOutOfSyncHndVirt; });
                                return rc;
                            }
                        }
# endif
                        /*
                         * Ok, it's an virtual page access handler.
                         *
                         * Since it's faster to search by address, we'll do that first
                         * and then retry by GCPhys if that fails.
                         */
                        /** @todo r=bird: perhaps we should consider looking up by physical address directly now? */
                        /** @note r=svl: true, but lookup on virtual address should remain as a fallback as phys & virt trees might be out of sync, because the
                          *              page was changed without us noticing it (not-present -> present without invlpg or mov cr3, xxx)
                          */
                        PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRangeGet(&CTXSUFF(pVM->pgm.s.pTrees)->VirtHandlers, pvFault);
                        if (pCur)
                        {
                            AssertMsg(!((RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr < pCur->cb)
                                      || (     pCur->enmType != PGMVIRTHANDLERTYPE_WRITE
                                           || !(uErr & X86_TRAP_PF_P)
                                           || (pCur->enmType == PGMVIRTHANDLERTYPE_WRITE && (uErr & X86_TRAP_PF_RW))),
                                      ("Unexpected trap for virtual handler: %VGv (phys=%VGp) HCPhys=%HGp uErr=%X, enum=%d\n", pvFault, GCPhys, pPage->HCPhys, uErr, pCur->enmType));

                            if (    (RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr < pCur->cb
                                &&  (    uErr & X86_TRAP_PF_RW
                                     ||  pCur->enmType != PGMVIRTHANDLERTYPE_WRITE ) )
                            {
#  ifdef IN_GC
                                STAM_PROFILE_START(&pCur->Stat, h);
                                rc = CTXSUFF(pCur->pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->GCPtr, (RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr);
                                STAM_PROFILE_STOP(&pCur->Stat, h);
#  else
                                rc = VINF_EM_RAW_EMULATE_INSTR; /** @todo for VMX */
#  endif
                                STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersVirtual);
                                STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                                STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eHndVirt; });
                                return rc;
                            }
                            /* Unhandled part of a monitored page */
                        }
                        else
                        {
                           /* Check by physical address. */
                            PPGMVIRTHANDLER pCur;
                            unsigned        iPage;
                            rc = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys + ((RTGCUINTPTR)pvFault & PAGE_OFFSET_MASK),
                                                                 &pCur, &iPage);
                            Assert(VBOX_SUCCESS(rc) || !pCur);
                            if (    pCur
                                &&  (   uErr & X86_TRAP_PF_RW
                                     || pCur->enmType != PGMVIRTHANDLERTYPE_WRITE ) )
                            {
                                Assert((pCur->aPhysToVirt[iPage].Core.Key & X86_PTE_PAE_PG_MASK) == GCPhys);
#  ifdef IN_GC
                                RTGCUINTPTR off = (iPage << PAGE_SHIFT) + ((RTGCUINTPTR)pvFault & PAGE_OFFSET_MASK) - ((RTGCUINTPTR)pCur->GCPtr & PAGE_OFFSET_MASK);
                                Assert(off < pCur->cb);
                                STAM_PROFILE_START(&pCur->Stat, h);
                                rc = CTXSUFF(pCur->pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->GCPtr, off);
                                STAM_PROFILE_STOP(&pCur->Stat, h);
#  else
                                rc = VINF_EM_RAW_EMULATE_INSTR; /** @todo for VMX */
#  endif
                                STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersVirtualByPhys);
                                STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                                STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eHndVirt; });
                                return rc;
                            }
                        }
                    }
# endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */

                    /*
                     * There is a handled area of the page, but this fault doesn't belong to it.
                     * We must emulate the instruction.
                     *
                     * To avoid crashing (non-fatal) in the interpreter and go back to the recompiler
                     * we first check if this was a page-not-present fault for a page with only
                     * write access handlers. Restart the instruction if it wasn't a write access.
                     */
                    STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersUnhandled);

                    if (    !PGM_PAGE_HAVE_ACTIVE_ALL_HANDLERS(pPage)
                        &&  !(uErr & X86_TRAP_PF_P))
                    {
                        rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)pvFault, PGM_SYNC_NR_PAGES, uErr);
                        if (    VBOX_FAILURE(rc)
                            ||  rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE
                            ||  !(uErr & X86_TRAP_PF_RW))
                        {
                            AssertRC(rc);
                            STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersOutOfSync);
                            STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                            STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eOutOfSyncHndPhys; });
                            return rc;
                        }
                    }

                    /** @todo This particular case can cause quite a lot of overhead. E.g. early stage of kernel booting in Ubuntu 6.06
                     *        It's writing to an unhandled part of the LDT page several million times.
                     */
                    rc = PGMInterpretInstruction(pVM, pRegFrame, pvFault);
                    LogFlow(("PGM: PGMInterpretInstruction -> rc=%d HCPhys=%RHp%s%s\n",
                             rc, pPage->HCPhys,
                             PGM_PAGE_HAVE_ANY_PHYSICAL_HANDLERS(pPage) ? " phys" : "",
                             PGM_PAGE_HAVE_ANY_VIRTUAL_HANDLERS(pPage)  ? " virt" : ""));
                    STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                    STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eHndUnhandled; });
                    return rc;
                } /* if any kind of handler */

# if PGM_WITH_PAGING(PGM_GST_TYPE)
                if (uErr & X86_TRAP_PF_P)
                {
                    /*
                     * The page isn't marked, but it might still be monitored by a virtual page access handler.
                     * (ASSUMES no temporary disabling of virtual handlers.)
                     */
                    /** @todo r=bird: Since the purpose is to catch out of sync pages with virtual handler(s) here,
                     * we should correct both the shadow page table and physical memory flags, and not only check for
                     * accesses within the handler region but for access to pages with virtual handlers. */
                    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRangeGet(&CTXSUFF(pVM->pgm.s.pTrees)->VirtHandlers, pvFault);
                    if (pCur)
                    {
                        AssertMsg(   !((RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr < pCur->cb)
                                  || (    pCur->enmType != PGMVIRTHANDLERTYPE_WRITE
                                       || !(uErr & X86_TRAP_PF_P)
                                       || (pCur->enmType == PGMVIRTHANDLERTYPE_WRITE && (uErr & X86_TRAP_PF_RW))),
                                  ("Unexpected trap for virtual handler: %08X (phys=%08x) HCPhys=%X uErr=%X, enum=%d\n", pvFault, GCPhys, pPage->HCPhys, uErr, pCur->enmType));

                        if (    (RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr < pCur->cb
                            &&  (    uErr & X86_TRAP_PF_RW
                                 ||  pCur->enmType != PGMVIRTHANDLERTYPE_WRITE ) )
                        {
#  ifdef IN_GC
                            STAM_PROFILE_START(&pCur->Stat, h);
                            rc = CTXSUFF(pCur->pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->GCPtr, (RTGCUINTPTR)pvFault - (RTGCUINTPTR)pCur->GCPtr);
                            STAM_PROFILE_STOP(&pCur->Stat, h);
#  else
                            rc = VINF_EM_RAW_EMULATE_INSTR; /** @todo for VMX */
#  endif
                            STAM_COUNTER_INC(&pVM->pgm.s.StatHandlersVirtualUnmarked);
                            STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);
                            STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eHndVirt; });
                            return rc;
                        }
                    }
                }
# endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */
            }
            STAM_PROFILE_STOP(&pVM->pgm.s.StatHandlers, b);

# ifdef PGM_OUT_OF_SYNC_IN_GC
            /*
             * We are here only if page is present in Guest page tables and trap is not handled
             * by our handlers.
             * Check it for page out-of-sync situation.
             */
            STAM_PROFILE_START(&pVM->pgm.s.StatOutOfSync, c);

            if (!(uErr & X86_TRAP_PF_P))
            {
                /*
                 * Page is not present in our page tables.
                 * Try to sync it!
                 * BTW, fPageShw is invalid in this branch!
                 */
                if (uErr & X86_TRAP_PF_US)
                    STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageOutOfSyncUser);
                else /* supervisor */
                    STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageOutOfSyncSupervisor);

#  if defined(LOG_ENABLED) && !defined(IN_RING0)
                RTGCPHYS   GCPhys;
                uint64_t   fPageGst;
                PGMGstGetPage(pVM, pvFault, &fPageGst, &GCPhys);
                Log(("Page out of sync: %p eip=%08x PdeSrc.n.u1User=%d fPageGst=%08llx GCPhys=%VGp scan=%d\n",
                     pvFault, pRegFrame->eip, PdeSrc.n.u1User, fPageGst, GCPhys, CSAMDoesPageNeedScanning(pVM, (RTGCPTR)pRegFrame->eip)));
#  endif /* LOG_ENABLED */

#  if PGM_WITH_PAGING(PGM_GST_TYPE) && !defined(IN_RING0)
                if (CPUMGetGuestCPL(pVM, pRegFrame) == 0)
                {
                    uint64_t fPageGst;
                    rc = PGMGstGetPage(pVM, pvFault, &fPageGst, NULL);
                    if (    VBOX_SUCCESS(rc)
                        && !(fPageGst & X86_PTE_US))
                    {
                        /* Note: can't check for X86_TRAP_ID bit, because that requires execute disable support on the CPU */
                        if (    pvFault == (RTGCPTR)pRegFrame->eip
                            ||  (RTGCUINTPTR)pvFault - pRegFrame->eip < 8    /* instruction crossing a page boundary */
#   ifdef CSAM_DETECT_NEW_CODE_PAGES
                            ||  (   !PATMIsPatchGCAddr(pVM, (RTGCPTR)pRegFrame->eip)
                                 && CSAMDoesPageNeedScanning(pVM, (RTGCPTR)pRegFrame->eip))   /* any new code we encounter here */
#   endif /* CSAM_DETECT_NEW_CODE_PAGES */
                           )
                        {
                            LogFlow(("CSAMExecFault %VGv\n", pRegFrame->eip));
                            rc = CSAMExecFault(pVM, (RTGCPTR)pRegFrame->eip);
                            if (rc != VINF_SUCCESS)
                            {
                                /*
                                 * CSAM needs to perform a job in ring 3.
                                 *
                                 * Sync the page before going to the host context; otherwise we'll end up in a loop if
                                 * CSAM fails (e.g. instruction crosses a page boundary and the next page is not present)
                                 */
                                LogFlow(("CSAM ring 3 job\n"));
                                int rc2 = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)pvFault, 1, uErr);
                                AssertRC(rc2);

                                STAM_PROFILE_STOP(&pVM->pgm.s.StatOutOfSync, c);
                                STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eCSAM; });
                                return rc;
                            }
                        }
#   ifdef CSAM_DETECT_NEW_CODE_PAGES
                        else
                        if (    uErr == X86_TRAP_PF_RW
                            &&  pRegFrame->ecx >= 0x100         /* early check for movswd count */
                            &&  pRegFrame->ecx < 0x10000
                           )
                        {
                            /* In case of a write to a non-present supervisor shadow page, we'll take special precautions
                             * to detect loading of new code pages.
                             */

                            /*
                             * Decode the instruction.
                             */
                            RTGCPTR PC;
                            rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &PC);
                            if (rc == VINF_SUCCESS)
                            {
                                DISCPUSTATE Cpu;
                                uint32_t    cbOp;
                                rc = EMInterpretDisasOneEx(pVM, (RTGCUINTPTR)PC, pRegFrame, &Cpu, &cbOp);

                                /* For now we'll restrict this to rep movsw/d instructions */
                                if (    rc == VINF_SUCCESS
                                    &&  Cpu.pCurInstr->opcode == OP_MOVSWD
                                    &&  (Cpu.prefix & PREFIX_REP))
                                {
                                    CSAMMarkPossibleCodePage(pVM, pvFault);
                                }
                            }
                        }
#   endif  /* CSAM_DETECT_NEW_CODE_PAGES */

                        /*
                         * Mark this page as safe.
                         */
                        /** @todo not correct for pages that contain both code and data!! */
                        Log2(("CSAMMarkPage %p; scanned=%d\n", pvFault, true));
                        CSAMMarkPage(pVM, pvFault, true);
                    }
                }
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */
                rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)pvFault, PGM_SYNC_NR_PAGES, uErr);
                if (VBOX_SUCCESS(rc))
                {
                    /* The page was successfully synced, return to the guest. */
                    STAM_PROFILE_STOP(&pVM->pgm.s.StatOutOfSync, c);
                    STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eOutOfSync; });
                    return VINF_SUCCESS;
                }
            }
            else
            {
                /*
                 * A side effect of not flushing global PDEs are out of sync pages due
                 * to physical monitored regions, that are no longer valid.
                 * Assume for now it only applies to the read/write flag
                 */
                if (VBOX_SUCCESS(rc) && (uErr & X86_TRAP_PF_RW))
                {
                    if (uErr & X86_TRAP_PF_US)
                        STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageOutOfSyncUser);
                    else /* supervisor */
                        STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageOutOfSyncSupervisor);


                    /*
                     * Note: Do NOT use PGM_SYNC_NR_PAGES here. That only works if the page is not present, which is not true in this case.
                     */
                    rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)pvFault, 1, uErr);
                    if (VBOX_SUCCESS(rc))
                    {
                       /*
                        * Page was successfully synced, return to guest.
                        */
#  ifdef VBOX_STRICT
                        RTGCPHYS GCPhys;
                        uint64_t fPageGst;
                        rc = PGMGstGetPage(pVM, pvFault, &fPageGst, &GCPhys);
                        Assert(VBOX_SUCCESS(rc) && fPageGst & X86_PTE_RW);
                        LogFlow(("Obsolete physical monitor page out of sync %VGv - phys %VGp flags=%08llx\n", pvFault, GCPhys, (uint64_t)fPageGst));

                        uint64_t fPageShw;
                        rc = PGMShwGetPage(pVM, pvFault, &fPageShw, NULL);
                        Assert(VBOX_SUCCESS(rc) && fPageShw & X86_PTE_RW);
#  endif /* VBOX_STRICT */
                        STAM_PROFILE_STOP(&pVM->pgm.s.StatOutOfSync, c);
                        STAM_STATS({ pVM->pgm.s.CTXSUFF(pStatTrap0eAttribution) = &pVM->pgm.s.StatTrap0eOutOfSyncObsHnd; });
                        return VINF_SUCCESS;
                    }
                }

#  if PGM_WITH_PAGING(PGM_GST_TYPE)
#   ifdef VBOX_STRICT
                /*
                 * Check for VMM page flags vs. Guest page flags consistency.
                 * Currently only for debug purposes.
                 */
                if (VBOX_SUCCESS(rc))
                {
                    /* Get guest page flags. */
                    uint64_t fPageGst;
                    rc = PGMGstGetPage(pVM, pvFault, &fPageGst, NULL);
                    if (VBOX_SUCCESS(rc))
                    {
                        uint64_t fPageShw;
                        rc = PGMShwGetPage(pVM, pvFault, &fPageShw, NULL);

                        /*
                         * Compare page flags.
                         * Note: we have AVL, A, D bits desynched.
                         */
                        AssertMsg((fPageShw & ~(X86_PTE_A | X86_PTE_D | X86_PTE_AVL_MASK)) == (fPageGst & ~(X86_PTE_A | X86_PTE_D | X86_PTE_AVL_MASK)),
                                  ("Page flags mismatch! pvFault=%p GCPhys=%VGp fPageShw=%08llx fPageGst=%08llx\n", pvFault, GCPhys, fPageShw, fPageGst));
                    }
                    else
                        AssertMsgFailed(("PGMGstGetPage rc=%Vrc\n", rc));
                }
                else
                    AssertMsgFailed(("PGMGCGetPage rc=%Vrc\n", rc));
#   endif /* VBOX_STRICT */
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */
            }
            STAM_PROFILE_STOP(&pVM->pgm.s.StatOutOfSync, c);
# endif /* PGM_OUT_OF_SYNC_IN_GC */
        }
        else
        {
            /*
             * Page not present in Guest OS or invalid page table address.
             * This is potential virtual page access handler food.
             *
             * For the present we'll say that our access handlers don't
             * work for this case - we've already discarded the page table
             * not present case which is identical to this.
             *
             * When we perchance find we need this, we will probably have AVL
             * trees (offset based) to operate on and we can measure their speed
             * agains mapping a page table and probably rearrange this handling
             * a bit. (Like, searching virtual ranges before checking the
             * physical address.)
             */
        }
    }


# if PGM_WITH_PAGING(PGM_GST_TYPE)
    /*
     * Conclusion, this is a guest trap.
     */
    LogFlow(("PGM: Unhandled #PF -> route trap to recompiler!\n"));
    STAM_COUNTER_INC(&pVM->pgm.s.StatGCTrap0eUnhandled);
    return VINF_EM_RAW_GUEST_TRAP;
# else
    /* present, but not a monitored page; perhaps the guest is probing physical memory */
    return VINF_EM_RAW_EMULATE_INSTR;
# endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */


#else /* PGM_GST_TYPE != PGM_TYPE_32BIT */

    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    return VERR_INTERNAL_ERROR;
#endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}


/**
 * Emulation of the invlpg instruction.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 *
 * @remark  ASSUMES that the guest is updating before invalidating. This order
 *          isn't required by the CPU, so this is speculative and could cause
 *          trouble.
 *
 * @todo    Flush page or page directory only if necessary!
 * @todo    Add a #define for simply invalidating the page.
 */
PGM_BTH_DECL(int, InvalidatePage)(PVM pVM, RTGCUINTPTR GCPtrPage)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT

    LogFlow(("InvalidatePage %x\n", GCPtrPage));
# if PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE
    /*
     * Get the shadow PD entry and skip out if this PD isn't present.
     * (Guessing that it is frequent for a shadow PDE to not be present, do this first.)
     */
    const unsigned  iPDDst = GCPtrPage >> SHW_PD_SHIFT;
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PDE     pPdeDst = &pVM->pgm.s.CTXMID(p,32BitPD)->a[iPDDst];
#  else
    PX86PDEPAE  pPdeDst = &pVM->pgm.s.CTXMID(ap,PaePDs[0])->a[iPDDst];
#  endif
    const SHWPDE PdeDst = *pPdeDst;
    if (!PdeDst.n.u1Present)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePageSkipped));
        return VINF_SUCCESS;
    }

    /*
     * Get the guest PD entry and calc big page.
     */
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    PX86PD          pPDSrc      = CTXSUFF(pVM->pgm.s.pGuestPD);
    const unsigned  iPDSrc      = GCPtrPage >> GST_PD_SHIFT;
    GSTPDE          PdeSrc      = pPDSrc->a[iPDSrc];
#  else /* PAE */
    unsigned        iPDSrc;
    PX86PD          pPDSrc = pgmGstGetPaePDPtr(&pVM->pgm.s, GCPtrPage, &iPDSrc);

    GSTPDE          PdeSrc;;
    PdeSrc.u      = pgmGstGetPaePDE(&pVM->pgm.s, GCPtrPage);
#  endif

    const uint32_t  cr4         = CPUMGetGuestCR4(pVM);
    const bool      fIsBigPage  = PdeSrc.b.u1Size && (cr4 & X86_CR4_PSE);

#  ifdef IN_RING3
    /*
     * If a CR3 Sync is pending we may ignore the invalidate page operation
     * depending on the kind of sync and if it's a global page or not.
     * This doesn't make sense in GC/R0 so we'll skip it entirely there.
     */
#   ifdef PGM_SKIP_GLOBAL_PAGEDIRS_ON_NONGLOBAL_FLUSH
    if (    VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3)
        || (   VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3_NON_GLOBAL)
            && fIsBigPage
            && PdeSrc.b.u1Global
            && (cr4 & X86_CR4_PGE)
           )
       )
#   else
    if (VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL) )
#   endif
    {
        STAM_COUNTER_INC(&pVM->pgm.s.StatHCInvalidatePageSkipped);
        return VINF_SUCCESS;
    }
#  endif /* IN_RING3 */


    /*
     * Deal with the Guest PDE.
     */
    int rc = VINF_SUCCESS;
    if (PdeSrc.n.u1Present)
    {
        if (PdeDst.u & PGM_PDFLAGS_MAPPING)
        {
            /*
             * Conflict - Let SyncPT deal with it to avoid duplicate code.
             */
            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
            rc = PGM_BTH_NAME(SyncPT)(pVM, iPDSrc, pPDSrc, GCPtrPage);
        }
        else if (   PdeSrc.n.u1User != PdeDst.n.u1User
                 || (!PdeSrc.n.u1Write && PdeDst.n.u1Write))
        {
            /*
             * Mark not present so we can resync the PDE when it's used.
             */
            LogFlow(("InvalidatePage: Out-of-sync at %VGp PdeSrc=%RX64 PdeDst=%RX64\n",
                     GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, SHW_POOL_ROOT_IDX, iPDDst);
            pPdeDst->u = 0;
            STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePagePDOutOfSync));
            PGM_INVL_GUEST_TLBS();
        }
#  ifdef PGM_SYNC_ACCESSED_BIT
        else if (!PdeSrc.n.u1Accessed)
        {
            /*
             * Mark not present so we can set the accessed bit.
             */
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, SHW_POOL_ROOT_IDX, iPDDst);
            pPdeDst->u = 0;
            STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePagePDNAs));
            PGM_INVL_GUEST_TLBS();
        }
#  endif
        else if (!fIsBigPage)
        {
            /*
             * 4KB - page.
             */
            PPGMPOOLPAGE    pShwPage = pgmPoolGetPageByHCPhys(pVM, PdeDst.u & SHW_PDE_PG_MASK);
            RTGCPHYS        GCPhys = PdeSrc.u & GST_PDE_PG_MASK;
#  if PGM_SHW_TYPE != PGM_TYPE_32BIT
            GCPhys |= (iPDDst & 1) * (PAGE_SIZE/2);
#  endif
            if (pShwPage->GCPhys == GCPhys)
            {
#if 0 /* likely cause of a major performance regression; must be SyncPageWorkerTrackDeref then */
                const unsigned iPTEDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
                PSHWPT pPT = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
                if (pPT->a[iPTEDst].n.u1Present)
                {
#  ifdef PGMPOOL_WITH_USER_TRACKING
                    /* This is very unlikely with caching/monitoring enabled. */
                    PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVM, pShwPage, pPT->a[iPTEDst].u & SHW_PTE_PG_MASK);
#  endif
                    pPT->a[iPTEDst].u = 0;
                }
#else /* Syncing it here isn't 100% safe and it's probably not worth spending time syncing it. */
                rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, GCPtrPage, 1, 0);
                if (VBOX_SUCCESS(rc))
                    rc = VINF_SUCCESS;
#endif
                STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePage4KBPages));
                PGM_INVL_PG(GCPtrPage);
            }
            else
            {
                /*
                 * The page table address changed.
                 */
                LogFlow(("InvalidatePage: Out-of-sync at %VGp PdeSrc=%RX64 PdeDst=%RX64 ShwGCPhys=%VGp iPDDst=%#x\n",
                         GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u, pShwPage->GCPhys, iPDDst));
                pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, SHW_POOL_ROOT_IDX, iPDDst);
                pPdeDst->u = 0;
                STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePagePDOutOfSync));
                PGM_INVL_GUEST_TLBS();
            }
        }
        else
        {
            /*
             * 4MB - page.
             */
            /* Before freeing the page, check if anything really changed. */
            PPGMPOOLPAGE    pShwPage = pgmPoolGetPageByHCPhys(pVM, PdeDst.u & SHW_PDE_PG_MASK);
            RTGCPHYS        GCPhys = PdeSrc.u & GST_PDE4M_PG_MASK;
#  if PGM_SHW_TYPE != PGM_TYPE_32BIT
            GCPhys |= GCPtrPage & (1 << X86_PD_PAE_SHIFT);
#  endif
            if (    pShwPage->GCPhys == GCPhys
                &&  pShwPage->enmKind == BTH_PGMPOOLKIND_PT_FOR_BIG)
            {
                /* ASSUMES a the given bits are identical for 4M and normal PDEs */
                /** @todo PAT */
#  ifdef PGM_SYNC_DIRTY_BIT
                if (        (PdeSrc.u & (X86_PDE_P | X86_PDE_RW | X86_PDE_US | X86_PDE_PWT | X86_PDE_PCD))
                        ==  (PdeDst.u & (X86_PDE_P | X86_PDE_RW | X86_PDE_US | X86_PDE_PWT | X86_PDE_PCD))
                    &&  (   PdeSrc.b.u1Dirty /** @todo rainy day: What about read-only 4M pages? not very common, but still... */
                         || (PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY)))
#  else
                if (    (PdeSrc.u & (X86_PDE_P | X86_PDE_RW | X86_PDE_US | X86_PDE_PWT | X86_PDE_PCD))
                    ==  (PdeDst.u & (X86_PDE_P | X86_PDE_RW | X86_PDE_US | X86_PDE_PWT | X86_PDE_PCD)))
#  endif
                {
                    LogFlow(("Skipping flush for big page containing %VGv (PD=%X)-> nothing has changed!\n", GCPtrPage, iPDSrc));
                    STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePage4MBPagesSkip));
                    return VINF_SUCCESS;
                }
            }

            /*
             * Ok, the page table is present and it's been changed in the guest.
             * If we're in host context, we'll just mark it as not present taking the lazy approach.
             * We could do this for some flushes in GC too, but we need an algorithm for
             * deciding which 4MB pages containing code likely to be executed very soon.
             */
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, SHW_POOL_ROOT_IDX, iPDDst);
            pPdeDst->u = 0;
            STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePage4MBPages));
            DUMP_PDE_BIG("PGMInvalidatePage", iPDSrc, PdeSrc);
            PGM_INVL_BIG_PG(GCPtrPage);
        }
    }
    else
    {
        /*
         * Page directory is not present, mark shadow PDE not present.
         */
        if (!(PdeDst.u & PGM_PDFLAGS_MAPPING))
        {
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, SHW_POOL_ROOT_IDX, iPDDst);
            pPdeDst->u = 0;
            STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePagePDNPs));
            PGM_INVL_PG(GCPtrPage);
        }
        else
        {
            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
            STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,InvalidatePagePDMappings));
        }
    }

    return rc;

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
#  error "Guest 32-bit mode and shadow AMD64 mode doesn't add up!"
# endif
    return VINF_SUCCESS;

#elif PGM_GST_TYPE == PGM_TYPE_PAE
# if PGM_SHW_TYPE == PGM_TYPE_PAE
//# error not implemented
    return VERR_INTERNAL_ERROR;

# else  /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */
#  error "Guest PAE mode, but not the shadow mode ; 32bit - maybe, but amd64 no."
# endif /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */

#elif PGM_GST_TYPE == PGM_TYPE_AMD64
# if PGM_SHW_TYPE == PGM_TYPE_AMD64
//# error not implemented
    return VERR_INTERNAL_ERROR;

# else  /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */
#  error "Guest AMD64 mode, but not the shadow mode - that can't be right!"
# endif /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */

#else /* guest real and protected mode */
    /* There's no such thing when paging is disabled. */
    return VINF_SUCCESS;
#endif
}


#ifdef PGMPOOL_WITH_USER_TRACKING
/**
 * Update the tracking of shadowed pages.
 *
 * @param   pVM         The VM handle.
 * @param   pShwPage    The shadow page.
 * @param   HCPhys      The physical page we is being dereferenced.
 */
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorkerTrackDeref)(PVM pVM, PPGMPOOLPAGE pShwPage, RTHCPHYS HCPhys)
{
# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    STAM_PROFILE_START(&pVM->pgm.s.StatTrackDeref, a);
    LogFlow(("SyncPageWorkerTrackDeref: Damn HCPhys=%VHp pShwPage->idx=%#x!!!\n", HCPhys, pShwPage->idx));

    /** @todo If this turns out to be a bottle neck (*very* likely) two things can be done:
     *      1. have a medium sized HCPhys -> GCPhys TLB (hash?)
     *      2. write protect all shadowed pages. I.e. implement caching.
     */
    /*
     * Find the guest address.
     */
    for (PPGMRAMRANGE pRam = CTXALLSUFF(pVM->pgm.s.pRamRanges);
         pRam;
         pRam = CTXALLSUFF(pRam->pNext))
    {
        unsigned iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
        {
            if (PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys)
            {
                PPGMPOOL pPool = pVM->pgm.s.CTXSUFF(pPool);
                pgmTrackDerefGCPhys(pPool, pShwPage, &pRam->aPages[iPage]);
                pShwPage->cPresent--;
                pPool->cPresent--;
                STAM_PROFILE_STOP(&pVM->pgm.s.StatTrackDeref, a);
                return;
            }
        }
    }

    for (;;)
        AssertReleaseMsgFailed(("HCPhys=%VHp wasn't found!\n", HCPhys));
# else  /* !PGMPOOL_WITH_GCPHYS_TRACKING */
    pShwPage->cPresent--;
    pVM->pgm.s.CTXSUFF(pPool)->cPresent--;
# endif /* !PGMPOOL_WITH_GCPHYS_TRACKING */
}


/**
 * Update the tracking of shadowed pages.
 *
 * @param   pVM         The VM handle.
 * @param   pShwPage    The shadow page.
 * @param   u16         The top 16-bit of the pPage->HCPhys.
 * @param   pPage       Pointer to the guest page. this will be modified.
 * @param   iPTDst      The index into the shadow table.
 */
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorkerTrackAddref)(PVM pVM, PPGMPOOLPAGE pShwPage, uint16_t u16, PPGMPAGE pPage, const unsigned iPTDst)
{
# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    /*
     * We're making certain assumptions about the placement of cRef and idx.
     */
    Assert(MM_RAM_FLAGS_IDX_SHIFT == 48);
    Assert(MM_RAM_FLAGS_CREFS_SHIFT > MM_RAM_FLAGS_IDX_SHIFT);

    /*
     * Just deal with the simple first time here.
     */
    if (!u16)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.StatTrackVirgin);
        u16 = (1 << (MM_RAM_FLAGS_CREFS_SHIFT - MM_RAM_FLAGS_IDX_SHIFT)) | pShwPage->idx;
    }
    else
        u16 = pgmPoolTrackPhysExtAddref(pVM, u16, pShwPage->idx);

    /* write back, trying to be clever... */
    Log2(("SyncPageWorkerTrackAddRef: u16=%#x pPage->HCPhys=%VHp->%VHp iPTDst=%#x\n",
          u16, pPage->HCPhys, (pPage->HCPhys & MM_RAM_FLAGS_NO_REFS_MASK) | ((uint64_t)u16 << MM_RAM_FLAGS_CREFS_SHIFT), iPTDst));
    *((uint16_t *)&pPage->HCPhys + 3) = u16; /** @todo PAGE FLAGS */
# endif /* PGMPOOL_WITH_GCPHYS_TRACKING */

    /* update statistics. */
    pVM->pgm.s.CTXSUFF(pPool)->cPresent++;
    pShwPage->cPresent++;
    if (pShwPage->iFirstPresent > iPTDst)
        pShwPage->iFirstPresent = iPTDst;
}
#endif /* PGMPOOL_WITH_USER_TRACKING */


/**
 * Creates a 4K shadow page for a guest page.
 *
 * For 4M pages the caller must convert the PDE4M to a PTE, this includes adjusting the
 * physical address. The PdeSrc argument only the flags are used. No page structured
 * will be mapped in this function.
 *
 * @param   pVM         VM handle.
 * @param   pPteDst     Destination page table entry.
 * @param   PdeSrc      Source page directory entry (i.e. Guest OS page directory entry).
 *                      Can safely assume that only the flags are being used.
 * @param   PteSrc      Source page table entry (i.e. Guest OS page table entry).
 * @param   pShwPage    Pointer to the shadow page.
 * @param   iPTDst      The index into the shadow table.
 *
 * @remark  Not used for 2/4MB pages!
 */
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorker)(PVM pVM, PSHWPTE pPteDst, GSTPDE PdeSrc, VBOXPTE PteSrc, PPGMPOOLPAGE pShwPage, unsigned iPTDst)
{
    if (PteSrc.n.u1Present)
    {
        /*
         * Find the ram range.
         */
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageEx(&pVM->pgm.s, PteSrc.u & GST_PTE_PG_MASK, &pPage);
        if (VBOX_SUCCESS(rc))
        {
            /** @todo investiage PWT, PCD and PAT. */
            /*
             * Make page table entry.
             */
            const RTHCPHYS HCPhys = pPage->HCPhys; /** @todo FLAGS */
            SHWPTE PteDst;
            if (PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPage))
            {
                /** @todo r=bird: Are we actually handling dirty and access bits for pages with access handlers correctly? No. */
                if (!PGM_PAGE_HAVE_ACTIVE_ALL_HANDLERS(pPage))
                    PteDst.u = (PteSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT | X86_PTE_RW))
                             | (HCPhys & X86_PTE_PAE_PG_MASK);
                else
                {
                    LogFlow(("SyncPageWorker: monitored page (%VGp) -> mark not present\n", HCPhys));
                    PteDst.u = 0;
                }
                /** @todo count these two kinds. */
            }
            else
            {
#ifdef PGM_SYNC_DIRTY_BIT
# ifdef PGM_SYNC_ACCESSED_BIT
                /*
                 * If the page or page directory entry is not marked accessed,
                 * we mark the page not present.
                 */
                if (!PteSrc.n.u1Accessed || !PdeSrc.n.u1Accessed)
                {
                    LogFlow(("SyncPageWorker: page and or page directory not accessed -> mark not present\n"));
                    STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,AccessedPage));
                    PteDst.u = 0;
                }
                else
# endif
                /*
                 * If the page is not flagged as dirty and is writable, then make it read-only, so we can set the dirty bit
                 * when the page is modified.
                 */
                if (!PteSrc.n.u1Dirty && (PdeSrc.n.u1Write & PteSrc.n.u1Write))
                {
                    STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPage));
                    PteDst.u = (PteSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT | X86_PTE_RW))
                             | (HCPhys & X86_PTE_PAE_PG_MASK)
                             | PGM_PTFLAGS_TRACK_DIRTY;
                }
                else
                {
                    STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPageSkipped));
                    PteDst.u = (PteSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT))
                             | (HCPhys & X86_PTE_PAE_PG_MASK);
                }
#endif
            }

#ifdef PGMPOOL_WITH_USER_TRACKING
            /*
             * Keep user track up to date.
             */
            if (PteDst.n.u1Present)
            {
                if (!pPteDst->n.u1Present)
                    PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVM, pShwPage, HCPhys >> MM_RAM_FLAGS_IDX_SHIFT, pPage, iPTDst);
                else if ((pPteDst->u & SHW_PTE_PG_MASK) != (PteDst.u & SHW_PTE_PG_MASK))
                {
                    Log2(("SyncPageWorker: deref! *pPteDst=%RX64 PteDst=%RX64\n", (uint64_t)pPteDst->u, (uint64_t)PteDst.u));
                    PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVM, pShwPage, pPteDst->u & SHW_PTE_PG_MASK);
                    PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVM, pShwPage, HCPhys >> MM_RAM_FLAGS_IDX_SHIFT, pPage, iPTDst);
                }
            }
            else if (pPteDst->n.u1Present)
            {
                Log2(("SyncPageWorker: deref! *pPteDst=%RX64\n", (uint64_t)pPteDst->u));
                PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVM, pShwPage, pPteDst->u & SHW_PTE_PG_MASK);
            }
#endif /* PGMPOOL_WITH_USER_TRACKING */

            /*
             * Update statistics and commit the entry.
             */
            if (!PteSrc.n.u1Global)
                pShwPage->fSeenNonGlobal = true;
            *pPteDst = PteDst;
        }
        /* else MMIO or invalid page, we must handle them manually in the #PF handler. */
        /** @todo count these. */
    }
    else
    {
        /*
         * Page not-present.
         */
        LogFlow(("SyncPageWorker: page not present in Pte\n"));
#ifdef PGMPOOL_WITH_USER_TRACKING
        /* Keep user track up to date. */
        if (pPteDst->n.u1Present)
        {
            Log2(("SyncPageWorker: deref! *pPteDst=%RX64\n", (uint64_t)pPteDst->u));
            PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVM, pShwPage, pPteDst->u & SHW_PTE_PG_MASK);
        }
#endif /* PGMPOOL_WITH_USER_TRACKING */
        pPteDst->u = 0;
        /** @todo count these. */
    }
}


/**
 * Syncs a guest OS page.
 *
 * There are no conflicts at this point, neither is there any need for
 * page table allocations.
 *
 * @returns VBox status code.
 * @returns VINF_PGM_SYNCPAGE_MODIFIED_PDE if it modifies the PDE in any way.
 * @param   pVM         VM handle.
 * @param   PdeSrc      Page directory entry of the guest.
 * @param   GCPtrPage   Guest context page address.
 * @param   cPages      Number of pages to sync (PGM_SYNC_N_PAGES) (default=1).
 * @param   uErr        Fault error (X86_TRAP_PF_*).
 */
PGM_BTH_DECL(int, SyncPage)(PVM pVM, GSTPDE PdeSrc, RTGCUINTPTR GCPtrPage, unsigned cPages, unsigned uErr)
{
    LogFlow(("SyncPage: GCPtrPage=%VGv cPages=%d uErr=%#x\n", GCPtrPage, cPages, uErr));

#if PGM_GST_TYPE == PGM_TYPE_32BIT

# if PGM_SHW_TYPE != PGM_TYPE_32BIT && PGM_SHW_TYPE != PGM_TYPE_PAE
#  error "Invalid shadow mode for 32-bit guest mode!"
# endif

    /*
     * Assert preconditions.
     */
# if GC_ARCH_BITS != 32
    Assert(GCPtrPage < _4G); //???
# endif
    STAM_COUNTER_INC(&pVM->pgm.s.StatGCSyncPagePD[(GCPtrPage >> X86_PD_SHIFT) & GST_PD_MASK]);
    Assert(PdeSrc.n.u1Present);
    Assert(cPages);

    /*
     * Get the shadow PDE, find the shadow page table in the pool.
     */
    const unsigned iPDDst = GCPtrPage >> SHW_PD_SHIFT;
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    X86PDE          PdeDst = pVM->pgm.s.CTXMID(p,32BitPD)->a[iPDDst];
# else /* PAE */
    X86PDEPAE       PdeDst = pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[iPDDst];
# endif
    Assert(PdeDst.n.u1Present);
    PPGMPOOLPAGE    pShwPage = pgmPoolGetPageByHCPhys(pVM, PdeDst.u & SHW_PDE_PG_MASK);

    /*
     * Check that the page is present and that the shadow PDE isn't out of sync.
     */
    const bool      fBigPage = PdeSrc.b.u1Size && (CPUMGetGuestCR4(pVM) & X86_CR4_PSE);
    RTGCPHYS        GCPhys;
    if (!fBigPage)
    {
        GCPhys = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE != PGM_TYPE_32BIT
        GCPhys |= (iPDDst & 1) * (PAGE_SIZE/2);
# endif
    }
    else
    {
        GCPhys = PdeSrc.u & GST_PDE4M_PG_MASK;
# if PGM_SHW_TYPE != PGM_TYPE_32BIT
        GCPhys |= GCPtrPage & X86_PAGE_2M_SIZE;
# endif
    }
    if (    pShwPage->GCPhys == GCPhys
        &&  PdeSrc.n.u1Present
        &&  (PdeSrc.n.u1User == PdeDst.n.u1User)
        &&  (PdeSrc.n.u1Write == PdeDst.n.u1Write || !PdeDst.n.u1Write)
       )
    {
# ifdef PGM_SYNC_ACCESSED_BIT
        /*
         * Check that the PDE is marked accessed already.
         * Since we set the accessed bit *before* getting here on a #PF, this
         * check is only meant for dealing with non-#PF'ing paths.
         */
        if (PdeSrc.n.u1Accessed)
# endif
        {
            PSHWPT pPTDst = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
            if (!fBigPage)
            {
                /*
                 * 4KB Page - Map the guest page table.
                 */
                PVBOXPT pPTSrc;
                int rc = PGM_GCPHYS_2_PTR(pVM, PdeSrc.u & GST_PDE_PG_MASK, &pPTSrc);
                if (VBOX_SUCCESS(rc))
                {
# ifdef PGM_SYNC_N_PAGES
                    Assert(cPages == 1 || !(uErr & X86_TRAP_PF_P));
                    if (cPages > 1 && !(uErr & X86_TRAP_PF_P))
                    {
                        /*
                         * This code path is currently only taken when the caller is PGMTrap0eHandler
                         * for non-present pages!
                         *
                         * We're setting PGM_SYNC_NR_PAGES pages around the faulting page to sync it and
                         * deal with locality.
                         */
                        unsigned        iPTDst    = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
                        const unsigned  offPTSrc  = 0;
#  else
                        const unsigned  offPTSrc  = ((GCPtrPage >> SHW_PD_SHIFT) & 1) * 512;
#  endif
                        const unsigned  iPTDstEnd = RT_MIN(iPTDst + PGM_SYNC_NR_PAGES / 2, ELEMENTS(pPTDst->a));
                        if (iPTDst < PGM_SYNC_NR_PAGES / 2)
                            iPTDst = 0;
                        else
                            iPTDst -= PGM_SYNC_NR_PAGES / 2;
                        for (; iPTDst < iPTDstEnd; iPTDst++)
                        {
                            if (!pPTDst->a[iPTDst].n.u1Present)
                            {
                                VBOXPTE PteSrc = pPTSrc->a[offPTSrc + iPTDst];
                                RTGCUINTPTR GCPtrCurPage = ((RTGCUINTPTR)GCPtrPage & ~(RTGCUINTPTR)(GST_PT_MASK << GST_PT_SHIFT)) | ((offPTSrc + iPTDst) << PAGE_SHIFT);
                                NOREF(GCPtrCurPage);
#ifndef IN_RING0
                                /*
                                 * Assuming kernel code will be marked as supervisor - and not as user level
                                 * and executed using a conforming code selector - And marked as readonly.
                                 * Also assume that if we're monitoring a page, it's of no interest to CSAM.
                                 */
                                PPGMPAGE pPage;
                                if (    ((PdeSrc.u & PteSrc.u) & (X86_PTE_RW | X86_PTE_US))
                                    ||  iPTDst == ((GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK)   /* always sync GCPtrPage */
                                    ||  !CSAMDoesPageNeedScanning(pVM, (RTGCPTR)GCPtrCurPage)
                                    ||  (   (pPage = pgmPhysGetPage(&pVM->pgm.s, PteSrc.u & GST_PTE_PG_MASK))
                                         && PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPage))
                                   )
#endif /* else: CSAM not active */
                                    PGM_BTH_NAME(SyncPageWorker)(pVM, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);
                                Log2(("SyncPage: 4K+ %VGv PteSrc:{P=%d RW=%d U=%d raw=%08llx} PteDst=%08llx%s\n",
                                      GCPtrCurPage, PteSrc.n.u1Present,
                                      PteSrc.n.u1Write & PdeSrc.n.u1Write,
                                      PteSrc.n.u1User & PdeSrc.n.u1User,
                                      (uint64_t)PteSrc.u,
                                      (uint64_t)pPTDst->a[iPTDst].u,
                                      pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
                            }
                        }
                    }
                    else
# endif /* PGM_SYNC_N_PAGES */
                    {
                        const unsigned iPTSrc = (GCPtrPage >> GST_PT_SHIFT) & GST_PT_MASK;
                        VBOXPTE PteSrc = pPTSrc->a[iPTSrc];
                        const unsigned iPTDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
                        PGM_BTH_NAME(SyncPageWorker)(pVM, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);
                        Log2(("SyncPage: 4K  %VGv PteSrc:{P=%d RW=%d U=%d raw=%08llx}%s\n",
                              GCPtrPage, PteSrc.n.u1Present,
                              PteSrc.n.u1Write & PdeSrc.n.u1Write,
                              PteSrc.n.u1User & PdeSrc.n.u1User,
                              (uint64_t)PteSrc.u,
                              pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
                    }
                }
                else /* MMIO or invalid page: emulated in #PF handler. */
                {
                    LogFlow(("PGM_GCPHYS_2_PTR %VGp failed with %Vrc\n", GCPhys, rc));
                    Assert(!pPTDst->a[(GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK].n.u1Present);
                }
            }
            else
            {
                /*
                 * 4/2MB page - lazy syncing shadow 4K pages.
                 * (There are many causes of getting here, it's no longer only CSAM.)
                 */
                /* Calculate the GC physical address of this 4KB shadow page. */
                RTGCPHYS GCPhys = (PdeSrc.u & X86_PDE4M_PAE_PG_MASK) | ((RTGCUINTPTR)GCPtrPage & GST_BIG_PAGE_OFFSET_MASK);
                /* Find ram range. */
                PPGMPAGE pPage;
                int rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
                if (VBOX_SUCCESS(rc))
                {
                    /*
                     * Make shadow PTE entry.
                     */
                    const RTHCPHYS HCPhys = pPage->HCPhys; /** @todo PAGE FLAGS */
                    SHWPTE PteDst;
                    PteDst.u = (PdeSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT))
                             | (HCPhys & X86_PTE_PAE_PG_MASK);
                    if (PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPage))
                    {
                        if (!PGM_PAGE_HAVE_ACTIVE_ALL_HANDLERS(pPage))
                            PteDst.n.u1Write = 0;
                        else
                            PteDst.u = 0;
                    }
                    const unsigned iPTDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
# ifdef PGMPOOL_WITH_USER_TRACKING
                    if (PteDst.n.u1Present && !pPTDst->a[iPTDst].n.u1Present)
                        PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVM, pShwPage, HCPhys >> MM_RAM_FLAGS_IDX_SHIFT, pPage, iPTDst);
# endif
                    pPTDst->a[iPTDst] = PteDst;


# ifdef PGM_SYNC_DIRTY_BIT
                    /*
                     * If the page is not flagged as dirty and is writable, then make it read-only
                     * at PD level, so we can set the dirty bit when the page is modified.
                     *
                     * ASSUMES that page access handlers are implemented on page table entry level.
                     *      Thus we will first catch the dirty access and set PDE.D and restart. If
                     *      there is an access handler, we'll trap again and let it work on the problem.
                     */
                    /** @todo r=bird: figure out why we need this here, SyncPT should've taken care of this already.
                     * As for invlpg, it simply frees the whole shadow PT.
                     * ...It's possibly because the guest clears it and the guest doesn't really tell us... */
                    if (!PdeSrc.b.u1Dirty && PdeSrc.b.u1Write)
                    {
                        STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPageBig));
                        PdeDst.u |= PGM_PDFLAGS_TRACK_DIRTY;
                        PdeDst.n.u1Write = 0;
                    }
                    else
                    {
                        PdeDst.au32[0] &= ~PGM_PDFLAGS_TRACK_DIRTY;
                        PdeDst.n.u1Write = PdeSrc.n.u1Write;
                    }
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
                    pVM->pgm.s.CTXMID(p,32BitPD)->a[iPDDst]        = PdeDst;
#  else /* PAE */
                    pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[iPDDst] = PdeDst;
#  endif
# endif /* PGM_SYNC_DIRTY_BIT */
                    Log2(("SyncPage: BIG %VGv PdeSrc:{P=%d RW=%d U=%d raw=%08llx} GCPhys=%VGp%s\n",
                          GCPtrPage, PdeSrc.n.u1Present, PdeSrc.n.u1Write, PdeSrc.n.u1User, (uint64_t)PdeSrc.u, GCPhys,
                          PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
                }
                else
                    LogFlow(("PGM_GCPHYS_2_PTR %VGp (big) failed with %Vrc\n", GCPhys, rc));
            }
            return VINF_SUCCESS;
        }
# ifdef PGM_SYNC_ACCESSED_BIT
        STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncPagePDNAs));
#endif
    }
    else
    {
        STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncPagePDOutOfSync));
        Log2(("SyncPage: Out-Of-Sync PDE at %VGp PdeSrc=%RX64 PdeDst=%RX64\n",
              GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
    }

    /*
     * Mark the PDE not present. Restart the instruction and let #PF call SyncPT.
     * Yea, I'm lazy.
     */
    pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, SHW_POOL_ROOT_IDX, iPDDst);
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
    pVM->pgm.s.CTXMID(p,32BitPD)->a[iPDDst].u    = 0;
#  else /* PAE */
    pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[iPDDst].u = 0;
#  endif
    PGM_INVL_GUEST_TLBS();
    return VINF_PGM_SYNCPAGE_MODIFIED_PDE;

#elif PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT

# ifdef PGM_SYNC_N_PAGES
    /*
     * Get the shadow PDE, find the shadow page table in the pool.
     */
    const unsigned iPDDst = GCPtrPage >> SHW_PD_SHIFT;
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    X86PDE          PdeDst = pVM->pgm.s.CTXMID(p,32BitPD)->a[iPDDst];
# else /* PAE */
    X86PDEPAE       PdeDst = pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[iPDDst];
# endif
    Assert(PdeDst.n.u1Present);
    PPGMPOOLPAGE    pShwPage = pgmPoolGetPageByHCPhys(pVM, PdeDst.u & SHW_PDE_PG_MASK);
    PSHWPT pPTDst = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);

#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
    const unsigned  offPTSrc  = 0;
#  else
    const unsigned  offPTSrc  = ((GCPtrPage >> SHW_PD_SHIFT) & 1) * 512;
#  endif

    Assert(cPages == 1 || !(uErr & X86_TRAP_PF_P));
    if (cPages > 1 && !(uErr & X86_TRAP_PF_P))
    {
        /*
         * This code path is currently only taken when the caller is PGMTrap0eHandler
         * for non-present pages!
         *
         * We're setting PGM_SYNC_NR_PAGES pages around the faulting page to sync it and
         * deal with locality.
         */
        unsigned        iPTDst    = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
        const unsigned  iPTDstEnd = RT_MIN(iPTDst + PGM_SYNC_NR_PAGES / 2, ELEMENTS(pPTDst->a));
        if (iPTDst < PGM_SYNC_NR_PAGES / 2)
            iPTDst = 0;
        else
            iPTDst -= PGM_SYNC_NR_PAGES / 2;
        for (; iPTDst < iPTDstEnd; iPTDst++)
        {
            if (!pPTDst->a[iPTDst].n.u1Present)
            {
                VBOXPTE PteSrc;

                RTGCUINTPTR GCPtrCurPage = ((RTGCUINTPTR)GCPtrPage & ~(RTGCUINTPTR)(GST_PT_MASK << GST_PT_SHIFT)) | ((offPTSrc + iPTDst) << PAGE_SHIFT);

                /* Fake the page table entry */
                PteSrc.u = GCPtrCurPage;
                PteSrc.n.u1Present  = 1;
                PteSrc.n.u1Dirty    = 1;
                PteSrc.n.u1Accessed = 1;
                PteSrc.n.u1Write    = 1;
                PteSrc.n.u1User     = 1;

                PGM_BTH_NAME(SyncPageWorker)(pVM, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);

                Log2(("SyncPage: 4K+ %VGv PteSrc:{P=%d RW=%d U=%d raw=%08llx} PteDst=%08llx%s\n",
                      GCPtrCurPage, PteSrc.n.u1Present,
                      PteSrc.n.u1Write & PdeSrc.n.u1Write,
                      PteSrc.n.u1User & PdeSrc.n.u1User,
                      (uint64_t)PteSrc.u,
                      (uint64_t)pPTDst->a[iPTDst].u,
                      pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
            }
        }
    }
    else
# endif /* PGM_SYNC_N_PAGES */
    {
        VBOXPTE PteSrc;
        const unsigned iPTDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
        RTGCUINTPTR GCPtrCurPage = ((RTGCUINTPTR)GCPtrPage & ~(RTGCUINTPTR)(GST_PT_MASK << GST_PT_SHIFT)) | ((offPTSrc + iPTDst) << PAGE_SHIFT);

        /* Fake the page table entry */
        PteSrc.u = GCPtrCurPage;
        PteSrc.n.u1Present  = 1;
        PteSrc.n.u1Dirty    = 1;
        PteSrc.n.u1Accessed = 1;
        PteSrc.n.u1Write    = 1;
        PteSrc.n.u1User     = 1;
        PGM_BTH_NAME(SyncPageWorker)(pVM, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);

        Log2(("SyncPage: 4K  %VGv PteSrc:{P=%d RW=%d U=%d raw=%08llx}%s\n",
              GCPtrPage, PteSrc.n.u1Present,
              PteSrc.n.u1Write & PdeSrc.n.u1Write,
              PteSrc.n.u1User & PdeSrc.n.u1User,
              (uint64_t)PteSrc.u,
              pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
    }
    return VINF_SUCCESS;

#else /* PGM_GST_TYPE != PGM_TYPE_32BIT */
    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    return VERR_INTERNAL_ERROR;
#endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}



#if PGM_WITH_PAGING(PGM_GST_TYPE)

# ifdef PGM_SYNC_DIRTY_BIT

/**
 * Investigate page fault and handle write protection page faults caused by
 * dirty bit tracking.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   uErr        Page fault error code.
 * @param   pPdeDst     Shadow page directory entry.
 * @param   pPdeSrc     Guest page directory entry.
 * @param   GCPtrPage   Guest context page address.
 */
PGM_BTH_DECL(int, CheckPageFault)(PVM pVM, uint32_t uErr, PSHWPDE pPdeDst, PGSTPDE pPdeSrc, RTGCUINTPTR GCPtrPage)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTXMID(Stat, DirtyBitTracking), a);
    LogFlow(("CheckPageFault: GCPtrPage=%VGv uErr=%#x PdeSrc=%08x\n", GCPtrPage, uErr, pPdeSrc->u));

    /*
     * Real page fault?
     */
    if (    (uErr & X86_TRAP_PF_RSVD)
        ||  !pPdeSrc->n.u1Present
#if PGM_GST_TYPE == PGM_TYPE_PAE
        ||  ((uErr & X86_TRAP_PF_ID) &&  pPdeSrc->n.u1NoExecute)
#endif
        ||  ((uErr & X86_TRAP_PF_RW) && !pPdeSrc->n.u1Write)
        ||  ((uErr & X86_TRAP_PF_US) && !pPdeSrc->n.u1User) )
    {
#  ifdef IN_GC
        STAM_COUNTER_INC(&pVM->pgm.s.StatGCDirtyTrackRealPF);
#  endif
        STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat, DirtyBitTracking), a);
        LogFlow(("CheckPageFault: real page fault at %VGv (1)\n", GCPtrPage));

        if (pPdeSrc->n.u1Present)
        {
            /* Check the present bit as the shadow tables can cause different error codes by being out of sync.
             * See the 2nd case below as well.
             */
            if (pPdeSrc->b.u1Size && (CPUMGetGuestCR4(pVM) & X86_CR4_PSE))
            {
                TRPMSetErrorCode(pVM, uErr | X86_TRAP_PF_P); /* page-level protection violation */
            }
            else
            {
                /*
                * Map the guest page table.
                */
                PGSTPT pPTSrc;
                int rc = PGM_GCPHYS_2_PTR(pVM, pPdeSrc->u & GST_PDE_PG_MASK, &pPTSrc);
                if (VBOX_SUCCESS(rc))
                {
                    PGSTPTE         pPteSrc = &pPTSrc->a[(GCPtrPage >> PAGE_SHIFT) & GST_PT_MASK];
                    const GSTPTE    PteSrc = *pPteSrc;
                    if (pPteSrc->n.u1Present)
                        TRPMSetErrorCode(pVM, uErr | X86_TRAP_PF_P); /* page-level protection violation */
                }
                AssertRC(rc);
            }
        }
        return VINF_EM_RAW_GUEST_TRAP;
    }

    /*
     * First check the easy case where the page directory has been marked read-only to track
     * the dirty bit of an emulated BIG page
     */
    if (pPdeSrc->b.u1Size && (CPUMGetGuestCR4(pVM) & X86_CR4_PSE))
    {
        /* Mark guest page directory as accessed */
        pPdeSrc->b.u1Accessed = 1;

        /*
         * Only write protection page faults are relevant here.
         */
        if (uErr & X86_TRAP_PF_RW)
        {
            /* Mark guest page directory as dirty (BIG page only). */
            pPdeSrc->b.u1Dirty = 1;

            if (pPdeDst->n.u1Present && (pPdeDst->u & PGM_PDFLAGS_TRACK_DIRTY))
            {
                STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPageTrap));

                Assert(pPdeSrc->b.u1Write);

                pPdeDst->n.u1Write      = 1;
                pPdeDst->n.u1Accessed   = 1;
                pPdeDst->au32[0]       &= ~PGM_PDFLAGS_TRACK_DIRTY;
                PGM_INVL_BIG_PG(GCPtrPage);
                STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
                return VINF_PGM_HANDLED_DIRTY_BIT_FAULT;
            }
        }
        STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
        return VINF_PGM_NO_DIRTY_BIT_TRACKING;
    }
    /* else: 4KB page table */

    /*
     * Map the guest page table.
     */
    PGSTPT pPTSrc;
    int rc = PGM_GCPHYS_2_PTR(pVM, pPdeSrc->u & GST_PDE_PG_MASK, &pPTSrc);
    if (VBOX_SUCCESS(rc))
    {
        /*
         * Real page fault?
         */
        PGSTPTE        pPteSrc = &pPTSrc->a[(GCPtrPage >> PAGE_SHIFT) & GST_PT_MASK];
        const GSTPTE   PteSrc = *pPteSrc;
        if (    !PteSrc.n.u1Present
#if PGM_GST_TYPE == PGM_TYPE_PAE
            ||  ((uErr & X86_TRAP_PF_ID) && !PteSrc.n.u1NoExecute)
#endif
            ||  ((uErr & X86_TRAP_PF_RW) && !PteSrc.n.u1Write)
            ||  ((uErr & X86_TRAP_PF_US) && !PteSrc.n.u1User)
           )
        {
#  ifdef IN_GC
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCDirtyTrackRealPF);
#  endif
            STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
            LogFlow(("CheckPageFault: real page fault at %VGv PteSrc.u=%08x (2)\n", GCPtrPage, PteSrc.u));

            /* Check the present bit as the shadow tables can cause different error codes by being out of sync.
            * See the 2nd case above as well.
            */
            if (pPdeSrc->n.u1Present && pPteSrc->n.u1Present)
                TRPMSetErrorCode(pVM, uErr | X86_TRAP_PF_P); /* page-level protection violation */

            STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
            return VINF_EM_RAW_GUEST_TRAP;
        }
        LogFlow(("CheckPageFault: page fault at %VGv PteSrc.u=%08x\n", GCPtrPage, PteSrc.u));

        /*
         * Set the accessed bits in the page directory and the page table.
         */
        pPdeSrc->n.u1Accessed = 1;
        pPteSrc->n.u1Accessed = 1;

        /*
         * Only write protection page faults are relevant here.
         */
        if (uErr & X86_TRAP_PF_RW)
        {
            /* Write access, so mark guest entry as dirty. */
#  if defined(IN_GC) && defined(VBOX_WITH_STATISTICS)
            if (!pPteSrc->n.u1Dirty)
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCDirtiedPage);
            else
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageAlreadyDirty);
#  endif
            pPteSrc->n.u1Dirty = 1;

            if (pPdeDst->n.u1Present)
            {
                /* Bail out here as pgmPoolGetPageByHCPhys will return NULL and we'll crash below.
                 * Our individual shadow handlers will provide more information and force a fatal exit.
                 */
                if (MMHyperIsInsideArea(pVM, (RTGCPTR)GCPtrPage))
                {
                    LogRel(("CheckPageFault: write to hypervisor region %VGv\n", GCPtrPage));
                    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
                    return VINF_SUCCESS;
                }

                /*
                 * Map shadow page table.
                 */
                PPGMPOOLPAGE    pShwPage = pgmPoolGetPageByHCPhys(pVM, pPdeDst->u & SHW_PDE_PG_MASK);
                if (pShwPage)
                {
                    PSHWPT      pPTDst   = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
                    PSHWPTE     pPteDst  = &pPTDst->a[(GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK];
                    if (    pPteDst->n.u1Present    /** @todo Optimize accessed bit emulation? */
                        &&  (pPteDst->u & PGM_PTFLAGS_TRACK_DIRTY))
                    {
                        LogFlow(("DIRTY page trap addr=%VGv\n", GCPtrPage));
#  ifdef VBOX_STRICT
                        PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, pPteSrc->u & GST_PTE_PG_MASK);
                        if (pPage)
                            AssertMsg(!PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPage),
                                      ("Unexpected dirty bit tracking on monitored page %VGv (phys %VGp)!!!!!!\n", GCPtrPage, pPteSrc->u & X86_PTE_PAE_PG_MASK));
#  endif
                        STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPageTrap));

                        Assert(pPteSrc->n.u1Write);

                        pPteDst->n.u1Write    = 1;
                        pPteDst->n.u1Dirty    = 1;
                        pPteDst->n.u1Accessed = 1;
                        pPteDst->au32[0]     &= ~PGM_PTFLAGS_TRACK_DIRTY;
                        PGM_INVL_PG(GCPtrPage);

                        STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
                        return VINF_PGM_HANDLED_DIRTY_BIT_FAULT;
                    }
                }
                else
                    AssertMsgFailed(("pgmPoolGetPageByHCPhys %VGp failed!\n", pPdeDst->u & SHW_PDE_PG_MASK));
            }
        }
/** @todo Optimize accessed bit emulation? */
#  ifdef VBOX_STRICT
        /*
         * Sanity check.
         */
        else if (   !pPteSrc->n.u1Dirty
                 && (pPdeSrc->n.u1Write & pPteSrc->n.u1Write)
                 && pPdeDst->n.u1Present)
        {
            PPGMPOOLPAGE    pShwPage = pgmPoolGetPageByHCPhys(pVM, pPdeDst->u & SHW_PDE_PG_MASK);
            PSHWPT          pPTDst   = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
            PSHWPTE         pPteDst  = &pPTDst->a[(GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK];
            if (    pPteDst->n.u1Present
                &&  pPteDst->n.u1Write)
                LogFlow(("Writable present page %VGv not marked for dirty bit tracking!!!\n", GCPtrPage));
        }
#  endif /* VBOX_STRICT */
        STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
        return VINF_PGM_NO_DIRTY_BIT_TRACKING;
    }
    AssertRC(rc);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,DirtyBitTracking), a);
    return rc;
}

# endif

#endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */


/**
 * Sync a shadow page table.
 *
 * The shadow page table is not present. This includes the case where
 * there is a conflict with a mapping.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   iPD         Page directory index.
 * @param   pPDSrc      Source page directory (i.e. Guest OS page directory).
 *                      Assume this is a temporary mapping.
 * @param   GCPtrPage   GC Pointer of the page that caused the fault
 */
PGM_BTH_DECL(int, SyncPT)(PVM pVM, unsigned iPDSrc, PGSTPD pPDSrc, RTGCUINTPTR GCPtrPage)
{
    STAM_PROFILE_START(&pVM->pgm.s.CTXMID(Stat,SyncPT), a);
    STAM_COUNTER_INC(&pVM->pgm.s.StatGCSyncPtPD[iPDSrc]);
    LogFlow(("SyncPT: GCPtrPage=%VGv\n", GCPtrPage));

#if PGM_GST_TYPE == PGM_TYPE_32BIT

# if PGM_SHW_TYPE != PGM_TYPE_32BIT && PGM_SHW_TYPE != PGM_TYPE_PAE
#  error "Invalid shadow mode for 32-bit guest mode!"
# endif

    /*
     * Validate input a little bit.
     */
    Assert(iPDSrc == (GCPtrPage >> GST_PD_SHIFT));
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PD          pPDDst = pVM->pgm.s.CTXMID(p,32BitPD);
# else
    PX86PDPAE       pPDDst = pVM->pgm.s.CTXMID(ap,PaePDs)[0];
# endif
    const unsigned  iPDDst = GCPtrPage >> SHW_PD_SHIFT;
    PSHWPDE         pPdeDst = &pPDDst->a[iPDDst];
    SHWPDE          PdeDst = *pPdeDst;

    /*
     * Check for conflicts.
     * GC: In case of a conflict we'll go to Ring-3 and do a full SyncCR3.
     * HC: Simply resolve the conflict.
     */
    if (PdeDst.u & PGM_PDFLAGS_MAPPING)
    {
        Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
# ifndef IN_RING3
        Log(("SyncPT: Conflict at %VGv\n", GCPtrPage));
        STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncPT), a);
        return VERR_ADDRESS_CONFLICT;
# else
        PPGMMAPPING pMapping = pgmGetMapping(pVM, (RTGCPTR)GCPtrPage);
        Assert(pMapping);
        int rc = pgmR3SyncPTResolveConflict(pVM, pMapping, pPDSrc, iPDSrc);
        if (VBOX_FAILURE(rc))
        {
            STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncPT), a);
            return rc;
        }
        PdeDst = *pPdeDst;
# endif
    }
    Assert(!PdeDst.n.u1Present); /* We're only supposed to call SyncPT on PDE!P and conflicts.*/

    /*
     * Sync page directory entry.
     */
    int     rc     = VINF_SUCCESS;
    GSTPDE  PdeSrc = pPDSrc->a[iPDSrc];
    if (PdeSrc.n.u1Present)
    {
        /*
         * Allocate & map the page table.
         */
        PSHWPT          pPTDst;
        const bool      fPageTable = !PdeSrc.b.u1Size || !(CPUMGetGuestCR4(pVM) & X86_CR4_PSE);
        PPGMPOOLPAGE    pShwPage;
        RTGCPHYS        GCPhys;
        if (fPageTable)
        {
            GCPhys = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
            GCPhys |= (iPDDst & 1) * (PAGE_SIZE / 2);
# endif
            rc = pgmPoolAlloc(pVM, GCPhys, BTH_PGMPOOLKIND_PT_FOR_PT, SHW_POOL_ROOT_IDX, iPDDst, &pShwPage);
        }
        else
        {
            GCPhys = PdeSrc.u & GST_PDE4M_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
            GCPhys |= GCPtrPage & RT_BIT(X86_PAGE_2M_SHIFT);
# endif
            rc = pgmPoolAlloc(pVM, GCPhys, BTH_PGMPOOLKIND_PT_FOR_BIG, SHW_POOL_ROOT_IDX, iPDDst, &pShwPage);
        }
        if (rc == VINF_SUCCESS)
            pPTDst = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
        else if (rc == VINF_PGM_CACHED_PAGE)
        {
            /*
             * The PT was cached, just hook it up.
             */
            if (fPageTable)
                PdeDst.u = pShwPage->Core.Key
                         | (PdeSrc.u & ~(X86_PDE_PAE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));
            else
            {
                PdeDst.u = pShwPage->Core.Key
                         | (PdeSrc.u & ~(X86_PDE_PAE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));
#  ifdef PGM_SYNC_DIRTY_BIT /* (see explanation and assumtions further down.) */
                if (!PdeSrc.b.u1Dirty && PdeSrc.b.u1Write)
                {
                    STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPageBig));
                    PdeDst.u |= PGM_PDFLAGS_TRACK_DIRTY;
                    PdeDst.b.u1Write = 0;
                }
#  endif
            }
            *pPdeDst = PdeDst;
            return VINF_SUCCESS;
        }
        else if (rc == VERR_PGM_POOL_FLUSHED)
            return VINF_PGM_SYNC_CR3;
        else
            AssertMsgFailedReturn(("rc=%Vrc\n", rc), VERR_INTERNAL_ERROR);
        PdeDst.u &= X86_PDE_AVL_MASK;
        PdeDst.u |= pShwPage->Core.Key;

# ifdef PGM_SYNC_DIRTY_BIT
        /*
         * Page directory has been accessed (this is a fault situation, remember).
         */
        pPDSrc->a[iPDSrc].n.u1Accessed = 1;
# endif
        if (fPageTable)
        {
            /*
             * Page table - 4KB.
             *
             * Sync all or just a few entries depending on PGM_SYNC_N_PAGES.
             */
            Log2(("SyncPT:   4K  %VGv PdeSrc:{P=%d RW=%d U=%d raw=%08llx}\n",
                  GCPtrPage, PdeSrc.b.u1Present, PdeSrc.b.u1Write, PdeSrc.b.u1User, (uint64_t)PdeSrc.u));
            PGSTPT pPTSrc;
            rc = PGM_GCPHYS_2_PTR(pVM, PdeSrc.u & GST_PDE_PG_MASK, &pPTSrc);
            if (VBOX_SUCCESS(rc))
            {
                /*
                 * Start by syncing the page directory entry so CSAM's TLB trick works.
                 */
                PdeDst.u = (PdeDst.u & (X86_PDE_PAE_PG_MASK | X86_PDE_AVL_MASK))
                         | (PdeSrc.u & ~(X86_PDE_PAE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));
                *pPdeDst = PdeDst;

                /*
                 * Directory/page user or supervisor privilege: (same goes for read/write)
                 *
                 * Directory    Page    Combined
                 * U/S          U/S     U/S
                 *  0            0       0
                 *  0            1       0
                 *  1            0       0
                 *  1            1       1
                 *
                 * Simple AND operation. Table listed for completeness.
                 *
                 */
                STAM_COUNTER_INC(CTXSUFF(&pVM->pgm.s.StatSynPT4k));
# ifdef PGM_SYNC_N_PAGES
                unsigned        iPTBase   = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
                unsigned        iPTDst    = iPTBase;
                const unsigned  iPTDstEnd = RT_MIN(iPTDst + PGM_SYNC_NR_PAGES / 2, ELEMENTS(pPTDst->a));
                if (iPTDst <= PGM_SYNC_NR_PAGES / 2)
                    iPTDst = 0;
                else
                    iPTDst -= PGM_SYNC_NR_PAGES / 2;
# else /* !PGM_SYNC_N_PAGES */
                unsigned        iPTDst    = 0;
                const unsigned  iPTDstEnd = ELEMENTS(pPTDst->a);
# endif /* !PGM_SYNC_N_PAGES */
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
                const unsigned  offPTSrc  = 0;
#  else
                const unsigned  offPTSrc  = ((GCPtrPage >> SHW_PD_SHIFT) & 1) * 512;
#  endif
                for (; iPTDst < iPTDstEnd; iPTDst++)
                {
                    const unsigned iPTSrc = iPTDst + offPTSrc;
                    const GSTPTE   PteSrc = pPTSrc->a[iPTSrc];

                    if (PteSrc.n.u1Present) /* we've already cleared it above */
                    {
#ifndef IN_RING0
                        /*
                         * Assuming kernel code will be marked as supervisor - and not as user level
                         * and executed using a conforming code selector - And marked as readonly.
                         * Also assume that if we're monitoring a page, it's of no interest to CSAM.
                         */
                        PPGMPAGE pPage;
                        if (    ((PdeSrc.u & pPTSrc->a[iPTSrc].u) & (X86_PTE_RW | X86_PTE_US))
                            ||  !CSAMDoesPageNeedScanning(pVM, (RTGCPTR)((iPDSrc << GST_PD_SHIFT) | (iPTSrc << PAGE_SHIFT)))
                            ||  (   (pPage = pgmPhysGetPage(&pVM->pgm.s, PteSrc.u & GST_PTE_PG_MASK))
                                 &&  PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPage))
                           )
#endif
                            PGM_BTH_NAME(SyncPageWorker)(pVM, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);
                        Log2(("SyncPT:   4K+ %VGv PteSrc:{P=%d RW=%d U=%d raw=%08llx}%s dst.raw=%08llx iPTSrc=%x PdeSrc.u=%x physpte=%VGp\n",
                              (RTGCPTR)((iPDSrc << GST_PD_SHIFT) | (iPTSrc << PAGE_SHIFT)),
                              PteSrc.n.u1Present,
                              PteSrc.n.u1Write & PdeSrc.n.u1Write,
                              PteSrc.n.u1User & PdeSrc.n.u1User,
                              (uint64_t)PteSrc.u,
                              pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : "", pPTDst->a[iPTDst].u, iPTSrc, PdeSrc.au32[0],
                              (PdeSrc.u & GST_PDE_PG_MASK) + iPTSrc*sizeof(PteSrc)));
                    }
                } /* for PTEs */
            }
        }
        else
        {
            /*
             * Big page - 2/4MB.
             *
             * We'll walk the ram range list in parallel and optimize lookups.
             * We will only sync on shadow page table at a time.
             */
            STAM_COUNTER_INC(CTXSUFF(&pVM->pgm.s.StatSynPT4M));

            /**
             * @todo It might be more efficient to sync only a part of the 4MB page (similar to what we do for 4kb PDs).
             */

            /*
             * Start by syncing the page directory entry.
             */
            PdeDst.u = (PdeDst.u & (X86_PDE_PAE_PG_MASK | (X86_PDE_AVL_MASK & ~PGM_PDFLAGS_TRACK_DIRTY)))
                     | (PdeSrc.u & ~(X86_PDE_PAE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));

# ifdef PGM_SYNC_DIRTY_BIT
            /*
             * If the page is not flagged as dirty and is writable, then make it read-only
             * at PD level, so we can set the dirty bit when the page is modified.
             *
             * ASSUMES that page access handlers are implemented on page table entry level.
             *      Thus we will first catch the dirty access and set PDE.D and restart. If
             *      there is an access handler, we'll trap again and let it work on the problem.
             */
            /** @todo move the above stuff to a section in the PGM documentation. */
            Assert(!(PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY));
            if (!PdeSrc.b.u1Dirty && PdeSrc.b.u1Write)
            {
                STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,DirtyPageBig));
                PdeDst.u |= PGM_PDFLAGS_TRACK_DIRTY;
                PdeDst.b.u1Write = 0;
            }
# endif /* PGM_SYNC_DIRTY_BIT */
            *pPdeDst = PdeDst;

            /*
             * Fill the shadow page table.
             */
            /* Get address and flags from the source PDE. */
            SHWPTE      PteDstBase;
            PteDstBase.u = PdeSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT);

            /* Loop thru the entries in the shadow PT. */
            const RTGCUINTPTR GCPtr  = (GCPtrPage >> SHW_PD_SHIFT) << SHW_PD_SHIFT; NOREF(GCPtr);
            Log2(("SyncPT:   BIG %VGv PdeSrc:{P=%d RW=%d U=%d raw=%08llx} Shw=%VGv GCPhys=%VGp %s\n",
                  GCPtrPage, PdeSrc.b.u1Present, PdeSrc.b.u1Write, PdeSrc.b.u1User, (uint64_t)PdeSrc.u, GCPtr,
                  GCPhys, PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
            PPGMRAMRANGE        pRam   = CTXALLSUFF(pVM->pgm.s.pRamRanges);
            unsigned            iPTDst = 0;
            while (iPTDst < ELEMENTS(pPTDst->a))
            {
                /* Advance ram range list. */
                while (pRam && GCPhys > pRam->GCPhysLast)
                    pRam = CTXALLSUFF(pRam->pNext);
                if (pRam && GCPhys >= pRam->GCPhys)
                {
                    unsigned iHCPage = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
                    do
                    {
                        /* Make shadow PTE. */
                        PPGMPAGE    pPage = &pRam->aPages[iHCPage];
                        SHWPTE      PteDst;

                        /* Make sure the RAM has already been allocated. */
                        if (pRam->fFlags & MM_RAM_FLAGS_DYNAMIC_ALLOC)  /** @todo PAGE FLAGS */
                        {
                            if (RT_UNLIKELY(!PGM_PAGE_GET_HCPHYS(pPage)))
                            {
# ifdef IN_RING3
                                int rc = pgmr3PhysGrowRange(pVM, GCPhys);
# else
                                int rc = CTXALLMID(VMM, CallHost)(pVM, VMMCALLHOST_PGM_RAM_GROW_RANGE, GCPhys);
# endif
                                if (rc != VINF_SUCCESS)
                                    return rc;
                            }
                        }

                        if (PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPage))
                        {
                            if (!PGM_PAGE_HAVE_ACTIVE_ALL_HANDLERS(pPage))
                            {
                                PteDst.u = PGM_PAGE_GET_HCPHYS(pPage) | PteDstBase.u;
                                PteDst.n.u1Write = 0;
                            }
                            else
                                PteDst.u = 0;
                        }
# ifndef IN_RING0
                        /*
                         * Assuming kernel code will be marked as supervisor and not as user level and executed
                         * using a conforming code selector. Don't check for readonly, as that implies the whole
                         * 4MB can be code or readonly data. Linux enables write access for its large pages.
                         */
                        else if (    !PdeSrc.n.u1User
                                 &&  CSAMDoesPageNeedScanning(pVM, (RTGCPTR)(GCPtr | (iPTDst << SHW_PT_SHIFT))))
                            PteDst.u = 0;
# endif
                        else
                            PteDst.u = PGM_PAGE_GET_HCPHYS(pPage) | PteDstBase.u;
# ifdef PGMPOOL_WITH_USER_TRACKING
                        if (PteDst.n.u1Present)
                            PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVM, pShwPage, pPage->HCPhys >> MM_RAM_FLAGS_IDX_SHIFT, pPage, iPTDst); /** @todo PAGE FLAGS */
# endif
                        /* commit it */
                        pPTDst->a[iPTDst] = PteDst;
                        Log4(("SyncPT: BIG %VGv PteDst:{P=%d RW=%d U=%d raw=%08llx}%s\n",
                              (RTGCPTR)(GCPtr | (iPTDst << SHW_PT_SHIFT)), PteDst.n.u1Present, PteDst.n.u1Write, PteDst.n.u1User, (uint64_t)PteDst.u,
                              PteDst.u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));

                        /* advance */
                        GCPhys += PAGE_SIZE;
                        iHCPage++;
                        iPTDst++;
                    } while (   iPTDst < ELEMENTS(pPTDst->a)
                             && GCPhys <= pRam->GCPhysLast);
                }
                else if (pRam)
                {
                    Log(("Invalid pages at %VGp\n", GCPhys));
                    do
                    {
                        pPTDst->a[iPTDst].u = 0; /* MMIO or invalid page, we must handle them manually. */
                        GCPhys += PAGE_SIZE;
                        iPTDst++;
                    } while (   iPTDst < ELEMENTS(pPTDst->a)
                             && GCPhys < pRam->GCPhys);
                }
                else
                {
                    Log(("Invalid pages at %VGp (2)\n", GCPhys));
                    for ( ; iPTDst < ELEMENTS(pPTDst->a); iPTDst++)
                        pPTDst->a[iPTDst].u = 0; /* MMIO or invalid page, we must handle them manually. */
                }
            } /* while more PTEs */
        } /* 4KB / 4MB */
    }
    else
        AssertRelease(!PdeDst.n.u1Present);

    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncPT), a);
# ifdef IN_GC
    if (VBOX_FAILURE(rc))
      STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncPTFailed));
# endif
    return rc;

#elif PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT

    int     rc     = VINF_SUCCESS;

    /*
     * Validate input a little bit.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PD          pPDDst = pVM->pgm.s.CTXMID(p,32BitPD);
# else
    PX86PDPAE       pPDDst = pVM->pgm.s.CTXMID(ap,PaePDs)[0];
# endif
    const unsigned  iPDDst = GCPtrPage >> SHW_PD_SHIFT;
    PSHWPDE         pPdeDst = &pPDDst->a[iPDDst];
    SHWPDE          PdeDst = *pPdeDst;

    Assert(!(PdeDst.u & PGM_PDFLAGS_MAPPING));
    Assert(!PdeDst.n.u1Present); /* We're only supposed to call SyncPT on PDE!P and conflicts.*/

    GSTPDE PdeSrc;
    PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
    PdeSrc.n.u1Present  = 1;
    PdeSrc.n.u1Write    = 1;
    PdeSrc.n.u1Accessed = 1;
    PdeSrc.n.u1User     = 1;

    /*
     * Allocate & map the page table.
     */
    PSHWPT          pPTDst;
    PPGMPOOLPAGE    pShwPage;
    RTGCPHYS        GCPhys;

    /* Virtual address = physical address */
    GCPhys = GCPtrPage & X86_PAGE_4K_BASE_MASK_32;
    rc = pgmPoolAlloc(pVM, GCPhys, BTH_PGMPOOLKIND_PT_FOR_PT, SHW_POOL_ROOT_IDX, iPDDst, &pShwPage);

    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_CACHED_PAGE)
        pPTDst = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    else
        AssertMsgFailedReturn(("rc=%Vrc\n", rc), VERR_INTERNAL_ERROR);

    PdeDst.u &= X86_PDE_AVL_MASK;
    PdeDst.u |= pShwPage->Core.Key;
    PdeDst.n.u1Present = 1;
    *pPdeDst = PdeDst;

    rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, (RTGCUINTPTR)GCPtrPage, PGM_SYNC_NR_PAGES, 0 /* page not present */);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncPT), a);
    return rc;

#else /* PGM_GST_TYPE != PGM_TYPE_32BIT */

    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncPT), a);
    return VERR_INTERNAL_ERROR;
#endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}



/**
 * Prefetch a page/set of pages.
 *
 * Typically used to sync commonly used pages before entering raw mode
 * after a CR3 reload.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtrPage   Page to invalidate.
 */
PGM_BTH_DECL(int, PrefetchPage)(PVM pVM, RTGCUINTPTR GCPtrPage)
{
#if (PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT) && PGM_SHW_TYPE != PGM_TYPE_AMD64

# if PGM_SHW_TYPE != PGM_TYPE_32BIT && PGM_SHW_TYPE != PGM_TYPE_PAE
#  error "Invalid shadow mode for 32-bit guest mode!"
# endif

    /*
     * Check that all Guest levels thru the PDE are present, getting the
     * PD and PDE in the processes.
     */
    int             rc      = VINF_SUCCESS;
# if PGM_WITH_PAGING(PGM_GST_TYPE)
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDSrc = (RTGCUINTPTR)GCPtrPage >> GST_PD_SHIFT;
    PGSTPD          pPDSrc = CTXSUFF(pVM->pgm.s.pGuestPD);
#  else /* PAE */
    unsigned        iPDSrc
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVM->pgm.s, GCPtrPage, &iPDSrc);
#  endif
# else
    PGSTPD          pPDSrc = NULL;
    const unsigned  iPDSrc = 0;
# endif

# if PGM_WITH_PAGING(PGM_GST_TYPE)
    const GSTPDE PdeSrc = pPDSrc->a[iPDSrc];
# else
    GSTPDE PdeSrc;
    PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
    PdeSrc.n.u1Present  = 1;
    PdeSrc.n.u1Write    = 1;
    PdeSrc.n.u1Accessed = 1;
    PdeSrc.n.u1User     = 1;
# endif

# ifdef PGM_SYNC_ACCESSED_BIT
    if (PdeSrc.n.u1Present && PdeSrc.n.u1Accessed)
# else
    if (PdeSrc.n.u1Present)
# endif
    {
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
        const X86PDE    PdeDst = pVM->pgm.s.CTXMID(p,32BitPD)->a[GCPtrPage >> SHW_PD_SHIFT];
# else
        const X86PDEPAE PdeDst = pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[GCPtrPage >> SHW_PD_SHIFT];
# endif
        if (!(PdeDst.u & PGM_PDFLAGS_MAPPING))
        {
            if (!PdeDst.n.u1Present)
                /** r=bird: This guy will set the A bit on the PDE, probably harmless. */
                rc = PGM_BTH_NAME(SyncPT)(pVM, iPDSrc, pPDSrc, GCPtrPage);
            else
            {
                /** @note We used to sync PGM_SYNC_NR_PAGES pages, which triggered assertions in CSAM, because
                 *        R/W attributes of nearby pages were reset. Not sure how that could happen. Anyway, it
                 *        makes no sense to prefetch more than one page.
                 */
                rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, GCPtrPage, 1, 0);
                if (VBOX_SUCCESS(rc))
                    rc = VINF_SUCCESS;
            }
        }
    }
    return rc;

#else /* PGM_GST_TYPE != PGM_TYPE_32BIT */

    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_SHW_TYPE, PGM_GST_TYPE));
    return VERR_INTERNAL_ERROR;
#endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}




/**
 * Syncs a page during a PGMVerifyAccess() call.
 *
 * @returns VBox status code (informational included).
 * @param   GCPtrPage   The address of the page to sync.
 * @param   fPage       The effective guest page flags.
 * @param   uErr        The trap error code.
 */
PGM_BTH_DECL(int, VerifyAccessSyncPage)(PVM pVM, RTGCUINTPTR GCPtrPage, unsigned fPage, unsigned uErr)
{
    LogFlow(("VerifyAccessSyncPage: GCPtrPage=%VGv fPage=%#x uErr=%#x\n", GCPtrPage, fPage, uErr));

#if (PGM_GST_TYPE == PGM_TYPE_32BIT ||  PGM_GST_TYPE == PGM_TYPE_REAL ||  PGM_GST_TYPE == PGM_TYPE_PROT || PGM_GST_TYPE == PGM_TYPE_PAE) && PGM_SHW_TYPE != PGM_TYPE_AMD64

# if PGM_SHW_TYPE != PGM_TYPE_32BIT && PGM_SHW_TYPE != PGM_TYPE_PAE
#  error "Invalid shadow mode for 32-bit guest mode!"
# endif

#ifndef IN_RING0
    if (!(fPage & X86_PTE_US))
    {
        /*
         * Mark this page as safe.
         */
        /** @todo not correct for pages that contain both code and data!! */
        Log(("CSAMMarkPage %VGv; scanned=%d\n", GCPtrPage, true));
        CSAMMarkPage(pVM, (RTGCPTR)GCPtrPage, true);
    }
#endif
    /*
     * Get guest PD and index.
     */

# if PGM_WITH_PAGING(PGM_GST_TYPE)
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDSrc = (RTGCUINTPTR)GCPtrPage >> GST_PD_SHIFT;
    PGSTPD          pPDSrc = CTXSUFF(pVM->pgm.s.pGuestPD);
#  else /* PAE */
    unsigned        iPDSrc;
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVM->pgm.s, GCPtrPage, &iPDSrc);
#  endif
# else
    PGSTPD          pPDSrc = NULL;
    const unsigned  iPDSrc = 0;
# endif
    int         rc = VINF_SUCCESS;

    /*
     * First check if the shadow pd is present.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PDE     pPdeDst = &pVM->pgm.s.CTXMID(p,32BitPD)->a[GCPtrPage >> SHW_PD_SHIFT];
# else
    PX86PDEPAE  pPdeDst = &pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[GCPtrPage >> SHW_PD_SHIFT];
# endif
    if (!pPdeDst->n.u1Present)
    {
        rc = PGM_BTH_NAME(SyncPT)(pVM, iPDSrc, pPDSrc, GCPtrPage);
        AssertRC(rc);
        if (rc != VINF_SUCCESS)
            return rc;
    }

# if PGM_WITH_PAGING(PGM_GST_TYPE)
    /* Check for dirty bit fault */
    rc = PGM_BTH_NAME(CheckPageFault)(pVM, uErr, pPdeDst, &pPDSrc->a[iPDSrc], GCPtrPage);
    if (rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT)
        Log(("PGMVerifyAccess: success (dirty)\n"));
    else
    {
        GSTPDE PdeSrc = pPDSrc->a[iPDSrc];
#else
    {
        GSTPDE PdeSrc;
        PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
        PdeSrc.n.u1Present  = 1;
        PdeSrc.n.u1Write    = 1;
        PdeSrc.n.u1Accessed = 1;
        PdeSrc.n.u1User     = 1;

#endif /* PGM_WITH_PAGING(PGM_GST_TYPE) */
        Assert(rc != VINF_EM_RAW_GUEST_TRAP);
        if (uErr & X86_TRAP_PF_US)
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageOutOfSyncUser);
        else /* supervisor */
            STAM_COUNTER_INC(&pVM->pgm.s.StatGCPageOutOfSyncSupervisor);

        rc = PGM_BTH_NAME(SyncPage)(pVM, PdeSrc, GCPtrPage, 1, 0);
        if (VBOX_SUCCESS(rc))
        {
            /* Page was successfully synced */
            Log2(("PGMVerifyAccess: success (sync)\n"));
            rc = VINF_SUCCESS;
        }
        else
        {
            Log(("PGMVerifyAccess: access violation for %VGv rc=%d\n", GCPtrPage, rc));
            return VINF_EM_RAW_GUEST_TRAP;
        }
    }
    return rc;

#else /* PGM_GST_TYPE != PGM_TYPE_32BIT */

    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    return VERR_INTERNAL_ERROR;
#endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}


#if PGM_GST_TYPE == PGM_TYPE_32BIT
# if PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE
/**
 * Figures out which kind of shadow page this guest PDE warrants.
 *
 * @returns Shadow page kind.
 * @param   pPdeSrc     The guest PDE in question.
 * @param   cr4         The current guest cr4 value.
 */
DECLINLINE(PGMPOOLKIND) PGM_BTH_NAME(CalcPageKind)(const GSTPDE *pPdeSrc, uint32_t cr4)
{
    if (!pPdeSrc->n.u1Size || !(cr4 & X86_CR4_PSE))
        return BTH_PGMPOOLKIND_PT_FOR_PT;
    //switch (pPdeSrc->u & (X86_PDE4M_RW | X86_PDE4M_US /*| X86_PDE4M_PAE_NX*/))
    //{
    //    case 0:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_RO;
    //    case X86_PDE4M_RW:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_RW;
    //    case X86_PDE4M_US:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_US;
    //    case X86_PDE4M_RW | X86_PDE4M_US:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_RW_US;
#  if 0
    //    case X86_PDE4M_PAE_NX:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_NX;
    //    case X86_PDE4M_RW | X86_PDE4M_PAE_NX:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_RW_NX;
    //    case X86_PDE4M_US | X86_PDE4M_PAE_NX:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_US_NX;
    //    case X86_PDE4M_RW | X86_PDE4M_US | X86_PDE4M_PAE_NX:
    //      return BTH_PGMPOOLKIND_PT_FOR_BIG_RW_US_NX;
#  endif
            return BTH_PGMPOOLKIND_PT_FOR_BIG;
    //}
}
# endif
#endif

#undef MY_STAM_COUNTER_INC
#define MY_STAM_COUNTER_INC(a) do { } while (0)


/**
 * Syncs the paging hierarchy starting at CR3.
 *
 * @returns VBox status code, no specials.
 * @param   pVM         The virtual machine.
 * @param   cr0         Guest context CR0 register
 * @param   cr3         Guest context CR3 register
 * @param   cr4         Guest context CR4 register
 * @param   fGlobal     Including global page directories or not
 */
PGM_BTH_DECL(int, SyncCR3)(PVM pVM, uint32_t cr0, uint32_t cr3, uint32_t cr4, bool fGlobal)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT
# if PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE
    if (VM_FF_ISSET(pVM, VM_FF_PGM_SYNC_CR3))
        fGlobal = true; /* Change this CR3 reload to be a global one. */
# endif
#endif

    /*
     * Update page access handlers.
     * The virtual are always flushed, while the physical are only on demand.
     * WARNING: We are incorrectly not doing global flushing on Virtual Handler updates. We'll
     *          have to look into that later because it will have a bad influence on the performance.
     * @note SvL: There's no need for that. Just invalidate the virtual range(s).
     *      bird: Yes, but that won't work for aliases.
     */
    /** @todo this MUST go away. See #1557. */
    STAM_PROFILE_START(&pVM->pgm.s.CTXMID(Stat,SyncCR3Handlers), h);
    PGM_GST_NAME(HandlerVirtualUpdate)(pVM, cr4);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncCR3Handlers), h);

#ifdef PGMPOOL_WITH_MONITORING
    /*
     * When monitoring shadowed pages, we reset the modification counters on CR3 sync.
     * Occationally we will have to clear all the shadow page tables because we wanted
     * to monitor a page which was mapped by too many shadowed page tables. This operation
     * sometimes refered to as a 'lightweight flush'.
     */
    if (!(pVM->pgm.s.fSyncFlags & PGM_SYNC_CLEAR_PGM_POOL))
        pgmPoolMonitorModifiedClearAll(pVM);
    else
    {
# ifdef IN_RING3
        pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_CLEAR_PGM_POOL;
        pgmPoolClearAll(pVM);
# else
        LogFlow(("SyncCR3: PGM_SYNC_CLEAR_PGM_POOL is set -> VINF_PGM_SYNC_CR3\n"));
        return VINF_PGM_SYNC_CR3;
# endif
    }
#endif

    Assert(fGlobal || (cr4 & X86_CR4_PGE));
    MY_STAM_COUNTER_INC(fGlobal ? &pVM->pgm.s.CTXMID(Stat,SyncCR3Global) : &pVM->pgm.s.CTXMID(Stat,SyncCR3NotGlobal));

#if PGM_GST_TYPE == PGM_TYPE_32BIT
# if PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE
    /*
     * Get page directory addresses.
     */
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PDE     pPDEDst = &pVM->pgm.s.CTXMID(p,32BitPD)->a[0];
#  else
    PX86PDEPAE  pPDEDst = &pVM->pgm.s.CTXMID(ap,PaePDs)[0]->a[0];
#  endif

# if PGM_GST_TYPE == PGM_TYPE_32BIT
    PGSTPD          pPDSrc = CTXSUFF(pVM->pgm.s.pGuestPD);
# else /* PAE */
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVM->pgm.s, 0);
# endif

    Assert(pPDSrc);
#ifndef IN_GC
    Assert(MMPhysGCPhys2HCVirt(pVM, (RTGCPHYS)(cr3 & X86_CR3_PAGE_MASK), sizeof(*pPDSrc)) == pPDSrc);
#endif

    /*
     * Iterate the page directory.
     */
    PPGMMAPPING pMapping;
    unsigned    iPdNoMapping;
    const bool  fRawR0Enabled = EMIsRawRing0Enabled(pVM);
    PPGMPOOL    pPool         = pVM->pgm.s.CTXSUFF(pPool);

    /* Only check mappings if they are supposed to be put into the shadow page table. */
    if (pgmMapAreMappingsEnabled(&pVM->pgm.s))
    {
        pMapping      = pVM->pgm.s.CTXALLSUFF(pMappings);
        iPdNoMapping  = (pMapping) ? pMapping->GCPtr >> X86_PD_SHIFT : ~0U;     /** PAE todo */
    }
    else
    {
        pMapping      = 0;
        iPdNoMapping  = ~0U;
    }

    for (unsigned iPD = 0; iPD < ELEMENTS(pPDSrc->a); iPD++)
    {
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
        Assert(&pVM->pgm.s.CTXMID(p,32BitPD)->a[iPD] == pPDEDst);
#  else
        Assert(&pVM->pgm.s.CTXMID(ap,PaePDs)[iPD * 2 / 512]->a[iPD * 2 % 512] == pPDEDst);
#  endif
        register GSTPDE PdeSrc = pPDSrc->a[iPD];
        if (    PdeSrc.n.u1Present
            &&  (PdeSrc.n.u1User || fRawR0Enabled))
        {
            /*
             * Check for conflicts with GC mappings.
             */
            if (iPD == iPdNoMapping)
            {
                if (pVM->pgm.s.fMappingsFixed)
                {
                    /* It's fixed, just skip the mapping. */
                    const unsigned cPTs = pMapping->cPTs;
                    iPD += cPTs - 1;
                    pPDEDst += cPTs + (PGM_SHW_TYPE != PGM_TYPE_32BIT) * cPTs;
                    pMapping = pMapping->CTXALLSUFF(pNext);
                    iPdNoMapping = pMapping ? pMapping->GCPtr >> X86_PD_SHIFT : ~0U;
                    continue;
                }

#ifdef IN_RING3
                int rc = pgmR3SyncPTResolveConflict(pVM, pMapping, pPDSrc, iPD);
                if (VBOX_FAILURE(rc))
                    return rc;

                /*
                 * Update iPdNoMapping and pMapping.
                 */
                pMapping = pVM->pgm.s.pMappingsR3;
                while (pMapping && pMapping->GCPtr < (iPD << X86_PD_SHIFT))
                    pMapping = pMapping->pNextR3;
                iPdNoMapping = pMapping ? pMapping->GCPtr >> X86_PD_SHIFT : ~0U;
#else
                LogFlow(("SyncCR3: detected conflict -> VINF_PGM_SYNC_CR3\n"));
                return VINF_PGM_SYNC_CR3;
#endif
            }

            /*
             * Sync page directory entry.
             *
             * The current approach is to allocated the page table but to set
             * the entry to not-present and postpone the page table synching till
             * it's actually used.
             */
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
            const unsigned iPdShw = iPD; NOREF(iPdShw);
#  else
            for (unsigned i = 0, iPdShw = iPD * 2; i < 2; i++, iPdShw++) /* pray that the compiler unrolls this */
#  endif
            {
                SHWPDE PdeDst = *pPDEDst;
                if (PdeDst.n.u1Present)
                {
                    PPGMPOOLPAGE pShwPage = pgmPoolGetPage(pPool, PdeDst.u & SHW_PDE_PG_MASK);
                    RTGCPHYS     GCPhys;
                    if (    !PdeSrc.b.u1Size
                        ||  !(cr4 & X86_CR4_PSE))
                    {
                        GCPhys = PdeSrc.u & GST_PDE_PG_MASK;
#  if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                        GCPhys |= i * (PAGE_SIZE / 2);
#  endif
                    }
                    else
                    {
                        GCPhys = PdeSrc.u & GST_PDE4M_PG_MASK;
#  if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                        GCPhys |= i * X86_PAGE_2M_SIZE;
#  endif
                    }

                    if (    pShwPage->GCPhys == GCPhys
                        &&  pShwPage->enmKind == PGM_BTH_NAME(CalcPageKind)(&PdeSrc, cr4)
                        &&  (   pShwPage->fCached
                             || (   !fGlobal
                                 && (   false
#  ifdef PGM_SKIP_GLOBAL_PAGEDIRS_ON_NONGLOBAL_FLUSH
                                     || (   (PdeSrc.u & (X86_PDE4M_PS | X86_PDE4M_G)) == (X86_PDE4M_PS | X86_PDE4M_G)
                                         && (cr4 & (X86_CR4_PGE | X86_CR4_PSE)) == (X86_CR4_PGE | X86_CR4_PSE)) /* global 2/4MB page. */
                                     || (  !pShwPage->fSeenNonGlobal
                                         && (cr4 & X86_CR4_PGE))
#  endif
                                     )
                                )
                            )
                        &&  (   (PdeSrc.u & (X86_PDE_US | X86_PDE_RW)) == (PdeDst.u & (X86_PDE_US | X86_PDE_RW))
                             || (   (cr4 & X86_CR4_PSE)
                                 &&     ((PdeSrc.u & (X86_PDE_US | X86_PDE4M_PS | X86_PDE4M_D)) | PGM_PDFLAGS_TRACK_DIRTY)
                                    ==  ((PdeDst.u & (X86_PDE_US | X86_PDE_RW | PGM_PDFLAGS_TRACK_DIRTY)) | X86_PDE4M_PS))
                            )
                       )
                    {
#  ifdef VBOX_WITH_STATISTICS
                        if (   !fGlobal
                            && (PdeSrc.u & (X86_PDE4M_PS | X86_PDE4M_G)) == (X86_PDE4M_PS | X86_PDE4M_G)
                            && (cr4 & (X86_CR4_PGE | X86_CR4_PSE)) == (X86_CR4_PGE | X86_CR4_PSE))
                            MY_STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncCR3DstSkippedGlobalPD));
                        else if (!fGlobal && !pShwPage->fSeenNonGlobal && (cr4 & X86_CR4_PGE))
                            MY_STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncCR3DstSkippedGlobalPT));
                        else
                            MY_STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncCR3DstCacheHit));
#  endif /* VBOX_WITH_STATISTICS */
/** @todo a replacement strategy isn't really needed unless we're using a very small pool < 512 pages.
 * The whole ageing stuff should be put in yet another set of #ifdefs. For now, let's just skip it. */
//#  ifdef PGMPOOL_WITH_CACHE
//                        pgmPoolCacheUsed(pPool, pShwPage);
//#  endif
                    }
                    else
                    {
                        pgmPoolFreeByPage(pPool, pShwPage, SHW_POOL_ROOT_IDX, iPdShw);
                        pPDEDst->u = 0;
                        MY_STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncCR3DstFreed));
                    }
                }
                else
                    MY_STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncCR3DstNotPresent));
                pPDEDst++;
            }
        }
        else if (iPD != iPdNoMapping)
        {
            /*
             * Check if there is any page directory to mark not present here.
             */
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
            const unsigned iPdShw = iPD; NOREF(iPdShw);
#  else
            for (unsigned i = 0, iPdShw = iPD * 2; i < 2; i++, iPdShw++) /* pray that the compiler unrolls this */
#  endif
            {
                if (pPDEDst->n.u1Present)
                {
                    pgmPoolFreeByPage(pPool, pgmPoolGetPage(pPool, pPDEDst->u & SHW_PDE_PG_MASK), SHW_POOL_ROOT_IDX, iPdShw);
                    pPDEDst->u = 0;
                    MY_STAM_COUNTER_INC(&pVM->pgm.s.CTXMID(Stat,SyncCR3DstFreedSrcNP));
                }
                pPDEDst++;
            }
        }
        else
        {
            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
            const unsigned cPTs = pMapping->cPTs;
            if (pVM->pgm.s.fMappingsFixed)
            {
                /* It's fixed, just skip the mapping. */
                pMapping = pMapping->CTXALLSUFF(pNext);
                iPdNoMapping = pMapping ? pMapping->GCPtr >> X86_PD_SHIFT : ~0U;
            }
            else
            {
                /*
                 * Check for conflicts for subsequent pagetables
                 * and advance to the next mapping.
                 */
                iPdNoMapping = ~0U;
                unsigned iPT = cPTs;
                while (iPT-- > 1)
                {
                    if (    pPDSrc->a[iPD + iPT].n.u1Present
                        &&  (pPDSrc->a[iPD + iPT].n.u1User || fRawR0Enabled))
                    {
#  ifdef IN_RING3
                        int rc = pgmR3SyncPTResolveConflict(pVM, pMapping, pPDSrc, iPD);
                        if (VBOX_FAILURE(rc))
                            return rc;

                        /*
                         * Update iPdNoMapping and pMapping.
                         */
                        pMapping = pVM->pgm.s.CTXALLSUFF(pMappings);
                        while (pMapping && pMapping->GCPtr < (iPD << X86_PD_SHIFT))
                            pMapping = pMapping->CTXALLSUFF(pNext);
                        iPdNoMapping = pMapping ? pMapping->GCPtr >> X86_PD_SHIFT : ~0U;
                        break;
#  else
                        LogFlow(("SyncCR3: detected conflict -> VINF_PGM_SYNC_CR3\n"));
                        return VINF_PGM_SYNC_CR3;
#  endif
                    }
                }
                if (iPdNoMapping == ~0U && pMapping)
                {
                    pMapping = pMapping->CTXALLSUFF(pNext);
                    if (pMapping)
                        iPdNoMapping = pMapping->GCPtr >> X86_PD_SHIFT;
                }
            }
            /* advance. */
            iPD += cPTs - 1;
            pPDEDst += cPTs + (PGM_SHW_TYPE != PGM_TYPE_32BIT) * cPTs;
        }

    } /* for iPD */
# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
#  error "Guest 32-bit mode and shadow AMD64 mode doesn't add up!"
# endif

    return VINF_SUCCESS;

#elif PGM_GST_TYPE == PGM_TYPE_PAE
# if PGM_SHW_TYPE == PGM_TYPE_PAE
//# error not implemented
    return VERR_INTERNAL_ERROR;

# else  /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */
#  error "Guest PAE mode, but not the shadow mode ; 32bit - maybe, but amd64 no."
# endif /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */

#elif PGM_GST_TYPE == PGM_TYPE_AMD64
# if PGM_SHW_TYPE == PGM_TYPE_AMD64
//# error not implemented
    return VERR_INTERNAL_ERROR;

# else  /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */
#  error "Guest AMD64 mode, but not the shadow mode - that can't be right!"
# endif /* PGM_SHW_TYPE != PGM_TYPE_AMD64 */

#else /* guest real and protected mode */

    return VINF_SUCCESS;
#endif
}




#ifdef VBOX_STRICT
#ifdef IN_GC
# undef AssertMsgFailed
# define AssertMsgFailed Log
#endif
#ifdef IN_RING3
# include <VBox/dbgf.h>

/**
 * Dumps a page table hierarchy use only physical addresses and cr4/lm flags.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   pVM         The VM handle.
 * @param   cr3         The root of the hierarchy.
 * @param   crr         The cr4, only PAE and PSE is currently used.
 * @param   fLongMode   Set if long mode, false if not long mode.
 * @param   cMaxDepth   Number of levels to dump.
 * @param   pHlp        Pointer to the output functions.
 */
__BEGIN_DECLS
PGMR3DECL(int) PGMR3DumpHierarchyHC(PVM pVM, uint32_t cr3, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp);
__END_DECLS

#endif

/**
 * Checks that the shadow page table is in sync with the guest one.
 *
 * @returns The number of errors.
 * @param   pVM         The virtual machine.
 * @param   cr3         Guest context CR3 register
 * @param   cr4         Guest context CR4 register
 * @param   GCPtr       Where to start. Defaults to 0.
 * @param   cb          How much to check. Defaults to everything.
 */
PGM_BTH_DECL(unsigned, AssertCR3)(PVM pVM, uint32_t cr3, uint32_t cr4, RTGCUINTPTR GCPtr, RTGCUINTPTR cb)
{
    unsigned    cErrors = 0;

#if PGM_GST_TYPE == PGM_TYPE_32BIT

# if PGM_SHW_TYPE != PGM_TYPE_32BIT && PGM_SHW_TYPE != PGM_TYPE_PAE
#  error "Invalid shadow mode for 32-bit guest paging."
# endif

    PPGM        pPGM = &pVM->pgm.s;
    RTHCPHYS    HCPhysShw;              /* page address derived from the shadow page tables. */
    RTGCPHYS    GCPhysGst;              /* page address derived from the guest page tables. */
    RTHCPHYS    HCPhys;                 /* general usage. */
    int         rc;

    /*
     * Check that the Guest CR3 and all it's mappings are correct.
     */
    AssertMsgReturn(pPGM->GCPhysCR3 == (cr3 & X86_CR3_PAGE_MASK),
                    ("Invalid GCPhysCR3=%VGp cr3=%VGp\n", pPGM->GCPhysCR3, (RTGCPHYS)cr3),
                    false);
    rc = PGMShwGetPage(pVM, pPGM->pGuestPDGC, NULL, &HCPhysShw);
    AssertRCReturn(rc, 1);
    HCPhys = NIL_RTHCPHYS;
    rc = pgmRamGCPhys2HCPhys(pPGM, cr3 & X86_CR3_PAGE_MASK, &HCPhys);
    AssertMsgReturn(HCPhys == HCPhysShw, ("HCPhys=%VHp HCPhyswShw=%VHp (cr3)\n", HCPhys, HCPhysShw), false);
# ifdef IN_RING3
    RTGCPHYS GCPhys;
    rc = PGMR3DbgHCPtr2GCPhys(pVM, pPGM->pGuestPDHC, &GCPhys);
    AssertRCReturn(rc, 1);
    AssertMsgReturn((cr3 & X86_CR3_PAGE_MASK) == GCPhys, ("GCPhys=%VGp cr3=%VGp\n", GCPhys, (RTGCPHYS)cr3), false);
# endif
    const X86PD *pPDSrc = CTXSUFF(pPGM->pGuestPD);

    /*
     * Get and check the Shadow CR3.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    const X86PD    *pPDDst  = pPGM->CTXMID(p,32BitPD);
    unsigned        cPDEs   = ELEMENTS(pPDDst->a);
# else
    const X86PDPAE *pPDDst  = pPGM->CTXMID(ap,PaePDs[0]); /* use it as a 2048 entry PD */
    unsigned        cPDEs   = ELEMENTS(pPDDst->a) * ELEMENTS(pPGM->apHCPaePDs);
# endif
    if (cb != ~(RTGCUINTPTR)0)
        cPDEs = RT_MIN(cb >> SHW_PD_SHIFT, 1);

/** @todo call the other two PGMAssert*() functions. */

    /*
     * Iterate the shadow page directory.
     */
    GCPtr = (GCPtr >> SHW_PD_SHIFT) << SHW_PD_SHIFT;
    unsigned iPDDst = GCPtr >> SHW_PD_SHIFT;
    cPDEs += iPDDst;
    for (;
         iPDDst < cPDEs;
         iPDDst++, GCPtr += _4G / cPDEs)
    {
        const SHWPDE PdeDst = pPDDst->a[iPDDst];
        if (PdeDst.u & PGM_PDFLAGS_MAPPING)
        {
            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
            if ((PdeDst.u & X86_PDE_AVL_MASK) != PGM_PDFLAGS_MAPPING)
            {
                AssertMsgFailed(("Mapping shall only have PGM_PDFLAGS_MAPPING set! PdeDst.u=%#RX64\n", (uint64_t)PdeDst.u));
                cErrors++;
                continue;
            }
        }
        else if (   (PdeDst.u & X86_PDE_P)
                 || ((PdeDst.u & (X86_PDE_P | PGM_PDFLAGS_TRACK_DIRTY)) == (X86_PDE_P | PGM_PDFLAGS_TRACK_DIRTY))
                )
        {
            HCPhysShw = PdeDst.u & SHW_PDE_PG_MASK;
            PPGMPOOLPAGE pPoolPage = pgmPoolGetPageByHCPhys(pVM, HCPhysShw);
            if (!pPoolPage)
            {
                AssertMsgFailed(("Invalid page table address %VGp at %VGv! PdeDst=%#RX64\n",
                                 HCPhysShw, GCPtr, (uint64_t)PdeDst.u));
                cErrors++;
                continue;
            }
            const SHWPT *pPTDst = (const SHWPT *)PGMPOOL_PAGE_2_PTR(pVM, pPoolPage);

            if (PdeDst.u & (X86_PDE4M_PWT | X86_PDE4M_PCD))
            {
                AssertMsgFailed(("PDE flags PWT and/or PCD is set at %VGv! These flags are not virtualized! PdeDst=%#RX64\n",
                                 GCPtr, (uint64_t)PdeDst.u));
                cErrors++;
            }

            if (PdeDst.u & (X86_PDE4M_G | X86_PDE4M_D))
            {
                AssertMsgFailed(("4K PDE reserved flags at %VGv! PdeDst=%#RX64\n",
                                 GCPtr, (uint64_t)PdeDst.u));
                cErrors++;
            }

            const X86PDE    PdeSrc = pPDSrc->a[iPDDst >> (GST_PD_SHIFT - SHW_PD_SHIFT)];
            if (!PdeSrc.n.u1Present)
            {
                AssertMsgFailed(("Guest PDE at %VGv is not present! PdeDst=%#RX64 PdeSrc=%#RX64\n",
                                 GCPtr, (uint64_t)PdeDst.u, (uint64_t)PdeSrc.u));
                cErrors++;
                continue;
            }

            if (    !PdeSrc.b.u1Size
                ||  !(cr4 & X86_CR4_PSE))
            {
                GCPhysGst = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                GCPhysGst |= (iPDDst & 1) * (PAGE_SIZE / 2);
# endif
            }
            else
            {
                if (PdeSrc.u & X86_PDE4M_PG_HIGH_MASK)
                {
                    AssertMsgFailed(("Guest PDE at %VGv is using PSE36 or similar! PdeSrc=%#RX64\n",
                                     GCPtr, (uint64_t)PdeSrc.u));
                    cErrors++;
                    continue;
                }
                GCPhysGst = PdeSrc.u & GST_PDE4M_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                GCPhysGst |= GCPtr & RT_BIT(X86_PAGE_2M_SHIFT);
# endif
            }

            if (    pPoolPage->enmKind
                !=  (!PdeSrc.b.u1Size || !(cr4 & X86_CR4_PSE) ? BTH_PGMPOOLKIND_PT_FOR_PT : BTH_PGMPOOLKIND_PT_FOR_BIG))
            {
                AssertMsgFailed(("Invalid shadow page table kind %d at %VGv! PdeSrc=%#RX64\n",
                                 pPoolPage->enmKind, GCPtr, (uint64_t)PdeSrc.u));
                cErrors++;
            }

            PPGMPAGE pPhysPage = pgmPhysGetPage(pPGM, GCPhysGst);
            if (!pPhysPage)
            {
                AssertMsgFailed(("Cannot find guest physical address %VGp in the PDE at %VGv! PdeSrc=%#RX64\n",
                                 GCPhysGst, GCPtr, (uint64_t)PdeSrc.u));
                cErrors++;
                continue;
            }

            if (GCPhysGst != pPoolPage->GCPhys)
            {
                AssertMsgFailed(("GCPhysGst=%VGp != pPage->GCPhys=%VGp at %VGv\n",
                                 GCPhysGst, pPoolPage->GCPhys, GCPtr));
                cErrors++;
                continue;
            }

            if (    !PdeSrc.b.u1Size
                ||  !(cr4 & X86_CR4_PSE))
            {
                /*
                 * Page Table.
                 */
                const GSTPT *pPTSrc;
                rc = PGM_GCPHYS_2_PTR(pVM, GCPhysGst & ~(RTGCPHYS)(PAGE_SIZE - 1), &pPTSrc);
                if (VBOX_FAILURE(rc))
                {
                    AssertMsgFailed(("Cannot map/convert guest physical address %VGp in the PDE at %VGv! PdeSrc=%#RX64\n",
                                     GCPhysGst, GCPtr, (uint64_t)PdeSrc.u));
                    cErrors++;
                    continue;
                }
                if (    (PdeSrc.u & (X86_PDE_P | X86_PDE_US | X86_PDE_RW/* | X86_PDE_A*/))
                    !=  (PdeDst.u & (X86_PDE_P | X86_PDE_US | X86_PDE_RW/* | X86_PDE_A*/)))
                {
                    /// @todo We get here a lot on out-of-sync CR3 entries. The access handler should zap them to avoid false alarms here!
                    // (This problem will go away when/if we shadow multiple CR3s.)
                    AssertMsgFailed(("4K PDE flags mismatch at %VGv! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                     GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                    cErrors++;
                    continue;
                }
                if (PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY)
                {
                    AssertMsgFailed(("4K PDEs cannot have PGM_PDFLAGS_TRACK_DIRTY set! GCPtr=%VGv PdeDst=%#RX64\n",
                                     GCPtr, (uint64_t)PdeDst.u));
                    cErrors++;
                    continue;
                }

                /* iterate the page table. */
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
                const unsigned offPTSrc  = 0;
#  else
                const unsigned offPTSrc  = ((GCPtr >> SHW_PD_SHIFT) & 1) * 512;
#  endif
                for (unsigned iPT = 0, off = 0;
                     iPT < ELEMENTS(pPTDst->a);
                     iPT++, off += PAGE_SIZE)
                {
                    const SHWPTE PteDst = pPTDst->a[iPT];

                    /* skip not-present entries. */
                    if (!(PteDst.u & (X86_PTE_P | PGM_PTFLAGS_TRACK_DIRTY))) /** @todo deal with ALL handlers and CSAM !P pages! */
                        continue;
                    Assert(PteDst.n.u1Present);

                    const GSTPTE PteSrc = pPTSrc->a[iPT + offPTSrc];
                    if (!PteSrc.n.u1Present)
                    {
#ifdef IN_RING3
                        PGMAssertHandlerAndFlagsInSync(pVM);
                        PGMR3DumpHierarchyGC(pVM, cr3, cr4, (PdeSrc.u & GST_PDE_PG_MASK));
#endif
                        AssertMsgFailed(("Out of sync (!P) PTE at %VGv! PteSrc=%#RX64 PteDst=%#RX64 pPTSrc=%VGv iPTSrc=%x PdeSrc=%x physpte=%VGp\n",
                                         GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u, pPTSrc, iPT + offPTSrc, PdeSrc.au32[0],
                                         (PdeSrc.u & GST_PDE_PG_MASK) + (iPT + offPTSrc)*sizeof(PteSrc)));
                        cErrors++;
                        continue;
                    }

                    uint64_t fIgnoreFlags = GST_PTE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_G | X86_PTE_D | X86_PTE_PWT | X86_PTE_PCD | X86_PTE_PAT;
# if 1 /** @todo sync accessed bit properly... */
                    fIgnoreFlags |= X86_PTE_A;
# endif

                    /* match the physical addresses */
                    HCPhysShw = PteDst.u & SHW_PTE_PG_MASK;
                    GCPhysGst = PteSrc.u & GST_PTE_PG_MASK;

# ifdef IN_RING3
                    rc = PGMPhysGCPhys2HCPhys(pVM, GCPhysGst, &HCPhys);
                    if (VBOX_FAILURE(rc))
                    {
                        if (HCPhysShw != MMR3PageDummyHCPhys(pVM))
                        {
                            AssertMsgFailed(("Cannot find guest physical address %VGp at %VGv! PteSrc=%#RX64 PteDst=%#RX64\n",
                                             GCPhysGst, GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                            cErrors++;
                            continue;
                        }
                    }
                    else if (HCPhysShw != (HCPhys & SHW_PTE_PG_MASK))
                    {
                        AssertMsgFailed(("Out of sync (phys) at %VGv! HCPhysShw=%VHp HCPhys=%VHp GCPhysGst=%VGp PteSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, HCPhysShw, HCPhys, GCPhysGst, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                        continue;
                    }
# endif

                    pPhysPage = pgmPhysGetPage(pPGM, GCPhysGst);
                    if (!pPhysPage)
                    {
# ifdef IN_RING3 /** @todo make MMR3PageDummyHCPhys an 'All' function! */
                        if (HCPhysShw != MMR3PageDummyHCPhys(pVM))
                        {
                            AssertMsgFailed(("Cannot find guest physical address %VGp at %VGv! PteSrc=%#RX64 PteDst=%#RX64\n",
                                             GCPhysGst, GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                            cErrors++;
                            continue;
                        }
# endif
                        if (PteDst.n.u1Write)
                        {
                            AssertMsgFailed(("Invalid guest page at %VGv is writable! GCPhysGst=%VGp PteSrc=%#RX64 PteDst=%#RX64\n",
                                             GCPtr + off, GCPhysGst, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                            cErrors++;
                        }
                        fIgnoreFlags |= X86_PTE_RW;
                    }
                    else if (HCPhysShw != (PGM_PAGE_GET_HCPHYS(pPhysPage) & SHW_PTE_PG_MASK))
                    {
                        AssertMsgFailed(("Out of sync (phys) at %VGv! HCPhysShw=%VHp HCPhys=%VHp GCPhysGst=%VGp PteSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, HCPhysShw, pPhysPage->HCPhys, GCPhysGst, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                        continue;
                    }

                    /* flags */
                    if (PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPhysPage))
                    {
                        if (!PGM_PAGE_HAVE_ACTIVE_ALL_HANDLERS(pPhysPage))
                        {
                            if (PteDst.n.u1Write)
                            {
                                AssertMsgFailed(("WRITE access flagged at %VGv but the page is writable! HCPhys=%VGv PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, pPhysPage->HCPhys, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                            fIgnoreFlags |= X86_PTE_RW;
                        }
                        else
                        {
                            if (PteDst.n.u1Present)
                            {
                                AssertMsgFailed(("ALL access flagged at %VGv but the page is present! HCPhys=%VHp PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, pPhysPage->HCPhys, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                            fIgnoreFlags |= X86_PTE_P;
                        }
                    }
                    else
                    {
                        if (!PteSrc.n.u1Dirty && PteSrc.n.u1Write)
                        {
                            if (PteDst.n.u1Write)
                            {
                                AssertMsgFailed(("!DIRTY page at %VGv is writable! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                            if (!(PteDst.u & PGM_PTFLAGS_TRACK_DIRTY))
                            {
                                AssertMsgFailed(("!DIRTY page at %VGv is not marked TRACK_DIRTY! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                            if (PteDst.n.u1Dirty)
                            {
                                AssertMsgFailed(("!DIRTY page at %VGv is marked DIRTY! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                            }
# if 0 /** @todo sync access bit properly... */
                            if (PteDst.n.u1Accessed != PteSrc.n.u1Accessed)
                            {
                                AssertMsgFailed(("!DIRTY page at %VGv is has mismatching accessed bit! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                            }
                            fIgnoreFlags |= X86_PTE_RW;
# else
                            fIgnoreFlags |= X86_PTE_RW | X86_PTE_A;
# endif
                        }
                        else if (PteDst.u & PGM_PTFLAGS_TRACK_DIRTY)
                        {
                            /* access bit emulation (not implemented). */
                            if (PteSrc.n.u1Accessed || PteDst.n.u1Present)
                            {
                                AssertMsgFailed(("PGM_PTFLAGS_TRACK_DIRTY set at %VGv but no accessed bit emulation! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                            if (!PteDst.n.u1Accessed)
                            {
                                AssertMsgFailed(("!ACCESSED page at %VGv is has the accessed bit set! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                            }
                            fIgnoreFlags |= X86_PTE_P;
                        }
# ifdef DEBUG_sandervl
                        fIgnoreFlags |= X86_PTE_D | X86_PTE_A;
# endif
                    }

                    if (    (PteSrc.u & ~fIgnoreFlags) != (PteDst.u & ~fIgnoreFlags)
                        &&  (PteSrc.u & ~(fIgnoreFlags | X86_PTE_RW)) != (PteDst.u & ~fIgnoreFlags)
                       )
                    {
                        AssertMsgFailed(("Flags mismatch at %VGv! %#RX64 != %#RX64 fIgnoreFlags=%#RX64 PteSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, (uint64_t)PteSrc.u & ~fIgnoreFlags, (uint64_t)PteDst.u & ~fIgnoreFlags,
                                         fIgnoreFlags, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                        continue;
                    }
                } /* foreach PTE */
            }
            else
            {
                /*
                 * Big Page.
                 */
                uint64_t fIgnoreFlags = X86_PDE_AVL_MASK | X86_PDE_PAE_PG_MASK | X86_PDE4M_G | X86_PDE4M_D | X86_PDE4M_PS | X86_PDE4M_PWT | X86_PDE4M_PCD;
                if (!PdeSrc.b.u1Dirty && PdeSrc.b.u1Write)
                {
                    if (PdeDst.n.u1Write)
                    {
                        AssertMsgFailed(("!DIRTY page at %VGv is writable! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                         GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                        cErrors++;
                        continue;
                    }
                    if (!(PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY))
                    {
                        AssertMsgFailed(("!DIRTY page at %VGv is not marked TRACK_DIRTY! PteSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                        cErrors++;
                        continue;
                    }
# if 0 /** @todo sync access bit properly... */
                    if (PdeDst.n.u1Accessed != PdeSrc.b.u1Accessed)
                    {
                        AssertMsgFailed(("!DIRTY page at %VGv is has mismatching accessed bit! PteSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                        cErrors++;
                    }
                    fIgnoreFlags |= X86_PTE_RW;
# else
                    fIgnoreFlags |= X86_PTE_RW | X86_PTE_A;
# endif
                }
                else if (PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY)
                {
                    /* access bit emulation (not implemented). */
                    if (PdeSrc.b.u1Accessed || PdeDst.n.u1Present)
                    {
                        AssertMsgFailed(("PGM_PDFLAGS_TRACK_DIRTY set at %VGv but no accessed bit emulation! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                         GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                        cErrors++;
                        continue;
                    }
                    if (!PdeDst.n.u1Accessed)
                    {
                        AssertMsgFailed(("!ACCESSED page at %VGv is has the accessed bit set! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                         GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                        cErrors++;
                    }
                    fIgnoreFlags |= X86_PTE_P;
                }

                if ((PdeSrc.u & ~fIgnoreFlags) != (PdeDst.u & ~fIgnoreFlags))
                {
                    AssertMsgFailed(("Flags mismatch (B) at %VGv! %#RX64 != %#RX64 fIgnoreFlags=%#RX64 PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                     GCPtr, (uint64_t)PdeSrc.u & ~fIgnoreFlags, (uint64_t)PdeDst.u & ~fIgnoreFlags,
                                     fIgnoreFlags, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                    cErrors++;
                }

                /* iterate the page table. */
                for (unsigned iPT = 0, off = 0;
                     iPT < ELEMENTS(pPTDst->a);
                     iPT++, off += PAGE_SIZE, GCPhysGst += PAGE_SIZE)
                {
                    const SHWPTE PteDst = pPTDst->a[iPT];

                    if (PteDst.u & PGM_PTFLAGS_TRACK_DIRTY)
                    {
                        AssertMsgFailed(("The PTE at %VGv emulating a 2/4M page is marked TRACK_DIRTY! PdeSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                    }

                    /* skip not-present entries. */
                    if (!PteDst.n.u1Present) /** @todo deal with ALL handlers and CSAM !P pages! */
                        continue;

                    fIgnoreFlags = X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PWT | X86_PTE_PCD | X86_PTE_PAT;

                    /* match the physical addresses */
                    HCPhysShw = PteDst.u & X86_PTE_PAE_PG_MASK;

# ifdef IN_RING3
                    rc = PGMPhysGCPhys2HCPhys(pVM, GCPhysGst, &HCPhys);
                    if (VBOX_FAILURE(rc))
                    {
                        if (HCPhysShw != MMR3PageDummyHCPhys(pVM))
                        {
                            AssertMsgFailed(("Cannot find guest physical address %VGp at %VGv! PdeSrc=%#RX64 PteDst=%#RX64\n",
                                             GCPhysGst, GCPtr + off, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                            cErrors++;
                        }
                    }
                    else if (HCPhysShw != (HCPhys & X86_PTE_PAE_PG_MASK))
                    {
                        AssertMsgFailed(("Out of sync (phys) at %VGv! HCPhysShw=%VHp HCPhys=%VHp GCPhysGst=%VGp PdeSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, HCPhysShw, HCPhys, GCPhysGst, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                        continue;
                    }
# endif

                    pPhysPage = pgmPhysGetPage(pPGM, GCPhysGst);
                    if (!pPhysPage)
                    {
# ifdef IN_RING3 /** @todo make MMR3PageDummyHCPhys an 'All' function! */
                        if (HCPhysShw != MMR3PageDummyHCPhys(pVM))
                        {
                            AssertMsgFailed(("Cannot find guest physical address %VGp at %VGv! PdeSrc=%#RX64 PteDst=%#RX64\n",
                                             GCPhysGst, GCPtr + off, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                            cErrors++;
                            continue;
                        }
# endif
                        if (PteDst.n.u1Write)
                        {
                            AssertMsgFailed(("Invalid guest page at %VGv is writable! GCPhysGst=%VGp PdeSrc=%#RX64 PteDst=%#RX64\n",
                                             GCPtr + off, GCPhysGst, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                            cErrors++;
                        }
                        fIgnoreFlags |= X86_PTE_RW;
                    }
                    else if (HCPhysShw != (pPhysPage->HCPhys & X86_PTE_PAE_PG_MASK))
                    {
                        AssertMsgFailed(("Out of sync (phys) at %VGv! HCPhysShw=%VHp HCPhys=%VHp GCPhysGst=%VGp PdeSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, HCPhysShw, pPhysPage->HCPhys, GCPhysGst, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                        continue;
                    }

                    /* flags */
                    if (PGM_PAGE_HAVE_ACTIVE_HANDLERS(pPhysPage))
                    {
                        if (!PGM_PAGE_HAVE_ACTIVE_ALL_HANDLERS(pPhysPage))
                        {
                            if (PGM_PAGE_GET_HNDL_PHYS_STATE(pPhysPage) != PGM_PAGE_HNDL_PHYS_STATE_DISABLED)
                            {
                                if (PteDst.n.u1Write)
                                {
                                    AssertMsgFailed(("WRITE access flagged at %VGv but the page is writable! HCPhys=%VGv PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                     GCPtr + off, pPhysPage->HCPhys, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                    continue;
                                }
                                fIgnoreFlags |= X86_PTE_RW;
                            }
                        }
                        else
                        {
                            if (PteDst.n.u1Present)
                            {
                                AssertMsgFailed(("ALL access flagged at %VGv but the page is present! HCPhys=%VGv PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                 GCPtr + off, pPhysPage->HCPhys, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                            fIgnoreFlags |= X86_PTE_P;
                        }
                    }

                    if (    (PdeSrc.u & ~fIgnoreFlags) != (PteDst.u & ~fIgnoreFlags)
                        &&  (PdeSrc.u & ~(fIgnoreFlags | X86_PTE_RW)) != (PteDst.u & ~fIgnoreFlags) /* lazy phys handler dereg. */
                       )
                    {
                        AssertMsgFailed(("Flags mismatch (BT) at %VGv! %#RX64 != %#RX64 fIgnoreFlags=%#RX64 PdeSrc=%#RX64 PteDst=%#RX64\n",
                                         GCPtr + off, (uint64_t)PdeSrc.u & ~fIgnoreFlags, (uint64_t)PteDst.u & ~fIgnoreFlags,
                                         fIgnoreFlags, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                        cErrors++;
                        continue;
                    }
                } /* foreach PTE */
            }
        }
        /* not present */

    } /* forearch PDE */

# ifdef DEBUG
    if (cErrors)
        LogFlow(("AssertCR3: cErrors=%d\n", cErrors));
# endif

#elif PGM_GST_TYPE == PGM_TYPE_PAE
//# error not implemented


#elif PGM_GST_TYPE == PGM_TYPE_AMD64
//# error not implemented

/*#else: guest real and protected mode */
#endif
    return cErrors;
}
#endif /* VBOX_STRICT */

