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

NTSTATUS vboxVidPnCheckTopology(const D3DKMDT_HVIDPN hDesiredVidPn,
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology, const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface,
        BOOLEAN *pbSupported)
{
    const D3DKMDT_VIDPN_PRESENT_PATH *pNewVidPnPresentPathInfo = NULL;
    NTSTATUS Status = pVidPnTopologyInterface->pfnAcquireFirstPathInfo(hVidPnTopology, &pNewVidPnPresentPathInfo);
    BOOLEAN bSupported = TRUE;

    if (Status == STATUS_SUCCESS)
    {
        while (1)
        {
            /* @todo: which paths do we support? no matter for now
            pNewVidPnPresentPathInfo->VidPnSourceId
            pNewVidPnPresentPathInfo->VidPnTargetId

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
                /* mark it as unneened */
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
    D3DKMDT_2DREGION *pResolutionsCopy = (D3DKMDT_2DREGION*)vboxWddmMemAlloc(cResolutions * sizeof (D3DKMDT_2DREGION));
    if (pResolutionsCopy)
    {
        memcpy(pResolutionsCopy, pResolutions, cResolutions * sizeof (D3DKMDT_2DREGION));
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
                        for (uint32_t i = 0; i < cResolutions; ++i)
                        {
                            D3DKMDT_2DREGION *pRes = &pResolutionsCopy[i];
                            if (pRes->cx)
                            {
                                Status = vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy(pDevExt,
                                        hMonitorSMS,
                                        pMonitorSMSIf,
                                        pRes,
                                        enmOrigin,
                                        i == (uint32_t)iPreferred);
                                Assert(Status == STATUS_SUCCESS);
                                if (Status != STATUS_SUCCESS)
                                {
                                    drprintf((__FUNCTION__": vboxVidPnCreatePopulateMonitorSourceModeInfoFromLegacy failed Status(0x%x)\n", Status));
                                    break;
                                }
                            }
                        }
                    }
                }

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
    }
    else
    {
        drprintf((__FUNCTION__": failed to allocate resolution copy of size (%d)\n", cResolutions));
        Status = STATUS_NO_MEMORY;
    }

    return Status;
}

NTSTATUS vboxVidPnCreatePopulateVidPnFromLegacy(PDEVICE_EXTENSION pDevExt, D3DKMDT_HVIDPN hVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        VIDEO_MODE_INFORMATION *pModes, uint32_t cModes, int iPreferredMode,
        D3DKMDT_2DREGION *pResolutions, uint32_t cResolutions)
{
    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    VIDEO_MODE_INFORMATION *pPreferredMode = iPreferredMode >= 0 ? &pModes[iPreferredMode] : NULL;
    D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID PreferredSrcModeId = D3DDDI_ID_UNINITIALIZED;
    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface;
    D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId = 0;
    D3DDDI_VIDEO_PRESENT_TARGET_ID tgtId = 0;
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
                                    pNewVidPnPresentPathInfo->VidPnSourceId = 0;
                                    pNewVidPnPresentPathInfo->VidPnTargetId = 0;
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
    VIDEO_MODE_INFORMATION *pModes = pCbContext->pModes;
    uint32_t cModes = pCbContext->cModes;
    int iPreferredMode = pCbContext->iPreferredMode;
    uint32_t cResolutions = pCbContext->cResolutions;
    D3DKMDT_2DREGION * pResolutions = pCbContext->pResolutions;


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
    Assert(srcId < pDevExt->cSources);
    if (srcId < pDevExt->cSources)
    {
        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[srcId];
        return vboxVidPnSetupSourceInfo(pDevExt, srcId, pSource, pVidPnSourceModeInfo, pAllocation);
    }

    drprintf((__FUNCTION__": invalid srcId (%d), cSources(%d)\n", srcId, pDevExt->cSources));
    return STATUS_INVALID_PARAMETER;
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
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Assert(pPinnedVidPnSourceModeInfo);
            Status = vboxVidPnCommitSourceMode(pDevExt, srcId, pPinnedVidPnSourceModeInfo, pAllocation);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                drprintf((__FUNCTION__": vboxVidPnCommitSourceMode failed Status(0x%x)\n", Status));
            /* release */
            pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
        }
        else if (Status == STATUS_GRAPHICS_MODE_NOT_PINNED)
            drprintf((__FUNCTION__": no pPinnedVidPnSourceModeInfo available for source id (%d)\n", srcId));
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
