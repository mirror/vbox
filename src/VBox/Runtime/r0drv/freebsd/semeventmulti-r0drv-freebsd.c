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


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    Assert(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    AssertPtrReturn(pEventMultiSem, VERR_INVALID_POINTER);

    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)RTMemAllocZ(sizeof(*pEventMultiInt));
    if (pEventMultiInt)
    {
        pEventMultiInt->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pEventMultiInt->cWaiters = 0;
        pEventMultiInt->cWaking = 0;
        pEventMultiInt->fSignaled = 0;
        int rc = RTSpinlockCreate(&pEventMultiInt->hSpinLock);
        if (RT_SUCCESS(rc))
        {
            *pEventMultiSem = pEventMultiInt;
            return VINF_SUCCESS;
        }

        RTMemFree(pEventMultiInt);
        return rc;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    if (EventMultiSem == NIL_RTSEMEVENTMULTI)     /* don't bitch */
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    RTSPINLOCKTMP            Tmp;

    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pEventMultiInt->hSpinLock, &Tmp);
    ASMAtomicIncU32(&pEventMultiInt->u32Magic); /* make the handle invalid */
    if (pEventMultiInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        sleepq_lock(pEventMultiInt);
        sleepq_broadcast(pEventMultiInt, SLEEPQ_CONDVAR, 0, 0);
        sleepq_release(pEventMultiInt);
        RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
    }
    else if (pEventMultiInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
    else
    {
        RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
        RTSpinlockDestroy(pEventMultiInt->hSpinLock);
        RTMemFree(pEventMultiInt);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    RTSPINLOCKTMP            Tmp;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pEventMultiInt->hSpinLock, &Tmp);

    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, true);
    if (pEventMultiInt->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        ASMAtomicXchgU32(&pEventMultiInt->cWaiters, 0);
        sleepq_lock(pEventMultiInt);
        int fWakeupSwapProc = sleepq_signal(pEventMultiInt, SLEEPQ_CONDVAR, 0, 0);
        sleepq_release(pEventMultiInt);
        if (fWakeupSwapProc)
            kick_proc0();
    }

    RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    RTSPINLOCKTMP            Tmp;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pEventMultiInt->hSpinLock, &Tmp);
    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, false);
    RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fInterruptible)
{
    int rc;
    RTSPINLOCKTMP            Tmp;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    RTSpinlockAcquire(pEventMultiInt->hSpinLock, &Tmp);

    if (pEventMultiInt->fSignaled)
        rc = VINF_SUCCESS;
    else
    {
        if (cMillies == 0)
            rc = VERR_TIMEOUT;
        else
        {
            ASMAtomicIncU32(&pEventMultiInt->cWaiters);

            int fFlags = SLEEPQ_CONDVAR;

            if (fInterruptible)
                fFlags |= SLEEPQ_INTERRUPTIBLE;

            sleepq_lock(pEventMultiInt);
            sleepq_add(pEventMultiInt, NULL, "IPRT Event Semaphore", fFlags, 0);

            if (cMillies != RT_INDEFINITE_WAIT)
            {
                /*
                 * Translate milliseconds into ticks and go to sleep.
                 */
                struct timeval tv;

                tv.tv_sec = cMillies / 1000;
                tv.tv_usec = (cMillies % 1000) * 1000;

                sleepq_set_timeout(pEventMultiInt, tvtohz(&tv));

                RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);

                if (fInterruptible)
                    rc = sleepq_timedwait_sig(pEventMultiInt, 0);
                else
                    rc = sleepq_timedwait(pEventMultiInt, 0);
            }
            else
            {
                RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);

                if (fInterruptible)
                    rc = sleepq_wait_sig(pEventMultiInt, 0);
                else
                {
                    rc = 0;
                    sleepq_wait(pEventMultiInt, 0);
                }
            }

            RTSpinlockAcquire(pEventMultiInt->hSpinLock, &Tmp);

            switch (rc)
            {
                case 0:
                    if (pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC)
                    {
                        ASMAtomicDecU32(&pEventMultiInt->cWaking);
                        rc = VINF_SUCCESS;
                    }
                    else
                    {
                        rc = VERR_SEM_DESTROYED; /** @todo this isn't necessarily correct, we've
                                                  * could've woken up just before destruction... */
                        if (!ASMAtomicDecU32(&pEventMultiInt->cWaking))
                        {
                            /* The event was destroyed, as the last thread do the cleanup.
                               we don't actually know whether */
                            RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
                            RTSpinlockDestroy(pEventMultiInt->hSpinLock);
                            RTMemFree(pEventMultiInt);
                            return rc;
                        }
                    }
                    break;

                case EWOULDBLOCK:
                    Assert(cMillies != RT_INDEFINITE_WAIT);
                    if (pEventMultiInt->cWaiters > 0)
                        ASMAtomicDecU32(&pEventMultiInt->cWaiters);
                    rc = VERR_TIMEOUT;
                    break;

                case EINTR:
                case ERESTART:
                    Assert(fInterruptible);
                    if (pEventMultiInt->cWaiters > 0)
                        ASMAtomicDecU32(&pEventMultiInt->cWaiters);
                    rc = VERR_INTERRUPTED;
                    break;

                default:
                    AssertMsgFailed(("sleepq_* -> %d\n", rc));
                    rc = VERR_GENERAL_FAILURE;
                    break;
            }
        }
    }

    RTSpinlockRelease(pEventMultiInt->hSpinLock, &Tmp);
    return rc;
}


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(EventMultiSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(EventMultiSem, cMillies, true /* interruptible */);
}

