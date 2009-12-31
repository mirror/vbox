/* $Id$ */
/** @file
 * IPRT Testcase - Deadlock detection.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/asm.h>                   /* for return addresses */
#include <iprt/critsect.h>
#include <iprt/lockvalidator.h>

#include <iprt/err.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/test.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTTEST       g_hTest;

static uint32_t     g_cThreads;
static RTTHREAD     g_ahThreads[32];
static RTCRITSECT   g_aCritSects[32];


/**
 * Spin until someone else has taken ownership of the critical section.
 *
 * @returns true on success, false on abort.
 * @param   pCritSect   The critical section.
 */
static bool testWaitForCritSectToBeOwned(PRTCRITSECT pCritSect)
{
    unsigned iLoop = 0;
    while (!RTCritSectIsOwned(pCritSect))
    {
        if (!RTCritSectIsInitialized(pCritSect))
            return false;
        RTThreadSleep(iLoop > 256 ? 1 : 0);
        iLoop++;
    }
    return true;
}


/**
 * Waits for a thread to enter a sleeping state.
 *
 * @returns true on success, false on abort.
 * @param   hThread     The thread.
 */
static bool testWaitForThreadToSleep(RTTHREAD hThread)
{
    for (unsigned iLoop = 0; ; iLoop++)
    {
        RTTHREADSTATE enmState = RTThreadGetState(hThread);
        if (RTTHREAD_IS_SLEEPING(enmState))
            return true;
        if (enmState != RTTHREADSTATE_RUNNING)
            return false;
        RTThreadSleep(iLoop > 256 ? 1 : 0);
    }
}


static DECLCALLBACK(int) test1Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    PRTCRITSECT     pMine = &g_aCritSects[i];
    PRTCRITSECT     pNext = &g_aCritSects[(i + 1) % g_cThreads];

    RTTEST_CHECK_RC_RET(g_hTest, RTCritSectEnter(pMine), VINF_SUCCESS, rcCheck);
    if (testWaitForCritSectToBeOwned(pNext))
    {
        int rc;
        if (i != g_cThreads - 1)
            RTTEST_CHECK_RC(g_hTest, rc = RTCritSectEnter(pNext), VINF_SUCCESS);
        else
        {
            /* the last thread triggers the deadlock. */
            for (unsigned i = 0; i < g_cThreads - 1; i++)
                RTTEST_CHECK_RET(g_hTest, testWaitForThreadToSleep(g_ahThreads[i]), VINF_SUCCESS);
            RTTEST_CHECK_RC(g_hTest, rc = RTCritSectEnter(pNext), VERR_SEM_LV_DEADLOCK);
        }
        if (RT_SUCCESS(rc))
            RTTEST_CHECK_RC(g_hTest, rc = RTCritSectLeave(pNext), VINF_SUCCESS);
        RTTEST_CHECK_RC(g_hTest, RTCritSectLeave(pMine), VINF_SUCCESS);
    }
    return VINF_SUCCESS;
}


static void test1(uint32_t cThreads)
{
    uint32_t i;
    RTTestSubF(g_hTest, "critsect deadlock with %u threads", cThreads);
    RTTEST_CHECK_RETV(g_hTest, RT_ELEMENTS(g_ahThreads) >= cThreads);
    RTTEST_CHECK_RETV(g_hTest, RT_ELEMENTS(g_aCritSects) >= cThreads);

    g_cThreads = cThreads;
    for (i = 0; i < cThreads; i++)
        RTTEST_CHECK_RC_RETV(g_hTest, RTCritSectInit(&g_aCritSects[i]), VINF_SUCCESS);

    for (i = 0; i < cThreads && g_ahThreads[i]; i++)
        g_ahThreads[i] = NIL_RTTHREAD;
    int rc = VINF_SUCCESS;
    for (i = 0; i < cThreads && RT_SUCCESS(rc); i++)
        RTTEST_CHECK_RC_OK(g_hTest, rc = RTThreadCreateF(&g_ahThreads[i], test1Thread, (void *)(uintptr_t)i, 0,
                                                         RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "test1-%02u", i));
    if (RT_SUCCESS(rc))
    {
        int rc2 = VINF_SUCCESS;
        i = cThreads;
        while (i-- > 0 && RT_SUCCESS(rc) && RT_SUCCESS(rc2))
        {
            RTTEST_CHECK_RC_OK(g_hTest, rc = RTThreadWait(g_ahThreads[i], 30*1000, &rc2));
            if (RT_SUCCESS(rc))
                g_ahThreads[i] = NIL_RTTHREAD;
        }
    }

    for (uint32_t i = 0; i < cThreads; i++)
        RTTEST_CHECK_RC_RETV(g_hTest, RTCritSectDelete(&g_aCritSects[i]), VINF_SUCCESS);
    for (uint32_t i = 0; i < cThreads; i++)
        if (g_ahThreads[i] != NIL_RTTHREAD)
        {
            RTTEST_CHECK_RC_OK(g_hTest, rc = RTThreadWait(g_ahThreads[i], 10*1000, NULL));
            g_ahThreads[i] = NIL_RTTHREAD;
        }
}

static bool testIsLockValidationCompiledIn(void)
{
    RTCRITSECT CritSect;
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTCritSectInit(&CritSect), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTCritSectEnter(&CritSect), false);
    bool fRet = CritSect.pValidatorRec
             && CritSect.pValidatorRec->hThread == RTThreadSelf();
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTCritSectLeave(&CritSect), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTCritSectDelete(&CritSect), false);

    /* Add check for the other types as we add more scenarios. */
    return fRet;
}

int main()
{
    /*
     * Init.
     */
    int rc = RTTestInitAndCreate("tstRTDeadlock", &g_hTest);
    if (rc)
        return rc;
    RTTestBanner(g_hTest);

    RTLockValidatorSetEnabled(true);
    RTLockValidatorSetMayPanic(false);
    if (!testIsLockValidationCompiledIn())
        return RTTestErrorCount(g_hTest) > 0
            ? RTTestSummaryAndDestroy(g_hTest)
            : RTTestSkipAndDestroy(g_hTest, "deadlock detection is not compiled in");

    test1(2);
    test1(3);
    test1(15);
    test1(30);

    return RTTestSummaryAndDestroy(g_hTest);
}

