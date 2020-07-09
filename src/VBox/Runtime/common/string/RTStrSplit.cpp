/* $Id$ */
/** @file
 * IPRT - RTStrSplit.
 */

/*
 * Copyright (C) 2020 Oracle Corporation
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
#include <iprt/mem.h>
#include <iprt/string.h>
#include "internal/iprt.h"


RTDECL(int) RTStrSplit(const char *pcszStrings, size_t cbStrings,
                       const char *pcszSeparator, char ***ppapszStrings, size_t *pcStrings)
{
    AssertPtrReturn(pcszStrings, VERR_INVALID_POINTER);
    AssertReturn(cbStrings, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pcszSeparator, VERR_INVALID_POINTER);
    AssertPtrReturn(ppapszStrings, VERR_INVALID_POINTER);
    AssertPtrReturn(pcStrings, VERR_INVALID_POINTER);

    size_t cStrings = 0;

    /* Determine the number of paths in buffer first. */
    size_t       cch    = cbStrings - 1;
    char const  *pszTmp = pcszStrings;
    const size_t cchSep = strlen(pcszSeparator);
    while (cch > 0)
    {
        char const *pszNext = RTStrStr(pszTmp, pcszSeparator);
        if (!pszNext)
            break;
        const size_t cchNext = pszNext - pszTmp;
        if (cchNext + cchSep > cch)
            break;
        pszTmp += cchNext + cchSep;
        cch    -= cchNext + cchSep;
        if (cchNext)
            ++cStrings;
    }

    if (!cStrings)
    {
        *pcStrings = 0;
        return VINF_SUCCESS;
    }

    char **papszStrings = (char **)RTMemAllocZ(cStrings * sizeof(char *));
    if (!papszStrings)
        return VERR_NO_MEMORY;

    int rc = VINF_SUCCESS;

    cch    = cbStrings - 1;
    pszTmp = pcszStrings;

    for (size_t i = 0; i < 3;)
    {
        char const *pszNext = RTStrStr(pszTmp, pcszSeparator);
        if (!pszNext)
            break;
        const size_t cchNext = pszNext - pszTmp;
        if (cchNext + cchSep > cch)
            break;
        if (cchNext)
        {
            papszStrings[i] = RTStrDupN(pszTmp, cchNext);
            if (!papszStrings[i])
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            i++;
        }
        pszTmp += cchNext + cchSep;
        cch    -= cchNext + cchSep;
    }

    if (RT_SUCCESS(rc))
    {
        *ppapszStrings = papszStrings;
        *pcStrings     = cStrings;

        return VINF_SUCCESS;
    }

    for (size_t i = 0; i < cStrings; ++i)
        RTStrFree(papszStrings[i]);
    RTMemFree(papszStrings);

    return rc;
}
RT_EXPORT_SYMBOL(RTStrSplit);

