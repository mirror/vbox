/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-basic-2, 16-bit C code.
 */

/*
 * Copyright (C) 2007-2016 Oracle Corporation
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
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_TssGateEsp);
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_RaiseXcpt1);
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_iret);
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_sidt);
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_sgdt);
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_lidt);
BS3TESTMODE_PROTOTYPES_MODE(bs3CpuBasic2_lgdt);

FNBS3TESTDOMODE             bs3CpuBasic2_sidt_f16;
FNBS3TESTDOMODE             bs3CpuBasic2_sgdt_f16;
FNBS3TESTDOMODE             bs3CpuBasic2_lidt_f16;
FNBS3TESTDOMODE             bs3CpuBasic2_lgdt_f16;
FNBS3TESTDOMODE             bs3CpuBasic2_iret_f16;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#if 0
static const BS3TESTMODEENTRY g_aModeTest[] =
{
#if 0
    BS3TESTMODEENTRY_MODE("tss / gate / esp", bs3CpuBasic2_TssGateEsp),
    BS3TESTMODEENTRY_MODE("raise xcpt #1", bs3CpuBasic2_RaiseXcpt1),
//    BS3TESTMODEENTRY_MODE("sidt", bs3CpuBasic2_sidt),
//    BS3TESTMODEENTRY_MODE("sgdt", bs3CpuBasic2_sgdt),
//    BS3TESTMODEENTRY_MODE("lidt", bs3CpuBasic2_lidt),
//    BS3TESTMODEENTRY_MODE("lgdt", bs3CpuBasic2_lgdt),
#endif
//    BS3TESTMODEENTRY_MODE("iret", bs3CpuBasic2_iret),
};
#endif

static const BS3TESTMODEBYONEENTRY g_aModeByOneTests[] =
{
    //{ "iret", bs3CpuBasic2_iret_f16, 0 },
    { "lgdt", bs3CpuBasic2_lgdt_f16, 0 },
};


BS3_DECL(void) Main_rm()
{
    Bs3InitAll_rm();
    Bs3TestInit("bs3-cpu-basic-2");
    Bs3TestPrintf("g_uBs3CpuDetected=%#x\n", g_uBs3CpuDetected);

    //Bs3TestDoModes_rm(g_aModeTest, RT_ELEMENTS(g_aModeTest));
    Bs3TestDoModesByOne_rm(g_aModeByOneTests, RT_ELEMENTS(g_aModeByOneTests), 0);

    Bs3TestTerm();
for (;;) { ASMHalt(); }
}

