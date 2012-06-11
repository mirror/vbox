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
#ifdef USING_VISUAL_STUDIO
# include <stdafx.h>
#endif

#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/string.h>
#include <iprt/stdarg.h>
#include "DisasmInternal.h"
#include "DisasmTables.h"

#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED)
# include <stdlib.h>
# include <stdio.h>
#endif


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static int disCoreParseInstr(PDISCPUSTATE pCpu, RTUINTPTR uInstrAddr, const OPCODE *paOneByteMap,
                             unsigned *pcbInstruction);
#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED)
static void disasmAddString(char *psz, const char *pszString);
static void disasmAddStringF(char *psz, const char *pszFormat, ...);
static void disasmAddChar(char *psz, char ch);
# define disasmAddStringF1(psz, pszFmt, a1)         disasmAddStringF(psz, pszFmt, a1)
# define disasmAddStringF2(psz, pszFmt, a1, a2)     disasmAddStringF(psz, pszFmt, a1, a2)
# define disasmAddStringF3(psz, pszFmt, a1, a2, a3) disasmAddStringF(psz, pszFmt, a1, a2, a3)
#else
# define disasmAddString(psz, pszString)            do {} while (0)
# define disasmAddStringF1(psz, pszFmt, a1)         do {} while (0)
# define disasmAddStringF2(psz, pszFmt, a1, a2)     do {} while (0)
# define disasmAddStringF3(psz, pszFmt, a1, a2, a3) do {} while (0)
# define disasmAddChar(psz, ch)                     do {} while (0)
#endif

static unsigned QueryModRM(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, unsigned *pSibInc = NULL);
static unsigned QueryModRM_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, unsigned *pSibInc = NULL);
static void     UseSIB(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
static unsigned ParseSIB_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

PFNDISPARSE  pfnFullDisasm[IDX_ParseMax] =
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

PFNDISPARSE  pfnCalcSize[IDX_ParseMax] =
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


/**
 * Parses one guest instruction.
 * The result is found in pCpu and pcbInstruction.
 *
 * @returns VBox status code.
 * @param   uInstrAddr      Address of the instruction to decode. What this means
 *                          is left to the pfnReadBytes function.
 * @param   enmCpuMode      The CPU mode. CPUMODE_32BIT, CPUMODE_16BIT, or CPUMODE_64BIT.
 * @param   pfnReadBytes    Callback for reading instruction bytes.
 * @param   pvUser          User argument for the instruction reader. (Ends up in apvUserData[0].)
 * @param   pCpu            Pointer to cpu structure. Will be initialized.
 * @param   pcbInstruction  Where to store the size of the instruction.
 *                          NULL is allowed.
 */
DISDECL(int) DISCoreOneEx(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                          PDISCPUSTATE pCpu, unsigned *pcbInstruction)
{
    const OPCODE *paOneByteMap;

    /*
     * Initialize the CPU state.
     * Note! The RT_BZERO make ASSUMPTIONS about the placement of apvUserData.
     */
    RT_BZERO(pCpu, RT_OFFSETOF(DISCPUSTATE, apvUserData));

    pCpu->mode              = enmCpuMode;
    if (enmCpuMode == CPUMODE_64BIT)
    {
        paOneByteMap        = g_aOneByteMapX64;
        pCpu->addrmode      = CPUMODE_64BIT;
        pCpu->opmode        = CPUMODE_32BIT;
    }
    else
    {
        paOneByteMap        = g_aOneByteMapX86;
        pCpu->addrmode      = enmCpuMode;
        pCpu->opmode        = enmCpuMode;
    }
    pCpu->prefix            = PREFIX_NONE;
    pCpu->enmPrefixSeg      = DIS_SELREG_DS;
    pCpu->uInstrAddr        = uInstrAddr;
    pCpu->pfnDisasmFnTable  = pfnFullDisasm;
    pCpu->uFilter           = OPTYPE_ALL;
    pCpu->rc                = VINF_SUCCESS;
    pCpu->pfnReadBytes      = pfnReadBytes ? pfnReadBytes : disReadBytesDefault;
    pCpu->apvUserData[0]    = pvUser;

    return disCoreParseInstr(pCpu, uInstrAddr, paOneByteMap, pcbInstruction);
}

/**
 * Internal worker for DISCoreOne and DISCoreOneEx.
 *
 * @returns VBox status code.
 * @param   pCpu            Initialized cpu state.
 * @param   paOneByteMap    The one byte opcode map to use.
 * @param   uInstrAddr      Instruction address.
 * @param   pcbInstruction  Where to store the instruction size. Can be NULL.
 */
static int disCoreParseInstr(PDISCPUSTATE pCpu, RTUINTPTR uInstrAddr, const OPCODE *paOneByteMap, unsigned *pcbInstruction)
{
    /*
     * Parse byte by byte.
     */
    unsigned  iByte = 0;
    unsigned  cbInc;
    for (;;)
    {
        uint8_t codebyte = DISReadByte(pCpu, uInstrAddr+iByte);
        uint8_t opcode   = paOneByteMap[codebyte].opcode;

        /* Hardcoded assumption about OP_* values!! */
        if (opcode <= OP_LAST_PREFIX)
        {
            /* The REX prefix must precede the opcode byte(s). Any other placement is ignored. */
            if (opcode != OP_REX)
            {
                /** Last prefix byte (for SSE2 extension tables); don't include the REX prefix */
                pCpu->lastprefix = opcode;
                pCpu->prefix &= ~PREFIX_REX;
            }

            switch (opcode)
            {
            case OP_INVALID:
                if (pcbInstruction)
                    *pcbInstruction = iByte + 1;
                return pCpu->rc = VERR_DIS_INVALID_OPCODE;

            // segment override prefix byte
            case OP_SEG:
                pCpu->enmPrefixSeg = (DIS_SELREG)(paOneByteMap[codebyte].param1 - OP_PARM_REG_SEG_START);
                /* Segment prefixes for CS, DS, ES and SS are ignored in long mode. */
                if (   pCpu->mode != CPUMODE_64BIT
                    || pCpu->enmPrefixSeg >= DIS_SELREG_FS)
                {
                    pCpu->prefix    |= PREFIX_SEG;
                }
                iByte += sizeof(uint8_t);
                continue;   //fetch the next byte

            // lock prefix byte
            case OP_LOCK:
                pCpu->prefix |= PREFIX_LOCK;
                iByte       += sizeof(uint8_t);
                continue;   //fetch the next byte

            // address size override prefix byte
            case OP_ADDRSIZE:
                pCpu->prefix |= PREFIX_ADDRSIZE;
                if (pCpu->mode == CPUMODE_16BIT)
                    pCpu->addrmode = CPUMODE_32BIT;
                else
                if (pCpu->mode == CPUMODE_32BIT)
                    pCpu->addrmode = CPUMODE_16BIT;
                else
                    pCpu->addrmode = CPUMODE_32BIT;     /* 64 bits */

                iByte        += sizeof(uint8_t);
                continue;   //fetch the next byte

            // operand size override prefix byte
            case OP_OPSIZE:
                pCpu->prefix |= PREFIX_OPSIZE;
                if (pCpu->mode == CPUMODE_16BIT)
                    pCpu->opmode = CPUMODE_32BIT;
                else
                    pCpu->opmode = CPUMODE_16BIT;  /* for 32 and 64 bits mode (there is no 32 bits operand size override prefix) */

                iByte        += sizeof(uint8_t);
                continue;   //fetch the next byte

            // rep and repne are not really prefixes, but we'll treat them as such
            case OP_REPE:
                pCpu->prefix |= PREFIX_REP;
                iByte       += sizeof(uint8_t);
                continue;   //fetch the next byte

            case OP_REPNE:
                pCpu->prefix |= PREFIX_REPNE;
                iByte       += sizeof(uint8_t);
                continue;   //fetch the next byte

            case OP_REX:
                Assert(pCpu->mode == CPUMODE_64BIT);
                /* REX prefix byte */
                pCpu->prefix    |= PREFIX_REX;
                pCpu->prefix_rex = PREFIX_REX_OP_2_FLAGS(paOneByteMap[codebyte].param1);
                iByte           += sizeof(uint8_t);

                if (pCpu->prefix_rex & PREFIX_REX_FLAGS_W)
                    pCpu->opmode = CPUMODE_64BIT;  /* overrides size prefix byte */
                continue;   //fetch the next byte
            }
        }

        unsigned uIdx = iByte;
        iByte += sizeof(uint8_t); //first opcode byte

        pCpu->opaddr = uInstrAddr;
        pCpu->opcode = codebyte;

        cbInc = ParseInstruction(uInstrAddr + iByte, &paOneByteMap[pCpu->opcode], pCpu);
        iByte += cbInc;
        break;
    }

    AssertMsg(pCpu->opsize == iByte || RT_FAILURE_NP(pCpu->rc), ("%u %u\n", pCpu->opsize, iByte));
    pCpu->opsize = iByte;
    if (pcbInstruction)
        *pcbInstruction = iByte;

    if (pCpu->prefix & PREFIX_LOCK)
        disValidateLockSequence(pCpu);

    return pCpu->rc;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseInstruction(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, PDISCPUSTATE pCpu)
{
    int  size = 0;
    bool fFiltered = false;

    Assert(lpszCodeBlock && pOp && pCpu);

    // Store the opcode format string for disasmPrintf
#ifndef DIS_CORE_ONLY
    pCpu->pszOpcode = pOp->pszOpcode;
#endif
    pCpu->pCurInstr = pOp;

    /*
     * Apply filter to instruction type to determine if a full disassembly is required.
     * @note Multibyte opcodes are always marked harmless until the final byte.
     */
    if ((pOp->optype & pCpu->uFilter) == 0)
    {
        fFiltered = true;
        pCpu->pfnDisasmFnTable = pfnCalcSize;
    }
    else
    {
        /* Not filtered out -> full disassembly */
        pCpu->pfnDisasmFnTable = pfnFullDisasm;
    }

    // Should contain the parameter type on input
    pCpu->param1.param = pOp->param1;
    pCpu->param2.param = pOp->param2;
    pCpu->param3.param = pOp->param3;

    /* Correct the operand size if the instruction is marked as forced or default 64 bits */
    if (pCpu->mode == CPUMODE_64BIT)
    {
        if (pOp->optype & OPTYPE_FORCED_64_OP_SIZE)
            pCpu->opmode = CPUMODE_64BIT;
        else
        if (    (pOp->optype & OPTYPE_DEFAULT_64_OP_SIZE)
            &&  !(pCpu->prefix & PREFIX_OPSIZE))
            pCpu->opmode = CPUMODE_64BIT;
    }
    else
    if (pOp->optype & OPTYPE_FORCED_32_OP_SIZE_X86)
    {
        /* Forced 32 bits operand size for certain instructions (mov crx, mov drx). */
        Assert(pCpu->mode != CPUMODE_64BIT);
        pCpu->opmode = CPUMODE_32BIT;
    }

    if (pOp->idxParse1 != IDX_ParseNop)
    {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse1](lpszCodeBlock, pOp, &pCpu->param1, pCpu);
        if (fFiltered == false) pCpu->param1.size = DISGetParamSize(pCpu, &pCpu->param1);
    }

    if (pOp->idxParse2 != IDX_ParseNop)
    {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse2](lpszCodeBlock+size, pOp, &pCpu->param2, pCpu);
        if (fFiltered == false) pCpu->param2.size = DISGetParamSize(pCpu, &pCpu->param2);
    }

    if (pOp->idxParse3 != IDX_ParseNop)
    {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse3](lpszCodeBlock+size, pOp, &pCpu->param3, pCpu);
        if (fFiltered == false) pCpu->param3.size = DISGetParamSize(pCpu, &pCpu->param3);
    }
    // else simple one byte instruction

    return size;
}
//*****************************************************************************
/* Floating point opcode parsing */
//*****************************************************************************
unsigned ParseEscFP(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int index;
    const OPCODE *fpop;
    unsigned size = 0;
    unsigned ModRM;
    NOREF(pOp);

    ModRM = DISReadByte(pCpu, lpszCodeBlock);

    index = pCpu->opcode - 0xD8;
    if (ModRM <= 0xBF)
    {
        fpop            = &(g_paMapX86_FP_Low[index])[MODRM_REG(ModRM)];
        pCpu->pCurInstr = (PCOPCODE)fpop;

        // Should contain the parameter type on input
        pCpu->param1.param = fpop->param1;
        pCpu->param2.param = fpop->param2;
    }
    else
    {
        fpop            = &(g_paMapX86_FP_High[index])[ModRM - 0xC0];
        pCpu->pCurInstr = (PCOPCODE)fpop;
    }

    /*
     * Apply filter to instruction type to determine if a full disassembly is required.
     * @note Multibyte opcodes are always marked harmless until the final byte.
     */
    if ((fpop->optype & pCpu->uFilter) == 0)
    {
        pCpu->pfnDisasmFnTable = pfnCalcSize;
    }
    else
    {
        /* Not filtered out -> full disassembly */
        pCpu->pfnDisasmFnTable = pfnFullDisasm;
    }

    /* Correct the operand size if the instruction is marked as forced or default 64 bits */
    if (pCpu->mode == CPUMODE_64BIT)
    {
        /* Note: redundant, but just in case this ever changes */
        if (fpop->optype & OPTYPE_FORCED_64_OP_SIZE)
            pCpu->opmode = CPUMODE_64BIT;
        else
        if (    (fpop->optype & OPTYPE_DEFAULT_64_OP_SIZE)
            &&  !(pCpu->prefix & PREFIX_OPSIZE))
            pCpu->opmode = CPUMODE_64BIT;
    }

    // Little hack to make sure the ModRM byte is included in the returned size
    if (fpop->idxParse1 != IDX_ParseModRM && fpop->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    if (fpop->idxParse1 != IDX_ParseNop)
        size += pCpu->pfnDisasmFnTable[fpop->idxParse1](lpszCodeBlock+size, (PCOPCODE)fpop, pParam, pCpu);

    if (fpop->idxParse2 != IDX_ParseNop)
        size += pCpu->pfnDisasmFnTable[fpop->idxParse2](lpszCodeBlock+size, (PCOPCODE)fpop, pParam, pCpu);

    // Store the opcode format string for disasmPrintf
#ifndef DIS_CORE_ONLY
    pCpu->pszOpcode = fpop->pszOpcode;
#endif

    return size;
}
//*****************************************************************************
// SIB byte: (32 bits mode only)
// 7 - 6  5 - 3  2-0
// Scale  Index  Base
//*****************************************************************************
static const char *szSIBBaseReg[8]    = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"};
static const char *szSIBIndexReg[8]   = {"EAX", "ECX", "EDX", "EBX", NULL,  "EBP", "ESI", "EDI"};
static const char *szSIBBaseReg64[16] = {"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
static const char *szSIBIndexReg64[16]= {"RAX", "RCX", "RDX", "RBX", NULL,  "RBP", "RSI", "RDI", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED) || defined(_MSC_VER)
static const char *szSIBScale[4]    = {"", "*2", "*4", "*8"};
#endif
//*****************************************************************************
void UseSIB(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned scale, base, index, regtype;
    const char **ppszSIBIndexReg;
    const char **ppszSIBBaseReg;
    NOREF(lpszCodeBlock); NOREF(pOp);

    scale = pCpu->SIB.Bits.Scale;
    base  = pCpu->SIB.Bits.Base;
    index = pCpu->SIB.Bits.Index;

    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        ppszSIBIndexReg = szSIBIndexReg;
        ppszSIBBaseReg  = szSIBBaseReg;
        regtype         = USE_REG_GEN32;
    }
    else
    {
        ppszSIBIndexReg = szSIBIndexReg64;
        ppszSIBBaseReg  = szSIBBaseReg64;
        regtype         = USE_REG_GEN64;
    }

    if (ppszSIBIndexReg[index])
    {
         pParam->flags |= USE_INDEX | regtype;
         pParam->index.reg_gen = index;

         if (scale != 0)
         {
             pParam->flags |= USE_SCALE;
             pParam->scale  = (1<<scale);
         }

         if (base == 5 && pCpu->ModRM.Bits.Mod == 0)
             disasmAddStringF2(pParam->szParam, "%s%s", ppszSIBIndexReg[index], szSIBScale[scale]);
         else
             disasmAddStringF3(pParam->szParam, "%s+%s%s", ppszSIBBaseReg[base], ppszSIBIndexReg[index], szSIBScale[scale]);
    }
    else
    {
         if (base != 5 || pCpu->ModRM.Bits.Mod != 0)
             disasmAddStringF1(pParam->szParam, "%s", ppszSIBBaseReg[base]);
    }

    if (base == 5 && pCpu->ModRM.Bits.Mod == 0)
    {
        // [scaled index] + disp32
        if (pCpu->addrmode == CPUMODE_32BIT)
        {
            pParam->flags |= USE_DISPLACEMENT32;
            pParam->disp32 = pCpu->i32SibDisp;
            disasmAddChar(pParam->szParam, '+');
            disasmPrintDisp32(pParam);
        }
        else
        {   /* sign-extend to 64 bits */
            pParam->flags |= USE_DISPLACEMENT64;
            pParam->disp64 = pCpu->i32SibDisp;
            disasmAddChar(pParam->szParam, '+');
            disasmPrintDisp64(pParam);
        }
    }
    else
    {
        pParam->flags |= USE_BASE | regtype;
        pParam->base.reg_gen = base;
    }
    return;   /* Already fetched everything in ParseSIB; no size returned */
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseSIB(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);
    unsigned SIB;
    NOREF(pOp); NOREF(pParam);

    SIB = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += size;

    pCpu->SIB.Bits.Base  = SIB_BASE(SIB);
    pCpu->SIB.Bits.Index = SIB_INDEX(SIB);
    pCpu->SIB.Bits.Scale = SIB_SCALE(SIB);

    if (pCpu->prefix & PREFIX_REX)
    {
        /* REX.B extends the Base field if not scaled index + disp32 */
        if (!(pCpu->SIB.Bits.Base == 5 && pCpu->ModRM.Bits.Mod == 0))
            pCpu->SIB.Bits.Base  |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_B)) << 3);

        pCpu->SIB.Bits.Index |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_X)) << 3);
    }

    if (    pCpu->SIB.Bits.Base == 5
        &&  pCpu->ModRM.Bits.Mod == 0)
    {
        /* Additional 32 bits displacement. No change in long mode. */
        pCpu->i32SibDisp = DISReadDWord(pCpu, lpszCodeBlock);
        size += sizeof(int32_t);
    }
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseSIB_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);
    unsigned SIB;
    NOREF(pOp); NOREF(pParam);

    SIB = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += size;

    pCpu->SIB.Bits.Base  = SIB_BASE(SIB);
    pCpu->SIB.Bits.Index = SIB_INDEX(SIB);
    pCpu->SIB.Bits.Scale = SIB_SCALE(SIB);

    if (pCpu->prefix & PREFIX_REX)
    {
        /* REX.B extends the Base field. */
        pCpu->SIB.Bits.Base  |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_B)) << 3);
        /* REX.X extends the Index field. */
        pCpu->SIB.Bits.Index |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_X)) << 3);
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
unsigned UseModRM(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int      vtype = OP_PARM_VTYPE(pParam->param);
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
                pParam->flags |= USE_REG_CR;

                if (    pCpu->pCurInstr->opcode == OP_MOV_CR
                    &&  pCpu->opmode == CPUMODE_32BIT
                    &&  (pCpu->prefix & PREFIX_LOCK))
                {
                    pCpu->prefix &= ~PREFIX_LOCK;
                    pParam->base.reg_ctrl = USE_REG_CR8;
                }
                else
                    pParam->base.reg_ctrl = reg;

                disasmAddStringF1(pParam->szParam, "CR%d", pParam->base.reg_ctrl);
                return 0;

            case OP_PARM_D: //debug register
                disasmAddStringF1(pParam->szParam, "DR%d", reg);
                pParam->flags |= USE_REG_DBG;
                pParam->base.reg_dbg = reg;
                return 0;

            case OP_PARM_P: //MMX register
                reg &= 7;   /* REX.R has no effect here */
                disasmAddStringF1(pParam->szParam, "MM%d", reg);
                pParam->flags |= USE_REG_MMX;
                pParam->base.reg_mmx = reg;
                return 0;

            case OP_PARM_S: //segment register
                reg &= 7;   /* REX.R has no effect here */
                disasmModRMSReg(pCpu, pOp, reg, pParam);
                pParam->flags |= USE_REG_SEG;
                return 0;

            case OP_PARM_T: //test register
                reg &= 7;   /* REX.R has no effect here */
                disasmAddStringF1(pParam->szParam, "TR%d", reg);
                pParam->flags |= USE_REG_TEST;
                pParam->base.reg_test = reg;
                return 0;

            case OP_PARM_W: //XMM register or memory operand
                if (mod != 3)
                    break;  /* memory operand */
                reg = rm; /* the RM field specifies the xmm register */
                /* else no break */

            case OP_PARM_V: //XMM register
                disasmAddStringF1(pParam->szParam, "XMM%d", reg);
                pParam->flags |= USE_REG_XMM;
                pParam->base.reg_xmm = reg;
                return 0;
            }
        }
    }

    /* @todo bound */

    if (pCpu->addrmode != CPUMODE_16BIT)
    {
        Assert(pCpu->addrmode == CPUMODE_32BIT || pCpu->addrmode == CPUMODE_64BIT);

        /*
         * Note: displacements in long mode are 8 or 32 bits and sign-extended to 64 bits
         */
        switch (mod)
        {
        case 0: //effective address
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if (rm == 4)
            {   /* SIB byte follows ModRM */
                UseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            }
            else
            if (rm == 5)
            {
                /* 32 bits displacement */
                if (pCpu->mode != CPUMODE_64BIT)
                {
                    pParam->flags |= USE_DISPLACEMENT32;
                    pParam->disp32 = pCpu->i32SibDisp;
                    disasmPrintDisp32(pParam);
                }
                else
                {
                    pParam->flags |= USE_RIPDISPLACEMENT32;
                    pParam->disp32 = pCpu->i32SibDisp;
                    disasmAddString(pParam->szParam, "RIP+");
                    disasmPrintDisp32(pParam);
                }
            }
            else {//register address
                pParam->flags |= USE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            disasmAddChar(pParam->szParam, ']');
            break;

        case 1: //effective address + 8 bits displacement
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if (rm == 4) {//SIB byte follows ModRM
                UseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            }
            else
            {
                pParam->flags |= USE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            pParam->disp8 = pCpu->i32SibDisp;
            pParam->flags |= USE_DISPLACEMENT8;

            if (pParam->disp8 != 0)
            {
                if (pParam->disp8 > 0)
                    disasmAddChar(pParam->szParam, '+');
                disasmPrintDisp8(pParam);
            }
            disasmAddChar(pParam->szParam, ']');
            break;

        case 2: //effective address + 32 bits displacement
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if (rm == 4) {//SIB byte follows ModRM
                UseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            }
            else
            {
                pParam->flags |= USE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            pParam->disp32 = pCpu->i32SibDisp;
            pParam->flags |= USE_DISPLACEMENT32;

            if (pParam->disp32 != 0)
            {
                disasmAddChar(pParam->szParam, '+');
                disasmPrintDisp32(pParam);
            }
            disasmAddChar(pParam->szParam, ']');
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
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if (rm == 6)
            {//16 bits displacement
                pParam->disp16 = pCpu->i32SibDisp;
                pParam->flags |= USE_DISPLACEMENT16;
                disasmPrintDisp16(pParam);
            }
            else
            {
                pParam->flags |= USE_BASE;
                disasmModRMReg16(pCpu, pOp, rm, pParam);
            }
            disasmAddChar(pParam->szParam, ']');
            break;

        case 1: //effective address + 8 bits displacement
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            disasmModRMReg16(pCpu, pOp, rm, pParam);
            pParam->disp8 = pCpu->i32SibDisp;
            pParam->flags |= USE_BASE | USE_DISPLACEMENT8;

            if (pParam->disp8 != 0)
            {
                if (pParam->disp8 > 0)
                    disasmAddChar(pParam->szParam, '+');
                disasmPrintDisp8(pParam);
            }
            disasmAddChar(pParam->szParam, ']');
            break;

        case 2: //effective address + 16 bits displacement
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            disasmModRMReg16(pCpu, pOp, rm, pParam);
            pParam->disp16 = pCpu->i32SibDisp;
            pParam->flags |= USE_BASE | USE_DISPLACEMENT16;

            if (pParam->disp16 != 0)
            {
                disasmAddChar(pParam->szParam, '+');
                disasmPrintDisp16(pParam);
            }
            disasmAddChar(pParam->szParam, ']');
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
unsigned QueryModRM(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, unsigned *pSibInc)
{
    unsigned sibinc;
    unsigned size = 0;
    // unsigned reg = pCpu->ModRM.Bits.Reg;
    unsigned mod = pCpu->ModRM.Bits.Mod;
    unsigned rm  = pCpu->ModRM.Bits.Rm;

    if (!pSibInc)
        pSibInc = &sibinc;

    *pSibInc = 0;

    if (pCpu->addrmode != CPUMODE_16BIT)
    {
        Assert(pCpu->addrmode == CPUMODE_32BIT || pCpu->addrmode == CPUMODE_64BIT);

        /*
         * Note: displacements in long mode are 8 or 32 bits and sign-extended to 64 bits
         */
        if (mod != 3 && rm == 4)
        {   /* SIB byte follows ModRM */
            *pSibInc = ParseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            lpszCodeBlock += *pSibInc;
            size += *pSibInc;
        }

        switch (mod)
        {
        case 0: /* Effective address */
            if (rm == 5) {  /* 32 bits displacement */
                pCpu->i32SibDisp = DISReadDWord(pCpu, lpszCodeBlock);
                size += sizeof(int32_t);
            }
            /* else register address */
            break;

        case 1: /* Effective address + 8 bits displacement */
            pCpu->i32SibDisp = (int8_t)DISReadByte(pCpu, lpszCodeBlock);
            size += sizeof(char);
            break;

        case 2: /* Effective address + 32 bits displacement */
            pCpu->i32SibDisp = DISReadDWord(pCpu, lpszCodeBlock);
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
                pCpu->i32SibDisp = DISReadWord(pCpu, lpszCodeBlock);
                size += sizeof(uint16_t);
            }
            /* else register address */
            break;

        case 1: /* Effective address + 8 bits displacement */
            pCpu->i32SibDisp = (int8_t)DISReadByte(pCpu, lpszCodeBlock);
            size += sizeof(char);
            break;

        case 2: /* Effective address + 32 bits displacement */
            pCpu->i32SibDisp = (int16_t)DISReadWord(pCpu, lpszCodeBlock);
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
unsigned QueryModRM_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, unsigned *pSibInc)
{
    unsigned sibinc;
    unsigned size = 0;
    // unsigned reg = pCpu->ModRM.Bits.Reg;
    unsigned mod = pCpu->ModRM.Bits.Mod;
    unsigned rm  = pCpu->ModRM.Bits.Rm;

    if (!pSibInc)
        pSibInc = &sibinc;

    *pSibInc = 0;

    if (pCpu->addrmode != CPUMODE_16BIT)
    {
        Assert(pCpu->addrmode == CPUMODE_32BIT || pCpu->addrmode == CPUMODE_64BIT);
        /*
         * Note: displacements in long mode are 8 or 32 bits and sign-extended to 64 bits
         */
        if (mod != 3 && rm == 4)
        {   /* SIB byte follows ModRM */
            *pSibInc = ParseSIB_SizeOnly(lpszCodeBlock, pOp, pParam, pCpu);
            lpszCodeBlock += *pSibInc;
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
unsigned ParseIllegal(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    AssertFailed();
    return 0;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseModRM(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);   //ModRM byte
    unsigned sibinc, ModRM;

    ModRM = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += sizeof(uint8_t);

    pCpu->ModRM.Bits.Rm  = MODRM_RM(ModRM);
    pCpu->ModRM.Bits.Mod = MODRM_MOD(ModRM);
    pCpu->ModRM.Bits.Reg = MODRM_REG(ModRM);

    /* Disregard the mod bits for certain instructions (mov crx, mov drx).
     *
     * From the AMD manual:
     * This instruction is always treated as a register-to-register (MOD = 11) instruction, regardless of the
     * encoding of the MOD field in the MODR/M byte.
     */
    if (pOp->optype & OPTYPE_MOD_FIXED_11)
        pCpu->ModRM.Bits.Mod = 3;

    if (pCpu->prefix & PREFIX_REX)
    {
        Assert(pCpu->mode == CPUMODE_64BIT);

        /* REX.R extends the Reg field. */
        pCpu->ModRM.Bits.Reg |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_R)) << 3);

        /* REX.B extends the Rm field if there is no SIB byte nor a 32 bits displacement */
        if (!(    pCpu->ModRM.Bits.Mod != 3
              &&  pCpu->ModRM.Bits.Rm  == 4)
            &&
            !(    pCpu->ModRM.Bits.Mod == 0
              &&  pCpu->ModRM.Bits.Rm  == 5))
        {
            pCpu->ModRM.Bits.Rm |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_B)) << 3);
        }
    }
    size += QueryModRM(lpszCodeBlock, pOp, pParam, pCpu, &sibinc);
    lpszCodeBlock += sibinc;

    UseModRM(lpszCodeBlock, pOp, pParam, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseModRM_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);   //ModRM byte
    unsigned sibinc, ModRM;

    ModRM = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += sizeof(uint8_t);

    pCpu->ModRM.Bits.Rm  = MODRM_RM(ModRM);
    pCpu->ModRM.Bits.Mod = MODRM_MOD(ModRM);
    pCpu->ModRM.Bits.Reg = MODRM_REG(ModRM);

    /* Disregard the mod bits for certain instructions (mov crx, mov drx).
     *
     * From the AMD manual:
     * This instruction is always treated as a register-to-register (MOD = 11) instruction, regardless of the
     * encoding of the MOD field in the MODR/M byte.
     */
    if (pOp->optype & OPTYPE_MOD_FIXED_11)
        pCpu->ModRM.Bits.Mod = 3;

    if (pCpu->prefix & PREFIX_REX)
    {
        Assert(pCpu->mode == CPUMODE_64BIT);

        /* REX.R extends the Reg field. */
        pCpu->ModRM.Bits.Reg |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_R)) << 3);

        /* REX.B extends the Rm field if there is no SIB byte nor a 32 bits displacement */
        if (!(    pCpu->ModRM.Bits.Mod != 3
              &&  pCpu->ModRM.Bits.Rm  == 4)
            &&
            !(    pCpu->ModRM.Bits.Mod == 0
              &&  pCpu->ModRM.Bits.Rm  == 5))
        {
            pCpu->ModRM.Bits.Rm |= ((!!(pCpu->prefix_rex & PREFIX_REX_FLAGS_B)) << 3);
        }
    }

    size += QueryModRM_SizeOnly(lpszCodeBlock, pOp, pParam, pCpu, &sibinc);
    lpszCodeBlock += sibinc;

    /* UseModRM is not necessary here; we're only interested in the opcode size */
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseModFence(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    ////AssertMsgFailed(("??\n"));
    //nothing to do apparently
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return 0;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByte(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->parval = DISReadByte(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE8;
    pParam->size   = sizeof(uint8_t);

    disasmAddStringF1(pParam->szParam, "0%02Xh", (uint32_t)pParam->parval);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByte_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByteSX(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    if (pCpu->opmode == CPUMODE_32BIT)
    {
        pParam->parval = (uint32_t)(int8_t)DISReadByte(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE32_SX8;
        pParam->size   = sizeof(uint32_t);
        disasmAddStringF1(pParam->szParam, "0%08Xh", (uint32_t)pParam->parval);
    }
    else
    if (pCpu->opmode == CPUMODE_64BIT)
    {
        pParam->parval = (uint64_t)(int8_t)DISReadByte(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE64_SX8;
        pParam->size   = sizeof(uint64_t);
        disasmAddStringF1(pParam->szParam, "0%016RX64h", pParam->parval);
    }
    else
    {
        pParam->parval = (uint16_t)(int8_t)DISReadByte(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16_SX8;
        pParam->size   = sizeof(uint16_t);
        disasmAddStringF1(pParam->szParam, "0%04Xh", (uint16_t)pParam->parval);
    }
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByteSX_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUshort(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE16;
    pParam->size   = sizeof(uint16_t);

    disasmAddStringF1(pParam->szParam, "0%04Xh", (uint16_t)pParam->parval);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUshort_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUlong(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE32;
    pParam->size   = sizeof(uint32_t);

    disasmAddStringF1(pParam->szParam, "0%08Xh", (uint32_t)pParam->parval);
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUlong_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmQword(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->parval = DISReadQWord(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE64;
    pParam->size   = sizeof(uint64_t);

    disasmAddStringF2(pParam->szParam, "0%08X%08Xh",
                      (uint32_t)pParam->parval, (uint32_t)(pParam->parval >> 32));
    return sizeof(uint64_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmQword_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(uint64_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmV(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    if (pCpu->opmode == CPUMODE_32BIT)
    {
        pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE32;
        pParam->size   = sizeof(uint32_t);

        disasmAddStringF1(pParam->szParam, "0%08Xh", (uint32_t)pParam->parval);
        return sizeof(uint32_t);
    }
    else
    if (pCpu->opmode == CPUMODE_64BIT)
    {
        pParam->parval = DISReadQWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE64;
        pParam->size   = sizeof(uint64_t);

        disasmAddStringF1(pParam->szParam, "0%RX64h", pParam->parval);
        return sizeof(uint64_t);
    }
    else
    {
        pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16;
        pParam->size   = sizeof(uint16_t);

        disasmAddStringF1(pParam->szParam, "0%04Xh", (uint32_t)pParam->parval);
        return sizeof(uint16_t);
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmV_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam);
    if (pCpu->opmode == CPUMODE_32BIT)
        return sizeof(uint32_t);
    if (pCpu->opmode == CPUMODE_64BIT)
        return sizeof(uint64_t);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmZ(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    /* Word for 16-bit operand-size or doubleword for 32 or 64-bit operand-size. */
    if (pCpu->opmode == CPUMODE_16BIT)
    {
        pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16;
        pParam->size   = sizeof(uint16_t);

        disasmAddStringF1(pParam->szParam, "0%04Xh", (uint32_t)pParam->parval);
        return sizeof(uint16_t);
    }
    else
    {
        /* 64 bits op mode means *sign* extend to 64 bits. */
        if (pCpu->opmode == CPUMODE_64BIT)
        {
            pParam->parval = (uint64_t)(int32_t)DISReadDWord(pCpu, lpszCodeBlock);
            pParam->flags |= USE_IMMEDIATE64;
            pParam->size   = sizeof(uint64_t);
            disasmAddStringF1(pParam->szParam, "0%RX64h", pParam->parval);
        }
        else
        {
            pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
            pParam->flags |= USE_IMMEDIATE32;
            pParam->size   = sizeof(uint32_t);
            disasmAddStringF1(pParam->szParam, "0%08Xh", (uint32_t)pParam->parval);
        }
        return sizeof(uint32_t);
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmZ_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam);
    /* Word for 16-bit operand-size or doubleword for 32 or 64-bit operand-size. */
    if (pCpu->opmode == CPUMODE_16BIT)
        return sizeof(uint16_t);
    return sizeof(uint32_t);
}

//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmBRel(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    pParam->parval = DISReadByte(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE8_REL;
    pParam->size   = sizeof(uint8_t);

    disasmAddStringF1(pParam->szParam, " (0%02Xh)", (uint32_t)pParam->parval);
    return sizeof(char);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmBRel_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam); NOREF(pCpu);
    return sizeof(char);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmVRel(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pOp);
    if (pCpu->opmode == CPUMODE_32BIT)
    {
        pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE32_REL;
        pParam->size   = sizeof(int32_t);

        disasmAddStringF1(pParam->szParam, " (0%08Xh)", (uint32_t)pParam->parval);
        return sizeof(int32_t);
    }
    else
    if (pCpu->opmode == CPUMODE_64BIT)
    {
        /* 32 bits relative immediate sign extended to 64 bits. */
        pParam->parval = (uint64_t)(int32_t)DISReadDWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE64_REL;
        pParam->size   = sizeof(int64_t);

        disasmAddStringF1(pParam->szParam, " (0%RX64h)", pParam->parval);
        return sizeof(int32_t);
    }
    else
    {
        pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16_REL;
        pParam->size   = sizeof(int16_t);

        disasmAddStringF1(pParam->szParam, " (0%04Xh)", (uint32_t)pParam->parval);
        return sizeof(int16_t);
    }
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmVRel_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp); NOREF(pParam);
    if (pCpu->opmode == CPUMODE_16BIT)
        return sizeof(int16_t);
    /* Both 32 & 64 bits mode use 32 bits relative immediates. */
    return sizeof(int32_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmAddr(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmGetPtrString(pCpu, pOp, pParam);
    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
        {// far 16:32 pointer
            pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
            *((uint32_t*)&pParam->parval+1) = DISReadWord(pCpu, lpszCodeBlock+sizeof(uint32_t));
            pParam->flags  |= USE_IMMEDIATE_ADDR_16_32;
            pParam->size   = sizeof(uint16_t) + sizeof(uint32_t);

            disasmAddStringF2(pParam->szParam, "0%04X:0%08Xh", (uint32_t)(pParam->parval>>32), (uint32_t)pParam->parval);
            return sizeof(uint32_t) + sizeof(uint16_t);
        }
        else
        {// near 32 bits pointer
            /*
             * Note: used only in "mov al|ax|eax, [Addr]" and "mov [Addr], al|ax|eax"
             * so we treat it like displacement.
             */
            pParam->disp32 = DISReadDWord(pCpu, lpszCodeBlock);
            pParam->flags |= USE_DISPLACEMENT32;
            pParam->size   = sizeof(uint32_t);

            disasmAddStringF1(pParam->szParam, "[0%08Xh]", pParam->disp32);
            return sizeof(uint32_t);
        }
    }
    else
    if (pCpu->addrmode == CPUMODE_64BIT)
    {
        Assert(OP_PARM_VSUBTYPE(pParam->param) != OP_PARM_p);
        /* near 64 bits pointer */
        /*
         * Note: used only in "mov al|ax|eax, [Addr]" and "mov [Addr], al|ax|eax"
         * so we treat it like displacement.
         */
        pParam->disp64 = DISReadQWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_DISPLACEMENT64;
        pParam->size   = sizeof(uint64_t);

        disasmAddStringF2(pParam->szParam, "[0%08X%08Xh]", (uint32_t)(pParam->disp64 >> 32), (uint32_t)pParam->disp64);
        return sizeof(uint64_t);
    }
    else
    {
        if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
        {// far 16:16 pointer
            pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
            pParam->flags |= USE_IMMEDIATE_ADDR_16_16;
            pParam->size   = 2*sizeof(uint16_t);

            disasmAddStringF2(pParam->szParam, "0%04X:0%04Xh", (uint32_t)(pParam->parval>>16), (uint16_t)pParam->parval );
            return sizeof(uint32_t);
        }
        else
        {// near 16 bits pointer
            /*
             * Note: used only in "mov al|ax|eax, [Addr]" and "mov [Addr], al|ax|eax"
             * so we treat it like displacement.
             */
            pParam->disp16 = DISReadWord(pCpu, lpszCodeBlock);
            pParam->flags |= USE_DISPLACEMENT16;
            pParam->size   = sizeof(uint16_t);

            disasmAddStringF1(pParam->szParam, "[0%04Xh]", (uint32_t)pParam->disp16);
            return sizeof(uint16_t);
        }
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmAddr_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp);
    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
        {// far 16:32 pointer
            return sizeof(uint32_t) + sizeof(uint16_t);
        }
        else
        {// near 32 bits pointer
            return sizeof(uint32_t);
        }
    }
    if (pCpu->addrmode == CPUMODE_64BIT)
    {
        Assert(OP_PARM_VSUBTYPE(pParam->param) != OP_PARM_p);
        return sizeof(uint64_t);
    }
    else
    {
        if (OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
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
unsigned ParseImmAddrF(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmGetPtrString(pCpu, pOp, pParam);
    // immediate far pointers - only 16:16 or 16:32; determined by operand, *not* address size!
    Assert(pCpu->opmode == CPUMODE_16BIT || pCpu->opmode == CPUMODE_32BIT);
    Assert(OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p);
    if (pCpu->opmode == CPUMODE_32BIT)
    {
        // far 16:32 pointer
        pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
        *((uint32_t*)&pParam->parval+1) = DISReadWord(pCpu, lpszCodeBlock+sizeof(uint32_t));
        pParam->flags  |= USE_IMMEDIATE_ADDR_16_32;
        pParam->size   = sizeof(uint16_t) + sizeof(uint32_t);

        disasmAddStringF2(pParam->szParam, "0%04X:0%08Xh", (uint32_t)(pParam->parval>>32), (uint32_t)pParam->parval);
        return sizeof(uint32_t) + sizeof(uint16_t);
    }
    else
    {
        // far 16:16 pointer
        pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE_ADDR_16_16;
        pParam->size   = 2*sizeof(uint16_t);

        disasmAddStringF2(pParam->szParam, "0%04X:0%04Xh", (uint32_t)(pParam->parval>>16), (uint16_t)pParam->parval );
        return sizeof(uint32_t);
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmAddrF_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock); NOREF(pOp);
    // immediate far pointers - only 16:16 or 16:32
    Assert(pCpu->opmode == CPUMODE_16BIT || pCpu->opmode == CPUMODE_32BIT);
    Assert(OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p);
    if (pCpu->opmode == CPUMODE_32BIT)
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
unsigned ParseFixedReg(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(lpszCodeBlock);

    /*
     * Sets up flags for stored in OPC fixed registers.
     */

    if (pParam->param == OP_PARM_NONE)
    {
        /* No parameter at all. */
        return 0;
    }

    AssertCompile(OP_PARM_REG_GEN32_END < OP_PARM_REG_SEG_END);
    AssertCompile(OP_PARM_REG_SEG_END < OP_PARM_REG_GEN16_END);
    AssertCompile(OP_PARM_REG_GEN16_END < OP_PARM_REG_GEN8_END);
    AssertCompile(OP_PARM_REG_GEN8_END < OP_PARM_REG_FP_END);

    if (pParam->param <= OP_PARM_REG_GEN32_END)
    {
        /* 32-bit EAX..EDI registers. */
        if (pCpu->opmode == CPUMODE_32BIT)
        {
            /* Use 32-bit registers. */
            pParam->base.reg_gen = pParam->param - OP_PARM_REG_GEN32_START;
            pParam->flags |= USE_REG_GEN32;
            pParam->size   = 4;
        }
        else
        if (pCpu->opmode == CPUMODE_64BIT)
        {
            /* Use 64-bit registers. */
            pParam->base.reg_gen = pParam->param - OP_PARM_REG_GEN32_START;
            if (    (pOp->optype & OPTYPE_REXB_EXTENDS_OPREG)
                &&  pParam == &pCpu->param1             /* ugly assumption that it only applies to the first parameter */
                &&  (pCpu->prefix & PREFIX_REX)
                &&  (pCpu->prefix_rex & PREFIX_REX_FLAGS))
                pParam->base.reg_gen += 8;

            pParam->flags |= USE_REG_GEN64;
            pParam->size   = 8;
        }
        else
        {
            /* Use 16-bit registers. */
            pParam->base.reg_gen = pParam->param - OP_PARM_REG_GEN32_START;
            pParam->flags |= USE_REG_GEN16;
            pParam->size   = 2;
            pParam->param = pParam->param - OP_PARM_REG_GEN32_START + OP_PARM_REG_GEN16_START;
        }
    }
    else
    if (pParam->param <= OP_PARM_REG_SEG_END)
    {
        /* Segment ES..GS registers. */
        pParam->base.reg_seg = (DIS_SELREG)(pParam->param - OP_PARM_REG_SEG_START);
        pParam->flags |= USE_REG_SEG;
        pParam->size   = 2;
    }
    else
    if (pParam->param <= OP_PARM_REG_GEN16_END)
    {
        /* 16-bit AX..DI registers. */
        pParam->base.reg_gen = pParam->param - OP_PARM_REG_GEN16_START;
        pParam->flags |= USE_REG_GEN16;
        pParam->size   = 2;
    }
    else
    if (pParam->param <= OP_PARM_REG_GEN8_END)
    {
        /* 8-bit AL..DL, AH..DH registers. */
        pParam->base.reg_gen = pParam->param - OP_PARM_REG_GEN8_START;
        pParam->flags |= USE_REG_GEN8;
        pParam->size   = 1;

        if (pCpu->opmode == CPUMODE_64BIT)
        {
            if (    (pOp->optype & OPTYPE_REXB_EXTENDS_OPREG)
                &&  pParam == &pCpu->param1             /* ugly assumption that it only applies to the first parameter */
                &&  (pCpu->prefix & PREFIX_REX)
                &&  (pCpu->prefix_rex & PREFIX_REX_FLAGS))
                pParam->base.reg_gen += 8;              /* least significant byte of R8-R15 */
        }
    }
    else
    if (pParam->param <= OP_PARM_REG_FP_END)
    {
        /* FPU registers. */
        pParam->base.reg_fp = pParam->param - OP_PARM_REG_FP_START;
        pParam->flags |= USE_REG_FP;
        pParam->size   = 10;
    }
    Assert(!(pParam->param >= OP_PARM_REG_GEN64_START && pParam->param <= OP_PARM_REG_GEN64_END));

    /* else - not supported for now registers. */

    return 0;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseXv(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pu8CodeBlock);
    disasmGetPtrString(pCpu, pOp, pParam);
    disasmAddString(pParam->szParam, (pCpu->addrmode == CPUMODE_32BIT) ? "DS:ESI" : "DS:SI");

    pParam->flags |= USE_POINTER_DS_BASED;
    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen = USE_REG_ESI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    if (pCpu->addrmode == CPUMODE_64BIT)
    {
        pParam->base.reg_gen = USE_REG_RSI;
        pParam->flags |= USE_REG_GEN64;
    }
    else
    {
        pParam->base.reg_gen = USE_REG_SI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseXb(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pu8CodeBlock); NOREF(pOp);
    disasmAddString(pParam->szParam, (pCpu->addrmode == CPUMODE_32BIT) ? "DS:ESI" : "DS:SI");

    pParam->flags |= USE_POINTER_DS_BASED;
    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen = USE_REG_ESI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    if (pCpu->addrmode == CPUMODE_64BIT)
    {
        pParam->base.reg_gen = USE_REG_RSI;
        pParam->flags |= USE_REG_GEN64;
    }
    else
    {
        pParam->base.reg_gen = USE_REG_SI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseYv(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pu8CodeBlock);
    disasmGetPtrString(pCpu, pOp, pParam);
    disasmAddString(pParam->szParam, (pCpu->addrmode == CPUMODE_32BIT) ? "ES:EDI" : "ES:DI");

    pParam->flags |= USE_POINTER_ES_BASED;
    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen = USE_REG_EDI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    if (pCpu->addrmode == CPUMODE_64BIT)
    {
        pParam->base.reg_gen = USE_REG_RDI;
        pParam->flags |= USE_REG_GEN64;
    }
    else
    {
        pParam->base.reg_gen = USE_REG_DI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseYb(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    NOREF(pu8CodeBlock); NOREF(pOp);
    disasmAddString(pParam->szParam, (pCpu->addrmode == CPUMODE_32BIT) ? "ES:EDI" : "ES:DI");

    pParam->flags |= USE_POINTER_ES_BASED;
    if (pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen = USE_REG_EDI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    if (pCpu->addrmode == CPUMODE_64BIT)
    {
        pParam->base.reg_gen = USE_REG_RDI;
        pParam->flags |= USE_REG_GEN64;
    }
    else
    {
        pParam->base.reg_gen = USE_REG_DI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseTwoByteEsc(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    const OPCODE *pOpcode;
    int           size    = sizeof(uint8_t);
    NOREF(pOp); NOREF(pParam);

    /* 2nd byte */
    pCpu->opcode = DISReadByte(pCpu, lpszCodeBlock);

    /* default to the non-prefixed table. */
    pOpcode      = &g_aTwoByteMapX86[pCpu->opcode];

    /* Handle opcode table extensions that rely on the address, repe or repne prefix byte.  */
    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    if (pCpu->lastprefix)
    {
        switch (pCpu->lastprefix)
        {
        case OP_OPSIZE: /* 0x66 */
            if (g_aTwoByteMapX86_PF66[pCpu->opcode].opcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */
                pOpcode = &g_aTwoByteMapX86_PF66[pCpu->opcode];

                /* Cancel prefix changes. */
                pCpu->prefix &= ~PREFIX_OPSIZE;
                pCpu->opmode  = pCpu->mode;
            }
            break;

        case OP_REPNE:   /* 0xF2 */
            if (g_aTwoByteMapX86_PFF2[pCpu->opcode].opcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */
                pOpcode = &g_aTwoByteMapX86_PFF2[pCpu->opcode];

                /* Cancel prefix changes. */
                pCpu->prefix &= ~PREFIX_REPNE;
            }
            break;

        case OP_REPE:  /* 0xF3 */
            if (g_aTwoByteMapX86_PFF3[pCpu->opcode].opcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */
                pOpcode = &g_aTwoByteMapX86_PFF3[pCpu->opcode];

                /* Cancel prefix changes. */
                pCpu->prefix &= ~PREFIX_REP;
            }
            break;
        }
    }

    size += ParseInstruction(lpszCodeBlock+size, pOpcode, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseThreeByteEsc4(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    const OPCODE *pOpcode;
    int           size    = sizeof(uint8_t);
    NOREF(pOp); NOREF(pParam);

    /* 3rd byte */
    pCpu->opcode = DISReadByte(pCpu, lpszCodeBlock);

    /* default to the non-prefixed table. */
    if (g_apThreeByteMapX86_0F38[pCpu->opcode >> 4])
    {
        pOpcode = g_apThreeByteMapX86_0F38[pCpu->opcode >> 4];
        pOpcode = &pOpcode[pCpu->opcode & 0xf];
    }
    else
        pOpcode = &g_InvalidOpcode[0];

    /* Handle opcode table extensions that rely on the address, repne prefix byte.  */
    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    switch (pCpu->lastprefix)
    {
    case OP_OPSIZE: /* 0x66 */
        if (g_apThreeByteMapX86_660F38[pCpu->opcode >> 4])
        {
            pOpcode = g_apThreeByteMapX86_660F38[pCpu->opcode >> 4];
            pOpcode = &pOpcode[pCpu->opcode & 0xf];

            if (pOpcode->opcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */

                /* Cancel prefix changes. */
                pCpu->prefix &= ~PREFIX_OPSIZE;
                pCpu->opmode  = pCpu->mode;
            }
        }
        break;

    case OP_REPNE:   /* 0xF2 */
        if (g_apThreeByteMapX86_F20F38[pCpu->opcode >> 4])
        {
            pOpcode = g_apThreeByteMapX86_F20F38[pCpu->opcode >> 4];
            pOpcode = &pOpcode[pCpu->opcode & 0xf];

            if (pOpcode->opcode != OP_INVALID)
            {
                /* Table entry is valid, so use the extension table. */

                /* Cancel prefix changes. */
                pCpu->prefix &= ~PREFIX_REPNE;
            }
        }
        break;
    }

    size += ParseInstruction(lpszCodeBlock+size, pOpcode, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseThreeByteEsc5(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    const OPCODE *pOpcode;
    int           size    = sizeof(uint8_t);
    NOREF(pOp); NOREF(pParam);

    /* 3rd byte */
    pCpu->opcode = DISReadByte(pCpu, lpszCodeBlock);

    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    Assert(pCpu->lastprefix == OP_OPSIZE);

    /* default to the non-prefixed table. */
    if (g_apThreeByteMapX86_660F3A[pCpu->opcode >> 4])
    {
        pOpcode = g_apThreeByteMapX86_660F3A[pCpu->opcode >> 4];
        pOpcode = &pOpcode[pCpu->opcode & 0xf];

        if (pOpcode->opcode != OP_INVALID)
        {
            /* Table entry is valid, so use the extension table. */

            /* Cancel prefix changes. */
            pCpu->prefix &= ~PREFIX_OPSIZE;
            pCpu->opmode  = pCpu->mode;
        }
    }
    else
        pOpcode = &g_InvalidOpcode[0];

    size += ParseInstruction(lpszCodeBlock+size, pOpcode, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseNopPause(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0;
    NOREF(pParam);

    if (pCpu->prefix & PREFIX_REP)
    {
        pOp = &g_aMapX86_NopPause[1]; /* PAUSE */
        pCpu->prefix &= ~PREFIX_REP;
    }
    else
        pOp = &g_aMapX86_NopPause[0]; /* NOP */

    size += ParseInstruction(pu8CodeBlock, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmGrpl(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int idx = (pCpu->opcode - 0x80) * 8;
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group1[idx+reg];
    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseShiftGrp2(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int idx;
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    switch (pCpu->opcode)
    {
    case 0xC0:
    case 0xC1:
        idx = (pCpu->opcode - 0xC0)*8;
        break;

    case 0xD0:
    case 0xD1:
    case 0xD2:
    case 0xD3:
        idx = (pCpu->opcode - 0xD0 + 2)*8;
        break;

    default:
        AssertMsgFailed(("Oops\n"));
        return sizeof(uint8_t);
    }

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group2[idx+reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp3(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int idx = (pCpu->opcode - 0xF6) * 8;
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group3[idx+reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp4(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group4[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp5(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group5[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
// 0xF 0xF [ModRM] [SIB] [displacement] imm8_opcode
// It would appear the ModRM byte must always be present. How else can you
// determine the offset of the imm8_opcode byte otherwise?
//
//*****************************************************************************
unsigned Parse3DNow(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrmsize;

#ifdef DEBUG_Sander
    //needs testing
    AssertMsgFailed(("Test me\n"));
#endif

    unsigned ModRM = DISReadByte(pCpu, lpszCodeBlock);
    pCpu->ModRM.Bits.Rm  = MODRM_RM(ModRM);
    pCpu->ModRM.Bits.Mod = MODRM_MOD(ModRM);
    pCpu->ModRM.Bits.Reg = MODRM_REG(ModRM);

    modrmsize = QueryModRM(lpszCodeBlock+sizeof(uint8_t), pOp, pParam, pCpu);

    uint8_t opcode = DISReadByte(pCpu, lpszCodeBlock+sizeof(uint8_t)+modrmsize);

    pOp = (PCOPCODE)&g_aTwoByteMapX86_3DNow[opcode];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
#ifdef DEBUG_Sander /* bird, 2005-06-28: Alex is getting this during full installation of win2ksp4. */
        AssertMsgFailed(("Oops!\n")); //shouldn't happen!
#endif
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);
    size += sizeof(uint8_t);   //imm8_opcode uint8_t

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp6(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group6[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp7(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg, rm, mod;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    mod   = MODRM_MOD(modrm);
    reg   = MODRM_REG(modrm);
    rm    = MODRM_RM(modrm);

    if (mod == 3 && rm == 0)
        pOp = (PCOPCODE)&g_aMapX86_Group7_mod11_rm000[reg];
    else
    if (mod == 3 && rm == 1)
        pOp = (PCOPCODE)&g_aMapX86_Group7_mod11_rm001[reg];
    else
        pOp = (PCOPCODE)&g_aMapX86_Group7_mem[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp8(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group8[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp9(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group9[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp10(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group10[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp12(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    if (pCpu->prefix & PREFIX_OPSIZE)
        reg += 8;   //2nd table

    pOp = (PCOPCODE)&g_aMapX86_Group12[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp13(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);
    if (pCpu->prefix & PREFIX_OPSIZE)
        reg += 8;   //2nd table

    pOp = (PCOPCODE)&g_aMapX86_Group13[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp14(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);
    if (pCpu->prefix & PREFIX_OPSIZE)
        reg += 8;   //2nd table

    pOp = (PCOPCODE)&g_aMapX86_Group14[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp15(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg, mod, rm;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    mod   = MODRM_MOD(modrm);
    reg   = MODRM_REG(modrm);
    rm    = MODRM_RM(modrm);

    if (mod == 3 && rm == 0)
        pOp = (PCOPCODE)&g_aMapX86_Group15_mod11_rm000[reg];
    else
        pOp = (PCOPCODE)&g_aMapX86_Group15_mem[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp16(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;
    NOREF(pParam);

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group16[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if (pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
        size = sizeof(uint8_t); //ModRM byte

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);
    return size;
}
//*****************************************************************************
#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED)
static const char *szModRMReg8[]      = {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH", "R8B", "R9B", "R10B", "R11B", "R12B", "R13B", "R14B", "R15B", "SPL", "BPL", "SIL", "DIL"};
static const char *szModRMReg16[]     = {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "R8W", "R9W", "R10W", "R11W", "R12W", "R13W", "R14W", "R15W"};
static const char *szModRMReg32[]     = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI", "R8D", "R9D", "R10D", "R11D", "R12D", "R13D", "R14D", "R15D"};
static const char *szModRMReg64[]     = {"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"};
static const char *szModRMReg1616[8]  = {"BX+SI", "BX+DI", "BP+SI", "BP+DI", "SI", "DI", "BP", "BX"};
#endif
static const char *szModRMSegReg[6]   = {"ES", "CS", "SS", "DS", "FS", "GS"};
static const int   BaseModRMReg16[8]  = { USE_REG_BX, USE_REG_BX, USE_REG_BP, USE_REG_BP, USE_REG_SI, USE_REG_DI, USE_REG_BP, USE_REG_BX};
static const int   IndexModRMReg16[4] = { USE_REG_SI, USE_REG_DI, USE_REG_SI, USE_REG_DI};
//*****************************************************************************
void disasmModRMReg(PDISCPUSTATE pCpu, PCOPCODE pOp, unsigned idx, POP_PARAMETER pParam, int fRegAddr)
{
    int subtype, type, mod;
    NOREF(pOp); NOREF(pCpu);

    mod     = pCpu->ModRM.Bits.Mod;

    type    = OP_PARM_VTYPE(pParam->param);
    subtype = OP_PARM_VSUBTYPE(pParam->param);
    if (fRegAddr)
        subtype = (pCpu->addrmode == CPUMODE_64BIT) ? OP_PARM_q : OP_PARM_d;
    else
    if (subtype == OP_PARM_v || subtype == OP_PARM_NONE)
    {
        switch(pCpu->opmode)
        {
        case CPUMODE_32BIT:
            subtype = OP_PARM_d;
            break;
        case CPUMODE_64BIT:
            subtype = OP_PARM_q;
            break;
        case CPUMODE_16BIT:
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
        Assert(idx < (pCpu->prefix & PREFIX_REX) ? 16 : 8);

        /* AH, BH, CH & DH map to DIL, SIL, EBL & SPL when a rex prefix is present. */
        /* Intel® 64 and IA-32 Architectures Software Developer’s Manual: 3.4.1.1 */
        if (    (pCpu->prefix & PREFIX_REX)
            &&  idx >= USE_REG_AH
            &&  idx <= USE_REG_BH)
        {
            idx += (USE_REG_SPL - USE_REG_AH);
        }
        disasmAddString(pParam->szParam, szModRMReg8[idx]);

        pParam->flags |= USE_REG_GEN8;
        pParam->base.reg_gen = idx;
        break;

    case OP_PARM_w:
        disasmAddString(pParam->szParam, szModRMReg16[idx]);
        Assert(idx < (pCpu->prefix & PREFIX_REX) ? 16 : 8);

        pParam->flags |= USE_REG_GEN16;
        pParam->base.reg_gen = idx;
        break;

    case OP_PARM_d:
        disasmAddString(pParam->szParam, szModRMReg32[idx]);
        Assert(idx < (pCpu->prefix & PREFIX_REX) ? 16 : 8);

        pParam->flags |= USE_REG_GEN32;
        pParam->base.reg_gen = idx;
        break;

    case OP_PARM_q:
        disasmAddString(pParam->szParam, szModRMReg64[idx]);
        pParam->flags |= USE_REG_GEN64;
        pParam->base.reg_gen = idx;
        break;

    default:
        Log(("disasmModRMReg %x:%x failed!!\n", type, subtype));
        pCpu->rc = VERR_DIS_INVALID_MODRM;
        break;
    }
}
//*****************************************************************************
//*****************************************************************************
void disasmModRMReg16(PDISCPUSTATE pCpu, PCOPCODE pOp, unsigned idx, POP_PARAMETER pParam)
{
    NOREF(pCpu); NOREF(pOp);
    disasmAddString(pParam->szParam, szModRMReg1616[idx]);
    pParam->flags |= USE_REG_GEN16;
    pParam->base.reg_gen = BaseModRMReg16[idx];
    if (idx < 4)
    {
        pParam->flags |= USE_INDEX;
        pParam->index.reg_gen = IndexModRMReg16[idx];
    }
}
//*****************************************************************************
//*****************************************************************************
void disasmModRMSReg(PDISCPUSTATE pCpu, PCOPCODE pOp, unsigned idx, POP_PARAMETER pParam)
{
    NOREF(pOp);
    if (idx >= RT_ELEMENTS(szModRMSegReg))
    {
        Log(("disasmModRMSReg %d failed!!\n", idx));
        pCpu->rc = VERR_DIS_INVALID_PARAMETER;
        return;
    }

    disasmAddString(pParam->szParam, szModRMSegReg[idx]);
    pParam->flags |= USE_REG_SEG;
    pParam->base.reg_seg = (DIS_SELREG)idx;
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintAbs32(POP_PARAMETER pParam)
{
    disasmAddStringF1(pParam->szParam, "%08Xh", pParam->disp32); NOREF(pParam);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp32(POP_PARAMETER pParam)
{
    disasmAddStringF1(pParam->szParam, "%08Xh", pParam->disp32); NOREF(pParam);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp64(POP_PARAMETER pParam)
{
    disasmAddStringF1(pParam->szParam, "%16RX64h", pParam->disp64); NOREF(pParam);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp8(POP_PARAMETER pParam)
{
    disasmAddStringF1(pParam->szParam, "%d", pParam->disp8); NOREF(pParam);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp16(POP_PARAMETER pParam)
{
    disasmAddStringF1(pParam->szParam, "%04Xh", pParam->disp16); NOREF(pParam);
}
//*****************************************************************************
//*****************************************************************************
void disasmGetPtrString(PDISCPUSTATE pCpu, PCOPCODE pOp, POP_PARAMETER pParam)
{
    int subtype = OP_PARM_VSUBTYPE(pParam->param);
    NOREF(pOp);

    if (subtype == OP_PARM_v)
    {
        switch(pCpu->opmode)
        {
        case CPUMODE_32BIT:
            subtype = OP_PARM_d;
            break;
        case CPUMODE_64BIT:
            subtype = OP_PARM_q;
            break;
        case CPUMODE_16BIT:
            subtype = OP_PARM_w;
            break;
        default:
            /* make gcc happy */
            break;
        }
    }

    switch (subtype)
    {
    case OP_PARM_a: //two words or dwords depending on operand size (bound only)
        break;

    case OP_PARM_b:
        disasmAddString(pParam->szParam, "byte ptr ");
        break;

    case OP_PARM_w:
        disasmAddString(pParam->szParam, "word ptr ");
        break;

    case OP_PARM_d:
        disasmAddString(pParam->szParam, "dword ptr ");
        break;

    case OP_PARM_q:
    case OP_PARM_dq:
        disasmAddString(pParam->szParam, "qword ptr ");
        break;

    case OP_PARM_p:
        disasmAddString(pParam->szParam, "far ptr ");
        break;

    case OP_PARM_s:
        break; //??

    case OP_PARM_z:
        break;
    default:
        break; //no pointer type specified/necessary
    }
    if (pCpu->prefix & PREFIX_SEG)
        disasmAddStringF1(pParam->szParam, "%s:", szModRMSegReg[pCpu->enmPrefixSeg]);
}


/**
 * Slow path for storing instruction bytes.
 *
 * @param   pCpu                The disassembler state.
 * @param   uAddress            The address.
 * @param   pbSrc               The bytes.
 * @param   cbSrc               The number of bytes.
 */
DECL_NO_INLINE(static, void)
disStoreInstrBytesSlow(PDISCPUSTATE pCpu, RTUINTPTR uAddress, const uint8_t *pbSrc, size_t cbSrc)
{
    /*
     * Figure out which case it is.
     */
    uint32_t  cbInstr = pCpu->opsize;
    RTUINTPTR off     = uAddress - pCpu->uInstrAddr;
    if (off < cbInstr)
    {
        if (off + cbSrc <= cbInstr)
        {
            AssertMsg(memcmp(&pCpu->abInstr[off], pbSrc, cbSrc) == 0,
                      ("%RTptr LB %zx off=%RTptr (%.*Rhxs)", uAddress, cbSrc, off, cbInstr, pCpu->abInstr));
            return; /* fully re-reading old stuff. */
        }

        /* Only partially re-reading stuff, skip ahead and add the rest. */
        uint32_t cbAlreadyRead = cbInstr - (uint32_t)off;
        Assert(memcmp(&pCpu->abInstr[off], pbSrc, cbAlreadyRead) == 0);
        uAddress += cbAlreadyRead;
        pbSrc    += cbAlreadyRead;
        cbSrc    -= cbAlreadyRead;
    }

    if (off >= sizeof(cbInstr))
    {
        /* The instruction is too long! This shouldn't happen. */
        AssertMsgFailed(("%RTptr LB %zx off=%RTptr (%.*Rhxs)", uAddress, cbSrc, off, cbInstr, pCpu->abInstr));
        return;
    }
    else if (off > cbInstr)
    {
        /* Mind the gap - this shouldn't happen, but read the gap bytes if it does. */
        AssertMsgFailed(("%RTptr LB %zx off=%RTptr (%.16Rhxs)", uAddress, cbSrc, off, cbInstr, pCpu->abInstr));
        uint32_t cbGap = off - cbInstr;
        int rc = pCpu->pfnReadBytes(pCpu, &pCpu->abInstr[cbInstr], uAddress - cbGap, cbGap);
        if (RT_FAILURE(rc))
        {
            pCpu->rc = VERR_DIS_MEM_READ;
            RT_BZERO(&pCpu->abInstr[cbInstr], cbGap);
        }
        pCpu->opsize = cbInstr = off;
    }

    /*
     * Copy the bytes.
     */
    if (off + cbSrc <= sizeof(pCpu->abInstr))
    {
        memcpy(&pCpu->abInstr[cbInstr], pbSrc, cbSrc);
        pCpu->opsize = cbInstr + cbSrc;
    }
    else
    {
        uint32_t cbToCopy = sizeof(pCpu->abInstr) - off;
        memcpy(&pCpu->abInstr[cbInstr], pbSrc, cbToCopy);
        pCpu->opsize = sizeof(pCpu->abInstr);
        AssertMsgFailed(("%RTptr LB %zx off=%RTptr (%.*Rhxs)", uAddress, cbSrc, off, sizeof(pCpu->abInstr), pCpu->abInstr));
    }
}

DECLCALLBACK(int) disReadBytesDefault(PDISCPUSTATE pCpu, uint8_t *pbDst, RTUINTPTR uSrcAddr, uint32_t cbToRead)
{
#ifdef IN_RING0
    AssertMsgFailed(("DISReadWord with no read callback in ring 0!!\n"));
    RT_BZERO(pbDst, cbToRead);
    return VERR_DIS_NO_READ_CALLBACK;
#else
    memcpy(pbDst, (void const *)(uintptr_t)uSrcAddr, cbToRead);
    return VINF_SUCCESS;
#endif
}

//*****************************************************************************
/* Read functions for getting the opcode bytes */
//*****************************************************************************
uint8_t DISReadByte(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    uint8_t bTemp = 0;
    int rc = pCpu->pfnReadBytes(pCpu, &bTemp, uAddress, sizeof(bTemp));
    if (RT_FAILURE(rc))
    {
        Log(("DISReadByte failed!!\n"));
        pCpu->rc = VERR_DIS_MEM_READ;
    }

/** @todo change this into reading directly into abInstr and use it as a
 *        cache. */
    if (RT_LIKELY(   pCpu->uInstrAddr + pCpu->opsize == uAddress
                  && pCpu->opsize + sizeof(bTemp) < sizeof(pCpu->abInstr)))
        pCpu->abInstr[pCpu->opsize++] = bTemp;
    else
        disStoreInstrBytesSlow(pCpu, uAddress, &bTemp, sizeof(bTemp));

    return bTemp;
}
//*****************************************************************************
//*****************************************************************************
uint16_t DISReadWord(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    RTUINT16U uTemp;
    uTemp.u = 0;
    int rc = pCpu->pfnReadBytes(pCpu, uTemp.au8, uAddress, sizeof(uTemp));
    if (RT_FAILURE(rc))
    {
        Log(("DISReadWord failed!!\n"));
        pCpu->rc = VERR_DIS_MEM_READ;
    }

    if (RT_LIKELY(   pCpu->uInstrAddr + pCpu->opsize == uAddress
                  && pCpu->opsize + sizeof(uTemp) < sizeof(pCpu->abInstr)))
    {
        pCpu->abInstr[pCpu->opsize    ] = uTemp.au8[0];
        pCpu->abInstr[pCpu->opsize + 1] = uTemp.au8[1];
        pCpu->opsize += 2;
    }
    else
        disStoreInstrBytesSlow(pCpu, uAddress, uTemp.au8, sizeof(uTemp));

    return uTemp.u;
}
//*****************************************************************************
//*****************************************************************************
uint32_t DISReadDWord(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    RTUINT32U uTemp;
    uTemp.u = 0;
    int rc = pCpu->pfnReadBytes(pCpu, uTemp.au8, uAddress, sizeof(uTemp));
    if (RT_FAILURE(rc))
    {
        Log(("DISReadDWord failed!!\n"));
        pCpu->rc = VERR_DIS_MEM_READ;
    }

    if (RT_LIKELY(   pCpu->uInstrAddr + pCpu->opsize == uAddress
                  && pCpu->opsize + sizeof(uTemp) < sizeof(pCpu->abInstr)))
    {
        pCpu->abInstr[pCpu->opsize    ] = uTemp.au8[0];
        pCpu->abInstr[pCpu->opsize + 1] = uTemp.au8[1];
        pCpu->abInstr[pCpu->opsize + 2] = uTemp.au8[2];
        pCpu->abInstr[pCpu->opsize + 3] = uTemp.au8[3];
        pCpu->opsize += 4;
    }
    else
        disStoreInstrBytesSlow(pCpu, uAddress, uTemp.au8, sizeof(uTemp));

    return uTemp.u;
}
//*****************************************************************************
//*****************************************************************************
uint64_t DISReadQWord(PDISCPUSTATE pCpu, RTUINTPTR uAddress)
{
    RTUINT64U uTemp;
    uTemp.u = 0;
    int rc = pCpu->pfnReadBytes(pCpu, uTemp.au8, uAddress, sizeof(uTemp));
    if (RT_FAILURE(rc))
    {
        Log(("DISReadQWord %x failed!!\n", uAddress));
        pCpu->rc = VERR_DIS_MEM_READ;
    }

    if (RT_LIKELY(   pCpu->uInstrAddr + pCpu->opsize == uAddress
                  && pCpu->opsize + sizeof(uTemp) < sizeof(pCpu->abInstr)))
    {
        pCpu->abInstr[pCpu->opsize    ] = uTemp.au8[0];
        pCpu->abInstr[pCpu->opsize + 1] = uTemp.au8[1];
        pCpu->abInstr[pCpu->opsize + 2] = uTemp.au8[2];
        pCpu->abInstr[pCpu->opsize + 3] = uTemp.au8[3];
        pCpu->abInstr[pCpu->opsize + 4] = uTemp.au8[4];
        pCpu->abInstr[pCpu->opsize + 5] = uTemp.au8[5];
        pCpu->abInstr[pCpu->opsize + 6] = uTemp.au8[6];
        pCpu->abInstr[pCpu->opsize + 7] = uTemp.au8[7];
        pCpu->opsize += 4;
    }
    else
        disStoreInstrBytesSlow(pCpu, uAddress, uTemp.au8, sizeof(uTemp));

    return uTemp.u;
}

#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED)
//*****************************************************************************
//*****************************************************************************
void disasmAddString(char *psz, const char *pszAdd)
{
    strcat(psz, pszAdd);
}
//*****************************************************************************
//*****************************************************************************
void disasmAddStringF(char *psz, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    size_t  cchCur = strlen(psz);
    Assert(cchCur < RT_SIZEOFMEMB(OP_PARAMETER, szParam));
    RTStrPrintfV(psz + cchCur, RT_SIZEOFMEMB(OP_PARAMETER, szParam) - cchCur,
                 pszFormat, args);
    va_end(args);
}

//*****************************************************************************
//*****************************************************************************
void disasmAddChar(char *psz, char ch)
{
    char sz[2];

    sz[0] = ch;
    sz[1] = '\0';
    strcat(psz, sz);
}
#endif /* !DIS_CORE_ONLY */


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
void disValidateLockSequence(PDISCPUSTATE pCpu)
{
    Assert(pCpu->prefix & PREFIX_LOCK);

    /*
     * Filter out the valid lock sequences.
     */
    switch (pCpu->pCurInstr->opcode)
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
            if (pCpu->param1.flags & (USE_BASE | USE_INDEX | USE_DISPLACEMENT64 | USE_DISPLACEMENT32 | USE_DISPLACEMENT16 | USE_DISPLACEMENT8 | USE_RIPDISPLACEMENT32))
                return;
            break;

        default:
            break;
    }

    /*
     * Invalid lock sequence, make it a OP_ILLUD2.
     */
    pCpu->pCurInstr = &g_aTwoByteMapX86[11];
    Assert(pCpu->pCurInstr->opcode == OP_ILLUD2);
}

