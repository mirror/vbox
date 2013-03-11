/* $Id$ */
/** @file
 * VBoxGuest - Guest Additions Driver.
 */

/*
 * Copyright (C) 2010-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBoxGuestInternal_h
#define ___VBoxGuestInternal_h

#include <iprt/types.h>
#include <iprt/list.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <VBox/VMMDev.h>
#include <VBox/VBoxGuest.h>
#include <VBox/VBoxGuestLib.h>

/** @def VBOXGUEST_USE_WAKE_UP_LIST
 * Defer wake-up of waiting thread when defined. */
#if defined(RT_OS_SOLARIS) || defined(RT_OS_WINDOWS) || defined(DOXYGEN_RUNNING)
# define VBOXGUEST_USE_DEFERRED_WAKE_UP
#endif


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
    /** The list node. */
    RTLISTNODE                  ListNode;
    /** The events we are waiting on. */
    uint32_t                    fReqEvents;
    /** The events we received. */
    uint32_t volatile           fResEvents;
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    /** Set by VBoxGuestWaitDoWakeUps before leaving the spinlock to call
     *  RTSemEventMultiSignal. */
    bool volatile               fPendingWakeUp;
    /** Set by the requestor thread if it got the spinlock before the
     * signaller.  Deals with the race in VBoxGuestWaitDoWakeUps. */
    bool volatile               fFreeMe;
#endif
    /** The event semaphore. */
    RTSEMEVENTMULTI             Event;
    /** The session that's waiting. */
    PVBOXGUESTSESSION           pSession;
#ifdef VBOX_WITH_HGCM
    /** The HGCM request we're waiting for to complete. */
    VMMDevHGCMRequestHeader volatile *pHGCMReq;
#endif
} VBOXGUESTWAIT;


/**
 * VBox guest memory balloon.
 */
typedef struct VBOXGUESTMEMBALLOON
{
    /** Mutex protecting the members below from concurrent access.. */
    RTSEMFASTMUTEX              hMtx;
    /** The current number of chunks in the balloon. */
    uint32_t                    cChunks;
    /** The maximum number of chunks in the balloon (typically the amount of guest
     * memory / chunksize). */
    uint32_t                    cMaxChunks;
    /** This is true if we are using RTR0MemObjAllocPhysNC() / RTR0MemObjGetPagePhysAddr()
     * and false otherwise. */
    bool                        fUseKernelAPI;
    /** The current owner of the balloon.
     * This is automatically assigned to the first session using the ballooning
     * API and first released when the session closes. */
    PVBOXGUESTSESSION           pOwner;
    /** The pointer to the array of memory objects holding the chunks of the
     *  balloon.  This array is cMaxChunks in size when present. */
    PRTR0MEMOBJ                 paMemObj;
} VBOXGUESTMEMBALLOON;
/** Pointer to a memory balloon. */
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
    /** Wait-for-event list for threads waiting for multiple events
     * (VBOXGUESTWAIT). */
    RTLISTANCHOR                WaitList;
#ifdef VBOX_WITH_HGCM
    /** Wait-for-event list for threads waiting on HGCM async completion
     * (VBOXGUESTWAIT).
     *
     * The entire list is evaluated upon the arrival of an HGCM event, unlike
     * the other lists which are only evaluated till the first thread has
     * been woken up. */
    RTLISTANCHOR                HGCMWaitList;
#endif
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
    /** List of wait-for-event entries that needs waking up
     * (VBOXGUESTWAIT). */
    RTLISTANCHOR                WakeUpList;
#endif
    /** List of wait-for-event entries that has been woken up
     * (VBOXGUESTWAIT). */
    RTLISTANCHOR                WokenUpList;
    /** List of free wait-for-event entries (VBOXGUESTWAIT). */
    RTLISTANCHOR                FreeList;
    /** Mask of pending events. */
    uint32_t volatile           f32PendingEvents;
    /** Current VMMDEV_EVENT_MOUSE_POSITION_CHANGED sequence number.
     * Used to implement polling.  */
    uint32_t volatile           u32MousePosChangedSeq;

    /** Spinlock various items in the VBOXGUESTSESSION. */
    RTSPINLOCK                  SessionSpinlock;
#ifdef VBOX_WITH_VRDP_SESSION_HANDLING
    bool                        fVRDPEnabled;
#endif
    /** Flag indicating whether logging to the release log
     *  is enabled. */
    bool                        fLoggingEnabled;
    /** Memory balloon information for RTR0MemObjAllocPhysNC(). */
    VBOXGUESTMEMBALLOON         MemBalloon;
    /** For each mouse status feature the number of sessions which have
     * enabled it.  A feature is enabled globally if at least one session has
     * requested it. */
    /** @todo can we programmatically determine the size of the array and
     * still get the following alignment right? */
    uint32_t volatile           acMouseFeatureUsage[32];
    /** The mouse feature status matching the counts above.  These are updated
     * together inside the session spinlock. */
    uint32_t volatile           fMouseStatus;
    /** Counter of number of active ISRs.  Currently used for safely removing
     * the mouse handler callback. */
    uint32_t volatile           cISR;
    /** Callback and user data for a kernel mouse handler. */
    VBoxGuestMouseSetNotifyCallback MouseNotifyCallback;

    /** Windows part.
     * @todo r=bird: Entirely wrong approach. You should create
     *       VBOXGUESTDEVEXTWIN as a super structure to VBOXGUESTDEVEXT. If they
     *       were classes, class VBOXGUESTDEVEXTWIN : public VBOXGUESTDEVEXT ... */
    union
    {
#ifdef ___VBoxGuest_win_h
        VBOXGUESTDEVEXTWIN          s;
#else
        uint32_t                    dummy;
#endif
    } win;

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
    uint32_t volatile           aHGCMClientIds[64];
#endif
    /** The last consumed VMMDEV_EVENT_MOUSE_POSITION_CHANGED sequence number.
     * Used to implement polling.  */
    uint32_t volatile           u32MousePosChangedSeq;
    /** Mouse features supported.  A feature enabled in any guest session will
     * be enabled for the host. */
    uint32_t volatile           fMouseStatus;

} VBOXGUESTSESSION;

RT_C_DECLS_BEGIN

int  VBoxGuestInitDevExt(PVBOXGUESTDEVEXT pDevExt, uint16_t IOPortBase, void *pvMMIOBase, uint32_t cbMMIO, VBOXOSTYPE enmOSType, uint32_t fEvents);
bool VBoxGuestCommonISR(PVBOXGUESTDEVEXT pDevExt);
void VBoxGuestDeleteDevExt(PVBOXGUESTDEVEXT pDevExt);
int  VBoxGuestReinitDevExtAfterHibernation(PVBOXGUESTDEVEXT pDevExt, VBOXOSTYPE enmOSType);
int  VBoxGuestSetGuestCapabilities(uint32_t fOr, uint32_t fNot);
#ifdef VBOXGUEST_USE_DEFERRED_WAKE_UP
void VBoxGuestWaitDoWakeUps(PVBOXGUESTDEVEXT pDevExt);
#endif

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

