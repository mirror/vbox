/* $Id$ */
/** @file
 * DBGF - Debugger Facility, R0 part.
 *
 * Almost identical to DBGFGC.cpp, except for the fInHyper stuff.
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
#include <VBox/dbgf.h>
#include <VBox/selm.h>
#include <VBox/log.h>
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <iprt/assert.h>



/**
 * \#DB (Debug event) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The virtual CPU handle.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   uDr6        The DR6 register value.
 */
VMMR0DECL(int) DBGFR0Trap01Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame, RTGCUINTREG uDr6)
{
    /** @todo Intel docs say that X86_DR6_BS has the highest priority... */
    /*
     * A breakpoint?
     */
    if (uDr6 & (X86_DR6_B0 | X86_DR6_B1 | X86_DR6_B2 | X86_DR6_B3))
    {
        Assert(X86_DR6_B0 == 1 && X86_DR6_B1 == 2 && X86_DR6_B2 == 4 && X86_DR6_B3 == 8);
        for (unsigned iBp = 0; iBp < RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints); iBp++)
        {
            if (    ((uint32_t)uDr6 & RT_BIT_32(iBp))
                &&  pVM->dbgf.s.aHwBreakpoints[iBp].enmType == DBGFBPTYPE_REG)
            {
                pVCpu->dbgf.s.iActiveBp = pVM->dbgf.s.aHwBreakpoints[iBp].iBp;
                pVCpu->dbgf.s.fSingleSteppingRaw = false;
                LogFlow(("DBGFR0Trap03Handler: hit hw breakpoint %d at %04x:%RGv\n",
                         pVM->dbgf.s.aHwBreakpoints[iBp].iBp, pRegFrame->cs, pRegFrame->rip));

                return VINF_EM_DBG_BREAKPOINT;
            }
        }
    }

    /*
     * Single step?
     * Are we single stepping or is it the guest?
     */
    if (    (uDr6 & X86_DR6_BS)
        &&  (pVCpu->dbgf.s.fSingleSteppingRaw))
    {
        pVCpu->dbgf.s.fSingleSteppingRaw = false;
        LogFlow(("DBGFR0Trap01Handler: single step at %04x:%RGv\n", pRegFrame->cs, pRegFrame->rip));
        return VINF_EM_DBG_STEPPED;
    }

    /*
     * Currently we only implement single stepping in the guest,
     * so we'll bitch if this is not a BS event.
     */
    AssertMsg(uDr6 & X86_DR6_BS, ("hey! we're not doing guest BPs yet! dr6=%RTreg %04x:%RGv\n",
                                  uDr6, pRegFrame->cs, pRegFrame->rip));
    /** @todo virtualize DRx. */
    LogFlow(("DBGFR0Trap01Handler: guest debug event %RTreg at %04x:%RGv!\n", uDr6, pRegFrame->cs, pRegFrame->rip));
    return VINF_EM_RAW_GUEST_TRAP;
}


/**
 * \#BP (Breakpoint) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The VM handle.
 * @param   pVCpu       The virtual CPU handle.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 */
VMMR0DECL(int) DBGFR0Trap03Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTXCORE pRegFrame)
{
    /*
     * Get the trap address and look it up in the breakpoint table.
     * Don't bother if we don't have any breakpoints.
     */
    if (pVM->dbgf.s.cBreakpoints > 0)
    {
        RTGCPTR pPc;
        int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid,
                                              (RTGCPTR)pRegFrame->rip /* no -1 in R0 */, &pPc);
        AssertRCReturn(rc, rc);

        for (unsigned iBp = 0; iBp < RT_ELEMENTS(pVM->dbgf.s.aBreakpoints); iBp++)
        {
            if (    pVM->dbgf.s.aBreakpoints[iBp].GCPtr == (RTGCUINTPTR)pPc
                &&  pVM->dbgf.s.aBreakpoints[iBp].enmType == DBGFBPTYPE_INT3)
            {
                pVM->dbgf.s.aBreakpoints[iBp].cHits++;
                pVCpu->dbgf.s.iActiveBp = pVM->dbgf.s.aBreakpoints[iBp].iBp;

                LogFlow(("DBGFR0Trap03Handler: hit breakpoint %d at %RGv (%04x:%RGv) cHits=0x%RX64\n",
                         pVM->dbgf.s.aBreakpoints[iBp].iBp, pPc, pRegFrame->cs, pRegFrame->rip,
                         pVM->dbgf.s.aBreakpoints[iBp].cHits));
                return VINF_EM_DBG_BREAKPOINT;
            }
        }
    }

    return VINF_EM_RAW_GUEST_TRAP;
}

