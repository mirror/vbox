/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Threads, l4env.
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
#define LOG_GROUP RTLOGGROUP_THREAD
#include <l4/thread/thread.h>
#include <l4/sys/ipc.h>
#include <l4/sys/syscalls.h>
#include <l4/util/rdtsc.h>
#include <l4/env/errno.h>

#include <iprt/alloc.h>
#include <iprt/thread.h>
#include <iprt/ctype.h>
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/err.h>
#include "internal/thread.h"


/*******************************************************************************
*   Global variables                                                           *
*******************************************************************************/
/** The pthread key in which we store the pointer to our own PRTTHREAD structure. */
static int g_SelfKey;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtThreadNativeMain(void *pvArgs);


int rtThreadNativeInit(void)
{
    g_SelfKey = l4thread_data_allocate_key();
    if (g_SelfKey == -L4_ENOKEY)
        return VERR_NO_TLS_FOR_SELF;
    return VINF_SUCCESS;
}


/**
 * L4 exit functions must be passed in a per-thread structure in which
 * the thread argument and a pointer to the next structure are stored.
 */
typedef struct RTL4THREADEXIT
{
    /** The thread this structure belongs to */
    PRTTHREADINT        pThread;
    /** Structure for passing the fn */
    l4thread_exit_desc  ExitFunction;
} RTL4THREADEXIT, *PRTL4THREADEXIT;


/**
 * Destructor called when an alien thread terminates.
 *
 * @param   pNativeThread The native thread which is exiting.
 * @param   pvValue       The key value. PRTTHREAD in our case.
 */
void rtL4ThreadExitFn(l4thread_t pNativeThread, void *pvValue)
{
    /*
     * Deal with alien threads.
     */
     PRTL4THREADEXIT pThreadExit = (PRTL4THREADEXIT)pvValue;
     if (pThreadExit->pThread->fIntFlags & RTTHREADINT_FLAGS_ALIEN)
         rtThreadTerminate(pThreadExit->pThread, 0);
     RTMemFree(pThreadExit);
}


/**
 * Adopts a thread, this is called immediately after allocating the
 * thread structure.
 *
 * @param   pThread     Pointer to the thread structure.
 */
int rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    PRTL4THREADEXIT pThreadExit = (PRTL4THREADEXIT)RTMemAlloc(sizeof(RTL4THREADEXIT));
    if (!pThreadExit)
        return VERR_NO_MEMORY;
    pThreadExit->ExitFunction.fn = rtL4ThreadExitFn;
    pThreadExit->ExitFunction.data = NULL;
    pThreadExit->ExitFunction.next = NULL;
    if (l4thread_on_exit(&pThreadExit->ExitFunction, pThreadExit))
        return VERR_FAILED_TO_SET_SELF_TLS;  /* Well, sort of */
    if (l4thread_data_set_current(g_SelfKey, pThreadExit))
        return VERR_FAILED_TO_SET_SELF_TLS;
    return VINF_SUCCESS;
}


/**
 * Wrapper which unpacks the runtime thread params and calls thread function.
 * @param   pvArgs      Pointer to a RTTHREADINT structure.
 */
static void rtThreadNativeMain(void *pvArgs)
{
    PRTTHREADINT  pThread = (PRTTHREADINT)pvArgs;

    /*
     * Set the TLS item and check if we're actually started.
     * (Someone try explain what l4thread_started is about, and why we
     *  don't check it before we set the key.)
     */
    int rc = l4thread_data_set_current(g_SelfKey, pThread);
    AssertReleaseMsg(!rc, ("failed to set self TLS. rc=%d thread '%s'\n",
                           rc, pThread->szName));

    l4thread_t Self = l4thread_myself();
    if (l4thread_started(NULL) != 0)
        return; /* else - error starting thread - Will l4thread_create_long fail? */
                /** @todo when you've figured out the failure path here, you might have to call rtThreadTerminate here. */

    /*
     * Call common main.
     * When we return pThread could be invalid.
     */
    rtThreadMain(pThread, Self);

    l4thread_data_set_current(g_SelfKey, NULL);
}


int rtThreadNativeCreate(PRTTHREADINT pThread, PRTNATIVETHREAD pNativeThread)
{
    /*
     * Set the default stack size.
     */
    if (!pThread->cbStack)
        pThread->cbStack = 512*_1K;

    /*
     * Change thread name to lowercase and prepend '.' to achieve thread names like 'VBoxBFE.timer'.
     */
    size_t  i;
    char    szL4ThreadName[32] = { "." };
    for (i = 0; pThread->szName[i] && i < sizeof(szL4ThreadName) - 2; i++)
        szL4ThreadName[i + 1] = tolower(pThread->szName[i]);
    szL4ThreadName[i + 1] = '\0';

    /*
     * Create the thread.
     */
    l4thread_t ThreadId = l4thread_create_long(L4THREAD_INVALID_ID,
                              rtThreadNativeMain, szL4ThreadName,
                              L4THREAD_INVALID_SP, pThread->cbStack,
                              L4THREAD_DEFAULT_PRIO, pThread,
                              L4THREAD_CREATE_SYNC);
    if (ThreadId >= 0)
    {
        *pNativeThread = ThreadId;
        Assert((l4thread_t)*pNativeThread == ThreadId);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromL4Errno(-ThreadId);
}


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    PRTTHREADINT pThread =
        (PRTTHREADINT)l4thread_data_get_current(g_SelfKey);
    /** @todo import alien threads? */
    return pThread;
}


RTDECL(RTNATIVETHREAD) RTThreadNativeSelf(void)
{
    return (RTNATIVETHREAD)l4thread_myself();
}


RTR3DECL(int)   RTThreadSleep(unsigned cMillies)
{
    LogFlow(("RTThreadSleep: cMillies=%d\n", cMillies));
    if (!cMillies)
        l4_yield();
    else
    {
         l4thread_sleep(cMillies);
         LogFlow(("RTThreadSleep: returning VINF_SUCCESS (cMillies=%d)\n", cMillies));
    }
    return VINF_SUCCESS; /* can't fail */
}


RTR3DECL(bool) RTThreadYield(void)
{
    uint64_t u64TS = l4_rdtsc();
    l4_yield();
    u64TS = l4_rdtsc() - u64TS;
    bool fRc = u64TS > 1500;
    LogFlow(("RTThreadYield: returning %d (%llu ticks)\n", fRc, u64TS));
    return fRc;
}


RTR3DECL(uint64_t) RTThreadGetAffinity(void)
{
    return 1;
}


RTR3DECL(int) RTThreadSetAffinity(uint64_t u64Mask)
{
    if (u64Mask != 1)
        return VERR_INVALID_PARAMETER;
    return VINF_SUCCESS;
}

