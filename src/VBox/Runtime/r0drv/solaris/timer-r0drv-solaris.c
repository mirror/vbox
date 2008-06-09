/** $Id$ */
/** @file
 * IPRT - Timer, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/timer.h>
#include <iprt/time.h>
#include <iprt/mp.h>
#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * This is used to track sub-timer data.
 */
typedef struct RTTIMERSOLSUBTIMER
{
    /** The current timer tick. */
    uint64_t                iTick;
    /** Pointer to the parent timer. */
    PRTTIMER                pParent;
} RTTIMERSOLSUBTIMER;
/** Pointer to a Solaris sub-timer. */
typedef RTTIMERSOLSUBTIMER *PRTTIMERSOLSUBTIMER;

/**
 * The internal representation of a Solaris timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed to indicate clearly that thread should exit. */
    uint32_t volatile       u32Magic;
    /** The cyclic timer id.
     * This is CYCLIC_NONE if the timer hasn't been started. */
    cyclic_id_t volatile    CyclicId;
    /** Flag used by rtTimerSolarisOmniOnlineCallback to see whether we're inside the cyclic_add_omni call or not. */
    bool volatile           fStarting;
    /** Whether the timer must run on a specific CPU or not. */
    bool                    fSpecificCpu;
    /** Set if we're using an omni cyclic. */
    bool                    fOmni;
    /** The CPU it must run on if fSpecificCpu is set. */
    RTCPUID                 idCpu;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** User argument. */
    void                   *pvUser;
    /** The timer interval. 0 for one-shot timer */
    uint64_t                u64NanoInterval;
    /** The timer spec (for omni timers mostly). */
    cyc_time_t              TimeSpecs;
    /** The number of sub-timers. */
    RTCPUID                 cSubTimers;
    /** Sub-timer data.
     * When fOmni is set, this will be an array indexed by CPU id.
     * When fOmni is clear, the array will only have one member. */
    RTTIMERSOLSUBTIMER      aSubTimers[1];
} RTTIMER;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtTimerSolarisCallback(void *pvTimer);
static void rtTimerSolarisOmniCallback(void *pvSubTimer);
static void rtTimerSolarisOmniDummyCallback(void *pvIgnored);
static void rtTimerSolarisOmniOnlineCallback(void *pvTimer, cpu_t *pCpu, cyc_handler_t *pCyclicInfo, cyc_time_t *pTimeSpecs);
static void rtTimerSolarisOmniOfflineCallback(void *pvTimer, cpu_t *pCpu, void *pvTick);
static bool rtTimerSolarisStop(PRTTIMER pTimer);


AssertCompileSize(cyclic_id_t, sizeof(void *));

/** Atomic read of RTTIMER::CyclicId. */
DECLINLINE(cyclic_id_t) rtTimerSolarisGetCyclicId(PRTTIMER pTimer)
{
    return (cyclic_id_t)ASMAtomicUoReadPtr((void * volatile *)&pTimer->CyclicId);
}


/** Atomic write of RTTIMER::CyclicId. */
DECLINLINE(cyclic_id_t) rtTimerSolarisSetCyclicId(PRTTIMER pTimer, cyclic_id_t CyclicIdNew)
{
    ASMAtomicWritePtr((void * volatile *)&pTimer->CyclicId, (void *)CyclicIdNew);
}


/** Atomic compare and exchange of RTTIMER::CyclicId. */
DECLINLINE(bool) rtTimerSolarisCmpXchgCyclicId(PRTTIMER pTimer, cyclic_id_t CyclicIdNew, cyclic_id_t CyclicIdOld)
{
    return ASMAtomicCmpXchgPtr((void * volatile *)&pTimer->CyclicId, (void *)CyclicIdNew, (void *)CyclicIdOld);
}


RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    RTCPUID i;
    *ppTimer = NULL;

    /*
     * Validate flags.
     */
    if (!RTTIMER_FLAGS_ARE_VALID(fFlags))
        return VERR_INVALID_PARAMETER;
    if (    (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC)
        &&  (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL
        &&  !RTMpIsCpuPossible((fFlags & RTTIMER_FLAGS_CPU_MASK)))
        return VERR_CPU_NOT_FOUND;

    /*
     * Allocate and initialize the timer handle.
     */
    size_t cCpus = (fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL
                 ? RTMpGetMaxCpuId() + 1 /* ASSUMES small max value, no pointers. */
                 : 1;
    PRTTIMER pTimer = (PRTTIMER)RTMemAllocZ(RT_OFFSETOF(RTTIMER, aSubTimers[cCpus]));
    if (!pTimer)
        return VERR_NO_MEMORY;

    pTimer->u32Magic = RTTIMER_MAGIC;
    pTimer->CyclicId = CYCLIC_NONE;
    pTimer->fStarting = false;
    if (    (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC)
        &&  (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL)
    {
        pTimer->fSpecificCpu = true;
        pTimer->fOmni = false;
        pTimer->idCpu = fFlags & RTTIMER_FLAGS_CPU_MASK;
    }
    else
    {
        pTimer->fSpecificCpu = false;
        pTimer->fOmni = (fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL;
        pTimer->idCpu = NIL_RTCPUID;
    }
    pTimer->pfnTimer = pfnTimer;
    pTimer->pvUser = pvUser;
    pTimer->u64NanoInterval = u64NanoInterval;
    pTimer->cSubTimers = cCpus;

    for (i = 0; i < cCpus; i++)
    {
        pTimer->aSubTimers[i].iTick = 0;
        pTimer->aSubTimers[i].pParent = pTimer;
    }

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerDestroy(PRTTIMER pTimer)
{
    /*
     * Validate.
     */
    if (pTimer == NULL)
        return VINF_SUCCESS;
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Invalid the timer, stop it, and free the associated resources.
     */
    ASMAtomicWriteU32(&pTimer->u32Magic, ~RTTIMER_MAGIC);
    rtTimerSolarisStop(pTimer);
    RTMemFree(pTimer);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    RTCPUID             i;
    cyclic_id_t         CyclicId;
    cyc_handler_t       CyclicInfo;
    cyc_omni_handler_t  CyclicOmniInfo;
    int                 rc = VINF_SUCCESS;

    /*
     * Validate.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);
    if (rtTimerSolarisGetCyclicId(pTimer) != CYCLIC_NONE)
    {
        /*
         * If it's a one-shot we might end up here because it didn't stop after
         * the first firing. There are two reasons for this depending on the
         * kind type of timer. (1) Non-omni timers are (potentially) racing our
         * RTTimerStart in setting RTTIMER::CyclicId. (2) Omni timers are stopped
         * on the 2nd firing because we have to make sure all cpus gets called, and
         * we're using the 2nd round that comes 1 sec after the first because this
         * is the easier way out.
         */
        if (pTimer->u64NanoInterval)
            return VERR_TIMER_ACTIVE;

        for (i = 0; i < pTimer->cSubTimers; i++)
            if (pTimer->aSubTimers[i].iTick)
                break; /* has fired */
        if (i >= pTimer->cSubTimers)
            return VERR_TIMER_ACTIVE;

        rtTimerSolarisStop(pTimer);
    }

    /*
     * Do the setup bits that doesn't need the lock.
     * We'll setup both omni and non-omni stuff here because it shorter than if'ing it.
     */
    CyclicInfo.cyh_func  = rtTimerSolarisCallback;
    CyclicInfo.cyh_arg   = pTimer;
    CyclicInfo.cyh_level = CY_LOCK_LEVEL;

    CyclicOmniInfo.cyo_online  = rtTimerSolarisOmniOnlineCallback;
    CyclicOmniInfo.cyo_offline = rtTimerSolarisOmniOfflineCallback;
    CyclicOmniInfo.cyo_arg     = pTimer;

    for (i = 0; i > pTimer->cSubTimers; i++)
        pTimer->aSubTimers[i].iTick = 0;

    if (pTimer->fSpecificCpu && u64First < 10000)
        u64First = RTTimeNanoTS() + 10000;  /* Try make sure it doesn't fire before we re-bind it. */
    else
        u64First += RTTimeNanoTS(); /* ASSUMES it is implemented via gethrtime() */

    pTimer->TimeSpecs.cyt_when = u64First;
    pTimer->TimeSpecs.cyt_interval = !pTimer->u64NanoInterval
                                   ? 1000000000 /* 1 sec */
                                   : pTimer->u64NanoInterval;

    /*
     * Acquire the cpu lock and call cyclic_add/cyclic_add_omni.
     */
    mutex_enter(&cpu_lock);

    ASMAtomicWriteBool(&pTimer->fStarting, true);
    if (pTimer->fOmni)
        CyclicId = cyclic_add_omni(&CyclicOmniInfo);
    else if (pTimer->fSpecificCpu)
    {
        cpu_t *pCpu = cpu_get(pTimer->idCpu);
        CyclicId = CYCLIC_NONE;
        if (pCpu)
        {
            if (cpu_is_online(pCpu))
            {
                CyclicId = cyclic_add(&CyclicInfo, &pTimer->TimeSpecs);
                if (CyclicId != CYCLIC_NONE)
                    cyclic_bind(CyclicId, pCpu, NULL);
            }
            else
                rc = VERR_CPU_OFFLINE;
        }
        else
            rc = VERR_CPU_NOT_FOUND;
    }
    else
        CyclicId = cyclic_add(&CyclicInfo, &pTimer->TimeSpecs);

    rtTimerSolarisSetCyclicId(pTimer, CyclicId);
    ASMAtomicWriteBool(&pTimer->fStarting, false);

    mutex_exit(&cpu_lock);

    /*
     * Just some sanity checks should the cylic code start returning errors.
     */
    Assert(RT_SUCCESS(rc) || CyclicId == CYCLIC_NONE);
    if (CyclicId == CYCLIC_NONE && rc == VINF_SUCCESS)
        rc = VERR_GENERAL_FAILURE;
    return rc;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    /*
     * Validate.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Stop the timer.
     */
    if (!rtTimerSolarisStop(pTimer))
        return VERR_TIMER_SUSPENDED;
    return VINF_SUCCESS;
}


/**
 * Timer callback function for non-omni timers.
 *
 * @param   pvTimer     Pointer to the timer.
 */
static void rtTimerSolarisCallback(void *pvTimer)
{
    PRTTIMER pTimer = (PRTTIMER)pvTimer;

    /* Check for destruction. */
    if (pTimer->u32Magic != RTTIMER_MAGIC)
        return;

    /*
     * If this is a one shot timer, suspend the timer here as Solaris
     * does not support single-shot timers implicitly.
     */
    if (!pTimer->u64NanoInterval)
    {
        rtTimerSolarisStop(pTimer);
        if (!pTimer->aSubTimers[0].iTick)
        {
            ASMAtomicWriteU64(&pTimer->aSubTimers[0].iTick, 1); /* paranoia */
            pTimer->pfnTimer(pTimer, pTimer->pvUser, 1);
        }
    }
    else
        /* recurring */
        pTimer->pfnTimer(pTimer, pTimer->pvUser, ++pTimer->aSubTimers[0].iTick);
}


/**
 * Timer callback function for omni timers.
 *
 * @param   pvTimer     Pointer to the sub-timer.
 */
static void rtTimerSolarisOmniCallback(void *pvSubTimer)
{
    PRTTIMERSOLSUBTIMER pSubTimer = (PRTTIMERSOLSUBTIMER)pvSubTimer;
    PRTTIMER pTimer = pSubTimer->pParent;

    /* Check for destruction. */
    if (    !VALID_PTR(pTimer)
        ||  pTimer->u32Magic != RTTIMER_MAGIC)
        return;

    /*
     * If this is a one-shot timer, suspend it here the 2nd time around.
     * We cannot do it the first time like for the non-omni timers since
     * we don't know if it has fired on all the cpus yet.
     */
    if (!pTimer->u64NanoInterval)
    {
        if (!pSubTimer->iTick)
        {
            ASMAtomicWriteU64(&pSubTimer->iTick, 1); /* paranoia */
            pTimer->pfnTimer(pTimer, pTimer->pvUser, 1);
        }
        else
            rtTimerSolarisStop(pTimer);
    }
    else
        /* recurring */
        pTimer->pfnTimer(pTimer, pTimer->pvUser, ++pSubTimer->iTick);
}


/**
 * This is a dummy callback use for the cases where we get cpus which id we
 * cannot handle because of broken RTMpGetMaxCpuId(), or if we're racing
 * RTTimerDestroy().
 *
 * This shouldn't happen of course, but if it does we wish to handle
 * gracefully instead of crashing.
 *
 * @param   pvIgnored   Ignored
 */
static void rtTimerSolarisOmniDummyCallback(void *pvIgnored)
{
    NOREF(pvIgnored);
}


/**
 * Omni-timer callback that sets up the timer for a cpu during cyclic_add_omni
 * or at later when a CPU comes online.
 *
 *
 * @param   pvTimer         Pointer to the timer.
 * @param   pCpu            The cpu that has come online.
 * @param   pCyclicInfo     Where to store the cyclic handler info.
 * @param   pTimeSpecs      Where to store the timer firing specs.
 */
static void rtTimerSolarisOmniOnlineCallback(void *pvTimer, cpu_t *pCpu, cyc_handler_t *pCyclicInfo, cyc_time_t *pTimeSpecs)
{
    PRTTIMER pTimer = (PRTTIMER)pvTimer;
    RTCPUID idCpu = pCpu->cpu_id;
    AssertMsg(idCpu < pTimer->cSubTimers, ("%d < %d\n", (int)idCpu, (int)pTimer->cSubTimers));
    if (    idCpu < pTimer->cSubTimers
        &&  pTimer->u32Magic == RTTIMER_MAGIC)
    {
        PRTTIMERSOLSUBTIMER pSubTimer = &pTimer->aSubTimers[idCpu];

        pCyclicInfo->cyh_func  = rtTimerSolarisOmniCallback;
        pCyclicInfo->cyh_arg   = pSubTimer;
        pCyclicInfo->cyh_level = CY_LOCK_LEVEL;

        if (pTimer->fStarting)
        {
            /*
             * Called during cyclic_add_omni, just spread the 2nd tick
             * for the one-shots to avoid unnecessary lock contention.
             */
            *pTimeSpecs = pTimer->TimeSpecs;
            if (!pTimer->u64NanoInterval)
                pTimeSpecs->cyt_interval += idCpu * (unsigned)nsec_per_tick * 2U;
        }
        else
        {
            /*
             * Called at run-time, have to make sure cyt_when isn't in the past.
             */
            ASMAtomicWriteU64(&pSubTimer->iTick, 0); /* paranoia */

            uint64_t u64Now = RTTimeNanoTS(); /* ASSUMES it's implemented using gethrtime(). */
            if (pTimer->TimeSpecs.cyt_when > u64Now)
                *pTimeSpecs = pTimer->TimeSpecs;
            else
            {
                if (!pTimer->u64NanoInterval)
                {
                    /* one-shot: Just schedule a 1 sec timeout and set the tick to 1. */
                    pTimeSpecs->cyt_when     = u64Now + 1000000000;
                    pTimeSpecs->cyt_interval = 1000000000;
                    ASMAtomicWriteU64(&pSubTimer->iTick, 1);
                }
                else
                {
#if 1 /* This might be made into a RTTIMER_FLAGS_something later, for now ASAP is what we need. */
                    /* recurring: ASAP. */
                    pTimeSpecs->cyt_when = u64Now;
#else
                    /* recurring: Adjust it to the next tick. */
                    uint64_t cTicks = (u64Now - pTimer->TimeSpecs.cyt_when) / pTimer->TimeSpecs.cyt_interval;
                    pTimeSpecs->cyt_when = (cTicks + 1) * pTimer->TimeSpecs.cyt_interval;
#endif
                    pTimeSpecs->cyt_interval = pTimer->TimeSpecs.cyt_interval;
                }
            }
        }
    }
    else
    {
        /*
         * Invalid cpu id or destruction race.
         */
        pCyclicInfo->cyh_func  = rtTimerSolarisOmniDummyCallback;
        pCyclicInfo->cyh_arg   = NULL;
        pCyclicInfo->cyh_level = CY_LOCK_LEVEL;

        pTimeSpecs->cyt_when = RTTimeNanoTS() + 1000000000;
        pTimeSpecs->cyt_interval = 1000000000;
    }
}


/**
 * Callback for when a CPU goes offline.
 *
 * Currently, we don't need to perform any tasks here.
 *
 * @param   pvTimer         Pointer to the timer.
 * @param   pCpu            Pointer to the cpu.
 * @param   pvSubTimer      Pointer to the sub timer. This may be NULL.
 */
static void rtTimerSolarisOmniOfflineCallback(void *pvTimer, cpu_t *pCpu, void *pvSubTimer)
{
    /*PRTTIMER pTimer = (PRTTIMER)pvTimer;*/
    NOREF(pvTimer);
    NOREF(pCpu);
    NOREF(pvSubTimer);
}


/**
 * Worker function used to stop the timer.
 *
 * This is used from within the callback functions (one-shot scenarious) and
 * from RTTimerStop, RTTimerDestroy and RTTimerStart. We use atomic cmpxchg
 * here to avoid some unnecessary cpu_lock contention and to avoid
 * potential (?) deadlocks between the callback and the APIs. There is a
 * slight chance of a race between active callbacks and the APIs, but this
 * is preferred to a
 *
 * @returns true if we stopped it, false if it was already stopped.
 * @param   pTimer      The timer to stop.
 */
static bool rtTimerSolarisStop(PRTTIMER pTimer)
{
    /*
     * This is a bit problematic. I'm a bit unsure whether cyclic_remove might
     * or may not deadlock with a callback trying to aquire the cpu_lock. So,
     * in order to avoid this issue I'm making sure that we don't take the lock
     * unless we know we're gonna call cyclic_remove. However, the downside of
     * this is that it's possible races between RTTimerStart/RTTimerDestroy and
     * currently active callbacks, which may cause iTick to have a bad value or
     * in the worst case, memory to accessed after cleanup.
     */
    cyclic_id_t CyclicId = rtTimerSolarisGetCyclicId(pTimer);
    if (    CyclicId != CYCLIC_NONE
        &&  rtTimerSolarisCmpXchgCyclicId(pTimer, CYCLIC_NONE, CyclicId))
    {
        mutex_enter(&cpu_lock);
        cyclic_remove(CyclicId);
        mutex_exit(&cpu_lock);
        return true;
    }
    return false;
}


RTDECL(uint32_t) RTTimerGetSystemGranularity(void)
{
    return nsec_per_tick;
}


RTDECL(int) RTTimerRequestSystemGranularity(uint32_t u32Request, uint32_t *pu32Granted)
{
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTTimerReleaseSystemGranularity(uint32_t u32Granted)
{
    return VERR_NOT_SUPPORTED;
}

