/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Critical Sections.
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
#ifdef TRY_WIN32_CRIT
# include <Windows.h>
#endif
#include <iprt/critsect.h>
#include <iprt/lock.h>
#include <iprt/thread.h>
#include <iprt/log.h>
#include <iprt/semaphore.h>
#include <iprt/asm.h>
#include <iprt/runtime.h>
#include <iprt/time.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/err.h>

#include <stdio.h>
#include <stdlib.h>

#ifndef TRY_WIN32_CRIT
#define LOCKERS(sect)   ((sect).cLockers)
#else
/* This is for comparing with the "real thing". */
#define RTCRITSECT      CRITICAL_SECTION
#define PRTCRITSECT     LPCRITICAL_SECTION
#define LOCKERS(sect)   (*(LONG volatile *)&(sect).LockCount)

inline int RTCritSectInit(PCRITICAL_SECTION pCritSect)
{
    InitializeCriticalSection(pCritSect);
    return VINF_SUCCESS;
}

#undef RTCritSectEnter
inline int RTCritSectEnter(PCRITICAL_SECTION pCritSect)
{
    EnterCriticalSection(pCritSect);
    return VINF_SUCCESS;
}

inline int RTCritSectLeave(PCRITICAL_SECTION pCritSect)
{
    LeaveCriticalSection(pCritSect);
    return VINF_SUCCESS;
}

inline int RTCritSectDelete(PCRITICAL_SECTION pCritSect)
{
    DeleteCriticalSection(pCritSect);
    return VINF_SUCCESS;
}
#endif

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Arguments to ThreadTest1().
 */
typedef struct THREADTEST1ARGS
{
    /** The critical section. */
    PRTCRITSECT         pCritSect;
    /** The thread ordinal. */
    uint32_t            iThread;
    /** Pointer to the release counter. */
    uint32_t volatile  *pu32Release;
} THREADTEST1ARGS, *PTHREADTEST1ARGS;


/**
 * Arguments to ThreadTest2().
 */
typedef struct THREADTEST2ARGS
{
    /** The critical section. */
    PRTCRITSECT         pCritSect;
    /** The thread ordinal. */
    uint32_t            iThread;
    /** Pointer to the release counter. */
    uint32_t volatile  *pu32Release;
    /** Pointer to the alone indicator. */
    uint32_t volatile  *pu32Alone;
    /** Pointer to the previous thread variable. */
    uint32_t volatile  *pu32Prev;
    /** Pointer to the sequential enters counter. */
    uint32_t volatile  *pcSeq;
    /** Pointer to the reordered enters counter. */
    uint32_t volatile  *pcReordered;
    /** Pointer to the variable counting running threads. */
    uint32_t volatile  *pcThreadRunning;
    /** Number of times this thread was inside the section. */
    uint32_t volatile   cTimes;
    /** The number of threads. */
    uint32_t            cThreads;
    /** Number of iterations (sum of all threads). */
    uint32_t            cIterations;
    /** Yield while inside the section. */
    unsigned            cCheckLoops;
    /** Signal this when done. */
    RTSEMEVENT          EventDone;
} THREADTEST2ARGS, *PTHREADTEST2ARGS;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Error counter. */
static volatile uint32_t g_cErrors = 0;

/**
 * Thread which goes to sleep on the critsect and checks that it's released in the right order.
 */
static DECLCALLBACK(int) ThreadTest1(RTTHREAD ThreadSelf, void *pvArg)
{
    PTHREADTEST1ARGS pArgs = (PTHREADTEST1ARGS)pvArg;
    Log2(("ThreadTest1: Start - iThread=%d ThreadSelf=%p\n", pArgs->iThread, ThreadSelf));

    /*
     * Enter it.
     */
    int rc = RTCritSectEnter(pArgs->pCritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - thread %d: RTCritSectEnter -> %d\n", pArgs->iThread, rc);
        ASMAtomicIncU32(&g_cErrors);
        exit(g_cErrors);
        return 1;
    }

    /*
     * Check release order.
     */
    if (*pArgs->pu32Release != pArgs->iThread)
    {
        printf("tstCritSect: FAILURE - thread %d: released as number %d\n", pArgs->iThread, *pArgs->pu32Release);
        ASMAtomicIncU32(&g_cErrors);
    }
    ASMAtomicIncU32(pArgs->pu32Release);

    /*
     * Leave it.
     */
    rc = RTCritSectLeave(pArgs->pCritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - thread %d: RTCritSectEnter -> %d\n", pArgs->iThread, rc);
        ASMAtomicIncU32(&g_cErrors);
        exit(g_cErrors);
        return 1;
    }

    Log2(("ThreadTest1: End - iThread=%d ThreadSelf=%p\n", pArgs->iThread, ThreadSelf));
    return 0;
}


int Test1(unsigned cThreads)
{
    /*
     * Create a critical section.
     */
    RTCRITSECT CritSect;
    int rc = RTCritSectInit(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTCritSectInit -> %d\n", rc);
        return 1;
    }

    /*
     * Enter, leave and enter again.
     */
    rc = RTCritSectEnter(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTCritSectEnter -> %d\n", rc);
        return 1;
    }
    rc = RTCritSectLeave(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTCritSectLeave -> %d\n", rc);
        return 1;
    }
    rc = RTCritSectEnter(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTCritSectEnter -> %d (2nd)\n", rc);
        return 1;
    }

    /*
     * Now spawn threads which will go to sleep entering the critsect.
     */
    uint32_t    u32Release = 0;
    for (uint32_t iThread = 0; iThread < cThreads; iThread++)
    {
        PTHREADTEST1ARGS pArgs = (PTHREADTEST1ARGS)calloc(sizeof(*pArgs), 1);
        pArgs->iThread = iThread;
        pArgs->pCritSect = &CritSect;
        pArgs->pu32Release = &u32Release;
        int32_t     iLock = LOCKERS(CritSect);
        char szThread[17];
        RTStrPrintf(szThread, sizeof(szThread), "T%d", iThread);
        RTTHREAD  Thread;
        rc = RTThreadCreate(&Thread, ThreadTest1, pArgs, 0, RTTHREADTYPE_DEFAULT, 0, szThread);
        if (RT_FAILURE(rc))
        {
            printf("tstCritSect: FATAL FAILURE - RTThreadCreate -> %d\n", rc);
            exit(1);
        }
        /* wait for it to get into waiting. */
        while (LOCKERS(CritSect) == iLock)
            RTThreadSleep(10);
        RTThreadSleep(20);
    }

    /*
     * Now we'll release the threads and wait for all of them to quit.
     */
    u32Release = 0;
    rc = RTCritSectLeave(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTCritSectLeave -> %d (2nd)\n", rc);
        return 1;
    }
    while (u32Release < cThreads)
        RTThreadSleep(10);

    rc = RTCritSectDelete(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FAILURE - RTCritSectDelete -> %d\n", rc);
        ASMAtomicIncU32(&g_cErrors);
    }

    return 0;
}



/**
 * Thread which goes to sleep on the critsect and checks
 * that it's released along and in the right order. This is done a number of times.
 *
 */
static DECLCALLBACK(int) ThreadTest2(RTTHREAD ThreadSelf, void *pvArg)
{
    PTHREADTEST2ARGS pArgs = (PTHREADTEST2ARGS)pvArg;
    Log2(("ThreadTest2: Start - iThread=%d ThreadSelf=%p\n", pArgs->iThread, ThreadSelf));
    uint64_t    u64TSStart = 0;
    ASMAtomicIncU32(pArgs->pcThreadRunning);

    for (unsigned i = 0; *pArgs->pu32Release < pArgs->cIterations; i++)
    {
        /*
         * Enter it.
         */
        int rc = RTCritSectEnter(pArgs->pCritSect);
        if (RT_FAILURE(rc))
        {
            printf("tstCritSect: FATAL FAILURE - Test 2 - thread %d, iteration %d: RTCritSectEnter -> %d\n", pArgs->iThread, i, rc);
            ASMAtomicIncU32(&g_cErrors);
            exit(g_cErrors);
            return 1;
        }
        if (!u64TSStart)
            u64TSStart = RTTimeNanoTS();

        #if 0 /* We just check for sequences. */
        /*
         * Check release order.
         */
        if ((*pArgs->pu32Release % pArgs->cThreads) != pArgs->iThread)
        {
            printf("tstCritSect: FAILURE - Test 2 - thread %d, iteration %d: released as number %d (%d)\n",
                   pArgs->iThread, i, *pArgs->pu32Release % pArgs->cThreads, *pArgs->pu32Release);
            ASMAtomicIncU32(&g_cErrors);
        }
        else
            printf("tstCritSect: SUCCESS - Test 2 - thread %d, iteration %d: released as number %d (%d)\n",
                   pArgs->iThread, i, *pArgs->pu32Release % pArgs->cThreads, *pArgs->pu32Release);
        #endif
        pArgs->cTimes++;
        ASMAtomicIncU32(pArgs->pu32Release);

        /*
         * Check distribution every now and again.
         */
#if 0
        if (!(*pArgs->pu32Release % 879))
        {
            uint32_t u32Perfect = *pArgs->pu32Release / pArgs->cThreads;
            for (int iThread = 0 ; iThread < (int)pArgs->cThreads; iThread++)
            {
                int cDiff = pArgs[iThread - pArgs->iThread].cTimes - u32Perfect;
                if ((unsigned)RT_ABS(cDiff) > RT_MAX(u32Perfect / 10000, 2))
                {
                    printf("tstCritSect: FAILURE - bad distribution thread %d u32Perfect=%d cTimes=%d cDiff=%d (runtime)\n",
                           iThread, u32Perfect, pArgs[iThread - pArgs->iThread].cTimes, cDiff);
                    ASMAtomicIncU32(&g_cErrors);
                }
            }
        }
#endif
        /*
         * Check alone and make sure we stay inside here a while
         * so the other guys can get ready.
         */
        uint32_t u32;
        for (u32 = 0; u32 < pArgs->cCheckLoops; u32++)
        {
            if (*pArgs->pu32Alone != ~0U)
            {
                printf("tstCritSect: FATAL FAILURE - Test 2 - thread %d, iteration %d: not alone!!!\n", pArgs->iThread, i);
                AssertReleaseMsgFailed(("Not alone!\n"));
                ASMAtomicIncU32(&g_cErrors);
                exit(g_cErrors);
                return 1;
            }
        }
        ASMAtomicCmpXchgU32(pArgs->pu32Alone, pArgs->iThread, ~0);
        for (u32 = 0; u32 < pArgs->cCheckLoops; u32++)
        {
            if (*pArgs->pu32Alone != pArgs->iThread)
            {
                printf("tstCritSect: FATAL FAILURE - Test 2 - thread %d, iteration %d: not alone!!!\n", pArgs->iThread, i);
                AssertReleaseMsgFailed(("Not alone!\n"));
                ASMAtomicIncU32(&g_cErrors);
                exit(g_cErrors);
                return 1;
            }
        }
        ASMAtomicXchgU32(pArgs->pu32Alone, ~0);

        /*
         * Check for sequences.
         */
        if (*pArgs->pu32Prev == pArgs->iThread && pArgs->cThreads > 1)
            ASMAtomicIncU32(pArgs->pcSeq);
        else if ((*pArgs->pu32Prev + 1) % pArgs->cThreads != pArgs->iThread)
            ASMAtomicIncU32(pArgs->pcReordered);
        ASMAtomicXchgU32(pArgs->pu32Prev, pArgs->iThread);

        /*
         * Leave it.
         */
        rc = RTCritSectLeave(pArgs->pCritSect);
        if (RT_FAILURE(rc))
        {
            printf("tstCritSect: FATAL FAILURE - Test 2 - thread %d, iteration %d: RTCritSectEnter -> %d\n", pArgs->iThread, i, rc);
            ASMAtomicIncU32(&g_cErrors);
            exit(g_cErrors);
            return 1;
        }
    }

    uint64_t u64TSEnd = RTTimeNanoTS(); NOREF(u64TSEnd);
    ASMAtomicDecU32(pArgs->pcThreadRunning);
    RTSemEventSignal(pArgs->EventDone);
    Log2(("ThreadTest2: End - iThread=%d ThreadSelf=%p time=%lld\n", pArgs->iThread, ThreadSelf, u64TSEnd - u64TSStart));
    return 0;
}

int Test2(unsigned cThreads, unsigned cIterations, unsigned cCheckLoops)
{
    printf("tstCritSect: Test2 - cThread=%d cIterations=%d cCheckLoops=%d...\n", cThreads, cIterations, cCheckLoops);

    /*
     * Create a critical section.
     */
    RTCRITSECT CritSect;
    int rc = RTCritSectInit(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - Test 2 - RTCritSectInit -> %d\n", rc);
        return 1;
    }

    /*
     * Enter, leave and enter again.
     */
    rc = RTCritSectEnter(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - Test 2 - RTCritSectEnter -> %d\n", rc);
        return 1;
    }
    rc = RTCritSectLeave(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - Test 2 - RTCritSectLeave -> %d\n", rc);
        return 1;
    }
    rc = RTCritSectEnter(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - Test 2 - RTCritSectEnter -> %d (2nd)\n", rc);
        return 1;
    }

    /*
     * Now spawn threads which will go to sleep entering the critsect.
     */
    PTHREADTEST2ARGS paArgs = (PTHREADTEST2ARGS)calloc(sizeof(THREADTEST2ARGS), cThreads);
    RTSEMEVENT    EventDone;
    rc = RTSemEventCreate(&EventDone);
    uint32_t volatile   u32Release = 0;
    uint32_t volatile   u32Alone = ~0;
    uint32_t volatile   u32Prev = ~0;
    uint32_t volatile   cSeq = 0;
    uint32_t volatile   cReordered = 0;
    uint32_t volatile   cThreadRunning = 0;
    unsigned iThread;
    for (iThread = 0; iThread < cThreads; iThread++)
    {
        paArgs[iThread].iThread     = iThread;
        paArgs[iThread].pCritSect   = &CritSect;
        paArgs[iThread].pu32Release = &u32Release;
        paArgs[iThread].pu32Alone   = &u32Alone;
        paArgs[iThread].pu32Prev    = &u32Prev;
        paArgs[iThread].pcSeq       = &cSeq;
        paArgs[iThread].pcReordered = &cReordered;
        paArgs[iThread].pcThreadRunning = &cThreadRunning;
        paArgs[iThread].cTimes      = 0;
        paArgs[iThread].cThreads    = cThreads;
        paArgs[iThread].cIterations = cIterations;
        paArgs[iThread].cCheckLoops = cCheckLoops;
        paArgs[iThread].EventDone   = EventDone;
        int32_t     iLock = LOCKERS(CritSect);
        char szThread[17];
        RTStrPrintf(szThread, sizeof(szThread), "T%d", iThread);
        RTTHREAD  Thread;
        rc = RTThreadCreate(&Thread, ThreadTest2, &paArgs[iThread], 0, RTTHREADTYPE_DEFAULT, 0, szThread);
        if (RT_FAILURE(rc))
        {
            printf("tstCritSect: FATAL FAILURE - Test 2 - RTThreadCreate -> %d\n", rc);
            exit(1);
        }
        /* wait for it to get into waiting. */
        while (LOCKERS(CritSect) == iLock)
            RTThreadSleep(10);
        RTThreadSleep(20);
    }
    printf("tstCritSect: Test2 - threads created...\n");

    /*
     * Now we'll release the threads and wait for all of them to quit.
     */
    u32Release = 0;
    uint64_t u64TSStart = RTTimeNanoTS();
    rc = RTCritSectLeave(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTCritSectLeave -> %d (2nd)\n", rc);
        return 1;
    }

    while (cThreadRunning > 0)
        RTSemEventWait(EventDone, RT_INDEFINITE_WAIT);
    uint64_t u64TSEnd = RTTimeNanoTS();

    /*
     * Clean up and report results.
     */
    rc = RTCritSectDelete(&CritSect);
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FAILURE - RTCritSectDelete -> %d\n", rc);
        ASMAtomicIncU32(&g_cErrors);
    }

    /* sequences */
    if (cSeq > RT_MAX(u32Release / 10000, 1))
    {
        printf("tstCritSect: FAILURE - too many same thread sequences! cSeq=%d\n", cSeq);
        ASMAtomicIncU32(&g_cErrors);
    }

    /* distribution caused by sequences / reordering. */
    unsigned cDiffTotal = 0;
    uint32_t u32Perfect = (u32Release + cThreads / 2) / cThreads;
    for (iThread = 0; iThread < cThreads; iThread++)
    {
        int cDiff = paArgs[iThread].cTimes - u32Perfect;
        if ((unsigned)RT_ABS(cDiff) > RT_MAX(u32Perfect / 10000, 2))
        {
            printf("tstCritSect: FAILURE - bad distribution thread %d u32Perfect=%d cTimes=%d cDiff=%d\n",
                   iThread, u32Perfect, paArgs[iThread].cTimes, cDiff);
            ASMAtomicIncU32(&g_cErrors);
        }
        cDiffTotal += RT_ABS(cDiff);
    }

    uint32_t cMillies = (uint32_t)((u64TSEnd - u64TSStart) / 1000000);
    printf("tstCritSect: Test2 - DONE. %d enter+leave in %dms cSeq=%d cReordered=%d cDiffTotal=%d\n",
           u32Release, cMillies, cSeq, cReordered, cDiffTotal);
    return 0;
}


int main(int argc, char *argv[])
{
    printf("tstCritSect: TESTING\n");

    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        printf("tstCritSect: FATAL FAILURE - RTR3Init -> %d\n", rc);
        return 1;
    }

    printf("tstCritSect: Test1...\n");
    if (Test1(1))
        return 1;
    if (Test1(3))
        return 1;
    if (Test1(10))
        return 1;
    if (Test1(63))
        return 1;
    if (Test2(1, 200000, 1000))
        return 1;
    if (Test2(2, 200000, 1000))
        return 1;
    if (Test2(3, 200000, 1000))
        return 1;
    if (Test2(4, 200000, 1000))
        return 1;
    if (Test2(5, 200000, 1000))
        return 1;
    if (Test2(7, 200000, 1000))
        return 1;
    if (Test2(67, 200000, 1000))
        return 1;

    /*
     * Summary.
     */
    if (!g_cErrors)
        printf("tstCritSect: SUCCESS\n");
    else
        printf("tstCritSect: FAILURE - %d errors\n", g_cErrors);

    return !!g_cErrors;
}
