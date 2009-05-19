/* $Id$ */
/** @file
 * TM - Internal header file.
 */

/*
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

#ifndef ___TMInternal_h
#define ___TMInternal_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <iprt/time.h>
#include <iprt/timer.h>
#include <iprt/assert.h>
#include <VBox/stam.h>
#include <VBox/pdmcritsect.h>

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
    /** Timer is being destroyed. */
    TMTIMERSTATE_DESTROY,
    /** Timer is free. */
    TMTIMERSTATE_FREE
} TMTIMERSTATE;

/** Predicate that returns true if the give state is pending scheduling or
 *  rescheduling of any kind. Will reference the argument more than once! */
#define TMTIMERSTATE_IS_PENDING_SCHEDULING(enmState) \
    (   (enmState) <= TMTIMERSTATE_PENDING_RESCHEDULE \
     && (enmState) >= TMTIMERSTATE_PENDING_SCHEDULE_SET_EXPIRE)


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
            PPDMDEVINSR3                pDevIns;
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
            RTR3PTR                     pvUser;
        } Internal;

        /** TMTIMERTYPE_EXTERNAL. */
        struct
        {
            /** Callback. */
            R3PTRTYPE(PFNTMTIMEREXT)    pfnTimer;
            /** User data. */
            RTR3PTR                     pvUser;
        } External;
    } u;

    /** Timer state. */
    volatile TMTIMERSTATE   enmState;
    /** Timer relative offset to the next timer in the schedule list. */
    int32_t volatile        offScheduleNext;

    /** Timer relative offset to the next timer in the chain. */
    int32_t                 offNext;
    /** Timer relative offset to the previous timer in the chain. */
    int32_t                 offPrev;

    /** Pointer to the next timer in the list of created or free timers. (TM::pTimers or TM::pFree) */
    PTMTIMERR3              pBigNext;
    /** Pointer to the previous timer in the list of all created timers. (TM::pTimers) */
    PTMTIMERR3              pBigPrev;
    /** Pointer to the timer description. */
    R3PTRTYPE(const char *) pszDesc;
    /** Pointer to the VM the timer belongs to - R3 Ptr. */
    PVMR3                   pVMR3;
    /** Pointer to the VM the timer belongs to - R0 Ptr. */
    PVMR0                   pVMR0;
    /** Pointer to the VM the timer belongs to - RC Ptr. */
    PVMRC                   pVMRC;
#if HC_ARCH_BITS == 64
    RTRCPTR                 padding0; /**< pad structure to multiple of 8 bytes. */
#endif
} TMTIMER;
AssertCompileMemberSize(TMTIMER, enmState, sizeof(uint32_t));


/**
 * Updates a timer state in the correct atomic manner.
 */
#if 1
# define TM_SET_STATE(pTimer, state) \
    ASMAtomicWriteU32((uint32_t volatile *)&(pTimer)->enmState, state)
#else
# define TM_SET_STATE(pTimer, state) \
    do { \
        uint32_t uOld1 = (pTimer)->enmState; \
        Log(("%s: %p: %d -> %d\n", __FUNCTION__, (pTimer), (pTimer)->enmState, state)); \
        uint32_t uOld2 = ASMAtomicXchgU32((uint32_t volatile *)&(pTimer)->enmState, state); \
        Assert(uOld1 == uOld2); \
    } while (0)
#endif

/**
 * Tries to updates a timer state in the correct atomic manner.
 */
#if 1
# define TM_TRY_SET_STATE(pTimer, StateNew, StateOld, fRc) \
    (fRc) = ASMAtomicCmpXchgU32((uint32_t volatile *)&(pTimer)->enmState, StateNew, StateOld)
#else
# define TM_TRY_SET_STATE(pTimer, StateNew, StateOld, fRc) \
    do { (fRc) = ASMAtomicCmpXchgU32((uint32_t volatile *)&(pTimer)->enmState, StateNew, StateOld); \
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

    /** Set if we fully virtualize the TSC, i.e. intercept all rdtsc instructions.
     * Config variable: TSCVirtualized (bool) */
    bool                        fTSCVirtualized;
    /** Set if we use the real TSC as time source or if we use the virtual clock.
     * If fTSCVirtualized is set we maintain a offset to the TSC and pausing/resuming the
     * ticking. fTSCVirtualized = false implies fTSCUseRealTSC = true.
     * Config variable: TSCUseRealTSC (bool) */
    bool                        fTSCUseRealTSC;
    /** Flag indicating that the host TSC is suitable for use in AMD-V and VT-x mode.
     * Config variable: MaybeUseOffsettedHostTSC (boolean) */
    bool                        fMaybeUseOffsettedHostTSC;
    /** Whether the TSC is tied to the execution of code.
     * Config variable: TSCTiedToExecution (bool) */
    bool                        fTSCTiedToExecution;
    /** Modifier for fTSCTiedToExecution which pauses the TSC while halting if true.
     * Config variable: TSCNotTiedToHalt (bool) */
    bool                        fTSCNotTiedToHalt;
    bool                        afAlignment0[2]; /**< alignment padding */
    /** The ID of the virtual CPU that normally runs the timers. */
    VMCPUID                     idTimerCpu;
    /** The number of CPU clock ticks per second (TMCLOCK_TSC).
     * Config variable: TSCTicksPerSecond (64-bit unsigned int)
     * The config variable implies fTSCVirtualized = true and fTSCUseRealTSC = false. */
    uint64_t                    cTSCTicksPerSecond;

    /** Virtual time ticking enabled indicator (counter for each VCPU). (TMCLOCK_VIRTUAL) */
    uint32_t volatile           cVirtualTicking;
    /** Virtual time is not running at 100%. */
    bool                        fVirtualWarpDrive;
    /** Virtual timer synchronous time ticking enabled indicator (bool). (TMCLOCK_VIRTUAL_SYNC) */
    bool volatile               fVirtualSyncTicking;
    /** Virtual timer synchronous time catch-up active. */
    bool volatile               fVirtualSyncCatchUp;
    bool                        afAlignment[5]; /**< alignment padding */
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
    /** The previously returned nano TS.
     * This handles TSC drift on SMP systems and expired interval.
     * This is a valid range u64NanoTS to u64NanoTS + 1000000000 (ie. 1sec). */
    uint64_t volatile           u64VirtualRawPrev;
    /** The ring-3 data structure for the RTTimeNanoTS workers used by tmVirtualGetRawNanoTS. */
    RTTIMENANOTSDATAR3          VirtualGetRawDataR3;
    /** The ring-0 data structure for the RTTimeNanoTS workers used by tmVirtualGetRawNanoTS. */
    RTTIMENANOTSDATAR0          VirtualGetRawDataR0;
    /** The ring-0 data structure for the RTTimeNanoTS workers used by tmVirtualGetRawNanoTS. */
    RTTIMENANOTSDATARC          VirtualGetRawDataRC;
    /** Pointer to the ring-3 tmVirtualGetRawNanoTS worker function. */
    R3PTRTYPE(PFNTIMENANOTSINTERNAL) pfnVirtualGetRawR3;
    /** Pointer to the ring-3 tmVirtualGetRawNanoTS worker function. */
    R0PTRTYPE(PFNTIMENANOTSINTERNAL) pfnVirtualGetRawR0;
    /** Pointer to the ring-3 tmVirtualGetRawNanoTS worker function. */
    RCPTRTYPE(PFNTIMENANOTSINTERNAL) pfnVirtualGetRawRC;
    /** Alignment. */
    RTRCPTR                     AlignmentRCPtr;
    /** The guest virtual timer synchronous time when fVirtualSyncTicking is cleared. */
    uint64_t volatile           u64VirtualSync;
    /** The offset of the timer synchronous virtual clock (TMCLOCK_VIRTUAL_SYNC) relative
     * to the virtual clock (TMCLOCK_VIRTUAL).
     * (This is accessed by the timer thread and must be updated atomically.) */
    uint64_t volatile           offVirtualSync;
    /** The offset into offVirtualSync that's been irrevocably given up by failed catch-up attempts.
     * Thus the current lag is offVirtualSync - offVirtualSyncGivenUp. */
    uint64_t                    offVirtualSyncGivenUp;
    /** The TMCLOCK_VIRTUAL at the previous TMVirtualGetSync call when catch-up is active. */
    uint64_t volatile           u64VirtualSyncCatchUpPrev;
    /** The current catch-up percentage. */
    uint32_t volatile           u32VirtualSyncCatchUpPercentage;
    /** How much slack when processing timers. */
    uint32_t                    u32VirtualSyncScheduleSlack;
    /** When to stop catch-up. */
    uint64_t                    u64VirtualSyncCatchUpStopThreshold;
    /** When to give up catch-up. */
    uint64_t                    u64VirtualSyncCatchUpGiveUpThreshold;
/** @def TM_MAX_CATCHUP_PERIODS
 * The number of catchup rates. */
#define TM_MAX_CATCHUP_PERIODS  10
    /** The agressivness of the catch-up relative to how far we've lagged behind.
     * The idea is to have increasing catch-up percentage as the lag increases. */
    struct TMCATCHUPPERIOD
    {
        uint64_t                u64Start;       /**< When this period starts. (u64VirtualSyncOffset). */
        uint32_t                u32Percentage;  /**< The catch-up percent to apply. */
        uint32_t                u32Alignment;   /**< Structure alignment */
    }                           aVirtualSyncCatchUpPeriods[TM_MAX_CATCHUP_PERIODS];

    /** The UTC offset in ns.
     * This is *NOT* for converting UTC to local time. It is for converting real
     * world UTC time to VM UTC time. This feature is indented for doing date
     * testing of software and similar.
     * @todo Implement warpdrive on UTC. */
    int64_t                     offUTC;

    /** Timer queues for the different clock types - R3 Ptr */
    R3PTRTYPE(PTMTIMERQUEUE)    paTimerQueuesR3;
    /** Timer queues for the different clock types - R0 Ptr */
    R0PTRTYPE(PTMTIMERQUEUE)    paTimerQueuesR0;
    /** Timer queues for the different clock types - RC Ptr */
    RCPTRTYPE(PTMTIMERQUEUE)    paTimerQueuesRC;

    /** Pointer to our RC mapping of the GIP. */
    RCPTRTYPE(void *)           pvGIPRC;
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
    R3PTRTYPE(PRTTIMER)         pTimer;
    /** Interval in milliseconds of the pTimer timer. */
    uint32_t                    u32TimerMillies;

    /** Indicates that queues are being run. */
    bool volatile               fRunningQueues;
    /** Indicates that the virtual sync queue is being run. */
    bool volatile               fRunningVirtualSyncQueue;
    /* Alignment */
    bool                        u8Alignment[2];

    /** Lock serializing EMT access to TM. */
    PDMCRITSECT                 EmtLock;
    /** Lock serializing access to the VirtualSync clock. */
    PDMCRITSECT                 VirtualSyncLock;

    /** TMR3TimerQueuesDo
     * @{ */
    STAMPROFILE                 StatDoQueues;
    STAMPROFILEADV              aStatDoQueues[TMCLOCK_MAX];
    /** @} */
    /** tmSchedule
     * @{ */
    STAMPROFILE                 StatScheduleOneRZ;
    STAMPROFILE                 StatScheduleOneR3;
    STAMCOUNTER                 StatScheduleSetFF;
    STAMCOUNTER                 StatPostponedR3;
    STAMCOUNTER                 StatPostponedRZ;
    /** @} */
    /** Read the time
     * @{ */
    STAMCOUNTER                 StatVirtualGet;
    STAMCOUNTER                 StatVirtualGetSetFF;
    STAMCOUNTER                 StatVirtualSyncGet;
    STAMCOUNTER                 StatVirtualSyncGetELoop;
    STAMCOUNTER                 StatVirtualSyncGetExpired;
    STAMCOUNTER                 StatVirtualSyncGetLockless;
    STAMCOUNTER                 StatVirtualSyncGetLocked;
    STAMCOUNTER                 StatVirtualSyncGetSetFF;
    STAMCOUNTER                 StatVirtualPause;
    STAMCOUNTER                 StatVirtualResume;
    /* @} */
    /** TMTimerPoll
     * @{ */
    STAMCOUNTER                 StatPoll;
    STAMCOUNTER                 StatPollAlreadySet;
    STAMCOUNTER                 StatPollELoop;
    STAMCOUNTER                 StatPollMiss;
    STAMCOUNTER                 StatPollRunning;
    STAMCOUNTER                 StatPollSimple;
    STAMCOUNTER                 StatPollVirtual;
    STAMCOUNTER                 StatPollVirtualSync;
    /** @} */
    /** TMTimerPollGIP
     * @{ */
    STAMCOUNTER                 StatPollGIP;
    STAMCOUNTER                 StatPollGIPAlreadySet;
    STAMCOUNTER                 StatPollGIPVirtual;
    STAMCOUNTER                 StatPollGIPVirtualSync;
    STAMCOUNTER                 StatPollGIPMiss;
    STAMCOUNTER                 StatPollGIPRunning;
    /** @} */
    /** TMTimerSet
     * @{ */
    STAMPROFILE                 StatTimerSetRZ;
    STAMPROFILE                 StatTimerSetR3;
    /** @} */
    /** TMTimerStop
     * @{ */
    STAMPROFILE                 StatTimerStopRZ;
    STAMPROFILE                 StatTimerStopR3;
    /** @} */
    /** VirtualSync - Running and Catching Up
     * @{ */
    STAMCOUNTER                 StatVirtualSyncRun;
    STAMCOUNTER                 StatVirtualSyncRunRestart;
    STAMPROFILE                 StatVirtualSyncRunSlack;
    STAMCOUNTER                 StatVirtualSyncRunStop;
    STAMCOUNTER                 StatVirtualSyncRunStoppedAlready;
    STAMCOUNTER                 StatVirtualSyncGiveUp;
    STAMCOUNTER                 StatVirtualSyncGiveUpBeforeStarting;
    STAMPROFILEADV              StatVirtualSyncCatchup;
    STAMCOUNTER                 aStatVirtualSyncCatchupInitial[TM_MAX_CATCHUP_PERIODS];
    STAMCOUNTER                 aStatVirtualSyncCatchupAdjust[TM_MAX_CATCHUP_PERIODS];
    /** @} */
    /** TMR3VirtualSyncFF (non dedicated EMT). */
    STAMPROFILE                 StatVirtualSyncFF;
    /** The timer callback. */
    STAMCOUNTER                 StatTimerCallbackSetFF;

    /** @name Reasons for refusing TSC offsetting in TMCpuTickCanUseRealTSC.
     * @{ */
    STAMCOUNTER                 StatTSCNotFixed;
    STAMCOUNTER                 StatTSCNotTicking;
    STAMCOUNTER                 StatTSCCatchupLE010;
    STAMCOUNTER                 StatTSCCatchupLE025;
    STAMCOUNTER                 StatTSCCatchupLE100;
    STAMCOUNTER                 StatTSCCatchupOther;
    STAMCOUNTER                 StatTSCWarp;
    STAMCOUNTER                 StatTSCSyncNotTicking;
    /** @} */
} TM;
/** Pointer to TM VM instance data. */
typedef TM *PTM;

/**
 * TM VMCPU Instance data.
 * Changes to this must checked against the padding of the tm union in VM!
 */
typedef struct TMCPU
{
    /** Offset to the VMCPU structure.
     * See TMCPU2VM(). */
    RTUINT                      offVMCPU;

    /** CPU timestamp ticking enabled indicator (bool). (RDTSC) */
    bool                        fTSCTicking;
    bool                        afAlignment0[3]; /**< alignment padding */

    /** The offset between the host TSC and the Guest TSC.
     * Only valid if fTicking is set and and fTSCUseRealTSC is clear. */
    uint64_t                    u64TSCOffset;

    /** The guest TSC when fTicking is cleared. */
    uint64_t                    u64TSC;

} TMCPU;
/** Pointer to TM VMCPU instance data. */
typedef TMCPU *PTMCPU;

#if 0 /* enable this to rule out locking bugs on single cpu guests. */
# define tmLock(pVM)                VINF_SUCCESS
# define tmTryLock(pVM)             VINF_SUCCESS
# define tmUnlock(pVM)              ((void)0)
# define tmVirtualSyncLock(pVM)     VINF_SUCCESS
# define tmVirtualSyncTryLock(pVM)  VINF_SUCCESS
# define tmVirtualSyncUnlock(pVM)   ((void)0)
# define TM_ASSERT_EMT_LOCK(pVM) VM_ASSERT_EMT(pVM)
#else
int                     tmLock(PVM pVM);
int                     tmTryLock(PVM pVM);
void                    tmUnlock(PVM pVM);
/** Checks that the caller owns the EMT lock.  */
#define TM_ASSERT_EMT_LOCK(pVM) Assert(PDMCritSectIsOwner(&pVM->tm.s.EmtLock))
int                     tmVirtualSyncLock(PVM pVM);
int                     tmVirtualSyncTryLock(PVM pVM);
void                    tmVirtualSyncUnlock(PVM pVM);
#endif

const char             *tmTimerState(TMTIMERSTATE enmState);
void                    tmTimerQueueSchedule(PVM pVM, PTMTIMERQUEUE pQueue);
#ifdef VBOX_STRICT
void                    tmTimerQueuesSanityChecks(PVM pVM, const char *pszWhere);
#endif

int                     tmCpuTickPause(PVM pVM, PVMCPU pVCpu);
int                     tmCpuTickResume(PVM pVM, PVMCPU pVCpu);

int                     tmVirtualPauseLocked(PVM pVM);
int                     tmVirtualResumeLocked(PVM pVM);
DECLEXPORT(void)        tmVirtualNanoTSBad(PRTTIMENANOTSDATA pData, uint64_t u64NanoTS, uint64_t u64DeltaPrev, uint64_t u64PrevNanoTS);
DECLEXPORT(uint64_t)    tmVirtualNanoTSRediscover(PRTTIMENANOTSDATA pData);


/** @} */

__END_DECLS

#endif

