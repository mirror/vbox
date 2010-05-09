/* $Id$ */
/** @file
 * IPRT - Multiple Release Event Semaphores, Ring-0 Driver, Darwin.
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
#include "the-darwin-kernel.h"
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
 * Darwin multiple release event semaphore.
 */
typedef struct RTSEMEVENTMULTIINTERNAL
{
    /** Magic value (RTSEMEVENTMULTI_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The number of waiting threads. */
    uint32_t volatile   cWaiters;
    /** Set if the event object is signaled. */
    uint8_t volatile    fSignaled;
    /** The number of threads in the process of waking up. */
    uint32_t volatile   cWaking;
    /** The spinlock protecting us. */
    lck_spin_t         *pSpinlock;
} RTSEMEVENTMULTIINTERNAL, *PRTSEMEVENTMULTIINTERNAL;



RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI phEventMultiSem)
{
    return RTSemEventMultiCreateEx(phEventMultiSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventMultiCreateEx(PRTSEMEVENTMULTI phEventMultiSem, uint32_t fFlags, RTLOCKVALCLASS hClass,
                                     const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMEVENTMULTI_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    AssertCompile(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    AssertPtrReturn(phEventMultiSem, VERR_INVALID_POINTER);
    RT_ASSERT_PREEMPTIBLE();

    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pThis->cWaiters = 0;
        pThis->cWaking = 0;
        pThis->fSignaled = 0;
        Assert(g_pDarwinLockGroup);
        pThis->pSpinlock = lck_spin_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pThis->pSpinlock)
        {
            *phEventMultiSem = pThis;
            return VINF_SUCCESS;
        }

        pThis->u32Magic = 0;
        RTMemFree(pThis);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (pThis == NIL_RTSEMEVENTMULTI)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();

    lck_spin_lock(pThis->pSpinlock);
    ASMAtomicIncU32(&pThis->u32Magic); /* make the handle invalid */
    if (pThis->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        thread_wakeup_prim((event_t)pThis, FALSE /* all threads */, THREAD_RESTART);
        lck_spin_unlock(pThis->pSpinlock);
    }
    else if (pThis->cWaking)
        /* the last waking thread is gonna do the cleanup */
        lck_spin_unlock(pThis->pSpinlock);
    else
    {
        lck_spin_unlock(pThis->pSpinlock);
        lck_spin_destroy(pThis->pSpinlock, g_pDarwinLockGroup);
        RTMemFree(pThis);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    RT_ASSERT_PREEMPT_CPUID_VAR();
    RT_ASSERT_INTS_ON();

    lck_spin_lock(pThis->pSpinlock);

    ASMAtomicXchgU8(&pThis->fSignaled, true);
    if (pThis->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        ASMAtomicXchgU32(&pThis->cWaiters, 0);
        thread_wakeup_prim((event_t)pThis, FALSE /* all threads */, THREAD_AWAKENED);
    }

    lck_spin_unlock(pThis->pSpinlock);

    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    RT_ASSERT_PREEMPT_CPUID_VAR();
    RT_ASSERT_INTS_ON();

    lck_spin_lock(pThis->pSpinlock);
    ASMAtomicXchgU8(&pThis->fSignaled, false);
    lck_spin_unlock(pThis->pSpinlock);

    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies, wait_interrupt_t fInterruptible)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    if (cMillies)
        RT_ASSERT_PREEMPTIBLE();

    lck_spin_lock(pThis->pSpinlock);

    int rc;
    if (pThis->fSignaled)
        rc = VINF_SUCCESS;
    else if (!cMillies)
        rc = VERR_TIMEOUT;
    else
    {
        ASMAtomicIncU32(&pThis->cWaiters);

        wait_result_t rcWait;
        if (cMillies == RT_INDEFINITE_WAIT)
            rcWait = lck_spin_sleep(pThis->pSpinlock, LCK_SLEEP_DEFAULT, (event_t)pThis, fInterruptible);
        else
        {
            uint64_t u64AbsTime;
            nanoseconds_to_absolutetime(cMillies * UINT64_C(1000000), &u64AbsTime);
            u64AbsTime += mach_absolute_time();

            rcWait = lck_spin_sleep_deadline(pThis->pSpinlock, LCK_SLEEP_DEFAULT,
                                             (event_t)pThis, fInterruptible, u64AbsTime);
        }
        switch (rcWait)
        {
            case THREAD_AWAKENED:
                Assert(pThis->cWaking > 0);
                if (    !ASMAtomicDecU32(&pThis->cWaking)
                    &&  pThis->u32Magic != RTSEMEVENTMULTI_MAGIC)
                {
                    /* the event was destroyed after we woke up, as the last thread do the cleanup. */
                    lck_spin_unlock(pThis->pSpinlock);
                    Assert(g_pDarwinLockGroup);
                    lck_spin_destroy(pThis->pSpinlock, g_pDarwinLockGroup);
                    RTMemFree(pThis);
                    return VINF_SUCCESS;
                }
                rc = VINF_SUCCESS;
                break;

            case THREAD_TIMED_OUT:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                ASMAtomicDecU32(&pThis->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case THREAD_INTERRUPTED:
                Assert(fInterruptible);
                ASMAtomicDecU32(&pThis->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            case THREAD_RESTART:
                /* Last one out does the cleanup. */
                if (!ASMAtomicDecU32(&pThis->cWaking))
                {
                    lck_spin_unlock(pThis->pSpinlock);
                    Assert(g_pDarwinLockGroup);
                    lck_spin_destroy(pThis->pSpinlock, g_pDarwinLockGroup);
                    RTMemFree(pThis);
                    return VERR_SEM_DESTROYED;
                }

                rc = VERR_SEM_DESTROYED;
                break;

            default:
                AssertMsgFailed(("rcWait=%d\n", rcWait));
                rc = VERR_GENERAL_FAILURE;
                break;
        }
    }

    lck_spin_unlock(pThis->pSpinlock);
    return rc;
}


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, THREAD_UNINT);
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, THREAD_ABORTSAFE);
}

