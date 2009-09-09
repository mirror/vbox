/* $Id$ */
/** @file
 * PDM - Critical Sections, All Contexts.
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_PDM//_CRITSECT
#include "PDMInternal.h"
#include <VBox/pdmcritsect.h>
#include <VBox/mm.h>
#include <VBox/vmm.h>
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/hwaccm.h>

#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#ifdef IN_RING3
# include <iprt/semaphore.h>
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The number loops to spin for in ring-3. */
#define PDMCRITSECT_SPIN_COUNT_R3       20
/** The number loops to spin for in ring-0. */
#define PDMCRITSECT_SPIN_COUNT_R0       256
/** The number loops to spin for in the raw-mode context. */
#define PDMCRITSECT_SPIN_COUNT_RC       256

/** @def PDMCRITSECT_STRICT
 * Enables/disables PDM critsect strictness like deadlock detection. */
#if defined(VBOX_STRICT) || defined(DOXYGEN_RUNNING)
# define PDMCRITSECT_STRICT
#endif


/**
 * Gets the ring-3 native thread handle of the calling thread.
 *
 * @returns native thread handle (ring-3).
 * @param   pCritSect           The critical section. This is used in R0 and RC.
 */
DECL_FORCE_INLINE(RTNATIVETHREAD) pdmCritSectGetNativeSelf(PPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
    NOREF(pCritSect);
    RTNATIVETHREAD  hNativeSelf = RTThreadNativeSelf();
#else
    AssertMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC, ("%RX32\n", pCritSect->s.Core.u32Magic),
                    VERR_SEM_DESTROYED);
    PVM             pVM         = pCritSect->s.CTX_SUFF(pVM); AssertPtr(pVM);
    PVMCPU          pVCpu       = VMMGetCpu(pVM);             AssertPtr(pVCpu);
    RTNATIVETHREAD  hNativeSelf = pVCpu->hNativeThread;       Assert(hNativeSelf != NIL_RTNATIVETHREAD);
#endif
    return hNativeSelf;
}


/**
 * Tail code called when we've wont the battle for the lock.
 *
 * @returns VINF_SUCCESS.
 *
 * @param   pCritSect       The critical section.
 * @param   hNativeSelf     The native handle of this thread.
 */
DECL_FORCE_INLINE(int) pdmCritSectEnterFirst(PPDMCRITSECT pCritSect, RTNATIVETHREAD hNativeSelf)
{
    AssertMsg(pCritSect->s.Core.NativeThreadOwner == NIL_RTNATIVETHREAD, ("NativeThreadOwner=%p\n", pCritSect->s.Core.NativeThreadOwner));
    Assert(!(pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK));

    ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 1);
    ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, hNativeSelf);

# if defined(PDMCRITSECT_STRICT) && defined(IN_RING3)
    pCritSect->s.Core.Strict.pszEnterFile = NULL;
    pCritSect->s.Core.Strict.u32EnterLine = 0;
    pCritSect->s.Core.Strict.uEnterId     = 0;
    RTTHREAD hSelf = RTThreadSelf();
    ASMAtomicWriteHandle(&pCritSect->s.Core.Strict.ThreadOwner, hSelf);
    RTThreadWriteLockInc(hSelf);
# endif

    STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l);
    return VINF_SUCCESS;
}


#ifdef IN_RING3
/**
 * Deals with the contended case in ring-3.
 *
 * @returns VINF_SUCCESS or VERR_SEM_DESTROYED.
 * @param   pCritSect           The critsect.
 * @param   hNativeSelf         The native thread handle.
 */
static int pdmR3CritSectEnterContended(PPDMCRITSECT pCritSect, RTNATIVETHREAD hNativeSelf)
{
    /*
     * Start waiting.
     */
    if (ASMAtomicIncS32(&pCritSect->s.Core.cLockers) == 0)
        return pdmCritSectEnterFirst(pCritSect, hNativeSelf);
    STAM_COUNTER_INC(&pCritSect->s.StatContentionR3);

    /*
     * The wait loop.
     */
    PSUPDRVSESSION  pSession = pCritSect->s.CTX_SUFF(pVM)->pSession;
    SUPSEMEVENT     hEvent   = (SUPSEMEVENT)pCritSect->s.Core.EventSem;
# ifdef PDMCRITSECT_STRICT
    RTTHREAD        hSelf    = RTThreadSelf();
    if (hSelf == NIL_RTTHREAD)
        RTThreadAdopt(RTTHREADTYPE_DEFAULT, 0, NULL, &hSelf);
# endif
    for (;;)
    {
# ifdef PDMCRITSECT_STRICT
        RTThreadBlocking(hSelf, RTTHREADSTATE_CRITSECT, (uintptr_t)pCritSect, NULL, 0, 0);
# endif
        int rc = SUPSemEventWaitNoResume(pSession, hEvent, RT_INDEFINITE_WAIT);
# ifdef PDMCRITSECT_STRICT
        RTThreadUnblocked(hSelf, RTTHREADSTATE_CRITSECT);
# endif
        if (RT_UNLIKELY(pCritSect->s.Core.u32Magic != RTCRITSECT_MAGIC))
            return VERR_SEM_DESTROYED;
        if (rc == VINF_SUCCESS)
            return pdmCritSectEnterFirst(pCritSect, hNativeSelf);
        AssertMsg(rc == VERR_INTERRUPTED, ("rc=%Rrc\n", rc));
    }
    /* won't get here */
}
#endif /* IN_RING3 */


/**
 * Enters a PDM critical section.
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in GC/R0.
 * @returns VERR_SEM_DESTROYED if the critical section is dead.
 *
 * @param   pCritSect           The PDM critical section to enter.
 * @param   rcBusy              The status code to return when we're in GC or R0
 *                              and the section is busy.
 */
VMMDECL(int) PDMCritSectEnter(PPDMCRITSECT pCritSect, int rcBusy)
{
    Assert(pCritSect->s.Core.cNestings < 8);  /* useful to catch incorrect locking */

    /*
     * If the critical section has already been destroyed, then inform the caller.
     */
    AssertMsgReturn(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC,
                    ("%p %RX32\n", pCritSect, pCritSect->s.Core.u32Magic),
                    VERR_SEM_DESTROYED);

    /*
     * See if we're lucky.
     */
    RTNATIVETHREAD hNativeSelf = pdmCritSectGetNativeSelf(pCritSect);
    /* Not owned ... */
    if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, 0, -1))
        return pdmCritSectEnterFirst(pCritSect, hNativeSelf);

    /* ... or nested. */
    if (pCritSect->s.Core.NativeThreadOwner == hNativeSelf)
    {
        ASMAtomicIncS32(&pCritSect->s.Core.cLockers);
        ASMAtomicIncS32(&pCritSect->s.Core.cNestings);
        ASMAtomicAndU32(&pCritSect->s.Core.fFlags, ~PDMCRITSECT_FLAGS_PENDING_UNLOCK);
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
            return pdmCritSectEnterFirst(pCritSect, hNativeSelf);
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
    return pdmR3CritSectEnterContended(pCritSect, hNativeSelf);
#else
    /*
     * Return busy.
     */
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZLock);
    LogFlow(("PDMCritSectEnter: locked => R3 (%Rrc)\n", rcBusy));
    return rcBusy;
#endif
}


/**
 * Try enter a critical section.
 *
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_SEM_BUSY if the critsect was owned.
 * @retval  VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @retval  VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 *
 * @param   pCritSect   The critical section.
 */
VMMDECL(int) PDMCritSectTryEnter(PPDMCRITSECT pCritSect)
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
    RTNATIVETHREAD hNativeSelf = pdmCritSectGetNativeSelf(pCritSect);
    /* Not owned ... */
    if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, 0, -1))
        return pdmCritSectEnterFirst(pCritSect, hNativeSelf);

    /* ... or nested. */
    if (pCritSect->s.Core.NativeThreadOwner == hNativeSelf)
    {
        ASMAtomicIncS32(&pCritSect->s.Core.cLockers);
        ASMAtomicIncS32(&pCritSect->s.Core.cNestings);
        ASMAtomicAndU32(&pCritSect->s.Core.fFlags, ~PDMCRITSECT_FLAGS_PENDING_UNLOCK);
        return VINF_SUCCESS;
    }

    /* no spinning */

    /*
     * Return busy.
     */
#ifdef IN_RING3
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionR3);
#else
    STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZLock);
#endif
    LogFlow(("PDMCritSectTryEnter: locked\n"));
    return VERR_SEM_BUSY;
}


#ifdef IN_RING3
/**
 * Enters a PDM critical section.
 *
 * @returns VINF_SUCCESS if entered successfully.
 * @returns rcBusy when encountering a busy critical section in GC/R0.
 * @returns VERR_SEM_DESTROYED if the critical section is dead.
 *
 * @param   pCritSect           The PDM critical section to enter.
 * @param   fCallRing3          Whether this is a VMMRZCallRing3()request.
 */
VMMR3DECL(int) PDMR3CritSectEnterEx(PPDMCRITSECT pCritSect, bool fCallRing3)
{
    int rc = PDMCritSectEnter(pCritSect, VERR_INTERNAL_ERROR);
    if (    rc == VINF_SUCCESS
        &&  fCallRing3
        &&  pCritSect->s.Core.Strict.ThreadOwner != NIL_RTTHREAD)
    {
        RTThreadWriteLockDec(pCritSect->s.Core.Strict.ThreadOwner);
        ASMAtomicWriteHandle(&pCritSect->s.Core.Strict.ThreadOwner, NIL_RTTHREAD);
    }
    return rc;
}
#endif /* IN_RING3 */


/**
 * Leaves a critical section entered with PDMCritSectEnter().
 *
 * @param   pCritSect           The PDM critical section to leave.
 */
VMMDECL(void) PDMCritSectLeave(PPDMCRITSECT pCritSect)
{
    AssertMsg(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC, ("%p %RX32\n", pCritSect, pCritSect->s.Core.u32Magic));
    Assert(pCritSect->s.Core.u32Magic == RTCRITSECT_MAGIC);
    Assert(pCritSect->s.Core.NativeThreadOwner == pdmCritSectGetNativeSelf(pCritSect));
    Assert(pCritSect->s.Core.cNestings >= 1);

    /*
     * Nested leave.
     */
    if (pCritSect->s.Core.cNestings > 1)
    {
        ASMAtomicDecS32(&pCritSect->s.Core.cNestings);
        ASMAtomicDecS32(&pCritSect->s.Core.cLockers);
        return;
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
# ifdef IN_RING3
        RTSEMEVENT hEventToSignal    = pCritSect->s.EventToSignal;
        pCritSect->s.EventToSignal   = NIL_RTSEMEVENT;
#  if defined(PDMCRITSECT_STRICT)
        if (pCritSect->s.Core.Strict.ThreadOwner != NIL_RTTHREAD)
            RTThreadWriteLockDec(pCritSect->s.Core.Strict.ThreadOwner);
        ASMAtomicWriteHandle(&pCritSect->s.Core.Strict.ThreadOwner, NIL_RTTHREAD);
#  endif
# endif
        ASMAtomicAndU32(&pCritSect->s.Core.fFlags, ~PDMCRITSECT_FLAGS_PENDING_UNLOCK);
        Assert(pCritSect->s.Core.Strict.ThreadOwner == NIL_RTTHREAD);
        ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, NIL_RTNATIVETHREAD);
        ASMAtomicDecS32(&pCritSect->s.Core.cNestings);

        /* stop and decrement lockers. */
        STAM_PROFILE_ADV_STOP(&pCritSect->s.StatLocked, l);
        ASMCompilerBarrier();
        if (ASMAtomicDecS32(&pCritSect->s.Core.cLockers) >= 0)
        {
            /* Someone is waiting, wake up one of them. */
            SUPSEMEVENT     hEvent   = (SUPSEMEVENT)pCritSect->s.Core.EventSem;
            PSUPDRVSESSION  pSession = pCritSect->s.CTX_SUFF(pVM)->pSession;
            int rc = SUPSemEventSignal(pSession, hEvent);
            AssertRC(rc);
        }

# ifdef IN_RING3
        /* Signal exit event. */
        if (hEventToSignal != NIL_RTSEMEVENT)
        {
            LogBird(("Signalling %#x\n", hEventToSignal));
            int rc = RTSemEventSignal(hEventToSignal);
            AssertRC(rc);
        }
# endif

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
            ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 0);
            RTNATIVETHREAD hNativeThread = pCritSect->s.Core.NativeThreadOwner;
            ASMAtomicAndU32(&pCritSect->s.Core.fFlags, ~PDMCRITSECT_FLAGS_PENDING_UNLOCK);
            STAM_PROFILE_ADV_STOP(&pCritSect->s.StatLocked, l);

            ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, NIL_RTNATIVETHREAD);
            if (ASMAtomicCmpXchgS32(&pCritSect->s.Core.cLockers, -1, 0))
                return;

            /* darn, someone raced in on us. */
            ASMAtomicWriteHandle(&pCritSect->s.Core.NativeThreadOwner, hNativeThread);
            STAM_PROFILE_ADV_START(&pCritSect->s.StatLocked, l);
            ASMAtomicWriteS32(&pCritSect->s.Core.cNestings, 1);
        }
        ASMAtomicOrU32(&pCritSect->s.Core.fFlags, PDMCRITSECT_FLAGS_PENDING_UNLOCK);

        /*
         * Queue the request.
         */
        PVM         pVM   = pCritSect->s.CTX_SUFF(pVM);     AssertPtr(pVM);
        PVMCPU      pVCpu = VMMGetCpu(pVM);                 AssertPtr(pVCpu);
        uint32_t    i     = pVCpu->pdm.s.cQueuedCritSectLeaves++;
        LogFlow(("PDMCritSectLeave: [%d]=%p => R3\n", i, pCritSect));
        AssertFatal(i < RT_ELEMENTS(pVCpu->pdm.s.apQueuedCritSectsLeaves));
        pVCpu->pdm.s.apQueuedCritSectsLeaves[i] = MMHyperCCToR3(pVM, pCritSect);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_PDM_CRITSECT);
        VMCPU_FF_SET(pVCpu, VMCPU_FF_TO_R3);
        STAM_REL_COUNTER_INC(&pVM->pdm.s.StatQueuedCritSectLeaves);
        STAM_REL_COUNTER_INC(&pCritSect->s.StatContentionRZUnlock);
    }
#endif /* IN_RING0 || IN_RC */
}


#if defined(IN_RING3) || defined(IN_RING0)
/**
 * Process the critical sections queued for ring-3 'leave'.
 *
 * @param   pVCpu         The VMCPU handle.
 */
VMMDECL(void) PDMCritSectFF(PVMCPU pVCpu)
{
    Assert(pVCpu->pdm.s.cQueuedCritSectLeaves > 0);

    const RTUINT c = pVCpu->pdm.s.cQueuedCritSectLeaves;
    for (RTUINT i = 0; i < c; i++)
    {
# ifdef IN_RING3
        PPDMCRITSECT pCritSect = pVCpu->pdm.s.apQueuedCritSectsLeaves[i];
# else
        PPDMCRITSECT pCritSect = (PPDMCRITSECT)MMHyperR3ToCC(pVCpu->CTX_SUFF(pVM), pVCpu->pdm.s.apQueuedCritSectsLeaves[i]);
# endif

        PDMCritSectLeave(pCritSect);
        LogFlow(("PDMR3CritSectFF: %p\n", pCritSect));
    }

    pVCpu->pdm.s.cQueuedCritSectLeaves = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_PDM_CRITSECT);
}
#endif /* IN_RING3 || IN_RING0 */


/**
 * Checks the caller is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pCritSect   The critical section.
 */
VMMDECL(bool) PDMCritSectIsOwner(PCPDMCRITSECT pCritSect)
{
#ifdef IN_RING3
    return RTCritSectIsOwner(&pCritSect->s.Core);
#else
    PVM     pVM   = pCritSect->s.CTX_SUFF(pVM); AssertPtr(pVM);
    PVMCPU  pVCpu = VMMGetCpu(pVM);             AssertPtr(pVCpu);
    if (pCritSect->s.Core.NativeThreadOwner != pVCpu->hNativeThread)
        return false;
    return (pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK) == 0;
#endif
}


/**
 * Checks the specified VCPU is the owner of the critical section.
 *
 * @returns true if owner.
 * @returns false if not owner.
 * @param   pCritSect   The critical section.
 * @param   idCpu       VCPU id
 */
VMMDECL(bool) PDMCritSectIsOwnerEx(PCPDMCRITSECT pCritSect, VMCPUID idCpu)
{
#ifdef IN_RING3
    NOREF(idCpu);
    return RTCritSectIsOwner(&pCritSect->s.Core);
#else
    PVM pVM = pCritSect->s.CTX_SUFF(pVM);
    AssertPtr(pVM);
    Assert(idCpu < pVM->cCpus);
    return pCritSect->s.Core.NativeThreadOwner == pVM->aCpus[idCpu].hNativeThread
        && (pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK) == 0;
#endif
}


/**
 * Checks if somebody currently owns the critical section.
 *
 * @returns true if locked.
 * @returns false if not locked.
 *
 * @param   pCritSect   The critical section.
 *
 * @remarks This doesn't prove that no deadlocks will occur later on; it's
 *          just a debugging tool
 */
VMMDECL(bool) PDMCritSectIsOwned(PCPDMCRITSECT pCritSect)
{
    return pCritSect->s.Core.NativeThreadOwner != NIL_RTNATIVETHREAD
        && (pCritSect->s.Core.fFlags & PDMCRITSECT_FLAGS_PENDING_UNLOCK) == 0;
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
