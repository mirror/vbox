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
typedef int FNDISPARSEARMV8(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit);
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
static FNDISPARSEARMV8 disArmV8ParseImm;
static FNDISPARSEARMV8 disArmV8ParseImmRel;
static FNDISPARSEARMV8 disArmV8ParseImmAdr;
static FNDISPARSEARMV8 disArmV8ParseReg;
static FNDISPARSEARMV8 disArmV8ParseImmsImmrN;
static FNDISPARSEARMV8 disArmV8ParseHw;
static FNDISPARSEARMV8 disArmV8ParseCond;
static FNDISPARSEARMV8 disArmV8ParsePState;
/** @}  */


/** @name Decoders
 * @{ */
static FNDISDECODEARMV8 disArmV8DecodeIllegal;
static FNDISDECODEARMV8 disArmV8DecodeLookup;
/** @} */


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
/** Parser opcode table for full disassembly. */
static PFNDISPARSEARMV8 const g_apfnDisasm[kDisParmParseMax] =
{
    disArmV8ParseIllegal,
    disArmV8ParseImm,
    disArmV8ParseImmRel,
    disArmV8ParseImmAdr,
    disArmV8ParseReg,
    disArmV8ParseImmsImmrN,
    disArmV8ParseHw,
    disArmV8ParseCond,
    disArmV8ParsePState,
};


/** Opcode decoder table. */
static PFNDISDECODEARMV8 const g_apfnOpcDecode[kDisArmV8OpcDecodeMax] =
{
    disArmV8DecodeIllegal,
    disArmV8DecodeLookup,
};


DECLINLINE(uint32_t) disArmV8ExtractBitVecFromInsn(uint32_t u32Insn, uint8_t idxBitStart, uint8_t cBits)
{
    uint32_t fMask = RT_BIT_32(idxBitStart + cBits) - 1;
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


static int disArmV8ParseIllegal(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, u32Insn, pInsnClass, pParam, pInsnParm, f64Bit);
    AssertFailed();
    return VERR_INTERNAL_ERROR;
}


static int disArmV8ParseImm(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, pInsnClass, f64Bit);

    AssertReturn(pInsnParm->idxBitStart + pInsnParm->cBits < 32, VERR_INTERNAL_ERROR_2);

    pParam->uValue = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    if (pInsnParm->cBits <= 8)
    {
        pParam->arch.armv8.cb = sizeof(uint8_t);
        pParam->fUse |= DISUSE_IMMEDIATE8;
    }
    else if (pInsnParm->cBits <= 16)
    {
        pParam->arch.armv8.cb = sizeof(uint16_t);
        pParam->fUse |= DISUSE_IMMEDIATE16;
    }
    else if (pInsnParm->cBits <= 32)
    {
        pParam->arch.armv8.cb = sizeof(uint32_t);
        pParam->fUse |= DISUSE_IMMEDIATE32;
    }
    else
        AssertReleaseFailed();

    return VINF_SUCCESS;
}


static int disArmV8ParseImmRel(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, pInsnClass, f64Bit);

    AssertReturn(pInsnParm->idxBitStart + pInsnParm->cBits < 32, VERR_INTERNAL_ERROR_2);

    pParam->uValue = (int64_t)disArmV8ExtractBitVecFromInsnSignExtend(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    if (pInsnParm->cBits <= 8)
    {
        pParam->arch.armv8.cb = sizeof(int8_t);
        pParam->fUse |= DISUSE_IMMEDIATE8_REL;
    }
    else if (pInsnParm->cBits <= 16)
    {
        pParam->arch.armv8.cb = sizeof(int16_t);
        pParam->fUse |= DISUSE_IMMEDIATE16_REL;
    }
    else if (pInsnParm->cBits <= 32)
    {
        pParam->arch.armv8.cb = sizeof(int32_t);
        pParam->fUse |= DISUSE_IMMEDIATE32_REL;
    }
    else
        AssertReleaseFailed();

    return VINF_SUCCESS;
}


static int disArmV8ParseImmAdr(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, pInsnClass, f64Bit, pInsnParm);

    pParam->uValue  = disArmV8ExtractBitVecFromInsn(u32Insn, 5, 19);
    pParam->uValue |= disArmV8ExtractBitVecFromInsn(u32Insn, 29, 2) << 29;
    pParam->fUse |= DISUSE_IMMEDIATE32;
    return VINF_SUCCESS;
}


static int disArmV8ParseReg(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, pInsnClass);
    pParam->arch.armv8.Reg.idxGenReg = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    pParam->arch.armv8.cb            = f64Bit ? sizeof(uint64_t) : sizeof(uint32_t);
    pParam->fUse |= f64Bit ? DISUSE_REG_GEN64 : DISUSE_REG_GEN32;
    return VINF_SUCCESS;
}


static int disArmV8ParseImmsImmrN(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis);
    AssertReturn(pInsnParm->cBits == 13, VERR_INTERNAL_ERROR_2);

    uint32_t u32ImmRaw = disArmV8ExtractBitVecFromInsn(u32Insn, pInsnParm->idxBitStart, pInsnParm->cBits);
    /* N bit must be 0 if 32-bit variant is used. */
    if (   (   (u32ImmRaw & RT_BIT_32(12))
            && !f64Bit)
        || (   !(u32ImmRaw & RT_BIT_32(12))
            && f64Bit
            && (pInsnClass->fClass & DISARMV8INSNCLASS_F_N_FORCED_1_ON_64BIT)))
        return VERR_DIS_INVALID_OPCODE;

    /** @todo Decode according to spec. */
    pParam->uValue        = u32ImmRaw;
    pParam->arch.armv8.cb = sizeof(uint32_t);
    pParam->fUse         |= DISUSE_IMMEDIATE32;
    return VINF_SUCCESS;
}


static int disArmV8ParseHw(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, u32Insn, pInsnClass, pParam, pInsnParm, f64Bit);
    AssertFailed();
    /** @todo */
    return VINF_SUCCESS;
}


static int disArmV8ParseCond(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, u32Insn, pInsnClass, pParam, pInsnParm, f64Bit);
    //AssertFailed();
    /** @todo */
    return VINF_SUCCESS;
}


static int disArmV8ParsePState(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8INSNCLASS pInsnClass, PDISOPPARAM pParam, PCDISARMV8INSNPARAM pInsnParm, bool f64Bit)
{
    RT_NOREF(pDis, u32Insn, pInsnClass, pParam, pInsnParm, f64Bit);
    //AssertFailed();
    /** @todo */
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


static int disArmV8A64ParseInstruction(PDISSTATE pDis, uint32_t u32Insn, PCDISARMV8OPCODE pOp, PCDISARMV8INSNCLASS pInsnClass)
{
    AssertPtr(pOp);
    AssertPtr(pDis);
    Assert((u32Insn & pOp->fMask) == pOp->fValue);

    /* Should contain the parameter type on input. */
    pDis->Param1.arch.armv8.fParam = pOp->Opc.fParam1;
    pDis->Param2.arch.armv8.fParam = pOp->Opc.fParam2;
    pDis->Param3.arch.armv8.fParam = pOp->Opc.fParam3;
    pDis->Param4.arch.armv8.fParam = pOp->Opc.fParam4;

    pDis->pCurInstr = &pOp->Opc;
    Assert(&pOp->Opc != &g_ArmV8A64InvalidOpcode[0]);

    bool f64Bit = false;

    if (pInsnClass->fClass & DISARMV8INSNCLASS_F_SF)
        f64Bit = RT_BOOL(u32Insn & RT_BIT_32(31));
    else if (pInsnClass->fClass & DISARMV8INSNCLASS_F_FORCED_64BIT)
        f64Bit = true;

    int rc = VINF_SUCCESS;
    if (pInsnClass->aParms[0].idxParse != kDisParmParseNop)
        rc = g_apfnDisasm[pInsnClass->aParms[0].idxParse](pDis, u32Insn, pInsnClass, &pDis->Param1, &pInsnClass->aParms[0], f64Bit);

    if (   pInsnClass->aParms[1].idxParse != kDisParmParseNop
        && RT_SUCCESS(rc))
        rc = g_apfnDisasm[pInsnClass->aParms[1].idxParse](pDis, u32Insn, pInsnClass, &pDis->Param2, &pInsnClass->aParms[1], f64Bit);

    if (   pInsnClass->aParms[2].idxParse != kDisParmParseNop
        && RT_SUCCESS(rc))
        rc = g_apfnDisasm[pInsnClass->aParms[2].idxParse](pDis, u32Insn, pInsnClass, &pDis->Param3, &pInsnClass->aParms[2], f64Bit);

    if (   pInsnClass->aParms[3].idxParse != kDisParmParseNop
        && RT_SUCCESS(rc))
        rc = g_apfnDisasm[pInsnClass->aParms[3].idxParse](pDis, u32Insn, pInsnClass, &pDis->Param4, &pInsnClass->aParms[3], f64Bit);

    /* If parameter parsing returned an invalid opcode error the encoding is invalid. */
    if (rc == VERR_DIS_INVALID_OPCODE)
    {
        pDis->pCurInstr = &g_ArmV8A64InvalidOpcode[0];

        pDis->Param1.arch.armv8.fParam = g_ArmV8A64InvalidOpcode[0].fParam1;
        pDis->Param2.arch.armv8.fParam = g_ArmV8A64InvalidOpcode[0].fParam2;
        pDis->Param3.arch.armv8.fParam = g_ArmV8A64InvalidOpcode[0].fParam3;
        pDis->Param4.arch.armv8.fParam = g_ArmV8A64InvalidOpcode[0].fParam4;
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

        pDis->u.u32   = RT_LE2H_U32(u32Insn);
        pDis->cbInstr = sizeof(u32Insn);

        return disInstrArmV8DecodeWorker(pDis, u32Insn, &g_ArmV8A64DecodeL0.Hdr);
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
