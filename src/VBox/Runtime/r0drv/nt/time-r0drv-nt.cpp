/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Time, Ring-0 Driver, Nt.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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
#include "the-nt-kernel.h"
#include <iprt/time.h>


DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
#if 1
    ULONGLONG InterruptTime = KeQueryInterruptTime();
    return (uint64_t)InterruptTime * 100;
#else
    LARGE_INTEGER InterruptTime;
    do
    {
        InterruptTime.HighPart = ((KUSER_SHARED_DATA volatile *)SharedUserData)->InterruptTime.High1Time;
        InterruptTime.LowPart = ((KUSER_SHARED_DATA volatile *)SharedUserData)->InterruptTime.LowPart;
    } while (((KUSER_SHARED_DATA volatile *)SharedUserData)->InterruptTime.High2Time != InterruptTime.HighPart);

    return (uint64_t)InterruptTime.QuadPart * 100;
#endif
}


RTDECL(uint64_t) RTTimeNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


RTDECL(uint64_t) RTTimeMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / 1000000;
}


RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / 1000000;
}


RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    LARGE_INTEGER SystemTime;
#if 1
    KeQuerySystemTime(&SystemTime);
#else
    do
    {
        SystemTime.HighPart = ((KUSER_SHARED_DATA volatile *)SharedUserData)->SystemTime.High1Time;
        SystemTime.LowPart = ((KUSER_SHARED_DATA volatile *)SharedUserData)->SystemTime.LowPart;
    } while (((KUSER_SHARED_DATA volatile *)SharedUserData)->SystemTime.High2Time != SystemTime.HighPart);
#endif
    return RTTimeSpecSetNtTime(pTime, SystemTime.QuadPart);
}

