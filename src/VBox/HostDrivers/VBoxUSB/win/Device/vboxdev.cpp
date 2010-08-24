/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    vboxdev.cpp

Abstract:

    This file contains dispatch routines for create,
    close and selective suspend.
    The selective suspend feature is enabled if
    the SSRegistryEnable key in the registry is set to 1.

Environment:

    Kernel mode

Notes:

    Copyright (c) 2000 Microsoft Corporation.
    All Rights Reserved.

--*/

#include "vboxusb.h"
#include "vboxpnp.h"
#include "vboxpwr.h"
#include "vboxdev.h"
#include "vboxrwr.h"
#include <iprt/assert.h>
#include <VBox/usblib.h>
#include <usbioctl.h>
#define _USBD_
#include <usbdlib.h>
#include <usbbusif.h>


static NTSTATUS VBoxUSBSetConfig(PDEVICE_EXTENSION pDevice, unsigned uConfiguration);
static NTSTATUS VBoxUSBSetInterface(PDEVICE_EXTENSION pDevice, uint32_t InterfaceNumber, int AlternateSetting);
static NTSTATUS VBoxUSBClearEndpoint(PDEVICE_EXTENSION pDevice, uint32_t EndPointAddress, bool fReset);
static NTSTATUS VBoxUSBClearPipe(PDEVICE_EXTENSION pDevice, HANDLE PipeHandle, bool fReset);
static HANDLE   VBoxUSBGetPipeHandle(PDEVICE_EXTENSION pDevice, uint32_t EndPointAddress);
static void     VBoxUSBFreeInterfaces(PDEVICE_EXTENSION pDevice, bool fAbortPipes);
static NTSTATUS VBoxUSBGetDeviceDescription(PDEVICE_EXTENSION pDevice);
static NTSTATUS VBoxUSBGetDeviceSpeed(PDEVICE_EXTENSION pDevExt);
       NTSTATUS VBoxUSBSendIOCTL(PDEVICE_EXTENSION pDevice, ULONG control_code, void *buffer);
       NTSTATUS VBoxUSBSendURB(PDEVICE_EXTENSION pDevExt, PIRP pIrp, PUSBSUP_URB pIn, PUSBSUP_URB pOut);
       NTSTATUS VBoxUSBSyncSendRequest(PDEVICE_EXTENSION deviceExtension, ULONG control_code, void *buffer);


/**
 * Get USB descriptor
 *
 * @returns NT Status
 * @param   Pdo         Device object
 * @param   buffer      Descriptor buffer
 * @param   size        size of buffer
 * @param   type        descriptor type
 * @param   index       descriptor index
 * @param   language_id descriptor language id
 */
NTSTATUS VBoxUSBGetDescriptor(PDEVICE_EXTENSION pDevice, void *buffer, int size, int type, int index, int language_id)
{
    NTSTATUS rc;
    PURB     urb;

    urb = (PURB)ExAllocatePool(NonPagedPool,sizeof(URB));
    if(urb == NULL)
    {
        dprintf(("Failed to alloc mem for urb\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }
    memset(urb, 0, sizeof(*urb));

    urb->UrbHeader.Function = URB_FUNCTION_GET_DESCRIPTOR_FROM_DEVICE;
    urb->UrbHeader.Length = sizeof(struct _URB_CONTROL_DESCRIPTOR_REQUEST);
    urb->UrbControlDescriptorRequest.TransferBufferLength = size;
    urb->UrbControlDescriptorRequest.TransferBuffer       = buffer;
    urb->UrbControlDescriptorRequest.Index                = (UCHAR)index;
    urb->UrbControlDescriptorRequest.DescriptorType       = (UCHAR)type;
    urb->UrbControlDescriptorRequest.LanguageId           = (USHORT)language_id;

    rc = VBoxUSBSendIOCTL(pDevice, IOCTL_INTERNAL_USB_SUBMIT_URB, urb);
    if(!NT_SUCCESS(rc) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
        dprintf(("VBoxUSBGetDescriptor: VBoxUSBSendIOCTL failed with %x (%x)\n", rc, urb->UrbHeader.Status));
    }
    ExFreePool(urb);
    return rc;
}

/**
 * Free cached USB device/configuration descriptors
 *
 * @param   pDevice             USB device pointer
 */
static void VBoxUSBFreeCachedDescriptors(PDEVICE_EXTENSION pDevice)
{
    unsigned        i;

    if (pDevice->usbdev.devdescr)
    {
        ExFreePool(pDevice->usbdev.devdescr);
        pDevice->usbdev.devdescr = NULL;
    }
    for (i = 0; i < MAX_CFGS; ++i)
    {
        if (pDevice->usbdev.cfgdescr[i])
        {
            ExFreePool(pDevice->usbdev.cfgdescr[i]);
            pDevice->usbdev.cfgdescr[i] = NULL;
        }
    }
}

/**
 * Cache USB device/configuration descriptors
 *
 * @returns NT Status
 * @param   pDevice             USB device pointer
 */
static NTSTATUS VBoxUSBCacheDescriptors(PDEVICE_EXTENSION pDevice)
{
    NTSTATUS                        status;
    USB_CONFIGURATION_DESCRIPTOR    *tmp_cfgdescr = NULL;
    uint32_t                        uTotalLength;
    unsigned                        i;

    /* Reading descriptors is relatively expensive and they aren't going to change
     * except possibly when the device is reset (and even then only very rarely).
     */

    /* Read device descriptor */
    pDevice->usbdev.devdescr = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePool(NonPagedPool, sizeof(USB_DEVICE_DESCRIPTOR));
    if (pDevice->usbdev.devdescr == NULL)
    {
        dprintf(("Failed to alloc mem for device descriptor\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto end;
    }
    memset(pDevice->usbdev.devdescr, 0, sizeof(USB_DEVICE_DESCRIPTOR));

    status = VBoxUSBGetDescriptor(pDevice, pDevice->usbdev.devdescr, sizeof(USB_DEVICE_DESCRIPTOR), USB_DEVICE_DESCRIPTOR_TYPE, 0, 0);
    if (!NT_SUCCESS(status))
    {
        AssertMsgFailed(("VBoxUSBCacheDescriptors: getting device descriptor failed\n"));
        goto end;
    }
    Assert(pDevice->usbdev.devdescr->bNumConfigurations > 0);
    dprintf(("Nr of configurations %d\n", pDevice->usbdev.devdescr->bNumConfigurations));

    tmp_cfgdescr = (USB_CONFIGURATION_DESCRIPTOR *) ExAllocatePool(NonPagedPool, sizeof(USB_CONFIGURATION_DESCRIPTOR));
    if (!tmp_cfgdescr)
    {
        AssertMsgFailed(("VBoxUSBCacheDescriptors: ExAllocatePool failed\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto end;
    }

    /* Read descriptors for all configurations */
    for (i = 0; i < pDevice->usbdev.devdescr->bNumConfigurations; ++i)
    {
        status = VBoxUSBGetDescriptor(pDevice, tmp_cfgdescr, sizeof(USB_CONFIGURATION_DESCRIPTOR), USB_CONFIGURATION_DESCRIPTOR_TYPE, i, 0);
        if (!NT_SUCCESS(status))
        {
            AssertMsgFailed(("VBoxUSBCacheDescriptors: VBoxUSBGetDescriptor (cfg %d) failed with %x\n", i + 1, status));
            goto end;
        }

        uTotalLength = tmp_cfgdescr->wTotalLength;
        dprintf(("Total length = %d\n", tmp_cfgdescr->wTotalLength));

        pDevice->usbdev.cfgdescr[i] = (USB_CONFIGURATION_DESCRIPTOR *)ExAllocatePool(NonPagedPool, uTotalLength);
        if (!pDevice->usbdev.cfgdescr[i])
        {
            AssertMsgFailed(("VBoxUSBCacheDescriptors: ExAllocatePool failed!\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }

        status = VBoxUSBGetDescriptor(pDevice, pDevice->usbdev.cfgdescr[i], uTotalLength, USB_CONFIGURATION_DESCRIPTOR_TYPE, i, 0);
        if (!NT_SUCCESS(status))
        {
            AssertMsgFailed(("VBoxUSBCacheDescriptors: VBoxUSBGetDescriptor (cfg %d) failed with %x\n", i + 1, status));
            goto end;
        }
    }

end:
    if (tmp_cfgdescr)   ExFreePool(tmp_cfgdescr);

    return status;
}

/**
 * Free memory allocated by this module
 *
 * @param   pDevice             USB device pointer
 */
VOID VBoxUSB_FreeMemory(PDEVICE_EXTENSION DeviceExtension)
{
    VBoxUSBFreeCachedDescriptors(DeviceExtension);
    VBoxUSBFreeInterfaces(DeviceExtension, FALSE);
}

NTSTATUS
VBoxUSB_DispatchCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )
/*++

Routine Description:

    Dispatch routine for create.

Arguments:

    DeviceObject - pointer to device object
    Irp - I/O request packet.

Return Value:

    NT status value

--*/
{
//    ULONG                       i;
    NTSTATUS                    ntStatus;
    PFILE_OBJECT                fileObject;
    PDEVICE_EXTENSION           deviceExtension;
    PIO_STACK_LOCATION          irpStack;
//    PVBOXUSB_PIPE_CONTEXT       pipeContext;
//    PUSBD_INTERFACE_INFORMATION interface;

    PAGED_CODE();

    dprintf(("VBoxUSB_DispatchCreate - begins\n"));

    //
    // initialize variables
    //
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpStack->FileObject;
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    if(deviceExtension->DeviceState != Working) {

        ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto VBoxUSB_DispatchCreate_Exit;
    }

#if 0
    if(deviceExtension->UsbInterface) {

        interface = deviceExtension->UsbInterface;
    }
    else {

        dprintf(("UsbInterface not found\n"));

        ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto VBoxUSB_DispatchCreate_Exit;
    }
#endif

    //
    // FsContext is Null for the device
    //
    if(fileObject) {

        fileObject->FsContext = NULL;
    }
    else {

        ntStatus = STATUS_INVALID_PARAMETER;
        goto VBoxUSB_DispatchCreate_Exit;
    }

    if(0 == fileObject->FileName.Length) {

        //
        // opening a device as opposed to pipe.
        //
        ntStatus = STATUS_SUCCESS;

        InterlockedIncrement(&deviceExtension->OpenHandleCount);

        //
        // the device is idle if it has no open handles or pending PnP Irps
        // since we just received an open handle request, cancel idle req.
        //
        if(deviceExtension->SSEnable) {

            CancelSelectSuspend(deviceExtension);
        }

        goto VBoxUSB_DispatchCreate_Exit;
    }

#if 0
    pipeContext = VBoxUSB_PipeWithName(DeviceObject, &fileObject->FileName);

    if(pipeContext == NULL) {

        ntStatus = STATUS_INVALID_PARAMETER;
        goto VBoxUSB_DispatchCreate_Exit;
    }

    ntStatus = STATUS_INVALID_PARAMETER;

    for(i=0; i<interface->NumberOfPipes; i++) {

        if(pipeContext == &deviceExtension->PipeContext[i]) {

            //
            // found a match
            //
            dprintf(("open pipe %d\n", i));

            fileObject->FsContext = &interface->Pipes[i];

            ASSERT(fileObject->FsContext);

            pipeContext->PipeOpen = TRUE;

            ntStatus = STATUS_SUCCESS;

            //
            // increment OpenHandleCounts
            //
            InterlockedIncrement(&deviceExtension->OpenHandleCount);

            //
            // the device is idle if it has no open handles or pending PnP Irps
            // since we just received an open handle request, cancel idle req.
            //
            if(deviceExtension->SSEnable) {

                CancelSelectSuspend(deviceExtension);
            }
        }
    }
#endif

VBoxUSB_DispatchCreate_Exit:

    Irp->IoStatus.Status = ntStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    dprintf(("VBoxUSB_DispatchCreate - ends\n"));

    return ntStatus;
}

NTSTATUS
VBoxUSB_DispatchClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )
/*++

Routine Description:

    Dispatch routine for close.

Arguments:

    DeviceObject - pointer to device object
    Irp - I/O request packet

Return Value:

    NT status value

--*/
{
    NTSTATUS               ntStatus;
    PFILE_OBJECT           fileObject;
    PDEVICE_EXTENSION      deviceExtension;
    PIO_STACK_LOCATION     irpStack;
    PVBOXUSB_PIPE_CONTEXT  pipeContext;
    PUSBD_PIPE_INFORMATION pipeInformation;

    PAGED_CODE();

    //
    // initialize variables
    //
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpStack->FileObject;
    pipeContext = NULL;
    pipeInformation = NULL;
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    dprintf(("VBoxUSB_DispatchClose - begins\n"));

    if(fileObject && fileObject->FsContext) {

        pipeInformation = (PUSBD_PIPE_INFORMATION)fileObject->FsContext;

        if(0 != fileObject->FileName.Length) {

            pipeContext = VBoxUSB_PipeWithName(DeviceObject,
                                               &fileObject->FileName);
        }

        if(pipeContext && pipeContext->PipeOpen) {

            pipeContext->PipeOpen = FALSE;
        }
    }

    //
    // set ntStatus to STATUS_SUCCESS
    //
    ntStatus = STATUS_SUCCESS;

    Irp->IoStatus.Status = ntStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    InterlockedDecrement(&deviceExtension->OpenHandleCount);

    dprintf(("VBoxUSB_DispatchClose - ends\n"));

#if 0
    /* Force an unplug and re-plug to load the original Windows driver (or to give it to another running VM)
     * Only when the device was actually claimed and it's still operational (not removed already for instance).
     */
    if (    deviceExtension->OpenHandleCount == 0
        &&  deviceExtension->usbdev.fClaimed
        &&  deviceExtension->DeviceState == Working)
    {
        NTSTATUS rc = VBoxUSBSendIOCTL(deviceExtension, IOCTL_INTERNAL_USB_CYCLE_PORT, NULL);
        Assert(NT_SUCCESS(rc));
    }
    else
        dprintf(("Didn't cycle port OpenHandleCount=%d fClaimed=%d DeviceState=%d\n", deviceExtension->OpenHandleCount, deviceExtension->usbdev.fClaimed, deviceExtension->DeviceState));
#endif

    return ntStatus;
}

NTSTATUS
VBoxUSB_DispatchDevCtrl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    Dispatch routine for IRP_MJ_DEVICE_CONTROL

Arguments:

    DeviceObject - pointer to device object
    Irp - I/O request packet

Return Value:

    NT status value

--*/
{
    ULONG              code;
    PVOID              ioBuffer;
    ULONG              inputBufferLength;
    ULONG              outputBufferLength;
    ULONG              info;
    NTSTATUS           ntStatus = STATUS_SUCCESS;
    PDEVICE_EXTENSION  deviceExtension;
    PIO_STACK_LOCATION irpStack;

    //
    // initialize variables
    //
    info = 0;
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    code = irpStack->Parameters.DeviceIoControl.IoControlCode;
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    ioBuffer           = Irp->AssociatedIrp.SystemBuffer;
    inputBufferLength  = irpStack->Parameters.DeviceIoControl.InputBufferLength;
    outputBufferLength = irpStack->Parameters.DeviceIoControl.OutputBufferLength;

    if(deviceExtension->DeviceState != Working)
    {
        dprintf(("Invalid device state\n"));

        Irp->IoStatus.Status = ntStatus = STATUS_INVALID_DEVICE_STATE;
        Irp->IoStatus.Information = info;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
        return ntStatus;
    }

    dprintf(("VBoxUSB_DispatchDevCtrl::"));
    VBoxUSB_IoIncrement(deviceExtension);

    //
    // It is true that the client driver cancelled the selective suspend
    // request in the dispatch routine for create.
    // But there is no guarantee that it has indeed been completed.
    // so wait on the NoIdleReqPendEvent and proceed only if this event
    // is signalled.
    //
    dprintf(("Waiting on the IdleReqPendEvent\n"));

    //
    // make sure that the selective suspend request has been completed.
    //

    if(deviceExtension->SSEnable)
    {
        KeWaitForSingleObject(&deviceExtension->NoIdleReqPendEvent,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
    }

    switch(code) {

    case SUPUSB_IOCTL_USB_CLAIM_DEVICE:
    {
        PUSBSUP_CLAIMDEV pIn  = (PUSBSUP_CLAIMDEV)ioBuffer;
        PUSBSUP_CLAIMDEV pOut = (PUSBSUP_CLAIMDEV)ioBuffer;

        dprintf(("SUPUSB_IOCTL_USB_CLAIM_DEVICE\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != sizeof(*pOut))
        {
            AssertMsgFailed(("SUPUSB_IOCTL_USB_GRAB_DEVICE: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, sizeof(*pIn), outputBufferLength, sizeof(*pOut)));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        if (deviceExtension->usbdev.fClaimed == false)
        {
            VBoxUSBFreeCachedDescriptors(deviceExtension);
            ntStatus = VBoxUSBCacheDescriptors(deviceExtension);
            if (NT_SUCCESS(ntStatus))
            {
                InterlockedExchange(&deviceExtension->usbdev.fClaimed, true);
                pIn->fClaimed = true;
            }
        }
        else
        {
            pIn->fClaimed = false;
        }
        info = sizeof(*pOut);
        break;
    }

    case SUPUSB_IOCTL_USB_RELEASE_DEVICE:
    {
        VBoxUSBFreeCachedDescriptors(deviceExtension);
        /* Don't set fClaimed to false here, or else the device won't be returned to Windows afterwards */
        break;
    }

    case SUPUSB_IOCTL_GET_DEVICE:
    {
        PUSBSUP_GETDEV pIn  = (PUSBSUP_GETDEV)ioBuffer;
        PUSBSUP_GETDEV pOut = (PUSBSUP_GETDEV)ioBuffer;

        dprintf(("SUPUSB_IOCTL_GET_DEVICE\n"));
        if (!ioBuffer || inputBufferLength != outputBufferLength || inputBufferLength != sizeof(*pIn))
        {
            AssertMsgFailed(("SUPUSB_IOCTL_GET_DEVICE: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, sizeof(*pIn), outputBufferLength, sizeof(*pOut)));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        ntStatus = VBoxUSBGetDeviceDescription(deviceExtension);
        if (!NT_SUCCESS(ntStatus))
        {
            AssertMsgFailed(("VBoxUSBGetDeviceDescription failed with %x\n", ntStatus));
            break;
        }
        ntStatus = VBoxUSBGetDeviceSpeed(deviceExtension);
        if (!NT_SUCCESS(ntStatus))
        {
            AssertMsgFailed(("VBoxUSBGetDeviceSpeed failed with %x\n", ntStatus));
            break;
        }

        pOut->fAttached = true;
        pOut->fHiSpeed  = deviceExtension->usbdev.fIsHighSpeed;
        pOut->vid       = deviceExtension->usbdev.idVendor;
        pOut->did       = deviceExtension->usbdev.idProduct;
        pOut->rev       = deviceExtension->usbdev.bcdDevice;
        strncpy(pOut->serial_hash, deviceExtension->usbdev.szSerial, sizeof(pOut->serial_hash));

        dprintf(("New device vid=%x pid=%x rev=%x\n", deviceExtension->usbdev.idVendor, deviceExtension->usbdev.idProduct, deviceExtension->usbdev.bcdDevice));

        info = sizeof(*pOut);
        break;
    }

    case SUPUSB_IOCTL_USB_RESET:
    {
        dprintf(("SUPUSB_IOCTL_USB_RESET\n"));

        if (ioBuffer || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSB_IOCTL_USB_RESET: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, outputBufferLength));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        ntStatus = VBoxUSBSendIOCTL(deviceExtension, IOCTL_INTERNAL_USB_RESET_PORT, NULL);
        dprintf(("IOCTL_INTERNAL_USB_RESET_PORT returned %x\n", ntStatus));
        break;
    }

    case SUPUSB_IOCTL_USB_SET_CONFIG:
    {
        PUSBSUP_SET_CONFIG pIn  = (PUSBSUP_SET_CONFIG)ioBuffer;

        dprintf(("SUPUSB_IOCTL_USB_SET_CONFIG\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSB_IOCTL_USB_SET_CONFIG: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, sizeof(*pIn), outputBufferLength, 0));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        ntStatus = VBoxUSBSetConfig(deviceExtension, pIn->bConfigurationValue);
        dprintf(("VBoxUSBSetConfig returned %x\n", ntStatus));
        break;
    }

    case SUPUSB_IOCTL_USB_SELECT_INTERFACE:
    {
        PUSBSUP_SELECT_INTERFACE pIn  = (PUSBSUP_SELECT_INTERFACE)ioBuffer;

        dprintf(("SUPUSB_IOCTL_USB_SELECT_INTERFACE\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSB_IOCTL_USB_SELECT_INTERFACE: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, 0));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        ntStatus = VBoxUSBSetInterface(deviceExtension, pIn->bInterfaceNumber, pIn->bAlternateSetting);
        dprintf(("VBoxUSBSetInterface returned %x\n", ntStatus));
        break;
    }

    case SUPUSB_IOCTL_USB_CLEAR_ENDPOINT:
    {
        PUSBSUP_CLEAR_ENDPOINT pIn  = (PUSBSUP_CLEAR_ENDPOINT)ioBuffer;

        dprintf(("SUPUSB_IOCTL_USB_CLEAR_ENDPOINT\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSB_IOCTL_USB_CLEAR_ENDPOINT: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, 0));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        ntStatus = VBoxUSBClearEndpoint(deviceExtension, pIn->bEndpoint, TRUE);
        dprintf(("VBoxUSBClearEndpoint returned %x\n", ntStatus));
        break;
    }

    case SUPUSB_IOCTL_USB_ABORT_ENDPOINT:
    {
        PUSBSUP_CLEAR_ENDPOINT pIn  = (PUSBSUP_CLEAR_ENDPOINT)ioBuffer;

        dprintf(("SUPUSB_IOCTL_USB_ABORT_ENDPOINT\n"));
        if (!ioBuffer || inputBufferLength != sizeof(*pIn) || outputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSB_IOCTL_USB_ABORT_ENDPOINT: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, 0));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        ntStatus = VBoxUSBClearEndpoint(deviceExtension, pIn->bEndpoint, FALSE);
        dprintf(("VBoxUSBClearEndpoint returned %x\n", ntStatus));
        break;
    }

    case SUPUSB_IOCTL_SEND_URB:
    {
        PUSBSUP_URB pIn  = (PUSBSUP_URB)ioBuffer;
        PUSBSUP_URB pOut = (PUSBSUP_URB)ioBuffer;

        dprintf(("SUPUSB_IOCTL_SEND_URB\n"));
        if (!ioBuffer || inputBufferLength != outputBufferLength || inputBufferLength != sizeof(*pIn))
        {
            AssertMsgFailed(("SUPUSB_IOCTL_SEND_URB: Invalid input/output sizes. inputBufferLength=%d expected %d. outputBufferLength=%d expected %d.\n",
                             inputBufferLength, sizeof(*pIn), outputBufferLength, sizeof(*pOut)));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }

        ntStatus = VBoxUSBSendURB(deviceExtension, Irp, pIn, pOut);
        break;
    }

    case SUPUSB_IOCTL_IS_OPERATIONAL:
    {
        // if we get this far, then we're still up and running
        break;
    }

    case SUPUSB_IOCTL_GET_VERSION:
    {
        PUSBSUP_VERSION pOut = (PUSBSUP_VERSION)ioBuffer;

        if (!ioBuffer || outputBufferLength != sizeof(*pOut) || inputBufferLength != 0)
        {
            AssertMsgFailed(("SUPUSB_IOCTL_GET_VERSION: Invalid input/output sizes. cbIn=%d expected %d. cbOut=%d expected %d.\n",
                             inputBufferLength, 0, outputBufferLength, sizeof(*pOut)));
            ntStatus = STATUS_INVALID_PARAMETER;
            break;
        }
        pOut->u32Major = USBDRV_MAJOR_VERSION;
        pOut->u32Minor = USBDRV_MINOR_VERSION;
        info = sizeof(*pOut);
        break;
    }

    default :
        ntStatus = STATUS_INVALID_DEVICE_REQUEST;
        break;
    }

    if (ntStatus != STATUS_PENDING)
    {
        Irp->IoStatus.Status = ntStatus;
        Irp->IoStatus.Information = info;

        IoCompleteRequest(Irp, IO_NO_INCREMENT);
    }
    else
    {
        // pIrp status already filled in by the lower layer (as we reuse the IRP for sending URBs)
        Assert(code == SUPUSB_IOCTL_SEND_URB);
    }

    dprintf(("VBoxUSB_DispatchDevCtrl::"));
    VBoxUSB_IoDecrement(deviceExtension);

    return ntStatus;
}

NTSTATUS
VBoxUSB_ResetPipe(
    IN PDEVICE_OBJECT         DeviceObject,
    IN USBD_PIPE_HANDLE       PipeHandle
    )
/*++

Routine Description:

    This routine synchronously submits a URB_FUNCTION_RESET_PIPE
    request down the stack.

Arguments:

    DeviceObject - pointer to device object
    PipeHandle - pipe handle

Return Value:

    NT status value

--*/
{
    PURB              urb;
    NTSTATUS          ntStatus;
    PDEVICE_EXTENSION deviceExtension;

    //
    // initialize variables
    //

    urb = NULL;
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    urb = (PURB)ExAllocatePool(NonPagedPool, sizeof(struct _URB_PIPE_REQUEST));
    if(urb) {

        urb->UrbHeader.Length = (USHORT) sizeof(struct _URB_PIPE_REQUEST);
        urb->UrbHeader.Function = URB_FUNCTION_RESET_PIPE;
        urb->UrbPipeRequest.PipeHandle = PipeHandle;

        ntStatus = CallUSBD(DeviceObject, urb);

        ExFreePool(urb);
    }
    else {

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

    if(NT_SUCCESS(ntStatus)) {

        dprintf(("VBoxUSB_ResetPipe - success\n"));
        ntStatus = STATUS_SUCCESS;
    }
    else {

        dprintf(("VBoxUSB_ResetPipe - failed\n"));
    }

    return ntStatus;
}

NTSTATUS
VBoxUSB_ResetDevice(
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine invokes VBoxUSB_ResetParentPort to reset the device

Arguments:

    DeviceObject - pointer to device object

Return Value:

    NT status value

--*/
{
    NTSTATUS ntStatus;
    ULONG    portStatus;

    dprintf(("VBoxUSB_ResetDevice - begins\n"));

    ntStatus = VBoxUSB_GetPortStatus(DeviceObject, &portStatus);

    if((NT_SUCCESS(ntStatus))                 &&
       (!(portStatus & USBD_PORT_ENABLED))    &&
       (portStatus & USBD_PORT_CONNECTED)) {

        ntStatus = VBoxUSB_ResetParentPort(DeviceObject);
    }

    dprintf(("VBoxUSB_ResetDevice - ends\n"));

    return ntStatus;
}

NTSTATUS
VBoxUSB_GetPortStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN OUT PULONG     PortStatus
    )
/*++

Routine Description:

    This routine retrieves the status value

Arguments:

    DeviceObject - pointer to device object
    PortStatus - port status

Return Value:

    NT status value

--*/
{
    NTSTATUS           ntStatus;
    KEVENT             event;
    PIRP               irp;
    IO_STATUS_BLOCK    ioStatus;
    PIO_STACK_LOCATION nextStack;
    PDEVICE_EXTENSION  deviceExtension;

    //
    // initialize variables
    //
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    *PortStatus = 0;

    dprintf(("VBoxUSB_GetPortStatus - begins\n"));

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
                    IOCTL_INTERNAL_USB_GET_PORT_STATUS,
                    deviceExtension->TopOfStackDeviceObject,
                    NULL,
                    0,
                    NULL,
                    0,
                    TRUE,
                    &event,
                    &ioStatus);

    if(NULL == irp) {

        dprintf(("memory alloc for irp failed\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    nextStack = IoGetNextIrpStackLocation(irp);

    ASSERT(nextStack != NULL);

    nextStack->Parameters.Others.Argument1 = PortStatus;

    ntStatus = IoCallDriver(deviceExtension->TopOfStackDeviceObject, irp);

    if(STATUS_PENDING == ntStatus) {

        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    else {

        ioStatus.Status = ntStatus;
    }

    ntStatus = ioStatus.Status;

    dprintf(("VBoxUSB_GetPortStatus - ends\n"));

    return ntStatus;
}

NTSTATUS
VBoxUSB_ResetParentPort(
    IN PDEVICE_OBJECT DeviceObject
    )
/*++

Routine Description:

    This routine sends an IOCTL_INTERNAL_USB_RESET_PORT
    synchronously down the stack.

Arguments:

Return Value:

--*/
{
    NTSTATUS           ntStatus;
    KEVENT             event;
    PIRP               irp;
    IO_STATUS_BLOCK    ioStatus;
    PIO_STACK_LOCATION nextStack;
    PDEVICE_EXTENSION  deviceExtension;

    //
    // initialize variables
    //
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    dprintf(("VBoxUSB_ResetParentPort - begins\n"));

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    irp = IoBuildDeviceIoControlRequest(
                    IOCTL_INTERNAL_USB_RESET_PORT,
                    deviceExtension->TopOfStackDeviceObject,
                    NULL,
                    0,
                    NULL,
                    0,
                    TRUE,
                    &event,
                    &ioStatus);

    if(NULL == irp) {

        dprintf(("memory alloc for irp failed\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    nextStack = IoGetNextIrpStackLocation(irp);

    ASSERT(nextStack != NULL);

    ntStatus = IoCallDriver(deviceExtension->TopOfStackDeviceObject, irp);

    if(STATUS_PENDING == ntStatus) {

        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
    }
    else {

        ioStatus.Status = ntStatus;
    }

    ntStatus = ioStatus.Status;

    dprintf(("VBoxUSB_ResetParentPort - ends\n"));

    return ntStatus;
}


NTSTATUS
SubmitIdleRequestIrp(
    IN PDEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This routine builds an idle request irp with an associated callback routine
    and a completion routine in the driver and passes the irp down the stack.

Arguments:

    DeviceExtension - pointer to device extension

Return Value:

    NT status value

--*/
{
    PIRP                    irp;
    NTSTATUS                ntStatus;
    KIRQL                   oldIrql;
    PUSB_IDLE_CALLBACK_INFO idleCallbackInfo;
    PIO_STACK_LOCATION      nextStack;

    //
    // initialize variables
    //

    irp = NULL;
    idleCallbackInfo = NULL;

    dprintf(("SubmitIdleRequest - begins\n"));

    ASSERT(KeGetCurrentIrql() == PASSIVE_LEVEL);

    if(PowerDeviceD0 != DeviceExtension->DevPower) {

        ntStatus = STATUS_POWER_STATE_INVALID;

        goto SubmitIdleRequestIrp_Exit;
    }

    KeAcquireSpinLock(&DeviceExtension->IdleReqStateLock, &oldIrql);

    if(InterlockedExchange(&DeviceExtension->IdleReqPend, 1)) {

        dprintf(("Idle request pending..\n"));

        KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

        ntStatus = STATUS_DEVICE_BUSY;

        goto SubmitIdleRequestIrp_Exit;
    }

    //
    // clear the NoIdleReqPendEvent because we are about
    // to submit an idle request. Since we are so early
    // to clear this event, make sure that if we fail this
    // request we set back the event.
    //
    KeClearEvent(&DeviceExtension->NoIdleReqPendEvent);

    idleCallbackInfo = (PUSB_IDLE_CALLBACK_INFO)ExAllocatePool(NonPagedPool, sizeof(struct _USB_IDLE_CALLBACK_INFO));

    if(idleCallbackInfo) {

        idleCallbackInfo->IdleCallback = (USB_IDLE_CALLBACK)IdleNotificationCallback;

        idleCallbackInfo->IdleContext = (PVOID)DeviceExtension;

        ASSERT(DeviceExtension->IdleCallbackInfo == NULL);

        DeviceExtension->IdleCallbackInfo = idleCallbackInfo;

        //
        // we use IoAllocateIrp to create an irp to selectively suspend the
        // device. This irp lies pending with the hub driver. When appropriate
        // the hub driver will invoked callback, where we power down. The completion
        // routine is invoked when we power back.
        //
        irp = IoAllocateIrp(DeviceExtension->TopOfStackDeviceObject->StackSize,
                            FALSE);

        if(irp == NULL) {

            dprintf(("cannot build idle request irp\n"));

            KeSetEvent(&DeviceExtension->NoIdleReqPendEvent,
                       IO_NO_INCREMENT,
                       FALSE);

            InterlockedExchange(&DeviceExtension->IdleReqPend, 0);

            KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

            ExFreePool(idleCallbackInfo);

            ntStatus = STATUS_INSUFFICIENT_RESOURCES;

            goto SubmitIdleRequestIrp_Exit;
        }

        nextStack = IoGetNextIrpStackLocation(irp);

        nextStack->MajorFunction =
                    IRP_MJ_INTERNAL_DEVICE_CONTROL;

        nextStack->Parameters.DeviceIoControl.IoControlCode =
                    IOCTL_INTERNAL_USB_SUBMIT_IDLE_NOTIFICATION;

        nextStack->Parameters.DeviceIoControl.Type3InputBuffer =
                    idleCallbackInfo;

        nextStack->Parameters.DeviceIoControl.InputBufferLength =
                    sizeof(struct _USB_IDLE_CALLBACK_INFO);


        IoSetCompletionRoutine(irp,
                               (PIO_COMPLETION_ROUTINE)IdleNotificationRequestComplete,
                               DeviceExtension,
                               TRUE,
                               TRUE,
                               TRUE);

        DeviceExtension->PendingIdleIrp = irp;

        //
        // we initialize the count to 2.
        // The reason is, if the CancelSelectSuspend routine manages
        // to grab the irp from the device extension, then the last of the
        // CancelSelectSuspend routine/IdleNotificationRequestComplete routine
        // to execute will free this irp. We need to have this schema so that
        // 1. completion routine does not attempt to touch the irp freed by
        //    CancelSelectSuspend routine.
        // 2. CancelSelectSuspend routine doesnt wait for ever for the completion
        //    routine to complete!
        //
        DeviceExtension->FreeIdleIrpCount = 2;

        KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

        //
        // check if the device is idle.
        // A check here ensures that a race condition did not
        // completely reverse the call sequence of SubmitIdleRequestIrp
        // and CancelSelectiveSuspend
        //

        if(!CanDeviceSuspend(DeviceExtension) ||
           PowerDeviceD0 != DeviceExtension->DevPower) {

            //
            // IRPs created using IoBuildDeviceIoControlRequest should be
            // completed by calling IoCompleteRequest and not merely
            // deallocated.
            //

            dprintf(("Device is not idle\n"));

            KeAcquireSpinLock(&DeviceExtension->IdleReqStateLock, &oldIrql);

            DeviceExtension->IdleCallbackInfo = NULL;

            DeviceExtension->PendingIdleIrp = NULL;

            KeSetEvent(&DeviceExtension->NoIdleReqPendEvent,
                       IO_NO_INCREMENT,
                       FALSE);

            InterlockedExchange(&DeviceExtension->IdleReqPend, 0);

            KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

            if(idleCallbackInfo) {

                ExFreePool(idleCallbackInfo);
            }

            //
            // it is still safe to touch the local variable "irp" here.
            // the irp has not been passed down the stack, the irp has
            // no cancellation routine. The worse position is that the
            // CancelSelectSuspend has run after we released the spin
            // lock above. It is still essential to free the irp.
            //
            if(irp) {

                IoFreeIrp(irp);
            }

            ntStatus = STATUS_UNSUCCESSFUL;

            goto SubmitIdleRequestIrp_Exit;
        }

        dprintf(("Cancel the timers\n"));
        //
        // Cancel the timer so that the DPCs are no longer fired.
        // Thus, we are making judicious usage of our resources.
        // we do not need DPCs because we already have an idle irp pending.
        // The timers are re-initialized in the completion routine.
        //
        KeCancelTimer(&DeviceExtension->Timer);

        ntStatus = IoCallDriver(DeviceExtension->TopOfStackDeviceObject, irp);

        if(!NT_SUCCESS(ntStatus)) {

            dprintf(("IoCallDriver failed\n"));

            goto SubmitIdleRequestIrp_Exit;
        }
    }
    else {

        dprintf(("Memory allocation for idleCallbackInfo failed\n"));

        KeSetEvent(&DeviceExtension->NoIdleReqPendEvent,
                   IO_NO_INCREMENT,
                   FALSE);

        InterlockedExchange(&DeviceExtension->IdleReqPend, 0);

        KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }

SubmitIdleRequestIrp_Exit:

    dprintf(("SubmitIdleRequest - ends\n"));

    return ntStatus;
}


VOID
IdleNotificationCallback(
    IN PDEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

  "A pointer to a callback function in your driver is passed down the stack with
   this IOCTL, and it is this callback function that is called by USBHUB when it
   safe for your device to power down."

  "When the callback in your driver is called, all you really need to do is to
   to first ensure that a WaitWake Irp has been submitted for your device, if
   remote wake is possible for your device and then request a SetD2 (or DeviceWake)"

Arguments:

    DeviceExtension - pointer to device extension

Return Value:

    NT status value

--*/
{
    NTSTATUS                ntStatus;
    POWER_STATE             powerState;
    KEVENT                  irpCompletionEvent;
    PIRP_COMPLETION_CONTEXT irpContext;

    dprintf(("IdleNotificationCallback - begins\n"));

    //
    // Dont idle, if the device was just disconnected or being stopped
    // i.e. return for the following DeviceState(s)
    // NotStarted, Stopped, PendingStop, PendingRemove, SurpriseRemoved, Removed
    //

    if(DeviceExtension->DeviceState != Working) {

        return;
    }

    //
    // If there is not already a WW IRP pending, submit one now
    //
    if(DeviceExtension->WaitWakeEnable) {

        IssueWaitWake(DeviceExtension);
    }


    //
    // power down the device
    //

    irpContext = (PIRP_COMPLETION_CONTEXT)
                 ExAllocatePool(NonPagedPool,
                                sizeof(IRP_COMPLETION_CONTEXT));

    if(!irpContext) {

        dprintf(("Failed to alloc memory for irpContext\n"));
        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
    }
    else {

        //
        // increment the count. In the HoldIoRequestWorkerRoutine, the
        // count is decremented twice (one for the system Irp and the
        // other for the device Irp. An increment here compensates for
        // the sytem irp..The decrement corresponding to this increment
        // is in the completion function
        //

        dprintf(("IdleNotificationCallback::"));
        VBoxUSB_IoIncrement(DeviceExtension);

        powerState.DeviceState = (DEVICE_POWER_STATE)DeviceExtension->PowerDownLevel;

        KeInitializeEvent(&irpCompletionEvent, NotificationEvent, FALSE);

        irpContext->DeviceExtension = DeviceExtension;
        irpContext->Event = &irpCompletionEvent;

        ntStatus = PoRequestPowerIrp(
                          DeviceExtension->PhysicalDeviceObject,
                          IRP_MN_SET_POWER,
                          powerState,
                          (PREQUEST_POWER_COMPLETE) PoIrpCompletionFunc,
                          irpContext,
                          NULL);

        if(STATUS_PENDING == ntStatus) {

            dprintf(("IdleNotificationCallback::"
                           "waiting for the power irp to complete\n"));

            KeWaitForSingleObject(&irpCompletionEvent,
                                  Executive,
                                  KernelMode,
                                  FALSE,
                                  NULL);
        }
    }

    if(!NT_SUCCESS(ntStatus)) {

        if(irpContext) {

            ExFreePool(irpContext);
        }
    }

    dprintf(("IdleNotificationCallback - ends\n"));
}


NTSTATUS
IdleNotificationRequestComplete(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp,
    IN PDEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

  Completion routine for idle notification irp

Arguments:

    DeviceObject - pointer to device object
    Irp - I/O request packet
    DeviceExtension - pointer to device extension

Return Value:

    NT status value

--*/
{
    NTSTATUS                ntStatus;
    POWER_STATE             powerState;
    KIRQL                   oldIrql;
    LARGE_INTEGER           dueTime;
    PIRP                    idleIrp;
    PUSB_IDLE_CALLBACK_INFO idleCallbackInfo;

    dprintf(("IdleNotificationRequestCompete - begins\n"));

    idleIrp = NULL;

    //
    // check the Irp status
    //

    ntStatus = Irp->IoStatus.Status;

    if(!NT_SUCCESS(ntStatus) && ntStatus != STATUS_NOT_SUPPORTED) {

        dprintf(("Idle irp completes with error::"));

        switch(ntStatus) {

        case STATUS_INVALID_DEVICE_REQUEST:

            dprintf(("STATUS_INVALID_DEVICE_REQUEST\n"));

            break;

        case STATUS_CANCELLED:

            dprintf(("STATUS_CANCELLED\n"));

            break;

        case STATUS_POWER_STATE_INVALID:

            dprintf(("STATUS_POWER_STATE_INVALID\n"));

            goto IdleNotificationRequestComplete_Exit;

        case STATUS_DEVICE_BUSY:

            dprintf(("STATUS_DEVICE_BUSY\n"));

            break;

        default:

            dprintf(("default: status = %X\n", ntStatus));

            break;
        }

        //
        // if in error, issue a SetD0 (only when not in D0)
        //

        if(PowerDeviceD0 != DeviceExtension->DevPower) {
            dprintf(("IdleNotificationRequestComplete::"));
            VBoxUSB_IoIncrement(DeviceExtension);

            powerState.DeviceState = PowerDeviceD0;

            ntStatus = PoRequestPowerIrp(
                              DeviceExtension->PhysicalDeviceObject,
                              IRP_MN_SET_POWER,
                              powerState,
                              (PREQUEST_POWER_COMPLETE) PoIrpAsyncCompletionFunc,
                              DeviceExtension,
                              NULL);

            if(!NT_SUCCESS(ntStatus)) {

                dprintf(("PoRequestPowerIrp failed\n"));
            }
        }
    }

IdleNotificationRequestComplete_Exit:

    KeAcquireSpinLock(&DeviceExtension->IdleReqStateLock, &oldIrql);

    idleCallbackInfo = DeviceExtension->IdleCallbackInfo;

    DeviceExtension->IdleCallbackInfo = NULL;

    idleIrp = (PIRP) InterlockedExchangePointer(
                                        (PVOID *)&DeviceExtension->PendingIdleIrp,
                                        NULL);

    InterlockedExchange(&DeviceExtension->IdleReqPend, 0);

    KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

    if(idleCallbackInfo) {

        ExFreePool(idleCallbackInfo);
    }

    //
    // since the irp was created using IoAllocateIrp,
    // the Irp needs to be freed using IoFreeIrp.
    // Also return STATUS_MORE_PROCESSING_REQUIRED so that
    // the kernel does not reference this in the near future.
    //

    if(idleIrp) {

        dprintf(("completion routine has a valid irp and frees it\n"));
        IoFreeIrp(Irp);
        KeSetEvent(&DeviceExtension->NoIdleReqPendEvent,
                   IO_NO_INCREMENT,
                   FALSE);
    }
    else {

        //
        // The CancelSelectiveSuspend routine has grabbed the Irp from the device
        // extension. Now the last one to decrement the FreeIdleIrpCount should
        // free the irp.
        //
        if(0 == InterlockedDecrement(&DeviceExtension->FreeIdleIrpCount)) {

            dprintf(("completion routine frees the irp\n"));
            IoFreeIrp(Irp);

            KeSetEvent(&DeviceExtension->NoIdleReqPendEvent,
                       IO_NO_INCREMENT,
                       FALSE);
        }
    }

    if(DeviceExtension->SSEnable) {

        dprintf(("Set the timer to fire DPCs\n"));

        dueTime.QuadPart = -10000 * IDLE_INTERVAL;               // 5000 ms

        KeSetTimerEx(&DeviceExtension->Timer,
                     dueTime,
                     IDLE_INTERVAL,                              // 5000 ms
                     &DeviceExtension->DeferredProcCall);

        dprintf(("IdleNotificationRequestCompete - ends\n"));
    }

    return STATUS_MORE_PROCESSING_REQUIRED;
}

VOID
CancelSelectSuspend(
    IN PDEVICE_EXTENSION DeviceExtension
    )
/*++

Routine Description:

    This routine is invoked to cancel selective suspend request.

Arguments:

    DeviceExtension - pointer to device extension

Return Value:

    None.

--*/
{
    PIRP  irp;
    KIRQL oldIrql;

    irp = NULL;

    dprintf(("CancelSelectSuspend - begins\n"));

    KeAcquireSpinLock(&DeviceExtension->IdleReqStateLock, &oldIrql);

    if(!CanDeviceSuspend(DeviceExtension))
    {
        dprintf(("Device is not idle\n"));

        irp = (PIRP) InterlockedExchangePointer(
                            (PVOID *)&DeviceExtension->PendingIdleIrp,
                            NULL);
    }

    KeReleaseSpinLock(&DeviceExtension->IdleReqStateLock, oldIrql);

    //
    // since we have a valid Irp ptr,
    // we can call IoCancelIrp on it,
    // without the fear of the irp
    // being freed underneath us.
    //
    if(irp) {

        //
        // This routine has the irp pointer.
        // It is safe to call IoCancelIrp because we know that
        // the compleiton routine will not free this irp unless...
        //
        //
        if(IoCancelIrp(irp)) {

            dprintf(("IoCancelIrp returns TRUE\n"));
        }
        else {
            dprintf(("IoCancelIrp returns FALSE\n"));
        }

        //
        // ....we decrement the FreeIdleIrpCount from 2 to 1.
        // if completion routine runs ahead of us, then this routine
        // decrements the FreeIdleIrpCount from 1 to 0 and hence shall
        // free the irp.
        //
        if(0 == InterlockedDecrement(&DeviceExtension->FreeIdleIrpCount)) {

            dprintf(("CancelSelectSuspend frees the irp\n"));
            IoFreeIrp(irp);

            KeSetEvent(&DeviceExtension->NoIdleReqPendEvent,
                       IO_NO_INCREMENT,
                       FALSE);
        }
    }

    dprintf(("CancelSelectSuspend - ends\n"));

    return;
}

VOID
PoIrpCompletionFunc(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Context,
    IN PIO_STATUS_BLOCK IoStatus
    )
/*++

Routine Description:

    Completion routine for power irp PoRequested in
    IdleNotificationCallback.

Arguments:

    DeviceObject - pointer to device object
    MinorFunciton - minor function for the irp.
    PowerState - irp power state
    Context - context passed to the completion function
    IoStatus - status block.

Return Value:

    None

--*/
{
    PIRP_COMPLETION_CONTEXT irpContext;

    //
    // initialize variables
    //
    irpContext = NULL;

    if(Context) {

        irpContext = (PIRP_COMPLETION_CONTEXT) Context;
    }

    //
    // all we do is set the event and decrement the count
    //

    if(irpContext) {

        KeSetEvent(irpContext->Event, 0, FALSE);

        dprintf(("PoIrpCompletionFunc::"));
        VBoxUSB_IoDecrement(irpContext->DeviceExtension);

        ExFreePool(irpContext);
    }

    return;
}

VOID
PoIrpAsyncCompletionFunc(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Context,
    IN PIO_STATUS_BLOCK IoStatus
    )
/*++

Routine Description:

    Completion routine for power irp PoRequested in IdleNotification
    RequestComplete routine.

Arguments:

    DeviceObject - pointer to device object
    MinorFunciton - minor function for the irp.
    PowerState - irp power state
    Context - context passed to the completion function
    IoStatus - status block.

Return Value:

    None

--*/
{
    PDEVICE_EXTENSION DeviceExtension;

    //
    // initialize variables
    //
    DeviceExtension = (PDEVICE_EXTENSION) Context;

    //
    // all we do is decrement the count
    //

    dprintf(("PoIrpAsyncCompletionFunc::"));
    VBoxUSB_IoDecrement(DeviceExtension);

    return;
}

VOID
WWIrpCompletionFunc(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Context,
    IN PIO_STATUS_BLOCK IoStatus
    )
/*++

Routine Description:

    Completion routine for PoRequest wait wake irp

Arguments:

    DeviceObject - pointer to device object
    MinorFunciton - minor function for the irp.
    PowerState - irp power state
    Context - context passed to the completion function
    IoStatus - status block.

Return Value:

    None

--*/
{
    PDEVICE_EXTENSION DeviceExtension;

    //
    // initialize variables
    //
    DeviceExtension = (PDEVICE_EXTENSION) Context;

    //
    // all we do is decrement the count
    //

    dprintf(("WWIrpCompletionFunc::"));
    VBoxUSB_IoDecrement(DeviceExtension);

    return;
}



/**
 * Free per-device interface info
 *
 * @param   pDevice             USB device pointer
 * @param   fAbortPipes         If true, also abort any open pipes
 */
void VBoxUSBFreeInterfaces(PDEVICE_EXTENSION pDevice, bool fAbortPipes)
{
    unsigned i;
    unsigned j;

    /*
     * Free old interface info
     */
    if (pDevice->usbdev.pVBIfaceInfo)
    {
        for (i=0;i<pDevice->usbdev.uNumInterfaces;i++)
        {
            if (pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo)
            {
                if (fAbortPipes)
                {
                    for(j=0; j<pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->NumberOfPipes; j++)
                    {
                        dprintf(("Aborting Pipe %d handle %x address %x\n", j,
                                 pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->Pipes[j].PipeHandle,
                                 pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->Pipes[j].EndpointAddress));
                        VBoxUSBClearPipe(pDevice, pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->Pipes[j].PipeHandle, FALSE);
                    }
                }
                ExFreePool(pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo);
            }
            pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo = NULL;
            if (pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo)
                ExFreePool(pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo);
            pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo = NULL;
        }
        ExFreePool(pDevice->usbdev.pVBIfaceInfo);
        pDevice->usbdev.pVBIfaceInfo = NULL;
    }
}

/**
 * Return handle of pipe that corresponds to given endpoint address
 *
 * @returns Pipe handle (or 0 when not found)
 * @param   pDevice             USB device pointer
 * @param   EndPointAddress     end point address
 */
HANDLE VBoxUSBGetPipeHandle(PDEVICE_EXTENSION pDevice, uint32_t EndPointAddress)
{
    unsigned i, j;

    for (i=0;i<pDevice->usbdev.uNumInterfaces;i++)
    {
        for (j=0;j<pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->NumberOfPipes;j++)
        {
            /* Note that bit 7 determines pipe direction, but is still significant
             * because endpoints may be numbered like 0x01, 0x81, 0x02, 0x82 etc.
             */
            if (pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->Pipes[j].EndpointAddress == EndPointAddress)
                return pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->Pipes[j].PipeHandle;
        }
    }
    return 0;
}

/**
 * Return pipe state information for given endpoint address
 *
 * @returns Pointer to pipe state (or NULL if not found)
 * @param   pDevice             USB device pointer
 * @param   EndPointAddress     end point address
 */
VBOXUSB_PIPE_INFO *VBoxUSBGetPipeState(PDEVICE_EXTENSION pDevice, uint32_t EndPointAddress)
{
    unsigned i, j;

    for (i=0;i<pDevice->usbdev.uNumInterfaces;i++)
    {
        for (j=0;j<pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->NumberOfPipes;j++)
        {
            if (pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo[j].EndpointAddress == EndPointAddress)
                return &pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo[j];
        }
    }
    return NULL;
}

/**
 * Get a valid USB string descriptor language ID (the first ID found).
 *
 * @returns NT Status
 * @param   pDevice     device extension
 * @param   lang_id     pointer to language id
 */
NTSTATUS VBoxUSBGetLangID(PDEVICE_EXTENSION pDevice, int *lang_id)
{
    NTSTATUS                status;
    unsigned                length;
    char                    buffer[MAXIMUM_USB_STRING_LENGTH];
    PUSB_STRING_DESCRIPTOR  pstrdescr = (PUSB_STRING_DESCRIPTOR)&buffer;

    Assert(lang_id);
    *lang_id = 0;

    length = sizeof(buffer);
    memset(pstrdescr, 0, length);
    pstrdescr->bLength         = length;
    pstrdescr->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;

    status = VBoxUSBGetDescriptor(pDevice, pstrdescr, length, USB_STRING_DESCRIPTOR_TYPE, 0, 0);
    if (!NT_SUCCESS(status))
    {
        dprintf(("VBoxUSBGetLangID: language ID table not present (?)\n"));
        goto fail;
    }
    /* Just grab the first lang ID if available. In 99% cases, it will be US English (0x0409).*/
    if (pstrdescr->bLength >= sizeof(USB_STRING_DESCRIPTOR))
    {
        Assert(sizeof(pstrdescr->bString[0]) == sizeof(uint16_t));
        *lang_id = pstrdescr->bString[0];
        status = STATUS_SUCCESS;
    }
    else
        status = STATUS_INVALID_PARAMETER;
fail:
    return status;
}

/**
 * Query device descriptor
 *
 * @returns NT Status
 * @param   pDevice             USB device pointer
 */
NTSTATUS VBoxUSBGetDeviceDescription(PDEVICE_EXTENSION pDevice)
{
    NTSTATUS status;
    PUSB_DEVICE_DESCRIPTOR devdescr = 0;
    PUSB_STRING_DESCRIPTOR pstrdescr = 0;
    uint32_t uLength;

    devdescr = (PUSB_DEVICE_DESCRIPTOR)ExAllocatePool(NonPagedPool,sizeof(USB_DEVICE_DESCRIPTOR));
    if(devdescr == NULL)
    {
        dprintf(("Failed to alloc mem for urb\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto fail;
    }
    memset(devdescr, 0, sizeof(*devdescr));

    status = VBoxUSBGetDescriptor(pDevice, devdescr, sizeof(*devdescr),  USB_DEVICE_DESCRIPTOR_TYPE, 0, 0);
    if (!NT_SUCCESS(status))
    {
        AssertMsgFailed(("VBoxUSBGetDeviceDescription: getting device descriptor failed\n"));
        goto fail;
    }
    dprintf(("Device pid=%x vid=%x rev=%x\n", devdescr->idVendor, devdescr->idProduct, devdescr->bcdDevice));
    pDevice->usbdev.idVendor    = devdescr->idVendor;
    pDevice->usbdev.idProduct   = devdescr->idProduct;
    pDevice->usbdev.bcdDevice   = devdescr->bcdDevice;
    pDevice->usbdev.szSerial[0] = 0;

    UNICODE_STRING ustr;
    ANSI_STRING astr;
    int langId = 0;

    if (devdescr->iSerialNumber
#ifdef DEBUG
        || devdescr->iProduct || devdescr->iManufacturer
#endif
       ) 
    {
        status = VBoxUSBGetLangID(pDevice, &langId);
        if (!NT_SUCCESS(status))
        {
            dprintf(("VBoxUSBGetDeviceDescription: no language ID (?)\n"));
            status = STATUS_SUCCESS;    //not fatal
            goto fail;
        }

        uLength = sizeof(USB_STRING_DESCRIPTOR) + MAX_USB_SERIAL_STRING * sizeof(pstrdescr->bString[0]);

        pstrdescr = (PUSB_STRING_DESCRIPTOR)ExAllocatePool(NonPagedPool, uLength);
        if (!pstrdescr)
        {
            AssertMsgFailed(("VBoxUSBGetDeviceDescription: ExAllocatePool failed\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto fail;
        }
        memset(pstrdescr, 0, uLength);
    
        pstrdescr->bLength         = uLength;
        pstrdescr->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;
    
        status = VBoxUSBGetDescriptor(pDevice, pstrdescr, uLength,  USB_STRING_DESCRIPTOR_TYPE, devdescr->iSerialNumber, langId);
        if (!NT_SUCCESS(status))
        {
            dprintf(("VBoxUSBGetDeviceDescription: no serial string present (?)\n"));
            status = STATUS_SUCCESS;    //not fatal
            goto fail;
        }
        /* Did we get a string back or not? */
        if (pstrdescr->bLength > sizeof(USB_STRING_DESCRIPTOR))
        {
            RtlInitUnicodeString(&ustr, pstrdescr->bString);
            RtlInitAnsiString(&astr, NULL);

            RtlUnicodeStringToAnsiString(&astr, &ustr, TRUE);

            strncpy(pDevice->usbdev.szSerial, astr.Buffer, sizeof(pDevice->usbdev.szSerial));
            RtlFreeAnsiString(&astr);
        }
    }

#ifdef DEBUG
    if (pDevice->usbdev.szSerial[0])
        dprintf(("Serial number %s\n", pDevice->usbdev.szSerial));

    if (devdescr->iManufacturer)
    {
        memset(pstrdescr, 0, uLength);
        pstrdescr->bLength         = uLength;
        pstrdescr->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;

        status = VBoxUSBGetDescriptor(pDevice, pstrdescr, uLength,  USB_STRING_DESCRIPTOR_TYPE, devdescr->iManufacturer, langId);
        if (!NT_SUCCESS(status))
        {
            dprintf(("VBoxUSBGetDeviceDescription: no manufacturer string present (?)\n"));
            status = STATUS_SUCCESS;    //not fatal
            goto fail;
        }
        if (pstrdescr->bLength > sizeof(USB_STRING_DESCRIPTOR))
        {
            RtlInitUnicodeString(&ustr, pstrdescr->bString);
            RtlInitAnsiString(&astr, NULL);

            RtlUnicodeStringToAnsiString(&astr, &ustr, TRUE);
            if (astr.Buffer[0])
                dprintf(("Manufacturer: %s\n", astr.Buffer));
            RtlFreeAnsiString(&astr);
        }
    }
    if (devdescr->iProduct)
    {
        memset(pstrdescr, 0, uLength);
        pstrdescr->bLength         = uLength;
        pstrdescr->bDescriptorType = USB_STRING_DESCRIPTOR_TYPE;

        status = VBoxUSBGetDescriptor(pDevice, pstrdescr, uLength,  USB_STRING_DESCRIPTOR_TYPE, devdescr->iProduct, langId);
        if (!NT_SUCCESS(status))
        {
            dprintf(("VBoxUSBGetDeviceDescription: no product string present (?)\n"));
            status = STATUS_SUCCESS;    //not fatal
            goto fail;
        }
        if (pstrdescr->bLength > sizeof(USB_STRING_DESCRIPTOR))
        {
            RtlInitUnicodeString(&ustr, pstrdescr->bString);
            RtlInitAnsiString(&astr, NULL);

            RtlUnicodeStringToAnsiString(&astr, &ustr, TRUE);
            if (astr.Buffer[0])
                dprintf(("Product: %s\n", astr.Buffer));
            RtlFreeAnsiString(&astr);
        }
    }
#endif

    status = STATUS_SUCCESS;

fail:
    if (pstrdescr)
        ExFreePool(pstrdescr);
    if (devdescr)
        ExFreePool(devdescr);
    return status;
}

static NTSTATUS VBoxUSBClearPipe(PDEVICE_EXTENSION pDevice, HANDLE PipeHandle, bool fReset)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB *urb = NULL;

    urb = (URB *)ExAllocatePool(NonPagedPool, sizeof(URB));
    if (!urb)
    {
        AssertMsgFailed(("VBoxUSBClearPipe: ExAllocatePool failed!\n"));
        status = STATUS_NO_MEMORY;
        goto end;
    }

    if (fReset)
        urb->UrbHeader.Function                 = URB_FUNCTION_RESET_PIPE;
    else
        urb->UrbHeader.Function                 = URB_FUNCTION_ABORT_PIPE;
    urb->UrbHeader.Length                       = sizeof(struct _URB_PIPE_REQUEST);
    urb->UrbPipeRequest.PipeHandle              = PipeHandle;
    if (urb->UrbPipeRequest.PipeHandle == 0)
    {
        // pretend success
        dprintf(("Resetting the control pipe??\n"));
        status = STATUS_SUCCESS;
        goto end;
    }

    urb->UrbPipeRequest.Reserved                = 0;

    status = VBoxUSBSendIOCTL(pDevice, IOCTL_INTERNAL_USB_SUBMIT_URB, urb);
    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
        AssertMsgFailed(("VBoxUSBClearPipe: VBoxUSBSendIOCTL failed with %x (%x)\n", status, urb->UrbHeader.Status));
        goto end;
    }

end:
    if (urb)        ExFreePool(urb);

    return status;
}

static NTSTATUS VBoxUSBClearEndpoint(PDEVICE_EXTENSION pDevice, uint32_t EndPointAddress, bool fReset)
{
    NTSTATUS status = STATUS_SUCCESS;
    URB *urb = NULL;

    status = VBoxUSBClearPipe(pDevice, VBoxUSBGetPipeHandle(pDevice, EndPointAddress), fReset );
    if (!NT_SUCCESS(status))
    {
        AssertMsgFailed(("VBoxUSBClearEndpoint: VBoxUSBSendIOCTL failed with %x (%x)\n", status, urb->UrbHeader.Status));
    }

    return status;
}

static USB_CONFIGURATION_DESCRIPTOR *VBoxUSBFindConfigDesc(PDEVICE_EXTENSION pDevice, unsigned uConfigValue)
{
    USB_CONFIGURATION_DESCRIPTOR *cfgdescr = NULL;
    unsigned i;

    Assert(pDevice);
    for (i = 0; i < MAX_CFGS; ++i)
    {
        if (pDevice->usbdev.cfgdescr[i])
        {
            if (pDevice->usbdev.cfgdescr[i]->bConfigurationValue == uConfigValue)
            {
                cfgdescr = pDevice->usbdev.cfgdescr[i];
                break;
            }
        }
    }

    return cfgdescr;
}

/**
 * Select USB interface
 *
 * @returns NT Status
 * @param   pDevice             USB device pointer
 * @param   InterfaceNumber     interface number
 * @param   AlternateSetting    alternate setting
 */
NTSTATUS VBoxUSBSetInterface(PDEVICE_EXTENSION pDevice, uint32_t InterfaceNumber, int AlternateSetting)
{
    NTSTATUS status;
    URB *urb = NULL;
    USB_CONFIGURATION_DESCRIPTOR *cfgdescr = NULL;
    USB_INTERFACE_DESCRIPTOR *ifacedesc = NULL;
    USBD_INTERFACE_LIST_ENTRY *interfaces = NULL;
    uint32_t uTotalIfaceInfoLength;
    USHORT uUrbSize;
    unsigned i;

    if (!pDevice->usbdev.uConfigValue)
    {
        AssertMsgFailed(("Can't select an interface without an active configuration\n"));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    if (InterfaceNumber >= pDevice->usbdev.uNumInterfaces)
    {
        AssertMsgFailed(("InterfaceNumber %d too high!!\n", InterfaceNumber));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    cfgdescr = VBoxUSBFindConfigDesc(pDevice, pDevice->usbdev.uConfigValue);
    if (!cfgdescr)
    {
        AssertMsgFailed(("VBoxUSBSetInterface: configuration %d not found!!\n", pDevice->usbdev.uConfigValue));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    dprintf(("Calling USBD_ParseConfigurationDescriptorEx...\n"));
    ifacedesc = USBD_ParseConfigurationDescriptorEx(cfgdescr, cfgdescr, InterfaceNumber, AlternateSetting, -1, -1, -1);
    if (!ifacedesc)
    {
        AssertMsgFailed(("VBoxUSBSetInterface: invalid interface %d or alternate setting %d\n", InterfaceNumber, AlternateSetting));
        status = STATUS_UNSUCCESSFUL;
        goto end;
    }
    dprintf(("USBD_ParseConfigurationDescriptorEx successful\n"));

    uUrbSize = GET_SELECT_INTERFACE_REQUEST_SIZE(ifacedesc->bNumEndpoints);
    uTotalIfaceInfoLength = GET_USBD_INTERFACE_SIZE(ifacedesc->bNumEndpoints);

    urb = (URB *)ExAllocatePool(NonPagedPool, uUrbSize);
    if (!urb)
    {
        AssertMsgFailed(("VBoxUSBSetInterface: ExAllocatePool failed!\n"));
        status = STATUS_NO_MEMORY;
        goto end;
    }

    /*
     * Free old interface and pipe info, allocate new again
     */
    if (pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo) {
        /* Clear pipes associated with the interface, else Windows may hang. */
        for(i=0; i<pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo->NumberOfPipes; i++)
        {
            dprintf(("Aborting Pipe %d handle %x address %x\n", i,
                     pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo->Pipes[i].PipeHandle,
                     pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo->Pipes[i].EndpointAddress));
            VBoxUSBClearPipe(pDevice, pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo->Pipes[i].PipeHandle, FALSE);
        }
        ExFreePool(pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo);
    }
    if (pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo)
        ExFreePool(pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo);

    pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo = (PUSBD_INTERFACE_INFORMATION) ExAllocatePool(NonPagedPool, uTotalIfaceInfoLength);
    if (!pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo)
    {
        AssertMsgFailed(("VBoxUSBSetInterface: ExAllocatePool failed!\n"));
        status = STATUS_NO_MEMORY;
        goto end;
    }
    if (ifacedesc->bNumEndpoints > 0) {
        pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo = (VBOXUSB_PIPE_INFO *) ExAllocatePool(NonPagedPool, ifacedesc->bNumEndpoints * sizeof(VBOXUSB_PIPE_INFO));
        if (!pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo)
        {
            AssertMsgFailed(("VBoxUSBSetInterface: ExAllocatePool failed!\n"));
            status = STATUS_NO_MEMORY;
            goto end;
        }
    }
    else
        pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo = NULL;

    memset(urb, 0, uUrbSize);
    UsbBuildSelectInterfaceRequest(urb, uUrbSize, pDevice->usbdev.hConfiguration, InterfaceNumber, AlternateSetting);
    urb->UrbSelectInterface.Interface.Length = GET_USBD_INTERFACE_SIZE(ifacedesc->bNumEndpoints);

    status = VBoxUSBSendIOCTL(pDevice, IOCTL_INTERNAL_USB_SUBMIT_URB, urb);
//    status = VBoxUSBSyncSendRequest(pDevice, IOCTL_INTERNAL_USB_SUBMIT_URB, urb);
    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
        AssertMsgFailed(("VBoxUSBSetInterface: VBoxUSBSendIOCTL failed with %x (%x)\n", status, urb->UrbHeader.Status));
        goto end;
    }

    USBD_INTERFACE_INFORMATION *ifaceinfo = &urb->UrbSelectInterface.Interface;
    memcpy(pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo, ifaceinfo, GET_USBD_INTERFACE_SIZE(ifacedesc->bNumEndpoints));

    Assert(ifaceinfo->NumberOfPipes == ifacedesc->bNumEndpoints);
    for(i=0;i<ifaceinfo->NumberOfPipes;i++)
    {
        dprintf(("Pipe %d: handle %x address %x transfer size=%d\n", i, ifaceinfo->Pipes[i].PipeHandle, ifaceinfo->Pipes[i].EndpointAddress, ifaceinfo->Pipes[i].MaximumTransferSize));
        pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pInterfaceInfo->Pipes[i] = ifaceinfo->Pipes[i];
        pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo[i].EndpointAddress = ifaceinfo->Pipes[i].EndpointAddress;
        pDevice->usbdev.pVBIfaceInfo[InterfaceNumber].pPipeInfo[i].NextScheduledFrame = 0;
    }

end:
    if (interfaces) ExFreePool(interfaces);
    if (urb)        ExFreePool(urb);

    return status;
}

/**
 * Select USB configuration
 *
 * @returns NT Status
 * @param   pDevice             USB device pointer
 * @param   uConfiguration      configuration value
 */
NTSTATUS VBoxUSBSetConfig(PDEVICE_EXTENSION pDevice, unsigned uConfiguration)
{
    NTSTATUS status;
    URB *urb = NULL;
    USB_CONFIGURATION_DESCRIPTOR *cfgdescr = NULL;
    USBD_INTERFACE_LIST_ENTRY *interfaces = NULL;
    unsigned i;

    if (uConfiguration == 0)
    {
        urb = (PURB)ExAllocatePool(NonPagedPool,sizeof(URB));
        if(urb == NULL)
        {
            dprintf(("VBoxUSBSetConfig: Failed to alloc mem for urb\n"));
            return STATUS_INSUFFICIENT_RESOURCES;
        }

        /* Before setting the configuration, free any existing interface information.
         * Also abort any open pipes, else the set config request will hang.
         */
        VBoxUSBFreeInterfaces(pDevice, TRUE);

        memset(urb, 0, sizeof(URB));
        urb->UrbHeader.Function = URB_FUNCTION_SELECT_CONFIGURATION;
        urb->UrbHeader.Length   = sizeof(struct _URB_SELECT_CONFIGURATION);
        urb->UrbSelectConfiguration.ConfigurationDescriptor = NULL; //no config

        status = VBoxUSBSendIOCTL(pDevice, IOCTL_INTERNAL_USB_SUBMIT_URB, urb);
        if(!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
        {
            AssertMsgFailed(("VBoxUSBSetConfig: VBoxUSBSendIOCTL failed with %x (%x)\n", status, urb->UrbHeader.Status));
            ExFreePool(urb);
            return status;
        }

        pDevice->usbdev.hConfiguration = urb->UrbSelectConfiguration.ConfigurationHandle;
        pDevice->usbdev.uConfigValue   = uConfiguration;
        ExFreePool(urb);
        return status;
    }

    cfgdescr = VBoxUSBFindConfigDesc(pDevice, uConfiguration);
    if (!cfgdescr)
    {
        AssertMsgFailed(("VBoxUSBSetConfig: configuration %d not found\n", uConfiguration));
        status = STATUS_INVALID_PARAMETER;
        goto end;
    }

    interfaces = (USBD_INTERFACE_LIST_ENTRY *) ExAllocatePool(NonPagedPool,(cfgdescr->bNumInterfaces + 1) * sizeof(USBD_INTERFACE_LIST_ENTRY));
    if (!interfaces)
    {
        AssertMsgFailed(("VBoxUSBSetConfig: ExAllocatePool failed!\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto end;
    }

    memset(interfaces, 0, (cfgdescr->bNumInterfaces + 1) * sizeof(USBD_INTERFACE_LIST_ENTRY));

    dprintf(("NumInterfaces %d\n", cfgdescr->bNumInterfaces));
    for(i=0;i<cfgdescr->bNumInterfaces;i++)
    {
        interfaces[i].InterfaceDescriptor = USBD_ParseConfigurationDescriptorEx(cfgdescr, cfgdescr, i, 0, -1, -1, -1);
        if (!interfaces[i].InterfaceDescriptor)
        {
            AssertMsgFailed(("VBoxUSBSetConfig: interface %d not found\n", i));
            status = STATUS_INVALID_PARAMETER;
            goto end;
        }
        else
        {
            dprintf(("VBoxUSBSetConfig: interface %d found\n", i));
        }
    }

    urb = USBD_CreateConfigurationRequestEx(cfgdescr, interfaces);
    if (!urb)
    {
        AssertMsgFailed(("VBoxUSBSetConfig: USBD_CreateConfigurationRequestEx failed!\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto end;
    }

    status = VBoxUSBSendIOCTL(pDevice, IOCTL_INTERNAL_USB_SUBMIT_URB, urb);
    if (!NT_SUCCESS(status) || !USBD_SUCCESS(urb->UrbHeader.Status))
    {
        AssertMsgFailed(("VBoxUSBSetConfig: VBoxUSBSendIOCTL failed with %x (%x)\n", status, urb->UrbHeader.Status));
        goto end;
    }

    /*
     * Free per-device interface info
     */
    VBoxUSBFreeInterfaces(pDevice, FALSE);

    pDevice->usbdev.hConfiguration = urb->UrbSelectConfiguration.ConfigurationHandle;
    pDevice->usbdev.uConfigValue   = uConfiguration;
    pDevice->usbdev.uNumInterfaces = cfgdescr->bNumInterfaces;

    /*
     * Allocate room for interface pointer array
     */
    pDevice->usbdev.pVBIfaceInfo = (VBOXUSB_IFACE_INFO *)ExAllocatePool(NonPagedPool, pDevice->usbdev.uNumInterfaces * sizeof(VBOXUSB_IFACE_INFO));
    if (!pDevice->usbdev.pVBIfaceInfo)
    {
        AssertMsgFailed(("VBoxUSBSetConfig: ExAllocatePool failed!\n"));
        status = STATUS_NO_MEMORY;
        goto end;
    }
    memset(pDevice->usbdev.pVBIfaceInfo, 0, pDevice->usbdev.uNumInterfaces * sizeof(VBOXUSB_IFACE_INFO));

    /*
     * And fill in the information for all interfaces
     */
    for (i=0;i<pDevice->usbdev.uNumInterfaces;i++)
    {
        uint32_t uTotalIfaceInfoLength = sizeof(struct _URB_SELECT_INTERFACE) + ((interfaces[i].Interface->NumberOfPipes > 0) ? (interfaces[i].Interface->NumberOfPipes - 1) : 0) * sizeof(USBD_PIPE_INFORMATION);

        pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo = (PUSBD_INTERFACE_INFORMATION) ExAllocatePool(NonPagedPool, uTotalIfaceInfoLength);
        if (!pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo)
        {
            AssertMsgFailed(("VBoxUSBSetConfig: ExAllocatePool failed!\n"));
            status = STATUS_INSUFFICIENT_RESOURCES;
            goto end;
        }
        if (interfaces[i].Interface->NumberOfPipes > 0)
        {
            pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo = (VBOXUSB_PIPE_INFO *) ExAllocatePool(NonPagedPool, interfaces[i].Interface->NumberOfPipes * sizeof(VBOXUSB_PIPE_INFO));
            if (!pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo)
            {
                AssertMsgFailed(("VBoxUSBSetConfig: ExAllocatePool failed!\n"));
                status = STATUS_NO_MEMORY;
                goto end;
            }
        }
        else
            pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo = NULL;

        *pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo = *interfaces[i].Interface;

        dprintf(("interface %d nr   %d\n", i, interfaces[i].Interface->InterfaceNumber));
        dprintf(("interface %d aset %d\n", i, interfaces[i].Interface->AlternateSetting));

        for (int j=0;j<(int)interfaces[i].Interface->NumberOfPipes;j++)
        {
            dprintf(("Pipe %d: handle %x address %x transfer size=%d\n", j, interfaces[i].Interface->Pipes[j].PipeHandle, interfaces[i].Interface->Pipes[j].EndpointAddress, interfaces[i].Interface->Pipes[j].MaximumTransferSize));
            pDevice->usbdev.pVBIfaceInfo[i].pInterfaceInfo->Pipes[j] = interfaces[i].Interface->Pipes[j];
            pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo[j].EndpointAddress = interfaces[i].Interface->Pipes[j].EndpointAddress;
            pDevice->usbdev.pVBIfaceInfo[i].pPipeInfo[j].NextScheduledFrame = 0;

        }
    }

end:
    if (interfaces) ExFreePool(interfaces);
    if (urb)        ExFreePool(urb);

    return status;
}



// Generic completion routine for simple requests
static NTSTATUS _stdcall VBoxUSB_StopCompletion(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PKEVENT event;

    event = (PKEVENT) Context;
    KeSetEvent(event, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

// Send IRP/URB to fetch the current frame number for isochronous transfers
static ULONG VBoxUSB_GetCurrentFrame(PDEVICE_EXTENSION DeviceExtension, PIRP Irp)
{
    KEVENT                               event;
    PIO_STACK_LOCATION                   nextStack;
    struct _URB_GET_CURRENT_FRAME_NUMBER urb;

    // initialize the urb
    urb.Hdr.Function = URB_FUNCTION_GET_CURRENT_FRAME_NUMBER;
    urb.Hdr.Length = sizeof(urb);
    urb.FrameNumber = (ULONG) -1;

    nextStack = IoGetNextIrpStackLocation(Irp);
    nextStack->Parameters.Others.Argument1 = (PVOID) &urb;
    nextStack->Parameters.DeviceIoControl.IoControlCode =
                                IOCTL_INTERNAL_USB_SUBMIT_URB;
    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;

    KeInitializeEvent(&event, NotificationEvent, FALSE);
    IoSetCompletionRoutine(Irp, VBoxUSB_StopCompletion, &event, TRUE, TRUE, TRUE);

    dprintf(("VBoxUSB_GetCurrentFrame::"));
    VBoxUSB_IoIncrement(DeviceExtension);

    IoCallDriver(DeviceExtension->TopOfStackDeviceObject, Irp);
    KeWaitForSingleObject((PVOID) &event, Executive, KernelMode, FALSE, NULL);

    dprintf(("VBoxUSB_GetCurrentFrame::"));
    VBoxUSB_IoDecrement(DeviceExtension);

    dprintf(("VBoxUSB_GetCurrentFrame returns %d\n", urb.FrameNumber));

    return urb.FrameNumber;
}

/**
 * Query device speed (High-speed vs. low/full-speed)
 *
 * @returns NT Status
 * @param   pDevice             USB device pointer
 */
NTSTATUS VBoxUSBGetDeviceSpeed(PDEVICE_EXTENSION pDevExt)
{
    PIRP                       irp;
    KEVENT                     event;
    NTSTATUS                   ntStatus;
    PIO_STACK_LOCATION         nextStack;
    USB_BUS_INTERFACE_USBDI_V1 busInterfaceVer1;

    irp = IoAllocateIrp(pDevExt->TopOfStackDeviceObject->StackSize, FALSE);

    if (irp == NULL) {
        AssertMsgFailed(("VBoxUSBGetDeviceSpeed: Failed to allocate IRP!\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // All PnP IRPs need the status field initialized to STATUS_NOT_SUPPORTED
    irp->IoStatus.Status = STATUS_NOT_SUPPORTED;
    KeInitializeEvent(&event, NotificationEvent, FALSE);

    // Set the completion routine, which will signal the event
    IoSetCompletionRoutine(irp,
                           VBoxUSB_StopCompletion,
                           &event,
                           TRUE,    // InvokeOnSuccess
                           TRUE,    // InvokeOnError
                           TRUE);   // InvokeOnCancel

    nextStack = IoGetNextIrpStackLocation(irp);
    Assert(nextStack);
    nextStack->MajorFunction = IRP_MJ_PNP;
    nextStack->MinorFunction = IRP_MN_QUERY_INTERFACE;

    // Allocate memory for an interface of type
    // USB_BUS_INTERFACE_USBDI_V1 and set up the IRP
    nextStack->Parameters.QueryInterface.Interface = (PINTERFACE)&busInterfaceVer1;
    nextStack->Parameters.QueryInterface.InterfaceSpecificData = NULL;
    nextStack->Parameters.QueryInterface.InterfaceType = &USB_BUS_INTERFACE_USBDI_GUID;
    nextStack->Parameters.QueryInterface.Size = sizeof(USB_BUS_INTERFACE_USBDI_V1);
    nextStack->Parameters.QueryInterface.Version = USB_BUSIF_USBDI_VERSION_1;

    dprintf(("VBoxUSBGetDeviceSpeed::"));
    VBoxUSB_IoIncrement(pDevExt);

    ntStatus = IoCallDriver(pDevExt->TopOfStackDeviceObject, irp);
    if(STATUS_PENDING == ntStatus) {
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        ntStatus = irp->IoStatus.Status;
    }

    if(NT_SUCCESS(ntStatus)) {
        pDevExt->usbdev.fIsHighSpeed = busInterfaceVer1.IsDeviceHighSpeed(busInterfaceVer1.BusContext);
        dprintf(("VBoxUSBGetDeviceSpeed: IsDeviceHighSpeed = %x\n", pDevExt->usbdev.fIsHighSpeed));
    }

    IoFreeIrp(irp);

    dprintf(("VBoxUSBGetDeviceSpeed::"));
    VBoxUSB_IoDecrement(pDevExt);
    return ntStatus;
}

static NTSTATUS _stdcall VBoxUSBIOCTLAsyncCompletion(DEVICE_OBJECT *device_object, IRP *pIrp, void *context)
{
    PVBOXUSB_URB_CONTEXT pContext = (PVBOXUSB_URB_CONTEXT)context;
    PURB            urb = NULL;
    PMDL            pMdlBuf = NULL;
    PUSBSUP_URB     pOut;
    NTSTATUS        ntStatus;
    PDEVICE_EXTENSION pDevExt;

    if (!context)
    {
        AssertFailed();
        pIrp->IoStatus.Information = 0;
        return STATUS_CONTINUE_COMPLETION;
    }

    if (pContext->ulMagic != VBOXUSB_MAGIC)
    {
        AssertMsgFailed(("Invalid context magic!!\n"));
        pIrp->IoStatus.Information = 0;
        return STATUS_CONTINUE_COMPLETION;
    }

    urb     = pContext->pUrb;
    pMdlBuf = pContext->pMdlBuf;
    pOut    = (PUSBSUP_URB)pContext->pOut;
    pDevExt = (PDEVICE_EXTENSION)pContext->DeviceExtension;

    dprintf(("VBoxURBAsyncCompletion %p status=%x URB %p IRQL=%d\n", pIrp, pIrp->IoStatus.Status, urb, KeGetCurrentIrql()));

    if (!urb || !pMdlBuf || !pOut | !pDevExt)
    {
        AssertFailed();
        if (pDevExt)
            VBoxUSB_IoDecrement(pDevExt);
        pIrp->IoStatus.Information = 0;
        return STATUS_CONTINUE_COMPLETION;
    }

    ////rt(MmIsAddressValid(pOut) == TRUE);

    ntStatus = pIrp->IoStatus.Status;
    if (ntStatus == STATUS_SUCCESS)
    {
        switch(urb->UrbHeader.Status)
        {
        case USBD_STATUS_CRC:
            pOut->error = USBSUP_XFER_CRC;
            break;
        case USBD_STATUS_SUCCESS:
            pOut->error = USBSUP_XFER_OK;
            break;
        case USBD_STATUS_STALL_PID:
            pOut->error = USBSUP_XFER_STALL;
            break;
        case USBD_STATUS_INVALID_URB_FUNCTION:
        case USBD_STATUS_INVALID_PARAMETER:
            AssertFailed();     // SW error - we probably messed up
            // fall through
        case USBD_STATUS_DEV_NOT_RESPONDING:
        default:
            pOut->error = USBSUP_XFER_DNR;
            break;
        }

        switch(pContext->ulTransferType)
        {
        case USBSUP_TRANSFER_TYPE_CTRL:
        case USBSUP_TRANSFER_TYPE_MSG:
            pOut->len = urb->UrbControlTransfer.TransferBufferLength;
            if (pContext->ulTransferType == USBSUP_TRANSFER_TYPE_MSG)
            {
               /* QUSB_TRANSFER_TYPE_MSG is a control transfer, but it is special
                * the first 8 bytes of the buffer is the setup packet so the real
                * data length is therefore urb->len - 8
                */
                pOut->len += sizeof(urb->UrbControlTransfer.SetupPacket);
            }
            break;
        case USBSUP_TRANSFER_TYPE_ISOC:
            pOut->len = urb->UrbIsochronousTransfer.TransferBufferLength;
            break;
        case USBSUP_TRANSFER_TYPE_BULK:
        case USBSUP_TRANSFER_TYPE_INTR:
            if (  (pOut->dir == USBSUP_DIRECTION_IN) && (pOut->error == USBSUP_XFER_OK)
              && !(pOut->flags & USBSUP_FLAG_SHORT_OK) && (pOut->len > urb->UrbBulkOrInterruptTransfer.TransferBufferLength))
            {
                /* If we don't use the USBD_SHORT_TRANSFER_OK flag, the returned buffer lengths are
                 * wrong for short transfers (always a multiple of max packet size?). So we just figure
                 * out if this was a data underrun on our own.
                 */
                pOut->error = USBSUP_XFER_UNDERRUN;
            }
            pOut->len = urb->UrbBulkOrInterruptTransfer.TransferBufferLength;
            break;
        }
    }
    else
    {
        pOut->len = 0;

        dprintf(("URB failed with %x\n", urb->UrbHeader.Status));
#ifdef DEBUG
        switch(pContext->ulTransferType)
        {
        case USBSUP_TRANSFER_TYPE_CTRL:
        case USBSUP_TRANSFER_TYPE_MSG:
            dprintf(("Ctrl/Msg length=%d\n", urb->UrbControlTransfer.TransferBufferLength));
            break;
        case USBSUP_TRANSFER_TYPE_ISOC:
            dprintf(("ISOC length=%d\n", urb->UrbIsochronousTransfer.TransferBufferLength));
            break;
        case USBSUP_TRANSFER_TYPE_BULK:
        case USBSUP_TRANSFER_TYPE_INTR:
            dprintf(("BULK/INTR length=%d\n", urb->UrbBulkOrInterruptTransfer.TransferBufferLength));
            break;
        }
#endif
        switch(urb->UrbHeader.Status)
        {
        case USBD_STATUS_CRC:
            pOut->error = USBSUP_XFER_CRC;
            ntStatus = STATUS_SUCCESS;
            break;
        case USBD_STATUS_STALL_PID:
            pOut->error = USBSUP_XFER_STALL;
            ntStatus = STATUS_SUCCESS;
            break;
        case USBD_STATUS_DEV_NOT_RESPONDING:
            pOut->error = USBSUP_XFER_DNR;
            ntStatus = STATUS_SUCCESS;
            break;
        case ((USBD_STATUS)0xC0010000L): // USBD_STATUS_CANCELED - too bad usbdi.h and usb.h aren't consistent!
            // TODO: What the heck are we really supposed to do here?
            pOut->error = USBSUP_XFER_STALL;
            ntStatus = STATUS_SUCCESS;
            break;
        case USBD_STATUS_BAD_START_FRAME:   // This one really shouldn't happen
        case USBD_STATUS_ISOCH_REQUEST_FAILED:
            pOut->error = USBSUP_XFER_NAC;
            ntStatus = STATUS_SUCCESS;
            break;
        default:
            AssertMsgFailed(("VBoxUSBSendIOCTL returned %x (%x)\n", ntStatus, urb->UrbHeader.Status));
            pOut->error = USBSUP_XFER_DNR;
            ntStatus = STATUS_SUCCESS;
            break;
        }
    }
    // For isochronous transfers, always update the individual packets
    if (pContext->ulTransferType == USBSUP_TRANSFER_TYPE_ISOC)
    {
        Assert(pOut->numIsoPkts == urb->UrbIsochronousTransfer.NumberOfPackets);
        for (unsigned i = 0; i < pOut->numIsoPkts; ++i)
        {
            Assert(pOut->aIsoPkts[i].off == urb->UrbIsochronousTransfer.IsoPacket[i].Offset);
            pOut->aIsoPkts[i].cb = (uint16_t)urb->UrbIsochronousTransfer.IsoPacket[i].Length;
            switch (urb->UrbIsochronousTransfer.IsoPacket[i].Status)
            {
                case USBD_STATUS_SUCCESS:
                    pOut->aIsoPkts[i].stat = USBSUP_XFER_OK;
                    break;
                case USBD_STATUS_NOT_ACCESSED:
                    pOut->aIsoPkts[i].stat = USBSUP_XFER_NAC;
                    break;
                default:
                    pOut->aIsoPkts[i].stat = USBSUP_XFER_STALL;
                    break;
            }
        }
    }
    MmUnlockPages(pMdlBuf);
    IoFreeMdl(pMdlBuf);

    ExFreePool(pContext);

    dprintf(("VBoxUSBIOCTLAsyncCompletion::"));
    VBoxUSB_IoDecrement(pDevExt);

    Assert(pIrp->IoStatus.Status != STATUS_IO_TIMEOUT);
    // Number of bytes returned
    pIrp->IoStatus.Information = sizeof(*pOut);
    pIrp->IoStatus.Status      = ntStatus;
    return STATUS_CONTINUE_COMPLETION;
}

NTSTATUS VBoxUSBSendURB(PDEVICE_EXTENSION pDevExt, PIRP pIrp, PUSBSUP_URB pIn, PUSBSUP_URB pOut)
{
    NTSTATUS        status;
    PIO_STACK_LOCATION stackloc;
    PURB            urb = NULL;
    HANDLE          PipeHandle;
    PMDL            pMdlBuf = NULL;
    PVBOXUSB_URB_CONTEXT pContext = NULL;
    ULONG           urbSize;

    Assert(pIn && pOut);

    // Isochronous transfers use multiple packets -> variable URB size
    if (pIn->type == USBSUP_TRANSFER_TYPE_ISOC)
    {
        Assert(pOut->numIsoPkts <= 8);
        urbSize = GET_ISO_URB_SIZE(pOut->numIsoPkts);
    }
    else
        urbSize = sizeof(URB);

    pContext = (PVBOXUSB_URB_CONTEXT)ExAllocatePool(NonPagedPool, urbSize + sizeof(VBOXUSB_URB_CONTEXT));
    if(pContext == NULL)
    {
        dprintf(("Failed to alloc mem for urb\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendUrbFailure;
    }
    memset(pContext, 0, urbSize + sizeof(VBOXUSB_URB_CONTEXT));
    urb = (PURB)(pContext + 1);

    PipeHandle = 0;
    if (pIn->ep)
    {
        PipeHandle = VBoxUSBGetPipeHandle(pDevExt, pIn->ep | ((pIn->dir == USBSUP_DIRECTION_IN) ? 0x80 : 0x00));
        if (PipeHandle == 0)
        {
            AssertMsgFailed(("VBoxUSBGetPipeHandle failed for endpoint %x!\n", pIn->ep));
            status = STATUS_INVALID_PARAMETER;
            goto SendUrbFailure;
        }
    }

    /*
     * Allocate pMdl for the user mode data buffer
     */
    pMdlBuf = IoAllocateMdl(pIn->buf, (ULONG)pIn->len, FALSE, FALSE, NULL);
    if (!pMdlBuf)
    {
        AssertMsgFailed(("IoAllocateMdl failed for buffer %p length %d\n", pIn->buf, pIn->len));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendUrbFailure;
    }
    __try
    {
        MmProbeAndLockPages(pMdlBuf, KernelMode, IoModifyAccess);
    }
    __except(EXCEPTION_EXECUTE_HANDLER)
    {
        status = GetExceptionCode();
        IoFreeMdl(pMdlBuf);
        pMdlBuf = NULL;
        dprintf(("MmProbeAndLockPages: Exception Code %x\n", status));
        goto SendUrbFailure;
    }

    /* For some reason, passing a MDL in the URB does not work reliably. Notably
     * the iPhone when used with iTunes fails.
     */
    PVOID pBuffer = MmGetSystemAddressForMdlSafe(pMdlBuf, NormalPagePriority);
    if (!pBuffer)
    {
        AssertMsgFailed(("MmGetSystemAddressForMdlSafe failed for buffer!!\n"));
        status = STATUS_INSUFFICIENT_RESOURCES;
        goto SendUrbFailure;
    }

    /* Setup URB */
    switch(pIn->type)
    {
    case USBSUP_TRANSFER_TYPE_CTRL:
    case USBSUP_TRANSFER_TYPE_MSG:
        urb->UrbHeader.Function = URB_FUNCTION_CONTROL_TRANSFER;
        urb->UrbHeader.Length = sizeof(struct _URB_CONTROL_TRANSFER);
        urb->UrbControlTransfer.PipeHandle           = PipeHandle;
        urb->UrbControlTransfer.TransferBufferLength = (ULONG)pIn->len;
        urb->UrbControlTransfer.TransferFlags        = ((pIn->dir == USBSUP_DIRECTION_IN) ? USBD_TRANSFER_DIRECTION_IN : USBD_TRANSFER_DIRECTION_OUT);
        urb->UrbControlTransfer.UrbLink              = 0;

        if (PipeHandle == 0)
            urb->UrbControlTransfer.TransferFlags |= USBD_DEFAULT_PIPE_TRANSFER;

        if (pIn->type == USBSUP_TRANSFER_TYPE_MSG)
        {
           /* QUSB_TRANSFER_TYPE_MSG is a control transfer, but it is special
            * the first 8 bytes of the buffer is the setup packet so the real
            * data length is therefore urb->len - 8
            */
            PUSBSETUP pSetup = (PUSBSETUP)urb->UrbControlTransfer.SetupPacket;
            memcpy(urb->UrbControlTransfer.SetupPacket, pBuffer, min(sizeof(urb->UrbControlTransfer.SetupPacket), pIn->len));

            if (urb->UrbControlTransfer.TransferBufferLength <= sizeof(urb->UrbControlTransfer.SetupPacket))
                urb->UrbControlTransfer.TransferBufferLength = 0;
            else
                urb->UrbControlTransfer.TransferBufferLength -= sizeof(urb->UrbControlTransfer.SetupPacket);

            urb->UrbControlTransfer.TransferBuffer        = (uint8_t *)pBuffer + sizeof(urb->UrbControlTransfer.SetupPacket);
            urb->UrbControlTransfer.TransferBufferMDL     = 0;
            urb->UrbControlTransfer.TransferFlags        |= USBD_SHORT_TRANSFER_OK;
        }
        else
        {
            urb->UrbControlTransfer.TransferBuffer        = 0;
            urb->UrbControlTransfer.TransferBufferMDL     = pMdlBuf;
        }
        break;

    case USBSUP_TRANSFER_TYPE_ISOC:
        ULONG                   frameNum, startFrame;
        VBOXUSB_PIPE_INFO       *pipeInfo;

        Assert(pIn->dir == USBSUP_DIRECTION_IN || pIn->type == USBSUP_TRANSFER_TYPE_BULK);
        Assert(PipeHandle);
        pipeInfo = VBoxUSBGetPipeState(pDevExt, pIn->ep | ((pIn->dir == USBSUP_DIRECTION_IN) ? 0x80 : 0x00));
        if (pipeInfo == NULL)
        {
            /* Can happen if the isoc request comes in too early or late. */
            AssertMsgFailed(("pipeInfo not found!!\n"));
            status = STATUS_INVALID_PARAMETER;
            goto SendUrbFailure;
        }

        urb->UrbHeader.Function = URB_FUNCTION_ISOCH_TRANSFER;
        urb->UrbHeader.Length = (USHORT)urbSize;
        urb->UrbIsochronousTransfer.PipeHandle           = PipeHandle;
        urb->UrbIsochronousTransfer.TransferBufferLength = (ULONG)pIn->len;
        urb->UrbIsochronousTransfer.TransferBufferMDL    = 0;
        urb->UrbIsochronousTransfer.TransferBuffer       = pBuffer;
        urb->UrbIsochronousTransfer.TransferFlags        = ((pIn->dir == USBSUP_DIRECTION_IN) ? USBD_TRANSFER_DIRECTION_IN : USBD_TRANSFER_DIRECTION_OUT);
        urb->UrbIsochronousTransfer.TransferFlags       |= USBD_SHORT_TRANSFER_OK;  // May be implied already
        urb->UrbIsochronousTransfer.NumberOfPackets      = pOut->numIsoPkts;
        urb->UrbIsochronousTransfer.ErrorCount           = 0;
        urb->UrbIsochronousTransfer.UrbLink              = 0;

        Assert(pOut->numIsoPkts == urb->UrbIsochronousTransfer.NumberOfPackets);
        for (unsigned i = 0; i < pOut->numIsoPkts; ++i)
        {
            urb->UrbIsochronousTransfer.IsoPacket[i].Offset = pOut->aIsoPkts[i].off;
            urb->UrbIsochronousTransfer.IsoPacket[i].Length = pOut->aIsoPkts[i].cb;
        }

        /* We have to schedule the URBs ourselves. There is an ASAP flag but
         * that can only be reliably used after pipe creation/reset, ie. it's
         * almost completely useless.
         */
        frameNum = VBoxUSB_GetCurrentFrame(pDevExt, pIrp) + 2;  // Some fudge factor is required
        startFrame = pipeInfo->NextScheduledFrame;
        if ((frameNum < startFrame) || (startFrame > frameNum + 512))
            frameNum = startFrame;
        pipeInfo->NextScheduledFrame = frameNum + pOut->numIsoPkts;
        urb->UrbIsochronousTransfer.StartFrame = frameNum;
        dprintf(("URB scheduled to start at frame %d (%d packets)\n", frameNum, pOut->numIsoPkts));
        break;

    case USBSUP_TRANSFER_TYPE_BULK:
    case USBSUP_TRANSFER_TYPE_INTR:
        Assert(pIn->dir != USBSUP_DIRECTION_SETUP);
        // Interrupt transfers must have USBD_TRANSFER_DIRECTION_IN according to the DDK
        Assert(pIn->dir == USBSUP_DIRECTION_IN || pIn->type == USBSUP_TRANSFER_TYPE_BULK);
        Assert(PipeHandle);

        urb->UrbHeader.Function = URB_FUNCTION_BULK_OR_INTERRUPT_TRANSFER;
        urb->UrbHeader.Length = sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER);
        urb->UrbBulkOrInterruptTransfer.PipeHandle           = PipeHandle;
        urb->UrbBulkOrInterruptTransfer.TransferBufferLength = (ULONG)pIn->len;
        urb->UrbBulkOrInterruptTransfer.TransferBufferMDL    = 0;
        urb->UrbBulkOrInterruptTransfer.TransferBuffer       = pBuffer;
        urb->UrbBulkOrInterruptTransfer.TransferFlags        = ((pIn->dir == USBSUP_DIRECTION_IN) ? USBD_TRANSFER_DIRECTION_IN : USBD_TRANSFER_DIRECTION_OUT);

        if (urb->UrbBulkOrInterruptTransfer.TransferFlags & USBD_TRANSFER_DIRECTION_IN)
            urb->UrbBulkOrInterruptTransfer.TransferFlags   |= (USBD_SHORT_TRANSFER_OK);

        urb->UrbBulkOrInterruptTransfer.UrbLink              = 0;
        break;

    default:
        AssertFailed();
        status = STATUS_INVALID_PARAMETER;
        goto SendUrbFailure;
    }

    pContext->DeviceExtension = pDevExt;
    pContext->pMdlBuf         = pMdlBuf;
    pContext->pUrb            = urb;
    pContext->pOut            = pOut;
    pContext->ulTransferType  = pIn->type;
    pContext->ulMagic         = VBOXUSB_MAGIC;

    // Reuse the original IRP
    stackloc                                           = IoGetNextIrpStackLocation(pIrp);
    stackloc->MajorFunction                            = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    stackloc->Parameters.DeviceIoControl.IoControlCode = IOCTL_INTERNAL_USB_SUBMIT_URB;
    stackloc->Parameters.Others.Argument1              = urb;
    stackloc->Parameters.Others.Argument2              = NULL;

    IoSetCompletionRoutine(pIrp, VBoxUSBIOCTLAsyncCompletion, pContext, TRUE, TRUE, TRUE);
    IoMarkIrpPending(pIrp);

    dprintf(("VBoxUSBSendURB::"));
    VBoxUSB_IoIncrement(pDevExt);

    dprintf(("Send URB %p Pipe=%x Buffer=%p Length=%d\n", urb, PipeHandle, pIn->buf, pIn->len));
    status = IoCallDriver(pDevExt->TopOfStackDeviceObject, pIrp);
    if (!NT_SUCCESS(status))
    {
        dprintf(("IoCallDriver failed with status %X\n", status));
    }

    dprintf(("IoCallDriver returned %x\n", status));
    /* We've marked the IRP as pending, so we must return STATUS_PENDING regardless of what IoCallDriver returns. */
    return STATUS_PENDING;

SendUrbFailure:
    if (pMdlBuf)
    {
        MmUnlockPages(pMdlBuf);
        IoFreeMdl(pMdlBuf);
    }
    if (pContext)
        ExFreePool(pContext);

    return status;
}


NTSTATUS VBoxUSBSendIOCTL(PDEVICE_EXTENSION pDevExt, ULONG control_code, void *buffer)
{
    IO_STATUS_BLOCK io_status;
    KEVENT          event;
    NTSTATUS        status;
    IRP            *pIrp;
    PIO_STACK_LOCATION stackloc;

    KeInitializeEvent(&event, NotificationEvent, FALSE);

    pIrp = IoBuildDeviceIoControlRequest(control_code, pDevExt->TopOfStackDeviceObject, NULL, 0, NULL, 0, TRUE, &event, &io_status);
    if (!pIrp)
    {
        AssertMsgFailed(("IoBuildDeviceIoControlRequest failed!!\n"));
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    stackloc = IoGetNextIrpStackLocation(pIrp);
    stackloc->Parameters.Others.Argument1 = buffer;
    stackloc->Parameters.Others.Argument2 = NULL;

    dprintf(("VBoxUSBSendIOCTL::"));
    VBoxUSB_IoIncrement(pDevExt);

    status = IoCallDriver(pDevExt->TopOfStackDeviceObject, pIrp);
    if (status == STATUS_PENDING)
    {
        dprintf(("IoCallDriver returned STATUS_PENDING!!\n"));
        KeWaitForSingleObject(&event, Executive, KernelMode, FALSE, NULL);
        status = io_status.Status;
    }
    dprintf(("VBoxUSBSendIOCTL::"));
    VBoxUSB_IoDecrement(pDevExt);

    dprintf(("IoCallDriver returned %x\n", status));

    return status;
}

#if 0 /* dead code */
NTSTATUS VBoxUSBSyncCompletionRoutine(PDEVICE_OBJECT DeviceObject, PIRP Irp, PVOID Context)
{
    PKEVENT kevent;

    kevent = (PKEVENT)Context;
    KeSetEvent(kevent, IO_NO_INCREMENT, FALSE);
    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS VBoxUSBSyncSendRequest(PDEVICE_EXTENSION deviceExtension, ULONG control_code, void *buffer)
{
    KEVENT              localevent;
    PIRP                irp;
    PIO_STACK_LOCATION  nextStack;
    NTSTATUS            ntStatus;

//    VBoxUsb_DbgPrint(2, ("enter: VBoxUsb_SyncSendUsbRequest\n"));

    // Initialize the event we'll wait on
    KeInitializeEvent(&localevent, SynchronizationEvent, FALSE);

    // Allocate the Irp
    irp = IoAllocateIrp(deviceExtension->TopOfStackDeviceObject->StackSize, FALSE);

    if (irp == NULL)
    {
        return STATUS_INSUFFICIENT_RESOURCES;
    }

    // Set the Irp parameters
    nextStack = IoGetNextIrpStackLocation(irp);

    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    nextStack->Parameters.DeviceIoControl.IoControlCode = control_code;
    nextStack->Parameters.Others.Argument1 = buffer;

    // Set the completion routine, which will signal the event
    IoSetCompletionRoutine(irp,
                           VBoxUSBSyncCompletionRoutine,
                           &localevent,
                           TRUE,    // InvokeOnSuccess
                           TRUE,    // InvokeOnError
                           TRUE);   // InvokeOnCancel



    // Pass the Irp & Urb down the stack
    ntStatus = IoCallDriver(deviceExtension->TopOfStackDeviceObject, irp);

    // If the request is pending, block until it completes
    if (ntStatus == STATUS_PENDING)
    {
        LARGE_INTEGER timeout;

        // Specify a timeout of 5 seconds to wait for this call to complete.
        timeout.QuadPart = -10000 * 5000;
        ntStatus = KeWaitForSingleObject(&localevent, Executive, KernelMode, FALSE, &timeout);

        if (ntStatus == STATUS_TIMEOUT)
        {
            ntStatus = STATUS_IO_TIMEOUT;

            // Cancel the Irp we just sent.
            IoCancelIrp(irp);

            // And wait until the cancel completes
            KeWaitForSingleObject(&localevent, Executive, KernelMode, FALSE, NULL);
        }
        else
        {
            ntStatus = irp->IoStatus.Status;
        }
    }

    // Done with the Irp, now free it.
    IoFreeIrp(irp);

//    VBoxUsb_DbgPrint(2, ("exit:  VBoxUsb_SyncSendUsbRequest %08X\n", ntStatus));
    return ntStatus;
}
#endif
