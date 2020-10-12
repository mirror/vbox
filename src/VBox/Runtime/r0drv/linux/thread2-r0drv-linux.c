/* $Id$ */
/** @file
 * IPRT - Threads (Part 2), Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "the-linux-kernel.h"
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/thread.h>
#include <iprt/errcore.h>
#include "internal/thread.h"

#if RTLNX_VER_MIN(4,11,0)
    #include <uapi/linux/sched/types.h>
#endif /* >= KERNEL_VERSION(4, 11, 0) */

RTDECL(RTTHREAD) RTThreadSelf(void)
{
    return rtThreadGetByNative((RTNATIVETHREAD)current);
}


DECLHIDDEN(int) rtThreadNativeInit(void)
{
    return VINF_SUCCESS;
}


DECLHIDDEN(int) rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType)
{
    int                 iSchedClass = SCHED_FIFO;
    int                 rc = VINF_SUCCESS;
    struct sched_param  Param = { 0 };

    RT_NOREF_PV(pThread);
#if RTLNX_VER_MIN(5,9,0)
    RT_NOREF_PV(iSchedClass);
    RT_NOREF_PV(Param);
#endif
    switch (enmType)
    {
        case RTTHREADTYPE_IO:
#if RTLNX_VER_MAX(5,9,0)
            /* Set max. priority to preempt all other threads on this CPU. */
            Param.sched_priority = MAX_RT_PRIO - 1;
#else 
            /* Effectively changes prio to 50 */
            sched_set_fifo(current);
#endif
            break;
        case RTTHREADTYPE_TIMER:
#if RTLNX_VER_MAX(5,9,0)
            Param.sched_priority = 1; /* not 0 just in case */
#else
            /* Just one above SCHED_NORMAL class */
            sched_set_fifo_low(current);
#endif
            break;
        default:
            /* pretend success instead of VERR_NOT_SUPPORTED */
            return rc;
    }
#if RTLNX_VER_MAX(5,9,0)
    if ((sched_setscheduler(current, iSchedClass, &Param)) != 0) {
        rc = VERR_GENERAL_FAILURE;
    }
#endif
    return rc;
}


DECLHIDDEN(int) rtThreadNativeAdopt(PRTTHREADINT pThread)
{
    RT_NOREF_PV(pThread);
    return VERR_NOT_IMPLEMENTED;
}


DECLHIDDEN(void) rtThreadNativeWaitKludge(PRTTHREADINT pThread)
{
    /** @todo fix RTThreadWait/RTR0Term race on linux. */
    RTThreadSleep(1); NOREF(pThread);
}


DECLHIDDEN(void) rtThreadNativeDestroy(PRTTHREADINT pThread)
{
    NOREF(pThread);
}


#if RTLNX_VER_MIN(2,6,4)
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
#endif


DECLHIDDEN(int) rtThreadNativeCreate(PRTTHREADINT pThreadInt, PRTNATIVETHREAD pNativeThread)
{
#if RTLNX_VER_MIN(2,6,4)
    struct task_struct *NativeThread;
    IPRT_LINUX_SAVE_EFL_AC();

    RT_ASSERT_PREEMPTIBLE();

    NativeThread = kthread_run(rtThreadNativeMain, pThreadInt, "iprt-%s", pThreadInt->szName);

    if (!IS_ERR(NativeThread))
    {
        *pNativeThread = (RTNATIVETHREAD)NativeThread;
        IPRT_LINUX_RESTORE_EFL_AC();
        return VINF_SUCCESS;
    }
    IPRT_LINUX_RESTORE_EFL_AC();
    return VERR_GENERAL_FAILURE;
#else
    return VERR_NOT_IMPLEMENTED;
#endif
}

