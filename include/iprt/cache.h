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

#ifndef ___iprt_cache_h
#define ___iprt_cache_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/critsect.h>

RT_BEGIN_DECLS

/** Protect the object requester against concurrent access from different threads. */
#define RTOBJCACHE_PROTECT_REQUEST   RT_BIT(0)
/** Protect the object insert function against concurrent access from different threads. */
#define RTOBJCACHE_PROTECT_INSERT    RT_BIT(1)
/** All valid protection flags. */
#define RTOBJCACHE_PROTECT_VALID (RTOBJCACHE_PROTECT_REQUEST | RTOBJCACHE_PROTECT_INSERT)

/**
 * Object cache header for unlimited sized caches.
 */
typedef struct RTOBJCACHEHDR
{
    /** Magic value for identifying .*/
    uint32_t                         uMagic;
    /** Next element in the list. */
    volatile struct RTOBJCACHEHDR   *pNext;
} RTOBJCACHEHDR, *PRTOBJCACHEHDR;

/**
 * Object cache
 */
typedef struct RTOBJCACHE
{
    /** Size of the objects to cache. */
    size_t      cbObj;
    /** Spinlock protecting the object request function if requested. */
    RTSPINLOCK  SpinlockRequest;
    /** Spinlock protecting the object insert function if requested. */
    RTSPINLOCK  SpinlockInsert;
    /** Size of the cache - 0 if unlimited size. */
    uint32_t    cElements;
    /** Cache type dependent data. */
    union
    {
        /** Structure for unlimited cache. */
        struct
        {
            /** Pointer to the first element in the list. */
            volatile RTOBJCACHEHDR *pFirst;
            /** Pointer to the last element in the list. */
            volatile RTOBJCACHEHDR *pLast;
        } unlimited;
        /** Structure for cache with defined size. */
        struct
        {
            /** Current fill rate. */
            volatile uint32_t  cElementsInCache;
            /** Next free element to get. */
            volatile uint32_t  cNextObjRead;
            /** Next free slot to write to. */
            volatile uint32_t  cNextFreeSlotWrite;
            /** Array of cached objects.  - real size allocated on cache creation. */
            volatile void     *apObjCached[1];
        } defined;
    } u;
} RTOBJCACHE, *PRTOBJCACHE;


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
 *                        RTOBJCACHE_PROTECT_REQUEST   to protect the request function.
 *                        RTOBJCACHE_PROTECT_INSERT    to protect the insert function.
 */
RTDECL(int) RTCacheCreate(PRTOBJCACHE *ppCache, uint32_t cElements, size_t cbElement, uint32_t fProt);

/**
 * Destroy a cache freeing allocated memory.
 *
 * @returns iprt status code.
 * @param   pCache     Pointer to the cache.
 */
RTDECL(int) RTCacheDestroy(PRTOBJCACHE pCache);

/**
 * Request an object from the cache.
 *
 * @returns iprt status
 *          VERR_CACHE_EMPTY if cache is not unlimited and there is no object left in cache.
 * @param   pCache     Pointer to the cache to get an object from.
 * @param   ppObj      Where to store the pointer to the object.
 */
RTDECL(int) RTCacheRequest(PRTOBJCACHE pCache, void **ppObj);

/**
 * Insert an object into the cache.
 *
 * @returns iprt status code.
 *          VERR_CACHE_FULL if cache is not unlimited and there is no free entry left in cache.
 * @param   pCache     Pointer to the cache.
 * @param   pObj       Pointer to the object to insert.
 */
RTDECL(int) RTCacheInsert(PRTOBJCACHE pCache, void *pObj);

RT_END_DECLS

#endif /* __iprt_cache_h */
