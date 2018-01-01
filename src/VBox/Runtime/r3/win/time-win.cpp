/* $Id$ */
/** @file
 * IPRT - Time, Windows.
 */

/*
 * Copyright (C) 2006-2018 Oracle Corporation
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
#include <iprt/nt/nt-and-windows.h>

#include <iprt/time.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/time.h"
#include "internal-r3-win.h"

/*
 * Note! The selected time source be the exact same one as we use in kernel land!
 */
//#define USE_TICK_COUNT
//#define USE_PERFORMANCE_COUNTER
//# define USE_FILE_TIME
//#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
# define USE_INTERRUPT_TIME
//#else
//# define USE_TICK_COUNT
//#endif



DECLINLINE(uint64_t) rtTimeGetSystemNanoTS(void)
{
#if defined USE_TICK_COUNT
    /*
     * This would work if it didn't flip over every 49 (or so) days.
     */
    return (uint64_t)GetTickCount() * RT_NS_1MS_64;

#elif defined USE_PERFORMANCE_COUNTER
    /*
     * Slow and not derived from InterruptTime.
     */
    static LARGE_INTEGER    llFreq;
    static unsigned         uMult;
    if (!llFreq.QuadPart)
    {
        if (!QueryPerformanceFrequency(&llFreq))
            return (uint64_t)GetTickCount() * RT_NS_1MS_64;
        llFreq.QuadPart /=    1000;
        uMult            = 1000000;     /* no math genius, but this seemed to help avoiding floating point. */
    }

    LARGE_INTEGER   ll;
    if (QueryPerformanceCounter(&ll))
        return (ll.QuadPart * uMult) / llFreq.QuadPart;
    return (uint64_t)GetTickCount() * RT_NS_1MS_64;

#elif defined USE_FILE_TIME
    /*
     * This is SystemTime not InterruptTime.
     */
    uint64_t u64; /* manual say larger integer, should be safe to assume it's the same. */
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return u64 * 100;

#elif defined USE_INTERRUPT_TIME
    /*
     * Use interrupt time if we can (not possible on NT 3.1).
     * Note! We cannot entirely depend on g_enmWinVer here as we're likely to
     *       get called before IPRT is initialized.  Ditto g_hModNtDll.
     */
    static PFNRTLGETINTERRUPTTIMEPRECISE    s_pfnRtlGetInterruptTimePrecise = NULL;
    static int volatile                     s_iCanUseUserSharedData         = -1;
    int                                     iCanUseUserSharedData           = s_iCanUseUserSharedData;
    if (iCanUseUserSharedData != -1)
    { /* likely */ }
    else
    {
        /* We may be called before g_enmWinVer has been initialized. */
        if (g_enmWinVer != kRTWinOSType_UNKNOWN)
            iCanUseUserSharedData = g_enmWinVer > kRTWinOSType_NT310;
        else
        {
            DWORD dwVer = GetVersion();
            iCanUseUserSharedData = (dwVer & 0xff) != 3 || ((dwVer >> 8) & 0xff) >= 50;
        }
        if (iCanUseUserSharedData != 0)
        {
            FARPROC pfn = GetProcAddress(g_hModNtDll ? g_hModNtDll : GetModuleHandleW(L"ntdll"), "RtlGetInterruptTimePrecise");
            if (pfn != NULL)
            {
                ASMAtomicWritePtr(&s_pfnRtlGetInterruptTimePrecise, pfn);
                iCanUseUserSharedData = 42;
            }
        }
        s_iCanUseUserSharedData = iCanUseUserSharedData;
    }

    if (iCanUseUserSharedData != 0)
    {
        LARGE_INTEGER Time;
        if (iCanUseUserSharedData == 42)
        {
            uint64_t iIgnored;
            Time.QuadPart = s_pfnRtlGetInterruptTimePrecise(&iIgnored);
        }
        else
        {
            PKUSER_SHARED_DATA pUserSharedData = (PKUSER_SHARED_DATA)MM_SHARED_USER_DATA_VA;
            do
            {
                Time.HighPart = pUserSharedData->InterruptTime.High1Time;
                Time.LowPart  = pUserSharedData->InterruptTime.LowPart;
            } while (pUserSharedData->InterruptTime.High2Time != Time.HighPart);
        }

        return (uint64_t)Time.QuadPart * 100;
    }

    return (uint64_t)GetTickCount() * RT_NS_1MS_64;

#else
# error "Must select a method bright guy!"
#endif
}


RTDECL(uint64_t) RTTimeSystemNanoTS(void)
{
    return rtTimeGetSystemNanoTS();
}


RTDECL(uint64_t) RTTimeSystemMilliTS(void)
{
    return rtTimeGetSystemNanoTS() / RT_NS_1MS;
}


RTDECL(PRTTIMESPEC) RTTimeNow(PRTTIMESPEC pTime)
{
    uint64_t u64;
    AssertCompile(sizeof(u64) == sizeof(FILETIME));
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return RTTimeSpecSetNtTime(pTime, u64);
}


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


RTDECL(int64_t) RTTimeLocalDeltaNano(void)
{
    /*
     * UTC = local + Tzi.Bias;
     * The bias is given in minutes.
     */
    TIME_ZONE_INFORMATION Tzi;
    Tzi.Bias = 0;
    if (GetTimeZoneInformation(&Tzi) != TIME_ZONE_ID_INVALID)
        return -(int64_t)Tzi.Bias * 60 * RT_NS_1SEC_64;
    return 0;
}

