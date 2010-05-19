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

VBOXVHWACMD* vboxVhwaCtlCommandCreate(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, VBOXVHWACMD_TYPE enmCmd, VBOXVHWACMD_LENGTH cbCmd)
{
    VBOXVHWACMD* pHdr = (VBOXVHWACMD*)VBoxSHGSMICommandAlloc(&pDevExt->u.primary.hgsmiAdapterHeap,
                              cbCmd + VBOXVHWACMD_HEADSIZE(),
                              HGSMI_CH_VBVA,
                              VBVA_VHWA_CMD);
    Assert(pHdr);
    if (!pHdr)
    {
        drprintf((__FUNCTION__": vboxHGSMIBufferAlloc failed\n"));
    }
    else
    {
        vboxVhwaInitHdr(pHdr, srcId, enmCmd);
    }

    return pHdr;
}

void vboxVhwaCtlCommandFree(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
    VBoxSHGSMICommandFree(&pDevExt->u.primary.hgsmiAdapterHeap, pCmd);
}

static int vboxVhwaCtlCommandSubmitHgsmi(struct _DEVICE_EXTENSION* pDevExt, HGSMIOFFSET offDr)
{
    VBoxHGSMIGuestWrite(pDevExt, offDr);
    return VINF_SUCCESS;
}

int vboxVhwaCtlCommandSubmit(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD* pCmd)
{
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
                rc = vboxVhwaCtlCommandSubmitHgsmi(pDevExt, offCmd);
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
}

void vboxVHWAFreeHostInfo1(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO1* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vboxVhwaCtlCommandFree(pDevExt, pCmd);
}

void vboxVHWAFreeHostInfo2(PDEVICE_EXTENSION pDevExt, VBOXVHWACMD_QUERYINFO2* pInfo)
{
    VBOXVHWACMD* pCmd = VBOXVHWACMD_HEAD(pInfo);
    vboxVhwaCtlCommandFree(pDevExt, pCmd);
}

VBOXVHWACMD_QUERYINFO1* vboxVHWAQueryHostInfo1(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    VBOXVHWACMD* pCmd = vboxVhwaCtlCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_QUERY_INFO1, sizeof(VBOXVHWACMD_QUERYINFO1));
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

    if(vboxVhwaCtlCommandSubmit(pDevExt, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            return VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO1);
        }
    }

    vboxVhwaCtlCommandFree(pDevExt, pCmd);
    return NULL;
}

VBOXVHWACMD_QUERYINFO2* vboxVHWAQueryHostInfo2(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, uint32_t numFourCC)
{
    VBOXVHWACMD* pCmd = vboxVhwaCtlCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_QUERY_INFO2, VBOXVHWAINFO2_SIZE(numFourCC));
    VBOXVHWACMD_QUERYINFO2 *pInfo2;
    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWAQueryHostInfo2: vboxVHWACommandCreate failed\n"));
        return NULL;
    }

    pInfo2 = VBOXVHWACMD_BODY(pCmd, VBOXVHWACMD_QUERYINFO2);
    pInfo2->numFourCC = numFourCC;

    if(vboxVhwaCtlCommandSubmit(pDevExt, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            if(pInfo2->numFourCC == numFourCC)
            {
                return pInfo2;
            }
        }
    }

    vboxVhwaCtlCommandFree(pDevExt, pCmd);
    return NULL;
}

int vboxVHWAEnable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd;

    pCmd = vboxVhwaCtlCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_ENABLE, 0);
    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWAEnable: vboxVHWACommandCreate failed\n"));
        return rc;
    }

    if(vboxVhwaCtlCommandSubmit(pDevExt, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            rc = VINF_SUCCESS;
        }
    }

    vboxVhwaCtlCommandFree(pDevExt, pCmd);
    return rc;
}

int vboxVHWADisable(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId)
{
    int rc = VERR_GENERAL_FAILURE;
    VBOXVHWACMD* pCmd;

    pCmd = vboxVhwaCtlCommandCreate(pDevExt, srcId, VBOXVHWACMD_TYPE_DISABLE, 0);
    Assert(pCmd);
    if (!pCmd)
    {
        drprintf((0, "VBoxDISP::vboxVHWADisable: vboxVHWACommandCreate failed\n"));
        return rc;
    }

    if(vboxVhwaCtlCommandSubmit(pDevExt, pCmd))
    {
        if(RT_SUCCESS(pCmd->rc))
        {
            rc = VINF_SUCCESS;
        }
    }

    vboxVhwaCtlCommandFree(pDevExt, pCmd);
    return rc;
}

void vboxVHWAInit(PDEVICE_EXTENSION pDevExt)
{
    VBOXVHWA_INFO *pSettings = &pDevExt->u.primary.Vhwa;
    memset (pSettings, 0, sizeof (VBOXVHWA_INFO));

    VBOXVHWACMD_QUERYINFO1* pInfo1 = vboxVHWAQueryHostInfo1(pDevExt,
            0 /*D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId*/);
    if (pInfo1)
    {
        if (pInfo1->u.out.cfgFlags & VBOXVHWA_CFG_ENABLED)
        {
            if (pInfo1->u.out.numOverlays)
            {
                pSettings->bEnabled = true;
                pSettings->cFormats = 0;

                pSettings->aFormats[pSettings->cFormats] = D3DDDIFMT_X8R8G8B8;
                ++pSettings->cFormats;
                pSettings->aFormats[pSettings->cFormats] = D3DDDIFMT_A8R8G8B8;
                ++pSettings->cFormats;

                if (pInfo1->u.out.numFourCC)
                {
                    VBOXVHWACMD_QUERYINFO2* pInfo2 = vboxVHWAQueryHostInfo2(pDevExt,
                            0 /*D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId*/,
                            pInfo1->u.out.numFourCC);
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
