/** @file
 *
 * TRPM - The Trap Monitor - HC Ring 0
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
    if (uActiveVector >= 256)
    {
        AssertMsgFailed(("uActiveVector=%#x is invalid! (More assertions to come, please enjoy!)\n",
                         uActiveVector));
        return;
    }

#if HC_ARCH_BITS == 32
    /*
     * Get the handler pointer (16:32 ptr).
     */
    RTIDTR      Idtr;
    ASMGetIDTR(&Idtr);
    PVBOXIDTE   pIdte = &((PVBOXIDTE)Idtr.pIdt)[uActiveVector];
    if (!pIdte->Gen.u1Present)
    {
        AssertMsgFailed(("The IDT entry (%d) is not present!\n", uActiveVector));
        return;
    }
    if (    pIdte->Gen.u3Type1 != VBOX_IDTE_TYPE1
        &&  pIdte->Gen.u5Type2 != VBOX_IDTE_TYPE2_INT_32)
    {
        AssertMsgFailed(("The IDT entry (%d) is not 32-bit int gate! type1=%#x type2=%#x\n",
                         uActiveVector, pIdte->Gen.u3Type1, pIdte->Gen.u5Type2));
        return;
    }

    RTFAR32   pfnHandler;
    pfnHandler.off = (pIdte->Gen.u16OffsetHigh << 16) | pIdte->Gen.u16OffsetLow;
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
    if (!pIdte->Gen.u1Present)
    {
        AssertMsgFailed(("The IDT entry (%d) is not present!\n", uActiveVector));
        return;
    }
    if (    pIdte->Gen.u3Type1 != VBOX_IDTE_TYPE1
        &&  pIdte->Gen.u5Type2 != VBOX_IDTE_TYPE2_INT_32)
    {
        AssertMsgFailed(("The IDT entry (%d) is not 32-bit int gate! type1=%#x type2=%#x\n",
                         uActiveVector, pIdte->Gen.u3Type1, pIdte->Gen.u5Type2));
        return;
    }

    RTFAR64   pfnHandler;
    pfnHandler.off = (pIdte->Gen.u16OffsetHigh << 16) | pIdte->Gen.u16OffsetLow;
    pfnHandler.off |= (uint64_t)(*(uint32_t *)(pIdte + 1)) << 32; //cleanup!
    pfnHandler.sel = pIdte->Gen.u16SegSel;

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
    pau[0] = (uint64_t)trpmR0InterruptDispatcher; /* new return address */
    pau[3] = pfnHandler.off;            /* retf off */
    pau[4] = pfnHandler.sel;            /* retf sel */
#endif

//    dprintf(("Interrupt: %04x:%08x vector %d\n", pfnHandler.sel, pfnHandler.off, uActiveVector));
}
