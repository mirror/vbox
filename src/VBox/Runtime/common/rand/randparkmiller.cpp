/* $Id$ */
/** @file
 * IPRT - Random Numbers, Park-Miller Pseudo Random.
 */

/*
 * Copyright (C) 2008 Sun Microsystems, Inc.
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
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include <iprt/rand.h>
#include <iprt/asm.h>
#include <iprt/mem.h>
#include <iprt/err.h>
#include "internal/rand.h"
#include "internal/magics.h"



DECLINLINE(uint32_t) rtRandParkMillerU31(uint32_t *pu32Ctx)
{
    /*
     * Park-Miller random number generator:
     *      X2 = X1 * g mod n.
     *
     * We use the constants suggested by Park and Miller:
     *      n = 2^31 - 1 = INT32_MAX
     *      g = 7^5 = 16807
     *
     * This will produce numbers in the range [0..INT32_MAX-1], which is
     * almost 31-bits. We'll ignore the missing number for now and settle
     * for just filling in the missing bit instead (the caller does this).
     */
    uint32_t x1 = *pu32Ctx;
    if (!x1)
        x1 = 20080806;
    /*uint32_t x2 = ((uint64_t)x1 * 16807) % INT32_MAX;*/
    uint32_t x2 = ASMModU64ByU32RetU32(ASMMult2xU32RetU64(x1, 16807), INT32_MAX);
    return *pu32Ctx = x2;
}


/** @copydoc RTRANDINT::pfnGetU32 */
static uint32_t rtRandParkMillerGetU32(PRTRANDINT pThis, uint32_t u32First, uint32_t u32Last)
{
    uint32_t off;
    uint32_t offLast = u32Last - u32First;
    if (offLast == UINT32_MAX)
    {
        /* 30 + 2 bit (make up for the missing INT32_MAX value) */
        off = rtRandParkMillerU31(&pThis->u.ParkMiller.u32Ctx);
        if (pThis->u.ParkMiller.cBits < 2)
        {
            pThis->u.ParkMiller.u32Bits = rtRandParkMillerU31(&pThis->u.ParkMiller.u32Ctx);
            pThis->u.ParkMiller.cBits = 30;
        }
        off >>= 1;
        off |= (pThis->u.ParkMiller.u32Bits & 3) << 30;
        pThis->u.ParkMiller.u32Bits >>= 2;
        pThis->u.ParkMiller.cBits -= 2;
    }
    else if (offLast == (uint32_t)INT32_MAX - 1)
        /* The exact range. */
        off = rtRandParkMillerU31(&pThis->u.ParkMiller.u32Ctx);
    else if (offLast < UINT32_C(0x07ffffff))
    {
        /* Requested 23 or fewer bits, just lose the lower bit. */
        off = rtRandParkMillerU31(&pThis->u.ParkMiller.u32Ctx);
        off >>= 1;
        off %= (offLast + 1);
    }
    else
    {
        /*
         * 30 + 6 bits.
         */
        uint64_t off64 = rtRandParkMillerU31(&pThis->u.ParkMiller.u32Ctx);
        if (pThis->u.ParkMiller.cBits < 6)
        {
            pThis->u.ParkMiller.u32Bits = rtRandParkMillerU31(&pThis->u.ParkMiller.u32Ctx);
            pThis->u.ParkMiller.cBits = 30;
        }
        off64 >>= 1;
        off64 |= (uint64_t)(pThis->u.ParkMiller.u32Bits & 0x3f) << 30;
        pThis->u.ParkMiller.u32Bits >>= 6;
        pThis->u.ParkMiller.cBits -= 6;
        off = ASMModU64ByU32RetU32(off64, offLast + 1);
    }
    return off + u32First;
}


/** @copydoc RTRANDINT::pfnSeed */
static int rtRandParkMillerSeed(PRTRANDINT pThis, uint64_t u64Seed)
{
    pThis->u.ParkMiller.u32Ctx = u64Seed;
    pThis->u.ParkMiller.u32Bits = 0;
    pThis->u.ParkMiller.cBits = 0;
    return VINF_SUCCESS;
}


/** @copydoc RTRANDINT::pfnDestroy */
static int rtRandParkMillerDestroy(PRTRANDINT pThis)
{
    pThis->u32Magic = ~RTRANDINT_MAGIC;
    RTMemFree(pThis);
    return VINF_SUCCESS;
}


RTDECL(int) RTRandAdvCreateParkMiller(PRTRAND phRand) RT_NO_THROW
{
    PRTRANDINT pThis = (PRTRANDINT)RTMemAlloc(sizeof(*pThis));
    if (!pThis)
        return VERR_NO_MEMORY;
    pThis->u32Magic = RTRANDINT_MAGIC;
    pThis->pfnGetBytes= rtRandAdvSynthesizeBytesFromU32;
    pThis->pfnGetU32  = rtRandParkMillerGetU32;
    pThis->pfnGetU64  = rtRandAdvSynthesizeU64FromU32;
    pThis->pfnSeed    = rtRandParkMillerSeed;
    pThis->pfnDestroy = rtRandParkMillerDestroy;
    pThis->u.ParkMiller.u32Ctx = 0x20080806;
    pThis->u.ParkMiller.u32Bits = 0;
    pThis->u.ParkMiller.cBits = 0;
    *phRand = pThis;
    return VINF_SUCCESS;
}

