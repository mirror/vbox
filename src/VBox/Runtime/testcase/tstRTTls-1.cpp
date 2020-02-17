/* $Id$ */
/** @file
 * IPRT Testcase - Thread Local Storage (TLS).
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <iprt/thread.h>

#include <iprt/asm.h>
#include <iprt/errcore.h>
#include <iprt/test.h>
#include <iprt/time.h>


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
static RTTEST            g_hTest;
static uint32_t volatile g_cDtorCalls = 0;

/**
 * @callback_method_impl{FNRTTLSDTOR}
 */
static DECLCALLBACK(void) testDtorCallback(void *pvValue)
{
    RTTEST_CHECK(g_hTest, pvValue != NULL);
    ASMAtomicIncU32(&g_cDtorCalls);
}


static DECLCALLBACK(int) testDtorThread1(RTTHREAD hSelf, void *pvUser)
{
    RTTEST_CHECK_RC(g_hTest, RTTlsSet((RTTLS)pvUser, hSelf), VINF_SUCCESS);
    return VINF_SUCCESS;
}


static void testDtor(void)
{
    RTTestISub("TLS Destructors");

    g_cDtorCalls = 0;
    RTTLS iTls;
    int rc = RTTlsAllocEx(&iTls, testDtorCallback);
    if (rc == VERR_NOT_SUPPORTED)
    {
        RTTestSkipped(g_hTest, "RTTlsAllocEx -> VERR_NOT_SUPPORTED");
        return;
    }
    RTTESTI_CHECK_RC_RETV(rc, VINF_SUCCESS);

    RTTHREAD ahThreads[16];
    uint32_t cThreads = 0;
    while (cThreads < RT_ELEMENTS(ahThreads))
    {
        RTTESTI_CHECK_RC_BREAK(RTThreadCreateF(&ahThreads[cThreads], testDtorThread1, (void *)iTls, 0, RTTHREADTYPE_DEFAULT,
                                               RTTHREADFLAGS_WAITABLE, "dtor-%u", cThreads), VINF_SUCCESS);
        cThreads++;
    }
    uint32_t const cExpectedDtorCalls = cThreads;

    while (cThreads-- > 0)
        RTTESTI_CHECK_RC(RTThreadWait(ahThreads[cThreads], RT_MS_30SEC, NULL), VINF_SUCCESS);

    /* RTThreadWait will return while the native portition of the thread may
       still be shutting down, so we need to fudge a little here.  */
    uint64_t const nsStart = RTTimeNanoTS();
    RTMSINTERVAL   nsSleep = 2;
    while (   ASMAtomicReadU32(&g_cDtorCalls) != cExpectedDtorCalls
           && RTTimeNanoTS() - nsStart < RT_NS_10SEC)
    {
        nsSleep = RT_MAX(nsSleep + 1, 128);
        RTThreadSleep(nsSleep);
    }

    uint32_t cCalls = ASMAtomicReadU32(&g_cDtorCalls);
    if (cCalls != cExpectedDtorCalls)
        RTTestFailed(g_hTest, "%u dtor calls, expected %u\n", cCalls, cExpectedDtorCalls);

    RTTESTI_CHECK_RC(RTTlsFree(iTls), VINF_SUCCESS);
}


int main()
{
    RTEXITCODE rcExit = RTTestInitAndCreate("tstRTStrCatCopy", &g_hTest);
    if (rcExit != RTEXITCODE_SUCCESS)
        return rcExit;

    testDtor();

    return RTTestSummaryAndDestroy(g_hTest);
}

