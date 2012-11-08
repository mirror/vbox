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
#include <unistd.h>
#include <sys/statvfs.h>
#include <errno.h>
#include <mntent.h>
#include <iprt/alloc.h>
#include <iprt/cdefs.h>
#include <iprt/ctype.h>
#include <iprt/err.h>
#include <iprt/param.h>
#include <iprt/path.h>
#include <iprt/string.h>
#include <iprt/mp.h>

#include <map>
#include <vector>

#include "Logging.h"
#include "Performance.h"

#define VBOXVOLINFO_NAME "VBoxVolInfo"

namespace pm {

class CollectorLinux : public CollectorHAL
{
public:
    CollectorLinux();
    virtual int preCollect(const CollectorHints& hints, uint64_t /* iTick */);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getHostFilesystemUsage(const char *name, ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawHostNetworkLoad(const char *name, uint64_t *rx, uint64_t *tx);
    virtual int getRawHostDiskLoad(const char *name, uint64_t *disk_ms, uint64_t *total_ms);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);

    virtual int getDiskListByFs(const char *name, DiskList& list);
private:
    virtual int _getRawHostCpuLoad();
    int getRawProcessStats(RTPROCESS process, uint64_t *cpuUser, uint64_t *cpuKernel, ULONG *memPagesUsed);
    char *getDiskName(char *pszDiskName, size_t cbDiskName, const char *pszDevName, bool fTrimDigits);
    void addVolumeDependencies(const char *pcszVolume, DiskList& listDisks);
    char *trimTrailingDigits(char *pszName);

    struct VMProcessStats
    {
        uint64_t cpuUser;
        uint64_t cpuKernel;
        ULONG    pagesUsed;
    };

    typedef std::map<RTPROCESS, VMProcessStats> VMProcessMap;

    VMProcessMap mProcessStats;
    uint64_t     mUser, mKernel, mIdle;
    uint64_t     mSingleUser, mSingleKernel, mSingleIdle;
    uint32_t     mHZ;
};

CollectorHAL *createHAL()
{
    return new CollectorLinux();
}

// Collector HAL for Linux

CollectorLinux::CollectorLinux()
{
    long hz = sysconf(_SC_CLK_TCK);
    if (hz == -1)
    {
        LogRel(("CollectorLinux failed to obtain HZ from kernel, assuming 100.\n"));
        mHZ = 100;
    }
    else
        mHZ = hz;
    LogFlowThisFunc(("mHZ=%u\n", mHZ));
}

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
        _getRawHostCpuLoad();
    }
    return VINF_SUCCESS;
}

int CollectorLinux::_getRawHostCpuLoad()
{
    int rc = VINF_SUCCESS;
    long long unsigned uUser, uNice, uKernel, uIdle, uIowait, uIrq, uSoftirq;
    FILE *f = fopen("/proc/stat", "r");

    if (f)
    {
        char szBuf[128];
        if (fgets(szBuf, sizeof(szBuf), f))
        {
            if (sscanf(szBuf, "cpu %llu %llu %llu %llu %llu %llu %llu",
                       &uUser, &uNice, &uKernel, &uIdle, &uIowait,
                       &uIrq, &uSoftirq) == 7)
            {
                mUser   = uUser + uNice;
                mKernel = uKernel + uIrq + uSoftirq;
                mIdle   = uIdle + uIowait;
            }
            /* Try to get single CPU stats. */
            if (fgets(szBuf, sizeof(szBuf), f))
            {
                if (sscanf(szBuf, "cpu0 %llu %llu %llu %llu %llu %llu %llu",
                           &uUser, &uNice, &uKernel, &uIdle, &uIowait,
                           &uIrq, &uSoftirq) == 7)
                {
                    mSingleUser   = uUser + uNice;
                    mSingleKernel = uKernel + uIrq + uSoftirq;
                    mSingleIdle   = uIdle + uIowait;
                }
                else
                {
                    /* Assume that this is not an SMP system. */
                    Assert(RTMpGetCount() == 1);
                    mSingleUser   = mUser;
                    mSingleKernel = mKernel;
                    mSingleIdle   = mIdle;
                }
            }
            else
                rc = VERR_FILE_IO_ERROR;
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

int CollectorLinux::getHostFilesystemUsage(const char *path, ULONG *total, ULONG *used, ULONG *available)
{
    struct statvfs stats;
    const unsigned _MB = 1024 * 1024;

    if (statvfs(path, &stats) == -1)
    {
        LogRel(("Failed to collect %s filesystem usage: errno=%d.\n", path, errno));
        return VERR_ACCESS_DENIED;
    }
    uint64_t cbBlock = stats.f_frsize ? stats.f_frsize : stats.f_bsize;
    *total = (ULONG)(cbBlock * stats.f_blocks / _MB);
    *used  = (ULONG)(cbBlock * (stats.f_blocks - stats.f_bfree) / _MB);
    *available = (ULONG)(cbBlock * stats.f_bavail / _MB);

    return VINF_SUCCESS;
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

int CollectorLinux::getRawHostDiskLoad(const char *name, uint64_t *disk_ms, uint64_t *total_ms)
{
#if 0
    int rc = VINF_SUCCESS;
    char szIfName[/*IFNAMSIZ*/ 16 + 36];
    long long unsigned int u64Busy, tmp;

    RTStrPrintf(szIfName, sizeof(szIfName), "/sys/class/block/%s/stat", name);
    FILE *f = fopen(szIfName, "r");
    if (f)
    {
        if (fscanf(f, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                   &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &u64Busy, &tmp) == 11)
        {
            *disk_ms   = u64Busy;
            *total_ms  = (uint64_t)(mSingleUser + mSingleKernel + mSingleIdle) * 1000 / mHZ;
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;
#else
    int rc = VERR_MISSING;
    FILE *f = fopen("/proc/diskstats", "r");
    if (f)
    {
        char szBuf[128];
        while (fgets(szBuf, sizeof(szBuf), f))
        {
            char *pszBufName = &szBuf[13];
            char *pszBufData = strchr(pszBufName, ' ');
            if (!pszBufData)
            {
                LogRel(("CollectorLinux::getRawHostDiskLoad() failed to parse disk stats: %s\n", szBuf));
                continue;
            }
            *pszBufData++ = '\0';
            if (!strcmp(name, pszBufName))
            {
                long long unsigned int u64Busy, tmp;

                if (sscanf(pszBufData, "%llu %llu %llu %llu %llu %llu %llu %llu %llu %llu %llu",
                           &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &tmp, &u64Busy, &tmp) == 11)
                {
                    *disk_ms   = u64Busy;
                    *total_ms  = (uint64_t)(mSingleUser + mSingleKernel + mSingleIdle) * 1000 / mHZ;
                    rc = VINF_SUCCESS;
                }
                else
                    rc = VERR_FILE_IO_ERROR;
                break;
            }
        }
        fclose(f);
    }
#endif

    return rc;
}

char *CollectorLinux::trimTrailingDigits(char *pszName)
{
    unsigned cbName = strlen(pszName);
    if (cbName == 0)
        return pszName;

    char *pszEnd = pszName + cbName - 1;
    while (pszEnd > pszName && RT_C_IS_DIGIT(*pszEnd))
        pszEnd--;
    pszEnd[1] = '\0';

    return pszName;
}

char *CollectorLinux::getDiskName(char *pszDiskName, size_t cbDiskName, const char *pszDevName, bool fTrimDigits)
{
    unsigned cbName = 0;
    unsigned cbDevName = strlen(pszDevName);
    const char *pszEnd = pszDevName + cbDevName - 1;
    if (fTrimDigits)
        while (pszEnd > pszDevName && RT_C_IS_DIGIT(*pszEnd))
            pszEnd--;
    while (pszEnd > pszDevName && *pszEnd != '/')
    {
        cbName++;
        pszEnd--;
    }
    RTStrCopy(pszDiskName, RT_MIN(cbName + 1, cbDiskName), pszEnd + 1);
    return pszDiskName;
}

void CollectorLinux::addVolumeDependencies(const char *pcszVolume, DiskList& listDisks)
{
    char szVolInfo[RTPATH_MAX];
    int rc = RTPathExecDir(szVolInfo, sizeof(szVolInfo) - sizeof("/" VBOXVOLINFO_NAME " ") - strlen(pcszVolume));
    if (RT_FAILURE(rc))
    {
        LogRel(("VolInfo: Failed to get program path, rc=%Rrc\n", rc));
        return;
    }
    strcat(szVolInfo, "/" VBOXVOLINFO_NAME " ");
    strcat(szVolInfo, pcszVolume);

    FILE *fp = popen(szVolInfo, "r");
    if (fp)
    {
        char szBuf[128];

        while (fgets(szBuf, sizeof(szBuf), fp))
            listDisks.push_back(RTCString(trimTrailingDigits(szBuf)));

        pclose(fp);
    }
    else
        listDisks.push_back(RTCString(pcszVolume));
}

int CollectorLinux::getDiskListByFs(const char *pszPath, DiskList& listDisks)
{
    FILE *mtab = setmntent("/etc/mtab", "r");
    if (mtab)
    {
        struct mntent *mntent;
        while ((mntent = getmntent(mtab)))
        {
            if (strcmp(pszPath, mntent->mnt_dir) == 0)
            {
                char szDevName[128];
                if (strncmp(mntent->mnt_fsname, "/dev/mapper", 11))
                {
                    getDiskName(szDevName, sizeof(szDevName), mntent->mnt_fsname, true);
                    listDisks.push_back(RTCString(szDevName));
                }
                else
                {
                    getDiskName(szDevName, sizeof(szDevName), mntent->mnt_fsname, false);
                    addVolumeDependencies(szDevName, listDisks);
                }
                break;
            }
        }
        endmntent(mtab);
    }
    return VINF_SUCCESS;
}

}

