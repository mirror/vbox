/* $Id$ */
/** @file
 * InnoTek Portable Runtime - Semaphores, l4env.
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
#define LOG_GROUP RTLOGGROUP_SEM
#include <l4/l4vm/sync.h>
#include <l4/semaphore/semaphore.h>
#include <l4/thread/thread.h>

#include <iprt/semaphore.h>
#include <iprt/assert.h>
#include <iprt/alloc.h>
#include <iprt/string.h>
#include <iprt/err.h>


RTDECL(int)   RTSemEventCreate(PRTSEMEVENT pEventSem)
{
    l4sync_t *pSync = (l4sync_t*) RTMemAlloc(sizeof(l4sync_t));

    if (l4sync_init() != 0)
    {
        AssertMsgFailed(("Failed to set up the l4env SemEvent library!\n"));
        return VERR_SEM_ERROR;
    }
    if (!pSync)
        return VERR_NO_MEMORY;

    *pSync     = L4SYNC_AUTORESET_INIT(L4SYNC_NOT_SIGNALLED);
    *pEventSem = (RTSEMEVENTINTERNAL*) pSync;
    return VINF_SUCCESS;
}

RTDECL(int)  RTSemEventDestroy(RTSEMEVENT EventSem)
{
    l4sync_t *pSync = (l4sync_t *) EventSem;

    if (l4sync_destroy(pSync) != 0)
    {
        AssertMsgFailed(("Failed to destroy event semaphore!"));
        return VERR_SEM_ERROR;
    }
    RTMemFree(pSync);
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventSignal(RTSEMEVENT EventSem)
{
    l4sync_t *pSync = (l4sync_t *) EventSem;

    l4sync_signal(pSync);
    return VINF_SUCCESS;
}

RTDECL(int)  RTSemEventWaitNoResume(RTSEMEVENT EventSem, unsigned cMillies)
{
    l4sync_t *pSync = (l4sync_t *) EventSem;
    int rc;

    if (cMillies == 0)
        rc = l4sync_try(pSync);  /* 0 is an indefinite wait! */
    else
        rc = l4sync_wait(pSync, cMillies == RT_INDEFINITE_WAIT ? 0 : cMillies);
    switch (rc)
    {
        case L4SYNC_WAIT_SUCCESS:
            return VINF_SUCCESS;
        case L4SYNC_WAIT_TIMEOUT:
            return VERR_TIMEOUT;
        default:
           AssertMsgFailed(("unknown %s:%d error=%d", __FILE__, __LINE__, rc));
           return VERR_INTERNAL_ERROR;
    }
}


RTDECL(int)   RTSemEventMultiCreate(PRTSEMEVENTMULTI pEventSem)
{
    if (l4sync_init() != 0)
    {
        AssertMsgFailed(("Failed to set up the l4env SemEvent library!"));
        return VERR_SEM_ERROR;
    }
    l4sync_t *pSync = (l4sync_t*) RTMemAlloc(sizeof(l4sync_t));
    if (!pSync)
        return VERR_NO_MEMORY;

    *pSync     =  L4SYNC_INIT(L4SYNC_NOT_SIGNALLED);
    *pEventSem =  (RTSEMEVENTMULTIINTERNAL*) pSync;
    return VINF_SUCCESS;
}


RTDECL(int)  RTSemEventMultiDestroy(RTSEMEVENTMULTI EventMultiSem)
{
    /* the code and the structure is identical with other type for this function. */
    return RTSemEventDestroy((RTSEMEVENT)EventMultiSem);
}

RTDECL(int)  RTSemEventMultiSignal(RTSEMEVENTMULTI EventMultiSem)
{
    /* the code and the structure is identical with other type for this function. */
    return RTSemEventSignal((RTSEMEVENT)EventMultiSem);
}

RTDECL(int)  RTSemEventMultiReset(RTSEMEVENTMULTI EventMultiSem)
{
    l4sync_t *pSync = (l4sync_t *) EventMultiSem;

    l4sync_multi_reset(pSync);
    return VINF_SUCCESS;
}

RTDECL(int)  RTSemEventMultiWaitNoResume(RTSEMEVENTMULTI EventMultiSem, unsigned cMillies)
{
    /*
     * Wait for condition.
     */
    int rc;
    l4sync_t *pSync = (l4sync_t *) EventMultiSem;

    if (cMillies == 0)  /* 0 is an indefinite wait! */
        rc = l4sync_multi_try(pSync);
    else
        rc = l4sync_multi_wait(pSync, cMillies == RT_INDEFINITE_WAIT ? 0 : cMillies);
    switch (rc)
    {
    case L4SYNC_WAIT_SUCCESS:
        return VINF_SUCCESS;
    case L4SYNC_WAIT_TIMEOUT:
        return VERR_TIMEOUT;
    default:
        AssertMsgFailed(("unknown %s:%d error=%d", __FILE__, __LINE__, rc));
        return VERR_INTERNAL_ERROR;
    }
}

struct RTSEMMUTEXINTERNAL
{
    /** Magic number for verification purposes */
    uint32_t magic;
    /** An l4env lock which is the actual mutex */
    l4semaphore_t lock;
    /** Current lock owner */
    volatile l4thread_t owner;
    /** Nesting count */
    volatile uint32_t cNesting;
};

#define RTSEMMUTEXMAGIC 4242

RTDECL(int)  RTSemMutexCreate(PRTSEMMUTEX pMutexSem)
{
    static const l4semaphore_t unlocked_lock = L4SEMAPHORE_UNLOCKED;  /* Rather ugly, but there is no initialisation function in the API */
    *pMutexSem = (struct RTSEMMUTEXINTERNAL *) RTMemAlloc(sizeof(struct RTSEMMUTEXINTERNAL));
    if (!*pMutexSem)
        return VERR_NO_MEMORY;
    (*pMutexSem)->magic = RTSEMMUTEXMAGIC;
    (*pMutexSem)->owner = L4THREAD_INVALID_ID;
    (*pMutexSem)->cNesting = 0;
    memcpy(&(*pMutexSem)->lock, &unlocked_lock, sizeof(unlocked_lock));
    return VINF_SUCCESS;
}

RTDECL(int)  RTSemMutexDestroy(RTSEMMUTEX MutexSem)
{
    if (MutexSem->magic != RTSEMMUTEXMAGIC)
    {
        AssertMsgFailed(("RTSemMutexDestroy: bad magic in MutexSem\n"));
        return VERR_INVALID_MAGIC;
    }
    if (MutexSem->cNesting > 0 || RTSemMutexRequest(MutexSem, 0) != VINF_SUCCESS)
    {
        AssertMsgFailed(("RTSemMutexDestroy: attempt to destroy locked MutexSem\n"));
        return VERR_RESOURCE_IN_USE;
    }
    RTMemFree(MutexSem);
    return VINF_SUCCESS;
}

RTDECL(int)  RTSemMutexRequestNoResume(RTSEMMUTEX MutexSem, unsigned cMillies)
{
    if (MutexSem->magic != RTSEMMUTEXMAGIC)
    {
        AssertMsgFailed(("RTSemMutexRequest: bad magic in MutexSem\n"));
        return VERR_INVALID_MAGIC;
    }
    l4thread_t me = l4thread_myself();
    /* Do we already own this lock? */
    if (MutexSem->cNesting > 0 && l4thread_equal(me, MutexSem->owner))
    {
        MutexSem->cNesting++;
        return VINF_SUCCESS;
    }
    /* get semaphore */
    if (cMillies == RT_INDEFINITE_WAIT)
        l4semaphore_down(&MutexSem->lock);
    else
    {
        if (cMillies == 0 && l4semaphore_try_down(&MutexSem->lock) == 0)
            return VERR_TIMEOUT;
        else if (l4semaphore_down_timed(&MutexSem->lock, cMillies) != 0)
            return VERR_TIMEOUT;
    }
    /* set owner / reference counter */
    MutexSem->owner = me;
    MutexSem->cNesting = 1;
    return VINF_SUCCESS;
}

RTDECL(int)  RTSemMutexRelease(RTSEMMUTEX MutexSem)
{
    if (MutexSem->magic != RTSEMMUTEXMAGIC)
    {
        AssertMsgFailed(("RTSemMutexRelease: bad magic in MutexSem\n"));
        return VERR_INVALID_MAGIC;
    }
    l4thread_t me = l4thread_myself();
    if (MutexSem->cNesting == 0 || !l4thread_equal(MutexSem->owner, me))
    {
        AssertMsgFailed(("RTSemMutexRelease: tried to release a MutexSem we do not own.\n"));
        return VERR_NOT_OWNER;
    }
    MutexSem->cNesting--;
    if (MutexSem->cNesting <= 0)
        l4semaphore_up(&MutexSem->lock);
    return VINF_SUCCESS;
}
