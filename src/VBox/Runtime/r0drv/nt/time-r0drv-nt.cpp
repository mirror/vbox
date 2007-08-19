/* $Id$ */
/** @file
 * innotek Portable Runtime - Time, Ring-0 Driver, Nt.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
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

