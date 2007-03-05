/* $Id$ */
/** @file
 * InnoTek Portable Runtime Testcase - Simple RTTime tests.
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
#ifdef __WIN__
# include <Windows.h>

#elif defined __L4__

#else /* posix */
# include <sys/time.h>
#endif

#include <iprt/time.h>
#include <iprt/stream.h>
#include <iprt/runtime.h>
#include <iprt/thread.h>
#include <VBox/sup.h>

DECLINLINE(uint64_t) OSNanoTS(void)
{
#ifdef __WIN__
    uint64_t u64; /* manual say larger integer, should be safe to assume it's the same. */
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return u64 * 100;

#elif defined __L4__
    /** @todo fix a different timesource on l4. */
    return RTTimeNanoTS();

#else /* posix */

    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (uint64_t)tv.tv_sec  * (uint64_t)(1000 * 1000 * 1000)
         + (uint64_t)(tv.tv_usec * 1000);
#endif
}



int main()
{
    unsigned cErrors = 0;
    int i;
    RTR3Init();
SUPInit();
    RTPrintf("tstTime: TESTING...\n");

    /*
     * RTNanoTimeTS() shall never return something which
     * is less or equal to the return of the previous call.
     */

    OSNanoTS(); RTTimeNanoTS(); RTThreadYield();
    uint64_t u64RTStartTS = RTTimeNanoTS();
    uint64_t u64OSStartTS = OSNanoTS();

    uint64_t u64Prev = RTTimeNanoTS();
    for (i = 0; i < 100*_1M; i++)
    {
        uint64_t u64 = RTTimeNanoTS();
        if (u64 <= u64Prev)
        {
            /** @todo wrapping detection. */
            RTPrintf("tstTime: error: i=%#010x u64=%#llx u64Prev=%#llx (1)\n", i, u64, u64Prev);
            cErrors++;
            RTThreadYield();
            u64 = RTTimeNanoTS();
        }
        else if (u64 - u64Prev > 1000000000 /* 1sec */)
        {
            RTPrintf("tstTime: error: i=%#010x u64=%#llx u64Prev=%#llx delta=%lld\n", i, u64, u64Prev, u64 - u64Prev);
            cErrors++;
            RTThreadYield();
            u64 = RTTimeNanoTS();
        }
        if (!(i & (_1M*2 - 1)))
        {
            RTPrintf("tstTime: i=%#010x u64=%#llx u64Prev=%#llx delta=%lld\n", i, u64, u64Prev, u64 - u64Prev);
            RTThreadYield();
            u64 = RTTimeNanoTS();
        }
        u64Prev = u64;
    }

    OSNanoTS(); RTTimeNanoTS(); RTThreadYield();
    uint64_t u64RTElapsedTS = RTTimeNanoTS();
    uint64_t u64OSElapsedTS = OSNanoTS();
    u64RTElapsedTS -= u64RTStartTS;
    u64OSElapsedTS -= u64OSStartTS;
    int64_t i64Diff = u64OSElapsedTS >= u64RTElapsedTS ? u64OSElapsedTS - u64RTElapsedTS : u64RTElapsedTS - u64OSElapsedTS;
    if (i64Diff > (int64_t)(u64OSElapsedTS / 1000))
    {
        RTPrintf("tstTime: error: total time differs too much! u64OSElapsedTS=%#llx u64RTElapsedTS=%#llx delta=%lld\n",
                 u64OSElapsedTS, u64RTElapsedTS, u64OSElapsedTS - u64RTElapsedTS);
        cErrors++;
    }
    else
        RTPrintf("tstTime: total time difference: u64OSElapsedTS=%#llx u64RTElapsedTS=%#llx delta=%lld\n",
                 u64OSElapsedTS, u64RTElapsedTS, u64OSElapsedTS - u64RTElapsedTS);

    RTPrintf("RTTime1nsSteps -> %u (%d ppt)\n", RTTime1nsSteps(), (RTTime1nsSteps() * 1000) / i);
    if (!cErrors)
        RTPrintf("tstTime: SUCCESS\n");
    else
        RTPrintf("tstTime: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
