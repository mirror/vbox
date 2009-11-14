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
#include <iprt/ctype.h> /* needed for RT_C_IS_DIGIT */
#include <iprt/err.h>
#include <iprt/mem.h>



/**
 * Converts a string representation of a version number to an unsigned number.
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
 * @param   pszValue    Pointer to the string value.
 * @param   pu32        Where to store the converted number.
 *
 * @todo    r=bird: The returned value isn't really suitable for comparing two
 *          version strings.  Try see which result you get when converting
 *          "3.0.14" and "3.1.0" and comparing the values.  The way to fix this
 *          deficiency would be to convert the individual parts and dividing the
 *          return value into sections: bits 31:24 FirstNumber; 23:16 Second;
 *          15:8 Third; 7:0 Forth.  It would probably be a good idea to use a
 *          64-bit return value instead of a 32-bit one, so there is room for
 *          revision number when found.
 *
 *          Actually, because of the above, the kind of API I had in mind was
 *          int RTStrVersionCompare(const char *pszVer1, const char *pszVer2).
 *          It wouldn't try convert input to numbers, just do a parallel parse.
 *          This would allow easy handling beta/alpha/++ indicators and any
 *          number of dots and dashes.
 */
RTDECL(int) RTStrVersionToUInt32(const char *pszVer, uint32_t *pu32)
{
    const char *psz = pszVer;
    AssertPtr(pu32);
    AssertPtr(psz);

    char *pszNew = (char*)RTMemAllocZ((strlen(pszVer) + 1) * sizeof(char));
    if (pszNew == NULL)
        return VERR_NO_MEMORY;

    unsigned    i            = 0;
    bool        fLastInvalid = false;
    while (    psz
           && *psz != '\0')
    {
        if (fLastInvalid)
        {
            if (   *psz == '-'
                || *psz == '_')
                fLastInvalid = false;
        }
        else
        {
            if (RT_C_IS_DIGIT(*psz))
                pszNew[i++] = *psz;
            else if (   *psz != '.'
                     && i == 0)
                fLastInvalid = true;
        }
        psz++;
    }
    pszNew[i] = '\0';

    /* Convert final number string to number */
    int rc;
    if (fLastInvalid)
    {
        *pu32 = 0;
        rc = VERR_NO_DIGITS;
    }
    else
    {
        rc = RTStrToUInt32Ex(pszNew, NULL /*pszNext*/, 10 /*uBase*/, pu32);
        if (rc != VINF_SUCCESS)
            *pu32 = 0;
    }
    RTStrFree(pszNew);
    return rc;
}

