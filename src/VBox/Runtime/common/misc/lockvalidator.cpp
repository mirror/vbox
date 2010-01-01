/* $Id$ */
/** @file
 * IPRT - Lock Validator.
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
#include <iprt/lockvalidator.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/once.h>
#include <iprt/semaphore.h>
#include <iprt/string.h>
#include <iprt/thread.h>

#include "internal/lockvalidator.h"
#include "internal/magics.h"
#include "internal/thread.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * Deadlock detection stack entry.
 */
typedef struct RTLOCKVALDDENTRY
{
    /** The current record. */
    PRTLOCKVALRECUNION      pRec;
    /** The current entry number if pRec is a shared one. */
    uint32_t                iEntry;
    /** The thread state of the thread we followed to get to pFirstSibling.
     * This is only used for validating a deadlock stack.  */
    RTTHREADSTATE           enmState;
    /** The thread we followed to get to pFirstSibling.
     * This is only used for validating a deadlock stack. */
    PRTTHREADINT            pThread;
    /** What pThread is waiting on, i.e. where we entered the circular list of
     * siblings.  This is used for validating a deadlock stack as well as
     * terminating the sibling walk. */
    PRTLOCKVALRECUNION      pFirstSibling;
} RTLOCKVALDDENTRY;


/**
 * Deadlock detection stack.
 */
typedef struct RTLOCKVALDDSTACK
{
    /** The number stack entries. */
    uint32_t                c;
    /** The stack entries. */
    RTLOCKVALDDENTRY        a[32];
} RTLOCKVALDDSTACK;
/** Pointer to a deadlock detction stack. */
typedef RTLOCKVALDDSTACK *PRTLOCKVALDDSTACK;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Serializing object destruction and deadlock detection.
 *
 * This makes sure that none of the memory examined by the deadlock detection
 * code will become invalid (reused for other purposes or made not present)
 * while the detection is in progress.
 *
 * NS: RTLOCKVALREC*, RTTHREADINT and RTLOCKVALDRECSHRD::papOwners destruction.
 * EW: Deadlock detection and some related activities.
 */
static RTSEMXROADS      g_hLockValidatorXRoads   = NIL_RTSEMXROADS;
/** Whether the lock validator is enabled or disabled.
 * Only applies to new locks.  */
static bool volatile    g_fLockValidatorEnabled  = true;
/** Set if the lock validator is quiet. */
#ifdef RT_STRICT
static bool volatile    g_fLockValidatorQuiet    = false;
#else
static bool volatile    g_fLockValidatorQuiet    = true;
#endif
/** Set if the lock validator may panic. */
#ifdef RT_STRICT
static bool volatile    g_fLockValidatorMayPanic = true;
#else
static bool volatile    g_fLockValidatorMayPanic = false;
#endif


/** Wrapper around ASMAtomicReadPtr. */
DECL_FORCE_INLINE(PRTLOCKVALRECUNION) rtLockValidatorReadRecUnionPtr(PRTLOCKVALRECUNION volatile *ppRec)
{
    return (PRTLOCKVALRECUNION)ASMAtomicReadPtr((void * volatile *)ppRec);
}


/** Wrapper around ASMAtomicWritePtr. */
DECL_FORCE_INLINE(void) rtLockValidatorWriteRecUnionPtr(PRTLOCKVALRECUNION volatile *ppRec, PRTLOCKVALRECUNION pRecNew)
{
    ASMAtomicWritePtr((void * volatile *)ppRec, pRecNew);
}


/** Wrapper around ASMAtomicReadPtr. */
DECL_FORCE_INLINE(PRTTHREADINT) rtLockValidatorReadThreadHandle(RTTHREAD volatile *phThread)
{
    return (PRTTHREADINT)ASMAtomicReadPtr((void * volatile *)phThread);
}


/** Wrapper around ASMAtomicUoReadPtr. */
DECL_FORCE_INLINE(PRTLOCKVALRECSHRDOWN) rtLockValidatorUoReadSharedOwner(PRTLOCKVALRECSHRDOWN volatile *ppOwner)
{
    return (PRTLOCKVALRECSHRDOWN)ASMAtomicUoReadPtr((void * volatile *)ppOwner);
}


/**
 * Reads a volatile thread handle field and returns the thread name.
 *
 * @returns Thread name (read only).
 * @param   phThread            The thread handle field.
 */
static const char *rtLockValidatorNameThreadHandle(RTTHREAD volatile *phThread)
{
    PRTTHREADINT pThread = rtLockValidatorReadThreadHandle(phThread);
    if (!pThread)
        return "<NIL>";
    if (!VALID_PTR(pThread))
        return "<INVALID>";
    if (pThread->u32Magic != RTTHREADINT_MAGIC)
        return "<BAD-THREAD-MAGIC>";
    return pThread->szName;
}


/**
 * Launch a simple assertion like complaint w/ panic.
 *
 * @param   RT_SRC_POS_DECL     Where from.
 * @param   pszWhat             What we're complaining about.
 * @param   ...                 Format arguments.
 */
static void rtLockValidatorComplain(RT_SRC_POS_DECL, const char *pszWhat, ...)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        RTAssertMsg1Weak("RTLockValidator", iLine, pszFile, pszFunction);
        va_list va;
        va_start(va, pszWhat);
        RTAssertMsg2WeakV(pszWhat, va);
        va_end(va);
    }
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
        RTAssertPanic();
}


/**
 * Describes the lock.
 *
 * @param   pszPrefix           Message prefix.
 * @param   Rec                 The lock record we're working on.
 * @param   pszPrefix           Message suffix.
 */
static void rtLockValidatorComplainAboutLock(const char *pszPrefix, PRTLOCKVALRECUNION pRec, const char *pszSuffix)
{
    if (    VALID_PTR(pRec)
        &&  !ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        switch (pRec->Core.u32Magic)
        {
            case RTLOCKVALRECEXCL_MAGIC:
                RTAssertMsg2AddWeak("%s%p %s xrec=%p own=%s nest=%u pos={%Rbn(%u) %Rfn %p}%s", pszPrefix,
                                    pRec->Excl.hLock, pRec->Excl.pszName, pRec,
                                    rtLockValidatorNameThreadHandle(&pRec->Excl.hThread), pRec->Excl.cRecursion,
                                    pRec->Excl.SrcPos.pszFile, pRec->Excl.SrcPos.uLine, pRec->Excl.SrcPos.pszFunction, pRec->Excl.SrcPos.uId,
                                    pszSuffix);
                break;

            case RTLOCKVALRECSHRD_MAGIC:
                RTAssertMsg2AddWeak("%s%p %s srec=%p%s", pszPrefix,
                                    pRec->Shared.hLock, pRec->Shared.pszName, pRec,
                                    pszSuffix);
                break;

            case RTLOCKVALRECSHRDOWN_MAGIC:
            {
                PRTLOCKVALRECSHRD pShared = pRec->ShrdOwner.pSharedRec;
                if (    VALID_PTR(pShared)
                    &&  pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)
                    RTAssertMsg2AddWeak("%s%p %s srec=%p trec=%p thr=%s nest=%u pos={%Rbn(%u) %Rfn %p}%s", pszPrefix,
                                        pShared->hLock, pShared->pszName, pShared,
                                        pRec, rtLockValidatorNameThreadHandle(&pRec->ShrdOwner.hThread), pRec->ShrdOwner.cRecursion,
                                        pRec->ShrdOwner.SrcPos.pszFile, pRec->ShrdOwner.SrcPos.uLine, pRec->ShrdOwner.SrcPos.pszFunction, pRec->ShrdOwner.SrcPos.uId,
                                        pszSuffix);
                else
                    RTAssertMsg2AddWeak("%sbad srec=%p trec=%p thr=%s nest=%u pos={%Rbn(%u) %Rfn %p}%s", pszPrefix,
                                        pShared,
                                        pRec, rtLockValidatorNameThreadHandle(&pRec->ShrdOwner.hThread), pRec->ShrdOwner.cRecursion,
                                        pRec->ShrdOwner.SrcPos.pszFile, pRec->ShrdOwner.SrcPos.uLine, pRec->ShrdOwner.SrcPos.pszFunction, pRec->ShrdOwner.SrcPos.uId,
                                        pszSuffix);
                break;
            }

            default:
                RTAssertMsg2AddWeak("%spRec=%p u32Magic=%#x (bad)%s", pszPrefix, pRec, pRec->Core.u32Magic, pszSuffix);
                break;
        }
    }
}


/**
 * Launch the initial complaint.
 *
 * @param   pszWhat             What we're complaining about.
 * @param   pSrcPos             Where we are complaining from, as it were.
 * @param   pThreadSelf         The calling thread.
 * @param   pRec                The main lock involved. Can be NULL.
 */
static void rtLockValidatorComplainFirst(const char *pszWhat, PCRTLOCKVALSRCPOS pSrcPos, PRTTHREADINT pThreadSelf, PRTLOCKVALRECUNION pRec)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        RTAssertMsg1Weak("RTLockValidator", pSrcPos ? pSrcPos->uLine : 0, pSrcPos ? pSrcPos->pszFile : NULL, pSrcPos ? pSrcPos->pszFunction : NULL);
        if (pSrcPos && pSrcPos->uId)
            RTAssertMsg2Weak("%s  [uId=%p  thrd=%s]\n", pszWhat, pSrcPos->uId, VALID_PTR(pThreadSelf) ? pThreadSelf->szName : "<NIL>");
        else
            RTAssertMsg2Weak("%s  [thrd=%s]\n", pszWhat, VALID_PTR(pThreadSelf) ? pThreadSelf->szName : "<NIL>");
        rtLockValidatorComplainAboutLock("Lock: ", pRec, "\n");
    }
}


/**
 * Continue bitching.
 *
 * @param   pszFormat           Format string.
 * @param   ...                 Format arguments.
 */
static void rtLockValidatorComplainMore(const char *pszFormat, ...)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        va_list va;
        va_start(va, pszFormat);
        RTAssertMsg2AddWeakV(pszFormat, va);
        va_end(va);
    }
}


/**
 * Raise a panic if enabled.
 */
static void rtLockValidatorComplainPanic(void)
{
    if (ASMAtomicUoReadBool(&g_fLockValidatorMayPanic))
        RTAssertPanic();
}


/**
 * Copy a source position record.
 *
 * @param   pDst                The destination.
 * @param   pSrc                The source.  Can be NULL.
 */
DECL_FORCE_INLINE(void) rtLockValidatorCopySrcPos(PRTLOCKVALSRCPOS pDst, PCRTLOCKVALSRCPOS pSrc)
{
    if (pSrc)
    {
        ASMAtomicUoWriteU32(&pDst->uLine,                           pSrc->uLine);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFile,      pSrc->pszFile);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFunction,  pSrc->pszFunction);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->uId,          (void *)pSrc->uId);
    }
    else
    {
        ASMAtomicUoWriteU32(&pDst->uLine,                           0);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFile,      NULL);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFunction,  NULL);
        ASMAtomicUoWritePtr((void * volatile *)&pDst->uId,          0);
    }
}


/**
 * Init a source position record.
 *
 * @param   pSrcPos             The source position record.
 */
DECL_FORCE_INLINE(void) rtLockValidatorInitSrcPos(PRTLOCKVALSRCPOS pSrcPos)
{
    pSrcPos->pszFile        = NULL;
    pSrcPos->pszFunction    = NULL;
    pSrcPos->uId            = 0;
    pSrcPos->uLine          = 0;
#if HC_ARCH_BITS == 64
    pSrcPos->u32Padding     = 0;
#endif
}


/**
 * Serializes destruction of RTLOCKVALREC* and RTTHREADINT structures.
 */
DECLHIDDEN(void) rtLockValidatorSerializeDestructEnter(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsNSEnter(hXRoads);
}


/**
 * Call after rtLockValidatorSerializeDestructEnter.
 */
DECLHIDDEN(void) rtLockValidatorSerializeDestructLeave(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsNSLeave(hXRoads);
}


/**
 * Serializes deadlock detection against destruction of the objects being
 * inspected.
 */
DECLINLINE(void) rtLockValidatorSerializeDetectionEnter(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsEWEnter(hXRoads);
}


/**
 * Call after rtLockValidatorSerializeDetectionEnter.
 */
DECLHIDDEN(void) rtLockValidatorSerializeDetectionLeave(void)
{
    RTSEMXROADS hXRoads = g_hLockValidatorXRoads;
    if (hXRoads != NIL_RTSEMXROADS)
        RTSemXRoadsEWLeave(hXRoads);
}


/**
 * Initializes the per thread lock validator data.
 *
 * @param   pPerThread      The data.
 */
DECLHIDDEN(void) rtLockValidatorInitPerThread(RTLOCKVALPERTHREAD *pPerThread)
{
    pPerThread->bmFreeShrdOwners = UINT32_MAX;

    /* ASSUMES the rest has already been zeroed. */
    Assert(pPerThread->pRec == NULL);
    Assert(pPerThread->cWriteLocks == 0);
    Assert(pPerThread->cReadLocks == 0);
}


/**
 * Verifies the deadlock stack before calling it a deadlock.
 *
 * @retval  VERR_SEM_LV_DEADLOCK if it's a deadlock.
 * @retval  VERR_SEM_LV_ILLEGAL_UPGRADE if it's a deadlock on the same lock.
 * @retval  VERR_TRY_AGAIN if something changed.
 *
 * @param   pStack              The deadlock detection stack.
 */
static int rtLockValidatorDdVerifyDeadlock(PRTLOCKVALDDSTACK pStack)
{
    uint32_t const c = pStack->c;
    for (uint32_t iPass = 0; iPass < 3; iPass++)
    {
        for (uint32_t i = 1; i < c; i++)
        {
            PRTTHREADINT pThread = pStack->a[i].pThread;
            if (pThread->u32Magic != RTTHREADINT_MAGIC)
                return VERR_TRY_AGAIN;
            if (rtThreadGetState(pThread) != pStack->a[i].enmState)
                return VERR_TRY_AGAIN;
            if (rtLockValidatorReadRecUnionPtr(&pThread->LockValidator.pRec) != pStack->a[i].pFirstSibling)
                return VERR_TRY_AGAIN;
        }
        RTThreadYield();
    }

    if (c == 1)
        return VERR_SEM_LV_ILLEGAL_UPGRADE;
    return VERR_SEM_LV_DEADLOCK;
}


/**
 * Checks for stack cycles caused by another deadlock before returning.
 *
 * @retval  VINF_SUCCESS if the stack is simply too small.
 * @retval  VERR_SEM_LV_EXISTING_DEADLOCK if a cycle was detected.
 *
 * @param   pStack              The deadlock detection stack.
 */
static int rtLockValidatorDdHandleStackOverflow(PRTLOCKVALDDSTACK pStack)
{
    for (size_t i = 0; i < RT_ELEMENTS(pStack->a) - 1; i++)
    {
        PRTTHREADINT pThread = pStack->a[i].pThread;
        for (size_t j = i + 1; j < RT_ELEMENTS(pStack->a); j++)
            if (pStack->a[j].pThread == pThread)
                return VERR_SEM_LV_EXISTING_DEADLOCK;
    }
    static bool volatile s_fComplained = false;
    if (!s_fComplained)
    {
        s_fComplained = true;
        rtLockValidatorComplain(RT_SRC_POS, "lock validator stack is too small! (%zu entries)\n", RT_ELEMENTS(pStack->a));
    }
    return VINF_SUCCESS;
}


/**
 * Worker for rtLockValidatorDoDeadlockCheck that checks if there is more work
 * to be done during unwind.
 *
 * @returns true if there is more work left for this lock, false if not.
 * @param   pRec            The current record.
 * @param   iEntry          The current index.
 * @param   pFirstSibling   The first record we examined.
 */
DECL_FORCE_INLINE(bool) rtLockValidatorDdMoreWorkLeft(PRTLOCKVALRECUNION pRec, uint32_t iEntry, PRTLOCKVALRECUNION pFirstSibling)
{
    PRTLOCKVALRECUNION pSibling;

    switch (pRec->Core.u32Magic)
    {
        case RTLOCKVALRECEXCL_MAGIC:
            pSibling = pRec->Excl.pSibling;
            break;

        case RTLOCKVALRECSHRD_MAGIC:
            if (iEntry + 1 < pRec->Shared.cAllocated)
                return true;
            pSibling = pRec->Excl.pSibling;
            break;

        default:
            return false;
    }
    return pSibling != NULL
        && pSibling != pFirstSibling;
}


/**
 * Worker for rtLockValidatorDeadlockDetection that does the actual deadlock
 * detection.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_SEM_LV_DEADLOCK
 * @retval  VERR_SEM_LV_EXISTING_DEADLOCK
 * @retval  VERR_SEM_LV_ILLEGAL_UPGRADE
 * @retval  VERR_TRY_AGAIN
 *
 * @param   pStack          The stack to use.
 * @param   pOriginalRec    The original record.
 * @param   pThreadSelf     The calling thread.
 */
static int rtLockValidatorDdDoDetection(PRTLOCKVALDDSTACK pStack, PRTLOCKVALRECUNION const pOriginalRec,
                                        PRTTHREADINT const pThreadSelf)
{
    pStack->c = 0;

    /* We could use a single RTLOCKVALDDENTRY variable here, but the
       compiler may make a better job of it when using individual variables. */
    PRTLOCKVALRECUNION  pRec            = pOriginalRec;
    PRTLOCKVALRECUNION  pFirstSibling   = pOriginalRec;
    uint32_t            iEntry          = UINT32_MAX;
    PRTTHREADINT        pThread         = NIL_RTTHREAD;
    RTTHREADSTATE       enmState        = RTTHREADSTATE_RUNNING;
    for (;;)
    {
        /*
         * Process the current record.
         */
        /* Find the next relevant owner thread. */
        PRTTHREADINT pNextThread;
        switch (pRec->Core.u32Magic)
        {
            case RTLOCKVALRECEXCL_MAGIC:
                Assert(iEntry == UINT32_MAX);
                pNextThread = rtLockValidatorReadThreadHandle(&pRec->Excl.hThread);
                if (    pNextThread
                    &&  pNextThread->u32Magic == RTTHREADINT_MAGIC
                    &&  !RTTHREAD_IS_SLEEPING(pNextThread->enmState)
                    &&  pNextThread != pThreadSelf)
                    pNextThread = NIL_RTTHREAD;

                if (    pNextThread == NIL_RTTHREAD
                    &&  pRec->Excl.pSibling
                    &&  pRec->Excl.pSibling != pFirstSibling)
                {
                    pRec = pRec->Excl.pSibling;
                    continue;
                }
                break;

            case RTLOCKVALRECSHRD_MAGIC:
                /* Skip to the next sibling if same side.  ASSUMES reader priority. */
                /** @todo The read side of a read-write lock is problematic if
                 * the implementation prioritizes writers over readers because
                 * that means we should could deadlock against current readers
                 * if a writer showed up.  If the RW sem implementation is
                 * wrapping some native API, it's not so easy to detect when we
                 * should do this and when we shouldn't.  Checking when we
                 * shouldn't is subject to wakeup scheduling and cannot easily
                 * be made reliable.
                 *
                 * At the moment we circumvent all this mess by declaring that
                 * readers has priority.  This is TRUE on linux, but probably
                 * isn't on Solaris and FreeBSD. */
                if (   pRec == pFirstSibling
                    && pRec->Shared.pSibling != NULL
                    && pRec->Shared.pSibling != pFirstSibling)
                {
                    pRec = pRec->Shared.pSibling;
                    Assert(iEntry == UINT32_MAX);
                    continue;
                }

                /* Scan the owner table for blocked owners. */
                pNextThread = NIL_RTTHREAD;
                if (ASMAtomicUoReadU32(&pRec->Shared.cEntries) > 0)
                {
                    uint32_t                        cAllocated = ASMAtomicUoReadU32(&pRec->Shared.cAllocated);
                    PRTLOCKVALRECSHRDOWN volatile  *papOwners  = pRec->Shared.papOwners;
                    while (++iEntry < cAllocated)
                    {
                        PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorUoReadSharedOwner(&papOwners[iEntry]);
                        if (   pEntry
                            && pEntry->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC)
                        {
                            pNextThread = rtLockValidatorReadThreadHandle(&pEntry->hThread);
                            if (pNextThread)
                            {
                                if (   pNextThread->u32Magic == RTTHREADINT_MAGIC
                                    && (   RTTHREAD_IS_SLEEPING(pNextThread->enmState)
                                        || pNextThread == pThreadSelf))
                                    break;
                                pNextThread = NIL_RTTHREAD;
                            }
                        }
                        else
                            Assert(!pEntry || pEntry->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC_DEAD);
                    }
                    if (pNextThread == NIL_RTTHREAD)
                        break;
                }

                /* Advance to the next sibling, if any. */
                if (   pRec->Shared.pSibling != NULL
                    && pRec->Shared.pSibling != pFirstSibling)
                {
                    pRec = pRec->Shared.pSibling;
                    iEntry = UINT32_MAX;
                    continue;
                }
                break;

            case RTLOCKVALRECEXCL_MAGIC_DEAD:
            case RTLOCKVALRECSHRD_MAGIC_DEAD:
                pNextThread = NIL_RTTHREAD;
                break;

            case RTLOCKVALRECSHRDOWN_MAGIC:
            case RTLOCKVALRECSHRDOWN_MAGIC_DEAD:
            default:
                AssertMsgFailed(("%p: %#x\n", pRec, pRec->Core));
                pNextThread = NIL_RTTHREAD;
                break;
        }

        /* If we found a thread, check if it is still waiting for something. */
        RTTHREADSTATE       enmNextState = RTTHREADSTATE_RUNNING;
        PRTLOCKVALRECUNION  pNextRec     = NULL;
        if (   pNextThread != NIL_RTTHREAD
            && RT_LIKELY(pNextThread->u32Magic == RTTHREADINT_MAGIC))
        {
            do
            {
                enmNextState = rtThreadGetState(pNextThread);
                if (    !RTTHREAD_IS_SLEEPING(enmNextState)
                    &&  pNextThread != pThreadSelf)
                    break;
                pNextRec = rtLockValidatorReadRecUnionPtr(&pNextThread->LockValidator.pRec);
                if (RT_LIKELY(   !pNextRec
                              || enmNextState == rtThreadGetState(pNextThread)))
                    break;
                pNextRec = NULL;
            } while (pNextThread->u32Magic == RTTHREADINT_MAGIC);
        }
        if (pNextRec)
        {
            /*
             * Recurse and check for deadlock.
             */
            uint32_t i = pStack->c;
            if (RT_UNLIKELY(i >= RT_ELEMENTS(pStack->a)))
                return rtLockValidatorDdHandleStackOverflow(pStack);

            pStack->c++;
            pStack->a[i].pRec           = pRec;
            pStack->a[i].iEntry         = iEntry;
            pStack->a[i].enmState       = enmState;
            pStack->a[i].pThread        = pThread;
            pStack->a[i].pFirstSibling  = pFirstSibling;

            if (RT_UNLIKELY(pNextThread == pThreadSelf))
                return rtLockValidatorDdVerifyDeadlock(pStack);

            pRec            = pNextRec;
            pFirstSibling   = pNextRec;
            iEntry          = UINT32_MAX;
            enmState        = enmNextState;
            pThread         = pNextThread;
        }
        else if (RT_LIKELY(!pNextThread))
        {
            /*
             * No deadlock here, unwind the stack and deal with any unfinished
             * business there.
             */
            uint32_t i = pStack->c;
            for (;;)
            {
                /* pop */
                if (i == 0)
                    return VINF_SUCCESS;
                i--;

                /* examine it. */
                pRec            = pStack->a[i].pRec;
                pFirstSibling   = pStack->a[i].pFirstSibling;
                iEntry          = pStack->a[i].iEntry;
                if (rtLockValidatorDdMoreWorkLeft(pRec, iEntry, pFirstSibling))
                {
                    enmState    = pStack->a[i].enmState;
                    pThread     = pStack->a[i].pThread;
                    pStack->c   = i;
                    break;
                }
            }
        }
        /* else: see if there is another thread to check for this lock. */
    }
}


/**
 * Check for the simple no-deadlock case.
 *
 * @returns true if no deadlock, false if further investigation is required.
 *
 * @param   pOriginalRec    The original record.
 */
DECLINLINE(int) rtLockValidatorIsSimpleNoDeadlockCase(PRTLOCKVALRECUNION pOriginalRec)
{
    if (    pOriginalRec->Excl.Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
        &&  !pOriginalRec->Excl.pSibling)
    {
        PRTTHREADINT pThread = rtLockValidatorReadThreadHandle(&pOriginalRec->Excl.hThread);
        if (   !pThread
            || pThread->u32Magic != RTTHREADINT_MAGIC)
            return true;
        RTTHREADSTATE enmState = rtThreadGetState(pThread);
        if (!RTTHREAD_IS_SLEEPING(enmState))
            return true;
    }
    return false;
}


/**
 * Worker for rtLockValidatorDeadlockDetection that bitches about a deadlock.
 *
 * @param   pStack          The chain of locks causing the deadlock.
 * @param   pRec            The record relating to the current thread's lock
 *                          operation.
 * @param   pThreadSelf     This thread.
 * @param   pSrcPos         Where we are going to deadlock.
 * @param   rc              The return code.
 */
static void rcLockValidatorDoDeadlockComplaining(PRTLOCKVALDDSTACK pStack, PRTLOCKVALRECUNION pRec,
                                                 PRTTHREADINT pThreadSelf, PCRTLOCKVALSRCPOS pSrcPos, int rc)
{
    if (!ASMAtomicUoReadBool(&g_fLockValidatorQuiet))
    {
        const char *pszWhat;
        switch (rc)
        {
            case VERR_SEM_LV_DEADLOCK:          pszWhat = "Detected deadlock!"; break;
            case VERR_SEM_LV_EXISTING_DEADLOCK: pszWhat = "Found existing deadlock!"; break;
            case VERR_SEM_LV_ILLEGAL_UPGRADE:   pszWhat = "Illegal lock upgrade!"; break;
            default:            AssertFailed(); pszWhat = "!unexpected rc!"; break;
        }
        rtLockValidatorComplainFirst(pszWhat, pSrcPos, pThreadSelf, pStack->a[0].pRec != pRec ? pRec : NULL);
        rtLockValidatorComplainMore("---- start of deadlock chain - %u entries ----\n", pStack->c);
        for (uint32_t i = 0; i < pStack->c; i++)
        {
            char szPrefix[24];
            RTStrPrintf(szPrefix, sizeof(szPrefix), "#%02u: ", i);
            PRTLOCKVALRECSHRDOWN pShrdOwner = NULL;
            if (pStack->a[i].pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)
                pShrdOwner = pStack->a[i].pRec->Shared.papOwners[pStack->a[i].iEntry];
            if (VALID_PTR(pShrdOwner) && pShrdOwner->Core.u32Magic == RTLOCKVALRECSHRDOWN_MAGIC)
                rtLockValidatorComplainAboutLock(szPrefix, (PRTLOCKVALRECUNION)pShrdOwner, "\n");
            else
                rtLockValidatorComplainAboutLock(szPrefix, pStack->a[i].pRec, "\n");
        }
        rtLockValidatorComplainMore("---- end of deadlock chain ----\n");
    }

    rtLockValidatorComplainPanic();
}


/**
 * Perform deadlock detection.
 *
 * @retval  VINF_SUCCESS
 * @retval  VERR_SEM_LV_DEADLOCK
 * @retval  VERR_SEM_LV_EXISTING_DEADLOCK
 * @retval  VERR_SEM_LV_ILLEGAL_UPGRADE
 *
 * @param   pRec            The record relating to the current thread's lock
 *                          operation.
 * @param   pThreadSelf     The current thread.
 * @param   pSrcPos         The position of the current lock operation.
 */
static int rtLockValidatorDeadlockDetection(PRTLOCKVALRECUNION pRec, PRTTHREADINT pThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
#ifdef DEBUG_bird
    RTLOCKVALDDSTACK Stack;
    int rc = rtLockValidatorDdDoDetection(&Stack, pRec, pThreadSelf);
    if (RT_SUCCESS(rc))
        return VINF_SUCCESS;

    if (rc == VERR_TRY_AGAIN)
    {
        for (uint32_t iLoop = 0; ; iLoop++)
        {
            rc = rtLockValidatorDdDoDetection(&Stack, pRec, pThreadSelf);
            if (RT_SUCCESS_NP(rc))
                return VINF_SUCCESS;
            if (rc != VERR_TRY_AGAIN)
                break;
            RTThreadYield();
            if (iLoop >= 3)
                return VINF_SUCCESS;
        }
    }

    rcLockValidatorDoDeadlockComplaining(&Stack, pRec, pThreadSelf, pSrcPos, rc);
    return rc;
#else
    return VINF_SUCCESS;
#endif
}


/**
 * Unlinks all siblings.
 *
 * This is used during record deletion and assumes no races.
 *
 * @param   pCore               One of the siblings.
 */
static void rtLockValidatorUnlinkAllSiblings(PRTLOCKVALRECCORE pCore)
{
    /* ASSUMES sibling destruction doesn't involve any races and that all
       related records are to be disposed off now.  */
    PRTLOCKVALRECUNION pSibling = (PRTLOCKVALRECUNION)pCore;
    while (pSibling)
    {
        PRTLOCKVALRECUNION volatile *ppCoreNext;
        switch (pSibling->Core.u32Magic)
        {
            case RTLOCKVALRECEXCL_MAGIC:
            case RTLOCKVALRECEXCL_MAGIC_DEAD:
                ppCoreNext = &pSibling->Excl.pSibling;
                break;

            case RTLOCKVALRECSHRD_MAGIC:
            case RTLOCKVALRECSHRD_MAGIC_DEAD:
                ppCoreNext = &pSibling->Shared.pSibling;
                break;

            default:
                AssertFailed();
                ppCoreNext = NULL;
                break;
        }
        if (RT_UNLIKELY(ppCoreNext))
            break;
        pSibling = (PRTLOCKVALRECUNION)ASMAtomicXchgPtr((void * volatile *)ppCoreNext, NULL);
    }
}


RTDECL(int) RTLockValidatorRecMakeSiblings(PRTLOCKVALRECCORE pRec1, PRTLOCKVALRECCORE pRec2)
{
    /*
     * Validate input.
     */
    PRTLOCKVALRECUNION p1 = (PRTLOCKVALRECUNION)pRec1;
    PRTLOCKVALRECUNION p2 = (PRTLOCKVALRECUNION)pRec2;

    AssertPtrReturn(p1, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(   p1->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 || p1->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);

    AssertPtrReturn(p2, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(   p2->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 || p2->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Link them (circular list).
     */
    if (    p1->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
        &&  p2->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)
    {
        p1->Excl.pSibling   = p2;
        p2->Shared.pSibling = p1;
    }
    else if (   p1->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
             && p2->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC)
    {
        p1->Shared.pSibling = p2;
        p2->Excl.pSibling   = p1;
    }
    else
        AssertFailedReturn(VERR_SEM_LV_INVALID_PARAMETER); /* unsupported mix */

    return VINF_SUCCESS;
}




RTDECL(void) RTLockValidatorRecExclInit(PRTLOCKVALRECEXCL pRec, RTLOCKVALIDATORCLASS hClass,
                                        uint32_t uSubClass, const char *pszName, void *hLock)
{
    pRec->Core.u32Magic = RTLOCKVALRECEXCL_MAGIC;
    pRec->fEnabled      = RTLockValidatorIsEnabled();
    pRec->afReserved[0] = 0;
    pRec->afReserved[1] = 0;
    pRec->afReserved[2] = 0;
    rtLockValidatorInitSrcPos(&pRec->SrcPos);
    pRec->hThread       = NIL_RTTHREAD;
    pRec->pDown         = NULL;
    pRec->hClass        = hClass;
    pRec->uSubClass     = uSubClass;
    pRec->cRecursion    = 0;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;
    pRec->pSibling      = NULL;

    /* Lazily initialize the crossroads semaphore. */
    static uint32_t volatile s_fInitializing = false;
    if (RT_UNLIKELY(    g_hLockValidatorXRoads == NIL_RTSEMXROADS
                    &&  ASMAtomicCmpXchgU32(&s_fInitializing, true, false)))
    {
        RTSEMXROADS hXRoads;
        int rc = RTSemXRoadsCreate(&hXRoads);
        if (RT_SUCCESS(rc))
            ASMAtomicWriteHandle(&g_hLockValidatorXRoads, hXRoads);
        ASMAtomicWriteU32(&s_fInitializing, false);
    }
}


RTDECL(int)  RTLockValidatorRecExclCreate(PRTLOCKVALRECEXCL *ppRec, RTLOCKVALIDATORCLASS hClass,
                                          uint32_t uSubClass, const char *pszName, void *pvLock)
{
    PRTLOCKVALRECEXCL pRec;
    *ppRec = pRec = (PRTLOCKVALRECEXCL)RTMemAlloc(sizeof(*pRec));
    if (!pRec)
        return VERR_NO_MEMORY;

    RTLockValidatorRecExclInit(pRec, hClass, uSubClass, pszName, pvLock);

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecExclDelete(PRTLOCKVALRECEXCL pRec)
{
    Assert(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC);

    rtLockValidatorSerializeDestructEnter();

    ASMAtomicWriteU32(&pRec->Core.u32Magic, RTLOCKVALRECEXCL_MAGIC_DEAD);
    ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    ASMAtomicWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);
    if (pRec->pSibling)
        rtLockValidatorUnlinkAllSiblings(&pRec->Core);
    rtLockValidatorSerializeDestructLeave();
}


RTDECL(void) RTLockValidatorRecExclDestroy(PRTLOCKVALRECEXCL *ppRec)
{
    PRTLOCKVALRECEXCL pRec = *ppRec;
    *ppRec = NULL;
    if (pRec)
    {
        RTLockValidatorRecExclDelete(pRec);
        RTMemFree(pRec);
    }
}


RTDECL(void) RTLockValidatorRecExclSetOwner(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf,
                                            PCRTLOCKVALSRCPOS pSrcPos, bool fFirstRecursion)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC);
    if (!pRec->fEnabled)
        return;
    if (hThreadSelf == NIL_RTTHREAD)
    {
        hThreadSelf = RTThreadSelfAutoAdopt();
        AssertReturnVoid(hThreadSelf != NIL_RTTHREAD);
    }
    Assert(hThreadSelf == RTThreadSelf());

    ASMAtomicIncS32(&hThreadSelf->LockValidator.cWriteLocks);

    if (pRec->hThread == hThreadSelf)
    {
        Assert(!fFirstRecursion);
        pRec->cRecursion++;
    }
    else
    {
        Assert(pRec->hThread == NIL_RTTHREAD);

        /*
         * Update the record.
         */
        rtLockValidatorCopySrcPos(&pRec->SrcPos, pSrcPos);
        ASMAtomicUoWriteU32(&pRec->cRecursion, 1);
        ASMAtomicWriteHandle(&pRec->hThread, hThreadSelf);

        /*
         * Push the lock onto the lock stack.
         */
        /** @todo push it onto the per-thread lock stack. */
    }
}


RTDECL(int)  RTLockValidatorRecExclReleaseOwner(PRTLOCKVALRECEXCL pRec, bool fFinalRecursion)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    RTLockValidatorRecExclReleaseOwnerUnchecked(pRec);
    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecExclReleaseOwnerUnchecked(PRTLOCKVALRECEXCL pRec)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC);
    if (!pRec->fEnabled)
        return;
    RTTHREADINT *pThread = pRec->hThread;
    AssertReturnVoid(pThread != NIL_RTTHREAD);
    Assert(pThread == RTThreadSelf());

    ASMAtomicDecS32(&pThread->LockValidator.cWriteLocks);

    if (ASMAtomicDecU32(&pRec->cRecursion) == 0)
    {
        /*
         * Pop (remove) the lock.
         */
        /** @todo remove it from the per-thread stack/whatever. */

        /*
         * Update the record.
         */
        ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    }
}


RTDECL(int) RTLockValidatorRecExclRecursion(PRTLOCKVALRECEXCL pRec, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion < _1M);
    pRec->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclUnwind(PRTLOCKVALRECEXCL pRec)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion);
    pRec->cRecursion--;
    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclRecursionMixed(PRTLOCKVALRECEXCL pRec, PRTLOCKVALRECCORE pRecMixed, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    PRTLOCKVALRECUNION pRecMixedU = (PRTLOCKVALRECUNION)pRecMixed;
    AssertReturn(   pRecMixedU->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 || pRecMixedU->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion < _1M);
    pRec->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclUnwindMixed(PRTLOCKVALRECEXCL pRec, PRTLOCKVALRECCORE pRecMixed)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    PRTLOCKVALRECUNION pRecMixedU = (PRTLOCKVALRECUNION)pRecMixed;
    AssertReturn(   pRecMixedU->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
                 || pRecMixedU->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion);
    pRec->cRecursion--;
    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclCheckOrder(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecExclCheckBlocking(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf,
                                                PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk)
{
    /*
     * Fend off wild life.
     */
    PRTLOCKVALRECUNION pRecU = (PRTLOCKVALRECUNION)pRec;
    AssertPtrReturn(pRecU, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRecU->Core.u32Magic == RTLOCKVALRECEXCL_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRecU->Excl.fEnabled)
        return VINF_SUCCESS;

    PRTTHREADINT pThreadSelf = hThreadSelf;
    AssertPtrReturn(pThreadSelf, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThreadSelf->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    Assert(pThreadSelf == RTThreadSelf());

    RTTHREADSTATE enmThreadState = rtThreadGetState(pThreadSelf);
    AssertReturn(   enmThreadState == RTTHREADSTATE_RUNNING
                 || enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                 || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Record the location.
     */
    rtLockValidatorWriteRecUnionPtr(&pThreadSelf->LockValidator.pRec, pRecU);
    rtLockValidatorCopySrcPos(&pThreadSelf->LockValidator.SrcPos, pSrcPos);

    /*
     * Don't do deadlock detection if we're recursing.
     *
     * On some hosts we don't do recursion accounting our selves and there
     * isn't any other place to check for this.  semmutex-win.cpp for instance.
     */
    if (rtLockValidatorReadThreadHandle(&pRecU->Excl.hThread) == pThreadSelf)
    {
        if (fRecursiveOk)
            return VINF_SUCCESS;
        rtLockValidatorComplainFirst("Recursion not allowed", pSrcPos, pThreadSelf, pRecU);
        rtLockValidatorComplainPanic();
        return VERR_SEM_LV_NESTED;
    }

    /*
     * Perform deadlock detection.
     */
    if (rtLockValidatorIsSimpleNoDeadlockCase(pRecU))
        return VINF_SUCCESS;
    return rtLockValidatorDeadlockDetection(pRecU, pThreadSelf, pSrcPos);
}
RT_EXPORT_SYMBOL(RTLockValidatorRecExclCheckBlocking);


RTDECL(int) RTLockValidatorRecExclCheckOrderAndBlocking(PRTLOCKVALRECEXCL pRec, RTTHREAD hThreadSelf,
                                                        PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk)
{
    int rc = RTLockValidatorRecExclCheckOrder(pRec, hThreadSelf, pSrcPos);
    if (RT_SUCCESS(rc))
        rc = RTLockValidatorRecExclCheckBlocking(pRec, hThreadSelf, pSrcPos, fRecursiveOk);
    return rc;
}
RT_EXPORT_SYMBOL(RTLockValidatorRecExclCheckOrderAndBlocking);


RTDECL(void) RTLockValidatorRecSharedInit(PRTLOCKVALRECSHRD pRec, RTLOCKVALIDATORCLASS hClass,
                                          uint32_t uSubClass, const char *pszName, void *hLock)
{
    pRec->Core.u32Magic = RTLOCKVALRECSHRD_MAGIC;
    pRec->uSubClass     = uSubClass;
    pRec->hClass        = hClass;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;
    pRec->fEnabled      = RTLockValidatorIsEnabled();
    pRec->pSibling      = NULL;

    /* the table */
    pRec->cEntries      = 0;
    pRec->iLastEntry    = 0;
    pRec->cAllocated    = 0;
    pRec->fReallocating = false;
    pRec->afPadding[0]  = false;
    pRec->afPadding[1]  = false;
    pRec->papOwners     = NULL;
#if HC_ARCH_BITS == 32
    pRec->u32Alignment  = UINT32_MAX;
#endif
}


RTDECL(void) RTLockValidatorRecSharedDelete(PRTLOCKVALRECSHRD pRec)
{
    Assert(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);

    /*
     * Flip it into table realloc mode and take the destruction lock.
     */
    rtLockValidatorSerializeDestructEnter();
    while (!ASMAtomicCmpXchgBool(&pRec->fReallocating, true, false))
    {
        rtLockValidatorSerializeDestructLeave();

        rtLockValidatorSerializeDetectionEnter();
        rtLockValidatorSerializeDetectionLeave();

        rtLockValidatorSerializeDestructEnter();
    }

    ASMAtomicWriteU32(&pRec->Core.u32Magic, RTLOCKVALRECSHRD_MAGIC_DEAD);
    ASMAtomicUoWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);
    if (pRec->papOwners)
    {
        PRTLOCKVALRECSHRDOWN volatile *papOwners = pRec->papOwners;
        ASMAtomicUoWritePtr((void * volatile *)&pRec->papOwners, NULL);
        ASMAtomicUoWriteU32(&pRec->cAllocated, 0);

        RTMemFree((void *)pRec->papOwners);
    }
    if (pRec->pSibling)
        rtLockValidatorUnlinkAllSiblings(&pRec->Core);
    ASMAtomicWriteBool(&pRec->fReallocating, false);

    rtLockValidatorSerializeDestructLeave();
}


/**
 * Locates an owner (thread) in a shared lock record.
 *
 * @returns Pointer to the owner entry on success, NULL on failure..
 * @param   pShared             The shared lock record.
 * @param   hThread             The thread (owner) to find.
 * @param   piEntry             Where to optionally return the table in index.
 *                              Optional.
 */
DECLINLINE(PRTLOCKVALRECSHRDOWN)
rtLockValidatorRecSharedFindOwner(PRTLOCKVALRECSHRD pShared, RTTHREAD hThread, uint32_t *piEntry)
{
    rtLockValidatorSerializeDetectionEnter();

    PRTLOCKVALRECSHRDOWN volatile *papOwners = pShared->papOwners;
    if (papOwners)
    {
        uint32_t const cMax = pShared->cAllocated;
        for (uint32_t iEntry = 0; iEntry < cMax; iEntry++)
        {
            PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorUoReadSharedOwner(&papOwners[iEntry]);
            if (pEntry && pEntry->hThread == hThread)
            {
                rtLockValidatorSerializeDetectionLeave();
                if (piEntry)
                    *piEntry = iEntry;
                return pEntry;
            }
        }
    }

    rtLockValidatorSerializeDetectionLeave();
    return NULL;
}


RTDECL(int) RTLockValidatorRecSharedCheckOrder(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturn(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    Assert(hThreadSelf == NIL_RTTHREAD || hThreadSelf == RTThreadSelf());

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecSharedCheckBlocking(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf,
                                                  PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk)
{
    /*
     * Fend off wild life.
     */
    PRTLOCKVALRECUNION pRecU = (PRTLOCKVALRECUNION)pRec;
    AssertPtrReturn(pRecU, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRecU->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRecU->Shared.fEnabled)
        return VINF_SUCCESS;

    PRTTHREADINT pThreadSelf = hThreadSelf;
    AssertPtrReturn(pThreadSelf, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThreadSelf->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    Assert(pThreadSelf == RTThreadSelf());

    RTTHREADSTATE enmThreadState = rtThreadGetState(pThreadSelf);
    AssertReturn(   enmThreadState == RTTHREADSTATE_RUNNING
                 || enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                 || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Record the location.
     */
    rtLockValidatorWriteRecUnionPtr(&pThreadSelf->LockValidator.pRec, pRecU);
    rtLockValidatorCopySrcPos(&pThreadSelf->LockValidator.SrcPos, pSrcPos);

    /*
     * Don't do deadlock detection if we're recursing.
     */
    PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorRecSharedFindOwner(&pRecU->Shared, pThreadSelf, NULL);
    if (pEntry)
    {
        if (fRecursiveOk)
            return VINF_SUCCESS;
        rtLockValidatorComplainFirst("Recursion not allowed", pSrcPos, pThreadSelf, pRecU);
        rtLockValidatorComplainPanic();
        return VERR_SEM_LV_NESTED;
    }

    /*
     * Perform deadlock detection.
     */
    if (rtLockValidatorIsSimpleNoDeadlockCase(pRecU))
        return VINF_SUCCESS;
    return rtLockValidatorDeadlockDetection(pRecU, pThreadSelf, pSrcPos);
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedCheckBlocking);


RTDECL(int) RTLockValidatorRecSharedCheckOrderAndBlocking(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf, PCRTLOCKVALSRCPOS pSrcPos, bool fRecursiveOk)
{
    int rc = RTLockValidatorRecSharedCheckOrder(pRec, hThreadSelf, pSrcPos);
    if (RT_SUCCESS(rc))
        rc = RTLockValidatorRecSharedCheckBlocking(pRec, hThreadSelf, pSrcPos, fRecursiveOk);
    return rc;
}
RT_EXPORT_SYMBOL(RTLockValidatorRecSharedCheckOrderAndBlocking);


/**
 * Allocates and initializes an owner entry for the shared lock record.
 *
 * @returns The new owner entry.
 * @param   pShared             The shared lock record.
 * @param   pThreadSelf         The calling thread and owner.  Used for record
 *                              initialization and allocation.
 * @param   pSrcPos             The source position.
 */
DECLINLINE(PRTLOCKVALRECSHRDOWN)
rtLockValidatorRecSharedAllocOwner(PRTLOCKVALRECSHRD pRead, PRTTHREADINT pThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    PRTLOCKVALRECSHRDOWN pEntry;

    /*
     * Check if the thread has any statically allocated records we can use.
     */
    unsigned iEntry = ASMBitFirstSetU32(pThreadSelf->LockValidator.bmFreeShrdOwners);
    if (iEntry > 0)
    {
        iEntry--;
        pThreadSelf->LockValidator.bmFreeShrdOwners &= ~RT_BIT_32(iEntry);
        pEntry = &pThreadSelf->LockValidator.aShrdOwners[iEntry];
        Assert(!pEntry->fReserved);
        pEntry->fStaticAlloc = true;
    }
    else
    {
        pEntry = (PRTLOCKVALRECSHRDOWN)RTMemAlloc(sizeof(RTLOCKVALRECSHRDOWN));
        if (RT_UNLIKELY(!pEntry))
            return NULL;
        pEntry->fStaticAlloc = false;
    }

    pEntry->Core.u32Magic   = RTLOCKVALRECSHRDOWN_MAGIC;
    pEntry->cRecursion      = 1;
    pEntry->fReserved       = true;
    pEntry->hThread         = pThreadSelf;
    pEntry->pDown           = NULL;
    pEntry->pSharedRec      = pRead;
#if HC_ARCH_BITS == 32
    pEntry->pvReserved      = NULL;
#endif
    if (pSrcPos)
        pEntry->SrcPos      = *pSrcPos;
    else
        rtLockValidatorInitSrcPos(&pEntry->SrcPos);
    return pEntry;
}


/**
 * Frees an owner entry allocated by rtLockValidatorRecSharedAllocOwner.
 *
 * @param   pEntry              The owner entry.
 */
DECLINLINE(void) rtLockValidatorRecSharedFreeOwner(PRTLOCKVALRECSHRDOWN pEntry)
{
    if (pEntry)
    {
        ASMAtomicWriteU32(&pEntry->Core.u32Magic, RTLOCKVALRECSHRDOWN_MAGIC_DEAD);

        PRTTHREADINT pThreadSelf = pEntry->hThread;
        ASMAtomicXchgHandle(&pEntry->hThread, NIL_RTTHREAD, &pThreadSelf);
        Assert(pThreadSelf == RTThreadSelf());

        Assert(pEntry->fReserved);
        pEntry->fReserved = false;

        if (pEntry->fStaticAlloc)
        {
            uintptr_t iEntry = pEntry - &pThreadSelf->LockValidator.aShrdOwners[0];
            AssertReleaseReturnVoid(iEntry < RT_ELEMENTS(pThreadSelf->LockValidator.aShrdOwners));
            pThreadSelf->LockValidator.bmFreeShrdOwners |= RT_BIT_32(iEntry);
        }
        else
        {
            rtLockValidatorSerializeDestructEnter();
            rtLockValidatorSerializeDestructLeave();

            RTMemFree(pEntry);
        }
    }
}


/**
 * Make more room in the table.
 *
 * @retval  true on success
 * @retval  false if we're out of memory or running into a bad race condition
 *          (probably a bug somewhere).  No longer holding the lock.
 *
 * @param   pShared             The shared lock record.
 */
static bool rtLockValidatorRecSharedMakeRoom(PRTLOCKVALRECSHRD pShared)
{
    for (unsigned i = 0; i < 1000; i++)
    {
        /*
         * Switch to the other data access direction.
         */
        rtLockValidatorSerializeDetectionLeave();
        if (i >= 10)
        {
            Assert(i != 10 && i != 100);
            RTThreadSleep(i >= 100);
        }
        rtLockValidatorSerializeDestructEnter();

        /*
         * Try grab the privilege to reallocating the table.
         */
        if (    pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC
            &&  ASMAtomicCmpXchgBool(&pShared->fReallocating, true, false))
        {
            uint32_t cAllocated = pShared->cAllocated;
            if (cAllocated < pShared->cEntries)
            {
                /*
                 * Ok, still not enough space.  Reallocate the table.
                 */
#if 0  /** @todo enable this after making sure growing works flawlessly. */
                uint32_t                cInc = RT_ALIGN_32(pShared->cEntries - cAllocated, 16);
#else
                uint32_t                cInc = RT_ALIGN_32(pShared->cEntries - cAllocated, 1);
#endif
                PRTLOCKVALRECSHRDOWN   *papOwners;
                papOwners = (PRTLOCKVALRECSHRDOWN *)RTMemRealloc((void *)pShared->papOwners,
                                                                 (cAllocated + cInc) * sizeof(void *));
                if (!papOwners)
                {
                    ASMAtomicWriteBool(&pShared->fReallocating, false);
                    rtLockValidatorSerializeDestructLeave();
                    /* RTMemRealloc will assert */
                    return false;
                }

                while (cInc-- > 0)
                {
                    papOwners[cAllocated] = NULL;
                    cAllocated++;
                }

                ASMAtomicWritePtr((void * volatile *)&pShared->papOwners, papOwners);
                ASMAtomicWriteU32(&pShared->cAllocated, cAllocated);
            }
            ASMAtomicWriteBool(&pShared->fReallocating, false);
        }
        rtLockValidatorSerializeDestructLeave();

        rtLockValidatorSerializeDetectionEnter();
        if (RT_UNLIKELY(pShared->Core.u32Magic != RTLOCKVALRECSHRD_MAGIC))
            break;

        if (pShared->cAllocated >= pShared->cEntries)
            return true;
    }

    rtLockValidatorSerializeDetectionLeave();
    AssertFailed(); /* too many iterations or destroyed while racing. */
    return false;
}


/**
 * Adds an owner entry to a shared lock record.
 *
 * @returns true on success, false on serious race or we're if out of memory.
 * @param   pShared             The shared lock record.
 * @param   pEntry              The owner entry.
 */
DECLINLINE(bool) rtLockValidatorRecSharedAddOwner(PRTLOCKVALRECSHRD pShared, PRTLOCKVALRECSHRDOWN pEntry)
{
    rtLockValidatorSerializeDetectionEnter();
    if (RT_LIKELY(pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC)) /* paranoia */
    {
        if (   ASMAtomicIncU32(&pShared->cEntries) > pShared->cAllocated /** @todo add fudge */
            && !rtLockValidatorRecSharedMakeRoom(pShared))
            return false; /* the worker leave the lock */

        PRTLOCKVALRECSHRDOWN volatile  *papOwners = pShared->papOwners;
        uint32_t const                  cMax      = pShared->cAllocated;
        for (unsigned i = 0; i < 100; i++)
        {
            for (uint32_t iEntry = 0; iEntry < cMax; iEntry++)
            {
                if (ASMAtomicCmpXchgPtr((void * volatile *)&papOwners[iEntry], pEntry, NULL))
                {
                    rtLockValidatorSerializeDetectionLeave();
                    return true;
                }
            }
            Assert(i != 25);
        }
        AssertFailed();
    }
    rtLockValidatorSerializeDetectionLeave();
    return false;
}


/**
 * Remove an owner entry from a shared lock record and free it.
 *
 * @param   pShared             The shared lock record.
 * @param   pEntry              The owner entry to remove.
 * @param   iEntry              The last known index.
 */
DECLINLINE(void) rtLockValidatorRecSharedRemoveAndFreeOwner(PRTLOCKVALRECSHRD pShared, PRTLOCKVALRECSHRDOWN pEntry,
                                                            uint32_t iEntry)
{
    /*
     * Remove it from the table.
     */
    rtLockValidatorSerializeDetectionEnter();
    AssertReturnVoidStmt(pShared->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, rtLockValidatorSerializeDetectionLeave());
    if (RT_UNLIKELY(   iEntry >= pShared->cAllocated
                    || !ASMAtomicCmpXchgPtr((void * volatile *)&pShared->papOwners[iEntry], NULL, pEntry)))
    {
        /* this shouldn't happen yet... */
        AssertFailed();
        PRTLOCKVALRECSHRDOWN volatile  *papOwners = pShared->papOwners;
        uint32_t const                  cMax      = pShared->cAllocated;
        for (iEntry = 0; iEntry < cMax; iEntry++)
            if (ASMAtomicCmpXchgPtr((void * volatile *)&papOwners[iEntry], NULL, pEntry))
               break;
        AssertReturnVoidStmt(iEntry < cMax, rtLockValidatorSerializeDetectionLeave());
    }
    uint32_t cNow = ASMAtomicDecU32(&pShared->cEntries);
    Assert(!(cNow & RT_BIT_32(31))); NOREF(cNow);
    rtLockValidatorSerializeDetectionLeave();

    /*
     * Successfully removed, now free it.
     */
    rtLockValidatorRecSharedFreeOwner(pEntry);
}


RTDECL(void) RTLockValidatorSharedRecAddOwner(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf, PCRTLOCKVALSRCPOS pSrcPos)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);
    if (!pRec->fEnabled)
        return;
    AssertReturnVoid(hThreadSelf != NIL_RTTHREAD);
    AssertReturnVoid(hThreadSelf->u32Magic == RTTHREADINT_MAGIC);
    Assert(hThreadSelf == RTThreadSelf());

    /*
     * Recursive?
     *
     * Note! This code can be optimized to try avoid scanning the table on
     *       insert.  However, that's annoying work that makes the code big,
     *       so it can wait til later sometime.
     */
    PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorRecSharedFindOwner(pRec, hThreadSelf, NULL);
    if (pEntry)
    {
        pEntry->cRecursion++;
        return;
    }

    /*
     * Allocate a new owner entry and insert it into the table.
     */
    pEntry = rtLockValidatorRecSharedAllocOwner(pRec, hThreadSelf, pSrcPos);
    if (    pEntry
        &&  !rtLockValidatorRecSharedAddOwner(pRec, pEntry))
        rtLockValidatorRecSharedFreeOwner(pEntry);
}
RT_EXPORT_SYMBOL(RTLockValidatorSharedRecAddOwner);


RTDECL(void) RTLockValidatorSharedRecRemoveOwner(PRTLOCKVALRECSHRD pRec, RTTHREAD hThreadSelf)
{
    AssertReturnVoid(pRec->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC);
    if (!pRec->fEnabled)
        return;
    AssertReturnVoid(hThreadSelf != NIL_RTTHREAD);
    AssertReturnVoid(hThreadSelf->u32Magic == RTTHREADINT_MAGIC);
    Assert(hThreadSelf == RTThreadSelf());

    /*
     * Find the entry hope it's a recursive one.
     */
    uint32_t iEntry = UINT32_MAX; /* shuts up gcc */
    PRTLOCKVALRECSHRDOWN pEntry = rtLockValidatorRecSharedFindOwner(pRec, hThreadSelf, &iEntry);
    AssertReturnVoid(pEntry);
    if (pEntry->cRecursion > 1)
        pEntry->cRecursion--;
    else
        rtLockValidatorRecSharedRemoveAndFreeOwner(pRec, pEntry, iEntry);
}
RT_EXPORT_SYMBOL(RTLockValidatorSharedRecRemoveOwner);


RTDECL(int) RTLockValidatorRecSharedCheckAndRelease(PRTLOCKVALRECSHRD pRead, RTTHREAD hThreadSelf)
{
    AssertReturn(pRead->Core.u32Magic == RTLOCKVALRECSHRD_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRead->fEnabled)
        return VINF_SUCCESS;
    if (hThreadSelf == NIL_RTTHREAD)
    {
        hThreadSelf = RTThreadSelfAutoAdopt();
        AssertReturn(hThreadSelf != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    }
    Assert(hThreadSelf == RTThreadSelf());

    /*
     * Locate the entry for this thread in the table.
     */
    uint32_t                iEntry = 0;
    PRTLOCKVALRECSHRDOWN    pEntry = rtLockValidatorRecSharedFindOwner(pRead, hThreadSelf, &iEntry);
    AssertReturn(pEntry, VERR_SEM_LV_NOT_OWNER);

    /*
     * Check the release order.
     */
    if (pRead->hClass != NIL_RTLOCKVALIDATORCLASS)
    {
        /** @todo order validation */
    }

    /*
     * Release the ownership or unwind a level of recursion.
     */
    Assert(pEntry->cRecursion > 0);
    if (pEntry->cRecursion > 1)
        pEntry->cRecursion--;
    else
        rtLockValidatorRecSharedRemoveAndFreeOwner(pRead, pEntry, iEntry);

    return VINF_SUCCESS;
}


RTDECL(int32_t) RTLockValidatorWriteLockGetCount(RTTHREAD Thread)
{
    if (Thread == NIL_RTTHREAD)
        return 0;

    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (!pThread)
        return VERR_INVALID_HANDLE;
    int32_t cWriteLocks = ASMAtomicReadS32(&pThread->LockValidator.cWriteLocks);
    rtThreadRelease(pThread);
    return cWriteLocks;
}
RT_EXPORT_SYMBOL(RTLockValidatorWriteLockGetCount);


RTDECL(void) RTLockValidatorWriteLockInc(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    AssertReturnVoid(pThread);
    ASMAtomicIncS32(&pThread->LockValidator.cWriteLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorWriteLockInc);


RTDECL(void) RTLockValidatorWriteLockDec(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    AssertReturnVoid(pThread);
    ASMAtomicDecS32(&pThread->LockValidator.cWriteLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorWriteLockDec);


RTDECL(int32_t) RTLockValidatorReadLockGetCount(RTTHREAD Thread)
{
    if (Thread == NIL_RTTHREAD)
        return 0;

    PRTTHREADINT pThread = rtThreadGet(Thread);
    if (!pThread)
        return VERR_INVALID_HANDLE;
    int32_t cReadLocks = ASMAtomicReadS32(&pThread->LockValidator.cReadLocks);
    rtThreadRelease(pThread);
    return cReadLocks;
}
RT_EXPORT_SYMBOL(RTLockValidatorReadLockGetCount);


RTDECL(void) RTLockValidatorReadLockInc(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    Assert(pThread);
    ASMAtomicIncS32(&pThread->LockValidator.cReadLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorReadLockInc);


RTDECL(void) RTLockValidatorReadLockDec(RTTHREAD Thread)
{
    PRTTHREADINT pThread = rtThreadGet(Thread);
    Assert(pThread);
    ASMAtomicDecS32(&pThread->LockValidator.cReadLocks);
    rtThreadRelease(pThread);
}
RT_EXPORT_SYMBOL(RTLockValidatorReadLockDec);


RTDECL(bool) RTLockValidatorSetEnabled(bool fEnabled)
{
    return ASMAtomicXchgBool(&g_fLockValidatorEnabled, fEnabled);
}
RT_EXPORT_SYMBOL(RTLockValidatorSetEnabled);


RTDECL(bool) RTLockValidatorIsEnabled(void)
{
    return ASMAtomicUoReadBool(&g_fLockValidatorEnabled);
}
RT_EXPORT_SYMBOL(RTLockValidatorIsEnabled);


RTDECL(bool) RTLockValidatorSetQuiet(bool fQuiet)
{
    return ASMAtomicXchgBool(&g_fLockValidatorQuiet, fQuiet);
}
RT_EXPORT_SYMBOL(RTLockValidatorSetQuiet);


RTDECL(bool) RTLockValidatorAreQuiet(void)
{
    return ASMAtomicUoReadBool(&g_fLockValidatorQuiet);
}
RT_EXPORT_SYMBOL(RTLockValidatorAreQuiet);


RTDECL(bool) RTLockValidatorSetMayPanic(bool fMayPanic)
{
    return ASMAtomicXchgBool(&g_fLockValidatorMayPanic, fMayPanic);
}
RT_EXPORT_SYMBOL(RTLockValidatorSetMayPanic);


RTDECL(bool) RTLockValidatorMayPanic(void)
{
    return ASMAtomicUoReadBool(&g_fLockValidatorMayPanic);
}
RT_EXPORT_SYMBOL(RTLockValidatorMayPanic);

