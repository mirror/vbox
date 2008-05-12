/** @file
 *
 * VBoxGuest -- VirtualBox Win32 guest support driver
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

#ifndef __VBOXGUESTINTERNAL_h__
#define __VBOXGUESTINTERNAL_h__


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/

#include <iprt/cdefs.h>

#ifdef IN_RING0
# if (_MSC_VER >= 1400) && !defined(VBOX_WITH_PATCHED_DDK)
#  include <iprt/asm.h>
#  define _InterlockedExchange           _InterlockedExchange_StupidDDKVsCompilerCrap
#  define _InterlockedExchangeAdd        _InterlockedExchangeAdd_StupidDDKVsCompilerCrap
#  define _InterlockedCompareExchange    _InterlockedCompareExchange_StupidDDKVsCompilerCrap
#  define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
__BEGIN_DECLS
#  include <ntddk.h>
__END_DECLS
#  undef  _InterlockedExchange
#  undef  _InterlockedExchangeAdd
#  undef  _InterlockedCompareExchange
#  undef  _InterlockedAddLargeStatistic
# else
__BEGIN_DECLS
#  include <ntddk.h>
__END_DECLS
# endif
#endif

#include <VBox/VBoxGuest.h>

/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/


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

// the maximum scatter/gather transfer length
#define MAXIMUM_TRANSFER_LENGTH     64*1024

#define PCI_MAX_BUSES 256

/*
 * Error codes.
 */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

// possible device states for our state machine
enum DEVSTATE
{
    STOPPED,
    WORKING,
    PENDINGSTOP,
    PENDINGREMOVE,
    SURPRISEREMOVED,
    REMOVED
};

// undocumented API to set the system time
extern "C"
{
NTSYSAPI NTSTATUS NTAPI ZwSetSystemTime(IN PLARGE_INTEGER NewTime, OUT PLARGE_INTEGER OldTime OPTIONAL);
}

#ifdef IN_RING0
typedef struct _BASE_ADDRESS {

    PHYSICAL_ADDRESS RangeStart;    // Original device physical address
    ULONG   RangeLength;            // Length of I/O or memory range
    BOOLEAN RangeInMemory;          // Flag: unmapped range is I/O or memory range

    PVOID   MappedRangeStart;       // Mapped I/O or memory range
    BOOLEAN MappedRangeInMemory;    // Flag: mapped range is I/O or memory range

    BOOLEAN ResourceMapped;         // Flag: resource is mapped (i.e. MmMapIoSpace called)

} BASE_ADDRESS, *PBASE_ADDRESS;


/**
 * Device extension.
 */
typedef struct VBOXGUESTDEVEXT
{
//// legacy stuff
    // bus number where the device is located
    ULONG busNumber;
    // slot number where the device is located
    ULONG slotNumber;
    // device interrupt level
    ULONG interruptLevel;
    // device interrupt vector
    ULONG interruptVector;
    // affinity mask
    KAFFINITY interruptAffinity;
    // LevelSensitive or Latched
    KINTERRUPT_MODE interruptMode;

    // PCI base address information
    ULONG addressCount;
    BASE_ADDRESS baseAddress[PCI_TYPE0_ADDRESSES];

    // adapter object pointer, returned by HalGetAdapter
    PADAPTER_OBJECT adapterObject;

    // interrupt object pointer
    PKINTERRUPT interruptObject;
/////


    // our functional driver object
    PDEVICE_OBJECT deviceObject;
    // the top of the stack
    PDEVICE_OBJECT nextLowerDriver;
    // currently active Irp
    IRP *currentIrp;
    PKTHREAD workerThread;
    PKTHREAD idleThread;
    KEVENT workerThreadRequest;
    BOOLEAN stopThread;
    // device state
    DEVSTATE devState;
    // start port address
    ULONG startPortAddress;
    // start of hypervisor mapping
    PVOID hypervisorMapping;
    // size in bytes of the hypervisor mapping
    ULONG hypervisorMappingSize;

    /* Physical address and length of VMMDev memory */
    PHYSICAL_ADDRESS memoryAddress;
    ULONG memoryLength;

    /* Virtual address of VMMDev memory */
    VMMDevMemory *pVMMDevMemory;

    /* Pending event flags signalled by host */
    ULONG u32Events;

    /* Notification semaphore */
    KEVENT keventNotification;

    LARGE_INTEGER HGCMWaitTimeout;

    /* Old Windows session id */
    ULONG   ulOldActiveConsoleId;

    /* VRDP hook state */
    BOOLEAN fVRDPEnabled;

    /* Preallocated VMMDevEvents for IRQ handler */
    VMMDevEvents *irqAckEvents;


    struct
    {
        uint32_t     cBalloons;
        uint32_t     cMaxBalloons;
        PMDL        *paMdlMemBalloon;
    } MemBalloon;

} VBOXGUESTDEVEXT, *PVBOXGUESTDEVEXT;

// Windows version identifier
typedef enum
{
    WINNT4   = 1,
    WIN2K    = 2,
    WINXP    = 3,
    WIN2K3   = 4,
    WINVISTA = 5
} winVersion_t;
extern winVersion_t winVersion;

extern "C"
{
VOID     VBoxGuestDpcHandler(PKDPC dpc, PDEVICE_OBJECT pDevObj,
                             PIRP irp, PVOID context);
BOOLEAN  VBoxGuestIsrHandler(PKINTERRUPT interrupt, PVOID serviceContext);
NTSTATUS createThreads(PVBOXGUESTDEVEXT pDevExt);
VOID     unreserveHypervisorMemory(PVBOXGUESTDEVEXT pDevExt);
void     VBoxInitMemBalloon(PVBOXGUESTDEVEXT pDevExt);
void     VBoxCleanupMemBalloon(PVBOXGUESTDEVEXT pDevExt);
}

#endif

#endif // __H_VBOXGUESTINTERNAL
