/* $Id$ */
/** @file
 * innotek Portable Runtime - Event Semaphore, Linux (2.6.x+).
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
    struct RTSEMEVENTINTERNAL *pThis = (struct RTSEMEVENTINTERNAL *)RTMemAlloc(sizeof(struct RTSEMEVENTINTERNAL));
    if (pThis)
    {
        pThis->iMagic = RTSEMEVENT_MAGIC;
        pThis->cWaiters = 0;
        *pEventSem = pThis;
        return VINF_SUCCESS;
    }
    return  VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pThis = EventSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENT_MAGIC,
                 VERR_INVALID_HANDLE);

    /*
     * Invalidate the semaphore and wake up anyone waiting on it.
     */
    ASMAtomicXchgSize(&pThis->iMagic, RTSEMEVENT_MAGIC + 1);
    if (ASMAtomicXchgS32(&pThis->cWaiters, INT32_MIN / 2) > 0)
    {
        sys_futex(&pThis->cWaiters, FUTEX_WAKE, INT_MAX, NULL, NULL, 0);
        usleep(1000);
    }

    /*
     * Free the semaphore memory and be gone.
     */
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    struct RTSEMEVENTINTERNAL *pThis = EventSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENT_MAGIC,
                 VERR_INVALID_HANDLE);
    /*
     * Try signal it.
     */
    for (unsigned i = 0;; i++)
    {
        int32_t iCur = pThis->cWaiters;
        if (iCur == 0)
        {
            if (ASMAtomicCmpXchgS32(&pThis->cWaiters, -1, 0))
                break; /* nobody is waiting */
        }
        else if (iCur < 0)
            break; /* already signaled */
        else
        {
            /* somebody is waiting, try wake up one of them. */
            long cWoken = sys_futex(&pThis->cWaiters, FUTEX_WAKE, 1, NULL, NULL, 0);
            if (RT_LIKELY(cWoken == 1))
            {
                ASMAtomicDecS32(&pThis->cWaiters);
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
                    AssertReleaseMsg(i < 4096, ("iCur=%#x pThis=%p\n", iCur, pThis));
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
    struct RTSEMEVENTINTERNAL *pThis = EventSem;
    AssertReturn(VALID_PTR(pThis) && pThis->iMagic == RTSEMEVENT_MAGIC,
                 VERR_INVALID_HANDLE);

    /*
     * Quickly check whether it's signaled.
     */
    if (ASMAtomicCmpXchgS32(&pThis->cWaiters, 0, -1))
        return VINF_SUCCESS;

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
         * Announce that we're among the waiters.
         */
        int32_t iNew = ASMAtomicIncS32(&pThis->cWaiters);
        if (iNew == 0)
            return VINF_SUCCESS;
        if (RT_LIKELY(iNew > 0))
        {
            /*
             * Go to sleep.
             */
            long rc = sys_futex(&pThis->cWaiters, FUTEX_WAIT, iNew, pTimeout, NULL, 0);
            if (RT_UNLIKELY(pThis->iMagic != RTSEMEVENT_MAGIC))
                return VERR_SEM_DESTROYED;

            /* Did somebody wake us up from RTSemEventSignal()? */
            if (rc == 0)
                return VINF_SUCCESS;

            /* No, then the kernel woke us up or we failed going to sleep. Adjust the accounting. */
            iNew = ASMAtomicDecS32(&pThis->cWaiters);
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
            if (RT_UNLIKELY(pThis->iMagic != RTSEMEVENT_MAGIC))
                return VERR_SEM_DESTROYED;
            AssertReleaseMsgFailed(("iNew=%d\n", iNew));
        }
    }
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    int rc = rtSemEventWait(EventSem, cMillies, true);
    Assert(rc != VERR_INTERRUPTED);
    Assert(rc != VERR_TIMEOUT || cMillies != RT_INDEFINITE_WAIT);
    return rc;
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, false);
}

