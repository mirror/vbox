/* $Id$ */
/** @file
 * IPRT - No-CRT - copysign().
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
#include <iprt/nocrt/math.h>
#include <iprt/assertcompile.h>


#undef copysign
double RT_NOCRT(copysign)(double rdMagnitude, double rdSign)
{
    AssertCompile(sizeof(rdMagnitude) == sizeof(RTFLOAT64U));
#ifndef RT_LITTLE_ENDIAN /* MSC outputs a lot better code for the alternative below. */
    RTFLOAT64U uRet, uSign;
    uSign.rd     = rdSign;
    uRet.rd      = rdMagnitude;
    uRet.s.fSign = uSign.s.fSign;
#else
    RTFLOAT64U uRet;
    uRet.u = (*(uint64_t const *)&rdMagnitude & (RT_BIT_64(63) - 1U)) | (*(uint64_t const *)&rdSign & RT_BIT_64(63));
#endif
    return uRet.rd;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(copysign);

