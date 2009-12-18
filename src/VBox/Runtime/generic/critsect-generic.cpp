/* $Id$ */
/** @file
 * IPRT - Critical Section, Generic.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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
#include <iprt/critsect.h>
#include "internal/iprt.h"

#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include "internal/thread.h"
#include "internal/strict.h"


/* In strict mode we're redefining these, so undefine them now for the implementation. */
#undef RTCritSectEnter
#undef RTCritSectTryEnter
#undef RTCritSectEnterMultiple



RTDECL(int) RTCritSectInit(PRTCRITSECT pCritSect)
{
    return RTCritSectInitEx(pCritSect, 0);
}
RT_EXPORT_SYMBOL(RTCritSectInit);


RTDECL(int) RTCritSectInitEx(PRTCRITSECT pCritSect, uint32_t fFlags)
{
    /*
     * Initialize the structure and
     */
    pCritSect->u32Magic             = RTCRITSECT_MAGIC;
    pCritSect->fFlags               = fFlags;
    pCritSect->cNestings            = 0;
    pCritSect->cLockers             = -1;
    pCritSect->NativeThreadOwner    = NIL_RTNATIVETHREAD;
    int rc = RTLockValidatorRecCreate(&pCritSect->pValidatorRec, NIL_RTLOCKVALIDATORCLASS, 0, NULL, pCritSect);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pCritSect->EventSem);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        RTLockValidatorRecDestroy(&pCritSect->pValidatorRec);
    }

    AssertRC(rc);
    pCritSect->EventSem = NULL;
    pCritSect->u32Magic = (uint32_t)rc;
    return rc;
}
RT_EXPORT_SYMBOL(RTCritSectInitEx);


DECL_FORCE_INLINE(int) rtCritSectTryEnter(PRTCRITSECT pCritSect, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    RTNATIVETHREAD  NativeThreadSelf = RTThreadNativeSelf();
#ifdef RTCRITSECT_STRICT
    RTTHREAD        ThreadSelf = RTThreadSelf();
    if (ThreadSelf == NIL_RTTHREAD)
        RTThreadAdopt(RTTHREADTYPE_DEFAULT, 0, NULL, &ThreadSelf);
#endif

    /*
     * Try take the lock. (cLockers is -1 if it's free)
     */
    if (!ASMAtomicCmpXchgS32(&pCritSect->cLockers, 0, -1))
    {
        /*
         * Somebody is owning it (or will be soon). Perhaps it's us?
         */
        if (pCritSect->NativeThreadOwner == NativeThreadSelf)
        {
            if (!(pCritSect->fFlags & RTCRITSECT_FLAGS_NO_NESTING))
            {
                ASMAtomicIncS32(&pCritSect->cLockers);
                pCritSect->cNestings++;
                return VINF_SUCCESS;
            }
            AssertMsgFailed(("Nested entry of critsect %p\n", pCritSect));
            return VERR_SEM_NESTED;
        }
        return VERR_SEM_BUSY;
    }

    /*
     * First time
     */
    pCritSect->cNestings = 1;
    ASMAtomicWriteHandle(&pCritSect->NativeThreadOwner, NativeThreadSelf);
#ifdef RTCRITSECT_STRICT
    RTLockValidatorSetOwner(pCritSect->pValidatorRec, ThreadSelf, pSrcPos);
#endif

    return VINF_SUCCESS;
}


RTDECL(int) RTCritSectTryEnter(PRTCRITSECT pCritSect)
{
#ifndef RTCRTISECT_STRICT
    return rtCritSectTryEnter(pCritSect, NULL);
#else
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_NORMAL_API();
    return rtCritSectTryEnter(pCritSect, &SrcPos);
#endif
}
RT_EXPORT_SYMBOL(RTCritSectTryEnter);


RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_DEBUG_API();
    return rtCritSectTryEnter(pCritSect, &SrcPos);
}
RT_EXPORT_SYMBOL(RTCritSectTryEnterDebug);


DECL_FORCE_INLINE(int) rtCritSectEnter(PRTCRITSECT pCritSect, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    RTNATIVETHREAD  NativeThreadSelf = RTThreadNativeSelf();
#ifdef RTCRITSECT_STRICT
    RTTHREAD        hThreadSelf = RTThreadSelfAutoAdopt();
    RTLockValidatorCheckOrder(pCritSect->pValidatorRec, hThreadSelf, pSrcPos);
#endif

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

            AssertBreakpoint(); /* don't do normal assertion here, the logger uses this code too. */
            ASMAtomicDecS32(&pCritSect->cLockers);
            return VERR_SEM_NESTED;
        }

        /*
         * Wait for the current owner to release it.
         */
#ifndef RTCRITSECT_STRICT
        RTTHREAD hThreadSelf = RTThreadSelf();
#endif
        for (;;)
        {
#ifdef RTCRITSECT_STRICT
            int rc9 = RTLockValidatorCheckBlocking(pCritSect->pValidatorRec, hThreadSelf, RTTHREADSTATE_CRITSECT,
                                                   !(pCritSect->fFlags & RTCRITSECT_FLAGS_NO_NESTING),
                                                   pSrcPos);
            if (RT_FAILURE(rc9))
            {
                ASMAtomicDecS32(&pCritSect->cLockers);
                return rc9;
            }
#endif

            RTThreadBlocking(hThreadSelf, RTTHREADSTATE_CRITSECT);
            int rc = RTSemEventWait(pCritSect->EventSem, RT_INDEFINITE_WAIT);
            RTThreadUnblocked(hThreadSelf, RTTHREADSTATE_CRITSECT);

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
#ifdef RTCRITSECT_STRICT
    RTLockValidatorSetOwner(pCritSect->pValidatorRec, hThreadSelf, pSrcPos);
#endif

    return VINF_SUCCESS;
}


RTDECL(int) RTCritSectEnter(PRTCRITSECT pCritSect)
{
#ifndef RTCRITSECT_STRICT
    return rtCritSectEnter(pCritSect, NULL);
#else
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_NORMAL_API();
    return rtCritSectEnter(pCritSect, &SrcPos);
#endif
}
RT_EXPORT_SYMBOL(RTCritSectEnter);


RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_DEBUG_API();
    return rtCritSectEnter(pCritSect, &SrcPos);
}
RT_EXPORT_SYMBOL(RTCritSectEnterDebug);


RTDECL(int) RTCritSectLeave(PRTCRITSECT pCritSect)
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
#ifdef RTCRITSECT_STRICT
        RTLockValidatorUnsetOwner(pCritSect->pValidatorRec);
#endif
        ASMAtomicWriteHandle(&pCritSect->NativeThreadOwner, NIL_RTNATIVETHREAD);
        if (ASMAtomicDecS32(&pCritSect->cLockers) >= 0)
        {
            int rc = RTSemEventSignal(pCritSect->EventSem);
            AssertReleaseMsg(RT_SUCCESS(rc), ("RTSemEventSignal -> %Rrc\n", rc));
        }
    }
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTCritSectLeave);





static int rtCritSectEnterMultiple(size_t cCritSects, PRTCRITSECT *papCritSects, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    Assert(cCritSects > 0);
    AssertPtr(papCritSects);

    /*
     * Try get them all.
     */
    int rc = VERR_INVALID_PARAMETER;
    size_t i;
    for (i = 0; i < cCritSects; i++)
    {
        rc = rtCritSectTryEnter(papCritSects[i], pSrcPos);
        if (RT_FAILURE(rc))
            break;
    }
    if (RT_SUCCESS(rc))
        return rc;

    /*
     * The retry loop.
     */
    for (unsigned cTries = 0; ; cTries++)
    {
        /*
         * We've failed, release any locks we might have gotten. ('i' is the lock that failed btw.)
         */
        size_t j = i;
        while (j-- > 0)
        {
            int rc2 = RTCritSectLeave(papCritSects[j]);
            AssertRC(rc2);
        }
        if (rc != VERR_SEM_BUSY)
            return rc;

        /*
         * Try prevent any theoretical synchronous races with other threads.
         */
        Assert(cTries < 1000000);
        if (cTries > 10000)
            RTThreadSleep(cTries % 3);

        /*
         * Wait on the one we failed to get.
         */
        rc = rtCritSectEnter(papCritSects[i], pSrcPos);
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Try take the others.
         */
        for (j = 0; j < cCritSects; j++)
        {
            if (j != i)
            {
                rc = rtCritSectTryEnter(papCritSects[j], pSrcPos);
                if (RT_FAILURE(rc))
                    break;
            }
        }
        if (RT_SUCCESS(rc))
            return rc;

        /*
         * We failed.
         */
        if (i > j)
        {
            int rc2 = RTCritSectLeave(papCritSects[i]);
            AssertRC(rc2);
        }
        i = j;
    }
}


RTDECL(int) RTCritSectEnterMultiple(size_t cCritSects, PRTCRITSECT *papCritSects)
{
#ifndef RTCRITSECT_STRICT
    return rtCritSectEnterMultiple(cCritSects, papCritSects, NULL);
#else
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_NORMAL_API();
    return rtCritSectEnterMultiple(cCritSects, papCritSects, &SrcPos);
#endif
}
RT_EXPORT_SYMBOL(RTCritSectEnterMultiple);


RTDECL(int) RTCritSectEnterMultipleDebug(size_t cCritSects, PRTCRITSECT *papCritSects, RTUINTPTR uId, RT_SRC_POS_DECL)
{
    RTLOCKVALIDATORSRCPOS SrcPos = RTLOCKVALIDATORSRCPOS_INIT_DEBUG_API();
    return rtCritSectEnterMultiple(cCritSects, papCritSects, &SrcPos);
}
RT_EXPORT_SYMBOL(RTCritSectEnterMultipleDebug);



RTDECL(int) RTCritSectLeaveMultiple(size_t cCritSects, PRTCRITSECT *papCritSects)
{
    int rc = VINF_SUCCESS;
    for (size_t i = 0; i < cCritSects; i++)
    {
        int rc2 = RTCritSectLeave(papCritSects[i]);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}
RT_EXPORT_SYMBOL(RTCritSectLeaveMultiple);


RTDECL(int) RTCritSectDelete(PRTCRITSECT pCritSect)
{
    /*
     * Assert free waiters and so on.
     */
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    Assert(pCritSect->cNestings == 0);
    Assert(pCritSect->cLockers == -1);
    Assert(pCritSect->NativeThreadOwner == NIL_RTNATIVETHREAD);

    /*
     * Invalidate the structure and free the mutex.
     * In case someone is waiting we'll signal the semaphore cLockers + 1 times.
     */
    ASMAtomicWriteU32(&pCritSect->u32Magic, ~RTCRITSECT_MAGIC);
    pCritSect->fFlags           = 0;
    pCritSect->cNestings        = 0;
    pCritSect->NativeThreadOwner= NIL_RTNATIVETHREAD;
    RTSEMEVENT EventSem = pCritSect->EventSem;
    pCritSect->EventSem         = NIL_RTSEMEVENT;

    while (pCritSect->cLockers-- >= 0)
        RTSemEventSignal(EventSem);
    ASMAtomicWriteS32(&pCritSect->cLockers, -1);
    int rc = RTSemEventDestroy(EventSem);
    AssertRC(rc);

    RTLockValidatorRecDestroy(&pCritSect->pValidatorRec);

    return rc;
}
RT_EXPORT_SYMBOL(RTCritSectDelete);

