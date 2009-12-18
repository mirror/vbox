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
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/lockvalidator.h>
#include <iprt/mem.h>
#include <iprt/thread.h>

#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/time.h>

#include "internal/magics.h"
#include "internal/strict.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
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
#ifdef RTSEMRW_STRICT
    /** The validator record for the writer. */
    RTLOCKVALIDATORREC      ValidatorWrite;
    /** The validator record for the readers. */
    RTLOCKVALIDATORSHARED   ValidatorRead;
#endif
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
#ifdef RTSEMRW_STRICT
                RTLockValidatorRecInit(&pThis->ValidatorWrite, NIL_RTLOCKVALIDATORCLASS, RTLOCKVALIDATOR_SUB_CLASS_NONE, NULL, pThis);
                RTLockValidatorSharedRecInit(&pThis->ValidatorRead,  NIL_RTLOCKVALIDATORCLASS, RTLOCKVALIDATOR_SUB_CLASS_NONE, NULL, pThis);
#endif
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
    struct RTSEMRWINTERNAL *pThis = RWSem;
    if (pThis == NIL_RTSEMRW)
        return VINF_SUCCESS;
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
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, ~RTSEMRW_MAGIC, RTSEMRW_MAGIC), VERR_INVALID_HANDLE);
    int rc = pthread_rwlock_destroy(&pThis->RWLock);
    if (!rc)
    {
#ifdef RTSEMRW_STRICT
        RTLockValidatorSharedRecDelete(&pThis->ValidatorRead);
        RTLockValidatorRecDelete(&pThis->ValidatorWrite);
#endif
        RTMemFree(pThis);
        rc = VINF_SUCCESS;
    }
    else
    {
        ASMAtomicWriteU32(&pThis->u32Magic, RTSEMRW_MAGIC);
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
#ifdef RTSEMRW_STRICT
    RTTHREAD  ThreadSelf = RTThreadSelf();
#endif
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    if (Writer == Self)
    {
        Assert(pThis->cWriterReads < INT32_MAX);
        pThis->cWriterReads++;
#ifdef RTSEMRW_STRICT
        if (ThreadSelf != NIL_RTTHREAD)
            RTLockValidatorReadLockInc(ThreadSelf);
#endif
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

#ifdef RTSEMRW_STRICT
    if (ThreadSelf != NIL_RTTHREAD)
        RTLockValidatorReadLockInc(ThreadSelf);
#endif
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
#ifdef RTSEMRW_STRICT
    RTTHREAD  ThreadSelf = RTThreadSelf();
#endif
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    if (Writer == Self)
    {
        AssertMsgReturn(pThis->cWriterReads > 0,
                        ("pThis=%p\n", pThis), VERR_NOT_OWNER);
        pThis->cWriterReads--;
#ifdef RTSEMRW_STRICT
        if (ThreadSelf != NIL_RTTHREAD)
            RTLockValidatorReadLockDec(ThreadSelf);
#endif
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

#ifdef RTSEMRW_STRICT
    if (ThreadSelf != NIL_RTTHREAD)
        RTLockValidatorReadLockDec(ThreadSelf);
#endif
    return VINF_SUCCESS;
}


DECL_FORCE_INLINE(int) rtSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);
#ifdef RTSEMRW_STRICT
    RTTHREAD hThreadSelf = RTThreadSelfAutoAdopt();
    RTLockValidatorCheckOrder(&pThis->ValidatorWrite, hThreadSelf, pSrcPos);
#endif

    /*
     * Recursion?
     */
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    if (Writer == Self)
    {
#ifdef RTSEMRW_STRICT
        int rc9 = RTLockValidatorRecordRecursion(&pThis->ValidatorWrite, pSrcPos);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        Assert(pThis->cWrites < INT32_MAX);
        pThis->cWrites++;
        return VINF_SUCCESS;
    }
#ifndef RTSEMRW_STRICT
    RTTHREAD hThreadSelf = RTThreadSelf();
#endif

    /*
     * Try lock it.
     */
    if (cMillies)
    {
#ifdef RTSEMRW_STRICT
        int rc9 = RTLockValidatorCheckBlocking(&pThis->ValidatorWrite, hThreadSelf, RTTHREADSTATE_RW_WRITE, true, pSrcPos);
        if (RT_FAILURE(rc9))
            return rc9;
#endif
        RTThreadBlocking(hThreadSelf, RTTHREADSTATE_RW_WRITE);
    }

    if (cMillies == RT_INDEFINITE_WAIT)
    {
        /* take rwlock */
        int rc = pthread_rwlock_wrlock(&pThis->RWLock);
        RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_RW_WRITE);
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
        RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_RW_WRITE);
        if (rc)
        {
            AssertMsg(rc == ETIMEDOUT, ("Failed read lock read-write sem %p, rc=%d.\n", RWSem, rc));
            return RTErrConvertFromErrno(rc);
        }
#endif /* !RT_OS_DARWIN */
    }

    ATOMIC_SET_PTHREAD_T(&pThis->Writer, Self);
    pThis->cWrites = 1;
#ifdef RTSEMRW_STRICT
    RTLockValidatorSetOwner(&pThis->ValidatorWrite, hThreadSelf, pSrcPos);
#endif
    return VINF_SUCCESS;
}


RTDECL(int) RTSemRWRequestWrite(RTSEMRW RWSem, unsigned cMillies)
{
#ifndef RTSEMRW_STRICT
    return rtSemRWRequestWrite(RWSem, cMillies, NULL);
#else
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_NORMAL_API();
    return rtSemRWRequestWrite(RWSem, cMillies, &SrcPos);
#endif
}


RTDECL(int) RTSemRWRequestWriteDebug(RTSEMRW RWSem, unsigned cMillies, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_DEBUG_API();
    return rtSemRWRequestWrite(RWSem, cMillies, &SrcPos);
}


RTDECL(int) RTSemRWRequestWriteNoResume(RTSEMRW RWSem, unsigned cMillies)
{
    /* EINTR isn't returned by the wait functions we're using. */
#ifndef RTSEMRW_STRICT
    return rtSemRWRequestWrite(RWSem, cMillies, NULL);
#else
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_NORMAL_API();
    return rtSemRWRequestWrite(RWSem, cMillies, &SrcPos);
#endif
}


RTDECL(int) RTSemRWRequestWriteNoResumeDebug(RTSEMRW RWSem, unsigned cMillies, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    /* EINTR isn't returned by the wait functions we're using. */
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_DEBUG_API();
    return rtSemRWRequestWrite(RWSem, cMillies, &SrcPos);
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
    AssertReturn(pThis->cWriterReads == 0 || pThis->cWrites > 1, VERR_WRONG_ORDER);

    pThis->cWrites--;
    if (pThis->cWrites)
    {
#ifdef RTSEMRW_STRICT
        RTLockValidatorRecordUnwind(&pThis->ValidatorWrite);
#endif
        return VINF_SUCCESS;
    }

    /*
     * Try unlock it.
     */
#ifdef RTSEMRW_STRICT
    RTLockValidatorCheckReleaseOrder(&pThis->ValidatorWrite);
    RTLockValidatorUnsetOwner(&pThis->ValidatorWrite);
#endif

    ATOMIC_SET_PTHREAD_T(&pThis->Writer, (pthread_t)-1);
    int rc = pthread_rwlock_unlock(&pThis->RWLock);
    if (rc)
    {
        AssertMsgFailed(("Failed write unlock read-write sem %p, rc=%d.\n", RWSem, rc));
        return RTErrConvertFromErrno(rc);
    }

    return VINF_SUCCESS;
}


RTDECL(bool) RTSemRWIsWriteOwner(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, false);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    false);

    /*
     * Check ownership.
     */
    pthread_t Self = pthread_self();
    pthread_t Writer;
    ATOMIC_GET_PTHREAD_T(&pThis->Writer, &Writer);
    return Writer == Self;
}


RTDECL(uint32_t) RTSemRWGetWriteRecursion(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, 0);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    0);

    /*
     * Return the requested data.
     */
    return pThis->cWrites;
}


RTDECL(uint32_t) RTSemRWGetWriterReadRecursion(RTSEMRW RWSem)
{
    /*
     * Validate input.
     */
    struct RTSEMRWINTERNAL *pThis = RWSem;
    AssertPtrReturn(pThis, 0);
    AssertMsgReturn(pThis->u32Magic == RTSEMRW_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    0);

    /*
     * Return the requested data.
     */
    return pThis->cWriterReads;
}
