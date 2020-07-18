/* $Id$ */
/** @file
 * IPRT - RTPathFindCommon implementations.
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
#include "internal/iprt.h"
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/path.h>


RTDECL(size_t) RTPathFindCommonEx(size_t cPaths, const char * const *papszPaths, uint32_t fFlags)
{
    AssertReturn(cPaths > 0, 0);
    AssertPtrReturn(papszPaths, 0);
    AssertReturn(RTPATH_STR_F_IS_VALID(fFlags, 0), 0);

    /** @todo r=bird: Extremely naive code.
     * - The original idea of taking either '/' or '\\' as separators is very out of
     *   touch with the rest of path.h.  On DOS based systems we need to handle both
     *   of those as well as ':'.
     * - Why compare pszRef with itself?
     * - Why derefernece pszRef[cch] for each other path.
     * - Why perform NULL checks for each outer iteration.
     * - Why perform '\0' check before comparing with pszRef[cch]?
     *   It's sufficient to check if pszRef[cch] is '\0'.
     * - Why backtrack to the last path separator?  It won't return the expected
     *   result for cPaths=1, unless the path ends with a separator.
     * - Multiple consequtive path separators must be treated as a single one (most
     *   of the time anyways - UNC crap).
     */
    const char *pszRef = papszPaths[0]; /* The reference we're comparing with. */
    const char chNaiveSep = (fFlags & RTPATH_STR_F_STYLE_MASK) == RTPATH_STR_F_STYLE_HOST
                          ? RTPATH_SLASH
                          : (fFlags & RTPATH_STR_F_STYLE_MASK) == RTPATH_STR_F_STYLE_DOS ? '\\' : '/';

    size_t cch = 0;
    do
    {
        for (size_t i = 0; i < cPaths; ++i)
        {
            const char *pcszPath = papszPaths[i];
            if (   pcszPath
                && pcszPath[cch]
                && pcszPath[cch] == pszRef[cch])
                continue;

            while (   cch
                   && pszRef[--cch] != chNaiveSep) { }

            return cch ? cch + 1 : 0;
        }
    } while (++cch);

    return 0;
}
RT_EXPORT_SYMBOL(RTPathFindCommonEx);


RTDECL(size_t) RTPathFindCommon(size_t cPaths, const char * const *papszPaths)
{
    return RTPathFindCommonEx(cPaths, papszPaths, RTPATH_STR_F_STYLE_HOST);
}
RT_EXPORT_SYMBOL(RTPathFindCommon);

