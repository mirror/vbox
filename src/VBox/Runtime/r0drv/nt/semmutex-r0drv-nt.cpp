/* $Id$ */
/** @file
 * innotek Portable Runtime - Mutex Semaphores, Ring-0 Driver, NT.
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
#include "the-nt-kernel.h"
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
 * NT mutex semaphore.
 */
typedef struct RTSEMMUTEXINTERNAL
{
    /** Magic value (RTSEMMUTEX_MAGIC). */
    uint32_t volatile   u32Magic;
#ifdef RT_USE_FAST_MUTEX
    /** The fast mutex object. */
    FAST_MUTEX          Mutex;
#else
    /** The NT Mutex object. */
    KMUTEX              Mutex;
#endif
} RTSEMMUTEXINTERNAL, *PRTSEMMUTEXINTERNAL;



RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    Assert(sizeof(RTSEMMUTEXINTERNAL) > sizeof(void *));
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)RTMemAlloc(sizeof(*pMutexInt));
    if (pMutexInt)
    {
        pMutexInt->u32Magic = RTSEMMUTEX_MAGIC;
#ifdef RT_USE_FAST_MUTEX
        ExInitializeFastMutex(&pMutexInt->Mutex);
#else
        KeInitializeMutex(&pMutexInt->Mutex, 0);
#endif
        *pMutexSem = pMutexInt;
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
     * Get the mutex.
     */
#ifdef RT_USE_FAST_MUTEX
    AssertMsg(cMillies == RT_INDEFINITE_WAIT, ("timeouts are not supported when using fast mutexes!\n"));
    ExAcquireFastMutex(&pMutexInt->Mutex);
#else
    NTSTATUS rcNt;
    if (cMillies == RT_INDEFINITE_WAIT)
        rcNt = KeWaitForSingleObject(&pMutexInt->Mutex, Executive, KernelMode, TRUE, NULL);
    else
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -(int64_t)cMillies * 10000;
        rcNt = KeWaitForSingleObject(&pMutexInt->Mutex, Executive, KernelMode, TRUE, &Timeout);
    }
    switch (rcNt)
    {
        case STATUS_SUCCESS:
            if (pMutexInt->u32Magic == RTSEMMUTEX_MAGIC)
                return VINF_SUCCESS;
            return VERR_SEM_DESTROYED;
        case STATUS_ALERTED:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_USER_APC:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_TIMEOUT:
            return VERR_TIMEOUT;
        default:
            AssertMsgFailed(("pMutexInt->u32Magic=%RX32 pMutexInt=%p: wait returned %lx!\n",
                             pMutexInt->u32Magic, pMutexInt, (long)rcNt));
            return VERR_INTERNAL_ERROR;
    }
#endif
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

    /*
     * Release the mutex.
     */
#ifdef RT_USE_FAST_MUTEX
    ExReleaseFastMutex(&pMutexInt->Mutex);
#else
    KeReleaseMutex(&pMutexInt->Mutex, FALSE);
#endif
    return VINF_SUCCESS;
}

