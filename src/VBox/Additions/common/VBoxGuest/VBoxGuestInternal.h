/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver.
 */

/*
 * Copyright (C) 2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VBoxGuestInternal_h
#define ___VBoxGuestInternal_h

#include <iprt/types.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>


/** Pointer to a wait-for-event entry. */
typedef struct VBOXGUESTWAIT *PVBOXGUESTWAIT;

/**
 * VBox guest wait for event entry.
 *
 * Each waiting thread allocates one of these items and adds
 * it to the wait list before going to sleep on the event sem.
 */
typedef struct VBOXGUESTWAIT
{
    /** The next entry in the list. */
    PVBOXGUESTWAIT volatile     pNext;
    /** The previous entry in the list. */
    PVBOXGUESTWAIT volatile     pPrev;
    /** The event semaphore. */
    RTSEMEVENTMULTI             Event;
    /** The events we are waiting on. */
    uint32_t                    fReqEvents;
    /** The events we received. */
    uint32_t volatile           fResEvents;
#ifdef VBOX_HGCM
    /** The HGCM request we're waiting for to complete. */
    VMMDevHGCMRequestHeader volatile *pHGCMReq;
#endif
} VBOXGUESTWAIT;

/**
 * VBox guest wait for event list.
 */
typedef struct VBOXGUESTWAITLIST
{
    /** The head. */
    PVBOXGUESTWAIT volatile     pHead;
    /** The tail. */
    PVBOXGUESTWAIT volatile     pTail;
} VBOXGUESTWAITLIST;
/** Pointer to a wait list. */
typedef VBOXGUESTWAITLIST *PVBOXGUESTWAITLIST;


/**
 * VBox guest device (data) extension.
 */
typedef struct VBOXGUESTDEVEXT
{
    /** The base of the adapter I/O ports. */
    RTCCPHYS                    PhysMMIOBase;
    /** The base of the adapter I/O ports. */
    RTIOPORT                    IOPortBase;
    /** The memory object for the MMIO memory. */
    RTR0MEMOBJ                  MemObjMMIO;
    /** The memory mapping object the MMIO memory. */
    RTR0MEMOBJ                  MemMapMMIO;
    /** Pointer to the mapping of the VMMDev adapter memory. */
    VMMDevMemory volatile      *pVMMDevMemory;

    /** Preallocated VMMDevEvents for the IRQ handler. */
    VMMDevEvents               *pIrqAckEvents;
    /** Spinlock protecting the signaling and resetting of the wait-for-event semaphores. */
    RTSPINLOCK                  WaitSpinlock;
    /** Wait-for-event list for threads waiting for multiple events. */
    VBOXGUESTWAITLIST           WaitList;
#ifdef VBOX_HGCM
    /** Wait-for-event list for threads waiting on HGCM async completion.
     * The entire list is evaluated upon the arrival of an HGCM event, unlike
     * the other lists which are only evaluated till the first thread has been woken up. */
    VBOXGUESTWAITLIST           HGCMWaitList;
#endif
    /** List of free wait-for-event entries. */
    VBOXGUESTWAITLIST           FreeList;
    /** Mask of pending events. */
    uint32_t volatile           f32PendingEvents;

    /** Spinlock various items in the VBOXGUESTSESSION. */
    RTSPINLOCK                  SessionSpinlock;

    /** The current clipboard client ID, 0 if no client.
     * For implementing the VBOXGUEST_IOCTL_CLIPBOARD_CONNECT interface. */
    uint32_t                    u32ClipboardClientId;


} VBOXGUESTDEVEXT;
/** Pointer to the VBoxGuest driver data. */
typedef VBOXGUESTDEVEXT *PVBOXGUESTDEVEXT;


/** Pointer to the VBoxGuest per session data. */
typedef struct VBOXGUESTSESSION *PVBOXGUESTSESSION;

/**
 * The VBoxGuest per session data.
 *
 * @remark  Not quite sure whether this will be useful or not, but since
 *          its already there let's keep it for now in case it might come
 *          in handy later.
 */
typedef struct VBOXGUESTSESSION
{
#if defined(RT_OS_OS2) || defined(RT_OS_FREEBSD) || defined(RT_OS_SOLARIS)
    /** Pointer to the next session with the same hash. */
    PVBOXGUESTSESSION           pNextHash;
#endif
#if defined(RT_OS_OS2)
    /** The system file number of this session. */
    uint16_t                    sfn;
    uint16_t                    Alignment; /**< Alignment */
#endif
    /** The process (id) of the session.
     * This is NIL if it's a kernel session. */
    RTPROCESS                   Process;
    /** Which process this session is associated with.
     * This is NIL if it's a kernel session. */
    RTR0PROCESS                 R0Process;
    /** Pointer to the device extension. */
    PVBOXGUESTDEVEXT            pDevExt;

#ifdef VBOX_HGCM
    /** Array containing HGCM client IDs associated with this session.
     * This will be automatically disconnected when the session is closed. */
    uint32_t volatile           aHGCMClientIds[8];
#endif
} VBOXGUESTSESSION;


__BEGIN_DECLS

int  VBoxGuestInitDevExt(PVBOXGUESTDEVEXT pDevExt, uint16_t IOPortBase, RTCCPHYS PhysMMIOBase, VBOXOSTYPE enmOSType);
void VBoxGuestDeleteDevExt(PVBOXGUESTDEVEXT pDevExt);
bool VBoxGuestCommonISR(PVBOXGUESTDEVEXT pDevExt);

int  VBoxGuestCreateUserSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession);
int  VBoxGuestCreateKernelSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession);
void VBoxGuestCloseSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession);

int  VBoxGuestCommonIOCtlFast(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession);
int  VBoxGuestCommonIOCtl(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                          void *pvData, size_t cbData, size_t *pcbDataReturned);

__END_DECLS

#endif

