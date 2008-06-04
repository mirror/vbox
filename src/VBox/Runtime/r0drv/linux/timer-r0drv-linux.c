/* $Id$ */
/** @file
 * IPRT - Timers, Ring-0 Driver, Linux.
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
#include "the-linux-kernel.h"

#include <iprt/timer.h>
#include <iprt/time.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>

#include "internal/magics.h"

#if !defined(RT_USE_LINUX_HRTIMER) && LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 23) /* ?? */
# define RT_USE_LINUX_HRTIMER
#endif

/* This check must match the ktime usage in rtTimeGetSystemNanoTS() / time-r0drv-linux.c. */
#if defined(RT_USE_LINUX_HRTIMER) \
 && LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 16)
# error "RT_USE_LINUX_HRTIMER requires 2.6.16 or later, sorry."
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Timer state machine.
 *
 * This is used to try handle the issues with MP events and
 * timers that runs on all CPUs. It's relatively nasty :-/
 */
typedef enum RTTIMERLNXSTATE
{
    /** Stopped. */
    RTTIMERLNXSTATE_STOPPED = 0,
    /** Transient state; next ACTIVE. */
    RTTIMERLNXSTATE_STARTING,
    /** Transient state; next ACTIVE. (not really necessary) */
    RTTIMERLNXSTATE_MP_STARTING,
    /** Active. */
    RTTIMERLNXSTATE_ACTIVE,
    /** Transient state; next STOPPED. */
    RTTIMERLNXSTATE_STOPPING,
    /** Transient state; next STOPPED. */
    RTTIMERLNXSTATE_MP_STOPPING,
    /** The usual 32-bit hack. */
    RTTIMERLNXSTATE_32BIT_HACK = 0x7fffffff
} RTTIMERLNXSTATE;


/**
 * A Linux sub-timer.
 */
typedef struct RTTIMERLNXSUBTIMER
{
    /** The linux timer structure. */
#ifdef RT_USE_LINUX_HRTIMER
    struct hrtimer          LnxTimer;
#else
    struct timer_list       LnxTimer;
#endif
    /** The start of the current run (ns).
     * This is used to calculate when the timer ought to fire the next time. */
    uint64_t                u64StartTS;
    /** The start of the current run (ns).
     * This is used to calculate when the timer ought to fire the next time. */
    uint64_t                u64NextTS;
    /** The current tick number (since u64StartTS). */
    uint64_t                iTick;
    /** Pointer to the parent timer. */
    PRTTIMER                pParent;
    /** The current sub-timer state. */
    RTTIMERLNXSTATE volatile enmState;
} RTTIMERLNXSUBTIMER;
/** Pointer to a linux sub-timer. */
typedef RTTIMERLNXSUBTIMER *PRTTIMERLNXSUBTIMER;
AssertCompileMemberOffset(RTTIMERLNXSUBTIMER, LnxTimer, 0);


/**
 * The internal representation of an Linux timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed to indicate clearly that thread should exit. */
    uint32_t volatile       u32Magic;
    /** Spinlock synchronizing the fSuspended and MP event handling.
     * This is NIL_RTSPINLOCK if cCpus == 1. */
    RTSPINLOCK              hSpinlock;
    /** Flag indicating the the timer is suspended. */
    bool volatile           fSuspended;
    /** Whether the timer must run on one specific CPU or not. */
    bool                    fSpecificCpu;
#ifdef CONFIG_SMP
    /** Whether the timer must run on all CPUs or not. */
    bool                    fAllCpus;
#endif /* else: All -> specific on non-SMP kernels */
    /** The CPU it must run on if fSpecificCpu is set. */
    RTCPUID                 idCpu;
    /** The number of CPUs this timer should run on. */
    RTCPUID                 cCpus;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** User argument. */
    void                   *pvUser;
    /** The timer interval. 0 if one-shot. */
    uint64_t                u64NanoInterval;
    /** Sub-timers.
     * Normally there is just one, but for RTTIMER_FLAGS_CPU_ALL this will contain
     * an entry for all possible cpus. In that case the index will be the same as
     * for the RTCpuSet. */
    RTTIMERLNXSUBTIMER      aSubTimers[1];
} RTTIMER;


/**
 * A rtTimerLinuxStartOnCpu and rtTimerLinuxStartOnCpu argument package.
 */
typedef struct RTTIMERLINUXSTARTONCPUARGS
{
    /** The current time (RTTimeNanoTS). */
    uint64_t                u64Now;
    /** When to start firing (delta). */
    uint64_t                u64First;
} RTTIMERLINUXSTARTONCPUARGS;
/** Pointer to a rtTimerLinuxStartOnCpu argument package. */
typedef RTTIMERLINUXSTARTONCPUARGS *PRTTIMERLINUXSTARTONCPUARGS;


/**
 * Sets the state.
 */
DECLINLINE(void) rtTimerLnxSetState(RTTIMERLNXSTATE volatile *penmState, RTTIMERLNXSTATE enmNewState)
{
    ASMAtomicWriteU32((uint32_t volatile *)penmState, enmNewState);
}


/**
 * Sets the state if it has a certain value.
 */
DECLINLINE(bool) rtTimerLnxCmpXchgState(RTTIMERLNXSTATE volatile *penmState, RTTIMERLNXSTATE enmNewState, RTTIMERLNXSTATE enmCurState)
{
    return ASMAtomicCmpXchgU32((uint32_t volatile *)penmState, enmNewState, enmCurState);
}


/**
 * Gets the state.
 */
DECLINLINE(RTTIMERLNXSTATE) rtTimerLnxGetState(RTTIMERLNXSTATE volatile *penmState)
{
    return (RTTIMERLNXSTATE)ASMAtomicUoReadU32((uint32_t volatile *)penmState);
}


/**
 * Starts a sub-timer (RTTimerStart).
 *
 * @param   pSubTimer   The sub-timer to start.
 * @param   u64Now      The current timestamp (RTTimeNanoTS()).
 * @param   u64First    The interval from u64Now to the first time the timer should fire.
 */
static void rtTimerLnxStartSubTimer(PRTTIMERLNXSUBTIMER pSubTimer, uint64_t u64Now, uint64_t u64First)
{
    /*
     * Calc when it should start firing.
     */
    uint64_t u64NextTS = u64Now + u64First;
    pSubTimer->u64StartTS = u64Now;
    pSubTimer->u64NextTS = u64NextTS;

#ifdef RT_USE_LINUX_HRTIMER
    {
        /* ASSUMES RTTimeNanoTS() is implemented using ktime_get_ts(). */
        struct timespec Ts;
        ktime_t Kt;
        Ts.tv_sec  = u64NextTS / 1000000000;
        Ts.tv_nsec = u64NextTS % 1000000000;
        Kt = timespec_to_ktime(Ts);
        hrtimer_start(&pSubTimer->LnxTimer, Kt, HRTIMER_MODE_ABS);
    }
#else
    {
        uint64_t cJiffies = !u64First ? 0 : u64First / TICK_NSEC;
        if (cJiffies > MAX_JIFFY_OFFSET)
            cJiffies = MAX_JIFFY_OFFSET;
        mod_timer(&pSubTimer->LnxTimer, jiffies + cJiffies);
    }
#endif

    rtTimerLnxSetState(&pSubTimer->enmState, RTTIMERLNXSTATE_ACTIVE);
}


/**
 * Stops a sub-timer (RTTimerStart and rtTimerLinuxMpEvent()).
 *
 * @param   pSubTimer       The sub-timer.
 */
static void rtTimerLnxStopSubTimer(PRTTIMERLNXSUBTIMER pSubTimer)
{
#ifdef RT_USE_LINUX_HRTIMER
    hrtimer_cancel(&pSubTimer->LnxTimer);
#else
    if (timer_pending(&pSubTimer->LnxTimer))
        del_timer_sync(&pSubTimer->LnxTimer);
#endif

    rtTimerLnxSetState(&pSubTimer->enmState, RTTIMERLNXSTATE_STOPPED);
}


#ifdef RT_USE_LINUX_HRTIMER
/**
 * Timer callback function.
 * @returns HRTIMER_NORESTART or HRTIMER_RESTART depending on whether it's a one-shot or interval timer.
 * @param   pHrTimer    Pointer to the sub-timer structure.
 */
static enum hrtimer_restart rtTimerLinuxCallback(struct hrtimer *pHrTimer)
#else
/**
 * Timer callback function.
 * @param   ulUser      Address of the sub-timer structure.
 */
static void rtTimerLinuxCallback(unsigned long ulUser)
#endif
{
#ifdef RT_USE_LINUX_HRTIMER
    enum hrtimer_restart rc;
    PRTTIMERLNXSUBTIMER pSubTimer = (PRTTIMERLNXSUBTIMER)pHrTimer;
#else
    PRTTIMERLNXSUBTIMER pSubTimer = (PRTTIMERLNXSUBTIMER)ulUser;
#endif
    PRTTIMER pTimer = pSubTimer->pParent;

    /*
     * Don't call the handler if the timer has been suspended.
     * Also, when running on all CPUS, make sure we don't call out twice
     * on a CPU because of timer migration.
     *
     * For the specific cpu case, we're just ignoring timer migration for now... (bad)
     */
    if (    pTimer->fSuspended
#ifdef CONFIG_SMP
        ||  (   pTimer->fAllCpus
             && (pSubTimer - &pTimer->aSubTimers[0]) != RTMpCpuId())
#endif
       )
    {
        rtTimerLnxCmpXchgState(&pSubTimer->enmState, RTTIMERLNXSTATE_STOPPED, RTTIMERLNXSTATE_ACTIVE);
# ifdef RT_USE_LINUX_HRTIMER
        rc = HRTIMER_NORESTART;
# endif
    }
    else if (!pTimer->u64NanoInterval)
    {
        /*
         * One shot timer, stop it before dispatching it.
         */
        if (pTimer->cCpus == 1)
            ASMAtomicWriteBool(&pTimer->fSuspended, true);
        rtTimerLnxCmpXchgState(&pSubTimer->enmState, RTTIMERLNXSTATE_STOPPED, RTTIMERLNXSTATE_ACTIVE);
#ifdef RT_USE_LINUX_HRTIMER
        rc = HRTIMER_NORESTART;
#else
        /* detached before we're called, nothing to do for this case. */
#endif

        pTimer->pfnTimer(pTimer, pTimer->pvUser);
    }
    else
    {
        /*
         * Interval timer, calculate the next timeout and re-arm it.
         *
         * The first time around, we'll re-adjust the u64StartTS to
         * try prevent some jittering if we were started at a bad time.
         * This may of course backfire with highres timers...
         */
        const uint64_t u64NanoTS = RTTimeNanoTS();
#if 0 /* this needs to be tested before it's enabled. */
        if (    pSubTimer->iTick == 0
            &&  pTimer->u64NanoInterval < u64NanoTS)
            pSubTimer->u64StartTS = pSubTimer->u64FirstTS = u64NanoTS - 1000; /* ugly */
#endif
        pSubTimer->iTick++;
        pSubTimer->u64NextTS = pSubTimer->u64StartTS
                             + pSubTimer->iTick * pTimer->u64NanoInterval;
        if (pSubTimer->u64NextTS < u64NanoTS)
            pSubTimer->u64NextTS = u64NanoTS + RTTimerGetSystemGranularity() / 2;

#ifdef RT_USE_LINUX_HRTIMER
        {
            /* ASSUMES RTTimeNanoTS() is implemented using ktime_get_ts(). */
            struct timespec Ts;
            Ts.tv_sec  = pSubTimer->u64NextTS / 1000000000;
            Ts.tv_nsec = pSubTimer->u64NextTS % 1000000000;
            pSubTimer->LnxTimer.expires = timespec_to_ktime(Ts);
            rc = HRTIMER_RESTART;
        }
#else
        {
            uint64_t offDelta = pSubTimer->u64NextTS - u64NanoTS;
            uint64_t cJiffies = offDelta / TICK_NSEC; /* (We end up doing 64-bit div here which ever way we go.) */
            if (cJiffies > MAX_JIFFY_OFFSET)
                cJiffies = MAX_JIFFY_OFFSET;
            else if (cJiffies == 0)
                cJiffies = 1;
            mod_timer(&pSubTimer->LnxTimer, jiffies + cJiffies);
        }
#endif

        /*
         * Run the timer.
         */
        pTimer->pfnTimer(pTimer, pTimer->pvUser);
    }

#ifdef RT_USE_LINUX_HRTIMER
    return rc;
#endif
}


#ifdef CONFIG_SMP

/**
 * Per-cpu callback function (RTMpOnAll/RTMpOnSpecific).
 *
 * @param   idCpu       The current CPU.
 * @param   pvUser1     Pointer to the timer.
 * @param   pvUser2     Pointer to the argument structure.
 */
static DECLCALLBACK(void) rtTimerLnxStartAllOnCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PRTTIMERLINUXSTARTONCPUARGS pArgs = (PRTTIMERLINUXSTARTONCPUARGS)pvUser2;
    PRTTIMER pTimer = (PRTTIMER)pvUser1;
    Assert(idCpu < pTimer->cCpus);
    rtTimerLnxStartSubTimer(&pTimer->aSubTimers[idCpu], pArgs->u64Now, pArgs->u64First);
}


/**
 * Worker for RTTimerStart() that takes care of the ugly bit.s
 *
 * @returns RTTimerStart() return value.
 * @param   pTimer      The timer.
 * @param   pArgs       The argument structure.
 */
static int rtTimerLnxStartAll(PRTTIMER pTimer, PRTTIMERLINUXSTARTONCPUARGS pArgs)
{
    RTSPINLOCKTMP   Tmp;
    RTCPUID         iCpu;
    RTCPUSET        OnlineSet;
    RTCPUSET        OnlineSet2;
    int             rc2;

    /*
     * Prepare all the sub-timers for the startup and then flag the timer
     * as a whole as non-suspended, make sure we get them all before
     * clearing fSuspended as the MP handler will be waiting on this
     * should something happen while we're looping.
     */
    RTSpinlockAcquire(pTimer->hSpinlock, &Tmp);

    do
    {
        RTMpGetOnlineSet(&OnlineSet);
        for (iCpu = 0; iCpu <= pTimer->cCpus; iCpu++)
        {
            Assert(pTimer->aSubTimers[iCpu].enmState != RTTIMERLNXSTATE_MP_STOPPING);
            rtTimerLnxSetState(&pTimer->aSubTimers[iCpu].enmState,
                               RTCpuSetIsMember(&OnlineSet, iCpu)
                               ? RTTIMERLNXSTATE_STARTING
                               : RTTIMERLNXSTATE_STOPPED);
        }
    } while (!RTCpuSetIsEqual(&OnlineSet, RTMpGetOnlineSet(&OnlineSet2)));

    ASMAtomicWriteBool(&pTimer->fSuspended, false);

    RTSpinlockRelease(pTimer->hSpinlock, &Tmp);

    /*
     * Start them (can't find any exported function that allows me to
     * do this without the cross calls).
     */
    pArgs->u64Now = RTTimeNanoTS();
    rc2 = RTMpOnAll(rtTimerLnxStartAllOnCpu, pTimer, pArgs);
    AssertRC(rc2); /* screw this if it fails. */

    /*
     * Reset the sub-timers who didn't start up (ALL CPUs case).
     * CPUs that comes online between the
     */
    RTSpinlockAcquire(pTimer->hSpinlock, &Tmp);

    for (iCpu = 0; iCpu <= pTimer->cCpus; iCpu++)
        if (rtTimerLnxCmpXchgState(&pTimer->aSubTimers[iCpu].enmState, RTTIMERLNXSTATE_STOPPED, RTTIMERLNXSTATE_STARTING))
        {
            /** @todo very odd case for a rainy day. Cpus that temporarily went offline while
             * we were between calls needs to nudged as the MP handler will ignore events for
             * them because of the STARTING state. This is an extremely unlikely case - not that
             * that means anything in my experience... ;-) */
        }

    RTSpinlockRelease(pTimer->hSpinlock, &Tmp);

    return VINF_SUCCESS;
}


/**
 * Worker for RTTimerStop() that takes care of the ugly SMP bits.
 *
 * @returns RTTimerStop() return value.
 * @param   pTimer      The timer (valid).
 */
static int rtTimerLnxStopAll(PRTTIMER pTimer)
{
    RTCPUID         iCpu;
    RTSPINLOCKTMP   Tmp;


    /*
     * Mark the timer as suspended and flag all timers as stopping, except
     * for those being stopped by an MP event.
     */
    RTSpinlockAcquire(pTimer->hSpinlock, &Tmp);

    ASMAtomicWriteBool(&pTimer->fSuspended, true);
    for (iCpu = 0; iCpu < pTimer->cCpus; iCpu++)
    {
        RTTIMERLNXSTATE enmState;
        do
        {
            enmState = rtTimerLnxGetState(&pTimer->aSubTimers[iCpu].enmState);
            if (    enmState == RTTIMERLNXSTATE_STOPPED
                ||  enmState == RTTIMERLNXSTATE_MP_STOPPING)
                break;
            Assert(enmState == RTTIMERLNXSTATE_ACTIVE);
        } while (rtTimerLnxCmpXchgState(&pTimer->aSubTimers[iCpu].enmState, RTTIMERLNXSTATE_STOPPING, enmState));
    }

    RTSpinlockRelease(pTimer->hSpinlock, &Tmp);

    /*
     * Do the actual stopping. Fortunately, this doesn't require any IPIs.
     * Unfortunately it cannot be done synchronously from within the spinlock,
     * because we might end up in an active waiting for a handler to complete.
     */
    for (iCpu = 0; iCpu < pTimer->cCpus; iCpu++)
        if (rtTimerLnxGetState(&pTimer->aSubTimers[iCpu].enmState) == RTTIMERLNXSTATE_STOPPING)
            rtTimerLnxStopSubTimer(&pTimer->aSubTimers[iCpu]);

    return VINF_SUCCESS;
}


/**
 * Per-cpu callback function (RTMpOnSpecific) used by rtTimerLinuxMpEvent()
 * to start a sub-timer on a cpu that just have come online.
 *
 * @param   idCpu       The current CPU.
 * @param   pvUser1     Pointer to the timer.
 * @param   pvUser2     Pointer to the argument structure.
 */
static DECLCALLBACK(void) rtTimerLinuxMpStartOnCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PRTTIMERLINUXSTARTONCPUARGS pArgs = (PRTTIMERLINUXSTARTONCPUARGS)pvUser2;
    PRTTIMER pTimer = (PRTTIMER)pvUser1;
    RTSPINLOCK hSpinlock;
    Assert(idCpu < pTimer->cCpus);

    /*
     * We have to be kind of careful here as we might RTTimerStop (and RTTimerDestroy),
     * thus the paranoia.
     */
    hSpinlock = pTimer->hSpinlock;
    if (    hSpinlock != NIL_RTSPINLOCK
        &&  pTimer->u32Magic == RTTIMER_MAGIC)
    {
        RTSPINLOCKTMP Tmp;
        RTSpinlockAcquire(hSpinlock, &Tmp);

        if (    !pTimer->fSuspended
            &&  pTimer->u32Magic == RTTIMER_MAGIC)
        {
            /* We're sane and the timer is not suspended yet. */
            PRTTIMERLNXSUBTIMER pSubTimer = &pTimer->aSubTimers[idCpu];
            if (rtTimerLnxCmpXchgState(&pSubTimer->enmState, RTTIMERLNXSTATE_MP_STARTING, RTTIMERLNXSTATE_STOPPED))
                rtTimerLnxStartSubTimer(pSubTimer, pArgs->u64Now, pArgs->u64First);
        }

        RTSpinlockRelease(hSpinlock, &Tmp);
    }
}


/**
 * MP event notification callback.
 *
 * @param   enmEvent    The event.
 * @param   idCpu       The cpu it applies to.
 * @param   pvUser      The timer.
 */
static DECLCALLBACK(void) rtTimerLinuxMpEvent(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvUser)
{
    PRTTIMER pTimer = (PRTTIMER)pvUser;
    PRTTIMERLNXSUBTIMER pSubTimer = &pTimer->aSubTimers[idCpu];
    RTSPINLOCK hSpinlock;
    RTSPINLOCKTMP Tmp;

    Assert(idCpu < pTimer->cCpus);

    /*
     * Some initial paranoia.
     */
    if (pTimer->u32Magic != RTTIMER_MAGIC)
        return;
    hSpinlock = pTimer->hSpinlock;
    if (hSpinlock == NIL_RTSPINLOCK)
        return;

    RTSpinlockAcquireNoInts(hSpinlock, &Tmp);

    /* Is it active? */
    if (    !pTimer->fSuspended
        &&  !pTimer->u32Magic == RTTIMER_MAGIC)
    {
        switch (enmEvent)
        {
            /*
             * Try do it without leaving the spin lock, but if we have to, retake it
             * when we're on the right cpu.
             */
            case RTMPEVENT_ONLINE:
                if (rtTimerLnxCmpXchgState(&pSubTimer->enmState, RTTIMERLNXSTATE_MP_STARTING, RTTIMERLNXSTATE_STOPPED))
                {
                    RTTIMERLINUXSTARTONCPUARGS Args;
                    Args.u64Now = RTTimeNanoTS();
                    Args.u64First = pTimer->u64NanoInterval;

                    if (RTMpCpuId() == idCpu)
                        rtTimerLnxStartSubTimer(pSubTimer, Args.u64Now, Args.u64First);
                    else
                    {
                        rtTimerLnxSetState(&pSubTimer->enmState, RTTIMERLNXSTATE_STOPPED); /* we'll recheck it. */
                        RTSpinlockReleaseNoInts(hSpinlock, &Tmp);

                        RTMpOnSpecific(idCpu, rtTimerLinuxMpStartOnCpu, pTimer, &Args);
                        return; /* we've left the spinlock */
                    }
                }
                break;

            /*
             * The CPU is (going) offline, make sure the sub-timer is stopped.
             *
             * Linux will migrate it to a different CPU, but we don't want this. The
             * timer function is checking for this.
             */
            case RTMPEVENT_OFFLINE:
                if (rtTimerLnxCmpXchgState(&pSubTimer->enmState, RTTIMERLNXSTATE_MP_STOPPING, RTTIMERLNXSTATE_ACTIVE))
                {
                    RTSpinlockAcquireNoInts(hSpinlock, &Tmp);

                    rtTimerLnxStopSubTimer(pSubTimer);
                    return; /* we've left the spinlock */
                }
                break;
        }
    }

    RTSpinlockAcquireNoInts(hSpinlock, &Tmp);
}

#endif /* CONFIG_SMP */


/**
 * Callback function use by RTTimerStart via RTMpOnSpecific to start
 * a timer running on a specific CPU.
 *
 * @param   idCpu       The current CPU.
 * @param   pvUser1     Pointer to the timer.
 * @param   pvUser2     Pointer to the argument structure.
 */
static DECLCALLBACK(void) rtTimerLnxStartOnSpecificCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PRTTIMERLINUXSTARTONCPUARGS pArgs = (PRTTIMERLINUXSTARTONCPUARGS)pvUser2;
    PRTTIMER pTimer = (PRTTIMER)pvUser1;
    rtTimerLnxStartSubTimer(&pTimer->aSubTimers[0], pArgs->u64Now, pArgs->u64First);
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    RTTIMERLINUXSTARTONCPUARGS Args;
    int rc2;

    /*
     * Validate.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    if (!pTimer->fSuspended)
        return VERR_TIMER_ACTIVE;

    Args.u64First = u64First;
#ifdef CONFIG_SMP
    if (pTimer->fAllCpus)
        return rtTimerLnxStartAll(pTimer, &Args);
#endif

    /*
     * This is pretty straight forwards.
     */
    Args.u64Now = RTTimeNanoTS();
    rtTimerLnxSetState(&pTimer->aSubTimers[0].enmState, RTTIMERLNXSTATE_STARTING);
    ASMAtomicWriteBool(&pTimer->fSuspended, false);
    if (!pTimer->fSpecificCpu)
        rtTimerLnxStartSubTimer(&pTimer->aSubTimers[0], Args.u64Now, Args.u64First);
    else
    {
        rc2 = RTMpOnSpecific(pTimer->idCpu, rtTimerLnxStartOnSpecificCpu, pTimer, &Args);
        if (RT_FAILURE(rc2))
        {
            /* Suspend it, the cpu id is probably invalid or offline. */
            ASMAtomicWriteBool(&pTimer->fSuspended, true);
            rtTimerLnxSetState(&pTimer->aSubTimers[0].enmState, RTTIMERLNXSTATE_STOPPED);
            return rc2;
        }
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{

    /*
     * Validate.
     */
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    if (pTimer->fSuspended)
        return VERR_TIMER_SUSPENDED;

#ifdef CONFIG_SMP
    if (pTimer->fAllCpus)
        return rtTimerLnxStopAll(pTimer);
#endif

    /*
     * Cancel the timer.
     */
    ASMAtomicWriteBool(&pTimer->fSuspended, true); /* just to be on the safe side. */
    rtTimerLnxSetState(&pTimer->aSubTimers[0].enmState, RTTIMERLNXSTATE_STOPPING);
    rtTimerLnxStopSubTimer(&pTimer->aSubTimers[0]);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerDestroy(PRTTIMER pTimer)
{
    RTSPINLOCK hSpinlock;

    /* It's ok to pass NULL pointer. */
    if (pTimer == /*NIL_RTTIMER*/ NULL)
        return VINF_SUCCESS;
    AssertPtrReturn(pTimer, VERR_INVALID_HANDLE);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Remove the MP notifications first because it'll reduce the risk of
     * us overtaking any MP event that might theoretically be racing us here.
     */
    hSpinlock = pTimer->hSpinlock;
#ifdef CONFIG_SMP
    if (    pTimer->cCpus > 1
        &&  hSpinlock != NIL_RTSPINLOCK)
    {
        int rc = RTMpNotificationDeregister(rtTimerLinuxMpEvent, pTimer);
        AssertRC(rc);
    }
#endif /* CONFIG_SMP */

    /*
     * Stop the timer if it's running.
     */
    if (!ASMAtomicUoReadBool(&pTimer->fSuspended)) /* serious paranoia */
        RTTimerStop(pTimer);

    /*
     * Uninitialize the structure and free the associated resources.
     * The spinlock goes last.
     */
    ASMAtomicWriteU32(&pTimer->u32Magic, ~RTTIMER_MAGIC);
    RTMemFree(pTimer);
    if (hSpinlock != NIL_RTSPINLOCK)
        RTSpinlockDestroy(hSpinlock);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    PRTTIMER pTimer;
    RTCPUID  iCpu;
    unsigned cCpus;

    *ppTimer = NULL;

    /*
     * Validate flags.
     */
    if (!RTTIMER_FLAGS_IS_VALID(fFlags))
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
    cCpus = 1;
#ifdef CONFIG_SMP
    if ((fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL)
    {
        cCpus = RTMpGetMaxCpuId() + 1;
        Assert(cCpus <= RTCPUSET_MAX_CPUS); /* On linux we have a 1:1 relationship between cpuid and set index. */
        AssertReturn(u64NanoInterval, VERR_NOT_IMPLEMENTED); /* We don't implement single shot on all cpus, sorry. */
    }
#endif

    pTimer = (PRTTIMER)RTMemAllocZ(RT_OFFSETOF(RTTIMER, aSubTimers[cCpus]));
    if (!pTimer)
        return VERR_NO_MEMORY;

    /*
     * Initialize it.
     */
    pTimer->u32Magic = RTTIMER_MAGIC;
    pTimer->hSpinlock = NIL_RTSPINLOCK;
    pTimer->fSuspended = true;
#ifdef CONFIG_SMP
    pTimer->fSpecificCpu = (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC) && (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL;
    pTimer->fAllCpus = (fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL;
    pTimer->idCpu = fFlags & RTTIMER_FLAGS_CPU_MASK;
#else
    pTimer->fSpecificCpu = !!(fFlags & RTTIMER_FLAGS_CPU_SPECIFIC);
    pTimer->idCpu = RTMpCpuId();
#endif
    pTimer->cCpus = cCpus;
    pTimer->pfnTimer = pfnTimer;
    pTimer->pvUser = pvUser;
    pTimer->u64NanoInterval = u64NanoInterval;

    for (iCpu = 0; iCpu < cCpus; iCpu++)
    {
#ifdef RT_USE_LINUX_HRTIMER
        hrtimer_init(&pTimer->aSubTimers[iCpu].LnxTimer, CLOCK_MONOTONIC, HRTIMER_MODE_ABS);
        pTimer->aSubTimers[iCpu].LnxTimer.function = rtTimerLinuxCallback;
#else
        init_timer(&pTimer->aSubTimers[iCpu].LnxTimer);
        pTimer->aSubTimers[iCpu].LnxTimer.data     = (unsigned long)pTimer;
        pTimer->aSubTimers[iCpu].LnxTimer.function = rtTimerLinuxCallback;
        pTimer->aSubTimers[iCpu].LnxTimer.expires  = jiffies;
#endif
        pTimer->aSubTimers[iCpu].u64StartTS = 0;
        pTimer->aSubTimers[iCpu].u64NextTS = 0;
        pTimer->aSubTimers[iCpu].iTick = 0;
        pTimer->aSubTimers[iCpu].pParent = pTimer;
        pTimer->aSubTimers[iCpu].enmState = RTTIMERLNXSTATE_STOPPED;
    }

#ifdef CONFIG_SMP
    /*
     * If this is running on ALL cpus, we'll have to register a callback
     * for MP events (so timers can be started/stopped on cpus going
     * online/offline). We also create the spinlock for syncrhonizing
     * stop/start/mp-event.
     */
    if (cCpus > 1)
    {
        int rc = RTSpinlockCreate(&pTimer->hSpinlock);
        if (RT_SUCCESS(rc))
            rc = RTMpNotificationRegister(rtTimerLinuxMpEvent, pTimer);
        else
            pTimer->hSpinlock = NIL_RTSPINLOCK;
        if (RT_FAILURE(rc))
        {
            RTTimerDestroy(pTimer);
            return rc;
        }
    }
#endif /* CONFIG_SMP */

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


RTDECL(uint32_t) RTTimerGetSystemGranularity(void)
{
#ifdef RT_USE_LINUX_HRTIMER
    /** @todo later... */
    return 1000000000 / HZ; /* ns */
#else
    return 1000000000 / HZ; /* ns */
#endif
}


RTDECL(int) RTTimerRequestSystemGranularity(uint32_t u32Request, uint32_t *pu32Granted)
{
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTTimerReleaseSystemGranularity(uint32_t u32Granted)
{
    return VERR_NOT_SUPPORTED;
}

