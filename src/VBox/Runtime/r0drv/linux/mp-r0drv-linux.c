/* $Id$ */
/** @file
 * innotek Portable Runtime - Multiprocessor, Ring-0 Driver, Linux.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "r0drv/mp-r0drv.h"


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return smp_processor_id();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < NR_CPUS ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return iCpu < NR_CPUS ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return NR_CPUS - 1; //???
}


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


RTDECL(bool) RTMpDoesCpuExist(RTCPUID idCpu)
{
#ifdef CONFIG_SMP
    if (RT_UNLIKELY(idCpu >= NR_CPUS))
        return false;
# ifdef CONFIG_HOTPLUG_CPU /* introduced & uses cpu_present */
    return cpu_present(idCpu);
# elif defined(cpu_possible)
    return cpu_possible(idCpu);
# else /* 2.4: */
    return idCpu < (RTCPUID)smp_num_cpus;
# endif
#else
    return idCpu == RTMpCpuId();
#endif
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId();
    do
    {
        if (RTMpDoesCpuExist(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);
    return pSet;
}


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

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 0)
    rc = on_each_cpu(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);

#else /* older kernels */

# ifdef preempt_disable
    preempt_disable();
# endif
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
    local_irq_disable();
    rtmpLinuxWrapper(&Args);
    local_irq_enable();
# ifdef preempt_enable
    preempt_enable();
# endif
#endif /* older kernels */
    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

# ifdef preempt_disable
    preempt_disable();
# endif
    rc = smp_call_function(rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
# ifdef preempt_enable
    preempt_enable();
# endif

    Assert(rc == 0); NOREF(rc);
    return VINF_SUCCESS;
}


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

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;

    if (!RTMpDoesCpuExist(idCpu))
        return VERR_CPU_NOT_FOUND;

# ifdef preempt_disable
    preempt_disable();
# endif
    if (idCpu != RTMpCpuId())
    {
        if (RTMpIsCpuOnline(idCpu))
        {
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 6, 19)
            rc = smp_call_function_single(idCpu, rtmpLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
#else
            rc = smp_call_function(rtmpOnSpecificLinuxWrapper, &Args, 0 /* retry */, 1 /* wait */);
#endif
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
# ifdef preempt_enable
    preempt_enable();
# endif

    NOREF(rc);
    return rc;
}

