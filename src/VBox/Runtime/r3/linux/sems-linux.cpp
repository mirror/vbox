/* $Id$ */
/** @file
 * innotek Portable Runtime - Semaphores, Linux (AMD64 only ATM).
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <iprt/semaphore.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include "internal/magics.h"

#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/syscall.h>
#if 0 /* With 2.6.17 futex.h has become C++ unfriendly. */
# include <linux/futex.h>
#else
# define FUTEX_WAIT 0
# define FUTEX_WAKE 1
#endif 


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Linux (single wakup) event semaphore. 
 */
struct RTSEMEVENTINTERNAL
{
    /** Magic value. */
    intptr_t volatile   iMagic;
    /** The futex state variable.
     * <0 means signaled. 
     *  0 means not signaled, no waiters.
     * >0 means not signaled, and the value gives the number of waiters. 
     */
    int32_t volatile    cWaiters;
};


/** 
 * Linux multiple wakup event semaphore. 
 */
struct RTSEMEVENTMULTIINTERNAL
{
    /** Magic value. */
    intptr_t volatile   iMagic;
    /** The futex state variable.
     * -1 means signaled. 
     *  0 means not signaled, no waiters.
     * >0 means not signaled, and the value gives the number of waiters. 
     */
    int32_t volatile    iState;
};


#ifndef VBOX_REWRITTEN_MUTEX
/**
 * Posix internal representation of a Mutex semaphore. 
 */
struct RTSEMMUTEXINTERNAL
{
    /** pthread mutex. */
    pthread_mutex_t     Mutex;
    /** The owner of the mutex. */
    volatile pthread_t  Owner;
    /** Nesting count. */
    volatile uint32_t   cNesting;
};
#else /* VBOX_REWRITTEN_MUTEX */
/**
 * Linux internal representation of a Mutex semaphore.
 */
struct RTSEMMUTEXINTERNAL
{
    /** Magic value. */
    intptr_t volatile   iMagic;
    /** The futex state variable.
     * 0 means unlocked.
     * 1 means locked, no waiters.
     * 2 means locked, one or more waiters.
     */
    int32_t volatile    iState;
    /** The owner of the mutex. */
    volatile pthread_t  Owner;
    /** Nesting count. */
    volatile uint32_t   cNesting;
};
#endif /* VBOX_REWRITTEN_MUTEX */


/** 
 * Posix internal representation of a read-write semaphore. 
 */
struct RTSEMRWINTERNAL
{
    /** pthread rwlock. */
    pthread_rwlock_t    RWLock;
    /** Variable to check if initialized.
     * 0 is uninitialized, ~0 is inititialized. */
    volatile unsigned   uCheck;
    /** The write owner of the lock. */
    volatile pthread_t  WROwner;
};


/**
 * Wrapper for the futex syscall.
 */
static long sys_futex(int32_t volatile *uaddr, int op, int val, struct timespec *utime, int32_t *uaddr2, int val3)
{
    errno = 0;
    long rc = syscall(__NR_futex, uaddr, op, val, utime, uaddr2, val3);
    if (rc < 0)
    {
        Assert(rc == -1);
        rc = -errno;
    }
    return rc;
}



RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    /*
     * Allocate semaphore handle.
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = (struct RTSEMEVENTINTERNAL *)RTMemAlloc(sizeof(struct RTSEMEVENTINTERNAL));
    if (pIntEventSem)
    {
        pIntEventSem->iMagic = RTSEMEVENT_MAGIC;
        pIntEventSem->cWaiters = 0;
        *pEventSem = pIntEventSem;
        return VINF_SUCCESS;
    }
    return  VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = EventSem;
    AssertReturn(VALID_PTR(pIntEventSem) && pIntEventSem->iMagic == RTSEMEVENT_MAGIC, 
                 VERR_INVALID_HANDLE);

    /*
     * Invalidate the semaphore and wake up anyone waiting on it.
     */
    ASMAtomicXchgSize(&pIntEventSem->iMagic, RTSEMEVENT_MAGIC + 1);
    if (ASMAtomicXchgS32(&pIntEventSem->cWaiters, INT32_MIN / 2) > 0)
    {
        sys_futex(&pIntEventSem->cWaiters, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        usleep(1000);
    }

    /*
     * Free the semaphore memory and be gone.
     */
    RTMemFree(pIntEventSem);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = EventSem;
    AssertReturn(VALID_PTR(pIntEventSem) && pIntEventSem->iMagic == RTSEMEVENT_MAGIC, 
                 VERR_INVALID_HANDLE);
    /*
     * Try signal it.
     */
    for (unsigned i = 0;; i++)
    {
        int32_t iCur = pIntEventSem->cWaiters;
        if (iCur == 0)
        {
            if (ASMAtomicCmpXchgS32(&pIntEventSem->cWaiters, -1, 0))
                break; /* nobody is waiting */
        }
        else if (iCur < 0)
            break; /* already signaled */
        else 
        {
            /* somebody is waiting, try wake up one of them. */
            long cWoken = sys_futex(&pIntEventSem->cWaiters, FUTEX_WAKE, 1, NULL, NULL, 0);
            if (RT_LIKELY(cWoken == 1))
            {
                ASMAtomicDecS32(&pIntEventSem->cWaiters);
                break;
            }
            AssertMsg(cWoken == 0, ("%ld\n", cWoken));

            /* 
             * This path is taken in two situations:
             *      1) A waiting thread is returning from the sys_futex call with a 
             *         non-zero return value.
             *      2) There are two threads signaling the event at the 
             *         same time and only one thread waiting.
             *
             * At this point we know that nobody is activly waiting on the event but 
             * at the same time, we are racing someone updating the state. The current 
             * strategy is to spin till the thread racing us is done, this is kind of 
             * brain dead and need fixing of course.
             */
            if (RT_UNLIKELY(i > 32))
            {
                if ((i % 128) == 127)
                    usleep(1000);
                else if (!(i % 4))
                    pthread_yield();
                else
                    AssertReleaseMsg(i < 4096, ("iCur=%#x pIntEventSem=%p\n", iCur, pIntEventSem));
            }
        }
    }
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT EventSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = EventSem;
    AssertReturn(VALID_PTR(pIntEventSem) && pIntEventSem->iMagic == RTSEMEVENT_MAGIC, 
                 VERR_INVALID_HANDLE);

    /*
     * Quickly check whether it's signaled.
     */
    if (ASMAtomicCmpXchgS32(&pIntEventSem->cWaiters, 0, -1))
        return VINF_SUCCESS;

    /*
     * Convert timeout value.
     */
    struct timespec ts;
    struct timespec *pTimeout = 0;
    if (cMillies != RT_INDEFINITE_WAIT)
    {
        ts.tv_sec  = cMillies / 1000;
        ts.tv_nsec = (cMillies % 1000) * 1000000;
        pTimeout = &ts;
    }

    /*
     * The wait loop.
     */
    for (unsigned i = 0;; i++)
    {
        /* 
         * Announce that we're among the waiters. 
         */
        int32_t iNew = ASMAtomicIncS32(&pIntEventSem->cWaiters);
        if (iNew == 0)
            return VINF_SUCCESS;
        if (RT_LIKELY(iNew > 0))
        {
            /*
             * Go to sleep.
             */
            long rc = sys_futex(&pIntEventSem->cWaiters, FUTEX_WAIT, iNew, pTimeout, NULL, 0);
            if (RT_UNLIKELY(pIntEventSem->iMagic != RTSEMEVENT_MAGIC))
                return VERR_SEM_DESTROYED;

            /* Did somebody wake us up from RTSemEventSignal()? */
            if (rc == 0)
                return VINF_SUCCESS; 

            /* No, then the kernel woke us up or we failed going to sleep. Adjust the accounting. */
            iNew = ASMAtomicDecS32(&pIntEventSem->cWaiters);
            Assert(iNew >= 0);

            /* 
             * Act on the wakup code.
             */
            if (rc == -ETIMEDOUT)
            {
                Assert(pTimeout);
                return VERR_TIMEOUT;
            } 
            if (rc == -EWOULDBLOCK)
                /* retry with new value. */;
            else if (rc == -EINTR)
            {
                if (!fAutoResume)
                    return VERR_INTERRUPTED;
            }
            else
            {
                /* this shouldn't happen! */
                AssertMsgFailed(("rc=%ld errno=%d\n", rc, errno));
                return RTErrConvertFromErrno(rc);
            }
        }
        else
        {
            /* this can't happen. */
            if (RT_UNLIKELY(pIntEventSem->iMagic != RTSEMEVENT_MAGIC))
                return VERR_SEM_DESTROYED;
            AssertReleaseMsgFailed(("iNew=%d\n", iNew));
        }
    }
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    int rc = rtSemEventWait(EventSem, cMillies, true);
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, false);
}





RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    /*
     * Allocate semaphore handle.
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = (struct RTSEMEVENTMULTIINTERNAL *)RTMemAlloc(sizeof(struct RTSEMEVENTMULTIINTERNAL));
    if (pIntEventMultiSem)
    {
        pIntEventMultiSem->iMagic = RTSEMEVENTMULTI_MAGIC;
        pIntEventMultiSem->iState = 0;
        *pEventMultiSem = pIntEventMultiSem;
        return VINF_SUCCESS;
    }
    return  VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = EventMultiSem;
    AssertReturn(VALID_PTR(pIntEventMultiSem) && pIntEventMultiSem->iMagic == RTSEMEVENTMULTI_MAGIC, 
                 VERR_INVALID_HANDLE);

    /*
     * Invalidate the semaphore and wake up anyone waiting on it.
     */
    ASMAtomicXchgSize(&pIntEventMultiSem->iMagic, RTSEMEVENTMULTI_MAGIC + 1);
    if (ASMAtomicXchgS32(&pIntEventMultiSem->iState, -1) == 1)
    {
        sys_futex(&pIntEventMultiSem->iState, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        usleep(1000);
    }

    /*
     * Free the semaphore memory and be gone.
     */
    RTMemFree(pIntEventMultiSem);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = EventMultiSem;
    AssertReturn(VALID_PTR(pIntEventMultiSem) && pIntEventMultiSem->iMagic == RTSEMEVENTMULTI_MAGIC, 
                 VERR_INVALID_HANDLE);
    /*
     * Signal it.
     */
    int32_t iOld = ASMAtomicXchgS32(&pIntEventMultiSem->iState, -1);
    if (iOld > 0)
    {
        /* wake up sleeping threads. */
        long cWoken = sys_futex(&pIntEventMultiSem->iState, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        AssertMsg(cWoken >= 0, ("%ld\n", cWoken)); NOREF(cWoken);
    }
    Assert(iOld == 0 || iOld == -1 || iOld == 1);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = EventMultiSem;
    AssertReturn(VALID_PTR(pIntEventMultiSem) && pIntEventMultiSem->iMagic == RTSEMEVENTMULTI_MAGIC, 
                 VERR_INVALID_HANDLE);
#ifdef RT_STRICT
    int32_t i = pIntEventMultiSem->iState;
    Assert(i == 0 || i == -1 || i == 1);
#endif 

    /*
     * Reset it.
     */
    ASMAtomicCmpXchgS32(&pIntEventMultiSem->iState, 0, -1);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = EventMultiSem;
    AssertReturn(VALID_PTR(pIntEventMultiSem) && pIntEventMultiSem->iMagic == RTSEMEVENTMULTI_MAGIC, 
                 VERR_INVALID_HANDLE);

    /*
     * Quickly check whether it's signaled.
     */
    int32_t iCur = pIntEventMultiSem->iState;
    Assert(iCur == 0 || iCur == -1 || iCur == 1);
    if (iCur == -1)
        return VINF_SUCCESS;
    if (!cMillies)
        return VERR_TIMEOUT;

    /*
     * Convert timeout value.
     */
    struct timespec ts;
    struct timespec *pTimeout = NULL;
    if (cMillies != RT_INDEFINITE_WAIT)
    {
        ts.tv_sec  = cMillies / 1000;
        ts.tv_nsec = (cMillies % 1000) * 1000000;
        pTimeout = &ts;
    }

    /*
     * The wait loop.
     */
    for (unsigned i = 0;; i++)
    {
        /*
         * Start waiting. We only account for there being or having been 
         * threads waiting on the semaphore to keep things simple.
         */
        iCur = pIntEventMultiSem->iState;
        Assert(iCur == 0 || iCur == -1 || iCur == 1);
        if (    iCur == 1
            ||  ASMAtomicCmpXchgS32(&pIntEventMultiSem->iState, 1, 0))
        {
            long rc = sys_futex(&pIntEventMultiSem->iState, FUTEX_WAIT, 1, pTimeout, NULL, 0);
            if (RT_UNLIKELY(pIntEventMultiSem->iMagic != RTSEMEVENTMULTI_MAGIC))
                return VERR_SEM_DESTROYED;
            if (rc == 0)
                return VINF_SUCCESS; 

            /* 
             * Act on the wakup code.
             */
            if (rc == -ETIMEDOUT)
            {
                Assert(pTimeout);
                return VERR_TIMEOUT;
            } 
            if (rc == -EWOULDBLOCK)
                /* retry, the value changed. */;
            else if (rc == -EINTR)
            {
                if (!fAutoResume)
                    return VERR_INTERRUPTED;
            }
            else
            {
                /* this shouldn't happen! */
                AssertMsgFailed(("rc=%ld errno=%d\n", rc, errno));
                return RTErrConvertFromErrno(rc);
            }
        }
        else if (iCur == -1)
            return VINF_SUCCESS;
    }
}


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    int rc = rtSemEventMultiWait(EventMultiSem, cMillies, true);
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(EventMultiSem, cMillies, false);
}





/**
 * Validate a Mutex semaphore handle passed to one of the interface.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pIntMutexSem    Pointer to the mutex semaphore to validate.
 */
inline bool rtsemMutexValid(struct RTSEMMUTEXINTERNAL *pIntMutexSem)
{
    if ((uintptr_t)pIntMutexSem < 0x10000)
        return false;

#ifdef VBOX_REWRITTEN_MUTEX
    if (pIntMutexSem->iMagic != RTSEMMUTEX_MAGIC)
        return false;

#endif /* VBOX_REWRITTEN_MUTEX */
    if (pIntMutexSem->cNesting == (uint32_t)~0)
        return false;

    return true;
}


#ifndef VBOX_REWRITTEN_MUTEX
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    int rc;

    /*
     * Allocate semaphore handle.
     */
    struct RTSEMMUTEXINTERNAL *pIntMutexSem = (struct RTSEMMUTEXINTERNAL *)RTMemAlloc(sizeof(struct RTSEMMUTEXINTERNAL));
    if (pIntMutexSem)
    {
        /*
         * Create the semaphore.
         */
        pthread_mutexattr_t MutexAttr;
        rc = pthread_mutexattr_init(&MutexAttr);
        if (!rc)
        {
            rc = pthread_mutex_init(&pIntMutexSem->Mutex, &MutexAttr);
            if (!rc)
            {
                pthread_mutexattr_destroy(&MutexAttr);

                pIntMutexSem->Owner    = (pthread_t)~0;
                pIntMutexSem->cNesting = 0;

                *pMutexSem = pIntMutexSem;
                return VINF_SUCCESS;
            }
            pthread_mutexattr_destroy(&MutexAttr);
        }
        RTMemFree(pIntMutexSem);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    if (!rtsemMutexValid(MutexSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", MutexSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Try destroy it.
     */
    struct RTSEMMUTEXINTERNAL *pIntMutexSem = MutexSem;
    int rc = pthread_mutex_destroy(&pIntMutexSem->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to destroy mutex sem %p, rc=%d.\n", MutexSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Free the memory and be gone.
     */
    pIntMutexSem->Owner    = (pthread_t)~0;
    pIntMutexSem->cNesting = ~0;
    RTMemTmpFree(pIntMutexSem);

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    if (!rtsemMutexValid(MutexSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", MutexSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Check if nested request.
     */
    pthread_t                       Self = pthread_self();
    struct RTSEMMUTEXINTERNAL    *pIntMutexSem = MutexSem;
    if (    pIntMutexSem->Owner == Self
        &&  pIntMutexSem->cNesting > 0)
    {
        pIntMutexSem->cNesting++;
        return VINF_SUCCESS;
    }

    /*
     * Lock it.
     */
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take mutex */
        int rc = pthread_mutex_lock(&pIntMutexSem->Mutex);
        if (rc)
        {
            AssertMsgFailed(("Failed to lock mutex sem %p, rc=%d.\n", MutexSem, rc)); NOREF(rc);
            return RTErrConvertFromErrno(rc);
        }
    }
    else
    {
        /*
         * Get current time and calc end of wait time.
         */
        struct timespec     ts = {0,0};
        clock_gettime(CLOCK_REALTIME, &ts);
        if (cMillies != 0)
        {
            ts.tv_nsec += (cMillies % 1000) * 1000000;
            ts.tv_sec  += cMillies / 1000;
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_nsec -= 1000000000;
                ts.tv_sec++;
            }
        }

        /* take mutex */
        int rc = pthread_mutex_timedlock(&pIntMutexSem->Mutex, &ts);
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed to lock mutex sem %p, rc=%d.\n", MutexSem, rc)); NOREF(rc);
            return RTErrConvertFromErrno(rc);
        }
    }

    /*
     * Set the owner and nesting.
     */
    pIntMutexSem->Owner = Self;
    ASMAtomicXchgU32(&pIntMutexSem->cNesting, 1);

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    /* EINTR isn't returned by the wait functions we're using. */
    return RTSemMutexRequest(MutexSem, cMillies);
}


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    if (!rtsemMutexValid(MutexSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", MutexSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Check if nested.
     */
    pthread_t                       Self = pthread_self();
    struct RTSEMMUTEXINTERNAL    *pIntMutexSem = MutexSem;
    if (    pIntMutexSem->Owner != Self
        ||  pIntMutexSem->cNesting == (uint32_t)~0)
    {
        AssertMsgFailed(("Not owner of mutex %p!! Self=%08x Owner=%08x cNesting=%d\n",
                         pIntMutexSem, Self, pIntMutexSem->Owner, pIntMutexSem->cNesting));
        return VERR_NOT_OWNER;
    }

    /*
     * If nested we'll just pop a nesting.
     */
    if (pIntMutexSem->cNesting > 1)
    {
        pIntMutexSem->cNesting--;
        return VINF_SUCCESS;
    }

    /*
     * Clear the state. (cNesting == 1)
     */
    pIntMutexSem->Owner    = (pthread_t)~0;
    ASMAtomicXchgU32(&pIntMutexSem->cNesting, 0);

    /*
     * Unlock mutex semaphore.
     */
    int rc = pthread_mutex_unlock(&pIntMutexSem->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to unlock mutex sem %p, rc=%d.\n", MutexSem, rc)); NOREF(rc);
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}
#else /* VBOX_REWRITTEN_MUTEX */
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    /*
     * Allocate semaphore handle.
     */
    struct RTSEMMUTEXINTERNAL *pIntMutexSem = (struct RTSEMMUTEXINTERNAL *)RTMemAlloc(sizeof(struct RTSEMMUTEXINTERNAL));
    if (pIntMutexSem)
    {
        pIntMutexSem->iMagic   = RTSEMMUTEX_MAGIC;
        pIntMutexSem->iState   = 0;
        pIntMutexSem->Owner    = (pthread_t)~0;
        pIntMutexSem->cNesting = 0;

        *pMutexSem = pIntMutexSem;
        return VINF_SUCCESS;
    }

    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    struct RTSEMMUTEXINTERNAL *pIntMutexSem = MutexSem;
    /*
     * Validate input.
     */
    if (!rtsemMutexValid(pIntMutexSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", MutexSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Invalidate the semaphore and wake up anyone waiting on it.
     */
    ASMAtomicXchgSize(&pIntMutexSem->iMagic, RTSEMMUTEX_MAGIC + 1);
    if (ASMAtomicXchgS32(&pIntMutexSem->iState, 0) > 0)
    {
        sys_futex(&pIntMutexSem->iState, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        usleep(1000);
    }
    pIntMutexSem->Owner    = (pthread_t)~0;
    pIntMutexSem->cNesting = ~0;

    /*
     * Free the semaphore memory and be gone.
     */
    RTMemFree(pIntMutexSem);
    return VINF_SUCCESS;
}


static int rtsemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    struct RTSEMMUTEXINTERNAL *pIntMutexSem = MutexSem;
    if (!rtsemMutexValid(pIntMutexSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", MutexSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Check if nested request.
     */
    pthread_t Self = pthread_self();
    if (    pIntMutexSem->Owner == Self
        &&  pIntMutexSem->cNesting > 0)
    {
        pIntMutexSem->cNesting++;
        return VINF_SUCCESS;
    }

    /*
     * Convert timeout value.
     */
    struct timespec ts;
    struct timespec *pTimeout = NULL;
    if (cMillies != RT_INDEFINITE_WAIT)
    {
        ts.tv_sec  = cMillies / 1000;
        ts.tv_nsec = (cMillies % 1000) * 1000000;
        pTimeout = &ts;
    }

    /*
     * Lock the mutex.
     */
    int32_t iOld;
    ASMAtomicCmpXchgExS32(&pIntMutexSem->iState, 1, 0, &iOld);
    if (iOld != 0)
    {
	iOld = ASMAtomicXchgS32(&pIntMutexSem->iState, 2);
	while (iOld != 0)
	{
            /*
             * Go to sleep.
             */
            long rc = sys_futex(&pIntMutexSem->iState, FUTEX_WAIT, 2, pTimeout, NULL, 0);
            if (RT_UNLIKELY(pIntMutexSem->iMagic != RTSEMMUTEX_MAGIC))
                return VERR_SEM_DESTROYED;

            /*
             * Act on the wakup code.
             */
            if (rc == -ETIMEDOUT)
            {
                Assert(pTimeout);
	        iOld = ASMAtomicXchgS32(&pIntMutexSem->iState, 2);
                return VERR_TIMEOUT;
            }
	    if (rc == 0)
	        /* we'll leave the loop now unless another thread is faster */;
            else if (rc == -EWOULDBLOCK)
                /* retry with new value. */;
            else if (rc == -EINTR)
            {
                if (!fAutoResume)
                    return VERR_INTERRUPTED;
            }
            else
            {
                /* this shouldn't happen! */
                AssertMsgFailed(("rc=%ld errno=%d\n", rc, errno));
                return RTErrConvertFromErrno(rc);
            }

	    iOld = ASMAtomicXchgS32(&pIntMutexSem->iState, 2);
	}
    }

    /*
     * Set the owner and nesting.
     */
    pIntMutexSem->Owner = Self;
    ASMAtomicXchgU32(&pIntMutexSem->cNesting, 1);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    int rc = rtsemMutexRequest(MutexSem, cMillies, true);
    Assert(rc != VERR_INTERRUPTED);
    return rc;
}


RTDECL(int)  RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    return rtsemMutexRequest(MutexSem, cMillies, false);
}


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    struct RTSEMMUTEXINTERNAL *pIntMutexSem = MutexSem;
    if (!rtsemMutexValid(pIntMutexSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", MutexSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Check if nested.
     */
    pthread_t Self = pthread_self();
    if (    pIntMutexSem->Owner != Self
        ||  pIntMutexSem->cNesting == (uint32_t)~0)
    {
        AssertMsgFailed(("Not owner of mutex %p!! Self=%08x Owner=%08x cNesting=%d\n",
                         pIntMutexSem, Self, pIntMutexSem->Owner, pIntMutexSem->cNesting));
        return VERR_NOT_OWNER;
    }

    /*
     * If nested we'll just pop a nesting.
     */
    if (pIntMutexSem->cNesting > 1)
    {
        pIntMutexSem->cNesting--;
        return VINF_SUCCESS;
    }

    /*
     * Clear the state. (cNesting == 1)
     */
    pIntMutexSem->Owner = (pthread_t)~0;
    ASMAtomicXchgU32(&pIntMutexSem->cNesting, 0);

    /*
     * Release the mutex.
     */
    int32_t iNew = ASMAtomicDecS32(&pIntMutexSem->iState);
    if (iNew != 0)
    {
        /* somebody is waiting, try wake up one of them. */
        pIntMutexSem->iState = 0;
        (void)sys_futex(&pIntMutexSem->iState, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
    return VINF_SUCCESS;
}
#endif /* VBOX_REWRITTEN_MUTEX */




/**
 * Validate a read-write semaphore handle passed to one of the interface.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pIntRWSem   Pointer to the read-write semaphore to validate.
 */
inline bool rtsemRWValid(struct RTSEMRWINTERNAL *pIntRWSem)
{
    if ((uintptr_t)pIntRWSem < 0x10000)
        return false;

    if (pIntRWSem->uCheck != (unsigned)~0)
        return false;

    return true;
}


RTDECL(int)   RTSemRWCreate(PRTSEMRW pRWSem)
{
    int rc;

    /*
     * Allocate handle.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = (struct RTSEMRWINTERNAL *)RTMemAlloc(sizeof(struct RTSEMRWINTERNAL));
    if (pIntRWSem)
    {
        /*
         * Create the rwlock.
         */
        pthread_rwlockattr_t    Attr;
        rc = pthread_rwlockattr_init(&Attr);
        if (!rc)
        {
            rc = pthread_rwlock_init(&pIntRWSem->RWLock, &Attr);
            if (!rc)
            {
                pIntRWSem->uCheck = ~0;
                pIntRWSem->WROwner = (pthread_t)~0;
                *pRWSem = pIntRWSem;
                return VINF_SUCCESS;
            }
        }

        rc = RTErrConvertFromErrno(rc);
        RTMemFree(pIntRWSem);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int)   RTSemRWDestroy(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Try destroy it.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    int rc = pthread_rwlock_destroy(&pIntRWSem->RWLock);
    if (!rc)
    {
        pIntRWSem->uCheck = 0;
        RTMemFree(pIntRWSem);
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed(("Failed to destroy read-write sem %p, rc=%d.\n", RWSem, rc));
        rc = RTErrConvertFromErrno(rc);
    }

    return rc;
}


RTDECL(int)   RTSemRWRequestRead(RTSEMRW RWSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Try lock it.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take rwlock */
        int rc = pthread_rwlock_rdlock(&pIntRWSem->RWLock);
        if (rc)
        {
            AssertMsgFailed(("Failed read lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
    }
    else
    {
        /*
         * Get current time and calc end of wait time.
         */
        struct timespec     ts = {0,0};
        clock_gettime(CLOCK_REALTIME, &ts);
        if (cMillies != 0)
        {
            ts.tv_nsec += (cMillies % 1000) * 1000000;
            ts.tv_sec  += cMillies / 1000;
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_nsec -= 1000000000;
                ts.tv_sec++;
            }
        }

        /* take rwlock */
        int rc = pthread_rwlock_timedrdlock(&pIntRWSem->RWLock, &ts);
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed read lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
    }

    return VINF_SUCCESS;
}


RTDECL(int)   RTSemRWRequestReadNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    /* EINTR isn't returned by the wait functions we're using. */
    return RTSemRWRequestRead(RWSem, cMillies);
}


RTDECL(int)   RTSemRWReleaseRead(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Try unlock it.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    if (pIntRWSem->WROwner == pthread_self())
    {
        AssertMsgFailed(("Tried to read unlock when write owner - read-write sem %p.\n", RWSem));
        return VERR_NOT_OWNER;
    }
    int rc = pthread_rwlock_unlock(&pIntRWSem->RWLock);
    if (rc)
    {
        AssertMsgFailed(("Failed read unlock read-write sem %p, rc=%d.\n", RWSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}


RTDECL(int)   RTSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Try lock it.
     */
    struct RTSEMRWINTERNAL *pIntRWSem = RWSem;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take rwlock */
        int rc = pthread_rwlock_wrlock(&pIntRWSem->RWLock);
        if (rc)
        {
            AssertMsgFailed(("Failed write lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
    }
    else
    {
        /*
         * Get current time and calc end of wait time.
         */
        struct timespec     ts = {0,0};
        clock_gettime(CLOCK_REALTIME, &ts);
        if (cMillies != 0)
        {
            ts.tv_nsec += (cMillies % 1000) * 1000000;
            ts.tv_sec  += cMillies / 1000;
            if (ts.tv_nsec >= 1000000000)
            {
                ts.tv_nsec -= 1000000000;
                ts.tv_sec++;
            }
        }

        /* take rwlock */
        int rc = pthread_rwlock_timedwrlock(&pIntRWSem->RWLock, &ts);
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed read lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
    }

    ASMAtomicXchgPtr((void * volatile *)&pIntRWSem->WROwner, (void *)pthread_self());

    return VINF_SUCCESS;
}


RTDECL(int)   RTSemRWRequestWriteNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    /* EINTR isn't returned by the wait functions we're using. */
    return RTSemRWRequestWrite(RWSem, cMillies);
}


RTDECL(int)   RTSemRWReleaseWrite(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    if (!rtsemRWValid(RWSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", RWSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Try unlock it.
     */
    pthread_t                   Self = pthread_self();
    struct RTSEMRWINTERNAL   *pIntRWSem = RWSem;
    if (pIntRWSem->WROwner != Self)
    {
        AssertMsgFailed(("Not Write owner!\n"));
        return VERR_NOT_OWNER;
    }

    /*
     * Try unlock it.
     */
    AssertMsg(sizeof(pthread_t) == sizeof(void *), ("pthread_t is not the size of a pointer but %d bytes\n", sizeof(pthread_t)));
    ASMAtomicXchgPtr((void * volatile *)&pIntRWSem->WROwner, (void *)(uintptr_t)~0);
    int rc = pthread_rwlock_unlock(&pIntRWSem->RWLock);
    if (rc)
    {
        AssertMsgFailed(("Failed write unlock read-write sem %p, rc=%d.\n", RWSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}

