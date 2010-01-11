/* $Id$ */
/** @file
 * IPRT - Multiple Release Event Semaphores, Ring-0 Driver, OS/2.
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


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI phEventMultiSem)
{
    return RTSemEventMultiCreateEx(phEventMultiSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventMultiCreateEx(PRTSEMEVENTMULTI phEventMultiSem, uint32_t fFlags, RTLOCKVALCLASS hClass,
                                     const char *pszNameFmt, ...)
{
    AssertReturn(!(fFlags & ~RTSEMEVENTMULTI_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    AssertPtrReturn(phEventMultiSem, VERR_INVALID_POINTER);

    AssertCompile(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pThis->cWaiters = 0;
        pThis->cWaking = 0;
        pThis->fSignaled = 0;
        KernAllocSpinLock(&pThis->Spinlock);

        *phEventMultiSem = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (pThis == NIL_RTSEMEVENTMULTI)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic), VERR_INVALID_HANDLE);

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


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pThis->Spinlock);

    ASMAtomicXchgU8(&pThis->fSignaled, true);
    if (pThis->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        ASMAtomicXchgU32(&pThis->cWaiters, 0);
        ULONG cThreads;
        KernWakeup((ULONG)pThis, WAKEUP_DATA, &cThreads, VINF_SUCCESS);
    }

    KernReleaseSpinLock(&pThis->Spinlock);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI hEventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pThis->Spinlock);
    ASMAtomicXchgU8(&pThis->fSignaled, false);
    KernReleaseSpinLock(&pThis->Spinlock);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies, bool fInterruptible)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    KernAcquireSpinLock(&pThis->Spinlock);

    int rc;
    if (pThis->fSignaled)
        rc = VINF_SUCCESS;
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
                    &&  pThis->u32Magic != RTSEMEVENTMULTI_MAGIC)
                {
                    /* The event was destroyed (ulData == VINF_SUCCESS if it was after we awoke), as
                       the last thread do the cleanup. */
                    KernReleaseSpinLock(&pThis->Spinlock);
                    KernFreeSpinLock(&pThis->Spinlock);
                    RTMemFree(pThis);
                    return VINF_SUCCESS;
                }
                rc = VINF_SUCCESS;
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


RTDECL(int)  RTSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, false /* not interruptible */);
}


RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI hEventMultiSem, unsigned cMillies)
{
    return rtSemEventMultiWait(hEventMultiSem, cMillies, true /* interruptible */);
}

