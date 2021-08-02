/* $Id$ */
/** @file
 * PDM - Write-Only Critical Section, All Contexts.
 */

/*
 * Copyright (C) 2006-2020 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM_CRITSECT
#include "PDMInternal.h"
#include <VBox/vmm/pdmcritsect.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/vmm.h>
#include <VBox/vmm/vmcc.h>
#include <VBox/err.h>
#include <VBox/vmm/hm.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/lockvalidator.h>
# include <iprt/semaphore.h>
#endif
#ifdef IN_RING0
# include <iprt/time.h>
#endif
#if defined(IN_RING3) || defined(IN_RING0)
# include <iprt/thread.h>
#endif


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** The number loops to spin for in ring-3. */
#define PDMCRITSECT_SPIN_COUNT_R3       20
/** The number loops to spin for in ring-0. */
#define PDMCRITSECT_SPIN_COUNT_R0       256
/** The number loops to spin for in the raw-mode context. */
#define PDMCRITSECT_SPIN_COUNT_RC       256


/** Skips some of the overly paranoid atomic updates.
 * Makes some assumptions about cache coherence, though not brave enough not to
 * always end with an atomic update. */
#define PDMCRITSECT_WITH_LESS_ATOMIC_STUFF

/* Undefine the automatic VBOX_STRICT API mappings. */
#undef PDMCritSectEnter
#undef PDMCritSectTryEnter


/**
 * Gets the ring-3 native thread handle of the calling thread.
 *
 * @returns native thread handle (ring-3).
 * @param   pVM         The cross context VM structure.
 * @param   pCritSect   The critical section. This is used in R0 and RC.
 */
DECL_FORCE_INLINE(RTNATIVETHREAD) pdmCritSectGetNativeSelf(PVMCC pVM, PCPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
    RT_NOREF(pVM, pCritSect);
    RTNATIVETHREAD  hNativeSelf = RTThreadNativeSelf();
#else
    AssertMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC, ("%RX32\n", pCritSect->s.Core.u32Magic),
                    NIL_RTNATIVETHREAD);
    PVMCPUCC        pVCpu       = VMMGetCpu(pVM);                                       AssertPtr(pVCpu);
    RTNATIVETHREAD  hNativeSelf = pVCpu ? pVCpu->hNativeThread : NIL_RTNATIVETHREAD;    Assert(hNativeSelf != NIL_RTNATIVETHREAD);
#endif
    return hNativeSelf;
}


/**
 * Tail code called when we've won the battle for the lock.
 *
 * @returns VINF_SUCCESS.
 *
 * @param   pCritSect       The critical section.
 * @param   hNativeSelf     The native handle of this thread.
 * @param   pSrcPos         The source position of the lock operation.
 */
DECL_FORCE_INLINE(int) pdmCritSectEnterFirst(PPDMCRITSECT pCritSect, RTNATIVETHREAD hNativeSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertMsg(pCritSect->s.Core.NativeThreadOwner == NIL_RTNATIVETHREAD, ("NativeThreadOwner=%p\n", pCritSect->s.Core.NativeThreadOwner));
    Assert(!(pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK));

# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
    pCritSect->s.Core.cNestings = 1;
# else
    ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 1);
# endif
    Assert(pCritSect->s.Core.cNestings == 1);
    ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, hNativeSelf);

# ifdef PDMCRITSECT_STRICT
    RTLockValidatorRecExclSetOwner(pCritSect->s.Core.pValidatorRec, NIL_RTTHREAD, pSrcPos, true);
# else
    NOREF(pSrcPos);
# endif

    STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l);
    return VINF_SUCCESS;
}


#if defined(IN_RING3) || defined(IN_RING0)
/**
 * Deals with the contended case in ring-3 and ring-0.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_DESTROYED if destroyed.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pVCpu       The cross context virtual CPU structure if ring-0 and on
 *                      an EMT, otherwise NULL.
 * @param   pCritSect   The critsect.
 * @param   hNativeSelf The native thread handle.
 * @param   pSrcPos     The source position of the lock operation.
 * @param   rcBusy      The status code to return when we're in RC or R0
 */
static int pdmR3R0CritSectEnterContended(PVMCC pVM, PVMCPU pVCpu, PPDMCRITSECT pCritSect, RTNATIVETHREAD hNativeSelf,
                                         PCRTLOCKVALSRCPOS pSrcPos, int rcBusy)
{
    /*
     * Start waiting.
     */
    if (ASMAtomicIncS32(&pCritSect->s.Core.cLockers) == 0)
        return pdmCritSectEnterFirst(pCritSect, hNativeSelf, pSrcPos);
# ifdef IN_RING3
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionR3);
# else
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZLock);
# endif

    /*
     * The wait loop.
     *
     * This handles VERR_TIMEOUT and VERR_INTERRUPTED.
     */
    STAM_REL_PROFILE_START(&pCritSect->s.StatWait, a);
    PSUPDRVSESSION const    pSession    = pVM->pSession;
    SUPSEMEVENT const       hEvent      = (SUPSEMEVENT)pCritSect->s.Core.EventSem;
# ifdef IN_RING3
#  ifdef PDMCRITSECT_STRICT
    RTTHREAD const          hThreadSelf = RTThreadSelfAutoAdopt();
    int rc2 = RTLockValidatorRecExclCheckOrder(pCritSect->s.Core.pValidatorRec, hThreadSelf, pSrcPos, RT_INDEFINITE_WAIT);
    if (RT_FAILURE(rc2))
        return rc2;
#  else
    RTTHREAD const          hThreadSelf = RTThreadSelf();
#  endif
# else /* IN_RING0 */
    uint64_t const          tsStart     = RTTimeNanoTS();
    uint64_t                cNsMaxTotal = RT_NS_5MIN;
    uint64_t const          cNsMaxRetry = RT_NS_15SEC;
    uint32_t                cMsMaxOne   = RT_MS_5SEC;
    bool                    fNonInterruptible = false;
# endif
    for (;;)
    {
        /*
         * Do the wait.
         *
         * In ring-3 this gets cluttered by lock validation and thread state
         * maintainence.
         *
         * In ring-0 we have to deal with the possibility that the thread has
         * been signalled and the interruptible wait function returning
         * immediately.  In that case we do normal R0/RC rcBusy handling.
         *
         * We always do a timed wait here, so the event handle is revalidated
         * regularly and we won't end up stuck waiting for a destroyed critsect.
         */
        /** @todo Make SUPSemEventClose wake up all waiters. */
# ifdef IN_RING3
#  ifdef PDMCRITSECT_STRICT
        int rc9 = RTLockValidatorRecExclCheckBlocking(pCritSect->s.Core.pValidatorRec, hThreadSelf, pSrcPos,
                                                      !(pCritSect->s.Core.fFlags & RTCRITSECT_FLAGS_NO_NESTING),
                                                      RT_INDEFINITE_WAIT, RTTHREADSTATE_CRITSECT, true);
        if (RT_FAILURE(rc9))
            return rc9;
#  else
        RTThreadBlocking(hThreadSelf, RTTHREADSTATE_CRITSECT, true);
#  endif
        int const rc = SUPSemEventWaitNoResume(pSession, hEvent, RT_MS_5SEC);
        RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_CRITSECT);
# else  /* IN_RING0 */
        int const rc = !fNonInterruptible
                     ? SUPSemEventWaitNoResume(pSession, hEvent, cMsMaxOne)
                     : SUPSemEventWait(pSession, hEvent, cMsMaxOne);
# endif /* IN_RING0 */

        /*
         * Make sure the critical section hasn't been delete before continuing.
         */
        if (RT_LIKELY(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC))
        { /* likely */ }
        else
        {
            LogRel(("PDMCritSectEnter: Destroyed while waiting; pCritSect=%p rc=%Rrc\n", pCritSect, rc));
            return VERR_SEM_DESTROYED;
        }

        /*
         * Most likely we're here because we got signalled.
         */
        if (rc == VINF_SUCCESS)
        {
            STAM_REL_PROFILE_STOP(&pCritSect->s.StatContentionWait, a);
            return pdmCritSectEnterFirst(pCritSect, hNativeSelf, pSrcPos);
        }

        /*
         * Timeout and interrupted waits needs careful handling in ring-0
         * because we're cooperating with ring-3 on this critical section
         * and thus need to make absolutely sure we won't get stuck here.
         *
         * The r0 interrupted case means something is pending (termination,
         * signal, APC, debugger, whatever), so we must try our best to
         * return to the caller and to ring-3 so it can be dealt with.
         */
        if (RT_LIKELY(rc == VINF_TIMEOUT || rc == VERR_INTERRUPTED))
        {
# ifdef IN_RING0
            uint64_t const cNsElapsed = RTTimeNanoTS() - tsStart;
            int const      rcTerm     = RTThreadQueryTerminationStatus(NIL_RTTHREAD);
            AssertMsg(rcTerm == VINF_SUCCESS || rcTerm == VERR_NOT_SUPPORTED || rcTerm == VINF_THREAD_IS_TERMINATING,
                      ("rcTerm=%Rrc\n", rcTerm));
            if (rcTerm == VERR_NOT_SUPPORTED)
                cNsMaxTotal = RT_NS_1MIN;

            if (rc == VERR_TIMEOUT)
            {
                /* Try return get out of here with a non-VINF_SUCCESS status if
                   the thread is terminating or if the timeout has been exceeded. */
                STAM_REL_COUNTER_INC(&pVM->pdm.s.StatCritSectVerrTimeout);
                if (   rcTerm != VINF_THREAD_IS_TERMINATING
                    && cNsElapsed <= cNsMaxTotal)
                    continue;
            }
            else
            {
                /* For interrupt cases, we must return if we can.  If rcBusy is VINF_SUCCESS,
                   we will try non-interruptible sleep for a while to help resolve the issue
                   w/o guru'ing. */
                STAM_REL_COUNTER_INC(&pVM->pdm.s.StatCritSectVerrInterrupted);
                if (   rcTerm != VINF_THREAD_IS_TERMINATING
                    && rcBusy == VINF_SUCCESS
                    && pVCpu != NULL
                    && cNsElapsed <= cNsMaxTotal)
                {
                    if (!fNonInterruptible)
                    {
                        STAM_REL_COUNTER_INC(&pVM->pdm.s.StatCritSectNonInterruptibleWaits);
                        fNonInterruptible   = true;
                        cMsMaxOne           = 32;
                        uint64_t cNsLeft = cNsMaxTotal - cNsElapsed;
                        if (cNsLeft > RT_NS_10SEC)
                            cNsMaxTotal = cNsElapsed + RT_NS_10SEC;
                    }
                    continue;
                }
            }

            /*
             * Let try get out of here.  We must very carefully undo the
             * cLockers increment we did using compare-and-exchange so that
             * we don't race the semaphore signalling in PDMCritSectLeave
             * and end up with spurious wakeups and two owners at once.
             */
            uint32_t cNoIntWaits = 0;
            uint32_t cCmpXchgs   = 0;
            int32_t  cLockers    = ASMAtomicReadS32(&pCritSect->s.Core.cLockers);
            for (;;)
            {
                if (pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC)
                {
                    if (cLockers > 0 && cCmpXchgs < _64M)
                    {
                        bool fRc = ASMAtomicCmpXchgExS32(&pCritSect->s.Core.cLockers, cLockers - 1, cLockers, &cLockers);
                        if (fRc)
                        {
                            LogFunc(("Aborting wait on %p (rc=%Rrc rcTerm=%Rrc cNsElapsed=%'RU64) -> %Rrc\n", pCritSect,
                                     rc, rcTerm, cNsElapsed, rcBusy != VINF_SUCCESS ? rcBusy : rc));
                            STAM_REL_COUNTER_INC(&pVM->pdm.s.StatAbortedCritSectEnters);
                            return rcBusy != VINF_SUCCESS ? rcBusy : rc;
                        }
                        cCmpXchgs++;
                        ASMNopPause();
                        continue;
                    }

                    if (cLockers == 0)
                    {
                        /*
                         * We are racing someone in PDMCritSectLeave.
                         *
                         * For the VERR_TIMEOUT case we'll just retry taking it the normal
                         * way for a while.  For VERR_INTERRUPTED we're in for more fun as
                         * the previous owner might not have signalled the semaphore yet,
                         * so we'll do a short non-interruptible wait instead and then guru.
                         */
                        if (   rc == VERR_TIMEOUT
                            && RTTimeNanoTS() - tsStart <= cNsMaxTotal + cNsMaxRetry)
                            break;

                        if (   rc == VERR_INTERRUPTED
                            && (   cNoIntWaits == 0
                                || RTTimeNanoTS() - (tsStart + cNsElapsed) < RT_NS_100MS))
                        {
                            int const rc2 = SUPSemEventWait(pSession, hEvent, 1 /*ms*/);
                            if (rc2 == VINF_SUCCESS)
                            {
                                STAM_REL_COUNTER_INC(&pVM->pdm.s.StatCritSectEntersWhileAborting);
                                STAM_REL_PROFILE_STOP(&pCritSect->s.StatContentionWait, a);
                                return pdmCritSectEnterFirst(pCritSect, hNativeSelf, pSrcPos);
                            }
                            cNoIntWaits++;
                            cLockers = ASMAtomicReadS32(&pCritSect->s.Core.cLockers);
                            continue;
                        }
                    }
                    else
                        LogFunc(("Critical section %p has a broken cLockers count. Aborting.\n", pCritSect));

                    /* Sabotage the critical section and return error to caller. */
                    ASMAtomicWriteU32(&pCritSect->s.Core.u32Magic, PDMCRITSECT_MAGIC_FAILED_ABORT);
                    LogRel(("PDMCritSectEnter: Failed to abort wait on pCritSect=%p (rc=%Rrc rcTerm=%Rrc)\n",
                            pCritSect, rc, rcTerm));
                    return VERR_PDM_CRITSECT_ABORT_FAILED;
                }
                LogRel(("PDMCritSectEnter: Destroyed while aborting wait; pCritSect=%p/%#x rc=%Rrc rcTerm=%Rrc\n",
                        pCritSect, pCritSect->s.Core.u32Magic, rc, rcTerm));
                return VERR_SEM_DESTROYED;
            }

            /* We get here if we timed out.  Just retry now that it
               appears someone left already. */
            Assert(rc == VINF_TIMEOUT);
            cMsMaxOne = 10 /*ms*/;

# else  /* IN_RING3 */
            RT_NOREF(pVM, pVCpu, rcBusy);
# endif /* IN_RING3 */
        }
        /*
         * Any other return code is fatal.
         */
        else
        {
            AssertMsgFailed(("rc=%Rrc\n", rc));
            return RT_FAILURE_NP(rc) ? rc : -rc;
        }
    }
    /* won't get here */
}
#endif /* IN_RING3 || IN_RING0 */


/**
 * Common worker for the debug and normal APIs.
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in RC/R0.
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in RC or R0
 * @param   pSrcPos             The source position of the lock operation.
 */
DECL_FORCE_INLINE(int) pdmCritSectEnter(PVMCC pVM, PPDMCRITSECT pCritSect, int rcBusy, PCRTLOCKVALSRCPOS pSrcPos)
{
    Assert(pCritSect->s.Core.cNestings < 8);  /* useful to catch incorrect locking */
    Assert(pCritSect->s.Core.cNestings >= 0);

    /*
     * If the critical section has already been destroyed, then inform the caller.
     */
    AssertMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC,
                    ("%p %RX32\n", pCritSect, pCritSect->s.Core.u32Magic),
                    VERR_SEM_DESTROYED);

    /*
     * See if we're lucky.
     */
    /* NOP ... */
    if (!(pCritSect->s.Core.fFlags & RTCRITSECT_FLAGS_NOP))
    { /* We're more likely to end up here with real critsects than a NOP one. */ }
    else
        return VINF_SUCCESS;

    RTNATIVETHREAD hNativeSelf = pdmCritSectGetNativeSelf(pVM, pCritSect);
    AssertReturn(hNativeSelf != NIL_RTNATIVETHREAD, VERR_VM_THREAD_NOT_EMT);
    /* ... not owned ... */
    if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, 0, -1))
        return pdmCritSectEnterFirst(pCritSect, hNativeSelf, pSrcPos);

    /* ... or nested. */
    if (pCritSect->s.Core.NativeThreadOwner == hNativeSelf)
    {
        Assert(pCritSect->s.Core.cNestings >= 1);
# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
        pCritSect->s.Core.cNestings += 1;
# else
        ASMAtomicIncS32(&pCritSect->s.Core.cNestings);
# endif
        ASMAtomicIncS32(&pCritSect->s.Core.cLockers);
        return VINF_SUCCESS;
    }

    /*
     * Spin for a bit without incrementing the counter.
     */
    /** @todo Move this to cfgm variables since it doesn't make sense to spin on UNI
     *        cpu systems. */
    int32_t cSpinsLeft = CTX_SUFF(PDMCRITSECT_SPIN_COUNT_);
    while (cSpinsLeft-- > 0)
    {
        if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, 0, -1))
            return pdmCritSectEnterFirst(pCritSect, hNativeSelf, pSrcPos);
        ASMNopPause();
        /** @todo Should use monitor/mwait on e.g. &cLockers here, possibly with a
           cli'ed pendingpreemption check up front using sti w/ instruction fusing
           for avoiding races. Hmm ... This is assuming the other party is actually
           executing code on another CPU ... which we could keep track of if we
           wanted. */
    }

#ifdef IN_RING3
    /*
     * Take the slow path.
     */
    NOREF(rcBusy);
    return pdmR3R0CritSectEnterContended(pVM, NULL, pCritSect, hNativeSelf, pSrcPos, rcBusy);

#elif defined(IN_RING0)
# if 0 /* new code */
    /*
     * In ring-0 context we have to take the special VT-x/AMD-V HM context into
     * account when waiting on contended locks.
     *
     * While we usually (it can be VINF_SUCCESS) have to option via the rcBusy
     * parameter of going to back to ring-3 and to re-start the work there, it's
     * almost always more efficient to try wait for the lock here.  The rcBusy
     * will be used if we encounter an VERR_INTERRUPTED situation though.
     *
     * We must never block if VMMRZCallRing3Disable is active.
     */
    PVMCPUCC pVCpu = VMMGetCpu(pVM);
    if (pVCpu)
    {
        VMMR0EMTBLOCKCTX Ctx;
        int rc = VMMR0EmtPrepareToBlock(pVCpu, rcBusy, __FUNCTION__, pCritSect, &Ctx);
        if (rc == VINF_SUCCESS)
        {
            Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));

            rc = pdmR3R0CritSectEnterContended(pVM, pVCpu, pCritSect, hNativeSelf, pSrcPos, rcBusy);

            VMMR0EmtResumeAfterBlocking(pVCpu, &Ctx);
        }
        else
            STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZLockBusy);
        return rc;
    }

    /* Non-EMT. */
    Assert(RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    return pdmR3R0CritSectEnterContended(pVM, NULL, pCritSect, hNativeSelf, pSrcPos, rcBusy);

# else /* old code: */
    /*
     * We preemption hasn't been disabled, we can block here in ring-0.
     */
    if (   RTThreadPreemptIsEnabled(NIL_RTTHREAD)
        && ASMIntAreEnabled())
        return pdmR3R0CritSectEnterContended(pVM, VMMGetCpu(pVM), pCritSect, hNativeSelf, pSrcPos, rcBusy);

    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZLock);

    /*
     * Call ring-3 to acquire the critical section?
     */
    if (rcBusy == VINF_SUCCESS)
    {
        PVMCPUCC pVCpu = VMMGetCpu(pVM);
        AssertReturn(pVCpu, VERR_PDM_CRITSECT_IPE);
        return VMMRZCallRing3(pVM, pVCpu, VMMCALLRING3_PDM_CRIT_SECT_ENTER, MMHyperCCToR3(pVM, pCritSect));
    }

    /*
     * Return busy.
     */
    LogFlow(("PDMCritSectEnter: locked => R3 (%Rrc)\n", rcBusy));
    return rcBusy;
# endif  /* old code */
#else
# error "Unsupported context"
#endif
}


/**
 * Enters a PDM critical section.
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in RC/R0.
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in RC or R0
 *                              and the section is busy.  Pass VINF_SUCCESS to
 *                              acquired the critical section thru a ring-3
 *                              call if necessary.
 *
 * @note    Even callers setting @a rcBusy to VINF_SUCCESS must either handle
 *          possible failures in ring-0 or apply
 *          PDM_CRITSECT_RELEASE_ASSERT_RC(),
 *          PDM_CRITSECT_RELEASE_ASSERT_RC_DEV(),
 *          PDM_CRITSECT_RELEASE_ASSERT_RC_DRV() or
 *          PDM_CRITSECT_RELEASE_ASSERT_RC_USB() to the return value of this
 *          function.
 */
VMMDECL(DECL_CHECK_RETURN_NOT_R3(int)) PDMCritSectEnter(PVMCC pVM, PPDMCRITSECT pCritSect, int rcBusy)
{
#ifndef PDMCRITSECT_STRICT
    return pdmCritSectEnter(pVM, pCritSect, rcBusy, NULL);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return pdmCritSectEnter(pVM, pCritSect, rcBusy, &SrcPos);
#endif
}


/**
 * Enters a PDM critical section, with location information for debugging.
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in RC/R0.
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in RC or R0
 *                              and the section is busy.   Pass VINF_SUCCESS to
 *                              acquired the critical section thru a ring-3
 *                              call if necessary.
 * @param   uId                 Some kind of locking location ID.  Typically a
 *                              return address up the stack.  Optional (0).
 * @param   SRC_POS             The source position where to lock is being
 *                              acquired from.  Optional.
 */
VMMDECL(DECL_CHECK_RETURN_NOT_R3(int))
PDMCritSectEnterDebug(PVMCC pVM, PPDMCRITSECT pCritSect, int rcBusy, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
#ifdef PDMCRITSECT_STRICT
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return pdmCritSectEnter(pVM, pCritSect, rcBusy, &SrcPos);
#else
    NOREF(uId); RT_SRC_POS_NOREF();
    return pdmCritSectEnter(pVM, pCritSect, rcBusy, NULL);
#endif
}


/**
 * Common worker for the debug and normal APIs.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pCritSect   The critical section.
 * @param   pSrcPos     The source position of the lock operation.
 */
static int pdmCritSectTryEnter(PVMCC pVM, PPDMCRITSECT pCritSect, PCRTLOCKVALSRCPOS pSrcPos)
{
    /*
     * If the critical section has already been destroyed, then inform the caller.
     */
    AssertMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC,
                    ("%p %RX32\n", pCritSect, pCritSect->s.Core.u32Magic),
                    VERR_SEM_DESTROYED);

    /*
     * See if we're lucky.
     */
    /* NOP ... */
    if (!(pCritSect->s.Core.fFlags & RTCRITSECT_FLAGS_NOP))
    { /* We're more likely to end up here with real critsects than a NOP one. */ }
    else
        return VINF_SUCCESS;

    RTNATIVETHREAD hNativeSelf = pdmCritSectGetNativeSelf(pVM, pCritSect);
    AssertReturn(hNativeSelf != NIL_RTNATIVETHREAD, VERR_VM_THREAD_NOT_EMT);
    /* ... not owned ... */
    if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, 0, -1))
        return pdmCritSectEnterFirst(pCritSect, hNativeSelf, pSrcPos);

    /* ... or nested. */
    if (pCritSect->s.Core.NativeThreadOwner == hNativeSelf)
    {
        Assert(pCritSect->s.Core.cNestings >= 1);
# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
        pCritSect->s.Core.cNestings += 1;
# else
        ASMAtomicIncS32(&pCritSect->s.Core.cNestings);
# endif
        ASMAtomicIncS32(&pCritSect->s.Core.cLockers);
        return VINF_SUCCESS;
    }

    /* no spinning */

    /*
     * Return busy.
     */
#ifdef IN_RING3
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionR3);
#else
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZLockBusy);
#endif
    LogFlow(("PDMCritSectTryEnter: locked\n"));
    return VERR_SEM_BUSY;
}


/**
 * Try enter a critical section.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pCritSect   The critical section.
 */
VMMDECL(DECL_CHECK_RETURN(int)) PDMCritSectTryEnter(PVMCC pVM, PPDMCRITSECT pCritSect)
{
#ifndef PDMCRITSECT_STRICT
    return pdmCritSectTryEnter(pVM, pCritSect, NULL);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return pdmCritSectTryEnter(pVM, pCritSect, &SrcPos);
#endif
}


/**
 * Try enter a critical section, with location information for debugging.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCritSect           The critical section.
 * @param   uId                 Some kind of locking location ID.  Typically a
 *                              return address up the stack.  Optional (0).
 * @param   SRC_POS             The source position where to lock is being
 *                              acquired from.  Optional.
 */
VMMDECL(DECL_CHECK_RETURN(int))
PDMCritSectTryEnterDebug(PVMCC pVM, PPDMCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
#ifdef PDMCRITSECT_STRICT
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return pdmCritSectTryEnter(pVM, pCritSect, &SrcPos);
#else
    NOREF(uId); RT_SRC_POS_NOREF();
    return pdmCritSectTryEnter(pVM, pCritSect, NULL);
#endif
}


#ifdef IN_RING3
/**
 * Enters a PDM critical section.
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in GC/R0.
 * @retval  VERR_SEM_DESTROYED if the critical section is delete before or
 *          during the operation.
 *
 * @param   pVM                 The cross context VM structure.
 * @param   pCritSect           The PDM critical section to enter.
 * @param   fCallRing3          Whether this is a VMMRZCallRing3()request.
 */
VMMR3DECL(int) PDMR3CritSectEnterEx(PVM pVM, PPDMCRITSECT pCritSect, bool fCallRing3)
{
    int rc = PDMCritSectEnter(pVM, pCritSect, VERR_IGNORED);
    if (    rc == VINF_SUCCESS
        &&  fCallRing3
        &&  pCritSect->s.Core.pValidatorRec
        &&  pCritSect->s.Core.pValidatorRec->hThread != NIL_RTTHREAD)
        RTLockValidatorRecExclReleaseOwnerUnchecked(pCritSect->s.Core.pValidatorRec);
    return rc;
}
#endif /* IN_RING3 */


/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @returns Indication whether we really exited the critical section.
 * @retval  VINF_SUCCESS if we really exited.
 * @retval  VINF_SEM_NESTED if we only reduced the nesting count.
 * @retval  VERR_NOT_OWNER if you somehow ignore release assertions.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pCritSect   The PDM critical section to leave.
 */
VMMDECL(int) PDMCritSectLeave(PVMCC pVM, PPDMCRITSECT pCritSect)
{
    AssertMsg(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC, ("%p %RX32\n", pCritSect, pCritSect->s.Core.u32Magic));
    Assert(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC);

    /* Check for NOP sections before asserting ownership. */
    if (!(pCritSect->s.Core.fFlags & RTCRITSECT_FLAGS_NOP))
    { /* We're more likely to end up here with real critsects than a NOP one. */ }
    else
        return VINF_SUCCESS;

    /*
     * Always check that the caller is the owner (screw performance).
     */
    RTNATIVETHREAD const hNativeSelf = pdmCritSectGetNativeSelf(pVM, pCritSect);
    AssertReleaseMsgReturn(pCritSect->s.Core.NativeThreadOwner == hNativeSelf || hNativeSelf == NIL_RTNATIVETHREAD,
                           ("%p %s: %p != %p; cLockers=%d cNestings=%d\n", pCritSect, R3STRING(pCritSect->s.pszName),
                            pCritSect->s.Core.NativeThreadOwner, hNativeSelf,
                            pCritSect->s.Core.cLockers, pCritSect->s.Core.cNestings),
                           VERR_NOT_OWNER);

    /*
     * Nested leave.
     */
    int32_t const cNestings = pCritSect->s.Core.cNestings;
    Assert(cNestings >= 1);
    if (cNestings > 1)
    {
# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
        pCritSect->s.Core.cNestings = cNestings - 1;
# else
        ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, cNestings - 1);
# endif
        ASMAtomicDecS32(&pCritSect->s.Core.cLockers);
        Assert(pCritSect->s.Core.cLockers >= 0);
        return VINF_SEM_NESTED;
    }

#ifdef IN_RING0
# if 0 /** @todo Make SUPSemEventSignal interrupt safe (handle table++) and enable this for: defined(RT_OS_LINUX) || defined(RT_OS_OS2) */
    if (1) /* SUPSemEventSignal is safe */
# else
    if (ASMIntAreEnabled())
# endif
#endif
#if defined(IN_RING3) || defined(IN_RING0)
    {
        /*
         * Leave for real.
         */
        /* update members. */
        SUPSEMEVENT hEventToSignal  = pCritSect->s.hEventToSignal;
        pCritSect->s.hEventToSignal = NIL_SUPSEMEVENT;
# ifdef IN_RING3
#  if defined(PDMCRITSECT_STRICT)
        if (pCritSect->s.Core.pValidatorRec->hThread != NIL_RTTHREAD)
            RTLockValidatorRecExclReleaseOwnerUnchecked(pCritSect->s.Core.pValidatorRec);
#  endif
        Assert(!pCritSect->s.Core.pValidatorRec || pCritSect->s.Core.pValidatorRec->hThread == NIL_RTTHREAD);
# endif
# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
        //pCritSect->s.Core.cNestings = 0; /* not really needed */
        pCritSect->s.Core.NativeThreadOwner = NIL_RTNATIVETHREAD;
# else
        ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 0);
        ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, NIL_RTNATIVETHREAD);
# endif
        ASMAtomicAndU32(&pCritSect->s.Core.fFlags, ~PDMCRITSECT_FLAGS_PENDING_UNLOCK);

        /* stop and decrement lockers. */
        STAM_PROFILE_ADV_STOP(&pCritSect->s.StatLocked, l);
        ASMCompilerBarrier();
        if (ASMAtomicDecS32(&pCritSect->s.Core.cLockers) < 0)
        { /* hopefully likely */ }
        else
        {
            /* Someone is waiting, wake up one of them. */
            SUPSEMEVENT     hEvent   = (SUPSEMEVENT)pCritSect->s.Core.EventSem;
            PSUPDRVSESSION  pSession = pVM->pSession;
            int rc = SUPSemEventSignal(pSession, hEvent);
            AssertRC(rc);
        }

        /* Signal exit event. */
        if (RT_LIKELY(hEventToSignal == NIL_SUPSEMEVENT))
        { /* likely */ }
        else
        {
            Log8(("Signalling %#p\n", hEventToSignal));
            int rc = SUPSemEventSignal(pVM->pSession, hEventToSignal);
            AssertRC(rc);
        }

# if defined(DEBUG_bird) && defined(IN_RING0)
        VMMTrashVolatileXMMRegs();
# endif
    }
#endif  /* IN_RING3 || IN_RING0 */
#ifdef IN_RING0
    else
#endif
#if defined(IN_RING0) || defined(IN_RC)
    {
        /*
         * Try leave it.
         */
        if (pCritSect->s.Core.cLockers == 0)
        {
# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
            //pCritSect->s.Core.cNestings = 0; /* not really needed */
# else
            ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 0);
# endif
            RTNATIVETHREAD hNativeThread = pCritSect->s.Core.NativeThreadOwner;
            ASMAtomicAndU32(&pCritSect->s.Core.fFlags, ~PDMCRITSECT_FLAGS_PENDING_UNLOCK);
            STAM_PROFILE_ADV_STOP(&pCritSect->s.StatLocked, l);

            ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, NIL_RTNATIVETHREAD);
            if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, -1, 0))
                return VINF_SUCCESS;

            /* darn, someone raced in on us. */
            ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, hNativeThread);
            STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l);
# ifdef PDMCRITSECT_WITH_LESS_ATOMIC_STUFF
            //pCritSect->s.Core.cNestings = 1;
            Assert(pCritSect->s.Core.cNestings == 1);
# else
            //Assert(pCritSect->s.Core.cNestings == 0);
            ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 1);
# endif
        }
        ASMAtomicOrU32(&pCritSect->s.Core.fFlags, PDMCRITSECT_FLAGS_PENDING_UNLOCK);

        /*
         * Queue the request.
         */
        PVMCPUCC    pVCpu = VMMGetCpu(pVM);                 AssertPtr(pVCpu);
        uint32_t    i     = pVCpu->pdm.s.cQueuedCritSectLeaves++;
        LogFlow(("PDMCritSectLeave: [%d]=%p => R3\n", i, pCritSect));
        AssertFatal(i < RT_ELEMENTS(pVCpu->pdm.s.apQueuedCritSectLeaves));
        pVCpu->pdm.s.apQueuedCritSectLeaves[i] = MMHyperCCToR3(pVM, pCritSect);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PDM_CRITSECT);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_TO_R3);
        STAM_REL_COUNTER_INC(&pVM->pdm.s.StatQueuedCritSectLeaves);
        STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZUnlock);
    }
#endif /* IN_RING0 || IN_RC */

    return VINF_SUCCESS;
}


#if defined(IN_RING0) || defined(IN_RING3)
/**
 * Schedule a event semaphore for signalling upon critsect exit.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_TOO_MANY_SEMAPHORES if an event was already scheduled.
 * @returns VERR_NOT_OWNER if we're not the critsect owner (ring-3 only).
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 *
 * @param   pCritSect       The critical section.
 * @param   hEventToSignal  The support driver event semaphore that should be
 *                          signalled.
 */
VMMDECL(int) PDMHCCritSectScheduleExitEvent(PPDMCRITSECT pCritSect, SUPSEMEVENT hEventToSignal)
{
    AssertPtr(pCritSect);
    Assert(!(pCritSect->s.Core.fFlags & RTCRITSECT_FLAGS_NOP));
    Assert(hEventToSignal != NIL_SUPSEMEVENT);
# ifdef IN_RING3
    if (RT_UNLIKELY(!RTCritSectIsOwner(&pCritSect->s.Core)))
        return VERR_NOT_OWNER;
# endif
    if (RT_LIKELY(   pCritSect->s.hEventToSignal == NIL_RTSEMEVENT
                  || pCritSect->s.hEventToSignal == hEventToSignal))
    {
        pCritSect->s.hEventToSignal = hEventToSignal;
        return VINF_SUCCESS;
    }
    return VERR_TOO_MANY_SEMAPHORES;
}
#endif /* IN_RING0 || IN_RING3 */


/**
 * Checks the caller is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pVM         The cross context VM structure.
 * @param   pCritSect   The critical section.
 */
VMMDECL(bool) PDMCritSectIsOwner(PVMCC pVM, PCPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
    RT_NOREF(pVM);
    return RTCritSectIsOwner(&pCritSect->s.Core);
#else
    PVMCPUCC pVCpu = VMMGetCpu(pVM);
    if (   !pVCpu
        || pCritSect->s.Core.NativeThreadOwner != pVCpu->hNativeThread)
        return false;
    return (pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK) == 0
        || pCritSect->s.Core.cNestings > 1;
#endif
}


/**
 * Checks the specified VCPU is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pVCpu       The cross context virtual CPU structure.
 * @param   pCritSect   The critical section.
 */
VMMDECL(bool) PDMCritSectIsOwnerEx(PVMCPUCC pVCpu, PCPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
    NOREF(pVCpu);
    return RTCritSectIsOwner(&pCritSect->s.Core);
#else
    Assert(VMCC_GET_CPU(pVCpu->CTX_SUFF(pVM), pVCpu->idCpu) == pVCpu);
    if (pCritSect->s.Core.NativeThreadOwner != pVCpu->hNativeThread)
        return false;
    return (pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK) == 0
        || pCritSect->s.Core.cNestings > 1;
#endif
}


/**
 * Checks if anyone is waiting on the critical section we own.
 *
 * @returns true if someone is waiting.
 * @returns false if no one is waiting.
 * @param   pVM         The cross context VM structure.
 * @param   pCritSect   The critical section.
 */
VMMDECL(bool) PDMCritSectHasWaiters(PVMCC pVM, PCPDMCRITSECT pCritSect)
{
    AssertReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC, false);
    Assert(pCritSect->s.Core.NativeThreadOwner == pdmCritSectGetNativeSelf(pVM, pCritSect)); RT_NOREF(pVM);
    return pCritSect->s.Core.cLockers >= pCritSect->s.Core.cNestings;
}


/**
 * Checks if a critical section is initialized or not.
 *
 * @returns true if initialized.
 * @returns false if not initialized.
 * @param   pCritSect   The critical section.
 */
VMMDECL(bool) PDMCritSectIsInitialized(PCPDMCRITSECT pCritSect)
{
    return RTCritSectIsInitialized(&pCritSect->s.Core);
}


/**
 * Gets the recursion depth.
 *
 * @returns The recursion depth.
 * @param   pCritSect   The critical section.
 */
VMMDECL(uint32_t) PDMCritSectGetRecursion(PCPDMCRITSECT pCritSect)
{
    return RTCritSectGetRecursion(&pCritSect->s.Core);
}

