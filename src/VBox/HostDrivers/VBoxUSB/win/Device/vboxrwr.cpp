/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    vboxrwr.cpp

Abstract:

    This file has routines to perform reads and writes.
    The read and writes are for bulk transfers.

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
#include <VBox/log.h>
#include <iprt/assert.h>


PVBOXUSB_PIPE_CONTEXT
VBoxUSB_PipeWithName(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PUNICODE_STRING FileName
    )
/*++

Routine Description:

    This routine will pass the string pipe name and
    fetch the pipe number.

Arguments:

    DeviceObject - pointer to DeviceObject
    FileName - string pipe name

Return Value:

    The device extension maintains a pipe context for
    the pipes on 82930 board.
    This routine returns the pointer to this context in
    the device extension for the "FileName" pipe.

--*/
{
    LONG                  ix;
    ULONG                 uval;
    ULONG                 nameLength;
    ULONG                 umultiplier;
    PDEVICE_EXTENSION     deviceExtension;
    PVBOXUSB_PIPE_CONTEXT pipeContext;

    //
    // initialize variables
    //
    pipeContext = NULL;
    //
    // typedef WCHAR *PWSTR;
    //
    nameLength = (FileName->Length / sizeof(WCHAR));
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    dprintf(("VBoxUSB_PipeWithName - begins\n"));

    if(nameLength != 0) {

        dprintf(("Filename = %ws nameLength = %d\n", FileName->Buffer, nameLength));

        //
        // Parse the pipe#
        //
        ix = nameLength - 1;

        // if last char isn't digit, decrement it.
        while((ix > -1) &&
              ((FileName->Buffer[ix] < (WCHAR) '0')  ||
               (FileName->Buffer[ix] > (WCHAR) '9')))             {

            ix--;
        }

        if(ix > -1) {

            uval = 0;
            umultiplier = 1;

            // traversing least to most significant digits.

            while((ix > -1) &&
                  (FileName->Buffer[ix] >= (WCHAR) '0') &&
                  (FileName->Buffer[ix] <= (WCHAR) '9'))          {

                uval += (umultiplier *
                         (ULONG) (FileName->Buffer[ix] - (WCHAR) '0'));

                ix--;
                umultiplier *= 10;
            }

            if(uval < 6 && deviceExtension->PipeContext) {

                pipeContext = &deviceExtension->PipeContext[uval];
            }
        }
    }

    dprintf(("VBoxUSB_PipeWithName - ends\n"));

    return pipeContext;
}

NTSTATUS
VBoxUSB_DispatchReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    )
/*++

Routine Description:

    Dispatch routine for read and write.
    This routine creates a VBOXUSB_RW_CONTEXT for a read/write.
    This read/write is performed in stages of VBoxUSB_MAX_TRANSFER_SIZE.
    once a stage of transfer is complete, then the irp is circulated again,
    until the requested length of transfer is performed.

Arguments:

    DeviceObject - pointer to device object
    Irp - I/O request packet

Return Value:

    NT status value

--*/
{
    PMDL                   mdl;
    PURB                   urb;
    ULONG                  totalLength;
    ULONG                  stageLength;
    ULONG                  urbFlags;
    BOOLEAN                read;
    NTSTATUS               ntStatus;
    ULONG_PTR              virtualAddress;
    PFILE_OBJECT           fileObject;
    PDEVICE_EXTENSION      deviceExtension;
    PIO_STACK_LOCATION     irpStack;
    PIO_STACK_LOCATION     nextStack;
    PVBOXUSB_RW_CONTEXT    rwContext;
    PUSBD_PIPE_INFORMATION pipeInformation;

    //
    // initialize variables
    //
    urb = NULL;
    mdl = NULL;
    rwContext = NULL;
    totalLength = 0;
    irpStack = IoGetCurrentIrpStackLocation(Irp);
    fileObject = irpStack->FileObject;
    read = (irpStack->MajorFunction == IRP_MJ_READ) ? TRUE : FALSE;
    deviceExtension = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    dprintf(("VBoxUSB_DispatchReadWrite - begins\n"));

    if(deviceExtension->DeviceState != Working) {

        dprintf(("Invalid device state\n"));

        ntStatus = STATUS_INVALID_DEVICE_STATE;
        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    //
    // It is true that the client driver cancelled the selective suspend
    // request in the dispatch routine for create Irps.
    // But there is no guarantee that it has indeed completed.
    // so wait on the NoIdleReqPendEvent and proceed only if this event
    // is signalled.
    //
    dprintf(("Waiting on the IdleReqPendEvent\n"));

    //
    // make sure that the selective suspend request has been completed.
    //

    if(deviceExtension->SSEnable) {

        KeWaitForSingleObject(&deviceExtension->NoIdleReqPendEvent,
                              Executive,
                              KernelMode,
                              FALSE,
                              NULL);
    }

    if(fileObject && fileObject->FsContext) {

        pipeInformation = (PUSBD_PIPE_INFORMATION)fileObject->FsContext;

        if((UsbdPipeTypeBulk != pipeInformation->PipeType) &&
           (UsbdPipeTypeInterrupt != pipeInformation->PipeType)) {

            dprintf(("Usbd pipe type is not bulk or interrupt\n"));

            ntStatus = STATUS_INVALID_HANDLE;
            goto VBoxUSB_DispatchReadWrite_Exit;
        }
    }
    else {

        dprintf(("Invalid handle\n"));

        ntStatus = STATUS_INVALID_HANDLE;
        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    rwContext = (PVBOXUSB_RW_CONTEXT)
                ExAllocatePool(NonPagedPool,
                               sizeof(VBOXUSB_RW_CONTEXT));

    if(rwContext == NULL) {

        dprintf(("Failed to alloc mem for rwContext\n"));

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;
        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    if(Irp->MdlAddress) {

        totalLength = MmGetMdlByteCount(Irp->MdlAddress);
    }

    if(totalLength > VBOXUSB_TEST_BOARD_TRANSFER_BUFFER_SIZE) {

        dprintf(("Transfer length > circular buffer\n"));

        ntStatus = STATUS_INVALID_PARAMETER;

        ExFreePool(rwContext);

        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    if(totalLength == 0) {

        dprintf(("Transfer data length = 0\n"));

        ntStatus = STATUS_SUCCESS;

        ExFreePool(rwContext);

        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    urbFlags = USBD_SHORT_TRANSFER_OK;
    virtualAddress = (ULONG_PTR) MmGetMdlVirtualAddress(Irp->MdlAddress);

    if(read) {

        urbFlags |= USBD_TRANSFER_DIRECTION_IN;
        dprintf(("Read operation\n"));
    }
    else {

        urbFlags |= USBD_TRANSFER_DIRECTION_OUT;
        dprintf(("Write operation\n"));
    }

    //
    // the transfer request is for totalLength.
    // we can perform a max of VBoxUSB_MAX_TRANSFER_SIZE
    // in each stage.
    //
    if(totalLength > VBOXUSB_MAX_TRANSFER_SIZE) {

        stageLength = VBOXUSB_MAX_TRANSFER_SIZE;
    }
    else {

        stageLength = totalLength;
    }

    mdl = IoAllocateMdl((PVOID) virtualAddress,
                        totalLength,
                        FALSE,
                        FALSE,
                        NULL);

    if(mdl == NULL) {

        dprintf(("Failed to alloc mem for mdl\n"));

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;

        ExFreePool(rwContext);

        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    //
    // map the portion of user-buffer described by an mdl to another mdl
    //
    IoBuildPartialMdl(Irp->MdlAddress,
                      mdl,
                      (PVOID) virtualAddress,
                      stageLength);

    urb = (PURB)ExAllocatePool(NonPagedPool,sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER));

    if(urb == NULL) {

        dprintf(("Failed to alloc mem for urb\n"));

        ntStatus = STATUS_INSUFFICIENT_RESOURCES;

        ExFreePool(rwContext);
        IoFreeMdl(mdl);

        goto VBoxUSB_DispatchReadWrite_Exit;
    }

    UsbBuildInterruptOrBulkTransferRequest(
                            urb,
                            sizeof(struct _URB_BULK_OR_INTERRUPT_TRANSFER),
                            pipeInformation->PipeHandle,
                            NULL,
                            mdl,
                            stageLength,
                            urbFlags,
                            NULL);

    //
    // set VBOXUSB_RW_CONTEXT parameters.
    //

    rwContext->Urb             = urb;
    rwContext->Mdl             = mdl;
    rwContext->Length          = totalLength - stageLength;
    rwContext->Numxfer         = 0;
    rwContext->VirtualAddress  = virtualAddress + stageLength;
    rwContext->DeviceExtension = deviceExtension;

    //
    // use the original read/write irp as an internal device control irp
    //

    nextStack = IoGetNextIrpStackLocation(Irp);
    nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
    nextStack->Parameters.Others.Argument1 = (PVOID) urb;
    nextStack->Parameters.DeviceIoControl.IoControlCode =
                                             IOCTL_INTERNAL_USB_SUBMIT_URB;

    IoSetCompletionRoutine(Irp,
                           (PIO_COMPLETION_ROUTINE)VBoxUSB_ReadWriteCompletion,
                           rwContext,
                           TRUE,
                           TRUE,
                           TRUE);

    //
    // since we return STATUS_PENDING call IoMarkIrpPending.
    // This is the boiler plate code.
    // This may cause extra overhead of an APC for the Irp completion
    // but this is the correct thing to do.
    //

    IoMarkIrpPending(Irp);

    dprintf(("VBoxUSB_DispatchReadWrite::"));
    VBoxUSB_IoIncrement(deviceExtension);

    ntStatus = IoCallDriver(deviceExtension->TopOfStackDeviceObject,
                            Irp);

    if(!NT_SUCCESS(ntStatus)) {

        dprintf(("IoCallDriver fails with status %X\n", ntStatus));

        //
        // if the device was yanked out, then the pipeInformation
        // field is invalid.
        // similarly if the request was cancelled, then we need not
        // invoked reset pipe/device.
        //
        if((ntStatus != STATUS_CANCELLED) &&
           (ntStatus != STATUS_DEVICE_NOT_CONNECTED)) {

            ntStatus = VBoxUSB_ResetPipe(DeviceObject,
                                     pipeInformation->PipeHandle);

            if(!NT_SUCCESS(ntStatus)) {

                dprintf(("VBoxUSB_ResetPipe failed\n"));

                ntStatus = VBoxUSB_ResetDevice(DeviceObject);
            }
        }
        else {

            dprintf(("ntStatus is STATUS_CANCELLED or "
                                 "STATUS_DEVICE_NOT_CONNECTED\n"));
        }
    }

    //
    // we return STATUS_PENDING and not the status returned by the lower layer.
    //
    return STATUS_PENDING;

VBoxUSB_DispatchReadWrite_Exit:

    Irp->IoStatus.Status = ntStatus;
    Irp->IoStatus.Information = 0;

    IoCompleteRequest(Irp, IO_NO_INCREMENT);

    dprintf(("VBoxUSB_DispatchReadWrite - ends\n"));

    return ntStatus;
}

NTSTATUS
VBoxUSB_ReadWriteCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    )
/*++

Routine Description:

    This is the completion routine for reads/writes
    If the irp completes with success, we check if we
    need to recirculate this irp for another stage of
    transfer. In this case return STATUS_MORE_PROCESSING_REQUIRED.
    if the irp completes in error, free all memory allocs and
    return the status.

Arguments:

    DeviceObject - pointer to device object
    Irp - I/O request packet
    Context - context passed to the completion routine.

Return Value:

    NT status value

--*/
{
    ULONG               stageLength;
    NTSTATUS            ntStatus;
    PIO_STACK_LOCATION  nextStack;
    PVBOXUSB_RW_CONTEXT rwContext;

    //
    // initialize variables
    //
    rwContext = (PVBOXUSB_RW_CONTEXT) Context;
    ntStatus = Irp->IoStatus.Status;

    UNREFERENCED_PARAMETER(DeviceObject);
    dprintf(("VBoxUSB_ReadWriteCompletion - begins\n"));

    //
    // successfully performed a stageLength of transfer.
    // check if we need to recirculate the irp.
    //
    if(NT_SUCCESS(ntStatus)) {

        if(rwContext) {

            rwContext->Numxfer +=
              rwContext->Urb->UrbBulkOrInterruptTransfer.TransferBufferLength;

            if(rwContext->Length) {

                //
                // another stage transfer
                //
                dprintf(("Another stage transfer...\n"));

                if(rwContext->Length > VBOXUSB_MAX_TRANSFER_SIZE) {

                    stageLength = VBOXUSB_MAX_TRANSFER_SIZE;
                }
                else {

                    stageLength = rwContext->Length;
                }

                // the source MDL is not mapped and so when the lower driver
                // calls MmGetSystemAddressForMdl(Safe) on Urb->Mdl (target Mdl),
                // system PTEs are used.
                // IoFreeMdl calls MmPrepareMdlForReuse to release PTEs (unlock
                // VA address before freeing any Mdl
                // Rather than calling IoFreeMdl and IoAllocateMdl each time,
                // just call MmPrepareMdlForReuse
                // Not calling MmPrepareMdlForReuse will leak system PTEs
                //
                MmPrepareMdlForReuse(rwContext->Mdl);

                IoBuildPartialMdl(Irp->MdlAddress,
                                  rwContext->Mdl,
                                  (PVOID) rwContext->VirtualAddress,
                                  stageLength);

                //
                // reinitialize the urb
                //
                rwContext->Urb->UrbBulkOrInterruptTransfer.TransferBufferLength
                                                                  = stageLength;
                rwContext->VirtualAddress += stageLength;
                rwContext->Length -= stageLength;

                nextStack = IoGetNextIrpStackLocation(Irp);
                nextStack->MajorFunction = IRP_MJ_INTERNAL_DEVICE_CONTROL;
                nextStack->Parameters.Others.Argument1 = rwContext->Urb;
                nextStack->Parameters.DeviceIoControl.IoControlCode =
                                            IOCTL_INTERNAL_USB_SUBMIT_URB;

                IoSetCompletionRoutine(Irp,
                                       VBoxUSB_ReadWriteCompletion,
                                       rwContext,
                                       TRUE,
                                       TRUE,
                                       TRUE);

                IoCallDriver(rwContext->DeviceExtension->TopOfStackDeviceObject,
                             Irp);

                return STATUS_MORE_PROCESSING_REQUIRED;
            }
            else {

                //
                // this is the last transfer
                //

                Irp->IoStatus.Information = rwContext->Numxfer;
            }
        }
    }
    else {

        dprintf(("ReadWriteCompletion - failed with status = %X\n", ntStatus));
    }

    if(rwContext) {

        //
        // dump rwContext
        //
        dprintf(("rwContext->Urb             = %X\n",
                             rwContext->Urb));
        dprintf(("rwContext->Mdl             = %X\n",
                             rwContext->Mdl));
        dprintf(("rwContext->Length          = %d\n",
                             rwContext->Length));
        dprintf(("rwContext->Numxfer         = %d\n",
                             rwContext->Numxfer));
        dprintf(("rwContext->VirtualAddress  = %X\n",
                             rwContext->VirtualAddress));
        dprintf(("rwContext->DeviceExtension = %X\n",
                             rwContext->DeviceExtension));

        dprintf(("VBoxUSB_ReadWriteCompletion::"));
        VBoxUSB_IoDecrement(rwContext->DeviceExtension);

        ExFreePool(rwContext->Urb);
        IoFreeMdl(rwContext->Mdl);
        ExFreePool(rwContext);
    }

    dprintf(("VBoxUSB_ReadWriteCompletion - ends\n"));

    return ntStatus;
}

