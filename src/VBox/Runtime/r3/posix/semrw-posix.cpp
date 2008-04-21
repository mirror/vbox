/* $Id$ */
/** @file
 * IPRT - Read-Write Semaphore, POSIX.
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

#include "internal/magics.h"

/** @todo move this to r3/posix/something.h. */
#ifdef RT_OS_SOLARIS
# define ATOMIC_GET_PTHREAD_T(pvVar, pThread) ASMAtomicReadSize(pvVar, pThread)
# define ATOMIC_SET_PTHREAD_T(pvVar, pThread) ASMAtomicWriteSize(pvVar, pThread)
#else
AssertCompileSize(pthread_t, sizeof(void *));
# define ATOMIC_GET_PTHREAD_T(pvVar, pThread) do { *(pThread) = (pthread_t)ASMAtomicReadPtr((void *volatile *)pvVar); } while (0)
# define ATOMIC_SET_PTHREAD_T(pvVar, pThread) ASMAtomicWritePtr((void *volatile *)pvVar, (void *)pThread)
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Posix internal representation of a read-write semaphore. */
struct RTSEMRWINTERNAL
{
    /** The usual magic. (RTSEMRW_MAGIC) */
    uint32_t            u32Magic;
    /* Alignment padding. */
    uint32_t            u32Padding;
    /** Number of write recursions. */
    uint32_t            cWrites;
    /** Number of read recursions by the writer. */
    uint32_t            cWriterReads;
    /** The write owner of the lock. */
    volatile pthread_t  Writer;
    /** pthread rwlock. */
    pthread_rwlock_t    RWLock;
};


RTDECL(int) RTSemRWCreate(PRTSEMRW pRWSem)
{
    int rc;

    /*
     * Allocate handle.
     */
    struct RTSEMRWINTERNAL *pThis = (struct RTSEMRWINTERNAL *)RTMemAlloc(sizeof(struct RTSEMRWINTERNAL));
    if (pThis)
    {
        /*
         * Create the rwlock.
         */
        pthread_rwlockattr_t Attr;
        rc = pthread_rwlockattr_init(&Attr);
        if (!rc)
        {
            rc = pthread_rwlock_init(&pThis->RWLock, &Attr);
            if (!rc)
            {
                pThis->u32Magic = RTSEMRW_MAGIC;
                pThis->u32Padding = 0;
                pThis->cWrites = 0;
                pThis->cWriterReads = 0;
                pThis->Writer = (pthread_t)-1;
                *pRWSem = pThis;
                return VINF_SUCCESS;
            }
        }

        rc = RTErrConvertFromErrno(rc);
        RTMemFree(pThis);
    }
    else
        rc = VERR_NO_MEMORY;

    return rc;
}


RTDECL(int) RTSemRWDestroy(RTSEMRW RWSem)
{
    /*
     * Validate input, nil handle is fine.
     */
    if (RWSem == NIL_RTSEMRW)
        return VINF_SUCCESS;
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);
    Assert(pThis->Writer == (pthread_t)-1);
    Assert(!pThis->cWrites);
    Assert(!pThis->cWriterReads);

    /*
     * Try destroy it.
     */
    int rc = pthread_rwlock_destroy(&pThis->RWLock);
    if (!rc)
    {
        pThis->u32Magic++;
        RTMemFree(pThis);
        rc = VINF_SUCCESS;
    }
    else
    {
        AssertMsgFailed(("Failed to destroy read-write sem %p, rc=%d.\n", RWSem, rc));
        rc = RTErrConvertFromErrno(rc);
    }

    return rc;
}


RTDECL(int) RTSemRWRequestRead(RTSEMRW RWSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    /*
     * Check if it's the writer (implement write+read recursion).
     */
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    if (Writer == Self)
    {
        Assert(pThis->cWriterReads < INT32_MAX);
        pThis->cWriterReads++;
        return VINF_SUCCESS;
    }

    /*
     * Try lock it.
     */
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take rwlock */
        int rc = pthread_rwlock_rdlock(&pThis->RWLock);
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
        int rc = pthread_rwlock_timedrdlock(&pThis->RWLock, &ts);
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed read lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
#endif /* !RT_OS_DARWIN */
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTSemRWRequestReadNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    /* EINTR isn't returned by the wait functions we're using. */
    return RTSemRWRequestRead(RWSem, cMillies);
}


RTDECL(int) RTSemRWReleaseRead(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    /*
     * Check if it's the writer.
     */
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    if (Writer == Self)
    {
        AssertMsgReturn(pThis->cWriterReads > 0,
                        ("pThis=%p\n", pThis), VERR_NOT_OWNER);
        pThis->cWriterReads--;
        return VINF_SUCCESS;
    }

    /*
     * Try unlock it.
     */
    int rc = pthread_rwlock_unlock(&pThis->RWLock);
    if (rc)
    {
        AssertMsgFailed(("Failed read unlock read-write sem %p, rc=%d.\n", RWSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}


RTDECL(int) RTSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    /*
     * Recursion?
     */
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    if (Writer == Self)
    {
        Assert(pThis->cWrites < INT32_MAX);
        pThis->cWrites++;
        return VINF_SUCCESS;
    }

    /*
     * Try lock it.
     */
    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take rwlock */
        int rc = pthread_rwlock_wrlock(&pThis->RWLock);
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
        int rc = pthread_rwlock_timedwrlock(&pThis->RWLock, &ts);
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed read lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
#endif /* !RT_OS_DARWIN */
    }

    ATOMIC_SET_PTHREAD_T(&pThis->Writer, Self);
    pThis->cWrites = 1;
    return VINF_SUCCESS;
}


RTDECL(int) RTSemRWRequestWriteNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    /* EINTR isn't returned by the wait functions we're using. */
    return RTSemRWRequestWrite(RWSem, cMillies);
}


RTDECL(int) RTSemRWReleaseWrite(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    /*
     * Verify ownership and implement recursion.
     */
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    AssertMsgReturn(Writer == Self, ("pThis=%p\n", pThis), VERR_NOT_OWNER);
    pThis->cWrites--;
    if (pThis->cWrites)
        return VINF_SUCCESS;
    AssertReturn(!pThis->cWriterReads, VERR_WRONG_ORDER);

    /*
     * Try unlock it.
     */
    ATOMIC_SET_PTHREAD_T(&pThis->Writer, (pthread_t)-1);
    int rc = pthread_rwlock_unlock(&pThis->RWLock);
    if (rc)
    {
        AssertMsgFailed(("Failed write unlock read-write sem %p, rc=%d.\n", RWSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}

