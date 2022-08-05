/* $Id$ */
/** @file
 * IPRT - No-CRT - atoi.
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
#include <iprt/string.h>


#undef atoi
int RT_NOCRT(atoi)(const char *psz)
{
#if INT_MAX == INT32_MAX
    int32_t iValue = 0;
    int rc = RTStrToInt32Ex(psz, NULL, 10, &iValue);
#else
# error "Unsupported integer size"
#endif
    if (rc == VINF_SUCCESS || rc == VWRN_TRAILING_CHARS || rc == VWRN_TRAILING_SPACES)
        return iValue;
    if (rc == VWRN_NUMBER_TOO_BIG)
        return iValue < 0 ? INT_MIN : INT_MAX;
    return 0;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(atoi);

