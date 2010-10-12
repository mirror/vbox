/* $Id$ */
/** @file
 * IPRT - Multiple Release Event Semaphores, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"
#include "internal/iprt.h"
#include <iprt/semaphore.h>

#include <iprt/assert.h>
#include <iprt/asm.h>
#if defined(RT_ARCH_AMD64) || defined(RT_ARCH_X86)
# include <iprt/asm-amd64-x86.h>
#endif
#include <iprt/err.h>
#include <iprt/lockvalidator.h>
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/thread.h>
#include <iprt/time.h>
#include "internal/magics.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name fStateAndGen values
 * @{ */
/** The state bit number. */
#define RTSEMEVENTMULTISOL_STATE_BIT        0
/** The state mask. */
#define RTSEMEVENTMULTISOL_STATE_MASK       RT_BIT_32(RTSEMEVENTMULTISOL_STATE_BIT)
/** The generation mask. */
#define RTSEMEVENTMULTISOL_GEN_MASK         ~RTSEMEVENTMULTISOL_STATE_MASK
/** The generation shift. */
#define RTSEMEVENTMULTISOL_GEN_SHIFT        1
/** The initial variable value. */
#define RTSEMEVENTMULTISOL_STATE_GEN_INIT   UINT32_C(0xfffffffc)
/** @}  */


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Solaris multiple release event semaphore.
 */
typedef struct RTSEMEVENTMULTIINTERNAL
{
    /** Magic value (RTSEMEVENTMULTI_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The number of references. */
    uint32_t volatile   cRefs;
    /** The object state bit and generation counter.
     * The generation counter is incremented every time the object is
     * signalled. */
    uint32_t volatile   fStateAndGen;
    /** The Solaris mutex protecting this structure and pairing up the with the cv. */
    kmutex_t            Mtx;
    /** The Solaris condition variable. */
    kcondvar_t          Cnd;
} RTSEMEVENTMULTIINTERNAL, *PRTSEMEVENTMULTIINTERNAL;



RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI phEventMultiSem)
{
    return RTSemEventMultiCreateEx(phEventMultiSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventMultiCreateEx(PRTSEMEVENTMULTI phEventMultiSem, uint32_t fFlags, RTLOCKVALCLASS hClass,
                                     const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMEVENTMULTI_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phEventMultiSem, VERR_INVALID_POINTER);
    RT_ASSERT_PREEMPTIBLE();

    AssertCompile(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic     = RTSEMEVENTMULTI_MAGIC;
        pThis->cRefs        = 1;
        pThis->fStateAndGen = RTSEMEVENTMULTISOL_STATE_GEN_INIT;
        mutex_init(&pThis->Mtx, "IPRT Multiple Release Event Semaphore", MUTEX_DRIVER, (void *)ipltospl(DISP_LEVEL));
        cv_init(&pThis->Cnd, "IPRT CV", CV_DRIVER, NULL);

        *phEventMultiSem = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Retain a reference to the semaphore.
 *
 * @param   pThis       The semaphore.
 */
DECLINLINE(void) rtR0SemEventMultiSolRetain(PRTSEMEVENTMULTIINTERNAL pThis)
{
    uint32_t cRefs = ASMAtomicIncU32(&pThis->cRefs);
    Assert(cRefs && cRefs < 100000);
}


/**
 * Destructor that is called when cRefs == 0.
 *
 * @param   pThis       The instance to destroy.
 */
static void rtSemEventMultiDtor(PRTSEMEVENTMULTIINTERNAL pThis)
{
    Assert(pThis->u32Magic != RTSEMEVENTMULTI_MAGIC);
    cv_destroy(&pThis->Cnd);
    mutex_destroy(&pThis->Mtx);
    RTMemFree(pThis);
}


/**
 * Release a reference, destroy the thing if necessary.
 *
 * @param   pThis       The semaphore.
 */
DECLINLINE(void) rtR0SemEventMultiSolRelease(PRTSEMEVENTMULTIINTERNAL pThis)
{
    if (RT_UNLIKELY(ASMAtomicDecU32(&pThis->cRefs) == 0))
        rtSemEventMultiDtor(pThis);
}



RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (pThis == NIL_RTSEMEVENTMULTI)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->cRefs > 0, ("pThis=%p cRefs=%d\n", pThis, pThis->cRefs), VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();

    mutex_enter(&pThis->Mtx);

    /* Invalidate the handle and wake up all threads that might be waiting on the semaphore. */
    Assert(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC);
    ASMAtomicWriteU32(&pThis->u32Magic, RTSEMEVENTMULTI_MAGIC_DEAD);
    ASMAtomicAndU32(&pThis->fStateAndGen, RTSEMEVENTMULTISOL_GEN_MASK);
    cv_broadcast(&pThis->Cnd);

    /* Drop the reference from RTSemEventMultiCreateEx. */
    mutex_exit(&pThis->Mtx);
    rtR0SemEventMultiSolRelease(pThis);

    return VINF_SUCCESS;
}


RTDECL(int) RTSemEventMultiSignal(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    RT_ASSERT_PREEMPT_CPUID_VAR();

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();
    rtR0SemEventMultiSolRetain(pThis);

    /*
     * If we're in interrupt context we need to unpin the underlying current
     * thread as this could lead to a deadlock (see #4259 for the full explanation)
     *
     * Note! See remarks about preemption in RTSemEventSignal.
     */
    int fAcquired = mutex_tryenter(&pThis->Mtx);
    if (!fAcquired)
    {
        if (curthread->t_intr && getpil() < DISP_LEVEL)
        {
            RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
            RTThreadPreemptDisable(&PreemptState);
            preempt();
            RTThreadPreemptRestore(&PreemptState);
        }
        mutex_enter(&pThis->Mtx);
    }
    Assert(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC);

    /*
     * Do the job.
     */
    uint32_t fNew = ASMAtomicUoReadU32(&pThis->fStateAndGen);
    fNew += 1 << RTSEMEVENTMULTISOL_GEN_SHIFT;
    fNew |= RTSEMEVENTMULTISOL_STATE_MASK;
    ASMAtomicWriteU32(&pThis->fStateAndGen, fNew);

    cv_broadcast(&pThis->Cnd);

    mutex_exit(&pThis->Mtx);

    rtR0SemEventMultiSolRelease(pThis);
    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}


RTDECL(int) RTSemEventMultiReset(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    RT_ASSERT_PREEMPT_CPUID_VAR();

    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();
    rtR0SemEventMultiSolRetain(pThis);

    /*
     * If we're in interrupt context we need to unpin the underlying current
     * thread as this could lead to a deadlock (see #4259 for the full explanation)
     *
     * Note! See remarks about preemption in RTSemEventSignal.
     */
    int fAcquired = mutex_tryenter(&pThis->Mtx);
    if (!fAcquired)
    {
        if (curthread->t_intr && getpil() < DISP_LEVEL)
        {
            RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
            RTThreadPreemptDisable(&PreemptState);
            preempt();
            RTThreadPreemptRestore(&PreemptState);
        }
        mutex_enter(&pThis->Mtx);
    }
    Assert(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC);

    /*
     * Do the job (could be done without the lock, but play safe).
     */
    ASMAtomicAndU32(&pThis->fStateAndGen, ~RTSEMEVENTMULTISOL_STATE_MASK);

    mutex_exit(&pThis->Mtx);

    rtR0SemEventMultiSolRelease(pThis);
    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}

#if 0 /* NEW_STUFF - not working yet :-) */

typedef struct RTR0SEMSOLWAIT
{
    /** The absolute timeout given as nano seconds since the start of the
     *  monotonic clock. */
    uint64_t        uNsAbsTimeout;
    /** The timeout in nano seconds relative to the start of the wait. */
    uint64_t        cNsRelTimeout;
    /** The native timeout value. */
    union
    {
        /** The timeout (abs lbolt) when fHighRes is false.  */
        clock_t     lTimeout;
    } u;
    /** Set if we use high resolution timeouts. */
    bool            fHighRes;
    /** Set if it's an indefinite wait. */
    bool            fIndefinite;
    /** Set if we've already timed out.
     * Set by rtR0SemSolWaitDoIt or rtR0SemSolWaitHighResTimeout, read by
     * rtR0SemSolWaitHasTimedOut. */
    bool volatile   fTimedOut;
    /** Whether the wait was interrupted. */
    bool            fInterrupted;
    /** Interruptible or uninterruptible wait. */
    bool            fInterruptible;
    /** The thread to wake up. */
    kthread_t      *pThread;
} RTR0SEMSOLWAIT;
typedef RTR0SEMSOLWAIT *PRTR0SEMSOLWAIT;


/**
 * Initializes a wait.
 *
 * The caller MUST check the wait condition BEFORE calling this function or the
 * timeout logic will be flawed.
 *
 * @returns VINF_SUCCESS or VERR_TIMEOUT.
 * @param   pWait               The wait structure.
 * @param   fFlags              The wait flags.
 * @param   uTimeout            The timeout.
 * @param   pWaitQueue          The wait queue head.
 */
DECLINLINE(int) rtR0SemSolWaitInit(PRTR0SEMSOLWAIT pWait, uint32_t fFlags, uint64_t uTimeout)
{
    /*
     * Process the flags and timeout.
     */
    if (!(fFlags & RTSEMWAIT_FLAGS_INDEFINITE))
    {
        if (fFlags & RTSEMWAIT_FLAGS_MILLISECS)
            uTimeout = uTimeout < UINT64_MAX / UINT32_C(1000000) * UINT32_C(1000000)
                     ? uTimeout * UINT32_C(1000000)
                     : UINT64_MAX;
        if (uTimeout == UINT64_MAX)
            fFlags |= RTSEMWAIT_FLAGS_INDEFINITE;
        else
        {
            uint64_t u64Now;
            if (fFlags & RTSEMWAIT_FLAGS_RELATIVE)
            {
                if (uTimeout == 0)
                    return VERR_TIMEOUT;

                u64Now = RTTimeSystemNanoTS();
                pWait->cNsRelTimeout = uTimeout;
                pWait->uNsAbsTimeout = u64Now + uTimeout;
                if (pWait->uNsAbsTimeout < u64Now) /* overflow */
                    fFlags |= RTSEMWAIT_FLAGS_INDEFINITE;
            }
            else
            {
                u64Now = RTTimeSystemNanoTS();
                if (u64Now >= uTimeout)
                    return VERR_TIMEOUT;

                pWait->cNsRelTimeout = uTimeout - u64Now;
                pWait->uNsAbsTimeout = uTimeout;
            }
        }
    }

    if (!(fFlags & RTSEMWAIT_FLAGS_INDEFINITE))
    {
        pWait->fIndefinite      = false;
        if (   (fFlags & (RTSEMWAIT_FLAGS_NANOSECS | RTSEMWAIT_FLAGS_ABSOLUTE))
            || pWait->cNsRelTimeout < UINT32_C(1000000000) / 100 /*Hz*/ * 4)
            pWait->fHighRes     = true;
        else
        {
#if 1
            uint64_t cTicks     = NSEC_TO_TICK_ROUNDUP(uTimeout);
#else
            uint64_t cTicks     = drv_usectohz((clock_t)(uTimeout / 1000));
#endif
            if (cTicks >= LONG_MAX)
                fFlags |= RTSEMWAIT_FLAGS_INDEFINITE;
            else
            {
                pWait->u.lTimeout = ddi_get_lbolt() + cTicks;
                pWait->fHighRes = false;
            }
        }
    }

    if (fFlags & RTSEMWAIT_FLAGS_INDEFINITE)
    {
        pWait->fIndefinite      = true;
        pWait->fHighRes         = false;
        pWait->uNsAbsTimeout    = UINT64_MAX;
        pWait->cNsRelTimeout    = UINT64_MAX;
        pWait->u.lTimeout       = LONG_MAX;
    }

    pWait->fTimedOut        = false;
    pWait->fInterrupted     = false;
    pWait->fInterruptible   = !!(fFlags & RTSEMWAIT_FLAGS_INTERRUPTIBLE);
    pWait->pThread          = curthread;

    return VINF_SUCCESS;
}


/**
 * Cyclic timeout callback that sets the timeout indicator and wakes up the
 * waiting thread.
 *
 * @param   pvUser              The wait structure.
 */
static void rtR0SemSolWaitHighResTimeout(void *pvUser)
{
    PRTR0SEMSOLWAIT pWait   = (PRTR0SEMSOLWAIT)pvUser;
    kthread_t      *pThread = pWait->pThread;
    if (VALID_PTR(pThread)) /* paranoia */
    {
        ASMAtomicWriteBool(&pWait->fTimedOut, true);
        setrun(pThread);
    }
}


/**
 * Do the actual wait.
 *
 * @param   pWait               The wait structure.
 * @param   pCnd                The condition variable to wait on.
 * @param   pMtx                The mutex related to the condition variable.
 *                              The caller has entered this.
 */
DECLINLINE(void) rtR0SemSolWaitDoIt(PRTR0SEMSOLWAIT pWait, kcondvar_t *pCnd, kmutex_t *pMtx)
{
    int rc = 1;
    if (pWait->fIndefinite)
    {
        /*
         * No timeout - easy.
         */
        if (pWait->fInterruptible)
            rc = cv_wait_sig(pCnd, pMtx);
        else
            cv_wait(pCnd, pMtx);
    }
    else if (pWait->fHighRes)
    {
        /*
         * High resolution timeout - arm a one-shot cyclic for waking up
         * the thread at the desired time.
         */
        cyc_handler_t   Cyh;
        Cyh.cyh_arg      = pWait;
        Cyh.cyh_func     = rtR0SemSolWaitHighResTimeout;
        Cyh.cyh_level    = CY_LOW_LEVEL; /// @todo try CY_LOCK_LEVEL and CY_HIGH_LEVEL?

        cyc_time_t      Cyt;
        Cyt.cyt_when     = pWait->uNsAbsTimeout;
        Cyt.cyt_interval = 0;

        mutex_enter(&cpu_lock);
        cyclic_id_t     idCy = cyclic_add(&Cyh, &Cyt);
        mutex_exit(&cpu_lock);

        if (pWait->fInterruptible)
            rc = cv_wait_sig(pCnd, pMtx);
        else
            cv_wait(pCnd, pMtx);

        mutex_enter(&cpu_lock);
        cyclic_remove(idCy);
        mutex_exit(&cpu_lock);
    }
    else
    {
        /*
         * Normal timeout.
         */
        if (pWait->fInterruptible)
            rc = cv_timedwait_sig(pCnd, pMtx, pWait->u.lTimeout);
        else
            rc = cv_timedwait(pCnd, pMtx, pWait->u.lTimeout);
    }

    /* Above zero means normal wake-up. */
    if (rc > 0)
        return;

    /* Timeout is signalled by -1. */
    if (rc == -1)
        pWait->fTimedOut = true;
    /* Interruption is signalled by 0. */
    else
    {
        AssertMsg(rc == 0, ("rc=%d\n", rc));
        pWait->fInterrupted = true;
    }
}


/**
 * Checks if a solaris wait was interrupted.
 *
 * @returns true / false
 * @param   pWait               The wait structure.
 * @remarks This shall be called before the first rtR0SemSolWaitDoIt().
 */
DECLINLINE(bool) rtR0SemSolWaitWasInterrupted(PRTR0SEMSOLWAIT pWait)
{
    return pWait->fInterrupted;
}


/**
 * Checks if a solaris wait has timed out.
 *
 * @returns true / false
 * @param   pWait               The wait structure.
 */
DECLINLINE(bool) rtR0SemSolWaitHasTimedOut(PRTR0SEMSOLWAIT pWait)
{
    return pWait->fTimedOut;
}


/**
 * Deletes a solaris wait.
 *
 * @param   pWait               The wait structure.
 */
DECLINLINE(void) rtR0SemSolWaitDelete(PRTR0SEMSOLWAIT pWait)
{
    pWait->pThread = NULL;
}



/**
 * Worker for RTSemEventMultiWaitEx and RTSemEventMultiWaitExDebug.
 *
 * @returns VBox status code.
 * @param   pThis           The event semaphore.
 * @param   fFlags          See RTSemEventMultiWaitEx.
 * @param   uTimeout        See RTSemEventMultiWaitEx.
 * @param   pSrcPos         The source code position of the wait.
 */
static int rtR0SemEventMultiSolWait(PRTSEMEVENTMULTIINTERNAL pThis, uint32_t fFlags, uint64_t uTimeout,
                                    PCRTLOCKVALSRCPOS pSrcPos)
{
    uint32_t    fOrgStateAndGen;
    int         rc;

    /*
     * Validate the input.
     */
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("%p u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_PARAMETER);
    AssertReturn(RTSEMWAIT_FLAGS_ARE_VALID(fFlags), VERR_INVALID_PARAMETER);
    rtR0SemEventMultiSolRetain(pThis);
    mutex_enter(&pThis->Mtx); /* this could be moved down to the else, but play safe for now. */

    /*
     * Is the event already signalled or do we have to wait?
     */
    fOrgStateAndGen = ASMAtomicUoReadU32(&pThis->fStateAndGen);
    if (fOrgStateAndGen & RTSEMEVENTMULTISOL_STATE_MASK)
        rc = VINF_SUCCESS;
    else
    {
        /*
         * We have to wait.
         */
        RTR0SEMSOLWAIT Wait;
        rtR0SemSolWaitInit(&Wait, fFlags, uTimeout);
        for (;;)
        {
            /* The destruction test. */
            if (RT_UNLIKELY(pThis->u32Magic != RTSEMEVENTMULTI_MAGIC))
                rc = VERR_SEM_DESTROYED;
            else
            {
                /* Check the exit conditions. */
                if (RT_UNLIKELY(pThis->u32Magic != RTSEMEVENTMULTI_MAGIC))
                    rc = VERR_SEM_DESTROYED;
                else if (ASMAtomicUoReadU32(&pThis->fStateAndGen) != fOrgStateAndGen)
                    rc = VINF_SUCCESS;
                else if (rtR0SemSolWaitHasTimedOut(&Wait))
                    rc = VERR_TIMEOUT;
                else if (rtR0SemSolWaitWasInterrupted(&Wait))
                    rc = VERR_INTERRUPTED;
                else
                {
                    /* Do the wait and then recheck the conditions. */
                    rtR0SemSolWaitDoIt(&Wait, &pThis->Cnd, &pThis->Mtx);
                    continue;
                }
            }
            break;
        }
        rtR0SemSolWaitDelete(&Wait);
    }

    mutex_exit(&pThis->Mtx);
    rtR0SemEventMultiSolRelease(pThis);
    return rc;
}



#undef RTSemEventMultiWaitEx
RTDECL(int)  RTSemEventMultiWaitEx(RTSEMEVENTMULTI hEventMultiSem, uint32_t fFlags, uint64_t uTimeout)
{
#ifndef RTSEMEVENT_STRICT
    return rtR0SemEventMultiSolWait(hEventMultiSem, fFlags, uTimeout, NULL);
#else
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_NORMAL_API();
    return rtR0SemEventMultiSolWait(hEventMultiSem, fFlags, uTimeout, &SrcPos);
#endif
}


RTDECL(int)  RTSemEventMultiWaitExDebug(RTSEMEVENTMULTI hEventMultiSem, uint32_t fFlags, uint64_t uTimeout,
                                        RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALSRCPOS SrcPos = RTLOCKVALSRCPOS_INIT_DEBUG_API();
    return rtR0SemEventMultiSolWait(hEventMultiSem, fFlags, uTimeout, &SrcPos);
}


#include "../../generic/RTSemEventMultiWait-2-ex-generic.cpp"
#include "../../generic/RTSemEventMultiWaitNoResume-2-ex-generic.cpp"

#else /* OLD_STUFF */

/**
 * Translate milliseconds into ticks and go to sleep using the right method.
 *
 * @retval  >0 on normal or spurious wake-up.
 * @retval  -1 on timeout.
 * @retval  0 on signal.
 */
static int rtSemEventMultiWaitWorker(PRTSEMEVENTMULTIINTERNAL pThis, RTMSINTERVAL cMillies, bool fInterruptible)
{
    int rc;
    if (cMillies != RT_INDEFINITE_WAIT)
    {
        clock_t cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
        clock_t cTimeout = ddi_get_lbolt();
        cTimeout += cTicks;
        if (fInterruptible)
            rc = cv_timedwait_sig(&pThis->Cnd, &pThis->Mtx, cTimeout);
        else
            rc = cv_timedwait(&pThis->Cnd, &pThis->Mtx, cTimeout);
    }
    else
    {
        if (fInterruptible)
            rc = cv_wait_sig(&pThis->Cnd, &pThis->Mtx);
        else
        {
            cv_wait(&pThis->Cnd, &pThis->Mtx);
            rc = 1;
        }
    }
    return rc;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies, bool fInterruptible)
{
    int rc;
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);
    rtR0SemEventMultiSolRetain(pThis);
    if (cMillies)
        RT_ASSERT_PREEMPTIBLE();

    mutex_enter(&pThis->Mtx);
    Assert(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC);

    if (pThis->fStateAndGen & RTSEMEVENTMULTISOL_STATE_MASK)
        rc = VINF_SUCCESS;
    else if (!cMillies)
        rc = VERR_TIMEOUT;
    else
    {
        /* This loop is only for continuing after a spurious wake-up. */
        for (;;)
        {
            uint32_t const uSignalGenBeforeWait = pThis->fStateAndGen;
            rc = rtSemEventMultiWaitWorker(pThis, cMillies, fInterruptible);
            if (rc > 0)
            {
                if (RT_LIKELY(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC))
                {
                    if (pThis->fStateAndGen == uSignalGenBeforeWait)
                        continue; /* Spurious wake-up, go back to waiting. */

                    /* Retured due to call to cv_signal() or cv_broadcast(). */
                    rc = VINF_SUCCESS;
                }
                else
                    /* We're being destroyed. */
                    rc = VERR_SEM_DESTROYED;
            }
            else if (rc == -1)
                /* Returned due to timeout being reached. */
                rc = VERR_TIMEOUT;
            else
                rc = VERR_INTERRUPTED;
                /* Returned due to pending signal. */
            break;
        }
    }

    mutex_exit(&pThis->Mtx);
    rtR0SemEventMultiSolRelease(pThis);
    return rc;
}


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, true /* interruptible */);
}

#endif
