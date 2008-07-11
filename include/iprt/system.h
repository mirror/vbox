/** @file
 * IPRT - System.
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

#ifndef ___iprt_system_h
#define ___iprt_system_h

#include <iprt/cdefs.h>
#include <iprt/types.h>

#define IPRT_USAGE_MULTIPLIER   UINT64_C(1000000000)

/**
 * This structure holds both computed and raw values of overall CPU load counters.
 *
 * @todo r=bird: What does these values mean?
 *
 *               Also, I no longer use 'u32', 'u16', 'u8', 'u64', etc unless this information
 *               is really important for the user. So, unless there is a better prefix just
 *               stick to 'u' here.
 *
 *               The name of the struct should include the prefix or a shortened
 *               version of it: RTSYSCPUUSAGESTATS
 *
 *               Also, I'd sugest calling it RTSYSCPULOADSTATS, replacing 'usage' with 'load',
 *               no particular reason, other than that it is easier to read in the (silly)
 *               condensed form we use for typedefs here.
 *
 *               Finally, I'm wondering how portable this is. I'd like to see what APIs are
 *               available on the important systems (Windows, Solaris, Linux) and compare
 *               the kind of info they return. This should be done in the defect *before*
 *               any of the above, please.
 */
typedef struct RTSYSCPUUSAGESTATS
{
    uint32_t u32User;
    uint32_t u32System;
    uint32_t u32Idle;
    /* Internal raw counter values. */
    uint32_t u32RawUser;
    uint32_t u32RawNice;
    uint32_t u32RawSystem;
    uint32_t u32RawIdle;
} RTCPUUSAGESTATS;
typedef RTCPUUSAGESTATS *PRTCPUUSAGESTATS;

/* This structure holds both computed and raw values of per-VM CPU load counters. */
typedef struct
{
    uint32_t u32User;
    uint32_t u32System;
    /* Internal raw counter values. */
    uint64_t u64RawTotal;
    uint32_t u32RawProcUser;
    uint32_t u32RawProcSystem;
} RTPROCCPUUSAGESTATS;
typedef RTPROCCPUUSAGESTATS *PRTPROCCPUUSAGESTATS;


__BEGIN_DECLS

/** @defgroup grp_rt_system RTSystem - System Information
 * @ingroup grp_rt
 * @{
 */

/**
 * Gets the number of logical (not physical) processors in the system.
 *
 * @returns Number of logical processors in the system.
 *
 * @todo Replaced by RTMpGetOnlineCount / RTMpGetCount, retire this guy.
 */
RTDECL(unsigned) RTSystemProcessorGetCount(void);

/**
 * Gets the active logical processor mask.
 *
 * @returns Active logical processor mask. (bit 0 == logical cpu 0)
 *
 * @todo Replaced by RTMpGetOnlineSet, retire this guy.
 */
RTDECL(uint64_t) RTSystemProcessorGetActiveMask(void);

/**
 * Gets the current figures of overall system processor usage.
 *
 * @remarks To get meaningful stats this function has to be
 *          called twice with a bit of delay between calls. This
 *          is due to the fact that at least two samples of
 *          system usage stats are needed to calculate the load.
 *
 * @returns IPRT status code.
 * @param   pStats  Pointer to the structure that contains the
 *                  results. Note that this structure is
 *                  modified with each call to this function and
 *                  is used to provide both in and out values.
 * @todo r=bird: Change to RTSystemGetCpuLoadStats.
 */
RTDECL(int) RTSystemProcessorGetUsageStats(PRTCPUUSAGESTATS pStats);

/**
 * Gets the current processor usage for a partucilar process.
 *
 * @remarks To get meaningful stats this function has to be
 *          called twice with a bit of delay between calls. This
 *          is due to the fact that at least two samples of
 *          system usage stats are needed to calculate the load.
 *
 * @returns IPRT status code.
 * @param   pid     VM process id.
 * @param   pStats  Pointer to the structure that contains the
 *                  results. Note that this structure is
 *                  modified with each call to this function and
 *                  is used to provide both in and out values.
 *
 * @todo    Perharps this function should be moved somewhere
 *          else.
 * @todo    r=bird: Yes is should, iprt/proc.h. RTProcGetCpuLoadStats.
 */
RTDECL(int) RTProcessGetProcessorUsageStats(RTPROCESS pid, PRTPROCCPUUSAGESTATS pStats);

/** @} */

__END_DECLS

#endif

