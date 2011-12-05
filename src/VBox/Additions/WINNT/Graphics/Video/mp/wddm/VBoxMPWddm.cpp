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
#include "common/VBoxMPCommon.h"
#include "common/VBoxMPHGSMI.h"
#include "VBoxMPVhwa.h"
#include "VBoxMPVidPn.h"

#include <iprt/asm.h>
//#include <iprt/initterm.h>

#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxVideo.h>
#include <wingdi.h> /* needed for RGNDATA definition */
#include <VBoxDisplay.h> /* this is from Additions/WINNT/include/ to include escape codes */
#include <VBox/Hardware/VBoxVideoVBE.h>

#define VBOXWDDM_MEMTAG 'MDBV'
PVOID vboxWddmMemAlloc(IN SIZE_T cbSize)
{
    return ExAllocatePoolWithTag(NonPagedPool, cbSize, VBOXWDDM_MEMTAG);
}

PVOID vboxWddmMemAllocZero(IN SIZE_T cbSize)
{
    PVOID pvMem = vboxWddmMemAlloc(cbSize);
    memset(pvMem, 0, cbSize);
    return pvMem;
}


VOID vboxWddmMemFree(PVOID pvMem)
{
    ExFreePool(pvMem);
}

DECLINLINE(PVBOXWDDM_ALLOCATION) vboxWddmGetAllocationFromHandle(PVBOXMP_DEVEXT pDevExt, D3DKMT_HANDLE hAllocation)
{
    DXGKARGCB_GETHANDLEDATA GhData;
    GhData.hObject = hAllocation;
    GhData.Type = DXGK_HANDLE_ALLOCATION;
    GhData.Flags.Value = 0;
    return (PVBOXWDDM_ALLOCATION)pDevExt->u.primary.DxgkInterface.DxgkCbGetHandleData(&GhData);
}

DECLINLINE(PVBOXWDDM_ALLOCATION) vboxWddmGetAllocationFromAllocList(PVBOXMP_DEVEXT pDevExt, DXGK_ALLOCATIONLIST *pAllocList)
{
    PVBOXWDDM_OPENALLOCATION pOa = (PVBOXWDDM_OPENALLOCATION)pAllocList->hDeviceSpecificAllocation;
    return pOa->pAllocation;
}

static void vboxWddmPopulateDmaAllocInfo(PVBOXWDDM_DMA_ALLOCINFO pInfo, PVBOXWDDM_ALLOCATION pAlloc, DXGK_ALLOCATIONLIST *pDmaAlloc)
{
    pInfo->pAlloc = pAlloc;
    if (pDmaAlloc->SegmentId)
    {
        pInfo->offAlloc = (VBOXVIDEOOFFSET)pDmaAlloc->PhysicalAddress.QuadPart;
        pInfo->segmentIdAlloc = pDmaAlloc->SegmentId;
    }
    else
        pInfo->segmentIdAlloc = 0;
    pInfo->srcId = pAlloc->SurfDesc.VidPnSourceId;
}

static void vboxWddmPopulateDmaAllocInfoWithOffset(PVBOXWDDM_DMA_ALLOCINFO pInfo, PVBOXWDDM_ALLOCATION pAlloc, DXGK_ALLOCATIONLIST *pDmaAlloc, uint32_t offStart)
{
    pInfo->pAlloc = pAlloc;
    if (pDmaAlloc->SegmentId)
    {
        pInfo->offAlloc = (VBOXVIDEOOFFSET)pDmaAlloc->PhysicalAddress.QuadPart + offStart;
        pInfo->segmentIdAlloc = pDmaAlloc->SegmentId;
    }
    else
        pInfo->segmentIdAlloc = 0;
    pInfo->srcId = pAlloc->SurfDesc.VidPnSourceId;
}

VBOXVIDEOOFFSET vboxWddmValidatePrimary(PVBOXWDDM_ALLOCATION pAllocation)
{
    Assert(pAllocation);
    if (!pAllocation)
    {
        WARN(("no allocation specified for Source"));
        return VBOXVIDEOOFFSET_VOID;
    }

    Assert(pAllocation->SegmentId);
    if (!pAllocation->SegmentId)
    {
        WARN(("allocation is not paged in"));
        return VBOXVIDEOOFFSET_VOID;
    }

    VBOXVIDEOOFFSET offVram = pAllocation->offVram;
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        WARN(("VRAM pffset is not defined"));

    return offVram;
}

NTSTATUS vboxWddmGhDisplayPostInfoScreenBySDesc (PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SURFACE_DESC pDesc, POINT * pVScreenPos, uint16_t fFlags)
{
    void *p = VBoxHGSMIBufferAlloc (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx,
                                      sizeof (VBVAINFOSCREEN),
                                      HGSMI_CH_VBVA,
                                      VBVA_INFO_SCREEN);
    Assert(p);
    if (p)
    {
        VBVAINFOSCREEN *pScreen = (VBVAINFOSCREEN *)p;

        pScreen->u32ViewIndex    = pDesc->VidPnSourceId;
        pScreen->i32OriginX      = pVScreenPos->x;
        pScreen->i32OriginY      = pVScreenPos->y;
        pScreen->u32StartOffset  = 0; //(uint32_t)offVram; /* we pretend the view is located at the start of each framebuffer */
        pScreen->u32LineSize     = pDesc->pitch;
        pScreen->u32Width        = pDesc->width;
        pScreen->u32Height       = pDesc->height;
        pScreen->u16BitsPerPixel = (uint16_t)pDesc->bpp;
        pScreen->u16Flags        = fFlags;

        VBoxHGSMIBufferSubmit (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx, p);

        VBoxHGSMIBufferFree (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx, p);
    }

    return STATUS_SUCCESS;

}

NTSTATUS vboxWddmGhDisplayPostInfoScreen (PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation, POINT * pVScreenPos)
{
    NTSTATUS Status = vboxWddmGhDisplayPostInfoScreenBySDesc(pDevExt, &pAllocation->SurfDesc, pVScreenPos, VBVA_SCREEN_F_ACTIVE);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

NTSTATUS vboxWddmGhDisplayPostInfoView(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXVIDEOOFFSET offVram = pAllocation->offVram;
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    /* Issue the screen info command. */
    void *p = VBoxHGSMIBufferAlloc (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx,
                                      sizeof (VBVAINFOVIEW),
                                      HGSMI_CH_VBVA,
                                      VBVA_INFO_VIEW);
    Assert(p);
    if (p)
    {
        VBVAINFOVIEW *pView = (VBVAINFOVIEW *)p;

        pView->u32ViewIndex     = pAllocation->SurfDesc.VidPnSourceId;
        pView->u32ViewOffset    = (uint32_t)offVram; /* we pretend the view is located at the start of each framebuffer */
        pView->u32ViewSize      = vboxWddmVramCpuVisibleSegmentSize(pDevExt)/VBoxCommonFromDeviceExt(pDevExt)->cDisplays;

        pView->u32MaxScreenSize = pView->u32ViewSize;

        VBoxHGSMIBufferSubmit (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx, p);

        VBoxHGSMIBufferFree (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx, p);
    }

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmGhDisplaySetMode(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
//    PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimaryInfo = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
    if (/*pPrimaryInfo->*/pAllocation->SurfDesc.VidPnSourceId)
        return STATUS_SUCCESS;

    USHORT width  = pAllocation->SurfDesc.width;
    USHORT height = pAllocation->SurfDesc.height;
    USHORT bpp    = pAllocation->SurfDesc.bpp;
    ULONG cbLine  = VBOXWDDM_ROUNDBOUND(((width * bpp) + 7) / 8, 4);
    ULONG yOffset = (ULONG)pAllocation->offVram / cbLine;
    ULONG xOffset = (ULONG)pAllocation->offVram % cbLine;

    if (bpp == 4)
    {
        xOffset <<= 1;
    }
    else
    {
        Assert(!(xOffset%((bpp + 7) >> 3)));
        xOffset /= ((bpp + 7) >> 3);
    }
    Assert(xOffset <= 0xffff);
    Assert(yOffset <= 0xffff);

    VBoxVideoSetModeRegisters(width, height, width, bpp, 0, (uint16_t)xOffset, (uint16_t)yOffset);
    /*@todo read back from port to check if mode switch was successful */

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmGhDisplayUpdateScreenPos(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource, POINT *pVScreenPos)
{
    if (pSource->VScreenPos.x == pVScreenPos->x
            && pSource->VScreenPos.y == pVScreenPos->y)
        return STATUS_SUCCESS;

    pSource->VScreenPos = *pVScreenPos;

    PVBOXWDDM_ALLOCATION pAllocation = VBOXWDDM_FB_ALLOCATION(pSource);
    NTSTATUS Status = vboxWddmGhDisplayPostInfoScreen(pDevExt, pAllocation, &pSource->VScreenPos);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

NTSTATUS vboxWddmGhDisplaySetInfo(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
{
    PVBOXWDDM_ALLOCATION pAllocation = VBOXWDDM_FB_ALLOCATION(pSource);
    VBOXVIDEOOFFSET offVram = vboxWddmValidatePrimary(pAllocation);
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    /*
     * Set the current mode into the hardware.
     */
//    NTSTATUS Status= vboxWddmDisplaySettingsQueryPos(pDevExt, VidPnSourceId, &pSource->VScreenPos);
//    Assert(Status == STATUS_SUCCESS);
    NTSTATUS Status = vboxWddmGhDisplaySetMode(pDevExt, pAllocation);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmGhDisplayPostInfoView(pDevExt, pAllocation);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxWddmGhDisplayPostInfoScreen(pDevExt, pAllocation, &pSource->VScreenPos);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                WARN(("vboxWddmGhDisplayPostInfoScreen failed"));
        }
        else
            WARN(("vboxWddmGhDisplayPostInfoView failed"));
    }
    else
        WARN(("vboxWddmGhDisplaySetMode failed"));

    return Status;
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
bool vboxWddmCheckUpdateShadowAddress(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_SOURCE pSource,
        D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId, UINT SegmentId, VBOXVIDEOOFFSET offVram)
{
    if (pSource->pPrimaryAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC)
    {
        Assert(pSource->offVram == VBOXVIDEOOFFSET_VOID);
        return false;
    }
    if (pSource->offVram == offVram)
        return false;
    pSource->offVram = offVram;
    pSource->pShadowAllocation->SegmentId = SegmentId;
    pSource->pShadowAllocation->offVram = offVram;

    NTSTATUS Status = vboxWddmGhDisplaySetInfo(pDevExt, pSource, VidPnSourceId);
    Assert(Status == STATUS_SUCCESS);
    if (Status != STATUS_SUCCESS)
        WARN(("vboxWddmGhDisplaySetInfo failed, Status (0x%x)", Status));

    return true;
}
#endif

HGSMIHEAP* vboxWddmHgsmiGetHeapFromCmdOffset(PVBOXMP_DEVEXT pDevExt, HGSMIOFFSET offCmd)
{
#ifdef VBOX_WITH_VDMA
    if(HGSMIAreaContainsOffset(&pDevExt->u.primary.Vdma.CmdHeap.area, offCmd))
        return &pDevExt->u.primary.Vdma.CmdHeap;
#endif
    if (HGSMIAreaContainsOffset(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx.area, offCmd))
        return &VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx;
    return NULL;
}

typedef enum
{
    VBOXWDDM_HGSMICMD_TYPE_UNDEFINED = 0,
    VBOXWDDM_HGSMICMD_TYPE_CTL       = 1,
#ifdef VBOX_WITH_VDMA
    VBOXWDDM_HGSMICMD_TYPE_DMACMD    = 2
#endif
} VBOXWDDM_HGSMICMD_TYPE;

VBOXWDDM_HGSMICMD_TYPE vboxWddmHgsmiGetCmdTypeFromOffset(PVBOXMP_DEVEXT pDevExt, HGSMIOFFSET offCmd)
{
#ifdef VBOX_WITH_VDMA
    if(HGSMIAreaContainsOffset(&pDevExt->u.primary.Vdma.CmdHeap.area, offCmd))
        return VBOXWDDM_HGSMICMD_TYPE_DMACMD;
#endif
    if (HGSMIAreaContainsOffset(&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx.area, offCmd))
        return VBOXWDDM_HGSMICMD_TYPE_CTL;
    return VBOXWDDM_HGSMICMD_TYPE_UNDEFINED;
}

static NTSTATUS vboxWddmChildStatusReportPerform(PVBOXMP_DEVEXT pDevExt, PVBOXVDMA_CHILD_STATUS pChildStatus, D3DDDI_VIDEO_PRESENT_TARGET_ID iChild)
{
    DXGK_CHILD_STATUS DdiChildStatus;
    if (pChildStatus->fFlags & VBOXVDMA_CHILD_STATUS_F_DISCONNECTED)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusConnection;
        if (iChild != D3DDDI_ID_UNINITIALIZED)
        {
            Assert(iChild < UINT32_MAX/2);
            Assert(iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
            DdiChildStatus.ChildUid = iChild;
        }
        else
        {
            Assert(pChildStatus->iChild < UINT32_MAX/2);
            Assert(pChildStatus->iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
            DdiChildStatus.ChildUid = pChildStatus->iChild;
        }
        DdiChildStatus.HotPlug.Connected = FALSE;
        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
    }

    if (pChildStatus->fFlags & VBOXVDMA_CHILD_STATUS_F_CONNECTED)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusConnection;
        if (iChild != D3DDDI_ID_UNINITIALIZED)
        {
            Assert(iChild < UINT32_MAX/2);
            Assert(iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
            DdiChildStatus.ChildUid = iChild;
        }
        else
        {
            Assert(pChildStatus->iChild < UINT32_MAX/2);
            Assert(pChildStatus->iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
            DdiChildStatus.ChildUid = pChildStatus->iChild;
        }
        DdiChildStatus.HotPlug.Connected = TRUE;
        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
    }

    if (pChildStatus->fFlags & VBOXVDMA_CHILD_STATUS_F_ROTATED)
    {
        /* report disconnected */
        memset(&DdiChildStatus, 0, sizeof (DdiChildStatus));
        DdiChildStatus.Type = StatusRotation;
        if (iChild != D3DDDI_ID_UNINITIALIZED)
        {
            Assert(iChild < UINT32_MAX/2);
            Assert(iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
            DdiChildStatus.ChildUid = iChild;
        }
        else
        {
            Assert(pChildStatus->iChild < UINT32_MAX/2);
            Assert(pChildStatus->iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
            DdiChildStatus.ChildUid = pChildStatus->iChild;
        }
        DdiChildStatus.Rotation.Angle = pChildStatus->u8RotationAngle;
        NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbIndicateChildStatus(pDevExt->u.primary.DxgkInterface.DeviceHandle, &DdiChildStatus);
        if (!NT_SUCCESS(Status))
        {
            WARN(("DxgkCbIndicateChildStatus failed with Status (0x%x)", Status));
            return Status;
        }
    }

    return STATUS_SUCCESS;
}

typedef struct VBOXWDDMCHILDSTATUSCB
{
    PVBOXVDMACBUF_DR pDr;
    PKEVENT pEvent;
} VBOXWDDMCHILDSTATUSCB, *PVBOXWDDMCHILDSTATUSCB;

static DECLCALLBACK(VOID) vboxWddmChildStatusReportCompletion(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    /* we should be called from our DPC routine */
    Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);

    PVBOXWDDMCHILDSTATUSCB pCtx = (PVBOXWDDMCHILDSTATUSCB)pvContext;
    PVBOXVDMACBUF_DR pDr = pCtx->pDr;
    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    VBOXVDMACMD_CHILD_STATUS_IRQ *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHILD_STATUS_IRQ);

    for (UINT i = 0; i < pBody->cInfos; ++i)
    {
        PVBOXVDMA_CHILD_STATUS pInfo = &pBody->aInfos[i];
        if (pBody->fFlags & VBOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL)
        {
            for (D3DDDI_VIDEO_PRESENT_TARGET_ID iChild = 0; iChild < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++iChild)
            {
                NTSTATUS Status = vboxWddmChildStatusReportPerform(pDevExt, pInfo, iChild);
                if (!NT_SUCCESS(Status))
                {
                    WARN(("vboxWddmChildStatusReportPerform failed with Status (0x%x)", Status));
                    break;
                }
            }
        }
        else
        {
            NTSTATUS Status = vboxWddmChildStatusReportPerform(pDevExt, pInfo, D3DDDI_ID_UNINITIALIZED);
            if (!NT_SUCCESS(Status))
            {
                WARN(("vboxWddmChildStatusReportPerform failed with Status (0x%x)", Status));
                break;
            }
        }
    }

    vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);

    if (pCtx->pEvent)
    {
        KeSetEvent(pCtx->pEvent, 0, FALSE);
    }
}

static NTSTATUS vboxWddmChildStatusReportReconnected(PVBOXMP_DEVEXT pDevExt, D3DDDI_VIDEO_PRESENT_TARGET_ID idTarget)
{
    NTSTATUS Status = STATUS_UNSUCCESSFUL;
    UINT cbCmd = VBOXVDMACMD_SIZE_FROMBODYSIZE(sizeof (VBOXVDMACMD_CHILD_STATUS_IRQ));

    PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate(&pDevExt->u.primary.Vdma, cbCmd);
    if (pDr)
    {
        // vboxVdmaCBufDrCreate zero initializes the pDr
        /* the command data follows the descriptor */
        pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
        pDr->cbBuf = cbCmd;
        pDr->rc = VERR_NOT_IMPLEMENTED;

        PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
        pHdr->enmType = VBOXVDMACMD_TYPE_CHILD_STATUS_IRQ;
        pHdr->u32CmdSpecific = 0;
        PVBOXVDMACMD_CHILD_STATUS_IRQ pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHILD_STATUS_IRQ);
        pBody->cInfos = 1;
        if (idTarget == D3DDDI_ID_ALL)
        {
            pBody->fFlags |= VBOXVDMACMD_CHILD_STATUS_IRQ_F_APPLY_TO_ALL;
        }
        pBody->aInfos[0].iChild = idTarget;
        pBody->aInfos[0].fFlags = VBOXVDMA_CHILD_STATUS_F_DISCONNECTED | VBOXVDMA_CHILD_STATUS_F_CONNECTED;

        /* we're going to KeWaitForSingleObject */
        Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

        PVBOXVDMADDI_CMD pDdiCmd = VBOXVDMADDI_CMD_FROM_BUF_DR(pDr);
        VBOXWDDMCHILDSTATUSCB Ctx;
        KEVENT Event;
        KeInitializeEvent(&Event, NotificationEvent, FALSE);
        Ctx.pDr = pDr;
        Ctx.pEvent = &Event;
        vboxVdmaDdiCmdInit(pDdiCmd, 0, 0, vboxWddmChildStatusReportCompletion, &Ctx);
        /* mark command as submitted & invisible for the dx runtime since dx did not originate it */
        vboxVdmaDdiCmdSubmittedNotDx(pDdiCmd);
        int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
        Assert(rc == VINF_SUCCESS);
        if (RT_SUCCESS(rc))
        {
            Status = KeWaitForSingleObject(&Event, Executive, KernelMode, FALSE, NULL);
            Assert(Status == STATUS_SUCCESS);
            return STATUS_SUCCESS;
        }

        Status = STATUS_UNSUCCESSFUL;

        vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
    }
    else
    {
        Assert(0);
        /* @todo: try flushing.. */
        LOGREL(("vboxVdmaCBufDrCreate returned NULL"));
        Status = STATUS_INSUFFICIENT_RESOURCES;
    }

    return Status;
}

static NTSTATUS vboxWddmChildStatusCheck(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_VIDEOMODES_INFO paInfos)
{
    NTSTATUS Status = STATUS_SUCCESS;
    bool bChanged[VBOX_VIDEO_MAX_SCREENS] = {0};
    int i;

    Assert(!bChanged[0]);
    for (i = 1; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        /* @todo: check that we actually need the current source->target */
        PVBOXWDDM_VIDEOMODES_INFO pInfo = &paInfos[i];
        VIDEO_MODE_INFORMATION *pModeInfo = &pInfo->aModes[pInfo->iPreferredMode];
        BOOLEAN fMatch = FALSE;
        Status = vboxVidPnMatchMonitorModes(pDevExt, i, pInfo->aResolutions, pInfo->cResolutions, &fMatch);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vboxVidPnMatchMonitorModes failed Status(0x%x)", Status));
            /* ignore the failures here, although we probably should not?? */
            break;
        }

        bChanged[i] = !fMatch;

        if (!fMatch)
        {
            Status = vboxWddmChildStatusReportReconnected(pDevExt, i);
            if (!NT_SUCCESS(Status))
            {
                WARN(("vboxWddmChildStatusReportReconnected failed Status(0x%x)", Status));
                /* ignore the failures here, although we probably should not?? */
                break;
            }
        }
    }

    if (!NT_SUCCESS(Status))
    {
        WARN(("updating monitor modes failed, Status(0x%x)", Status));
        return Status;
    }

    /* wait for the reconnected monitor data to be picked up */
    CONST DXGK_MONITOR_INTERFACE *pMonitorInterface;
    Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryMonitorInterface(pDevExt->u.primary.DxgkInterface.DeviceHandle, DXGK_MONITOR_INTERFACE_VERSION_V1, &pMonitorInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
        return Status;
    }

    for (i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        D3DKMDT_HMONITORSOURCEMODESET hMonitorSMS;
        CONST DXGK_MONITORSOURCEMODESET_INTERFACE *pMonitorSMSIf;
        if (!bChanged[i])
            continue;

        while (1)
        {
            Status = pMonitorInterface->pfnAcquireMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle,
                                                i,
                                                &hMonitorSMS,
                                                &pMonitorSMSIf);
            if (NT_SUCCESS(Status))
            {
                NTSTATUS tmpStatus = pMonitorInterface->pfnReleaseMonitorSourceModeSet(pDevExt->u.primary.DxgkInterface.DeviceHandle, hMonitorSMS);
                if (!NT_SUCCESS(tmpStatus))
                {
                    WARN(("pfnReleaseMonitorSourceModeSet failed tmpStatus(0x%x)", tmpStatus));
                }
                break;
            }

            if (Status != STATUS_GRAPHICS_MONITOR_NOT_CONNECTED)
            {
                WARN(("DxgkCbQueryMonitorInterface failed, Status()0x%x", Status));
                break;
            }

            Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);

            LARGE_INTEGER Interval;
            Interval.QuadPart = -(int64_t) 2 /* ms */ * 10000;
            NTSTATUS tmpStatus = KeDelayExecutionThread(KernelMode, FALSE, &Interval);
            if (!NT_SUCCESS(tmpStatus))
            {
                WARN(("KeDelayExecutionThread failed tmpStatus(0x%x)", tmpStatus));
            }
        }
    }

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmPickResources(PVBOXMP_DEVEXT pContext, PDXGK_DEVICE_INFO pDeviceInfo, PULONG pAdapterMemorySize)
{
    NTSTATUS Status = STATUS_SUCCESS;
    USHORT DispiId;
    *pAdapterMemorySize = VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES;

    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    VBoxVideoCmnPortWriteUshort(VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID2);
    DispiId = VBoxVideoCmnPortReadUshort(VBE_DISPI_IOPORT_DATA);
    if (DispiId == VBE_DISPI_ID2)
    {
       LOGREL(("found the VBE card"));
       /*
        * Write some hardware information to registry, so that
        * it's visible in Windows property dialog.
        */

       /*
        * Query the adapter's memory size. It's a bit of a hack, we just read
        * an ULONG from the data port without setting an index before.
        */
       *pAdapterMemorySize = VBoxVideoCmnPortReadUlong(VBE_DISPI_IOPORT_DATA);
       if (VBoxHGSMIIsSupported ())
       {
           PCM_RESOURCE_LIST pRcList = pDeviceInfo->TranslatedResourceList;
           /* @todo: verify resources */
           for (ULONG i = 0; i < pRcList->Count; ++i)
           {
               PCM_FULL_RESOURCE_DESCRIPTOR pFRc = &pRcList->List[i];
               for (ULONG j = 0; j < pFRc->PartialResourceList.Count; ++j)
               {
                   PCM_PARTIAL_RESOURCE_DESCRIPTOR pPRc = &pFRc->PartialResourceList.PartialDescriptors[j];
                   switch (pPRc->Type)
                   {
                       case CmResourceTypePort:
                           break;
                       case CmResourceTypeInterrupt:
                           break;
                       case CmResourceTypeMemory:
                           break;
                       case CmResourceTypeDma:
                           break;
                       case CmResourceTypeDeviceSpecific:
                           break;
                       case CmResourceTypeBusNumber:
                           break;
                       default:
                           break;
                   }
               }
           }
       }
       else
       {
           LOGREL(("HGSMI unsupported, returning err"));
           /* @todo: report a better status */
           Status = STATUS_UNSUCCESSFUL;
       }
    }
    else
    {
        LOGREL(("VBE card not found, returning err"));
        Status = STATUS_UNSUCCESSFUL;
    }


    return Status;
}

static void vboxWddmDevExtZeroinit(PVBOXMP_DEVEXT pDevExt, CONST PDEVICE_OBJECT pPDO)
{
    memset(pDevExt, 0, sizeof (VBOXMP_DEVEXT));
    pDevExt->pPDO = pPDO;
    PWCHAR pName = (PWCHAR)(((uint8_t*)pDevExt) + VBOXWDDM_ROUNDBOUND(sizeof(VBOXMP_DEVEXT), 8));
    RtlInitUnicodeString(&pDevExt->RegKeyName, pName);
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    for (int i = 0; i < RT_ELEMENTS(pDevExt->aSources); ++i)
    {
        pDevExt->aSources[i].offVram = VBOXVIDEOOFFSET_VOID;
    }
#endif
}

static void vboxWddmSetupDisplays(PVBOXMP_DEVEXT pDevExt)
{
    /* For WDDM, we simply store the number of monitors as we will deal with
     * VidPN stuff later */
    int rc = STATUS_SUCCESS;

    if (VBoxCommonFromDeviceExt(pDevExt)->bHGSMI)
    {
        ULONG ulAvailable = VBoxCommonFromDeviceExt(pDevExt)->cbVRAM
                            - VBoxCommonFromDeviceExt(pDevExt)->cbMiniportHeap
                            - VBVA_ADAPTER_INFORMATION_SIZE;

        ULONG ulSize;
        ULONG offset;
#ifdef VBOX_WITH_VDMA
        ulSize = ulAvailable / 2;
        if (ulSize > VBOXWDDM_C_VDMA_BUFFER_SIZE)
            ulSize = VBOXWDDM_C_VDMA_BUFFER_SIZE;

        /* Align down to 4096 bytes. */
        ulSize &= ~0xFFF;
        offset = ulAvailable - ulSize;

        Assert(!(offset & 0xFFF));
#else
        offset = ulAvailable;
#endif
        rc = vboxVdmaCreate (pDevExt, &pDevExt->u.primary.Vdma
#ifdef VBOX_WITH_VDMA
                , offset, ulSize
#endif
                );
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            /* can enable it right away since the host does not need any screen/FB info
             * for basic DMA functionality */
            rc = vboxVdmaEnable(pDevExt, &pDevExt->u.primary.Vdma);
            AssertRC(rc);
            if (RT_FAILURE(rc))
                vboxVdmaDestroy(pDevExt, &pDevExt->u.primary.Vdma);
        }

        ulAvailable = offset;
        ulSize = ulAvailable/2;
        offset = ulAvailable - ulSize;

        NTSTATUS Status = vboxVideoAMgrCreate(pDevExt, &pDevExt->AllocMgr, offset, ulSize);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            offset = ulAvailable;
        }

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
        if (RT_SUCCESS(rc))
        {
            ulAvailable = offset;
            ulSize = ulAvailable / 2;
            ulSize /= VBoxCommonFromDeviceExt(pDevExt)->cDisplays;
            Assert(ulSize > VBVA_MIN_BUFFER_SIZE);
            if (ulSize > VBVA_MIN_BUFFER_SIZE)
            {
                ULONG ulRatio = ulSize/VBVA_MIN_BUFFER_SIZE;
                ulRatio >>= 4; /* /= 16; */
                if (ulRatio)
                    ulSize = VBVA_MIN_BUFFER_SIZE * ulRatio;
                else
                    ulSize = VBVA_MIN_BUFFER_SIZE;
            }
            else
            {
                /* todo: ?? */
            }

            ulSize &= ~0xFFF;
            Assert(ulSize);

            Assert(ulSize * VBoxCommonFromDeviceExt(pDevExt)->cDisplays < ulAvailable);

            for (int i = VBoxCommonFromDeviceExt(pDevExt)->cDisplays-1; i >= 0; --i)
            {
                offset -= ulSize;
                rc = vboxVbvaCreate(pDevExt, &pDevExt->aSources[i].Vbva, offset, ulSize, i);
                AssertRC(rc);
                if (RT_SUCCESS(rc))
                {
                    rc = vboxVbvaEnable(pDevExt, &pDevExt->aSources[i].Vbva);
                    AssertRC(rc);
                    if (RT_FAILURE(rc))
                    {
                        /* @todo: de-initialize */
                    }
                }
            }
        }
#endif

        rc = VBoxMPCmnMapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt), (void**)&pDevExt->pvVisibleVram,
                                       0, vboxWddmVramCpuVisibleSize(pDevExt));
        Assert(rc == VINF_SUCCESS);
        if (rc != VINF_SUCCESS)
            pDevExt->pvVisibleVram = NULL;

        if (RT_FAILURE(rc))
            VBoxCommonFromDeviceExt(pDevExt)->bHGSMI = FALSE;
    }
}

static int vboxWddmFreeDisplays(PVBOXMP_DEVEXT pDevExt)
{
    int rc = VINF_SUCCESS;

    Assert(pDevExt->pvVisibleVram);
    if (pDevExt->pvVisibleVram)
        VBoxMPCmnUnmapAdapterMemory(VBoxCommonFromDeviceExt(pDevExt), (void**)&pDevExt->pvVisibleVram);

    for (int i = VBoxCommonFromDeviceExt(pDevExt)->cDisplays-1; i >= 0; --i)
    {
        rc = vboxVbvaDisable(pDevExt, &pDevExt->aSources[i].Vbva);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            rc = vboxVbvaDestroy(pDevExt, &pDevExt->aSources[i].Vbva);
            AssertRC(rc);
            if (RT_FAILURE(rc))
            {
                /* @todo: */
            }
        }
    }

    vboxVideoAMgrDestroy(pDevExt, &pDevExt->AllocMgr);

    rc = vboxVdmaDisable(pDevExt, &pDevExt->u.primary.Vdma);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        rc = vboxVdmaDestroy(pDevExt, &pDevExt->u.primary.Vdma);
        AssertRC(rc);
    }
    return rc;
}

/* driver callbacks */
NTSTATUS DxgkDdiAddDevice(
    IN CONST PDEVICE_OBJECT PhysicalDeviceObject,
    OUT PVOID *MiniportDeviceContext
    )
{
    /* The DxgkDdiAddDevice function should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, pdo(0x%x)", PhysicalDeviceObject));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pDevExt;

    WCHAR RegKeyBuf[512];
    ULONG cbRegKeyBuf = sizeof (RegKeyBuf);

    Status = IoGetDeviceProperty (PhysicalDeviceObject,
                                  DevicePropertyDriverKeyName,
                                  cbRegKeyBuf,
                                  RegKeyBuf,
                                  &cbRegKeyBuf);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        pDevExt = (PVBOXMP_DEVEXT)vboxWddmMemAllocZero(VBOXWDDM_ROUNDBOUND(sizeof(VBOXMP_DEVEXT), 8) + cbRegKeyBuf);
        if (pDevExt)
        {
            PWCHAR pName = (PWCHAR)(((uint8_t*)pDevExt) + VBOXWDDM_ROUNDBOUND(sizeof(VBOXMP_DEVEXT), 8));
            memcpy(pName, RegKeyBuf, cbRegKeyBuf);
            vboxWddmDevExtZeroinit(pDevExt, PhysicalDeviceObject);
            *MiniportDeviceContext = pDevExt;
        }
        else
        {
            Status  = STATUS_NO_MEMORY;
            LOGREL(("ERROR, failed to create context"));
        }
    }

    LOGF(("LEAVE, Status(0x%x), pDevExt(0x%x)", Status, pDevExt));

    return Status;
}

NTSTATUS DxgkDdiStartDevice(
    IN CONST PVOID  MiniportDeviceContext,
    IN PDXGK_START_INFO  DxgkStartInfo,
    IN PDXGKRNL_INTERFACE  DxgkInterface,
    OUT PULONG  NumberOfVideoPresentSources,
    OUT PULONG  NumberOfChildren
    )
{
    /* The DxgkDdiStartDevice function should be made pageable. */
    PAGED_CODE();

    NTSTATUS Status;

    LOGF(("ENTER, context(0x%x)", MiniportDeviceContext));

    vboxVDbgBreakFv();

    if ( ARGUMENT_PRESENT(MiniportDeviceContext) &&
        ARGUMENT_PRESENT(DxgkInterface) &&
        ARGUMENT_PRESENT(DxgkStartInfo) &&
        ARGUMENT_PRESENT(NumberOfVideoPresentSources),
        ARGUMENT_PRESENT(NumberOfChildren)
        )
    {
        PVBOXMP_DEVEXT pContext = (PVBOXMP_DEVEXT)MiniportDeviceContext;

        vboxWddmVGuidGet(pContext);

        /* Save DeviceHandle and function pointers supplied by the DXGKRNL_INTERFACE structure passed to DxgkInterface. */
        memcpy(&pContext->u.primary.DxgkInterface, DxgkInterface, sizeof (DXGKRNL_INTERFACE));

        /* Allocate a DXGK_DEVICE_INFO structure, and call DxgkCbGetDeviceInformation to fill in the members of that structure, which include the registry path, the PDO, and a list of translated resources for the display adapter represented by MiniportDeviceContext. Save selected members (ones that the display miniport driver will need later)
         * of the DXGK_DEVICE_INFO structure in the context block represented by MiniportDeviceContext. */
        DXGK_DEVICE_INFO DeviceInfo;
        Status = pContext->u.primary.DxgkInterface.DxgkCbGetDeviceInformation (pContext->u.primary.DxgkInterface.DeviceHandle, &DeviceInfo);
        if (Status == STATUS_SUCCESS)
        {
            ULONG AdapterMemorySize;
            Status = vboxWddmPickResources(pContext, &DeviceInfo, &AdapterMemorySize);
            if (Status == STATUS_SUCCESS)
            {
                /* Initialize VBoxGuest library, which is used for requests which go through VMMDev. */
                VbglInit ();

                /* Guest supports only HGSMI, the old VBVA via VMMDev is not supported.
                 * The host will however support both old and new interface to keep compatibility
                 * with old guest additions.
                 */
                VBoxSetupDisplaysHGSMI(VBoxCommonFromDeviceExt(pContext),
                                       AdapterMemorySize,
                                       VBVACAPS_COMPLETEGCMD_BY_IOREAD | VBVACAPS_IRQ);
                if (VBoxCommonFromDeviceExt(pContext)->bHGSMI)
                {
                    vboxWddmSetupDisplays(pContext);
                    if (!VBoxCommonFromDeviceExt(pContext)->bHGSMI)
                        VBoxFreeDisplaysHGSMI(VBoxCommonFromDeviceExt(pContext));
                }
                if (VBoxCommonFromDeviceExt(pContext)->bHGSMI)
                {
                    LOGREL(("using HGSMI"));
                    *NumberOfVideoPresentSources = VBoxCommonFromDeviceExt(pContext)->cDisplays;
                    *NumberOfChildren = VBoxCommonFromDeviceExt(pContext)->cDisplays;
                    LOG(("sources(%d), children(%d)", *NumberOfVideoPresentSources, *NumberOfChildren));

                    vboxVdmaDdiNodesInit(pContext);
                    vboxVideoCmInit(&pContext->CmMgr);
                    InitializeListHead(&pContext->SwapchainList3D);
                    pContext->cContexts3D = 0;
                    ExInitializeFastMutex(&pContext->ContextMutex);
                    KeInitializeSpinLock(&pContext->SynchLock);

                    VBoxMPCmnInitCustomVideoModes(pContext);
                    VBoxWddmInvalidateVideoModesInfo(pContext);
#if 0
                    vboxShRcTreeInit(pContext);
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
                    vboxVhwaInit(pContext);
#endif
                }
                else
                {
                    LOGREL(("HGSMI failed to initialize, returning err"));

                    VbglTerminate();
                    /* @todo: report a better status */
                    Status = STATUS_UNSUCCESSFUL;
                }
            }
            else
            {
                LOGREL(("vboxWddmPickResources failed Status(0x%x), returning err", Status));
                Status = STATUS_UNSUCCESSFUL;
            }
        }
        else
        {
            LOGREL(("DxgkCbGetDeviceInformation failed Status(0x%x), returning err", Status));
        }
    }
    else
    {
        LOGREL(("invalid parameter, returning err"));
        Status = STATUS_INVALID_PARAMETER;
    }

    LOGF(("LEAVE, status(0x%x)", Status));

    return Status;
}

NTSTATUS DxgkDdiStopDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* The DxgkDdiStopDevice function should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%p)", MiniportDeviceContext));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;
    NTSTATUS Status = STATUS_SUCCESS;

    vboxVideoCmTerm(&pDevExt->CmMgr);

    /* do everything we did on DxgkDdiStartDevice in the reverse order */
#ifdef VBOX_WITH_VIDEOHWACCEL
    vboxVhwaFree(pDevExt);
#endif
#if 0
    vboxShRcTreeTerm(pDevExt);
#endif

    int rc = vboxWddmFreeDisplays(pDevExt);
    if (RT_SUCCESS(rc))
        VBoxFreeDisplaysHGSMI(VBoxCommonFromDeviceExt(pDevExt));
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        VbglTerminate();

        vboxWddmVGuidFree(pDevExt);

        /* revert back to the state we were right after the DxgkDdiAddDevice */
        vboxWddmDevExtZeroinit(pDevExt, pDevExt->pPDO);
    }
    else
        Status = STATUS_UNSUCCESSFUL;

    return Status;
}

NTSTATUS DxgkDdiRemoveDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* DxgkDdiRemoveDevice should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%p)", MiniportDeviceContext));

    vboxVDbgBreakFv();

    vboxWddmMemFree(MiniportDeviceContext);

    LOGF(("LEAVE, context(0x%p)", MiniportDeviceContext));

    return STATUS_SUCCESS;
}

NTSTATUS DxgkDdiDispatchIoRequest(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG VidPnSourceId,
    IN PVIDEO_REQUEST_PACKET VideoRequestPacket
    )
{
    LOGF(("ENTER, context(0x%p), ctl(0x%x)", MiniportDeviceContext, VideoRequestPacket->IoControlCode));

    AssertBreakpoint();
#if 0
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;

    switch (VideoRequestPacket->IoControlCode)
    {
        case IOCTL_VIDEO_QUERY_COLOR_CAPABILITIES:
        {
            if (VideoRequestPacket->OutputBufferLength < sizeof(VIDEO_COLOR_CAPABILITIES))
            {
                AssertBreakpoint();
                VideoRequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }
            VIDEO_COLOR_CAPABILITIES *pCaps = (VIDEO_COLOR_CAPABILITIES*)VideoRequestPacket->OutputBuffer;

            pCaps->Length = sizeof (VIDEO_COLOR_CAPABILITIES);
            pCaps->AttributeFlags = VIDEO_DEVICE_COLOR;
            pCaps->RedPhosphoreDecay = 0;
            pCaps->GreenPhosphoreDecay = 0;
            pCaps->BluePhosphoreDecay = 0;
            pCaps->WhiteChromaticity_x = 3127;
            pCaps->WhiteChromaticity_y = 3290;
            pCaps->WhiteChromaticity_Y = 0;
            pCaps->RedChromaticity_x = 6700;
            pCaps->RedChromaticity_y = 3300;
            pCaps->GreenChromaticity_x = 2100;
            pCaps->GreenChromaticity_y = 7100;
            pCaps->BlueChromaticity_x = 1400;
            pCaps->BlueChromaticity_y = 800;
            pCaps->WhiteGamma = 0;
            pCaps->RedGamma = 20000;
            pCaps->GreenGamma = 20000;
            pCaps->BlueGamma = 20000;

            VideoRequestPacket->StatusBlock->Status = NO_ERROR;
            VideoRequestPacket->StatusBlock->Information = sizeof (VIDEO_COLOR_CAPABILITIES);
            break;
        }
#if 0
        case IOCTL_VIDEO_HANDLE_VIDEOPARAMETERS:
        {
            if (VideoRequestPacket->OutputBufferLength < sizeof(VIDEOPARAMETERS)
                    || VideoRequestPacket->InputBufferLength < sizeof(VIDEOPARAMETERS))
            {
                AssertBreakpoint();
                VideoRequestPacket->StatusBlock->Status = ERROR_INSUFFICIENT_BUFFER;
                return TRUE;
            }

            Result = VBoxVideoResetDevice((PVBOXMP_DEVEXT)HwDeviceExtension,
                                          RequestPacket->StatusBlock);
            break;
        }
#endif
        default:
            AssertBreakpoint();
            VideoRequestPacket->StatusBlock->Status = ERROR_INVALID_FUNCTION;
            VideoRequestPacket->StatusBlock->Information = 0;
    }
#endif
    LOGF(("LEAVE, context(0x%p), ctl(0x%x)", MiniportDeviceContext, VideoRequestPacket->IoControlCode));

    return STATUS_SUCCESS;
}

BOOLEAN DxgkDdiInterruptRoutine(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG MessageNumber
    )
{
//    LOGF(("ENTER, context(0x%p), msg(0x%x)", MiniportDeviceContext, MessageNumber));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;
    BOOLEAN bOur = FALSE;
    BOOLEAN bNeedDpc = FALSE;
    if (VBoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags) /* If HGSMI is enabled at all. */
    {
        VBOXSHGSMILIST CtlList;
#ifdef VBOX_WITH_VDMA
        VBOXSHGSMILIST DmaCmdList;
#endif
        vboxSHGSMIListInit(&CtlList);
#ifdef VBOX_WITH_VDMA
        vboxSHGSMIListInit(&DmaCmdList);
#endif

#ifdef VBOX_WITH_VIDEOHWACCEL
        VBOXSHGSMILIST VhwaCmdList;
        vboxSHGSMIListInit(&VhwaCmdList);
#endif

        uint32_t flags = VBoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags->u32HostFlags;
        bOur = (flags & HGSMIHOSTFLAGS_IRQ);
        do
        {
            if (flags & HGSMIHOSTFLAGS_GCOMMAND_COMPLETED)
            {
                /* read the command offset */
                HGSMIOFFSET offCmd = VBoxVideoCmnPortReadUlong(VBoxCommonFromDeviceExt(pDevExt)->guestCtx.port);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    VBOXWDDM_HGSMICMD_TYPE enmType = vboxWddmHgsmiGetCmdTypeFromOffset(pDevExt, offCmd);
                    PVBOXSHGSMILIST pList;
                    HGSMIHEAP * pHeap = NULL;
                    switch (enmType)
                    {
#ifdef VBOX_WITH_VDMA
                        case VBOXWDDM_HGSMICMD_TYPE_DMACMD:
                            pList = &DmaCmdList;
                            pHeap = &pDevExt->u.primary.Vdma.CmdHeap;
                            break;
#endif
                        case VBOXWDDM_HGSMICMD_TYPE_CTL:
                            pList = &CtlList;
                            pHeap = &VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx;
                            break;
                        default:
                            AssertBreakpoint();
                    }

                    if (pHeap)
                    {
                        uint16_t chInfo;
                        uint8_t *pvCmd = HGSMIBufferDataAndChInfoFromOffset (&pHeap->area, offCmd, &chInfo);
                        Assert(pvCmd);
                        if (pvCmd)
                        {
                            switch (chInfo)
                            {
#ifdef VBOX_WITH_VDMA
                                case VBVA_VDMA_CMD:
                                case VBVA_VDMA_CTL:
                                {
                                    int rc = VBoxSHGSMICommandProcessCompletion (pHeap, (VBOXSHGSMIHEADER*)pvCmd, TRUE /*bool bIrq*/ , pList);
                                    AssertRC(rc);
                                    break;
                                }
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
                                case VBVA_VHWA_CMD:
                                {
                                    vboxVhwaPutList(&VhwaCmdList, (VBOXVHWACMD*)pvCmd);
                                    break;
                                }
#endif /* # ifdef VBOX_WITH_VIDEOHWACCEL */
                                default:
                                    AssertBreakpoint();
                            }
                        }
                    }
                }
            }
            else if (flags & HGSMIHOSTFLAGS_COMMANDS_PENDING)
            {
                AssertBreakpoint();
                /* @todo: FIXME: implement !!! */
            }
            else
                break;

            flags = VBoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags->u32HostFlags;
        } while (1);

        if (!vboxSHGSMIListIsEmpty(&CtlList))
        {
            vboxSHGSMIListCat(&pDevExt->CtlList, &CtlList);
            bNeedDpc = TRUE;
        }
#ifdef VBOX_WITH_VDMA
        if (!vboxSHGSMIListIsEmpty(&DmaCmdList))
        {
            vboxSHGSMIListCat(&pDevExt->DmaCmdList, &DmaCmdList);
            bNeedDpc = TRUE;
        }
#endif
        if (!vboxSHGSMIListIsEmpty(&VhwaCmdList))
        {
            vboxSHGSMIListCat(&pDevExt->VhwaCmdList, &VhwaCmdList);
            bNeedDpc = TRUE;
        }

        bNeedDpc |= !vboxVdmaDdiCmdIsCompletedListEmptyIsr(pDevExt);

        if (pDevExt->bNotifyDxDpc)
        {
//            Assert(bNeedDpc == TRUE);
//            pDevExt->bNotifyDxDpc = TRUE;
//            pDevExt->bSetNotifyDxDpc = FALSE;
            bNeedDpc = TRUE;
        }

        if (bOur)
        {
            VBoxHGSMIClearIrq(&VBoxCommonFromDeviceExt(pDevExt)->hostCtx);
#if 0 //def DEBUG_misha
            /* this is not entirely correct since host may concurrently complete some commands and raise a new IRQ while we are here,
             * still this allows to check that the host flags are correctly cleared after the ISR */
            Assert(VBoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags);
            uint32_t flags = VBoxCommonFromDeviceExt(pDevExt)->hostCtx.pfHostFlags->u32HostFlags;
            Assert(flags == 0);
#endif
        }

        if (bNeedDpc)
        {
            pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
        }
    }

//    LOGF(("LEAVE, context(0x%p), bOur(0x%x)", MiniportDeviceContext, (ULONG)bOur));

    return bOur;
}


typedef struct VBOXWDDM_DPCDATA
{
    VBOXSHGSMILIST CtlList;
#ifdef VBOX_WITH_VDMA
    VBOXSHGSMILIST DmaCmdList;
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    VBOXSHGSMILIST VhwaCmdList;
#endif
    LIST_ENTRY CompletedDdiCmdQueue;
    BOOL bNotifyDpc;
} VBOXWDDM_DPCDATA, *PVBOXWDDM_DPCDATA;

typedef struct VBOXWDDM_GETDPCDATA_CONTEXT
{
    PVBOXMP_DEVEXT pDevExt;
    VBOXWDDM_DPCDATA data;
} VBOXWDDM_GETDPCDATA_CONTEXT, *PVBOXWDDM_GETDPCDATA_CONTEXT;

BOOLEAN vboxWddmGetDPCDataCallback(PVOID Context)
{
    PVBOXWDDM_GETDPCDATA_CONTEXT pdc = (PVBOXWDDM_GETDPCDATA_CONTEXT)Context;

    vboxSHGSMIListDetach2List(&pdc->pDevExt->CtlList, &pdc->data.CtlList);
#ifdef VBOX_WITH_VDMA
    vboxSHGSMIListDetach2List(&pdc->pDevExt->DmaCmdList, &pdc->data.DmaCmdList);
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    vboxSHGSMIListDetach2List(&pdc->pDevExt->VhwaCmdList, &pdc->data.VhwaCmdList);
#endif
    vboxVdmaDdiCmdGetCompletedListIsr(pdc->pDevExt, &pdc->data.CompletedDdiCmdQueue);

    pdc->data.bNotifyDpc = pdc->pDevExt->bNotifyDxDpc;
    pdc->pDevExt->bNotifyDxDpc = FALSE;
    return TRUE;
}

VOID DxgkDdiDpcRoutine(
    IN CONST PVOID  MiniportDeviceContext
    )
{
//    LOGF(("ENTER, context(0x%p)", MiniportDeviceContext));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;

    VBOXWDDM_GETDPCDATA_CONTEXT context = {0};
    BOOLEAN bRet;

    context.pDevExt = pDevExt;

    /* get DPC data at IRQL */
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxWddmGetDPCDataCallback,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    Assert(Status == STATUS_SUCCESS);

    if (context.data.bNotifyDpc)
        pDevExt->u.primary.DxgkInterface.DxgkCbNotifyDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

    if (!vboxSHGSMIListIsEmpty(&context.data.CtlList))
    {
        int rc = VBoxSHGSMICommandPostprocessCompletion (&VBoxCommonFromDeviceExt(pDevExt)->guestCtx.heapCtx, &context.data.CtlList);
        AssertRC(rc);
    }
#ifdef VBOX_WITH_VDMA
    if (!vboxSHGSMIListIsEmpty(&context.data.DmaCmdList))
    {
        int rc = VBoxSHGSMICommandPostprocessCompletion (&pDevExt->u.primary.Vdma.CmdHeap, &context.data.DmaCmdList);
        AssertRC(rc);
    }
#endif
#ifdef VBOX_WITH_VIDEOHWACCEL
    if (!vboxSHGSMIListIsEmpty(&context.data.VhwaCmdList))
    {
        vboxVhwaCompletionListProcess(pDevExt, &context.data.VhwaCmdList);
    }
#endif

    vboxVdmaDdiCmdHandleCompletedList(pDevExt, &context.data.CompletedDdiCmdQueue);

//    LOGF(("LEAVE, context(0x%p)", MiniportDeviceContext));
}

NTSTATUS DxgkDdiQueryChildRelations(
    IN CONST PVOID MiniportDeviceContext,
    IN OUT PDXGK_CHILD_DESCRIPTOR ChildRelations,
    IN ULONG ChildRelationsSize
    )
{
    /* The DxgkDdiQueryChildRelations function should be made pageable. */
    PAGED_CODE();

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)MiniportDeviceContext;

    LOGF(("ENTER, context(0x%x)", MiniportDeviceContext));
    Assert(ChildRelationsSize == (VBoxCommonFromDeviceExt(pDevExt)->cDisplays + 1)*sizeof(DXGK_CHILD_DESCRIPTOR));
    for (int i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        ChildRelations[i].ChildDeviceType = TypeVideoOutput;
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.InterfaceTechnology = D3DKMDT_VOT_HD15; /* VGA */
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_INTERRUPTIBLE; /* ?? D3DKMDT_MOA_NONE*/
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
        ChildRelations[i].ChildCapabilities.HpdAwareness = HpdAwarenessInterruptible; /* ?? HpdAwarenessAlwaysConnected; */
        ChildRelations[i].AcpiUid =  0; /* */
        ChildRelations[i].ChildUid = i; /* should be == target id */
    }
    LOGF(("LEAVE, context(0x%x)", MiniportDeviceContext));
    return STATUS_SUCCESS;
}

NTSTATUS DxgkDdiQueryChildStatus(
    IN CONST PVOID  MiniportDeviceContext,
    IN PDXGK_CHILD_STATUS  ChildStatus,
    IN BOOLEAN  NonDestructiveOnly
    )
{
    /* The DxgkDdiQueryChildStatus should be made pageable. */
    PAGED_CODE();

    vboxVDbgBreakFv();

    LOGF(("ENTER, context(0x%x)", MiniportDeviceContext));

    NTSTATUS Status = STATUS_SUCCESS;
    switch (ChildStatus->Type)
    {
        case StatusConnection:
            ChildStatus->HotPlug.Connected = TRUE;
            LOGF(("StatusConnection"));
            break;
        case StatusRotation:
            ChildStatus->Rotation.Angle = 0;
            LOGF(("StatusRotation"));
            break;
        default:
            LOGREL(("ERROR: status type: %d", ChildStatus->Type));
            AssertBreakpoint();
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    LOGF(("LEAVE, context(0x%x)", MiniportDeviceContext));

    return Status;
}

NTSTATUS DxgkDdiQueryDeviceDescriptor(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG ChildUid,
    IN OUT PDXGK_DEVICE_DESCRIPTOR DeviceDescriptor
    )
{
    /* The DxgkDdiQueryDeviceDescriptor should be made pageable. */
    PAGED_CODE();

    vboxVDbgBreakFv();

    LOGF(("ENTER, context(0x%x)", MiniportDeviceContext));

    LOGF(("LEAVE, context(0x%x)", MiniportDeviceContext));

    /* we do not support EDID */
    return STATUS_MONITOR_NO_DESCRIPTOR;
}

NTSTATUS DxgkDdiSetPowerState(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG DeviceUid,
    IN DEVICE_POWER_STATE DevicePowerState,
    IN POWER_ACTION ActionType
    )
{
    /* The DxgkDdiSetPowerState function should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", MiniportDeviceContext));

    vboxVDbgBreakFv();

    LOGF(("LEAVE, context(0x%x)", MiniportDeviceContext));

    return STATUS_SUCCESS;
}

NTSTATUS DxgkDdiNotifyAcpiEvent(
    IN CONST PVOID  MiniportDeviceContext,
    IN DXGK_EVENT_TYPE  EventType,
    IN ULONG  Event,
    IN PVOID  Argument,
    OUT PULONG  AcpiFlags
    )
{
    LOGF(("ENTER, MiniportDeviceContext(0x%x)", MiniportDeviceContext));

    vboxVDbgBreakF();

    LOGF(("LEAVE, MiniportDeviceContext(0x%x)", MiniportDeviceContext));

    return STATUS_SUCCESS;
}

VOID DxgkDdiResetDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* DxgkDdiResetDevice can be called at any IRQL, so it must be in nonpageable memory.  */
    vboxVDbgBreakF();



    LOGF(("ENTER, context(0x%x)", MiniportDeviceContext));
    LOGF(("LEAVE, context(0x%x)", MiniportDeviceContext));
}

VOID DxgkDdiUnload(
    VOID
    )
{
    /* DxgkDdiUnload should be made pageable. */
    PAGED_CODE();
    LOGF((": unloading"));

    vboxVDbgBreakFv();

    PRTLOGGER pLogger = RTLogRelSetDefaultInstance(NULL);
    if (pLogger)
    {
        RTLogDestroy(pLogger);
    }
    pLogger = RTLogSetDefaultInstance(NULL);
    if (pLogger)
    {
        RTLogDestroy(pLogger);
    }
}

NTSTATUS DxgkDdiQueryInterface(
    IN CONST PVOID MiniportDeviceContext,
    IN PQUERY_INTERFACE QueryInterface
    )
{
    LOGF(("ENTER, MiniportDeviceContext(0x%x)", MiniportDeviceContext));

    vboxVDbgBreakFv();

    LOGF(("LEAVE, MiniportDeviceContext(0x%x)", MiniportDeviceContext));

    return STATUS_NOT_SUPPORTED;
}

VOID DxgkDdiControlEtwLogging(
    IN BOOLEAN  Enable,
    IN ULONG  Flags,
    IN UCHAR  Level
    )
{
    LOGF(("ENTER"));

    vboxVDbgBreakF();

    LOGF(("LEAVE"));
}

/**
 * DxgkDdiQueryAdapterInfo
 */
NTSTATUS APIENTRY DxgkDdiQueryAdapterInfo(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_QUERYADAPTERINFO*  pQueryAdapterInfo)
{
    /* The DxgkDdiQueryAdapterInfo should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x), Query type (%d)", hAdapter, pQueryAdapterInfo->Type));
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pContext = (PVBOXMP_DEVEXT)hAdapter;

    vboxVDbgBreakFv();

    switch (pQueryAdapterInfo->Type)
    {
        case DXGKQAITYPE_DRIVERCAPS:
        {
            DXGK_DRIVERCAPS *pCaps = (DXGK_DRIVERCAPS*)pQueryAdapterInfo->pOutputData;

            pCaps->HighestAcceptableAddress.QuadPart = ~((uintptr_t)0);
            pCaps->MaxAllocationListSlotId = 16;
            pCaps->ApertureSegmentCommitLimit = 0;
            pCaps->MaxPointerWidth  = VBOXWDDM_C_POINTER_MAX_WIDTH;
            pCaps->MaxPointerHeight = VBOXWDDM_C_POINTER_MAX_HEIGHT;
            pCaps->PointerCaps.Value = 3; /* Monochrome , Color*/ /* MaskedColor == Value | 4, dosable for now */
            pCaps->InterruptMessageNumber = 0;
            pCaps->NumberOfSwizzlingRanges = 0;
            pCaps->MaxOverlays = 0;
#ifdef VBOX_WITH_VIDEOHWACCEL
            for (int i = 0; i < VBoxCommonFromDeviceExt(pContext)->cDisplays; ++i)
            {
                if ( pContext->aSources[i].Vhwa.Settings.fFlags & VBOXVHWA_F_ENABLED)
                    pCaps->MaxOverlays += pContext->aSources[i].Vhwa.Settings.cOverlaysSupported;
            }
#endif
            pCaps->GammaRampCaps.Value = 0;
            pCaps->PresentationCaps.Value = 0;
            pCaps->PresentationCaps.NoScreenToScreenBlt = 1;
            pCaps->PresentationCaps.NoOverlapScreenBlt = 1;
            pCaps->MaxQueuedFlipOnVSync = 0; /* do we need it? */
            pCaps->FlipCaps.Value = 0;
            /* ? pCaps->FlipCaps.FlipOnVSyncWithNoWait = 1; */
            pCaps->SchedulingCaps.Value = 0;
            /* we might need it for Aero.
             * Setting this flag means we support DeviceContext, i.e.
             *  DxgkDdiCreateContext and DxgkDdiDestroyContext
             */
            pCaps->SchedulingCaps.MultiEngineAware = 1;
            pCaps->MemoryManagementCaps.Value = 0;
            /* @todo: this correlates with pCaps->SchedulingCaps.MultiEngineAware */
            pCaps->MemoryManagementCaps.PagingNode = 0;
            /* @todo: this correlates with pCaps->SchedulingCaps.MultiEngineAware */
            pCaps->GpuEngineTopology.NbAsymetricProcessingNodes = VBOXWDDM_NUM_NODES;

            break;
        }
        case DXGKQAITYPE_QUERYSEGMENT:
        {
            /* no need for DXGK_QUERYSEGMENTIN as it contains AGP aperture info, which (AGP aperture) we do not support
             * DXGK_QUERYSEGMENTIN *pQsIn = (DXGK_QUERYSEGMENTIN*)pQueryAdapterInfo->pInputData; */
            DXGK_QUERYSEGMENTOUT *pQsOut = (DXGK_QUERYSEGMENTOUT*)pQueryAdapterInfo->pOutputData;
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
# define VBOXWDDM_SEGMENTS_COUNT 2
#else
# define VBOXWDDM_SEGMENTS_COUNT 1
#endif
            if (!pQsOut->pSegmentDescriptor)
            {
                /* we are requested to provide the number of segments we support */
                pQsOut->NbSegment = VBOXWDDM_SEGMENTS_COUNT;
            }
            else if (pQsOut->NbSegment != VBOXWDDM_SEGMENTS_COUNT)
            {
                AssertBreakpoint();
                LOGREL(("NbSegment (%d) != 1", pQsOut->NbSegment));
                Status = STATUS_INVALID_PARAMETER;
            }
            else
            {
                DXGK_SEGMENTDESCRIPTOR* pDr = pQsOut->pSegmentDescriptor;
                /* we are requested to provide segment information */
                pDr->BaseAddress.QuadPart = 0; /* VBE_DISPI_LFB_PHYSICAL_ADDRESS; */
                pDr->CpuTranslatedAddress.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS;
                /* make sure the size is page aligned */
                /* @todo: need to setup VBVA buffers and adjust the mem size here */
                pDr->Size = vboxWddmVramCpuVisibleSegmentSize(pContext);
                pDr->NbOfBanks = 0;
                pDr->pBankRangeTable = 0;
                pDr->CommitLimit = pDr->Size;
                pDr->Flags.Value = 0;
                pDr->Flags.CpuVisible = 1;
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
                ++pDr;
                /* create cpu-invisible segment of the same size */
                pDr->BaseAddress.QuadPart = 0;
                pDr->CpuTranslatedAddress.QuadPart = 0;
                /* make sure the size is page aligned */
                /* @todo: need to setup VBVA buffers and adjust the mem size here */
                pDr->Size = vboxWddmVramCpuInvisibleSegmentSize(pContext);
                pDr->NbOfBanks = 0;
                pDr->pBankRangeTable = 0;
                pDr->CommitLimit = pDr->Size;
                pDr->Flags.Value = 0;
#endif

                pQsOut->PagingBufferSegmentId = 0;
                pQsOut->PagingBufferSize = 1024;
                pQsOut->PagingBufferPrivateDataSize = 0; /* @todo: do we need a private buffer ? */
            }
            break;
        }
        case DXGKQAITYPE_UMDRIVERPRIVATE:
            Assert (pQueryAdapterInfo->OutputDataSize >= sizeof (VBOXWDDM_QI));
            if (pQueryAdapterInfo->OutputDataSize >= sizeof (VBOXWDDM_QI))
            {
                VBOXWDDM_QI * pQi = (VBOXWDDM_QI*)pQueryAdapterInfo->pOutputData;
                memset (pQi, 0, sizeof (VBOXWDDM_QI));
                pQi->u32Version = VBOXVIDEOIF_VERSION;
                pQi->cInfos = VBoxCommonFromDeviceExt(pContext)->cDisplays;
#ifdef VBOX_WITH_VIDEOHWACCEL
                for (int i = 0; i < VBoxCommonFromDeviceExt(pContext)->cDisplays; ++i)
                {
                    pQi->aInfos[i] = pContext->aSources[i].Vhwa.Settings;
                }
#endif
            }
            else
            {
                LOGREL(("buffer too small"));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        default:
            LOGREL(("unsupported Type (%d)", pQueryAdapterInfo->Type));
            AssertBreakpoint();
            Status = STATUS_NOT_SUPPORTED;
            break;
    }
    LOGF(("LEAVE, context(0x%x), Status(0x%x)", hAdapter, Status));
    return Status;
}

/**
 * DxgkDdiCreateDevice
 */
NTSTATUS APIENTRY DxgkDdiCreateDevice(
    CONST HANDLE  hAdapter,
    DXGKARG_CREATEDEVICE*  pCreateDevice)
{
    /* DxgkDdiCreateDevice should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pContext = (PVBOXMP_DEVEXT)hAdapter;

    vboxVDbgBreakFv();

    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)vboxWddmMemAllocZero(sizeof (VBOXWDDM_DEVICE));
    pCreateDevice->hDevice = pDevice;
    if (pCreateDevice->Flags.SystemDevice)
        pDevice->enmType = VBOXWDDM_DEVICE_TYPE_SYSTEM;
//    else
//    {
//        AssertBreakpoint(); /* we do not support custom contexts for now */
//        LOGREL(("we do not support custom devices for now, hAdapter (0x%x)", hAdapter));
//    }

    pDevice->pAdapter = pContext;
    pDevice->hDevice = pCreateDevice->hDevice;

    pCreateDevice->hDevice = pDevice;
    pCreateDevice->pInfo = NULL;

    LOGF(("LEAVE, context(0x%x), Status(0x%x)", hAdapter, Status));

    return Status;
}

PVBOXWDDM_RESOURCE vboxWddmResourceCreate(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_RCINFO pRcInfo)
{
    PVBOXWDDM_RESOURCE pResource = (PVBOXWDDM_RESOURCE)vboxWddmMemAllocZero(RT_OFFSETOF(VBOXWDDM_RESOURCE, aAllocations[pRcInfo->cAllocInfos]));
    if (!pResource)
    {
        AssertFailed();
        return NULL;
    }
    pResource->cRefs = 1;
    pResource->cAllocations = pRcInfo->cAllocInfos;
    pResource->fFlags = pRcInfo->fFlags;
    pResource->RcDesc = pRcInfo->RcDesc;
    return pResource;
}

VOID vboxWddmResourceRetain(PVBOXWDDM_RESOURCE pResource)
{
    ASMAtomicIncU32(&pResource->cRefs);
}

static VOID vboxWddmResourceDestroy(PVBOXWDDM_RESOURCE pResource)
{
    vboxWddmMemFree(pResource);
}

VOID vboxWddmResourceWaitDereference(PVBOXWDDM_RESOURCE pResource)
{
    vboxWddmCounterU32Wait(&pResource->cRefs, 1);
}

VOID vboxWddmResourceRelease(PVBOXWDDM_RESOURCE pResource)
{
    uint32_t cRefs = ASMAtomicDecU32(&pResource->cRefs);
    Assert(cRefs < UINT32_MAX/2);
    if (!cRefs)
    {
        vboxWddmResourceDestroy(pResource);
    }
}

void vboxWddmAllocationDeleteFromResource(PVBOXWDDM_RESOURCE pResource, PVBOXWDDM_ALLOCATION pAllocation)
{
    Assert(pAllocation->pResource == pResource);
    if (pResource)
    {
        Assert(&pResource->aAllocations[pAllocation->iIndex] == pAllocation);
        vboxWddmResourceRelease(pResource);
    }
    else
    {
        vboxWddmMemFree(pAllocation);
    }
}

VOID vboxWddmAllocationCleanup(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    switch (pAllocation->enmType)
    {
        case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
        case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
        {
            if (pAllocation->bAssigned)
            {
                /* @todo: do we need to notify host? */
                vboxWddmAssignPrimary(pDevExt, &pDevExt->aSources[pAllocation->SurfDesc.VidPnSourceId], NULL, pAllocation->SurfDesc.VidPnSourceId);
            }

#if 0
            if (pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC)
            {
                if (pAllocation->hSharedHandle)
                {
                    vboxShRcTreeRemove(pDevExt, pAllocation);
                }
            }
#endif
            break;
        }
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
        case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
        {
            if (pAllocation->bAssigned)
            {
                Assert(pAllocation->SurfDesc.VidPnSourceId != D3DDDI_ID_UNINITIALIZED);
                /* @todo: do we need to notify host? */
                vboxWddmAssignShadow(pDevExt, &pDevExt->aSources[pAllocation->SurfDesc.VidPnSourceId], NULL, pAllocation->SurfDesc.VidPnSourceId);
            }
            break;
        }
#endif
        case VBOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER:
        {
            if (pAllocation->pSynchEvent)
            {
                ObDereferenceObject(pAllocation->pSynchEvent);
            }
            break;
        }
        default:
            break;
    }

    PVBOXWDDM_SWAPCHAIN pSwapchain = vboxWddmSwapchainRetainByAlloc(pDevExt, pAllocation);
    if (pSwapchain)
    {
        vboxWddmSwapchainAllocRemove(pDevExt, pSwapchain, pAllocation);
        vboxWddmSwapchainRelease(pSwapchain);
    }
}

VOID vboxWddmAllocationDestroy(PVBOXWDDM_ALLOCATION pAllocation)
{
    PAGED_CODE();

    vboxWddmAllocationDeleteFromResource(pAllocation->pResource, pAllocation);
}

PVBOXWDDM_ALLOCATION vboxWddmAllocationCreateFromResource(PVBOXWDDM_RESOURCE pResource, uint32_t iIndex)
{
    PVBOXWDDM_ALLOCATION pAllocation = NULL;
    if (pResource)
    {
        Assert(iIndex < pResource->cAllocations);
        if (iIndex < pResource->cAllocations)
        {
            pAllocation = &pResource->aAllocations[iIndex];
            memset(pAllocation, 0, sizeof (VBOXWDDM_ALLOCATION));
        }
        vboxWddmResourceRetain(pResource);
    }
    else
        pAllocation = (PVBOXWDDM_ALLOCATION)vboxWddmMemAllocZero(sizeof (VBOXWDDM_ALLOCATION));

    if (pAllocation)
    {
        if (pResource)
        {
            pAllocation->pResource = pResource;
            pAllocation->iIndex = iIndex;
        }
    }

    return pAllocation;
}

VOID vboxWddmAllocationWaitDereference(PVBOXWDDM_ALLOCATION pAllocation)
{
    vboxWddmCounterU32Wait(&pAllocation->cRefs, 1);
}


NTSTATUS vboxWddmAllocationCreate(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_RESOURCE pResource, uint32_t iIndex, DXGK_ALLOCATIONINFO* pAllocationInfo)
{
    PAGED_CODE();

    NTSTATUS Status = STATUS_SUCCESS;

    Assert(pAllocationInfo->PrivateDriverDataSize == sizeof (VBOXWDDM_ALLOCINFO));
    if (pAllocationInfo->PrivateDriverDataSize >= sizeof (VBOXWDDM_ALLOCINFO))
    {
        PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pAllocationInfo->pPrivateDriverData;
        PVBOXWDDM_ALLOCATION pAllocation = vboxWddmAllocationCreateFromResource(pResource, iIndex);
        Assert(pAllocation);
        if (pAllocation)
        {
            pAllocationInfo->pPrivateDriverData = NULL;
            pAllocationInfo->PrivateDriverDataSize = 0;
            pAllocationInfo->Alignment = 0;
            pAllocationInfo->PitchAlignedSize = 0;
            pAllocationInfo->HintedBank.Value = 0;
            pAllocationInfo->PreferredSegment.Value = 0;
            pAllocationInfo->SupportedReadSegmentSet = 1;
            pAllocationInfo->SupportedWriteSegmentSet = 1;
            pAllocationInfo->EvictionSegmentSet = 0;
            pAllocationInfo->MaximumRenamingListLength = 0;
            pAllocationInfo->hAllocation = pAllocation;
            pAllocationInfo->Flags.Value = 0;
            pAllocationInfo->pAllocationUsageHint = NULL;
            pAllocationInfo->AllocationPriority = D3DDDI_ALLOCATIONPRIORITY_NORMAL;

            pAllocation->enmType = pAllocInfo->enmType;
            pAllocation->offVram = VBOXVIDEOOFFSET_VOID;
            pAllocation->cRefs = 1;
            pAllocation->bVisible = FALSE;
            pAllocation->bAssigned = FALSE;
            InitializeListHead(&pAllocation->OpenList);

            switch (pAllocInfo->enmType)
            {
                case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
                case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
                case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                case VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:
                {
                    pAllocation->fRcFlags = pAllocInfo->fFlags;
                    pAllocation->SurfDesc = pAllocInfo->SurfDesc;

                    pAllocationInfo->Size = pAllocInfo->SurfDesc.cbSize;

                    switch (pAllocInfo->enmType)
                    {
                        case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
#if 0 //defined(VBOXWDDM_RENDER_FROM_SHADOW)
                            pAllocationInfo->SupportedReadSegmentSet = 2;
                            pAllocationInfo->SupportedWriteSegmentSet = 2;
#endif
#ifndef VBOXWDDM_RENDER_FROM_SHADOW
                            pAllocationInfo->Flags.CpuVisible = 1;
#endif
                            break;
                        case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
#ifdef VBOX_WITH_VIDEOHWACCEL
                            if (pAllocInfo->fFlags.Overlay)
                            {
                                /* actually we can not "properly" issue create overlay commands to the host here
                                 * because we do not know source VidPn id here, i.e.
                                 * the primary which is supposed to be overlayed,
                                 * however we need to get some info like pitch & size from the host here */
                                int rc = vboxVhwaHlpGetSurfInfo(pDevExt, pAllocation);
                                AssertRC(rc);
                                if (RT_SUCCESS(rc))
                                {
                                    pAllocationInfo->Flags.Overlay = 1;
                                    pAllocationInfo->Flags.CpuVisible = 1;
                                    pAllocationInfo->Size = pAllocation->SurfDesc.cbSize;

                                    pAllocationInfo->AllocationPriority = D3DDDI_ALLOCATIONPRIORITY_HIGH;
                                }
                                else
                                    Status = STATUS_UNSUCCESSFUL;
                            }
                            else
#endif
                            {
                                Assert(pAllocation->SurfDesc.bpp);
                                Assert(pAllocation->SurfDesc.pitch);
                                Assert(pAllocation->SurfDesc.cbSize);
                                if (!pAllocInfo->fFlags.SharedResource)
                                {
                                    pAllocationInfo->Flags.CpuVisible = 1;
                                }
                                else
                                {
                                    pAllocation->hSharedHandle = (HANDLE)pAllocInfo->hSharedHandle;
#if 0
                                    if (pAllocation->hSharedHandle)
                                    {
                                        vboxShRcTreePut(pDevExt, pAllocation);
                                    }
#endif
                                }
                            }
                            break;
                        case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                        case VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:
                            pAllocationInfo->Flags.CpuVisible = 1;
                            break;
                    }

                    if (Status == STATUS_SUCCESS)
                    {
                        pAllocation->UsageHint.Version = 0;
                        pAllocation->UsageHint.v1.Flags.Value = 0;
                        pAllocation->UsageHint.v1.Format = pAllocInfo->SurfDesc.format;
                        pAllocation->UsageHint.v1.SwizzledFormat = 0;
                        pAllocation->UsageHint.v1.ByteOffset = 0;
                        pAllocation->UsageHint.v1.Width = pAllocation->SurfDesc.width;
                        pAllocation->UsageHint.v1.Height = pAllocation->SurfDesc.height;
                        pAllocation->UsageHint.v1.Pitch = pAllocation->SurfDesc.pitch;
                        pAllocation->UsageHint.v1.Depth = 0;
                        pAllocation->UsageHint.v1.SlicePitch = 0;

                        Assert(!pAllocationInfo->pAllocationUsageHint);
                        pAllocationInfo->pAllocationUsageHint = &pAllocation->UsageHint;
                    }

                    break;
                }
                case VBOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER:
                {
                    pAllocationInfo->Size = pAllocInfo->cbBuffer;
                    pAllocation->enmSynchType = pAllocInfo->enmSynchType;
                    pAllocation->SurfDesc.cbSize = pAllocInfo->cbBuffer;
                    pAllocationInfo->Flags.CpuVisible = 1;
//                    pAllocationInfo->Flags.SynchronousPaging = 1;
                    pAllocationInfo->AllocationPriority = D3DDDI_ALLOCATIONPRIORITY_MAXIMUM;
                    switch (pAllocInfo->enmSynchType)
                    {
                        case VBOXUHGSMI_SYNCHOBJECT_TYPE_EVENT:
                            Status = ObReferenceObjectByHandle((HANDLE)pAllocInfo->hSynch, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
                                    (PVOID*)&pAllocation->pSynchEvent,
                                    NULL);
                            Assert(Status == STATUS_SUCCESS);
                            break;
                        case VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE:
                            Status = ObReferenceObjectByHandle((HANDLE)pAllocInfo->hSynch, EVENT_MODIFY_STATE, *ExSemaphoreObjectType, UserMode,
                                    (PVOID*)&pAllocation->pSynchSemaphore,
                                    NULL);
                            Assert(Status == STATUS_SUCCESS);
                            break;
                        case VBOXUHGSMI_SYNCHOBJECT_TYPE_NONE:
                            pAllocation->pSynchEvent = NULL;
                            Status = STATUS_SUCCESS;
                            break;
                        default:
                            LOGREL(("ERROR: invalid synch info type(%d)", pAllocInfo->enmSynchType));
                            AssertBreakpoint();
                            Status = STATUS_INVALID_PARAMETER;
                            break;
                    }
                    break;
                }

                default:
                    LOGREL(("ERROR: invalid alloc info type(%d)", pAllocInfo->enmType));
                    AssertBreakpoint();
                    Status = STATUS_INVALID_PARAMETER;
                    break;

            }

            if (Status != STATUS_SUCCESS)
                vboxWddmAllocationDeleteFromResource(pResource, pAllocation);
        }
        else
        {
            LOGREL(("ERROR: failed to create allocation description"));
            Status = STATUS_NO_MEMORY;
        }

    }
    else
    {
        LOGREL(("ERROR: PrivateDriverDataSize(%d) less than header size(%d)", pAllocationInfo->PrivateDriverDataSize, sizeof (VBOXWDDM_ALLOCINFO)));
        Status = STATUS_INVALID_PARAMETER;
    }

    return Status;
}

NTSTATUS APIENTRY DxgkDdiCreateAllocation(
    CONST HANDLE  hAdapter,
    DXGKARG_CREATEALLOCATION*  pCreateAllocation)
{
    /* DxgkDdiCreateAllocation should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_RESOURCE pResource = NULL;

    if (pCreateAllocation->PrivateDriverDataSize)
    {
        Assert(pCreateAllocation->PrivateDriverDataSize == sizeof (VBOXWDDM_RCINFO));
        Assert(pCreateAllocation->pPrivateDriverData);
        if (pCreateAllocation->PrivateDriverDataSize < sizeof (VBOXWDDM_RCINFO))
        {
            WARN(("invalid private data size (%d)", pCreateAllocation->PrivateDriverDataSize));
            return STATUS_INVALID_PARAMETER;
        }

        PVBOXWDDM_RCINFO pRcInfo = (PVBOXWDDM_RCINFO)pCreateAllocation->pPrivateDriverData;
//            Assert(pRcInfo->RcDesc.VidPnSourceId < VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
        if (pRcInfo->cAllocInfos != pCreateAllocation->NumAllocations)
        {
            WARN(("invalid number of allocations passed in, (%d), expected (%d)", pRcInfo->cAllocInfos, pCreateAllocation->NumAllocations));
            return STATUS_INVALID_PARAMETER;
        }

        /* a check to ensure we do not get the allocation size which is too big to overflow the 32bit value */
        if (VBOXWDDM_TRAILARRAY_MAXELEMENTSU32(VBOXWDDM_RESOURCE, aAllocations) < pRcInfo->cAllocInfos)
        {
            WARN(("number of allocations passed too big (%d), max is (%d)", pRcInfo->cAllocInfos, VBOXWDDM_TRAILARRAY_MAXELEMENTSU32(VBOXWDDM_RESOURCE, aAllocations)));
            return STATUS_INVALID_PARAMETER;
        }

        pResource = (PVBOXWDDM_RESOURCE)vboxWddmMemAllocZero(RT_OFFSETOF(VBOXWDDM_RESOURCE, aAllocations[pRcInfo->cAllocInfos]));
        if (!pResource)
        {
            WARN(("vboxWddmMemAllocZero failed for (%d) allocations", pRcInfo->cAllocInfos));
            return STATUS_NO_MEMORY;
        }

        pResource->cRefs = 1;
        pResource->cAllocations = pRcInfo->cAllocInfos;
        pResource->fFlags = pRcInfo->fFlags;
        pResource->RcDesc = pRcInfo->RcDesc;
    }


    for (UINT i = 0; i < pCreateAllocation->NumAllocations; ++i)
    {
        Status = vboxWddmAllocationCreate(pDevExt, pResource, i, &pCreateAllocation->pAllocationInfo[i]);
        if (Status != STATUS_SUCCESS)
        {
            WARN(("vboxWddmAllocationCreate(%d) failed, Status(0x%x)", i, Status));
            /* note: i-th allocation is expected to be cleared in a fail handling code above */
            for (UINT j = 0; j < i; ++j)
            {
                vboxWddmAllocationCleanup(pDevExt, (PVBOXWDDM_ALLOCATION)pCreateAllocation->pAllocationInfo[j].hAllocation);
                vboxWddmAllocationRelease((PVBOXWDDM_ALLOCATION)pCreateAllocation->pAllocationInfo[j].hAllocation);
            }
        }
    }

    pCreateAllocation->hResource = pResource;
    if (pResource && Status != STATUS_SUCCESS)
        vboxWddmResourceRelease(pResource);

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyAllocation(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_DESTROYALLOCATION*  pDestroyAllocation)
{
    /* DxgkDdiDestroyAllocation should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXWDDM_RESOURCE pRc = (PVBOXWDDM_RESOURCE)pDestroyAllocation->hResource;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;

    if (pRc)
    {
        Assert(pRc->cAllocations == pDestroyAllocation->NumAllocations);
    }

    for (UINT i = 0; i < pDestroyAllocation->NumAllocations; ++i)
    {
        PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pDestroyAllocation->pAllocationList[i];
        Assert(pAlloc->pResource == pRc);
        /* wait for all current allocation-related ops are completed */
        vboxWddmAllocationWaitDereference(pAlloc);
        vboxWddmAllocationCleanup(pDevExt, pAlloc);
        vboxWddmAllocationRelease(pAlloc);
    }

    if (pRc)
    {
        /* wait for all current resource-related ops are completed */
        vboxWddmResourceWaitDereference(pRc);
        vboxWddmResourceRelease(pRc);
    }

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

/**
 * DxgkDdiDescribeAllocation
 */
NTSTATUS
APIENTRY
DxgkDdiDescribeAllocation(
    CONST HANDLE  hAdapter,
    DXGKARG_DESCRIBEALLOCATION*  pDescribeAllocation)
{
//    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pDescribeAllocation->hAllocation;
    pDescribeAllocation->Width = pAllocation->SurfDesc.width;
    pDescribeAllocation->Height = pAllocation->SurfDesc.height;
    pDescribeAllocation->Format = pAllocation->SurfDesc.format;
    memset (&pDescribeAllocation->MultisampleMethod, 0, sizeof (pDescribeAllocation->MultisampleMethod));
    pDescribeAllocation->RefreshRate.Numerator = 60000;
    pDescribeAllocation->RefreshRate.Denominator = 1000;
    pDescribeAllocation->PrivateDriverFormatAttribute = 0;

//    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

/**
 * DxgkDdiGetStandardAllocationDriverData
 */
NTSTATUS
APIENTRY
DxgkDdiGetStandardAllocationDriverData(
    CONST HANDLE  hAdapter,
    DXGKARG_GETSTANDARDALLOCATIONDRIVERDATA*  pGetStandardAllocationDriverData)
{
    /* DxgkDdiGetStandardAllocationDriverData should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_ALLOCINFO pAllocInfo = NULL;

    switch (pGetStandardAllocationDriverData->StandardAllocationType)
    {
        case D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE:
        {
            LOGF(("D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE"));
            if(pGetStandardAllocationDriverData->pAllocationPrivateDriverData)
            {
                pAllocInfo = (PVBOXWDDM_ALLOCINFO)pGetStandardAllocationDriverData->pAllocationPrivateDriverData;
                memset (pAllocInfo, 0, sizeof (VBOXWDDM_ALLOCINFO));
                pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE;
                pAllocInfo->SurfDesc.width = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Width;
                pAllocInfo->SurfDesc.height = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Height;
                pAllocInfo->SurfDesc.format = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Format;
                pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Width, pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.cbSize = vboxWddmCalcSize(pAllocInfo->SurfDesc.pitch, pAllocInfo->SurfDesc.height, pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.depth = 0;
                pAllocInfo->SurfDesc.slicePitch = 0;
                pAllocInfo->SurfDesc.RefreshRate = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->RefreshRate;
                pAllocInfo->SurfDesc.VidPnSourceId = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->VidPnSourceId;
            }
            pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = sizeof (VBOXWDDM_ALLOCINFO);

            pGetStandardAllocationDriverData->ResourcePrivateDriverDataSize = 0;
            break;
        }
        case D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE:
        {
            LOGF(("D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE"));
            UINT bpp = vboxWddmCalcBitsPerPixel(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format);
            Assert(bpp);
            if (bpp != 0)
            {
                UINT Pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width, pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format);
                pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Pitch = Pitch;

                /* @todo: need [d/q]word align?? */

                if (pGetStandardAllocationDriverData->pAllocationPrivateDriverData)
                {
                    pAllocInfo = (PVBOXWDDM_ALLOCINFO)pGetStandardAllocationDriverData->pAllocationPrivateDriverData;
                    pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE;
                    pAllocInfo->SurfDesc.width = pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width;
                    pAllocInfo->SurfDesc.height = pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Height;
                    pAllocInfo->SurfDesc.format = pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format;
                    pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->SurfDesc.format);
                    pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width, pAllocInfo->SurfDesc.format);
                    pAllocInfo->SurfDesc.cbSize = vboxWddmCalcSize(pAllocInfo->SurfDesc.pitch, pAllocInfo->SurfDesc.height, pAllocInfo->SurfDesc.format);
                    pAllocInfo->SurfDesc.depth = 0;
                    pAllocInfo->SurfDesc.slicePitch = 0;
                    pAllocInfo->SurfDesc.RefreshRate.Numerator = 0;
                    pAllocInfo->SurfDesc.RefreshRate.Denominator = 1000;
                    pAllocInfo->SurfDesc.VidPnSourceId = D3DDDI_ID_UNINITIALIZED;

                    pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Pitch = pAllocInfo->SurfDesc.pitch;
                }
                pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = sizeof (VBOXWDDM_ALLOCINFO);

                pGetStandardAllocationDriverData->ResourcePrivateDriverDataSize = 0;
            }
            else
            {
                LOGREL(("Invalid format (%d)", pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format));
                Status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        case D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE:
        {
            LOGF(("D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE"));
            if(pGetStandardAllocationDriverData->pAllocationPrivateDriverData)
            {
                pAllocInfo = (PVBOXWDDM_ALLOCINFO)pGetStandardAllocationDriverData->pAllocationPrivateDriverData;
                pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE;
                pAllocInfo->SurfDesc.width = pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Width;
                pAllocInfo->SurfDesc.height = pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Height;
                pAllocInfo->SurfDesc.format = D3DDDIFMT_X8R8G8B8; /* staging has always always D3DDDIFMT_X8R8G8B8 */
                pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Width, pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.cbSize = vboxWddmCalcSize(pAllocInfo->SurfDesc.pitch, pAllocInfo->SurfDesc.height, pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.depth = 0;
                pAllocInfo->SurfDesc.slicePitch = 0;
                pAllocInfo->SurfDesc.RefreshRate.Numerator = 0;
                pAllocInfo->SurfDesc.RefreshRate.Denominator = 1000;
                pAllocInfo->SurfDesc.VidPnSourceId = D3DDDI_ID_UNINITIALIZED;

                pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Pitch = pAllocInfo->SurfDesc.pitch;
            }
            pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = sizeof (VBOXWDDM_ALLOCINFO);

            pGetStandardAllocationDriverData->ResourcePrivateDriverDataSize = 0;
            break;
        }
//#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN7)
//        case D3DKMDT_STANDARDALLOCATION_GDISURFACE:
//# error port to Win7 DDI
//              break;
//#endif
        default:
            LOGREL(("Invalid allocation type (%d)", pGetStandardAllocationDriverData->StandardAllocationType));
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiAcquireSwizzlingRange(
    CONST HANDLE  hAdapter,
    DXGKARG_ACQUIRESWIZZLINGRANGE*  pAcquireSwizzlingRange)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiReleaseSwizzlingRange(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RELEASESWIZZLINGRANGE*  pReleaseSwizzlingRange)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiPatch(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PATCH*  pPatch)
{
    /* DxgkDdiPatch should be made pageable. */
    PAGED_CODE();

    NTSTATUS Status = STATUS_SUCCESS;

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    /* Value == 2 is Present
     * Value == 4 is RedirectedPresent
     * we do not expect any other flags to be set here */
//    Assert(pPatch->Flags.Value == 2 || pPatch->Flags.Value == 4);
    if (pPatch->DmaBufferPrivateDataSubmissionEndOffset - pPatch->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        Assert(pPatch->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR));
        VBOXWDDM_DMA_PRIVATEDATA_BASEHDR *pPrivateDataBase = (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR*)((uint8_t*)pPatch->pDmaBufferPrivateData + pPatch->DmaBufferPrivateDataSubmissionStartOffset);
        switch (pPrivateDataBase->enmCmd)
        {
            case VBOXVDMACMD_TYPE_DMA_PRESENT_SHADOW2PRIMARY:
            {
                PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY pS2P = (PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 2);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_SOURCE_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pSrcAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pSrcAllocationList->SegmentId);
                pS2P->Shadow2Primary.ShadowAlloc.segmentIdAlloc = pSrcAllocationList->SegmentId;
                pS2P->Shadow2Primary.ShadowAlloc.offAlloc = (VBOXVIDEOOFFSET)pSrcAllocationList->PhysicalAddress.QuadPart;
//
//                pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart + 1];
//                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_DESTINATION_INDEX);
//                Assert(pPatchList->PatchOffset == 4);
//                const DXGK_ALLOCATIONLIST *pDstAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
//                Assert(pDstAllocationList->SegmentId);
//                pPrivateData->DstAllocInfo.segmentIdAlloc = pDstAllocationList->SegmentId;
//                pPrivateData->DstAllocInfo.offAlloc = (VBOXVIDEOOFFSET)pDstAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
            {
                PVBOXWDDM_DMA_PRIVATEDATA_BLT pBlt = (PVBOXWDDM_DMA_PRIVATEDATA_BLT)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 2);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_SOURCE_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pSrcAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pSrcAllocationList->SegmentId);
                pBlt->Blt.SrcAlloc.segmentIdAlloc = pSrcAllocationList->SegmentId;
                pBlt->Blt.SrcAlloc.offAlloc = (VBOXVIDEOOFFSET)pSrcAllocationList->PhysicalAddress.QuadPart;

                pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart + 1];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_DESTINATION_INDEX);
                Assert(pPatchList->PatchOffset == 4);
                const DXGK_ALLOCATIONLIST *pDstAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pDstAllocationList->SegmentId);
                pBlt->Blt.DstAlloc.segmentIdAlloc = pDstAllocationList->SegmentId;
                pBlt->Blt.DstAlloc.offAlloc = (VBOXVIDEOOFFSET)pDstAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
            {
                PVBOXWDDM_DMA_PRIVATEDATA_FLIP pFlip = (PVBOXWDDM_DMA_PRIVATEDATA_FLIP)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 1);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_SOURCE_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pSrcAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pSrcAllocationList->SegmentId);
                pFlip->Flip.Alloc.segmentIdAlloc = pSrcAllocationList->SegmentId;
                pFlip->Flip.Alloc.offAlloc = (VBOXVIDEOOFFSET)pSrcAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
            {
                PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateDataBase;
                Assert(pPatch->PatchLocationListSubmissionLength == 1);
                const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[pPatch->PatchLocationListSubmissionStart];
                Assert(pPatchList->AllocationIndex == DXGK_PRESENT_DESTINATION_INDEX);
                Assert(pPatchList->PatchOffset == 0);
                const DXGK_ALLOCATIONLIST *pDstAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                Assert(pDstAllocationList->SegmentId);
                pCF->ClrFill.Alloc.segmentIdAlloc = pDstAllocationList->SegmentId;
                pCF->ClrFill.Alloc.offAlloc = (VBOXVIDEOOFFSET)pDstAllocationList->PhysicalAddress.QuadPart;
                break;
            }
            case VBOXVDMACMD_TYPE_DMA_NOP:
                break;
            case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
            {
                uint8_t * pPrivateBuf = (uint8_t*)pPrivateDataBase;
                for (UINT i = pPatch->PatchLocationListSubmissionStart; i < pPatch->PatchLocationListSubmissionLength; ++i)
                {
                    const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[i];
                    Assert(pPatchList->AllocationIndex < pPatch->AllocationListSize);
                    const DXGK_ALLOCATIONLIST *pAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                    Assert(pAllocationList->SegmentId);
                    if (pAllocationList->SegmentId)
                    {
                        DXGK_ALLOCATIONLIST *pAllocation2Patch = (DXGK_ALLOCATIONLIST*)(pPrivateBuf + pPatchList->PatchOffset);
                        pAllocation2Patch->SegmentId = pAllocationList->SegmentId;
                        pAllocation2Patch->PhysicalAddress.QuadPart = pAllocationList->PhysicalAddress.QuadPart + pPatchList->AllocationOffset;
                        Assert(!(pAllocationList->PhysicalAddress.QuadPart & 0xfffUL)); /* <- just a check to ensure allocation offset does not go here */
                    }
                }
                break;
            }
            default:
            {
                AssertBreakpoint();
                uint8_t *pBuf = ((uint8_t *)pPatch->pDmaBuffer) + pPatch->DmaBufferSubmissionStartOffset;
                for (UINT i = pPatch->PatchLocationListSubmissionStart; i < pPatch->PatchLocationListSubmissionLength; ++i)
                {
                    const D3DDDI_PATCHLOCATIONLIST* pPatchList = &pPatch->pPatchLocationList[i];
                    Assert(pPatchList->AllocationIndex < pPatch->AllocationListSize);
                    const DXGK_ALLOCATIONLIST *pAllocationList = &pPatch->pAllocationList[pPatchList->AllocationIndex];
                    if (pAllocationList->SegmentId)
                    {
                        Assert(pPatchList->PatchOffset < (pPatch->DmaBufferSubmissionEndOffset - pPatch->DmaBufferSubmissionStartOffset));
                        *((VBOXVIDEOOFFSET*)(pBuf+pPatchList->PatchOffset)) = (VBOXVIDEOOFFSET)pAllocationList->PhysicalAddress.QuadPart;
                    }
                    else
                    {
                        /* sanity */
                        if (pPatch->Flags.Value == 2 || pPatch->Flags.Value == 4)
                            Assert(i == 0);
                    }
                }
                break;
            }
        }
    }
    else if (pPatch->DmaBufferPrivateDataSubmissionEndOffset == pPatch->DmaBufferPrivateDataSubmissionStartOffset)
    {
        /* this is a NOP, just return success */
        WARN(("null data size, treating as NOP"));
        return STATUS_SUCCESS;
    }
    else
    {
        WARN(("DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR) (%d)",
                pPatch->DmaBufferPrivateDataSubmissionEndOffset,
                pPatch->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }

    LOGF(("LEAVE, context(0x%x)", hAdapter));

    return Status;
}

typedef struct VBOXWDDM_CALL_ISR
{
    PVBOXMP_DEVEXT pDevExt;
    ULONG MessageNumber;
} VBOXWDDM_CALL_ISR, *PVBOXWDDM_CALL_ISR;

static BOOLEAN vboxWddmCallIsrCb(PVOID Context)
{
    PVBOXWDDM_CALL_ISR pdc = (PVBOXWDDM_CALL_ISR)Context;
    return DxgkDdiInterruptRoutine(pdc->pDevExt, pdc->MessageNumber);
}

NTSTATUS vboxWddmCallIsr(PVBOXMP_DEVEXT pDevExt)
{
    VBOXWDDM_CALL_ISR context;
    context.pDevExt = pDevExt;
    context.MessageNumber = 0;
    BOOLEAN bRet;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxWddmCallIsrCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

static NTSTATUS vboxWddmSubmitCmd(PVBOXMP_DEVEXT pDevExt, VBOXVDMAPIPE_CMD_DMACMD *pCmd)
{
    NTSTATUS Status = vboxVdmaGgCmdDmaNotifySubmitted(pDevExt, pCmd);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        NTSTATUS submStatus = vboxVdmaGgCmdSubmit(pDevExt, &pCmd->Hdr);
        Assert(submStatus == STATUS_SUCCESS);
        if (submStatus != STATUS_SUCCESS)
        {
            vboxVdmaGgCmdDmaNotifyCompleted(pDevExt, pCmd, DXGK_INTERRUPT_DMA_FAULTED);
        }
    }
    else
    {
        vboxVdmaGgCmdDestroy(pDevExt, &pCmd->Hdr);
    }
    return Status;
}

static NTSTATUS vboxWddmSubmitBltCmd(PVBOXMP_DEVEXT pDevExt, VBOXWDDM_CONTEXT *pContext, UINT u32FenceId, PVBOXWDDM_DMA_PRIVATEDATA_BLT pBlt, VBOXVDMAPIPE_FLAGS_DMACMD fBltFlags)
{
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXVDMAPIPE_CMD_DMACMD_BLT pBltCmd = (PVBOXVDMAPIPE_CMD_DMACMD_BLT)vboxVdmaGgCmdCreate(pDevExt, VBOXVDMAPIPE_CMD_TYPE_DMACMD, RT_OFFSETOF(VBOXVDMAPIPE_CMD_DMACMD_BLT, Blt.DstRects.UpdateRects.aRects[pBlt->Blt.DstRects.UpdateRects.cRects]));
    Assert(pBltCmd);
    if (pBltCmd)
    {
        VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pBlt->Blt.DstAlloc.srcId];
        vboxVdmaGgCmdDmaNotifyInit(&pBltCmd->Hdr, pContext->NodeOrdinal, u32FenceId, NULL, NULL);
        pBltCmd->Hdr.fFlags = fBltFlags;
        pBltCmd->Hdr.pContext = pContext;
        pBltCmd->Hdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_BLT;
        memcpy(&pBltCmd->Blt, &pBlt->Blt, RT_OFFSETOF(VBOXVDMA_BLT, DstRects.UpdateRects.aRects[pBlt->Blt.DstRects.UpdateRects.cRects]));
        vboxWddmSubmitCmd(pDevExt, &pBltCmd->Hdr);
    }
    else
    {
        Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, u32FenceId, DXGK_INTERRUPT_DMA_FAULTED);
    }
    return Status;
}

#ifdef VBOX_WITH_CRHGSMI
DECLCALLBACK(VOID) vboxWddmDmaCompleteChromiumCmd(PVBOXMP_DEVEXT pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
{
    PVBOXVDMACBUF_DR pDr = (PVBOXVDMACBUF_DR)pvContext;
    PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
    VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
    UINT cBufs = pBody->cBuffers;
    for (UINT i = 0; i < cBufs; ++i)
    {
        VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
        if (!pBufCmd->u32GuesData)
        {
            /* signal completion */
            PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pBufCmd->u64GuesData;
            switch (pAlloc->enmSynchType)
            {
                case VBOXUHGSMI_SYNCHOBJECT_TYPE_EVENT:
                    KeSetEvent(pAlloc->pSynchEvent, 3, FALSE);
                    break;
                case VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE:
                    KeReleaseSemaphore(pAlloc->pSynchSemaphore,
                        3,
                        1,
                        FALSE);
                    break;
                case VBOXUHGSMI_SYNCHOBJECT_TYPE_NONE:
                    break;
                default:
                    Assert(0);
            }
        }
    }

    vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
}
#endif

NTSTATUS
APIENTRY
DxgkDdiSubmitCommand(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SUBMITCOMMAND*  pSubmitCommand)
{
    /* DxgkDdiSubmitCommand runs at dispatch, should not be pageable. */
    NTSTATUS Status = STATUS_SUCCESS;

//    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pSubmitCommand->hContext;
    PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR pPrivateDataBase = NULL;
    VBOXVDMACMD_TYPE enmCmd = VBOXVDMACMD_TYPE_UNDEFINED;
    Assert(pContext);
    Assert(pContext->pDevice);
    Assert(pContext->pDevice->pAdapter == pDevExt);
    Assert(!pSubmitCommand->DmaBufferSegmentId);

    /* the DMA command buffer is located in system RAM, the host will need to pick it from there */
    //BufInfo.fFlags = 0; /* see VBOXVDMACBUF_FLAG_xx */
    if (pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        pPrivateDataBase = (PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR)((uint8_t*)pSubmitCommand->pDmaBufferPrivateData + pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset);
        Assert(pPrivateDataBase);
        enmCmd = pPrivateDataBase->enmCmd;
    }
    else if (pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset == pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset)
    {
        WARN(("null data size, treating as NOP"));
        enmCmd = VBOXVDMACMD_TYPE_DMA_NOP;
    }
    else
    {
        WARN(("DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR) (%d)",
                pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset,
                pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }

    switch (enmCmd)
    {
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
        case VBOXVDMACMD_TYPE_DMA_PRESENT_SHADOW2PRIMARY:
        {
            PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY pS2P = (PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY)pPrivateDataBase;
            VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pS2P->Shadow2Primary.VidPnSourceId];
            PVBOXWDDM_ALLOCATION pSrcAlloc = pS2P->Shadow2Primary.ShadowAlloc.pAlloc;
            vboxWddmAssignShadow(pDevExt, pSource, pSrcAlloc, pS2P->Shadow2Primary.VidPnSourceId);
            vboxWddmCheckUpdateShadowAddress(pDevExt, pSource, pS2P->Shadow2Primary.VidPnSourceId,
                    pS2P->Shadow2Primary.ShadowAlloc.segmentIdAlloc, pS2P->Shadow2Primary.ShadowAlloc.offAlloc);
            uint32_t cUnlockedVBVADisabled = ASMAtomicReadU32(&pDevExt->cUnlockedVBVADisabled);
            if (!cUnlockedVBVADisabled)
                VBOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &pS2P->Shadow2Primary.SrcRect);
            else
            {
                Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
                VBOXVBVA_OP_WITHLOCK_ATDPC(ReportDirtyRect, pDevExt, pSource, &pS2P->Shadow2Primary.SrcRect);
            }
            /* get DPC data at IRQL */

            Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            break;
        }
#endif
        case VBOXVDMACMD_TYPE_DMA_PRESENT_BLT:
        {
            VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR *pPrivateData = (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR*)pPrivateDataBase;
            PVBOXWDDM_DMA_PRIVATEDATA_BLT pBlt = (PVBOXWDDM_DMA_PRIVATEDATA_BLT)pPrivateData;
            PVBOXWDDM_ALLOCATION pDstAlloc = pBlt->Blt.DstAlloc.pAlloc;
            PVBOXWDDM_ALLOCATION pSrcAlloc = pBlt->Blt.SrcAlloc.pAlloc;

            uint32_t cContexts3D = ASMAtomicReadU32(&pDevExt->cContexts3D);
            BOOLEAN bComplete = TRUE;
            switch (pDstAlloc->enmType)
            {
                case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
                case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
                {
                    if (pDstAlloc->bAssigned)
                    {
                        VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pDstAlloc->SurfDesc.VidPnSourceId];
                        Assert(pSource->pPrimaryAllocation == pDstAlloc);
                        switch (pSrcAlloc->enmType)
                        {
                            case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                            {
                                VBOXVDMAPIPE_FLAGS_DMACMD fBltFlags;
                                fBltFlags.Value = 0;
                                fBltFlags.b3DRelated = !!cContexts3D;
                                Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_SYSTEM);
                                vboxWddmAssignShadow(pDevExt, pSource, pSrcAlloc, pDstAlloc->SurfDesc.VidPnSourceId);
                                vboxWddmCheckUpdateShadowAddress(pDevExt, pSource,
                                        pDstAlloc->SurfDesc.VidPnSourceId, pBlt->Blt.SrcAlloc.segmentIdAlloc, pBlt->Blt.SrcAlloc.offAlloc);
                                if (vboxVhwaHlpOverlayListIsEmpty(pDevExt, pDstAlloc->SurfDesc.VidPnSourceId))
                                {
                                    RECT rect;
                                    if (pBlt->Blt.DstRects.UpdateRects.cRects)
                                    {
                                        rect = pBlt->Blt.DstRects.UpdateRects.aRects[0];
                                        for (UINT i = 1; i < pBlt->Blt.DstRects.UpdateRects.cRects; ++i)
                                        {
                                            vboxWddmRectUnited(&rect, &rect, &pBlt->Blt.DstRects.UpdateRects.aRects[i]);
                                        }
                                    }
                                    else
                                        rect = pBlt->Blt.DstRects.ContextRect;

                                    uint32_t cUnlockedVBVADisabled = ASMAtomicReadU32(&pDevExt->cUnlockedVBVADisabled);
                                    if (!cUnlockedVBVADisabled)
                                    {
                                        VBOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &rect);
                                    }
                                    else
                                    {
                                        VBOXVBVA_OP_WITHLOCK_ATDPC(ReportDirtyRect, pDevExt, pSource, &rect);
                                    }
                                }
                                else
                                {
                                    fBltFlags.b2DRelated = 1;
                                }

                                if (fBltFlags.Value)
                                {
                                    Status = vboxWddmSubmitBltCmd(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, pBlt, fBltFlags);
                                    bComplete = FALSE;
                                }
                                break;
                            }
                            case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
                            {
                                Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D);
                                Assert(pSrcAlloc->fRcFlags.RenderTarget);
                                if (pSrcAlloc->fRcFlags.RenderTarget)
                                {
                                    VBOXVDMAPIPE_FLAGS_DMACMD fBltFlags;
                                    fBltFlags.Value = 0;
                                    fBltFlags.b3DRelated = 1;
                                    Status = vboxWddmSubmitBltCmd(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, pBlt, fBltFlags);
                                    bComplete = FALSE;
                                }
                                break;
                            }
                            default:
                                AssertBreakpoint();
                                break;
                        }
                    }
                    break;
                }
                case VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:
                {
                    Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D);
                    Assert(pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC);
                    Assert(pSrcAlloc->fRcFlags.RenderTarget);
                    Assert(vboxWddmRectIsEqual(&pBlt->Blt.SrcRect, &pBlt->Blt.DstRects.ContextRect));
                    Assert(pBlt->Blt.DstRects.UpdateRects.cRects == 1);
                    Assert(vboxWddmRectIsEqual(&pBlt->Blt.SrcRect, pBlt->Blt.DstRects.UpdateRects.aRects));
                    break;
                }
                case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
                {
                    Assert(pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE);
                    Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_SYSTEM);
                    VBOXVDMAPIPE_FLAGS_DMACMD fBltFlags;
                    fBltFlags.Value = 0;
                    if (pDstAlloc->SurfDesc.VidPnSourceId != D3DDDI_ID_UNINITIALIZED &&
                            !vboxVhwaHlpOverlayListIsEmpty(pDevExt, pDstAlloc->SurfDesc.VidPnSourceId))
                    {
                        fBltFlags.b2DRelated = 1;
                        Status = vboxWddmSubmitBltCmd(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, pBlt, fBltFlags);
                        bComplete = FALSE;
                    }
                    break;
                }
                default:
                    AssertBreakpoint();
                    break;
            }

            if (bComplete)
            {
                Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            }
            break;
        }
        case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
        {
#ifdef VBOX_WITH_CRHGSMI
            VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD *pChromiumCmd = (VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD*)pPrivateDataBase;
            UINT cbCmd = VBOXVDMACMD_SIZE_FROMBODYSIZE(RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CMD, aBuffers[pChromiumCmd->Base.u32CmdReserved]));

            PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate (&pDevExt->u.primary.Vdma, cbCmd);
            if (!pDr)
            {
                /* @todo: try flushing.. */
                LOGREL(("vboxVdmaCBufDrCreate returned NULL"));
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            // vboxVdmaCBufDrCreate zero initializes the pDr
            pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
            pDr->cbBuf = cbCmd;
            pDr->rc = VERR_NOT_IMPLEMENTED;

            PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
            pHdr->enmType = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
            pHdr->u32CmdSpecific = 0;
            VBOXVDMACMD_CHROMIUM_CMD *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_CHROMIUM_CMD);
            pBody->cBuffers = pChromiumCmd->Base.u32CmdReserved;
            for (UINT i = 0; i < pChromiumCmd->Base.u32CmdReserved; ++i)
            {
                VBOXVDMACMD_CHROMIUM_BUFFER *pBufCmd = &pBody->aBuffers[i];
                VBOXWDDM_UHGSMI_BUFFER_SUBMIT_INFO *pBufInfo = &pChromiumCmd->aBufInfos[i];

                pBufCmd->offBuffer = pBufInfo->Alloc.offAlloc;
                pBufCmd->cbBuffer = pBufInfo->cbData;
                pBufCmd->u32GuesData = pBufInfo->bDoNotSignalCompletion;
                pBufCmd->u64GuesData = (uint64_t)pBufInfo->Alloc.pAlloc;
            }

            PVBOXVDMADDI_CMD pDdiCmd = VBOXVDMADDI_CMD_FROM_BUF_DR(pDr);
            vboxVdmaDdiCmdInit(pDdiCmd, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, vboxWddmDmaCompleteChromiumCmd, pDr);
            NTSTATUS Status = vboxVdmaDdiCmdSubmitted(pDevExt, pDdiCmd);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                int rc = vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
                Assert(rc == VINF_SUCCESS);
            }
            else
            {
                vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
            }
#else
            Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            Assert(Status == STATUS_SUCCESS);
#endif
            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
        {
            VBOXWDDM_DMA_PRIVATEDATA_FLIP *pFlip = (VBOXWDDM_DMA_PRIVATEDATA_FLIP*)pPrivateDataBase;
            PVBOXVDMAPIPE_CMD_DMACMD_FLIP pFlipCmd = (PVBOXVDMAPIPE_CMD_DMACMD_FLIP)vboxVdmaGgCmdCreate(pDevExt,
                    VBOXVDMAPIPE_CMD_TYPE_DMACMD, sizeof (VBOXVDMAPIPE_CMD_DMACMD_FLIP));
            Assert(pFlipCmd);
            if (pFlipCmd)
            {
                VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pFlip->Flip.Alloc.srcId];
                vboxVdmaGgCmdDmaNotifyInit(&pFlipCmd->Hdr, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, NULL, NULL);
                pFlipCmd->Hdr.fFlags.Value = 0;
                pFlipCmd->Hdr.fFlags.b3DRelated = 1;
                pFlipCmd->Hdr.pContext = pContext;
                pFlipCmd->Hdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP;
                memcpy(&pFlipCmd->Flip, &pFlip->Flip, sizeof (pFlipCmd->Flip));
                Status = vboxWddmSubmitCmd(pDevExt, &pFlipCmd->Hdr);
                Assert(Status == STATUS_SUCCESS);
            }
            else
            {
                Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_FAULTED);
                Assert(Status == STATUS_SUCCESS);
            }
            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
        {
            PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateDataBase;
            PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCFCmd = (PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL)vboxVdmaGgCmdCreate(pDevExt,
                    VBOXVDMAPIPE_CMD_TYPE_DMACMD, RT_OFFSETOF(VBOXVDMAPIPE_CMD_DMACMD_CLRFILL, ClrFill.Rects.aRects[pCF->ClrFill.Rects.cRects]));
            Assert(pCFCmd);
            if (pCFCmd)
            {
//                VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pFlip->Flip.Alloc.srcId];
                vboxVdmaGgCmdDmaNotifyInit(&pCFCmd->Hdr, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, NULL, NULL);
                pCFCmd->Hdr.fFlags.Value = 0;
                pCFCmd->Hdr.fFlags.b2DRelated = 1;
                pCFCmd->Hdr.pContext = pContext;
                pCFCmd->Hdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL;
                memcpy(&pCFCmd->ClrFill, &pCF->ClrFill, RT_OFFSETOF(VBOXVDMA_CLRFILL, Rects.aRects[pCF->ClrFill.Rects.cRects]));
                Status = vboxWddmSubmitCmd(pDevExt, &pCFCmd->Hdr);
                Assert(Status == STATUS_SUCCESS);
            }
            else
            {
                Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_FAULTED);
                Assert(Status == STATUS_SUCCESS);
            }

            break;
        }
        case VBOXVDMACMD_TYPE_DMA_NOP:
        {
            Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext->NodeOrdinal, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            Assert(Status == STATUS_SUCCESS);
            break;
        }
        default:
        {
            WARN(("unexpected command %d", enmCmd));
#if 0 //def VBOX_WITH_VDMA
            VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR *pPrivateData = (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR*)pPrivateDataBase;
            PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate (&pDevExt->u.primary.Vdma, 0);
            if (!pDr)
            {
                /* @todo: try flushing.. */
                LOGREL(("vboxVdmaCBufDrCreate returned NULL"));
                return STATUS_INSUFFICIENT_RESOURCES;
            }
            // vboxVdmaCBufDrCreate zero initializes the pDr
            //pDr->fFlags = 0;
            pDr->cbBuf = pSubmitCommand->DmaBufferSubmissionEndOffset - pSubmitCommand->DmaBufferSubmissionStartOffset;
            pDr->u32FenceId = pSubmitCommand->SubmissionFenceId;
            pDr->rc = VERR_NOT_IMPLEMENTED;
            if (pPrivateData)
                pDr->u64GuestContext = (uint64_t)pPrivateData->pContext;
        //    else    // vboxVdmaCBufDrCreate zero initializes the pDr
        //        pDr->u64GuestContext = NULL;
            pDr->Location.phBuf = pSubmitCommand->DmaBufferPhysicalAddress.QuadPart + pSubmitCommand->DmaBufferSubmissionStartOffset;

            vboxVdmaCBufDrSubmit(pDevExt, &pDevExt->u.primary.Vdma, pDr);
#endif
            break;
        }
    }
//    LOGF(("LEAVE, context(0x%x)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiPreemptCommand(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PREEMPTCOMMAND*  pPreemptCommand)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertFailed();
    /* @todo: fixme: implement */

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

#if 0
static uint32_t vboxWddmSysMemElBuild(PVBOXVDMACMD_SYSMEMEL pEl, PMDL pMdl, uint32_t iPfn, uint32_t cPages, uint32_t cbBuffer, uint32_t *pcPagesRemaining)
{
    uint32_t cbInitialBuffer = cbBuffer;
    if (cbBuf >= sizeof (*pEl))
    {
        PFN_NUMBER cur = MmGetMdlPfnArray(pMdl)[iPfn];
        uint32_t cbEl = sizeof (*pEl);
        uint32_t cBufs = 1;
        pEl->phBuf[0] = (cur << 12);
        --cPages;
        cbBuffer -= sizeof (*pEl);
        bool bArrayMode = false;
        while (cPages)
        {
            PFN_NUMBER next = MmGetMdlPfnArray(pMdl)[iPfn+cBufs];
            if (!bArrayMode)
            {
                if (next == cur+1)
                {
                    cur = next;
                    ++cBufs;
                    --cPages;
                }
                else if (cBufs > 1)
                {
                    break;
                }
                else
                {
                    bArrayMode = true;
                }
            }

            /* array mode */
            if (cbBuffer < sizeof (pEl->phBuf[0]))
            {
                break;
            }

            pEl->phBuf[cBufs] = (next << 12);
            cbBuffer -= sizeof (pEl->phBuf[0]);
            ++cBufs;
            --cPages;
        }

        pEl->cPages = cPages;
        if (bArrayMode)
            pEl->fFlags = VBOXVDMACMD_SYSMEMEL_F_PAGELIST;
        else
            pEl->fFlags = 0;
    }
    else
    {
        Assert(0);
    }

    *pcPagesRemaining = cPages;
    return cbInitialBuffer - cbBuffer;
}

static uint32_t vboxWddmBpbTransferVRamSysBuildEls(PVBOXVDMACMD_DMA_BPB_TRANSFER_VRAMSYS pCmd, PMDL pMdl, uint32_t iPfn, uint32_t cPages, uint32_t cbBuffer, uint32_t *pcPagesRemaining)
{
    uint32_t cInitPages = cPages;
    uint32_t cbBufferUsed = vboxWddmSysMemElBuild(&pCmd->FirstEl, pMdl, iPfn, cPages, cbBuffer, &cPages);
    if (cbBufferUsed)
    {
        uint32_t cEls = 1;
        PVBOXVDMACMD_SYSMEMEL pEl = &pCmd->FirstEl;
        while (cPages)
        {
            PVBOXVDMACMD_SYSMEMEL pEl = VBOXVDMACMD_SYSMEMEL_NEXT(pEl);
            cbBufferUsed = vboxWddmSysMemElBuild(pEl, pMdl, iPfn + cInitPages - cPages, cPages, cbBuffer - cbBufferUsed, &cPages);
            if (cbBufferUsed)
            {
                ++cEls;
            }
            else
                break;
        }
    }
    else
    {
        Assert(0);
    }

    pCmd->cTransferPages = (cInitPages - cPages);
    *pcPagesRemaining = cPages;
    return cbBufferUsed;
}
#endif
/*
 * DxgkDdiBuildPagingBuffer
 */
NTSTATUS
APIENTRY
DxgkDdiBuildPagingBuffer(
    CONST HANDLE  hAdapter,
    DXGKARG_BUILDPAGINGBUFFER*  pBuildPagingBuffer)
{
    /* DxgkDdiBuildPagingBuffer should be made pageable. */
    PAGED_CODE();

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;

    LOGF(("ENTER, context(0x%x)", hAdapter));

    /* @todo: */
    switch (pBuildPagingBuffer->Operation)
    {
        case DXGK_OPERATION_TRANSFER:
        {
#ifdef VBOX_WITH_VDMA
#if 0
            if ((!pBuildPagingBuffer->Transfer.Source.SegmentId) != (!pBuildPagingBuffer->Transfer.Destination.SegmentId))
            {
                PVBOXVDMACMD pCmd = (PVBOXVDMACMD)pBuildPagingBuffer->pDmaBuffer;
                pCmd->enmType = VBOXVDMACMD_TYPE_DMA_BPB_TRANSFER_VRAMSYS;
                pCmd->u32CmdSpecific = 0;
                PVBOXVDMACMD_DMA_BPB_TRANSFER_VRAMSYS pBody = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_BPB_TRANSFER_VRAMSYS);
                PMDL pMdl;
                uint32_t cPages = (pBuildPagingBuffer->Transfer.TransferSize + 0xfff) >> 12;
                cPages -= pBuildPagingBuffer->MultipassOffset;
                uint32_t iFirstPage = pBuildPagingBuffer->Transfer.MdlOffset + pBuildPagingBuffer->MultipassOffset;
                uint32_t cPagesRemaining;
                if (pBuildPagingBuffer->Transfer.Source.SegmentId)
                {
                    uint64_t off = pBuildPagingBuffer->Transfer.Source.SegmentAddress.QuadPart;
                    off += pBuildPagingBuffer->Transfer.TransferOffset + (pBuildPagingBuffer->MultipassOffset << 12);
                    pBody->offVramBuf = off;
                    pMdl = pBuildPagingBuffer->Transfer.Source.pMdl;
                    pBody->fFlags = 0;//VBOXVDMACMD_DMA_BPB_TRANSFER_VRAMSYS_SYS2VRAM
                }
                else
                {
                    uint64_t off = pBuildPagingBuffer->Transfer.Destination.SegmentAddress.QuadPart;
                    off += pBuildPagingBuffer->Transfer.TransferOffset + (pBuildPagingBuffer->MultipassOffset << 12);
                    pBody->offVramBuf = off;
                    pMdl = pBuildPagingBuffer->Transfer.Destination.pMdl;
                    pBody->fFlags = VBOXVDMACMD_DMA_BPB_TRANSFER_VRAMSYS_SYS2VRAM;
                }

                uint32_t sbBufferUsed = vboxWddmBpbTransferVRamSysBuildEls(pBody, pMdl, iFirstPage, cPages, pBuildPagingBuffer->DmaSize, &cPagesRemaining);
                Assert(sbBufferUsed);
            }

#else
            PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pBuildPagingBuffer->Transfer.hAllocation;
            Assert(pAlloc);
            if (pAlloc
                    && !pAlloc->fRcFlags.Overlay /* overlay surfaces actually contain a valid data */
                    && pAlloc->enmType != VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE  /* shadow primary - also */
                    && pAlloc->enmType != VBOXWDDM_ALLOC_TYPE_UMD_HGSMI_BUFFER /* hgsmi buffer - also */
                    )
            {
                /* we do not care about the others for now */
                Status = STATUS_SUCCESS;
                break;
            }
            UINT cbCmd = VBOXVDMACMD_SIZE(VBOXVDMACMD_DMA_BPB_TRANSFER);
            PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate (&pDevExt->u.primary.Vdma, cbCmd);
            Assert(pDr);
            if (pDr)
            {
                SIZE_T cbTransfered = 0;
                SIZE_T cbTransferSize = pBuildPagingBuffer->Transfer.TransferSize;
                PVBOXVDMACMD pHdr = VBOXVDMACBUF_DR_TAIL(pDr, VBOXVDMACMD);
                do
                {
                    // vboxVdmaCBufDrCreate zero initializes the pDr
                    pDr->fFlags = VBOXVDMACBUF_FLAG_BUF_FOLLOWS_DR;
                    pDr->cbBuf = cbCmd;
                    pDr->rc = VERR_NOT_IMPLEMENTED;

                    pHdr->enmType = VBOXVDMACMD_TYPE_DMA_BPB_TRANSFER;
                    pHdr->u32CmdSpecific = 0;
                    VBOXVDMACMD_DMA_BPB_TRANSFER *pBody = VBOXVDMACMD_BODY(pHdr, VBOXVDMACMD_DMA_BPB_TRANSFER);
//                    pBody->cbTransferSize = (uint32_t)pBuildPagingBuffer->Transfer.TransferSize;
                    pBody->fFlags = 0;
                    SIZE_T cSrcPages = (cbTransferSize + 0xfff ) >> 12;
                    SIZE_T cDstPages = cSrcPages;

                    if (pBuildPagingBuffer->Transfer.Source.SegmentId)
                    {
                        uint64_t off = pBuildPagingBuffer->Transfer.Source.SegmentAddress.QuadPart;
                        off += pBuildPagingBuffer->Transfer.TransferOffset + cbTransfered;
                        pBody->Src.offVramBuf = off;
                        pBody->fFlags |= VBOXVDMACMD_DMA_BPB_TRANSFER_F_SRC_VRAMOFFSET;
                    }
                    else
                    {
                        UINT index = pBuildPagingBuffer->Transfer.MdlOffset + (UINT)(cbTransfered>>12);
                        pBody->Src.phBuf = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Source.pMdl)[index] << 12;
                        PFN_NUMBER num = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Source.pMdl)[index];
                        cSrcPages = 1;
                        for (UINT i = 1; i < ((cbTransferSize - cbTransfered + 0xfff) >> 12); ++i)
                        {
                            PFN_NUMBER cur = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Source.pMdl)[index+i];
                            if(cur != ++num)
                            {
                                cSrcPages+= i-1;
                                break;
                            }
                        }
                    }

                    if (pBuildPagingBuffer->Transfer.Destination.SegmentId)
                    {
                        uint64_t off = pBuildPagingBuffer->Transfer.Destination.SegmentAddress.QuadPart;
                        off += pBuildPagingBuffer->Transfer.TransferOffset;
                        pBody->Dst.offVramBuf = off + cbTransfered;
                        pBody->fFlags |= VBOXVDMACMD_DMA_BPB_TRANSFER_F_DST_VRAMOFFSET;
                    }
                    else
                    {
                        UINT index = pBuildPagingBuffer->Transfer.MdlOffset + (UINT)(cbTransfered>>12);
                        pBody->Dst.phBuf = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Destination.pMdl)[index] << 12;
                        PFN_NUMBER num = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Destination.pMdl)[index];
                        cDstPages = 1;
                        for (UINT i = 1; i < ((cbTransferSize - cbTransfered + 0xfff) >> 12); ++i)
                        {
                            PFN_NUMBER cur = MmGetMdlPfnArray(pBuildPagingBuffer->Transfer.Destination.pMdl)[index+i];
                            if(cur != ++num)
                            {
                                cDstPages+= i-1;
                                break;
                            }
                        }
                    }

                    SIZE_T cbCurTransfer;
                    cbCurTransfer = RT_MIN(cbTransferSize - cbTransfered, cSrcPages << 12);
                    cbCurTransfer = RT_MIN(cbCurTransfer, cDstPages << 12);

                    pBody->cbTransferSize = (UINT)cbCurTransfer;
                    Assert(!(cbCurTransfer & 0xfff));

                    int rc = vboxVdmaCBufDrSubmitSynch(pDevExt, &pDevExt->u.primary.Vdma, pDr);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        Status = STATUS_SUCCESS;
                        cbTransfered += cbCurTransfer;
                    }
                    else
                        Status = STATUS_UNSUCCESSFUL;
                } while (cbTransfered < cbTransferSize);
                Assert(cbTransfered == cbTransferSize);
                vboxVdmaCBufDrFree(&pDevExt->u.primary.Vdma, pDr);
            }
            else
            {
                /* @todo: try flushing.. */
                LOGREL(("vboxVdmaCBufDrCreate returned NULL"));
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }
#endif
#endif /* #ifdef VBOX_WITH_VDMA */
            break;
        }
        case DXGK_OPERATION_FILL:
        {
            Assert(pBuildPagingBuffer->Fill.FillPattern == 0);
            PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pBuildPagingBuffer->Fill.hAllocation;
//            pBuildPagingBuffer->pDmaBuffer = (uint8_t*)pBuildPagingBuffer->pDmaBuffer + VBOXVDMACMD_SIZE(VBOXVDMACMD_DMA_BPB_FILL);
            break;
        }
        case DXGK_OPERATION_DISCARD_CONTENT:
        {
//            AssertBreakpoint();
            break;
        }
        default:
        {
            LOGREL(("unsupported op (%d)", pBuildPagingBuffer->Operation));
            AssertBreakpoint();
            break;
        }
    }

    LOGF(("LEAVE, context(0x%x)", hAdapter));

    return Status;

}

NTSTATUS
APIENTRY
DxgkDdiSetPalette(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPALETTE*  pSetPalette
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

BOOL vboxWddmPointerCopyColorData(CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape, PVIDEO_POINTER_ATTRIBUTES pPointerAttributes)
{
    ULONG srcMaskW, srcMaskH;
    ULONG dstBytesPerLine;
    ULONG x, y;
    BYTE *pSrc, *pDst, bit;

    srcMaskW = pSetPointerShape->Width;
    srcMaskH = pSetPointerShape->Height;

    /* truncate masks if we exceed supported size */
    pPointerAttributes->Width = min(srcMaskW, VBOXWDDM_C_POINTER_MAX_WIDTH);
    pPointerAttributes->Height = min(srcMaskH, VBOXWDDM_C_POINTER_MAX_HEIGHT);
    pPointerAttributes->WidthInBytes = pPointerAttributes->Width * 4;

    /* cnstruct and mask from alpha color channel */
    pSrc = (PBYTE)pSetPointerShape->pPixels;
    pDst = pPointerAttributes->Pixels;
    dstBytesPerLine = (pPointerAttributes->Width+7)/8;

    /* sanity check */
    uint32_t cbData = RT_ALIGN_T(dstBytesPerLine*pPointerAttributes->Height, 4, ULONG)+
                      pPointerAttributes->Height*pPointerAttributes->WidthInBytes;
    uint32_t cbPointerAttributes = RT_OFFSETOF(VIDEO_POINTER_ATTRIBUTES, Pixels[cbData]);
    Assert(VBOXWDDM_POINTER_ATTRIBUTES_SIZE >= cbPointerAttributes);
    if (VBOXWDDM_POINTER_ATTRIBUTES_SIZE < cbPointerAttributes)
    {
        LOGREL(("VBOXWDDM_POINTER_ATTRIBUTES_SIZE(%d) < cbPointerAttributes(%d)", VBOXWDDM_POINTER_ATTRIBUTES_SIZE, cbPointerAttributes));
        return FALSE;
    }

    memset(pDst, 0xFF, dstBytesPerLine*pPointerAttributes->Height);
    for (y=0; y<pPointerAttributes->Height; ++y)
    {
        for (x=0, bit=7; x<pPointerAttributes->Width; ++x, --bit)
        {
            if (0xFF==bit) bit=7;

            if (pSrc[y*pSetPointerShape->Pitch + x*4 + 3] > 0x7F)
            {
                pDst[y*dstBytesPerLine + x/8] &= ~RT_BIT(bit);
            }
        }
    }

    /* copy 32bpp to XOR DIB, it start in pPointerAttributes->Pixels should be 4bytes aligned */
    pSrc = (BYTE*)pSetPointerShape->pPixels;
    pDst = pPointerAttributes->Pixels + RT_ALIGN_T(dstBytesPerLine*pPointerAttributes->Height, 4, ULONG);
    dstBytesPerLine = pPointerAttributes->Width * 4;

    for (y=0; y<pPointerAttributes->Height; ++y)
    {
        memcpy(pDst+y*dstBytesPerLine, pSrc+y*pSetPointerShape->Pitch, dstBytesPerLine);
    }

    return TRUE;
}

BOOL vboxWddmPointerCopyMonoData(CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape, PVIDEO_POINTER_ATTRIBUTES pPointerAttributes)
{
    ULONG srcMaskW, srcMaskH;
    ULONG dstBytesPerLine;
    ULONG x, y;
    BYTE *pSrc, *pDst, bit;

    srcMaskW = pSetPointerShape->Width;
    srcMaskH = pSetPointerShape->Height;

    /* truncate masks if we exceed supported size */
    pPointerAttributes->Width = min(srcMaskW, VBOXWDDM_C_POINTER_MAX_WIDTH);
    pPointerAttributes->Height = min(srcMaskH, VBOXWDDM_C_POINTER_MAX_HEIGHT);
    pPointerAttributes->WidthInBytes = pPointerAttributes->Width * 4;

    /* copy AND mask */
    pSrc = (PBYTE)pSetPointerShape->pPixels;
    pDst = pPointerAttributes->Pixels;
    dstBytesPerLine = (pPointerAttributes->Width+7)/8;

    for (y=0; y<pPointerAttributes->Height; ++y)
    {
        memcpy(pDst+y*dstBytesPerLine, pSrc+y*pSetPointerShape->Pitch, dstBytesPerLine);
    }

    /* convert XOR mask to RGB0 DIB, it start in pPointerAttributes->Pixels should be 4bytes aligned */
    pSrc = (BYTE*)pSetPointerShape->pPixels + srcMaskH*pSetPointerShape->Pitch;
    pDst = pPointerAttributes->Pixels + RT_ALIGN_T(dstBytesPerLine*pPointerAttributes->Height, 4, ULONG);
    dstBytesPerLine = pPointerAttributes->Width * 4;

    for (y=0; y<pPointerAttributes->Height; ++y)
    {
        for (x=0, bit=7; x<pPointerAttributes->Width; ++x, --bit)
        {
            if (0xFF==bit) bit=7;

            *(ULONG*)&pDst[y*dstBytesPerLine+x*4] = (pSrc[y*pSetPointerShape->Pitch+x/8] & RT_BIT(bit)) ? 0x00FFFFFF : 0;
        }
    }

    return TRUE;
}

static BOOLEAN vboxVddmPointerShapeToAttributes(CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape, PVBOXWDDM_POINTER_INFO pPointerInfo)
{
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = &pPointerInfo->Attributes.data;
    /* pPointerAttributes maintains the visibility state, clear all except visibility */
    pPointerAttributes->Enable &= VBOX_MOUSE_POINTER_VISIBLE;

    Assert(pSetPointerShape->Flags.Value == 1 || pSetPointerShape->Flags.Value == 2);
    if (pSetPointerShape->Flags.Color)
    {
        if (vboxWddmPointerCopyColorData(pSetPointerShape, pPointerAttributes))
        {
            pPointerAttributes->Flags = VIDEO_MODE_COLOR_POINTER;
            pPointerAttributes->Enable |= VBOX_MOUSE_POINTER_ALPHA;
        }
        else
        {
            LOGREL(("vboxWddmPointerCopyColorData failed"));
            AssertBreakpoint();
            return FALSE;
        }

    }
    else if (pSetPointerShape->Flags.Monochrome)
    {
        if (vboxWddmPointerCopyMonoData(pSetPointerShape, pPointerAttributes))
        {
            pPointerAttributes->Flags = VIDEO_MODE_MONO_POINTER;
        }
        else
        {
            LOGREL(("vboxWddmPointerCopyMonoData failed"));
            AssertBreakpoint();
            return FALSE;
        }
    }
    else
    {
        LOGREL(("unsupported pointer type Flags.Value(0x%x)", pSetPointerShape->Flags.Value));
        AssertBreakpoint();
        return FALSE;
    }

    pPointerAttributes->Enable |= VBOX_MOUSE_POINTER_SHAPE;

    /*
     * The hot spot coordinates and alpha flag will be encoded in the pPointerAttributes::Enable field.
     * High word will contain hot spot info and low word - flags.
     */
    pPointerAttributes->Enable |= (pSetPointerShape->YHot & 0xFF) << 24;
    pPointerAttributes->Enable |= (pSetPointerShape->XHot & 0xFF) << 16;

    return TRUE;
}

NTSTATUS
APIENTRY
DxgkDdiSetPointerPosition(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERPOSITION*  pSetPointerPosition)
{
//    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    /* mouse integration is ON */
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    PVBOXWDDM_POINTER_INFO pPointerInfo = &pDevExt->aSources[pSetPointerPosition->VidPnSourceId].PointerInfo;
    PVBOXWDDM_GLOBAL_POINTER_INFO pGlobalPointerInfo = &pDevExt->PointerInfo;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = &pPointerInfo->Attributes.data;
    BOOLEAN fScreenVisState = !!(pPointerAttributes->Enable & VBOX_MOUSE_POINTER_VISIBLE);
    BOOLEAN fVisStateChanged = FALSE;
    BOOLEAN fScreenChanged = pGlobalPointerInfo->iLastReportedScreen != pSetPointerPosition->VidPnSourceId;

    if (pSetPointerPosition->Flags.Visible)
    {
        pPointerAttributes->Enable |= VBOX_MOUSE_POINTER_VISIBLE;
        if (!fScreenVisState)
        {
            fVisStateChanged = !!pGlobalPointerInfo->cVisible;
            ++pGlobalPointerInfo->cVisible;
        }
    }
    else
    {
        pPointerAttributes->Enable &= ~VBOX_MOUSE_POINTER_VISIBLE;
        if (fScreenVisState)
        {
            --pGlobalPointerInfo->cVisible;
            fVisStateChanged = !!pGlobalPointerInfo->cVisible;
        }
    }

    pGlobalPointerInfo->iLastReportedScreen = pSetPointerPosition->VidPnSourceId;

    if ((fVisStateChanged || fScreenChanged) && VBoxQueryHostWantsAbsolute())
    {
        if (fScreenChanged)
        {
            BOOLEAN bResult = VBoxMPCmnUpdatePointerShape(VBoxCommonFromDeviceExt(pDevExt), &pPointerInfo->Attributes.data, VBOXWDDM_POINTER_ATTRIBUTES_SIZE);
            Assert(bResult);
        }
        else
        {
            // tell the host to use the guest's pointer
            VIDEO_POINTER_ATTRIBUTES PointerAttributes;

            /* Visible and No Shape means Show the pointer.
             * It is enough to init only this field.
             */
            PointerAttributes.Enable = pSetPointerPosition->Flags.Visible ? VBOX_MOUSE_POINTER_VISIBLE : 0;

            BOOLEAN bResult = VBoxMPCmnUpdatePointerShape(VBoxCommonFromDeviceExt(pDevExt), &PointerAttributes, sizeof (PointerAttributes));
            Assert(bResult);
        }
    }

//    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiSetPointerShape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERSHAPE*  pSetPointerShape)
{
//    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_NOT_SUPPORTED;

    if (VBoxQueryHostWantsAbsolute())
    {
        /* mouse integration is ON */
        PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
        PVBOXWDDM_POINTER_INFO pPointerInfo = &pDevExt->aSources[pSetPointerShape->VidPnSourceId].PointerInfo;
        /* @todo: to avoid extra data copy and extra heap allocation,
         *  need to maintain the pre-allocated HGSMI buffer and convert the data directly to it */
        if (vboxVddmPointerShapeToAttributes(pSetPointerShape, pPointerInfo))
        {
            pDevExt->PointerInfo.iLastReportedScreen = pSetPointerShape->VidPnSourceId;
            if (VBoxMPCmnUpdatePointerShape(VBoxCommonFromDeviceExt(pDevExt), &pPointerInfo->Attributes.data, VBOXWDDM_POINTER_ATTRIBUTES_SIZE))
                Status = STATUS_SUCCESS;
            else
            {
                AssertBreakpoint();
                LOGREL(("vboxUpdatePointerShape failed"));
            }
        }
    }

//    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY CALLBACK
DxgkDdiResetFromTimeout(
    CONST HANDLE  hAdapter)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}


/* the lpRgnData->Buffer comes to us as RECT
 * to avoid extra memcpy we cast it to PRTRECT assuming
 * they are identical */
AssertCompile(sizeof(RECT) == sizeof(RTRECT));
AssertCompile(RT_OFFSETOF(RECT, left) == RT_OFFSETOF(RTRECT, xLeft));
AssertCompile(RT_OFFSETOF(RECT, bottom) == RT_OFFSETOF(RTRECT, yBottom));
AssertCompile(RT_OFFSETOF(RECT, right) == RT_OFFSETOF(RTRECT, xRight));
AssertCompile(RT_OFFSETOF(RECT, top) == RT_OFFSETOF(RTRECT, yTop));

NTSTATUS
APIENTRY
DxgkDdiEscape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_ESCAPE*  pEscape)
{
    PAGED_CODE();

//    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    Assert(pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE));
    if (pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE))
    {
        PVBOXDISPIFESCAPE pEscapeHdr = (PVBOXDISPIFESCAPE)pEscape->pPrivateDriverData;
        switch (pEscapeHdr->escapeCode)
        {
#ifdef VBOX_WITH_CRHGSMI
            case VBOXESC_UHGSMI_SUBMIT:
            {
                /* submit UHGSMI command */
                PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
                PVBOXDISPIFESCAPE_UHGSMI_SUBMIT pSubmit = (PVBOXDISPIFESCAPE_UHGSMI_SUBMIT)pEscapeHdr;
                Assert(pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_UHGSMI_SUBMIT)
                        && pEscape->PrivateDriverDataSize == RT_OFFSETOF(VBOXDISPIFESCAPE_UHGSMI_SUBMIT, aBuffers[pEscapeHdr->u32CmdSpecific]));
                if (pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD)
                        && pEscape->PrivateDriverDataSize == RT_OFFSETOF(VBOXDISPIFESCAPE_UHGSMI_SUBMIT, aBuffers[pEscapeHdr->u32CmdSpecific]))
                {
                    Status = vboxVideoAMgrCtxAllocSubmit(pDevExt, &pContext->AllocContext, pEscapeHdr->u32CmdSpecific, pSubmit->aBuffers);
                    Assert(Status == STATUS_SUCCESS);
                }
                else
                    Status = STATUS_BUFFER_TOO_SMALL;

                break;
            }
#endif
            case VBOXESC_UHGSMI_ALLOCATE:
            {
                /* allocate UHGSMI buffer */
                PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
                PVBOXDISPIFESCAPE_UHGSMI_ALLOCATE pAlocate = (PVBOXDISPIFESCAPE_UHGSMI_ALLOCATE)pEscapeHdr;
                Assert(pEscape->PrivateDriverDataSize == sizeof (VBOXDISPIFESCAPE_UHGSMI_ALLOCATE));
                if (pEscape->PrivateDriverDataSize == sizeof (VBOXDISPIFESCAPE_UHGSMI_ALLOCATE))
                {
                    Status = vboxVideoAMgrCtxAllocCreate(&pContext->AllocContext, &pAlocate->Alloc);
                    Assert(Status == STATUS_SUCCESS);
                }
                else
                    Status = STATUS_BUFFER_TOO_SMALL;

                break;
            }
            case VBOXESC_UHGSMI_DEALLOCATE:
            {
                /* deallocate UHGSMI buffer */
                PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
                PVBOXDISPIFESCAPE_UHGSMI_DEALLOCATE pDealocate = (PVBOXDISPIFESCAPE_UHGSMI_DEALLOCATE)pEscapeHdr;
                Assert(pEscape->PrivateDriverDataSize == sizeof (VBOXDISPIFESCAPE_UHGSMI_DEALLOCATE));
                if (pEscape->PrivateDriverDataSize == sizeof (VBOXDISPIFESCAPE_UHGSMI_DEALLOCATE))
                {
                    Status = vboxVideoAMgrCtxAllocDestroy(&pContext->AllocContext, pDealocate->hAlloc);
                    Assert(Status == STATUS_SUCCESS);
                }
                else
                    Status = STATUS_BUFFER_TOO_SMALL;

                break;
            }
            case VBOXESC_GETVBOXVIDEOCMCMD:
            {
                /* get the list of r0->r3 commands (d3d window visible regions reporting )*/
                PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
                PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD pRegions = (PVBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD)pEscapeHdr;
                Assert(pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD));
                if (pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_GETVBOXVIDEOCMCMD))
                {
                    Status = vboxVideoCmEscape(&pContext->CmContext, pRegions, pEscape->PrivateDriverDataSize);
                    Assert(Status == STATUS_SUCCESS);
                }
                else
                    Status = STATUS_BUFFER_TOO_SMALL;

                break;
            }
            case VBOXESC_SETVISIBLEREGION:
            {
                /* visible regions for seamless */
                LPRGNDATA lpRgnData = VBOXDISPIFESCAPE_DATA(pEscapeHdr, RGNDATA);
                uint32_t cbData = VBOXDISPIFESCAPE_DATA_SIZE(pEscape->PrivateDriverDataSize);
                uint32_t cbRects = cbData - RT_OFFSETOF(RGNDATA, Buffer);
                /* the lpRgnData->Buffer comes to us as RECT
                 * to avoid extra memcpy we cast it to PRTRECT assuming
                 * they are identical
                 * see AssertCompile's above */

                RTRECT   *pRect = (RTRECT *)&lpRgnData->Buffer;

                uint32_t cRects = cbRects/sizeof(RTRECT);
                int      rc;

                LOG(("IOCTL_VIDEO_VBOX_SETVISIBLEREGION cRects=%d", cRects));
                Assert(cbRects >= sizeof(RTRECT)
                    &&  cbRects == cRects*sizeof(RTRECT)
                    &&  cRects == lpRgnData->rdh.nCount);
                if (    cbRects >= sizeof(RTRECT)
                    &&  cbRects == cRects*sizeof(RTRECT)
                    &&  cRects == lpRgnData->rdh.nCount)
                {
                    /*
                     * Inform the host about the visible region
                     */
                    VMMDevVideoSetVisibleRegion *req = NULL;

                    rc = VbglGRAlloc ((VMMDevRequestHeader **)&req,
                                      sizeof (VMMDevVideoSetVisibleRegion) + (cRects-1)*sizeof(RTRECT),
                                      VMMDevReq_VideoSetVisibleRegion);
                    AssertRC(rc);
                    if (RT_SUCCESS(rc))
                    {
                        req->cRect = cRects;
                        memcpy(&req->Rect, pRect, cRects*sizeof(RTRECT));

                        rc = VbglGRPerform (&req->header);
                        AssertRC(rc);
                        if (!RT_SUCCESS(rc))
                        {
                            LOGREL(("VbglGRPerform failed rc (%d)", rc));
                            Status = STATUS_UNSUCCESSFUL;
                        }
                    }
                    else
                    {
                        LOGREL(("VbglGRAlloc failed rc (%d)", rc));
                        Status = STATUS_UNSUCCESSFUL;
                    }
                }
                else
                {
                    LOGREL(("VBOXESC_SETVISIBLEREGION: incorrect buffer size (%d), reported count (%d)", cbRects, lpRgnData->rdh.nCount));
                    AssertBreakpoint();
                    Status = STATUS_INVALID_PARAMETER;
                }
                break;
            }
            case VBOXESC_ISVRDPACTIVE:
                /* @todo: implement */
                Status = STATUS_SUCCESS;
                break;
            case VBOXESC_SCREENLAYOUT:
            {
                /* set screen layout (unused currently) */
                Assert(pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT));
                if (pEscape->PrivateDriverDataSize < sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT))
                {
                    WARN(("VBOXESC_SCREENLAYOUT: incorrect buffer size (%d) < sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT) (%d)",
                            pEscape->PrivateDriverDataSize, sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT)));
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                PVBOXDISPIFESCAPE_SCREENLAYOUT pLo = (PVBOXDISPIFESCAPE_SCREENLAYOUT)pEscapeHdr;
                if (pLo->ScreenLayout.cScreens > (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays)
                {
                    WARN(("VBOXESC_SCREENLAYOUT: number of screens too big (%d), should be <= (%d)",
                            pLo->ScreenLayout.cScreens, VBoxCommonFromDeviceExt(pDevExt)->cDisplays));
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                for (UINT i = 0; i < pLo->ScreenLayout.cScreens; ++i)
                {
                    PVBOXSCREENLAYOUT_ELEMENT pEl = &pLo->ScreenLayout.aScreens[i];
                    Assert(pEl->VidPnSourceId < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays);
                    if (pEl->VidPnSourceId < (UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays)
                    {
                        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pEl->VidPnSourceId];
                        NTSTATUS tmpStatus = vboxWddmGhDisplayUpdateScreenPos(pDevExt, pSource, &pEl->pos);
                        Assert(tmpStatus == STATUS_SUCCESS);
                    }
                }

                Status = STATUS_SUCCESS;
                break;
            }
            case VBOXESC_SWAPCHAININFO:
            {
                /* set swapchain information */
                PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
                Status = vboxWddmSwapchainCtxEscape(pDevExt, pContext, (PVBOXDISPIFESCAPE_SWAPCHAININFO)pEscapeHdr, pEscape->PrivateDriverDataSize);
                Assert(Status == STATUS_SUCCESS);
                break;
            }
            case VBOXESC_REINITVIDEOMODES:
            {
                /* clear driver's internal videomodes cache */
                VBoxWddmInvalidateVideoModesInfo(pDevExt);
                Status = STATUS_SUCCESS;
                break;
            }
            case VBOXESC_SHRC_ADDREF:
            case VBOXESC_SHRC_RELEASE:
            {
                PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)pEscape->hDevice;
                /* query whether the allocation represanted by the given [wine-generated] shared resource handle still exists */
                if (pEscape->PrivateDriverDataSize != sizeof (VBOXDISPIFESCAPE_SHRC_REF))
                {
                    WARN(("invalid buffer size for VBOXDISPIFESCAPE_SHRC_REF, was(%d), but expected (%d)",
                            pEscape->PrivateDriverDataSize, sizeof (VBOXDISPIFESCAPE_SHRC_REF)));
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                PVBOXDISPIFESCAPE_SHRC_REF pShRcRef = (PVBOXDISPIFESCAPE_SHRC_REF)pEscapeHdr;
                PVBOXWDDM_ALLOCATION pAlloc = vboxWddmGetAllocationFromHandle(pDevExt, (D3DKMT_HANDLE)pShRcRef->hAlloc);
                if (!pAlloc)
                {
                    WARN(("failed to get allocation from handle"));
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                PVBOXWDDM_OPENALLOCATION pOa = NULL;
                for (PLIST_ENTRY pCur = pAlloc->OpenList.Flink; pCur != &pAlloc->OpenList; pCur = pCur->Flink)
                {
                    PVBOXWDDM_OPENALLOCATION pCurOa = CONTAINING_RECORD(pCur, VBOXWDDM_OPENALLOCATION, ListEntry);
                    if (pCurOa->pDevice == pDevice)
                    {
                        pOa = pCurOa;
                        break;
                    }
                }

                if (!pOa)
                {
                    WARN(("failed to get open allocation from alloc"));
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }

                Assert(pAlloc->cShRcRefs >= pOa->cShRcRefs);

                if (pEscapeHdr->escapeCode == VBOXESC_SHRC_ADDREF)
                {
#ifdef DEBUG
                    Assert(!pAlloc->fAssumedDeletion);
#endif
                    ++pAlloc->cShRcRefs;
                    ++pOa->cShRcRefs;
                }
                else
                {
                    Assert(pAlloc->cShRcRefs);
                    Assert(pOa->cShRcRefs);
                    --pAlloc->cShRcRefs;
                    --pOa->cShRcRefs;
#ifdef DEBUG
                    Assert(!pAlloc->fAssumedDeletion);
                    if (!pAlloc->cShRcRefs)
                    {
                        pAlloc->fAssumedDeletion = TRUE;
                    }
#endif
                }

                pShRcRef->EscapeHdr.u32CmdSpecific = pAlloc->cShRcRefs;
                Status = STATUS_SUCCESS;
                break;
            }
            case VBOXESC_DBGPRINT:
            {
                /* use RT_OFFSETOF instead of sizeof since sizeof will give an aligned size that might
                 * be bigger than the VBOXDISPIFESCAPE_DBGPRINT with a data containing just a few chars */
                Assert(pEscape->PrivateDriverDataSize >= RT_OFFSETOF(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[1]));
                /* only do DbgPrint when pEscape->PrivateDriverDataSize > RT_OFFSETOF(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[1])
                 * since == RT_OFFSETOF(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[1]) means the buffer contains just \0,
                 * i.e. no need to print it */
                if (pEscape->PrivateDriverDataSize > RT_OFFSETOF(VBOXDISPIFESCAPE_DBGPRINT, aStringBuf[1]))
                {
                    PVBOXDISPIFESCAPE_DBGPRINT pDbgPrint = (PVBOXDISPIFESCAPE_DBGPRINT)pEscapeHdr;
                    /* ensure the last char is \0*/
                    *((uint8_t*)pDbgPrint + pEscape->PrivateDriverDataSize - 1) = '\0';
#if defined(DEBUG_misha) || defined(DEBUG_leo)
                    DbgPrint("%s", pDbgPrint->aStringBuf);
#else
                    LOGREL_EXACT(("%s", pDbgPrint->aStringBuf));
#endif
                }
                Status = STATUS_SUCCESS;
                break;
            }
            case VBOXESC_DBGDUMPBUF:
            {
                Status = vboxUmdDumpBuf((PVBOXDISPIFESCAPE_DBGDUMPBUF)pEscapeHdr, pEscape->PrivateDriverDataSize);
                break;
            }
            default:
                Assert(0);
                LOGREL(("unsupported escape code (0x%x)", pEscapeHdr->escapeCode));
                break;
        }
    }
    else
    {
        LOGREL(("pEscape->PrivateDriverDataSize(%d) < (%d)", pEscape->PrivateDriverDataSize, sizeof (VBOXDISPIFESCAPE)));
        AssertBreakpoint();
        Status = STATUS_BUFFER_TOO_SMALL;
    }

//    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiCollectDbgInfo(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_COLLECTDBGINFO*  pCollectDbgInfo
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

typedef struct VBOXWDDM_QUERYCURFENCE_CB
{
    PVBOXMP_DEVEXT pDevExt;
    ULONG MessageNumber;
    ULONG uLastCompletedCmdFenceId;
} VBOXWDDM_QUERYCURFENCE_CB, *PVBOXWDDM_QUERYCURFENCE_CB;

static BOOLEAN vboxWddmQueryCurrentFenceCb(PVOID Context)
{
    PVBOXWDDM_QUERYCURFENCE_CB pdc = (PVBOXWDDM_QUERYCURFENCE_CB)Context;
    BOOL bRc = DxgkDdiInterruptRoutine(pdc->pDevExt, pdc->MessageNumber);
    pdc->uLastCompletedCmdFenceId = pdc->pDevExt->u.primary.Vdma.uLastCompletedPagingBufferCmdFenceId;
    return bRc;
}

NTSTATUS
APIENTRY
DxgkDdiQueryCurrentFence(
    CONST HANDLE  hAdapter,
    DXGKARG_QUERYCURRENTFENCE*  pCurrentFence)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vboxVDbgBreakF();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    VBOXWDDM_QUERYCURFENCE_CB context = {0};
    context.pDevExt = pDevExt;
    BOOLEAN bRet;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxWddmQueryCurrentFenceCb,
            &context,
            0, /* IN ULONG MessageNumber */
            &bRet);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        pCurrentFence->CurrentFence = context.uLastCompletedCmdFenceId;
    }

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiIsSupportedVidPn(
    CONST HANDLE  hAdapter,
    OUT DXGKARG_ISSUPPORTEDVIDPN*  pIsSupportedVidPnArg
    )
{
    /* The DxgkDdiIsSupportedVidPn should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    BOOLEAN bSupported = TRUE;

    PVBOXMP_DEVEXT pContext = (PVBOXMP_DEVEXT)hAdapter;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    Status = pContext->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pIsSupportedVidPnArg->hDesiredVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryVidPnInterface failed Status()0x%x\n", Status));
        return Status;
    }

#ifdef VBOXWDDM_DEBUG_VIDPN
    vboxVidPnDumpVidPn("\n>>>>IS SUPPORTED VidPN : >>>>", pContext, pIsSupportedVidPnArg->hDesiredVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<");
#endif

    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    Status = pVidPnInterface->pfnGetTopology(pIsSupportedVidPnArg->hDesiredVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnGetTopology failed Status()0x%x\n", Status));
        return Status;
    }

    VBOXVIDPNPATHITEM aItems[VBOX_VIDEO_MAX_SCREENS];
    BOOLEAN fDisabledFound = FALSE;
    Status = vboxVidPnCheckTopology(hVidPnTopology, pVidPnTopologyInterface, TRUE /* fBreakOnDisabled */, RT_ELEMENTS(aItems), aItems, &fDisabledFound);
    Assert(Status == STATUS_SUCCESS);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxVidPnCheckTopology failed Status()0x%x\n", Status));
        return Status;
    }

    if (fDisabledFound)
    {
        WARN(("found unsupported path"));
        bSupported = FALSE;
    }

    pIsSupportedVidPnArg->IsVidPnSupported = bSupported;

#ifdef VBOXWDDM_DEBUG_VIDPN
    LOGREL(("The Given VidPn is %ssupported\n", pIsSupportedVidPnArg->IsVidPnSupported ? "" : "!!NOT!! "));
#endif

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendFunctionalVidPn(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDFUNCTIONALVIDPN* CONST  pRecommendFunctionalVidPnArg
    )
{
    /* The DxgkDdiRecommendFunctionalVidPn should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    NTSTATUS Status;
    PVBOXWDDM_RECOMMENDVIDPN pVidPnInfo = pRecommendFunctionalVidPnArg->PrivateDriverDataSize >= sizeof (VBOXWDDM_RECOMMENDVIDPN) ?
            (PVBOXWDDM_RECOMMENDVIDPN)pRecommendFunctionalVidPnArg->pPrivateDriverData : NULL;
    PVBOXWDDM_VIDEOMODES_INFO pInfos = VBoxWddmUpdateVideoModesInfo(pDevExt, NULL /*pVidPnInfo*/);
    int i;

    for (i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        /* @todo: check that we actually need the current source->target */
        PVBOXWDDM_VIDEOMODES_INFO pInfo = &pInfos[i];
        VIDEO_MODE_INFORMATION *pModeInfo = &pInfo->aModes[pInfo->iPreferredMode];
#if 0
        D3DKMDT_2DREGION Resolution;
        Resolution.cx = pModeInfo->VisScreenWidth;
        Resolution.cy = pModeInfo->VisScreenHeight;
        Status = vboxVidPnCheckAddMonitorModes(pDevExt, i, D3DKMDT_MCO_DRIVER, &Resolution, 1, 0);
#else
        Status = vboxVidPnCheckAddMonitorModes(pDevExt, i, D3DKMDT_MCO_DRIVER, pInfo->aResolutions, pInfo->cResolutions, pInfo->iPreferredResolution);
#endif
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL(("vboxVidPnCheckAddMonitorModes failed Status(0x%x)", Status));
            break;
        }
    }

    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryVidPnInterface failed Status(0x%x)", Status));
        return Status;
    }

    for (i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        Status = vboxVidPnPathAdd(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, pVidPnInterface, i, i);
        if (!NT_SUCCESS(Status))
        {
            WARN(("vboxVidPnPathAdd failed Status(0x%x)", Status));
            return Status;
        }
    }

    VIDEO_MODE_INFORMATION *pResModes = NULL;
    uint32_t cResModes = 0;

    for (i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        D3DKMDT_2DREGION Resolution;
        PVBOXWDDM_VIDEOMODES_INFO pInfo = &pInfos[i];
        VIDEO_MODE_INFORMATION *pModeInfo = &pInfo->aModes[pInfo->iPreferredMode];
        Resolution.cx = pModeInfo->VisScreenWidth;
        Resolution.cy = pModeInfo->VisScreenHeight;
        int32_t iPreferableResMode;
        uint32_t cActualResModes;

        Status = VBoxWddmGetModesForResolution(pInfo->aModes, pInfo->cModes, pInfo->iPreferredMode, &Resolution,
                pResModes, cResModes, &cActualResModes, &iPreferableResMode);
        Assert(Status == STATUS_SUCCESS || Status == STATUS_BUFFER_TOO_SMALL);
        if (Status == STATUS_BUFFER_TOO_SMALL)
        {
            Assert(cResModes < cActualResModes);
            if (pResModes)
            {
                vboxWddmMemFree(pResModes);
            }
            pResModes = (VIDEO_MODE_INFORMATION*)vboxWddmMemAllocZero(sizeof (*pResModes) * cActualResModes);
            Assert(pResModes);
            if (!pResModes)
            {
                Status = STATUS_NO_MEMORY;
                break;
            }
            cResModes = cActualResModes;
            Status = VBoxWddmGetModesForResolution(pInfo->aModes, pInfo->cModes, pInfo->iPreferredMode, &Resolution,
                                pResModes, cResModes, &cActualResModes, &iPreferableResMode);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                break;
        }
        else if (Status != STATUS_SUCCESS)
            break;

        Assert(iPreferableResMode >= 0);
        Assert(cActualResModes);

        Status = vboxVidPnCreatePopulateVidPnPathFromLegacy(pDevExt, pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, pVidPnInterface,
                        pResModes, cActualResModes, iPreferableResMode,
                        &Resolution, 1 /* cResolutions */,
                        i, i); /* srcId, tgtId */
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            LOGREL(("vboxVidPnCreatePopulateVidPnFromLegacy failed Status(0x%x)", Status));
            break;
        }
    }

    if(pResModes)
        vboxWddmMemFree(pResModes);

#ifdef VBOXWDDM_DEBUG_VIDPN
        vboxVidPnDumpVidPn("\n>>>>Recommended VidPN: >>>>", pDevExt, pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<\n");
#endif

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiEnumVidPnCofuncModality(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_ENUMVIDPNCOFUNCMODALITY* CONST  pEnumCofuncModalityArg
    )
{
    /* The DxgkDdiEnumVidPnCofuncModality function should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pEnumCofuncModalityArg->hConstrainingVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (!NT_SUCCESS(Status))
    {
        WARN(("DxgkCbQueryVidPnInterface failed Status()0x%x\n", Status));
        return Status;
    }
#ifdef VBOXWDDM_DEBUG_VIDPN
    vboxVidPnDumpCofuncModalityArg(">>>>MODALITY Args: ", pEnumCofuncModalityArg, "\n");
    vboxVidPnDumpVidPn(">>>>MODALITY VidPN (IN) : >>>>\n", pDevExt, pEnumCofuncModalityArg->hConstrainingVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<\n");
#endif

    D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
    const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
    Status = pVidPnInterface->pfnGetTopology(pEnumCofuncModalityArg->hConstrainingVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
    Assert(Status == STATUS_SUCCESS);
    if (!NT_SUCCESS(Status))
    {
        WARN(("pfnGetTopology failed Status()0x%x\n", Status));
        return Status;
    }

    VBOXVIDPNPATHITEM aItems[VBOX_VIDEO_MAX_SCREENS];
    Status = vboxVidPnCheckTopology(hVidPnTopology, pVidPnTopologyInterface, FALSE /* fBreakOnDisabled */, RT_ELEMENTS(aItems), aItems, NULL /* *pfDisabledFound */);
    Assert(Status == STATUS_SUCCESS);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxVidPnCheckTopology failed Status()0x%x\n", Status));
        return Status;
    }

    VBOXVIDPNCOFUNCMODALITY CbContext = {0};
    CbContext.pDevExt = pDevExt;
    CbContext.pVidPnInterface = pVidPnInterface;
    CbContext.pEnumCofuncModalityArg = pEnumCofuncModalityArg;
    CbContext.pInfos = VBoxWddmGetAllVideoModesInfos(pDevExt);
    CbContext.cPathInfos = RT_ELEMENTS(aItems);
    CbContext.apPathInfos = aItems;

    Status = vboxVidPnEnumPaths(hVidPnTopology, pVidPnTopologyInterface,
                    vboxVidPnCofuncModalityPathEnum, &CbContext);
    Assert(Status == STATUS_SUCCESS);
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxVidPnEnumPaths failed Status()0x%x\n", Status));
        return Status;
    }

    Status = CbContext.Status;
    if (!NT_SUCCESS(Status))
    {
        WARN(("vboxVidPnCofuncModalityPathEnum failed Status()0x%x\n", Status));
        return Status;
    }

#ifdef VBOXWDDM_DEBUG_VIDPN
        vboxVidPnDumpVidPn("\n>>>>MODALITY VidPN (OUT) : >>>>\n", pDevExt, pEnumCofuncModalityArg->hConstrainingVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<\n\n");
#endif

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiSetVidPnSourceAddress(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETVIDPNSOURCEADDRESS*  pSetVidPnSourceAddress
    )
{
    /* The DxgkDdiSetVidPnSourceAddress function should be made pageable. */
    PAGED_CODE();

    vboxVDbgBreakFv();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    Assert((UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceAddress->VidPnSourceId);

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pSetVidPnSourceAddress->VidPnSourceId];
    Status= vboxWddmDisplaySettingsQueryPos(pDevExt, pSetVidPnSourceAddress->VidPnSourceId, &pSource->VScreenPos);
    Assert(Status == STATUS_SUCCESS);
    Status = STATUS_SUCCESS;

    if ((UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceAddress->VidPnSourceId)
    {
        PVBOXWDDM_ALLOCATION pAllocation;
        Assert(pSetVidPnSourceAddress->hAllocation);
        Assert(pSetVidPnSourceAddress->hAllocation || pSource->pPrimaryAllocation);
        Assert (pSetVidPnSourceAddress->Flags.Value < 2); /* i.e. 0 or 1 (ModeChange) */
        if (pSetVidPnSourceAddress->hAllocation)
        {
            pAllocation = (PVBOXWDDM_ALLOCATION)pSetVidPnSourceAddress->hAllocation;
            vboxWddmAssignPrimary(pDevExt, pSource, pAllocation, pSetVidPnSourceAddress->VidPnSourceId);
        }
        else
            pAllocation = pSource->pPrimaryAllocation;

        Assert(pAllocation);
        if (pAllocation)
        {
//            Assert(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE);
            pAllocation->offVram = (VBOXVIDEOOFFSET)pSetVidPnSourceAddress->PrimaryAddress.QuadPart;
            pAllocation->SegmentId = pSetVidPnSourceAddress->PrimarySegment;
            Assert (pAllocation->SegmentId);
            Assert (!pAllocation->bVisible);
            Assert(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
                    || pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC);

            if (
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
                    /* this is the case of full-screen d3d, ensure we notify host */
                    pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC &&
#endif
                    pAllocation->bVisible)
            {
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
                /* to ensure the resize request gets issued in case we exit a full-screen D3D mode */
                pSource->offVram = VBOXVIDEOOFFSET_VOID;
#else
                /* should not generally happen, but still inform host*/
                Status = vboxWddmGhDisplaySetInfo(pDevExt, pSource, pSetVidPnSourceAddress->VidPnSourceId);
                Assert(Status == STATUS_SUCCESS);
                if (Status != STATUS_SUCCESS)
                    LOGREL(("vboxWddmGhDisplaySetInfo failed, Status (0x%x)", Status));
#endif
            }
        }
        else
        {
            LOGREL(("no allocation data available!!"));
            Status = STATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        LOGREL(("invalid VidPnSourceId (%d), should be smaller than (%d)", pSetVidPnSourceAddress->VidPnSourceId, VBoxCommonFromDeviceExt(pDevExt)->cDisplays));
        Status = STATUS_INVALID_PARAMETER;
    }

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiSetVidPnSourceVisibility(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETVIDPNSOURCEVISIBILITY* pSetVidPnSourceVisibility
    )
{
    /* DxgkDdiSetVidPnSourceVisibility should be made pageable. */
    PAGED_CODE();

    vboxVDbgBreakFv();

    LOGF(("ENTER, context(0x%x)", hAdapter));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    Assert((UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceVisibility->VidPnSourceId);

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pSetVidPnSourceVisibility->VidPnSourceId];
    Status= vboxWddmDisplaySettingsQueryPos(pDevExt, pSetVidPnSourceVisibility->VidPnSourceId, &pSource->VScreenPos);
    Assert(Status == STATUS_SUCCESS);
    Status = STATUS_SUCCESS;

    if ((UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceVisibility->VidPnSourceId)
    {
        PVBOXWDDM_ALLOCATION pAllocation = pSource->pPrimaryAllocation;
        if (pAllocation)
        {
            Assert(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
                    || pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC);

            Assert(pAllocation->bVisible != pSetVidPnSourceVisibility->Visible);
            if (pAllocation->bVisible != pSetVidPnSourceVisibility->Visible)
            {
                pAllocation->bVisible = pSetVidPnSourceVisibility->Visible;
                if (pAllocation->bVisible)
                {
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
                    if (/* this is the case of full-screen d3d, ensure we notify host */
                        pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC
                    )
#endif
                    {
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
                        /* to ensure the resize request gets issued in case we exit a full-screen D3D mode */
                        pSource->offVram = VBOXVIDEOOFFSET_VOID;
#else
                        Status = vboxWddmGhDisplaySetInfo(pDevExt, pSource, pSetVidPnSourceVisibility->VidPnSourceId);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status != STATUS_SUCCESS)
                            LOGREL(("vboxWddmGhDisplaySetInfo failed, Status (0x%x)", Status));
#endif
                    }
                }
#ifdef VBOX_WITH_VDMA
                else
                {
                    vboxVdmaFlush (pDevExt, &pDevExt->u.primary.Vdma);
                }
#endif
            }
        }
        else
        {
            Assert(!pSetVidPnSourceVisibility->Visible);
        }
    }
    else
    {
        LOGREL(("invalid VidPnSourceId (%d), should be smaller than (%d)", pSetVidPnSourceVisibility->VidPnSourceId, VBoxCommonFromDeviceExt(pDevExt)->cDisplays));
        Status = STATUS_INVALID_PARAMETER;
    }

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiCommitVidPn(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_COMMITVIDPN* CONST  pCommitVidPnArg
    )
{
    LOGF(("ENTER, context(0x%x)", hAdapter));

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;

    vboxVDbgBreakFv();

    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pCommitVidPnArg->hFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
#ifdef VBOXWDDM_DEBUG_VIDPN
        vboxVidPnDumpVidPn("\n>>>>COMMIT VidPN: >>>>", pDevExt, pCommitVidPnArg->hFunctionalVidPn, pVidPnInterface, "<<<<<<<<<<<<<<<<<<<<\n");
#endif
        if (pCommitVidPnArg->AffectedVidPnSourceId != D3DDDI_ID_ALL)
        {
            Status = vboxVidPnCommitSourceModeForSrcId(
                    pDevExt,
                    pCommitVidPnArg->hFunctionalVidPn, pVidPnInterface,
                    pCommitVidPnArg->AffectedVidPnSourceId, (PVBOXWDDM_ALLOCATION)pCommitVidPnArg->hPrimaryAllocation);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                LOGREL(("vboxVidPnCommitSourceModeForSrcId failed Status(0x%x)", Status));
        }
        else
        {
            /* clear all current primaries */
            for (int i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
            {
                vboxWddmAssignPrimary(pDevExt, &pDevExt->aSources[i], NULL, i);
            }

            D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
            const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
            NTSTATUS Status = pVidPnInterface->pfnGetTopology(pCommitVidPnArg->hFunctionalVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                VBOXVIDPNCOMMIT CbContext = {0};
                CbContext.pDevExt = pDevExt;
                CbContext.pVidPnInterface = pVidPnInterface;
                CbContext.pCommitVidPnArg = pCommitVidPnArg;
                Status = vboxVidPnEnumPaths(hVidPnTopology, pVidPnTopologyInterface,
                            vboxVidPnCommitPathEnum, &CbContext);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                        Status = CbContext.Status;
                        Assert(Status == STATUS_SUCCESS);
                        if (Status != STATUS_SUCCESS)
                            LOGREL(("vboxVidPnCommitPathEnum failed Status(0x%x)", Status));
                }
                else
                    LOGREL(("vboxVidPnEnumPaths failed Status(0x%x)", Status));
            }
            else
                LOGREL(("pfnGetTopology failed Status(0x%x)", Status));
        }

        if (Status == STATUS_SUCCESS)
        {
            pDevExt->u.primary.hCommittedVidPn = pCommitVidPnArg->hFunctionalVidPn;
        }
    }
    else
        LOGREL(("DxgkCbQueryVidPnInterface failed Status(0x%x)", Status));

    LOGF(("LEAVE, status(0x%x), context(0x%x)", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateActiveVidPnPresentPath(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH* CONST  pUpdateActiveVidPnPresentPathArg
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendMonitorModes(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDMONITORMODES* CONST  pRecommendMonitorModesArg
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    NTSTATUS Status;
    PVBOXWDDM_VIDEOMODES_INFO pInfo = VBoxWddmGetVideoModesInfo(pDevExt, pRecommendMonitorModesArg->VideoPresentTargetId);
    PVIDEO_MODE_INFORMATION pPreferredMode = &pInfo->aModes[pInfo->iPreferredMode];


    for (uint32_t i = 0; i < pInfo->cResolutions; i++)
    {
        D3DKMDT_MONITOR_SOURCE_MODE * pNewMonitorSourceModeInfo;
        Status = pRecommendMonitorModesArg->pMonitorSourceModeSetInterface->pfnCreateNewModeInfo(
                    pRecommendMonitorModesArg->hMonitorSourceModeSet, &pNewMonitorSourceModeInfo);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxVidPnPopulateMonitorSourceModeInfoFromLegacy(pDevExt,
                    pNewMonitorSourceModeInfo,
                    &pInfo->aResolutions[i],
                    D3DKMDT_MCO_DRIVER,
                    pPreferredMode->VisScreenWidth == pInfo->aResolutions[i].cx
                    && pPreferredMode->VisScreenHeight == pInfo->aResolutions[i].cy);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                Status = pRecommendMonitorModesArg->pMonitorSourceModeSetInterface->pfnAddMode(
                        pRecommendMonitorModesArg->hMonitorSourceModeSet, pNewMonitorSourceModeInfo);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                    continue;
            }

            /* error has occurred, release & break */
            pRecommendMonitorModesArg->pMonitorSourceModeSetInterface->pfnReleaseModeInfo(
                    pRecommendMonitorModesArg->hMonitorSourceModeSet, pNewMonitorSourceModeInfo);
            break;
        }
    }

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendVidPnTopology(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY* CONST  pRecommendVidPnTopologyArg
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    vboxVDbgBreakFv();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_GRAPHICS_NO_RECOMMENDED_VIDPN_TOPOLOGY;
}

NTSTATUS
APIENTRY
DxgkDdiGetScanLine(
    CONST HANDLE  hAdapter,
    DXGKARG_GETSCANLINE*  pGetScanLine)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;

    Assert((UINT)VBoxCommonFromDeviceExt(pDevExt)->cDisplays > pGetScanLine->VidPnTargetId);
    VBOXWDDM_TARGET *pTarget = &pDevExt->aTargets[pGetScanLine->VidPnTargetId];
    Assert(pTarget->HeightTotal);
    Assert(pTarget->HeightVisible);
    Assert(pTarget->HeightTotal > pTarget->HeightVisible);
    Assert(pTarget->ScanLineState < pTarget->HeightTotal);
    if (pTarget->HeightTotal)
    {
        uint32_t curScanLine = pTarget->ScanLineState;
        ++pTarget->ScanLineState;
        if (pTarget->ScanLineState >= pTarget->HeightTotal)
            pTarget->ScanLineState = 0;


        BOOL bVBlank = (!curScanLine || curScanLine > pTarget->HeightVisible);
        pGetScanLine->ScanLine = curScanLine;
        pGetScanLine->InVerticalBlank = bVBlank;
    }
    else
    {
        pGetScanLine->InVerticalBlank = TRUE;
        pGetScanLine->ScanLine = 0;
    }

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiStopCapture(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_STOPCAPTURE*  pStopCapture)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

    AssertBreakpoint();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiControlInterrupt(
    CONST HANDLE hAdapter,
    CONST DXGK_INTERRUPT_TYPE InterruptType,
    BOOLEAN Enable
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));

//    AssertBreakpoint();

    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));

    /* @todo: STATUS_NOT_IMPLEMENTED ?? */
    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiCreateOverlay(
    CONST HANDLE  hAdapter,
    DXGKARG_CREATEOVERLAY  *pCreateOverlay)
{
    LOGF(("ENTER, hAdapter(0x%p)", hAdapter));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXMP_DEVEXT pDevExt = (PVBOXMP_DEVEXT)hAdapter;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)vboxWddmMemAllocZero(sizeof (VBOXWDDM_OVERLAY));
    Assert(pOverlay);
    if (pOverlay)
    {
        int rc = vboxVhwaHlpOverlayCreate(pDevExt, pCreateOverlay->VidPnSourceId, &pCreateOverlay->OverlayInfo, pOverlay);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            pCreateOverlay->hOverlay = pOverlay;
        }
        else
        {
            vboxWddmMemFree(pOverlay);
            Status = STATUS_UNSUCCESSFUL;
        }
    }
    else
        Status = STATUS_NO_MEMORY;

    LOGF(("LEAVE, hAdapter(0x%p)", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyDevice(
    CONST HANDLE  hDevice)
{
    /* DxgkDdiDestroyDevice should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, hDevice(0x%x)", hDevice));

    vboxVDbgBreakFv();

    vboxWddmMemFree(hDevice);

    LOGF(("LEAVE, "));

    return STATUS_SUCCESS;
}

/*
 * DxgkDdiOpenAllocation
 */
NTSTATUS
APIENTRY
DxgkDdiOpenAllocation(
    CONST HANDLE  hDevice,
    CONST DXGKARG_OPENALLOCATION  *pOpenAllocation)
{
    /* DxgkDdiOpenAllocation should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, hDevice(0x%x)", hDevice));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)hDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;
    PVBOXWDDM_RCINFO pRcInfo = NULL;
    if (pOpenAllocation->PrivateDriverSize)
    {
        Assert(pOpenAllocation->PrivateDriverSize == sizeof (VBOXWDDM_RCINFO));
        Assert(pOpenAllocation->pPrivateDriverData);
        if (pOpenAllocation->PrivateDriverSize >= sizeof (VBOXWDDM_RCINFO))
        {
            pRcInfo = (PVBOXWDDM_RCINFO)pOpenAllocation->pPrivateDriverData;
            Assert(pRcInfo->cAllocInfos == pOpenAllocation->NumAllocations);
        }
        else
            Status = STATUS_INVALID_PARAMETER;
    }

    if (Status == STATUS_SUCCESS)
    {
        UINT i = 0;
        for (; i < pOpenAllocation->NumAllocations; ++i)
        {
            DXGK_OPENALLOCATIONINFO* pInfo = &pOpenAllocation->pOpenAllocation[i];
            Assert(pInfo->PrivateDriverDataSize == sizeof (VBOXWDDM_ALLOCINFO));
            Assert(pInfo->pPrivateDriverData);
            PVBOXWDDM_ALLOCATION pAllocation = vboxWddmGetAllocationFromHandle(pDevExt, pInfo->hAllocation);
            if (!pAllocation)
            {
                WARN(("invalid handle"));
                Status = STATUS_INVALID_PARAMETER;
                break;
            }

            PVBOXWDDM_OPENALLOCATION pOa = (PVBOXWDDM_OPENALLOCATION)vboxWddmMemAllocZero(sizeof (VBOXWDDM_OPENALLOCATION));
            if (!pOa)
            {
                WARN(("failed to allocation alloc info"));
                Status = STATUS_INSUFFICIENT_RESOURCES;
                break;
            }

#ifdef DEBUG
            for (PLIST_ENTRY pCur = pAllocation->OpenList.Flink; pCur != &pAllocation->OpenList; pCur = pCur->Flink)
            {
                PVBOXWDDM_OPENALLOCATION pCurOa = CONTAINING_RECORD(pCur, VBOXWDDM_OPENALLOCATION, ListEntry);
                if (pCurOa->pDevice == pDevice)
                {
                    /* should not happen */
                    Assert(0);
                    break;
                }
            }
            Assert(!pAllocation->fAssumedDeletion);
#endif
            InsertHeadList(&pAllocation->OpenList, &pOa->ListEntry);
            pOa->hAllocation = pInfo->hAllocation;
            pOa->pAllocation = pAllocation;
            pOa->pDevice = pDevice;
            pInfo->hDeviceSpecificAllocation = pOa;


            if (pRcInfo)
            {
                Assert(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC);

                if (pInfo->PrivateDriverDataSize < sizeof (VBOXWDDM_ALLOCINFO)
                        || !pInfo->pPrivateDriverData)
                {
                    WARN(("invalid data size"));
                    vboxWddmMemFree(pOa);
                    Status = STATUS_INVALID_PARAMETER;
                    break;
                }
                PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pInfo->pPrivateDriverData;

#ifdef VBOX_WITH_VIDEOHWACCEL
                if (pRcInfo->RcDesc.fFlags.Overlay)
                {
                    /* we have queried host for some surface info, like pitch & size,
                     * need to return it back to the UMD (User Mode Drive) */
                    pAllocInfo->SurfDesc = pAllocation->SurfDesc;
                    /* success, just continue */
                }
#endif
            }

            ++pAllocation->cOpens;
        }

        if (Status != STATUS_SUCCESS)
        {
            for (UINT j = 0; j < i; ++j)
            {
                DXGK_OPENALLOCATIONINFO* pInfo2Free = &pOpenAllocation->pOpenAllocation[j];
                PVBOXWDDM_OPENALLOCATION pOa2Free = (PVBOXWDDM_OPENALLOCATION)pInfo2Free->hDeviceSpecificAllocation;
                PVBOXWDDM_ALLOCATION pAllocation = pOa2Free->pAllocation;
                RemoveEntryList(&pOa2Free->ListEntry);
                Assert(pAllocation->cOpens);
                --pAllocation->cOpens;
                vboxWddmMemFree(pOa2Free);
            }
        }
    }
    LOGF(("LEAVE, hDevice(0x%x)", hDevice));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiCloseAllocation(
    CONST HANDLE  hDevice,
    CONST DXGKARG_CLOSEALLOCATION*  pCloseAllocation)
{
    /* DxgkDdiCloseAllocation should be made pageable. */
    PAGED_CODE();

    LOGF(("ENTER, hDevice(0x%x)", hDevice));

    vboxVDbgBreakFv();

    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)hDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;

    for (UINT i = 0; i < pCloseAllocation->NumAllocations; ++i)
    {
        PVBOXWDDM_OPENALLOCATION pOa2Free = (PVBOXWDDM_OPENALLOCATION)pCloseAllocation->pOpenHandleList[i];
        PVBOXWDDM_ALLOCATION pAllocation = pOa2Free->pAllocation;
        RemoveEntryList(&pOa2Free->ListEntry);
        Assert(pAllocation->cShRcRefs >= pOa2Free->cShRcRefs);
        pAllocation->cShRcRefs -= pOa2Free->cShRcRefs;
        Assert(pAllocation->cOpens);
        --pAllocation->cOpens;
        vboxWddmMemFree(pOa2Free);
    }

    LOGF(("LEAVE, hDevice(0x%x)", hDevice));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiRender(
    CONST HANDLE  hContext,
    DXGKARG_RENDER  *pRender)
{
//    LOGF(("ENTER, hContext(0x%x)", hContext));

    Assert(pRender->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR));
    if (pRender->DmaBufferPrivateDataSize < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        LOGREL(("Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATEDATA_BASEHDR (%d)",
                pRender->DmaBufferPrivateDataSize , sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        /* @todo: can this actually happen? what status to return? */
        return STATUS_INVALID_PARAMETER;
    }
    if (pRender->CommandLength < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        Assert(0);
        LOGREL(("Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATEDATA_BASEHDR (%d)",
                pRender->DmaBufferPrivateDataSize , sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        /* @todo: can this actually happen? what status to return? */
        return STATUS_INVALID_PARAMETER;
    }

    PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR pInputHdr = (PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR)pRender->pCommand;
    NTSTATUS Status = STATUS_SUCCESS;
    switch (pInputHdr->enmCmd)
    {
        case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
        {
            if (pRender->CommandLength != RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD, aBufInfos[pInputHdr->u32CmdReserved]))
            {
                Assert(0);
                return STATUS_INVALID_PARAMETER;
            }
            PVBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD pUmCmd = (PVBOXWDDM_DMA_PRIVATEDATA_UM_CHROMIUM_CMD)pInputHdr;
            PVBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD pChromiumCmd = (PVBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD)pRender->pDmaBufferPrivateData;
            const uint32_t cbDma = RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD, aBufInfos[pInputHdr->u32CmdReserved]);
            if (pRender->DmaBufferPrivateDataSize < cbDma)
            {
                Assert(0);
                return STATUS_INVALID_PARAMETER;
            }
            if (pRender->DmaSize < cbDma)
            {
                Assert(0);
                return STATUS_INVALID_PARAMETER;
            }

            if (pRender->PatchLocationListOutSize < pInputHdr->u32CmdReserved)
            {
                Assert(0);
                return STATUS_INVALID_PARAMETER;
            }

            PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
            PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
            PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;

            pChromiumCmd->Base.enmCmd = VBOXVDMACMD_TYPE_CHROMIUM_CMD;
            pChromiumCmd->Base.u32CmdReserved = pInputHdr->u32CmdReserved;
            pRender->pDmaBufferPrivateData = (uint8_t*)pRender->pDmaBufferPrivateData + cbDma;
            pRender->pDmaBuffer = ((uint8_t*)pRender->pDmaBuffer) + cbDma;
            D3DDDI_PATCHLOCATIONLIST* pPLL = pRender->pPatchLocationListOut;
            memset(pPLL, 0, sizeof (*pPLL) * pChromiumCmd->Base.u32CmdReserved);
            pRender->pPatchLocationListOut += pInputHdr->u32CmdReserved;
            PVBOXWDDM_UHGSMI_BUFFER_SUBMIT_INFO pSubmInfo = pChromiumCmd->aBufInfos;
            PVBOXWDDM_UHGSMI_BUFFER_UI_SUBMIT_INFO pSubmUmInfo = pUmCmd->aBufInfos;
            DXGK_ALLOCATIONLIST *pAllocationList = pRender->pAllocationList;
            for (UINT i = 0; i < pChromiumCmd->Base.u32CmdReserved; ++i)
            {
                PVBOXWDDM_ALLOCATION pAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pAllocationList);
                vboxWddmPopulateDmaAllocInfoWithOffset(&pSubmInfo->Alloc, pAlloc, pAllocationList, pSubmUmInfo->offData);

                pSubmInfo->cbData = pSubmUmInfo->cbData;
                pSubmInfo->bDoNotSignalCompletion = pSubmUmInfo->fSubFlags.bDoNotSignalCompletion;

                pPLL->AllocationIndex = i;
                pPLL->PatchOffset = RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD, aBufInfos[i].Alloc);
                pPLL->AllocationOffset = pSubmUmInfo->offData;

                ++pPLL;
                ++pSubmInfo;
                ++pSubmUmInfo;
                ++pAllocationList;
            }

            break;
        }
        case VBOXVDMACMD_TYPE_DMA_NOP:
        {
            PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR pPrivateData = (PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR)pRender->pDmaBufferPrivateData;
            pPrivateData->enmCmd = VBOXVDMACMD_TYPE_DMA_NOP;

            pRender->pDmaBufferPrivateData = (uint8_t*)pRender->pDmaBufferPrivateData + sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR);
            pRender->pDmaBuffer = ((uint8_t*)pRender->pDmaBuffer) + pRender->CommandLength;
            Assert(pRender->DmaSize >= pRender->CommandLength);
            Assert(pRender->PatchLocationListOutSize >= pRender->PatchLocationListInSize);
            UINT cbPLL = pRender->PatchLocationListInSize * sizeof (pRender->pPatchLocationListOut[0]);
            memcpy(pRender->pPatchLocationListOut, pRender->pPatchLocationListIn, cbPLL);
            pRender->pPatchLocationListOut += pRender->PatchLocationListInSize;
            break;
        }
        default:
            return STATUS_INVALID_PARAMETER;
    }

//    LOGF(("LEAVE, hContext(0x%x)", hContext));

    return Status;
}

#define VBOXVDMACMD_DMA_PRESENT_BLT_MINSIZE() (VBOXVDMACMD_SIZE(VBOXVDMACMD_DMA_PRESENT_BLT))
#define VBOXVDMACMD_DMA_PRESENT_BLT_SIZE(_c) (VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, aDstSubRects[_c]))

#ifdef VBOX_WITH_VDMA
DECLINLINE(VOID) vboxWddmRectlFromRect(const RECT *pRect, PVBOXVDMA_RECTL pRectl)
{
    pRectl->left = (int16_t)pRect->left;
    pRectl->width = (uint16_t)(pRect->right - pRect->left);
    pRectl->top = (int16_t)pRect->top;
    pRectl->height = (uint16_t)(pRect->bottom - pRect->top);
}

DECLINLINE(VBOXVDMA_PIXEL_FORMAT) vboxWddmFromPixFormat(D3DDDIFORMAT format)
{
    return (VBOXVDMA_PIXEL_FORMAT)format;
}

DECLINLINE(VOID) vboxWddmSurfDescFromAllocation(PVBOXWDDM_ALLOCATION pAllocation, PVBOXVDMA_SURF_DESC pDesc)
{
    pDesc->width = pAllocation->SurfDesc.width;
    pDesc->height = pAllocation->SurfDesc.height;
    pDesc->format = vboxWddmFromPixFormat(pAllocation->SurfDesc.format);
    pDesc->bpp = pAllocation->SurfDesc.bpp;
    pDesc->pitch = pAllocation->SurfDesc.pitch;
    pDesc->fFlags = 0;
}
#endif

DECLINLINE(BOOLEAN) vboxWddmPixFormatConversionSupported(D3DDDIFORMAT From, D3DDDIFORMAT To)
{
    Assert(From != D3DDDIFMT_UNKNOWN);
    Assert(To != D3DDDIFMT_UNKNOWN);
    Assert(From == To);
    return From == To;
}

#if 0
DECLINLINE(bool) vboxWddmCheckForVisiblePrimary(PVBOXMP_DEVEXT pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    !!!primary could be of pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC!!!
    if (pAllocation->enmType != VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
        return false;

    if (!pAllocation->bVisible)
        return false;

    D3DDDI_VIDEO_PRESENT_SOURCE_ID id = pAllocation->SurfDesc.VidPnSourceId;
    if (id >= (D3DDDI_VIDEO_PRESENT_SOURCE_ID)VBoxCommonFromDeviceExt(pDevExt)->cDisplays)
        return false;

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[id];
    if (pSource->pPrimaryAllocation != pAllocation)
        return false;

    return true;
}
#endif

/**
 * DxgkDdiPresent
 */
NTSTATUS
APIENTRY
DxgkDdiPresent(
    CONST HANDLE  hContext,
    DXGKARG_PRESENT  *pPresent)
{
    PAGED_CODE();

//    LOGF(("ENTER, hContext(0x%x)", hContext));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;

    Assert(pPresent->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR));
    if (pPresent->DmaBufferPrivateDataSize < sizeof (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR))
    {
        LOGREL(("Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR (%d)", pPresent->DmaBufferPrivateDataSize , sizeof (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR)));
        /* @todo: can this actually happen? what status tu return? */
        return STATUS_INVALID_PARAMETER;
    }

    PVBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR pPrivateData = (PVBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR)pPresent->pDmaBufferPrivateData;
    pPrivateData->BaseHdr.fFlags.Value = 0;
    uint32_t cContexts3D = ASMAtomicReadU32(&pDevExt->cContexts3D);
#define VBOXWDDM_DUMMY_DMABUFFER_SIZE sizeof(RECT)

    if (pPresent->Flags.Blt)
    {
        Assert(pPresent->Flags.Value == 1); /* only Blt is set, we do not support anything else for now */
        DXGK_ALLOCATIONLIST *pSrc =  &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
        DXGK_ALLOCATIONLIST *pDst =  &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];
        PVBOXWDDM_ALLOCATION pSrcAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pSrc);
        Assert(pSrcAlloc);
        if (pSrcAlloc)
        {
            PVBOXWDDM_ALLOCATION pDstAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pDst);
            Assert(pDstAlloc);
            if (pDstAlloc)
            {
                do
                {
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
#if 0
                    Assert (pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE);
                    Assert (pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE);
#else
                    if (pContext->enmType == VBOXWDDM_CONTEXT_TYPE_SYSTEM)
                    {
                        Assert ((pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE
                                && pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
                                || (pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
                                        && pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE));
                    }
#endif
                    /* issue VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE ONLY in case there are no 3D contexts currently
                     * otherwise we would need info about all rects being updated on primary for visible rect reporting */
                    if (!cContexts3D)
                    {
                        if (pDstAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE
                                && pSrcAlloc->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE)
                        {
                            Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_SYSTEM);
                            Assert(pDstAlloc->bAssigned);
                            Assert(pDstAlloc->bVisible);
                            if (pDstAlloc->bAssigned
                                    && pDstAlloc->bVisible)
                            {
#ifdef VBOX_WITH_VIDEOHWACCEL
                                if (vboxVhwaHlpOverlayListIsEmpty(pDevExt, pDstAlloc->SurfDesc.VidPnSourceId))
#endif
                                {
                                    Assert(pPresent->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY));
                                    if (pPresent->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY))
                                    {
                                        VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pDstAlloc->SurfDesc.VidPnSourceId];
                                        vboxWddmAssignShadow(pDevExt, pSource, pSrcAlloc, pDstAlloc->SurfDesc.VidPnSourceId);
                                        Assert(pPresent->SrcRect.left == pPresent->DstRect.left);
                                        Assert(pPresent->SrcRect.right == pPresent->DstRect.right);
                                        Assert(pPresent->SrcRect.top == pPresent->DstRect.top);
                                        Assert(pPresent->SrcRect.bottom == pPresent->DstRect.bottom);
                                        RECT rect;
                                        if (pPresent->SubRectCnt)
                                        {
                                            rect = pPresent->pDstSubRects[0];
                                            for (UINT i = 1; i < pPresent->SubRectCnt; ++i)
                                            {
                                                vboxWddmRectUnited(&rect, &rect, &pPresent->pDstSubRects[i]);
                                            }
                                        }
                                        else
                                            rect = pPresent->SrcRect;


                                        pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + sizeof (VBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY);
                                        pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VBOXWDDM_DUMMY_DMABUFFER_SIZE;
                                        Assert(pPresent->DmaSize >= VBOXWDDM_DUMMY_DMABUFFER_SIZE);
                                        memset(pPresent->pPatchLocationListOut, 0, 2*sizeof (D3DDDI_PATCHLOCATIONLIST));
                                        pPresent->pPatchLocationListOut->PatchOffset = 0;
                                        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
                                        ++pPresent->pPatchLocationListOut;
                                        pPresent->pPatchLocationListOut->PatchOffset = 4;
                                        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
                                        ++pPresent->pPatchLocationListOut;

                                        pPrivateData->BaseHdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_SHADOW2PRIMARY;
                                        PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY pS2P = (PVBOXWDDM_DMA_PRIVATEDATA_SHADOW2PRIMARY)pPrivateData;
                                        /* we do not know the shadow address yet, perform dummy DMA cycle */
                                        vboxWddmPopulateDmaAllocInfo(&pS2P->Shadow2Primary.ShadowAlloc, pSrcAlloc, pSrc);
//                                        vboxWddmPopulateDmaAllocInfo(&pPrivateData->DstAllocInfo, pDstAlloc, pDst);
                                        pS2P->Shadow2Primary.SrcRect = rect;
                                        pS2P->Shadow2Primary.VidPnSourceId = pDstAlloc->SurfDesc.VidPnSourceId;
                                        break;
                                    }
                                    else
                                    {
                                        Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    /* we're here because this is NOT a shadow->primary update
                     * or because there are d3d contexts and we need to report visible rects
                     * or because we have overlays active and we need a special handling for primary */
#endif
                    UINT cbCmd = pPresent->DmaBufferPrivateDataSize;
                    pPrivateData->BaseHdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_BLT;

                    PVBOXWDDM_DMA_PRIVATEDATA_BLT pBlt = (PVBOXWDDM_DMA_PRIVATEDATA_BLT)pPrivateData;

                    vboxWddmPopulateDmaAllocInfo(&pBlt->Blt.SrcAlloc, pSrcAlloc, pSrc);
                    vboxWddmPopulateDmaAllocInfo(&pBlt->Blt.DstAlloc, pDstAlloc, pDst);

                    ASSERT_WARN(!pSrcAlloc->fRcFlags.SharedResource, ("Shared Allocatoin used in Present!"));

                    pBlt->Blt.SrcRect = pPresent->SrcRect;
                    pBlt->Blt.DstRects.ContextRect = pPresent->DstRect;
                    pBlt->Blt.DstRects.UpdateRects.cRects = 0;
                    UINT cbHead = RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_BLT, Blt.DstRects.UpdateRects.aRects[0]);
                    Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);
                    UINT cbRects = (pPresent->SubRectCnt - pPresent->MultipassOffset) * sizeof (RECT);
                    pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbHead + cbRects;
                    pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VBOXWDDM_DUMMY_DMABUFFER_SIZE;
                    Assert(pPresent->DmaSize >= VBOXWDDM_DUMMY_DMABUFFER_SIZE);
                    cbCmd -= cbHead;
                    Assert(cbCmd < UINT32_MAX/2);
                    Assert(cbCmd > sizeof (RECT));
                    if (cbCmd >= cbRects)
                    {
                        cbCmd -= cbRects;
                        memcpy(&pBlt->Blt.DstRects.UpdateRects.aRects[pPresent->MultipassOffset], pPresent->pDstSubRects, cbRects);
                        pBlt->Blt.DstRects.UpdateRects.cRects += cbRects/sizeof (RECT);
                    }
                    else
                    {
                        UINT cbFitingRects = (cbCmd/sizeof (RECT)) * sizeof (RECT);
                        Assert(cbFitingRects);
                        memcpy(&pBlt->Blt.DstRects.UpdateRects.aRects[pPresent->MultipassOffset], pPresent->pDstSubRects, cbFitingRects);
                        cbCmd -= cbFitingRects;
                        pPresent->MultipassOffset += cbFitingRects/sizeof (RECT);
                        pBlt->Blt.DstRects.UpdateRects.cRects += cbFitingRects/sizeof (RECT);
                        Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);
                        Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                    }

                    memset(pPresent->pPatchLocationListOut, 0, 2*sizeof (D3DDDI_PATCHLOCATIONLIST));
                    pPresent->pPatchLocationListOut->PatchOffset = 0;
                    pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
                    ++pPresent->pPatchLocationListOut;
                    pPresent->pPatchLocationListOut->PatchOffset = 4;
                    pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
                    ++pPresent->pPatchLocationListOut;

                    break;
#ifdef VBOX_WITH_VDMA
                    cbCmd = pPresent->DmaSize;

                    Assert(pPresent->SubRectCnt);
                    UINT cmdSize = VBOXVDMACMD_DMA_PRESENT_BLT_SIZE(pPresent->SubRectCnt - pPresent->MultipassOffset);
                    PVBOXVDMACMD pCmd = (PVBOXVDMACMD)pPresent->pDmaBuffer;
                    pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + cmdSize;
                    Assert(cbCmd >= VBOXVDMACMD_DMA_PRESENT_BLT_MINSIZE());
                    if (cbCmd >= VBOXVDMACMD_DMA_PRESENT_BLT_MINSIZE())
                    {
                        if (vboxWddmPixFormatConversionSupported(pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format))
                        {
                            memset(pPresent->pPatchLocationListOut, 0, 2*sizeof (D3DDDI_PATCHLOCATIONLIST));
            //                        pPresent->pPatchLocationListOut->PatchOffset = 0;
            //                        ++pPresent->pPatchLocationListOut;
                            pPresent->pPatchLocationListOut->PatchOffset = VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, offSrc);
                            pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
                            ++pPresent->pPatchLocationListOut;
                            pPresent->pPatchLocationListOut->PatchOffset = VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, offDst);
                            pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
                            ++pPresent->pPatchLocationListOut;

                            pCmd->enmType = VBOXVDMACMD_TYPE_DMA_PRESENT_BLT;
                            pCmd->u32CmdSpecific = 0;
                            PVBOXVDMACMD_DMA_PRESENT_BLT pTransfer = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_PRESENT_BLT);
                            pTransfer->offSrc = (VBOXVIDEOOFFSET)pSrc->PhysicalAddress.QuadPart;
                            pTransfer->offDst = (VBOXVIDEOOFFSET)pDst->PhysicalAddress.QuadPart;
                            vboxWddmSurfDescFromAllocation(pSrcAlloc, &pTransfer->srcDesc);
                            vboxWddmSurfDescFromAllocation(pDstAlloc, &pTransfer->dstDesc);
                            vboxWddmRectlFromRect(&pPresent->SrcRect, &pTransfer->srcRectl);
                            vboxWddmRectlFromRect(&pPresent->DstRect, &pTransfer->dstRectl);
                            UINT i = 0;
                            cbCmd -= VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, aDstSubRects);
                            Assert(cbCmd >= sizeof (VBOXVDMA_RECTL));
                            Assert(cbCmd < pPresent->DmaSize);
                            for (; i < pPresent->SubRectCnt; ++i)
                            {
                                if (cbCmd < sizeof (VBOXVDMA_RECTL))
                                {
                                    Assert(i);
                                    pPresent->MultipassOffset += i;
                                    Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                                    break;
                                }
                                vboxWddmRectlFromRect(&pPresent->pDstSubRects[i + pPresent->MultipassOffset], &pTransfer->aDstSubRects[i]);
                                cbCmd -= sizeof (VBOXVDMA_RECTL);
                            }
                            Assert(i);
                            pPrivateData->BaseHdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_BLT;
                            pTransfer->cDstSubRects = i;
                            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + sizeof(VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR);
                        }
                        else
                        {
                            AssertBreakpoint();
                            LOGREL(("unsupported format conversion from(%d) to (%d)",pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format));
                            Status = STATUS_GRAPHICS_CANNOTCOLORCONVERT;
                        }
                    }
                    else
                    {
                        /* this should not happen actually */
                        LOGREL(("cbCmd too small!! (%d)", cbCmd));
                        Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                    }
#endif
                } while(0);
            }
            else
            {
                /* this should not happen actually */
                LOGREL(("failed to get Dst Allocation info for hDeviceSpecificAllocation(0x%x)",pDst->hDeviceSpecificAllocation));
                Status = STATUS_INVALID_HANDLE;
            }
        }
        else
        {
            /* this should not happen actually */
            LOGREL(("failed to get Src Allocation info for hDeviceSpecificAllocation(0x%x)",pSrc->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
        }
#if 0
        UINT cbCmd = pPresent->DmaSize;

        Assert(pPresent->SubRectCnt);
        UINT cmdSize = VBOXVDMACMD_DMA_PRESENT_BLT_SIZE(pPresent->SubRectCnt - pPresent->MultipassOffset);
        PVBOXVDMACMD pCmd = (PVBOXVDMACMD)pPresent->pDmaBuffer;
        pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + cmdSize;
        Assert(cbCmd >= VBOXVDMACMD_DMA_PRESENT_BLT_MINSIZE());
        if (cbCmd >= VBOXVDMACMD_DMA_PRESENT_BLT_MINSIZE())
        {
            DXGK_ALLOCATIONLIST *pSrc =  &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
            DXGK_ALLOCATIONLIST *pDst =  &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];
            PVBOXWDDM_ALLOCATION pSrcAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pSrc);
            Assert(pSrcAlloc);
            if (pSrcAlloc)
            {
                PVBOXWDDM_ALLOCATION pDstAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pDst);
                Assert(pDstAlloc);
                if (pDstAlloc)
                {
                    if (vboxWddmPixFormatConversionSupported(pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format))
                    {
                        memset(pPresent->pPatchLocationListOut, 0, 2*sizeof (D3DDDI_PATCHLOCATIONLIST));
//                        pPresent->pPatchLocationListOut->PatchOffset = 0;
//                        ++pPresent->pPatchLocationListOut;
                        pPresent->pPatchLocationListOut->PatchOffset = VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, offSrc);
                        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
                        ++pPresent->pPatchLocationListOut;
                        pPresent->pPatchLocationListOut->PatchOffset = VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, offDst);
                        pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
                        ++pPresent->pPatchLocationListOut;

                        pCmd->enmType = VBOXVDMACMD_TYPE_DMA_PRESENT_BLT;
                        pCmd->u32CmdSpecific = 0;
                        PVBOXVDMACMD_DMA_PRESENT_BLT pTransfer = VBOXVDMACMD_BODY(pCmd, VBOXVDMACMD_DMA_PRESENT_BLT);
                        pTransfer->offSrc = (VBOXVIDEOOFFSET)pSrc->PhysicalAddress.QuadPart;
                        pTransfer->offDst = (VBOXVIDEOOFFSET)pDst->PhysicalAddress.QuadPart;
                        vboxWddmSurfDescFromAllocation(pSrcAlloc, &pTransfer->srcDesc);
                        vboxWddmSurfDescFromAllocation(pDstAlloc, &pTransfer->dstDesc);
                        vboxWddmRectlFromRect(&pPresent->SrcRect, &pTransfer->srcRectl);
                        vboxWddmRectlFromRect(&pPresent->DstRect, &pTransfer->dstRectl);
                        UINT i = 0;
                        cbCmd -= VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, aDstSubRects);
                        Assert(cbCmd >= sizeof (VBOXVDMA_RECTL));
                        Assert(cbCmd < pPresent->DmaSize);
                        for (; i < pPresent->SubRectCnt; ++i)
                        {
                            if (cbCmd < sizeof (VBOXVDMA_RECTL))
                            {
                                Assert(i);
                                pPresent->MultipassOffset += i;
                                Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                                break;
                            }
                            vboxWddmRectlFromRect(&pPresent->pDstSubRects[i + pPresent->MultipassOffset], &pTransfer->aDstSubRects[i]);
                            cbCmd -= sizeof (VBOXVDMA_RECTL);
                        }
                        Assert(i);
                        pTransfer->cDstSubRects = i;
                        pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + sizeof(VBOXWDDM_DMA_PRIVATEDATA_HDR);
                    }
                    else
                    {
                        AssertBreakpoint();
                        LOGREL(("unsupported format conversion from(%d) to (%d)",pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format));
                        Status = STATUS_GRAPHICS_CANNOTCOLORCONVERT;
                    }
                }
                else
                {
                    /* this should not happen actually */
                    LOGREL(("failed to get Dst Allocation info for hDeviceSpecificAllocation(0x%x)",pDst->hDeviceSpecificAllocation));
                    Status = STATUS_INVALID_HANDLE;
                }
            }
            else
            {
                /* this should not happen actually */
                LOGREL(("failed to get Src Allocation info for hDeviceSpecificAllocation(0x%x)",pSrc->hDeviceSpecificAllocation));
                Status = STATUS_INVALID_HANDLE;
            }
        }
        else
        {
            /* this should not happen actually */
            LOGREL(("cbCmd too small!! (%d)", cbCmd));
            Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
        }
#endif
    }
    else if (pPresent->Flags.Flip)
    {
        Assert(pPresent->Flags.Value == 4); /* only Blt is set, we do not support anything else for now */
        Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D);
        DXGK_ALLOCATIONLIST *pSrc =  &pPresent->pAllocationList[DXGK_PRESENT_SOURCE_INDEX];
        PVBOXWDDM_ALLOCATION pSrcAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pSrc);
        Assert(pSrcAlloc);
        if (pSrcAlloc)
        {
            Assert(cContexts3D);
            pPrivateData->BaseHdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP;
            PVBOXWDDM_DMA_PRIVATEDATA_FLIP pFlip = (PVBOXWDDM_DMA_PRIVATEDATA_FLIP)pPrivateData;

            vboxWddmPopulateDmaAllocInfo(&pFlip->Flip.Alloc, pSrcAlloc, pSrc);

            UINT cbCmd = sizeof (VBOXWDDM_DMA_PRIVATEDATA_FLIP);
            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbCmd;
            pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VBOXWDDM_DUMMY_DMABUFFER_SIZE;
            Assert(pPresent->DmaSize >= VBOXWDDM_DUMMY_DMABUFFER_SIZE);

            memset(pPresent->pPatchLocationListOut, 0, sizeof (D3DDDI_PATCHLOCATIONLIST));
            pPresent->pPatchLocationListOut->PatchOffset = 0;
            pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_SOURCE_INDEX;
            ++pPresent->pPatchLocationListOut;
        }
        else
        {
            /* this should not happen actually */
            LOGREL(("failed to get pSrc Allocation info for hDeviceSpecificAllocation(0x%x)",pSrc->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
        }
    }
    else if (pPresent->Flags.ColorFill)
    {
        Assert(pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_2D);
        Assert(pPresent->Flags.Value == 2); /* only ColorFill is set, we do not support anything else for now */
        DXGK_ALLOCATIONLIST *pDst =  &pPresent->pAllocationList[DXGK_PRESENT_DESTINATION_INDEX];
        PVBOXWDDM_ALLOCATION pDstAlloc = vboxWddmGetAllocationFromAllocList(pDevExt, pDst);
        Assert(pDstAlloc);
        if (pDstAlloc)
        {
            UINT cbCmd = pPresent->DmaBufferPrivateDataSize;
            pPrivateData->BaseHdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL;
            PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateData;

            vboxWddmPopulateDmaAllocInfo(&pCF->ClrFill.Alloc, pDstAlloc, pDst);

            pCF->ClrFill.Color = pPresent->Color;
            pCF->ClrFill.Rects.cRects = 0;
            UINT cbHead = RT_OFFSETOF(VBOXWDDM_DMA_PRIVATEDATA_CLRFILL, ClrFill.Rects.aRects[0]);
            Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);
            UINT cbRects = (pPresent->SubRectCnt - pPresent->MultipassOffset) * sizeof (RECT);
            pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + cbHead + cbRects;
            pPresent->pDmaBuffer = ((uint8_t*)pPresent->pDmaBuffer) + VBOXWDDM_DUMMY_DMABUFFER_SIZE;
            Assert(pPresent->DmaSize >= VBOXWDDM_DUMMY_DMABUFFER_SIZE);
            cbCmd -= cbHead;
            Assert(cbCmd < UINT32_MAX/2);
            Assert(cbCmd > sizeof (RECT));
            if (cbCmd >= cbRects)
            {
                cbCmd -= cbRects;
                memcpy(&pCF->ClrFill.Rects.aRects[pPresent->MultipassOffset], pPresent->pDstSubRects, cbRects);
                pCF->ClrFill.Rects.cRects += cbRects/sizeof (RECT);
            }
            else
            {
                UINT cbFitingRects = (cbCmd/sizeof (RECT)) * sizeof (RECT);
                Assert(cbFitingRects);
                memcpy(&pCF->ClrFill.Rects.aRects[pPresent->MultipassOffset], pPresent->pDstSubRects, cbFitingRects);
                cbCmd -= cbFitingRects;
                pPresent->MultipassOffset += cbFitingRects/sizeof (RECT);
                pCF->ClrFill.Rects.cRects += cbFitingRects/sizeof (RECT);
                Assert(pPresent->SubRectCnt > pPresent->MultipassOffset);
                Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
            }

            memset(pPresent->pPatchLocationListOut, 0, sizeof (D3DDDI_PATCHLOCATIONLIST));
            pPresent->pPatchLocationListOut->PatchOffset = 0;
            pPresent->pPatchLocationListOut->AllocationIndex = DXGK_PRESENT_DESTINATION_INDEX;
            ++pPresent->pPatchLocationListOut;
        }
        else
        {
            /* this should not happen actually */
            LOGREL(("failed to get pDst Allocation info for hDeviceSpecificAllocation(0x%x)",pDst->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
        }

    }
    else
    {
        LOGREL(("cmd NOT IMPLEMENTED!! Flags(0x%x)", pPresent->Flags.Value));
        AssertBreakpoint();
    }

//    LOGF(("LEAVE, hContext(0x%x), Status(0x%x)", hContext, Status));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_UPDATEOVERLAY  *pUpdateOverlay)
{
    LOGF(("ENTER, hOverlay(0x%p)", hOverlay));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)hOverlay;
    Assert(pOverlay);
    int rc = vboxVhwaHlpOverlayUpdate(pOverlay, &pUpdateOverlay->OverlayInfo);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
    {
        RECT DstRect;
        vboxVhwaHlpOverlayDstRectGet(pOverlay->pDevExt, pOverlay, &DstRect);
        Status = vboxVdmaHlpUpdatePrimary(pOverlay->pDevExt, pOverlay->VidPnSourceId, &DstRect);
        Assert(Status == STATUS_SUCCESS);
    }
    else
        Status = STATUS_UNSUCCESSFUL;

    LOGF(("LEAVE, hOverlay(0x%p)", hOverlay));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiFlipOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_FLIPOVERLAY  *pFlipOverlay)
{
    LOGF(("ENTER, hOverlay(0x%p)", hOverlay));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)hOverlay;
    Assert(pOverlay);
    int rc = vboxVhwaHlpOverlayFlip(pOverlay, pFlipOverlay);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        Status = STATUS_UNSUCCESSFUL;

    LOGF(("LEAVE, hOverlay(0x%p)", hOverlay));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyOverlay(
    CONST HANDLE  hOverlay)
{
    LOGF(("ENTER, hOverlay(0x%p)", hOverlay));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)hOverlay;
    Assert(pOverlay);
    int rc = vboxVhwaHlpOverlayDestroy(pOverlay);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        vboxWddmMemFree(pOverlay);
    else
        Status = STATUS_UNSUCCESSFUL;

    LOGF(("LEAVE, hOverlay(0x%p)", hOverlay));

    return Status;
}

/**
 * DxgkDdiCreateContext
 */
NTSTATUS
APIENTRY
DxgkDdiCreateContext(
    CONST HANDLE  hDevice,
    DXGKARG_CREATECONTEXT  *pCreateContext)
{
    /* DxgkDdiCreateContext should be made pageable */
    PAGED_CODE();

    LOGF(("ENTER, hDevice(0x%x)", hDevice));

    vboxVDbgBreakFv();

    if (pCreateContext->NodeOrdinal >= VBOXWDDM_NUM_NODES)
    {
        WARN(("Invalid NodeOrdinal (%d), expected to be less that (%d)\n", pCreateContext->NodeOrdinal, VBOXWDDM_NUM_NODES));
        return STATUS_INVALID_PARAMETER;
    }

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)hDevice;
    PVBOXMP_DEVEXT pDevExt = pDevice->pAdapter;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)vboxWddmMemAllocZero(sizeof (VBOXWDDM_CONTEXT));
    Assert(pContext);
    if (pContext)
    {
        pContext->pDevice = pDevice;
        pContext->hContext = pCreateContext->hContext;
        pContext->EngineAffinity = pCreateContext->EngineAffinity;
        pContext->NodeOrdinal = pCreateContext->NodeOrdinal;
        vboxVideoCmCtxInitEmpty(&pContext->CmContext);
        if (pCreateContext->Flags.SystemContext || pCreateContext->PrivateDriverDataSize == 0)
        {
            Assert(pCreateContext->PrivateDriverDataSize == 0);
            Assert(!pCreateContext->pPrivateDriverData);
            Assert(pCreateContext->Flags.Value <= 2); /* 2 is a GDI context in Win7 */
            pContext->enmType = VBOXWDDM_CONTEXT_TYPE_SYSTEM;
            for (int i = 0; i < VBoxCommonFromDeviceExt(pDevExt)->cDisplays; ++i)
            {
                pDevExt->aSources[i].offVram = VBOXVIDEOOFFSET_VOID;
                NTSTATUS tmpStatus= vboxWddmDisplaySettingsQueryPos(pDevExt, i, &pDevExt->aSources[i].VScreenPos);
                Assert(tmpStatus == STATUS_SUCCESS);
            }
        }
        else
        {
            Assert(pCreateContext->Flags.Value == 0);
            Assert(pCreateContext->PrivateDriverDataSize == sizeof (VBOXWDDM_CREATECONTEXT_INFO));
            Assert(pCreateContext->pPrivateDriverData);
            if (pCreateContext->PrivateDriverDataSize == sizeof (VBOXWDDM_CREATECONTEXT_INFO))
            {
                PVBOXWDDM_CREATECONTEXT_INFO pInfo = (PVBOXWDDM_CREATECONTEXT_INFO)pCreateContext->pPrivateDriverData;
                switch (pInfo->enmType)
                {
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D:
                    {
                        Status = vboxVideoAMgrCtxCreate(&pDevExt->AllocMgr, &pContext->AllocContext);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status == STATUS_SUCCESS)
                        {
                            Status = vboxWddmSwapchainCtxInit(pDevExt, pContext);
                            Assert(Status == STATUS_SUCCESS);
                            if (Status == STATUS_SUCCESS)
                            {
                                pContext->enmType = VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D;
                                Status = vboxVideoCmCtxAdd(&pDevice->pAdapter->CmMgr, &pContext->CmContext, (HANDLE)pInfo->hUmEvent, pInfo->u64UmInfo);
                                Assert(Status == STATUS_SUCCESS);
                                if (Status == STATUS_SUCCESS)
                                {
        //                            Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
        //                            ExAcquireFastMutex(&pDevExt->ContextMutex);
                                    ASMAtomicIncU32(&pDevExt->cContexts3D);
        //                            ExReleaseFastMutex(&pDevExt->ContextMutex);
                                    break;
                                }

                                vboxWddmSwapchainCtxTerm(pDevExt, pContext);
                            }
                        }
                        break;
                    }
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_3D:
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_GL:
                    {
                        Status = vboxVideoAMgrCtxCreate(&pDevExt->AllocMgr, &pContext->AllocContext);
                        Assert(Status == STATUS_SUCCESS);
                        if (Status != STATUS_SUCCESS)
                            break;
                        /* do not break to go to the _2D branch and do the rest stuff */
                    }
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_2D:
                    {
                        pContext->enmType = pInfo->enmType;
                        break;
                    }
                    default:
                    {
                        Assert(0);
                        Status = STATUS_INVALID_PARAMETER;
                        break;
                    }
                }
            }
        }

        if (Status == STATUS_SUCCESS)
        {
            pCreateContext->hContext = pContext;
            pCreateContext->ContextInfo.DmaBufferSize = VBOXWDDM_C_DMA_BUFFER_SIZE;
            pCreateContext->ContextInfo.DmaBufferSegmentSet = 0;
            pCreateContext->ContextInfo.DmaBufferPrivateDataSize = VBOXWDDM_C_DMA_PRIVATEDATA_SIZE;
            pCreateContext->ContextInfo.AllocationListSize = VBOXWDDM_C_ALLOC_LIST_SIZE;
            pCreateContext->ContextInfo.PatchLocationListSize = VBOXWDDM_C_PATH_LOCATION_LIST_SIZE;
        //#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN7)
        //# error port to Win7 DDI
        //    //pCreateContext->ContextInfo.DmaBufferAllocationGroup = ???;
        //#endif // DXGKDDI_INTERFACE_VERSION
        }
        else
            vboxWddmMemFree(pContext);
    }
    else
        Status = STATUS_NO_MEMORY;

    LOGF(("LEAVE, hDevice(0x%x)", hDevice));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyContext(
    CONST HANDLE  hContext)
{
    LOGF(("ENTER, hContext(0x%x)", hContext));
    vboxVDbgBreakFv();
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXMP_DEVEXT pDevExt = pContext->pDevice->pAdapter;
    if (pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D)
    {
        Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
//        ExAcquireFastMutex(&pDevExt->ContextMutex);
//        RemoveEntryList(&pContext->ListEntry);
        uint32_t cContexts = ASMAtomicDecU32(&pDevExt->cContexts3D);
//        ExReleaseFastMutex(&pDevExt->ContextMutex);
        Assert(cContexts < UINT32_MAX/2);
    }

    /* first terminate the swapchain, this will also ensure
     * all currently pending driver->user Cm commands
     * (i.e. visible regions commands) are completed */
    vboxWddmSwapchainCtxTerm(pDevExt, pContext);

    NTSTATUS Status = vboxVideoAMgrCtxDestroy(&pContext->AllocContext);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxVideoCmCtxRemove(&pContext->pDevice->pAdapter->CmMgr, &pContext->CmContext);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            vboxWddmMemFree(pContext);
        }
    }

    LOGF(("LEAVE, hContext(0x%x)", hContext));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiLinkDevice(
    __in CONST PDEVICE_OBJECT  PhysicalDeviceObject,
    __in CONST PVOID  MiniportDeviceContext,
    __inout PLINKED_DEVICE  LinkedDevice
    )
{
    LOGF(("ENTER, MiniportDeviceContext(0x%x)", MiniportDeviceContext));
    vboxVDbgBreakFv();
    AssertBreakpoint();
    LOGF(("LEAVE, MiniportDeviceContext(0x%x)", MiniportDeviceContext));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetDisplayPrivateDriverFormat(
    CONST HANDLE  hAdapter,
    /*CONST*/ DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT*  pSetDisplayPrivateDriverFormat
    )
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));
    vboxVDbgBreakFv();
    AssertBreakpoint();
    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY CALLBACK DxgkDdiRestartFromTimeout(IN_CONST_HANDLE hAdapter)
{
    LOGF(("ENTER, hAdapter(0x%x)", hAdapter));
    vboxVDbgBreakFv();
    AssertBreakpoint();
    LOGF(("LEAVE, hAdapter(0x%x)", hAdapter));
    return STATUS_SUCCESS;
}

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    PAGED_CODE();

    vboxVDbgBreakFv();

/*
    int irc = RTR0Init(0);
    if (RT_FAILURE(irc))
    {
        LOGREL(("VBoxMP::failed to init IPRT (rc=%#x)", irc));
        return ERROR_INVALID_FUNCTION;
    }
*/

#ifdef DEBUG_misha
    RTLogGroupSettings(0, "+default.e.l.f.l2.l3");
#endif

    LOGREL(("Built %s %s", __DATE__, __TIME__));

    DRIVER_INITIALIZATION_DATA DriverInitializationData = {'\0'};

    if (! ARGUMENT_PRESENT(DriverObject) ||
        ! ARGUMENT_PRESENT(RegistryPath))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Fill in the DriverInitializationData structure and call DxgkInitialize()
    DriverInitializationData.Version = DXGKDDI_INTERFACE_VERSION;

    DriverInitializationData.DxgkDdiAddDevice = DxgkDdiAddDevice;
    DriverInitializationData.DxgkDdiStartDevice = DxgkDdiStartDevice;
    DriverInitializationData.DxgkDdiStopDevice = DxgkDdiStopDevice;
    DriverInitializationData.DxgkDdiRemoveDevice = DxgkDdiRemoveDevice;
    DriverInitializationData.DxgkDdiDispatchIoRequest = DxgkDdiDispatchIoRequest;
    DriverInitializationData.DxgkDdiInterruptRoutine = DxgkDdiInterruptRoutine;
    DriverInitializationData.DxgkDdiDpcRoutine = DxgkDdiDpcRoutine;
    DriverInitializationData.DxgkDdiQueryChildRelations = DxgkDdiQueryChildRelations;
    DriverInitializationData.DxgkDdiQueryChildStatus = DxgkDdiQueryChildStatus;
    DriverInitializationData.DxgkDdiQueryDeviceDescriptor = DxgkDdiQueryDeviceDescriptor;
    DriverInitializationData.DxgkDdiSetPowerState = DxgkDdiSetPowerState;
    DriverInitializationData.DxgkDdiNotifyAcpiEvent = DxgkDdiNotifyAcpiEvent;
    DriverInitializationData.DxgkDdiResetDevice = DxgkDdiResetDevice;
    DriverInitializationData.DxgkDdiUnload = DxgkDdiUnload;
    DriverInitializationData.DxgkDdiQueryInterface = DxgkDdiQueryInterface;
    DriverInitializationData.DxgkDdiControlEtwLogging = DxgkDdiControlEtwLogging;

    DriverInitializationData.DxgkDdiQueryAdapterInfo = DxgkDdiQueryAdapterInfo;
    DriverInitializationData.DxgkDdiCreateDevice = DxgkDdiCreateDevice;
    DriverInitializationData.DxgkDdiCreateAllocation = DxgkDdiCreateAllocation;
    DriverInitializationData.DxgkDdiDestroyAllocation = DxgkDdiDestroyAllocation;
    DriverInitializationData.DxgkDdiDescribeAllocation = DxgkDdiDescribeAllocation;
    DriverInitializationData.DxgkDdiGetStandardAllocationDriverData = DxgkDdiGetStandardAllocationDriverData;
    DriverInitializationData.DxgkDdiAcquireSwizzlingRange = DxgkDdiAcquireSwizzlingRange;
    DriverInitializationData.DxgkDdiReleaseSwizzlingRange = DxgkDdiReleaseSwizzlingRange;
    DriverInitializationData.DxgkDdiPatch = DxgkDdiPatch;
    DriverInitializationData.DxgkDdiSubmitCommand = DxgkDdiSubmitCommand;
    DriverInitializationData.DxgkDdiPreemptCommand = DxgkDdiPreemptCommand;
    DriverInitializationData.DxgkDdiBuildPagingBuffer = DxgkDdiBuildPagingBuffer;
    DriverInitializationData.DxgkDdiSetPalette = DxgkDdiSetPalette;
    DriverInitializationData.DxgkDdiSetPointerPosition = DxgkDdiSetPointerPosition;
    DriverInitializationData.DxgkDdiSetPointerShape = DxgkDdiSetPointerShape;
    DriverInitializationData.DxgkDdiResetFromTimeout = DxgkDdiResetFromTimeout;
    DriverInitializationData.DxgkDdiRestartFromTimeout = DxgkDdiRestartFromTimeout;
    DriverInitializationData.DxgkDdiEscape = DxgkDdiEscape;
    DriverInitializationData.DxgkDdiCollectDbgInfo = DxgkDdiCollectDbgInfo;
    DriverInitializationData.DxgkDdiQueryCurrentFence = DxgkDdiQueryCurrentFence;
    DriverInitializationData.DxgkDdiIsSupportedVidPn = DxgkDdiIsSupportedVidPn;
    DriverInitializationData.DxgkDdiRecommendFunctionalVidPn = DxgkDdiRecommendFunctionalVidPn;
    DriverInitializationData.DxgkDdiEnumVidPnCofuncModality = DxgkDdiEnumVidPnCofuncModality;
    DriverInitializationData.DxgkDdiSetVidPnSourceAddress = DxgkDdiSetVidPnSourceAddress;
    DriverInitializationData.DxgkDdiSetVidPnSourceVisibility = DxgkDdiSetVidPnSourceVisibility;
    DriverInitializationData.DxgkDdiCommitVidPn = DxgkDdiCommitVidPn;
    DriverInitializationData.DxgkDdiUpdateActiveVidPnPresentPath = DxgkDdiUpdateActiveVidPnPresentPath;
    DriverInitializationData.DxgkDdiRecommendMonitorModes = DxgkDdiRecommendMonitorModes;
    DriverInitializationData.DxgkDdiRecommendVidPnTopology = DxgkDdiRecommendVidPnTopology;
    DriverInitializationData.DxgkDdiGetScanLine = DxgkDdiGetScanLine;
    DriverInitializationData.DxgkDdiStopCapture = DxgkDdiStopCapture;
    DriverInitializationData.DxgkDdiControlInterrupt = DxgkDdiControlInterrupt;
    DriverInitializationData.DxgkDdiCreateOverlay = DxgkDdiCreateOverlay;

    DriverInitializationData.DxgkDdiDestroyDevice = DxgkDdiDestroyDevice;
    DriverInitializationData.DxgkDdiOpenAllocation = DxgkDdiOpenAllocation;
    DriverInitializationData.DxgkDdiCloseAllocation = DxgkDdiCloseAllocation;
    DriverInitializationData.DxgkDdiRender = DxgkDdiRender;
    DriverInitializationData.DxgkDdiPresent = DxgkDdiPresent;

    DriverInitializationData.DxgkDdiUpdateOverlay = DxgkDdiUpdateOverlay;
    DriverInitializationData.DxgkDdiFlipOverlay = DxgkDdiFlipOverlay;
    DriverInitializationData.DxgkDdiDestroyOverlay = DxgkDdiDestroyOverlay;

    DriverInitializationData.DxgkDdiCreateContext = DxgkDdiCreateContext;
    DriverInitializationData.DxgkDdiDestroyContext = DxgkDdiDestroyContext;

    DriverInitializationData.DxgkDdiLinkDevice = NULL; //DxgkDdiLinkDevice;
    DriverInitializationData.DxgkDdiSetDisplayPrivateDriverFormat = DxgkDdiSetDisplayPrivateDriverFormat;
//#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN7)
//# error port to Win7 DDI
//    DriverInitializationData.DxgkDdiRenderKm  = DxgkDdiRenderKm;
//    DriverInitializationData.DxgkDdiRestartFromTimeout  = DxgkDdiRestartFromTimeout;
//    DriverInitializationData.DxgkDdiSetVidPnSourceVisibility  = DxgkDdiSetVidPnSourceVisibility;
//    DriverInitializationData.DxgkDdiUpdateActiveVidPnPresentPath  = DxgkDdiUpdateActiveVidPnPresentPath;
//    DriverInitializationData.DxgkDdiQueryVidPnHWCapability  = DxgkDdiQueryVidPnHWCapability;
//#endif

    return DxgkInitialize(DriverObject,
                          RegistryPath,
                          &DriverInitializationData);
}
