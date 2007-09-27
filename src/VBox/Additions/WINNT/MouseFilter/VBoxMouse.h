/** @file
 *
 * VBoxGuest -- VirtualBox Win32 guest mouse filter driver
 *
 * Based on a Microsoft DDK sample
 *
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


#ifndef VBOXMOUSE_H
#define VBOXMOUSE_H

#include <iprt/assert.h>
__BEGIN_DECLS
#include "ntddk.h"
#include "kbdmou.h"
#include <ntddmou.h>
#include <ntdd8042.h>
__END_DECLS
#include <VBox/VBoxGuest.h>

#define VBOXMOUSE_POOL_TAG (ULONG) 'oMBV'
#undef ExAllocatePool
#define ExAllocatePool(type, size) \
            ExAllocatePoolWithTag (type, size, VBOXMOUSE_POOL_TAG)

//VBOX begin
/* debug printf */
# define OSDBGPRINT(a) DbgPrint a

/* dprintf */
#if (defined(DEBUG) && !defined(NO_LOGGING)) || defined(LOG_ENABLED)
# ifdef LOG_TO_BACKDOOR
#  include <VBox/log.h>
#  define dprintf(a) RTLogBackdoorPrintf a
# else
#  define dprintf(a) OSDBGPRINT(a)
# endif
#else
# define dprintf(a) do {} while (0)
#endif

/* dprintf2 - extended logging. */
#if 0
# define dprintf2 dprintf
#else
# define dprintf2(a) do { } while (0)
#endif

//VBOX end

#if DBG

#define TRAP()                      DbgBreakPoint()
#define DbgRaiseIrql(_x_,_y_)       KeRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_)           KeLowerIrql(_x_)

#else   // DBG

#define TRAP()
#define DbgRaiseIrql(_x_,_y_)
#define DbgLowerIrql(_x_)

#endif

typedef struct _DEVICE_EXTENSION
{
    //
    // A backpointer to the device object for which this is the extension
    //
    PDEVICE_OBJECT  Self;

    //
    // "THE PDO"  (ejected by the bus)
    //
    PDEVICE_OBJECT  PDO;

    //
    // The top of the stack before this filter was added.  AKA the location
    // to which all IRPS should be directed.
    //
    PDEVICE_OBJECT  TopOfStack;

    //
    // Number of creates sent down
    //
    LONG EnableCount;

    //
    // Previous hook routine and context
    //
    PVOID UpperContext;
    PI8042_MOUSE_ISR UpperIsrHook;

    //
    // Write to the mouse in the context of VBoxMouse_IsrHook
    //
    IN PI8042_ISR_WRITE_PORT IsrWritePort;

    //
    // Context for IsrWritePort, QueueMousePacket
    //
    IN PVOID CallContext;

    //
    // Queue the current packet (ie the one passed into VBoxMouse_IsrHook)
    // to be reported to the class driver
    //
    IN PI8042_QUEUE_PACKET QueueMousePacket;

    //
    // The real connect data that this driver reports to
    //
    CONNECT_DATA UpperConnectData;

    //
    // current power state of the device
    //
    DEVICE_POWER_STATE  DeviceState;

    //
    // State of the stack and this device object
    //
    BOOLEAN Started;
    BOOLEAN SurpriseRemoved;
    BOOLEAN Removed;

// VBOX begin
    /* Preallocated request for VBoxMouse_ServiceCallback */
    VMMDevReqMouseStatus *reqSC;

    BOOLEAN HostMouse;
// VBOX end

} DEVICE_EXTENSION, *PDEVICE_EXTENSION;

//
// Prototypes
//
__BEGIN_DECLS

NTSTATUS
VBoxMouse_AddDevice(
    IN PDRIVER_OBJECT DriverObject,
    IN PDEVICE_OBJECT BusDeviceObject
    );

NTSTATUS
VBoxMouse_CreateClose (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
VBoxMouse_DispatchPassThrough(
        IN PDEVICE_OBJECT DeviceObject,
        IN PIRP Irp
        );

NTSTATUS
VBoxMouse_InternIoCtl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
VBoxMouse_IoCtl (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
VBoxMouse_PnP (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

NTSTATUS
VBoxMouse_Power (
    IN PDEVICE_OBJECT DeviceObject,
    IN PIRP Irp
    );

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
);

VOID
VBoxMouse_ServiceCallback(
    IN PDEVICE_OBJECT DeviceObject,
    IN PMOUSE_INPUT_DATA InputDataStart,
    IN PMOUSE_INPUT_DATA InputDataEnd,
    IN OUT PULONG InputDataConsumed
    );

VOID
VBoxMouse_Unload (
    IN PDRIVER_OBJECT DriverObject
    );

NTSTATUS
VBoxDispatchIo(
    IN PDEVICE_OBJECT    DeviceObject,
    IN PIRP              Irp
    );

__END_DECLS

#endif  // VBOXMOUSE_H
