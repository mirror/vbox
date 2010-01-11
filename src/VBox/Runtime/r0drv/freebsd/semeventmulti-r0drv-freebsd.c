/* $Id$ */
/** @file
 * IPRT - Multiple Release Event Semaphores, Ring-0 Driver, FreeBSD.
 */

/*
 * Copyright (c) 2007 knut st. osmundsen <bird-src-spam@anduin.net>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-freebsd-kernel.h"

#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/spinlock.h>

#include "internal/magics.h"

/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * FreeBSD multiple release event semaphore.
 */
typedef struct RTSEMEVENTMULTIINTERNAL
{
    /** Magic value (RTSEMEVENTMULTI_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The number of waiting threads. */
    uint32_t volatile   cWaiters;
    /** Set if the event object is signaled. */
    uint8_t volatile    fSignaled;
    /** The number of threads in the process of waking up. */
    uint32_t volatile   cWaking;
    /** Spinlock protecting this structure. */
    RTSPINLOCK          hSpinLock;
} RTSEMEVENTMULTIINTERNAL, *PRTSEMEVENTMULTIINTERNAL;


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI phEventMultiSem)
{
    return RTSemEventMultiCreateEx(phEventMultiSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventMultiCreateEx(PRTSEMEVENTMULTI phEventMultiSem, uint32_t fFlags, RTLOCKVALCLASS hClass,
                                     const char *pszNameFmt, ...)
{
    AssertCompile(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    AssertReturn(!(fFlags & ~RTSEMEVENTMULTI_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phEventMultiSem, VERR_INVALID_POINTER);

    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)RTMemAllocZ(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pThis->cWaiters = 0;
        pThis->cWaking = 0;
        pThis->fSignaled = 0;
        int rc = RTSpinlockCreate(&pThis->hSpinLock);
        if (RT_SUCCESS(rc))
        {
            *phEventMultiSem = pThis;
            return VINF_SUCCESS;
        }

        RTMemFree(pThis);
        return rc;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    RTSPINLOCKTMP            Tmp = RTSPINLOCKTMP_INITIALIZER;

    if (pThis == NIL_RTSEMEVENTMULTI)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pThis->hSpinLock, &Tmp);
    ASMAtomicIncU32(&pThis->u32Magic); /* make the handle invalid */
    if (pThis->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        sleepq_lock(pThis);
        sleepq_broadcast(pThis, SLEEPQ_CONDVAR, 0, 0);
        sleepq_release(pThis);
        RTSpinlockRelease(pThis->hSpinLock, &Tmp);
    }
    else if (pThis->cWaking)
        /* the last waking thread is gonna do the cleanup */
        RTSpinlockRelease(pThis->hSpinLock, &Tmp);
    else
    {
        RTSpinlockRelease(pThis->hSpinLock, &Tmp);
        RTSpinlockDestroy(pThis->hSpinLock);
        RTMemFree(pThis);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI hEventMultiSem)
{
    RTSPINLOCKTMP            Tmp = RTSPINLOCKTMP_INITIALIZER;
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pThis->hSpinLock, &Tmp);

    ASMAtomicXchgU8(&pThis->fSignaled, true);
    if (pThis->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        ASMAtomicXchgU32(&pThis->cWaiters, 0);
        sleepq_lock(pThis);
        int fWakeupSwapProc = sleepq_signal(pThis, SLEEPQ_CONDVAR, 0, 0);
        sleepq_release(pThis);
        if (fWakeupSwapProc)
            kick_proc0();
    }

    RTSpinlockRelease(pThis->hSpinLock, &Tmp);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI hEventMultiSem)
{
    RTSPINLOCKTMP            Tmp = RTSPINLOCKTMP_INITIALIZER;
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pThis->hSpinLock, &Tmp);
    ASMAtomicXchgU8(&pThis->fSignaled, false);
    RTSpinlockRelease(pThis->hSpinLock, &Tmp);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies, bool fInterruptible)
{
    int rc;
    RTSPINLOCKTMP            Tmp = RTSPINLOCKTMP_INITIALIZER;
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pThis->hSpinLock, &Tmp);

    if (pThis->fSignaled)
        rc = VINF_SUCCESS;
    else
    {
        if (cMillies == 0)
            rc = VERR_TIMEOUT;
        else
        {
            ASMAtomicIncU32(&pThis->cWaiters);

            int fFlags = SLEEPQ_CONDVAR;

            if (fInterruptible)
                fFlags |= SLEEPQ_INTERRUPTIBLE;

            sleepq_lock(pThis);
            sleepq_add(pThis, NULL, "IPRT Event Semaphore", fFlags, 0);

            if (cMillies != RT_INDEFINITE_WAIT)
            {
                /*
                 * Translate milliseconds into ticks and go to sleep.
                 */
                struct timeval tv;

                tv.tv_sec = cMillies / 1000;
                tv.tv_usec = (cMillies % 1000) * 1000;

                sleepq_set_timeout(pThis, tvtohz(&tv));

                RTSpinlockRelease(pThis->hSpinLock, &Tmp);

                if (fInterruptible)
                    rc = SLEEPQ_TIMEDWAIT_SIG(pThis);
                else
                    rc = SLEEPQ_TIMEDWAIT(pThis);
            }
            else
            {
                RTSpinlockRelease(pThis->hSpinLock, &Tmp);

                if (fInterruptible)
                    rc = SLEEPQ_WAIT_SIG(pThis);
                else
                {
                    rc = 0;
                    SLEEPQ_WAIT(pThis);
                }
            }

            RTSpinlockAcquire(pThis->hSpinLock, &Tmp);

            switch (rc)
            {
                case 0:
                    if (pThis->u32Magic == RTSEMEVENTMULTI_MAGIC)
                    {
                        ASMAtomicDecU32(&pThis->cWaking);
                        rc = VINF_SUCCESS;
                    }
                    else
                    {
                        rc = VERR_SEM_DESTROYED; /** @todo this isn't necessarily correct, we've
                                                  * could've woken up just before destruction... */
                        if (!ASMAtomicDecU32(&pThis->cWaking))
                        {
                            /* The event was destroyed, as the last thread do the cleanup.
                               we don't actually know whether */
                            RTSpinlockRelease(pThis->hSpinLock, &Tmp);
                            RTSpinlockDestroy(pThis->hSpinLock);
                            RTMemFree(pThis);
                            return rc;
                        }
                    }
                    break;

                case EWOULDBLOCK:
                    Assert(cMillies != RT_INDEFINITE_WAIT);
                    if (pThis->cWaiters > 0)
                        ASMAtomicDecU32(&pThis->cWaiters);
                    rc = VERR_TIMEOUT;
                    break;

                case EINTR:
                case ERESTART:
                    Assert(fInterruptible);
                    if (pThis->cWaiters > 0)
                        ASMAtomicDecU32(&pThis->cWaiters);
                    rc = VERR_INTERRUPTED;
                    break;

                default:
                    AssertMsgFailed(("sleepq_* -> %d\n", rc));
                    rc = VERR_GENERAL_FAILURE;
                    break;
            }
        }
    }

    RTSpinlockRelease(pThis->hSpinLock, &Tmp);
    return rc;
}


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, true /* interruptible */);
}

