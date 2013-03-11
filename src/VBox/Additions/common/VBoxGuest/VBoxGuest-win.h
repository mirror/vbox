/* $Id$ */
/** @file
 * VBoxGuest - Windows specifics.
 */

/*
 * Copyright (C) 2010-2013 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxGuest_win_h
#define ___VBoxGuest_win_h


#include <iprt/cdefs.h>

RT_C_DECLS_BEGIN
#ifdef RT_ARCH_X86
# define _InterlockedAddLargeStatistic  _InterlockedAddLargeStatistic_StupidDDKVsCompilerCrap
#endif
#include <ntddk.h>
#ifdef RT_ARCH_X86
# undef _InterlockedAddLargeStatistic
#endif
RT_C_DECLS_END

#include <iprt/spinlock.h>
#include <iprt/memobj.h>

#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include "VBoxGuestInternal.h"



/** Possible device states for our state machine. */
enum DEVSTATE
{
    STOPPED,
    WORKING,
    PENDINGSTOP,
    PENDINGREMOVE,
    SURPRISEREMOVED,
    REMOVED
};

typedef struct VBOXGUESTWINBASEADDRESS
{
    /** Original device physical address. */
    PHYSICAL_ADDRESS RangeStart;
    /** Length of I/O or memory range. */
    ULONG   RangeLength;
    /** Flag: Unmapped range is I/O or memory range. */
    BOOLEAN RangeInMemory;
    /** Mapped I/O or memory range. */
    PVOID   MappedRangeStart;
    /** Flag: mapped range is I/O or memory range. */
    BOOLEAN MappedRangeInMemory;
    /** Flag: resource is mapped (i.e. MmMapIoSpace called). */
    BOOLEAN ResourceMapped;
} VBOXGUESTWINBASEADDRESS, *PVBOXGUESTWINBASEADDRESS;


/** Windows-specific device extension bits. */
typedef struct VBOXGUESTDEVEXTWIN
{
    VBOXGUESTDEVEXT Core;

    /** Our functional driver object. */
    PDEVICE_OBJECT pDeviceObject;
    /** Top of the stack. */
    PDEVICE_OBJECT pNextLowerDriver;
    /** Currently active Irp. */
    IRP *pCurrentIrp;
    /** Interrupt object pointer. */
    PKINTERRUPT pInterruptObject;

    /** Bus number where the device is located. */
    ULONG busNumber;
    /** Slot number where the device is located. */
    ULONG slotNumber;
    /** Device interrupt level. */
    ULONG interruptLevel;
    /** Device interrupt vector. */
    ULONG interruptVector;
    /** Affinity mask. */
    KAFFINITY interruptAffinity;
    /** LevelSensitive or Latched. */
    KINTERRUPT_MODE interruptMode;

    /** PCI base address information. */
    ULONG pciAddressCount;
    VBOXGUESTWINBASEADDRESS pciBaseAddress[PCI_TYPE0_ADDRESSES];

    /** Physical address and length of VMMDev memory. */
    PHYSICAL_ADDRESS vmmDevPhysMemoryAddress;
    ULONG vmmDevPhysMemoryLength;

    /** Device state. */
    DEVSTATE devState;
    DEVSTATE prevDevState;

    /** Last system power action set (see VBoxGuestPower). */
    POWER_ACTION LastSystemPowerAction;
    /** Preallocated generic request for shutdown. */
    VMMDevPowerStateRequest *pPowerStateRequest;
    /** Preallocated VMMDevEvents for IRQ handler. */
    VMMDevEvents *pIrqAckEvents;

    /** Pre-allocated kernel session data. This is needed
     * for handling kernel IOCtls. */
    struct VBOXGUESTSESSION *pKernelSession;

    /** Spinlock protecting MouseNotifyCallback. Required since the consumer is
     *  in a DPC callback and not the ISR. */
    KSPIN_LOCK MouseEventAccessLock;
} VBOXGUESTDEVEXTWIN, *PVBOXGUESTDEVEXTWIN;


/** NT (windows) version identifier. */
typedef enum VBGDNTVER
{
    VBGDNTVER_INVALID = 0,
    VBGDNTVER_WINNT4,
    VBGDNTVER_WIN2K,
    VBGDNTVER_WINXP,
    VBGDNTVER_WIN2K3,
    VBGDNTVER_WINVISTA,
    VBGDNTVER_WIN7,
    VBGDNTVER_WIN8
} VBGDNTVER;
extern VBGDNTVER g_enmVbgdNtVer;


#define VBOXGUEST_UPDATE_DEVSTATE(a_pDevExt, a_newDevState) \
    do { \
        (a_pDevExt)->prevDevState = (a_pDevExt)->devState; \
        (a_pDevExt)->devState     = (a_newDevState); \
    } while (0)

/** CM_RESOURCE_MEMORY_* flags which were used on XP or earlier. */
#define VBOX_CM_PRE_VISTA_MASK (0x3f)


RT_C_DECLS_BEGIN

#ifdef TARGET_NT4
NTSTATUS   vbgdNt4CreateDevice(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath);
#else
NTSTATUS   vbgdNtInit(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS   vbgdNtPnP(PDEVICE_OBJECT pDevObj, PIRP pIrp);
NTSTATUS   vbgdNtPower(PDEVICE_OBJECT pDevObj, PIRP pIrp);
#endif

/** @name Common routines used in both (PnP and legacy) driver versions.
 * @{
 */
#ifdef TARGET_NT4
NTSTATUS   vbgdNtInit(PDRIVER_OBJECT pDrvObj, PDEVICE_OBJECT pDevObj, PUNICODE_STRING pRegPath);
#else
NTSTATUS   vbgdNtInit(PDEVICE_OBJECT pDevObj, PIRP pIrp);
#endif
NTSTATUS   vbgdNtCleanup(PDEVICE_OBJECT pDevObj);
VOID       vbgdNtDpcHandler(PKDPC pDPC, PDEVICE_OBJECT pDevObj, PIRP pIrp, PVOID pContext);
BOOLEAN    vbgdNtIsrHandler(PKINTERRUPT interrupt, PVOID serviceContext);
NTSTATUS   vbgdNtScanPCIResourceList(PCM_RESOURCE_LIST pResList, PVBOXGUESTDEVEXTWIN pDevExt);
NTSTATUS   vbgdNtMapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt, PHYSICAL_ADDRESS PhysAddr, ULONG cbToMap,
                                 void **ppvMMIOBase, uint32_t *pcbMMIO);
void       vbgdNtUnmapVMMDevMemory(PVBOXGUESTDEVEXTWIN pDevExt);
VBOXOSTYPE vbgdNtVersionToOSType(VBGDNTVER enmNtVer);
/** @}  */

RT_C_DECLS_END

#ifdef TARGET_NT4
/*
 * XP DDK #defines ExFreePool to ExFreePoolWithTag. The latter does not exist
 * on NT4, so... The same for ExAllocatePool.
 */
# undef ExAllocatePool
# undef ExFreePool
#endif

#endif /* !___VBoxGuest_win_h */


