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
        drprintf(("VBoxVideoWddm: pfnAcquireFirstModeInfo failed Status(0x%x)\n"));
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
        drprintf((__FUNCTION__": pfnAcquireFirstModeInfo failed Status(0x%x)\n"));
    }

    *pbSupported = bSupported;
    return Status;
}
