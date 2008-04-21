/* $Id$ */
/** @file
 * IPRT - Threads (Part 2), Ring-0 Driver, Darwin.
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
#include "the-darwin-kernel.h"
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include "internal/thread.h"


int rtThreadNativeInit(void)
{
    /* No TLS in Ring-0. :-/ */
    return VINF_SUCCESS;
}


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative((RTNATIVETHREAD)current_thread());
}


int rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType)
{
    /*
     * Convert the priority type to scheduling policies.
     * (This is really just guess work.)
     */
    bool                            fSetExtended = false;
    thread_extended_policy          Extended = { true };
    bool                            fSetTimeContstraint = false;
    thread_time_constraint_policy   TimeConstraint = { 0, 0, 0, true };
    thread_precedence_policy        Precedence = { 0 };
    switch (enmType)
    {
        case RTTHREADTYPE_INFREQUENT_POLLER:
            Precedence.importance = 1;
            break;

        case RTTHREADTYPE_EMULATION:
            Precedence.importance = 30;
            break;

        case RTTHREADTYPE_DEFAULT:
            Precedence.importance = 31;
            break;

        case RTTHREADTYPE_MSG_PUMP:
            Precedence.importance = 34;
            break;

        case RTTHREADTYPE_IO:
            Precedence.importance = 98;
            break;

        case RTTHREADTYPE_TIMER:
            Precedence.importance = 0x7fffffff;

            fSetExtended = true;
            Extended.timeshare = FALSE;

            fSetTimeContstraint = true;
            TimeConstraint.period = 0; /* not really true for a real timer thread, but we've really no idea. */
            TimeConstraint.computation = rtDarwinAbsTimeFromNano(100000); /* 100 us*/
            TimeConstraint.constraint = rtDarwinAbsTimeFromNano(500000);  /* 500 us */
            TimeConstraint.preemptible = FALSE;
            break;

        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return VERR_INVALID_PARAMETER;
    }

    /*
     * Do the actual modification.
     */
    kern_return_t kr = thread_policy_set((thread_t)pThread->Core.Key, THREAD_PRECEDENCE_POLICY,
                                         (thread_policy_t)&Precedence, THREAD_PRECEDENCE_POLICY_COUNT);
    AssertMsg(kr == KERN_SUCCESS, ("%rc\n", kr)); NOREF(kr);

    if (fSetExtended)
    {
        kr = thread_policy_set((thread_t)pThread->Core.Key, THREAD_EXTENDED_POLICY,
                               (thread_policy_t)&Extended, THREAD_EXTENDED_POLICY_COUNT);
        AssertMsg(kr == KERN_SUCCESS, ("%rc\n", kr));
    }

    if (fSetTimeContstraint)
    {
        kr = thread_policy_set((thread_t)pThread->Core.Key, THREAD_TIME_CONSTRAINT_POLICY,
                               (thread_policy_t)&TimeConstraint, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
        AssertMsg(kr == KERN_SUCCESS, ("%rc\n", kr));
    }

    return VINF_SUCCESS; /* ignore any errors for now */
}


int rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Native kernel thread wrapper function.
 *
 * This will forward to rtThreadMain and do termination upon return.
 *
 * @param pvArg         Pointer to the argument package.
 * @param Ignored       Wait result, which we ignore.
 */
static void rtThreadNativeMain(void *pvArg, wait_result_t Ignored)
{
    const thread_t Self = current_thread();
    PRTTHREADINT pThread = (PRTTHREADINT)pvArg;

    rtThreadMain(pThread, (RTNATIVETHREAD)Self, &pThread->szName[0]);

    kern_return_t kr = thread_terminate(Self);
    AssertFatalMsgFailed(("kr=%d\n", kr));
}


int rtThreadNativeCreate(PRTTHREADINT pThreadInt, PRTNATIVETHREAD pNativeThread)
{
    thread_t NativeThread;
    kern_return_t kr = kernel_thread_start(rtThreadNativeMain, pThreadInt, &NativeThread);
    if (kr == KERN_SUCCESS)
    {
        *pNativeThread = (RTNATIVETHREAD)NativeThread;
        thread_deallocate(NativeThread);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromMachKernReturn(kr);
}

