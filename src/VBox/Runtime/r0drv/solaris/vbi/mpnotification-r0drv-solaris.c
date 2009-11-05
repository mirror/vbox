/* $Id$ */
/** @file
 * IPRT - Multiprocessor Event Notifications, Ring-0 Driver, Solaris.
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
#include "../the-solaris-kernel.h"
#include "internal/iprt.h"

#include <iprt/err.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>
#include "r0drv/mp-r0drv.h"


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** CPU watch callback handle. */
static vbi_cpu_watch_t *g_hVbiCpuWatch = NULL;
/** Set of online cpus that is maintained by the MP callback.
 * This avoids locking issues quering the set from the kernel as well as
 * eliminating any uncertainty regarding the online status during the
 * callback. */
RTCPUSET g_rtMpSolarisCpuSet;


static void rtMpNotificationSolarisCallback(void *pvUser, int iCpu, int online)
{
    NOREF(pvUser);

    /* ASSUMES iCpu == RTCPUID */
    if (online)
    {
        RTCpuSetAdd(&g_rtMpSolarisCpuSet, iCpu);
        rtMpNotificationDoCallbacks(RTMPEVENT_ONLINE, iCpu);
    }
    else
    {
        RTCpuSetDel(&g_rtMpSolarisCpuSet, iCpu);
        rtMpNotificationDoCallbacks(RTMPEVENT_OFFLINE, iCpu);
    }
}


int rtR0MpNotificationNativeInit(void)
{
    if (vbi_revision_level < 2)
        return VERR_NOT_SUPPORTED;
    if (g_hVbiCpuWatch != NULL)
        return VERR_WRONG_ORDER;

    /*
     * Register the callback building the online cpu set as we
     * do so (current_too = 1).
     */
    RTCpuSetEmpty(&g_rtMpSolarisCpuSet);
    g_hVbiCpuWatch = vbi_watch_cpus(rtMpNotificationSolarisCallback, NULL, 1 /*current_too*/);

    return VINF_SUCCESS;
}


void rtR0MpNotificationNativeTerm(void)
{
    if (vbi_revision_level >= 2 && g_hVbiCpuWatch != NULL)
        vbi_ignore_cpus(g_hVbiCpuWatch);
    g_hVbiCpuWatch = NULL;
}

