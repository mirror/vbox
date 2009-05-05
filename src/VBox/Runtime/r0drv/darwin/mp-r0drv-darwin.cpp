/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, Darwin.
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
#include "the-darwin-kernel.h"

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include "r0drv/mp-r0drv.h"

#define MY_DARWIN_MAX_CPUS      (0xf + 1) /* see MAX_CPUS */


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
static int32_t volatile g_cMaxCpus = -1;


static int rtMpDarwinInitMaxCpus(void)
{
    int32_t cCpus = -1;
    size_t  oldLen = sizeof(cCpus);
    int rc = sysctlbyname("hw.ncpu", &cCpus, &oldLen, NULL, NULL);
    if (rc)
    {
        printf("IPRT: sysctlbyname(hw.ncpu) failed with rc=%d!\n", rc);
        cCpus = MY_DARWIN_MAX_CPUS;
    }

    ASMAtomicWriteS32(&g_cMaxCpus, cCpus);
    return cCpus;
}


DECLINLINE(int) rtMpDarwinMaxCpus(void)
{
    int cCpus = g_cMaxCpus;
    if (RT_UNLIKELY(cCpus <= 0))
        return rtMpDarwinInitMaxCpus();
    return cCpus;
}


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return cpu_number();
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < MY_DARWIN_MAX_CPUS ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < MY_DARWIN_MAX_CPUS ? (RTCPUID)iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return rtMpDarwinMaxCpus() - 1;
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    return idCpu < MY_DARWIN_MAX_CPUS
        && idCpu < (RTCPUID)rtMpDarwinMaxCpus();
}


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


RTDECL(RTCPUID) RTMpGetCount(void)
{
    return rtMpDarwinMaxCpus();
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    /** @todo darwin R0 MP */
    return RTMpGetSet(pSet);
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    /** @todo darwin R0 MP */
    return RTMpGetCount();
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    /** @todo darwin R0 MP */
    return RTMpIsCpuPossible(idCpu);
}


RTDECL(PRTCPUSET) RTMpGetPresentSet(PRTCPUSET pSet)
{
    return RTMpGetSet(pSet);
}


RTDECL(RTCPUID) RTMpGetPresentCount(void)
{
    return RTMpGetCount();
}


RTDECL(bool) RTMpIsCpuPresent(RTCPUID idCpu)
{
    return RTMpIsCpuPossible(idCpu);
}


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    /** @todo darwin R0 MP (rainy day) */
    return 0;
}


RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    /** @todo darwin R0 MP (rainy day) */
    return 0;
}


RTDECL(bool) RTMpIsCpuWorkPending(void)
{
    /** @todo (not used on non-Windows platforms yet). */
    return false;
}


/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnAll API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnAllDarwinWrapper(void *pvArg)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    pArgs->pfnWorker(cpu_number(), pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;
    mp_rendezvous_no_intrs(rtmpOnAllDarwinWrapper, &Args);
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnOthers API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnOthersDarwinWrapper(void *pvArg)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    RTCPUID idCpu = cpu_number();
    if (pArgs->idCpu != idCpu)
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnOthers(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = RTMpCpuId();
    Args.cHits = 0;
    mp_rendezvous_no_intrs(rtmpOnOthersDarwinWrapper, &Args);
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native darwin per-cpu callback and PFNRTWORKER
 * for the RTMpOnSpecific API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnSpecificDarwinWrapper(void *pvArg)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    RTCPUID idCpu = cpu_number();
    if (pArgs->idCpu == idCpu)
    {
        pArgs->pfnWorker(idCpu, pArgs->pvUser1, pArgs->pvUser2);
        ASMAtomicIncU32(&pArgs->cHits);
    }
}


RTDECL(int) RTMpOnSpecific(RTCPUID idCpu, PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    int rc;
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = idCpu;
    Args.cHits = 0;
    mp_rendezvous_no_intrs(rtmpOnSpecificDarwinWrapper, &Args);
    return Args.cHits == 1
         ? VINF_SUCCESS
         : VERR_CPU_NOT_FOUND;
}


RTDECL(int) RTMpPokeCpu(RTCPUID idCpu)
{
    /* no unicast IPI */
    return VERR_NOT_SUPPORTED;
}

