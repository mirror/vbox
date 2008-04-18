/* $Id$ */
/** @file
 * innotek Portable Runtime - Fast Mutex Semaphores, Ring-0 Driver, Linux.
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
#ifdef IPRT_DEBUG_SEMS
# include <iprt/thread.h>
#endif

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Wrapper for the linux semaphore structure.
 */
typedef struct RTSEMFASTMUTEXINTERNAL
{
    /** Magic value (RTSEMFASTMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** the linux semaphore. */
    struct semaphore    Semaphore;
#ifdef IPRT_DEBUG_SEMS
    /** For check. */
    RTNATIVETHREAD volatile Owner;
#endif
} RTSEMFASTMUTEXINTERNAL, *PRTSEMFASTMUTEXINTERNAL;


RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem)
{
    /*
     * Allocate.
     */
    PRTSEMFASTMUTEXINTERNAL pFastInt;
    pFastInt = (PRTSEMFASTMUTEXINTERNAL)RTMemAlloc(sizeof(*pFastInt));
    if (!pFastInt)
        return VERR_NO_MEMORY;

    /*
     * Initialize.
     */
    pFastInt->u32Magic = RTSEMFASTMUTEX_MAGIC;
    sema_init(&pFastInt->Semaphore, 1);
#ifdef IPRT_DEBUG_SEMS
    pFastInt->Owner = NIL_RTNATIVETHREAD;
#endif
    *pMutexSem = pFastInt;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem)
{
    /*
     * Validate.
     */
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    if (!pFastInt)
        return VERR_INVALID_PARAMETER;
    if (pFastInt->u32Magic != RTSEMFASTMUTEX_MAGIC)
    {
        AssertMsgFailed(("pFastInt->u32Magic=%RX32 pMutexInt=%p\n", pFastInt->u32Magic, pFastInt));
        return VERR_INVALID_PARAMETER;
    }

    ASMAtomicIncU32(&pFastInt->u32Magic);
    RTMemFree(pFastInt);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX MutexSem)
{
    /*
     * Validate.
     */
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    if (    !pFastInt
        ||  pFastInt->u32Magic != RTSEMFASTMUTEX_MAGIC)
    {
        AssertMsgFailed(("pFastInt->u32Magic=%RX32 pMutexInt=%p\n", pFastInt ? pFastInt->u32Magic : 0, pFastInt));
        return VERR_INVALID_PARAMETER;
    }

#ifdef IPRT_DEBUG_SEMS
    snprintf(current->comm, TASK_COMM_LEN, "d%lx", IPRT_DEBUG_SEMS_ADDRESS(pFastInt));
#endif
    down(&pFastInt->Semaphore);
#ifdef IPRT_DEBUG_SEMS
    snprintf(current->comm, TASK_COMM_LEN, "o%lx", IPRT_DEBUG_SEMS_ADDRESS(pFastInt));
    AssertRelease(pFastInt->Owner == NIL_RTNATIVETHREAD);
    ASMAtomicUoWriteSize(&pFastInt->Owner, RTThreadNativeSelf());
#endif
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem)
{
    /*
     * Validate.
     */
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    if (    !pFastInt
        ||  pFastInt->u32Magic != RTSEMFASTMUTEX_MAGIC)
    {
        AssertMsgFailed(("pFastInt->u32Magic=%RX32 pMutexInt=%p\n", pFastInt ? pFastInt->u32Magic : 0, pFastInt));
        return VERR_INVALID_PARAMETER;
    }

#ifdef IPRT_DEBUG_SEMS
    AssertRelease(pFastInt->Owner == RTThreadNativeSelf());
    ASMAtomicUoWriteSize(&pFastInt->Owner, NIL_RTNATIVETHREAD);
#endif
    up(&pFastInt->Semaphore);
#ifdef IPRT_DEBUG_SEMS
    snprintf(current->comm, TASK_COMM_LEN, "u%lx", IPRT_DEBUG_SEMS_ADDRESS(pFastInt));
#endif
    return VINF_SUCCESS;
}

