/* $Id$ */
/** @file
 * Incredibly Portable Runtime - Mutex Semaphores, Ring-0 Driver, Linux.
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



RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)RTMemAlloc(sizeof(*pMutexInt));
    if (pMutexInt)
    {
        pMutexInt->u32Magic = RTSEMMUTEX_MAGIC;
        init_waitqueue_head(&pMutexInt->Head);
        *pMutexSem = pMutexInt;
AssertReleaseMsgFailed(("This mutex implementation is buggy, fix it!\n"));
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (!pMutexInt)
        return VERR_INVALID_PARAMETER;
    if (pMutexInt->u32Magic != RTSEMMUTEX_MAGIC)
    {
        AssertMsgFailed(("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt->u32Magic, pMutexInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicIncU32(&pMutexInt->u32Magic);
    ASMAtomicXchgU32(&pMutexInt->cRecursion, 0);
    Assert(!waitqueue_active(&pMutexInt->Head));
    wake_up_all(&pMutexInt->Head);
    RTMemFree(pMutexInt);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRequest(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (!pMutexInt)
        return VERR_INVALID_PARAMETER;
    if (    !pMutexInt
        ||  pMutexInt->u32Magic != RTSEMMUTEX_MAGIC)
    {
        AssertMsgFailed(("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt ? pMutexInt->u32Magic : 0, pMutexInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Check for recursive request.
     */
    if (pMutexInt->pOwner == current)
    {
        Assert(pMutexInt->cRecursion < 1000);
        ASMAtomicIncU32(&pMutexInt->cRecursion);
        return VINF_SUCCESS;
    }

    /*
     * Try aquire it.
     */
    if (ASMAtomicCmpXchgU32(&pMutexInt->cRecursion, 1, 0))
    {
        ASMAtomicXchgPtr(&pMutexInt->pOwner, current);
        return VINF_SUCCESS;
    }
    else
    {
        /*
         * Ok wait for it.
         */
        DEFINE_WAIT(Wait);
        int     rc       = VINF_SUCCESS;
        long    lTimeout = cMillies == RT_INDEFINITE_WAIT ? MAX_SCHEDULE_TIMEOUT : msecs_to_jiffies(cMillies);
        for (;;)
        {
            /* make everything thru schedule() atomic scheduling wise. */
            prepare_to_wait(&pMutexInt->Head, &Wait, TASK_INTERRUPTIBLE);

            /* check the condition. */
            if (ASMAtomicCmpXchgU32(&pMutexInt->cRecursion, 1, 0))
            {
                ASMAtomicXchgPtr(&pMutexInt->pOwner, current);
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

            /* Check if someone destroyed the semaphore while we was waiting. */
            if (pMutexInt->u32Magic != RTSEMMUTEX_MAGIC)
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
        finish_wait(&pMutexInt->Head, &Wait);
        return rc;
    }
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    /*
     * Validate input.
     */
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)MutexSem;
    if (!pMutexInt)
        return VERR_INVALID_PARAMETER;
    if (    !pMutexInt
        ||  pMutexInt->u32Magic != RTSEMMUTEX_MAGIC)
    {
        AssertMsgFailed(("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt ? pMutexInt->u32Magic : 0, pMutexInt));
        return VERR_INVALID_PARAMETER;
    }
    if (pMutexInt->pOwner != current)
    {
        AssertMsgFailed(("Not owner, pOwner=%p current=%p\n", (void *)pMutexInt->pOwner, (void *)current));
        return VERR_NOT_OWNER;
    }

    /*
     * Release the mutex.
     */
    if (pMutexInt->cRecursion == 1)
    {
        ASMAtomicXchgPtr(&pMutexInt->pOwner, NULL);
        ASMAtomicXchgU32(&pMutexInt->cRecursion, 0);
    }
    else
        ASMAtomicDecU32(&pMutexInt->cRecursion);

    return VINF_SUCCESS;
}

