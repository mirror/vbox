/** @file
 *
 * VBoxGuestLib - A support library for VirtualBox guest additions:
 * Generic VMMDev request management
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

#include <VBox/VBoxGuestLib.h>
#include "VBGLInternal.h"
#include <iprt/asm.h>

#if defined(__LINUX__) && defined(__KERNEL__)
#include <linux/string.h>
#else
#include <string.h>
#endif

DECLVBGL(int) VbglGRAlloc (VMMDevRequestHeader **ppReq, uint32_t cbSize, VMMDevRequestType reqType)
{
    int rc = VbglEnter ();

    if (VBOX_FAILURE(rc))
    {
        return rc;
    }

    if (!ppReq || cbSize < sizeof (VMMDevRequestHeader))
    {
        dprintf(("VbglGRAlloc: Invalid parameter: ppReq = %p, cbSize = %d\n", ppReq, cbSize));
        return VERR_INVALID_PARAMETER;
    }

    VMMDevRequestHeader *pReq = (VMMDevRequestHeader *)VbglPhysHeapAlloc (cbSize);

    if (!pReq)
    {
        dprintf(("VbglGRAlloc: no memory\n"));
        rc = VERR_NO_MEMORY;
    }
    else
    {
        memset(pReq, 0xAA, cbSize);

        pReq->size        = cbSize;
        pReq->version     = VMMDEV_REQUEST_HEADER_VERSION;
        pReq->requestType = reqType;
        pReq->rc          = VERR_GENERAL_FAILURE;
        pReq->reserved1   = 0;
        pReq->reserved2   = 0;

        *ppReq = pReq;
    }

    return rc;
}

DECLVBGL(int) VbglGRPerform (VMMDevRequestHeader *pReq)
{
    int rc = VbglEnter ();

    if (VBOX_FAILURE(rc))
    {
        return rc;
    }

    if (!pReq)
    {
        return VERR_INVALID_PARAMETER;
    }

    if (g_vbgldata.portVMMDev == 0)
    {
        return VERR_VBGL_NOT_INITIALIZED;
    }

    RTCCPHYS physaddr = VbglPhysHeapGetPhysAddr (pReq);

    if (!physaddr)
    {
        rc = VERR_VBGL_INVALID_ADDR;
    }
    else
    {
        ASMOutU32(g_vbgldata.portVMMDev + PORT_VMMDEV_REQUEST_OFFSET, (uint32_t)physaddr);

        rc = pReq->rc;
    }
    return rc;
}

DECLVBGL(void) VbglGRFree (VMMDevRequestHeader *pReq)
{
    int rc = VbglEnter ();

    if (VBOX_FAILURE(rc))
    {
        return;
    }

    VbglPhysHeapFree (pReq);
}
