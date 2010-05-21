/*
 * Copyright (C) 2010 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */
#include "../VBoxVideo.h"
#include "../Helper.h"

#ifndef VBOXVHWA_WITH_SHGSMI
# include <iprt/semaphore.h>
# include <iprt/asm.h>
#endif



DECLINLINE(void) vboxVhwaHdrInit(VBOXVHWACMD* pHdr, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, VBOXVHWACMD_TYPE enmCmd)
{
    memset(pHdr, 0, sizeof(VBOXVHWACMD));
    pHdr->iDisplay = srcId;
    pHdr->rc = VERR_GENERAL_FAILURE;
    pHdr->enmCmd = enmCmd;
#ifndef VBOXVHWA_WITH_SHGSMI
    pHdr->cRefs = 1;
#endif
}

#ifdef VBOXVHWA_WITH_SHGSMI
static int vboxVhwaCommandSubmitHgsmi(struct _DEVICE_EXTENSION* pDevExt, HGSMIOFFSET offDr)
{
    VBoxHGSMIGuestWrite(pDevExt, offDr);
    return VINF_SUCCESS;
}
#else
DECLINLINE(void) vbvaVhwaCommandRelease(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
    uint32_t cRefs = ASMAtomicDecU32(&pCmd->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if(!cRefs)
    {
        vboxHGSMIBufferFree(pDevExt, pCmd);
    }
}

DECLINLINE(void) vbvaVhwaCommandRetain(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
    ASMAtomicIncU32(&pCmd->cRefs);
}

/* do not wait for completion */
void vboxVhwaCommandSubmitAsynch(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd, PFNVBOXVHWACMDCOMPLETION pfnCompletion, void * pContext)
{
    pCmd->GuestVBVAReserved1 = (uintptr_t)pfnCompletion;
    pCmd->GuestVBVAReserved2 = (uintptr_t)pContext;
    vbvaVhwaCommandRetain(pDevExt, pCmd);

    vboxHGSMIBufferSubmit(pDevExt, pCmd);

    if(!(pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH)
            || ((pCmd->Flags & VBOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION)
                    && (pCmd->Flags & VBOXVHWACMD_FLAG_HG_ASYNCH_RETURNED)))
    {
        /* the command is completed */
        pfnCompletion(pDevExt, pCmd, pContext);
    }

    vbvaVhwaCommandRelease(pDevExt, pCmd);
}

static DECLCALLBACK(void) vboxVhwaCompletionSetEvent(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD * pCmd, void * pvContext)
{
    RTSemEventSignal((RTSEMEVENT)pvContext);
}

void vboxVhwaCommandSubmitAsynchByEvent(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd, RTSEMEVENT hEvent)
{
    vboxVhwaCommandSubmitAsynch(pDevExt, pCmd, vboxVhwaCompletionSetEvent, hEvent);
}
#endif

VBOXVHWACMD* vboxVhwaCommandCreate(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, VBOXVHWACMD_TYPE enmCmd, VBOXVHWACMD_LENGTH cbCmd)
{
#ifdef VBOXVHWA_WITH_SHGSMI
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)VBoxSHGSMICommandAlloc(&pDevExt->u.primary.hgsmiAdapterHeap,
                              cbCmd + VBOXVHWACMD_HEADSIZE(),
                              HGSMI_CH_VBVA,
                              VBVA_VHWA_CMD);
#else
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)vboxHGSMIBufferAlloc(pDevExt,
                              cbCmd + VBOXVHWACMD_HEADSIZE(),
                              HGSMI_CH_VBVA,
                              VBVA_VHWA_CMD);
#endif
    Assert(pHdr);
    if (!pHdr)
    {
        drprintf((__FUNCTION__": vboxHGSMIBufferAlloc failed\n"));
    }
    else
    {
        vboxVhwaHdrInit(pHdr, srcId, enmCmd);
    }

    return pHdr;
}

void vboxVhwaCommandFree(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
#ifdef VBOXVHWA_WITH_SHGSMI
    VBoxSHGSMICommandFree(&pDevExt->u.primary.hgsmiAdapterHeap, pCmd);
#else
    vbvaVhwaCommandRelease(pDevExt, pCmd);
#endif
}

int vboxVhwaCommandSubmit(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
#ifdef VBOXVHWA_WITH_SHGSMI
    const VBOXSHGSMIHEADER* pHdr = VBoxSHGSMICommandPrepSynch(&pDevExt->u.primary.hgsmiAdapterHeap, pCmd);
    Assert(pHdr);
    int rc = VERR_GENERAL_FAILURE;
    if (pHdr)
    {
        do
        {
            HGSMIOFFSET offCmd = VBoxSHGSMICommandOffset(&pDevExt->u.primary.hgsmiAdapterHeap, pHdr);
            Assert(offCmd != HGSMIOFFSET_VOID);
            if (offCmd != HGSMIOFFSET_VOID)
            {
                rc = vboxVhwaCommandSubmitHgsmi(pDevExt, offCmd);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    VBoxSHGSMICommandDoneSynch(&pDevExt->u.primary.hgsmiAdapterHeap, pHdr);
                    AssertRC(rc);
                    break;
                }
            }
            else
                rc = VERR_INVALID_PARAMETER;
            /* fail to submit, cancel it */
            VBoxSHGSMICommandCancelSynch(&pDevExt->u.primary.hgsmiAdapterHeap, pHdr);
        } while (0);
    }
    else
        rc = VERR_INVALID_PARAMETER;
    return rc;
#else
    RTSEMEVENT hEvent;
    int rc = RTSemEventCreate(&hEvent);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        pCmd->Flags |= VBOXVHWACMD_FLAG_GH_ASYNCH_IRQ;
        vboxVhwaCommandSubmitAsynchByEvent(pDevExt, pCmd, hEvent);
        rc = RTSemEventWait(hEvent, RT_INDEFINITE_WAIT);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            RTSemEventDestroy(hEvent);
    }
    return rc;
#endif
}

#ifndef VBOXVHWA_WITH_SHGSMI
static DECLCALLBACK(void) vboxVhwaCompletionFreeCmd(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD * pCmd, void * pContext)
{
    vboxVhwaCommandFree(pDevExt, pCmd);
}

void vboxVhwaCompletionListProcess(PDEVICE_EXTENSION pDevExt, VBOXSHGSMILIST *pList)
{
    PVBOXSHGSMILIST_ENTRY pNext, pCur;
    for (pCur = pList->pFirst; pCur; pCur = pNext)
    {
        /* need to save next since the command may be released in a pfnCallback and thus its data might be invalid */
        pNext = pCur->pNext;
        VBOXVHWACMD *pCmd = VBOXVHWA_LISTENTRY2CMD(pCur);
        PFNVBOXVHWACMDCOMPLETION pfnCallback = (PFNVBOXVHWACMDCOMPLETION)pCmd->GuestVBVAReserved1;
        void *pvCallback = (void*)pCmd->GuestVBVAReserved2;
        pfnCallback(pDevExt, pCmd, pvCallback);
    }
}

#endif

void vboxVhwaCommandSubmitAsynchAndComplete(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
#ifdef VBOXVHWA_WITH_SHGSMI
# error "port me"
#else
    pCmd->Flags |= VBOXVHWACMD_FLAG_GH_ASYNCH_NOCOMPLETION;

    vboxVhwaCommandSubmitAsynch(pDevExt, pCmd, vboxVhwaCompletionFreeCmd, NULL);
#endif
}

void vboxVHWAFreeHostInfo1(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO1* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vboxVhwaCommandFree(pDevExt, pCmd);
}

void vboxVHWAFreeHostInfo2(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO2* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vboxVhwaCommandFree(pDevExt, pCmd);
}

VBOXVHWACMD_QUERYINFO1* vboxVHWAQueryHostInfo1(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    VBOXVHWACMD* pCmd = vboxVhwaCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_QUERY_INFO1, sizeof(VBOXVHWACMD_QUERYINFO1));
    VBOXVHWACMD_QUERYINFO1 *pInfo1;

    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWAQueryHostInfo1: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    pInfo1 = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
    pInfo1->u.in.guestVersion.maj = VBOXVHWA_VERSION_MAJ;
    pInfo1->u.in.guestVersion.min = VBOXVHWA_VERSION_MIN;
    pInfo1->u.in.guestVersion.bld = VBOXVHWA_VERSION_BLD;
    pInfo1->u.in.guestVersion.reserved = VBOXVHWA_VERSION_RSV;

    int rc = vboxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            return VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
        }
    }

    vboxVhwaCommandFree(pDevExt, pCmd);
    return NULL;
}

VBOXVHWACMD_QUERYINFO2* vboxVHWAQueryHostInfo2(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, uint32_t numFourCC)
{
    VBOXVHWACMD* pCmd = vboxVhwaCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_QUERY_INFO2, VBOXVHWAINFO2_SIZE(numFourCC));
    VBOXVHWACMD_QUERYINFO2 *pInfo2;
    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWAQueryHostInfo2: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    pInfo2 = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO2);
    pInfo2->numFourCC = numFourCC;

    int rc = vboxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        AssertRC(pCmd->rc);
        if(RT_SUCCESS(pCmd->rc))
        {
            if(pInfo2->numFourCC == numFourCC)
            {
                return pInfo2;
            }
        }
    }

    vboxVhwaCommandFree(pDevExt, pCmd);
    return NULL;
}

int vboxVHWAEnable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd;

    pCmd = vboxVhwaCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_ENABLE, 0);
    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWAEnable: vboxVHWACommandCreate failed\n"));
        return rc;
    }

    rc = vboxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        AssertRC(pCmd->rc);
        if(RT_SUCCESS(pCmd->rc))
            rc = VINF_SUCCESS;
        else
            rc = pCmd->rc;
    }

    vboxVhwaCommandFree(pDevExt, pCmd);
    return rc;
}

int vboxVHWADisable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd;

    pCmd = vboxVhwaCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_DISABLE, 0);
    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWADisable: vboxVHWACommandCreate failed\n"));
        return rc;
    }

    rc = vboxVhwaCommandSubmit(pDevExt, pCmd);
    AssertRC(rc);
    if(RT_SUCCESS(rc))
    {
        AssertRC(pCmd->rc);
        if(RT_SUCCESS(pCmd->rc))
            rc = VINF_SUCCESS;
        else
            rc = pCmd->rc;
    }

    vboxVhwaCommandFree(pDevExt, pCmd);
    return rc;
}

static void vboxVHWAInitSrc(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    Assert(srcId < pDevExt->cSources);
    VBOXVHWA_INFO *pSettings = &pDevExt->aSources[srcId].Vhwa.Settings;
    memset (pSettings, 0, sizeof (VBOXVHWA_INFO));

    VBOXVHWACMD_QUERYINFO1* pInfo1 = vboxVHWAQueryHostInfo1(pDevExt, srcId);
    if (pInfo1)
    {
        if (pInfo1->u.out.cfgFlags & VBOXVHWA_CFG_ENABLED)
        {
            if ((pInfo1->u.out.caps & VBOXVHWA_CAPS_OVERLAY)
                    && (pInfo1->u.out.caps & VBOXVHWA_CAPS_OVERLAYSTRETCH)
                    && (pInfo1->u.out.surfaceCaps & VBOXVHWA_SCAPS_OVERLAY)
                    && (pInfo1->u.out.surfaceCaps & VBOXVHWA_SCAPS_FLIP)
                    && (pInfo1->u.out.surfaceCaps & VBOXVHWA_SCAPS_LOCALVIDMEM)
                    && pInfo1->u.out.numOverlays)
            {
                pSettings->fFlags |= VBOXVHWA_F_ENABLED;

                if (pInfo1->u.out.caps & VBOXVHWA_CAPS_COLORKEY)
                {
                    if (pInfo1->u.out.colorKeyCaps & VBOXVHWA_CKEYCAPS_SRCOVERLAY)
                    {
                        pSettings->fFlags |= VBOXVHWA_F_CKEY_SRC;
                        /* todo: VBOXVHWA_CKEYCAPS_SRCOVERLAYONEACTIVE ? */
                    }

                    if (pInfo1->u.out.colorKeyCaps & VBOXVHWA_CKEYCAPS_DESTOVERLAY)
                    {
                        pSettings->fFlags |= VBOXVHWA_F_CKEY_DST;
                        /* todo: VBOXVHWA_CKEYCAPS_DESTOVERLAYONEACTIVE ? */
                    }
                }

                pSettings->cFormats = 0;

                pSettings->aFormats[pSettings->cFormats] = D3DDDIFMT_X8R8G8B8;
                ++pSettings->cFormats;

                if (pInfo1->u.out.numFourCC
                        && (pInfo1->u.out.caps & VBOXVHWA_CAPS_OVERLAYFOURCC))
                {
                    VBOXVHWACMD_QUERYINFO2* pInfo2 = vboxVHWAQueryHostInfo2(pDevExt, srcId, pInfo1->u.out.numFourCC);
                    if (pInfo2)
                    {
                        for (uint32_t i = 0; i < pInfo2->numFourCC; ++i)
                        {
                            pSettings->aFormats[pSettings->cFormats] = (D3DDDIFORMAT)pInfo2->FourCC[i];
                            ++pSettings->cFormats;
                        }
                        vboxVHWAFreeHostInfo2(pDevExt, pInfo2);
                    }
                }
            }
        }
        vboxVHWAFreeHostInfo1(pDevExt, pInfo1);
    }
}

void vboxVHWAInit(PDEVICE_EXTENSION pDevExt)
{
    for (uint32_t i = 0; i < pDevExt->cSources; ++i)
    {
        vboxVHWAInitSrc(pDevExt, i);
    }
}
