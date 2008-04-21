/** @file
 * IPRT - Time.
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

#ifndef ___iprt_time_h
#define ___iprt_time_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

__BEGIN_DECLS

/** @defgroup grp_rt_time   RTTime - Time
 * @ingroup grp_rt
 * @{
 */

/** Time Specification.
 *
 * Use the inline RTTimeSpecGet/Set to operate on structure this so we
 * can easily change the representation if required later.
 *
 * The current representation is in nanoseconds relative to the unix epoch
 * (1970-01-01 00:00:00 UTC). This gives us an approximate span from
 * 1678 to 2262 without sacrifying the resolution offered by the various
 * host OSes (BSD & LINUX 1ns, NT 100ns).
 */
typedef struct RTTIMESPEC
{
    /** Nanoseconds since epoch.
     * The name is intentially too long to be comfortable to use because you should be
     * using inline helpers! */
    int64_t     i64NanosecondsRelativeToUnixEpoch;
} RTTIMESPEC;
/** Pointer to a time spec structure. */
typedef RTTIMESPEC *PRTTIMESPEC;
/** Pointer to a const time spec structure. */
typedef const RTTIMESPEC *PCRTTIMESPEC;


/** @name RTTIMESPEC methods
 * @{ */

/**
 * Gets the time as nanoseconds relative to the unix epoch.
 *
 * @returns Nanoseconds relative to unix epoch.
 * @param   pTime       The time spec to interpret.
 */
DECLINLINE(int64_t) RTTimeSpecGetNano(PCRTTIMESPEC pTime)
{
    return pTime->i64NanosecondsRelativeToUnixEpoch;
}


/**
 * Sets the time give by nanoseconds relative to the unix epoch.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Nano     The new time in nanoseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetNano(PRTTIMESPEC pTime, int64_t i64Nano)
{
    pTime->i64NanosecondsRelativeToUnixEpoch = i64Nano;
    return pTime;
}


/**
 * Gets the time as microseconds relative to the unix epoch.
 *
 * @returns microseconds relative to unix epoch.
 * @param   pTime       The time spec to interpret.
 */
DECLINLINE(int64_t) RTTimeSpecGetMicro(PCRTTIMESPEC pTime)
{
    return pTime->i64NanosecondsRelativeToUnixEpoch / 1000;
}


/**
 * Sets the time given by microseconds relative to the unix epoch.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Micro    The new time in microsecond.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetMicro(PRTTIMESPEC pTime, int64_t i64Micro)
{
    pTime->i64NanosecondsRelativeToUnixEpoch = i64Micro * 1000;
    return pTime;
}


/**
 * Gets the time as milliseconds relative to the unix epoch.
 *
 * @returns milliseconds relative to unix epoch.
 * @param   pTime       The time spec to interpret.
 */
DECLINLINE(int64_t) RTTimeSpecGetMilli(PCRTTIMESPEC pTime)
{
    return pTime->i64NanosecondsRelativeToUnixEpoch / 1000000;
}


/**
 * Sets the time given by milliseconds relative to the unix epoch.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Milli    The new time in milliseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetMilli(PRTTIMESPEC pTime, int64_t i64Milli)
{
    pTime->i64NanosecondsRelativeToUnixEpoch = i64Milli * 1000000;
    return pTime;
}


/**
 * Gets the time as seconds relative to the unix epoch.
 *
 * @returns seconds relative to unix epoch.
 * @param   pTime       The time spec to interpret.
 */
DECLINLINE(int64_t) RTTimeSpecGetSeconds(PCRTTIMESPEC pTime)
{
    return pTime->i64NanosecondsRelativeToUnixEpoch / 1000000000;
}


/**
 * Sets the time given by seconds relative to the unix epoch.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Seconds  The new time in seconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetSeconds(PRTTIMESPEC pTime, int64_t i64Seconds)
{
    pTime->i64NanosecondsRelativeToUnixEpoch = i64Seconds * 1000000000;
    return pTime;
}


/**
 * Makes the time spec absolute like abs() does (i.e. a positive value).
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecAbsolute(PRTTIMESPEC pTime)
{
    if (pTime->i64NanosecondsRelativeToUnixEpoch < 0)
        pTime->i64NanosecondsRelativeToUnixEpoch = -pTime->i64NanosecondsRelativeToUnixEpoch;
    return pTime;
}


/**
 * Negates the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecNegate(PRTTIMESPEC pTime)
{
    pTime->i64NanosecondsRelativeToUnixEpoch = -pTime->i64NanosecondsRelativeToUnixEpoch;
    return pTime;
}


/**
 * Adds a time period to the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   pTimeAdd    The time spec to add to pTime.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecAdd(PRTTIMESPEC pTime, PCRTTIMESPEC pTimeAdd)
{
    pTime->i64NanosecondsRelativeToUnixEpoch += pTimeAdd->i64NanosecondsRelativeToUnixEpoch;
    return pTime;
}


/**
 * Adds a time period give as nanoseconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Nano     The time period in nanoseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecAddNano(PRTTIMESPEC pTime, int64_t i64Nano)
{
    pTime->i64NanosecondsRelativeToUnixEpoch += i64Nano;
    return pTime;
}


/**
 * Adds a time period give as microseconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Micro    The time period in microseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecAddMicro(PRTTIMESPEC pTime, int64_t i64Micro)
{
    pTime->i64NanosecondsRelativeToUnixEpoch += i64Micro * 1000;
    return pTime;
}


/**
 * Adds a time period give as milliseconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Milli    The time period in milliseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecAddMilli(PRTTIMESPEC pTime, int64_t i64Milli)
{
    pTime->i64NanosecondsRelativeToUnixEpoch += i64Milli * 1000000;
    return pTime;
}


/**
 * Adds a time period give as seconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Seconds  The time period in seconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecAddSeconds(PRTTIMESPEC pTime, int64_t i64Seconds)
{
    pTime->i64NanosecondsRelativeToUnixEpoch += i64Seconds * 1000000000;
    return pTime;
}


/**
 * Subtracts a time period from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   pTimeSub    The time spec to subtract from pTime.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSub(PRTTIMESPEC pTime, PCRTTIMESPEC pTimeSub)
{
    pTime->i64NanosecondsRelativeToUnixEpoch -= pTimeSub->i64NanosecondsRelativeToUnixEpoch;
    return pTime;
}


/**
 * Subtracts a time period give as nanoseconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Nano     The time period in nanoseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSubNano(PRTTIMESPEC pTime, int64_t i64Nano)
{
    pTime->i64NanosecondsRelativeToUnixEpoch -= i64Nano;
    return pTime;
}


/**
 * Subtracts a time period give as microseconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Micro    The time period in microseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSubMicro(PRTTIMESPEC pTime, int64_t i64Micro)
{
    pTime->i64NanosecondsRelativeToUnixEpoch -= i64Micro * 1000;
    return pTime;
}


/**
 * Subtracts a time period give as milliseconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Milli    The time period in milliseconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSubMilli(PRTTIMESPEC pTime, int64_t i64Milli)
{
    pTime->i64NanosecondsRelativeToUnixEpoch -= i64Milli * 1000000;
    return pTime;
}


/**
 * Subtracts a time period give as seconds from the time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Seconds  The time period in seconds.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSubSeconds(PRTTIMESPEC pTime, int64_t i64Seconds)
{
    pTime->i64NanosecondsRelativeToUnixEpoch -= i64Seconds * 100000000;
    return pTime;
}


/* PORTME: Add struct timeval guard macro here. */
#if defined(RTTIME_INCL_TIMEVAL) || defined(_STRUCT_TIMEVAL) || defined(_SYS__TIMEVAL_H_) || defined(_SYS_TIME_H) || defined(_TIMEVAL)
/**
 * Gets the time as POSIX timeval.
 *
 * @returns pTime.
 * @param   pTime       The time spec to interpret.
 * @param   pTimeval    Where to store the time as POSIX timeval.
 */
DECLINLINE(struct timeval *) RTTimeSpecGetTimeval(PCRTTIMESPEC pTime, struct timeval *pTimeval)
{
    int64_t i64 = RTTimeSpecGetMicro(pTime);
    int32_t i32Micro = (int32_t)(i64 % 1000000);
    i64 /= 1000000;
    if (i32Micro < 0)
    {
        i32Micro += 1000000;
        i64++;
    }
    pTimeval->tv_sec = (time_t)i64;
    pTimeval->tv_usec = i32Micro;
    return pTimeval;
}

/**
 * Sets the time as POSIX timeval.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   pTimeval    Pointer to the POSIX timeval struct with the new time.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetTimeval(PRTTIMESPEC pTime, const struct timeval *pTimeval)
{
    return RTTimeSpecAddMicro(RTTimeSpecSetSeconds(pTime, pTimeval->tv_sec), pTimeval->tv_usec);
}
#endif /* various ways of detecting struct timeval */


/* PORTME: Add struct timespec guard macro here. */
#if defined(RTTIME_INCL_TIMESPEC) || defined(_STRUCT_TIMESPEC) || defined(_SYS__TIMESPEC_H_) || defined(TIMEVAL_TO_TIMESPEC) || defined(_TIMESPEC)
/**
 * Gets the time as POSIX timespec.
 *
 * @returns pTime.
 * @param   pTime       The time spec to interpret.
 * @param   pTimespec   Where to store the time as POSIX timespec.
 */
DECLINLINE(struct timespec *) RTTimeSpecGetTimespec(PCRTTIMESPEC pTime, struct timespec *pTimespec)
{
    int64_t i64 = RTTimeSpecGetNano(pTime);
    int32_t i32Nano = (int32_t)(i64 % 1000000000);
    i64 /= 1000000000;
    if (i32Nano < 0)
    {
        i32Nano += 1000000000;
        i64++;
    }
    pTimespec->tv_sec = (time_t)i64;
    pTimespec->tv_nsec = i32Nano;
    return pTimespec;
}

/**
 * Sets the time as POSIX timespec.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   pTimespec   Pointer to the POSIX timespec struct with the new time.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetTimespec(PRTTIMESPEC pTime, const struct timespec *pTimespec)
{
    return RTTimeSpecAddNano(RTTimeSpecSetSeconds(pTime, pTimespec->tv_sec), pTimespec->tv_nsec);
}
#endif /* various ways of detecting struct timespec */



/** The offset of the unix epoch and the base for NT time (in 100ns units).
 * Nt time starts at 1601-01-01 00:00:00. */
#define RTTIME_NT_TIME_OFFSET_UNIX      (116444736000000000LL)


/**
 * Gets the time as NT time.
 *
 * @returns Nt time.
 * @param   pTime       The time spec to interpret.
 */
DECLINLINE(uint64_t) RTTimeSpecGetNtTime(PCRTTIMESPEC pTime)
{
    return pTime->i64NanosecondsRelativeToUnixEpoch / 100
        + RTTIME_NT_TIME_OFFSET_UNIX;
}


/**
 * Sets the time given by Nt time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   u64NtTime   The new time in Nt time.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetNtTime(PRTTIMESPEC pTime, uint64_t u64NtTime)
{
    pTime->i64NanosecondsRelativeToUnixEpoch =
        ((int64_t)u64NtTime - RTTIME_NT_TIME_OFFSET_UNIX) * 100;
    return pTime;
}


#ifdef _FILETIME_
/**
 * Gets the time as NT file time.
 *
 * @returns pFileTime.
 * @param   pTime       The time spec to interpret.
 * @param   pFileTime   Pointer to NT filetime structure.
 */
DECLINLINE(PFILETIME) RTTimeSpecGetNtFileTime(PCRTTIMESPEC pTime, PFILETIME pFileTime)
{
    *((uint64_t *)pFileTime) = RTTimeSpecGetNtTime(pTime);
    return pFileTime;
}

/**
 * Sets the time as NT file time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   pFileTime   Where to store the time as Nt file time.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetNtFileTime(PRTTIMESPEC pTime, const FILETIME *pFileTime)
{
    return RTTimeSpecSetNtTime(pTime, *(const uint64_t *)pFileTime);
}
#endif


/** The offset to the start of DOS time.
 * DOS time starts 1980-01-01 00:00:00.  */
#define RTTIME_OFFSET_DOS_TIME          (315532800000000000LL)


/**
 * Gets the time as seconds relative to the start of dos time.
 *
 * @returns seconds relative to the start of dos time.
 * @param   pTime       The time spec to interpret.
 */
DECLINLINE(int64_t) RTTimeSpecGetDosSeconds(PCRTTIMESPEC pTime)
{
    return (pTime->i64NanosecondsRelativeToUnixEpoch + RTTIME_OFFSET_DOS_TIME)
        / 1000000000;
}


/**
 * Sets the time given by seconds relative to the start of dos time.
 *
 * @returns pTime.
 * @param   pTime       The time spec to modify.
 * @param   i64Seconds  The new time in seconds relative to the start of dos time.
 */
DECLINLINE(PRTTIMESPEC) RTTimeSpecSetDosSeconds(PRTTIMESPEC pTime, int64_t i64Seconds)
{
    pTime->i64NanosecondsRelativeToUnixEpoch = i64Seconds * 1000000000
        - RTTIME_NT_TIME_OFFSET_UNIX;
    return pTime;
}


/**
 * Compare two time specs.
 *
 * @returns true they are equal.
 * @returns false they are not equal.
 * @param   pTime1  The 1st time spec.
 * @param   pTime2  The 2nd time spec.
 */
DECLINLINE(bool) RTTimeSpecIsEqual(PCRTTIMESPEC pTime1, PCRTTIMESPEC pTime2)
{
    return pTime1->i64NanosecondsRelativeToUnixEpoch == pTime2->i64NanosecondsRelativeToUnixEpoch;
}

/**
 * Converts a time spec to a ISO date string.
 *
 * @returns psz on success.
 * @returns NULL on buffer underflow.
 * @param   pTime       The time spec.
 * @param   psz         Where to store the string.
 * @param   cb          The size of the buffer.
 */
RTDECL(char *) RTTimeSpecToString(PCRTTIMESPEC pTime, char *psz, size_t cb);

/** @} */


/**
 * Exploded time.
 */
#pragma pack(1)
typedef struct RTTIME
{
    /** The year number. */
    int32_t     i32Year;
    /** The month of the year (1-12). January is 1. */
    uint8_t     u8Month;
    /** The day of the week (0-6). Monday is 0. */
    uint8_t     u8WeekDay;
    /** The day of the year (1-366). January the 1st is 1. */
    uint16_t    u16YearDay;
    /** The day of the month (1-31). */
    uint8_t     u8MonthDay;
    /** Hour of the day (0-23). */
    uint8_t     u8Hour;
    /** The minute of the hour (0-59). */
    uint8_t     u8Minute;
    /** The second of the minute (0-60).
     * (u32Nanosecond / 1000000) */
    uint8_t     u8Second;
    /** The nanoseconds of the second (0-999999999). */
    uint32_t    u32Nanosecond;
    /** Flags, of the RTTIME_FLAGS_* \#defines. */
    uint32_t    fFlags;
    /** UCT time offset in minutes (-840-840).
     * @remarks The implementation of RTTimeLocal* isn't quite there yet, so this might not be 100% correct. */
    int32_t     offUTC;
} RTTIME;
#pragma pack()
/** Pointer to a exploded time structure. */
typedef RTTIME *PRTTIME;
/** Pointer to a const exploded time structure. */
typedef const RTTIME *PCRTTIME;

/** @name RTTIME::fFlags values.
 * @{ */
/** Set if the time is UTC. If clear the time local time. */
#define RTTIME_FLAGS_TYPE_MASK      3
/** the time is UTC time. */
#define RTTIME_FLAGS_TYPE_UTC       2
/** The time is local time. */
#define RTTIME_FLAGS_TYPE_LOCAL     3

/** Set if the time is local and daylight saving time is in effect.
 * Not bit is not valid if RTTIME_FLAGS_NO_DST_DATA is set. */
#define RTTIME_FLAGS_DST            RT_BIT(4)
/** Set if the time is local and there is no data available on daylight saving time. */
#define RTTIME_FLAGS_NO_DST_DATA    RT_BIT(5)
/** Set if the year is a leap year.
 * This is mutual exclusiv with RTTIME_FLAGS_COMMON_YEAR. */
#define RTTIME_FLAGS_LEAP_YEAR      RT_BIT(6)
/** Set if the year is a common year.
 * This is mutual exclusiv with RTTIME_FLAGS_LEAP_YEAR. */
#define RTTIME_FLAGS_COMMON_YEAR    RT_BIT(7)
/** The mask of valid flags. */
#define RTTIME_FLAGS_MASK           UINT32_C(0xff)
/** @} */


/**
 * Gets the current system time (UTC).
 *
 * @returns pTime.
 * @param   pTime   Where to store the time.
 */
RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime);

/**
 * Explodes a time spec (UTC).
 *
 * @returns pTime.
 * @param   pTime       Where to store the exploded time.
 * @param   pTimeSpec   The time spec to exploded.
 */
RTDECL(PRTTIME) RTTimeExplode(PRTTIME pTime, PCRTTIMESPEC pTimeSpec);

/**
 * Implodes exploded time to a time spec (UTC).
 *
 * @returns pTime on success.
 * @returns NULL if the pTime data is invalid.
 * @param   pTimeSpec   Where to store the imploded UTC time.
 *                      If pTime specifies a time which outside the range, maximum or
 *                      minimum values will be returned.
 * @param   pTime       Pointer to the exploded time to implode.
 *                      The fields u8Month, u8WeekDay and u8MonthDay are not used,
 *                      and all the other fields are expected to be within their
 *                      bounds. Use RTTimeNormalize() to calculate u16YearDay and
 *                      normalize the ranges of the fields.
 */
RTDECL(PRTTIMESPEC) RTTimeImplode(PRTTIMESPEC pTimeSpec, PCRTTIME pTime);

/**
 * Normalizes the fields of a time structure.
 *
 * It is possible to calculate year-day from month/day and vice
 * versa. If you adjust any of of these, make sure to zero the
 * other so you make it clear which of the fields to use. If
 * it's ambiguous, the year-day field is used (and you get
 * assertions in debug builds).
 *
 * All the time fields and the year-day or month/day fields will
 * be adjusted for overflows. (Since all fields are unsigned, there
 * is no underflows.) It is possible to exploit this for simple
 * date math, though the recommended way of doing that to implode
 * the time into a timespec and do the math on that.
 *
 * @returns pTime on success.
 * @returns NULL if the data is invalid.
 *
 * @param   pTime       The time structure to normalize.
 *
 * @remarks This function doesn't work with local time, only with UTC time.
 */
RTDECL(PRTTIME) RTTimeNormalize(PRTTIME pTime);

/**
 * Gets the current local system time.
 *
 * @returns pTime.
 * @param   pTime   Where to store the local time.
 */
RTDECL(PRTTIMESPEC) RTTimeLocalNow(PRTTIMESPEC pTime);

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
RTDECL(int64_t) RTTimeLocalDeltaNano(void);

/**
 * Explodes a time spec to the localized timezone.
 *
 * @returns pTime.
 * @param   pTime       Where to store the exploded time.
 * @param   pTimeSpec   The time spec to exploded (UTC).
 */
RTDECL(PRTTIME) RTTimeLocalExplode(PRTTIME pTime, PCRTTIMESPEC pTimeSpec);

/**
 * Normalizes the fields of a time structure containing local time.
 *
 * See RTTimeNormalize for details.
 *
 * @returns pTime on success.
 * @returns NULL if the data is invalid.
 * @param   pTime       The time structure to normalize.
 */
RTDECL(PRTTIME) RTTimeLocalNormalize(PRTTIME pTime);

/**
 * Converts a time spec to a ISO date string.
 *
 * @returns psz on success.
 * @returns NULL on buffer underflow.
 * @param   pTime       The time. Caller should've normalized this.
 * @param   psz         Where to store the string.
 * @param   cb          The size of the buffer.
 */
RTDECL(char *) RTTimeToString(PCRTTIME pTime, char *psz, size_t cb);

/**
 * Gets the current nanosecond timestamp.
 *
 * @returns nanosecond timestamp.
 */
RTDECL(uint64_t) RTTimeNanoTS(void);

/**
 * Gets the current millisecond timestamp.
 *
 * @returns millisecond timestamp.
 */
RTDECL(uint64_t) RTTimeMilliTS(void);

/**
 * Debugging the time api.
 *
 * @returns the number of 1ns steps which has been applied by RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgSteps(void);

/**
 * Debugging the time api.
 *
 * @returns the number of times the TSC interval expired RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgExpired(void);

/**
 * Debugging the time api.
 *
 * @returns the number of bad previous values encountered by RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgBad(void);

/**
 * Debugging the time api.
 *
 * @returns the number of update races in RTTimeNanoTS().
 */
RTDECL(uint32_t) RTTimeDbgRaces(void);

/** @name RTTimeNanoTS GIP worker functions, for TM.
 * @{ */
/** Pointer to a RTTIMENANOTSDATA structure. */
typedef struct RTTIMENANOTSDATA *PRTTIMENANOTSDATA;

/**
 * Nanosecond timestamp data.
 *
 * This is used to keep track of statistics and callback so IPRT
 * and TM (VirtualBox) can share code.
 *
 * @remark Keep this in sync with the assembly version in timesupA.asm.
 */
typedef struct RTTIMENANOTSDATA
{
    /** Where the previous timestamp is stored.
     * This is maintained to ensure that time doesn't go backwards or anything. */
    uint64_t volatile  *pu64Prev;

    /**
     * Helper function that's used by the assembly routines when something goes bust.
     *
     * @param   pData           Pointer to this structure.
     * @param   u64NanoTS       The calculated nano ts.
     * @param   u64DeltaPrev    The delta relative to the previously returned timestamp.
     * @param   u64PrevNanoTS   The previously returned timestamp (as it was read it).
     */
    DECLCALLBACKMEMBER(void, pfnBad)(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS);

    /**
     * Callback for when rediscovery is required.
     *
     * @returns Nanosecond timestamp.
     * @param   pData           Pointer to this structure.
     */
    DECLCALLBACKMEMBER(uint64_t, pfnRediscover)(PRTTIMENANOTSDATA pData);

    /** Just a dummy alignment member. */
    void               *pvDummy;

    /** Number of 1ns steps because of overshooting the period. */
    uint32_t            c1nsSteps;
    /** The number of times the interval expired (overflow). */
    uint32_t            cExpired;
    /** Number of "bad" previous values. */
    uint32_t            cBadPrev;
    /** The number of update races. */
    uint32_t            cUpdateRaces;
} RTTIMENANOTSDATA;

#ifndef IN_RING3
/**
 * The Ring-3 layout of the RTTIMENANOTSDATA structure.
 */
typedef struct RTTIMENANOTSDATAR3
{
    R3PTRTYPE(uint64_t volatile  *) pu64Prev;
    DECLR3CALLBACKMEMBER(void, pfnBad,(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS));
    DECLR3CALLBACKMEMBER(uint64_t, pfnRediscover,(PRTTIMENANOTSDATA pData));
    RTR3PTR             pvDummy;
    uint32_t            c1nsSteps;
    uint32_t            cExpired;
    uint32_t            cBadPrev;
    uint32_t            cUpdateRaces;
} RTTIMENANOTSDATAR3;
#else
typedef RTTIMENANOTSDATA RTTIMENANOTSDATAR3;
#endif

#ifndef IN_RING0
/**
 * The Ring-3 layout of the RTTIMENANOTSDATA structure.
 */
typedef struct RTTIMENANOTSDATAR0
{
    R0PTRTYPE(uint64_t volatile  *) pu64Prev;
    DECLR0CALLBACKMEMBER(void, pfnBad,(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS));
    DECLR0CALLBACKMEMBER(uint64_t, pfnRediscover,(PRTTIMENANOTSDATA pData));
    RTR0PTR             pvDummy;
    uint32_t            c1nsSteps;
    uint32_t            cExpired;
    uint32_t            cBadPrev;
    uint32_t            cUpdateRaces;
} RTTIMENANOTSDATAR0;
#else
typedef RTTIMENANOTSDATA RTTIMENANOTSDATAR0;
#endif

#ifndef IN_GC
/**
 * The GC layout of the RTTIMENANOTSDATA structure.
 */
typedef struct RTTIMENANOTSDATAGC
{
    GCPTRTYPE(uint64_t volatile  *) pu64Prev;
    DECLGCCALLBACKMEMBER(void, pfnBad,(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS));
    DECLGCCALLBACKMEMBER(uint64_t, pfnRediscover,(PRTTIMENANOTSDATA pData));
    RTGCPTR             pvDummy;
    uint32_t            c1nsSteps;
    uint32_t            cExpired;
    uint32_t            cBadPrev;
    uint32_t            cUpdateRaces;
} RTTIMENANOTSDATAGC;
#else
typedef RTTIMENANOTSDATA RTTIMENANOTSDATAGC;
#endif

/** Internal RTTimeNanoTS worker (assembly). */
typedef DECLCALLBACK(uint64_t) FNTIMENANOTSINTERNAL(PRTTIMENANOTSDATA pData);
/** Pointer to an internal RTTimeNanoTS worker (assembly). */
typedef FNTIMENANOTSINTERNAL *PFNTIMENANOTSINTERNAL;

RTDECL(uint64_t) RTTimeNanoTSLegacySync(PRTTIMENANOTSDATA pData);
RTDECL(uint64_t) RTTimeNanoTSLegacyAsync(PRTTIMENANOTSDATA pData);
RTDECL(uint64_t) RTTimeNanoTSLFenceSync(PRTTIMENANOTSDATA pData);
RTDECL(uint64_t) RTTimeNanoTSLFenceAsync(PRTTIMENANOTSDATA pData);
/** @} */


/**
 * Gets the current nanosecond timestamp.
 *
 * This differs from RTTimeNanoTS in that it will use system APIs and not do any
 * resolution or performance optimizations.
 *
 * @returns nanosecond timestamp.
 */
RTDECL(uint64_t) RTTimeSystemNanoTS(void);

/**
 * Gets the current millisecond timestamp.
 *
 * This differs from RTTimeNanoTS in that it will use system APIs and not do any
 * resolution or performance optimizations.
 *
 * @returns millisecond timestamp.
 */
RTDECL(uint64_t) RTTimeSystemMilliTS(void);

/**
 * Get the nanosecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint64_t)  RTTimeProgramNanoTS(void);

/**
 * Get the microsecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint64_t)  RTTimeProgramMicroTS(void);

/**
 * Get the millisecond timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint64_t)  RTTimeProgramMilliTS(void);

/**
 * Get the second timestamp relative to program startup.
 *
 * @returns Timestamp relative to program startup.
 */
RTDECL(uint32_t)  RTTimeProgramSecTS(void);

/**
 * Get the RTTimeNanoTS() of when the program started.
 *
 * @returns Program startup timestamp.
 */
RTDECL(uint64_t) RTTimeProgramStartNanoTS(void);

/** @} */

__END_DECLS

#endif

