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
/** Whether the lock validator is enabled or disabled.
 * Only applies to new locks.  */
static bool volatile g_fLockValidatorEnabled = true;


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
    if (pRec->pSibling)
    {
        /* ASSUMES sibling destruction doesn't involve any races.  */
        ASMAtomicUoWritePtr((void * volatile *)&pRec->pSibling->pSibling, NULL);
        ASMAtomicUoWritePtr((void * volatile *)&pRec->pSibling, NULL);
    }

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


RTDECL(void) RTLockValidatorSharedRecDelete(PRTLOCKVALIDATORSHARED pRec)
{
    Assert(pRec->u32Magic == RTLOCKVALIDATORSHARED_MAGIC);

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

    ASMAtomicWriteU32(&pRec->u32Magic, RTLOCKVALIDATORSHARED_MAGIC_DEAD);
    ASMAtomicUoWriteHandle(&pRec->hClass, NIL_RTLOCKVALIDATORCLASS);
    if (pRec->papOwners)
    {
        PRTLOCKVALIDATORSHAREDONE volatile *papOwners = pRec->papOwners;
        ASMAtomicUoWritePtr((void * volatile *)&pRec->papOwners, NULL);
        ASMAtomicUoWriteU32(&pRec->cAllocated, 0);

        RTMemFree((void *)pRec->papOwners);
    }
    if (pRec->pSibling)
    {
        /* ASSUMES sibling destruction doesn't involve any races.  */
        ASMAtomicUoWritePtr((void * volatile *)&pRec->pSibling->pSibling, NULL);
        ASMAtomicUoWritePtr((void * volatile *)&pRec->pSibling, NULL);
    }
    ASMAtomicWriteBool(&pRec->fReallocating, false);

    rtLockValidatorSerializeDestructLeave();
}


/**
 * Locates a thread in a shared lock record.
 *
 * @returns Pointer to the thread record on success, NULL on failure..
 * @param   pShared             The shared lock record.
 * @param   hThread             The thread to find.
 * @param   piEntry             Where to optionally return the table in index.
 */
DECLINLINE(PRTLOCKVALIDATORSHAREDONE)
rtLockValidatorSharedRecFindThread(PRTLOCKVALIDATORSHARED pShared, RTTHREAD hThread, uint32_t *piEntry)
{
    rtLockValidatorSerializeDetectionEnter();
    if (pShared->papOwners)
    {
        PRTLOCKVALIDATORSHAREDONE volatile *papOwners = pShared->papOwners;
        uint32_t const                      cMax      = pShared->cAllocated;
        for (uint32_t iEntry = 0; iEntry < cMax; iEntry++)
        {
            PRTLOCKVALIDATORSHAREDONE pEntry;
            pEntry = (PRTLOCKVALIDATORSHAREDONE)ASMAtomicUoReadPtr((void * volatile *)&papOwners[iEntry]);
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


/**
 * Allocates and initializes a thread entry for the shared lock record.
 *
 * @returns The new thread entry.
 * @param   pShared             The shared lock record.
 * @param   hThread             The thread handle.
 * @param   pSrcPos             The source position.
 */
DECLINLINE(PRTLOCKVALIDATORSHAREDONE)
rtLockValidatorSharedRecAllocThread(PRTLOCKVALIDATORSHARED pRead, RTTHREAD hThread, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    PRTLOCKVALIDATORSHAREDONE pEntry;

    pEntry = (PRTLOCKVALIDATORSHAREDONE)RTMemAlloc(sizeof(RTLOCKVALIDATORSHAREDONE));
    if (pEntry)
    {
        pEntry->u32Magic        = RTLOCKVALIDATORSHAREDONE_MAGIC;
        pEntry->cRecursion      = 1;
        pEntry->hThread         = hThread;
        pEntry->pDown           = NULL;
        pEntry->pSharedRec      = pRead;
#if HC_ARCH_BITS == 32
        pEntry->pvReserved      = NULL;
#endif
        if (pSrcPos)
            pEntry->SrcPos      = *pSrcPos;
        else
            rtLockValidatorInitSrcPos(&pEntry->SrcPos);
    }

    return pEntry;
}

/**
 * Frees a thread entry allocated by rtLockValidatorSharedRecAllocThread.
 *
 * @param   pEntry              The thread entry.
 */
DECLINLINE(void) rtLockValidatorSharedRecFreeThread(PRTLOCKVALIDATORSHAREDONE pEntry)
{
    if (pEntry)
    {
        rtLockValidatorSerializeDestructEnter();
        ASMAtomicWriteU32(&pEntry->u32Magic, RTLOCKVALIDATORSHAREDONE_MAGIC_DEAD);
        ASMAtomicWriteHandle(&pEntry->hThread, NIL_RTTHREAD);
        rtLockValidatorSerializeDestructLeave();

        RTMemFree(pEntry);
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
static bool rtLockValidatorSharedRecMakeRoom(PRTLOCKVALIDATORSHARED pShared)
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
        if (    pShared->u32Magic == RTLOCKVALIDATORSHARED_MAGIC
            &&  ASMAtomicCmpXchgBool(&pShared->fReallocating, true, false))
        {
            uint32_t cAllocated = pShared->cAllocated;
            if (cAllocated < pShared->cEntries)
            {
                /*
                 * Ok, still not enough space.  Reallocate the table.
                 */
#if 0  /** @todo enable this after making sure growing works flawlessly. */
                uint32_t                    cInc = RT_ALIGN_32(pShared->cEntries - cAllocated, 16);
#else
                uint32_t                    cInc = RT_ALIGN_32(pShared->cEntries - cAllocated, 1);
#endif
                PRTLOCKVALIDATORSHAREDONE  *papOwners;
                papOwners = (PRTLOCKVALIDATORSHAREDONE *)RTMemRealloc((void *)pShared->papOwners,
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
        if (RT_UNLIKELY(pShared->u32Magic != RTLOCKVALIDATORSHARED_MAGIC))
            break;

        if (pShared->cAllocated >= pShared->cEntries)
            return true;
    }

    rtLockValidatorSerializeDetectionLeave();
    AssertFailed(); /* too many iterations or destroyed while racing. */
    return false;
}


/**
 * Adds a thread entry to a shared lock record.
 *
 * @returns true on success, false on serious race or we're if out of memory.
 * @param   pShared             The shared lock record.
 * @param   pEntry              The thread entry.
 */
DECLINLINE(bool) rtLockValidatorSharedRecAddThread(PRTLOCKVALIDATORSHARED pShared, PRTLOCKVALIDATORSHAREDONE pEntry)
{
    rtLockValidatorSerializeDetectionEnter();
    if (RT_LIKELY(pShared->u32Magic == RTLOCKVALIDATORSHARED_MAGIC)) /* paranoia */
    {
        if (   ASMAtomicIncU32(&pShared->cEntries) > pShared->cAllocated /** @todo add fudge */
            && !rtLockValidatorSharedRecMakeRoom(pShared))
            return false; /* the worker leave the lock */

        PRTLOCKVALIDATORSHAREDONE volatile *papOwners = pShared->papOwners;
        uint32_t const                      cMax      = pShared->cAllocated;
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
 * Remove a thread entry from a shared lock record and free it.
 *
 * @param   pShared             The shared lock record.
 * @param   pEntry              The thread entry to remove.
 * @param   iEntry              The last known index.
 */
DECLINLINE(void) rtLockValidatorSharedRecRemoveAndFree(PRTLOCKVALIDATORSHARED pShared, PRTLOCKVALIDATORSHAREDONE pEntry,
                                                       uint32_t iEntry)
{
    /*
     * Remove it from the table.
     */
    rtLockValidatorSerializeDetectionEnter();
    AssertReturnVoidStmt(pShared->u32Magic == RTLOCKVALIDATORSHARED_MAGIC, rtLockValidatorSerializeDetectionLeave());
    if (RT_UNLIKELY(   iEntry >= pShared->cAllocated
                    || !ASMAtomicCmpXchgPtr((void * volatile *)&pShared->papOwners[iEntry], NULL, pEntry)))
    {
        /* this shouldn't happen yet... */
        AssertFailed();
        PRTLOCKVALIDATORSHAREDONE volatile *papOwners = pShared->papOwners;
        uint32_t const                      cMax      = pShared->cAllocated;
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
    rtLockValidatorSharedRecFreeThread(pEntry);
}


RTDECL(int) RTLockValidatorMakeSiblings(void *pvRec1, void *pvRec2)
{
    /*
     * Validate input.
     */
    union
    {
        PRTLOCKVALIDATORREC     pRec;
        PRTLOCKVALIDATORSHARED  pShared;
        uint32_t               *pu32Magic;
        void                   *pv;
    } u1, u2;
    u1.pv = pvRec1;
    u2.pv = pvRec2;

    AssertPtrReturn(u1.pv, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(   *u1.pu32Magic == RTLOCKVALIDATORREC_MAGIC
                 || *u1.pu32Magic == RTLOCKVALIDATORSHARED_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);

    AssertPtrReturn(u2.pv, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(   *u2.pu32Magic == RTLOCKVALIDATORREC_MAGIC
                 || *u2.pu32Magic == RTLOCKVALIDATORSHARED_MAGIC
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Link them.
     */
    if (    *u1.pu32Magic == RTLOCKVALIDATORREC_MAGIC
        &&  *u2.pu32Magic == RTLOCKVALIDATORSHARED_MAGIC)
    {
        u1.pRec->pSibling    = u2.pShared;
        u2.pShared->pSibling = u1.pRec;
    }
    else if (    *u1.pu32Magic == RTLOCKVALIDATORSHARED_MAGIC
             &&  *u2.pu32Magic == RTLOCKVALIDATORREC_MAGIC)
    {
        u1.pShared->pSibling = u2.pRec;
        u2.pRec->pSibling    = u1.pShared;
    }
    else
        AssertFailedReturn(VERR_SEM_LV_INVALID_PARAMETER); /* unsupported mix */

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorCheckOrder(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
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


RTDECL(int)  RTLockValidatorCheckAndRelease(PRTLOCKVALIDATORREC pRec)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    RTLockValidatorUnsetOwner(pRec);
    return VINF_SUCCESS;
}


RTDECL(int)  RTLockValidatorCheckAndReleaseReadOwner(PRTLOCKVALIDATORSHARED pRead, RTTHREAD hThread)
{
    AssertReturn(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRead->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Locate the entry for this thread in the table.
     */
    uint32_t                    iEntry = 0;
    PRTLOCKVALIDATORSHAREDONE   pEntry = rtLockValidatorSharedRecFindThread(pRead, hThread, &iEntry);
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
        rtLockValidatorSharedRecRemoveAndFree(pRead, pEntry, iEntry);

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecordRecursion(PRTLOCKVALIDATORREC pRec, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion < _1M);
    pRec->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorUnwindRecursion(PRTLOCKVALIDATORREC pRec)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pRec->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pRec->cRecursion);
    pRec->cRecursion--;
    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorRecordReadWriteRecursion(PRTLOCKVALIDATORREC pWrite, PRTLOCKVALIDATORSHARED pRead, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pWrite->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->fEnabled == pWrite->fEnabled, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pWrite->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pWrite->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pWrite->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pWrite->cRecursion < _1M);
    pWrite->cRecursion++;

    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorUnwindReadWriteRecursion(PRTLOCKVALIDATORREC pWrite, PRTLOCKVALIDATORSHARED pRead)
{
    AssertReturn(pWrite->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->fEnabled == pWrite->fEnabled, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pWrite->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(pWrite->hThread != NIL_RTTHREAD, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pWrite->cRecursion > 0, VERR_SEM_LV_INVALID_PARAMETER);

    Assert(pWrite->cRecursion);
    pWrite->cRecursion--;
    return VINF_SUCCESS;
}


RTDECL(RTTHREAD) RTLockValidatorSetOwner(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, NIL_RTTHREAD);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
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
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
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


RTDECL(void) RTLockValidatorAddReadOwner(PRTLOCKVALIDATORSHARED pRead, RTTHREAD hThread, PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    AssertReturnVoid(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC);
    if (!pRead->fEnabled)
        return;
    AssertReturnVoid(hThread != NIL_RTTHREAD);

    /*
     * Recursive?
     *
     * Note! This code can be optimized to try avoid scanning the table on
     *       insert. However, that's annoying work that makes the code big,
     *       so it can wait til later sometime.
     */
    PRTLOCKVALIDATORSHAREDONE pEntry = rtLockValidatorSharedRecFindThread(pRead, hThread, NULL);
    if (pEntry)
    {
        pEntry->cRecursion++;
        return;
    }

    /*
     * Allocate a new thread entry and insert it into the table.
     */
    pEntry = rtLockValidatorSharedRecAllocThread(pRead, hThread, pSrcPos);
    if (    pEntry
        &&  !rtLockValidatorSharedRecAddThread(pRead, pEntry))
        rtLockValidatorSharedRecFreeThread(pEntry);
}


RTDECL(void) RTLockValidatorRemoveReadOwner(PRTLOCKVALIDATORSHARED pRead, RTTHREAD hThread)
{
    AssertReturnVoid(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC);
    if (!pRead->fEnabled)
        return;
    AssertReturnVoid(hThread != NIL_RTTHREAD);

    /*
     * Find the entry hope it's a recursive one.
     */
    uint32_t iEntry;
    PRTLOCKVALIDATORSHAREDONE pEntry = rtLockValidatorSharedRecFindThread(pRead, hThread, &iEntry);
    AssertReturnVoid(pEntry);
    if (pEntry->cRecursion > 1)
        pEntry->cRecursion--;
    else
        rtLockValidatorSharedRecRemoveAndFree(pRead, pEntry, iEntry);
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
    RTAssertMsg1Weak(pCur == pThread ? "!!Deadlock detected!!" : "!!Deadlock exists!!", pSrcPos->uLine, pSrcPos->pszFile, pSrcPos->pszFunction);

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
        RTAssertMsg2Weak(" #%u: %RTthrd/%RTnthrd %s: %s(%u) %RTptr\n",
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
            case RTTHREADSTATE_SPIN_MUTEX:
            {
                PRTLOCKVALIDATORREC pCurRec      = pCur->LockValidator.pRec;
                RTTHREADSTATE       enmCurState2 = rtThreadGetState(pCur);
                if (enmCurState2 != enmCurState)
                {
                    RTAssertMsg2Weak(" Impossible!!! enmState=%s -> %s (%d)\n",
                                     RTThreadStateName(enmCurState), RTThreadStateName(enmCurState2), enmCurState2);
                    break;
                }
                if (   VALID_PTR(pCurRec)
                    && pCurRec->u32Magic == RTLOCKVALIDATORREC_MAGIC)
                {
                    RTAssertMsg2Weak("     Waiting on %s %p [%s]: Entered %s(%u) %s %p\n",
                                     RTThreadStateName(enmCurState), pCurRec->hLock, pCurRec->pszName,
                                     pCurRec->SrcPos.pszFile, pCurRec->SrcPos.uLine, pCurRec->SrcPos.pszFunction, pCurRec->SrcPos.uId);
                    pNext = pCurRec->hThread;
                }
                else if (VALID_PTR(pCurRec))
                    RTAssertMsg2Weak("     Waiting on %s pCurRec=%p: invalid magic number: %#x\n",
                                     RTThreadStateName(enmCurState), pCurRec, pCurRec->u32Magic);
                else
                    RTAssertMsg2Weak("     Waiting on %s pCurRec=%p: invalid pointer\n",
                                     RTThreadStateName(enmCurState), pCurRec);
                break;
            }

#if 0
            case RTTHREADSTATE_RW_READ:
            case RTTHREADSTATE_RW_WRITE:
            {
            }
#endif

            default:
                RTAssertMsg2Weak(" Impossible!!! enmState=%s (%d)\n", RTThreadStateName(enmCurState), enmCurState);
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
                RTAssertMsg2Weak(" Cycle!\n");
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


RTDECL(int) RTLockValidatorCheckWriteOrderBlocking(PRTLOCKVALIDATORREC pWrite, PRTLOCKVALIDATORSHARED pRead,
                                                   RTTHREAD hThread, RTTHREADSTATE enmState, bool fRecursiveOk,
                                                   PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    /*
     * Fend off wild life.
     */
    AssertPtrReturn(pWrite, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pWrite->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertPtrReturn(pRead, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->fEnabled == pWrite->fEnabled, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pWrite->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(RTTHREAD_IS_SLEEPING(enmState), VERR_SEM_LV_INVALID_PARAMETER);
    PRTTHREADINT pThread = hThread;
    AssertPtrReturn(pThread, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThread->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    RTTHREADSTATE enmThreadState = rtThreadGetState(pThread);
    AssertReturn(   enmThreadState == RTTHREADSTATE_RUNNING
                 || enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                 || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                 , VERR_SEM_LV_INVALID_PARAMETER);

    /*
     * Check for attempts at doing a read upgrade.
     */
    PRTLOCKVALIDATORSHAREDONE pEntry = rtLockValidatorSharedRecFindThread(pRead, hThread, NULL);
    if (pEntry)
    {
        AssertMsgFailed(("Read lock upgrade at %s(%d) %s %p!\nRead lock take at %s(%d) %s %p!\n",
                         pSrcPos->pszFile, pSrcPos->uLine, pSrcPos->pszFunction, pSrcPos->uId,
                         pEntry->SrcPos.pszFile, pEntry->SrcPos.uLine, pEntry->SrcPos.pszFunction, pEntry->SrcPos.uId));
        return VERR_SEM_LV_UPGRADE;
    }



    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorCheckReadOrderBlocking(PRTLOCKVALIDATORSHARED pRead, PRTLOCKVALIDATORREC pWrite,
                                                  RTTHREAD hThread, RTTHREADSTATE enmState, bool fRecursiveOk,
                                                  PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    /*
     * Fend off wild life.
     */
    AssertPtrReturn(pRead, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->u32Magic == RTLOCKVALIDATORSHARED_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertPtrReturn(pWrite, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pWrite->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRead->fEnabled == pWrite->fEnabled, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRead->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(RTTHREAD_IS_SLEEPING(enmState), VERR_SEM_LV_INVALID_PARAMETER);
    PRTTHREADINT pThread = hThread;
    AssertPtrReturn(pThread, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pThread->u32Magic == RTTHREADINT_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    RTTHREADSTATE enmThreadState = rtThreadGetState(pThread);
    AssertReturn(   enmThreadState == RTTHREADSTATE_RUNNING
                 || enmThreadState == RTTHREADSTATE_TERMINATED   /* rtThreadRemove uses locks too */
                 || enmThreadState == RTTHREADSTATE_INITIALIZING /* rtThreadInsert uses locks too */
                 , VERR_SEM_LV_INVALID_PARAMETER);
    Assert(pWrite->hThread != pThread);


    return VINF_SUCCESS;
}


RTDECL(int) RTLockValidatorCheckBlocking(PRTLOCKVALIDATORREC pRec, RTTHREAD hThread,
                                         RTTHREADSTATE enmState, bool fRecursiveOk,
                                         PCRTLOCKVALIDATORSRCPOS pSrcPos)
{
    /*
     * Fend off wild life.
     */
    AssertPtrReturn(pRec, VERR_SEM_LV_INVALID_PARAMETER);
    AssertReturn(pRec->u32Magic == RTLOCKVALIDATORREC_MAGIC, VERR_SEM_LV_INVALID_PARAMETER);
    if (!pRec->fEnabled)
        return VINF_SUCCESS;
    AssertReturn(RTTHREAD_IS_SLEEPING(enmState), VERR_SEM_LV_INVALID_PARAMETER);
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

#if 0
                    case RTTHREADSTATE_RW_WRITE:
                    {
                        PRTLOCKVALIDATORREC pCurRec = pCur->LockValidator.pRec;
                        if (    rtThreadGetState(pCur) != enmCurState
                            ||  !VALID_PTR(pCurRec)
                            ||  pCurRec->u32Magic != RTLOCKVALIDATORREC_MAGIC)
                            continue;

                        break;
                    }

                    case RTTHREADSTATE_RW_READ:
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
#endif

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
RT_EXPORT_SYMBOL(RTLockValidatorCheckBlocking);


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

