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

    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pEventMultiInt));
    if (pEventMultiInt)
    {
        pEventMultiInt->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pEventMultiInt->cWaiters = 0;
        pEventMultiInt->cWaking = 0;
        pEventMultiInt->fSignaled = 0;
        mutex_init(&pEventMultiInt->Mtx, "IPRT Multiple Release Event Semaphore", MUTEX_DRIVER, NULL);
        cv_init(&pEventMultiInt->Cnd, "IPRT CV", CV_DRIVER, NULL);
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

    mutex_enter(&pEventMultiInt->Mtx);
    ASMAtomicIncU32(&pEventMultiInt->u32Magic); /* make the handle invalid */
    if (pEventMultiInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        cv_signal(&pEventMultiInt->Cnd);
        mutex_exit(&pEventMultiInt->Mtx);
    }
    else if (pEventMultiInt->cWaking)
        /* the last waking thread is gonna do the cleanup */
        mutex_exit(&pEventMultiInt->Mtx);
    else
    {
        mutex_exit(&pEventMultiInt->Mtx);
        cv_destroy(&pEventMultiInt->Cnd);
        mutex_destroy(&pEventMultiInt->Mtx);
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

    mutex_enter(&pEventMultiInt->Mtx);

    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, true);
    if (pEventMultiInt->cWaiters > 0)
    {
        ASMAtomicXchgU32(&pEventMultiInt->cWaking, pEventMultiInt->cWaking + pEventMultiInt->cWaiters);
        ASMAtomicXchgU32(&pEventMultiInt->cWaiters, 0);
        cv_signal(&pEventMultiInt->Cnd);
    }

    mutex_exit(&pEventMultiInt->Mtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    PRTSEMEVENTMULTIINTERNAL pEventMultiInt = (PRTSEMEVENTMULTIINTERNAL)EventMultiSem;
    AssertPtrReturn(pEventMultiInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventMultiInt->u32Magic == RTSEMEVENTMULTI_MAGIC,
                    ("pEventMultiInt=%p u32Magic=%#x\n", pEventMultiInt, pEventMultiInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mutex_enter(&pEventMultiInt->Mtx);
    ASMAtomicXchgU8(&pEventMultiInt->fSignaled, false);
    mutex_exit(&pEventMultiInt->Mtx);
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

    mutex_enter(&pEventMultiInt->Mtx);

    if (pEventMultiInt->fSignaled)
        rc = VINF_SUCCESS;
    else
    {
        /*
         * Translate milliseconds into ticks and go to sleep.
         */
        int cTicks;
        clock_t timeout;
        if (cMillies != RT_INDEFINITE_WAIT)
            cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
        else
            cTicks = 0;

        timeout = ddi_get_lbolt();
        timeout += cTicks;
        
        ASMAtomicIncU32(&pEventMultiInt->cWaiters);

        /** @todo r=bird: Is this interruptible or non-interruptible? */
        rc = cv_timedwait_sig(&pEventMultiInt->Cnd, &pEventMultiInt->Mtx, timeout);
        if (rc > 0)
        {
            /* Retured due to call to cv_signal() or cv_broadcast() */
            if (pEventMultiInt->u32Magic != RTSEMEVENT_MAGIC)
            {
                rc = VERR_SEM_DESTROYED;
                if (!ASMAtomicDecU32(&pEventMultiInt->cWaking))
                {
                    mutex_exit(&pEventMultiInt->Mtx);
                    cv_destroy(&pEventMultiInt->Cnd);
                    mutex_destroy(&pEventMultiInt->Mtx);
                    RTMemFree(pEventMultiInt);
                    return rc;
                }
            }

            ASMAtomicDecU32(&pEventMultiInt->cWaking);
            rc = VINF_SUCCESS;
        }
        else if (rc == -1)
        {
            /* Returned due to timeout being reached */
            if (pEventMultiInt->cWaiters > 0)
                ASMAtomicDecU32(&pEventMultiInt->cWaiters);
            rc = VERR_TIMEOUT;
        }
        else
        {
            /* Returned due to pending signal */
            if (pEventMultiInt->cWaiters > 0)
                ASMAtomicDecU32(&pEventMultiInt->cWaiters);
            rc = VERR_INTERRUPTED;
        }
    }

    mutex_exit(&pEventMultiInt->Mtx);
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

