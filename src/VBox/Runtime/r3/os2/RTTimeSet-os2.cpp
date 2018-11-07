/* $Id$ */
/** @file
 * IPRT - RTTimeSet, OS/2.
 */

/*
 * Copyright (c) 2018 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_TIME
#define INCL_DOSDATETIME
#include <os2.h>

#include <iprt/time.h>
#include "internal/iprt.h"

#include <iprt/err.h>


RTDECL(int) RTTimeSet(PCRTTIMESPEC pTime)
{
    /*
     * Convert to local time and explode it, keeping the distance
     * between UTC and local.
     */
    int64_t    cNsLocalDelta = RTTimeLocalDeltaNanoFor(pTime);
    RTTIMESPEC TimeLocal     = *pTime;
    RTTIME     Exploded;
    if (RTTimeExplode(&Exploded, RTTimeSpecAddNano(&TimeLocal, cNsLocalDelta))))
    {
        /*
         * Fill in the OS/2 structure and make the call.
         */
        DATETIME DateTime;
        DateTime.hour       = Exploded.u8Hour;
        DateTime.minutes    = Exploded.u8Minute;
        DateTime.seconds    = Exploded.u8Second;
        DateTime.hundredths = (uint8_t)(Exploded.u32Nanosecond / (RT_NS_1SEC_64 / 100));
        DateTime.day        = Exploded.u8MonthDay;
        DateTime.month      = Exploded.u8Month;
        DateTime.year       = (uint16_t)Exploded.i32Year;
        DateTime.weekday    = Exploded.u8WeekDay;

        /* Minutes from UTC.  http://www.edm2.com/os2api/Dos/DosSetDateTime.html says
           that timezones west of UTC should have a positive value.  The kernel fails
           the call if we're more than +/-780 min (13h) distant, so clamp it in
           case of bogus TZ values. */
        DateTime.timezone   = (int16_t)(-cNsLocalDelta / RT_NS_1MIN);
        if (DateTime.timezone > 780)
            DateTime.timezone = 780;
        else if (DateTime.timezone < -780)
            DateTime.timezone = -780;

        APIRET rc = DosSetDateTime(&DateTime);
        if (rc == NO_ERROR)
            return VINF_SUCCESS;
        AssertRC(rc);
        return RTErrConvertFromOS2(rc);
    }
    return VERR_INVALID_PARAMETER;
}

