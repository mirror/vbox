/* $Id$ */
/** @file
 * IPRT - RTPathCreateRelative.
 */

/*
 * Copyright (C) 2013-2019 Oracle Corporation
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
#include <iprt/path.h>

#include <iprt/assert.h>
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS
# include <iprt/ctype.h>
#endif
#include <iprt/err.h>
#include <iprt/string.h>
#include "internal/path.h"



RTDECL(int) RTPathCalcRelative(char *pszPathDst, size_t cbPathDst,
                               const char *pszPathFrom, bool fFromFile,
                               const char *pszPathTo)
{
    AssertPtrReturn(pszPathDst, VERR_INVALID_POINTER);
    AssertReturn(cbPathDst, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pszPathFrom, VERR_INVALID_POINTER);
    AssertPtrReturn(pszPathTo, VERR_INVALID_POINTER);

    /*
     * Check for different root specifiers (drive letters), creating a relative path doesn't work here.
     */
    size_t offRootFrom = rtPathRootSpecLen(pszPathFrom);
    AssertReturn(offRootFrom > 0, VERR_INVALID_PARAMETER);

    size_t offRootTo   = rtPathRootSpecLen(pszPathTo);
    AssertReturn(offRootTo > 0, VERR_INVALID_PARAMETER);


    /** @todo correctly deal with extra root slashes! */
    if (offRootFrom != offRootTo)
        return VERR_NOT_SUPPORTED;

#if RTPATH_STYLE != RTPATH_STR_F_STYLE_DOS
    if (RTStrNCmp(pszPathFrom, pszPathTo, offRootFrom))
        return VERR_NOT_SUPPORTED;
#else
    if (RTStrNICmp(pszPathFrom, pszPathTo, offRootFrom))
        for (size_t off = 0; off < offRootFrom; off++)
        {
            char const chFrom = pszPathFrom[off];
            char const chTo   = pszPathTo[off];
            if (   chFrom != chTo
                && RT_C_TO_LOWER(chFrom) != RT_C_TO_LOWER(chTo) /** @todo proper case insensitivity! */
                && (!RTPATH_IS_SLASH(chFrom) || !RTPATH_IS_SLASH(chTo)) )
                return VERR_NOT_SUPPORTED;
        }
#endif

    pszPathFrom += offRootFrom;
    pszPathTo   += offRootTo;

    /*
     * Skip out the part of the path which is equal to both.
     */
    const char *pszStartOfFromComp = pszPathFrom;
    for (;;)
    {
        char const chFrom = *pszPathFrom;
        char const chTo   = *pszPathTo;
        if (!RTPATH_IS_SLASH(chFrom))
        {
            if (chFrom == chTo)
            {
                if (chFrom)
                { /* likely */ }
                else
                {
                    /* Special case: The two paths are equal. */
                    if (fFromFile)
                    {
                        size_t cchComp = pszPathFrom - pszStartOfFromComp;
                        if (cchComp < cbPathDst)
                        {
                            memcpy(pszPathDst, pszStartOfFromComp, cchComp);
                            pszPathDst[cchComp] = '\0';
                            return VINF_SUCCESS;
                        }
                    }
                    else if (sizeof(".") <= cbPathDst)
                    {
                        memcpy(pszPathDst, ".", sizeof("."));
                        return VINF_SUCCESS;
                    }
                    return VERR_BUFFER_OVERFLOW;
                }
            }
#if RTPATH_STYLE == RTPATH_STR_F_STYLE_DOS
            else if (RT_C_TO_LOWER(chFrom) == RT_C_TO_LOWER(chTo))
            { /* if not likely, then simpler code structure wise. */ }
#endif
            else if (chFrom != '\0' || !RTPATH_IS_SLASH(chTo) || fFromFile)
                break;
            else
            {
                pszStartOfFromComp = pszPathFrom;
                do
                    pszPathTo++;
                while (RTPATH_IS_SLASH(*pszPathTo));
                break;
            }
            pszPathFrom++;
            pszPathTo++;
        }
        else if (RTPATH_IS_SLASH(chTo))
        {
            /* Both have slashes.  Skip any additional ones before taking down
               the start of the component for rewinding purposes. */
            do
                pszPathTo++;
            while (RTPATH_IS_SLASH(*pszPathTo));
            do
                pszPathFrom++;
            while (RTPATH_IS_SLASH(*pszPathFrom));
            pszStartOfFromComp = pszPathFrom;
        }
        else
            break;
    }

    /* Rewind to the start of the current component. */
    pszPathTo  -= pszPathFrom - pszStartOfFromComp;
    pszPathFrom = pszStartOfFromComp;

    /* Paths point to the first non equal component now. */

    /*
     * Constructure the relative path.
     */

    /* Create the part to go up from pszPathFrom. */
    unsigned offDst = 0;

    if (!fFromFile && *pszPathFrom != '\0')
    {
        if (offDst + 3 < cbPathDst)
        {
            pszPathDst[offDst++] = '.';
            pszPathDst[offDst++] = '.';
            pszPathDst[offDst++] = RTPATH_SLASH;
        }
        else
            return VERR_BUFFER_OVERFLOW;
    }

    while (*pszPathFrom != '\0')
    {
        char ch;
        while (   (ch = *pszPathFrom) != '\0'
               && !RTPATH_IS_SLASH(*pszPathFrom))
            pszPathFrom++;
        while (   (ch = *pszPathFrom) != '\0'
               && RTPATH_IS_SLASH(ch))
            pszPathFrom++;
        if (!ch)
            break;

        if (offDst + 3 < cbPathDst)
        {
            pszPathDst[offDst++] = '.';
            pszPathDst[offDst++] = '.';
            pszPathDst[offDst++] = RTPATH_SLASH;
        }
        else
            return VERR_BUFFER_OVERFLOW;
    }

    /* Now append the rest of pszPathTo to the final path. */
    size_t cchTo = strlen(pszPathTo);
    if (offDst + cchTo <= cbPathDst)
    {
        memcpy(&pszPathDst[offDst], pszPathTo, cchTo);
        pszPathDst[offDst + cchTo] = '\0';
        return VINF_SUCCESS;
    }
    return VERR_BUFFER_OVERFLOW;
}

