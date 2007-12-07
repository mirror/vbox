/* $Id$ */
/** @file
 * innotek Portable Runtime - Semaphores, POSIX.
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

#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#ifdef RT_OS_DARWIN
# define pthread_yield() pthread_yield_np()
#endif

#ifdef RT_OS_SOLARIS
# include <sched.h>
# define pthread_yield() sched_yield()
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/** Internal representation of the POSIX implementation of an Event semaphore.
 * The POSIX implementation uses a mutex and a condition variable to implement
 * the automatic reset event semaphore semantics.
 *
 * This must be identical to RTSEMEVENTMULTIINTERNAL!
 */
struct RTSEMEVENTINTERNAL
{
    /** pthread condition. */
    pthread_cond_t      Cond;
    /** pthread mutex which protects the condition and the event state. */
    pthread_mutex_t     Mutex;
    /** The state of the semaphore.
     * This is operated while owning mutex and using atomic updating. */
    volatile uint32_t   u32State;
    /** Number of waiters. */
    volatile uint32_t   cWaiters;
};

/** Posix internal representation of a Mutex Multi semaphore.
 * This must be identical to RTSEMEVENTINTERNAL! */
struct RTSEMEVENTMULTIINTERNAL
{
    /** pthread condition. */
    pthread_cond_t      Cond;
    /** pthread mutex which protects the condition and the event state. */
    pthread_mutex_t     Mutex;
    /** The state of the semaphore.
     * This is operated while owning mutex and using atomic updating. */
    volatile uint32_t   u32State;
    /** Number of waiters. */
    volatile uint32_t   cWaiters;
};

/** The valus of the u32State variable in a RTSEMEVENTINTERNAL and RTSEMEVENTMULTIINTERNAL.
 * @{ */
/** The object isn't initialized. */
#define EVENT_STATE_UNINITIALIZED   0
/** The semaphore is is signaled. */
#define EVENT_STATE_SIGNALED        0xff00ff00
/** The semaphore is not signaled. */
#define EVENT_STATE_NOT_SIGNALED    0x00ff00ff
/** @} */


/** Posix internal representation of a Mutex semaphore. */
struct RTSEMMUTEXINTERNAL
{
    /** pthread mutex. */
    pthread_mutex_t     Mutex;
    /** The owner of the mutex. */
    volatile pthread_t  Owner;
    /** Nesting count. */
    volatile uint32_t   cNesting;
};

/** Posix internal representation of a read-write semaphore. */
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
 * Validate an Event semaphore handle passed to one of the interface.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pIntEventSem    Pointer to the event semaphore to validate.
 */
inline bool rtsemEventValid(struct RTSEMEVENTINTERNAL *pIntEventSem)
{
    if ((uintptr_t)pIntEventSem < 0x10000)
        return false;

    uint32_t    u32 = pIntEventSem->u32State; /* this is volatile, so a explicit read like this is needed. */
    if (    u32 != EVENT_STATE_NOT_SIGNALED
        &&  u32 != EVENT_STATE_SIGNALED)
        return false;

    return true;
}


RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    int rc;

    /*
     * Allocate semaphore handle.
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = (struct RTSEMEVENTINTERNAL *)RTMemAlloc(sizeof(struct RTSEMEVENTINTERNAL));
    if (pIntEventSem)
    {
        /*
         * Create the condition variable.
         */
        pthread_condattr_t CondAttr;
        rc = pthread_condattr_init(&CondAttr);
        if (!rc)
        {
            rc = pthread_cond_init(&pIntEventSem->Cond, &CondAttr);
            if (!rc)
            {
                /*
                 * Create the semaphore.
                 */
                pthread_mutexattr_t MutexAttr;
                rc = pthread_mutexattr_init(&MutexAttr);
                if (!rc)
                {
                    rc = pthread_mutex_init(&pIntEventSem->Mutex, &MutexAttr);
                    if (!rc)
                    {
                        pthread_mutexattr_destroy(&MutexAttr);
                        pthread_condattr_destroy(&CondAttr);

                        ASMAtomicXchgU32(&pIntEventSem->u32State, EVENT_STATE_NOT_SIGNALED);
                        ASMAtomicXchgU32(&pIntEventSem->cWaiters, 0);

                        *pEventSem = pIntEventSem;
                        return VINF_SUCCESS;
                    }
                    pthread_mutexattr_destroy(&MutexAttr);
                }
                pthread_cond_destroy(&pIntEventSem->Cond);
            }
            pthread_condattr_destroy(&CondAttr);
        }

        rc = RTErrConvertFromErrno(rc);
        RTMemFree(pIntEventSem);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    /*
     * Validate handle.
     */
    if (!rtsemEventValid(EventSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", EventSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Abort all waiters forcing them to return failure.
     *
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = EventSem;
    int rc;
    for (int i = 30; i > 0; i--)
    {
        ASMAtomicXchgU32(&pIntEventSem->u32State, EVENT_STATE_UNINITIALIZED);
        rc = pthread_cond_destroy(&pIntEventSem->Cond);
        if (rc != EBUSY)
            break;
        pthread_cond_broadcast(&pIntEventSem->Cond);
        usleep(1000);
    } while (rc == EBUSY);
    if (rc)
    {
        AssertMsgFailed(("Failed to destroy event sem %p, rc=%d.\n", EventSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Destroy the semaphore
     * If it's busy we'll wait a bit to give the threads a chance to be scheduled.
     */
    for (int i = 30; i > 0; i--)
    {
        rc = pthread_mutex_destroy(&pIntEventSem->Mutex);
        if (rc != EBUSY)
            break;
        usleep(1000);
    }
    if (rc)
    {
        AssertMsgFailed(("Failed to destroy event sem %p, rc=%d. (mutex)\n", EventSem, rc));
        return RTErrConvertFromErrno(rc);
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
    if (!rtsemEventValid(EventSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", EventSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Lock the mutex semaphore.
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = EventSem;
    int rc = pthread_mutex_lock(&pIntEventSem->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to lock event sem %p, rc=%d.\n", EventSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Check the state.
     */
    if (pIntEventSem->u32State == EVENT_STATE_NOT_SIGNALED)
    {
        ASMAtomicXchgU32(&pIntEventSem->u32State, EVENT_STATE_SIGNALED);
        rc = pthread_cond_signal(&pIntEventSem->Cond);
        AssertMsg(!rc, ("Failed to signal event sem %p, rc=%d.\n", EventSem, rc));
    }
    else if (pIntEventSem->u32State == EVENT_STATE_SIGNALED)
    {
        rc = pthread_cond_signal(&pIntEventSem->Cond); /* give'm another kick... */
        AssertMsg(!rc, ("Failed to signal event sem %p, rc=%d. (2)\n", EventSem, rc));
    }
    else
        rc = VERR_SEM_DESTROYED;

    /*
     * Release the mutex and return.
     */
    int rc2 = pthread_mutex_unlock(&pIntEventSem->Mutex);
    AssertMsg(!rc2, ("Failed to unlock event sem %p, rc=%d.\n", EventSem, rc));
    if (rc)
        return RTErrConvertFromErrno(rc);
    if (rc2)
        return RTErrConvertFromErrno(rc2);

    return VINF_SUCCESS;
}


static int  rtSemEventWait(RTSEMEVENT EventSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    if (!rtsemEventValid(EventSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", EventSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Timed or indefinite wait?
     */
    struct RTSEMEVENTINTERNAL *pIntEventSem = EventSem;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* for fairness, yield before going to sleep. */
        if (    ASMAtomicIncU32(&pIntEventSem->cWaiters) > 1
            &&  pIntEventSem->u32State == EVENT_STATE_SIGNALED)
            pthread_yield();

         /* take mutex */
        int rc = pthread_mutex_lock(&pIntEventSem->Mutex);
        if (rc)
        {
            ASMAtomicDecU32(&pIntEventSem->cWaiters);
            AssertMsgFailed(("Failed to lock event sem %p, rc=%d.\n", EventSem, rc));
            return RTErrConvertFromErrno(rc);
        }

        for (;;)
        {
            /* check state. */
            if (pIntEventSem->u32State == EVENT_STATE_SIGNALED)
            {
                ASMAtomicXchgU32(&pIntEventSem->u32State, EVENT_STATE_NOT_SIGNALED);
                ASMAtomicDecU32(&pIntEventSem->cWaiters);
                rc = pthread_mutex_unlock(&pIntEventSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event sem %p, rc=%d.\n", EventSem, rc)); NOREF(rc);
                return VINF_SUCCESS;
            }
            if (pIntEventSem->u32State == EVENT_STATE_UNINITIALIZED)
            {
                rc = pthread_mutex_unlock(&pIntEventSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event sem %p, rc=%d.\n", EventSem, rc)); NOREF(rc);
                return VERR_SEM_DESTROYED;
            }

            /* wait */
            rc = pthread_cond_wait(&pIntEventSem->Cond, &pIntEventSem->Mutex);
            if (rc)
            {
                AssertMsgFailed(("Failed to wait on event sem %p, rc=%d.\n", EventSem, rc));
                ASMAtomicDecU32(&pIntEventSem->cWaiters);
                int rc2 = pthread_mutex_unlock(&pIntEventSem->Mutex);
                AssertMsg(!rc2, ("Failed to unlock event sem %p, rc=%d.\n", EventSem, rc2)); NOREF(rc2);
                return RTErrConvertFromErrno(rc);
            }
        }
    }
    else
    {
        /*
         * Get current time and calc end of wait time.
         */
        struct timespec     ts = {0,0};
#ifdef RT_OS_DARWIN
        struct timeval      tv = {0,0};
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
#else
        clock_gettime(CLOCK_REALTIME, &ts);
#endif
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

        /* for fairness, yield before going to sleep. */
        if (ASMAtomicIncU32(&pIntEventSem->cWaiters) > 1)
            pthread_yield();

        /* take mutex */
#ifdef RT_OS_DARWIN
        int rc = pthread_mutex_lock(&pIntEventSem->Mutex);
#else
        int rc = pthread_mutex_timedlock(&pIntEventSem->Mutex, &ts);
#endif
        if (rc)
        {
            ASMAtomicDecU32(&pIntEventSem->cWaiters);
            AssertMsg(rc == ETIMEDOUT, ("Failed to lock event sem %p, rc=%d.\n", EventSem, rc));
            return RTErrConvertFromErrno(rc);
        }

        for (;;)
        {
            /* check state. */
            if (pIntEventSem->u32State == EVENT_STATE_SIGNALED)
            {
                ASMAtomicXchgU32(&pIntEventSem->u32State, EVENT_STATE_NOT_SIGNALED);
                ASMAtomicDecU32(&pIntEventSem->cWaiters);
                rc = pthread_mutex_unlock(&pIntEventSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event sem %p, rc=%d.\n", EventSem, rc)); NOREF(rc);
                return VINF_SUCCESS;
            }
            if (pIntEventSem->u32State == EVENT_STATE_UNINITIALIZED)
            {
                rc = pthread_mutex_unlock(&pIntEventSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event sem %p, rc=%d.\n", EventSem, rc)); NOREF(rc);
                return VERR_SEM_DESTROYED;
            }

            /* wait */
            rc = pthread_cond_timedwait(&pIntEventSem->Cond, &pIntEventSem->Mutex, &ts);
            if (rc && (rc != EINTR || !fAutoResume)) /* according to SuS this function shall not return EINTR, but linux man page says differently. */
            {
                AssertMsg(rc == ETIMEDOUT, ("Failed to wait on event sem %p, rc=%d.\n", EventSem, rc));
                ASMAtomicDecU32(&pIntEventSem->cWaiters);
                int rc2 = pthread_mutex_unlock(&pIntEventSem->Mutex);
                AssertMsg(!rc2, ("Failed to unlock event sem %p, rc2=%d.\n", EventSem, rc2)); NOREF(rc2);
                return RTErrConvertFromErrno(rc);
            }
        } /* for (;;) */
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






/**
 * Validate an event multi semaphore handle passed to one of the interface.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pIntEventMultiSem    Pointer to the event semaphore to validate.
 */
inline bool rtsemEventMultiValid(struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem)
{
    if ((uintptr_t)pIntEventMultiSem < 0x10000)
        return false;

    uint32_t    u32 = pIntEventMultiSem->u32State; /* this is volatile, so a explicit read like this is needed. */
    if (    u32 != EVENT_STATE_NOT_SIGNALED
        &&  u32 != EVENT_STATE_SIGNALED)
        return false;

    return true;
}


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    /* the code and the structure is identical with other type for this function. */
    return RTSemEventCreate((PRTSEMEVENT)pEventMultiSem);
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /* the code and the structure is identical with other type for this function. */
    return RTSemEventDestroy((RTSEMEVENT)EventMultiSem);
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    /* the code and the structure is identical with other type for this function. */
    return RTSemEventSignal((RTSEMEVENT)EventMultiSem);
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Validate input.
     */
    if (!rtsemEventMultiValid(EventMultiSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", EventMultiSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Lock the mutex semaphore.
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = EventMultiSem;
    int rc = pthread_mutex_lock(&pIntEventMultiSem->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to lock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Check the state.
     */
    if (pIntEventMultiSem->u32State == EVENT_STATE_SIGNALED)
        ASMAtomicXchgU32(&pIntEventMultiSem->u32State, EVENT_STATE_NOT_SIGNALED);
    else if (pIntEventMultiSem->u32State != EVENT_STATE_NOT_SIGNALED)
        rc = VERR_SEM_DESTROYED;

    /*
     * Release the mutex and return.
     */
    rc = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;

}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    if (!rtsemEventMultiValid(EventMultiSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", EventMultiSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Timed or indefinite wait?
     */
    struct RTSEMEVENTMULTIINTERNAL *pIntEventMultiSem = EventMultiSem;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take mutex */
        int rc = pthread_mutex_lock(&pIntEventMultiSem->Mutex);
        if (rc)
        {
            AssertMsgFailed(("Failed to lock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
            return RTErrConvertFromErrno(rc);
        }
        ASMAtomicIncU32(&pIntEventMultiSem->cWaiters);

        for (;;)
        {
            /* check state. */
            if (pIntEventMultiSem->u32State == EVENT_STATE_SIGNALED)
            {
                ASMAtomicDecU32(&pIntEventMultiSem->cWaiters);
                rc = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VINF_SUCCESS;
            }
            if (pIntEventMultiSem->u32State == EVENT_STATE_UNINITIALIZED)
            {
                rc = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VERR_SEM_DESTROYED;
            }

            /* wait */
            rc = pthread_cond_wait(&pIntEventMultiSem->Cond, &pIntEventMultiSem->Mutex);
            if (rc)
            {
                AssertMsgFailed(("Failed to wait on event multi sem %p, rc=%d.\n", EventMultiSem, rc));
                ASMAtomicDecU32(&pIntEventMultiSem->cWaiters);
                int rc2 = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
                AssertMsg(!rc2, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc2)); NOREF(rc2);
                return RTErrConvertFromErrno(rc);
            }
        }
    }
    else
    {
        /*
         * Get current time and calc end of wait time.
         */
        struct timespec     ts = {0,0};
#ifdef RT_OS_DARWIN
        struct timeval      tv = {0,0};
        gettimeofday(&tv, NULL);
        ts.tv_sec = tv.tv_sec;
        ts.tv_nsec = tv.tv_usec * 1000;
#else
        clock_gettime(CLOCK_REALTIME, &ts);
#endif
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
#ifdef RT_OS_DARWIN
        int rc = pthread_mutex_lock(&pIntEventMultiSem->Mutex);
#else
        int rc = pthread_mutex_timedlock(&pIntEventMultiSem->Mutex, &ts);
#endif
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed to lock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
            return RTErrConvertFromErrno(rc);
        }
        ASMAtomicIncU32(&pIntEventMultiSem->cWaiters);

        for (;;)
        {
            /* check state. */
            if (pIntEventMultiSem->u32State == EVENT_STATE_SIGNALED)
            {
                ASMAtomicXchgU32(&pIntEventMultiSem->u32State, EVENT_STATE_NOT_SIGNALED);
                ASMAtomicDecU32(&pIntEventMultiSem->cWaiters);
                rc = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VINF_SUCCESS;
            }
            if (pIntEventMultiSem->u32State == EVENT_STATE_UNINITIALIZED)
            {
                rc = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VERR_SEM_DESTROYED;
            }

            /* wait */
            rc = pthread_cond_timedwait(&pIntEventMultiSem->Cond, &pIntEventMultiSem->Mutex, &ts);
            if (rc && (rc != EINTR || !fAutoResume)) /* according to SuS this function shall not return EINTR, but linux man page says differently. */
            {
                AssertMsg(rc == ETIMEDOUT, ("Failed to wait on event multi sem %p, rc=%d.\n", EventMultiSem, rc));
                ASMAtomicDecU32(&pIntEventMultiSem->cWaiters);
                int rc2 = pthread_mutex_unlock(&pIntEventMultiSem->Mutex);
                AssertMsg(!rc2, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc2)); NOREF(rc2);
                return RTErrConvertFromErrno(rc);
            }
        }
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

    if (pIntMutexSem->cNesting == (uint32_t)~0)
        return false;

    return true;
}


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

                pIntMutexSem->Owner    = (pthread_t)-1;
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
    pIntMutexSem->Owner    = (pthread_t)-1;
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
    pthread_t                     Self = pthread_self();
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
#ifdef RT_OS_DARWIN
        AssertMsgFailed(("Not implemented on Darwin yet because of incomplete pthreads API."));
        return VERR_NOT_IMPLEMENTED;
#else /* !RT_OS_DARWIN */
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
#endif /* !RT_OS_DARWIN */
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
    pthread_t                     Self = pthread_self();
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
    pIntMutexSem->Owner    = (pthread_t)-1;
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
                pIntRWSem->WROwner = (pthread_t)-1;
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
#ifdef RT_OS_DARWIN
        AssertMsgFailed(("Not implemented on Darwin yet because of incomplete pthreads API."));
        return VERR_NOT_IMPLEMENTED;
#else /* !RT_OS_DARWIN */
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
#endif /* !RT_OS_DARWIN */
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
#ifdef RT_OS_DARWIN
        AssertMsgFailed(("Not implemented on Darwin yet because of incomplete pthreads API."));
        return VERR_NOT_IMPLEMENTED;
#else /* !RT_OS_DARWIN */
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
#endif /* !RT_OS_DARWIN */
    }

#ifdef RT_OS_SOLARIS
    ASMAtomicXchgSize(&pIntRWSem->WROwner, pthread_self());
#else
    ASMAtomicXchgPtr((void * volatile *)&pIntRWSem->WROwner, (void *)pthread_self());
#endif

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
    pthread_t                 Self = pthread_self();
    struct RTSEMRWINTERNAL   *pIntRWSem = RWSem;
    if (pIntRWSem->WROwner != Self)
    {
        AssertMsgFailed(("Not Write owner!\n"));
        return VERR_NOT_OWNER;
    }

    /*
     * Try unlock it.
     */
#ifdef RT_OS_SOLARIS
    ASMAtomicXchgSize(&pIntRWSem->WROwner, (pthread_t)-1);
#else
    AssertMsg(sizeof(pthread_t) == sizeof(void *), ("pthread_t is not the size of a pointer but %d bytes\n", sizeof(pthread_t)));
    ASMAtomicXchgPtr((void * volatile *)&pIntRWSem->WROwner, (void *)(pthread_t)-1);
#endif
    int rc = pthread_rwlock_unlock(&pIntRWSem->RWLock);
    if (rc)
    {
        AssertMsgFailed(("Failed write unlock read-write sem %p, rc=%d.\n", RWSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}

