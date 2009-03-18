/** @file
 *
 * VBoxGuest -- VirtualBox Win32 guest mouse filter driver
 *
 * Based on a Microsoft DDK sample
 *
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
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

// #define LOG_ENABLED

#include "VBoxMouse.h"

// VBOX start
#include <VBox/err.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>
// VBOX end

__BEGIN_DECLS
NTSTATUS DriverEntry (PDRIVER_OBJECT, PUNICODE_STRING);
__END_DECLS

#ifdef ALLOC_PRAGMA
#pragma alloc_text (INIT, DriverEntry)
#pragma alloc_text (PAGE, VBoxMouse_AddDevice)
#pragma alloc_text (PAGE, VBoxMouse_CreateClose)
#pragma alloc_text (PAGE, VBoxMouse_IoCtl)
#pragma alloc_text (PAGE, VBoxMouse_InternIoCtl)
#pragma alloc_text (PAGE, VBoxMouse_PnP)
#pragma alloc_text (PAGE, VBoxMouse_Power)
#pragma alloc_text (PAGE, VBoxMouse_Unload)
#endif

NTSTATUS
DriverEntry (
    IN  PDRIVER_OBJECT  DriverObject,
    IN  PUNICODE_STRING RegistryPath
    )
/*++
Routine Description:

    Initialize the entry points of the driver.

--*/
{
    ULONG i;

    UNREFERENCED_PARAMETER (RegistryPath);

    dprintf(("VBoxMouse::DriverEntry\n"));

    //
    // Fill in all the dispatch entry points with the pass through function
    // and the explicitly fill in the functions we are going to intercept
    //
    for (i = 0; i <= IRP_MJ_MAXIMUM_FUNCTION; i++)
    {
        DriverObject->MajorFunction[i] = VBoxMouse_DispatchPassThrough;
    }

//    ExInitializeFastMutex(&vboxControlMutex);
    DriverObject->MajorFunction [IRP_MJ_CREATE] =
    DriverObject->MajorFunction [IRP_MJ_CLOSE] =          VBoxMouse_CreateClose;
//    DriverObject->MajorFunction [IRP_MJ_DEVICE_CONTROL] = VBoxDispatchIo;
    DriverObject->MajorFunction [IRP_MJ_PNP] =            VBoxMouse_PnP;
    DriverObject->MajorFunction [IRP_MJ_POWER] =          VBoxMouse_Power;
    DriverObject->MajorFunction [IRP_MJ_CLEANUP] =
    DriverObject->MajorFunction [IRP_MJ_INTERNAL_DEVICE_CONTROL] =
                                                          VBoxMouse_InternIoCtl;
    DriverObject->DriverUnload = VBoxMouse_Unload;
    DriverObject->DriverExtension->AddDevice = VBoxMouse_AddDevice;

    dprintf(("leaving DriverEntry with success\n"));
    return STATUS_SUCCESS;
}

#ifdef LOG_ENABLED
void dumpDevExt (PDEVICE_EXTENSION devExt)
{
    dprintf ((
             "devExt %p\n"
             "  %p %p %p\n"
             "  %d %p %p %p %p\n"
             "  %p\n"
             "  %p %p\n"
             "  %d %d %d %d\n"
             "  %p %d\n",
             devExt,
             devExt->Self, devExt->PDO, devExt->TopOfStack,
             devExt->EnableCount, devExt->UpperContext, devExt->UpperIsrHook, devExt->IsrWritePort, devExt->CallContext,
             devExt->QueueMousePacket,
             devExt->UpperConnectData.ClassDeviceObject, devExt->UpperConnectData.ClassService,
             devExt->DeviceState, devExt->Started, devExt->SurpriseRemoved, devExt->Removed,
             devExt->reqSC, devExt->HostMouse
            ));
}
#else
#define dumpDevExt(a) do {} while (0)
#endif /* LOG_ENABLED */

// VBOX start
typedef struct _VBoxGlobalContext
{
    volatile LONG cDevicesStarted;
    volatile LONG fVBGLInited;
    volatile LONG fVBGLInitFailed;
    volatile LONG fHostInformed;
    volatile LONG fHostMouseFound;
} VBoxGlobalContext;

static VBoxGlobalContext g_ctx = { 0, FALSE, FALSE, FALSE, FALSE };

static BOOLEAN vboxIsVBGLInited (void)
{
   return InterlockedCompareExchange (&g_ctx.fVBGLInited, TRUE, TRUE) == TRUE;
}

static BOOLEAN vboxIsVBGLInitFailed (void)
{
   return InterlockedCompareExchange (&g_ctx.fVBGLInitFailed, TRUE, TRUE) == TRUE;
}

static BOOLEAN vboxIsHostInformed (void)
{
   return InterlockedCompareExchange (&g_ctx.fHostInformed, TRUE, TRUE) == TRUE;
}

static BOOLEAN vboxIsHostMouseFound (void)
{
   return InterlockedCompareExchange (&g_ctx.fHostMouseFound, TRUE, TRUE) == TRUE;
}

static void vboxDeviceAdded (PDEVICE_EXTENSION devExt)
{
    LONG c = InterlockedIncrement (&g_ctx.cDevicesStarted);

    if (c == 1)
    {
        if (   !vboxIsVBGLInited ()
            && !vboxIsVBGLInitFailed ())
        {
            /*  Do one time initialization. */
            int vboxRC = VbglInit ();

            if (RT_SUCCESS (vboxRC))
            {
                /* Atomically set the flag. */
                InterlockedExchange (&g_ctx.fVBGLInited, TRUE);
                dprintf(("VBoxMouse::vboxDeviceStarted: guest library initialization OK\n"));
            }
            else
            {
                InterlockedExchange (&g_ctx.fVBGLInitFailed, TRUE);
                dprintf(("VBoxMouse::vboxDeviceStarted: guest library initialization failed\n"));
            }
        }
    }
    if (!vboxIsHostMouseFound ())
    {
        NTSTATUS status;
        UCHAR Property[512];
        ULONG ResultLength;

        ResultLength = 0;
        status = IoGetDeviceProperty(devExt->PDO, DevicePropertyBootConfiguration,
                                     sizeof (Property),
                                     &Property,
                                     &ResultLength);

        dprintf(("VBoxMouse::vboxDeviceAdded: looking for host mouse: Len is %d, status %x\n",
                 ResultLength, status));

        if (status == STATUS_SUCCESS)
        {
            /* Check whether the device claims the IO port 0x60. */
            BOOLEAN bPort60h = FALSE;

            CM_RESOURCE_LIST *pResourceList = (CM_RESOURCE_LIST *)&Property[0];

            dprintf(("Configuration: descriptors %d\n",
                      pResourceList->Count));

            ULONG iDescriptor = 0;
            for (; iDescriptor < pResourceList->Count; iDescriptor++)
            {
                CM_FULL_RESOURCE_DESCRIPTOR *pFullDescriptor = &pResourceList->List[iDescriptor];

                dprintf(("Descriptor %d: InterfaceType %d, BusType %d, list ver %d, list rev %d, count %d\n",
                         iDescriptor,
                         pFullDescriptor->InterfaceType,
                         pFullDescriptor->BusNumber,
                         pFullDescriptor->PartialResourceList.Version,
                         pFullDescriptor->PartialResourceList.Revision,
                         pFullDescriptor->PartialResourceList.Count));

                ULONG iPartialDescriptor = 0;
                for (; iPartialDescriptor < pFullDescriptor->PartialResourceList.Count; iPartialDescriptor++)
                {
                    CM_PARTIAL_RESOURCE_DESCRIPTOR *pPartialDescriptor = &pFullDescriptor->PartialResourceList.PartialDescriptors[iPartialDescriptor];

                    dprintf(("  PartialDescriptor %d: type %d, ShareDisposition %d, Flags 0x%04X, Start 0x%llx, length 0x%x\n",
                             iPartialDescriptor,
                             pPartialDescriptor->Type, pPartialDescriptor->ShareDisposition, pPartialDescriptor->Flags,
                             pPartialDescriptor->u.Generic.Start.QuadPart, pPartialDescriptor->u.Generic.Length));

                    if (pPartialDescriptor->Type == CmResourceTypePort)
                    {
                        if (pPartialDescriptor->u.Port.Start.QuadPart == 0x60)
                        {
                            bPort60h = TRUE;
                        }
                    }
                }
            }

            if (bPort60h)
            {
                /* It's the emulated 8042 PS/2 mouse/kbd device, so mark it as the Host one.
                 * For this device the filter will query absolute mouse coords from the host.
                 */
                InterlockedExchange (&g_ctx.fHostMouseFound, TRUE);
                devExt->HostMouse = TRUE;
                dprintf(("VBoxMouse::vboxDeviceAdded: host mouse found.\n"));
            }
        }
    }
}

static void vboxDeviceRemoved (PDEVICE_EXTENSION devExt)
{
    dprintf(("VBoxMouse::vboxDeviceRemoved\n"));

    /* Save the allocated request pointer and clear the devExt. */
    VMMDevReqMouseStatus *reqSC = (VMMDevReqMouseStatus *)InterlockedExchangePointer ((PVOID volatile *)&devExt->reqSC, NULL);

    if (devExt->HostMouse && vboxIsHostInformed ())
    {
        // tell the VMM that from now on we can't handle absolute coordinates anymore
        VMMDevReqMouseStatus *req = NULL;

        int vboxRC = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMouseStatus), VMMDevReq_SetMouseStatus);

        if (RT_SUCCESS(vboxRC))
        {
            req->mouseFeatures = 0;
            req->pointerXPos = 0;
            req->pointerYPos = 0;

            vboxRC = VbglGRPerform (&req->header);

            if (RT_FAILURE(vboxRC) || RT_FAILURE(req->header.rc))
            {
                dprintf(("VBoxMouse::vboxDeviceRemoved: ERROR communicating new mouse capabilities to VMMDev.\n"
                         "rc = %d, VMMDev rc = %Rrc\n", vboxRC, req->header.rc));
            }

            VbglGRFree (&req->header);
        }
        else
        {
            dprintf(("VBoxMouse::vboxDeviceRemoved: the request allocation has failed.\n"));
        }
    
        InterlockedExchange (&g_ctx.fHostInformed, FALSE);
    }

    if (reqSC)
    {
        VbglGRFree (&reqSC->header);
    }

    LONG c = InterlockedDecrement (&g_ctx.cDevicesStarted);

    if (c == 0)
    {
        if (vboxIsVBGLInited ())
        {
            /* Set the flag to prevent reinitializing of the VBGL. */
            InterlockedExchange (&g_ctx.fVBGLInitFailed, TRUE);

            VbglTerminate ();

            /* The VBGL is now in the not initialized state. */
            InterlockedExchange (&g_ctx.fVBGLInited, FALSE);
            InterlockedExchange (&g_ctx.fVBGLInitFailed, FALSE);
        }
    }
}

static void vboxInformHost (PDEVICE_EXTENSION devExt)
{
    dprintf (("VBoxMouse::vboxInformHost: %p\n", devExt));

    if (vboxIsVBGLInited ())
    {
        if (devExt->HostMouse && !vboxIsHostInformed ())
        {
            VMMDevReqMouseStatus *req = NULL;

            int vboxRC = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMouseStatus), VMMDevReq_SetMouseStatus);

            if (RT_SUCCESS(vboxRC))
            {
                /* Inform host that we support absolute */
                req->mouseFeatures = VBOXGUEST_MOUSE_GUEST_CAN_ABSOLUTE;
                req->pointerXPos = 0;
                req->pointerYPos = 0;

                vboxRC = VbglGRPerform (&req->header);

                if (RT_FAILURE(vboxRC) || RT_FAILURE(req->header.rc))
                {
                    dprintf(("VBoxMouse::vboxInformHost: ERROR communicating new mouse capabilities to VMMDev."
                              "rc = %d, VMMDev rc = %Rrc\n", vboxRC, req->header.rc));
                }
                else
                {
                    InterlockedExchange (&g_ctx.fHostInformed, TRUE);
                }

                VbglGRFree(&req->header);
            }
        }

        if (devExt->HostMouse && !devExt->reqSC)
        {
            /* Preallocate request for ServiceCallback */
            VMMDevReqMouseStatus *req = NULL;

            int vboxRC = VbglGRAlloc ((VMMDevRequestHeader **)&req, sizeof (VMMDevReqMouseStatus), VMMDevReq_GetMouseStatus);

            if (RT_SUCCESS(vboxRC))
            {
                InterlockedExchangePointer ((PVOID volatile *)&devExt->reqSC, req);
                dumpDevExt (devExt);
            }
            else
            {
                dprintf(("VBoxMouse::vboxInformHost: request allocation for service callback failed\n"));
            }
        }
    }
}
// VBOX end

NTSTATUS
VBoxMouse_AddDevice(
    IN PDRIVER_OBJECT   Driver,
    IN PDEVICE_OBJECT   PDO
    )
{
    PDEVICE_EXTENSION        devExt;
    IO_ERROR_LOG_PACKET      errorLogEntry;
    PDEVICE_OBJECT           device;
    NTSTATUS                 status = STATUS_SUCCESS;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(errorLogEntry);

    dprintf(("VBoxMouse::AddDevice Driver %p, PDO %p\n", Driver, PDO));

#ifdef LOG_ENABLED
    WCHAR wszProperty[512];
    ULONG ResultLength = 0;
    wszProperty[0] = 0;
    status = IoGetDeviceProperty(PDO, DevicePropertyDeviceDescription,
                                 sizeof (wszProperty),
                                 &wszProperty,
                                 &ResultLength);
    if (status == STATUS_SUCCESS)
    {
        dprintf(("VBoxMouse::AddDevice %ls Len is %d\n", wszProperty, ResultLength));
    }
#endif /* LOG_ENABLED */

    status = IoCreateDevice(Driver,
                            sizeof(DEVICE_EXTENSION),
                            NULL,
                            FILE_DEVICE_MOUSE,
                            0,
                            FALSE,
                            &device
                            );

    if (!NT_SUCCESS(status)) {
        return (status);
    }

    RtlZeroMemory(device->DeviceExtension, sizeof(DEVICE_EXTENSION));

    devExt = (PDEVICE_EXTENSION) device->DeviceExtension;
    devExt->TopOfStack = IoAttachDeviceToDeviceStack(device, PDO);
    if (devExt->TopOfStack == NULL) {
        IoDeleteDevice(device);
        return STATUS_DEVICE_NOT_CONNECTED;
    }

    ASSERT(devExt->TopOfStack);

    devExt->Self =          device;
    devExt->PDO =           PDO;
    devExt->DeviceState =   PowerDeviceD0;

    devExt->SurpriseRemoved = FALSE;
    devExt->Removed =         FALSE;
    devExt->Started =         FALSE;

// VBOX start
    vboxDeviceAdded (devExt);
// VBOX end

    device->Flags |= (DO_BUFFERED_IO | DO_POWER_PAGABLE);
    device->Flags &= ~DO_DEVICE_INITIALIZING;

    dprintf(("DevExt = %p, returning from AddDevice with rc = 0x%x\n", devExt, status));
    return status;
}

NTSTATUS
VBoxMouse_StartComplete(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    )
/*++
Routine Description:

    Generic completion routine that allows the driver to send the irp down the
    stack, catch it on the way up, and do more processing at the original IRQL.

--*/
{
    PKEVENT             event;
    PDEVICE_EXTENSION   pDevExt;

    event = (PKEVENT) Context;

    pDevExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    dprintf(("VBoxMouse_StartComplete %p\n", pDevExt));

    //
    // We could switch on the major and minor functions of the IRP to perform
    // different functions, but we know that Context is an event that needs
    // to be set.
    //
    KeSetEvent(event, 0, FALSE);

    return STATUS_MORE_PROCESSING_REQUIRED;
}

NTSTATUS
VBoxMouse_CreateClose(
    IN  PDEVICE_OBJECT  DeviceObject,
    IN  PIRP            Irp
    )
/*++
Routine Description:

    Maintain a simple count of the creates and closes sent against this device

--*/
{
    PIO_STACK_LOCATION  irpStack;
    NTSTATUS            status;
    PDEVICE_EXTENSION   devExt;

    PAGED_CODE();

    irpStack = IoGetCurrentIrpStackLocation(Irp);
    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

    dprintf(("VBoxMouse::CreateClose %p\n", devExt));

    status = Irp->IoStatus.Status;

    switch (irpStack->MajorFunction) {
    case IRP_MJ_CREATE:
        dprintf(("IRP_MJ_CREATE\n"));
        if (NULL == devExt->UpperConnectData.ClassService) {
            //
            // No Connection yet.  How can we be enabled?
            //
            dprintf(("VBoxMouse: Not connected, returning STATUS_INVALID_DEVICE_STATE\n"));
            status = STATUS_INVALID_DEVICE_STATE;
        }
        else if ( 1 >= InterlockedIncrement(&devExt->EnableCount)) {
            //
            // First time enable here
            //
        }
        else {
            //
            // More than one create was sent down
            //
        }

        break;

    case IRP_MJ_CLOSE:

        dprintf(("IRP_MJ_CLOSE\n"));
        ASSERT(0 < devExt->EnableCount);

        if (0 >= InterlockedDecrement(&devExt->EnableCount)) {
            //
            // successfully closed the device, do any appropriate work here
            //
        }

        break;
    }

    Irp->IoStatus.Status = status;

    //
    // Pass on the create and the close
    //
    return VBoxMouse_DispatchPassThrough(DeviceObject, Irp);
}

NTSTATUS
VBoxMouse_DispatchPassThrough(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        )
/*++
Routine Description:

    Passes a request on to the lower driver.

Considerations:

    If you are creating another device object (to communicate with user mode
    via IOCTLs), then this function must act differently based on the intended
    device object.  If the IRP is being sent to the solitary device object, then
    this function should just complete the IRP (becuase there is no more stack
    locations below it).  If the IRP is being sent to the PnP built stack, then
    the IRP should be passed down the stack.

    These changes must also be propagated to all the other IRP_MJ dispatch
    functions (such as create, close, cleanup, etc.) as well!

--*/
{
    PIO_STACK_LOCATION irpStack = IoGetCurrentIrpStackLocation(Irp);

    dprintf(("VBoxMouse_DispatchPassThrough\n"));

    //
    // Pass the IRP to the target
    //
    IoSkipCurrentIrpStackLocation(Irp);

    return IoCallDriver(((PDEVICE_EXTENSION) DeviceObject->DeviceExtension)->TopOfStack, Irp);
}

NTSTATUS
VBoxMouse_InternIoCtl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for internal device control requests.
    There are two specific control codes that are of interest:

    IOCTL_INTERNAL_MOUSE_CONNECT:
        Store the old context and function pointer and replace it with our own.
        This makes life much simpler than intercepting IRPs sent by the RIT and
        modifying them on the way back up.

    IOCTL_INTERNAL_I8042_HOOK_MOUSE:
        Add in the necessary function pointers and context values so that we can
        alter how the ps/2 mouse is initialized.

    NOTE:  Handling IOCTL_INTERNAL_I8042_HOOK_MOUSE is *NOT* necessary if
           all you want to do is filter MOUSE_INPUT_DATAs.  You can remove
           the handling code and all related device extension fields and
           functions to conserve space.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION          irpStack;
    PDEVICE_EXTENSION           devExt;
    KEVENT                      event;
    PCONNECT_DATA               connectData;
    PINTERNAL_I8042_HOOK_MOUSE  hookMouse;
    NTSTATUS                    status = STATUS_SUCCESS;

    UNREFERENCED_PARAMETER(event);

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    Irp->IoStatus.Information = 0;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    dprintf(("VBoxMouse_InternIoCtl %p\n", devExt));

    dprintf(("VBoxMouse_InternIoCtl: %08X, fn = %d(%04X)\n",
              irpStack->Parameters.DeviceIoControl.IoControlCode,
              (irpStack->Parameters.DeviceIoControl.IoControlCode >> 2) & 0xFFF,
              (irpStack->Parameters.DeviceIoControl.IoControlCode >> 2) & 0xFFF
              ));

    switch (irpStack->Parameters.DeviceIoControl.IoControlCode) {

    //
    // Connect a mouse class device driver to the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_CONNECT:
        //
        // Only allow one connection.
        //
        if (devExt->UpperConnectData.ClassService != NULL) {
            status = STATUS_SHARING_VIOLATION;
            break;
        }
        else if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                sizeof(CONNECT_DATA)) {
            //
            // invalid buffer
            //
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Copy the connection parameters to the device extension.
        //
        connectData = ((PCONNECT_DATA)
            (irpStack->Parameters.DeviceIoControl.Type3InputBuffer));

        devExt->UpperConnectData = *connectData;

        //
        // Hook into the report chain.  Everytime a mouse packet is reported to
        // the system, VBoxMouse_ServiceCallback will be called
        //
        connectData->ClassDeviceObject = devExt->Self;
        connectData->ClassService = VBoxMouse_ServiceCallback;

        break;

    //
    // Disconnect a mouse class device driver from the port driver.
    //
    case IOCTL_INTERNAL_MOUSE_DISCONNECT:

        //
        // Clear the connection parameters in the device extension.
        //
        // devExt->UpperConnectData.ClassDeviceObject = NULL;
        // devExt->UpperConnectData.ClassService = NULL;

        status = STATUS_NOT_IMPLEMENTED;
        break;

    //
    // Attach this driver to the initialization and byte processing of the
    // i8042 (ie PS/2) mouse.  This is only necessary if you want to do PS/2
    // specific functions, otherwise hooking the CONNECT_DATA is sufficient
    //
    case IOCTL_INTERNAL_I8042_HOOK_MOUSE:

        if (irpStack->Parameters.DeviceIoControl.InputBufferLength <
                 sizeof(INTERNAL_I8042_HOOK_MOUSE)) {
            //
            // invalid buffer
            //
            status = STATUS_INVALID_PARAMETER;
            break;
        }

        //
        // Copy the connection parameters to the device extension.
        //
        hookMouse = (PINTERNAL_I8042_HOOK_MOUSE)
            (irpStack->Parameters.DeviceIoControl.Type3InputBuffer);

        //
        // Set isr routine and context and record any values from above this driver
        //
        devExt->UpperContext = hookMouse->Context;
        hookMouse->Context = (PVOID) DeviceObject;

        if (hookMouse->IsrRoutine) {
            devExt->UpperIsrHook = hookMouse->IsrRoutine;
        }
        hookMouse->IsrRoutine = (PI8042_MOUSE_ISR) VBoxMouse_IsrHook;

        //
        // Store all of the other functions we might need in the future
        //
        devExt->IsrWritePort = hookMouse->IsrWritePort;
        devExt->CallContext = hookMouse->CallContext;
        devExt->QueueMousePacket = hookMouse->QueueMousePacket;

        break;

    //
    // These internal ioctls are not supported by the new PnP model.
    //
#if 0       // obsolete
    case IOCTL_INTERNAL_MOUSE_ENABLE:
    case IOCTL_INTERNAL_MOUSE_DISABLE:
        status = STATUS_NOT_SUPPORTED;
        break;
#endif  // obsolete

    //
    // Might want to capture this in the future.  For now, then pass it down
    // the stack.  These queries must be successful for the RIT to communicate
    // with the mouse.
    //
    case IOCTL_MOUSE_QUERY_ATTRIBUTES:
    default:
        break;
    }

// VBOX start
    dprintf(("VBoxMouse_InternIoCtl: calling VBoxInformHost\n"));
    vboxInformHost (devExt);
// VBOX end

    if (!NT_SUCCESS(status)) {
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        return status;
    }

    return VBoxMouse_DispatchPassThrough(DeviceObject, Irp);
}

NTSTATUS
VBoxMouse_PnP(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for plug and play irps

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION           devExt;
    PIO_STACK_LOCATION          irpStack;
    NTSTATUS                    status = STATUS_SUCCESS;
    KIRQL                       oldIrql;
    KEVENT                      event;

    PAGED_CODE();

    UNREFERENCED_PARAMETER(oldIrql);

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    dprintf(("VBoxMouse_PnP %p\n", devExt));

    switch (irpStack->MinorFunction) {
    case IRP_MN_START_DEVICE: {

        dprintf(("IRP_MN_START_DEVICE\n"));

        //
        // The device is starting.
        //
        // We cannot touch the device (send it any non pnp irps) until a
        // start device has been passed down to the lower drivers.
        //
        IoCopyCurrentIrpStackLocationToNext(Irp);
        KeInitializeEvent(&event,
                          NotificationEvent,
                          FALSE
                          );

        IoSetCompletionRoutine(Irp,
                               (PIO_COMPLETION_ROUTINE) VBoxMouse_StartComplete,
                               &event,
                               TRUE,
                               TRUE,
                               TRUE); // No need for Cancel

        status = IoCallDriver(devExt->TopOfStack, Irp);

        if (STATUS_PENDING == status) {
            KeWaitForSingleObject(
               &event,
               Executive, // Waiting for reason of a driver
               KernelMode, // Waiting in kernel mode
               FALSE, // No allert
               NULL); // No timeout
        }

        dprintf(("status: %x, irp status: %x\n", Irp->IoStatus.Status));

        if (NT_SUCCESS(status) && NT_SUCCESS(Irp->IoStatus.Status)) {
            devExt->Started = TRUE;
            devExt->Removed = FALSE;
            devExt->SurpriseRemoved = FALSE;
        }

        //
        // We must now complete the IRP, since we stopped it in the
        // completetion routine with MORE_PROCESSING_REQUIRED.
        //
        Irp->IoStatus.Status = status;
        Irp->IoStatus.Information = 0;
        IoCompleteRequest(Irp, IO_NO_INCREMENT);

        break;
    }

    case IRP_MN_SURPRISE_REMOVAL:

        dprintf(("IRP_MN_SURPRISE_REMOVAL\n"));

        //
        // Same as a remove device, but don't call IoDetach or IoDeleteDevice
        //
        devExt->SurpriseRemoved = TRUE;

        // Remove code here

        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
        break;

    case IRP_MN_REMOVE_DEVICE:
    {
        dprintf(("IRP_MN_REMOVE_DEVICE\n"));
// VBOX start
        vboxDeviceRemoved (devExt);
// VBOX end

        devExt->Removed = TRUE;

        // remove code here
        Irp->IoStatus.Status = STATUS_SUCCESS;

        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);

        IoDetachDevice(devExt->TopOfStack);
        IoDeleteDevice(DeviceObject);

    }   break;

    case IRP_MN_QUERY_REMOVE_DEVICE:
    case IRP_MN_QUERY_STOP_DEVICE:
    case IRP_MN_CANCEL_REMOVE_DEVICE:
    case IRP_MN_CANCEL_STOP_DEVICE:
    case IRP_MN_FILTER_RESOURCE_REQUIREMENTS:
    case IRP_MN_STOP_DEVICE:
    case IRP_MN_QUERY_DEVICE_RELATIONS:
    case IRP_MN_QUERY_INTERFACE:
    case IRP_MN_QUERY_CAPABILITIES:
    case IRP_MN_QUERY_DEVICE_TEXT:
    case IRP_MN_QUERY_RESOURCES:
    case IRP_MN_QUERY_RESOURCE_REQUIREMENTS:
    case IRP_MN_READ_CONFIG:
    case IRP_MN_WRITE_CONFIG:
    case IRP_MN_EJECT:
    case IRP_MN_SET_LOCK:
    case IRP_MN_QUERY_ID:
    case IRP_MN_QUERY_PNP_DEVICE_STATE:
    default:
        //
        // Here the filter driver might modify the behavior of these IRPS
        // Please see PlugPlay documentation for use of these IRPs.
        //
        IoSkipCurrentIrpStackLocation(Irp);
        status = IoCallDriver(devExt->TopOfStack, Irp);
        break;
    }

    return status;
}

NTSTATUS
VBoxMouse_Power(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    )
/*++

Routine Description:

    This routine is the dispatch routine for power irps   Does nothing except
    record the state of the device.

Arguments:

    DeviceObject - Pointer to the device object.

    Irp - Pointer to the request packet.

Return Value:

    Status is returned.

--*/
{
    PIO_STACK_LOCATION  irpStack;
    PDEVICE_EXTENSION   devExt;
    POWER_STATE         powerState;
    POWER_STATE_TYPE    powerType;

    PAGED_CODE();

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;
    irpStack = IoGetCurrentIrpStackLocation(Irp);

    dprintf(("VBoxMouse_Power %p\n", devExt));

    powerType = irpStack->Parameters.Power.Type;
    powerState = irpStack->Parameters.Power.State;

    switch (irpStack->MinorFunction) {
    case IRP_MN_SET_POWER:
        if (powerType  == DevicePowerState) {
            devExt->DeviceState = powerState.DeviceState;
        }

    case IRP_MN_QUERY_POWER:
    case IRP_MN_WAIT_WAKE:
    case IRP_MN_POWER_SEQUENCE:
    default:
        break;
    }

    PoStartNextPowerIrp(Irp);
    IoSkipCurrentIrpStackLocation(Irp);
    return PoCallDriver(devExt->TopOfStack, Irp);
}

BOOLEAN
VBoxMouse_IsrHook (
    PDEVICE_OBJECT          DeviceObject,
    PMOUSE_INPUT_DATA       CurrentInput,
    POUTPUT_PACKET          CurrentOutput,
    UCHAR                   StatusByte,
    PUCHAR                  DataByte,
    PBOOLEAN                ContinueProcessing,
    PMOUSE_STATE            MouseState,
    PMOUSE_RESET_SUBSTATE   ResetSubState
)
/*++

Remarks:
    i8042prt specific code, if you are writing a packet only filter driver, you
    can remove this function

Arguments:

    DeviceObject - Our context passed during IOCTL_INTERNAL_I8042_HOOK_MOUSE

    CurrentInput - Current input packet being formulated by processing all the
                    interrupts

    CurrentOutput - Current list of bytes being written to the mouse or the
                    i8042 port.

    StatusByte    - Byte read from I/O port 60 when the interrupt occurred

    DataByte      - Byte read from I/O port 64 when the interrupt occurred.
                    This value can be modified and i8042prt will use this value
                    if ContinueProcessing is TRUE

    ContinueProcessing - If TRUE, i8042prt will proceed with normal processing of
                         the interrupt.  If FALSE, i8042prt will return from the
                         interrupt after this function returns.  Also, if FALSE,
                         it is this functions responsibilityt to report the input
                         packet via the function provided in the hook IOCTL or via
                         queueing a DPC within this driver and calling the
                         service callback function acquired from the connect IOCTL

Return Value:

    Status is returned.

  --+*/
{
    PDEVICE_EXTENSION   devExt;
    BOOLEAN             retVal = TRUE;

    devExt = (PDEVICE_EXTENSION)DeviceObject->DeviceExtension;

    //dprintf(("VBoxMouse_IsrHook\n"));

    if (devExt->UpperIsrHook) {
        retVal = (*devExt->UpperIsrHook) (
            devExt->UpperContext,
            CurrentInput,
            CurrentOutput,
            StatusByte,
            DataByte,
            ContinueProcessing,
            MouseState,
            ResetSubState
            );

        if (!retVal || !(*ContinueProcessing)) {
            return retVal;
        }
    }

    *ContinueProcessing = TRUE;
    return retVal;
}

VOID
VBoxMouse_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    )
/*++

Routine Description:

    Called when there are mouse packets to report to the RIT.  You can do
    anything you like to the packets.  For instance:

    o Drop a packet altogether
    o Mutate the contents of a packet
    o Insert packets into the stream

Arguments:

    DeviceObject - Context passed during the connect IOCTL

    InputDataStart - First packet to be reported

    InputDataEnd - One past the last packet to be reported.  Total number of
                   packets is equal to InputDataEnd - InputDataStart

    InputDataConsumed - Set to the total number of packets consumed by the RIT
                        (via the function pointer we replaced in the connect
                        IOCTL)

Return Value:

    Status is returned.

--*/
{
    PDEVICE_EXTENSION   devExt;

    devExt = (PDEVICE_EXTENSION) DeviceObject->DeviceExtension;

// VBOX start
    // dprintf(("VBoxMouse_ServiceCallback: devExt = %p, reqSC = %p\n", devExt, devExt->reqSC));

    VMMDevReqMouseStatus *req = devExt->reqSC;

    if (req)
    {
        int rc = VbglGRPerform (&req->header);

        if (RT_SUCCESS(rc) && RT_SUCCESS(req->header.rc))
        {
            if (req->mouseFeatures & VBOXGUEST_MOUSE_HOST_CAN_ABSOLUTE)
            {
                PMOUSE_INPUT_DATA inputData = InputDataStart;
                while (inputData < InputDataEnd)
                {
                    // modify the event data
                    inputData->LastX = req->pointerXPos;
                    inputData->LastY = req->pointerYPos;
                    inputData->Flags = MOUSE_MOVE_ABSOLUTE;
                    inputData++;
                }
            }
        }
        else
        {
            dprintf(("VBoxMouse::VBoxMouse_ServiceCallback: ERROR querying mouse capabilities from VMMDev."
                     "rc = %Rrc, VMMDev rc = %Rrc\n", rc, req->header.rc));
        }
    }
// VBOX end

    //
    // UpperConnectData must be called at DISPATCH
    //
    (*(PSERVICE_CALLBACK_ROUTINE) devExt->UpperConnectData.ClassService)(
        devExt->UpperConnectData.ClassDeviceObject,
        InputDataStart,
        InputDataEnd,
        InputDataConsumed
        );
}

VOID
VBoxMouse_Unload(
   IN PDRIVER_OBJECT Driver
   )
/*++

Routine Description:

   Free all the allocated resources associated with this driver.

Arguments:

   DriverObject - Pointer to the driver object.

Return Value:

   None.

--*/

{
    PAGED_CODE();

    dprintf(("VBoxMouse_Unload\n"));

    UNREFERENCED_PARAMETER(Driver);

    ASSERT(NULL == Driver->DeviceObject);
}
