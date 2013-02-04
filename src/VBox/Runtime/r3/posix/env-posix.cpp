/* $Id$ */
/** @file
 * IPRT - Environment, Posix.
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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
#ifdef RT_OS_DARWIN
/* pick the correct prototype for unsetenv. */
# define _POSIX_C_SOURCE 1
#endif
#include <iprt/env.h>
#include <iprt/string.h>
#include <iprt/alloca.h>
#include <iprt/assert.h>
#if defined(DEBUG) && defined(RT_OS_LINUX)
# include <iprt/asm.h>
#endif

#include <stdlib.h>
#include <errno.h>

#include "internal/alignmentchecks.h"


RTDECL(bool) RTEnvExist(const char *pszVar)
{
    return RTEnvGet(pszVar) != NULL;
}


RTDECL(const char *) RTEnvGet(const char *pszVar)
{
    IPRT_ALIGNMENT_CHECKS_DISABLE(); /* glibc causes trouble */
    const char *pszValue = getenv(pszVar);
    IPRT_ALIGNMENT_CHECKS_ENABLE();
    return pszValue;
}


RTDECL(int) RTEnvPut(const char *pszVarEqualValue)
{
    /** @todo putenv is a source memory leaks. deal with this on a per system basis. */
    if (!putenv((char *)pszVarEqualValue))
        return 0;
    return RTErrConvertFromErrno(errno);
}

RTDECL(int) RTEnvSet(const char *pszVar, const char *pszValue)
{
#if defined(_MSC_VER)
    /* make a local copy and feed it to putenv. */
    const size_t cchVar = strlen(pszVar);
    const size_t cchValue = strlen(pszValue);
    char *pszTmp = (char *)alloca(cchVar + cchValue + 2 + !*pszValue);
    memcpy(pszTmp, pszVar, cchVar);
    pszTmp[cchVar] = '=';
    if (*pszValue)
        memcpy(pszTmp + cchVar + 1, pszValue, cchValue + 1);
    else
    {
        pszTmp[cchVar + 1] = ' '; /* wrong, but putenv will remove it otherwise. */
        pszTmp[cchVar + 2] = '\0';
    }

    if (!putenv(pszTmp))
        return 0;
    return RTErrConvertFromErrno(errno);

#else
    if (!setenv(pszVar, pszValue, 1))
        return VINF_SUCCESS;
    return RTErrConvertFromErrno(errno);
#endif
}


RTDECL(int) RTEnvUnset(const char *pszVar)
{
    AssertReturn(!strchr(pszVar, '='), VERR_INVALID_PARAMETER);

    /*
     * Check that it exists first.
     */
    if (!RTEnvExist(pszVar))
        return VINF_ENV_VAR_NOT_FOUND;

    /*
     * Ok, try remove it.
     */
#ifdef RT_OS_WINDOWS
    /* Use putenv(var=) since Windows does not have unsetenv(). */
    size_t cchVar = strlen(pszVar);
    char *pszBuf = (char *)alloca(cchVar + 2);
    memcpy(pszBuf, pszVar, cchVar);
    pszBuf[cchVar]     = '=';
    pszBuf[cchVar + 1] = '\0';

    if (!putenv(pszBuf))
        return VINF_SUCCESS;

#else
    /* This is the preferred function as putenv() like used above does neither work on Solaris nor on Darwin. */
    if (!unsetenv((char*)pszVar))
        return VINF_SUCCESS;
#endif

    return RTErrConvertFromErrno(errno);
}

