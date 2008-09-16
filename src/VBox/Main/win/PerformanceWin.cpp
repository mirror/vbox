/* $Id$ */

/** @file
 *
 * VBox Windows-specific Performance Classes implementation.
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

/* @todo Replace the following _WIN32_WINNT override with proper diagnostic:
#if (_WIN32_WINNT < 0x0501)
#error Win XP or later required!
#endif
*/
#ifdef _WIN32_WINNT
#if (_WIN32_WINNT < 0x0501)
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0501
#endif
#else
#define _WIN32_WINNT 0x0501
#endif
#include <windows.h>

#include <psapi.h>
extern "C" {
#include <powrprof.h>
}
#include <iprt/err.h>
#include <iprt/mp.h>

#include <map>

#include "Logging.h"
#include "Performance.h"

namespace pm {

class CollectorWin : public CollectorHAL
{
public:
    CollectorWin();
    virtual ~CollectorWin();

    virtual int preCollect(const CollectorHints& hints);
    virtual int getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle);
    virtual int getHostCpuMHz(ULONG *mhz);
    virtual int getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available);
    virtual int getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel);
    virtual int getProcessMemoryUsage(RTPROCESS process, ULONG *used);

    virtual int getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle);
    virtual int getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total);
private:
    struct VMProcessStats
    {
        uint64_t cpuUser;
        uint64_t cpuKernel;
        uint64_t cpuTotal;
        uint64_t ramUsed;
    };

    typedef std::map<RTPROCESS, VMProcessStats> VMProcessMap;

    VMProcessMap       mProcessStats;
};

CollectorHAL *createHAL()
{
    return new CollectorWin();
}

CollectorWin::CollectorWin()
{
}

CollectorWin::~CollectorWin()
{

}


#define FILETTIME_TO_100NS(ft) (((uint64_t)ft.dwHighDateTime << 32) + ft.dwLowDateTime)

int CollectorWin::preCollect(const CollectorHints& hints)
{
    LogFlowThisFuncEnter();

    uint64_t user, kernel, idle, total;
    int rc = getRawHostCpuLoad(&user, &kernel, &idle);
    if (RT_FAILURE(rc))
        return rc;
    total = user + kernel + idle;

    DWORD dwError;
    const CollectorHints::ProcessList& processes = hints.getProcessFlags();
    CollectorHints::ProcessList::const_iterator it;

    mProcessStats.clear();

    for (it = processes.begin(); it != processes.end() && RT_SUCCESS(rc); it++)
    {
        RTPROCESS process = it->first;
        HANDLE h = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ,
                               FALSE, process);

        if (!h)
        {
            dwError = GetLastError();
            Log (("OpenProcess() -> 0x%x\n", dwError));
            rc = RTErrConvertFromWin32(dwError);
            break;
        }

        VMProcessStats vmStats;
        if ((it->second & COLLECT_CPU_LOAD) != 0)
        {
            FILETIME ftCreate, ftExit, ftKernel, ftUser;
            if (!GetProcessTimes(h, &ftCreate, &ftExit, &ftKernel, &ftUser))
            {
                dwError = GetLastError();
                Log (("GetProcessTimes() -> 0x%x\n", dwError));
                rc = RTErrConvertFromWin32(dwError);
            }
            else
            {
                vmStats.cpuKernel = FILETTIME_TO_100NS(ftKernel);
                vmStats.cpuUser   = FILETTIME_TO_100NS(ftUser);
                vmStats.cpuTotal  = total;
            }
        }
        if (RT_SUCCESS(rc) && (it->second & COLLECT_RAM_USAGE) != 0)
        {
            PROCESS_MEMORY_COUNTERS pmc;
            if (!GetProcessMemoryInfo(h, &pmc, sizeof(pmc)))
            {
                dwError = GetLastError();
                Log (("GetProcessMemoryInfo() -> 0x%x\n", dwError));
                rc = RTErrConvertFromWin32(dwError);
            }
            else
                vmStats.ramUsed = pmc.WorkingSetSize;
        }
        CloseHandle(h);
        mProcessStats[process] = vmStats;
    }

    LogFlowThisFuncLeave();

    return rc;
}

int CollectorWin::getHostCpuLoad(ULONG *user, ULONG *kernel, ULONG *idle)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorWin::getRawHostCpuLoad(uint64_t *user, uint64_t *kernel, uint64_t *idle)
{
    LogFlowThisFuncEnter();

    FILETIME ftIdle, ftKernel, ftUser;

    if (!GetSystemTimes(&ftIdle, &ftKernel, &ftUser))
    {
        DWORD dwError = GetLastError();
        Log (("GetSystemTimes() -> 0x%x\n", dwError));
        return RTErrConvertFromWin32(dwError);
    }

    *user   = FILETTIME_TO_100NS(ftUser);
    *idle   = FILETTIME_TO_100NS(ftIdle);
    *kernel = FILETTIME_TO_100NS(ftKernel) - *idle;

    LogFlowThisFunc(("user=%lu kernel=%lu idle=%lu\n", *user, *kernel, *idle));
    LogFlowThisFuncLeave();

    return VINF_SUCCESS;
}

typedef struct _PROCESSOR_POWER_INFORMATION {
  ULONG  Number;
  ULONG  MaxMhz;
  ULONG  CurrentMhz;
  ULONG  MhzLimit;
  ULONG  MaxIdleState;
  ULONG  CurrentIdleState;
} PROCESSOR_POWER_INFORMATION , *PPROCESSOR_POWER_INFORMATION;

int CollectorWin::getHostCpuMHz(ULONG *mhz)
{
    uint64_t uTotalMhz   = 0;
    RTCPUID  nProcessors = RTMpGetCount();
    PPROCESSOR_POWER_INFORMATION ppi = new PROCESSOR_POWER_INFORMATION[nProcessors];
    LONG ns = CallNtPowerInformation(ProcessorInformation, NULL, 0, ppi,
        nProcessors * sizeof(PROCESSOR_POWER_INFORMATION));
    if (ns)
    {
        Log(("CallNtPowerInformation() -> %x\n", ns));
        return VERR_INTERNAL_ERROR;
    }

    /* Compute an average over all CPUs */
    for (unsigned i = 0; i < nProcessors;  i++)
        uTotalMhz += ppi[i].CurrentMhz;
    *mhz = (ULONG)(uTotalMhz / nProcessors);

    LogFlowThisFunc(("mhz=%u\n", *mhz));
    LogFlowThisFuncLeave();

    return VINF_SUCCESS;
}

int CollectorWin::getHostMemoryUsage(ULONG *total, ULONG *used, ULONG *available)
{
    MEMORYSTATUSEX mstat;

    mstat.dwLength = sizeof(mstat);
    if (GlobalMemoryStatusEx(&mstat))
    {
        *total = (ULONG)( mstat.ullTotalPhys / 1000 );
        *available = (ULONG)( mstat.ullAvailPhys / 1000 );
        *used = *total - *available;
    }
    else
        return RTErrConvertFromWin32(GetLastError());

    return VINF_SUCCESS;
}

int CollectorWin::getProcessCpuLoad(RTPROCESS process, ULONG *user, ULONG *kernel)
{
    return VERR_NOT_IMPLEMENTED;
}

int CollectorWin::getRawProcessCpuLoad(RTPROCESS process, uint64_t *user, uint64_t *kernel, uint64_t *total)
{
    VMProcessMap::const_iterator it = mProcessStats.find(process);

    if (it == mProcessStats.end())
    {
        Log (("No stats pre-collected for process %x\n", process));
        return VERR_INTERNAL_ERROR;
    }
    *user   = it->second.cpuUser;
    *kernel = it->second.cpuKernel;
    *total  = it->second.cpuTotal;
    return VINF_SUCCESS;
}

int CollectorWin::getProcessMemoryUsage(RTPROCESS process, ULONG *used)
{
    VMProcessMap::const_iterator it = mProcessStats.find(process);

    if (it == mProcessStats.end())
    {
        Log (("No stats pre-collected for process %x\n", process));
        return VERR_INTERNAL_ERROR;
    }
    *used = (ULONG)(it->second.ramUsed / 1024);
    return VINF_SUCCESS;
}

}
