/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Multiprocessor, Ring-0 Driver, Solaris.
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
#include "the-solaris-kernel.h"

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "r0drv/mp-r0drv.h"


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return cpuid_get_chipid(CPU); /* is there a better way? */
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < NCPU ? idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < NCPU ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return NCPU - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    cpu_t *pCpu = idCpu < NCPU ? cpu_get(idCpu) : NULL;
    return pCpu
        && cpu_is_online(pCpu);
}


RTDECL(bool) RTMpDoesCpuExist(RTCPUID idCpu)
{
    cpu_t *pCpu = idCpu < NCPU ? cpu_get(idCpu) : NULL;
    return pCpu != NULL;
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId(); /* it's inclusive */
    do
    {
        if (RTMpDoesCpuExist(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);

    return pSet;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    return ncpus;
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId(); /* it's inclusive */
    do
    {
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);

    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    return ncpus_online;
}



/**
 * Wrapper between the native solaris per-cpu callback and PFNRTWORKER
 * for the RTMpOnAll API.
 *
 * @param   uArgs       Pointer to the RTMPARGS package.
 * @param   uIgnored1   Ignored.
 * @param   uIgnored2   Ignored.
 */
static int rtmpOnAllSolarisWrapper(xc_arg_t uArg, xc_arg_t uIgnored1, xc_arg_t uIgnored2)
{
    PRTMPARGS pArgs = (PRTMPARGS)(uArg);

    pArgs->pfnWorker(RTMpCpuId(), pArgs->pvUser1, pArgs->pvUser2);

    NOREF(uIgnored1);
    NOREF(uIgnored2);
    return 0;
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    cpuset_t Set;
    RTMPARGS Args;

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;

    kpreempt_disable();

    CPUSET_ALL(Set);
    xc_call((uintptr_t)&Args, 0, 0, X_CALL_HIPRI, Set, rtmpOnAllSolarisWrapper);

    kpreempt_enable();

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
static int rtmpOnOthersSolarisWrapper(xc_arg_t uArg, xc_arg_t uIgnored1, xc_arg_t uIgnored2)
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
    int rc;
    cpuset_t Set;
    RTMPARGS Args;

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = RTMpCpuId(); /** @todo should disable pre-emption before doing this.... */
    Args.cHits = 0;

    kpreempt_disable();

    CPUSET_ALL_BUT(Set, Args.idCpu);
    xc_call((uintptr_t)&Args, 0, 0, X_CALL_HIPRI, Set, rtmpOnOthersSolarisWrapper);

    kpreempt_enable();

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
static int rtmpOnSpecificSolarisWrapper(xc_arg_t uArg, xc_arg_t uIgnored1, xc_arg_t uIgnored2)
{
    PRTMPARGS pArgs = (PRTMPARGS)(uArg);
    RTCPUID idCpu = RTMpCpuId();

    Assert(idCpu != pArgs->idCpu);
    pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
    ASMAtomicIncU32(&pArgs->cHits);

    NOREF(uIgnored1);
    NOREF(uIgnored2);
    return 0;
}


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    cpuset_t Set;
    RTMPARGS Args;

    if (idCpu >= NCPU)
        return VERR_CPU_NOT_FOUND;

    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;

    kpreempt_disable();

    CPUSET_ZERO(Set);
    CPUSET_ADD(Set, idCpu);

    xc_call((uintptr_t)&Args, 0, 0, X_CALL_HIPRI, Set, rtmpOnSpecificSolarisWrapper);

    kpreempt_enable();

    Assert(ASMAtomicUoReadU32(&Args.cHits) <= 1);

    return ASMAtomicUoReadU32(&Args.cHits) == 1
         ? VINF_SUCCESS
         : VERR_CPU_NOT_FOUND;
}

