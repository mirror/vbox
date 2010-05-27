/* $Id$ */
/** @file
 * IPRT - IPv4 address parsing.
 */

/*
 * Copyright (C) 2006-2008 Oracle Corporation
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


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/cidr.h>
#include "internal/iprt.h"

#include <iprt/ctype.h>
#include <iprt/string.h>
#include <iprt/stream.h>


RTDECL(int) RTCidrStrToIPv4(const char *pszAddress, PRTIPV4ADDR pNetwork, PRTIPV4ADDR pNetmask)
{
    uint8_t cBits;
    uint8_t addr[4];
    uint32_t u32Netmask;
    uint32_t u32Network;
    const char *psz = pszAddress;
    char *pszNext;
    int  rc = VINF_SUCCESS;
    int cDelimiter = 0;
    int cDelimiterLimit = 0;
    if (   pszAddress == NULL
        || pNetwork == NULL
        || pNetmask == NULL)
        return VERR_INVALID_PARAMETER;
    char *pszNetmask = RTStrStr(psz, "/");
    *(uint32_t *)addr = 0;
    if (pszNetmask == NULL)
        cBits = 32; 
    else 
    { 
        rc = RTStrToUInt8Ex(pszNetmask + 1, &pszNext, 10, &cBits);
        if (   RT_FAILURE(rc) 
            || cBits > 32 
            || rc != 0) /* No trailing symbols are accptable after the digit */
            return VERR_INVALID_PARAMETER;
    }
    u32Netmask = ~(uint32_t)((1<< (32 - cBits)) - 1);

    rc = RTStrToUInt8Ex(psz, &pszNext, 10, &addr[0]);
    if (RT_FAILURE(rc))
        return rc;

    if (cBits < 9)
        cDelimiterLimit = 0;
    else if (cBits <= 16)
        cDelimiterLimit = 1;
    else if (cBits <= 24)
        cDelimiterLimit = 2;
    else if (cBits <= 32)
        cDelimiterLimit = 3;

    rc = RTStrToUInt8Ex(psz, &pszNext, 10, &addr[cDelimiter]);
    while (RT_SUCCESS(rc))
    {
        if (*pszNext == '.')
            cDelimiter++;
        else if(   cDelimiter >= cDelimiterLimit
                && (   *pszNext == '\0'
                    || *pszNext == '/'))
            break;
        else 
            return VERR_INVALID_PARAMETER;

        if(cDelimiter > 3)
            /* no more than four octets */
            return VERR_INVALID_PARAMETER;

        rc = RTStrToUInt8Ex(pszNext + 1, &pszNext, 10, &addr[cDelimiter]);
        if (rc == VWRN_NUMBER_TOO_BIG)
            break;
    }
    if (   RT_FAILURE(rc) 
        || rc == VWRN_NUMBER_TOO_BIG)
        return VERR_INVALID_PARAMETER;
    u32Network = RT_MAKE_U32_FROM_U8(addr[3], addr[2], addr[1], addr[0]);
    /* corner case: see rfc 790 page 2 and rfc 4632 page 6*/
    if (   addr[0] == 0 
        && (   *(uint32_t *)addr != 0
            || u32Netmask == (uint32_t)~0))
        return VERR_INVALID_PARAMETER;

    if ((u32Network & ~u32Netmask) != 0)
        return VERR_INVALID_PARAMETER;
    
    *pNetmask = u32Netmask;
    *pNetwork = u32Network; 
    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTCidrStrToIPv4);

