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
#include <iprt/mem.h>
#include <iprt/mp.h>
#include <iprt/thread.h>
#include "internal/magics.h"


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
    int32_t volatile    cRefs;
    /** Set if the event object is signaled. */
    bool                fSignaled;
    /** Object generation.
     * This is incremented every time the object is signalled and used to
     * check for spurious wake-ups. */
    uint32_t            uSignalGen;
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
        pThis->fSignaled    = false;
        pThis->uSignalGen   = 0;
        mutex_init(&pThis->Mtx, "IPRT Multiple Release Event Semaphore", MUTEX_DRIVER, (void *)ipltospl(DISP_LEVEL));
        cv_init(&pThis->Cnd, "IPRT CV", CV_DRIVER, NULL);

        *phEventMultiSem = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


/**
 * Destructor that is called when cRefs == 0.
 * @param   pThis               The instance to destroy.
 */
static void rtSemEventMultiDtor(PRTSEMEVENTMULTIINTERNAL pThis)
{
    cv_destroy(&pThis->Cnd);
    mutex_destroy(&pThis->Mtx);
    RTMemFree(pThis);
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
    pThis->u32Magic = RTSEMEVENTMULTI_MAGIC_DEAD;
    cv_broadcast(&pThis->Cnd);

    /* Drop the reference from RTSemEventMultiCreateEx. */
    if (ASMAtomicDecS32(&pThis->cRefs))
        mutex_exit(&pThis->Mtx);
    else
        rtSemEventMultiDtor(pThis);
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
    pThis->fSignaled = true;
    pThis->uSignalGen++;
    cv_broadcast(&pThis->Cnd);

    mutex_exit(&pThis->Mtx);

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
    pThis->fSignaled = false;

    mutex_exit(&pThis->Mtx);

    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}


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
    if (cMillies)
        RT_ASSERT_PREEMPTIBLE();

    mutex_enter(&pThis->Mtx);
    Assert(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC);
    ASMAtomicIncS32(&pThis->cRefs);

    if (pThis->fSignaled)
        rc = VINF_SUCCESS;
    else if (!cMillies)
        rc = VERR_TIMEOUT;
    else
    {

        /* This loop is only for continuing after a spurious wake-up. */
        for (;;)
        {
            uint32_t const uSignalGenBeforeWait = pThis->uSignalGen;
            rc = rtSemEventMultiWaitWorker(pThis, cMillies, fInterruptible);
            if (rc > 0)
            {
                if (RT_LIKELY(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC))
                {
                    if (pThis->uSignalGen == uSignalGenBeforeWait)
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

    if (RT_LIKELY(ASMAtomicDecS32(&pThis->cRefs)))
        mutex_exit(&pThis->Mtx);
    else
        rtSemEventMultiDtor(pThis);
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

