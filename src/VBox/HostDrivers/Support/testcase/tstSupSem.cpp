/* $Id$ */
/** @file
 * Support Library Testcase - Ring-3 Semaphore interface.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#include <VBox/sup.h>

#include <VBox/param.h>
#include <iprt/err.h>
#include <iprt/initterm.h>
#include <iprt/stream.h>
#include <iprt/test.h>
#include <iprt/thread.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
static PSUPDRVSESSION   g_pSession;
static RTTEST           g_hTest;
static uint32_t         g_cMillies; /* Used by the interruptible tests. */



static DECLCALLBACK(int) tstSupSemInterruptibleSRE(RTTHREAD hSelf, void *pvUser)
{
    SUPSEMEVENT hEvent = (SUPSEMEVENT)pvUser;
    RTThreadUserSignal(hSelf);
    return SUPSemEventWaitNoResume(g_pSession, hEvent, g_cMillies);
}


static DECLCALLBACK(int) tstSupSemInterruptibleMRE(RTTHREAD hSelf, void *pvUser)
{
    SUPSEMEVENTMULTI hEventMulti = (SUPSEMEVENTMULTI)pvUser;
    RTThreadUserSignal(hSelf);
    return SUPSemEventMultiWaitNoResume(g_pSession, hEventMulti, g_cMillies);
}


int main(int argc, char **argv)
{
    /*
     * Init.
     */
    int rc = RTR3InitAndSUPLib();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSupSem: fatal error: RTR3InitAndSUPLib failed with rc=%Rrc\n", rc);
        return 1;
    }

    RTTEST hTest;
    rc = RTTestCreate("tstSupSem", &hTest);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSupSem: fatal error: RTTestCreate failed with rc=%Rrc\n", rc);
        return 1;
    }
    g_hTest = hTest;

    PSUPDRVSESSION pSession;
    rc = SUPR3Init(&pSession);
    if (RT_FAILURE(rc))
    {
        RTTestFailed(hTest, "SUPR3Init failed with rc=%Rrc\n", rc);
        return RTTestSummaryAndDestroy(hTest);
    }
    g_pSession = pSession;

    /*
     * Basic API checks.
     */
    RTTestSub(hTest, "Single Release Event (SRE) API");
    SUPSEMEVENT hEvent = NIL_SUPSEMEVENT;
    RTTESTI_CHECK_RC(SUPSemEventCreate(pSession, &hEvent),          VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent,20),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent,20),  VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventSignal(pSession, hEvent),           VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent, 0),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventWaitNoResume(pSession, hEvent,20),  VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, hEvent),            VINF_OBJECT_DESTROYED);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, hEvent),            VERR_INVALID_HANDLE);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, NIL_SUPSEMEVENT),   VINF_SUCCESS);

    RTTestSub(hTest, "Multiple Release Event (MRE) API");
    SUPSEMEVENTMULTI hEventMulti = NIL_SUPSEMEVENT;
    RTTESTI_CHECK_RC(SUPSemEventMultiCreate(pSession, &hEventMulti),            VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti,20),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiReset(pSession, hEventMulti),              VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti,20),    VERR_TIMEOUT);
    RTTESTI_CHECK_RC(SUPSemEventMultiSignal(pSession, hEventMulti),             VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiWaitNoResume(pSession, hEventMulti, 0),    VINF_SUCCESS);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, hEventMulti),              VINF_OBJECT_DESTROYED);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, hEventMulti),              VERR_INVALID_HANDLE);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, NIL_SUPSEMEVENTMULTI),     VINF_SUCCESS);

#if !defined(RT_OS_OS2) && !defined(RT_OS_WINDOWS)
    RTTestSub(hTest, "SRE Interruptibility");
    RTTESTI_CHECK_RC(SUPSemEventCreate(pSession, &hEvent), VINF_SUCCESS);
    g_cMillies = RT_INDEFINITE_WAIT;
    RTTHREAD hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(RTThreadCreate(&hThread, tstSupSemInterruptibleSRE, (void *)hEvent, 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "IntSRE"), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadUserWait(hThread, 60*1000), VINF_SUCCESS);
    RTThreadSleep(120);
    RTThreadPoke(hThread);
    int rcThread = VINF_SUCCESS;
    RTTESTI_CHECK_RC(RTThreadWait(hThread, 60*1000, &rcThread), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rcThread, VERR_INTERRUPTED);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, hEvent), VINF_OBJECT_DESTROYED);

    RTTESTI_CHECK_RC(SUPSemEventCreate(pSession, &hEvent), VINF_SUCCESS);
    g_cMillies = 120*1000;
    hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(RTThreadCreate(&hThread, tstSupSemInterruptibleSRE, (void *)hEvent, 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "IntSRE"), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadUserWait(hThread, 60*1000), VINF_SUCCESS);
    RTThreadSleep(120);
    RTThreadPoke(hThread);
    rcThread = VINF_SUCCESS;
    RTTESTI_CHECK_RC(RTThreadWait(hThread, 60*1000, &rcThread), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rcThread, VERR_INTERRUPTED);
    RTTESTI_CHECK_RC(SUPSemEventClose(pSession, hEvent), VINF_OBJECT_DESTROYED);


    RTTestSub(hTest, "MRE Interruptibility");
    RTTESTI_CHECK_RC(SUPSemEventMultiCreate(pSession, &hEventMulti), VINF_SUCCESS);
    g_cMillies = RT_INDEFINITE_WAIT;
    hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(RTThreadCreate(&hThread, tstSupSemInterruptibleMRE, (void *)hEventMulti, 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "IntMRE"), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadUserWait(hThread, 60*1000), VINF_SUCCESS);
    RTThreadSleep(120);
    RTThreadPoke(hThread);
    rcThread = VINF_SUCCESS;
    RTTESTI_CHECK_RC(RTThreadWait(hThread, 60*1000, &rcThread), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rcThread, VERR_INTERRUPTED);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, hEventMulti), VINF_OBJECT_DESTROYED);

    RTTESTI_CHECK_RC(SUPSemEventMultiCreate(pSession, &hEventMulti), VINF_SUCCESS);
    g_cMillies = 120*1000;
    hThread = NIL_RTTHREAD;
    RTTESTI_CHECK_RC(RTThreadCreate(&hThread, tstSupSemInterruptibleMRE, (void *)hEventMulti, 0, RTTHREADTYPE_TIMER, RTTHREADFLAGS_WAITABLE, "IntMRE"), VINF_SUCCESS);
    RTTESTI_CHECK_RC(RTThreadUserWait(hThread, 60*1000), VINF_SUCCESS);
    RTThreadSleep(120);
    RTThreadPoke(hThread);
    rcThread = VINF_SUCCESS;
    RTTESTI_CHECK_RC(RTThreadWait(hThread, 60*1000, &rcThread), VINF_SUCCESS);
    RTTESTI_CHECK_RC(rcThread, VERR_INTERRUPTED);
    RTTESTI_CHECK_RC(SUPSemEventMultiClose(pSession, hEventMulti), VINF_OBJECT_DESTROYED);
#endif /* !OS2 && !WINDOWS */

    /*
     * Done.
     */
    return RTTestSummaryAndDestroy(hTest);
}

