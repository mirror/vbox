/* $Id$ */
/** @file
 * DBGF - Debugger Facility, All Context Code.
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
#include "DBGFInternal.h"
#include <VBox/vm.h>
#include <iprt/assert.h>


/**
 * Gets the hardware breakpoint configuration as DR7.
 *
 * @returns DR7 from the DBGF point of view.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTGCUINTREG) DBGFBpGetDR7(PVM pVM)
{
    RTGCUINTREG uDr7 = X86_DR7_GD | X86_DR7_GE | X86_DR7_LE | X86_DR7_MB1_MASK;
    PDBGFBP     pBp = &pVM->dbgf.s.aHwBreakpoints[0];
    unsigned    cLeft = RT_ELEMENTS(pVM->dbgf.s.aHwBreakpoints);
    while (cLeft-- > 0)
    {
        if (    pBp->enmType == DBGFBPTYPE_REG
            &&  pBp->fEnabled)
        {
            static const uint8_t s_au8Sizes[8] =
            {
                X86_DR7_LEN_BYTE, X86_DR7_LEN_BYTE, X86_DR7_LEN_WORD, X86_DR7_LEN_BYTE,
                X86_DR7_LEN_DWORD,X86_DR7_LEN_BYTE, X86_DR7_LEN_BYTE, X86_DR7_LEN_QWORD
            };
            uDr7 |= X86_DR7_G(pBp->u.Reg.iReg)
                 |  X86_DR7_RW(pBp->u.Reg.iReg, pBp->u.Reg.fType)
                 |  X86_DR7_LEN(pBp->u.Reg.iReg, s_au8Sizes[pBp->u.Reg.cb]);
        }
        pBp++;
    }
    return uDr7;
}


/**
 * Gets the address of the hardware breakpoint number 0.
 *
 * @returns DR0 from the DBGF point of view.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTGCUINTREG) DBGFBpGetDR0(PVM pVM)
{
    PCDBGFBP    pBp = &pVM->dbgf.s.aHwBreakpoints[0];
    Assert(pBp->u.Reg.iReg == 0);
    return pBp->GCPtr;
}


/**
 * Gets the address of the hardware breakpoint number 1.
 *
 * @returns DR1 from the DBGF point of view.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTGCUINTREG) DBGFBpGetDR1(PVM pVM)
{
    PCDBGFBP    pBp = &pVM->dbgf.s.aHwBreakpoints[1];
    Assert(pBp->u.Reg.iReg == 1);
    return pBp->GCPtr;
}


/**
 * Gets the address of the hardware breakpoint number 2.
 *
 * @returns DR2 from the DBGF point of view.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTGCUINTREG) DBGFBpGetDR2(PVM pVM)
{
    PCDBGFBP    pBp = &pVM->dbgf.s.aHwBreakpoints[2];
    Assert(pBp->u.Reg.iReg == 2);
    return pBp->GCPtr;
}


/**
 * Gets the address of the hardware breakpoint number 3.
 *
 * @returns DR3 from the DBGF point of view.
 * @param   pVM         The VM handle.
 */
VMMDECL(RTGCUINTREG) DBGFBpGetDR3(PVM pVM)
{
    PCDBGFBP    pBp = &pVM->dbgf.s.aHwBreakpoints[3];
    Assert(pBp->u.Reg.iReg == 3);
    return pBp->GCPtr;
}


/**
 * Returns the single stepping state for a virtual CPU.
 *
 * @returns stepping (true) or not (false).
 *
 * @param   pVCpu       The virtual CPU handle.
 */
VMMDECL(bool) DBGFIsStepping(PVMCPU pVCpu)
{
    return pVCpu->dbgf.s.fSingleSteppingRaw;
}

