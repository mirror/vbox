/* $Id$ */
/** @file
 * VBoxGuestLibR0 - Mouse Integration.
 */

/*
 * Copyright (C) 2012-2016 Oracle Corporation
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

#include "VBGLInternal.h"

/**
 * Sets the function which is called back on each mouse pointer event.  Only
 * one callback can be active at once, so if you need several for any reason
 * you must multiplex yourself.  Call backs can be disabled by passing NULL
 * as the function pointer.
 *
 * @remarks Ring-0.
 * @returns iprt status code.
 * @returns VERR_TRY_AGAIN if the main guest driver hasn't finished
 *          initialising.
 *
 * @param   pfnNotify  the function to call back.  NULL to disable call backs.
 * @param   pvUser     user supplied data/cookie to be passed to the function.
 */
DECLVBGL(int) VbglSetMouseNotifyCallback(PFNVBOXGUESTMOUSENOTIFY pfnNotify, void *pvUser)
{
    VBoxGuestMouseSetNotifyCallback NotifyCallback;
    VBGLDRIVER *pDriver;
    int rc = vbglGetDriver(&pDriver);
    if (RT_FAILURE(rc))
        return rc;
    NotifyCallback.pfnNotify = pfnNotify;
    NotifyCallback.pvUser    = pvUser;
    return vbglDriverIOCtl(pDriver, VBOXGUEST_IOCTL_SET_MOUSE_NOTIFY_CALLBACK, &NotifyCallback, sizeof(NotifyCallback));
}

/**
 * Retrieve mouse coordinates and features from the host.
 *
 * @remarks Ring-0.
 * @returns VBox status code.
 *
 * @param   pfFeatures  Where to store the mouse features.
 * @param   px          Where to store the X co-ordinate.
 * @param   py          Where to store the Y co-ordinate.
 */
DECLVBGL(int) VbglGetMouseStatus(uint32_t *pfFeatures, uint32_t *px, uint32_t *py)
{
    VMMDevReqMouseStatus Req;
    VBGLDRIVER *pDriver;
    int rc;

    rc = vbglGetDriver(&pDriver);
    if (RT_FAILURE(rc))
        return rc;
    vmmdevInitRequest(&Req.header, VMMDevReq_GetMouseStatus);
    Req.mouseFeatures = 0;
    Req.pointerXPos = 0;
    Req.pointerYPos = 0;
    rc = vbglDriverIOCtl(pDriver, VBOXGUEST_IOCTL_VMMREQUEST(sizeof(Req)), &Req.header, sizeof(Req));
    if (RT_FAILURE(rc))
        return rc;
    if (RT_FAILURE(Req.header.rc))
        return Req.header.rc;
    if (pfFeatures)
        *pfFeatures = Req.mouseFeatures;
    if (px)
        *px = Req.pointerXPos;
    if (py)
        *py = Req.pointerYPos;
    return VINF_SUCCESS;
}

/**
 * Send mouse features to the host.
 *
 * @remarks Ring-0.
 * @returns VBox status code.
 *
 * @param   fFeatures  Supported mouse pointer features.  The main guest driver
 *                     will mediate different callers and show the host any
 *                     feature enabled by any guest caller.
 */
DECLVBGL(int) VbglSetMouseStatus(uint32_t fFeatures)
{
    VBGLDRIVER *pDriver;
    int rc;

    rc = vbglGetDriver(&pDriver);
    if (RT_FAILURE(rc))
        return rc;
    return vbglDriverIOCtl(pDriver, VBOXGUEST_IOCTL_SET_MOUSE_STATUS, &fFeatures, sizeof(fFeatures));
}

