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

#include <VBox/VBoxGuestLib.h>
#include <VBox/VBoxVideo.h>

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

void vboxSHGSMICbCommandWrite(struct _HGSMIHEAP * pHeap, HGSMIOFFSET data)
{
    /* @todo: this should be taken from PDEVICE_EXTENSION */
    //VBoxHGSMIGuestWrite(pDevExt, data);
    VBoxVideoCmnPortWriteUlong((PULONG)VGA_PORT_HGSMI_GUEST, data);
}

#if 0
void vboxWddmProcessDisplayInfo (PPDEV ppdev)
{
    if (ppdev->bHGSMISupported)
    {
        /* Issue the screen info command. */
        void *p = HGSMIHeapAlloc (&ppdev->hgsmiDisplayHeap,
                                  sizeof (VBVAINFOSCREEN),
                                  HGSMI_CH_VBVA,
                                  VBVA_INFO_SCREEN);
        if (!p)
        {
            DISPDBG((0, "VBoxDISP::VBoxProcessDisplayInfo: HGSMIHeapAlloc failed\n"));
        }
        else
        {
            VBVAINFOSCREEN *pScreen = (VBVAINFOSCREEN *)p;

            pScreen->u32ViewIndex    = ppdev->iDevice;
            pScreen->i32OriginX      = ppdev->ptlDevOrg.x;
            pScreen->i32OriginY      = ppdev->ptlDevOrg.y;
            pScreen->u32StartOffset  = 0;
            pScreen->u32LineSize     = ppdev->lDeltaScreen > 0?ppdev->lDeltaScreen: -ppdev->lDeltaScreen;
            pScreen->u32Width        = ppdev->cxScreen;
            pScreen->u32Height       = ppdev->cyScreen;
            pScreen->u16BitsPerPixel = (uint16_t)ppdev->ulBitCount;
            pScreen->u16Flags        = VBVA_SCREEN_F_ACTIVE;

            vboxHGSMIBufferSubmit (ppdev, p);

            HGSMIHeapFree (&ppdev->hgsmiDisplayHeap, p);
        }
    }

    return;
}

NTSTATUS vboxWddmGhInitPrimary(PDEVICE_EXTENSION pDevExt, D3DDDI_VIDEO_PRESENT_SOURCE_ID srcId, D3DKMDT_VIDPN_SOURCE_MODE *pVidPnSourceModeInfo)
{
    NTSTATUS Status;
    PVBOXWDDM_SOURCE pSourceInfo = &pDevExt->aSources[srcId];
    memset(pSourceInfo, 0, sizeof (VBOXWDDM_SOURCE));
    pSourceInfo->VisScreenWidth = pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx;
    pSourceInfo->VisScreenHeight = pVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy;
    pSourceInfo->BitsPerPlane = vboxWddmCalcBitsPerPixel(pVidPnSourceModeInfo->Format.Graphics.PixelFormat);
    Assert(pSourceInfo->BitsPerPlane);
    if (pSourceInfo->BitsPerPlane)
    {
        /*
         * Set the current mode into the hardware.
         */
        if (VBoxVideoSetCurrentMode(pDevExt, srcId, pSourceInfo))
        {


        }
        else
        {
            AssertBreakpoint();

        }
    }
    else
    {
        drprintf((__FUNCTION__":ERROR: failed to caltulate bpp from pixel format (%d)\n",
                pVidPnSourceModeInfo->Format.Graphics.PixelFormat));
        return STATUS_INVALID_PARAMETER;
    }

    return Status;



//    DWORD returnedDataLength;
//    DWORD MaxWidth, MaxHeight;
//    VIDEO_MEMORY videoMemory;
//    VIDEO_MEMORY_INFORMATION videoMemoryInformation;
//    ULONG RemappingNeeded = 0;


    /*
     * don't need to map anything here, we use one common HGSMI heap for all sources
     */

    // If this is the first time we enable the surface we need to map in the
    // memory also.
    //

    if (bFirst || RemappingNeeded)
    {
//        videoMemory.RequestedVirtualAddress = NULL;
//
//        if (EngDeviceIoControl(ppdev->hDriver,
//                               IOCTL_VIDEO_MAP_VIDEO_MEMORY,
//                               &videoMemory,
//                               sizeof(VIDEO_MEMORY),
//                               &videoMemoryInformation,
//                               sizeof(VIDEO_MEMORY_INFORMATION),
//                               &returnedDataLength))
//        {
//            DISPDBG((1, "DISP bInitSURF failed IOCTL_VIDEO_MAP\n"));
//            return(FALSE);
//        }
//
//        ppdev->pjScreen = (PBYTE)(videoMemoryInformation.FrameBufferBase);
//
//        if (videoMemoryInformation.FrameBufferBase !=
//            videoMemoryInformation.VideoRamBase)
//        {
//            DISPDBG((0, "VideoRamBase does not correspond to FrameBufferBase\n"));
//        }
//
//        //
//        // Make sure we can access this video memory
//        //
//
//        *(PULONG)(ppdev->pjScreen) = 0xaa55aa55;
//
//        if (*(PULONG)(ppdev->pjScreen) != 0xaa55aa55) {
//
//            DISPDBG((1, "Frame buffer memory is not accessible.\n"));
//            return(FALSE);
//        }
//
//        /* Clear VRAM to avoid distortions during the video mode change. */
//        RtlZeroMemory(ppdev->pjScreen,
//                      ppdev->cyScreen * (ppdev->lDeltaScreen > 0? ppdev->lDeltaScreen: -ppdev->lDeltaScreen));
//
//        //
//        // Initialize the head of the offscreen list to NULL.
//        //
//
//        ppdev->pOffscreenList = NULL;
//
//        // It's a hardware pointer; set up pointer attributes.
//
//        MaxHeight = ppdev->PointerCapabilities.MaxHeight;
//
//        // Allocate space for two DIBs (data/mask) for the pointer. If this
//        // device supports a color Pointer, we will allocate a larger bitmap.
//        // If this is a color bitmap we allocate for the largest possible
//        // bitmap because we have no idea of what the pixel depth might be.
//
//        // Width rounded up to nearest byte multiple
//
//        if (!(ppdev->PointerCapabilities.Flags & VIDEO_MODE_COLOR_POINTER))
//        {
//            MaxWidth = (ppdev->PointerCapabilities.MaxWidth + 7) / 8;
//        }
//        else
//        {
//            MaxWidth = ppdev->PointerCapabilities.MaxWidth * sizeof(DWORD);
//        }
//
//        ppdev->cjPointerAttributes =
//                sizeof(VIDEO_POINTER_ATTRIBUTES) +
//                ((sizeof(UCHAR) * MaxWidth * MaxHeight) * 2);
//
//        ppdev->pPointerAttributes = (PVIDEO_POINTER_ATTRIBUTES)
//                EngAllocMem(0, ppdev->cjPointerAttributes, ALLOC_TAG);
//
//        if (ppdev->pPointerAttributes == NULL) {
//
//            DISPDBG((0, "bInitPointer EngAllocMem failed\n"));
//            return(FALSE);
//        }
//
//        ppdev->pPointerAttributes->Flags = ppdev->PointerCapabilities.Flags;
//        ppdev->pPointerAttributes->WidthInBytes = MaxWidth;
//        ppdev->pPointerAttributes->Width = ppdev->PointerCapabilities.MaxWidth;
//        ppdev->pPointerAttributes->Height = MaxHeight;
//        ppdev->pPointerAttributes->Column = 0;
//        ppdev->pPointerAttributes->Row = 0;
//        ppdev->pPointerAttributes->Enable = 0;

        vboxInitVBoxVideo (ppdev, &videoMemoryInformation);

#ifndef VBOX_WITH_HGSMI
        if (ppdev->bVBoxVideoSupported)
        {
            /* Setup the display information. */
            vboxSetupDisplayInfo (ppdev, &videoMemoryInformation);
        }
#endif /* !VBOX_WITH_HGSMI */
    }


    DISPDBG((1, "DISP bInitSURF: ppdev->ulBitCount %d\n", ppdev->ulBitCount));

    if (   ppdev->ulBitCount == 16
        || ppdev->ulBitCount == 24
        || ppdev->ulBitCount == 32)
    {
#ifndef VBOX_WITH_HGSMI
        if (ppdev->pInfo) /* Do not use VBVA on old hosts. */
        {
            /* Enable VBVA for this video mode. */
            vboxVbvaEnable (ppdev);
        }
#else
        if (ppdev->bHGSMISupported)
        {
            /* Enable VBVA for this video mode. */
            ppdev->bHGSMISupported = vboxVbvaEnable (ppdev);
            LogRel(("VBoxDisp[%d]: VBVA %senabled\n", ppdev->iDevice, ppdev->bHGSMISupported? "": "not "));
        }
#endif /* VBOX_WITH_HGSMI */
    }

    DISPDBG((1, "DISP bInitSURF success\n"));

#ifndef VBOX_WITH_HGSMI
    /* Update the display information. */
    vboxUpdateDisplayInfo (ppdev);
#else
    /* Inform the host about this screen layout. */
    DISPDBG((1, "bInitSURF: %d,%d\n", ppdev->ptlDevOrg.x, ppdev->ptlDevOrg.y));
    VBoxProcessDisplayInfo (ppdev);
#endif /* VBOX_WITH_HGSMI */

#ifdef VBOX_WITH_VIDEOHWACCEL
    /* tells we can process host commands */
    vboxVHWAEnable(ppdev);
#endif

    return(TRUE);
}
#endif

HGSMIHEAP* vboxWddmHgsmiGetHeapFromCmdOffset(PDEVICE_EXTENSION pDevExt, HGSMIOFFSET offCmd)
{
    if(HGSMIAreaContainsOffset(&pDevExt->u.primary.Vdma.CmdHeap.area, offCmd))
        return &pDevExt->u.primary.Vdma.CmdHeap;
    if (HGSMIAreaContainsOffset(&pDevExt->u.primary.hgsmiAdapterHeap.area, offCmd))
        return &pDevExt->u.primary.hgsmiAdapterHeap;
    return NULL;
}

typedef enum
{
    VBOXWDDM_HGSMICMD_TYPE_UNDEFINED = 0,
    VBOXWDDM_HGSMICMD_TYPE_CTL       = 1,
    VBOXWDDM_HGSMICMD_TYPE_DMACMD    = 2
} VBOXWDDM_HGSMICMD_TYPE;

VBOXWDDM_HGSMICMD_TYPE vboxWddmHgsmiGetCmdTypeFromOffset(PDEVICE_EXTENSION pDevExt, HGSMIOFFSET offCmd)
{
    if(HGSMIAreaContainsOffset(&pDevExt->u.primary.Vdma.CmdHeap.area, offCmd))
        return VBOXWDDM_HGSMICMD_TYPE_DMACMD;
    if (HGSMIAreaContainsOffset(&pDevExt->u.primary.hgsmiAdapterHeap.area, offCmd))
        return VBOXWDDM_HGSMICMD_TYPE_CTL;
    return VBOXWDDM_HGSMICMD_TYPE_UNDEFINED;
}


#define VBOXWDDM_REG_DRVKEY_PREFIX L"\\Registry\\Machine\\System\\CurrentControlSet\\Control\\Class\\"

NTSTATUS vboxWddmRegQueryDrvKeyName(PDEVICE_EXTENSION pDevExt, ULONG cbBuf, PWCHAR pBuf, PULONG pcbResult)
{
    WCHAR fallBackBuf[2];
    PWCHAR pSuffix;
    bool bFallback = false;

    if (cbBuf > sizeof(VBOXWDDM_REG_DRVKEY_PREFIX))
    {
        wcscpy(pBuf, VBOXWDDM_REG_DRVKEY_PREFIX);
        pSuffix = pBuf + (sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2);
        cbBuf -= sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2;
    }
    else
    {
        pSuffix = fallBackBuf;
        cbBuf = sizeof (fallBackBuf);
        bFallback = true;
    }

    NTSTATUS Status = IoGetDeviceProperty (pDevExt->pPDO,
                                  DevicePropertyDriverKeyName,
                                  cbBuf,
                                  pSuffix,
                                  &cbBuf);
    if (Status == STATUS_SUCCESS && bFallback)
        Status = STATUS_BUFFER_TOO_SMALL;
    if (Status == STATUS_BUFFER_TOO_SMALL)
        *pcbResult = cbBuf + sizeof (VBOXWDDM_REG_DRVKEY_PREFIX)-2;

    return Status;
}

NTSTATUS vboxWddmRegOpenKey(OUT PHANDLE phKey, IN PWCHAR pName, IN ACCESS_MASK fAccess)
{
    OBJECT_ATTRIBUTES ObjAttr;
    UNICODE_STRING RtlStr;
    RtlStr.Buffer = pName;
    RtlStr.Length = USHORT(wcslen(pName) * sizeof(WCHAR));
    RtlStr.MaximumLength = RtlStr.Length + sizeof(WCHAR);

    InitializeObjectAttributes(&ObjAttr, &RtlStr, OBJ_CASE_INSENSITIVE, NULL, NULL);

    return ZwOpenKey(phKey, fAccess, &ObjAttr);
}

NTSTATUS vboxWddmRegQueryValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT PDWORD pDword)
{
    UCHAR Buf[32]; /* should be enough */
    ULONG cbBuf;
    PKEY_VALUE_PARTIAL_INFORMATION pInfo = (PKEY_VALUE_PARTIAL_INFORMATION)Buf;
    UNICODE_STRING RtlStr;
    RtlStr.Buffer = pName;
    RtlStr.Length = USHORT(wcslen(pName) * sizeof(WCHAR));
    RtlStr.MaximumLength = RtlStr.Length + sizeof(WCHAR);
    NTSTATUS Status = ZwQueryValueKey(hKey,
                &RtlStr,
                KeyValuePartialInformation,
                pInfo,
                sizeof(Buf),
                &cbBuf);
    if (Status == STATUS_SUCCESS)
    {
        if (pInfo->Type == REG_DWORD)
        {
            Assert(pInfo->DataLength == 4);
            *pDword = *((PULONG)pInfo->Data);
            return STATUS_SUCCESS;
        }
    }

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS vboxWddmRegSetValueDword(IN HANDLE hKey, IN PWCHAR pName, OUT DWORD val)
{
    UCHAR Buf[32]; /* should be enough */
    PKEY_VALUE_PARTIAL_INFORMATION pInfo = (PKEY_VALUE_PARTIAL_INFORMATION)Buf;
    UNICODE_STRING RtlStr;
    RtlStr.Buffer = pName;
    RtlStr.Length = USHORT(wcslen(pName) * sizeof(WCHAR));
    RtlStr.MaximumLength = RtlStr.Length + sizeof(WCHAR);
    return ZwSetValueKey(hKey, &RtlStr,
            NULL, /* IN ULONG  TitleIndex  OPTIONAL, reserved */
            REG_DWORD,
            &val,
            sizeof(val));
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

UINT vboxWddmCalcBitsPerPixel(D3DDDIFORMAT format)
{
    switch (format)
    {
        case D3DDDIFMT_R8G8B8:
            return 24;
        case D3DDDIFMT_A8R8G8B8:
        case D3DDDIFMT_X8R8G8B8:
            return 32;
        case D3DDDIFMT_R5G6B5:
        case D3DDDIFMT_X1R5G5B5:
        case D3DDDIFMT_A1R5G5B5:
        case D3DDDIFMT_A4R4G4B4:
            return 16;
        case D3DDDIFMT_R3G3B2:
        case D3DDDIFMT_A8:
            return 8;
        case D3DDDIFMT_A8R3G3B2:
        case D3DDDIFMT_X4R4G4B4:
            return 16;
        case D3DDDIFMT_A2B10G10R10:
        case D3DDDIFMT_A8B8G8R8:
        case D3DDDIFMT_X8B8G8R8:
        case D3DDDIFMT_G16R16:
        case D3DDDIFMT_A2R10G10B10:
            return 32;
        case D3DDDIFMT_A16B16G16R16:
            return 64;
        default:
            AssertBreakpoint();
            return 0;
    }
}

D3DDDIFORMAT vboxWddmCalcPixelFormat(VIDEO_MODE_INFORMATION *pInfo)
{
    switch (pInfo->BitsPerPlane)
    {
        case 32:
            if(!(pInfo->AttributeFlags & VIDEO_MODE_PALETTE_DRIVEN) && !(pInfo->AttributeFlags & VIDEO_MODE_MANAGED_PALETTE))
            {
                if (pInfo->RedMask == 0xFF0000 && pInfo->GreenMask == 0xFF00 && pInfo->BlueMask == 0xFF)
                    return D3DDDIFMT_X8R8G8B8;
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

UINT vboxWddmCalcPitch(UINT w, UINT bitsPerPixel)
{
    UINT Pitch = bitsPerPixel * w;
    /* pitch is now in bits, translate in bytes */
    if(Pitch & 7)
        Pitch = (Pitch >> 3) + 1;
    else
        Pitch = (Pitch >> 3);

    return Pitch;
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
       if (VBoxHGSMIIsSupported (pContext))
       {
           pContext->u.primary.IOPortHost = (RTIOPORT)VGA_PORT_HGSMI_HOST;
           pContext->u.primary.IOPortGuest = (RTIOPORT)VGA_PORT_HGSMI_GUEST;

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

/* driver callbacks */

NTSTATUS DxgkDdiAddDevice(
    IN CONST PDEVICE_OBJECT PhysicalDeviceObject,
    OUT PVOID *MiniportDeviceContext
    )
{
    /* The DxgkDdiAddDevice function should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", pdo(0x%x)\n", PhysicalDeviceObject));
    NTSTATUS Status = STATUS_UNSUCCESSFUL;

    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)vboxWddmMemAllocZero(sizeof (DEVICE_EXTENSION));
    if (pContext)
    {
        pContext->pPDO = PhysicalDeviceObject;
        *MiniportDeviceContext = pContext;
    }
    else
    {
        Status  = STATUS_INSUFFICIENT_RESOURCES;
        drprintf(("VBoxVideoWddm: ERROR, failed to create context\n"));
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), pContext(0x%x)\n", Status, pContext));

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

    if ( ARGUMENT_PRESENT(MiniportDeviceContext) &&
        ARGUMENT_PRESENT(DxgkInterface) &&
        ARGUMENT_PRESENT(DxgkStartInfo) &&
        ARGUMENT_PRESENT(NumberOfVideoPresentSources),
        ARGUMENT_PRESENT(NumberOfChildren)
        )
    {
        PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)MiniportDeviceContext;

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
                VBoxSetupDisplaysHGSMI(pContext, AdapterMemorySize);
                if ((pContext)->u.primary.bHGSMI)
                {
                    drprintf(("VBoxVideoWddm: using HGSMI\n"));
                    *NumberOfVideoPresentSources = pContext->u.primary.cDisplays;
                    *NumberOfChildren = pContext->u.primary.cDisplays;
                    dprintf(("VBoxVideoWddm: sources(%d), children(%d)\n", *NumberOfVideoPresentSources, *NumberOfChildren));
                }
                else
                {
                    drprintf(("VBoxVideoWddm: HGSMI failed to initialize, returning err\n"));
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
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS DxgkDdiRemoveDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS DxgkDdiDispatchIoRequest(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG VidPnSourceId,
    IN PVIDEO_REQUEST_PACKET VideoRequestPacket
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

BOOLEAN DxgkDdiInterruptRoutine(
    IN CONST PVOID MiniportDeviceContext,
    IN ULONG MessageNumber
    )
{
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;
    BOOLEAN bOur = FALSE;
    if (pDevExt->u.primary.pHostFlags) /* If HGSMI is enabled at all. */
    {
        VBOXSHGSMILIST CtlList;
        VBOXSHGSMILIST DmaCmdList;
        vboxSHGSMIListInit(&CtlList);
        vboxSHGSMIListInit(&DmaCmdList);
        do
        {
            uint32_t flags = pDevExt->u.primary.pHostFlags->u32HostFlags;
            if (flags & HGSMIHOSTFLAGS_GCOMMAND_COMPLETED)
            {
                bOur = TRUE;
                /* read the command offset */
                HGSMIOFFSET offCmd = VBoxHGSMIGuestRead(pDevExt);
                Assert(offCmd != HGSMIOFFSET_VOID);
                if (offCmd != HGSMIOFFSET_VOID)
                {
                    VBOXWDDM_HGSMICMD_TYPE enmType = vboxWddmHgsmiGetCmdTypeFromOffset(pDevExt, offCmd);
                    PVBOXSHGSMILIST pList;
                    HGSMIHEAP * pHeap = NULL;
                    switch (enmType)
                    {
                        case VBOXWDDM_HGSMICMD_TYPE_DMACMD:
                            pList = &DmaCmdList;
                            pHeap = &pDevExt->u.primary.Vdma.CmdHeap;
                            break;
                        case VBOXWDDM_HGSMICMD_TYPE_CTL:
                            pList = &CtlList;
                            pHeap = &pDevExt->u.primary.hgsmiAdapterHeap;
                            break;
                        default:
                            AssertBreakpoint();
                    }

                    if (pHeap)
                    {
                        int rc = VBoxSHGSMICommandProcessCompletion (pHeap, offCmd, TRUE /*bool bIrq*/ , pList);
                        AssertRC(rc);
                    }
                }
            }
            else if (flags & HGSMIHOSTFLAGS_COMMANDS_PENDING)
            {
                bOur = TRUE;
                AssertBreakpoint();
                /* @todo: FIXME: implement !!! */
            }
            else if (flags & HGSMIHOSTFLAGS_IRQ)
            {
                bOur = TRUE;
                AssertBreakpoint();
                /* unknown command */
            }
            else
                break;
        } while (1);

        if (!vboxSHGSMIListIsEmpty(&CtlList))
            vboxSHGSMIListCat(&pDevExt->CtlList, &CtlList);

        if (!vboxSHGSMIListIsEmpty(&DmaCmdList))
            vboxSHGSMIListCat(&pDevExt->DmaCmdList, &DmaCmdList);

        if (pDevExt->bSetNotifyDxDpc)
        {
            pDevExt->bNotifyDxDpc = TRUE;
            pDevExt->bSetNotifyDxDpc = FALSE;
        }

        if (bOur)
            HGSMIClearIrq (pDevExt);
    }

    return bOur;
}


typedef struct VBOXWDDM_DPCDATA
{
    VBOXSHGSMILIST CtlList;
    VBOXSHGSMILIST DmaCmdList;
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

    vboxSHGSMICmdListDetach2List(&pdc->pDevExt->CtlList, &pdc->data.CtlList);
    vboxSHGSMICmdListDetach2List(&pdc->pDevExt->DmaCmdList, &pdc->data.DmaCmdList);
    pdc->data.bNotifyDpc = pdc->pDevExt->bNotifyDxDpc;
    pdc->pDevExt->bNotifyDxDpc = FALSE;
    return TRUE;
}

VOID DxgkDdiDpcRoutine(
    IN CONST PVOID  MiniportDeviceContext
    )
{
    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;

    VBOXWDDM_DPCDATA dpcData = {0};
    BOOLEAN bRet;

    /* get DPC data at IRQL */
    NTSTATUS Status = pDevExt->u.primary.DxgkInterface.DxgkCbSynchronizeExecution(
            pDevExt->u.primary.DxgkInterface.DeviceHandle,
            vboxWddmGetDPCDataCallback,
            &dpcData,
            0,
            &bRet);
    Assert(Status == STATUS_SUCCESS);

    if (!vboxSHGSMIListIsEmpty(&dpcData.CtlList))
    {
        int rc = VBoxSHGSMICommandPostprocessCompletion (&pDevExt->u.primary.hgsmiAdapterHeap, &dpcData.CtlList);
        AssertRC(rc);
    }

    if (!vboxSHGSMIListIsEmpty(&dpcData.DmaCmdList))
    {
        int rc = VBoxSHGSMICommandPostprocessCompletion (&pDevExt->u.primary.Vdma.CmdHeap, &dpcData.DmaCmdList);
        AssertRC(rc);
    }

    if (dpcData.bNotifyDpc)
        pDevExt->u.primary.DxgkInterface.DxgkCbNotifyDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
}

NTSTATUS DxgkDdiQueryChildRelations(
    IN CONST PVOID MiniportDeviceContext,
    IN OUT PDXGK_CHILD_DESCRIPTOR ChildRelations,
    IN ULONG ChildRelationsSize
    )
{
    /* The DxgkDdiQueryChildRelations function should be made pageable. */
    PAGED_CODE();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));
    for (int i = 0; i < pDevExt->u.primary.cDisplays; ++i)
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
    return STATUS_NOT_IMPLEMENTED;
}

VOID DxgkDdiResetDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* DxgkDdiResetDevice can be called at any IRQL, so it must be in nonpageable memory.  */
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
    dfprintf(("<== "__FUNCTION__ "\n"));
}

NTSTATUS DxgkDdiQueryInterface(
    IN CONST PVOID MiniportDeviceContext,
    IN PQUERY_INTERFACE QueryInterface
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

VOID DxgkDdiControlEtwLogging(
    IN BOOLEAN  Enable,
    IN ULONG  Flags,
    IN UCHAR  Level
    )
{

}

NTSTATUS APIENTRY DxgkDdiQueryAdapterInfo(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_QUERYADAPTERINFO*  pQueryAdapterInfo)
{
    /* The DxgkDdiQueryAdapterInfo should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));
    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;

    switch (pQueryAdapterInfo->Type)
    {
        case DXGKQAITYPE_DRIVERCAPS:
        {
            DXGK_DRIVERCAPS *pCaps = (DXGK_DRIVERCAPS*)pQueryAdapterInfo->pOutputData;

            pCaps->HighestAcceptableAddress.HighPart = 0x0;
            pCaps->HighestAcceptableAddress.LowPart = 0xffffffffUL;
            pCaps->MaxAllocationListSlotId = 16;
            pCaps->ApertureSegmentCommitLimit = 0;
            pCaps->MaxPointerWidth = 64;
            pCaps->MaxPointerHeight = 64;
            pCaps->PointerCaps.Value = 3; /* Monochrome , Color*/ /* MaskedColor == Value | 4, dosable for now */
            pCaps->InterruptMessageNumber = 0;
            pCaps->NumberOfSwizzlingRanges = 0;
            /* @todo: need to adjust this for proper 2D Accel support */
            pCaps->MaxOverlays = 0; /* ?? how much should we support? 32 */
            pCaps->GammaRampCaps.Value = 0;
            pCaps->PresentationCaps.Value = 0;
            pCaps->PresentationCaps.NoScreenToScreenBlt = 1;
            pCaps->PresentationCaps.NoOverlapScreenBlt = 1;
            pCaps->MaxQueuedFlipOnVSync = 0; /* do we need it? */
            pCaps->FlipCaps.Value = 0;
            /* ? pCaps->FlipCaps.FlipOnVSyncWithNoWait = 1; */
            pCaps->SchedulingCaps.Value = 0;
            /* we might need it for Aero.
             * Setting this glag means we support DeviceContext, i.e.
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
            if (!pQsOut->pSegmentDescriptor)
            {
                /* we are requested to provide the number of segments we support */
                pQsOut->NbSegment = 1;
            }
            else if (pQsOut->NbSegment != 1)
            {
                AssertBreakpoint();
                drprintf((__FUNCTION__ " NbSegment (%d) != 1\n", pQsOut->NbSegment));
                Status = STATUS_INVALID_PARAMETER;
            }
            else
            {
                /* we are requested to provide segment information */
                pQsOut->pSegmentDescriptor->BaseAddress.QuadPart = 0; /* VBE_DISPI_LFB_PHYSICAL_ADDRESS; */
                pQsOut->pSegmentDescriptor->CpuTranslatedAddress.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS;
                /* make sure the size is page aligned */
                /* @todo: need to setup VBVA buffers and adjust the mem size here */
                pQsOut->pSegmentDescriptor->Size = (pContext->u.primary.cbVRAM - VBVA_ADAPTER_INFORMATION_SIZE - pContext->u.primary.cbMiniportHeap) & (~0xfffUL);
                pQsOut->pSegmentDescriptor->NbOfBanks = 0;
                pQsOut->pSegmentDescriptor->pBankRangeTable = 0;
                pQsOut->pSegmentDescriptor->CommitLimit = pQsOut->pSegmentDescriptor->Size;
                pQsOut->pSegmentDescriptor->Flags.Value = 0;
                pQsOut->pSegmentDescriptor->Flags.CpuVisible = 1;
            }
            pQsOut->PagingBufferSegmentId = 0;
            pQsOut->PagingBufferSize = 1024;
            pQsOut->PagingBufferPrivateDataSize = 0; /* @todo: do we need a private buffer ? */
            break;
        }
        case DXGKQAITYPE_UMDRIVERPRIVATE:
            drprintf((__FUNCTION__ ": we do not support DXGKQAITYPE_UMDRIVERPRIVATE\n"));
            AssertBreakpoint();
            Status = STATUS_NOT_SUPPORTED;
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

NTSTATUS APIENTRY DxgkDdiCreateDevice(
    CONST HANDLE  hAdapter,
    DXGKARG_CREATEDEVICE*  pCreateDevice)
{
    /* DxgkDdiCreateDevice should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));
    NTSTATUS Status = STATUS_SUCCESS;
    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;

    PVBOXWDDM_DEVICE pDevice = (PVBOXWDDM_DEVICE)vboxWddmMemAllocZero(sizeof (VBOXWDDM_DEVICE));
    pDevice->hDevice = pCreateDevice->hDevice;
    pDevice->pAdapter = pContext;
    pDevice->fCreationFlags = pCreateDevice->Flags;

    pDevice->DeviceInfo.AllocationListSize = 1024;
    pDevice->DeviceInfo.DmaBufferSegmentSet = 0;
    pDevice->DeviceInfo.DmaBufferPrivateDataSize = 0;
    pDevice->DeviceInfo.AllocationListSize = 4;
    pDevice->DeviceInfo.PatchLocationListSize = 4;
    pDevice->DeviceInfo.Flags.Value = 0;
    pDevice->DeviceInfo.Flags.GuaranteedDmaBufferContract = 1;

    pCreateDevice->pInfo = &pDevice->DeviceInfo;
    pCreateDevice->hDevice = pDevice;

    dfprintf(("<== "__FUNCTION__ ", context(0x%x), Status(0x%x)\n", hAdapter, Status));

    return Status;
}
NTSTATUS vboxWddmDestroyAllocation(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    PAGED_CODE();

    vboxWddmMemFree(pAllocation);
    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmCreateAllocation(PDEVICE_EXTENSION pDevExt, DXGK_ALLOCATIONINFO* pAllocationInfo)
{
    PAGED_CODE();

    NTSTATUS Status = STATUS_SUCCESS;

    Assert(pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_HEADSIZE());
    if (pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_HEADSIZE())
    {
        PVBOXWDDM_ALLOCINFO pAllocInfo = (PVBOXWDDM_ALLOCINFO)pAllocationInfo->pPrivateDriverData;
        switch (pAllocInfo->enmType)
        {
            case VBOXWDDM_ALLOC_STD_SHAREDPRIMARYSURFACE:
            {
                Assert(pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE));
                if (pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE))
                {
                    PVBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE pInfo = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE);
                    UINT bpp = vboxWddmCalcBitsPerPixel(pInfo->SurfData.Format);
                    Assert(bpp);
                    if (bpp != 0)
                    {
                        PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)vboxWddmMemAllocZero(VBOXWDDM_ALLOCATION_SIZE(VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE));
                        Assert(pAllocation);
                        if (pAllocation)
                        {
                            UINT Pitch = vboxWddmCalcPitch(pInfo->SurfData.Width, bpp);
                            pAllocation->enmType = VBOXWDDM_ALLOC_STD_SHAREDPRIMARYSURFACE;
                            PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pAlloc = VBOXWDDM_ALLOCATION_BODY(pAllocInfo, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
                            memcpy(&pAlloc->AllocInfo, pInfo, sizeof (pAlloc->AllocInfo));

                            pAllocationInfo->pPrivateDriverData = NULL;
                            pAllocationInfo->PrivateDriverDataSize = 0;
                            pAllocationInfo->Alignment = 0;
                            pAllocationInfo->Size = Pitch * pInfo->SurfData.Height;
                            pAllocationInfo->PitchAlignedSize = 0;
                            pAllocationInfo->HintedBank.Value = 0;
                            pAllocationInfo->PreferredSegment.Value = 0;
                            pAllocationInfo->SupportedReadSegmentSet = 1;
                            pAllocationInfo->SupportedWriteSegmentSet = 1;
                            pAllocationInfo->EvictionSegmentSet = 0;
                            pAllocationInfo->MaximumRenamingListLength = 0;
                            pAllocationInfo->hAllocation = pAlloc;
                            pAllocationInfo->Flags.Value = 0;
                            pAllocationInfo->Flags.CpuVisible = 1;
                            pAllocationInfo->pAllocationUsageHint = NULL;
                            pAllocationInfo->AllocationPriority = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
                        }
                        else
                        {
                            drprintf((__FUNCTION__ ": ERROR: failed to create allocation description\n"));
                            Status = STATUS_NO_MEMORY;
                        }
                    }
                    else
                    {
                        drprintf((__FUNCTION__ ": Invalid format (%d)\n", pInfo->SurfData.Format));
                        Status = STATUS_INVALID_PARAMETER;
                    }
                }
                else
                {
                    drprintf((__FUNCTION__ ": ERROR: PrivateDriverDataSize(%d) less than VBOXWDDM_ALLOC_STD_SHAREDPRIMARYSURFACE cmd size(%d)\n", pAllocationInfo->PrivateDriverDataSize, VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHADOWSURFACE)));
                    Status = STATUS_INVALID_PARAMETER;
                }
                break;
            }
            case VBOXWDDM_ALLOC_STD_SHADOWSURFACE:
            {
                Assert(pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHADOWSURFACE));
                if (pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHADOWSURFACE))
                {
                    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)vboxWddmMemAllocZero(VBOXWDDM_ALLOCATION_SIZE(VBOXWDDM_ALLOCATION_SHADOWSURFACE));
                    Assert(pAllocation);
                    if (pAllocation)
                    {
                        pAllocation->enmType = VBOXWDDM_ALLOC_STD_SHADOWSURFACE;
                        PVBOXWDDM_ALLOCATION_SHADOWSURFACE pAlloc = VBOXWDDM_ALLOCATION_BODY(pAllocInfo, VBOXWDDM_ALLOCATION_SHADOWSURFACE);
                        PVBOXWDDM_ALLOCINFO_SHADOWSURFACE pInfo = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_SHADOWSURFACE);
                        memcpy(&pAlloc->AllocInfo, pInfo, sizeof (pAlloc->AllocInfo));

                        pAllocationInfo->pPrivateDriverData = NULL;
                        pAllocationInfo->PrivateDriverDataSize = 0;
                        pAllocationInfo->Alignment = 0;
                        pAllocationInfo->Size = pInfo->SurfData.Pitch * pInfo->SurfData.Height;
                        pAllocationInfo->PitchAlignedSize = 0;
                        pAllocationInfo->HintedBank.Value = 0;
                        pAllocationInfo->PreferredSegment.Value = 0;
                        pAllocationInfo->SupportedReadSegmentSet = 1;
                        pAllocationInfo->SupportedWriteSegmentSet = 1;
                        pAllocationInfo->EvictionSegmentSet = 0;
                        pAllocationInfo->MaximumRenamingListLength = 0;
                        pAllocationInfo->hAllocation = pAlloc;
                        pAllocationInfo->Flags.Value = 0;
                        pAllocationInfo->Flags.CpuVisible = 1;
                        pAllocationInfo->pAllocationUsageHint = NULL;
                        pAllocationInfo->AllocationPriority = D3DDDI_ALLOCATIONPRIORITY_NORMAL;
                    }
                    else
                    {
                        drprintf((__FUNCTION__ ": ERROR: failed to create allocation description\n"));
                        Status = STATUS_NO_MEMORY;
                    }
                }
                else
                {
                    drprintf((__FUNCTION__ ": ERROR: PrivateDriverDataSize(%d) less than VBOXWDDM_ALLOC_STD_SHADOWSURFACE cmd size(%d)\n", pAllocationInfo->PrivateDriverDataSize, VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHADOWSURFACE)));
                    Status = STATUS_INVALID_PARAMETER;
                }
                break;
            }
            case VBOXWDDM_ALLOC_STD_STAGINGSURFACE:
            {
                /* @todo: impl */
                AssertBreakpoint();
                break;
            }
            default:
                drprintf((__FUNCTION__ ": ERROR: invalid alloc info type(%d)\n", pAllocInfo->enmType));
                Status = STATUS_INVALID_PARAMETER;
                break;
        }
    }
    else
    {
        drprintf((__FUNCTION__ ": ERROR: PrivateDriverDataSize(%d) less than header size(%d)\n", pAllocationInfo->PrivateDriverDataSize, VBOXWDDM_ALLOCINFO_HEADSIZE()));
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

    NTSTATUS Status = STATUS_SUCCESS;

    for (UINT i = 0; i < pCreateAllocation->NumAllocations; ++i)
    {
        Status = vboxWddmCreateAllocation((PDEVICE_EXTENSION)hAdapter, &pCreateAllocation->pAllocationInfo[i]);
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
        {
            drprintf((__FUNCTION__ ": ERROR: vboxWddmCreateAllocation error (0x%x)\n", Status));
            /* note: i-th allocation is expected to be cleared in a fail handling code above */
            for (UINT j = 0; j < i; ++j)
            {
                vboxWddmDestroyAllocation((PDEVICE_EXTENSION)hAdapter, (PVBOXWDDM_ALLOCATION)pCreateAllocation->pAllocationInfo[j].hAllocation);
            }
        }
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
    NTSTATUS Status = STATUS_SUCCESS;

    for (UINT i = 0; i < pDestroyAllocation->NumAllocations; ++i)
    {
        vboxWddmDestroyAllocation((PDEVICE_EXTENSION)hAdapter, (PVBOXWDDM_ALLOCATION)pDestroyAllocation->pAllocationList[i]);
    }

    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

    return Status;
}


NTSTATUS
APIENTRY
DxgkDdiDescribeAllocation(
    CONST HANDLE  hAdapter,
    DXGKARG_DESCRIBEALLOCATION*  pDescribeAllocation)
{
    return STATUS_NOT_IMPLEMENTED;
}

/**
 *
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
                pAllocInfo->enmType = VBOXWDDM_ALLOC_STD_SHAREDPRIMARYSURFACE;
                PVBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE pInfo = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE);
                memcpy(&pInfo->SurfData, pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData, sizeof (pInfo->SurfData));
            }
            pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE);

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
                    pAllocInfo->enmType = VBOXWDDM_ALLOC_STD_SHADOWSURFACE;
                    PVBOXWDDM_ALLOCINFO_SHADOWSURFACE pInfo = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_SHADOWSURFACE);
                    memcpy(&pInfo->SurfData, pGetStandardAllocationDriverData->pCreateShadowSurfaceData, sizeof (pInfo->SurfData));
                }
                pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHADOWSURFACE);

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
                pAllocInfo->enmType = VBOXWDDM_ALLOC_STD_STAGINGSURFACE;
                PVBOXWDDM_ALLOCINFO_STAGINGSURFACE pInfo = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_STAGINGSURFACE);
                memcpy(&pInfo->SurfData, pGetStandardAllocationDriverData->pCreateStagingSurfaceData, sizeof (pInfo->SurfData));
            }
            pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_STAGINGSURFACE);

            pGetStandardAllocationDriverData->ResourcePrivateDriverDataSize = 0;
            break;
        }
//#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN7)
//        case D3DKMDT_STANDARDALLOCATION_GDISURFACE:
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
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiReleaseSwizzlingRange(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RELEASESWIZZLINGRANGE*  pReleaseSwizzlingRange)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiPatch(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PATCH*  pPatch)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSubmitCommand(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SUBMITCOMMAND*  pSubmitCommand)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiPreemptCommand(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_PREEMPTCOMMAND*  pPreemptCommand)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiBuildPagingBuffer(
    CONST HANDLE  hAdapter,
    DXGKARG_BUILDPAGINGBUFFER*  pBuildPagingBuffer)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetPalette(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPALETTE*  pSetPalette
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetPointerPosition(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERPOSITION*  pSetPointerPosition)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetPointerShape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERSHAPE*  pSetPointerShape)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY CALLBACK
DxgkDdiResetFromTimeout(
    CONST HANDLE  hAdapter)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiEscape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_ESCAPE*  pEscape)
{
    PAGED_CODE();

    return STATUS_INVALID_PARAMETER;
}

NTSTATUS
APIENTRY
DxgkDdiCollectDbgInfo(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_COLLECTDBGINFO*  pCollectDbgInfo
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiQueryCurrentFence(
    CONST HANDLE  hAdapter,
    DXGKARG_QUERYCURRENTFENCE*  pCurrentFence)
{
    return STATUS_NOT_IMPLEMENTED;
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
                for (int id = 0; id < pContext->u.primary.cDisplays; id++)
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
                    for (int id = 0; id < pContext->u.primary.cDisplays; id++)
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

    return STATUS_SUCCESS;
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

    PDEVICE_EXTENSION pContext = (PDEVICE_EXTENSION)hAdapter;
    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    NTSTATUS Status = pContext->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    if (Status == STATUS_SUCCESS)
    {
        D3DKMDT_HVIDPNTOPOLOGY hVidPnTopology;
        const DXGK_VIDPNTOPOLOGY_INTERFACE* pVidPnTopologyInterface;
        Status = pVidPnInterface->pfnGetTopology(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, &hVidPnTopology, &pVidPnTopologyInterface);
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
                pNewVidPnPresentPathInfo->ContentTransformation.ScalingSupport.Centered = 1;
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
                pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionType = D3DKMDT_VPPMT_NOPROTECTION;
                pNewVidPnPresentPathInfo->CopyProtection.APSTriggerBits = 0;
                memset(&pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport, 0, sizeof (pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport));
                pNewVidPnPresentPathInfo->CopyProtection.CopyProtectionSupport.NoProtection  = 1;
                pNewVidPnPresentPathInfo->GammaRamp.Type = D3DDDI_GAMMARAMP_DEFAULT;
                pNewVidPnPresentPathInfo->GammaRamp.DataSize = 0;
                Status = pVidPnTopologyInterface->pfnAddPath(hVidPnTopology, pNewVidPnPresentPathInfo);
                if (Status == STATUS_SUCCESS)
                {
                    D3DKMDT_HVIDPNSOURCEMODESET hNewVidPnSourceModeSet;
                    const DXGK_VIDPNSOURCEMODESET_INTERFACE *pVidPnSourceModeSetInterface;
                    Status = pVidPnInterface->pfnCreateNewSourceModeSet(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn,
                                    0, /*__in CONST D3DDDI_VIDEO_PRESENT_SOURCE_ID  VidPnSourceId */
                                    &hNewVidPnSourceModeSet,
                                    &pVidPnSourceModeSetInterface);
                    if (Status == STATUS_SUCCESS)
                    {
                        D3DKMDT_VIDPN_SOURCE_MODE *pNewVidPnSourceModeInfo;
                        Status = pVidPnSourceModeSetInterface->pfnCreateNewModeInfo(hNewVidPnSourceModeSet, &pNewVidPnSourceModeInfo);
                        if (Status == STATUS_SUCCESS)
                        {
                            D3DKMDT_VIDEO_PRESENT_SOURCE_MODE_ID modeId = pNewVidPnSourceModeInfo->Id;
                            pNewVidPnSourceModeInfo->Type = D3DKMDT_RMT_GRAPHICS;
                            /* @todo: should we obtain the default mode from the host? */
                            pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx = 1024;
                            pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cy = 768;
                            pNewVidPnSourceModeInfo->Format.Graphics.VisibleRegionSize = pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize;
                            pNewVidPnSourceModeInfo->Format.Graphics.Stride = pNewVidPnSourceModeInfo->Format.Graphics.PrimSurfSize.cx * 4;
                            pNewVidPnSourceModeInfo->Format.Graphics.PixelFormat = D3DDDIFMT_X8R8G8B8;
                            pNewVidPnSourceModeInfo->Format.Graphics.ColorBasis = D3DKMDT_CB_SRGB;
                            pNewVidPnSourceModeInfo->Format.Graphics.PixelValueAccessMode = D3DKMDT_PVAM_DIRECT;
                            Status = pVidPnSourceModeSetInterface->pfnAddMode(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                            if (Status == STATUS_SUCCESS)
                            {
                                Status = pVidPnSourceModeSetInterface->pfnPinMode(hNewVidPnSourceModeSet, modeId);
                                if (Status == STATUS_SUCCESS)
                                {
                                    D3DKMDT_HVIDPNTARGETMODESET hNewVidPnTargetModeSet;
                                    CONST DXGK_VIDPNTARGETMODESET_INTERFACE *pVidPnTargetModeSetInterface;
                                    Status = pVidPnInterface->pfnCreateNewTargetModeSet(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn,
                                                0, /* __in CONST D3DDDI_VIDEO_PRESENT_TARGET_ID  VidPnTargetId */
                                                &hNewVidPnTargetModeSet,
                                                &pVidPnTargetModeSetInterface);
                                    if (Status == STATUS_SUCCESS)
                                    {
                                        D3DKMDT_VIDPN_TARGET_MODE *pNewVidPnTargetModeInfo;
                                        Status = pVidPnTargetModeSetInterface->pfnCreateNewModeInfo(hNewVidPnTargetModeSet, &pNewVidPnTargetModeInfo);
                                        if (Status == STATUS_SUCCESS)
                                        {
                                            D3DKMDT_VIDEO_PRESENT_TARGET_MODE_ID targetId = pNewVidPnTargetModeInfo->Id;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.VideoStandard = D3DKMDT_VSS_OTHER;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cx = 1024;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize.cy = 768;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.ActiveSize = pNewVidPnTargetModeInfo->VideoSignalInfo.TotalSize;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Numerator = 60;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.VSyncFreq.Denominator = 1;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.HSyncFreq.Numerator = 63 * 768; /* @todo: do we need that? */
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.HSyncFreq.Denominator = 1;
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.PixelRate = 165000; /* ?? */
                                            pNewVidPnTargetModeInfo->VideoSignalInfo.ScanLineOrdering = D3DDDI_VSSLO_PROGRESSIVE;
                                            pNewVidPnTargetModeInfo->Preference = D3DKMDT_MP_PREFERRED;
                                            Status = pVidPnTargetModeSetInterface->pfnAddMode(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                                            if (Status == STATUS_SUCCESS)
                                            {
                                                Status = pVidPnTargetModeSetInterface->pfnPinMode(hNewVidPnTargetModeSet, targetId);
                                                if (Status == STATUS_SUCCESS)
                                                {

                                                }
                                                else
                                                {
                                                    drprintf(("VBoxVideoWddm: pfnPinMode (target) failed Status(0x%x)\n"));
                                                }
                                            }
                                            else
                                            {
                                                drprintf(("VBoxVideoWddm: pfnAddMode (target) failed Status(0x%x)\n"));
                                                pVidPnTargetModeSetInterface->pfnReleaseModeInfo(hNewVidPnTargetModeSet, pNewVidPnTargetModeInfo);
                                                pNewVidPnTargetModeInfo = NULL;
                                            }
                                        }
                                        else
                                        {
                                            drprintf(("VBoxVideoWddm: pfnCreateNewModeInfo (target) failed Status(0x%x)\n"));
                                        }
                                    }
                                    else
                                    {
                                        drprintf(("VBoxVideoWddm: pfnCreateNewTargetModeSet failed Status(0x%x)\n"));
                                    }
                                }
                                else
                                {
                                    drprintf(("VBoxVideoWddm: pfnPinMode failed Status(0x%x)\n"));
                                }
                            }
                            else
                            {
                                drprintf(("VBoxVideoWddm: pfnAddMode failed Status(0x%x)\n"));
                                pVidPnSourceModeSetInterface->pfnReleaseModeInfo(hNewVidPnSourceModeSet, pNewVidPnSourceModeInfo);
                                pNewVidPnSourceModeInfo = NULL;
                            }
                        }
                        else
                        {
                            drprintf(("VBoxVideoWddm: pfnCreateNewModeInfo failed Status(0x%x)\n"));
                        }
                    }
                    else
                    {
                        drprintf(("VBoxVideoWddm: pfnCreateNewSourceModeSet failed Status(0x%x)\n"));
                    }
                }
                else
                {
                    drprintf(("VBoxVideoWddm: pfnAddPath failed Status(0x%x)\n"));
                    pVidPnTopologyInterface->pfnReleasePathInfo(hVidPnTopology, pNewVidPnPresentPathInfo);
                    pNewVidPnPresentPathInfo = NULL;
                }
            }
            else
            {
                drprintf(("VBoxVideoWddm: pfnCreateNewPathInfo failed Status(0x%x)\n"));
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
                    true, /* bool bRebuildTable*/
                    &CbContext.pModes, /* VIDEO_MODE_INFORMATION ** ppModes*/
                    &CbContext.cModes, /* uint32_t * pcModes */
                    &CbContext.iPreferredMode, /* uint32_t * pPreferrableMode*/
                    &CbContext.pResolutions, /* D3DKMDT_2DREGION **ppResolutions */
                    &CbContext.cResolutions /* uint32_t * pcResolutions */);
            Assert(CbContext.cModes);
            Assert(CbContext.cModes > CbContext.iPreferredMode);
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

    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetVidPnSourceAddress(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETVIDPNSOURCEADDRESS*  pSetVidPnSourceAddress
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetVidPnSourceVisibility(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETVIDPNSOURCEVISIBILITY* pSetVidPnSourceVisibility
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiCommitVidPn(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_COMMITVIDPN* CONST  pCommitVidPnArg
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateActiveVidPnPresentPath(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_UPDATEACTIVEVIDPNPRESENTPATH* CONST  pUpdateActiveVidPnPresentPathArg
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendMonitorModes(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDMONITORMODES* CONST  pRecommendMonitorModesArg
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiRecommendVidPnTopology(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_RECOMMENDVIDPNTOPOLOGY* CONST  pRecommendVidPnTopologyArg
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiGetScanLine(
    CONST HANDLE  hAdapter,
    DXGKARG_GETSCANLINE*  pGetScanLine)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiStopCapture(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_STOPCAPTURE*  pStopCapture)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiControlInterrupt(
    CONST HANDLE hAdapter,
    CONST DXGK_INTERRUPT_TYPE InterruptType,
    BOOLEAN Enable
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiCreateOverlay(
    CONST HANDLE  hAdapter,
    DXGKARG_CREATEOVERLAY  *pCreateOverlay)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyDevice(
    CONST HANDLE  hDevice)
{
    /* DxgkDdiDestroyDevice should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", hDevice(0x%x)\n", hDevice));

    vboxWddmMemFree(hDevice);

    dfprintf(("<== "__FUNCTION__ ", \n"));

    return STATUS_UNSUCCESSFUL;
}

NTSTATUS
APIENTRY
DxgkDdiOpenAllocation(
    CONST HANDLE  hDevice,
    CONST DXGKARG_OPENALLOCATION  *pOpenAllocation)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiCloseAllocation(
    CONST HANDLE  hDevice,
    CONST DXGKARG_CLOSEALLOCATION*  pCloseAllocation)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiRender(
    CONST HANDLE  hContext,
    DXGKARG_RENDER  *pRender)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiPresent(
    CONST HANDLE  hContext,
    DXGKARG_PRESENT  *pPresent)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_UPDATEOVERLAY  *pUpdateOverlay)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiFlipOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_FLIPOVERLAY  *pFlipOverlay)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyOverlay(
    CONST HANDLE  hOverlay)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiCreateContext(
    CONST HANDLE  hDevice,
    DXGKARG_CREATECONTEXT  *pCreateContext)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyContext(
    CONST HANDLE  hContext)
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiLinkDevice(
    __in CONST PDEVICE_OBJECT  PhysicalDeviceObject,
    __in CONST PVOID  MiniportDeviceContext,
    __inout PLINKED_DEVICE  LinkedDevice
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiSetDisplayPrivateDriverFormat(
    CONST HANDLE  hAdapter,
    /*CONST*/ DXGKARG_SETDISPLAYPRIVATEDRIVERFORMAT*  pSetDisplayPrivateDriverFormat
    )
{
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT DriverObject,
    IN PUNICODE_STRING RegistryPath
    )
{
    dprintf(("VBoxVideoWddm::DriverEntry. Built %s %s\n", __DATE__, __TIME__));

    DRIVER_INITIALIZATION_DATA DriverInitializationData = {'\0'};

    PAGED_CODE();

    if (! ARGUMENT_PRESENT(DriverObject) ||
        ! ARGUMENT_PRESENT(RegistryPath))
    {
        return STATUS_INVALID_PARAMETER;
    }

    // Fill in the DriverInitializationData structure and call DxgkInitialize()
    DriverInitializationData.Version  = DXGKDDI_INTERFACE_VERSION;

    DriverInitializationData.DxgkDdiAddDevice  = DxgkDdiAddDevice;
    DriverInitializationData.DxgkDdiStartDevice  = DxgkDdiStartDevice;
    DriverInitializationData.DxgkDdiStopDevice  = DxgkDdiStopDevice;
    DriverInitializationData.DxgkDdiRemoveDevice  = DxgkDdiRemoveDevice;
    DriverInitializationData.DxgkDdiDispatchIoRequest  = DxgkDdiDispatchIoRequest;
    DriverInitializationData.DxgkDdiInterruptRoutine  = DxgkDdiInterruptRoutine;
    DriverInitializationData.DxgkDdiDpcRoutine  = DxgkDdiDpcRoutine;
    DriverInitializationData.DxgkDdiQueryChildRelations  = DxgkDdiQueryChildRelations;
    DriverInitializationData.DxgkDdiQueryChildStatus   = DxgkDdiQueryChildStatus;
    DriverInitializationData.DxgkDdiQueryDeviceDescriptor  = DxgkDdiQueryDeviceDescriptor;
    DriverInitializationData.DxgkDdiSetPowerState  = DxgkDdiSetPowerState;
    DriverInitializationData.DxgkDdiNotifyAcpiEvent  = DxgkDdiNotifyAcpiEvent;
    DriverInitializationData.DxgkDdiResetDevice  = DxgkDdiResetDevice;
    DriverInitializationData.DxgkDdiUnload  = DxgkDdiUnload;
    DriverInitializationData.DxgkDdiQueryInterface  = DxgkDdiQueryInterface;
    DriverInitializationData.DxgkDdiControlEtwLogging  = DxgkDdiControlEtwLogging;

    DriverInitializationData.DxgkDdiQueryAdapterInfo  = DxgkDdiQueryAdapterInfo;
    DriverInitializationData.DxgkDdiCreateDevice  = DxgkDdiCreateDevice;
    DriverInitializationData.DxgkDdiCreateAllocation  = DxgkDdiCreateAllocation;
    DriverInitializationData.DxgkDdiDestroyAllocation  = DxgkDdiDestroyAllocation ;

    DriverInitializationData.DxgkDdiDescribeAllocation  = DxgkDdiDescribeAllocation;
    DriverInitializationData.DxgkDdiGetStandardAllocationDriverData = DxgkDdiGetStandardAllocationDriverData;

    DriverInitializationData.DxgkDdiAcquireSwizzlingRange  = DxgkDdiAcquireSwizzlingRange;
    DriverInitializationData.DxgkDdiReleaseSwizzlingRange  = DxgkDdiReleaseSwizzlingRange;

    DriverInitializationData.DxgkDdiPatch  = DxgkDdiPatch;

    DriverInitializationData.DxgkDdiSubmitCommand  = DxgkDdiSubmitCommand;
    DriverInitializationData.DxgkDdiPreemptCommand  = DxgkDdiPreemptCommand;
    DriverInitializationData.DxgkDdiBuildPagingBuffer  = DxgkDdiBuildPagingBuffer;

    DriverInitializationData.DxgkDdiSetPalette  = DxgkDdiSetPalette;
    DriverInitializationData.DxgkDdiSetPointerPosition  = DxgkDdiSetPointerPosition;
    DriverInitializationData.DxgkDdiSetPointerShape  = DxgkDdiSetPointerShape;

    DriverInitializationData.DxgkDdiResetFromTimeout  = DxgkDdiResetFromTimeout;

    DriverInitializationData.DxgkDdiEscape  = DxgkDdiEscape;

    DriverInitializationData.DxgkDdiCollectDbgInfo  = DxgkDdiCollectDbgInfo;

    DriverInitializationData.DxgkDdiQueryCurrentFence  = DxgkDdiQueryCurrentFence;

    DriverInitializationData.DxgkDdiIsSupportedVidPn  = DxgkDdiIsSupportedVidPn;
    DriverInitializationData.DxgkDdiRecommendFunctionalVidPn  = DxgkDdiRecommendFunctionalVidPn;
    DriverInitializationData.DxgkDdiEnumVidPnCofuncModality  = DxgkDdiEnumVidPnCofuncModality;
    DriverInitializationData.DxgkDdiSetVidPnSourceAddress  = DxgkDdiSetVidPnSourceAddress;
    DriverInitializationData.DxgkDdiSetVidPnSourceVisibility  = DxgkDdiSetVidPnSourceVisibility;
    DriverInitializationData.DxgkDdiCommitVidPn  = DxgkDdiCommitVidPn;
    DriverInitializationData.DxgkDdiUpdateActiveVidPnPresentPath  = DxgkDdiUpdateActiveVidPnPresentPath;

    DriverInitializationData.DxgkDdiRecommendMonitorModes  = DxgkDdiRecommendMonitorModes;
    DriverInitializationData.DxgkDdiRecommendVidPnTopology  = DxgkDdiRecommendVidPnTopology;

    DriverInitializationData.DxgkDdiGetScanLine  = DxgkDdiGetScanLine;

    DriverInitializationData.DxgkDdiStopCapture  = DxgkDdiStopCapture;

    DriverInitializationData.DxgkDdiControlInterrupt  = DxgkDdiControlInterrupt;

    DriverInitializationData.DxgkDdiCreateOverlay  = DxgkDdiCreateOverlay;

    DriverInitializationData.DxgkDdiDestroyDevice  = DxgkDdiDestroyDevice;

    DriverInitializationData.DxgkDdiOpenAllocation  = DxgkDdiOpenAllocation;
    DriverInitializationData.DxgkDdiCloseAllocation  = DxgkDdiCloseAllocation;

    DriverInitializationData.DxgkDdiRender  = DxgkDdiRender;
    DriverInitializationData.DxgkDdiPresent  = DxgkDdiPresent;

    DriverInitializationData.DxgkDdiUpdateOverlay  = DxgkDdiUpdateOverlay;
    DriverInitializationData.DxgkDdiFlipOverlay  = DxgkDdiFlipOverlay;
    DriverInitializationData.DxgkDdiDestroyOverlay  = DxgkDdiDestroyOverlay;

    DriverInitializationData.DxgkDdiCreateContext  = DxgkDdiCreateContext;
    DriverInitializationData.DxgkDdiDestroyContext  = DxgkDdiDestroyContext;

    DriverInitializationData.DxgkDdiLinkDevice  = DxgkDdiLinkDevice;
    DriverInitializationData.DxgkDdiSetDisplayPrivateDriverFormat  = DxgkDdiSetDisplayPrivateDriverFormat;

//#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN7)
//    DriverInitializationData.DxgkDdiRenderKm  = D3DDDIRenderKm;
//    DriverInitializationData.DxgkDdiRestartFromTimeout  = D3DDDIRestartFromTimeout;
//    DriverInitializationData.DxgkDdiSetVidPnSourceVisibility  = DxgkDdiSetVidPnSourceVisibility;
//    DriverInitializationData.DxgkDdiUpdateActiveVidPnPresentPath  = D3DDDIUpdateActiveVidPnPresentPath;
//    DriverInitializationData.DxgkDdiQueryVidPnHWCapability  = D3DDDI DxgkDdiQueryVidPnHWCapability;
//#endif

    return DxgkInitialize(DriverObject,
                          RegistryPath,
                          &DriverInitializationData);
}
