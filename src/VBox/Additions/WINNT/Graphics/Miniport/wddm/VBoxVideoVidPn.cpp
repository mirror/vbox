/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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

    while (pNewVidPnPresentPathInfo)
    {
        /* @todo: which paths do we support? no matter for now
        pNewVidPnPresentPathInfo->VidPnSourceId
        pNewVidPnPresentPathInfo->VidPnTargetId

        ImportanceOrdinal does not matter for now
        pNewVidPnPresentPathInfo->ImportanceOrdinal
        */

        if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY
                || pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_CENTERED)
        {
            dprintf(("unsupported Scaling (%d)\n", pNewVidPnPresentPathInfo->ContentTransformation.Scaling));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Stretched)
        {
            dprintf(("unsupported Scaling support (Stretched)\n"));
            bSupported = FALSE;
            break;
        }

        if (!pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Identity
                && !pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered)
        {
            dprintf(("\"Identity\" or \"Centered\" Scaling support not set\n"));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY)
        {
            dprintf(("unsupported rotation (%d)\n", pNewVidPnPresentPathInfo->ContentTransformation.Rotation));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate180
                || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate270
                || pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Rotate90)
        {
            dprintf(("unsupported RotationSupport\n"));
            bSupported = FALSE;
            break;
        }

        if (!pNewVidPnPresentPathInfo->ContentTransformation.RotationSupport.Identity)
        {
            dprintf(("\"Identity\" RotationSupport not set\n"));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx
                || pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy)
        {
            dprintf(("Non-zero TLOffset: cx(%d), cy(%d)\n",
                    pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cx,
                    pNewVidPnPresentPathInfo->VisibleFromActiveTLOffset.cy));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx
                || pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy)
        {
            dprintf(("Non-zero TLOffset: cx(%d), cy(%d)\n",
                    pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cx,
                    pNewVidPnPresentPathInfo->VisibleFromActiveBROffset.cy));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->VidPnTargetColorBasis != D3DKMDT_CB_SRGB)
        {
            dprintf(("unsupported VidPnTargetColorBasis (%d)\n", pNewVidPnPresentPathInfo->VidPnTargetColorBasis));
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
            bSupported = FALSE;
            break;
        }

        /* Content (D3DKMDT_VPPC_GRAPHICS, _NOTSPECIFIED, _VIDEO), does not matter for now
        pNewVidPnPresentPathInfo->Content
        */
        /* not support copy protection for now */
        if (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType != D3DKMDT_VPPMT_NOPROTECTION)
        {
            dprintf(("Copy protection not supported CopyProtectionType(%d)\n", pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits)
        {
            dprintf(("Copy protection not supported APSTriggerBits(%d)\n", pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits));
            bSupported = FALSE;
            break;
        }

        D3DKMDT_VIDPN_PRESENT_PATH_COPYPROTECTION_SUPPORT tstCPSupport = {0};
        tstCPSupport.NoProtection = 1;
        if (memcmp(&tstCPSupport, &pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, sizeof(tstCPSupport)))
        {
            dprintf(("Copy protection support (0x%x)\n", *((UINT*)&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport)));
            bSupported = FALSE;
            break;
        }

        if (pNewVidPnPresentPathInfo->GammaRamp.Type != D3DDDI_GAMMARAMP_DEFAULT)
        {
            dprintf(("Unsupported GammaRamp.Type (%d)\n", pNewVidPnPresentPathInfo->GammaRamp.Type));
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
        while (pNewVidPnSourceModeInfo)
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
    else
    {
        drprintf(("VBoxVideoWddm: pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));
    }

    *pbSupported = bSupported;
    return Status;
}

NTSTATUS vboxVidPnCheckTargetModeInfo(const D3DKMDT_HVIDPN hDesiredVidPn,
        const D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo,
        BOOLEAN *pbSupported)
{
    BOOLEAN bSupported = TRUE;
    do
    {
        /* video standatd does not matter ??
        pNewVidPnTargetModeInfo->VideoSignalInfo.VideoStandard

        we support any total size??
        pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cx
        pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy
        */
        /* ActualSize should be same as total size??*/
        if (pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cx != pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cx
                || pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy != pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy)
        {
            dprintf(("ActiveSize(%d, %d) !=  TotalSize(%d, %d)\n",
                    pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cx,
                    pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize.cy,
                    pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cx,
                    pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy));
            bSupported = FALSE;
            break;
        }

        /* we do not care about those
        pNewVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Numerator
        pNewVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Denominator
        pNewVidPnTargetModeInfo->VideoSignalInfo.HSyncFreq.Numerator
        pNewVidPnTargetModeInfo->VideoSignalInfo.HSyncFreq.Denominator
        pNewVidPnTargetModeInfo->VideoSignalInfo.PixelRate
        pNewVidPnTargetModeInfo->VideoSignalInfo.ScanLineOrdering
        pNewVidPnTargetModeInfo->Preference
        */
    } while (1);

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
        while (pNewVidPnTargetModeInfo)
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
    else
    {
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));
    }

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
        uint32_t iPreferredMomde)
{
    NTSTATUS Status = STATUS_SUCCESS;
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
                    if (iPreferredMomde == i)
                    {
                        Status = pNewVidPnSourceModeSetInterface->pfnPinMode(hNewVidPnSourceModeSet, modeId);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status != STATUS_SUCCESS)
                        {
                            drprintf((__FUNCTION__": pfnPinMode failed, Status(0x%x)", Status));
                            /* don't treat it as fatal */
                            Status = STATUS_SUCCESS;
                        }
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
    return Status;
}

NTSTATUS vboxVidPnPopulateTargetModeInfoFromLegacy(PDEVICE_EXTENSION pDevExt,
        D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo,
        VIDEO_MODE_INFORMATION *pMode)
{
    NTSTATUS Status = STATUS_SUCCESS;
#if 0
    todo();
    if (pMode->AttributeFlags & VIDEO_MODE_GRAPHICS)
    {
        /* this is a graphics mode */
        pNewVidPnSourceModeInfo->Type = D3DKMDT_RMT_GRAPHICS;
        pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx = pMode->VisScreenWidth;
        pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy = pMode->VisScreenHeight;
        pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize = pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize;
        pNewVidPnSourceModeInfo->Format.Graphics.Stride = pMode->ScreenStride;
        pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat = vboxWddmCalcPixelFormat(pInfo);
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
        AssertBreakPoint();
        Status = STATUS_INVALID_PARAMETER;
    }
#endif
    return Status;
}

NTSTATUS vboxVidPnPopulateTargetModeSetFromLegacy(PDEVICE_EXTENSION pDevExt,
        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet,
        const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface,
        VIDEO_MODE_INFORMATION *pModes,
        uint32_t cModes,
        uint32_t iPreferredMomde)
{
    NTSTATUS Status = STATUS_SUCCESS;
    for (uint32_t i = 0; i < cModes; ++i)
    {
        D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
        Status = pNewVidPnTargetModeSetInterface->pfnCreateNewModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxVidPnPopulateTargetModeInfoFromLegacy(pDevExt, pNewVidPnTargetModeInfo, &pModes[i]);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID modeId = pNewVidPnTargetModeInfo->Id;
                Status = pNewVidPnTargetModeSetInterface->pfnAddMode(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    if (iPreferredMomde == i)
                    {
                        Status = pNewVidPnTargetModeSetInterface->pfnPinMode(hNewVidPnTargetModeSet, modeId);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status != STATUS_SUCCESS)
                        {
                            drprintf((__FUNCTION__": pfnPinMode failed, Status(0x%x)", Status));
                            /* don't treat it as fatal */
                            Status = STATUS_SUCCESS;
                        }
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
    uint32_t iPreferredMode = pCbContext->iPreferredMode;

    /* adjust scaling */
    if (pCbContext->pEnumCofuncModalityArg->EnumPivotType != D3DKMDT_EPT_SCALING)
    {
        if (pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_IDENTITY
                && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_CENTERED
                && pNewVidPnPresentPathInfo->ContentTransformation.Scaling != D3DKMDT_VPPS_STRETCHED)
        {
            const_cast<D3DKMDT_VIDPN_PRESENT_PATH*>(pNewVidPnPresentPathInfo)->ContentTransformation.Scaling = D3DKMDT_VPPS_IDENTITY;
        }
    }

    /* adjust rotation */
    if (pCbContext->pEnumCofuncModalityArg->EnumPivotType != D3DKMDT_EPT_ROTATION)
    {
        if (pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_IDENTITY
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE90
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE180
                && pNewVidPnPresentPathInfo->ContentTransformation.Rotation != D3DKMDT_VPPR_ROTATE270)
        {
            const_cast<D3DKMDT_VIDPN_PRESENT_PATH*>(pNewVidPnPresentPathInfo)->ContentTransformation.Rotation = D3DKMDT_VPPR_IDENTITY;
        }
    }

    if (pCbContext->pEnumCofuncModalityArg->EnumPivotType != D3DKMDT_EPT_VIDPNSOURCE
            || pNewVidPnPresentPathInfo->VidPnSourceId != pCbContext->pEnumCofuncModalityArg->EnumPivot.VidPnSourceId)
    {
        D3DKMDT_HVIDPNSOURCEMODESET hCurVidPnSourceModeSet;
        const DXGK_VIDPNSOURCEMODESET_INTERFACE *pCurVidPnSourceModeSetInterface;

        Status = pVidPnInterface->pfnAcquireSourceModeSet(hDesiredVidPn,
                    pNewVidPnPresentPathInfo->VidPnSourceId,
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
                if (!pPinnedVidPnSourceModeInfo)
                {
                    /* just create and populate the new source mode set for now */
                    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
                    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pNewVidPnSourceModeSetInterface;
                    Status = pVidPnInterface->pfnCreateNewSourceModeSet(hDesiredVidPn,
                                    pNewVidPnPresentPathInfo->VidPnSourceId, /*__in CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId */
                                    &hNewVidPnSourceModeSet,
                                    &pNewVidPnSourceModeSetInterface);
                    Assert(Status == STATUS_SUCCESS);
                    if (Status == STATUS_SUCCESS)
                    {
                        Status = vboxVidPnPopulateSourceModeSetFromLegacy(pDevExt,
                                hNewVidPnSourceModeSet, pNewVidPnSourceModeSetInterface,
                                pModes, cModes, iPreferredMode);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status == STATUS_SUCCESS)
                        {
                            Status = pVidPnInterface->pfnAssignSourceModeSet(hDesiredVidPn,
                                    pNewVidPnPresentPathInfo->VidPnSourceId,
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
                }
                else
                {
                    /* release */
                    pCurVidPnSourceModeSetInterface->pfnReleaseModeInfo(hCurVidPnSourceModeSet, pPinnedVidPnSourceModeInfo);
                }
            }
            else
                drprintf((__FUNCTION__": pfnAcquirePinnedModeInfo failed Status(0x%x)\n", Status));

            pVidPnInterface->pfnReleaseSourceModeSet(hDesiredVidPn, hCurVidPnSourceModeSet);
        }
        else
        {
            drprintf((__FUNCTION__": pfnAcquireSourceModeSet failed Status(0x%x)\n", Status));
        }
    }

    /* ensure we didn't fail earlier */
    if (Status == STATUS_SUCCESS)
    {
        if (pCbContext->pEnumCofuncModalityArg->EnumPivotType != D3DKMDT_EPT_VIDPNTARGET
                || pNewVidPnPresentPathInfo->VidPnTargetId != pCbContext->pEnumCofuncModalityArg->EnumPivot.VidPnTargetId)
        {
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
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                    if (!pPinnedVidPnTargetModeInfo)
                    {
                        /* just create and populate a new target mode info for now */
                        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet;
                        const DXGK_VIDPNTARGETMODESET_INTERFACE *pNewVidPnTargetModeSetInterface;
                        Status = pVidPnInterface->pfnCreateNewTargetModeSet(hDesiredVidPn,
                                        pNewVidPnPresentPathInfo->VidPnTargetId, /*__in CONST D3DDDI_VIDEO_PRESENT_TARGET_ID  VidPnTargetId */
                                        &hNewVidPnTargetModeSet,
                                        &pNewVidPnTargetModeSetInterface);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status == STATUS_SUCCESS)
                        {
                            Status = vboxVidPnPopulateTargetModeSetFromLegacy(pDevExt,
                                    hNewVidPnTargetModeSet, pNewVidPnTargetModeSetInterface,
                                    pModes, cModes, iPreferredMode);
                            Assert(Status == STATUS_SUCCESS);
                            if (Status == STATUS_SUCCESS)
                            {
                                Status = pVidPnInterface->pfnAssignTargetModeSet(hDesiredVidPn,
                                        pNewVidPnPresentPathInfo->VidPnTargetId,
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
                    }
                    else
                    {
                        /* nothing to be done here, just release */
                        pCurVidPnTargetModeSetInterface->pfnReleaseModeInfo(hCurVidPnTargetModeSet, pPinnedVidPnTargetModeInfo);
                    }
                }
                else
                    drprintf((__FUNCTION__": pfnAcquirePinnedModeInfo failed Status(0x%x)\n", Status));

                pVidPnInterface->pfnReleaseTargetModeSet(hDesiredVidPn, hCurVidPnTargetModeSet);
            }
            else
                drprintf((__FUNCTION__": pfnAcquireTargetModeSet failed Status(0x%x)\n", Status));
        }
    }

    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);

    pCbContext->Status = Status;
    return Status == STATUS_SUCCESS;
}

NTSTATUS vboxVidPnEnumSourceModes(PDEVICE_EXTENSION pDevExt, const D3DKMDT_HVIDPN hDesiredVidPn, const DXGK_VIDPN_INTERFACE* pVidPnInterface,
        D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet, const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface,
        PFNVBOXVIDPNENUMSOURCEMODES pfnCallback, PVOID pContext)
{
    const D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
    NTSTATUS Status = pVidPnSourceModeSetInterface->pfnAcquireFirstModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
    if (Status == STATUS_SUCCESS)
    {
        while (pNewVidPnSourceModeInfo)
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
                else
                {
                    drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnSourceModeInfo = pNextVidPnSourceModeInfo;
            else
            {
                AssertBreakpoint();
                drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
                pNewVidPnSourceModeInfo = NULL;
                break;
            }
        }
    }
    else
    {
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));
    }

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
        while (pNewVidPnTargetModeInfo)
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
                else
                {
                    drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnTargetModeInfo = pNextVidPnTargetModeInfo;
            else
            {
                AssertBreakpoint();
                drprintf((__FUNCTION__": pfnAcquireNextModeInfo Failed Status(0x%x)\n", Status));
                pNewVidPnTargetModeInfo = NULL;
                break;
            }
        }
    }
    else
    {
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n", Status));
    }

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
        while (pNewVidPnPresentPathInfo)
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
                    drprintf((__FUNCTION__": pfnAcquireNextPathInfo Failed Status(0x%x), ignored since callback returned false\n", Status));
                    Status = STATUS_SUCCESS;
                }

                break;
            }
            else if (Status == STATUS_SUCCESS)
                pNewVidPnPresentPathInfo = pNextVidPnPresentPathInfo;
            else
            {
                AssertBreakpoint();
                drprintf((__FUNCTION__": pfnAcquireNextPathInfo Failed Status(0x%x)\n", Status));
                pNewVidPnPresentPathInfo = NULL;
                break;
            }
        }
    }

    return Status;
}
