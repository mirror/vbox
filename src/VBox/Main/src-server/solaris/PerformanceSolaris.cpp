/* $Id$ */

/** @file
 *
 * VBox Solaris-specific Performance Classes implementation.
 */

/*
 * Copyright (C) 2008 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#undef _FILE_OFFSET_BITS
#include <procfs.h>
#include <stdio.h>
#include <errno.h>
#include <fcntl.h>
#include <kstat.h>
#include <unistd.h>
#include <sys/sysinfo.h>
#include <sys/time.h>

#include <iprt/ctype.h>
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
    virtual ~CollectorSolaris();
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawHostNetworkLoad(const char *name, uint64_t *rx, uint64_t *tx);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
private:
    static uint32_t getInstance(const char *pszIfaceName, char *pszDevName);
    kstat_ctl_t *mKC;
    kstat_t     *mSysPages;
    kstat_t     *mZFSCache;
};

CollectorHAL *createHAL()
{
    return new CollectorSolaris();
}

// Collector HAL for Solaris


CollectorSolaris::CollectorSolaris()
    : mKC(0),
      mSysPages(0),
      mZFSCache(0)
{
    if ((mKC = kstat_open()) == 0)
    {
        Log(("kstat_open() -> %d\n", errno));
        return;
    }

    if ((mSysPages = kstat_lookup(mKC, (char *)"unix", 0, (char *)"system_pages")) == 0)
    {
        Log(("kstat_lookup(system_pages) -> %d\n", errno));
        return;
    }

    if ((mZFSCache = kstat_lookup(mKC, (char *)"zfs", 0, (char *)"arcstats")) == 0)
    {
        Log(("kstat_lookup(system_pages) -> %d\n", errno));
    }
}

CollectorSolaris::~CollectorSolaris()
{
    if (mKC)
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
                Log(("kstat_read() -> %d\n", errno));
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
    RTStrFree(pszName);

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
            Log(("read() -> %d\n", errno));
            rc = VERR_FILE_IO_ERROR;
        }
        close(h);
    }
    else
    {
        Log(("open() -> %d\n", errno));
        rc = VERR_ACCESS_DENIED;
    }

    return rc;
}

int CollectorSolaris::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    int rc = VINF_SUCCESS;

    kstat_named_t *kn;

    if (mKC == 0 || mSysPages == 0)
        return VERR_INTERNAL_ERROR;

    if (kstat_read(mKC, mSysPages, 0) == -1)
    {
        Log(("kstat_read(sys_pages) -> %d\n", errno));
        return VERR_INTERNAL_ERROR;
    }
    if ((kn = (kstat_named_t *)kstat_data_lookup(mSysPages, (char *)"freemem")) == 0)
    {
        Log(("kstat_data_lookup(freemem) -> %d\n", errno));
        return VERR_INTERNAL_ERROR;
    }
    *available = kn->value.ul * (PAGE_SIZE/1024);

    if (kstat_read(mKC, mZFSCache, 0) != -1)
    {
        if (mZFSCache)
        {
            if ((kn = (kstat_named_t *)kstat_data_lookup(mZFSCache, (char *)"size")))
            {
                ulong_t ulSize = kn->value.ul;

                if ((kn = (kstat_named_t *)kstat_data_lookup(mZFSCache, (char *)"c_min")))
                {
                    /*
                     * Account for ZFS minimum arc cache size limit.
                     * "c_min" is the target minimum size of the ZFS cache, and not the hard limit. It's possible
                     * for "size" to shrink below "c_min" (e.g: during boot & high memory consumption).
                     */
                    ulong_t ulMin = kn->value.ul;
                    *available += ulSize > ulMin ? (ulSize - ulMin) / 1024 : 0;
                }
                else
                    Log(("kstat_data_lookup(c_min) ->%d\n", errno));
            }
            else
                Log(("kstat_data_lookup(size) -> %d\n", errno));
        }
        else
            Log(("mZFSCache missing.\n"));
    }

    if ((kn = (kstat_named_t *)kstat_data_lookup(mSysPages, (char *)"physmem")) == 0)
    {
        Log(("kstat_data_lookup(physmem) -> %d\n", errno));
        return VERR_INTERNAL_ERROR;
    }
    *total = kn->value.ul * (PAGE_SIZE/1024);
    *used = *total - *available;

    return rc;
}

int CollectorSolaris::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    int rc = VINF_SUCCESS;
    char *pszName = NULL;
    psinfo_t psinfo;

    RTStrAPrintf(&pszName, "/proc/%d/psinfo", process);
    Log(("Opening %s...\n", pszName));
    int h = open(pszName, O_RDONLY);
    RTStrFree(pszName);

    if (h != -1)
    {
        if (read(h, &psinfo, sizeof(psinfo)) == sizeof(psinfo))
        {
            Assert((pid_t)process == psinfo.pr_pid);
            *used = psinfo.pr_rssize;
        }
        else
        {
            Log(("read() -> %d\n", errno));
            rc = VERR_FILE_IO_ERROR;
        }
        close(h);
    }
    else
    {
        Log(("open() -> %d\n", errno));
        rc = VERR_ACCESS_DENIED;
    }

    return rc;
}

uint32_t CollectorSolaris::getInstance(const char *pszIfaceName, char *pszDevName)
{
    /*
     * Get the instance number from the interface name, then clip it off.
     */
    int cbInstance = 0;
    int cbIface = strlen(pszIfaceName);
    const char *pszEnd = pszIfaceName + cbIface - 1;
    for (int i = 0; i < cbIface - 1; i++)
    {
        if (!RT_C_IS_DIGIT(*pszEnd))
            break;
        cbInstance++;
        pszEnd--;
    }

    uint32_t uInstance = RTStrToUInt32(pszEnd + 1);
    strncpy(pszDevName, pszIfaceName, cbIface - cbInstance);
    pszDevName[cbIface - cbInstance] = '\0';
    return uInstance;
}

int CollectorSolaris::getRawHostNetworkLoad(const char *name, uint64_t *rx, uint64_t *tx)
{
    AssertReturn(strlen(name) < KSTAT_STRLEN, VERR_INVALID_PARAMETER);
    kstat_t *ksAdapter = kstat_lookup(mKC, NULL, -1, (char *)name);
    if (ksAdapter == 0)
    {
        char szModule[KSTAT_STRLEN];
        uint32_t uInstance = getInstance(name, szModule);
        ksAdapter = kstat_lookup(mKC, szModule, uInstance, NULL);
        if (ksAdapter == 0)
        {
            LogRel(("Failed to get network statistics for %s\n", name));
            return VERR_INTERNAL_ERROR;
        }
    }
    if (kstat_read(mKC, ksAdapter, 0) == -1)
    {
        LogRel(("kstat_read(adapter) -> %d\n", errno));
        return VERR_INTERNAL_ERROR;
    }
    kstat_named_t *kn;
    if ((kn = (kstat_named_t *)kstat_data_lookup(ksAdapter, (char *)"rbytes")) == 0)
    {
        LogRel(("kstat_data_lookup(rbytes) -> %d\n", errno));
        return VERR_INTERNAL_ERROR;
    }
    *rx = kn->value.ul;
    if ((kn = (kstat_named_t *)kstat_data_lookup(ksAdapter, (char *)"obytes")) == 0)
    {
        LogRel(("kstat_data_lookup(obytes) -> %d\n", errno));
        return VERR_INTERNAL_ERROR;
    }
    *tx = kn->value.ul;
    return VINF_SUCCESS;
}

}
