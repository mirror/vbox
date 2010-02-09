/* $Id: $ */
/** @file
 * VBoxStats - Guest statistics notification
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */
#ifdef TARGET_NT4
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x501
#endif

#include <windows.h>
#include <psapi.h>
#include <winternl.h>

#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/thread.h>
#include <iprt/string.h>
#include <iprt/semaphore.h>
#include <iprt/system.h>
#include <iprt/time.h>
#include <VBox/VBoxGuestLib.h>
#include "VBoxServiceInternal.h"
#include "VBoxServiceUtils.h"

typedef struct _VBOXSTATSCONTEXT
{
    uint32_t              uStatInterval;

    uint64_t              ullLastCpuLoad_Idle;
    uint64_t              ullLastCpuLoad_Kernel;
    uint64_t              ullLastCpuLoad_User;

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
    return VINF_SUCCESS;
}


/** @copydoc VBOXSERVICE::pfnInit */
static DECLCALLBACK(int) VBoxServiceVMStatsInit(void)
{
    VBoxServiceVerbose(3, "VBoxServiceVMStatsInit\n");

    int rc = RTSemEventMultiCreate(&g_VMStatEvent);
    AssertRCReturn(rc, rc);

    gCtx.uStatInterval          = 0;     /* default; update disabled */
    gCtx.ullLastCpuLoad_Idle    = 0;
    gCtx.ullLastCpuLoad_Kernel  = 0;
    gCtx.ullLastCpuLoad_User    = 0;

    rc = VbglR3StatQueryInterval(&gCtx.uStatInterval);
    if (RT_SUCCESS(rc))
        VBoxServiceVerbose(3, "VBoxStatsInit: new statistics interval %d seconds\n", gCtx.uStatInterval);
    else
        VBoxServiceVerbose(3, "VBoxStatsInit: DeviceIoControl failed with %d\n", rc);

#ifdef RT_OS_WINDOWS
    /* NtQuerySystemInformation might be dropped in future releases, so load it dynamically as per Microsoft's recommendation */
    HMODULE hMod = LoadLibrary("NTDLL.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnNtQuerySystemInformation = (uintptr_t)GetProcAddress(hMod, "NtQuerySystemInformation");
        if (gCtx.pfnNtQuerySystemInformation)
            VBoxServiceVerbose(3, "VBoxStatsInit: gCtx.pfnNtQuerySystemInformation = %x\n", gCtx.pfnNtQuerySystemInformation);
        else
        {
            VBoxServiceError("VBoxStatsInit: NTDLL.NtQuerySystemInformation not found!!\n");
            return VERR_NOT_IMPLEMENTED;
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
            /** @todo now fails in NT4; do we care? */
            VBoxServiceError("VBoxStatsInit: KERNEL32.GlobalMemoryStatusEx not found!!\n");
            return VERR_NOT_IMPLEMENTED;
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


static void VBoxServiceVMStatsReport()
{
#ifdef RT_OS_WINDOWS
    SYSTEM_INFO systemInfo;
    PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pProcInfo;
    MEMORYSTATUSEX memStatus;
    VMMDevReportGuestStats req;
    uint32_t cbStruct;
    DWORD    cbReturned;

    Assert(gCtx.pfnGlobalMemoryStatusEx && gCtx.pfnNtQuerySystemInformation);
    if (    !gCtx.pfnGlobalMemoryStatusEx
        ||  !gCtx.pfnNtQuerySystemInformation)
        return;

    /* Query and report guest statistics */
    GetSystemInfo(&systemInfo);

    memStatus.dwLength = sizeof(memStatus);
    gCtx.pfnGlobalMemoryStatusEx(&memStatus);

    req.guestStats.u32PageSize          = systemInfo.dwPageSize;
    req.guestStats.u32PhysMemTotal      = (uint32_t)(memStatus.ullTotalPhys / systemInfo.dwPageSize);
    req.guestStats.u32PhysMemAvail      = (uint32_t)(memStatus.ullAvailPhys / systemInfo.dwPageSize);
    /* The current size of the committed memory limit, in bytes. This is physical memory plus the size of the page file, minus a small overhead. */
    req.guestStats.u32PageFileSize      = (uint32_t)(memStatus.ullTotalPageFile / systemInfo.dwPageSize) - req.guestStats.u32PhysMemTotal;
    req.guestStats.u32MemoryLoad        = memStatus.dwMemoryLoad;
    req.guestStats.u32PhysMemBalloon    = VBoxServiceBalloonQuerySize() * (_1M/systemInfo.dwPageSize);    /* was in megabytes */
    req.guestStats.u32StatCaps          = VBOX_GUEST_STAT_PHYS_MEM_TOTAL | VBOX_GUEST_STAT_PHYS_MEM_AVAIL | VBOX_GUEST_STAT_PAGE_FILE_SIZE | VBOX_GUEST_STAT_MEMORY_LOAD | VBOX_GUEST_STAT_PHYS_MEM_BALLOON;

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
            req.guestStats.u32StatCaps |= VBOX_GUEST_STAT_PROCESSES | VBOX_GUEST_STAT_THREADS | VBOX_GUEST_STAT_HANDLES | VBOX_GUEST_STAT_MEM_COMMIT_TOTAL | VBOX_GUEST_STAT_MEM_KERNEL_TOTAL | VBOX_GUEST_STAT_MEM_KERNEL_PAGED | VBOX_GUEST_STAT_MEM_KERNEL_NONPAGED | VBOX_GUEST_STAT_MEM_SYSTEM_CACHE;
        }
        else
            VBoxServiceVerbose(3, "GetPerformanceInfo failed with %d\n", GetLastError());
    }

    /* Query CPU load information */
    cbStruct = systemInfo.dwNumberOfProcessors*sizeof(SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION);
    pProcInfo = (PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION)malloc(cbStruct);
    Assert(pProcInfo);
    if (!pProcInfo)
        return;

    /* Unfortunately GetSystemTimes is XP SP1 and up only, so we need to use the semi-undocumented NtQuerySystemInformation */
    NTSTATUS rc = gCtx.pfnNtQuerySystemInformation(SystemProcessorPerformanceInformation, pProcInfo, cbStruct, &cbReturned);
    if (    !rc
        &&  cbReturned == cbStruct)
    {
        if (gCtx.ullLastCpuLoad_Kernel == 0)
        {
            /* first time */
            gCtx.ullLastCpuLoad_Idle    = pProcInfo->IdleTime.QuadPart;
            gCtx.ullLastCpuLoad_Kernel  = pProcInfo->KernelTime.QuadPart;
            gCtx.ullLastCpuLoad_User    = pProcInfo->UserTime.QuadPart;

            Sleep(250);

            rc = gCtx.pfnNtQuerySystemInformation(SystemProcessorPerformanceInformation, pProcInfo, cbStruct, &cbReturned);
            Assert(!rc);
        }

        uint64_t deltaIdle    = (pProcInfo->IdleTime.QuadPart - gCtx.ullLastCpuLoad_Idle);
        uint64_t deltaKernel  = (pProcInfo->KernelTime.QuadPart - gCtx.ullLastCpuLoad_Kernel);
        uint64_t deltaUser    = (pProcInfo->UserTime.QuadPart - gCtx.ullLastCpuLoad_User);
        deltaKernel          -= deltaIdle;  /* idle time is added to kernel time */
        uint64_t ullTotalTime = deltaIdle + deltaKernel + deltaUser;

        req.guestStats.u32CpuLoad_Idle      = (uint32_t)(deltaIdle  * 100 / ullTotalTime);
        req.guestStats.u32CpuLoad_Kernel    = (uint32_t)(deltaKernel* 100 / ullTotalTime);
        req.guestStats.u32CpuLoad_User      = (uint32_t)(deltaUser  * 100 / ullTotalTime);

        req.guestStats.u32StatCaps |= VBOX_GUEST_STAT_CPU_LOAD_IDLE | VBOX_GUEST_STAT_CPU_LOAD_KERNEL | VBOX_GUEST_STAT_CPU_LOAD_USER;

        gCtx.ullLastCpuLoad_Idle    = pProcInfo->IdleTime.QuadPart;
        gCtx.ullLastCpuLoad_Kernel  = pProcInfo->KernelTime.QuadPart;
        gCtx.ullLastCpuLoad_User    = pProcInfo->UserTime.QuadPart;
    }

    for (uint32_t i=0;i<systemInfo.dwNumberOfProcessors;i++)
    {
        req.guestStats.u32CpuId = i;

        rc = VbglR3StatReport(&req);
        if (RT_SUCCESS(rc))
        {
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: new statistics reported successfully!\n");
        }
        else
            VBoxServiceVerbose(3, "VBoxStatsReportStatistics: DeviceIoControl (stats report) failed with %d\n", GetLastError());
    }

    free(pProcInfo);
#else
    /* todo: implement for other platforms. */
    return;
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
        uint32_t u32WaitMillies;

        /* Check if an update interval change is pending. */
        rc = VbglR3WaitEvent(VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST, 0 /* no wait */, &fEvents);
        if (    RT_SUCCESS(rc)
            &&  (fEvents & VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST))
        {
            VbglR3StatQueryInterval(&gCtx.uStatInterval);
        }

        if (gCtx.uStatInterval)
        {
            VBoxServiceVMStatsReport();
            u32WaitMillies = gCtx.uStatInterval;
        }
        else
            u32WaitMillies = 3000;

        /*
         * Block for a while.
         *
         * The event semaphore takes care of ignoring interruptions and it
         * allows us to implement service wakeup later.
         */
        if (*pfShutdown)
            break;
        int rc2 = RTSemEventMultiWait(g_VMStatEvent, u32WaitMillies);
        if (*pfShutdown)
            break;
        if (rc2 != VERR_TIMEOUT && RT_FAILURE(rc2))
        {
            VBoxServiceError("RTSemEventMultiWait failed; rc2=%Rrc\n", rc2);
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
