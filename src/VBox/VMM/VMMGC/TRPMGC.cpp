/* $Id$ */
/** @file
 * TRPM - The Trap Monitor, Guest Context
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
#define LOG_GROUP LOG_GROUP_TRPM
#include <VBox/trpm.h>
#include <VBox/cpum.h>
#include <VBox/vmm.h>
#include "TRPMInternal.h"
#include <VBox/vm.h>

#include <VBox/err.h>
#include <VBox/x86.h>
#include <VBox/em.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <VBox/log.h>
#include <VBox/selm.h>



/**
 * Arms a temporary trap handler for traps in Hypervisor code.
 *
 * The operation is similar to a System V signal handler. I.e. when the handler
 * is called it is first set to default action. So, if you need to handler more
 * than one trap, you must reinstall the handler.
 *
 * To uninstall the temporary handler, call this function with pfnHandler set to NULL.
 *
 * @returns VBox status.
 * @param   pVM         VM handle.
 * @param   iTrap       Trap number to install handler [0..255].
 * @param   pfnHandler  Pointer to the handler. Use NULL for uninstalling the handler.
 */
TRPMGCDECL(int) TRPMGCSetTempHandler(PVM pVM, unsigned iTrap, PFNTRPMGCTRAPHANDLER pfnHandler)
{
    /*
     * Validate input.
     */
    if (iTrap >= ELEMENTS(pVM->trpm.s.aTmpTrapHandlers))
    {
        AssertMsgFailed(("Trap handler iTrap=%u is out of range!\n", iTrap));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Install handler.
     */
    pVM->trpm.s.aTmpTrapHandlers[iTrap] = (RTRCPTR)pfnHandler;
    return VINF_SUCCESS;
}


/**
 * Return to host context from a hypervisor trap handler.
 *
 * This function will *never* return.
 * It will also reset any traps that are pending.
 *
 * @param   pVM     The VM handle.
 * @param   rc      The return code for host context.
 */
TRPMGCDECL(void) TRPMGCHyperReturnToHost(PVM pVM, int rc)
{
    LogFlow(("TRPMGCHyperReturnToHost: rc=%Vrc\n", rc));
    TRPMResetTrap(pVM);
    CPUMHyperSetCtxCore(pVM, NULL);
    VMMGCGuestToHost(pVM, rc);
    AssertReleaseFailed();
}


/**
 * \#PF Virtual Handler callback for Guest write access to the Guest's own current IDT.
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
TRPMGCDECL(int) trpmgcGuestIDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    uint16_t    cbIDT;
    RTGCPTR     GCPtrIDT    = (RTGCPTR)CPUMGetGuestIDTR(pVM, &cbIDT);
#ifdef VBOX_STRICT
    RTGCPTR     GCPtrIDTEnd = (RTGCPTR)((RTGCUINTPTR)GCPtrIDT + cbIDT + 1);
#endif
    uint32_t    iGate       = ((RTGCUINTPTR)pvFault - (RTGCUINTPTR)GCPtrIDT)/sizeof(VBOXIDTE);

    AssertMsg(offRange < (uint32_t)cbIDT+1, ("pvFault=%VGv GCPtrIDT=%VGv-%VGv pvRange=%VGv\n", pvFault, GCPtrIDT, GCPtrIDTEnd, pvRange));
    Assert((RTGCPTR)(RTRCUINTPTR)pvRange == GCPtrIDT);

#if 0
    /** @note this causes problems in Windows XP as instructions following the update can be dangerous (str eax has been seen) */
    /** @note not going back to ring 3 could make the code scanner miss them. */
    /* Check if we can handle the write here. */    
    if (     iGate != 3                                         /* Gate 3 is handled differently; could do it here as well, but let ring 3 handle this case for now. */
        &&  !ASMBitTest(&pVM->trpm.s.au32IdtPatched[0], iGate)) /* Passthru gates need special attention too. */
    {
        uint32_t cb;
        int rc = EMInterpretInstruction(pVM, pRegFrame, pvFault, &cb);
        if (VBOX_SUCCESS(rc) && cb)
        {
            uint32_t iGate1 = (offRange + cb - 1)/sizeof(VBOXIDTE);

            Log(("trpmgcGuestIDTWriteHandler: write to gate %x (%x) offset %x cb=%d\n", iGate, iGate1, offRange, cb));

            trpmClearGuestTrapHandler(pVM, iGate);
            if (iGate != iGate1)
                trpmClearGuestTrapHandler(pVM, iGate1);

            STAM_COUNTER_INC(&pVM->trpm.s.StatGCWriteGuestIDTHandled);
            return VINF_SUCCESS;
        }
    }
#else
    NOREF(iGate);
#endif

    Log(("trpmgcGuestIDTWriteHandler: eip=%VGv write to gate %x offset %x\n", pRegFrame->eip, iGate, offRange));

    /** @todo Check which IDT entry and keep the update cost low in TRPMR3SyncIDT() and CSAMCheckGates(). */
    VM_FF_SET(pVM, VM_FF_TRPM_SYNC_IDT);

    STAM_COUNTER_INC(&pVM->trpm.s.StatGCWriteGuestIDTFault);
    return VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT;
}


/**
 * \#PF Virtual Handler callback for Guest write access to the VBox shadow IDT.
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
TRPMGCDECL(int) trpmgcShadowIDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, void *pvFault, void *pvRange, uintptr_t offRange)
{
    LogRel(("FATAL ERROR: trpmgcShadowIDTWriteHandler: eip=%08X pvFault=%08X pvRange=%08X\r\n", pRegFrame->eip, pvFault, pvRange));

    /* If we ever get here, then the guest has executed an sidt instruction that we failed to patch. In theory this could be very bad, but
     * there are nasty applications out there that install device drivers that mess with the guest's IDT. In those cases, it's quite ok
     * to simply ignore the writes and pretend success.
     */
    RTGCPTR PC;
    int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid, (RTGCPTR)pRegFrame->eip, &PC);
    if (rc == VINF_SUCCESS)
    {
        DISCPUSTATE Cpu;
        uint32_t    cbOp;
        rc = EMInterpretDisasOneEx(pVM, (RTGCUINTPTR)PC, pRegFrame, &Cpu, &cbOp);
        if (rc == VINF_SUCCESS)
        {
            /* Just ignore the write. */
            pRegFrame->eip += Cpu.opsize;
            return VINF_SUCCESS;
        }
    }

    return VERR_TRPM_SHADOW_IDT_WRITE;
}

