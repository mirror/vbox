/* $Id$ */
/** @file
 * VBoxGuestR3Lib - Ring-3 Support Library for VirtualBox guest additions,
 *                  implements the minimum of runtime functions needed for
 *                  XFree86 driver code.
 */

/*
 * Copyright (C) 2007-2010 Oracle Corporation
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


/****************************************************************************
*   Header Files                                                            *
****************************************************************************/
#include <iprt/assert.h>
#include <iprt/log.h>
#include <iprt/mem.h>
#include <iprt/string.h>
extern "C" {
# define XFree86LOADER
# include <xf86_ansic.h>
# include <errno.h>
# undef size_t
}

/* This is risky as it restricts call to the ANSI format type specifiers. */
RTDECL(size_t) RTStrPrintf(char *pszBuffer, size_t cchBuffer, const char *pszFormat, ...)
{
    va_list args;
    int cbRet;
    va_start(args, pszFormat);
    cbRet = xf86vsnprintf(pszBuffer, cchBuffer, pszFormat, args);
    va_end(args);
    return cbRet >= 0 ? cbRet : 0;
}

RTDECL(int) RTStrToUInt32Ex(const char *pszValue, char **ppszNext, unsigned uBase, uint32_t *pu32)
{
    char *pszNext = NULL;
    xf86errno = 0;
    unsigned long ul = xf86strtoul(pszValue, &pszNext, uBase);
    if (ppszNext)
        *ppszNext = pszNext;
    if (RT_UNLIKELY(pszValue == pszNext))
        return VERR_NO_DIGITS;
    if (RT_UNLIKELY(ul > UINT32_MAX))
        ul = UINT32_MAX;
    if (pu32)
        *pu32 = (uint32_t) ul;
    if (RT_UNLIKELY(xf86errno == EINVAL))
        return VERR_INVALID_PARAMETER;
    if (RT_UNLIKELY(xf86errno == ERANGE))
        return VWRN_NUMBER_TOO_BIG;
    if (RT_UNLIKELY(xf86errno))
        /* RTErrConvertFromErrno() is not available */
        return VERR_UNRESOLVED_ERROR;
    if (RT_UNLIKELY(*pszValue == '-'))
        return VWRN_NEGATIVE_UNSIGNED;
    if (RT_UNLIKELY(*pszNext))
    {
        while (*pszNext)
            if (!xf86isspace(*pszNext))
                return VWRN_TRAILING_CHARS;
        return VWRN_TRAILING_SPACES;
    }
    return VINF_SUCCESS;
}

RTDECL(int) RTStrToUInt32Full(const char *pszValue, unsigned uBase, uint32_t *pu32)
{
    char *psz;
    int rc = RTStrToUInt32Ex(pszValue, &psz, uBase, pu32);
    if (RT_SUCCESS(rc) && *psz)
        if (rc == VWRN_TRAILING_CHARS || rc == VWRN_TRAILING_SPACES)
            rc = -rc;
    return rc;
}

RTDECL(void)    RTAssertMsg1Weak(const char *pszExpr, unsigned uLine, const char *pszFile, const char *pszFunction)
{
    ErrorF("Assertion failed!  Expression: %s at %s in\n", pszExpr,
           pszFunction);
    ErrorF("%s:%u\n", pszFile, uLine);
}

RTDECL(void)    RTAssertMsg2Weak(const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    VErrorF(pszFormat, args);
    va_end(args);
}

RTDECL(bool)    RTAssertShouldPanic(void)
{
    return false;
}

RTDECL(PRTLOGGER) RTLogRelDefaultInstance(void)
{
    return NULL;
}

RTDECL(void) RTLogLoggerEx(PRTLOGGER, unsigned, unsigned, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    VErrorF(pszFormat, args);
    va_end(args);
}

RTDECL(void *)  RTMemTmpAlloc(size_t cb)
{
    return xalloc(cb);
}

RTDECL(void)    RTMemTmpFree(void *pv)
{
    xfree(pv);
}

