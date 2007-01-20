/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Semaphores, Ring-0 Driver, Darwin.
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
 */



/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "the-darwin-kernel.h"
#include <iprt/semaphore.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
#if 0 /** @todo */
/**
 * Darwin event semaphore.
 */
typedef struct RTSEMEVENTINTERNAL
{
    /** Magic value (RTSEMEVENT_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The NT Event object. */
    KEVENT              Event;
} RTSEMEVENTINTERNAL, *PRTSEMEVENTINTERNAL;

/** Magic for the Darwin event semaphore structure. (Neil Gaiman) */
#define RTSEMEVENT_MAGIC 0x19601110
#endif

#if 0 /** @todo */
/**
 * Darwin mutex semaphore.
 */
typedef struct RTSEMMUTEXINTERNAL
{
    /** Magic value (RTSEMMUTEX_MAGIC). */
    uint32_t volatile   u32Magic;
    /** The mutex. */
    lck_mtx_t          *pMtx;
} RTSEMMUTEXINTERNAL, *PRTSEMMUTEXINTERNAL;

/** Magic for the Darwin mutex semaphore structure. (Douglas Adams) */
#define RTSEMMUTEX_MAGIC 0x19520311
#endif


/**
 * Wrapper for the darwin semaphore structure.
 */
typedef struct RTSEMFASTMUTEXINTERNAL
{
    /** Magic value (RTSEMFASTMUTEX_MAGIC). */
    uint32_t            u32Magic;
    /** The mutex. */
    lck_mtx_t          *pMtx;
} RTSEMFASTMUTEXINTERNAL, *PRTSEMFASTMUTEXINTERNAL;

/** Magic value for RTSEMFASTMUTEXINTERNAL::u32Magic (John Ronald Reuel Tolkien). */
#define RTSEMFASTMUTEX_MAGIC    0x18920102


#if 0/** @todo */
RTDECL(int)  RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    Assert(sizeof(RTSEMEVENTINTERNAL) > sizeof(void *));
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)RTMemAlloc(sizeof(*pEventInt));
    if (pEventInt)
    {
        pEventInt->u32Magic = RTSEMEVENT_MAGIC;
        wait_queue_init
        KeInitializeEvent(&pEventInt->Event, SynchronizationEvent, FALSE);
        *pEventSem = pEventInt;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    if (!pEventInt)
        return VERR_INVALID_PARAMETER;
    if (pEventInt->u32Magic != RTSEMEVENT_MAGIC)
    {
        AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p\n", pEventInt->u32Magic, pEventInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicIncU32(&pEventInt->u32Magic);
    KeSetEvent(&pEventInt->Event, 0xfff, FALSE);
    RTMemFree(pEventInt);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    if (!pEventInt)
        return VERR_INVALID_PARAMETER;
    if (    !pEventInt
        ||  pEventInt->u32Magic != RTSEMEVENT_MAGIC)
    {
        AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p\n", pEventInt ? pEventInt->u32Magic : 0, pEventInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Signal the event object.
     */
    KeSetEvent(&pEventInt->Event, 1, FALSE);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventWait(RTSEMEVENT EventSem, unsigned cMillies)
{
    /*
     * Validate input.
     */
    PRTSEMEVENTINTERNAL pEventInt = (PRTSEMEVENTINTERNAL)EventSem;
    if (!pEventInt)
        return VERR_INVALID_PARAMETER;
    if (    !pEventInt
        ||  pEventInt->u32Magic != RTSEMEVENT_MAGIC)
    {
        AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p\n", pEventInt ? pEventInt->u32Magic : 0, pEventInt));
        return VERR_INVALID_PARAMETER;
    }

    /*
     * Wait for it.
     */
    NTSTATUS rcNt;
    if (cMillies == RT_INDEFINITE_WAIT)
        rcNt = KeWaitForSingleObject(&pEventInt->Event, Executive, KernelMode, TRUE, NULL);
    else
    {
        LARGE_INTEGER Timeout;
        Timeout.QuadPart = -(int64_t)cMillies * 10000;
        rcNt = KeWaitForSingleObject(&pEventInt->Event, Executive, KernelMode, TRUE, &Timeout);
    }
    switch (rcNt)
    {
        case STATUS_SUCCESS:
            if (pEventInt->u32Magic == RTSEMEVENT_MAGIC)
                return VINF_SUCCESS;
            return VERR_SEM_DESTROYED;
        case STATUS_ALERTED:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_USER_APC:
            return VERR_INTERRUPTED; /** @todo VERR_INTERRUPTED isn't correct anylonger. please fix r0drv stuff! */
        case STATUS_TIMEOUT:
            return VERR_TIMEOUT;
        default:
            AssertMsgFailed(("pEventInt->u32Magic=%RX32 pEventInt=%p: wait returned %lx!\n",
                             pEventInt->u32Magic, pEventInt, (long)rcNt));
            return VERR_INTERNAL_ERROR;
    }
}
#endif /* todo */



#if 0 /* need proper timeout lock function! */
RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    AssertCompile(sizeof(RTSEMMUTEXINTERNAL) > sizeof(void *));
    PRTSEMMUTEXINTERNAL pMutexInt = (PRTSEMMUTEXINTERNAL)RTMemAlloc(sizeof(*pMutexInt));
    if (pMutexInt)
    {
        pMutexInt->u32Magic = RTSEMMUTEX_MAGIC;
        Assert(g_pDarwinLockGroup);
        pMutexInt->pMtx = lck_mtx_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pMutexInt->pMtx)
        {
            *pMutexSem = pMutexInt;
            return VINF_SUCCESS;
        }
        RTMemFree(pMutexInt);
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
    AssertPtrReturn(pMutexInt, VERR_INVALID_POINTER);
    AssertMsg(pMutexInt->u32Magic == RTSEMMUTEX_MAGIC,
              ("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt->u32Magic, pMutexInt)
              VERR_INVALID_PARAMETER);

    /*
     * Invalidate it and signal the object just in case.
     */
    ASMAtomicIncU32(&pMutexInt->u32Magic);

    Assert(g_pDarwinLockGroup);
    lck_mtx_free(pMutexInt->pMtx, g_pDarwinLockGroup);
    pMutexInt->pMtx = NULL;

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
    AssertPtrReturn(pMutexInt, VERR_INVALID_POINTER);
    AssertMsg(pMutexInt->u32Magic == RTSEMMUTEX_MAGIC,
              ("pMutexInt->u32Magic=%RX32 pMutexInt=%p\n", pMutexInt->u32Magic, pMutexInt)
              VERR_INVALID_PARAMETER);

    /*
     * Get the mutex.
     */
    wait_result_t rc = lck_mtx_lock_deadlink
#if 1
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

#endif /* later */




RTDECL(int)  RTSemFastMutexCreate(PRTSEMFASTMUTEX pMutexSem)
{
    AssertCompile(sizeof(RTSEMFASTMUTEXINTERNAL) > sizeof(void *));
    AssertPtrReturn(VALID_PTR(pMutexSem), VERR_INVALID_POINTER);

    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)RTMemAlloc(sizeof(*pFastInt));
    if (pFastInt)
    {
        pFastInt->u32Magic = RTSEMFASTMUTEX_MAGIC;
        Assert(g_pDarwinLockGroup);
        pFastInt->pMtx = lck_mtx_alloc_init(g_pDarwinLockGroup, LCK_ATTR_NULL);
        if (pFastInt->pMtx)
        {
            *pMutexSem = pFastInt;
            return VINF_SUCCESS;
        }

        RTMemFree(pFastInt);
    }
    return VERR_NO_MEMORY;
}


RTDECL(int)  RTSemFastMutexDestroy(RTSEMFASTMUTEX MutexSem)
{
    if (MutexSem == NIL_RTSEMFASTMUTEX) /* don't bitch */
        return VERR_INVALID_PARAMETER;
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertPtrReturn(VALID_PTR(pFastInt), VERR_INVALID_POINTER);
    AssertMsgReturn(pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", pFastInt->u32Magic, pFastInt),
                    VERR_INVALID_PARAMETER);

    ASMAtomicIncU32(&pFastInt->u32Magic); /* make the handle invalid. */
    Assert(g_pDarwinLockGroup);
    lck_mtx_free(pFastInt->pMtx, g_pDarwinLockGroup);
    pFastInt->pMtx = NULL;
    RTMemFree(pFastInt);

    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRequest(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertMsgReturn(VALID_PTR(pFastInt) && pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", VALID_PTR(pFastInt) ? pFastInt->u32Magic : 0, pFastInt),
                    VERR_INVALID_PARAMETER);
    lck_mtx_lock(pFastInt->pMtx);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemFastMutexRelease(RTSEMFASTMUTEX MutexSem)
{
    PRTSEMFASTMUTEXINTERNAL pFastInt = (PRTSEMFASTMUTEXINTERNAL)MutexSem;
    AssertMsgReturn(VALID_PTR(pFastInt) && pFastInt->u32Magic == RTSEMFASTMUTEX_MAGIC,
                    ("pFastInt->u32Magic=%RX32 pFastInt=%p\n", VALID_PTR(pFastInt) ? pFastInt->u32Magic : 0, pFastInt),
                    VERR_INVALID_PARAMETER);
    lck_mtx_unlock(pFastInt->pMtx);
    return VINF_SUCCESS;
}

