/* $Id$ */

/** @file
 *
 * VBox Solaris-specific Performance Classes implementation.
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

#include <stdio.h>
#include "Performance.h"

namespace pm {

class CollectorSolaris : public CollectorHAL
{
public:
    virtual int getHostCpuMHz(unsigned long *mhz);
    virtual int getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available);
    virtual int getProcessMemoryUsage(RTPROCESS process, unsigned long *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
};

// Solaris Metric factory

MetricFactorySolaris::MetricFactorySolaris()
{
    mHAL = new CollectorSolaris();
    Assert(mHAL);
}

BaseMetric *MetricFactorySolaris::createHostCpuLoad(ComPtr<IUnknown> object, SubMetric *user, SubMetric *kernel, SubMetric *idle)
{
    Assert(mHAL);
    return new HostCpuLoadRaw(mHAL, object, user, kernel, idle);
}

BaseMetric *MetricFactorySolaris::createMachineCpuLoad(ComPtr<IUnknown> object, RTPROCESS process, SubMetric *user, SubMetric *kernel)
{
    Assert(mHAL);
    return new MachineCpuLoadRaw(mHAL, object, process, user, kernel);
}

// Collector HAL for Solaris

int CollectorSolaris::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;
    unsigned long nice;
    FILE *f = fopen("/proc/stat", "r");

    if (f)
    {
        if (fscanf(f, "cpu %lu %lu %lu %lu", user, &nice, kernel, idle) == 4)
            *user += nice;
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
#else
    return E_NOTIMPL;
#endif
}

int CollectorSolaris::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;
    char *pszName;
    pid_t pid2;
    char c;
    int iTmp;
    unsigned uTmp;
    unsigned long ulTmp;
    char buf[80]; /* @todo: this should be tied to max allowed proc name. */

    RTStrAPrintf(&pszName, "/proc/%d/stat", process);
    //printf("Opening %s...\n", pszName);
    FILE *f = fopen(pszName, "r");
    RTMemFree(pszName);

    if (f)
    {
        if (fscanf(f, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
                   &pid2, buf, &c, &iTmp, &iTmp, &iTmp, &iTmp, &iTmp, &uTmp,
                   &ulTmp, &ulTmp, &ulTmp, &ulTmp, user, kernel) == 15)
        {
            Assert((pid_t)process == pid2);
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
#else
    return E_NOTIMPL;
#endif
}

int CollectorSolaris::getHostCpuMHz(unsigned long *mhz)
{
    return E_NOTIMPL;
}

int CollectorSolaris::getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available)
{
#ifdef RT_OS_LINUX
    int rc = VINF_SUCCESS;
    unsigned long buffers, cached;
    FILE *f = fopen("/proc/meminfo", "r");

    if (f)
    {
        int processed = fscanf(f, "MemTotal: %lu kB", total);
        processed    += fscanf(f, "MemFree: %lu kB", available);
        processed    += fscanf(f, "Buffers: %lu kB", &buffers);
        processed    += fscanf(f, "Cached: %lu kB", &cached);
        if (processed == 4)
            *available += buffers + cached;
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
#else
    return E_NOTIMPL;
#endif
}
int CollectorSolaris::getProcessMemoryUsage(RTPROCESS process, unsigned long *used)
{
    return E_NOTIMPL;
}

}
