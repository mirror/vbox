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
/** Whether CPUs are being watched or not. */
static volatile bool g_fSolCpuWatch = false;
/** Set of online cpus that is maintained by the MP callback.
 * This avoids locking issues querying the set from the kernel as well as
 * eliminating any uncertainty regarding the online status during the
 * callback. */
RTCPUSET g_rtMpSolCpuSet;

/**
 * Internal solaris representation for watching CPUs.
 */
typedef struct RTMPSOLWATCHCPUS
{
    /** Function pointer to Mp worker. */
    PFNRTMPWORKER   pfnWorker;
    /** Argument to pass to the Mp worker. */
    void           *pvArg;
} RTMPSOLWATCHCPUS;
typedef RTMPSOLWATCHCPUS *PRTMPSOLWATCHCPUS;


/**
 * PFNRTMPWORKER worker for executing Mp events on the target CPU.
 *
 * @param    idCpu          The current CPU Id.
 * @param    pvArg          Opaque pointer to event type (online/offline).
 * @param    pvIgnored1     Ignored.
 */
static void rtMpNotificationSolOnCurrentCpu(RTCPUID idCpu, void *pvArg, void *pvIgnored1)
{
    NOREF(pvIgnored1);
    NOREF(idCpu);

    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    AssertRelease(pArgs && pArgs->idCpu == RTMpCpuId());
    Assert(pArgs->pvUser1);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    RTMPEVENT enmMpEvent = *(RTMPEVENT *)pArgs->pvUser1;
    rtMpNotificationDoCallbacks(enmMpEvent, pArgs->idCpu);
}


/**
 * Solaris callback function for Mp event notification.
 *
 * @param    CpuState   The current event/state of the CPU.
 * @param    iCpu       Which CPU is this event fore.
 * @param    pvArg      Ignored.
 *
 * @remarks This function assumes index == RTCPUID.
 * @returns Solaris error code.
 */
static int rtMpNotificationCpuEvent(cpu_setup_t CpuState, int iCpu, void *pvArg)
{
    RTMPEVENT enmMpEvent;

    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
    RTThreadPreemptDisable(&PreemptState);

    /*
     * Update our CPU set structures first regardless of whether we've been
     * scheduled on the right CPU or not, this is just atomic accounting.
     */
    if (CpuState == CPU_ON)
    {
        enmMpEvent = RTMPEVENT_ONLINE;
        RTCpuSetAdd(&g_rtMpSolCpuSet, iCpu);
    }
    else if (CpuState == CPU_OFF)
    {
        enmMpEvent = RTMPEVENT_OFFLINE;
        RTCpuSetDel(&g_rtMpSolCpuSet, iCpu);
    }
    else
        return 0;

    /*
     * Since we don't absolutely need to do CPU bound code in any of the CPU offline
     * notification hooks, run it on the current CPU. Scheduling a callback to execute
     * on the CPU going offline at this point is too late and will not work reliably.
     */
    bool fRunningOnTargetCpu = iCpu == RTMpCpuId();
    if (   fRunningOnTargetCpu == true
        || enmMpEvent == RTMPEVENT_OFFLINE)
    {
        rtMpNotificationDoCallbacks(enmMpEvent, iCpu);
    }
    else
    {
        /*
         * We're not on the target CPU, schedule (synchronous) the event notification callback
         * to run on the target CPU i.e. the CPU that was online'd.
         */
        RTMPARGS Args;
        RT_ZERO(Args);
        Args.pvUser1 = &enmMpEvent;
        Args.pvUser2 = NULL;
        Args.idCpu   = iCpu;
        RTMpOnSpecific(iCpu, rtMpNotificationSolOnCurrentCpu, &Args, NULL /* pvIgnored1 */);
    }

    RTThreadPreemptRestore(&PreemptState);

    NOREF(pvArg);
    return 0;
}


DECLHIDDEN(int) rtR0MpNotificationNativeInit(void)
{
    if (ASMAtomicReadBool(&g_fSolCpuWatch) == true)
        return VERR_WRONG_ORDER;

    /*
     * Register the callback building the online cpu set as we do so.
     */
    RTCpuSetEmpty(&g_rtMpSolCpuSet);

    mutex_enter(&cpu_lock);
    register_cpu_setup_func(rtMpNotificationCpuEvent, NULL /* pvArg */);

    for (int i = 0; i < (int)RTMpGetCount(); ++i)
        if (cpu_is_online(cpu[i]))
            rtMpNotificationCpuEvent(CPU_ON, i, NULL /* pvArg */);

    ASMAtomicWriteBool(&g_fSolCpuWatch, true);
    mutex_exit(&cpu_lock);

    return VINF_SUCCESS;
}


DECLHIDDEN(void) rtR0MpNotificationNativeTerm(void)
{
    if (ASMAtomicReadBool(&g_fSolCpuWatch) == true)
    {
        mutex_enter(&cpu_lock);
        unregister_cpu_setup_func(rtMpNotificationCpuEvent, NULL /* pvArg */);
        ASMAtomicWriteBool(&g_fSolCpuWatch, false);
        mutex_exit(&cpu_lock);
    }
}

