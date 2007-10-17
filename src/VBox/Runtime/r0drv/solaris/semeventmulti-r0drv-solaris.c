/* $Id$ */
/** @file
 * innotek Portable Runtime - Multiple Release Event Semaphores, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"

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
    /** The Solaris mutex protecting this structure and pairing up the with the cv. */
    kmutex_t            Mtx;
    /** The Solaris condition variable. */
    kcondvar_t          Cnd;
} RTSEMEVENTMULTIINTERNAL, *PRTSEMEVENTMULTIINTERNAL;


RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventMultiSem)
{
    Assert(sizeof(RTSEMEVENTMULTIINTERNAL) > sizeof(void *));
    AssertPtrReturn(pEventMultiSem, VERR_INVALID_POINTER);

    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pThis->cWaiters = 0;
        pThis->cWaking = 0;
        pThis->fSignaled = 0;
        mutex_init(&pThis->Mtx, "IPRT Multiple Release Event Semaphore", MUTEX_DRIVER, NULL);
        cv_init(&pThis->Cnd, "IPRT CV", CV_DRIVER, NULL);
        *pEventMultiSem = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    if (EventMultiSem == NIL_RTSEMEVENTMULTI)     /* don't bitch */
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    mutex_enter(&pThis->Mtx);
    ASMAtomicIncU32(&pThis->u32Magic); /* make the handle invalid */
    if (pThis->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        cv_broadcast(&pThis->Cnd);
        mutex_exit(&pThis->Mtx);
    }
    else if (pThis->cWaking)
        /* the last waking thread is gonna do the cleanup */
        mutex_exit(&pThis->Mtx);
    else
    {
        mutex_exit(&pThis->Mtx);
        cv_destroy(&pThis->Cnd);
        mutex_destroy(&pThis->Mtx);
        RTMemFree(pThis);
    }

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    mutex_enter(&pThis->Mtx);

    ASMAtomicXchgU8(&pThis->fSignaled, true);
    if (pThis->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pThis->cWaking, pThis->cWaking + pThis->cWaiters);
        ASMAtomicXchgU32(&pThis->cWaiters, 0);
        cv_signal(&pThis->Cnd);
    }

    mutex_exit(&pThis->Mtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    mutex_enter(&pThis->Mtx);
    ASMAtomicXchgU8(&pThis->fSignaled, false);
    mutex_exit(&pThis->Mtx);
    return VINF_SUCCESS;
}


static int rtSemEventMultiWait(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies, bool fInterruptible)
{
    int rc;
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pThis=%p u32Magic=%#x\n", pThis, pThis->u32Magic),
                    VERR_INVALID_HANDLE);

    mutex_enter(&pThis->Mtx);

    if (pThis->fSignaled)
        rc = VINF_SUCCESS;
    else
    {
        ASMAtomicIncU32(&pThis->cWaiters);

        /*
         * Translate milliseconds into ticks and go to sleep.
         */
        if (cMillies != RT_INDEFINITE_WAIT)
        {
            int cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
            clock_t timeout = ddi_get_lbolt();
            timeout += cTicks;
            if (fInterruptible)
                rc = cv_timedwait_sig(&pThis->Cnd, &pThis->Mtx, timeout);
            else
                rc = cv_timedwait(&pThis->Cnd, &pThis->Mtx, timeout);
        }
        else
        {
            if (fInterruptible)
                rc = cv_wait_sig(&pThis->Cnd, &pThis->Mtx);
            else
            {
                cv_wait(&pThis->Cnd, &pThis->Mtx);
                rc = 1;
            }
        }
        if (rc > 0)
        {
            /* Retured due to call to cv_signal() or cv_broadcast() */
            if (RT_LIKELY(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC))
                rc = VINF_SUCCESS;
            else
            {
                rc = VERR_SEM_DESTROYED;
                if (!ASMAtomicDecU32(&pThis->cWaking))
                {
                    mutex_exit(&pThis->Mtx);
                    cv_destroy(&pThis->Cnd);
                    mutex_destroy(&pThis->Mtx);
                    RTMemFree(pThis);
                    return rc;
                }
            }
            ASMAtomicDecU32(&pThis->cWaking);
        }
        else if (rc == -1)
        {
            /* Returned due to timeout being reached */
            if (pThis->cWaiters > 0)
                ASMAtomicDecU32(&pThis->cWaiters);
            rc = VERR_TIMEOUT;
        }
        else
        {
            /* Returned due to pending signal */
            if (pThis->cWaiters > 0)
                ASMAtomicDecU32(&pThis->cWaiters);
            rc = VERR_INTERRUPTED;
        }
    }

    mutex_exit(&pThis->Mtx);
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

