/* $Id$ */
/** @file
 * VBox Disassembler - Core Components.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
 *
 * This file is part of VirtualBox base platform packages, as
 * available from https://www.virtualbox.org.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation, in version 3 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 *
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_DIS
#include <VBox/dis.h>
#include <VBox/log.h>
#include <iprt/asm.h> /* Required to get Armv8A64ConvertImmRImmS2Mask64() from armv8.h. */
#include <iprt/armv8.h>
#include <iprt/assert.h>
#include <iprt/errcore.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include "DisasmInternal-armv8.h"


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/

/** Parser callback.
 * @remark no DECLCALLBACK() here because it's considered to be internal and
 *         there is no point in enforcing CDECL. */
typedef int FNDISPARSEARMV8(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit);
/** Pointer to a disassembler parser function. */
typedef FNDISPARSEARMV8 *PFNDISPARSEARMV8;


/** Opcode decoder callback.
 * @remark no DECLCALLBACK() here because it's considered to be internal and
 *         there is no point in enforcing CDECL. */
typedef uint32_t FNDISDECODEARMV8(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass);
/** Pointer to a disassembler parser function. */
typedef FNDISDECODEARMV8 *PFNDISDECODEARMV8;


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/


/*********************************************************************************************************************************
*   Internal Functions                                                                                                           *
*********************************************************************************************************************************/
/** @name Parsers
 * @{ */
static FNDISPARSEARMV8 disArmV8ParseIllegal;
static FNDISPARSEARMV8 disArmV8ParseSize;
static FNDISPARSEARMV8 disArmV8ParseImm;
static FNDISPARSEARMV8 disArmV8ParseImmRel;
static FNDISPARSEARMV8 disArmV8ParseImmAdr;
static FNDISPARSEARMV8 disArmV8ParseReg;
static FNDISPARSEARMV8 disArmV8ParseImmsImmrN;
static FNDISPARSEARMV8 disArmV8ParseHw;
static FNDISPARSEARMV8 disArmV8ParseCond;
static FNDISPARSEARMV8 disArmV8ParsePState;
static FNDISPARSEARMV8 disArmV8ParseSysReg;
static FNDISPARSEARMV8 disArmV8ParseSh12;
static FNDISPARSEARMV8 disArmV8ParseImmTbz;
static FNDISPARSEARMV8 disArmV8ParseShift;
static FNDISPARSEARMV8 disArmV8ParseShiftAmount;
static FNDISPARSEARMV8 disArmV8ParseImmMemOff;
static FNDISPARSEARMV8 disArmV8ParseSImmMemOff;
/** @}  */


/** @name Decoders
 * @{ */
static FNDISDECODEARMV8 disArmV8DecodeIllegal;
static FNDISDECODEARMV8 disArmV8DecodeLookup;
static FNDISDECODEARMV8 disArmV8DecodeCollate;
/** @} */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Parser opcode table for full disassembly. */
static PFNDISPARSEARMV8 const g_apfnDisasm[kDisParmParseMax] =
{
    disArmV8ParseIllegal,
    disArmV8ParseSize,
    disArmV8ParseImm,
    disArmV8ParseImmRel,
    disArmV8ParseImmAdr,
    disArmV8ParseReg,
    disArmV8ParseImmsImmrN,
    disArmV8ParseHw,
    disArmV8ParseCond,
    disArmV8ParsePState,
    NULL,
    disArmV8ParseSysReg,
    disArmV8ParseSh12,
    disArmV8ParseImmTbz,
    disArmV8ParseShift,
    disArmV8ParseShiftAmount,
    disArmV8ParseImmMemOff,
    disArmV8ParseSImmMemOff
};


/** Opcode decoder table. */
static PFNDISDECODEARMV8 const g_apfnOpcDecode[kDisArmV8OpcDecodeMax] =
{
    disArmV8DecodeIllegal,
    disArmV8DecodeLookup,
    disArmV8DecodeCollate
};


DECLINLINE(uint32_t) disArmV8ExtractBitVecFromInsn(uint32_t u32Insn, uint8_t idxBitStart, uint8_t cBits)
{
    uint32_t fMask = (uint32_t)(RT_BIT_64(idxBitStart + cBits) - 1);
    return (u32Insn & fMask) >> idxBitStart;
}


DECLINLINE(int32_t) disArmV8ExtractBitVecFromInsnSignExtend(uint32_t u32Insn, uint8_t idxBitStart, uint8_t cBits)
{
    uint32_t fMask = RT_BIT_32(idxBitStart + cBits) - 1;
    uint32_t fSign = ~(UINT32_MAX & (RT_BIT_32(cBits - 1) - 1));
    uint32_t fValue = (u32Insn & fMask) >> idxBitStart;
    if (fValue & fSign)
        return (int32_t)(fValue | fSign);

    return (int32_t)fValue;
}


static int disArmV8ParseIllegal(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, u32Insn, pOp, pInsnClass, pParam, pInsnParm, pf64Bit);
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


static int disArmV8ParseSize(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pInsnClass, pParam);

    Assert(pInsnParm->cBits == 2);
    uint32_t u32Size = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    switch (u32Size)
    {
        case 0: pDis->armv8.cbOperand = sizeof(uint8_t); break;
        case 1: pDis->armv8.cbOperand = sizeof(uint16_t); break;
        case 2: pDis->armv8.cbOperand = sizeof(uint32_t); break;
        case 3: pDis->armv8.cbOperand = sizeof(uint64_t); break;
        default:
            AssertReleaseFailed();
    }
    *pf64Bit =    pDis->armv8.cbOperand == sizeof(uint64_t)
               || (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_64BIT);
    return VINF_SUCCESS;
}


static int disArmV8ParseImm(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);

    AssertReturn(pInsnParm->idxBitStart + pInsnParm->cBits < 32, VERR_INTERNAL_ERROR_2);

    pParam->uValue = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    if (pInsnParm->cBits <= 8)
    {
        pParam->armv8.cb = sizeof(uint8_t);
        pParam->fUse |= DISUSE_IMMEDIATE8;
    }
    else if (pInsnParm->cBits <= 16)
    {
        pParam->armv8.cb = sizeof(uint16_t);
        pParam->fUse |= DISUSE_IMMEDIATE16;
    }
    else if (pInsnParm->cBits <= 32)
    {
        pParam->armv8.cb = sizeof(uint32_t);
        pParam->fUse |= DISUSE_IMMEDIATE32;
    }
    else
        AssertReleaseFailed();

    return VINF_SUCCESS;
}


static int disArmV8ParseImmRel(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);

    AssertReturn(pInsnParm->idxBitStart + pInsnParm->cBits < 32, VERR_INTERNAL_ERROR_2);

    pParam->uValue = (int64_t)disArmV8ExtractBitVecFromInsnSignExtend(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    if (pInsnParm->cBits <= 8)
    {
        pParam->armv8.cb = sizeof(int8_t);
        pParam->fUse |= DISUSE_IMMEDIATE8_REL;
    }
    else if (pInsnParm->cBits <= 16)
    {
        pParam->armv8.cb = sizeof(int16_t);
        pParam->fUse |= DISUSE_IMMEDIATE16_REL;
    }
    else if (pInsnParm->cBits <= 32)
    {
        pParam->armv8.cb = sizeof(int32_t);
        pParam->fUse |= DISUSE_IMMEDIATE32_REL;
    }
    else
        AssertReleaseFailed();

    return VINF_SUCCESS;
}


static int disArmV8ParseImmAdr(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit, pInsnParm);

    pParam->uValue  = disArmV8ExtractBitVecFromInsn(u32Insn, 5, 19);
    pParam->uValue |= disArmV8ExtractBitVecFromInsn(u32Insn, 29, 2) << 29;
    pParam->fUse |= DISUSE_IMMEDIATE32;
    return VINF_SUCCESS;
}


static int disArmV8ParseReg(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass);
    pParam->armv8.Reg.idxGenReg = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    pParam->armv8.cb            = *pf64Bit ? sizeof(uint64_t) : sizeof(uint32_t);
    pParam->fUse |=   (*pf64Bit || (pParam->armv8.enmType == kDisArmv8OpParmAddrInGpr))
                    ? DISUSE_REG_GEN64
                    : DISUSE_REG_GEN32;
    return VINF_SUCCESS;
}


static int disArmV8ParseImmsImmrN(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp);
    AssertReturn(pInsnParm->cBits == 13, VERR_INTERNAL_ERROR_2);

    uint32_t u32ImmRaw = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    /* N bit must be 0 if 32-bit variant is used. */
    if (   (   (u32ImmRaw & RT_BIT_32(12))
            && !*pf64Bit)
        || (   !(u32ImmRaw & RT_BIT_32(12))
            && *pf64Bit
            && (pInsnClass->fClass & DISARMV8INSNCLASS_F_N_FORCED_1_ON_64BIT)))
        return VERR_DIS_INVALID_OPCODE;

    uint32_t uImm7SizeLen   = ((u32ImmRaw & RT_BIT_32(12)) >> 6) | (u32ImmRaw & 0x3f);
    uint32_t uImm6Rotations = (u32ImmRaw >> 6) & 0x3f;
    pParam->uValue        =   *pf64Bit
                            ? Armv8A64ConvertImmRImmS2Mask64(uImm7SizeLen, uImm6Rotations)
                            : Armv8A64ConvertImmRImmS2Mask32(uImm7SizeLen, uImm6Rotations);
    pParam->armv8.cb      = pParam->uValue > UINT32_MAX ? sizeof(uint64_t) : sizeof(uint32_t);
    pParam->fUse         |= pParam->uValue > UINT32_MAX ? DISUSE_IMMEDIATE64 : DISUSE_IMMEDIATE32;
    return VINF_SUCCESS;
}


static int disArmV8ParseHw(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pParam);
    Assert(pInsnParm->cBits == 2);

    uint32_t u32 = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    /* hw<1> must be 0 if this is the 32-bit variant. */
    if (   !*pf64Bit
        && (u32 & RT_BIT_32(1)))
        return VERR_DIS_INVALID_OPCODE;

    Assert(pParam->armv8.enmType == kDisArmv8OpParmImm);
    Assert(pParam->fUse & (DISUSE_IMMEDIATE8 | DISUSE_IMMEDIATE16 | DISUSE_IMMEDIATE32));
    if (u32)
    {
        pParam->armv8.enmShift = kDisArmv8OpParmShiftLeft;
        pParam->armv8.u.cShift = ((uint8_t)u32 & 0x3) << 4;
    }
    return VINF_SUCCESS;
}


static int disArmV8ParseCond(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pInsnClass, pOp, pParam, pf64Bit);
    Assert(pInsnParm->cBits <= 4);
    if (pParam)
    {
        /* Conditional as a parameter (CCMP/CCMN). */
        Assert(pParam->armv8.enmType == kDisArmv8OpParmCond);
        pParam->armv8.Reg.enmCond = (DISARMV8INSTRCOND)disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    }
    else /* Conditional for the base instruction. */
        pDis->armv8.enmCond = (DISARMV8INSTRCOND)disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    return VINF_SUCCESS;
}


static int disArmV8ParsePState(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, u32Insn, pOp, pInsnClass, pParam, pInsnParm, pf64Bit);
    //AssertFailed();
    /** @todo */
    return VINF_SUCCESS;
}


static int disArmV8ParseSysReg(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);
    AssertReturn(pInsnParm->cBits == 15, VERR_INTERNAL_ERROR_2);

    /* Assumes a op0:op1:CRn:CRm:op2 encoding in the instruction starting at the given bit position. */
    uint32_t u32ImmRaw = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    pParam->armv8.Reg.idSysReg = ARMV8_AARCH64_SYSREG_ID_CREATE(2 + ((u32ImmRaw >> 14) & 0x1),
                                                                (u32ImmRaw >> 11) & 0x7,
                                                                (u32ImmRaw >> 7) & 0xf,
                                                                (u32ImmRaw >> 3) & 0xf,
                                                                u32ImmRaw & 0x7);
    pParam->armv8.cb = 0;
    pParam->fUse    |= DISUSE_REG_SYSTEM;
    return VINF_SUCCESS;
}


static int disArmV8ParseSh12(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);
    Assert(pInsnParm->cBits == 1);
    if (u32Insn & RT_BIT_32(pInsnParm->idxBitStart))
    {
        /* Shift the immediate pointed to. */
        pParam->uValue <<= 12;

        /* Re-evaluate the immediate data size. */
        pParam->fUse &= ~(DISUSE_IMMEDIATE8 | DISUSE_IMMEDIATE16 | DISUSE_IMMEDIATE32);
        if (pParam->uValue <= UINT8_MAX)
        {
            pParam->armv8.cb = sizeof(uint8_t);
            pParam->fUse |= DISUSE_IMMEDIATE8;
        }
        else if (pParam->uValue <= UINT16_MAX)
        {
            pParam->armv8.cb = sizeof(uint16_t);
            pParam->fUse |= DISUSE_IMMEDIATE16;
        }
        else if (pParam->uValue <= UINT32_MAX)
        {
            pParam->armv8.cb = sizeof(uint32_t);
            pParam->fUse |= DISUSE_IMMEDIATE32;
        }
        else
            AssertReleaseFailed();

    }
    return VINF_SUCCESS;
}


static int disArmV8ParseImmTbz(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);

    AssertReturn(!pInsnParm->idxBitStart && !pInsnParm->cBits, VERR_INTERNAL_ERROR_2);

    pParam->uValue = disArmV8ExtractBitVecFromInsn(u32Insn, 19, 5);
    pParam->uValue |= (u32Insn & RT_BIT_32(31)) >> 26;

    pParam->armv8.cb = sizeof(uint8_t);
    pParam->fUse |= DISUSE_IMMEDIATE8;
    return VINF_SUCCESS;
}


static int disArmV8ParseShift(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);

    AssertReturn(pInsnParm->cBits == 2, VERR_INTERNAL_ERROR_2);

    uint32_t u32Shift = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    switch (u32Shift)
    {
        case 0: pParam->armv8.enmShift = kDisArmv8OpParmShiftLeft;       break;
        case 1: pParam->armv8.enmShift = kDisArmv8OpParmShiftRight;      break;
        case 2: pParam->armv8.enmShift = kDisArmv8OpParmShiftArithRight; break;
        case 3: pParam->armv8.enmShift = kDisArmv8OpParmShiftRotate;     break;
        default:
            AssertReleaseFailed();
    }
    return VINF_SUCCESS;
}


static int disArmV8ParseShiftAmount(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pDis, pOp, pInsnClass, pf64Bit);

    uint32_t u32Amount = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    /* For a 32-bit operand it is impossible to shift/rotate more than 31 bits. */
    if (   !*pf64Bit
        && u32Amount > 31)
        return VERR_DIS_INVALID_OPCODE;

    Assert(pParam->armv8.enmShift != kDisArmv8OpParmShiftNone);
    Assert(u32Amount < 64);
    pParam->armv8.u.cShift = (uint8_t)u32Amount;
    /* Any shift operation with a 0 is essentially no shift being applied. */
    if (pParam->armv8.u.cShift == 0)
        pParam->armv8.enmShift = kDisArmv8OpParmShiftNone;
    return VINF_SUCCESS;
}


static int disArmV8ParseImmMemOff(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pInsnClass, pOp, pf64Bit);

    AssertReturn(pInsnParm->cBits <= 12, VERR_INTERNAL_ERROR_2);
    AssertReturn(pDis->armv8.cbOperand != 0, VERR_INTERNAL_ERROR_2);

    pParam->armv8.u.offBase = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    switch (pDis->armv8.cbOperand)
    {
        case sizeof(uint8_t): break;
        case sizeof(uint16_t): pParam->armv8.u.offBase <<= 1; break;
        case sizeof(uint32_t): pParam->armv8.u.offBase <<= 2; break;
        case sizeof(uint64_t): pParam->armv8.u.offBase <<= 3; break;
        default:
            AssertReleaseFailed();
    }
    pParam->armv8.cb = sizeof(int16_t);
    return VINF_SUCCESS;
}


static int disArmV8ParseSImmMemOff(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool *pf64Bit)
{
    RT_NOREF(pInsnClass, pf64Bit);

    AssertReturn(pInsnParm->cBits <= 7, VERR_INTERNAL_ERROR_2);
    AssertReturn(   (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_32BIT)
                 || (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_64BIT),
                 VERR_INTERNAL_ERROR_2);

    pParam->armv8.cb = sizeof(int16_t);
    pParam->armv8.u.offBase = disArmV8ExtractBitVecFromInsnSignExtend(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    pParam->armv8.u.offBase <<= (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_32BIT) ? 2 : 3; 
    pDis->armv8.cbOperand   =   (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_32BIT) ? sizeof(uint32_t) : sizeof(uint64_t);
    return VINF_SUCCESS;
}


static uint32_t disArmV8DecodeIllegal(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass)
{
    RT_NOREF(pDis, u32Insn, pInsnClass);
    AssertFailed();
    return UINT32_MAX;
}


static uint32_t disArmV8DecodeLookup(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass)
{
    RT_NOREF(pDis);

    for (uint32_t i = 0; i < pInsnClass->Hdr.cDecode; i++)
    {
        PCDISARMV8OPCODE pOp = &pInsnClass->paOpcodes[i];
        if (u32Insn == pOp->fValue)
            return i;
    }

    return UINT32_MAX;
}


static uint32_t disArmV8DecodeCollate(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass)
{
    RT_NOREF(pDis);

    /* Need to build a compact representation of the relevant bits from the mask to create an index. */
    uint32_t fMask = pInsnClass->fMask >> pInsnClass->cShift;

    /** @todo Optimize. */
    uint32_t idx = 0;
    uint32_t cShift = 0;
    while (fMask)
    {
        if (fMask & 0x1)
        {
            idx |= (u32Insn & 1) << cShift;
            cShift++;
        }

        u32Insn >>= 1;
        fMask   >>= 1;
    }

    if (RT_LIKELY(idx < pInsnClass->Hdr.cDecode))
        return idx;

    return UINT32_MAX;
}


/**
 * Looks for possible alias conversions for the given disassembler state.
 *
 * @param   pDis        The disassembler state to process.
 */
static void disArmV8A64InsnAliasesProcess(PDISSTATE pDis)
{
#define DIS_ARMV8_ALIAS(a_Name) s_DisArmv8Alias ## a_Name
#define DIS_ARMV8_ALIAS_CREATE(a_Name, a_szOpcode, a_uOpcode, a_fOpType) static const DISOPCODE DIS_ARMV8_ALIAS(a_Name) = OP(a_szOpcode, 0, 0, 0, a_uOpcode, 0, 0, 0, a_fOpType)
#define DIS_ARMV8_ALIAS_REF(a_Name) &DIS_ARMV8_ALIAS(a_Name)
    switch (pDis->pCurInstr->uOpcode)
    {
        case OP_ARMV8_A64_ORR:
        {
            /* Check for possible MOV conversion for the register variant when: shift is None and the first source is the zero register. */
            Assert(pDis->aParams[1].armv8.enmType == kDisArmv8OpParmGpr);

            if (   pDis->aParams[2].armv8.enmType == kDisArmv8OpParmGpr
                && pDis->aParams[2].armv8.enmShift == kDisArmv8OpParmShiftNone
                && pDis->aParams[1].armv8.Reg.idxGenReg == ARMV8_A64_REG_XZR)
            {
                DIS_ARMV8_ALIAS_CREATE(Mov, "mov", OP_ARMV8_A64_MOV, DISOPTYPE_HARMLESS);
                pDis->pCurInstr  = DIS_ARMV8_ALIAS_REF(Mov);
                pDis->aParams[1] = pDis->aParams[2];
                pDis->aParams[2].armv8.enmType = kDisArmv8OpParmNone;
            }
            /** @todo Immediate variant. */
            break;
        }
        case OP_ARMV8_A64_SUBS:
        {
            Assert(pDis->aParams[0].armv8.enmType == kDisArmv8OpParmGpr);
            if (pDis->aParams[0].armv8.Reg.idxGenReg == ARMV8_A64_REG_XZR)
            {
                DIS_ARMV8_ALIAS_CREATE(Cmp, "cmp", OP_ARMV8_A64_CMP, DISOPTYPE_HARMLESS);
                pDis->pCurInstr  = DIS_ARMV8_ALIAS_REF(Cmp);
                pDis->aParams[0] = pDis->aParams[1];
                pDis->aParams[1] = pDis->aParams[2];
                pDis->aParams[2].armv8.enmType = kDisArmv8OpParmNone;
            }
            break;
        }
        default:
            break; /* No conversion */
    }
#undef DIS_ARMV8_ALIAS_REF
#undef DIS_ARMV8_ALIAS_CREATE
#undef DIS_ARMV8_ALIAS
}


static int disArmV8A64ParseInstruction(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass)
{
    AssertPtr(pOp);
    AssertPtr(pDis);
    Assert((u32Insn & pInsnClass->fFixedInsn) == pOp->fValue);

    /* Should contain the parameter type on input. */
    pDis->aParams[0].armv8.enmType  = pInsnClass->aenmParamTypes[0];
    pDis->aParams[1].armv8.enmType  = pInsnClass->aenmParamTypes[1];
    pDis->aParams[2].armv8.enmType  = pInsnClass->aenmParamTypes[2];
    pDis->aParams[3].armv8.enmType  = pInsnClass->aenmParamTypes[3];
    pDis->aParams[0].armv8.enmShift = kDisArmv8OpParmShiftNone;
    pDis->aParams[1].armv8.enmShift = kDisArmv8OpParmShiftNone;
    pDis->aParams[2].armv8.enmShift = kDisArmv8OpParmShiftNone;
    pDis->aParams[3].armv8.enmShift = kDisArmv8OpParmShiftNone;
    pDis->armv8.enmCond             = kDisArmv8InstrCond_Al;
    pDis->armv8.cbOperand           = 0;

    pDis->pCurInstr = &pOp->Opc;
    Assert(&pOp->Opc != &g_ArmV8A64InvalidOpcode[0]);

    bool f64Bit = false;

    /** @todo Get rid of these and move them to the per opcode
     * (SF can become a decoder step). */
    if (pInsnClass->fClass & DISARMV8INSNCLASS_F_SF)
        f64Bit = RT_BOOL(u32Insn & RT_BIT_32(31));
    else if (pInsnClass->fClass & DISARMV8INSNCLASS_F_FORCED_64BIT)
        f64Bit = true;

    if (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_32BIT)
        f64Bit = false;
    else if (pOp->fFlags & DISARMV8INSNCLASS_F_FORCED_64BIT)
        f64Bit = true;

    int rc = VINF_SUCCESS;
    PCDISARMV8INSNPARAM pDecode = &pInsnClass->paParms[0];
    while (   (pDecode->idxParse != kDisParmParseNop)
           && RT_SUCCESS(rc))
    {
        rc = g_apfnDisasm[pDecode->idxParse](pDis, u32Insn, pOp, pInsnClass,
                                               pDecode->idxParam != DIS_ARMV8_INSN_PARAM_UNSET
                                             ? &pDis->aParams[pDecode->idxParam]
                                             : NULL,
                                             pDecode, &f64Bit);
        pDecode++;
    }

    /* If parameter parsing returned an invalid opcode error the encoding is invalid. */
    if (RT_SUCCESS(rc)) /** @todo Introduce flag to switch alias conversion on/off. */
        disArmV8A64InsnAliasesProcess(pDis);
    else if (rc == VERR_DIS_INVALID_OPCODE)
    {
        pDis->pCurInstr = &g_ArmV8A64InvalidOpcode[0];

        pDis->aParams[0].armv8.enmType = kDisArmv8OpParmNone;
        pDis->aParams[1].armv8.enmType = kDisArmv8OpParmNone;
        pDis->aParams[2].armv8.enmType = kDisArmv8OpParmNone;
        pDis->aParams[3].armv8.enmType = kDisArmv8OpParmNone;
    }
    pDis->rc = rc;
    return rc;
}


static int disArmV8A64ParseInvOpcode(PDISSTATE pDis)
{
    pDis->pCurInstr = &g_ArmV8A64InvalidOpcode[0];
    pDis->rc = VERR_DIS_INVALID_OPCODE;
    return VERR_DIS_INVALID_OPCODE;
}


static int disInstrArmV8DecodeWorker(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8DECODEHDR pHdr)
{
    while (   pHdr
           && pHdr->enmDecodeType != kDisArmV8DecodeType_InsnClass)
    {
        if (pHdr->enmDecodeType == kDisArmV8DecodeType_Map)
        {
            PCDISARMV8DECODEMAP pMap = (PCDISARMV8DECODEMAP)pHdr;

            uint32_t idxNext = (u32Insn & pMap->fMask) >> pMap->cShift;
            if (RT_LIKELY(idxNext < pMap->Hdr.cDecode))
                pHdr = pMap->papNext[idxNext];
            else
            {
                pHdr = NULL;
                break;
            }
        }
        else
        {
            Assert(pHdr->enmDecodeType == kDisArmV8DecodeType_Table);
            PCDISARMV8DECODETBL pTbl = (PCDISARMV8DECODETBL)pHdr;

            /* Walk all entries in the table and select the best match. */
            pHdr = NULL;
            for (uint32_t i = 0; i < pTbl->Hdr.cDecode; i++)
            {
                PCDISARMV8DECODETBLENTRY pEntry = &pTbl->paEntries[i];
                if ((u32Insn & pEntry->fMask) == pEntry->fValue)
                {
                    pHdr = pEntry->pHdrNext;
                    break;
                }
            }
        }
    }

    if (pHdr)
    {
        Assert(pHdr->enmDecodeType == kDisArmV8DecodeType_InsnClass);
        PCDISARMV8INSNCLASS pInsnClass = (PCDISARMV8INSNCLASS)pHdr;

        /* Decode the opcode from the instruction class. */
        uint32_t uOpcRaw = 0;
        if (pInsnClass->Hdr.cDecode > 1)
        {
            uOpcRaw = (u32Insn & pInsnClass->fMask) >> pInsnClass->cShift;
            if (pInsnClass->enmOpcDecode != kDisArmV8OpcDecodeNop)
                uOpcRaw = g_apfnOpcDecode[pInsnClass->enmOpcDecode](pDis, uOpcRaw, pInsnClass);
        }

        if (uOpcRaw < pInsnClass->Hdr.cDecode)
        {
            PCDISARMV8OPCODE pOp = &pInsnClass->paOpcodes[uOpcRaw];
            return disArmV8A64ParseInstruction(pDis, u32Insn, pOp, pInsnClass);
        }
    }

    return disArmV8A64ParseInvOpcode(pDis);
}


/**
 * Internal worker for DISInstrEx and DISInstrWithPrefetchedBytes.
 *
 * @returns VBox status code.
 * @param   pDis            Initialized disassembler state.
 * @param   paOneByteMap    The one byte opcode map to use.
 * @param   pcbInstr        Where to store the instruction size. Can be NULL.
 */
DECLHIDDEN(int) disInstrWorkerArmV8(PDISSTATE pDis, PCDISOPCODE paOneByteMap, uint32_t *pcbInstr)
{
    RT_NOREF(paOneByteMap);

    if (pDis->uCpuMode == DISCPUMODE_ARMV8_A64)
    {
        *pcbInstr = sizeof(uint32_t);

        /* Instructions are always little endian and 4 bytes. */
        uint32_t u32Insn = disReadDWord(pDis, 0 /*offInstr*/);
        if (RT_FAILURE(pDis->rc))
            return pDis->rc;

        /** @todo r=bird: This is a waste of time if the host is little endian... */
        pDis->Instr.u32 = RT_LE2H_U32(u32Insn);
        pDis->cbInstr   = sizeof(u32Insn);

        return disInstrArmV8DecodeWorker(pDis, u32Insn, &g_aArmV8A64InsnDecodeL0.Hdr);
    }

    AssertReleaseFailed();
    return VERR_NOT_IMPLEMENTED;
}


/**
 * Inlined worker that initializes the disassembler state.
 *
 * @returns The primary opcode map to use.
 * @param   pDis            The disassembler state.
 * @param   uInstrAddr      The instruction address.
 * @param   enmCpuMode      The CPU mode.
 * @param   fFilter         The instruction filter settings.
 * @param   pfnReadBytes    The byte reader, can be NULL.
 * @param   pvUser          The user data for the reader.
 */
DECLHIDDEN(PCDISOPCODE) disInitializeStateArmV8(PDISSTATE pDis, DISCPUMODE enmCpuMode, uint32_t fFilter)
{
    RT_NOREF(pDis, enmCpuMode, fFilter);
    return NULL;
}
