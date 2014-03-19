/** @file
 * VBoxGuestLib - Central calls header.
 */

/*
 * Copyright (C) 2012 Oracle Corporation
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
#include "VBoxGuestR0LibCrOgl.h"

#include <iprt/string.h>

#include "VBGLInternal.h"

struct VBGLHGCMHANDLEDATA *vbglHGCMHandleAlloc (void);
void vbglHGCMHandleFree (struct VBGLHGCMHANDLEDATA *pHandle);

DECLVBGL(int) vboxCrCtlCreate(HVBOXCRCTL *phCtl)
{
    int rc;
    struct VBGLHGCMHANDLEDATA *pHandleData;

    if (!phCtl)
        return VERR_INVALID_PARAMETER;

    pHandleData = vbglHGCMHandleAlloc ();

    rc = VINF_SUCCESS;

    if (!pHandleData)
    {
        rc = VERR_NO_MEMORY;
    }
    else
    {
        rc = vbglDriverOpen (&pHandleData->driver);

        if (RT_SUCCESS(rc))
        {
            *phCtl = pHandleData;
            return VINF_SUCCESS;
        }

        vbglHGCMHandleFree (pHandleData);
    }

    *phCtl = NULL;
    return rc;
}

DECLVBGL(int) vboxCrCtlDestroy(HVBOXCRCTL hCtl)
{
    vbglDriverClose(&hCtl->driver);

    vbglHGCMHandleFree(hCtl);

    return VINF_SUCCESS;
}

DECLVBGL(int) vboxCrCtlConConnect(HVBOXCRCTL hCtl, uint32_t *pu32ClientID)
{
    VBoxGuestHGCMConnectInfo info;
    int rc;

    if (!hCtl || !pu32ClientID)
        return VERR_INVALID_PARAMETER;

    memset(&info, 0, sizeof (info));
    info.Loc.type = VMMDevHGCMLoc_LocalHost_Existing;
    RTStrCopy(info.Loc.u.host.achName, sizeof (info.Loc.u.host.achName), "VBoxSharedCrOpenGL");
    rc = vbglDriverIOCtl (&hCtl->driver, VBOXGUEST_IOCTL_HGCM_CONNECT, &info, sizeof (info));
    if (RT_SUCCESS(rc))
    {
        rc = info.result;
        if (RT_SUCCESS(rc))
        {
            Assert(info.u32ClientID);
            *pu32ClientID = info.u32ClientID;
            return rc;
        }
    }

    Assert(RT_FAILURE(rc));
    *pu32ClientID = 0;
    return rc;
}

DECLVBGL(int) vboxCrCtlConDisconnect(HVBOXCRCTL hCtl, uint32_t u32ClientID)
{
    VBoxGuestHGCMDisconnectInfo info;
    memset (&info, 0, sizeof (info));
    info.u32ClientID = u32ClientID;
    return vbglDriverIOCtl (&hCtl->driver, VBOXGUEST_IOCTL_HGCM_DISCONNECT, &info, sizeof (info));
}

DECLVBGL(int) vboxCrCtlConCall(HVBOXCRCTL hCtl, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo)
{
    return vbglDriverIOCtl (&hCtl->driver, VBOXGUEST_IOCTL_HGCM_CALL(cbCallInfo), pCallInfo, cbCallInfo);
}

DECLVBGL(int) vboxCrCtlConCallUserData(HVBOXCRCTL hCtl, struct VBoxGuestHGCMCallInfo *pCallInfo, int cbCallInfo)
{
    return vbglDriverIOCtl (&hCtl->driver, VBOXGUEST_IOCTL_HGCM_CALL_USERDATA(cbCallInfo), pCallInfo, cbCallInfo);
}

#endif /* #ifndef VBGL_VBOXGUEST */
