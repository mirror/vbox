/* $Id$ */
/** @file
 * IPRT - Critical Section, Generic.
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
#include <iprt/critsect.h>
#include <iprt/semaphore.h>
#include <iprt/thread.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/err.h>
#include "internal/thread.h"
#include "internal/strict.h"


/* in strict mode we're redefining this, so undefine it now for the implementation. */
#undef RTCritSectEnter
#undef RTCritSectTryEnter
#undef RTCritSectEnterMultiple


/**
 * Initialize a critical section.
 */
RTDECL(int) RTCritSectInit(PRTCRITSECT pCritSect)
{
    return RTCritSectInitEx(pCritSect, 0);
}


/**
 * Initialize a critical section.
 *
 * @returns iprt status code.
 * @param   pCritSect   Pointer to the critical section structure.
 * @param   fFlags      Flags, any combination of the RTCRITSECT_FLAGS \#defines.
 */
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
    pCritSect->Strict.ThreadOwner   = NIL_RTTHREAD;
    pCritSect->Strict.pszEnterFile  = NULL;
    pCritSect->Strict.u32EnterLine  = 0;
    pCritSect->Strict.uEnterId      = 0;
    int rc = RTSemEventCreate(&pCritSect->EventSem);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    AssertRC(rc);
    pCritSect->EventSem = NULL;
    pCritSect->u32Magic = (uint32_t)rc;
    return rc;
}


/**
 * Enter multiple critical sections.
 *
 * This function will enter ALL the specified critical sections before returning.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   cCritSects      Number of critical sections in the array.
 * @param   papCritSects    Array of critical section pointers.
 *
 * @remark  Please note that this function will not necessarily come out favourable in a
 *          fight with other threads which are using the normal RTCritSectEnter() function.
 *          Therefore, avoid having to enter multiple critical sections!
 */
RTDECL(int) RTCritSectEnterMultiple(unsigned cCritSects, PRTCRITSECT *papCritSects)
#ifdef RTCRITSECT_STRICT
{
    return RTCritSectEnterMultipleDebug(cCritSects, papCritSects, __FILE__, __LINE__, 0);
}
RTDECL(int) RTCritSectEnterMultipleDebug(unsigned cCritSects, PRTCRITSECT *papCritSects, const char *pszFile, unsigned uLine, RTUINTPTR uId)
#endif /* RTCRITSECT_STRICT */
{
    Assert(cCritSects > 0);
    Assert(VALID_PTR(papCritSects));

    /*
     * Try get them all.
     */
    int rc = VERR_INVALID_PARAMETER;
    unsigned i;
    for (i = 0; i < cCritSects; i++)
    {
#ifdef RTCRITSECT_STRICT
        rc = RTCritSectTryEnterDebug(papCritSects[i], pszFile, uLine, uId);
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
        unsigned j = i;
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
        rc = RTCritSectEnterDebug(papCritSects[i], pszFile, uLine, uId);
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
                rc = RTCritSectTryEnterDebug(papCritSects[j], pszFile, uLine, uId);
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


/**
 * Try enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectTryEnter(PRTCRITSECT pCritSect)
#ifdef RTCRITSECT_STRICT
{
    return RTCritSectTryEnterDebug(pCritSect, __FILE__, __LINE__, 0);
}
RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, const char *pszFile, unsigned uLine, RTUINTPTR uId)
#endif /* RTCRITSECT_STRICT */
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
    ASMAtomicXchgSize(&pCritSect->NativeThreadOwner, NativeThreadSelf);
#ifdef RTCRITSECT_STRICT
    pCritSect->Strict.pszEnterFile = pszFile;
    pCritSect->Strict.u32EnterLine = uLine;
    pCritSect->Strict.uEnterId     = uId;
    ASMAtomicXchgSize(&pCritSect->Strict.ThreadOwner, (RTUINTPTR)ThreadSelf); /* screw gcc and its pedantic warnings. */
#endif

    return VINF_SUCCESS;
}


/**
 * Enter a critical section.
 *
 * @returns VINF_SUCCESS on success.
 * @returns VERR_SEM_NESTED if nested enter on a no nesting section. (Asserted.)
 * @returns VERR_SEM_DESTROYED if RTCritSectDelete was called while waiting.
 * @param   pCritSect   The critical section.
 */
RTDECL(int) RTCritSectEnter(PRTCRITSECT pCritSect)
#ifdef RTCRITSECT_STRICT
{
    return RTCritSectEnterDebug(pCritSect, __FILE__, __LINE__, 0);
}
RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, const char *pszFile, unsigned uLine, RTUINTPTR uId)
#endif /* RTCRITSECT_STRICT */
{
    Assert(pCritSect);
    Assert(pCritSect->u32Magic == RTCRITSECT_MAGIC);
    RTNATIVETHREAD  NativeThreadSelf = RTThreadNativeSelf();
#ifdef RTCRITSECT_STRICT
    RTTHREAD        ThreadSelf = RTThreadSelf();
    if (ThreadSelf == NIL_RTTHREAD)
        RTThreadAdopt(RTTHREADTYPE_DEFAULT, 0, NULL, &ThreadSelf);
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
            else
            {
                AssertMsgFailed(("Nested entry of critsect %p\n", pCritSect));
                ASMAtomicDecS32(&pCritSect->cLockers);
                return VERR_SEM_NESTED;
            }
        }

        for (;;)
        {
#ifdef RTCRITSECT_STRICT
            rtThreadBlocking(ThreadSelf, RTTHREADSTATE_CRITSECT, (uintptr_t)pCritSect, pszFile, uLine, uId);
#endif
            int rc = RTSemEventWait(pCritSect->EventSem, RT_INDEFINITE_WAIT);
#ifdef RTCRITSECT_STRICT
            rtThreadUnblocked(ThreadSelf, RTTHREADSTATE_CRITSECT);
#endif
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
    ASMAtomicXchgSize(&pCritSect->NativeThreadOwner, NativeThreadSelf);
#ifdef RTCRITSECT_STRICT
    pCritSect->Strict.pszEnterFile = pszFile;
    pCritSect->Strict.u32EnterLine = uLine;
    pCritSect->Strict.uEnterId     = uId;
    ASMAtomicXchgSize(&pCritSect->Strict.ThreadOwner, (RTUINTPTR)ThreadSelf); /* screw gcc and its pedantic warnings. */
    RTThreadWriteLockInc(ThreadSelf);
#endif

    return VINF_SUCCESS;
}


/**
 * Leave a critical section.
 *
 * @returns VINF_SUCCESS.
 * @param   pCritSect   The critical section.
 */
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
        if (pCritSect->Strict.ThreadOwner != NIL_RTTHREAD) /* May happen for PDMCritSects when entering GC/R0. */
            RTThreadWriteLockDec(pCritSect->Strict.ThreadOwner);
        ASMAtomicXchgSize(&pCritSect->Strict.ThreadOwner, NIL_RTTHREAD);
#endif
        ASMAtomicXchgSize(&pCritSect->NativeThreadOwner, NIL_RTNATIVETHREAD);
        if (ASMAtomicDecS32(&pCritSect->cLockers) >= 0)
        {
            int rc = RTSemEventSignal(pCritSect->EventSem);
            AssertReleaseMsg(RT_SUCCESS(rc), ("RTSemEventSignal -> %Rrc\n", rc));
        }
    }
    return VINF_SUCCESS;
}


/**
 * Leave multiple critical sections.
 *
 * @returns VINF_SUCCESS.
 * @param   cCritSects      Number of critical sections in the array.
 * @param   papCritSects    Array of critical section pointers.
 */
RTDECL(int) RTCritSectLeaveMultiple(unsigned cCritSects, PRTCRITSECT *papCritSects)
{
    int rc = VINF_SUCCESS;
    for (unsigned i = 0; i < cCritSects; i++)
    {
        int rc2 = RTCritSectLeave(papCritSects[i]);
        if (RT_FAILURE(rc2) && RT_SUCCESS(rc))
            rc = rc2;
    }
    return rc;
}


#ifndef RTCRITSECT_STRICT
RTDECL(int) RTCritSectEnterDebug(PRTCRITSECT pCritSect, const char *pszFile, unsigned uLine, RTUINTPTR uId)
{
    return RTCritSectEnter(pCritSect);
}

RTDECL(int) RTCritSectTryEnterDebug(PRTCRITSECT pCritSect, const char *pszFile, unsigned uLine, RTUINTPTR uId)
{
    return RTCritSectTryEnter(pCritSect);
}

RTDECL(int) RTCritSectEnterMultipleDebug(unsigned cCritSects, PRTCRITSECT *papCritSects, const char *pszFile, unsigned uLine, RTUINTPTR uId)
{
    return RTCritSectEnterMultiple(cCritSects, papCritSects);
}
#endif /* RT_STRICT */


/**
 * Deletes a critical section.
 *
 * @returns VINF_SUCCESS.
 * @param   pCritSect   The critical section.
 */
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
    pCritSect->EventSem         = NULL;
    while (pCritSect->cLockers-- >= 0)
        RTSemEventSignal(EventSem);
    ASMAtomicWriteS32(&pCritSect->cLockers, -1);
    int rc = RTSemEventDestroy(EventSem);
    AssertRC(rc);

    return rc;
}

