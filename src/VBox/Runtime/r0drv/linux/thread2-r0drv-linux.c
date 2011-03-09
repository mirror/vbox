/* $Id$ */
/** @file
 * IPRT - Threads (Part 2), Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include "the-linux-kernel.h"
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/err.h>
#include "internal/thread.h"


RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative((RTNATIVETHREAD)current);
}

int rtThreadNativeInit(void)
{
    return VINF_SUCCESS;
}

/*
 * Following is verbatim comment from Linux's sched.h.
 *
 * Priority of a process goes from 0..MAX_PRIO-1, valid RT
 * priority is 0..MAX_RT_PRIO-1, and SCHED_NORMAL/SCHED_BATCH
 * tasks are in the range MAX_RT_PRIO..MAX_PRIO-1. Priority
 * values are inverted: lower p->prio value means higher priority.
 *
 * The MAX_USER_RT_PRIO value allows the actual maximum
 * RT priority to be separate from the value exported to
 * user-space.  This allows kernel threads to set their
 * priority to a value higher than any user task. Note:
 * MAX_RT_PRIO must not be smaller than MAX_USER_RT_PRIO.
 */

int rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType)
{
    int    sched_class = SCHED_NORMAL;
    struct sched_param param = { .sched_priority = MAX_PRIO-1 };
    switch (enmType)
    {
        case RTTHREADTYPE_INFREQUENT_POLLER:
        {
            param.sched_priority = MAX_RT_PRIO + 5;
            break;
        }
        case RTTHREADTYPE_EMULATION:
        {
            param.sched_priority = MAX_RT_PRIO + 4;
            break;
        }
        case RTTHREADTYPE_DEFAULT:
        {
            param.sched_priority = MAX_RT_PRIO + 3;
            break;
        }
        case RTTHREADTYPE_MSG_PUMP:
        {
            param.sched_priority = MAX_RT_PRIO + 2;
            break;
        }
        case RTTHREADTYPE_IO:
        {
            sched_class = SCHED_FIFO;
            param.sched_priority = MAX_RT_PRIO - 1;
            break;
        }
        case RTTHREADTYPE_TIMER:
        {
            sched_class = SCHED_FIFO;
            param.sched_priority = 1; /* not 0 just in case */
        }
        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return VERR_INVALID_PARAMETER;
    }
    sched_setscheduler(current, sched_class, &param);

    return VINF_SUCCESS;
}

int rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    return VERR_NOT_IMPLEMENTED;
}


void rtThreadNativeDestroy(PRTTHREADINT pThread)
{
    NOREF(pThread);
}

/**
 * Native kernel thread wrapper function.
 *
 * This will forward to rtThreadMain and do termination upon return.
 *
 * @param pvArg         Pointer to the argument package.
 */
static int rtThreadNativeMain(void *pvArg)
{
    PRTTHREADINT pThread = (PRTTHREADINT)pvArg;

    rtThreadMain(pThread, (RTNATIVETHREAD)current, &pThread->szName[0]);
    return 0;
}


int rtThreadNativeCreate(PRTTHREADINT pThreadInt, PRTNATIVETHREAD pNativeThread)
{
    struct task_struct *NativeThread;

    RT_ASSERT_PREEMPTIBLE();

    NativeThread = kthread_run(rtThreadNativeMain, pThreadInt, "%s", pThreadInt->szName);

    if (IS_ERR(NativeThread))
        return VERR_GENERAL_FAILURE;

    *pNativeThread = (RTNATIVETHREAD)NativeThread;
    return VINF_SUCCESS;
}
