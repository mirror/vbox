/* $Id$ */
/** @file
 * CPU Instruction Decoding & Execution Tests - First bunch of instructions.
 */

/*
 * Copyright (C) 2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "cidet.h"



static DECLCALLBACK(int) cidetInOutAdd(PCIDETCORE pThis, bool fInvalid)
{
    return 0;
}


/** First bunch of instructions.  */
const CIDETINSTR g_aCidetInstructions1[] =
{
    {
        "add Eb,Gb", cidetInOutAdd,  1, {0x00, 0, 0}, 0, 2,
        {   CIDET_OF_K_GPR | CIDET_OF_Z_BYTE | CIDET_OF_M_RM | CIDET_OF_A_RW,
            CIDET_OF_K_GPR | CIDET_OF_Z_BYTE | CIDET_OF_M_REG | CIDET_OF_A_R,
            0, 0 }, CIDET_IF_MODRM
    },
#if 0
    {
        "add Ev,Gv", cidetInOutAdd,  1, {0x00, 0, 0}, 0, 2,
        {   CIDET_OF_K_GPR | CIDET_OF_Z_VAR_WDQ | CIDET_OF_M_RM,
            CIDET_OF_K_GPR | CIDET_OF_Z_VAR_WDQ | CIDET_OF_M_REG,
            0, 0 }, CIDET_IF_MODRM
    },
#endif
};
/** Number of instruction in the g_aInstructions1 array. */
const uint32_t g_cCidetInstructions1 = RT_ELEMENTS(g_aCidetInstructions1);

