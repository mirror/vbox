/* $Id$ */
/** @file
 * innotek Portable Runtime - Read-Write Semaphore, POSIX.
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

