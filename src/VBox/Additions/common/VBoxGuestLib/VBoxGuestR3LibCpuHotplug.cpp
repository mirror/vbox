/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions, CPU hotplug.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "VBGLR3Internal.h"

VBGLR3DECL(int) VbglR3CpuHotplugInit(void)
{
    int rc = VINF_SUCCESS;
    VMMDevCpuHotPlugStatusRequest Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_SetCpuHotPlugStatus);
    Req.enmStatusType = VMMDevCpuStatusType_Enable;

    rc = VbglR3CtlFilterMask(VMMDEV_EVENT_CPU_HOTPLUG, 0);
    if (RT_FAILURE(rc))
        return rc;

    rc = vbglR3GRPerform(&Req.header);
    if (RT_FAILURE(rc))
        VbglR3CtlFilterMask(0, VMMDEV_EVENT_CPU_HOTPLUG);

    return rc;
}

VBGLR3DECL(int) VbglR3CpuHotplugTerm(void)
{
    VMMDevCpuHotPlugStatusRequest Req;
    vmmdevInitRequest(&Req.header, VMMDevReq_SetCpuHotPlugStatus);
    Req.enmStatusType = VMMDevCpuStatusType_Disable;

    /* Clear the events. */
    VbglR3CtlFilterMask(0, VMMDEV_EVENT_CPU_HOTPLUG);

    int rc = vbglR3GRPerform(&Req.header);
    return rc;
}

VBGLR3DECL(int) VbglR3CpuHotplugWaitForEvent(VMMDevCpuEventType *penmEventType, uint32_t *pidCpuCore, uint32_t *pidCpuPackage)
{
    VBoxGuestWaitEventInfo waitEvent;
    int rc;

    AssertPtrReturn(penmEventType, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pidCpuCore, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pidCpuPackage, VERR_INVALID_PARAMETER);
    waitEvent.u32TimeoutIn = RT_INDEFINITE_WAIT;
    waitEvent.u32EventMaskIn = VMMDEV_EVENT_CPU_HOTPLUG;
    waitEvent.u32Result = VBOXGUEST_WAITEVENT_ERROR;
    waitEvent.u32EventFlagsOut = 0;
    rc = vbglR3DoIOCtl(VBOXGUEST_IOCTL_WAITEVENT, &waitEvent, sizeof(waitEvent));
    if (RT_SUCCESS(rc))
    {
        /* did we get the right event? */
        if (waitEvent.u32EventFlagsOut & VMMDEV_EVENT_CPU_HOTPLUG)
        {
            VMMDevGetCpuHotplugRequest Req;

            /* get the seamless change request */
            vmmdevInitRequest(&Req.header, VMMDevReq_GetCpuHotPlugRequest);
            Req.idCpuCore    = UINT32_MAX;
            Req.idCpuPackage = UINT32_MAX;
            Req.enmEventType = VMMDevCpuEventType_None;
            rc = vbglR3GRPerform(&Req.header);
            if (RT_SUCCESS(rc))
            {
                *penmEventType = Req.enmEventType;
                *pidCpuCore    = Req.idCpuCore;
                *pidCpuPackage = Req.idCpuPackage;
                return VINF_SUCCESS;
            }
        }
        else
            rc = VERR_TRY_AGAIN;
    }
    return rc;
}

