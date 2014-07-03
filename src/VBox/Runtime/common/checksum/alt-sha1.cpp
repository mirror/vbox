/* $Id$ */
/** @file
 * IPRT - SHA-1 hash functions, Alternative Implementation.
 */

/*
 * Copyright (C) 2009-2014 Oracle Corporation
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
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The SHA-1 block size (in bytes). */
#define RTSHA1_BLOCK_SIZE   64U


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#include "internal/iprt.h"
#include <iprt/types.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>


/** Our private context structure. */
typedef struct RTSHA1ALTPRIVATECTX
{
    /** The W array.
     * Buffering happens in the first 16 words, converted from big endian to host
     * endian immediately before processing.  The amount of buffered data is kept
     * in the 6 least significant bits of cbMessage. */
    uint32_t    auW[80];
    /** The message length (in bytes). */
    uint64_t    cbMessage;

    /** The 5 hash values. */
    uint32_t    auH[5];
} RTSHA1ALTPRIVATECTX;

#define RT_SHA1_PRIVATE_ALT_CONTEXT
#include <iprt/sha.h>


AssertCompile(RT_SIZEOFMEMB(RTSHA1CONTEXT, abPadding) >= RT_SIZEOFMEMB(RTSHA1CONTEXT, AltPrivate));
AssertCompileMemberSize(RTSHA1ALTPRIVATECTX, auH, RTSHA1_HASH_SIZE);




RTDECL(void) RTSha1Init(PRTSHA1CONTEXT pCtx)
{
    pCtx->AltPrivate.cbMessage = 0;
    pCtx->AltPrivate.auH[0] = UINT32_C(0x67452301);
    pCtx->AltPrivate.auH[1] = UINT32_C(0xefcdab89);
    pCtx->AltPrivate.auH[2] = UINT32_C(0x98badcfe);
    pCtx->AltPrivate.auH[3] = UINT32_C(0x10325476);
    pCtx->AltPrivate.auH[4] = UINT32_C(0xc3d2e1f0);
}
RT_EXPORT_SYMBOL(RTSha1Init);


/**
 * Initializes the auW array from the specfied input block.
 *
 * @param   pCtx                The SHA1 context.
 * @param   pbBlock             The block.  Must be 32-bit aligned.
 */
DECLINLINE(void) rtSha1BlockInit(PRTSHA1CONTEXT pCtx, uint8_t const *pbBlock)
{
    uint32_t const *pu32Block = (uint32_t const *)pbBlock;
    Assert(!((uintptr_t)pu32Block & 3));

    unsigned iWord;
    for (iWord = 0; iWord < 16; iWord++)
        pCtx->AltPrivate.auW[iWord] = RT_BE2H_U32(pu32Block[iWord]);

    for (; iWord < RT_ELEMENTS(pCtx->AltPrivate.auW); iWord++)
    {
        uint32_t u32 = pCtx->AltPrivate.auW[iWord - 16];
        u32         ^= pCtx->AltPrivate.auW[iWord - 14];
        u32         ^= pCtx->AltPrivate.auW[iWord -  8];
        u32         ^= pCtx->AltPrivate.auW[iWord -  3];
        pCtx->AltPrivate.auW[iWord] = ASMRotateLeftU32(u32, 1);
    }
}


/**
 * Initializes the auW array from data buffered in the first part of the array.
 *
 * @param   pCtx                The SHA1 context.
 */
DECLINLINE(void) rtSha1BlockInitBuffered(PRTSHA1CONTEXT pCtx)
{
    unsigned iWord;
    for (iWord = 0; iWord < 16; iWord++)
        pCtx->AltPrivate.auW[iWord] = RT_BE2H_U32(pCtx->AltPrivate.auW[iWord]);

    for (; iWord < RT_ELEMENTS(pCtx->AltPrivate.auW); iWord++)
    {
        uint32_t u32 = pCtx->AltPrivate.auW[iWord - 16];
        u32         ^= pCtx->AltPrivate.auW[iWord - 14];
        u32         ^= pCtx->AltPrivate.auW[iWord -  8];
        u32         ^= pCtx->AltPrivate.auW[iWord -  3];
        pCtx->AltPrivate.auW[iWord] = ASMRotateLeftU32(u32, 1);
    }
}


/**
 * Process the current block.
 *
 * Requires one of the rtSha1BlockInit functions to be called first.
 *
 * @param   pCtx                The SHA1 context.
 */
static void rtSha1BlockProcess(PRTSHA1CONTEXT pCtx)
{
    uint32_t uA = pCtx->AltPrivate.auH[0];
    uint32_t uB = pCtx->AltPrivate.auH[1];
    uint32_t uC = pCtx->AltPrivate.auH[2];
    uint32_t uD = pCtx->AltPrivate.auH[3];
    uint32_t uE = pCtx->AltPrivate.auH[4];

#if 1
    unsigned iWord = 0;
# define TWENTY_ITERATIONS(a_iWordStop, a_uK, a_uExprBCD) \
        for (; iWord < a_iWordStop; iWord++) \
        { \
            uint32_t uTemp = ASMRotateLeftU32(uA, 5); \
            uTemp += (a_uExprBCD); \
            uTemp += uE; \
            uTemp += pCtx->AltPrivate.auW[iWord]; \
            uTemp += (a_uK); \
            \
            uE = uD; \
            uD = uC; \
            uC = ASMRotateLeftU32(uB, 30); \
            uB = uA; \
            uA = uTemp; \
        } do { } while (0)
    TWENTY_ITERATIONS(20, UINT32_C(0x5a827999), (uB & uC) | (~uB & uD));
    TWENTY_ITERATIONS(40, UINT32_C(0x6ed9eba1), uB ^ uC ^ uD);
    TWENTY_ITERATIONS(60, UINT32_C(0x8f1bbcdc), (uB & uC) | (uB & uD) | (uC & uD));
    TWENTY_ITERATIONS(80, UINT32_C(0xca62c1d6), uB ^ uC ^ uD);
#else
    for (unsigned iWord = 0; iWord < RT_ELEMENTS(pCtx->AltPrivate.auW); iWord++)
    {
        uint32_t uTemp = ASMRotateLeftU32(uA, 5);
        uTemp += uE;
        uTemp += pCtx->AltPrivate.auW[iWord];
        if (iWord <= 19)
        {
            uTemp += (uB & uC) | (~uB & uD);
            uTemp += UINT32_C(0x5a827999);
        }
        else if (iWord <= 39)
        {
            uTemp += uB ^ uC ^ uD;
            uTemp += UINT32_C(0x6ed9eba1);
        }
        else if (iWord <= 59)
        {
            uTemp += (uB & uC) | (uB & uD) | (uC & uD);
            uTemp += UINT32_C(0x8f1bbcdc);
        }
        else
        {
            uTemp += uB ^ uC ^ uD;
            uTemp += UINT32_C(0xca62c1d6);
        }

        uE = uD;
        uD = uC;
        uC = ASMRotateLeftU32(uB, 30);
        uB = uA;
        uA = uTemp;
    }
#endif

    pCtx->AltPrivate.auH[0] += uA;
    pCtx->AltPrivate.auH[1] += uB;
    pCtx->AltPrivate.auH[2] += uC;
    pCtx->AltPrivate.auH[3] += uD;
    pCtx->AltPrivate.auH[4] += uE;
}


RTDECL(void) RTSha1Update(PRTSHA1CONTEXT pCtx, const void *pvBuf, size_t cbBuf)
{
    Assert(pCtx->AltPrivate.cbMessage < UINT64_MAX / 2);
    uint8_t const *pbBuf = (uint8_t const *)pvBuf;

    /*
     * Deal with buffered bytes first.
     */
    size_t cbBuffered = (size_t)pCtx->AltPrivate.cbMessage & (RTSHA1_BLOCK_SIZE - 1U);
    if (cbBuffered)
    {
        size_t cbMissing = RTSHA1_BLOCK_SIZE - cbBuffered;
        if (cbBuf >= cbMissing)
        {
            memcpy((uint8_t *)&pCtx->AltPrivate.auW[0] + cbBuffered, pbBuf, cbMissing);
            pCtx->AltPrivate.cbMessage += cbMissing;
            pbBuf += cbMissing;
            cbBuf -= cbMissing;

            rtSha1BlockInitBuffered(pCtx);
            rtSha1BlockProcess(pCtx);
        }
        else
        {
            memcpy((uint8_t *)&pCtx->AltPrivate.auW[0] + cbBuffered, pbBuf, cbBuf);
            pCtx->AltPrivate.cbMessage += cbBuf;
            return;
        }
    }

    if (!((uintptr_t)pbBuf & 3))
    {
        /*
         * Process full blocks directly from the input buffer.
         */
        while (cbBuf >= RTSHA1_BLOCK_SIZE)
        {
            rtSha1BlockInit(pCtx, pbBuf);
            rtSha1BlockProcess(pCtx);

            pCtx->AltPrivate.cbMessage += RTSHA1_BLOCK_SIZE;
            pbBuf += RTSHA1_BLOCK_SIZE;
            cbBuf -= RTSHA1_BLOCK_SIZE;
        }
    }
    else
    {
        /*
         * Unaligned input, so buffer it.
         */
        while (cbBuf >= RTSHA1_BLOCK_SIZE)
        {
            memcpy((uint8_t *)&pCtx->AltPrivate.auW[0], pbBuf, RTSHA1_BLOCK_SIZE);
            rtSha1BlockInitBuffered(pCtx);
            rtSha1BlockProcess(pCtx);

            pCtx->AltPrivate.cbMessage += RTSHA1_BLOCK_SIZE;
            pbBuf += RTSHA1_BLOCK_SIZE;
            cbBuf -= RTSHA1_BLOCK_SIZE;
        }
    }

    /*
     * Stash any remaining bytes into the context buffer.
     */
    if (cbBuf > 0)
    {
        memcpy((uint8_t *)&pCtx->AltPrivate.auW[0], pbBuf, cbBuf);
        pCtx->AltPrivate.cbMessage += cbBuf;
    }
}
RT_EXPORT_SYMBOL(RTSha1Update);


RTDECL(void) RTSha1Final(PRTSHA1CONTEXT pCtx, uint8_t pabDigest[RTSHA1_HASH_SIZE])
{
    Assert(pCtx->AltPrivate.cbMessage < UINT64_MAX / 2);

    /*
     * Complete the message by adding a single bit (0x80), padding till
     * the next 448-bit boundrary, the add the message length.
     */
    uint64_t const cMessageBits = pCtx->AltPrivate.cbMessage * 8;

    unsigned cbMissing = RTSHA1_BLOCK_SIZE - ((unsigned)pCtx->AltPrivate.cbMessage & (RTSHA1_BLOCK_SIZE - 1U));
    static uint8_t const s_abSingleBitAndSomePadding[12] =  { 0x80, 0, 0, 0,  0, 0, 0, 0,  0, 0, 0, 0, };
    if (cbMissing < 1U + 8U)
        /* Less than 64+8 bits left in the current block, force a new block. */
        RTSha1Update(pCtx, &s_abSingleBitAndSomePadding, sizeof(s_abSingleBitAndSomePadding));
    else
        RTSha1Update(pCtx, &s_abSingleBitAndSomePadding, 1);

    unsigned cbBuffered = (unsigned)pCtx->AltPrivate.cbMessage & (RTSHA1_BLOCK_SIZE - 1U);
    cbMissing = RTSHA1_BLOCK_SIZE - cbBuffered;
    Assert(cbMissing >= 8);
    memset((uint8_t *)&pCtx->AltPrivate.auW[0] + cbBuffered, 0, cbMissing - 8);

    *(uint64_t *)&pCtx->AltPrivate.auW[14] = RT_H2BE_U64(cMessageBits);

    /*
     * Process the last buffered block constructed/completed above.
     */
    rtSha1BlockInitBuffered(pCtx);
    rtSha1BlockProcess(pCtx);

    /*
     * Convert the byte order of the hash words and we're done.
     */
    pCtx->AltPrivate.auH[0] = RT_H2BE_U32(pCtx->AltPrivate.auH[0]);
    pCtx->AltPrivate.auH[1] = RT_H2BE_U32(pCtx->AltPrivate.auH[1]);
    pCtx->AltPrivate.auH[2] = RT_H2BE_U32(pCtx->AltPrivate.auH[2]);
    pCtx->AltPrivate.auH[3] = RT_H2BE_U32(pCtx->AltPrivate.auH[3]);
    pCtx->AltPrivate.auH[4] = RT_H2BE_U32(pCtx->AltPrivate.auH[4]);

    memcpy(pabDigest, &pCtx->AltPrivate.auH[0], RTSHA1_HASH_SIZE);

    RT_ZERO(pCtx->AltPrivate);
    pCtx->AltPrivate.cbMessage = UINT64_MAX;
}
RT_EXPORT_SYMBOL(RTSha1Final);


RTDECL(void) RTSha1(const void *pvBuf, size_t cbBuf, uint8_t pabDigest[RTSHA1_HASH_SIZE])
{
    RTSHA1CONTEXT Ctx;
    RTSha1Init(&Ctx);
    RTSha1Update(&Ctx, pvBuf, cbBuf);
    RTSha1Final(&Ctx, pabDigest);
}
RT_EXPORT_SYMBOL(RTSha1);


