/* $Id$ */
/** @file
 * IPRT Testcase - Event Semaphore Test.
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
static RTSEMEVENTMULTI      g_hSemEM = NIL_RTSEMEVENTMULTI;
static uint32_t volatile    g_cErrors;


int PrintError(const char *pszFormat, ...)
{
    ASMAtomicIncU32(&g_cErrors);

    RTPrintf("tstSemEvent: FAILURE - ");
    va_list va;
    va_start(va, pszFormat);
    RTPrintfV(pszFormat, va);
    va_end(va);

    return 1;
}


int ThreadTest1(RTTHREAD ThreadSelf, void *pvUser)
{
    int rc;
    rc = RTSemEventMultiWait(g_hSemEM, 1000);
    if (rc != VERR_TIMEOUT)
    {
        PrintError("Thread 1: unexpected result of first RTSemEventMultiWait %Rrc\n", rc);
        return VINF_SUCCESS;
    }

    rc = RTSemEventMultiWait(g_hSemEM, 1000);
    if (RT_FAILURE(rc))
    {
        PrintError("Thread 1: unexpected result of second RTSemEventMultiWait %Rrc\n", rc);
        return VINF_SUCCESS;
    }

    RTPrintf("tstSemEvent: Thread 1 normal exit...\n");
    return VINF_SUCCESS;
}


int ThreadTest2(RTTHREAD ThreadSelf, void *pvUser)
{
    int rc;
    rc = RTSemEventMultiWait(g_hSemEM, RT_INDEFINITE_WAIT);
    if (RT_FAILURE(rc))
    {
        PrintError("Thread 2: unexpected result of RTSemEventMultiWait %Rrc\n", rc);
        return VINF_SUCCESS;
    }

    RTPrintf("tstSemEvent: Thread 2 normal exit...\n");
    return VINF_SUCCESS;
}


static int Test1()
{
    int rc;
    RTTHREAD Thread1, Thread2;

    rc = RTSemEventMultiCreate(&g_hSemEM);
    if (RT_FAILURE(rc))
        return PrintError("RTSemEventMultiCreate failed (rc=%Rrc)\n", rc);

    /*
     * Create the threads and let them block on the event multi semaphore.
     */
    rc = RTSemEventMultiReset(g_hSemEM);
    if (RT_FAILURE(rc))
        return PrintError("RTSemEventMultiReset failed (rc=%Rrc)\n", rc);

    rc = RTThreadCreate(&Thread2, ThreadTest2, NULL, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "test2");
    if (RT_FAILURE(rc))
        return PrintError("RTThreadCreate failed for thread 2 (rc=%Rrc)\n", rc);
    RTThreadSleep(100);

    rc = RTThreadCreate(&Thread1, ThreadTest1, NULL, 0, RTTHREADTYPE_DEFAULT, RTTHREADFLAGS_WAITABLE, "test1");
    if (RT_FAILURE(rc))
        return PrintError("RTThreadCreate failed for thread 1 (rc=%Rrc)\n", rc);

    /* Force first thread (which has a timeout of 1 second) to timeout in the
     * first wait, and the second wait will succeed. */
    RTThreadSleep(1500);
    rc = RTSemEventMultiSignal(g_hSemEM);
    if (RT_FAILURE(rc))
        PrintError("RTSemEventMultiSignal failed (rc=%Rrc)\n", rc);

    rc = RTThreadWait(Thread1, 1000, NULL);
    if (RT_FAILURE(rc))
        PrintError("RTThreadWait failed for thread 1 (rc=%Rrc)\n", rc);

    rc = RTThreadWait(Thread2, 1000, NULL);
    if (RT_FAILURE(rc))
        PrintError("RTThreadWait failed for thread 2 (rc=%Rrc)\n", rc);

    rc = RTSemEventMultiDestroy(g_hSemEM);
    if (RT_FAILURE(rc))
        PrintError("RTSemEventMultiDestroy failed - %Rrc\n", rc);
    g_hSemEM = NIL_RTSEMEVENTMULTI;
    if (g_cErrors)
        RTThreadSleep(100);
    return 0;
}


int main(int argc, char **argv)
{
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstSemEvent: RTR3Init failed (rc=%Rrc)\n", rc);
        return 1;
    }
    RTPrintf("tstSemEvent: TESTING...\n");
    Test1();

    if (!g_cErrors)
        RTPrintf("tstSemEvent: SUCCESS\n");
    else
        RTPrintf("tstSemEvent: FAILURE - %u errors\n", g_cErrors);
    return g_cErrors != 0;
}

