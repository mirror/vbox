/* $Id$ */
/** @file
 * IPRT - String Manipulation.
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
#include <iprt/string.h>
#include "internal/iprt.h"

#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/string.h"


/**
 * Free string allocated by any of the non-UCS-2 string functions.
 *
 * @returns iprt status code.
 * @param   pszString      Pointer to buffer with string to free.
 *                         NULL is accepted.
 */
RTDECL(void)  RTStrFree(char *pszString)
{
    if (pszString)
        RTMemTmpFree(pszString);
}
RT_EXPORT_SYMBOL(RTStrFree);


/**
 * Allocates a new copy of the given UTF-8 string.
 *
 * @returns Pointer to the allocated UTF-8 string.
 * @param   pszString       UTF-8 string to duplicate.
 */
RTDECL(char *) RTStrDup(const char *pszString)
{
    AssertPtr(pszString);
    size_t cch = strlen(pszString) + 1;
    char *psz = (char *)RTMemAlloc(cch);
    if (psz)
        memcpy(psz, pszString, cch);
    return psz;
}
RT_EXPORT_SYMBOL(RTStrDup);


/**
 * Allocates a new copy of the given UTF-8 string.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of the allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to duplicate.
 */
RTDECL(int)  RTStrDupEx(char **ppszString, const char *pszString)
{
    AssertPtr(ppszString);
    AssertPtr(pszString);

    size_t cch = strlen(pszString) + 1;
    char *psz = (char *)RTMemAlloc(cch);
    if (psz)
    {
        memcpy(psz, pszString, cch);
        *ppszString = psz;
        return VINF_SUCCESS;
    }
    return VERR_NO_MEMORY;
}
RT_EXPORT_SYMBOL(RTStrDupEx);


/**
 * Allocates a new copy of the given UTF-8 substring.
 *
 * @returns Pointer to the allocated UTF-8 substring.
 * @param   pszString       UTF-8 string to duplicate.
 * @param   cchMax          The max number of chars to duplicate, not counting
 *                          the terminator.
 */
RTDECL(char *) RTStrDupN(const char *pszString, size_t cchMax)
{
    AssertPtr(pszString);
    char  *pszEnd = (char *)memchr(pszString, '\0', cchMax);
    size_t cch    = pszEnd ? (uintptr_t)pszEnd - (uintptr_t)pszString : cchMax;
    char  *pszDst = (char *)RTMemAlloc(cch);
    if (pszDst)
    {
        memcpy(pszDst, pszString, cch);
        pszDst[cch] = '\0';
    }
    return pszDst;
}
RT_EXPORT_SYMBOL(RTStrDupN);



