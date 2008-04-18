/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Simple RTTime tests.
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
#include <iprt/initterm.h>
#include <iprt/thread.h>
#include <VBox/sup.h>


int main()
{
    unsigned cErrors = 0;
    int i;
    RTR3Init();
    RTPrintf("tstTime: TESTING...\n");

    /*
     * RTNanoTimeTS() shall never return something which
     * is less or equal to the return of the previous call.
     */

    RTTimeSystemNanoTS(); RTTimeNanoTS(); RTThreadYield();
    uint64_t u64RTStartTS = RTTimeNanoTS();
    uint64_t u64OSStartTS = RTTimeSystemNanoTS();

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

    RTTimeSystemNanoTS(); RTTimeNanoTS(); RTThreadYield();
    uint64_t u64RTElapsedTS = RTTimeNanoTS();
    uint64_t u64OSElapsedTS = RTTimeSystemNanoTS();
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

    RTPrintf("RTTimeDbgSteps   -> %u (%d ppt)\n", RTTimeDbgSteps(),   ((uint64_t)RTTimeDbgSteps() * 1000) / i);
    RTPrintf("RTTimeDbgExpired -> %u (%d ppt)\n", RTTimeDbgExpired(), ((uint64_t)RTTimeDbgExpired() * 1000) / i);
    RTPrintf("RTTimeDbgBad     -> %u (%d ppt)\n", RTTimeDbgBad(),     ((uint64_t)RTTimeDbgBad() * 1000) / i);
    RTPrintf("RTTimeDbgRaces   -> %u (%d ppt)\n", RTTimeDbgRaces(),   ((uint64_t)RTTimeDbgRaces() * 1000) / i);
    if (!cErrors)
        RTPrintf("tstTime: SUCCESS\n");
    else
        RTPrintf("tstTime: FAILURE - %d errors\n", cErrors);
    return !!cErrors;
}
