/* $Id$ */
/** @file
 * IPRT - String Cache, stub implementation.
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

#ifndef ___iprt_strcache_h
#define ___iprt_strcache_h

#include <iprt/types.h>

RT_C_DECLS_BEGIN


/**
 * Create a new string cache.
 *
 * @returns IPRT status code
 *
 * @param   phStrCache          Where to return the string cache handle.
 * @param   pszName             The name of the cache (for debug purposes).
 */
RTDECL(int) RTStrCacheCreate(PRTSTRCACHE phStrCache, const char *pszName);


/**
 * Destroys a string cache.
 *
 * This will cause all strings in the cache to be released and thus become
 * invalid.
 *
 * @returns IPRT status.
 *
 * @param   hStrCache           Handle to the string cache. The nil and default
 *                              handles are ignored quietly (VINF_SUCCESS).
 */
RTDECL(int) RTStrCacheDestroy(RTSTRCACHE hStrCache);


/**
 * Enters a string into the cache.
 *
 * @returns Pointer to a read-only copy of the string.
 *
 * @param   hStrCache           Handle to the string cache.
 * @param   pchString           Pointer to a string. This does not need to be
 *                              zero terminated, but must not contain any zero
 *                              characters.
 * @param   cchString           The number of characters (bytes) to enter.
 *
 * @remarks It is implementation dependent whether the returned string pointer
 *          differs when entering the same string twice.
 */
RTDECL(const char *) RTStrCacheEnterN(RTSTRCACHE hStrCache, const char *pchString, size_t cchString);

/**
 * Enters a string into the cache.
 *
 * @returns Pointer to a read-only copy of the string.
 *
 * @param   hStrCache           Handle to the string cache.
 * @param   psz                 Pointer to a zero terminated string.
 *
 * @remarks See RTStrCacheEnterN.
 */
RTDECL(const char *) RTStrCacheEnter(RTSTRCACHE hStrCache, const char *psz);


/**
 * Retains a reference to a string.
 *
 * @returns The new reference count. UINT32_MAX is returned if the string
 *          pointer is invalid.
 */
RTDECL(uint32_t) RTStrCacheRetain(const char *psz);

/**
 * Releases a reference to a string.
 *
 * @returns The new reference count.
 *          UINT32_MAX is returned if the string pointer is invalid.
 *
 * @param   hStrCache           Handle to the string cache. Passing NIL is ok,
 *                              but this may come a performance hit.
 * @param   psz                 Pointer to a cached string.
 */
RTDECL(uint32_t) RTStrCacheRelease(RTSTRCACHE hStrCache, const char *psz);

/**
 * Gets the string length of a cache entry.
 *
 * @returns The string length. 0 if the string is invalid (asserted).
 *
 * @param   psz             Pointer to a cached string.
 */
RTDECL(size_t) RTStrCacheLength(const char *psz);

RT_C_DECLS_END

#endif

