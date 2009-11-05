/* $Id$ */
/** @file
 * IPRT - Timer, Ring-0 Driver, Solaris.
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
#include "../the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/timer.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/spinlock.h>
#include <iprt/time.h>
#include <iprt/thread.h>
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
    /** Flag indicating that the timer is suspended. */
    uint8_t volatile        fSuspended;
    /** Run on all CPUs if set */
    uint8_t                 fAllCpu;
    /** Whether the timer must run on a specific CPU or not. */
    uint8_t                 fSpecificCpu;
    /** The CPU it must run on if fSpecificCpu is set. */
    uint8_t                 iCpu;
    /** The nano second interval for repeating timers */
    uint64_t                interval;
    /** simple Solaris timer handle. */
    vbi_stimer_t           *stimer;
    /** global Solaris timer handle. */
    vbi_gtimer_t           *gtimer;
    /** The user callback. */
    PFNRTTIMER              pfnTimer;
    /** The argument for the user callback. */
    void                   *pvUser;
} RTTIMER;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates that the timer is valid. */
#define RTTIMER_ASSERT_VALID_RET(pTimer) \
    do \
    { \
        AssertPtrReturn(pTimer, VERR_INVALID_HANDLE); \
        AssertReturn((pTimer)->u32Magic == RTTIMER_MAGIC, VERR_INVALID_HANDLE); \
    } while (0)


/*
 * Need a wrapper to get the PRTTIMER passed through
 */
static void rtTimerSolarisCallbackWrapper(PRTTIMER pTimer, uint64_t tick)
{
    pTimer->pfnTimer(pTimer, pTimer->pvUser, tick);
}




RTDECL(int) RTTimerCreateEx(PRTTIMER *ppTimer, uint64_t u64NanoInterval, unsigned fFlags, PFNRTTIMER pfnTimer, void *pvUser)
{
    RT_ASSERT_PREEMPTIBLE();
    *ppTimer = NULL;

    /*
     * Validate flags.
     */
    if (!RTTIMER_FLAGS_ARE_VALID(fFlags))
        return VERR_INVALID_PARAMETER;
    if (vbi_revision_level < 2)
        return VERR_NOT_SUPPORTED;

    if (    (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC)
        &&  (fFlags & RTTIMER_FLAGS_CPU_ALL) != RTTIMER_FLAGS_CPU_ALL
        &&  !RTMpIsCpuPossible((fFlags & RTTIMER_FLAGS_CPU_MASK)))
        return VERR_CPU_NOT_FOUND;

    if ((fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL && u64NanoInterval == 0)
        return VERR_NOT_SUPPORTED;

    /*
     * Allocate and initialize the timer handle.
     */
    PRTTIMER pTimer = (PRTTIMER)RTMemAlloc(sizeof(*pTimer));
    if (!pTimer)
        return VERR_NO_MEMORY;

    pTimer->u32Magic = RTTIMER_MAGIC;
    pTimer->fSuspended = true;
    if ((fFlags & RTTIMER_FLAGS_CPU_ALL) == RTTIMER_FLAGS_CPU_ALL)
    {
        pTimer->fAllCpu = true;
        pTimer->fSpecificCpu = false;
        pTimer->iCpu = 255;
    }
    else if (fFlags & RTTIMER_FLAGS_CPU_SPECIFIC)
    {
        pTimer->fAllCpu = false;
        pTimer->fSpecificCpu = true;
        pTimer->iCpu = fFlags & RTTIMER_FLAGS_CPU_MASK;
    }
    else
    {
        pTimer->fAllCpu = false;
        pTimer->fSpecificCpu = false;
        pTimer->iCpu = 255;
    }
    pTimer->interval = u64NanoInterval;
    pTimer->pfnTimer = pfnTimer;
    pTimer->pvUser = pvUser;
    pTimer->stimer = NULL;
    pTimer->gtimer = NULL;

    *ppTimer = pTimer;
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerDestroy(PRTTIMER pTimer)
{
    if (pTimer == NULL)
        return VINF_SUCCESS;
    RTTIMER_ASSERT_VALID_RET(pTimer);
    RT_ASSERT_INTS_ON();

    /*
     * Free the associated resources.
     */
    RTTimerStop(pTimer);
    ASMAtomicWriteU32(pTimer, ~RTTIMER_MAGIC);
    RTMemFree(pTimer);
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    RTTIMER_ASSERT_VALID_RET(pTimer);
    RT_ASSERT_INTS_ON();

    if (!pTimer->fSuspended)
        return VERR_TIMER_ACTIVE;

    pTimer->fSuspended = false;
    if (pTimer->fAllCpu)
    {
        pTimer->gtimer = vbi_gtimer_begin(rtTimerSolarisCallbackWrapper, pTimer, u64First, pTimer->interval);
        if (pTimer->gtimer == NULL)
            return VERR_INVALID_PARAMETER;
    }
    else
    {
        int cpu = VBI_ANY_CPU;
        if (pTimer->fSpecificCpu)
            cpu = pTimer->iCpu;
        pTimer->stimer = vbi_stimer_begin(rtTimerSolarisCallbackWrapper, pTimer, u64First, pTimer->interval, cpu);
        if (pTimer->stimer == NULL)
        {
            if (cpu != VBI_ANY_CPU)
                return VERR_CPU_OFFLINE;
            return VERR_INVALID_PARAMETER;
        }
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStop(PRTTIMER pTimer)
{
    RTTIMER_ASSERT_VALID_RET(pTimer);
    RT_ASSERT_INTS_ON();

    if (pTimer->fSuspended)
        return VERR_TIMER_SUSPENDED;

    pTimer->fSuspended = true;
    if (pTimer->stimer)
    {
        vbi_stimer_end(pTimer->stimer);
        pTimer->stimer = NULL;
    }
    else if (pTimer->gtimer)
    {
        vbi_gtimer_end(pTimer->gtimer);
        pTimer->gtimer = NULL;
    }

    return VINF_SUCCESS;
}



RTDECL(uint32_t) RTTimerGetSystemGranularity(void)
{
    return vbi_timer_granularity();
}


RTDECL(int) RTTimerRequestSystemGranularity(uint32_t u32Request, uint32_t *pu32Granted)
{
    return VERR_NOT_SUPPORTED;
}


RTDECL(int) RTTimerReleaseSystemGranularity(uint32_t u32Granted)
{
    return VERR_NOT_SUPPORTED;
}

