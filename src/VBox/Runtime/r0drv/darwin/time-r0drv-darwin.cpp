/* $Id$ */
/** @file
 * innotek Portable Runtime - Time, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP RTLOGGROUP_TIME
#include "the-darwin-kernel.h"
#include <iprt/time.h>
#include <iprt/asm.h>


DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
    static int8_t s_fSimple = -1;

    /* first call: check if life is simple or not. */
    if (s_fSimple < 0)
    {
        struct mach_timebase_info Info;
        clock_timebase_info(&Info);
        ASMAtomicXchgS8((int8_t * volatile)&s_fSimple, Info.denom == 1 && Info.numer == 1);
    }

    /* special case: absolute time is in nanoseconds */
    if (s_fSimple)
        return mach_absolute_time();

    /* general case: let mach do the mult/div for us. */
    uint64_t u64;
    absolutetime_to_nanoseconds(mach_absolute_time(), &u64);
    return u64;
}


/**
 * Gets the current nanosecond timestamp.
 *
 * @returns nanosecond timestamp.
 */
RTDECL(uint64_t) RTTimeNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


/**
 * Gets the current millisecond timestamp.
 *
 * @returns millisecond timestamp.
 */
RTDECL(uint64_t) RTTimeMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / 1000000;
}


/**
 * Gets the current nanosecond timestamp.
 *
 * This differs from RTTimeNanoTS in that it will use system APIs and not do any
 * resolution or performance optimizations.
 *
 * @returns nanosecond timestamp.
 */
RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


/**
 * Gets the current millisecond timestamp.
 *
 * This differs from RTTimeNanoTS in that it will use system APIs and not do any
 * resolution or performance optimizations.
 *
 * @returns millisecond timestamp.
 */
RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / 1000000;
}


/**
 * Gets the current system time.
 *
 * @returns pTime.
 * @param   pTime   Where to store the time.
 */
RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    uint32_t u32Secs;
    uint32_t u32Nanosecs;
    clock_get_calendar_nanotime(&u32Secs, &u32Nanosecs);
    return RTTimeSpecSetNano(pTime, (uint64_t)u32Secs * 1000000000 + u32Nanosecs);
}

