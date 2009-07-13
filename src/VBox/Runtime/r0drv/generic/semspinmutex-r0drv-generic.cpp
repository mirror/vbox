/* $Id$ */
/** @file
 * IPRT - Spinning Mutex Semaphores, Ring-0 Driver, Generic.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
#ifdef RT_OS_WINDOWS
# include "../nt/the-nt-kernel.h"
#endif
#include "internal/iprt.h"

#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include "internal/magic.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
typedef struct RTSEMSPINMUTEXINTERNAL
{
    /** Magic value (RTSEMSPINMUTEX_MAGIC)
     * RTCRITSECT_MAGIC is the value of an initialized & operational section. */
    uint32_t volatile       u32Magic;
    /** Number of lockers.
     * -1 if the section is free. */
    int32_t volatile        cLockers;
    /** The owner thread. */
    RTNATIVETHREAD volatile hOwner;
    /** Flags. This is a combination of RTSEMSPINMUTEX_FLAGS_XXX and
     *  RTSEMSPINMUTEX_INT_FLAGS_XXX. */
    uint32_t volatile       fFlags;
    /** The semaphore to block on. */
    RTSEMEVENT              hEventSem;
    /** Saved flags register of the owner. (AMD64 & x86) */
    RTCCUINTREG             fSavedFlags;
    /** The preemption disable state of the owner. */
    RTTHREADPREEMPTSTATE    PreemptState;
} RTSEMSPINMUTEXINTERNAL;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
//#define RTSEMSPINMUTEX_INT_FLAGS_MUST

/** Validates the handle, returning if invalid. */
#define RTSEMSPINMUTEX_VALIDATE_RETURN(pThis) \
    do \
    { \
        AssertPtr(pThis); \
        uint32_t u32Magic = (pThis)->u32Magic; \
        if (u32Magic != RTSEMSPINMUTEX_MAGIC) \
        { \
            AssertMsgFailed(("u32Magic=%#x pThis=%p\n", u32Magic, pThis)); \
            return u32Magic == RTSEMSPINMUTEX_MAGIC_DEAD ? VERR_SEM_DESTROYED : VERR_INVALID_HANDLE; \
        } \
    } while (0)


RTDECL(int) RTSemSpinMutexCreate(PRTSEMSPINMUTEX phSpinMtx, uint32_t fFlags)
{
    AssertReturn(!(fFlags & ~(RTSEMSPINMUTEX_FLAGS_VALID_MASK)), VERR_INVALID_PARAMETER);
    AssertPtr(phSpinMtx);

    /*
     * Allocate and initialize the structure.
     */
    RTSEMSPINMUTEXINTERNAL *pThis = (RTSEMSPINMUTEXINTERNAL *)RTMemAllocZ(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->u32Magic             = RTSEMSPINMUTEX_MAGIC;
    pThis->fFlags               = fFlags;
    pThis->cLockers             = -1;
    pThis->NativeThreadOwner    = NIL_RTNATIVETHREAD;
    int rc = RTSemEventCreate(&pThis->hEventSem);
    if (RT_SUCCESS(rc))
    {
        *phSpinMtx = pThis;
        return VINF_SUCCESS;
    }

    RTMemFree(pThis);
    return rc;
}
RT_EXPORT_SYMBOL(RTSemSpinMutexCreate);



RTDECL(int) RTSemSpinMutexTryRequest(RTSEMSPINMUTEX hSpinMtx)
{
    RTSEMSPINMUTEXINTERNAL *pThis       = hSpinMtx;
    RTNATIVETHREAD          hSelf       = RTThreadNativeSelf();
    RTCCUINTREG             fSavedFlags = 0;
    RTTHREADPREEMPTSTATE    PreemptState;
    uint32_t                fFlags;
    RTSEMSPINMUTEX_VALIDATE_RETURN(pThis);

    /*
     * Check context, disable preemption and save flags if necessary.
     */
#ifdef RT_OS_WINDOWS
    /* NT: IRQL <= DISPATCH_LEVEL for waking up threads. */
    PreemptState.uchOldIrql = KeGetCurrentIrql();
    if (PreemptState.uchOldIrql > DISPATCH_LEVEL)
        return VERR_SEM_BAD_CONTEXT; /** @todo Can be optimized by switching the thing into spinlock mode. But later. */
    if (PreemptState.uchOldIrql < DISPATCH_LEVEL)
        KeRaiseIrql(DISPATCH_LEVEL, &PreemptState.uchOldIrql);
#else /* PORTME: Check for context where we cannot wake up threads. */
    RTThreadPreemptDisable(&PreemptState);
#endif
    fFlags = pThis->fFlags
    if (fFlags & RTSEMSPINMUTEX_FLAGS_IRQ_SAFE)
        fSavedFlags = ASMIntDisableFlags();

    /*
     * Try take the lock. (cLockers is -1 if it's free)
     */
    if (!ASMAtomicCmpXchgS32(&pThis->cLockers, 0, -1))
    {
        /* busy, too bad. */
        int rc = VERR_SEM_BUSY;
        if (RT_UNLIKELY(pThis->hOwner != hSelf))
        {
            AssertMsgFailed(("%p attempt at nested access\n"));
            rc = VERR_SEM_NESTED
        }

        /*
         * Restore preemption and flags.
         */
        if (fFlags & RTSEMSPINMUTEX_FLAGS_IRQ_SAFE)
            ASMSetFlags(fSavedFlags);
#ifdef RT_OS_WINDOWS
        if (PreemptState.uchOldIrql < DISPATCH_LEVEL)
            KeLowerIrql(PreemptState.uchOldIrql);
#else
        RTThreadPreemptEnabled(&PreemptState);
#endif
        return rc;
    }

    /*
     * We've got the lock.
     */
    ASMAtomicWriteHandle(&pThis->hOwner, hSelf);
    pThis->fSavedFlags = fSavedFlags;
    pThis->PreemptState = PreemptState;

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemSpinMutexTryRequest);


RTDECL(int) RTSemSpinMutexRequest(PRTCRITSECT pCritSect)
{
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    RTNATIVETHREAD  NativeThreadSelf = RTThreadNativeSelf();

    /** If the critical section has already been destroyed, then inform the caller. */
    if (pCritSect->u32Magic != RTCRITSECT_MAGIC)
        return VERR_SEM_DESTROYED;

    /*
     * Increment the waiter counter.
     * This becomes 0 when the section is free.
     */
    if (ASMAtomicIncS32(&pCritSect->cLockers) > 0)
    {
        /*
         * Nested?
         */
        if (pCritSect->NativeThreadOwner == NativeThreadSelf)
        {
            if (!(pCritSect->fFlags & RTCRITSECT_FLAGS_NO_NESTING))
            {
                pCritSect->cNestings++;
                return VINF_SUCCESS;
            }
            else
            {
                AssertMsgFailed(("Nested entry of critsect %p\n", pCritSect));
                ASMAtomicDecS32(&pCritSect->cLockers);
                return VERR_SEM_NESTED;
            }
        }

        for (;;)
        {
            int rc = RTSemEventWait(pCritSect->EventSem, RT_INDEFINITE_WAIT);
            if (pCritSect->u32Magic != RTCRITSECT_MAGIC)
                return VERR_SEM_DESTROYED;
            if (rc == VINF_SUCCESS)
                break;
            AssertMsg(rc == VERR_TIMEOUT || rc == VERR_INTERRUPTED, ("rc=%Rrc\n", rc));
        }
        AssertMsg(pCritSect->NativeThreadOwner == NIL_RTNATIVETHREAD, ("pCritSect->NativeThreadOwner=%p\n", pCritSect->NativeThreadOwner));
    }

    /*
     * First time
     */
    pCritSect->cNestings = 1;
    ASMAtomicWriteHandle(&pCritSect->NativeThreadOwner, NativeThreadSelf);

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemSpinMutexRequest);


/**
 * Leave a critical section.
 *
 * @returns VINF_SUCCESS.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTSemSpinMutexRelease(PRTCRITSECT pCritSect)
{
    /*
     * Assert ownership and so on.
     */
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    Assert(pCritSect->cNestings > 0);
    Assert(pCritSect->cLockers >= 0);
    Assert(pCritSect->NativeThreadOwner == RTThreadNativeSelf());

    /*
     * Decrement nestings, if <= 0 when we'll release the critsec.
     */
    pCritSect->cNestings--;
    if (pCritSect->cNestings > 0)
        ASMAtomicDecS32(&pCritSect->cLockers);
    else
    {
        /*
         * Set owner to zero.
         * Decrement waiters, if >= 0 then we have to wake one of them up.
         */
        ASMAtomicWriteHandle(&pCritSect->NativeThreadOwner, NIL_RTNATIVETHREAD);
        if (ASMAtomicDecS32(&pCritSect->cLockers) >= 0)
        {
            int rc = RTSemEventSignal(pCritSect->EventSem);
            AssertReleaseMsg(RT_SUCCESS(rc), ("RTSemEventSignal -> %Rrc\n", rc));
        }
    }
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTSemSpinMutexRelease);

