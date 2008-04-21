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
    /** The FreeBSD spinlock protecting this structure. */
    struct mtx          Mtx;
} RTSEMEVENTMULTIINTERNAL, *PRTSEMEVENTMULTIINTERNAL;


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    Assert(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    AssertPtrReturn(pEventMultiSem, VERR_INVALID_POINTER);

    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pEventMultiInt));
    if (pEventMultiInt)
    {
        pEventMultiInt->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pEventMultiInt->cWaiters = 0;
        pEventMultiInt->cWaking = 0;
        pEventMultiInt->fSignaled = 0;
        mtx_init(&pEventMultiInt->Mtx, "IPRT Multiple Release Event Semaphore", NULL, MTX_SPIN);
        *pEventMultiSem = pEventMultiInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    if (EventMultiSem == NIL_RTSEMEVENTMULTI)     /* don't bitch */
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventMultiInt->Mtx);
    ASMAtomicIncU32(&pEventMultiInt->u32Magic); /* make the handle invalid */
    if (pEventMultiInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        wakeup(pEventMultiInt);
        mtx_unlock_spin(&pEventMultiInt->Mtx);
    }
    else if (pEventMultiInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        mtx_unlock_spin(&pEventMultiInt->Mtx);
    else
    {
        mtx_unlock_spin(&pEventMultiInt->Mtx);
        mtx_destroy(&pEventMultiInt->Mtx);
        RTMemFree(pEventMultiInt);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventMultiInt->Mtx);

    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, true);
    if (pEventMultiInt->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        ASMAtomicXchgU32(&pEventMultiInt->cWaiters, 0);
        wakeup(pEventMultiInt);
    }

    mtx_unlock_spin(&pEventMultiInt->Mtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventMultiInt->Mtx);
    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, false);
    mtx_unlock_spin(&pEventMultiInt->Mtx);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fInterruptible)
{
    int rc;
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mtx_lock_spin(&pEventMultiInt->Mtx);

    if (pEventMultiInt->fSignaled)
        rc = VINF_SUCCESS;
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

        ASMAtomicIncU32(&pEventMultiInt->cWaiters);

        rc = msleep(pEventMultiInt,     /* block id */
                    &pEventMultiInt->Mtx,
                    fInterruptible ? PZERO | PCATCH : PZERO,
                    "iprtev",           /* max 6 chars */
                    cTicks);
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
                        mtx_unlock_spin(&pEventMultiInt->Mtx);
                        mtx_destroy(&pEventMultiInt->Mtx);
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
                AssertMsgFailed(("msleep -> %d\n", rc));
                rc = VERR_GENERAL_FAILURE;
                break;
        }
    }

    mtx_unlock_spin(&pEventMultiInt->Mtx);
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

