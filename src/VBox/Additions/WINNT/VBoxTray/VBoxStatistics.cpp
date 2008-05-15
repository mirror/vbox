/** @file
 *
 * VBoxStats - Guest statistics notification
 *
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
#define _WIN32_WINNT 0x0500
#include <windows.h>
#include <psapi.h>
#include "VBoxTray.h"
#include "VBoxStatistics.h"
#include "VBoxMemBalloon.h"
#include <VBoxDisplay.h>
#include <VBox/VBoxDev.h>
#include <VBox/VBoxGuest.h>
#include <VBoxGuestInternal.h>
#include <iprt/assert.h>
#include "helpers.h"
#include <winternl.h>

typedef struct _VBOXSTATSCONTEXT
{
    const VBOXSERVICEENV *pEnv;
    uint32_t              uStatInterval;

    uint64_t              ullLastCpuLoad_Idle;
    uint64_t              ullLastCpuLoad_Kernel;
    uint64_t              ullLastCpuLoad_User;

    NTSTATUS (WINAPI *pfnNtQuerySystemInformation)(SYSTEM_INFORMATION_CLASS SystemInformationClass, PVOID SystemInformation, ULONG SystemInformationLength, PULONG ReturnLength);
    void     (WINAPI *pfnGlobalMemoryStatusEx)(LPMEMORYSTATUSEX lpBuffer);
    BOOL     (WINAPI *pfnGetPerformanceInfo)(PPERFORMANCE_INFORMATION pPerformanceInformation, DWORD cb);
} VBOXSTATSCONTEXT;


static VBOXSTATSCONTEXT gCtx = {0};


int VBoxStatsInit(const VBOXSERVICEENV *pEnv, void **ppInstance, bool *pfStartThread)
{
    HANDLE gVBoxDriver = pEnv->hDriver;
    DWORD  cbReturned;

    dprintf(("VBoxStatsInit\n"));

    gCtx.pEnv                   = pEnv;
    gCtx.uStatInterval          = 0;     /* default */
    gCtx.ullLastCpuLoad_Idle    = 0;
    gCtx.ullLastCpuLoad_Kernel  = 0;
    gCtx.ullLastCpuLoad_User    = 0;

    VMMDevGetStatisticsChangeRequest req;
    vmmdevInitRequest(&req.header, VMMDevReq_GetStatisticsChangeRequest);
    req.eventAck = 0;

    if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &req, req.header.size, &req, req.header.size, &cbReturned, NULL))
    {
        dprintf(("VBoxStatsInit: new statistics interval %d seconds\n", req.u32StatInterval));
        gCtx.uStatInterval = req.u32StatInterval * 1000;
    }
    else
        dprintf(("VBoxStatsInit: DeviceIoControl failed with %d\n", GetLastError()));

    /* NtQuerySystemInformation might be dropped in future releases, so load it dynamically as per Microsoft's recommendation */
    HMODULE hMod = LoadLibrary("NTDLL.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnNtQuerySystemInformation = (uintptr_t)GetProcAddress(hMod, "NtQuerySystemInformation");
        if (gCtx.pfnNtQuerySystemInformation)
            dprintf(("gCtx.pfnNtQuerySystemInformation = %x\n", gCtx.pfnNtQuerySystemInformation));
        else
        {
            dprintf(("NTDLL.NtQuerySystemInformation not found!!\n"));
            return VERR_NOT_IMPLEMENTED;
        }
    }

    /* GlobalMemoryStatus is win2k and up, so load it dynamically */
    hMod = LoadLibrary("KERNEL32.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnGlobalMemoryStatusEx = (uintptr_t)GetProcAddress(hMod, "GlobalMemoryStatusEx");
        if (gCtx.pfnGlobalMemoryStatusEx)
            dprintf(("gCtx.GlobalMemoryStatusEx = %x\n", gCtx.pfnGlobalMemoryStatusEx));
        else
        {
            /** @todo now fails in NT4; do we care? */
            dprintf(("KERNEL32.GlobalMemoryStatusEx not found!!\n"));
            return VERR_NOT_IMPLEMENTED;
        }
    }
    /* GetPerformanceInfo is xp and up, so load it dynamically */
    hMod = LoadLibrary("PSAPI.DLL");
    if (hMod)
    {
        *(uintptr_t *)&gCtx.pfnGetPerformanceInfo = (uintptr_t)GetProcAddress(hMod, "GetPerformanceInfo");
        if (gCtx.pfnGetPerformanceInfo)
            dprintf(("gCtx.pfnGetPerformanceInfo= %x\n", gCtx.pfnGetPerformanceInfo));
        /* failure is not fatal */
    }

    *pfStartThread = true;
    *ppInstance = &gCtx;
    return VINF_SUCCESS;
}


void VBoxStatsDestroy(const VBOXSERVICEENV *pEnv, void *pInstance)
{
    dprintf(("VBoxStatsDestroy\n"));
    return;
}

void VBoxStatsReportStatistics(VBOXSTATSCONTEXT *pCtx)
{
    SYSTEM_INFO systemInfo;
    PSYSTEM_PROCESSOR_PERFORMANCE_INFORMATION pProcInfo;
    MEMORYSTATUSEX memStatus;
    VMMDevReportGuestStats req;
    uint32_t cbStruct;
    DWORD    cbReturned;
    HANDLE   gVBoxDriver = pCtx->pEnv->hDriver;

    Assert(gCtx.pfnGlobalMemoryStatusEx && gCtx.pfnNtQuerySystemInformation);
    if (    !gCtx.pfnGlobalMemoryStatusEx 
        ||  !gCtx.pfnNtQuerySystemInformation)
        return;

    vmmdevInitRequest(&req.header, VMMDevReq_ReportGuestStats);

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
    req.guestStats.u32PhysMemBalloon    = VBoxMemBalloonQuerySize() * (_1M/systemInfo.dwPageSize);    /* was in megabytes */
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
            dprintf(("GetPerformanceInfo failed with %d\n", GetLastError()));
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

        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &req, req.header.size, &req, req.header.size, &cbReturned, NULL))
        {
            dprintf(("VBoxStatsReportStatistics: new statistics reported successfully!\n"));
        }
        else
            dprintf(("VBoxStatsReportStatistics: DeviceIoControl (stats report) failed with %d\n", GetLastError()));
    }

    free(pProcInfo);
}

/**
 * Thread function to wait for and process seamless mode change
 * requests
 */
unsigned __stdcall VBoxStatsThread(void *pInstance)
{
    VBOXSTATSCONTEXT *pCtx = (VBOXSTATSCONTEXT *)pInstance;
    HANDLE gVBoxDriver = pCtx->pEnv->hDriver;
    bool fTerminate = false;
    VBoxGuestFilterMaskInfo maskInfo;
    DWORD cbReturned;

    maskInfo.u32OrMask = VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;
    maskInfo.u32NotMask = 0;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxStatsThread: DeviceIOControl(CtlMask - or) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxStatsThread: DeviceIOControl(CtlMask) failed, SeamlessChangeThread exited\n"));
        return 0;
    }

    do
    {
        /* wait for a seamless change event */
        VBoxGuestWaitEventInfo waitEvent;
        waitEvent.u32TimeoutIn = (pCtx->uStatInterval) ? pCtx->uStatInterval : 5000;
        waitEvent.u32EventMaskIn = VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;
        if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_WAITEVENT, &waitEvent, sizeof(waitEvent), &waitEvent, sizeof(waitEvent), &cbReturned, NULL))
        {
            dprintf(("VBoxStatsThread: DeviceIOControl succeded\n"));

            /* are we supposed to stop? */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 0) == WAIT_OBJECT_0)
                break;

            dprintf(("VBoxStatsThread: checking event\n"));

            /* did we get the right event? */
            if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST)
            {
                VMMDevGetStatisticsChangeRequest req;
                vmmdevInitRequest(&req.header, VMMDevReq_GetStatisticsChangeRequest);
                req.eventAck = VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;

                if (DeviceIoControl(gVBoxDriver, IOCTL_VBOXGUEST_VMMREQUEST, &req, req.header.size, &req, req.header.size, &cbReturned, NULL))
                {
                    dprintf(("VBoxStatsThread: new statistics interval %d seconds\n", req.u32StatInterval));
                    pCtx->uStatInterval = req.u32StatInterval * 1000;
                }
                else
                    dprintf(("VBoxStatsThread: DeviceIoControl (stat) failed with %d\n", GetLastError()));
            }
        } 
        else
        {
            dprintf(("VBoxStatsThread: error 0 from DeviceIoControl IOCTL_VBOXGUEST_WAITEVENT\n"));

            /* sleep a bit to not eat too much CPU in case the above call always fails */
            if (WaitForSingleObject(pCtx->pEnv->hStopEvent, 10) == WAIT_OBJECT_0)
            {
                fTerminate = true;
                break;
            }
        }
        /* Report statistics to the host */
        if (    gCtx.uStatInterval 
            &&  gCtx.pfnNtQuerySystemInformation)
        {
            VBoxStatsReportStatistics(pCtx);
        }
    } 
    while (!fTerminate);

    maskInfo.u32OrMask = 0;
    maskInfo.u32NotMask = VMMDEV_EVENT_STATISTICS_INTERVAL_CHANGE_REQUEST;
    if (DeviceIoControl (gVBoxDriver, IOCTL_VBOXGUEST_CTL_FILTER_MASK, &maskInfo, sizeof (maskInfo), NULL, 0, &cbReturned, NULL))
    {
        dprintf(("VBoxStatsThread: DeviceIOControl(CtlMask - not) succeeded\n"));
    }
    else
    {
        dprintf(("VBoxStatsThread: DeviceIOControl(CtlMask) failed\n"));
    }

    dprintf(("VBoxStatsThread: finished statistics change request thread\n"));
    return 0;
}


