/* $Id$ */
/** @file
 * VBox Disassembler - Core Components.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */


/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_DIS
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/param.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include "DisasmInternal.h"


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** This must be less or equal to DISCPUSTATE::abInstr. */
#define DIS_MAX_INSTR_LENGTH 16

/** Whether we can do unaligned access. */
#if defined(RT_ARCH_X86) || defined(RT_ARCH_AMD64)
# define DIS_HOST_UNALIGNED_ACCESS_OK
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void     disasmModRMReg(PDISCPUSTATE pCpu, PCDISOPCODE pOp, unsigned idx, PDISOPPARAM pParam, int fRegAddr);
static void     disasmModRMReg16(PDISCPUSTATE pCpu, PCDISOPCODE pOp, unsigned idx, PDISOPPARAM pParam);
static void     disasmModRMSReg(PDISCPUSTATE pCpu, PCDISOPCODE pOp, unsigned idx, PDISOPPARAM pParam);


/** @name Parsers
 * @{ */
static FNDISPARSE ParseIllegal;
static FNDISPARSE ParseModRM;
static FNDISPARSE ParseModRM_SizeOnly;
static FNDISPARSE UseModRM;
static FNDISPARSE ParseImmByte;
static FNDISPARSE ParseImmByte_SizeOnly;
static FNDISPARSE ParseImmByteSX;
static FNDISPARSE ParseImmByteSX_SizeOnly;
static FNDISPARSE ParseImmBRel;
static FNDISPARSE ParseImmBRel_SizeOnly;
static FNDISPARSE ParseImmUshort;
static FNDISPARSE ParseImmUshort_SizeOnly;
static FNDISPARSE ParseImmV;
static FNDISPARSE ParseImmV_SizeOnly;
static FNDISPARSE ParseImmVRel;
static FNDISPARSE ParseImmVRel_SizeOnly;
static FNDISPARSE ParseImmZ;
static FNDISPARSE ParseImmZ_SizeOnly;

static FNDISPARSE ParseImmAddr;
static FNDISPARSE ParseImmAddr_SizeOnly;
static FNDISPARSE ParseImmAddrF;
static FNDISPARSE ParseImmAddrF_SizeOnly;
static FNDISPARSE ParseFixedReg;
static FNDISPARSE ParseImmUlong;
static FNDISPARSE ParseImmUlong_SizeOnly;
static FNDISPARSE ParseImmQword;
static FNDISPARSE ParseImmQword_SizeOnly;

static FNDISPARSE ParseTwoByteEsc;
static FNDISPARSE ParseThreeByteEsc4;
static FNDISPARSE ParseThreeByteEsc5;
static FNDISPARSE ParseImmGrpl;
static FNDISPARSE ParseShiftGrp2;
static FNDISPARSE ParseGrp3;
static FNDISPARSE ParseGrp4;
static FNDISPARSE ParseGrp5;
static FNDISPARSE Parse3DNow;
static FNDISPARSE ParseGrp6;
static FNDISPARSE ParseGrp7;
static FNDISPARSE ParseGrp8;
static FNDISPARSE ParseGrp9;
static FNDISPARSE ParseGrp10;
static FNDISPARSE ParseGrp12;
static FNDISPARSE ParseGrp13;
static FNDISPARSE ParseGrp14;
static FNDISPARSE ParseGrp15;
static FNDISPARSE ParseGrp16;
static FNDISPARSE ParseModFence;
static FNDISPARSE ParseNopPause;

static FNDISPARSE ParseYv;
static FNDISPARSE ParseYb;
static FNDISPARSE ParseXv;
static FNDISPARSE ParseXb;

/** Floating point parsing */
static FNDISPARSE ParseEscFP;
/** @}  */


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Parser opcode table for full disassembly. */
static PFNDISPARSE const g_apfnFullDisasm[IDX_ParseMax] =
{
    ParseIllegal,
    ParseModRM,
    UseModRM,
    ParseImmByte,
    ParseImmBRel,
    ParseImmUshort,
    ParseImmV,
    ParseImmVRel,
    ParseImmAddr,
    ParseFixedReg,
    ParseImmUlong,
    ParseImmQword,
    ParseTwoByteEsc,
    ParseImmGrpl,
    ParseShiftGrp2,
    ParseGrp3,
    ParseGrp4,
    ParseGrp5,
    Parse3DNow,
    ParseGrp6,
    ParseGrp7,
    ParseGrp8,
    ParseGrp9,
    ParseGrp10,
    ParseGrp12,
    ParseGrp13,
    ParseGrp14,
    ParseGrp15,
    ParseGrp16,
    ParseModFence,
    ParseYv,
    ParseYb,
    ParseXv,
    ParseXb,
    ParseEscFP,
    ParseNopPause,
    ParseImmByteSX,
    ParseImmZ,
    ParseThreeByteEsc4,
    ParseThreeByteEsc5,
    ParseImmAddrF
};

/** Parser opcode table for only calculating instruction size. */
static PFNDISPARSE const g_apfnCalcSize[IDX_ParseMax] =
{
    ParseIllegal,
    ParseModRM_SizeOnly,
    UseModRM,
    ParseImmByte_SizeOnly,
    ParseImmBRel_SizeOnly,
    ParseImmUshort_SizeOnly,
    ParseImmV_SizeOnly,
    ParseImmVRel_SizeOnly,
    ParseImmAddr_SizeOnly,
    ParseFixedReg,
    ParseImmUlong_SizeOnly,
    ParseImmQword_SizeOnly,
    ParseTwoByteEsc,
    ParseImmGrpl,
    ParseShiftGrp2,
    ParseGrp3,
    ParseGrp4,
    ParseGrp5,
    Parse3DNow,
    ParseGrp6,
    ParseGrp7,
    ParseGrp8,
    ParseGrp9,
    ParseGrp10,
    ParseGrp12,
    ParseGrp13,
    ParseGrp14,
    ParseGrp15,
    ParseGrp16,
    ParseModFence,
    ParseYv,
    ParseYb,
    ParseXv,
    ParseXb,
    ParseEscFP,
    ParseNopPause,
    ParseImmByteSX_SizeOnly,
    ParseImmZ_SizeOnly,
    ParseThreeByteEsc4,
    ParseThreeByteEsc5,
    ParseImmAddrF_SizeOnly
};





//*****************************************************************************
/* Read functions for getting the opcode bytes */
//*****************************************************************************


/**
 * @interface_method_impl{FNDISREADBYTES, The default byte reader callber.}
 */
static DECLCALLBACK(int) disReadBytesDefault(PDISCPUSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead)
{
#ifdef IN_RING0
    AssertMsgFailed(("disReadWord with no read callback in ring 0!!\n"));
    RT_BZERO(&pDis->abInstr[offInstr], cbMaxRead);
    pDis->cbCachedInstr = offInstr + cbMaxRead;
    return VERR_DIS_NO_READ_CALLBACK;
#else
    uint8_t const  *pbSrc        = (uint8_t const *)(uintptr_t)pDis->uInstrAddr + offInstr;
    size_t          cbLeftOnPage = (uintptr_t)pbSrc & PAGE_OFFSET_MASK;
    uint8_t         cbToRead     = cbLeftOnPage >= cbMaxRead
                                 ? cbMaxRead
                                 : cbLeftOnPage <= cbMinRead
                                 ? cbMinRead
                                 : (uint8_t)cbLeftOnPage;
    memcpy(&pDis->abInstr[offInstr], pbSrc, cbToRead);
    pDis->cbCachedInstr = offInstr + cbToRead;
    return VINF_SUCCESS;
#endif
}


/**
 * Read more bytes into the DISCPUSTATE::abInstr buffer, advance
 * DISCPUSTATE::cbCachedInstr.
 *
 * Will set DISCPUSTATE::rc on failure, but still advance cbCachedInstr.
 *
 * The caller shall fend off reads beyond the DISCPUSTATE::abInstr buffer.
 *
 * @param   pCpu                The disassembler state.
 * @param   off                 The offset of the read request.
 * @param   cbMin               The size of the read request that needs to be
 *                              satisfied.
 */
DECL_NO_INLINE(static, void) disReadMore(PDISCPUSTATE pCpu, uint8_t off, uint8_t cbMin)
{
    Assert(cbMin + off <= sizeof(pCpu->abInstr));

    /*
     * Adjust the incoming request to not overlap with bytes that has already
     * been read and to make sure we don't leave unread gaps.
     */
    if (off < pCpu->cbCachedInstr)
    {
        Assert(off + cbMin > pCpu->cbCachedInstr);
        cbMin -= pCpu->cbCachedInstr - off;
        off = pCpu->cbCachedInstr;
    }
    else if (off > pCpu->cbCachedInstr)
    {
        cbMin += off - pCpu->cbCachedInstr;
        off = pCpu->cbCachedInstr;
    }

    /*
     * Do the read.
     * (No need to zero anything on failure as abInstr is already zeroed by the
     * DISInstrEx API.)
     */
    int rc = pCpu->pfnReadBytes(pCpu, off, cbMin, sizeof(pCpu->abInstr) - off);
    if (RT_SUCCESS(rc))
    {
        Assert(pCpu->cbCachedInstr >= off + cbMin);
        Assert(pCpu->cbCachedInstr <= sizeof(pCpu->abInstr));
    }
    else
    {
        Log(("disReadMore failed with rc=%Rrc!!\n", rc));
        pCpu->rc = VERR_DIS_MEM_READ;
    }
}


/**
 * Function for handling a 8-bit cache miss.
 *
 * @returns The requested byte.
 * @param   pCpu                The disassembler state.
 * @param   off                 The offset of the byte relative to the
 *                              instruction.
 */
DECL_NO_INLINE(static, uint8_t) disReadByteSlow(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off >= DIS_MAX_INSTR_LENGTH))
    {
        Log(("disReadByte: too long instruction...\n"));
        pCpu->rc = VERR_DIS_TOO_LONG_INSTR;
        return 0;
    }

    if (off <= pCpu->cbCachedInstr)
        disReadMore(pCpu, off, 1);

    return pCpu->abInstr[off];
}


/**
 * Read a byte (8-bit) instruction byte by offset.
 *
 * @returns The requested byte.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECLINLINE(uint8_t) disReadByteByOff(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off >= pCpu->cbCachedInstr))
        return disReadByteSlow(pCpu, off);

    return pCpu->abInstr[off];
}


/**
 * Read a byte (8-bit) instruction byte.
 *
 * @returns The requested byte.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECL_FORCE_INLINE(uint8_t) disReadByte(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    return disReadByteByOff(pCpu, uAddress - pCpu->uInstrAddr);
}


/**
 * Function for handling a 16-bit cache miss.
 *
 * @returns The requested word.
 * @param   pCpu                The disassembler state.
 * @param   off                 The offset of the word relative to the
 *                              instruction.
 */
DECL_NO_INLINE(static, uint16_t) disReadWordSlow(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off + 2 > DIS_MAX_INSTR_LENGTH))
    {
        Log(("disReadWord: too long instruction...\n"));
        pCpu->rc = VERR_DIS_TOO_LONG_INSTR;
        if (off < DIS_MAX_INSTR_LENGTH)
            return pCpu->abInstr[off];
        return 0;
    }

    if (off + 2 < pCpu->cbCachedInstr)
        disReadMore(pCpu, off, 2);

#ifdef DIS_HOST_UNALIGNED_ACCESS_OK
    return *(uint16_t const *)&pCpu->abInstr[off];
#else
    return RT_MAKE_U16(pCpu->abInstr[off], pCpu->abInstr[off + 1]);
#endif
}


/**
 * Read a word (16-bit) instruction byte by offset.
 *
 * @returns The requested word.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECLINLINE(uint16_t) disReadWordByOff(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off + 2 > pCpu->cbCachedInstr))
        return disReadWordSlow(pCpu, off);

#ifdef DIS_HOST_UNALIGNED_ACCESS_OK
    return *(uint16_t const *)&pCpu->abInstr[off];
#else
    return RT_MAKE_U16(pCpu->abInstr[off], pCpu->abInstr[off + 1]);
#endif
}


/**
 * Read a word (16-bit) instruction byte.
 *
 * @returns The requested word.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECL_FORCE_INLINE(uint16_t) disReadWord(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    return disReadWordByOff(pCpu, uAddress - pCpu->uInstrAddr);
}


/**
 * Function for handling a 32-bit cache miss.
 *
 * @returns The requested dword.
 * @param   pCpu                The disassembler state.
 * @param   off                 The offset of the dword relative to the
 *                              instruction.
 */
DECL_NO_INLINE(static, uint32_t) disReadDWordSlow(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off + 2 > DIS_MAX_INSTR_LENGTH))
    {
        Log(("disReadDWord: too long instruction...\n"));
        pCpu->rc = VERR_DIS_TOO_LONG_INSTR;
        switch ((RTUINTPTR)DIS_MAX_INSTR_LENGTH - off)
        {
            case 1:
                return RT_MAKE_U32_FROM_U8(pCpu->abInstr[off], 0, 0, 0);
            case 2:
                return RT_MAKE_U32_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], 0, 0);
            case 3:
                return RT_MAKE_U32_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], 0);
        }
        return 0;
    }

    if (off + 2 < pCpu->cbCachedInstr)
        disReadMore(pCpu, off, 2);

#ifdef DIS_HOST_UNALIGNED_ACCESS_OK
    return *(uint32_t const *)&pCpu->abInstr[off];
#else
    return RT_MAKE_U32_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3]);
#endif
}


/**
 * Read a dword (32-bit) instruction byte by offset.
 *
 * @returns The requested dword.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECLINLINE(uint32_t) disReadDWordByOff(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off + 2 > pCpu->cbCachedInstr))
        return disReadDWordSlow(pCpu, off);

#ifdef DIS_HOST_UNALIGNED_ACCESS_OK
    return *(uint32_t const *)&pCpu->abInstr[off];
#else
    return RT_MAKE_U32_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3]);
#endif
}


/**
 * Read a dword (32-bit) instruction byte.
 *
 * @returns The requested dword.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECL_FORCE_INLINE(uint32_t) disReadDWord(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    return disReadDWordByOff(pCpu, uAddress - pCpu->uInstrAddr);
}


/**
 * Function for handling a 64-bit cache miss.
 *
 * @returns The requested qword.
 * @param   pCpu                The disassembler state.
 * @param   off                 The offset of the qword relative to the
 *                              instruction.
 */
DECL_NO_INLINE(static, uint64_t) disReadQWordSlow(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off + 2 > DIS_MAX_INSTR_LENGTH))
    {
        Log(("disReadQWord: too long instruction...\n"));
        pCpu->rc = VERR_DIS_TOO_LONG_INSTR;
        switch ((RTUINTPTR)DIS_MAX_INSTR_LENGTH - off)
        {
            case 1:
                return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off], 0, 0, 0,   0, 0, 0, 0);
            case 2:
                return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], 0, 0,   0, 0, 0, 0);
            case 3:
                return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], 0,   0, 0, 0, 0);
            case 4:
                return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3],
                                           pCpu->abInstr[off + 4], 0, 0, 0);
            case 5:
                return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3],
                                           pCpu->abInstr[off + 4], pCpu->abInstr[off + 5], 0, 0);
            case 6:
                return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3],
                                           pCpu->abInstr[off + 4], pCpu->abInstr[off + 5], pCpu->abInstr[off + 6], 0);
        }
        return 0;
    }

    if (off + 2 < pCpu->cbCachedInstr)
        disReadMore(pCpu, off, 2);

#ifdef DIS_HOST_UNALIGNED_ACCESS_OK
    return *(uint64_t const *)&pCpu->abInstr[off];
#else
    return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off    ], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3],
                               pCpu->abInstr[off + 4], pCpu->abInstr[off + 5], pCpu->abInstr[off + 6], pCpu->abInstr[off + 7]);
#endif
}


/**
 * Read a qword (64-bit) instruction byte by offset.
 *
 * @returns The requested qword.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECLINLINE(uint64_t) disReadQWordByOff(PDISCPUSTATE pCpu, RTUINTPTR off)
{
    if (RT_UNLIKELY(off + 2 > pCpu->cbCachedInstr))
        return disReadQWordSlow(pCpu, off);

#ifdef DIS_HOST_UNALIGNED_ACCESS_OK
    return *(uint64_t const *)&pCpu->abInstr[off];
#else
    return RT_MAKE_U64_FROM_U8(pCpu->abInstr[off    ], pCpu->abInstr[off + 1], pCpu->abInstr[off + 2], pCpu->abInstr[off + 3],
                               pCpu->abInstr[off + 4], pCpu->abInstr[off + 5], pCpu->abInstr[off + 6], pCpu->abInstr[off + 7]);
#endif
}


/**
 * Read a qword (64-bit) instruction byte.
 *
 * @returns The requested qword.
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 */
DECL_FORCE_INLINE(uint64_t) disReadQWord(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    return disReadQWordByOff(pCpu, uAddress - pCpu->uInstrAddr);
}



//*****************************************************************************
//*****************************************************************************
static unsigned disParseInstruction(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISCPUSTATE pCpu)
{
    int  size = 0;
    bool fFiltered = false;

    Assert(uCodePtr && pOp && pCpu);

    // Store the opcode format string for disasmPrintf
    pCpu->pCurInstr = pOp;

    /*
     * Apply filter to instruction type to determine if a full disassembly is required.
     * Note! Multibyte opcodes are always marked harmless until the final byte.
     */
    if ((pOp->fOpType & pCpu->fFilter) == 0)
    {
        fFiltered = true;
        pCpu->pfnDisasmFnTable = g_apfnCalcSize;
    }
    else
    {
        /* Not filtered out -> full disassembly */
        pCpu->pfnDisasmFnTable = g_apfnFullDisasm;
    }

    // Should contain the parameter type on input
    pCpu->Param1.fParam = pOp->fParam1;
    pCpu->Param2.fParam = pOp->fParam2;
    pCpu->Param3.fParam = pOp->fParam3;

    /* Correct the operand size if the instruction is marked as forced or default 64 bits */
    if (pCpu->uCpuMode == DISCPUMODE_64BIT)
    {
        if (pOp->fOpType & DISOPTYPE_FORCED_64_OP_SIZE)
            pCpu->uOpMode = DISCPUMODE_64BIT;
        else
        if (    (pOp->fOpType & DISOPTYPE_DEFAULT_64_OP_SIZE)
            &&  !(pCpu->fPrefix & DISPREFIX_OPSIZE))
            pCpu->uOpMode = DISCPUMODE_64BIT;
    }
    else
    if (pOp->fOpType & DISOPTYPE_FORCED_32_OP_SIZE_X86)
    {
        /* Forced 32 bits operand size for certain instructions (mov crx, mov drx). */
        Assert(pCpu->uCpuMode != DISCPUMODE_64BIT);
        pCpu->uOpMode = DISCPUMODE_32BIT;
    }

    if (pOp->idxParse1 != IDX_ParseNop)
    {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse1](uCodePtr, pOp, &pCpu->Param1, pCpu);
        if (fFiltered == false) pCpu->Param1.cb = DISGetParamSize(pCpu, &pCpu->Param1);
    }

    if (pOp->idxParse2 != IDX_ParseNop)
    {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse2](uCodePtr+size, pOp, &pCpu->Param2, pCpu);
        if (fFiltered == false) pCpu->Param2.cb = DISGetParamSize(pCpu, &pCpu->Param2);
    }

    if (pOp->idxParse3 != IDX_ParseNop)
    {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse3](uCodePtr+size, pOp, &pCpu->Param3, pCpu);
        if (fFiltered == false) pCpu->Param3.cb = DISGetParamSize(pCpu, &pCpu->Param3);
    }
    // else simple one byte instruction

    return size;
}
//*****************************************************************************
/* Floating point opcode parsing */
//*****************************************************************************
static unsigned ParseEscFP(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    int index;
    PCDISOPCODE fpop;
    unsigned size = 0;
    unsigned ModRM;
    NOREF(pOp);

    ModRM = disReadByte(pCpu, uCodePtr);

    index = pCpu->bOpCode - 0xD8;
    if (ModRM <= 0xBF)
    {
        fpop            = &(g_apMapX86_FP_Low[index])[MODRM_REG(ModRM)];
        pCpu->pCurInstr = (PCDISOPCODE)fpop;

        // Should contain the parameter type on input
        pCpu->Param1.fParam = fpop->fParam1;
        pCpu->Param2.fParam = fpop->fParam2;
    }
    else
    {
        fpop            = &(g_apMapX86_FP_High[index])[ModRM - 0xC0];
        pCpu->pCurInstr = (PCDISOPCODE)fpop;
    }

    /*
     * Apply filter to instruction type to determine if a full disassembly is required.
     * @note Multibyte opcodes are always marked harmless until the final byte.
     */
    if ((fpop->fOpType & pCpu->fFilter) == 0)
        pCpu->pfnDisasmFnTable = g_apfnCalcSize;
    else
        /* Not filtered out -> full disassembly */
        pCpu->pfnDisasmFnTable = g_apfnFullDisasm;

    /* Correct the operand size if the instruction is marked as forced or default 64 bits */
    if (pCpu->uCpuMode == DISCPUMODE_64BIT)
    {
        /* Note: redundant, but just in case this ever changes */
        if (fpop->fOpType & DISOPTYPE_FORCED_64_OP_SIZE)
            pCpu->uOpMode = DISCPUMODE_64BIT;
        else
        if (    (fpop->fOpType & DISOPTYPE_DEFAULT_64_OP_SIZE)
            &&  !(pCpu->fPrefix & DISPREFIX_OPSIZE))
            pCpu->uOpMode = DISCPUMODE_64BIT;
    }

    // Little hack to make sure the ModRM byte is included in the returned size
    if (fpop->idxParse1 != IDX_ParseModRM && fpop->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    if (fpop->idxParse1 != IDX_ParseNop)
        size += pCpu->pfnDisasmFnTable[fpop->idxParse1](uCodePtr+size, (PCDISOPCODE)fpop, pParam, pCpu);

    if (fpop->idxParse2 != IDX_ParseNop)
        size += pCpu->pfnDisasmFnTable[fpop->idxParse2](uCodePtr+size, (PCDISOPCODE)fpop, pParam, pCpu);

    return size;
}
//*****************************************************************************
// SIB byte: (not 16-bit mode)
// 7 - 6  5 - 3  2-0
// Scale  Index  Base
//*****************************************************************************
static void UseSIB(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned regtype;
    NOREF(uCodePtr); NOREF(pOp);

    unsigned scale = pCpu->SIB.Bits.Scale;
    unsigned base  = pCpu->SIB.Bits.Base;
    unsigned index = pCpu->SIB.Bits.Index;

    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
        regtype    = DISUSE_REG_GEN32;
    else
        regtype    = DISUSE_REG_GEN64;

    if (index != 4)
    {
         pParam->fUse |= DISUSE_INDEX | regtype;
         pParam->Index.idxGenReg = index;

         if (scale != 0)
         {
             pParam->fUse  |= DISUSE_SCALE;
             pParam->uScale = (1<<scale);
         }
    }

    if (base == 5 && pCpu->ModRM.Bits.Mod == 0)
    {
        // [scaled index] + disp32
        if (pCpu->uAddrMode == DISCPUMODE_32BIT)
        {
            pParam->fUse |= DISUSE_DISPLACEMENT32;
            pParam->uDisp.i32 = pCpu->i32SibDisp;
        }
        else
        {   /* sign-extend to 64 bits */
            pParam->fUse |= DISUSE_DISPLACEMENT64;
            pParam->uDisp.i64 = pCpu->i32SibDisp;
        }
    }
    else
    {
        pParam->fUse |= DISUSE_BASE | regtype;
        pParam->Base.idxGenReg = base;
    }
    return;   /* Already fetched everything in ParseSIB; no size returned */
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseSIB(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);
    unsigned SIB;
    NOREF(pOp); NOREF(pParam);

    SIB = disReadByte(pCpu, uCodePtr);
    uCodePtr += size;

    pCpu->SIB.Bits.Base  = SIB_BASE(SIB);
    pCpu->SIB.Bits.Index = SIB_INDEX(SIB);
    pCpu->SIB.Bits.Scale = SIB_SCALE(SIB);

    if (pCpu->fPrefix & DISPREFIX_REX)
    {
        /* REX.B extends the Base field if not scaled index + disp32 */
        if (!(pCpu->SIB.Bits.Base == 5 && pCpu->ModRM.Bits.Mod == 0))
            pCpu->SIB.Bits.Base  |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_B)) << 3);

        pCpu->SIB.Bits.Index |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_X)) << 3);
    }

    if (    pCpu->SIB.Bits.Base == 5
        &&  pCpu->ModRM.Bits.Mod == 0)
    {
        /* Additional 32 bits displacement. No change in long mode. */
        pCpu->i32SibDisp = disReadDWord(pCpu, uCodePtr);
        size += sizeof(int32_t);
    }
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseSIB_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);
    unsigned SIB;
    NOREF(pOp); NOREF(pParam);

    SIB = disReadByte(pCpu, uCodePtr);
    uCodePtr += size;

    pCpu->SIB.Bits.Base  = SIB_BASE(SIB);
    pCpu->SIB.Bits.Index = SIB_INDEX(SIB);
    pCpu->SIB.Bits.Scale = SIB_SCALE(SIB);

    if (pCpu->fPrefix & DISPREFIX_REX)
    {
        /* REX.B extends the Base field. */
        pCpu->SIB.Bits.Base  |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_B)) << 3);
        /* REX.X extends the Index field. */
        pCpu->SIB.Bits.Index |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_X)) << 3);
    }

    if (    pCpu->SIB.Bits.Base == 5
        &&  pCpu->ModRM.Bits.Mod == 0)
    {
        /* Additional 32 bits displacement. No change in long mode. */
        size += sizeof(int32_t);
    }
    return size;
}
//*****************************************************************************
// ModR/M byte:
// 7 - 6  5 - 3       2-0
// Mod    Reg/Opcode  R/M
//*****************************************************************************
static unsigned UseModRM(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned vtype = OP_PARM_VTYPE(pParam->fParam);
    unsigned reg = pCpu->ModRM.Bits.Reg;
    unsigned mod = pCpu->ModRM.Bits.Mod;
    unsigned rm  = pCpu->ModRM.Bits.Rm;

    switch (vtype)
    {
    case OP_PARM_G: //general purpose register
        disasmModRMReg(pCpu, pOp, reg, pParam, 0);
        return 0;

    default:
        if (IS_OP_PARM_RARE(vtype))
        {
            switch (vtype)
            {
            case OP_PARM_C: //control register
                pParam->fUse |= DISUSE_REG_CR;

                if (    pCpu->pCurInstr->uOpcode == OP_MOV_CR
                    &&  pCpu->uOpMode == DISCPUMODE_32BIT
                    &&  (pCpu->fPrefix & DISPREFIX_LOCK))
                {
                    pCpu->fPrefix &= ~DISPREFIX_LOCK;
                    pParam->Base.idxCtrlReg = DISCREG_CR8;
                }
                else
                    pParam->Base.idxCtrlReg = reg;
                return 0;

            case OP_PARM_D: //debug register
                pParam->fUse |= DISUSE_REG_DBG;
                pParam->Base.idxDbgReg = reg;
                return 0;

            case OP_PARM_P: //MMX register
                reg &= 7;   /* REX.R has no effect here */
                pParam->fUse |= DISUSE_REG_MMX;
                pParam->Base.idxMmxReg = reg;
                return 0;

            case OP_PARM_S: //segment register
                reg &= 7;   /* REX.R has no effect here */
                disasmModRMSReg(pCpu, pOp, reg, pParam);
                pParam->fUse |= DISUSE_REG_SEG;
                return 0;

            case OP_PARM_T: //test register
                reg &= 7;   /* REX.R has no effect here */
                pParam->fUse |= DISUSE_REG_TEST;
                pParam->Base.idxTestReg = reg;
                return 0;

            case OP_PARM_W: //XMM register or memory operand
                if (mod != 3)
                    break;  /* memory operand */
                reg = rm; /* the RM field specifies the xmm register */
                /* else no break */

            case OP_PARM_V: //XMM register
                pParam->fUse |= DISUSE_REG_XMM;
                pParam->Base.idxXmmReg = reg;
                return 0;
            }
        }
    }

    /* @todo bound */

    if (pCpu->uAddrMode != DISCPUMODE_16BIT)
    {
        Assert(pCpu->uAddrMode == DISCPUMODE_32BIT || pCpu->uAddrMode == DISCPUMODE_64BIT);

        /*
         * Note: displacements in long mode are 8 or 32 bits and sign-extended to 64 bits
         */
        switch (mod)
        {
        case 0: //effective address
            if (rm == 4)
            {   /* SIB byte follows ModRM */
                UseSIB(uCodePtr, pOp, pParam, pCpu);
            }
            else
            if (rm == 5)
            {
                /* 32 bits displacement */
                if (pCpu->uCpuMode != DISCPUMODE_64BIT)
                {
                    pParam->fUse |= DISUSE_DISPLACEMENT32;
                    pParam->uDisp.i32 = pCpu->i32SibDisp;
                }
                else
                {
                    pParam->fUse |= DISUSE_RIPDISPLACEMENT32;
                    pParam->uDisp.i32 = pCpu->i32SibDisp;
                }
            }
            else
            {   //register address
                pParam->fUse |= DISUSE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            break;

        case 1: //effective address + 8 bits displacement
            if (rm == 4) {//SIB byte follows ModRM
                UseSIB(uCodePtr, pOp, pParam, pCpu);
            }
            else
            {
                pParam->fUse |= DISUSE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            pParam->uDisp.i8 = pCpu->i32SibDisp;
            pParam->fUse |= DISUSE_DISPLACEMENT8;
            break;

        case 2: //effective address + 32 bits displacement
            if (rm == 4) {//SIB byte follows ModRM
                UseSIB(uCodePtr, pOp, pParam, pCpu);
            }
            else
            {
                pParam->fUse |= DISUSE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            pParam->uDisp.i32 = pCpu->i32SibDisp;
            pParam->fUse |= DISUSE_DISPLACEMENT32;
            break;

        case 3: //registers
            disasmModRMReg(pCpu, pOp, rm, pParam, 0);
            break;
        }
    }
    else
    {//16 bits addressing mode
        switch (mod)
        {
        case 0: //effective address
            if (rm == 6)
            {//16 bits displacement
                pParam->uDisp.i16 = pCpu->i32SibDisp;
                pParam->fUse |= DISUSE_DISPLACEMENT16;
            }
            else
            {
                pParam->fUse |= DISUSE_BASE;
                disasmModRMReg16(pCpu, pOp, rm, pParam);
            }
            break;

        case 1: //effective address + 8 bits displacement
            disasmModRMReg16(pCpu, pOp, rm, pParam);
            pParam->uDisp.i8 = pCpu->i32SibDisp;
            pParam->fUse |= DISUSE_BASE | DISUSE_DISPLACEMENT8;
            break;

        case 2: //effective address + 16 bits displacement
            disasmModRMReg16(pCpu, pOp, rm, pParam);
            pParam->uDisp.i16 = pCpu->i32SibDisp;
            pParam->fUse |= DISUSE_BASE | DISUSE_DISPLACEMENT16;
            break;

        case 3: //registers
            disasmModRMReg(pCpu, pOp, rm, pParam, 0);
            break;
        }
    }
    return 0;   //everything was already fetched in ParseModRM
}
//*****************************************************************************
// Query the size of the ModRM parameters and fetch the immediate data (if any)
//*****************************************************************************
static unsigned QueryModRM(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu, unsigned *pSibInc)
{
    unsigned sibinc;
    unsigned size = 0;
    // unsigned reg = pCpu->ModRM.Bits.Reg;
    unsigned mod = pCpu->ModRM.Bits.Mod;
    unsigned rm  = pCpu->ModRM.Bits.Rm;

    if (!pSibInc)
        pSibInc = &sibinc;

    *pSibInc = 0;

    if (pCpu->uAddrMode != DISCPUMODE_16BIT)
    {
        Assert(pCpu->uAddrMode == DISCPUMODE_32BIT || pCpu->uAddrMode == DISCPUMODE_64BIT);

        /*
         * Note: displacements in long mode are 8 or 32 bits and sign-extended to 64 bits
         */
        if (mod != 3 && rm == 4)
        {   /* SIB byte follows ModRM */
            *pSibInc = ParseSIB(uCodePtr, pOp, pParam, pCpu);
            uCodePtr += *pSibInc;
            size += *pSibInc;
        }

        switch (mod)
        {
        case 0: /* Effective address */
            if (rm == 5) {  /* 32 bits displacement */
                pCpu->i32SibDisp = disReadDWord(pCpu, uCodePtr);
                size += sizeof(int32_t);
            }
            /* else register address */
            break;

        case 1: /* Effective address + 8 bits displacement */
            pCpu->i32SibDisp = (int8_t)disReadByte(pCpu, uCodePtr);
            size += sizeof(char);
            break;

        case 2: /* Effective address + 32 bits displacement */
            pCpu->i32SibDisp = disReadDWord(pCpu, uCodePtr);
            size += sizeof(int32_t);
            break;

        case 3: /* registers */
            break;
        }
    }
    else
    {
        /* 16 bits mode */
        switch (mod)
        {
        case 0: /* Effective address */
            if (rm == 6) {
                pCpu->i32SibDisp = disReadWord(pCpu, uCodePtr);
                size += sizeof(uint16_t);
            }
            /* else register address */
            break;

        case 1: /* Effective address + 8 bits displacement */
            pCpu->i32SibDisp = (int8_t)disReadByte(pCpu, uCodePtr);
            size += sizeof(char);
            break;

        case 2: /* Effective address + 32 bits displacement */
            pCpu->i32SibDisp = (int16_t)disReadWord(pCpu, uCodePtr);
            size += sizeof(uint16_t);
            break;

        case 3: /* registers */
            break;
        }
    }
    return size;
}
//*****************************************************************************
// Query the size of the ModRM parameters and fetch the immediate data (if any)
//*****************************************************************************
static unsigned QueryModRM_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu, unsigned *pSibInc)
{
    unsigned sibinc;
    unsigned size = 0;
    // unsigned reg = pCpu->ModRM.Bits.Reg;
    unsigned mod = pCpu->ModRM.Bits.Mod;
    unsigned rm  = pCpu->ModRM.Bits.Rm;

    if (!pSibInc)
        pSibInc = &sibinc;

    *pSibInc = 0;

    if (pCpu->uAddrMode != DISCPUMODE_16BIT)
    {
        Assert(pCpu->uAddrMode == DISCPUMODE_32BIT || pCpu->uAddrMode == DISCPUMODE_64BIT);
        /*
         * Note: displacements in long mode are 8 or 32 bits and sign-extended to 64 bits
         */
        if (mod != 3 && rm == 4)
        {   /* SIB byte follows ModRM */
            *pSibInc = ParseSIB_SizeOnly(uCodePtr, pOp, pParam, pCpu);
            uCodePtr += *pSibInc;
            size += *pSibInc;
        }

        switch (mod)
        {
        case 0: //effective address
            if (rm == 5) {  /* 32 bits displacement */
                size += sizeof(int32_t);
            }
            /* else register address */
            break;

        case 1: /* Effective address + 8 bits displacement */
            size += sizeof(char);
            break;

        case 2: /* Effective address + 32 bits displacement */
            size += sizeof(int32_t);
            break;

        case 3: /* registers */
            break;
        }
    }
    else
    {
        /* 16 bits mode */
        switch (mod)
        {
        case 0: //effective address
            if (rm == 6) {
                size += sizeof(uint16_t);
            }
            /* else register address */
            break;

        case 1: /* Effective address + 8 bits displacement */
            size += sizeof(char);
            break;

        case 2: /* Effective address + 32 bits displacement */
            size += sizeof(uint16_t);
            break;

        case 3: /* registers */
            break;
        }
    }
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseIllegal(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    AssertFailed();
    return 0;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseModRM(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);   //ModRM byte
    unsigned sibinc, ModRM;

    ModRM = disReadByte(pCpu, uCodePtr);
    uCodePtr += sizeof(uint8_t);

    pCpu->ModRM.Bits.Rm  = MODRM_RM(ModRM);
    pCpu->ModRM.Bits.Mod = MODRM_MOD(ModRM);
    pCpu->ModRM.Bits.Reg = MODRM_REG(ModRM);

    /* Disregard the mod bits for certain instructions (mov crx, mov drx).
     *
     * From the AMD manual:
     * This instruction is always treated as a register-to-register (MOD = 11) instruction, regardless of the
     * encoding of the MOD field in the MODR/M byte.
     */
    if (pOp->fOpType & DISOPTYPE_MOD_FIXED_11)
        pCpu->ModRM.Bits.Mod = 3;

    if (pCpu->fPrefix & DISPREFIX_REX)
    {
        Assert(pCpu->uCpuMode == DISCPUMODE_64BIT);

        /* REX.R extends the Reg field. */
        pCpu->ModRM.Bits.Reg |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_R)) << 3);

        /* REX.B extends the Rm field if there is no SIB byte nor a 32 bits displacement */
        if (!(    pCpu->ModRM.Bits.Mod != 3
              &&  pCpu->ModRM.Bits.Rm  == 4)
            &&
            !(    pCpu->ModRM.Bits.Mod == 0
              &&  pCpu->ModRM.Bits.Rm  == 5))
        {
            pCpu->ModRM.Bits.Rm |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_B)) << 3);
        }
    }
    size += QueryModRM(uCodePtr, pOp, pParam, pCpu, &sibinc);
    uCodePtr += sibinc;

    UseModRM(uCodePtr, pOp, pParam, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseModRM_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);   //ModRM byte
    unsigned sibinc, ModRM;

    ModRM = disReadByte(pCpu, uCodePtr);
    uCodePtr += sizeof(uint8_t);

    pCpu->ModRM.Bits.Rm  = MODRM_RM(ModRM);
    pCpu->ModRM.Bits.Mod = MODRM_MOD(ModRM);
    pCpu->ModRM.Bits.Reg = MODRM_REG(ModRM);

    /* Disregard the mod bits for certain instructions (mov crx, mov drx).
     *
     * From the AMD manual:
     * This instruction is always treated as a register-to-register (MOD = 11) instruction, regardless of the
     * encoding of the MOD field in the MODR/M byte.
     */
    if (pOp->fOpType & DISOPTYPE_MOD_FIXED_11)
        pCpu->ModRM.Bits.Mod = 3;

    if (pCpu->fPrefix & DISPREFIX_REX)
    {
        Assert(pCpu->uCpuMode == DISCPUMODE_64BIT);

        /* REX.R extends the Reg field. */
        pCpu->ModRM.Bits.Reg |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_R)) << 3);

        /* REX.B extends the Rm field if there is no SIB byte nor a 32 bits displacement */
        if (!(    pCpu->ModRM.Bits.Mod != 3
              &&  pCpu->ModRM.Bits.Rm  == 4)
            &&
            !(    pCpu->ModRM.Bits.Mod == 0
              &&  pCpu->ModRM.Bits.Rm  == 5))
        {
            pCpu->ModRM.Bits.Rm |= ((!!(pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_B)) << 3);
        }
    }

    size += QueryModRM_SizeOnly(uCodePtr, pOp, pParam, pCpu, &sibinc);
    uCodePtr += sibinc;

    /* UseModRM is not necessary here; we're only interested in the opcode size */
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseModFence(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    ////AssertMsgFailed(("??\n"));
    //nothing to do apparently
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return 0;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmByte(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->uValue = disReadByte(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE8;
    pParam->cb     = sizeof(uint8_t);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmByte_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmByteSX(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    if (pCpu->uOpMode == DISCPUMODE_32BIT)
    {
        pParam->uValue = (uint32_t)(int8_t)disReadByte(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE32_SX8;
        pParam->cb     = sizeof(uint32_t);
    }
    else
    if (pCpu->uOpMode == DISCPUMODE_64BIT)
    {
        pParam->uValue = (uint64_t)(int8_t)disReadByte(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE64_SX8;
        pParam->cb     = sizeof(uint64_t);
    }
    else
    {
        pParam->uValue = (uint16_t)(int8_t)disReadByte(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE16_SX8;
        pParam->cb     = sizeof(uint16_t);
    }
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmByteSX_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmUshort(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->uValue = disReadWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE16;
    pParam->cb     = sizeof(uint16_t);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmUshort_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmUlong(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->uValue = disReadDWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE32;
    pParam->cb     = sizeof(uint32_t);
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmUlong_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmQword(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->uValue = disReadQWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE64;
    pParam->cb     = sizeof(uint64_t);
    return sizeof(uint64_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmQword_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint64_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmV(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    if (pCpu->uOpMode == DISCPUMODE_32BIT)
    {
        pParam->uValue = disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE32;
        pParam->cb     = sizeof(uint32_t);
        return sizeof(uint32_t);
    }

    if (pCpu->uOpMode == DISCPUMODE_64BIT)
    {
        pParam->uValue = disReadQWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE64;
        pParam->cb     = sizeof(uint64_t);
        return sizeof(uint64_t);
    }

    pParam->uValue = disReadWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE16;
    pParam->cb     = sizeof(uint16_t);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmV_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam);
    if (pCpu->uOpMode == DISCPUMODE_32BIT)
        return sizeof(uint32_t);
    if (pCpu->uOpMode == DISCPUMODE_64BIT)
        return sizeof(uint64_t);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmZ(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    /* Word for 16-bit operand-size or doubleword for 32 or 64-bit operand-size. */
    if (pCpu->uOpMode == DISCPUMODE_16BIT)
    {
        pParam->uValue = disReadWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE16;
        pParam->cb     = sizeof(uint16_t);
        return sizeof(uint16_t);
    }

    /* 64 bits op mode means *sign* extend to 64 bits. */
    if (pCpu->uOpMode == DISCPUMODE_64BIT)
    {
        pParam->uValue = (uint64_t)(int32_t)disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE64;
        pParam->cb     = sizeof(uint64_t);
    }
    else
    {
        pParam->uValue = disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE32;
        pParam->cb     = sizeof(uint32_t);
    }
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmZ_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam);
    /* Word for 16-bit operand-size or doubleword for 32 or 64-bit operand-size. */
    if (pCpu->uOpMode == DISCPUMODE_16BIT)
        return sizeof(uint16_t);
    return sizeof(uint32_t);
}

//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
static unsigned ParseImmBRel(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->uValue = disReadByte(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE8_REL;
    pParam->cb     = sizeof(uint8_t);
    return sizeof(char);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
static unsigned ParseImmBRel_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(char);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
static unsigned ParseImmVRel(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    if (pCpu->uOpMode == DISCPUMODE_32BIT)
    {
        pParam->uValue = disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE32_REL;
        pParam->cb     = sizeof(int32_t);
        return sizeof(int32_t);
    }

    if (pCpu->uOpMode == DISCPUMODE_64BIT)
    {
        /* 32 bits relative immediate sign extended to 64 bits. */
        pParam->uValue = (uint64_t)(int32_t)disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE64_REL;
        pParam->cb     = sizeof(int64_t);
        return sizeof(int32_t);
    }

    pParam->uValue = disReadWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE16_REL;
    pParam->cb     = sizeof(int16_t);
    return sizeof(int16_t);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
static unsigned ParseImmVRel_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp); NOREF(pParam);
    if (pCpu->uOpMode == DISCPUMODE_16BIT)
        return sizeof(int16_t);
    /* Both 32 & 64 bits mode use 32 bits relative immediates. */
    return sizeof(int32_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmAddr(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
    {
        if (OP_PARM_VSUBTYPE(pParam->fParam) == OP_PARM_p)
        {
            /* far 16:32 pointer */
            pParam->uValue = disReadDWord(pCpu, uCodePtr);
            *((uint32_t*)&pParam->uValue+1) = disReadWord(pCpu, uCodePtr+sizeof(uint32_t));
            pParam->fUse   |= DISUSE_IMMEDIATE_ADDR_16_32;
            pParam->cb     = sizeof(uint16_t) + sizeof(uint32_t);
            return sizeof(uint32_t) + sizeof(uint16_t);
        }

        /*
         * near 32 bits pointer
         *
         * Note: used only in "mov al|ax|eax, [Addr]" and "mov [Addr], al|ax|eax"
         * so we treat it like displacement.
         */
        pParam->uDisp.i32 = disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_DISPLACEMENT32;
        pParam->cb     = sizeof(uint32_t);
        return sizeof(uint32_t);
    }

    if (pCpu->uAddrMode == DISCPUMODE_64BIT)
    {
        Assert(OP_PARM_VSUBTYPE(pParam->fParam) != OP_PARM_p);
        /*
         * near 64 bits pointer
         *
         * Note: used only in "mov al|ax|eax, [Addr]" and "mov [Addr], al|ax|eax"
         * so we treat it like displacement.
         */
        pParam->uDisp.i64 = disReadQWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_DISPLACEMENT64;
        pParam->cb     = sizeof(uint64_t);
        return sizeof(uint64_t);
    }
    if (OP_PARM_VSUBTYPE(pParam->fParam) == OP_PARM_p)
    {
        /* far 16:16 pointer */
        pParam->uValue = disReadDWord(pCpu, uCodePtr);
        pParam->fUse  |= DISUSE_IMMEDIATE_ADDR_16_16;
        pParam->cb     = 2*sizeof(uint16_t);
        return sizeof(uint32_t);
    }

    /*
     * near 16 bits pointer
     *
     * Note: used only in "mov al|ax|eax, [Addr]" and "mov [Addr], al|ax|eax"
     * so we treat it like displacement.
     */
    pParam->uDisp.i16 = disReadWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_DISPLACEMENT16;
    pParam->cb     = sizeof(uint16_t);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmAddr_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp);
    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
    {
        if (OP_PARM_VSUBTYPE(pParam->fParam) == OP_PARM_p)
        {// far 16:32 pointer
            return sizeof(uint32_t) + sizeof(uint16_t);
        }
        else
        {// near 32 bits pointer
            return sizeof(uint32_t);
        }
    }
    if (pCpu->uAddrMode == DISCPUMODE_64BIT)
    {
        Assert(OP_PARM_VSUBTYPE(pParam->fParam) != OP_PARM_p);
        return sizeof(uint64_t);
    }
    else
    {
        if (OP_PARM_VSUBTYPE(pParam->fParam) == OP_PARM_p)
        {// far 16:16 pointer
            return sizeof(uint32_t);
        }
        else
        {// near 16 bits pointer
            return sizeof(uint16_t);
        }
    }
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmAddrF(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    // immediate far pointers - only 16:16 or 16:32; determined by operand, *not* address size!
    Assert(pCpu->uOpMode == DISCPUMODE_16BIT || pCpu->uOpMode == DISCPUMODE_32BIT);
    Assert(OP_PARM_VSUBTYPE(pParam->fParam) == OP_PARM_p);
    if (pCpu->uOpMode == DISCPUMODE_32BIT)
    {
        // far 16:32 pointer
        pParam->uValue = disReadDWord(pCpu, uCodePtr);
        *((uint32_t*)&pParam->uValue+1) = disReadWord(pCpu, uCodePtr+sizeof(uint32_t));
        pParam->fUse   |= DISUSE_IMMEDIATE_ADDR_16_32;
        pParam->cb     = sizeof(uint16_t) + sizeof(uint32_t);
        return sizeof(uint32_t) + sizeof(uint16_t);
    }

    // far 16:16 pointer
    pParam->uValue = disReadDWord(pCpu, uCodePtr);
    pParam->fUse  |= DISUSE_IMMEDIATE_ADDR_16_16;
    pParam->cb     = 2*sizeof(uint16_t);
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmAddrF_SizeOnly(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp);
    // immediate far pointers - only 16:16 or 16:32
    Assert(pCpu->uOpMode == DISCPUMODE_16BIT || pCpu->uOpMode == DISCPUMODE_32BIT);
    Assert(OP_PARM_VSUBTYPE(pParam->fParam) == OP_PARM_p);
    if (pCpu->uOpMode == DISCPUMODE_32BIT)
    {
        // far 16:32 pointer
        return sizeof(uint32_t) + sizeof(uint16_t);
    }
    else
    {
        // far 16:16 pointer
        return sizeof(uint32_t);
    }
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseFixedReg(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr);

    /*
     * Sets up flags for stored in OPC fixed registers.
     */

    if (pParam->fParam == OP_PARM_NONE)
    {
        /* No parameter at all. */
        return 0;
    }

    AssertCompile(OP_PARM_REG_GEN32_END < OP_PARM_REG_SEG_END);
    AssertCompile(OP_PARM_REG_SEG_END < OP_PARM_REG_GEN16_END);
    AssertCompile(OP_PARM_REG_GEN16_END < OP_PARM_REG_GEN8_END);
    AssertCompile(OP_PARM_REG_GEN8_END < OP_PARM_REG_FP_END);

    if (pParam->fParam <= OP_PARM_REG_GEN32_END)
    {
        /* 32-bit EAX..EDI registers. */
        if (pCpu->uOpMode == DISCPUMODE_32BIT)
        {
            /* Use 32-bit registers. */
            pParam->Base.idxGenReg = pParam->fParam - OP_PARM_REG_GEN32_START;
            pParam->fUse  |= DISUSE_REG_GEN32;
            pParam->cb     = 4;
        }
        else
        if (pCpu->uOpMode == DISCPUMODE_64BIT)
        {
            /* Use 64-bit registers. */
            pParam->Base.idxGenReg = pParam->fParam - OP_PARM_REG_GEN32_START;
            if (    (pOp->fOpType & DISOPTYPE_REXB_EXTENDS_OPREG)
                &&  pParam == &pCpu->Param1             /* ugly assumption that it only applies to the first parameter */
                &&  (pCpu->fPrefix & DISPREFIX_REX)
                &&  (pCpu->fRexPrefix & DISPREFIX_REX_FLAGS))
                pParam->Base.idxGenReg += 8;

            pParam->fUse  |= DISUSE_REG_GEN64;
            pParam->cb     = 8;
        }
        else
        {
            /* Use 16-bit registers. */
            pParam->Base.idxGenReg = pParam->fParam - OP_PARM_REG_GEN32_START;
            pParam->fUse  |= DISUSE_REG_GEN16;
            pParam->cb     = 2;
            pParam->fParam = pParam->fParam - OP_PARM_REG_GEN32_START + OP_PARM_REG_GEN16_START;
        }
    }
    else
    if (pParam->fParam <= OP_PARM_REG_SEG_END)
    {
        /* Segment ES..GS registers. */
        pParam->Base.idxSegReg = (DISSELREG)(pParam->fParam - OP_PARM_REG_SEG_START);
        pParam->fUse  |= DISUSE_REG_SEG;
        pParam->cb     = 2;
    }
    else
    if (pParam->fParam <= OP_PARM_REG_GEN16_END)
    {
        /* 16-bit AX..DI registers. */
        pParam->Base.idxGenReg = pParam->fParam - OP_PARM_REG_GEN16_START;
        pParam->fUse  |= DISUSE_REG_GEN16;
        pParam->cb     = 2;
    }
    else
    if (pParam->fParam <= OP_PARM_REG_GEN8_END)
    {
        /* 8-bit AL..DL, AH..DH registers. */
        pParam->Base.idxGenReg = pParam->fParam - OP_PARM_REG_GEN8_START;
        pParam->fUse  |= DISUSE_REG_GEN8;
        pParam->cb     = 1;

        if (pCpu->uOpMode == DISCPUMODE_64BIT)
        {
            if (    (pOp->fOpType & DISOPTYPE_REXB_EXTENDS_OPREG)
                &&  pParam == &pCpu->Param1             /* ugly assumption that it only applies to the first parameter */
                &&  (pCpu->fPrefix & DISPREFIX_REX)
                &&  (pCpu->fRexPrefix & DISPREFIX_REX_FLAGS))
                pParam->Base.idxGenReg += 8;              /* least significant byte of R8-R15 */
        }
    }
    else
    if (pParam->fParam <= OP_PARM_REG_FP_END)
    {
        /* FPU registers. */
        pParam->Base.idxFpuReg = pParam->fParam - OP_PARM_REG_FP_START;
        pParam->fUse  |= DISUSE_REG_FP;
        pParam->cb     = 10;
    }
    Assert(!(pParam->fParam >= OP_PARM_REG_GEN64_START && pParam->fParam <= OP_PARM_REG_GEN64_END));

    /* else - not supported for now registers. */

    return 0;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseXv(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr);

    pParam->fUse |= DISUSE_POINTER_DS_BASED;
    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
    {
        pParam->Base.idxGenReg = DISGREG_ESI;
        pParam->fUse |= DISUSE_REG_GEN32;
    }
    else
    if (pCpu->uAddrMode == DISCPUMODE_64BIT)
    {
        pParam->Base.idxGenReg = DISGREG_RSI;
        pParam->fUse |= DISUSE_REG_GEN64;
    }
    else
    {
        pParam->Base.idxGenReg = DISGREG_SI;
        pParam->fUse |= DISUSE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseXb(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp);

    pParam->fUse |= DISUSE_POINTER_DS_BASED;
    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
    {
        pParam->Base.idxGenReg = DISGREG_ESI;
        pParam->fUse |= DISUSE_REG_GEN32;
    }
    else
    if (pCpu->uAddrMode == DISCPUMODE_64BIT)
    {
        pParam->Base.idxGenReg = DISGREG_RSI;
        pParam->fUse |= DISUSE_REG_GEN64;
    }
    else
    {
        pParam->Base.idxGenReg = DISGREG_SI;
        pParam->fUse |= DISUSE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseYv(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr);

    pParam->fUse |= DISUSE_POINTER_ES_BASED;
    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
    {
        pParam->Base.idxGenReg = DISGREG_EDI;
        pParam->fUse |= DISUSE_REG_GEN32;
    }
    else
    if (pCpu->uAddrMode == DISCPUMODE_64BIT)
    {
        pParam->Base.idxGenReg = DISGREG_RDI;
        pParam->fUse |= DISUSE_REG_GEN64;
    }
    else
    {
        pParam->Base.idxGenReg = DISGREG_DI;
        pParam->fUse |= DISUSE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseYb(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    NOREF(uCodePtr); NOREF(pOp);

    pParam->fUse |= DISUSE_POINTER_ES_BASED;
    if (pCpu->uAddrMode == DISCPUMODE_32BIT)
    {
        pParam->Base.idxGenReg = DISGREG_EDI;
        pParam->fUse |= DISUSE_REG_GEN32;
    }
    else
    if (pCpu->uAddrMode == DISCPUMODE_64BIT)
    {
        pParam->Base.idxGenReg = DISGREG_RDI;
        pParam->fUse |= DISUSE_REG_GEN64;
    }
    else
    {
        pParam->Base.idxGenReg = DISGREG_DI;
        pParam->fUse |= DISUSE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseTwoByteEsc(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    PCDISOPCODE   pOpcode;
    int           size    = sizeof(uint8_t);
    NOREF(pOp); NOREF(pParam);

    /* 2nd byte */
    pCpu->bOpCode = disReadByte(pCpu, uCodePtr);

    /* default to the non-prefixed table. */
    pOpcode      = &g_aTwoByteMapX86[pCpu->bOpCode];

    /* Handle opcode table extensions that rely on the address, repe or repne prefix byte.  */
    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    if (pCpu->bLastPrefix)
    {
        switch (pCpu->bLastPrefix)
        {
        case OP_OPSIZE: /* 0x66 */
            if (g_aTwoByteMapX86_PF66[pCpu->bOpCode].uOpcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */
                pOpcode = &g_aTwoByteMapX86_PF66[pCpu->bOpCode];

                /* Cancel prefix changes. */
                pCpu->fPrefix &= ~DISPREFIX_OPSIZE;
                pCpu->uOpMode  = pCpu->uCpuMode;
            }
            break;

        case OP_REPNE:   /* 0xF2 */
            if (g_aTwoByteMapX86_PFF2[pCpu->bOpCode].uOpcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */
                pOpcode = &g_aTwoByteMapX86_PFF2[pCpu->bOpCode];

                /* Cancel prefix changes. */
                pCpu->fPrefix &= ~DISPREFIX_REPNE;
            }
            break;

        case OP_REPE:  /* 0xF3 */
            if (g_aTwoByteMapX86_PFF3[pCpu->bOpCode].uOpcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */
                pOpcode = &g_aTwoByteMapX86_PFF3[pCpu->bOpCode];

                /* Cancel prefix changes. */
                pCpu->fPrefix &= ~DISPREFIX_REP;
            }
            break;
        }
    }

    size += disParseInstruction(uCodePtr+size, pOpcode, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseThreeByteEsc4(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    PCDISOPCODE   pOpcode;
    int           size    = sizeof(uint8_t);
    NOREF(pOp); NOREF(pParam);

    /* 3rd byte */
    pCpu->bOpCode = disReadByte(pCpu, uCodePtr);

    /* default to the non-prefixed table. */
    if (g_apThreeByteMapX86_0F38[pCpu->bOpCode >> 4])
    {
        pOpcode = g_apThreeByteMapX86_0F38[pCpu->bOpCode >> 4];
        pOpcode = &pOpcode[pCpu->bOpCode & 0xf];
    }
    else
        pOpcode = &g_InvalidOpcode[0];

    /* Handle opcode table extensions that rely on the address, repne prefix byte.  */
    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    switch (pCpu->bLastPrefix)
    {
    case OP_OPSIZE: /* 0x66 */
        if (g_apThreeByteMapX86_660F38[pCpu->bOpCode >> 4])
        {
            pOpcode = g_apThreeByteMapX86_660F38[pCpu->bOpCode >> 4];
            pOpcode = &pOpcode[pCpu->bOpCode & 0xf];

            if (pOpcode->uOpcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */

                /* Cancel prefix changes. */
                pCpu->fPrefix &= ~DISPREFIX_OPSIZE;
                pCpu->uOpMode  = pCpu->uCpuMode;
            }
        }
        break;

    case OP_REPNE:   /* 0xF2 */
        if (g_apThreeByteMapX86_F20F38[pCpu->bOpCode >> 4])
        {
            pOpcode = g_apThreeByteMapX86_F20F38[pCpu->bOpCode >> 4];
            pOpcode = &pOpcode[pCpu->bOpCode & 0xf];

            if (pOpcode->uOpcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */

                /* Cancel prefix changes. */
                pCpu->fPrefix &= ~DISPREFIX_REPNE;
            }
        }
        break;
    }

    size += disParseInstruction(uCodePtr+size, pOpcode, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseThreeByteEsc5(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    PCDISOPCODE   pOpcode;
    int           size    = sizeof(uint8_t);
    NOREF(pOp); NOREF(pParam);

    /* 3rd byte */
    pCpu->bOpCode = disReadByte(pCpu, uCodePtr);

    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    Assert(pCpu->bLastPrefix == OP_OPSIZE);

    /* default to the non-prefixed table. */
    if (g_apThreeByteMapX86_660F3A[pCpu->bOpCode >> 4])
    {
        pOpcode = g_apThreeByteMapX86_660F3A[pCpu->bOpCode >> 4];
        pOpcode = &pOpcode[pCpu->bOpCode & 0xf];

        if (pOpcode->uOpcode != OP_INVALID)
        {
            /* Table entry is valid, so use the extension table. */

            /* Cancel prefix changes. */
            pCpu->fPrefix &= ~DISPREFIX_OPSIZE;
            pCpu->uOpMode  = pCpu->uCpuMode;
        }
    }
    else
        pOpcode = &g_InvalidOpcode[0];

    size += disParseInstruction(uCodePtr+size, pOpcode, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseNopPause(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0;
    NOREF(pParam);

    if (pCpu->fPrefix & DISPREFIX_REP)
    {
        pOp = &g_aMapX86_NopPause[1]; /* PAUSE */
        pCpu->fPrefix &= ~DISPREFIX_REP;
    }
    else
        pOp = &g_aMapX86_NopPause[0]; /* NOP */

    size += disParseInstruction(uCodePtr, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseImmGrpl(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    int idx = (pCpu->bOpCode - 0x80) * 8;
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group1[idx+reg];
    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseShiftGrp2(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    int idx;
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    switch (pCpu->bOpCode)
    {
    case 0xC0:
    case 0xC1:
        idx = (pCpu->bOpCode - 0xC0)*8;
        break;

    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
        idx = (pCpu->bOpCode - 0xD0 + 2)*8;
        break;

    default:
        AssertMsgFailed(("Oops\n"));
        return sizeof(uint8_t);
    }

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group2[idx+reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp3(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    int idx = (pCpu->bOpCode - 0xF6) * 8;
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group3[idx+reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp4(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group4[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp5(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group5[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
// 0xF 0xF [ModRM] [SIB] [displacement] imm8_opcode
// It would appear the ModRM byte must always be present. How else can you
// determine the offset of the imm8_opcode byte otherwise?
//
//*****************************************************************************
static unsigned Parse3DNow(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrmsize;

#ifdef DEBUG_Sander
    //needs testing
    AssertMsgFailed(("Test me\n"));
#endif

    unsigned ModRM = disReadByte(pCpu, uCodePtr);
    pCpu->ModRM.Bits.Rm  = MODRM_RM(ModRM);
    pCpu->ModRM.Bits.Mod = MODRM_MOD(ModRM);
    pCpu->ModRM.Bits.Reg = MODRM_REG(ModRM);

    modrmsize = QueryModRM(uCodePtr+sizeof(uint8_t), pOp, pParam, pCpu, NULL);

    uint8_t opcode = disReadByte(pCpu, uCodePtr+sizeof(uint8_t)+modrmsize);

    pOp = (PCDISOPCODE)&g_aTwoByteMapX86_3DNow[opcode];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
#ifdef DEBUG_Sander /* bird, 2005-06-28: Alex is getting this during full installation of win2ksp4. */
        AssertMsgFailed(("Oops!\n")); //shouldn't happen!
#endif
        size = sizeof(uint8_t); //ModRM byte
    }

    size += disParseInstruction(uCodePtr, pOp, pCpu);
    size += sizeof(uint8_t);   //imm8_opcode uint8_t

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp6(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group6[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp7(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg, rm, mod;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    mod   = MODRM_MOD(modrm);
    reg   = MODRM_REG(modrm);
    rm    = MODRM_RM(modrm);

    if (mod == 3 && rm == 0)
        pOp = (PCDISOPCODE)&g_aMapX86_Group7_mod11_rm000[reg];
    else
    if (mod == 3 && rm == 1)
        pOp = (PCDISOPCODE)&g_aMapX86_Group7_mod11_rm001[reg];
    else
        pOp = (PCDISOPCODE)&g_aMapX86_Group7_mem[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp8(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group8[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp9(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group9[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp10(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group10[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp12(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    if (pCpu->fPrefix & DISPREFIX_OPSIZE)
        reg += 8;   //2nd table

    pOp = (PCDISOPCODE)&g_aMapX86_Group12[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp13(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);
    if (pCpu->fPrefix & DISPREFIX_OPSIZE)
        reg += 8;   //2nd table

    pOp = (PCDISOPCODE)&g_aMapX86_Group13[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp14(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);
    if (pCpu->fPrefix & DISPREFIX_OPSIZE)
        reg += 8;   //2nd table

    pOp = (PCDISOPCODE)&g_aMapX86_Group14[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp15(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg, mod, rm;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    mod   = MODRM_MOD(modrm);
    reg   = MODRM_REG(modrm);
    rm    = MODRM_RM(modrm);

    if (mod == 3 && rm == 0)
        pOp = (PCDISOPCODE)&g_aMapX86_Group15_mod11_rm000[reg];
    else
        pOp = (PCDISOPCODE)&g_aMapX86_Group15_mem[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static unsigned ParseGrp16(RTUINTPTR uCodePtr, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = disReadByte(pCpu, uCodePtr);
    reg   = MODRM_REG(modrm);

    pOp = (PCDISOPCODE)&g_aMapX86_Group16[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += disParseInstruction(uCodePtr, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
static void disasmModRMReg(PDISCPUSTATE pCpu, PCDISOPCODE pOp, unsigned idx, PDISOPPARAM pParam, int fRegAddr)
{
    NOREF(pOp); NOREF(pCpu);

    unsigned mod     = pCpu->ModRM.Bits.Mod;

    unsigned type    = OP_PARM_VTYPE(pParam->fParam);
    unsigned subtype = OP_PARM_VSUBTYPE(pParam->fParam);
    if (fRegAddr)
        subtype = (pCpu->uAddrMode == DISCPUMODE_64BIT) ? OP_PARM_q : OP_PARM_d;
    else
    if (subtype == OP_PARM_v || subtype == OP_PARM_NONE)
    {
        switch (pCpu->uOpMode)
        {
        case DISCPUMODE_32BIT:
            subtype = OP_PARM_d;
            break;
        case DISCPUMODE_64BIT:
            subtype = OP_PARM_q;
            break;
        case DISCPUMODE_16BIT:
            subtype = OP_PARM_w;
            break;
        default:
            /* make gcc happy */
            break;
        }
    }

    switch (subtype)
    {
    case OP_PARM_b:
        Assert(idx < (pCpu->fPrefix & DISPREFIX_REX ? 16U : 8U));

        /* AH, BH, CH & DH map to DIL, SIL, EBL & SPL when a rex prefix is present. */
        /* Intel® 64 and IA-32 Architectures Software Developer’s Manual: 3.4.1.1 */
        if (    (pCpu->fPrefix & DISPREFIX_REX)
            &&  idx >= DISGREG_AH
            &&  idx <= DISGREG_BH)
        {
            idx += (DISGREG_SPL - DISGREG_AH);
        }

        pParam->fUse |= DISUSE_REG_GEN8;
        pParam->Base.idxGenReg = idx;
        break;

    case OP_PARM_w:
        Assert(idx < (pCpu->fPrefix & DISPREFIX_REX ? 16U : 8U));

        pParam->fUse |= DISUSE_REG_GEN16;
        pParam->Base.idxGenReg = idx;
        break;

    case OP_PARM_d:
        Assert(idx < (pCpu->fPrefix & DISPREFIX_REX ? 16U : 8U));

        pParam->fUse |= DISUSE_REG_GEN32;
        pParam->Base.idxGenReg = idx;
        break;

    case OP_PARM_q:
        pParam->fUse |= DISUSE_REG_GEN64;
        pParam->Base.idxGenReg = idx;
        break;

    default:
        Log(("disasmModRMReg %x:%x failed!!\n", type, subtype));
        pCpu->rc = VERR_DIS_INVALID_MODRM;
        break;
    }
}
//*****************************************************************************
static const uint8_t g_auBaseModRMReg16[8]  =
{ DISGREG_BX, DISGREG_BX, DISGREG_BP, DISGREG_BP, DISGREG_SI, DISGREG_DI, DISGREG_BP, DISGREG_BX};
static const uint8_t g_auIndexModRMReg16[4] = { DISGREG_SI, DISGREG_DI, DISGREG_SI, DISGREG_DI };
//*****************************************************************************
static void disasmModRMReg16(PDISCPUSTATE pCpu, PCDISOPCODE pOp, unsigned idx, PDISOPPARAM pParam)
{
    NOREF(pCpu); NOREF(pOp);
    pParam->fUse |= DISUSE_REG_GEN16;
    pParam->Base.idxGenReg = g_auBaseModRMReg16[idx];
    if (idx < 4)
    {
        pParam->fUse |= DISUSE_INDEX;
        pParam->Index.idxGenReg = g_auIndexModRMReg16[idx];
    }
}
//*****************************************************************************
//*****************************************************************************
static void disasmModRMSReg(PDISCPUSTATE pCpu, PCDISOPCODE pOp, unsigned idx, PDISOPPARAM pParam)
{
    NOREF(pOp);
    if (idx >= DISSELREG_END)
    {
        Log(("disasmModRMSReg %d failed!!\n", idx));
        pCpu->rc = VERR_DIS_INVALID_PARAMETER;
        return;
    }

    pParam->fUse |= DISUSE_REG_SEG;
    pParam->Base.idxSegReg = (DISSELREG)idx;
}



/**
 * Validates the lock sequence.
 *
 * The AMD manual lists the following instructions:
 *      ADC
 *      ADD
 *      AND
 *      BTC
 *      BTR
 *      BTS
 *      CMPXCHG
 *      CMPXCHG8B
 *      CMPXCHG16B
 *      DEC
 *      INC
 *      NEG
 *      NOT
 *      OR
 *      SBB
 *      SUB
 *      XADD
 *      XCHG
 *      XOR
 *
 * @param   pCpu    Fully disassembled instruction.
 */
static void disValidateLockSequence(PDISCPUSTATE pCpu)
{
    Assert(pCpu->fPrefix & DISPREFIX_LOCK);

    /*
     * Filter out the valid lock sequences.
     */
    switch (pCpu->pCurInstr->uOpcode)
    {
        /* simple: no variations */
        case OP_CMPXCHG8B: /* == OP_CMPXCHG16B? */
            return;

        /* simple: /r - reject register destination. */
        case OP_BTC:
        case OP_BTR:
        case OP_BTS:
        case OP_CMPXCHG:
        case OP_XADD:
            if (pCpu->ModRM.Bits.Mod == 3)
                break;
            return;

        /*
         * Lots of variants but its sufficient to check that param 1
         * is a memory operand.
         */
        case OP_ADC:
        case OP_ADD:
        case OP_AND:
        case OP_DEC:
        case OP_INC:
        case OP_NEG:
        case OP_NOT:
        case OP_OR:
        case OP_SBB:
        case OP_SUB:
        case OP_XCHG:
        case OP_XOR:
            if (pCpu->Param1.fUse & (DISUSE_BASE | DISUSE_INDEX | DISUSE_DISPLACEMENT64 | DISUSE_DISPLACEMENT32
                                     | DISUSE_DISPLACEMENT16 | DISUSE_DISPLACEMENT8 | DISUSE_RIPDISPLACEMENT32))
                return;
            break;

        default:
            break;
    }

    /*
     * Invalid lock sequence, make it a OP_ILLUD2.
     */
    pCpu->pCurInstr = &g_aTwoByteMapX86[11];
    Assert(pCpu->pCurInstr->uOpcode == OP_ILLUD2);
}


/**
 * Internal worker for DISInstEx.
 *
 * @returns VBox status code.
 * @param   pCpu            Initialized cpu state.
 * @param   paOneByteMap    The one byte opcode map to use.
 * @param   uInstrAddr      Instruction address.
 * @param   pcbInstr        Where to store the instruction size. Can be NULL.
 */
static int disInstrWorker(PDISCPUSTATE pCpu, RTUINTPTR uInstrAddr, PCDISOPCODE paOneByteMap, uint32_t *pcbInstr)
{
    /*
     * Parse byte by byte.
     */
    size_t offByte = 0;
    for (;;)
    {
        uint8_t codebyte = disReadByteByOff(pCpu, offByte++);
        uint8_t opcode   = paOneByteMap[codebyte].uOpcode;

        /* Hardcoded assumption about OP_* values!! */
        if (opcode <= OP_LAST_PREFIX)
        {
            /* The REX prefix must precede the opcode byte(s). Any other placement is ignored. */
            if (opcode != OP_REX)
            {
                /** Last prefix byte (for SSE2 extension tables); don't include the REX prefix */
                pCpu->bLastPrefix = opcode;
                pCpu->fPrefix &= ~DISPREFIX_REX;
            }

            switch (opcode)
            {
            case OP_INVALID:
                if (pcbInstr)
                    *pcbInstr = (uint32_t)offByte;
                return pCpu->rc = VERR_DIS_INVALID_OPCODE;

            // segment override prefix byte
            case OP_SEG:
                pCpu->idxSegPrefix = (DISSELREG)(paOneByteMap[codebyte].fParam1 - OP_PARM_REG_SEG_START);
                /* Segment prefixes for CS, DS, ES and SS are ignored in long mode. */
                if (   pCpu->uCpuMode != DISCPUMODE_64BIT
                    || pCpu->idxSegPrefix >= DISSELREG_FS)
                {
                    pCpu->fPrefix   |= DISPREFIX_SEG;
                }
                continue;   //fetch the next byte

            // lock prefix byte
            case OP_LOCK:
                pCpu->fPrefix |= DISPREFIX_LOCK;
                continue;   //fetch the next byte

            // address size override prefix byte
            case OP_ADDRSIZE:
                pCpu->fPrefix |= DISPREFIX_ADDRSIZE;
                if (pCpu->uCpuMode == DISCPUMODE_16BIT)
                    pCpu->uAddrMode = DISCPUMODE_32BIT;
                else
                if (pCpu->uCpuMode == DISCPUMODE_32BIT)
                    pCpu->uAddrMode = DISCPUMODE_16BIT;
                else
                    pCpu->uAddrMode = DISCPUMODE_32BIT;     /* 64 bits */
                continue;   //fetch the next byte

            // operand size override prefix byte
            case OP_OPSIZE:
                pCpu->fPrefix |= DISPREFIX_OPSIZE;
                if (pCpu->uCpuMode == DISCPUMODE_16BIT)
                    pCpu->uOpMode = DISCPUMODE_32BIT;
                else
                    pCpu->uOpMode = DISCPUMODE_16BIT;  /* for 32 and 64 bits mode (there is no 32 bits operand size override prefix) */
                continue;   //fetch the next byte

            // rep and repne are not really prefixes, but we'll treat them as such
            case OP_REPE:
                pCpu->fPrefix |= DISPREFIX_REP;
                continue;   //fetch the next byte

            case OP_REPNE:
                pCpu->fPrefix |= DISPREFIX_REPNE;
                continue;   //fetch the next byte

            case OP_REX:
                Assert(pCpu->uCpuMode == DISCPUMODE_64BIT);
                /* REX prefix byte */
                pCpu->fPrefix   |= DISPREFIX_REX;
                pCpu->fRexPrefix = DISPREFIX_REX_OP_2_FLAGS(paOneByteMap[codebyte].fParam1);
                if (pCpu->fRexPrefix & DISPREFIX_REX_FLAGS_W)
                    pCpu->uOpMode = DISCPUMODE_64BIT;  /* overrides size prefix byte */
                continue;   //fetch the next byte
            }
        }

        /* first opcode byte. */
        pCpu->bOpCode = codebyte;
        offByte += disParseInstruction(uInstrAddr + offByte, &paOneByteMap[pCpu->bOpCode], pCpu);
        break;
    }

    pCpu->cbInstr = offByte;
    if (pcbInstr)
        *pcbInstr = offByte;

    if (pCpu->fPrefix & DISPREFIX_LOCK)
        disValidateLockSequence(pCpu);

    return pCpu->rc;
}


/**
 * Disassembles on instruction, details in @a pCpu and length in @a pcbInstr.
 *
 * @returns VBox status code.
 * @param   uInstrAddr      Address of the instruction to decode. What this means
 *                          is left to the pfnReadBytes function.
 * @param   enmCpuMode      The CPU mode. DISCPUMODE_32BIT, DISCPUMODE_16BIT, or DISCPUMODE_64BIT.
 * @param   pfnReadBytes    Callback for reading instruction bytes.
 * @param   fFilter         Instruction type filter.
 * @param   pvUser          User argument for the instruction reader. (Ends up in pvUser.)
 * @param   pCpu            Pointer to CPU structure. With the exception of
 *                          DISCPUSTATE::pvUser2, the structure will be
 *                          completely initialized by this API, i.e. no input is
 *                          taken from it.
 * @param   pcbInstr        Where to store the size of the instruction.  (This
 *                          is also stored in PDISCPUSTATE::cbInstr.)  Optional.
 */
DISDECL(int) DISInstEx(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, uint32_t fFilter,
                       PFNDISREADBYTES pfnReadBytes, void *pvUser,
                       PDISCPUSTATE pCpu, uint32_t *pcbInstr)
{
    PCDISOPCODE paOneByteMap;

    /*
     * Initialize the CPU state.
     * Note! The RT_BZERO make ASSUMPTIONS about the placement of pvUser2.
     */
    RT_BZERO(pCpu, RT_OFFSETOF(DISCPUSTATE, pvUser2));

#ifdef VBOX_STRICT /* poison */
    pCpu->Param1.Base.idxGenReg  = 0xc1;
    pCpu->Param2.Base.idxGenReg  = 0xc2;
    pCpu->Param3.Base.idxGenReg  = 0xc3;
    pCpu->Param1.Index.idxGenReg = 0xc4;
    pCpu->Param2.Index.idxGenReg = 0xc5;
    pCpu->Param3.Index.idxGenReg = 0xc6;
    pCpu->Param1.uDisp.u64 = UINT64_C(0xd1d1d1d1d1d1d1d1);
    pCpu->Param2.uDisp.u64 = UINT64_C(0xd2d2d2d2d2d2d2d2);
    pCpu->Param3.uDisp.u64 = UINT64_C(0xd3d3d3d3d3d3d3d3);
    pCpu->Param1.uValue    = UINT64_C(0xb1b1b1b1b1b1b1b1);
    pCpu->Param2.uValue    = UINT64_C(0xb2b2b2b2b2b2b2b2);
    pCpu->Param3.uValue    = UINT64_C(0xb3b3b3b3b3b3b3b3);
    pCpu->Param1.uScale    = 28;
    pCpu->Param2.uScale    = 29;
    pCpu->Param3.uScale    = 30;
#endif

    pCpu->uCpuMode          = enmCpuMode;
    if (enmCpuMode == DISCPUMODE_64BIT)
    {
        paOneByteMap        = g_aOneByteMapX64;
        pCpu->uAddrMode     = DISCPUMODE_64BIT;
        pCpu->uOpMode       = DISCPUMODE_32BIT;
    }
    else
    {
        paOneByteMap        = g_aOneByteMapX86;
        pCpu->uAddrMode     = enmCpuMode;
        pCpu->uOpMode       = enmCpuMode;
    }
    pCpu->fPrefix           = DISPREFIX_NONE;
    pCpu->idxSegPrefix      = DISSELREG_DS;
    pCpu->uInstrAddr        = uInstrAddr;
    pCpu->pfnDisasmFnTable  = g_apfnFullDisasm;
    pCpu->fFilter           = fFilter;
    pCpu->rc                = VINF_SUCCESS;
    pCpu->pfnReadBytes      = pfnReadBytes ? pfnReadBytes : disReadBytesDefault;
    pCpu->pvUser            = pvUser;

    /*
     * Read some bytes into the cache.  (If this fail we continue as nothing
     * has gone wrong since this is what would happen if we didn't precharge
     * the cache here.)
     */
    int rc = pCpu->pfnReadBytes(pCpu, 0, 1, sizeof(pCpu->abInstr));
    if (RT_SUCCESS(rc))
    {
        Assert(pCpu->cbCachedInstr >= 1);
        Assert(pCpu->cbCachedInstr <= sizeof(pCpu->abInstr));
    }
    else
    {
        Log(("Initial read failed with rc=%Rrc!!\n", rc));
        pCpu->rc = VERR_DIS_MEM_READ;
    }

    return disInstrWorker(pCpu, uInstrAddr, paOneByteMap, pcbInstr);
}


/**
 * Parses one guest instruction.
 *
 * The result is found in pCpu and pcbInstr.
 *
 * @returns VBox status code.
 * @param   uInstrAddr      Address of the instruction to decode. What this means
 *                          is left to the pfnReadBytes function.
 * @param   enmCpuMode      The CPU mode. DISCPUMODE_32BIT, DISCPUMODE_16BIT, or DISCPUMODE_64BIT.
 * @param   pfnReadBytes    Callback for reading instruction bytes.
 * @param   pvUser          User argument for the instruction reader. (Ends up in pvUser.)
 * @param   pCpu            Pointer to cpu structure. Will be initialized.
 * @param   pcbInstr        Where to store the size of the instruction.
 *                          NULL is allowed.  This is also stored in
 *                          PDISCPUSTATE::cbInstr.
 */
DISDECL(int) DISInstrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                PDISCPUSTATE pCpu, uint32_t *pcbInstr)
{
    return DISInstEx(uInstrAddr, enmCpuMode, DISOPTYPE_ALL, pfnReadBytes, pvUser, pCpu, pcbInstr);
}


/**
 * Parses one guest instruction.
 *
 * The result is found in pCpu and pcbInstr.
 *
 * @returns VBox status code.
 * @param   pvInstr         Address of the instruction to decode.  This is a
 *                          real address in the current context that can be
 *                          accessed without faulting.  (Consider
 *                          DISInstrWithReader if this isn't the case.)
 * @param   enmCpuMode      The CPU mode. DISCPUMODE_32BIT, DISCPUMODE_16BIT, or DISCPUMODE_64BIT.
 * @param   pfnReadBytes    Callback for reading instruction bytes.
 * @param   pvUser          User argument for the instruction reader. (Ends up in pvUser.)
 * @param   pCpu            Pointer to cpu structure. Will be initialized.
 * @param   pcbInstr        Where to store the size of the instruction.
 *                          NULL is allowed.  This is also stored in
 *                          PDISCPUSTATE::cbInstr.
 */
DISDECL(int) DISInstr(const void *pvInstr, DISCPUMODE enmCpuMode, PDISCPUSTATE pCpu, uint32_t *pcbInstr)
{
    return DISInstEx((uintptr_t)pvInstr, enmCpuMode, DISOPTYPE_ALL, NULL /*pfnReadBytes*/, NULL /*pvUser*/, pCpu, pcbInstr);
}

