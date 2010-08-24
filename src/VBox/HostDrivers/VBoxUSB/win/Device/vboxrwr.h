/*++

Copyright (c) 2000  Microsoft Corporation

Module Name:

    vboxrwr.h

Abstract:

Environment:

    Kernel mode

Notes:

    Copyright (c) 2000 Microsoft Corporation.
    All Rights Reserved.

--*/
#ifndef _VBoxUSB_RWR_H
#define _VBoxUSB_RWR_H

typedef struct _VBOXUSB_RW_CONTEXT {

    PURB              Urb;
    PMDL              Mdl;
    ULONG             Length;         // remaining to xfer
    ULONG             Numxfer;        // cumulate xfer
    ULONG_PTR         VirtualAddress; // va for next segment of xfer.
    PDEVICE_EXTENSION DeviceExtension;

} VBOXUSB_RW_CONTEXT, * PVBOXUSB_RW_CONTEXT;

RT_C_DECLS_BEGIN

PVBOXUSB_PIPE_CONTEXT
VBoxUSB_PipeWithName(
    IN PDEVICE_OBJECT  DeviceObject,
    IN PUNICODE_STRING FileName
    );

NTSTATUS
VBoxUSB_DispatchReadWrite(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp
    );

NTSTATUS
VBoxUSB_ReadWriteCompletion(
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP           Irp,
    IN PVOID          Context
    );

RT_C_DECLS_END

#endif
