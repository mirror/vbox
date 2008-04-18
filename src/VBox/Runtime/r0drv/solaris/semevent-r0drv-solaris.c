/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Semaphores, Ring-0 Driver, Solaris.
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-solaris-kernel.h"
#include <time.h>

#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Solaris event semaphore.
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
    /** The Solaris mutex protecting this structure and pairing up the with the cv. */
    kmutex_t            Mtx;
    /** The Solaris condition variable. */
    kcondvar_t          Cnd;
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
        mutex_init(&pEventInt->Mtx, "IPRT Event Semaphore", MUTEX_DRIVER, NULL);
        cv_init(&pEventInt->Cnd, "IPRT CV", CV_DRIVER, NULL);
        *pEventSem = pEventInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    if (EventSem == NIL_RTSEMEVENT)
        return VERR_INVALID_HANDLE;
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    AssertPtrReturn(pEventInt, VERR_INVALID_HANDLE);
    AssertMsgReturn(pEventInt->u32Magic == RTSEMEVENT_MAGIC,
                    ("pEventInt=%p u32Magic=%#x\n", pEventInt, pEventInt->u32Magic),
                    VERR_INVALID_HANDLE);

    mutex_enter(&pEventInt->Mtx);
    ASMAtomicIncU32(&pEventInt->u32Magic); /* make the handle invalid */
    if (pEventInt->cWaiters > 0)
    {
        /* abort waiting thread, last man cleans up. */
        ASMAtomicXchgU32(&pEventInt->cWaking, pEventInt->cWaking + pEventInt->cWaiters);
        cv_broadcast(&pEventInt->Cnd);
        mutex_exit(&pEventInt->Mtx);
    }
    else if (pEventInt->cWaking)
    {
        /* the last waking thread is gonna do the cleanup */
        mutex_exit(&pEventInt->Mtx);
    }
    else
    {
        mutex_exit(&pEventInt->Mtx);
        cv_destroy(&pEventInt->Cnd);
        mutex_destroy(&pEventInt->Mtx);
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

    mutex_enter(&pEventInt->Mtx);

    if (pEventInt->cWaiters > 0)
    {
        ASMAtomicDecU32(&pEventInt->cWaiters);
        ASMAtomicIncU32(&pEventInt->cWaking);
        cv_signal(&pEventInt->Cnd);
    }
    else
        ASMAtomicXchgU8(&pEventInt->fSignaled, true);

    mutex_exit(&pEventInt->Mtx);
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

    mutex_enter(&pEventInt->Mtx);

    if (pEventInt->fSignaled)
    {
        Assert(!pEventInt->cWaiters);
        ASMAtomicXchgU8(&pEventInt->fSignaled, false);
        rc = VINF_SUCCESS;
    }
    else
    {
        ASMAtomicIncU32(&pEventInt->cWaiters);
        
        /*
         * Translate milliseconds into ticks and go to sleep.
         */
        if (cMillies != RT_INDEFINITE_WAIT)
        {
            clock_t cTicks = drv_usectohz((clock_t)(cMillies * 1000L));
            clock_t timeout = ddi_get_lbolt();
            timeout += cTicks;
            if (fInterruptible)
                rc = cv_timedwait_sig(&pEventInt->Cnd, &pEventInt->Mtx, timeout);
            else
                rc = cv_timedwait(&pEventInt->Cnd, &pEventInt->Mtx, timeout);
        }
        else
        {
            if (fInterruptible)
                rc = cv_wait_sig(&pEventInt->Cnd, &pEventInt->Mtx);
            else
            {
                cv_wait(&pEventInt->Cnd, &pEventInt->Mtx);
                rc = 1;
            }
        }

        if (rc > 0)
        {
            /* Retured due to call to cv_signal() or cv_broadcast() */
            if (pEventInt->u32Magic != RTSEMEVENT_MAGIC)
            {
                rc = VERR_SEM_DESTROYED;
                if (!ASMAtomicDecU32(&pEventInt->cWaking))
                {
                    mutex_exit(&pEventInt->Mtx);
                    cv_destroy(&pEventInt->Cnd);
                    mutex_destroy(&pEventInt->Mtx);
                    RTMemFree(pEventInt);
                    return rc;
                }
            }

            ASMAtomicDecU32(&pEventInt->cWaking);
            rc = VINF_SUCCESS;
        }
        else if (rc == -1)
        {
            /* Returned due to timeout being reached */
            if (pEventInt->cWaiters > 0)
                ASMAtomicDecU32(&pEventInt->cWaiters);
            rc = VERR_TIMEOUT;
        }
        else
        {
            /* Returned due to pending signal */
            if (pEventInt->cWaiters > 0)
                ASMAtomicDecU32(&pEventInt->cWaiters);
            rc = VERR_INTERRUPTED;
        }
    }

    mutex_exit(&pEventInt->Mtx);
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
