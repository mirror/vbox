/* $Id$ */
/** @file
 * IPRT - Timers, Ring-0 Driver, NT.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include "the-nt-kernel.h"

#include <iprt/timer.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

#include "internal-r0drv-nt.h"
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * A sub timer structure.
 *
 * This is used for keeping the per-cpu tick and DPC object.
 */
typedef struct RTTIMERNTSUBTIMER
{
    /** The tick counter. */
    uint64_t                iTick;
    /** Pointer to the parent timer. */
    PRTTIMER                pParent;
    /** The NT DPC object. */
    KDPC                    NtDpc;
} RTTIMERNTSUBTIMER;
/** Pointer to a NT sub-timer structure. */
typedef RTTIMERNTSUBTIMER *PRTTIMERNTSUBTIMER;

/**
 * The internal representation of an Linux timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed to indicate clearly that thread should exit. */
    uint32_t volatile       u32Magic;
    /** Flag indicating the the timer is suspended. */
    bool volatile           fSuspended;
    /** Whether the timer must run on one specific CPU or not. */
    bool                    fSpecificCpu;
    /** Whether the timer must run on all CPUs or not. */
    bool                    fOmniTimer;
    /** The CPU it must run on if fSpecificCpu is set.
     * The master CPU for an omni-timer. */
    RTCPUID                 idCpu;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** User argument. */
    void                   *pvUser;
    /** The timer interval. 0 if one-shot. */
    uint64_t                u64NanoInterval;
    /** The Nt timer object. */
    KTIMER                  NtTimer;
    /** The number of sub-timers. */
    RTCPUID                 cSubTimers;
    /** Sub-timers.
     * Normally there is just one, but for RTTIMER_FLAGS_CPU_ALL this will contain
     * an entry for all possible cpus. In that case the index will be the same as
     * for the RTCpuSet. */
    RTTIMERNTSUBTIMER       aSubTimers[1];
} RTTIMER;



/**
 * Timer callback function for the non-omni timers.
 *
 * @returns HRTIMER_NORESTART or HRTIMER_RESTART depending on whether it's a one-shot or interval timer.
 * @param   pHrTimer    Pointer to the timer structure.
 */
static void _stdcall rtTimerNtSimpleCallback(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PRTTIMER pTimer = (PRTTIMER)pvUser;
    AssertPtr(pTimer);
#ifdef RT_STRICT
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        AssertMsg2("rtTimerNtSimpleCallback: Irql=%d expected >=%d\n", KeGetCurrentIrql(), DISPATCH_LEVEL);
#endif

    /*
     * Check that we haven't been suspended before doing the callout.
     */
    if (    !ASMAtomicUoReadBool(&pTimer->fSuspended)
        &&  pTimer->u32Magic == RTTIMER_MAGIC)
        pTimer->pfnTimer(pTimer, pTimer->pvUser, ++pTimer->aSubTimers[0].iTick);

    NOREF(pDpc); NOREF(SystemArgument1); NOREF(SystemArgument2);
}


/**
 * The slave DPC callback for an omni timer.
 *
 * @param   pDpc                The DPC object.
 * @param   pvUser              Pointer to the sub-timer.
 * @param   SystemArgument1     Some system stuff.
 * @param   SystemArgument2     Some system stuff.
 */
static void _stdcall rtTimerNtOmniSlaveCallback(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PRTTIMERNTSUBTIMER pSubTimer = (PRTTIMERNTSUBTIMER)pvUser;
    PRTTIMER pTimer = pSubTimer->pParent;

    AssertPtr(pTimer);
#ifdef RT_STRICT
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        AssertMsg2("rtTimerNtOmniSlaveCallback: Irql=%d expected >=%d\n", KeGetCurrentIrql(), DISPATCH_LEVEL);
    int iCpuSelf = RTMpCpuIdToSetIndex(RTMpCpuId());
    if (pSubTimer - &pTimer->aSubTimers[0] != iCpuSelf)
        AssertMsg2("rtTimerNtOmniSlaveCallback: iCpuSelf=%d pSubTimer=%p / %d\n", iCpuSelf, pSubTimer, pSubTimer - &pTimer->aSubTimers[0]);
#endif

    /*
     * Check that we haven't been suspended before doing the callout.
     */
    if (    !ASMAtomicUoReadBool(&pTimer->fSuspended)
        &&  pTimer->u32Magic == RTTIMER_MAGIC)
        pTimer->pfnTimer(pTimer, pTimer->pvUser, ++pSubTimer->iTick);

    NOREF(pDpc); NOREF(SystemArgument1); NOREF(SystemArgument2);
}


/**
 * The timer callback for an omni-timer.
 *
 * This is responsible for queueing the DPCs for the other CPUs and
 * perform the callback on the CPU on which it is called.
 *
 * @param   pDpc                The DPC object.
 * @param   pvUser              Pointer to the sub-timer.
 * @param   SystemArgument1     Some system stuff.
 * @param   SystemArgument2     Some system stuff.
 */
static void _stdcall rtTimerNtOmniMasterCallback(IN PKDPC pDpc, IN PVOID pvUser, IN PVOID SystemArgument1, IN PVOID SystemArgument2)
{
    PRTTIMERNTSUBTIMER pSubTimer = (PRTTIMERNTSUBTIMER)pvUser;
    PRTTIMER pTimer = pSubTimer->pParent;
    int iCpuSelf = RTMpCpuIdToSetIndex(RTMpCpuId());

    AssertPtr(pTimer);
#ifdef RT_STRICT
    if (KeGetCurrentIrql() < DISPATCH_LEVEL)
        AssertMsg2("rtTimerNtOmniMasterCallback: Irql=%d expected >=%d\n", KeGetCurrentIrql(), DISPATCH_LEVEL);
    if (pSubTimer - &pTimer->aSubTimers[0] != iCpuSelf)
        AssertMsg2("rtTimerNtOmniMasterCallback: iCpuSelf=%d pSubTimer=%p / %d\n", iCpuSelf, pSubTimer, pSubTimer - &pTimer->aSubTimers[0]);
#endif

    /*
     * Check that we haven't been suspended before scheduling the other DPCs
     * and doing the callout.
     */
    if (    !ASMAtomicUoReadBool(&pTimer->fSuspended)
        &&  pTimer->u32Magic == RTTIMER_MAGIC)
    {
        RTCPUSET    OnlineSet;
        RTMpGetOnlineSet(&OnlineSet);
        for (int iCpu = 0; iCpu < RTCPUSET_MAX_CPUS; iCpu++)
            if (    RTCpuSetIsMemberByIndex(&OnlineSet, iCpu)
                &&  iCpuSelf != iCpu)
                KeInsertQueueDpc(&pTimer->aSubTimers[iCpu].NtDpc, 0, 0);

        pTimer->pfnTimer(pTimer, pTimer->pvUser, ++pSubTimer->iTick);
    }

    NOREF(pDpc); NOREF(SystemArgument1); NOREF(SystemArgument2);
}



RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    if (!ASMAtomicUoReadBool(&pTimer->fSuspended))
        return VERR_TIMER_ACTIVE;

    /*
     * Start the timer.
     */
    PKDPC pMasterDpc = pTimer->fOmniTimer
                     ? &pTimer->aSubTimers[RTMpCpuIdToSetIndex(pTimer->idCpu)].NtDpc
                     : &pTimer->aSubTimers[0].NtDpc;

    uint64_t u64Interval = pTimer->u64NanoInterval / 100000; /* This is ms, believe it or not. */
    ULONG ulInterval = (ULONG)u64Interval;
    if (ulInterval != u64Interval)
        ulInterval = MAXLONG;
    else if (!ulInterval && pTimer->u64NanoInterval)
        ulInterval = 1;

    LARGE_INTEGER DueTime;
    DueTime.QuadPart = -(int64_t)(u64First / 100); /* Relative, NT time. */
    if (DueTime.QuadPart)
        DueTime.QuadPart = -1;

    ASMAtomicWriteBool(&pTimer->fSuspended, false);
    KeSetTimerEx(&pTimer->NtTimer, DueTime, ulInterval, pMasterDpc);
    return VINF_SUCCESS;
}


/**
 * Worker function that stops an active timer.
 *
 * Shared by RTTimerStop and RTTimerDestroy.
 *
 * @param   pTimer      The active timer.
 */
static void rtTimerNtStopWorker(PRTTIMER pTimer)
{
    /*
     * Just cancel the timer, dequeue the DPCs and flush them (if this is supported).
     */
    ASMAtomicWriteBool(&pTimer->fSuspended, true);
    KeCancelTimer(&pTimer->NtTimer);

    for (RTCPUID iCpu = 0; iCpu < pTimer->cSubTimers; iCpu++)
        KeRemoveQueueDpc(&pTimer->aSubTimers[iCpu].NtDpc);

    /*
     * I'm a bit uncertain whether this should be done during RTTimerStop
     * or only in RTTimerDestroy()... Linux and Solaris will wait AFAIK,
     * which is why I'm keeping this here for now.
     */
    if (g_pfnrtNtKeFlushQueuedDpcs)
        g_pfnrtNtKeFlushQueuedDpcs();
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    if (ASMAtomicUoReadBool(&pTimer->fSuspended))
        return VERR_TIMER_SUSPENDED;

    /*
     * Call the worker we share with RTTimerDestroy.
     */
    rtTimerNtStopWorker(pTimer);
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerDestroy(PRTTIMER pTimer)
{
    /* It's ok to pass NULL pointer. */
    if (pTimer == /*NIL_RTTIMER*/ NULL)
        return VINF_SUCCESS;
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Invalidate the timer, stop it if it's running and finally                   .
     * free up the memory.
     */
    ASMAtomicWriteU32(&pTimer->u32Magic, ~RTTIMER_MAGIC);
    if (!ASMAtomicUoReadBool(&pTimer->fSuspended))
        rtTimerNtStopWorker(pTimer);
    RTMemFree(pTimer);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    *ppTimer = NULL;

    /*
     * Validate flags.
     */
    if (!RTTIMER_FLAGS_ARE_VALID(fFlags))
        return VERR_INVALID_PARAMETER;
    if (    (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC)
        &&  (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL
        &&  !RTMpIsCpuOnline(fFlags & RTTIMER_FLAGS_CPU_MASK))
        return (fFlags & RTTIMER_FLAGS_CPU_MASK) > RTMpGetMaxCpuId()
             ? VERR_CPU_NOT_FOUND
             : VERR_CPU_OFFLINE;

    /*
     * Allocate the timer handler.
     */
    RTCPUID cSubTimers = 1;
    if ((fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL)
    {
        cSubTimers = RTMpGetMaxCpuId() + 1;
        Assert(cSubTimers <= RTCPUSET_MAX_CPUS); /* On Windows we have a 1:1 relationship between cpuid and set index. */
    }

    PRTTIMER pTimer = (PRTTIMER)RTMemAllocZ(RT_OFFSETOF(RTTIMER, aSubTimers[cSubTimers]));
    if (!pTimer)
        return VERR_NO_MEMORY;

    /*
     * Initialize it.
     */
    pTimer->u32Magic = RTTIMER_MAGIC;
    pTimer->fSuspended = true;
    pTimer->fSpecificCpu = (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC) && (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL;
    pTimer->fOmniTimer = (fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL;
    pTimer->idCpu = fFlags & RTTIMER_FLAGS_CPU_MASK;
    pTimer->cSubTimers = cSubTimers;
    pTimer->pfnTimer = pfnTimer;
    pTimer->pvUser = pvUser;
    pTimer->u64NanoInterval = u64NanoInterval;
    KeInitializeTimerEx(&pTimer->NtTimer, SynchronizationTimer);
    if (pTimer->fOmniTimer)
    {
        /*
         * Initialize the per-cpu "sub-timers", select the first online cpu
         * to be the master.
         * ASSUMES that no cpus will ever go offline.
         */
        pTimer->idCpu = NIL_RTCPUID; /* */
        for (unsigned iCpu = 0; iCpu < cSubTimers; iCpu++)
        {
            pTimer->aSubTimers[iCpu].iTick = 0;
            pTimer->aSubTimers[iCpu].pParent = pTimer;

            if (    pTimer->idCpu == NIL_RTCPUID
                &&  RTMpIsCpuOnline(RTMpCpuIdFromSetIndex(iCpu)))
            {
                pTimer->idCpu = RTMpCpuIdFromSetIndex(iCpu);
                KeInitializeDpc(&pTimer->aSubTimers[iCpu].NtDpc, rtTimerNtOmniMasterCallback, &pTimer->aSubTimers[iCpu]);
            }
            else
                KeInitializeDpc(&pTimer->aSubTimers[iCpu].NtDpc, rtTimerNtOmniSlaveCallback, &pTimer->aSubTimers[iCpu]);
            KeSetImportanceDpc(&pTimer->aSubTimers[iCpu].NtDpc, HighImportance);
            KeSetTargetProcessorDpc(&pTimer->aSubTimers[iCpu].NtDpc, (int)RTMpCpuIdFromSetIndex(iCpu));
        }
        Assert(pTimer->idCpu != NIL_RTCPUID);
    }
    else
    {
        /*
         * Initialize the first "sub-timer", target the DPC on a specific processor
         * if requested to do so.
         */
        pTimer->aSubTimers[0].iTick = 0;
        pTimer->aSubTimers[0].pParent = pTimer;

        KeInitializeDpc(&pTimer->aSubTimers[0].NtDpc, rtTimerNtSimpleCallback, pTimer);
        KeSetImportanceDpc(&pTimer->aSubTimers[0].NtDpc, HighImportance);
        if (pTimer->fSpecificCpu)
            KeSetTargetProcessorDpc(&pTimer->aSubTimers[0].NtDpc, (int)pTimer->idCpu);
    }

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


RTDECL(uint32_t) RTTimerGetSystemGranularity(void)
{
    /*
     * Get the default/max timer increment value, return it if ExtSetTimerResolution
     * isn't available. Accoring to the sysinternals guys NtQueryTimerResolution
     * is only available in userland and they find it equally annoying.
     */
    ULONG ulTimeInc = KeQueryTimeIncrement();
    if (!g_pfnrtNtExSetTimerResolution)
        return ulTimeInc * 100; /* The value is in 100ns, the funny NT unit. */

    /*
     * Use the value returned by ExSetTimerResolution. Since the kernel is keeping
     * count of these calls, we have to do two calls that cancel each other out.
     */
    ULONG ulResolution1 = g_pfnrtNtExSetTimerResolution(ulTimeInc, TRUE);
    ULONG ulResolution2 = g_pfnrtNtExSetTimerResolution(0 /*ignored*/, FALSE);
    AssertMsg(ulResolution1 == ulResolution2, ("%ld, %ld\n", ulResolution1, ulResolution2)); /* not supposed to change it! */
    return ulResolution2 * 100; /* NT -> ns */
}


RTDECL(int) RTTimerRequestSystemGranularity(uint32_t u32Request, uint32_t *pu32Granted)
{
    if (!g_pfnrtNtExSetTimerResolution)
        return VERR_NOT_SUPPORTED;

    ULONG ulGranted = g_pfnrtNtExSetTimerResolution(u32Request / 100, TRUE);
    if (pu32Granted)
        *pu32Granted = ulGranted;
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerReleaseSystemGranularity(uint32_t u32Granted)
{
    if (!g_pfnrtNtExSetTimerResolution)
        return VERR_NOT_SUPPORTED;

    g_pfnrtNtExSetTimerResolution(0 /* ignored */, FALSE);
    NOREF(u32Granted);
    return VINF_SUCCESS;
}


