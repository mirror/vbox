/* $Id$ */
/** @file
 * IPRT - ASMMemZeroPage - generic C implementation.
 */

/*
 * Copyright (C) 2021 Oracle Corporation
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
#include <iprt/asm.h>
#include "internal/iprt.h"

#include <iprt/string.h>
#include <iprt/assert.h>


RT_ASM_DECL_PRAGMA_WATCOM(void) ASMMemFill32(volatile void RT_FAR *pv, size_t cb, uint32_t u32) RT_NOTHROW_DEF
{
    Assert(!(cb & 3));
    size_t cFills = cb / sizeof(uint32_t);
    uint32_t *pu32Dst = (uint32_t *)pv;

    while (cFills >= 8)
    {
        pu32Dst[0] = u32;
        pu32Dst[1] = u32;
        pu32Dst[2] = u32;
        pu32Dst[3] = u32;
        pu32Dst[4] = u32;
        pu32Dst[5] = u32;
        pu32Dst[6] = u32;
        pu32Dst[7] = u32;
        pu32Dst += 8;
        cFills  -= 8;
    }

    while (cFills > 0)
    {
        *pu32Dst++ = u32;
        cFills -= 1;
    }
}

