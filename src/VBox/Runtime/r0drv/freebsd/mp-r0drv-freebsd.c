/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Ring-0 Driver, FreeBSD.
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
#include "the-freebsd-kernel.h"

#include <iprt/mp.h>
#include <iprt/err.h>
#include <iprt/asm.h>
#include <iprt/cpuset.h>
#include "r0drv/mp-r0drv.h"


RTDECL(RTCPUID) RTMpCpuId(void)
{
    return curcpu;
}


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return (int)idCpu < mp_ncpus ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return iCpu < mp_ncpus ? (RTCPUID)iCpu : NIL_RTCPUID;
}

RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return mp_ncpus;
}

RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    if (RT_UNLIKELY((int)idCpu > mp_ncpus))
        return false;
    else
        return true;
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
    return mp_ncpus;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    if (RT_UNLIKELY((int)idCpu > mp_ncpus))
        return false;

    return CPU_ABSENT(idCpu) ? false : true;
}

RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTCPUID idCpu;

    RTCpuSetEmpty(pSet);
    idCpu = RTMpGetMaxCpuId();
    do
    {
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    } while (idCpu-- > 0);

    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    return mp_ncpus;
}

/**
 * Wrapper between the native FreeBSD per-cpu callback and PFNRTWORKER
 * for the RTMpOnAll API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnAllFreeBSDWrapper(void *pvArg)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    pArgs->pfnWorker(curcpu, pArgs->pvUser1, pArgs->pvUser2);
}


RTDECL(int) RTMpOnAll(PFNRTMPWORKER pfnWorker, void *pvUser1, void *pvUser2)
{
    RTMPARGS Args;
    Args.pfnWorker = pfnWorker;
    Args.pvUser1 = pvUser1;
    Args.pvUser2 = pvUser2;
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;
    smp_rendezvous(NULL, rtmpOnAllFreeBSDWrapper, NULL, &Args);
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native FreeBSD per-cpu callback and PFNRTWORKER
 * for the RTMpOnOthers API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnOthersFreeBSDWrapper(void *pvArg)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    RTCPUID idCpu = curcpu;
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
    Args.idCpu = NIL_RTCPUID;
    Args.cHits = 0;
    smp_rendezvous(NULL, rtmpOnOthersFreeBSDWrapper, NULL, &Args);
    return VINF_SUCCESS;
}


/**
 * Wrapper between the native FreeBSD per-cpu callback and PFNRTWORKER
 * for the RTMpOnSpecific API.
 *
 * @param   pvArg   Pointer to the RTMPARGS package.
 */
static void rtmpOnSpecificFreeBSDWrapper(void *pvArg)
{
    PRTMPARGS pArgs = (PRTMPARGS)pvArg;
    RTCPUID idCpu = curcpu;
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
    smp_rendezvous(NULL, rtmpOnSpecificFreeBSDWrapper, NULL, &Args);
    return Args.cHits == 1
         ? VINF_SUCCESS
         : VERR_CPU_NOT_FOUND;
}

