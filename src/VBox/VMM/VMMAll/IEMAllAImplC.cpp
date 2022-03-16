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
#include <iprt/errcore.h>
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
/* IEM_WITH_ASSEMBLY trumps IEM_WITHOUT_ASSEMBLY for tstIEMAImplAsm purposes. */
#ifdef IEM_WITH_ASSEMBLY
# undef IEM_WITHOUT_ASSEMBLY
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
#define X86_EFL_GET_OF_8(a_uValue)  (((uint32_t)(a_uValue) << (X86_EFL_OF_BIT - 8 + 1))  & X86_EFL_OF)
#define X86_EFL_GET_OF_16(a_uValue) ((uint32_t)((a_uValue) >> (16 - X86_EFL_OF_BIT - 1)) & X86_EFL_OF)
#define X86_EFL_GET_OF_32(a_uValue) ((uint32_t)((a_uValue) >> (32 - X86_EFL_OF_BIT - 1)) & X86_EFL_OF)
#define X86_EFL_GET_OF_64(a_uValue) ((uint32_t)((a_uValue) >> (64 - X86_EFL_OF_BIT - 1)) & X86_EFL_OF)

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
 * @param   a_uSrcOf        The a_uSrc value to use for overflow calculation.
 */
#define IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(a_pfEFlags, a_uResult, a_uDst, a_uSrc, a_cBitsWidth, a_CfExpr, a_uSrcOf) \
    do { \
        uint32_t fEflTmp = *(a_pfEFlags); \
        fEflTmp &= ~X86_EFL_STATUS_BITS; \
        fEflTmp |= (a_CfExpr) << X86_EFL_CF_BIT; \
        fEflTmp |= g_afParity[(a_uResult) & 0xff]; \
        fEflTmp |= ((uint32_t)(a_uResult) ^ (uint32_t)(a_uSrc) ^ (uint32_t)(a_uDst)) & X86_EFL_AF; \
        fEflTmp |= X86_EFL_CALC_ZF(a_uResult); \
        fEflTmp |= X86_EFL_CALC_SF(a_uResult, a_cBitsWidth); \
        \
        /* Overflow during ADDition happens when both inputs have the same signed \
           bit value and the result has a different sign bit value. \
           \
           Since subtraction can be rewritten as addition: 2 - 1 == 2 + -1, it \
           follows that for SUBtraction the signed bit value must differ between \
           the two inputs and the result's signed bit diff from the first input. \
           Note! Must xor with sign bit to convert, not do (0 - a_uSrc). \
           \
           See also: http://teaching.idallen.com/dat2343/10f/notes/040_overflow.txt */ \
        fEflTmp |= X86_EFL_GET_OF_ ## a_cBitsWidth(  (  ((uint ## a_cBitsWidth ## _t)~((a_uDst) ^ (a_uSrcOf))) \
                                                      & RT_BIT_64(a_cBitsWidth - 1)) \
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
/**
 * Parity calculation table.
 *
 * This is also used by iemAllAImpl.asm.
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
uint8_t const g_afParity[256] =
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
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uResult < uDst, uSrc);
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_add_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uResult < uDst, uSrc);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_add_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uResult < uDst, uSrc);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_add_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst + uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uResult < uDst, uSrc);
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uResult <= uDst, uSrc);
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uResult <= uDst, uSrc);
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uResult <= uDst, uSrc);
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uResult <= uDst, uSrc);
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
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uDst < uSrc, uSrc ^ RT_BIT_64(63));
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    uint32_t uDst    = *puDst;
    uint32_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uDst < uSrc, uSrc ^ RT_BIT_32(31));
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    uint16_t uDst    = *puDst;
    uint16_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uDst < uSrc, uSrc ^ (uint16_t)0x8000);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_sub_u8,(uint8_t *puDst, uint8_t uSrc, uint32_t *pfEFlags))
{
    uint8_t uDst    = *puDst;
    uint8_t uResult = uDst - uSrc;
    *puDst = uResult;
    IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uDst < uSrc, uSrc ^ (uint8_t)0x80);
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 64, uDst <= uSrc, uSrc ^ RT_BIT_64(63));
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 32, uDst <= uSrc, uSrc ^ RT_BIT_32(31));
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 16, uDst <= uSrc, uSrc ^ (uint16_t)0x8000);
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
        IEM_EFL_UPDATE_STATUS_BITS_FOR_ARITHMETIC(pfEFlags, uResult, uDst, uSrc, 8, uDst <= uSrc, uSrc ^ (uint8_t)0x80);
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
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  However, it seems they're
             not modified by either AMD (3990x) or Intel (i9-9980HK). */
    Assert(uSrc < 64);
    uint64_t uDst = *puDst;
    if (uDst & RT_BIT_64(uSrc))
        *pfEFlags |= X86_EFL_CF;
    else
        *pfEFlags &= ~X86_EFL_CF;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bt_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  However, it seems they're
             not modified by either AMD (3990x) or Intel (i9-9980HK). */
    Assert(uSrc < 32);
    uint32_t uDst = *puDst;
    if (uDst & RT_BIT_32(uSrc))
        *pfEFlags |= X86_EFL_CF;
    else
        *pfEFlags &= ~X86_EFL_CF;
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bt_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  However, it seems they're
             not modified by either AMD (3990x) or Intel (i9-9980HK). */
    Assert(uSrc < 16);
    uint16_t uDst = *puDst;
    if (uDst & RT_BIT_32(uSrc))
        *pfEFlags |= X86_EFL_CF;
    else
        *pfEFlags &= ~X86_EFL_CF;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */

/*
 * BTC
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  However, it seems they're
             not modified by either AMD (3990x) or Intel (i9-9980HK). */
    Assert(uSrc < 64);
    uint64_t fMask = RT_BIT_64(uSrc);
    uint64_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        *pfEFlags |= X86_EFL_CF;
    }
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        *pfEFlags &= ~X86_EFL_CF;
    }
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  However, it seems they're
             not modified by either AMD (3990x) or Intel (i9-9980HK). */
    Assert(uSrc < 32);
    uint32_t fMask = RT_BIT_32(uSrc);
    uint32_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        *pfEFlags |= X86_EFL_CF;
    }
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        *pfEFlags &= ~X86_EFL_CF;
    }
}


IEM_DECL_IMPL_DEF(void, iemAImpl_btc_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    /* Note! "undefined" flags: OF, SF, ZF, AF, PF.  However, it seems they're
             not modified by either AMD (3990x) or Intel (i9-9980HK). */
    Assert(uSrc < 16);
    uint16_t fMask = RT_BIT_32(uSrc);
    uint16_t uDst = *puDst;
    if (uDst & fMask)
    {
        uDst &= ~fMask;
        *puDst = uDst;
        *pfEFlags |= X86_EFL_CF;
    }
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        *pfEFlags &= ~X86_EFL_CF;
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
        *pfEFlags |= X86_EFL_CF;
    }
    else
        *pfEFlags &= ~X86_EFL_CF;
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
        *pfEFlags |= X86_EFL_CF;
    }
    else
        *pfEFlags &= ~X86_EFL_CF;
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
        *pfEFlags |= X86_EFL_CF;
    }
    else
        *pfEFlags &= ~X86_EFL_CF;
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
        *pfEFlags |= X86_EFL_CF;
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        *pfEFlags &= ~X86_EFL_CF;
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
        *pfEFlags |= X86_EFL_CF;
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        *pfEFlags &= ~X86_EFL_CF;
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
        *pfEFlags |= X86_EFL_CF;
    else
    {
        uDst |= fMask;
        *puDst = uDst;
        *pfEFlags &= ~X86_EFL_CF;
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
 * Helpers for BSR and BSF.
 *
 * Note! "undefined" flags: OF, SF, AF, PF, CF.
 *       Intel behavior modelled on 10980xe, AMD on 3990X.  Other marchs may
 *       produce different result (see https://www.sandpile.org/x86/flags.htm),
 *       but we restrict ourselves to emulating these recent marchs.
 */
#define SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlag, a_iBit) do { \
        unsigned iBit = (a_iBit); \
        uint32_t fEfl = *pfEFlags & ~(X86_EFL_OF | X86_EFL_SF | X86_EFL_ZF | X86_EFL_AF | X86_EFL_PF | X86_EFL_CF); \
        if (iBit) \
        { \
            *puDst    = --iBit; \
            fEfl     |= g_afParity[iBit]; \
        } \
        else \
            fEfl     |= X86_EFL_ZF | X86_EFL_PF; \
        *pfEFlags = fEfl; \
    } while (0)
#define SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlag, a_iBit) do { \
        unsigned const iBit = (a_iBit); \
        if (iBit) \
        { \
            *puDst     = iBit - 1; \
            *pfEFlags &= ~X86_EFL_ZF; \
        } \
        else \
            *pfEFlags |= X86_EFL_ZF; \
    } while (0)


/*
 * BSF - first (least significant) bit set
 */
IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitFirstSetU64(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u64_intel,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitFirstSetU64(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u64_amd,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlags, ASMBitFirstSetU64(uSrc));
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitFirstSetU32(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u32_intel,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitFirstSetU32(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u32_amd,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlags, ASMBitFirstSetU32(uSrc));
}


IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitFirstSetU16(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u16_intel,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitFirstSetU16(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsf_u16_amd,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlags, ASMBitFirstSetU16(uSrc));
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * BSR - last (most significant) bit set
 */
IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u64,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitLastSetU64(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u64_intel,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitLastSetU64(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u64_amd,(uint64_t *puDst, uint64_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlags, ASMBitLastSetU64(uSrc));
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u32,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitLastSetU32(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u32_intel,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitLastSetU32(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u32_amd,(uint32_t *puDst, uint32_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlags, ASMBitLastSetU32(uSrc));
}


IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u16,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitLastSetU16(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u16_intel,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_INTEL(puDst, pfEFlags, ASMBitLastSetU16(uSrc));
}

IEM_DECL_IMPL_DEF(void, iemAImpl_bsr_u16_amd,(uint16_t *puDst, uint16_t uSrc, uint32_t *pfEFlags))
{
    SET_BIT_SEARCH_RESULT_AMD(puDst, pfEFlags, ASMBitLastSetU16(uSrc));
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * XCHG
 */

IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_locked,(uint64_t *puMem, uint64_t *puReg))
{
#if ARCH_BITS >= 64
    *puReg = ASMAtomicXchgU64(puMem, *puReg);
#else
    uint64_t uOldMem = *puMem;
    while (!ASMAtomicCmpXchgExU64(puMem, *puReg, uOldMem, &uOldMem))
        ASMNopPause();
    *puReg = uOldMem;
#endif
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_locked,(uint32_t *puMem, uint32_t *puReg))
{
    *puReg = ASMAtomicXchgU32(puMem, *puReg);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_locked,(uint16_t *puMem, uint16_t *puReg))
{
    *puReg = ASMAtomicXchgU16(puMem, *puReg);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_locked,(uint8_t *puMem, uint8_t *puReg))
{
    *puReg = ASMAtomicXchgU8(puMem, *puReg);
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/* Unlocked variants for fDisregardLock mode: */

IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u64_unlocked,(uint64_t *puMem, uint64_t *puReg))
{
    uint64_t const uOld = *puMem;
    *puMem = *puReg;
    *puReg = uOld;
}

# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u32_unlocked,(uint32_t *puMem, uint32_t *puReg))
{
    uint32_t const uOld = *puMem;
    *puMem = *puReg;
    *puReg = uOld;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u16_unlocked,(uint16_t *puMem, uint16_t *puReg))
{
    uint16_t const uOld = *puMem;
    *puMem = *puReg;
    *puReg = uOld;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_xchg_u8_unlocked,(uint8_t *puMem, uint8_t *puReg))
{
    uint8_t const uOld = *puMem;
    *puMem = *puReg;
    *puReg = uOld;
}

# endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */


/*
 * XADD and LOCK XADD.
 */
#define EMIT_XADD(a_cBitsWidth, a_Type) \
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u ## a_cBitsWidth,(a_Type *puDst, a_Type *puReg, uint32_t *pfEFlags)) \
{ \
    a_Type uDst    = *puDst; \
    a_Type uResult = uDst; \
    iemAImpl_add_u ## a_cBitsWidth(&uResult, *puReg, pfEFlags); \
    *puDst = uResult; \
    *puReg = uDst; \
} \
\
IEM_DECL_IMPL_DEF(void, iemAImpl_xadd_u ## a_cBitsWidth ## _locked,(a_Type *puDst, a_Type *puReg, uint32_t *pfEFlags)) \
{ \
    a_Type uOld = ASMAtomicUoReadU ## a_cBitsWidth(puDst); \
    a_Type uResult; \
    uint32_t fEflTmp; \
    do \
    { \
        uResult = uOld; \
        fEflTmp = *pfEFlags; \
        iemAImpl_add_u ## a_cBitsWidth(&uResult, *puReg, &fEflTmp); \
    } while (!ASMAtomicCmpXchgExU ## a_cBitsWidth(puDst, uResult, uOld, &uOld)); \
    *puReg    = uOld; \
    *pfEFlags = fEflTmp; \
}
EMIT_XADD(64, uint64_t)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_XADD(32, uint32_t)
EMIT_XADD(16, uint16_t)
EMIT_XADD(8, uint8_t)
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
    uint8_t uOld = *puAl;
    if (ASMAtomicCmpXchgExU8(pu8Dst, uSrcReg, uOld, puAl))
        Assert(*puAl == uOld);
    iemAImpl_cmp_u8(&uOld, *puAl, pEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u16_locked,(uint16_t *pu16Dst, uint16_t *puAx,  uint16_t uSrcReg, uint32_t *pEFlags))
{
    uint16_t uOld = *puAx;
    if (ASMAtomicCmpXchgExU16(pu16Dst, uSrcReg, uOld, puAx))
        Assert(*puAx == uOld);
    iemAImpl_cmp_u16(&uOld, *puAx, pEFlags);
}


IEM_DECL_IMPL_DEF(void, iemAImpl_cmpxchg_u32_locked,(uint32_t *pu32Dst, uint32_t *puEax, uint32_t uSrcReg, uint32_t *pEFlags))
{
    uint32_t uOld = *puEax;
    if (ASMAtomicCmpXchgExU32(pu32Dst, uSrcReg, uOld, puEax))
        Assert(*puEax == uOld);
    iemAImpl_cmp_u32(&uOld, *puEax, pEFlags);
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
    uint64_t uOld = *puRax;
    if (ASMAtomicCmpXchgExU64(pu64Dst, uSrcReg, uOld, puRax))
        Assert(*puRax == uOld);
    iemAImpl_cmp_u64(&uOld, *puRax, pEFlags);
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

#if defined(IEM_WITHOUT_ASSEMBLY)

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

#endif /* defined(IEM_WITHOUT_ASSEMBLY) */

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)

/*
 * MUL, IMUL, DIV and IDIV helpers.
 *
 * - The U64 versions must use 128-bit intermediates, so we need to abstract the
 *   division step so we can select between using C operators and
 *   RTUInt128DivRem/RTUInt128MulU64ByU64.
 *
 * - The U8 versions work returns output in AL + AH instead of xDX + xAX, with the
 *   IDIV/DIV taking all the input in AX too.  This means we have to abstract some
 *   input loads and the result storing.
 */

DECLINLINE(void) RTUInt128DivRemByU64(PRTUINT128U pQuotient, PRTUINT128U pRemainder, PCRTUINT128U pDividend, uint64_t u64Divisor)
{
# ifdef __GNUC__ /* GCC maybe really annoying in function. */
    pQuotient->s.Lo = 0;
    pQuotient->s.Hi = 0;
# endif
    RTUINT128U Divisor;
    Divisor.s.Lo = u64Divisor;
    Divisor.s.Hi = 0;
    RTUInt128DivRem(pQuotient, pRemainder, pDividend, &Divisor);
}

# define DIV_LOAD(a_Dividend)  \
    a_Dividend.s.Lo = *puA, a_Dividend.s.Hi = *puD
# define DIV_LOAD_U8(a_Dividend) \
    a_Dividend.u = *puAX

# define DIV_STORE(a_Quotient, a_uReminder)    *puA  = (a_Quotient),    *puD = (a_uReminder)
# define DIV_STORE_U8(a_Quotient, a_uReminder) *puAX = (uint8_t)(a_Quotient) | ((uint16_t)(a_uReminder) << 8)

# define MUL_LOAD_F1()                         *puA
# define MUL_LOAD_F1_U8()                      ((uint8_t)*puAX)

# define MUL_STORE(a_Result)                   *puA  = (a_Result).s.Lo, *puD = (a_Result).s.Hi
# define MUL_STORE_U8(a_Result)                *puAX = a_Result.u

# define MULDIV_NEG(a_Value, a_cBitsWidth2x) \
    (a_Value).u = UINT ## a_cBitsWidth2x ## _C(0) - (a_Value).u
# define MULDIV_NEG_U128(a_Value, a_cBitsWidth2x) \
    RTUInt128AssignNeg(&(a_Value))

# define MULDIV_MUL(a_Result, a_Factor1, a_Factor2, a_cBitsWidth2x) \
    (a_Result).u = (uint ## a_cBitsWidth2x ## _t)(a_Factor1) * (a_Factor2)
# define MULDIV_MUL_U128(a_Result, a_Factor1, a_Factor2, a_cBitsWidth2x) \
    RTUInt128MulU64ByU64(&(a_Result), a_Factor1, a_Factor2);

# define MULDIV_MODDIV(a_Quotient, a_Remainder, a_Dividend, a_uDivisor) \
    a_Quotient.u = (a_Dividend).u / (a_uDivisor), \
    a_Remainder.u = (a_Dividend).u % (a_uDivisor)
# define MULDIV_MODDIV_U128(a_Quotient, a_Remainder, a_Dividend, a_uDivisor) \
    RTUInt128DivRemByU64(&a_Quotient, &a_Remainder, &a_Dividend, a_uDivisor)


/*
 * MUL
 */
# define EMIT_MUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnMul, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(int, RT_CONCAT3(iemAImpl_mul_u,a_cBitsWidth,a_Suffix), a_Args) \
{ \
    RTUINT ## a_cBitsWidth2x ## U Result; \
    a_fnMul(Result, a_fnLoadF1(), uFactor, a_cBitsWidth2x); \
    a_fnStore(Result); \
    \
    /* Calc EFLAGS: */ \
    uint32_t fEfl = *pfEFlags; \
    if (a_fIntelFlags) \
    { /* Intel: 6700K and 10980XE behavior */ \
        fEfl &= ~(X86_EFL_SF | X86_EFL_CF | X86_EFL_OF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_PF); \
        if (Result.s.Lo & RT_BIT_64(a_cBitsWidth - 1)) \
            fEfl |= X86_EFL_SF; \
        fEfl |= g_afParity[Result.s.Lo & 0xff]; \
        if (Result.s.Hi != 0) \
            fEfl |= X86_EFL_CF | X86_EFL_OF; \
    } \
    else \
    { /* AMD: 3990X */ \
        if (Result.s.Hi != 0) \
            fEfl |= X86_EFL_CF | X86_EFL_OF; \
        else \
            fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
    } \
    *pfEFlags = fEfl; \
    return 0; \
} \

# define EMIT_MUL(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnMul) \
    EMIT_MUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnMul, RT_NOTHING, 1) \
    EMIT_MUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnMul, _intel,     1) \
    EMIT_MUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnMul, _amd,       0) \

# ifndef DOXYGEN_RUNNING /* this totally confuses doxygen for some reason */
EMIT_MUL(64, 128, (uint64_t *puA, uint64_t *puD, uint64_t uFactor, uint32_t *pfEFlags), (puA, puD, uFactor, pfEFlags),
         MUL_LOAD_F1, MUL_STORE, MULDIV_MUL_U128)
#  if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_MUL(32, 64, (uint32_t *puA, uint32_t *puD, uint32_t uFactor, uint32_t *pfEFlags),  (puA, puD, uFactor, pfEFlags),
         MUL_LOAD_F1, MUL_STORE, MULDIV_MUL)
EMIT_MUL(16, 32, (uint16_t *puA, uint16_t *puD, uint16_t uFactor, uint32_t *pfEFlags),  (puA, puD, uFactor, pfEFlags),
         MUL_LOAD_F1, MUL_STORE, MULDIV_MUL)
EMIT_MUL(8, 16, (uint16_t *puAX, uint8_t uFactor, uint32_t *pfEFlags),                  (puAX,     uFactor, pfEFlags),
         MUL_LOAD_F1_U8, MUL_STORE_U8, MULDIV_MUL)
#  endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */
# endif /* !DOXYGEN_RUNNING */


/*
 * IMUL
 *
 * The SF, ZF, AF and PF flags are "undefined". AMD (3990x) leaves these
 * flags as is.  Whereas Intel skylake (6700K and 10980X (Cascade Lake)) always
 * clear AF and ZF and calculates SF and PF as per the lower half of the result.
 */
# define EMIT_IMUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnNeg, a_fnMul, \
                         a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(int, RT_CONCAT3(iemAImpl_imul_u,a_cBitsWidth,a_Suffix),a_Args) \
{ \
    RTUINT ## a_cBitsWidth2x ## U Result; \
    uint32_t fEfl = *pfEFlags & ~(X86_EFL_CF | X86_EFL_OF); \
    \
    uint ## a_cBitsWidth ## _t const uFactor1 = a_fnLoadF1(); \
    if (!(uFactor1 & RT_BIT_64(a_cBitsWidth - 1))) \
    { \
        if (!(uFactor2 & RT_BIT_64(a_cBitsWidth - 1))) \
        { \
            a_fnMul(Result, uFactor1, uFactor2, a_cBitsWidth2x); \
            if (Result.s.Hi != 0 || Result.s.Lo >= RT_BIT_64(a_cBitsWidth - 1)) \
                fEfl |= X86_EFL_CF | X86_EFL_OF; \
        } \
        else \
        { \
            uint ## a_cBitsWidth ## _t const uPositiveFactor2 = UINT ## a_cBitsWidth ## _C(0) - uFactor2; \
            a_fnMul(Result, uFactor1, uPositiveFactor2, a_cBitsWidth2x); \
            if (Result.s.Hi != 0 || Result.s.Lo > RT_BIT_64(a_cBitsWidth - 1)) \
                fEfl |= X86_EFL_CF | X86_EFL_OF; \
            a_fnNeg(Result, a_cBitsWidth2x); \
        } \
    } \
    else \
    { \
        if (!(uFactor2 & RT_BIT_64(a_cBitsWidth - 1))) \
        { \
            uint ## a_cBitsWidth ## _t const uPositiveFactor1 = UINT ## a_cBitsWidth ## _C(0) - uFactor1; \
            a_fnMul(Result, uPositiveFactor1, uFactor2, a_cBitsWidth2x); \
            if (Result.s.Hi != 0 || Result.s.Lo > RT_BIT_64(a_cBitsWidth - 1)) \
                fEfl |= X86_EFL_CF | X86_EFL_OF; \
            a_fnNeg(Result, a_cBitsWidth2x); \
        } \
        else \
        { \
            uint ## a_cBitsWidth ## _t const uPositiveFactor1 = UINT ## a_cBitsWidth ## _C(0) - uFactor1; \
            uint ## a_cBitsWidth ## _t const uPositiveFactor2 = UINT ## a_cBitsWidth ## _C(0) - uFactor2; \
            a_fnMul(Result, uPositiveFactor1, uPositiveFactor2, a_cBitsWidth2x); \
            if (Result.s.Hi != 0 || Result.s.Lo >= RT_BIT_64(a_cBitsWidth - 1)) \
                fEfl |= X86_EFL_CF | X86_EFL_OF; \
        } \
    } \
    a_fnStore(Result); \
    \
    if (a_fIntelFlags) \
    { \
        fEfl &= ~(X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_PF); \
        if (Result.s.Lo & RT_BIT_64(a_cBitsWidth - 1)) \
            fEfl |= X86_EFL_SF;  \
        fEfl |= g_afParity[Result.s.Lo & 0xff]; \
    } \
    *pfEFlags = fEfl; \
    return 0; \
}
# define EMIT_IMUL(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnNeg, a_fnMul) \
    EMIT_IMUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnNeg, a_fnMul, RT_NOTHING, 1) \
    EMIT_IMUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnNeg, a_fnMul, _intel,     1) \
    EMIT_IMUL_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoadF1, a_fnStore, a_fnNeg, a_fnMul, _amd,       0)

# ifndef DOXYGEN_RUNNING /* this totally confuses doxygen for some reason */
EMIT_IMUL(64, 128, (uint64_t *puA, uint64_t *puD, uint64_t uFactor2, uint32_t *pfEFlags), (puA, puD, uFactor2, pfEFlags),
          MUL_LOAD_F1, MUL_STORE, MULDIV_NEG_U128, MULDIV_MUL_U128)
#  if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_IMUL(32, 64, (uint32_t *puA, uint32_t *puD, uint32_t uFactor2, uint32_t *pfEFlags),  (puA, puD, uFactor2, pfEFlags),
          MUL_LOAD_F1, MUL_STORE, MULDIV_NEG, MULDIV_MUL)
EMIT_IMUL(16, 32, (uint16_t *puA, uint16_t *puD, uint16_t uFactor2, uint32_t *pfEFlags),  (puA, puD, uFactor2, pfEFlags),
          MUL_LOAD_F1, MUL_STORE, MULDIV_NEG, MULDIV_MUL)
EMIT_IMUL(8, 16, (uint16_t *puAX, uint8_t uFactor2, uint32_t *pfEFlags),                  (puAX,     uFactor2, pfEFlags),
          MUL_LOAD_F1_U8, MUL_STORE_U8, MULDIV_NEG, MULDIV_MUL)
#  endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */
# endif /* !DOXYGEN_RUNNING */


/*
 * IMUL with two operands are mapped onto the three operand variant, ignoring
 * the high part of the product.
 */
# define EMIT_IMUL_TWO(a_cBits, a_uType) \
IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u ## a_cBits,(a_uType *puDst, a_uType uSrc, uint32_t *pfEFlags)) \
{ \
    a_uType uIgn; \
    iemAImpl_imul_u ## a_cBits(puDst, &uIgn, uSrc, pfEFlags); \
} \
\
IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u ## a_cBits ## _intel,(a_uType *puDst, a_uType uSrc, uint32_t *pfEFlags)) \
{ \
    a_uType uIgn; \
    iemAImpl_imul_u ## a_cBits ## _intel(puDst, &uIgn, uSrc, pfEFlags); \
} \
\
IEM_DECL_IMPL_DEF(void, iemAImpl_imul_two_u ## a_cBits ## _amd,(a_uType *puDst, a_uType uSrc, uint32_t *pfEFlags)) \
{ \
    a_uType uIgn; \
    iemAImpl_imul_u ## a_cBits ## _amd(puDst, &uIgn, uSrc, pfEFlags); \
}

EMIT_IMUL_TWO(64, uint64_t)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_IMUL_TWO(32, uint32_t)
EMIT_IMUL_TWO(16, uint16_t)
# endif


/*
 * DIV
 */
# define EMIT_DIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnDivRem, \
                        a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(int, RT_CONCAT3(iemAImpl_div_u,a_cBitsWidth,a_Suffix),a_Args) \
{ \
    RTUINT ## a_cBitsWidth2x ## U Dividend; \
    a_fnLoad(Dividend); \
    if (   uDivisor != 0 \
        && Dividend.s.Hi < uDivisor) \
    { \
        RTUINT ## a_cBitsWidth2x ## U Remainder, Quotient; \
        a_fnDivRem(Quotient, Remainder, Dividend, uDivisor); \
        a_fnStore(Quotient.s.Lo, Remainder.s.Lo); \
        \
        /* Calc EFLAGS: Intel 6700K and 10980XE leaves them alone.  AMD 3990X sets AF and clears PF, ZF and SF. */ \
        if (!a_fIntelFlags) \
            *pfEFlags = (*pfEFlags & ~(X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF)) | X86_EFL_AF; \
        return 0; \
    } \
    /* #DE */ \
    return -1; \
}
# define EMIT_DIV(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnDivRem) \
    EMIT_DIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnDivRem, RT_NOTHING, 1) \
    EMIT_DIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnDivRem, _intel,     1) \
    EMIT_DIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnDivRem, _amd,       0)

# ifndef DOXYGEN_RUNNING /* this totally confuses doxygen for some reason */
EMIT_DIV(64,128,(uint64_t *puA, uint64_t *puD, uint64_t uDivisor, uint32_t *pfEFlags), (puA, puD, uDivisor, pfEFlags),
         DIV_LOAD, DIV_STORE, MULDIV_MODDIV_U128)
#  if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_DIV(32,64, (uint32_t *puA, uint32_t *puD, uint32_t uDivisor, uint32_t *pfEFlags), (puA, puD, uDivisor, pfEFlags),
         DIV_LOAD, DIV_STORE, MULDIV_MODDIV)
EMIT_DIV(16,32, (uint16_t *puA, uint16_t *puD, uint16_t uDivisor, uint32_t *pfEFlags), (puA, puD, uDivisor, pfEFlags),
         DIV_LOAD, DIV_STORE, MULDIV_MODDIV)
EMIT_DIV(8,16,  (uint16_t *puAX, uint8_t uDivisor, uint32_t *pfEFlags),                (puAX,     uDivisor, pfEFlags),
         DIV_LOAD_U8, DIV_STORE_U8, MULDIV_MODDIV)
#  endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */
# endif /* !DOXYGEN_RUNNING */


/*
 * IDIV
 *
 * EFLAGS are ignored and left as-is by Intel 6700K and 10980XE.  AMD 3990X will
 * set AF and clear PF, ZF and SF just like it does for DIV.
 *
 */
# define EMIT_IDIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnNeg, a_fnDivRem, \
                         a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(int, RT_CONCAT3(iemAImpl_idiv_u,a_cBitsWidth,a_Suffix),a_Args) \
{ \
    /* Note! Skylake leaves all flags alone. */ \
    \
    /** @todo overflow checks */ \
    if (uDivisor != 0) \
    { \
        /* \
         * Convert to unsigned division. \
         */ \
        RTUINT ## a_cBitsWidth2x ## U Dividend; \
        a_fnLoad(Dividend); \
        bool const fSignedDividend = RT_BOOL(Dividend.s.Hi & RT_BIT_64(a_cBitsWidth - 1)); \
        if (fSignedDividend) \
            a_fnNeg(Dividend, a_cBitsWidth2x); \
        \
        uint ## a_cBitsWidth ## _t uDivisorPositive; \
        if (!(uDivisor & RT_BIT_64(a_cBitsWidth - 1))) \
            uDivisorPositive = uDivisor; \
        else \
            uDivisorPositive = UINT ## a_cBitsWidth ## _C(0) - uDivisor; \
        \
        RTUINT ## a_cBitsWidth2x ## U Remainder, Quotient; \
        a_fnDivRem(Quotient, Remainder, Dividend, uDivisorPositive); \
        \
        /* \
         * Setup the result, checking for overflows. \
         */ \
        if (!(uDivisor & RT_BIT_64(a_cBitsWidth - 1))) \
        { \
            if (!fSignedDividend) \
            { \
                /* Positive divisor, positive dividend => result positive. */ \
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint ## a_cBitsWidth ## _t)INT ## a_cBitsWidth ## _MAX) \
                { \
                    a_fnStore(Quotient.s.Lo, Remainder.s.Lo); \
                    if (!a_fIntelFlags) \
                        *pfEFlags = (*pfEFlags & ~(X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF)) | X86_EFL_AF; \
                    return 0; \
                } \
            } \
            else \
            { \
                /* Positive divisor, negative dividend => result negative. */ \
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= RT_BIT_64(a_cBitsWidth - 1)) \
                { \
                    a_fnStore(UINT ## a_cBitsWidth ## _C(0) - Quotient.s.Lo, UINT ## a_cBitsWidth ## _C(0) - Remainder.s.Lo); \
                    if (!a_fIntelFlags) \
                        *pfEFlags = (*pfEFlags & ~(X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF)) | X86_EFL_AF; \
                    return 0; \
                } \
            } \
        } \
        else \
        { \
            if (!fSignedDividend) \
            { \
                /* Negative divisor, positive dividend => negative quotient, positive remainder. */ \
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= RT_BIT_64(a_cBitsWidth - 1)) \
                { \
                    a_fnStore(UINT ## a_cBitsWidth ## _C(0) - Quotient.s.Lo, Remainder.s.Lo); \
                    if (!a_fIntelFlags) \
                        *pfEFlags = (*pfEFlags & ~(X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF)) | X86_EFL_AF; \
                    return 0; \
                } \
            } \
            else \
            { \
                /* Negative divisor, negative dividend => positive quotient, negative remainder. */ \
                if (Quotient.s.Hi == 0 && Quotient.s.Lo <= (uint ## a_cBitsWidth ## _t)INT ## a_cBitsWidth ## _MAX) \
                { \
                    a_fnStore(Quotient.s.Lo, UINT ## a_cBitsWidth ## _C(0) - Remainder.s.Lo); \
                    if (!a_fIntelFlags) \
                        *pfEFlags = (*pfEFlags & ~(X86_EFL_PF | X86_EFL_ZF | X86_EFL_SF)) | X86_EFL_AF; \
                    return 0; \
                } \
            } \
        } \
    } \
    /* #DE */ \
    return -1; \
}
# define EMIT_IDIV(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnNeg, a_fnDivRem) \
     EMIT_IDIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnNeg, a_fnDivRem, RT_NOTHING, 1) \
     EMIT_IDIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnNeg, a_fnDivRem, _intel,     1) \
     EMIT_IDIV_INNER(a_cBitsWidth, a_cBitsWidth2x, a_Args, a_CallArgs, a_fnLoad, a_fnStore, a_fnNeg, a_fnDivRem, _amd,       0)

# ifndef DOXYGEN_RUNNING /* this totally confuses doxygen for some reason */
EMIT_IDIV(64,128,(uint64_t *puA, uint64_t *puD, uint64_t uDivisor, uint32_t *pfEFlags), (puA, puD, uDivisor, pfEFlags),
          DIV_LOAD, DIV_STORE, MULDIV_NEG_U128, MULDIV_MODDIV_U128)
#  if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_IDIV(32,64,(uint32_t *puA, uint32_t *puD, uint32_t uDivisor, uint32_t *pfEFlags),  (puA, puD, uDivisor, pfEFlags),
          DIV_LOAD, DIV_STORE, MULDIV_NEG, MULDIV_MODDIV)
EMIT_IDIV(16,32,(uint16_t *puA, uint16_t *puD, uint16_t uDivisor, uint32_t *pfEFlags),  (puA, puD, uDivisor, pfEFlags),
          DIV_LOAD, DIV_STORE, MULDIV_NEG, MULDIV_MODDIV)
EMIT_IDIV(8,16,(uint16_t *puAX, uint8_t uDivisor, uint32_t *pfEFlags),                  (puAX,     uDivisor, pfEFlags),
          DIV_LOAD_U8, DIV_STORE_U8, MULDIV_NEG, MULDIV_MODDIV)
#  endif /* !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY) */
# endif /* !DOXYGEN_RUNNING */


/*********************************************************************************************************************************
*   Unary operations.                                                                                                            *
*********************************************************************************************************************************/

/** @def IEM_EFL_UPDATE_STATUS_BITS_FOR_INC_DEC
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
        fEflTmp &= ~X86_EFL_STATUS_BITS | X86_EFL_CF; \
        fEflTmp |= g_afParity[(a_uResult) & 0xff]; \
        fEflTmp |= ((uint32_t)(a_uResult) ^ (uint32_t)(a_uDst)) & X86_EFL_AF; \
        fEflTmp |= X86_EFL_CALC_ZF(a_uResult); \
        fEflTmp |= X86_EFL_CALC_SF(a_uResult, a_cBitsWidth); \
        fEflTmp |= X86_EFL_GET_OF_ ## a_cBitsWidth(a_OfMethod == 0 ? (((a_uDst) ^ RT_BIT_64(a_cBitsWidth - 1)) & (a_uResult)) \
                                                                   : ((a_uDst) & ((a_uResult) ^ RT_BIT_64(a_cBitsWidth - 1))) ); \
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

EMIT_LOCKED_UNARY_OP(inc, 64)
EMIT_LOCKED_UNARY_OP(dec, 64)
EMIT_LOCKED_UNARY_OP(not, 64)
EMIT_LOCKED_UNARY_OP(neg, 64)
# if !defined(RT_ARCH_X86) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_LOCKED_UNARY_OP(inc, 32)
EMIT_LOCKED_UNARY_OP(dec, 32)
EMIT_LOCKED_UNARY_OP(not, 32)
EMIT_LOCKED_UNARY_OP(neg, 32)

EMIT_LOCKED_UNARY_OP(inc, 16)
EMIT_LOCKED_UNARY_OP(dec, 16)
EMIT_LOCKED_UNARY_OP(not, 16)
EMIT_LOCKED_UNARY_OP(neg, 16)

EMIT_LOCKED_UNARY_OP(inc, 8)
EMIT_LOCKED_UNARY_OP(dec, 8)
EMIT_LOCKED_UNARY_OP(not, 8)
EMIT_LOCKED_UNARY_OP(neg, 8)
# endif

#endif /* !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY) */


/*********************************************************************************************************************************
*   Shifting and Rotating                                                                                                        *
*********************************************************************************************************************************/

/*
 * ROL
 */
#define EMIT_ROL(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags, a_fnHlp) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_rol_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (cShift) \
    { \
        if (a_cBitsWidth < 32) \
            cShift &= a_cBitsWidth - 1; \
        a_uType const uDst    = *puDst; \
        a_uType const uResult = a_fnHlp(uDst, cShift); \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags; \
        fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fCarry = (uResult & X86_EFL_CF); \
        fEfl |= fCarry; \
        if (!a_fIntelFlags) /* AMD 3990X: According to the last sub-shift: */ \
            fEfl |= ((uResult >> (a_cBitsWidth - 1)) ^ fCarry) << X86_EFL_OF_BIT; \
        else                /* Intel 10980XE: According to the first sub-shift: */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ (uDst << 1)); \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROL(64, uint64_t, RT_NOTHING, 1, ASMRotateLeftU64)
#endif
EMIT_ROL(64, uint64_t, _intel,     1, ASMRotateLeftU64)
EMIT_ROL(64, uint64_t, _amd,       0, ASMRotateLeftU64)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROL(32, uint32_t, RT_NOTHING, 1, ASMRotateLeftU32)
#endif
EMIT_ROL(32, uint32_t, _intel,     1, ASMRotateLeftU32)
EMIT_ROL(32, uint32_t, _amd,       0, ASMRotateLeftU32)

DECL_FORCE_INLINE(uint16_t) iemAImpl_rol_u16_hlp(uint16_t uValue, uint8_t cShift)
{
    return (uValue << cShift) | (uValue >> (16 - cShift));
}
#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROL(16, uint16_t, RT_NOTHING, 1, iemAImpl_rol_u16_hlp)
#endif
EMIT_ROL(16, uint16_t, _intel,     1, iemAImpl_rol_u16_hlp)
EMIT_ROL(16, uint16_t, _amd,       0, iemAImpl_rol_u16_hlp)

DECL_FORCE_INLINE(uint8_t) iemAImpl_rol_u8_hlp(uint8_t uValue, uint8_t cShift)
{
    return (uValue << cShift) | (uValue >> (8 - cShift));
}
#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROL(8,  uint8_t,  RT_NOTHING, 1, iemAImpl_rol_u8_hlp)
#endif
EMIT_ROL(8,  uint8_t,  _intel,     1, iemAImpl_rol_u8_hlp)
EMIT_ROL(8,  uint8_t,  _amd,       0, iemAImpl_rol_u8_hlp)


/*
 * ROR
 */
#define EMIT_ROR(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags, a_fnHlp) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_ror_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (cShift) \
    { \
        if (a_cBitsWidth < 32) \
            cShift &= a_cBitsWidth - 1; \
        a_uType const uDst    = *puDst; \
        a_uType const uResult = a_fnHlp(uDst, cShift); \
        *puDst = uResult; \
        \
        /* Calc EFLAGS:  */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags; \
        fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fCarry = (uResult >> ((a_cBitsWidth) - 1)) & X86_EFL_CF; \
        fEfl |= fCarry; \
        if (!a_fIntelFlags) /* AMD 3990X: According to the last sub-shift: */ \
            fEfl |= (((uResult >> ((a_cBitsWidth) - 2)) ^ fCarry) & 1) << X86_EFL_OF_BIT; \
        else                /* Intel 10980XE: According to the first sub-shift: */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ (uDst << (a_cBitsWidth - 1))); \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROR(64, uint64_t, RT_NOTHING, 1, ASMRotateRightU64)
#endif
EMIT_ROR(64, uint64_t, _intel,     1, ASMRotateRightU64)
EMIT_ROR(64, uint64_t, _amd,       0, ASMRotateRightU64)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROR(32, uint32_t, RT_NOTHING, 1, ASMRotateRightU32)
#endif
EMIT_ROR(32, uint32_t, _intel,     1, ASMRotateRightU32)
EMIT_ROR(32, uint32_t, _amd,       0, ASMRotateRightU32)

DECL_FORCE_INLINE(uint16_t) iemAImpl_ror_u16_hlp(uint16_t uValue, uint8_t cShift)
{
    return (uValue >> cShift) | (uValue << (16 - cShift));
}
#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROR(16, uint16_t, RT_NOTHING, 1, iemAImpl_ror_u16_hlp)
#endif
EMIT_ROR(16, uint16_t, _intel,     1, iemAImpl_ror_u16_hlp)
EMIT_ROR(16, uint16_t, _amd,       0, iemAImpl_ror_u16_hlp)

DECL_FORCE_INLINE(uint8_t) iemAImpl_ror_u8_hlp(uint8_t uValue, uint8_t cShift)
{
    return (uValue >> cShift) | (uValue << (8 - cShift));
}
#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_ROR(8,  uint8_t,  RT_NOTHING, 1, iemAImpl_ror_u8_hlp)
#endif
EMIT_ROR(8,  uint8_t,  _intel,     1, iemAImpl_ror_u8_hlp)
EMIT_ROR(8,  uint8_t,  _amd,       0, iemAImpl_ror_u8_hlp)


/*
 * RCL
 */
#define EMIT_RCL(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_rcl_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (a_cBitsWidth < 32 && a_fIntelFlags) \
        cShift %= a_cBitsWidth + 1; \
    if (cShift) \
    { \
        if (a_cBitsWidth < 32 && !a_fIntelFlags) \
            cShift %= a_cBitsWidth + 1; \
        a_uType const uDst    = *puDst; \
        a_uType       uResult = uDst << cShift; \
        if (cShift > 1) \
            uResult |= uDst >> (a_cBitsWidth + 1 - cShift); \
        \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl     = *pfEFlags; \
        uint32_t fInCarry = fEfl & X86_EFL_CF; \
        uResult |= (a_uType)fInCarry << (cShift - 1); \
        \
        *puDst = uResult; \
        \
        /* Calc EFLAGS. */ \
        fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fOutCarry = a_cBitsWidth >= 32 || a_fIntelFlags || cShift \
                                 ? (uDst >> (a_cBitsWidth - cShift)) & X86_EFL_CF : fInCarry; \
        fEfl |= fOutCarry; \
        if (!a_fIntelFlags) /* AMD 3990X: According to the last sub-shift: */ \
            fEfl |= ((uResult >> (a_cBitsWidth - 1)) ^ fOutCarry) << X86_EFL_OF_BIT; \
        else                /* Intel 10980XE: According to the first sub-shift: */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ (uDst << 1)); \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCL(64, uint64_t, RT_NOTHING, 1)
#endif
EMIT_RCL(64, uint64_t, _intel,     1)
EMIT_RCL(64, uint64_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCL(32, uint32_t, RT_NOTHING, 1)
#endif
EMIT_RCL(32, uint32_t, _intel,     1)
EMIT_RCL(32, uint32_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCL(16, uint16_t, RT_NOTHING, 1)
#endif
EMIT_RCL(16, uint16_t, _intel,     1)
EMIT_RCL(16, uint16_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCL(8,  uint8_t,  RT_NOTHING, 1)
#endif
EMIT_RCL(8,  uint8_t,  _intel,     1)
EMIT_RCL(8,  uint8_t,  _amd,       0)


/*
 * RCR
 */
#define EMIT_RCR(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_rcr_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (a_cBitsWidth < 32 && a_fIntelFlags) \
        cShift %= a_cBitsWidth + 1; \
    if (cShift) \
    { \
        if (a_cBitsWidth < 32 && !a_fIntelFlags) \
            cShift %= a_cBitsWidth + 1; \
        a_uType const uDst    = *puDst; \
        a_uType       uResult = uDst >> cShift; \
        if (cShift > 1) \
            uResult |= uDst << (a_cBitsWidth + 1 - cShift); \
        \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl     = *pfEFlags; \
        uint32_t fInCarry = fEfl & X86_EFL_CF; \
        uResult |= (a_uType)fInCarry << (a_cBitsWidth - cShift); \
        *puDst = uResult; \
        \
        /* Calc EFLAGS.  The OF bit is undefined if cShift > 1, we implement \
           it the same way as for 1 bit shifts. */ \
        fEfl &= ~(X86_EFL_CF | X86_EFL_OF); \
        uint32_t const fOutCarry = a_cBitsWidth >= 32 || a_fIntelFlags || cShift \
                                 ? (uDst >> (cShift - 1)) & X86_EFL_CF : fInCarry; \
        fEfl |= fOutCarry; \
        if (!a_fIntelFlags) /* AMD 3990X: XOR two most signficant bits of the result: */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uResult ^ (uResult << 1));  \
        else                /* Intel 10980XE: same as AMD, but only for the first sub-shift: */ \
            fEfl |= (fInCarry ^ (uint32_t)(uDst >> (a_cBitsWidth - 1))) << X86_EFL_OF_BIT; \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCR(64, uint64_t, RT_NOTHING, 1)
#endif
EMIT_RCR(64, uint64_t, _intel,     1)
EMIT_RCR(64, uint64_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCR(32, uint32_t, RT_NOTHING, 1)
#endif
EMIT_RCR(32, uint32_t, _intel,     1)
EMIT_RCR(32, uint32_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCR(16, uint16_t, RT_NOTHING, 1)
#endif
EMIT_RCR(16, uint16_t, _intel,     1)
EMIT_RCR(16, uint16_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_RCR(8,  uint8_t,  RT_NOTHING, 1)
#endif
EMIT_RCR(8,  uint8_t,  _intel,     1)
EMIT_RCR(8,  uint8_t,  _amd,       0)


/*
 * SHL
 */
#define EMIT_SHL(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_shl_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (cShift) \
    { \
        a_uType const uDst  = *puDst; \
        a_uType       uResult = uDst << cShift; \
        *puDst = uResult; \
        \
        /* Calc EFLAGS. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        uint32_t fCarry = (uDst >> (a_cBitsWidth - cShift)) & X86_EFL_CF; \
        fEfl |= fCarry; \
        if (!a_fIntelFlags) \
            fEfl |= ((uResult >> (a_cBitsWidth - 1)) ^ fCarry) << X86_EFL_OF_BIT; /* AMD 3990X: Last shift result. */ \
        else \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ (uDst << 1)); /* Intel 10980XE: First shift result. */ \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        if (!a_fIntelFlags) \
            fEfl |= X86_EFL_AF; /* AMD 3990x sets it unconditionally, Intel 10980XE does the oposite */ \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHL(64, uint64_t, RT_NOTHING, 1)
#endif
EMIT_SHL(64, uint64_t, _intel,     1)
EMIT_SHL(64, uint64_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHL(32, uint32_t, RT_NOTHING, 1)
#endif
EMIT_SHL(32, uint32_t, _intel,     1)
EMIT_SHL(32, uint32_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHL(16, uint16_t, RT_NOTHING, 1)
#endif
EMIT_SHL(16, uint16_t, _intel,     1)
EMIT_SHL(16, uint16_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHL(8,  uint8_t,  RT_NOTHING, 1)
#endif
EMIT_SHL(8,  uint8_t,  _intel,     1)
EMIT_SHL(8,  uint8_t,  _amd,       0)


/*
 * SHR
 */
#define EMIT_SHR(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_shr_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (cShift) \
    { \
        a_uType const uDst    = *puDst; \
        a_uType       uResult = uDst >> cShift; \
        *puDst = uResult; \
        \
        /* Calc EFLAGS. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        fEfl |= (uDst >> (cShift - 1)) & X86_EFL_CF; \
        if (a_fIntelFlags || cShift == 1) /* AMD 3990x does what intel documents; Intel 10980XE does this for all shift counts. */ \
            fEfl |= (uDst >> (a_cBitsWidth - 1)) << X86_EFL_OF_BIT; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        if (!a_fIntelFlags) \
            fEfl |= X86_EFL_AF; /* AMD 3990x sets it unconditionally, Intel 10980XE does the oposite */ \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHR(64, uint64_t, RT_NOTHING, 1)
#endif
EMIT_SHR(64, uint64_t, _intel,     1)
EMIT_SHR(64, uint64_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHR(32, uint32_t, RT_NOTHING, 1)
#endif
EMIT_SHR(32, uint32_t, _intel,     1)
EMIT_SHR(32, uint32_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHR(16, uint16_t, RT_NOTHING, 1)
#endif
EMIT_SHR(16, uint16_t, _intel,     1)
EMIT_SHR(16, uint16_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHR(8,  uint8_t,  RT_NOTHING, 1)
#endif
EMIT_SHR(8,  uint8_t,  _intel,     1)
EMIT_SHR(8,  uint8_t,  _amd,       0)


/*
 * SAR
 */
#define EMIT_SAR(a_cBitsWidth, a_uType, a_iType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_sar_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth >= 32 ? a_cBitsWidth - 1 : 31; \
    if (cShift) \
    { \
        a_iType const iDst    = (a_iType)*puDst; \
        a_uType       uResult = iDst >> cShift; \
        *puDst = uResult; \
        \
        /* Calc EFLAGS. \
           Note! The OF flag is always zero because the result never differs from the input. */ \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        fEfl |= (iDst >> (cShift - 1)) & X86_EFL_CF; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        if (!a_fIntelFlags) \
            fEfl |= X86_EFL_AF; /* AMD 3990x sets it unconditionally, Intel 10980XE does the oposite */ \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SAR(64, uint64_t, int64_t, RT_NOTHING, 1)
#endif
EMIT_SAR(64, uint64_t, int64_t, _intel,     1)
EMIT_SAR(64, uint64_t, int64_t, _amd,       0)

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SAR(32, uint32_t, int32_t, RT_NOTHING, 1)
#endif
EMIT_SAR(32, uint32_t, int32_t, _intel,     1)
EMIT_SAR(32, uint32_t, int32_t, _amd,       0)

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SAR(16, uint16_t, int16_t, RT_NOTHING, 1)
#endif
EMIT_SAR(16, uint16_t, int16_t, _intel,     1)
EMIT_SAR(16, uint16_t, int16_t, _amd,       0)

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SAR(8,  uint8_t,  int8_t,  RT_NOTHING, 1)
#endif
EMIT_SAR(8,  uint8_t,  int8_t,  _intel,     1)
EMIT_SAR(8,  uint8_t,  int8_t,  _amd,       0)


/*
 * SHLD
 *
 *  - CF is the last bit shifted out of puDst.
 *  - AF is always cleared by Intel 10980XE.
 *  - AF is always set by AMD 3990X.
 *  - OF is set according to the first shift on Intel 10980XE, it seems.
 *  - OF is set according to the last sub-shift on AMD 3990X.
 *  - ZF, SF and PF are calculated according to the result by both vendors.
 *
 * For 16-bit shifts the count mask isn't 15, but 31, and the CPU will
 * pick either the source register or the destination register for input bits
 * when going beyond 16.  According to https://www.sandpile.org/x86/flags.htm
 * intel has changed behaviour here several times.  We implement what current
 * skylake based does for now, we can extend this later as needed.
 */
#define EMIT_SHLD(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_shld_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, a_uType uSrc, uint8_t cShift, \
                                                                           uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        a_uType const uDst    = *puDst; \
        a_uType       uResult = uDst << cShift; \
        uResult |= uSrc >> (a_cBitsWidth - cShift); \
        *puDst = uResult; \
        \
        /* CALC EFLAGS: */ \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        if (a_fIntelFlags) \
            /* Intel 6700K & 10980XE: Set according to the first shift. AF always cleared. */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ (uDst << 1));  \
        else \
        {   /* AMD 3990X: Set according to last shift. AF always set. */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth((uDst << (cShift - 1)) ^ uResult); \
            fEfl |= X86_EFL_AF; \
        } \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        fEfl |= (uDst >> (a_cBitsWidth - cShift)) & X86_EFL_CF; /* CF = last bit shifted out */ \
        fEfl |= g_afParity[uResult & 0xff]; \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHLD(64, uint64_t, RT_NOTHING, 1)
#endif
EMIT_SHLD(64, uint64_t, _intel,     1)
EMIT_SHLD(64, uint64_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHLD(32, uint32_t, RT_NOTHING, 1)
#endif
EMIT_SHLD(32, uint32_t, _intel,     1)
EMIT_SHLD(32, uint32_t, _amd,       0)

#define EMIT_SHLD_16(a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT(iemAImpl_shld_u16,a_Suffix),(uint16_t *puDst, uint16_t uSrc, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= 31; \
    if (cShift) \
    { \
        uint16_t const uDst    = *puDst; \
        uint64_t const uTmp    = a_fIntelFlags \
                               ? ((uint64_t)uDst << 32) | ((uint32_t)uSrc << 16) | uDst \
                               : ((uint64_t)uDst << 32) | ((uint32_t)uSrc << 16) | uSrc; \
        uint16_t const uResult = (uint16_t)((uTmp << cShift) >> 32); \
        *puDst = uResult; \
        \
        /* CALC EFLAGS: */ \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        if (a_fIntelFlags) \
        { \
            fEfl |= (uTmp >> (48 - cShift)) & X86_EFL_CF; /* CF = last bit shifted out of the combined operand */ \
            /* Intel 6700K & 10980XE: OF is et according to the first shift. AF always cleared. */ \
            fEfl |= X86_EFL_GET_OF_16(uDst ^ (uDst << 1));  \
        } \
        else \
        { \
            /* AMD 3990X: OF is set according to last shift, with some weirdness. AF always set. CF = last bit shifted out of uDst. */ \
            if (cShift < 16) \
            { \
                fEfl |= (uDst >> (16 - cShift)) & X86_EFL_CF;  \
                fEfl |= X86_EFL_GET_OF_16((uDst << (cShift - 1)) ^ uResult); \
            } \
            else \
            { \
                if (cShift == 16) \
                    fEfl |= uDst & X86_EFL_CF; \
                fEfl |= X86_EFL_GET_OF_16((uDst << (cShift - 1)) ^ 0); \
            } \
            fEfl |= X86_EFL_AF; \
        } \
        fEfl |= g_afParity[uResult & 0xff]; \
        fEfl |= X86_EFL_CALC_SF(uResult, 16); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        *pfEFlags = fEfl; \
    } \
}

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHLD_16(RT_NOTHING, 1)
#endif
EMIT_SHLD_16(_intel,     1)
EMIT_SHLD_16(_amd,       0)


/*
 * SHRD
 *
 * EFLAGS behaviour seems to be the same as with SHLD:
 *  - CF is the last bit shifted out of puDst.
 *  - AF is always cleared by Intel 10980XE.
 *  - AF is always set by AMD 3990X.
 *  - OF is set according to the first shift on Intel 10980XE, it seems.
 *  - OF is set according to the last sub-shift on AMD 3990X.
 *  - ZF, SF and PF are calculated according to the result by both vendors.
 *
 * For 16-bit shifts the count mask isn't 15, but 31, and the CPU will
 * pick either the source register or the destination register for input bits
 * when going beyond 16.  According to https://www.sandpile.org/x86/flags.htm
 * intel has changed behaviour here several times.  We implement what current
 * skylake based does for now, we can extend this later as needed.
 */
#define EMIT_SHRD(a_cBitsWidth, a_uType, a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT3(iemAImpl_shrd_u,a_cBitsWidth,a_Suffix),(a_uType *puDst, a_uType uSrc, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= a_cBitsWidth - 1; \
    if (cShift) \
    { \
        a_uType const uDst    = *puDst; \
        a_uType       uResult = uDst >> cShift; \
        uResult |= uSrc << (a_cBitsWidth - cShift); \
        *puDst = uResult; \
        \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        fEfl |= (uDst >> (cShift - 1)) & X86_EFL_CF; \
        if (a_fIntelFlags) \
            /* Intel 6700K & 10980XE: Set according to the first shift. AF always cleared. */ \
            fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ (uSrc << (a_cBitsWidth - 1))); \
        else \
        {   /* AMD 3990X: Set according to last shift. AF always set. */ \
            if (cShift > 1) /* Set according to last shift. */ \
                fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth((uSrc << (a_cBitsWidth - cShift + 1)) ^ uResult); \
            else \
                fEfl |= X86_EFL_GET_OF_ ## a_cBitsWidth(uDst ^ uResult); \
            fEfl |= X86_EFL_AF; \
        } \
        fEfl |= X86_EFL_CALC_SF(uResult, a_cBitsWidth); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}

#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHRD(64, uint64_t, RT_NOTHING, 1)
#endif
EMIT_SHRD(64, uint64_t, _intel,     1)
EMIT_SHRD(64, uint64_t, _amd,       0)

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHRD(32, uint32_t, RT_NOTHING, 1)
#endif
EMIT_SHRD(32, uint32_t, _intel,     1)
EMIT_SHRD(32, uint32_t, _amd,       0)

#define EMIT_SHRD_16(a_Suffix, a_fIntelFlags) \
IEM_DECL_IMPL_DEF(void, RT_CONCAT(iemAImpl_shrd_u16,a_Suffix),(uint16_t *puDst, uint16_t uSrc, uint8_t cShift, uint32_t *pfEFlags)) \
{ \
    cShift &= 31; \
    if (cShift) \
    { \
        uint16_t const uDst    = *puDst; \
        uint64_t const uTmp    = a_fIntelFlags \
                               ? uDst | ((uint32_t)uSrc << 16) | ((uint64_t)uDst << 32) \
                               : uDst | ((uint32_t)uSrc << 16) | ((uint64_t)uSrc << 32); \
        uint16_t const uResult = (uint16_t)(uTmp >> cShift); \
        *puDst = uResult; \
        \
        uint32_t fEfl = *pfEFlags & ~X86_EFL_STATUS_BITS; \
        AssertCompile(X86_EFL_CF_BIT == 0); \
        if (a_fIntelFlags) \
        { \
            /* Intel 10980XE: The CF is the last shifted out of the combined uTmp operand. */ \
            fEfl |= (uTmp >> (cShift - 1)) & X86_EFL_CF; \
            /* Intel 6700K & 10980XE: Set according to the first shift. AF always cleared. */ \
            fEfl |= X86_EFL_GET_OF_16(uDst ^ (uSrc << 15)); \
        } \
        else \
        { \
            /* AMD 3990X: CF flag seems to be last bit shifted out of uDst, not the combined uSrc:uSrc:uDst operand. */ \
            fEfl |= (uDst >> (cShift - 1)) & X86_EFL_CF; \
            /* AMD 3990X: Set according to last shift. AF always set. */ \
            if (cShift > 1) /* Set according to last shift. */ \
                fEfl |= X86_EFL_GET_OF_16((uint16_t)(uTmp >> (cShift - 1)) ^ uResult); \
            else \
                fEfl |= X86_EFL_GET_OF_16(uDst ^ uResult); \
            fEfl |= X86_EFL_AF; \
        } \
        fEfl |= X86_EFL_CALC_SF(uResult, 16); \
        fEfl |= X86_EFL_CALC_ZF(uResult); \
        fEfl |= g_afParity[uResult & 0xff]; \
        *pfEFlags = fEfl; \
    } \
}

#if (!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64)) || defined(IEM_WITHOUT_ASSEMBLY)
EMIT_SHRD_16(RT_NOTHING, 1)
#endif
EMIT_SHRD_16(_intel,     1)
EMIT_SHRD_16(_amd,       0)


#if !defined(RT_ARCH_AMD64) || defined(IEM_WITHOUT_ASSEMBLY)

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
#if 0
    *(uint16_t *)puDst = ASMByteSwapU16(*(uint16_t *)puDst);
#else
    /* This is the behaviour AMD 3990x (64-bit mode): */
    *(uint16_t *)puDst = 0;
#endif
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


/*********************************************************************************************************************************
*   x87 FPU                                                                                                                      *
*********************************************************************************************************************************/
#if defined(IEM_WITHOUT_ASSEMBLY)

IEM_DECL_IMPL_DEF(void, iemAImpl_f2xm1_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fabs_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fadd_r80_by_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fadd_r80_by_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fadd_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fchs_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fcom_r80_by_r32,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFSW, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fcom_r80_by_r64,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFSW, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fcom_r80_by_r80,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFSW, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_fcomi_r80_by_r80,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                       PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFSW, pr80Val1, pr80Val2);
    AssertReleaseFailed();
    return 0;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fcos_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fdiv_r80_by_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fdiv_r80_by_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fdiv_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fdivr_r80_by_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fdivr_r80_by_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fdivr_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fiadd_r80_by_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fiadd_r80_by_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_ficom_r80_by_i16,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pu16Fsw, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_ficom_r80_by_i32,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pu16Fsw, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fidiv_r80_by_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fidiv_r80_by_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fidivr_r80_by_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fidivr_r80_by_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fild_i16_to_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int16_t const *pi16Val))
{
    RT_NOREF(pFpuState, pFpuRes, pi16Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fild_i32_to_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int32_t const *pi32Val))
{
    RT_NOREF(pFpuState, pFpuRes, pi32Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fild_i64_to_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, int64_t const *pi64Val))
{
    RT_NOREF(pFpuState, pFpuRes, pi64Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fimul_r80_by_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fimul_r80_by_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fist_r80_to_i16,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                  int16_t *pi16Val, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16FSW, pi16Val, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fist_r80_to_i32,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                  int32_t *pi32Val, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16FSW, pi32Val, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fist_r80_to_i64,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                  int64_t *pi64Val, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16FSW, pi64Val, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fistt_r80_to_i16,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                   int16_t *pi16Val, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16FSW, pi16Val, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fistt_r80_to_i32,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                   int32_t *pi32Val, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16FSW, pi32Val, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fistt_r80_to_i64,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                   int64_t *pi64Val, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16FSW, pi64Val, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fisub_r80_by_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fisub_r80_by_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fisubr_r80_by_i16,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, int16_t const *pi16Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi16Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fisubr_r80_by_i32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, int32_t const *pi32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pi32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT32U pr32Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr32Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT64U pr64Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr64Val);
    AssertReleaseFailed();
}

IEM_DECL_IMPL_DEF(void, iemAImpl_fld_r80_from_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fld1,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = 0 + 16383;
    pFpuRes->r80Result.sj64.fInteger    = 1;
    pFpuRes->r80Result.sj64.uFraction   = 0;

    /*
     * FPU status word:
     *      - TOP is irrelevant, but we must match x86 assembly version.
     *      - C1 is always cleared as we don't have any stack overflows.
     *      - C0, C2, and C3 are undefined and Intel 10980XE does not touch them.
     */
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3));
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fldl2e,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = 0 + 16383;
    pFpuRes->r80Result.sj64.fInteger    = 1;
    pFpuRes->r80Result.sj64.uFraction   =    (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_NEAREST
                                          || (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_UP
                                        ? UINT64_C(0x38aa3b295c17f0bc) : UINT64_C(0x38aa3b295c17f0bb);
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3)); /* see iemAImpl_fld1 */
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fldl2t,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = 1 + 16383;
    pFpuRes->r80Result.sj64.fInteger    = 1;
    pFpuRes->r80Result.sj64.uFraction   = (pFpuState->FCW & X86_FCW_RC_MASK) != X86_FCW_RC_UP
                                        ? UINT64_C(0x549a784bcd1b8afe) : UINT64_C(0x549a784bcd1b8aff);
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3)); /* see iemAImpl_fld1 */
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fldlg2,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = -2 + 16383;
    pFpuRes->r80Result.sj64.fInteger    = 1;
    pFpuRes->r80Result.sj64.uFraction   =    (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_NEAREST
                                          || (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_UP
                                        ? UINT64_C(0x1a209a84fbcff799) : UINT64_C(0x1a209a84fbcff798);
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3)); /* see iemAImpl_fld1 */
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fldln2,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = -1 + 16383;
    pFpuRes->r80Result.sj64.fInteger    = 1;
    pFpuRes->r80Result.sj64.uFraction   =    (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_NEAREST
                                          || (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_UP
                                        ? UINT64_C(0x317217f7d1cf79ac) : UINT64_C(0x317217f7d1cf79ab);
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3)); /* see iemAImpl_fld1 */
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fldpi,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = 1 + 16383;
    pFpuRes->r80Result.sj64.fInteger    = 1;
    pFpuRes->r80Result.sj64.uFraction   =    (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_NEAREST
                                          || (pFpuState->FCW & X86_FCW_RC_MASK) == X86_FCW_RC_UP
                                        ? UINT64_C(0x490fdaa22168c235) : UINT64_C(0x490fdaa22168c234);
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3)); /* see iemAImpl_fld1 */
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fldz,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes))
{
    pFpuRes->r80Result.sj64.fSign       = 0;
    pFpuRes->r80Result.sj64.uExponent   = 0;
    pFpuRes->r80Result.sj64.fInteger    = 0;
    pFpuRes->r80Result.sj64.uFraction   = 0;
    pFpuRes->FSW = (7 << X86_FSW_TOP_SHIFT) | (pFpuState->FSW & (X86_FSW_C0 | X86_FSW_C2 | X86_FSW_C3)); /* see iemAImpl_fld1 */
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fmul_r80_by_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fmul_r80_by_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fmul_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fpatan_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fprem_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fprem1_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fptan_r80_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULTTWO pFpuResTwo, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuResTwo, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_frndint_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fscale_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                    PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsin_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsincos_r80_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULTTWO pFpuResTwo, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuResTwo, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsqrt_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r32,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT32U pr32Dst, PCRTFLOAT80U pr80Src))
{
    RT_NOREF(pFpuState, pu16FSW, pr32Dst, pr80Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r64,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT64U pr64Dst, PCRTFLOAT80U pr80Src))
{
    RT_NOREF(pFpuState, pu16FSW, pr64Dst, pr80Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_r80,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTFLOAT80U pr80Dst, PCRTFLOAT80U pr80Src))
{
    RT_NOREF(pFpuState, pu16FSW, pr80Dst, pr80Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fst_r80_to_d80,(PCX86FXSTATE pFpuState, uint16_t *pu16FSW,
                                                 PRTPBCD80U pd80Dst, PCRTFLOAT80U pr80Src))
{
    RT_NOREF(pFpuState, pu16FSW, pd80Dst, pr80Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsub_r80_by_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsub_r80_by_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsub_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                  PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsubr_r80_by_r32,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT32U pr32Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr32Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsubr_r80_by_r64,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT64U pr64Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr64Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fsubr_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_ftst_r80,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16Fsw, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fucom_r80_by_r80,(PCX86FXSTATE pFpuState, uint16_t *pFSW,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFSW, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(uint32_t, iemAImpl_fucomi_r80_by_r80,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw,
                                                        PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pu16Fsw, pr80Val1, pr80Val2);
    AssertReleaseFailed();
    return 0;
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fxam_r80,(PCX86FXSTATE pFpuState, uint16_t *pu16Fsw, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pu16Fsw, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fxtract_r80_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULTTWO pFpuResTwo, PCRTFLOAT80U pr80Val))
{
    RT_NOREF(pFpuState, pFpuResTwo, pr80Val);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fyl2x_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                   PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_fyl2xp1_r80_by_r80,(PCX86FXSTATE pFpuState, PIEMFPURESULT pFpuRes,
                                                     PCRTFLOAT80U pr80Val1, PCRTFLOAT80U pr80Val2))
{
    RT_NOREF(pFpuState, pFpuRes, pr80Val1, pr80Val2);
    AssertReleaseFailed();
}

#endif /* IEM_WITHOUT_ASSEMBLY */


/*********************************************************************************************************************************
*   MMX, SSE & AVX                                                                                                               *
*********************************************************************************************************************************/

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

#ifdef  IEM_WITHOUT_ASSEMBLY

IEM_DECL_IMPL_DEF(void, iemAImpl_pcmpeqb_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pcmpeqb_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pcmpeqw_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pcmpeqw_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pcmpeqd_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pcmpeqd_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pxor_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pxor_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pmovmskb_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();

}


IEM_DECL_IMPL_DEF(void, iemAImpl_pmovmskb_u128,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pshufw,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src, uint8_t bEvil))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src, bEvil);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pshufhw,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src, uint8_t bEvil))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src, bEvil);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pshuflw,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src, uint8_t bEvil))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src, bEvil);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_pshufd,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src, uint8_t bEvil))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src, bEvil);
    AssertReleaseFailed();
}

/* PUNPCKHxxx */

IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhbw_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhbw_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhwd_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhwd_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhdq_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhdq_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckhqdq_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, PCRTUINT128U pu128Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu128Src);
    AssertReleaseFailed();
}

/* PUNPCKLxxx */

IEM_DECL_IMPL_DEF(void, iemAImpl_punpcklbw_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint32_t const *pu32Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu32Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpcklbw_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpcklwd_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint32_t const *pu32Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu32Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpcklwd_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckldq_u64,(PCX86FXSTATE pFpuState, uint64_t *pu64Dst, uint32_t const *pu32Src))
{
    RT_NOREF(pFpuState, pu64Dst, pu32Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpckldq_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu64Src);
    AssertReleaseFailed();
}


IEM_DECL_IMPL_DEF(void, iemAImpl_punpcklqdq_u128,(PCX86FXSTATE pFpuState, PRTUINT128U pu128Dst, uint64_t const *pu64Src))
{
    RT_NOREF(pFpuState, pu128Dst, pu64Src);
    AssertReleaseFailed();
}

#endif /* IEM_WITHOUT_ASSEMBLY */
