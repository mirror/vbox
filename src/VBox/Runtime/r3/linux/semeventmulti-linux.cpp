/* $Id$ */
/** @file
 * IPRT - Multiple Release Event Semaphore, Linux (2.6.x+).
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


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    /*
     * Allocate semaphore handle.
     */
    struct RTSEMEVENTMULTIINTERNAL *pThis = (struct RTSEMEVENTMULTIINTERNAL *)RTMemAlloc(sizeof(struct RTSEMEVENTMULTIINTERNAL));
    if (pThis)
    {
        pThis->iMagic = RTSEMEVENTMULTI_MAGIC;
        pThis->iState = 0;
        *pEventMultiSem = pThis;
        return VINF_SUCCESS;
    }
    return  VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENTMULTI_MAGIC,
                 VERR_INVALID_HANDLE);

    /*
     * Invalidate the semaphore and wake up anyone waiting on it.
     */
    ASMAtomicWriteSize(&pThis->iMagic, RTSEMEVENTMULTI_MAGIC + 1);
    if (ASMAtomicXchgS32(&pThis->iState, -1) == 1)
    {
        sys_futex(&pThis->iState, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        usleep(1000);
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
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENTMULTI_MAGIC,
                 VERR_INVALID_HANDLE);
    /*
     * Signal it.
     */
    int32_t iOld = ASMAtomicXchgS32(&pThis->iState, -1);
    if (iOld > 0)
    {
        /* wake up sleeping threads. */
        long cWoken = sys_futex(&pThis->iState, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
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
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENTMULTI_MAGIC,
                 VERR_INVALID_HANDLE);
#ifdef RT_STRICT
    int32_t i = pThis->iState;
    Assert(i == 0 || i == -1 || i == 1);
#endif

    /*
     * Reset it.
     */
    ASMAtomicCmpXchgS32(&pThis->iState, 0, -1);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fAutoResume)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTMULTIINTERNAL *pThis = EventMultiSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENTMULTI_MAGIC,
                 VERR_INVALID_HANDLE);

    /*
     * Quickly check whether it's signaled.
     */
    int32_t iCur = ASMAtomicUoReadS32(&pThis->iState);
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
        iCur = ASMAtomicUoReadS32(&pThis->iState);
        Assert(iCur == 0 || iCur == -1 || iCur == 1);
        if (    iCur == 1
            ||  ASMAtomicCmpXchgS32(&pThis->iState, 1, 0))
        {
            long rc = sys_futex(&pThis->iState, FUTEX_WAIT, 1, pTimeout, NULL, 0);
            if (RT_UNLIKELY(pThis->iMagic != RTSEMEVENTMULTI_MAGIC))
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

