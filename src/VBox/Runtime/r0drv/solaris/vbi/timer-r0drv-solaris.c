/** $Id$ */
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
    /** The Solaris timer handle. */
    void                    *handle;
} RTTIMER;


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
    pTimer->handle = vbi_timer_create(pfnTimer, pTimer, pvUser, u64NanoInterval);

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
    vbi_timer_stop(pTimer->handle);
    vbi_timer_destroy(pTimer->handle);
    RTMemFree(pTimer);
    return VINF_SUCCESS;
}


RTDECL(int) RTTimerStart(PRTTIMER pTimer, uint64_t u64First)
{
    if (!rtTimerIsValid(pTimer))
        return VERR_INVALID_HANDLE;
    if (!pTimer->fSuspended)
        return VERR_TIMER_ACTIVE;

    /*
     * Calc when it should start firing.
     */
    pTimer->fSuspended = false;
    vbi_timer_start(pTimer->handle, u64First);

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
    vbi_timer_stop(pTimer->handle);

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

