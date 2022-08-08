/* $Id$ */
/** @file
 * IPRT - No-CRT - fmaxl().
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


#undef fmaxl
long double RT_NOCRT(fmaxl)(long double  lrdLeft, long double lrdRight)
{
    if (!isnan(lrdLeft))
    {
        if (!isnan(lrdRight))
        {
            /* We don't trust the hw with comparing signed zeros, thus
               the 0.0 test and signbit fun here. */
            if (lrdLeft != lrdRight || lrdLeft != 0.0)
                return lrdLeft >= lrdRight ? lrdLeft : lrdRight;
            return signbit(lrdLeft) <= signbit(lrdRight) ? lrdLeft : lrdRight;
        }
        return lrdLeft;
    }
    return lrdRight;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(fmaxl);

