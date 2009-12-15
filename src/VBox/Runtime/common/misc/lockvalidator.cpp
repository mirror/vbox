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



RTDECL(void) RTLockValidatorInit(PRTLOCKVALIDATORREC pRec, RTLOCKVALIDATORCLASS hClass,
                                 uint32_t uSubClass, const char *pszName, void *hLock)
{
    pRec->u32Magic      = RTLOCKVALIDATORREC_MAGIC;
    pRec->uLine         = 0;
    pRec->pszFile       = NULL;
    pRec->pszFunction   = NULL;
    pRec->uId           = 0;
    pRec->hThread       = NIL_RTTHREAD;
    pRec->pDown         = NULL;
    pRec->hClass        = hClass;
    pRec->uSubClass     = uSubClass;
    pRec->cRecursion    = 0;
    pRec->hLock         = hLock;
    pRec->pszName       = pszName;
}


RTDECL(int)  RTLockValidatorCreate(PRTLOCKVALIDATORREC *ppRec, RTLOCKVALIDATORCLASS hClass,
                                   uint32_t uSubClass, const char *pszName, void *pvLock)
{
    PRTLOCKVALIDATORREC pRec;
    *ppRec = pRec = (PRTLOCKVALIDATORREC)RTMemAlloc(sizeof(*pRec));
    if (!pRec)
        return VERR_NO_MEMORY;

    RTLockValidatorInit(pRec, hClass, uSubClass, pszName, pvLock);

    return VINF_SUCCESS;
}


RTDECL(void) RTLockValidatorDelete(PRTLOCKVALIDATORREC pRec)
{
    Assert(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);

    ASMAtomicWriteU32(&pRec->u32Magic, RTLOCKVALIDATORREC_MAGIC_DEAD);
    ASMAtomicWriteHandle(&pRec->hThread, NIL_RTTHREAD);
    ASMAtomicWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);
}


RTDECL(void) RTLockValidatorDestroy(PRTLOCKVALIDATORREC *ppRec)
{
    PRTLOCKVALIDATORREC pRec = *ppRec;
    *ppRec = NULL;
    if (pRec)
    {
        RTLockValidatorDelete(pRec);
        RTMemFree(pRec);
    }
}


RTDECL(int) RTLockValidatorCheckOrder(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_INVALID_MAGIC);

    /*
     * Check it locks we're currently holding.
     */
    /** @todo later */

    /*
     * If missing order rules, add them.
     */

    return VINF_SUCCESS;
}


RTDECL(RTTHREAD) RTLockValidatorSetOwner(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, NIL_RTTHREAD);

    if (hThread == NIL_RTTHREAD)
        hThread = RTThreadSelfAutoAdopt();
    if (pRec->hThread == hThread)
        pRec->cRecursion++;
    else
    {
        Assert(pRec->hThread == NIL_RTTHREAD);

        /*
         * Update the record.
         */
        ASMAtomicUoWriteU32(&pRec->uLine, iLine);
        ASMAtomicUoWritePtr((void * volatile *)&pRec->pszFile,      pszFile);
        ASMAtomicUoWritePtr((void * volatile *)&pRec->pszFunction,  pszFunction);
        ASMAtomicUoWritePtr((void * volatile *)&pRec->uId,          (void *)uId);
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
    RTTHREAD hThread = pRec->hThread;
    AssertReturn(hThread != NIL_RTTHREAD, hThread);
    AssertReturn(pRec->hThread == hThread, hThread);

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

    return hThread;
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
 * @param   RT_SRC_POS_DECL Where we are going to deadlock.
 * @param   uId             Where we are going to deadlock.
 */
static void rtLockValidatorComplainAboutDeadlock(PRTLOCKVALIDATORREC pRec, PRTTHREADINT pThread, RTTHREADSTATE enmState,
                                                 PRTTHREADINT pCur, RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    AssertMsg1(pCur == pThread ? "!!Deadlock detected!!" : "!!Deadlock exists!!", iLine, pszFile, pszFunction);

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
                   pCur->LockValidator.pszBlockFile, pCur->LockValidator.uBlockLine,
                   pCur->LockValidator.pszBlockFunction, pCur->LockValidator.uBlockId);
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
                               pCurRec->pszFile, pCurRec->uLine, pCurRec->pszFunction, pCurRec->uId);
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


/**
 * Change the thread state to blocking and do deadlock detection.
 *
 * @param   pRec                The validator record we're blocing on.
 * @param   hThread             The current thread.  Shall not be NIL_RTTHREAD!
 * @param   enmState            The sleep state.
 * @param   pvBlock             Pointer to a RTLOCKVALIDATORREC structure.
 * @param   fRecursiveOk        Whether it's ok to recurse.
 * @param   uId                 Where we are blocking.
 * @param   RT_SRC_POS_DECL     Where we are blocking.
 */
RTDECL(void) RTLockValidatorCheckBlocking(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread,
                                          RTTHREADSTATE enmState, bool fRecursiveOk,
                                          RTHCUINTPTR uId, RT_SRC_POS_DECL)
{
    /*
     * Fend off wild life.
     */
    AssertReturnVoid(RTTHREAD_IS_SLEEPING(enmState));
    AssertPtrReturnVoid(pRec);
    AssertReturnVoid(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC);
    PRTTHREADINT pThread = hThread;
    AssertPtrReturnVoid(pThread);
    AssertReturnVoid(pThread->u32Magic == RTTHREADINT_MAGIC);
    RTTHREADSTATE enmThreadState = rtThreadGetState(pThread);
    AssertReturnVoid(   enmThreadState == RTTHREADSTATE_RUNNING
                     || enmThreadState == RTTHREADSTATE_TERMINATED /* rtThreadRemove uses locks too */);

    /*
     * Record the location and everything before changing the state and
     * performing deadlock detection.
     */
    /** @todo This has to be serialized! The deadlock detection isn't 100% safe!!! */
    pThread->LockValidator.pRec             = pRec;
    pThread->LockValidator.pszBlockFunction = pszFunction;
    pThread->LockValidator.pszBlockFile     = pszFile;
    pThread->LockValidator.uBlockLine       = iLine;
    pThread->LockValidator.uBlockId         = uId;

    RTThreadBlocking(hThread, enmState);

    /*
     * Don't do deadlock detection if we're recursing and that's OK.
     *
     * On some hosts we don't do recursion accounting our selves and there
     * isn't any other place to check for this.  semmutex-win.cpp for instance.
     */
    if (    !fRecursiveOk
        ||  pRec->hThread != pThread)
    {
        /*
         * Do deadlock detection.
         *
         * Since we're missing proper serialization, we don't declare it a
         * deadlock until we've got three runs with the same list length.
         * While this isn't perfect, it should avoid out the most obvious
         * races on SMP boxes.
         */
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
                    return;

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
        rtLockValidatorComplainAboutDeadlock(pRec, pThread, enmState, pCur, uId, RT_SRC_POS_ARGS);
    }
}
RT_EXPORT_SYMBOL(RTThreadBlocking);

