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

#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>

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



RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    Assert(sizeof(RTSEMEVENTINTERNAL) > sizeof(void *));
    AssertPtrReturn(pEventSem, VERR_INVALID_POINTER);

    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)RTMemAlloc(sizeof(*pEventInt));
    if (pEventInt)
    {
        pEventInt->u32Magic = RTSEMEVENT_MAGIC;
        pEventInt->cWaiters = 0;
        pEventInt->cWaking = 0;
        pEventInt->fSignaled = 0;
        Assert(g_pDarwinLockGroup);
        pEventInt->pSpinlock = lck_spin_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pEventInt->pSpinlock)
        {
            *pEventSem = pEventInt;
            return VINF_SUCCESS;
        }

        pEventInt->u32Magic = 0;
        RTMemFree(pEventInt);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    if (EventSem == NIL_RTSEMEVENT)     /* don't bitch */
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventInt->pSpinlock);
    ASMAtomicIncU32(&pEventInt->u32Magic); /* make the handle invalid */
    if (pEventInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventInt->cWaking, pEventInt->cWaking + pEventInt->cWaiters);
        thread_wakeup_prim((event_t)pEventInt, FALSE /* all threads */, THREAD_RESTART);
        lck_spin_unlock(pEventInt->pSpinlock);
    }
    else if (pEventInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        lck_spin_unlock(pEventInt->pSpinlock);
    else
    {
        lck_spin_unlock(pEventInt->pSpinlock);
        lck_spin_destroy(pEventInt->pSpinlock, g_pDarwinLockGroup);
        RTMemFree(pEventInt);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventInt->pSpinlock);

    if (pEventInt->cWaiters > 0)
    {
        ASMAtomicDecU32(&pEventInt->cWaiters);
        ASMAtomicIncU32(&pEventInt->cWaking);
        thread_wakeup_prim((event_t)pEventInt, TRUE /* one thread */, THREAD_AWAKENED);
        /** @todo this isn't safe. a scheduling interrupt on the other cpu while we're in here
         * could cause the thread to be timed out before we manage to wake it up and the event
         * ends up in the wrong state. ditto for posix signals. */
    }
    else
        ASMAtomicXchgU8(&pEventInt->fSignaled, true);

    lck_spin_unlock(pEventInt->pSpinlock);
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT EventSem, unsigned cMillies, wait_interrupt_t fInterruptible)
{
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventInt->pSpinlock);

    int rc;
    if (pEventInt->fSignaled)
    {
        Assert(!pEventInt->cWaiters);
        ASMAtomicXchgU8(&pEventInt->fSignaled, false);
        rc = VINF_SUCCESS;
    }
    else
    {
        ASMAtomicIncU32(&pEventInt->cWaiters);

        wait_result_t rcWait;
        if (cMillies == RT_INDEFINITE_WAIT)
            rcWait = lck_spin_sleep(pEventInt->pSpinlock, LCK_SLEEP_DEFAULT, (event_t)pEventInt, fInterruptible);
        else
        {
            uint64_t u64AbsTime;
            nanoseconds_to_absolutetime(cMillies * UINT64_C(1000000), &u64AbsTime);
            u64AbsTime += mach_absolute_time();

            rcWait = lck_spin_sleep_deadline(pEventInt->pSpinlock, LCK_SLEEP_DEFAULT,
                                             (event_t)pEventInt, fInterruptible, u64AbsTime);
        }
        switch (rcWait)
        {
            case THREAD_AWAKENED:
                Assert(pEventInt->cWaking > 0);
                if (    !ASMAtomicDecU32(&pEventInt->cWaking)
                    &&  pEventInt->u32Magic != RTSEMEVENT_MAGIC)
                {
                    /* the event was destroyed after we woke up, as the last thread do the cleanup. */
                    lck_spin_unlock(pEventInt->pSpinlock);
                    Assert(g_pDarwinLockGroup);
                    lck_spin_destroy(pEventInt->pSpinlock, g_pDarwinLockGroup);
                    RTMemFree(pEventInt);
                    return VINF_SUCCESS;
                }
                rc = VINF_SUCCESS;
                break;

            case THREAD_TIMED_OUT:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                ASMAtomicDecU32(&pEventInt->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case THREAD_INTERRUPTED:
                Assert(fInterruptible);
                ASMAtomicDecU32(&pEventInt->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            case THREAD_RESTART:
                /* Last one out does the cleanup. */
                if (!ASMAtomicDecU32(&pEventInt->cWaking))
                {
                    lck_spin_unlock(pEventInt->pSpinlock);
                    Assert(g_pDarwinLockGroup);
                    lck_spin_destroy(pEventInt->pSpinlock, g_pDarwinLockGroup);
                    RTMemFree(pEventInt);
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

    lck_spin_unlock(pEventInt->pSpinlock);
    return rc;
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, THREAD_UNINT);
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, THREAD_ABORTSAFE);
}



RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    Assert(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    AssertPtrReturn(pEventMultiSem, VERR_INVALID_POINTER);

    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pEventMultiInt));
    if (pEventMultiInt)
    {
        pEventMultiInt->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pEventMultiInt->cWaiters = 0;
        pEventMultiInt->cWaking = 0;
        pEventMultiInt->fSignaled = 0;
        Assert(g_pDarwinLockGroup);
        pEventMultiInt->pSpinlock = lck_spin_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pEventMultiInt->pSpinlock)
        {
            *pEventMultiSem = pEventMultiInt;
            return VINF_SUCCESS;
        }

        pEventMultiInt->u32Magic = 0;
        RTMemFree(pEventMultiInt);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    if (EventMultiSem == NIL_RTSEMEVENTMULTI)     /* don't bitch */
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventMultiInt->pSpinlock);
    ASMAtomicIncU32(&pEventMultiInt->u32Magic); /* make the handle invalid */
    if (pEventMultiInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        thread_wakeup_prim((event_t)pEventMultiInt, FALSE /* all threads */, THREAD_RESTART);
        lck_spin_unlock(pEventMultiInt->pSpinlock);
    }
    else if (pEventMultiInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        lck_spin_unlock(pEventMultiInt->pSpinlock);
    else
    {
        lck_spin_unlock(pEventMultiInt->pSpinlock);
        lck_spin_destroy(pEventMultiInt->pSpinlock, g_pDarwinLockGroup);
        RTMemFree(pEventMultiInt);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventMultiInt->pSpinlock);

    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, true);
    if (pEventMultiInt->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        ASMAtomicXchgU32(&pEventMultiInt->cWaiters, 0);
        thread_wakeup_prim((event_t)pEventMultiInt, FALSE /* all threads */, THREAD_AWAKENED);
    }

    lck_spin_unlock(pEventMultiInt->pSpinlock);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventMultiInt->pSpinlock);
    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, false);
    lck_spin_unlock(pEventMultiInt->pSpinlock);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, wait_interrupt_t fInterruptible)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    lck_spin_lock(pEventMultiInt->pSpinlock);

    int rc;
    if (pEventMultiInt->fSignaled)
        rc = VINF_SUCCESS;
    else
    {
        ASMAtomicIncU32(&pEventMultiInt->cWaiters);

        wait_result_t rcWait;
        if (cMillies == RT_INDEFINITE_WAIT)
            rcWait = lck_spin_sleep(pEventMultiInt->pSpinlock, LCK_SLEEP_DEFAULT, (event_t)pEventMultiInt, fInterruptible);
        else
        {
            uint64_t u64AbsTime;
            nanoseconds_to_absolutetime(cMillies * UINT64_C(1000000), &u64AbsTime);
            u64AbsTime += mach_absolute_time();

            rcWait = lck_spin_sleep_deadline(pEventMultiInt->pSpinlock, LCK_SLEEP_DEFAULT,
                                             (event_t)pEventMultiInt, fInterruptible, u64AbsTime);
        }
        switch (rcWait)
        {
            case THREAD_AWAKENED:
                Assert(pEventMultiInt->cWaking > 0);
                if (    !ASMAtomicDecU32(&pEventMultiInt->cWaking)
                    &&  pEventMultiInt->u32Magic != RTSEMEVENTMULTI_MAGIC)
                {
                    /* the event was destroyed after we woke up, as the last thread do the cleanup. */
                    lck_spin_unlock(pEventMultiInt->pSpinlock);
                    Assert(g_pDarwinLockGroup);
                    lck_spin_destroy(pEventMultiInt->pSpinlock, g_pDarwinLockGroup);
                    RTMemFree(pEventMultiInt);
                    return VINF_SUCCESS;
                }
                rc = VINF_SUCCESS;
                break;

            case THREAD_TIMED_OUT:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                ASMAtomicDecU32(&pEventMultiInt->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case THREAD_INTERRUPTED:
                Assert(fInterruptible);
                ASMAtomicDecU32(&pEventMultiInt->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            case THREAD_RESTART:
                /* Last one out does the cleanup. */
                if (!ASMAtomicDecU32(&pEventMultiInt->cWaking))
                {
                    lck_spin_unlock(pEventMultiInt->pSpinlock);
                    Assert(g_pDarwinLockGroup);
                    lck_spin_destroy(pEventMultiInt->pSpinlock, g_pDarwinLockGroup);
                    RTMemFree(pEventMultiInt);
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

    lck_spin_unlock(pEventMultiInt->pSpinlock);
    return rc;
}


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(EventMultiSem, cMillies, THREAD_UNINT);
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(EventMultiSem, cMillies, THREAD_ABORTSAFE);
}





#if 0 /* need proper timeout lock function! */
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    AssertCompile(sizeof(RTSEMMUTEXINTERNAL) > sizeof(void *));
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)RTMemAlloc(sizeof(*pMutexInt));
    if (pMutexInt)
    {
        pMutexInt->u32Magic = RTSEMMUTEX_MAGIC;
        Assert(g_pDarwinLockGroup);
        pMutexInt->pMtx = lck_mtx_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pMutexInt->pMtx)
        {
            *pMutexSem = pMutexInt;
            return VINF_SUCCESS;
        }
        RTMemFree(pMutexInt);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (!pMutexInt)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pMutexInt, VERR_INVALID_POINTER);
    AssertMsg(pMutexInt->u32Magic == RTSEMMUTEX_MAGIC,
              ("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt->u32Magic, pMutexInt)
              VERR_INVALID_PARAMETER);

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicIncU32(&pMutexInt->u32Magic);

    Assert(g_pDarwinLockGroup);
    lck_mtx_free(pMutexInt->pMtx, g_pDarwinLockGroup);
    pMutexInt->pMtx = NULL;

    RTMemFree(pMutexInt);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (!pMutexInt)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pMutexInt, VERR_INVALID_POINTER);
    AssertMsg(pMutexInt->u32Magic == RTSEMMUTEX_MAGIC,
              ("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt->u32Magic, pMutexInt)
              VERR_INVALID_PARAMETER);

    /*
     * Get the mutex.
     */
    wait_result_t rc = lck_mtx_lock_deadlink
#if 1
#else
    NTSTATUS rcNt;
    if (cMillies == RT_INDEFINITE_WAIT)
        rcNt = KeWaitForSingleObject(&pMutexInt->Mutex, Executive, KernelMode, TRUE, NULL);
    else
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -(int64_t)cMillies * 10000;
        rcNt = KeWaitForSingleObject(&pMutexInt->Mutex, Executive, KernelMode, TRUE, &Timeout);
    }
    switch (rcNt)
    {
        case STATUS_SUCCESS:
            if (pMutexInt->u32Magic == RTSEMMUTEX_MAGIC)
                return VINF_SUCCESS;
            return VERR_SEM_DESTROYED;
        case STATUS_ALERTED:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_USER_APC:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_TIMEOUT:
            return VERR_TIMEOUT;
        default:
            AssertMsgFailed(("pMutexInt->u32Magic=%RX32 pMutexInt=%p: wait returned %lx!\n",
                             pMutexInt->u32Magic, pMutexInt, (long)rcNt));
            return VERR_INTERNAL_ERROR;
    }
#endif
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (!pMutexInt)
        return VERR_INVALID_PARAMETER;
    if (    !pMutexInt
        ||  pMutexInt->u32Magic != RTSEMMUTEX_MAGIC)
    {
        AssertMsgFailed(("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt ? pMutexInt->u32Magic : 0, pMutexInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Release the mutex.
     */
#ifdef RT_USE_FAST_MUTEX
    ExReleaseFastMutex(&pMutexInt->Mutex);
#else
    KeReleaseMutex(&pMutexInt->Mutex, FALSE);
#endif
    return VINF_SUCCESS;
}

#endif /* later */




RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem)
{
    AssertCompile(sizeof(RTSEMFASTMUTEXINTERNAL) > sizeof(void *));
    AssertPtrReturn(pMutexSem, VERR_INVALID_POINTER);

    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)RTMemAlloc(sizeof(*pFastInt));
    if (pFastInt)
    {
        pFastInt->u32Magic = RTSEMFASTMUTEX_MAGIC;
        Assert(g_pDarwinLockGroup);
        pFastInt->pMtx = lck_mtx_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pFastInt->pMtx)
        {
            *pMutexSem = pFastInt;
            return VINF_SUCCESS;
        }

        RTMemFree(pFastInt);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem)
{
    if (MutexSem == NIL_RTSEMFASTMUTEX) /* don't bitch */
        return VERR_INVALID_PARAMETER;
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    ASMAtomicIncU32(&pFastInt->u32Magic); /* make the handle invalid. */
    Assert(g_pDarwinLockGroup);
    lck_mtx_free(pFastInt->pMtx, g_pDarwinLockGroup);
    pFastInt->pMtx = NULL;
    RTMemFree(pFastInt);

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);
    lck_mtx_lock(pFastInt->pMtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pFastInt, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);
    lck_mtx_unlock(pFastInt->pMtx);
    return VINF_SUCCESS;
}

