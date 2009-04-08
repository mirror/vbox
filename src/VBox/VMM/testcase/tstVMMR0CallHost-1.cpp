/* $Id$ */
/** @file
 * Testcase for the VMMR0JMPBUF operations.
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
#include <iprt/initterm.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include <iprt/alloca.h>
#include <iprt/test.h>
#include <VBox/err.h>

#define IN_VMM_R0
#define IN_RING0 /* pretent we're in Ring-0 to get the prototypes. */
#include <VBox/vmm.h>
#include "VMMInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#ifdef RT_OS_DARWIN
# define VMM_R0_SWITCH_STACK
#endif


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The jump buffer. */
static VMMR0JMPBUF          g_Jmp;
/** The number of jumps we've done. */
static unsigned volatile    g_cJmps;


int foo(int i, int iZero, int iMinusOne)
{
    /* allocate a buffer which we fill up to the end. */
    size_t cb = (i % 5555) + 32;
    char  *pv = (char *)alloca(cb);
    RTStrPrintf(pv, cb, "i=%d%*s\n", i, cb, "");

    /* Do long jmps every 7th time */
    if ((i % 7) == 0)
    {
        g_cJmps++;
        int rc = vmmR0CallHostLongJmp(&g_Jmp, 42);
        if (!rc)
            return i + 10000;
        return -1;
    }
    NOREF(iMinusOne);
    return i;
}


DECLCALLBACK(int) tst2(intptr_t i, intptr_t i2)
{
    RTTESTI_CHECK_MSG_RET(i >= 0 && i <= 8192, ("i=%d is out of range [0..8192]\n", i),      1);
    RTTESTI_CHECK_MSG_RET(i2 == 0,             ("i2=%d is out of range [0]\n", i2),          1);
    int iExpect = (i % 7) == 0 ? i + 10000 : i;
    int rc = foo(i, 0, -1);
    RTTESTI_CHECK_MSG_RET(rc == iExpect,       ("i=%d rc=%d expected=%d\n", i, rc, iExpect), 1);
    return 0;
}


void tst(int iFrom, int iTo, int iInc)
{
#ifdef VMM_R0_SWITCH_STACK
    int const cIterations = iFrom > iTo ? iFrom - iTo : iTo - iFrom;
    void   *pvPrev = alloca(1);
#endif

    g_cJmps = 0;
    for (int i = iFrom, iItr = 0; i != iTo; i += iInc, iItr++)
    {
        int rc = vmmR0CallHostSetJmp(&g_Jmp, (PFNVMMR0SETJMP)tst2, (PVM)i, 0);
        RTTESTI_CHECK_MSG_RETV(rc == 0 || rc == 42, ("i=%d rc=%d setjmp\n", i, rc));

#ifdef VMM_R0_SWITCH_STACK
        /* Make the stack pointer slide for the second half of the calls. */
        if (iItr >= cIterations / 2)
        {
            /* Note! gcc does funny rounding up of alloca(). */
            void  *pv2 = alloca((i % 63) | 1);
            size_t cb2 = (uintptr_t)pvPrev - (uintptr_t)pv2;
            RTTESTI_CHECK_MSG(cb2 >= 16 && cb2 <= 128, ("cb2=%zu pv2=%p pvPrev=%p iAlloca=%d\n", cb2, pv2, pvPrev, iItr));
            memset(pv2, 0xff, cb2);
            memset(pvPrev, 0xee, 1);
            pvPrev = pv2;
        }
#endif
    }
    RTTESTI_CHECK_MSG_RETV(g_cJmps, ("No jumps!"));
}


int main()
{
    /*
     * Init.
     */
    RTTEST hTest;
    int rc;
    if (    RT_FAILURE(rc = RTR3Init())
        ||  RT_FAILURE(rc = RTTestCreate("tstVMMR0CallHost-1", &hTest)))
    {
        RTStrmPrintf(g_pStdErr, "tstVMMR0CallHost-1: Fatal error during init: %Rrc\n", rc);
        return 1;
    }
    RTTestBanner(hTest);

    g_Jmp.pvSavedStack = (RTR0PTR)RTTestGuardedAllocTail(hTest, 8192);

    /*
     * Run two test with about 1000 long jumps each.
     */
    RTTestSub(hTest, "Increasing stack usage");
    tst(0, 7000, 1);
    RTTestSub(hTest, "Decreasing stack usage");
    tst(7599, 0, -1);

    return RTTestSummaryAndDestroy(hTest);
}
