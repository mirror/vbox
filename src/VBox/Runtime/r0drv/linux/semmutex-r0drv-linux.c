/* $Id$ */
/** @file
 * IPRT - Mutex Semaphores, Ring-0 Driver, Linux.
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
 * Linux mutex semaphore.
 */
typedef struct RTSEMMUTEXINTERNAL
{
    /** Magic value (RTSEMMUTEX_MAGIC). */
    uint32_t volatile   u32Magic;
    /** Number of recursive locks - 0 if not owned by anyone, > 0 if owned. */
    uint32_t volatile   cRecursion;
    /** The wait queue. */
    wait_queue_head_t   Head;
    /** The current owner. */
    void * volatile     pOwner;
} RTSEMMUTEXINTERNAL, *PRTSEMMUTEXINTERNAL;



RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX phMutexSem)
{
    return RTSemMutexCreateEx(phMutexSem, 0 /*fFlags*/, NIL_RTLOCKVALCLASS, RTLOCKVAL_SUB_CLASS_NONE, NULL);
}


RTDECL(int)  RTSemMutexCreateEx(PRTSEMMUTEX phMutexSem, uint32_t fFlags,
                                RTLOCKVALCLASS hClass, uint32_t uSubClass, const char *pszNameFmt, ...)
{
    PRTSEMMUTEXINTERNAL pThis;

    AssertReturn(!(fFlags & ~RTSEMMUTEX_FLAGS_NO_LOCK_VAL), VERR_INVALID_PARAMETER);

    pThis = (PRTSEMMUTEXINTERNAL)RTMemAlloc(sizeof(*pThis));
    if (pThis)
    {
        pThis->u32Magic   = RTSEMMUTEX_MAGIC;
        pThis->cRecursion = 0;
        pThis->pOwner     = NULL;
        init_waitqueue_head(&pThis->Head);

        *phMutexSem = pThis;
AssertReleaseMsgFailed(("This mutex implementation is buggy, fix it!\n"));
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}
RT_EXPORT_SYMBOL(RTSemMutexCreate);


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (pThis == NIL_RTSEMMUTEX)
        return VINF_SUCCESS;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Invalidate it and signal the object just in case.
     */
    AssertReturn(ASMAtomicCmpXchgU32(&pThis->u32Magic, RTSEMMUTEX_MAGIC_DEAD, RTSEMMUTEX_MAGIC), VERR_INVALID_HANDLE);
    ASMAtomicWriteU32(&pThis->cRecursion, 0);
    Assert(!waitqueue_active(&pThis->Head));
    wake_up_all(&pThis->Head);

    RTMemFree(pThis);
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemMutexDestroy);


#undef RTSemMutexRequest
RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    int                 rc    = VINF_SUCCESS;
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)MutexSem;

    /*
     * Validate input.
     */
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, VERR_INVALID_HANDLE);

    /*
     * Check for recursive request.
     */
    if (pThis->pOwner == current)
    {
        Assert(pThis->cRecursion < 1000);
        ASMAtomicIncU32(&pThis->cRecursion);
        return VINF_SUCCESS;
    }

    /*
     * Try aquire it.
     */
    if (ASMAtomicCmpXchgU32(&pThis->cRecursion, 1, 0))
        ASMAtomicWritePtr(&pThis->pOwner, current);
    else
    {
        /*
         * Ok wait for it.
         */
        DEFINE_WAIT(Wait);
        long    lTimeout = cMillies == RT_INDEFINITE_WAIT ? MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(cMillies);
        for (;;)
        {
            /* make everything thru schedule() atomic scheduling wise. */
            prepare_to_wait(&pThis->Head, &Wait, TASK_INTERRUPTIBLE);

            /* check the condition. */
            if (ASMAtomicCmpXchgU32(&pThis->cRecursion, 1, 0))
            {
                ASMAtomicWritePtr(&pThis->pOwner, current);
                break;
            }

            /* check for pending signals. */
            if (signal_pending(current))
            {
                rc = VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
                break;
            }

            /* wait */
            lTimeout = schedule_timeout(lTimeout);

            after_wait(&Wait);

            /* Check if someone destroyed the semaphore while we was waiting. */
            if (pThis->u32Magic != RTSEMMUTEX_MAGIC)
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
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTSemMutexRequest);


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pThis = (PRTSEMMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTSEMMUTEX_MAGIC, VERR_INVALID_HANDLE);

    AssertReturn(pThis->pOwner == current, VERR_NOT_OWNER);

    /*
     * Release the mutex.
     */
    if (pThis->cRecursion == 1)
    {
        ASMAtomicWritePtr(&pThis->pOwner, NULL);
        ASMAtomicWriteU32(&pThis->cRecursion, 0);
        wake_up(&pThis->Head);
    }
    else
        ASMAtomicDecU32(&pThis->cRecursion);

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemMutexRelease);

