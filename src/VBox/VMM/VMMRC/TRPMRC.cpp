/* $Id$ */
/** @file
 * TRPM - The Trap Monitor, Guest Context
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_TRPM
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/vmm.h>
#include "TRPMInternal.h"
#include <VBox/vmm/vm.h>

#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/x86.h>
#include <VBox/log.h>
#include <VBox/vmm/selm.h>



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
 * @param   pVM         Pointer to the VM.
 * @param   iTrap       Trap number to install handler [0..255].
 * @param   pfnHandler  Pointer to the handler. Use NULL for uninstalling the handler.
 */
VMMRCDECL(int) TRPMGCSetTempHandler(PVM pVM, unsigned iTrap, PFNTRPMGCTRAPHANDLER pfnHandler)
{
    /*
     * Validate input.
     */
    if (iTrap >= RT_ELEMENTS(pVM->trpm.s.aTmpTrapHandlers))
    {
        AssertMsgFailed(("Trap handler iTrap=%u is out of range!\n", iTrap));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Install handler.
     */
    pVM->trpm.s.aTmpTrapHandlers[iTrap] = (RTRCPTR)(uintptr_t)pfnHandler;
    return VINF_SUCCESS;
}


/**
 * Return to host context from a hypervisor trap handler.
 *
 * This function will *never* return.
 * It will also reset any traps that are pending.
 *
 * @param   pVM     Pointer to the VM.
 * @param   rc      The return code for host context.
 */
VMMRCDECL(void) TRPMGCHyperReturnToHost(PVM pVM, int rc)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);

    LogFlow(("TRPMGCHyperReturnToHost: rc=%Rrc\n", rc));
    TRPMResetTrap(pVCpu);
    VMMGCGuestToHost(pVM, rc);
    AssertReleaseFailed();
}


/**
 * \#PF Virtual Handler callback for Guest write access to the Guest's own current IDT.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         Pointer to the VM.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this is the EIP, if not it's pvFault.)
 */
VMMRCDECL(int) trpmRCGuestIDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    PVMCPU      pVCpu = VMMGetCpu0(pVM);
    uint16_t    cbIDT;
    RTGCPTR     GCPtrIDT    = (RTGCPTR)CPUMGetGuestIDTR(pVCpu, &cbIDT);
#ifdef VBOX_STRICT
    RTGCPTR     GCPtrIDTEnd = (RTGCPTR)((RTGCUINTPTR)GCPtrIDT + cbIDT + 1);
#endif
    uint32_t    iGate       = ((RTGCUINTPTR)pvFault - (RTGCUINTPTR)GCPtrIDT)/sizeof(VBOXIDTE);

    AssertMsg(offRange < (uint32_t)cbIDT+1, ("pvFault=%RGv GCPtrIDT=%RGv-%RGv pvRange=%RGv\n", pvFault, GCPtrIDT, GCPtrIDTEnd, pvRange));
    Assert((RTGCPTR)(RTRCUINTPTR)pvRange == GCPtrIDT);
    NOREF(uErrorCode);

#if 0
    /* Note! this causes problems in Windows XP as instructions following the update can be dangerous (str eax has been seen) */
    /* Note! not going back to ring 3 could make the code scanner miss them. */
    /* Check if we can handle the write here. */
    if (     iGate != 3                                         /* Gate 3 is handled differently; could do it here as well, but let ring 3 handle this case for now. */
        &&  !ASMBitTest(&pVM->trpm.s.au32IdtPatched[0], iGate)) /* Passthru gates need special attention too. */
    {
        uint32_t cb;
        int rc = EMInterpretInstructionEx(pVM, pVCpu, pRegFrame, pvFault, &cb);
        if (RT_SUCCESS(rc) && cb)
        {
            uint32_t iGate1 = (offRange + cb - 1)/sizeof(VBOXIDTE);

            Log(("trpmRCGuestIDTWriteHandler: write to gate %x (%x) offset %x cb=%d\n", iGate, iGate1, offRange, cb));

            trpmClearGuestTrapHandler(pVM, iGate);
            if (iGate != iGate1)
                trpmClearGuestTrapHandler(pVM, iGate1);

            STAM_COUNTER_INC(&pVM->trpm.s.StatRCWriteGuestIDTHandled);
            return VINF_SUCCESS;
        }
    }
#else
    NOREF(iGate);
#endif

    Log(("trpmRCGuestIDTWriteHandler: eip=%RGv write to gate %x offset %x\n", pRegFrame->eip, iGate, offRange));

    /** @todo Check which IDT entry and keep the update cost low in TRPMR3SyncIDT() and CSAMCheckGates(). */
    VMCPU_FF_SET(pVCpu, VMCPU_FF_TRPM_SYNC_IDT);

    STAM_COUNTER_INC(&pVM->trpm.s.StatRCWriteGuestIDTFault);
    return VINF_EM_RAW_EMULATE_INSTR_IDT_FAULT;
}


/**
 * \#PF Virtual Handler callback for Guest write access to the VBox shadow IDT.
 *
 * @returns VBox status code (appropriate for trap handling and GC return).
 * @param   pVM         Pointer to the VM.
 * @param   uErrorCode   CPU Error code.
 * @param   pRegFrame   Trap register frame.
 * @param   pvFault     The fault address (cr2).
 * @param   pvRange     The base address of the handled virtual range.
 * @param   offRange    The offset of the access into this range.
 *                      (If it's a EIP range this is the EIP, if not it's pvFault.)
 */
VMMRCDECL(int) trpmRCShadowIDTWriteHandler(PVM pVM, RTGCUINT uErrorCode, PCPUMCTXCORE pRegFrame, RTGCPTR pvFault, RTGCPTR pvRange, uintptr_t offRange)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);
    LogRel(("FATAL ERROR: trpmRCShadowIDTWriteHandler: eip=%08X pvFault=%RGv pvRange=%08RGv\r\n", pRegFrame->eip, pvFault, pvRange));
    NOREF(uErrorCode); NOREF(offRange);

    /*
     * If we ever get here, then the guest has *probably* executed an SIDT
     * instruction that we failed to patch.  In theory this could be very bad,
     * but there are nasty applications out there that install device drivers
     * that mess with the guest's IDT.  In those cases, it's quite ok to simply
     * ignore the writes and pretend success.
     *
     * Another posibility is that the guest is touching some page memory and
     * it having nothing to do with our IDT or anything like that, just a
     * potential conflict that we didn't discover in time.
     */
    DISSTATE Dis;
    int rc = EMInterpretDisasCurrent(pVM, pVCpu, &Dis, NULL);
    if (rc == VINF_SUCCESS)
    {
        /* Just ignore the write. */
        pRegFrame->eip += Dis.cbInstr;
        return VINF_SUCCESS;
    }

    return VERR_TRPM_SHADOW_IDT_WRITE;
}

