/* $Id$ */
/** @file
 * IEM - Instruction Implementation in Assembly, portable C variant.
 */

/*
 * Copyright (C) 2011-2022 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include "IEMInternal.h"
#include <VBox/vmm/vmcc.h>
#include <VBox/err.h>
#include <iprt/x86.h>
#include <iprt/uint128.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/
/** @def IEM_WITHOUT_ASSEMBLY
 * Enables all the code in this file.
 */
#if !defined(IEM_WITHOUT_ASSEMBLY)
# if defined(RT_ARCH_ARM32) || defined(RT_ARCH_ARM64) || defined(DOXYGEN_RUNNING)
#  define IEM_WITHOUT_ASSEMBLY
# endif
#endif

/**
 * Calculates the signed flag value given a result and it's bit width.
 *
 * The signed flag (SF) is a duplication of the most significant bit in the
 * result.
 *
 * @returns X86_EFL_SF or 0.
 * @param   a_uResult       Unsigned result value.
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 */
#define X86_EFL_CALC_SF(a_uResult, a_cBitsWidth) \
    ( (uint32_t)((a_uResult) >> ((a_cBitsWidth) - X86_EFL_SF_BIT - 1)) & X86_EFL_SF )

/**
 * Calculates the zero flag value given a result.
 *
 * The zero flag (ZF) indicates whether the result is zero or not.
 *
 * @returns X86_EFL_ZF or 0.
 * @param   a_uResult       Unsigned result value.
 */
#define X86_EFL_CALC_ZF(a_uResult) \
    ( (uint32_t)((a_uResult) == 0) << X86_EFL_ZF_BIT )

/**
 * Extracts the OF flag from a OF calculation result.
 *
 * These are typically used by concating with a bitcount.  The problem is that
 * 8-bit values needs shifting in the other direction than the others.
 */
#define X86_EFL_GET_OF_8(a_uValue)  ((uint32_t)((a_uValue) << (X86_EFL_OF_BIT - 8))  & X86_EFL_OF)
#define X86_EFL_GET_OF_16(a_uValue) ((uint32_t)((a_uValue) >> (16 - X86_EFL_OF_BIT)) & X86_EFL_OF)
#define X86_EFL_GET_OF_32(a_uValue) ((uint32_t)((a_uValue) >> (32 - X86_EFL_OF_BIT)) & X86_EFL_OF)
#define X86_EFL_GET_OF_64(a_uValue) ((uint32_t)((a_uValue) >> (64 - X86_EFL_OF_BIT)) & X86_EFL_OF)

/**
 * Updates the status bits (CF, PF, AF, ZF, SF, and OF) after arithmetic op.
 *
 * @returns Status bits.
 * @param   a_pfEFlags      Pointer to the 32-bit EFLAGS value to update.
 * @param   a_uResult       Unsigned result value.
 * @param   a_uSrc          The source value (for AF calc).
 * @param   a_uDst          The original destination value (for AF calc).
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 * @param   a_CfExpr        Bool expression for the carry flag (CF).
 * @param   a_OfMethod      0 for ADD-style, 1 for SUB-style.
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(a_pfEFlags, a_uResult, a_uDst, a_uSrc, a_cBitsWidth, a_CfExpr, a_OfMethod) \
    do { \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~X86_EFL_STATUS_BITS; \
        fEflTmp |= (a_CfExpr) << X86_EFL_CF_BIT; \
        fEflTmp |= g_afParity[(a_uResult) & 0xff]; \
        fEflTmp |= ((uint32_t)(a_uResult) ^ (uint32_t)(a_uSrc) ^ (uint32_t)(a_uDst)) & X86_EFL_AF; \
        fEflTmp |= X86_EFL_CALC_ZF(a_uResult); \
        fEflTmp |= X86_EFL_CALC_SF(a_uResult, a_cBitsWidth); \
        fEflTmp |= X86_EFL_GET_OF_ ## a_cBitsWidth( ((a_uDst) ^ (a_uSrc) ^ (a_OfMethod == 0 ? RT_BIT_64(a_cBitsWidth - 1) : 0)) \
                                                   & ((a_uResult) ^ (a_uDst)) ); \
        *(a_pfEFlags) = fEflTmp; \
    } while (0)

/**
 * Updates the status bits (CF, PF, AF, ZF, SF, and OF) after a logical op.
 *
 * CF and OF are defined to be 0 by logical operations.  AF on the other hand is
 * undefined.  We do not set AF, as that seems to make the most sense (which
 * probably makes it the most wrong in real life).
 *
 * @returns Status bits.
 * @param   a_pfEFlags      Pointer to the 32-bit EFLAGS value to update.
 * @param   a_uResult       Unsigned result value.
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 * @param   a_fExtra        Additional bits to set.
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(a_pfEFlags, a_uResult, a_cBitsWidth, a_fExtra) \
    do { \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~X86_EFL_STATUS_BITS; \
        fEflTmp |= g_afParity[(a_uResult) & 0xff]; \
        fEflTmp |= X86_EFL_CALC_ZF(a_uResult); \
        fEflTmp |= X86_EFL_CALC_SF(a_uResult, a_cBitsWidth); \
        fEflTmp |= (a_fExtra); \
        *(a_pfEFlags) = fEflTmp; \
    } while (0)


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
/**
 * Parity calculation table.
 *
 * The generator code:
 * @code
 * #include <stdio.h>
 *
 * int main()
 * {
 *     unsigned b;
 *     for (b = 0; b < 256; b++)
 *     {
 *         int cOnes = ( b       & 1)
 *                   + ((b >> 1) & 1)
 *                   + ((b >> 2) & 1)
 *                   + ((b >> 3) & 1)
 *                   + ((b >> 4) & 1)
 *                   + ((b >> 5) & 1)
 *                   + ((b >> 6) & 1)
 *                   + ((b >> 7) & 1);
 *         printf("    /" "* %#04x = %u%u%u%u%u%u%u%ub *" "/ %s,\n",
 *                b,
 *                (b >> 7) & 1,
 *                (b >> 6) & 1,
 *                (b >> 5) & 1,
 *                (b >> 4) & 1,
 *                (b >> 3) & 1,
 *                (b >> 2) & 1,
 *                (b >> 1) & 1,
 *                 b       & 1,
 *                cOnes & 1 ? "0" : "X86_EFL_PF");
 *     }
 *     return 0;
 * }
 * @endcode
 */
static uint8_t const g_afParity[256] =
{
    /* 0000 = 00000000b */ X86_EFL_PF,
    /* 0x01 = 00000001b */ 0,
    /* 0x02 = 00000010b */ 0,
    /* 0x03 = 00000011b */ X86_EFL_PF,
    /* 0x04 = 00000100b */ 0,
    /* 0x05 = 00000101b */ X86_EFL_PF,
    /* 0x06 = 00000110b */ X86_EFL_PF,
    /* 0x07 = 00000111b */ 0,
    /* 0x08 = 00001000b */ 0,
    /* 0x09 = 00001001b */ X86_EFL_PF,
    /* 0x0a = 00001010b */ X86_EFL_PF,
    /* 0x0b = 00001011b */ 0,
    /* 0x0c = 00001100b */ X86_EFL_PF,
    /* 0x0d = 00001101b */ 0,
    /* 0x0e = 00001110b */ 0,
    /* 0x0f = 00001111b */ X86_EFL_PF,
    /* 0x10 = 00010000b */ 0,
    /* 0x11 = 00010001b */ X86_EFL_PF,
    /* 0x12 = 00010010b */ X86_EFL_PF,
    /* 0x13 = 00010011b */ 0,
    /* 0x14 = 00010100b */ X86_EFL_PF,
    /* 0x15 = 00010101b */ 0,
    /* 0x16 = 00010110b */ 0,
    /* 0x17 = 00010111b */ X86_EFL_PF,
    /* 0x18 = 00011000b */ X86_EFL_PF,
    /* 0x19 = 00011001b */ 0,
    /* 0x1a = 00011010b */ 0,
    /* 0x1b = 00011011b */ X86_EFL_PF,
    /* 0x1c = 00011100b */ 0,
    /* 0x1d = 00011101b */ X86_EFL_PF,
    /* 0x1e = 00011110b */ X86_EFL_PF,
    /* 0x1f = 00011111b */ 0,
    /* 0x20 = 00100000b */ 0,
    /* 0x21 = 00100001b */ X86_EFL_PF,
    /* 0x22 = 00100010b */ X86_EFL_PF,
    /* 0x23 = 00100011b */ 0,
    /* 0x24 = 00100100b */ X86_EFL_PF,
    /* 0x25 = 00100101b */ 0,
    /* 0x26 = 00100110b */ 0,
    /* 0x27 = 00100111b */ X86_EFL_PF,
    /* 0x28 = 00101000b */ X86_EFL_PF,
    /* 0x29 = 00101001b */ 0,
    /* 0x2a = 00101010b */ 0,
    /* 0x2b = 00101011b */ X86_EFL_PF,
    /* 0x2c = 00101100b */ 0,
    /* 0x2d = 00101101b */ X86_EFL_PF,
    /* 0x2e = 00101110b */ X86_EFL_PF,
    /* 0x2f = 00101111b */ 0,
    /* 0x30 = 00110000b */ X86_EFL_PF,
    /* 0x31 = 00110001b */ 0,
    /* 0x32 = 00110010b */ 0,
    /* 0x33 = 00110011b */ X86_EFL_PF,
    /* 0x34 = 00110100b */ 0,
    /* 0x35 = 00110101b */ X86_EFL_PF,
    /* 0x36 = 00110110b */ X86_EFL_PF,
    /* 0x37 = 00110111b */ 0,
    /* 0x38 = 00111000b */ 0,
    /* 0x39 = 00111001b */ X86_EFL_PF,
    /* 0x3a = 00111010b */ X86_EFL_PF,
    /* 0x3b = 00111011b */ 0,
    /* 0x3c = 00111100b */ X86_EFL_PF,
    /* 0x3d = 00111101b */ 0,
    /* 0x3e = 00111110b */ 0,
    /* 0x3f = 00111111b */ X86_EFL_PF,
    /* 0x40 = 01000000b */ 0,
    /* 0x41 = 01000001b */ X86_EFL_PF,
    /* 0x42 = 01000010b */ X86_EFL_PF,
    /* 0x43 = 01000011b */ 0,
    /* 0x44 = 01000100b */ X86_EFL_PF,
    /* 0x45 = 01000101b */ 0,
    /* 0x46 = 01000110b */ 0,
    /* 0x47 = 01000111b */ X86_EFL_PF,
    /* 0x48 = 01001000b */ X86_EFL_PF,
    /* 0x49 = 01001001b */ 0,
    /* 0x4a = 01001010b */ 0,
    /* 0x4b = 01001011b */ X86_EFL_PF,
    /* 0x4c = 01001100b */ 0,
    /* 0x4d = 01001101b */ X86_EFL_PF,
    /* 0x4e = 01001110b */ X86_EFL_PF,
    /* 0x4f = 01001111b */ 0,
    /* 0x50 = 01010000b */ X86_EFL_PF,
    /* 0x51 = 01010001b */ 0,
    /* 0x52 = 01010010b */ 0,
    /* 0x53 = 01010011b */ X86_EFL_PF,
    /* 0x54 = 01010100b */ 0,
    /* 0x55 = 01010101b */ X86_EFL_PF,
    /* 0x56 = 01010110b */ X86_EFL_PF,
    /* 0x57 = 01010111b */ 0,
    /* 0x58 = 01011000b */ 0,
    /* 0x59 = 01011001b */ X86_EFL_PF,
    /* 0x5a = 01011010b */ X86_EFL_PF,
    /* 0x5b = 01011011b */ 0,
    /* 0x5c = 01011100b */ X86_EFL_PF,
    /* 0x5d = 01011101b */ 0,
    /* 0x5e = 01011110b */ 0,
    /* 0x5f = 01011111b */ X86_EFL_PF,
    /* 0x60 = 01100000b */ X86_EFL_PF,
    /* 0x61 = 01100001b */ 0,
    /* 0x62 = 01100010b */ 0,
    /* 0x63 = 01100011b */ X86_EFL_PF,
    /* 0x64 = 01100100b */ 0,
    /* 0x65 = 01100101b */ X86_EFL_PF,
    /* 0x66 = 01100110b */ X86_EFL_PF,
    /* 0x67 = 01100111b */ 0,
    /* 0x68 = 01101000b */ 0,
    /* 0x69 = 01101001b */ X86_EFL_PF,
    /* 0x6a = 01101010b */ X86_EFL_PF,
    /* 0x6b = 01101011b */ 0,
    /* 0x6c = 01101100b */ X86_EFL_PF,
    /* 0x6d = 01101101b */ 0,
    /* 0x6e = 01101110b */ 0,
    /* 0x6f = 01101111b */ X86_EFL_PF,
    /* 0x70 = 01110000b */ 0,
    /* 0x71 = 01110001b */ X86_EFL_PF,
    /* 0x72 = 01110010b */ X86_EFL_PF,
    /* 0x73 = 01110011b */ 0,
    /* 0x74 = 01110100b */ X86_EFL_PF,
    /* 0x75 = 01110101b */ 0,
    /* 0x76 = 01110110b */ 0,
    /* 0x77 = 01110111b */ X86_EFL_PF,
    /* 0x78 = 01111000b */ X86_EFL_PF,
    /* 0x79 = 01111001b */ 0,
    /* 0x7a = 01111010b */ 0,
    /* 0x7b = 01111011b */ X86_EFL_PF,
    /* 0x7c = 01111100b */ 0,
    /* 0x7d = 01111101b */ X86_EFL_PF,
    /* 0x7e = 01111110b */ X86_EFL_PF,
    /* 0x7f = 01111111b */ 0,
    /* 0x80 = 10000000b */ 0,
    /* 0x81 = 10000001b */ X86_EFL_PF,
    /* 0x82 = 10000010b */ X86_EFL_PF,
    /* 0x83 = 10000011b */ 0,
    /* 0x84 = 10000100b */ X86_EFL_PF,
    /* 0x85 = 10000101b */ 0,
    /* 0x86 = 10000110b */ 0,
    /* 0x87 = 10000111b */ X86_EFL_PF,
    /* 0x88 = 10001000b */ X86_EFL_PF,
    /* 0x89 = 10001001b */ 0,
    /* 0x8a = 10001010b */ 0,
    /* 0x8b = 10001011b */ X86_EFL_PF,
    /* 0x8c = 10001100b */ 0,
    /* 0x8d = 10001101b */ X86_EFL_PF,
    /* 0x8e = 10001110b */ X86_EFL_PF,
    /* 0x8f = 10001111b */ 0,
    /* 0x90 = 10010000b */ X86_EFL_PF,
    /* 0x91 = 10010001b */ 0,
    /* 0x92 = 10010010b */ 0,
    /* 0x93 = 10010011b */ X86_EFL_PF,
    /* 0x94 = 10010100b */ 0,
    /* 0x95 = 10010101b */ X86_EFL_PF,
    /* 0x96 = 10010110b */ X86_EFL_PF,
    /* 0x97 = 10010111b */ 0,
    /* 0x98 = 10011000b */ 0,
    /* 0x99 = 10011001b */ X86_EFL_PF,
    /* 0x9a = 10011010b */ X86_EFL_PF,
    /* 0x9b = 10011011b */ 0,
    /* 0x9c = 10011100b */ X86_EFL_PF,
    /* 0x9d = 10011101b */ 0,
    /* 0x9e = 10011110b */ 0,
    /* 0x9f = 10011111b */ X86_EFL_PF,
    /* 0xa0 = 10100000b */ X86_EFL_PF,
    /* 0xa1 = 10100001b */ 0,
    /* 0xa2 = 10100010b */ 0,
    /* 0xa3 = 10100011b */ X86_EFL_PF,
    /* 0xa4 = 10100100b */ 0,
    /* 0xa5 = 10100101b */ X86_EFL_PF,
    /* 0xa6 = 10100110b */ X86_EFL_PF,
    /* 0xa7 = 10100111b */ 0,
    /* 0xa8 = 10101000b */ 0,
    /* 0xa9 = 10101001b */ X86_EFL_PF,
    /* 0xaa = 10101010b */ X86_EFL_PF,
    /* 0xab = 10101011b */ 0,
    /* 0xac = 10101100b */ X86_EFL_PF,
    /* 0xad = 10101101b */ 0,
    /* 0xae = 10101110b */ 0,
    /* 0xaf = 10101111b */ X86_EFL_PF,
    /* 0xb0 = 10110000b */ 0,
    /* 0xb1 = 10110001b */ X86_EFL_PF,
    /* 0xb2 = 10110010b */ X86_EFL_PF,
    /* 0xb3 = 10110011b */ 0,
    /* 0xb4 = 10110100b */ X86_EFL_PF,
    /* 0xb5 = 10110101b */ 0,
    /* 0xb6 = 10110110b */ 0,
    /* 0xb7 = 10110111b */ X86_EFL_PF,
    /* 0xb8 = 10111000b */ X86_EFL_PF,
    /* 0xb9 = 10111001b */ 0,
    /* 0xba = 10111010b */ 0,
    /* 0xbb = 10111011b */ X86_EFL_PF,
    /* 0xbc = 10111100b */ 0,
    /* 0xbd = 10111101b */ X86_EFL_PF,
    /* 0xbe = 10111110b */ X86_EFL_PF,
    /* 0xbf = 10111111b */ 0,
    /* 0xc0 = 11000000b */ X86_EFL_PF,
    /* 0xc1 = 11000001b */ 0,
    /* 0xc2 = 11000010b */ 0,
    /* 0xc3 = 11000011b */ X86_EFL_PF,
    /* 0xc4 = 11000100b */ 0,
    /* 0xc5 = 11000101b */ X86_EFL_PF,
    /* 0xc6 = 11000110b */ X86_EFL_PF,
    /* 0xc7 = 11000111b */ 0,
    /* 0xc8 = 11001000b */ 0,
    /* 0xc9 = 11001001b */ X86_EFL_PF,
    /* 0xca = 11001010b */ X86_EFL_PF,
    /* 0xcb = 11001011b */ 0,
    /* 0xcc = 11001100b */ X86_EFL_PF,
    /* 0xcd = 11001101b */ 0,
    /* 0xce = 11001110b */ 0,
    /* 0xcf = 11001111b */ X86_EFL_PF,
    /* 0xd0 = 11010000b */ 0,
    /* 0xd1 = 11010001b */ X86_EFL_PF,
    /* 0xd2 = 11010010b */ X86_EFL_PF,
    /* 0xd3 = 11010011b */ 0,
    /* 0xd4 = 11010100b */ X86_EFL_PF,
    /* 0xd5 = 11010101b */ 0,
    /* 0xd6 = 11010110b */ 0,
    /* 0xd7 = 11010111b */ X86_EFL_PF,
    /* 0xd8 = 11011000b */ X86_EFL_PF,
    /* 0xd9 = 11011001b */ 0,
    /* 0xda = 11011010b */ 0,
    /* 0xdb = 11011011b */ X86_EFL_PF,
    /* 0xdc = 11011100b */ 0,
    /* 0xdd = 11011101b */ X86_EFL_PF,
    /* 0xde = 11011110b */ X86_EFL_PF,
    /* 0xdf = 11011111b */ 0,
    /* 0xe0 = 11100000b */ 0,
    /* 0xe1 = 11100001b */ X86_EFL_PF,
    /* 0xe2 = 11100010b */ X86_EFL_PF,
    /* 0xe3 = 11100011b */ 0,
    /* 0xe4 = 11100100b */ X86_EFL_PF,
    /* 0xe5 = 11100101b */ 0,
    /* 0xe6 = 11100110b */ 0,
    /* 0xe7 = 11100111b */ X86_EFL_PF,
    /* 0xe8 = 11101000b */ X86_EFL_PF,
    /* 0xe9 = 11101001b */ 0,
    /* 0xea = 11101010b */ 0,
    /* 0xeb = 11101011b */ X86_EFL_PF,
    /* 0xec = 11101100b */ 0,
    /* 0xed = 11101101b */ X86_EFL_PF,
    /* 0xee = 11101110b */ X86_EFL_PF,
    /* 0xef = 11101111b */ 0,
    /* 0xf0 = 11110000b */ X86_EFL_PF,
    /* 0xf1 = 11110001b */ 0,
    /* 0xf2 = 11110010b */ 0,
    /* 0xf3 = 11110011b */ X86_EFL_PF,
    /* 0xf4 = 11110100b */ 0,
    /* 0xf5 = 11110101b */ X86_EFL_PF,
    /* 0xf6 = 11110110b */ X86_EFL_PF,
    /* 0xf7 = 11110111b */ 0,
    /* 0xf8 = 11111000b */ 0,
    /* 0xf9 = 11111001b */ X86_EFL_PF,
    /* 0xfa = 11111010b */ X86_EFL_PF,
    /* 0xfb = 11111011b */ 0,
    /* 0xfc = 11111100b */ X86_EFL_PF,
    /* 0xfd = 11111101b */ 0,
    /* 0xfe = 11111110b */ 0,
    /* 0xff = 11111111b */ X86_EFL_PF,
};
#endif /* !RT_ARCH_AMD64 || IEM_WITHOUT_ASSEMBLY */



/*
 * There are a few 64-bit on 32-bit things we'd rather do in C.  Actually, doing
 * it all in C is probably safer atm., optimize what's necessary later, maybe.
 */
#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)


/*********************************************************************************************************************************
*   Binary Operations                                                                                                            *
*********************************************************************************************************************************/

/*
 * ADD
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_add_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uResult < uDst, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_add_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uResult < uDst, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_add_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uResult < uDst, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_add_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uResult < uDst, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * ADC
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_adc_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_add_u64(puDst, uSrc, pfEFlags);
    else
    {
        uint64_t uDst    = *puDst;
        uint64_t uResult = uDst + uSrc + 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uResult <= uDst, 0);
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_adc_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_add_u32(puDst, uSrc, pfEFlags);
    else
    {
        uint32_t uDst    = *puDst;
        uint32_t uResult = uDst + uSrc + 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uResult <= uDst, 0);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_adc_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_add_u16(puDst, uSrc, pfEFlags);
    else
    {
        uint16_t uDst    = *puDst;
        uint16_t uResult = uDst + uSrc + 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uResult <= uDst, 0);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_adc_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_add_u8(puDst, uSrc, pfEFlags);
    else
    {
        uint8_t uDst    = *puDst;
        uint8_t uResult = uDst + uSrc + 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uResult <= uDst, 0);
    }
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * SUB
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uResult < uDst, 1);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uResult < uDst, 1);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uResult < uDst, 1);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uResult < uDst, 1);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * SBB
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_sbb_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_sub_u64(puDst, uSrc, pfEFlags);
    else
    {
        uint64_t uDst    = *puDst;
        uint64_t uResult = uDst - uSrc - 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uResult <= uDst, 1);
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_sbb_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_sub_u32(puDst, uSrc, pfEFlags);
    else
    {
        uint32_t uDst    = *puDst;
        uint32_t uResult = uDst - uSrc - 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uResult <= uDst, 1);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sbb_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_sub_u16(puDst, uSrc, pfEFlags);
    else
    {
        uint16_t uDst    = *puDst;
        uint16_t uResult = uDst - uSrc - 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uResult <= uDst, 1);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sbb_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    if (!(*pfEFlags & X86_EFL_CF))
        iemAImpl_sub_u8(puDst, uSrc, pfEFlags);
    else
    {
        uint8_t uDst    = *puDst;
        uint8_t uResult = uDst - uSrc - 1;
        *puDst = uResult;
        /** @todo verify AF and OF calculations. */
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uResult <= uDst, 1);
    }
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * OR
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_or_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uResult = *puDst | uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 64, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_or_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uResult = *puDst | uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 32, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_or_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uResult = *puDst | uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 16, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_or_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uResult = *puDst | uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 8, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * XOR
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_xor_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uResult = *puDst ^ uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 64, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_xor_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uResult = *puDst ^ uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 32, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xor_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uResult = *puDst ^ uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 16, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xor_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uResult = *puDst ^ uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 8, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * AND
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_and_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uResult = *puDst & uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 64, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_and_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uResult = *puDst & uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 32, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_and_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uResult = *puDst & uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 16, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_and_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uResult = *puDst & uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 8, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * CMP
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_cmp_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uDstTmp = *puDst;
    iemAImpl_sub_u64(&uDstTmp, uSrc, pfEFlags);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_cmp_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uDstTmp = *puDst;
    iemAImpl_sub_u32(&uDstTmp, uSrc, pfEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmp_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uDstTmp = *puDst;
    iemAImpl_sub_u16(&uDstTmp, uSrc, pfEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmp_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uDstTmp = *puDst;
    iemAImpl_sub_u8(&uDstTmp, uSrc, pfEFlags);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * TEST
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_test_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    uint64_t uResult = *puDst & uSrc;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 64, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_test_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uResult = *puDst & uSrc;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 32, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_test_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uResult = *puDst & uSrc;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 16, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_test_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uResult = *puDst & uSrc;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uResult, 8, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * LOCK prefixed variants of the above
 */

/** 64-bit locked binary operand operation. */
# define DO_LOCKED_BIN_OP(a_Mnemonic, a_cBitsWidth) \
    do { \
        uint ## a_cBitsWidth ## _t uOld = ASMAtomicUoReadU ## a_cBitsWidth(puDst); \
        uint ## a_cBitsWidth ## _t uTmp; \
        uint32_t fEflTmp; \
        do \
        { \
            uTmp = uOld; \
            fEflTmp = *pfEFlags; \
            iemAImpl_ ## a_Mnemonic ## _u ## a_cBitsWidth(&uTmp, uSrc, &fEflTmp); \
        } while (!ASMAtomicCmpXchgExU ## a_cBitsWidth(puDst, uTmp, uOld, &uOld)); \
        *pfEFlags = fEflTmp; \
    } while (0)


#define EMIT_LOCKED_BIN_OP(a_Mnemonic, a_cBitsWidth) \
    IEM_DECL_IMPL_DEF(void, iemAImpl_ ## a_Mnemonic ## _u ## a_cBitsWidth ##  _locked,(uint ## a_cBitsWidth ## _t *puDst, \
                                                                                       uint ## a_cBitsWidth ## _t uSrc, \
                                                                                       uint32_t *pfEFlags)) \
    { \
        DO_LOCKED_BIN_OP(a_Mnemonic, a_cBitsWidth); \
    }

EMIT_LOCKED_BIN_OP(add, 64)
EMIT_LOCKED_BIN_OP(adc, 64)
EMIT_LOCKED_BIN_OP(sub, 64)
EMIT_LOCKED_BIN_OP(sbb, 64)
EMIT_LOCKED_BIN_OP(or,  64)
EMIT_LOCKED_BIN_OP(xor, 64)
EMIT_LOCKED_BIN_OP(and, 64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_LOCKED_BIN_OP(add, 32)
EMIT_LOCKED_BIN_OP(adc, 32)
EMIT_LOCKED_BIN_OP(sub, 32)
EMIT_LOCKED_BIN_OP(sbb, 32)
EMIT_LOCKED_BIN_OP(or,  32)
EMIT_LOCKED_BIN_OP(xor, 32)
EMIT_LOCKED_BIN_OP(and, 32)

EMIT_LOCKED_BIN_OP(add, 16)
EMIT_LOCKED_BIN_OP(adc, 16)
EMIT_LOCKED_BIN_OP(sub, 16)
EMIT_LOCKED_BIN_OP(sbb, 16)
EMIT_LOCKED_BIN_OP(or,  16)
EMIT_LOCKED_BIN_OP(xor, 16)
EMIT_LOCKED_BIN_OP(and, 16)

EMIT_LOCKED_BIN_OP(add, 8)
EMIT_LOCKED_BIN_OP(adc, 8)
EMIT_LOCKED_BIN_OP(sub, 8)
EMIT_LOCKED_BIN_OP(sbb, 8)
EMIT_LOCKED_BIN_OP(or,  8)
EMIT_LOCKED_BIN_OP(xor, 8)
EMIT_LOCKED_BIN_OP(and, 8)
# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * Bit operations (same signature as above).
 */

/*
 * BT
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_bt_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 64);
    uint64_t uDst = *puDst;
    if (uDst & RT_BIT_64(uSrc))
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, X86_EFL_CF);
    else
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bt_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 32);
    uint32_t uDst = *puDst;
    if (uDst & RT_BIT_32(uSrc))
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, X86_EFL_CF);
    else
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, 0);
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bt_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 16);
    uint16_t uDst = *puDst;
    if (uDst & RT_BIT_32(uSrc))
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 16, X86_EFL_CF);
    else
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 16, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * BTC
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 64);
    uint64_t fMask = RT_BIT_64(uSrc);
    uint64_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, X86_EFL_CF);
    }
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, 0);
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 32);
    uint32_t fMask = RT_BIT_32(uSrc);
    uint32_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, X86_EFL_CF);
    }
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, 0);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 16);
    uint16_t fMask = RT_BIT_32(uSrc);
    uint16_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 16, X86_EFL_CF);
    }
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 16, 0);
    }
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * BTR
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_btr_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 64);
    uint64_t fMask = RT_BIT_64(uSrc);
    uint64_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, X86_EFL_CF);
    }
    else
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, 0);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_btr_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 32);
    uint32_t fMask = RT_BIT_32(uSrc);
    uint32_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, X86_EFL_CF);
    }
    else
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, 0);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_btr_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 16);
    uint16_t fMask = RT_BIT_32(uSrc);
    uint16_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 16, X86_EFL_CF);
    }
    else
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 16, 0);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * BTS
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_bts_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 64);
    uint64_t fMask = RT_BIT_64(uSrc);
    uint64_t uDst = *puDst;
    if (uDst & fMask)
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, X86_EFL_CF);
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 64, 0);
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bts_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 32);
    uint32_t fMask = RT_BIT_32(uSrc);
    uint32_t uDst = *puDst;
    if (uDst & fMask)
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, X86_EFL_CF);
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, 0);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_bts_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  We set them as after an
       logical operation (AND/OR/whatever). */
    Assert(uSrc < 16);
    uint16_t fMask = RT_BIT_32(uSrc);
    uint32_t uDst = *puDst;
    if (uDst & fMask)
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, X86_EFL_CF);
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_LOGIC(pfEFlags, uDst, 32, 0);
    }
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


EMIT_LOCKED_BIN_OP(btc, 64)
EMIT_LOCKED_BIN_OP(btr, 64)
EMIT_LOCKED_BIN_OP(bts, 64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_LOCKED_BIN_OP(btc, 32)
EMIT_LOCKED_BIN_OP(btr, 32)
EMIT_LOCKED_BIN_OP(bts, 32)

EMIT_LOCKED_BIN_OP(btc, 16)
EMIT_LOCKED_BIN_OP(btr, 16)
EMIT_LOCKED_BIN_OP(bts, 16)
# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * BSF - first (least significant) bit set
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    /** @todo check what real CPUs do. */
    unsigned iBit = ASMBitFirstSetU64(uSrc);
    if (iBit)
    {
        *puDst     = iBit - 1;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    /** @todo check what real CPUs do. */
    unsigned iBit = ASMBitFirstSetU32(uSrc);
    if (iBit)
    {
        *puDst     = iBit - 1;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    /** @todo check what real CPUs do. */
    unsigned iBit = ASMBitFirstSetU16(uSrc);
    if (iBit)
    {
        *puDst     = iBit - 1;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * BSR - last (most significant) bit set
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    /** @todo check what real CPUs do. */
    unsigned iBit = ASMBitLastSetU64(uSrc);
    if (uSrc)
    {
        *puDst     = iBit - 1;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    /** @todo check what real CPUs do. */
    unsigned iBit = ASMBitLastSetU32(uSrc);
    if (uSrc)
    {
        *puDst     = iBit - 1;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, AF, PF, CF. */
    /** @todo check what real CPUs do. */
    unsigned iBit = ASMBitLastSetU16(uSrc);
    if (uSrc)
    {
        *puDst     = iBit - 1;
        *pfEFlags &= ~X86_EFL_ZF;
    }
    else
        *pfEFlags |= X86_EFL_ZF;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * XCHG
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64,(uint64_t *puMem, uint64_t *puReg))
{
    /* XCHG implies LOCK. */
    uint64_t uOldMem = *puMem;
    while (!ASMAtomicCmpXchgExU64(puMem, *puReg, uOldMem, &uOldMem))
        ASMNopPause();
    *puReg = uOldMem;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32,(uint32_t *puMem, uint32_t *puReg))
{
    /* XCHG implies LOCK. */
    uint32_t uOldMem = *puMem;
    while (!ASMAtomicCmpXchgExU32(puMem, *puReg, uOldMem, &uOldMem))
        ASMNopPause();
    *puReg = uOldMem;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16,(uint16_t *puMem, uint16_t *puReg))
{
    /* XCHG implies LOCK. */
    uint16_t uOldMem = *puMem;
    while (!ASMAtomicCmpXchgExU16(puMem, *puReg, uOldMem, &uOldMem))
        ASMNopPause();
    *puReg = uOldMem;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8,(uint8_t *puMem, uint8_t *puReg))
{
    /* XCHG implies LOCK. */
    uint8_t uOldMem = *puMem;
    while (!ASMAtomicCmpXchgExU8(puMem, *puReg, uOldMem, &uOldMem))
        ASMNopPause();
    *puReg = uOldMem;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * XADD and LOCK XADD.
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64,(uint64_t *puDst, uint64_t *puReg, uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = uDst;
    iemAImpl_add_u64(&uResult, *puReg, pfEFlags);
    *puDst = uResult;
    *puReg = uDst;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u64_locked,(uint64_t *puDst, uint64_t *puReg, uint32_t *pfEFlags))
{
    uint64_t uOld = ASMAtomicUoReadU64(puDst);
    uint64_t uTmpDst;
    uint32_t fEflTmp;
    do
    {
        uTmpDst = uOld;
        fEflTmp = *pfEFlags;
        iemAImpl_add_u64(&uTmpDst, *puReg, pfEFlags);
    } while (!ASMAtomicCmpXchgExU64(puDst, uTmpDst, uOld, &uOld));
    *puReg    = uOld;
    *pfEFlags = fEflTmp;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u32,(uint32_t *puDst, uint32_t *puReg, uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst;
    iemAImpl_add_u32(&uResult, *puReg, pfEFlags);
    *puDst = uResult;
    *puReg = uDst;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u32_locked,(uint32_t *puDst, uint32_t *puReg, uint32_t *pfEFlags))
{
    uint32_t uOld = ASMAtomicUoReadU32(puDst);
    uint32_t uTmpDst;
    uint32_t fEflTmp;
    do
    {
        uTmpDst = uOld;
        fEflTmp = *pfEFlags;
        iemAImpl_add_u32(&uTmpDst, *puReg, pfEFlags);
    } while (!ASMAtomicCmpXchgExU32(puDst, uTmpDst, uOld, &uOld));
    *puReg    = uOld;
    *pfEFlags = fEflTmp;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u16,(uint16_t *puDst, uint16_t *puReg, uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst;
    iemAImpl_add_u16(&uResult, *puReg, pfEFlags);
    *puDst = uResult;
    *puReg = uDst;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u16_locked,(uint16_t *puDst, uint16_t *puReg, uint32_t *pfEFlags))
{
    uint16_t uOld = ASMAtomicUoReadU16(puDst);
    uint16_t uTmpDst;
    uint32_t fEflTmp;
    do
    {
        uTmpDst = uOld;
        fEflTmp = *pfEFlags;
        iemAImpl_add_u16(&uTmpDst, *puReg, pfEFlags);
    } while (!ASMAtomicCmpXchgExU16(puDst, uTmpDst, uOld, &uOld));
    *puReg    = uOld;
    *pfEFlags = fEflTmp;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u8,(uint8_t *puDst, uint8_t *puReg, uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst;
    iemAImpl_add_u8(&uResult, *puReg, pfEFlags);
    *puDst = uResult;
    *puReg = uDst;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u8_locked,(uint8_t *puDst, uint8_t *puReg, uint32_t *pfEFlags))
{
    uint8_t uOld = ASMAtomicUoReadU8(puDst);
    uint8_t uTmpDst;
    uint32_t fEflTmp;
    do
    {
        uTmpDst = uOld;
        fEflTmp = *pfEFlags;
        iemAImpl_add_u8(&uTmpDst, *puReg, pfEFlags);
    } while (!ASMAtomicCmpXchgExU8(puDst, uTmpDst, uOld, &uOld));
    *puReg    = uOld;
    *pfEFlags = fEflTmp;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */
#endif

/*
 * CMPXCHG, CMPXCHG8B, CMPXCHG16B
 *
 * Note! We don't have non-locking/atomic cmpxchg primitives, so all cmpxchg
 *       instructions are emulated as locked.
 */
#if defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u8_locked, (uint8_t  *pu8Dst,  uint8_t  *puAl,  uint8_t  uSrcReg, uint32_t *pEFlags))
{
    uint8_t const uOld = *puAl;
    if (ASMAtomicCmpXchgExU8(pu8Dst, uSrcReg, uOld, puAl))
    {
        Assert(*puAl == uOld);
        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16_locked,(uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags))
{
    uint16_t const uOld = *puAx;
    if (ASMAtomicCmpXchgExU16(pu16Dst, uSrcReg, uOld, puAx))
    {
        Assert(*puAx == uOld);
        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32_locked,(uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags))
{
    uint32_t const uOld = *puEax;
    if (ASMAtomicCmpXchgExU32(pu32Dst, uSrcReg, uOld, puEax))
    {
        Assert(*puEax == uOld);
        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}


# if ARCH_BITS == 32
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64_locked,(uint64_t *pu64Dst, uint64_t *puRax, uint64_t *puSrcReg, uint32_t *pEFlags))
# else
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64_locked,(uint64_t *pu64Dst, uint64_t *puRax, uint64_t uSrcReg, uint32_t *pEFlags))
# endif
{
# if ARCH_BITS == 32
    uint64_t const uSrcReg = *puSrcReg;
# endif
    uint64_t const uOld = *puRax;
    if (ASMAtomicCmpXchgExU64(pu64Dst, uSrcReg, uOld, puRax))
    {
        Assert(*puRax == uOld);
        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b_locked,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx,
                                                   uint32_t *pEFlags))
{
    uint64_t const uNew = pu64EbxEcx->u;
    uint64_t const uOld = pu64EaxEdx->u;
    if (ASMAtomicCmpXchgExU64(pu64Dst, uNew, uOld, &pu64EaxEdx->u))
    {
        Assert(pu64EaxEdx->u == uOld);
        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}


# if defined(RT_ARCH_AMD64) || defined(RT_ARCH_ARM64)
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b_locked,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx, PRTUINT128U pu128RbxRcx,
                                                    uint32_t *pEFlags))
{
#  ifdef VBOX_STRICT
    RTUINT128U const uOld = *pu128RaxRdx;
#  endif
#  if defined(RT_ARCH_AMD64)
    if (ASMAtomicCmpXchgU128v2(&pu128Dst->u, pu128RbxRcx->s.Hi, pu128RbxRcx->s.Lo, pu128RaxRdx->s.Hi, pu128RaxRdx->s.Lo,
                               &pu128RaxRdx->u))
#  else
    if (ASMAtomicCmpXchgU128(&pu128Dst->u, pu128RbxRcx->u, pu128RaxRdx->u, &pu128RaxRdx->u))
#  endif
    {
        Assert(pu128RaxRdx->s.Lo == uOld.s.Lo && pu128RaxRdx->s.Hi == uOld.s.Hi);
        *pEFlags |= X86_EFL_ZF;
    }
    else
        *pEFlags &= ~X86_EFL_ZF;
}
# endif

#endif /* defined(IEM_WITHOUT_ASSEMBLY) */


# if !defined(RT_ARCH_ARM64) /** @todo may need this for unaligned accesses... */
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b_fallback,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx,
                                                      PRTUINT128U pu128RbxRcx, uint32_t *pEFlags))
{
    RTUINT128U u128Tmp = *pu128Dst;
    if (   u128Tmp.s.Lo == pu128RaxRdx->s.Lo
        && u128Tmp.s.Hi == pu128RaxRdx->s.Hi)
    {
        *pu128Dst = *pu128RbxRcx;
        *pEFlags |= X86_EFL_ZF;
    }
    else
    {
        *pu128RaxRdx = u128Tmp;
        *pEFlags &= ~X86_EFL_ZF;
    }
}
#endif /* !RT_ARCH_ARM64 */


/* Unlocked versions mapped to the locked ones: */

IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u8,        (uint8_t  *pu8Dst,  uint8_t  *puAl,  uint8_t  uSrcReg, uint32_t *pEFlags))
{
    iemAImpl_cmpxchg_u8_locked(pu8Dst, puAl, uSrcReg, pEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16,       (uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags))
{
    iemAImpl_cmpxchg_u16_locked(pu16Dst, puAx, uSrcReg, pEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32,       (uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags))
{
    iemAImpl_cmpxchg_u32_locked(pu32Dst, puEax, uSrcReg, pEFlags);
}


# if ARCH_BITS == 32
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64,       (uint64_t *pu64Dst, uint64_t *puRax, uint64_t *puSrcReg, uint32_t *pEFlags))
{
    iemAImpl_cmpxchg_u64_locked(pu64Dst, puRax, puSrcReg, pEFlags);
}
# else
IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u64,       (uint64_t *pu64Dst, uint64_t *puRax, uint64_t uSrcReg, uint32_t *pEFlags))
{
    iemAImpl_cmpxchg_u64_locked(pu64Dst, puRax, uSrcReg, pEFlags);
}
# endif


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg8b,(uint64_t *pu64Dst, PRTUINT64U pu64EaxEdx, PRTUINT64U pu64EbxEcx, uint32_t *pEFlags))
{
    iemAImpl_cmpxchg8b_locked(pu64Dst, pu64EaxEdx, pu64EbxEcx, pEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg16b,(PRTUINT128U pu128Dst, PRTUINT128U pu128RaxRdx, PRTUINT128U pu128RbxRcx,
                                             uint32_t *pEFlags))
{
    iemAImpl_cmpxchg16b_locked(pu128Dst, pu128RaxRdx, pu128RbxRcx, pEFlags);
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)

/*
 * MUL
 */

IEM_DECL_IMPL_DEF(int, iemAImpl_mul_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Factor, uint32_t *pfEFlags))
{
    RTUINT128U Result;
    RTUInt128MulU64ByU64(&Result, *pu64RAX, u64Factor);
    *pu64RAX = Result.s.Lo;
    *pu64RDX = Result.s.Hi;

    /* MUL EFLAGS according to Skylake (similar to IMUL). */
    *pfEFlags &= ~(X86_EFL_SF | X86_EFL_CF | X86_EFL_OF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_PF);
    if (Result.s.Lo & RT_BIT_64(63))
        *pfEFlags |= X86_EFL_SF;
    *pfEFlags |= g_afParity[Result.s.Lo & 0xff]; /* (Skylake behaviour) */
    if (Result.s.Hi != 0)
        *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
    return 0;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(int, iemAImpl_mul_u32,(uint32_t *pu32RAX, uint32_t *pu32RDX, uint32_t u32Factor, uint32_t *pfEFlags))
{
    RTUINT64U Result;
    Result.u = (uint64_t)*pu32RAX * u32Factor;
    *pu32RAX = Result.s.Lo;
    *pu32RDX = Result.s.Hi;

    /* MUL EFLAGS according to Skylake (similar to IMUL). */
    *pfEFlags &= ~(X86_EFL_SF | X86_EFL_CF | X86_EFL_OF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_PF);
    if (Result.s.Lo & RT_BIT_32(31))
        *pfEFlags |= X86_EFL_SF;
    *pfEFlags |= g_afParity[Result.s.Lo & 0xff]; /* (Skylake behaviour) */
    if (Result.s.Hi != 0)
        *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
    return 0;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_mul_u16,(uint16_t *pu16RAX, uint16_t *pu16RDX, uint16_t u16Factor, uint32_t *pfEFlags))
{
    RTUINT32U Result;
    Result.u = (uint32_t)*pu16RAX * u16Factor;
    *pu16RAX = Result.s.Lo;
    *pu16RDX = Result.s.Hi;

    /* MUL EFLAGS according to Skylake (similar to IMUL). */
    *pfEFlags &= ~(X86_EFL_SF | X86_EFL_CF | X86_EFL_OF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_PF);
    if (Result.s.Lo & RT_BIT_32(15))
        *pfEFlags |= X86_EFL_SF;
    *pfEFlags |= g_afParity[Result.s.Lo & 0xff]; /* (Skylake behaviour) */
    if (Result.s.Hi != 0)
        *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
    return 0;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * IMUL
 */

IEM_DECL_IMPL_DEF(int, iemAImpl_imul_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Factor, uint32_t *pfEFlags))
{
    RTUINT128U Result;
    *pfEFlags &= ~( X86_EFL_SF | X86_EFL_CF | X86_EFL_OF
                   /* Skylake always clears: */ | X86_EFL_AF | X86_EFL_ZF
                   /* Skylake may set: */       | X86_EFL_PF);

    if ((int64_t)*pu64RAX >= 0)
    {
        if ((int64_t)u64Factor >= 0)
        {
            RTUInt128MulU64ByU64(&Result, *pu64RAX, u64Factor);
            if (Result.s.Hi != 0 || Result.s.Lo >= UINT64_C(0x8000000000000000))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
        }
        else
        {
            RTUInt128MulU64ByU64(&Result, *pu64RAX, UINT64_C(0) - u64Factor);
            if (Result.s.Hi != 0 || Result.s.Lo > UINT64_C(0x8000000000000000))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
            RTUInt128AssignNeg(&Result);
        }
    }
    else
    {
        if ((int64_t)u64Factor >= 0)
        {
            RTUInt128MulU64ByU64(&Result, UINT64_C(0) - *pu64RAX, u64Factor);
            if (Result.s.Hi != 0 || Result.s.Lo > UINT64_C(0x8000000000000000))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
            RTUInt128AssignNeg(&Result);
        }
        else
        {
            RTUInt128MulU64ByU64(&Result, UINT64_C(0) - *pu64RAX, UINT64_C(0) - u64Factor);
            if (Result.s.Hi != 0 || Result.s.Lo >= UINT64_C(0x8000000000000000))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
        }
    }
    *pu64RAX = Result.s.Lo;
    if (Result.s.Lo & RT_BIT_64(63))
        *pfEFlags |= X86_EFL_SF;
    *pfEFlags |= g_afParity[Result.s.Lo & 0xff]; /* (Skylake behaviour) */
    *pu64RDX = Result.s.Hi;

    return 0;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
/** @todo Testcase: IMUL 2 and 3 operands. */
    uint64_t u64Ign;
    iemAImpl_imul_u64(puDst, &u64Ign, uSrc, pfEFlags);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(int, iemAImpl_imul_u32,(uint32_t *pu32RAX, uint32_t *pu32RDX, uint32_t u32Factor, uint32_t *pfEFlags))
{
    RTUINT64U Result;
    *pfEFlags &= ~( X86_EFL_SF | X86_EFL_CF | X86_EFL_OF
                   /* Skylake always clears: */ | X86_EFL_AF | X86_EFL_ZF
                   /* Skylake may set: */       | X86_EFL_PF);

    if ((int32_t)*pu32RAX >= 0)
    {
        if ((int32_t)u32Factor >= 0)
        {
            Result.u = (uint64_t)*pu32RAX * u32Factor;
            if (Result.s.Hi != 0 || Result.s.Lo >= RT_BIT_32(31))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
        }
        else
        {
            Result.u = (uint64_t)*pu32RAX * (UINT32_C(0) - u32Factor);
            if (Result.s.Hi != 0 || Result.s.Lo > RT_BIT_32(31))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
            Result.u = UINT64_C(0) - Result.u;
        }
    }
    else
    {
        if ((int32_t)u32Factor >= 0)
        {
            Result.u = (uint64_t)(UINT32_C(0) - *pu32RAX) * u32Factor;
            if (Result.s.Hi != 0 || Result.s.Lo > RT_BIT_32(31))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
            Result.u = UINT64_C(0) - Result.u;
        }
        else
        {
            Result.u = (uint64_t)(UINT32_C(0) - *pu32RAX) * (UINT32_C(0) - u32Factor);
            if (Result.s.Hi != 0 || Result.s.Lo >= RT_BIT_32(31))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
        }
    }
    *pu32RAX = Result.s.Lo;
    if (Result.s.Lo & RT_BIT_32(31))
        *pfEFlags |= X86_EFL_SF;
    *pfEFlags |= g_afParity[Result.s.Lo & 0xff]; /* (Skylake behaviour) */
    *pu32RDX = Result.s.Hi;

    return 0;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
/** @todo Testcase: IMUL 2 and 3 operands. */
    uint32_t u32Ign;
    iemAImpl_imul_u32(puDst, &u32Ign, uSrc, pfEFlags);
}


IEM_DECL_IMPL_DEF(int, iemAImpl_imul_u16,(uint16_t *pu16RAX, uint16_t *pu16RDX, uint16_t u16Factor, uint32_t *pfEFlags))
{
    RTUINT32U Result;
    *pfEFlags &= ~( X86_EFL_SF | X86_EFL_CF | X86_EFL_OF
                   /* Skylake always clears: */ | X86_EFL_AF | X86_EFL_ZF
                   /* Skylake may set: */       | X86_EFL_PF);

    if ((int16_t)*pu16RAX >= 0)
    {
        if ((int16_t)u16Factor >= 0)
        {
            Result.u = (uint32_t)*pu16RAX * u16Factor;
            if (Result.s.Hi != 0 || Result.s.Lo >= RT_BIT_32(15))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
        }
        else
        {
            Result.u = (uint32_t)*pu16RAX * (UINT16_C(0) - u16Factor);
            if (Result.s.Hi != 0 || Result.s.Lo > RT_BIT_32(15))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
            Result.u = UINT32_C(0) - Result.u;
        }
    }
    else
    {
        if ((int16_t)u16Factor >= 0)
        {
            Result.u = (uint32_t)(UINT16_C(0) - *pu16RAX) * u16Factor;
            if (Result.s.Hi != 0 || Result.s.Lo > RT_BIT_32(15))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
            Result.u = UINT32_C(0) - Result.u;
        }
        else
        {
            Result.u = (uint32_t)(UINT16_C(0) - *pu16RAX) * (UINT16_C(0) - u16Factor);
            if (Result.s.Hi != 0 || Result.s.Lo >= RT_BIT_32(15))
                *pfEFlags |= X86_EFL_CF | X86_EFL_OF;
        }
    }
    *pu16RAX = Result.s.Lo;
    if (Result.s.Lo & RT_BIT_32(15))
        *pfEFlags |= X86_EFL_SF;
    *pfEFlags |= g_afParity[Result.s.Lo & 0xff]; /* (Skylake behaviour) */
    *pu16RDX = Result.s.Hi;

    return 0;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
/** @todo Testcase: IMUL 2 and 3 operands. */
    uint16_t u16Ign;
    iemAImpl_imul_u16(puDst, &u16Ign, uSrc, pfEFlags);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * DIV
 */

IEM_DECL_IMPL_DEF(int, iemAImpl_div_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    if (   u64Divisor != 0
        && *pu64RDX < u64Divisor)
    {
        RTUINT128U Dividend;
        Dividend.s.Lo = *pu64RAX;
        Dividend.s.Hi = *pu64RDX;

        RTUINT128U Divisor;
        Divisor.s.Lo = u64Divisor;
        Divisor.s.Hi = 0;

        RTUINT128U Remainder;
        RTUINT128U Quotient;
# ifdef __GNUC__ /* GCC maybe really annoying in function. */
        Quotient.s.Lo = 0;
        Quotient.s.Hi = 0;
# endif
        RTUInt128DivRem(&Quotient, &Remainder, &Dividend, &Divisor);
        Assert(Quotient.s.Hi == 0);
        Assert(Remainder.s.Hi == 0);

        *pu64RAX = Quotient.s.Lo;
        *pu64RDX = Remainder.s.Lo;
        /** @todo research the undefined DIV flags. */
        return 0;

    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(int, iemAImpl_div_u32,(uint32_t *pu32RAX, uint32_t *pu32RDX, uint32_t u32Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    if (   u32Divisor != 0
        && *pu32RDX < u32Divisor)
    {
        RTUINT64U Dividend;
        Dividend.s.Lo = *pu32RAX;
        Dividend.s.Hi = *pu32RDX;

        RTUINT64U Remainder;
        RTUINT64U Quotient;
        Quotient.u  = Dividend.u / u32Divisor;
        Remainder.u = Dividend.u % u32Divisor;

        *pu32RAX = Quotient.s.Lo;
        *pu32RDX = Remainder.s.Lo;
        /** @todo research the undefined DIV flags. */
        return 0;

    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_div_u16,(uint16_t *pu16RAX, uint16_t *pu16RDX, uint16_t u16Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    if (   u16Divisor != 0
        && *pu16RDX < u16Divisor)
    {
        RTUINT32U Dividend;
        Dividend.s.Lo = *pu16RAX;
        Dividend.s.Hi = *pu16RDX;

        RTUINT32U Remainder;
        RTUINT32U Quotient;
        Quotient.u  = Dividend.u / u16Divisor;
        Remainder.u = Dividend.u % u16Divisor;

        *pu16RAX = Quotient.s.Lo;
        *pu16RDX = Remainder.s.Lo;
        /** @todo research the undefined DIV flags. */
        return 0;

    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_div_u8,(uint8_t *pu8RAX, uint8_t *pu8RDX, uint8_t u8Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    if (   u8Divisor != 0
        && *pu8RDX < u8Divisor)
    {
        RTUINT16U Dividend;
        Dividend.s.Lo = *pu8RAX;
        Dividend.s.Hi = *pu8RDX;

        RTUINT16U Remainder;
        RTUINT16U Quotient;
        Quotient.u  = Dividend.u / u8Divisor;
        Remainder.u = Dividend.u % u8Divisor;

        *pu8RAX = Quotient.s.Lo;
        *pu8RDX = Remainder.s.Lo;
        /** @todo research the undefined DIV flags. */
        return 0;

    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * IDIV
 */

IEM_DECL_IMPL_DEF(int, iemAImpl_idiv_u64,(uint64_t *pu64RAX, uint64_t *pu64RDX, uint64_t u64Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    /** @todo overflow checks   */
    if (u64Divisor != 0)
    {
        /*
         * Convert to unsigned division.
         */
        RTUINT128U Dividend;
        Dividend.s.Lo = *pu64RAX;
        Dividend.s.Hi = *pu64RDX;
        if ((int64_t)*pu64RDX < 0)
            RTUInt128AssignNeg(&Dividend);

        RTUINT128U Divisor;
        Divisor.s.Hi = 0;
        if ((int64_t)u64Divisor >= 0)
            Divisor.s.Lo = u64Divisor;
        else
            Divisor.s.Lo = UINT64_C(0) - u64Divisor;

        RTUINT128U Remainder;
        RTUINT128U Quotient;
# ifdef __GNUC__ /* GCC maybe really annoying. */
        Quotient.s.Lo = 0;
        Quotient.s.Hi = 0;
# endif
        RTUInt128DivRem(&Quotient, &Remainder, &Dividend, &Divisor);

        /*
         * Setup the result, checking for overflows.
         */
        if ((int64_t)u64Divisor >= 0)
        {
            if ((int64_t)*pu64RDX >= 0)
            {
                /* Positive divisor, positive dividend => result positive. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint64_t)INT64_MAX)
                {
                    *pu64RAX = Quotient.s.Lo;
                    *pu64RDX = Remainder.s.Lo;
                    return 0;
                }
            }
            else
            {
                /* Positive divisor, positive dividend => result negative. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= UINT64_C(0x8000000000000000))
                {
                    *pu64RAX = UINT64_C(0) - Quotient.s.Lo;
                    *pu64RDX = UINT64_C(0) - Remainder.s.Lo;
                    return 0;
                }
            }
        }
        else
        {
            if ((int64_t)*pu64RDX >= 0)
            {
                /* Negative divisor, positive dividend => negative quotient, positive remainder. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= UINT64_C(0x8000000000000000))
                {
                    *pu64RAX = UINT64_C(0) - Quotient.s.Lo;
                    *pu64RDX = Remainder.s.Lo;
                    return 0;
                }
            }
            else
            {
                /* Negative divisor, negative dividend => positive quotient, negative remainder. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint64_t)INT64_MAX)
                {
                    *pu64RAX = Quotient.s.Lo;
                    *pu64RDX = UINT64_C(0) - Remainder.s.Lo;
                    return 0;
                }
            }
        }
    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(int, iemAImpl_idiv_u32,(uint32_t *pu32RAX, uint32_t *pu32RDX, uint32_t u32Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    /** @todo overflow checks   */
    if (u32Divisor != 0)
    {
        /*
         * Convert to unsigned division.
         */
        RTUINT64U Dividend;
        Dividend.s.Lo = *pu32RAX;
        Dividend.s.Hi = *pu32RDX;
        if ((int32_t)*pu32RDX < 0)
            Dividend.u = UINT64_C(0) - Dividend.u;

        uint32_t u32DivisorPositive;
        if ((int32_t)u32Divisor >= 0)
            u32DivisorPositive = u32Divisor;
        else
            u32DivisorPositive = UINT32_C(0) - u32Divisor;

        RTUINT64U Remainder;
        RTUINT64U Quotient;
        Quotient.u  = Dividend.u / u32DivisorPositive;
        Remainder.u = Dividend.u % u32DivisorPositive;

        /*
         * Setup the result, checking for overflows.
         */
        if ((int32_t)u32Divisor >= 0)
        {
            if ((int32_t)*pu32RDX >= 0)
            {
                /* Positive divisor, positive dividend => result positive. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint32_t)INT32_MAX)
                {
                    *pu32RAX = Quotient.s.Lo;
                    *pu32RDX = Remainder.s.Lo;
                    return 0;
                }
            }
            else
            {
                /* Positive divisor, positive dividend => result negative. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= RT_BIT_32(31))
                {
                    *pu32RAX = UINT32_C(0) - Quotient.s.Lo;
                    *pu32RDX = UINT32_C(0) - Remainder.s.Lo;
                    return 0;
                }
            }
        }
        else
        {
            if ((int32_t)*pu32RDX >= 0)
            {
                /* Negative divisor, positive dividend => negative quotient, positive remainder. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= RT_BIT_32(31))
                {
                    *pu32RAX = UINT32_C(0) - Quotient.s.Lo;
                    *pu32RDX = Remainder.s.Lo;
                    return 0;
                }
            }
            else
            {
                /* Negative divisor, negative dividend => positive quotient, negative remainder. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint32_t)INT32_MAX)
                {
                    *pu32RAX = Quotient.s.Lo;
                    *pu32RDX = UINT32_C(0) - Remainder.s.Lo;
                    return 0;
                }
            }
        }
    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}


IEM_DECL_IMPL_DEF(int, iemAImpl_idiv_u16,(uint16_t *pu16RAX, uint16_t *pu16RDX, uint16_t u16Divisor, uint32_t *pfEFlags))
{
    /* Note! Skylake leaves all flags alone. */
    RT_NOREF_PV(pfEFlags);

    if (u16Divisor != 0)
    {
        /*
         * Convert to unsigned division.
         */
        RTUINT32U Dividend;
        Dividend.s.Lo = *pu16RAX;
        Dividend.s.Hi = *pu16RDX;
        if ((int16_t)*pu16RDX < 0)
            Dividend.u = UINT32_C(0) - Dividend.u;

        uint16_t u16DivisorPositive;
        if ((int16_t)u16Divisor >= 0)
            u16DivisorPositive = u16Divisor;
        else
            u16DivisorPositive = UINT16_C(0) - u16Divisor;

        RTUINT32U Remainder;
        RTUINT32U Quotient;
        Quotient.u  = Dividend.u / u16DivisorPositive;
        Remainder.u = Dividend.u % u16DivisorPositive;

        /*
         * Setup the result, checking for overflows.
         */
        if ((int16_t)u16Divisor >= 0)
        {
            if ((int16_t)*pu16RDX >= 0)
            {
                /* Positive divisor, positive dividend => result positive. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint16_t)INT16_MAX)
                {
                    *pu16RAX = Quotient.s.Lo;
                    *pu16RDX = Remainder.s.Lo;
                    return 0;
                }
            }
            else
            {
                /* Positive divisor, positive dividend => result negative. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= RT_BIT_32(15))
                {
                    *pu16RAX = UINT16_C(0) - Quotient.s.Lo;
                    *pu16RDX = UINT16_C(0) - Remainder.s.Lo;
                    return 0;
                }
            }
        }
        else
        {
            if ((int16_t)*pu16RDX >= 0)
            {
                /* Negative divisor, positive dividend => negative quotient, positive remainder. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= RT_BIT_32(15))
                {
                    *pu16RAX = UINT16_C(0) - Quotient.s.Lo;
                    *pu16RDX = Remainder.s.Lo;
                    return 0;
                }
            }
            else
            {
                /* Negative divisor, negative dividend => positive quotient, negative remainder. */
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint16_t)INT16_MAX)
                {
                    *pu16RAX = Quotient.s.Lo;
                    *pu16RDX = UINT16_C(0) - Remainder.s.Lo;
                    return 0;
                }
            }
        }
    }
    /* #DE */
    return VERR_IEM_ASPECT_NOT_IMPLEMENTED;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*********************************************************************************************************************************
*   Unary operations.                                                                                                            *
*********************************************************************************************************************************/

/**
 * Updates the status bits (CF, PF, AF, ZF, SF, and OF) for an INC or DEC instruction.
 *
 * CF is NOT modified for hysterical raisins (allegedly for carrying and
 * borrowing in arithmetic loops on intel 8008).
 *
 * @returns Status bits.
 * @param   a_pfEFlags      Pointer to the 32-bit EFLAGS value to update.
 * @param   a_uResult       Unsigned result value.
 * @param   a_uDst          The original destination value (for AF calc).
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 * @param   a_OfMethod      0 for INC-style, 1 for DEC-style.
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(a_pfEFlags, a_uResult, a_uDst, a_cBitsWidth, a_OfMethod) \
    do { \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~X86_EFL_STATUS_BITS & ~X86_EFL_CF; \
        fEflTmp |= g_afParity[(a_uResult) & 0xff]; \
        fEflTmp |= ((uint32_t)(a_uResult) ^ (uint32_t)(a_uDst)) & X86_EFL_AF; \
        fEflTmp |= X86_EFL_CALC_ZF(a_uResult); \
        fEflTmp |= X86_EFL_CALC_SF(a_uResult, a_cBitsWidth); \
        fEflTmp |= X86_EFL_GET_OF_ ## a_cBitsWidth(a_OfMethod == 0 ? (((a_uDst) ^ RT_BIT_64(63)) & (a_uResult)) \
                                                                   : ((a_uDst) & ((a_uResult) ^ RT_BIT_64(63))) ); \
        *(a_pfEFlags) = fEflTmp; \
    } while (0)

/*
 * INC
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_inc_u64,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = uDst + 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 64, 0 /*INC*/);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_inc_u32,(uint32_t  *puDst,  uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst + 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 32, 0 /*INC*/);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_inc_u16,(uint16_t  *puDst,  uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst + 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 16, 0 /*INC*/);
}

IEM_DECL_IMPL_DEF(void, iemAImpl_inc_u8,(uint8_t  *puDst,  uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst + 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 8, 0 /*INC*/);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * DEC
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_dec_u64,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = uDst - 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 64, 1 /*INC*/);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_dec_u32,(uint32_t  *puDst,  uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst - 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 32, 1 /*INC*/);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_dec_u16,(uint16_t  *puDst,  uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst - 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 16, 1 /*INC*/);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_dec_u8,(uint8_t  *puDst,  uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst - 1;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC(pfEFlags, uResult, uDst, 8, 1 /*INC*/);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * NOT
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_not_u64,(uint64_t  *puDst,  uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = ~uDst;
    *puDst = uResult;
    /* EFLAGS are not modified. */
    RT_NOREF_PV(pfEFlags);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_not_u32,(uint32_t  *puDst,  uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = ~uDst;
    *puDst = uResult;
    /* EFLAGS are not modified. */
    RT_NOREF_PV(pfEFlags);
}

IEM_DECL_IMPL_DEF(void, iemAImpl_not_u16,(uint16_t  *puDst,  uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = ~uDst;
    *puDst = uResult;
    /* EFLAGS are not modified. */
    RT_NOREF_PV(pfEFlags);
}

IEM_DECL_IMPL_DEF(void, iemAImpl_not_u8,(uint8_t  *puDst,  uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = ~uDst;
    *puDst = uResult;
    /* EFLAGS are not modified. */
    RT_NOREF_PV(pfEFlags);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * NEG
 */

/**
 * Updates the status bits (CF, PF, AF, ZF, SF, and OF) for an NEG instruction.
 *
 * @returns Status bits.
 * @param   a_pfEFlags      Pointer to the 32-bit EFLAGS value to update.
 * @param   a_uResult       Unsigned result value.
 * @param   a_uDst          The original destination value (for AF calc).
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_NEG(a_pfEFlags, a_uResult, a_uDst, a_cBitsWidth) \
    do { \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~X86_EFL_STATUS_BITS & ~X86_EFL_CF; \
        fEflTmp |= ((a_uDst) != 0) << X86_EFL_CF_BIT; \
        fEflTmp |= g_afParity[(a_uResult) & 0xff]; \
        fEflTmp |= ((uint32_t)(a_uResult) ^ (uint32_t)(a_uDst)) & X86_EFL_AF; \
        fEflTmp |= X86_EFL_CALC_ZF(a_uResult); \
        fEflTmp |= X86_EFL_CALC_SF(a_uResult, a_cBitsWidth); \
        fEflTmp |= X86_EFL_GET_OF_ ## a_cBitsWidth((a_uDst) & (a_uResult)); \
        *(a_pfEFlags) = fEflTmp; \
    } while (0)

IEM_DECL_IMPL_DEF(void, iemAImpl_neg_u64,(uint64_t *puDst, uint32_t *pfEFlags))
{
    uint64_t uDst    = *puDst;
    uint64_t uResult = (uint64_t)0 - uDst;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_NEG(pfEFlags, uResult, uDst, 64);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_neg_u32,(uint32_t *puDst, uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = (uint32_t)0 - uDst;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_NEG(pfEFlags, uResult, uDst, 32);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_neg_u16,(uint16_t *puDst, uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = (uint16_t)0 - uDst;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_NEG(pfEFlags, uResult, uDst, 16);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_neg_u8,(uint8_t *puDst, uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = (uint8_t)0 - uDst;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_NEG(pfEFlags, uResult, uDst, 8);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * Locked variants.
 */

/** Emit a function for doing a locked unary operand operation. */
# define EMIT_LOCKED_UNARY_OP(a_Mnemonic, a_cBitsWidth) \
    IEM_DECL_IMPL_DEF(void, iemAImpl_ ## a_Mnemonic ## _u ## a_cBitsWidth ## _locked,(uint ## a_cBitsWidth ## _t *puDst, \
                                                                                      uint32_t *pfEFlags)) \
    { \
        uint ## a_cBitsWidth ## _t uOld = ASMAtomicUoReadU ## a_cBitsWidth(puDst); \
        uint ## a_cBitsWidth ## _t uTmp; \
        uint32_t fEflTmp; \
        do \
        { \
            uTmp = uOld; \
            fEflTmp = *pfEFlags; \
            iemAImpl_ ## a_Mnemonic ## _u ## a_cBitsWidth(&uTmp, &fEflTmp); \
        } while (!ASMAtomicCmpXchgExU ## a_cBitsWidth(puDst, uTmp, uOld, &uOld)); \
        *pfEFlags = fEflTmp; \
    }

EMIT_LOCKED_UNARY_OP(inc, 64);
EMIT_LOCKED_UNARY_OP(dec, 64);
EMIT_LOCKED_UNARY_OP(not, 64);
EMIT_LOCKED_UNARY_OP(neg, 64);
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_LOCKED_UNARY_OP(inc, 32);
EMIT_LOCKED_UNARY_OP(dec, 32);
EMIT_LOCKED_UNARY_OP(not, 32);
EMIT_LOCKED_UNARY_OP(neg, 32);

EMIT_LOCKED_UNARY_OP(inc, 16);
EMIT_LOCKED_UNARY_OP(dec, 16);
EMIT_LOCKED_UNARY_OP(not, 16);
EMIT_LOCKED_UNARY_OP(neg, 16);

EMIT_LOCKED_UNARY_OP(inc, 8);
EMIT_LOCKED_UNARY_OP(dec, 8);
EMIT_LOCKED_UNARY_OP(not, 8);
EMIT_LOCKED_UNARY_OP(neg, 8);
# endif


/*********************************************************************************************************************************
*   Shifting and Rotating                                                                                                        *
*********************************************************************************************************************************/

/*
 * ROL
 */

/**
 * Updates the status bits (OF and CF) for an ROL instruction.
 *
 * @returns Status bits.
 * @param   a_pfEFlags      Pointer to the 32-bit EFLAGS value to update.
 * @param   a_uResult       Unsigned result value.
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_ROL(a_pfEFlags, a_uResult, a_cBitsWidth) do { \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fCarry = ((a_uResult) & X86_EFL_CF); \
        fEflTmp |= fCarry; \
        fEflTmp |= (((a_uResult) >> (a_cBitsWidth - 1)) ^ fCarry) << X86_EFL_OF_BIT; \
        *(a_pfEFlags) = fEflTmp; \
    } while (0)

IEM_DECL_IMPL_DEF(void, iemAImpl_rol_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 63;
    if (cShift)
    {
        uint64_t uResult = ASMRotateLeftU64(*puDst, cShift);
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROL(pfEFlags, uResult, 64);
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_rol_u32,(uint32_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 31;
    if (cShift)
    {
        uint32_t uResult = ASMRotateLeftU32(*puDst, cShift);
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROL(pfEFlags, uResult, 32);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_rol_u16,(uint16_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 15;
    if (cShift)
    {
        uint16_t uDst = *puDst;
        uint16_t uResult = (uDst << cShift) | (uDst >> (16 - cShift));
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROL(pfEFlags, uResult, 16);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_rol_u8,(uint8_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 7;
    if (cShift)
    {
        uint8_t uDst = *puDst;
        uint8_t uResult = (uDst << cShift) | (uDst >> (8 - cShift));
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROL(pfEFlags, uResult, 8);
    }
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * ROR
 */

/**
 * Updates the status bits (OF and CF) for an ROL instruction.
 *
 * @returns Status bits.
 * @param   a_pfEFlags      Pointer to the 32-bit EFLAGS value to update.
 * @param   a_uResult       Unsigned result value.
 * @param   a_cBitsWidth    The width of the result (8, 16, 32, 64).
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_ROR(a_pfEFlags, a_uResult, a_cBitsWidth) do { \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fCarry = ((a_uResult) >> ((a_cBitsWidth) - 1)) & X86_EFL_CF; \
        fEflTmp |= fCarry; \
        fEflTmp |= (((a_uResult) >> ((a_cBitsWidth) - 2)) ^ fCarry) << X86_EFL_OF_BIT; \
        *(a_pfEFlags) = fEflTmp; \
    } while (0)

IEM_DECL_IMPL_DEF(void, iemAImpl_ror_u64,(uint64_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 63;
    if (cShift)
    {
        uint64_t const uResult = ASMRotateRightU64(*puDst, cShift);
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROR(pfEFlags, uResult, 64);
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_ror_u32,(uint32_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 31;
    if (cShift)
    {
        uint64_t const uResult = ASMRotateRightU32(*puDst, cShift);
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROR(pfEFlags, uResult, 32);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_ror_u16,(uint16_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 15;
    if (cShift)
    {
        uint16_t uDst = *puDst;
        uint16_t uResult;
        uResult  = uDst >> cShift;
        uResult |= uDst << (16 - cShift);
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROR(pfEFlags, uResult, 16);
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_ror_u8,(uint8_t *puDst, uint8_t cShift, uint32_t *pfEFlags))
{
    cShift &= 7;
    if (cShift)
    {
        uint8_t uDst = *puDst;
        uint8_t uResult;
        uResult  = uDst >> cShift;
        uResult |= uDst << (8 - cShift);
        *puDst = uResult;
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ROR(pfEFlags, uResult, 8);
    }
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * RCL
 */
#define EMIT_RCL(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_rcl_u ## a_cBitsWidth,(uint ## a_cBitsWidth ## _t *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ## _t const uDst    = *puDst; \
        uint ## a_cBitsWidth ## _t       uResult = uDst << cShift; \
        if (cShift > 1) \
            uResult |= uDst >> (a_cBitsWidth + 1 - cShift); \
        \
        uint32_t fEfl = *pfEFlags; \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uResult |= (uint ## a_cBitsWidth ## _t)(fEfl & X86_EFL_CF) << (cShift - 1); \
        \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts. */ \
        fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fCarry = (uDst >> (a_cBitsWidth - cShift)) & X86_EFL_CF; \
        fEfl |= fCarry; \
        fEfl |= ((uResult >> (a_cBitsWidth - 1)) ^ fCarry) << X86_EFL_OF_BIT; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_RCL(64);
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCL(32);
EMIT_RCL(16);
EMIT_RCL(8);
# endif


/*
 * RCR
 */
#define EMIT_RCR(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_rcr_u ## a_cBitsWidth,(uint ## a_cBitsWidth ##_t *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ## _t const uDst    = *puDst; \
        uint ## a_cBitsWidth ## _t       uResult = uDst >> cShift; \
        if (cShift > 1) \
            uResult |= uDst << (a_cBitsWidth + 1 - cShift); \
        \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags; \
        uResult |= (uint ## a_cBitsWidth ## _t)(fEfl & X86_EFL_CF) << (a_cBitsWidth - cShift); \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts. */ \
        fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fCarry = (uDst >> (cShift - 1)) & X86_EFL_CF; \
        fEfl |= fCarry; \
        fEfl |= ((uResult >> (a_cBitsWidth - 1)) ^ fCarry) << X86_EFL_OF_BIT; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_RCR(64);
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCR(32);
EMIT_RCR(16);
EMIT_RCR(8);
# endif


/*
 * SHL
 */
#define EMIT_SHL(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_shl_u ## a_cBitsWidth,(uint ## a_cBitsWidth ## _t *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ##_t const uDst  = *puDst; \
        uint ## a_cBitsWidth ##_t       uResult = uDst << cShift; \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts.  The AF bit is undefined, we \
           always set it to zero atm. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        uint32_t fCarry = (uDst >> (a_cBitsWidth - cShift)) & X86_EFL_CF; \
        fEfl |= fCarry; \
        fEfl |= ((uResult >> (a_cBitsWidth - 1)) ^ fCarry) << X86_EFL_OF_BIT; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_SHL(64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHL(32)
EMIT_SHL(16)
EMIT_SHL(8)
# endif


/*
 * SHR
 */
#define EMIT_SHR(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_shr_u ## a_cBitsWidth,(uint ## a_cBitsWidth ## _t *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ## _t const uDst    = *puDst; \
        uint ## a_cBitsWidth ## _t       uResult = uDst >> cShift; \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts.  The AF bit is undefined, we \
           always set it to zero atm. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        fEfl |= (uDst >> (cShift - 1)) & X86_EFL_CF; \
        fEfl |= (uDst >> (a_cBitsWidth - 1)) << X86_EFL_OF_BIT; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_SHR(64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHR(32)
EMIT_SHR(16)
EMIT_SHR(8)
# endif


/*
 * SAR
 */
#define EMIT_SAR(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_sar_u ## a_cBitsWidth,(uint ## a_cBitsWidth ## _t *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ## _t const uDst    = *puDst; \
        uint ## a_cBitsWidth ## _t       uResult = (int ## a_cBitsWidth ## _t)uDst >> cShift; \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts (0).  The AF bit is undefined, \
           we always set it to zero atm. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        fEfl |= (uDst >> (cShift - 1)) & X86_EFL_CF; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_SAR(64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SAR(32)
EMIT_SAR(16)
EMIT_SAR(8)
# endif


/*
 * SHLD
 */
#define EMIT_SHLD(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_shld_u ## a_cBitsWidth,(uint ## a_cBitsWidth ## _t *puDst, \
                                                         uint ## a_cBitsWidth ## _t uSrc, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ## _t const uDst = *puDst; \
        uint ## a_cBitsWidth ## _t       uResult = uDst << cShift; \
        uResult |= uSrc >> (a_cBitsWidth - cShift); \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts.  The AF bit is undefined, \
           we always set it to zero atm. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        fEfl |= (uDst >> (a_cBitsWidth - cShift)) & X86_EFL_CF; \
        fEfl |= (uint32_t)((uDst >> (a_cBitsWidth - 1)) ^ (uint32_t)(uResult >> (a_cBitsWidth - 1))) << X86_EFL_OF_BIT; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_SHLD(64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHLD(32)
EMIT_SHLD(16)
EMIT_SHLD(8)
# endif


/*
 * SHRD
 */
#define EMIT_SHRD(a_cBitsWidth) \
IEM_DECL_IMPL_DEF(void, iemAImpl_shrd_u ## a_cBitsWidth,(uint ## a_cBitsWidth ## _t *puDst, \
                                                         uint ## a_cBitsWidth ## _t uSrc, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        uint ## a_cBitsWidth ## _t const uDst    = *puDst; \
        uint ## a_cBitsWidth ## _t       uResult = uDst >> cShift; \
        uResult |= uSrc << (a_cBitsWidth - cShift); \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts.  The AF bit is undefined, \
           we always set it to zero atm. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        fEfl |= (uDst >> (cShift - 1)) & X86_EFL_CF; \
        fEfl |= (uint32_t)((uDst >> (a_cBitsWidth - 1)) ^ (uint32_t)(uResult >> (a_cBitsWidth - 1))) << X86_EFL_OF_BIT;  \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}
EMIT_SHRD(64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHRD(32)
EMIT_SHRD(16)
EMIT_SHRD(8)
# endif


# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
/*
 * BSWAP
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_bswap_u64,(uint64_t *puDst))
{
    *puDst = ASMByteSwapU64(*puDst);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_bswap_u32,(uint32_t *puDst))
{
    *puDst = ASMByteSwapU32(*puDst);
}


/* Note! undocument, so 32-bit arg */
IEM_DECL_IMPL_DEF(void, iemAImpl_bswap_u16,(uint32_t *puDst))
{
    *puDst = ASMByteSwapU16((uint16_t)*puDst) | (*puDst & UINT32_C(0xffff0000));
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */



# if defined(IEM_WITHOUT_ASSEMBLY)

/*
 * LFENCE, SFENCE & MFENCE.
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_lfence,(void))
{
    ASMReadFence();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sfence,(void))
{
    ASMWriteFence();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_mfence,(void))
{
    ASMMemoryFence();
}


#  ifndef RT_ARCH_ARM64
IEM_DECL_IMPL_DEF(void, iemAImpl_alt_mem_fence,(void))
{
    ASMMemoryFence();
}
#  endif

# endif

#endif /* !RT_ARCH_AMD64 || IEM_WITHOUT_ASSEMBLY */


IEM_DECL_IMPL_DEF(void, iemAImpl_arpl,(uint16_t *pu16Dst, uint16_t u16Src, uint32_t *pfEFlags))
{
    if ((*pu16Dst & X86_SEL_RPL) < (u16Src & X86_SEL_RPL))
    {
        *pu16Dst &= X86_SEL_MASK_OFF_RPL;
        *pu16Dst |= u16Src & X86_SEL_RPL;

        *pfEFlags |= X86_EFL_ZF;
    }
    else
        *pfEFlags &= ~X86_EFL_ZF;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_movsldup,(PCX86FXSTATE pFpuState, PRTUINT128U puDst, PCRTUINT128U puSrc))
{
    RT_NOREF(pFpuState);
    puDst->au32[0] = puSrc->au32[0];
    puDst->au32[1] = puSrc->au32[0];
    puDst->au32[2] = puSrc->au32[2];
    puDst->au32[3] = puSrc->au32[2];
}

#ifdef IEM_WITH_VEX

IEM_DECL_IMPL_DEF(void, iemAImpl_vmovsldup_256_rr,(PX86XSAVEAREA pXState, uint8_t iYRegDst, uint8_t iYRegSrc))
{
    pXState->x87.aXMM[iYRegDst].au32[0] = pXState->x87.aXMM[iYRegSrc].au32[0];
    pXState->x87.aXMM[iYRegDst].au32[1] = pXState->x87.aXMM[iYRegSrc].au32[0];
    pXState->x87.aXMM[iYRegDst].au32[2] = pXState->x87.aXMM[iYRegSrc].au32[2];
    pXState->x87.aXMM[iYRegDst].au32[3] = pXState->x87.aXMM[iYRegSrc].au32[2];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[0] = pXState->u.YmmHi.aYmmHi[iYRegSrc].au32[0];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[1] = pXState->u.YmmHi.aYmmHi[iYRegSrc].au32[0];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[2] = pXState->u.YmmHi.aYmmHi[iYRegSrc].au32[2];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[3] = pXState->u.YmmHi.aYmmHi[iYRegSrc].au32[2];
}


IEM_DECL_IMPL_DEF(void, iemAImpl_vmovsldup_256_rm,(PX86XSAVEAREA pXState, uint8_t iYRegDst, PCRTUINT256U pSrc))
{
    pXState->x87.aXMM[iYRegDst].au32[0]       = pSrc->au32[0];
    pXState->x87.aXMM[iYRegDst].au32[1]       = pSrc->au32[0];
    pXState->x87.aXMM[iYRegDst].au32[2]       = pSrc->au32[2];
    pXState->x87.aXMM[iYRegDst].au32[3]       = pSrc->au32[2];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[0] = pSrc->au32[4];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[1] = pSrc->au32[4];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[2] = pSrc->au32[6];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au32[3] = pSrc->au32[6];
}

#endif /* IEM_WITH_VEX */


IEM_DECL_IMPL_DEF(void, iemAImpl_movshdup,(PCX86FXSTATE pFpuState, PRTUINT128U puDst, PCRTUINT128U puSrc))
{
    RT_NOREF(pFpuState);
    puDst->au32[0] = puSrc->au32[1];
    puDst->au32[1] = puSrc->au32[1];
    puDst->au32[2] = puSrc->au32[3];
    puDst->au32[3] = puSrc->au32[3];
}


IEM_DECL_IMPL_DEF(void, iemAImpl_movddup,(PCX86FXSTATE pFpuState, PRTUINT128U puDst, uint64_t uSrc))
{
    RT_NOREF(pFpuState);
    puDst->au64[0] = uSrc;
    puDst->au64[1] = uSrc;
}

#ifdef IEM_WITH_VEX

IEM_DECL_IMPL_DEF(void, iemAImpl_vmovddup_256_rr,(PX86XSAVEAREA pXState, uint8_t iYRegDst, uint8_t iYRegSrc))
{
    pXState->x87.aXMM[iYRegDst].au64[0] = pXState->x87.aXMM[iYRegSrc].au64[0];
    pXState->x87.aXMM[iYRegDst].au64[1] = pXState->x87.aXMM[iYRegSrc].au64[0];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au64[0] = pXState->u.YmmHi.aYmmHi[iYRegSrc].au64[0];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au64[1] = pXState->u.YmmHi.aYmmHi[iYRegSrc].au64[0];
}

IEM_DECL_IMPL_DEF(void, iemAImpl_vmovddup_256_rm,(PX86XSAVEAREA pXState, uint8_t iYRegDst, PCRTUINT256U pSrc))
{
    pXState->x87.aXMM[iYRegDst].au64[0]       = pSrc->au64[0];
    pXState->x87.aXMM[iYRegDst].au64[1]       = pSrc->au64[0];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au64[0] = pSrc->au64[2];
    pXState->u.YmmHi.aYmmHi[iYRegDst].au64[1] = pSrc->au64[2];
}

#endif /* IEM_WITH_VEX */

