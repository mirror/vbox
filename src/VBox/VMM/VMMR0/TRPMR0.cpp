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
TRPMR0DECL(void) TRPMR0DispatchHostInterrupt(PVM pVM)
{
    RTUINT uActiveVector = pVM->trpm.s.uActiveVector;
    pVM->trpm.s.uActiveVector = ~0;
    AssertMsgReturnVoid(uActiveVector < 256, ("uActiveVector=%#x is invalid! (More assertions to come, please enjoy!)\n", uActiveVector));

#ifdef VBOX_WITH_HYBIRD_32BIT_KERNEL
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


#ifdef VBOX_WITH_IDT_PATCHING
# ifdef VBOX_WITH_HYBIRD_32BIT_KERNEL
#  error "VBOX_WITH_HYBIRD_32BIT_KERNEL with VBOX_WITH_IDT_PATCHING isn't supported"
# endif

/**
 * Changes the VMMR0Entry() call frame and stack used by the IDT patch code
 * so that we'll dispatch an interrupt rather than returning directly to Ring-3
 * when VMMR0Entry() returns.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pvRet       Pointer to the return address of VMMR0Entry() on the stack.
 */
TRPMR0DECL(void) TRPMR0SetupInterruptDispatcherFrame(PVM pVM, void *pvRet)
{
    RTUINT uActiveVector = pVM->trpm.s.uActiveVector;
    pVM->trpm.s.uActiveVector = ~0;
    AssertMsgReturnVoid(uActiveVector < 256, ("uActiveVector=%#x is invalid! (More assertions to come, please enjoy!)\n", uActiveVector));

#if HC_ARCH_BITS == 32
    /*
     * Get the handler pointer (16:32 ptr).
     */
    RTIDTR      Idtr;
    ASMGetIDTR(&Idtr);
    PVBOXIDTE   pIdte = &((PVBOXIDTE)Idtr.pIdt)[uActiveVector];
    AssertMsgReturnVoid(pIdte->Gen.u1Present, ("The IDT entry (%d) is not present!\n", uActiveVector));
    AssertMsgReturnVoid(    pIdte->Gen.u3Type1 == VBOX_IDTE_TYPE1
                        &&  pIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_INT_32,
                        ("The IDT entry (%d) is not 32-bit int gate! type1=%#x type2=%#x\n",
                         uActiveVector, pIdte->Gen.u3Type1, pIdte->Gen.u5Type2));

    RTFAR32   pfnHandler;
    pfnHandler.off = VBOXIDTE_OFFSET(*pIdte);
    pfnHandler.sel = pIdte->Gen.u16SegSel;

    /*
     * The stack frame is as follows:
     *
     *      1c  iret frame
     *      18  fs
     *      14  ds
     *      10  es
     *       c  uArg
     *       8  uOperation
     *       4  pVM
     *       0  return address (pvRet points here)
     *
     * We'll change the stackframe so that we will not return
     * to the caller but to a interrupt dispatcher. We'll also
     * setup the frame so that ds and es are moved to give room
     * to a far return (to the handler).
     */
    unsigned *pau = (unsigned *)pvRet;
    pau[0] = (unsigned)trpmR0InterruptDispatcher; /* new return address */
    pau[3] = pau[6];                    /* uArg = fs */
    pau[2] = pau[5];                    /* uOperation = ds */
    pau[5] = pfnHandler.off;            /* ds = retf off */
    pau[6] = pfnHandler.sel;            /* fs = retf sel */

#else /* 64-bit: */

    /*
     * Get the handler pointer (16:48 ptr).
     */
    RTIDTR      Idtr;
    ASMGetIDTR(&Idtr);
    PVBOXIDTE   pIdte = &((PVBOXIDTE)Idtr.pIdt)[uActiveVector * 2];

    AssertMsgReturnVoid(pIdte->Gen.u1Present, ("The IDT entry (%d) is not present!\n", uActiveVector));
    AssertMsgReturnVoid(    pIdte->Gen.u3Type1 == VBOX_IDTE_TYPE1
                        &&  pIdte->Gen.u5Type2 == VBOX_IDTE_TYPE2_INT_32, /* == 64 */
                        ("The IDT entry (%d) is not 64-bit int gate! type1=%#x type2=%#x\n",
                         uActiveVector, pIdte->Gen.u3Type1, pIdte->Gen.u5Type2));

    RTFAR64   pfnHandler;
    pfnHandler.off = VBOXIDTE_OFFSET(*pIdte);
    pfnHandler.off |= (uint64_t)(*(uint32_t *)(pIdte + 1)) << 32; //cleanup!
    pfnHandler.sel = pIdte->Gen.u16SegSel;

    if (pIdte->au32[1] & 0x7 /*IST*/)
    {
        /** @todo implement IST */
    }

    /*
     * The stack frame is as follows:
     *
     *      28  iret frame
     *      20  dummy
     *      14  uArg
     *      10  uOperation
     *       8  pVM
     *       0  return address (pvRet points here)
     *
     * We'll change the stackframe so that we will not return
     * to the caller but to a interrupt dispatcher. And we'll create
     * a 64-bit far return frame where dummy and uArg is.
     */
    uint64_t *pau = (uint64_t *)pvRet;
    Assert(pau[1] == (uint64_t)pVM);
    pau[0] = (uint64_t)trpmR0InterruptDispatcher; /* new return address */
    pau[3] = pfnHandler.off;            /* retf off */
    pau[4] = pfnHandler.sel;            /* retf sel */
#endif
}

#endif /* VBOX_WITH_IDT_PATCHING */
