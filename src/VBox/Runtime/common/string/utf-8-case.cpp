/* $Id$ */
/** @file
 * IPRT - UTF-8 Case Sensitivity and Folding.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

#include <iprt/uni.h>
#include <iprt/alloc.h>
#include <iprt/assert.h>
#include <iprt/err.h>
#include "internal/string.h"



/**
 * Performs a case insensitive string compare between two UTF-8 strings.
 *
 * This is a simplified compare, as only the simplified lower/upper case folding
 * specified by the unicode specs are used. It does not consider character pairs
 * as they are used in some languages, just simple upper & lower case compares.
 *
 * The result is the difference between the mismatching codepoints after they
 * both have been lower cased.
 *
 * If the string encoding is invalid the function will assert (strict builds)
 * and use RTStrCmp for the remainder of the string.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   psz1        First UTF-8 string. Null is allowed.
 * @param   psz2        Second UTF-8 string. Null is allowed.
 */
RTDECL(int) RTStrICmp(const char *psz1, const char *psz2)
{
    if (psz1 == psz2)
        return 0;
    if (!psz1)
        return -1;
    if (!psz2)
        return 1;

    const char *pszStart1 = psz1;
    for (;;)
    {
        /* Get the codepoints */
        RTUNICP cp1;
        int rc = RTStrGetCpEx(&psz1, &cp1);
        if (RT_FAILURE(rc))
        {
            AssertRC(rc);
            psz1--;
            break;
        }

        RTUNICP cp2;
        rc = RTStrGetCpEx(&psz2, &cp2);
        if (RT_FAILURE(rc))
        {
            AssertRC(rc);
            psz2--;
            psz1 = RTStrPrevCp(pszStart1, psz1);
            break;
        }

        /* compare */
        int iDiff = cp1 - cp2;
        if (iDiff)
        {
            iDiff = RTUniCpToUpper(cp1) != RTUniCpToUpper(cp2);
            if (iDiff)
            {
                iDiff = RTUniCpToLower(cp1) - RTUniCpToLower(cp2); /* lower case diff last! */
                if (iDiff)
                    return iDiff;
            }
        }

        /* hit the terminator? */
        if (!cp1)
            return 0;
    }

    /* Hit some bad encoding, continue in case insensitive mode. */
    return RTStrCmp(psz1, psz2);
}
RT_EXPORT_SYMBOL(RTStrICmp);


/**
 * Performs a case insensitive string compare between two UTF-8 strings, given a
 * maximum string length.
 *
 * This is a simplified compare, as only the simplified lower/upper case folding
 * specified by the unicode specs are used. It does not consider character pairs
 * as they are used in some languages, just simple upper & lower case compares.
 *
 * The result is the difference between the mismatching codepoints after they
 * both have been lower cased.
 *
 * If the string encoding is invalid the function will assert (strict builds)
 * and use RTStrCmp for the remainder of the string.
 *
 * @returns < 0 if the first string less than the second string.
 * @returns 0 if the first string identical to the second string.
 * @returns > 0 if the first string greater than the second string.
 * @param   psz1        First UTF-8 string. Null is allowed.
 * @param   psz2        Second UTF-8 string. Null is allowed.
 * @param   cchMax      Maximum string length
 */
RTDECL(int) RTStrNICmp(const char *psz1, const char *psz2, size_t cchMax)
{
    if (cchMax == 0)
        return 0;
    if (psz1 == psz2)
        return 0;
    if (!psz1)
        return -1;
    if (!psz2)
        return 1;

    for (;;)
    {
        /* Get the codepoints */
        RTUNICP cp1;
        size_t cchMax2 = cchMax;
        int rc = RTStrGetCpNEx(&psz1, &cchMax, &cp1);
        if (RT_FAILURE(rc))
        {
            AssertRC(rc);
            psz1--;
            cchMax++;
            break;
        }

        RTUNICP cp2;
        rc = RTStrGetCpNEx(&psz2, &cchMax2, &cp2);
        if (RT_FAILURE(rc))
        {
            AssertRC(rc);
            psz2--;
            psz1 -= (cchMax - cchMax2 + 1);  /* This can't overflow, can it? */
            cchMax = cchMax2 + 1;
            break;
        }

        /* compare */
        int iDiff = cp1 - cp2;
        if (iDiff)
        {
            iDiff = RTUniCpToUpper(cp1) != RTUniCpToUpper(cp2);
            if (iDiff)
            {
                iDiff = RTUniCpToLower(cp1) - RTUniCpToLower(cp2); /* lower case diff last! */
                if (iDiff)
                    return iDiff;
            }
        }

        /* hit the terminator? */
        if (!cp1 || cchMax == 0)
            return 0;
    }

    /* Hit some bad encoding, continue in case insensitive mode. */
    return RTStrNCmp(psz1, psz2, cchMax);
}
RT_EXPORT_SYMBOL(RTStrNICmp);


RTDECL(char *) RTStrIStr(const char *pszHaystack, const char *pszNeedle)
{
    /* Any NULL strings means NULL return. (In the RTStrCmp tradition.) */
    if (!pszHaystack)
        return NULL;
    if (!pszNeedle)
        return NULL;

    /* The empty string matches everything. */
    if (!*pszNeedle)
        return (char *)pszHaystack;

    /*
     * The search strategy is to pick out the first char of the needle, fold it,
     * and match it against the haystack code point by code point. When encountering
     * a matching code point we use RTStrNICmp for the remainder (if any) of the needle.
     */
    const char * const pszNeedleStart = pszNeedle;
    RTUNICP Cp0;
    RTStrGetCpEx(&pszNeedle, &Cp0);     /* pszNeedle is advanced one code point. */
    size_t const    cchNeedle   = strlen(pszNeedle);
    size_t const    cchNeedleCp0= pszNeedle - pszNeedleStart;
    RTUNICP const   Cp0Lower    = RTUniCpToLower(Cp0);
    RTUNICP const   Cp0Upper    = RTUniCpToUpper(Cp0);
    if (    Cp0Lower == Cp0Upper
        &&  Cp0Lower == Cp0)
    {
        /* Cp0 is not a case sensitive char. */
        for (;;)
        {
            RTUNICP Cp;
            RTStrGetCpEx(&pszHaystack, &Cp);
            if (!Cp)
                break;
            if (    Cp == Cp0
                &&  !RTStrNICmp(pszHaystack, pszNeedle, cchNeedle))
                return (char *)pszHaystack - cchNeedleCp0;
        }
    }
    else if (   Cp0Lower == Cp0
             || Cp0Upper != Cp0)
    {
        /* Cp0 is case sensitive */
        for (;;)
        {
            RTUNICP Cp;
            RTStrGetCpEx(&pszHaystack, &Cp);
            if (!Cp)
                break;
            if (    (   Cp == Cp0Upper
                     || Cp == Cp0Lower)
                &&  !RTStrNICmp(pszHaystack, pszNeedle, cchNeedle))
                return (char *)pszHaystack - cchNeedleCp0;
        }
    }
    else
    {
        /* Cp0 is case sensitive and folds to two difference chars. (paranoia) */
        for (;;)
        {
            RTUNICP Cp;
            RTStrGetCpEx(&pszHaystack, &Cp);
            if (!Cp)
                break;
            if (    (   Cp == Cp0
                     || Cp == Cp0Upper
                     || Cp == Cp0Lower)
                &&  !RTStrNICmp(pszHaystack, pszNeedle, cchNeedle))
                return (char *)pszHaystack - cchNeedleCp0;
        }
    }


    return NULL;
}
RT_EXPORT_SYMBOL(RTStrIStr);


RTDECL(char *) RTStrToLower(char *psz)
{
    /*
     * Loop the code points in the string, converting them one by one.
     * ASSUMES that the code points for upper and lower case are encoded
     *         with the exact same length.
     */
    /** @todo Handled bad encodings correctly+quietly, remove assumption,
     *        optimize. */
    char *pszCur = psz;
    while (*pszCur)
    {
        RTUNICP cp = RTStrGetCp(pszCur);
        cp = RTUniCpToLower(cp);
        pszCur = RTStrPutCp(pszCur, cp);
    }
    return psz;
}
RT_EXPORT_SYMBOL(RTStrToLower);


RTDECL(char *) RTStrToUpper(char *psz)
{
    /*
     * Loop the code points in the string, converting them one by one.
     * ASSUMES that the code points for upper and lower case are encoded
     *         with the exact same length.
     */
    /** @todo Handled bad encodings correctly+quietly, remove assumption,
     *        optimize. */
    char *pszCur = psz;
    while(*pszCur)
    {
        RTUNICP cp = RTStrGetCp(pszCur);
        cp = RTUniCpToUpper(cp);
        pszCur = RTStrPutCp(pszCur, cp);
    }
    return psz;
}
RT_EXPORT_SYMBOL(RTStrToUpper);

