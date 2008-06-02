/* $Id$ */
/** @file
 * SELM - The Selector Manager, Guest Context.
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
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SELM
#include <VBox/selm.h>
#include <VBox/mm.h>
#include <VBox/em.h>
#include <VBox/trpm.h>
#include "SELMInternal.h"
#include <VBox/vm.h>
#include <VBox/pgm.h>

#include <VBox/param.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>


/**
 * Synchronizes one GDT entry (guest -> shadow).
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   pRegFrame   Trap register frame.
 * @param   iGDTEntry   The GDT entry to sync.
 */
static int selmGCSyncGDTEntry(PVM pVM, PCPUMCTXCORE pRegFrame, unsigned iGDTEntry)
{
    Log2(("GDT %04X LDTR=%04X\n", iGDTEntry, CPUMGetGuestLDTR(pVM)));

    /*
     * Validate the offset.
     */
    VBOXGDTR GdtrGuest;
    CPUMGetGuestGDTR(pVM, &GdtrGuest);
    unsigned offEntry = iGDTEntry * sizeof(VBOXDESC);
    if (    iGDTEntry >= SELM_GDT_ELEMENTS
        ||  offEntry > GdtrGuest.cbGdt)
        return VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT;

    /*
     * Read the guest descriptor.
     */
    VBOXDESC Desc;
    int rc = MMGCRamRead(pVM, &Desc, (uint8_t *)GdtrGuest.pGdt + offEntry, sizeof(VBOXDESC));
    if (VBOX_FAILURE(rc))
        return VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT;

    /*
     * Check for conflicts.
     */
    RTSEL   Sel = iGDTEntry << X86_SEL_SHIFT;
    Assert(   !(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS] & ~X86_SEL_MASK)
           && !(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS] & ~X86_SEL_MASK)
           && !(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64] & ~X86_SEL_MASK)
           && !(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS] & ~X86_SEL_MASK)
           && !(pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] & ~X86_SEL_MASK));
    if (    pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS]         == Sel
        ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_DS]         == Sel
        ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_CS64]       == Sel
        ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS]        == Sel
        ||  pVM->selm.s.aHyperSel[SELM_HYPER_SEL_TSS_TRAP08] == Sel)
    {
        if (Desc.Gen.u1Present)
        {
            Log(("selmGCSyncGDTEntry: Sel=%d Desc=%.8Vhxs: detected conflict!!\n", Sel, &Desc));
            return VINF_SELM_SYNC_GDT;
        }
        Log(("selmGCSyncGDTEntry: Sel=%d Desc=%.8Vhxs: potential conflict (still not present)!\n", Sel, &Desc));

        /* Note: we can't continue below or else we'll change the shadow descriptor!! */
        /* When the guest makes the selector present, then we'll do a GDT sync. */
        return VINF_SUCCESS;
    }

    /*
     * Code and data selectors are generally 1:1, with the
     * 'little' adjustment we do for DPL 0 selectors.
     */
    PVBOXDESC   pShadowDescr = &pVM->selm.s.paGdtGC[iGDTEntry];
    if (Desc.Gen.u1DescType)
    {
        /*
         * Hack for A-bit against Trap E on read-only GDT.
         */
        /** @todo Fix this by loading ds and cs before turning off WP. */
        Desc.Gen.u4Type |= X86_SEL_TYPE_ACCESSED;

        /*
         * All DPL 0 code and data segments are squeezed into DPL 1.
         *
         * We're skipping conforming segments here because those
         * cannot give us any trouble.
         */
        if (    Desc.Gen.u2Dpl == 0
            &&      (Desc.Gen.u4Type & (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF))
                !=  (X86_SEL_TYPE_CODE | X86_SEL_TYPE_CONF) )
            Desc.Gen.u2Dpl = 1;
    }
    else
    {
        /*
         * System type selectors are marked not present.
         * Recompiler or special handling is required for these.
         */
        /** @todo what about interrupt gates and rawr0? */
        Desc.Gen.u1Present = 0;
    }
    //Log(("O: base=%08X limit=%08X attr=%04X\n", pShadowDescr->Gen.u16BaseLow | (pShadowDescr->Gen.u8BaseHigh1 << 16) | (pShadowDescr->Gen.u8BaseHigh2 << 24), pShadowDescr->Gen.u16LimitLow | (pShadowDescr->Gen.u4LimitHigh << 16), (pShadowDescr->au32[1] >> 8) & 0xFFFF ));
    //Log(("N: base=%08X limit=%08X attr=%04X\n", Desc.Gen.u16BaseLow | (Desc.Gen.u8BaseHigh1 << 16) | (Desc.Gen.u8BaseHigh2 << 24), Desc.Gen.u16LimitLow | (Desc.Gen.u4LimitHigh << 16), (Desc.au32[1] >> 8) & 0xFFFF ));
    *pShadowDescr = Desc;

    /* Check if we change the LDT selector */
    if (Sel == CPUMGetGuestLDTR(pVM))
    {
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_LDT);
        return VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT;
    }

    /* Or the TR selector */
    if (Sel == CPUMGetGuestTR(pVM))
    {
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
        return VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT;
    }

#ifdef VBOX_STRICT
    if (Sel == (pRegFrame->cs & X86_SEL_MASK))
        Log(("GDT write to selector in CS register %04X\n", pRegFrame->cs));
    else
    if (Sel == (pRegFrame->ds & X86_SEL_MASK))
        Log(("GDT write to selector in DS register %04X\n", pRegFrame->ds));
    else
    if (Sel == (pRegFrame->es & X86_SEL_MASK))
        Log(("GDT write to selector in ES register %04X\n", pRegFrame->es));
    else
    if (Sel == (pRegFrame->fs & X86_SEL_MASK))
        Log(("GDT write to selector in FS register %04X\n", pRegFrame->fs));
    else
    if (Sel == (pRegFrame->gs & X86_SEL_MASK))
        Log(("GDT write to selector in GS register %04X\n", pRegFrame->gs));
    else
    if (Sel == (pRegFrame->ss & X86_SEL_MASK))
        Log(("GDT write to selector in SS register %04X\n", pRegFrame->ss));
#endif
    return VINF_SUCCESS;
}


/**
 * \#PF Virtual Handler callback for Guest write access to the Guest's own GDT.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
SELMGCDECL(int) selmgcGuestGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    LogFlow(("selmgcGuestGDTWriteHandler errcode=%x fault=%08x offRange=%08x\n", uErrorCode, pvFault, offRange));

    /*
     * First check if this is the LDT entry.
     * LDT updates are problemous since an invalid LDT entry will cause trouble during worldswitch.
     */
    int rc;
    if (CPUMGetGuestLDTR(pVM) / sizeof(VBOXDESC) == offRange / sizeof(VBOXDESC))
    {
        Log(("LDTR selector change -> fall back to HC!!\n"));
        rc = VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT;
        /** @todo We're not handling changed to the selectors in LDTR and TR correctly at all.
         * We should ignore any changes to those and sync them only when they are loaded by the guest! */
    }
    else
    {
        /*
         * Attempt to emulate the instruction and sync the affected entries.
         */
        /** @todo should check if any affected selectors are loaded. */
        uint32_t cb;
        rc = EMInterpretInstruction(pVM, pRegFrame, (RTGCPTR)(RTRCUINTPTR)pvFault, &cb);
        if (VBOX_SUCCESS(rc) && cb)
        {
            unsigned iGDTE1 = offRange / sizeof(VBOXDESC);
            int rc2 = selmGCSyncGDTEntry(pVM, pRegFrame, iGDTE1);
            if (rc2 == VINF_SUCCESS)
            {
                Assert(cb);
                unsigned iGDTE2 = (offRange + cb - 1) / sizeof(VBOXDESC);
                if (iGDTE1 != iGDTE2)
                    rc2 = selmGCSyncGDTEntry(pVM, pRegFrame, iGDTE2);
                if (rc2 == VINF_SUCCESS)
                {
                    STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestGDTHandled);
                    return rc;
                }
            }
            if (rc == VINF_SUCCESS || VBOX_FAILURE(rc2))
                rc = rc2;
        }
        else
        {
            Assert(VBOX_FAILURE(rc));
            if (rc == VERR_EM_INTERPRETER)
                rc = VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT;
        }
    }
    if (    rc != VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT
        &&  rc != VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT)
    {
        /* Not necessary when we need to go back to the host context to sync the LDT or TSS. */
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_GDT);
    }
    STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestGDTUnhandled);
    return rc;
}


/**
 * \#PF Virtual Handler callback for Guest write access to the Guest's own LDT.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
SELMGCDECL(int) selmgcGuestLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    /** @todo To be implemented. */
    ////LogCom(("selmgcGuestLDTWriteHandler: eip=%08X pvFault=%08X pvRange=%08X\r\n", pRegFrame->eip, pvFault, pvRange));

    VM_FF_SET(pVM, VM_FF_SELM_SYNC_LDT);
    STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestLDT);
    return VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT;
}


/**
 * \#PF Virtual Handler callback for Guest write access to the Guest's own current TSS.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode  CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
SELMGCDECL(int) selmgcGuestTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    LogFlow(("selmgcGuestTSSWriteHandler errcode=%x fault=%08x offRange=%08x\n", uErrorCode, pvFault, offRange));

    /*
     * Try emulate the access and compare the R0 ss:esp with the shadow tss values.
     *
     * Note, that it's safe to access the TSS after a successfull instruction emulation,
     * even if the stuff that was changed wasn't the ss0 or esp0 bits. The CPU insists
     * on the TSS being all one physical page, so ASSUMING that we're not trapping
     * I/O map accesses this is safe.
     */
    uint32_t cb;
    int rc = EMInterpretInstruction(pVM, pRegFrame, (RTGCPTR)(RTRCUINTPTR)pvFault, &cb);
    if (VBOX_SUCCESS(rc) && cb)
    {
        PCVBOXTSS pGuestTSS = (PVBOXTSS)pVM->selm.s.GCPtrGuestTss;
        if (    pGuestTSS->esp0 !=  pVM->selm.s.Tss.esp1
            ||  pGuestTSS->ss0  != (pVM->selm.s.Tss.ss1 & ~1)) /* undo raw-r0 */
        {
            Log(("selmgcGuestTSSWriteHandler: R0 stack: %RTsel:%VGv -> %RTsel:%VGv\n",
                 (RTSEL)(pVM->selm.s.Tss.ss1 & ~1), pVM->selm.s.Tss.esp1, (RTSEL)pGuestTSS->ss0, pGuestTSS->esp0));
            pVM->selm.s.Tss.esp1 = pGuestTSS->esp0;
            pVM->selm.s.Tss.ss1 = pGuestTSS->ss0 | 1;
            STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestTSSHandledChanged);
        }
        if (CPUMGetGuestCR4(pVM) & X86_CR4_VME)
        {
            uint32_t offIntRedirBitmap = pGuestTSS->offIoBitmap - sizeof(pVM->selm.s.Tss.IntRedirBitmap);

            /** @todo not sure how the partial case is handled; probably not allowed */
            if (   offIntRedirBitmap <= offRange 
                && offIntRedirBitmap + sizeof(pVM->selm.s.Tss.IntRedirBitmap) >= offRange + cb
                && offIntRedirBitmap + sizeof(pVM->selm.s.Tss.IntRedirBitmap) <= pVM->selm.s.cbGuestTss)
            {
                Log(("offIoBitmap=%x offIntRedirBitmap=%x cbTSS=%x\n", pGuestTSS->offIoBitmap, offIntRedirBitmap, pVM->selm.s.cbGuestTss));
                /** @todo only update the changed part. */
                for (uint32_t i = 0; i < sizeof(pVM->selm.s.Tss.IntRedirBitmap) / 8;i++)
                {
                    rc = MMGCRamRead(pVM, &pVM->selm.s.Tss.IntRedirBitmap[i * 8], (uint8_t *)pGuestTSS + offIntRedirBitmap + i * 8, 8);
                    if (VBOX_FAILURE(rc))
                    {
                        /* Shadow page table might be out of sync */
                        rc = PGMPrefetchPage(pVM, (RTGCPTR)(RTRCUINTPTR)((uint8_t *)pGuestTSS + offIntRedirBitmap + i*8));
                        if (VBOX_FAILURE(rc))
                        {
                            AssertMsg(rc == VINF_SUCCESS, ("PGMPrefetchPage %VGv failed with %Vrc\n", (uint8_t *)pGuestTSS + offIntRedirBitmap + i*8, rc));
                            break;
                        }
                        rc = MMGCRamRead(pVM, &pVM->selm.s.Tss.IntRedirBitmap[i * 8], (uint8_t *)pGuestTSS + offIntRedirBitmap + i * 8, 8);
                    }
                    AssertMsg(rc == VINF_SUCCESS, ("MMGCRamRead %VGv failed with %Vrc\n", (uint8_t *)pGuestTSS + offIntRedirBitmap + i * 8, rc));
                }
                STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestTSSRedir);
            }
        }
        STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestTSSHandled);
    }
    else
    {
        Assert(VBOX_FAILURE(rc));
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
        STAM_COUNTER_INC(&pVM->selm.s.StatGCWriteGuestTSSUnhandled);
        if (rc == VERR_EM_INTERPRETER)
            rc = VINF_EM_RAW_EMULATE_INSTR_TSS_FAULT;
    }
    return rc;
}



/**
 * \#PF Virtual Handler callback for Guest write access to the VBox shadow GDT.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
SELMGCDECL(int) selmgcShadowGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: selmgcShadowGDTWriteHandler: eip=%08X pvFault=%08X pvRange=%08X\r\n", pRegFrame->eip, pvFault, pvRange));
    return VERR_SELM_SHADOW_GDT_WRITE;
}

/**
 * \#PF Virtual Handler callback for Guest write access to the VBox shadow LDT.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
SELMGCDECL(int) selmgcShadowLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: selmgcShadowLDTWriteHandler: eip=%08X pvFault=%08X pvRange=%08X\r\n", pRegFrame->eip, pvFault, pvRange));
    Assert(pvFault >= pVM->selm.s.GCPtrLdt && (uintptr_t)pvFault < (uintptr_t)pVM->selm.s.GCPtrLdt + 65536 + PAGE_SIZE);
    return VERR_SELM_SHADOW_LDT_WRITE;
}

/**
 * \#PF Virtual Handler callback for Guest write access to the VBox shadow TSS.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         VM Handle.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this's the EIP, if not it's pvFault.)
 */
SELMGCDECL(int) selmgcShadowTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: selmgcShadowTSSWriteHandler: eip=%08X pvFault=%08X pvRange=%08X\r\n", pRegFrame->eip, pvFault, pvRange));
    return VERR_SELM_SHADOW_TSS_WRITE;
}


/**
 * Gets ss:esp for ring1 in main Hypervisor's TSS.
 *
 * @returns VBox status code.
 * @param   pVM     VM Handle.
 * @param   pSS     Ring1 SS register value.
 * @param   pEsp    Ring1 ESP register value.
 */
SELMGCDECL(int) SELMGCGetRing1Stack(PVM pVM, uint32_t *pSS, uint32_t *pEsp)
{
    if (pVM->selm.s.fSyncTSSRing0Stack)
    {
        RCPTRTYPE(uint8_t *) GCPtrTss = (RCPTRTYPE(uint8_t *))pVM->selm.s.GCPtrGuestTss;
        int     rc;
        VBOXTSS tss;

        Assert(pVM->selm.s.GCPtrGuestTss && pVM->selm.s.cbMonitoredGuestTss);

#ifdef IN_GC
        bool    fTriedAlready = false;

l_tryagain:
        rc  = MMGCRamRead(pVM, &tss.ss0,  GCPtrTss + RT_OFFSETOF(VBOXTSS, ss0), sizeof(tss.ss0));
        rc |= MMGCRamRead(pVM, &tss.esp0, GCPtrTss + RT_OFFSETOF(VBOXTSS, esp0), sizeof(tss.esp0));
  #ifdef DEBUG
        rc |= MMGCRamRead(pVM, &tss.offIoBitmap, GCPtrTss + RT_OFFSETOF(VBOXTSS, offIoBitmap), sizeof(tss.offIoBitmap));
  #endif

        if (VBOX_FAILURE(rc))
        {
            if (!fTriedAlready)
            {
                /* Shadow page might be out of sync. Sync and try again */
                /** @todo might cross page boundary */
                fTriedAlready = true;
                rc = PGMPrefetchPage(pVM, (RTGCPTR)(RTRCUINTPTR)GCPtrTss);
                if (rc != VINF_SUCCESS)
                    return rc;
                goto l_tryagain;
            }
            AssertMsgFailed(("Unable to read TSS structure at %08X\n", GCPtrTss));
            return rc;
        }

#else /* !IN_GC */
        /* Reading too much. Could be cheaper than two seperate calls though. */
        rc = PGMPhysReadGCPtr(pVM, &tss, GCPtrTss, sizeof(VBOXTSS));
        if (VBOX_FAILURE(rc))
        {
            AssertReleaseMsgFailed(("Unable to read TSS structure at %08X\n", GCPtrTss));
            return rc;
        }
#endif /* !IN_GC */

#ifdef LOG_ENABLED
        uint32_t ssr0  = pVM->selm.s.Tss.ss1;
        uint32_t espr0 = pVM->selm.s.Tss.esp1;
        ssr0 &= ~1;

        if (ssr0 != tss.ss0 || espr0 != tss.esp0)
            Log(("SELMGetRing1Stack: Updating TSS ring 0 stack to %04X:%08X\n", tss.ss0, tss.esp0));

        Log(("offIoBitmap=%#x\n", tss.offIoBitmap));
#endif
        /* Update our TSS structure for the guest's ring 1 stack */
        SELMSetRing1Stack(pVM, tss.ss0 | 1, (RTGCPTR32)tss.esp0);
        pVM->selm.s.fSyncTSSRing0Stack = false;
    }

    *pSS  = pVM->selm.s.Tss.ss1;
    *pEsp = pVM->selm.s.Tss.esp1;

    return VINF_SUCCESS;
}
