/* $Id$ */
/** @file
 * IPRT - RTDirCreateUniqueNumbered, generic implementation.
 */

/*
 * Copyright (C) 2011-2016 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/dir.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/path.h>
#include <iprt/string.h>


RTDECL(int) RTDirCreateUniqueNumbered(char *pszPath, size_t cbSize, RTFMODE fMode, signed int cchDigits, char chSep)
{
    /*
     * Validate input.
     */
    AssertPtrReturn(pszPath, VERR_INVALID_POINTER);
    AssertReturn(cbSize, VERR_BUFFER_OVERFLOW);
    AssertReturn(cchDigits > 0, VERR_INVALID_PARAMETER);

    /* Check that there is sufficient space. */
    char *pszEnd = RTStrEnd(pszPath, cbSize);
    AssertReturn(pszEnd, VERR_BUFFER_OVERFLOW);
    AssertReturn(cbSize - 1 - (pszEnd - pszPath) >= (size_t)cchDigits + (chSep ? 1 : 0), VERR_BUFFER_OVERFLOW);
    size_t cbLeft = cbSize - (pszEnd - pszPath);

    /* First try is to create the path without any numbers. */
    int rc = RTDirCreate(pszPath, fMode, 0);
    if (   RT_SUCCESS(rc)
        || rc != VERR_ALREADY_EXISTS)
        return rc;

    /* If the separator value isn't zero, add it. */
    if (chSep != '\0')
    {
        cbLeft--;
        *pszEnd++ = chSep;
        *pszEnd   = '\0';
    }

    /* How many tries? Stay within somewhat sane limits. */
    uint32_t cMaxTries;
    if (cchDigits >= 8)
        cMaxTries = 100 * _1M;
    else
    {
        cMaxTries = 10;
        for (int a = 0; a < cchDigits - 1; ++a)
            cMaxTries *= 10;
    }

    /* Try cMaxTries - 1 times to create a directory with appended numbers. */
    uint32_t i = 1;
    while (i < cMaxTries)
    {
        /* Format the number with leading zero's. */
        ssize_t rc2 = RTStrFormatU32(pszEnd, cbLeft, i, 10, cchDigits, 0, RTSTR_F_WIDTH | RTSTR_F_ZEROPAD);
        if (RT_FAILURE((int) rc2))
        {
            *pszPath = '\0';
            return (int)rc2;
        }
        rc = RTDirCreate(pszPath, fMode, 0);
        if (RT_SUCCESS(rc))
            return rc;
        ++i;
    }

    /* We've given up. */
    *pszPath = '\0';
    return VERR_ALREADY_EXISTS;
}
RT_EXPORT_SYMBOL(RTDirCreateUniqueNumbered);

