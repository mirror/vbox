/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-3 - MMX, SSE and AVX instructions, 16-bit C code.
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


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_andps_andpd_pand);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_andnps_andnpd_pandn);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_orps_orpd_por);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_xorps_xorpd_pxor);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_pcmpgtb_pcmpgtw_pcmpgtd_pcmpgtq);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_pcmpeqb_pcmpeqw_pcmpeqd_pcmpeqq);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_paddb_paddw_paddd_paddq);
BS3TESTMODEBYMAX_PROTOTYPES_CMN(bs3CpuInstr3_v_psubb_psubw_psubd_psubq);


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static const BS3TESTMODEBYMAXENTRY g_aTests[] =
{
#if 1
    BS3TESTMODEBYMAXENTRY_CMN("[v]andps/[v]andpd/[v]pand",      bs3CpuInstr3_v_andps_andpd_pand),
    BS3TESTMODEBYMAXENTRY_CMN("[v]andnps/[v]andnpd/[v]pandn",   bs3CpuInstr3_v_andnps_andnpd_pandn),
    BS3TESTMODEBYMAXENTRY_CMN("[v]orps/[v]orpd/[v]or",          bs3CpuInstr3_v_orps_orpd_por),
    BS3TESTMODEBYMAXENTRY_CMN("[v]xorps/[v]xorpd/[v]pxor",      bs3CpuInstr3_v_xorps_xorpd_pxor),
#endif
#if 1
    BS3TESTMODEBYMAXENTRY_CMN("[v]pcmpgtb/[v]pcmpgtw/[v]pcmpgtd/[v]pcmpgtq", bs3CpuInstr3_v_pcmpgtb_pcmpgtw_pcmpgtd_pcmpgtq),
    BS3TESTMODEBYMAXENTRY_CMN("[v]pcmpeqb/[v]pcmpeqw/[v]pcmpeqd/[v]pcmpeqq", bs3CpuInstr3_v_pcmpeqb_pcmpeqw_pcmpeqd_pcmpeqq),
#endif
#if 1
    BS3TESTMODEBYMAXENTRY_CMN("[v]paddb/[v]paddw/[v]paddd/[v]paddq", bs3CpuInstr3_v_paddb_paddw_paddd_paddq),
    BS3TESTMODEBYMAXENTRY_CMN("[v]psubb/[v]psubw/[v]psubd/[v]psubq", bs3CpuInstr3_v_psubb_psubw_psubd_psubq),
#endif
};


BS3_DECL(void) Main_rm()
{
    Bs3InitAll_rm();
    Bs3TestInit("bs3-cpu-instr-3");

    Bs3TestDoModesByMax_rm(g_aTests, RT_ELEMENTS(g_aTests));

    Bs3TestTerm();
}

