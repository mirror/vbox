/* $Id$ */
/** @file
 * IPRT - String Manipulation.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include <iprt/string.h>
#include "internal/iprt.h"

#include <iprt/alloca.h>
#include <iprt/assert.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include "internal/string.h"



RTDECL(char *) RTStrAlloc(size_t cb)
{
    char *psz = (char *)RTMemAlloc(RT_MAX(cb, 1));
    if (psz)
        *psz = '\0';
    return psz;
}
RT_EXPORT_SYMBOL(RTStrAlloc);


RTDECL(int) RTStrAllocEx(char **ppsz, size_t cb)
{
    char *psz = *ppsz = (char *)RTMemAlloc(RT_MAX(cb, 1));
    if (psz)
    {
        *psz = '\0';
        return VINF_SUCCESS;
    }
    return VERR_NO_STR_MEMORY;
}
RT_EXPORT_SYMBOL(RTStrAlloc);


RTDECL(int) RTStrRealloc(char **ppsz, size_t cbNew)
{
    char *pszOld = *ppsz;
    if (!cbNew)
    {
        RTMemFree(pszOld);
        *ppsz = NULL;
    }
    else if (pszOld)
    {
        char *pszNew = (char *)RTMemRealloc(pszOld, cbNew);
        if (!pszNew)
            return VERR_NO_STR_MEMORY;
        pszNew[cbNew - 1] = '\0';
        *ppsz = pszNew;
    }
    else
    {
        char *pszNew = (char *)RTMemAlloc(cbNew);
        if (!pszNew)
            return VERR_NO_STR_MEMORY;
        pszNew[0] = '\0';
        pszNew[cbNew - 1] = '\0';
        *ppsz = pszNew;
    }
    return VINF_SUCCESS;
}

RTDECL(void)  RTStrFree(char *pszString)
{
    if (pszString)
        RTMemTmpFree(pszString);
}
RT_EXPORT_SYMBOL(RTStrFree);


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


RTDECL(char *) RTStrDupN(const char *pszString, size_t cchMax)
{
    AssertPtr(pszString);
    char  *pszEnd = (char *)memchr(pszString, '\0', cchMax);
    size_t cch    = pszEnd ? (uintptr_t)pszEnd - (uintptr_t)pszString : cchMax;
    char  *pszDst = (char *)RTMemAlloc(cch + 1);
    if (pszDst)
    {
        memcpy(pszDst, pszString, cch);
        pszDst[cch] = '\0';
    }
    return pszDst;
}
RT_EXPORT_SYMBOL(RTStrDupN);


RTDECL(int) RTStrAAppend(char **ppsz, const char *pszAppend)
{
    if (!pszAppend)
        return VINF_SUCCESS;
    return RTStrAAppendN(ppsz, pszAppend, RTSTR_MAX);
}


RTDECL(int) RTStrAAppendN(char **ppsz, const char *pszAppend, size_t cchAppend)
{
    if (!cchAppend)
        return VINF_SUCCESS;
    if (cchAppend == RTSTR_MAX)
        cchAppend = strlen(pszAppend);
    else
        Assert(cchAppend == RTStrNLen(pszAppend, cchAppend));

    size_t const cchOrg = *ppsz ? strlen(*ppsz) : 0;
    char *pszNew = (char *)RTMemRealloc(*ppsz, cchOrg + cchAppend + 1);
    if (!pszNew)
        return VERR_NO_STR_MEMORY;

    memcpy(&pszNew[cchOrg], pszAppend, cchAppend);
    pszNew[cchOrg + cchAppend] = '\0';

    *ppsz = pszNew;
    return VINF_SUCCESS;
}


RTDECL(int) RTStrAAppendExNV(char **ppsz, size_t cPairs, va_list va)
{
    AssertPtr(ppsz);
    if (!cPairs)
        return VINF_SUCCESS;

    /*
     * Determin the length of each string and calc the new total.
     */
    struct RTStrAAppendExNVStruct
    {
        const char *psz;
        size_t      cch;
    } *paPairs = (struct RTStrAAppendExNVStruct *)alloca(cPairs * sizeof(*paPairs));
    AssertReturn(paPairs, VERR_NO_STR_MEMORY);

    size_t  cchOrg      = *ppsz ? strlen(*ppsz) : 0;
    size_t  cchNewTotal = cchOrg;
    for (size_t i = 0; i < cPairs; i++)
    {
        const char *psz = va_arg(va, const char *);
        size_t      cch = va_arg(va, size_t);
        AssertPtrNull(psz);
        Assert(cch == RTSTR_MAX || cch == RTStrNLen(psz, cch));

        if (cch == RTSTR_MAX)
            cch = psz ? strlen(psz) : 0;
        cchNewTotal += cch;

        paPairs[i].cch = cch;
        paPairs[i].psz = psz;
    }
    cchNewTotal++;                      /* '\0' */

    /*
     * Try reallocate the string.
     */
    char *pszNew = (char *)RTMemRealloc(*ppsz, cchNewTotal);
    if (!pszNew)
        return VERR_NO_STR_MEMORY;

    /*
     * Do the appending.
     */
    size_t off = cchOrg;
    for (size_t i = 0; i < cPairs; i++)
    {
        memcpy(&pszNew[off], paPairs[i].psz, paPairs[i].cch);
        off += paPairs[i].cch;
    }
    Assert(off + 1 == cchNewTotal);
    pszNew[off] = '\0';

    /* done */
    *ppsz = pszNew;
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTStrAAppendExNV);


RTDECL(int) RTStrAAppendExN(char **ppsz, size_t cPairs, ...)
{
    va_list va;
    va_start(va, cPairs);
    int rc = RTStrAAppendExNV(ppsz, cPairs, va);
    va_end(va);
    return rc;
}
RT_EXPORT_SYMBOL(RTStrAAppendExN);


RTDECL(int) RTStrATruncate(char **ppsz, size_t cchNew)
{
    char *pszOld = *ppsz;
    if (!cchNew)
    {
        if (pszOld && *pszOld)
        {
            *pszOld = '\0';
            char *pszNew = (char *)RTMemRealloc(pszOld, 1);
            if (pszNew)
                *ppsz = pszNew;
        }
    }
    else
    {
        AssertPtrReturn(pszOld, VERR_OUT_OF_RANGE);
        char *pszZero = (char *)memchr(pszOld, '\0', cchNew + 63);
        AssertReturn(!pszZero || (size_t)(pszZero - pszOld) >= cchNew, VERR_OUT_OF_RANGE);
        pszOld[cchNew] = '\0';
        if (!pszZero)
        {
            char *pszNew = (char *)RTMemRealloc(pszOld,  cchNew + 1);
            if (pszNew)
                *ppsz = pszNew;
        }
    }
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTStrATruncate);

