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
#include <iprt/err.h>
#include <iprt/mem.h>


/*******************************************************************************
*   Defined Constants                                                          *
*******************************************************************************/
#define ISDIGIT(c) ((c) >= '0' && (c) <= '9')


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static uint16_t RTStrVersionGetBlockCount(const char *pszVer)
{
    uint16_t l = 0;
    const char *pszCur = pszVer;
    while (pszCur = RTStrStr(pszCur, "."))
    {
        if (pszCur == NULL)
            break;
        l++;
        pszCur++;
    }
    /* Adjust block count to also count in the very first block */
    if (*pszVer != '\0')
        l++;
    return l;
}


static int RTStrVersionGetUInt32(const char *pszVer, uint16_t u16Block, uint32_t *pu32)
{
    /* First make a copy of the version string so that we can modify it */
    char *pszString;
    int rc = RTStrDupEx(&pszString, pszVer);
    if (RT_FAILURE(rc))
        return rc;

    /* Go to the beginning of the block we want to parse */
    char *pszCur = pszString;
    for (uint16_t i = 0; i < u16Block; i++)
    {
        pszCur = RTStrStr(pszCur, ".");
        if (pszCur == NULL)
            break;
        if (*pszCur != '\0')
            pszCur++;
    }

    if (pszCur != NULL && *pszCur != '\0')
    {
        /* Skip trailing non-digits at the start of the block */
        while (pszCur && *pszCur != '\0')
        {
            if (ISDIGIT(*pszCur))
                break;
            pszCur++;
        }

        /* Mark ending of the block */
        char *pszEnd = RTStrStr(pszCur, ".");
        if (NULL != pszEnd)
            *pszEnd = '\0';

        /* Convert to number */
        rc = RTStrToUInt32Ex(pszCur, NULL /* ppszNext */, 10 /* Base */, pu32);
        /* Skip trailing warnings */
        if (   rc == VWRN_TRAILING_CHARS
            || rc == VWRN_TRAILING_SPACES)
            rc = VINF_SUCCESS;
    }
    else
        rc = VERR_NOT_FOUND;

    RTStrFree(pszString);
    return rc;
}


/**
 * Compares two version strings and returns the result. The version string has
 * to be made of at least one number section, each section delimited by a ".",
 * e.g. "123.45.67". Trailing zeros at the beginning and non-digits in a section
 * will be skipped, so "12.foo006" becomes "12.6".
 *
 * @returns iprt status code.
 *          Warnings are used to indicate convertion problems.
 * @retval  VWRN_NUMBER_TOO_BIG
 * @retval  VWRN_TRAILING_CHARS
 * @retval  VWRN_TRAILING_SPACES
 * @retval  VINF_SUCCESS
 * @retval  VERR_NO_MEMORY
 * @retval  VERR_NO_DIGITS
 *
 * @todo    Deal with prefixes and suffixes!
 * @param   pszVer1     First version string to compare.
 * @param   pszVer2     First version string to compare.*
 * @param   pui8Res     Pointer uint8_t value where to store the comparison result:
 *                      0 if equal, 1 if pszVer1 is greater, 2 if pszVer2 is greater.
 */
int RTStrVersionCompare(const char *pszVer1, const char *pszVer2, uint8_t *pui8Res)
{
    AssertPtr(pszVer1);
    AssertPtr(pszVer2);

    uint16_t len1 = RTStrVersionGetBlockCount(pszVer1);
    uint16_t len2 = RTStrVersionGetBlockCount(pszVer2);

    int rc = 0;
    if (len1 > 0 && len2 > 0)
    {
        /* Figure out which version string is longer and set the corresponding
         * pointers */
        uint16_t range;
        uint16_t padding;
        const char *pszShorter, *pszLonger;
        if (len1 >= len2)
        {
            range = len1;
            padding = len1 - len2;
            pszLonger = pszVer1;
            pszShorter = pszVer2;
        }
        else if (len2 > len1)
        {
            range = len2;
            padding = len2 - len1;
            pszLonger = pszVer2;
            pszShorter = pszVer1;
        }

        /* Now process each section (delimited by a ".") */
        AssertPtr(pszShorter);
        AssertPtr(pszLonger);
        AssertPtr(pui8Res);
        *pui8Res = 0;
        uint32_t val1, val2;
        for (uint16_t i = 0;    i < range
                             && *pui8Res == 0
                             && RT_SUCCESS(rc)
                           ; i++)
        {
            rc = RTStrVersionGetUInt32(pszLonger, i, &val1);
            if (RT_SUCCESS(rc))
            {
                if (i >= range - padding)
                {
                    /* If we're in the padding range, there are no numbers left
                     * to compare with anymore, so just assume "0" then */
                    val2 = 0;
                }
                else
                {
                    rc = RTStrVersionGetUInt32(pszShorter, i, &val2);
                }
            }

            if (RT_SUCCESS(rc))
            {
                if (val1 > val2)
                {
                    *pui8Res = (pszLonger == pszVer1) ? 1 : 2;
                    break;
                }
                else if (val2 > val1)
                {
                    *pui8Res = (pszShorter == pszVer1) ? 1 : 2;
                    break;
                }
            }
        }
    }
    else
    {
        rc = VERR_NO_DIGITS;
    }

    if (RT_FAILURE(rc))
        *pui8Res = 0; /* Zero out value */
    return rc;
}
RT_EXPORT_SYMBOL(RTStrVersionCompare);
