/* $Id$ */
/** @file
 * IPRT - Multiprocessor Event Notifications, Ring-0 Driver, Linux.
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
 *
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"

#include <iprt/mp.h>
#include <iprt/err.h>
#include <iprt/cpuset.h>
#include "r0drv/mp-r0drv.h"


#if LINUX_VERSION_CODE >= KERNEL_VERSION(2, 5, 71) && defined(CONFIG_SMP)

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int rtMpNotificationLinuxCallback(struct notifier_block *pNotifierBlock, unsigned long ulNativeEvent, void *pvCpu);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * The notifier block we use for registering the callback.
 */
static struct notifier_block g_NotifierBlock =
{
    .notifier_call = rtMpNotificationLinuxCallback,
    .next = NULL,
    .priority = 0
};

#ifdef CPU_DOWN_FAILED
/**
 * The set of CPUs we've seen going offline recently.
 */
static RTCPUSET g_MpPendingOfflineSet;
#endif


/**
 * The native callback.
 *
 * @returns 0.
 * @param   pNotifierBlock  Pointer to g_NotifierBlock.
 * @param   ulNativeEvent   The native event.
 * @param   pvCpu           The cpu id cast into a pointer value.
 */
static int rtMpNotificationLinuxCallback(struct notifier_block *pNotifierBlock, unsigned long ulNativeEvent, void *pvCpu)
{
    RTCPUID idCpu = (uintptr_t)pvCpu;
    NOREF(pNotifierBlock);

    /* ASSUMES iCpu == RTCPUID */
    switch (ulNativeEvent)
    {
#ifdef CPU_DOWN_FAILED
        case CPU_DOWN_FAILED:
# ifdef CPU_TASKS_FROZEN
        case CPU_DOWN_FAILED_FROZEN:
# endif
            if (!RTCpuSetIsMember(&g_MpPendingOfflineSet, idCpu))
                return 0;
        /* fall thru */
#endif
        case CPU_ONLINE:
# ifdef CPU_TASKS_FROZEN
        case CPU_ONLINE_FROZEN:
# endif
            RTCpuSetDel(&g_MpPendingOfflineSet, idCpu);
            rtMpNotificationDoCallbacks(RTMPEVENT_ONLINE, idCpu);
            break;

#ifdef CPU_DOWN_PREPARE
        case CPU_DOWN_PREPARE:
# ifdef CPU_TASKS_FROZEN
        case CPU_DOWN_PREPARE_FROZEN:
# endif
#else
        case CPU_DEAD:
# ifdef CPU_TASKS_FROZEN
        case CPU_DEAD_FROZEN:
# endif
#endif
            rtMpNotificationDoCallbacks(RTMPEVENT_OFFLINE, idCpu);
            RTCpuSetAdd(&g_MpPendingOfflineSet, idCpu);
            break;
    }

    return NOTIFY_DONE;
}


int rtR0MpNotificationNativeInit(void)
{
    int rc;

#ifdef CPU_DOWN_FAILED
    RTCpuSetEmpty(&g_MpPendingOfflineSet);
#endif

    rc = register_cpu_notifier(&g_NotifierBlock);
    AssertMsgReturn(!rc, ("%d\n", rc), RTErrConvertFromErrno(rc));
    return VINF_SUCCESS;
}


void rtR0MpNotificationNativeTerm(void)
{
    unregister_cpu_notifier(&g_NotifierBlock);
}

#else   /* Not supported / Not needed */

int rtR0MpNotificationNativeInit(void)
{
    return VINF_SUCCESS;
}

void rtR0MpNotificationNativeTerm(void)
{
}

#endif  /* Not supported / Not needed */

