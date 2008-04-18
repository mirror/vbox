/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Mutex Semaphore, POSIX.
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


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
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

