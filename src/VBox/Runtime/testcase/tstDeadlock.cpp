/* $Id$ */
/** @file
 * IPRT Testcase - deadlock detection. Will never really "work".
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/thread.h>
#include <iprt/critsect.h>
#include <iprt/stream.h>
#include <iprt/err.h>
#include <iprt/runtime.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static RTCRITSECT g_CritSect1;
static RTCRITSECT g_CritSect2;
static RTCRITSECT g_CritSect3;

#define UNIT 250

static DECLCALLBACK(int) Thread1(RTTHREAD ThreadSelf, void *pvUser)
{
    RTCritSectEnter(&g_CritSect1);
    RTPrintf("thread1: got 1\n");
    RTThreadSleep(3*UNIT);
    RTPrintf("thread1: taking 2\n");
    RTCritSectEnter(&g_CritSect2);
    RTPrintf("thread1: got 2!!!\n");
    return VERR_DEADLOCK;
}

static DECLCALLBACK(int) Thread2(RTTHREAD ThreadSelf, void *pvUser)
{
    RTCritSectEnter(&g_CritSect2);
    RTPrintf("thread2: got 2\n");
    RTThreadSleep(1*UNIT);
    RTPrintf("thread2: taking 3\n");
    RTCritSectEnter(&g_CritSect3);
    RTPrintf("thread2: got 3!!!\n");
    return VERR_DEADLOCK;
}

static DECLCALLBACK(int) Thread3(RTTHREAD ThreadSelf, void *pvUser)
{
    RTCritSectEnter(&g_CritSect3);
    RTPrintf("thread3: got 3\n");
    RTThreadSleep(2*UNIT);
    RTPrintf("thread3: taking 1\n");
    RTCritSectEnter(&g_CritSect1);
    RTPrintf("thread1: got 1!!!\n");
    return VERR_DEADLOCK;
}


int main()
{
    /*
     * Init.
     */
    RTR3Init();
    int rc = RTCritSectInit(&g_CritSect1);
    if (RT_SUCCESS(rc))
        rc = RTCritSectInit(&g_CritSect2);
    if (RT_SUCCESS(rc))
        rc = RTCritSectInit(&g_CritSect3);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstDeadlock: failed to initialize critsects: %Rra\n", rc);
        return 1;
    }
    RTCritSectEnter(&g_CritSect1);
    if (g_CritSect1.Strict.ThreadOwner == NIL_RTTHREAD)
    {
        RTPrintf("tstDeadlock: deadlock detection is not enabled in this build\n");
        return 1;
    }
    RTCritSectLeave(&g_CritSect1);

    /*
     * Start the threads and wait for them to deadlock.
     */
    RTPrintf("tstDeadlock: TESTING...\n");
    RTThreadYield();
    rc = RTThreadCreate(NULL, Thread1, NULL, 0, RTTHREADTYPE_DEFAULT, 0, "Thread1");
    if (RT_SUCCESS(rc))
        rc = RTThreadCreate(NULL, Thread2, NULL, 0, RTTHREADTYPE_DEFAULT, 0, "Thread2");
    if (RT_SUCCESS(rc))
        rc = RTThreadCreate(NULL, Thread3, NULL, 0, RTTHREADTYPE_DEFAULT, 0, "Thread3");
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstDeadlock: failed to create threads: %Rra\n");
        return 1;
    }
    for (;;)
        RTThreadSleep(60000);

    RTPrintf("tstDeadlock: Impossible!!!\n");
    return 0;
}

