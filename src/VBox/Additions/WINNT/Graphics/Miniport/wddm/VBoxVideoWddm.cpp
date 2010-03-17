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

NTSTATUS vboxWddmGhDisplayPostInfoScreen (PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXVIDEOOFFSET offVram = pAllocation->offVram;
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimaryInfo = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
    /* Issue the screen info command. */
    void *p = vboxHGSMIBufferAlloc (pDevExt,
                                      sizeof (VBVAINFOSCREEN),
                                      HGSMI_CH_VBVA,
                                      VBVA_INFO_SCREEN);
    Assert(p);
    if (p)
    {
        VBVAINFOSCREEN *pScreen = (VBVAINFOSCREEN *)p;

        pScreen->u32ViewIndex    = pPrimaryInfo->VidPnSourceId;
        pScreen->i32OriginX      = 0;
        pScreen->i32OriginY      = 0;
        pScreen->u32StartOffset  = (uint32_t)offVram;
        pScreen->u32LineSize     = pAllocation->u.SurfInfo.pitch;
        pScreen->u32Width        = pAllocation->u.SurfInfo.width;
        pScreen->u32Height       = pAllocation->u.SurfInfo.height;
        pScreen->u16BitsPerPixel = (uint16_t)pAllocation->u.SurfInfo.bpp;
        pScreen->u16Flags        = VBVA_SCREEN_F_ACTIVE;

        vboxHGSMIBufferSubmit (pDevExt, p);

        vboxHGSMIBufferFree (pDevExt, p);
    }

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmGhDisplayPostInfoView (PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    VBOXVIDEOOFFSET offVram = pAllocation->offVram;
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimaryInfo = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
    /* Issue the screen info command. */
    void *p = vboxHGSMIBufferAlloc (pDevExt,
                                      sizeof (VBVAINFOVIEW),
                                      HGSMI_CH_VBVA,
                                      VBVA_INFO_VIEW);
    Assert(p);
    if (p)
    {
        VBVAINFOVIEW *pView = (VBVAINFOVIEW *)p;

        pView->u32ViewIndex     = pPrimaryInfo->VidPnSourceId;
        pView->u32ViewOffset    = (uint32_t)offVram;
        pView->u32ViewSize      = vboxWddmVramReportedSegmentSize(pDevExt)/pDevExt->cSources;

        pView->u32MaxScreenSize = pView->u32ViewSize;

        vboxHGSMIBufferSubmit (pDevExt, p);

        vboxHGSMIBufferFree (pDevExt, p);
    }

    return STATUS_SUCCESS;
}

NTSTATUS vboxWddmGhDisplaySetMode (PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimaryInfo = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
    if (pPrimaryInfo->VidPnSourceId)
        return STATUS_SUCCESS;

    if (VBoxVideoSetCurrentModePerform(pDevExt, pAllocation->u.SurfInfo.width,
            pAllocation->u.SurfInfo.height, pAllocation->u.SurfInfo.bpp))
        return STATUS_SUCCESS;

    AssertBreakpoint();
    drprintf((__FUNCTION__": VBoxVideoSetCurrentModePerform failed\n"));
    return STATUS_UNSUCCESSFUL;
}


NTSTATUS vboxWddmGhDisplaySetInfo(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_SOURCE pSource)
{
    PVBOXWDDM_ALLOCATION pAllocation = pSource->pAllocation;
    VBOXVIDEOOFFSET offVram = vboxWddmValidatePrimary(pAllocation);
    Assert(offVram != VBOXVIDEOOFFSET_VOID);
    if (offVram == VBOXVIDEOOFFSET_VOID)
        return STATUS_INVALID_PARAMETER;

    /*
     * Set the current mode into the hardware.
     */
    NTSTATUS Status = vboxWddmGhDisplaySetMode (pDevExt, pAllocation);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        Status = vboxWddmGhDisplayPostInfoView (pDevExt, pAllocation);
        Assert(Status == STATUS_SUCCESS);
        if (Status == STATUS_SUCCESS)
        {
            Status = vboxWddmGhDisplayPostInfoScreen (pDevExt, pAllocation);
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

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;

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

    vboxVDbgBreakFv();

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
                    *NumberOfVideoPresentSources = pContext->cSources;
                    *NumberOfChildren = pContext->cSources;
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
    /* The DxgkDdiStopDevice function should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    return STATUS_SUCCESS;
}

NTSTATUS DxgkDdiRemoveDevice(
    IN CONST PVOID MiniportDeviceContext
    )
{
    /* DxgkDdiRemoveDevice should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

    vboxVDbgBreakF();

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
    dfprintf(("==> "__FUNCTION__ ", context(0x%p), msg(0x%x)\n", MiniportDeviceContext, MessageNumber));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)MiniportDeviceContext;
    BOOLEAN bOur = FALSE;
    BOOLEAN bNeedDpc = FALSE;
    if (pDevExt->u.primary.pHostFlags) /* If HGSMI is enabled at all. */
    {
        VBOXSHGSMILIST CtlList;
        VBOXSHGSMILIST DmaCmdList;
        vboxSHGSMIListInit(&CtlList);
        vboxSHGSMIListInit(&DmaCmdList);

        uint32_t flags = pDevExt->u.primary.pHostFlags->u32HostFlags;
        bOur = (flags & HGSMIHOSTFLAGS_IRQ);
        do
        {
            if (flags & HGSMIHOSTFLAGS_GCOMMAND_COMPLETED)
            {
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
                AssertBreakpoint();
                /* @todo: FIXME: implement !!! */
            }
            else
                break;

            flags = pDevExt->u.primary.pHostFlags->u32HostFlags;
        } while (1);

        if (!vboxSHGSMIListIsEmpty(&CtlList))
        {
            vboxSHGSMIListCat(&pDevExt->CtlList, &CtlList);
            bNeedDpc = TRUE;
        }

        if (!vboxSHGSMIListIsEmpty(&DmaCmdList))
        {
            vboxSHGSMIListCat(&pDevExt->DmaCmdList, &DmaCmdList);
            bNeedDpc = TRUE;
        }

        if (pDevExt->bSetNotifyDxDpc)
        {
            Assert(bNeedDpc == TRUE);
            pDevExt->bNotifyDxDpc = TRUE;
            pDevExt->bSetNotifyDxDpc = FALSE;
            bNeedDpc = TRUE;
        }

        if (bOur)
        {
            HGSMIClearIrq (pDevExt);
#ifdef DEBUG_misha
            /* this is not entirely correct since host may concurrently complete some commands and raise a new IRQ while we are here,
             * still this allows to check that the host flags are correctly cleared after the ISR */
            Assert(pDevExt->u.primary.pHostFlags);
            uint32_t flags = pDevExt->u.primary.pHostFlags->u32HostFlags;
            Assert(flags == 0);
#endif
            BOOLEAN bDpcQueued = pDevExt->u.primary.DxgkInterface.DxgkCbQueueDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);
            Assert(bDpcQueued);
        }
    }

    dfprintf(("<== "__FUNCTION__ ", context(0x%p), bOur(0x%x)\n", MiniportDeviceContext, (ULONG)bOur));

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
    dfprintf(("==> "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));

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
        int rc = VBoxSHGSMICommandPostprocessCompletion (&pDevExt->u.primary.hgsmiAdapterHeap, &context.data.CtlList);
        AssertRC(rc);
    }

    if (!vboxSHGSMIListIsEmpty(&context.data.DmaCmdList))
    {
        int rc = VBoxSHGSMICommandPostprocessCompletion (&pDevExt->u.primary.Vdma.CmdHeap, &context.data.DmaCmdList);
        AssertRC(rc);
    }

    if (context.data.bNotifyDpc)
        pDevExt->u.primary.DxgkInterface.DxgkCbNotifyDpc(pDevExt->u.primary.DxgkInterface.DeviceHandle);

    dfprintf(("<== "__FUNCTION__ ", context(0x%p)\n", MiniportDeviceContext));
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
    Assert(ChildRelationsSize == (pDevExt->cSources + 1)*sizeof(DXGK_CHILD_DESCRIPTOR));
    for (UINT i = 0; i < pDevExt->cSources; ++i)
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
    vboxVDbgBreakF();

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

    AssertBreakpoint();

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));
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
            if (!pQsOut->pSegmentDescriptor)
            {
                /* we are requested to provide the number of segments we support */
                pQsOut->NbSegment = 2;
            }
            else if (pQsOut->NbSegment != 2)
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
                pDr->Size = vboxWddmVramReportedSegmentSize(pContext);
                pDr->NbOfBanks = 0;
                pDr->pBankRangeTable = 0;
                pDr->CommitLimit = pDr->Size;
                pDr->Flags.Value = 0;
                pDr->Flags.CpuVisible = 1;

                ++pDr;
                /* create cpu-invisible segment of the same size */
                pDr->BaseAddress.QuadPart = 0;
                pDr->CpuTranslatedAddress.QuadPart = 0;
                /* make sure the size is page aligned */
                /* @todo: need to setup VBVA buffers and adjust the mem size here */
                pDr->Size = vboxWddmVramReportedSegmentSize(pContext);
                pDr->NbOfBanks = 0;
                pDr->pBankRangeTable = 0;
                pDr->CommitLimit = pDr->Size;
                pDr->Flags.Value = 0;

                pQsOut->PagingBufferSegmentId = 0;
                pQsOut->PagingBufferSize = 1024;
                pQsOut->PagingBufferPrivateDataSize = 0; /* @todo: do we need a private buffer ? */
            }
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
NTSTATUS vboxWddmDestroyAllocation(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    PAGED_CODE();

    switch (pAllocation->enmType)
    {
        case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
        {
            PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pAlloc = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
            if (pAlloc->bAssigned)
            {
                /* @todo: do we need to notify host? */
                vboxWddmAssignPrimary(pDevExt, &pDevExt->aSources[pAlloc->VidPnSourceId], NULL, pAlloc->VidPnSourceId);
            }
            break;
        }
        default:
            break;
    }
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
            case VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE:
            {
                Assert(pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE));
                if (pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE))
                {
                    if (pAllocInfo->u.SurfInfo.bpp != 0)
                    {
                        PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)vboxWddmMemAllocZero(VBOXWDDM_ALLOCATION_SIZE(VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE));
                        Assert(pAllocation);
                        if (pAllocation)
                        {
                            pAllocation->enmType = VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE;
                            pAllocation->offVram = VBOXVIDEOOFFSET_VOID;
                            pAllocation->u.SurfInfo = pAllocInfo->u.SurfInfo;
                            PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pAlloc = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
                            PVBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE pAllocI = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE);
                            pAlloc->RefreshRate = pAllocI->RefreshRate;
                            pAlloc->VidPnSourceId = pAllocI->VidPnSourceId;
//                            pAlloc->offAddress = VBOXVIDEOOFFSET_VOID;
                            pAlloc->bVisible = FALSE;

                            pAllocationInfo->pPrivateDriverData = NULL;
                            pAllocationInfo->PrivateDriverDataSize = 0;
                            pAllocationInfo->Alignment = 0;
                            pAllocationInfo->Size = pAllocInfo->u.SurfInfo.pitch * pAllocInfo->u.SurfInfo.height;
                            pAllocationInfo->PitchAlignedSize = 0;
                            pAllocationInfo->HintedBank.Value = 0;
                            pAllocationInfo->PreferredSegment.Value = 0;
                            pAllocationInfo->SupportedReadSegmentSet = 1;
                            pAllocationInfo->SupportedWriteSegmentSet = 1;
                            pAllocationInfo->EvictionSegmentSet = 0;
                            pAllocationInfo->MaximumRenamingListLength = 0;
                            pAllocationInfo->hAllocation = pAllocation;
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
                        drprintf((__FUNCTION__ ": Invalid format (%d)\n", pAllocInfo->u.SurfInfo.format));
                        Status = STATUS_INVALID_PARAMETER;
                    }
                }
                else
                {
                    drprintf((__FUNCTION__ ": ERROR: PrivateDriverDataSize(%d) less than VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE cmd size(%d)\n", pAllocationInfo->PrivateDriverDataSize, VBOXWDDM_ALLOCINFO_SIZE(VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE)));
                    Status = STATUS_INVALID_PARAMETER;
                }
                break;
            }
            case VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE:
            case VBOXWDDM_ALLOC_TYPE_STD_STAGINGSURFACE:
            {
                Assert(pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_HEADSIZE());
                if (pAllocationInfo->PrivateDriverDataSize >= VBOXWDDM_ALLOCINFO_HEADSIZE())
                {
                    PVBOXWDDM_ALLOCATION pAllocation = (PVBOXWDDM_ALLOCATION)vboxWddmMemAllocZero(VBOXWDDM_ALLOCATION_HEADSIZE());
                    Assert(pAllocation);
                    if (pAllocation)
                    {
                        pAllocation->enmType = pAllocInfo->enmType;
                        pAllocation->offVram = VBOXVIDEOOFFSET_VOID;
                        pAllocation->u.SurfInfo = pAllocInfo->u.SurfInfo;

                        pAllocationInfo->pPrivateDriverData = NULL;
                        pAllocationInfo->PrivateDriverDataSize = 0;
                        pAllocationInfo->Alignment = 0;
                        pAllocationInfo->Size = pAllocInfo->u.SurfInfo.pitch * pAllocInfo->u.SurfInfo.height;
                        pAllocationInfo->PitchAlignedSize = 0;
                        pAllocationInfo->HintedBank.Value = 0;
                        pAllocationInfo->PreferredSegment.Value = 0;
                        pAllocationInfo->SupportedReadSegmentSet = 1;
                        pAllocationInfo->SupportedWriteSegmentSet = 1;
                        pAllocationInfo->EvictionSegmentSet = 0;
                        pAllocationInfo->MaximumRenamingListLength = 0;
                        pAllocationInfo->hAllocation = pAllocation;
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
                    drprintf((__FUNCTION__ ": ERROR: PrivateDriverDataSize(%d) less than cmd size(%d)\n", pAllocationInfo->PrivateDriverDataSize, VBOXWDDM_ALLOCINFO_HEADSIZE()));
                    Status = STATUS_INVALID_PARAMETER;
                }
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

    vboxVDbgBreakFv();

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

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;

    for (UINT i = 0; i < pDestroyAllocation->NumAllocations; ++i)
    {
        vboxWddmDestroyAllocation((PDEVICE_EXTENSION)hAdapter, (PVBOXWDDM_ALLOCATION)pDestroyAllocation->pAllocationList[i]);
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
    pDescribeAllocation->Width = pAllocation->u.SurfInfo.width;
    pDescribeAllocation->Height = pAllocation->u.SurfInfo.height;
    pDescribeAllocation->Format = pAllocation->u.SurfInfo.format;
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
                pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE;
                pAllocInfo->u.SurfInfo.width = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Width;
                pAllocInfo->u.SurfInfo.height = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Height;
                pAllocInfo->u.SurfInfo.format = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Format;
                pAllocInfo->u.SurfInfo.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->u.SurfInfo.format);
                pAllocInfo->u.SurfInfo.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->Width, pAllocInfo->u.SurfInfo.bpp);
                PVBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE pInfo = VBOXWDDM_ALLOCINFO_BODY(pAllocInfo, VBOXWDDM_ALLOCINFO_SHAREDPRIMARYSURFACE);
                pInfo->RefreshRate = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->RefreshRate;
                pInfo->VidPnSourceId = pGetStandardAllocationDriverData->pCreateSharedPrimarySurfaceData->VidPnSourceId;
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
                    pAllocInfo->enmType = VBOXWDDM_ALLOC_TYPE_STD_SHADOWSURFACE;
                    pAllocInfo->u.SurfInfo.width = pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width;
                    pAllocInfo->u.SurfInfo.height = pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Height;
                    pAllocInfo->u.SurfInfo.format = pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Format;
                    pAllocInfo->u.SurfInfo.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->u.SurfInfo.format);
                    pAllocInfo->u.SurfInfo.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Width, pAllocInfo->u.SurfInfo.bpp);

                    pGetStandardAllocationDriverData->pCreateShadowSurfaceData->Pitch = pAllocInfo->u.SurfInfo.pitch;
                }
                pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = VBOXWDDM_ALLOCINFO_HEADSIZE();

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
                pAllocInfo->u.SurfInfo.width = pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Width;
                pAllocInfo->u.SurfInfo.height = pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Height;
                pAllocInfo->u.SurfInfo.format = D3DDDIFMT_X8R8G8B8; /* staging has always always D3DDDIFMT_X8R8G8B8 */
                pAllocInfo->u.SurfInfo.bpp = vboxWddmCalcBitsPerPixel(pAllocInfo->u.SurfInfo.format);
                pAllocInfo->u.SurfInfo.pitch = vboxWddmCalcPitch(pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Width, pAllocInfo->u.SurfInfo.bpp);

                pGetStandardAllocationDriverData->pCreateStagingSurfaceData->Pitch = pAllocInfo->u.SurfInfo.pitch;
            }
            pGetStandardAllocationDriverData->AllocationPrivateDriverDataSize = VBOXWDDM_ALLOCINFO_HEADSIZE();

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

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiSubmitCommand(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SUBMITCOMMAND*  pSubmitCommand)
{
    /* DxgkDdiSubmitCommand runs at dispatch, should not be pageable. */
    NTSTATUS Status = STATUS_SUCCESS;

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    vboxVDbgBreakFv();

    PDEVICE_EXTENSION pDevExt = (PDEVICE_EXTENSION)hAdapter;

    Assert(!pSubmitCommand->DmaBufferSegmentId);

    /* the DMA command buffer is located in system RAM, the host will need to pick it from there */
    //BufInfo.fFlags = 0; /* see VBOXVDMACBUF_FLAG_xx */
    Assert(pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset >= sizeof (VBOXWDDM_DMA_PRIVATE_DATA));
    if (pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset - pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset < sizeof (VBOXWDDM_DMA_PRIVATE_DATA))
    {
        drprintf((__FUNCTION__": DmaBufferPrivateDataSubmissionEndOffset (%d) - DmaBufferPrivateDataSubmissionStartOffset (%d) < sizeof (VBOXWDDM_DMA_PRIVATE_DATA) (%d)\n",
                pSubmitCommand->DmaBufferPrivateDataSubmissionEndOffset,
                pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset,
                sizeof (VBOXWDDM_DMA_PRIVATE_DATA)));
        return STATUS_INVALID_PARAMETER;
    }

    PVBOXWDDM_DMA_PRIVATE_DATA pPrivateData = (PVBOXWDDM_DMA_PRIVATE_DATA)((uint8_t*)pSubmitCommand->pDmaBufferPrivateData + pSubmitCommand->DmaBufferPrivateDataSubmissionStartOffset);
    Assert(pPrivateData);
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

    vboxVdmaCBufDrSubmit (pDevExt, &pDevExt->u.primary.Vdma, pDr);

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", hAdapter));

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

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

    /* @todo: */
    switch (pBuildPagingBuffer->Operation)
    {
        case DXGK_OPERATION_TRANSFER:
        {
//            pBuildPagingBuffer->pDmaBuffer = (uint8_t*)pBuildPagingBuffer->pDmaBuffer + VBOXVDMACMD_SIZE(VBOXVDMACMD_DMA_BPB_TRANSFER);
            break;
        }
        case DXGK_OPERATION_FILL:
        {
//            pBuildPagingBuffer->pDmaBuffer = (uint8_t*)pBuildPagingBuffer->pDmaBuffer + VBOXVDMACMD_SIZE(VBOXVDMACMD_DMA_BPB_FILL);
            break;
        }
        case DXGK_OPERATION_DISCARD_CONTENT:
        {
            AssertBreakpoint();
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

NTSTATUS
APIENTRY
DxgkDdiSetPointerPosition(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERPOSITION*  pSetPointerPosition)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_SUCCESS;
}

NTSTATUS
APIENTRY
DxgkDdiSetPointerShape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_SETPOINTERSHAPE*  pSetPointerShape)
{
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();
    /* @todo: fixme: implement */

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_NOT_SUPPORTED;
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

NTSTATUS
APIENTRY
DxgkDdiEscape(
    CONST HANDLE  hAdapter,
    CONST DXGKARG_ESCAPE*  pEscape)
{
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    return STATUS_INVALID_PARAMETER;
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

//    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

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
                for (UINT id = 0; id < pContext->cSources; ++id)
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
                    for (UINT id = 0; id < pContext->cSources; ++id)
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

//    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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
    uint32_t iPreferredMode;
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


    const DXGK_VIDPN_INTERFACE* pVidPnInterface = NULL;
    Status = pDevExt->u.primary.DxgkInterface.DxgkCbQueryVidPnInterface(pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, DXGK_VIDPN_INTERFACE_VERSION_V1, &pVidPnInterface);
    Assert(Status == STATUS_SUCCESS);
    if (Status == STATUS_SUCCESS)
    {
        D3DKMDT_2DREGION PrefRegion;
        PrefRegion.cx = pModes[iPreferredMode].VisScreenHeight;
        PrefRegion.cy = pModes[iPreferredMode].VisScreenWidth;
        Status = vboxVidPnCreatePopulateVidPnFromLegacy(pDevExt, pRecommendFunctionalVidPnArg->hRecommendedFunctionalVidPn, pVidPnInterface,
#if 0
                pModes, cModes, iPreferredMode,
                pResolutions, cResolutions
#else
                &pModes[iPreferredMode], 1, 0,
                &PrefRegion, 1
#endif
                );
        Assert(Status == STATUS_SUCCESS);
        if (Status != STATUS_SUCCESS)
            drprintf((__FUNCTION__": vboxVidPnCreatePopulateVidPnFromLegacy failed Status(0x%x)\n", Status));
    }
    else
        drprintf((__FUNCTION__": DxgkCbQueryVidPnInterface failed Status(0x%x)\n", Status));

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

//    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", hAdapter));

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

//    dfprintf(("<== "__FUNCTION__ ", status(0x%x), context(0x%x)\n", Status, hAdapter));

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
    Assert(pDevExt->cSources > pSetVidPnSourceAddress->VidPnSourceId);
    if (pDevExt->cSources > pSetVidPnSourceAddress->VidPnSourceId)
    {
        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pSetVidPnSourceAddress->VidPnSourceId];
        PVBOXWDDM_ALLOCATION pAllocation;
        Assert(pSetVidPnSourceAddress->hAllocation);
        Assert(pSetVidPnSourceAddress->hAllocation || pSource->pAllocation);
        Assert (pSetVidPnSourceAddress->Flags.Value < 2); /* i.e. 0 or 1 (ModeChange) */
        if (pSetVidPnSourceAddress->hAllocation)
        {
            pAllocation = (PVBOXWDDM_ALLOCATION)pSetVidPnSourceAddress->hAllocation;
            vboxWddmAssignPrimary(pDevExt, pSource, pAllocation, pSetVidPnSourceAddress->VidPnSourceId);
        }
        else
            pAllocation = pSource->pAllocation;

        Assert(pAllocation);
        if (pAllocation)
        {
            Assert(pAllocation->enmType == VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE);
            PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimary = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
            pAllocation->offVram = (VBOXVIDEOOFFSET)pSetVidPnSourceAddress->PrimaryAddress.QuadPart;
            pAllocation->SegmentId = pSetVidPnSourceAddress->PrimarySegment;
            Assert (pAllocation->SegmentId);
            Assert (!pPrimary->bVisible);
            if (pPrimary->bVisible)
            {
                /* should not generally happen, but still inform host*/
                Status = vboxWddmGhDisplaySetInfo(pDevExt, pSource);
                Assert(Status == STATUS_SUCCESS);
                if (Status != STATUS_SUCCESS)
                    drprintf((__FUNCTION__": vboxWddmGhDisplaySetInfo failed, Status (0x%x)\n", Status));
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
        drprintf((__FUNCTION__": invalid VidPnSourceId (%d), should be smaller than (%d)\n", pSetVidPnSourceAddress->VidPnSourceId, pDevExt->cSources));
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
    Assert(pDevExt->cSources > pSetVidPnSourceVisibility->VidPnSourceId);
    if (pDevExt->cSources > pSetVidPnSourceVisibility->VidPnSourceId)
    {
        PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[pSetVidPnSourceVisibility->VidPnSourceId];
        PVBOXWDDM_ALLOCATION pAllocation = pSource->pAllocation;
        PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimary = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);

        Assert(pPrimary->bVisible != pSetVidPnSourceVisibility->Visible);
        if (pPrimary->bVisible != pSetVidPnSourceVisibility->Visible)
        {
            pPrimary->bVisible = pSetVidPnSourceVisibility->Visible;
            if (pPrimary->bVisible)
            {
                Status = vboxWddmGhDisplaySetInfo(pDevExt, pSource);
                Assert(Status == STATUS_SUCCESS);
                if (Status != STATUS_SUCCESS)
                    drprintf((__FUNCTION__": vboxWddmGhDisplaySetInfo failed, Status (0x%x)\n", Status));
            }
            else
            {
                vboxVdmaFlush (pDevExt, &pDevExt->u.primary.Vdma);
            }
        }
    }
    else
    {
        drprintf((__FUNCTION__": invalid VidPnSourceId (%d), should be smaller than (%d)\n", pSetVidPnSourceVisibility->VidPnSourceId, pDevExt->cSources));
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
            for (UINT i = 0; i < pDevExt->cSources; ++i)
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
    uint32_t iPreferredMode;
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
                    true);
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

    AssertBreakpoint();

    pGetScanLine->InVerticalBlank = FALSE;
    pGetScanLine->ScanLine = 0;

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

    AssertBreakpoint();

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
    dfprintf(("==> "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

    AssertBreakpoint();

    dfprintf(("<== "__FUNCTION__ ", hAdapter(0x%x)\n", hAdapter));

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

    for (UINT i = 0; i < pOpenAllocation->NumAllocations; ++i)
    {
        DXGK_OPENALLOCATIONINFO* pInfo = &pOpenAllocation->pOpenAllocation[i];
        PVBOXWDDM_OPENALLOCATION pOa = (PVBOXWDDM_OPENALLOCATION)vboxWddmMemAllocZero(sizeof (VBOXWDDM_OPENALLOCATION));
        pOa->hAllocation = pInfo->hAllocation;
        pInfo->hDeviceSpecificAllocation = pOa;
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
    drprintf(("==> "__FUNCTION__ ", !!NOT_IMPLEMENTED!! hContext(0x%x)\n", hContext));

    AssertBreakpoint();

    drprintf(("<== "__FUNCTION__ ", !!NOT_IMPLEMENTED!! hContext(0x%x)\n", hContext));

    return STATUS_NOT_IMPLEMENTED;
}

#define VBOXVDMACMD_DMA_PRESENT_BLT_MINSIZE() (VBOXVDMACMD_SIZE(VBOXVDMACMD_DMA_PRESENT_BLT))
#define VBOXVDMACMD_DMA_PRESENT_BLT_SIZE(_c) (VBOXVDMACMD_BODY_FIELD_OFFSET(UINT, VBOXVDMACMD_DMA_PRESENT_BLT, aDstSubRects[_c]))

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

DECLINLINE(VBOXVIDEOOFFSET) vboxWddmOffsetFromPhAddress(PHYSICAL_ADDRESS phAddr)
{
    return (VBOXVIDEOOFFSET)(phAddr.QuadPart ? phAddr.QuadPart - VBE_DISPI_LFB_PHYSICAL_ADDRESS : VBOXVIDEOOFFSET_VOID);
}

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
    pDesc->width = pAllocation->u.SurfInfo.width;
    pDesc->height = pAllocation->u.SurfInfo.height;
    pDesc->format = vboxWddmFromPixFormat(pAllocation->u.SurfInfo.format);
    pDesc->bpp = pAllocation->u.SurfInfo.bpp;
    pDesc->pitch = pAllocation->u.SurfInfo.pitch;
    pDesc->fFlags = 0;
}

DECLINLINE(BOOLEAN) vboxWddmPixFormatConversionSupported(D3DDDIFORMAT From, D3DDDIFORMAT To)
{
    Assert(From != D3DDDIFMT_UNKNOWN);
    Assert(To != D3DDDIFMT_UNKNOWN);
    Assert(From == To);
    return From == To;
}

DECLINLINE(PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE) vboxWddmCheckForVisiblePrimary(PDEVICE_EXTENSION pDevExt, PVBOXWDDM_ALLOCATION pAllocation)
{
    if (pAllocation->enmType != VBOXWDDM_ALLOC_TYPE_STD_SHAREDPRIMARYSURFACE)
        return NULL;

    PVBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE pPrimary = VBOXWDDM_ALLOCATION_BODY(pAllocation, VBOXWDDM_ALLOCATION_SHAREDPRIMARYSURFACE);
    if (!pPrimary->bVisible)
        return NULL;

    D3DDDI_VIDEO_PRESENT_SOURCE_ID id = pPrimary->VidPnSourceId;
    if (id >=  pDevExt->cSources)
        return NULL;

    PVBOXWDDM_SOURCE pSource = &pDevExt->aSources[id];
    if (pSource->pAllocation != pAllocation)
        return NULL;

    return pPrimary;
}

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

    dfprintf(("==> "__FUNCTION__ ", hContext(0x%x)\n", hContext));

    vboxVDbgBreakFv();

    NTSTATUS Status = STATUS_SUCCESS;
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)hContext;
    PVBOXWDDM_DEVICE pDevice = pContext->pDevice;
    PDEVICE_EXTENSION pDevExt = pDevice->pAdapter;

    Assert(pPresent->DmaBufferPrivateDataSize >= sizeof (VBOXWDDM_DMA_PRIVATE_DATA));
    if (pPresent->DmaBufferPrivateDataSize < sizeof (VBOXWDDM_DMA_PRIVATE_DATA))
    {
        drprintf((__FUNCTION__": Present->DmaBufferPrivateDataSize(%d) < sizeof VBOXWDDM_DMA_PRIVATE_DATA (%d)\n", pPresent->DmaBufferPrivateDataSize , sizeof (VBOXWDDM_DMA_PRIVATE_DATA)));
        /* @todo: can this actually happen? what status tu return? */
        return STATUS_INVALID_PARAMETER;
    }

    PVBOXWDDM_DMA_PRIVATE_DATA pPrivateData = (PVBOXWDDM_DMA_PRIVATE_DATA)pPresent->pDmaBufferPrivateData;
    pPrivateData->pContext = (PVBOXWDDM_CONTEXT)hContext;

    if (pPresent->Flags.Blt)
    {
        Assert(pPresent->Flags.Value == 1); /* only Blt is set, we do not support anything else for now */
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
                    if (vboxWddmPixFormatConversionSupported(pSrcAlloc->u.SurfInfo.format, pDstAlloc->u.SurfInfo.format))
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
                        pPresent->pDmaBufferPrivateData = (uint8_t*)pPresent->pDmaBufferPrivateData + sizeof(VBOXWDDM_DMA_PRIVATE_DATA);
                    }
                    else
                    {
                        AssertBreakpoint();
                        drprintf((__FUNCTION__": unsupported format conversion from(%d) to (%d)\n",pSrcAlloc->u.SurfInfo.format, pDstAlloc->u.SurfInfo.format));
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

    }
    else
    {
        drprintf((__FUNCTION__": cmd NOT IMPLEMENTED!! Flags(0x%x)\n", pPresent->Flags.Value));
        AssertBreakpoint();
    }

    dfprintf(("<== "__FUNCTION__ ", hContext(0x%x), Status(0x%x)\n", hContext, Status));

    return Status;
}

NTSTATUS
APIENTRY
DxgkDdiUpdateOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_UPDATEOVERLAY  *pUpdateOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hOverlay(0x%x)\n", hOverlay));
    AssertBreakpoint();
    dfprintf(("<== "__FUNCTION__ ", hOverlay(0x%x)\n", hOverlay));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiFlipOverlay(
    CONST HANDLE  hOverlay,
    CONST DXGKARG_FLIPOVERLAY  *pFlipOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hOverlay(0x%x)\n", hOverlay));
    AssertBreakpoint();
    dfprintf(("<== "__FUNCTION__ ", hOverlay(0x%x)\n", hOverlay));
    return STATUS_NOT_IMPLEMENTED;
}

NTSTATUS
APIENTRY
DxgkDdiDestroyOverlay(
    CONST HANDLE  hOverlay)
{
    dfprintf(("==> "__FUNCTION__ ", hOverlay(0x%x)\n", hOverlay));
    AssertBreakpoint();
    dfprintf(("<== "__FUNCTION__ ", hOverlay(0x%x)\n", hOverlay));
    return STATUS_NOT_IMPLEMENTED;
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
    PVBOXWDDM_CONTEXT pContext = (PVBOXWDDM_CONTEXT)vboxWddmMemAllocZero(sizeof (VBOXWDDM_CONTEXT));

    pContext->pDevice = pDevice;
    pContext->hContext = pCreateContext->hContext;
    pContext->EngineAffinity = pCreateContext->EngineAffinity;
    pContext->NodeOrdinal = pCreateContext->NodeOrdinal;
    if (pCreateContext->Flags.SystemContext)
        pContext->enmType = VBOXWDDM_CONTEXT_TYPE_SYSTEM;
//    else
//    {
//        AssertBreakpoint(); /* we do not support custom contexts for now */
//        drprintf((__FUNCTION__ ", we do not support custom contexts for now, hDevice (0x%x)\n", hDevice));
//    }

    pCreateContext->hContext = pContext;
    pCreateContext->ContextInfo.DmaBufferSize = VBOXWDDM_C_DMA_BUFFER_SIZE;
    pCreateContext->ContextInfo.DmaBufferSegmentSet = 0;
    pCreateContext->ContextInfo.DmaBufferPrivateDataSize = sizeof (VBOXWDDM_DMA_PRIVATE_DATA);
    pCreateContext->ContextInfo.AllocationListSize = VBOXWDDM_C_ALLOC_LIST_SIZE;
    pCreateContext->ContextInfo.PatchLocationListSize = VBOXWDDM_C_PATH_LOCATION_LIST_SIZE;
//#if (DXGKDDI_INTERFACE_VERSION >= DXGKDDI_INTERFACE_VERSION_WIN7)
//# error port to Win7 DDI
//    //pCreateContext->ContextInfo.DmaBufferAllocationGroup = ???;
//#endif // DXGKDDI_INTERFACE_VERSION

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
    vboxWddmMemFree(hContext);
    dfprintf(("<== "__FUNCTION__ ", hContext(0x%x)\n", hContext));
    return STATUS_SUCCESS;
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

    AssertBreakpoint();

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
