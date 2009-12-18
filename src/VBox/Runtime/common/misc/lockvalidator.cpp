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
#include <iprt/thread.h>

#include "internal/lockvalidator.h"
#include "internal/magics.h"
#include "internal/thread.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Serializing object destruction and deadlock detection.
 * NS: RTLOCKVALIDATORREC and RTTHREADINT destruction.
 * EW: Deadlock detection.
 */
static RTSEMXROADS g_hLockValidatorXRoads = NIL_RTSEMXROADS;


/**
 * Copy a source position record.
 *
 * @param   pDst                The destination.
 * @param   pSrc                The source.
 */
DECL_FORCE_INLINE(void) rtLockValidatorCopySrcPos(PRTLOCKVALIDATORSRCPOS pDst, PCRTLOCKVALIDATORSRCPOS pSrc)
{
    ASMAtomicUoWriteU32(&pDst->uLine,                           pSrc->uLine);
    ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFile,      pSrc->pszFile);
    ASMAtomicUoWritePtr((void * volatile *)&pDst->pszFunction,  pSrc->pszFunction);
    ASMAtomicUoWritePtr((void * volatile *)&pDst->uId,          (void *)pSrc->uId);
}


/**
 * Init a source position record.
 *
 * @param   pSrcPos             The source position record.
 */
DECL_FORCE_INLINE(void) rtLockValidatorInitSrcPos(PRTLOCKVALIDATORSRCPOS pSrcPos)
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
 * Serializes destruction of RTLOCKVALIDATORREC and RTTHREADINT structures.
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


RTDECL(void) RTLockValidatorRecInit(PRTLOCKVALIDATORREC pRec, RTLOCKVALIDATORCLASS hClass,
                                    uint32_t uSubClass, const char *pszName, void *hLock)
{
    pRec->u32Magic      = RTLOCKVALIDATORREC_MAGIC;
    rtLockValidatorInitSrcPos(&pRec->SrcPos);
    pRec->hThread       = NIL_RTTHREAD;
    pRec->pDown         = NULL;
    pRec->hClass        = hClass;
    pRec->uSubClass     = uSubClass;
    pRec->cRecursion    = 0;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;

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


RTDECL(int)  RTLockValidatorRecCreate(PRTLOCKVALIDATORREC *ppRec, RTLOCKVALIDATORCLASS hClass,
                                      uint32_t uSubClass, const char *pszName, void *pvLock)
{
    PRTLOCKVALIDATORREC pRec;
    *ppRec = pRec = (PRTLOCKVALIDATORREC)RTMemAlloc(sizeof(*pRec));
    if (!pRec)
        return VERR_NO_MEMORY;

    RTLockValidatorRecInit(pRec, hClass, uSubClass, pszName, pvLock);

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecDelete(PRTLOCKVALIDATORREC pRec)
{
    Assert(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);

    rtLockValidatorSerializeDestructEnter();

    ASMAtomicWriteU32(&pRec->u32Magic, RTLOCKVALIDATORREC_MAGIC_DEAD);
    ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    ASMAtomicWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);

    rtLockValidatorSerializeDestructLeave();
}


RTDECL(void) RTLockValidatorRecDestroy(PRTLOCKVALIDATORREC *ppRec)
{
    PRTLOCKVALIDATORREC pRec = *ppRec;
    *ppRec = NULL;
    if (pRec)
    {
        RTLockValidatorRecDelete(pRec);
        RTMemFree(pRec);
    }
}


RTDECL(void) RTLockValidatorSharedRecInit(PRTLOCKVALIDATORSHARED pRec, RTLOCKVALIDATORCLASS hClass,
                                          uint32_t uSubClass, const char *pszName, void *hLock)
{
    pRec->u32Magic      = RTLOCKVALIDATORSHARED_MAGIC;
    pRec->uSubClass     = uSubClass;
    pRec->hClass        = hClass;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;

    /* the table */
    pRec->cEntries      = 0;
    pRec->iLastEntry    = 0;
    pRec->cAllocated    = 0;
    pRec->fReallocating = false;
    pRec->afPadding[0]  = false;
    pRec->afPadding[1]  = false;
    pRec->afPadding[2]  = false;
    pRec->papOwners     = NULL;
    pRec->u64Alignment  = UINT64_MAX;
}


RTDECL(void) RTLockValidatorSharedRecDelete(PRTLOCKVALIDATORSHARED pRec)
{
    Assert(pRec->u32Magic == RTLOCKVALIDATORSHARED_MAGIC);

    /*
     * Flip it into table realloc mode and take the destruction lock.
     */
    rtLockValidatorSerializeDestructEnter();

    if (!ASMAtomicCmpXchgBool(&pRec->fReallocating, true, false))
    {
        for (;;)
        {
            rtLockValidatorSerializeDestructEnter();
            rtLockValidatorSerializeDetectionEnter();
            rtLockValidatorSerializeDetectionLeave();
            rtLockValidatorSerializeDestructEnter();
            if (ASMAtomicCmpXchgBool(&pRec->fReallocating, true, false))
                break;
        }
    }

    ASMAtomicWriteU32(&pRec->u32Magic, RTLOCKVALIDATORSHARED_MAGIC_DEAD);
    ASMAtomicUoWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);
    if (pRec->papOwners)
    {
        PRTLOCKVALIDATORSHAREDONE volatile *papOwners = pRec->papOwners;
        ASMAtomicUoWritePtr((void * volatile *)&pRec->papOwners, NULL);
        ASMAtomicUoWriteU32(&pRec->cAllocated, 0);

        RTMemFree((void *)pRec->papOwners);
    }
    ASMAtomicWriteBool(&pRec->fReallocating, false);

    rtLockValidatorSerializeDestructLeave();
}


RTDECL(int) RTLockValidatorCheckOrder(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(int)  RTLockValidatorCheckReleaseOrder(PRTLOCKVALIDATORREC pRec)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecordRecursion(PRTLOCKVALIDATORREC pRec, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion < _1M);
    pRec->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorRecordUnwind(PRTLOCKVALIDATORREC pRec)
{
    AssertReturnVoid(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);
    AssertReturnVoid(pRec->hThread != NIL_RTTHREAD);
    AssertReturnVoid(pRec->cRecursion > 0);

    pRec->cRecursion--;
}


RTDECL(RTTHREAD) RTLockValidatorSetOwner(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, NIL_RTTHREAD);

    if (hThread == NIL_RTTHREAD)
    {
        hThread = RTThreadSelfAutoAdopt();
        AssertReturn(hThread != NIL_RTTHREAD, hThread);
    }

    ASMAtomicIncS32(&hThread->LockValidator.cWriteLocks);

    if (pRec->hThread == hThread)
        pRec->cRecursion++;
    else
    {
        Assert(pRec->hThread == NIL_RTTHREAD);

        /*
         * Update the record.
         */
        rtLockValidatorCopySrcPos(&pRec->SrcPos, pSrcPos);
        ASMAtomicUoWriteU32(&pRec->cRecursion, 1);
        ASMAtomicWriteHandle(&pRec->hThread, hThread);

        /*
         * Push the lock onto the lock stack.
         */
        /** @todo push it onto the per-thread lock stack. */
    }

    return hThread;
}


RTDECL(RTTHREAD) RTLockValidatorUnsetOwner(PRTLOCKVALIDATORREC pRec)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, NIL_RTTHREAD);
    RTTHREADINT *pThread = pRec->hThread;
    AssertReturn(pThread != NIL_RTTHREAD, pThread);

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

    return pThread;
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



/**
 * Bitch about a deadlock.
 *
 * @param   pRec            The lock validator record we're going to block on.
 * @param   pThread         This thread.
 * @param   pCur            The thread we're deadlocking with.
 * @param   enmState        The sleep state.
 * @param   pSrcPos         Where we are going to deadlock.
 */
static void rtLockValidatorComplainAboutDeadlock(PRTLOCKVALIDATORREC pRec, PRTTHREADINT pThread, RTTHREADSTATE enmState,
                                                 PRTTHREADINT pCur, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertMsg1(pCur == pThread ? "!!Deadlock detected!!" : "!!Deadlock exists!!", pSrcPos->uLine, pSrcPos->pszFile, pSrcPos->pszFunction);

    /*
     * Print the threads and locks involved.
     */
    PRTTHREADINT    apSeenThreads[8] = {0,0,0,0,0,0,0,0};
    unsigned        iSeenThread = 0;
    pCur = pThread;
    for (unsigned iEntry = 0; pCur && iEntry < 256; iEntry++)
    {
        /*
         * Print info on pCur. Determin next while doing so.
         */
        AssertMsg2(" #%u: %RTthrd/%RTnthrd %s: %s(%u) %RTptr\n",
                   iEntry, pCur, pCur->Core.Key, pCur->szName,
                   pCur->LockValidator.SrcPos.pszFile, pCur->LockValidator.SrcPos.uLine,
                   pCur->LockValidator.SrcPos.pszFunction, pCur->LockValidator.SrcPos.uId);
        PRTTHREADINT  pNext       = NULL;
        RTTHREADSTATE enmCurState = rtThreadGetState(pCur);
        switch (enmCurState)
        {
            case RTTHREADSTATE_CRITSECT:
            case RTTHREADSTATE_EVENT:
            case RTTHREADSTATE_EVENT_MULTI:
            case RTTHREADSTATE_FAST_MUTEX:
            case RTTHREADSTATE_MUTEX:
            case RTTHREADSTATE_RW_READ:
            case RTTHREADSTATE_RW_WRITE:
            case RTTHREADSTATE_SPIN_MUTEX:
            {
                PRTLOCKVALIDATORREC pCurRec      = pCur->LockValidator.pRec;
                RTTHREADSTATE       enmCurState2 = rtThreadGetState(pCur);
                if (enmCurState2 != enmCurState)
                {
                    AssertMsg2(" Impossible!!! enmState=%s -> %s (%d)\n",
                               RTThreadStateName(enmCurState), RTThreadStateName(enmCurState2), enmCurState2);
                    break;
                }
                if (   VALID_PTR(pCurRec)
                    && pCurRec->u32Magic == RTLOCKVALIDATORREC_MAGIC)
                {
                    AssertMsg2("     Waiting on %s %p [%s]: Entered %s(%u) %s %p\n",
                               RTThreadStateName(enmCurState), pCurRec->hLock, pCurRec->pszName,
                               pCurRec->SrcPos.pszFile, pCurRec->SrcPos.uLine, pCurRec->SrcPos.pszFunction, pCurRec->SrcPos.uId);
                    pNext = pCurRec->hThread;
                }
                else if (VALID_PTR(pCurRec))
                    AssertMsg2("     Waiting on %s pCurRec=%p: invalid magic number: %#x\n",
                               RTThreadStateName(enmCurState), pCurRec, pCurRec->u32Magic);
                else
                    AssertMsg2("     Waiting on %s pCurRec=%p: invalid pointer\n",
                               RTThreadStateName(enmCurState), pCurRec);
                break;
            }

            default:
                AssertMsg2(" Impossible!!! enmState=%s (%d)\n", RTThreadStateName(enmCurState), enmCurState);
                break;
        }

        /*
         * Check for cycle.
         */
        if (iEntry && pCur == pThread)
            break;
        for (unsigned i = 0; i < RT_ELEMENTS(apSeenThreads); i++)
            if (apSeenThreads[i] == pCur)
            {
                AssertMsg2(" Cycle!\n");
                pNext = NULL;
                break;
            }

        /*
         * Advance to the next thread.
         */
        iSeenThread = (iSeenThread + 1) % RT_ELEMENTS(apSeenThreads);
        apSeenThreads[iSeenThread] = pCur;
        pCur = pNext;
    }
    AssertBreakpoint();
}


RTDECL(int) RTLockValidatorCheckBlocking(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread,
                                         RTTHREADSTATE enmState, bool fRecursiveOk,
                                         PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    /*
     * Fend off wild life.
     */
    AssertReturn(RTTHREAD_IS_SLEEPING(enmState), VERR_SEM_LV_INVALID_PARAMETER);
    AssertPtrReturn(pRec, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    PRTTHREADINT pThread = hThread;
    AssertPtrReturn(pThread, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThread->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    RTTHREADSTATE enmThreadState = rtThreadGetState(pThread);
    AssertReturn(   enmThreadState == RTTHREADSTATE_RUNNING
                 || enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                 || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Record the location and everything before changing the state and
     * performing deadlock detection.
     */
    pThread->LockValidator.pRec = pRec;
    rtLockValidatorCopySrcPos(&pThread->LockValidator.SrcPos, pSrcPos);

    /*
     * Don't do deadlock detection if we're recursing and that's OK.
     *
     * On some hosts we don't do recursion accounting our selves and there
     * isn't any other place to check for this.  semmutex-win.cpp for instance.
     */
    if (pRec->hThread == pThread)
    {
        if (fRecursiveOk)
            return VINF_SUCCESS;
        AssertMsgFailed(("%p (%s)\n", pRec->hLock, pRec->pszName));
        return VERR_SEM_LV_NESTED;
    }

    /*
     * Do deadlock detection.
     *
     * Since we're missing proper serialization, we don't declare it a
     * deadlock until we've got three runs with the same list length.
     * While this isn't perfect, it should avoid out the most obvious
     * races on SMP boxes.
     */
    rtLockValidatorSerializeDetectionEnter();

    PRTTHREADINT    pCur;
    unsigned        cPrevLength = ~0U;
    unsigned        cEqualRuns  = 0;
    unsigned        iParanoia   = 256;
    do
    {
        unsigned cLength = 0;
        pCur = pThread;
        for (;;)
        {
            /*
             * Get the next thread.
             */
            PRTTHREADINT pNext = NULL;
            for (;;)
            {
                RTTHREADSTATE enmCurState = rtThreadGetState(pCur);
                switch (enmCurState)
                {
                    case RTTHREADSTATE_CRITSECT:
                    case RTTHREADSTATE_EVENT:
                    case RTTHREADSTATE_EVENT_MULTI:
                    case RTTHREADSTATE_FAST_MUTEX:
                    case RTTHREADSTATE_MUTEX:
                    case RTTHREADSTATE_RW_READ:
                    case RTTHREADSTATE_RW_WRITE:
                    case RTTHREADSTATE_SPIN_MUTEX:
                    {
                        PRTLOCKVALIDATORREC pCurRec = pCur->LockValidator.pRec;
                        if (    rtThreadGetState(pCur) != enmCurState
                            ||  !VALID_PTR(pCurRec)
                            ||  pCurRec->u32Magic != RTLOCKVALIDATORREC_MAGIC)
                            continue;
                        pNext = pCurRec->hThread;
                        if (    rtThreadGetState(pCur) != enmCurState
                            ||  pCurRec->u32Magic != RTLOCKVALIDATORREC_MAGIC
                            ||  pCurRec->hThread != pNext)
                            continue;
                        break;
                    }

                    default:
                        pNext = NULL;
                        break;
                }
                break;
            }

            /*
             * If we arrive at the end of the list we're good.
             */
            pCur = pNext;
            if (!pCur)
            {
                rtLockValidatorSerializeDetectionLeave();
                return VINF_SUCCESS;
            }

            /*
             * If we've got back to the blocking thread id we've
             * got a deadlock.
             */
            if (pCur == pThread)
                break;

            /*
             * If we've got a chain of more than 256 items, there is some
             * kind of cycle in the list, which means that there is already
             * a deadlock somewhere.
             */
            if (cLength >= 256)
                break;

            cLength++;
        }

        /* compare with previous list run. */
        if (cLength != cPrevLength)
        {
            cPrevLength = cLength;
            cEqualRuns = 0;
        }
        else
            cEqualRuns++;
    } while (cEqualRuns < 3 && --iParanoia > 0);

    /*
     * Ok, if we ever get here, it's most likely a genuine deadlock.
     */
    rtLockValidatorComplainAboutDeadlock(pRec, pThread, enmState, pCur, pSrcPos);

    rtLockValidatorSerializeDetectionLeave();

    return VERR_SEM_LV_DEADLOCK;
}
RT_EXPORT_SYMBOL(RTThreadBlocking);

