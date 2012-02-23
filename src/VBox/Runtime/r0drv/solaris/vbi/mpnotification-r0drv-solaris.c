/* $Id$ */
/** @file
 * IPRT - Multiprocessor Event Notifications, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
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
#include "../the-solaris-kernel.h"
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include "r0drv/mp-r0drv.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** CPU watch callback handle. */
static vbi_cpu_watch_t *g_hVbiCpuWatch = NULL;
/** Set of online cpus that is maintained by the MP callback.
 * This avoids locking issues querying the set from the kernel as well as
 * eliminating any uncertainty regarding the online status during the
 * callback. */
RTCPUSET g_rtMpSolarisCpuSet;


static void rtMpNotificationSolarisOnCurrentCpu(void *pvArgs, void *uIgnored1, void *uIgnored2)
{
    NOREF(uIgnored1);
    NOREF(uIgnored2);

    PRTMPARGS pArgs = (PRTMPARGS)(pvArgs);
    AssertRelease(pArgs && pArgs->idCpu == RTMpCpuId());
    Assert(pArgs->pvUser2);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    int online = *(int *)pArgs->pvUser2;
    if (online)
    {
        RTCpuSetAdd(&g_rtMpSolarisCpuSet, pArgs->idCpu);
        rtMpNotificationDoCallbacks(RTMPEVENT_ONLINE, pArgs->idCpu);
    }
    else
    {
        RTCpuSetDel(&g_rtMpSolarisCpuSet, pArgs->idCpu);
        rtMpNotificationDoCallbacks(RTMPEVENT_OFFLINE, pArgs->idCpu);
    }
}


static void rtMpNotificationSolarisCallback(void *pvUser, int iCpu, int online)
{
    vbi_preempt_disable();

    RTMPARGS Args;
    RT_ZERO(Args);
    Args.pvUser1 = pvUser;
    Args.pvUser2 = &online;
    Args.idCpu   = iCpu;

    /*
     * If we're not on the target CPU, schedule (synchronous) the event notification callback
     * to run on the target CPU i.e. the one pertaining to the MP event.
     */
    bool fRunningOnTargetCpu = iCpu == RTMpCpuId();      /* ASSUMES iCpu == RTCPUID */
    if (fRunningOnTargetCpu)
        rtMpNotificationSolarisOnCurrentCpu(&Args, NULL /* pvIgnored1 */, NULL /* pvIgnored2 */);
    else
    {
        if (online)
            vbi_execute_on_one(rtMpNotificationSolarisOnCurrentCpu, &Args, iCpu);
        else
        {
            /*
             * Since we don't absolutely need to do CPU bound code in any of the CPU offline
             * notification hooks, run it on the current CPU. Scheduling a callback to execute
             * on the CPU going offline at this point is too late and will not work reliably.
             */
            RTCpuSetDel(&g_rtMpSolarisCpuSet, iCpu);
            rtMpNotificationDoCallbacks(RTMPEVENT_OFFLINE, iCpu);
        }
    }

    vbi_preempt_enable();
}


DECLHIDDEN(int) rtR0MpNotificationNativeInit(void)
{
    if (g_hVbiCpuWatch != NULL)
        return VERR_WRONG_ORDER;

    /*
     * Register the callback building the online cpu set as we
     * do so (current_too = 1).
     */
    RTCpuSetEmpty(&g_rtMpSolarisCpuSet);
    g_hVbiCpuWatch = vbi_watch_cpus(rtMpNotificationSolarisCallback, NULL, 1 /*current_too*/);

    return VINF_SUCCESS;
}


DECLHIDDEN(void) rtR0MpNotificationNativeTerm(void)
{
    if (g_hVbiCpuWatch != NULL)
        vbi_ignore_cpus(g_hVbiCpuWatch);
    g_hVbiCpuWatch = NULL;
}

