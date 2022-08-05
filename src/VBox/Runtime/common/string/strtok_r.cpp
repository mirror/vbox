/* $Id$ */
/** @file
 * IPRT - No-CRT Strings, strtok_r().
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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


#undef strtok_r
char *RT_NOCRT(strtok_r)(char *psz, const char *pszDelimiters, char **ppszState)
{
    /*
     * Load state.
     */
    if (!psz)
    {
        psz = *ppszState;
        if (!psz)
            return NULL;
    }

    /*
     * Skip leading delimiters.
     */
    size_t const cchDelimiters = strlen(pszDelimiters);
    for (;;)
    {
        char ch = *psz;
        if (!ch)
            return *ppszState = NULL;
        if (memchr(pszDelimiters, ch, cchDelimiters) == NULL)
            break;
        psz++;
    }

    /*
     * Find the end of the token.
     */
    char * const pszRet = psz;
    for (;;)
    {
        char ch = *++psz;
        if (memchr(pszDelimiters, ch, cchDelimiters + 1 /* '\0' */) == NULL)
        { /* semi-likely */ }
        else
        {
            *psz = '\0';
            *ppszState = ch ? psz + 1 : NULL;
            return pszRet;
        }
    }
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(strtok_r);

