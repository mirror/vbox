/* $Id$ */
/** @file
 * InnoTek Portable Runtime Testcase - Timers.
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

static DECLCALLBACK(void) TimerCallback(PRTTIMER pTimer, void *pvUser)
{
    gcTicks++;
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
        rc = RTTimerCreate(&pTimer, aTests[i].uMilliesInterval, TimerCallback, NULL);
        if (RT_FAILURE(rc))
        {
            RTPrintf("RTTimerCreate(,%d,) -> %d\n", aTests[i].uMilliesInterval, rc);
            cErrors++;
            continue;
        }

        /*
         * Sleep for a while and then kill it.
         */
        uint64_t uTSBegin = RTTimeNanoTS();
#if 1
        while (RTTimeNanoTS() - uTSBegin < (uint64_t)aTests[i].uMilliesWait * 1000000)
            /* nothing */;

#else
        rc = RTThreadSleep(aTests[i].uMilliesWait);
#endif
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
            RTPrintf("tstTimer: FAILURE - Too few ticks gcTicks=%d (expected %d-%d)\n", gcTicks, aTests[i].cUpper, aTests[i].cLower);
            cErrors++;
        }
        else if (gcTicks > aTests[i].cUpper)
        {
            RTPrintf("tstTimer: FAILURE - Too many ticks gcTicks=%d (expected %d-%d)\n", gcTicks, aTests[i].cUpper, aTests[i].cLower);
            cErrors++;
        }
        else
            RTPrintf("tstTimer: OK      - gcTicks=%d\n",  gcTicks);
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



