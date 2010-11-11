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

NTSTATUS vboxVidPnCheckTopology(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn,
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        BOOLEAN *pbSupported)
{
    const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo = NULL;
    NTSTATUS Status = pVidPnTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
    BOOLEAN bSupported = TRUE;

    if (Status == STATUS_SUCCESS)
    {
        BOOLEAN bFoundPrimary = FALSE;

        while (1)
        {
            if (pNewVidPnPresentPathInfo->VidPnSourceId != pNewVidPnPresentPathInfo->VidPnTargetId)
            {
                dprintf(("unsupported source(%d)->target(%d) pare\n", pNewVidPnPresentPathInfo->VidPnSourceId, pNewVidPnPresentPathInfo->VidPnTargetId));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->VidPnSourceId == 0)
            {
                bFoundPrimary = TRUE;
            }

            /*
            ImportanceOrdinal does not matter for now
            pNewVidPnPresentPathInfo->ImportanceOrdinal
            */

            if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_UNPINNED
                    && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY
                    && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_CENTERED
                    && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_NOTSPECIFIED)
            {
                dprintf(("unsupported Scaling (%d)\n", pNewVidPnPresentPathInfo->ContentTransformation.Scaling));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Stretched)
            {
                dprintf(("unsupported Scaling support (Stretched)\n"));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (!pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Identity
                    && !pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered)
            {
                dprintf(("\"Identity\" or \"Centered\" Scaling support not set\n"));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_UNPINNED
                    && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY)
            {
                dprintf(("unsupported rotation (%d)\n", pNewVidPnPresentPathInfo->ContentTransformation.Rotation));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate180
                    || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate270
                    || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate90)
            {
                dprintf(("unsupported RotationSupport\n"));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (!pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Identity)
            {
                dprintf(("\"Identity\" RotationSupport not set\n"));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx
                    || pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy)
            {
                dprintf(("Non-zero TLOffset: cx(%d), cy(%d)\n",
                        pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx,
                        pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx
                    || pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy)
            {
                dprintf(("Non-zero TLOffset: cx(%d), cy(%d)\n",
                        pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx,
                        pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_SRGB
                    && pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_UNINITIALIZED)
            {
                dprintf(("unsupported VidPnTargetColorBasis (%d)\n", pNewVidPnPresentPathInfo->VidPnTargetColorBasis));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            /* channels?
            pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FirstChannel;
            pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.SecondChannel;
            pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.ThirdChannel;
            we definitely not support fourth channel
            */
            if (pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel)
            {
                dprintf(("Non-zero FourthChannel (%d)\n", pNewVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges.FourthChannel));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            /* Content (D3DKMDT_VPPC_GRAPHICS, _NOTSPECIFIED, _VIDEO), does not matter for now
            pNewVidPnPresentPathInfo->Content
            */
            /* not support copy protection for now */
            if (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_NOPROTECTION
                    && pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_UNINITIALIZED)
            {
                dprintf(("Copy protection not supported CopyProtectionType(%d)\n", pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits)
            {
                dprintf(("Copy protection not supported APSTriggerBits(%d)\n", pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION_SUPPORT tstCPSupport = {0};
            tstCPSupport.NoProtection = 1;
            if (memcmp(&tstCPSupport, &pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, sizeof(tstCPSupport)))
            {
                dprintf(("Copy protection support (0x%x)\n", *((UINT*)&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport)));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_DEFAULT
                    && pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_UNINITIALIZED)
            {
                dprintf(("Unsupported GammaRamp.Type (%d)\n", pNewVidPnPresentPathInfo->GammaRamp.Type));
                AssertBreakpoint();
                bSupported = FALSE;
                break;
            }

            if (pNewVidPnPresentPathInfo->GammaRamp.DataSize != 0)
            {
                dprintf(("Warning: non-zero GammaRamp.DataSize (%d), treating as supported\n", pNewVidPnPresentPathInfo->GammaRamp.DataSize));
            }

            const D3DKMDT_VIDPN_PRESENT_PATH *pNextVidPnPresentPathInfo;

            Status = pVidPnTopologyInterface->pfnAcquireNextPathInfo(hVidPnTopology, pNewVidPnPresentPathInfo, &pNextVidPnPresentPathInfo);
            pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);
            if (Status == STATUS_SUCCESS)
            {
                pNewVidPnPresentPathInfo = pNextVidPnPresentPathInfo;
            }
            else if (Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
            {
                Status = STATUS_SUCCESS;
                pNewVidPnPresentPathInfo = NULL;
                break;
            }
            else
            {
                AssertBreakpoint();
                dprintf(("pfnAcquireNextPathInfo Failed Status(0x%x)\n", Status));
                pNewVidPnPresentPathInfo = NULL;
                break;
            }
        }

        bSupported &= bFoundPrimary;

        if (pNewVidPnPresentPathInfo)
            pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);

    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        drprintf((__FUNCTION__": pfnAcquireFirstPathInfo failed Status(0x%x)\n", Status));

    *pbSupported = bSupported;

    return Status;
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
                dprintf(("VisibleRegionSize(%d, %d) !=  PrimSurfSize(%d, %d)\n",
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
            dprintf(("Warning: Unknown Src mode Type (%d)\n", pNewVidPnSourceModeInfo->Type));
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
                    drprintf(("pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
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
        drprintf(("VBoxVideoWddm: pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));

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
        drprintf((__FUNCTION__": vboxVidPnPopulateVideoSignalInfo error Status (0x%x)\n", Status));
        return Status;
    }

    if (!vboxVidPnMatchVideoSignal(&CmpVsi, &pNewVidPnTargetModeInfo->VideoSignalInfo))
    {
        dfprintf((__FUNCTION__": VideoSignalInfos do not match!!!\n"));
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
                    drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
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
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));

    *pbSupported = bSupported;
    return Status;
}

#if 0
DECLCALLBACK(BOOLEAN) vboxVidPnCofuncModalitySourceModeCheck(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPN_NEW_SRCMODESET_CHECK pCbContext = (PVBOXVIDPN_NEW_SRCMODESET_CHECK)pContext;
    pCbContext->CommonInfo.Status = STATUS_SUCCESS;

    pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);

    pCbContext->CommonInfo.Status = Status;
    return Status == STATUS_SUCCESS;
}

DECLCALLBACK(BOOLEAN) vboxVidPnCofuncModalitySourceModeEnum(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNCMCONTEXT pCbContext = (PVBOXVIDPNCMCONTEXT)pContext;
    pCbContext->Status = STATUS_SUCCESS;

    pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);

    pCbContext->Status = Status;
    return Status == STATUS_SUCCESS;
}

DECLCALLBACK(BOOLEAN) vboxVidPnCofuncModalityTargetModeEnum(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNCMCONTEXT pCbContext = (PVBOXVIDPNCMCONTEXT)pContext;
    pCbContext->Status = STATUS_SUCCESS;

    pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);

    pCbContext->Status = Status;
    return Status == STATUS_SUCCESS;
}
#endif

NTSTATUS vboxVidPnPopulateSourceModeInfoFromLegacy(PDEVICE_EXTENSION pDevExt,
        D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo,
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
            drprintf((__FUNCTION__": vboxWddmCalcPixelFormat failed\n"));
            Status = STATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        /* @todo: XPDM driver does not seem to return text modes, should we? */
        drprintf((__FUNCTION__": text mode not supported currently\n"));
        AssertBreakpoint();
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

NTSTATUS vboxVidPnPopulateSourceModeSetFromLegacy(PDEVICE_EXTENSION pDevExt,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet,
        const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface,
        VIDEO_MODE_INFORMATION *pModes,
        uint32_t cModes,
        int iPreferredMode,
        D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID *pPreferredModeId)
{
    NTSTATUS Status = STATUS_SUCCESS;
    D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID PreferredModeId = D3DDDI_ID_UNINITIALIZED;
    for (uint32_t i = 0; i < cModes; ++i)
    {
        D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
        /* disable 24 bpp for now */
        if (pModes[i].BitsPerPlane == 24)
            continue;

        Status = pNewVidPnSourceModeSetInterface->pfnCreateNewModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxVidPnPopulateSourceModeInfoFromLegacy(pDevExt, pNewVidPnSourceModeInfo, &pModes[i]);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID modeId = pNewVidPnSourceModeInfo->Id;
                Status = pNewVidPnSourceModeSetInterface->pfnAddMode(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    if (iPreferredMode == i)
                    {
                        PreferredModeId = modeId;
//                        AssertBreakpoint();
//                        Status = pNewVidPnSourceModeSetInterface->pfnPinMode(hNewVidPnSourceModeSet, modeId);
//                        Assert(Status == STATUS_SUCCESS);
//                        if (Status != STATUS_SUCCESS)
//                        {
//                            drprintf((__FUNCTION__": pfnPinMode failed, Status(0x%x)", Status));
//                            /* don't treat it as fatal */
//                            Status = STATUS_SUCCESS;
//                        }
                    }
                }
                else
                {
                    drprintf((__FUNCTION__": pfnAddMode failed, Status(0x%x)", Status));
                    pNewVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                    break;
                }
            }
            else
            {
                drprintf((__FUNCTION__": pfnCreateNewModeInfo failed, Status(0x%x)", Status));
                pNewVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                break;
            }
        }
    }

    if (pPreferredModeId)
        *pPreferredModeId = PreferredModeId;

    return Status;
}

NTSTATUS vboxVidPnPopulateMonitorSourceModeInfoFromLegacy(PDEVICE_EXTENSION pDevExt,
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

NTSTATUS vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy(PDEVICE_EXTENSION pDevExt,
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
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                    break;
                drprintf((__FUNCTION__": pfnAddMode failed, Status(0x%x)", Status));
            }
            else
                drprintf((__FUNCTION__": vboxVidPnPopulateMonitorSourceModeInfoFromLegacy failed, Status(0x%x)", Status));

            Assert (Status != STATUS_SUCCESS);
            /* we're here because of a failure */
            NTSTATUS tmpStatus = pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pMonitorSMI);
            Assert(tmpStatus == STATUS_SUCCESS);
            if (tmpStatus != STATUS_SUCCESS)
                drprintf((__FUNCTION__": pfnReleaseModeInfo failed tmpStatus(0x%x)\n", tmpStatus));
        } while (0);
    }
    else
        drprintf((__FUNCTION__": pfnCreateNewModeInfo failed, Status(0x%x)", Status));

    return Status;
}

NTSTATUS vboxVidPnPopulateTargetModeInfoFromLegacy(D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo,
        D3DKMDT_2DREGION *pResolution,
        BOOLEAN bPreferred)
{
    Assert(!bPreferred);
    pNewVidPnTargetModeInfo->Preference = bPreferred ? D3DKMDT_MP_PREFERRED : D3DKMDT_MP_NOTPREFERRED;

    return vboxVidPnPopulateVideoSignalInfo(&pNewVidPnTargetModeInfo->VideoSignalInfo, pResolution, 60 /* ULONG VSync */);
}

#define VBOXVIDPN_MODESET_NO_PIN_PREFERRED  0x00000001
#define VBOXVIDPN_MODESET_MARK_PREFERRED    0x00000002

NTSTATUS vboxVidPnPopulateTargetModeSetFromLegacy(PDEVICE_EXTENSION pDevExt,
        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet,
        const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface,
        D3DKMDT_2DREGION *pResolutions,
        uint32_t cResolutions,
        VIDEO_MODE_INFORMATION *pPreferredMode,
        uint32_t fFlags,
        D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID *pPreferredModeId)
{
    NTSTATUS Status = STATUS_SUCCESS;
    D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID PreferredModeId = D3DDDI_ID_UNINITIALIZED;
    for (uint32_t i = 0; i < cResolutions; ++i)
    {
        D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
        Status = pNewVidPnTargetModeSetInterface->pfnCreateNewModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            bool bPreferred = pPreferredMode ? pPreferredMode->VisScreenWidth == pResolutions[i].cx
                    && pPreferredMode->VisScreenHeight == pResolutions[i].cy : FALSE;
            Status = vboxVidPnPopulateTargetModeInfoFromLegacy(pNewVidPnTargetModeInfo, &pResolutions[i], bPreferred && (fFlags & VBOXVIDPN_MODESET_MARK_PREFERRED));
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID modeId = pNewVidPnTargetModeInfo->Id;
                Status = pNewVidPnTargetModeSetInterface->pfnAddMode(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    if (bPreferred) // && !(fFlags & VBOXVIDPN_MODESET_NO_PIN_PREFERRED))
                    {
                        PreferredModeId = modeId;
//                        AssertBreakpoint();
//                        Status = pNewVidPnTargetModeSetInterface->pfnPinMode(hNewVidPnTargetModeSet, modeId);
//                        Assert(Status == STATUS_SUCCESS);
//                        if (Status != STATUS_SUCCESS)
//                        {
//                            drprintf((__FUNCTION__": pfnPinMode failed, Status(0x%x)", Status));
//                            /* don't treat it as fatal */
//                            Status = STATUS_SUCCESS;
//                        }
                    }
                }
                else
                {
                    drprintf((__FUNCTION__": pfnAddMode failed, Status(0x%x)", Status));
                    pNewVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                    break;
                }
            }
            else
            {
                drprintf((__FUNCTION__": pfnCreateNewModeInfo failed, Status(0x%x)", Status));
                pNewVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                break;
            }
        }
    }

    if (pPreferredModeId)
        *pPreferredModeId = PreferredModeId;
    return Status;
}

NTSTATUS vboxVidPnCreatePopulateSourceModeSetFromLegacy(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
                    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId,
                    VIDEO_MODE_INFORMATION *pModes, uint32_t cModes, int iPreferredMode, D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID *pPreferredModeId)
{
    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface;
    NTSTATUS Status = pVidPnInterface->pfnCreateNewSourceModeSet(hDesiredVidPn,
                        srcId, /*__in CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId */
                        &hNewVidPnSourceModeSet,
                        &pNewVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVidPnPopulateSourceModeSetFromLegacy(pDevExt,
                    hNewVidPnSourceModeSet, pNewVidPnSourceModeSetInterface,
                    pModes, cModes, iPreferredMode, pPreferredModeId);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = pVidPnInterface->pfnAssignSourceModeSet(hDesiredVidPn,
                        srcId,
                        hNewVidPnSourceModeSet);
            Assert(Status == STATUS_SUCCESS);
            if(Status != STATUS_SUCCESS)
            {
                drprintf((__FUNCTION__": pfnAssignSourceModeSet failed Status(0x%x)\n", Status));
                pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hNewVidPnSourceModeSet);
            }
        }
        else
        {
            drprintf((__FUNCTION__": vboxVidPnPopulateSourceModeSetFromLegacy failed Status(0x%x)\n", Status));
            pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hNewVidPnSourceModeSet);
        }
    }
    else
        drprintf((__FUNCTION__": pfnCreateNewSourceModeSet failed Status(0x%x)\n", Status));
    return Status;
}

NTSTATUS vboxVidPnCreatePopulateTargetModeSetFromLegacy(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
                    D3DDDI_VIDEO_PRESENT_TARGET_ID tgtId,
                    D3DKMDT_2DREGION *pResolutions,
                    uint32_t cResolutions,
                    VIDEO_MODE_INFORMATION *pPreferredMode, uint32_t fFlags,
                    D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID *pPreferredModeId)
{
    D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface;
    NTSTATUS Status = pVidPnInterface->pfnCreateNewTargetModeSet(hDesiredVidPn,
                        tgtId, /*__in CONST D3DDDI_VIDEO_PRESENT_TARGET_ID  VidPnTargetId */
                        &hNewVidPnTargetModeSet,
                        &pNewVidPnTargetModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVidPnPopulateTargetModeSetFromLegacy(pDevExt,
                    hNewVidPnTargetModeSet, pNewVidPnTargetModeSetInterface,
                    pResolutions, cResolutions, pPreferredMode, fFlags, pPreferredModeId);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = pVidPnInterface->pfnAssignTargetModeSet(hDesiredVidPn,
                        tgtId,
                        hNewVidPnTargetModeSet);
            Assert(Status == STATUS_SUCCESS);
            if(Status != STATUS_SUCCESS)
            {
                drprintf((__FUNCTION__": pfnAssignTargetModeSet failed Status(0x%x)\n", Status));
                pVidPnInterface->pfnReleaseTargetModeSet(hDesiredVidPn, hNewVidPnTargetModeSet);
            }
        }
        else
        {
            drprintf((__FUNCTION__": vboxVidPnPopulateTargetModeSetFromLegacy failed Status(0x%x)\n", Status));
            pVidPnInterface->pfnReleaseTargetModeSet(hDesiredVidPn, hNewVidPnTargetModeSet);
        }
    }
    else
        drprintf((__FUNCTION__": pfnCreateNewTargetModeSet failed Status(0x%x)\n", Status));
    return Status;

}

typedef struct VBOXVIDPNCHECKADDMONITORMODES
{
    NTSTATUS Status;
    D3DKMDT_2DREGION *pResolutions;
    uint32_t cResolutions;
} VBOXVIDPNCHECKADDMONITORMODES, *PVBOXVIDPNCHECKADDMONITORMODES;

static DECLCALLBACK(BOOLEAN) vboxVidPnCheckAddMonitorModesEnum(struct _DEVICE_EXTENSION* pDevExt, D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
        CONST D3DKMDT_MONITOR_SOURCE_MODE *pMonitorSMI, PVOID pContext)
{
    PVBOXVIDPNCHECKADDMONITORMODES pData = (PVBOXVIDPNCHECKADDMONITORMODES)pContext;
    NTSTATUS Status = STATUS_SUCCESS;

    for (uint32_t i = 0; i < pData->cResolutions; ++i)
    {
        D3DKMDT_VIDPN_TARGET_MODE dummyMode = {0};
        Status = vboxVidPnPopulateTargetModeInfoFromLegacy(&dummyMode, &pData->pResolutions[i], FALSE);
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
            drprintf((__FUNCTION__": vboxVidPnPopulateTargetModeInfoFromLegacy failed Status(0x%x)\n", Status));
            break;
        }
    }

    pMonitorSMSIf->pfnReleaseModeInfo(hMonitorSMS, pMonitorSMI);

    pData->Status = Status;

    return Status == STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCheckAddMonitorModes(PDEVICE_EXTENSION pDevExt,
        D3DDDI_VIDEO_PRESENT_TARGET_ID targetId, D3DKMDT_MONITOR_CAPABILITIES_ORIGIN enmOrigin,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions, int32_t iPreferred)
{
    NTSTATUS Status;
#if 0
    D3DKMDT_2DREGION *pResolutionsCopy = (D3DKMDT_2DREGION*)vboxWddmMemAlloc(cResolutions * sizeof (D3DKMDT_2DREGION));
    if (pResolutionsCopy)
    {
        memcpy(pResolutionsCopy, pResolutions, cResolutions * sizeof (D3DKMDT_2DREGION));
#endif
        CONST DXGK_MONITOR_INTERFACE *pMonitorInterface;
        Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryMonitorInterface(pDevExt->u.primary.DxgkInterface.DeviceHandle, DXGK_MONITOR_INTERFACE_VERSION_V1, &pMonitorInterface);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS;
            CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf;
            Status = pMonitorInterface->pfnAcquireMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle,
                                            targetId,
                                            &hMonitorSMS,
                                            &pMonitorSMSIf);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
#if 0
                VBOXVIDPNCHECKADDMONITORMODES EnumData = {0};
                EnumData.cResolutions = cResolutions;
                EnumData.pResolutions = pResolutionsCopy;
                Status = vboxVidPnEnumMonitorSourceModes(pDevExt, hMonitorSMS, pMonitorSMSIf,
                        vboxVidPnCheckAddMonitorModesEnum, &EnumData);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    Assert(EnumData.Status == STATUS_SUCCESS);
                    if (EnumData.Status == STATUS_SUCCESS)
                    {
#endif
                        for (uint32_t i = 0; i < cResolutions; ++i)
                        {
#if 0
                            D3DKMDT_2DREGION *pRes = &pResolutionsCopy[i];
#else
                            D3DKMDT_2DREGION *pRes = &pResolutions[i];
#endif
                            if (pRes->cx)
                            {
                                Status = vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy(pDevExt,
                                        hMonitorSMS,
                                        pMonitorSMSIf,
                                        pRes,
                                        enmOrigin,
                                        i == (uint32_t)iPreferred);
                                Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET);
                                if (Status == STATUS_GRAPHICS_MODE_ALREADY_IN_MODESET)
                                {
                                    Status = STATUS_SUCCESS;
                                }
                                else if (Status != STATUS_SUCCESS)
                                {
                                    drprintf((__FUNCTION__": vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy failed Status(0x%x)\n", Status));
                                    break;
                                }
                            }
                        }
#if 0
                    }
                }
#endif
                NTSTATUS tmpStatus = pMonitorInterface->pfnReleaseMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle, hMonitorSMS);
                Assert(tmpStatus == STATUS_SUCCESS);
                if (tmpStatus != STATUS_SUCCESS)
                    drprintf((__FUNCTION__": pfnReleaseMonitorSourceModeSet failed tmpStatus(0x%x)\n", tmpStatus));
            }
            else
                drprintf((__FUNCTION__": pfnAcquireMonitorSourceModeSet failed Status(0x%x)\n", Status));
        }
        else
            drprintf((__FUNCTION__": DxgkCbQueryMonitorInterface failed Status(0x%x)\n", Status));
#if 0
        vboxWddmMemFree(pResolutionsCopy);
    }
    else
    {
        drprintf((__FUNCTION__": failed to allocate resolution copy of size (%d)\n", cResolutions));
        Status = STATUS_NO_MEMORY;
    }
#endif

    return Status;
}

NTSTATUS vboxVidPnCreatePopulateVidPnFromLegacy(PDEVICE_EXTENSION pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        VIDEO_MODE_INFORMATION *pModes, uint32_t cModes, int iPreferredMode,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions,
        const D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, const D3DDDI_VIDEO_PRESENT_TARGET_ID tgtId)
{
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    VIDEO_MODE_INFORMATION *pPreferredMode = iPreferredMode >= 0 ? &pModes[iPreferredMode] : NULL;
    D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID PreferredSrcModeId = D3DDDI_ID_UNINITIALIZED;
    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnCreateNewSourceModeSet(hVidPn,
                        srcId, /*__in CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId */
                        &hNewVidPnSourceModeSet,
                        &pNewVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVidPnPopulateSourceModeSetFromLegacy(pDevExt,
                    hNewVidPnSourceModeSet, pNewVidPnSourceModeSetInterface,
                    pModes, cModes, iPreferredMode, &PreferredSrcModeId);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = pVidPnInterface->pfnAssignSourceModeSet(hVidPn,
                        srcId,
                        hNewVidPnSourceModeSet);
            Assert(Status == STATUS_SUCCESS);
            if(Status == STATUS_SUCCESS)
            {
                Assert(PreferredSrcModeId != D3DDDI_ID_UNINITIALIZED);
                D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID PreferredTrgModeId = D3DDDI_ID_UNINITIALIZED;
                D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet;
                const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface;
                NTSTATUS Status = pVidPnInterface->pfnCreateNewTargetModeSet(hVidPn,
                                    tgtId, /*__in CONST D3DDDI_VIDEO_PRESENT_TARGET_ID  VidPnTargetId */
                                    &hNewVidPnTargetModeSet,
                                    &pNewVidPnTargetModeSetInterface);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    Status = vboxVidPnPopulateTargetModeSetFromLegacy(pDevExt,
                                hNewVidPnTargetModeSet, pNewVidPnTargetModeSetInterface,
                                pResolutions, cResolutions, pPreferredMode, 0, &PreferredTrgModeId);
                    Assert(Status == STATUS_SUCCESS);
                    if (Status == STATUS_SUCCESS)
                    {
                        Status = pVidPnInterface->pfnAssignTargetModeSet(hVidPn,
                                    tgtId,
                                    hNewVidPnTargetModeSet);
                        Assert(Status == STATUS_SUCCESS);
                        if(Status == STATUS_SUCCESS)
                        {

                            Assert(PreferredTrgModeId != D3DDDI_ID_UNINITIALIZED);
                            Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
                            if (Status == STATUS_SUCCESS)
                            {
                                D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo;
                                Status = pVidPnTopologyInterface->pfnCreateNewPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
                                if (Status == STATUS_SUCCESS)
                                {
                                    pNewVidPnPresentPathInfo->VidPnSourceId = srcId;
                                    pNewVidPnPresentPathInfo->VidPnTargetId = tgtId;
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
                                    Assert(Status == STATUS_SUCCESS);
                                    if (Status == STATUS_SUCCESS)
                                    {
                                        if (PreferredSrcModeId != D3DDDI_ID_UNINITIALIZED)
                                        {
                                            Status = pNewVidPnSourceModeSetInterface->pfnPinMode(hNewVidPnSourceModeSet, PreferredSrcModeId);
                                            Assert(Status == STATUS_SUCCESS);
                                            if (Status == STATUS_SUCCESS)
                                            {
                                                Status = pNewVidPnTargetModeSetInterface->pfnPinMode(hNewVidPnTargetModeSet, PreferredTrgModeId);
                                                Assert(Status == STATUS_SUCCESS);
                                                if (Status != STATUS_SUCCESS)
                                                    drprintf((__FUNCTION__": TRG pfnPinMode failed Status(0x%x)\n", Status));
                                            }
                                            else
                                                drprintf((__FUNCTION__": SRC pfnPinMode failed Status(0x%x)\n", Status));
                                        }
                                    }
                                    else
                                    {
                                        drprintf((__FUNCTION__": pfnAddPath failed Status(0x%x)\n", Status));
                                        pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);
                                        pNewVidPnPresentPathInfo = NULL;
                                    }
                                }
                                else
                                    drprintf((__FUNCTION__": pfnCreateNewPathInfo failed Status(0x%x)\n", Status));
                            }
                            else
                                drprintf((__FUNCTION__": pfnGetTopology failed Status(0x%x)\n", Status));
                        }
                        else
                        {
                            drprintf((__FUNCTION__": pfnAssignTargetModeSet failed Status(0x%x)\n", Status));
                            pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hNewVidPnTargetModeSet);
                        }
                    }
                    else
                    {
                        drprintf((__FUNCTION__": vboxVidPnPopulateTargetModeSetFromLegacy failed Status(0x%x)\n", Status));
                        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hNewVidPnTargetModeSet);
                    }
                }
                else
                    drprintf((__FUNCTION__": pfnCreateNewTargetModeSet failed Status(0x%x)\n", Status));
            }
            else
            {
                drprintf((__FUNCTION__": pfnAssignSourceModeSet failed Status(0x%x)\n", Status));
                pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hNewVidPnSourceModeSet);
            }
        }
        else
        {
            drprintf((__FUNCTION__": vboxVidPnPopulateSourceModeSetFromLegacy failed Status(0x%x)\n", Status));
            pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hNewVidPnSourceModeSet);
        }
    }
    else
        drprintf((__FUNCTION__": pfnCreateNewSourceModeSet failed Status(0x%x)\n", Status));

    return Status;
}

DECLCALLBACK(BOOLEAN) vboxVidPnCofuncModalityPathEnum(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo, PVOID pContext)
{
    PVBOXVIDPNCOFUNCMODALITY pCbContext = (PVBOXVIDPNCOFUNCMODALITY)pContext;
    NTSTATUS Status = STATUS_SUCCESS;
    pCbContext->Status = STATUS_SUCCESS;
    PVBOXWDDM_VIDEOMODES_INFO pInfo = &pCbContext->pInfos[pNewVidPnPresentPathInfo->VidPnTargetId];
    VIDEO_MODE_INFORMATION *pModes = pInfo->aModes;
    uint32_t cModes = pInfo->cModes;
    /* we do not want the mode to be pinned */
    int iPreferredMode = -1; /* pInfo->iPreferredMode; */
    uint32_t cResolutions = pInfo->cResolutions;
    D3DKMDT_2DREGION * pResolutions = pInfo->aResolutions;


    /* adjust scaling */
    if (pCbContext->pEnumCofuncModalityArg->EnumPivotType != D3DKMDT_EPT_SCALING)
    {
        if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY
                && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_CENTERED
                && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_STRETCHED
                && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_UNPINNED
                && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_NOTSPECIFIED)
        {
            AssertBreakpoint();
            /* todo: create a new path (if not done already) and assign a proper info */
        }
    }

    /* adjust rotation */
    if (pCbContext->pEnumCofuncModalityArg->EnumPivotType != D3DKMDT_EPT_ROTATION)
    {
        if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE90
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE180
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE270
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_UNPINNED)
        {
            AssertBreakpoint();
            /* todo: create a new path (if not done already) and assign a proper info */
        }
    }

    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;
    VIDEO_MODE_INFORMATION *pPreferredMode = iPreferredMode >= 0 ? &pModes[iPreferredMode] : NULL;

    Status = pVidPnInterface->pfnAcquireSourceModeSet(hDesiredVidPn,
                pNewVidPnPresentPathInfo->VidPnSourceId,
                &hCurVidPnSourceModeSet,
                &pCurVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        CONST D3DKMDT_VIDPN_SOURCE_MODE* pPinnedVidPnSourceModeInfo;
        Status = pCurVidPnSourceModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnSourceModeSet, &pPinnedVidPnSourceModeInfo);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
        if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            pPinnedVidPnSourceModeInfo = NULL;
            Status = STATUS_SUCCESS;
        }
        else if (Status != STATUS_SUCCESS)
            drprintf((__FUNCTION__": pfnAcquirePinnedModeInfo failed Status(0x%x)\n", Status));

        D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
        const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;
        Status = pVidPnInterface->pfnAcquireTargetModeSet(hDesiredVidPn,
                            pNewVidPnPresentPathInfo->VidPnTargetId,
                            &hCurVidPnTargetModeSet,
                            &pCurVidPnTargetModeSetInterface);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            CONST D3DKMDT_VIDPN_TARGET_MODE* pPinnedVidPnTargetModeInfo;
            Status = pCurVidPnTargetModeSetInterface->pfnAcquirePinnedModeInfo(hCurVidPnTargetModeSet, &pPinnedVidPnTargetModeInfo);
            Assert(Status == STATUS_SUCCESS || Status == STATUS_GRAPHICS_MODE_NOT_PINNED);
            if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
            {
                pPinnedVidPnTargetModeInfo = NULL;
                Status = STATUS_SUCCESS;
            }
            else if (Status != STATUS_SUCCESS)
                drprintf((__FUNCTION__": pfnAcquirePinnedModeInfo failed Status(0x%x)\n", Status));

            switch (pCbContext->pEnumCofuncModalityArg->EnumPivotType)
            {
                case D3DKMDT_EPT_VIDPNSOURCE:
                    if (!pPinnedVidPnTargetModeInfo)
                    {
                        if (pPinnedVidPnSourceModeInfo)
                        {
                            SIZE_T cModes;
                            Status = pCurVidPnTargetModeSetInterface->pfnGetNumModes(hCurVidPnTargetModeSet, &cModes);
                            Assert(Status == STATUS_SUCCESS);
                            if (Status == STATUS_SUCCESS)
                            {
                                D3DKMDT_2DREGION Resolution;
                                Resolution.cx = pPinnedVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cx;
                                Resolution.cy = pPinnedVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize.cy;
                                BOOLEAN bCreateTrg = FALSE;
                                if (cModes == 1)
                                {
                                    const D3DKMDT_VIDPN_TARGET_MODE *pVidPnTargetModeInfo;
                                    Status = pCurVidPnTargetModeSetInterface->pfnAcquireFirstModeInfo(hCurVidPnTargetModeSet, &pVidPnTargetModeInfo);
                                    Assert(Status == STATUS_SUCCESS);
                                    if (Status == STATUS_SUCCESS)
                                    {
                                        D3DKMDT_VIDPN_TARGET_MODE dummyMode = {0};
                                        Status = vboxVidPnPopulateTargetModeInfoFromLegacy(&dummyMode, &Resolution, FALSE);
                                        Assert(Status == STATUS_SUCCESS);
                                        if (Status == STATUS_SUCCESS)
                                        {
                                            if (!vboxVidPnMatchVideoSignal(&dummyMode.VideoSignalInfo, &pVidPnTargetModeInfo->VideoSignalInfo))
                                                bCreateTrg = TRUE;
                                            else
                                            {
                                                /* do we need to check pVidPnTargetModeInfo->Preference; ? */
                                            }
                                        }
                                        else
                                        {
                                            drprintf((__FUNCTION__": trg vboxVidPnPopulateTargetModeInfoFromLegacy failed Status(0x%x), pretending success\n", Status));
                                            Status = STATUS_SUCCESS;
                                            bCreateTrg = TRUE;
                                        }
                                    }
                                    else
                                        drprintf((__FUNCTION__": trg pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));
                                }
                                else
                                    bCreateTrg = TRUE;

                                if (bCreateTrg)
                                {
                                    Status = vboxVidPnCreatePopulateTargetModeSetFromLegacy(pDevExt, hDesiredVidPn, pVidPnInterface,
                                            pNewVidPnPresentPathInfo->VidPnTargetId,
                                            &Resolution,
                                            1,
                                            pPreferredMode,
                                            0,
                                            NULL);
                                    Assert(Status == STATUS_SUCCESS);
                                    if (Status != STATUS_SUCCESS)
                                        drprintf((__FUNCTION__": vboxVidPnCreatePopulateTargetModeSetFromLegacy for pinned source failed Status(0x%x)\n", Status));                                }
                            }
                            else
                                drprintf((__FUNCTION__": trg pfnGetNumModes failed Status(0x%x)\n", Status));
                        }
                        else
                        {
                            dprintf((__FUNCTION__": source pivot w/o pinned source mode\n"));
//                            AssertBreakpoint();
                        }
                    }
                    break;
                case D3DKMDT_EPT_VIDPNTARGET:
                    break;
                case D3DKMDT_EPT_SCALING:
                    break;
                case D3DKMDT_EPT_ROTATION:
                    break;
                case D3DKMDT_EPT_NOPIVOT:
                    Assert(!!pPinnedVidPnSourceModeInfo == !!pPinnedVidPnTargetModeInfo);
                    if (!pPinnedVidPnSourceModeInfo && !pPinnedVidPnTargetModeInfo)
                    {
                        /* just create and populate the new source mode set for now */
                        Status = vboxVidPnCreatePopulateSourceModeSetFromLegacy(pDevExt, hDesiredVidPn, pVidPnInterface,
                                pNewVidPnPresentPathInfo->VidPnSourceId,
                                pModes, cModes, iPreferredMode, NULL);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status == STATUS_SUCCESS)
                        {
                            /* just create and populate a new target mode info for now */
                            Status = vboxVidPnCreatePopulateTargetModeSetFromLegacy(pDevExt, hDesiredVidPn, pVidPnInterface,
                                    pNewVidPnPresentPathInfo->VidPnTargetId,
                                    pResolutions,
                                    cResolutions,
                                    pPreferredMode,
                                    0,
                                    NULL);
                            Assert(Status == STATUS_SUCCESS);
                            if (Status != STATUS_SUCCESS)
                                drprintf((__FUNCTION__": vboxVidPnCreatePopulateTargetModeSetFromLegacy failed Status(0x%x)\n", Status));
                        }
                        else
                            drprintf((__FUNCTION__": vboxVidPnCreatePopulateSourceModeSetFromLegacy failed Status(0x%x)\n", Status));
                    }
                    break;
                default:
                    drprintf((__FUNCTION__": unknown EnumPivotType (%dx)\n", pCbContext->pEnumCofuncModalityArg->EnumPivotType));
                    break;
            }

            if (pPinnedVidPnTargetModeInfo)
                pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
            pVidPnInterface->pfnReleaseTargetModeSet(hDesiredVidPn, hCurVidPnTargetModeSet);
        }
        else
            drprintf((__FUNCTION__": pfnAcquireTargetModeSet failed Status(0x%x)\n", Status));

        if (pPinnedVidPnSourceModeInfo)
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hCurVidPnSourceModeSet);
    }
    else
        drprintf((__FUNCTION__": pfnAcquireSourceModeSet failed Status(0x%x)\n", Status));

    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);

    pCbContext->Status = Status;
    Assert(Status == STATUS_SUCCESS);
    return Status == STATUS_SUCCESS;
}

NTSTATUS vboxVidPnEnumMonitorSourceModes(PDEVICE_EXTENSION pDevExt, D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS, CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf,
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
            if (!pfnCallback(pDevExt, hMonitorSMS, pMonitorSMSIf, pMonitorSMI, pContext))
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
                    drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
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
                drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
                pNextMonitorSMI = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumSourceModes(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
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
            if (!pfnCallback(pDevExt, hDesiredVidPn, pVidPnInterface,
                    hNewVidPnSourceModeSet, pVidPnSourceModeSetInterface,
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
                    drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
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
                drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
                pNewVidPnSourceModeInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumTargetModes(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
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
            if (!pfnCallback(pDevExt, hDesiredVidPn, pVidPnInterface,
                    hNewVidPnTargetModeSet, pVidPnTargetModeSetInterface,
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
                    drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
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
                drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
                pNewVidPnTargetModeInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumTargetsForSource(PDEVICE_EXTENSION pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
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
                drprintf((__FUNCTION__": pfnEnumPathTargetsFromSource failed Status(0x%x)\n", Status));
                break;
            }
        }
    }
    else if (Status != STATUS_GRAPHICS_SOURCE_NOT_IN_TOPOLOGY)
        drprintf((__FUNCTION__": pfnGetNumPathsFromSource failed Status(0x%x)\n", Status));

    return Status;
}

NTSTATUS vboxVidPnEnumPaths(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
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

            if (!pfnCallback(pDevExt, hDesiredVidPn, pVidPnInterface, hVidPnTopology, pVidPnTopologyInterface, pNewVidPnPresentPathInfo, pContext))
            {
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNextVidPnPresentPathInfo);
                else
                {
                    Assert(Status == STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET);
                    if (Status != STATUS_GRAPHICS_NO_MORE_ELEMENTS_IN_DATASET)
                        drprintf((__FUNCTION__": pfnAcquireNextPathInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
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
                drprintf((__FUNCTION__": pfnAcquireNextPathInfo Failed Status(0x%x)\n", Status));
                pNewVidPnPresentPathInfo = NULL;
                break;
            }
        }
    }
    else if (Status == STATUS_GRAPHICS_DATASET_IS_EMPTY)
        Status = STATUS_SUCCESS;
    else
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));

    return Status;
}

NTSTATUS vboxVidPnSetupSourceInfo(struct _DEVICE_EXTENSION* pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, PVBOXWDDM_SOURCE pSource, CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, PVBOXWDDM_ALLOCATION pAllocation)
{
    vboxWddmAssignPrimary(pDevExt, pSource, pAllocation, srcId);
    return STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCommitSourceMode(struct _DEVICE_EXTENSION* pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, CONST D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, PVBOXWDDM_ALLOCATION pAllocation)
{
    Assert(srcId < (UINT)commonFromDeviceExt(pDevExt)->cDisplays);
    if (srcId < (UINT)commonFromDeviceExt(pDevExt)->cDisplays)
    {
        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[srcId];
        return vboxVidPnSetupSourceInfo(pDevExt, srcId, pSource, pVidPnSourceModeInfo, pAllocation);
    }

    drprintf((__FUNCTION__": invalid srcId (%d), cSources(%d)\n", srcId, commonFromDeviceExt(pDevExt)->cDisplays));
    return STATUS_INVALID_PARAMETER;
}

typedef struct VBOXVIDPNCOMMITTARGETMODE
{
    NTSTATUS Status;
    D3DKMDT_HVIDPN hVidPn;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface;
} VBOXVIDPNCOMMITTARGETMODE;

DECLCALLBACK(BOOLEAN) vboxVidPnCommitTargetModeEnum(PDEVICE_EXTENSION pDevExt, D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId, SIZE_T cTgtPaths, PVOID pContext)
{
    VBOXVIDPNCOMMITTARGETMODE *pInfo = (VBOXVIDPNCOMMITTARGETMODE*)pContext;
    Assert(cTgtPaths <= (SIZE_T)commonFromDeviceExt(pDevExt)->cDisplays);
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
        drprintf((__FUNCTION__": pfnAcquireTargetModeSet failed Status(0x%x)\n", Status));

    pInfo->Status = Status;
    return Status == STATUS_SUCCESS;
}

NTSTATUS vboxVidPnCommitSourceModeForSrcId(struct _DEVICE_EXTENSION* pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, PVBOXWDDM_ALLOCATION pAllocation)
{
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

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
                        drprintf((__FUNCTION__": vboxVidPnEnumTargetsForSource failed Status(0x%x)\n", Status));
                }
                else
                    drprintf((__FUNCTION__": pfnGetTopology failed Status(0x%x)\n", Status));
            }
            else
                drprintf((__FUNCTION__": vboxVidPnCommitSourceMode failed Status(0x%x)\n", Status));
            /* release */
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
        {
            Status = vboxVidPnCommitSourceMode(pDevExt, srcId, NULL, pAllocation);
            Assert(Status == STATUS_SUCCESS);
        }
        else
            drprintf((__FUNCTION__": pfnAcquirePinnedModeInfo failed Status(0x%x)\n", Status));

        pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        drprintf((__FUNCTION__": pfnAcquireSourceModeSet failed Status(0x%x)\n", Status));
    }

    return Status;
}

DECLCALLBACK(BOOLEAN) vboxVidPnCommitPathEnum(struct _DEVICE_EXTENSION* pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo, PVOID pContext)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVIDPNCOMMIT pCommitInfo = (PVBOXVIDPNCOMMIT)pContext;

    if (pCommitInfo->pCommitVidPnArg->AffectedVidPnSourceId == D3DDDI_ID_ALL
            || pCommitInfo->pCommitVidPnArg->AffectedVidPnSourceId == pVidPnPresentPathInfo->VidPnSourceId)
    {
        Status = vboxVidPnCommitSourceModeForSrcId(pDevExt, hDesiredVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnSourceId, (PVBOXWDDM_ALLOCATION)pCommitInfo->pCommitVidPnArg->hPrimaryAllocation);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
            drprintf((__FUNCTION__": vboxVidPnCommitSourceModeForSrcId failed Status(0x%x)\n", Status));
    }

    pCommitInfo->Status = Status;
    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathInfo);
    return Status == STATUS_SUCCESS;
}

#define VBOXVIDPNDUMP_STRCASE(_t) \
        case _t: return #_t;
#define VBOXVIDPNDUMP_STRCASE_UNKNOWN() \
        default: return "Unknown";

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

void vboxVidPnDumpCopyProtectoin(const D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION *pCopyProtection)
{
    drprintf(("CopyProtection: CopyProtectionType(%s),  TODO: Dump All the rest\n",
            vboxVidPnDumpStrCopyProtectionType(pCopyProtection->CopyProtectionType)));
}


void vboxVidPnDumpPathTransformation(const D3DKMDT_VIDPN_PRESENT_PATH_TRANSFORMATION *pContentTransformation)
{
    drprintf(("Transformation: Scaling(%s),  ScalingSupport(%d), Rotation(%s), RotationSupport(%d)\n",
            vboxVidPnDumpStrScaling(pContentTransformation->Scaling), pContentTransformation->ScalingSupport,
            vboxVidPnDumpStrRotation(pContentTransformation->Rotation), pContentTransformation->RotationSupport));
}

void vboxVidPnDumpRegion(const char *pPrefix, const D3DKMDT_2DREGION *pRegion, const char *pSuffix)
{
    drprintf(("%s%dX%d%s", pPrefix, pRegion->cx, pRegion->cy, pSuffix));
}

void vboxVidPnDumpRational(const char *pPrefix, const D3DDDI_RATIONAL *pRational, const char *pSuffix)
{
    drprintf(("%s%d/%d=%d%s", pPrefix, pRational->Numerator, pRational->Denominator, pRational->Numerator/pRational->Denominator, pSuffix));
}

void vboxVidPnDumpRanges(const char *pPrefix, const D3DKMDT_COLOR_COEFF_DYNAMIC_RANGES *pDynamicRanges, const char *pSuffix)
{
    drprintf(("%sFirstChannel(%d), SecondChannel(%d), ThirdChannel(%d), FourthChannel(%d)%s", pPrefix,
            pDynamicRanges->FirstChannel,
            pDynamicRanges->SecondChannel,
            pDynamicRanges->ThirdChannel,
            pDynamicRanges->FourthChannel,
            pSuffix));
}

void vboxVidPnDumpGammaRamp(const char *pPrefix, const D3DKMDT_GAMMA_RAMP *pGammaRamp, const char *pSuffix)
{
    drprintf(("%sType(%s), DataSize(%d), TODO: dump the rest%s", pPrefix,
            vboxVidPnDumpStrGammaRampType(pGammaRamp->Type), pGammaRamp->DataSize,
            pSuffix));
}

void vboxVidPnDumpSourceMode(const char *pPrefix, const D3DKMDT_VIDPN_SOURCE_MODE* pVidPnSourceModeInfo, const char *pSuffix)
{
    drprintf(("%sType(%s), ", pPrefix, vboxVidPnDumpStrSourceModeType(pVidPnSourceModeInfo->Type)));
    vboxVidPnDumpRegion("surf(", &pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize, "), ");
    vboxVidPnDumpRegion("vis(", &pVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize, "), ");
    drprintf(("stride(%d), ", pVidPnSourceModeInfo->Format.Graphics.Stride));
    drprintf(("format(%s), ", vboxVidPnDumpStrPixFormat(pVidPnSourceModeInfo->Format.Graphics.PixelFormat)));
    drprintf(("clrBasis(%s), ", vboxVidPnDumpStrColorBasis(pVidPnSourceModeInfo->Format.Graphics.ColorBasis)));
    drprintf(("pvam(%s)%s", vboxVidPnDumpStrPvam(pVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode), pSuffix));
}

void vboxVidPnDumpSignalInfo(const char *pPrefix, const D3DKMDT_VIDEO_SIGNAL_INFO *pVideoSignalInfo, const char *pSuffix)
{
    drprintf(("%sVStd(%s), ", pPrefix, vboxVidPnDumpStrSignalStandard(pVideoSignalInfo->VideoStandard)));
    vboxVidPnDumpRegion("totSize(", &pVideoSignalInfo->TotalSize, "), ");
    vboxVidPnDumpRegion("activeSize(", &pVideoSignalInfo->ActiveSize, "), ");
    vboxVidPnDumpRational("VSynch(", &pVideoSignalInfo->VSyncFreq, "), ");
    drprintf(("PixelRate(%d), ScanLineOrdering(%s)%s\n", pVideoSignalInfo->PixelRate, vboxVidPnDumpStrScanLineOrdering(pVideoSignalInfo->ScanLineOrdering), pSuffix));
}

void vboxVidPnDumpTargetMode(const char *pPrefix, const D3DKMDT_VIDPN_TARGET_MODE* CONST  pVidPnTargetModeInfo, const char *pSuffix)
{
    drprintf(("%s", pPrefix));
    vboxVidPnDumpSignalInfo("VSI: ", &pVidPnTargetModeInfo->VideoSignalInfo, ", ");
    drprintf(("Preference(%s)%s", vboxVidPnDumpStrModePreference(pVidPnTargetModeInfo->Preference), pPrefix));
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
            drprintf(("Source NOT Pinned\n"));
        }
        else
        {
            drprintf(("ERROR getting piined Source Mode(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        drprintf(("ERROR getting SourceModeSet(0x%x)\n", Status));
    }
}


static DECLCALLBACK(BOOLEAN) vboxVidPnDumpSourceModeSetEnum(struct _DEVICE_EXTENSION* pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo, PVOID pContext)
{
    vboxVidPnDumpSourceMode("SourceMode: ", pNewVidPnSourceModeInfo, "\n");
    return TRUE;
}

void vboxVidPnDumpSourceModeSet(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    drprintf(("  >>>***SourceMode Set for Source(%d)***\n", VidPnSourceId));
    D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireSourceModeSet(hVidPn,
                        VidPnSourceId,
                        &hCurVidPnSourceModeSet,
                        &pCurVidPnSourceModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {

        Status = vboxVidPnEnumSourceModes(pDevExt, hVidPn, pVidPnInterface, hCurVidPnSourceModeSet, pCurVidPnSourceModeSetInterface,
                vboxVidPnDumpSourceModeSetEnum, NULL);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            drprintf(("ERROR enumerating Source Modes(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseSourceModeSet(hVidPn, hCurVidPnSourceModeSet);
    }
    else
    {
        drprintf(("ERROR getting SourceModeSet for Source(%d), Status(0x%x)\n", VidPnSourceId, Status));
    }

    drprintf(("  <<<***End Of SourceMode Set for Source(%d)***\n", VidPnSourceId));
}

DECLCALLBACK(BOOLEAN) vboxVidPnDumpTargetModeSetEnum(struct _DEVICE_EXTENSION* pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet, const DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo, PVOID pContext)
{
    vboxVidPnDumpTargetMode("TargetMode: ", pNewVidPnTargetModeInfo, "\n");
    return TRUE;
}

void vboxVidPnDumpTargetModeSet(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, D3DDDI_VIDEO_PRESENT_TARGET_ID VidPnTargetId)
{
    drprintf(("  >>>***TargetMode Set for Target(%d)***\n", VidPnTargetId));
    D3DKMDT_HVIDPNTARGETMODESET hCurVidPnTargetModeSet;
    const DXGK_VIDPNTARGETMODESET_INTERFACE *pCurVidPnTargetModeSetInterface;

    NTSTATUS Status = pVidPnInterface->pfnAcquireTargetModeSet(hVidPn,
                        VidPnTargetId,
                        &hCurVidPnTargetModeSet,
                        &pCurVidPnTargetModeSetInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {

        Status = vboxVidPnEnumTargetModes(pDevExt, hVidPn, pVidPnInterface, hCurVidPnTargetModeSet, pCurVidPnTargetModeSetInterface,
                vboxVidPnDumpTargetModeSetEnum, NULL);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            drprintf(("ERROR enumerating Target Modes(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    }
    else
    {
        drprintf(("ERROR getting TargetModeSet for Target(%d), Status(0x%x)\n", VidPnTargetId, Status));
    }

    drprintf(("  <<<***End Of TargetMode Set for Target(%d)***\n", VidPnTargetId));
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
            drprintf(("Target NOT Pinned\n"));
        }
        else
        {
            drprintf(("ERROR getting piined Target Mode(0x%x)\n", Status));
        }
        pVidPnInterface->pfnReleaseTargetModeSet(hVidPn, hCurVidPnTargetModeSet);
    }
    else
    {
        drprintf(("ERROR getting TargetModeSet(0x%x)\n", Status));
    }
}

void vboxVidPnDumpPath(struct _DEVICE_EXTENSION* pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo)
{
    drprintf((" >>Start Dump VidPn Path>>\n"));
    drprintf(("VidPnSourceId(%d),  VidPnTargetId(%d)\n",
            pVidPnPresentPathInfo->VidPnSourceId, pVidPnPresentPathInfo->VidPnTargetId));

    vboxVidPnDumpPinnedSourceMode(hVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnSourceId);
    vboxVidPnDumpPinnedTargetMode(hVidPn, pVidPnInterface, pVidPnPresentPathInfo->VidPnTargetId);

    drprintf(("ImportanceOrdinal(%s), VidPnTargetColorBasis(%s), Content(%s)\n",
            vboxVidPnDumpStrImportance(pVidPnPresentPathInfo->ImportanceOrdinal),
            vboxVidPnDumpStrColorBasis(pVidPnPresentPathInfo->VidPnTargetColorBasis),
            vboxVidPnDumpStrContent(pVidPnPresentPathInfo->Content)));
    vboxVidPnDumpPathTransformation(&pVidPnPresentPathInfo->ContentTransformation);
    vboxVidPnDumpRegion("VisibleFromActiveTLOffset(", &pVidPnPresentPathInfo->VisibleFromActiveTLOffset, ")\n");
    vboxVidPnDumpRegion("VisibleFromActiveBROffset(", &pVidPnPresentPathInfo->VisibleFromActiveBROffset, ")\n");
    vboxVidPnDumpRanges("VidPnTargetColorCoeffDynamicRanges: ", &pVidPnPresentPathInfo->VidPnTargetColorCoeffDynamicRanges, "\n");
    vboxVidPnDumpCopyProtectoin(&pVidPnPresentPathInfo->CopyProtection);
    vboxVidPnDumpGammaRamp("GammaRamp: ", &pVidPnPresentPathInfo->GammaRamp, "\n");

    drprintf((" <<Stop Dump VidPn Path<<\n"));
}

static DECLCALLBACK(BOOLEAN) vboxVidPnDumpPathEnum(struct _DEVICE_EXTENSION* pDevExt, const D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        const D3DKMDT_VIDPN_PRESENT_PATH *pVidPnPresentPathInfo, PVOID pContext)
{
    vboxVidPnDumpPath(pDevExt, hVidPn, pVidPnInterface, pVidPnPresentPathInfo);

    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pVidPnPresentPathInfo);
    return TRUE;
}

void vboxVidPnDumpVidPn(const char * pPrefix, PDEVICE_EXTENSION pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface, const char * pSuffix)
{
    drprintf (("%s", pPrefix));

    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    NTSTATUS Status = pVidPnInterface->pfnGetTopology(hVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVidPnEnumPaths(pDevExt, hVidPn, pVidPnInterface,
                                        hVidPnTopology, pVidPnTopologyInterface,
                                        vboxVidPnDumpPathEnum, NULL);
        Assert(Status == STATUS_SUCCESS);
    }

    for (int i = 0; i < commonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        vboxVidPnDumpSourceModeSet(pDevExt, hVidPn, pVidPnInterface, (D3DDDI_VIDEO_PRESENT_SOURCE_ID)i);
        vboxVidPnDumpTargetModeSet(pDevExt, hVidPn, pVidPnInterface, (D3DDDI_VIDEO_PRESENT_TARGET_ID)i);
    }

    drprintf (("%s", pSuffix));
}
