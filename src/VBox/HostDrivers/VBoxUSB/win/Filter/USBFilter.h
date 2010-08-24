/*++

Copyright (c) Microsoft Corporation.  All rights reserved.

    THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY
    KIND, EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
    IMPLIED WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR
    PURPOSE.

Module Name:

    filter.h

Abstract:

    Contains structure definitions and function prototypes for filter driver.

Environment:

    Kernel mode

Revision History:

    Eliyas Yakub Oct 29 1998

--*/
#include <ntddk.h>
#include <wdmsec.h> // for IoCreateDeviceSecure
#include <initguid.h>

#define IOCTL_INTERFACE     1

#if !defined(_FILTER_H_)
#define _FILTER_H_

#define DRIVERNAME "VBoxUSBFlt.sys: "


#ifdef DEBUG
#define DebugPrint(_x_) DbgPrint _x_

#define TRAP() DbgBreakPoint()

#else
#define DebugPrint(_x_)
#define TRAP()
#endif


#ifndef  STATUS_CONTINUE_COMPLETION //required to build driver in Win2K and XP build environment
//
// This value should be returned from completion routines to continue
// completing the IRP upwards. Otherwise, STATUS_MORE_PROCESSING_REQUIRED
// should be returned.
//
#define STATUS_CONTINUE_COMPLETION      STATUS_SUCCESS

#endif

#define POOL_TAG   'liFT'

//
// These are the states Filter transition to upon
// receiving a specific PnP Irp. Refer to the PnP Device States
// diagram in DDK documentation for better understanding.
//

typedef enum _DEVICE_PNP_STATE {

    NotStarted = 0,         // Not started yet
    Started,                // Device has received the START_DEVICE IRP
    StopPending,            // Device has received the QUERY_STOP IRP
    Stopped,                // Device has received the STOP_DEVICE IRP
    RemovePending,          // Device has received the QUERY_REMOVE IRP
    SurpriseRemovePending,  // Device has received the SURPRISE_REMOVE IRP
    Deleted                 // Device has received the REMOVE_DEVICE IRP

} DEVICE_PNP_STATE;

#define INITIALIZE_PNP_STATE(_Data_)    \
        (_Data_)->DevicePnPState =  NotStarted;\
        (_Data_)->PreviousPnPState = NotStarted;

#define SET_NEW_PNP_STATE(_Data_, _state_) \
        (_Data_)->PreviousPnPState =  (_Data_)->DevicePnPState;\
        (_Data_)->DevicePnPState = (_state_);

#define RESTORE_PREVIOUS_PNP_STATE(_Data_)   \
        (_Data_)->DevicePnPState =   (_Data_)->PreviousPnPState;\


typedef struct _DEVICE_EXTENSION
{
    //
    // A back pointer to the device object.
    //

    PDEVICE_OBJECT  Self;

    //
    // The top of the stack before this filter was added.
    //

    PDEVICE_OBJECT  NextLowerDriver;

    //
    // current PnP state of the device
    //

    DEVICE_PNP_STATE  DevicePnPState;

    //
    // Remembers the previous pnp state
    //

    DEVICE_PNP_STATE    PreviousPnPState;

    //
    // Removelock to track IRPs so that device can be removed and
    // the driver can be unloaded safely.
    //
    IO_REMOVE_LOCK RemoveLock;


    BOOLEAN        fHookDevice;
} DEVICE_EXTENSION, *PDEVICE_EXTENSION;


#ifdef __cplusplus
extern "C" {
#endif

#ifdef DEBUG
PCHAR
PnPMinorFunctionString (
    UCHAR MinorFunction
);
#endif

NTSTATUS
FilterAddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT PhysicalDeviceObject
    );


NTSTATUS
FilterDispatchPnp (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
FilterDispatchPower(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    );

VOID
FilterUnload(
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
FilterPass (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
DriverEntry(
    IN PDRIVER_OBJECT  DriverObject,
    IN PUNICODE_STRING RegistryPath
    );

NTSTATUS
FilterStartCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    );

NTSTATUS
FilterDeviceUsageNotificationCompletionRoutine(
    IN PDEVICE_OBJECT   DeviceObject,
    IN PIRP             Irp,
    IN PVOID            Context
    );

int ListAddDevice(PDEVICE_OBJECT pdo, PDEVICE_OBJECT fdo);
int ListKnownPDO(PDEVICE_OBJECT pdo);
int ListRemoveDevice(PDEVICE_OBJECT fdo);
int ListDeviceIsCaptured(PDEVICE_OBJECT fdo);
int ListStartDevice(PDEVICE_OBJECT fdo);
int ListCaptureDevice(PDEVICE_OBJECT fdo);

#ifdef IOCTL_INTERFACE

typedef struct _CONTROL_DEVICE_EXTENSION {

    ULONG   Deleted; // False if the deviceobject is valid, TRUE if it's deleted

    PVOID   ControlData; // Store your control data here

} CONTROL_DEVICE_EXTENSION, *PCONTROL_DEVICE_EXTENSION;

NTSTATUS
FilterCreateControlObject(
    IN PDEVICE_OBJECT    DeviceObject
);

VOID
FilterDeleteControlObject(
    );

NTSTATUS
FilterDispatchIo(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    );

#endif

NTSTATUS _stdcall VBoxUSBPnPCompletion(DEVICE_OBJECT *fido, IRP *Irp, void *context);
NTSTATUS _stdcall VBoxUSBDispatchIO(PDEVICE_OBJECT DeviceObject, PIRP Irp);
NTSTATUS _stdcall VBoxUSBCreate();
NTSTATUS _stdcall VBoxUSBClose();
NTSTATUS _stdcall VBoxUSBInit();


#ifdef __cplusplus
}
#endif

#endif

