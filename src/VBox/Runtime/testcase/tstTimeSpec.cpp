/* $Id$ */
/** @file
 * InnoTek Portable Runtime - RTTimeSpec and PRTTIME tests.
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
#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/string.h>


/**
 * Format the time into a string using a static buffer.
 */
char *ToString(PRTTIME pTime)
{
    static char szBuf[128];
    RTStrPrintf(szBuf, sizeof(szBuf), "%04d-%02d-%02dT%02u-%02u-%02u.%09u [YD%u WD%u F%#x]",
                pTime->i32Year,
                pTime->u8Month,
                pTime->u8MonthDay,
                pTime->u8Hour,
                pTime->u8Minute,
                pTime->u8Second,
                pTime->u32Nanosecond,
                pTime->u16YearDay,
                pTime->u8WeekDay,
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
            return 1; \
        } \
    } while (0)

#define TEST_SEC(sec) do {\
        CHECK_NZ(RTTimeExplode(&T1, RTTimeSpecSetSeconds(&Ts1, sec))); \
        RTPrintf("tstTimeSpec: %RI64 sec - %s\n", sec, ToString(&T1)); \
        CHECK_NZ(RTTimeImplode(&Ts2, &T1)); \
        if (!RTTimeSpecIsEqual(&Ts2, &Ts1)) \
        { \
            RTPrintf("tstTimeSpec: FAILURE - %RI64 != %RI64\n", RTTimeSpecGetNano(&Ts2), RTTimeSpecGetNano(&Ts1)); \
            return 1; \
        } \
    } while (0)

#define CHECK_TIME(pTime, _i32Year, _u8Month, _u8MonthDay, _u8Hour, _u8Minute, _u8Second, _u32Nanosecond, _u16YearDay, _u8WeekDay, _fFlags)\
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
            ||  (pTime)->fFlags != (_fFlags) \
            ) \
        { \
            RTPrintf("tstTimeSpec: FAILURE - %s\n" \
                     "                    != %04d-%02d-%02dT%02u-%02u-%02u.%09u [YD%u WD%u F%#x]\n", \
                     ToString(pTime), (_i32Year), (_u8Month), (_u8MonthDay), (_u8Hour), (_u8Minute), \
                     (_u8Second), (_u32Nanosecond), (_u16YearDay), (_u8WeekDay), (_fFlags)); \
            return 1; \
        } \
    } while (0)


int main()
{
    RTTIMESPEC  Now;
    RTTIMESPEC  Ts1;
    RTTIMESPEC  Ts2;
    RTTIME      T1;
    //RTTIME      T2;

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
        return 1;
    }

    /*
     * Some simple tests with fixed dates (just checking for smoke).
     */
    TEST_NS(INT64_C(0));
    CHECK_TIME(&T1, 1970,01,01, 00,00,00,        0,   1, 3, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);
    TEST_NS(INT64_C(86400000000000));
    CHECK_TIME(&T1, 1970,01,02, 00,00,00,        0,   2, 4, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    TEST_NS(INT64_C(1));
    CHECK_TIME(&T1, 1970,01,01, 00,00,00,        1,   1, 3, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);
    TEST_NS(INT64_C(-1));
    CHECK_TIME(&T1, 1969,12,31, 23,59,59,999999999, 365, 2, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_COMMON_YEAR);

    /*
     * Test the limits.
     */
    TEST_NS(INT64_MAX);
    TEST_NS(INT64_MIN);
    TEST_SEC(1095379198);
    CHECK_TIME(&T1, 2004, 9,16, 23,59,58,        0, 260, 3, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    TEST_SEC(1095379199);
    CHECK_TIME(&T1, 2004, 9,16, 23,59,59,        0, 260, 3, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    TEST_SEC(1095379200);
    CHECK_TIME(&T1, 2004, 9,17, 00,00,00,        0, 261, 4, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    TEST_SEC(1095379201);
    CHECK_TIME(&T1, 2004, 9,17, 00,00,01,        0, 261, 4, RTTIME_FLAGS_TYPE_UTC | RTTIME_FLAGS_LEAP_YEAR);
    RTPrintf("tstTimeSpec: SUCCESS\n");
    return 0;
}

