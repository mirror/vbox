/* $Id$ */
/** @file
 * IPRT - Command Line Parsing, Argument Vector.
 */

/*
 * Copyright (C) 2010 Sun Microsystems, Inc.
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
#include <iprt/getopt.h>
#include "internal/iprt.h"

#include <iprt/assert.h>
#include <iprt/err.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/**
 * Look for an unicode code point in the separator string.
 *
 * @returns true if it's a separator, false if it isn't.
 * @param   Cp              The code point.
 * @param   pszSeparators   The separators.
 */
static bool rtGetOptIsUniCpInString(RTUNICP Cp, const char *pszSeparators)
{
    /* This could be done in a more optimal fashion.  Probably worth a
       separate RTStr function at some point. */
    for (;;)
    {
        RTUNICP CpSep;
        int rc = RTStrGetCpEx(&pszSeparators, &CpSep);
        AssertRCReturn(rc, false);
        if (CpSep == Cp)
            return true;
        if (!CpSep)
            return false;
    }
}


/**
 * Look for an 7-bit ASCII character in the separator string.
 *
 * @returns true if it's a separator, false if it isn't.
 * @param   ch              The character.
 * @param   pszSeparators   The separators.
 * @param   cchSeparators   The number of separators chars.
 */
DECLINLINE(bool) rtGetOptIsAsciiInSet(char ch, const char *pszSeparators, size_t cchSeparators)
{
    switch (cchSeparators)
    {
        case 8: if (ch == pszSeparators[7]) return true;
        case 7: if (ch == pszSeparators[6]) return true;
        case 6: if (ch == pszSeparators[5]) return true;
        case 5: if (ch == pszSeparators[4]) return true;
        case 4: if (ch == pszSeparators[3]) return true;
        case 3: if (ch == pszSeparators[2]) return true;
        case 2: if (ch == pszSeparators[1]) return true;
        case 1: if (ch == pszSeparators[0]) return true;
            return false;
        default:
            return memchr(pszSeparators, ch, cchSeparators);
    }
}


/**
 * Checks if the character is in the set of separators
 *
 * @returns true if it is, false if it isn't.
 *
 * @param   Cp              The code point.
 * @param   pszSeparators   The separators.
 * @param   cchSeparators   The length of @a pszSeparators.
 */
DECL_FORCE_INLINE(bool) rtGetOptIsCpInSet(RTUNICP Cp, const char *pszSeparators, size_t cchSeparators)
{
    if (RT_LIKELY(Cp <= 127))
        return rtGetOptIsAsciiInSet((char)Cp, pszSeparators, cchSeparators);
    return rtGetOptIsUniCpInString(Cp, pszSeparators);
}


/**
 * Skips any delimiters at the start of the string that is pointed to.
 *
 * @returns VINF_SUCCESS or RTStrGetCpEx status code.
 * @param   ppszSrc         Where to get and return the string pointer.
 * @param   pszSeparators   The separators.
 * @param   cchSeparators   The length of @a pszSeparators.
 */
static int rtGetOptSkipDelimiters(const char **ppszSrc, const char *pszSeparators, size_t cchSeparators)
{
    const char *pszSrc = *ppszSrc;
    const char *pszRet;
    for (;;)
    {
        pszRet = pszSrc;
        RTUNICP Cp;
        int rc = RTStrGetCpEx(&pszSrc, &Cp);
        if (RT_FAILURE(rc))
        {
            *ppszSrc = pszRet;
            return rc;
        }
        if (   !Cp
            || !rtGetOptIsCpInSet(Cp, pszSeparators, cchSeparators))
            break;
    }

    *ppszSrc = pszRet;
    return VINF_SUCCESS;
}


RTDECL(int) RTGetOptArgvFromString(char ***ppapszArgv, int *pcArgs, const char *pszCmdLine, const char *pszSeparators)
{
    /*
     * Some input validation.
     */
    AssertPtr(pszCmdLine);
    AssertPtr(pcArgs);
    AssertPtr(ppapszArgv);
    if (!pszSeparators)
        pszSeparators = " \t\n\r";
    else
        AssertPtr(pszSeparators);
    size_t const cchSeparators = strlen(pszSeparators);
    AssertReturn(cchSeparators > 0, VERR_INVALID_PARAMETER);

    /*
     * Parse the command line and chop off it into argv individual argv strings.
     */
    int         rc        = VINF_SUCCESS;
    const char *pszSrc    = pszCmdLine;
    char       *pszDup    = (char *)RTMemAlloc(strlen(pszSrc) + 1);
    char       *pszDst    = pszDup;
    if (!pszDup)
        return VERR_NO_STR_MEMORY;
    char      **papszArgs = NULL;
    unsigned    iArg      = 0;
    while (*pszSrc)
    {
        /* Skip stuff */
        rc = rtGetOptSkipDelimiters(&pszSrc, pszSeparators, cchSeparators);
        if (RT_FAILURE(rc))
            break;
        if (!*pszSrc)
            break;

        /* Start a new entry. */
        if ((iArg % 32) == 0)
        {
            void *pvNew = RTMemRealloc(papszArgs, (iArg + 33) * sizeof(char *));
            if (!pvNew)
            {
                rc = VERR_NO_MEMORY;
                break;
            }
            papszArgs = (char **)pvNew;
        }
        papszArgs[iArg++] = pszDst;

        /* Parse and copy the string over. */
        RTUNICP CpQuote = 0;
        for (;;)
        {
            RTUNICP Cp;
            rc = RTStrGetCpEx(&pszSrc, &Cp);
            if (RT_FAILURE(rc))
                break;
            if (!Cp)
                break;
            if (!CpQuote)
            {
                if (Cp == '"' || Cp == '\'')
                    CpQuote = Cp;
                else if (rtGetOptIsCpInSet(Cp, pszSeparators, cchSeparators))
                    break;
                else
                    pszDst = RTStrPutCp(pszDst, Cp);
            }
            else if (CpQuote != Cp)
                pszDst = RTStrPutCp(pszDst, Cp);
            else
                CpQuote = 0;
        }
        *pszDst++ = '\0';
        if (RT_FAILURE(rc))
            break;
    }

    if (RT_FAILURE(rc))
    {
        RTMemFree(pszDup);
        RTMemFree(papszArgs);
        return rc;
    }

    /*
     * Terminate the array.
     * Check for empty string to make sure we've got an array.
     */
    if (iArg == 0)
    {
        RTMemFree(pszDup);
        papszArgs = (char **)RTMemAlloc(1 * sizeof(char *));
        if (!papszArgs)
            return VERR_NO_MEMORY;
    }
    papszArgs[iArg] = NULL;

    *pcArgs     = iArg;
    *ppapszArgv = papszArgs;
    return VINF_SUCCESS;
}


RTDECL(void) RTGetOptArgvFree(char **papszArgv)
{
    if (papszArgv)
    {
        RTMemFree(papszArgv[0]);
        RTMemFree(papszArgv);
    }
}


/** @todo RTGetOptArgvToString (for windows)?
 *  RTGetOptArgvSort for RTGetOptInit()? */

