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


static NTSTATUS svgaRenderPatches(PVBOXWDDM_CONTEXT pContext, DXGKARG_RENDER *pRender, void *pvDmaBuffer, uint32_t cbDmaBuffer)
{
    /** @todo Verify that patch is within the DMA buffer. */
    RT_NOREF(pContext, cbDmaBuffer);
    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cOut = 0;
    for (unsigned i = 0; i < pRender->PatchLocationListInSize; ++i)
    {
        D3DDDI_PATCHLOCATIONLIST const *pIn = &pRender->pPatchLocationListIn[i];
        void * const pPatchAddress = (uint8_t *)pvDmaBuffer + pIn->PatchOffset;
        VBOXDXALLOCATIONTYPE const enmAllocationType = (VBOXDXALLOCATIONTYPE)pIn->DriverId;

        DXGK_ALLOCATIONLIST *pAllocationListEntry = &pRender->pAllocationList[pIn->AllocationIndex];
        PVBOXWDDM_OPENALLOCATION pOA = (PVBOXWDDM_OPENALLOCATION)pAllocationListEntry->hDeviceSpecificAllocation;
        if (pOA)
        {
            PVBOXWDDM_ALLOCATION pAllocation = pOA->pAllocation;
            /* Allocation type determines what the patch is about. */
            Assert(pAllocation->dx.desc.enmAllocationType == enmAllocationType);
            if (enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
            {
                /* Surfaces might also need a mobid. */
                if (pAllocationListEntry->SegmentId == 3)
                {
                    /* DEFAULT resources only require the sid, because they exist outside the guest. */
                    if (pAllocation->dx.sid != SVGA3D_INVALID_ID)
                    {
                        *(uint32_t *)pPatchAddress = pAllocation->dx.sid;
                        continue;
                    }
                }
                else
                {
                    /* For Aperture segment, the surface need a mob too. */
                    if (   pAllocation->dx.sid != SVGA3D_INVALID_ID
                        && pAllocation->dx.mobid != SVGA3D_INVALID_ID)
                    {
                        *(uint32_t *)pPatchAddress = pAllocation->dx.sid;
                        continue;
                    }
                }
            }
            else if (enmAllocationType == VBOXDXALLOCATIONTYPE_SHADERS)
            {
                if (pAllocation->dx.mobid != SVGA3D_INVALID_ID)
                {
                    *(uint32_t *)pPatchAddress = pAllocation->dx.mobid;
                    continue;
                }
            }
        }
        else
        {
            if (   enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE
                || enmAllocationType == VBOXDXALLOCATIONTYPE_SHADERS)
            {
                *(uint32_t *)pPatchAddress = SVGA3D_INVALID_ID;
                continue;
            }
        }

        pRender->pPatchLocationListOut[cOut++] = *pIn;
    }

    GALOG(("pvDmaBuffer = %p, cbDmaBuffer = %u, cOut = %u\n", pvDmaBuffer, cbDmaBuffer, cOut));

    pRender->pPatchLocationListOut = &pRender->pPatchLocationListOut[cOut];
    return Status;
}


NTSTATUS APIENTRY DxgkDdiDXRender(PVBOXWDDM_CONTEXT pContext, DXGKARG_RENDER *pRender)
{
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;

    GALOG(("[%p] Command %p/%d, Dma %p/%d, Private %p/%d, MO %d, S %d, Phys 0x%RX64, AL %p/%d, PLLIn %p/%d, PLLOut %p/%d\n",
           pContext,
           pRender->pCommand, pRender->CommandLength,
           pRender->pDmaBuffer, pRender->DmaSize,
           pRender->pDmaBufferPrivateData, pRender->DmaBufferPrivateDataSize,
           pRender->MultipassOffset, pRender->DmaBufferSegmentId, pRender->DmaBufferPhysicalAddress.QuadPart,
           pRender->pAllocationList, pRender->AllocationListSize,
           pRender->pPatchLocationListIn, pRender->PatchLocationListInSize,
           pRender->pPatchLocationListOut, pRender->PatchLocationListOutSize
         ));

    AssertReturn(pRender->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA), STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER);

    GARENDERDATA *pRenderData = NULL;  /* Pointer to the DMA buffer description. */
    uint32_t cbPrivateData = 0;        /* Bytes to place into the private data buffer. */
    uint32_t u32TargetLength = 0;      /* Bytes to place into the DMA buffer. */
    uint32_t u32ProcessedLength = 0;   /* Bytes consumed from command buffer. */

    /* Calculate where the commands start. */
    void const *pvSource = (uint8_t *)pRender->pCommand + pRender->MultipassOffset;
    uint32_t cbSource = pRender->CommandLength - pRender->MultipassOffset;

    NTSTATUS Status = STATUS_SUCCESS;
    __try
    {
        /* Generate DMA buffer from the supplied command buffer.
         * Store the command buffer descriptor to pDmaBufferPrivateData.
         *
         * The display miniport driver must validate the command buffer.
         *
         * Copy commands to the pDmaBuffer.
         */
        Status = SvgaRenderCommandsD3D(pGaDevExt->hw.pSvga, pContext->pSvgaContext,
                                       pRender->pDmaBuffer, pRender->DmaSize, pvSource, cbSource,
                                       &u32TargetLength, &u32ProcessedLength);
        if (NT_SUCCESS(Status))
        {
            Status = svgaRenderPatches(pContext, pRender, pRender->pDmaBuffer, u32ProcessedLength);
        }

        /* Fill RenderData description in any case, it will be ignored if the above code failed. */
        pRenderData = (GARENDERDATA *)pRender->pDmaBufferPrivateData;
        pRenderData->u32DataType  = GARENDERDATA_TYPE_RENDER;
        pRenderData->cbData       = u32TargetLength;
        pRenderData->pFenceObject = NULL;
        pRenderData->pvDmaBuffer  = pRender->pDmaBuffer; /** @todo Should not be needed for D3D context. */
        pRenderData->pHwRenderData = NULL;
        cbPrivateData = sizeof(GARENDERDATA);
        GALOG(("Status = 0x%x\n", Status));
    }
    __except (EXCEPTION_EXECUTE_HANDLER)
    {
        Status = STATUS_INVALID_PARAMETER;
    }

    switch (Status)
    {
        case STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER:
            pRender->MultipassOffset += u32ProcessedLength;
            RT_FALL_THRU();
        case STATUS_SUCCESS:
        {
            Assert(pRenderData);
            if (u32TargetLength == 0)
            {
                DEBUG_BREAKPOINT_TEST();
                /* Trigger command submission anyway by increasing pDmaBufferPrivateData */
                /* Update the DMA buffer description. */
                pRenderData->u32DataType  = GARENDERDATA_TYPE_FENCE;
                pRenderData->cbData       = u32TargetLength;
                /* pRenderData->pFenceObject stays */
                pRenderData->pvDmaBuffer  = NULL; /* Not used */
            }
            pRender->pDmaBuffer = (uint8_t *)pRender->pDmaBuffer + u32TargetLength;
            pRender->pDmaBufferPrivateData = (uint8_t *)pRender->pDmaBufferPrivateData + cbPrivateData;
        } break;
        default: break;
    }

    return Status;
}


NTSTATUS APIENTRY DxgkDdiDXPresent(const HANDLE hContext, DXGKARG_PRESENT *pPresent)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;

    DXGK_ALLOCATIONLIST *pSrc = &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
    //DXGK_ALLOCATIONLIST *pDst = &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];

    if (pPresent->Flags.Blt)
    {
        //PVBOXWDDM_ALLOCATION pSrcAlloc = vboxWddmGetAllocationFromAllocList(pSrc);
        //PVBOXWDDM_ALLOCATION pDstAlloc = vboxWddmGetAllocationFromAllocList(pDst);

        //GALOGG(GALOG_GROUP_PRESENT, ("Blt: sid=%x -> sid=%x\n", pSrcAlloc->AllocData.hostID, pDstAlloc->AllocData.hostID));

        DEBUG_BREAKPOINT_TEST();

        /* Generate empty DMA buffer.
         * Store the command buffer descriptor to pDmaBufferPrivateData.
         */
        GARENDERDATA *pRenderData = NULL;
        uint32_t u32TargetLength = 0;
        uint32_t cbPrivateData = 0;

        if (pPresent->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA))
        {
            /* Fill RenderData description in any case, it will be ignored if the above code failed. */
            pRenderData = (GARENDERDATA *)pPresent->pDmaBufferPrivateData;
            pRenderData->u32DataType  = GARENDERDATA_TYPE_PRESENT;
            pRenderData->cbData       = u32TargetLength;
            pRenderData->pFenceObject = NULL; /* Not a user request, so no user accessible fence object. */
            pRenderData->pvDmaBuffer  = pPresent->pDmaBuffer;
            pRenderData->pHwRenderData = NULL;
            cbPrivateData = sizeof(GARENDERDATA);
        }
        else
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

        switch (Status)
        {
            case STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER:
                if (pRenderData == NULL)
                {
                    /* Not enough space in pDmaBufferPrivateData. */
                    break;
                }
                RT_FALL_THRU();
            case STATUS_SUCCESS:
            {
                pPresent->pDmaBuffer = (uint8_t *)pPresent->pDmaBuffer + u32TargetLength;
                pPresent->pDmaBufferPrivateData = (uint8_t *)pPresent->pDmaBufferPrivateData + cbPrivateData;
            } break;
            default: break;
        }
    }
    else if (pPresent->Flags.Flip)
    {
        PVBOXWDDM_OPENALLOCATION pOa = (PVBOXWDDM_OPENALLOCATION)pSrc->hDeviceSpecificAllocation;
        PVBOXWDDM_ALLOCATION pSrcAllocation = pOa->pAllocation;
        Assert(pSrcAllocation->dx.desc.fPrimary);

        GALOGG(GALOG_GROUP_PRESENT, ("Flip: sid=%u %dx%d\n",
            pSrcAllocation->dx.sid, pSrcAllocation->dx.desc.surfaceInfo.size.width, pSrcAllocation->dx.desc.surfaceInfo.size.height));

        /* Generate DMA buffer containing the present commands.
         * Store the command buffer descriptor to pDmaBufferPrivateData.
         */
        GARENDERDATA *pRenderData = NULL;
        uint32_t u32TargetLength = 0;
        uint32_t cbPrivateData = 0;

        if (pPresent->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA))
        {
            void *pvTarget          = pPresent->pDmaBuffer;
            const uint32_t cbTarget = pPresent->DmaSize;
            /* SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN */
            RECT rect;
            rect.left   = 0;
            rect.top    = 0;
            rect.right  = pSrcAllocation->dx.desc.surfaceInfo.size.width;
            rect.bottom = pSrcAllocation->dx.desc.surfaceInfo.size.height;
            uint32_t const cInClipRects = pPresent->SubRectCnt - pPresent->MultipassOffset;
            uint32_t cOutClipRects = 0;
            Status = SvgaGenBlitSurfaceToScreen(pDevExt->pGa->hw.pSvga,
                                                pSrcAllocation->dx.sid,
                                                &rect,
                                                pSrcAllocation->dx.desc.PrimaryDesc.VidPnSourceId,
                                                &rect,
                                                cInClipRects,
                                                pPresent->pDstSubRects + pPresent->MultipassOffset,
                                                pvTarget, cbTarget, &u32TargetLength, &cOutClipRects);
            if (Status == STATUS_BUFFER_OVERFLOW)
                Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;

            if (Status == STATUS_SUCCESS)
            {
                if (cOutClipRects < cInClipRects)
                {
                    /* Not all rectangles were copied. */
                    Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                }

                /* Advance the current rectangle index. */
                pPresent->MultipassOffset += cOutClipRects;
            }

            /* Fill RenderData description in any case, it will be ignored if the above code failed. */
            pRenderData = (GARENDERDATA *)pPresent->pDmaBufferPrivateData;
            pRenderData->u32DataType  = GARENDERDATA_TYPE_PRESENT;
            pRenderData->cbData       = u32TargetLength;
            pRenderData->pFenceObject = NULL; /* Not a user request, so no user accessible fence object. */
            pRenderData->pvDmaBuffer = pPresent->pDmaBuffer;
            pRenderData->pHwRenderData = NULL;
            cbPrivateData = sizeof(GARENDERDATA);
        }
        else
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

        switch (Status)
        {
            case STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER:
                if (pRenderData == NULL)
                {
                    /* Not enough space in pDmaBufferPrivateData. */
                    break;
                }
                RT_FALL_THRU();
            case STATUS_SUCCESS:
            {
                pPresent->pDmaBuffer = (uint8_t *)pPresent->pDmaBuffer + u32TargetLength;
                pPresent->pDmaBufferPrivateData = (uint8_t *)pPresent->pDmaBufferPrivateData + cbPrivateData;
            } break;
            default: break;
        }
    }
    else if (pPresent->Flags.ColorFill)
    {
        LogRelMax(16, ("ColorFill is not implemented\n"));
        AssertFailed();

        /* Generate empty DMA buffer.
         * Store the command buffer descriptor to pDmaBufferPrivateData.
         */
        GARENDERDATA *pRenderData = NULL;
        uint32_t u32TargetLength = 0;
        uint32_t cbPrivateData = 0;

        if (pPresent->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA))
        {
            /* Fill RenderData description in any case, it will be ignored if the above code failed. */
            pRenderData = (GARENDERDATA *)pPresent->pDmaBufferPrivateData;
            pRenderData->u32DataType  = GARENDERDATA_TYPE_PRESENT;
            pRenderData->cbData       = u32TargetLength;
            pRenderData->pFenceObject = NULL; /* Not a user request, so no user accessible fence object. */
            pRenderData->pvDmaBuffer  = pPresent->pDmaBuffer;
            pRenderData->pHwRenderData = NULL;
            cbPrivateData = sizeof(GARENDERDATA);
        }
        else
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

        switch (Status)
        {
            case STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER:
                if (pRenderData == NULL)
                {
                    /* Not enough space in pDmaBufferPrivateData. */
                    break;
                }
                RT_FALL_THRU();
            case STATUS_SUCCESS:
            {
                pPresent->pDmaBuffer = (uint8_t *)pPresent->pDmaBuffer + u32TargetLength;
                pPresent->pDmaBufferPrivateData = (uint8_t *)pPresent->pDmaBufferPrivateData + cbPrivateData;
            } break;
            default: break;
        }
    }
    else
    {
        WARN(("cmd NOT IMPLEMENTED!! Flags(0x%x)", pPresent->Flags.Value));
        Status = STATUS_NOT_SUPPORTED;
    }

    return Status;
}


static NTSTATUS svgaPagingMapApertureSegment(PVBOXMP_DEVEXT pDevExt, DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer, uint32_t *pcbCommands)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pDevExt->pGa->hw.pSvga;

    /* Define a MOB for the supplied MDL and bind the allocation to the MOB. */

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pBuildPagingBuffer->MapApertureSegment.hAllocation;
    AssertReturn(pAllocation, STATUS_INVALID_PARAMETER);
    AssertReturn(pBuildPagingBuffer->MapApertureSegment.SegmentId == 2, STATUS_INVALID_PARAMETER);

    /** @todo Mobs require locked pages. Could DX provide a Mdl without locked pages? */
    Assert(pBuildPagingBuffer->MapApertureSegment.pMdl->MdlFlags & MDL_PAGES_LOCKED);

    if (pAllocation->dx.mobid != SVGA3D_INVALID_ID)
    {
        AssertFailed();
        return STATUS_SUCCESS;
    }

    PVMSVGAMOB pMob;
    NTSTATUS Status = SvgaMobCreate(pSvga, &pMob,
                                    pBuildPagingBuffer->MapApertureSegment.NumberOfPages,
                                    pBuildPagingBuffer->MapApertureSegment.hAllocation);
    AssertReturn(NT_SUCCESS(Status), Status);

    Status = SvgaMobFillPageTableForMDL(pSvga, pMob, pBuildPagingBuffer->MapApertureSegment.pMdl,
                                        pBuildPagingBuffer->MapApertureSegment.MdlOffset);
    AssertReturnStmt(NT_SUCCESS(Status), SvgaMobFree(pSvga, pMob), Status);

    pAllocation->dx.mobid = VMSVGAMOB_ID(pMob);

    uint32_t cbRequired = sizeof(SVGA3dCmdHeader) + sizeof(SVGA3dCmdDefineGBMob64);
    if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
    {
        cbRequired += sizeof(SVGA3dCmdHeader) + sizeof(SVGA3dCmdBindGBSurface);
        cbRequired += sizeof(SVGA3dCmdHeader) + sizeof(SVGA3dCmdUpdateGBSurface);
    }

    if (pBuildPagingBuffer->DmaSize < cbRequired)
    {
        SvgaMobFree(pSvga, pMob);
        return STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
    }

    /* Add to the container. */
    ExAcquireFastMutex(&pSvga->SvgaMutex);
    RTAvlU32Insert(&pSvga->MobTree, &pMob->core);
    ExReleaseFastMutex(&pSvga->SvgaMutex);

    uint8_t *pu8Cmd = (uint8_t *)pBuildPagingBuffer->pDmaBuffer;

    SVGA3dCmdHeader *pHdr = (SVGA3dCmdHeader *)pu8Cmd;
    pHdr->id   = SVGA_3D_CMD_DEFINE_GB_MOB64;
    pHdr->size = sizeof(SVGA3dCmdDefineGBMob64);
    pu8Cmd += sizeof(*pHdr);

    {
    SVGA3dCmdDefineGBMob64 *pCmd = (SVGA3dCmdDefineGBMob64 *)pu8Cmd;
    pCmd->mobid       = VMSVGAMOB_ID(pMob);
    pCmd->ptDepth     = pMob->enmMobFormat;
    pCmd->base        = pMob->base;
    pCmd->sizeInBytes = pMob->cbMob;
    pu8Cmd += sizeof(*pCmd);
    }

    if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
    {
        /* Bind. */
        pHdr = (SVGA3dCmdHeader *)pu8Cmd;
        pHdr->id   = SVGA_3D_CMD_BIND_GB_SURFACE;
        pHdr->size = sizeof(SVGA3dCmdBindGBSurface);
        pu8Cmd += sizeof(*pHdr);

        {
        SVGA3dCmdBindGBSurface *pCmd = (SVGA3dCmdBindGBSurface *)pu8Cmd;
        pCmd->sid         = pAllocation->dx.sid;
        pCmd->mobid       = VMSVGAMOB_ID(pMob);
        pu8Cmd += sizeof(*pCmd);
        }

        /* Update */
        pHdr = (SVGA3dCmdHeader *)pu8Cmd;
        pHdr->id   = SVGA_3D_CMD_UPDATE_GB_SURFACE;
        pHdr->size = sizeof(SVGA3dCmdUpdateGBSurface);
        pu8Cmd += sizeof(*pHdr);

        {
        SVGA3dCmdUpdateGBSurface *pCmd = (SVGA3dCmdUpdateGBSurface *)pu8Cmd;
        pCmd->sid         = pAllocation->dx.sid;
        pu8Cmd += sizeof(*pCmd);
        }
    }

    *pcbCommands = (uintptr_t)pu8Cmd - (uintptr_t)pBuildPagingBuffer->pDmaBuffer;

    return STATUS_SUCCESS;
}

static NTSTATUS svgaPagingUnmapApertureSegment(PVBOXMP_DEVEXT pDevExt, DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer, uint32_t *pcbCommands)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pDevExt->pGa->hw.pSvga;

    /* Unbind the allocation from the MOB and destroy the MOB which is bound to the allocation. */

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pBuildPagingBuffer->UnmapApertureSegment.hAllocation;
    AssertReturn(pAllocation, STATUS_INVALID_PARAMETER);
    AssertReturn(pBuildPagingBuffer->UnmapApertureSegment.SegmentId == 2, STATUS_INVALID_PARAMETER);

    if (pAllocation->dx.mobid == SVGA3D_INVALID_ID)
    {
        DEBUG_BREAKPOINT_TEST();
        return STATUS_SUCCESS;
    }

    /* Find the mob. */
    ExAcquireFastMutex(&pSvga->SvgaMutex);
    PVMSVGAMOB pMob = (PVMSVGAMOB)RTAvlU32Get(&pSvga->MobTree, pAllocation->dx.mobid);
    ExReleaseFastMutex(&pSvga->SvgaMutex);
    AssertReturn(pMob, STATUS_INVALID_PARAMETER);

    uint32_t cbRequired = sizeof(SVGA3dCmdHeader) + sizeof(SVGA3dCmdDestroyGBMob);
    if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
    {
        cbRequired += sizeof(SVGA3dCmdHeader) + sizeof(SVGA3dCmdBindGBSurface);
    }

    if (pBuildPagingBuffer->DmaSize < cbRequired)
    {
        SvgaMobFree(pSvga, pMob);
        return STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
    }

    uint8_t *pu8Cmd = (uint8_t *)pBuildPagingBuffer->pDmaBuffer;

    SVGA3dCmdHeader *pHdr;
    if (pAllocation->dx.desc.enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
    {
        /* Unbind. */
        pHdr = (SVGA3dCmdHeader *)pu8Cmd;
        pHdr->id   = SVGA_3D_CMD_BIND_GB_SURFACE;
        pHdr->size = sizeof(SVGA3dCmdBindGBSurface);
        pu8Cmd += sizeof(*pHdr);

        {
        SVGA3dCmdBindGBSurface *pCmd = (SVGA3dCmdBindGBSurface *)pu8Cmd;
        pCmd->sid   = pAllocation->dx.sid;
        pCmd->mobid = SVGA3D_INVALID_ID;
        pu8Cmd += sizeof(*pCmd);
        }
    }

    pHdr = (SVGA3dCmdHeader *)pu8Cmd;
    pHdr->id   = SVGA_3D_CMD_DESTROY_GB_MOB;
    pHdr->size = sizeof(SVGA3dCmdDestroyGBMob);
    pu8Cmd += sizeof(*pHdr);

    {
    SVGA3dCmdDestroyGBMob *pCmd = (SVGA3dCmdDestroyGBMob *)pu8Cmd;
    pCmd->mobid = VMSVGAMOB_ID(pMob);
    pu8Cmd += sizeof(*pCmd);
    }

    *pcbCommands = (uintptr_t)pu8Cmd - (uintptr_t)pBuildPagingBuffer->pDmaBuffer;

    SvgaMobFree(pSvga, pMob);
    pAllocation->dx.mobid = SVGA3D_INVALID_ID;

    return STATUS_SUCCESS;
}


NTSTATUS DxgkDdiDXBuildPagingBuffer(PVBOXMP_DEVEXT pDevExt, DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer)
{
    AssertReturn(pBuildPagingBuffer->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA), STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER);

    NTSTATUS Status = STATUS_SUCCESS;
    uint32_t cbCommands = 0;
    switch (pBuildPagingBuffer->Operation)
    {
        case DXGK_OPERATION_TRANSFER:
        {
            DEBUG_BREAKPOINT_TEST();
            break;
        }
        case DXGK_OPERATION_FILL:
        {
            PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pBuildPagingBuffer->Fill.hAllocation;
            RT_NOREF(pAllocation);
            DEBUG_BREAKPOINT_TEST();
            break;
        }
        case DXGK_OPERATION_DISCARD_CONTENT:
        {
            DEBUG_BREAKPOINT_TEST();
            break;
        }
        case DXGK_OPERATION_MAP_APERTURE_SEGMENT:
        {
            Status = svgaPagingMapApertureSegment(pDevExt, pBuildPagingBuffer, &cbCommands);
            break;
        }
        case DXGK_OPERATION_UNMAP_APERTURE_SEGMENT:
        {
            Status = svgaPagingUnmapApertureSegment(pDevExt, pBuildPagingBuffer, &cbCommands);
            break;
        }
        default:
            AssertFailedStmt(Status = STATUS_NOT_IMPLEMENTED);
    }

    /* Fill RenderData description in any case, it will be ignored if the above code failed. */
    GARENDERDATA *pRenderData = (GARENDERDATA *)pBuildPagingBuffer->pDmaBufferPrivateData;
    pRenderData->u32DataType   = GARENDERDATA_TYPE_PAGING;
    pRenderData->cbData        = cbCommands;
    pRenderData->pFenceObject  = NULL;
    pRenderData->pvDmaBuffer   = pBuildPagingBuffer->pDmaBuffer;
    pRenderData->pHwRenderData = NULL;

    switch (Status)
    {
        case STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER:
            AssertFailed(); /** @todo test */
            RT_FALL_THRU();
        case STATUS_SUCCESS:
        {
            pBuildPagingBuffer->pDmaBuffer = (uint8_t *)pBuildPagingBuffer->pDmaBuffer + cbCommands;
            pBuildPagingBuffer->pDmaBufferPrivateData = (uint8_t *)pBuildPagingBuffer->pDmaBufferPrivateData + sizeof(GARENDERDATA);
        } break;
        default: break;
    }

    return Status;
}


NTSTATUS APIENTRY DxgkDdiDXPatch(PVBOXMP_DEVEXT pDevExt, const DXGKARG_PATCH *pPatch)
{
    GALOG(("pDmaBuffer = %p, cbDmaBuffer = %u, cPatches = %u\n",
           pPatch->pDmaBuffer, pPatch->DmaBufferSubmissionEndOffset - pPatch->DmaBufferSubmissionStartOffset,
           pPatch->PatchLocationListSubmissionLength));

    DEBUG_BREAKPOINT_TEST();

    for (UINT i = 0; i < pPatch->PatchLocationListSubmissionLength; ++i)
    {
        D3DDDI_PATCHLOCATIONLIST const *pPatchListEntry
            = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart + i];
        void * const pPatchAddress = (uint8_t *)pPatch->pDmaBuffer + pPatchListEntry->PatchOffset;
        VBOXDXALLOCATIONTYPE const enmAllocationType = (VBOXDXALLOCATIONTYPE)pPatchListEntry->DriverId;

        /* Ignore a dummy patch request. */
        if (pPatchListEntry->PatchOffset == ~0UL)
            continue;

        AssertReturn(   pPatchListEntry->PatchOffset >= pPatch->DmaBufferSubmissionStartOffset
                     && pPatchListEntry->PatchOffset < pPatch->DmaBufferSubmissionEndOffset, STATUS_INVALID_PARAMETER);
        AssertReturn(pPatchListEntry->AllocationIndex < pPatch->AllocationListSize, STATUS_INVALID_PARAMETER);

        DXGK_ALLOCATIONLIST const *pAllocationListEntry = &pPatch->pAllocationList[pPatchListEntry->AllocationIndex];
        AssertContinue(pAllocationListEntry->SegmentId != 0);

        PVBOXWDDM_OPENALLOCATION pOA = (PVBOXWDDM_OPENALLOCATION)pAllocationListEntry->hDeviceSpecificAllocation;
        if (pOA)
        {
            PVBOXWDDM_ALLOCATION pAllocation = pOA->pAllocation;
            /* Allocation type determines what the patch is about. */
            Assert(pAllocation->dx.desc.enmAllocationType == enmAllocationType);
            if (enmAllocationType == VBOXDXALLOCATIONTYPE_SURFACE)
            {
                Assert(pAllocation->dx.sid != SVGA3D_INVALID_ID);
                *(uint32_t *)pPatchAddress = pAllocation->dx.sid;
            }
            else if (enmAllocationType == VBOXDXALLOCATIONTYPE_SHADERS)
            {
                Assert(pAllocation->dx.mobid != SVGA3D_INVALID_ID);
                *(uint32_t *)pPatchAddress = pAllocation->dx.mobid;
            }
            else
            {
                AssertFailed();
                uint32_t *poffVRAM = (uint32_t *)pPatchAddress;
                *poffVRAM = pAllocationListEntry->PhysicalAddress.LowPart + pPatchListEntry->AllocationOffset;
            }
        }
        else
            AssertFailed(); /* Render should have already filtered out such patches. */
    }

#ifdef DEBUG
    if (!pPatch->Flags.Paging && !pPatch->Flags.Present)
    {
        SvgaDebugCommandsD3D(pDevExt->pGa->hw.pSvga,
                             ((PVBOXWDDM_CONTEXT)pPatch->hContext)->pSvgaContext,
                             (uint8_t *)pPatch->pDmaBuffer + pPatch->DmaBufferSubmissionStartOffset,
                             pPatch->DmaBufferSubmissionEndOffset - pPatch->DmaBufferSubmissionStartOffset);
    }
#else
    RT_NOREF(pDevExt);
#endif
    return STATUS_SUCCESS;
}
