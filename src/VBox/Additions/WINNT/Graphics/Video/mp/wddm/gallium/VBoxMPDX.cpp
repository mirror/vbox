/* $Id$ */
/** @file
 * VirtualBox Windows Guest Graphics Driver - Direct3D (DX) driver function.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#define GALOG_GROUP GALOG_GROUP_DXGK

#include "VBoxMPGaWddm.h"
#include "../VBoxMPVidPn.h"
#include "VBoxMPGaExt.h"

#include "Svga.h"
#include "SvgaFifo.h"
#include "SvgaCmd.h"
#include "SvgaHw.h"


bool SvgaIsDXSupported(PVBOXMP_DEVEXT pDevExt)
{
    return    pDevExt->pGa
           && pDevExt->pGa->hw.pSvga
           && RT_BOOL(pDevExt->pGa->hw.pSvga->u32Caps & SVGA_CAP_DX);
}


static NTSTATUS svgaCreateSurfaceForAllocation(PVBOXWDDM_EXT_GA pGaDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    NTSTATUS Status = SvgaSurfaceIdAlloc(pSvga, &pAllocation->dx.sid);
    if (NT_SUCCESS(Status))
    {
        void *pvCmd = SvgaCmdBuf3dCmdReserve(pSvga, SVGA_3D_CMD_DEFINE_GB_SURFACE_V2, sizeof(SVGA3dCmdDefineGBSurface_v2), SVGA3D_INVALID_ID);
        if (pvCmd)
        {
            SVGA3dCmdDefineGBSurface_v2 *pCmd = (SVGA3dCmdDefineGBSurface_v2 *)pvCmd;
            pCmd->sid              = pAllocation->dx.sid;
            pCmd->surfaceFlags     = (uint32_t)pAllocation->dx.desc.surfaceInfo.surfaceFlags;
            pCmd->format           = pAllocation->dx.desc.surfaceInfo.format;
            pCmd->numMipLevels     = pAllocation->dx.desc.surfaceInfo.numMipLevels;
            pCmd->multisampleCount = pAllocation->dx.desc.surfaceInfo.multisampleCount;
            pCmd->autogenFilter    = pAllocation->dx.desc.surfaceInfo.autogenFilter;
            pCmd->size             = pAllocation->dx.desc.surfaceInfo.size;
            pCmd->arraySize        = pAllocation->dx.desc.surfaceInfo.arraySize;
            pCmd->pad              = 0;
            SvgaCmdBufCommit(pSvga, sizeof(SVGA3dCmdDefineGBSurface_v2));
        }
        else
            Status = STATUS_INSUFFICIENT_RESOURCES;
    }
    return Status;
}


static NTSTATUS svgaCreateAllocationSurface(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation, DXGK_ALLOCATIONINFO *pAllocationInfo)
{
    NTSTATUS Status = svgaCreateSurfaceForAllocation(pDevExt->pGa, pAllocation);
    AssertReturn(NT_SUCCESS(Status), Status);

    /* Fill data for WDDM. */
    pAllocationInfo->Alignment                       = 0;
    pAllocationInfo->Size                            = pAllocation->dx.desc.cbAllocation;
    pAllocationInfo->PitchAlignedSize                = 0;
    pAllocationInfo->HintedBank.Value                = 0;
    pAllocationInfo->Flags.Value                     = 0;
    if (pAllocation->dx.desc.surfaceInfo.surfaceFlags
        & (SVGA3D_SURFACE_HINT_INDIRECT_UPDATE | SVGA3D_SURFACE_HINT_STATIC))
    {
        /* USAGE_DEFAULT */
/// @todo Might need to put primaries in a CPU visible segment for readback.
//        if (pAllocation->dx.desc.fPrimary)
//        {
//            /* Put primaries to the CPU visible segment. */
//            pAllocationInfo->PreferredSegment.Value      = 0;
//            pAllocationInfo->SupportedReadSegmentSet     = 1; /* VRAM */
//            pAllocationInfo->SupportedWriteSegmentSet    = 1; /* VRAM */
//            pAllocationInfo->Flags.CpuVisible            = 1;
//        }
//        else
        {
            pAllocationInfo->PreferredSegment.Value      = 0;
            pAllocationInfo->SupportedReadSegmentSet     = 4; /* Host */
            pAllocationInfo->SupportedWriteSegmentSet    = 4; /* Host */
        }
    }
    else if (pAllocation->dx.desc.surfaceInfo.surfaceFlags
             & SVGA3D_SURFACE_HINT_DYNAMIC)
    {
        /* USAGE_DYNAMIC */
        pAllocationInfo->PreferredSegment.Value      = 0;
        pAllocationInfo->SupportedReadSegmentSet     = 2; /* Aperture */
        pAllocationInfo->SupportedWriteSegmentSet    = 2; /* Aperture */
        pAllocationInfo->Flags.CpuVisible            = 1;
    }
    else if (pAllocation->dx.desc.surfaceInfo.surfaceFlags
             & (SVGA3D_SURFACE_STAGING_UPLOAD | SVGA3D_SURFACE_STAGING_DOWNLOAD))
    {
        /* USAGE_STAGING */
        /** @todo Maybe use VRAM? */
        pAllocationInfo->PreferredSegment.SegmentId0 = 0;
        pAllocationInfo->SupportedReadSegmentSet     = 2; /* Aperture */
        pAllocationInfo->SupportedWriteSegmentSet    = 2; /* Aperture */
        pAllocationInfo->Flags.CpuVisible            = 1;
    }
    pAllocationInfo->EvictionSegmentSet              = 0;
    pAllocationInfo->MaximumRenamingListLength       = 1;
    pAllocationInfo->hAllocation                     = pAllocation;
    pAllocationInfo->pAllocationUsageHint            = NULL;
    pAllocationInfo->AllocationPriority              = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
    return Status;
}


static NTSTATUS svgaCreateAllocationShaders(PVBOXWDDM_ALLOCATION pAllocation, DXGK_ALLOCATIONINFO *pAllocationInfo)
{
    /* Fill data for WDDM. */
    pAllocationInfo->Alignment                       = 0;
    pAllocationInfo->Size                            = pAllocation->dx.desc.cbAllocation;
    pAllocationInfo->PitchAlignedSize                = 0;
    pAllocationInfo->HintedBank.Value                = 0;
    pAllocationInfo->Flags.Value                     = 0;
    pAllocationInfo->Flags.CpuVisible                = 1;
    pAllocationInfo->PreferredSegment.Value          = 0;
    pAllocationInfo->SupportedReadSegmentSet         = 2; /* Aperture */
    pAllocationInfo->SupportedWriteSegmentSet        = 2; /* Aperture */
    pAllocationInfo->EvictionSegmentSet              = 0;
    pAllocationInfo->MaximumRenamingListLength       = 0;
    pAllocationInfo->hAllocation                     = pAllocation;
    pAllocationInfo->pAllocationUsageHint            = NULL;
    pAllocationInfo->AllocationPriority              = D3DDDI_ALLOCATIONPRIORITY_MAXIMUM;
    return STATUS_SUCCESS;
}


static NTSTATUS svgaDestroyAllocationSurface(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pDevExt->pGa->hw.pSvga;
    NTSTATUS Status = STATUS_SUCCESS;
    if (pAllocation->dx.sid != SVGA3D_INVALID_ID)
    {
        void *pvCmd = SvgaCmdBuf3dCmdReserve(pSvga, SVGA_3D_CMD_DESTROY_GB_SURFACE, sizeof(SVGA3dCmdDestroyGBSurface), SVGA3D_INVALID_ID);
        if (pvCmd)
        {
            SVGA3dCmdDestroyGBSurface *pCmd = (SVGA3dCmdDestroyGBSurface *)pvCmd;
            pCmd->sid = pAllocation->dx.sid;
            SvgaCmdBufCommit(pSvga, sizeof(SVGA3dCmdDestroyGBSurface));
        }
        else
            Status = STATUS_INSUFFICIENT_RESOURCES;

        if (NT_SUCCESS(Status))
            Status = SvgaSurfaceIdFree(pSvga, pAllocation->dx.sid);
    }
    return Status;
}


static NTSTATUS svgaDestroyAllocationShaders(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pDevExt->pGa->hw.pSvga;
    if (pAllocation->dx.mobid != SVGA3D_INVALID_ID)
    {
        void *pvCmd = SvgaCmdBuf3dCmdReserve(pSvga, SVGA_3D_CMD_DESTROY_GB_MOB, sizeof(SVGA3dCmdDestroyGBMob), SVGA3D_INVALID_ID);
        if (pvCmd)
        {
            SVGA3dCmdDestroyGBMob *pCmd = (SVGA3dCmdDestroyGBMob *)pvCmd;
            pCmd->mobid = pAllocation->dx.mobid;
            SvgaCmdBufCommit(pSvga, sizeof(SVGA3dCmdDestroyGBMob));
        }
        else
            AssertFailedReturn(STATUS_INSUFFICIENT_RESOURCES);
    }
    return STATUS_SUCCESS;
}


NTSTATUS APIENTRY DxgkDdiDXCreateAllocation(
    CONST HANDLE hAdapter,
    DXGKARG_CREATEALLOCATION *pCreateAllocation)
{
    DXGK_ALLOCATIONINFO *pAllocationInfo = &pCreateAllocation->pAllocationInfo[0];
    AssertReturn(   pCreateAllocation->PrivateDriverDataSize == 0
                 && pCreateAllocation->NumAllocations == 1
                 && pCreateAllocation->pAllocationInfo[0].PrivateDriverDataSize == sizeof(VBOXDXALLOCATIONDESC),
                 STATUS_INVALID_PARAMETER);

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)GaMemAllocZero(sizeof(VBOXWDDM_ALLOCATION));
    AssertReturn(pAllocation, STATUS_INSUFFICIENT_RESOURCES);

    /* Init allocation data. */
    pAllocation->enmType = VBOXWDDM_ALLOC_TYPE_D3D;
    pAllocation->dx.desc = *(PVBOXDXALLOCATIONDESC)pAllocationInfo->pPrivateDriverData;
    pAllocation->dx.sid = SVGA3D_INVALID_ID;
    pAllocation->dx.mobid = SVGA3D_INVALID_ID;

    /* Legacy. Unused. */
    KeInitializeSpinLock(&pAllocation->OpenLock);
    InitializeListHead(&pAllocation->OpenList);

    if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
        Status = svgaCreateAllocationSurface(pDevExt, pAllocation, pAllocationInfo);
    else if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SHADERS)
        Status = svgaCreateAllocationShaders(pAllocation, pAllocationInfo);
    else
        Status = STATUS_INVALID_PARAMETER;
    AssertReturnStmt(NT_SUCCESS(Status), GaMemFree(pAllocation), Status);

    return Status;
}


NTSTATUS APIENTRY DxgkDdiDXDestroyAllocation(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_DESTROYALLOCATION*  pDestroyAllocation)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    NTSTATUS Status = STATUS_SUCCESS;

    AssertReturn(pDestroyAllocation->NumAllocations == 1, STATUS_INVALID_PARAMETER);

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pDestroyAllocation->pAllocationList[0];
    AssertReturn(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_D3D, STATUS_INVALID_PARAMETER);

    if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
        Status = svgaDestroyAllocationSurface(pDevExt, pAllocation);
    else if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SHADERS)
        Status = svgaDestroyAllocationShaders(pDevExt, pAllocation);
    else
        AssertFailedReturn(STATUS_INVALID_PARAMETER);

    RT_ZERO(*pAllocation);
    GaMemFree(pAllocation);

    return Status;
}


NTSTATUS APIENTRY DxgkDdiDXDescribeAllocation(
    CONST HANDLE  hAdapter,
    DXGKARG_DESCRIBEALLOCATION*  pDescribeAllocation)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    RT_NOREF(pDevExt);

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pDescribeAllocation->hAllocation;
    AssertReturn(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_D3D, STATUS_INVALID_PARAMETER);
    AssertReturn(pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE, STATUS_INVALID_PARAMETER);

    pDescribeAllocation->Width                        = pAllocation->dx.desc.surfaceInfo.size.width;
    pDescribeAllocation->Height                       = pAllocation->dx.desc.surfaceInfo.size.height;
    pDescribeAllocation->Format                       = pAllocation->dx.desc.enmDDIFormat;
    pDescribeAllocation->MultisampleMethod.NumSamples       = 0; /** @todo Multisample. */
    pDescribeAllocation->MultisampleMethod.NumQualityLevels = 0;
    if (pAllocation->dx.desc.fPrimary)
    {
        pDescribeAllocation->RefreshRate.Numerator    = pAllocation->dx.desc.PrimaryDesc.ModeDesc.RefreshRate.Numerator;
        pDescribeAllocation->RefreshRate.Denominator  = pAllocation->dx.desc.PrimaryDesc.ModeDesc.RefreshRate.Denominator;
    }
    else
    {
        pDescribeAllocation->RefreshRate.Numerator    = 0;
        pDescribeAllocation->RefreshRate.Denominator  = 0;
    }
    pDescribeAllocation->PrivateDriverFormatAttribute = 0;
    pDescribeAllocation->Flags.Value                  = 0;
    pDescribeAllocation->Rotation                     = D3DDDI_ROTATION_IDENTITY;

    return STATUS_SUCCESS;
}

