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
    return false;
}

VOID DxgkDdiDpcRoutine(
    IN CONST PVOID  MiniportDeviceContext
    )
{

}

NTSTATUS DxgkDdiQueryChildRelations(
    IN CONST PVOID MiniportDeviceContext,
    IN OUT PDXGK_CHILD_DESCRIPTOR ChildRelations,
    IN ULONG ChildRelationsSize
    )
{
    /* The DxgkDdiQueryChildRelations function should be made pageable. */
    PAGED_CODE();

    dfprintf(("==> "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));
    for (ULONG i = 0; i < ChildRelationsSize; ++i)
    {
        ChildRelations[i].ChildDeviceType = TypeVideoOutput;
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.InterfaceTechnology = D3DKMDT_VOT_OTHER;
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.MonitorOrientationAwareness = D3DKMDT_MOA_NONE;
        ChildRelations[i].ChildCapabilities.Type.VideoOutput.SupportsSdtvModes = FALSE;
        ChildRelations[i].ChildCapabilities.HpdAwareness = HpdAwarenessAlwaysConnected;
        ChildRelations[i].AcpiUid =  i; /* @todo: do we need it? could it be zero ? */
        ChildRelations[i].ChildUid = i; /* could it be zero ? */
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

    /* we do not support EDID */
    DeviceDescriptor->DescriptorOffset = 0;
    DeviceDescriptor->DescriptorLength = 0;
    DeviceDescriptor->DescriptorBuffer = NULL;

    dfprintf(("<== "__FUNCTION__ ", context(0x%x)\n", MiniportDeviceContext));

    return STATUS_SUCCESS;
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

            pCaps->HighestAcceptableAddress.HighPart = 0xffffffffUL;
            pCaps->HighestAcceptableAddress.LowPart = 0xffffffffUL;
            pCaps->MaxAllocationListSlotId = 0xffffffffUL;
            pCaps->ApertureSegmentCommitLimit = 0;
            pCaps->MaxPointerWidth = 0xffffffffUL;
            pCaps->MaxPointerHeight = 0xffffffffUL;
            pCaps->PointerCaps.Value = 0x7; /* Monochrome , Color , MaskedColor */
            pCaps->InterruptMessageNumber = 0;
            pCaps->NumberOfSwizzlingRanges = 0;
            /* @todo: need to adjust this for proper 2D Accel support */
            pCaps->MaxOverlays = 0;
            pCaps->GammaRampCaps.Value = 0;
            pCaps->PresentationCaps.Value = 0;
            pCaps->PresentationCaps.NoScreenToScreenBlt = 1;
            pCaps->PresentationCaps.NoOverlapScreenBlt = 1;
            pCaps->MaxQueuedFlipOnVSync = 1; /* @todo: need more ?? */
            pCaps->FlipCaps.Value = 0;
            pCaps->FlipCaps.FlipOnVSyncWithNoWait = 1;
            pCaps->FlipCaps.FlipOnVSyncMmIo = 1; /* @todo: ?? */
            pCaps->SchedulingCaps.Value = 0;
            /* @todo: we might need it for Aero.
             * Setting this glag would mean we support DeviceContext, i.e.
             *  DxgkDdiCreateContext and DxgkDdiDestroyContext
             *
             *  pCaps->SchedulingCaps.MultiEngineAware = 1;
             */
            pCaps->MemoryManagementCaps.Value = 0;
            /* @todo: this corelates with pCaps->SchedulingCaps.MultiEngineAware */
            pCaps->MemoryManagementCaps.PagingNode = 0;
            /* @todo: this corelates with pCaps->SchedulingCaps.MultiEngineAware */
            pCaps->GpuEngineTopology.NbAsymetricProcessingNodes = 0;

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
                pQsOut->pSegmentDescriptor->BaseAddress.QuadPart = VBE_DISPI_LFB_PHYSICAL_ADDRESS;
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
            pQsOut->PagingBufferSize = 1024; /* @todo: ??*/
            pQsOut->PagingBufferPrivateDataSize = 0; /* @todo: ??*/
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
            VBOXVIDPNCMCONTEXT CbContext = {0};
            CbContext.pEnumCofuncModalityArg = pEnumCofuncModalityArg;
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
