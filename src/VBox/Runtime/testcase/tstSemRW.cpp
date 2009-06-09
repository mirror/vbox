/* $Id$ */
/** @file
 * IPRT Testcase - Reader/Writer Semaphore Test.
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
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/time.h>
#include <iprt/initterm.h>
#include <iprt/rand.h>
#include <iprt/asm.h>
#include <iprt/assert.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTSEMRW              g_hSemRW = NIL_RTSEMRW;
static bool volatile        g_fTerminate;
static bool                 g_fYield;
static bool                 g_fQuiet;
static unsigned             g_uWritePercent;
static uint32_t volatile    g_cbConcurrentWrite;
static uint32_t volatile    g_cbConcurrentRead;
static uint32_t volatile    g_cErrors;


int PrintError(const char *pszFormat, ...)
{
    ASMAtomicIncU32(&g_cErrors);

    RTPrintf("tstSemRW: FAILURE - ");
    va_list va;
    va_start(va, pszFormat);
    RTPrintfV(pszFormat, va);
    va_end(va);

    return 1;
}


int ThreadTest1(RTTHREAD ThreadSelf, void *pvUser)
{
    // Use randomization to get a little more variation of the sync pattern
    unsigned c100 = RTRandU32Ex(0, 99);
    uint64_t *pu64 = (uint64_t *)pvUser;
    bool fWrite;
    for (;;)
    {
        int rc;
        fWrite = (c100 < g_uWritePercent);
        if (fWrite)
        {
            rc = RTSemRWRequestWriteNoResume(g_hSemRW, RT_INDEFINITE_WAIT);
            if (RT_FAILURE(rc))
            {
                PrintError("%x: RTSemRWRequestWriteNoResume failed with %Rrc\n", rc);
                break;
            }
            if (ASMAtomicIncU32(&g_cbConcurrentWrite) != 1)
            {
                PrintError("g_cbConcurrentWrite=%d after request!\n", g_cbConcurrentWrite);
                break;
            }
            if (g_cbConcurrentRead != 0)
            {
                PrintError("g_cbConcurrentRead=%d after request!\n", g_cbConcurrentRead);
                break;
            }
        }
        else
        {
            rc = RTSemRWRequestReadNoResume(g_hSemRW, RT_INDEFINITE_WAIT);
            if (RT_FAILURE(rc))
            {
                PrintError("%x: RTSemRWRequestReadNoResume failed with %Rrc\n", rc);
                break;
            }
            ASMAtomicIncU32(&g_cbConcurrentRead);
            if (g_cbConcurrentWrite != 0)
            {
                PrintError("g_cbConcurrentWrite=%d after request!\n", g_cbConcurrentWrite);
                break;
            }
        }

        /*
         * Check for fairness: The values of the threads should not differ too much
         */
        (*pu64)++;

        /*
         * Check for correctness: Give other threads a chance. If the implementation is
         * correct, no other thread will be able to enter this lock now.
         */
        if (g_fYield)
            RTThreadYield();

        if (fWrite)
        {
            if (ASMAtomicDecU32(&g_cbConcurrentWrite) != 0)
            {
                PrintError("g_cbConcurrentWrite=%d before release!\n", g_cbConcurrentWrite);
                break;
            }
            if (g_cbConcurrentRead != 0)
            {
                PrintError("g_cbConcurrentRead=%d before release!\n", g_cbConcurrentRead);
                break;
            }
            rc = RTSemRWReleaseWrite(g_hSemRW);
            if (RT_FAILURE(rc))
            {
                PrintError("%x: RTSemRWReleaseWrite failed with %Rrc\n", rc);
                break;
            }
        }
        else
        {
            if (g_cbConcurrentWrite != 0)
            {
                PrintError("g_cbConcurrentWrite=%d before release!\n", g_cbConcurrentWrite);
                break;
            }
            ASMAtomicDecU32(&g_cbConcurrentRead);
            rc = RTSemRWReleaseRead(g_hSemRW);
            if (RT_FAILURE(rc))
            {
                PrintError("%x: RTSemRWReleaseRead failed with %Rrc\n", rc);
                break;
            }
        }

        if (g_fTerminate)
            break;

        c100++;
        c100 %= 100;
    }
    if (!g_fQuiet)
        RTPrintf("tstSemRW: Thread %08x exited with %lld\n", ThreadSelf, *pu64);
    return VINF_SUCCESS;
}


static int Test1(unsigned cThreads, unsigned cSeconds, unsigned uWritePercent, bool fYield, bool fQuiet)
{
    int rc;
    unsigned i;
    uint64_t g_au64[32];
    RTTHREAD aThreads[RT_ELEMENTS(g_au64)];
    AssertRelease(cThreads <= RT_ELEMENTS(g_au64));

    /*
     * Init globals.
     */
    g_fYield = fYield;
    g_fQuiet = fQuiet;
    g_fTerminate = false;
    g_uWritePercent = uWritePercent;
    g_cbConcurrentWrite = 0;
    g_cbConcurrentRead = 0;

    rc = RTSemRWCreate(&g_hSemRW);
    if (RT_FAILURE(rc))
        return PrintError("RTSemRWCreate failed (rc=%Rrc)\n", rc);

    /*
     * Create the threads and let them block on the semrw.
     */
    rc = RTSemRWRequestWrite(g_hSemRW, RT_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
        return PrintError("RTSemRWRequestWrite failed (rc=%Rrc)\n", rc);

    for (i = 0; i < cThreads; i++)
    {
        g_au64[i] = 0;
        rc = RTThreadCreate(&aThreads[i], ThreadTest1, &g_au64[i], 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "test");
        if (RT_FAILURE(rc))
            return PrintError("RTThreadCreate failed for thread %u (rc=%Rrc)\n", i, rc);
    }

    if (!fQuiet)
        RTPrintf("tstSemRW: %zu Threads created. Racing them for %u seconds (%s) ...\n",
                 cThreads, cSeconds, g_fYield ? "yielding" : "no yielding");

    uint64_t u64StartTS = RTTimeNanoTS();
    rc = RTSemRWReleaseWrite(g_hSemRW);
    if (RT_FAILURE(rc))
        PrintError("RTSemRWReleaseWrite failed (rc=%Rrc)\n", rc);
    RTThreadSleep(cSeconds * 1000);
    ASMAtomicXchgBool(&g_fTerminate, true);
    uint64_t ElapsedNS = RTTimeNanoTS() - u64StartTS;

    for (i = 0; i < cThreads; i++)
    {
        rc = RTThreadWait(aThreads[i], 5000, NULL);
        if (RT_FAILURE(rc))
            PrintError("RTThreadWait failed for thread %u (rc=%Rrc)\n", i, rc);
    }

    if (g_cbConcurrentWrite != 0)
        PrintError("g_cbConcurrentWrite=%d at end of test!\n", g_cbConcurrentWrite);
    if (g_cbConcurrentRead != 0)
        PrintError("g_cbConcurrentRead=%d at end of test!\n", g_cbConcurrentRead);

    rc = RTSemRWDestroy(g_hSemRW);
    if (RT_FAILURE(rc))
        PrintError("RTSemRWDestroy failed - %Rrc\n", rc);
    g_hSemRW = NIL_RTSEMRW;
    if (g_cErrors)
        RTThreadSleep(100);

    /*
     * Collect and display the results.
     */
    uint64_t Total = g_au64[0];
    for (i = 1; i < cThreads; i++)
        Total += g_au64[i];

    uint64_t Normal = Total / cThreads;
    uint64_t MaxDeviation = 0;
    for (i = 0; i < cThreads; i++)
    {
        uint64_t Delta = RT_ABS((int64_t)(g_au64[i] - Normal));
        if (Delta > Normal / 2)
            RTPrintf("tstSemRW: Warning! Thread %d deviates by more than 50%% - %llu (it) vs. %llu (avg)\n",
                     i, g_au64[i], Normal);
        if (Delta > MaxDeviation)
            MaxDeviation = Delta;

    }

    RTPrintf("tstSemRW: Threads: %u  Total: %llu  Per Sec: %llu  Avg: %llu ns  Max dev: %llu%%\n",
             cThreads,
             Total,
             Total / cSeconds,
             ElapsedNS / Total,
             MaxDeviation * 100 / Normal
             );
    return 0;
}


int main(int argc, char **argv)
{
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSemRW: RTR3Init failed (rc=%Rrc)\n", rc);
        return 1;
    }
    RTPrintf("tstSemRW: TESTING...\n");

    if (argc == 1)
    {
        /*    threads, seconds, writePercent,  yield,  quiet */
        Test1(      1,       1,            0,   true,  false);
        Test1(      1,       1,            1,   true,  false);
        Test1(      1,       1,            5,   true,  false);
        Test1(      2,       1,            3,   true,  false);
        Test1(     10,       1,            5,   true,  false);
        Test1(     10,      10,           10,  false,  false);

        RTPrintf("tstSemRW: benchmarking...\n");
        for (unsigned cThreads = 1; cThreads < 32; cThreads++)
            Test1(cThreads,  2,            1,  false,   true);

        /** @todo add a testcase where some stuff times out. */
    }
    else
    {
        /*    threads, seconds, writePercent,  yield,  quiet */
        RTPrintf("tstSemRW: benchmarking...\n");
        Test1(      1,       3,            1,  false,   true);
        Test1(      1,       3,            1,  false,   true);
        Test1(      1,       3,            1,  false,   true);
        Test1(      2,       3,            1,  false,   true);
        Test1(      2,       3,            1,  false,   true);
        Test1(      2,       3,            1,  false,   true);
        Test1(      3,       3,            1,  false,   true);
        Test1(      3,       3,            1,  false,   true);
        Test1(      3,       3,            1,  false,   true);
    }

    if (!g_cErrors)
        RTPrintf("tstSemRW: SUCCESS\n");
    else
        RTPrintf("tstSemRW: FAILURE - %u errors\n", g_cErrors);
    return g_cErrors != 0;
}

