/* $Id$ */
/** @file
 * VBox - Page Manager, Guest Paging Template - All context code.
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
PGM_GST_DECL(int, GetPage)(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);
PGM_GST_DECL(int, ModifyPage)(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGM_GST_DECL(int, GetPDE)(PVM pVM, RTGCPTR GCPtr, PX86PDEPAE pPDE);
PGM_GST_DECL(bool, HandlerVirtualUpdate)(PVM pVM, uint32_t cr4);
__END_DECLS



/**
 * Gets effective Guest OS page information.
 *
 * When GCPtr is in a big page, the function will return as if it was a normal
 * 4KB page. If the need for distinguishing between big and normal page becomes
 * necessary at a later point, a PGMGstGetPage Ex() will be created for that
 * purpose.
 *
 * @returns VBox status.
 * @param   pVM         VM Handle.
 * @param   GCPtr       Guest Context virtual address of the page. Page aligned!
 * @param   pfFlags     Where to store the flags. These are X86_PTE_*, even for big pages.
 * @param   pGCPhys     Where to store the GC physical address of the page.
 *                      This is page aligned. The fact that the
 */
PGM_GST_DECL(int, GetPage)(PVM pVM, RTGCPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys)
{
#if PGM_GST_TYPE == PGM_TYPE_REAL \
 || PGM_GST_TYPE == PGM_TYPE_PROT
    /*
     * Fake it.
     */
    if (pfFlags)
        *pfFlags = X86_PTE_P | X86_PTE_RW | X86_PTE_US;
    if (pGCPhys)
        *pGCPhys = GCPtr & PAGE_BASE_GC_MASK;
    return VINF_SUCCESS;

#elif PGM_GST_TYPE == PGM_TYPE_32BIT || PGM_GST_TYPE == PGM_TYPE_PAE || PGM_GST_TYPE == PGM_TYPE_AMD64

    /*
     * Get the PDE.
     */
# if PGM_GST_TYPE == PGM_TYPE_32BIT
    X86PDE      Pde = pgmGstGet32bitPDE(&pVM->pgm.s, GCPtr);

#elif PGM_GST_TYPE == PGM_TYPE_PAE
    /* pgmGstGetPaePDE will return 0 if the PDPTE is marked as not present.
     * All the other bits in the PDPTE are only valid in long mode (r/w, u/s, nx). */
    X86PDEPAE   Pde = pgmGstGetPaePDE(&pVM->pgm.s, GCPtr);
    bool        fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVM) & MSR_K6_EFER_NXE);

#elif PGM_GST_TYPE == PGM_TYPE_AMD64
    PX86PML4E   pPml4e;
    X86PDPE     Pdpe;
    X86PDEPAE   Pde = pgmGstGetLongModePDEEx(&pVM->pgm.s, GCPtr, &pPml4e, &Pdpe);
    bool        fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVM) & MSR_K6_EFER_NXE);

    Assert(pPml4e);
    if (!(pPml4e->n.u1Present & Pdpe.n.u1Present))
        return VERR_PAGE_TABLE_NOT_PRESENT;

    /* Merge accessed, write, user and no-execute bits into the PDE. */
    Pde.n.u1Accessed  &= pPml4e->n.u1Accessed & Pdpe.lm.u1Accessed;
    Pde.n.u1Write     &= pPml4e->n.u1Write & Pdpe.lm.u1Write;
    Pde.n.u1User      &= pPml4e->n.u1User & Pdpe.lm.u1User;
    Pde.n.u1NoExecute &= pPml4e->n.u1NoExecute & Pdpe.lm.u1NoExecute;
# endif

    /*
     * Lookup the page.
     */
    if (!Pde.n.u1Present)
        return VERR_PAGE_TABLE_NOT_PRESENT;

    if (    !Pde.b.u1Size
# if PGM_GST_TYPE != PGM_TYPE_AMD64
        ||  !(CPUMGetGuestCR4(pVM) & X86_CR4_PSE)
# endif
        )
    {
        PGSTPT pPT;
        int rc = PGM_GCPHYS_2_PTR(pVM, Pde.u & GST_PDE_PG_MASK, &pPT);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Get PT entry and check presence.
         */
        const GSTPTE Pte = pPT->a[(GCPtr >> GST_PT_SHIFT) & GST_PT_MASK];
        if (!Pte.n.u1Present)
            return VERR_PAGE_NOT_PRESENT;

        /*
         * Store the result.
         * RW and US flags depend on all levels (bitwise AND) - except for legacy PAE
         * where the PDPE is simplified.
         */
        if (pfFlags)
        {
            *pfFlags = (Pte.u & ~GST_PTE_PG_MASK)
                     & ((Pde.u & (X86_PTE_RW | X86_PTE_US)) | ~(uint64_t)(X86_PTE_RW | X86_PTE_US));
# if PGM_WITH_NX(PGM_GST_TYPE, PGM_GST_TYPE)
            /* The NX bit is determined by a bitwise OR between the PT and PD */
            if (fNoExecuteBitValid)
                *pfFlags |= (Pte.u & Pde.u & X86_PTE_PAE_NX);
# endif
        }
        if (pGCPhys)
            *pGCPhys = Pte.u & GST_PTE_PG_MASK;
    }
    else
    {
        /*
         * Map big to 4k PTE and store the result
         */
        if (pfFlags)
        {
            *pfFlags = (Pde.u & ~(GST_PTE_PG_MASK | X86_PTE_PAT))
                     | ((Pde.u & X86_PDE4M_PAT) >> X86_PDE4M_PAT_SHIFT);
# if PGM_WITH_NX(PGM_GST_TYPE, PGM_GST_TYPE)
            /* The NX bit is determined by a bitwise OR between the PT and PD */
            if (fNoExecuteBitValid)
                *pfFlags |= (Pde.u & X86_PTE_PAE_NX);
# endif
        }
        if (pGCPhys)
            *pGCPhys = GST_GET_PDE_BIG_PG_GCPHYS(Pde) | (GCPtr & (~GST_PDE_BIG_PG_MASK ^ ~GST_PTE_PG_MASK));
    }
    return VINF_SUCCESS;
#else
# error "shouldn't be here!"
    /* something else... */
    return VERR_NOT_SUPPORTED;
#endif
}


/**
 * Modify page flags for a range of pages in the guest's tables
 *
 * The existing flags are ANDed with the fMask and ORed with the fFlags.
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   GCPtr       Virtual address of the first page in the range. Page aligned!
 * @param   cb          Size (in bytes) of the page range to apply the modification to. Page aligned!
 * @param   fFlags      The OR  mask - page flags X86_PTE_*, excluding the page mask of course.
 * @param   fMask       The AND mask - page flags X86_PTE_*.
 */
PGM_GST_DECL(int, ModifyPage)(PVM pVM, RTGCPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

    for (;;)
    {
        /*
         * Get the PD entry.
         */
# if PGM_GST_TYPE == PGM_TYPE_32BIT
        PX86PDE pPde = pgmGstGet32bitPDEPtr(&pVM->pgm.s, GCPtr);

# elif PGM_GST_TYPE == PGM_TYPE_PAE
        /* pgmGstGetPaePDEPtr will return 0 if the PDPTE is marked as not present
         * All the other bits in the PDPTE are only valid in long mode (r/w, u/s, nx)
         */
        PX86PDEPAE pPde = pgmGstGetPaePDEPtr(&pVM->pgm.s, GCPtr);
        Assert(pPde);
        if (!pPde)
            return VERR_PAGE_TABLE_NOT_PRESENT;
# elif PGM_GST_TYPE == PGM_TYPE_AMD64
        /** @todo Setting the r/w, u/s & nx bits might have no effect depending on the pdpte & pml4 values */
        PX86PDEPAE pPde = pgmGstGetLongModePDEPtr(&pVM->pgm.s, GCPtr);
        Assert(pPde);
        if (!pPde)
            return VERR_PAGE_TABLE_NOT_PRESENT;
# endif
        GSTPDE Pde = *pPde;
        Assert(Pde.n.u1Present);
        if (!Pde.n.u1Present)
            return VERR_PAGE_TABLE_NOT_PRESENT;

        if (    !Pde.b.u1Size
# if PGM_GST_TYPE != PGM_TYPE_AMD64
            ||  !(CPUMGetGuestCR4(pVM) & X86_CR4_PSE)
# endif
            )
        {
            /*
             * 4KB Page table
             *
             * Walk page tables and pages till we're done.
             */
            PGSTPT pPT;
            int rc = PGM_GCPHYS_2_PTR(pVM, Pde.u & GST_PDE_PG_MASK, &pPT);
            if (RT_FAILURE(rc))
                return rc;

            unsigned iPTE = (GCPtr >> GST_PT_SHIFT) & GST_PT_MASK;
            while (iPTE < RT_ELEMENTS(pPT->a))
            {
                GSTPTE Pte = pPT->a[iPTE];
                Pte.u = (Pte.u & (fMask | X86_PTE_PAE_PG_MASK))
                      | (fFlags & ~GST_PTE_PG_MASK);
                pPT->a[iPTE] = Pte;

                /* next page */
                cb -= PAGE_SIZE;
                if (!cb)
                    return VINF_SUCCESS;
                GCPtr += PAGE_SIZE;
                iPTE++;
            }
        }
        else
        {
            /*
             * 4MB Page table
             */
# if PGM_GST_TYPE == PGM_TYPE_32BIT
            Pde.u = (Pde.u & (fMask | ((fMask & X86_PTE_PAT) << X86_PDE4M_PAT_SHIFT) | GST_PDE_BIG_PG_MASK | X86_PDE4M_PG_HIGH_MASK | X86_PDE4M_PS))
# else
            Pde.u = (Pde.u & (fMask | ((fMask & X86_PTE_PAT) << X86_PDE4M_PAT_SHIFT) | GST_PDE_BIG_PG_MASK | X86_PDE4M_PS))
# endif
                  | (fFlags & ~GST_PTE_PG_MASK)
                  | ((fFlags & X86_PTE_PAT) << X86_PDE4M_PAT_SHIFT);
            *pPde = Pde;

            /* advance */
            const unsigned cbDone = GST_BIG_PAGE_SIZE - (GCPtr & GST_BIG_PAGE_OFFSET_MASK);
            if (cbDone >= cb)
                return VINF_SUCCESS;
            cb    -= cbDone;
            GCPtr += cbDone;
        }
    }

#else
    /* real / protected mode: ignore. */
    return VINF_SUCCESS;
#endif
}


/**
 * Retrieve guest PDE information
 *
 * @returns VBox status code.
 * @param   pVM         The virtual machine.
 * @param   GCPtr       Guest context pointer
 * @param   pPDE        Pointer to guest PDE structure
 */
PGM_GST_DECL(int, GetPDE)(PVM pVM, RTGCPTR GCPtr, PX86PDEPAE pPDE)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE   \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

# if PGM_GST_TYPE == PGM_TYPE_32BIT
    X86PDE    Pde = pgmGstGet32bitPDE(&pVM->pgm.s, GCPtr);
# elif PGM_GST_TYPE == PGM_TYPE_PAE
    X86PDEPAE Pde = pgmGstGetPaePDE(&pVM->pgm.s, GCPtr);
# elif PGM_GST_TYPE == PGM_TYPE_AMD64
    X86PDEPAE Pde = pgmGstGetLongModePDE(&pVM->pgm.s, GCPtr);
# endif

    pPDE->u = (X86PGPAEUINT)Pde.u;
    return VINF_SUCCESS;
#else
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
#endif
}


#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE \
 || PGM_GST_TYPE == PGM_TYPE_AMD64
/**
 * Updates one virtual handler range.
 *
 * @returns 0
 * @param   pNode   Pointer to a PGMVIRTHANDLER.
 * @param   pvUser  Pointer to a PGMVHUARGS structure (see PGM.cpp).
 */
static DECLCALLBACK(int) PGM_GST_NAME(VirtHandlerUpdateOne)(PAVLROGCPTRNODECORE pNode, void *pvUser)
{
    PPGMVIRTHANDLER pCur  = (PPGMVIRTHANDLER)pNode;
    PPGMHVUSTATE    pState = (PPGMHVUSTATE)pvUser;
    Assert(pCur->enmType != PGMVIRTHANDLERTYPE_HYPERVISOR);

#if PGM_GST_TYPE == PGM_TYPE_32BIT
    PX86PD          pPDSrc = pgmGstGet32bitPDPtr(&pState->pVM->pgm.s);
#endif

    RTGCPTR         GCPtr = pCur->Core.Key;
#if PGM_GST_MODE != PGM_MODE_AMD64
    /* skip all stuff above 4GB if not AMD64 mode. */
    if (GCPtr >= _4GB)
        return 0;
#endif

    unsigned        offPage = GCPtr & PAGE_OFFSET_MASK;
    unsigned        iPage = 0;
    while (iPage < pCur->cPages)
    {
#if PGM_GST_TYPE == PGM_TYPE_32BIT
        X86PDE      Pde = pPDSrc->a[GCPtr >> X86_PD_SHIFT];
#elif PGM_GST_TYPE == PGM_TYPE_PAE
        X86PDEPAE   Pde = pgmGstGetPaePDE(&pState->pVM->pgm.s, GCPtr);
#elif PGM_GST_TYPE == PGM_TYPE_AMD64
        X86PDEPAE   Pde = pgmGstGetLongModePDE(&pState->pVM->pgm.s, GCPtr);
#endif
        if (Pde.n.u1Present)
        {
            if (    !Pde.b.u1Size
# if PGM_GST_TYPE != PGM_TYPE_AMD64
                ||  !(pState->cr4 & X86_CR4_PSE)
# endif
                )
            {
                /*
                 * Normal page table.
                 */
                PGSTPT pPT;
                int rc = PGM_GCPHYS_2_PTR(pState->pVM, Pde.u & GST_PDE_PG_MASK, &pPT);
                if (RT_SUCCESS(rc))
                {
                    for (unsigned iPTE = (GCPtr >> GST_PT_SHIFT) & GST_PT_MASK;
                         iPTE < RT_ELEMENTS(pPT->a) && iPage < pCur->cPages;
                         iPTE++, iPage++, GCPtr += PAGE_SIZE, offPage = 0)
                    {
                        GSTPTE      Pte = pPT->a[iPTE];
                        RTGCPHYS    GCPhysNew;
                        if (Pte.n.u1Present)
                            GCPhysNew = (RTGCPHYS)(pPT->a[iPTE].u & GST_PTE_PG_MASK) + offPage;
                        else
                            GCPhysNew = NIL_RTGCPHYS;
                        if (pCur->aPhysToVirt[iPage].Core.Key != GCPhysNew)
                        {
                            if (pCur->aPhysToVirt[iPage].Core.Key != NIL_RTGCPHYS)
                                pgmHandlerVirtualClearPage(&pState->pVM->pgm.s, pCur, iPage);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                            AssertReleaseMsg(!pCur->aPhysToVirt[iPage].offNextAlias,
                                             ("{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} GCPhysNew=%RGp\n",
                                              pCur->aPhysToVirt[iPage].Core.Key, pCur->aPhysToVirt[iPage].Core.KeyLast,
                                              pCur->aPhysToVirt[iPage].offVirtHandler, pCur->aPhysToVirt[iPage].offNextAlias, GCPhysNew));
#endif
                            pCur->aPhysToVirt[iPage].Core.Key = GCPhysNew;
                            pState->fTodo |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
                        }
                    }
                }
                else
                {
                    /* not-present. */
                    offPage = 0;
                    AssertRC(rc);
                    for (unsigned iPTE = (GCPtr >> GST_PT_SHIFT) & GST_PT_MASK;
                         iPTE < RT_ELEMENTS(pPT->a) && iPage < pCur->cPages;
                         iPTE++, iPage++, GCPtr += PAGE_SIZE)
                    {
                        if (pCur->aPhysToVirt[iPage].Core.Key != NIL_RTGCPHYS)
                        {
                            pgmHandlerVirtualClearPage(&pState->pVM->pgm.s, pCur, iPage);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                            AssertReleaseMsg(!pCur->aPhysToVirt[iPage].offNextAlias,
                                             ("{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
                                              pCur->aPhysToVirt[iPage].Core.Key, pCur->aPhysToVirt[iPage].Core.KeyLast,
                                              pCur->aPhysToVirt[iPage].offVirtHandler, pCur->aPhysToVirt[iPage].offNextAlias));
#endif
                            pCur->aPhysToVirt[iPage].Core.Key = NIL_RTGCPHYS;
                            pState->fTodo |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
                        }
                    }
                }
            }
            else
            {
                /*
                 * 2/4MB page.
                 */
                RTGCPHYS GCPhys = (RTGCPHYS)(Pde.u & GST_PDE_PG_MASK);
                for (unsigned i4KB = (GCPtr >> GST_PT_SHIFT) & GST_PT_MASK;
                     i4KB < PAGE_SIZE / sizeof(GSTPDE) && iPage < pCur->cPages;
                     i4KB++, iPage++, GCPtr += PAGE_SIZE, offPage = 0)
                {
                    RTGCPHYS GCPhysNew = GCPhys + (i4KB << PAGE_SHIFT) + offPage;
                    if (pCur->aPhysToVirt[iPage].Core.Key != GCPhysNew)
                    {
                        if (pCur->aPhysToVirt[iPage].Core.Key != NIL_RTGCPHYS)
                            pgmHandlerVirtualClearPage(&pState->pVM->pgm.s, pCur, iPage);
#ifdef VBOX_STRICT_PGM_HANDLER_VIRTUAL
                        AssertReleaseMsg(!pCur->aPhysToVirt[iPage].offNextAlias,
                                         ("{.Core.Key=%RGp, .Core.KeyLast=%RGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} GCPhysNew=%RGp\n",
                                          pCur->aPhysToVirt[iPage].Core.Key, pCur->aPhysToVirt[iPage].Core.KeyLast,
                                          pCur->aPhysToVirt[iPage].offVirtHandler, pCur->aPhysToVirt[iPage].offNextAlias, GCPhysNew));
#endif
                        pCur->aPhysToVirt[iPage].Core.Key = GCPhysNew;
                        pState->fTodo |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
                    }
                }
            } /* pde type */
        }
        else
        {
            /* not-present. */
            for (unsigned cPages = (GST_PT_MASK + 1) - ((GCPtr >> GST_PT_SHIFT) & GST_PT_MASK);
                 cPages && iPage < pCur->cPages;
                 iPage++, GCPtr += PAGE_SIZE)
            {
                if (pCur->aPhysToVirt[iPage].Core.Key != NIL_RTGCPHYS)
                {
                    pgmHandlerVirtualClearPage(&pState->pVM->pgm.s, pCur, iPage);
                    pCur->aPhysToVirt[iPage].Core.Key = NIL_RTGCPHYS;
                    pState->fTodo |= PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
                }
            }
            offPage = 0;
        }
    } /* for pages in virtual mapping. */

    return 0;
}
#endif /* 32BIT, PAE and AMD64 */


/**
 * Updates the virtual page access handlers.
 *
 * @returns true if bits were flushed.
 * @returns false if bits weren't flushed.
 * @param   pVM     VM handle.
 * @param   pPDSrc  The page directory.
 * @param   cr4     The cr4 register value.
 */
PGM_GST_DECL(bool, HandlerVirtualUpdate)(PVM pVM, uint32_t cr4)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

    /** @todo
     * In theory this is not sufficient: the guest can change a single page in a range with invlpg
     */

    /*
     * Resolve any virtual address based access handlers to GC physical addresses.
     * This should be fairly quick.
     */
    PGMHVUSTATE State;

    pgmLock(pVM);
    STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3HandlerVirtualUpdate), a);
    State.pVM   = pVM;
    State.fTodo = pVM->pgm.s.fSyncFlags;
    State.cr4   = cr4;
    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, true, PGM_GST_NAME(VirtHandlerUpdateOne), &State);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3HandlerVirtualUpdate), a);


    /*
     * Set / reset bits?
     */
    if (State.fTodo & PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL)
    {
        STAM_PROFILE_START(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3HandlerVirtualReset), b);
        Log(("pgmR3VirtualHandlersUpdate: resets bits\n"));
        RTAvlroGCPtrDoWithAll(&pVM->pgm.s.CTX_SUFF(pTrees)->VirtHandlers, true, pgmHandlerVirtualResetOne, pVM);
        pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
        STAM_PROFILE_STOP(&pVM->pgm.s.CTX_MID_Z(Stat,SyncCR3HandlerVirtualReset), b);
    }
    pgmUnlock(pVM);

    return !!(State.fTodo & PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL);

#else /* real / protected */
    return false;
#endif
}

