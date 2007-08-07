/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Simple RTTime test.
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
#ifdef RT_OS_WINDOWS
# include <Windows.h>

#elif defined RT_OS_L4

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
#ifdef RT_OS_WINDOWS
    uint64_t u64; /* manual say larger integer, should be safe to assume it's the same. */
    GetSystemTimeAsFileTime((LPFILETIME)&u64);
    return u64 * 100;

#elif defined RT_OS_L4
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
    RTPrintf("tstTime-2: TESTING...\n");

    /*
     * RTNanoTimeTS() shall never return something which
     * is less or equal to the return of the previous call.
     */

    OSNanoTS(); RTTimeNanoTS(); RTThreadYield();
    uint64_t u64RTStartTS = RTTimeNanoTS();
    uint64_t u64OSStartTS = OSNanoTS();
#define NUMBER_OF_CALLS (100*_1M)

    for (i = 0; i < NUMBER_OF_CALLS; i++)
        RTTimeNanoTS();

    uint64_t u64RTElapsedTS = RTTimeNanoTS();
    uint64_t u64OSElapsedTS = OSNanoTS();
    u64RTElapsedTS -= u64RTStartTS;
    u64OSElapsedTS -= u64OSStartTS;
    int64_t i64Diff = u64OSElapsedTS >= u64RTElapsedTS ? u64OSElapsedTS - u64RTElapsedTS : u64RTElapsedTS - u64OSElapsedTS;
    if (i64Diff > (int64_t)(u64OSElapsedTS / 1000))
    {
        RTPrintf("tstTime-2: error: total time differs too much! u64OSElapsedTS=%#llx u64RTElapsedTS=%#llx delta=%lld\n",
                 u64OSElapsedTS, u64RTElapsedTS, u64OSElapsedTS - u64RTElapsedTS);
        cErrors++;
    }
    else
        RTPrintf("tstTime-2: total time difference: u64OSElapsedTS=%#llx u64RTElapsedTS=%#llx delta=%lld\n",
                 u64OSElapsedTS, u64RTElapsedTS, u64OSElapsedTS - u64RTElapsedTS);

#if defined RT_OS_WINDOWS || defined RT_OS_LINUX
    RTPrintf("tstTime-2: RTTime1nsSteps -> %u out of %u calls\n", RTTime1nsSteps(), NUMBER_OF_CALLS);
#endif
    if (!cErrors)
        RTPrintf("tstTime-2: SUCCESS\n");
    else
        RTPrintf("tstTime-2: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
