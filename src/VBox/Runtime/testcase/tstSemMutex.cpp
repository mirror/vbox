/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Simple Semaphore Smoke Test.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/stream.h>
#include <iprt/initterm.h>
#include <iprt/asm.h>

#define SECONDS 10

static RTSEMMUTEX g_mutex;
static uint64_t   g_au64[10];
static bool       g_fTerminate;
static bool       g_fYield = true;
static uint32_t   g_cbConcurrent;

static uint32_t volatile    g_cErrors;


int PrintError(const char *pszFormat, ...)
{
    ASMAtomicIncU32(&g_cErrors);

    RTPrintf("tstSemMutex: FAILURE - ");
    va_list va;
    va_start(va, pszFormat);
    RTPrintfV(pszFormat, va);
    va_end(va);

    return 1;
}


int ThreadTest(RTTHREAD ThreadSelf, void *pvUser)
{
    uint64_t *pu64 = (uint64_t *)pvUser;
    for (;;)
    {
        int rc = RTSemMutexRequestNoResume(g_mutex, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
        {
            PrintError("%x: RTSemMutexRequestNoResume failed with %Rrc\n", rc);
            break;
        }
        if (ASMAtomicIncU32(&g_cbConcurrent) != 1)
        {
            PrintError("g_cbConcurrent=%d after request!\n", g_cbConcurrent);
            break;
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
        if (ASMAtomicDecU32(&g_cbConcurrent) != 0)
        {
            PrintError("g_cbConcurrent=%d before release!\n", g_cbConcurrent);
            break;
        }
        rc = RTSemMutexRelease(g_mutex);
        if (RT_FAILURE(rc))
        {
            PrintError("%x: RTSemMutexRelease failed with %Rrc\n", rc);
            break;
        }
        if (g_fTerminate)
            break;
    }
    RTPrintf("tstSemMutex: Thread %08x exited with %lld\n", ThreadSelf, *pu64);
    return VINF_SUCCESS;
}

int main()
{
    int rc;
    unsigned u;
    RTTHREAD aThreads[RT_ELEMENTS(g_au64)];

    rc = RTR3Init(false, 0);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSemMutex: RTR3Init failed (rc=%Rrc)\n", rc);
        return 1;
    }


    rc = RTSemMutexCreate(&g_mutex);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSemMutex: RTSemMutexCreate failed (rc=%Rrc)\n", rc);
        return 1;
    }
    for (u = 0; u < RT_ELEMENTS(g_au64); u++)
    {
        rc = RTThreadCreate(&aThreads[u], ThreadTest, &g_au64[u], 0, RTTHREADTYPE_DEFAULT, 0, "test");
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstSemMutex: RTThreadCreate failed for thread %u (rc=%Rrc)\n", u, rc);
            return 1;
        }
    }

    RTPrintf("tstSemMutex: %zu Threads created. Racing them for %u seconds (%s) ...\n",
             RT_ELEMENTS(g_au64), SECONDS, g_fYield ? "yielding" : "no yielding");
    RTThreadSleep(SECONDS * 1000);
    g_fTerminate = true;
    RTThreadSleep(100);
    RTSemMutexDestroy(g_mutex);
    RTThreadSleep(100);

    for (u = 1; u < RT_ELEMENTS(g_au64); u++)
        g_au64[0] += g_au64[u];
    RTPrintf("tstSemMutex: Done. In total: %lld\n", g_au64[0]);


    if (!g_cErrors)
        RTPrintf("tstSemMutex: SUCCESS\n");
    else
        RTPrintf("tstSemMutex: FAILURE - %u errors\n", g_cErrors);
    return g_cErrors != 0;
}

