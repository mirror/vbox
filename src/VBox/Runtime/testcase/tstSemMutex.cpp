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

#include <stdio.h>
#include <stdlib.h>

#define SECONDS 10

static RTSEMMUTEX g_mutex;
static uint64_t   g_au64[10];
static bool       g_fTerminate;
static uint32_t   g_cbConcurrent;


int ThreadTest(RTTHREAD ThreadSelf, void *pvUser)
{
    uint64_t *pu64 = (uint64_t*) pvUser;
    for (;;)
    {
        int rc = RTSemMutexRequestNoResume(g_mutex, RT_INDEFINITE_WAIT);
        if (RT_FAILURE(rc))
        {
            RTPrintf("%x: RTSemMutexRequestNoResume failed with %Vrc\n", rc);
            break;
        }
        if (ASMAtomicIncU32(&g_cbConcurrent) != 1)
        {
            RTPrintf("g_cbConcurrent=%d after request!\n", g_cbConcurrent);
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
        RTThreadYield();
        if (ASMAtomicDecU32(&g_cbConcurrent) != 0)
        {
            RTPrintf("g_cbConcurrent=%d before release!\n", g_cbConcurrent);
            break;
        }
        rc = RTSemMutexRelease(g_mutex);
        if (RT_FAILURE(rc))
        {
            RTPrintf("%x: RTSemMutexRelease failed with %Vrc\n", rc);
            break;
        }
        if (g_fTerminate)
            break;
    }
    RTPrintf("Thread %08x exited with %lld\n", ThreadSelf, *pu64);
    return VINF_SUCCESS;
}

int main()
{
    int rc;
    unsigned u;
    RTTHREAD Thread[ELEMENTS(g_au64)];

    rc = RTR3Init(false, 0);
    if (RT_FAILURE(rc))
    {
        RTPrintf("RTR3Init failed (rc=%Vrc)\n", rc);
        exit(1);
    }
    rc = RTSemMutexCreate(&g_mutex);
    if (RT_FAILURE(rc))
    {
        RTPrintf("RTSemMutexCreate failed (rc=%Vrc)\n", rc);
        exit(1);
    }
    for (u=0; u<ELEMENTS(g_au64); u++)
    {
        rc = RTThreadCreate(&Thread[u], ThreadTest, &g_au64[u], 0, RTTHREADTYPE_DEFAULT, 0, "test");
        if (RT_FAILURE(rc))
        {
            RTPrintf("RTThreadCreate failed for thread %u (rc=%Vrc)\n", u, rc);
            exit(1);
        }
    }

    RTPrintf("%u Threads created. Waiting for %u seconds ...\n", ELEMENTS(g_au64), SECONDS);
    RTThreadSleep(SECONDS * 1000);
    g_fTerminate = true;
    RTThreadSleep(100);
    RTSemMutexDestroy(g_mutex);
    RTThreadSleep(100);

    for (u=1; u<ELEMENTS(g_au64); u++)
        g_au64[0] += g_au64[u];

    RTPrintf("Done. In total: %lld\n", g_au64[0]);
}
