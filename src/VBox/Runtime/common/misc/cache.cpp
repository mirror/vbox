/* $Id$ */
/** @file
 * IPRT - Object cache
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
#include <iprt/cache.h>
#include "internal/iprt.h"

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/err.h>
#include <iprt/spinlock.h>
#include <iprt/mem.h>
#include <iprt/asm.h>

#include "internal/magics.h"


/**
 * Create a cache for objects.
 *
 * @returns iprt status code.
 * @param   ppObjCache    Where to store the pointer to the created cache.
 * @param   cElements     Number of elements the cache can hold.
 *                        0 if unlimited size.
 * @param   cbElement     Size of one element in bytes
 * @param   fProt         Protection flags for protecting cache against concurrent access
 *                        from different threads.
 *                        RTOBJCACHE_PROTECT_REQUESTER to protect the request function.
 *                        RTOBJCACHE_PROTECT_INSERT    to protect the insert function.
 */
RTDECL(int) RTCacheCreate(PRTOBJCACHE *ppCache, uint32_t cElements, size_t cbElement, uint32_t fProt)
{
    int rc = VINF_SUCCESS;
    uint32_t cbCache = sizeof(RTOBJCACHE);

    if (   RT_UNLIKELY(!VALID_PTR(ppCache))
        || !cbElement
        || (fProt & ~RTOBJCACHE_PROTECT_VALID))
        return VERR_INVALID_PARAMETER;

    if (cElements > 0)
        cbCache += (cElements + 1) * sizeof(void *); /* One slot is always free. */

    PRTOBJCACHE pCacheNew = (PRTOBJCACHE)RTMemAllocZ(cbCache);
    if (!pCacheNew)
        return VERR_NO_MEMORY;

    pCacheNew->cbObj             = cbElement;
    pCacheNew->cElements         = cElements;
    pCacheNew->SpinlockRequest = NIL_RTSPINLOCK;
    pCacheNew->SpinlockInsert    = NIL_RTSPINLOCK;
    if (fProt & RTOBJCACHE_PROTECT_REQUEST)
    {
        rc = RTSpinlockCreate(&pCacheNew->SpinlockRequest);
        if (RT_FAILURE(rc))
        {
            RTMemFree(pCacheNew);
            return rc;
        }
    }

    if (fProt & RTOBJCACHE_PROTECT_INSERT)
    {
        rc = RTSpinlockCreate(&pCacheNew->SpinlockInsert);
        if (RT_FAILURE(rc))
        {
            if (pCacheNew->SpinlockRequest != NIL_RTSPINLOCK)
                RTSpinlockDestroy(pCacheNew->SpinlockRequest);

            RTMemFree(pCacheNew);
            return rc;
        }
    }

    /* Initialize array if cache is limited. */
    if (pCacheNew->cElements > 0)
    {
        pCacheNew->u.defined.cElementsInCache = 0;
        pCacheNew->u.defined.cNextObjRead = 0;
        pCacheNew->u.defined.cNextFreeSlotWrite = 0;
    }
    else
    {
        /* Unlimited cache - Create dummy element. */
        PRTOBJCACHEHDR pDummy = (PRTOBJCACHEHDR)RTMemAllocZ(sizeof(RTOBJCACHEHDR) + pCacheNew->cbObj);
        if (!pDummy)
        {
            /* Cleanup. */
            if (pCacheNew->SpinlockRequest != NIL_RTSPINLOCK)
                RTSpinlockDestroy(pCacheNew->SpinlockRequest);

            if (pCacheNew->SpinlockInsert != NIL_RTSPINLOCK)
                RTSpinlockDestroy(pCacheNew->SpinlockInsert);

            RTMemFree(pCacheNew);
            return VERR_NO_MEMORY;
        }

        pDummy->uMagic = RTMEMCACHE_MAGIC;
        pCacheNew->u.unlimited.pFirst = pDummy;
        pCacheNew->u.unlimited.pLast  = pDummy;
    }

    if (RT_SUCCESS(rc))
        *ppCache = pCacheNew;

    return rc;
}
RT_EXPORT_SYMBOL(RTCacheCreate);


/**
 * Destroy a cache freeing allocated memory.
 *
 * @returns iprt status code.
 * @param   pCache     Pointer to the cache.
 */
RTDECL(int) RTCacheDestroy(PRTOBJCACHE pCache)
{
    if (pCache->SpinlockRequest != NIL_RTSPINLOCK)
        RTSpinlockDestroy(pCache->SpinlockRequest);

    if (pCache->SpinlockInsert != NIL_RTSPINLOCK)
        RTSpinlockDestroy(pCache->SpinlockInsert);

    /* Free all objects left in cache. */
    if (pCache->cElements == 0)
    {
        volatile RTOBJCACHEHDR *pHdr = pCache->u.unlimited.pFirst;

        while (pHdr)
        {
            volatile RTOBJCACHEHDR *pTemp = pHdr;

            pHdr = pHdr->pNext;
            RTMemFree((void *)pTemp);
        }
    }
    else
    {
        while (pCache->u.defined.cNextObjRead != pCache->u.defined.cNextFreeSlotWrite)
        {
            if (pCache->u.defined.apObjCached[pCache->u.defined.cNextObjRead])
                RTMemFree((void *)pCache->u.defined.apObjCached[pCache->u.defined.cNextObjRead]);
            pCache->u.defined.cNextObjRead++;
            pCache->u.defined.cNextObjRead &= pCache->cElements;
        }
    }

    RTMemFree(pCache);

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTCacheDestroy);


/**
 * Request an object from the cache.
 *
 * @returns iprt status
 *          VERR_CACHE_EMPTY if cache is not unlimited and there is no object left in cache.
 * @param   pCache     Pointer to the cache to get an object from.
 * @param   ppObj      Where to store the pointer to the object.
 */
RTDECL(int) RTCacheRequest(PRTOBJCACHE pCache, void **ppObj)
{
    RTSPINLOCKTMP spinlockTmp = RTSPINLOCKTMP_INITIALIZER;

    if (pCache->SpinlockRequest != NIL_RTSPINLOCK)
        RTSpinlockAcquire(pCache->SpinlockRequest, &spinlockTmp);

    if (pCache->cElements == 0)
    {
        if (pCache->u.unlimited.pFirst != pCache->u.unlimited.pLast)
        {
            volatile RTOBJCACHEHDR *pHdr = pCache->u.unlimited.pFirst;

            pCache->u.unlimited.pFirst = pHdr->pNext;

            if (pCache->SpinlockRequest != NIL_RTSPINLOCK)
                RTSpinlockRelease(pCache->SpinlockRequest, &spinlockTmp);

             pHdr->pNext = NULL;
            *ppObj = (uint8_t *)pHdr + sizeof(RTOBJCACHEHDR);
        }
        else
        {
            /* We have to create a new cached object. - We can leave the spinlock safely here. */
            if (pCache->SpinlockRequest != NIL_RTSPINLOCK)
                RTSpinlockRelease(pCache->SpinlockRequest, &spinlockTmp);

            PRTOBJCACHEHDR pObjNew = (PRTOBJCACHEHDR)RTMemAllocZ(sizeof(RTOBJCACHEHDR) + pCache->cbObj);
            if (!pObjNew)
                return VERR_NO_MEMORY;

            pObjNew->uMagic = RTMEMCACHE_MAGIC;
            *ppObj = (uint8_t *)pObjNew + sizeof(RTOBJCACHEHDR);
        }
    }
    else
    {
        if (pCache->u.defined.cElementsInCache > 0)
        {
            *ppObj = (void *)pCache->u.defined.apObjCached[pCache->u.defined.cNextObjRead];
            pCache->u.defined.cNextObjRead++;
            pCache->u.defined.cNextObjRead &= pCache->cElements;
            ASMAtomicDecU32((volatile uint32_t *)&pCache->u.defined.cElementsInCache);

            if (pCache->SpinlockRequest != NIL_RTSPINLOCK)
                RTSpinlockRelease(pCache->SpinlockRequest, &spinlockTmp);
        }
        else
        {
            if (pCache->SpinlockRequest != NIL_RTSPINLOCK)
                RTSpinlockRelease(pCache->SpinlockRequest, &spinlockTmp);

            return VERR_CACHE_EMPTY;
        }
    }

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTCacheRequest);


/**
 * Insert an object into the cache.
 *
 * @returns iprt status code.
 *          VERR_CACHE_FULL if cache is not unlimited and there is no free entry left in cache.
 * @param   pCache     Pointer to the cache.
 * @param   pObj       Pointer to the object to insert.
 */
RTDECL(int) RTCacheInsert(PRTOBJCACHE pCache, void *pObj)
{
    RTSPINLOCKTMP spinlockTmp = RTSPINLOCKTMP_INITIALIZER;
    int rc = VINF_SUCCESS;

    if (pCache->SpinlockInsert != NIL_RTSPINLOCK)
        RTSpinlockAcquire(pCache->SpinlockInsert, &spinlockTmp);

    if (pCache->cElements == 0)
    {
        PRTOBJCACHEHDR pHdr = (PRTOBJCACHEHDR)((uint8_t *)pObj - sizeof(RTOBJCACHEHDR));

        AssertMsg(pHdr->uMagic == RTMEMCACHE_MAGIC, ("This is not a cached object\n"));

        pCache->u.unlimited.pLast->pNext = pHdr;
        pCache->u.unlimited.pLast = pHdr;
    }
    else
    {
        if (pCache->u.defined.cElementsInCache == pCache->cElements)
            rc = VERR_CACHE_FULL;
        else
        {
            pCache->u.defined.apObjCached[pCache->u.defined.cNextFreeSlotWrite] = pObj;
            pCache->u.defined.cNextFreeSlotWrite++;
            pCache->u.defined.cNextFreeSlotWrite &= pCache->cElements;
            ASMAtomicIncU32((volatile uint32_t *)&pCache->u.defined.cElementsInCache);
        }
    }

    if (pCache->SpinlockInsert != NIL_RTSPINLOCK)
        RTSpinlockRelease(pCache->SpinlockInsert, &spinlockTmp);

    return rc;
}
RT_EXPORT_SYMBOL(RTCacheInsert);

