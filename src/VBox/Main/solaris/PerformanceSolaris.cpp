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
#include <errno.h>
#include <fcntl.h>
#include <kstat.h>
#include <sys/sysinfo.h>
#include <sys/time.h>
#include <procfs.h>

#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/param.h>
#include <VBox/log.h>
#include "Performance.h"

namespace pm {

class CollectorSolaris : public CollectorHAL
{
public:
    CollectorSolaris();
    ~CollectorSolaris();
    virtual int getHostCpuMHz(unsigned long *mhz);
    virtual int getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available);
    virtual int getProcessMemoryUsage(RTPROCESS process, unsigned long *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
private:
    kstat_ctl_t *mKC;
    kstat_t     *mSysPages;
};

// Solaris Metric factory

MetricFactorySolaris::MetricFactorySolaris()
{
    mHAL = new CollectorSolaris();
    Assert(mHAL);
}

// Collector HAL for Solaris


CollectorSolaris::CollectorSolaris() : mKC(0), mSysPages(0)
{
    if ((mKC = kstat_open()) == 0)
    {
        Log(("kstat_open() -> %d\n", errno));
        return;
    }

    if ((mSysPages = kstat_lookup(mKC, "unix", 0, "system_pages")) == 0)
    {
        Log(("kstat_lookup(system_pages) -> %d\n", errno));
        return;
    }
}

CollectorSolaris::~CollectorSolaris()
{
    kstat_close(mKC);
}

int CollectorSolaris::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    int rc = VINF_SUCCESS;
    kstat_t *ksp;
    uint64_t tmpUser, tmpKernel, tmpIdle;
    int cpus;
    cpu_stat_t cpu_stats;
    
    if (mKC == 0)
        return VERR_INTERNAL_ERROR;
    
    tmpUser = tmpKernel = tmpIdle = cpus = 0;
    for (ksp = mKC->kc_chain; ksp != NULL; ksp = ksp->ks_next) {
        if (strcmp(ksp->ks_module, "cpu_stat") == 0) {
            if (kstat_read(mKC, ksp, &cpu_stats) == -1)
            {
                Log(("kstat_read() -> %d", errno));
                return VERR_INTERNAL_ERROR;
            }
            ++cpus;
            tmpUser   += cpu_stats.cpu_sysinfo.cpu[CPU_USER];
            tmpKernel += cpu_stats.cpu_sysinfo.cpu[CPU_KERNEL];
            tmpIdle   += cpu_stats.cpu_sysinfo.cpu[CPU_IDLE];
        }         
    }
    
    if (cpus == 0)
    {
        Log(("no cpu stats found!\n"));
        return VERR_INTERNAL_ERROR;
    }

    if (user)   *user   = tmpUser;
    if (kernel) *kernel = tmpKernel;
    if (idle)   *idle   = tmpIdle;

    return rc;
}

int CollectorSolaris::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
    int rc = VINF_SUCCESS;
    char *pszName;
    prusage_t prusage;

    RTStrAPrintf(&pszName, "/proc/%d/usage", process);
    Log(("Opening %s...\n", pszName));
    int h = open(pszName, O_RDONLY);
    RTMemFree(pszName);

    if (h != -1)
    {
        if (read(h, &prusage, sizeof(prusage)) == sizeof(prusage))
        {
            //Assert((pid_t)process == pstatus.pr_pid);
            //Log(("user=%u kernel=%u total=%u\n", prusage.pr_utime.tv_sec, prusage.pr_stime.tv_sec, prusage.pr_tstamp.tv_sec));
            *user = (uint64_t)prusage.pr_utime.tv_sec * 1000000000 + prusage.pr_utime.tv_nsec;
            *kernel = (uint64_t)prusage.pr_stime.tv_sec * 1000000000 + prusage.pr_stime.tv_nsec;
            *total = (uint64_t)prusage.pr_tstamp.tv_sec * 1000000000 + prusage.pr_tstamp.tv_nsec;
            //Log(("user=%llu kernel=%llu total=%llu\n", *user, *kernel, *total));
        }
        else
        {
            Log(("read() -> %d", errno));
            rc = VERR_FILE_IO_ERROR;
        }
        close(h);
    }
    else
    {
        Log(("open() -> %d", errno));
        rc = VERR_ACCESS_DENIED;
    }

    return rc;
}

int CollectorSolaris::getHostCpuMHz(unsigned long *mhz)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorSolaris::getHostMemoryUsage(unsigned long *total, unsigned long *used, unsigned long *available)
{
    int rc = VINF_SUCCESS;

    kstat_named_t *kn;
    
    if (mKC == 0 || mSysPages == 0)
        return VERR_INTERNAL_ERROR;

    if (kstat_read(mKC, mSysPages, 0) == -1)
    {
        Log(("kstat_read(sys_pages) -> %d", errno));
        return VERR_INTERNAL_ERROR;
    }
    if ((kn = (kstat_named_t *)kstat_data_lookup(mSysPages, "freemem")) == 0)
    {
        Log(("kstat_data_lookup(freemem) -> %d", errno));
        return VERR_INTERNAL_ERROR;
    }
    *available = kn->value.ul * (PAGE_SIZE/1024);
    if ((kn = (kstat_named_t *)kstat_data_lookup(mSysPages, "physmem")) == 0)
    {
        Log(("kstat_data_lookup(physmem) -> %d", errno));
        return VERR_INTERNAL_ERROR;
    }
    *total = kn->value.ul * (PAGE_SIZE/1024);
    *used = *total - *available;

    return rc;
}
int CollectorSolaris::getProcessMemoryUsage(RTPROCESS process, unsigned long *used)
{
    int rc = VINF_SUCCESS;
    char *pszName;
    pid_t pid2;
    char buf[80]; /* @todo: this should be tied to max allowed proc name. */
    psinfo_t psinfo;

    RTStrAPrintf(&pszName, "/proc/%d/psinfo", process);
    Log(("Opening %s...\n", pszName));
    int h = open(pszName, O_RDONLY);
    RTMemFree(pszName);

    if (h != -1)
    {
        if (read(h, &psinfo, sizeof(psinfo)) == sizeof(psinfo))
        {
            Assert((pid_t)process == psinfo.pr_pid);
            *used = psinfo.pr_rssize;
        }
        else
        {
            Log(("read() -> %d", errno));
            rc = VERR_FILE_IO_ERROR;
        }
        close(h);
    }
    else
    {
        Log(("open() -> %d", errno));
        rc = VERR_ACCESS_DENIED;
    }

    return rc;
}

}

