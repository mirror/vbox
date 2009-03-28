/* $Id$ */
/** @file
 * DBGF - Debugger Facility, GC part.
 *
 * Almost identical to DBGFR0.cpp, except for the fInHyper stuff.
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
#define LOG_GROUP LOG_GROUP_DBGF
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
 * @param   pRegFrame   Pointer to the register frame for the trap.
 * @param   uDr6        The DR6 register value.
 */
VMMRCDECL(int) DBGFGCTrap01Handler(PVM pVM, PCPUMCTXCORE pRegFrame, RTGCUINTREG uDr6)
{
    const bool fInHyper = !(pRegFrame->ss & X86_SEL_RPL) && !pRegFrame->eflags.Bits.u1VM;

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
                pVM->dbgf.s.iActiveBp = pVM->dbgf.s.aHwBreakpoints[iBp].iBp;
                pVM->dbgf.s.fSingleSteppingRaw = false;
                LogFlow(("DBGFGCTrap03Handler: hit hw breakpoint %d at %04x:%08x\n",
                         pVM->dbgf.s.aHwBreakpoints[iBp].iBp, pRegFrame->cs, pRegFrame->eip));

                return fInHyper ? VINF_EM_DBG_HYPER_BREAKPOINT : VINF_EM_DBG_BREAKPOINT;
            }
        }
    }

    /*
     * Single step?
     * Are we single stepping or is it the guest?
     */
    if (    (uDr6 & X86_DR6_BS)
        &&  (fInHyper || pVM->dbgf.s.fSingleSteppingRaw))
    {
        pVM->dbgf.s.fSingleSteppingRaw = false;
        LogFlow(("DBGFGCTrap01Handler: single step at %04x:%08x\n", pRegFrame->cs, pRegFrame->eip));
        return fInHyper ? VINF_EM_DBG_HYPER_STEPPED : VINF_EM_DBG_STEPPED;
    }

    /*
     * Currently we only implement single stepping in the guest,
     * so we'll bitch if this is not a BS event.
     */
    AssertMsg(uDr6 & X86_DR6_BS, ("hey! we're not doing guest BPs yet! dr6=%RTreg %04x:%08x\n",
                                  uDr6, pRegFrame->cs, pRegFrame->eip));
    /** @todo virtualize DRx. */
    LogFlow(("DBGFGCTrap01Handler: guest debug event %RTreg at %04x:%08x!\n", uDr6, pRegFrame->cs, pRegFrame->eip));
    return fInHyper ? VERR_INTERNAL_ERROR : VINF_EM_RAW_GUEST_TRAP;
}


/**
 * \#BP (Breakpoint) handler.
 *
 * @returns VBox status code.
 *          VINF_SUCCESS means we completely handled this trap,
 *          other codes are passed execution to host context.
 *
 * @param   pVM         The VM handle.
 * @param   pRegFrame   Pointer to the register frame for the trap.
 */
VMMRCDECL(int) DBGFGCTrap03Handler(PVM pVM, PCPUMCTXCORE pRegFrame)
{
    /*
     * Get the trap address and look it up in the breakpoint table.
     * Don't bother if we don't have any breakpoints.
     */
    if (pVM->dbgf.s.cBreakpoints > 0)
    {
        RTGCPTR pPc;
        int rc = SELMValidateAndConvertCSAddr(pVM, pRegFrame->eflags, pRegFrame->ss, pRegFrame->cs, &pRegFrame->csHid,
                                              (RTGCPTR)((RTGCUINTPTR)pRegFrame->eip - 1),
                                              &pPc);
        AssertRCReturn(rc, rc);

        for (unsigned iBp = 0; iBp < RT_ELEMENTS(pVM->dbgf.s.aBreakpoints); iBp++)
        {
            if (    pVM->dbgf.s.aBreakpoints[iBp].GCPtr == (RTGCUINTPTR)pPc
                &&  pVM->dbgf.s.aBreakpoints[iBp].enmType == DBGFBPTYPE_INT3)
            {
                pVM->dbgf.s.aBreakpoints[iBp].cHits++;
                pVM->dbgf.s.iActiveBp = pVM->dbgf.s.aBreakpoints[iBp].iBp;

                LogFlow(("DBGFGCTrap03Handler: hit breakpoint %d at %RGv (%04x:%08x) cHits=0x%RX64\n",
                         pVM->dbgf.s.aBreakpoints[iBp].iBp, pPc, pRegFrame->cs, pRegFrame->eip,
                         pVM->dbgf.s.aBreakpoints[iBp].cHits));
                return !(pRegFrame->ss & X86_SEL_RPL) && !pRegFrame->eflags.Bits.u1VM
                    ? VINF_EM_DBG_HYPER_BREAKPOINT
                    : VINF_EM_DBG_BREAKPOINT;
            }
        }
    }

    return !(pRegFrame->ss & X86_SEL_RPL) && !pRegFrame->eflags.Bits.u1VM
        ? VINF_EM_DBG_HYPER_ASSERTION
        : VINF_EM_RAW_GUEST_TRAP;
}

