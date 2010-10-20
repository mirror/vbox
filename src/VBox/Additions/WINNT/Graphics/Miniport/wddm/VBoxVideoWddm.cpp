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

#include <iprt/asm.h>

#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxVideo.h>
#include <wingdi.h> /* needed for RGNDATA definition */
#include <VBoxDisplay.h> /* this is from Additions/WINNT/include/ to include escape codes */

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

DECLINLINE(PVBOXWDDM_ALLOCATION) vboxWddmGetAllocationFromOpenData(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_OPENALLOCATION pOa)
{
    DXGKARGCB_GETHANDLEDATA GhData;
    GhData.hObject = pOa->hAllocation;
    GhData.Type = DXGK_HANDLE_ALLOCATION;
    GhData.Flags.Value = 0;
    return (PVBOXWDDM_ALLOCATION)pDevExt->u.primary.DxgkInterface.DxgkCbGetHandleData(&GhData);
}

DECLINLINE(PVBOXWDDM_ALLOCATION) vboxWddmGetAllocationFromAllocList(PDEVICE_EXTENSION pDevExt, DXGK_ALLOCATIONLIST *pAllocList)
{
    return vboxWddmGetAllocationFromOpenData(pDevExt, (PVBOXWDDM_OPENALLOCATION)pAllocList->hDeviceSpecificAllocation);
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

//VBOXVIDEOOFFSET vboxWddmVRAMAddressToOffset(PDEVICE_EXTENSION pDevExt, PHYSICAL_ADDRESS phAddress)
//{
//    Assert(phAddress.QuadPart >= VBE_DISPI_LFB_PHYSICAL_ADDRESS);
//    if (phAddress.QuadPart < VBE_DISPI_LFB_PHYSICAL_ADDRESS)
//        return VBOXVIDEOOFFSET_VOID;
//
//    VBOXVIDEOOFFSET off = phAddress.QuadPart - VBE_DISPI_LFB_PHYSICAL_ADDRESS;
//    Assert(off < pDevExt->u.primary.cbVRAM);
//    if (off >= pDevExt->u.primary.cbVRAM)
//        return VBOXVIDEOOFFSET_VOID;
//
//    return off;
//}

VBOXVIDEOOFFSET vboxWddmValidatePrimary(PVBOXWDDM_ALLOCATION pAllocation)
{
    Assert(pAllocation);
    if (!pAllocation)
    {
        drprintf((__FUNCTION__": no allocation specified for Source\n"));
        return VBOXVIDEOOFFSET_VOID;
    }

    Assert(pAllocation->SegmentId);
    if (!pAllocation->SegmentId)
    {
        drprintf((__FUNCTION__": allocation is not paged in\n"));
        return VBOXVIDEOOFFSET_VOID;
    }

    VBOXVIDEOOFFSET offVram = pAllocation->offVram;
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        drprintf((__FUNCTION__": VRAM pffset is not defined\n"));

    return offVram;
}

NTSTATUS vboxWddmGhDisplayPostInfoScreenBySDesc (PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SURFACE_DESC pDesc, POINT * pVScreenPos, uint16_t fFlags)
{
    void *p = vboxHGSMIBufferAlloc (commonFromDeviceExt(pDevExt),
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

        vboxHGSMIBufferSubmit (commonFromDeviceExt(pDevExt), p);

        vboxHGSMIBufferFree (commonFromDeviceExt(pDevExt), p);
    }

    return STATUS_SUCCESS;

}

NTSTATUS vboxWddmGhDisplayPostInfoScreen (PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation, POINT * pVScreenPos)
{
    NTSTATUS Status = vboxWddmGhDisplayPostInfoScreenBySDesc(pDevExt, &pAllocation->SurfDesc, pVScreenPos, VBVA_SCREEN_F_ACTIVE);
    Assert(Status == STATUS_SUCCESS);
    return Status;
}

NTSTATUS vboxWddmGhDisplayPostInfoView(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXVIDEOOFFSET offVram = pAllocation->offVram;
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    /* Issue the screen info command. */
    void *p = vboxHGSMIBufferAlloc (commonFromDeviceExt(pDevExt),
                                      sizeof (VBVAINFOVIEW),
                                      HGSMI_CH_VBVA,
                                      VBVA_INFO_VIEW);
    Assert(p);
    if (p)
    {
        VBVAINFOVIEW *pView = (VBVAINFOVIEW *)p;

        pView->u32ViewIndex     = pAllocation->SurfDesc.VidPnSourceId;
        pView->u32ViewOffset    = (uint32_t)offVram; /* we pretend the view is located at the start of each framebuffer */
        pView->u32ViewSize      = vboxWddmVramCpuVisibleSegmentSize(pDevExt)/commonFromDeviceExt(pDevExt)->cDisplays;

        pView->u32MaxScreenSize = pView->u32ViewSize;

        vboxHGSMIBufferSubmit (commonFromDeviceExt(pDevExt), p);

        vboxHGSMIBufferFree (commonFromDeviceExt(pDevExt), p);
    }

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmGhDisplaySetMode(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
//    PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimaryInfo = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
    if (/*pPrimaryInfo->*/pAllocation->SurfDesc.VidPnSourceId)
        return STATUS_SUCCESS;

    if (VBoxVideoSetCurrentModePerform(pDevExt, pAllocation->SurfDesc.width,
            pAllocation->SurfDesc.height, pAllocation->SurfDesc.bpp,
            (ULONG)pAllocation->offVram))
        return STATUS_SUCCESS;

    AssertBreakpoint();
    drprintf((__FUNCTION__": VBoxVideoSetCurrentModePerform failed\n"));
    return STATUS_UNSUCCESSFUL;
}

NTSTATUS vboxWddmGhDisplayUpdateScreenPos(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SOURCE pSource, POINT *pVScreenPos)
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

NTSTATUS vboxWddmGhDisplaySetInfo(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SOURCE pSource, D3DDDI_VIDEO_PRESENT_SOURCE_ID VidPnSourceId)
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
                drprintf((__FUNCTION__": vboxWddmGhDisplayPostInfoScreen failed\n"));
        }
        else
            drprintf((__FUNCTION__": vboxWddmGhDisplayPostInfoView failed\n"));
    }
    else
        drprintf((__FUNCTION__": vboxWddmGhDisplaySetMode failed\n"));

    return Status;
}

#ifdef VBOXWDDM_RENDER_FROM_SHADOW
bool vboxWddmCheckUpdateShadowAddress(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SOURCE pSource,
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
        drprintf((__FUNCTION__": vboxWddmGhDisplaySetInfo failed, Status (0x%x)\n", Status));

    return true;
}
#endif

HGSMIHEAP* vboxWddmHgsmiGetHeapFromCmdOffset(PDEVICE_EXTENSION pDevExt, HGSMIOFFSET offCmd)
{
#ifdef VBOX_WITH_VDMA
    if(HGSMIAreaContainsOffset(&pDevExt->u.primary.Vdma.CmdHeap.area, offCmd))
        return &pDevExt->u.primary.Vdma.CmdHeap;
#endif
    if (HGSMIAreaContainsOffset(&commonFromDeviceExt(pDevExt)->hgsmiAdapterHeap.area, offCmd))
        return &commonFromDeviceExt(pDevExt)->hgsmiAdapterHeap;
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

VBOXWDDM_HGSMICMD_TYPE vboxWddmHgsmiGetCmdTypeFromOffset(PDEVICE_EXTENSION pDevExt, HGSMIOFFSET offCmd)
{
#ifdef VBOX_WITH_VDMA
    if(HGSMIAreaContainsOffset(&pDevExt->u.primary.Vdma.CmdHeap.area, offCmd))
        return VBOXWDDM_HGSMICMD_TYPE_DMACMD;
#endif
    if (HGSMIAreaContainsOffset(&commonFromDeviceExt(pDevExt)->hgsmiAdapterHeap.area, offCmd))
        return VBOXWDDM_HGSMICMD_TYPE_CTL;
    return VBOXWDDM_HGSMICMD_TYPE_UNDEFINED;
}


VP_STATUS VBoxVideoCmnRegQueryDword(IN VBOXCMNREG Reg, PWSTR pName, uint32_t *pVal)
{
    if(!Reg)
        return ERROR_INVALID_PARAMETER;
    NTSTATUS Status = vboxWddmRegQueryValueDword(Reg, pName, (PDWORD)pVal);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

VP_STATUS VBoxVideoCmnRegSetDword(IN VBOXCMNREG Reg, PWSTR pName, uint32_t Val)
{
    if(!Reg)
        return ERROR_INVALID_PARAMETER;
    NTSTATUS Status = vboxWddmRegSetValueDword(Reg, pName, Val);
    return Status == STATUS_SUCCESS ? NO_ERROR : ERROR_INVALID_PARAMETER;
}

VP_STATUS VBoxVideoCmnRegInit(IN PDEVICE_EXTENSION pDeviceExtension, OUT VBOXCMNREG *pReg)
{
    WCHAR Buf[512];
    ULONG cbBuf = sizeof(Buf);
    NTSTATUS Status = vboxWddmRegQueryDrvKeyName(pDeviceExtension, cbBuf, Buf, &cbBuf);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmRegOpenKey(pReg, Buf, GENERIC_READ | GENERIC_WRITE);
        Assert(Status == STATUS_SUCCESS);
        if(Status == STATUS_SUCCESS)
            return NO_ERROR;
    }

    /* fall-back to make the subsequent VBoxVideoCmnRegXxx calls treat the fail accordingly
     * basically needed to make as less modifications to the current XPDM code as possible */
    *pReg = NULL;

    return ERROR_INVALID_PARAMETER;
}

D3DDDIFORMAT vboxWddmCalcPixelFormat(VIDEO_MODE_INFORMATION *pInfo)
{
    switch (pInfo->BitsPerPlane)
    {
        case 32:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_A8R8G8B8;
                drprintf((__FUNCTION__": unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)\n", pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                drprintf((__FUNCTION__": unsupported AttributeFlags(0x%x)\n", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 24:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_R8G8B8;
                drprintf((__FUNCTION__": unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)\n", pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                drprintf((__FUNCTION__": unsupported AttributeFlags(0x%x)\n", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        case 16:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xF800 && pInfo->GreenMask == 0x7E0 && pInfo->BlueMask == 0x1F)
                    return D3DDDIFMT_R5G6B5;
                drprintf((__FUNCTION__": unsupported format: bpp(%d), rmask(%d), gmask(%d), bmask(%d)\n", pInfo->BitsPerPlane, pInfo->RedMask, pInfo->GreenMask, pInfo->BlueMask));
                AssertBreakpoint();
            }
            else
            {
                drprintf((__FUNCTION__": unsupported AttributeFlags(0x%x)\n", pInfo->AttributeFlags));
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
                drprintf((__FUNCTION__": unsupported AttributeFlags(0x%x)\n", pInfo->AttributeFlags));
                AssertBreakpoint();
            }
            break;
        default:
            drprintf((__FUNCTION__": unsupported bpp(%d)\n", pInfo->BitsPerPlane));
            AssertBreakpoint();
            break;
    }

    return D3DDDIFMT_UNKNOWN;
}

NTSTATUS vboxWddmPickResources(PDEVICE_EXTENSION pContext, PDXGK_DEVICE_INFO pDeviceInfo, PULONG pAdapterMemorySize)
{
    NTSTATUS Status = STATUS_SUCCESS;
    USHORT DispiId;
    *pAdapterMemorySize = VBE_DISPI_TOTAL_VIDEO_MEMORY_BYTES;

    VBoxVideoCmnPortWriteUshort((PUSHORT)VBE_DISPI_IOPORT_INDEX, VBE_DISPI_INDEX_ID);
    VBoxVideoCmnPortWriteUshort((PUSHORT)VBE_DISPI_IOPORT_DATA, VBE_DISPI_ID2);
    DispiId = VBoxVideoCmnPortReadUshort((PUSHORT)VBE_DISPI_IOPORT_DATA);
    if (DispiId == VBE_DISPI_ID2)
    {
       dprintf(("VBoxVideoWddm: found the VBE card\n"));
       /*
        * Write some hardware information to registry, so that
        * it's visible in Windows property dialog.
        */

       /*
        * Query the adapter's memory size. It's a bit of a hack, we just read
        * an ULONG from the data port without setting an index before.
        */
       *pAdapterMemorySize = VBoxVideoCmnPortReadUlong((PULONG)VBE_DISPI_IOPORT_DATA);
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
           drprintf(("VBoxVideoWddm: HGSMI unsupported, returning err\n"));
           /* @todo: report a better status */
           Status = STATUS_UNSUCCESSFUL;
       }
    }
    else
    {
        drprintf(("VBoxVideoWddm:: VBE card not found, returning err\n"));
        Status = STATUS_UNSUCCESSFUL;
    }


    return Status;
}

static void vboxWddmDevExtZeroinit(PDEVICE_EXTENSION pDevExt, CONST PDEVICE_OBJECT pPDO)
{
    memset(pDevExt, 0, sizeof (DEVICE_EXTENSION));
    pDevExt->pPDO = pPDO;
    PWCHAR pName = (PWCHAR)(((uint8_t*)pDevExt) + VBOXWDDM_ROUNDBOUND(sizeof (DEVICE_EXTENSION), 8));
    RtlInitUnicodeString(&pDevExt->RegKeyName, pName);
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
    for (int i = 0; i < RT_ELEMENTS(pDevExt->aSources); ++i)
    {
        pDevExt->aSources[i].offVram = VBOXVIDEOOFFSET_VOID;
    }
#endif
}

/* driver callbacks */
NTSTATUS DxgkDdiAddDevice(
    IN CONST PDEVICE_OBJECT PhysicalDeviceObject,
    OUT PVOID *MiniportDeviceContext
    )
{
    /* The DxgkDdiAddDevice function should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", pdo(0x%x)\n", PhysicalDeviceObject));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pDevExt;

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
        pDevExt = (PDEVICE_EXTENSION)vboxWddmMemAllocZero(VBOXWDDM_ROUNDBOUND(sizeof (DEVICE_EXTENSION), 8) + cbRegKeyBuf);
        if (pDevExt)
        {
            PWCHAR pName = (PWCHAR)(((uint8_t*)pDevExt) + VBOXWDDM_ROUNDBOUND(sizeof (DEVICE_EXTENSION), 8));
            memcpy(pName, RegKeyBuf, cbRegKeyBuf);
            vboxWddmDevExtZeroinit(pDevExt, PhysicalDeviceObject);
            *MiniportDeviceContext = pDevExt;
        }
        else
        {
            Status  = STATUS_NO_MEMORY;
            drprintf(("VBoxVideoWddm: ERROR, failed to create context\n"));
        }
    }

    dfprintf(("<== "__FUNCTION__ ", Status(0x%x), pDevExt(0x%x)\n", Status, pDevExt));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

    vboxVDbgBreakFv();

    if ( ARGUMENT_PRESENT(MiniportDeviceContext) &&
        ARGUMENT_PRESENT(DxgkInterface) &&
        ARGUMENT_PRESENT(DxgkStartInfo) &&
        ARGUMENT_PRESENT(NumberOfVideoPresentSources),
        ARGUMENT_PRESENT(NumberOfChildren)
        )
    {
        PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)MiniportDeviceContext;

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

                /* Guest supports only HGSMI, the old VBVA via VMMDev is not supported. Old
                 * code will be ifdef'ed and later removed.
                 * The host will however support both old and new interface to keep compatibility
                 * with old guest additions.
                 */
                VBoxSetupDisplaysHGSMI(pContext, AdapterMemorySize,
                                       VBVACAPS_COMPLETEGCMD_BY_IOREAD | VBVACAPS_IRQ);
                if (commonFromDeviceExt(pContext)->bHGSMI)
                {
                    drprintf(("VBoxVideoWddm: using HGSMI\n"));
                    *NumberOfVideoPresentSources = commonFromDeviceExt(pContext)->cDisplays;
                    *NumberOfChildren = commonFromDeviceExt(pContext)->cDisplays;
                    dprintf(("VBoxVideoWddm: sources(%d), children(%d)\n", *NumberOfVideoPresentSources, *NumberOfChildren));

                    vboxVdmaDdiQueueInit(pContext, &pContext->DdiCmdQueue);
                    vboxVideoCmInit(&pContext->CmMgr);
                    InitializeListHead(&pContext->SwapchainList3D);
                    pContext->cContexts3D = 0;
                    ExInitializeFastMutex(&pContext->ContextMutex);
                    KeInitializeSpinLock(&pContext->SynchLock);

#ifdef VBOX_WITH_VIDEOHWACCEL
                    vboxVhwaInit(pContext);
#endif
                }
                else
                {
                    drprintf(("VBoxVideoWddm: HGSMI failed to initialize, returning err\n"));

                    VbglTerminate();
                    /* @todo: report a better status */
                    Status = STATUS_UNSUCCESSFUL;
                }
            }
            else
            {
                drprintf(("VBoxVideoWddm:: vboxWddmPickResources failed Status(0x%x), returning err\n", Status));
                Status = STATUS_UNSUCCESSFUL;
            }
        }
        else
        {
            drprintf(("VBoxVideoWddm: DxgkCbGetDeviceInformation failed Status(0x%x), returning err\n", Status));
        }
    }
    else
    {
        drprintf(("VBoxVideoWddm: invalid parameter, returning err\n"));
        Status = STATUS_INVALID_PARAMETER;
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x)\n", Status));

    return Status;
}

NTSTATUS DxgkDdiStopDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* The DxgkDdiStopDevice function should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;
    NTSTATUS Status = STATUS_SUCCESS;

    vboxVideoCmTerm(&pDevExt->CmMgr);

    /* do everything we did on DxgkDdiStartDevice in the reverse order */
#ifdef VBOX_WITH_VIDEOHWACCEL
    vboxVhwaFree(pDevExt);
#endif

    int rc = VBoxFreeDisplaysHGSMI(pDevExt);
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

    dfprintf(("==> "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    vboxVDbgBreakFv();

    vboxWddmMemFree(MiniportDeviceContext);

    dfprintf(("<== "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    return STATUS_SUCCESS;
}

NTSTATUS DxgkDdiDispatchIoRequest(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG VidPnSourceId,
    IN PVIDEO_REQUEST_PACKET VideoRequestPacket
    )
{
    dfprintf(("==> "__FUNCTION__ ", context(0x%p), ctl(0x%x)\n", MiniportDeviceContext, VideoRequestPacket->IoControlCode));

    AssertBreakpoint();
#if 0
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;

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

            Result = VBoxVideoResetDevice((PDEVICE_EXTENSION)HwDeviceExtension,
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
    dfprintf(("<== "__FUNCTION__ ", context(0x%p), ctl(0x%x)\n", MiniportDeviceContext, VideoRequestPacket->IoControlCode));

    return STATUS_SUCCESS;
}

BOOLEAN DxgkDdiInterruptRoutine(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG MessageNumber
    )
{
//    dfprintf(("==> "__FUNCTION__ ", context(0x%p), msg(0x%x)\n", MiniportDeviceContext, MessageNumber));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;
    BOOLEAN bOur = FALSE;
    BOOLEAN bNeedDpc = FALSE;
    if (commonFromDeviceExt(pDevExt)->pHostFlags) /* If HGSMI is enabled at all. */
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

        uint32_t flags = commonFromDeviceExt(pDevExt)->pHostFlags->u32HostFlags;
        bOur = (flags & HGSMIHOSTFLAGS_IRQ);
        do
        {
            if (flags & HGSMIHOSTFLAGS_GCOMMAND_COMPLETED)
            {
                /* read the command offset */
                HGSMIOFFSET offCmd = VBoxHGSMIGuestRead(commonFromDeviceExt(pDevExt));
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
                            pHeap = &commonFromDeviceExt(pDevExt)->hgsmiAdapterHeap;
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

            flags = commonFromDeviceExt(pDevExt)->pHostFlags->u32HostFlags;
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

        if (pDevExt->bNotifyDxDpc)
        {
//            Assert(bNeedDpc == TRUE);
//            pDevExt->bNotifyDxDpc = TRUE;
//            pDevExt->bSetNotifyDxDpc = FALSE;
            bNeedDpc = TRUE;
        }

        if (bOur)
        {
            HGSMIClearIrq (commonFromDeviceExt(pDevExt));
#ifdef DEBUG_misha
            /* this is not entirely correct since host may concurrently complete some commands and raise a new IRQ while we are here,
             * still this allows to check that the host flags are correctly cleared after the ISR */
            Assert(commonFromDeviceExt(pDevExt)->pHostFlags);
            uint32_t flags = commonFromDeviceExt(pDevExt)->pHostFlags->u32HostFlags;
            Assert(flags == 0);
#endif
        }

        if (bNeedDpc)
        {
            BOOLEAN bDpcQueued = pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
            Assert(bDpcQueued);
        }
    }

//    dfprintf(("<== "__FUNCTION__ ", context(0x%p), bOur(0x%x)\n", MiniportDeviceContext, (ULONG)bOur));

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
    PDEVICE_EXTENSION pDevExt;
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
    vboxVdmaDdiCmdGetCompletedListIsr(&pdc->pDevExt->DdiCmdQueue, &pdc->data.CompletedDdiCmdQueue);

    pdc->data.bNotifyDpc = pdc->pDevExt->bNotifyDxDpc;
    pdc->pDevExt->bNotifyDxDpc = FALSE;
    return TRUE;
}

VOID DxgkDdiDpcRoutine(
    IN CONST PVOID  MiniportDeviceContext
    )
{
//    dfprintf(("==> "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;

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

    if (!vboxSHGSMIListIsEmpty(&context.data.CtlList))
    {
        int rc = VBoxSHGSMICommandPostprocessCompletion (&commonFromDeviceExt(pDevExt)->hgsmiAdapterHeap, &context.data.CtlList);
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

    vboxVdmaDdiCmdHandleCompletedList(pDevExt, &pDevExt->DdiCmdQueue, &context.data.CompletedDdiCmdQueue);

    if (context.data.bNotifyDpc)
        pDevExt->u.primary.DxgkInterface.DxgkCbNotifyDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

//    dfprintf(("<== "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));
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

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));
    Assert(ChildRelationsSize == (commonFromDeviceExt(pDevExt)->cDisplays + 1)*sizeof(DXGK_CHILD_DESCRIPTOR));
    for (int i = 0; i < commonFromDeviceExt(pDevExt)->cDisplays; ++i)
    {
        ChildRelations[i].ChildDeviceType = TypeVideoOutput;
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.InterfaceTechnology = D3DKMDT_VOT_HD15; /* VGA */
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_INTERRUPTIBLE; /* ?? D3DKMDT_MOA_NONE*/
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
        ChildRelations[i].ChildCapabilities.HpdAwareness = HpdAwarenessInterruptible; /* ?? HpdAwarenessAlwaysConnected; */
        ChildRelations[i].AcpiUid =  i; /* */
        ChildRelations[i].ChildUid = i; /* should be == target id */
    }
    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));
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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

    NTSTATUS Status = STATUS_SUCCESS;
    switch (ChildStatus->Type)
    {
        case StatusConnection:
            ChildStatus->HotPlug.Connected = TRUE;
            dfprintf(("VBoxVideoWddm: StatusConnection\n"));
            break;
        case StatusRotation:
            ChildStatus->Rotation.Angle = 0;
            dfprintf(("VBoxVideoWddm: StatusRotation\n"));
            break;
        default:
            drprintf(("VBoxVideoWddm: ERROR: status type: %d\n", ChildStatus->Type));
            AssertBreakpoint();
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

    /* @todo: */
//    vboxVDbgBreakF();

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

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
    dfprintf(("==> "__FUNCTION__ ", MiniportDeviceContext(0x%x)\n", MiniportDeviceContext));

    vboxVDbgBreakF();

    dfprintf(("<== "__FUNCTION__ ", MiniportDeviceContext(0x%x)\n", MiniportDeviceContext));

    return STATUS_SUCCESS;
}

VOID DxgkDdiResetDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* DxgkDdiResetDevice can be called at any IRQL, so it must be in nonpageable memory.  */
    vboxVDbgBreakF();



    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));
    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));
}

VOID DxgkDdiUnload(
    VOID
    )
{
    /* DxgkDdiUnload should be made pageable. */
    PAGED_CODE();
    dfprintf(("==> "__FUNCTION__ "\n"));

    vboxVDbgBreakFv();

    dfprintf(("<== "__FUNCTION__ "\n"));
}

NTSTATUS DxgkDdiQueryInterface(
    IN CONST PVOID MiniportDeviceContext,
    IN PQUERY_INTERFACE QueryInterface
    )
{
    dfprintf(("==> "__FUNCTION__ ", MiniportDeviceContext(0x%x)\n", MiniportDeviceContext));

    vboxVDbgBreakFv();

    dfprintf(("<== "__FUNCTION__ ", MiniportDeviceContext(0x%x)\n", MiniportDeviceContext));

    return STATUS_NOT_SUPPORTED;
}

VOID DxgkDdiControlEtwLogging(
    IN BOOLEAN  Enable,
    IN ULONG  Flags,
    IN UCHAR  Level
    )
{
    dfprintf(("==> "__FUNCTION__ "\n"));

    vboxVDbgBreakF();

    dfprintf(("<== "__FUNCTION__ "\n"));
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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x), Query type (%d)\n", hAdapter, pQueryAdapterInfo->Type));
    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;

    vboxVDbgBreakFv();

    switch (pQueryAdapterInfo->Type)
    {
        case DXGKQAITYPE_DRIVERCAPS:
        {
            DXGK_DRIVERCAPS *pCaps = (DXGK_DRIVERCAPS*)pQueryAdapterInfo->pOutputData;

            pCaps->HighestAcceptableAddress.HighPart = 0x0;
            pCaps->HighestAcceptableAddress.LowPart = 0xffffffffUL;
            pCaps->MaxAllocationListSlotId = 16;
            pCaps->ApertureSegmentCommitLimit = 0;
            pCaps->MaxPointerWidth  = VBOXWDDM_C_POINTER_MAX_WIDTH;
            pCaps->MaxPointerHeight = VBOXWDDM_C_POINTER_MAX_HEIGHT;
            pCaps->PointerCaps.Value = 3; /* Monochrome , Color*/ /* MaskedColor == Value | 4, dosable for now */
            pCaps->InterruptMessageNumber = 0;
            pCaps->NumberOfSwizzlingRanges = 0;
            pCaps->MaxOverlays = 0;
#ifdef VBOX_WITH_VIDEOHWACCEL
            for (int i = 0; i < commonFromDeviceExt(pContext)->cDisplays; ++i)
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
            /* @todo: this corelates with pCaps->SchedulingCaps.MultiEngineAware */
            pCaps->MemoryManagementCaps.PagingNode = 0;
            /* @todo: this corelates with pCaps->SchedulingCaps.MultiEngineAware */
            pCaps->GpuEngineTopology.NbAsymetricProcessingNodes = 1;

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
                drprintf((__FUNCTION__ " NbSegment (%d) != 1\n", pQsOut->NbSegment));
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
                pQi->cInfos = commonFromDeviceExt(pContext)->cDisplays;
#ifdef VBOX_WITH_VIDEOHWACCEL
                for (int i = 0; i < commonFromDeviceExt(pContext)->cDisplays; ++i)
                {
                    pQi->aInfos[i] = pContext->aSources[i].Vhwa.Settings;
                }
#endif
            }
            else
            {
                drprintf((__FUNCTION__ ": buffer too small\n"));
                Status = STATUS_BUFFER_TOO_SMALL;
            }
            break;
        default:
            drprintf((__FUNCTION__ ": unsupported Type (%d)\n", pQueryAdapterInfo->Type));
            AssertBreakpoint();
            Status = STATUS_NOT_SUPPORTED;
            break;
    }
    dfprintf(("<== "__FUNCTION__ ", context(0x%x), Status(0x%x)\n", hAdapter, Status));
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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));
    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;

    vboxVDbgBreakFv();

    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)vboxWddmMemAllocZero(sizeof (VBOXWDDM_DEVICE));
    pCreateDevice->hDevice = pDevice;
    if (pCreateDevice->Flags.SystemDevice)
        pDevice->enmType = VBOXWDDM_DEVICE_TYPE_SYSTEM;
//    else
//    {
//        AssertBreakpoint(); /* we do not support custom contexts for now */
//        drprintf((__FUNCTION__ ": we do not support custom devices for now, hAdapter (0x%x)\n", hAdapter));
//    }

    pDevice->pAdapter = pContext;
    pDevice->hDevice = pCreateDevice->hDevice;

    pCreateDevice->hDevice = pDevice;
    pCreateDevice->pInfo = NULL;

    dfprintf(("<== "__FUNCTION__ ", context(0x%x), Status(0x%x)\n", hAdapter, Status));

    return Status;
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

void vboxWddmAllocationDeleteFromResource(PVBOXWDDM_RESOURCE pResource, PVBOXWDDM_ALLOCATION pAllocation)
{
    Assert(pAllocation->pResource == pResource);
    if (pResource)
    {
        Assert(&pResource->aAllocations[pAllocation->iIndex] == pAllocation);
    }
    else
    {
        vboxWddmMemFree(pAllocation);
    }
}

NTSTATUS vboxWddmDestroyAllocation(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    PAGED_CODE();

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
//#ifdef VBOX_WITH_VIDEOHWACCEL
//        case VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC:
//        {
//            if (pAllocation->fRcFlags.Overlay)
//            {
//                vboxVhwaHlpDestroyOverlay(pDevExt, pAllocation);
//            }
//            break;
//        }
//#endif
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

    vboxWddmAllocationDeleteFromResource(pAllocation->pResource, pAllocation);

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmCreateAllocation(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_RESOURCE pResource, uint32_t iIndex, DXGK_ALLOCATIONINFO* pAllocationInfo)
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
            pAllocation->bVisible = FALSE;
            pAllocation->bAssigned = FALSE;

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
                            Status = ObReferenceObjectByHandle(pAllocInfo->hSynch, EVENT_MODIFY_STATE, *ExEventObjectType, UserMode,
                                    (PVOID*)&pAllocation->pSynchEvent,
                                    NULL);
                            Assert(Status == STATUS_SUCCESS);
                            break;
                        case VBOXUHGSMI_SYNCHOBJECT_TYPE_SEMAPHORE:
                            Status = ObReferenceObjectByHandle(pAllocInfo->hSynch, EVENT_MODIFY_STATE, *ExSemaphoreObjectType, UserMode,
                                    (PVOID*)&pAllocation->pSynchSemaphore,
                                    NULL);
                            Assert(Status == STATUS_SUCCESS);
                            break;
                        case VBOXUHGSMI_SYNCHOBJECT_TYPE_NONE:
                            pAllocation->pSynchEvent = NULL;
                            Status = STATUS_SUCCESS;
                            break;
                        default:
                            drprintf((__FUNCTION__ ": ERROR: invalid synch info type(%d)\n", pAllocInfo->enmSynchType));
                            AssertBreakpoint();
                            Status = STATUS_INVALID_PARAMETER;
                            break;
                    }
                    break;
                }

                default:
                    drprintf((__FUNCTION__ ": ERROR: invalid alloc info type(%d)\n", pAllocInfo->enmType));
                    AssertBreakpoint();
                    Status = STATUS_INVALID_PARAMETER;
                    break;

            }

            if (Status != STATUS_SUCCESS)
                vboxWddmAllocationDeleteFromResource(pResource, pAllocation);
        }
        else
        {
            drprintf((__FUNCTION__ ": ERROR: failed to create allocation description\n"));
            Status = STATUS_NO_MEMORY;
        }

    }
    else
    {
        drprintf((__FUNCTION__ ": ERROR: PrivateDriverDataSize(%d) less than header size(%d)\n", pAllocationInfo->PrivateDriverDataSize, sizeof (VBOXWDDM_ALLOCINFO)));
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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_RESOURCE pResource = NULL;

    if (pCreateAllocation->PrivateDriverDataSize)
    {
        Assert(pCreateAllocation->PrivateDriverDataSize == sizeof (VBOXWDDM_RCINFO));
        Assert(pCreateAllocation->pPrivateDriverData);
        if (pCreateAllocation->PrivateDriverDataSize >= sizeof (VBOXWDDM_RCINFO))
        {
            PVBOXWDDM_RCINFO pRcInfo = (PVBOXWDDM_RCINFO)pCreateAllocation->pPrivateDriverData;
//            Assert(pRcInfo->RcDesc.VidPnSourceId < commonFromDeviceExt(pDevExt)->cDisplays);
            Assert(pRcInfo->cAllocInfos == pCreateAllocation->NumAllocations);
            pResource = (PVBOXWDDM_RESOURCE)vboxWddmMemAllocZero(RT_OFFSETOF(VBOXWDDM_RESOURCE, aAllocations[pRcInfo->cAllocInfos]));
            Assert(pResource);
            if (pResource)
            {
                pResource->cAllocations = pRcInfo->cAllocInfos;
                pResource->fFlags = pRcInfo->fFlags;
                pResource->RcDesc = pRcInfo->RcDesc;
            }
            else
                Status = STATUS_NO_MEMORY;
        }
        else
            Status = STATUS_INVALID_PARAMETER;
        /* @todo: Implement Resource Data Handling */
        drprintf((__FUNCTION__ ": WARNING: Implement Resource Data Handling\n"));
    }

    if (Status == STATUS_SUCCESS)
    {
        for (UINT i = 0; i < pCreateAllocation->NumAllocations; ++i)
        {
            Status = vboxWddmCreateAllocation(pDevExt, pResource, i, &pCreateAllocation->pAllocationInfo[i]);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
            {
                drprintf((__FUNCTION__ ": ERROR: vboxWddmCreateAllocation error (0x%x)\n", Status));
                /* note: i-th allocation is expected to be cleared in a fail handling code above */
                for (UINT j = 0; j < i; ++j)
                {
                    vboxWddmDestroyAllocation(pDevExt, (PVBOXWDDM_ALLOCATION)pCreateAllocation->pAllocationInfo[j].hAllocation);
                }
            }
        }

        pCreateAllocation->hResource = pResource;
        if (pResource && Status != STATUS_SUCCESS)
            vboxWddmMemFree(pResource);
    }
    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXWDDM_RESOURCE pRc = (PVBOXWDDM_RESOURCE)pDestroyAllocation->hResource;

    if (pRc)
    {
        Assert(pRc->cAllocations == pDestroyAllocation->NumAllocations);
    }

    for (UINT i = 0; i < pDestroyAllocation->NumAllocations; ++i)
    {
        PVBOXWDDM_ALLOCATION pAlloc = (PVBOXWDDM_ALLOCATION)pDestroyAllocation->pAllocationList[i];
        Assert(pAlloc->pResource == pRc);
        vboxWddmDestroyAllocation((PDEVICE_EXTENSION)hAdapter, pAlloc);
    }

    if (pRc)
    {
        vboxWddmMemFree(pRc);
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)pDescribeAllocation->hAllocation;
    pDescribeAllocation->Width = pAllocation->SurfDesc.width;
    pDescribeAllocation->Height = pAllocation->SurfDesc.height;
    pDescribeAllocation->Format = pAllocation->SurfDesc.format;
    memset (&pDescribeAllocation->MultisampleMethod, 0, sizeof (pDescribeAllocation->MultisampleMethod));
    pDescribeAllocation->RefreshRate.Numerator = 60000;
    pDescribeAllocation->RefreshRate.Denominator = 1000;
    pDescribeAllocation->PrivateDriverFormatAttribute = 0;

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_ALLOCINFO pAllocInfo = NULL;

    switch (pGetStandardAllocationDriverData->StandardAllocationType)
    {
        case D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE:
        {
            dfprintf((__FUNCTION__ ": D3DKMDT_STANDARDALLOCATION_SHAREDPRIMARYSURFACE\n"));
            if(pGetStandardAllocationDriverData->pAllocationPrivateDriverData)
            {
                pAllocInfo = (PVBOXWDDM_ALLOCINFO)pGetStandardAllocationDriverData->pAllocationPrivateDriverData;
                memset (pAllocInfo, 0, sizeof (VBOXWDDM_ALLOCINFO));
                pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE;
                pAllocInfo->SurfDesc.width = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Width;
                pAllocInfo->SurfDesc.height = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Height;
                pAllocInfo->SurfDesc.format = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Format;
                pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Width, pAllocInfo->SurfDesc.bpp);
                pAllocInfo->SurfDesc.cbSize = pAllocInfo->SurfDesc.pitch * pAllocInfo->SurfDesc.height;
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
            dfprintf((__FUNCTION__ ": D3DKMDT_STANDARDALLOCATION_SHADOWSURFACE\n"));
            UINT bpp = vboxWddmCalcBitsPerPixel(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format);
            Assert(bpp);
            if (bpp != 0)
            {
                UINT Pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width, bpp);
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
                    pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width, pAllocInfo->SurfDesc.bpp);
                    pAllocInfo->SurfDesc.cbSize = pAllocInfo->SurfDesc.pitch * pAllocInfo->SurfDesc.height;
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
                drprintf((__FUNCTION__ ": Invalid format (%d)\n", pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format));
                Status = STATUS_INVALID_PARAMETER;
            }
            break;
        }
        case D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE:
        {
            dfprintf((__FUNCTION__ ": D3DKMDT_STANDARDALLOCATION_STAGINGSURFACE\n"));
            if(pGetStandardAllocationDriverData->pAllocationPrivateDriverData)
            {
                pAllocInfo = (PVBOXWDDM_ALLOCINFO)pGetStandardAllocationDriverData->pAllocationPrivateDriverData;
                pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE;
                pAllocInfo->SurfDesc.width = pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Width;
                pAllocInfo->SurfDesc.height = pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Height;
                pAllocInfo->SurfDesc.format = D3DDDIFMT_X8R8G8B8; /* staging has always always D3DDDIFMT_X8R8G8B8 */
                pAllocInfo->SurfDesc.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->SurfDesc.format);
                pAllocInfo->SurfDesc.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Width, pAllocInfo->SurfDesc.bpp);
                pAllocInfo->SurfDesc.cbSize = pAllocInfo->SurfDesc.pitch * pAllocInfo->SurfDesc.height;
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
            drprintf((__FUNCTION__ ": Invalid allocation type (%d)\n", pGetStandardAllocationDriverData->StandardAllocationType));
            Status = STATUS_INVALID_PARAMETER;
            break;
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiAcquireSwizzlingRange(
    CONST HANDLE  hAdapter,
    DXGKARG_ACQUIRESWIZZLINGRANGE*  pAcquireSwizzlingRange)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiReleaseSwizzlingRange(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RELEASESWIZZLINGRANGE*  pReleaseSwizzlingRange)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    /* Value == 2 is Present
     * Value == 4 is RedirectedPresent
     * we do not expect any other flags to be set here */
//    Assert(pPatch->Flags.Value == 2 || pPatch->Flags.Value == 4);
    Assert(pPatch->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR));
    Assert(pPatch->DmaBufferPrivateDataSubmissionEndOffset - pPatch->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR));
    if (pPatch->DmaBufferPrivateDataSubmissionEndOffset - pPatch->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
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
    else
    {
        drprintf((__FUNCTION__": DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR) (%d)\n",
                pPatch->DmaBufferPrivateDataSubmissionEndOffset,
                pPatch->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    return Status;
}

typedef struct VBOXWDDM_CALL_ISR
{
    PDEVICE_EXTENSION pDevExt;
    ULONG MessageNumber;
} VBOXWDDM_CALL_ISR, *PVBOXWDDM_CALL_ISR;

static BOOLEAN vboxWddmCallIsrCb(PVOID Context)
{
    PVBOXWDDM_CALL_ISR pdc = (PVBOXWDDM_CALL_ISR)Context;
    return DxgkDdiInterruptRoutine(pdc->pDevExt, pdc->MessageNumber);
}

NTSTATUS vboxWddmCallIsr(PDEVICE_EXTENSION pDevExt)
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

static NTSTATUS vboxWddmSubmitCmd(PDEVICE_EXTENSION pDevExt, VBOXVDMAPIPE_CMD_DMACMD *pCmd)
{
    NTSTATUS Status = vboxVdmaDdiCmdSubmitted(pDevExt, &pDevExt->DdiCmdQueue, &pCmd->DdiCmd);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        if (pCmd->fFlags.bDecVBVAUnlock)
        {
            uint32_t cNew = ASMAtomicIncU32(&pDevExt->cUnlockedVBVADisabled);
            Assert(cNew < UINT32_MAX/2);
        }
        NTSTATUS submStatus = vboxVdmaGgCmdSubmit(&pDevExt->u.primary.Vdma.DmaGg, &pCmd->Hdr);
        Assert(submStatus == STATUS_SUCCESS);
        if (submStatus != STATUS_SUCCESS)
        {
            if (pCmd->fFlags.bDecVBVAUnlock)
            {
                uint32_t cNew = ASMAtomicDecU32(&pDevExt->cUnlockedVBVADisabled);
                Assert(cNew < UINT32_MAX/2);
            }
            vboxVdmaDdiCmdCompleted(pDevExt, &pDevExt->DdiCmdQueue, &pCmd->DdiCmd, DXGK_INTERRUPT_DMA_FAULTED);
        }
    }
    else
    {
        vboxVdmaGgCmdDestroy(&pCmd->Hdr);
    }
    return Status;
}

static NTSTATUS vboxWddmSubmitBltCmd(PDEVICE_EXTENSION pDevExt, VBOXWDDM_CONTEXT *pContext, UINT u32FenceId, PVBOXWDDM_DMA_PRIVATEDATA_BLT pBlt, VBOXVDMAPIPE_FLAGS_DMACMD fBltFlags)
{
    NTSTATUS Status = STATUS_SUCCESS;

    PVBOXVDMAPIPE_CMD_DMACMD_BLT pBltCmd = (PVBOXVDMAPIPE_CMD_DMACMD_BLT)vboxVdmaGgCmdCreate(&pDevExt->u.primary.Vdma.DmaGg, VBOXVDMAPIPE_CMD_TYPE_DMACMD, RT_OFFSETOF(VBOXVDMAPIPE_CMD_DMACMD_BLT, Blt.DstRects.UpdateRects.aRects[pBlt->Blt.DstRects.UpdateRects.cRects]));
    Assert(pBltCmd);
    if (pBltCmd)
    {
        VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pBlt->Blt.DstAlloc.srcId];
        vboxVdmaDdiCmdInit(&pBltCmd->Hdr.DdiCmd, u32FenceId, pContext, vboxVdmaGgDdiCmdDestroy, pBltCmd);
        pBltCmd->Hdr.pDevExt = pDevExt;
        pBltCmd->Hdr.fFlags = fBltFlags;
        pBltCmd->Hdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_BLT;
        memcpy(&pBltCmd->Blt, &pBlt->Blt, RT_OFFSETOF(VBOXVDMA_BLT, DstRects.UpdateRects.aRects[pBlt->Blt.DstRects.UpdateRects.cRects]));
        vboxWddmSubmitCmd(pDevExt, &pBltCmd->Hdr);
    }
    else
    {
        Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, u32FenceId, DXGK_INTERRUPT_DMA_FAULTED);
    }

    return Status;
}

#ifdef VBOX_WITH_VDMA
DECLCALLBACK(VOID) vboxWddmDmaCompleteChromiumCmd(PDEVICE_EXTENSION pDevExt, PVBOXVDMADDI_CMD pCmd, PVOID pvContext)
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

//    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pSubmitCommand->hContext;
    Assert(pContext);
    Assert(pContext->pDevice);
    Assert(pContext->pDevice->pAdapter == pDevExt);
    Assert(!pSubmitCommand->DmaBufferSegmentId);

    /* the DMA command buffer is located in system RAM, the host will need to pick it from there */
    //BufInfo.fFlags = 0; /* see VBOXVDMACBUF_FLAG_xx */
    Assert(pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR));
    if (pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        drprintf((__FUNCTION__": DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR) (%d)\n",
                pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset,
                pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        return STATUS_INVALID_PARAMETER;
    }

    PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR pPrivateDataBase = (PVBOXWDDM_DMA_PRIVATEDATA_BASEHDR)((uint8_t*)pSubmitCommand->pDmaBufferPrivateData + pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset);
    Assert(pPrivateDataBase);
    switch (pPrivateDataBase->enmCmd)
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

            Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
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
                                        VBOXVBVA_OP(ReportDirtyRect, pDevExt, pSource, &rect);
                                    else
                                    {
                                        Assert(KeGetCurrentIrql() == DISPATCH_LEVEL);
                                        VBOXVBVA_OP_WITHLOCK_ATDPC(ReportDirtyRect, pDevExt, pSource, &rect);
                                    }
                                }
                                else
                                {
                                    fBltFlags.b2DRelated = 1;
                                    fBltFlags.bDecVBVAUnlock = 1;
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
                    break;
                }
                default:
                    AssertBreakpoint();
                    break;
            }

            if (bComplete)
            {
                Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            }
            break;
        }
        case VBOXVDMACMD_TYPE_CHROMIUM_CMD:
        {
#ifdef VBOX_WITH_VDMA
            VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD *pChromiumCmd = (VBOXWDDM_DMA_PRIVATEDATA_CHROMIUM_CMD*)pPrivateDataBase;
            UINT cbCmd = VBOXVDMACMD_SIZE_FROMBODYSIZE(RT_OFFSETOF(VBOXVDMACMD_CHROMIUM_CMD, aBuffers[pChromiumCmd->Base.u32CmdReserved]));

            PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate (&pDevExt->u.primary.Vdma, cbCmd);
            if (!pDr)
            {
                /* @todo: try flushing.. */
                drprintf((__FUNCTION__": vboxVdmaCBufDrCreate returned NULL\n"));
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
            vboxVdmaDdiCmdInit(pDdiCmd, pSubmitCommand->SubmissionFenceId, pContext, vboxWddmDmaCompleteChromiumCmd, pDr);
            NTSTATUS Status = vboxVdmaDdiCmdSubmitted(pDevExt, &pDevExt->DdiCmdQueue, pDdiCmd);
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
            Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            Assert(Status == STATUS_SUCCESS);
#endif
            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP:
        {
            VBOXWDDM_DMA_PRIVATEDATA_FLIP *pFlip = (VBOXWDDM_DMA_PRIVATEDATA_FLIP*)pPrivateDataBase;
            PVBOXVDMAPIPE_CMD_DMACMD_FLIP pFlipCmd = (PVBOXVDMAPIPE_CMD_DMACMD_FLIP)vboxVdmaGgCmdCreate(
                    &pDevExt->u.primary.Vdma.DmaGg, VBOXVDMAPIPE_CMD_TYPE_DMACMD, sizeof (VBOXVDMAPIPE_CMD_DMACMD_FLIP));
            Assert(pFlipCmd);
            if (pFlipCmd)
            {
                VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pFlip->Flip.Alloc.srcId];
                vboxVdmaDdiCmdInit(&pFlipCmd->Hdr.DdiCmd, pSubmitCommand->SubmissionFenceId, pContext, vboxVdmaGgDdiCmdDestroy, pFlipCmd);
                pFlipCmd->Hdr.pDevExt = pDevExt;
                pFlipCmd->Hdr.fFlags.Value = 0;
                pFlipCmd->Hdr.fFlags.b3DRelated = 1;
                pFlipCmd->Hdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_FLIP;
                memcpy(&pFlipCmd->Flip, &pFlip->Flip, sizeof (pFlipCmd->Flip));
                Status = vboxWddmSubmitCmd(pDevExt, &pFlipCmd->Hdr);
                Assert(Status == STATUS_SUCCESS);
            }
            else
            {
                Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_FAULTED);
                Assert(Status == STATUS_SUCCESS);
            }
            break;
        }
        case VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL:
        {
            PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL pCF = (PVBOXWDDM_DMA_PRIVATEDATA_CLRFILL)pPrivateDataBase;
            PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL pCFCmd = (PVBOXVDMAPIPE_CMD_DMACMD_CLRFILL)vboxVdmaGgCmdCreate(
                    &pDevExt->u.primary.Vdma.DmaGg, VBOXVDMAPIPE_CMD_TYPE_DMACMD,
                    RT_OFFSETOF(VBOXVDMAPIPE_CMD_DMACMD_CLRFILL, ClrFill.Rects.aRects[pCF->ClrFill.Rects.cRects]));
            Assert(pCFCmd);
            if (pCFCmd)
            {
//                VBOXWDDM_SOURCE *pSource = &pDevExt->aSources[pFlip->Flip.Alloc.srcId];
                vboxVdmaDdiCmdInit(&pCFCmd->Hdr.DdiCmd, pSubmitCommand->SubmissionFenceId, pContext, vboxVdmaGgDdiCmdDestroy, pCFCmd);
                pCFCmd->Hdr.pDevExt = pDevExt;
                pCFCmd->Hdr.pDevExt = pDevExt;
                pCFCmd->Hdr.fFlags.Value = 0;
                pCFCmd->Hdr.fFlags.b2DRelated = 1;
                pCFCmd->Hdr.fFlags.bDecVBVAUnlock = 1;
                pCFCmd->Hdr.enmCmd = VBOXVDMACMD_TYPE_DMA_PRESENT_CLRFILL;
                memcpy(&pCFCmd->ClrFill, &pCF->ClrFill, RT_OFFSETOF(VBOXVDMA_CLRFILL, Rects.aRects[pCF->ClrFill.Rects.cRects]));
                Status = vboxWddmSubmitCmd(pDevExt, &pCFCmd->Hdr);
                Assert(Status == STATUS_SUCCESS);
            }
            else
            {
                Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_FAULTED);
                Assert(Status == STATUS_SUCCESS);
            }

            break;
        }
        case VBOXVDMACMD_TYPE_DMA_NOP:
        {
            Status = vboxVdmaDdiCmdFenceComplete(pDevExt, pContext, pSubmitCommand->SubmissionFenceId, DXGK_INTERRUPT_DMA_COMPLETED);
            Assert(Status == STATUS_SUCCESS);
            break;
        }
        default:
        {
            AssertBreakpoint();
#if 0 //def VBOX_WITH_VDMA
            VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR *pPrivateData = (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR*)pPrivateDataBase;
            PVBOXVDMACBUF_DR pDr = vboxVdmaCBufDrCreate (&pDevExt->u.primary.Vdma, 0);
            if (!pDr)
            {
                /* @todo: try flushing.. */
                drprintf((__FUNCTION__": vboxVdmaCBufDrCreate returned NULL\n"));
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
//    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiPreemptCommand(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PREEMPTCOMMAND*  pPreemptCommand)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

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
#ifdef DEBUG_misha
                        Assert(pBuildPagingBuffer->Transfer.Source.SegmentAddress.QuadPart);
#endif
                        uint64_t off = pBuildPagingBuffer->Transfer.Source.SegmentAddress.QuadPart;
                        off += pBuildPagingBuffer->Transfer.TransferOffset + cbTransfered;
                        pBody->Src.offVramBuf = off;
                        pBody->fFlags |= VBOXVDMACMD_DMA_BPB_TRANSFER_F_SRC_VRAMOFFSET;
                    }
                    else
                    {
                        UINT index = pBuildPagingBuffer->Transfer.MdlOffset + (cbTransfered>>12);
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
#ifdef DEBUG_misha
                        Assert(pBuildPagingBuffer->Transfer.Destination.SegmentAddress.QuadPart);
#endif
                        uint64_t off = pBuildPagingBuffer->Transfer.Destination.SegmentAddress.QuadPart;
                        off += pBuildPagingBuffer->Transfer.TransferOffset;
                        pBody->Dst.offVramBuf = off + cbTransfered;
                        pBody->fFlags |= VBOXVDMACMD_DMA_BPB_TRANSFER_F_DST_VRAMOFFSET;
                    }
                    else
                    {
                        UINT index = pBuildPagingBuffer->Transfer.MdlOffset + (cbTransfered>>12);
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

                    pBody->cbTransferSize = cbCurTransfer;
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
                drprintf((__FUNCTION__": vboxVdmaCBufDrCreate returned NULL\n"));
                Status = STATUS_INSUFFICIENT_RESOURCES;
            }
#endif
#endif /* #ifdef VBOX_WITH_VDMA */
            break;
        }
        case DXGK_OPERATION_FILL:
        {
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
            drprintf((__FUNCTION__": unsupported op (%d)\n", pBuildPagingBuffer->Operation));
            AssertBreakpoint();
            break;
        }
    }

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    return Status;

}

NTSTATUS
APIENTRY
DxgkDdiSetPalette(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPALETTE*  pSetPalette
    )
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

BOOL vboxWddmPointerCopyColorData(CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape, PVIDEO_POINTER_ATTRIBUTES pPointerAttributes)
{
    /* Format of "hardware" pointer is:
     * 1 bpp AND mask with byte aligned scanlines,
     * B G R A bytes of XOR mask that starts on the next 4 byte aligned offset after AND mask.
     *
     * If fl & SPS_ALPHA then A bytes contain alpha channel information.
     * Otherwise A bytes are undefined (but will be 0).
     *
     */
    PBYTE pjSrcAnd = NULL;
    PBYTE pjSrcXor = NULL;

    ULONG cy = 0;

    PBYTE pjDstAnd = pPointerAttributes->Pixels;
    ULONG cjAnd = 0;
    PBYTE pjDstXor = pPointerAttributes->Pixels;

    ULONG cxSrc = pSetPointerShape->Width;
    ULONG cySrc = pSetPointerShape->Width;

    // Make sure the new pointer isn't too big to handle,
    // strip the size to 64x64 if necessary
    if (cxSrc > VBOXWDDM_C_POINTER_MAX_WIDTH)
        cxSrc = VBOXWDDM_C_POINTER_MAX_WIDTH;

    if (cySrc > VBOXWDDM_C_POINTER_MAX_HEIGHT)
        cySrc = VBOXWDDM_C_POINTER_MAX_HEIGHT;

    /* Size of AND mask in bytes */
    cjAnd = ((cxSrc + 7) / 8) * cySrc;

    /* Pointer to XOR mask is 4-bytes aligned */
    pjDstXor += (cjAnd + 3) & ~3;

    pPointerAttributes->Width = cxSrc;
    pPointerAttributes->Height = cySrc;
    pPointerAttributes->WidthInBytes = cxSrc * 4;

    uint32_t cbData = ((cjAnd + 3) & ~3) + pPointerAttributes->Height*pPointerAttributes->WidthInBytes;
    uint32_t cbPointerAttributes = RT_OFFSETOF(VIDEO_POINTER_ATTRIBUTES, Pixels[cbData]);
    Assert(VBOXWDDM_POINTER_ATTRIBUTES_SIZE >= cbPointerAttributes);
    if (VBOXWDDM_POINTER_ATTRIBUTES_SIZE < cbPointerAttributes)
    {
        drprintf((__FUNCTION__": VBOXWDDM_POINTER_ATTRIBUTES_SIZE(%d) < cbPointerAttributes(%d)\n", VBOXWDDM_POINTER_ATTRIBUTES_SIZE, cbPointerAttributes));
        return FALSE;
    }

    /* Init AND mask to 1 */
    RtlFillMemory (pjDstAnd, cjAnd, 0xFF);

    PBYTE pjSrcAlpha = (PBYTE)pSetPointerShape->pPixels;

    /*
     * Emulate AND mask to provide viewable mouse pointer for
     * hardware which does not support alpha channel.
     */

    for (cy = 0; cy < cySrc; cy++)
    {
        ULONG cx;

        UCHAR bitmask = 0x80;

        for (cx = 0; cx < cxSrc; cx++, bitmask >>= 1)
        {
            if (bitmask == 0)
            {
                bitmask = 0x80;
            }

            if (pjSrcAlpha[cx * 4 + 3] > 0x7f)
            {
               pjDstAnd[cx / 8] &= ~bitmask;
            }
        }

        // Point to next source and dest scans
        pjSrcAlpha += pSetPointerShape->Pitch;
        pjDstAnd += (cxSrc + 7) / 8;
    }

    /*
     * pso is 32 bit BGRX bitmap. Copy it to Pixels
     */
    pjSrcXor = (PBYTE)pSetPointerShape->pPixels;
    for (cy = 0; cy < cySrc; cy++)
    {
        /* 32 bit bitmap is being copied */
        RtlCopyMemory (pjDstXor, pjSrcXor, cxSrc * 4);

        /* Point to next source and dest scans */
        pjSrcXor += pSetPointerShape->Pitch;
        pjDstXor += pPointerAttributes->WidthInBytes;
    }

    return TRUE;
}

BOOL vboxWddmPointerCopyMonoData(CONST DXGKARG_SETPOINTERSHAPE* pSetPointerShape, PVIDEO_POINTER_ATTRIBUTES pPointerAttributes)
{
    PBYTE pjSrc = NULL;

    ULONG cy = 0;

    PBYTE pjDstAnd = pPointerAttributes->Pixels;
    ULONG cjAnd = 0;
    PBYTE pjDstXor = pPointerAttributes->Pixels;

    ULONG cxSrc = pSetPointerShape->Width;
    ULONG cySrc = pSetPointerShape->Height;

    // Make sure the new pointer isn't too big to handle,
    // strip the size to 64x64 if necessary
    if (cxSrc > VBOXWDDM_C_POINTER_MAX_WIDTH)
        cxSrc = VBOXWDDM_C_POINTER_MAX_WIDTH;

    if (cySrc > VBOXWDDM_C_POINTER_MAX_HEIGHT)
        cySrc = VBOXWDDM_C_POINTER_MAX_HEIGHT;

    /* Size of AND mask in bytes */
    cjAnd = ((cxSrc + 7) / 8) * cySrc;

    /* Pointer to XOR mask is 4-bytes aligned */
    pjDstXor += (cjAnd + 3) & ~3;

    pPointerAttributes->Width = cxSrc;
    pPointerAttributes->Height = cySrc;
    pPointerAttributes->WidthInBytes = cxSrc * 4;

    /* Init AND mask to 1 */
    RtlFillMemory (pjDstAnd, cjAnd, 0xFF);

    /*
     * Copy AND mask.
     */
    pjSrc = (PBYTE)pSetPointerShape->pPixels;

    for (cy = 0; cy < cySrc; cy++)
    {
        RtlCopyMemory (pjDstAnd, pjSrc, (cxSrc + 7) / 8);

        // Point to next source and dest scans
        pjSrc += pSetPointerShape->Pitch;
        pjDstAnd += (cxSrc + 7) / 8;
    }

    for (cy = 0; cy < cySrc; ++cy)
    {
        ULONG cx;

        UCHAR bitmask = 0x80;

        for (cx = 0; cx < cxSrc; cx++, bitmask >>= 1)
        {
            if (bitmask == 0)
            {
                bitmask = 0x80;
            }

            if (pjSrc[cx / 8] & bitmask)
            {
                *(ULONG *)&pjDstXor[cx * 4] = 0x00FFFFFF;
            }
            else
            {
                *(ULONG *)&pjDstXor[cx * 4] = 0;
            }
        }

        // Point to next source and dest scans
        pjSrc += pSetPointerShape->Pitch;
        pjDstXor += cxSrc * 4;
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
            drprintf((__FUNCTION__": vboxWddmPointerCopyColorData failed\n"));
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
            drprintf((__FUNCTION__": vboxWddmPointerCopyMonoData failed\n"));
            AssertBreakpoint();
            return FALSE;
        }
    }
    else
    {
        drprintf((__FUNCTION__": unsupported pointer type Flags.Value(0x%x)\n", pSetPointerShape->Flags.Value));
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
//    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    /* mouse integration is ON */
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    PVBOXWDDM_POINTER_INFO pPointerInfo = &pDevExt->aSources[pSetPointerPosition->VidPnSourceId].PointerInfo;
    PVBOXWDDM_GLOBAL_POINTER_INFO pGlobalPointerInfo = &pDevExt->PointerInfo;
    PVIDEO_POINTER_ATTRIBUTES pPointerAttributes = &pPointerInfo->Attributes.data;
    BOOLEAN bNotifyVisibility;
    if (pSetPointerPosition->Flags.Visible)
    {
        bNotifyVisibility = (pGlobalPointerInfo->cVisible == 0);
        if (!(pPointerAttributes->Enable & VBOX_MOUSE_POINTER_VISIBLE))
        {
            ++pGlobalPointerInfo->cVisible;
            pPointerAttributes->Enable |= VBOX_MOUSE_POINTER_VISIBLE;
        }
    }
    else
    {
        if (!!(pPointerAttributes->Enable & VBOX_MOUSE_POINTER_VISIBLE))
        {
            --pGlobalPointerInfo->cVisible;
            Assert(pGlobalPointerInfo->cVisible < UINT32_MAX/2);
            pPointerAttributes->Enable &= ~VBOX_MOUSE_POINTER_VISIBLE;
            bNotifyVisibility = (pGlobalPointerInfo->cVisible == 0);
        }
    }

    pPointerAttributes->Column = pSetPointerPosition->X;
    pPointerAttributes->Row = pSetPointerPosition->Y;

    if (bNotifyVisibility && vboxQueryHostWantsAbsolute())
    {
        // tell the host to use the guest's pointer
        VIDEO_POINTER_ATTRIBUTES PointerAttributes;

        /* Visible and No Shape means Show the pointer.
         * It is enough to init only this field.
         */
        PointerAttributes.Enable = pSetPointerPosition->Flags.Visible ? VBOX_MOUSE_POINTER_VISIBLE : 0;

        BOOLEAN bResult = vboxUpdatePointerShape(pDevExt, &PointerAttributes, sizeof (PointerAttributes));
        Assert(bResult);
    }

//    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiSetPointerShape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERSHAPE*  pSetPointerShape)
{
//    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_NOT_SUPPORTED;

    if (vboxQueryHostWantsAbsolute())
    {
        /* mouse integration is ON */
        PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
        PVBOXWDDM_POINTER_INFO pPointerInfo = &pDevExt->aSources[pSetPointerShape->VidPnSourceId].PointerInfo;
        /* @todo: to avoid extra data copy and extra heap allocation,
         *  need to maintain the pre-allocated HGSMI buffer and convert the data directly to it */
        if (vboxVddmPointerShapeToAttributes(pSetPointerShape, pPointerInfo))
        {
            if (vboxUpdatePointerShape (pDevExt, &pPointerInfo->Attributes.data, VBOXWDDM_POINTER_ATTRIBUTES_SIZE))
                Status = STATUS_SUCCESS;
            else
            {
                AssertBreakpoint();
                drprintf((__FUNCTION__": vboxUpdatePointerShape failed\n"));
            }
        }
    }

//    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY CALLBACK
DxgkDdiResetFromTimeout(
    CONST HANDLE  hAdapter)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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

//    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    NTSTATUS Status = STATUS_NOT_SUPPORTED;
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    Assert(pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE));
    if (pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE))
    {
        PVBOXDISPIFESCAPE pEscapeHdr = (PVBOXDISPIFESCAPE)pEscape->pPrivateDriverData;
        switch (pEscapeHdr->escapeCode)
        {
            case VBOXESC_GETVBOXVIDEOCMCMD:
            {
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

                dprintf(("IOCTL_VIDEO_VBOX_SETVISIBLEREGION cRects=%d\n", cRects));
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
                            drprintf((__FUNCTION__": VbglGRPerform failed rc (%d)", rc));
                            Status = STATUS_UNSUCCESSFUL;
                        }
                    }
                    else
                    {
                        drprintf((__FUNCTION__": VbglGRAlloc failed rc (%d)", rc));
                        Status = STATUS_UNSUCCESSFUL;
                    }
                }
                else
                {
                    drprintf((__FUNCTION__": VBOXESC_SETVISIBLEREGION: incorrect buffer size (%d), reported count (%d)\n", cbRects, lpRgnData->rdh.nCount));
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
                Assert(pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT));
                if (pEscape->PrivateDriverDataSize >= sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT))
                {
                    PVBOXDISPIFESCAPE_SCREENLAYOUT pLo = (PVBOXDISPIFESCAPE_SCREENLAYOUT)pEscapeHdr;
                    Assert(pLo->ScreenLayout.cScreens <= (UINT)commonFromDeviceExt(pDevExt)->cDisplays);
                    for (UINT i = 0; i < pLo->ScreenLayout.cScreens; ++i)
                    {
                        PVBOXSCREENLAYOUT_ELEMENT pEl = &pLo->ScreenLayout.aScreens[i];
                        Assert(pEl->VidPnSourceId < (UINT)commonFromDeviceExt(pDevExt)->cDisplays);
                        if (pEl->VidPnSourceId < (UINT)commonFromDeviceExt(pDevExt)->cDisplays)
                        {
                            PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pEl->VidPnSourceId];
                            NTSTATUS tmpStatus = vboxWddmGhDisplayUpdateScreenPos(pDevExt, pSource, &pEl->pos);
                            Assert(tmpStatus == STATUS_SUCCESS);
                        }
                    }
                    Status = STATUS_SUCCESS;
                    break;
                }
                else
                {
                    drprintf((__FUNCTION__": VBOXESC_SCREENLAYOUT: incorrect buffer size (%d) < sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT) (%d)\n",
                            pEscape->PrivateDriverDataSize, sizeof (VBOXDISPIFESCAPE_SCREENLAYOUT)));
                    AssertBreakpoint();
                    Status = STATUS_INVALID_PARAMETER;
                }
            }
            case VBOXESC_SWAPCHAININFO:
            {
                PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)pEscape->hContext;
                Status = vboxWddmSwapchainCtxEscape(pDevExt, pContext, (PVBOXDISPIFESCAPE_SWAPCHAININFO)pEscapeHdr, pEscape->PrivateDriverDataSize);
                Assert(Status == STATUS_SUCCESS);
                break;
            }
            case VBOXESC_REINITVIDEOMODES:
                VBoxWddmInvalidateModesTable(pDevExt);
                Status = STATUS_SUCCESS;
                break;
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
                    DbgPrint(pDbgPrint->aStringBuf);
                }
                Status = STATUS_SUCCESS;
                break;
            }
            default:
                Assert(0);
                drprintf((__FUNCTION__": unsupported escape code (0x%x)\n", pEscapeHdr->escapeCode));
                break;
        }
    }
    else
    {
        drprintf((__FUNCTION__": pEscape->PrivateDriverDataSize(%d) < (%d)\n", pEscape->PrivateDriverDataSize, sizeof (VBOXDISPIFESCAPE)));
        AssertBreakpoint();
        Status = STATUS_BUFFER_TOO_SMALL;
    }

//    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiCollectDbgInfo(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_COLLECTDBGINFO*  pCollectDbgInfo
    )
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiQueryCurrentFence(
    CONST HANDLE  hAdapter,
    DXGKARG_QUERYCURRENTFENCE*  pCurrentFence)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;
    BOOLEAN bSupported = TRUE;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pContext->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pIsSupportedVidPnArg->hDesiredVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (Status == STATUS_SUCCESS)
    {
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
        const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
        Status = pVidPnInterface->pfnGetTopology(pIsSupportedVidPnArg->hDesiredVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxVidPnCheckTopology(pIsSupportedVidPnArg->hDesiredVidPn, hVidPnTopology, pVidPnTopologyInterface, &bSupported);
            if (Status == STATUS_SUCCESS && bSupported)
            {
                for (int id = 0; id < commonFromDeviceExt(pContext)->cDisplays; ++id)
                {
                    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
                    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface;
                    Status = pVidPnInterface->pfnAcquireSourceModeSet(pIsSupportedVidPnArg->hDesiredVidPn,
                                    id,
                                    &hNewVidPnSourceModeSet,
                                    &pVidPnSourceModeSetInterface);
                    if (Status == STATUS_SUCCESS)
                    {
                        Status = vboxVidPnCheckSourceModeSet(pIsSupportedVidPnArg->hDesiredVidPn, hNewVidPnSourceModeSet, pVidPnSourceModeSetInterface, &bSupported);

                        pVidPnInterface->pfnReleaseSourceModeSet(pIsSupportedVidPnArg->hDesiredVidPn, hNewVidPnSourceModeSet);

                        if (Status != STATUS_SUCCESS || !bSupported)
                            break;
                    }
                    else if (Status == STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE)
                    {
                        drprintf(("VBoxVideoWddm: Warning: pfnAcquireSourceModeSet returned STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE, continuing\n"));
                        Status = STATUS_SUCCESS;
                    }
                    else
                    {
                        drprintf(("VBoxVideoWddm: pfnAcquireSourceModeSet failed Status(0x%x)\n"));
                        break;
                    }
                }

                if (Status == STATUS_SUCCESS && bSupported)
                {
                    for (int id = 0; id < commonFromDeviceExt(pContext)->cDisplays; ++id)
                    {
                        D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet;
                        CONST DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface;
                        Status = pVidPnInterface->pfnAcquireTargetModeSet(pIsSupportedVidPnArg->hDesiredVidPn,
                                        id, /*__in CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId */
                                        &hNewVidPnTargetModeSet,
                                        &pVidPnTargetModeSetInterface);
                        if (Status == STATUS_SUCCESS)
                        {
                            Status = vboxVidPnCheckTargetModeSet(pIsSupportedVidPnArg->hDesiredVidPn, hNewVidPnTargetModeSet, pVidPnTargetModeSetInterface, &bSupported);

                            pVidPnInterface->pfnReleaseTargetModeSet(pIsSupportedVidPnArg->hDesiredVidPn, hNewVidPnTargetModeSet);

                            if (Status != STATUS_SUCCESS || !bSupported)
                                break;
                        }
                        else if (Status == STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE)
                        {
                            drprintf(("VBoxVideoWddm: Warning: pfnAcquireSourceModeSet returned STATUS_GRAPHICS_INVALID_VIDEO_PRESENT_SOURCE, continuing\n"));
                            Status = STATUS_SUCCESS;
                        }
                        else
                        {
                            drprintf(("VBoxVideoWddm: pfnAcquireSourceModeSet failed Status(0x%x)\n"));
                            break;
                        }
                    }
                }
            }
        }
        else
        {
            drprintf(("VBoxVideoWddm: pfnGetTopology failed Status(0x%x)\n"));
        }
    }
    else
    {
        drprintf(("VBoxVideoWddm: DxgkCbQueryVidPnInterface failed Status(0x%x)\n"));
    }
    pIsSupportedVidPnArg->IsVidPnSupported = bSupported;

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakF();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    NTSTATUS Status;
    uint32_t cModes;
    int iPreferredMode;
    VIDEO_MODE_INFORMATION *pModes;
    uint32_t cResolutions;
    D3DKMDT_2DREGION *pResolutions;
    VIDEO_MODE_INFORMATION ModeInfos[4];
    VIDEO_MODE_INFORMATION *pModeInfos;
    D3DKMDT_2DREGION Resolution;
    uint32_t cModeInfos;
    int32_t iPreferredModeInfo;
    bool bFreeModes = false;
    VBoxWddmGetModesTable(pDevExt, /* PDEVICE_EXTENSION DeviceExtension */
            true, /* bool bRebuildTable*/
            &pModes, /* VIDEO_MODE_INFORMATION ** ppModes*/
            &cModes, /* uint32_t * pcModes */
            &iPreferredMode, /* uint32_t * pPreferrableMode*/
            &pResolutions, /* D3DKMDT_2DREGION **ppResolutions */
            &cResolutions /* uint32_t * pcResolutions */);
    Resolution.cx = pModes[iPreferredMode].VisScreenWidth;
    Resolution.cy = pModes[iPreferredMode].VisScreenHeight;
    Status = VBoxWddmGetModesForResolution(pDevExt, false,
            &Resolution,
            ModeInfos, RT_ELEMENTS(ModeInfos), &cModeInfos, &iPreferredModeInfo);
    Assert(Status == STATUS_SUCCESS || Status == STATUS_BUFFER_TOO_SMALL);
    if (Status == STATUS_SUCCESS)
        pModeInfos = ModeInfos;
    else if (Status == STATUS_BUFFER_TOO_SMALL)
    {
        uint32_t cModeInfos2;
        pModeInfos = (VIDEO_MODE_INFORMATION*)vboxWddmMemAlloc(sizeof (VIDEO_MODE_INFORMATION) * cModeInfos);
        if (pModeInfos)
        {
            bFreeModes = true;
            Status = VBoxWddmGetModesForResolution(pDevExt, false,
                    &Resolution,
                    pModeInfos, cModeInfos, &cModeInfos2, &iPreferredModeInfo);
            Assert(Status == STATUS_SUCCESS);
            Assert(iPreferredModeInfo >= 0); /* the array should contain the preffered info */
            if (Status != STATUS_SUCCESS)
                drprintf((__FUNCTION__": second call to VBoxWddmGetModesForResolution failed Status(0x%x), cModeInfos(%d), cModeInfos2(%d)\n", Status, cModeInfos, cModeInfos2));
        }
    }
    else
        drprintf((__FUNCTION__": VBoxWddmGetModesForResolution failed Status(0x%x)\n", Status));

    if (Status == STATUS_SUCCESS)
    {
        for (int i = 0; i < commonFromDeviceExt(pDevExt)->cDisplays; ++i)
        {
            Status = vboxVidPnCheckAddMonitorModes(pDevExt, i, D3DKMDT_MCO_DRIVER, &Resolution, 1, 0);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
            {
                drprintf((__FUNCTION__": vboxVidPnCheckAddMonitorModes failed Status(0x%x)\n", Status));
                break;
            }
        }

        if (Status == STATUS_SUCCESS)
        {
            const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
            Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                Assert (iPreferredModeInfo >= 0);
                Status = vboxVidPnCreatePopulateVidPnFromLegacy(pDevExt, pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, pVidPnInterface,
                        pModeInfos, cModeInfos, iPreferredModeInfo,
                        &Resolution, 1);
                Assert(Status == STATUS_SUCCESS);
                if (Status != STATUS_SUCCESS)
                    drprintf((__FUNCTION__": vboxVidPnCreatePopulateVidPnFromLegacy failed Status(0x%x)\n", Status));
            }
            else
                drprintf((__FUNCTION__": DxgkCbQueryVidPnInterface failed Status(0x%x)\n", Status));
        }
    }

    if (bFreeModes)
        vboxWddmMemFree(pModeInfos);

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pContext->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pEnumCofuncModalityArg->hConstrainingVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (Status == STATUS_SUCCESS)
    {
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
        const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
        NTSTATUS Status = pVidPnInterface->pfnGetTopology(pEnumCofuncModalityArg->hConstrainingVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            VBOXVIDPNCOFUNCMODALITY CbContext = {0};
            CbContext.pEnumCofuncModalityArg = pEnumCofuncModalityArg;
            VBoxWddmGetModesTable(pContext, /* PDEVICE_EXTENSION DeviceExtension */
                    false, /* bool bRebuildTable*/
                    &CbContext.pModes, /* VIDEO_MODE_INFORMATION ** ppModes*/
                    &CbContext.cModes, /* uint32_t * pcModes */
                    &CbContext.iPreferredMode, /* uint32_t * pPreferrableMode*/
                    &CbContext.pResolutions, /* D3DKMDT_2DREGION **ppResolutions */
                    &CbContext.cResolutions /* uint32_t * pcResolutions */);
            Assert(CbContext.cModes);
            Assert(CbContext.cModes > (uint32_t)CbContext.iPreferredMode);
            CbContext.iPreferredMode = -1; /* <- we do not want the modes to be pinned */
            Status = vboxVidPnEnumPaths(pContext, pEnumCofuncModalityArg->hConstrainingVidPn, pVidPnInterface,
                    hVidPnTopology, pVidPnTopologyInterface,
                    vboxVidPnCofuncModalityPathEnum, &CbContext);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                Status = CbContext.Status;
                Assert(Status == STATUS_SUCCESS);
                if (Status != STATUS_SUCCESS)
                    drprintf((__FUNCTION__ ": vboxVidPnAdjustSourcesTargetsCallback failed Status(0x%x)\n", Status));
            }
            else
                drprintf((__FUNCTION__ ": vboxVidPnEnumPaths failed Status(0x%x)\n", Status));
        }
        else
            drprintf((__FUNCTION__ ": pfnGetTopology failed Status(0x%x)\n", Status));
    }
    else
        drprintf((__FUNCTION__ ": DxgkCbQueryVidPnInterface failed Status(0x%x)\n", Status));

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    Assert((UINT)commonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceAddress->VidPnSourceId);

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pSetVidPnSourceAddress->VidPnSourceId];
    Status= vboxWddmDisplaySettingsQueryPos(pDevExt, pSetVidPnSourceAddress->VidPnSourceId, &pSource->VScreenPos);
    Assert(Status == STATUS_SUCCESS);
    Status = STATUS_SUCCESS;

    if ((UINT)commonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceAddress->VidPnSourceId)
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
                    drprintf((__FUNCTION__": vboxWddmGhDisplaySetInfo failed, Status (0x%x)\n", Status));
#endif
            }
        }
        else
        {
            drprintf((__FUNCTION__": no allocation data available!!\n"));
            Status = STATUS_INVALID_PARAMETER;
        }
    }
    else
    {
        drprintf((__FUNCTION__": invalid VidPnSourceId (%d), should be smaller than (%d)\n", pSetVidPnSourceAddress->VidPnSourceId, commonFromDeviceExt(pDevExt)->cDisplays));
        Status = STATUS_INVALID_PARAMETER;
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    Assert((UINT)commonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceVisibility->VidPnSourceId);

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pSetVidPnSourceVisibility->VidPnSourceId];
    Status= vboxWddmDisplaySettingsQueryPos(pDevExt, pSetVidPnSourceVisibility->VidPnSourceId, &pSource->VScreenPos);
    Assert(Status == STATUS_SUCCESS);
    Status = STATUS_SUCCESS;

    if ((UINT)commonFromDeviceExt(pDevExt)->cDisplays > pSetVidPnSourceVisibility->VidPnSourceId)
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
                            drprintf((__FUNCTION__": vboxWddmGhDisplaySetInfo failed, Status (0x%x)\n", Status));
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
        drprintf((__FUNCTION__": invalid VidPnSourceId (%d), should be smaller than (%d)\n", pSetVidPnSourceVisibility->VidPnSourceId, commonFromDeviceExt(pDevExt)->cDisplays));
        Status = STATUS_INVALID_PARAMETER;
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiCommitVidPn(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_COMMITVIDPN* CONST  pCommitVidPnArg
    )
{
    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;

    vboxVDbgBreakFv();

    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pCommitVidPnArg->hFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (Status == STATUS_SUCCESS)
    {
        if (pCommitVidPnArg->AffectedVidPnSourceId != D3DDDI_ID_ALL)
        {
            Status = vboxVidPnCommitSourceModeForSrcId(
                    pDevExt,
                    pCommitVidPnArg->hFunctionalVidPn, pVidPnInterface,
                    pCommitVidPnArg->AffectedVidPnSourceId, (PVBOXWDDM_ALLOCATION)pCommitVidPnArg->hPrimaryAllocation);
            Assert(Status == STATUS_SUCCESS);
            if (Status != STATUS_SUCCESS)
                drprintf((__FUNCTION__ ": vboxVidPnCommitSourceModeForSrcId failed Status(0x%x)\n", Status));
        }
        else
        {
            /* clear all current primaries */
            for (int i = 0; i < commonFromDeviceExt(pDevExt)->cDisplays; ++i)
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
                CbContext.pCommitVidPnArg = pCommitVidPnArg;
                Status = vboxVidPnEnumPaths(pDevExt, pCommitVidPnArg->hFunctionalVidPn, pVidPnInterface,
                            hVidPnTopology, pVidPnTopologyInterface,
                            vboxVidPnCommitPathEnum, &CbContext);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                {
                        Status = CbContext.Status;
                        Assert(Status == STATUS_SUCCESS);
                        if (Status != STATUS_SUCCESS)
                            drprintf((__FUNCTION__ ": vboxVidPnCommitPathEnum failed Status(0x%x)\n", Status));
                }
                else
                    drprintf((__FUNCTION__ ": vboxVidPnEnumPaths failed Status(0x%x)\n", Status));
            }
            else
                drprintf((__FUNCTION__ ": pfnGetTopology failed Status(0x%x)\n", Status));
        }

        if (Status == STATUS_SUCCESS)
        {
            pDevExt->u.primary.hCommittedVidPn = pCommitVidPnArg->hFunctionalVidPn;
        }
    }
    else
        drprintf((__FUNCTION__ ": DxgkCbQueryVidPnInterface failed Status(0x%x)\n", Status));

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateActiveVidPnPresentPath(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH* CONST  pUpdateActiveVidPnPresentPathArg
    )
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendMonitorModes(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDMONITORMODES* CONST  pRecommendMonitorModesArg
    )
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    NTSTATUS Status;
    uint32_t cModes;
    int32_t iPreferredMode;
    VIDEO_MODE_INFORMATION *pModes;
    uint32_t cResolutions;
    D3DKMDT_2DREGION *pResolutions;
    VBoxWddmGetModesTable(pDevExt, /* PDEVICE_EXTENSION DeviceExtension */
            false, /* bool bRebuildTable*/
            &pModes, /* VIDEO_MODE_INFORMATION ** ppModes*/
            &cModes, /* uint32_t * pcModes */
            &iPreferredMode, /* uint32_t * pPreferrableMode*/
            &pResolutions, /* D3DKMDT_2DREGION **ppResolutions */
            &cResolutions /* uint32_t * pcResolutions */);

    for (uint32_t i = 0; i < cResolutions; i++)
    {
        D3DKMDT_MONITOR_SOURCE_MODE * pNewMonitorSourceModeInfo;
        Status = pRecommendMonitorModesArg->pMonitorSourceModeSetInterface->pfnCreateNewModeInfo(
                    pRecommendMonitorModesArg->hMonitorSourceModeSet, &pNewMonitorSourceModeInfo);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxVidPnPopulateMonitorSourceModeInfoFromLegacy(pDevExt,
                    pNewMonitorSourceModeInfo,
                    &pResolutions[i],
                    D3DKMDT_MCO_DRIVER,
                    FALSE);
            Assert(Status == STATUS_SUCCESS);
            if (Status == STATUS_SUCCESS)
            {
                Status = pRecommendMonitorModesArg->pMonitorSourceModeSetInterface->pfnAddMode(
                        pRecommendMonitorModesArg->hMonitorSourceModeSet, pNewMonitorSourceModeInfo);
                Assert(Status == STATUS_SUCCESS);
                if (Status == STATUS_SUCCESS)
                    continue;
            }

            /* error has occured, release & break */
            pRecommendMonitorModesArg->pMonitorSourceModeSetInterface->pfnReleaseModeInfo(
                    pRecommendMonitorModesArg->hMonitorSourceModeSet, pNewMonitorSourceModeInfo);
            break;
        }
    }

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendVidPnTopology(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY* CONST  pRecommendVidPnTopologyArg
    )
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_GRAPHICS_NO_RECOMMENDED_VIDPN_TOPOLOGY;
}

NTSTATUS
APIENTRY
DxgkDdiGetScanLine(
    CONST HANDLE  hAdapter,
    DXGKARG_GETSCANLINE*  pGetScanLine)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;

    Assert((UINT)commonFromDeviceExt(pDevExt)->cDisplays > pGetScanLine->VidPnTargetId);
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

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiStopCapture(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_STOPCAPTURE*  pStopCapture)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

//    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    /* @todo: STATUS_NOT_IMPLEMENTED ?? */
    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiCreateOverlay(
    CONST HANDLE  hAdapter,
    DXGKARG_CREATEOVERLAY  *pCreateOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%p)\n", hAdapter));

    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)vboxWddmMemAllocZero(sizeof (VBOXWDDM_OVERLAY));
    Assert(pOverlay);
    if (pOverlay)
    {
        int rc = vboxVhwaHlpOverlayCreate(pDevExt, pCreateOverlay->VidPnSourceId, &pCreateOverlay->OverlayInfo, pOverlay);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            vboxVhwaHlpOverlayListAdd(pDevExt, pOverlay);
#ifdef VBOXWDDM_RENDER_FROM_SHADOW
            RECT DstRect;
            vboxVhwaHlpOverlayDstRectGet(pDevExt, pOverlay, &DstRect);
            Status = vboxVdmaHlpUpdatePrimary(pDevExt, pCreateOverlay->VidPnSourceId, &DstRect);
            Assert(Status == STATUS_SUCCESS);
            /* ignore primary update failure */
            Status = STATUS_SUCCESS;
#endif
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

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%p)\n", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyDevice(
    CONST HANDLE  hDevice)
{
    /* DxgkDdiDestroyDevice should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    vboxVDbgBreakFv();

    vboxWddmMemFree(hDevice);

    dfprintf(("<== "__FUNCTION__ ", \n"));

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

    dfprintf(("==> "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)hDevice;
    PDEVICE_EXTENSION pDevExt = pDevice->pAdapter;
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
        for (UINT i = 0; i < pOpenAllocation->NumAllocations; ++i)
        {
            DXGK_OPENALLOCATIONINFO* pInfo = &pOpenAllocation->pOpenAllocation[i];
            Assert(pInfo->PrivateDriverDataSize == sizeof (VBOXWDDM_ALLOCINFO));
            Assert(pInfo->pPrivateDriverData);
            PVBOXWDDM_OPENALLOCATION pOa = (PVBOXWDDM_OPENALLOCATION)vboxWddmMemAllocZero(sizeof (VBOXWDDM_OPENALLOCATION));
            pOa->hAllocation = pInfo->hAllocation;
            pInfo->hDeviceSpecificAllocation = pOa;

            if (pRcInfo)
            {
#ifdef VBOX_WITH_VIDEOHWACCEL
                if (pRcInfo->RcDesc.fFlags.Overlay)
                {
                    if (pInfo->PrivateDriverDataSize >= sizeof (VBOXWDDM_ALLOCINFO))
                    {
                        PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pInfo->pPrivateDriverData;
                        PVBOXWDDM_ALLOCATION pAllocation = vboxWddmGetAllocationFromOpenData(pDevExt, pOa);
                        Assert(pAllocation);
                        if (pAllocation)
                        {
                            /* we have queried host for some surface info, like pitch & size,
                             * need to return it back to the UMD (User Mode Drive) */
                            pAllocInfo->SurfDesc = pAllocation->SurfDesc;
                            /* success, just contionue */
                            continue;
                        }
                        else
                            Status = STATUS_INVALID_PARAMETER;
                    }
                    else
                        Status = STATUS_INVALID_PARAMETER;

                    /* we are here in case of error */
                    AssertBreakpoint();

                    for (UINT j = 0; j < i; ++j)
                    {
                        DXGK_OPENALLOCATIONINFO* pInfo2Free = &pOpenAllocation->pOpenAllocation[j];
                        PVBOXWDDM_OPENALLOCATION pOa2Free = (PVBOXWDDM_OPENALLOCATION)pInfo2Free->hDeviceSpecificAllocation;
                        vboxWddmMemFree(pOa2Free);
                    }
                }
#endif
            }
        }
    }
    dfprintf(("<== "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

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

    dfprintf(("==> "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    vboxVDbgBreakFv();

    for (UINT i = 0; i < pCloseAllocation->NumAllocations; ++i)
    {
        vboxWddmMemFree(pCloseAllocation->pOpenHandleList[i]);
    }

    dfprintf(("<== "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiRender(
    CONST HANDLE  hContext,
    DXGKARG_RENDER  *pRender)
{
    drprintf(("==> "__FUNCTION__ ", hContext(0x%x)\n", hContext));

    Assert(pRender->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR));
    if (pRender->DmaBufferPrivateDataSize < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        drprintf((__FUNCTION__": Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATEDATA_BASEHDR (%d)\n",
                pRender->DmaBufferPrivateDataSize , sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR)));
        /* @todo: can this actually happen? what status to return? */
        return STATUS_INVALID_PARAMETER;
    }
    if (pRender->CommandLength < sizeof (VBOXWDDM_DMA_PRIVATEDATA_BASEHDR))
    {
        Assert(0);
        drprintf((__FUNCTION__": Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATEDATA_BASEHDR (%d)\n",
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
            PDEVICE_EXTENSION pDevExt = pDevice->pAdapter;

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

    drprintf(("<== "__FUNCTION__ ", hContext(0x%x)\n", hContext));

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
DECLINLINE(bool) vboxWddmCheckForVisiblePrimary(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    !!!primary could be of pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_UMD_RC_GENERIC!!!
    if (pAllocation->enmType != VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
        return false;

    if (!pAllocation->bVisible)
        return false;

    D3DDDI_VIDEO_PRESENT_SOURCE_ID id = pAllocation->SurfDesc.VidPnSourceId;
    if (id >= (D3DDDI_VIDEO_PRESENT_SOURCE_ID)commonFromDeviceExt(pDevExt)->cDisplays)
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

//    dfprintf(("==> "__FUNCTION__ ", hContext(0x%x)\n", hContext));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PDEVICE_EXTENSION pDevExt = pDevice->pAdapter;

    Assert(pPresent->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR));
    if (pPresent->DmaBufferPrivateDataSize < sizeof (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR))
    {
        drprintf((__FUNCTION__": Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR (%d)\n", pPresent->DmaBufferPrivateDataSize , sizeof (VBOXWDDM_DMA_PRIVATEDATA_PRESENTHDR)));
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
                            drprintf((__FUNCTION__": unsupported format conversion from(%d) to (%d)\n",pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format));
                            Status = STATUS_GRAPHICS_CANNOTCOLORCONVERT;
                        }
                    }
                    else
                    {
                        /* this should not happen actually */
                        drprintf((__FUNCTION__": cbCmd too small!! (%d)\n", cbCmd));
                        Status = STATUS_GRAPHICS_INSUFFICIENT_DMA_BUFFER;
                    }
#endif
                } while(0);
            }
            else
            {
                /* this should not happen actually */
                drprintf((__FUNCTION__": failed to get Dst Allocation info for hDeviceSpecificAllocation(0x%x)\n",pDst->hDeviceSpecificAllocation));
                Status = STATUS_INVALID_HANDLE;
            }
        }
        else
        {
            /* this should not happen actually */
            drprintf((__FUNCTION__": failed to get Src Allocation info for hDeviceSpecificAllocation(0x%x)\n",pSrc->hDeviceSpecificAllocation));
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
                        drprintf((__FUNCTION__": unsupported format conversion from(%d) to (%d)\n",pSrcAlloc->SurfDesc.format, pDstAlloc->SurfDesc.format));
                        Status = STATUS_GRAPHICS_CANNOTCOLORCONVERT;
                    }
                }
                else
                {
                    /* this should not happen actually */
                    drprintf((__FUNCTION__": failed to get Dst Allocation info for hDeviceSpecificAllocation(0x%x)\n",pDst->hDeviceSpecificAllocation));
                    Status = STATUS_INVALID_HANDLE;
                }
            }
            else
            {
                /* this should not happen actually */
                drprintf((__FUNCTION__": failed to get Src Allocation info for hDeviceSpecificAllocation(0x%x)\n",pSrc->hDeviceSpecificAllocation));
                Status = STATUS_INVALID_HANDLE;
            }
        }
        else
        {
            /* this should not happen actually */
            drprintf((__FUNCTION__": cbCmd too small!! (%d)\n", cbCmd));
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
            drprintf((__FUNCTION__": failed to get pSrc Allocation info for hDeviceSpecificAllocation(0x%x)\n",pSrc->hDeviceSpecificAllocation));
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
            drprintf((__FUNCTION__": failed to get pDst Allocation info for hDeviceSpecificAllocation(0x%x)\n",pDst->hDeviceSpecificAllocation));
            Status = STATUS_INVALID_HANDLE;
        }

    }
    else
    {
        drprintf((__FUNCTION__": cmd NOT IMPLEMENTED!! Flags(0x%x)\n", pPresent->Flags.Value));
        AssertBreakpoint();
    }

//    dfprintf(("<== "__FUNCTION__ ", hContext(0x%x), Status(0x%x)\n", hContext, Status));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_UPDATEOVERLAY  *pUpdateOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hOverlay(0x%p)\n", hOverlay));

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

    dfprintf(("<== "__FUNCTION__ ", hOverlay(0x%p)\n", hOverlay));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiFlipOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_FLIPOVERLAY  *pFlipOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hOverlay(0x%p)\n", hOverlay));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)hOverlay;
    Assert(pOverlay);
    int rc = vboxVhwaHlpOverlayFlip(pOverlay, pFlipOverlay);
    AssertRC(rc);
    if (RT_FAILURE(rc))
        Status = STATUS_UNSUCCESSFUL;

    dfprintf(("<== "__FUNCTION__ ", hOverlay(0x%p)\n", hOverlay));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyOverlay(
    CONST HANDLE  hOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hOverlay(0x%p)\n", hOverlay));

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_OVERLAY pOverlay = (PVBOXWDDM_OVERLAY)hOverlay;
    Assert(pOverlay);
    vboxVhwaHlpOverlayListRemove(pOverlay->pDevExt, pOverlay);
    int rc = vboxVhwaHlpOverlayDestroy(pOverlay);
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        vboxWddmMemFree(pOverlay);
    else
        Status = STATUS_UNSUCCESSFUL;

    dfprintf(("<== "__FUNCTION__ ", hOverlay(0x%p)\n", hOverlay));

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

    dfprintf(("==> "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)hDevice;
    PDEVICE_EXTENSION pDevExt = pDevice->pAdapter;
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
            for (int i = 0; i < commonFromDeviceExt(pDevExt)->cDisplays; ++i)
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
                        ExInitializeFastMutex(&pContext->SwapchainMutex);
                        Status = vboxWddmHTableCreate(&pContext->Swapchains, 4);
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
                            }
                            else
                            {
                                vboxWddmHTableDestroy(&pContext->Swapchains);
                            }
                        }
                        break;
                    }
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_2D:
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_3D:
                    case VBOXWDDM_CONTEXT_TYPE_CUSTOM_UHGSMI_GL:
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

    dfprintf(("<== "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyContext(
    CONST HANDLE  hContext)
{
    dfprintf(("==> "__FUNCTION__ ", hContext(0x%x)\n", hContext));
    vboxVDbgBreakFv();
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PDEVICE_EXTENSION pDevExt = pContext->pDevice->pAdapter;
    if (pContext->enmType == VBOXWDDM_CONTEXT_TYPE_CUSTOM_3D)
    {
        Assert(KeGetCurrentIrql() < DISPATCH_LEVEL);
//        ExAcquireFastMutex(&pDevExt->ContextMutex);
//        RemoveEntryList(&pContext->ListEntry);
        uint32_t cContexts = ASMAtomicDecU32(&pDevExt->cContexts3D);
//        ExReleaseFastMutex(&pDevExt->ContextMutex);
        Assert(cContexts < UINT32_MAX/2);
    }

    NTSTATUS Status = vboxVideoCmCtxRemove(&pContext->pDevice->pAdapter->CmMgr, &pContext->CmContext);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        vboxWddmSwapchainCtxDestroyAll(pDevExt, pContext);
        vboxWddmMemFree(pContext);
    }

    dfprintf(("<== "__FUNCTION__ ", hContext(0x%x)\n", hContext));

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
    drprintf(("==> "__FUNCTION__ ", MiniportDeviceContext(0x%x)\n", MiniportDeviceContext));
    vboxVDbgBreakFv();
    AssertBreakpoint();
    drprintf(("<== "__FUNCTION__ ", MiniportDeviceContext(0x%x)\n", MiniportDeviceContext));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetDisplayPrivateDriverFormat(
    CONST HANDLE  hAdapter,
    /*CONST*/ DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT*  pSetDisplayPrivateDriverFormat
    )
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));
    vboxVDbgBreakFv();
    AssertBreakpoint();
    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));
    return STATUS_SUCCESS;
}

NTSTATUS APIENTRY CALLBACK DxgkDdiRestartFromTimeout(IN_CONST_HANDLE hAdapter)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));
    vboxVDbgBreakFv();
    AssertBreakpoint();
    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));
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

    drprintf(("VBoxVideoWddm::DriverEntry. Built %s %s\n", __DATE__, __TIME__));

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
