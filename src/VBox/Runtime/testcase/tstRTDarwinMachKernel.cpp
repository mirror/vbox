/* $Id$ */
/** @file
 * IPRT Testcase - mach_kernel symbol resolving hack.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/test.h>

#include "../r0drv/darwin/mach_kernel-r0drv-darwin.cpp"


static void dotest(const char *pszMachKernel)
{
    PRTR0DARWINKERNEL pKernel;
    RTTESTI_CHECK_RC_RETV(rtR0DarwinMachKernelOpen(pszMachKernel, &pKernel), VINF_SUCCESS);
    static const char * const s_apszSyms[] =
    {
        "ast_pending",
        "i386_signal_cpu",
        "i386_cpu_IPI",
        "dtrace_register",
        "dtrace_suspend",
    };
    for (unsigned i = 0; i < RT_ELEMENTS(s_apszSyms); i++)
    {
        uintptr_t uPtr = rtR0DarwinMachKernelLookup(pKernel, s_apszSyms[i]);
        RTTestIPrintf(RTTESTLVL_ALWAYS, "%p %s\n", uPtr, s_apszSyms[i]);
        RTTESTI_CHECK(uPtr != 0);
    }
    rtR0DarwinMachKernelClose(pKernel);
}


int main(int argc, char **argv)
{
    RTTEST hTest;
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTDarwinMachKernel", &hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;
    RTTestBanner(hTest);

    dotest("/mach_kernel");

    return RTTestSummaryAndDestroy(hTest);
}

