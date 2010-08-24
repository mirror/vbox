/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    vboxdev.h

Abstract:

Environment:

    Kernel mode

Notes:

    Copyright (c) 2000 Microsoft Corporation.
    All Rights Reserved.

--*/

#ifndef _VBoxUSB_DEV_H
#define _VBoxUSB_DEV_H

#define VBOXUSB_MAGIC  0xABCF1423

typedef struct {
    PURB              pUrb;
    PMDL              pMdlBuf;
    PDEVICE_EXTENSION DeviceExtension;
    PVOID             pOut;
    ULONG             ulTransferType;
    ULONG             ulMagic;
} VBOXUSB_URB_CONTEXT, * PVBOXUSB_URB_CONTEXT;

RT_C_DECLS_BEGIN

NTSTATUS
VBoxUSB_DispatchCreate(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
VBoxUSB_DispatchClose(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
VBoxUSB_DispatchDevCtrl(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
VBoxUSB_ResetPipe(
    IN PDEVICE_OBJECT         DeviceObject,
    IN USBD_PIPE_HANDLE       PipeHandle
    );

NTSTATUS
VBoxUSB_ResetDevice(
    IN PDEVICE_OBJECT DeviceObject
    );

NTSTATUS
VBoxUSB_GetPortStatus(
    IN PDEVICE_OBJECT DeviceObject,
    IN PULONG PortStatus
    );

NTSTATUS
VBoxUSB_ResetParentPort(
    IN IN PDEVICE_OBJECT DeviceObject
    );

VOID
VBoxUSB_FreeMemory(
    IN PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
SubmitIdleRequestIrp(
    IN PDEVICE_EXTENSION DeviceExtension
    );

VOID
IdleNotificationCallback(
    IN PDEVICE_EXTENSION DeviceExtension
    );

NTSTATUS
IdleNotificationRequestComplete(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp,
    IN PDEVICE_EXTENSION DeviceExtension
    );

VOID
CancelSelectSuspend(
    IN PDEVICE_EXTENSION DeviceExtension
    );

VOID
PoIrpCompletionFunc(
    IN PDEVICE_OBJECT   DeviceObject,
    IN UCHAR            MinorFunction,
    IN POWER_STATE      PowerState,
    IN PVOID            Context,
    IN PIO_STATUS_BLOCK IoStatus
    );

VOID
PoIrpAsyncCompletionFunc(
    IN PDEVICE_OBJECT DeviceObject,
    IN UCHAR MinorFunction,
    IN POWER_STATE PowerState,
    IN PVOID Context,
    IN PIO_STATUS_BLOCK IoStatus
    );

VOID
WWIrpCompletionFunc(
    IN PDEVICE_OBJECT   DeviceObject,
    IN UCHAR            MinorFunction,
    IN POWER_STATE      PowerState,
    IN PVOID            Context,
    IN PIO_STATUS_BLOCK IoStatus
    );

NTSTATUS VBoxUSBCheckRootHub(IN IN PDEVICE_OBJECT DeviceObject);

RT_C_DECLS_END

#ifndef  STATUS_CONTINUE_COMPLETION
#define STATUS_CONTINUE_COMPLETION      STATUS_SUCCESS
#endif

#endif
