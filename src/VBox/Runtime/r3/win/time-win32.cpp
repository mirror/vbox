/* $Id$ */
/** @file
 * innotek Portable Runtime - Time, win32.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <iprt/assert.h>
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
RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    uint64_t u64;
    AssertCompile(sizeof(u64) == sizeof(FILETIME));
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return RTTimeSpecSetNtTime(pTime, u64);
}


/**
 * Gets the current local system time.
 *
 * @returns pTime.
 * @param   pTime   Where to store the local time.
 */
RTDECL(PRTTIMESPEC) RTTimeLocalNow(PRTTIMESPEC pTime)
{
    uint64_t u64;
    AssertCompile(sizeof(u64) == sizeof(FILETIME));
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    uint64_t u64Local;
    if (!FileTimeToLocalFileTime((FILETIME const *)&u64, (LPFILETIME)&u64Local))
        u64Local = u64;
    return RTTimeSpecSetNtTime(pTime, u64Local);
}


/**
 * Gets the delta between UTC and local time.
 *
 * @code
 *      RTTIMESPEC LocalTime;
 *      RTTimeSpecAddNano(RTTimeNow(&LocalTime), RTTimeLocalDeltaNano());
 * @endcode
 *
 * @returns Returns the nanosecond delta between UTC and local time.
 */
RTDECL(int64_t) RTTimeLocalDeltaNano(void)
{
    /*
     * UTC = local + Tzi.Bias;
     * The bias is given in minutes.
     */
    TIME_ZONE_INFORMATION Tzi;
    Tzi.Bias = 0;
    if (GetTimeZoneInformation(&Tzi) != TIME_ZONE_ID_INVALID)
        return -(int64_t)Tzi.Bias * 60*1000*1000*1000;
    return 0;
}


/**
 * Explodes a time spec to the localized timezone.
 *
 * @returns pTime.
 * @param   pTime       Where to store the exploded time.
 * @param   pTimeSpec   The time spec to exploded. (UTC)
 */
RTDECL(PRTTIME) RTTimeLocalExplode(PRTTIME pTime, PCRTTIMESPEC pTimeSpec)
{
    /*
     * FileTimeToLocalFileTime does not do the right thing, so we'll have
     * to convert to system time and SystemTimeToTzSpecificLocalTime instead.
     */
    RTTIMESPEC LocalTime;
    SYSTEMTIME SystemTimeIn;
    FILETIME FileTime;
    if (FileTimeToSystemTime(RTTimeSpecGetNtFileTime(pTimeSpec, &FileTime), &SystemTimeIn))
    {
        SYSTEMTIME SystemTimeOut;
        if (SystemTimeToTzSpecificLocalTime(NULL /* use current TZI */,
                                            &SystemTimeIn,
                                            &SystemTimeOut))
        {
            if (SystemTimeToFileTime(&SystemTimeOut, &FileTime))
            {
                RTTimeSpecSetNtFileTime(&LocalTime, &FileTime);
                pTime = RTTimeExplode(pTime, &LocalTime);
                if (pTime)
                    pTime->fFlags = (pTime->fFlags & ~RTTIME_FLAGS_TYPE_MASK) | RTTIME_FLAGS_TYPE_LOCAL;
                return pTime;
            }
        }
    }

    /*
     * The fallback is to use the current offset.
     * (A better fallback would be to use the offset of the same time of the year.)
     */
    LocalTime = *pTimeSpec;
    RTTimeSpecAddNano(&LocalTime, RTTimeLocalDeltaNano());
    pTime = RTTimeExplode(pTime, &LocalTime);
    if (pTime)
        pTime->fFlags = (pTime->fFlags & ~RTTIME_FLAGS_TYPE_MASK) | RTTIME_FLAGS_TYPE_LOCAL;
    return pTime;
}

