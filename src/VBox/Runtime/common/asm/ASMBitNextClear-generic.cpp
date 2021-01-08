/* $Id$ */
/** @file
 * IPRT - ASMBitNextClear - generic C implementation.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
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

#include <iprt/assert.h>


RTDECL(int) ASMBitNextClear(const volatile void RT_FAR *pvBitmap, uint32_t cBits, uint32_t iBitPrev) RT_NOTHROW_DEF
{
    const volatile uint32_t RT_FAR *pau32Bitmap = (const volatile uint32_t RT_FAR *)pvBitmap;
    int                             iBit = ++iBitPrev & 31;
    Assert(!(cBits & 31));
    Assert(!((uintptr_t)pvBitmap & 3));
    if (iBit)
    {
        /*
         * Inspect the 32-bit word containing the unaligned bit.
         */
        uint32_t u32 = ~RT_LE2H_U32(pau32Bitmap[iBitPrev / 32]) >> iBit;
        if (u32)
            return iBitPrev + ASMBitFirstSetU32(u32) - 1;

        /*
         * Skip ahead and see if there is anything left to search.
         */
        iBitPrev |= 31;
        iBitPrev++;
        if (cBits <= (uint32_t)iBitPrev)
            return -1;
    }

    /*
     * 32-bit aligned search, let ASMBitFirstClear do the dirty work.
     */
    iBit = ASMBitFirstClear(&pau32Bitmap[iBitPrev / 32], cBits - iBitPrev);
    if (iBit >= 0)
        iBit += iBitPrev;
    return iBit;
}

