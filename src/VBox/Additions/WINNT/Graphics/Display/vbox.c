/* $Id$ */
/** @file
 * Display - VirtualBox Win 2000/XP guest display driver, support functions.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#include "driver.h"

#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>

void VBoxProcessDisplayInfo (PPDEV ppdev)
{
    if (ppdev->bHGSMISupported)
    {
        VBoxHGSMIProcessDisplayInfo(&ppdev->guestCtx, ppdev->iDevice,
                                     ppdev->ptlDevOrg.x, ppdev->ptlDevOrg.y, 0,
                                       ppdev->lDeltaScreen > 0
                                     ? ppdev->lDeltaScreen
                                     : -ppdev->lDeltaScreen, ppdev->cxScreen,
                                     ppdev->cyScreen,
                                     (uint16_t)ppdev->ulBitCount);
    }

    return;
}

#ifdef VBOX_WITH_VIDEOHWACCEL

VBOXVHWACMD* vboxVHWACommandCreate (PPDEV ppdev, VBOXVHWACMD_TYPE enmCmd, VBOXVHWACMD_LENGTH cbCmd)
{
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)VBoxHGSMIBufferAlloc(&ppdev->guestCtx,
                              cbCmd + VBOXVHWACMD_HEADSIZE(),
                              HGSMI_CH_VBVA,
                              VBVA_VHWA_CMD);
    if (!pHdr)
    {
        LogFunc(("HGSMIHeapAlloc failed\n"));
    }
    else
    {
        memset(pHdr, 0, sizeof(VBOXVHWACMD));
        pHdr->iDisplay = ppdev->iDevice;
        pHdr->rc = VERR_GENERAL_FAILURE;
        pHdr->enmCmd = enmCmd;
        pHdr->cRefs = 1;
    }

    /* temporary hack */
    vboxVHWACommandCheckHostCmds(ppdev);

    return pHdr;
}

void vboxVHWACommandFree (PPDEV ppdev, VBOXVHWACMD* pCmd)
{
    VBoxHGSMIBufferFree(&ppdev->guestCtx, pCmd);
}

static DECLCALLBACK(void) vboxVHWACommandCompletionCallbackEvent(PPDEV ppdev, VBOXVHWACMD * pCmd, void * pContext)
{
    VBOXPEVENT pEvent = (VBOXPEVENT)pContext;
    LONG oldState = ppdev->VideoPortProcs.pfnSetEvent(ppdev->pVideoPortContext, pEvent);
    Assert(!oldState);
}

static int vboxVHWAHanldeVHWACmdCompletion(PPDEV ppdev, VBVAHOSTCMD * pHostCmd)
{
    VBVAHOSTCMDVHWACMDCOMPLETE * pComplete = VBVAHOSTCMD_BODY(pHostCmd, VBVAHOSTCMDVHWACMDCOMPLETE);
    VBOXVHWACMD* pComplCmd = (VBOXVHWACMD*)HGSMIOffsetToPointer (&ppdev->guestCtx.heapCtx.area, pComplete->offCmd);
    PFNVBOXVHWACMDCOMPLETION pfnCompletion = (PFNVBOXVHWACMDCOMPLETION)pComplCmd->GuestVBVAReserved1;
    void * pContext = (void *)pComplCmd->GuestVBVAReserved2;

    pfnCompletion(ppdev, pComplCmd, pContext);

    vboxVBVAHostCommandComplete(ppdev, pHostCmd);

    return 0;
}

static void vboxVBVAHostCommandHanlder(PPDEV ppdev, VBVAHOSTCMD * pCmd)
{
    int rc = VINF_SUCCESS;
    switch(pCmd->customOpCode)
    {
# ifdef VBOX_WITH_VIDEOHWACCEL /** @todo why is this ifdef nested within itself? */
        case VBVAHG_DCUSTOM_VHWA_CMDCOMPLETE:
        {
            vboxVHWAHanldeVHWACmdCompletion(ppdev, pCmd);
            break;
        }
# endif
        default:
        {
            Assert(0);
            vboxVBVAHostCommandComplete(ppdev, pCmd);
        }
    }
}

void vboxVHWACommandCheckHostCmds(PPDEV ppdev)
{
    VBVAHOSTCMD * pCmd, * pNextCmd;
    int rc = ppdev->pfnHGSMIRequestCommands(ppdev->hMpHGSMI, HGSMI_CH_VBVA, ppdev->iDevice, &pCmd);
    /* don't assert here, otherwise NT4 will be unhappy */
    if(RT_SUCCESS(rc))
    {
        for(;pCmd; pCmd = pNextCmd)
        {
            pNextCmd = pCmd->u.pNext;
            vboxVBVAHostCommandHanlder(ppdev, pCmd);
        }
    }
}

void vboxVHWACommandSubmitAsynchByEvent (PPDEV ppdev, VBOXVHWACMD* pCmd, VBOXPEVENT pEvent)
{
//    Assert(0);
    pCmd->GuestVBVAReserved1 = (uintptr_t)pEvent;
    pCmd->GuestVBVAReserved2 = 0;
    /* ensure the command is not removed until we're processing it */
    vbvaVHWACommandRetain(ppdev, pCmd);

    /* complete it asynchronously by setting event */
    pCmd->Flags |= VBOXVHWACMD_FLAG_GH_ASYNCH_EVENT;
    VBoxHGSMIBufferSubmit(&ppdev->guestCtx, pCmd);

    if(!(ASMAtomicReadU32((volatile uint32_t *)&pCmd->Flags)  & VBOXVHWACMD_FLAG_HG_ASYNCH))
    {
        /* the command is completed */
        ppdev->VideoPortProcs.pfnSetEvent(ppdev->pVideoPortContext, pEvent);
    }

    vbvaVHWACommandRelease(ppdev, pCmd);
}

BOOL vboxVHWACommandSubmit (PPDEV ppdev, VBOXVHWACMD* pCmd)
{
    VBOXPEVENT pEvent;
    VBOXVP_STATUS rc = ppdev->VideoPortProcs.pfnCreateEvent(ppdev->pVideoPortContext, VBOXNOTIFICATION_EVENT, NULL, &pEvent);
    /* don't assert here, otherwise NT4 will be unhappy */
    if(rc == VBOXNO_ERROR)
    {
        pCmd->Flags |= VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ;
        vboxVHWACommandSubmitAsynchByEvent (ppdev, pCmd, pEvent);

        rc = ppdev->VideoPortProcs.pfnWaitForSingleObject(ppdev->pVideoPortContext, pEvent,
                NULL /*IN PLARGE_INTEGER  pTimeOut*/
                );
        Assert(rc == VBOXNO_ERROR);
        if(rc == VBOXNO_ERROR)
        {
            ppdev->VideoPortProcs.pfnDeleteEvent(ppdev->pVideoPortContext, pEvent);
        }
    }
    return rc == VBOXNO_ERROR;
}

/* do not wait for completion */
void vboxVHWACommandSubmitAsynch (PPDEV ppdev, VBOXVHWACMD* pCmd, PFNVBOXVHWACMDCOMPLETION pfnCompletion, void * pContext)
{
//    Assert(0);
    pCmd->GuestVBVAReserved1 = (uintptr_t)pfnCompletion;
    pCmd->GuestVBVAReserved2 = (uintptr_t)pContext;
    vbvaVHWACommandRetain(ppdev, pCmd);

    VBoxHGSMIBufferSubmit(&ppdev->guestCtx, pCmd);

    if(!(pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH))
    {
        /* the command is completed */
        pfnCompletion(ppdev, pCmd, pContext);
    }

    vbvaVHWACommandRelease(ppdev, pCmd);
}

static DECLCALLBACK(void) vboxVHWAFreeCmdCompletion(PPDEV ppdev, VBOXVHWACMD * pCmd, void * pContext)
{
    vbvaVHWACommandRelease(ppdev, pCmd);
}

void vboxVHWACommandSubmitAsynchAndComplete (PPDEV ppdev, VBOXVHWACMD* pCmd)
{
//    Assert(0);
    pCmd->GuestVBVAReserved1 = (uintptr_t)vboxVHWAFreeCmdCompletion;

    vbvaVHWACommandRetain(ppdev, pCmd);

    pCmd->Flags |= VBOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION;

    VBoxHGSMIBufferSubmit(&ppdev->guestCtx, pCmd);

    if(!(pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH)
            || pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH_RETURNED)
    {
        /* the command is completed */
        vboxVHWAFreeCmdCompletion(ppdev, pCmd, NULL);
    }

    vbvaVHWACommandRelease(ppdev, pCmd);
}

void vboxVHWAFreeHostInfo1(PPDEV ppdev, VBOXVHWACMD_QUERYINFO1* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vbvaVHWACommandRelease (ppdev, pCmd);
}

void vboxVHWAFreeHostInfo2(PPDEV ppdev, VBOXVHWACMD_QUERYINFO2* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vbvaVHWACommandRelease (ppdev, pCmd);
}

VBOXVHWACMD_QUERYINFO1* vboxVHWAQueryHostInfo1(PPDEV ppdev)
{
    VBOXVHWACMD* pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_QUERY_INFO1, sizeof(VBOXVHWACMD_QUERYINFO1));
    VBOXVHWACMD_QUERYINFO1 *pInfo1;
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    pInfo1 = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
    pInfo1->u.in.guestVersion.maj = VBOXVHWA_VERSION_MAJ;
    pInfo1->u.in.guestVersion.min = VBOXVHWA_VERSION_MIN;
    pInfo1->u.in.guestVersion.bld = VBOXVHWA_VERSION_BLD;
    pInfo1->u.in.guestVersion.reserved = VBOXVHWA_VERSION_RSV;

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            return VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
        }
    }

    vbvaVHWACommandRelease (ppdev, pCmd);
    return NULL;
}

VBOXVHWACMD_QUERYINFO2* vboxVHWAQueryHostInfo2(PPDEV ppdev, uint32_t numFourCC)
{
    VBOXVHWACMD* pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_QUERY_INFO2, VBOXVHWAINFO2_SIZE(numFourCC));
    VBOXVHWACMD_QUERYINFO2 *pInfo2;
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAQueryHostInfo2: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    pInfo2 = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO2);
    pInfo2->numFourCC = numFourCC;

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            if(pInfo2->numFourCC == numFourCC)
            {
                return pInfo2;
            }
        }
    }

    vbvaVHWACommandRelease (ppdev, pCmd);
    return NULL;
}

int vboxVHWAInitHostInfo1(PPDEV ppdev)
{
    VBOXVHWACMD_QUERYINFO1* pInfo;

    if (!ppdev->bHGSMISupported)
        return VERR_NOT_SUPPORTED;

    pInfo = vboxVHWAQueryHostInfo1(ppdev);
    if(!pInfo)
    {
        ppdev->vhwaInfo.bVHWAEnabled = false;
        return VERR_OUT_OF_RESOURCES;
    }

    ppdev->vhwaInfo.caps = pInfo->u.out.caps;
    ppdev->vhwaInfo.caps2 = pInfo->u.out.caps2;
    ppdev->vhwaInfo.colorKeyCaps = pInfo->u.out.colorKeyCaps;
    ppdev->vhwaInfo.stretchCaps = pInfo->u.out.stretchCaps;
    ppdev->vhwaInfo.surfaceCaps = pInfo->u.out.surfaceCaps;
    ppdev->vhwaInfo.numOverlays = pInfo->u.out.numOverlays;
    ppdev->vhwaInfo.numFourCC = pInfo->u.out.numFourCC;
    ppdev->vhwaInfo.bVHWAEnabled = (pInfo->u.out.cfgFlags & VBOXVHWA_CFG_ENABLED);
    vboxVHWAFreeHostInfo1(ppdev, pInfo);
    return VINF_SUCCESS;
}

int vboxVHWAInitHostInfo2(PPDEV ppdev, DWORD *pFourCC)
{
    VBOXVHWACMD_QUERYINFO2* pInfo;
    int rc = VINF_SUCCESS;

    if (!ppdev->bHGSMISupported)
        return VERR_NOT_SUPPORTED;

    pInfo = vboxVHWAQueryHostInfo2(ppdev, ppdev->vhwaInfo.numFourCC);

    Assert(pInfo);
    if(!pInfo)
        return VERR_OUT_OF_RESOURCES;

    if(ppdev->vhwaInfo.numFourCC)
    {
        memcpy(pFourCC, pInfo->FourCC, ppdev->vhwaInfo.numFourCC * sizeof(pFourCC[0]));
    }
    else
    {
        Assert(0);
        rc = VERR_GENERAL_FAILURE;
    }

    vboxVHWAFreeHostInfo2(ppdev, pInfo);

    return rc;
}

int vboxVHWAEnable(PPDEV ppdev)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd;

    if (!ppdev->bHGSMISupported)
        return VERR_NOT_SUPPORTED;

    pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_ENABLE, 0);
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWAEnable: vboxVHWACommandCreate failed\n"));
        return rc;
    }

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            rc = VINF_SUCCESS;
        }
    }

    vbvaVHWACommandRelease (ppdev, pCmd);
    return rc;
}

int vboxVHWADisable(PPDEV ppdev)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd;

    if (!ppdev->bHGSMISupported)
        return VERR_NOT_SUPPORTED;

    pCmd = vboxVHWACommandCreate (ppdev, VBOXVHWACMD_TYPE_DISABLE, 0);
    if (!pCmd)
    {
        DISPDBG((0, "VBoxDISP::vboxVHWADisable: vboxVHWACommandCreate failed\n"));
        return rc;
    }

    if(vboxVHWACommandSubmit (ppdev, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            rc = VINF_SUCCESS;
        }
    }

    vbvaVHWACommandRelease (ppdev, pCmd);

    vboxVHWACommandCheckHostCmds(ppdev);

    return rc;
}

void vboxVHWAInit(PPDEV ppdev)
{
    VHWAQUERYINFO info;
    DWORD returnedDataLength;
    DWORD err;

    memset(&info, 0, sizeof (info));

    err = EngDeviceIoControl(ppdev->hDriver,
            IOCTL_VIDEO_VHWA_QUERY_INFO,
            NULL,
            0,
            &info,
            sizeof(info),
            &returnedDataLength);
    Assert(!err);
    if(!err)
    {
        ppdev->vhwaInfo.offVramBase = info.offVramBase;
        ppdev->vhwaInfo.bVHWAInited = TRUE;
    }
    else
        ppdev->vhwaInfo.bVHWAInited = FALSE;
}

#endif

void vboxVBVAHostCommandComplete(PPDEV ppdev, VBVAHOSTCMD * pCmd)
{
    ppdev->pfnHGSMICommandComplete(ppdev->hMpHGSMI, pCmd);
}

