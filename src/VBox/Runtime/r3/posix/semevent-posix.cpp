/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Event Semaphore, POSIX.
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

/** The valus of the u32State variable in a RTSEMEVENTINTERNAL.
 * @{ */
/** The object isn't initialized. */
#define EVENT_STATE_UNINITIALIZED   0
/** The semaphore is is signaled. */
#define EVENT_STATE_SIGNALED        0xff00ff00
/** The semaphore is not signaled. */
#define EVENT_STATE_NOT_SIGNALED    0x00ff00ff
/** @} */


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

