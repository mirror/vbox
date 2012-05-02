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

#include <cr_protocol.h>

#if 0
#include <cr_pack.h>

typedef struct PVBOXMP_SHGSMIPACKER
{
    PVBOXMP_DEVEXT pDevExt;
    CRPackContext CrPacker;
    CRPackBuffer CrBuffer;
} PVBOXMP_SHGSMIPACKER, *PPVBOXMP_SHGSMIPACKER;

static void* vboxMpCrShgsmiBufferAlloc(PVBOXMP_DEVEXT pDevExt, HGSMISIZE cbData)
{
    return VBoxSHGSMIHeapBufferAlloc(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, cbData);
}

static void vboxMpCrShgsmiBufferFree(PVBOXMP_DEVEXT pDevExt, void *pvBuffer)
{
    VBoxSHGSMIHeapBufferFree(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, pvBuffer);
}

static void vboxMpCrShgsmiPackerCbFlush(void *pvFlush)
{
    PPVBOXMP_SHGSMIPACKER pPacker = (PPVBOXMP_SHGSMIPACKER)pvFlush;

    crPackReleaseBuffer(&pPacker->CrPacker);

    if (pPacker->CrBuffer.opcode_current != pPacker->CrBuffer.opcode_start)
    {
        CRMessageOpcodes *pHdr;
        unsigned int len;
        pHdr = vboxMpCrPackerPrependHeader(&pPacker->CrBuffer, &len, 0);

        /*Send*/
    }


    crPackSetBuffer(&pPacker->CrPacker, &pPacker->CrBuffer);
    crPackResetPointers(&pPacker->CrPacker);
}

static int vboxMpCrShgsmiPackerInit(PPVBOXMP_SHGSMIPACKER pPacker, PVBOXMP_DEVEXT pDevExt)
{
    memset(pPacker, 0, sizeof (*pPacker));

    static const cbBuffer = 1000;
    void *pvBuffer = vboxMpCrShgsmiBufferAlloc(pDevExt, cbBuffer);
    if (!pvBuffer)
    {
        WARN(("vboxMpCrShgsmiBufferAlloc failed"));
        return VERR_NO_MEMORY;
    }
    crPackInitBuffer(&pPacker->CrBuffer, pvBuffer, cbBuffer, cbBuffer);
    crPackSetBuffer(&pPacker->CrPacker, &pPacker->CrBuffer);
    crPackFlushFunc(&pPacker->CrPacker, vboxMpCrShgsmiPackerCbFlush);
    crPackFlushArg(&pPacker->CrPacker, pPacker);
//    crPackSendHugeFunc( thread->packer, packspuHuge );
    return VINF_SUCCESS;
}
#endif

static int vboxMpCrCtlAddRef(PVBOXMP_CRCTLCON pCrCtlCon)
{
    if (pCrCtlCon->cCrCtlRefs++)
        return VINF_ALREADY_INITIALIZED;

    int rc = vboxCrCtlCreate(&pCrCtlCon->hCrCtl);
    if (RT_SUCCESS(rc))
    {
        Assert(pCrCtlCon->hCrCtl);
        return VINF_SUCCESS;
    }

    WARN(("vboxCrCtlCreate failed, rc (%d)", rc));

    --pCrCtlCon->cCrCtlRefs;
    return rc;
}

static int vboxMpCrCtlRelease(PVBOXMP_CRCTLCON pCrCtlCon)
{
    Assert(pCrCtlCon->cCrCtlRefs);
    if (--pCrCtlCon->cCrCtlRefs)
    {
        return VINF_SUCCESS;
    }

    int rc = vboxCrCtlDestroy(pCrCtlCon->hCrCtl);
    if (RT_SUCCESS(rc))
    {
        pCrCtlCon->hCrCtl = NULL;
        return VINF_SUCCESS;
    }

    WARN(("vboxCrCtlDestroy failed, rc (%d)", rc));

    ++pCrCtlCon->cCrCtlRefs;
    return rc;
}

static int vboxMpCrCtlConSetVersion(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID, uint32_t vMajor, uint32_t vMinor)
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

    rc = vboxCrCtlConCall(pCrCtlCon->hCrCtl, &parms.hdr, sizeof (parms));
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

static int vboxMpCrCtlConSetPID(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID)
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

    rc = vboxCrCtlConCall(pCrCtlCon->hCrCtl, &parms.hdr, sizeof (parms));
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

int VBoxMpCrCtlConConnect(PVBOXMP_CRCTLCON pCrCtlCon,
        uint32_t crVersionMajor, uint32_t crVersionMinor,
        uint32_t *pu32ClientID)
{
    uint32_t u32ClientID;
    int rc = vboxMpCrCtlAddRef(pCrCtlCon);
    if (RT_SUCCESS(rc))
    {
        rc = vboxCrCtlConConnect(pCrCtlCon->hCrCtl, &u32ClientID);
        if (RT_SUCCESS(rc))
        {
            rc = vboxMpCrCtlConSetVersion(pCrCtlCon, u32ClientID, crVersionMajor, crVersionMinor);
            if (RT_SUCCESS(rc))
            {
                rc = vboxMpCrCtlConSetPID(pCrCtlCon, u32ClientID);
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
            vboxCrCtlConDisconnect(pCrCtlCon->hCrCtl, u32ClientID);
        }
        else
        {
            WARN(("vboxCrCtlConConnect failed, rc (%d)", rc));
        }
        vboxMpCrCtlRelease(pCrCtlCon);
    }
    else
    {
        WARN(("vboxMpCrCtlAddRef failed, rc (%d)", rc));
    }

    *pu32ClientID = 0;
    Assert(RT_FAILURE(rc));
    return rc;
}

int VBoxMpCrCtlConDisconnect(PVBOXMP_CRCTLCON pCrCtlCon, uint32_t u32ClientID)
{
    int rc = vboxCrCtlConDisconnect(pCrCtlCon->hCrCtl, u32ClientID);
    if (RT_SUCCESS(rc))
    {
        vboxMpCrCtlRelease(pCrCtlCon);
        return VINF_SUCCESS;
    }
    else
    {
        WARN(("vboxCrCtlConDisconnect failed, rc (%d)", rc));
    }
    return rc;
}

int VBoxMpCrCtlConCall(PVBOXMP_CRCTLCON pCrCtlCon, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = vboxCrCtlConCall(pCrCtlCon->hCrCtl, pData, cbData);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vboxCrCtlConCallUserData failed, rc(%d)", rc));
    return rc;
}

int VBoxMpCrCtlConCallUserData(PVBOXMP_CRCTLCON pCrCtlCon, VBoxGuestHGCMCallInfo *pData, uint32_t cbData)
{
    int rc = vboxCrCtlConCallUserData(pCrCtlCon->hCrCtl, pData, cbData);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    WARN(("vboxCrCtlConCallUserData failed, rc(%d)", rc));
    return rc;
}

bool VBoxMpCrCtlConIs3DSupported()
{
    VBOXMP_CRCTLCON CrCtlCon = {0};
    uint32_t u32ClientID = 0;
    int rc = VBoxMpCrCtlConConnect(&CrCtlCon, CR_PROTOCOL_VERSION_MAJOR, CR_PROTOCOL_VERSION_MINOR, &u32ClientID);
    if (RT_FAILURE(rc))
    {
        LOGREL(("VBoxMpCrCtlConConnect failed with rc(%d), 3D not supported!"));
        return false;
    }

    rc = VBoxMpCrCtlConDisconnect(&CrCtlCon, u32ClientID);
    if (RT_FAILURE(rc))
        WARN(("VBoxMpCrCtlConDisconnect failed, ignoring.."));

    return true;
}
