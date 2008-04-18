/* $Id$ */
/** @file
 * innotek Portable Runtime - Time, Darwin.
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
#define LOG_GROUP RTLOGGROUP_TIME
#define RTTIME_INCL_TIMEVAL
#include <mach/mach_time.h>
#include <mach/kern_return.h>
#include <sys/time.h>
#include <time.h>

#include <iprt/time.h>
#include "internal/time.h"


DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
    static struct mach_timebase_info    s_Info = { 0, 0 };
    static double                       s_rdFactor = 0.0;

    /* get the factors the first time (pray we're not racing anyone) */
    if (s_Info.denom == 0)
    {
        static bool s_fFailedToGetTimeBaseInfo = false;
        if (!s_fFailedToGetTimeBaseInfo)
        {
            struct mach_timebase_info Info;
            if (mach_timebase_info(&Info) != KERN_SUCCESS)
            {
                s_rdFactor = (double)Info.numer / (double)Info.denom;
                s_Info = Info;
            }
            else
            {
                s_Info.denom = s_Info.numer = 0;
                s_fFailedToGetTimeBaseInfo = true;
            }
        }
    }

    /* special case: absolute time is in nanoseconds */
    if (s_Info.denom == 1 && s_Info.numer == 1)
        return mach_absolute_time();

    /* general case: multiply by factor to get nanoseconds. */
    if (s_rdFactor != 0.0)
        return mach_absolute_time() * s_rdFactor;

    /* worst case: fallback to gettimeofday(). */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec  * (uint64_t)(1000 * 1000 * 1000)
         + (uint64_t)(tv.tv_usec * 1000);
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
    return rtTimeGetSystemNanoTS();
}


/**
 * Gets the current system time.
 *
 * @returns pTime.
 * @param   pTime   Where to store the time.
 */
RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    /** @todo find nanosecond API for getting time of day. */
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return RTTimeSpecSetTimeval(pTime, &tv);
}

