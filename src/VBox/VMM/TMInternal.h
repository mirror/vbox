/* $Id$ */
/** @file
 * TM - Internal header file.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */

#ifndef __TMInternal_h__
#define __TMInternal_h__

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/timer.h>
#include <VBox/stam.h>

__BEGIN_DECLS


/** @defgroup grp_tm_int       Internal
 * @ingroup grp_tm
 * @internal
 * @{
 */

/** Frequency of the real clock. */
#define TMCLOCK_FREQ_REAL       UINT32_C(1000)
/** Frequency of the virtual clock. */
#define TMCLOCK_FREQ_VIRTUAL    UINT32_C(1000000000)


/**
 * Timer type.
 */
typedef enum TMTIMERTYPE
{
    /** Device timer. */
    TMTIMERTYPE_DEV = 1,
    /** Driver timer. */
    TMTIMERTYPE_DRV,
    /** Internal timer . */
    TMTIMERTYPE_INTERNAL,
    /** External timer. */
    TMTIMERTYPE_EXTERNAL
} TMTIMERTYPE;

/**
 * Timer state
 */
typedef enum TMTIMERSTATE
{
    /** Timer is stopped. */
    TMTIMERSTATE_STOPPED = 1,
    /** Timer is active. */
    TMTIMERSTATE_ACTIVE,
    /** Timer is expired, is being delivered. */
    TMTIMERSTATE_EXPIRED,

    /** Timer is stopped but still in the active list.
     * Currently in the ScheduleTimers list. */
    TMTIMERSTATE_PENDING_STOP,
    /** Timer is stopped but needs unlinking from the ScheduleTimers list.
     * Currently in the ScheduleTimers list. */
    TMTIMERSTATE_PENDING_STOP_SCHEDULE,
    /** Timer is being modified and will soon be pending scheduling.
     * Currently in the ScheduleTimers list. */
    TMTIMERSTATE_PENDING_SCHEDULE_SET_EXPIRE,
    /** Timer is pending scheduling.
     * Currently in the ScheduleTimers list. */
    TMTIMERSTATE_PENDING_SCHEDULE,
    /** Timer is being modified and will soon be pending rescheduling.
     * Currently in the ScheduleTimers list and the active list. */
    TMTIMERSTATE_PENDING_RESCHEDULE_SET_EXPIRE,
    /** Timer is modified and is now pending rescheduling.
     * Currently in the ScheduleTimers list and the active list. */
    TMTIMERSTATE_PENDING_RESCHEDULE,
    /** Timer is destroyed but needs to be replaced from the
     * active to the free list.
     * Currently in the ScheduleTimers list and the active list. */
    TMTIMERSTATE_PENDING_STOP_DESTROY,
    /** Timer is destroyed but needs moving to the free list.
     * Currently in the ScheduleTimers list. */
    TMTIMERSTATE_PENDING_DESTROY,
    /** Timer is free. */
    TMTIMERSTATE_FREE
} TMTIMERSTATE;


/**
 * Internal representation of a timer.
 *
 * For correct serialization (without the use of semaphores and
 * other blocking/slow constructs) certain rules applies to updating
 * this structure:
 *      - For thread other than EMT only u64Expire, enmState and pScheduleNext*
 *        are changeable. Everything else is out of bounds.
 *      - Updating of u64Expire timer can only happen in the TMTIMERSTATE_STOPPED
 *        and TMTIMERSTATE_PENDING_RESCHEDULING_SET_EXPIRE states.
 *      - Timers in the TMTIMERSTATE_EXPIRED state are only accessible from EMT.
 *      - Actual destruction of a timer can only be done at scheduling time.
 */
typedef struct TMTIMER
{
    /** Expire time. */
    volatile uint64_t       u64Expire;
    /** Clock to apply to u64Expire. */
    TMCLOCK                 enmClock;
    /** Timer callback type. */
    TMTIMERTYPE             enmType;
    /** Type specific data. */
    union
    {
        /** TMTIMERTYPE_DEV. */
        struct
        {
            /** Callback. */
            R3PTRTYPE(PFNTMTIMERDEV)    pfnTimer;
            /** Device instance. */
            R3PTRTYPE(PPDMDEVINS)       pDevIns;
        } Dev;

        /** TMTIMERTYPE_DRV. */
        struct
        {
            /** Callback. */
            R3PTRTYPE(PFNTMTIMERDRV)    pfnTimer;
            /** Device instance. */
            R3PTRTYPE(PPDMDRVINS)       pDrvIns;
        } Drv;

        /** TMTIMERTYPE_INTERNAL. */
        struct
        {
            /** Callback. */
            R3PTRTYPE(PFNTMTIMERINT)    pfnTimer;
            /** User argument. */
            R3PTRTYPE(void *)           pvUser;
        } Internal;

        /** TMTIMERTYPE_EXTERNAL. */
        struct
        {
            /** Callback. */
            R3PTRTYPE(PFNTMTIMEREXT)    pfnTimer;
            /** User data. */
            R3PTRTYPE(void *)           pvUser;
        } External;
    } u;

    /** Timer state. */
    volatile TMTIMERSTATE   enmState;
    /** Timer relative offset to the next timer in the schedule list. */
    int32_t                 offScheduleNext;

    /** Timer relative offset to the next timer in the chain. */
    int32_t                 offNext;
    /** Timer relative offset to the previous timer in the chain. */
    int32_t                 offPrev;

    /** Pointer to the next timer in the list of created or free timers. (TM::pTimers or TM::pFree) */
    PTMTIMERR3              pBigNext;
    /** Pointer to the previous timer in the list of all created timers. (TM::pTimers) */
    PTMTIMERR3              pBigPrev;
    /** Pointer to the timer description. */
    HCPTRTYPE(const char *) pszDesc;
    /** Pointer to the VM the timer belongs to - R3 Ptr. */
    PVMR3                   pVMR3;
    /** Pointer to the VM the timer belongs to - R0 Ptr. */
    PVMR0                   pVMR0;
    /** Pointer to the VM the timer belongs to - GC Ptr. */
    PVMGC                   pVMGC;
#if HC_ARCH_BITS == 64 && GC_ARCH_BITS == 32
    RTGCPTR                 padding0; /**< pad structure to multiple of 8 bytes. */
#endif
} TMTIMER;


/**
 * Updates a timer state in the correct atomic manner.
 */
#if 1
# define TM_SET_STATE(pTimer, state) \
    ASMAtomicXchgSize(&(pTimer)->enmState, state)
#else
# define TM_SET_STATE(pTimer, state) \
    do { Log(("%s: %p: %d -> %d\n", __FUNCTION__, (pTimer), (pTimer)->enmState, state)); \
         ASMAtomicXchgSize(&(pTimer)->enmState, state);\
    } while (0)
#endif

/**
 * Tries to updates a timer state in the correct atomic manner.
 */
#if 1
# define TM_TRY_SET_STATE(pTimer, StateNew, StateOld, fRc) \
    ASMAtomicCmpXchgSize(&(pTimer)->enmState, StateNew, StateOld, fRc)
#else
# define TM_TRY_SET_STATE(pTimer, StateNew, StateOld, fRc) \
    do { ASMAtomicCmpXchgSize(&(pTimer)->enmState, StateNew, StateOld, fRc); \
         Log(("%s: %p: %d -> %d %RTbool\n", __FUNCTION__, (pTimer), StateOld, StateNew, fRc)); \
    } while (0)
#endif

/** Get the previous timer. */
#define TMTIMER_GET_PREV(pTimer) ((PTMTIMER)((pTimer)->offPrev ? (intptr_t)(pTimer) + (pTimer)->offPrev : 0))
/** Get the next timer. */
#define TMTIMER_GET_NEXT(pTimer) ((PTMTIMER)((pTimer)->offNext ? (intptr_t)(pTimer) + (pTimer)->offNext : 0))
/** Set the previous timer link. */
#define TMTIMER_SET_PREV(pTimer, pPrev) ((pTimer)->offPrev = (pPrev) ? (intptr_t)(pPrev) - (intptr_t)(pTimer) : 0)
/** Set the next timer link. */
#define TMTIMER_SET_NEXT(pTimer, pNext) ((pTimer)->offNext = (pNext) ? (intptr_t)(pNext) - (intptr_t)(pTimer) : 0)


/**
 * A timer queue.
 *
 * This is allocated on the hyper heap.
 */
typedef struct TMTIMERQUEUE
{
    /** The cached expire time for this queue.
     * Updated by EMT when scheduling the queue or modifying the head timer.
     * Assigned UINT64_MAX when there is no head timer. */
    uint64_t                u64Expire;
    /** Doubly linked list of active timers.
     *
     * When no scheduling is pending, this list is will be ordered by expire time (ascending).
     * Access is serialized by only letting the emulation thread (EMT) do changes.
     *
     * The offset is relative to the queue structure.
     */
    int32_t                 offActive;
    /** List of timers pending scheduling of some kind.
     *
     * Timer stats allowed in the list are TMTIMERSTATE_PENDING_STOPPING,
     * TMTIMERSTATE_PENDING_DESTRUCTION, TMTIMERSTATE_PENDING_STOPPING_DESTRUCTION,
     * TMTIMERSTATE_PENDING_RESCHEDULING and TMTIMERSTATE_PENDING_SCHEDULE.
     *
     * The offset is relative to the queue structure.
     */
    int32_t volatile        offSchedule;
    /** The clock for this queue. */
    TMCLOCK                 enmClock;
    /** Pad the structure up to 32 bytes. */
    uint32_t                au32Padding[3];
} TMTIMERQUEUE;

/** Pointer to a timer queue. */
typedef TMTIMERQUEUE *PTMTIMERQUEUE;

/** Get the head of the active timer list. */
#define TMTIMER_GET_HEAD(pQueue)        ((PTMTIMER)((pQueue)->offActive ? (intptr_t)(pQueue) + (pQueue)->offActive : 0))
/** Set the head of the active timer list. */
#define TMTIMER_SET_HEAD(pQueue, pHead) ((pQueue)->offActive = pHead ? (intptr_t)pHead - (intptr_t)(pQueue) : 0)


/**
 * Converts a TM pointer into a VM pointer.
 * @returns Pointer to the VM structure the TM is part of.
 * @param   pTM   Pointer to TM instance data.
 */
#define TM2VM(pTM)  ( (PVM)((char*)pTM - pTM->offVM) )


/**
 * TM VM Instance data.
 * Changes to this must checked against the padding of the cfgm union in VM!
 */
typedef struct TM
{
    /** Offset to the VM structure.
     * See TM2VM(). */
    RTUINT                      offVM;

    /** CPU timestamp ticking enabled indicator (bool). (RDTSC) */
    bool                        fTSCTicking;
    /** The offset between the host TSC and the Guest TSC.
     * Only valid if fTicking is set. */
    uint64_t                    u64TSCOffset;
    /** The guest TSC when fTicking is cleared. */
    uint64_t                    u64TSC;
    /** The number of CPU clock ticks per second (TMCLOCK_TSC).
     * If GIP is available, g_pSUPGlobalInfoPage->u64CpuHz will be used instead. */
    uint64_t                    cTSCTicksPerSecond;

    /** Virtual time ticking enabled indicator (bool). (TMCLOCK_VIRTUAL) */
    bool                        fVirtualTicking;
    /** Virtual time is not running at 100%. */
    bool                        fVirtualWarpDrive;
    /** Virtual timer synchronous time ticking enabled indicator (bool). (TMCLOCK_VIRTUAL_SYNC) */
    bool                        fVirtualSyncTicking;
    /** Virtual timer synchronous time catch-up active. */
    bool volatile               fVirtualSyncCatchUp;
    /** WarpDrive percentage. 
     * 100% is normal (fVirtualSyncNormal == true). When other than 100% we apply 
     * this percentage to the raw time source for the period it's been valid in, 
     * i.e. since u64VirtualWarpDriveStart. */
    uint32_t                    u32VirtualWarpDrivePercentage;

    /** The offset of the virtual clock relative to it's timesource.
     * Only valid if fVirtualTicking is set. */
    uint64_t                    u64VirtualOffset;
    /** The guest virtual time when fVirtualTicking is cleared. */
    uint64_t                    u64Virtual;
    /** When the Warp drive was started or last adjusted.
     * Only valid when fVirtualWarpDrive is set. */
    uint64_t                    u64VirtualWarpDriveStart;

    /** The offset of the virtual timer synchronous clock (TMCLOCK_VIRTUAL_SYNC) relative
     * to the virtual clock. */
    uint64_t volatile           u64VirtualSyncOffset;
    /** The TMCLOCK_VIRTUAL at the previous TMVirtualGetSync call when catch-up is active. */
    uint64_t volatile           u64VirtualSyncCatchUpPrev;
    /** The guest virtual timer synchronous time when fVirtualSyncTicking is cleared. */
    uint64_t                    u64VirtualSync;
    /** How many percent faster the clock should advance when catch-up is active. */
    uint32_t                    u32VirtualSyncCatchupPercentage;
    /** When to stop catch-up. */
    uint32_t                    u32VirtualSyncCatchupStopThreashold;
    /** When to start catch-up. */
    uint64_t                    u64VirtualSyncCatchupStartTreashold;
    /** When to give up catch-up. */
    uint64_t                    u64VirtualSyncCatchupGiveUpTreashold;

    /** Timer queues for the different clock types - R3 Ptr */
    R3PTRTYPE(PTMTIMERQUEUE)    paTimerQueuesR3;
    /** Timer queues for the different clock types - R0 Ptr */
    R0PTRTYPE(PTMTIMERQUEUE)    paTimerQueuesR0;
    /** Timer queues for the different clock types - GC Ptr */
    GCPTRTYPE(PTMTIMERQUEUE)    paTimerQueuesGC;

    /** Pointer to our GC mapping of the GIP. */
    GCPTRTYPE(void *)           pvGIPGC;
    /** Pointer to our R3 mapping of the GIP. */
    R3PTRTYPE(void *)           pvGIPR3;

    /** Pointer to a singly linked list of free timers.
     * This chain is using the TMTIMER::pBigNext members.
     * Only accessible from the emulation thread. */
    PTMTIMERR3                  pFree;

    /** Pointer to a doubly linked list of created timers.
     * This chain is using the TMTIMER::pBigNext and TMTIMER::pBigPrev members.
     * Only accessible from the emulation thread. */
    PTMTIMERR3                  pCreated;

    /** The schedulation timer timer handle (runtime timer).
     * This timer will do freqent check on pending queue schedulations and
     * raise VM_FF_TIMER to pull EMTs attention to them.
     */
    HCPTRTYPE(PRTTIMER)         pTimer;
    /** Interval in milliseconds of the pTimer timer. */
    uint32_t                    u32TimerMillies;

    /** Alignment padding to ensure that the statistics are 64-bit aligned when using GCC. */
    uint32_t                    u32Padding;

    /** TMR3TimerQueuesDo
     * @{ */
    STAMPROFILE                 StatDoQueues;
    STAMPROFILEADV              StatDoQueuesSchedule;
    STAMPROFILEADV              StatDoQueuesRun;
    /** @} */
    /** tmSchedule
     * @{ */
    STAMPROFILE                 StatScheduleOneGC;
    STAMPROFILE                 StatScheduleOneR0;
    STAMPROFILE                 StatScheduleOneR3;
    STAMCOUNTER                 StatScheduleSetFF;
    /** @} */
    STAMCOUNTER                 StatVirtualGet;
    STAMCOUNTER                 StatVirtualGetSync;
    STAMCOUNTER                 StatVirtualPause;
    STAMCOUNTER                 StatVirtualResume;
    /** TMTimerPoll
     * @{ */
    STAMCOUNTER                 StatPollAlreadySet;
    STAMCOUNTER                 StatPollVirtual;
    STAMCOUNTER                 StatPollVirtualSync;
    STAMCOUNTER                 StatPollMiss;
    /** @} */
    /** TMTimerSet
     * @{ */
    STAMPROFILE                 StatTimerSetGC;
    STAMPROFILE                 StatTimerSetR0;
    STAMPROFILE                 StatTimerSetR3;
    /** @} */
    /** TMTimerStop
     * @{ */
    STAMPROFILE                 StatTimerStopGC;
    STAMPROFILE                 StatTimerStopR0;
    STAMPROFILE                 StatTimerStopR3;
    /** @} */
    /**
     * @{ */
    STAMCOUNTER                 StatPostponedR3;
    STAMCOUNTER                 StatPostponedR0;
    STAMCOUNTER                 StatPostponedGC;
    /** @} */
    /** The timer callback. */
    STAMCOUNTER                 StatTimerCallbackSetFF;

} TM;
/** Pointer to TM VM instance data. */
typedef TM *PTM;


const char *tmTimerState(TMTIMERSTATE enmState);
void tmTimerQueueSchedule(PVM pVM, PTMTIMERQUEUE pQueue);
#ifdef VBOX_STRICT
void tmTimerQueuesSanityChecks(PVM pVM, const char *pszWhere);
#endif

/** @} */

__END_DECLS

#endif

