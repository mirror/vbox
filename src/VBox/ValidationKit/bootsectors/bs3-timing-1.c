/* $Id$ */
/** @file
 * BS3Kit - bs3-timing-1, 16-bit C code.
 */

/*
 * Copyright (C) 2007-2022 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <bs3kit.h>
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
BS3_DECL_CALLBACK(void)     bs3Timing1_Tsc_pe32();



BS3_DECL(void) Main_rm()
{
    Bs3InitAll_rm();
    Bs3TestInit("bs3-timing-1");

    /* Switch to PE32 and do the work from there, all the 64-bit integer
       handling should be a little more efficient in 32-bit mode. */
    Bs3SwitchTo32BitAndCallC_rm(bs3Timing1_Tsc_pe32, 0);

    Bs3TestTerm();
}

