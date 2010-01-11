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


RTDECL(int)  RTSemEventCreate(PRTSEMEVENT phEventSem)
{
    return RTSemEventCreateEx(phEventSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventCreateEx(PRTSEMEVENT phEventSem, uint32_t fFlags, RTLOCKVALCLASS hClass, const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMEVENT_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    AssertCompile(sizeof(RTSEMEVENTINTERNAL) > sizeof(void *));
    AssertPtrReturn(phEventSem, VERR_INVALID_POINTER);

    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;

    pThis->u32Magic = RTSEMEVENT_MAGIC;
    pThis->cWaiters = 0;
    pThis->cWaking = 0;
    pThis->fSignaled = 0;
    KernAllocSpinLock(&pThis->Spinlock);

    *phEventSem = pThis;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT hEventSem)
{
    PRTSEMEVENTINTERNAL pThis = hEventSem;
    if (pThis == NIL_RTSEMEVENT)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pThis->Spinlock);
    ASMAtomicIncU32(&pThis->u32Magic); /* make the handle invalid */
    if (pThis->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        ULONG cThreads;
        KernWakeup((ULONG)pThis, WAKEUP_DATA | WAKEUP_BOOST, &cThreads, (ULONG)VERR_SEM_DESTROYED);
        KernReleaseSpinLock(&pThis->Spinlock);
    }
    else if (pThis->cWaking)
        /* the last waking thread is gonna do the cleanup */
        KernReleaseSpinLock(&pThis->Spinlock);
    else
    {
        KernReleaseSpinLock(&pThis->Spinlock);
        KernFreeSpinLock(&pThis->Spinlock);
        RTMemFree(pThis);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT hEventSem)
{
    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)hEventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pThis->Spinlock);

    if (pThis->cWaiters > 0)
    {
        ASMAtomicDecU32(&pThis->cWaiters);
        ASMAtomicIncU32(&pThis->cWaking);
        ULONG cThreads;
        KernWakeup((ULONG)pThis, WAKEUP_DATA | WAKEUP_ONE, &cThreads, VINF_SUCCESS);
        if (RT_UNLIKELY(!cThreads))
        {
            /* shouldn't ever happen on OS/2 */
            ASMAtomicXchgU8(&pThis->fSignaled, true);
            ASMAtomicDecU32(&pThis->cWaking);
            ASMAtomicIncU32(&pThis->cWaiters);
        }
    }
    else
        ASMAtomicXchgU8(&pThis->fSignaled, true);

    KernReleaseSpinLock(&pThis->Spinlock);
    return VINF_SUCCESS;
}


static int rtSemEventWait(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies, bool fInterruptible)
{
    PRTSEMEVENTINTERNAL pThis = (PRTSEMEVENTINTERNAL)hEventSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENT_MAGIC, ("u32Magic=%RX32 pThis=%p\n", pThis->u32Magic, pThis), VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pThis->Spinlock);

    int rc;
    if (pThis->fSignaled)
    {
        Assert(!pThis->cWaiters);
        ASMAtomicXchgU8(&pThis->fSignaled, false);
        rc = VINF_SUCCESS;
    }
    else
    {
        ASMAtomicIncU32(&pThis->cWaiters);

        ULONG ulData = (ULONG)VERR_INTERNAL_ERROR;
        rc = KernBlock((ULONG)pThis,
                       cMillies == RT_INDEFINITE_WAIT ? SEM_INDEFINITE_WAIT : cMillies,
                       BLOCK_SPINLOCK | (!fInterruptible ? BLOCK_UNINTERRUPTABLE : 0),
                       &pThis->Spinlock,
                       &ulData);
        switch (rc)
        {
            case NO_ERROR:
                rc = (int)ulData;
                Assert(rc == VINF_SUCCESS || rc == VERR_SEM_DESTROYED);
                Assert(pThis->cWaking > 0);
                if (    !ASMAtomicDecU32(&pThis->cWaking)
                    &&  pThis->u32Magic != RTSEMEVENT_MAGIC)
                {
                    /* The event was destroyed (ulData == VINF_SUCCESS if it was after we awoke), as
                       the last thread do the cleanup. */
                    KernReleaseSpinLock(&pThis->Spinlock);
                    KernFreeSpinLock(&pThis->Spinlock);
                    RTMemFree(pThis);
                    return rc;
                }
                break;

            case ERROR_TIMEOUT:
                Assert(cMillies != RT_INDEFINITE_WAIT);
                ASMAtomicDecU32(&pThis->cWaiters);
                rc = VERR_TIMEOUT;
                break;

            case ERROR_INTERRUPT:
                Assert(fInterruptible);
                ASMAtomicDecU32(&pThis->cWaiters);
                rc = VERR_INTERRUPTED;
                break;

            default:
                AssertMsgFailed(("rc=%d\n", rc));
                rc = VERR_GENERAL_FAILURE;
                break;
        }
    }

    KernReleaseSpinLock(&pThis->Spinlock);
    return rc;
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies)
{
    return rtSemEventWait(hEventSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT hEventSem, RTMSINTERVAL cMillies)
{
    return rtSemEventWait(hEventSem, cMillies, true /* interruptible */);
}

