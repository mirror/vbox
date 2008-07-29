/* $Id$ */

/** @file
 *
 * Collector classes test cases.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include <iprt/runtime.h>
#include <iprt/stream.h>
#include <iprt/err.h>

#ifdef RT_OS_SOLARIS
#include "../solaris/PerformanceSolaris.cpp"
#endif
#ifdef RT_OS_LINUX
#include "../linux/PerformanceLinux.cpp"
#endif
#ifdef RT_OS_WINDOWS
#include "../win/PerformanceWin.cpp"
#endif
#ifdef RT_OS_OS2
#include "../os2/PerformanceOS2.cpp"
#endif
#ifdef RT_OS_DARWIN
#include "../darwin/PerformanceDarwin.cpp"
#endif

pm::CollectorHAL *createCollector()
{
#ifdef RT_OS_SOLARIS
    return new pm::CollectorSolaris();
#endif
#ifdef RT_OS_LINUX
    return new pm::CollectorLinux();
#endif
#ifdef RT_OS_WINDOWS
    return new pm::CollectorWin();
#endif
#ifdef RT_OS_OS2
    return new pm::CollectorOS2();
#endif
#ifdef RT_OS_DARWIN
    return new pm::CollectorDarwin();
#endif
    return 0;
}

int main(int argc, char *argv[])
{
    /*
     * Initialize the VBox runtime without loading
     * the support driver.
     */
    int rc = RTR3Init(false);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: RTR3Init() -> %d\n", rc);
        return 1;
    }

    pm::CollectorHAL *collector = createCollector();
    if (!collector)
    {
        RTPrintf("tstCollector: createMetricFactory() failed\n", rc);
        return 1;
    }

    uint64_t hostUserStart, hostKernelStart, hostIdleStart;
    uint64_t hostUserStop, hostKernelStop, hostIdleStop, hostTotal;

    uint64_t processUserStart, processKernelStart, processTotalStart;
    uint64_t processUserStop, processKernelStop, processTotalStop;

    RTPrintf("tstCollector: TESTING - CPU load, sleeping for 5 sec\n");

    rc = collector->getRawHostCpuLoad(&hostUserStart, &hostKernelStart, &hostIdleStart);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawHostCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    rc = collector->getRawProcessCpuLoad(getpid(), &processUserStart, &processKernelStart, &processTotalStart);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawProcessCpuLoad() -> %Vrc\n", rc);
        return 1;
    }

    RTThreadSleep(5000); // Sleep for 5 seconds

    rc = collector->getRawHostCpuLoad(&hostUserStop, &hostKernelStop, &hostIdleStop);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawHostCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    rc = collector->getRawProcessCpuLoad(getpid(), &processUserStop, &processKernelStop, &processTotalStop);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawProcessCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    hostTotal = hostUserStop - hostUserStart
        + hostKernelStop - hostKernelStart
        + hostIdleStop - hostIdleStart;
    RTPrintf("tstCollector: host cpu user      = %llu %%\n", (hostUserStop - hostUserStart) * 100 / hostTotal);
    RTPrintf("tstCollector: host cpu kernel    = %llu %%\n", (hostKernelStop - hostKernelStart) * 100 / hostTotal);
    RTPrintf("tstCollector: host cpu idle      = %llu %%\n", (hostIdleStop - hostIdleStart) * 100 / hostTotal);
    RTPrintf("tstCollector: process cpu user   = %llu %%\n", (processUserStop - processUserStart) * 100 / (processTotalStop - processTotalStart));
    RTPrintf("tstCollector: process cpu kernel = %llu %%\n\n", (processKernelStop - processKernelStart) * 100 / (processTotalStop - processTotalStart));

    RTPrintf("tstCollector: TESTING - CPU load, looping for 5 sec\n");
    rc = collector->getRawHostCpuLoad(&hostUserStart, &hostKernelStart, &hostIdleStart);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawHostCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    rc = collector->getRawProcessCpuLoad(getpid(), &processUserStart, &processKernelStart, &processTotalStart);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawProcessCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    uint64_t start = RTTimeMilliTS();
    while(RTTimeMilliTS() - start < 5000); // Loop for 5 seconds
    rc = collector->getRawHostCpuLoad(&hostUserStop, &hostKernelStop, &hostIdleStop);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawHostCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    rc = collector->getRawProcessCpuLoad(getpid(), &processUserStop, &processKernelStop, &processTotalStop);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getRawProcessCpuLoad() -> %Vrc\n", rc);
        return 1;
    }
    hostTotal = hostUserStop - hostUserStart
        + hostKernelStop - hostKernelStart
        + hostIdleStop - hostIdleStart;
    RTPrintf("tstCollector: host cpu user      = %llu %%\n", (hostUserStop - hostUserStart) * 100 / hostTotal);
    RTPrintf("tstCollector: host cpu kernel    = %llu %%\n", (hostKernelStop - hostKernelStart) * 100 / hostTotal);
    RTPrintf("tstCollector: host cpu idle      = %llu %%\n", (hostIdleStop - hostIdleStart) * 100 / hostTotal);
    RTPrintf("tstCollector: process cpu user   = %llu %%\n", (processUserStop - processUserStart) * 100 / (processTotalStop - processTotalStart));
    RTPrintf("tstCollector: process cpu kernel = %llu %%\n\n", (processKernelStop - processKernelStart) * 100 / (processTotalStop - processTotalStart));

    RTPrintf("tstCollector: TESTING - Memory usage\n");

    unsigned long total, used, available, processUsed;

    rc = collector->getHostMemoryUsage(&total, &used, &available);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getHostMemoryUsage() -> %Vrc\n", rc);
        return 1;
    }
    rc = collector->getProcessMemoryUsage(getpid(), &processUsed);
    if (RT_FAILURE(rc))
    {
        RTPrintf("tstCollector: getProcessMemoryUsage() -> %Vrc\n", rc);
        return 1;
    }
    RTPrintf("tstCollector: host mem total     = %lu kB\n", total);
    RTPrintf("tstCollector: host mem used      = %lu kB\n", used);
    RTPrintf("tstCollector: host mem available = %lu kB\n", available);
    RTPrintf("tstCollector: process mem used   = %lu kB\n", processUsed);

    delete collector;

    printf ("tstCollector FINISHED.\n");

    return rc;
}

