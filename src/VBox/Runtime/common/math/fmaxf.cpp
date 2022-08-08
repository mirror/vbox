/* $Id$ */
/** @file
 * IPRT - No-CRT - fmaxf().
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


#undef fmaxf
float RT_NOCRT(fmaxf)(float r32Left, float r32Right)
{
    if (!isnan(r32Left))
    {
        if (!isnan(r32Right))
        {
            /* We don't trust the hw with comparing signed zeros, thus
               the 0.0 test and signbit fun here. */
            if (r32Left != r32Right || r32Left != 0.0)
                return r32Left >= r32Right ? r32Left : r32Right;
            return signbit(r32Left) <= signbit(r32Right) ? r32Left : r32Right;
        }
        return r32Left;
    }
    return r32Right;
}
RT_ALIAS_AND_EXPORT_NOCRT_SYMBOL(fmaxf);

