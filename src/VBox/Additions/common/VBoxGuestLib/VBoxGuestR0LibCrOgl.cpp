/* $Id$ */
/** @file
 * VBoxGuestLib - Ring-3 Support Library for VirtualBox guest additions, Chromium OpenGL Service.
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

/* Entire file is ifdef'ed with !VBGL_VBOXGUEST */
#ifdef VBGL_VBOXGUEST
# error "VBGL_VBOXGUEST should not be defined"
#else

#include <iprt/string.h>

#include "VBGLInternal.h"


DECLVBGL(int) VbglR0CrCtlCreate(VBGLCRCTLHANDLE *phCtl)
{
    int rc;

    if (phCtl)
    {
        struct VBGLHGCMHANDLEDATA *pHandleData = vbglHGCMHandleAlloc();

        if (pHandleData)
        {
            rc = vbglDriverOpen(&pHandleData->driver);

            if (RT_SUCCESS(rc))
            {
                *phCtl = pHandleData;
                return VINF_SUCCESS;
            }

            vbglHGCMHandleFree(pHandleData);
        }
        else
            rc = VERR_NO_MEMORY;

        *phCtl = NULL;
    }
    else
        rc = VERR_INVALID_PARAMETER;

    return rc;
}

DECLVBGL(int) VbglR0CrCtlDestroy(VBGLCRCTLHANDLE hCtl)
{
    vbglDriverClose(&hCtl->driver);

    vbglHGCMHandleFree(hCtl);

    return VINF_SUCCESS;
}

DECLVBGL(int) VbglR0CrCtlConConnect(VBGLCRCTLHANDLE hCtl, HGCMCLIENTID *pidClient)
{
    VBoxGuestHGCMConnectInfo info;
    int rc;

    if (!hCtl || !pidClient)
        return VERR_INVALID_PARAMETER;

    RT_ZERO(info);
    info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    RTStrCopy(info.Loc.u.host.achName, sizeof(info.Loc.u.host.achName), "VBoxSharedCrOpenGL");
    rc = vbglDriverIOCtl(&hCtl->driver, VBOXGUEST_IOCTL_HGCM_CONNECT, &info, sizeof(info));
    if (RT_SUCCESS(rc))
    {
        rc = info.result;
        if (RT_SUCCESS(rc))
        {
            Assert(info.u32ClientID);
            *pidClient = info.u32ClientID;
            return rc;
        }
    }

    AssertRC(rc);
    *pidClient = 0;
    return rc;
}

DECLVBGL(int) VbglR0CrCtlConDisconnect(VBGLCRCTLHANDLE hCtl, HGCMCLIENTID idClient)
{
    VBoxGuestHGCMDisconnectInfo info;
    RT_ZERO(info);
    info.u32ClientID = idClient;
    return vbglDriverIOCtl(&hCtl->driver, VBOXGUEST_IOCTL_HGCM_DISCONNECT, &info, sizeof(info));
}

DECLVBGL(int) VbglR0CrCtlConCall(VBGLCRCTLHANDLE hCtl, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo)
{
    return vbglDriverIOCtl(&hCtl->driver, VBOXGUEST_IOCTL_HGCM_CALL(cbCallInfo), pCallInfo, cbCallInfo);
}

DECLVBGL(int) VbglR0CrCtlConCallUserData(VBGLCRCTLHANDLE hCtl, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo)
{
    return vbglDriverIOCtl(&hCtl->driver, VBOXGUEST_IOCTL_HGCM_CALL_USERDATA(cbCallInfo), pCallInfo, cbCallInfo);
}

#endif /* #ifndef VBGL_VBOXGUEST */
