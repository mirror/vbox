/* $Id$ */
/** @file
 * IPRT - IPv4 address parsing.
 */

/*
 * Copyright (C) 2006-2008 Sun Microsystems, Inc.
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
#include <iprt/cidr.h>
#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/stream.h>


/**
 * Scan a digit of an IPv4 address.
 *
 * @returns IPRT status code.
 *
 * @param   iDigit     The index of the IPv4 digit to scan.
 * @param   psz        Pointer to the begin of the string.
 * @param   ppszNext   Pointer to variable that should be set pointing to the first invalid character. (output)
 * @param   pu8        Pointer to the digit to write (output).
 */
static int scanIPv4Digit(int iDigit, const char *psz, char **ppszNext, uint8_t *pu8)
{
    int rc = RTStrToUInt8Ex(psz, ppszNext, 10, pu8);
    if ((      rc != VINF_SUCCESS
           && rc != VWRN_TRAILING_CHARS)
        || *pu8 > 254)
        return VERR_INVALID_PARAMETER;

    /* first digit cannot be 0 */
    if (   iDigit == 1
        && *pu8 < 1)
        return VERR_INVALID_PARAMETER;

    if (**ppszNext == '/')
        return VINF_SUCCESS;

    if (   iDigit != 4
        && (   **ppszNext == '\0'
            || **ppszNext != '.'))
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}


int RTCidrStrToIPv4(const char *pszAddress, PRTIPV4ADDR pNetwork, PRTIPV4ADDR pNetmask)
{
    uint8_t cBits;
    uint8_t a;
    uint8_t b = 0;
    uint8_t c = 0;
    uint8_t d = 0;
    const char *psz = pszAddress;
    char *pszNext;
    int  rc;

    do
    {
        /* 1st digit */
        rc = scanIPv4Digit(1, psz, &pszNext, &a);
        if (RT_FAILURE(rc))
            return rc;
        if (*pszNext == '/')
            break;
        psz = pszNext + 1;

        /* 2nd digit */
        rc = scanIPv4Digit(2, psz, &pszNext, &b);
        if (RT_FAILURE(rc))
            return rc;
        if (*pszNext == '/')
            break;
        psz = pszNext + 1;

        /* 3rd digit */
        rc = scanIPv4Digit(3, psz, &pszNext, &c);
        if (RT_FAILURE(rc))
            return rc;
        if (*pszNext == '/')
            break;
        psz = pszNext + 1;

        /* 4th digit */
        rc = scanIPv4Digit(4, psz, &pszNext, &d);
        if (RT_FAILURE(rc))
            return rc;
    } while (0);

    if (*pszNext == '/')
    {
        psz = pszNext + 1;
        rc = RTStrToUInt8Ex(psz, &pszNext, 10, &cBits);
        if (rc != VINF_SUCCESS || cBits < 8 || cBits > 28)
            return VERR_INVALID_PARAMETER;
    }
    else
        cBits = 0;

    for (psz = pszNext; RT_C_IS_SPACE(*psz); psz++)
        /* nothing */;
    if (*psz != '\0')
        return VERR_INVALID_PARAMETER;

    *pNetwork = RT_MAKE_U32_FROM_U8(d, c, b, a);
    *pNetmask = ~(((uint32_t)1 << (32 - cBits)) - 1);
    return VINF_SUCCESS;
}
