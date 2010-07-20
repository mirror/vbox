/* $Id$ */
/** @file
 * IPRT - Semaphores, Ring-0 Driver, Solaris.
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
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/thread.h>
#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Solaris event semaphore.
 */
typedef struct RTSEMEVENTINTERNAL
{
    /** Magic value (RTSEMEVENT_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The number of threads referencing this object. */
    uint32_t volatile   cRefs;
    /** Set if the object is signalled when there are no waiters. */
    bool                fSignaled;
    /** Object generation.
     * This is incremented every time the object is signalled and used to
     * check for spurious wake-ups. */
    uint32_t            uSignalGen;
    /** The number of waiting threads. */
    uint32_t            cWaiters;
    /** The number of signalled threads. */
    uint32_t            cWakeUp;
    /** The Solaris mutex protecting this structure and pairing up the with the cv. */
    kmutex_t            Mtx;
    /** The Solaris condition variable. */
    kcondvar_t          Cnd;
} RTSEMEVENTINTERNAL, *PRTSEMEVENTINTERNAL;



RTDECL(int)  RTSemEventCreate(PRTSEMEVENT phEventSem)
{
    return RTSemEventCreateEx(phEventSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventCreateEx(PRTSEMEVENT phEventSem, uint32_t fFlags, RTLOCKVALCLASS hClass, const char *pszNameFmt, ...)
{
    AssertCompile(sizeof(RTSEMEVENTINTERNAL) > sizeof(void *));
    AssertReturn(!(fFlags & ~RTSEMEVENT_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phEventSem, VERR_INVALID_POINTER);
    RT_ASSERT_PREEMPTIBLE();

    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic       = RTSEMEVENT_MAGIC;
    pThis->cRefs          = 1;
    pThis->fSignaled      = false;
    pThis->uSignalGen     = 0;
    pThis->cWaiters       = 0;
    pThis->cWakeUp        = 0;
    mutex_init(&pThis->Mtx, "IPRT Event Semaphore", MUTEX_DRIVER, (void *)ipltospl(DISP_LEVEL));
    cv_init(&pThis->Cnd, "IPRT CV", CV_DRIVER, NULL);

    *phEventSem = pThis;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT hEventSem)
{
    PRTSEMEVENTINTERNAL pThis = hEventSem;
    if (pThis == NIL_RTSEMEVENT)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();

    mutex_enter(&pThis->Mtx);

    ASMAtomicDecU32(&pThis->cRefs);

    pThis->u32Magic = RTSEMEVENT_MAGIC_DEAD; /* make the handle invalid */
    if (pThis->cWaiters > 0)
    {
        /*
         * Signal all threads to destroy.
         */
        cv_broadcast(&pThis->Cnd);
        mutex_exit(&pThis->Mtx);
    }
    else if (pThis->cRefs == 0)
    {
        /*
         * We're the last thread referencing this object, destroy it.
         */
        mutex_exit(&pThis->Mtx);
        cv_destroy(&pThis->Cnd);
        mutex_destroy(&pThis->Mtx);
        RTMemFree(pThis);
    }
    else
    {
        /*
         * There are other threads still referencing this object, last one cleans up.
         */
        mutex_exit(&pThis->Mtx);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT hEventSem)
{
    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)hEventSem;
    RT_ASSERT_PREEMPT_CPUID_VAR();
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();

    /*
     * If we're in interrupt context we need to unpin the underlying current
     * thread as this could lead to a deadlock (see #4259 for the full explanation)
     *
     * Note! This assumes nobody is using the RTThreadPreemptDisable in an
     *       interrupt context and expects it to work right.  The swtch will
     *       result in a voluntary preemption.  To fix this, we would have to
     *       do our own counting in RTThreadPreemptDisable/Restore like we do
     *       on systems which doesn't do preemption (OS/2, linux, ...) and
     *       check whether preemption was disabled via RTThreadPreemptDisable
     *       or not and only call swtch if RTThreadPreemptDisable wasn't called.
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

    /*
     * If there are more waiting threads, wake them up. Otherwise leave the
     * semaphore in the signalled state.
     */
    pThis->cWakeUp++;
    if (pThis->cWakeUp <= pThis->cWaiters) /** @todo r=bird: see cWakeup = 0 below. */
    {
        cv_signal(&pThis->Cnd);
        pThis->uSignalGen++;
    }
    else
        pThis->fSignaled = true;

    mutex_exit(&pThis->Mtx);

    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}


static int rtSemEventWaitWorker(PRTSEMEVENTINTERNAL pThis, RTMSINTERVAL cMillies, bool fInterruptible)
{
    /*
     * Translate milliseconds into ticks and go to sleep.
     */
    int rc = 0;
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


static int rtSemEventWait(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies, bool fInterruptible)
{
    int rc;
    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)hEventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);
    if (cMillies)
        RT_ASSERT_PREEMPTIBLE();

    mutex_enter(&pThis->Mtx);

    ASMAtomicIncU32(&pThis->cRefs);

    if (pThis->fSignaled)
    {
        /*
         * The last signal occurred without any waiters and now we're the first thread
         * waiting for the event signal. So no real need to wait for one.
         */
        Assert(!pThis->cWaiters);
        pThis->fSignaled = false;
        /** @todo r=bird: This will get out of whack if someone is in the
         *        process of waking up (waiting to be scheduled). Further
         *        more, a race between a cv_signal and a
         *        timeout/interruption may cause wakeups to go unconsumed.
         *        Not sure how we could easily deal with this rigth now... */
        pThis->cWakeUp   = 0;
        rc = VINF_SUCCESS;
    }
    else if (!cMillies)
        rc = VERR_TIMEOUT;
    else
    {
        pThis->cWaiters++;
        /* This loop is only for continuing after a spurious wake-up. */
        for (;;)
        {
            uint32_t const uSignalGenBeforeWait = pThis->uSignalGen;
            rc = rtSemEventWaitWorker(pThis, cMillies, fInterruptible);
            if (rc > 0)
            {
                if (pThis->u32Magic == RTSEMEVENT_MAGIC)
                {
                    if (pThis->uSignalGen != uSignalGenBeforeWait)
                    {
                        /* We've been signaled by cv_signal(), consume the wake up. */
                        --pThis->cWakeUp; /** @todo r=bird: May cause underflow, see above. */
                        rc = VINF_SUCCESS;
                    }
                    else
                        /* Spurious wakeup due to some signal, go back to waiting. */
                        continue;
                }
                else
                    /* We're being destroyed. */
                    rc = VERR_SEM_DESTROYED;
            }
            else if (rc == -1)
                /* Timeout reached. */
                rc = VERR_TIMEOUT;
            else
                /* Returned due to pending signal */
                rc = VERR_INTERRUPTED;

            break;
        }
        --pThis->cWaiters;
    }

    if (!ASMAtomicDecU32(&pThis->cRefs))
    {
        Assert(RT_FAILURE_NP(rc));
        mutex_exit(&pThis->Mtx);
        cv_destroy(&pThis->Cnd);
        mutex_destroy(&pThis->Mtx);
        RTMemFree(pThis);
        return rc;
    }

    mutex_exit(&pThis->Mtx);
    return rc;
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies)
{
    return rtSemEventWait(hEventSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies)
{
    return rtSemEventWait(hEventSem, cMillies, true /* interruptible */);
}

