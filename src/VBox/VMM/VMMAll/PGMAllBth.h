/* $Id$ */
/** @file
 * VBox - Page Manager, Shadow+Guest Paging Template - All context code.
 *
 * This file is a big challenge!
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
PGM_BTH_DECL(int, Trap0eHandler)(PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault);
PGM_BTH_DECL(int, InvalidatePage)(PVMCPU pVCpu, RTGCPTR GCPtrPage);
PGM_BTH_DECL(int, SyncPage)(PVMCPU pVCpu, GSTPDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uErr);
PGM_BTH_DECL(int, CheckPageFault)(PVMCPU pVCpu, uint32_t uErr, PSHWPDE pPdeDst, PGSTPDE pPdeSrc, RTGCPTR GCPtrPage);
PGM_BTH_DECL(int, SyncPT)(PVMCPU pVCpu, unsigned iPD, PGSTPD pPDSrc, RTGCPTR GCPtrPage);
PGM_BTH_DECL(int, VerifyAccessSyncPage)(PVMCPU pVCpu, RTGCPTR Addr, unsigned fPage, unsigned uErr);
PGM_BTH_DECL(int, PrefetchPage)(PVMCPU pVCpu, RTGCPTR GCPtrPage);
PGM_BTH_DECL(int, SyncCR3)(PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal);
#ifdef VBOX_STRICT
PGM_BTH_DECL(unsigned, AssertCR3)(PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr = 0, RTGCPTR cb = ~(RTGCPTR)0);
#endif
#ifdef PGMPOOL_WITH_USER_TRACKING
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorkerTrackDeref)(PVMCPU pVCpu, PPGMPOOLPAGE pShwPage, RTHCPHYS HCPhys);
#endif
PGM_BTH_DECL(int, MapCR3)(PVMCPU pVCpu, RTGCPHYS GCPhysCR3);
PGM_BTH_DECL(int, UnmapCR3)(PVMCPU pVCpu);
__END_DECLS


/* Filter out some illegal combinations of guest and shadow paging, so we can remove redundant checks inside functions. */
#if      PGM_GST_TYPE == PGM_TYPE_PAE && PGM_SHW_TYPE != PGM_TYPE_PAE && PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT
# error "Invalid combination; PAE guest implies PAE shadow"
#endif

#if     (PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT) \
    && !(PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE || PGM_SHW_TYPE == PGM_TYPE_AMD64 || PGM_SHW_TYPE == PGM_TYPE_NESTED || PGM_SHW_TYPE == PGM_TYPE_EPT)
# error "Invalid combination; real or protected mode without paging implies 32 bits or PAE shadow paging."
#endif

#if     (PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_PAE) \
    && !(PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE || PGM_SHW_TYPE == PGM_TYPE_NESTED || PGM_SHW_TYPE == PGM_TYPE_EPT)
# error "Invalid combination; 32 bits guest paging or PAE implies 32 bits or PAE shadow paging."
#endif

#if    (PGM_GST_TYPE == PGM_TYPE_AMD64 && PGM_SHW_TYPE != PGM_TYPE_AMD64 && PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT) \
    || (PGM_SHW_TYPE == PGM_TYPE_AMD64 && PGM_GST_TYPE != PGM_TYPE_AMD64 && PGM_GST_TYPE != PGM_TYPE_PROT)
# error "Invalid combination; AMD64 guest implies AMD64 shadow and vice versa"
#endif

#ifdef IN_RING0 /* no mappings in VT-x and AMD-V mode */
# define PGM_WITHOUT_MAPPINGS
#endif


#ifndef IN_RING3
/**
 * #PF Handler for raw-mode guest execution.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 *
 * @param   pVCpu       VMCPU Handle.
 * @param   uErr        The trap error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address.
 */
PGM_BTH_DECL(int, Trap0eHandler)(PVMCPU pVCpu, RTGCUINT uErr, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

# if defined(IN_RC) && defined(VBOX_STRICT)
    PGMDynCheckLocks(pVM);
# endif

# if  (PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT || PGM_GST_TYPE == PGM_TYPE_PAE || PGM_GST_TYPE == PGM_TYPE_AMD64) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED    \
    && (PGM_SHW_TYPE != PGM_TYPE_EPT || PGM_GST_TYPE == PGM_TYPE_PROT)

#  if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE != PGM_TYPE_PAE
    /*
     * Hide the instruction fetch trap indicator for now.
     */
    /** @todo NXE will change this and we must fix NXE in the switcher too! */
    if (uErr & X86_TRAP_PF_ID)
    {
        uErr &= ~X86_TRAP_PF_ID;
        TRPMSetErrorCode(pVCpu, uErr);
    }
#  endif

    /*
     * Get PDs.
     */
    int             rc;
#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
#   if PGM_GST_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDSrc = pvFault >> GST_PD_SHIFT;
    PGSTPD          pPDSrc = pgmGstGet32bitPDPtr(&pVCpu->pgm.s);

#   elif PGM_GST_TYPE == PGM_TYPE_PAE || PGM_GST_TYPE == PGM_TYPE_AMD64

#    if PGM_GST_TYPE == PGM_TYPE_PAE
    unsigned        iPDSrc;
    X86PDPE         PdpeSrc;
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVCpu->pgm.s, pvFault, &iPDSrc, &PdpeSrc);

#    elif PGM_GST_TYPE == PGM_TYPE_AMD64
    unsigned        iPDSrc;
    PX86PML4E       pPml4eSrc;
    X86PDPE         PdpeSrc;
    PGSTPD          pPDSrc;

    pPDSrc = pgmGstGetLongModePDPtr(&pVCpu->pgm.s, pvFault, &pPml4eSrc, &PdpeSrc, &iPDSrc);
    Assert(pPml4eSrc);
#    endif

    /* Quick check for a valid guest trap. (PAE & AMD64) */
    if (!pPDSrc)
    {
#    if PGM_GST_TYPE == PGM_TYPE_AMD64 && GC_ARCH_BITS == 64
        LogFlow(("Trap0eHandler: guest PML4 %d not present CR3=%RGp\n", (int)((pvFault >> X86_PML4_SHIFT) & X86_PML4_MASK), CPUMGetGuestCR3(pVCpu) & X86_CR3_PAGE_MASK));
#    else
        LogFlow(("Trap0eHandler: guest iPDSrc=%u not present CR3=%RGp\n", iPDSrc, CPUMGetGuestCR3(pVCpu) & X86_CR3_PAGE_MASK));
#    endif
        STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2GuestTrap; });
        TRPMSetErrorCode(pVCpu, uErr);
        return VINF_EM_RAW_GUEST_TRAP;
    }
#   endif

#  else  /* !PGM_WITH_PAGING */
    PGSTPD          pPDSrc = NULL;
    const unsigned  iPDSrc = 0;
#  endif /* !PGM_WITH_PAGING */

    /* Fetch the guest PDE */
#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
    GSTPDE PdeSrc = pPDSrc->a[iPDSrc];
#  else
    GSTPDE PdeSrc;
    PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
    PdeSrc.n.u1Present  = 1;
    PdeSrc.n.u1Write    = 1;
    PdeSrc.n.u1Accessed = 1;
    PdeSrc.n.u1User     = 1;
#  endif

    pgmLock(pVM);
    {   /* Force the shadow pointers to go out of scope after releasing the lock. */
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
        const unsigned  iPDDst = pvFault >> SHW_PD_SHIFT;
        PX86PD          pPDDst = pgmShwGet32BitPDPtr(&pVCpu->pgm.s);

#  elif PGM_SHW_TYPE == PGM_TYPE_PAE
        const unsigned  iPDDst = (pvFault >> SHW_PD_SHIFT) & SHW_PD_MASK;   /* pPDDst index, not used with the pool. */

        PX86PDPAE       pPDDst;
#    if PGM_GST_TYPE != PGM_TYPE_PAE
        X86PDPE         PdpeSrc;

        /* Fake PDPT entry; access control handled on the page table level, so allow everything. */
        PdpeSrc.u  = X86_PDPE_P;   /* rw/us are reserved for PAE pdpte's; accessed bit causes invalid VT-x guest state errors */
#    endif
        rc = pgmShwSyncPaePDPtr(pVCpu, pvFault, &PdpeSrc, &pPDDst);
        if (rc != VINF_SUCCESS)
        {
            pgmUnlock(pVM);
            AssertRC(rc);
            return rc;
        }
        Assert(pPDDst);

#  elif PGM_SHW_TYPE == PGM_TYPE_AMD64
        const unsigned  iPDDst = ((pvFault >> SHW_PD_SHIFT) & SHW_PD_MASK);
        PX86PDPAE       pPDDst;
#   if PGM_GST_TYPE == PGM_TYPE_PROT
        /* AMD-V nested paging */
        X86PML4E        Pml4eSrc;
        X86PDPE         PdpeSrc;
        PX86PML4E       pPml4eSrc = &Pml4eSrc;

        /* Fake PML4 & PDPT entry; access control handled on the page table level, so allow everything. */
        Pml4eSrc.u = X86_PML4E_P | X86_PML4E_RW | X86_PML4E_US | X86_PML4E_A;
        PdpeSrc.u  = X86_PDPE_P | X86_PDPE_RW | X86_PDPE_US | X86_PDPE_A;
#   endif

        rc = pgmShwSyncLongModePDPtr(pVCpu, pvFault, pPml4eSrc, &PdpeSrc, &pPDDst);
        if (rc != VINF_SUCCESS)
        {
            pgmUnlock(pVM);
            AssertRC(rc);
            return rc;
        }   
        Assert(pPDDst);

#  elif PGM_SHW_TYPE == PGM_TYPE_EPT
        const unsigned  iPDDst = ((pvFault >> SHW_PD_SHIFT) & SHW_PD_MASK);
        PEPTPD          pPDDst;

        rc = pgmShwGetEPTPDPtr(pVCpu, pvFault, NULL, &pPDDst);
        if (rc != VINF_SUCCESS)
        {
            pgmUnlock(pVM);
            AssertRC(rc);
            return rc;
        }
        Assert(pPDDst);
#  endif

#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
        /*
         * If we successfully correct the write protection fault due to dirty bit
         * tracking, or this page fault is a genuine one, then return immediately.
         */
        STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0eTimeCheckPageFault, e);
        rc = PGM_BTH_NAME(CheckPageFault)(pVCpu, uErr, &pPDDst->a[iPDDst], &pPDSrc->a[iPDSrc], pvFault);
        STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeCheckPageFault, e);
        if (    rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT
            ||  rc == VINF_EM_RAW_GUEST_TRAP)
        {
            STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution)
                        = rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT ? &pVCpu->pgm.s.StatRZTrap0eTime2DirtyAndAccessed : &pVCpu->pgm.s.StatRZTrap0eTime2GuestTrap; });
            LogBird(("Trap0eHandler: returns %s\n", rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT ? "VINF_SUCCESS" : "VINF_EM_RAW_GUEST_TRAP"));
            pgmUnlock(pVM);
            return rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT ? VINF_SUCCESS : rc;
        }

        STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0ePD[iPDSrc]);
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */

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
        if (    !(uErr & X86_TRAP_PF_P) /* not set means page not present instead of page protection violation */
            &&  !pPDDst->a[iPDDst].n.u1Present
            &&  PdeSrc.n.u1Present
        )
        {
            STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2SyncPT; });
            STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0eTimeSyncPT, f);
            LogFlow(("=>SyncPT %04x = %08x\n", iPDSrc, PdeSrc.au32[0]));
            rc = PGM_BTH_NAME(SyncPT)(pVCpu, iPDSrc, pPDSrc, pvFault);
            pgmUnlock(pVM);
            if (RT_SUCCESS(rc))
            {
                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeSyncPT, f);
                return rc;
            }
            Log(("SyncPT: %d failed!! rc=%d\n", iPDSrc, rc));
            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3); /** @todo no need to do global sync, right? */
            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeSyncPT, f);
            return VINF_PGM_SYNC_CR3;
        }
        pgmUnlock(pVM);
    }

#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
    /*
     * Check if this address is within any of our mappings.
     *
     * This is *very* fast and it's gonna save us a bit of effort below and prevent
     * us from screwing ourself with MMIO2 pages which have a GC Mapping (VRam).
     * (BTW, it's impossible to have physical access handlers in a mapping.)
     */
    if (pgmMapAreMappingsEnabled(&pVM->pgm.s))
    {
        STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0eTimeMapping, a);
        PPGMMAPPING pMapping = pVM->pgm.s.CTX_SUFF(pMappings);
        for ( ; pMapping; pMapping = pMapping->CTX_SUFF(pNext))
        {
            if (pvFault < pMapping->GCPtr)
                break;
            if (pvFault - pMapping->GCPtr < pMapping->cb)
            {
                /*
                 * The first thing we check is if we've got an undetected conflict.
                 */
                if (!pVM->pgm.s.fMappingsFixed)
                {
                    unsigned iPT = pMapping->cb >> GST_PD_SHIFT;
                    while (iPT-- > 0)
                        if (pPDSrc->a[iPDSrc + iPT].n.u1Present)
                        {
                            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eConflicts);
                            Log(("Trap0e: Detected Conflict %RGv-%RGv\n", pMapping->GCPtr, pMapping->GCPtrLast));
                            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3); /** @todo no need to do global sync,right? */
                            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeMapping, a);
                            return VINF_PGM_SYNC_CR3;
                        }
                }

                /*
                 * Check if the fault address is in a virtual page access handler range.
                 */
                PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->HyperVirtHandlers, pvFault);
                if (    pCur
                    &&  pvFault - pCur->Core.Key < pCur->cb
                    &&  uErr & X86_TRAP_PF_RW)
                {
#   ifdef IN_RC
                    STAM_PROFILE_START(&pCur->Stat, h);
                    rc = pCur->CTX_SUFF(pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->Core.Key, pvFault - pCur->Core.Key);
                    STAM_PROFILE_STOP(&pCur->Stat, h);
#   else
                    AssertFailed();
                    rc = VINF_EM_RAW_EMULATE_INSTR; /* can't happen with VMX */
#   endif
                    STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersMapping);
                    STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeMapping, a);
                    return rc;
                }

                /*
                 * Pretend we're not here and let the guest handle the trap.
                 */
                TRPMSetErrorCode(pVCpu, uErr & ~X86_TRAP_PF_P);
                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eGuestPFMapping);
                LogFlow(("PGM: Mapping access -> route trap to recompiler!\n"));
                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeMapping, a);
                return VINF_EM_RAW_GUEST_TRAP;
            }
        }
        STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeMapping, a);
    } /* pgmAreMappingsEnabled(&pVM->pgm.s) */
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */

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
        RTGCPHYS    GCPhys = NIL_RTGCPHYS;

#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
#   if PGM_GST_TYPE == PGM_TYPE_AMD64
        bool fBigPagesSupported = true;
#   else
        bool fBigPagesSupported = !!(CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);
#   endif
        if (    PdeSrc.b.u1Size
            &&  fBigPagesSupported)
            GCPhys = GST_GET_PDE_BIG_PG_GCPHYS(PdeSrc)
                    | ((RTGCPHYS)pvFault & (GST_BIG_PAGE_OFFSET_MASK ^ PAGE_OFFSET_MASK));
        else
        {
            PGSTPT pPTSrc;
            rc = PGM_GCPHYS_2_PTR(pVM, PdeSrc.u & GST_PDE_PG_MASK, &pPTSrc);
            if (RT_SUCCESS(rc))
            {
                unsigned iPTESrc = (pvFault >> GST_PT_SHIFT) & GST_PT_MASK;
                if (pPTSrc->a[iPTESrc].n.u1Present)
                    GCPhys = pPTSrc->a[iPTESrc].u & GST_PTE_PG_MASK;
            }
        }
#  else
        /* No paging so the fault address is the physical address */
        GCPhys = (RTGCPHYS)(pvFault & ~PAGE_OFFSET_MASK);
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */

        /*
         * If we have a GC address we'll check if it has any flags set.
         */
        if (GCPhys != NIL_RTGCPHYS)
        {
            STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);

            PPGMPAGE pPage;
            rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
            if (RT_SUCCESS(rc)) /** just handle the failure immediate (it returns) and make things easier to read. */
            {
                if (   PGM_PAGE_HAS_ACTIVE_PHYSICAL_HANDLERS(pPage)
                    || PGM_PAGE_HAS_ACTIVE_VIRTUAL_HANDLERS(pPage))
                {
                    if (PGM_PAGE_HAS_ANY_PHYSICAL_HANDLERS(pPage))
                    {
                        /*
                         * Physical page access handler.
                         */
                        const RTGCPHYS  GCPhysFault = GCPhys | (pvFault & PAGE_OFFSET_MASK);
                        PPGMPHYSHANDLER pCur = (PPGMPHYSHANDLER)RTAvlroGCPhysRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->PhysHandlers, GCPhysFault);
                        if (pCur)
                        {
#  ifdef PGM_SYNC_N_PAGES
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
                                rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, pvFault, PGM_SYNC_NR_PAGES, uErr);
                                if (    RT_FAILURE(rc)
                                    || !(uErr & X86_TRAP_PF_RW)
                                    || rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE)
                                {
                                    AssertRC(rc);
                                    STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersOutOfSync);
                                    STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                                    STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2OutOfSyncHndPhys; });
                                    return rc;
                                }
                            }
#  endif

                            AssertMsg(   pCur->enmType != PGMPHYSHANDLERTYPE_PHYSICAL_WRITE
                                      || (pCur->enmType == PGMPHYSHANDLERTYPE_PHYSICAL_WRITE && (uErr & X86_TRAP_PF_RW)),
                                      ("Unexpected trap for physical handler: %08X (phys=%08x) pPage=%R[pgmpage] uErr=%X, enum=%d\n", pvFault, GCPhys, pPage, uErr, pCur->enmType));

# if defined(IN_RC) || defined(IN_RING0)
                            if (pCur->CTX_SUFF(pfnHandler))
                            {
                                STAM_PROFILE_START(&pCur->Stat, h);
                                rc = pCur->CTX_SUFF(pfnHandler)(pVM, uErr, pRegFrame, pvFault, GCPhysFault, pCur->CTX_SUFF(pvUser));
                                STAM_PROFILE_STOP(&pCur->Stat, h);
                            }
                            else
# endif
                                rc = VINF_EM_RAW_EMULATE_INSTR;
                            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersPhysical);
                            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                            STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2HndPhys; });
                            return rc;
                        }
                    }
#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
                    else
                    {
#  ifdef PGM_SYNC_N_PAGES
                        /*
                         * If the region is write protected and we got a page not present fault, then sync
                         * the pages. If the fault was caused by a read, then restart the instruction.
                         * In case of write access continue to the GC write handler.
                         */
                        if (    PGM_PAGE_GET_HNDL_VIRT_STATE(pPage) < PGM_PAGE_HNDL_PHYS_STATE_ALL
                            && !(uErr & X86_TRAP_PF_P))
                        {
                            rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, pvFault, PGM_SYNC_NR_PAGES, uErr);
                            if (    RT_FAILURE(rc)
                                ||  rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE
                                ||  !(uErr & X86_TRAP_PF_RW))
                            {
                                AssertRC(rc);
                                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersOutOfSync);
                                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                                STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2OutOfSyncHndVirt; });
                                return rc;
                            }
                        }
#  endif
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
                        PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, pvFault);
                        if (pCur)
                        {
                            AssertMsg(!(pvFault - pCur->Core.Key < pCur->cb)
                                      || (     pCur->enmType != PGMVIRTHANDLERTYPE_WRITE
                                           || !(uErr & X86_TRAP_PF_P)
                                           || (pCur->enmType == PGMVIRTHANDLERTYPE_WRITE && (uErr & X86_TRAP_PF_RW))),
                                      ("Unexpected trap for virtual handler: %RGv (phys=%RGp) pPage=%R[pgmpage] uErr=%X, enum=%d\n", pvFault, GCPhys, pPage, uErr, pCur->enmType));

                            if (    pvFault - pCur->Core.Key < pCur->cb
                                &&  (    uErr & X86_TRAP_PF_RW
                                     ||  pCur->enmType != PGMVIRTHANDLERTYPE_WRITE ) )
                            {
#   ifdef IN_RC
                                STAM_PROFILE_START(&pCur->Stat, h);
                                rc = pCur->CTX_SUFF(pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->Core.Key, pvFault - pCur->Core.Key);
                                STAM_PROFILE_STOP(&pCur->Stat, h);
#   else
                                rc = VINF_EM_RAW_EMULATE_INSTR; /** @todo for VMX */
#   endif
                                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersVirtual);
                                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                                STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2HndVirt; });
                                return rc;
                            }
                            /* Unhandled part of a monitored page */
                        }
                        else
                        {
                           /* Check by physical address. */
                            PPGMVIRTHANDLER pCur;
                            unsigned        iPage;
                            rc = pgmHandlerVirtualFindByPhysAddr(pVM, GCPhys + (pvFault & PAGE_OFFSET_MASK),
                                                                 &pCur, &iPage);
                            Assert(RT_SUCCESS(rc) || !pCur);
                            if (    pCur
                                &&  (   uErr & X86_TRAP_PF_RW
                                     || pCur->enmType != PGMVIRTHANDLERTYPE_WRITE ) )
                            {
                                Assert((pCur->aPhysToVirt[iPage].Core.Key & X86_PTE_PAE_PG_MASK) == GCPhys);
#   ifdef IN_RC
                                RTGCPTR off = (iPage << PAGE_SHIFT) + (pvFault & PAGE_OFFSET_MASK) - (pCur->Core.Key & PAGE_OFFSET_MASK);
                                Assert(off < pCur->cb);
                                STAM_PROFILE_START(&pCur->Stat, h);
                                rc = pCur->CTX_SUFF(pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->Core.Key, off);
                                STAM_PROFILE_STOP(&pCur->Stat, h);
#   else
                                rc = VINF_EM_RAW_EMULATE_INSTR; /** @todo for VMX */
#   endif
                                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersVirtualByPhys);
                                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                                STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2HndVirt; });
                                return rc;
                            }
                        }
                    }
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */

                    /*
                     * There is a handled area of the page, but this fault doesn't belong to it.
                     * We must emulate the instruction.
                     *
                     * To avoid crashing (non-fatal) in the interpreter and go back to the recompiler
                     * we first check if this was a page-not-present fault for a page with only
                     * write access handlers. Restart the instruction if it wasn't a write access.
                     */
                    STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersUnhandled);

                    if (    !PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage)
                        &&  !(uErr & X86_TRAP_PF_P))
                    {
                        rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, pvFault, PGM_SYNC_NR_PAGES, uErr);
                        if (    RT_FAILURE(rc)
                            ||  rc == VINF_PGM_SYNCPAGE_MODIFIED_PDE
                            ||  !(uErr & X86_TRAP_PF_RW))
                        {
                            AssertRC(rc);
                            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersOutOfSync);
                            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                            STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2OutOfSyncHndPhys; });
                            return rc;
                        }
                    }

                    /** @todo This particular case can cause quite a lot of overhead. E.g. early stage of kernel booting in Ubuntu 6.06
                     *        It's writing to an unhandled part of the LDT page several million times.
                     */
                    rc = PGMInterpretInstruction(pVM, pVCpu, pRegFrame, pvFault);
                    LogFlow(("PGM: PGMInterpretInstruction -> rc=%d pPage=%R[pgmpage]\n", rc, pPage));
                    STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                    STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2HndUnhandled; });
                    return rc;
                } /* if any kind of handler */

#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
                if (uErr & X86_TRAP_PF_P)
                {
                    /*
                     * The page isn't marked, but it might still be monitored by a virtual page access handler.
                     * (ASSUMES no temporary disabling of virtual handlers.)
                     */
                    /** @todo r=bird: Since the purpose is to catch out of sync pages with virtual handler(s) here,
                     * we should correct both the shadow page table and physical memory flags, and not only check for
                     * accesses within the handler region but for access to pages with virtual handlers. */
                    PPGMVIRTHANDLER pCur = (PPGMVIRTHANDLER)RTAvlroGCPtrRangeGet(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, pvFault);
                    if (pCur)
                    {
                        AssertMsg(   !(pvFault - pCur->Core.Key < pCur->cb)
                                  || (    pCur->enmType != PGMVIRTHANDLERTYPE_WRITE
                                       || !(uErr & X86_TRAP_PF_P)
                                       || (pCur->enmType == PGMVIRTHANDLERTYPE_WRITE && (uErr & X86_TRAP_PF_RW))),
                                  ("Unexpected trap for virtual handler: %08X (phys=%08x) %R[pgmpage] uErr=%X, enum=%d\n", pvFault, GCPhys, pPage, uErr, pCur->enmType));

                        if (    pvFault - pCur->Core.Key < pCur->cb
                            &&  (    uErr & X86_TRAP_PF_RW
                                 ||  pCur->enmType != PGMVIRTHANDLERTYPE_WRITE ) )
                        {
#   ifdef IN_RC
                            STAM_PROFILE_START(&pCur->Stat, h);
                            rc = pCur->CTX_SUFF(pfnHandler)(pVM, uErr, pRegFrame, pvFault, pCur->Core.Key, pvFault - pCur->Core.Key);
                            STAM_PROFILE_STOP(&pCur->Stat, h);
#   else
                            rc = VINF_EM_RAW_EMULATE_INSTR; /** @todo for VMX */
#   endif
                            STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersVirtualUnmarked);
                            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                            STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2HndVirt; });
                            return rc;
                        }
                    }
                }
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */
            }
            else
            {
                /*
                 * When the guest accesses invalid physical memory (e.g. probing
                 * of RAM or accessing a remapped MMIO range), then we'll fall
                 * back to the recompiler to emulate the instruction.
                 */
                LogFlow(("PGM #PF: pgmPhysGetPageEx(%RGp) failed with %Rrc\n", GCPhys, rc));
                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eHandlersInvalid);
                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);
                return VINF_EM_RAW_EMULATE_INSTR;
            }

            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeHandlers, b);

#  ifdef PGM_OUT_OF_SYNC_IN_GC /** @todo remove this bugger. */
            /*
             * We are here only if page is present in Guest page tables and
             * trap is not handled by our handlers.
             *
             * Check it for page out-of-sync situation.
             */
            STAM_PROFILE_START(&pVCpu->pgm.s.StatRZTrap0eTimeOutOfSync, c);

            if (!(uErr & X86_TRAP_PF_P))
            {
                /*
                 * Page is not present in our page tables.
                 * Try to sync it!
                 * BTW, fPageShw is invalid in this branch!
                 */
                if (uErr & X86_TRAP_PF_US)
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageOutOfSyncUser));
                else /* supervisor */
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageOutOfSyncSupervisor));

#   if defined(LOG_ENABLED) && !defined(IN_RING0)
                RTGCPHYS   GCPhys;
                uint64_t   fPageGst;
                PGMGstGetPage(pVCpu, pvFault, &fPageGst, &GCPhys);
                Log(("Page out of sync: %RGv eip=%08x PdeSrc.n.u1User=%d fPageGst=%08llx GCPhys=%RGp scan=%d\n",
                     pvFault, pRegFrame->eip, PdeSrc.n.u1User, fPageGst, GCPhys, CSAMDoesPageNeedScanning(pVM, (RTRCPTR)pRegFrame->eip)));
#   endif /* LOG_ENABLED */

#   if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) && !defined(IN_RING0)
                if (CPUMGetGuestCPL(pVCpu, pRegFrame) == 0)
                {
                    uint64_t fPageGst;
                    rc = PGMGstGetPage(pVCpu, pvFault, &fPageGst, NULL);
                    if (    RT_SUCCESS(rc)
                        && !(fPageGst & X86_PTE_US))
                    {
                        /* Note: can't check for X86_TRAP_ID bit, because that requires execute disable support on the CPU */
                        if (    pvFault == (RTGCPTR)pRegFrame->eip
                            ||  pvFault - pRegFrame->eip < 8    /* instruction crossing a page boundary */
#    ifdef CSAM_DETECT_NEW_CODE_PAGES
                            ||  (   !PATMIsPatchGCAddr(pVM, (RTGCPTR)pRegFrame->eip)
                                 && CSAMDoesPageNeedScanning(pVM, (RTRCPTR)pRegFrame->eip))   /* any new code we encounter here */
#    endif /* CSAM_DETECT_NEW_CODE_PAGES */
                           )
                        {
                            LogFlow(("CSAMExecFault %RX32\n", pRegFrame->eip));
                            rc = CSAMExecFault(pVM, (RTRCPTR)pRegFrame->eip);
                            if (rc != VINF_SUCCESS)
                            {
                                /*
                                 * CSAM needs to perform a job in ring 3.
                                 *
                                 * Sync the page before going to the host context; otherwise we'll end up in a loop if
                                 * CSAM fails (e.g. instruction crosses a page boundary and the next page is not present)
                                 */
                                LogFlow(("CSAM ring 3 job\n"));
                                int rc2 = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, pvFault, 1, uErr);
                                AssertRC(rc2);

                                STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeOutOfSync, c);
                                STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2CSAM; });
                                return rc;
                            }
                        }
#    ifdef CSAM_DETECT_NEW_CODE_PAGES
                        else if (    uErr == X86_TRAP_PF_RW
                                 &&  pRegFrame->ecx >= 0x100         /* early check for movswd count */
                                 &&  pRegFrame->ecx < 0x10000)
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
                                rc = EMInterpretDisasOneEx(pVM, pVCpu, PC, pRegFrame, &Cpu, &cbOp);

                                /* For now we'll restrict this to rep movsw/d instructions */
                                if (    rc == VINF_SUCCESS
                                    &&  Cpu.pCurInstr->opcode == OP_MOVSWD
                                    &&  (Cpu.prefix & PREFIX_REP))
                                {
                                    CSAMMarkPossibleCodePage(pVM, pvFault);
                                }
                            }
                        }
#    endif  /* CSAM_DETECT_NEW_CODE_PAGES */

                        /*
                         * Mark this page as safe.
                         */
                        /** @todo not correct for pages that contain both code and data!! */
                        Log2(("CSAMMarkPage %RGv; scanned=%d\n", pvFault, true));
                        CSAMMarkPage(pVM, (RTRCPTR)pvFault, true);
                    }
                }
#   endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) && !defined(IN_RING0) */
                rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, pvFault, PGM_SYNC_NR_PAGES, uErr);
                if (RT_SUCCESS(rc))
                {
                    /* The page was successfully synced, return to the guest. */
                    STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeOutOfSync, c);
                    STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2OutOfSync; });
                    return VINF_SUCCESS;
                }
            }
            else /* uErr & X86_TRAP_PF_P: */
            {
                /*
                 * Write protected pages are make writable when the guest makes the first
                 * write to it. This happens for pages that are shared, write monitored
                 * and not yet allocated.
                 *
                 * Also, a side effect of not flushing global PDEs are out of sync pages due
                 * to physical monitored regions, that are no longer valid.
                 * Assume for now it only applies to the read/write flag.
                 */
                if (RT_SUCCESS(rc) && (uErr & X86_TRAP_PF_RW))
                {
                    if (PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED)
                    {
                        Log(("PGM #PF: Make writable: %RGp %R[pgmpage] pvFault=%RGp uErr=%#x\n",
                             GCPhys, pPage, pvFault, uErr));
                        rc = pgmPhysPageMakeWritableUnlocked(pVM, pPage, GCPhys);
                        if (rc != VINF_SUCCESS)
                        {
                            AssertMsg(rc == VINF_PGM_SYNC_CR3 || RT_FAILURE(rc), ("%Rrc\n", rc));
                            return rc;
                        }
                        if (RT_UNLIKELY(VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY)))
                            return VINF_EM_NO_MEMORY;
                    }
                    /// @todo count the above case; else
                    if (uErr & X86_TRAP_PF_US)
                        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageOutOfSyncUser));
                    else /* supervisor */
                        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageOutOfSyncSupervisor));

                    /*
                     * Note: Do NOT use PGM_SYNC_NR_PAGES here. That only works if the
                     *       page is not present, which is not true in this case.
                     */
                    rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, pvFault, 1, uErr);
                    if (RT_SUCCESS(rc))
                    {
                       /*
                        * Page was successfully synced, return to guest.
                        */
#   ifdef VBOX_STRICT
                        RTGCPHYS GCPhys;
                        uint64_t fPageGst;
                        rc = PGMGstGetPage(pVCpu, pvFault, &fPageGst, &GCPhys);
                        Assert(RT_SUCCESS(rc) && fPageGst & X86_PTE_RW);
                        LogFlow(("Obsolete physical monitor page out of sync %RGv - phys %RGp flags=%08llx\n", pvFault, GCPhys, (uint64_t)fPageGst));

                        uint64_t fPageShw;
                        rc = PGMShwGetPage(pVCpu, pvFault, &fPageShw, NULL);
                        AssertMsg(RT_SUCCESS(rc) && fPageShw & X86_PTE_RW, ("rc=%Rrc fPageShw=%RX64\n", rc, fPageShw));
#   endif /* VBOX_STRICT */
                        STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeOutOfSync, c);
                        STAM_STATS({ pVCpu->pgm.s.CTX_SUFF(pStatTrap0eAttribution) = &pVCpu->pgm.s.StatRZTrap0eTime2OutOfSyncHndObs; });
                        return VINF_SUCCESS;
                    }

                    /* Check to see if we need to emulate the instruction as X86_CR0_WP has been cleared. */
                    if (    CPUMGetGuestCPL(pVCpu, pRegFrame) == 0
                        &&  ((CPUMGetGuestCR0(pVCpu) & (X86_CR0_WP | X86_CR0_PG)) == X86_CR0_PG)
                        &&  (uErr & (X86_TRAP_PF_RW | X86_TRAP_PF_P)) == (X86_TRAP_PF_RW | X86_TRAP_PF_P))
                    {
                        uint64_t fPageGst;
                        rc = PGMGstGetPage(pVCpu, pvFault, &fPageGst, NULL);
                        if (    RT_SUCCESS(rc)
                            && !(fPageGst & X86_PTE_RW))
                        {
                            rc = PGMInterpretInstruction(pVM, pVCpu, pRegFrame, pvFault);
                            if (RT_SUCCESS(rc))
                                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eWPEmulInRZ);
                            else
                                STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eWPEmulToR3);
                            return rc;
                        }
                        AssertMsgFailed(("Unexpected r/w page %RGv flag=%x rc=%Rrc\n", pvFault, (uint32_t)fPageGst, rc));
                    }
                }

#   if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
#    ifdef VBOX_STRICT
                /*
                 * Check for VMM page flags vs. Guest page flags consistency.
                 * Currently only for debug purposes.
                 */
                if (RT_SUCCESS(rc))
                {
                    /* Get guest page flags. */
                    uint64_t fPageGst;
                    rc = PGMGstGetPage(pVCpu, pvFault, &fPageGst, NULL);
                    if (RT_SUCCESS(rc))
                    {
                        uint64_t fPageShw;
                        rc = PGMShwGetPage(pVCpu, pvFault, &fPageShw, NULL);

                        /*
                         * Compare page flags.
                         * Note: we have AVL, A, D bits desynched.
                         */
                        AssertMsg((fPageShw & ~(X86_PTE_A | X86_PTE_D | X86_PTE_AVL_MASK)) == (fPageGst & ~(X86_PTE_A | X86_PTE_D | X86_PTE_AVL_MASK)),
                                  ("Page flags mismatch! pvFault=%RGv uErr=%x GCPhys=%RGp fPageShw=%RX64 fPageGst=%RX64\n", pvFault, (uint32_t)uErr, GCPhys, fPageShw, fPageGst));
                    }
                    else
                        AssertMsgFailed(("PGMGstGetPage rc=%Rrc\n", rc));
                }
                else
                    AssertMsgFailed(("PGMGCGetPage rc=%Rrc\n", rc));
#    endif /* VBOX_STRICT */
#   endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */
            }
            STAM_PROFILE_STOP(&pVCpu->pgm.s.StatRZTrap0eTimeOutOfSync, c);
#  endif /* PGM_OUT_OF_SYNC_IN_GC */
        }
        else /* GCPhys == NIL_RTGCPHYS */
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
    /* else: !present (guest) */


#  if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
    /*
     * Conclusion, this is a guest trap.
     */
    LogFlow(("PGM: Unhandled #PF -> route trap to recompiler!\n"));
    STAM_COUNTER_INC(&pVCpu->pgm.s.StatRZTrap0eGuestPFUnh);
    return VINF_EM_RAW_GUEST_TRAP;
#  else
    /* present, but not a monitored page; perhaps the guest is probing physical memory */
    return VINF_EM_RAW_EMULATE_INSTR;
#  endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */


# else /* PGM_GST_TYPE != PGM_TYPE_32BIT */

    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    return VERR_INTERNAL_ERROR;
# endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}
#endif /* !IN_RING3 */


/**
 * Emulation of the invlpg instruction.
 *
 *
 * @returns VBox status code.
 *
 * @param   pVCpu       The VMCPU handle.
 * @param   GCPtrPage   Page to invalidate.
 *
 * @remark  ASSUMES that the guest is updating before invalidating. This order
 *          isn't required by the CPU, so this is speculative and could cause
 *          trouble.
 *
 * @todo    Flush page or page directory only if necessary!
 * @todo    Add a #define for simply invalidating the page.
 */
PGM_BTH_DECL(int, InvalidatePage)(PVMCPU pVCpu, RTGCPTR GCPtrPage)
{
#if    PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)   \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED \
    && PGM_SHW_TYPE != PGM_TYPE_EPT
    int rc;
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

    LogFlow(("InvalidatePage %RGv\n", GCPtrPage));
    /*
     * Get the shadow PD entry and skip out if this PD isn't present.
     * (Guessing that it is frequent for a shadow PDE to not be present, do this first.)
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDDst    = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PX86PDE         pPdeDst   = pgmShwGet32BitPDEPtr(&pVCpu->pgm.s, GCPtrPage);

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pVCpu->pgm.s.CTX_SUFF(pShwPageCR3);
    Assert(pShwPde);

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    const unsigned  iPdpt     = (GCPtrPage >> X86_PDPT_SHIFT);
    PX86PDPT        pPdptDst  = pgmShwGetPaePDPTPtr(&pVCpu->pgm.s);

    /* If the shadow PDPE isn't present, then skip the invalidate. */
    if (!pPdptDst->a[iPdpt].n.u1Present)
    {
        Assert(!(pPdptDst->a[iPdpt].u & PGM_PLXFLAGS_MAPPING));
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePageSkipped));
        return VINF_SUCCESS;
    }

    const unsigned  iPDDst  = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PPGMPOOLPAGE    pShwPde = NULL;
    PX86PDPAE       pPDDst;

    /* Fetch the pgm pool shadow descriptor. */
    rc = pgmShwGetPaePoolPagePD(&pVCpu->pgm.s, GCPtrPage, &pShwPde);
    AssertRCSuccessReturn(rc, rc);
    Assert(pShwPde);

    pPDDst             = (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGM(&pVM->pgm.s, pShwPde);
    PX86PDEPAE pPdeDst = &pPDDst->a[iPDDst];

# else /* PGM_SHW_TYPE == PGM_TYPE_AMD64 */
    /* PML4 */
    const unsigned  iPml4     = (GCPtrPage >> X86_PML4_SHIFT) & X86_PML4_MASK;
    const unsigned  iPdpt     = (GCPtrPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
    const unsigned  iPDDst    = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PX86PDPAE       pPDDst;
    PX86PDPT        pPdptDst;
    PX86PML4E       pPml4eDst;
    rc = pgmShwGetLongModePDPtr(pVCpu, GCPtrPage, &pPml4eDst, &pPdptDst, &pPDDst);
    if (rc != VINF_SUCCESS)
    {
        AssertMsg(rc == VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT || rc == VERR_PAGE_MAP_LEVEL4_NOT_PRESENT, ("Unexpected rc=%Rrc\n", rc));
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePageSkipped));
        if (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3))
            PGM_INVL_GUEST_TLBS();
        return VINF_SUCCESS;
    }
    Assert(pPDDst);

    PX86PDEPAE  pPdeDst  = &pPDDst->a[iPDDst];
    PX86PDPE    pPdpeDst = &pPdptDst->a[iPdpt];

    if (!pPdpeDst->n.u1Present)
    {
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePageSkipped));
        if (!VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3))
            PGM_INVL_GUEST_TLBS();
        return VINF_SUCCESS;
    }

# endif /* PGM_SHW_TYPE == PGM_TYPE_AMD64 */

    const SHWPDE PdeDst = *pPdeDst;
    if (!PdeDst.n.u1Present)
    {
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePageSkipped));
        return VINF_SUCCESS;
    }

# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynLockHCPage(pVM, (uint8_t *)pPdeDst);
# endif

    /*
     * Get the guest PD entry and calc big page.
     */
# if PGM_GST_TYPE == PGM_TYPE_32BIT
    PGSTPD          pPDSrc      = pgmGstGet32bitPDPtr(&pVCpu->pgm.s);
    const unsigned  iPDSrc      = GCPtrPage >> GST_PD_SHIFT;
    GSTPDE          PdeSrc      = pPDSrc->a[iPDSrc];
# else /* PGM_GST_TYPE != PGM_TYPE_32BIT */
    unsigned        iPDSrc = 0;
#  if PGM_GST_TYPE == PGM_TYPE_PAE
    X86PDPE         PdpeSrc;
    PX86PDPAE       pPDSrc      = pgmGstGetPaePDPtr(&pVCpu->pgm.s, GCPtrPage, &iPDSrc, &PdpeSrc);
#  else /* AMD64 */
    PX86PML4E       pPml4eSrc;
    X86PDPE         PdpeSrc;
    PX86PDPAE       pPDSrc      = pgmGstGetLongModePDPtr(&pVCpu->pgm.s, GCPtrPage, &pPml4eSrc, &PdpeSrc, &iPDSrc);
#  endif
    GSTPDE          PdeSrc;

    if (pPDSrc)
        PdeSrc = pPDSrc->a[iPDSrc];
    else
        PdeSrc.u = 0;
# endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */

# if PGM_GST_TYPE == PGM_TYPE_AMD64
    const bool      fIsBigPage  = PdeSrc.b.u1Size;
# else
    const bool      fIsBigPage  = PdeSrc.b.u1Size && (CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);
# endif

# ifdef IN_RING3
    /*
     * If a CR3 Sync is pending we may ignore the invalidate page operation
     * depending on the kind of sync and if it's a global page or not.
     * This doesn't make sense in GC/R0 so we'll skip it entirely there.
     */
#  ifdef PGM_SKIP_GLOBAL_PAGEDIRS_ON_NONGLOBAL_FLUSH
    if (    VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3)
        || (   VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL)
            && fIsBigPage
            && PdeSrc.b.u1Global
           )
       )
#  else
    if (VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL) )
#  endif
    {
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePageSkipped));
        return VINF_SUCCESS;
    }
# endif /* IN_RING3 */

# if PGM_GST_TYPE == PGM_TYPE_AMD64
    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE pShwPdpt = pgmPoolGetPage(pPool, pPml4eDst->u & X86_PML4E_PG_MASK);
    Assert(pShwPdpt);

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE pShwPde = pgmPoolGetPage(pPool, pPdptDst->a[iPdpt].u & SHW_PDPE_PG_MASK);
    Assert(pShwPde);

    Assert(pPml4eDst->n.u1Present && (pPml4eDst->u & SHW_PDPT_MASK));
    RTGCPHYS GCPhysPdpt = pPml4eSrc->u & X86_PML4E_PG_MASK;

    if (    !pPml4eSrc->n.u1Present
        ||  pShwPdpt->GCPhys != GCPhysPdpt)
    {
        LogFlow(("InvalidatePage: Out-of-sync PML4E (P/GCPhys) at %RGv GCPhys=%RGp vs %RGp Pml4eSrc=%RX64 Pml4eDst=%RX64\n",
                 GCPtrPage, pShwPdpt->GCPhys, GCPhysPdpt, (uint64_t)pPml4eSrc->u, (uint64_t)pPml4eDst->u));
        pgmPoolFreeByPage(pPool, pShwPdpt, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3)->idx, iPml4);
        ASMAtomicWriteSize(pPml4eDst, 0);
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDNPs));
        PGM_INVL_GUEST_TLBS();
        return VINF_SUCCESS;
    }
    if (   pPml4eSrc->n.u1User != pPml4eDst->n.u1User
        || (!pPml4eSrc->n.u1Write && pPml4eDst->n.u1Write))
    {
        /*
         * Mark not present so we can resync the PML4E when it's used.
         */
        LogFlow(("InvalidatePage: Out-of-sync PML4E at %RGv Pml4eSrc=%RX64 Pml4eDst=%RX64\n",
                 GCPtrPage, (uint64_t)pPml4eSrc->u, (uint64_t)pPml4eDst->u));
        pgmPoolFreeByPage(pPool, pShwPdpt, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3)->idx, iPml4);
        ASMAtomicWriteSize(pPml4eDst, 0);
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDOutOfSync));
        PGM_INVL_GUEST_TLBS();
    }
    else if (!pPml4eSrc->n.u1Accessed)
    {
        /*
         * Mark not present so we can set the accessed bit.
         */
        LogFlow(("InvalidatePage: Out-of-sync PML4E (A) at %RGv Pml4eSrc=%RX64 Pml4eDst=%RX64\n",
                 GCPtrPage, (uint64_t)pPml4eSrc->u, (uint64_t)pPml4eDst->u));
        pgmPoolFreeByPage(pPool, pShwPdpt, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3)->idx, iPml4);
        ASMAtomicWriteSize(pPml4eDst, 0);
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDNAs));
        PGM_INVL_GUEST_TLBS();
    }

    /* Check if the PDPT entry has changed. */
    Assert(pPdpeDst->n.u1Present && pPdpeDst->u & SHW_PDPT_MASK);
    RTGCPHYS GCPhysPd = PdpeSrc.u & GST_PDPE_PG_MASK;
    if (    !PdpeSrc.n.u1Present
        ||  pShwPde->GCPhys != GCPhysPd)
    {
        LogFlow(("InvalidatePage: Out-of-sync PDPE (P/GCPhys) at %RGv GCPhys=%RGp vs %RGp PdpeSrc=%RX64 PdpeDst=%RX64\n",
                    GCPtrPage, pShwPde->GCPhys, GCPhysPd, (uint64_t)PdpeSrc.u, (uint64_t)pPdpeDst->u));
        pgmPoolFreeByPage(pPool, pShwPde, pShwPdpt->idx, iPdpt);
        ASMAtomicWriteSize(pPdpeDst, 0);
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDNPs));
        PGM_INVL_GUEST_TLBS();
        return VINF_SUCCESS;
    }
    if (   PdpeSrc.lm.u1User != pPdpeDst->lm.u1User
        || (!PdpeSrc.lm.u1Write && pPdpeDst->lm.u1Write))
    {
        /*
         * Mark not present so we can resync the PDPTE when it's used.
         */
        LogFlow(("InvalidatePage: Out-of-sync PDPE at %RGv PdpeSrc=%RX64 PdpeDst=%RX64\n",
                 GCPtrPage, (uint64_t)PdpeSrc.u, (uint64_t)pPdpeDst->u));
        pgmPoolFreeByPage(pPool, pShwPde, pShwPdpt->idx, iPdpt);
        ASMAtomicWriteSize(pPdpeDst, 0);
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDOutOfSync));
        PGM_INVL_GUEST_TLBS();
    }
    else if (!PdpeSrc.lm.u1Accessed)
    {
        /*
         * Mark not present so we can set the accessed bit.
         */
        LogFlow(("InvalidatePage: Out-of-sync PDPE (A) at %RGv PdpeSrc=%RX64 PdpeDst=%RX64\n",
                 GCPtrPage, (uint64_t)PdpeSrc.u, (uint64_t)pPdpeDst->u));
        pgmPoolFreeByPage(pPool, pShwPde, pShwPdpt->idx, iPdpt);
        ASMAtomicWriteSize(pPdpeDst, 0);
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDNAs));
        PGM_INVL_GUEST_TLBS();
    }
# endif /* PGM_GST_TYPE == PGM_TYPE_AMD64 */

    /*
     * Deal with the Guest PDE.
     */
    rc = VINF_SUCCESS;
    if (PdeSrc.n.u1Present)
    {
        if (PdeDst.u & PGM_PDFLAGS_MAPPING)
        {
            /*
             * Conflict - Let SyncPT deal with it to avoid duplicate code.
             */
            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
            Assert(PGMGetGuestMode(pVCpu) <= PGMMODE_PAE);
            pgmLock(pVM);
            rc = PGM_BTH_NAME(SyncPT)(pVCpu, iPDSrc, pPDSrc, GCPtrPage);
            pgmUnlock(pVM);
        }
        else if (   PdeSrc.n.u1User != PdeDst.n.u1User
                 || (!PdeSrc.n.u1Write && PdeDst.n.u1Write))
        {
            /*
             * Mark not present so we can resync the PDE when it's used.
             */
            LogFlow(("InvalidatePage: Out-of-sync at %RGp PdeSrc=%RX64 PdeDst=%RX64\n",
                     GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, pShwPde->idx, iPDDst);
            ASMAtomicWriteSize(pPdeDst, 0);
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDOutOfSync));
            PGM_INVL_GUEST_TLBS();
        }
        else if (!PdeSrc.n.u1Accessed)
        {
            /*
             * Mark not present so we can set the accessed bit.
             */
            LogFlow(("InvalidatePage: Out-of-sync (A) at %RGp PdeSrc=%RX64 PdeDst=%RX64\n",
                     GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, pShwPde->idx, iPDDst);
            ASMAtomicWriteSize(pPdeDst, 0);
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDNAs));
            PGM_INVL_GUEST_TLBS();
        }
        else if (!fIsBigPage)
        {
            /*
             * 4KB - page.
             */
            PPGMPOOLPAGE    pShwPage = pgmPoolGetPage(pPool, PdeDst.u & SHW_PDE_PG_MASK);
            RTGCPHYS        GCPhys   = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
            /* Select the right PDE as we're emulating a 4kb page table with 2 shadow page tables. */
            GCPhys |= (iPDDst & 1) * (PAGE_SIZE/2);
# endif
            if (pShwPage->GCPhys == GCPhys)
            {
# if 0 /* likely cause of a major performance regression; must be SyncPageWorkerTrackDeref then */
                const unsigned iPTEDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
                PSHWPT pPT = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
                if (pPT->a[iPTEDst].n.u1Present)
                {
#  ifdef PGMPOOL_WITH_USER_TRACKING
                    /* This is very unlikely with caching/monitoring enabled. */
                    PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pShwPage, pPT->a[iPTEDst].u & SHW_PTE_PG_MASK);
#  endif
                    pPT->a[iPTEDst].u = 0;
                }
# else /* Syncing it here isn't 100% safe and it's probably not worth spending time syncing it. */
                rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, GCPtrPage, 1, 0);
                if (RT_SUCCESS(rc))
                    rc = VINF_SUCCESS;
# endif
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePage4KBPages));
                PGM_INVL_PG(GCPtrPage);
            }
            else
            {
                /*
                 * The page table address changed.
                 */
                LogFlow(("InvalidatePage: Out-of-sync at %RGp PdeSrc=%RX64 PdeDst=%RX64 ShwGCPhys=%RGp iPDDst=%#x\n",
                         GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u, pShwPage->GCPhys, iPDDst));
                pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, pShwPde->idx, iPDDst);
                ASMAtomicWriteSize(pPdeDst, 0);
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDOutOfSync));
                PGM_INVL_GUEST_TLBS();
            }
        }
        else
        {
            /*
             * 2/4MB - page.
             */
            /* Before freeing the page, check if anything really changed. */
            PPGMPOOLPAGE    pShwPage = pgmPoolGetPage(pPool, PdeDst.u & SHW_PDE_PG_MASK);
            RTGCPHYS        GCPhys   = GST_GET_PDE_BIG_PG_GCPHYS(PdeSrc);
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
            /* Select the right PDE as we're emulating a 4MB page directory with two 2 MB shadow PDEs.*/
            GCPhys |= GCPtrPage & (1 << X86_PD_PAE_SHIFT);
# endif
            if (    pShwPage->GCPhys == GCPhys
                &&  pShwPage->enmKind == BTH_PGMPOOLKIND_PT_FOR_BIG)
            {
                /* ASSUMES a the given bits are identical for 4M and normal PDEs */
                /** @todo PAT */
                if (        (PdeSrc.u & (X86_PDE_P | X86_PDE_RW | X86_PDE_US | X86_PDE_PWT | X86_PDE_PCD))
                        ==  (PdeDst.u & (X86_PDE_P | X86_PDE_RW | X86_PDE_US | X86_PDE_PWT | X86_PDE_PCD))
                    &&  (   PdeSrc.b.u1Dirty /** @todo rainy day: What about read-only 4M pages? not very common, but still... */
                         || (PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY)))
                {
                    LogFlow(("Skipping flush for big page containing %RGv (PD=%X .u=%RX64)-> nothing has changed!\n", GCPtrPage, iPDSrc, PdeSrc.u));
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePage4MBPagesSkip));
# if defined(IN_RC)
                    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
                    PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
                    return VINF_SUCCESS;
                }
            }

            /*
             * Ok, the page table is present and it's been changed in the guest.
             * If we're in host context, we'll just mark it as not present taking the lazy approach.
             * We could do this for some flushes in GC too, but we need an algorithm for
             * deciding which 4MB pages containing code likely to be executed very soon.
             */
            LogFlow(("InvalidatePage: Out-of-sync PD at %RGp PdeSrc=%RX64 PdeDst=%RX64\n",
                     GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, pShwPde->idx, iPDDst);
            ASMAtomicWriteSize(pPdeDst, 0);
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePage4MBPages));
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
            pgmPoolFree(pVM, PdeDst.u & SHW_PDE_PG_MASK, pShwPde->idx, iPDDst);
            ASMAtomicWriteSize(pPdeDst, 0);
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDNPs));
            PGM_INVL_PG(GCPtrPage);
        }
        else
        {
            Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,InvalidatePagePDMappings));
        }
    }
# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
    return rc;

#else /* guest real and protected mode */
    /* There's no such thing as InvalidatePage when paging is disabled, so just ignore. */
    return VINF_SUCCESS;
#endif
}


#ifdef PGMPOOL_WITH_USER_TRACKING
/**
 * Update the tracking of shadowed pages.
 *
 * @param   pVCpu       The VMCPU handle.
 * @param   pShwPage    The shadow page.
 * @param   HCPhys      The physical page we is being dereferenced.
 */
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorkerTrackDeref)(PVMCPU pVCpu, PPGMPOOLPAGE pShwPage, RTHCPHYS HCPhys)
{
# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    STAM_PROFILE_START(&pVM->pgm.s.StatTrackDeref, a);
    LogFlow(("SyncPageWorkerTrackDeref: Damn HCPhys=%RHp pShwPage->idx=%#x!!!\n", HCPhys, pShwPage->idx));

    /** @todo If this turns out to be a bottle neck (*very* likely) two things can be done:
     *      1. have a medium sized HCPhys -> GCPhys TLB (hash?)
     *      2. write protect all shadowed pages. I.e. implement caching.
     */
    /*
     * Find the guest address.
     */
    for (PPGMRAMRANGE pRam = pVM->pgm.s.CTX_SUFF(pRamRanges);
         pRam;
         pRam = pRam->CTX_SUFF(pNext))
    {
        unsigned iPage = pRam->cb >> PAGE_SHIFT;
        while (iPage-- > 0)
        {
            if (PGM_PAGE_GET_HCPHYS(&pRam->aPages[iPage]) == HCPhys)
            {
                PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
                pgmTrackDerefGCPhys(pPool, pShwPage, &pRam->aPages[iPage]);
                pShwPage->cPresent--;
                pPool->cPresent--;
                STAM_PROFILE_STOP(&pVM->pgm.s.StatTrackDeref, a);
                return;
            }
        }
    }

    for (;;)
        AssertReleaseMsgFailed(("HCPhys=%RHp wasn't found!\n", HCPhys));
# else  /* !PGMPOOL_WITH_GCPHYS_TRACKING */
    pShwPage->cPresent--;
    pVM->pgm.s.CTX_SUFF(pPool)->cPresent--;
# endif /* !PGMPOOL_WITH_GCPHYS_TRACKING */
}


/**
 * Update the tracking of shadowed pages.
 *
 * @param   pVCpu       The VMCPU handle.
 * @param   pShwPage    The shadow page.
 * @param   u16         The top 16-bit of the pPage->HCPhys.
 * @param   pPage       Pointer to the guest page. this will be modified.
 * @param   iPTDst      The index into the shadow table.
 */
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorkerTrackAddref)(PVMCPU pVCpu, PPGMPOOLPAGE pShwPage, uint16_t u16, PPGMPAGE pPage, const unsigned iPTDst)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);
# ifdef PGMPOOL_WITH_GCPHYS_TRACKING
    /*
     * Just deal with the simple first time here.
     */
    if (!u16)
    {
        STAM_COUNTER_INC(&pVM->pgm.s.StatTrackVirgin);
        u16 = PGMPOOL_TD_MAKE(1, pShwPage->idx);
    }
    else
        u16 = pgmPoolTrackPhysExtAddref(pVM, u16, pShwPage->idx);

    /* write back */
    Log2(("SyncPageWorkerTrackAddRef: u16=%#x->%#x  iPTDst=%#x\n", u16, PGM_PAGE_GET_TRACKING(pPage), iPTDst));
    PGM_PAGE_SET_TRACKING(pPage, u16);

# endif /* PGMPOOL_WITH_GCPHYS_TRACKING */

    /* update statistics. */
    pVM->pgm.s.CTX_SUFF(pPool)->cPresent++;
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
 * @param   pVCpu       The VMCPU handle.
 * @param   pPteDst     Destination page table entry.
 * @param   PdeSrc      Source page directory entry (i.e. Guest OS page directory entry).
 *                      Can safely assume that only the flags are being used.
 * @param   PteSrc      Source page table entry (i.e. Guest OS page table entry).
 * @param   pShwPage    Pointer to the shadow page.
 * @param   iPTDst      The index into the shadow table.
 *
 * @remark  Not used for 2/4MB pages!
 */
DECLINLINE(void) PGM_BTH_NAME(SyncPageWorker)(PVMCPU pVCpu, PSHWPTE pPteDst, GSTPDE PdeSrc, GSTPTE PteSrc, PPGMPOOLPAGE pShwPage, unsigned iPTDst)
{
    if (PteSrc.n.u1Present)
    {
        PVM pVM = pVCpu->CTX_SUFF(pVM);

        /*
         * Find the ram range.
         */
        PPGMPAGE pPage;
        int rc = pgmPhysGetPageEx(&pVM->pgm.s, PteSrc.u & GST_PTE_PG_MASK, &pPage);
        if (RT_SUCCESS(rc))
        {
#ifndef VBOX_WITH_NEW_LAZY_PAGE_ALLOC
            /* Try make the page writable if necessary. */
            if (    PteSrc.n.u1Write
                &&  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED
                &&  PGM_PAGE_GET_TYPE(pPage)  == PGMPAGETYPE_RAM)
            {
                rc = pgmPhysPageMakeWritableUnlocked(pVM, pPage, PteSrc.u & GST_PTE_PG_MASK);
                AssertRC(rc);
            }
#endif

            /** @todo investiage PWT, PCD and PAT. */
            /*
             * Make page table entry.
             */
            SHWPTE PteDst;
            if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
            {
                /** @todo r=bird: Are we actually handling dirty and access bits for pages with access handlers correctly? No. */
                if (!PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage))
                {
#if PGM_SHW_TYPE == PGM_TYPE_EPT
                    PteDst.u             = PGM_PAGE_GET_HCPHYS(pPage);
                    PteDst.n.u1Present   = 1;
                    PteDst.n.u1Execute   = 1;
                    PteDst.n.u1IgnorePAT = 1;
                    PteDst.n.u3EMT       = VMX_EPT_MEMTYPE_WB;
                    /* PteDst.n.u1Write = 0 && PteDst.n.u1Size = 0 */
#else
                    PteDst.u = (PteSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT | X86_PTE_RW))
                             | PGM_PAGE_GET_HCPHYS(pPage);
#endif
                }
                else
                {
                    LogFlow(("SyncPageWorker: monitored page (%RHp) -> mark not present\n", PGM_PAGE_GET_HCPHYS(pPage)));
                    PteDst.u = 0;
                }
                /** @todo count these two kinds. */
            }
            else
            {
#if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
                /*
                 * If the page or page directory entry is not marked accessed,
                 * we mark the page not present.
                 */
                if (!PteSrc.n.u1Accessed || !PdeSrc.n.u1Accessed)
                {
                    LogFlow(("SyncPageWorker: page and or page directory not accessed -> mark not present\n"));
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,AccessedPage));
                    PteDst.u = 0;
                }
                else
                /*
                 * If the page is not flagged as dirty and is writable, then make it read-only, so we can set the dirty bit
                 * when the page is modified.
                 */
                if (!PteSrc.n.u1Dirty && (PdeSrc.n.u1Write & PteSrc.n.u1Write))
                {
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPage));
                    PteDst.u = (PteSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT | X86_PTE_RW))
                             | PGM_PAGE_GET_HCPHYS(pPage)
                             | PGM_PTFLAGS_TRACK_DIRTY;
                }
                else
#endif
                {
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageSkipped));
#if PGM_SHW_TYPE == PGM_TYPE_EPT
                    PteDst.u             = PGM_PAGE_GET_HCPHYS(pPage);
                    PteDst.n.u1Present   = 1;
                    PteDst.n.u1Write     = 1;
                    PteDst.n.u1Execute   = 1;
                    PteDst.n.u1IgnorePAT = 1;
                    PteDst.n.u3EMT       = VMX_EPT_MEMTYPE_WB;
                    /* PteDst.n.u1Size = 0 */
#else
                    PteDst.u = (PteSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT))
                             | PGM_PAGE_GET_HCPHYS(pPage);
#endif
                }
            }

            /*
             * Make sure only allocated pages are mapped writable.
             */
            if (    PteDst.n.u1Write
                &&  PteDst.n.u1Present
                &&  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED)
            {
                PteDst.n.u1Write = 0;   /** @todo this isn't quite working yet. */
                Log3(("SyncPageWorker: write-protecting %RGp pPage=%R[pgmpage]at iPTDst=%d\n", (RTGCPHYS)(PteSrc.u & X86_PTE_PAE_PG_MASK), pPage, iPTDst));
            }

#ifdef PGMPOOL_WITH_USER_TRACKING
            /*
             * Keep user track up to date.
             */
            if (PteDst.n.u1Present)
            {
                if (!pPteDst->n.u1Present)
                    PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVCpu, pShwPage, PGM_PAGE_GET_TRACKING(pPage), pPage, iPTDst);
                else if ((pPteDst->u & SHW_PTE_PG_MASK) != (PteDst.u & SHW_PTE_PG_MASK))
                {
                    Log2(("SyncPageWorker: deref! *pPteDst=%RX64 PteDst=%RX64\n", (uint64_t)pPteDst->u, (uint64_t)PteDst.u));
                    PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVCpu, pShwPage, pPteDst->u & SHW_PTE_PG_MASK);
                    PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVCpu, pShwPage, PGM_PAGE_GET_TRACKING(pPage), pPage, iPTDst);
                }
            }
            else if (pPteDst->n.u1Present)
            {
                Log2(("SyncPageWorker: deref! *pPteDst=%RX64\n", (uint64_t)pPteDst->u));
                PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVCpu, pShwPage, pPteDst->u & SHW_PTE_PG_MASK);
            }
#endif /* PGMPOOL_WITH_USER_TRACKING */

            /*
             * Update statistics and commit the entry.
             */
#if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
            if (!PteSrc.n.u1Global)
                pShwPage->fSeenNonGlobal = true;
#endif
            ASMAtomicWriteSize(pPteDst, PteDst.u);
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
            PGM_BTH_NAME(SyncPageWorkerTrackDeref)(pVCpu, pShwPage, pPteDst->u & SHW_PTE_PG_MASK);
        }
#endif /* PGMPOOL_WITH_USER_TRACKING */
        ASMAtomicWriteSize(pPteDst, 0);
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
 * @param   pVCpu       The VMCPU handle.
 * @param   PdeSrc      Page directory entry of the guest.
 * @param   GCPtrPage   Guest context page address.
 * @param   cPages      Number of pages to sync (PGM_SYNC_N_PAGES) (default=1).
 * @param   uErr        Fault error (X86_TRAP_PF_*).
 */
PGM_BTH_DECL(int, SyncPage)(PVMCPU pVCpu, GSTPDE PdeSrc, RTGCPTR GCPtrPage, unsigned cPages, unsigned uErr)
{
    PVM      pVM = pVCpu->CTX_SUFF(pVM);
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);
    LogFlow(("SyncPage: GCPtrPage=%RGv cPages=%u uErr=%#x\n", GCPtrPage, cPages, uErr));

#if    (   PGM_GST_TYPE == PGM_TYPE_32BIT  \
        || PGM_GST_TYPE == PGM_TYPE_PAE    \
        || PGM_GST_TYPE == PGM_TYPE_AMD64) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED     \
    && PGM_SHW_TYPE != PGM_TYPE_EPT

# if PGM_WITH_NX(PGM_GST_TYPE, PGM_SHW_TYPE)
    bool fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVCpu) & MSR_K6_EFER_NXE);
# endif

    /*
     * Assert preconditions.
     */
    Assert(PdeSrc.n.u1Present);
    Assert(cPages);
    STAM_COUNTER_INC(&pVCpu->pgm.s.StatSyncPagePD[(GCPtrPage >> GST_PD_SHIFT) & GST_PD_MASK]);

    /*
     * Get the shadow PDE, find the shadow page table in the pool.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDDst   = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PX86PDE         pPdeDst  = pgmShwGet32BitPDEPtr(&pVCpu->pgm.s, GCPtrPage);

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pVCpu->pgm.s.CTX_SUFF(pShwPageCR3);
    Assert(pShwPde);

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    const unsigned  iPDDst  = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PPGMPOOLPAGE    pShwPde = NULL;
    PX86PDPAE       pPDDst;

    /* Fetch the pgm pool shadow descriptor. */
    int rc = pgmShwGetPaePoolPagePD(&pVCpu->pgm.s, GCPtrPage, &pShwPde);
    AssertRCSuccessReturn(rc, rc);
    Assert(pShwPde);

    pPDDst             = (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGM(&pVM->pgm.s, pShwPde);
    PX86PDEPAE pPdeDst = &pPDDst->a[iPDDst];

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    const unsigned  iPDDst   = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    const unsigned  iPdpt    = (GCPtrPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
    PX86PDPAE       pPDDst;
    PX86PDPT        pPdptDst;

    int rc = pgmShwGetLongModePDPtr(pVCpu, GCPtrPage, NULL, &pPdptDst, &pPDDst);
    AssertRCSuccessReturn(rc, rc);
    Assert(pPDDst && pPdptDst);
    PX86PDEPAE      pPdeDst = &pPDDst->a[iPDDst];
# endif
    SHWPDE          PdeDst   = *pPdeDst;
    AssertMsg(PdeDst.n.u1Present, ("%p=%llx\n", pPdeDst, (uint64_t)PdeDst.u));
    PPGMPOOLPAGE    pShwPage = pgmPoolGetPage(pPool, PdeDst.u & SHW_PDE_PG_MASK);

# if PGM_GST_TYPE == PGM_TYPE_AMD64
    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde  = pgmPoolGetPage(pPool, pPdptDst->a[iPdpt].u & X86_PDPE_PG_MASK);
    Assert(pShwPde);
# endif

# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynLockHCPage(pVM, (uint8_t *)pPdeDst);
# endif

    /*
     * Check that the page is present and that the shadow PDE isn't out of sync.
     */
# if PGM_GST_TYPE == PGM_TYPE_AMD64
    const bool      fBigPage = PdeSrc.b.u1Size;
# else
    const bool      fBigPage = PdeSrc.b.u1Size && (CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);
# endif
    RTGCPHYS        GCPhys;
    if (!fBigPage)
    {
        GCPhys = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
        /* Select the right PDE as we're emulating a 4kb page table with 2 shadow page tables. */
        GCPhys |= (iPDDst & 1) * (PAGE_SIZE/2);
# endif
    }
    else
    {
        GCPhys = GST_GET_PDE_BIG_PG_GCPHYS(PdeSrc);
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
        /* Select the right PDE as we're emulating a 4MB page directory with two 2 MB shadow PDEs.*/
        GCPhys |= GCPtrPage & (1 << X86_PD_PAE_SHIFT);
# endif
    }
    if (    pShwPage->GCPhys == GCPhys
        &&  PdeSrc.n.u1Present
        &&  (PdeSrc.n.u1User == PdeDst.n.u1User)
        &&  (PdeSrc.n.u1Write == PdeDst.n.u1Write || !PdeDst.n.u1Write)
# if PGM_WITH_NX(PGM_GST_TYPE, PGM_SHW_TYPE)
        &&  (!fNoExecuteBitValid || PdeSrc.n.u1NoExecute == PdeDst.n.u1NoExecute)
# endif
       )
    {
        /*
         * Check that the PDE is marked accessed already.
         * Since we set the accessed bit *before* getting here on a #PF, this
         * check is only meant for dealing with non-#PF'ing paths.
         */
        if (PdeSrc.n.u1Accessed)
        {
            PSHWPT pPTDst = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
            if (!fBigPage)
            {
                /*
                 * 4KB Page - Map the guest page table.
                 */
                PGSTPT pPTSrc;
                int rc = PGM_GCPHYS_2_PTR(pVM, PdeSrc.u & GST_PDE_PG_MASK, &pPTSrc);
                if (RT_SUCCESS(rc))
                {
# ifdef PGM_SYNC_N_PAGES
                    Assert(cPages == 1 || !(uErr & X86_TRAP_PF_P));
                    if (    cPages > 1
                        &&  !(uErr & X86_TRAP_PF_P)
                        &&  !VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY))
                    {
                        /*
                         * This code path is currently only taken when the caller is PGMTrap0eHandler
                         * for non-present pages!
                         *
                         * We're setting PGM_SYNC_NR_PAGES pages around the faulting page to sync it and
                         * deal with locality.
                         */
                        unsigned        iPTDst    = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
#  if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                        /* Select the right PDE as we're emulating a 4kb page table with 2 shadow page tables. */
                        const unsigned  offPTSrc  = ((GCPtrPage >> SHW_PD_SHIFT) & 1) * 512;
#  else
                        const unsigned  offPTSrc  = 0;
#  endif
                        const unsigned  iPTDstEnd = RT_MIN(iPTDst + PGM_SYNC_NR_PAGES / 2, RT_ELEMENTS(pPTDst->a));
                        if (iPTDst < PGM_SYNC_NR_PAGES / 2)
                            iPTDst = 0;
                        else
                            iPTDst -= PGM_SYNC_NR_PAGES / 2;
                        for (; iPTDst < iPTDstEnd; iPTDst++)
                        {
                            if (!pPTDst->a[iPTDst].n.u1Present)
                            {
                                GSTPTE PteSrc = pPTSrc->a[offPTSrc + iPTDst];
                                RTGCPTR GCPtrCurPage = (GCPtrPage & ~(RTGCPTR)(GST_PT_MASK << GST_PT_SHIFT)) | ((offPTSrc + iPTDst) << PAGE_SHIFT);
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
                                    ||  !CSAMDoesPageNeedScanning(pVM, (RTRCPTR)GCPtrCurPage)
                                    ||  (   (pPage = pgmPhysGetPage(&pVM->pgm.s, PteSrc.u & GST_PTE_PG_MASK))
                                         && PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                                   )
#endif /* else: CSAM not active */
                                    PGM_BTH_NAME(SyncPageWorker)(pVCpu, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);
                                Log2(("SyncPage: 4K+ %RGv PteSrc:{P=%d RW=%d U=%d raw=%08llx} PteDst=%08llx%s\n",
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
                        GSTPTE PteSrc = pPTSrc->a[iPTSrc];
                        const unsigned iPTDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
                        PGM_BTH_NAME(SyncPageWorker)(pVCpu, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);
                        Log2(("SyncPage: 4K  %RGv PteSrc:{P=%d RW=%d U=%d raw=%08llx}%s\n",
                              GCPtrPage, PteSrc.n.u1Present,
                              PteSrc.n.u1Write & PdeSrc.n.u1Write,
                              PteSrc.n.u1User & PdeSrc.n.u1User,
                              (uint64_t)PteSrc.u,
                              pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
                    }
                }
                else /* MMIO or invalid page: emulated in #PF handler. */
                {
                    LogFlow(("PGM_GCPHYS_2_PTR %RGp failed with %Rrc\n", GCPhys, rc));
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
                RTGCPHYS GCPhys = GST_GET_PDE_BIG_PG_GCPHYS(PdeSrc) | (GCPtrPage & GST_BIG_PAGE_OFFSET_MASK);
                /* Find ram range. */
                PPGMPAGE pPage;
                int rc = pgmPhysGetPageEx(&pVM->pgm.s, GCPhys, &pPage);
                if (RT_SUCCESS(rc))
                {
# ifndef VBOX_WITH_NEW_LAZY_PAGE_ALLOC
                    /* Try make the page writable if necessary. */
                    if (    PdeSrc.n.u1Write
                        &&  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED
                        &&  PGM_PAGE_GET_TYPE(pPage)  == PGMPAGETYPE_RAM)
                    {
                        rc = pgmPhysPageMakeWritableUnlocked(pVM, pPage, GCPhys);
                        AssertRC(rc);
                    }
# endif

                    /*
                     * Make shadow PTE entry.
                     */
                    SHWPTE PteDst;
                    PteDst.u = (PdeSrc.u & ~(X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT))
                             | PGM_PAGE_GET_HCPHYS(pPage);
                    if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                    {
                        if (!PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage))
                            PteDst.n.u1Write = 0;
                        else
                            PteDst.u = 0;
                    }
                    const unsigned iPTDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
# ifdef PGMPOOL_WITH_USER_TRACKING
                    if (PteDst.n.u1Present && !pPTDst->a[iPTDst].n.u1Present)
                        PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVCpu, pShwPage, PGM_PAGE_GET_TRACKING(pPage), pPage, iPTDst);
# endif
                    /* Make sure only allocated pages are mapped writable. */
                    if (    PteDst.n.u1Write
                        &&  PteDst.n.u1Present
                        &&  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED)
                    {
                        PteDst.n.u1Write = 0;   /** @todo this isn't quite working yet... */
                        Log3(("SyncPage: write-protecting %RGp pPage=%R[pgmpage] at %RGv\n", GCPhys, pPage, GCPtrPage));
                    }

                    ASMAtomicWriteSize(&pPTDst->a[iPTDst], PteDst.u);

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
                        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageBig));
                        PdeDst.u |= PGM_PDFLAGS_TRACK_DIRTY;
                        PdeDst.n.u1Write = 0;
                    }
                    else
                    {
                        PdeDst.au32[0] &= ~PGM_PDFLAGS_TRACK_DIRTY;
                        PdeDst.n.u1Write = PdeSrc.n.u1Write;
                    }
                    ASMAtomicWriteSize(pPdeDst, PdeDst.u);
                    Log2(("SyncPage: BIG %RGv PdeSrc:{P=%d RW=%d U=%d raw=%08llx} GCPhys=%RGp%s\n",
                          GCPtrPage, PdeSrc.n.u1Present, PdeSrc.n.u1Write, PdeSrc.n.u1User, (uint64_t)PdeSrc.u, GCPhys,
                          PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
                }
                else
                    LogFlow(("PGM_GCPHYS_2_PTR %RGp (big) failed with %Rrc\n", GCPhys, rc));
            }
# if defined(IN_RC)
            /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
            PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
            return VINF_SUCCESS;
        }
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPagePDNAs));
    }
    else
    {
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPagePDOutOfSync));
        Log2(("SyncPage: Out-Of-Sync PDE at %RGp PdeSrc=%RX64 PdeDst=%RX64 (GCPhys %RGp vs %RGp)\n",
              GCPtrPage, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u, pShwPage->GCPhys, GCPhys));
    }

    /*
     * Mark the PDE not present. Restart the instruction and let #PF call SyncPT.
     * Yea, I'm lazy.
     */
    pgmPoolFreeByPage(pPool, pShwPage, pShwPde->idx, iPDDst);
    ASMAtomicWriteSize(pPdeDst, 0);

# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
    PGM_INVL_GUEST_TLBS();
    return VINF_PGM_SYNCPAGE_MODIFIED_PDE;

#elif (PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED \
    && (PGM_SHW_TYPE != PGM_TYPE_EPT || PGM_GST_TYPE == PGM_TYPE_PROT) \
    && !defined(IN_RC)

# ifdef PGM_SYNC_N_PAGES
    /*
     * Get the shadow PDE, find the shadow page table in the pool.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    X86PDE          PdeDst = pgmShwGet32BitPDE(&pVCpu->pgm.s, GCPtrPage);

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    X86PDEPAE       PdeDst = pgmShwGetPaePDE(&pVCpu->pgm.s, GCPtrPage);

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    const unsigned  iPDDst   = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
    const unsigned  iPdpt    = (GCPtrPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64; NOREF(iPdpt);
    PX86PDPAE       pPDDst;
    X86PDEPAE       PdeDst;
    PX86PDPT        pPdptDst;

    int rc = pgmShwGetLongModePDPtr(pVCpu, GCPtrPage, NULL, &pPdptDst, &pPDDst);
    AssertRCSuccessReturn(rc, rc);
    Assert(pPDDst && pPdptDst);
    PdeDst = pPDDst->a[iPDDst];
# elif PGM_SHW_TYPE == PGM_TYPE_EPT
    const unsigned  iPDDst = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
    PEPTPD          pPDDst;
    EPTPDE          PdeDst;

    int rc = pgmShwGetEPTPDPtr(pVCpu, GCPtrPage, NULL, &pPDDst);
    if (rc != VINF_SUCCESS)
    {
        AssertRC(rc);
        return rc;
    }
    Assert(pPDDst);
    PdeDst = pPDDst->a[iPDDst];
# endif
    AssertMsg(PdeDst.n.u1Present, ("%#llx\n", (uint64_t)PdeDst.u));
    PPGMPOOLPAGE  pShwPage = pgmPoolGetPage(pPool, PdeDst.u & SHW_PDE_PG_MASK);
    PSHWPT        pPTDst   = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);

    Assert(cPages == 1 || !(uErr & X86_TRAP_PF_P));
    if (    cPages > 1
        &&  !(uErr & X86_TRAP_PF_P)
        &&  !VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY))
    {
        /*
         * This code path is currently only taken when the caller is PGMTrap0eHandler
         * for non-present pages!
         *
         * We're setting PGM_SYNC_NR_PAGES pages around the faulting page to sync it and
         * deal with locality.
         */
        unsigned        iPTDst    = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
        const unsigned  iPTDstEnd = RT_MIN(iPTDst + PGM_SYNC_NR_PAGES / 2, RT_ELEMENTS(pPTDst->a));
        if (iPTDst < PGM_SYNC_NR_PAGES / 2)
            iPTDst = 0;
        else
            iPTDst -= PGM_SYNC_NR_PAGES / 2;
        for (; iPTDst < iPTDstEnd; iPTDst++)
        {
            if (!pPTDst->a[iPTDst].n.u1Present)
            {
                GSTPTE PteSrc;

                RTGCPTR GCPtrCurPage = (GCPtrPage & ~(RTGCPTR)(SHW_PT_MASK << SHW_PT_SHIFT)) | (iPTDst << PAGE_SHIFT);

                /* Fake the page table entry */
                PteSrc.u = GCPtrCurPage;
                PteSrc.n.u1Present  = 1;
                PteSrc.n.u1Dirty    = 1;
                PteSrc.n.u1Accessed = 1;
                PteSrc.n.u1Write    = 1;
                PteSrc.n.u1User     = 1;

                PGM_BTH_NAME(SyncPageWorker)(pVCpu, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);

                Log2(("SyncPage: 4K+ %RGv PteSrc:{P=%d RW=%d U=%d raw=%08llx} PteDst=%08llx%s\n",
                      GCPtrCurPage, PteSrc.n.u1Present,
                      PteSrc.n.u1Write & PdeSrc.n.u1Write,
                      PteSrc.n.u1User & PdeSrc.n.u1User,
                      (uint64_t)PteSrc.u,
                      (uint64_t)pPTDst->a[iPTDst].u,
                      pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));

                if (RT_UNLIKELY(VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY)))
                    break;
            }
            else
                Log4(("%RGv iPTDst=%x pPTDst->a[iPTDst] %RX64\n", (GCPtrPage & ~(RTGCPTR)(SHW_PT_MASK << SHW_PT_SHIFT)) | (iPTDst << PAGE_SHIFT), iPTDst, pPTDst->a[iPTDst].u));
        }
    }
    else
# endif /* PGM_SYNC_N_PAGES */
    {
        GSTPTE PteSrc;
        const unsigned iPTDst = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
        RTGCPTR GCPtrCurPage = (GCPtrPage & ~(RTGCPTR)(SHW_PT_MASK << SHW_PT_SHIFT)) | (iPTDst << PAGE_SHIFT);

        /* Fake the page table entry */
        PteSrc.u = GCPtrCurPage;
        PteSrc.n.u1Present  = 1;
        PteSrc.n.u1Dirty    = 1;
        PteSrc.n.u1Accessed = 1;
        PteSrc.n.u1Write    = 1;
        PteSrc.n.u1User     = 1;
        PGM_BTH_NAME(SyncPageWorker)(pVCpu, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);

        Log2(("SyncPage: 4K  %RGv PteSrc:{P=%d RW=%d U=%d raw=%08llx}PteDst=%08llx%s\n",
              GCPtrPage, PteSrc.n.u1Present,
              PteSrc.n.u1Write & PdeSrc.n.u1Write,
              PteSrc.n.u1User & PdeSrc.n.u1User,
              (uint64_t)PteSrc.u,
              (uint64_t)pPTDst->a[iPTDst].u,
              pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
    }
    return VINF_SUCCESS;

#else
    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    return VERR_INTERNAL_ERROR;
#endif
}


#if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
/**
 * Investigate page fault and handle write protection page faults caused by
 * dirty bit tracking.
 *
 * @returns VBox status code.
 * @param   pVCpu       The VMCPU handle.
 * @param   uErr        Page fault error code.
 * @param   pPdeDst     Shadow page directory entry.
 * @param   pPdeSrc     Guest page directory entry.
 * @param   GCPtrPage   Guest context page address.
 */
PGM_BTH_DECL(int, CheckPageFault)(PVMCPU pVCpu, uint32_t uErr, PSHWPDE pPdeDst, PGSTPDE pPdeSrc, RTGCPTR GCPtrPage)
{
    bool fWriteProtect      = !!(CPUMGetGuestCR0(pVCpu) & X86_CR0_WP);
    bool fUserLevelFault    = !!(uErr & X86_TRAP_PF_US);
    bool fWriteFault        = !!(uErr & X86_TRAP_PF_RW);
# if PGM_GST_TYPE == PGM_TYPE_AMD64
    bool fBigPagesSupported = true;
# else
    bool fBigPagesSupported = !!(CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);
# endif
# if PGM_WITH_NX(PGM_GST_TYPE, PGM_SHW_TYPE)
    bool fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVCpu) & MSR_K6_EFER_NXE);
# endif
    unsigned uPageFaultLevel;
    int rc;
    PVM pVM = pVCpu->CTX_SUFF(pVM);
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

    Assert(PGMIsLockOwner(pVM));

    STAM_PROFILE_START(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
    LogFlow(("CheckPageFault: GCPtrPage=%RGv uErr=%#x PdeSrc=%08x\n", GCPtrPage, uErr, pPdeSrc->u));

# if    PGM_GST_TYPE == PGM_TYPE_PAE \
     || PGM_GST_TYPE == PGM_TYPE_AMD64

#  if PGM_GST_TYPE == PGM_TYPE_AMD64
    PX86PML4E    pPml4eSrc;
    PX86PDPE     pPdpeSrc;

    pPdpeSrc = pgmGstGetLongModePDPTPtr(&pVCpu->pgm.s, GCPtrPage, &pPml4eSrc);
    Assert(pPml4eSrc);

    /*
     * Real page fault? (PML4E level)
     */
    if (    (uErr & X86_TRAP_PF_RSVD)
        ||  !pPml4eSrc->n.u1Present
        ||  (fNoExecuteBitValid && (uErr & X86_TRAP_PF_ID) && pPml4eSrc->n.u1NoExecute)
        ||  (fWriteFault && !pPml4eSrc->n.u1Write && (fUserLevelFault || fWriteProtect))
        ||  (fUserLevelFault && !pPml4eSrc->n.u1User)
       )
    {
        uPageFaultLevel = 0;
        goto l_UpperLevelPageFault;
    }
    Assert(pPdpeSrc);

#  else  /* PAE */
    PX86PDPE pPdpeSrc = pgmGstGetPaePDPEPtr(&pVCpu->pgm.s, GCPtrPage);
#  endif /* PAE */

    /*
     * Real page fault? (PDPE level)
     */
    if (    (uErr & X86_TRAP_PF_RSVD)
        ||  !pPdpeSrc->n.u1Present
# if PGM_GST_TYPE == PGM_TYPE_AMD64 /* NX, r/w, u/s bits in the PDPE are long mode only */
        ||  (fNoExecuteBitValid && (uErr & X86_TRAP_PF_ID) && pPdpeSrc->lm.u1NoExecute)
        ||  (fWriteFault && !pPdpeSrc->lm.u1Write && (fUserLevelFault || fWriteProtect))
        ||  (fUserLevelFault && !pPdpeSrc->lm.u1User)
# endif
       )
    {
        uPageFaultLevel = 1;
        goto l_UpperLevelPageFault;
    }
# endif

    /*
     * Real page fault? (PDE level)
     */
    if (    (uErr & X86_TRAP_PF_RSVD)
        ||  !pPdeSrc->n.u1Present
# if PGM_WITH_NX(PGM_GST_TYPE, PGM_SHW_TYPE)
        ||  (fNoExecuteBitValid && (uErr & X86_TRAP_PF_ID) && pPdeSrc->n.u1NoExecute)
# endif
        ||  (fWriteFault && !pPdeSrc->n.u1Write && (fUserLevelFault || fWriteProtect))
        ||  (fUserLevelFault && !pPdeSrc->n.u1User) )
    {
        uPageFaultLevel = 2;
        goto l_UpperLevelPageFault;
    }

    /*
     * First check the easy case where the page directory has been marked read-only to track
     * the dirty bit of an emulated BIG page
     */
    if (pPdeSrc->b.u1Size && fBigPagesSupported)
    {
        /* Mark guest page directory as accessed */
#  if PGM_GST_TYPE == PGM_TYPE_AMD64
        pPml4eSrc->n.u1Accessed = 1;
        pPdpeSrc->lm.u1Accessed = 1;
#  endif
        pPdeSrc->b.u1Accessed   = 1;

        /*
         * Only write protection page faults are relevant here.
         */
        if (fWriteFault)
        {
            /* Mark guest page directory as dirty (BIG page only). */
            pPdeSrc->b.u1Dirty = 1;

            if (    pPdeDst->n.u1Present
                && (pPdeDst->u & PGM_PDFLAGS_TRACK_DIRTY))
            {
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageTrap));
                Assert(pPdeSrc->b.u1Write);

                /* Note: No need to invalidate this entry on other VCPUs as a stale TLB entry will not harm; write access will simply
                 *       fault again and take this path to only invalidate the entry.
                 */
                pPdeDst->n.u1Write      = 1;
                pPdeDst->n.u1Accessed   = 1;
                pPdeDst->au32[0]       &= ~PGM_PDFLAGS_TRACK_DIRTY;
                PGM_INVL_BIG_PG(GCPtrPage);
                STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
                return VINF_PGM_HANDLED_DIRTY_BIT_FAULT;
            }
        }
        STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
        return VINF_PGM_NO_DIRTY_BIT_TRACKING;
    }
    /* else: 4KB page table */

    /*
     * Map the guest page table.
     */
    PGSTPT pPTSrc;
    rc = PGM_GCPHYS_2_PTR(pVM, pPdeSrc->u & GST_PDE_PG_MASK, &pPTSrc);
    if (RT_SUCCESS(rc))
    {
        /*
         * Real page fault?
         */
        PGSTPTE        pPteSrc = &pPTSrc->a[(GCPtrPage >> GST_PT_SHIFT) & GST_PT_MASK];
        const GSTPTE   PteSrc = *pPteSrc;
        if (    !PteSrc.n.u1Present
#  if PGM_WITH_NX(PGM_GST_TYPE, PGM_SHW_TYPE)
            ||  (fNoExecuteBitValid && (uErr & X86_TRAP_PF_ID) && PteSrc.n.u1NoExecute)
#  endif
            ||  (fWriteFault && !PteSrc.n.u1Write && (fUserLevelFault || fWriteProtect))
            ||  (fUserLevelFault && !PteSrc.n.u1User)
           )
        {
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyTrackRealPF));
            STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
            LogFlow(("CheckPageFault: real page fault at %RGv PteSrc.u=%08x (2)\n", GCPtrPage, PteSrc.u));

            /* Check the present bit as the shadow tables can cause different error codes by being out of sync.
             * See the 2nd case above as well.
             */
            if (pPdeSrc->n.u1Present && pPteSrc->n.u1Present)
                TRPMSetErrorCode(pVCpu, uErr | X86_TRAP_PF_P); /* page-level protection violation */

            STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
            return VINF_EM_RAW_GUEST_TRAP;
        }
        LogFlow(("CheckPageFault: page fault at %RGv PteSrc.u=%08x\n", GCPtrPage, PteSrc.u));

        /*
         * Set the accessed bits in the page directory and the page table.
         */
#  if PGM_GST_TYPE == PGM_TYPE_AMD64
        pPml4eSrc->n.u1Accessed = 1;
        pPdpeSrc->lm.u1Accessed = 1;
#  endif
        pPdeSrc->n.u1Accessed   = 1;
        pPteSrc->n.u1Accessed   = 1;

        /*
         * Only write protection page faults are relevant here.
         */
        if (fWriteFault)
        {
            /* Write access, so mark guest entry as dirty. */
#  ifdef VBOX_WITH_STATISTICS
            if (!pPteSrc->n.u1Dirty)
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtiedPage));
            else
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageAlreadyDirty));
#  endif

            pPteSrc->n.u1Dirty = 1;

            if (pPdeDst->n.u1Present)
            {
#ifndef IN_RING0
                /* Bail out here as pgmPoolGetPageByHCPhys will return NULL and we'll crash below.
                 * Our individual shadow handlers will provide more information and force a fatal exit.
                 */
                if (MMHyperIsInsideArea(pVM, (RTGCPTR)GCPtrPage))
                {
                    LogRel(("CheckPageFault: write to hypervisor region %RGv\n", GCPtrPage));
                    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
                    return VINF_SUCCESS;
                }
#endif
                /*
                 * Map shadow page table.
                 */
                PPGMPOOLPAGE    pShwPage = pgmPoolGetPage(pPool, pPdeDst->u & SHW_PDE_PG_MASK);
                if (pShwPage)
                {
                    PSHWPT      pPTDst   = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
                    PSHWPTE     pPteDst  = &pPTDst->a[(GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK];
                    if (pPteDst->n.u1Present)    /** @todo Optimize accessed bit emulation? */
                    {
                        if (pPteDst->u & PGM_PTFLAGS_TRACK_DIRTY)
                        {
                            LogFlow(("DIRTY page trap addr=%RGv\n", GCPtrPage));
#  ifdef VBOX_STRICT
                            PPGMPAGE pPage = pgmPhysGetPage(&pVM->pgm.s, pPteSrc->u & GST_PTE_PG_MASK);
                            if (pPage)
                                AssertMsg(!PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage),
                                        ("Unexpected dirty bit tracking on monitored page %RGv (phys %RGp)!!!!!!\n", GCPtrPage, pPteSrc->u & X86_PTE_PAE_PG_MASK));
#  endif
                            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageTrap));

                            Assert(pPteSrc->n.u1Write);

                            /* Note: No need to invalidate this entry on other VCPUs as a stale TLB entry will not harm; write access will simply
                             *       fault again and take this path to only invalidate the entry.
                             */
                            pPteDst->n.u1Write    = 1;
                            pPteDst->n.u1Dirty    = 1;
                            pPteDst->n.u1Accessed = 1;
                            pPteDst->au32[0]     &= ~PGM_PTFLAGS_TRACK_DIRTY;
                            PGM_INVL_PG(GCPtrPage);

                            STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
                            return VINF_PGM_HANDLED_DIRTY_BIT_FAULT;
                        }
                        else
                        if (    pPteDst->n.u1Write == 1 
                            &&  pPteDst->n.u1Accessed == 1)
                        {
                            /* Stale TLB entry. */
                            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageStale));
                            PGM_INVL_PG(GCPtrPage);

                            STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
                            return VINF_PGM_HANDLED_DIRTY_BIT_FAULT;
                        }
                    }
                }
                else
                    AssertMsgFailed(("pgmPoolGetPageByHCPhys %RGp failed!\n", pPdeDst->u & SHW_PDE_PG_MASK));
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
            PPGMPOOLPAGE    pShwPage = pgmPoolGetPage(pPool, pPdeDst->u & SHW_PDE_PG_MASK);
            PSHWPT          pPTDst   = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
            PSHWPTE         pPteDst  = &pPTDst->a[(GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK];
            if (    pPteDst->n.u1Present
                &&  pPteDst->n.u1Write)
                LogFlow(("Writable present page %RGv not marked for dirty bit tracking!!!\n", GCPtrPage));
        }
#  endif /* VBOX_STRICT */
        STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
        return VINF_PGM_NO_DIRTY_BIT_TRACKING;
    }
    AssertRC(rc);
    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
    return rc;


l_UpperLevelPageFault:
    /*
     * Pagefault detected while checking the PML4E, PDPE or PDE.
     * Single exit handler to get rid of duplicate code paths.
     */
    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyTrackRealPF));
    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyBitTracking), a);
    Log(("CheckPageFault: real page fault at %RGv (%d)\n", GCPtrPage, uPageFaultLevel));

    if (
#  if PGM_GST_TYPE == PGM_TYPE_AMD64
            pPml4eSrc->n.u1Present &&
#  endif
#  if PGM_GST_TYPE == PGM_TYPE_AMD64 || PGM_GST_TYPE == PGM_TYPE_PAE
            pPdpeSrc->n.u1Present  &&
#  endif
            pPdeSrc->n.u1Present)
    {
        /* Check the present bit as the shadow tables can cause different error codes by being out of sync. */
        if (pPdeSrc->b.u1Size && fBigPagesSupported)
        {
            TRPMSetErrorCode(pVCpu, uErr | X86_TRAP_PF_P); /* page-level protection violation */
        }
        else
        {
            /*
             * Map the guest page table.
             */
            PGSTPT pPTSrc;
            rc = PGM_GCPHYS_2_PTR(pVM, pPdeSrc->u & GST_PDE_PG_MASK, &pPTSrc);
            if (RT_SUCCESS(rc))
            {
                PGSTPTE         pPteSrc = &pPTSrc->a[(GCPtrPage >> GST_PT_SHIFT) & GST_PT_MASK];
                const GSTPTE    PteSrc = *pPteSrc;
                if (pPteSrc->n.u1Present)
                    TRPMSetErrorCode(pVCpu, uErr | X86_TRAP_PF_P); /* page-level protection violation */
            }
            AssertRC(rc);
        }
    }
    return VINF_EM_RAW_GUEST_TRAP;
}
#endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */


/**
 * Sync a shadow page table.
 *
 * The shadow page table is not present. This includes the case where
 * there is a conflict with a mapping.
 *
 * @returns VBox status code.
 * @param   pVCpu       The VMCPU handle.
 * @param   iPD         Page directory index.
 * @param   pPDSrc      Source page directory (i.e. Guest OS page directory).
 *                      Assume this is a temporary mapping.
 * @param   GCPtrPage   GC Pointer of the page that caused the fault
 */
PGM_BTH_DECL(int, SyncPT)(PVMCPU pVCpu, unsigned iPDSrc, PGSTPD pPDSrc, RTGCPTR GCPtrPage)
{
    PVM      pVM   = pVCpu->CTX_SUFF(pVM);
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

    STAM_PROFILE_START(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT), a);
    STAM_COUNTER_INC(&pVCpu->pgm.s.StatSyncPtPD[iPDSrc]);
    LogFlow(("SyncPT: GCPtrPage=%RGv\n", GCPtrPage));

    Assert(PGMIsLocked(pVM));

#if   (   PGM_GST_TYPE == PGM_TYPE_32BIT  \
       || PGM_GST_TYPE == PGM_TYPE_PAE    \
       || PGM_GST_TYPE == PGM_TYPE_AMD64) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED    \
    && PGM_SHW_TYPE != PGM_TYPE_EPT

    int             rc = VINF_SUCCESS;

    /*
     * Validate input a little bit.
     */
    AssertMsg(iPDSrc == ((GCPtrPage >> GST_PD_SHIFT) & GST_PD_MASK), ("iPDSrc=%x GCPtrPage=%RGv\n", iPDSrc, GCPtrPage));
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDDst   = GCPtrPage >> SHW_PD_SHIFT;
    PSHWPDE         pPdeDst  = pgmShwGet32BitPDEPtr(&pVCpu->pgm.s, GCPtrPage);

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde  = pVCpu->pgm.s.CTX_SUFF(pShwPageCR3);
    Assert(pShwPde);

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    const unsigned  iPDDst  = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PPGMPOOLPAGE    pShwPde = NULL;
    PX86PDPAE       pPDDst;
    PSHWPDE         pPdeDst;

    /* Fetch the pgm pool shadow descriptor. */
    rc = pgmShwGetPaePoolPagePD(&pVCpu->pgm.s, GCPtrPage, &pShwPde);
    AssertRCSuccessReturn(rc, rc);
    Assert(pShwPde);

    pPDDst  = (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGM(&pVM->pgm.s, pShwPde);
    pPdeDst = &pPDDst->a[iPDDst];

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    const unsigned  iPdpt    = (GCPtrPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
    const unsigned  iPDDst   = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PX86PDPAE       pPDDst;
    PX86PDPT        pPdptDst;
    rc = pgmShwGetLongModePDPtr(pVCpu, GCPtrPage, NULL, &pPdptDst, &pPDDst);
    AssertRCSuccessReturn(rc, rc);
    Assert(pPDDst);
    PSHWPDE         pPdeDst = &pPDDst->a[iPDDst];
# endif
    SHWPDE          PdeDst = *pPdeDst;

# if PGM_GST_TYPE == PGM_TYPE_AMD64
    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE pShwPde = pgmPoolGetPage(pPool, pPdptDst->a[iPdpt].u & X86_PDPE_PG_MASK);
    Assert(pShwPde);
# endif

# ifndef PGM_WITHOUT_MAPPINGS
    /*
     * Check for conflicts.
     * GC: In case of a conflict we'll go to Ring-3 and do a full SyncCR3.
     * HC: Simply resolve the conflict.
     */
    if (PdeDst.u & PGM_PDFLAGS_MAPPING)
    {
        Assert(pgmMapAreMappingsEnabled(&pVM->pgm.s));
#  ifndef IN_RING3
        Log(("SyncPT: Conflict at %RGv\n", GCPtrPage));
        STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT), a);
        return VERR_ADDRESS_CONFLICT;
#  else
        PPGMMAPPING pMapping = pgmGetMapping(pVM, (RTGCPTR)GCPtrPage);
        Assert(pMapping);
#   if PGM_GST_TYPE == PGM_TYPE_32BIT
        int rc = pgmR3SyncPTResolveConflict(pVM, pMapping, pPDSrc, GCPtrPage & (GST_PD_MASK << GST_PD_SHIFT));
#   elif PGM_GST_TYPE == PGM_TYPE_PAE
        int rc = pgmR3SyncPTResolveConflictPAE(pVM, pMapping, GCPtrPage & (GST_PD_MASK << GST_PD_SHIFT));
#   else
        AssertFailed();     /* can't happen for amd64 */
#   endif
        if (RT_FAILURE(rc))
        {
            STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT), a);
            return rc;
        }
        PdeDst = *pPdeDst;
#  endif
    }
# else  /* PGM_WITHOUT_MAPPINGS */
    Assert(!pgmMapAreMappingsEnabled(&pVM->pgm.s));
# endif /* PGM_WITHOUT_MAPPINGS */
    Assert(!PdeDst.n.u1Present); /* We're only supposed to call SyncPT on PDE!P and conflicts.*/

# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynLockHCPage(pVM, (uint8_t *)pPdeDst);
# endif

    /*
     * Sync page directory entry.
     */
    GSTPDE  PdeSrc = pPDSrc->a[iPDSrc];
    if (PdeSrc.n.u1Present)
    {
        /*
         * Allocate & map the page table.
         */
        PSHWPT          pPTDst;
# if PGM_GST_TYPE == PGM_TYPE_AMD64
        const bool      fPageTable = !PdeSrc.b.u1Size;
# else
        const bool      fPageTable = !PdeSrc.b.u1Size || !(CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);
# endif
        PPGMPOOLPAGE    pShwPage;
        RTGCPHYS        GCPhys;
        if (fPageTable)
        {
            GCPhys = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
            /* Select the right PDE as we're emulating a 4kb page table with 2 shadow page tables. */
            GCPhys |= (iPDDst & 1) * (PAGE_SIZE / 2);
# endif
            rc = pgmPoolAlloc(pVM, GCPhys, BTH_PGMPOOLKIND_PT_FOR_PT, pShwPde->idx,      iPDDst, &pShwPage);
        }
        else
        {
            GCPhys = GST_GET_PDE_BIG_PG_GCPHYS(PdeSrc);
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
            /* Select the right PDE as we're emulating a 4MB page directory with two 2 MB shadow PDEs.*/
            GCPhys |= GCPtrPage & (1 << X86_PD_PAE_SHIFT);
# endif
            rc = pgmPoolAlloc(pVM, GCPhys, BTH_PGMPOOLKIND_PT_FOR_BIG, pShwPde->idx,      iPDDst, &pShwPage);
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
                         | (PdeSrc.u & ~(GST_PDE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));
            else
            {
                PdeDst.u = pShwPage->Core.Key
                         | (PdeSrc.u & ~(GST_PDE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));
                /* (see explanation and assumptions further down.) */
                if (!PdeSrc.b.u1Dirty && PdeSrc.b.u1Write)
                {
                    STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageBig));
                    PdeDst.u |= PGM_PDFLAGS_TRACK_DIRTY;
                    PdeDst.b.u1Write = 0;
                }
            }
            ASMAtomicWriteSize(pPdeDst, PdeDst.u);
# if defined(IN_RC)
            PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
            return VINF_SUCCESS;
        }
        else if (rc == VERR_PGM_POOL_FLUSHED)
        {
            VMCPU_FF_SET(pVCpu, VMCPU_FF_PGM_SYNC_CR3);
# if defined(IN_RC)
            PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
            return VINF_PGM_SYNC_CR3;
        }
        else
            AssertMsgFailedReturn(("rc=%Rrc\n", rc), VERR_INTERNAL_ERROR);
        PdeDst.u &= X86_PDE_AVL_MASK;
        PdeDst.u |= pShwPage->Core.Key;

        /*
         * Page directory has been accessed (this is a fault situation, remember).
         */
        pPDSrc->a[iPDSrc].n.u1Accessed = 1;
        if (fPageTable)
        {
            /*
             * Page table - 4KB.
             *
             * Sync all or just a few entries depending on PGM_SYNC_N_PAGES.
             */
            Log2(("SyncPT:   4K  %RGv PdeSrc:{P=%d RW=%d U=%d raw=%08llx}\n",
                  GCPtrPage, PdeSrc.b.u1Present, PdeSrc.b.u1Write, PdeSrc.b.u1User, (uint64_t)PdeSrc.u));
            PGSTPT pPTSrc;
            rc = PGM_GCPHYS_2_PTR(pVM, PdeSrc.u & GST_PDE_PG_MASK, &pPTSrc);
            if (RT_SUCCESS(rc))
            {
                /*
                 * Start by syncing the page directory entry so CSAM's TLB trick works.
                 */
                PdeDst.u = (PdeDst.u & (SHW_PDE_PG_MASK | X86_PDE_AVL_MASK))
                         | (PdeSrc.u & ~(GST_PDE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));
                ASMAtomicWriteSize(pPdeDst, PdeDst.u);
# if defined(IN_RC)
                PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif

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
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT4K));
# ifdef PGM_SYNC_N_PAGES
                unsigned        iPTBase   = (GCPtrPage >> SHW_PT_SHIFT) & SHW_PT_MASK;
                unsigned        iPTDst    = iPTBase;
                const unsigned  iPTDstEnd = RT_MIN(iPTDst + PGM_SYNC_NR_PAGES / 2, RT_ELEMENTS(pPTDst->a));
                if (iPTDst <= PGM_SYNC_NR_PAGES / 2)
                    iPTDst = 0;
                else
                    iPTDst -= PGM_SYNC_NR_PAGES / 2;
# else /* !PGM_SYNC_N_PAGES */
                unsigned        iPTDst    = 0;
                const unsigned  iPTDstEnd = RT_ELEMENTS(pPTDst->a);
# endif /* !PGM_SYNC_N_PAGES */
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                /* Select the right PDE as we're emulating a 4kb page table with 2 shadow page tables. */
                const unsigned  offPTSrc  = ((GCPtrPage >> SHW_PD_SHIFT) & 1) * 512;
# else
                const unsigned  offPTSrc  = 0;
# endif
                for (; iPTDst < iPTDstEnd; iPTDst++)
                {
                    const unsigned iPTSrc = iPTDst + offPTSrc;
                    const GSTPTE   PteSrc = pPTSrc->a[iPTSrc];

                    if (PteSrc.n.u1Present) /* we've already cleared it above */
                    {
# ifndef IN_RING0
                        /*
                         * Assuming kernel code will be marked as supervisor - and not as user level
                         * and executed using a conforming code selector - And marked as readonly.
                         * Also assume that if we're monitoring a page, it's of no interest to CSAM.
                         */
                        PPGMPAGE pPage;
                        if (    ((PdeSrc.u & pPTSrc->a[iPTSrc].u) & (X86_PTE_RW | X86_PTE_US))
                            ||  !CSAMDoesPageNeedScanning(pVM, (RTRCPTR)((iPDSrc << GST_PD_SHIFT) | (iPTSrc << PAGE_SHIFT)))
                            ||  (   (pPage = pgmPhysGetPage(&pVM->pgm.s, PteSrc.u & GST_PTE_PG_MASK))
                                 &&  PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                           )
# endif
                            PGM_BTH_NAME(SyncPageWorker)(pVCpu, &pPTDst->a[iPTDst], PdeSrc, PteSrc, pShwPage, iPTDst);
                        Log2(("SyncPT:   4K+ %RGv PteSrc:{P=%d RW=%d U=%d raw=%08llx}%s dst.raw=%08llx iPTSrc=%x PdeSrc.u=%x physpte=%RGp\n",
                              (RTGCPTR)((iPDSrc << GST_PD_SHIFT) | (iPTSrc << PAGE_SHIFT)),
                              PteSrc.n.u1Present,
                              PteSrc.n.u1Write & PdeSrc.n.u1Write,
                              PteSrc.n.u1User & PdeSrc.n.u1User,
                              (uint64_t)PteSrc.u,
                              pPTDst->a[iPTDst].u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : "", pPTDst->a[iPTDst].u, iPTSrc, PdeSrc.au32[0],
                              (RTGCPHYS)((PdeSrc.u & GST_PDE_PG_MASK) + iPTSrc*sizeof(PteSrc)) ));
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
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT4M));

            /**
             * @todo It might be more efficient to sync only a part of the 4MB page (similar to what we do for 4kb PDs).
             */

            /*
             * Start by syncing the page directory entry.
             */
            PdeDst.u = (PdeDst.u & (SHW_PDE_PG_MASK | (X86_PDE_AVL_MASK & ~PGM_PDFLAGS_TRACK_DIRTY)))
                     | (PdeSrc.u & ~(GST_PDE_PG_MASK | X86_PDE_AVL_MASK | X86_PDE_PCD | X86_PDE_PWT | X86_PDE_PS | X86_PDE4M_G | X86_PDE4M_D));

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
                STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,DirtyPageBig));
                PdeDst.u |= PGM_PDFLAGS_TRACK_DIRTY;
                PdeDst.b.u1Write = 0;
            }
            ASMAtomicWriteSize(pPdeDst, PdeDst.u);
# if defined(IN_RC)
            PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif

            /*
             * Fill the shadow page table.
             */
            /* Get address and flags from the source PDE. */
            SHWPTE PteDstBase;
            PteDstBase.u = PdeSrc.u & ~(GST_PDE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PAT | X86_PTE_PCD | X86_PTE_PWT);

            /* Loop thru the entries in the shadow PT. */
            const RTGCPTR   GCPtr  = (GCPtrPage >> SHW_PD_SHIFT) << SHW_PD_SHIFT; NOREF(GCPtr);
            Log2(("SyncPT:   BIG %RGv PdeSrc:{P=%d RW=%d U=%d raw=%08llx} Shw=%RGv GCPhys=%RGp %s\n",
                  GCPtrPage, PdeSrc.b.u1Present, PdeSrc.b.u1Write, PdeSrc.b.u1User, (uint64_t)PdeSrc.u, GCPtr,
                  GCPhys, PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));
            PPGMRAMRANGE    pRam   = pVM->pgm.s.CTX_SUFF(pRamRanges);
            unsigned        iPTDst = 0;
            while (     iPTDst < RT_ELEMENTS(pPTDst->a)
                   &&   !VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY))
            {
                /* Advance ram range list. */
                while (pRam && GCPhys > pRam->GCPhysLast)
                    pRam = pRam->CTX_SUFF(pNext);
                if (pRam && GCPhys >= pRam->GCPhys)
                {
                    unsigned iHCPage = (GCPhys - pRam->GCPhys) >> PAGE_SHIFT;
                    do
                    {
                        /* Make shadow PTE. */
                        PPGMPAGE    pPage = &pRam->aPages[iHCPage];
                        SHWPTE      PteDst;

# ifndef VBOX_WITH_NEW_LAZY_PAGE_ALLOC
                        /* Try make the page writable if necessary. */
                        if (    PteDstBase.n.u1Write
                            &&  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED
                            &&  PGM_PAGE_GET_TYPE(pPage)  == PGMPAGETYPE_RAM)
                        {
                            rc = pgmPhysPageMakeWritableUnlocked(pVM, pPage, GCPhys);
                            AssertRCReturn(rc, rc);
                            if (VM_FF_ISPENDING(pVM, VM_FF_PGM_NO_MEMORY))
                                break;
                        }
# endif

                        if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPage))
                        {
                            if (!PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPage))
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
                                 &&  CSAMDoesPageNeedScanning(pVM, (RTRCPTR)(GCPtr | (iPTDst << SHW_PT_SHIFT))))
                            PteDst.u = 0;
# endif
                        else
                            PteDst.u = PGM_PAGE_GET_HCPHYS(pPage) | PteDstBase.u;

                        /* Only map writable pages writable. */
                        if (    PteDst.n.u1Write
                            &&  PteDst.n.u1Present
                            &&  PGM_PAGE_GET_STATE(pPage) != PGM_PAGE_STATE_ALLOCATED)
                        {
                            PteDst.n.u1Write = 0;   /** @todo this isn't quite working yet... */
                            Log3(("SyncPT: write-protecting %RGp pPage=%R[pgmpage] at %RGv\n", GCPhys, pPage, (RTGCPTR)(GCPtr | (iPTDst << SHW_PT_SHIFT))));
                        }

# ifdef PGMPOOL_WITH_USER_TRACKING
                        if (PteDst.n.u1Present)
                            PGM_BTH_NAME(SyncPageWorkerTrackAddref)(pVCpu, pShwPage, PGM_PAGE_GET_TRACKING(pPage), pPage, iPTDst);
# endif
                        /* commit it */
                        pPTDst->a[iPTDst] = PteDst;
                        Log4(("SyncPT: BIG %RGv PteDst:{P=%d RW=%d U=%d raw=%08llx}%s\n",
                              (RTGCPTR)(GCPtr | (iPTDst << SHW_PT_SHIFT)), PteDst.n.u1Present, PteDst.n.u1Write, PteDst.n.u1User, (uint64_t)PteDst.u,
                              PteDst.u & PGM_PTFLAGS_TRACK_DIRTY ? " Track-Dirty" : ""));

                        /* advance */
                        GCPhys += PAGE_SIZE;
                        iHCPage++;
                        iPTDst++;
                    } while (   iPTDst < RT_ELEMENTS(pPTDst->a)
                             && GCPhys <= pRam->GCPhysLast);
                }
                else if (pRam)
                {
                    Log(("Invalid pages at %RGp\n", GCPhys));
                    do
                    {
                        pPTDst->a[iPTDst].u = 0; /* MMIO or invalid page, we must handle them manually. */
                        GCPhys += PAGE_SIZE;
                        iPTDst++;
                    } while (   iPTDst < RT_ELEMENTS(pPTDst->a)
                             && GCPhys < pRam->GCPhys);
                }
                else
                {
                    Log(("Invalid pages at %RGp (2)\n", GCPhys));
                    for ( ; iPTDst < RT_ELEMENTS(pPTDst->a); iPTDst++)
                        pPTDst->a[iPTDst].u = 0; /* MMIO or invalid page, we must handle them manually. */
                }
            } /* while more PTEs */
        } /* 4KB / 4MB */
    }
    else
        AssertRelease(!PdeDst.n.u1Present);

    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT), a);
    if (RT_FAILURE(rc))
        STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPTFailed));
    return rc;

#elif (PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED \
    && (PGM_SHW_TYPE != PGM_TYPE_EPT || PGM_GST_TYPE == PGM_TYPE_PROT) \
    && !defined(IN_RC)

    /*
     * Validate input a little bit.
     */
    int             rc = VINF_SUCCESS;
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDDst  = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PSHWPDE         pPdeDst = pgmShwGet32BitPDEPtr(&pVCpu->pgm.s, GCPtrPage);

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pVCpu->pgm.s.CTX_SUFF(pShwPageCR3);
    Assert(pShwPde);

# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    const unsigned  iPDDst  = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PPGMPOOLPAGE    pShwPde;
    PX86PDPAE       pPDDst;
    PSHWPDE         pPdeDst;

    /* Fetch the pgm pool shadow descriptor. */
    rc = pgmShwGetPaePoolPagePD(&pVCpu->pgm.s, GCPtrPage, &pShwPde);
    AssertRCSuccessReturn(rc, rc);
    Assert(pShwPde);

    pPDDst  = (PX86PDPAE)PGMPOOL_PAGE_2_PTR_BY_PGM(&pVM->pgm.s, pShwPde);
    pPdeDst = &pPDDst->a[iPDDst];

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    const unsigned  iPdpt   = (GCPtrPage >> X86_PDPT_SHIFT) & X86_PDPT_MASK_AMD64;
    const unsigned  iPDDst  = (GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK;
    PX86PDPAE       pPDDst;
    PX86PDPT        pPdptDst;
    rc = pgmShwGetLongModePDPtr(pVCpu, GCPtrPage, NULL, &pPdptDst, &pPDDst);
    AssertRCSuccessReturn(rc, rc);
    Assert(pPDDst);
    PSHWPDE         pPdeDst = &pPDDst->a[iPDDst];

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE    pShwPde = pgmPoolGetPage(pPool, pPdptDst->a[iPdpt].u & X86_PDPE_PG_MASK);
    Assert(pShwPde);

# elif PGM_SHW_TYPE == PGM_TYPE_EPT
    const unsigned  iPdpt   = (GCPtrPage >> EPT_PDPT_SHIFT) & EPT_PDPT_MASK;
    const unsigned  iPDDst  = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
    PEPTPD          pPDDst;
    PEPTPDPT        pPdptDst;

    rc = pgmShwGetEPTPDPtr(pVCpu, GCPtrPage, &pPdptDst, &pPDDst);
    if (rc != VINF_SUCCESS)
    {
        AssertRC(rc);
        return rc;
    }
    Assert(pPDDst);
    PSHWPDE         pPdeDst = &pPDDst->a[iPDDst];

    /* Fetch the pgm pool shadow descriptor. */
    PPGMPOOLPAGE pShwPde = pgmPoolGetPage(pPool, pPdptDst->a[iPdpt].u & EPT_PDPTE_PG_MASK);
    Assert(pShwPde);
# endif
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
    GCPhys = GCPtrPage & X86_PAGE_4K_BASE_MASK;
    rc = pgmPoolAlloc(pVM, GCPhys & ~(RT_BIT_64(SHW_PD_SHIFT) - 1), BTH_PGMPOOLKIND_PT_FOR_PT, pShwPde->idx, iPDDst, &pShwPage);

    if (    rc == VINF_SUCCESS
        ||  rc == VINF_PGM_CACHED_PAGE)
        pPTDst = (PSHWPT)PGMPOOL_PAGE_2_PTR(pVM, pShwPage);
    else
        AssertMsgFailedReturn(("rc=%Rrc\n", rc), VERR_INTERNAL_ERROR);

    PdeDst.u &= X86_PDE_AVL_MASK;
    PdeDst.u |= pShwPage->Core.Key;
    PdeDst.n.u1Present  = 1;
    PdeDst.n.u1Write    = 1;
# if PGM_SHW_TYPE == PGM_TYPE_EPT
    PdeDst.n.u1Execute  = 1;
# else
    PdeDst.n.u1User     = 1;
    PdeDst.n.u1Accessed = 1;
# endif
    ASMAtomicWriteSize(pPdeDst, PdeDst.u);

    rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, GCPtrPage, PGM_SYNC_NR_PAGES, 0 /* page not present */);
    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT), a);
    return rc;

#else
    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_SHW_TYPE, PGM_GST_TYPE));
    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncPT), a);
    return VERR_INTERNAL_ERROR;
#endif
}



/**
 * Prefetch a page/set of pages.
 *
 * Typically used to sync commonly used pages before entering raw mode
 * after a CR3 reload.
 *
 * @returns VBox status code.
 * @param   pVCpu       The VMCPU handle.
 * @param   GCPtrPage   Page to invalidate.
 */
PGM_BTH_DECL(int, PrefetchPage)(PVMCPU pVCpu, RTGCPTR GCPtrPage)
{
#if   (PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_REAL || PGM_GST_TYPE == PGM_TYPE_PROT || PGM_GST_TYPE == PGM_TYPE_PAE || PGM_GST_TYPE == PGM_TYPE_AMD64) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT
    /*
     * Check that all Guest levels thru the PDE are present, getting the
     * PD and PDE in the processes.
     */
    int             rc      = VINF_SUCCESS;
# if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDSrc = GCPtrPage >> GST_PD_SHIFT;
    PGSTPD          pPDSrc = pgmGstGet32bitPDPtr(&pVCpu->pgm.s);
#  elif PGM_GST_TYPE == PGM_TYPE_PAE
    unsigned        iPDSrc;
    X86PDPE         PdpeSrc;
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVCpu->pgm.s, GCPtrPage, &iPDSrc, &PdpeSrc);
    if (!pPDSrc)
        return VINF_SUCCESS; /* not present */
#  elif PGM_GST_TYPE == PGM_TYPE_AMD64
    unsigned        iPDSrc;
    PX86PML4E       pPml4eSrc;
    X86PDPE         PdpeSrc;
    PGSTPD          pPDSrc = pgmGstGetLongModePDPtr(&pVCpu->pgm.s, GCPtrPage, &pPml4eSrc, &PdpeSrc, &iPDSrc);
    if (!pPDSrc)
        return VINF_SUCCESS; /* not present */
#  endif
    const GSTPDE    PdeSrc = pPDSrc->a[iPDSrc];
# else
    PGSTPD          pPDSrc = NULL;
    const unsigned  iPDSrc = 0;
    GSTPDE          PdeSrc;

    PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
    PdeSrc.n.u1Present  = 1;
    PdeSrc.n.u1Write    = 1;
    PdeSrc.n.u1Accessed = 1;
    PdeSrc.n.u1User     = 1;
# endif

    if (PdeSrc.n.u1Present && PdeSrc.n.u1Accessed)
    {
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
        const X86PDE    PdeDst = pgmShwGet32BitPDE(&pVCpu->pgm.s, GCPtrPage);
# elif PGM_SHW_TYPE == PGM_TYPE_PAE
        const unsigned  iPDDst = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
        PX86PDPAE       pPDDst;
        X86PDEPAE       PdeDst;
#   if PGM_GST_TYPE != PGM_TYPE_PAE
        X86PDPE         PdpeSrc;

        /* Fake PDPT entry; access control handled on the page table level, so allow everything. */
        PdpeSrc.u  = X86_PDPE_P;   /* rw/us are reserved for PAE pdpte's; accessed bit causes invalid VT-x guest state errors */
#   endif
        int rc = pgmShwSyncPaePDPtr(pVCpu, GCPtrPage, &PdpeSrc, &pPDDst);
        if (rc != VINF_SUCCESS)
        {
            AssertRC(rc);
            return rc;
        }
        Assert(pPDDst);
        PdeDst = pPDDst->a[iPDDst];

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
        const unsigned  iPDDst = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
        PX86PDPAE       pPDDst;
        X86PDEPAE       PdeDst;

#  if PGM_GST_TYPE == PGM_TYPE_PROT
        /* AMD-V nested paging */
        X86PML4E        Pml4eSrc;
        X86PDPE         PdpeSrc;
        PX86PML4E       pPml4eSrc = &Pml4eSrc;

        /* Fake PML4 & PDPT entry; access control handled on the page table level, so allow everything. */
        Pml4eSrc.u = X86_PML4E_P | X86_PML4E_RW | X86_PML4E_US | X86_PML4E_NX | X86_PML4E_A;
        PdpeSrc.u  = X86_PDPE_P | X86_PDPE_RW | X86_PDPE_US | X86_PDPE_NX | X86_PDPE_A;
#  endif

        int rc = pgmShwSyncLongModePDPtr(pVCpu, GCPtrPage, pPml4eSrc, &PdpeSrc, &pPDDst);
        if (rc != VINF_SUCCESS)
        {
            AssertRC(rc);
            return rc;
        }
        Assert(pPDDst);
        PdeDst = pPDDst->a[iPDDst];
# endif
        if (!(PdeDst.u & PGM_PDFLAGS_MAPPING))
        {
            if (!PdeDst.n.u1Present)
            {
                PVM pVM = pVCpu->CTX_SUFF(pVM);
                /** r=bird: This guy will set the A bit on the PDE, probably harmless. */
                pgmLock(pVM);
                rc = PGM_BTH_NAME(SyncPT)(pVCpu, iPDSrc, pPDSrc, GCPtrPage);
                pgmUnlock(pVM);
            }
            else
            {
                /** @note We used to sync PGM_SYNC_NR_PAGES pages, which triggered assertions in CSAM, because
                 *        R/W attributes of nearby pages were reset. Not sure how that could happen. Anyway, it
                 *        makes no sense to prefetch more than one page.
                 */
                rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, GCPtrPage, 1, 0);
                if (RT_SUCCESS(rc))
                    rc = VINF_SUCCESS;
            }
        }
    }
    return rc;

#elif PGM_SHW_TYPE == PGM_TYPE_NESTED || PGM_SHW_TYPE == PGM_TYPE_EPT
    return VINF_SUCCESS; /* ignore */
#endif
}




/**
 * Syncs a page during a PGMVerifyAccess() call.
 *
 * @returns VBox status code (informational included).
 * @param   pVCpu       The VMCPU handle.
 * @param   GCPtrPage   The address of the page to sync.
 * @param   fPage       The effective guest page flags.
 * @param   uErr        The trap error code.
 */
PGM_BTH_DECL(int, VerifyAccessSyncPage)(PVMCPU pVCpu, RTGCPTR GCPtrPage, unsigned fPage, unsigned uErr)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    LogFlow(("VerifyAccessSyncPage: GCPtrPage=%RGv fPage=%#x uErr=%#x\n", GCPtrPage, fPage, uErr));

    Assert(!HWACCMIsNestedPagingActive(pVM));
#if   (PGM_GST_TYPE == PGM_TYPE_32BIT ||  PGM_GST_TYPE == PGM_TYPE_REAL ||  PGM_GST_TYPE == PGM_TYPE_PROT || PGM_GST_TYPE == PGM_TYPE_PAE || PGM_TYPE_AMD64) \
    && PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT

# ifndef IN_RING0
    if (!(fPage & X86_PTE_US))
    {
        /*
         * Mark this page as safe.
         */
        /** @todo not correct for pages that contain both code and data!! */
        Log(("CSAMMarkPage %RGv; scanned=%d\n", GCPtrPage, true));
        CSAMMarkPage(pVM, (RTRCPTR)GCPtrPage, true);
    }
# endif

    /*
     * Get guest PD and index.
     */
# if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    const unsigned  iPDSrc = GCPtrPage >> GST_PD_SHIFT;
    PGSTPD          pPDSrc = pgmGstGet32bitPDPtr(&pVCpu->pgm.s);
#  elif PGM_GST_TYPE == PGM_TYPE_PAE
    unsigned        iPDSrc  = 0;
    X86PDPE         PdpeSrc;
    PGSTPD          pPDSrc = pgmGstGetPaePDPtr(&pVCpu->pgm.s, GCPtrPage, &iPDSrc, &PdpeSrc);

    if (pPDSrc)
    {
        Log(("PGMVerifyAccess: access violation for %RGv due to non-present PDPTR\n", GCPtrPage));
        return VINF_EM_RAW_GUEST_TRAP;
    }
#  elif PGM_GST_TYPE == PGM_TYPE_AMD64
    unsigned        iPDSrc;
    PX86PML4E       pPml4eSrc;
    X86PDPE         PdpeSrc;
    PGSTPD          pPDSrc = pgmGstGetLongModePDPtr(&pVCpu->pgm.s, GCPtrPage, &pPml4eSrc, &PdpeSrc, &iPDSrc);
    if (!pPDSrc)
    {
        Log(("PGMVerifyAccess: access violation for %RGv due to non-present PDPTR\n", GCPtrPage));
        return VINF_EM_RAW_GUEST_TRAP;
    }
#  endif
# else
    PGSTPD          pPDSrc = NULL;
    const unsigned  iPDSrc = 0;
# endif
    int             rc = VINF_SUCCESS;

    pgmLock(pVM);

    /*
     * First check if the shadow pd is present.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    PX86PDE         pPdeDst = pgmShwGet32BitPDEPtr(&pVCpu->pgm.s, GCPtrPage);
# elif PGM_SHW_TYPE == PGM_TYPE_PAE
    PX86PDEPAE      pPdeDst;
    const unsigned  iPDDst = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
    PX86PDPAE       pPDDst;
#   if PGM_GST_TYPE != PGM_TYPE_PAE
    X86PDPE         PdpeSrc;

    /* Fake PDPT entry; access control handled on the page table level, so allow everything. */
    PdpeSrc.u  = X86_PDPE_P;   /* rw/us are reserved for PAE pdpte's; accessed bit causes invalid VT-x guest state errors */
#   endif
    rc = pgmShwSyncPaePDPtr(pVCpu, GCPtrPage, &PdpeSrc, &pPDDst);
    if (rc != VINF_SUCCESS)
    {
        AssertRC(rc);
        return rc;
    }
    Assert(pPDDst);
    pPdeDst = &pPDDst->a[iPDDst];

# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    const unsigned  iPDDst = ((GCPtrPage >> SHW_PD_SHIFT) & SHW_PD_MASK);
    PX86PDPAE       pPDDst;
    PX86PDEPAE      pPdeDst;

#  if PGM_GST_TYPE == PGM_TYPE_PROT
    /* AMD-V nested paging */
    X86PML4E        Pml4eSrc;
    X86PDPE         PdpeSrc;
    PX86PML4E       pPml4eSrc = &Pml4eSrc;

    /* Fake PML4 & PDPT entry; access control handled on the page table level, so allow everything. */
    Pml4eSrc.u = X86_PML4E_P | X86_PML4E_RW | X86_PML4E_US | X86_PML4E_NX | X86_PML4E_A;
    PdpeSrc.u  = X86_PDPE_P | X86_PDPE_RW | X86_PDPE_US | X86_PDPE_NX | X86_PDPE_A;
#  endif

    rc = pgmShwSyncLongModePDPtr(pVCpu, GCPtrPage, pPml4eSrc, &PdpeSrc, &pPDDst);
    if (rc != VINF_SUCCESS)
    {
        AssertRC(rc);
        return rc;
    }
    Assert(pPDDst);
    pPdeDst = &pPDDst->a[iPDDst];
# endif

# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynLockHCPage(pVM, (uint8_t *)pPdeDst);
# endif

    if (!pPdeDst->n.u1Present)
    {
        rc = PGM_BTH_NAME(SyncPT)(pVCpu, iPDSrc, pPDSrc, GCPtrPage);
        if (rc != VINF_SUCCESS)
        {
# if defined(IN_RC)
            /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
            PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
            pgmUnlock(pVM);
            AssertRC(rc);
            return rc;
        }
    }

# if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
    /* Check for dirty bit fault */
    rc = PGM_BTH_NAME(CheckPageFault)(pVCpu, uErr, pPdeDst, &pPDSrc->a[iPDSrc], GCPtrPage);
    if (rc == VINF_PGM_HANDLED_DIRTY_BIT_FAULT)
        Log(("PGMVerifyAccess: success (dirty)\n"));
    else
    {
        GSTPDE PdeSrc = pPDSrc->a[iPDSrc];
# else
    {
        GSTPDE PdeSrc;
        PdeSrc.au32[0]      = 0; /* faked so we don't have to #ifdef everything */
        PdeSrc.n.u1Present  = 1;
        PdeSrc.n.u1Write    = 1;
        PdeSrc.n.u1Accessed = 1;
        PdeSrc.n.u1User     = 1;

# endif /* PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE) */
        Assert(rc != VINF_EM_RAW_GUEST_TRAP);
        if (uErr & X86_TRAP_PF_US)
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageOutOfSyncUser));
        else /* supervisor */
            STAM_COUNTER_INC(&pVCpu->pgm.s.CTX_MID_Z(Stat,PageOutOfSyncSupervisor));

        rc = PGM_BTH_NAME(SyncPage)(pVCpu, PdeSrc, GCPtrPage, 1, 0);
        if (RT_SUCCESS(rc))
        {
            /* Page was successfully synced */
            Log2(("PGMVerifyAccess: success (sync)\n"));
            rc = VINF_SUCCESS;
        }
        else
        {
            Log(("PGMVerifyAccess: access violation for %RGv rc=%d\n", GCPtrPage, rc));
            rc = VINF_EM_RAW_GUEST_TRAP;
        }
    }
# if defined(IN_RC)
    /* Make sure the dynamic pPdeDst mapping will not be reused during this function. */
    PGMDynUnlockHCPage(pVM, (uint8_t *)pPdeDst);
# endif
    pgmUnlock(pVM);
    return rc;

#else /* PGM_GST_TYPE != PGM_TYPE_32BIT */

    AssertReleaseMsgFailed(("Shw=%d Gst=%d is not implemented!\n", PGM_GST_TYPE, PGM_SHW_TYPE));
    return VERR_INTERNAL_ERROR;
#endif /* PGM_GST_TYPE != PGM_TYPE_32BIT */
}


#if PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_PAE || PGM_GST_TYPE == PGM_TYPE_AMD64
# if PGM_SHW_TYPE == PGM_TYPE_32BIT || PGM_SHW_TYPE == PGM_TYPE_PAE || PGM_SHW_TYPE == PGM_TYPE_AMD64
/**
 * Figures out which kind of shadow page this guest PDE warrants.
 *
 * @returns Shadow page kind.
 * @param   pPdeSrc     The guest PDE in question.
 * @param   cr4         The current guest cr4 value.
 */
DECLINLINE(PGMPOOLKIND) PGM_BTH_NAME(CalcPageKind)(const GSTPDE *pPdeSrc, uint32_t cr4)
{
#  if PMG_GST_TYPE == PGM_TYPE_AMD64
    if (!pPdeSrc->n.u1Size)
#  else
    if (!pPdeSrc->n.u1Size || !(cr4 & X86_CR4_PSE))
#  endif
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
 * @param   pVCpu       The VMCPU handle.
 * @param   cr0         Guest context CR0 register
 * @param   cr3         Guest context CR3 register
 * @param   cr4         Guest context CR4 register
 * @param   fGlobal     Including global page directories or not
 */
PGM_BTH_DECL(int, SyncCR3)(PVMCPU pVCpu, uint64_t cr0, uint64_t cr3, uint64_t cr4, bool fGlobal)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    if (VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3))
        fGlobal = true; /* Change this CR3 reload to be a global one. */

    LogFlow(("SyncCR3 %d\n", fGlobal));

#if PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT
    /*
     * Update page access handlers.
     * The virtual are always flushed, while the physical are only on demand.
     * WARNING: We are incorrectly not doing global flushing on Virtual Handler updates. We'll
     *          have to look into that later because it will have a bad influence on the performance.
     * @note SvL: There's no need for that. Just invalidate the virtual range(s).
     *      bird: Yes, but that won't work for aliases.
     */
    /** @todo this MUST go away. See #1557. */
    STAM_PROFILE_START(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncCR3Handlers), h);
    PGM_GST_NAME(HandlerVirtualUpdate)(pVM, cr4);
    STAM_PROFILE_STOP(&pVCpu->pgm.s.CTX_MID_Z(Stat,SyncCR3Handlers), h);
#endif

#if PGM_SHW_TYPE == PGM_TYPE_NESTED || PGM_SHW_TYPE == PGM_TYPE_EPT
    /*
     * Nested / EPT - almost no work.
     */
    /** @todo check if this is really necessary; the call does it as well... */
    HWACCMFlushTLB(pVM);
    return VINF_SUCCESS;

#elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    /*
     * AMD64 (Shw & Gst) - No need to check all paging levels; we zero
     * out the shadow parts when the guest modifies its tables.
     */
    return VINF_SUCCESS;

#else /* PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT && PGM_SHW_TYPE != PGM_TYPE_AMD64 */

#  ifdef PGM_WITHOUT_MAPPINGS
    Assert(pVM->pgm.s.fMappingsFixed);
    return VINF_SUCCESS;
#  else
    /* Nothing to do when mappings are fixed. */
    if (pVM->pgm.s.fMappingsFixed)
        return VINF_SUCCESS;

    int rc = PGMMapResolveConflicts(pVM);
    Assert(rc == VINF_SUCCESS || rc == VINF_PGM_SYNC_CR3);
    if (rc == VINF_PGM_SYNC_CR3)
    {
        LogFlow(("SyncCR3: detected conflict -> VINF_PGM_SYNC_CR3\n"));
        return VINF_PGM_SYNC_CR3;
    }
#  endif
    return VINF_SUCCESS;
#endif /* PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT && PGM_SHW_TYPE != PGM_TYPE_AMD64 */
}




#ifdef VBOX_STRICT
#ifdef IN_RC
# undef AssertMsgFailed
# define AssertMsgFailed Log
#endif
#ifdef IN_RING3
# include <VBox/dbgf.h>

/**
 * Dumps a page table hierarchy use only physical addresses and cr4/lm flags.
 *
 * @returns VBox status code (VINF_SUCCESS).
 * @param   cr3         The root of the hierarchy.
 * @param   crr         The cr4, only PAE and PSE is currently used.
 * @param   fLongMode   Set if long mode, false if not long mode.
 * @param   cMaxDepth   Number of levels to dump.
 * @param   pHlp        Pointer to the output functions.
 */
__BEGIN_DECLS
VMMR3DECL(int) PGMR3DumpHierarchyHC(PVM pVM, uint32_t cr3, uint32_t cr4, bool fLongMode, unsigned cMaxDepth, PCDBGFINFOHLP pHlp);
__END_DECLS

#endif

/**
 * Checks that the shadow page table is in sync with the guest one.
 *
 * @returns The number of errors.
 * @param   pVM         The virtual machine.
 * @param   pVCpu       The VMCPU handle.
 * @param   cr3         Guest context CR3 register
 * @param   cr4         Guest context CR4 register
 * @param   GCPtr       Where to start. Defaults to 0.
 * @param   cb          How much to check. Defaults to everything.
 */
PGM_BTH_DECL(unsigned, AssertCR3)(PVMCPU pVCpu, uint64_t cr3, uint64_t cr4, RTGCPTR GCPtr, RTGCPTR cb)
{
#if PGM_SHW_TYPE == PGM_TYPE_NESTED || PGM_SHW_TYPE == PGM_TYPE_EPT
    return 0;
#else
    unsigned cErrors = 0;
    PVM      pVM     = pVCpu->CTX_SUFF(pVM);
    PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

#if PGM_GST_TYPE == PGM_TYPE_PAE
    /** @todo currently broken; crashes below somewhere */
    AssertFailed();
#endif

#if    PGM_GST_TYPE == PGM_TYPE_32BIT \
    || PGM_GST_TYPE == PGM_TYPE_PAE \
    || PGM_GST_TYPE == PGM_TYPE_AMD64

# if PGM_GST_TYPE == PGM_TYPE_AMD64
    bool            fBigPagesSupported = true;
# else
    bool            fBigPagesSupported = !!(CPUMGetGuestCR4(pVCpu) & X86_CR4_PSE);
# endif
    PPGMCPU         pPGM = &pVCpu->pgm.s;
    RTGCPHYS        GCPhysGst;              /* page address derived from the guest page tables. */
    RTHCPHYS        HCPhysShw;              /* page address derived from the shadow page tables. */
# ifndef IN_RING0
    RTHCPHYS        HCPhys;                 /* general usage. */
# endif
    int             rc;

    /*
     * Check that the Guest CR3 and all its mappings are correct.
     */
    AssertMsgReturn(pPGM->GCPhysCR3 == (cr3 & GST_CR3_PAGE_MASK),
                    ("Invalid GCPhysCR3=%RGp cr3=%RGp\n", pPGM->GCPhysCR3, (RTGCPHYS)cr3),
                    false);
# if !defined(IN_RING0) && PGM_GST_TYPE != PGM_TYPE_AMD64
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    rc = PGMShwGetPage(pVCpu, (RTGCPTR)pPGM->pGst32BitPdRC, NULL, &HCPhysShw);
#  else
    rc = PGMShwGetPage(pVCpu, (RTGCPTR)pPGM->pGstPaePdptRC, NULL, &HCPhysShw);
#  endif
    AssertRCReturn(rc, 1);
    HCPhys = NIL_RTHCPHYS;
    rc = pgmRamGCPhys2HCPhys(&pVM->pgm.s, cr3 & GST_CR3_PAGE_MASK, &HCPhys);
    AssertMsgReturn(HCPhys == HCPhysShw, ("HCPhys=%RHp HCPhyswShw=%RHp (cr3)\n", HCPhys, HCPhysShw), false);
#  if PGM_GST_TYPE == PGM_TYPE_32BIT && defined(IN_RING3)
    pgmGstGet32bitPDPtr(pPGM);
    RTGCPHYS GCPhys;
    rc = PGMR3DbgR3Ptr2GCPhys(pVM, pPGM->pGst32BitPdR3, &GCPhys);
    AssertRCReturn(rc, 1);
    AssertMsgReturn((cr3 & GST_CR3_PAGE_MASK) == GCPhys, ("GCPhys=%RGp cr3=%RGp\n", GCPhys, (RTGCPHYS)cr3), false);
#  endif
# endif /* !IN_RING0 */

    /*
     * Get and check the Shadow CR3.
     */
# if PGM_SHW_TYPE == PGM_TYPE_32BIT
    unsigned        cPDEs       = X86_PG_ENTRIES;
    unsigned        cIncrement  = X86_PG_ENTRIES * PAGE_SIZE;
# elif PGM_SHW_TYPE == PGM_TYPE_PAE
#  if PGM_GST_TYPE == PGM_TYPE_32BIT
    unsigned        cPDEs       = X86_PG_PAE_ENTRIES * 4;   /* treat it as a 2048 entry table. */
#  else
    unsigned        cPDEs       = X86_PG_PAE_ENTRIES;
#  endif
    unsigned        cIncrement  = X86_PG_PAE_ENTRIES * PAGE_SIZE;
# elif PGM_SHW_TYPE == PGM_TYPE_AMD64
    unsigned        cPDEs       = X86_PG_PAE_ENTRIES;
    unsigned        cIncrement  = X86_PG_PAE_ENTRIES * PAGE_SIZE;
# endif
    if (cb != ~(RTGCPTR)0)
        cPDEs = RT_MIN(cb >> SHW_PD_SHIFT, 1);

/** @todo call the other two PGMAssert*() functions. */

# if PGM_GST_TYPE == PGM_TYPE_AMD64
    unsigned iPml4 = (GCPtr >> X86_PML4_SHIFT) & X86_PML4_MASK;

    for (; iPml4 < X86_PG_PAE_ENTRIES; iPml4++)
    {
        PPGMPOOLPAGE    pShwPdpt = NULL;
        PX86PML4E       pPml4eSrc;
        PX86PML4E       pPml4eDst;
        RTGCPHYS        GCPhysPdptSrc;

        pPml4eSrc     = pgmGstGetLongModePML4EPtr(&pVCpu->pgm.s, iPml4);
        pPml4eDst     = pgmShwGetLongModePML4EPtr(&pVCpu->pgm.s, iPml4);

        /* Fetch the pgm pool shadow descriptor if the shadow pml4e is present. */
        if (!pPml4eDst->n.u1Present)
        {
            GCPtr += _2M * UINT64_C(512) * UINT64_C(512);
            continue;
        }

        pShwPdpt = pgmPoolGetPage(pPool, pPml4eDst->u & X86_PML4E_PG_MASK);
        GCPhysPdptSrc = pPml4eSrc->u & X86_PML4E_PG_MASK_FULL;

        if (pPml4eSrc->n.u1Present != pPml4eDst->n.u1Present)
        {
            AssertMsgFailed(("Present bit doesn't match! pPml4eDst.u=%#RX64 pPml4eSrc.u=%RX64\n", pPml4eDst->u, pPml4eSrc->u));
            GCPtr += _2M * UINT64_C(512) * UINT64_C(512);
            cErrors++;
            continue;
        }

        if (GCPhysPdptSrc != pShwPdpt->GCPhys)
        {
            AssertMsgFailed(("Physical address doesn't match! iPml4 %d pPml4eDst.u=%#RX64 pPml4eSrc.u=%RX64 Phys %RX64 vs %RX64\n", iPml4, pPml4eDst->u, pPml4eSrc->u, pShwPdpt->GCPhys, GCPhysPdptSrc));
            GCPtr += _2M * UINT64_C(512) * UINT64_C(512);
            cErrors++;
            continue;
        }

        if (    pPml4eDst->n.u1User      != pPml4eSrc->n.u1User
            ||  pPml4eDst->n.u1Write     != pPml4eSrc->n.u1Write
            ||  pPml4eDst->n.u1NoExecute != pPml4eSrc->n.u1NoExecute)
        {
            AssertMsgFailed(("User/Write/NoExec bits don't match! pPml4eDst.u=%#RX64 pPml4eSrc.u=%RX64\n", pPml4eDst->u, pPml4eSrc->u));
            GCPtr += _2M * UINT64_C(512) * UINT64_C(512);
            cErrors++;
            continue;
        }
# else  /* PGM_GST_TYPE != PGM_TYPE_AMD64 */
    {
# endif /* PGM_GST_TYPE != PGM_TYPE_AMD64 */

# if PGM_GST_TYPE == PGM_TYPE_AMD64 || PGM_GST_TYPE == PGM_TYPE_PAE
        /*
         * Check the PDPTEs too.
         */
        unsigned iPdpt = (GCPtr >> SHW_PDPT_SHIFT) & SHW_PDPT_MASK;

        for (;iPdpt <= SHW_PDPT_MASK; iPdpt++)
        {
            unsigned        iPDSrc;
            PPGMPOOLPAGE    pShwPde = NULL;
            PX86PDPE        pPdpeDst;
            RTGCPHYS        GCPhysPdeSrc;
#  if PGM_GST_TYPE == PGM_TYPE_PAE
            X86PDPE         PdpeSrc;
            PGSTPD          pPDSrc    = pgmGstGetPaePDPtr(&pVCpu->pgm.s, GCPtr, &iPDSrc, &PdpeSrc);
            PX86PDPT        pPdptDst  = pgmShwGetPaePDPTPtr(&pVCpu->pgm.s);
#  else
            PX86PML4E       pPml4eSrc;
            X86PDPE         PdpeSrc;
            PX86PDPT        pPdptDst;
            PX86PDPAE       pPDDst;
            PGSTPD          pPDSrc    = pgmGstGetLongModePDPtr(&pVCpu->pgm.s, GCPtr, &pPml4eSrc, &PdpeSrc, &iPDSrc);

            rc = pgmShwGetLongModePDPtr(pVCpu, GCPtr, NULL, &pPdptDst, &pPDDst);
            if (rc != VINF_SUCCESS)
            {
                AssertMsg(rc == VERR_PAGE_DIRECTORY_PTR_NOT_PRESENT, ("Unexpected rc=%Rrc\n", rc));
                GCPtr += 512 * _2M;
                continue;   /* next PDPTE */
            }
            Assert(pPDDst);
#  endif
            Assert(iPDSrc == 0);

            pPdpeDst = &pPdptDst->a[iPdpt];

            if (!pPdpeDst->n.u1Present)
            {
                GCPtr += 512 * _2M;
                continue;   /* next PDPTE */
            }

            pShwPde      = pgmPoolGetPage(pPool, pPdpeDst->u & X86_PDPE_PG_MASK);
            GCPhysPdeSrc = PdpeSrc.u & X86_PDPE_PG_MASK;

            if (pPdpeDst->n.u1Present != PdpeSrc.n.u1Present)
            {
                AssertMsgFailed(("Present bit doesn't match! pPdpeDst.u=%#RX64 pPdpeSrc.u=%RX64\n", pPdpeDst->u, PdpeSrc.u));
                GCPtr += 512 * _2M;
                cErrors++;
                continue;
            }

            if (GCPhysPdeSrc != pShwPde->GCPhys)
            {
#  if PGM_GST_TYPE == PGM_TYPE_AMD64
                AssertMsgFailed(("Physical address doesn't match! iPml4 %d iPdpt %d pPdpeDst.u=%#RX64 pPdpeSrc.u=%RX64 Phys %RX64 vs %RX64\n", iPml4, iPdpt, pPdpeDst->u, PdpeSrc.u, pShwPde->GCPhys, GCPhysPdeSrc));
#  else
                AssertMsgFailed(("Physical address doesn't match! iPdpt %d pPdpeDst.u=%#RX64 pPdpeSrc.u=%RX64 Phys %RX64 vs %RX64\n", iPdpt, pPdpeDst->u, PdpeSrc.u, pShwPde->GCPhys, GCPhysPdeSrc));
#  endif
                GCPtr += 512 * _2M;
                cErrors++;
                continue;
            }

#  if PGM_GST_TYPE == PGM_TYPE_AMD64
            if (    pPdpeDst->lm.u1User      != PdpeSrc.lm.u1User
                ||  pPdpeDst->lm.u1Write     != PdpeSrc.lm.u1Write
                ||  pPdpeDst->lm.u1NoExecute != PdpeSrc.lm.u1NoExecute)
            {
                AssertMsgFailed(("User/Write/NoExec bits don't match! pPdpeDst.u=%#RX64 pPdpeSrc.u=%RX64\n", pPdpeDst->u, PdpeSrc.u));
                GCPtr += 512 * _2M;
                cErrors++;
                continue;
            }
#  endif

# else  /* PGM_GST_TYPE != PGM_TYPE_AMD64 && PGM_GST_TYPE != PGM_TYPE_PAE */
        {
# endif /* PGM_GST_TYPE != PGM_TYPE_AMD64 && PGM_GST_TYPE != PGM_TYPE_PAE */
# if PGM_GST_TYPE == PGM_TYPE_32BIT
            GSTPD const    *pPDSrc = pgmGstGet32bitPDPtr(&pVCpu->pgm.s);
#  if PGM_SHW_TYPE == PGM_TYPE_32BIT
            PCX86PD         pPDDst = pgmShwGet32BitPDPtr(&pVCpu->pgm.s);
#  endif
# endif /* PGM_GST_TYPE == PGM_TYPE_32BIT */
            /*
            * Iterate the shadow page directory.
            */
            GCPtr = (GCPtr >> SHW_PD_SHIFT) << SHW_PD_SHIFT;
            unsigned iPDDst = (GCPtr >> SHW_PD_SHIFT) & SHW_PD_MASK;

            for (;
                iPDDst < cPDEs;
                iPDDst++, GCPtr += cIncrement)
            {
# if PGM_SHW_TYPE == PGM_TYPE_PAE
                const SHWPDE PdeDst = *pgmShwGetPaePDEPtr(pPGM, GCPtr);
# else
                const SHWPDE PdeDst = pPDDst->a[iPDDst];
# endif
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
                    PPGMPOOLPAGE pPoolPage = pgmPoolGetPage(pPool, HCPhysShw);
                    if (!pPoolPage)
                    {
                        AssertMsgFailed(("Invalid page table address %RHp at %RGv! PdeDst=%#RX64\n",
                                        HCPhysShw, GCPtr, (uint64_t)PdeDst.u));
                        cErrors++;
                        continue;
                    }
                    const SHWPT *pPTDst = (const SHWPT *)PGMPOOL_PAGE_2_PTR(pVM, pPoolPage);

                    if (PdeDst.u & (X86_PDE4M_PWT | X86_PDE4M_PCD))
                    {
                        AssertMsgFailed(("PDE flags PWT and/or PCD is set at %RGv! These flags are not virtualized! PdeDst=%#RX64\n",
                                        GCPtr, (uint64_t)PdeDst.u));
                        cErrors++;
                    }

                    if (PdeDst.u & (X86_PDE4M_G | X86_PDE4M_D))
                    {
                        AssertMsgFailed(("4K PDE reserved flags at %RGv! PdeDst=%#RX64\n",
                                        GCPtr, (uint64_t)PdeDst.u));
                        cErrors++;
                    }

                    const GSTPDE PdeSrc = pPDSrc->a[(iPDDst >> (GST_PD_SHIFT - SHW_PD_SHIFT)) & GST_PD_MASK];
                    if (!PdeSrc.n.u1Present)
                    {
                        AssertMsgFailed(("Guest PDE at %RGv is not present! PdeDst=%#RX64 PdeSrc=%#RX64\n",
                                        GCPtr, (uint64_t)PdeDst.u, (uint64_t)PdeSrc.u));
                        cErrors++;
                        continue;
                    }

                    if (    !PdeSrc.b.u1Size
                        ||  !fBigPagesSupported)
                    {
                        GCPhysGst = PdeSrc.u & GST_PDE_PG_MASK;
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                        GCPhysGst |= (iPDDst & 1) * (PAGE_SIZE / 2);
# endif
                    }
                    else
                    {
# if PGM_GST_TYPE == PGM_TYPE_32BIT
                        if (PdeSrc.u & X86_PDE4M_PG_HIGH_MASK)
                        {
                            AssertMsgFailed(("Guest PDE at %RGv is using PSE36 or similar! PdeSrc=%#RX64\n",
                                            GCPtr, (uint64_t)PdeSrc.u));
                            cErrors++;
                            continue;
                        }
# endif
                        GCPhysGst = GST_GET_PDE_BIG_PG_GCPHYS(PdeSrc);
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                        GCPhysGst |= GCPtr & RT_BIT(X86_PAGE_2M_SHIFT);
# endif
                    }

                    if (    pPoolPage->enmKind
                        !=  (!PdeSrc.b.u1Size || !fBigPagesSupported ? BTH_PGMPOOLKIND_PT_FOR_PT : BTH_PGMPOOLKIND_PT_FOR_BIG))
                    {
                        AssertMsgFailed(("Invalid shadow page table kind %d at %RGv! PdeSrc=%#RX64\n",
                                        pPoolPage->enmKind, GCPtr, (uint64_t)PdeSrc.u));
                        cErrors++;
                    }

                    PPGMPAGE pPhysPage = pgmPhysGetPage(&pVM->pgm.s, GCPhysGst);
                    if (!pPhysPage)
                    {
                        AssertMsgFailed(("Cannot find guest physical address %RGp in the PDE at %RGv! PdeSrc=%#RX64\n",
                                        GCPhysGst, GCPtr, (uint64_t)PdeSrc.u));
                        cErrors++;
                        continue;
                    }

                    if (GCPhysGst != pPoolPage->GCPhys)
                    {
                        AssertMsgFailed(("GCPhysGst=%RGp != pPage->GCPhys=%RGp at %RGv\n",
                                        GCPhysGst, pPoolPage->GCPhys, GCPtr));
                        cErrors++;
                        continue;
                    }

                    if (    !PdeSrc.b.u1Size
                        ||  !fBigPagesSupported)
                    {
                        /*
                        * Page Table.
                        */
                        const GSTPT *pPTSrc;
                        rc = PGM_GCPHYS_2_PTR(pVM, GCPhysGst & ~(RTGCPHYS)(PAGE_SIZE - 1), &pPTSrc);
                        if (RT_FAILURE(rc))
                        {
                            AssertMsgFailed(("Cannot map/convert guest physical address %RGp in the PDE at %RGv! PdeSrc=%#RX64\n",
                                            GCPhysGst, GCPtr, (uint64_t)PdeSrc.u));
                            cErrors++;
                            continue;
                        }
                        if (    (PdeSrc.u & (X86_PDE_P | X86_PDE_US | X86_PDE_RW/* | X86_PDE_A*/))
                            !=  (PdeDst.u & (X86_PDE_P | X86_PDE_US | X86_PDE_RW/* | X86_PDE_A*/)))
                        {
                            /// @todo We get here a lot on out-of-sync CR3 entries. The access handler should zap them to avoid false alarms here!
                            // (This problem will go away when/if we shadow multiple CR3s.)
                            AssertMsgFailed(("4K PDE flags mismatch at %RGv! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                            GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                            cErrors++;
                            continue;
                        }
                        if (PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY)
                        {
                            AssertMsgFailed(("4K PDEs cannot have PGM_PDFLAGS_TRACK_DIRTY set! GCPtr=%RGv PdeDst=%#RX64\n",
                                            GCPtr, (uint64_t)PdeDst.u));
                            cErrors++;
                            continue;
                        }

                        /* iterate the page table. */
# if PGM_SHW_TYPE == PGM_TYPE_PAE && PGM_GST_TYPE == PGM_TYPE_32BIT
                        /* Select the right PDE as we're emulating a 4kb page table with 2 shadow page tables. */
                        const unsigned offPTSrc  = ((GCPtr >> SHW_PD_SHIFT) & 1) * 512;
# else
                        const unsigned offPTSrc  = 0;
# endif
                        for (unsigned iPT = 0, off = 0;
                            iPT < RT_ELEMENTS(pPTDst->a);
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
# ifdef IN_RING3
                                PGMAssertHandlerAndFlagsInSync(pVM);
                                PGMR3DumpHierarchyGC(pVM, cr3, cr4, (PdeSrc.u & GST_PDE_PG_MASK));
# endif
                                AssertMsgFailed(("Out of sync (!P) PTE at %RGv! PteSrc=%#RX64 PteDst=%#RX64 pPTSrc=%RGv iPTSrc=%x PdeSrc=%x physpte=%RGp\n",
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
                            if (RT_FAILURE(rc))
                            {
                                if (HCPhysShw != MMR3PageDummyHCPhys(pVM)) /** @todo this is wrong. */
                                {
                                    AssertMsgFailed(("Cannot find guest physical address %RGp at %RGv! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                    GCPhysGst, GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                    continue;
                                }
                            }
                            else if (HCPhysShw != (HCPhys & SHW_PTE_PG_MASK))
                            {
                                AssertMsgFailed(("Out of sync (phys) at %RGv! HCPhysShw=%RHp HCPhys=%RHp GCPhysGst=%RGp PteSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr + off, HCPhysShw, HCPhys, GCPhysGst, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
# endif

                            pPhysPage = pgmPhysGetPage(&pVM->pgm.s, GCPhysGst);
                            if (!pPhysPage)
                            {
# ifdef IN_RING3 /** @todo make MMR3PageDummyHCPhys an 'All' function! */
                                if (HCPhysShw != MMR3PageDummyHCPhys(pVM))  /** @todo this is wrong. */
                                {
                                    AssertMsgFailed(("Cannot find guest physical address %RGp at %RGv! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                    GCPhysGst, GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                    continue;
                                }
# endif
                                if (PteDst.n.u1Write)
                                {
                                    AssertMsgFailed(("Invalid guest page at %RGv is writable! GCPhysGst=%RGp PteSrc=%#RX64 PteDst=%#RX64\n",
                                                    GCPtr + off, GCPhysGst, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                }
                                fIgnoreFlags |= X86_PTE_RW;
                            }
                            else if (HCPhysShw != PGM_PAGE_GET_HCPHYS(pPhysPage))
                            {
                                AssertMsgFailed(("Out of sync (phys) at %RGv! HCPhysShw=%RHp pPhysPage:%R[pgmpage] GCPhysGst=%RGp PteSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr + off, HCPhysShw, pPhysPage, GCPhysGst, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }

                            /* flags */
                            if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPhysPage))
                            {
                                if (!PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPhysPage))
                                {
                                    if (PteDst.n.u1Write)
                                    {
                                        AssertMsgFailed(("WRITE access flagged at %RGv but the page is writable! pPhysPage=%R[pgmpage] PteSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, pPhysPage, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                        cErrors++;
                                        continue;
                                    }
                                    fIgnoreFlags |= X86_PTE_RW;
                                }
                                else
                                {
                                    if (PteDst.n.u1Present)
                                    {
                                        AssertMsgFailed(("ALL access flagged at %RGv but the page is present! pPhysPage=%R[pgmpage] PteSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, pPhysPage, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
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
                                        AssertMsgFailed(("!DIRTY page at %RGv is writable! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                        cErrors++;
                                        continue;
                                    }
                                    if (!(PteDst.u & PGM_PTFLAGS_TRACK_DIRTY))
                                    {
                                        AssertMsgFailed(("!DIRTY page at %RGv is not marked TRACK_DIRTY! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                        cErrors++;
                                        continue;
                                    }
                                    if (PteDst.n.u1Dirty)
                                    {
                                        AssertMsgFailed(("!DIRTY page at %RGv is marked DIRTY! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                        cErrors++;
                                    }
# if 0 /** @todo sync access bit properly... */
                                    if (PteDst.n.u1Accessed != PteSrc.n.u1Accessed)
                                    {
                                        AssertMsgFailed(("!DIRTY page at %RGv is has mismatching accessed bit! PteSrc=%#RX64 PteDst=%#RX64\n",
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
                                        AssertMsgFailed(("PGM_PTFLAGS_TRACK_DIRTY set at %RGv but no accessed bit emulation! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, (uint64_t)PteSrc.u, (uint64_t)PteDst.u));
                                        cErrors++;
                                        continue;
                                    }
                                    if (!PteDst.n.u1Accessed)
                                    {
                                        AssertMsgFailed(("!ACCESSED page at %RGv is has the accessed bit set! PteSrc=%#RX64 PteDst=%#RX64\n",
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
                                AssertMsgFailed(("Flags mismatch at %RGv! %#RX64 != %#RX64 fIgnoreFlags=%#RX64 PteSrc=%#RX64 PteDst=%#RX64\n",
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
                        uint64_t fIgnoreFlags = X86_PDE_AVL_MASK | GST_PDE_PG_MASK | X86_PDE4M_G | X86_PDE4M_D | X86_PDE4M_PS | X86_PDE4M_PWT | X86_PDE4M_PCD;
                        if (!PdeSrc.b.u1Dirty && PdeSrc.b.u1Write)
                        {
                            if (PdeDst.n.u1Write)
                            {
                                AssertMsgFailed(("!DIRTY page at %RGv is writable! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                                GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                                cErrors++;
                                continue;
                            }
                            if (!(PdeDst.u & PGM_PDFLAGS_TRACK_DIRTY))
                            {
                                AssertMsgFailed(("!DIRTY page at %RGv is not marked TRACK_DIRTY! PteSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                                cErrors++;
                                continue;
                            }
# if 0 /** @todo sync access bit properly... */
                            if (PdeDst.n.u1Accessed != PdeSrc.b.u1Accessed)
                            {
                                AssertMsgFailed(("!DIRTY page at %RGv is has mismatching accessed bit! PteSrc=%#RX64 PteDst=%#RX64\n",
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
                                AssertMsgFailed(("PGM_PDFLAGS_TRACK_DIRTY set at %RGv but no accessed bit emulation! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                                GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                                cErrors++;
                                continue;
                            }
                            if (!PdeDst.n.u1Accessed)
                            {
                                AssertMsgFailed(("!ACCESSED page at %RGv is has the accessed bit set! PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                                GCPtr, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                                cErrors++;
                            }
                            fIgnoreFlags |= X86_PTE_P;
                        }

                        if ((PdeSrc.u & ~fIgnoreFlags) != (PdeDst.u & ~fIgnoreFlags))
                        {
                            AssertMsgFailed(("Flags mismatch (B) at %RGv! %#RX64 != %#RX64 fIgnoreFlags=%#RX64 PdeSrc=%#RX64 PdeDst=%#RX64\n",
                                            GCPtr, (uint64_t)PdeSrc.u & ~fIgnoreFlags, (uint64_t)PdeDst.u & ~fIgnoreFlags,
                                            fIgnoreFlags, (uint64_t)PdeSrc.u, (uint64_t)PdeDst.u));
                            cErrors++;
                        }

                        /* iterate the page table. */
                        for (unsigned iPT = 0, off = 0;
                            iPT < RT_ELEMENTS(pPTDst->a);
                            iPT++, off += PAGE_SIZE, GCPhysGst += PAGE_SIZE)
                        {
                            const SHWPTE PteDst = pPTDst->a[iPT];

                            if (PteDst.u & PGM_PTFLAGS_TRACK_DIRTY)
                            {
                                AssertMsgFailed(("The PTE at %RGv emulating a 2/4M page is marked TRACK_DIRTY! PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr + off, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                            }

                            /* skip not-present entries. */
                            if (!PteDst.n.u1Present) /** @todo deal with ALL handlers and CSAM !P pages! */
                                continue;

                            fIgnoreFlags = X86_PTE_PAE_PG_MASK | X86_PTE_AVL_MASK | X86_PTE_PWT | X86_PTE_PCD | X86_PTE_PAT | X86_PTE_D | X86_PTE_A | X86_PTE_G | X86_PTE_PAE_NX;

                            /* match the physical addresses */
                            HCPhysShw = PteDst.u & X86_PTE_PAE_PG_MASK;

# ifdef IN_RING3
                            rc = PGMPhysGCPhys2HCPhys(pVM, GCPhysGst, &HCPhys);
                            if (RT_FAILURE(rc))
                            {
                                if (HCPhysShw != MMR3PageDummyHCPhys(pVM))  /** @todo this is wrong. */
                                {
                                    AssertMsgFailed(("Cannot find guest physical address %RGp at %RGv! PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                    GCPhysGst, GCPtr + off, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                }
                            }
                            else if (HCPhysShw != (HCPhys & X86_PTE_PAE_PG_MASK))
                            {
                                AssertMsgFailed(("Out of sync (phys) at %RGv! HCPhysShw=%RHp HCPhys=%RHp GCPhysGst=%RGp PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr + off, HCPhysShw, HCPhys, GCPhysGst, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
# endif
                            pPhysPage = pgmPhysGetPage(&pVM->pgm.s, GCPhysGst);
                            if (!pPhysPage)
                            {
# ifdef IN_RING3 /** @todo make MMR3PageDummyHCPhys an 'All' function! */
                                if (HCPhysShw != MMR3PageDummyHCPhys(pVM))  /** @todo this is wrong. */
                                {
                                    AssertMsgFailed(("Cannot find guest physical address %RGp at %RGv! PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                    GCPhysGst, GCPtr + off, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                    continue;
                                }
# endif
                                if (PteDst.n.u1Write)
                                {
                                    AssertMsgFailed(("Invalid guest page at %RGv is writable! GCPhysGst=%RGp PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                    GCPtr + off, GCPhysGst, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                    cErrors++;
                                }
                                fIgnoreFlags |= X86_PTE_RW;
                            }
                            else if (HCPhysShw != PGM_PAGE_GET_HCPHYS(pPhysPage))
                            {
                                AssertMsgFailed(("Out of sync (phys) at %RGv! HCPhysShw=%RHp pPhysPage=%R[pgmpage] GCPhysGst=%RGp PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr + off, HCPhysShw, pPhysPage, GCPhysGst, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }

                            /* flags */
                            if (PGM_PAGE_HAS_ACTIVE_HANDLERS(pPhysPage))
                            {
                                if (!PGM_PAGE_HAS_ACTIVE_ALL_HANDLERS(pPhysPage))
                                {
                                    if (PGM_PAGE_GET_HNDL_PHYS_STATE(pPhysPage) != PGM_PAGE_HNDL_PHYS_STATE_DISABLED)
                                    {
                                        if (PteDst.n.u1Write)
                                        {
                                            AssertMsgFailed(("WRITE access flagged at %RGv but the page is writable! pPhysPage=%R[pgmpage] PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                            GCPtr + off, pPhysPage, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
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
                                        AssertMsgFailed(("ALL access flagged at %RGv but the page is present! pPhysPage=%R[pgmpage] PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                        GCPtr + off, pPhysPage, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
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
                                AssertMsgFailed(("Flags mismatch (BT) at %RGv! %#RX64 != %#RX64 fIgnoreFlags=%#RX64 PdeSrc=%#RX64 PteDst=%#RX64\n",
                                                GCPtr + off, (uint64_t)PdeSrc.u & ~fIgnoreFlags, (uint64_t)PteDst.u & ~fIgnoreFlags,
                                                fIgnoreFlags, (uint64_t)PdeSrc.u, (uint64_t)PteDst.u));
                                cErrors++;
                                continue;
                            }
                        } /* for each PTE */
                    }
                }
                /* not present */

            } /* for each PDE */

        } /* for each PDPTE */

    } /* for each PML4E */

# ifdef DEBUG
    if (cErrors)
        LogFlow(("AssertCR3: cErrors=%d\n", cErrors));
# endif

#endif /* GST == 32BIT, PAE or AMD64 */
    return cErrors;

#endif /* PGM_SHW_TYPE != PGM_TYPE_NESTED && PGM_SHW_TYPE != PGM_TYPE_EPT */
}
#endif /* VBOX_STRICT */


/**
 * Sets up the CR3 for shadow paging
 *
 * @returns Strict VBox status code.
 * @retval  VINF_SUCCESS.
 *
 * @param   pVCpu       The VMCPU handle.
 * @param   GCPhysCR3       The physical address in the CR3 register.
 */
PGM_BTH_DECL(int, MapCR3)(PVMCPU pVCpu, RTGCPHYS GCPhysCR3)
{
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /* Update guest paging info. */
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

    LogFlow(("MapCR3: %RGp\n", GCPhysCR3));

    /*
     * Map the page CR3 points at.
     */
    RTHCPTR     HCPtrGuestCR3;
    RTHCPHYS    HCPhysGuestCR3;
    pgmLock(pVM);
    PPGMPAGE    pPage = pgmPhysGetPage(&pVM->pgm.s, GCPhysCR3);
    AssertReturn(pPage, VERR_INTERNAL_ERROR_2);
    HCPhysGuestCR3 = PGM_PAGE_GET_HCPHYS(pPage);
    /** @todo this needs some reworking wrt. locking.  */
# if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
    HCPtrGuestCR3 = NIL_RTHCPTR;
    int rc = VINF_SUCCESS;
# else
    int rc = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhysCR3 & GST_CR3_PAGE_MASK, (void **)&HCPtrGuestCR3);
# endif
    pgmUnlock(pVM);
    if (RT_SUCCESS(rc))
    {
        rc = PGMMap(pVM, (RTGCPTR)pVM->pgm.s.GCPtrCR3Mapping, HCPhysGuestCR3, PAGE_SIZE, 0);
        if (RT_SUCCESS(rc))
        {
# ifdef IN_RC
            PGM_INVL_PG(pVM->pgm.s.GCPtrCR3Mapping);
# endif
# if PGM_GST_TYPE == PGM_TYPE_32BIT
            pVCpu->pgm.s.pGst32BitPdR3 = (R3PTRTYPE(PX86PD))HCPtrGuestCR3;
#  ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
            pVCpu->pgm.s.pGst32BitPdR0 = (R0PTRTYPE(PX86PD))HCPtrGuestCR3;
#  endif
            pVCpu->pgm.s.pGst32BitPdRC = (RCPTRTYPE(PX86PD))pVM->pgm.s.GCPtrCR3Mapping;

# elif PGM_GST_TYPE == PGM_TYPE_PAE
            unsigned off = GCPhysCR3 & GST_CR3_PAGE_MASK & PAGE_OFFSET_MASK;
            pVCpu->pgm.s.pGstPaePdptR3 = (R3PTRTYPE(PX86PDPT))HCPtrGuestCR3;
#  ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
            pVCpu->pgm.s.pGstPaePdptR0 = (R0PTRTYPE(PX86PDPT))HCPtrGuestCR3;
#  endif
            pVCpu->pgm.s.pGstPaePdptRC = (RCPTRTYPE(PX86PDPT))((RCPTRTYPE(uint8_t *))pVM->pgm.s.GCPtrCR3Mapping + off);
            Log(("Cached mapping %RRv\n", pVCpu->pgm.s.pGstPaePdptRC));

            /*
             * Map the 4 PDs too.
             */
            PX86PDPT pGuestPDPT = pgmGstGetPaePDPTPtr(&pVCpu->pgm.s);
            RTGCPTR  GCPtr      = pVM->pgm.s.GCPtrCR3Mapping + PAGE_SIZE;
            for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++, GCPtr += PAGE_SIZE)
            {
                if (pGuestPDPT->a[i].n.u1Present)
                {
                    RTHCPTR     HCPtr;
                    RTHCPHYS    HCPhys;
                    RTGCPHYS    GCPhys = pGuestPDPT->a[i].u & X86_PDPE_PG_MASK;
                    pgmLock(pVM);
                    PPGMPAGE    pPage  = pgmPhysGetPage(&pVM->pgm.s, GCPhys);
                    AssertReturn(pPage, VERR_INTERNAL_ERROR_2);
                    HCPhys = PGM_PAGE_GET_HCPHYS(pPage);
#  if defined(IN_RC) || defined(VBOX_WITH_2X_4GB_ADDR_SPACE_IN_R0)
                    HCPtr = NIL_RTHCPTR;
                    int rc2 = VINF_SUCCESS;
#  else
                    int rc2 = pgmPhysGCPhys2CCPtrInternal(pVM, pPage, GCPhys, (void **)&HCPtr);
#  endif
                    pgmUnlock(pVM);
                    if (RT_SUCCESS(rc2))
                    {
                        rc = PGMMap(pVM, GCPtr, HCPhys, PAGE_SIZE, 0);
                        AssertRCReturn(rc, rc);

                        pVCpu->pgm.s.apGstPaePDsR3[i]     = (R3PTRTYPE(PX86PDPAE))HCPtr;
#  ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
                        pVCpu->pgm.s.apGstPaePDsR0[i]     = (R0PTRTYPE(PX86PDPAE))HCPtr;
#  endif
                        pVCpu->pgm.s.apGstPaePDsRC[i]     = (RCPTRTYPE(PX86PDPAE))GCPtr;
                        pVCpu->pgm.s.aGCPhysGstPaePDs[i]  = GCPhys;
                        PGM_INVL_PG(GCPtr); /** @todo This ends up calling HWACCMInvalidatePage, is that correct? */
                        continue;
                    }
                    AssertMsgFailed(("pgmR3Gst32BitMapCR3: rc2=%d GCPhys=%RGp i=%d\n", rc2, GCPhys, i));
                }

                pVCpu->pgm.s.apGstPaePDsR3[i]     = 0;
#  ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
                pVCpu->pgm.s.apGstPaePDsR0[i]     = 0;
#  endif
                pVCpu->pgm.s.apGstPaePDsRC[i]     = 0;
                pVCpu->pgm.s.aGCPhysGstPaePDs[i]  = NIL_RTGCPHYS;
                PGM_INVL_PG(GCPtr); /** @todo this shouldn't be necessary? */
            }

# elif PGM_GST_TYPE == PGM_TYPE_AMD64
            pVCpu->pgm.s.pGstAmd64Pml4R3 = (R3PTRTYPE(PX86PML4))HCPtrGuestCR3;
#  ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
            pVCpu->pgm.s.pGstAmd64Pml4R0 = (R0PTRTYPE(PX86PML4))HCPtrGuestCR3;
#  endif
# endif
        }
        else
            AssertMsgFailed(("rc=%Rrc GCPhysGuestPD=%RGp\n", rc, GCPhysCR3));
    }
    else
        AssertMsgFailed(("rc=%Rrc GCPhysGuestPD=%RGp\n", rc, GCPhysCR3));

#else /* prot/real stub */
    int rc = VINF_SUCCESS;
#endif

    /* Update shadow paging info for guest modes with paging (32, pae, 64). */
# if  (   (   PGM_SHW_TYPE == PGM_TYPE_32BIT \
           || PGM_SHW_TYPE == PGM_TYPE_PAE    \
           || PGM_SHW_TYPE == PGM_TYPE_AMD64) \
       && (   PGM_GST_TYPE != PGM_TYPE_REAL   \
           && PGM_GST_TYPE != PGM_TYPE_PROT))

    Assert(!HWACCMIsNestedPagingActive(pVM));

    /*
     * Update the shadow root page as well since that's not fixed.
     */
    PPGMPOOL     pPool             = pVM->pgm.s.CTX_SUFF(pPool);
    PPGMPOOLPAGE pOldShwPageCR3    = pVCpu->pgm.s.CTX_SUFF(pShwPageCR3);
    uint32_t     iOldShwUserTable  = pVCpu->pgm.s.iShwUserTable;
    uint32_t     iOldShwUser       = pVCpu->pgm.s.iShwUser;
    PPGMPOOLPAGE pNewShwPageCR3;

    Assert(!(GCPhysCR3 >> (PAGE_SHIFT + 32)));
    rc = pgmPoolAlloc(pVM, GCPhysCR3 & GST_CR3_PAGE_MASK, BTH_PGMPOOLKIND_ROOT, SHW_POOL_ROOT_IDX, GCPhysCR3 >> PAGE_SHIFT, &pNewShwPageCR3);
    AssertFatalRC(rc);
    rc = VINF_SUCCESS;

    /* Mark the page as locked; disallow flushing. */
    pgmPoolLockPage(pPool, pNewShwPageCR3);

#  ifdef IN_RC
    /* NOTE: We can't deal with jumps to ring 3 here as we're now in an inconsistent state! */
    bool fLog = VMMGCLogDisable(pVM);
    pgmLock(pVM);
#  endif

    pVCpu->pgm.s.iShwUser      = SHW_POOL_ROOT_IDX;
    pVCpu->pgm.s.iShwUserTable = GCPhysCR3 >> PAGE_SHIFT;
    pVCpu->pgm.s.CTX_SUFF(pShwPageCR3) = pNewShwPageCR3;
#  ifdef IN_RING0
    pVCpu->pgm.s.pShwPageCR3R3 = MMHyperCCToR3(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
    pVCpu->pgm.s.pShwPageCR3RC = MMHyperCCToRC(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
#  elif defined(IN_RC)
    pVCpu->pgm.s.pShwPageCR3R3 = MMHyperCCToR3(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
    pVCpu->pgm.s.pShwPageCR3R0 = MMHyperCCToR0(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
#  else
    pVCpu->pgm.s.pShwPageCR3R0 = MMHyperCCToR0(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
    pVCpu->pgm.s.pShwPageCR3RC = MMHyperCCToRC(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
#  endif

#  ifndef PGM_WITHOUT_MAPPINGS
    /*
     * Apply all hypervisor mappings to the new CR3.
     * Note that SyncCR3 will be executed in case CR3 is changed in a guest paging mode; this will
     * make sure we check for conflicts in the new CR3 root.
     */
#   if PGM_WITH_PAGING(PGM_GST_TYPE, PGM_SHW_TYPE)
    Assert(VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL) || VMCPU_FF_ISSET(pVCpu, VMCPU_FF_PGM_SYNC_CR3));
#   endif
    rc = pgmMapActivateCR3(pVM, pNewShwPageCR3);
    AssertRCReturn(rc, rc);
#  endif

    /* Set the current hypervisor CR3. */
    CPUMSetHyperCR3(pVCpu, PGMGetHyperCR3(pVCpu));
    SELMShadowCR3Changed(pVM, pVCpu);

#  ifdef IN_RC
    pgmUnlock(pVM);
    VMMGCLogRestore(pVM, fLog);
#  endif

    /* Clean up the old CR3 root. */
    if (pOldShwPageCR3)
    {
        Assert(pOldShwPageCR3->enmKind != PGMPOOLKIND_FREE);
#  ifndef PGM_WITHOUT_MAPPINGS
        /* Remove the hypervisor mappings from the shadow page table. */
        pgmMapDeactivateCR3(pVM, pOldShwPageCR3);
#  endif
        /* Mark the page as unlocked; allow flushing again. */
        pgmPoolUnlockPage(pPool, pOldShwPageCR3);

        pgmPoolFreeByPage(pPool, pOldShwPageCR3, iOldShwUser, iOldShwUserTable);
    }

# endif

    return rc;
}

/**
 * Unmaps the shadow CR3.
 *
 * @returns VBox status, no specials.
 * @param   pVCpu       The VMCPU handle.
 */
PGM_BTH_DECL(int, UnmapCR3)(PVMCPU pVCpu)
{
    LogFlow(("UnmapCR3\n"));

    int rc  = VINF_SUCCESS;
    PVM pVM = pVCpu->CTX_SUFF(pVM);

    /*
     * Update guest paging info.
     */
#if PGM_GST_TYPE == PGM_TYPE_32BIT
    pVCpu->pgm.s.pGst32BitPdR3 = 0;
# ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    pVCpu->pgm.s.pGst32BitPdR0 = 0;
# endif
    pVCpu->pgm.s.pGst32BitPdRC = 0;

#elif PGM_GST_TYPE == PGM_TYPE_PAE
    pVCpu->pgm.s.pGstPaePdptR3 = 0;
# ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    pVCpu->pgm.s.pGstPaePdptR0 = 0;
# endif
    pVCpu->pgm.s.pGstPaePdptRC = 0;
    for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
    {
        pVCpu->pgm.s.apGstPaePDsR3[i]    = 0;
# ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
        pVCpu->pgm.s.apGstPaePDsR0[i]    = 0;
# endif
        pVCpu->pgm.s.apGstPaePDsRC[i]    = 0;
        pVCpu->pgm.s.aGCPhysGstPaePDs[i] = NIL_RTGCPHYS;
    }

#elif PGM_GST_TYPE == PGM_TYPE_AMD64
    pVCpu->pgm.s.pGstAmd64Pml4R3 = 0;
# ifndef VBOX_WITH_2X_4GB_ADDR_SPACE
    pVCpu->pgm.s.pGstAmd64Pml4R0 = 0;
# endif

#else /* prot/real mode stub */
    /* nothing to do */
#endif

#if !defined(IN_RC) /* In RC we rely on MapCR3 to do the shadow part for us at a safe time */
    /*
     * Update shadow paging info.
     */
# if  (   (   PGM_SHW_TYPE == PGM_TYPE_32BIT  \
           || PGM_SHW_TYPE == PGM_TYPE_PAE    \
           || PGM_SHW_TYPE == PGM_TYPE_AMD64))

#  if PGM_GST_TYPE != PGM_TYPE_REAL
    Assert(!HWACCMIsNestedPagingActive(pVM));
#  endif

# ifndef PGM_WITHOUT_MAPPINGS
    if (pVCpu->pgm.s.CTX_SUFF(pShwPageCR3))
        /* Remove the hypervisor mappings from the shadow page table. */
        pgmMapDeactivateCR3(pVM, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));
# endif

    if (pVCpu->pgm.s.CTX_SUFF(pShwPageCR3))
    {
        PPGMPOOL pPool = pVM->pgm.s.CTX_SUFF(pPool);

        Assert(pVCpu->pgm.s.iShwUser != PGMPOOL_IDX_NESTED_ROOT);

        /* Mark the page as unlocked; allow flushing again. */
        pgmPoolUnlockPage(pPool, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3));

        pgmPoolFreeByPage(pPool, pVCpu->pgm.s.CTX_SUFF(pShwPageCR3), pVCpu->pgm.s.iShwUser, pVCpu->pgm.s.iShwUserTable);
        pVCpu->pgm.s.pShwPageCR3R3 = 0;
        pVCpu->pgm.s.pShwPageCR3R0 = 0;
        pVCpu->pgm.s.pShwPageCR3RC = 0;
        pVCpu->pgm.s.iShwUser      = 0;
        pVCpu->pgm.s.iShwUserTable = 0;
    }
# endif
#endif /* !IN_RC*/

    return rc;
}

