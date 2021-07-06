/* $Id$ */
/** @file
 * IPRT - EFI time conversion helpers.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_TIME
#include <iprt/efi.h>

#include <iprt/cdefs.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/

RTDECL(PRTTIMESPEC) RTEfiTimeToTimeSpec(PRTTIMESPEC pTimeSpec, PCEFI_TIME pEfiTime)
{
    RTTIME Time; RT_ZERO(Time);

    Time.i32Year       = pEfiTime->u16Year + 1900;
    Time.u8Month       = pEfiTime->u8Month;
    Time.u8MonthDay    = pEfiTime->u8Day;
    Time.u8Hour        = pEfiTime->u8Hour;
    Time.u8Minute      = pEfiTime->u8Minute;
    Time.u8Second      = pEfiTime->u8Second;
    Time.u32Nanosecond = pEfiTime->u32Nanosecond;
    if (pEfiTime->iTimezone != EFI_TIME_TIMEZONE_UNSPECIFIED)
        Time.offUTC = pEfiTime->iTimezone;
    Time.fFlags = RTTIME_FLAGS_TYPE_LOCAL;
    if (RTTimeIsLeapYear(Time.i32Year))
        Time.fFlags |= RTTIME_FLAGS_LEAP_YEAR;
    else
        Time.fFlags |= RTTIME_FLAGS_COMMON_YEAR;
    if (pEfiTime->u8Daylight & EFI_TIME_DAYLIGHT_ADJUST)
    {
        if (pEfiTime->u8Daylight & EFI_TIME_DAYLIGHT_INDST)
            Time.fFlags |= RTTIME_FLAGS_DST;
    }
    else
        Time.fFlags |= RTTIME_FLAGS_NO_DST_DATA;

    if (!RTTimeLocalNormalize(&Time))
        return NULL;

    return RTTimeImplode(pTimeSpec, &Time);
}


RTDECL(PEFI_TIME) RTEfiTimeToTimeSpec(PEFI_TIME pEfiTime, PCRTTIMESPEC pTimeSpec)
{
    RTTIME Time; RT_ZERO(Time);
    if (!RTTimeExplode(&Time, pTimeSpec))
        return NULL;

    RT_ZERO(*pEfiTime);
    pEfiTime->u16Year       =   Time.i32Year < 1900
                              ? 0
                              : Time.i32Year - 1900;
    pEfiTime->u8Month       = Time.u8Month;
    pEfiTime->u8Day         = Time.u8MonthDay;
    pEfiTime->u8Hour        = Time.u8Hour;
    pEfiTime->u8Minute      = Time.u8Minute;
    pEfiTime->u8Second      = Time.u8Second;
    pEfiTime->u32Nanosecond = Time.u32Nanosecond;
    if ((Time.fFlags & RTTIME_FLAGS_TYPE_MASK) == RTTIME_FLAGS_TYPE_LOCAL)
        pEfiTime->iTimezone = Time.offUTC;
    else
        pEfiTime->iTimezone = EFI_TIME_TIMEZONE_UNSPECIFIED;
    if (!(Time.fFlags & RTTIME_FLAGS_NO_DST_DATA))
    {
        pEfiTime->u8Daylight = EFI_TIME_DAYLIGHT_ADJUST;
        if (Time.fFlags & RTTIME_FLAGS_DST)
            pEfiTime->u8Daylight |= EFI_TIME_DAYLIGHT_INDST;
    }
    return pEfiTime;
}

