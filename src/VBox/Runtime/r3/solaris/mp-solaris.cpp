/* $Id$ */
/** @file
 * IPRT - Multiprocessor, Solaris.
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
#define LOG_GROUP RTLOGGROUP_DEFAULT
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <kstat.h>

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/log.h>

static kstat_ctl_t *g_kc;
static kstat_t    **g_cpuInfo;
static RTCPUID      g_nCPUs;

void rtLookupCpuInfoStats()
{
    g_kc = kstat_open();
    if (!g_kc)
    {
        Log(("kstat_open() -> %d\n", errno));
        return;
    }

    g_nCPUs = RTMpGetCount();
    g_cpuInfo = (kstat_t**)RTMemAlloc(g_nCPUs * sizeof(kstat_t*));
    if (!g_cpuInfo)
    {
        Log(("RTMemAlloc() -> NULL\n"));
        return;
    }

    RTCPUID i = 0;
    kstat_t *ksp;
    for (ksp = g_kc->kc_chain; ksp != NULL; ksp = ksp->ks_next)
    {
        if (strcmp(ksp->ks_module, "cpu_info") == 0)
        {
            g_cpuInfo[i++] = ksp;
        }
        Assert(i <= g_nCPUs);
    }
}

static uint64_t rtMpGetFrequency(RTCPUID idCpu, char *statName)
{
    if (!g_kc)
        rtLookupCpuInfoStats();

    if (idCpu < g_nCPUs && g_cpuInfo[idCpu])
        if (kstat_read(g_kc, g_cpuInfo[idCpu], 0) != -1)
        {
            kstat_named_t *kn;
            kn = (kstat_named_t *)kstat_data_lookup(g_cpuInfo[idCpu], statName);
            if (kn)
                return kn->value.ul;
            else
                Log(("kstat_data_lookup(%s) -> %d\n", statName, errno));
        }
        else
            Log(("kstat_read() -> %d\n", errno));
    else
        Log(("invalid idCpu: %d\n", idCpu));

    return 0;
}


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    return rtMpGetFrequency(idCpu, "current_clock_Hz") / 1000000;
}

RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    return rtMpGetFrequency(idCpu, "clock_MHz");
}
