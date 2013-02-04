/* $Id$ */
/** @file
 * VBoxStats - Guest statistics notification
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#if defined(RT_OS_WINDOWS)
# ifdef TARGET_NT4
#  undef _WIN32_WINNT
#  define _WIN32_WINNT 0x501
# endif
# include <windows.h>
# include <psapi.h>
# include <winternl.h>

#elif defined(RT_OS_LINUX)
# include <iprt/ctype.h>
# include <iprt/stream.h>
# include <unistd.h>

#elif defined(RT_OS_SOLARIS)
# include <kstat.h>
# include <sys/sysinfo.h>
# include <unistd.h>
#else
/** @todo port me. */

#endif

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <VBox/param.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <iprt/thread.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct _VBOXSTATSCONTEXT
{
    RTMSINTERVAL          cMsStatInterval;

    uint64_t              au64LastCpuLoad_Idle[VMM_MAX_CPU_COUNT];
    uint64_t              au64LastCpuLoad_Kernel[VMM_MAX_CPU_COUNT];
    uint64_t              au64LastCpuLoad_User[VMM_MAX_CPU_COUNT];
    uint64_t              au64LastCpuLoad_Nice[VMM_MAX_CPU_COUNT];

#ifdef RT_OS_WINDOWS
    NTSTATUS (WINAPI *pfnNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
    void     (WINAPI *pfnGlobalMemoryStatusEx)(LPMEMORYSTATUSEX lpBuffer);
    BOOL     (WINAPI *pfnGetPerformanceInfo)(PPERFORMANCE_INFORMATION pPerformanceInformation, DWORD cb);
#endif
} VBOXSTATSCONTEXT;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static VBOXSTATSCONTEXT gCtx = {0};

/** The semaphore we're blocking on. */
static RTSEMEVENTMULTI  g_VMStatEvent = NIL_RTSEMEVENTMULTI;


/** @copydoc VBOXSERVICE::pfnPreInit */
static DECLCALLBACK(int) VBoxServiceVMStatsPreInit(void)
{
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnOption */
static DECLCALLBACK(int) VBoxServiceVMStatsOption(const char **ppszShort, int argc, char **argv, int *pi)
{
    NOREF(ppszShort);
    NOREF(argc);
    NOREF(argv);
    NOREF(pi);

    return -1;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceVMStatsInit(void)
{
    VBoxServiceVerbose(3, "VBoxServiceVMStatsInit\n");

    int rc = RTSemEventMultiCreate(&g_VMStatEvent);
    AssertRCReturn(rc, rc);

    gCtx.cMsStatInterval        = 0;     /* default; update disabled */
    RT_ZERO(gCtx.au64LastCpuLoad_Idle);
    RT_ZERO(gCtx.au64LastCpuLoad_Kernel);
    RT_ZERO(gCtx.au64LastCpuLoad_User);
    RT_ZERO(gCtx.au64LastCpuLoad_Nice);

    rc = VbglR3StatQueryInterval(&gCtx.cMsStatInterval);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "VBoxStatsInit: New statistics interval %u seconds\n", gCtx.cMsStatInterval);
    else
        VBoxServiceVerbose(3, "VBoxStatsInit: DeviceIoControl failed with %d\n", rc);

#ifdef RT_OS_WINDOWS
    /** @todo Use RTLdr instead of LoadLibrary/GetProcAddress here! */

    /* NtQuerySystemInformation might be dropped in future releases, so load it dynamically as per Microsoft's recommendation */
    HMODULE hMod = LoadLibrary("NTDLL.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnNtQuerySystemInformation = (uintptr_t)GetProcAddress(hMod, "NtQuerySystemInformation");
        if (gCtx.pfnNtQuerySystemInformation)
            VBoxServiceVerbose(3, "VBoxStatsInit: gCtx.pfnNtQuerySystemInformation = %x\n", gCtx.pfnNtQuerySystemInformation);
        else
        {
            VBoxServiceVerbose(3, "VBoxStatsInit: NTDLL.NtQuerySystemInformation not found!\n");
            return VERR_SERVICE_DISABLED;
        }
    }

    /* GlobalMemoryStatus is win2k and up, so load it dynamically */
    hMod = LoadLibrary("KERNEL32.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnGlobalMemoryStatusEx = (uintptr_t)GetProcAddress(hMod, "GlobalMemoryStatusEx");
        if (gCtx.pfnGlobalMemoryStatusEx)
            VBoxServiceVerbose(3, "VBoxStatsInit: gCtx.GlobalMemoryStatusEx = %x\n", gCtx.pfnGlobalMemoryStatusEx);
        else
        {
            /** @todo Now fails in NT4; do we care? */
            VBoxServiceVerbose(3, "VBoxStatsInit: KERNEL32.GlobalMemoryStatusEx not found!\n");
            return VERR_SERVICE_DISABLED;
        }
    }
    /* GetPerformanceInfo is xp and up, so load it dynamically */
    hMod = LoadLibrary("PSAPI.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnGetPerformanceInfo = (uintptr_t)GetProcAddress(hMod, "GetPerformanceInfo");
        if (gCtx.pfnGetPerformanceInfo)
            VBoxServiceVerbose(3, "VBoxStatsInit: gCtx.pfnGetPerformanceInfo= %x\n", gCtx.pfnGetPerformanceInfo);
        /* failure is not fatal */
    }
#endif /* RT_OS_WINDOWS */

    return VINF_SUCCESS;
}


/**
 * Gathers VM statistics and reports them to the host.
 */
static void VBoxServiceVMStatsReport(void)
{
#if defined(RT_OS_WINDOWS)
    SYSTEM_INFO systemInfo;
    PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pProcInfo;
    MEMORYSTATUSEX memStatus;
    uint32_t cbStruct;
    DWORD    cbReturned;

    Assert(gCtx.pfnGlobalMemoryStatusEx && gCtx.pfnNtQuerySystemInformation);
    if (    !gCtx.pfnGlobalMemoryStatusEx
        ||  !gCtx.pfnNtQuerySystemInformation)
        return;

    /* Clear the report so we don't report garbage should NtQuerySystemInformation
       behave in an unexpected manner. */
    VMMDevReportGuestStats req;
    RT_ZERO(req);

    /* Query and report guest statistics */
    GetSystemInfo(&systemInfo);

    memStatus.dwLength = sizeof(memStatus);
    gCtx.pfnGlobalMemoryStatusEx(&memStatus);

    req.guestStats.u32PageSize          = systemInfo.dwPageSize;
    req.guestStats.u32PhysMemTotal      = (uint32_t)(memStatus.ullTotalPhys / _4K);
    req.guestStats.u32PhysMemAvail      = (uint32_t)(memStatus.ullAvailPhys / _4K);
    /* The current size of the committed memory limit, in bytes. This is physical
       memory plus the size of the page file, minus a small overhead. */
    req.guestStats.u32PageFileSize      = (uint32_t)(memStatus.ullTotalPageFile / _4K) - req.guestStats.u32PhysMemTotal;
    req.guestStats.u32MemoryLoad        = memStatus.dwMemoryLoad;
    req.guestStats.u32StatCaps          = VBOX_GUEST_STAT_PHYS_MEM_TOTAL
                                        | VBOX_GUEST_STAT_PHYS_MEM_AVAIL
                                        | VBOX_GUEST_STAT_PAGE_FILE_SIZE
                                        | VBOX_GUEST_STAT_MEMORY_LOAD;
#ifdef VBOX_WITH_MEMBALLOON
    req.guestStats.u32PhysMemBalloon    = VBoxServiceBalloonQueryPages(_4K);
    req.guestStats.u32StatCaps         |= VBOX_GUEST_STAT_PHYS_MEM_BALLOON;
#else
    req.guestStats.u32PhysMemBalloon    = 0;
#endif

    if (gCtx.pfnGetPerformanceInfo)
    {
        PERFORMANCE_INFORMATION perfInfo;

        if (gCtx.pfnGetPerformanceInfo(&perfInfo, sizeof(perfInfo)))
        {
            req.guestStats.u32Processes         = perfInfo.ProcessCount;
            req.guestStats.u32Threads           = perfInfo.ThreadCount;
            req.guestStats.u32Handles           = perfInfo.HandleCount;
            req.guestStats.u32MemCommitTotal    = perfInfo.CommitTotal;     /* already in pages */
            req.guestStats.u32MemKernelTotal    = perfInfo.KernelTotal;     /* already in pages */
            req.guestStats.u32MemKernelPaged    = perfInfo.KernelPaged;     /* already in pages */
            req.guestStats.u32MemKernelNonPaged = perfInfo.KernelNonpaged;  /* already in pages */
            req.guestStats.u32MemSystemCache    = perfInfo.SystemCache;     /* already in pages */
            req.guestStats.u32StatCaps |= VBOX_GUEST_STAT_PROCESSES | VBOX_GUEST_STAT_THREADS | VBOX_GUEST_STAT_HANDLES
                                        | VBOX_GUEST_STAT_MEM_COMMIT_TOTAL | VBOX_GUEST_STAT_MEM_KERNEL_TOTAL
                                        | VBOX_GUEST_STAT_MEM_KERNEL_PAGED | VBOX_GUEST_STAT_MEM_KERNEL_NONPAGED
                                        | VBOX_GUEST_STAT_MEM_SYSTEM_CACHE;
        }
        else
            VBoxServiceVerbose(3, "VBoxServiceVMStatsReport: GetPerformanceInfo failed with %d\n", GetLastError());
    }

    /* Query CPU load information */
    cbStruct = systemInfo.dwNumberOfProcessors * sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
    pProcInfo = (PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)RTMemAlloc(cbStruct);
    if (!pProcInfo)
        return;

    /* Unfortunately GetSystemTimes is XP SP1 and up only, so we need to use the semi-undocumented NtQuerySystemInformation */
    NTSTATUS rc = gCtx.pfnNtQuerySystemInformation(SystemProcessorPerformanceInformation, pProcInfo, cbStruct, &cbReturned);
    if (    !rc
        &&  cbReturned == cbStruct)
    {
        if (gCtx.au64LastCpuLoad_Kernel == 0)
        {
            /* first time */
            gCtx.au64LastCpuLoad_Idle[0]    = pProcInfo->IdleTime.QuadPart;
            gCtx.au64LastCpuLoad_Kernel[0]  = pProcInfo->KernelTime.QuadPart;
            gCtx.au64LastCpuLoad_User[0]    = pProcInfo->UserTime.QuadPart;

            Sleep(250);

            rc = gCtx.pfnNtQuerySystemInformation(SystemProcessorPerformanceInformation, pProcInfo, cbStruct, &cbReturned);
            Assert(!rc);
        }

        uint64_t deltaIdle    = (pProcInfo->IdleTime.QuadPart   - gCtx.au64LastCpuLoad_Idle[0]);
        uint64_t deltaKernel  = (pProcInfo->KernelTime.QuadPart - gCtx.au64LastCpuLoad_Kernel[0]);
        uint64_t deltaUser    = (pProcInfo->UserTime.QuadPart   - gCtx.au64LastCpuLoad_User[0]);
        deltaKernel          -= deltaIdle;  /* idle time is added to kernel time */
        uint64_t ullTotalTime = deltaIdle + deltaKernel + deltaUser;
        if (ullTotalTime == 0) /* Prevent division through zero. */
            ullTotalTime = 1;

        req.guestStats.u32CpuLoad_Idle      = (uint32_t)(deltaIdle  * 100 / ullTotalTime);
        req.guestStats.u32CpuLoad_Kernel    = (uint32_t)(deltaKernel* 100 / ullTotalTime);
        req.guestStats.u32CpuLoad_User      = (uint32_t)(deltaUser  * 100 / ullTotalTime);

        req.guestStats.u32StatCaps |= VBOX_GUEST_STAT_CPU_LOAD_IDLE | VBOX_GUEST_STAT_CPU_LOAD_KERNEL | VBOX_GUEST_STAT_CPU_LOAD_USER;

        gCtx.au64LastCpuLoad_Idle[0]   = pProcInfo->IdleTime.QuadPart;
        gCtx.au64LastCpuLoad_Kernel[0] = pProcInfo->KernelTime.QuadPart;
        gCtx.au64LastCpuLoad_User[0]   = pProcInfo->UserTime.QuadPart;
        /** @todo SMP: report details for each CPU?  */
    }

    for (uint32_t i = 0; i < systemInfo.dwNumberOfProcessors; i++)
    {
        req.guestStats.u32CpuId = i;

        rc = VbglR3StatReport(&req);
        if (RT_SUCCESS(rc))
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: new statistics (CPU %u) reported successfully!\n", i);
        else
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: DeviceIoControl (stats report) failed with %d\n", GetLastError());
    }

    RTMemFree(pProcInfo);

#elif defined(RT_OS_LINUX)
    VMMDevReportGuestStats req;
    RT_ZERO(req);
    PRTSTREAM pStrm;
    char szLine[256];
    char *psz;

    int rc = RTStrmOpen("/proc/meminfo", "r", &pStrm);
    if (RT_SUCCESS(rc))
    {
        uint64_t u64Kb;
        uint64_t u64Total = 0, u64Free = 0, u64Buffers = 0, u64Cached = 0, u64PagedTotal = 0;
        for (;;)
        {
            rc = RTStrmGetLine(pStrm, szLine, sizeof(szLine));
            if (RT_FAILURE(rc))
                break;
            if (strstr(szLine, "MemTotal:") == szLine)
            {
                rc = RTStrToUInt64Ex(RTStrStripL(&szLine[9]), &psz, 0, &u64Kb);
                if (RT_SUCCESS(rc))
                    u64Total = u64Kb * _1K;
            }
            else if (strstr(szLine, "MemFree:") == szLine)
            {
                rc = RTStrToUInt64Ex(RTStrStripL(&szLine[8]), &psz, 0, &u64Kb);
                if (RT_SUCCESS(rc))
                    u64Free = u64Kb * _1K;
            }
            else if (strstr(szLine, "Buffers:") == szLine)
            {
                rc = RTStrToUInt64Ex(RTStrStripL(&szLine[8]), &psz, 0, &u64Kb);
                if (RT_SUCCESS(rc))
                    u64Buffers = u64Kb * _1K;
            }
            else if (strstr(szLine, "Cached:") == szLine)
            {
                rc = RTStrToUInt64Ex(RTStrStripL(&szLine[7]), &psz, 0, &u64Kb);
                if (RT_SUCCESS(rc))
                    u64Cached = u64Kb * _1K;
            }
            else if (strstr(szLine, "SwapTotal:") == szLine)
            {
                rc = RTStrToUInt64Ex(RTStrStripL(&szLine[10]), &psz, 0, &u64Kb);
                if (RT_SUCCESS(rc))
                    u64PagedTotal = u64Kb * _1K;
            }
        }
        req.guestStats.u32PhysMemTotal   = u64Total / _4K;
        req.guestStats.u32PhysMemAvail   = (u64Free + u64Buffers + u64Cached) / _4K;
        req.guestStats.u32MemSystemCache = (u64Buffers + u64Cached) / _4K;
        req.guestStats.u32PageFileSize   = u64PagedTotal / _4K;
        RTStrmClose(pStrm);
    }
    else
        VBoxServiceVerbose(3, "VBoxStatsReportStatistics: memory info not available!\n");

    req.guestStats.u32PageSize = getpagesize();
    req.guestStats.u32StatCaps  = VBOX_GUEST_STAT_PHYS_MEM_TOTAL
                                | VBOX_GUEST_STAT_PHYS_MEM_AVAIL
                                | VBOX_GUEST_STAT_MEM_SYSTEM_CACHE
                                | VBOX_GUEST_STAT_PAGE_FILE_SIZE;
#ifdef VBOX_WITH_MEMBALLOON
    req.guestStats.u32PhysMemBalloon  = VBoxServiceBalloonQueryPages(_4K);
    req.guestStats.u32StatCaps       |= VBOX_GUEST_STAT_PHYS_MEM_BALLOON;
#else
    req.guestStats.u32PhysMemBalloon  = 0;
#endif


    /** @todo req.guestStats.u32Threads */
    /** @todo req.guestStats.u32Processes */
    /* req.guestStats.u32Handles doesn't make sense here. */
    /** @todo req.guestStats.u32MemoryLoad */
    /** @todo req.guestStats.u32MemCommitTotal */
    /** @todo req.guestStats.u32MemKernelTotal */
    /** @todo req.guestStats.u32MemKernelPaged, make any sense?  = u32MemKernelTotal? */
    /** @todo req.guestStats.u32MemKernelNonPaged, make any sense? = 0? */

    bool fCpuInfoAvail = false;
    rc = RTStrmOpen("/proc/stat", "r", &pStrm);
    if (RT_SUCCESS(rc))
    {
        for (;;)
        {
            rc = RTStrmGetLine(pStrm, szLine, sizeof(szLine));
            if (RT_FAILURE(rc))
                break;
            if (   strstr(szLine, "cpu") == szLine
                && strlen(szLine) > 3
                && RT_C_IS_DIGIT(szLine[3]))
            {
                uint32_t u32CpuId;
                rc = RTStrToUInt32Ex(&szLine[3], &psz, 0, &u32CpuId);
                if (u32CpuId < VMM_MAX_CPU_COUNT)
                {
                    uint64_t u64User = 0;
                    if (RT_SUCCESS(rc))
                        rc = RTStrToUInt64Ex(RTStrStripL(psz), &psz, 0, &u64User);

                    uint64_t u64Nice = 0;
                    if (RT_SUCCESS(rc))
                        rc = RTStrToUInt64Ex(RTStrStripL(psz), &psz, 0, &u64Nice);

                    uint64_t u64System = 0;
                    if (RT_SUCCESS(rc))
                        rc = RTStrToUInt64Ex(RTStrStripL(psz), &psz, 0, &u64System);

                    uint64_t u64Idle = 0;
                    if (RT_SUCCESS(rc))
                        rc = RTStrToUInt64Ex(RTStrStripL(psz), &psz, 0, &u64Idle);

                    uint64_t u64DeltaIdle   = u64Idle   - gCtx.au64LastCpuLoad_Idle[u32CpuId];
                    uint64_t u64DeltaSystem = u64System - gCtx.au64LastCpuLoad_Kernel[u32CpuId];
                    uint64_t u64DeltaUser   = u64User   - gCtx.au64LastCpuLoad_User[u32CpuId];
                    uint64_t u64DeltaNice   = u64Nice   - gCtx.au64LastCpuLoad_Nice[u32CpuId];

                    uint64_t u64DeltaAll    = u64DeltaIdle
                                            + u64DeltaSystem
                                            + u64DeltaUser
                                            + u64DeltaNice;
                    if (u64DeltaAll == 0) /* Prevent division through zero. */
                        u64DeltaAll = 1;

                    gCtx.au64LastCpuLoad_Idle[u32CpuId]   = u64Idle;
                    gCtx.au64LastCpuLoad_Kernel[u32CpuId] = u64System;
                    gCtx.au64LastCpuLoad_User[u32CpuId]   = u64User;
                    gCtx.au64LastCpuLoad_Nice[u32CpuId]   = u64Nice;

                    req.guestStats.u32CpuId = u32CpuId;
                    req.guestStats.u32CpuLoad_Idle   = (uint32_t)(u64DeltaIdle   * 100 / u64DeltaAll);
                    req.guestStats.u32CpuLoad_Kernel = (uint32_t)(u64DeltaSystem * 100 / u64DeltaAll);
                    req.guestStats.u32CpuLoad_User   = (uint32_t)((u64DeltaUser
                                                                 + u64DeltaNice) * 100 / u64DeltaAll);
                    req.guestStats.u32StatCaps |= VBOX_GUEST_STAT_CPU_LOAD_IDLE
                                               |  VBOX_GUEST_STAT_CPU_LOAD_KERNEL
                                               |  VBOX_GUEST_STAT_CPU_LOAD_USER;
                    fCpuInfoAvail = true;
                    rc = VbglR3StatReport(&req);
                    if (RT_SUCCESS(rc))
                        VBoxServiceVerbose(3, "VBoxStatsReportStatistics: new statistics (CPU %u) reported successfully!\n", u32CpuId);
                    else
                        VBoxServiceVerbose(3, "VBoxStatsReportStatistics: stats report failed with rc=%Rrc\n", rc);
                }
                else
                    VBoxServiceVerbose(3, "VBoxStatsReportStatistics: skipping information for CPU%u\n", u32CpuId);
            }
        }
        RTStrmClose(pStrm);
    }
    if (!fCpuInfoAvail)
    {
        VBoxServiceVerbose(3, "VBoxStatsReportStatistics: CPU info not available!\n");
        rc = VbglR3StatReport(&req);
        if (RT_SUCCESS(rc))
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: new statistics reported successfully!\n");
        else
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: stats report failed with rc=%Rrc\n", rc);
    }

#elif defined(RT_OS_SOLARIS)
    VMMDevReportGuestStats req;
    RT_ZERO(req);
    kstat_ctl_t *pStatKern = kstat_open();
    if (pStatKern)
    {
        /*
         * Memory statistics.
         */
        uint64_t u64Total = 0, u64Free = 0, u64Buffers = 0, u64Cached = 0, u64PagedTotal = 0;
        int rc = -1;
        kstat_t *pStatPages = kstat_lookup(pStatKern, "unix", 0 /* instance */, "system_pages");
        if (pStatPages)
        {
            rc = kstat_read(pStatKern, pStatPages, NULL /* optional-copy-buf */);
            if (rc != -1)
            {
                kstat_named_t *pStat = NULL;
                pStat = (kstat_named_t *)kstat_data_lookup(pStatPages, "pagestotal");
                if (pStat)
                    u64Total = pStat->value.ul;

                pStat = (kstat_named_t *)kstat_data_lookup(pStatPages, "freemem");
                if (pStat)
                    u64Free = pStat->value.ul;
            }
        }

        kstat_t *pStatZFS = kstat_lookup(pStatKern, "zfs", 0 /* instance */, "arcstats");
        if (pStatZFS)
        {
            rc = kstat_read(pStatKern, pStatZFS, NULL /* optional-copy-buf */);
            if (rc != -1)
            {
                kstat_named_t *pStat = (kstat_named_t *)kstat_data_lookup(pStatZFS, "size");
                if (pStat)
                    u64Cached = pStat->value.ul;
            }
        }

        /*
         * The vminfo are accumulative counters updated every "N" ticks. Let's get the
         * number of stat updates so far and use that to divide the swap counter.
         */
        kstat_t *pStatInfo = kstat_lookup(pStatKern, "unix", 0 /* instance */, "sysinfo");
        if (pStatInfo)
        {
            sysinfo_t SysInfo;
            rc = kstat_read(pStatKern, pStatInfo, &SysInfo);
            if (rc != -1)
            {
                kstat_t *pStatVMInfo = kstat_lookup(pStatKern, "unix", 0 /* instance */, "vminfo");
                if (pStatVMInfo)
                {
                    vminfo_t VMInfo;
                    rc = kstat_read(pStatKern, pStatVMInfo, &VMInfo);
                    if (rc != -1)
                    {
                        Assert(SysInfo.updates != 0);
                        u64PagedTotal = VMInfo.swap_avail / SysInfo.updates;
                    }
                }
            }
        }

        req.guestStats.u32PhysMemTotal   = u64Total;        /* already in pages */
        req.guestStats.u32PhysMemAvail   = u64Free;         /* already in pages */
        req.guestStats.u32MemSystemCache = u64Cached / _4K;
        req.guestStats.u32PageFileSize   = u64PagedTotal;   /* already in pages */
        /** @todo req.guestStats.u32Threads */
        /** @todo req.guestStats.u32Processes */
        /** @todo req.guestStats.u32Handles -- ??? */
        /** @todo req.guestStats.u32MemoryLoad */
        /** @todo req.guestStats.u32MemCommitTotal */
        /** @todo req.guestStats.u32MemKernelTotal */
        /** @todo req.guestStats.u32MemKernelPaged */
        /** @todo req.guestStats.u32MemKernelNonPaged */
        req.guestStats.u32PageSize = getpagesize();

        req.guestStats.u32StatCaps = VBOX_GUEST_STAT_PHYS_MEM_TOTAL
                                   | VBOX_GUEST_STAT_PHYS_MEM_AVAIL
                                   | VBOX_GUEST_STAT_MEM_SYSTEM_CACHE
                                   | VBOX_GUEST_STAT_PAGE_FILE_SIZE;
#ifdef VBOX_WITH_MEMBALLOON
        req.guestStats.u32PhysMemBalloon  = VBoxServiceBalloonQueryPages(_4K);
        req.guestStats.u32StatCaps       |= VBOX_GUEST_STAT_PHYS_MEM_BALLOON;
#else
        req.guestStats.u32PhysMemBalloon  = 0;
#endif

        /*
         * CPU statistics.
         */
        cpu_stat_t StatCPU;
        RT_ZERO(StatCPU);
        kstat_t *pStatNode = NULL;
        uint32_t cCPUs = 0;
        bool fCpuInfoAvail = false;
        for (pStatNode = pStatKern->kc_chain; pStatNode != NULL; pStatNode = pStatNode->ks_next)
        {
            if (!strcmp(pStatNode->ks_module, "cpu_stat"))
            {
                rc = kstat_read(pStatKern, pStatNode, &StatCPU);
                if (rc == -1)
                    break;

                uint64_t u64Idle   = StatCPU.cpu_sysinfo.cpu[CPU_IDLE];
                uint64_t u64User   = StatCPU.cpu_sysinfo.cpu[CPU_USER];
                uint64_t u64System = StatCPU.cpu_sysinfo.cpu[CPU_KERNEL];

                uint64_t u64DeltaIdle   = u64Idle   - gCtx.au64LastCpuLoad_Idle[cCPUs];
                uint64_t u64DeltaSystem = u64System - gCtx.au64LastCpuLoad_Kernel[cCPUs];
                uint64_t u64DeltaUser   = u64User   - gCtx.au64LastCpuLoad_User[cCPUs];

                uint64_t u64DeltaAll    = u64DeltaIdle + u64DeltaSystem + u64DeltaUser;
                if (u64DeltaAll == 0) /* Prevent division through zero. */
                    u64DeltaAll = 1;

                gCtx.au64LastCpuLoad_Idle[cCPUs]   = u64Idle;
                gCtx.au64LastCpuLoad_Kernel[cCPUs] = u64System;
                gCtx.au64LastCpuLoad_User[cCPUs]   = u64User;

                req.guestStats.u32CpuId = cCPUs;
                req.guestStats.u32CpuLoad_Idle   = (uint32_t)(u64DeltaIdle   * 100 / u64DeltaAll);
                req.guestStats.u32CpuLoad_Kernel = (uint32_t)(u64DeltaSystem * 100 / u64DeltaAll);
                req.guestStats.u32CpuLoad_User   = (uint32_t)(u64DeltaUser   * 100 / u64DeltaAll);

                req.guestStats.u32StatCaps |= VBOX_GUEST_STAT_CPU_LOAD_IDLE
                                           |  VBOX_GUEST_STAT_CPU_LOAD_KERNEL
                                           |  VBOX_GUEST_STAT_CPU_LOAD_USER;
                fCpuInfoAvail = true;

                rc = VbglR3StatReport(&req);
                if (RT_SUCCESS(rc))
                    VBoxServiceVerbose(3, "VBoxStatsReportStatistics: new statistics (CPU %u) reported successfully!\n", cCPUs);
                else
                    VBoxServiceVerbose(3, "VBoxStatsReportStatistics: stats report failed with rc=%Rrc\n", rc);
                cCPUs++;
            }
        }

        /*
         * Report whatever statistics were collected.
         */
        if (!fCpuInfoAvail)
        {
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: CPU info not available!\n");
            rc = VbglR3StatReport(&req);
            if (RT_SUCCESS(rc))
                VBoxServiceVerbose(3, "VBoxStatsReportStatistics: new statistics reported successfully!\n");
            else
                VBoxServiceVerbose(3, "VBoxStatsReportStatistics: stats report failed with rc=%Rrc\n", rc);
        }

        kstat_close(pStatKern);
    }

#else
    /* todo: implement for other platforms. */

#endif
}

/** @copydoc VBOXSERVICE::pfnWorker */
DECLCALLBACK(int) VBoxServiceVMStatsWorker(bool volatile *pfShutdown)
{
    int rc = VINF_SUCCESS;

    /* Start monitoring of the stat event change event. */
    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST, 0);
    if (RT_FAILURE(rc))
    {
        VBoxServiceVerbose(3, "VBoxServiceVMStatsWorker: VbglR3CtlFilterMask failed with %d\n", rc);
        return rc;
    }

    /*
     * Tell the control thread that it can continue
     * spawning services.
     */
    RTThreadUserSignal(RTThreadSelf());

    /*
     * Now enter the loop retrieving runtime data continuously.
     */
    for (;;)
    {
        uint32_t fEvents = 0;
        RTMSINTERVAL cWaitMillies;

        /* Check if an update interval change is pending. */
        rc = VbglR3WaitEvent(VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST, 0 /* no wait */, &fEvents);
        if (    RT_SUCCESS(rc)
            &&  (fEvents & VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST))
        {
            VbglR3StatQueryInterval(&gCtx.cMsStatInterval);
        }

        if (gCtx.cMsStatInterval)
        {
            VBoxServiceVMStatsReport();
            cWaitMillies = gCtx.cMsStatInterval;
        }
        else
            cWaitMillies = 3000;

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_VMStatEvent, cWaitMillies);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("VBoxServiceVMStatsWorker: RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
            rc = rc2;
            break;
        }
    }

    /* Cancel monitoring of the stat event change event. */
    rc = VbglR3CtlFilterMask(0, VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST);
    if (RT_FAILURE(rc))
        VBoxServiceVerbose(3, "VBoxServiceVMStatsWorker: VbglR3CtlFilterMask failed with %d\n", rc);

    RTSemEventMultiDestroy(g_VMStatEvent);
    g_VMStatEvent = NIL_RTSEMEVENTMULTI;

    VBoxServiceVerbose(3, "VBoxStatsThread: finished statistics change request thread\n");
    return 0;
}


/** @copydoc VBOXSERVICE::pfnTerm */
static DECLCALLBACK(void) VBoxServiceVMStatsTerm(void)
{
    VBoxServiceVerbose(3, "VBoxServiceVMStatsTerm\n");
    return;
}


/** @copydoc VBOXSERVICE::pfnStop */
static DECLCALLBACK(void) VBoxServiceVMStatsStop(void)
{
    RTSemEventMultiSignal(g_VMStatEvent);
}


/**
 * The 'vminfo' service description.
 */
VBOXSERVICE g_VMStatistics =
{
    /* pszName. */
    "vmstats",
    /* pszDescription. */
    "Virtual Machine Statistics",
    /* pszUsage. */
    NULL,
    /* pszOptions. */
    NULL,
    /* methods */
    VBoxServiceVMStatsPreInit,
    VBoxServiceVMStatsOption,
    VBoxServiceVMStatsInit,
    VBoxServiceVMStatsWorker,
    VBoxServiceVMStatsStop,
    VBoxServiceVMStatsTerm
};
