/* $Id$ */
/** @file
 * IPRT - Single Release Event Semaphores, Ring-0 Driver, OS/2.
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
#include "the-os2-kernel.h"

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
 * OS/2 event semaphore.
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
    /** The OS/2 spinlock protecting this structure. */
    SpinLock_t          Spinlock;
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
        KernAllocSpinLock(&pEventInt->Spinlock);
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

    KernAcquireSpinLock(&pEventInt->Spinlock);
    ASMAtomicIncU32(&pEventInt->u32Magic); /* make the handle invalid */
    if (pEventInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventInt->cWaking, pEventInt->cWaking + pEventInt->cWaiters);
        ULONG cThreads;
        KernWakeup((ULONG)pEventInt, WAKEUP_DATA | WAKEUP_BOOST, &cThreads, (ULONG)VERR_SEM_DESTROYED);
        KernReleaseSpinLock(&pEventInt->Spinlock);
    }
    else if (pEventInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        KernReleaseSpinLock(&pEventInt->Spinlock);
    else
    {
        KernReleaseSpinLock(&pEventInt->Spinlock);
        KernFreeSpinLock(&pEventInt->Spinlock);
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

    KernAcquireSpinLock(&pEventInt->Spinlock);

    if (pEventInt->cWaiters > 0)
    {
        ASMAtomicDecU32(&pEventInt->cWaiters);
        ASMAtomicIncU32(&pEventInt->cWaking);
        ULONG cThreads;
        KernWakeup((ULONG)pEventInt, WAKEUP_DATA | WAKEUP_ONE, &cThreads, VINF_SUCCESS);
        if (RT_UNLIKELY(!cThreads))
        {
            /* shouldn't ever happen on OS/2 */
            ASMAtomicXchgU8(&pEventInt->fSignaled, true);
            ASMAtomicDecU32(&pEventInt->cWaking);
            ASMAtomicIncU32(&pEventInt->cWaiters);
        }
    }
    else
        ASMAtomicXchgU8(&pEventInt->fSignaled, true);

    KernReleaseSpinLock(&pEventInt->Spinlock);
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT EventSem, unsigned cMillies, bool fInterruptible)
{
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pEventInt->Spinlock);

    int rc;
    if (pEventInt->fSignaled)
    {
        Assert(!pEventInt->cWaiters);
        ASMAtomicXchgU8(&pEventInt->fSignaled, false);
        rc = VINF_SUCCESS;
    }
    else
    {
        ASMAtomicIncU32(&pEventInt->cWaiters);

        ULONG ulData = (ULONG)VERR_INTERNAL_ERROR;
        rc = KernBlock((ULONG)pEventInt,
                       cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies,
                       BLOCK_SPINLOCK | (!fInterruptible ? BLOCK_UNINTERRUPTABLE : 0),
                       &pEventInt->Spinlock,
                       &ulData);
        switch (rc)
        {
            case NO_ERROR:
                rc = (int)ulData;
                Assert(rc == VINF_SUCCESS || rc == VERR_SEM_DESTROYED);
                Assert(pEventInt->cWaking > 0);
                if (    !ASMAtomicDecU32(&pEventInt->cWaking)
                    &&  pEventInt->u32Magic != RTSEMEVENT_MAGIC)
                {
                    /* The event was destroyed (ulData == VINF_SUCCESS if it was after we awoke), as
                       the last thread do the cleanup. */
                    KernReleaseSpinLock(&pEventInt->Spinlock);
                    KernFreeSpinLock(&pEventInt->Spinlock);
                    RTMemFree(pEventInt);
                    return rc;
                }
                break;

            case ERROR_TIMEOUT:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                ASMAtomicDecU32(&pEventInt->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case ERROR_INTERRUPT:
                Assert(fInterruptible);
                ASMAtomicDecU32(&pEventInt->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            default:
                AssertMsgFailed(("rc=%d\n", rc));
                rc = VERR_GENERAL_FAILURE;
                break;
        }
    }

    KernReleaseSpinLock(&pEventInt->Spinlock);
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

