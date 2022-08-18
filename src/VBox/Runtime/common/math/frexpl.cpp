/* $Id$ */
/** @file
 * IPRT - No-CRT - frexpl().
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
#include <iprt/nocrt/limits.h>
#ifdef  RT_COMPILER_WITH_128BIT_LONG_DOUBLE
# include <iprt/uint128.h>
#endif


/* Similar to the fxtract instruction. */
#undef frexpl
long double RT_NOCRT(frexpl)(long double lrdValue, int *piExp)
{
#ifdef RT_COMPILER_WITH_64BIT_LONG_DOUBLE
    RTFLOAT64U Value;
    AssertCompile(sizeof(Value) == sizeof(lrdValue));
    Value.lrd = lrdValue;

    if (RTFLOAT64U_IS_NORMAL(&Value))
    {
        *piExp = (int)Value.s.uExponent - RTFLOAT64U_EXP_BIAS + 1;
        Value.s.uExponent = RTFLOAT64U_EXP_BIAS - 1;
    }
    else if (RTFLOAT64U_IS_ZERO(&Value))
    {
        *piExp = 0;
        return lrdValue;
    }
    else if (RTFLOAT64U_IS_SUBNORMAL(&Value))
    {
        int      iExp      = -RTFLOAT64U_EXP_BIAS + 1;
        uint64_t uFraction = Value.s64.uFraction;
        while (!(uFraction & RT_BIT_64(RTFLOAT64U_FRACTION_BITS)))
        {
            iExp--;
            uFraction <<= 1;
        }
        Value.s64.uFraction = uFraction;
        Value.s64.uExponent = RTFLOAT64U_EXP_BIAS - 1;
        *piExp = iExp + 1;
    }
    else
    {
        /* NaN, Inf */
        *piExp = Value.s.fSign ? INT_MIN : INT_MAX;
        return lrdValue;
    }
    return Value.lrd;

#elif defined(RT_COMPILER_WITH_80BIT_LONG_DOUBLE)
    RTFLOAT80U2 Value;
    Value.r = lrdValue;

    if (RTFLOAT80U_IS_NORMAL(&Value))
    {
        *piExp = (int)Value.s.uExponent - RTFLOAT80U_EXP_BIAS + 1;
        Value.s.uExponent = RTFLOAT80U_EXP_BIAS - 1;
    }
    else if (RTFLOAT80U_IS_ZERO(&Value))
    {
        *piExp = 0;
        return lrdValue;
    }
    else if (RTFLOAT80U_IS_DENORMAL_OR_PSEUDO_DENORMAL(&Value))
    {
        int iExp = -RTFLOAT80U_EXP_BIAS + 1;
        while (!(Value.s.uMantissa & RT_BIT_64(RTFLOAT80U_FRACTION_BITS)))
        {
            iExp--;
            Value.s.uMantissa <<= 1;
        }
        Value.s.uExponent = RTFLOAT80U_EXP_BIAS - 1;
        *piExp = iExp + 1;
    }
    else  /* NaN, Inf */
    {
        *piExp = Value.s.fSign ? INT_MIN : INT_MAX;
        return lrdValue;
    }
    return Value.r;


#elif defined(RT_COMPILER_WITH_128BIT_LONG_DOUBLE)
    RTFLOAT128U Value;
    AssertCompile(sizeof(Value) == sizeof(lrdValue));
    Value.r = lrdValue;

    if (RTFLOAT128U_IS_NORMAL(&Value))
    {
        *piExp = (int)Value.s.uExponent - RTFLOAT128U_EXP_BIAS + 1;
        Value.s.uExponent = RTFLOAT128U_EXP_BIAS - 1;
    }
    else if (RTFLOAT128U_IS_ZERO(&Value))
    {
        *piExp = 0;
        return lrdValue;
    }
    else if (RTFLOAT128U_IS_SUBNORMAL(&Value))
    {
        int        iExp = -RTFLOAT128U_EXP_BIAS + 1;
        RTUINT128U uFraction;
        uFraction.s.Hi = Value.s64.uFractionHi;
        uFraction.s.Lo = Value.s64.uFractionLo;
        while (!(uFraction.s.Hi & RT_BIT_64(RTFLOAT128U_FRACTION_BITS - 64)))
        {
            iExp--;
            RTUInt128AssignShiftLeft(&uFraction, 1);
        }
        Value.s64.uFractionHi = uFraction.s.Hi;
        Value.s64.uFractionLo = uFraction.s.Lo;
        Value.s64.uExponent   = RTFLOAT64U_EXP_BIAS - 1;
        *piExp = iExp + 1;
    }
    else
    {
        /* NaN, Inf */
        *piExp = Value.s.fSign ? INT_MIN : INT_MAX;
        return lrdValue;
    }
    return Value.r;
#else
# error "Port ME!"
#endif
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(frexpl);

