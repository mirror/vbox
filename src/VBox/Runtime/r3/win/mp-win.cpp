/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Windows.
 */

/*
 * Copyright (C) 2006-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP RTLOGGROUP_SYSTEM
#include <iprt/win/windows.h>

#include <iprt/mp.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/cpuset.h>
#include <iprt/ldr.h>
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
    /*
     * Resolve the API dynamically (one try) as it requires XP w/ sp3 or later.
     */
    typedef BOOL (WINAPI *PFNGETLOGICALPROCINFO)(PSYSTEM_LOGICAL_PROCESSOR_INFORMATION, PDWORD);
    static PFNGETLOGICALPROCINFO s_pfnGetLogicalProcInfo = (PFNGETLOGICALPROCINFO)~(uintptr_t)0;
    if (s_pfnGetLogicalProcInfo == (PFNGETLOGICALPROCINFO)~(uintptr_t)0)
        s_pfnGetLogicalProcInfo = (PFNGETLOGICALPROCINFO)RTLdrGetSystemSymbol("kernel32.dll", "GetLogicalProcessorInformation");

    /*
     * Sadly, on XP and Server 2003, even if the API is present, it does not tell us
     * how many physical cores there are (any package will look like a single core).
     * That is worse than not using the API at all, so just skip it unless it's Vista+.
     */
    bool fIsVistaOrLater = false;
    OSVERSIONINFOEX OSInfoEx = { 0 };
    OSInfoEx.dwOSVersionInfoSize = sizeof(OSVERSIONINFOEX);
    if (   GetVersionEx((LPOSVERSIONINFO) &OSInfoEx)
        && (OSInfoEx.dwPlatformId == VER_PLATFORM_WIN32_NT)
        && (OSInfoEx.dwMajorVersion >= 6))
        fIsVistaOrLater = true;

    if (s_pfnGetLogicalProcInfo && fIsVistaOrLater)
    {
        /*
         * Query the information. This unfortunately requires a buffer, so we
         * start with a guess and let windows advice us if it's too small.
         */
        DWORD                                   cbSysProcInfo = _4K;
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION   paSysInfo = NULL;
        BOOL                                    fRc = FALSE;
        do
        {
            cbSysProcInfo = RT_ALIGN_32(cbSysProcInfo, 256);
            void *pv = RTMemRealloc(paSysInfo, cbSysProcInfo);
            if (!pv)
                break;
            paSysInfo = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)pv;
            fRc = s_pfnGetLogicalProcInfo(paSysInfo, &cbSysProcInfo);
        } while (!fRc && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
        if (fRc)
        {
            /*
             * Parse the result.
             */
            uint32_t cCores = 0;
            uint32_t i      = cbSysProcInfo / sizeof(paSysInfo[0]);
            while (i-- > 0)
                if (paSysInfo[i].Relationship == RelationProcessorCore)
                    cCores++;

            RTMemFree(paSysInfo);
            Assert(cCores > 0);
            return cCores;
        }

        RTMemFree(paSysInfo);
    }

    /* If we don't have the necessary API or if it failed, return the same
       value as the generic implementation. */
    return RTMpGetCount();
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


RTDECL(RTCPUID) RTMpGetOnlineCoreCount(void)
{
    /** @todo this isn't entirely correct. */
    return RTMpGetCoreCount();
}

