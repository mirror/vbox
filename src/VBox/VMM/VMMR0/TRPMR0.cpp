/* $Id$ */
/** @file
 * TRPM - The Trap Monitor - HC Ring 0
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
#include "TRPMInternal.h"
#include <VBox/vm.h>
#include <VBox/vmm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>


/**
 * Dispatches an interrupt that arrived while we were in the guest context.
 *
 * @param   pVM     The VM handle.
 * @remark  Must be called with interrupts disabled.
 */
VMMR0DECL(void) TRPMR0DispatchHostInterrupt(PVM pVM)
{
    PVMCPU pVCpu = VMMGetCpu0(pVM);
    RTUINT uActiveVector = pVCpu->trpm.s.uActiveVector;

    pVCpu->trpm.s.uActiveVector = ~0;
    AssertMsgReturnVoid(uActiveVector < 256, ("uActiveVector=%#x is invalid! (More assertions to come, please enjoy!)\n", uActiveVector));

#ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    /*
     * Check if we're in long mode or not.
     */
    if (    (ASMCpuId_EDX(0x80000001) & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE)
        &&  (ASMRdMsr(MSR_K6_EFER) & MSR_K6_EFER_LMA))
    {
        trpmR0DispatchHostInterruptSimple(uActiveVector);
        return;
    }
#endif

    /*
     * Get the handler pointer (16:32 ptr) / (16:48 ptr).
     */
    RTIDTR      Idtr;
    ASMGetIDTR(&Idtr);
#if HC_ARCH_BITS == 32
    PVBOXIDTE   pIdte = &((PVBOXIDTE)Idtr.pIdt)[uActiveVector];
#else
    PVBOXIDTE   pIdte = &((PVBOXIDTE)Idtr.pIdt)[uActiveVector * 2];
#endif
    AssertMsgReturnVoid(pIdte->Gen.u1Present, ("The IDT entry (%d) is not present!\n", uActiveVector));
    AssertMsgReturnVoid(    pIdte->Gen.u3Type1 == VBOX_IDTE_TYPE1
                        ||  pIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_INT_32,
                        ("The IDT entry (%d) is not 32-bit int gate! type1=%#x type2=%#x\n",
                         uActiveVector, pIdte->Gen.u3Type1, pIdte->Gen.u5Type2));
#if HC_ARCH_BITS == 32
    RTFAR32   pfnHandler;
    pfnHandler.off = VBOXIDTE_OFFSET(*pIdte);
    pfnHandler.sel = pIdte->Gen.u16SegSel;

    const RTR0UINTREG uRSP = ~(RTR0UINTREG)0;

#else /* 64-bit: */
    RTFAR64   pfnHandler;
    pfnHandler.off = VBOXIDTE_OFFSET(*pIdte);
    pfnHandler.off |= (uint64_t)(*(uint32_t *)(pIdte + 1)) << 32; //cleanup!
    pfnHandler.sel = pIdte->Gen.u16SegSel;

    RTR0UINTREG uRSP = ~(RTR0UINTREG)0;
    if (pIdte->au32[1] & 0x7 /*IST*/)
    {
        trpmR0DispatchHostInterruptSimple(uActiveVector);
        return;
    }

#endif

    /*
     * Dispatch it.
     */
    trpmR0DispatchHostInterrupt(pfnHandler.off, pfnHandler.sel, uRSP);
}

