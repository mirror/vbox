/* $Id$ */
/** @file
 * IPRT - Semaphores, Ring-0 Driver, Darwin.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-darwin-kernel.h"
#include "internal/iprt.h"
#include <iprt/semaphore.h>

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include <iprt/mp.h>
#include <iprt/thread.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Darwin event semaphore.
 */
typedef struct RTSEMEVENTINTERNAL
{
    /** Magic value (RTSEMEVENT_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The number of waiting threads. */
    uint32_t volatile   cWaiters;
    /** Set if the event object is signaled. */
    uint8_t volatile    fSignaled;
    /** The number of threads in the process of waking up. */
    uint32_t volatile   cWaking;
    /** The spinlock protecting us. */
    lck_spin_t         *pSpinlock;
} RTSEMEVENTINTERNAL, *PRTSEMEVENTINTERNAL;


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


#if 0 /** @todo */
/**
 * Darwin mutex semaphore.
 */
typedef struct RTSEMMUTEXINTERNAL
{
    /** Magic value (RTSEMMUTEX_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The mutex. */
    lck_mtx_t          *pMtx;
} RTSEMMUTEXINTERNAL, *PRTSEMMUTEXINTERNAL;

#endif


/**
 * Wrapper for the darwin semaphore structure.
 */
typedef struct RTSEMFASTMUTEXINTERNAL
{
    /** Magic value (RTSEMFASTMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** The mutex. */
    lck_mtx_t          *pMtx;
} RTSEMFASTMUTEXINTERNAL, *PRTSEMFASTMUTEXINTERNAL;



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
    if (pThis)
    {
        pThis->u32Magic = RTSEMEVENT_MAGIC;
        pThis->cWaiters = 0;
        pThis->cWaking = 0;
        pThis->fSignaled = 0;
        Assert(g_pDarwinLockGroup);
        pThis->pSpinlock = lck_spin_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pThis->pSpinlock)
        {
            *phEventSem = pThis;
            return VINF_SUCCESS;
        }

        pThis->u32Magic = 0;
        RTMemFree(pThis);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT hEventSem)
{
    PRTSEMEVENTINTERNAL pThis = hEventSem;
    if (pThis == NIL_RTSEMEVENT)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
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


RTDECL(int)  RTSemEventSignal(RTSEMEVENT hEventSem)
{
    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)hEventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);
    RT_ASSERT_PREEMPT_CPUID_VAR();
    RT_ASSERT_INTS_ON();

    /** @todo should probably disable interrupts here... update
     *        semspinmutex-r0drv-generic.c when done. */
    lck_spin_lock(pThis->pSpinlock);

    if (pThis->cWaiters > 0)
    {
        ASMAtomicDecU32(&pThis->cWaiters);
        ASMAtomicIncU32(&pThis->cWaking);
        thread_wakeup_prim((event_t)pThis, TRUE /* one thread */, THREAD_AWAKENED);
        /** @todo this isn't safe. a scheduling interrupt on the other cpu while we're in here
         * could cause the thread to be timed out before we manage to wake it up and the event
         * ends up in the wrong state. ditto for posix signals.
         * Update: check the return code; it will return KERN_NOT_WAITING if no one is around. */
    }
    else
        ASMAtomicXchgU8(&pThis->fSignaled, true);

    lck_spin_unlock(pThis->pSpinlock);

    RT_ASSERT_PREEMPT_CPUID();
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies, wait_interrupt_t fInterruptible)
{
    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)hEventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    if (cMillies)
        RT_ASSERT_PREEMPTIBLE();

    lck_spin_lock(pThis->pSpinlock);

    int rc;
    if (pThis->fSignaled)
    {
        Assert(!pThis->cWaiters);
        ASMAtomicXchgU8(&pThis->fSignaled, false);
        rc = VINF_SUCCESS;
    }
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
                    &&  pThis->u32Magic != RTSEMEVENT_MAGIC)
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


RTDECL(int)  RTSemEventWait(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies)
{
    return rtSemEventWait(hEventSem, cMillies, THREAD_UNINT);
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies)
{
    return rtSemEventWait(hEventSem, cMillies, THREAD_ABORTSAFE);
}



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





#if 0 /* need proper timeout lock function! */
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX phFastMtx)
{
    RT_ASSERT_PREEMPTIBLE();
    AssertCompile(sizeof(RTSEMMUTEXINTERNAL) > sizeof(void *));
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMMUTEX_MAGIC;
        Assert(g_pDarwinLockGroup);
        pThis->pMtx = lck_mtx_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pThis->pMtx)
        {
            *phFastMtx = pThis;
            return VINF_SUCCESS;
        }
        RTMemFree(pThis);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX hMutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)hMutexSem;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsg(pThis->u32Magic == RTSEMMUTEX_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicIncU32(&pThis->u32Magic);

    Assert(g_pDarwinLockGroup);
    lck_mtx_free(pThis->pMtx, g_pDarwinLockGroup);
    pThis->pMtx = NULL;

    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX hMutexSem, RTMSINTERVAL cMillies)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)hMutexSem;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsg(pThis->u32Magic == RTSEMMUTEX_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);
    if (cMillies)
        RT_ASSERT_PREEMPTIBLE();

    /*
     * Get the mutex.
     */
    wait_result_t rc = lck_mtx_lock_deadlink
#if 1
#else
    NTSTATUS rcNt;
    if (cMillies == RT_INDEFINITE_WAIT)
        rcNt = KeWaitForSingleObject(&pThis->Mutex, Executive, KernelMode, TRUE, NULL);
    else
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -(int64_t)cMillies * 10000;
        rcNt = KeWaitForSingleObject(&pThis->Mutex, Executive, KernelMode, TRUE, &Timeout);
    }
    switch (rcNt)
    {
        case STATUS_SUCCESS:
            if (pThis->u32Magic == RTSEMMUTEX_MAGIC)
                return VINF_SUCCESS;
            return VERR_SEM_DESTROYED;
        case STATUS_ALERTED:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_USER_APC:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_TIMEOUT:
            return VERR_TIMEOUT;
        default:
            AssertMsgFailed(("pThis->u32Magic=%RX32 pThis=%p: wait returned %lx!\n",
                             pThis->u32Magic, pThis, (long)rcNt));
            return VERR_INTERNAL_ERROR;
    }
#endif
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX hMutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)hMutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsg(pThis->u32Magic == RTSEMMUTEX_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);
    RT_ASSERT_PREEMPTIBLE();

    /*
     * Release the mutex.
     */
#ifdef RT_USE_FAST_MUTEX
    ExReleaseFastMutex(&pThis->Mutex);
#else
    KeReleaseMutex(&pThis->Mutex, FALSE);
#endif
    return VINF_SUCCESS;
}

#endif /* later */




RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX phFastMtx)
{
    AssertCompile(sizeof(RTSEMFASTMUTEXINTERNAL) > sizeof(void *));
    AssertPtrReturn(phFastMtx, VERR_INVALID_POINTER);
    RT_ASSERT_PREEMPTIBLE();

    PRTSEMFASTMUTEXINTERNAL pThis = (PRTSEMFASTMUTEXINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMFASTMUTEX_MAGIC;
        Assert(g_pDarwinLockGroup);
        pThis->pMtx = lck_mtx_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pThis->pMtx)
        {
            *phFastMtx = pThis;
            return VINF_SUCCESS;
        }

        RTMemFree(pThis);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX hFastMtx)
{
    PRTSEMFASTMUTEXINTERNAL pThis = hFastMtx;
    if (pThis == NIL_RTSEMFASTMUTEX)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMFASTMUTEX_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    RT_ASSERT_INTS_ON();

    ASMAtomicWriteU32(&pThis->u32Magic, RTSEMFASTMUTEX_MAGIC_DEAD);
    Assert(g_pDarwinLockGroup);
    lck_mtx_free(pThis->pMtx, g_pDarwinLockGroup);
    pThis->pMtx = NULL;
    RTMemFree(pThis);

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX hFastMtx)
{
    PRTSEMFASTMUTEXINTERNAL pThis = hFastMtx;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMFASTMUTEX_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    RT_ASSERT_PREEMPTIBLE();

    lck_mtx_lock(pThis->pMtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX hFastMtx)
{
    PRTSEMFASTMUTEXINTERNAL pThis = hFastMtx;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMFASTMUTEX_MAGIC, ("%p: u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);
    RT_ASSERT_PREEMPTIBLE();

    lck_mtx_unlock(pThis->pMtx);
    return VINF_SUCCESS;
}

