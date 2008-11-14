/** @file
 *
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#include "the-linux-kernel.h"
#include "vboxmod.h"

/**
 * HGCM
 *
 */
static DECLVBGL(void)
vboxadd_hgcm_callback (VMMDevHGCMRequestHeader *pHeader, void *pvData, uint32_t u32Data)
{
    VBoxDevice *dev = pvData;
    wait_event (dev->eventq, pHeader->fu32Flags & VBOX_HGCM_REQ_DONE);
}

static DECLVBGL(void)
vboxadd_hgcm_callback_interruptible (VMMDevHGCMRequestHeader *pHeader, void *pvData, uint32_t u32Data)
{
    VBoxDevice *dev = pvData;
    wait_event_interruptible (dev->eventq, pHeader->fu32Flags & VBOX_HGCM_REQ_DONE);
}

static DECLVBGL(void)
vboxadd_hgcm_callback_timeout (VMMDevHGCMRequestHeader *pHeader, void *pvData, uint32_t u32Data)
{
    VBoxDevice *dev = pvData;
    wait_event_interruptible_timeout (dev->eventq, pHeader->fu32Flags & VBOX_HGCM_REQ_DONE,
                                      msecs_to_jiffies (u32Data));
}

DECLVBGL (int) vboxadd_cmc_call (void *opaque, uint32_t func, void *data)
{
    int rc = VINF_SUCCESS;

    /* this function can handle cancelled requests */
    if (   VBOXGUEST_IOCTL_STRIP_SIZE(func)
        == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL(0)))
        rc = VbglHGCMCall (data, vboxadd_hgcm_callback_interruptible, opaque, 0);
    /* this function can handle cancelled requests */
    else if (   VBOXGUEST_IOCTL_STRIP_SIZE(func)
             == VBOXGUEST_IOCTL_STRIP_SIZE(VBOXGUEST_IOCTL_HGCM_CALL_TIMEOUT(0)))
    {
        VBoxGuestHGCMCallInfoTimeout *pCallInfo;
        pCallInfo = (VBoxGuestHGCMCallInfoTimeout *) data;
        rc = VbglHGCMCall (&pCallInfo->info, vboxadd_hgcm_callback_timeout,
                           opaque, pCallInfo->u32Timeout);
    }
    else switch (func)
    {
        /* this function can NOT handle cancelled requests */
        case VBOXGUEST_IOCTL_HGCM_CONNECT:
            rc = VbglHGCMConnect (data, vboxadd_hgcm_callback, opaque, 0);
            break;

        /* this function can NOT handle cancelled requests */
        case VBOXGUEST_IOCTL_HGCM_DISCONNECT:
            rc = VbglHGCMDisconnect (data, vboxadd_hgcm_callback, opaque, 0);
            break;

        default:
            rc = VERR_VBGL_IOCTL_FAILED;
    }
    return rc;
}

int vboxadd_cmc_init (void)
{
    return 0;
}

void vboxadd_cmc_fini (void)
{
}

DECLVBGL (int)
vboxadd_cmc_ctl_guest_filter_mask (uint32_t or_mask, uint32_t not_mask)
{
    int rc;
    VMMDevCtlGuestFilterMask *req;

    rc = VbglGRAlloc ((VMMDevRequestHeader**) &req, sizeof (*req),
                      VMMDevReq_CtlGuestFilterMask);

    if (RT_FAILURE (rc))
    {
        elog ("VbglGRAlloc (CtlGuestFilterMask) failed rc=%d\n", rc);
        return -1;
    }

    req->u32OrMask = or_mask;
    req->u32NotMask = not_mask;

    rc = VbglGRPerform (&req->header);
    VbglGRFree (&req->header);
    if (RT_FAILURE (rc))
    {
        elog ("VbglGRPerform (CtlGuestFilterMask) failed rc=%d\n", rc);
        return -1;
    }
    return 0;
}

EXPORT_SYMBOL (vboxadd_cmc_call);
EXPORT_SYMBOL (vboxadd_cmc_ctl_guest_filter_mask);
