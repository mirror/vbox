/* $Id$ */
/** @file
 * IPRT - RTTimeSpec and PRTTIME tests.
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
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/**
 * Format the time into a string using a static buffer.
 */
char *ToString(PRTTIME pTime)
{
    static char szBuf[128];
    RTStrPrintf(szBuf, sizeof(szBuf), "%04d-%02d-%02dT%02u:%02u:%02u.%09u [YD%u WD%u UO%d F%#x]",
                pTime->i32Year,
                pTime->u8Month,
                pTime->u8MonthDay,
                pTime->u8Hour,
                pTime->u8Minute,
                pTime->u8Second,
                pTime->u32Nanosecond,
                pTime->u16YearDay,
                pTime->u8WeekDay,
                pTime->offUTC,
                pTime->fFlags);
    return szBuf;
}

#define CHECK_NZ(expr)  do { if (!(expr)) { RTPrintf("tstTimeSpec: FAILURE at line %d: %#x\n", __LINE__, #expr); return 1; } } while (0)

#define TEST_NS(ns) do {\
        CHECK_NZ(RTTimeExplode(&T1, RTTimeSpecSetNano(&Ts1, ns))); \
        RTPrintf("tstTimeSpec: %RI64 ns - %s\n", ns, ToString(&T1)); \
        CHECK_NZ(RTTimeImplode(&Ts2, &T1)); \
        if (!RTTimeSpecIsEqual(&Ts2, &Ts1)) \
        { \
            RTPrintf("tstTimeSpec: FAILURE - %RI64 != %RI64\n", RTTimeSpecGetNano(&Ts2), RTTimeSpecGetNano(&Ts1)); \
            RTPrintf("             line no %d\n", __LINE__); \
            cErrors++; \
        } \
    } while (0)

#define TEST_SEC(sec) do {\
        CHECK_NZ(RTTimeExplode(&T1, RTTimeSpecSetSeconds(&Ts1, sec))); \
        RTPrintf("tstTimeSpec: %RI64 sec - %s\n", sec, ToString(&T1)); \
        CHECK_NZ(RTTimeImplode(&Ts2, &T1)); \
        if (!RTTimeSpecIsEqual(&Ts2, &Ts1)) \
        { \
            RTPrintf("tstTimeSpec: FAILURE - %RI64 != %RI64\n", RTTimeSpecGetNano(&Ts2), RTTimeSpecGetNano(&Ts1)); \
            RTPrintf("             line no %d\n", __LINE__); \
            cErrors++; \
        } \
    } while (0)

#define CHECK_TIME(pTime, _i32Year, _u8Month, _u8MonthDay, _u8Hour, _u8Minute, _u8Second, _u32Nanosecond, _u16YearDay, _u8WeekDay, _offUTC, _fFlags)\
    do { \
        if (    (pTime)->i32Year != (_i32Year) \
            ||  (pTime)->u8Month != (_u8Month) \
            ||  (pTime)->u8WeekDay != (_u8WeekDay) \
            ||  (pTime)->u16YearDay != (_u16YearDay) \
            ||  (pTime)->u8MonthDay != (_u8MonthDay) \
            ||  (pTime)->u8Hour != (_u8Hour) \
            ||  (pTime)->u8Minute != (_u8Minute) \
            ||  (pTime)->u8Second != (_u8Second) \
            ||  (pTime)->u32Nanosecond != (_u32Nanosecond) \
            ||  (pTime)->offUTC != (_offUTC) \
            ||  (pTime)->fFlags != (_fFlags) \
            ) \
        { \
            RTPrintf("tstTimeSpec: FAILURE - %s\n" \
                     "                    != %04d-%02d-%02dT%02u-%02u-%02u.%09u [YD%u WD%u UO%d F%#x]\n", \
                     ToString(pTime), (_i32Year), (_u8Month), (_u8MonthDay), (_u8Hour), (_u8Minute), \
                     (_u8Second), (_u32Nanosecond), (_u16YearDay), (_u8WeekDay), (_offUTC), (_fFlags)); \
            RTPrintf("             line no %d\n", __LINE__); \
            cErrors++; \
        } \
        else \
            RTPrintf("          => %s\n", ToString(pTime)); \
    } while (0)

#define SET_TIME(pTime, _i32Year, _u8Month, _u8MonthDay, _u8Hour, _u8Minute, _u8Second, _u32Nanosecond, _u16YearDay, _u8WeekDay, _offUTC, _fFlags)\
    do { \
        (pTime)->i32Year = (_i32Year); \
        (pTime)->u8Month = (_u8Month); \
        (pTime)->u8WeekDay = (_u8WeekDay); \
        (pTime)->u16YearDay = (_u16YearDay); \
        (pTime)->u8MonthDay = (_u8MonthDay); \
        (pTime)->u8Hour = (_u8Hour); \
        (pTime)->u8Minute = (_u8Minute); \
        (pTime)->u8Second = (_u8Second); \
        (pTime)->u32Nanosecond = (_u32Nanosecond); \
        (pTime)->offUTC = (_offUTC); \
        (pTime)->fFlags = (_fFlags); \
        RTPrintf("tstTimeSpec: %s\n", ToString(pTime)); \
    } while (0)


int main()
{
    unsigned    cErrors = 0;
    RTTIMESPEC  Now;
    RTTIMESPEC  Ts1;
    RTTIMESPEC  Ts2;
    RTTIME      T1;
    RTTIME      T2;

    /*
     * Simple test with current time.
     */
    CHECK_NZ(RTTimeNow(&Now));
    CHECK_NZ(RTTimeExplode(&T1, &Now));
    RTPrintf("tstTimeSpec: %RI64 ns - %s\n", RTTimeSpecGetNano(&Now), ToString(&T1));
    CHECK_NZ(RTTimeImplode(&Ts1, &T1));
    if (!RTTimeSpecIsEqual(&Ts1, &Now))
    {
        RTPrintf("tstTimeSpec: FAILURE - %RI64 != %RI64\n", RTTimeSpecGetNano(&Ts1), RTTimeSpecGetNano(&Now));
        cErrors++;
    }

    /*
     * Simple test with current local time.
     */
    CHECK_NZ(RTTimeLocalNow(&Now));
    CHECK_NZ(RTTimeExplode(&T1, &Now));
    RTPrintf("tstTimeSpec: %RI64 ns - %s\n", RTTimeSpecGetNano(&Now), ToString(&T1));
    CHECK_NZ(RTTimeImplode(&Ts1, &T1));
    if (!RTTimeSpecIsEqual(&Ts1, &Now))
    {
        RTPrintf("tstTimeSpec: FAILURE - %RI64 != %RI64\n", RTTimeSpecGetNano(&Ts1), RTTimeSpecGetNano(&Now));
        cErrors++;
    }

    /*
     * Some simple tests with fixed dates (just checking for smoke).
     */
    TEST_NS(INT64_C(0));
    CHECK_TIME(&T1, 1970,01,01, 00,00,00,        0,   1, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);
    TEST_NS(INT64_C(86400000000000));
    CHECK_TIME(&T1, 1970,01,02, 00,00,00,        0,   2, 4, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    TEST_NS(INT64_C(1));
    CHECK_TIME(&T1, 1970,01,01, 00,00,00,        1,   1, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);
    TEST_NS(INT64_C(-1));
    CHECK_TIME(&T1, 1969,12,31, 23,59,59,999999999, 365, 2, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    /*
     * Test the limits.
     */
    TEST_NS(INT64_MAX);
    TEST_NS(INT64_MIN);
    TEST_SEC(1095379198);
    CHECK_TIME(&T1, 2004, 9,16, 23,59,58,        0, 260, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    TEST_SEC(1095379199);
    CHECK_TIME(&T1, 2004, 9,16, 23,59,59,        0, 260, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    TEST_SEC(1095379200);
    CHECK_TIME(&T1, 2004, 9,17, 00,00,00,        0, 261, 4, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    TEST_SEC(1095379201);
    CHECK_TIME(&T1, 2004, 9,17, 00,00,01,        0, 261, 4, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);


    /*
     * Test normalization (UTC).
     */
    /* simple */
    CHECK_NZ(RTTimeNow(&Now));
    CHECK_NZ(RTTimeExplode(&T1, &Now));
    T2 = T1;
    CHECK_NZ(RTTimeNormalize(&T1));
    if (memcmp(&T1, &T2, sizeof(T1)))
    {
        RTPrintf("tstTimeSpec: FAILURE - simple normalization failed\n");
        cErrors++;
    }
    CHECK_NZ(RTTimeImplode(&Ts1, &T1));
    CHECK_NZ(RTTimeSpecIsEqual(&Ts1, &Now));

    /* a few partial dates. */
    memset(&T1, 0, sizeof(T1));
    SET_TIME(  &T1, 1970,01,01, 00,00,00,        0,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1970,01,01, 00,00,00,        0,   1, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1970,00,00, 00,00,00,        1,   1, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1970,01,01, 00,00,00,        1,   1, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 2007,12,06, 02,15,23,        1,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2007,12,06, 02,15,23,        1, 340, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1968,01,30, 00,19,24,        5,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1968,01,30, 00,19,24,        5,  30, 1, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);

    SET_TIME(  &T1, 1969,01,31, 00, 9, 2,        7,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,01,31, 00, 9, 2,        7,  31, 4, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,03,31, 00, 9, 2,        7,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,03,31, 00, 9, 2,        7,  90, 0, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,12,31, 00,00,00,        9,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,31, 00,00,00,        9, 365, 2, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,12,30, 00,00,00,       30,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,30, 00,00,00,       30, 364, 1, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,00,00, 00,00,00,       30, 363, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,29, 00,00,00,       30, 363, 0, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,00,00, 00,00,00,       30, 362, 6, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,28, 00,00,00,       30, 362, 6, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,12,27, 00,00,00,       30,   0, 5, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,27, 00,00,00,       30, 361, 5, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,00,00, 00,00,00,       30, 360, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,26, 00,00,00,       30, 360, 4, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,12,25, 00,00,00,       12,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,25, 00,00,00,       12, 359, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 1969,12,24, 00,00,00,       16,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1969,12,24, 00,00,00,       16, 358, 2, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    /* outside the year table range */
    SET_TIME(  &T1, 1200,01,30, 00,00,00,        2,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1200,01,30, 00,00,00,        2,  30, 6, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);

    SET_TIME(  &T1, 2555,11,29, 00,00,00,        2,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2555,11,29, 00,00,00,        2, 333, 5, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 2555,00,00, 00,00,00,        3, 333, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2555,11,29, 00,00,00,        3, 333, 5, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    /* time overflow */
    SET_TIME(  &T1, 1969,12,30, 255,255,255, UINT32_MAX, 364, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 1970,01, 9, 19,19,19,294967295,   9, 4, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    /* date overflow */
    SET_TIME(  &T1, 2007,11,36, 02,15,23,        1,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2007,12,06, 02,15,23,        1, 340, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 2007,10,67, 02,15,23,        1,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2007,12,06, 02,15,23,        1, 340, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 2007,10,98, 02,15,23,        1,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2008,01,06, 02,15,23,        1,   6, 6, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);

    SET_TIME(  &T1, 2006,24,06, 02,15,23,        1,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2007,12,06, 02,15,23,        1, 340, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    SET_TIME(  &T1, 2003,60,37, 02,15,23,        1,   0, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2008,01,06, 02,15,23,        1,   6, 6, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);

    SET_TIME(  &T1, 2003,00,00, 02,15,23,        1,1801, 0, 0, 0);
    CHECK_NZ(RTTimeNormalize(&T1));
    CHECK_TIME(&T1, 2007,12,06, 02,15,23,        1, 340, 3, 0, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    /*
     * Summary
     */
    if (!cErrors)
        RTPrintf("tstTimeSpec: SUCCESS\n");
    else
        RTPrintf("tstTimeSpec: FAILURE - %d errors\n", cErrors);
    return cErrors ? 1 : 0;
}

