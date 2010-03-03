/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver.
 */

/*
 * Copyright (C) 2007 Sun Microsystems, Inc.
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

#ifndef ___VBoxGuestInternal_h
#define ___VBoxGuestInternal_h

#include <iprt/types.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>


/** Pointer to the VBoxGuest per session data. */
typedef struct VBOXGUESTSESSION *PVBOXGUESTSESSION;

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
    /** The session that's waiting. */
    PVBOXGUESTSESSION           pSession;
#ifdef VBOX_WITH_HGCM
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
 * VBox guest memory balloon.
 */
typedef struct VBOXGUESTMEMBALLOON
{
    /** The current number of chunks in the balloon */
    uint32_t                    cChunks;
    /** The maximum number of chunks in the balloon (typically the amount of guest
     * memory / chunksize) */
    uint32_t                    cMaxChunks;
    /** This is true if we are using RTR0MemObjAllocPhysNC() / RTR0MemObjGetPagePhysAddr()
     * and false otherwise */
    bool                        fUseKernelAPI;
    /* The pointer to the array of memory objects holding the chunks of the balloon */
    RTR0MEMOBJ                 *paMemObj;
} VBOXGUESTMEMBALLOON;
typedef VBOXGUESTMEMBALLOON *PVBOXGUESTMEMBALLOON;


/**
 * VBox guest device (data) extension.
 */
typedef struct VBOXGUESTDEVEXT
{
    /** The base of the adapter I/O ports. */
    RTIOPORT                    IOPortBase;
    /** Pointer to the mapping of the VMMDev adapter memory. */
    VMMDevMemory volatile      *pVMMDevMemory;
    /** Events we won't permit anyone to filter out. */
    uint32_t                    fFixedEvents;
    /** The memory object reserving space for the guest mappings. */
    RTR0MEMOBJ                  hGuestMappings;

    /** Spinlock protecting the signaling and resetting of the wait-for-event
     * semaphores as well as the event acking in the ISR. */
    RTSPINLOCK                  EventSpinlock;
    /** Preallocated VMMDevEvents for the IRQ handler. */
    VMMDevEvents               *pIrqAckEvents;
    /** The physical address of pIrqAckEvents. */
    RTCCPHYS                    PhysIrqAckEvents;
    /** Wait-for-event list for threads waiting for multiple events. */
    VBOXGUESTWAITLIST           WaitList;
#ifdef VBOX_WITH_HGCM
    /** Wait-for-event list for threads waiting on HGCM async completion.
     * The entire list is evaluated upon the arrival of an HGCM event, unlike
     * the other lists which are only evaluated till the first thread has been woken up. */
    VBOXGUESTWAITLIST           HGCMWaitList;
#endif
    /** List of free wait-for-event entries. */
    VBOXGUESTWAITLIST           FreeList;
    /** Mask of pending events. */
    uint32_t volatile           f32PendingEvents;
    /** Current VMMDEV_EVENT_MOUSE_POSITION_CHANGED sequence number.
     * Used to implement polling.  */
    uint32_t volatile           u32MousePosChangedSeq;

    /** Spinlock various items in the VBOXGUESTSESSION. */
    RTSPINLOCK                  SessionSpinlock;

    /** The current clipboard client ID, 0 if no client.
     * For implementing the VBOXGUEST_IOCTL_CLIPBOARD_CONNECT interface. */
    uint32_t                    u32ClipboardClientId;

    /* Memory balloon information for RTR0MemObjAllocPhysNC(). */
    VBOXGUESTMEMBALLOON         MemBalloon;

} VBOXGUESTDEVEXT;
/** Pointer to the VBoxGuest driver data. */
typedef VBOXGUESTDEVEXT *PVBOXGUESTDEVEXT;


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

#ifdef VBOX_WITH_HGCM
    /** Array containing HGCM client IDs associated with this session.
     * This will be automatically disconnected when the session is closed. */
    uint32_t volatile           aHGCMClientIds[8];
#endif
    /** The last consumed VMMDEV_EVENT_MOUSE_POSITION_CHANGED sequence number.
     * Used to implement polling.  */
    uint32_t volatile           u32MousePosChangedSeq;

    /* Memory ballooning information if userland provides the balloon memory. */
    VBOXGUESTMEMBALLOON         MemBalloon;
} VBOXGUESTSESSION;

RT_C_DECLS_BEGIN

int  VBoxGuestInitDevExt(PVBOXGUESTDEVEXT pDevExt, uint16_t IOPortBase, void *pvMMIOBase, uint32_t cbMMIO, VBOXOSTYPE enmOSType, uint32_t fEvents);
void VBoxGuestDeleteDevExt(PVBOXGUESTDEVEXT pDevExt);
bool VBoxGuestCommonISR(PVBOXGUESTDEVEXT pDevExt);
int  VBoxGuestSetGuestCapabilities(uint32_t fOr, uint32_t fNot);

int  VBoxGuestCreateUserSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession);
int  VBoxGuestCreateKernelSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION *ppSession);
void VBoxGuestCloseSession(PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession);

int  VBoxGuestCommonIOCtlFast(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession);
int  VBoxGuestCommonIOCtl(unsigned iFunction, PVBOXGUESTDEVEXT pDevExt, PVBOXGUESTSESSION pSession,
                          void *pvData, size_t cbData, size_t *pcbDataReturned);

#if defined(RT_OS_SOLARIS) \
 || defined(RT_OS_FREEBSD) \
 || defined(RT_OS_LINUX)
DECLVBGL(void *) VBoxGuestNativeServiceOpen(uint32_t *pu32Version);
DECLVBGL(void)   VBoxGuestNativeServiceClose(void *pvOpaque);
DECLVBGL(int)    VBoxGuestNativeServiceCall(void *pvOpaque, unsigned int iCmd, void *pvData, size_t cbSize, size_t *pcbReturn);
#endif

/**
 * ISR callback for notifying threads polling for mouse events.
 *
 * This is called at the end of the ISR, after leaving the event spinlock, if
 * VMMDEV_EVENT_MOUSE_POSITION_CHANGED was raised by the host.
 *
 * @param   pDevExt     The device extension.
 */
void VBoxGuestNativeISRMousePollEvent(PVBOXGUESTDEVEXT pDevExt);

RT_C_DECLS_END

#endif

