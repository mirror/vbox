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
#include <iprt/once.h>
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
# include <iprt/asm-amd64-x86.h>
#endif

#include "internal-r3-win.h"



/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** Initialize once. */
static RTONCE                                       g_MpInitOnce = RTONCE_INITIALIZER;
//static decltype(GetMaximumProcessorCount)          *g_pfnGetMaximumProcessorCount;
static decltype(GetCurrentProcessorNumber)         *g_pfnGetCurrentProcessorNumber;
static decltype(GetCurrentProcessorNumberEx)       *g_pfnGetCurrentProcessorNumberEx;
static decltype(GetLogicalProcessorInformation)    *g_pfnGetLogicalProcessorInformation;
static decltype(GetLogicalProcessorInformationEx)  *g_pfnGetLogicalProcessorInformationEx;


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** The required buffer size for getting group relations. */
static uint32_t     g_cbRtMpWinGrpRelBuf;
/** The max number of CPUs (RTMpGetCount). */
static uint32_t     g_cRtMpWinMaxCpus;
/** The max number of CPU cores (RTMpGetCoreCount). */
static uint32_t     g_cRtMpWinMaxCpuCores;
/** The max number of groups. */
static uint32_t     g_cRtMpWinMaxCpuGroups;
/** Static per group info. */
static struct
{
    /** The CPU ID (and CPU set index) of the first CPU in the group. */
    uint16_t    idFirstCpu;
    /** The max CPUs in the group. */
    uint16_t    cMaxCpus;
} g_aRtMpWinCpuGroups[RTCPUSET_MAX_CPUS];


/**
 * @callback_method_impl{FNRTONCE, Resolves dynamic imports.}
 */
static DECLCALLBACK(int32_t) rtMpWinInitOnce(void *pvUser)
{
    RT_NOREF(pvUser);

    Assert(g_WinOsInfoEx.dwOSVersionInfoSize != 0);
    Assert(g_hModKernel32 != NULL);

    /*
     * Resolve dynamic APIs.
     */
#define RESOLVE_API(a_szMod, a_FnName) \
        do { \
            RT_CONCAT(g_pfn,a_FnName) = (decltype(a_FnName) *)GetProcAddress(g_hModKernel32, #a_FnName); \
        } while (0)
    //RESOLVE_API("kernel32.dll", GetMaximumProcessorCount); /* Calls GetLogicalProcessorInformationEx/RelationGroup in W10. */
    RESOLVE_API("kernel32.dll", GetCurrentProcessorNumber);
    RESOLVE_API("kernel32.dll", GetCurrentProcessorNumberEx);
    RESOLVE_API("kernel32.dll", GetLogicalProcessorInformation);
    RESOLVE_API("kernel32.dll", GetLogicalProcessorInformationEx);

    /*
     * Query group information, partitioning CPU IDs and CPU set
     * indexes (they are the same).
     *
     * We ASSUME the the GroupInfo index is the same as the group number.
     * We ASSUME there are no inactive groups, because otherwise it will
     * be difficult to tell how many possible CPUs we can have and do a
     * reasonable CPU ID/index partitioning.
     *
     * Note! We will die if there are too many processors!
     */
    union
    {
        SYSTEM_INFO                                 SysInfo;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX     Info;
        uint8_t                                     abPaddingG[  sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)
                                                               + sizeof(PROCESSOR_GROUP_INFO) * RTCPUSET_MAX_CPUS];
        uint8_t                                     abPaddingC[  sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)
                                                               +   (sizeof(PROCESSOR_RELATIONSHIP) + sizeof(GROUP_AFFINITY))
                                                                 * RTCPUSET_MAX_CPUS];
    } uBuf;
    if (g_pfnGetLogicalProcessorInformationEx)
    {
        /* Query the information. */
        DWORD cbData = sizeof(uBuf);
        AssertFatalMsg(g_pfnGetLogicalProcessorInformationEx(RelationGroup, &uBuf.Info, &cbData) != FALSE,
                       ("last error = %u, cbData = %u (in %u)\n", GetLastError(), cbData, sizeof(uBuf)));
        AssertFatalMsg(uBuf.Info.Relationship == RelationGroup,
                       ("Relationship = %u, expected %u!\n", uBuf.Info.Relationship, RelationGroup));
        AssertFatalMsg(uBuf.Info.Group.MaximumGroupCount <= RT_ELEMENTS(g_aRtMpWinCpuGroups),
                       ("MaximumGroupCount is %u, we only support up to %u!\n",
                        uBuf.Info.Group.MaximumGroupCount, RT_ELEMENTS(g_aRtMpWinCpuGroups)));

        AssertMsg(uBuf.Info.Group.MaximumGroupCount == uBuf.Info.Group.ActiveGroupCount, /* 2nd assumption mentioned above. */
                  ("%u vs %u\n", uBuf.Info.Group.MaximumGroupCount, uBuf.Info.Group.ActiveGroupCount));
        AssertFatal(uBuf.Info.Group.MaximumGroupCount >= uBuf.Info.Group.ActiveGroupCount);


        /* Process the active groups. */
        g_cRtMpWinMaxCpuGroups = uBuf.Info.Group.MaximumGroupCount;
        uint16_t idxCpu        = 0;
        uint32_t idxGroup      = 0;
        for (; idxGroup < uBuf.Info.Group.ActiveGroupCount; idxGroup++)
        {
            g_aRtMpWinCpuGroups[idxGroup].idFirstCpu = idxCpu;
            g_aRtMpWinCpuGroups[idxGroup].cMaxCpus   = uBuf.Info.Group.GroupInfo[idxGroup].MaximumProcessorCount;
            idxCpu += uBuf.Info.Group.GroupInfo[idxGroup].MaximumProcessorCount;
        }

        /* Just in case the 2nd assumption doesn't hold true and there are inactive groups. */
        for (; idxGroup < uBuf.Info.Group.MaximumGroupCount; idxGroup++)
        {
            g_aRtMpWinCpuGroups[idxGroup].idFirstCpu = idxCpu;
            g_aRtMpWinCpuGroups[idxGroup].cMaxCpus   = RT_MAX(MAXIMUM_PROC_PER_GROUP, 64);
            idxCpu += RT_MAX(MAXIMUM_PROC_PER_GROUP, 64);
        }

        g_cRtMpWinMaxCpus = idxCpu;
    }
    else
    {
        /* Legacy: */
        GetSystemInfo(&uBuf.SysInfo);
        g_cRtMpWinMaxCpus                   = uBuf.SysInfo.dwNumberOfProcessors;
        g_cRtMpWinMaxCpuGroups              = 1;
        g_aRtMpWinCpuGroups[0].idFirstCpu   = 0;
        g_aRtMpWinCpuGroups[0].cMaxCpus     = uBuf.SysInfo.dwNumberOfProcessors;
    }

    AssertFatalMsg(g_cRtMpWinMaxCpus <= RTCPUSET_MAX_CPUS,
                   ("g_cRtMpWinMaxCpus=%u (%#x); RTCPUSET_MAX_CPUS=%u\n", g_cRtMpWinMaxCpus, g_cRtMpWinMaxCpus, RTCPUSET_MAX_CPUS));

    g_cbRtMpWinGrpRelBuf = sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX)
                         + (g_cRtMpWinMaxCpuGroups + 2) * sizeof(PROCESSOR_GROUP_INFO);

    /*
     * Get information about cores.
     *
     * Note! This will only give us info about active processors according to
     *       MSDN, we'll just have to hope that CPUs aren't hotplugged after we
     *       initialize here (or that the API consumers doesn't care too much).
     */
    /** @todo A hot CPU plug event would be nice. */
    g_cRtMpWinMaxCpuCores = g_cRtMpWinMaxCpus;
    if (g_pfnGetLogicalProcessorInformationEx)
    {
        /* Query the information. */
        DWORD cbData = sizeof(uBuf);
        AssertFatalMsg(g_pfnGetLogicalProcessorInformationEx(RelationProcessorCore, &uBuf.Info, &cbData) != FALSE,
                       ("last error = %u, cbData = %u (in %u)\n", GetLastError(), cbData, sizeof(uBuf)));
        g_cRtMpWinMaxCpuCores = 0;
        for (uint32_t off = 0; off < cbData; )
        {
            SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pCur = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)&uBuf.abPaddingG[off];
            AssertFatalMsg(pCur->Relationship == RelationProcessorCore,
                           ("off = %#x, Relationship = %u, expected %u!\n", off, pCur->Relationship, RelationProcessorCore));
            g_cRtMpWinMaxCpuCores++;
            off += pCur->Size;
        }

#if ARCH_BITS == 32
        if (g_cRtMpWinMaxCpuCores > g_cRtMpWinMaxCpus)
        {
            /** @todo WOW64 trouble where the emulation environment has folded the high
             *        processor masks (63..32) into the low (31..0), hiding some
             *        processors from us.  Currently we don't deal with that. */
            g_cRtMpWinMaxCpuCores = g_cRtMpWinMaxCpus;
        }
        else
            AssertStmt(g_cRtMpWinMaxCpuCores > 0, g_cRtMpWinMaxCpuCores = g_cRtMpWinMaxCpus);
#else
        AssertStmt(g_cRtMpWinMaxCpuCores > 0 && g_cRtMpWinMaxCpuCores <= g_cRtMpWinMaxCpus,
                   g_cRtMpWinMaxCpuCores = g_cRtMpWinMaxCpus);
#endif
    }
    else
    {
        /*
         * Sadly, on XP and Server 2003, even if the API is present, it does not tell us
         * how many physical cores there are (any package will look like a single core).
         * That is worse than not using the API at all, so just skip it unless it's Vista+.
         */
        if (   g_pfnGetLogicalProcessorInformation
            && g_WinOsInfoEx.dwPlatformId == VER_PLATFORM_WIN32_NT
            && g_WinOsInfoEx.dwMajorVersion >= 6)
        {
            /* Query the info. */
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
                fRc = g_pfnGetLogicalProcessorInformation(paSysInfo, &cbSysProcInfo);
            } while (!fRc && GetLastError() == ERROR_INSUFFICIENT_BUFFER);
            if (fRc)
            {
                /* Count the cores in the result. */
                g_cRtMpWinMaxCpuCores = 0;
                uint32_t i = cbSysProcInfo / sizeof(paSysInfo[0]);
                while (i-- > 0)
                    if (paSysInfo[i].Relationship == RelationProcessorCore)
                        g_cRtMpWinMaxCpuCores++;

                AssertStmt(g_cRtMpWinMaxCpuCores > 0 && g_cRtMpWinMaxCpuCores <= g_cRtMpWinMaxCpus,
                           g_cRtMpWinMaxCpuCores = g_cRtMpWinMaxCpus);
            }
            RTMemFree(paSysInfo);
        }
    }

    return VINF_SUCCESS;
}


RTDECL(RTCPUID) RTMpCpuId(void)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);

    if (g_pfnGetCurrentProcessorNumberEx)
    {
        PROCESSOR_NUMBER ProcNum;
        g_pfnGetCurrentProcessorNumberEx(&ProcNum);
        Assert(ProcNum.Group < g_cRtMpWinMaxCpuGroups);
        Assert(ProcNum.Number < g_aRtMpWinCpuGroups[ProcNum.Group].cMaxCpus);
        return g_aRtMpWinCpuGroups[ProcNum.Group].idFirstCpu + ProcNum.Number;
    }

    if (g_pfnGetCurrentProcessorNumber)
    {
        /* Should be safe wrt processor numbering, I hope... Only affects W2k3 and Vista. */
        Assert(g_cRtMpWinMaxCpuGroups == 1);
        return g_pfnGetCurrentProcessorNumber();
    }

    /* The API was introduced with W2K3 according to MSDN. */
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
    return ASMGetApicId();
#else
# error "Not ported to this architecture."
    return NIL_RTAPICID;
#endif
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);

    /* 1:1 mapping, just do range checking. */
    return idCpu < g_cRtMpWinMaxCpus ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);

    /* 1:1 mapping, just do range checking. */
    return (unsigned)iCpu < g_cRtMpWinMaxCpus ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);
    return g_cRtMpWinMaxCpus - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);
    RTCPUSET Set;
    return RTCpuSetIsMember(RTMpGetOnlineSet(&Set), idCpu);
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);
    /* Any CPU between 0 and g_cRtMpWinMaxCpus are possible. */
    return idCpu < g_cRtMpWinMaxCpus;
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
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);
    return g_cRtMpWinMaxCpus;
}


RTDECL(RTCPUID) RTMpGetCoreCount(void)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);
    return g_cRtMpWinMaxCpuCores;
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTOnce(&g_MpInitOnce, rtMpWinInitOnce, NULL);

    if (g_pfnGetLogicalProcessorInformationEx)
    {
        /*
         * Get the group relation info.
         *
         * In addition to the ASSUMPTIONS that are documented in rtMpWinInitOnce,
         * we ASSUME that PROCESSOR_GROUP_INFO::MaximumProcessorCount gives the
         * active processor mask width.
         */
        DWORD                                    cbInfo = g_cbRtMpWinGrpRelBuf;
        SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *pInfo = (SYSTEM_LOGICAL_PROCESSOR_INFORMATION_EX *)alloca(cbInfo);
        AssertFatalMsg(g_pfnGetLogicalProcessorInformationEx(RelationGroup, pInfo, &cbInfo) != FALSE,
                       ("last error = %u, cbInfo = %u (in %u)\n", GetLastError(), cbInfo, g_cbRtMpWinGrpRelBuf));
        AssertFatalMsg(pInfo->Relationship == RelationGroup,
                       ("Relationship = %u, expected %u!\n", pInfo->Relationship, RelationGroup));
        AssertFatalMsg(pInfo->Group.MaximumGroupCount == g_cRtMpWinMaxCpuGroups,
                       ("MaximumGroupCount is %u, expected %u!\n", pInfo->Group.MaximumGroupCount, g_cRtMpWinMaxCpuGroups));

        RTCpuSetEmpty(pSet);
        for (uint32_t idxGroup = 0; idxGroup < pInfo->Group.MaximumGroupCount; idxGroup++)
        {
            Assert(pInfo->Group.GroupInfo[idxGroup].MaximumProcessorCount == g_aRtMpWinCpuGroups[idxGroup].cMaxCpus);
            Assert(pInfo->Group.GroupInfo[idxGroup].ActiveProcessorCount  <= g_aRtMpWinCpuGroups[idxGroup].cMaxCpus);

            KAFFINITY fActive = pInfo->Group.GroupInfo[idxGroup].ActiveProcessorMask;
            if (fActive != 0)
            {
#ifdef RT_STRICT
                uint32_t    cMembersLeft = pInfo->Group.GroupInfo[idxGroup].ActiveProcessorCount;
#endif
                int const   idxFirst  = g_aRtMpWinCpuGroups[idxGroup].idFirstCpu;
                int const   cMembers  = g_aRtMpWinCpuGroups[idxGroup].cMaxCpus;
                for (int idxMember = 0; idxMember < cMembers; idxMember++)
                {
                    if (fActive & 1)
                    {
#ifdef RT_STRICT
                        cMembersLeft--;
#endif
                        RTCpuSetAddByIndex(pSet, idxFirst + idxMember);
                        fActive >>= 1;
                        if (!fActive)
                            break;
                    }
                    else
                    {
                        fActive >>= 1;
                    }
                }
                Assert(cMembersLeft == 0);
            }
            else
                Assert(pInfo->Group.GroupInfo[idxGroup].ActiveProcessorCount == 0);
        }

        return pSet;
    }

    /*
     * Legacy fallback code.
     */
    SYSTEM_INFO SysInfo;
    GetSystemInfo(&SysInfo);
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
    /** @todo this isn't entirely correct, but whatever. */
    return RTMpGetCoreCount();
}

