/* $Id$ */
/** @file
 * IPRT - Multiple Release Event Semaphores, Ring-0 Driver, Linux.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-linux-kernel.h"
#include "internal/iprt.h"
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
 * Linux event semaphore.
 */
typedef struct RTSEMEVENTMULTIINTERNAL
{
    /** Magic value (RTSEMEVENTMULTI_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The object status - !0 when signaled and 0 when reset. */
    uint32_t volatile   fState;
    /** The wait queue. */
    wait_queue_head_t   Head;
} RTSEMEVENTMULTIINTERNAL, *PRTSEMEVENTMULTIINTERNAL;



RTDECL(int)  RTSemEventMultiCreate(PRTSEMEVENTMULTI phEventMultiSem)
{
    return RTSemEventMultiCreateEx(phEventMultiSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, NULL);
}


RTDECL(int)  RTSemEventMultiCreateEx(PRTSEMEVENTMULTI phEventMultiSem, uint32_t fFlags, RTLOCKVALCLASS hClass,
                                     const char *pszNameFmt, ...)
{
    PRTSEMEVENTMULTIINTERNAL pThis;

    AssertReturn(!(fFlags & ~RTSEMEVENTMULTI_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);
    pThis = (PRTSEMEVENTMULTIINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic = RTSEMEVENTMULTI_MAGIC;
        pThis->fState   = 0;
        init_waitqueue_head(&pThis->Head);

        *phEventMultiSem = pThis;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}
RT_EXPORT_SYMBOL(RTSemEventMultiCreate);


RTDECL(int) RTSemEventMultiDestroy(RTSEMEVENTMULTI hEventMultiSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (pThis == NIL_RTSEMEVENTMULTI)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("%p u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_PARAMETER);

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicWriteU32(&pThis->u32Magic, ~RTSEMEVENTMULTI_MAGIC);
    ASMAtomicXchgU32(&pThis->fState, 0);
    Assert(!waitqueue_active(&pThis->Head));
    wake_up_all(&pThis->Head);
    RTMemFree(pThis);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemEventMultiDestroy);


RTDECL(int) RTSemEventMultiSignal(RTSEMEVENTMULTI hEventMultiSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("%p u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_PARAMETER);

    /*
     * Signal the event object.
     */
    ASMAtomicXchgU32(&pThis->fState, 1);
    wake_up_all(&pThis->Head);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemEventMultiSignal);


RTDECL(int) RTSemEventMultiReset(RTSEMEVENTMULTI hEventMultiSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("%p u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_PARAMETER);

    /*
     * Reset it.
     */
    ASMAtomicXchgU32(&pThis->fState, 0);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemEventMultiReset);


/**
 * Worker for RTSemEventMulti and RTSemEventMultiNoResume.
 *
 * @returns VBox status code.
 * @param   pThis           The event semaphore.
 * @param   cMillies            The number of milliseconds to wait.
 * @param   fInterruptible      Whether it's an interruptible wait or not.
 */
static int rtSemEventMultiWait(PRTSEMEVENTMULTIINTERNAL pThis, RTMSINTERVAL cMillies, bool fInterruptible)
{
    /*
     * Ok wait for it.
     */
    DEFINE_WAIT(Wait);
    int     rc       = VINF_SUCCESS;
    long    lTimeout = cMillies == RT_INDEFINITE_WAIT ? MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(cMillies);
#ifdef IPRT_DEBUG_SEMS
    snprintf(current->comm, sizeof(current->comm), "E%lx", IPRT_DEBUG_SEMS_ADDRESS(pThis));
#endif
    for (;;)
    {
        /* make everything thru schedule() atomic scheduling wise. */
        prepare_to_wait(&pThis->Head, &Wait, fInterruptible ? TASK_INTERRUPTIBLE : TASK_UNINTERRUPTIBLE);

        /* check the condition. */
        if (pThis->fState)
            break;

        /* check for pending signals. */
        if (fInterruptible && signal_pending(current))
        {
            rc = VERR_INTERRUPTED;
            break;
        }

        /* wait */
        lTimeout = schedule_timeout(lTimeout);

        after_wait(&Wait);

        /* Check if someone destroyed the semaphore while we were waiting. */
        if (pThis->u32Magic != RTSEMEVENTMULTI_MAGIC)
        {
            rc = VERR_SEM_DESTROYED;
            break;
        }

        /* check for timeout. */
        if (!lTimeout)
        {
            rc = VERR_TIMEOUT;
            break;
        }
    }

    finish_wait(&pThis->Head, &Wait);
#ifdef IPRT_DEBUG_SEMS
    snprintf(current->comm, sizeof(current->comm), "E%lx:%d", IPRT_DEBUG_SEMS_ADDRESS(pThis), rc);
#endif
    return rc;
}


RTDECL(int) RTSemEventMultiWait(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("%p u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_PARAMETER);

    if (pThis->fState)
        return VINF_SUCCESS;
    return rtSemEventMultiWait(pThis, cMillies, false /* fInterruptible */);
}
RT_EXPORT_SYMBOL(RTSemEventMultiWait);


RTDECL(int) RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI hEventMultiSem, RTMSINTERVAL cMillies)
{
    PRTSEMEVENTMULTIINTERNAL pThis = (PRTSEMEVENTMULTIINTERNAL)hEventMultiSem;
    if (!pThis)
        return VERR_INVALID_PARAMETER;
    AssertPtrReturn(pThis, VERR_INVALID_PARAMETER);
    AssertMsgReturn(pThis->u32Magic == RTSEMEVENTMULTI_MAGIC, ("%p u32Magic=%RX32\n", pThis, pThis->u32Magic), VERR_INVALID_PARAMETER);

    if (pThis->fState)
        return VINF_SUCCESS;
    return rtSemEventMultiWait(pThis, cMillies, true /* fInterruptible */);
}
RT_EXPORT_SYMBOL(RTSemEventMultiWaitNoResume);

