/* $Id$ */
/** @file
 * innotek Portable Runtime - Multiple Release Event Semaphore, POSIX.
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


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Posix internal representation of a Mutex Multi semaphore.
 * The POSIX implementation uses a mutex and a condition variable to implement
 * the automatic reset event semaphore semantics. */
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

/** The valus of the u32State variable in RTSEMEVENTMULTIINTERNAL.
 * @{ */
/** The object isn't initialized. */
#define EVENTMULTI_STATE_UNINITIALIZED   0
/** The semaphore is is signaled. */
#define EVENTMULTI_STATE_SIGNALED        0xff00ff00
/** The semaphore is not signaled. */
#define EVENTMULTI_STATE_NOT_SIGNALED    0x00ff00ff
/** @} */



/**
 * Validate an event multi semaphore handle passed to one of the interface.
 *
 * @returns true if valid.
 * @returns false if invalid.
 * @param   pThis    Pointer to the event semaphore to validate.
 */
inline bool rtsemEventMultiValid(struct RTSEMEVENTMULTIINTERNAL *pThis)
{
    if ((uintptr_t)pThis < 0x10000)
        return false;

    uint32_t    u32 = pThis->u32State; /* this is volatile, so a explicit read like this is needed. */
    if (    u32 != EVENTMULTI_STATE_NOT_SIGNALED
        &&  u32 != EVENTMULTI_STATE_SIGNALED)
        return false;

    return true;
}


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    int rc;

    /*
     * Allocate semaphore handle.
     */
    struct RTSEMEVENTMULTIINTERNAL *pThis = (struct RTSEMEVENTMULTIINTERNAL *)RTMemAlloc(sizeof(struct RTSEMEVENTMULTIINTERNAL));
    if (pThis)
    {
        /*
         * Create the condition variable.
         */
        pthread_condattr_t CondAttr;
        rc = pthread_condattr_init(&CondAttr);
        if (!rc)
        {
            rc = pthread_cond_init(&pThis->Cond, &CondAttr);
            if (!rc)
            {
                /*
                 * Create the semaphore.
                 */
                pthread_mutexattr_t MutexAttr;
                rc = pthread_mutexattr_init(&MutexAttr);
                if (!rc)
                {
                    rc = pthread_mutex_init(&pThis->Mutex, &MutexAttr);
                    if (!rc)
                    {
                        pthread_mutexattr_destroy(&MutexAttr);
                        pthread_condattr_destroy(&CondAttr);

                        ASMAtomicXchgU32(&pThis->u32State, EVENTMULTI_STATE_NOT_SIGNALED);
                        ASMAtomicXchgU32(&pThis->cWaiters, 0);

                        *pEventMultiSem = pThis;
                        return VINF_SUCCESS;
                    }

                    pthread_mutexattr_destroy(&MutexAttr);
                }
                pthread_cond_destroy(&pThis->Cond);
            }
            pthread_condattr_destroy(&CondAttr);
        }

        rc = RTErrConvertFromErrno(rc);
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;

}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Validate handle.
     */
    if (!rtsemEventMultiValid(EventMultiSem))
    {
        AssertMsgFailed(("Invalid handle %p!\n", EventMultiSem));
        return VERR_INVALID_HANDLE;
    }

    /*
     * Abort all waiters forcing them to return failure.
     */
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    int rc;
    for (int i = 30; i > 0; i--)
    {
        ASMAtomicXchgU32(&pThis->u32State, EVENTMULTI_STATE_UNINITIALIZED);
        rc = pthread_cond_destroy(&pThis->Cond);
        if (rc != EBUSY)
            break;
        pthread_cond_broadcast(&pThis->Cond);
        usleep(1000);
    } while (rc == EBUSY);
    if (rc)
    {
        AssertMsgFailed(("Failed to destroy event sem %p, rc=%d.\n", EventMultiSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Destroy the semaphore
     * If it's busy we'll wait a bit to give the threads a chance to be scheduled.
     */
    for (int i = 30; i > 0; i--)
    {
        rc = pthread_mutex_destroy(&pThis->Mutex);
        if (rc != EBUSY)
            break;
        usleep(1000);
    }
    if (rc)
    {
        AssertMsgFailed(("Failed to destroy event sem %p, rc=%d. (mutex)\n", EventMultiSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Free the semaphore memory and be gone.
     */
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
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
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    int rc = pthread_mutex_lock(&pThis->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to lock event sem %p, rc=%d.\n", EventMultiSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Check the state.
     */
    if (pThis->u32State == EVENTMULTI_STATE_NOT_SIGNALED)
    {
        ASMAtomicXchgU32(&pThis->u32State, EVENTMULTI_STATE_SIGNALED);
        rc = pthread_cond_signal(&pThis->Cond);
        AssertMsg(!rc, ("Failed to signal event sem %p, rc=%d.\n", EventMultiSem, rc));
    }
    else if (pThis->u32State == EVENTMULTI_STATE_SIGNALED)
    {
        rc = pthread_cond_signal(&pThis->Cond); /* give'm another kick... */
        AssertMsg(!rc, ("Failed to signal event sem %p, rc=%d. (2)\n", EventMultiSem, rc));
    }
    else
        rc = VERR_SEM_DESTROYED;

    /*
     * Release the mutex and return.
     */
    int rc2 = pthread_mutex_unlock(&pThis->Mutex);
    AssertMsg(!rc2, ("Failed to unlock event sem %p, rc=%d.\n", EventMultiSem, rc));
    if (rc)
        return RTErrConvertFromErrno(rc);
    if (rc2)
        return RTErrConvertFromErrno(rc2);

    return VINF_SUCCESS;
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
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    int rc = pthread_mutex_lock(&pThis->Mutex);
    if (rc)
    {
        AssertMsgFailed(("Failed to lock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    /*
     * Check the state.
     */
    if (pThis->u32State == EVENTMULTI_STATE_SIGNALED)
        ASMAtomicXchgU32(&pThis->u32State, EVENTMULTI_STATE_NOT_SIGNALED);
    else if (pThis->u32State != EVENTMULTI_STATE_NOT_SIGNALED)
        rc = VERR_SEM_DESTROYED;

    /*
     * Release the mutex and return.
     */
    rc = pthread_mutex_unlock(&pThis->Mutex);
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
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take mutex */
        int rc = pthread_mutex_lock(&pThis->Mutex);
        if (rc)
        {
            AssertMsgFailed(("Failed to lock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
            return RTErrConvertFromErrno(rc);
        }
        ASMAtomicIncU32(&pThis->cWaiters);

        for (;;)
        {
            /* check state. */
            if (pThis->u32State == EVENTMULTI_STATE_SIGNALED)
            {
                ASMAtomicDecU32(&pThis->cWaiters);
                rc = pthread_mutex_unlock(&pThis->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VINF_SUCCESS;
            }
            if (pThis->u32State == EVENTMULTI_STATE_UNINITIALIZED)
            {
                rc = pthread_mutex_unlock(&pThis->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VERR_SEM_DESTROYED;
            }

            /* wait */
            rc = pthread_cond_wait(&pThis->Cond, &pThis->Mutex);
            if (rc)
            {
                AssertMsgFailed(("Failed to wait on event multi sem %p, rc=%d.\n", EventMultiSem, rc));
                ASMAtomicDecU32(&pThis->cWaiters);
                int rc2 = pthread_mutex_unlock(&pThis->Mutex);
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
        int rc = pthread_mutex_lock(&pThis->Mutex);
#else
        int rc = pthread_mutex_timedlock(&pThis->Mutex, &ts);
#endif
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed to lock event multi sem %p, rc=%d.\n", EventMultiSem, rc));
            return RTErrConvertFromErrno(rc);
        }
        ASMAtomicIncU32(&pThis->cWaiters);

        for (;;)
        {
            /* check state. */
            if (pThis->u32State == EVENTMULTI_STATE_SIGNALED)
            {
                ASMAtomicXchgU32(&pThis->u32State, EVENTMULTI_STATE_NOT_SIGNALED);
                ASMAtomicDecU32(&pThis->cWaiters);
                rc = pthread_mutex_unlock(&pThis->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VINF_SUCCESS;
            }
            if (pThis->u32State == EVENTMULTI_STATE_UNINITIALIZED)
            {
                rc = pthread_mutex_unlock(&pThis->Mutex);
                AssertMsg(!rc, ("Failed to unlock event multi sem %p, rc=%d.\n", EventMultiSem, rc)); NOREF(rc);
                return VERR_SEM_DESTROYED;
            }

            /* wait */
            rc = pthread_cond_timedwait(&pThis->Cond, &pThis->Mutex, &ts);
            if (rc && (rc != EINTR || !fAutoResume)) /* according to SuS this function shall not return EINTR, but linux man page says differently. */
            {
                AssertMsg(rc == ETIMEDOUT, ("Failed to wait on event multi sem %p, rc=%d.\n", EventMultiSem, rc));
                ASMAtomicDecU32(&pThis->cWaiters);
                int rc2 = pthread_mutex_unlock(&pThis->Mutex);
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

