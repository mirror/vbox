/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Threads, Ring-0 Driver, Darwin.
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
#include "the-darwin-kernel.h"
#include <iprt/thread.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include "r0drv/thread-r0drv.h"


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return (RTTHREAD)current_thread();
}


RTDECL(int)   RTThreadSleep(unsigned cMillies)
{
    uint64_t u64Deadline;
    clock_interval_to_deadline(cMillies, kMillisecondScale, &u64Deadline);
    clock_delay_until(u64Deadline);
    return VINF_SUCCESS;
}


RTDECL(bool) RTThreadYield(void)
{
    thread_block(THREAD_CONTINUE_NULL);
    return true; /* this is fishy */
}


int rtThreadNativeSetPriority(RTTHREAD Thread, RTTHREADTYPE enmType)
{
    /*
     * Convert the priority type to scheduling policies.
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
    kern_return_t rc = thread_policy_set((thread_t)Thread, THREAD_PRECEDENCE_POLICY,
                                         (thread_policy_t)&Precedence, THREAD_PRECEDENCE_POLICY_COUNT);
    AssertMsg(rc == KERN_SUCCESS, ("%rc\n", rc)); NOREF(rc);

    if (fSetExtended)
    {
        rc = thread_policy_set((thread_t)Thread, THREAD_EXTENDED_POLICY,
                               (thread_policy_t)&Extended, THREAD_EXTENDED_POLICY_COUNT);
        AssertMsg(rc == KERN_SUCCESS, ("%rc\n", rc));
    }

    if (fSetTimeContstraint)
    {
        rc = thread_policy_set((thread_t)Thread, THREAD_TIME_CONSTRAINT_POLICY,
                               (thread_policy_t)&TimeConstraint, THREAD_TIME_CONSTRAINT_POLICY_COUNT);
        AssertMsg(rc == KERN_SUCCESS, ("%rc\n", rc));
    }

    return VINF_SUCCESS; /* ignore any errors for now */
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

    rtThreadMain((RTNATIVETHREAD)Self, (PRTR0THREADARGS)pvArg);

    kern_return_t rc = thread_terminate(Self);
    AssertFatalMsgFailed(("rc=%d\n", rc));
}


int rtThreadNativeCreate(PRTR0THREADARGS pArgs, PRTNATIVETHREAD pNativeThread)
{
    thread_t NativeThread;
    kern_return_t rc = kernel_thread_start(rtThreadNativeMain, pArgs, &NativeThread);
    if (rc == KERN_SUCCESS)
    {
        *pNativeThread = (RTNATIVETHREAD)NativeThread;
        thread_deallocate(NativeThread);
        return VINF_SUCCESS;
    }
    return RTErrConvertFromMachKernReturn(rc);
}

