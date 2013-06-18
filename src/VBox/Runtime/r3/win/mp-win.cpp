/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Windows.
 */

/*
 * Copyright (C) 2006-2011 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP RTLOGGROUP_SYSTEM
#include <Windows.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/assert.h>
#include <iprt/mem.h>


AssertCompile(MAXIMUM_PROCESSORS <= RTCPUSET_MAX_CPUS);


/** @todo RTmpCpuId(). */

RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < MAXIMUM_PROCESSORS ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < MAXIMUM_PROCESSORS ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return MAXIMUM_PROCESSORS - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    RTCPUSET Set;
    return RTCpuSetIsMember(RTMpGetOnlineSet(&Set), idCpu);
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    RTCPUSET Set;
    return RTCpuSetIsMember(RTMpGetSet(&Set), idCpu);
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCPUID idCpu = RTMpGetCount();
    RTCpuSetEmpty(pSet);
    while (idCpu-- > 0)
        RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
    Assert((RTCPUID)SysInfo.dwNumberOfProcessors == SysInfo.dwNumberOfProcessors);
    return SysInfo.dwNumberOfProcessors;
}

RTDECL(RTCPUID) RTMpGetCoreCount(void)
{
    BOOL (WINAPI *pfnGetLogicalProcInfo)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);

    pfnGetLogicalProcInfo = (BOOL (WINAPI *)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD))
                             GetProcAddress(GetModuleHandle("kernel32.dll"), "GetLogicalProcessorInformation");
    /* 0 represents an error condition. We cannot return VERR* error codes as caller expects a
     * unsigned value of core count.*/
    if (!pfnGetLogicalProcInfo)
        return 0;

    PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pSysInfo = NULL;
    DWORD cbSysProcInfo = 0;
    BOOL fRc = pfnGetLogicalProcInfo(pSysInfo, &cbSysProcInfo);
    if (!fRc)
    {
        if (GetLastError() ==  ERROR_INSUFFICIENT_BUFFER)
            pSysInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)RTMemAlloc(cbSysProcInfo);
    }

    RTCPUID cCores = 0;
    if (pSysInfo)
    {
        fRc = pfnGetLogicalProcInfo(pSysInfo, &cbSysProcInfo);
        if (fRc)
        {
            PSYSTEM_LOGICAL_PROCESSOR_INFORMATION pSysInfoTmp = pSysInfo;
            size_t offs = 0;
            while (offs + sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION) <= cbSysProcInfo)
            {
                switch (pSysInfoTmp->Relationship)
                {
                    case RelationProcessorCore:
                        cCores++;
                        break;
                    case RelationCache:
                    case RelationNumaNode:
                    case RelationProcessorPackage:
                    default:
                        ;
                }
                offs += sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                pSysInfoTmp++;
            }
        }
        RTMemFree(pSysInfo);
    }
    return cCores;
}

RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
/** @todo port to W2K8 / W7 w/ > 64 CPUs & grouping. */
    return RTCpuSetFromU64(pSet, SysInfo.dwActiveProcessorMask);
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    RTCPUSET Set;
    RTMpGetOnlineSet(&Set);
    return RTCpuSetCount(&Set);
}

