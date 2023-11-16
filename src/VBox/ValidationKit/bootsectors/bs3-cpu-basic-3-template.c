/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-basic-3, C code template.
 */

/*
 * Copyright (C) 2007-2023 Oracle and/or its affiliates.
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
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>



#ifdef BS3_INSTANTIATING_CMN

# if ARCH_BITS != 64
extern BS3_DECL_FAR(void) BS3_CMN_FAR_NM(bs3CpuBasic3_lea_16)(void);
extern BS3_DECL_FAR(void) BS3_CMN_FAR_NM(bs3CpuBasic3_lea_32)(void);
#else
extern BS3_DECL_FAR(void) BS3_CMN_FAR_NM(bs3CpuBasic3_lea_64)(void);
#endif

BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuBasic3_Lea)(uint8_t bMode)
{
    /* Repeat the test so the native recompiler get a chance to kick in...  */
    unsigned i;

# if ARCH_BITS != 64
    FPFNBS3FAR pfnWorker16 = Bs3SelLnkCodePtrToCurPtr(BS3_CMN_FAR_NM(bs3CpuBasic3_lea_16));
    FPFNBS3FAR pfnWorker32 = Bs3SelLnkCodePtrToCurPtr(BS3_CMN_FAR_NM(bs3CpuBasic3_lea_32));
    for (i = 0; i < 64; i++)
        pfnWorker16();
    for (i = 0; i < 64; i++)
        pfnWorker32();
# else
    //for (i = 0; i < 64; i++)
    //    BS3_CMN_FAR_NM(bs3CpuBasic3_lea_64)
    RT_NOREF(i);
# endif

    RT_NOREF(bMode);
    return UINT8_MAX;
}


#endif /* BS3_INSTANTIATING_CMN */


/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE


#endif /* BS3_INSTANTIATING_MODE */

