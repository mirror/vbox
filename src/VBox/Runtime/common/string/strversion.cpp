/* $Id$ */
/** @file
 * IPRT - Version String Parsing.
 */

/*
 * Copyright (C) 2009 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/string.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/ctype.h>
#include <iprt/err.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
#define RTSTRVER_IS_PUNCTUACTION(ch)    \
    ( (ch) == '_' || (ch) == '-' || (ch) == '+' || RT_C_IS_PUNCT(ch) )


/**
 * Parses a out the next block from a version string.
 *
 * @returns true if numeric, false if not.
 * @param   ppszVer             The string cursor, IN/OUT.
 * @param   pi32Value           Where to return the value if numeric.
 * @param   pcchBlock           Where to return the block length.
 */
static bool rtStrVersionParseBlock(const char **ppszVer, int32_t *pi32Value, size_t *pcchBlock)
{
    const char *psz = *ppszVer;

    /* Check for end-of-string. */
    if (!*psz)
    {
        *pi32Value = 0;
        *pcchBlock = 0;
        return false;
    }

    bool fNumeric = RT_C_IS_DIGIT(*psz);
    if (fNumeric)
    {
        do
            psz++;
        while (*psz && RT_C_IS_DIGIT(*psz));

        char *pszNext;
        int rc = RTStrToInt32Ex(*ppszVer, &pszNext, 10, pi32Value);
        AssertRC(rc);
        Assert(pszNext == psz);
        if (RT_FAILURE(rc) || rc == VWRN_NUMBER_TOO_BIG)
        {
            fNumeric = false;
            *pi32Value = 0;
        }
        *pcchBlock = psz - *ppszVer;
    }
    else
    {
        do
            psz++;
        while (*psz && !RT_C_IS_DIGIT(*psz) && !RTSTRVER_IS_PUNCTUACTION(*psz));
        *pcchBlock = psz - *ppszVer;

        /* Translate standard pre release terms to negative values. */
        if (   *pcchBlock == 2
            && !RTStrNICmp(*ppszVer, "RC", 4))
            *pi32Value = -100;
        else if (   *pcchBlock == 3
            && !RTStrNICmp(*ppszVer, "PRE", 3))
            *pi32Value = -200;
        else if (   *pcchBlock == 5
            && !RTStrNICmp(*ppszVer, "GAMMA", 5))
            *pi32Value = -300;
        else if (   *pcchBlock == 4
            && !RTStrNICmp(*ppszVer, "BETA", 4))
            *pi32Value = -400;
        else if (   *pcchBlock == 5
                 && !RTStrNICmp(*ppszVer, "ALPHA", 4))
            *pi32Value = -500;
        else
            *pi32Value = 0;
        if (*pi32Value < 0)
        {
            /* Trailing number, if so add it? */
        }
    }

    /* skip punctuation */
    if (RTSTRVER_IS_PUNCTUACTION(*psz))
        psz++;
    *ppszVer = psz;

    return fNumeric;
}


RTDECL(int) RTStrVersionCompare(const char *pszVer1, const char *pszVer2)
{
    AssertPtr(pszVer1);
    AssertPtr(pszVer2);

    /*
     * Do a parallel parse of the strings.
     */
    while (*pszVer1 || *pszVer2)
    {
        const char *pszBlock1 = pszVer1;
        size_t      cchBlock1;
        int32_t     iVal1;
        bool        fNumeric1 = rtStrVersionParseBlock(&pszVer1, &iVal1, &cchBlock1);

        const char *pszBlock2 = pszVer2;
        size_t      cchBlock2;
        int32_t     iVal2;
        bool        fNumeric2 = rtStrVersionParseBlock(&pszVer2, &iVal2, &cchBlock2);

        if (fNumeric1 && fNumeric2)
        {
            if (iVal1 != iVal2)
                return iVal1 < iVal2 ? -1 : 1;
        }
        else if (   !fNumeric1 && fNumeric2 && iVal2 <= 0 && cchBlock1 == 0
                 || !fNumeric2 && fNumeric1 && iVal1 <= 0 && cchBlock2 == 0
                )
        {
            /* 1.0 == 1.0.0.0.0. */;
        }
        else
        {
            int iDiff = RTStrNICmp(pszBlock1, pszBlock2, RT_MIN(cchBlock1, cchBlock2));
            if (!iDiff && cchBlock1 != cchBlock2)
                iDiff = cchBlock1 < cchBlock2 ? -1 : 1;
            if (iDiff)
            {
                /*
                 * Special hacks for dealing with 3.1.0_BETA1-r99 vs 3.1.0r99.
                 * Note that 3.1.0_BETA1 vs 3.0.1 is handled above with help of
                 * negative values returned by the parser.
                 */
/** @todo finish at home */

                return iDiff < 0 ? -1 : 1;
            }
        }
    }
    return 0;
}
RT_EXPORT_SYMBOL(RTStrVersionCompare);
