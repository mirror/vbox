/* $Id$ */
/** @file
 * IPRT - Multiprocessor, FreeBSD.
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
#include <sys/sysctl.h>

#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/log.h>
#include <iprt/once.h>
#include <iprt/critsect.h>


RTDECL(uint32_t) RTMpGetCurFrequency(RTCPUID idCpu)
{
    int uFreqCurr = 0;
    size_t cbParameter = sizeof(uFreqCurr);

    /* CPU's have a common frequency. */
    int rc = sysctlbyname("dev.cpu.0.freq", &uFreqCurr, &cbParameter, NULL, NULL);
    if (rc)
        return 0;

    return (uint32_t)uFreqCurr;
}


RTDECL(uint32_t) RTMpGetMaxFrequency(RTCPUID idCpu)
{
    char szFreqLevels[20]; /* Should be enough to get the highest level which is always the first. */
    size_t cbFreqLevels = sizeof(szFreqLevels);

    memset(szFreqLevels, 0, sizeof(szFreqLevels));

    /*
     * CPU 0 has the freq levels entry. ENOMEM is ok as we don't need all supported
     * levels but only the first one.
     */
    int rc = sysctlbyname("dev.cpu.0.freq_levels", szFreqLevels, &cbFreqLevels, NULL, NULL);
    if (   (rc && (errno != ENOMEM))
        || (cbFreqLevels == 0))
        return 0;

    /* Clear everything starting from the '/' */
    unsigned i = 0;

    do
    {
        if (szFreqLevels[i] == '/')
        {
            memset(&szFreqLevels[i], 0, sizeof(szFreqLevels) - i);
            break;
        }
        i++;
    } while (i < sizeof(szFreqLevels));

    /* Returns 0 on failure. */
    return RTStrToUInt32(szFreqLevels);
}

RTDECL(RTCPUID) RTMpGetOnlineCount(void)
{
    /*
     * FreeBSD has sysconf.
     */
    return sysconf(_SC_NPROCESSORS_ONLN);
}

