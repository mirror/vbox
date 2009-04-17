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
#include <VBox/vmm.h>
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
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    Log2(("GDT %04X LDTR=%04X\n", iGDTEntry, CPUMGetGuestLDTR(pVCpu)));

    /*
     * Validate the offset.
     */
    VBOXGDTR GdtrGuest;
    CPUMGetGuestGDTR(pVCpu, &GdtrGuest);
    unsigned offEntry = iGDTEntry * sizeof(X86DESC);
    if (    iGDTEntry >= SELM_GDT_ELEMENTS
        ||  offEntry > GdtrGuest.cbGdt)
        return VINF_EM_RAW_EMULATE_INSTR_GDT_FAULT;

    /*
     * Read the guest descriptor.
     */
    X86DESC Desc;
    int rc = MMGCRamRead(pVM, &Desc, (uint8_t *)GdtrGuest.pGdt + offEntry, sizeof(X86DESC));
    if (RT_FAILURE(rc))
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
            Log(("selmGCSyncGDTEntry: Sel=%d Desc=%.8Rhxs: detected conflict!!\n", Sel, &Desc));
            return VINF_SELM_SYNC_GDT;
        }
        Log(("selmGCSyncGDTEntry: Sel=%d Desc=%.8Rhxs: potential conflict (still not present)!\n", Sel, &Desc));

        /* Note: we can't continue below or else we'll change the shadow descriptor!! */
        /* When the guest makes the selector present, then we'll do a GDT sync. */
        return VINF_SUCCESS;
    }

    /*
     * Code and data selectors are generally 1:1, with the
     * 'little' adjustment we do for DPL 0 selectors.
     */
    PX86DESC   pShadowDescr = &pVM->selm.s.paGdtRC[iGDTEntry];
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
    //Log(("O: base=%08X limit=%08X attr=%04X\n", X86DESC_BASE(*pShadowDescr)), X86DESC_LIMIT(*pShadowDescr), (pShadowDescr->au32[1] >> 8) & 0xFFFF ));
    //Log(("N: base=%08X limit=%08X attr=%04X\n", X86DESC_BASE(Desc)), X86DESC_LIMIT(Desc), (Desc.au32[1] >> 8) & 0xFFFF ));
    *pShadowDescr = Desc;

    /* Check if we change the LDT selector */
    if (Sel == CPUMGetGuestLDTR(pVCpu)) /** @todo this isn't correct in two(+) ways! 1. It shouldn't be done until the LDTR is reloaded. 2. It caused the next instruction to be emulated.  */
    {
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_LDT);
        return VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT;
    }

#ifdef LOG_ENABLED
    if (Sel == (pRegFrame->cs & X86_SEL_MASK))
        Log(("GDT write to selector in CS register %04X\n", pRegFrame->cs));
    else if (Sel == (pRegFrame->ds & X86_SEL_MASK))
        Log(("GDT write to selector in DS register %04X\n", pRegFrame->ds));
    else if (Sel == (pRegFrame->es & X86_SEL_MASK))
        Log(("GDT write to selector in ES register %04X\n", pRegFrame->es));
    else if (Sel == (pRegFrame->fs & X86_SEL_MASK))
        Log(("GDT write to selector in FS register %04X\n", pRegFrame->fs));
    else if (Sel == (pRegFrame->gs & X86_SEL_MASK))
        Log(("GDT write to selector in GS register %04X\n", pRegFrame->gs));
    else if (Sel == (pRegFrame->ss & X86_SEL_MASK))
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
VMMRCDECL(int) selmRCGuestGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    LogFlow(("selmRCGuestGDTWriteHandler errcode=%x fault=%RGv offRange=%08x\n", (uint32_t)uErrorCode, pvFault, offRange));

    /*
     * First check if this is the LDT entry.
     * LDT updates are problemous since an invalid LDT entry will cause trouble during worldswitch.
     */
    int rc;
    if (CPUMGetGuestLDTR(pVCpu) / sizeof(X86DESC) == offRange / sizeof(X86DESC))
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
        rc = EMInterpretInstruction(pVM, pVCpu, pRegFrame, (RTGCPTR)(RTRCUINTPTR)pvFault, &cb);
        if (RT_SUCCESS(rc) && cb)
        {
            unsigned iGDTE1 = offRange / sizeof(X86DESC);
            int rc2 = selmGCSyncGDTEntry(pVM, pRegFrame, iGDTE1);
            if (rc2 == VINF_SUCCESS)
            {
                Assert(cb);
                unsigned iGDTE2 = (offRange + cb - 1) / sizeof(X86DESC);
                if (iGDTE1 != iGDTE2)
                    rc2 = selmGCSyncGDTEntry(pVM, pRegFrame, iGDTE2);
                if (rc2 == VINF_SUCCESS)
                {
                    STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestGDTHandled);
                    return rc;
                }
            }
            if (rc == VINF_SUCCESS || RT_FAILURE(rc2))
                rc = rc2;
        }
        else
        {
            Assert(RT_FAILURE(rc));
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
    STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestGDTUnhandled);
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
VMMRCDECL(int) selmRCGuestLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    /** @todo To be implemented. */
    ////LogCom(("selmRCGuestLDTWriteHandler: eip=%08X pvFault=%RGv pvRange=%RGv\r\n", pRegFrame->eip, pvFault, pvRange));

    VM_FF_SET(pVM, VM_FF_SELM_SYNC_LDT);
    STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestLDT);
    return VINF_EM_RAW_EMULATE_INSTR_LDT_FAULT;
}


/**
 * Read wrapper used by selmRCGuestTSSWriteHandler.
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         The VM handle
 * @param   pvDst       Where to put the bits we read.
 * @param   pvSrc       Guest address to read from.
 * @param   cb          The number of bytes to read.
 */
DECLINLINE(int) selmRCReadTssBits(PVM pVM, void *pvDst, void const *pvSrc, size_t cb)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    int rc = MMGCRamRead(pVM, pvDst, (void *)pvSrc, cb);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    /** @todo use different fallback?    */
    rc = PGMPrefetchPage(pVCpu, (uintptr_t)pvSrc);
    AssertMsg(rc == VINF_SUCCESS, ("PGMPrefetchPage %p failed with %Rrc\n", &pvSrc, rc));
    if (rc == VINF_SUCCESS)
    {
        rc = MMGCRamRead(pVM, pvDst, (void *)pvSrc, cb);
        AssertMsg(rc == VINF_SUCCESS, ("MMGCRamRead %p failed with %Rrc\n", &pvSrc, rc));
    }
    return rc;
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
VMMRCDECL(int) selmRCGuestTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    LogFlow(("selmRCGuestTSSWriteHandler errcode=%x fault=%RGv offRange=%08x\n", (uint32_t)uErrorCode, pvFault, offRange));

    /*
     * Try emulate the access.
     */
    uint32_t cb;
    int rc = EMInterpretInstruction(pVM, pVCpu, pRegFrame, (RTGCPTR)(RTRCUINTPTR)pvFault, &cb);
    if (RT_SUCCESS(rc) && cb)
    {
        rc = VINF_SUCCESS;

        /*
         * If it's on the same page as the esp0 and ss0 fields or actually one of them,
         * then check if any of these has changed.
         */
        PCVBOXTSS pGuestTss = (PVBOXTSS)pVM->selm.s.GCPtrGuestTss;
        if (    PAGE_ADDRESS(&pGuestTss->esp0) == PAGE_ADDRESS(&pGuestTss->padding_ss0)
            &&  PAGE_ADDRESS(&pGuestTss->esp0) == PAGE_ADDRESS((uint8_t *)pGuestTss + offRange)
            &&  (    pGuestTss->esp0 !=  pVM->selm.s.Tss.esp1
                 ||  pGuestTss->ss0  != (pVM->selm.s.Tss.ss1 & ~1)) /* undo raw-r0 */
           )
        {
            Log(("selmRCGuestTSSWriteHandler: R0 stack: %RTsel:%RGv -> %RTsel:%RGv\n",
                 (RTSEL)(pVM->selm.s.Tss.ss1 & ~1), (RTGCPTR)pVM->selm.s.Tss.esp1, (RTSEL)pGuestTss->ss0, (RTGCPTR)pGuestTss->esp0));
            pVM->selm.s.Tss.esp1 = pGuestTss->esp0;
            pVM->selm.s.Tss.ss1  = pGuestTss->ss0 | 1;
            STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestTSSHandledChanged);
        }
        /* Handle misaligned TSS in a safe manner (just in case). */
        else if (   offRange >= RT_UOFFSETOF(VBOXTSS, esp0)
                 && offRange < RT_UOFFSETOF(VBOXTSS, padding_ss0))
        {
            struct
            {
                uint32_t esp0;
                uint16_t ss0;
                uint16_t padding_ss0;
            } s;
            AssertCompileSize(s, 8);
            rc = selmRCReadTssBits(pVM, &s, &pGuestTss->esp0, sizeof(s));
            if (    rc == VINF_SUCCESS
                &&  (    s.esp0 !=  pVM->selm.s.Tss.esp1
                     ||  s.ss0  != (pVM->selm.s.Tss.ss1 & ~1)) /* undo raw-r0 */
               )
            {
                Log(("selmRCGuestTSSWriteHandler: R0 stack: %RTsel:%RGv -> %RTsel:%RGv [x-page]\n",
                     (RTSEL)(pVM->selm.s.Tss.ss1 & ~1), (RTGCPTR)pVM->selm.s.Tss.esp1, (RTSEL)s.ss0, (RTGCPTR)s.esp0));
                pVM->selm.s.Tss.esp1 = s.esp0;
                pVM->selm.s.Tss.ss1  = s.ss0 | 1;
                STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestTSSHandledChanged);
            }
        }

        /*
         * If VME is enabled we need to check if the interrupt redirection bitmap
         * needs updating.
         */
        if (    offRange >= RT_UOFFSETOF(VBOXTSS, offIoBitmap)
            &&  (CPUMGetGuestCR4(pVCpu) & X86_CR4_VME))
        {
            if (offRange - RT_UOFFSETOF(VBOXTSS, offIoBitmap) < sizeof(pGuestTss->offIoBitmap))
            {
                uint16_t offIoBitmap = pGuestTss->offIoBitmap;
                if (offIoBitmap != pVM->selm.s.offGuestIoBitmap)
                {
                    Log(("TSS offIoBitmap changed: old=%#x new=%#x -> resync in ring-3\n", pVM->selm.s.offGuestIoBitmap, offIoBitmap));
                    VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
                    VM_FF_SET(pVM, VM_FF_TO_R3);
                }
                else
                    Log(("TSS offIoBitmap: old=%#x new=%#x [unchanged]\n", pVM->selm.s.offGuestIoBitmap, offIoBitmap));
            }
            else
            {
                /** @todo not sure how the partial case is handled; probably not allowed */
                uint32_t offIntRedirBitmap = pVM->selm.s.offGuestIoBitmap - sizeof(pVM->selm.s.Tss.IntRedirBitmap);
                if (   offIntRedirBitmap <= offRange
                    && offIntRedirBitmap + sizeof(pVM->selm.s.Tss.IntRedirBitmap) >= offRange + cb
                    && offIntRedirBitmap + sizeof(pVM->selm.s.Tss.IntRedirBitmap) <= pVM->selm.s.cbGuestTss)
                {
                    Log(("TSS IntRedirBitmap Changed: offIoBitmap=%x offIntRedirBitmap=%x cbTSS=%x offRange=%x cb=%x\n",
                         pVM->selm.s.offGuestIoBitmap, offIntRedirBitmap, pVM->selm.s.cbGuestTss, offRange, cb));

                    /** @todo only update the changed part. */
                    for (uint32_t i = 0; i < sizeof(pVM->selm.s.Tss.IntRedirBitmap) / 8; i++)
                    {
                        rc = selmRCReadTssBits(pVM, &pVM->selm.s.Tss.IntRedirBitmap[i * 8],
                                               (uint8_t *)pGuestTss + offIntRedirBitmap + i * 8, 8);
                        if (rc != VINF_SUCCESS)
                            break;
                    }
                    STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestTSSRedir);
                }
            }
        }

        /* Return to ring-3 for a full resync if any of the above fails... (?) */
        if (rc != VINF_SUCCESS)
        {
            VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
            VM_FF_SET(pVM, VM_FF_TO_R3);
            if (RT_SUCCESS(rc))
                rc = VINF_SUCCESS;
        }

        STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestTSSHandled);
    }
    else
    {
        Assert(RT_FAILURE(rc));
        VM_FF_SET(pVM, VM_FF_SELM_SYNC_TSS);
        STAM_COUNTER_INC(&pVM->selm.s.StatRCWriteGuestTSSUnhandled);
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
VMMRCDECL(int) selmRCShadowGDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: selmRCShadowGDTWriteHandler: eip=%08X pvFault=%RGv pvRange=%RGv\r\n", pRegFrame->eip, pvFault, pvRange));
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
VMMRCDECL(int) selmRCShadowLDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: selmRCShadowLDTWriteHandler: eip=%08X pvFault=%RGv pvRange=%RGv\r\n", pRegFrame->eip, pvFault, pvRange));
    Assert((RTRCPTR)pvFault >= pVM->selm.s.pvLdtRC && (RTRCUINTPTR)pvFault < (RTRCUINTPTR)pVM->selm.s.pvLdtRC + 65536 + PAGE_SIZE);
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
VMMRCDECL(int) selmRCShadowTSSWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: selmRCShadowTSSWriteHandler: eip=%08X pvFault=%RGv pvRange=%RGv\r\n", pRegFrame->eip, pvFault, pvRange));
    return VERR_SELM_SHADOW_TSS_WRITE;
}

