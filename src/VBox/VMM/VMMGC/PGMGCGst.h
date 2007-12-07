/* $Id$ */
/** @file
 * VBox - Page Manager, Guest Paging Template - Guest Context.
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
#undef GST_PDE4M_PG_MASK
#undef GST_PD_SHIFT
#undef GST_PD_MASK
#undef GST_PTE_PG_MASK
#undef GST_PT_SHIFT
#undef GST_PT_MASK

#if PGM_GST_TYPE == PGM_TYPE_32BIT
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
# define GST_PDE4M_PG_MASK          X86_PDE4M_PG_MASK
# define GST_PD_SHIFT               X86_PD_SHIFT
# define GST_PD_MASK                X86_PD_MASK
# define GST_PTE_PG_MASK            X86_PTE_PG_MASK
# define GST_PT_SHIFT               X86_PT_SHIFT
# define GST_PT_MASK                X86_PT_MASK
#else
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
# define GST_PDE4M_PG_MASK          X86_PDE4M_PAE_PG_MASK
# define GST_PD_SHIFT               X86_PD_PAE_SHIFT
# define GST_PD_MASK                X86_PD_PAE_MASK
# define GST_PTE_PG_MASK            X86_PTE_PAE_PG_MASK
# define GST_PT_SHIFT               X86_PT_PAE_SHIFT
# define GST_PT_MASK                X86_PT_PAE_MASK
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
__BEGIN_DECLS
PGMGCDECL(int) pgmGCGst32BitWriteHandlerCR3(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser);
PGMGCDECL(int) pgmGCGstPAEWriteHandlerCR3(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser);
PGMGCDECL(int) pgmGCGstPAEWriteHandlerPD(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser);
__END_DECLS


#if PGM_GST_TYPE == PGM_TYPE_32BIT

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
PGMGCDECL(int) pgmGCGst32BitWriteHandlerCR3(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser)
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
        Assert(iPD1 < ELEMENTS(pVM->pgm.s.pGuestPDGC->a));
        Assert(iPD2 < ELEMENTS(pVM->pgm.s.pGuestPDGC->a));

#ifdef DEBUG
        Log(("pgmGCGst32BitWriteHandlerCR3: emulated change to PD %#x addr=%VGv\n", iPD1, iPD1 << X86_PD_SHIFT));
        if (iPD1 != iPD2)
            Log(("pgmGCGst32BitWriteHandlerCR3: emulated change to PD %#x addr=%VGv\n", iPD2, iPD2 << X86_PD_SHIFT));
#endif

        if (!pVM->pgm.s.fMappingsFixed)
        {
            PX86PD pPDSrc = pVM->pgm.s.pGuestPDGC;
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
                Log(("pgmGCGst32BitWriteHandlerCR3: detected conflict iPD1=%#x iPD2=%#x - returns %Rrc\n", iPD1, iPD2, rc));
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
        Log(("pgmGCGst32BitWriteHandlerCR3: returns %Rrc\n", rc));
        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteUnhandled);
    }
    return rc;
}

#endif /* PGM_TYPE_32BIT */


#if PGM_GST_TYPE == PGM_TYPE_PAE

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
PGMGCDECL(int) pgmGCGstPAEWriteHandlerCR3(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser)
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
        for (unsigned i = 0; i < 4; i++)
        {
            if (    CTXSUFF(pVM->pgm.s.pGstPaePDPTR)->a[i].n.u1Present
                &&  (   CTXSUFF(pVM->pgm.s.pGstPaePDPTR)->a[i].u & X86_PDPE_PG_MASK)
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
                Log(("pgmGCGstPaeWriteHandlerCR3: detected updated PDPE; [%d] = %#llx, Old GCPhys=%VGp\n",
                     i, CTXSUFF(pVM->pgm.s.pGstPaePDPTR)->a[i].u, pVM->pgm.s.aGCPhysGstPaePDsMonitored[i]));
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
    Log(("pgmGCGstPaeWriteHandlerCR3: returns %Rrc\n", rc));
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
PGMGCDECL(int) pgmGCGstPAEWriteHandlerPD(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, RTGCPHYS GCPhysFault, void *pvUser)
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
        for (i = 0; i < 4; i++)
            if (CTXSUFF(pVM->pgm.s.pGstPaePDPTR)->a[i].u == (GCPhysFault & X86_PTE_PAE_PG_MASK))
            {
                PX86PDPAE           pPDSrc = pgmGstGetPaePD(&pVM->pgm.s, i << X86_PDPTR_SHIFT);
                const RTGCUINTPTR offPD  = GCPhysFault & PAGE_OFFSET_MASK;
                const unsigned      iPD1   = offPD / sizeof(X86PDEPAE);
                const unsigned      iPD2   = (offPD + cb - 1) / sizeof(X86PDEPAE);

                Assert(cb > 0 && cb <= 8);
                Assert(iPD1 < X86_PG_PAE_ENTRIES);
                Assert(iPD2 < X86_PG_PAE_ENTRIES);

#ifdef DEBUG
                Log(("pgmGCGstPaeWriteHandlerPD: emulated change to i=%d iPD1=%#05x (%VGv)\n",
                     i, iPD1, (i << X86_PDPTR_SHIFT) | (iPD1 << X86_PD_PAE_SHIFT)));
                if (iPD1 != iPD2)
                    Log(("pgmGCGstPaeWriteHandlerPD: emulated change to i=%d iPD2=%#05x (%VGv)\n",
                         i, iPD2, (i << X86_PDPTR_SHIFT) | (iPD2 << X86_PD_PAE_SHIFT)));
#endif

                if (!pVM->pgm.s.fMappingsFixed)
                {
                    if (    (   pPDSrc->a[iPD1].n.u1Present
                             && pgmGetMapping(pVM, (RTGCPTR)((i << X86_PDPTR_SHIFT) | (iPD1 << X86_PD_PAE_SHIFT))) )
                        ||  (   iPD1 != iPD2
                             && pPDSrc->a[iPD2].n.u1Present
                             && pgmGetMapping(pVM, (RTGCPTR)((i << X86_PDPTR_SHIFT) | (iPD2 << X86_PD_PAE_SHIFT))) )
                       )
                    {
                        Log(("pgmGCGstPaeWriteHandlerPD: detected conflict iPD1=%#x iPD2=%#x\n", iPD1, iPD2));
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
            Log(("pgmGCGst32BitWriteHandlerCR3: returns %Rrc\n", rc));
        STAM_COUNTER_INC(&pVM->pgm.s.StatGCGuestCR3WriteUnhandled);
    }
    return rc;
}

#endif /* PGM_TYPE_PAE */


