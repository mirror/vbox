/** @file
 *
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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

static int vboxadd_hgcm_connect (VBoxDevice *dev, VBoxGuestHGCMConnectInfo *info)
{
    return VbglHGCMConnect (info, vboxadd_hgcm_callback, dev, 0);
}

static int vboxadd_hgcm_disconnect (VBoxDevice *dev, VBoxGuestHGCMDisconnectInfo *info)
{
    return VbglHGCMDisconnect (info, vboxadd_hgcm_callback, dev, 0);
}

static int vboxadd_hgcm_call (VBoxDevice *dev, VBoxGuestHGCMCallInfo *info)
{
    return VbglHGCMCall (info, vboxadd_hgcm_callback, dev, 0);
}

DECLVBGL (int) vboxadd_cmc_call (void *opaque, uint32_t func, void *data)
{
    switch (func)
    {
        case IOCTL_VBOXGUEST_HGCM_CONNECT:
            return vboxadd_hgcm_connect (opaque, data);

        case IOCTL_VBOXGUEST_HGCM_DISCONNECT:
            return vboxadd_hgcm_disconnect (opaque, data);

        case IOCTL_VBOXGUEST_HGCM_CALL:
            return vboxadd_hgcm_call (opaque, data);

        default:
            return VERR_VBGL_IOCTL_FAILED;
    }
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

    if (VBOX_FAILURE (rc))
    {
        elog ("VbglGRAlloc (CtlGuestFilterMask) failed rc=%d\n", rc);
        return -1;
    }

    req->u32OrMask = or_mask;
    req->u32NotMask = not_mask;

    rc = VbglGRPerform (&req->header);
    VbglGRFree (&req->header);
    if (VBOX_FAILURE (rc))
    {
        elog ("VbglGRPerform (CtlGuestFilterMask) failed rc=%d\n", rc);
        return -1;
    }
    return 0;
}

EXPORT_SYMBOL (vboxadd_cmc_call);
EXPORT_SYMBOL (vboxadd_cmc_ctl_guest_filter_mask);
