/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, Linux.
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
#include "the-linux-kernel.h"
#include "internal/iprt.h"

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/thread.h>
#include "r0drv/mp-r0drv.h"


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return smp_processor_id();
}
RT_EXPORT_SYMBOL(RTMpCpuId);


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < RTCPUSET_MAX_CPUS && idCpu < NR_CPUS ? (int)idCpu : -1;
}
RT_EXPORT_SYMBOL(RTMpCpuIdToSetIndex);


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return iCpu < NR_CPUS ? (RTCPUID)iCpu : NIL_RTCPUID;
}
RT_EXPORT_SYMBOL(RTMpCpuIdFromSetIndex);


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return NR_CPUS - 1; //???
}
RT_EXPORT_SYMBOL(RTMpGetMaxCpuId);


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
#if defined(CONFIG_SMP)
    if (RT_UNLIKELY(idCpu >= NR_CPUS))
        return false;

# if defined(cpu_possible)
    return cpu_possible(idCpu);
# else /* < 2.5.29 */
    return idCpu < (RTCPUID)smp_num_cpus;
# endif
#else
    return idCpu == RTMpCpuId();
#endif
}
RT_EXPORT_SYMBOL(RTMpIsCpuPossible);


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId();
    do
    {
        if (RTMpIsCpuPossible(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);
    return pSet;
}
RT_EXPORT_SYMBOL(RTMpGetSet);


RTDECL(RTCPUID) RTMpGetCount(void)
{
#ifdef CONFIG_SMP
# if defined(CONFIG_HOTPLUG_CPU) /* introduced & uses cpu_present */
    return num_present_cpus();
# elif defined(num_possible_cpus)
    return num_possible_cpus();
# elif LINUX_VERSION_CODE < KERNEL_VERSION(2, 5, 0)
    return smp_num_cpus;
# else
    RTCPUSET Set;
    RTMpGetSet(&Set);
    return RTCpuSetCount(&Set);
# endif
#else
    return 1;
#endif
}
RT_EXPORT_SYMBOL(RTMpGetCount);


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
#ifdef CONFIG_SMP
    if (RT_UNLIKELY(idCpu >= NR_CPUS))
        return false;
# ifdef cpu_online
    return cpu_online(idCpu);
# else /* 2.4: */
    return cpu_online_map & RT_BIT_64(idCpu);
# endif
#else
    return idCpu == RTMpCpuId();
#endif
}
RT_EXPORT_SYMBOL(RTMpIsCpuOnline);


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
#ifdef CONFIG_SMP
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId();
    do
    {
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);
#else
    RTCpuSetEmpty(pSet);
    RTCpuSetAdd(pSet, RTMpCpuId());
#endif
    return pSet;
}
RT_EXPORT_SYMBOL(RTMpGetOnlineSet);


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
#ifdef CONFIG_SMP
# if defined(num_online_cpus)
    return num_online_cpus();
# else
    RTCPUSET Set;
    RTMpGetOnlineSet(&Set);
    return RTCpuSetCount(&Set);
# endif
#else
    return 1;
#endif
}
RT_EXPORT_SYMBOL(RTMpGetOnlineCount);


RTDECL(bool) RTMpIsCpuWorkPending(void)
{
    /** @todo (not used on non-Windows platforms yet). */
    return false;
}
RT_EXPORT_SYMBOL(RTMpIsCpuWorkPending);


/**
 * Wrapper between the native linux per-cpu callbacks and PFNRTWORKER
 *
 * @param   pvInfo      Pointer to the RTMPARGS package.
 */
static void rtmpLinuxWrapper(void *pvInfo)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvInfo;
    ASMAtomicIncU32(&pArgs->cHits);
    pArgs->pfnWorker(RTMpCpuId(), pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 0)
    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
#endif
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    rc = on_each_cpu(rtmpLinuxWrapper, &Args, 1 /* wait */);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    rc = on_each_cpu(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
#else /* older kernels */
    RTThreadPreemptDisable(&PreemptState);
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
    local_irq_disable();
    rtmpLinuxWrapper(&Args);
    local_irq_enable();
    RTThreadPreemptRestore(&PreemptState);
#endif /* older kernels */
    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTMpOnAll);


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;

    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

    RTThreadPreemptDisable(&PreemptState);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 1 /* wait */);
#else /* older kernels */
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
#endif /* older kernels */
    RTThreadPreemptRestore(&PreemptState);

    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTMpOnOthers);


#if LINUX_VERSION_CODE < KERNEL_VERSION(2, 6, 19)
/**
 * Wrapper between the native linux per-cpu callbacks and PFNRTWORKER
 * employed by RTMpOnSpecific on older kernels that lacks smp_call_function_single.
 *
 * @param   pvInfo      Pointer to the RTMPARGS package.
 */
static void rtmpOnSpecificLinuxWrapper(void *pvInfo)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvInfo;
    RTCPUID idCpu = RTMpCpuId();

    if (idCpu == pArgs->idCpu)
    {
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
        ASMAtomicIncU32(&pArgs->cHits);
    }
}
#endif


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;

    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;

    if (!RTMpIsCpuPossible(idCpu))
        return VERR_CPU_NOT_FOUND;

    RTThreadPreemptDisable(&PreemptState);
    if (idCpu != RTMpCpuId())
    {
        if (RTMpIsCpuOnline(idCpu))
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
            rc = smp_call_function_single(idCpu, rtmpLinuxWrapper, &Args, 1 /* wait */);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
            rc = smp_call_function_single(idCpu, rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
#else /* older kernels */
            rc = smp_call_function(rtmpOnSpecificLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
#endif /* older kernels */
            Assert(rc == 0);
            rc = Args.cHits ? VINF_SUCCESS : VERR_CPU_OFFLINE;
        }
        else
            rc = VERR_CPU_OFFLINE;
    }
    else
    {
        rtmpLinuxWrapper(&Args);
        rc = VINF_SUCCESS;
    }
    RTThreadPreemptRestore(&PreemptState);;

    NOREF(rc);
    return rc;
}
RT_EXPORT_SYMBOL(RTMpOnSpecific);


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
/**
 * Dummy callback used by RTMpPokeCpu.
 *
 * @param   pvInfo      Ignored.
 */
static void rtmpLinuxPokeCpuCallback(void *pvInfo)
{
    NOREF(pvInfo);
}
#endif


RTDECL(int) RTMpPokeCpu(RTCPUID idCpu)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
    int rc;

    if (!RTMpIsCpuPossible(idCpu))
        return VERR_CPU_NOT_FOUND;
    if (!RTMpIsCpuOnline(idCpu))
        return VERR_CPU_OFFLINE;

# if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 27)
    rc = smp_call_function_single(idCpu, rtmpLinuxPokeCpuCallback, NULL, 0 /* wait */);
# elif LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
    rc = smp_call_function_single(idCpu, rtmpLinuxPokeCpuCallback, NULL, 0 /* retry */, 0 /* wait */);
# else  /* older kernels */
#  error oops
# endif /* older kernels */
    NOREF(rc);
    Assert(rc == 0);
    return VINF_SUCCESS;

#else  /* older kernels */
    /* no unicast here? */
    return VERR_NOT_SUPPORTED;
#endif /* older kernels */
}
RT_EXPORT_SYMBOL(RTMpPokeCpu);

