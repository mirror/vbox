/* $Id$ */
/** @file
 * IPRT - String Cache, stub implementation.
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
#include <iprt/strcache.h>

#include <iprt/asm.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mempool.h>
#include <iprt/string.h>


RTDECL(int) RTStrCacheCreate(PRTSTRCACHE phStrCache, const char *pszName)
{
    AssertCompile(sizeof(RTSTRCACHE) == sizeof(RTMEMPOOL));
    AssertCompile(NIL_RTSTRCACHE == (RTSTRCACHE)NIL_RTMEMPOOL);
    AssertCompile(RTSTRCACHE_DEFAULT == (RTSTRCACHE)RTMEMPOOL_DEFAULT);
    return RTMemPoolCreate((PRTMEMPOOL)phStrCache, pszName);
}


RTDECL(int) RTStrCacheDestroy(RTSTRCACHE hStrCache)
{
    if (hStrCache == NIL_RTSTRCACHE)
        return VINF_SUCCESS;
    return VERR_INVALID_HANDLE;
}


RTDECL(const char *) RTStrCacheEnterN(RTSTRCACHE hStrCache, const char *pchString, size_t cchString)
{
    AssertPtr(pchString);
    AssertReturn(cchString < _1G, NULL);
    Assert(!memchr(pchString, '\0', cchString));

    return (const char *)RTMemPoolDupEx((RTMEMPOOL)hStrCache, pchString, cchString, 1);
}


RTDECL(const char *) RTStrCacheEnter(RTSTRCACHE hStrCache, const char *psz)
{
    return RTStrCacheEnterN(hStrCache, psz, strlen(psz));
}


RTDECL(uint32_t) RTStrCacheRetain(const char *psz)
{
    AssertPtr(psz);
    return RTMemPoolRetain((void *)psz);
}


RTDECL(uint32_t) RTStrCacheRelease(RTSTRCACHE hStrCache, const char *psz)
{
    if (!psz)
        return 0;
    return RTMemPoolRelease((RTMEMPOOL)hStrCache, (void *)psz);
}


RTDECL(size_t) RTStrCacheLength(const char *psz)
{
    if (!psz)
        return 0;
    return strlen(psz);
}

