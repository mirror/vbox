/* $Id$ */

/** @file
 *
 * VBox Linux-specific Performance Classes implementation.
 */

/*
 * Copyright (C) 2008-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <stdio.h>
#include <iprt/alloc.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/string.h>

#include <map>
#include <vector>

#include "Logging.h"
#include "Performance.h"

namespace pm {

class CollectorLinux : public CollectorHAL
{
public:
    virtual int preCollect(const CollectorHints& hints, uint64_t /* iTick */);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawHostNetworkLoad(const char *name, uint64_t *rx, uint64_t *tx);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
private:
    virtual int _getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    int getRawProcessStats(RTPROCESS process, uint64_t *cpuUser, uint64_t *cpuKernel, ULONG *memPagesUsed);

    struct VMProcessStats
    {
        uint64_t cpuUser;
        uint64_t cpuKernel;
        ULONG    pagesUsed;
    };

    typedef std::map<RTPROCESS, VMProcessStats> VMProcessMap;

    VMProcessMap mProcessStats;
    uint64_t     mUser, mKernel, mIdle;
};

CollectorHAL *createHAL()
{
    return new CollectorLinux();
}

// Collector HAL for Linux

int CollectorLinux::preCollect(const CollectorHints& hints, uint64_t /* iTick */)
{
    std::vector<RTPROCESS> processes;
    hints.getProcesses(processes);

    std::vector<RTPROCESS>::iterator it;
    for (it = processes.begin(); it != processes.end(); it++)
    {
        VMProcessStats vmStats;
        int rc = getRawProcessStats(*it, &vmStats.cpuUser, &vmStats.cpuKernel, &vmStats.pagesUsed);
        /* On failure, do NOT stop. Just skip the entry. Having the stats for
         * one (probably broken) process frozen/zero is a minor issue compared
         * to not updating many process stats and the host cpu stats. */
        if (RT_SUCCESS(rc))
            mProcessStats[*it] = vmStats;
    }
    if (hints.isHostCpuLoadCollected() || mProcessStats.size())
    {
        _getRawHostCpuLoad(&mUser, &mKernel, &mIdle);
    }
    return VINF_SUCCESS;
}

int CollectorLinux::_getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    int rc = VINF_SUCCESS;
    ULONG u32user, u32nice, u32kernel, u32idle;
    FILE *f = fopen("/proc/stat", "r");

    if (f)
    {
        if (fscanf(f, "cpu %u %u %u %u", &u32user, &u32nice, &u32kernel, &u32idle) == 4)
        {
            *user   = (uint64_t)u32user + u32nice;
            *kernel = u32kernel;
            *idle   = u32idle;
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}

int CollectorLinux::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    *user   = mUser;
    *kernel = mKernel;
    *idle   = mIdle;
    return VINF_SUCCESS;
}

int CollectorLinux::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
    VMProcessMap::const_iterator it = mProcessStats.find(process);

    if (it == mProcessStats.end())
    {
        Log (("No stats pre-collected for process %x\n", process));
        return VERR_INTERNAL_ERROR;
    }
    *user   = it->second.cpuUser;
    *kernel = it->second.cpuKernel;
    *total  = mUser + mKernel + mIdle;
    return VINF_SUCCESS;
}

int CollectorLinux::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    int rc = VINF_SUCCESS;
    ULONG buffers, cached;
    FILE *f = fopen("/proc/meminfo", "r");

    if (f)
    {
        int processed = fscanf(f, "MemTotal: %u kB\n", total);
        processed    += fscanf(f, "MemFree: %u kB\n", available);
        processed    += fscanf(f, "Buffers: %u kB\n", &buffers);
        processed    += fscanf(f, "Cached: %u kB\n", &cached);
        if (processed == 4)
        {
            *available += buffers + cached;
            *used       = *total - *available;
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}

int CollectorLinux::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    VMProcessMap::const_iterator it = mProcessStats.find(process);

    if (it == mProcessStats.end())
    {
        Log (("No stats pre-collected for process %x\n", process));
        return VERR_INTERNAL_ERROR;
    }
    *used = it->second.pagesUsed * (PAGE_SIZE / 1024);
    return VINF_SUCCESS;
}

int CollectorLinux::getRawProcessStats(RTPROCESS process, uint64_t *cpuUser, uint64_t *cpuKernel, ULONG *memPagesUsed)
{
    int rc = VINF_SUCCESS;
    char *pszName;
    pid_t pid2;
    char c;
    int iTmp;
    long long unsigned int u64Tmp;
    unsigned uTmp;
    unsigned long ulTmp;
    signed long ilTmp;
    ULONG u32user, u32kernel;
    char buf[80]; /* @todo: this should be tied to max allowed proc name. */

    RTStrAPrintf(&pszName, "/proc/%d/stat", process);
    //printf("Opening %s...\n", pszName);
    FILE *f = fopen(pszName, "r");
    RTMemFree(pszName);

    if (f)
    {
        if (fscanf(f, "%d %79s %c %d %d %d %d %d %u %lu %lu %lu %lu %u %u "
                      "%ld %ld %ld %ld %ld %ld %llu %lu %u",
                   &pid2, buf, &c, &iTmp, &iTmp, &iTmp, &iTmp, &iTmp, &uTmp,
                   &ulTmp, &ulTmp, &ulTmp, &ulTmp, &u32user, &u32kernel,
                   &ilTmp, &ilTmp, &ilTmp, &ilTmp, &ilTmp, &ilTmp, &u64Tmp,
                   &ulTmp, memPagesUsed) == 24)
        {
            Assert((pid_t)process == pid2);
            *cpuUser   = u32user;
            *cpuKernel = u32kernel;
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}

int CollectorLinux::getRawHostNetworkLoad(const char *name, uint64_t *rx, uint64_t *tx)
{
    int rc = VINF_SUCCESS;
    char szIfName[/*IFNAMSIZ*/ 16 + 36];
    long long unsigned int u64Rx, u64Tx;

    RTStrPrintf(szIfName, sizeof(szIfName), "/sys/class/net/%s/statistics/rx_bytes", name);
    FILE *f = fopen(szIfName, "r");
    if (f)
    {
        if (fscanf(f, "%llu", &u64Rx) == 1)
            *rx = u64Rx;
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
        RTStrPrintf(szIfName, sizeof(szIfName), "/sys/class/net/%s/statistics/tx_bytes", name);
        f = fopen(szIfName, "r");
        if (f)
        {
            if (fscanf(f, "%llu", &u64Tx) == 1)
                *tx = u64Tx;
            else
                rc = VERR_FILE_IO_ERROR;
            fclose(f);
        }
        else
            rc = VERR_ACCESS_DENIED;
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}

}

