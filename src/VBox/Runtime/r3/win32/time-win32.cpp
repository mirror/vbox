/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Time, win32.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_TIME
//#define USE_TICK_COUNT
//#define USE_PERFORMANCE_COUNTER
#define USE_FILE_TIME
//#define USE_INTERRUPT_TIME
#ifndef USE_INTERRUPT_TIME
# include <Windows.h>
#else
# define _X86_
  extern "C" {
# include <ntddk.h>
  }
# undef PAGE_SIZE
# undef PAGE_SHIFT
#endif

#include <iprt/time.h>
#include <iprt/asm.h>
#include "internal/time.h"


DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
#if defined USE_TICK_COUNT
    return (uint64_t)GetTickCount() * (uint64_t)1000000;

#elif defined USE_PERFORMANCE_COUNTER
    static LARGE_INTEGER    llFreq;
    static unsigned         uMult;
    if (!llFreq.QuadPart)
    {
        if (!QueryPerformanceFrequency(&llFreq))
            return (uint64_t)GetTickCount() * (uint64_t)1000000;
        llFreq.QuadPart /=    1000;
        uMult            = 1000000;     /* no math genious, but this seemed to help avoiding floating point. */
    }

    LARGE_INTEGER   ll;
    if (QueryPerformanceCounter(&ll))
        return (ll.QuadPart * uMult) / llFreq.QuadPart;
    else
        return (uint64_t)GetTickCount() * (uint64_t)1000000;

#elif defined USE_FILE_TIME
    uint64_t u64; /* manual say larger integer, should be safe to assume it's the same. */
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return u64 * 100;

#elif defined USE_INTERRUPT_TIME

    /* HACK! HACK! HACK! HACK! HACK! HACK! */
    /* HACK! HACK! HACK! HACK! HACK! HACK! */
    /* HACK! HACK! HACK! HACK! HACK! HACK! */
# error "don't use this in production"

    static const KUSER_SHARED_DATA *s_pUserSharedData = NULL;
    if (!s_pUserSharedData)
    {
        /** @todo clever detection algorithm.
         * The com debugger class exports this too, windbg knows it too... */
        s_pUserSharedData = (const KUSER_SHARED_DATA *)0x7ffe0000;
    }

    /* use interrupt time */
    LARGE_INTEGER Time;
    do
    {
        Time.HighPart = s_pUserSharedData->InterruptTime.High1Time;
        Time.LowPart = s_pUserSharedData->InterruptTime.LowPart;
    } while (s_pUserSharedData->InterruptTime.High2Time != Time.HighPart);

    return (uint64_t)Time.QuadPart * 100;

#else
# error "Must select a method bright guy!"
#endif
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
RTR3DECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    uint64_t u64; /* manual say larger integer, should be safe to assume it's the same. */
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return RTTimeSpecSetNtTime(pTime, u64);
}

