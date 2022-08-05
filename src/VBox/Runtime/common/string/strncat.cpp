/* $Id$ */
/** @file
 * IPRT - No-CRT Strings, strncat().
 */

/*
 * Copyright (C) 2006-2022 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/string.h>


/**
 * Append a substring to an existing string.
 *
 * @returns Pointer to destination string
 * @param   pszDst      String to append @a cchMaxSrc chars from @a pszSrc to,
 *                      plus a zero terminator.
 * @param   pszSrc      Zero terminated string.
 * @param   cchMaxSrc   Maximum number of chars to copy from @a pszSrc.
 */
#undef strncat
char *RT_NOCRT(strncat)(char *pszDst, const char *pszSrc, size_t cchMaxSrc)
{
    char * const pszRet = pszDst;

    pszDst = RTStrEnd(pszDst, RTSTR_MAX);

    char ch;
    while (cchMaxSrc-- > 0 && (ch = *pszSrc++) != '\0')
        *pszDst++ = ch;
    *pszDst = '\0';

    return pszRet;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strncat);

