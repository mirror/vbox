/* $Id$ */
/** @file
 * innotek Portable Runtime Testcase - Timers.
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
#include <iprt/timer.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/err.h>



/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static volatile unsigned gcTicks;
static volatile uint64_t gu64Min;
static volatile uint64_t gu64Max;
static volatile uint64_t gu64Prev;

static DECLCALLBACK(void) TimerCallback(PRTTIMER pTimer, void *pvUser)
{
    gcTicks++;

    const uint64_t u64Now = RTTimeNanoTS();
    if (gu64Prev)
    {
        const uint64_t u64Delta = u64Now - gu64Prev;
        if (u64Delta < gu64Min)
            gu64Min = u64Delta;
        if (u64Delta > gu64Max)
            gu64Max = u64Delta;
    }
    gu64Prev = u64Now;
}


int main()
{
    /*
     * Init runtime
     */
    unsigned cErrors = 0;
    int rc = RTR3Init();
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstTimer: RTR3Init() -> %d\n", rc);
        return 1;
    }

    /*
     * Check that the clock is reliable.
     */
    RTPrintf("tstTimer: TESTING - RTTimeNanoTS() for 2sec\n");
    uint64_t uTSMillies = RTTimeMilliTS();
    uint64_t uTSBegin = RTTimeNanoTS();
    uint64_t uTSLast = uTSBegin;
    uint64_t uTSDiff;
    uint64_t cIterations = 0;

    do
    {
        uint64_t uTS = RTTimeNanoTS();
        if (uTS < uTSLast)
        {
            RTPrintf("tstTimer: FAILURE - RTTimeNanoTS() is unreliable. uTS=%RU64 uTSLast=%RU64\n", uTS, uTSLast);
            cErrors++;
        }
        if (++cIterations > (2*1000*1000*1000))
        {
            RTPrintf("tstTimer: FAILURE - RTTimeNanoTS() is unreliable. cIterations=%RU64 uTS=%RU64 uTSBegin=%RU64\n", cIterations, uTS, uTSBegin);
            return 1;
        }
        uTSLast = uTS;
        uTSDiff = uTSLast - uTSBegin;
    } while (uTSDiff < (2*1000*1000*1000));
    uTSMillies = RTTimeMilliTS() - uTSMillies;
    if (uTSMillies >= 2500 || uTSMillies <= 1500)
    {
        RTPrintf("tstTimer: FAILURE - uTSMillies=%RI64 uTSBegin=%RU64 uTSLast=%RU64 uTSDiff=%RU64\n",
                 uTSMillies, uTSBegin, uTSLast, uTSDiff);
        cErrors++;
    }
    if (!cErrors)
        RTPrintf("tstTimer: OK      - RTTimeNanoTS()\n");

    /*
     * Tests.
     */
    static struct
    {
        unsigned uMilliesInterval;
        unsigned uMilliesWait;
        unsigned cLower;
        unsigned cUpper;
    } aTests[] =
    {
        { 32, 2000, 0, 0 },
        { 20, 2000, 0, 0 },
        { 10, 2000, 0, 0 },
        { 8,  2000, 0, 0 },
        { 2,  2000, 0, 0 },
        { 1,  2000, 0, 0 }
    };

    unsigned i = 0;
    for (i = 0; i < ELEMENTS(aTests); i++)
    {
        aTests[i].cLower = (aTests[i].uMilliesWait - aTests[i].uMilliesWait / 10) / aTests[i].uMilliesInterval;
        aTests[i].cUpper = (aTests[i].uMilliesWait + aTests[i].uMilliesWait / 10) / aTests[i].uMilliesInterval;

        RTPrintf("tstTimer: TESTING - %d ms interval, %d ms wait, expects %d-%d ticks.\n",
                 aTests[i].uMilliesInterval, aTests[i].uMilliesWait, aTests[i].cLower, aTests[i].cUpper);

        /*
         * Start timer which ticks every 10ms.
         */
        gcTicks = 0;
        PRTTIMER pTimer;
        gu64Max = 0;
        gu64Min = UINT64_MAX;
        gu64Prev = 0;
        rc = RTTimerCreate(&pTimer, aTests[i].uMilliesInterval, TimerCallback, NULL);
        if (RT_FAILURE(rc))
        {
            RTPrintf("RTTimerCreate(,%d,) -> %d\n", aTests[i].uMilliesInterval, rc);
            cErrors++;
            continue;
        }

        /*
         * Active waiting for 2 seconds and then destroy it.
         */
        uint64_t uTSBegin = RTTimeNanoTS();
        while (RTTimeNanoTS() - uTSBegin < (uint64_t)aTests[i].uMilliesWait * 1000000)
            /* nothing */;
        uint64_t uTSEnd = RTTimeNanoTS();
        uint64_t uTSDiff = uTSEnd - uTSBegin;
        RTPrintf("uTS=%RI64 (%RU64 - %RU64)\n", uTSDiff, uTSBegin, uTSEnd);
        if (RT_FAILURE(rc))
            RTPrintf("warning: RTThreadSleep ended prematurely with %d\n", rc);
        rc = RTTimerDestroy(pTimer);
        if (RT_FAILURE(rc))
        {
            RTPrintf("tstTimer: FAILURE - RTTimerDestroy() -> %d gcTicks=%d\n", rc, gcTicks);
            cErrors++;
            continue;
        }
        unsigned cTicks = gcTicks;
        RTThreadSleep(100);
        if (gcTicks != cTicks)
        {
            RTPrintf("tstTimer: FAILURE - RTTimerDestroy() didn't really stop the timer! gcTicks=%d cTicks=%d\n", gcTicks, cTicks);
            cErrors++;
            continue;
        }

        /*
         * Check the number of ticks.
         */
        if (gcTicks < aTests[i].cLower)
        {
            RTPrintf("tstTimer: FAILURE - Too few ticks gcTicks=%d (expected %d-%d)", gcTicks, aTests[i].cUpper, aTests[i].cLower);
            cErrors++;
        }
        else if (gcTicks > aTests[i].cUpper)
        {
            RTPrintf("tstTimer: FAILURE - Too many ticks gcTicks=%d (expected %d-%d)", gcTicks, aTests[i].cUpper, aTests[i].cLower);
            cErrors++;
        }
        else
            RTPrintf("tstTimer: OK      - gcTicks=%d",  gcTicks);
        RTPrintf(" min=%RU64 max=%RU64\n", gu64Min, gu64Max);
    }

    /*
     * Summary.
     */
    if (!cErrors)
        RTPrintf("tstTimer: SUCCESS\n");
    else
        RTPrintf("tstTimer: FAILURE %d errors\n", cErrors);
    return !!cErrors;
}



