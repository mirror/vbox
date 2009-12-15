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
    int rc = RTLockValidatorCreate(&pCritSect->pValidatorRec, NIL_RTLOCKVALIDATORCLASS, 0, NULL, pCritSect);
    if (RT_SUCCESS(rc))
    {
        rc = RTSemEventCreate(&pCritSect->EventSem);
        if (RT_SUCCESS(rc))
            return VINF_SUCCESS;
        RTLockValidatorDestroy(&pCritSect->pValidatorRec);
    }

    AssertRC(rc);
    pCritSect->EventSem = NULL;
    pCritSect->u32Magic = (uint32_t)rc;
    return rc;
}
RT_EXPORT_SYMBOL(RTCritSectInitEx);


#ifdef RTCRITSECT_STRICT
RTDECL(int) RTCritSectEnterMultipleDebug(size_t cCritSects, PRTCRITSECT *papCritSects, RTUINTPTR uId, RT_SRC_POS_DECL)
#else
RTDECL(int) RTCritSectEnterMultiple(size_t cCritSects, PRTCRITSECT *papCritSects)
#endif
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
#ifdef RTCRITSECT_STRICT
        rc = RTCritSectTryEnterDebug(papCritSects[i], uId, RT_SRC_POS_ARGS);
#else
        rc = RTCritSectTryEnter(papCritSects[i]);
#endif
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
#ifdef RTCRITSECT_STRICT
        rc = RTCritSectEnterDebug(papCritSects[i], uId, RT_SRC_POS_ARGS);
#else
        rc = RTCritSectEnter(papCritSects[i]);
#endif
        if (RT_FAILURE(rc))
            return rc;

        /*
         * Try take the others.
         */
        for (j = 0; j < cCritSects; j++)
        {
            if (j != i)
            {
#ifdef RTCRITSECT_STRICT
                rc = RTCritSectTryEnterDebug(papCritSects[j], uId, RT_SRC_POS_ARGS);
#else
                rc = RTCritSectTryEnter(papCritSects[j]);
#endif
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
#ifdef RTCRITSECT_STRICT
RT_EXPORT_SYMBOL(RTCritSectEnterMultipleDebug);
#else
RT_EXPORT_SYMBOL(RTCritSectEnterMultiple);
#endif


#ifdef RTCRITSECT_STRICT
RTDECL(int) RTCritSectEnterMultiple(size_t cCritSects, PRTCRITSECT *papCritSects)
{
    return RTCritSectEnterMultipleDebug(cCritSects, papCritSects, 0, RT_SRC_POS);
}
RT_EXPORT_SYMBOL(RTCritSectEnterMultiple);


#else  /* !RTCRITSECT_STRICT */
RTDECL(int) RTCritSectEnterMultipleDebug(size_t cCritSects, PRTCRITSECT *papCritSects, RTUINTPTR uId, RT_SRC_POS_DECL)
{
    return RTCritSectEnterMultiple(cCritSects, papCritSects);
}
RT_EXPORT_SYMBOL(RTCritSectEnterMultipleDebug);
#endif /* !RTCRITSECT_STRICT */


#ifdef RTCRITSECT_STRICT
RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
#else
RTDECL(int) RTCritSectTryEnter(PRTCRITSECT pCritSect)
#endif
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
    RTThreadWriteLockInc(RTLockValidatorSetOwner(pCritSect->pValidatorRec, ThreadSelf, uId, RT_SRC_POS_ARGS));
#endif

    return VINF_SUCCESS;
}
#ifdef RTCRITSECT_STRICT
RT_EXPORT_SYMBOL(RTCritSectTryEnterDebug);
#else
RT_EXPORT_SYMBOL(RTCritSectTryEnter);
#endif


#ifdef RTCRITSECT_STRICT
RTDECL(int) RTCritSectTryEnter(PRTCRITSECT pCritSect)
{
    return RTCritSectTryEnterDebug(pCritSect, 0, RT_SRC_POS);
}
RT_EXPORT_SYMBOL(RTCritSectTryEnter);


#else  /* !RTCRITSECT_STRICT */
RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    return RTCritSectTryEnter(pCritSect);
}
RT_EXPORT_SYMBOL(RTCritSectTryEnterDebug);
#endif /* !RTCRITSECT_STRICT */


#ifdef RTCRITSECT_STRICT
RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
#else
RTDECL(int) RTCritSectEnter(PRTCRITSECT pCritSect)
#endif
{
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    RTNATIVETHREAD  NativeThreadSelf = RTThreadNativeSelf();
#ifdef RTCRITSECT_STRICT
    RTTHREAD        hThreadSelf = RTThreadSelfAutoAdopt();
    RTLockValidatorCheckOrder(pCritSect->pValidatorRec, hThreadSelf, uId, RT_SRC_POS_ARGS);
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
            RTCRITSECT_STRICT_BLOCK(hThreadSelf, pCritSect->pValidatorRec, !(pCritSect->fFlags & RTCRITSECT_FLAGS_NO_NESTING));
            int rc = RTSemEventWait(pCritSect->EventSem, RT_INDEFINITE_WAIT);
            RTCRITSECT_STRICT_UNBLOCK(hThreadSelf);
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
    RTThreadWriteLockInc(RTLockValidatorSetOwner(pCritSect->pValidatorRec, hThreadSelf, uId, RT_SRC_POS_ARGS));
#endif

    return VINF_SUCCESS;
}
#ifdef RTCRITSECT_STRICT
RT_EXPORT_SYMBOL(RTCritSectEnterDebug);
#else
RT_EXPORT_SYMBOL(RTCritSectEnter);
#endif


#ifdef RTCRITSECT_STRICT
RTDECL(int) RTCritSectEnter(PRTCRITSECT pCritSect)
{
    return RTCritSectEnterDebug(pCritSect, 0, RT_SRC_POS);
}
RT_EXPORT_SYMBOL(RTCritSectEnter);


#else  /* !RTCRITSECT_STRICT */
RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    return RTCritSectEnter(pCritSect);
}
RT_EXPORT_SYMBOL(RTCritSectEnterDebug);
#endif /* !RTCRITSECT_STRICT */


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
        RTThreadWriteLockInc(RTLockValidatorUnsetOwner(pCritSect->pValidatorRec));
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

    RTLockValidatorDestroy(&pCritSect->pValidatorRec);

    return rc;
}
RT_EXPORT_SYMBOL(RTCritSectDelete);

