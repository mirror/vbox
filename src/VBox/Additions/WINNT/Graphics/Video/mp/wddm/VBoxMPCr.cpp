/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
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
 */

#include "VBoxMPWddm.h"
#include "VBoxMPCr.h"

#include <VBox/HostServices/VBoxCrOpenGLSvc.h>

static int vboxMpCrCtlAddRef(struct _VBOXMP_DEVEXT *pDevExt)
{
    if (pDevExt->cCrCtlRefs++)
        return VINF_ALREADY_INITIALIZED;

    int rc = vboxCrCtlCreate(&pDevExt->hCrCtl);
    if (RT_SUCCESS(rc))
    {
        Assert(pDevExt->hCrCtl);
        return VINF_SUCCESS;
    }

    WARN(("vboxCrCtlCreate failed, rc (%d)", rc));

    --pDevExt->cCrCtlRefs;
    return rc;
}

static int vboxMpCrCtlRelease(struct _VBOXMP_DEVEXT *pDevExt)
{
    Assert(pDevExt->cCrCtlRefs);
    if (--pDevExt->cCrCtlRefs)
    {
        return VINF_SUCCESS;
    }

    int rc = vboxCrCtlDestroy(pDevExt->hCrCtl);
    if (RT_SUCCESS(rc))
    {
        pDevExt->hCrCtl = NULL;
        return VINF_SUCCESS;
    }

    WARN(("vboxCrCtlDestroy failed, rc (%d)", rc));

    ++pDevExt->cCrCtlRefs;
    return rc;
}

static int vboxMpCrCtlConSetVersion(struct _VBOXMP_DEVEXT *pDevExt, uint32_t u32ClientID, uint32_t vMajor, uint32_t vMinor)
{
    CRVBOXHGCMSETVERSION parms;
    int rc;

    parms.hdr.result      = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_SET_VERSION;
    parms.hdr.cParms      = SHCRGL_CPARMS_SET_VERSION;

    parms.vMajor.type      = VMMDevHGCMParmType_32bit;
    parms.vMajor.u.value32 = vMajor;
    parms.vMinor.type      = VMMDevHGCMParmType_32bit;
    parms.vMinor.u.value32 = vMinor;

    rc = vboxCrCtlConCall(pDevExt->hCrCtl, &parms.hdr, sizeof (parms));
    if (RT_FAILURE(rc))
    {
        WARN(("vboxCrCtlConCall failed, rc (%d)", rc));
        return rc;
    }

    if (RT_FAILURE(parms.hdr.result))
    {
        WARN(("version validation failed, rc (%d)", parms.hdr.result));
        return parms.hdr.result;
    }
    return VINF_SUCCESS;
}

static int vboxMpCrCtlConSetPID(struct _VBOXMP_DEVEXT *pDevExt, uint32_t u32ClientID)
{
    CRVBOXHGCMSETPID parms;
    int rc;

    parms.hdr.result      = VERR_WRONG_ORDER;
    parms.hdr.u32ClientID = u32ClientID;
    parms.hdr.u32Function = SHCRGL_GUEST_FN_SET_PID;
    parms.hdr.cParms      = SHCRGL_CPARMS_SET_PID;

    parms.u64PID.type     = VMMDevHGCMParmType_64bit;
    parms.u64PID.u.value64 = (uint64_t)PsGetCurrentProcessId();

    Assert(parms.u64PID.u.value64);

    rc = vboxCrCtlConCall(pDevExt->hCrCtl, &parms.hdr, sizeof (parms));
    if (RT_FAILURE(rc))
    {
        WARN(("vboxCrCtlConCall failed, rc (%d)", rc));
        return rc;
    }

    if (RT_FAILURE(parms.hdr.result))
    {
        WARN(("set PID failed, rc (%d)", parms.hdr.result));
        return parms.hdr.result;
    }
    return VINF_SUCCESS;
}

int VBoxMpCrCtlConConnect(struct _VBOXMP_DEVEXT *pDevExt,
        uint32_t crVersionMajor, uint32_t crVersionMinor,
        uint32_t *pu32ClientID)
{
    uint32_t u32ClientID;
    int rc = vboxMpCrCtlAddRef(pDevExt);
    if (RT_SUCCESS(rc))
    {
        rc = vboxCrCtlConConnect(pDevExt->hCrCtl, &u32ClientID);
        if (RT_SUCCESS(rc))
        {
            rc = vboxMpCrCtlConSetVersion(pDevExt, u32ClientID, crVersionMajor, crVersionMinor);
            if (RT_SUCCESS(rc))
            {
                rc = vboxMpCrCtlConSetPID(pDevExt, u32ClientID);
                if (RT_SUCCESS(rc))
                {
                    *pu32ClientID = u32ClientID;
                    return VINF_SUCCESS;
                }
                else
                {
                    WARN(("vboxMpCrCtlConSetPID failed, rc (%d)", rc));
                }
            }
            else
            {
                WARN(("vboxMpCrCtlConSetVersion failed, rc (%d)", rc));
            }
            vboxCrCtlConDisconnect(pDevExt->hCrCtl, u32ClientID);
        }
        else
        {
            WARN(("vboxCrCtlConConnect failed, rc (%d)", rc));
        }
        vboxMpCrCtlRelease(pDevExt);
    }
    else
    {
        WARN(("vboxMpCrCtlAddRef failed, rc (%d)", rc));
    }

    *pu32ClientID = 0;
    Assert(RT_FAILURE(rc));
    return rc;
}

int VBoxMpCrCtlConDisconnect(struct _VBOXMP_DEVEXT *pDevExt, uint32_t u32ClientID)
{
    int rc = vboxCrCtlConDisconnect(pDevExt->hCrCtl, u32ClientID);
    if (RT_SUCCESS(rc))
    {
        vboxMpCrCtlRelease(pDevExt);
        return VINF_SUCCESS;
    }
    else
    {
        WARN(("vboxCrCtlConDisconnect failed, rc (%d)", rc));
    }
    return rc;
}

int VBoxMpCrCtlConCall(struct _VBOXMP_DEVEXT *pDevExt, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = vboxCrCtlConCall(pDevExt->hCrCtl, pData, cbData);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vboxCrCtlConCallUserData failed, rc(%d)", rc));
    return rc;
}

int VBoxMpCrCtlConCallUserData(struct _VBOXMP_DEVEXT *pDevExt, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = vboxCrCtlConCallUserData(pDevExt->hCrCtl, pData, cbData);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vboxCrCtlConCallUserData failed, rc(%d)", rc));
    return rc;
}
