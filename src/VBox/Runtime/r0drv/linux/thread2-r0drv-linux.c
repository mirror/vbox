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

int rtThreadNativeSetPriority(PRTTHREADINT pThread, RTTHREADTYPE enmType)
{
    int Priority = 0;
    switch (enmType)
    {
        case RTTHREADTYPE_INFREQUENT_POLLER:    Priority = 9; break;
        case RTTHREADTYPE_EMULATION:            Priority = 7; break;
        case RTTHREADTYPE_DEFAULT:              Priority = 8; break;
        case RTTHREADTYPE_MSG_PUMP:             Priority = 9; break;
        case RTTHREADTYPE_IO:                   Priority = 10; break;
        case RTTHREADTYPE_TIMER:                Priority = 11; break;

        default:
            AssertMsgFailed(("enmType=%d\n", enmType));
            return VERR_INVALID_PARAMETER;
    }
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
