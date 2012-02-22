/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, Solaris.
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
#include "../the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/mp.h>
#include <iprt/cpuset.h>

#include <iprt/asm.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/err.h>
#include "r0drv/mp-r0drv.h"



RTDECL(bool) RTMpIsCpuWorkPending(void)
{
    return false;
}


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return vbi_cpu_id();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < RTCPUSET_MAX_CPUS && idCpu < vbi_cpu_maxcount() ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < vbi_cpu_maxcount() ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return vbi_max_cpu_id();
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    /*
     * We cannot query CPU status recursively, check cpu member from cached set.
     */
    if (idCpu >= vbi_cpu_count())
        return false;

    return RTCpuSetIsMember(&g_rtMpSolarisCpuSet, idCpu);

#if 0
    return idCpu < vbi_cpu_count() && vbi_cpu_online(idCpu);
#endif
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    return idCpu < vbi_cpu_count();
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId(); /* it's inclusive */
    do
    {
        if (RTMpIsCpuPossible(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);

    return pSet;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    return vbi_cpu_count();
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    /*
     * We cannot query CPU status recursively, return the cached set.
     */
    *pSet = g_rtMpSolarisCpuSet;
    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    RTCPUSET Set;
    RTMpGetOnlineSet(&Set);
    return RTCpuSetCount(&Set);
}



/**
 * Wrapper between the native solaris per-cpu callback and PFNRTWORKER
 * for the RTMpOnAll API.
 *
 * @param   uArgs       Pointer to the RTMPARGS package.
 * @param   uIgnored1   Ignored.
 * @param   uIgnored2   Ignored.
 */
static int rtmpOnAllSolarisWrapper(void *uArg, void *uIgnored1, void *uIgnored2)
{
    PRTMPARGS pArgs = (PRTMPARGS)(uArg);

    /*
     * Solaris CPU cross calls execute on offline CPUs too. Check our CPU cache
     * set and ignore if it's offline.
     */
    if (!RTMpIsCpuOnline(RTMpCpuId()))
        return 0;

    pArgs->pfnWorker(RTMpCpuId(), pArgs->pvUser1, pArgs->pvUser2);

    NOREF(uIgnored1);
    NOREF(uIgnored2);
    return 0;
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS Args;
    RT_ASSERT_INTS_ON();

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

    vbi_preempt_disable();

    vbi_execute_on_all(rtmpOnAllSolarisWrapper, &Args);

    vbi_preempt_enable();

    return VINF_SUCCESS;
}


/**
 * Wrapper between the native solaris per-cpu callback and PFNRTWORKER
 * for the RTMpOnOthers API.
 *
 * @param   uArgs       Pointer to the RTMPARGS package.
 * @param   uIgnored1   Ignored.
 * @param   uIgnored2   Ignored.
 */
static int rtmpOnOthersSolarisWrapper(void *uArg, void *uIgnored1, void *uIgnored2)
{
    PRTMPARGS pArgs = (PRTMPARGS)(uArg);
    RTCPUID idCpu = RTMpCpuId();

    Assert(idCpu != pArgs->idCpu);
    pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);

    NOREF(uIgnored1);
    NOREF(uIgnored2);
    return 0;
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS Args;
    RT_ASSERT_INTS_ON();

    /* The caller is supposed to have disabled preemption, but take no chances. */
    vbi_preempt_disable();

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = RTMpCpuId();
    Args.cHits = 0;

    vbi_execute_on_others(rtmpOnOthersSolarisWrapper, &Args);

    vbi_preempt_enable();

    return VINF_SUCCESS;
}


/**
 * Wrapper between the native solaris per-cpu callback and PFNRTWORKER
 * for the RTMpOnSpecific API.
 *
 *
 * @param   uArgs       Pointer to the RTMPARGS package.
 * @param   uIgnored1   Ignored.
 * @param   uIgnored2   Ignored.
 */
static int rtmpOnSpecificSolarisWrapper(void *uArg, void *uIgnored1, void *uIgnored2)
{
    PRTMPARGS pArgs = (PRTMPARGS)(uArg);
    RTCPUID idCpu = RTMpCpuId();

    Assert(idCpu == pArgs->idCpu);
    pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    ASMAtomicIncU32(&pArgs->cHits);

    NOREF(uIgnored1);
    NOREF(uIgnored2);
    return 0;
}


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS Args;
    RT_ASSERT_INTS_ON();

    if (idCpu >= vbi_cpu_count())
        return VERR_CPU_NOT_FOUND;

    if (RT_UNLIKELY(!RTMpIsCpuOnline(idCpu)))
        return RTMpIsCpuPresent(idCpu) ? VERR_CPU_OFFLINE : VERR_CPU_NOT_FOUND;

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;

    vbi_preempt_disable();

    vbi_execute_on_one(rtmpOnSpecificSolarisWrapper, &Args, idCpu);

    vbi_preempt_enable();

    Assert(ASMAtomicUoReadU32(&Args.cHits) <= 1);

    return ASMAtomicUoReadU32(&Args.cHits) == 1
         ? VINF_SUCCESS
         : VERR_CPU_NOT_FOUND;
}

