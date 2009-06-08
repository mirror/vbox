/* $Id$ */
/** @file
 * IPRT - Memory Allocation Pool.
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
#include <iprt/mempool.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/spinlock.h>
#include <iprt/string.h>

#include "internal/magics.h"


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/** Pointer to a memory pool instance. */
typedef struct RTMEMPOOLINT *PRTMEMPOOLINT;
/** Pointer to a memory pool entry. */
typedef struct RTMEMPOOLENTRY *PRTMEMPOOLENTRY;

/**
 * Memory pool entry.
 */
typedef struct RTMEMPOOLENTRY
{
    /** Pointer to the pool */
    PRTMEMPOOLINT               pMemPool;
    /** Pointer to the next entry. */
    PRTMEMPOOLENTRY volatile    pNext;
    /** Pointer to the previous entry. */
    PRTMEMPOOLENTRY volatile    pPrev;
    /** The number of references to the pool entry. */
    uint32_t volatile           cRefs;
} RTMEMPOOLENTRY;


/**
 * Memory pool instance data.
 */
typedef struct RTMEMPOOLINT
{
    /** Magic number (RTMEMPOOL_MAGIC). */
    uint32_t                    u32Magic;
    /** Spinlock protecting the pool entry list updates. */
    RTSPINLOCK                  hSpinLock;
    /** Head entry pointer. */
    PRTMEMPOOLENTRY volatile    pHead;
    /** The number of entries in the pool (for statistical purposes). */
    uint32_t volatile           cEntries;
    /** User data assoicated with the pool. */
    void                       *pvUser;
    /** The pool name. (variable length)  */
    char                        szName[8];
} RTMEMPOOLINT;


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a memory pool handle, translating RTMEMPOOL_DEFAULT when found,
 * and returns rc if not valid. */
#define RTMEMPOOL_VALID_RETURN_RC(pMemPool, rc) \
    do { \
        if (pMemPool == RTMEMPOOL_DEFAULT) \
            pMemPool = &g_rtMemPoolDefault; \
        else \
        { \
            AssertPtrReturn((pMemPool), (rc)); \
            AssertReturn((pMemPool)->u32Magic == RTMEMPOOL_MAGIC, (rc)); \
        } \
    } while (0)

/** Validates a memory pool entry and returns rc if not valid. */
#define RTMEMPOOL_VALID_ENTRY_RETURN_RC(pEntry, rc) \
    do { \
        AssertPtrReturn(pEntry, (rc)); \
        AssertPtrNullReturn((pEntry)->pMemPool, (rc)); \
        AssertReturn((pEntry)->pMemPool->u32Magic == RTMEMPOOL_MAGIC, (rc)); \
        AssertPtrNull((pEntry)->pNext); \
        AssertPtrNull((pEntry)->pPrev); \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** The */
static RTMEMPOOLINT g_rtMemPoolDefault =
{
    /* .u32Magic = */           RTMEMPOOL_MAGIC,
    /* .hSpinLock = */          NIL_RTSPINLOCK,
    /* .pHead = */              NULL,
    /* .cEntries = */           0,
    /* .pvUser = */             NULL,
    /* .szName = */             "default"
};



RTDECL(int) RTMemPoolCreate(PRTMEMPOOL phMemPool, const char *pszName)
{
    AssertPtr(phMemPool);
    AssertPtr(pszName);
    Assert(*pszName);

    size_t          cchName  = strlen(pszName);
    PRTMEMPOOLINT   pMemPool = (PRTMEMPOOLINT)RTMemAlloc(RT_OFFSETOF(RTMEMPOOLINT, szName[cchName + 1]));
    if (!pMemPool)
        return VERR_NO_MEMORY;
    int rc = RTSpinlockCreate(&pMemPool->hSpinLock);
    if (RT_SUCCESS(rc))
    {
        pMemPool->u32Magic = RTMEMPOOL_MAGIC;
        pMemPool->pHead = NULL;
        pMemPool->cEntries = 0;
        pMemPool->pvUser = NULL;
        memcpy(pMemPool->szName, pszName, cchName);
        *phMemPool = pMemPool;
        return VINF_SUCCESS;
    }
    RTMemFree(pMemPool);
    return rc;
}


RTDECL(int) RTMemPoolDestroy(RTMEMPOOL hMemPool)
{

    return VERR_NOT_IMPLEMENTED;
}



DECLINLINE(void) rtMemPoolInitAndLink(PRTMEMPOOLINT pMemPool, PRTMEMPOOLENTRY pEntry)
{
    pEntry->pMemPool = pMemPool;
    pEntry->pNext    = NULL;
    pEntry->pPrev    = NULL;
    pEntry->cRefs    = 1;

    if (pMemPool->hSpinLock != NIL_RTSPINLOCK)
    {
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquire(pMemPool->hSpinLock, &Tmp);

        PRTMEMPOOLENTRY pHead = pMemPool->pHead;
        pEntry->pNext = pHead;
        if (pHead)
            pHead->pPrev = pEntry;
        else
            pMemPool->pHead = pEntry;

        RTSpinlockRelease(pMemPool->hSpinLock, &Tmp);
    }

    ASMAtomicIncU32(&pMemPool->cEntries);
}


DECLINLINE(void) rtMemPoolUnlink(PRTMEMPOOLENTRY pEntry)
{
    PRTMEMPOOLINT pMemPool = pEntry->pMemPool;
    if (pMemPool->hSpinLock != NIL_RTSPINLOCK)
    {
        RTSPINLOCKTMP Tmp = RTSPINLOCKTMP_INITIALIZER;
        RTSpinlockAcquire(pMemPool->hSpinLock, &Tmp);

        if (pEntry->pNext)
            pEntry->pNext->pPrev = pEntry->pPrev;
        if (pEntry->pPrev)
            pEntry->pPrev->pNext = pEntry->pNext;
        else
            pMemPool->pHead      = pEntry->pNext;
        pEntry->pMemPool = NULL;

        RTSpinlockRelease(pMemPool->hSpinLock, &Tmp);
    }
    else
        pEntry->pMemPool = NULL;

    ASMAtomicDecU32(&pMemPool->cEntries);
}


RTDECL(void *) RTMemPoolAlloc(RTMEMPOOL hMemPool, size_t cb) RT_NO_THROW
{
    PRTMEMPOOLINT pMemPool = hMemPool;
    RTMEMPOOL_VALID_RETURN_RC(pMemPool, NULL);

    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)RTMemAlloc(cb + sizeof(*pEntry));
    if (!pEntry)
        return NULL;
    rtMemPoolInitAndLink(pMemPool, pEntry);

    return pEntry + 1;
}


RTDECL(void *) RTMemPoolAllocZ(RTMEMPOOL hMemPool, size_t cb) RT_NO_THROW
{
    PRTMEMPOOLINT pMemPool = hMemPool;
    RTMEMPOOL_VALID_RETURN_RC(pMemPool, NULL);

    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)RTMemAllocZ(cb + sizeof(*pEntry));
    if (!pEntry)
        return NULL;
    rtMemPoolInitAndLink(pMemPool, pEntry);

    return pEntry + 1;
}


RTDECL(void *) RTMemPoolDup(RTMEMPOOL hMemPool, const void *pvSrc, size_t cb) RT_NO_THROW
{
    PRTMEMPOOLINT pMemPool = hMemPool;
    RTMEMPOOL_VALID_RETURN_RC(pMemPool, NULL);

    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)RTMemAlloc(cb + sizeof(*pEntry));
    if (!pEntry)
        return NULL;
    memcpy(pEntry + 1, pvSrc, cb);
    rtMemPoolInitAndLink(pMemPool, pEntry);

    return pEntry + 1;
}


RTDECL(void *) RTMemPoolDupEx(RTMEMPOOL hMemPool, const void *pvSrc, size_t cbSrc, size_t cbExtra) RT_NO_THROW
{
    PRTMEMPOOLINT pMemPool = hMemPool;
    RTMEMPOOL_VALID_RETURN_RC(pMemPool, NULL);

    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)RTMemAlloc(cbSrc + cbExtra + sizeof(*pEntry));
    if (!pEntry)
        return NULL;
    memcpy(pEntry + 1, pvSrc, cbSrc);
    memset((uint8_t *)(pEntry + 1) + cbSrc, '\0', cbExtra);
    rtMemPoolInitAndLink(pMemPool, pEntry);

    return pEntry + 1;
}



RTDECL(void *) RTMemPoolRealloc(RTMEMPOOL hMemPool, void *pvOld, size_t cbNew) RT_NO_THROW
{
    /*
     * Fend off the odd cases.
     */
    if (!cbNew)
    {
        RTMemPoolRelease(hMemPool, pvOld);
        return NULL;
    }

    if (!pvOld)
        return RTMemPoolAlloc(hMemPool, cbNew);

    /*
     * Real realloc.
     */
    PRTMEMPOOLINT   pNewMemPool = hMemPool;
    RTMEMPOOL_VALID_RETURN_RC(pNewMemPool, NULL);

    PRTMEMPOOLENTRY pOldEntry = (PRTMEMPOOLENTRY)pvOld - 1;
    RTMEMPOOL_VALID_ENTRY_RETURN_RC(pOldEntry, NULL);
    PRTMEMPOOLINT   pOldMemPool = pOldEntry->pMemPool;
    AssertReturn(pOldEntry->cRefs == 1, NULL);

    /*
     * Unlink it from the current pool and try reallocate it.
     */
    rtMemPoolUnlink(pOldEntry);

    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)RTMemRealloc(pOldEntry, cbNew + sizeof(*pEntry));
    if (!pEntry)
    {
        rtMemPoolInitAndLink(pOldMemPool, pOldEntry);
        return NULL;
    }
    rtMemPoolInitAndLink(pNewMemPool, pEntry);

    return pEntry + 1;
}


RTDECL(void) RTMemPoolFree(RTMEMPOOL hMemPool, void *pv) RT_NO_THROW
{
    RTMemPoolRelease(hMemPool, pv);
}


RTDECL(uint32_t) RTMemPoolRetain(void *pv) RT_NO_THROW
{
    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)pv - 1;
    RTMEMPOOL_VALID_ENTRY_RETURN_RC(pEntry, UINT32_MAX);

    uint32_t cRefs = ASMAtomicIncU32(&pEntry->cRefs);
    Assert(cRefs < UINT32_MAX / 2);

    return cRefs;
}


RTDECL(uint32_t) RTMemPoolRelease(RTMEMPOOL hMemPool, void *pv) RT_NO_THROW
{
    if (!pv)
        return 0;

    PRTMEMPOOLENTRY pEntry = (PRTMEMPOOLENTRY)pv - 1;
    RTMEMPOOL_VALID_ENTRY_RETURN_RC(pEntry, UINT32_MAX);
    Assert(    hMemPool == NIL_RTMEMPOOL
           ||  hMemPool == pEntry->pMemPool
           ||  (hMemPool == RTMEMPOOL_DEFAULT && pEntry->pMemPool == &g_rtMemPoolDefault));
    AssertReturn(pEntry->cRefs > 0, UINT32_MAX);

    uint32_t cRefs = ASMAtomicDecU32(&pEntry->cRefs);
    Assert(cRefs < UINT32_MAX / 2);
    if (!cRefs)
    {
        rtMemPoolUnlink(pEntry);
        RTMemFree(pEntry);
    }

    return cRefs;
}

