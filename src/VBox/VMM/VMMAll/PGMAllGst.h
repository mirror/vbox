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
*   Defined Constants And Macros                                               *
*******************************************************************************/
#undef GSTPT
#undef PGSTPT
#undef GSTPTE
#undef PGSTPTE
#undef GSTPD
#undef PGSTPD
#undef GSTPDE
#undef PGSTPDE
#undef GST_BIG_PAGE_SIZE
#undef GST_BIG_PAGE_OFFSET_MASK
#undef GST_PDE_PG_MASK
#undef GST_PDE_BIG_PG_MASK
#undef GST_PD_SHIFT
#undef GST_PD_MASK
#undef GST_PTE_PG_MASK
#undef GST_PT_SHIFT
#undef GST_PT_MASK
#undef GST_TOTAL_PD_ENTRIES
#undef GST_CR3_PAGE_MASK
#undef GST_PDPE_ENTRIES
#undef GST_PDPT_SHIFT
#undef GST_PDPT_MASK

#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_REAL \
 || PGM_GST_TYPE == PGM_TYPE_PROT
# define GSTPT                      X86PT
# define PGSTPT                     PX86PT
# define GSTPTE                     X86PTE
# define PGSTPTE                    PX86PTE
# define GSTPD                      X86PD
# define PGSTPD                     PX86PD
# define GSTPDE                     X86PDE
# define PGSTPDE                    PX86PDE
# define GST_BIG_PAGE_SIZE          X86_PAGE_4M_SIZE
# define GST_BIG_PAGE_OFFSET_MASK   X86_PAGE_4M_OFFSET_MASK
# define GST_PDE_PG_MASK            X86_PDE_PG_MASK
# define GST_PDE_BIG_PG_MASK        X86_PDE4M_PG_MASK
# define GST_PD_SHIFT               X86_PD_SHIFT
# define GST_PD_MASK                X86_PD_MASK
# define GST_TOTAL_PD_ENTRIES       X86_PG_ENTRIES
# define GST_PTE_PG_MASK            X86_PTE_PG_MASK
# define GST_PT_SHIFT               X86_PT_SHIFT
# define GST_PT_MASK                X86_PT_MASK
# define GST_CR3_PAGE_MASK          X86_CR3_PAGE_MASK
#elif   PGM_GST_TYPE == PGM_TYPE_PAE \
     || PGM_GST_TYPE == PGM_TYPE_AMD64
# define GSTPT                      X86PTPAE
# define PGSTPT                     PX86PTPAE
# define GSTPTE                     X86PTEPAE
# define PGSTPTE                    PX86PTEPAE
# define GSTPD                      X86PDPAE
# define PGSTPD                     PX86PDPAE
# define GSTPDE                     X86PDEPAE
# define PGSTPDE                    PX86PDEPAE
# define GST_BIG_PAGE_SIZE          X86_PAGE_2M_SIZE
# define GST_BIG_PAGE_OFFSET_MASK   X86_PAGE_2M_OFFSET_MASK
# define GST_PDE_PG_MASK            X86_PDE_PAE_PG_MASK
# define GST_PDE_BIG_PG_MASK        X86_PDE2M_PAE_PG_MASK
# define GST_PD_SHIFT               X86_PD_PAE_SHIFT
# define GST_PD_MASK                X86_PD_PAE_MASK
# if PGM_GST_TYPE == PGM_TYPE_PAE
#  define GST_TOTAL_PD_ENTRIES      (X86_PG_PAE_ENTRIES * X86_PG_PAE_PDPE_ENTRIES)
#  define GST_PDPE_ENTRIES          X86_PG_PAE_PDPE_ENTRIES
#  define GST_PDPT_SHIFT            X86_PDPT_SHIFT
#  define GST_PDPT_MASK             X86_PDPT_MASK_PAE
# else
#  define GST_TOTAL_PD_ENTRIES      (X86_PG_AMD64_ENTRIES * X86_PG_AMD64_PDPE_ENTRIES)
#  define GST_PDPE_ENTRIES          X86_PG_AMD64_PDPE_ENTRIES
#  define GST_PDPT_SHIFT            X86_PDPT_SHIFT
#  define GST_PDPT_MASK             X86_PDPT_MASK_AMD64
# endif
# define GST_PTE_PG_MASK            X86_PTE_PAE_PG_MASK
# define GST_PT_SHIFT               X86_PT_PAE_SHIFT
# define GST_PT_MASK                X86_PT_PAE_MASK
# define GST_CR3_PAGE_MASK          X86_CR3_PAE_PAGE_MASK
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
PGM_GST_DECL(int, GetPage)(PVM pVM, RTGCUINTPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys);
PGM_GST_DECL(int, ModifyPage)(PVM pVM, RTGCUINTPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask);
PGM_GST_DECL(int, GetPDE)(PVM pVM, RTGCUINTPTR GCPtr, PX86PDEPAE pPDE);
PGM_GST_DECL(int, MapCR3)(PVM pVM, RTGCPHYS GCPhysCR3);
PGM_GST_DECL(int, UnmapCR3)(PVM pVM);
PGM_GST_DECL(int, MonitorCR3)(PVM pVM, RTGCPHYS GCPhysCR3);
PGM_GST_DECL(int, UnmonitorCR3)(PVM pVM);
PGM_GST_DECL(bool, HandlerVirtualUpdate)(PVM pVM, uint32_t cr4);
#ifndef IN_RING3
PGM_GST_DECL(int, WriteHandlerCR3)(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
# if PGM_GST_TYPE == PGM_TYPE_PAE \
  || PGM_GST_TYPE == PGM_TYPE_AMD64
PGM_GST_DECL(int, PAEWriteHandlerPD)(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser);
# endif
#endif
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
PGM_GST_DECL(int, GetPage)(PVM pVM, RTGCUINTPTR GCPtr, uint64_t *pfFlags, PRTGCPHYS pGCPhys)
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
    const X86PDE Pde = CTXSUFF(pVM->pgm.s.pGuestPD)->a[GCPtr >> X86_PD_SHIFT];
#elif PGM_GST_TYPE == PGM_TYPE_PAE
    X86PDEPAE    Pde;
    bool         fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVM) & MSR_K6_EFER_NXE);

    /* pgmGstGetPaePDE will return 0 if the PDPTE is marked as not present
     * All the other bits in the PDPTE are only valid in long mode (r/w, u/s, nx)
     */
    Pde.u = pgmGstGetPaePDE(&pVM->pgm.s, GCPtr);
#elif PGM_GST_TYPE == PGM_TYPE_AMD64
    PX86PML4E    pPml4e;
    X86PDPE      Pdpe;
    X86PDEPAE    Pde;
    bool         fNoExecuteBitValid = !!(CPUMGetGuestEFER(pVM) & MSR_K6_EFER_NXE);

    Pde.u = pgmGstGetLongModePDE(&pVM->pgm.s, GCPtr, &pPml4e, &Pdpe);
    Assert(pPml4e);
    if (!(pPml4e->n.u1Present & Pdpe.n.u1Present))
        return VERR_PAGE_TABLE_NOT_PRESENT;

    /* Merge accessed, write, user and no-execute bits into the PDE. */
    Pde.n.u1Accessed  &= pPml4e->n.u1Accessed & Pdpe.n.u1Accessed;
    Pde.n.u1Write     &= pPml4e->n.u1Write & Pdpe.n.u1Write;
    Pde.n.u1User      &= pPml4e->n.u1User & Pdpe.n.u1User;
    Pde.n.u1NoExecute &= pPml4e->n.u1NoExecute& Pdpe.n.u1NoExecute;
# endif

    /*
     * Lookup the page.
     */
    if (!Pde.n.u1Present)
        return VERR_PAGE_TABLE_NOT_PRESENT;

    if (    !Pde.b.u1Size
        ||  !(CPUMGetGuestCR4(pVM) & X86_CR4_PSE))
    {
        PGSTPT pPT;
        int rc = PGM_GCPHYS_2_PTR(pVM, Pde.u & GST_PDE_PG_MASK, &pPT);
        if (VBOX_FAILURE(rc))
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
# if PGM_WITH_NX(PGM_GST_TYPE)
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
# if PGM_WITH_NX(PGM_GST_TYPE)
            /* The NX bit is determined by a bitwise OR between the PT and PD */
            if (fNoExecuteBitValid)
                *pfFlags |= (Pde.u & X86_PTE_PAE_NX);
# endif
        }
        if (pGCPhys)
            *pGCPhys = (Pde.u & GST_PDE_BIG_PG_MASK) | (GCPtr & (~GST_PDE_BIG_PG_MASK ^ ~GST_PTE_PG_MASK)); /** @todo pse36 */
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
PGM_GST_DECL(int, ModifyPage)(PVM pVM, RTGCUINTPTR GCPtr, size_t cb, uint64_t fFlags, uint64_t fMask)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

    for (;;)
    {
        /*
         * Get the PD entry.
         */
#if PGM_GST_TYPE == PGM_TYPE_32BIT
        PX86PDE pPde = &CTXSUFF(pVM->pgm.s.pGuestPD)->a[GCPtr >> X86_PD_SHIFT];
#elif PGM_GST_TYPE == PGM_TYPE_PAE
        /* pgmGstGetPaePDEPtr will return 0 if the PDPTE is marked as not present
         * All the other bits in the PDPTE are only valid in long mode (r/w, u/s, nx)
         */
        PX86PDEPAE pPde = pgmGstGetPaePDEPtr(&pVM->pgm.s, GCPtr);
        Assert(pPde);
        if (!pPde)
            return VERR_PAGE_TABLE_NOT_PRESENT;
#elif PGM_GST_TYPE == PGM_TYPE_AMD64
        /** @todo Setting the r/w, u/s & nx bits might have no effect depending on the pdpte & pml4 values */
        PX86PDEPAE pPde = pgmGstGetLongModePDEPtr(&pVM->pgm.s, GCPtr);
        Assert(pPde);
        if (!pPde)
            return VERR_PAGE_TABLE_NOT_PRESENT;
#endif
        GSTPDE Pde = *pPde;
        Assert(Pde.n.u1Present);
        if (!Pde.n.u1Present)
            return VERR_PAGE_TABLE_NOT_PRESENT;

        if (    !Pde.b.u1Size
            ||  !(CPUMGetGuestCR4(pVM) & X86_CR4_PSE))
        {
            /*
             * 4KB Page table
             *
             * Walk page tables and pages till we're done.
             */
            PGSTPT pPT;
            int rc = PGM_GCPHYS_2_PTR(pVM, Pde.u & GST_PDE_PG_MASK, &pPT);
            if (VBOX_FAILURE(rc))
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
            Pde.u = (Pde.u & (fMask | ((fMask & X86_PTE_PAT) << X86_PDE4M_PAT_SHIFT) | GST_PDE_BIG_PG_MASK | X86_PDE4M_PS)) /** @todo pse36 */
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
PGM_GST_DECL(int, GetPDE)(PVM pVM, RTGCUINTPTR GCPtr, PX86PDEPAE pPDE)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE   \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

# if PGM_GST_TYPE == PGM_TYPE_32BIT
    X86PDE    Pde;
    Pde   = CTXSUFF(pVM->pgm.s.pGuestPD)->a[GCPtr >> GST_PD_SHIFT];
# elif PGM_GST_TYPE == PGM_TYPE_PAE
    X86PDEPAE Pde;
    Pde.u = pgmGstGetPaePDE(&pVM->pgm.s, GCPtr);
# elif PGM_GST_TYPE == PGM_TYPE_AMD64
    X86PDEPAE Pde;
    Pde.u = pgmGstGetLongModePDE(&pVM->pgm.s, GCPtr);
# endif

    pPDE->u = (X86PGPAEUINT)Pde.u;
    return VINF_SUCCESS;
#else
    AssertFailed();
    return VERR_NOT_IMPLEMENTED;
#endif
}



/**
 * Maps the CR3 into HMA in GC and locate it in HC.
 *
 * @returns VBox status, no specials.
 * @param   pVM             VM handle.
 * @param   GCPhysCR3       The physical address in the CR3 register.
 */
PGM_GST_DECL(int, MapCR3)(PVM pVM, RTGCPHYS GCPhysCR3)
{
#if PGM_GST_TYPE == PGM_TYPE_32BIT \
 || PGM_GST_TYPE == PGM_TYPE_PAE \
 || PGM_GST_TYPE == PGM_TYPE_AMD64

    LogFlow(("MapCR3: %VGp\n", GCPhysCR3));

    /*
     * Map the page CR3 points at.
     */
    RTHCPHYS    HCPhysGuestCR3;
    RTHCPTR     HCPtrGuestCR3;
    int rc = pgmRamGCPhys2HCPtrAndHCPhysWithFlags(&pVM->pgm.s, GCPhysCR3 & GST_CR3_PAGE_MASK, &HCPtrGuestCR3, &HCPhysGuestCR3);
    if (VBOX_SUCCESS(rc))
    {
        rc = PGMMap(pVM, (RTGCUINTPTR)pVM->pgm.s.GCPtrCR3Mapping, HCPhysGuestCR3, PAGE_SIZE, 0);
        if (VBOX_SUCCESS(rc))
        {
            PGM_INVL_PG(pVM->pgm.s.GCPtrCR3Mapping);
#if PGM_GST_TYPE == PGM_TYPE_32BIT
            pVM->pgm.s.pGuestPDHC = (R3R0PTRTYPE(PX86PD))HCPtrGuestCR3;
            pVM->pgm.s.pGuestPDGC = (GCPTRTYPE(PX86PD))pVM->pgm.s.GCPtrCR3Mapping;

#elif PGM_GST_TYPE == PGM_TYPE_PAE
            unsigned offset = GCPhysCR3 & GST_CR3_PAGE_MASK & PAGE_OFFSET_MASK;
            pVM->pgm.s.pGstPaePDPTHC = (R3R0PTRTYPE(PX86PDPT)) HCPtrGuestCR3;
            pVM->pgm.s.pGstPaePDPTGC = (GCPTRTYPE(PX86PDPT))   ((GCPTRTYPE(uint8_t *))pVM->pgm.s.GCPtrCR3Mapping + offset);
            Log(("Cached mapping %VGv\n", pVM->pgm.s.pGstPaePDPTGC));

            /*
             * Map the 4 PDs too.
             */
            RTGCUINTPTR GCPtr = (RTGCUINTPTR)pVM->pgm.s.GCPtrCR3Mapping + PAGE_SIZE;
            for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++, GCPtr += PAGE_SIZE)
            {
                if (pVM->pgm.s.CTXSUFF(pGstPaePDPT)->a[i].n.u1Present)
                {
                    RTHCPTR     HCPtr;
                    RTHCPHYS    HCPhys;
                    RTGCPHYS    GCPhys = pVM->pgm.s.CTXSUFF(pGstPaePDPT)->a[i].u & X86_PDPE_PG_MASK;
                    int rc2 = pgmRamGCPhys2HCPtrAndHCPhysWithFlags(&pVM->pgm.s, GCPhys, &HCPtr, &HCPhys);
                    if (VBOX_SUCCESS(rc2))
                    {
                        rc = PGMMap(pVM, GCPtr, HCPhys & X86_PTE_PAE_PG_MASK, PAGE_SIZE, 0);
                        AssertRCReturn(rc, rc);
                        pVM->pgm.s.apGstPaePDsHC[i]     = (R3R0PTRTYPE(PX86PDPAE))HCPtr;
                        pVM->pgm.s.apGstPaePDsGC[i]     = (GCPTRTYPE(PX86PDPAE))GCPtr;
                        pVM->pgm.s.aGCPhysGstPaePDs[i]  = GCPhys;
                        PGM_INVL_PG(GCPtr);
                        continue;
                    }
                    AssertMsgFailed(("pgmR3Gst32BitMapCR3: rc2=%d GCPhys=%RGp i=%d\n", rc2, GCPhys, i));
                }

                pVM->pgm.s.apGstPaePDsHC[i]     = 0;
                pVM->pgm.s.apGstPaePDsGC[i]     = 0;
                pVM->pgm.s.aGCPhysGstPaePDs[i]  = NIL_RTGCPHYS;
                PGM_INVL_PG(GCPtr);
            }

#else /* PGM_GST_TYPE == PGM_TYPE_AMD64 */
            rc = VERR_NOT_IMPLEMENTED;
#endif
        }
        else
            AssertMsgFailed(("rc=%Vrc GCPhysGuestPD=%VGp\n", rc, GCPhysCR3));
    }
    else
        AssertMsgFailed(("rc=%Vrc GCPhysGuestPD=%VGp\n", rc, GCPhysCR3));

#else /* prot/real mode stub */
    int rc = VINF_SUCCESS;
#endif
    return rc;
}


/**
 * Unmaps the CR3.
 *
 * @returns VBox status, no specials.
 * @param   pVM             VM handle.
 * @param   GCPhysCR3       The physical address in the CR3 register.
 */
PGM_GST_DECL(int, UnmapCR3)(PVM pVM)
{
    LogFlow(("UnmapCR3\n"));

    int rc = VINF_SUCCESS;
#if PGM_GST_TYPE == PGM_TYPE_32BIT
    pVM->pgm.s.pGuestPDHC = 0;
    pVM->pgm.s.pGuestPDGC = 0;

#elif PGM_GST_TYPE == PGM_TYPE_PAE
    pVM->pgm.s.pGstPaePDPTHC = 0;
    pVM->pgm.s.pGstPaePDPTGC = 0;
    for (unsigned i=0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
    {
        pVM->pgm.s.apGstPaePDsHC[i]    = 0;
        pVM->pgm.s.apGstPaePDsGC[i]    = 0;        
        pVM->pgm.s.aGCPhysGstPaePDs[i] = NIL_RTGCPHYS;
    }

#elif PGM_GST_TYPE == PGM_TYPE_AMD64
//#error not implemented
    rc = VERR_NOT_IMPLEMENTED;

#else /* prot/real mode stub */
    /* nothing to do */
#endif
    return rc;
}


#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_PGM_POOL

/**
 * Registers physical page monitors for the necessary paging
 * structures to detect conflicts with our guest mappings.
 *
 * This is always called after mapping CR3.
 * This is never called with fixed mappings.
 *
 * @returns VBox status, no specials.
 * @param   pVM             VM handle.
 * @param   GCPhysCR3       The physical address in the CR3 register.
 */
PGM_GST_DECL(int, MonitorCR3)(PVM pVM, RTGCPHYS GCPhysCR3)
{
    Assert(!pVM->pgm.s.fMappingsFixed);
    int rc = VINF_SUCCESS;

    /*
     * Register/Modify write phys handler for guest's CR3 if it changed.
     */
#if PGM_GST_TYPE == PGM_TYPE_32BIT

    if (pVM->pgm.s.GCPhysGstCR3Monitored != GCPhysCR3)
    {
# ifndef PGMPOOL_WITH_MIXED_PT_CR3
        const unsigned cbCR3Stuff = PGM_GST_TYPE == PGM_TYPE_PAE ? 32 : PAGE_SIZE;
        if (pVM->pgm.s.GCPhysGstCR3Monitored != NIL_RTGCPHYS)
            rc = PGMHandlerPhysicalModify(pVM, pVM->pgm.s.GCPhysGstCR3Monitored, GCPhysCR3, GCPhysCR3 + cbCR3Stuff - 1);
        else
            rc = PGMHandlerPhysicalRegisterEx(pVM, PGMPHYSHANDLERTYPE_PHYSICAL_WRITE, GCPhysCR3, GCPhysCR3 + cbCR3Stuff - 1,
                                              pVM->pgm.s.pfnR3GstWriteHandlerCR3, 0,
                                              pVM->pgm.s.pfnR0GstWriteHandlerCR3, 0,
                                              pVM->pgm.s.pfnGCGstWriteHandlerCR3, 0,
                                              pVM->pgm.s.pszR3GstWriteHandlerCR3);
# else  /* PGMPOOL_WITH_MIXED_PT_CR3 */
        rc = pgmPoolMonitorMonitorCR3(pVM->pgm.s.CTXSUFF(pPool),
                                         pVM->pgm.s.enmShadowMode == PGMMODE_PAE
                                      || pVM->pgm.s.enmShadowMode == PGMMODE_PAE_NX
                                      ? PGMPOOL_IDX_PAE_PD
                                      : PGMPOOL_IDX_PD,
                                      GCPhysCR3);
# endif /* PGMPOOL_WITH_MIXED_PT_CR3 */
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("PGMHandlerPhysicalModify/PGMR3HandlerPhysicalRegister failed, rc=%Rrc GCPhysGstCR3Monitored=%RGp GCPhysCR3=%RGp\n",
                             rc, pVM->pgm.s.GCPhysGstCR3Monitored, GCPhysCR3));
            return rc;
        }
        pVM->pgm.s.GCPhysGstCR3Monitored = GCPhysCR3;
    }

#elif PGM_GST_TYPE == PGM_TYPE_PAE
    /* Monitor the PDPT page */
    /*
     * Register/Modify write phys handler for guest's CR3 if it changed.
     */
# ifndef PGMPOOL_WITH_MIXED_PT_CR3
    AssertFailed();
# endif
    if (pVM->pgm.s.GCPhysGstCR3Monitored != GCPhysCR3)
    {
        rc = pgmPoolMonitorMonitorCR3(pVM->pgm.s.CTXSUFF(pPool), PGMPOOL_IDX_PDPT, GCPhysCR3);
        if (VBOX_FAILURE(rc))
        {
            AssertMsgFailed(("PGMHandlerPhysicalModify/PGMR3HandlerPhysicalRegister failed, rc=%Rrc GCPhysGstCR3Monitored=%RGp GCPhysCR3=%RGp\n",
                             rc, pVM->pgm.s.GCPhysGstCR3Monitored, GCPhysCR3));
            return rc;
        }
        pVM->pgm.s.GCPhysGstCR3Monitored = GCPhysCR3;
    }
    /*
     * Do the 4 PDs.
     */
    for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
    {
        if (CTXSUFF(pVM->pgm.s.pGstPaePDPT)->a[i].n.u1Present)
        {
            RTGCPHYS GCPhys = CTXSUFF(pVM->pgm.s.pGstPaePDPT)->a[i].u & X86_PDPE_PG_MASK;
            if (pVM->pgm.s.aGCPhysGstPaePDsMonitored[i] != GCPhys)
            {
                Assert(pVM->pgm.s.enmShadowMode == PGMMODE_PAE || pVM->pgm.s.enmShadowMode == PGMMODE_PAE_NX);

                rc = pgmPoolMonitorMonitorCR3(pVM->pgm.s.CTXSUFF(pPool), PGMPOOL_IDX_PAE_PD_0 + i, GCPhys);
            }

            if (VBOX_FAILURE(rc))
            {
                AssertMsgFailed(("PGMHandlerPhysicalModify/PGMR3HandlerPhysicalRegister failed, rc=%Rrc GCPhysGstCR3Monitored=%RGp GCPhysCR3=%RGp\n",
                                 rc, pVM->pgm.s.aGCPhysGstPaePDsMonitored[i], GCPhys));
                return rc;
            }
            pVM->pgm.s.aGCPhysGstPaePDsMonitored[i] = GCPhys;
        }
        else if (pVM->pgm.s.aGCPhysGstPaePDsMonitored[i] != NIL_RTGCPHYS)
        {
            rc = pgmPoolMonitorUnmonitorCR3(pVM->pgm.s.CTXSUFF(pPool), PGMPOOL_IDX_PAE_PD_0 + i);
            AssertRC(rc);
            pVM->pgm.s.aGCPhysGstPaePDsMonitored[i] = NIL_RTGCPHYS;
        }
    }

#elif PGM_GST_TYPE == PGM_TYPE_AMD64
    AssertFailed();
#else
    /* prot/real mode stub */

#endif
    return rc;
}

/**
 * Deregisters any physical page monitors installed by MonitorCR3.
 *
 * @returns VBox status code, no specials.
 * @param   pVM         The VM handle.
 */
PGM_GST_DECL(int, UnmonitorCR3)(PVM pVM)
{
    int rc = VINF_SUCCESS;

    /*
     * Deregister the access handlers.
     *
     * PGMSyncCR3 will reinstall it if required and PGMSyncCR3 will be executed
     * before we enter GC again.
     */
#if PGM_GST_TYPE == PGM_TYPE_32BIT
    if (pVM->pgm.s.GCPhysGstCR3Monitored != NIL_RTGCPHYS)
    {
# ifndef PGMPOOL_WITH_MIXED_PT_CR3
        rc = PGMHandlerPhysicalDeregister(pVM, pVM->pgm.s.GCPhysGstCR3Monitored);
        AssertRCReturn(rc, rc);
# else /* PGMPOOL_WITH_MIXED_PT_CR3 */
        rc = pgmPoolMonitorUnmonitorCR3(pVM->pgm.s.CTXSUFF(pPool),
                                           pVM->pgm.s.enmShadowMode == PGMMODE_PAE
                                        || pVM->pgm.s.enmShadowMode == PGMMODE_PAE_NX
                                        ? PGMPOOL_IDX_PAE_PD
                                        : PGMPOOL_IDX_PD);
        AssertRCReturn(rc, rc);
# endif /* PGMPOOL_WITH_MIXED_PT_CR3 */
        pVM->pgm.s.GCPhysGstCR3Monitored = NIL_RTGCPHYS;
    }

#elif PGM_GST_TYPE == PGM_TYPE_PAE
    /* The PDPT page */
# ifndef PGMPOOL_WITH_MIXED_PT_CR3
    AssertFailed();
# endif

    if (pVM->pgm.s.GCPhysGstCR3Monitored != NIL_RTGCPHYS)
    {
        rc = pgmPoolMonitorUnmonitorCR3(pVM->pgm.s.CTXSUFF(pPool), PGMPOOL_IDX_PDPT);
        AssertRC(rc);
    }

    /* The 4 PDs. */
    for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
    {
        if (pVM->pgm.s.aGCPhysGstPaePDsMonitored[i] != NIL_RTGCPHYS)
        {
            Assert(pVM->pgm.s.enmShadowMode == PGMMODE_PAE || pVM->pgm.s.enmShadowMode == PGMMODE_PAE_NX);
            int rc2 = pgmPoolMonitorUnmonitorCR3(pVM->pgm.s.CTXSUFF(pPool), PGMPOOL_IDX_PAE_PD_0 + i);
            AssertRC(rc2);
            if (VBOX_FAILURE(rc2))
                rc = rc2;
            pVM->pgm.s.aGCPhysGstPaePDsMonitored[i] = NIL_RTGCPHYS;
        }
    }
#elif PGM_GST_TYPE == PGM_TYPE_AMD64
    AssertFailed();
#else
    /* prot/real mode stub */
#endif
    return rc;

}

#undef LOG_GROUP
#define LOG_GROUP LOG_GROUP_PGM


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
    PX86PD          pPDSrc = pState->pVM->pgm.s.CTXSUFF(pGuestPD);
#endif

    RTGCUINTPTR     GCPtr = (RTUINTPTR)pCur->GCPtr;
#if PGM_GST_MODE != PGM_MODE_AMD64
    /* skip all stuff above 4GB if not AMD64 mode. */
    if (GCPtr >= _4GB)
        return 0;
#endif

    unsigned    offPage = GCPtr & PAGE_OFFSET_MASK;
    unsigned    iPage = 0;
    while (iPage < pCur->cPages)
    {
#if PGM_GST_TYPE == PGM_TYPE_32BIT
        X86PDE      Pde = pPDSrc->a[GCPtr >> X86_PD_SHIFT];
#elif PGM_GST_TYPE == PGM_TYPE_PAE
        X86PDEPAE   Pde;
        Pde.u = pgmGstGetPaePDE(&pState->pVM->pgm.s, GCPtr);
#elif PGM_GST_TYPE == PGM_TYPE_AMD64
        X86PDEPAE   Pde;
        Pde.u = pgmGstGetLongModePDE(&pState->pVM->pgm.s, GCPtr);
#endif
        if (Pde.n.u1Present)
        {
            if (!Pde.b.u1Size || !(pState->cr4 & X86_CR4_PSE))
            {
                /*
                 * Normal page table.
                 */
                PGSTPT pPT;
                int rc = PGM_GCPHYS_2_PTR(pState->pVM, Pde.u & GST_PDE_PG_MASK, &pPT);
                if (VBOX_SUCCESS(rc))
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
                                             ("{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} GCPhysNew=%VGp\n",
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
                                             ("{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32}\n",
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
                                         ("{.Core.Key=%VGp, .Core.KeyLast=%VGp, .offVirtHandler=%#RX32, .offNextAlias=%#RX32} GCPhysNew=%VGp\n",
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

#if PGM_GST_TYPE == PGM_TYPE_AMD64
    AssertFailed();
#endif

    /** @todo
     * In theory this is not sufficient: the guest can change a single page in a range with invlpg
     */

    /*
     * Resolve any virtual address based access handlers to GC physical addresses.
     * This should be fairly quick.
     */
    PGMHVUSTATE State;

    pgmLock(pVM);
    STAM_PROFILE_START(&pVM->pgm.s.CTXMID(Stat,SyncCR3HandlerVirtualUpdate), a);
    State.pVM   = pVM;
    State.fTodo = pVM->pgm.s.fSyncFlags;
    State.cr4   = cr4;
    RTAvlroGCPtrDoWithAll(&pVM->pgm.s.CTXSUFF(pTrees)->VirtHandlers, true, PGM_GST_NAME(VirtHandlerUpdateOne), &State);
    STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncCR3HandlerVirtualUpdate), a);


    /*
     * Set / reset bits?
     */
    if (State.fTodo & PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL)
    {
        STAM_PROFILE_START(&pVM->pgm.s.CTXMID(Stat,SyncCR3HandlerVirtualReset), b);
        Log(("pgmR3VirtualHandlersUpdate: resets bits\n"));
        RTAvlroGCPtrDoWithAll(&pVM->pgm.s.CTXSUFF(pTrees)->VirtHandlers, true, pgmHandlerVirtualResetOne, pVM);
        pVM->pgm.s.fSyncFlags &= ~PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL;
        STAM_PROFILE_STOP(&pVM->pgm.s.CTXMID(Stat,SyncCR3HandlerVirtualReset), b);
    }
    pgmUnlock(pVM);

    return !!(State.fTodo & PGM_SYNC_UPDATE_PAGE_BIT_VIRTUAL);

#else /* real / protected */
    return false;
#endif
}


#if PGM_GST_TYPE == PGM_TYPE_32BIT && !defined(IN_RING3)

/**
 * Write access handler for the Guest CR3 page in 32-bit mode.
 *
 * This will try interpret the instruction, if failure fail back to the recompiler.
 * Check if the changed PDEs are marked present and conflicts with our
 * mappings. If conflict, we'll switch to the host context and resolve it there
 *
 * @returns VBox status code (appropritate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
PGM_GST_DECL(int, WriteHandlerCR3)(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    AssertMsg(!pVM->pgm.s.fMappingsFixed, ("Shouldn't be registered when mappings are fixed!\n"));

    /*
     * Try interpret the instruction.
     */
    uint32_t cb;
    int rc = EMInterpretInstruction(pVM, pRegFrame, pvFault, &cb);
    if (VBOX_SUCCESS(rc) && cb)
    {
        /*
         * Check if the modified PDEs are present and mappings.
         */
        const RTGCUINTPTR offPD = GCPhysFault & PAGE_OFFSET_MASK;
        const unsigned      iPD1  = offPD / sizeof(X86PDE);
        const unsigned      iPD2  = (offPD + cb - 1) / sizeof(X86PDE);

        Assert(cb > 0 && cb <= 8);
        Assert(iPD1 < RT_ELEMENTS(pVM->pgm.s.CTXSUFF(pGuestPD)->a)); /// @todo R3/R0 separation.
        Assert(iPD2 < RT_ELEMENTS(pVM->pgm.s.CTXSUFF(pGuestPD)->a));

#ifdef DEBUG
        Log(("pgmXXGst32BitWriteHandlerCR3: emulated change to PD %#x addr=%VGv\n", iPD1, iPD1 << X86_PD_SHIFT));
        if (iPD1 != iPD2)
            Log(("pgmXXGst32BitWriteHandlerCR3: emulated change to PD %#x addr=%VGv\n", iPD2, iPD2 << X86_PD_SHIFT));
#endif

        if (!pVM->pgm.s.fMappingsFixed)
        {
            PX86PD pPDSrc = CTXSUFF(pVM->pgm.s.pGuestPD);
            if (    (   pPDSrc->a[iPD1].n.u1Present
                     && pgmGetMapping(pVM, (RTGCPTR)(iPD1 << X86_PD_SHIFT)) )
                ||  (   iPD1 != iPD2
                     && pPDSrc->a[iPD2].n.u1Present
                     && pgmGetMapping(pVM, (RTGCPTR)(iPD2 << X86_PD_SHIFT)) )
               )
            {
                STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteConflict);
                VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                if (rc == VINF_SUCCESS)
                    rc = VINF_PGM_SYNC_CR3;
                Log(("pgmXXGst32BitWriteHandlerCR3: detected conflict iPD1=%#x iPD2=%#x - returns %Rrc\n", iPD1, iPD2, rc));
                return rc;
            }
        }

        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteHandled);
    }
    else
    {
        Assert(VBOX_FAILURE(rc));
        if (rc == VERR_EM_INTERPRETER)
            rc = VINF_EM_RAW_EMULATE_INSTR_PD_FAULT;
        Log(("pgmXXGst32BitWriteHandlerCR3: returns %Rrc\n", rc));
        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteUnhandled);
    }
    return rc;
}

#endif /* PGM_TYPE_32BIT && !IN_RING3 */


#if PGM_GST_TYPE == PGM_TYPE_PAE && !defined(IN_RING3)

/**
 * Write access handler for the Guest CR3 page in PAE mode.
 *
 * This will try interpret the instruction, if failure fail back to the recompiler.
 * Check if the changed PDEs are marked present and conflicts with our
 * mappings. If conflict, we'll switch to the host context and resolve it there
 *
 * @returns VBox status code (appropritate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
PGM_GST_DECL(int, WriteHandlerCR3)(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    AssertMsg(!pVM->pgm.s.fMappingsFixed, ("Shouldn't be registered when mappings are fixed!\n"));

    /*
     * Try interpret the instruction.
     */
    uint32_t cb;
    int rc = EMInterpretInstruction(pVM, pRegFrame, pvFault, &cb);
    if (VBOX_SUCCESS(rc) && cb)
    {
        /*
         * Check if any of the PDs have changed.
         * We'll simply check all of them instead of figuring out which one/two to check.
         */
        for (unsigned i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
        {
            if (    CTXSUFF(pVM->pgm.s.pGstPaePDPT)->a[i].n.u1Present
                &&  (   CTXSUFF(pVM->pgm.s.pGstPaePDPT)->a[i].u & X86_PDPE_PG_MASK)
                     != pVM->pgm.s.aGCPhysGstPaePDsMonitored[i])
            {
                /*
                 * The PDPE has changed.
                 * We will schedule a monitoring update for the next TLB Flush,
                 * InvalidatePage or SyncCR3.
                 *
                 * This isn't perfect, because a lazy page sync might be dealing with an half
                 * updated PDPE. However, we assume that the guest OS is disabling interrupts
                 * and being extremely careful (cmpxchg8b) when updating a PDPE where it's
                 * executing.
                 */
                pVM->pgm.s.fSyncFlags |= PGM_SYNC_MONITOR_CR3;
                Log(("pgmXXGstPaeWriteHandlerCR3: detected updated PDPE; [%d] = %#llx, Old GCPhys=%VGp\n",
                     i, CTXSUFF(pVM->pgm.s.pGstPaePDPT)->a[i].u, pVM->pgm.s.aGCPhysGstPaePDsMonitored[i]));
            }
        }

        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteHandled);
    }
    else
    {
        Assert(VBOX_FAILURE(rc));
        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteUnhandled);
        if (rc == VERR_EM_INTERPRETER)
            rc = VINF_EM_RAW_EMULATE_INSTR_PD_FAULT;
    }
    Log(("pgmXXGstPaeWriteHandlerCR3: returns %Rrc\n", rc));
    return rc;
}


/**
 * Write access handler for the Guest PDs in PAE mode.
 *
 * This will try interpret the instruction, if failure fail back to the recompiler.
 * Check if the changed PDEs are marked present and conflicts with our
 * mappings. If conflict, we'll switch to the host context and resolve it there
 *
 * @returns VBox status code (appropritate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   GCPhysFault The GC physical address corresponding to pvFault.
 * @param   pvUser      User argument.
 */
PGM_GST_DECL(int, WriteHandlerPD)(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPHYS GCPhysFault, void *pvUser)
{
    AssertMsg(!pVM->pgm.s.fMappingsFixed, ("Shouldn't be registered when mappings are fixed!\n"));

    /*
     * Try interpret the instruction.
     */
    uint32_t cb;
    int rc = EMInterpretInstruction(pVM, pRegFrame, pvFault, &cb);
    if (VBOX_SUCCESS(rc) && cb)
    {
        /*
         * Figure out which of the 4 PDs this is.
         */
        RTGCUINTPTR i;
        for (i = 0; i < X86_PG_PAE_PDPE_ENTRIES; i++)
            if (CTXSUFF(pVM->pgm.s.pGstPaePDPT)->a[i].u == (GCPhysFault & X86_PTE_PAE_PG_MASK))
            {
                PX86PDPAE           pPDSrc = pgmGstGetPaePD(&pVM->pgm.s, i << X86_PDPT_SHIFT);
                const RTGCUINTPTR offPD  = GCPhysFault & PAGE_OFFSET_MASK;
                const unsigned      iPD1   = offPD / sizeof(X86PDEPAE);
                const unsigned      iPD2   = (offPD + cb - 1) / sizeof(X86PDEPAE);

                Assert(cb > 0 && cb <= 8);
                Assert(iPD1 < X86_PG_PAE_ENTRIES);
                Assert(iPD2 < X86_PG_PAE_ENTRIES);

#ifdef DEBUG
                Log(("pgmXXGstPaeWriteHandlerPD: emulated change to i=%d iPD1=%#05x (%VGv)\n",
                     i, iPD1, (i << X86_PDPT_SHIFT) | (iPD1 << X86_PD_PAE_SHIFT)));
                if (iPD1 != iPD2)
                    Log(("pgmXXGstPaeWriteHandlerPD: emulated change to i=%d iPD2=%#05x (%VGv)\n",
                         i, iPD2, (i << X86_PDPT_SHIFT) | (iPD2 << X86_PD_PAE_SHIFT)));
#endif

                if (!pVM->pgm.s.fMappingsFixed)
                {
                    if (    (   pPDSrc->a[iPD1].n.u1Present
                             && pgmGetMapping(pVM, (RTGCPTR)((i << X86_PDPT_SHIFT) | (iPD1 << X86_PD_PAE_SHIFT))) )
                        ||  (   iPD1 != iPD2
                             && pPDSrc->a[iPD2].n.u1Present
                             && pgmGetMapping(pVM, (RTGCPTR)((i << X86_PDPT_SHIFT) | (iPD2 << X86_PD_PAE_SHIFT))) )
                       )
                    {
                        Log(("pgmXXGstPaeWriteHandlerPD: detected conflict iPD1=%#x iPD2=%#x\n", iPD1, iPD2));
                        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteConflict);
                        VM_FF_SET(pVM, VM_FF_PGM_SYNC_CR3);
                        return VINF_PGM_SYNC_CR3;
                    }
                }
                break; /* ASSUMES no duplicate entries... */
            }
        Assert(i < 4);

        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteHandled);
    }
    else
    {
        Assert(VBOX_FAILURE(rc));
        if (rc == VERR_EM_INTERPRETER)
            rc = VINF_EM_RAW_EMULATE_INSTR_PD_FAULT;
        else
            Log(("pgmXXGst32BitWriteHandlerCR3: returns %Rrc\n", rc));
        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteUnhandled);
    }
    return rc;
}

#endif /* PGM_TYPE_PAE && !IN_RING3 */

