/** @file
 *
 * VirtualBox Linux Additions mouse driver utility functions
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include <iprt/assert.h>
#include <VBox/VBoxGuest.h>
#include "VBoxUtils.h"

#include "xf86.h"
#define NEED_XF86_TYPES
#include "xf86_ansic.h"
#include "compiler.h"

#ifndef RT_OS_SOLARIS
#include <asm/ioctl.h>
#endif

#ifdef RT_OS_SOLARIS        /** @todo later Linux should also use R3 lib for this */
int VBoxMouseInit(void)
{
    int rc = VbglR3Init();
    if (RT_FAILURE(rc))
    {
        ErrorF("VbglR3Init failed.\n");
        return 1;
    }

    rc = VbglR3SetMouseStatus(VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE | VBOXGUEST_MOUSE_GUEST_NEEDS_HOST_CURSOR);
    if (VBOX_FAILURE(rc))
    {
        ErrorF("Error sending mouse pointer capabilities to VMM! rc = %d (%s)\n",
               errno, strerror(errno));
        return 1;
    }
    xf86Msg(X_INFO, "VirtualBox mouse pointer integration available.\n");
    return 0;
}


int VBoxMouseQueryPosition(unsigned int *puAbsXPos, unsigned int *puAbsYPos)
{
    int rc;
    uint32_t pointerXPos;
    uint32_t pointerYPos;

    AssertPtrReturn(puAbsXPos, VERR_INVALID_PARAMETER);
    AssertPtrReturn(puAbsYPos, VERR_INVALID_PARAMETER);
    rc = VbglR3GetMouseStatus(NULL, &pointerXPos, &pointerYPos);
    if (VBOX_SUCCESS(rc))
    {
        *puAbsXPos = pointerXPos;
        *puAbsYPos = pointerYPos;
        xf86Msg(X_INFO, "VBoxMouseQueryPosition x=%u y=%u\n", *puAbsXPos, *puAbsYPos);
        return 0;
    }
    ErrorF("Error querying host mouse position! rc = %d\n", rc);
    return 2;
}


int VBoxMouseFini(void)
{
    int rc = VbglR3SetMouseStatus(0);
    VbglR3Term();
    return rc;
}
#else
/* the vboxadd module file handle */
static int g_vboxaddHandle = -1;
/* the request structure */
static VMMDevReqMouseStatus *g_vmmreqMouseStatus = NULL;

/**
 * Initialise mouse integration.  Returns 0 on success and 1 on failure
 * (for example, if the VBox kernel module is not loaded).
 */
int VBoxMouseInit(void)
{
    VMMDevReqMouseStatus req;

    /* return immediately if already initialized */
    if (g_vboxaddHandle != -1)
        return 0;

    /* open the driver */
    g_vboxaddHandle = open(VBOXGUEST_DEVICE_NAME, O_RDWR, 0);
    if (g_vboxaddHandle < 0)
    {
        ErrorF("Unable to open the virtual machine device: %s\n",
               strerror(errno));
        return 1;
    }

    /* prepare the request structure */
    g_vmmreqMouseStatus = malloc(vmmdevGetRequestSize(VMMDevReq_GetMouseStatus));
    if (!g_vmmreqMouseStatus)
    {
        ErrorF("Ran out of memory while querying the virtual machine for the mouse capabilities.\n");
        return 1;
    }
    vmmdevInitRequest((VMMDevRequestHeader*)g_vmmreqMouseStatus, VMMDevReq_GetMouseStatus);

    /* tell the host that we want absolute coordinates */
    vmmdevInitRequest((VMMDevRequestHeader*)&req, VMMDevReq_SetMouseStatus);
    req.mouseFeatures = VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE | VBOXGUEST_MOUSE_GUEST_NEEDS_HOST_CURSOR;
    req.pointerXPos = 0;
    req.pointerYPos = 0;
    if (ioctl(g_vboxaddHandle, IOCTL_VBOXGUEST_VMMREQUEST, (void*)&req) < 0)
    {
        ErrorF("Error sending mouse pointer capabilities to VMM! rc = %d (%s)\n",
               errno, strerror(errno));
        return 1;
    }
    /* everything is fine, put out some branding */
    xf86Msg(X_INFO, "VirtualBox mouse pointer integration available.\n");
    return 0;
}

/**
 * Queries the absolute mouse position from the host.
 *
 * Returns 0 on success.
 * Returns 1 when the host doesn't want absolute coordinates (no coordinates returned)
 * Otherwise > 1 which means unsuccessful.
 */
int VBoxMouseQueryPosition(unsigned int *abs_x, unsigned int *abs_y)
{
    /* If we failed to initialise, say that we don't want absolute co-ordinates. */
    if (g_vboxaddHandle < 0)
        return 1;
    /* perform VMM request */
    if (ioctl(g_vboxaddHandle, IOCTL_VBOXGUEST_VMMREQUEST, (void*)g_vmmreqMouseStatus) >= 0)
    {
        if (VBOX_SUCCESS(g_vmmreqMouseStatus->header.rc))
        {
            /* does the host want absolute coordinates? */
            if (g_vmmreqMouseStatus->mouseFeatures & VBOXGUEST_MOUSE_HOST_CAN_ABSOLUTE)
            {
                *abs_x = g_vmmreqMouseStatus->pointerXPos;
                *abs_y = g_vmmreqMouseStatus->pointerYPos;
                return 0;
            }
            else
                return 1;
        }
        else
        {
            ErrorF("Error querying host mouse position! header.rc = %d\n", g_vmmreqMouseStatus->header.rc);
        }
    }
    else
    {
        ErrorF("Error performing VMM request! errno = %d (%s)\n",
               errno, strerror(errno));
    }
    /* error! */
    return 2;
}

int VBoxMouseFini(void)
{
    VMMDevReqMouseStatus req;
    /* If we are not initialised, there is nothing to do */
    if (g_vboxaddHandle < 0)
        return 0;
    /* tell VMM that we no longer support absolute mouse handling */
    vmmdevInitRequest((VMMDevRequestHeader*)&req, VMMDevReq_SetMouseStatus);
    req.mouseFeatures = 0;
    req.pointerXPos = 0;
    req.pointerYPos = 0;
    if (ioctl(g_vboxaddHandle, IOCTL_VBOXGUEST_VMMREQUEST, (void*)&req) < 0)
    {
        ErrorF("ioctl to vboxadd module failed, rc = %d (%s)\n",
               errno, strerror(errno));
    }

    free(g_vmmreqMouseStatus);
    g_vmmreqMouseStatus = NULL;
    close(g_vboxaddHandle);
    g_vboxaddHandle = -1;
    return 0;
}
#endif  /* !RT_OS_SOLARIS */

