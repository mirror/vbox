/* $Id$ */
/** @file
 * innotek Portable Runtime testcase - profile some of the important functions.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/runtime.h>
#include <iprt/time.h>
#include <iprt/log.h>
#include <iprt/stream.h>
#include <iprt/thread.h>
#include <iprt/asm.h>


void PrintResult(uint64_t u64Ticks, uint64_t u64MaxTicks, uint64_t u64MinTicks, unsigned cTimes, const char *pszOperation)
{
    RTPrintf("tstPrfRT: %-32s %5lld / %5lld / %5lld ticks per call (%u calls %lld ticks)\n",
             pszOperation, u64MinTicks, u64Ticks / (uint64_t)cTimes, u64MaxTicks, cTimes, u64Ticks);
}

#define ITERATE(preexpr, expr, postexpr, cIterations) \
    for (i = 0, u64TotalTS = 0, u64MinTS = ~0, u64MaxTS = 0; i < (cIterations); i++) \
    { \
        { preexpr } \
        uint64_t u64StartTS = ASMReadTSC(); \
        { expr } \
        uint64_t u64ElapsedTS = ASMReadTSC() - u64StartTS; \
        { postexpr } \
        if (u64ElapsedTS > u64MinTS * 32) \
        { \
            i--; \
            continue; \
        } \
        if (u64ElapsedTS < u64MinTS) \
            u64MinTS = u64ElapsedTS; \
        if (u64ElapsedTS > u64MaxTS) \
            u64MaxTS = u64ElapsedTS; \
        u64TotalTS += u64ElapsedTS; \
    }

int main()
{
    uint64_t    u64TotalTS;
    uint64_t    u64MinTS;
    uint64_t    u64MaxTS;
    unsigned    i;

    RTR3Init();
    RTPrintf("tstPrfRT: TESTING...\n");

    /*
     * RTTimeNanoTS, RTTimeProgramNanoTS, RTTimeMilliTS, and RTTimeProgramMilliTS.
     */
    ITERATE(, RTTimeNanoTS();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTTimeNanoTS");

    ITERATE(, RTTimeProgramNanoTS();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTTimeProgramNanoTS");

    ITERATE(, RTTimeMilliTS();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTTimeMilliTS");

    ITERATE(, RTTimeProgramMilliTS();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTTimeProgramMilliTS");

    /*
     * RTTimeNow
     */
    RTTIMESPEC Time;
    ITERATE(, RTTimeNow(&Time);, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTTimeNow");

    /*
     * RTLogDefaultInstance()
     */
    ITERATE(, RTLogDefaultInstance();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTLogDefaultInstance");

    /*
     * RTThreadSelf and RTThreadNativeSelf
     */
    ITERATE(, RTThreadSelf();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTThreadSelf");

    ITERATE(, RTThreadNativeSelf();, , 1000000);
    PrintResult(u64TotalTS, u64MaxTS, u64MinTS, i, "RTThreadNativeSelf");

    RTPrintf("tstPrtRT: DONE\n");
    return 0;
}
