/* $Id$ */
/** @file
 * VirtualBox Windows Guest Mesa3D - Gallium driver interface for WDDM kernel mode driver.
 */

/*
 * Copyright (C) 2016-2020 Oracle Corporation
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

#include <iprt/time.h>

void GaAdapterStop(PVBOXMP_DEVEXT pDevExt)
{
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;
    GALOG(("pDevExt = %p, pDevExt->pGa = %p\n", pDevExt, pGaDevExt));

    if (pGaDevExt)
    {
        if (!RTListIsEmpty(&pGaDevExt->listHwRenderData))
        {
            GAHWRENDERDATA *pIter, *pNext;
            RTListForEachSafe(&pGaDevExt->listHwRenderData, pIter, pNext, GAHWRENDERDATA, node)
            {
                /* Delete the data. SvgaRenderComplete deallocates pIter. */
                RTListNodeRemove(&pIter->node);
                SvgaRenderComplete(pGaDevExt->hw.pSvga, pIter);
            }
        }

        /* Free fence objects. */
        GaFenceObjectsDestroy(pGaDevExt, NULL);

        if (pGaDevExt->hw.pSvga)
        {
            SvgaAdapterStop(pGaDevExt->hw.pSvga, &pDevExt->u.primary.DxgkInterface);
            pGaDevExt->hw.pSvga = NULL;
        }

        GaMemFree(pGaDevExt);
        pDevExt->pGa = NULL;
    }
}

NTSTATUS GaAdapterStart(PVBOXMP_DEVEXT pDevExt)
{
    GALOG(("pDevExt = %p\n", pDevExt));

    NTSTATUS Status;

    if (pDevExt->enmHwType == VBOXVIDEO_HWTYPE_VMSVGA)
    {
        VBOXWDDM_EXT_GA *pGaDevExt = (VBOXWDDM_EXT_GA *)GaMemAllocZero(sizeof(*pGaDevExt));
        if (pGaDevExt)
        {
            RTListInit(&pGaDevExt->listHwRenderData);

            /* Init fence objects. */
            pGaDevExt->fenceObjects.u32SeqNoSource = 0;
            RTListInit(&pGaDevExt->fenceObjects.list);

            KeInitializeSpinLock(&pGaDevExt->fenceObjects.SpinLock);
            RT_ZERO(pGaDevExt->fenceObjects.au32HandleBits);
            ASMBitSet(pGaDevExt->fenceObjects.au32HandleBits, 0); /* Exclude id==0, it is for NULL. */

            /* Start hardware. */
            Status = SvgaAdapterStart(&pGaDevExt->hw.pSvga, &pDevExt->u.primary.DxgkInterface,
                                      pDevExt->HwResources.phFIFO, pDevExt->HwResources.cbFIFO,
                                      pDevExt->HwResources.phIO, pDevExt->HwResources.cbIO);
            if (Status == STATUS_SUCCESS)
            {
                pDevExt->pGa = pGaDevExt;
            }
        }
        else
        {
            Status = STATUS_INSUFFICIENT_RESOURCES;
        }
    }
    else
    {
        Status = STATUS_NOT_SUPPORTED;
    }

    if (Status != STATUS_SUCCESS)
    {
        GaAdapterStop(pDevExt);
    }

    return Status;
}

NTSTATUS GaQueryInfo(PVBOXWDDM_EXT_GA pGaDevExt,
                     VBOXVIDEO_HWTYPE enmHwType,
                     VBOXGAHWINFO *pHWInfo)
{
    NTSTATUS Status = STATUS_SUCCESS;

    switch (enmHwType)
    {
        case VBOXVIDEO_HWTYPE_VMSVGA:
            pHWInfo->u32HwType = VBOX_GA_HW_TYPE_VMSVGA;
            break;
        default:
            Status = STATUS_NOT_SUPPORTED;
    }

    if (NT_SUCCESS(Status))
    {
        pHWInfo->u32Reserved = 0;
        RT_ZERO(pHWInfo->u.au8Raw);

        if (pHWInfo->u32HwType == VBOX_GA_HW_TYPE_VMSVGA)
        {
            Status = SvgaQueryInfo(pGaDevExt->hw.pSvga, &pHWInfo->u.svga);
        }
        else
        {
            Status = STATUS_NOT_SUPPORTED;
        }
    }

    return Status;
}

NTSTATUS GaDeviceCreate(PVBOXWDDM_EXT_GA pGaDevExt,
                        PVBOXWDDM_DEVICE pDevice)
{
    RT_NOREF2(pGaDevExt, pDevice);
    return STATUS_SUCCESS;
}

void GaDeviceDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                     PVBOXWDDM_DEVICE pDevice)
{
    /* Free fence objects and GMRs. This is useful when the application has crashed.. */
    GaFenceObjectsDestroy(pGaDevExt, pDevice);
    SvgaRegionsDestroy(pGaDevExt->hw.pSvga, pDevice);
}

NTSTATUS GaContextCreate(PVBOXWDDM_EXT_GA pGaDevExt,
                         PVBOXWDDM_CREATECONTEXT_INFO pInfo,
                         PVBOXWDDM_CONTEXT pContext)
{
    AssertReturn(pContext->NodeOrdinal == 0, STATUS_NOT_SUPPORTED);

    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    uint32_t u32Cid;
    NTSTATUS Status = SvgaContextIdAlloc(pSvga, &u32Cid);
    if (NT_SUCCESS(Status))
    {
        Status = SvgaContextCreate(pSvga, u32Cid);
        if (Status == STATUS_SUCCESS)
        {
            /** @todo Save other pInfo fields. */
            RT_NOREF(pInfo);

            /* Init pContext fields, which are relevant to the gallium context. */
            pContext->u32Cid = u32Cid;

            GALOG(("pGaDevExt = %p, cid = %d\n", pGaDevExt, u32Cid));
        }
        else
        {
            SvgaContextIdFree(pSvga, u32Cid);
        }
    }

    return Status;
}

NTSTATUS GaContextDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                          PVBOXWDDM_CONTEXT pContext)
{
    GALOG(("u32Cid = %d\n", pContext->u32Cid));

    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;

    SvgaContextDestroy(pSvga, pContext->u32Cid);

    return SvgaContextIdFree(pSvga, pContext->u32Cid);
}

NTSTATUS GaUpdate(PVBOXWDDM_EXT_GA pGaDevExt,
                  uint32_t u32X,
                  uint32_t u32Y,
                  uint32_t u32Width,
                  uint32_t u32Height)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaUpdate(pSvga, u32X, u32Y, u32Width, u32Height);
}

NTSTATUS GaDefineCursor(PVBOXWDDM_EXT_GA pGaDevExt,
                        uint32_t u32HotspotX,
                        uint32_t u32HotspotY,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        uint32_t u32AndMaskDepth,
                        uint32_t u32XorMaskDepth,
                        void const *pvAndMask,
                        uint32_t cbAndMask,
                        void const *pvXorMask,
                        uint32_t cbXorMask)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaDefineCursor(pSvga, u32HotspotX, u32HotspotY, u32Width, u32Height,
                            u32AndMaskDepth, u32XorMaskDepth,
                            pvAndMask, cbAndMask, pvXorMask, cbXorMask);
}

NTSTATUS GaDefineAlphaCursor(PVBOXWDDM_EXT_GA pGaDevExt,
                             uint32_t u32HotspotX,
                             uint32_t u32HotspotY,
                             uint32_t u32Width,
                             uint32_t u32Height,
                             void const *pvImage,
                             uint32_t cbImage)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaDefineAlphaCursor(pSvga, u32HotspotX, u32HotspotY, u32Width, u32Height,
                                 pvImage, cbImage);
}

static NTSTATUS gaSurfaceDefine(PVBOXWDDM_EXT_GA pGaDevExt,
                                GASURFCREATE *pCreateParms,
                                GASURFSIZE *paSizes,
                                uint32_t cSizes,
                                uint32_t *pu32Sid)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaSurfaceCreate(pSvga, pCreateParms, paSizes, cSizes, pu32Sid);
}

static NTSTATUS gaSurfaceDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                                 uint32_t u32Sid)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaSurfaceUnref(pSvga, u32Sid);
}

NTSTATUS GaScreenDefine(PVBOXWDDM_EXT_GA pGaDevExt,
                        uint32_t u32Offset,
                        uint32_t u32ScreenId,
                        int32_t xOrigin,
                        int32_t yOrigin,
                        uint32_t u32Width,
                        uint32_t u32Height,
                        bool fBlank)
{
    return SvgaScreenDefine(pGaDevExt->hw.pSvga, u32Offset, u32ScreenId, xOrigin, yOrigin, u32Width, u32Height, fBlank);
}

NTSTATUS GaScreenDestroy(PVBOXWDDM_EXT_GA pGaDevExt,
                         uint32_t u32ScreenId)
{
    return SvgaScreenDestroy(pGaDevExt->hw.pSvga, u32ScreenId);
}

static NTSTATUS gaSharedSidInsert(PVBOXWDDM_EXT_GA pGaDevExt,
                                  uint32_t u32Sid,
                                  uint32_t u32SharedSid)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaSharedSidInsert(pSvga, u32Sid, u32SharedSid);
}

static NTSTATUS gaSharedSidRemove(PVBOXWDDM_EXT_GA pGaDevExt,
                                  uint32_t u32Sid)
{
    VBOXWDDM_EXT_VMSVGA *pSvga = pGaDevExt->hw.pSvga;
    return SvgaSharedSidRemove(pSvga, u32Sid);
}

static NTSTATUS gaPresent(PVBOXWDDM_EXT_GA pGaDevExt,
                          uint32_t u32Sid,
                          uint32_t u32Width,
                          uint32_t u32Height,
                          uint32_t u32VRAMOffset)
{
    return SvgaPresentVRAM(pGaDevExt->hw.pSvga, u32Sid, u32Width, u32Height, u32VRAMOffset);
}

static int gaFenceCmp(uint32_t u32FenceA, uint32_t u32FenceB)
{
     if (   u32FenceA < u32FenceB
         || u32FenceA - u32FenceB > UINT32_MAX / 2)
     {
         return -1; /* FenceA is newer than FenceB. */
     }
     else if (u32FenceA == u32FenceB)
     {
         /* FenceA is equal to FenceB. */
         return 0;
     }

     /* FenceA is older than FenceB. */
     return 1;
}


static void dxgkNotifyDma(DXGKRNL_INTERFACE *pDxgkInterface,
                          DXGK_INTERRUPT_TYPE enmType,
                          UINT uNodeOrdinal,
                          UINT uFenceId,
                          UINT uLastCompletedFenceId)
{
    DXGKARGCB_NOTIFY_INTERRUPT_DATA notify;
    memset(&notify, 0, sizeof(notify));

    GALOG(("%d fence %d\n", enmType, uFenceId));
    switch (enmType)
    {
        case DXGK_INTERRUPT_DMA_COMPLETED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_COMPLETED;
            notify.DmaCompleted.SubmissionFenceId = uFenceId;
            notify.DmaCompleted.NodeOrdinal = uNodeOrdinal;
            break;

        case DXGK_INTERRUPT_DMA_PREEMPTED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_PREEMPTED;
            notify.DmaPreempted.PreemptionFenceId = uFenceId;
            notify.DmaPreempted.NodeOrdinal = uNodeOrdinal;
            notify.DmaPreempted.LastCompletedFenceId = uLastCompletedFenceId;
            break;

        case DXGK_INTERRUPT_DMA_FAULTED:
            notify.InterruptType = DXGK_INTERRUPT_DMA_FAULTED;
            notify.DmaFaulted.FaultedFenceId = uFenceId;
            notify.DmaFaulted.Status = STATUS_UNSUCCESSFUL;
            notify.DmaFaulted.NodeOrdinal = uNodeOrdinal;
            break;

        default:
            WARN(("completion type %d", enmType));
            break;
    }

    if (notify.InterruptType)
    {
        pDxgkInterface->DxgkCbNotifyInterrupt(pDxgkInterface->DeviceHandle, &notify);
        GALOG(("notified\n"));
    }
}

static void gaReportFence(PVBOXMP_DEVEXT pDevExt)
{
    /* Runs at device interrupt IRQL. */
    Assert(KeGetCurrentIrql() > DISPATCH_LEVEL);

    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;
    AssertReturnVoid(pGaDevExt);

    PVBOXWDDM_EXT_VMSVGA pSvga = pGaDevExt->hw.pSvga;
    AssertReturnVoid(pSvga);

    /* Read the last completed fence from the device. */
    const uint32_t u32Fence = SVGAFifoRead(pSvga, SVGA_FIFO_FENCE);
    GALOG(("Fence %u\n", u32Fence));

    if (u32Fence == ASMAtomicReadU32(&pGaDevExt->u32PreemptionFenceId))
    {
        ASMAtomicWriteU32(&pGaDevExt->u32PreemptionFenceId, 0);

        const uint32_t u32LastSubmittedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastSubmittedFenceId);
        ASMAtomicWriteU32(&pGaDevExt->u32LastCompletedFenceId, u32LastSubmittedFenceId);

        dxgkNotifyDma(&pDevExt->u.primary.DxgkInterface, DXGK_INTERRUPT_DMA_PREEMPTED,
                      0, u32Fence, u32LastSubmittedFenceId);

        /* Notify DXGK about the updated DMA fence. */
        pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
    }
    else
    {
        /* Check if we already reported it. */
        const uint32_t u32LastCompletedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastCompletedFenceId);
        if (gaFenceCmp(u32LastCompletedFenceId, u32Fence) < 0)
        {
            /* u32Fence is newer. */
            ASMAtomicWriteU32(&pGaDevExt->u32LastCompletedFenceId, u32Fence);

            dxgkNotifyDma(&pDevExt->u.primary.DxgkInterface, DXGK_INTERRUPT_DMA_COMPLETED,
                          0, u32Fence, u32Fence);

            /* Notify DXGK about the updated DMA fence. */
            pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
        }
    }
}

/*
 * Description of DMA buffer content.
 * These structures are stored in DmaBufferPrivateData.
 */
typedef struct GARENDERDATA
{
    uint32_t      u32DataType;    /* GARENDERDATA_TYPE_* */
    uint32_t      cbData;         /* How many bytes. */
    GAFENCEOBJECT *pFenceObject;  /* User mode fence associated with this command buffer. */
    void          *pvDmaBuffer;   /* Pointer to the DMA buffer. */
    GAHWRENDERDATA *pHwRenderData; /* The hardware module private data. */
} GARENDERDATA;

#define GARENDERDATA_TYPE_RENDER   1
#define GARENDERDATA_TYPE_PRESENT  2
#define GARENDERDATA_TYPE_PAGING   3
#define GARENDERDATA_TYPE_FENCE    4

/* If there are no commands but we need to trigger fence submission anyway, then submit a buffer of this size. */
#define GA_DMA_MIN_SUBMIT_SIZE 4
AssertCompile(GA_DMA_MIN_SUBMIT_SIZE < sizeof(SVGA3dCmdHeader));

DECLINLINE(PVBOXWDDM_ALLOCATION) vboxWddmGetAllocationFromAllocList(DXGK_ALLOCATIONLIST *pAllocList)
{
    PVBOXWDDM_OPENALLOCATION pOa = (PVBOXWDDM_OPENALLOCATION)pAllocList->hDeviceSpecificAllocation;
    return pOa? pOa->pAllocation: NULL;
}

static NTSTATUS gaGMRFBToVRAMSurface(DXGKARG_PRESENT *pPresent,
                                     PVBOXWDDM_EXT_VMSVGA pSvga,
                                     uint32_t idxAllocation,
                                     DXGK_ALLOCATIONLIST *pAllocationList,
                                     PVBOXWDDM_ALLOCATION pAllocation,
                                     uint8_t *pu8Target, uint32_t cbTarget, uint32_t *pu32TargetOut)
{
    NTSTATUS Status = SvgaGenDefineGMRFB(pSvga,
                                         pAllocationList->SegmentId != 0 ?
                                             pAllocationList->PhysicalAddress.LowPart : 0,
                                         pAllocation->AllocData.SurfDesc.pitch,
                                         pu8Target, cbTarget, pu32TargetOut);
    if (Status == STATUS_SUCCESS)
    {
        /* Always tell WDDM that the SHADOWSURFACE must be "paged in". */
        UINT PatchOffset =   (uintptr_t)pu8Target - (uintptr_t)pPresent->pDmaBuffer
                           + sizeof(uint32_t)
                           + RT_UOFFSETOF(SVGAFifoCmdDefineGMRFB, ptr.offset);

        memset(pPresent->pPatchLocationListOut, 0, sizeof(D3DDDI_PATCHLOCATIONLIST));
        pPresent->pPatchLocationListOut->AllocationIndex = idxAllocation;
        pPresent->pPatchLocationListOut->PatchOffset = PatchOffset;
        ++pPresent->pPatchLocationListOut;
    }
    return Status;
}

/** Generate commands for Blt case.
 *
 * @return Status code.
 * @param pPresent          Information about rectangles to blit.
 * @param pSvga             VMSVGA device extension.
 * @param pSrc              Source allocation description.
 * @param pSrcAlloc         Allocation to blit from.
 * @param pDst              Destination allocation description.
 * @param pDstAlloc         Allocation to blit to.
 * @param pu8Target         Command buffer to fill.
 * @param cbTarget          Size of command buffer.
 * @param pu32TargetOut     Where to store size of generated commands.
 */
static NTSTATUS gaPresentBlt(DXGKARG_PRESENT *pPresent,
                             PVBOXWDDM_EXT_VMSVGA pSvga,
                             DXGK_ALLOCATIONLIST *pSrc,
                             PVBOXWDDM_ALLOCATION pSrcAlloc,
                             DXGK_ALLOCATIONLIST *pDst,
                             PVBOXWDDM_ALLOCATION pDstAlloc,
                             uint8_t *pu8Target, uint32_t cbTarget, uint32_t *pu32TargetOut)
{
    NTSTATUS Status = STATUS_SUCCESS;

    uint8_t * const pu8TargetStart = pu8Target;

    uint32_t cbCmd = 0;

    /** @todo One subrect at a time for now, consider passing all pDstSubRects when possible,
     * for example in one BlitSurfaceToScreen.
     */
    uint32_t iSubRect;
    for (iSubRect = pPresent->MultipassOffset; iSubRect < pPresent->SubRectCnt; ++iSubRect)
    {
        /* DstSubRects are in Dst coords.
         * To calculate corresponding SrcSubRect:
         *    srcsub = src + (dstsub - dst) = dstsub + (src - dst).
         * Precompute the src - dst differences to use in the code below.
         */
        int32_t const dx = pPresent->SrcRect.left - pPresent->DstRect.left;
        int32_t const dy = pPresent->SrcRect.top  - pPresent->DstRect.top;

        if (iSubRect == 0)
        {
            if (   pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
                || pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE)
            {
                /* Define GMRFB to point to the shadow/staging surface. */
                Status = gaGMRFBToVRAMSurface(pPresent, pSvga,
                                              DXGK_PRESENT_SOURCE_INDEX, pSrc, pSrcAlloc,
                                              pu8Target, cbTarget, &cbCmd);
            }
            else if (   pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
                     || pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE)
            {
                /* Define GMRFB to point to the shadow/staging surface. */
                Status = gaGMRFBToVRAMSurface(pPresent, pSvga,
                                              DXGK_PRESENT_DESTINATION_INDEX, pDst, pDstAlloc,
                                              pu8Target, cbTarget, &cbCmd);
            }

            if (Status == STATUS_BUFFER_OVERFLOW)
            {
                Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                break;
            }

            pu8Target += cbCmd;
            cbTarget -= cbCmd;
        }

        if (pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
        {
            /* To screen. */
            if (   pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
                || pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE)
            {
                /* From GDI software drawing surface. */
                GALOGG(GALOG_GROUP_PRESENT, ("Blt: %s(%d) 0x%08X -> SHAREDPRIMARYSURFACE 0x%08X\n",
                    vboxWddmAllocTypeString(pSrcAlloc),
                    pSrcAlloc->enmType, pSrc->PhysicalAddress.LowPart, pDst->PhysicalAddress.LowPart));

                int32_t const xSrc = pPresent->pDstSubRects[iSubRect].left + dx;
                int32_t const ySrc = pPresent->pDstSubRects[iSubRect].top  + dy;
                Status = SvgaGenBlitGMRFBToScreen(pSvga,
                                                  pDstAlloc->AllocData.SurfDesc.VidPnSourceId,
                                                  xSrc, ySrc,
                                                  &pPresent->pDstSubRects[iSubRect],
                                                  pu8Target, cbTarget, &cbCmd);
            }
            else if (pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC)
            {
                /* From a surface. */
                GALOGG(GALOG_GROUP_PRESENT, ("Blt: surface sid=%u -> SHAREDPRIMARYSURFACE 0x%08X\n",
                    pSrcAlloc->AllocData.hostID, pDst->PhysicalAddress.LowPart));

                RECT const dstRect = pPresent->pDstSubRects[iSubRect];
                RECT srcRect;
                srcRect.left   = dstRect.left   + dx;
                srcRect.top    = dstRect.top    + dy;
                srcRect.right  = dstRect.right  + dx;
                srcRect.bottom = dstRect.bottom + dy;
                RECT clipRect = dstRect;
                Status = SvgaGenBlitSurfaceToScreen(pSvga,
                                                    pSrcAlloc->AllocData.hostID,
                                                    &srcRect,
                                                    pDstAlloc->AllocData.SurfDesc.VidPnSourceId,
                                                    &dstRect,
                                                    1, &clipRect,
                                                    pu8Target, cbTarget, &cbCmd, NULL);
            }
            else
            {
                AssertFailed();
            }
        }
        else if (  pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
                || pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE)
        {
            /* To GDI software drawing surface. */
            if (pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
            {
                /* From screen. */
                GALOGG(GALOG_GROUP_PRESENT, ("Blt: SHAREDPRIMARYSURFACE 0x%08X -> %s(%d) 0x%08X\n",
                    pSrc->PhysicalAddress.LowPart,
                    vboxWddmAllocTypeString(pDstAlloc),
                    pDstAlloc->enmType, pDst->PhysicalAddress.LowPart));

                int32_t const xSrc = pPresent->pDstSubRects[iSubRect].left + dx;
                int32_t const ySrc = pPresent->pDstSubRects[iSubRect].top  + dy;

                Status = SvgaGenBlitScreenToGMRFB(pSvga,
                                                  pSrcAlloc->AllocData.SurfDesc.VidPnSourceId,
                                                  xSrc, ySrc,
                                                  &pPresent->pDstSubRects[iSubRect],
                                                  pu8Target, cbTarget, &cbCmd);
            }
            else if (pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC)
            {
                /* From a surface. */
                GALOGG(GALOG_GROUP_PRESENT, ("Blt: surface sid=%u -> %s(%d) %d:0x%08X\n",
                    pSrcAlloc->AllocData.hostID,
                    vboxWddmAllocTypeString(pDstAlloc),
                    pDstAlloc->enmType, pDst->SegmentId, pDst->PhysicalAddress.LowPart));

                SVGAGuestImage guestImage;
                guestImage.ptr.gmrId  = SVGA_GMR_FRAMEBUFFER;
                guestImage.ptr.offset = pDst->SegmentId != 0 ?  pDst->PhysicalAddress.LowPart : 0;
                guestImage.pitch      = pDstAlloc->AllocData.SurfDesc.pitch;

                SVGA3dSurfaceImageId surfId;
                surfId.sid    = pSrcAlloc->AllocData.hostID;
                surfId.face   = 0;
                surfId.mipmap = 0;

                int32_t const xSrc = pPresent->pDstSubRects[iSubRect].left + dx;
                int32_t const ySrc = pPresent->pDstSubRects[iSubRect].top  + dy;

                Status = SvgaGenSurfaceDMA(pSvga,
                                           &guestImage, &surfId, SVGA3D_READ_HOST_VRAM,
                                           xSrc, ySrc,
                                           pPresent->pDstSubRects[iSubRect].left,
                                           pPresent->pDstSubRects[iSubRect].top,
                                           pPresent->pDstSubRects[iSubRect].right - pPresent->pDstSubRects[iSubRect].left,
                                           pPresent->pDstSubRects[iSubRect].bottom - pPresent->pDstSubRects[iSubRect].top,
                                           pu8Target, cbTarget, &cbCmd);
                if (Status == STATUS_SUCCESS)
                {
                    /* Always tell WDDM that the SHADOWSURFACE must be "paged in". */
                    UINT PatchOffset =   (uintptr_t)pu8Target - (uintptr_t)pPresent->pDmaBuffer
                                       + sizeof(SVGA3dCmdHeader)
                                       + RT_UOFFSETOF(SVGA3dCmdSurfaceDMA, guest.ptr.offset);

                    memset(pPresent->pPatchLocationListOut, 0, sizeof(D3DDDI_PATCHLOCATIONLIST));
                    pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
                    pPresent->pPatchLocationListOut->PatchOffset = PatchOffset;
                    ++pPresent->pPatchLocationListOut;
                }
            }
            else
            {
                AssertFailed();
            }
        }
        else
            AssertFailed();

        if (Status == STATUS_BUFFER_OVERFLOW)
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
            break;
        }

        pu8Target += cbCmd;
        cbTarget -= cbCmd;
    }

    *pu32TargetOut = (uintptr_t)pu8Target - (uintptr_t)pu8TargetStart;

    if (Status == STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER)
    {
        pPresent->MultipassOffset = iSubRect;
    }

    return Status;
}

NTSTATUS APIENTRY GaDxgkDdiPresent(const HANDLE hContext,
                                   DXGKARG_PRESENT *pPresent)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;

    DXGK_ALLOCATIONLIST *pSrc =  &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
    DXGK_ALLOCATIONLIST *pDst =  &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];

    GALOGG(GALOG_GROUP_PRESENT, ("%s: [%ld, %ld, %ld, %ld] -> [%ld, %ld, %ld, %ld] (SubRectCnt=%u)\n",
        pPresent->Flags.Blt ? "Blt" : (pPresent->Flags.Flip ? "Flip" : (pPresent->Flags.ColorFill ? "ColorFill" : "Unknown OP")),
        pPresent->SrcRect.left, pPresent->SrcRect.top, pPresent->SrcRect.right, pPresent->SrcRect.bottom,
        pPresent->DstRect.left, pPresent->DstRect.top, pPresent->DstRect.right, pPresent->DstRect.bottom,
        pPresent->SubRectCnt));
    if (GALOG_ENABLED(GALOG_GROUP_PRESENT))
        for (unsigned int i = 0; i < pPresent->SubRectCnt; ++i)
            GALOGG(GALOG_GROUP_PRESENT, ("   sub#%u = [%ld, %ld, %ld, %ld]\n",
                    i, pPresent->pDstSubRects[i].left, pPresent->pDstSubRects[i].top, pPresent->pDstSubRects[i].right, pPresent->pDstSubRects[i].bottom));

    if (pPresent->Flags.Blt)
    {
        PVBOXWDDM_ALLOCATION pSrcAlloc = vboxWddmGetAllocationFromAllocList(pSrc);
        PVBOXWDDM_ALLOCATION pDstAlloc = vboxWddmGetAllocationFromAllocList(pDst);

        GALOGG(GALOG_GROUP_PRESENT, ("Blt: sid=%x -> sid=%x\n", pSrcAlloc->AllocData.hostID, pDstAlloc->AllocData.hostID));

        /** @todo Review standard allocations (DxgkDdiGetStandardAllocationDriverData, etc).
         *        Probably can be used more naturally with VMSVGA.
         */

        /** @todo Merge common code for all branches (Blt, Flip, ColorFill) */

        /* Generate DMA buffer containing the SVGA_CMD_BLIT_GMRFB_TO_SCREEN commands.
         * Store the command buffer descriptor to pDmaBufferPrivateData.
         */
        GARENDERDATA *pRenderData = NULL;
        uint32_t u32TargetLength = 0;
        uint32_t cbPrivateData = 0;

        if (pPresent->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA))
        {
            uint8_t *pu8Target = (uint8_t *)pPresent->pDmaBuffer;
            uint32_t cbTarget = pPresent->DmaSize;

            Status = gaPresentBlt(pPresent, pDevExt->pGa->hw.pSvga, pSrc, pSrcAlloc, pDst, pDstAlloc,
                                  pu8Target, cbTarget, &u32TargetLength);

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
        PVBOXWDDM_ALLOCATION pSrcAlloc = vboxWddmGetAllocationFromAllocList(pSrc);

        GALOGG(GALOG_GROUP_PRESENT, ("Flip: %s sid=%u %dx%d\n",
            vboxWddmAllocTypeString(pSrcAlloc),
            pSrcAlloc->AllocData.hostID, pSrcAlloc->AllocData.SurfDesc.width, pSrcAlloc->AllocData.SurfDesc.height));

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
            if (cbTarget > GA_DMA_MIN_SUBMIT_SIZE)
            {
#if 1
                /* SVGA_3D_CMD_BLIT_SURFACE_TO_SCREEN */
                RECT rect;
                rect.left   = 0;
                rect.top    = 0;
                rect.right  = pSrcAlloc->AllocData.SurfDesc.width;
                rect.bottom = pSrcAlloc->AllocData.SurfDesc.height;
                uint32_t const cInClipRects = pPresent->SubRectCnt - pPresent->MultipassOffset;
                uint32_t cOutClipRects = 0;
                Status = SvgaGenBlitSurfaceToScreen(pDevExt->pGa->hw.pSvga,
                                                    pSrcAlloc->AllocData.hostID,
                                                    &rect,
                                                    pSrcAlloc->AllocData.SurfDesc.VidPnSourceId,
                                                    &rect,
                                                    cInClipRects,
                                                    pPresent->pDstSubRects + pPresent->MultipassOffset,
                                                    pvTarget, cbTarget, &u32TargetLength, &cOutClipRects);
                if (Status == STATUS_BUFFER_OVERFLOW)
                {
                    Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                }

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
#else
                /* This tests the SVGA_3D_CMD_PRESENT command. */
                RT_NOREF(pDevExt);
                Status = SvgaGenPresent(pSrcAlloc->AllocData.hostID,
                                        pSrcAlloc->AllocData.SurfDesc.width,
                                        pSrcAlloc->AllocData.SurfDesc.height,
                                        pvTarget, cbTarget, &u32TargetLength);
                if (Status == STATUS_BUFFER_OVERFLOW)
                {
                    Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                }
#endif
            }
            else
            {
                Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
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

NTSTATUS APIENTRY GaDxgkDdiRender(const HANDLE hContext, DXGKARG_RENDER *pRender)
{
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;

    AssertReturn(pContext && pContext->enmType == VBOXWDDM_CONTEXT_TYPE_GA_3D, STATUS_INVALID_PARAMETER);
    AssertReturn(pRender->CommandLength > pRender->MultipassOffset, STATUS_INVALID_PARAMETER);
    /* Expect 32 bit handle at the start of the command buffer. */
    AssertReturn(pRender->CommandLength >= sizeof(uint32_t), STATUS_INVALID_PARAMETER);

    GARENDERDATA *pRenderData = NULL;  /* Pointer to the DMA buffer description. */
    uint32_t cbPrivateData = 0;        /* Bytes to place into the private data buffer. */
    uint32_t u32TargetLength = 0;      /* Bytes to place into the DMA buffer. */
    uint32_t u32ProcessedLength = 0;   /* Bytes consumed from command buffer. */

    GALOG(("[%p] Command %p/%d, Dma %p/%d, Private %p/%d, MO %d, S %d, Phys 0x%RX64, AL %p/%d, PLLIn %p/%d, PLLOut %p/%d\n",
           hContext,
           pRender->pCommand, pRender->CommandLength,
           pRender->pDmaBuffer, pRender->DmaSize,
           pRender->pDmaBufferPrivateData, pRender->DmaBufferPrivateDataSize,
           pRender->MultipassOffset, pRender->DmaBufferSegmentId, pRender->DmaBufferPhysicalAddress.QuadPart,
           pRender->pAllocationList, pRender->AllocationListSize,
           pRender->pPatchLocationListIn, pRender->PatchLocationListInSize,
           pRender->pPatchLocationListOut, pRender->PatchLocationListOutSize
         ));

    /* 32 bit handle at the start of the command buffer. */
    if (pRender->MultipassOffset == 0)
        pRender->MultipassOffset += sizeof(uint32_t);

    NTSTATUS Status = STATUS_SUCCESS;
    __try
    {
        /* Calculate where the commands start. */
        void const *pvSource = (uint8_t *)pRender->pCommand + pRender->MultipassOffset;
        uint32_t cbSource = pRender->CommandLength - pRender->MultipassOffset;

        /* Generate DMA buffer from the supplied command buffer.
         * Store the command buffer descriptor to pDmaBufferPrivateData.
         *
         * The display miniport driver must validate the command buffer.
         *
         * Copy commands to the pDmaBuffer.
         * If a command uses a shared surface id, then replace the id with the original surface id.
         */
        if (pRender->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA))
        {
            void *pvTarget          = pRender->pDmaBuffer;
            uint32_t const cbTarget = pRender->DmaSize;
            GAHWRENDERDATA *pHwRenderData = NULL;
            if (cbTarget > GA_DMA_MIN_SUBMIT_SIZE)
            {
                Status = SvgaRenderCommands(pGaDevExt->hw.pSvga, pvTarget, cbTarget, pvSource, cbSource,
                                            &u32TargetLength, &u32ProcessedLength, &pHwRenderData);
            }
            else
            {
                Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
            }

            GAFENCEOBJECT *pFO = NULL;
            if (Status == STATUS_SUCCESS)
            {
                /* Completed the command buffer. Check if there is a user mode fence. */
                uint32_t const u32FenceHandle = *(uint32_t *)pRender->pCommand;
                if (u32FenceHandle != 0)
                {
                    /* Verify that the buffer handle is valid. */
                    gaFenceObjectsLock(pGaDevExt);
                    pFO = GaFenceLookup(pGaDevExt, u32FenceHandle);
                    gaFenceObjectsUnlock(pGaDevExt);

                    if (!pFO) // Maybe silently ignore?
                    {
                        AssertFailed();
                        Status = STATUS_INVALID_PARAMETER;
                    }
                }

                GALOG(("u32FenceHandle = %d, pFO = %p\n", u32FenceHandle, pFO));
            }

            /* Fill RenderData description in any case, it will be ignored if the above code failed. */
            pRenderData = (GARENDERDATA *)pRender->pDmaBufferPrivateData;
            pRenderData->u32DataType  = GARENDERDATA_TYPE_RENDER;
            pRenderData->cbData       = u32TargetLength;
            pRenderData->pFenceObject = pFO;
            pRenderData->pvDmaBuffer  = pRender->pDmaBuffer;
            pRenderData->pHwRenderData = pHwRenderData;
            cbPrivateData = sizeof(GARENDERDATA);
        }
        else
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

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
            if (pRenderData == NULL)
            {
                /* Not enough space in pDmaBufferPrivateData. */
                break;
            }
            RT_FALL_THRU();
        case STATUS_SUCCESS:
        {
            Assert(pRenderData);
            if (u32TargetLength == 0)
            {
                /* Trigger command submission anyway by increasing pDmaBuffer */
                u32TargetLength = GA_DMA_MIN_SUBMIT_SIZE;

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

static NTSTATUS gaSoftwarePagingTransfer(PVBOXMP_DEVEXT pDevExt,
                                         DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer)
{
    RT_NOREF2(pDevExt, pBuildPagingBuffer);
    /** @todo Implement.
     * Do the SysMem <-> VRAM transfer in software, because
     * the VMSVGA device does not have appropriate commands.
     */
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY GaDxgkDdiBuildPagingBuffer(const HANDLE hAdapter,
                                             DXGKARG_BUILDPAGINGBUFFER *pBuildPagingBuffer)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;

    GALOG(("DmaBufferPrivateData %p/%d, DmaBuffer %p/%d\n",
           pBuildPagingBuffer->pDmaBufferPrivateData,
           pBuildPagingBuffer->DmaBufferPrivateDataSize,
           pBuildPagingBuffer->pDmaBuffer,
           pBuildPagingBuffer->DmaSize));

    /* Generate DMA buffer containing the commands.
     * Store the command buffer descriptor pointer to pDmaBufferPrivateData.
     */
    GARENDERDATA *pRenderData = NULL;
    uint32_t u32TargetLength = 0;
    uint32_t cbPrivateData = 0;

    if (pBuildPagingBuffer->DmaBufferPrivateDataSize >= sizeof(GARENDERDATA))
    {
        // void *pvTarget          = pBuildPagingBuffer->pDmaBuffer;
        const uint32_t cbTarget = pBuildPagingBuffer->DmaSize;
        if (cbTarget > GA_DMA_MIN_SUBMIT_SIZE)
        {
            switch (pBuildPagingBuffer->Operation)
            {
                case DXGK_OPERATION_TRANSFER:
                {
                    GALOG(("DXGK_OPERATION_TRANSFER: %p: @0x%x, cb 0x%x; src: %d:%p; dst: %d:%p; flags 0x%x, off 0x%x\n",
                           pBuildPagingBuffer->Transfer.hAllocation,
                           pBuildPagingBuffer->Transfer.TransferOffset,
                           pBuildPagingBuffer->Transfer.TransferSize,
                           pBuildPagingBuffer->Transfer.Source.SegmentId,
                           pBuildPagingBuffer->Transfer.Source.pMdl,
                           pBuildPagingBuffer->Transfer.Destination.SegmentId,
                           pBuildPagingBuffer->Transfer.Destination.pMdl,
                           pBuildPagingBuffer->Transfer.Flags.Value,
                           pBuildPagingBuffer->Transfer.MdlOffset));
                    if (pBuildPagingBuffer->Transfer.Source.SegmentId == 0)
                    {
                        /* SysMem source. */
                        if (pBuildPagingBuffer->Transfer.Destination.SegmentId == 1)
                        {
                            /* SysMem -> VRAM. */
                            Status = gaSoftwarePagingTransfer(pDevExt, pBuildPagingBuffer);
                            if (Status == STATUS_SUCCESS)
                            {
                                /* Generate a NOP. */
                                Status = STATUS_NOT_SUPPORTED;
                            }
                        }
                        else if (pBuildPagingBuffer->Transfer.Destination.SegmentId == 0)
                        {
                            /* SysMem -> SysMem, should not happen, bugcheck. */
                            AssertFailed();
                            Status = STATUS_INVALID_PARAMETER;
                        }
                        else
                        {
                            /* SysMem -> GPU surface. Our driver probably does not need it.
                             * SVGA_3D_CMD_SURFACE_DMA(GMR -> Surface)?
                             */
                            AssertFailed();
                            Status = STATUS_NOT_SUPPORTED;
                        }
                    }
                    else if (pBuildPagingBuffer->Transfer.Source.SegmentId == 1)
                    {
                        /* VRAM source. */
                        if (pBuildPagingBuffer->Transfer.Destination.SegmentId == 0)
                        {
                            /* VRAM -> SysMem. */
                            Status = gaSoftwarePagingTransfer(pDevExt, pBuildPagingBuffer);
                            if (Status == STATUS_SUCCESS)
                            {
                                /* Generate a NOP. */
                                Status = STATUS_NOT_SUPPORTED;
                            }
                        }
                        else if (pBuildPagingBuffer->Transfer.Destination.SegmentId == 1)
                        {
                            /* VRAM -> VRAM, should not happen, bugcheck. */
                            AssertFailed();
                            Status = STATUS_INVALID_PARAMETER;
                        }
                        else
                        {
                            /* VRAM -> GPU surface. Our driver probably does not need it.
                             * SVGA_3D_CMD_SURFACE_DMA(SVGA_GMR_FRAMEBUFFER -> Surface)?
                             */
                            AssertFailed();
                            Status = STATUS_NOT_SUPPORTED;
                        }
                    }
                    else
                    {
                        /* GPU surface. Our driver probably does not need it.
                         * SVGA_3D_CMD_SURFACE_DMA(Surface -> GMR)?
                         */
                        AssertFailed();
                        Status = STATUS_NOT_SUPPORTED;
                    }

                    /** @todo Ignore for now. */
                    if (Status == STATUS_NOT_SUPPORTED)
                    {
                        /* NOP */
                        Status = STATUS_SUCCESS;
                    }
                } break;

                case DXGK_OPERATION_FILL:
                {
                    GALOG(("DXGK_OPERATION_FILL: %p: cb 0x%x, pattern 0x%x, %d:0x%08X\n",
                           pBuildPagingBuffer->Fill.hAllocation,
                           pBuildPagingBuffer->Fill.FillSize,
                           pBuildPagingBuffer->Fill.FillPattern,
                           pBuildPagingBuffer->Fill.Destination.SegmentId,
                           pBuildPagingBuffer->Fill.Destination.SegmentAddress.LowPart));
                    /* NOP */
                } break;

                case DXGK_OPERATION_DISCARD_CONTENT:
                {
                    GALOG(("DXGK_OPERATION_DISCARD_CONTENT: %p: flags 0x%x, %d:0x%08X\n",
                           pBuildPagingBuffer->DiscardContent.hAllocation,
                           pBuildPagingBuffer->DiscardContent.Flags,
                           pBuildPagingBuffer->DiscardContent.SegmentId,
                           pBuildPagingBuffer->DiscardContent.SegmentAddress.LowPart));
                    /* NOP */
                } break;

                default:
                    AssertFailed();
                    break;
            }
        }
        else
        {
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }

        /* Fill RenderData description in any case, it will be ignored if the above code failed. */
        pRenderData = (GARENDERDATA *)pBuildPagingBuffer->pDmaBufferPrivateData;
        pRenderData->u32DataType  = GARENDERDATA_TYPE_PAGING;
        pRenderData->cbData       = u32TargetLength;
        pRenderData->pFenceObject = NULL; /* Not a user request, so no user accessible fence object. */
        pRenderData->pvDmaBuffer = pBuildPagingBuffer->pDmaBuffer;
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
            AssertFailed(); /** @todo test */
            if (pRenderData == NULL)
            {
                /* Not enough space in pDmaBufferPrivateData. */
                break;
            }
            RT_FALL_THRU();
        case STATUS_SUCCESS:
        {
            pBuildPagingBuffer->pDmaBuffer = (uint8_t *)pBuildPagingBuffer->pDmaBuffer + u32TargetLength;
            pBuildPagingBuffer->pDmaBufferPrivateData = (uint8_t *)pBuildPagingBuffer->pDmaBufferPrivateData + cbPrivateData;
        } break;
        default: break;
    }

    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY GaDxgkDdiPatch(const HANDLE hAdapter, const DXGKARG_PATCH *pPatch)
{
    RT_NOREF2(hAdapter, pPatch);
    /* Nothing to do. */

    uint8_t *pu8DMABuffer = (uint8_t *)pPatch->pDmaBuffer + pPatch->DmaBufferSubmissionStartOffset;
    UINT const cbDMABuffer = pPatch->DmaBufferSubmissionEndOffset - pPatch->DmaBufferSubmissionStartOffset;

    UINT i;
    for (i = pPatch->PatchLocationListSubmissionStart; i < pPatch->PatchLocationListSubmissionLength; ++i)
    {
        D3DDDI_PATCHLOCATIONLIST const *pPatchList = &pPatch->pPatchLocationList[i];
        Assert(pPatchList->AllocationIndex < pPatch->AllocationListSize);

        DXGK_ALLOCATIONLIST const *pAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
        if (pAllocationList->SegmentId == 0)
        {
            WARN(("no segment id specified"));
            continue;
        }

        Assert(pAllocationList->SegmentId == 1);            /* CPU visible segment. */
        Assert(pAllocationList->PhysicalAddress.HighPart == 0); /* The segment is less than 4GB. */
        Assert(!(pAllocationList->PhysicalAddress.QuadPart & 0xfffUL)); /* <- just a check to ensure allocation offset does not go here */

        if (pPatchList->PatchOffset == ~0UL)
        {
            /* This is a dummy patch request, ignore. */
            continue;
        }

        if (pPatchList->PatchOffset >= cbDMABuffer) /// @todo A better condition.
        {
            WARN(("pPatchList->PatchOffset(%d) >= cbDMABuffer(%d)", pPatchList->PatchOffset, cbDMABuffer));
            return STATUS_INVALID_PARAMETER;
        }

        uint32_t *poffVRAM = (uint32_t *)(pu8DMABuffer + pPatchList->PatchOffset);
        *poffVRAM = pAllocationList->PhysicalAddress.LowPart + pPatchList->AllocationOffset;
    }

    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY GaDxgkDdiSubmitCommand(const HANDLE hAdapter, const DXGKARG_SUBMITCOMMAND *pSubmitCommand)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pSubmitCommand->hContext;
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;

    GALOG(("pContext %p, fence %d\n", pContext, pSubmitCommand->SubmissionFenceId));

    const uint32_t cbPrivateData = pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset;
    void *pvPrivateData = (uint8_t *)pSubmitCommand->pDmaBufferPrivateData + pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset;

    GALOG(("DmaBuffer (fence %d): End %d, Start %d\n",
           pSubmitCommand->SubmissionFenceId, pSubmitCommand->DmaBufferSubmissionEndOffset,
           pSubmitCommand->DmaBufferSubmissionStartOffset));
    GALOG(("PrivateData (fence %d): End %d, Start %d, cb %d\n",
           pSubmitCommand->SubmissionFenceId, pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset,
           pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset, cbPrivateData));

    uint32_t cbDmaBufferSubmission = pSubmitCommand->DmaBufferSubmissionEndOffset - pSubmitCommand->DmaBufferSubmissionStartOffset;
    uint32_t cDataBlocks = cbPrivateData / sizeof(GARENDERDATA);

    if (cDataBlocks == 0)
    {
        /* Sometimes a zero sized paging buffer is submitted.
         * Seen this on W10.17763 right after DXGK_OPERATION_DISCARD_CONTENT.
         * Can not ignore such block, since a new SubmissionFenceId is passed.
         * Try to handle it by emitting the fence command only.
         */
        Assert(cbPrivateData == 0);
        Assert(pSubmitCommand->Flags.Paging);
        LogRelMax(16, ("WDDM: empty buffer: cbPrivateData %d, flags 0x%x\n", cbPrivateData, pSubmitCommand->Flags.Value));
    }

    GARENDERDATA const *pRenderData = (GARENDERDATA *)pvPrivateData;
    while (cDataBlocks--)
    {
        AssertReturn(cbDmaBufferSubmission >= pRenderData->cbData, STATUS_INVALID_PARAMETER);
        cbDmaBufferSubmission -= pRenderData->cbData;

        void *pvDmaBuffer = NULL;
        if (   pRenderData->u32DataType == GARENDERDATA_TYPE_RENDER
            || pRenderData->u32DataType == GARENDERDATA_TYPE_FENCE)
        {
            if (pRenderData->u32DataType == GARENDERDATA_TYPE_RENDER)
            {
                pvDmaBuffer = pRenderData->pvDmaBuffer;
                AssertPtrReturn(pvDmaBuffer, STATUS_INVALID_PARAMETER);
            }

            GAFENCEOBJECT * const pFO = pRenderData->pFenceObject;
            if (pFO) /* Can be NULL if the user mode driver does not need the fence for this buffer. */
            {
                GALOG(("pFO = %p, u32FenceHandle = %d, Fence = %d\n",
                       pFO, pFO->u32FenceHandle, pSubmitCommand->SubmissionFenceId));

                gaFenceObjectsLock(pGaDevExt);

                Assert(pFO->u32FenceState == GAFENCE_STATE_IDLE);
                pFO->u32SubmissionFenceId = pSubmitCommand->SubmissionFenceId;
                pFO->u32FenceState = GAFENCE_STATE_SUBMITTED;
                pFO->u64SubmittedTS = RTTimeNanoTS();

                gaFenceObjectsUnlock(pGaDevExt);
            }
        }
        else if (pRenderData->u32DataType == GARENDERDATA_TYPE_PRESENT)
        {
            pvDmaBuffer = pRenderData->pvDmaBuffer;
            AssertPtrReturn(pvDmaBuffer, STATUS_INVALID_PARAMETER);
        }
        else if (pRenderData->u32DataType == GARENDERDATA_TYPE_PAGING)
        {
            pvDmaBuffer = pRenderData->pvDmaBuffer;
            AssertPtrReturn(pvDmaBuffer, STATUS_INVALID_PARAMETER);
        }
        else
        {
            AssertFailedReturn(STATUS_INVALID_PARAMETER);
        }

        if (pvDmaBuffer)
        {
            Assert(pSubmitCommand->DmaBufferSegmentId == 0);

            uint32_t const cbSubmit = pRenderData->cbData;
            if (cbSubmit)
            {
                /* Copy DmaBuffer to Fifo. */
                void *pvCmd = SvgaFifoReserve(pGaDevExt->hw.pSvga, cbSubmit);
                AssertPtrReturn(pvCmd, STATUS_INSUFFICIENT_RESOURCES);

                /* pvDmaBuffer is the actual address of the current data block.
                 * Therefore do not use pSubmitCommand->DmaBufferSubmissionStartOffset here.
                 */
                memcpy(pvCmd, pvDmaBuffer, cbSubmit);
                SvgaFifoCommit(pGaDevExt->hw.pSvga, cbSubmit);
            }
            else
            {
                /* 'Paging' buffers can be empty, implementation is incomplete. See GaDxgkDdiBuildPagingBuffer. */
                if (pSubmitCommand->Flags.Paging == 0)
                {
                    LogRelMax(16, ("WDDM: Zero sized command buffer. Flags 0x%x, type %d\n", pSubmitCommand->Flags.Value, pRenderData->u32DataType));
                    AssertFailed();
                }
            }
        }

        if (pRenderData->pHwRenderData)
        {
            GAHWRENDERDATA * const pHwRenderData = pRenderData->pHwRenderData;
            pHwRenderData->u32SubmissionFenceId = pSubmitCommand->SubmissionFenceId;
            pHwRenderData->u32Reserved = 0;

            KIRQL OldIrql;
            SvgaHostObjectsLock(pGaDevExt->hw.pSvga, &OldIrql);
            RTListAppend(&pGaDevExt->listHwRenderData, &pHwRenderData->node);
            SvgaHostObjectsUnlock(pGaDevExt->hw.pSvga, OldIrql);
        }

        ++pRenderData;
    }

    ASMAtomicWriteU32(&pGaDevExt->u32LastSubmittedFenceId, pSubmitCommand->SubmissionFenceId);

    /* Submit the fence. */
    SvgaFence(pGaDevExt->hw.pSvga, pSubmitCommand->SubmissionFenceId);

    GALOG(("done %d\n", pSubmitCommand->SubmissionFenceId));
    return STATUS_SUCCESS;
}

BOOLEAN GaDxgkDdiInterruptRoutine(const PVOID MiniportDeviceContext,
                                  ULONG MessageNumber)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;
    RT_NOREF(MessageNumber);

    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;
    if (!pGaDevExt)
    {
        /* Device is not initialized yet. Not a Gallium interrupt, "return FALSE immediately". */
        return FALSE;
    }

    PVBOXWDDM_EXT_VMSVGA pSvga = pGaDevExt->hw.pSvga;
    if (!pSvga)
    {
        /* Device is not initialized yet. Not a VMSVGA interrupt, "return FALSE immediately". */
        return FALSE;
    }

    const uint32_t u32IrqStatus = SVGAPortRead(pSvga, SVGA_IRQSTATUS_PORT);
    if (!u32IrqStatus)
    {
        /* Not a VMSVGA interrupt, "return FALSE immediately". */
        return FALSE;
    }

    /* "Dismiss the interrupt on the adapter." */
    SVGAPortWrite(pSvga, SVGA_IRQSTATUS_PORT, u32IrqStatus);
    GALOG(("u32IrqStatus = 0x%08X\n", u32IrqStatus));

    /* Check what happened. */
    if (u32IrqStatus & SVGA_IRQFLAG_ANY_FENCE)
    {
        /* A SVGA_CMD_FENCE command has been processed by the device. */
        gaReportFence(pDevExt);
    }

    pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

    GALOG(("leave\n"));
    /* "Return TRUE as quickly as possible". */
    return TRUE;
}

VOID GaDxgkDdiDpcRoutine(const PVOID MiniportDeviceContext)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;
    if (!pGaDevExt)
    {
        /* Device is not initialized yet. */
        return;
    }

    PVBOXWDDM_EXT_VMSVGA pSvga = pGaDevExt->hw.pSvga;
    if (!pSvga)
    {
        /* Device is not initialized yet. */
        return;
    }

    /* Scan fence objects and mark all with u32FenceId < u32LastCompletedFenceId as SIGNALED */
    const uint32_t u32LastCompletedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastCompletedFenceId);

    gaFenceObjectsLock(pGaDevExt);

    GAFENCEOBJECT *pIter, *pNext;
    RTListForEachSafe(&pGaDevExt->fenceObjects.list, pIter, pNext, GAFENCEOBJECT, node)
    {
        if (pIter->u32FenceState == GAFENCE_STATE_SUBMITTED)
        {
            if (gaFenceCmp(pIter->u32SubmissionFenceId, u32LastCompletedFenceId) <= 0)
            {
                GALOG(("u32SubmissionFenceId %u -> SIGNALED %RU64 ns\n",
                       pIter->u32SubmissionFenceId, RTTimeNanoTS() - pIter->u64SubmittedTS));

                ASMAtomicWriteU32(&pGaDevExt->u32LastCompletedSeqNo, pIter->u32SeqNo);
                pIter->u32FenceState = GAFENCE_STATE_SIGNALED;
                if (RT_BOOL(pIter->fu32FenceFlags & GAFENCE_F_WAITED))
                {
                    KeSetEvent(&pIter->event, 0, FALSE);
                }

                GaFenceUnrefLocked(pGaDevExt, pIter);
            }
        }
    }

    gaFenceObjectsUnlock(pGaDevExt);

    KIRQL OldIrql;
    SvgaHostObjectsLock(pSvga, &OldIrql);

    /* Move the completed render data objects to the local list under the lock. */
    RTLISTANCHOR listHwRenderData;
    RTListInit(&listHwRenderData);

    if (!RTListIsEmpty(&pGaDevExt->listHwRenderData))
    {
        GAHWRENDERDATA *pIter, *pNext;
        RTListForEachSafe(&pGaDevExt->listHwRenderData, pIter, pNext, GAHWRENDERDATA, node)
        {
            if (gaFenceCmp(pIter->u32SubmissionFenceId, u32LastCompletedFenceId) <= 0)
            {
                RTListNodeRemove(&pIter->node);
                RTListAppend(&listHwRenderData, &pIter->node);
            }
        }
    }

    SvgaHostObjectsUnlock(pSvga, OldIrql);

    if (!RTListIsEmpty(&listHwRenderData))
    {
        GAHWRENDERDATA *pIter, *pNext;
        RTListForEachSafe(&listHwRenderData, pIter, pNext, GAHWRENDERDATA, node)
        {
            /* Delete the data. SvgaRenderComplete deallocates pIter. */
            RTListNodeRemove(&pIter->node);
            SvgaRenderComplete(pSvga, pIter);
        }
    }
}

typedef struct GAPREEMPTCOMMANDCBCTX
{
    PVBOXMP_DEVEXT pDevExt;
    UINT uPreemptionFenceId;
    UINT uLastCompletedFenceId;
} GAPREEMPTCOMMANDCBCTX;

static BOOLEAN gaPreemptCommandCb(PVOID Context)
{
    GAPREEMPTCOMMANDCBCTX *pCtx = (GAPREEMPTCOMMANDCBCTX *)Context;
    dxgkNotifyDma(&pCtx->pDevExt->u.primary.DxgkInterface, DXGK_INTERRUPT_DMA_PREEMPTED,
                  0, pCtx->uPreemptionFenceId, pCtx->uLastCompletedFenceId);
    return TRUE;
}

NTSTATUS APIENTRY GaDxgkDdiPreemptCommand(const HANDLE hAdapter,
                                          const DXGKARG_PREEMPTCOMMAND *pPreemptCommand)
{
    NTSTATUS Status;

    GALOG(("hAdapter %p, fence %d\n", hAdapter, pPreemptCommand->PreemptionFenceId));

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;
    if (!pGaDevExt)
    {
        /* Device is not initialized yet. */
        return STATUS_SUCCESS;
    }

    /* We can not safely remove submitted data from FIFO, so just let the host process all submitted commands.
     */
    const uint32_t u32LastCompletedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastCompletedFenceId);
    const uint32_t u32LastSubmittedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastSubmittedFenceId);
    if (u32LastCompletedFenceId == u32LastSubmittedFenceId)
    {
        GAPREEMPTCOMMANDCBCTX Ctx;
        Ctx.pDevExt = pDevExt;
        Ctx.uPreemptionFenceId = pPreemptCommand->PreemptionFenceId;
        Ctx.uLastCompletedFenceId = u32LastCompletedFenceId;

        DXGKRNL_INTERFACE *pDxgkInterface = &pDevExt->u.primary.DxgkInterface;
        BOOLEAN bReturnValue = FALSE;
        Status = pDxgkInterface->DxgkCbSynchronizeExecution(pDxgkInterface->DeviceHandle,
                                                            gaPreemptCommandCb, &Ctx, 0, &bReturnValue);
        Assert(bReturnValue);
    }
    else
    {
        /* Submit the fence. */
        Assert(pGaDevExt->u32PreemptionFenceId == 0);
        ASMAtomicWriteU32(&pGaDevExt->u32PreemptionFenceId, pPreemptCommand->PreemptionFenceId);
        Status = SvgaFence(pGaDevExt->hw.pSvga, pPreemptCommand->PreemptionFenceId);
    }

    return Status;
}

static BOOLEAN gaQueryCurrentFenceCb(PVOID Context)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)Context;
    gaReportFence(pDevExt);
    return TRUE;
}

NTSTATUS APIENTRY GaDxgkDdiQueryCurrentFence(const HANDLE hAdapter,
                                             DXGKARG_QUERYCURRENTFENCE *pCurrentFence)
{
    NTSTATUS Status;

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    VBOXWDDM_EXT_GA *pGaDevExt = pDevExt->pGa;
    if (!pGaDevExt)
    {
        /* Device is not initialized yet. */
        return STATUS_SUCCESS;
    }

    DXGKRNL_INTERFACE *pDxgkInterface = &pDevExt->u.primary.DxgkInterface;
    LARGE_INTEGER DelayInterval;
    DelayInterval.QuadPart = -10LL * 1000LL * 1000LL;
    uint32_t u32LastCompletedFenceId = 0;

    /* Wait until the host processes all submitted buffers to allow delays on the host (debug, etc). */
    for (;;)
    {
        BOOLEAN bReturnValue = FALSE;
        Status = pDxgkInterface->DxgkCbSynchronizeExecution(pDxgkInterface->DeviceHandle,
                                                            gaQueryCurrentFenceCb, pDevExt, 0, &bReturnValue);
        Assert(bReturnValue);
        if (Status != STATUS_SUCCESS)
        {
            break;
        }

        u32LastCompletedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastCompletedFenceId);
        uint32_t const u32LastSubmittedFenceId = ASMAtomicReadU32(&pGaDevExt->u32LastSubmittedFenceId);
        if (u32LastCompletedFenceId == u32LastSubmittedFenceId)
        {
            break;
        }

        GALOG(("hAdapter %p, LastCompletedFenceId %d, LastSubmittedFenceId %d...\n", hAdapter, u32LastCompletedFenceId, u32LastSubmittedFenceId));

        KeDelayExecutionThread(KernelMode, FALSE, &DelayInterval);
    }

    if (Status == STATUS_SUCCESS)
    {
        pCurrentFence->CurrentFence = u32LastCompletedFenceId;
    }

    GALOG(("hAdapter %p, CurrentFence %d, Status 0x%x\n", hAdapter, pCurrentFence->CurrentFence, Status));

    return Status;
}

NTSTATUS APIENTRY GaDxgkDdiEscape(const HANDLE hAdapter,
                                  const DXGKARG_ESCAPE *pEscape)
{
    if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE))
    {
        AssertFailed();
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)pEscape->hDevice;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
    const VBOXDISPIFESCAPE *pEscapeHdr = (VBOXDISPIFESCAPE *)pEscape->pPrivateDriverData;
    switch (pEscapeHdr->escapeCode)
    {
        case VBOXESC_GAGETCID:
        {
            if (!pContext)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAGETCID))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAGETCID *pGaGetCid = (VBOXDISPIFESCAPE_GAGETCID *)pEscapeHdr;
            pGaGetCid->u32Cid = pContext->u32Cid;
            Status = STATUS_SUCCESS;
            break;
        }
        case VBOXESC_GAREGION:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAREGION))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAREGION *pGaRegion = (VBOXDISPIFESCAPE_GAREGION *)pEscapeHdr;
            if (pGaRegion->u32Command == GA_REGION_CMD_CREATE)
            {
                Status = SvgaRegionCreate(pDevExt->pGa->hw.pSvga, pDevice, pGaRegion->u32NumPages, &pGaRegion->u32GmrId, &pGaRegion->u64UserAddress);
            }
            else if (pGaRegion->u32Command == GA_REGION_CMD_DESTROY)
            {
                Status = SvgaRegionDestroy(pDevExt->pGa->hw.pSvga, pGaRegion->u32GmrId);
            }
            else
            {
                Status = STATUS_INVALID_PARAMETER;
            }
        } break;
        case VBOXESC_GAPRESENT:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAPRESENT))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAPRESENT *pGaPresent = (VBOXDISPIFESCAPE_GAPRESENT *)pEscapeHdr;
            /** @todo This always writes to the start of VRAM. This is a debug function
             * and is not used for normal operations anymore.
             */
            Status = gaPresent(pDevExt->pGa, pGaPresent->u32Sid, pGaPresent->u32Width, pGaPresent->u32Height, 0);
            break;
        }
        case VBOXESC_GASURFACEDEFINE:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GASURFACEDEFINE))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GASURFACEDEFINE *pGaSurfaceDefine = (VBOXDISPIFESCAPE_GASURFACEDEFINE *)pEscapeHdr;
            if (pEscape->PrivateDriverDataSize - sizeof(VBOXDISPIFESCAPE_GASURFACEDEFINE) < pGaSurfaceDefine->cbReq)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            GASURFCREATE *pCreateParms = (GASURFCREATE *)&pGaSurfaceDefine[1];
            GASURFSIZE *paSizes = (GASURFSIZE *)&pCreateParms[1];

            /// @todo verify the data
            Status = gaSurfaceDefine(pDevExt->pGa, pCreateParms, paSizes, pGaSurfaceDefine->cSizes, &pGaSurfaceDefine->u32Sid);
            break;
        }
        case VBOXESC_GASURFACEDESTROY:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GASURFACEDESTROY))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GASURFACEDESTROY *pGaSurfaceDestroy = (VBOXDISPIFESCAPE_GASURFACEDESTROY *)pEscapeHdr;
            Status = gaSurfaceDestroy(pDevExt->pGa, pGaSurfaceDestroy->u32Sid);
            break;
        }
        case VBOXESC_GASHAREDSID:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GASHAREDSID))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GASHAREDSID *pGaSharedSid = (VBOXDISPIFESCAPE_GASHAREDSID *)pEscapeHdr;
            if (pGaSharedSid->u32SharedSid == ~0)
            {
                Status = gaSharedSidRemove(pDevExt->pGa, pGaSharedSid->u32Sid);
            }
            else
            {
                Status = gaSharedSidInsert(pDevExt->pGa, pGaSharedSid->u32Sid, pGaSharedSid->u32SharedSid);
            }
            break;
        }
        case VBOXESC_GAFENCECREATE:
        {
            if (!pDevice)
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAFENCECREATE))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAFENCECREATE *pFenceCreate = (VBOXDISPIFESCAPE_GAFENCECREATE *)pEscapeHdr;
            Status = GaFenceCreate(pDevExt->pGa, pDevice, &pFenceCreate->u32FenceHandle);
            break;
        }
        case VBOXESC_GAFENCEQUERY:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAFENCEQUERY))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAFENCEQUERY *pFenceQuery = (VBOXDISPIFESCAPE_GAFENCEQUERY *)pEscapeHdr;
            Status = GaFenceQuery(pDevExt->pGa, pFenceQuery->u32FenceHandle,
                                  &pFenceQuery->u32SubmittedSeqNo, &pFenceQuery->u32ProcessedSeqNo,
                                  &pFenceQuery->u32FenceStatus);
            break;
        }
        case VBOXESC_GAFENCEWAIT:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAFENCEWAIT))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAFENCEWAIT *pFenceWait = (VBOXDISPIFESCAPE_GAFENCEWAIT *)pEscapeHdr;
            Status = GaFenceWait(pDevExt->pGa, pFenceWait->u32FenceHandle, pFenceWait->u32TimeoutUS);
            break;
        }
        case VBOXESC_GAFENCEUNREF:
        {
            if (pEscape->PrivateDriverDataSize < sizeof(VBOXDISPIFESCAPE_GAFENCEUNREF))
            {
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            VBOXDISPIFESCAPE_GAFENCEUNREF *pFenceUnref = (VBOXDISPIFESCAPE_GAFENCEUNREF *)pEscapeHdr;
            Status = GaFenceUnref(pDevExt->pGa, pFenceUnref->u32FenceHandle);
            break;
        }
        default:
            break;
    }

    return Status;
}

DECLINLINE(VBOXVIDEOOFFSET) vboxWddmAddrVRAMOffset(VBOXWDDM_ADDR const *pAddr)
{
    return (pAddr->offVram != VBOXVIDEOOFFSET_VOID && pAddr->SegmentId) ?
                (pAddr->SegmentId == 1 ? pAddr->offVram : 0) :
                VBOXVIDEOOFFSET_VOID;
}

static void vboxWddmRectCopy(void *pvDst, uint32_t cbDstBytesPerPixel, uint32_t cbDstPitch,
                             void const *pvSrc, uint32_t cbSrcBytesPerPixel, uint32_t cbSrcPitch,
                             RECT const *pRect)
{
    uint8_t *pu8Dst = (uint8_t *)pvDst;
    pu8Dst += pRect->top * cbDstPitch + pRect->left * cbDstBytesPerPixel;

    uint8_t const *pu8Src = (uint8_t *)pvSrc;
    pu8Src += pRect->top * cbSrcPitch + pRect->left * cbSrcBytesPerPixel;

    uint32_t const cbLine = (pRect->right - pRect->left) * cbDstBytesPerPixel;
    for (INT y = pRect->top; y < pRect->bottom; ++y)
    {
        memcpy(pu8Dst, pu8Src, cbLine);
        pu8Dst += cbDstPitch;
        pu8Src += cbSrcPitch;
    }
}

static NTSTATUS gaSourceBlitToScreen(PVBOXMP_DEVEXT pDevExt, VBOXWDDM_SOURCE *pSource, RECT const *pRect)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_EXT_VMSVGA pSvga = pDevExt->pGa->hw.pSvga;

    VBOXWDDM_TARGET_ITER Iter;
    VBoxVidPnStTIterInit(pSource, pDevExt->aTargets, VBoxCommonFromDeviceExt(pDevExt)->cDisplays, &Iter);
    for (PVBOXWDDM_TARGET pTarget = VBoxVidPnStTIterNext(&Iter);
         pTarget;
         pTarget = VBoxVidPnStTIterNext(&Iter))
    {
        Status = SvgaBlitGMRFBToScreen(pSvga,
                                       pTarget->u32Id,
                                       pRect->left,
                                       pRect->top,
                                       pRect);
        AssertBreak(Status == STATUS_SUCCESS);
    }

    return Status;
}

NTSTATUS APIENTRY GaDxgkDdiPresentDisplayOnly(const HANDLE hAdapter,
                                              const DXGKARG_PRESENT_DISPLAYONLY *pPresentDisplayOnly)
{
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;

    LOG(("VidPnSourceId %d, pSource %p, BytesPerPixel %d, Pitch %d, Flags 0x%x, NumMoves %d, NumDirtyRects %d, pfn %p\n",
         pPresentDisplayOnly->VidPnSourceId,
         pPresentDisplayOnly->pSource,
         pPresentDisplayOnly->BytesPerPixel,
         pPresentDisplayOnly->Pitch,
         pPresentDisplayOnly->Flags.Value,
         pPresentDisplayOnly->NumMoves,
         pPresentDisplayOnly->NumDirtyRects,
         pPresentDisplayOnly->pDirtyRect,
         pPresentDisplayOnly->pfnPresentDisplayOnlyProgress));

    /*
     * Copy the image to the corresponding VidPn source allocation.
     */
    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pPresentDisplayOnly->VidPnSourceId];
    AssertReturn(pSource->AllocData.Addr.SegmentId == 1, STATUS_SUCCESS); /* Ignore such VidPn sources. */

    VBOXVIDEOOFFSET const offVRAM = vboxWddmAddrVRAMOffset(&pSource->AllocData.Addr);
    AssertReturn(offVRAM != VBOXVIDEOOFFSET_VOID, STATUS_SUCCESS); /* Ignore such VidPn sources. */

    for (ULONG i = 0; i < pPresentDisplayOnly->NumMoves; ++i)
    {
        RECT *pRect = &pPresentDisplayOnly->pMoves[i].DestRect;
        vboxWddmRectCopy(pDevExt->pvVisibleVram + offVRAM,    // dst pointer
                         pSource->AllocData.SurfDesc.bpp / 8, // dst bytes per pixel
                         pSource->AllocData.SurfDesc.pitch,   // dst pitch
                         pPresentDisplayOnly->pSource,        // src pointer
                         pPresentDisplayOnly->BytesPerPixel,  // src bytes per pixel
                         pPresentDisplayOnly->Pitch,          // src pitch
                         pRect);
    }

    for (ULONG i = 0; i < pPresentDisplayOnly->NumDirtyRects; ++i)
    {
        RECT *pRect = &pPresentDisplayOnly->pDirtyRect[i];
        if (pRect->left >= pRect->right || pRect->top >= pRect->bottom)
        {
            continue;
        }

        vboxWddmRectCopy(pDevExt->pvVisibleVram + offVRAM,    // dst pointer
                         pSource->AllocData.SurfDesc.bpp / 8, // dst bytes per pixel
                         pSource->AllocData.SurfDesc.pitch,   // dst pitch
                         pPresentDisplayOnly->pSource,        // src pointer
                         pPresentDisplayOnly->BytesPerPixel,  // src bytes per pixel
                         pPresentDisplayOnly->Pitch,          // src pitch
                         pRect);
    }

    NTSTATUS Status = STATUS_SUCCESS;
    if (pSource->bVisible) /// @todo Does/should this have any effect?
    {
        PVBOXWDDM_EXT_VMSVGA pSvga = pDevExt->pGa->hw.pSvga;
        Status = SvgaDefineGMRFB(pSvga, (uint32_t)offVRAM, pSource->AllocData.SurfDesc.pitch, false);
        if (Status == STATUS_SUCCESS)
        {
            for (ULONG i = 0; i < pPresentDisplayOnly->NumMoves; ++i)
            {
                RECT *pRect = &pPresentDisplayOnly->pMoves[i].DestRect;
                Status = gaSourceBlitToScreen(pDevExt, pSource, pRect);
                AssertBreak(Status == STATUS_SUCCESS);
            }
        }

        if (Status == STATUS_SUCCESS)
        {
            for (ULONG i = 0; i < pPresentDisplayOnly->NumDirtyRects; ++i)
            {
                RECT *pRect = &pPresentDisplayOnly->pDirtyRect[i];
                Status = gaSourceBlitToScreen(pDevExt, pSource, pRect);
                AssertBreak(Status == STATUS_SUCCESS);
            }
        }
    }

    return Status;
}

NTSTATUS GaVidPnSourceReport(PVBOXMP_DEVEXT pDevExt, VBOXWDDM_SOURCE *pSource)
{
    NTSTATUS Status = STATUS_SUCCESS;

    VBOXVIDEOOFFSET offVRAM = vboxWddmAddrVRAMOffset(&pSource->AllocData.Addr);
    if (offVRAM == VBOXVIDEOOFFSET_VOID)
        return STATUS_SUCCESS; /* Ignore such VidPn sources. */

    VBOXWDDM_TARGET_ITER Iter;
    VBoxVidPnStTIterInit(pSource, pDevExt->aTargets, VBoxCommonFromDeviceExt(pDevExt)->cDisplays, &Iter);
    for (PVBOXWDDM_TARGET pTarget = VBoxVidPnStTIterNext(&Iter);
         pTarget;
         pTarget = VBoxVidPnStTIterNext(&Iter))
    {
        Status = GaScreenDefine(pDevExt->pGa,
                                (uint32_t)offVRAM,
                                pTarget->u32Id,
                                pSource->VScreenPos.x, pSource->VScreenPos.y,
                                pSource->AllocData.SurfDesc.width, pSource->AllocData.SurfDesc.height,
                                RT_BOOL(pSource->bBlankedByPowerOff));
        AssertBreak(Status == STATUS_SUCCESS);
    }

    return Status;
}

NTSTATUS GaVidPnSourceCheckPos(PVBOXMP_DEVEXT pDevExt, UINT iSource)
{
    POINT Pos = {0};
    NTSTATUS Status = vboxWddmDisplaySettingsQueryPos(pDevExt, iSource, &Pos);
    if (NT_SUCCESS(Status))
    {
        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[iSource];
        if (memcmp(&pSource->VScreenPos, &Pos, sizeof(Pos)))
        {
            pSource->VScreenPos = Pos;
            Status = GaVidPnSourceReport(pDevExt, pSource);
        }
    }
    return Status;
}
