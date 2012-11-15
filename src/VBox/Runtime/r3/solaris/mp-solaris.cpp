/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Solaris.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <kstat.h>
#include <sys/processor.h>

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/log.h>
#include <iprt/once.h>
#include <iprt/critsect.h>


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Initialization serializing (rtMpSolarisOnce). */
static RTONCE       g_MpSolarisOnce = RTONCE_INITIALIZER;
/** Critical section serializing access to kstat. */
static RTCRITSECT   g_MpSolarisCritSect;
/** The kstat handle. */
static kstat_ctl_t *g_pKsCtl;
/** Array pointing to the cpu_info instances. */
static kstat_t    **g_papCpuInfo;
/** The number of entries in g_papCpuInfo */
static RTCPUID      g_capCpuInfo;


/**
 * Run once function that initializes the kstats we need here.
 *
 * @returns IPRT status code.
 * @param   pvUser      Unused.
 */
static DECLCALLBACK(int) rtMpSolarisOnce(void *pvUser)
{
    int rc = VINF_SUCCESS;
    NOREF(pvUser);

    /*
     * Open kstat and find the cpu_info entries for each of the CPUs.
     */
    g_pKsCtl = kstat_open();
    if (g_pKsCtl)
    {
        g_capCpuInfo = RTMpGetCount();
        g_papCpuInfo = (kstat_t **)RTMemAllocZ(g_capCpuInfo * sizeof(kstat_t *));
        if (g_papCpuInfo)
        {
            rc = RTCritSectInit(&g_MpSolarisCritSect);
            if (RT_SUCCESS(rc))
            {
                RTCPUID i = 0;
                for (kstat_t *pKsp = g_pKsCtl->kc_chain; pKsp != NULL; pKsp = pKsp->ks_next)
                {
                    if (!strcmp(pKsp->ks_module, "cpu_info"))
                    {
                        AssertBreak(i < g_capCpuInfo);
                        g_papCpuInfo[i++] = pKsp;
                        /** @todo ks_instance == cpu_id (/usr/src/uts/common/os/cpu.c)? Check this and fix it ASAP. */
                    }
                }

                return VINF_SUCCESS;
            }

            /* bail out, we failed. */
            RTMemFree(g_papCpuInfo);
        }
        else
            rc = VERR_NO_MEMORY;
        kstat_close(g_pKsCtl);
        g_pKsCtl = NULL;
    }
    else
    {
        rc = RTErrConvertFromErrno(errno);
        if (RT_SUCCESS(rc))
            rc = VERR_INTERNAL_ERROR;
        Log(("kstat_open() -> %d (%Rrc)\n", errno, rc));
    }

    return rc;
}


/**
 * Worker for RTMpGetCurFrequency and RTMpGetMaxFrequency.
 *
 * @returns The desired frequency on success, 0 on failure.
 *
 * @param   idCpu           The CPU ID.
 * @param   pszStatName     The cpu_info stat name.
 */
static uint64_t rtMpSolarisGetFrequency(RTCPUID idCpu, char *pszStatName)
{
    uint64_t u64 = 0;
    int rc = RTOnce(&g_MpSolarisOnce, rtMpSolarisOnce, NULL);
    if (RT_SUCCESS(rc))
    {
        if (    idCpu < g_capCpuInfo
            &&  g_papCpuInfo[idCpu])
        {
            rc = RTCritSectEnter(&g_MpSolarisCritSect);
            AssertRC(rc);
            if (RT_SUCCESS(rc))
            {
                if (kstat_read(g_pKsCtl, g_papCpuInfo[idCpu], 0) != -1)
                {
                    kstat_named_t *pStat = (kstat_named_t *)kstat_data_lookup(g_papCpuInfo[idCpu], pszStatName);
                    if (pStat)
                    {
                        Assert(pStat->data_type == KSTAT_DATA_UINT64 || pStat->data_type == KSTAT_DATA_LONG);
                        switch (pStat->data_type)
                        {
                            case KSTAT_DATA_UINT64: u64 = pStat->value.ui64; break; /* current_clock_Hz */
                            case KSTAT_DATA_INT32:  u64 = pStat->value.i32;  break; /* clock_MHz */

                            /* just in case... */
                            case KSTAT_DATA_UINT32: u64 = pStat->value.ui32; break;
                            case KSTAT_DATA_INT64:  u64 = pStat->value.i64;  break;
                            default:
                                AssertMsgFailed(("%d\n", pStat->data_type));
                                break;
                        }
                    }
                    else
                        Log(("kstat_data_lookup(%s) -> %d\n", pszStatName, errno));
                }
                else
                    Log(("kstat_read() -> %d\n", errno));
                RTCritSectLeave(&g_MpSolarisCritSect);
            }
        }
        else
            Log(("invalid idCpu: %d (g_capCpuInfo=%d)\n", (int)idCpu, (int)g_capCpuInfo));
    }

    return u64;
}


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    return rtMpSolarisGetFrequency(idCpu, "current_clock_Hz") / 1000000;
}


RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    return rtMpSolarisGetFrequency(idCpu, "clock_MHz");
}


#if defined(RT_ARCH_SPARC) || defined(RT_ARCH_SPARC64)
RTDECL(RTCPUID) RTMpCpuId(void)
{
    /** @todo implement RTMpCpuId on solaris/r3! */
    return NIL_RTCPUID;
}
#endif


RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < RTCPUSET_MAX_CPUS ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < RTCPUSET_MAX_CPUS ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return RTMpGetCount() - 1;
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    return idCpu != NIL_RTCPUID
        && idCpu < (RTCPUID)RTMpGetCount();
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    int iStatus = p_online(idCpu, P_STATUS);
    return iStatus == P_ONLINE
        || iStatus == P_NOINTR;
}


RTDECL(bool) RTMpIsCpuPresent(RTCPUID idCpu)
{
    int iStatus = p_online(idCpu, P_STATUS);
    return iStatus != -1;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    /*
     * Solaris has sysconf.
     */
    int cCpus = sysconf(_SC_NPROCESSORS_MAX);
    if (cCpus < 0)
        cCpus = sysconf(_SC_NPROCESSORS_CONF);
    return cCpus;
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCpuSetEmpty(pSet);
    int idCpu = RTMpGetCount();
    while (idCpu-- > 0)
        RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    /*
     * Solaris has sysconf.
     */
    return sysconf(_SC_NPROCESSORS_ONLN);
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTCpuSetEmpty(pSet);
    RTCPUID cCpus = RTMpGetCount();
    for (RTCPUID idCpu = 0; idCpu < cCpus; idCpu++)
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(PRTCPUSET) RTMpGetPresentSet(PRTCPUSET pSet)
{
#ifdef RT_STRICT
    long cCpusPresent = 0;
#endif
    RTCpuSetEmpty(pSet);
    RTCPUID cCpus = RTMpGetCount();
    for (RTCPUID idCpu = 0; idCpu < cCpus; idCpu++)
        if (RTMpIsCpuPresent(idCpu))
        {
            RTCpuSetAdd(pSet, idCpu);
#ifdef RT_STRICT
            cCpusPresent++;
#endif
        }
    Assert(cCpusPresent == RTMpGetPresentCount());
    return pSet;
}


RTDECL(RTCPUID) RTMpGetPresentCount(void)
{
    /*
     * Solaris has sysconf.
     */
    return sysconf(_SC_NPROCESSORS_CONF);
}

