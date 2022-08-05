/* $Id$ */
/** @file
 * IPRT - No-CRT - strtoul.
 */

/*
 * Copyright (C) 2022 Oracle Corporation
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
#define IPRT_NO_CRT_FOR_3RD_PARTY
#include "internal/nocrt.h"
#include <iprt/nocrt/stdlib.h>
#include <iprt/nocrt/limits.h>
#include <iprt/nocrt/errno.h>
#include <iprt/string.h>


#undef strtoul
long RT_NOCRT(strtoul)(const char *psz, char **ppszNext, int iBase)
{
#if ULONG_BIT == 64
    uint64_t uValue = 0;
    int rc = RTStrToUInt64Ex(psz, ppszNext, (unsigned)iBase, &uiValue);
#elif ULONG_BIT == 32
    int32_t uValue = 0;
    int rc = RTStrToUInt32Ex(psz, ppszNext, (unsigned)iBase, &uiValue);
#else
# error "Unsupported LONG_BIT value"
#endif
    if (rc == VINF_SUCCESS || rc == VWRN_TRAILING_CHARS || rc == VWRN_TRAILING_SPACES || rc == VWRN_NEGATIVE_UNSIGNED)
        return uValue;
    if (rc == VWRN_NUMBER_TOO_BIG)
    {
        errno = ERANGE;
        return ULONG_MAX;
    }
    errno = EINVAL;
    return 0;
}

