/* $Id$ */

/** @file
 * VBox WDDM Miniport driver
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include "VBoxMPVidPn.h"
#include "common/VBoxMPCommon.h"

static D3DDDIFORMAT vboxWddmCalcPixelFormat(const VIDEO_MODE_INFORMATION *pInfo)
{
    switch (pInfo->BitsPerPlane)
    {
        case 32:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_A8R8G8B8;
                WARN(("unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)",
                      pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 24:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_R8G8B8;
                WARN(("unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)",
                     pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 16:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xF800 && pInfo->GreenMask == 0x7E0 && pInfo->BlueMask == 0x1F)
                    return D3DDDIFMT_R5G6B5;
                WARN(("unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)",
                      pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 8:
            if((pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && (pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                return D3DDDIFMT_P8;
            }
            else
            {
                WARN(("unsupported AttributeFlags(0x%x)", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        default:
            WARN(("unsupported bpp(%d)", pInfo->BitsPerPlane));
            AssertBreakpoint();
            break;
    }

    return D3DDDIFMT_UNKNOWN;
}

static int vboxWddmResolutionFind(const D3DKMDT_2DREGION *pResolutions, int cResolutions, const D3DKMDT_2DREGION *pRes)
{
    for (int i = 0; i < cResolutions; ++i)
    {
        const D3DKMDT_2DREGION *pResolution = &pResolutions[i];
        if (pResolution->cx == pRes->cx && pResolution->cy == pRes->cy)
            return i;
    }
    return -1;
}

static bool vboxWddmVideoModesMatch(const VIDEO_MODE_INFORMATION *pMode1, const VIDEO_MODE_INFORMATION *pMode2)
{
    return pMode1->VisScreenHeight == pMode2->VisScreenHeight
            && pMode1->VisScreenWidth == pMode2->VisScreenWidth
            && pMode1->BitsPerPlane == pMode2->BitsPerPlane;
}

static int vboxWddmVideoModeFind(const VIDEO_MODE_INFORMATION *pModes, int cModes, const VIDEO_MODE_INFORMATION *pM)
{
    for (int i = 0; i < cModes; ++i)
    {
        const VIDEO_MODE_INFORMATION *pMode = &pModes[i];
        if (vboxWddmVideoModesMatch(pMode, pM))
            return i;
    }
    return -1;
}

NTSTATUS vboxVidPnCheckSourceModeInfo(const D3DKMDT_HVIDPN hDesiredVidPn,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo,
        BOOLEAN *pbSupported)
{
    BOOLEAN bSupported = TRUE;
    /* we support both GRAPHICS and TEXT modes */
    switch (pNewVidPnSourceModeInfo->Type)
    {
        case D3DKMDT_RMT_GRAPHICS:
            /* any primary surface size actually
            pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx
            pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy
            */
            if (pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cx != pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx
                    || pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cy != pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy)
            {
                LOG(("VisibleRegionSize(%d, %d) !=  PrimSurfSize(%d, %d)",
                        pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cx,
                        pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cy,
                        pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx,
                        pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            /*
            pNewVidPnSourceModeInfo->Format.Graphics.Stride
            pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat
            pNewVidPnSourceModeInfo->Format.Graphics.ColorBasis
            pNewVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode
            */

            break;
        case D3DKMDT_RMT_TEXT:
            break;
        default:
            AssertBreakpoint();
            LOG(("Warning: Unknown Src mode Type (%d)", pNewVidPnSourceModeInfo->Type));
            break;
    }

    *pbSupported = bSupported;
    return STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCheckSourceModeSet(const D3DKMDT_HVIDPN hDesiredVidPn,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        BOOLEAN *pbSupported)
{
    const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
    NTSTATUS Status = pVidPnSourceModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
    BOOLEAN bSupported = TRUE;
    if (Status == STATUS_SUCCESS)
    {
        while (1)
        {
            Status = vboxVidPnCheckSourceModeInfo(hDesiredVidPn, pNewVidPnSourceModeInfo, &bSupported);
            if (Status == STATUS_SUCCESS && bSupported)
            {
                const D3DKMDT_VIDPN_SOURCE_MODE *pNextVidPnSourceModeInfo;
                Status = pVidPnSourceModeSetInterface->pfnAcquireNextModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo, &pNextVidPnSourceModeInfo);
                pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                if (Status == STATUS_SUCCESS)
                {
                    pNewVidPnSourceModeInfo = pNextVidPnSourceModeInfo;
                }
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                    break;
                }
            }
            else
            {
                pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("VBoxVideoWddm: pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    *pbSupported = bSupported;
    return Status;
}

NTSTATUS vboxVidPnPopulateVideoSignalInfo(D3DKMDT_VIDEO_SIGNAL_INFO *pVsi,
        D3DKMDT_2DREGION *pResolution,
        ULONG VSync)
{
    NTSTATUS Status = STATUS_SUCCESS;

    pVsi->VideoStandard  = D3DKMDT_VSS_VESA_DMT;
    pVsi->ActiveSize = *pResolution;
    pVsi->VSyncFreq.Numerator = VSync * 1000;
    pVsi->VSyncFreq.Denominator = 1000;
    pVsi->TotalSize.cx = pVsi->ActiveSize.cx + VBOXVDPN_C_DISPLAY_HBLANK_SIZE;
    pVsi->TotalSize.cy = pVsi->ActiveSize.cy + VBOXVDPN_C_DISPLAY_VBLANK_SIZE;
    pVsi->PixelRate = pVsi->TotalSize.cx * pVsi->TotalSize.cy * VSync;
    pVsi->HSyncFreq.Numerator = (UINT)((pVsi->PixelRate / pVsi->TotalSize.cy) * 1000);
    pVsi->HSyncFreq.Denominator = 1000;
    pVsi->ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;

    return Status;
}

BOOLEAN vboxVidPnMatchVideoSignal(const D3DKMDT_VIDEO_SIGNAL_INFO *pVsi1, const D3DKMDT_VIDEO_SIGNAL_INFO *pVsi2)
{
    if (pVsi1->VideoStandard != pVsi2->VideoStandard)
        return FALSE;
    if (pVsi1->TotalSize.cx != pVsi2->TotalSize.cx)
        return FALSE;
    if (pVsi1->TotalSize.cy != pVsi2->TotalSize.cy)
        return FALSE;
    if (pVsi1->ActiveSize.cx != pVsi2->ActiveSize.cx)
        return FALSE;
    if (pVsi1->ActiveSize.cy != pVsi2->ActiveSize.cy)
        return FALSE;
    if (pVsi1->VSyncFreq.Numerator != pVsi2->VSyncFreq.Numerator)
        return FALSE;
    if (pVsi1->VSyncFreq.Denominator != pVsi2->VSyncFreq.Denominator)
        return FALSE;
    if (pVsi1->HSyncFreq.Numerator != pVsi2->HSyncFreq.Numerator)
        return FALSE;
    if (pVsi1->HSyncFreq.Denominator != pVsi2->HSyncFreq.Denominator)
        return FALSE;
    if (pVsi1->PixelRate != pVsi2->PixelRate)
        return FALSE;
    if (pVsi1->ScanLineOrdering != pVsi2->ScanLineOrdering)
        return FALSE;

    return TRUE;
}

NTSTATUS vboxVidPnCheckTargetModeInfo(const D3DKMDT_HVIDPN hDesiredVidPn,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo,
        BOOLEAN *pbSupported)
{
    BOOLEAN bSupported = TRUE;
    D3DKMDT_VIDEO_SIGNAL_INFO CmpVsi;
    D3DKMDT_2DREGION CmpRes;
    CmpRes.cx = pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cx;
    CmpRes.cy = pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy;
    NTSTATUS Status = vboxVidPnPopulateVideoSignalInfo(&CmpVsi,
                &CmpRes,
                pNewVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Numerator/pNewVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Denominator);
    Assert(Status == STATUS_SUCCESS);
    if (Status != STATUS_SUCCESS)
    {
        LOGREL(("vboxVidPnPopulateVideoSignalInfo error Status (0x%x)", Status));
        return Status;
    }

    if (!vboxVidPnMatchVideoSignal(&CmpVsi, &pNewVidPnTargetModeInfo->VideoSignalInfo))
    {
        WARN(("VideoSignalInfos do not match!!!"));
        AssertBreakpoint();
        bSupported = FALSE;
    }

    *pbSupported = bSupported;
    return STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCheckTargetModeSet(const D3DKMDT_HVIDPN hDesiredVidPn,
        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        BOOLEAN *pbSupported)
{
    const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
    NTSTATUS Status = pVidPnTargetModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
    BOOLEAN bSupported = TRUE;
    if (Status == STATUS_SUCCESS)
    {
        Assert(pNewVidPnTargetModeInfo);
        while (1)
        {
            Status = vboxVidPnCheckTargetModeInfo(hDesiredVidPn, pNewVidPnTargetModeInfo, &bSupported);
            if (Status == STATUS_SUCCESS && bSupported)
            {
                const D3DKMDT_VIDPN_TARGET_MODE *pNextVidPnTargetModeInfo;
                Status = pVidPnTargetModeSetInterface->pfnAcquireNextModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo, &pNextVidPnTargetModeInfo);
                pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                if (Status == STATUS_SUCCESS)
                {
                    pNewVidPnTargetModeInfo = pNextVidPnTargetModeInfo;
                }
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                    break;
                }
            }
            else
            {
                pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    *pbSupported = bSupported;
    return Status;
}

NTSTATUS vboxVidPnPopulateSourceModeInfoFromLegacy(D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo,
        VIDEO_MODE_INFORMATION *pMode)
{
    NTSTATUS Status = STATUS_SUCCESS;
    if (pMode->AttributeFlags & VIDEO_MODE_GRAPHICS)
    {
        /* this is a graphics mode */
        pNewVidPnSourceModeInfo->Type = D3DKMDT_RMT_GRAPHICS;
        pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx = pMode->VisScreenWidth;
        pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy = pMode->VisScreenHeight;
        pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize = pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize;
        pNewVidPnSourceModeInfo->Format.Graphics.Stride = pMode->ScreenStride;
        pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat = vboxWddmCalcPixelFormat(pMode);
        Assert(pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat != D3DDDIFMT_UNKNOWN);
        if (pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat != D3DDDIFMT_UNKNOWN)
        {
            pNewVidPnSourceModeInfo->Format.Graphics.ColorBasis = D3DKMDT_CB_SRGB;
            if (pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat == D3DDDIFMT_P8)
                pNewVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_SETTABLEPALETTE;
            else
                pNewVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;
        }
        else
        {
            LOGREL(("vboxWddmCalcPixelFormat failed"));
            Status = STATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        /* @todo: XPDM driver does not seem to return text modes, should we? */
        LOGREL(("text mode not supported currently"));
        AssertBreakpoint();
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

NTSTATUS vboxVidPnPopulateMonitorSourceModeInfoFromLegacy(PVBOXMP_DEVEXT pDevExt,
        D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSourceMode,
        D3DKMDT_2DREGION *pResolution,
        D3DKMDT_MONITOR_CAPABILITIES_ORIGIN enmOrigin,
        BOOLEAN bPreferred)
{
    NTSTATUS Status = vboxVidPnPopulateVideoSignalInfo(&pMonitorSourceMode->VideoSignalInfo, pResolution, 60 /* ULONG VSync */);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        pMonitorSourceMode->ColorBasis = D3DKMDT_CB_SRGB;
        pMonitorSourceMode->ColorCoeffDynamicRanges.FirstChannel = 8;
        pMonitorSourceMode->ColorCoeffDynamicRanges.SecondChannel = 8;
        pMonitorSourceMode->ColorCoeffDynamicRanges.ThirdChannel = 8;
        pMonitorSourceMode->ColorCoeffDynamicRanges.FourthChannel = 0;
        pMonitorSourceMode->Origin = enmOrigin;
        pMonitorSourceMode->Preference = bPreferred ? D3DKMDT_MP_PREFERRED : D3DKMDT_MP_NOTPREFERRED;
    }

    return Status;
}

NTSTATUS vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy(PVBOXMP_DEVEXT pDevExt,
        CONST D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS,
        CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        D3DKMDT_2DREGION *pResolution,
        D3DKMDT_MONITOR_CAPABILITIES_ORIGIN enmOrigin,
        BOOLEAN bPreferred)
{
    D3DKMDT_MONITOR_SOURCE_MODE * pMonitorSMI;
    NTSTATUS Status = pMonitorSMSIf->pfnCreateNewModeInfo(hMonitorSMS, &pMonitorSMI);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        do
        {
            Status = vboxVidPnPopulateMonitorSourceModeInfoFromLegacy(pDevExt,
                    pMonitorSMI,
                    pResolution,
                    enmOrigin,
                    bPreferred);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                Status = pMonitorSMSIf->pfnAddMode(hMonitorSMS, pMonitorSMI);
                Assert(Status == STATUS_SUCCESS/* || Status == STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET*/);
                if (Status == STATUS_SUCCESS)
                    break;
                LOGREL(("pfnAddMode failed, Status(0x%x)", Status));
            }
            else
                LOGREL(("vboxVidPnPopulateMonitorSourceModeInfoFromLegacy failed, Status(0x%x)", Status));

            Assert (Status != STATUS_SUCCESS);
            /* we're here because of a failure */
            NTSTATUS tmpStatus = pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pMonitorSMI);
            Assert(tmpStatus == STATUS_SUCCESS);
            if (tmpStatus != STATUS_SUCCESS)
                LOGREL(("pfnReleaseModeInfo failed tmpStatus(0x%x)", tmpStatus));

            if (Status == STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET)
                Status = STATUS_SUCCESS;
        } while (0);
    }
    else
        LOGREL(("pfnCreateNewModeInfo failed, Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnPopulateTargetModeInfoFromLegacy(D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, D3DKMDT_2DREGION *pResolution, BOOLEAN fPreferred)
{
    pNewVidPnTargetModeInfo->Preference = fPreferred ? D3DKMDT_MP_PREFERRED : D3DKMDT_MP_NOTPREFERRED;
    return vboxVidPnPopulateVideoSignalInfo(&pNewVidPnTargetModeInfo->VideoSignalInfo, pResolution, 60 /* ULONG VSync */);
}

typedef struct VBOXVIDPNCHECKADDMONITORMODES
{
    NTSTATUS Status;
    D3DKMDT_2DREGION *pResolutions;
    uint32_t cResolutions;
} VBOXVIDPNCHECKADDMONITORMODES, *PVBOXVIDPNCHECKADDMONITORMODES;

static DECLCALLBACK(BOOLEAN) vboxVidPnCheckAddMonitorModesEnum(PVBOXMP_DEVEXT pDevExt, D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI, PVOID pContext)
{
    PVBOXVIDPNCHECKADDMONITORMODES pData = (PVBOXVIDPNCHECKADDMONITORMODES)pContext;
    NTSTATUS Status = STATUS_SUCCESS;

    for (uint32_t i = 0; i < pData->cResolutions; ++i)
    {
        D3DKMDT_VIDPN_TARGET_MODE dummyMode = {0};
        Status = vboxVidPnPopulateTargetModeInfoFromLegacy(&dummyMode, &pData->pResolutions[i], FALSE /* preference does not matter for now */);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            if (vboxVidPnMatchVideoSignal(&dummyMode.VideoSignalInfo, &pMonitorSMI->VideoSignalInfo))
            {
                /* mark it as unneeded */
                pData->pResolutions[i].cx = 0;
                break;
            }
        }
        else
        {
            LOGREL(("vboxVidPnPopulateTargetModeInfoFromLegacy failed Status(0x%x)", Status));
            break;
        }
    }

    pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pMonitorSMI);

    pData->Status = Status;

    return Status == STATUS_SUCCESS;
}

typedef struct VBOXVIDPNCHECKMONMODESENUM
{
    D3DKMDT_2DREGION Region;
    const D3DKMDT_MONITOR_SOURCE_MODE * pMonitorSMI;
} VBOXVIDPNCHECKMONMODESENUM, *PVBOXVIDPNCHECKMONMODESENUM;

static DECLCALLBACK(BOOLEAN) vboxFidPnCheckMonitorModesEnum(D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI, PVOID pContext)
{
    PVBOXVIDPNCHECKMONMODESENUM pInfo = (PVBOXVIDPNCHECKMONMODESENUM)pContext;
    if (pMonitorSMI->VideoSignalInfo.ActiveSize.cx == pInfo->Region.cx
            && pMonitorSMI->VideoSignalInfo.ActiveSize.cy == pInfo->Region.cy)
    {
        Assert(!pInfo->pMonitorSMI);
        if (pInfo->pMonitorSMI)
        {
            pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pInfo->pMonitorSMI);
        }
        pInfo->pMonitorSMI = pMonitorSMI;
    }
    else
    {
        pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pMonitorSMI);
    }
    return TRUE;
}

typedef struct VBOXVIDPNMATCHMONMODESENUM
{
    D3DKMDT_2DREGION *paResolutions;
    uint32_t cResolutions;
    BOOLEAN fMatch;
} VBOXVIDPNMATCHMONMODESENUM, *PVBOXVIDPNMATCHMONMODESENUM;

static DECLCALLBACK(BOOLEAN) vboxFidPnMatchMonitorModesEnum(D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI, PVOID pContext)
{
    PVBOXVIDPNMATCHMONMODESENUM pInfo = (PVBOXVIDPNMATCHMONMODESENUM)pContext;

    Assert(pInfo->fMatch);

    for (UINT i = 0; i < pInfo->cResolutions; ++i)
    {
        D3DKMDT_2DREGION *pResolution = &pInfo->paResolutions[i];
        if (pMonitorSMI->VideoSignalInfo.ActiveSize.cx != pResolution->cx
                || pMonitorSMI->VideoSignalInfo.ActiveSize.cy != pResolution->cy)
        {
            pInfo->fMatch = FALSE;
            break;
        }
    }

    pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pMonitorSMI);

    return pInfo->fMatch;
}

/* matches the monitor mode set for the given target id with the resolution set, and sets the pfMatch to true if they match, otherwise sets it to false */
NTSTATUS vboxVidPnMatchMonitorModes(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_TARGET_ID targetId,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions, BOOLEAN *pfMatch)
{
    *pfMatch = FALSE;
    CONST DXGK_MONITOR_INTERFACE *pMonitorInterface;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryMonitorInterface(pDevExt->u.primary.DxgkInterface.DeviceHandle, DXGK_MONITOR_INTERFACE_VERSION_V1, &pMonitorInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status (0x%x)", Status));
        return Status;
    }

    D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS;
    CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf;
    Status = pMonitorInterface->pfnAcquireMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle,
                                        targetId,
                                        &hMonitorSMS,
                                        &pMonitorSMSIf);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnAcquireMonitorSourceModeSet failed, Status (0x%x)", Status));
        if (Status == STATUS_GRAPHICS_MONITOR_NOT_CONNECTED)
        {
            /* this is ok in case we replug the monitor to pick up the monitor modes properly,
             * so pretend success  */
            *pfMatch = TRUE;
            Status = STATUS_SUCCESS;
        }
        return Status;
    }

    VBOXVIDPNMATCHMONMODESENUM Info;
    Info.paResolutions = pResolutions;
    Info.cResolutions = cResolutions;
    Info.fMatch = TRUE;

    Status = vboxVidPnEnumMonitorSourceModes(hMonitorSMS, pMonitorSMSIf, vboxFidPnMatchMonitorModesEnum, &Info);
    if (NT_SUCCESS(Status))
    {
        *pfMatch = Info.fMatch;
    }

    NTSTATUS tmpStatus = pMonitorInterface->pfnReleaseMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle, hMonitorSMS);
    if (!NT_SUCCESS(tmpStatus))
        WARN(("pfnReleaseMonitorSourceModeSet failed tmpStatus(0x%x)", tmpStatus));

    return Status;
}

NTSTATUS vboxVidPnCheckAddMonitorModes(PVBOXMP_DEVEXT pDevExt,
        D3DDDI_VIDEO_PRESENT_TARGET_ID targetId, D3DKMDT_MONITOR_CAPABILITIES_ORIGIN enmOrigin,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions, int iPreferred)
{
    NTSTATUS Status;
    CONST DXGK_MONITOR_INTERFACE *pMonitorInterface;
    Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryMonitorInterface(pDevExt->u.primary.DxgkInterface.DeviceHandle, DXGK_MONITOR_INTERFACE_VERSION_V1, &pMonitorInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
        return Status;
    }

    D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS;
    CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf;

    Status = pMonitorInterface->pfnAcquireMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle,
                                        targetId,
                                        &hMonitorSMS,
                                        &pMonitorSMSIf);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
        if (Status == STATUS_GRAPHICS_MONITOR_NOT_CONNECTED)
        {
            /* this is ok in case we replug the monitor to pick up the monitor modes properly,
             * so pretend success  */
            Status = STATUS_SUCCESS;
        }
        return Status;
    }

    for (uint32_t i = 0; i < cResolutions; ++i)
    {
        D3DKMDT_2DREGION *pRes = &pResolutions[i];

        Status = vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy(pDevExt,
                hMonitorSMS,
                pMonitorSMSIf,
                pRes,
                enmOrigin,
                iPreferred == i
                );
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL(("vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy failed Status(0x%x)", Status));
            break;
        }
    }

    NTSTATUS tmpStatus = pMonitorInterface->pfnReleaseMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle, hMonitorSMS);
    if (!NT_SUCCESS(tmpStatus))
    {
        WARN(("pfnReleaseMonitorSourceModeSet failed tmpStatus(0x%x)", tmpStatus));
    }

    return Status;
}

NTSTATUS vboxVidPnPathAdd(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        const D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, const D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    NTSTATUS Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo;
    Status = pVidPnTopologyInterface->pfnCreateNewPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    pNewVidPnPresentPathInfo->VidPnSourceId = VidPnSourceId;
    pNewVidPnPresentPathInfo->VidPnTargetId = VidPnTargetId;
    pNewVidPnPresentPathInfo->ImportanceOrdinal = D3DKMDT_VPPI_PRIMARY;
    pNewVidPnPresentPathInfo->ContentTransformation.Scaling = D3DKMDT_VPPS_IDENTITY;
    memset(&pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport,
            0, sizeof (pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport));
    pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Identity = 1;
    pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Stretched = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.Rotation = D3DKMDT_VPPR_IDENTITY;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Identity = 1;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate180 = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate270 = 0;
    pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate90 = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx = 0;
    pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy = 0;
    pNewVidPnPresentPathInfo->VidPnTargetColorBasis = D3DKMDT_CB_SRGB; /* @todo: how does it matters? */
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FirstChannel =  8;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.SecondChannel =  8;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.ThirdChannel =  8;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel =  0;
    pNewVidPnPresentPathInfo->Content = D3DKMDT_VPPC_GRAPHICS;
    pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType = D3DKMDT_VPPMT_UNINITIALIZED;
//                    pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType = D3DKMDT_VPPMT_NOPROTECTION;
    pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits = 0;
    memset(&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, 0, sizeof (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport));
//            pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport.NoProtection  = 1;
    memset (&pNewVidPnPresentPathInfo->GammaRamp, 0, sizeof (pNewVidPnPresentPathInfo->GammaRamp));
//            pNewVidPnPresentPathInfo->GammaRamp.Type = D3DDDI_GAMMARAMP_DEFAULT;
//            pNewVidPnPresentPathInfo->GammaRamp.DataSize = 0;
    Status = pVidPnTopologyInterface->pfnAddPath(hVidPnTopology, pNewVidPnPresentPathInfo);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        NTSTATUS tmpStatus = pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);
        Assert(NT_SUCCESS(tmpStatus));
    }

    return Status;
}

static NTSTATUS vboxVidPnCreatePopulateSourceModeInfoFromLegacy(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        const D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        VIDEO_MODE_INFORMATION *pModes, uint32_t cModes, int iModeToPin,
        D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID *pModeIdToPin,
        BOOLEAN fDoPin
        )
{
    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface;

    if (pModeIdToPin)
        *pModeIdToPin = D3DDDI_ID_UNINITIALIZED;

    NTSTATUS Status = pVidPnInterface->pfnCreateNewSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hNewVidPnSourceModeSet,
                        &pNewVidPnSourceModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID sourceModeId = D3DDDI_ID_UNINITIALIZED;

    for (uint32_t i = 0; i < cModes; ++i)
    {
        VIDEO_MODE_INFORMATION *pMode = &pModes[i];
        D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
        Status = pNewVidPnSourceModeSetInterface->pfnCreateNewModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
        if (!NT_SUCCESS(Status))
        {
            AssertFailed();
            break;
        }

        Status = vboxVidPnPopulateSourceModeInfoFromLegacy(pNewVidPnSourceModeInfo, pMode);
        if (NT_SUCCESS(Status))
        {
            if (i == iModeToPin)
            {
                sourceModeId = pNewVidPnSourceModeInfo->Id;
            }
            Status = pNewVidPnSourceModeSetInterface->pfnAddMode(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
            if (NT_SUCCESS(Status))
            {
                /* success */
                continue;
            }
            AssertFailed();
        }
        else
        {
            AssertFailed();
        }

        NTSTATUS tmpStatus = pNewVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
        Assert(tmpStatus == STATUS_SUCCESS);

        /* we're here because of an error */
        Assert(!NT_SUCCESS(Status));
        break;
    }

    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    if (sourceModeId != D3DDDI_ID_UNINITIALIZED)
    {
        if (pModeIdToPin)
        {
            *pModeIdToPin = sourceModeId;
        }
        Assert(iModeToPin >= 0);
        if (fDoPin)
        {
            Status = pNewVidPnSourceModeSetInterface->pfnPinMode(hNewVidPnSourceModeSet, sourceModeId);
            if (!NT_SUCCESS(Status))
            {
                AssertFailed();
                return Status;
            }
        }
    }
    else
    {
        Assert(iModeToPin < 0);
    }

    Status = pVidPnInterface->pfnAssignSourceModeSet(hVidPn, VidPnSourceId, hNewVidPnSourceModeSet);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    return Status;
}

static NTSTATUS vboxVidPnCreatePopulateTargetModeInfoFromLegacy(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        const D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions,
        VIDEO_MODE_INFORMATION *pModeToPin,
        D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID *pModeIdToPin,
        BOOLEAN fSetPreferred,
        BOOLEAN fDoPin
        )
{
    D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface;

    if (pModeIdToPin)
        *pModeIdToPin = D3DDDI_ID_UNINITIALIZED;

    NTSTATUS Status = pVidPnInterface->pfnCreateNewTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hNewVidPnTargetModeSet,
                        &pNewVidPnTargetModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID targetModeId = D3DDDI_ID_UNINITIALIZED;

    for (uint32_t i = 0; i < cResolutions; ++i)
    {
        D3DKMDT_2DREGION *pResolution = &pResolutions[i];
        D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
        Status = pNewVidPnTargetModeSetInterface->pfnCreateNewModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
        if (!NT_SUCCESS(Status))
        {
            AssertFailed();
            break;
        }

        BOOLEAN fIsPinMode = pModeToPin && pModeToPin->VisScreenWidth == pResolution->cx
                && pModeToPin->VisScreenHeight == pResolution->cy;

        Status = vboxVidPnPopulateTargetModeInfoFromLegacy(pNewVidPnTargetModeInfo, pResolution, fIsPinMode && fSetPreferred);
        if (NT_SUCCESS(Status))
        {
            if (fIsPinMode)
            {
                targetModeId = pNewVidPnTargetModeInfo->Id;
            }
            Status = pNewVidPnTargetModeSetInterface->pfnAddMode(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
            if (NT_SUCCESS(Status))
            {

                /* success */
                continue;
            }
            AssertFailed();
        }
        else
        {
            AssertFailed();
        }

        NTSTATUS tmpStatus = pNewVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
        Assert(tmpStatus == STATUS_SUCCESS);

        /* we're here because of an error */
        Assert(!NT_SUCCESS(Status));
        break;
    }

    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    if (targetModeId != D3DDDI_ID_UNINITIALIZED)
    {
        Assert(pModeToPin);

        if (pModeIdToPin)
        {
            *pModeIdToPin = targetModeId;
        }

        if (fDoPin)
        {
            Status = pNewVidPnTargetModeSetInterface->pfnPinMode(hNewVidPnTargetModeSet, targetModeId);
            if (!NT_SUCCESS(Status))
            {
                AssertFailed();
                return Status;
            }
        }
    }
    else
    {
        Assert(!pModeToPin);
    }

    Status = pVidPnInterface->pfnAssignTargetModeSet(hVidPn, VidPnTargetId, hNewVidPnTargetModeSet);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    return Status;
}

NTSTATUS vboxVidPnCreatePopulateVidPnPathFromLegacy(PVBOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        VIDEO_MODE_INFORMATION *pModes, uint32_t cModes, int iModeToPin,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions,
        const D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, const D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    NTSTATUS Status;

#if 0
    Status = vboxVidPnPathAdd(hVidPn, pVidPnInterface, VidPnSourceId, VidPnTargetId);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }
#endif

    VIDEO_MODE_INFORMATION *pModeToPin = iModeToPin >= 0 ? &pModes[iModeToPin] : NULL;
    Status = vboxVidPnCreatePopulateTargetModeInfoFromLegacy(hVidPn, pVidPnInterface, VidPnTargetId, pResolutions, cResolutions, pModeToPin, NULL, TRUE, TRUE);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    Status = vboxVidPnCreatePopulateSourceModeInfoFromLegacy(hVidPn, pVidPnInterface, VidPnSourceId, pModes, cModes, iModeToPin, NULL, TRUE);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    return Status;
}

typedef struct VBOXVIDPNPOPRESOLUTIONENUM
{
    NTSTATUS Status;
    D3DKMDT_2DREGION *pResolutions;
    int cResolutions;
    int cResultResolutions;
}VBOXVIDPNPOPRESOLUTIONENUM, *PVBOXVIDPNPOPRESOLUTIONENUM;

static DECLCALLBACK(BOOLEAN) vboxVidPnPopulateResolutionsFromSourceModeSetEnum(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNPOPRESOLUTIONENUM pInfo = (PVBOXVIDPNPOPRESOLUTIONENUM)pContext;
    Assert(pInfo->cResolutions >= pInfo->cResultResolutions);
    Assert(pInfo->Status == STATUS_SUCCESS);
    if (vboxWddmResolutionFind(pInfo->pResolutions, pInfo->cResultResolutions, &pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize) < 0)
    {
        if (pInfo->cResultResolutions < pInfo->cResolutions)
        {
            pInfo->pResolutions[pInfo->cResultResolutions] = pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize;
            ++pInfo->cResultResolutions;
        }
        else
        {
            Status = STATUS_BUFFER_OVERFLOW;
        }
    }

    pInfo->Status = Status;

    return Status == STATUS_SUCCESS;
}

static DECLCALLBACK(BOOLEAN) vboxVidPnPopulateResolutionsFromTargetModeSetEnum(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNPOPRESOLUTIONENUM pInfo = (PVBOXVIDPNPOPRESOLUTIONENUM)pContext;
    Assert(pInfo->cResolutions >= pInfo->cResultResolutions);
    Assert(pInfo->Status == STATUS_SUCCESS);
    if (vboxWddmResolutionFind(pInfo->pResolutions, pInfo->cResultResolutions, &pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize) < 0)
    {
        if (pInfo->cResultResolutions < pInfo->cResolutions)
        {
            pInfo->pResolutions[pInfo->cResultResolutions] = pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize;
            ++pInfo->cResultResolutions;
        }
        else
        {
            Status = STATUS_BUFFER_OVERFLOW;
        }
    }

    pInfo->Status = Status;

    return Status == STATUS_SUCCESS;
}

typedef struct VBOXVIDPNPOPMODEENUM
{
    NTSTATUS Status;
    VIDEO_MODE_INFORMATION *pModes;
    int cModes;
    int cResultModes;
}VBOXVIDPNPOPMODEENUM, *PVBOXVIDPNPOPMODEENUM;

static DECLCALLBACK(BOOLEAN) vboxVidPnPopulateModesFromSourceModeSetEnum(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNPOPMODEENUM pInfo = (PVBOXVIDPNPOPMODEENUM)pContext;
    VIDEO_MODE_INFORMATION Mode;
    Assert(pInfo->cModes >= pInfo->cResultModes);
    Assert(pInfo->Status == STATUS_SUCCESS);
    if (VBoxWddmFillMode(&Mode, pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat,
            pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cx,
            pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cy))
    {
        if (vboxWddmVideoModeFind(pInfo->pModes, pInfo->cModes, &Mode) < 0)
        {
            if (pInfo->cResultModes < pInfo->cModes)
            {
                pInfo->pModes[pInfo->cResultModes] = Mode;
                ++pInfo->cResultModes;
            }
            else
            {
                Status = STATUS_BUFFER_OVERFLOW;
            }
        }
    }
    else
    {
        Assert(0);
        Status = STATUS_INVALID_PARAMETER;
    }

    pInfo->Status = Status;

    return Status == STATUS_SUCCESS;
}

typedef struct VBOXVIDPNPOPMODETARGETENUM
{
    VBOXVIDPNPOPMODEENUM Base;
    VIDEO_MODE_INFORMATION *pSuperset;
    int cSuperset;
}VBOXVIDPNPOPMODETARGETENUM, *PVBOXVIDPNPOPMODETARGETENUM;

static DECLCALLBACK(BOOLEAN) vboxVidPnPopulateModesFromTargetModeSetEnum(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNPOPMODETARGETENUM pInfo = (PVBOXVIDPNPOPMODETARGETENUM)pContext;
    Assert(pInfo->Base.cModes >= pInfo->Base.cResultModes);
    Assert(pInfo->Base.Status == STATUS_SUCCESS);
    uint32_t cResult;
    Status = VBoxWddmGetModesForResolution(pInfo->pSuperset, pInfo->cSuperset, -1, &pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize,
            pInfo->Base.pModes + pInfo->Base.cResultModes, pInfo->Base.cModes - pInfo->Base.cResultModes, &cResult, NULL);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        pInfo->Base.cResultModes += cResult;
    }

    pInfo->Base.Status = Status;

    return Status == STATUS_SUCCESS;
}

static D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE vboxVidPnCofuncModalityCurrentPathPivot(CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* pEnumCofuncModalityArg,
                    D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    switch (pEnumCofuncModalityArg->EnumPivotType)
    {
        case D3DKMDT_EPT_VIDPNSOURCE:
            if (pEnumCofuncModalityArg->EnumPivot.VidPnSourceId == VidPnSourceId)
                return D3DKMDT_EPT_VIDPNSOURCE;
            if (pEnumCofuncModalityArg->EnumPivot.VidPnSourceId == D3DDDI_ID_ALL)
            {
#ifdef DEBUG_misha
                AssertFailed();
#endif
                return D3DKMDT_EPT_VIDPNSOURCE;
            }
            return D3DKMDT_EPT_NOPIVOT;
        case D3DKMDT_EPT_VIDPNTARGET:
            if (pEnumCofuncModalityArg->EnumPivot.VidPnTargetId == VidPnTargetId)
                return D3DKMDT_EPT_VIDPNTARGET;
            if (pEnumCofuncModalityArg->EnumPivot.VidPnTargetId == D3DDDI_ID_ALL)
            {
#ifdef DEBUG_misha
                AssertFailed();
#endif
                return D3DKMDT_EPT_VIDPNTARGET;
            }
            return D3DKMDT_EPT_NOPIVOT;
        case D3DKMDT_EPT_SCALING:
        case D3DKMDT_EPT_ROTATION:
        case D3DKMDT_EPT_NOPIVOT:
            return D3DKMDT_EPT_NOPIVOT;
        default:
            AssertFailed();
            return D3DKMDT_EPT_NOPIVOT;
    }
}

NTSTATUS vboxVidPnHasPinnedTargetMode(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, BOOLEAN *pfHas)
{
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;
    *pfHas = FALSE;
    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;
    Status = pCurVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
    if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
    {
        pPinnedVidPnTargetModeInfo = NULL;
        Status = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(Status))
    {
        LOGREL(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));
        AssertFailed();
    }
    else
    {
        Assert(pPinnedVidPnTargetModeInfo);
        NTSTATUS tmpStatus = pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        Assert(NT_SUCCESS(tmpStatus));
        *pfHas = TRUE;
    }

    NTSTATUS tmpStatus = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    Assert(tmpStatus == STATUS_SUCCESS);

    return Status;
}

NTSTATUS vboxVidPnHasPinnedSourceMode(D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, BOOLEAN *pfHas)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;
    *pfHas = FALSE;
    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;
    Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
    if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
    {
        pPinnedVidPnSourceModeInfo = NULL;
        Status = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(Status))
    {
        LOGREL(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));
        AssertFailed();
    }
    else
    {
        Assert(pPinnedVidPnSourceModeInfo);
        NTSTATUS tmpStatus = pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        Assert(NT_SUCCESS(tmpStatus));
        *pfHas = TRUE;
    }

    NTSTATUS tmpStatus = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    Assert(tmpStatus == STATUS_SUCCESS);

    return Status;
}

static NTSTATUS vboxVidPnCofuncModalityForPathTarget(PVBOXVIDPNCOFUNCMODALITY pCbContext,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    PVBOXMP_DEVEXT pDevExt = pCbContext->pDevExt;
    D3DKMDT_HVIDPN hVidPn = pCbContext->pEnumCofuncModalityArg->hConstrainingVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = pCbContext->pVidPnInterface;
    PVBOXWDDM_VIDEOMODES_INFO pInfo = &pCbContext->pInfos[VidPnTargetId];

    D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet = NULL;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface;

    if (VidPnSourceId != VidPnTargetId || pCbContext->apPathInfos[VidPnTargetId].enmState != VBOXVIDPNPATHITEM_STATE_PRESENT)
    {
        return STATUS_SUCCESS;
    }

    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;
    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                VidPnSourceId,
                &hCurVidPnSourceModeSet,
                &pCurVidPnSourceModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;
    Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
    if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
    {
        pPinnedVidPnSourceModeInfo = NULL;
        Status = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(Status))
    {
        LOGREL(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));
        AssertFailed();
    }
    else
    {
        Assert(pPinnedVidPnSourceModeInfo);
    }

    if (NT_SUCCESS(Status))
    {
        Status = pVidPnInterface->pfnCreateNewTargetModeSet(hVidPn,
                            VidPnTargetId,
                            &hNewVidPnTargetModeSet,
                            &pNewVidPnTargetModeSetInterface);
        if (NT_SUCCESS(Status))
        {
            Assert(hNewVidPnTargetModeSet);
            if (VidPnSourceId == VidPnTargetId && pCbContext->apPathInfos[VidPnTargetId].enmState == VBOXVIDPNPATHITEM_STATE_PRESENT)
            {
                Assert(VidPnSourceId == VidPnTargetId);

                for (uint32_t i = 0; i < pInfo->cResolutions; ++i)
                {
                    D3DKMDT_2DREGION *pResolution = &pInfo->aResolutions[i];
                    if (pPinnedVidPnSourceModeInfo)
                    {
                        if (pPinnedVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx != pResolution->cx
                                || pPinnedVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy != pResolution->cy)
                        {
                            continue;
                        }
                    }

                    D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
                    Status = pNewVidPnTargetModeSetInterface->pfnCreateNewModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
                    Assert(Status == STATUS_SUCCESS);
                    if (NT_SUCCESS(Status))
                    {
                        Status = vboxVidPnPopulateTargetModeInfoFromLegacy(pNewVidPnTargetModeInfo, pResolution, i == pInfo->iPreferredResolution);
                        Assert(Status == STATUS_SUCCESS);
                        if (NT_SUCCESS(Status))
                        {
                            Status = pNewVidPnTargetModeSetInterface->pfnAddMode(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                            Assert(Status == STATUS_SUCCESS);
                            if (NT_SUCCESS(Status))
                            {
                                /* success */
                                continue;
                            }
                        }

                        NTSTATUS tmpStatus = pNewVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                        Assert(tmpStatus == STATUS_SUCCESS);
                    }
                    /* we're here because of an error */
                    Assert(!NT_SUCCESS(Status));
                    break;
                }
            }
        }
        else
        {
            AssertFailed();
        }
    }
    else
    {
        AssertFailed();
    }

    if (pPinnedVidPnSourceModeInfo)
    {
        NTSTATUS tmpStatus = pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        Assert(tmpStatus == STATUS_SUCCESS);
    }

    NTSTATUS tmpStatus = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    Assert(tmpStatus == STATUS_SUCCESS);

    if (NT_SUCCESS(Status))
    {
        Assert(hNewVidPnTargetModeSet);
        Status = pVidPnInterface->pfnAssignTargetModeSet(hVidPn, VidPnTargetId, hNewVidPnTargetModeSet);
        if (!NT_SUCCESS(Status))
        {
            WARN(("\n\n!!!!!!!\n\n pfnAssignTargetModeSet failed, Status(0x%x)", Status));
            tmpStatus = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hNewVidPnTargetModeSet);
            Assert(tmpStatus == STATUS_SUCCESS);
        }
    }

    return Status;
}

static NTSTATUS vboxVidPnCofuncModalityForPathSource(PVBOXVIDPNCOFUNCMODALITY pCbContext,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    PVBOXMP_DEVEXT pDevExt = pCbContext->pDevExt;
    D3DKMDT_HVIDPN hVidPn = pCbContext->pEnumCofuncModalityArg->hConstrainingVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = pCbContext->pVidPnInterface;
    PVBOXWDDM_VIDEOMODES_INFO pInfo = &pCbContext->pInfos[VidPnTargetId];
    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet = NULL;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface;

    if (VidPnSourceId != VidPnTargetId || pCbContext->apPathInfos[VidPnSourceId].enmState != VBOXVIDPNPATHITEM_STATE_PRESENT)
    {
        return STATUS_SUCCESS;
    }

    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;
    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;
    Status = pCurVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
    if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
    {
        pPinnedVidPnTargetModeInfo = NULL;
        Status = STATUS_SUCCESS;
    }
    else if (!NT_SUCCESS(Status))
    {
        LOGREL(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));
        AssertFailed();
    }
    else
    {
        Assert(pPinnedVidPnTargetModeInfo);
    }

    if (NT_SUCCESS(Status))
    {
        NTSTATUS Status = pVidPnInterface->pfnCreateNewSourceModeSet(hVidPn,
                            VidPnSourceId,
                            &hNewVidPnSourceModeSet,
                            &pNewVidPnSourceModeSetInterface);
        if (NT_SUCCESS(Status))
        {
            Assert(hNewVidPnSourceModeSet);
            if (VidPnSourceId == VidPnTargetId && pCbContext->apPathInfos[VidPnSourceId].enmState == VBOXVIDPNPATHITEM_STATE_PRESENT)
            {
                Assert(VidPnSourceId == VidPnTargetId);
                for (uint32_t i = 0; i < pInfo->cModes; ++i)
                {
                    VIDEO_MODE_INFORMATION *pMode = &pInfo->aModes[i];
                    if (pPinnedVidPnTargetModeInfo)
                    {
                        if (pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cx != pMode->VisScreenWidth
                                || pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy != pMode->VisScreenHeight)
                        {
                            continue;
                        }
                    }

                    D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
                    Status = pNewVidPnSourceModeSetInterface->pfnCreateNewModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
                    Assert(Status == STATUS_SUCCESS);
                    if (NT_SUCCESS(Status))
                    {
                        Status = vboxVidPnPopulateSourceModeInfoFromLegacy(pNewVidPnSourceModeInfo, pMode);
                        Assert(Status == STATUS_SUCCESS);
                        if (NT_SUCCESS(Status))
                        {
                            Status = pNewVidPnSourceModeSetInterface->pfnAddMode(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                            Assert(Status == STATUS_SUCCESS);
                            if (NT_SUCCESS(Status))
                            {
                                /* success */
                                continue;
                            }
                        }

                        NTSTATUS tmpStatus = pNewVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                        Assert(tmpStatus == STATUS_SUCCESS);
                    }
                    /* we're here because of an error */
                    Assert(!NT_SUCCESS(Status));
                    break;
                }
            }
        }
        else
        {
            AssertFailed();
        }
    }
    else
    {
        AssertFailed();
    }

    if (pPinnedVidPnTargetModeInfo)
    {
        NTSTATUS tmpStatus = pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        Assert(tmpStatus == STATUS_SUCCESS);
    }

    NTSTATUS tmpStatus = pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    Assert(tmpStatus == STATUS_SUCCESS);

    if (NT_SUCCESS(Status))
    {
        Assert(hNewVidPnSourceModeSet);
        Status = pVidPnInterface->pfnAssignSourceModeSet(hVidPn, VidPnSourceId, hNewVidPnSourceModeSet);
        if (!NT_SUCCESS(Status))
        {
            WARN(("\n\n!!!!!!!\n\n pfnAssignSourceModeSet failed, Status(0x%x)", Status));
            tmpStatus = pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hNewVidPnSourceModeSet);
            Assert(tmpStatus == STATUS_SUCCESS);
        }
    }

    return Status;
}

NTSTATUS vboxVidPnCofuncModalityForPath(PVBOXVIDPNCOFUNCMODALITY pCbContext,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    PVBOXMP_DEVEXT pDevExt = pCbContext->pDevExt;
    D3DKMDT_HVIDPN hVidPn = pCbContext->pEnumCofuncModalityArg->hConstrainingVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = pCbContext->pVidPnInterface;
    NTSTATUS Status = STATUS_SUCCESS;
    pCbContext->Status = STATUS_SUCCESS;
    PVBOXWDDM_VIDEOMODES_INFO pInfo = &pCbContext->pInfos[VidPnTargetId];

    D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE enmPivot = vboxVidPnCofuncModalityCurrentPathPivot(pCbContext->pEnumCofuncModalityArg, VidPnSourceId, VidPnTargetId);
    BOOLEAN fHasPinnedMode = FALSE;
    Status = vboxVidPnHasPinnedTargetMode(hVidPn, pVidPnInterface, VidPnTargetId, &fHasPinnedMode);
    if (!NT_SUCCESS(Status))
    {
        AssertFailed();
        return Status;
    }

    BOOLEAN fNeedUpdate = enmPivot != D3DKMDT_EPT_VIDPNTARGET && !fHasPinnedMode;
    if (fNeedUpdate)
    {
        Status = vboxVidPnCofuncModalityForPathTarget(pCbContext, VidPnSourceId, VidPnTargetId);
    }

    if (NT_SUCCESS(Status))
    {
        fHasPinnedMode = FALSE;
        Status = vboxVidPnHasPinnedSourceMode(hVidPn, pVidPnInterface, VidPnSourceId, &fHasPinnedMode);
        if (!NT_SUCCESS(Status))
        {
            AssertFailed();
            return Status;
        }

        fNeedUpdate = enmPivot != D3DKMDT_EPT_VIDPNSOURCE && !fHasPinnedMode;
        if (fNeedUpdate)
        {
            Status = vboxVidPnCofuncModalityForPathSource(pCbContext, VidPnSourceId, VidPnTargetId);
        }
    }

    return Status;
}

DECLCALLBACK(BOOLEAN) vboxVidPnCofuncModalityPathEnum(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo, PVOID pContext)
{
    PVBOXVIDPNCOFUNCMODALITY pCbContext = (PVBOXVIDPNCOFUNCMODALITY)pContext;
    D3DKMDT_VIDPN_PRESENT_PATH AdjustedPath = {0};
    NTSTATUS Status = STATUS_SUCCESS;
    bool bUpdatePath = false;
    AdjustedPath.VidPnSourceId = pNewVidPnPresentPathInfo->VidPnSourceId;
    AdjustedPath.VidPnTargetId = pNewVidPnPresentPathInfo->VidPnTargetId;
    AdjustedPath.ContentTransformation = pNewVidPnPresentPathInfo->ContentTransformation;
    AdjustedPath.CopyProtection = pNewVidPnPresentPathInfo->CopyProtection;

    if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling == D3DKMDT_VPPS_UNPINNED)
    {
        AdjustedPath.ContentTransformation.ScalingSupport.Identity = TRUE;
        bUpdatePath = true;
    }

    if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation == D3DKMDT_VPPR_UNPINNED)
    {
        AdjustedPath.ContentTransformation.RotationSupport.Identity = TRUE;
        bUpdatePath = true;
    }

    if (bUpdatePath)
    {
        Status = pVidPnTopologyInterface->pfnUpdatePathSupportInfo(hVidPnTopology, &AdjustedPath);
        Assert(Status == STATUS_SUCCESS);
    }

    Status = vboxVidPnCofuncModalityForPath(pCbContext, pNewVidPnPresentPathInfo->VidPnSourceId, pNewVidPnPresentPathInfo->VidPnTargetId);

    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);

    pCbContext->Status = Status;
    Assert(Status == STATUS_SUCCESS);
    return Status == STATUS_SUCCESS;
}

static BOOLEAN vboxVidPnIsPathSupported(const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo)
{
    if (pNewVidPnPresentPathInfo->VidPnSourceId != pNewVidPnPresentPathInfo->VidPnTargetId)
    {
        WARN(("unsupported source(%d)->target(%d) pair", pNewVidPnPresentPathInfo->VidPnSourceId, pNewVidPnPresentPathInfo->VidPnTargetId));
        return FALSE;
    }

    /*
    ImportanceOrdinal does not matter for now
    pNewVidPnPresentPathInfo->ImportanceOrdinal
    */

    if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_UNPINNED
            && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY
            && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_NOTSPECIFIED)
    {
        WARN(("unsupported Scaling (%d)", pNewVidPnPresentPathInfo->ContentTransformation.Scaling));
        return FALSE;
    }

    if (    !pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Identity
         || pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered
         || pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Stretched)
    {
        WARN(("unsupported Scaling support"));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_UNPINNED
            && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY
            && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_NOTSPECIFIED)
    {
        WARN(("unsupported rotation (%d)", pNewVidPnPresentPathInfo->ContentTransformation.Rotation));
        return FALSE;
    }

    if (    !pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Identity
         || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate90
         || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate180
         || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate270)
    {
        WARN(("unsupported RotationSupport"));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx
            || pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy)
    {
        WARN(("Non-zero TLOffset: cx(%d), cy(%d)",
                pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx,
                pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx
            || pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy)
    {
        WARN(("Non-zero TLOffset: cx(%d), cy(%d)",
                pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx,
                pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_SRGB
            && pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_UNINITIALIZED)
    {
        WARN(("unsupported VidPnTargetColorBasis (%d)", pNewVidPnPresentPathInfo->VidPnTargetColorBasis));
        return FALSE;
    }

    /* channels?
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FirstChannel;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.SecondChannel;
    pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.ThirdChannel;
    we definitely not support fourth channel
    */
    if (pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel)
    {
        WARN(("Non-zero FourthChannel (%d)", pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel));
        return FALSE;
    }

    /* Content (D3DKMDT_VPPC_GRAPHICS, _NOTSPECIFIED, _VIDEO), does not matter for now
    pNewVidPnPresentPathInfo->Content
    */
    /* not support copy protection for now */
    if (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_NOPROTECTION
            && pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_UNINITIALIZED)
    {
        WARN(("Copy protection not supported CopyProtectionType(%d)", pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits)
    {
        WARN(("Copy protection not supported APSTriggerBits(%d)", pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits));
        return FALSE;
    }

    D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION_SUPPORT tstCPSupport = {0};
    tstCPSupport.NoProtection = 1;
    if (memcmp(&tstCPSupport, &pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, sizeof(tstCPSupport)))
    {
        WARN(("Copy protection support (0x%x)", *((UINT*)&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport)));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_DEFAULT
            && pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_UNINITIALIZED)
    {
        WARN(("Unsupported GammaRamp.Type (%d)", pNewVidPnPresentPathInfo->GammaRamp.Type));
        return FALSE;
    }

    if (pNewVidPnPresentPathInfo->GammaRamp.DataSize != 0)
    {
        WARN(("Warning: non-zero GammaRamp.DataSize (%d), treating as supported", pNewVidPnPresentPathInfo->GammaRamp.DataSize));
    }

    return TRUE;
}

typedef struct VBOXVIDPNGETPATHSINFO
{
    NTSTATUS Status;
    BOOLEAN fBreakOnDisabled;
    BOOLEAN fDisabledFound;
    UINT cItems;
    PVBOXVIDPNPATHITEM paItems;
} VBOXVIDPNGETPATHSINFO, *PVBOXVIDPNGETPATHSINFO;

static DECLCALLBACK(BOOLEAN) vboxVidPnCheckTopologyEnum(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo, PVOID pContext)
{
    PVBOXVIDPNGETPATHSINFO pCbContext = (PVBOXVIDPNGETPATHSINFO)pContext;
    NTSTATUS Status = STATUS_SUCCESS;
    CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId = pNewVidPnPresentPathInfo->VidPnSourceId;
    CONST D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId = pNewVidPnPresentPathInfo->VidPnTargetId;
    BOOLEAN fDisabledFound = !vboxVidPnIsPathSupported(pNewVidPnPresentPathInfo);
    do
    {
        if (fDisabledFound)
        {
            if (pCbContext->cItems > VidPnSourceId)
            {
                pCbContext->paItems[VidPnSourceId].enmState = VBOXVIDPNPATHITEM_STATE_DISABLED;
            }
            else
            {
                AssertFailed();
                Status = STATUS_BUFFER_OVERFLOW;
                break;
            }

            if (pCbContext->cItems > VidPnTargetId)
            {
                pCbContext->paItems[VidPnTargetId].enmState = VBOXVIDPNPATHITEM_STATE_DISABLED;
            }
            else
            {
                AssertFailed();
                Status = STATUS_BUFFER_OVERFLOW;
                break;
            }

            break;
        }

        /* VidPnSourceId == VidPnTargetId */
        if (pCbContext->cItems > VidPnSourceId)
        {
            if (pCbContext->paItems[VidPnSourceId].enmState != VBOXVIDPNPATHITEM_STATE_DISABLED)
            {
                Assert(pCbContext->paItems[VidPnSourceId].enmState == VBOXVIDPNPATHITEM_STATE_NOT_EXISTS);
                pCbContext->paItems[VidPnSourceId].enmState = VBOXVIDPNPATHITEM_STATE_PRESENT;
            }
        }
        else
        {
            AssertFailed();
            Status = STATUS_BUFFER_OVERFLOW;
            break;
        }
    } while (0);

    pCbContext->fDisabledFound |= fDisabledFound;
    pCbContext->Status = Status;
    if (!NT_SUCCESS(Status))
        return FALSE; /* do not continue on failure */

    return !fDisabledFound || !pCbContext->fBreakOnDisabled;
}

/* we currently support only 0 -> 0, 1 -> 1, 2 -> 2 paths, AND 0 -> 0 must be present
 * this routine disables all paths unsupported */
NTSTATUS vboxVidPnCheckTopology(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
                                    BOOLEAN fBreakOnDisabled, UINT cItems, PVBOXVIDPNPATHITEM paItems, BOOLEAN *pfDisabledFound)
{
    UINT i;
    for (i = 0; i < cItems; ++i)
    {
        paItems[i].enmState = VBOXVIDPNPATHITEM_STATE_NOT_EXISTS;
    }
    VBOXVIDPNGETPATHSINFO CbContext = {0};
    CbContext.Status = STATUS_SUCCESS;
    CbContext.fBreakOnDisabled = fBreakOnDisabled;
    CbContext.fDisabledFound = FALSE;
    CbContext.cItems = cItems;
    CbContext.paItems = paItems;
    NTSTATUS Status = vboxVidPnEnumPaths(hVidPnTopology, pVidPnTopologyInterface, vboxVidPnCheckTopologyEnum, &CbContext);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxVidPnEnumPaths failed Status()0x%x\n", Status));
        return Status;
    }

    Status = CbContext.Status;
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxVidPnCheckTopologyEnum returned failed Status()0x%x\n", Status));
        return Status;
    }

    if (pfDisabledFound)
        *pfDisabledFound = CbContext.fDisabledFound;

    if (!fBreakOnDisabled)
    {
        /* now check if 0->0 path is present and enabled, and if not, disable everything */
        if (cItems && paItems[0].enmState != VBOXVIDPNPATHITEM_STATE_PRESENT)
        {
            LOGREL(("path 0 not set to present\n"));
            for (i = 0; i < cItems; ++i)
            {
                if (paItems[i].enmState == VBOXVIDPNPATHITEM_STATE_PRESENT)
                    paItems[i].enmState = VBOXVIDPNPATHITEM_STATE_DISABLED;
            }
        }
    }

    return Status;
}

NTSTATUS vboxVidPnEnumMonitorSourceModes(D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        PFNVBOXVIDPNENUMMONITORSOURCEMODES pfnCallback, PVOID pContext)
{
    CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI;
    NTSTATUS Status = pMonitorSMSIf->pfnAcquireFirstModeInfo(hMonitorSMS, &pMonitorSMI);
    Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_DATASET_IS_EMPTY);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pMonitorSMI);
        while (1)
        {
            CONST D3DKMDT_MONITOR_SOURCE_MODE *pNextMonitorSMI;
            Status = pMonitorSMSIf->pfnAcquireNextModeInfo(hMonitorSMS, pMonitorSMI, &pNextMonitorSMI);
            if (!pfnCallback(hMonitorSMS, pMonitorSMSIf, pMonitorSMI, pContext))
            {
                Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET);
                if (Status == STATUS_SUCCESS)
                    pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pNextMonitorSMI);
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }
                break;
            }
            else if (Status == STATUS_SUCCESS)
                pMonitorSMI = pNextMonitorSMI;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                pNextMonitorSMI = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumSourceModes(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
                                    PFNVBOXVIDPNENUMSOURCEMODES pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
    NTSTATUS Status = pVidPnSourceModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pNewVidPnSourceModeInfo);
        while (1)
        {
            const D3DKMDT_VIDPN_SOURCE_MODE *pNextVidPnSourceModeInfo;
            Status = pVidPnSourceModeSetInterface->pfnAcquireNextModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo, &pNextVidPnSourceModeInfo);
            if (!pfnCallback(hNewVidPnSourceModeSet, pVidPnSourceModeSetInterface,
                    pNewVidPnSourceModeInfo, pContext))
            {
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                    pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNextVidPnSourceModeInfo);
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnSourceModeInfo = pNextVidPnSourceModeInfo;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                pNewVidPnSourceModeInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumTargetModes(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        PFNVBOXVIDPNENUMTARGETMODES pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
    NTSTATUS Status = pVidPnTargetModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
    if (Status == STATUS_SUCCESS)
    {
        Assert(pNewVidPnTargetModeInfo);
        while (1)
        {
            const D3DKMDT_VIDPN_TARGET_MODE *pNextVidPnTargetModeInfo;
            Status = pVidPnTargetModeSetInterface->pfnAcquireNextModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo, &pNextVidPnTargetModeInfo);
            if (!pfnCallback(hNewVidPnTargetModeSet, pVidPnTargetModeSetInterface,
                    pNewVidPnTargetModeInfo, pContext))
            {
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                    pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNextVidPnTargetModeInfo);
                else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                {
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnTargetModeInfo = pNextVidPnTargetModeInfo;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextModeInfo Failed Status(0x%x)", Status));
                pNewVidPnTargetModeInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumTargetsForSource(PVBOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId,
        PFNVBOXVIDPNENUMTARGETSFORSOURCE pfnCallback, PVOID pContext)
{
    SIZE_T cTgtPaths;
    NTSTATUS Status = pVidPnTopologyInterface->pfnGetNumPathsFromSource(hVidPnTopology, VidPnSourceId, &cTgtPaths);
    Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY);
    if (Status == STATUS_SUCCESS)
    {
        for (SIZE_T i = 0; i < cTgtPaths; ++i)
        {
            D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId;
            Status = pVidPnTopologyInterface->pfnEnumPathTargetsFromSource(hVidPnTopology, VidPnSourceId, i, &VidPnTargetId);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                if (!pfnCallback(pDevExt, hVidPnTopology, pVidPnTopologyInterface, VidPnSourceId, VidPnTargetId, cTgtPaths, pContext))
                    break;
            }
            else
            {
                LOGREL(("pfnEnumPathTargetsFromSource failed Status(0x%x)", Status));
                break;
            }
        }
    }
    else if (Status != STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY)
        LOGREL(("pfnGetNumPathsFromSource failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumPaths(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        PFNVBOXVIDPNENUMPATHS pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo = NULL;
    NTSTATUS Status = pVidPnTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
    if (Status == STATUS_SUCCESS)
    {
        while (1)
        {
            const D3DKMDT_VIDPN_PRESENT_PATH *pNextVidPnPresentPathInfo;
            Status = pVidPnTopologyInterface->pfnAcquireNextPathInfo(hVidPnTopology, pNewVidPnPresentPathInfo, &pNextVidPnPresentPathInfo);

            if (!pfnCallback(hVidPnTopology, pVidPnTopologyInterface, pNewVidPnPresentPathInfo, pContext))
            {
                if (Status == STATUS_SUCCESS)
                    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNextVidPnPresentPathInfo);
                else
                {
                    Assert(Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET);
                    if (Status != STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                        LOGREL(("pfnAcquireNextPathInfo Failed Status(0x%x), ignored since callback returned false", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnPresentPathInfo = pNextVidPnPresentPathInfo;
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                break;
            }
            else
            {
                AssertBreakpoint();
                LOGREL(("pfnAcquireNextPathInfo Failed Status(0x%x)", Status));
                pNewVidPnPresentPathInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        LOGREL(("pfnAcquireFirstModeInfo failed Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnSetupSourceInfo(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, PVBOXWDDM_SOURCE pSource, CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, PVBOXWDDM_ALLOCATION pAllocation)
{
    vboxWddmAssignPrimary(pDevExt, pSource, pAllocation, srcId);
    return STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCommitSourceMode(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, PVBOXWDDM_ALLOCATION pAllocation)
{
    Assert(srcId < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
    if (srcId < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays)
    {
        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[srcId];
        return vboxVidPnSetupSourceInfo(pDevExt, srcId, pSource, pVidPnSourceModeInfo, pAllocation);
    }

    LOGREL(("invalid srcId (%d), cSources(%d)", srcId, VBoxCommonFromDeviceExt(pDevExt)->cDisplays));
    return STATUS_INVALID_PARAMETER;
}

typedef struct VBOXVIDPNCOMMITTARGETMODE
{
    NTSTATUS Status;
    D3DKMDT_HVIDPN hVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface;
} VBOXVIDPNCOMMITTARGETMODE;

DECLCALLBACK(BOOLEAN) vboxVidPnCommitTargetModeEnum(PVBOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, SIZE_T cTgtPaths, PVOID pContext)
{
    VBOXVIDPNCOMMITTARGETMODE *pInfo = (VBOXVIDPNCOMMITTARGETMODE*)pContext;
    Assert(cTgtPaths <= (SIZE_T)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
    D3DKMDT_HVIDPNTARGETMODESET hVidPnTargetModeSet;
    CONST DXGK_VIDPNTARGETMODESET_INTERFACE* pVidPnTargetModeSetInterface;
    NTSTATUS Status = pInfo->pVidPnInterface->pfnAcquireTargetModeSet(pInfo->hVidPn, VidPnTargetId, &hVidPnTargetModeSet, &pVidPnTargetModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;
        Status = pVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            VBOXWDDM_TARGET *pTarget = &pDevExt->aTargets[VidPnTargetId];
            if (pTarget->HeightVisible != pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy
                    || pTarget->HeightTotal != pPinnedVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy)
            {
                pTarget->HeightVisible = pPinnedVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy;
                pTarget->HeightTotal = pPinnedVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy;
                pTarget->ScanLineState = 0;
            }
            pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        }

        pInfo->pVidPnInterface->pfnReleaseTargetModeSet(pInfo->hVidPn, hVidPnTargetModeSet);
    }
    else
        LOGREL(("pfnAcquireTargetModeSet failed Status(0x%x)", Status));

    pInfo->Status = Status;
    return Status == STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCommitSourceModeForSrcId(PVBOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, PVBOXWDDM_ALLOCATION pAllocation)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

#ifdef DEBUG_misha
    if (pAllocation)
    {
        Assert(pAllocation->SurfDesc.VidPnSourceId == srcId);
    }
#endif

    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hDesiredVidPn,
                srcId,
                &hCurVidPnSourceModeSet,
                &pCurVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;
        Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_SUCCESS)
        {
            Assert(pPinnedVidPnSourceModeInfo);
            Status = vboxVidPnCommitSourceMode(pDevExt, srcId, pPinnedVidPnSourceModeInfo, pAllocation);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
                CONST DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
                Status = pVidPnInterface->pfnGetTopology(hDesiredVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    VBOXVIDPNCOMMITTARGETMODE TgtModeInfo = {0};
                    TgtModeInfo.Status = STATUS_SUCCESS; /* <- to ensure we're succeeded if no targets are set */
                    TgtModeInfo.hVidPn = hDesiredVidPn;
                    TgtModeInfo.pVidPnInterface = pVidPnInterface;
                    Status = vboxVidPnEnumTargetsForSource(pDevExt, hVidPnTopology, pVidPnTopologyInterface,
                            srcId,
                            vboxVidPnCommitTargetModeEnum, &TgtModeInfo);
                    Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY);
                    if (Status == STATUS_SUCCESS)
                    {
                        Status = TgtModeInfo.Status;
                        Assert(Status == STATUS_SUCCESS);
                    }
                    else if (Status == STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY)
                    {
                        Status = STATUS_SUCCESS;
                    }
                    else
                        LOGREL(("vboxVidPnEnumTargetsForSource failed Status(0x%x)", Status));
                }
                else
                    LOGREL(("pfnGetTopology failed Status(0x%x)", Status));
            }
            else
                LOGREL(("vboxVidPnCommitSourceMode failed Status(0x%x)", Status));
            /* release */
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            Status = vboxVidPnCommitSourceMode(pDevExt, srcId, NULL, pAllocation);
            Assert(Status == STATUS_SUCCESS);
        }
        else
            LOGREL(("pfnAcquirePinnedModeInfo failed Status(0x%x)", Status));

        pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        LOGREL(("pfnAcquireSourceModeSet failed Status(0x%x)", Status));
    }

    return Status;
}

DECLCALLBACK(BOOLEAN) vboxVidPnCommitPathEnum(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNCOMMIT pCommitInfo = (PVBOXVIDPNCOMMIT)pContext;
    PVBOXMP_DEVEXT pDevExt = pCommitInfo->pDevExt;
    const D3DKMDT_HVIDPN hDesiredVidPn = pCommitInfo->pCommitVidPnArg->hFunctionalVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = pCommitInfo->pVidPnInterface;

    if (pCommitInfo->pCommitVidPnArg->AffectedVidPnSourceId == D3DDDI_ID_ALL
            || pCommitInfo->pCommitVidPnArg->AffectedVidPnSourceId == pVidPnPresentPathInfo->VidPnSourceId)
    {
        Status = vboxVidPnCommitSourceModeForSrcId(pDevExt, hDesiredVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnSourceId, (PVBOXWDDM_ALLOCATION)pCommitInfo->pCommitVidPnArg->hPrimaryAllocation);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
            LOGREL(("vboxVidPnCommitSourceModeForSrcId failed Status(0x%x)", Status));
    }

    pCommitInfo->Status = Status;
    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathInfo);
    return Status == STATUS_SUCCESS;
}

#define VBOXVIDPNDUMP_STRCASE(_t) \
        case _t: return #_t;
#define VBOXVIDPNDUMP_STRCASE_UNKNOWN() \
        default: Assert(0); return "Unknown";

#define VBOXVIDPNDUMP_STRFLAGS(_v, _t) \
        if ((_v)._t return #_t;

const char* vboxVidPnDumpStrImportance(D3DKMDT_VIDPN_PRESENT_PATH_IMPORTANCE ImportanceOrdinal)
{
    switch (ImportanceOrdinal)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_PRIMARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_SECONDARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_TERTIARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_QUATERNARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_QUINARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_SENARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_SEPTENARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_OCTONARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_NONARY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPI_DENARY);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrScaling(D3DKMDT_VIDPN_PRESENT_PATH_SCALING Scaling)
{
    switch (Scaling)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_IDENTITY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_CENTERED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_STRETCHED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_UNPINNED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPS_NOTSPECIFIED);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrRotation(D3DKMDT_VIDPN_PRESENT_PATH_ROTATION Rotation)
{
    switch (Rotation)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_IDENTITY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_ROTATE90);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_ROTATE180);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_ROTATE270);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_UNPINNED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPR_NOTSPECIFIED);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrColorBasis(const D3DKMDT_COLOR_BASIS ColorBasis)
{
    switch (ColorBasis)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_CB_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_CB_INTENSITY);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_CB_SRGB);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_CB_SCRGB);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_CB_YCBCR);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_CB_YPBPR);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrPvam(D3DKMDT_PIXEL_VALUE_ACCESS_MODE PixelValueAccessMode)
{
    switch (PixelValueAccessMode)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_DIRECT);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_PRESETPALETTE);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_PVAM_SETTABLEPALETTE);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}



const char* vboxVidPnDumpStrContent(D3DKMDT_VIDPN_PRESENT_PATH_CONTENT Content)
{
    switch (Content)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_GRAPHICS);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_VIDEO);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPC_NOTSPECIFIED);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrCopyProtectionType(D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION_TYPE CopyProtectionType)
{
    switch (CopyProtectionType)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_NOPROTECTION);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_MACROVISION_APSTRIGGER);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VPPMT_MACROVISION_FULLSUPPORT);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrGammaRampType(D3DDDI_GAMMARAMP_TYPE Type)
{
    switch (Type)
    {
        VBOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_DEFAULT);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_RGB256x3x16);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_GAMMARAMP_DXGI_1);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrSourceModeType(D3DKMDT_VIDPN_SOURCE_MODE_TYPE Type)
{
    switch (Type)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_RMT_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_RMT_GRAPHICS);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_RMT_TEXT);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrScanLineOrdering(D3DDDI_VIDEO_SIGNAL_SCANLINE_ORDERING ScanLineOrdering)
{
    switch (ScanLineOrdering)
    {
        VBOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_PROGRESSIVE);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_INTERLACED_UPPERFIELDFIRST);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_INTERLACED_LOWERFIELDFIRST);
        VBOXVIDPNDUMP_STRCASE(D3DDDI_VSSLO_OTHER);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrCFMPivotType(D3DKMDT_ENUMCOFUNCMODALITY_PIVOT_TYPE EnumPivotType)
{
    switch (EnumPivotType)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_VIDPNSOURCE);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_VIDPNTARGET);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_SCALING);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_ROTATION);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_EPT_NOPIVOT);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrModePreference(D3DKMDT_MODE_PREFERENCE Preference)
{
    switch (Preference)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_MP_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_MP_PREFERRED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_MP_NOTPREFERRED);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrSignalStandard(D3DKMDT_VIDEO_SIGNAL_STANDARD VideoStandard)
{
    switch (VideoStandard)
    {
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_UNINITIALIZED);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_VESA_DMT);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_VESA_GTF);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_VESA_CVT);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_IBM);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_APPLE);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_NTSC_M);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_NTSC_J);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_NTSC_443);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_B);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_B1);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_G);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_H);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_I);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_D);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_N);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_NC);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_B);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_D);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_G);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_H);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_K);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_K1);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_L);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_SECAM_L1);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_EIA_861);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_EIA_861A);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_EIA_861B);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_K);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_K1);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_L);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_PAL_M);
        VBOXVIDPNDUMP_STRCASE(D3DKMDT_VSS_OTHER);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

const char* vboxVidPnDumpStrPixFormat(D3DDDIFORMAT PixelFormat)
{
    switch (PixelFormat)
    {
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_UNKNOWN);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_R8G8B8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8R8G8B8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8R8G8B8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_R5G6B5);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X1R5G5B5);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A1R5G5B5);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A4R4G4B4);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_R3G3B2);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8R3G3B2);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X4R4G4B4);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A2B10G10R10);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8B8G8R8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8B8G8R8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_G16R16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A2R10G10B10);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A16B16G16R16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8P8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_R32F);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_G32R32F);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A32B32G32R32F);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_CxV8U8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A1);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_BINARYBUFFER);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_VERTEXDATA);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_INDEX16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_INDEX32);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_Q16W16V16U16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_MULTI2_ARGB8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_R16F);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_G16R16F);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A16B16G16R16F);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D32F_LOCKABLE);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24FS8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D32_LOCKABLE);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_S8_LOCKABLE);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_S1D15);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_S8D24);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8D24);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X4S4D24);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_L16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_UYVY);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_R8G8_B8G8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_YUY2);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_G8R8_G8B8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT1);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT2);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT3);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT4);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_DXT5);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D16_LOCKABLE);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D32);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D15S1);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24S8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24X8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D24X4S4);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_D16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_P8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_L8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A8L8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A4L4);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_V8U8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_L6V5U5);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_X8L8V8U8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_Q8W8V8U8);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_V16U16);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_W11V11U10);
        VBOXVIDPNDUMP_STRCASE(D3DDDIFMT_A2W10V10U10);
        VBOXVIDPNDUMP_STRCASE_UNKNOWN();
    }
}

void vboxVidPnDumpCopyProtectoin(const char *pPrefix, const D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION *pCopyProtection, const char *pSuffix)
{
    LOGREL_EXACT(("%sType(%s), TODO%s", pPrefix,
            vboxVidPnDumpStrCopyProtectionType(pCopyProtection->CopyProtectionType), pSuffix));
}


void vboxVidPnDumpPathTransformation(const D3DKMDT_VIDPN_PRESENT_PATH_TRANSFORMATION *pContentTransformation)
{
    LOGREL_EXACT(("  --Transformation: Scaling(%s), ScalingSupport(%d), Rotation(%s), RotationSupport(%d)--",
            vboxVidPnDumpStrScaling(pContentTransformation->Scaling), pContentTransformation->ScalingSupport,
            vboxVidPnDumpStrRotation(pContentTransformation->Rotation), pContentTransformation->RotationSupport));
}

void vboxVidPnDumpRegion(const char *pPrefix, const D3DKMDT_2DREGION *pRegion, const char *pSuffix)
{
    LOGREL_EXACT(("%s%dX%d%s", pPrefix, pRegion->cx, pRegion->cy, pSuffix));
}

void vboxVidPnDumpRational(const char *pPrefix, const D3DDDI_RATIONAL *pRational, const char *pSuffix)
{
    LOGREL_EXACT(("%s%d/%d=%d%s", pPrefix, pRational->Numerator, pRational->Denominator, pRational->Numerator/pRational->Denominator, pSuffix));
}

void vboxVidPnDumpRanges(const char *pPrefix, const D3DKMDT_COLOR_COEFF_DYNAMIC_RANGES *pDynamicRanges, const char *pSuffix)
{
    LOGREL_EXACT(("%sFirstChannel(%d), SecondChannel(%d), ThirdChannel(%d), FourthChannel(%d)%s", pPrefix,
            pDynamicRanges->FirstChannel,
            pDynamicRanges->SecondChannel,
            pDynamicRanges->ThirdChannel,
            pDynamicRanges->FourthChannel,
            pSuffix));
}

void vboxVidPnDumpGammaRamp(const char *pPrefix, const D3DKMDT_GAMMA_RAMP *pGammaRamp, const char *pSuffix)
{
    LOGREL_EXACT(("%sType(%s), DataSize(%d), TODO: dump the rest%s", pPrefix,
            vboxVidPnDumpStrGammaRampType(pGammaRamp->Type), pGammaRamp->DataSize,
            pSuffix));
}

void vboxVidPnDumpSourceMode(const char *pPrefix, const D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%sType(%s), ", pPrefix, vboxVidPnDumpStrSourceModeType(pVidPnSourceModeInfo->Type)));
    vboxVidPnDumpRegion("surf(", &pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize, "), ");
    vboxVidPnDumpRegion("vis(", &pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize, "), ");
    LOGREL_EXACT(("stride(%d), ", pVidPnSourceModeInfo->Format.Graphics.Stride));
    LOGREL_EXACT(("format(%s), ", vboxVidPnDumpStrPixFormat(pVidPnSourceModeInfo->Format.Graphics.PixelFormat)));
    LOGREL_EXACT(("clrBasis(%s), ", vboxVidPnDumpStrColorBasis(pVidPnSourceModeInfo->Format.Graphics.ColorBasis)));
    LOGREL_EXACT(("pvam(%s)%s", vboxVidPnDumpStrPvam(pVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode), pSuffix));
}

void vboxVidPnDumpSignalInfo(const char *pPrefix, const D3DKMDT_VIDEO_SIGNAL_INFO *pVideoSignalInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%sVStd(%s), ", pPrefix, vboxVidPnDumpStrSignalStandard(pVideoSignalInfo->VideoStandard)));
    vboxVidPnDumpRegion("totSize(", &pVideoSignalInfo->TotalSize, "), ");
    vboxVidPnDumpRegion("activeSize(", &pVideoSignalInfo->ActiveSize, "), ");
    vboxVidPnDumpRational("VSynch(", &pVideoSignalInfo->VSyncFreq, "), ");
    LOGREL_EXACT(("PixelRate(%d), ScanLineOrdering(%s)%s", pVideoSignalInfo->PixelRate, vboxVidPnDumpStrScanLineOrdering(pVideoSignalInfo->ScanLineOrdering), pSuffix));
}

void vboxVidPnDumpTargetMode(const char *pPrefix, const D3DKMDT_VIDPN_TARGET_MODE* CONST  pVidPnTargetModeInfo, const char *pSuffix)
{
    LOGREL_EXACT(("%s", pPrefix));
    vboxVidPnDumpSignalInfo("VSI: ", &pVidPnTargetModeInfo->VideoSignalInfo, ", ");
    LOGREL_EXACT(("Preference(%s)%s", vboxVidPnDumpStrModePreference(pVidPnTargetModeInfo->Preference), pSuffix));
}

void vboxVidPnDumpPinnedSourceMode(const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;

        Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_SUCCESS)
        {
            vboxVidPnDumpSourceMode("Source Pinned: ", pPinnedVidPnSourceModeInfo, "\n");
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            LOGREL_EXACT(("Source NOT Pinned\n"));
        }
        else
        {
            LOGREL_EXACT(("ERROR getting piined Source Mode(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting SourceModeSet(0x%x)\n", Status));
    }
}


DECLCALLBACK(BOOLEAN) vboxVidPnDumpSourceModeSetEnum(D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{
    vboxVidPnDumpSourceMode("SourceMode: ", pNewVidPnSourceModeInfo, "\n");
    return TRUE;
}

void vboxVidPnDumpSourceModeSet(PVBOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    LOGREL_EXACT(("  >>>+++SourceMode Set for Source(%d)+++\n", VidPnSourceId));
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {

        Status = vboxVidPnEnumSourceModes(hCurVidPnSourceModeSet, pCurVidPnSourceModeSetInterface,
                vboxVidPnDumpSourceModeSetEnum, NULL);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL_EXACT(("ERROR enumerating Source Modes(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting SourceModeSet for Source(%d), Status(0x%x)\n", VidPnSourceId, Status));
    }

    LOGREL_EXACT(("  <<<+++End Of SourceMode Set for Source(%d)+++", VidPnSourceId));
}

DECLCALLBACK(BOOLEAN) vboxVidPnDumpTargetModeSetEnum(D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext)
{
    vboxVidPnDumpTargetMode("TargetMode: ", pNewVidPnTargetModeInfo, "\n");
    return TRUE;
}

void vboxVidPnDumpTargetModeSet(PVBOXMP_DEVEXT pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    LOGREL_EXACT(("  >>>---TargetMode Set for Target(%d)---\n", VidPnTargetId));
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {

        Status = vboxVidPnEnumTargetModes(hCurVidPnTargetModeSet, pCurVidPnTargetModeSetInterface,
                vboxVidPnDumpTargetModeSetEnum, NULL);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL_EXACT(("ERROR enumerating Target Modes(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting TargetModeSet for Target(%d), Status(0x%x)\n", VidPnTargetId, Status));
    }

    LOGREL_EXACT(("  <<<---End Of TargetMode Set for Target(%d)---", VidPnTargetId));
}


void vboxVidPnDumpPinnedTargetMode(const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;

        Status = pCurVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_SUCCESS)
        {
            vboxVidPnDumpTargetMode("Target Pinned: ", pPinnedVidPnTargetModeInfo, "\n");
            pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            LOGREL_EXACT(("Target NOT Pinned\n"));
        }
        else
        {
            LOGREL_EXACT(("ERROR getting piined Target Mode(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    }
    else
    {
        LOGREL_EXACT(("ERROR getting TargetModeSet(0x%x)\n", Status));
    }
}

void vboxVidPnDumpCofuncModalityArg(const char *pPrefix, CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST  pEnumCofuncModalityArg, const char *pSuffix)
{
    LOGREL_EXACT(("%sPivotType(%s), SourceId(0x%x), TargetId(0x%x),%s", pPrefix, vboxVidPnDumpStrCFMPivotType(pEnumCofuncModalityArg->EnumPivotType),
            pEnumCofuncModalityArg->EnumPivot.VidPnSourceId, pEnumCofuncModalityArg->EnumPivot.VidPnTargetId, pSuffix));
}

void vboxVidPnDumpPath(const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo)
{
    LOGREL_EXACT((" >>**** Start Dump VidPn Path ****>>\n"));
    LOGREL_EXACT(("VidPnSourceId(%d),  VidPnTargetId(%d)\n",
            pVidPnPresentPathInfo->VidPnSourceId, pVidPnPresentPathInfo->VidPnTargetId));

    vboxVidPnDumpPinnedSourceMode(hVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnSourceId);
    vboxVidPnDumpPinnedTargetMode(hVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnTargetId);

    vboxVidPnDumpPathTransformation(&pVidPnPresentPathInfo->ContentTransformation);

    LOGREL_EXACT(("Importance(%s), TargetColorBasis(%s), Content(%s), ",
            vboxVidPnDumpStrImportance(pVidPnPresentPathInfo->ImportanceOrdinal),
            vboxVidPnDumpStrColorBasis(pVidPnPresentPathInfo->VidPnTargetColorBasis),
            vboxVidPnDumpStrContent(pVidPnPresentPathInfo->Content)));
    vboxVidPnDumpRegion("VFA_TL_O(", &pVidPnPresentPathInfo->VisibleFromActiveTLOffset, "), ");
    vboxVidPnDumpRegion("VFA_BR_O(", &pVidPnPresentPathInfo->VisibleFromActiveBROffset, "), ");
    vboxVidPnDumpRanges("CCDynamicRanges: ", &pVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges, "| ");
    vboxVidPnDumpCopyProtectoin("CProtection: ", &pVidPnPresentPathInfo->CopyProtection, "| ");
    vboxVidPnDumpGammaRamp("GammaRamp: ", &pVidPnPresentPathInfo->GammaRamp, "\n");

    LOGREL_EXACT((" <<**** Stop Dump VidPn Path ****<<"));
}

typedef struct VBOXVIDPNDUMPPATHENUM
{
    D3DKMDT_HVIDPN hVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface;
} VBOXVIDPNDUMPPATHENUM, *PVBOXVIDPNDUMPPATHENUM;

static DECLCALLBACK(BOOLEAN) vboxVidPnDumpPathEnum(D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo, PVOID pContext)
{
    PVBOXVIDPNDUMPPATHENUM pData = (PVBOXVIDPNDUMPPATHENUM)pContext;
    vboxVidPnDumpPath(pData->hVidPn, pData->pVidPnInterface, pVidPnPresentPathInfo);

    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathInfo);
    return TRUE;
}

void vboxVidPnDumpVidPn(const char * pPrefix, PVBOXMP_DEVEXT pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, const char * pSuffix)
{
    LOGREL_EXACT(("%s", pPrefix));

    VBOXVIDPNDUMPPATHENUM CbData;
    CbData.hVidPn = hVidPn;
    CbData.pVidPnInterface = pVidPnInterface;
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    NTSTATUS Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVidPnEnumPaths(hVidPnTopology, pVidPnTopologyInterface,
                                        vboxVidPnDumpPathEnum, &CbData);
        Assert(Status == STATUS_SUCCESS);
    }

    for (int i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        vboxVidPnDumpSourceModeSet(pDevExt, hVidPn, pVidPnInterface, (D3DDDI_VIDEO_PRESENT_SOURCE_ID)i);
        vboxVidPnDumpTargetModeSet(pDevExt, hVidPn, pVidPnInterface, (D3DDDI_VIDEO_PRESENT_TARGET_ID)i);
    }

    LOGREL_EXACT(("%s", pSuffix));
}
