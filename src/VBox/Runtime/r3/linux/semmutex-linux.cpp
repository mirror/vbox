/* $Id$ */
/** @file
 * innotek Portable Runtime - Mutex Semaphore, Linux  (2.6.x+).
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
 * Linux internal representation of a Mutex semaphore.
 */
struct RTSEMMUTEXINTERNAL
{
    /** The futex state variable.
     * 0 means unlocked.
     * 1 means locked, no waiters.
     * 2 means locked, one or more waiters.
     */
    int32_t volatile    iState;
    /** Nesting count. */
    uint32_t volatile   cNesting;
    /** The owner of the mutex. */
    pthread_t volatile  Owner;
    /** Magic value. */
    intptr_t volatile   iMagic;
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


RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    /*
     * Allocate semaphore handle.
     */
    struct RTSEMMUTEXINTERNAL *pThis = (struct RTSEMMUTEXINTERNAL *)RTMemAlloc(sizeof(struct RTSEMMUTEXINTERNAL));
    if (pThis)
    {
        pThis->iMagic   = RTSEMMUTEX_MAGIC;
        pThis->iState   = 0;
        pThis->Owner    = (pthread_t)~0;
        pThis->cNesting = 0;

        *pMutexSem = pThis;
        return VINF_SUCCESS;
    }

    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    if (MutexSem == NIL_RTSEMMUTEX)
        return VERR_INVALID_HANDLE;
    struct RTSEMMUTEXINTERNAL *pThis = MutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->iMagic == RTSEMMUTEX_MAGIC,
                    ("MutexSem=%p iMagic=%#x\n", pThis, pThis->iMagic),
                    VERR_INVALID_HANDLE);

    /*
     * Invalidate the semaphore and wake up anyone waiting on it.
     */
    ASMAtomicXchgSize(&pThis->iMagic, RTSEMMUTEX_MAGIC + 1);
    if (ASMAtomicXchgS32(&pThis->iState, 0) > 0)
    {
        sys_futex(&pThis->iState, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        usleep(1000);
    }
    pThis->Owner    = (pthread_t)~0;
    pThis->cNesting = 0;

    /*
     * Free the semaphore memory and be gone.
     */
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


static int rtsemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    struct RTSEMMUTEXINTERNAL *pThis = MutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->iMagic == RTSEMMUTEX_MAGIC,
                    ("MutexSem=%p iMagic=%#x\n", pThis, pThis->iMagic),
                    VERR_INVALID_HANDLE);

    /*
     * Check if nested request.
     */
    pthread_t Self = pthread_self();
    if (    pThis->Owner == Self
        &&  pThis->cNesting > 0)
    {
        pThis->cNesting++;
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
    if (RT_UNLIKELY(!ASMAtomicCmpXchgS32(&pThis->iState, 1, 0)))
    {
        for (;;)
        {
            int32_t iOld = ASMAtomicXchgS32(&pThis->iState, 2);

            /*
             * Was the lock released in the meantime? This is unlikely (but possible)
             */
            if (RT_UNLIKELY(iOld == 0))
                break;

            /*
             * Go to sleep.
             */
            long rc = sys_futex(&pThis->iState, FUTEX_WAIT, 2, pTimeout, NULL, 0);
            if (RT_UNLIKELY(pThis->iMagic != RTSEMMUTEX_MAGIC))
                return VERR_SEM_DESTROYED;

            /*
             * Act on the wakup code.
             */
            if (rc == -ETIMEDOUT)
            {
                Assert(pTimeout);
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
        }

        /*
         * When leaving this loop, iState is set to 2. This means that we gained the
         * Lock and there are _possibly_ some waiters. We don't know exactly as another
         * thread might entered this loop at nearly the same time. Therefore we will
         * call futex_wakeup once too often (if _no_ other thread entered this loop).
         * The key problem is the simple futex_wait test for x != y (iState != 2) in
         * our case).
         */
    }

    /*
     * Set the owner and nesting.
     */
    pThis->Owner = Self;
    ASMAtomicXchgU32(&pThis->cNesting, 1);
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
    struct RTSEMMUTEXINTERNAL *pThis = MutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->iMagic == RTSEMMUTEX_MAGIC,
                    ("MutexSem=%p iMagic=%#x\n", pThis, pThis->iMagic),
                    VERR_INVALID_HANDLE);

    /*
     * Check if nested.
     */
    pthread_t Self = pthread_self();
    if (RT_UNLIKELY(    pThis->Owner != Self
                    ||  pThis->cNesting == 0))
    {
        AssertMsgFailed(("Not owner of mutex %p!! Self=%08x Owner=%08x cNesting=%d\n",
                         pThis, Self, pThis->Owner, pThis->cNesting));
        return VERR_NOT_OWNER;
    }

    /*
     * If nested we'll just pop a nesting.
     */
    if (pThis->cNesting > 1)
    {
        pThis->cNesting--;
        return VINF_SUCCESS;
    }

    /*
     * Clear the state. (cNesting == 1)
     */
    pThis->Owner = (pthread_t)~0;
    ASMAtomicXchgU32(&pThis->cNesting, 0);

    /*
     * Release the mutex.
     */
    int32_t iNew = ASMAtomicDecS32(&pThis->iState);
    if (iNew != 0)
    {
        /* somebody is waiting, try wake up one of them. */
        ASMAtomicXchgS32(&pThis->iState, 0);
        (void)sys_futex(&pThis->iState, FUTEX_WAKE, 1, NULL, NULL, 0);
    }
    return VINF_SUCCESS;
}

