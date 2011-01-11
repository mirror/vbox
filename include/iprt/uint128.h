/** @file
 * IPRT - RTUINT128U & uint128_t methods.
 */

/*
 * Copyright (C) 2011 Oracle Corporation
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

#ifndef ___iprt_uint128_h
#define ___iprt_uint128_h

#include <iprt/cdefs.h>
#include <iprt/types.h>
#include <iprt/err.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_rt_once       RTUInt128 - 128-bit Unsigned Integer Methods
 * @ingroup grp_rt
 * @{
 */


/**
 * Test if a 128-bit unsigned integer value is zero.
 *
 * @returns true if they are, false if they aren't.
 * @param   pValue          The input and output value.
 */
DECLINLINE(bool) RTUInt128IsZero(PRTUINT128U pValue)
{
#if ARCH_BITS >= 64
    return pValue->s.Hi == 0
        && pValue->s.Lo == 0;
#else
    return pValue->DWords.dw0 == 0
        && pValue->DWords.dw1 == 0
        && pValue->DWords.dw2 == 0
        && pValue->DWords.dw3 == 0;
#endif
}


/**
 * Set a 128-bit unsigned integer value to zero.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 */
DECLINLINE(PRTUINT128U) RTUInt128SetZero(PRTUINT128U pResult)
{
#if ARCH_BITS >= 64
    pResult->s.Hi = 0;
    pResult->s.Lo = 0;
#else
    pResult->DWords.dw0 = 0;
    pResult->DWords.dw1 = 0;
    pResult->DWords.dw2 = 0;
    pResult->DWords.dw3 = 0;
#endif
    return pResult;
}


/**
 * Set a 128-bit unsigned integer value to the maximum value.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 */
DECLINLINE(PRTUINT128U) RTUInt128SetMax(PRTUINT128U pResult)
{
#if ARCH_BITS >= 64
    pResult->s.Hi = UINT64_MAX;
    pResult->s.Lo = UINT64_MAX;
#else
    pResult->DWords.dw0 = UINT32_MAX;
    pResult->DWords.dw1 = UINT32_MAX;
    pResult->DWords.dw2 = UINT32_MAX;
    pResult->DWords.dw3 = UINT32_MAX;
#endif
    return pResult;
}


RTDECL(PRTUINT128U) RTUInt128Add(PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128Sub(PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128Div(PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128Mod(PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128And(PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128Or( PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128Xor(PRTUINT128U pResult, PCRTUINT128U pValue1, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128ShiftLeft( PRTUINT128U pResult, PCRTUINT128U pValue, int cBits);
RTDECL(PRTUINT128U) RTUInt128ShiftRight(PRTUINT128U pResult, PCRTUINT128U pValue, int cBits);
RTDECL(PRTUINT128U) RTUInt128BooleanNot(PRTUINT128U pResult, PCRTUINT128U pValue);
RTDECL(PRTUINT128U) RTUInt128BitwiseNot(PRTUINT128U pResult, PCRTUINT128U pValue);


/**
 * Assigns one 128-bit unsigned integer value to another.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 * @param   pValue              The value to assign.
 */
DECLINLINE(PRTUINT128U) RTUInt128Assign(PRTUINT128U pResult, PCRTUINT128U pValue)
{
#if ARCH_BITS >= 64
    pResult->s.Hi = pValue->s.Hi;
    pResult->s.Lo = pValue->s.Lo;
#else
    pResult->DWords.dw0 = pValue->DWords.dw0;
    pResult->DWords.dw1 = pValue->DWords.dw1;
    pResult->DWords.dw2 = pValue->DWords.dw2;
    pResult->DWords.dw3 = pValue->DWords.dw3;
#endif
    return pResult;
}


/**
 * Assigns a boolean value to 128-bit unsigned integer.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 * @param   fValue              The boolean value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignBoolean(PRTUINT128U pValueResult, bool fValue)
{
#if ARCH_BITS >= 64
    pValueResult->s.Lo = fValue;
    pValueResult->s.Hi = 0;
#else
    pValueResult->DWords.dw0 = fValue;
    pValueResult->DWords.dw1 = 0;
    pValueResult->DWords.dw2 = 0;
    pValueResult->DWords.dw3 = 0;
#endif
    return pValueResult;
}


/**
 * Assigns a 8-bit unsigned integer value to 128-bit unsigned integer.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 * @param   u8Value             The 8-bit unsigned integer value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignU8(PRTUINT128U pValueResult, uint8_t u8Value)
{
#if ARCH_BITS >= 64
    pValueResult->s.Lo = u8Value;
    pValueResult->s.Hi = 0;
#else
    pValueResult->DWords.dw0 = u8Value;
    pValueResult->DWords.dw1 = 0;
    pValueResult->DWords.dw2 = 0;
    pValueResult->DWords.dw3 = 0;
#endif
    return pValueResult;
}


/**
 * Assigns a 16-bit unsigned integer value to 128-bit unsigned integer.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 * @param   u16Value            The 16-bit unsigned integer value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignU16(PRTUINT128U pValueResult, uint16_t u16Value)
{
#if ARCH_BITS >= 64
    pValueResult->s.Lo = u16Value;
    pValueResult->s.Hi = 0;
#else
    pValueResult->DWords.dw0 = u16Value;
    pValueResult->DWords.dw1 = 0;
    pValueResult->DWords.dw2 = 0;
    pValueResult->DWords.dw3 = 0;
#endif
    return pValueResult;
}


/**
 * Assigns a 16-bit unsigned integer value to 128-bit unsigned integer.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 * @param   u32Value            The 32-bit unsigned integer value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignU32(PRTUINT128U pValueResult, uint32_t u32Value)
{
#if ARCH_BITS >= 64
    pValueResult->s.Lo = u32Value;
    pValueResult->s.Hi = 0;
#else
    pValueResult->DWords.dw0 = u32Value;
    pValueResult->DWords.dw1 = 0;
    pValueResult->DWords.dw2 = 0;
    pValueResult->DWords.dw3 = 0;
#endif
    return pValueResult;
}


/**
 * Assigns a 64-bit unsigned integer value to 128-bit unsigned integer.
 *
 * @returns pResult
 * @param   pResult             The result variable.
 * @param   u32Value            The 32-bit unsigned integer value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignU64(PRTUINT128U pValueResult, uint64_t u64Value)
{
    pValueResult->s.Lo = u64Value;
    pValueResult->s.Hi = 0;
    return pValueResult;
}


RTDECL(PRTUINT128U) RTUInt128AssignAdd(PRTUINT128U pValue1Result, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128AssignSub(PRTUINT128U pValue1Result, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128AssignDiv(PRTUINT128U pValue1Result, PCRTUINT128U pValue2);
RTDECL(PRTUINT128U) RTUInt128AssignMod(PRTUINT128U pValue1Result, PCRTUINT128U pValue2);


/**
 * Performs a bitwise AND of two 128-bit unsigned integer values and assigned
 * the result to the first one.
 *
 * @returns pValue1Result.
 * @param   pValue1Result   The first value and result.
 * @param   pValue2         The second value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignAnd(PRTUINT128U pValue1Result, PCRTUINT128U pValue2)
{
#if ARCH_BITS >= 64
    pValue1Result->s.Hi &= pValue2->s.Hi;
    pValue1Result->s.Lo &= pValue2->s.Lo;
#else
    pValue1Result->DWords.dw0 &= pValue2->DWords.dw0;
    pValue1Result->DWords.dw1 &= pValue2->DWords.dw1;
    pValue1Result->DWords.dw2 &= pValue2->DWords.dw2;
    pValue1Result->DWords.dw3 &= pValue2->DWords.dw3;
#endif
    return pValue1Result;
}


/**
 * Performs a bitwise AND of a 128-bit unsigned integer value and a mask made
 * up of the first N bits, assigning the result to the the 128-bit value.
 *
 * @returns pValueResult.
 * @param   pValueResult    The value and result.
 * @param   cBits           The number of bits to AND (counting from the first
 *                          bit).
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignAndNFirstBits(PRTUINT128U pValueResult, unsigned cBits)
{
    if (cBits <= 64)
    {
        if (cBits != 64)
            pValueResult->s.Lo &= (RT_BIT_64(cBits) - 1);
        pValueResult->s.Hi = 0;
    }
    else if (cBits < 128)
        pValueResult->s.Hi &= (RT_BIT_64(cBits - 64) - 1);
/** @todo #if ARCH_BITS >= 64 */
    return pValueResult;
}


/**
 * Performs a bitwise OR of two 128-bit unsigned integer values and assigned
 * the result to the first one.
 *
 * @returns pValue1Result.
 * @param   pValue1Result   The first value and result.
 * @param   pValue2         The second value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignOr(PRTUINT128U pValue1Result, PCRTUINT128U pValue2)
{
#if ARCH_BITS >= 64
    pValue1Result->s.Hi |= pValue2->s.Hi;
    pValue1Result->s.Lo |= pValue2->s.Lo;
#else
    pValue1Result->DWords.dw0 |= pValue2->DWords.dw0;
    pValue1Result->DWords.dw1 |= pValue2->DWords.dw1;
    pValue1Result->DWords.dw2 |= pValue2->DWords.dw2;
    pValue1Result->DWords.dw3 |= pValue2->DWords.dw3;
#endif
    return pValue1Result;
}


/**
 * Performs a bitwise XOR of two 128-bit unsigned integer values and assigned
 * the result to the first one.
 *
 * @returns pValue1Result.
 * @param   pValue1Result   The first value and result.
 * @param   pValue2         The second value.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignXor(PRTUINT128U pValue1Result, PCRTUINT128U pValue2)
{
#if ARCH_BITS >= 64
    pValue1Result->s.Hi ^= pValue2->s.Hi;
    pValue1Result->s.Lo ^= pValue2->s.Lo;
#else
    pValue1Result->DWords.dw0 ^= pValue2->DWords.dw0;
    pValue1Result->DWords.dw1 ^= pValue2->DWords.dw1;
    pValue1Result->DWords.dw2 ^= pValue2->DWords.dw2;
    pValue1Result->DWords.dw3 ^= pValue2->DWords.dw3;
#endif
    return pValue1Result;
}


/**
 * Performs a bitwise left shift on a 128-bit unsigned integer value, assigning
 * the result to it.
 *
 * @returns pValue1Result.
 * @param   pValue1Result   The first value and result.
 * @param   cBits           The number of bits to shift.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignShiftLeft(PRTUINT128U pValueResult, int cBits)
{
    RTUINT128U const InVal = *pValueResult;
/** @todo #if ARCH_BITS >= 64 */
    if (cBits > 0)
    {
        /* (left shift) */
        if (cBits >= 128)
            RTUInt128SetZero(pValueResult);
        else if (cBits >= 64)
        {
            pValueResult->s.Lo  = 0;
            pValueResult->s.Hi  = InVal.s.Lo << (cBits - 64);
        }
        else
        {
            pValueResult->s.Hi  = InVal.s.Hi << cBits;
            pValueResult->s.Hi |= InVal.s.Lo >> (64 - cBits);
            pValueResult->s.Lo  = InVal.s.Lo << cBits;
        }
    }
    else if (cBits < 0)
    {
        /* (right shift) */
        cBits = -cBits;
        if (cBits >= 128)
            RTUInt128SetZero(pValueResult);
        else if (cBits >= 64)
        {
            pValueResult->s.Hi  = 0;
            pValueResult->s.Lo  = InVal.s.Hi >> (cBits - 64);
        }
        else
        {
            pValueResult->s.Lo  = InVal.s.Lo >> cBits;
            pValueResult->s.Lo |= InVal.s.Hi << (64 - cBits);
            pValueResult->s.Hi  = InVal.s.Hi >> cBits;
        }
    }
    return pValueResult;
}


/**
 * Performs a bitwise left shift on a 128-bit unsigned integer value, assigning
 * the result to it.
 *
 * @returns pValue1Result.
 * @param   pValue1Result   The first value and result.
 * @param   cBits           The number of bits to shift.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignShiftRight(PRTUINT128U pValueResult, int cBits)
{
    return RTUInt128AssignShiftLeft(pValueResult, -cBits);
}


/**
 * Performs a bitwise NOT on a 128-bit unsigned integer value, assigning the
 * result to it.
 *
 * @returns pValueResult
 * @param   pValueResult    The value and result.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignBitwiseNot(PRTUINT128U pValueResult)
{
#if ARCH_BITS >= 64
    pValueResult->s.Hi = ~pValueResult->s.Hi;
    pValueResult->s.Lo = ~pValueResult->s.Lo;
#else
    pValueResult->DWords.dw0 = ~pValueResult->DWords.dw0;
    pValueResult->DWords.dw1 = ~pValueResult->DWords.dw1;
    pValueResult->DWords.dw2 = ~pValueResult->DWords.dw2;
    pValueResult->DWords.dw3 = ~pValueResult->DWords.dw3;
#endif
    return pValueResult;
}


/**
 * Performs a boolean NOT on a 128-bit unsigned integer value, assigning the
 * result to it.
 *
 * @returns pValueResult
 * @param   pValueResult    The value and result.
 */
DECLINLINE(PRTUINT128U) RTUInt128AssignBooleanNot(PRTUINT128U pValueResult)
{
    return RTUInt128AssignBoolean(pValueResult, RTUInt128IsZero(pValueResult));
}


/**
 * Compares two 128-bit unsigned integer values.
 *
 * @retval  0 if equal.
 * @retval  -1 if the first value is smaller than the second.
 * @retval  1  if the first value is larger than the second.
 *
 * @param   pValue1             The first value.
 * @param   pValue2             The second value.
 */
DECLINLINE(int) RTUInt128Compare(PCRTUINT128U pValue1, PCRTUINT128U pValue2)
{
#if ARCH_BITS >= 64
    if (pValue1->s.Hi != pValue2->s.Hi)
        return pValue1->s.Hi > pValue2->s.Hi ? 1 : -1;
    if (pValue1->s.Lo != pValue2->s.Lo)
        return pValue1->s.Lo > pValue2->s.Lo ? 1 : -1;
    return 0;
#else
    if (pValue1->DWords.dw0 != pValue2->DWords.dw0)
        return pValue1->DWords.dw0 > pValue2->DWords.dw0 ? 1 : -1;
    if (pValue1->DWords.dw1 != pValue2->DWords.dw1)
        return pValue1->DWords.dw1 > pValue2->DWords.dw1 ? 1 : -1;
    if (pValue1->DWords.dw2 != pValue2->DWords.dw2)
        return pValue1->DWords.dw2 > pValue2->DWords.dw2 ? 1 : -1;
    if (pValue1->DWords.dw3 != pValue2->DWords.dw3)
        return pValue1->DWords.dw3 > pValue2->DWords.dw3 ? 1 : -1;
    return 0;
#endif
}


/**
 * Tests if two 128-bit unsigned integer values not equal.
 *
 * @returns true if equal, false if not equal.
 * @param   pValue1             The first value.
 * @param   pValue2             The second value.
 */
DECLINLINE(bool) RTUInt128IsEqual(PCRTUINT128U pValue1, PCRTUINT128U pValue2)
{
#if ARCH_BITS >= 64
    return pValue1->s.Hi == pValue2->s.Hi
        && pValue1->s.Lo == pValue2->s.Lo;
#else
    return pValue1->DWords.dw0 == pValue2->DWords.dw0
        && pValue1->DWords.dw1 == pValue2->DWords.dw1
        && pValue1->DWords.dw2 == pValue2->DWords.dw2
        && pValue1->DWords.dw3 == pValue2->DWords.dw3;
#endif
}


/**
 * Tests if two 128-bit unsigned integer values are not equal.
 *
 * @returns true if not equal, false if equal.
 * @param   pValue1             The first value.
 * @param   pValue2             The second value.
 */
DECLINLINE(bool) RTUInt128IsNotEqual(PCRTUINT128U pValue1, PCRTUINT128U pValue2)
{
    return !RTUInt128IsEqual(pValue1, pValue2);
}


/**
 * Sets a bit in a 128-bit unsigned integer type.
 *
 * @returns pValueResult.
 * @param   pValueResult    The input and output value.
 * @param   iBit            The bit to set.
 */
DECLINLINE(PRTUINT128U) RTUInt128BitSet(PRTUINT128U pValueResult, unsigned iBit)
{
    if (iBit < 64)
    {
#if ARCH_BITS >= 64
        pValueResult->s.Lo |= RT_BIT_64(iBit);
#else
        if (iBit < 32)
            pValueResult->DWords.dw0 |= RT_BIT_32(iBit);
        else
            pValueResult->DWords.dw1 |= RT_BIT_32(iBit - 32);
#endif
    }
    else if (iBit < 128)
    {
#if ARCH_BITS >= 64
        pValueResult->s.Hi |= RT_BIT_64(iBit - 64);
#else
        if (iBit < 96)
            pValueResult->DWords.dw2 |= RT_BIT_32(iBit - 64);
        else
            pValueResult->DWords.dw3 |= RT_BIT_32(iBit - 96);
#endif
    }
    return pValueResult;
}


/**
 * Sets a bit in a 128-bit unsigned integer type.
 *
 * @returns pValueResult.
 * @param   pValueResult    The input and output value.
 * @param   iBit            The bit to set.
 */
DECLINLINE(PRTUINT128U) RTUInt128BitClear(PRTUINT128U pValueResult, unsigned iBit)
{
    if (iBit < 64)
    {
#if ARCH_BITS >= 64
        pValueResult->s.Lo &= ~RT_BIT_64(iBit);
#else
        if (iBit < 32)
            pValueResult->DWords.dw0 &= ~RT_BIT_32(iBit);
        else
            pValueResult->DWords.dw1 &= ~RT_BIT_32(iBit - 32);
#endif
    }
    else if (iBit < 128)
    {
#if ARCH_BITS >= 64
        pValueResult->s.Hi &= ~RT_BIT_64(iBit - 64);
#else
        if (iBit < 96)
            pValueResult->DWords.dw2 &= ~RT_BIT_32(iBit - 64);
        else
            pValueResult->DWords.dw3 &= ~RT_BIT_32(iBit - 96);
#endif
    }
    return pValueResult;
}


/**
 * Tests if a bit in a 128-bit unsigned integer value is set.
 *
 * @returns pValueResult.
 * @param   pValueResult    The input and output value.
 * @param   iBit            The bit to test.
 */
DECLINLINE(bool) RTUInt128BitTest(PRTUINT128U pValueResult, unsigned iBit)
{
    bool fRc;
    if (iBit < 64)
    {
#if ARCH_BITS >= 64
        fRc = RT_BOOL(pValueResult->s.Lo & RT_BIT_64(iBit));
#else
        if (iBit < 32)
            fRc = RT_BOOL(pValueResult->DWords.dw0 & RT_BIT_32(iBit));
        else
            fRc = RT_BOOL(pValueResult->DWords.dw1 & RT_BIT_32(iBit - 32));
#endif
    }
    else if (iBit < 128)
    {
#if ARCH_BITS >= 64
        fRc = RT_BOOL(pValueResult->s.Hi & RT_BIT_64(iBit - 64));
#else
        if (iBit < 96)
            fRc = RT_BOOL(pValueResult->DWords.dw2 & RT_BIT_32(iBit - 64));
        else
            fRc = RT_BOOL(pValueResult->DWords.dw3 & RT_BIT_32(iBit - 96));
#endif
    }
    else
        fRc = false;
    return fRc;
}


/**
 * Set a range of bits a 128-bit unsigned integer value.
 *
 * @returns pValueResult.
 * @param   pValueResult    The input and output value.
 * @param   iFirstBit       The first bit to test.
 * @param   cBits           The number of bits to set.
 */
DECLINLINE(PRTUINT128U) RTUInt128BitSetRange(PRTUINT128U pValueResult, unsigned iFirstBit, unsigned cBits)
{
    /* bounds check & fix. */
    if (iFirstBit < 128)
    {
        if (iFirstBit + cBits > 128)
            cBits = 128 - iFirstBit;

#if ARCH_BITS >= 64
        if (iFirstBit + cBits < 64)
            pValueResult->s.Lo |= (RT_BIT_64(cBits) - 1) << iFirstBit;
        else if (iFirstBit + cBits < 128 && iFirstBit >= 64)
            pValueResult->s.Hi |= (RT_BIT_64(cBits) - 1) << (iFirstBit - 64);
        else
#else
        if (iFirstBit + cBits < 32)
            pValueResult->DWords.dw0 |= (RT_BIT_32(cBits) - 1) << iFirstBit;
        else if (iFirstBit + cBits < 64 && iFirstBit >= 32)
            pValueResult->DWords.dw1 |= (RT_BIT_32(cBits) - 1) << (iFirstBit - 32);
        else if (iFirstBit + cBits < 96 && iFirstBit >= 64)
            pValueResult->DWords.dw2 |= (RT_BIT_32(cBits) - 1) << (iFirstBit - 64);
        else if (iFirstBit + cBits < 128 && iFirstBit >= 96)
            pValueResult->DWords.dw3 |= (RT_BIT_32(cBits) - 1) << (iFirstBit - 96);
        else
#endif
            while (cBits-- > 0)
                RTUInt128BitSet(pValueResult, iFirstBit++);
    }
    return pValueResult;
}


/**
 * Test if all the bits of a 128-bit unsigned integer value are set.
 *
 * @returns true if they are, false if they aren't.
 * @param   pValue          The input and output value.
 */
DECLINLINE(bool) RTUInt128BitAreAllSet(PRTUINT128U pValue)
{
#if ARCH_BITS >= 64
    return pValue->s.Hi == UINT64_MAX
        && pValue->s.Lo == UINT64_MAX;
#else
    return pValue->DWords.dw0 == UINT32_MAX
        && pValue->DWords.dw1 == UINT32_MAX
        && pValue->DWords.dw2 == UINT32_MAX
        && pValue->DWords.dw3 == UINT32_MAX;
#endif
}


/**
 * Test if all the bits of a 128-bit unsigned integer value are clear.
 *
 * @returns true if they are, false if they aren't.
 * @param   pValue          The input and output value.
 */
DECLINLINE(bool) RTUInt128BitAreAllClear(PRTUINT128U pValue)
{
#if ARCH_BITS >= 64
    return pValue->s.Hi == 0
        && pValue->s.Lo == 0;
#else
    return pValue->DWords.dw0 == 0
        && pValue->DWords.dw1 == 0
        && pValue->DWords.dw2 == 0
        && pValue->DWords.dw3 == 0;
#endif
}

/** @} */

RT_C_DECLS_END

#endif

