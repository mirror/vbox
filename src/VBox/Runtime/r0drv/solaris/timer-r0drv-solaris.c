/** $Id$ */
/** @file
 * Incredibly Portable Runtime - Timer, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/timer.h>
#include <iprt/time.h>
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
 * The internal representation of a Solaris timer handle.
 */
typedef struct RTTIMER
{
    /** Magic.
     * This is RTTIMER_MAGIC, but changes to something else before the timer
     * is destroyed to indicate clearly that thread should exit. */
    uint32_t volatile       u32Magic;
    /** Flag indicating the the timer is suspended. */
    uint8_t volatile        fSuspended;
    /** Whether the timer must run on a specific CPU or not. */
    uint8_t                 fSpecificCpu;
    /** The CPU it must run on if fSpecificCpu is set. */
    uint8_t                 iCpu;
    /** The Solaris cyclic structure. */
    cyc_handler_t           CyclicInfo;
    /** The Solaris cyclic handle. */
    cyclic_id_t             CyclicID;
    /** Callback. */
    PFNRTTIMER              pfnTimer;
    /** User argument. */
    void                   *pvUser;
    /** The timer interval. 0 for one-shot timer */
    uint64_t                u64NanoInterval;
} RTTIMER;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void rtTimerSolarisCallback(void *pvTimer);
static void rtTimerSolarisStop(PRTTIMER pTimer);


RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    *ppTimer = NULL;

    /*
     * Validate flags.
     */
    if (!RTTIMER_FLAGS_IS_VALID(fFlags))
        return VERR_INVALID_PARAMETER;
    if (    (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC)
        /** @todo implement &&  (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL*/)
        return VERR_NOT_SUPPORTED;
    
    /*
     * Allocate and initialize the timer handle.
     */
    PRTTIMER pTimer = (PRTTIMER)RTMemAlloc(sizeof(*pTimer));
    if (!pTimer)
        return VERR_NO_MEMORY;

    pTimer->u32Magic = RTTIMER_MAGIC;
    pTimer->fSuspended = true;
    pTimer->fSpecificCpu = !!(fFlags & RTTIMER_FLAGS_CPU_SPECIFIC);
    pTimer->iCpu = fFlags & RTTIMER_FLAGS_CPU_MASK;
    pTimer->CyclicInfo.cyh_func = rtTimerSolarisCallback;
    pTimer->CyclicInfo.cyh_level = CY_LOCK_LEVEL;
    pTimer->CyclicInfo.cyh_arg = pTimer;
    pTimer->CyclicID = CYCLIC_NONE;
    pTimer->u64NanoInterval = u64NanoInterval;
    pTimer->pfnTimer = pfnTimer;
    pTimer->pvUser = pvUser;

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


/**
 * Validates the timer handle.
 *
 * @returns true if valid, false if invalid.
 * @param   pTimer  The handle.
 */
DECLINLINE(bool) rtTimerIsValid(PRTTIMER pTimer)
{
    AssertReturn(VALID_PTR(pTimer), false);
    AssertReturn(pTimer->u32Magic == RTTIMER_MAGIC, false);
    return true;
}


RTDECL(int) RTTimerDestroy(PRTTIMER pTimer)
{
    if (pTimer == NULL)
        return VINF_SUCCESS;
    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;

    /*
     * Free the associated resources.
     */
    pTimer->u32Magic++;
    rtTimerSolarisStop(pTimer);
    RTMemFree(pTimer);
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    cyc_time_t timerSpec;

    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;
    if (!pTimer->fSuspended)
        return VERR_TIMER_ACTIVE;

    /*
     * Calc when it should start fireing.
     */
    u64First += RTTimeNanoTS();

    pTimer->fSuspended = false;
    timerSpec.cyt_when = u64First;
    timerSpec.cyt_interval = pTimer->u64NanoInterval == 0 ? u64First : pTimer->u64NanoInterval;
    
    mutex_enter(&cpu_lock);
    pTimer->CyclicID = cyclic_add(&pTimer->CyclicInfo, &timerSpec);
    mutex_exit(&cpu_lock);

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;
    if (pTimer->fSuspended)
        return VERR_TIMER_SUSPENDED;

    /*
     * Suspend the timer.
     */
    pTimer->fSuspended = true;
    rtTimerSolarisStop(pTimer);
    
    return VINF_SUCCESS;
}


static void rtTimerSolarisCallback(void *pvTimer)
{
    PRTTIMER pTimer = (PRTTIMER)pvTimer;

    /* If this is a one shot timer, call pfnTimer and suspend
     * as Solaris does not support 0 interval timers implictly
     */
    if (!pTimer->u64NanoInterval)
    {
        pTimer->fSuspended = true;
        rtTimerSolarisStop(pTimer);
    }

    /* Callback user defined callback function */
    pTimer->pfnTimer(pTimer, pTimer->pvUser);
}


static void rtTimerSolarisStop(PRTTIMER pTimer)
{
    /* Important we check for invalid cyclic object */
    if (pTimer->CyclicID != CYCLIC_NONE)
    {
        mutex_enter(&cpu_lock);
        cyclic_remove(pTimer->CyclicID);
        mutex_exit(&cpu_lock);
        pTimer->CyclicID = CYCLIC_NONE;
    }
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

