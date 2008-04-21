/* $Id$ */
/** @file
 * IPRT - Single Release Event Semaphores, Ring-0 Driver, FreeBSD.
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

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * FreeBSD event semaphore.
 */
typedef struct RTSEMEVENTINTERNAL
{
    /** Magic value (RTSEMEVENT_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The number of waiting threads. */
    uint32_t volatile   cWaiters;
    /** Set if the event object is signaled. */
    uint8_t volatile    fSignaled;
    /** The number of threads in the process of waking up. */
    uint32_t volatile   cWaking;
    /** The FreeBSD spinlock protecting this structure. */
    struct mtx          Mtx;
} RTSEMEVENTINTERNAL, *PRTSEMEVENTINTERNAL;


RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    Assert(sizeof(RTSEMEVENTINTERNAL) > sizeof(void *));
    AssertPtrReturn(pEventSem, VERR_INVALID_POINTER);

    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)RTMemAlloc(sizeof(*pEventInt));
    if (pEventInt)
    {
        pEventInt->u32Magic = RTSEMEVENT_MAGIC;
        pEventInt->cWaiters = 0;
        pEventInt->cWaking = 0;
        pEventInt->fSignaled = 0;
        mtx_init(&pEventInt->Mtx, "IPRT Event Semaphore", NULL, MTX_SPIN);
        *pEventSem = pEventInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    if (EventSem == NIL_RTSEMEVENT)     /* don't bitch */
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventInt->Mtx);
    ASMAtomicIncU32(&pEventInt->u32Magic); /* make the handle invalid */
    if (pEventInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventInt->cWaking, pEventInt->cWaking + pEventInt->cWaiters);
        wakeup(pEventInt);
        mtx_unlock_spin(&pEventInt->Mtx);
    }
    else if (pEventInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        mtx_unlock_spin(&pEventInt->Mtx);
    else
    {
        mtx_unlock_spin(&pEventInt->Mtx);
        mtx_destroy(&pEventInt->Mtx);
        RTMemFree(pEventInt);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventInt->Mtx);

    if (pEventInt->cWaiters > 0)
    {
        ASMAtomicDecU32(&pEventInt->cWaiters);
        ASMAtomicIncU32(&pEventInt->cWaking);
        wakeup_one(pEventInt);
    }
    else
        ASMAtomicXchgU8(&pEventInt->fSignaled, true);

    mtx_unlock_spin(&pEventInt->Mtx);
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT EventSem, unsigned cMillies, bool fInterruptible)
{
    int rc;
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventInt->Mtx);

    if (pEventInt->fSignaled)
    {
        Assert(!pEventInt->cWaiters);
        ASMAtomicXchgU8(&pEventInt->fSignaled, false);
        rc = VINF_SUCCESS;
    }
    else
    {
        /*
         * Translate milliseconds into ticks and go to sleep.
         */
        int cTicks;
        if (cMillies != RT_INDEFINITE_WAIT)
        {
            if (hz == 1000)
                cTicks = cMillies;
            else if (hz == 100)
                cTicks = cMillies / 10;
            else
            {
                int64_t cTicks64 = ((uint64_t)cMillies * hz) / 1000;
                cTicks = (int)cTicks64;
                if (cTicks != cTicks64)
                    cTicks = INT_MAX;
            }
        }
        else
            cTicks = 0;

        ASMAtomicIncU32(&pEventInt->cWaiters);

        rc = msleep(pEventInt,          /* block id */
                    &pEventInt->Mtx,
                    fInterruptible ? PZERO | PCATCH : PZERO,
                    "iprtev",           /* max 6 chars */
                    cTicks);
        switch (rc)
        {
            case 0:
                if (pEventInt->u32Magic == RTSEMEVENT_MAGIC)
                {
                    ASMAtomicDecU32(&pEventInt->cWaking);
                    rc = VINF_SUCCESS;
                }
                else
                {
                    rc = VERR_SEM_DESTROYED; /** @todo this isn't necessarily correct, we've
                                              * could've woken up just before destruction... */
                    if (!ASMAtomicDecU32(&pEventInt->cWaking))
                    {
                        /* The event was destroyed, as the last thread do the cleanup.
                           we don't actually know whether */
                        mtx_unlock_spin(&pEventInt->Mtx);
                        mtx_destroy(&pEventInt->Mtx);
                        RTMemFree(pEventInt);
                        return rc;
                    }
                }
                break;

            case EWOULDBLOCK:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                if (pEventInt->cWaiters > 0)
                    ASMAtomicDecU32(&pEventInt->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case EINTR:
            case ERESTART:
                Assert(fInterruptible);
                if (pEventInt->cWaiters > 0)
                    ASMAtomicDecU32(&pEventInt->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            default:
                AssertMsgFailed(("msleep -> %d\n", rc));
                rc = VERR_GENERAL_FAILURE;
                break;
        }
    }

    mtx_unlock_spin(&pEventInt->Mtx);
    return rc;
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    return rtSemEventWait(EventSem, cMillies, true /* interruptible */);
}

