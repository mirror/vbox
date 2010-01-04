/* $Id$ */
/** @file
 * IPRT Testcase - RTLockValidator.
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
#include <iprt/lockvalidator.h>

#include <iprt/asm.h>                   /* for return addresses */
#include <iprt/critsect.h>
#include <iprt/err.h>
#include <iprt/semaphore.h>
#include <iprt/test.h>
#include <iprt/thread.h>
#include <iprt/time.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The testcase handle. */
static RTTEST               g_hTest;
/** Flip this in the debugger to get some peace to single step wild code. */
bool volatile               g_fDoNotSpin = false;

static uint32_t             g_cThreads;
static uint32_t             g_iDeadlockThread;
static RTTHREAD             g_ahThreads[32];
static RTCRITSECT           g_aCritSects[32];
static RTSEMRW              g_ahSemRWs[32];
static RTSEMMUTEX           g_ahSemMtxes[32];
static RTSEMEVENT           g_hSemEvt;
static RTSEMEVENTMULTI      g_hSemEvtMulti;

/** Multiple release event semaphore that is signalled by the main thread after
 * it has started all the threads. */
static RTSEMEVENTMULTI      g_hThreadStarteEvt;


/** When to stop testing. */
static uint64_t             g_NanoTSStop;
/** The number of deadlocks. */
static uint32_t volatile    g_cDeadlocks;
/** The number of loops. */
static uint32_t volatile    g_cLoops;


/**
 * Spin until someone else has taken ownership of the critical section.
 *
 * @returns true on success, false on abort.
 * @param   pCritSect   The critical section.
 */
static bool testWaitForCritSectToBeOwned(PRTCRITSECT pCritSect)
{
    RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventMultiWait(g_hThreadStarteEvt, 10*1000));

    unsigned iLoop = 0;
    while (!RTCritSectIsOwned(pCritSect))
    {
        if (!RTCritSectIsInitialized(pCritSect))
            return false;
        RTThreadSleep(g_fDoNotSpin ? 3600*1000 : iLoop > 256 ? 1 : 0);
        iLoop++;
    }
    return true;
}


/**
 * Spin until someone else has taken ownership (any kind) of the read-write
 * semaphore.
 *
 * @returns true on success, false on abort.
 * @param   hSemRW      The read-write semaphore.
 */
static bool testWaitForSemRWToBeOwned(RTSEMRW hSemRW)
{
    RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventMultiWait(g_hThreadStarteEvt, 10*1000));

    unsigned iLoop = 0;
    for (;;)
    {
        if (RTSemRWGetWriteRecursion(hSemRW) > 0)
            return true;
        if (RTSemRWGetReadCount(hSemRW) > 0)
            return true;
        RTThreadSleep(g_fDoNotSpin ? 3600*1000 : iLoop > 256 ? 1 : 0);
        iLoop++;
    }
    return true;
}


/**
 * Spin until someone else has taken ownership of the mutex semaphore.
 *
 * @returns true on success, false on abort.
 * @param   hSemMutex   The mutex sempahore.
 */
static bool testWaitForSemMutexToBeOwned(RTSEMMUTEX hSemMutex)
{
    RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventMultiWait(g_hThreadStarteEvt, 10*1000));

    unsigned iLoop = 0;
    while (!RTSemMutexIsOwned(hSemMutex))
    {
        RTThreadSleep(g_fDoNotSpin ? 3600*1000 : iLoop > 256 ? 1 : 0);
        iLoop++;
    }
    return true;
}


/**
 * Waits for all the other threads to enter sleeping states.
 *
 * @returns VINF_SUCCESS on success, VERR_INTERNAL_ERROR on failure.
 * @param   enmDesiredState     The desired thread sleep state.
 * @param   cWaitOn             The distance to the lock they'll be waiting on,
 *                              the lock type is derived from the desired state.
 *                              UINT32_MAX means no special lock.
 */
static int testWaitForAllOtherThreadsToSleep(RTTHREADSTATE enmDesiredState, uint32_t cWaitOn)
{
    RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    RTTEST_CHECK_RC_OK(g_hTest, RTSemEventMultiWait(g_hThreadStarteEvt, 10*1000));

    RTTHREAD hThreadSelf = RTThreadSelf();
    for (uint32_t iOuterLoop = 0; ; iOuterLoop++)
    {
        uint32_t cMissing  = 0;
        uint32_t cWaitedOn = 0;
        for (uint32_t i = 0; i < g_cThreads; i++)
        {
            RTTHREAD hThread = g_ahThreads[i];
            if (hThread == NIL_RTTHREAD)
                cMissing++;
            else if (hThread != hThreadSelf)
            {
                /*
                 * Figure out which lock to wait for.
                 */
                void *pvLock = NULL;
                if (cWaitOn != UINT32_MAX)
                {
                    uint32_t j = (i + cWaitOn) % g_cThreads;
                    switch (enmDesiredState)
                    {
                        case RTTHREADSTATE_CRITSECT:    pvLock = &g_aCritSects[j]; break;
                        case RTTHREADSTATE_RW_WRITE:
                        case RTTHREADSTATE_RW_READ:     pvLock = g_ahSemRWs[j]; break;
                        case RTTHREADSTATE_MUTEX:       pvLock = g_ahSemMtxes[j]; break;
                        default: break;
                    }
                }

                /*
                 * Wait for this thread.
                 */
                for (unsigned iLoop = 0; ; iLoop++)
                {
                    RTTHREADSTATE enmState = RTThreadGetReallySleeping(hThread);
                    if (RTTHREAD_IS_SLEEPING(enmState))
                    {
                        if (   enmState == enmDesiredState
                            && (   !pvLock
                                || (   pvLock == RTLockValidatorQueryBlocking(hThread)
                                    && !RTLockValidatorIsBlockedThreadInValidator(hThread) )
                               )
                            && RTThreadGetNativeState(hThread) != RTTHREADNATIVESTATE_RUNNING
                           )
                            break;
                    }
                    else if (   enmState != RTTHREADSTATE_RUNNING
                             && enmState != RTTHREADSTATE_INITIALIZING)
                        return VERR_INTERNAL_ERROR;
                    RTThreadSleep(g_fDoNotSpin ? 3600*1000 : iOuterLoop + iLoop > 256 ? 1 : 0);
                    cWaitedOn++;
                }
            }
        }

        if (!cMissing && !cWaitedOn)
            break;
        RTThreadSleep(g_fDoNotSpin ? 3600*1000 : iOuterLoop > 256 ? 1 : 0);
    }

    RTThreadSleep(0);                   /* fudge factor */
    return VINF_SUCCESS;
}


/**
 * Worker that starts the threads.
 *
 * @returns Same as RTThreadCreate.
 * @param   cThreads            The number of threads to start.
 * @param   pfnThread           Thread function.
 */
static int testStartThreads(uint32_t cThreads, PFNRTTHREAD pfnThread)
{
    RTSemEventMultiReset(g_hThreadStarteEvt);

    for (uint32_t i = 0; i < RT_ELEMENTS(g_ahThreads); i++)
        g_ahThreads[i] = NIL_RTTHREAD;

    int rc = VINF_SUCCESS;
    for (uint32_t i = 0; i < cThreads; i++)
    {
        rc = RTThreadCreateF(&g_ahThreads[i], pfnThread, (void *)(uintptr_t)i, 0,
                             RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "thread-%02u", i);
        RTTEST_CHECK_RC_OK(g_hTest, rc);
        if (RT_FAILURE(rc))
            break;
    }

    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemEventMultiSignal(g_hThreadStarteEvt), rcCheck);
    return rc;
}


/**
 * Worker that waits for the threads to complete.
 *
 * @param   cMillies            How long to wait for each.
 * @param   fStopOnError        Whether to stop on error and heed the thread
 *                              return status.
 */
static void testWaitForThreads(uint32_t cMillies, bool fStopOnError)
{
    uint32_t i = RT_ELEMENTS(g_ahThreads);
    while (i-- > 0)
        if (g_ahThreads[i] != NIL_RTTHREAD)
        {
            int rcThread;
            int rc2;
            RTTEST_CHECK_RC_OK(g_hTest, rc2 = RTThreadWait(g_ahThreads[i], cMillies, &rcThread));
            if (RT_SUCCESS(rc2))
                g_ahThreads[i] = NIL_RTTHREAD;
            if (fStopOnError && (RT_FAILURE(rc2) || RT_FAILURE(rcThread)))
                return;
        }
}


static void testIt(uint32_t cThreads, uint32_t cPasses, uint32_t cSecs, PFNRTTHREAD pfnThread, const char *pszName)
{
    /*
     * Init test.
     */
    if (cSecs)
        RTTestSubF(g_hTest, "%s, %u threads, %u secs", pszName, cThreads, cSecs * cPasses);
    else
        RTTestSubF(g_hTest, "%s, %u threads, %u passes", pszName, cThreads, cPasses);

    RTTEST_CHECK_RETV(g_hTest, RT_ELEMENTS(g_ahThreads) >= cThreads);
    RTTEST_CHECK_RETV(g_hTest, RT_ELEMENTS(g_aCritSects) >= cThreads);

    g_cThreads = cThreads;

    for (uint32_t i = 0; i < cThreads; i++)
    {
        RTTEST_CHECK_RC_RETV(g_hTest, RTCritSectInit(&g_aCritSects[i]), VINF_SUCCESS);
        RTTEST_CHECK_RC_RETV(g_hTest, RTSemRWCreate(&g_ahSemRWs[i]), VINF_SUCCESS);
        RTTEST_CHECK_RC_RETV(g_hTest, RTSemMutexCreate(&g_ahSemMtxes[i]), VINF_SUCCESS);
    }
    RTTEST_CHECK_RC_RETV(g_hTest, RTSemEventCreate(&g_hSemEvt), VINF_SUCCESS);
    RTTEST_CHECK_RC_RETV(g_hTest, RTSemEventMultiCreate(&g_hSemEvtMulti), VINF_SUCCESS);
    RTTEST_CHECK_RC_RETV(g_hTest, RTSemEventMultiCreate(&g_hThreadStarteEvt), VINF_SUCCESS);

    /*
     * The test loop.
     */
    uint32_t cLoops     = 0;
    uint32_t cDeadlocks = 0;
    uint32_t cErrors    = RTTestErrorCount(g_hTest);
    for (uint32_t iPass = 0; iPass < cPasses && RTTestErrorCount(g_hTest) == cErrors; iPass++)
    {
        g_iDeadlockThread = (cThreads - 1 + iPass) % cThreads;
        g_cLoops          = 0;
        g_cDeadlocks      = 0;
        g_NanoTSStop      = cSecs ? RTTimeNanoTS() + cSecs * UINT64_C(1000000000) : 0;

        int rc = testStartThreads(cThreads, pfnThread);
        if (RT_SUCCESS(rc))
            testWaitForThreads(30*1000 + cSecs*1000, true);

        RTTEST_CHECK(g_hTest, !cSecs || g_cLoops > 0);
        cLoops += g_cLoops;
        RTTEST_CHECK(g_hTest, !cSecs || g_cDeadlocks > 0);
        cDeadlocks += g_cDeadlocks;
    }

    /*
     * Cleanup.
     */
    for (uint32_t i = 0; i < cThreads; i++)
    {
        RTTEST_CHECK_RC(g_hTest, RTCritSectDelete(&g_aCritSects[i]), VINF_SUCCESS);
        RTTEST_CHECK_RC(g_hTest, RTSemRWDestroy(g_ahSemRWs[i]), VINF_SUCCESS);
        RTTEST_CHECK_RC(g_hTest, RTSemMutexDestroy(g_ahSemMtxes[i]), VINF_SUCCESS);
    }
    RTTEST_CHECK_RC(g_hTest, RTSemEventDestroy(g_hSemEvt), VINF_SUCCESS);
    RTTEST_CHECK_RC(g_hTest, RTSemEventMultiDestroy(g_hSemEvtMulti), VINF_SUCCESS);
    RTTEST_CHECK_RC(g_hTest, RTSemEventMultiDestroy(g_hThreadStarteEvt), VINF_SUCCESS);

    testWaitForThreads(10*1000, false);

    /*
     * Print results if applicable.
     */
    if (cSecs)
        RTTestPrintf(g_hTest,  RTTESTLVL_ALWAYS, "cLoops=%u cDeadlocks=%u (%u%%)\n",
                     cLoops, cDeadlocks, cLoops ? cDeadlocks * 100 / cLoops : 0);
}


static DECLCALLBACK(int) test1Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    PRTCRITSECT     pMine = &g_aCritSects[i];
    PRTCRITSECT     pNext = &g_aCritSects[(i + 1) % g_cThreads];

    RTTEST_CHECK_RC_RET(g_hTest, RTCritSectEnter(pMine), VINF_SUCCESS, rcCheck);
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTCritSectEnter(pMine), VINF_SUCCESS);
    if (testWaitForCritSectToBeOwned(pNext))
    {
        int rc;
        if (i != g_iDeadlockThread)
            RTTEST_CHECK_RC(g_hTest, rc = RTCritSectEnter(pNext), VINF_SUCCESS);
        else
        {
            RTTEST_CHECK_RC_OK(g_hTest, rc = testWaitForAllOtherThreadsToSleep(RTTHREADSTATE_CRITSECT, 1));
            if (RT_SUCCESS(rc))
                RTTEST_CHECK_RC(g_hTest, rc = RTCritSectEnter(pNext), VERR_SEM_LV_DEADLOCK);
        }
        RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
        if (RT_SUCCESS(rc))
            RTTEST_CHECK_RC(g_hTest, rc = RTCritSectLeave(pNext), VINF_SUCCESS);
    }
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTCritSectLeave(pMine), VINF_SUCCESS);
    RTTEST_CHECK_RC(g_hTest, RTCritSectLeave(pMine), VINF_SUCCESS);
    return VINF_SUCCESS;
}


static void test1(uint32_t cThreads, uint32_t cPasses)
{
    testIt(cThreads, cPasses, 0, test1Thread, "critsect");
}


static DECLCALLBACK(int) test2Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    RTSEMRW         hMine = g_ahSemRWs[i];
    RTSEMRW         hNext = g_ahSemRWs[(i + 1) % g_cThreads];
    int             rc;

    if (i & 1)
    {
        RTTEST_CHECK_RC_RET(g_hTest, RTSemRWRequestWrite(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS, rcCheck);
        if ((i & 3) == 3)
            RTTEST_CHECK_RC(g_hTest, RTSemRWRequestWrite(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS);
    }
    else
        RTTEST_CHECK_RC_RET(g_hTest, RTSemRWRequestRead(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS, rcCheck);
    if (testWaitForSemRWToBeOwned(hNext))
    {
        if (i != g_iDeadlockThread)
            RTTEST_CHECK_RC(g_hTest, rc = RTSemRWRequestWrite(hNext, RT_INDEFINITE_WAIT), VINF_SUCCESS);
        else
        {
            RTTEST_CHECK_RC_OK(g_hTest, rc = testWaitForAllOtherThreadsToSleep(RTTHREADSTATE_RW_WRITE, 1));
            if (RT_SUCCESS(rc))
            {
                if (g_cThreads > 1)
                    RTTEST_CHECK_RC(g_hTest, rc = RTSemRWRequestWrite(hNext, RT_INDEFINITE_WAIT), VERR_SEM_LV_DEADLOCK);
                else
                    RTTEST_CHECK_RC(g_hTest, rc = RTSemRWRequestWrite(hNext, RT_INDEFINITE_WAIT), VERR_SEM_LV_ILLEGAL_UPGRADE);
            }
        }
        RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
        if (RT_SUCCESS(rc))
            RTTEST_CHECK_RC(g_hTest, RTSemRWReleaseWrite(hNext), VINF_SUCCESS);
    }
    if (i & 1)
    {
        if ((i & 3) == 3)
            RTTEST_CHECK_RC(g_hTest, RTSemRWReleaseWrite(hMine), VINF_SUCCESS);
        RTTEST_CHECK_RC(g_hTest, RTSemRWReleaseWrite(hMine), VINF_SUCCESS);
    }
    else
        RTTEST_CHECK_RC(g_hTest, RTSemRWReleaseRead(hMine), VINF_SUCCESS);
    RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    return VINF_SUCCESS;
}


static void test2(uint32_t cThreads, uint32_t cPasses)
{
    testIt(cThreads, cPasses, 0, test2Thread, "read-write");
}


static DECLCALLBACK(int) test3Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    RTSEMRW         hMine = g_ahSemRWs[i];
    RTSEMRW         hNext = g_ahSemRWs[(i + 1) % g_cThreads];
    int             rc;

    if (i & 1)
        RTTEST_CHECK_RC_RET(g_hTest, RTSemRWRequestWrite(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS, rcCheck);
    else
        RTTEST_CHECK_RC_RET(g_hTest, RTSemRWRequestRead(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS, rcCheck);
    if (testWaitForSemRWToBeOwned(hNext))
    {
        do
        {
            rc = RTSemRWRequestWrite(hNext, 60*1000);
            if (rc != VINF_SUCCESS && rc != VERR_SEM_LV_DEADLOCK && rc != VERR_SEM_LV_ILLEGAL_UPGRADE)
            {
                RTTestFailed(g_hTest, "#%u: RTSemRWRequestWrite -> %Rrc\n", i, rc);
                break;
            }
            if (RT_SUCCESS(rc))
            {
                RTTEST_CHECK_RC(g_hTest, rc = RTSemRWReleaseWrite(hNext), VINF_SUCCESS);
                if (RT_FAILURE(rc))
                    break;
            }
            else
                ASMAtomicIncU32(&g_cDeadlocks);
            ASMAtomicIncU32(&g_cLoops);
        } while (RTTimeNanoTS() < g_NanoTSStop);
    }
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTSemRWReleaseWrite(hMine), VINF_SUCCESS);
    else
        RTTEST_CHECK_RC(g_hTest, RTSemRWReleaseRead(hMine), VINF_SUCCESS);
    RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    return VINF_SUCCESS;
}


static void test3(uint32_t cThreads, uint32_t cPasses, uint32_t cSecs)
{
    testIt(cThreads, cPasses, cSecs, test3Thread, "read-write race");
}


static DECLCALLBACK(int) test4Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    RTSEMRW         hMine = g_ahSemRWs[i];
    RTSEMRW         hNext = g_ahSemRWs[(i + 1) % g_cThreads];

    do
    {
        int rc1 = (i & 1 ? RTSemRWRequestWrite : RTSemRWRequestRead)(hMine, 60*1000); /* ugly ;-) */
        RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
        if (rc1 != VINF_SUCCESS && rc1 != VERR_SEM_LV_DEADLOCK && rc1 != VERR_SEM_LV_ILLEGAL_UPGRADE)
        {
            RTTestFailed(g_hTest, "#%u: RTSemRWRequest%s(hMine,) -> %Rrc\n", i, i & 1 ? "Write" : "read", rc1);
            break;
        }
        if (RT_SUCCESS(rc1))
        {
            for (unsigned iInner = 0; iInner < 4; iInner++)
            {
                int rc2 = RTSemRWRequestWrite(hNext, 60*1000);
                if (rc2 != VINF_SUCCESS && rc2 != VERR_SEM_LV_DEADLOCK && rc2 != VERR_SEM_LV_ILLEGAL_UPGRADE)
                {
                    RTTestFailed(g_hTest, "#%u: RTSemRWRequestWrite -> %Rrc\n", i, rc2);
                    break;
                }
                if (RT_SUCCESS(rc2))
                {
                    RTTEST_CHECK_RC(g_hTest, rc2 = RTSemRWReleaseWrite(hNext), VINF_SUCCESS);
                    if (RT_FAILURE(rc2))
                        break;
                }
                else
                    ASMAtomicIncU32(&g_cDeadlocks);
                ASMAtomicIncU32(&g_cLoops);
            }

            RTTEST_CHECK_RC(g_hTest, rc1 = (i & 1 ? RTSemRWReleaseWrite : RTSemRWReleaseRead)(hMine), VINF_SUCCESS);
            RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
            if (RT_FAILURE(rc1))
                break;
        }
        else
            ASMAtomicIncU32(&g_cDeadlocks);
        ASMAtomicIncU32(&g_cLoops);
    } while (RTTimeNanoTS() < g_NanoTSStop);

    return VINF_SUCCESS;
}


static void test4(uint32_t cThreads, uint32_t cPasses, uint32_t cSecs)
{
    testIt(cThreads, cPasses, cSecs, test4Thread, "read-write race v2");
}


static DECLCALLBACK(int) test5Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    RTSEMMUTEX      hMine = g_ahSemMtxes[i];
    RTSEMMUTEX      hNext = g_ahSemMtxes[(i + 1) % g_cThreads];

    RTTEST_CHECK_RC_RET(g_hTest, RTSemMutexRequest(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS, rcCheck);
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTSemMutexRequest(hMine, RT_INDEFINITE_WAIT), VINF_SUCCESS);
    if (testWaitForSemMutexToBeOwned(hNext))
    {
        int rc;
        if (i != g_iDeadlockThread)
            RTTEST_CHECK_RC(g_hTest, rc = RTSemMutexRequest(hNext, RT_INDEFINITE_WAIT), VINF_SUCCESS);
        else
        {
            RTTEST_CHECK_RC_OK(g_hTest, rc = testWaitForAllOtherThreadsToSleep(RTTHREADSTATE_MUTEX, 1));
            if (RT_SUCCESS(rc))
                RTTEST_CHECK_RC(g_hTest, rc = RTSemMutexRequest(hNext, RT_INDEFINITE_WAIT), VERR_SEM_LV_DEADLOCK);
        }
        RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
        if (RT_SUCCESS(rc))
            RTTEST_CHECK_RC(g_hTest, rc = RTSemMutexRelease(hNext), VINF_SUCCESS);
    }
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTSemMutexRelease(hMine), VINF_SUCCESS);
    RTTEST_CHECK_RC(g_hTest, RTSemMutexRelease(hMine), VINF_SUCCESS);
    return VINF_SUCCESS;
}


static void test5(uint32_t cThreads, uint32_t cPasses)
{
    testIt(cThreads, cPasses, 0, test5Thread, "mutex");
}


static DECLCALLBACK(int) test6Thread(RTTHREAD ThreadSelf, void *pvUser)
{
    uintptr_t       i     = (uintptr_t)pvUser;
    PRTCRITSECT     pMine = &g_aCritSects[i];
    PRTCRITSECT     pNext = &g_aCritSects[(i + 1) % g_cThreads];

    RTTEST_CHECK_RC_RET(g_hTest, RTCritSectEnter(pMine), VINF_SUCCESS, rcCheck);
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTCritSectEnter(pMine), VINF_SUCCESS);
    if (testWaitForCritSectToBeOwned(pNext))
    {
        int rc;
        if (i != g_iDeadlockThread)
        {
            RTTEST_CHECK_RC(g_hTest, rc = RTCritSectEnter(pNext), VINF_SUCCESS);
            RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
            if (RT_SUCCESS(rc))
                RTTEST_CHECK_RC(g_hTest, rc = RTCritSectLeave(pNext), VINF_SUCCESS);
        }
        else
        {
            RTTEST_CHECK_RC_OK(g_hTest, rc = testWaitForAllOtherThreadsToSleep(RTTHREADSTATE_CRITSECT, 1));
            if (RT_SUCCESS(rc))
            {
                RTSemEventSetSignaller(g_hSemEvt, g_ahThreads[0]);
                for (uint32_t iThread = 1; iThread < g_cThreads; iThread++)
                    RTSemEventAddSignaller(g_hSemEvt, g_ahThreads[iThread]);
                RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
                RTTEST_CHECK_RC(g_hTest, RTSemEventWait(g_hSemEvt, 10*1000), VERR_SEM_LV_DEADLOCK);
                RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
                RTTEST_CHECK_RC(g_hTest, RTSemEventSignal(g_hSemEvt), VINF_SUCCESS);
                RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
                RTTEST_CHECK_RC(g_hTest, RTSemEventWait(g_hSemEvt, 10*1000), VINF_SUCCESS);
                RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
                RTSemEventSetSignaller(g_hSemEvt, NIL_RTTHREAD);
            }
        }
        RTTEST_CHECK(g_hTest, RTThreadGetState(RTThreadSelf()) == RTTHREADSTATE_RUNNING);
    }
    if (i & 1)
        RTTEST_CHECK_RC(g_hTest, RTCritSectLeave(pMine), VINF_SUCCESS);
    RTTEST_CHECK_RC(g_hTest, RTCritSectLeave(pMine), VINF_SUCCESS);
    return VINF_SUCCESS;
}


static void test6(uint32_t cThreads, uint32_t cPasses)
{
    testIt(cThreads, cPasses, 0, test6Thread, "event");
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

    RTSEMRW hSemRW;
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemRWCreate(&hSemRW), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemRWRequestRead(hSemRW, 50), false);
    int rc = RTSemRWRequestWrite(hSemRW, 1);
    if (rc != VERR_SEM_LV_ILLEGAL_UPGRADE)
        fRet = false;
    RTTEST_CHECK_RET(g_hTest, RT_FAILURE_NP(rc), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemRWReleaseRead(hSemRW), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemRWDestroy(hSemRW), false);

#if 0 /** @todo detect it on RTSemMutex... */
    RTSEMMUTEX hSemMtx;
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemMutexCreate(&hSemRW), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemMutexRequest(hSemRW, 50), false);
    /*??*/
    RTTEST_CHECK_RET(g_hTest, RT_FAILURE_NP(rc), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemRWRelease(hSemRW), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemRWDestroy(hSemRW), false);
#endif

    RTSEMEVENT hSemEvt;
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemEventCreate(&hSemEvt), false);
    RTSemEventSetSignaller(hSemEvt, RTThreadSelf());
    RTSemEventSetSignaller(hSemEvt, NIL_RTTHREAD);
    rc = RTSemEventSignal(hSemEvt);
    if (rc != VERR_SEM_LV_NOT_SIGNALLER)
        fRet = false;
    RTTEST_CHECK_RET(g_hTest, RT_FAILURE_NP(rc), false);
    RTTEST_CHECK_RC_OK_RET(g_hTest, RTSemEventDestroy(hSemEvt), false);

    return fRet;
}


int main()
{
    /*
     * Init.
     */
    int rc = RTTestInitAndCreate("tstRTLockValidator", &g_hTest);
    if (rc)
        return rc;
    RTTestBanner(g_hTest);

    RTLockValidatorSetEnabled(true);
    RTLockValidatorSetMayPanic(false);
    RTLockValidatorSetQuiet(true);
    if (!testIsLockValidationCompiledIn())
        return RTTestErrorCount(g_hTest) > 0
            ? RTTestSummaryAndDestroy(g_hTest)
            : RTTestSkipAndDestroy(g_hTest, "deadlock detection is not compiled in");
    RTLockValidatorSetQuiet(false);

    /*
     * Some initial tests with verbose output.
     */
#if 1
    test1(3, 1);
    test2(1, 1);
    test2(3, 1);
    test5(3, 1);
    test6(3, 1);
#endif

    /*
     * More thorough testing without noisy output.
     */
    RTLockValidatorSetQuiet(true);
    test1( 2, 256);                     /* 256 * 4ms = 1s (approx); 4ms == fudge factor */
    test1( 3, 256);
    test1( 7, 256);
    test1(10, 256);
    test1(15, 256);
    test1(30, 256);

#if 1
    test2( 1, 256);
    test2( 2, 256);
    test2( 3, 256);
    test2( 7, 256);
    test2(10, 256);
    test2(15, 256);
    test2(30, 256);

    test3( 2,  1,  2);
    test3(10,  1,  2);

    test4( 2,  1,  2);
    test4( 6,  1,  2);
    test4(10,  1, 10);
    test4(30,  1, 10);

    test5( 2, 256);
    test5( 3, 256);
    test5( 7, 256);
    test5(10, 256);
    test5(15, 256);
    test5(30, 256);
#endif

    test6( 2, 256);
    test6( 3, 256);
    test6( 7, 256);
    test6(10, 256);
    test6(15, 256);
    test6(30, 256);

    return RTTestSummaryAndDestroy(g_hTest);
}

