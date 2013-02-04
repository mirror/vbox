/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Linux.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#include <stdio.h>
#include <errno.h>

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/linux/sysfs.h>


/**
 * Internal worker that determines the max possible CPU count.
 *
 * @returns Max cpus.
 */
static RTCPUID rtMpLinuxMaxCpus(void)
{
#if 0 /* this doesn't do the right thing :-/ */
    int cMax = sysconf(_SC_NPROCESSORS_CONF);
    Assert(cMax >= 1);
    return cMax;
#else
    static uint32_t s_cMax = 0;
    if (!s_cMax)
    {
        int cMax = 1;
        for (unsigned iCpu = 0; iCpu < RTCPUSET_MAX_CPUS; iCpu++)
            if (RTLinuxSysFsExists("devices/system/cpu/cpu%d", iCpu))
                cMax = iCpu + 1;
        ASMAtomicUoWriteU32((uint32_t volatile *)&s_cMax, cMax);
        return cMax;
    }
    return s_cMax;
#endif
}

/**
 * Internal worker that picks the processor speed in MHz from /proc/cpuinfo.
 *
 * @returns CPU frequency.
 */
static uint32_t rtMpLinuxGetFrequency(RTCPUID idCpu)
{
    FILE *pFile = fopen("/proc/cpuinfo", "r");
    if (!pFile)
        return 0;

    char sz[256];
    RTCPUID idCpuFound = NIL_RTCPUID;
    uint32_t Frequency = 0;
    while (fgets(sz, sizeof(sz), pFile))
    {
        char *psz;
        if (   !strncmp(sz, "processor", 9)
            && (sz[10] == ' ' || sz[10] == '\t' || sz[10] == ':')
            && (psz = strchr(sz, ':')))
        {
            psz += 2;
            int64_t iCpu;
            int rc = RTStrToInt64Ex(psz, NULL, 0, &iCpu);
            if (RT_SUCCESS(rc))
                idCpuFound = iCpu;
        }
        else if (   idCpu == idCpuFound
                 && !strncmp(sz, "cpu MHz", 7)
                 && (sz[10] == ' ' || sz[10] == '\t' || sz[10] == ':')
                 && (psz = strchr(sz, ':')))
        {
            psz += 2;
            int64_t v;
            int rc = RTStrToInt64Ex(psz, &psz, 0, &v);
            if (RT_SUCCESS(rc))
            {
                Frequency = v;
                break;
            }
        }
    }
    fclose(pFile);
    return Frequency;
}


/** @todo RTmpCpuId(). */

RTDECL(int) RTMpCpuIdToSetIndex(RTCPUID idCpu)
{
    return idCpu < rtMpLinuxMaxCpus() ? (int)idCpu : -1;
}


RTDECL(RTCPUID) RTMpCpuIdFromSetIndex(int iCpu)
{
    return (unsigned)iCpu < rtMpLinuxMaxCpus() ? iCpu : NIL_RTCPUID;
}


RTDECL(RTCPUID) RTMpGetMaxCpuId(void)
{
    return rtMpLinuxMaxCpus() - 1;
}


RTDECL(bool) RTMpIsCpuOnline(RTCPUID idCpu)
{
    /** @todo check if there is a simpler interface than this... */
    int i = RTLinuxSysFsReadIntFile(0, "devices/system/cpu/cpu%d/online", (int)idCpu);
    if (    i == -1
        &&  RTLinuxSysFsExists("devices/system/cpu/cpu%d", (int)idCpu))
    {
        /** @todo Assert(!RTLinuxSysFsExists("devices/system/cpu/cpu%d/online",
         *               (int)idCpu));
         * Unfortunately, the online file wasn't always world readable (centos
         * 2.6.18-164). */
        i = 1;
    }

    AssertMsg(i == 0 || i == -1 || i == 1, ("i=%d\n", i));
    return i != 0 && i != -1;
}


RTDECL(bool) RTMpIsCpuPossible(RTCPUID idCpu)
{
    /** @todo check this up with hotplugging! */
    return RTLinuxSysFsExists("devices/system/cpu/cpu%d", (int)idCpu);
}


RTDECL(PRTCPUSET) RTMpGetSet(PRTCPUSET pSet)
{
    RTCpuSetEmpty(pSet);
    RTCPUID cMax = rtMpLinuxMaxCpus();
    for (RTCPUID idCpu = 0; idCpu < cMax; idCpu++)
        if (RTMpIsCpuPossible(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetCount(void)
{
    RTCPUSET Set;
    RTMpGetSet(&Set);
    return RTCpuSetCount(&Set);
}


RTDECL(PRTCPUSET) RTMpGetOnlineSet(PRTCPUSET pSet)
{
    RTCpuSetEmpty(pSet);
    RTCPUID cMax = rtMpLinuxMaxCpus();
    for (RTCPUID idCpu = 0; idCpu < cMax; idCpu++)
        if (RTMpIsCpuOnline(idCpu))
            RTCpuSetAdd(pSet, idCpu);
    return pSet;
}


RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    RTCPUSET Set;
    RTMpGetOnlineSet(&Set);
    return RTCpuSetCount(&Set);
}


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    int64_t kHz = RTLinuxSysFsReadIntFile(0, "devices/system/cpu/cpu%d/cpufreq/cpuinfo_cur_freq", (int)idCpu);
    if (kHz == -1)
    {
        /*
         * The file may be just unreadable - in that case use plan B, i.e.
         * /proc/cpuinfo to get the data we want. The assumption is that if
         * cpuinfo_cur_freq doesn't exist then the speed won't change, and
         * thus cur == max. If it does exist then cpuinfo contains the
         * current frequency.
         */
        kHz = rtMpLinuxGetFrequency(idCpu) * 1000;
    }
    return (kHz + 999) / 1000;
}


RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    int64_t kHz = RTLinuxSysFsReadIntFile(0, "devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", (int)idCpu);
    if (kHz == -1)
    {
        /*
         * Check if the file isn't there - if it is there, then /proc/cpuinfo
         * would provide current frequency information, which is wrong.
         */
        if (!RTLinuxSysFsExists("devices/system/cpu/cpu%d/cpufreq/cpuinfo_max_freq", (int)idCpu))
            kHz = rtMpLinuxGetFrequency(idCpu) * 1000;
        else
            kHz = 0;
    }
    return (kHz + 999) / 1000;
}
