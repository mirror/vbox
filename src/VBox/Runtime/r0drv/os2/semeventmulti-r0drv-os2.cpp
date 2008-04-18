/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Multiple Release Event Semaphores, Ring-0 Driver, OS/2.
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
 * OS/2 multiple release event semaphore.
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
    /** The OS/2 spinlock protecting this structure. */
    SpinLock_t          Spinlock;
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
        KernAllocSpinLock(&pEventMultiInt->Spinlock);
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

    KernAcquireSpinLock(&pEventMultiInt->Spinlock);
    ASMAtomicIncU32(&pEventMultiInt->u32Magic); /* make the handle invalid */
    if (pEventMultiInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        ULONG cThreads;
        KernWakeup((ULONG)pEventMultiInt, WAKEUP_DATA | WAKEUP_BOOST, &cThreads, (ULONG)VERR_SEM_DESTROYED);
        KernReleaseSpinLock(&pEventMultiInt->Spinlock);
    }
    else if (pEventMultiInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        KernReleaseSpinLock(&pEventMultiInt->Spinlock);
    else
    {
        KernReleaseSpinLock(&pEventMultiInt->Spinlock);
        KernFreeSpinLock(&pEventMultiInt->Spinlock);
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

    KernAcquireSpinLock(&pEventMultiInt->Spinlock);

    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, true);
    if (pEventMultiInt->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        ASMAtomicXchgU32(&pEventMultiInt->cWaiters, 0);
        ULONG cThreads;
        KernWakeup((ULONG)pEventMultiInt, WAKEUP_DATA, &cThreads, VINF_SUCCESS);
    }

    KernReleaseSpinLock(&pEventMultiInt->Spinlock);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pEventMultiInt->Spinlock);
    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, false);
    KernReleaseSpinLock(&pEventMultiInt->Spinlock);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fInterruptible)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pEventMultiInt->Spinlock);

    int rc;
    if (pEventMultiInt->fSignaled)
        rc = VINF_SUCCESS;
    else
    {
        ASMAtomicIncU32(&pEventMultiInt->cWaiters);

        ULONG ulData = (ULONG)VERR_INTERNAL_ERROR;
        rc = KernBlock((ULONG)pEventMultiInt,
                       cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies,
                       BLOCK_SPINLOCK | (!fInterruptible ? BLOCK_UNINTERRUPTABLE : 0),
                       &pEventMultiInt->Spinlock,
                       &ulData);
        switch (rc)
        {
            case NO_ERROR:
                rc = (int)ulData;
                Assert(rc == VINF_SUCCESS || rc == VERR_SEM_DESTROYED);
                Assert(pEventMultiInt->cWaking > 0);
                if (    !ASMAtomicDecU32(&pEventMultiInt->cWaking)
                    &&  pEventMultiInt->u32Magic != RTSEMEVENTMULTI_MAGIC)
                {
                    /* The event was destroyed (ulData == VINF_SUCCESS if it was after we awoke), as
                       the last thread do the cleanup. */
                    KernReleaseSpinLock(&pEventMultiInt->Spinlock);
                    KernFreeSpinLock(&pEventMultiInt->Spinlock);
                    RTMemFree(pEventMultiInt);
                    return VINF_SUCCESS;
                }
                rc = VINF_SUCCESS;
                break;

            case ERROR_TIMEOUT:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                ASMAtomicDecU32(&pEventMultiInt->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case ERROR_INTERRUPT:
                Assert(fInterruptible);
                ASMAtomicDecU32(&pEventMultiInt->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            default:
                AssertMsgFailed(("rc=%d\n", rc));
                rc = VERR_GENERAL_FAILURE;
                break;
        }
    }

    KernReleaseSpinLock(&pEventMultiInt->Spinlock);
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

