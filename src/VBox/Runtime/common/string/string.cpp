/* $Id$ */
/** @file
 * innotek Portable Runtime - String Manipulation.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/string.h"

#include <locale.h>


/**
 * Init C runtime locale
 * note: actually where is no need in this global var, use it only for
 * auto run of setlocale() func.
 */
/** @todo rewrite this to do setlocale() from some proper init function. */
static int g_RTLocaleInited = (setlocale(LC_CTYPE, "") != NULL);


/**
 * Free string allocated by any of the non-UCS-2 string functions.
 *
 * @returns iprt status code.
 * @param   pszString      Pointer to buffer with string to free.
 *                         NULL is accepted.
 */
RTR3DECL(void)  RTStrFree(char *pszString)
{
    if (pszString)
        RTMemTmpFree(pszString);
}


/**
 * Allocates a new copy of the given UTF-8 string.
 *
 * @returns Pointer to the allocated UTF-8 string.
 * @param   pszString       UTF-8 string to duplicate.
 */
RTR3DECL(char *) RTStrDup(const char *pszString)
{
    Assert(VALID_PTR(pszString));
    size_t cch = strlen(pszString) + 1;
    char *psz = (char *)RTMemAlloc(cch);
    if (psz)
        memcpy(psz, pszString, cch);
    return psz;
}


/**
 * Allocates a new copy of the given UTF-8 string.
 *
 * @returns iprt status code.
 * @param   ppszString      Receives pointer of the allocated UTF-8 string.
 *                          The returned pointer must be freed using RTStrFree().
 * @param   pszString       UTF-8 string to duplicate.
 */
RTR3DECL(int)  RTStrDupEx(char **ppszString, const char *pszString)
{
    Assert(VALID_PTR(ppszString));
    Assert(VALID_PTR(pszString));

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

