/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-4 - SSE and AVX FPU instructions, IRQ handler.
 */

/*
 * Copyright (C) 2024 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "bs3kit-template-header.h"


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
/** The frame saved by the trap handler. */
extern BS3TRAPFRAME        g_Bs3CpuInstr4TrapFrame;
/** The extended CPU context to save from the trap handler. */
extern BS3EXTCTX           g_Bs3CpuInstr4ExtCtxTrap;
/** Whether the trap handler was called. */
extern bool volatile       g_Bs3CpuInstr4TrapRaised;
#endif


#ifdef BS3_INSTANTIATING_CMN
BS3_DECL_NEAR_CALLBACK(void) BS3_CMN_NM(bs3CpuInstr4TrapHandler)(PBS3TRAPFRAME pTrapFrame)
{
    Bs3ExtCtxSave(&g_Bs3CpuInstr4ExtCtxTrap);
    Bs3MemCpy(&g_Bs3CpuInstr4TrapFrame, pTrapFrame, sizeof(g_Bs3CpuInstr4TrapFrame));
    g_Bs3CpuInstr4TrapRaised = true;
}
#endif

