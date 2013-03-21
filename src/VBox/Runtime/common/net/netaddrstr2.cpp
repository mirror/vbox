/* $Id$ */
/** @file
 * IPRT - Network Address String Handling.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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
#include "internal/iprt.h"
#include <iprt/net.h>

#include <iprt/mem.h>
#include <iprt/string.h>
#include <iprt/stream.h>
#include "internal/string.h"

RTDECL(int) RTNetStrToIPv4Addr(const char *pszAddr, PRTNETADDRIPV4 pAddr)
{
    char *pszNext;
    AssertPtrReturn(pszAddr, VERR_INVALID_PARAMETER);
    AssertPtrReturn(pAddr, VERR_INVALID_PARAMETER);

    int rc = RTStrToUInt8Ex(RTStrStripL(pszAddr), &pszNext, 10, &pAddr->au8[0]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;
    if (*pszNext++ != '.')
        return VERR_INVALID_PARAMETER;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[1]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;
    if (*pszNext++ != '.')
        return VERR_INVALID_PARAMETER;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[2]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_CHARS)
        return VERR_INVALID_PARAMETER;
    if (*pszNext++ != '.')
        return VERR_INVALID_PARAMETER;

    rc = RTStrToUInt8Ex(pszNext, &pszNext, 10, &pAddr->au8[3]);
    if (rc != VINF_SUCCESS && rc != VWRN_TRAILING_SPACES)
        return VERR_INVALID_PARAMETER;
    pszNext = RTStrStripL(pszNext);
    if (*pszNext)
        return VERR_INVALID_PARAMETER;

    return VINF_SUCCESS;
}
RT_EXPORT_SYMBOL(RTNetStrToIPv4Addr);


