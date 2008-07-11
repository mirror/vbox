/* $Id$ */
/** @file
 * IPRT - System, POSIX.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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
#include <iprt/system.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/string.h>
#include <iprt/mem.h>

#include <unistd.h>
#include <stdio.h>
#if !defined(RT_OS_SOLARIS)
# include <sys/sysctl.h>
#endif


/**
 * Gets the number of logical (not physical) processors in the system.
 *
 * @returns Number of logical processors in the system.
 */
RTR3DECL(unsigned) RTSystemProcessorGetCount(void)
{
    int cCpus; NOREF(cCpus);

    /*
     * The sysconf way (linux and others).
     */
#ifdef _SC_NPROCESSORS_ONLN
    cCpus = sysconf(_SC_NPROCESSORS_ONLN);
    if (cCpus >= 1)
        return cCpus;
#endif

    /*
     * The BSD 4.4 way.
     */
#if defined(CTL_HW) && defined(HW_NCPU)
    int aiMib[2];
    aiMib[0] = CTL_HW;
    aiMib[1] = HW_NCPU;
    cCpus = -1;
    size_t cb = sizeof(cCpus);
    int rc = sysctl(aiMib, ELEMENTS(aiMib), &cCpus, &cb, NULL, 0);
    if (rc != -1 && cCpus >= 1)
        return cCpus;
#endif
    return 1;
}


/**
 * Gets the active logical processor mask.
 *
 * @returns Active logical processor mask. (bit 0 == logical cpu 0)
 */
RTR3DECL(uint64_t) RTSystemProcessorGetActiveMask(void)
{
    int cCpus = RTSystemProcessorGetCount();
    return ((uint64_t)1 << cCpus) - 1;
}

/**
 * Gets the current figures of overall system processor usage.
 *
 * @remarks To get meaningful stats this function has to be
 *          called twice with a bit of delay between calls. This
 *          is due to the fact that at least two samples of
 *          system usage stats are needed to calculate the load.
 *
 * @returns None.
 */
RTDECL(int) RTSystemProcessorGetUsageStats(PRTCPUUSAGESTATS pStats)
{
/** @todo r=bird: This is Linux specific and doesn't belong here. Move this to r3/linux/RTSystemGetCpuLoadStats-linux.cpp. */
    int rc = VINF_SUCCESS;
    uint32_t u32UserNow, u32NiceNow, u32SystemNow, u32IdleNow;
    uint32_t u32UserDelta, u32SystemDelta, u32IdleDelta, u32BusyDelta, u32TotalDelta;
    FILE *f = fopen("/proc/stat", "r");

    if (f)
    {
        if (fscanf(f, "cpu %u %u %u %u", &u32UserNow, &u32NiceNow, &u32SystemNow, &u32IdleNow) == 4)
        {
            u32UserDelta   = (u32UserNow - pStats->u32RawUser) + (u32NiceNow - pStats->u32RawNice);
            u32SystemDelta = u32SystemNow - pStats->u32RawSystem;
            u32IdleDelta   = u32IdleNow - pStats->u32RawIdle;
            u32BusyDelta   = u32UserDelta + u32SystemDelta;
            u32TotalDelta  = u32BusyDelta + u32IdleDelta;
            pStats->u32User   = (uint32_t)(IPRT_USAGE_MULTIPLIER * u32UserDelta / u32TotalDelta);
            pStats->u32System = (uint32_t)(IPRT_USAGE_MULTIPLIER * u32SystemDelta / u32TotalDelta);
            pStats->u32Idle   = (uint32_t)(IPRT_USAGE_MULTIPLIER * u32IdleDelta / u32TotalDelta);
            /* Update the base. */
            pStats->u32RawUser   = u32UserNow;
            pStats->u32RawNice   = u32NiceNow;
            pStats->u32RawSystem = u32SystemNow;
            pStats->u32RawIdle   = u32IdleNow;
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}

/**
 * Gets the current processor usage for a partucilar process.
 *
 * @remarks To get meaningful stats this function has to be
 *          called twice with a bit of delay between calls. This
 *          is due to the fact that at least two samples of
 *          system usage stats are needed to calculate the load.
 *
 * @returns None.
 */
RTDECL(int) RTProcessGetProcessorUsageStats(RTPROCESS pid, PRTPROCCPUUSAGESTATS pStats)
{
    int rc = VINF_SUCCESS;
    uint32_t u32UserNow, u32NiceNow, u32SystemNow, u32IdleNow;
    uint32_t u32UserDelta, u32SystemDelta;
    uint64_t u64TotalNow, u64TotalDelta;

/** @todo r=bird: This is Linux specific and doesn't belong here. Move this to r3/linux/RTSystemGetCpuLoadStats-linux.cpp. */
    FILE *f = fopen("/proc/stat", "r");

    if (f)
    {
        if (fscanf(f, "cpu %u %u %u %u", &u32UserNow, &u32NiceNow, &u32SystemNow, &u32IdleNow) == 4) /** @todo 'uint32_t' is not necessarily the same as 'unsigned int'. */
        {
            char *pszName;
            pid_t pid2;
            char c;
            int iTmp;
            unsigned uTmp;
            unsigned long ulTmp, ulUserNow, ulSystemNow;
            char buf[80]; /* @todo: this should be tied to max allowed proc name. */

            u64TotalNow = (uint64_t)u32UserNow + u32NiceNow + u32SystemNow + u32IdleNow;
            fclose(f);
            RTStrAPrintf(&pszName, "/proc/%d/stat", pid);
            //printf("Opening %s...\n", pszName);
            f = fopen(pszName, "r");
            RTMemFree(pszName);

            if (f)
            {
                if (fscanf(f, "%d %s %c %d %d %d %d %d %u %lu %lu %lu %lu %lu %lu",
                           &pid2, buf, &c, &iTmp, &iTmp, &iTmp, &iTmp, &iTmp, &uTmp,
                           &ulTmp, &ulTmp, &ulTmp, &ulTmp, &ulUserNow, &ulSystemNow) == 15)
                {
                    Assert((pid_t)pid == pid2);
                    u32UserDelta      = ulUserNow - pStats->u32RawProcUser;
                    u32SystemDelta    = ulSystemNow - pStats->u32RawProcSystem;
                    u64TotalDelta     = u64TotalNow - pStats->u64RawTotal;
                    pStats->u32User   = (uint32_t)(IPRT_USAGE_MULTIPLIER * u32UserDelta / u64TotalDelta);
                    pStats->u32System = (uint32_t)(IPRT_USAGE_MULTIPLIER * u32SystemDelta / u64TotalDelta);
//                  printf("%d: user=%u%% system=%u%% / raw user=%u raw system=%u total delta=%u\n", pid,
//                         pStats->u32User / 10000000, pStats->u32System / 10000000,
//                         pStats->u32RawProcUser, pStats->u32RawProcSystem, u64TotalDelta);
                    /* Update the base. */
                    pStats->u32RawProcUser   = ulUserNow;
                    pStats->u32RawProcSystem = ulSystemNow;
                    pStats->u64RawTotal      = u64TotalNow;
//                  printf("%d: updated raw user=%u raw system=%u raw total=%u\n", pid,
//                         pStats->u32RawProcUser, pStats->u32RawProcSystem, pStats->u64RawTotal);
                }
                else
                    rc = VERR_FILE_IO_ERROR;
            }
            else
                rc = VERR_ACCESS_DENIED;
        }
        else
            rc = VERR_FILE_IO_ERROR;
        fclose(f);
    }
    else
        rc = VERR_ACCESS_DENIED;

    return rc;
}
