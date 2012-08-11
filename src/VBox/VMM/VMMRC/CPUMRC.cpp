/* $Id$ */
/** @file
 * CPUM - Raw-mode Context Code.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
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
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/trpm.h>
#include "CPUMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <VBox/log.h>


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
RT_C_DECLS_BEGIN /* addressed from asm (not called so no DECLASM). */
DECLCALLBACK(int) cpumRCHandleNPAndGP(PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser);
RT_C_DECLS_END


/**
 * Deal with traps occurring during segment loading and IRET when resuming guest
 * context execution.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pRegFrame   The register frame.
 * @param   uUser       User argument. In this case a combination of the
 *                      CPUM_HANDLER_* \#defines.
 */
DECLCALLBACK(int) cpumRCHandleNPAndGP(PVM pVM, PCPUMCTXCORE pRegFrame, uintptr_t uUser)
{
    Log(("********************************************************\n"));
    Log(("cpumRCHandleNPAndGP: eip=%RX32 uUser=%#x\n", pRegFrame->eip, uUser));
    Log(("********************************************************\n"));

    /*
     * Take action based on what's happened.
     */
    switch (uUser & CPUM_HANDLER_TYPEMASK)
    {
        case CPUM_HANDLER_GS:
        case CPUM_HANDLER_DS:
        case CPUM_HANDLER_ES:
        case CPUM_HANDLER_FS:
            TRPMGCHyperReturnToHost(pVM, VINF_EM_RAW_STALE_SELECTOR);
            break;

        case CPUM_HANDLER_IRET:
            TRPMGCHyperReturnToHost(pVM, VINF_EM_RAW_IRET_TRAP);
            break;
    }

    AssertMsgFailed(("uUser=%#x eip=%#x\n", uUser, pRegFrame->eip));
    return VERR_TRPM_DONT_PANIC;
}


/**
 * Called by TRPM and CPUM assembly code to make sure the guest state is
 * ready for execution.
 *
 * @param   pVM                 The VM handle.
 */
DECLASM(void) CPUMRCAssertPreExecutionSanity(PVM pVM)
{
    /*
     * Check some important assumptions before resuming guest execution.
     */
    PVMCPU         pVCpu     = VMMGetCpu0(pVM);
    PCCPUMCTX      pCtx      = &pVCpu->cpum.s.Guest;
    uint8_t  const uRawCpl   = CPUMGetGuestCPL(pVCpu);
    uint32_t const u32EFlags = CPUMRawGetEFlags(pVCpu);
    bool     const fPatch    = PATMIsPatchGCAddr(pVM, pCtx->eip);
    AssertMsg(pCtx->eflags.Bits.u1IF,                ("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
    AssertMsg(pCtx->eflags.Bits.u2IOPL < RT_MAX(uRawCpl, 1U),
                                                     ("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
    if (!(u32EFlags & X86_EFL_VM))
    {
        AssertMsg((u32EFlags & X86_EFL_IF) || fPatch,("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
        AssertMsg((pCtx->cs.Sel & X86_SEL_RPL) > 0,  ("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
        AssertMsg((pCtx->ss.Sel & X86_SEL_RPL) > 0,  ("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
    }
    AssertMsg(CPUMIsGuestInRawMode(pVCpu),           ("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
Log2(("cs:eip=%04x:%08x ss:esp=%04x:%08x cpl=%u raw/efl=%#x/%#x%s\n", pCtx->cs.Sel, pCtx->eip, pCtx->ss.Sel, pCtx->esp, uRawCpl, u32EFlags, pCtx->eflags.u, fPatch ? " patch" : ""));
}
