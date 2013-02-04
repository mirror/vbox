/* $Id$ */
/** @file
 * IPRT - String Cache.
 */

/*
 * Copyright (C) 2010 Oracle Corporation
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
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/strcache.h>
#include "internal/iprt.h"

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mempool.h>
#include <iprt/string.h>
#include <iprt/once.h>


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * String cache entry.
 *
 * Each entry is
 */
typedef struct RTSTRCACHEENTRY
{
    /** The number of references. */
    uint32_t volatile   cRefs;
    /** Offset into the chunk (bytes). */
    uint32_t            offChunk;
    /** The string length. */
    uint32_t            cch;
    /** The primary hash value. */
    uint32_t            uHash1;
    /** The string. */
    char                szString[16];
} RTSTRCACHEENTRY;
AssertCompileSize(RTSTRCACHEENTRY, 32);
/** Pointer to a string cache entry. */
typedef RTSTRCACHEENTRY *PRTSTRCACHEENTRY;
/** Pointer to a const string cache entry. */
typedef RTSTRCACHEENTRY *PCRTSTRCACHEENTRY;

/**
 * Allocation chunk.
 */
typedef struct RTSTRCACHECHUNK
{
    /** Pointer to the main string cache structure. */
    struct RTSTRCACHEINT   *pCache;
    /** Padding to align the entries on a 32-byte boundary. */
    uint32_t                au32Padding[8 - (ARCH_BITS == 64) - 4];
    /** The index of the first unused entry. */
    uint32_t                iUnused;
    /** The number of used entries. */
    uint32_t                cUsed;
    /** The number of entries in this chunk. */
    uint32_t                cEntries;
    /** The string cache entries, variable size. */
    RTSTRCACHEENTRY         aEntries[1];
} RTSTRCACHECHUNK;


typedef struct RTSTRCACHEINT
{
    /** The string cache magic (RTSTRCACHE_MAGIC). */
    uint32_t        u32Magic;

} RTSTRCACHEINT;




/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** Validates a string cache handle, translating RTSTRCACHE_DEFAULT when found,
 * and returns rc if not valid. */
#define RTSTRCACHE_VALID_RETURN_RC(pStrCache, rc) \
    do { \
        if ((pStrCache) == RTMEMPOOL_DEFAULT) \
            (pStrCache) = &g_rtMemPoolDefault; \
        else \
        { \
            AssertPtrReturn((pStrCache), (rc)); \
            AssertReturn((pStrCache)->u32Magic == RTSTRCACHE_MAGIC, (rc)); \
        } \
    } while (0)

/** Validates a memory pool entry and returns rc if not valid. */
#define RTSTRCACHE_VALID_ENTRY_RETURN_RC(pEntry, rc) \
    do { \
        AssertPtrReturn(pEntry, (rc)); \
        AssertPtrNullReturn((pEntry)->pMemPool, (rc)); \
        Assert((pEntry)->cRefs < UINT32_MAX / 2); \
        AssertReturn((pEntry)->pMemPool->u32Magic == RTMEMPOOL_MAGIC, (rc)); \
    } while (0)


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/



RTDECL(int) RTStrCacheCreate(PRTSTRCACHE phStrCache, const char *pszName)
{
    AssertCompile(sizeof(RTSTRCACHE) == sizeof(RTMEMPOOL));
    AssertCompile(NIL_RTSTRCACHE == (RTSTRCACHE)NIL_RTMEMPOOL);
    AssertCompile(RTSTRCACHE_DEFAULT == (RTSTRCACHE)RTMEMPOOL_DEFAULT);
    return RTMemPoolCreate((PRTMEMPOOL)phStrCache, pszName);
}
RT_EXPORT_SYMBOL(RTStrCacheCreate);


RTDECL(int) RTStrCacheDestroy(RTSTRCACHE hStrCache)
{
    if (    hStrCache == NIL_RTSTRCACHE
        ||  hStrCache == RTSTRCACHE_DEFAULT)
        return VINF_SUCCESS;
    return RTMemPoolDestroy((RTMEMPOOL)hStrCache);
}
RT_EXPORT_SYMBOL(RTStrCacheDestroy);


RTDECL(const char *) RTStrCacheEnterN(RTSTRCACHE hStrCache, const char *pchString, size_t cchString)
{
    AssertPtr(pchString);
    AssertReturn(cchString < _1G, NULL);
    Assert(!RTStrEnd(pchString, cchString));

    return (const char *)RTMemPoolDupEx((RTMEMPOOL)hStrCache, pchString, cchString, 1);
}
RT_EXPORT_SYMBOL(RTStrCacheEnterN);


RTDECL(const char *) RTStrCacheEnter(RTSTRCACHE hStrCache, const char *psz)
{
    return RTStrCacheEnterN(hStrCache, psz, strlen(psz));
}
RT_EXPORT_SYMBOL(RTStrCacheEnter);


RTDECL(uint32_t) RTStrCacheRetain(const char *psz)
{
    AssertPtr(psz);
    return RTMemPoolRetain((void *)psz);
}
RT_EXPORT_SYMBOL(RTStrCacheRetain);


RTDECL(uint32_t) RTStrCacheRelease(RTSTRCACHE hStrCache, const char *psz)
{
    if (!psz)
        return 0;
    return RTMemPoolRelease((RTMEMPOOL)hStrCache, (void *)psz);
}
RT_EXPORT_SYMBOL(RTStrCacheRelease);


RTDECL(size_t) RTStrCacheLength(const char *psz)
{
    if (!psz)
        return 0;
    return strlen(psz);
}
RT_EXPORT_SYMBOL(RTStrCacheLength);

