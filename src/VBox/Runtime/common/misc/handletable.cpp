/* $Id$ */
/** @file
 * IPRT - Handle Tables.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
#include <iprt/handletable.h>
#include <iprt/mem.h>
#include <iprt/spinlock.h>
#include <iprt/err.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/asm.h>
#include "internal/magics.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The number of entries in the 2nd level lookup table. */
#define RTHT_LEVEL2_ENTRIES             2048

/** The number of (max) 1st level entries requiring dynamic allocation of the
 * 1st level table. If the max number is below this threshold, the 1st level
 * table will be allocated as part of the handle table structure. */
#define RTHT_LEVEL1_DYN_ALLOC_THRESHOLD 256

/** Checks whether a object pointer is really a free entry or not. */
#define RTHT_IS_FREE(pvObj)             ( ((uintptr_t)(pvObj) & 3) == 3 )

/** Sets RTHTENTRYFREE::iNext. */
#define RTHT_SET_FREE_IDX(pFree, idx) \
    do { \
        (pFree)->iNext = ((uintptr_t)((uint32_t)(idx)) << 2) | 3U; \
    } while (0)

/** Gets the index part of RTHTENTRYFREE::iNext. */
#define RTHT_GET_FREE_IDX(pFree)        ( (uint32_t)((pFree)->iNext >> 2) )

/** @def NIL_RTHT_INDEX
 * The NIL handle index for use in the free list. (The difference between
 * 32-bit and 64-bit hosts here comes down to the shifting performed for
 * RTHTENTRYFREE::iNext.) */
#if ARCH_BITS == 32
# define NIL_RTHT_INDEX                 ( UINT32_C(0x3fffffff) )
#elif ARCH_BITS >= 34
# define NIL_RTHT_INDEX                 ( UINT32_C(0xffffffff) )
#else
# error "Missing or unsupported ARCH_BITS."
#endif


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * Handle table entry, simple variant.
 */
typedef struct RTHTENTRY
{
    /** The object. */
    void *pvObj;
} RTHTENTRY;
/** Pointer to a handle table entry, simple variant. */
typedef RTHTENTRY *PRTHTENTRY;


/**
 * Handle table entry, context variant.
 */
typedef struct RTHTENTRYCTX
{
    /** The object. */
    void *pvObj;
    /** The context. */
    void *pvCtx;
} RTHTENTRYCTX;
/** Pointer to a handle table entry, context variant. */
typedef RTHTENTRYCTX *PRTHTENTRYCTX;


/**
 * Free handle table entry, shared by all variants.
 */
typedef struct RTHTENTRYFREE
{
    /** The index of the next handle, special format.
     * In order to distinguish free and used handle table entries we exploit
     * the heap alignment and use the lower two bits to do this. Used entries
     * will have these bits set to 0, while free entries will have tem set
     * to 3. Use the RTHT_GET_FREE_IDX and RTHT_SET_FREE_IDX macros to access
     * this field. */
    uintptr_t iNext;
} RTHTENTRYFREE;
/** Pointer to a free handle table entry. */
typedef RTHTENTRYFREE *PRTHTENTRYFREE;

AssertCompile(sizeof(RTHTENTRYFREE) <= sizeof(RTHTENTRY));
AssertCompile(sizeof(RTHTENTRYFREE) <= sizeof(RTHTENTRYCTX));
AssertCompileMemberOffset(RTHTENTRYFREE, iNext, 0);
AssertCompileMemberOffset(RTHTENTRY,     pvObj, 0);
AssertCompileMemberOffset(RTHTENTRYCTX,  pvObj, 0);


/**
 * Internal handle table structure.
 */
typedef struct RTHANDLETABLEINT
{
    /** Magic value (RTHANDLETABLE_MAGIC). */
    uint32_t u32Magic;
    /** The handle table flags specified to RTHandleTableCreateEx. */
    uint32_t fFlags;
    /** The base handle value (i.e. the first handle). */
    uint32_t uBase;
    /** The current number of handle table entries. */
    uint32_t cCur;
    /** The spinlock handle (NIL if RTHANDLETABLE_FLAGS_LOCKED wasn't used). */
    RTSPINLOCK hSpinlock;
    /** The level one lookup table. */
    void **papvLevel1;
    /** The retainer callback. Can be NULL. */
    PFNRTHANDLETABLERETAIN pfnRetain;
    /** The user argument to the retainer. */
    void *pvRetainUser;
    /** The max number of handles. */
    uint32_t cMax;
    /** The number of handles currently allocated. (for optimizing destruction) */
    uint32_t cCurAllocated;
    /** The current number of 1st level entries. */
    uint32_t cLevel1;
    /** Head of the list of free handle entires (index). */
    uint32_t iFreeHead;
    /** Tail of the list of free handle entires (index). */
    uint32_t iFreeTail;
} RTHANDLETABLEINT;
/** Pointer to an handle table structure. */
typedef RTHANDLETABLEINT *PRTHANDLETABLEINT;


/**
 * Looks up a simple index.
 *
 * @returns Pointer to the handle table entry on success, NULL on failure.
 * @param   pThis           The handle table structure.
 * @param   i               The index to look up.
 */
DECLINLINE(PRTHTENTRY) rtHandleTableLookupSimpleIdx(PRTHANDLETABLEINT pThis, uint32_t i)
{
    if (i < pThis->cCur)
    {
        PRTHTENTRY paTable = (PRTHTENTRY)pThis->papvLevel1[i / RTHT_LEVEL2_ENTRIES];
        if (paTable)
            return &paTable[i % RTHT_LEVEL2_ENTRIES];
    }
    return NULL;
}


/**
 * Looks up a simple handle.
 *
 * @returns Pointer to the handle table entry on success, NULL on failure.
 * @param   pThis           The handle table structure.
 * @param   h               The handle to look up.
 */
DECLINLINE(PRTHTENTRY) rtHandleTableLookupSimple(PRTHANDLETABLEINT pThis, uint32_t h)
{
    return rtHandleTableLookupSimpleIdx(pThis, h - pThis->uBase);
}


/**
 * Looks up a context index.
 *
 * @returns Pointer to the handle table entry on success, NULL on failure.
 * @param   pThis           The handle table structure.
 * @param   i               The index to look up.
 */
DECLINLINE(PRTHTENTRYCTX) rtHandleTableLookupWithCtxIdx(PRTHANDLETABLEINT pThis, uint32_t i)
{
    if (i < pThis->cCur)
    {
        PRTHTENTRYCTX paTable = (PRTHTENTRYCTX)pThis->papvLevel1[i / RTHT_LEVEL2_ENTRIES];
        if (paTable)
            return &paTable[i % RTHT_LEVEL2_ENTRIES];
    }
    return NULL;
}


/**
 * Looks up a context handle.
 *
 * @returns Pointer to the handle table entry on success, NULL on failure.
 * @param   pThis           The handle table structure.
 * @param   h               The handle to look up.
 */
DECLINLINE(PRTHTENTRYCTX) rtHandleTableLookupWithCtx(PRTHANDLETABLEINT pThis, uint32_t h)
{
    return rtHandleTableLookupWithCtxIdx(pThis, h - pThis->uBase);
}


/**
 * Locks the handle table.
 *
 * @param   pThis           The handle table structure.
 * @param   pTmp            The spinlock temp variable.
 */
DECLINLINE(void) rtHandleTableLock(PRTHANDLETABLEINT pThis, PRTSPINLOCKTMP pTmp)
{
    if (pThis->hSpinlock != NIL_RTSPINLOCK)
    {
        RTSPINLOCKTMP const Tmp = RTSPINLOCKTMP_INITIALIZER;
        *pTmp = Tmp;
        RTSpinlockAcquire(pThis->hSpinlock, pTmp);
    }
}


/**
 * Locks the handle table.
 *
 * @param   pThis           The handle table structure.
 * @param   pTmp            The spinlock temp variable.
 */
DECLINLINE(void) rtHandleTableUnlock(PRTHANDLETABLEINT pThis, PRTSPINLOCKTMP pTmp)
{
    if (pThis->hSpinlock != NIL_RTSPINLOCK)
        RTSpinlockRelease(pThis->hSpinlock, pTmp);
}


RTDECL(int) RTHandleTableCreateEx(PRTHANDLETABLE phHandleTable, uint32_t fFlags, uint32_t uBase, uint32_t cMax,
                                  PFNRTHANDLETABLERETAIN pfnRetain, void *pvUser)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(phHandleTable, VERR_INVALID_POINTER);
    *phHandleTable = NIL_RTHANDLETABLE;
    AssertPtrNullReturn(pfnRetain, VERR_INVALID_POINTER);
    AssertReturn(!(fFlags & ~RTHANDLETABLE_FLAGS_MASK), VERR_INVALID_PARAMETER);
    AssertReturn(cMax > 0, VERR_INVALID_PARAMETER);
    AssertReturn(UINT32_MAX - cMax >= uBase, VERR_INVALID_PARAMETER);

    /*
     * Adjust the cMax value so it is a multiple of the 2nd level tables.
     */
    if (cMax >= UINT32_MAX - RTHT_LEVEL2_ENTRIES)
        cMax = UINT32_MAX - RTHT_LEVEL2_ENTRIES + 1;
    cMax = ((cMax + RTHT_LEVEL2_ENTRIES - 1) / RTHT_LEVEL2_ENTRIES) * RTHT_LEVEL2_ENTRIES;

    uint32_t const cLevel1 = cMax / RTHT_LEVEL2_ENTRIES;
    Assert(cLevel1 * RTHT_LEVEL2_ENTRIES == cMax);

    /*
     * Allocate the structure, include the 1st level lookup table
     * if it's below the threshold size.
     */
    size_t cb = sizeof(RTHANDLETABLEINT);
    if (cLevel1 < RTHT_LEVEL1_DYN_ALLOC_THRESHOLD)
        cb = RT_ALIGN(cb, sizeof(void *)) + cLevel1 * sizeof(void *);
    PRTHANDLETABLEINT pThis = (PRTHANDLETABLEINT)RTMemAllocZ(cb);
    if (!pThis)
        return VERR_NO_MEMORY;

    /*
     * Initialize it.
     */
    pThis->u32Magic = RTHANDLETABLE_MAGIC;
    pThis->fFlags = fFlags;
    pThis->uBase = uBase;
    pThis->cCur = 0;
    pThis->hSpinlock = NIL_RTSPINLOCK;
    if (cLevel1 < RTHT_LEVEL1_DYN_ALLOC_THRESHOLD)
        pThis->papvLevel1 = (void **)((uint8_t *)pThis + RT_ALIGN(sizeof(*pThis), sizeof(void *)));
    else
        pThis->papvLevel1 = NULL;
    pThis->pfnRetain = pfnRetain;
    pThis->pvRetainUser = pvUser;
    pThis->cMax = cMax;
    pThis->cCurAllocated = 0;
    pThis->cLevel1 = cLevel1 < RTHT_LEVEL1_DYN_ALLOC_THRESHOLD ? cLevel1 : 0;
    pThis->iFreeHead = NIL_RTHT_INDEX;
    pThis->iFreeTail = NIL_RTHT_INDEX;
    if (fFlags & RTHANDLETABLE_FLAGS_LOCKED)
    {
        int rc = RTSpinlockCreate(&pThis->hSpinlock);
        if (RT_FAILURE(rc))
        {
            RTMemFree(pThis);
            return rc;
        }
    }

    *phHandleTable = pThis;
    return VINF_SUCCESS;
}


RTDECL(int) RTHandleTableCreate(PRTHANDLETABLE phHandleTable)
{
    return RTHandleTableCreateEx(phHandleTable, RTHANDLETABLE_FLAGS_LOCKED, 1, 65534, NULL, NULL);
}


RTDECL(int) RTHandleTableDestroy(RTHANDLETABLE hHandleTable, PFNRTHANDLETABLEDELETE pfnDelete, void *pvUser)
{
    /*
     * Validate input, quitely ignore the NIL handle.
     */
    if (hHandleTable == NIL_RTHANDLETABLE)
        return VINF_SUCCESS;
    PRTHANDLETABLEINT pThis = (PRTHANDLETABLEINT)hHandleTable;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTHANDLETABLE_MAGIC, VERR_INVALID_HANDLE);
    AssertPtrNullReturn(pfnDelete, VERR_INVALID_POINTER);

    /*
     * Mark the thing as invalid / deleted.
     * Then kill the lock.
     */
    RTSPINLOCKTMP Tmp;
    rtHandleTableLock(pThis, &Tmp);
    ASMAtomicWriteU32(&pThis->u32Magic, ~RTHANDLETABLE_MAGIC);
    rtHandleTableUnlock(pThis, &Tmp);

    if (pThis->hSpinlock != NIL_RTSPINLOCK)
    {
        rtHandleTableLock(pThis, &Tmp);
        rtHandleTableUnlock(pThis, &Tmp);

        RTSpinlockDestroy(pThis->hSpinlock);
        pThis->hSpinlock = NIL_RTSPINLOCK;
    }

    if (pfnDelete)
    {
        /*
         * Walk all the tables looking for used handles.
         */
        uint32_t cLeft = pThis->cCurAllocated;
        if (pThis->fFlags & RTHANDLETABLE_FLAGS_CONTEXT)
        {
            for (uint32_t i1 = 0; cLeft > 0 && i1 < pThis->cLevel1; i1++)
            {
                PRTHTENTRYCTX paTable = (PRTHTENTRYCTX)pThis->papvLevel1[i1];
                if (paTable)
                    for (uint32_t i = 0; i < RTHT_LEVEL2_ENTRIES; i++)
                        if (!RTHT_IS_FREE(paTable[i].pvObj))
                        {
                            pfnDelete(hHandleTable, pThis->uBase + i + i1 * RTHT_LEVEL2_ENTRIES,
                                      paTable[i].pvObj, paTable[i].pvCtx, pvUser);
                            Assert(cLeft > 0);
                            cLeft--;
                        }
            }
        }
        else
        {
            for (uint32_t i1 = 0; cLeft > 0 && i1 < pThis->cLevel1; i1++)
            {
                PRTHTENTRY paTable = (PRTHTENTRY)pThis->papvLevel1[i1];
                if (paTable)
                    for (uint32_t i = 0; i < RTHT_LEVEL2_ENTRIES; i++)
                        if (!RTHT_IS_FREE(paTable[i].pvObj))
                        {
                            pfnDelete(hHandleTable, pThis->uBase + i + i1 * RTHT_LEVEL2_ENTRIES,
                                      paTable[i].pvObj, NULL, pvUser);
                            Assert(cLeft > 0);
                            cLeft--;
                        }
            }
        }
        Assert(!cLeft);
    }

    /*
     * Free the memory.
     */
    for (uint32_t i1 = 0; i1 < pThis->cLevel1; i1++)
        if (pThis->papvLevel1[i1])
        {
            RTMemFree(pThis->papvLevel1[i1]);
            pThis->papvLevel1[i1] = NULL;
        }

    if (pThis->cMax / RTHT_LEVEL2_ENTRIES >= RTHT_LEVEL1_DYN_ALLOC_THRESHOLD)
        RTMemFree(pThis->papvLevel1);

    RTMemFree(pThis);

    return VINF_SUCCESS;
}


/**
 * Allocates a handle from the handle table.
 *
 * @returns IPRT status code, almost any.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NO_MEMORY if we failed to extend the handle table.
 * @retval  VERR_NO_MORE_HANDLES if we're out of handles.
 *
 * @param   hHandleTable    The handle to the handle table.
 * @param   pvObj           The object to associate with the new handle.
 * @param   ph              Where to return the handle on success.
 *
 * @remarks Do not call this if RTHANDLETABLE_FLAGS_CONTEXT was used during creation.
 */
RTDECL(int)     RTHandleTableAlloc(RTHANDLETABLE hHandleTable, void *pvObj, uint32_t *ph);

/**
 * Looks up a handle.
 *
 * @returns The object pointer on success. NULL on failure.
 *
 * @param   hHandleTable    The handle to the handle table.
 * @param   h               The handle to lookup.
 *
 * @remarks Do not call this if RTHANDLETABLE_FLAGS_CONTEXT was used during creation.
 */
RTDECL(void *)  RTHandleTableLookup(RTHANDLETABLE hHandleTable, uint32_t h);

/**
 * Looks up and frees a handle.
 *
 * @returns The object pointer on success. NULL on failure.
 *
 * @param   hHandleTable    The handle to the handle table.
 * @param   h               The handle to lookup.
 *
 * @remarks Do not call this if RTHANDLETABLE_FLAGS_CONTEXT was used during creation.
 */
RTDECL(void *)  RTHandleTableFree(RTHANDLETABLE hHandleTable, uint32_t h);


RTDECL(int)     RTHandleTableAllocWithCtx(RTHANDLETABLE hHandleTable, void *pvObj, void *pvCtx, uint32_t *ph)
{
    /* validate the input */
    PRTHANDLETABLEINT pThis = (PRTHANDLETABLEINT)hHandleTable;
    AssertPtrReturn(pThis, VERR_INVALID_HANDLE);
    AssertReturn(pThis->u32Magic == RTHANDLETABLE_MAGIC, VERR_INVALID_HANDLE);
    AssertReturn(pThis->fFlags & RTHANDLETABLE_FLAGS_CONTEXT, VERR_INVALID_FUNCTION);
    AssertReturn(!RTHT_IS_FREE(pvObj), VERR_INVALID_PARAMETER);
    AssertPtrReturn(ph, VERR_INVALID_POINTER);
    *ph = pThis->uBase - 1;

    /*
     * Allocation loop.
     */
    RTSPINLOCKTMP Tmp;
    rtHandleTableLock(pThis, &Tmp);

    int rc;
    do
    {
        /*
         * Try grab a free entry from the head of the free list.
         */
        uint32_t i = pThis->iFreeHead;
        if (i != NIL_RTHT_INDEX)
        {
            PRTHTENTRYFREE pFree = (PRTHTENTRYFREE)rtHandleTableLookupWithCtxIdx(pThis, i);
            Assert(pFree);
            if (i == pThis->iFreeTail)
                pThis->iFreeTail = pThis->iFreeHead = NIL_RTHT_INDEX;
            else
                pThis->iFreeHead = RTHT_GET_FREE_IDX(pFree);
            pThis->cCurAllocated++;
            Assert(pThis->cCurAllocated <= pThis->cCur);

            /*
             * Setup the entry and return.
             */
            PRTHTENTRYCTX pEntry = (PRTHTENTRYCTX)pFree;
            pEntry->pvObj = pvObj;
            pEntry->pvCtx = pvCtx;
            *ph = i + pThis->uBase;
            rc = VINF_SUCCESS;
        }
        /*
         * Must expand the handle table, unless it's full.
         */
        else if (pThis->cCur >= pThis->cMax)
        {
            rc = VERR_NO_MORE_HANDLES;
            Assert(pThis->cCur == pThis->cCurAllocated);
        }
        else
        {
            /*
             * Do we have to expand the 1st level table too?
             */
            uint32_t const iLevel1 = pThis->cCur / RTHT_LEVEL2_ENTRIES;
            uint32_t cLevel1 = iLevel1 >= pThis->cLevel1
                             ? pThis->cLevel1 + PAGE_SIZE / sizeof(void *)
                             : 0;
            if (cLevel1 > pThis->cMax / RTHT_LEVEL2_ENTRIES)
                cLevel1 = pThis->cMax / RTHT_LEVEL2_ENTRIES;
            Assert(!cLevel1 || pThis->cMax / RTHT_LEVEL2_ENTRIES >= RTHT_LEVEL1_DYN_ALLOC_THRESHOLD);

            /* leave the lock (never do fancy stuff from behind a spinlock). */
            rtHandleTableUnlock(pThis, &Tmp);

            /*
             * Do the allocation(s).
             */
            rc = VERR_TRY_AGAIN;
            void **papvLevel1 = NULL;
            if (cLevel1)
            {
                papvLevel1 = (void **)RTMemAlloc(sizeof(void *) * cLevel1);
                if (!papvLevel1)
                    return VERR_NO_MEMORY;
            }

            PRTHTENTRYCTX paTable = (PRTHTENTRYCTX)RTMemAlloc(sizeof(*paTable) * RTHT_LEVEL2_ENTRIES);
            if (!paTable)
            {
                RTMemFree(papvLevel1);
                return VERR_NO_MEMORY;
            }

            /* re-enter the lock. */
            rtHandleTableLock(pThis, &Tmp);

            /*
             * Insert the new bits, but be a bit careful as someone might have
             * raced us expanding the table.
             */
            /* deal with the 1st level lookup expansion first */
            if (cLevel1)
            {
                Assert(papvLevel1);
                if (cLevel1 > pThis->cLevel1)
                {
                    /* Replace the 1st level table. */
                    memcpy(papvLevel1, pThis->papvLevel1, sizeof(void *) * pThis->cLevel1);
                    memset(&papvLevel1[pThis->cLevel1], 0, sizeof(void *) * (cLevel1 - pThis->cLevel1));
                    pThis->cLevel1 = cLevel1;
                    void **papvTmp = pThis->papvLevel1;
                    pThis->papvLevel1 = papvLevel1;
                    papvLevel1 = papvTmp;
                }

                /* free the obsolete one (outside the lock of course) */
                rtHandleTableUnlock(pThis, &Tmp);
                RTMemFree(papvLevel1);
                rtHandleTableLock(pThis, &Tmp);
            }

            /* insert the table we allocated */
            if (pThis->cCur < pThis->cMax)
            {
                uint32_t iLevel1New = pThis->cCur / RTHT_LEVEL2_ENTRIES;
                pThis->papvLevel1[iLevel1New] = paTable;

                /* link all entries into a free list. */
                Assert(!(pThis->cCur % RTHT_LEVEL2_ENTRIES));
                for (uint32_t i = 0; i < RTHT_LEVEL2_ENTRIES - 1; i++)
                {
                    RTHT_SET_FREE_IDX((PRTHTENTRYFREE)&paTable[i], i + 1 + pThis->cCur);
                    paTable[i].pvCtx = (void *)~(uintptr_t)7;
                }
                RTHT_SET_FREE_IDX((PRTHTENTRYFREE)&paTable[RTHT_LEVEL2_ENTRIES - 1], NIL_RTHT_INDEX);
                paTable[RTHT_LEVEL2_ENTRIES - 1].pvCtx = (void *)~(uintptr_t)7;

                /* join the free list with the other. */
                if (pThis->iFreeTail == NIL_RTHT_INDEX)
                    pThis->iFreeHead = pThis->cCur;
                else
                {
                    PRTHTENTRYFREE pPrev = (PRTHTENTRYFREE)rtHandleTableLookupWithCtxIdx(pThis, pThis->iFreeTail);
                    Assert(pPrev);
                    RTHT_SET_FREE_IDX(pPrev, pThis->cCur);
                }
                pThis->iFreeTail = pThis->cCur + RTHT_LEVEL2_ENTRIES - 1;

                pThis->cCur += RTHT_LEVEL2_ENTRIES;
            }
            else
            {
                /* free the table (raced someone, and we lost). */
                rtHandleTableUnlock(pThis, &Tmp);
                RTMemFree(paTable);
                rtHandleTableLock(pThis, &Tmp);
            }

            rc = VERR_TRY_AGAIN;
        }
    } while (rc == VERR_TRY_AGAIN);

    rtHandleTableUnlock(pThis, &Tmp);

    return rc;
}


RTDECL(void *)  RTHandleTableLookupWithCtx(RTHANDLETABLE hHandleTable, uint32_t h, void *pvCtx)
{
    /* validate the input */
    PRTHANDLETABLEINT pThis = (PRTHANDLETABLEINT)hHandleTable;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTHANDLETABLE_MAGIC, NULL);
    AssertReturn(pThis->fFlags & RTHANDLETABLE_FLAGS_CONTEXT, NULL);

    void *pvObj = NULL;

    /* acquire the lock */
    RTSPINLOCKTMP Tmp;
    rtHandleTableLock(pThis, &Tmp);

    /*
     * Perform the lookup and retaining.
     */
    PRTHTENTRYCTX pEntry = rtHandleTableLookupWithCtx(pThis, h);
    if (pEntry && pEntry->pvCtx == pvCtx)
    {
        pvObj = pEntry->pvObj;
        if (!RTHT_IS_FREE(pvObj))
        {
            if (pThis->pfnRetain)
            {
                int rc = pThis->pfnRetain(hHandleTable, pEntry->pvObj, pvCtx, pThis->pvRetainUser);
                if (RT_FAILURE(rc))
                    pvObj = NULL;
            }
        }
        else
            pvObj = NULL;
    }

    /* release the lock */
    rtHandleTableUnlock(pThis, &Tmp);
    return pvObj;
}


RTDECL(void *)  RTHandleTableFreeWithCtx(RTHANDLETABLE hHandleTable, uint32_t h, void *pvCtx)
{
    /* validate the input */
    PRTHANDLETABLEINT pThis = (PRTHANDLETABLEINT)hHandleTable;
    AssertPtrReturn(pThis, NULL);
    AssertReturn(pThis->u32Magic == RTHANDLETABLE_MAGIC, NULL);
    AssertReturn(pThis->fFlags & RTHANDLETABLE_FLAGS_CONTEXT, NULL);

    void *pvObj = NULL;

    /* acquire the lock */
    RTSPINLOCKTMP Tmp;
    rtHandleTableLock(pThis, &Tmp);

    /*
     * Perform the lookup and retaining.
     */
    PRTHTENTRYCTX pEntry = rtHandleTableLookupWithCtx(pThis, h);
    if (pEntry && pEntry->pvCtx == pvCtx)
    {
        pvObj = pEntry->pvObj;
        if (!RTHT_IS_FREE(pvObj))
        {
            if (pThis->pfnRetain)
            {
                int rc = pThis->pfnRetain(hHandleTable, pEntry->pvObj, pvCtx, pThis->pvRetainUser);
                if (RT_FAILURE(rc))
                    pvObj = NULL;
            }

            /*
             * Link it into the free list.
             */
            if (pvObj)
            {
                pEntry->pvCtx = (void *)~(uintptr_t)7;

                PRTHTENTRYFREE pFree = (PRTHTENTRYFREE)pEntry;
                RTHT_SET_FREE_IDX(pFree, NIL_RTHT_INDEX);

                uint32_t const i = h - pThis->uBase;
                if (pThis->iFreeTail == NIL_RTHT_INDEX)
                    pThis->iFreeHead = pThis->iFreeTail = i;
                else
                {
                    PRTHTENTRYFREE pPrev = (PRTHTENTRYFREE)rtHandleTableLookupWithCtxIdx(pThis, pThis->iFreeTail);
                    Assert(pPrev);
                    RTHT_SET_FREE_IDX(pPrev, i);
                    pThis->iFreeTail = i;
                }

                Assert(pThis->cCurAllocated > 0);
                pThis->cCurAllocated--;
            }
        }
        else
            pvObj = NULL;
    }

    /* release the lock */
    rtHandleTableUnlock(pThis, &Tmp);
    return pvObj;
}

