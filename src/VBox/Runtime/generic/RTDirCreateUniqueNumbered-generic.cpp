/* $Id$ */
/** @file
 * IPRT - RTDirCreateUniqueNumbered, generic implementation.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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
#include <iprt/dir.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/string.h>

RTDECL(int) RTDirCreateUniqueNumbered(char *pszPath, size_t cbSize, RTFMODE fMode, size_t cchDigits, char chSep)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbSize, VERR_INVALID_PARAMETER);
    AssertReturn(cchDigits, VERR_INVALID_PARAMETER);
    /* Check for enough space. */
    char *pszEnd = strchr(pszPath, '\0');
    AssertReturn(cbSize - 1 - (pszEnd - pszPath) >= cchDigits + (chSep ? 1 : 0), VERR_BUFFER_OVERFLOW);

    /* First try is to create the path without any numbers. */
    int rc = RTDirCreate(pszPath, fMode);
    if (   RT_SUCCESS(rc)
        || rc != VERR_ALREADY_EXISTS)
        return rc;

    /* If the separator value isn't zero, add it. */
    if (chSep != '\0')
    {
        *pszEnd++ = chSep;
        *pszEnd   = '\0';
    }

    /* How many tries? */
    size_t cMaxTries = 10;
    for (size_t i = 0; i < cchDigits - 1; ++i)
        cMaxTries *= 10;

    /* Try cMaxTries - 1 counts to create a directory with the appended number. */
    size_t i = 1;
    while (i < cMaxTries)
    {
        /* Format the number with leading zero's. */
        rc = RTStrFormatNumber(pszEnd, i, 10, cchDigits, 0, RTSTR_F_WIDTH | RTSTR_F_ZEROPAD);
        if (RT_FAILURE(rc))
        {
            *pszPath = '\0';
            return rc;
        }
        rc = RTDirCreate(pszPath, fMode);
        if (RT_SUCCESS(rc))
            return rc;
        ++i;
    }

    /* We've given up. */
    *pszPath = '\0';
    return VERR_ALREADY_EXISTS;
}
RT_EXPORT_SYMBOL(RTDirCreateUniqueNumbered);

