/** @file
 *
 * VBox disassembler:
 * Core components
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#include <VBox/cpum.h>
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
static int disCoreOne(PDISCPUSTATE pCpu, RTUINTPTR InstructionAddr, unsigned *pcbInstruction);
#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED)
static void disasmAddString(char *psz, const char *pszString);
static void disasmAddStringF(char *psz, uint32_t cbString, const char *pszFormat, ...);
static void disasmAddChar(char *psz, char ch);
#else
# define disasmAddString(psz, pszString)        do {} while (0)
# ifdef _MSC_VER
#  define disasmAddStringF __noop
# else
#  define disasmAddStringF(psz, cbString, pszFormat...)   do {} while (0)  /* Arg wanna get rid of that warning */
# endif
# define disasmAddChar(psz, ch)                 do {} while (0)
#endif

static unsigned QueryModRM(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, int *pSibInc = NULL);
static unsigned QueryModRM_SizeOnly(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, int *pSibInc = NULL);
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
    ParseImmByteSX
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
    ParseImmByteSX_SizeOnly
};

/**
 * Array for accessing 32-bit general registers in VMMREGFRAME structure
 * by register's index from disasm.
 */
static const unsigned g_aReg32Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, eax),        /* USE_REG_EAX */
    RT_OFFSETOF(CPUMCTXCORE, ecx),        /* USE_REG_ECX */
    RT_OFFSETOF(CPUMCTXCORE, edx),        /* USE_REG_EDX */
    RT_OFFSETOF(CPUMCTXCORE, ebx),        /* USE_REG_EBX */
    RT_OFFSETOF(CPUMCTXCORE, esp),        /* USE_REG_ESP */
    RT_OFFSETOF(CPUMCTXCORE, ebp),        /* USE_REG_EBP */
    RT_OFFSETOF(CPUMCTXCORE, esi),        /* USE_REG_ESI */
    RT_OFFSETOF(CPUMCTXCORE, edi)         /* USE_REG_EDI */
};

/**
 * Macro for accessing 32-bit general purpose registers in CPUMCTXCORE structure.
 */
#define DIS_READ_REG32(p, idx)       (*(uint32_t *)((char *)(p) + g_aReg32Index[idx]))
#define DIS_WRITE_REG32(p, idx, val) (*(uint32_t *)((char *)(p) + g_aReg32Index[idx]) = val)
#define DIS_PTR_REG32(p, idx)        ( (uint32_t *)((char *)(p) + g_aReg32Index[idx]))

/**
 * Array for accessing 16-bit general registers in CPUMCTXCORE structure
 * by register's index from disasm.
 */
static const unsigned g_aReg16Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, eax),        /* USE_REG_AX */
    RT_OFFSETOF(CPUMCTXCORE, ecx),        /* USE_REG_CX */
    RT_OFFSETOF(CPUMCTXCORE, edx),        /* USE_REG_DX */
    RT_OFFSETOF(CPUMCTXCORE, ebx),        /* USE_REG_BX */
    RT_OFFSETOF(CPUMCTXCORE, esp),        /* USE_REG_SP */
    RT_OFFSETOF(CPUMCTXCORE, ebp),        /* USE_REG_BP */
    RT_OFFSETOF(CPUMCTXCORE, esi),        /* USE_REG_SI */
    RT_OFFSETOF(CPUMCTXCORE, edi)         /* USE_REG_DI */
};

/**
 * Macro for accessing 16-bit general purpose registers in CPUMCTXCORE structure.
 */
#define DIS_READ_REG16(p, idx)          (*(uint16_t *)((char *)(p) + g_aReg16Index[idx]))
#define DIS_WRITE_REG16(p, idx, val)    (*(uint16_t *)((char *)(p) + g_aReg16Index[idx]) = val)
#define DIS_PTR_REG16(p, idx)           ( (uint16_t *)((char *)(p) + g_aReg16Index[idx]))

/**
 * Array for accessing 8-bit general registers in CPUMCTXCORE structure
 * by register's index from disasm.
 */
static const unsigned g_aReg8Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, eax),        /* USE_REG_AL */
    RT_OFFSETOF(CPUMCTXCORE, ecx),        /* USE_REG_CL */
    RT_OFFSETOF(CPUMCTXCORE, edx),        /* USE_REG_DL */
    RT_OFFSETOF(CPUMCTXCORE, ebx),        /* USE_REG_BL */
    RT_OFFSETOF(CPUMCTXCORE, eax) + 1,    /* USE_REG_AH */
    RT_OFFSETOF(CPUMCTXCORE, ecx) + 1,    /* USE_REG_CH */
    RT_OFFSETOF(CPUMCTXCORE, edx) + 1,    /* USE_REG_DH */
    RT_OFFSETOF(CPUMCTXCORE, ebx) + 1     /* USE_REG_BH */
};

/**
 * Macro for accessing 8-bit general purpose registers in CPUMCTXCORE structure.
 */
#define DIS_READ_REG8(p, idx)           (*(uint8_t *)((char *)(p) + g_aReg8Index[idx]))
#define DIS_WRITE_REG8(p, idx, val)     (*(uint8_t *)((char *)(p) + g_aReg8Index[idx]) = val)
#define DIS_PTR_REG8(p, idx)            ( (uint8_t *)((char *)(p) + g_aReg8Index[idx]))

/**
 * Array for accessing segment registers in CPUMCTXCORE structure
 * by register's index from disasm.
 */
static const unsigned g_aRegSegIndex[] =
{
    RT_OFFSETOF(CPUMCTXCORE, es),         /* USE_REG_ES */
    RT_OFFSETOF(CPUMCTXCORE, cs),         /* USE_REG_CS */
    RT_OFFSETOF(CPUMCTXCORE, ss),         /* USE_REG_SS */
    RT_OFFSETOF(CPUMCTXCORE, ds),         /* USE_REG_DS */
    RT_OFFSETOF(CPUMCTXCORE, fs),         /* USE_REG_FS */
    RT_OFFSETOF(CPUMCTXCORE, gs)          /* USE_REG_GS */
};

static const unsigned g_aRegHidSegIndex[] =
{
    RT_OFFSETOF(CPUMCTXCORE, esHid),         /* USE_REG_ES */
    RT_OFFSETOF(CPUMCTXCORE, csHid),         /* USE_REG_CS */
    RT_OFFSETOF(CPUMCTXCORE, ssHid),         /* USE_REG_SS */
    RT_OFFSETOF(CPUMCTXCORE, dsHid),         /* USE_REG_DS */
    RT_OFFSETOF(CPUMCTXCORE, fsHid),         /* USE_REG_FS */
    RT_OFFSETOF(CPUMCTXCORE, gsHid)          /* USE_REG_GS */
};

/**
 * Macro for accessing segment registers in CPUMCTXCORE structure.
 */
#define DIS_READ_REGSEG(p, idx)         (*((uint16_t *)((char *)(p) + g_aRegSegIndex[idx])))
#define DIS_WRITE_REGSEG(p, idx, val)   (*((uint16_t *)((char *)(p) + g_aRegSegIndex[idx])) = val)

/**
 * Parses one instruction.
 * The result is found in pCpu.
 *
 * @returns Success indicator.
 * @param   pCpu            Pointer to cpu structure which has DISCPUSTATE::mode set correctly.
 * @param   InstructionAddr Pointer to the instruction to parse.
 * @param   pcbInstruction  Where to store the size of the instruction.
 *                          NULL is allowed.
 */
DISDECL(int) DISCoreOne(PDISCPUSTATE pCpu, RTUINTPTR InstructionAddr, unsigned *pcbInstruction)
{
    /*
     * Reset instruction settings
     */
    pCpu->prefix     = PREFIX_NONE;
    pCpu->prefix_seg = 0;
    pCpu->lastprefix = 0;
    pCpu->addrmode   = pCpu->mode;
    pCpu->opmode     = pCpu->mode;
    pCpu->ModRM      = 0;
    pCpu->SIB        = 0;
    pCpu->param1.parval = 0;
    pCpu->param2.parval = 0;
    pCpu->param3.parval = 0;
    pCpu->param1.szParam[0] = '\0';
    pCpu->param2.szParam[0] = '\0';
    pCpu->param3.szParam[0] = '\0';
    pCpu->param1.flags = 0;
    pCpu->param2.flags = 0;
    pCpu->param3.flags = 0;
    pCpu->param1.size  = 0;
    pCpu->param2.size  = 0;
    pCpu->param3.size  = 0;
    pCpu->pfnReadBytes = 0;
    pCpu->uFilter      = OPTYPE_ALL;
    pCpu->pfnDisasmFnTable = pfnFullDisasm;

    return VBOX_SUCCESS(disCoreOne(pCpu, InstructionAddr, pcbInstruction));
}

/**
 * Parses one guest instruction.
 * The result is found in pCpu and pcbInstruction.
 *
 * @returns VBox status code.
 * @param   InstructionAddr Address of the instruction to decode. What this means
 *                          is left to the pfnReadBytes function.
 * @param   enmCpuMode      The CPU mode. CPUMODE_32BIT, CPUMODE_16BIT, or CPUMODE_64BIT.
 * @param   pfnReadBytes    Callback for reading instruction bytes.
 * @param   pvUser          User argument for the instruction reader. (Ends up in apvUserData[0].)
 * @param   pCpu            Pointer to cpu structure. Will be initialized.
 * @param   pcbInstruction  Where to store the size of the instruction.
 *                          NULL is allowed.
 */
DISDECL(int) DISCoreOneEx(RTUINTPTR InstructionAddr, DISCPUMODE enmCpuMode, PFN_DIS_READBYTES pfnReadBytes, void *pvUser,
                          PDISCPUSTATE pCpu, unsigned *pcbInstruction)
{
    /*
     * Reset instruction settings
     */
    pCpu->prefix     = PREFIX_NONE;
    pCpu->prefix_seg = 0;
    pCpu->lastprefix = 0;
    pCpu->mode       = enmCpuMode;
    pCpu->addrmode   = enmCpuMode;
    pCpu->opmode     = enmCpuMode;
    pCpu->ModRM      = 0;
    pCpu->SIB        = 0;
    pCpu->param1.parval = 0;
    pCpu->param2.parval = 0;
    pCpu->param3.parval = 0;
    pCpu->param1.szParam[0] = '\0';
    pCpu->param2.szParam[0] = '\0';
    pCpu->param3.szParam[0] = '\0';
    pCpu->param1.flags      = 0;
    pCpu->param2.flags      = 0;
    pCpu->param3.flags      = 0;
    pCpu->param1.size       = 0;
    pCpu->param2.size       = 0;
    pCpu->param3.size       = 0;
    pCpu->pfnReadBytes      = pfnReadBytes;
    pCpu->apvUserData[0]    = pvUser;
    pCpu->uFilter           = OPTYPE_ALL;
    pCpu->pfnDisasmFnTable  = pfnFullDisasm;

    return disCoreOne(pCpu, InstructionAddr, pcbInstruction);
}

/**
 * Internal worker for DISCoreOne and DISCoreOneEx.
 *
 * @returns VBox status code.
 * @param   pCpu            Initialized cpu state.
 * @param   InstructionAddr Instruction address.
 * @param   pcbInstruction  Where to store the instruction size. Can be NULL.
 */
static int disCoreOne(PDISCPUSTATE pCpu, RTUINTPTR InstructionAddr, unsigned *pcbInstruction)
{
    /*
     * Parse byte by byte.
     */
    unsigned      iByte          = 0;

    while(1)
    {
        uint8_t codebyte   = DISReadByte(pCpu, InstructionAddr+iByte);
        uint8_t opcode     = g_aOneByteMapX86[codebyte].opcode;

        /* Hardcoded assumption about OP_* values!! */
        if (opcode <= OP_LOCK)
        {
            pCpu->lastprefix = opcode;
            switch(opcode)
            {
            case OP_INVALID:
                AssertMsgFailed(("Invalid opcode!!\n"));
                return VERR_GENERAL_FAILURE; /** @todo better error code. */

            // segment override prefix byte
            case OP_SEG:
                pCpu->prefix_seg = g_aOneByteMapX86[codebyte].param1 - OP_PARM_REG_SEG_START;
                pCpu->prefix    |= PREFIX_SEG;
                iByte           += sizeof(uint8_t);
                continue;   //fetch the next byte

            // lock prefix byte
            case OP_LOCK:
                pCpu->prefix |= PREFIX_LOCK;
                iByte       += sizeof(uint8_t);
                continue;   //fetch the next byte

            // address size override prefix byte
            case OP_ADRSIZE:
                pCpu->prefix |= PREFIX_ADDRSIZE;
                if(pCpu->mode == CPUMODE_16BIT)
                     pCpu->addrmode = CPUMODE_32BIT;
                else pCpu->addrmode = CPUMODE_16BIT;
                iByte        += sizeof(uint8_t);
                continue;   //fetch the next byte

            // operand size override prefix byte
            case OP_OPSIZE:
                pCpu->prefix |= PREFIX_OPSIZE;
                if(pCpu->mode == CPUMODE_16BIT)
                     pCpu->opmode = CPUMODE_32BIT;
                else pCpu->opmode = CPUMODE_16BIT;

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

            default:
                if (    pCpu->mode == CPUMODE_64BIT
                    &&  opcode >= OP_REX
                    &&  opcode <= OP_REX_WRXB)
                {
                    /* REX prefix byte */
                    pCpu->prefix    |= PREFIX_REX;
                    pCpu->prefix_rex = PREFIX_REX_OP_2_FLAGS(opcode);
                }
                break;
            }
        }

        unsigned uIdx = iByte;
        iByte += sizeof(uint8_t); //first opcode byte

        pCpu->opaddr = InstructionAddr + uIdx;
        pCpu->opcode = codebyte;

        int cbInc = ParseInstruction(InstructionAddr + iByte, &g_aOneByteMapX86[pCpu->opcode], pCpu);

        iByte += cbInc;
        break;
    }

    pCpu->opsize = iByte;
    if (pcbInstruction)
        *pcbInstruction = iByte;

    return VINF_SUCCESS;
}
//*****************************************************************************
//*****************************************************************************
DISDECL(int) DISGetParamSize(PDISCPUSTATE pCpu, POP_PARAMETER pParam)
{
    int subtype = OP_PARM_VSUBTYPE(pParam->param);

    if (subtype == OP_PARM_v)
    {
        subtype = (pCpu->opmode == CPUMODE_32BIT) ? OP_PARM_d : OP_PARM_w;
    }

    switch(subtype)
    {
    case OP_PARM_b:
        return 1;

    case OP_PARM_w:
        return 2;

    case OP_PARM_d:
        return 4;

    case OP_PARM_q:
    case OP_PARM_dq:
        return 8;

    case OP_PARM_p:
        if(pCpu->addrmode == CPUMODE_32BIT)
            return 8;
        else
            return 4;

    default:
        if(pParam->size)
            return pParam->size;
        else //@todo dangerous!!!
            return 4;
    }
}
//*****************************************************************************
//*****************************************************************************
DISDECL(int) DISDetectSegReg(PDISCPUSTATE pCpu, POP_PARAMETER pParam)
{
    if(pCpu->prefix & PREFIX_SEG)
    {
        /* Use specified SEG: prefix. */
        return pCpu->prefix_seg;
    }
    else
    {
        /* Guess segment register by parameter type. */
        if(pParam->flags & USE_REG_GEN32)
        {
            if(pParam->base.reg_gen32 == USE_REG_ESP || pParam->base.reg_gen32 == USE_REG_EBP)
                return USE_REG_SS;
        }
        else
        if(pParam->flags & USE_REG_GEN16)
        {
            if(pParam->base.reg_gen16 == USE_REG_SP || pParam->base.reg_gen16 == USE_REG_BP)
                return USE_REG_SS;
        }
        /* Default is use DS: for data access. */
        return USE_REG_DS;
    }
}
//*****************************************************************************
//*****************************************************************************
DISDECL(uint8_t) DISQuerySegPrefixByte(PDISCPUSTATE pCpu)
{
    Assert(pCpu->prefix & PREFIX_SEG);
    switch(pCpu->prefix_seg)
    {
    case USE_REG_ES:
        return 0x26;
    case USE_REG_CS:
        return 0x2E;
    case USE_REG_SS:
        return 0x36;
    case USE_REG_DS:
        return 0x3E;
    case USE_REG_FS:
        return 0x64;
    case USE_REG_GS:
        return 0x65;
    default:
        AssertFailed();
        return 0;
    }
}


/**
 * Returns the value of the specified 8 bits general purpose register
 *
 */
DISDECL(int) DISFetchReg8(PCPUMCTXCORE pCtx, uint32_t reg8, uint8_t *pVal)
{
    AssertReturn(reg8 < ELEMENTS(g_aReg8Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG8(pCtx, reg8);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified 16 bits general purpose register
 *
 */
DISDECL(int) DISFetchReg16(PCPUMCTXCORE pCtx, uint32_t reg16, uint16_t *pVal)
{
    AssertReturn(reg16 < ELEMENTS(g_aReg16Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG16(pCtx, reg16);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified 32 bits general purpose register
 *
 */
DISDECL(int) DISFetchReg32(PCPUMCTXCORE pCtx, uint32_t reg32, uint32_t *pVal)
{
    AssertReturn(reg32 < ELEMENTS(g_aReg32Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG32(pCtx, reg32);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 8 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg8(PCPUMCTXCORE pCtx, uint32_t reg8, uint8_t **ppReg)
{
    AssertReturn(reg8 < ELEMENTS(g_aReg8Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG8(pCtx, reg8);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 16 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg16(PCPUMCTXCORE pCtx, uint32_t reg16, uint16_t **ppReg)
{
    AssertReturn(reg16 < ELEMENTS(g_aReg16Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG16(pCtx, reg16);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 32 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg32(PCPUMCTXCORE pCtx, uint32_t reg32, uint32_t **ppReg)
{
    AssertReturn(reg32 < ELEMENTS(g_aReg32Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG32(pCtx, reg32);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified segment register
 *
 */
DISDECL(int) DISFetchRegSeg(PCPUMCTXCORE pCtx, uint32_t sel, RTSEL *pVal)
{
    AssertReturn(sel < ELEMENTS(g_aRegSegIndex), VERR_INVALID_PARAMETER);

    AssertCompile(sizeof(uint16_t) == sizeof(RTSEL));
    *pVal = DIS_READ_REGSEG(pCtx, sel);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified segment register including a pointer to the hidden register in the supplied cpu context
 *
 */
DISDECL(int) DISFetchRegSegEx(PCPUMCTXCORE pCtx, uint32_t sel, RTSEL *pVal, CPUMSELREGHID **ppSelHidReg)
{
    AssertReturn(sel < ELEMENTS(g_aRegSegIndex), VERR_INVALID_PARAMETER);

    AssertCompile(sizeof(uint16_t) == sizeof(RTSEL));
    *pVal = DIS_READ_REGSEG(pCtx, sel);
    *ppSelHidReg = (CPUMSELREGHID *)((char *)pCtx + g_aRegHidSegIndex[sel]);
    return VINF_SUCCESS;
}

/**
 * Updates the value of the specified 32 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg32(PCPUMCTXCORE pRegFrame, uint32_t reg32, uint32_t val32)
{
    AssertReturn(reg32 < ELEMENTS(g_aReg32Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG32(pRegFrame, reg32, val32);
    return VINF_SUCCESS;
}

/**
 * Updates the value of the specified 16 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg16(PCPUMCTXCORE pRegFrame, uint32_t reg16, uint16_t val16)
{
    AssertReturn(reg16 < ELEMENTS(g_aReg16Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG16(pRegFrame, reg16, val16);
    return VINF_SUCCESS;
}

/**
 * Updates the specified 8 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg8(PCPUMCTXCORE pRegFrame, uint32_t reg8, uint8_t val8)
{
    AssertReturn(reg8 < ELEMENTS(g_aReg8Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG8(pRegFrame, reg8, val8);
    return VINF_SUCCESS;
}

/**
 * Updates the specified segment register
 *
 */
DISDECL(int) DISWriteRegSeg(PCPUMCTXCORE pCtx, uint32_t sel, RTSEL val)
{
    AssertReturn(sel < ELEMENTS(g_aRegSegIndex), VERR_INVALID_PARAMETER);

    AssertCompile(sizeof(uint16_t) == sizeof(RTSEL));
    DIS_WRITE_REGSEG(pCtx, sel, val);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the parameter in pParam
 *
 * @returns VBox error code
 * @param   pCtx            CPU context structure pointer
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pParam          Pointer to the parameter to parse
 * @param   pParamVal       Pointer to parameter value (OUT)
 * @param   parmtype        Parameter type
 *
 * @note    Currently doesn't handle FPU/XMM/MMX/3DNow! parameters correctly!!
 *
 */
DISDECL(int) DISQueryParamVal(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, POP_PARAMETER pParam, POP_PARAMVAL pParamVal, PARAM_TYPE parmtype)
{
    memset(pParamVal, 0, sizeof(*pParamVal));

    if(pParam->flags & (USE_BASE|USE_INDEX|USE_DISPLACEMENT32|USE_DISPLACEMENT16|USE_DISPLACEMENT8))
    {
        // Effective address
        pParamVal->type = PARMTYPE_ADDRESS;
        pParamVal->size = pParam->size;

        if(pParam->flags & USE_BASE)
        {
            if(pParam->flags & USE_REG_GEN8)
            {
                pParamVal->flags |= PARAM_VAL8;
                if(VBOX_FAILURE(DISFetchReg8(pCtx, pParam->base.reg_gen8, &pParamVal->val.val8))) return VERR_INVALID_PARAMETER;
            }
            else
            if(pParam->flags & USE_REG_GEN16)
            {
                pParamVal->flags |= PARAM_VAL16;
                if(VBOX_FAILURE(DISFetchReg16(pCtx, pParam->base.reg_gen16, &pParamVal->val.val16))) return VERR_INVALID_PARAMETER;
            }
            else
            if(pParam->flags & USE_REG_GEN32)
            {
                pParamVal->flags |= PARAM_VAL32;
                if(VBOX_FAILURE(DISFetchReg32(pCtx, pParam->base.reg_gen32, &pParamVal->val.val32))) return VERR_INVALID_PARAMETER;
            }
            else {
                AssertFailed();
                return VERR_INVALID_PARAMETER;
            }
        }
        // Note that scale implies index (SIB byte)
        if(pParam->flags & USE_INDEX)
        {
            uint32_t val32;

            pParamVal->flags |= PARAM_VAL32;
            if(VBOX_FAILURE(DISFetchReg32(pCtx, pParam->index.reg_gen, &val32))) return VERR_INVALID_PARAMETER;

            if(pParam->flags & USE_SCALE)
            {
                val32 *= pParam->scale;
            }
            pParamVal->val.val32 += val32;
        }

        if(pParam->flags & USE_DISPLACEMENT8)
        {
            if(pCpu->mode & CPUMODE_32BIT)
            {
                pParamVal->val.val32 += (int32_t)pParam->disp8;
            }
            else
            {
                pParamVal->val.val16 += (int16_t)pParam->disp8;
            }
        }
        else
        if(pParam->flags & USE_DISPLACEMENT16)
        {
            if(pCpu->mode & CPUMODE_32BIT)
            {
                pParamVal->val.val32 += (int32_t)pParam->disp16;
            }
            else
            {
                pParamVal->val.val16 += pParam->disp16;
            }
        }
        else
        if(pParam->flags & USE_DISPLACEMENT32)
        {
            if(pCpu->mode & CPUMODE_32BIT)
            {
                pParamVal->val.val32 += pParam->disp32;
            }
            else
            {
                Assert(0);
            }
        }
        return VINF_SUCCESS;
    }

    if(pParam->flags & (USE_REG_GEN8|USE_REG_GEN16|USE_REG_GEN32|USE_REG_FP|USE_REG_MMX|USE_REG_XMM|USE_REG_CR|USE_REG_DBG|USE_REG_SEG|USE_REG_TEST))
    {
        if(parmtype == PARAM_DEST)
        {
            // Caller needs to interpret the register according to the instruction (source/target, special value etc)
            pParamVal->type = PARMTYPE_REGISTER;
            pParamVal->size = pParam->size;
            return VINF_SUCCESS;
        }
        //else PARAM_SOURCE

        pParamVal->type = PARMTYPE_IMMEDIATE;

        if(pParam->flags & USE_REG_GEN8)
        {
            pParamVal->flags |= PARAM_VAL8;
            pParamVal->size   = sizeof(uint8_t);
            if(VBOX_FAILURE(DISFetchReg8(pCtx, pParam->base.reg_gen8, &pParamVal->val.val8))) return VERR_INVALID_PARAMETER;
        }
        else
        if(pParam->flags & USE_REG_GEN16)
        {
            pParamVal->flags |= PARAM_VAL16;
            pParamVal->size   = sizeof(uint16_t);
            if(VBOX_FAILURE(DISFetchReg16(pCtx, pParam->base.reg_gen16, &pParamVal->val.val16))) return VERR_INVALID_PARAMETER;
        }
        else
        if(pParam->flags & USE_REG_GEN32)
        {
            pParamVal->flags |= PARAM_VAL32;
            pParamVal->size   = sizeof(uint32_t);
            if(VBOX_FAILURE(DISFetchReg32(pCtx, pParam->base.reg_gen32, &pParamVal->val.val32))) return VERR_INVALID_PARAMETER;
        }
        else
        {
            // Caller needs to interpret the register according to the instruction (source/target, special value etc)
            pParamVal->type = PARMTYPE_REGISTER;
        }
    }

    if(pParam->flags & USE_IMMEDIATE)
    {
        pParamVal->type = PARMTYPE_IMMEDIATE;
        if(pParam->flags & (USE_IMMEDIATE8|USE_IMMEDIATE8_REL))
        {
            pParamVal->flags |= PARAM_VAL8;
            if(pParam->size == 2)
            {
                pParamVal->size   = sizeof(uint16_t);
                pParamVal->val.val16 = (uint8_t)pParam->parval;
            }
            else
            {
                pParamVal->size   = sizeof(uint8_t);
                pParamVal->val.val8 = (uint8_t)pParam->parval;
            }
        }
        else
        if(pParam->flags & (USE_IMMEDIATE16|USE_IMMEDIATE16_REL|USE_IMMEDIATE_ADDR_0_16|USE_IMMEDIATE16_SX8))
        {
            pParamVal->flags |= PARAM_VAL16;
            pParamVal->size   = sizeof(uint16_t);
            pParamVal->val.val16 = (uint16_t)pParam->parval;
            Assert(pParamVal->size == pParam->size || ((pParam->size == 1) && (pParam->flags & USE_IMMEDIATE16_SX8)) );
        }
        else
        if(pParam->flags & (USE_IMMEDIATE32|USE_IMMEDIATE32_REL|USE_IMMEDIATE_ADDR_0_32|USE_IMMEDIATE32_SX8))
        {
            pParamVal->flags |= PARAM_VAL32;
            pParamVal->size   = sizeof(uint32_t);
            pParamVal->val.val32 = (uint32_t)pParam->parval;
            Assert(pParamVal->size == pParam->size || ((pParam->size == 1) && (pParam->flags & USE_IMMEDIATE32_SX8)) );
        }
        else
        if(pParam->flags & (USE_IMMEDIATE64))
        {
            pParamVal->flags |= PARAM_VAL64;
            pParamVal->size   = sizeof(uint64_t);
            pParamVal->val.val64 = pParam->parval;
            Assert(pParamVal->size == pParam->size);
        }
        else
        if(pParam->flags & (USE_IMMEDIATE_ADDR_16_16))
        {
            pParamVal->flags |= PARAM_VALFARPTR16;
            pParamVal->size   = sizeof(uint16_t)*2;
            pParamVal->val.farptr.sel    = (uint16_t)RT_LOWORD(pParam->parval >> 16);
            pParamVal->val.farptr.offset = (uint32_t)RT_LOWORD(pParam->parval);
            Assert(pParamVal->size == pParam->size);
        }
        else
        if(pParam->flags & (USE_IMMEDIATE_ADDR_16_32))
        {
            pParamVal->flags |= PARAM_VALFARPTR32;
            pParamVal->size   = sizeof(uint16_t) + sizeof(uint32_t);
            pParamVal->val.farptr.sel    = (uint16_t)RT_LOWORD(pParam->parval >> 32);
            pParamVal->val.farptr.offset = (uint32_t)(pParam->parval & 0xFFFFFFFF);
            Assert(pParam->size == 8);
        }
    }
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to a register of the parameter in pParam. We need this
 * pointer when an interpreted instruction updates a register as a side effect.
 * In CMPXCHG we know that only [r/e]ax is updated, but with XADD this could
 * be every register.
 *
 * @returns VBox error code
 * @param   pCtx            CPU context structure pointer
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pParam          Pointer to the parameter to parse
 * @param   pReg            Pointer to parameter value (OUT)
 * @param   cbsize          Parameter size (OUT)
 *
 * @note    Currently doesn't handle FPU/XMM/MMX/3DNow! parameters correctly!!
 *
 */
DISDECL(int) DISQueryParamRegPtr(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, POP_PARAMETER pParam, uint32_t **ppReg, size_t *pcbSize)
{
    if(pParam->flags & (USE_REG_GEN8|USE_REG_GEN16|USE_REG_GEN32|USE_REG_FP|USE_REG_MMX|USE_REG_XMM|USE_REG_CR|USE_REG_DBG|USE_REG_SEG|USE_REG_TEST))
    {
        if(pParam->flags & USE_REG_GEN8)
        {
            uint8_t *pu8Reg;
            if(VBOX_SUCCESS(DISPtrReg8(pCtx, pParam->base.reg_gen8, &pu8Reg)))
            {
                *pcbSize = sizeof(uint8_t);
                *ppReg = (uint32_t*)pu8Reg;
                return VINF_SUCCESS;
            }
        }
        else if(pParam->flags & USE_REG_GEN16)
        {
            uint16_t *pu16Reg;
            if(VBOX_SUCCESS(DISPtrReg16(pCtx, pParam->base.reg_gen16, &pu16Reg)))
            {
                *pcbSize = sizeof(uint16_t);
                *ppReg = (uint32_t*)pu16Reg;
                return VINF_SUCCESS;
            }
        }
        else if(pParam->flags & USE_REG_GEN32)
        {
            uint32_t *pu32Reg;
            if(VBOX_SUCCESS(DISPtrReg32(pCtx, pParam->base.reg_gen32, &pu32Reg)))
            {
                *pcbSize = sizeof(uint32_t);
                *ppReg = pu32Reg;
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_PARAMETER;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseInstruction(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, PDISCPUSTATE pCpu)
{
    int  size = 0;
    bool fFiltered = false;

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

    if(pOp->idxParse1 != IDX_ParseNop) {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse1](lpszCodeBlock, pOp, &pCpu->param1, pCpu);
        if (fFiltered == false) pCpu->param1.size = DISGetParamSize(pCpu, &pCpu->param1);
    }
    if(pOp->idxParse2 != IDX_ParseNop) {
        size += pCpu->pfnDisasmFnTable[pOp->idxParse2](lpszCodeBlock+size, pOp, &pCpu->param2, pCpu);
        if (fFiltered == false) pCpu->param2.size = DISGetParamSize(pCpu, &pCpu->param2);
    }
    if(pOp->idxParse3 != IDX_ParseNop) {
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

    pCpu->ModRM = DISReadByte(pCpu, lpszCodeBlock);

    index = pCpu->opcode - 0xD8;
    if(pCpu->ModRM <= 0xBF)
    {
        fpop            = &(g_paMapX86_FP_Low[index])[MODRM_REG(pCpu->ModRM)];
        pCpu->pCurInstr = (PCOPCODE)fpop;

        // Should contain the parameter type on input
        pCpu->param1.parval = fpop->param1;
        pCpu->param2.parval = fpop->param2;

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

        // Little hack to make sure the ModRM byte is included in the returned size
        if(fpop->idxParse1 != IDX_ParseModRM && fpop->idxParse2 != IDX_ParseModRM)
        {
            size = sizeof(uint8_t); //ModRM byte
        }

        if(fpop->idxParse1 != IDX_ParseNop) {
            size += pCpu->pfnDisasmFnTable[fpop->idxParse1](lpszCodeBlock+size, (PCOPCODE)fpop, pParam, pCpu);
        }
        if(fpop->idxParse2 != IDX_ParseNop) {
            size += pCpu->pfnDisasmFnTable[fpop->idxParse2](lpszCodeBlock+size, (PCOPCODE)fpop, pParam, pCpu);
        }
    }
    else
    {
        size            = sizeof(uint8_t); //ModRM byte only
        fpop            = &(g_paMapX86_FP_High[index])[pCpu->ModRM - 0xC0];
        pCpu->pCurInstr = (PCOPCODE)fpop;

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
    }

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
const char *szSIBBaseReg[8]  = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"};
const char *szSIBIndexReg[8] = {"EAX", "ECX", "EDX", "EBX", NULL,  "EBP", "ESI", "EDI"};
const char *szSIBScale[4]    = {"", "*2", "*4", "*8"};

//*****************************************************************************
void UseSIB(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int scale, base, index;
    char szTemp[32];
    szTemp[0] = '\0';

    scale = SIB_SCALE(pCpu->SIB);
    base  = SIB_BASE(pCpu->SIB);
    index = SIB_INDEX(pCpu->SIB);

    if(szSIBIndexReg[index])
    {
         pParam->flags |= USE_INDEX;
         pParam->index.reg_gen = index;

         if(scale != 0)
         {
             pParam->flags |= USE_SCALE;
             pParam->scale  = (1<<scale);
         }

         if(base == 5 && MODRM_MOD(pCpu->ModRM) == 0)
             disasmAddStringF(szTemp, sizeof(szTemp), "%s%s", szSIBIndexReg[index], szSIBScale[scale]);
         else
             disasmAddStringF(szTemp, sizeof(szTemp), "%s+%s%s", szSIBBaseReg[base], szSIBIndexReg[index], szSIBScale[scale]);
    }
    else
    {
         if(base != 5 || MODRM_MOD(pCpu->ModRM) != 0)
             disasmAddStringF(szTemp, sizeof(szTemp), "%s", szSIBBaseReg[base]);
    }

    if(base == 5 && MODRM_MOD(pCpu->ModRM) == 0)
    {
        // [scaled index] + disp32
        disasmAddString(pParam->szParam, &szTemp[0]);
        pParam->flags |= USE_DISPLACEMENT32;
        pParam->disp32 = pCpu->disp;
        disasmAddChar(pParam->szParam, '+');
        disasmPrintDisp32(pParam);
    }
    else
    {
        disasmAddString(pParam->szParam, szTemp);

        pParam->flags |= USE_BASE | USE_REG_GEN32;
        pParam->base.reg_gen32 = base;
    }
    return;   /* Already fetched everything in ParseSIB; no size returned */
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseSIB(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t), base;

    pCpu->SIB = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += size;

    base = SIB_BASE(pCpu->SIB);
    if(base == 5 && MODRM_MOD(pCpu->ModRM) == 0)
    {//additional 32 bits displacement
        pCpu->disp = DISReadDWord(pCpu, lpszCodeBlock);
        size += sizeof(int32_t);
    }
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseSIB_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t), base;

    pCpu->SIB = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += size;

    base = SIB_BASE(pCpu->SIB);
    if(base == 5 && MODRM_MOD(pCpu->ModRM) == 0)
    {//additional 32 bits displacement
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
    int reg   = MODRM_REG(pCpu->ModRM);
    int rm    = MODRM_RM(pCpu->ModRM);
    int mod   = MODRM_MOD(pCpu->ModRM);
    int vtype = OP_PARM_VTYPE(pParam->param);

    switch(vtype)
    {
    case OP_PARM_G: //general purpose register
        disasmModRMReg(pCpu, pOp, reg, pParam, 0);
        return 0;

    default:
        if (IS_OP_PARM_RARE(vtype))
        {
            switch(vtype)
            {
            case OP_PARM_C: //control register
                disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "CR%d", reg);
                pParam->flags |= USE_REG_CR;
                pParam->base.reg_ctrl = reg;
                return 0;

            case OP_PARM_D: //debug register
                disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "DR%d", reg);
                pParam->flags |= USE_REG_DBG;
                pParam->base.reg_dbg = reg;
                return 0;

            case OP_PARM_P: //MMX register
                disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "MM%d", reg);
                pParam->flags |= USE_REG_MMX;
                pParam->base.reg_mmx = reg;
                return 0;

            case OP_PARM_S: //segment register
                disasmModRMSReg(pCpu, pOp, reg, pParam);
                pParam->flags |= USE_REG_SEG;
                return 0;

            case OP_PARM_T: //test register
                disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "TR%d", reg);
                pParam->flags |= USE_REG_TEST;
                pParam->base.reg_test = reg;
                return 0;

            case OP_PARM_V: //XMM register
                disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "XMM%d", reg);
                pParam->flags |= USE_REG_XMM;
                pParam->base.reg_xmm = reg;
                return 0;

            case OP_PARM_W: //XMM register or memory operand
                if (mod == 3)
                {
                    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "XMM%d", rm);
                    pParam->flags |= USE_REG_XMM;
                    pParam->base.reg_xmm = rm;
                    return 0;
                }
                /* else memory operand */
            }
        }
    }

    //TODO: bound

    if(pCpu->addrmode == CPUMODE_32BIT)
    {//32 bits addressing mode
        switch(mod)
        {
        case 0: //effective address
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if(rm == 4) {//SIB byte follows ModRM
                UseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            }
            else
            if(rm == 5) {//32 bits displacement
                pParam->flags |= USE_DISPLACEMENT32;
                pParam->disp32 = pCpu->disp;
                disasmPrintDisp32(pParam);
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
            if(rm == 4) {//SIB byte follows ModRM
                UseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            }
            else
            {
                pParam->flags |= USE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            pParam->disp8 = pCpu->disp;
            pParam->flags |= USE_DISPLACEMENT8;

            if(pParam->disp8 != 0)
            {
                if(pParam->disp8 > 0)
                    disasmAddChar(pParam->szParam, '+');
                disasmPrintDisp8(pParam);
            }
            disasmAddChar(pParam->szParam, ']');
            break;

        case 2: //effective address + 32 bits displacement
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if(rm == 4) {//SIB byte follows ModRM
                UseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            }
            else
            {
                pParam->flags |= USE_BASE;
                disasmModRMReg(pCpu, pOp, rm, pParam, 1);
            }
            pParam->disp32 = pCpu->disp;
            pParam->flags |= USE_DISPLACEMENT32;

            if(pParam->disp32 != 0)
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
        switch(mod)
        {
        case 0: //effective address
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            if(rm == 6)
            {//16 bits displacement
                pParam->disp16 = pCpu->disp;
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
            pParam->disp8 = pCpu->disp;
            pParam->flags |= USE_BASE | USE_DISPLACEMENT8;

            if(pParam->disp8 != 0)
            {
                if(pParam->disp8 > 0)
                    disasmAddChar(pParam->szParam, '+');
                disasmPrintDisp8(pParam);
            }
            disasmAddChar(pParam->szParam, ']');
            break;

        case 2: //effective address + 16 bits displacement
            disasmGetPtrString(pCpu, pOp, pParam);
            disasmAddChar(pParam->szParam, '[');
            disasmModRMReg16(pCpu, pOp, rm, pParam);
            pParam->disp16 = pCpu->disp;
            pParam->flags |= USE_BASE | USE_DISPLACEMENT16;

            if(pParam->disp16 != 0)
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
unsigned QueryModRM(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, int *pSibInc)
{
    int mod, rm, sibinc;
    unsigned size = 0;

    rm  = MODRM_RM(pCpu->ModRM);
    mod = MODRM_MOD(pCpu->ModRM);

    if(!pSibInc)
    {
        pSibInc = &sibinc;
    }

    *pSibInc = 0;

    if(pCpu->addrmode == CPUMODE_32BIT)
    {//32 bits addressing mode
        if(mod != 3 && rm == 4)
        {//SIB byte follows ModRM
            *pSibInc = ParseSIB(lpszCodeBlock, pOp, pParam, pCpu);
            lpszCodeBlock += *pSibInc;
            size += *pSibInc;
        }

        switch(mod)
        {
        case 0: //effective address
            if(rm == 5) {//32 bits displacement
                pCpu->disp = DISReadDWord(pCpu, lpszCodeBlock);
                size += sizeof(int32_t);
            }
            //else register address
            break;

        case 1: //effective address + 8 bits displacement
            pCpu->disp = (int8_t)DISReadByte(pCpu, lpszCodeBlock);
            size += sizeof(char);
            break;

        case 2: //effective address + 32 bits displacement
            pCpu->disp = DISReadDWord(pCpu, lpszCodeBlock);
            size += sizeof(int32_t);
            break;

        case 3: //registers
            break;
        }
    }
    else
    {//16 bits addressing mode
        switch(mod)
        {
        case 0: //effective address
            if(rm == 6) {
                pCpu->disp = DISReadWord(pCpu, lpszCodeBlock);
                size += sizeof(uint16_t);
            }
            break;

        case 1: //effective address + 8 bits displacement
            pCpu->disp = (int8_t)DISReadByte(pCpu, lpszCodeBlock);
            size += sizeof(char);
            break;

        case 2: //effective address + 16 bits displacement
            pCpu->disp = (int16_t)DISReadWord(pCpu, lpszCodeBlock);
            size += sizeof(uint16_t);
            break;

        case 3: //registers
            break;
        }
    }
    return size;
}
//*****************************************************************************
// Query the size of the ModRM parameters and fetch the immediate data (if any)
//*****************************************************************************
unsigned QueryModRM_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu, int *pSibInc)
{
    int mod, rm, sibinc;
    unsigned size = 0;

    rm  = MODRM_RM(pCpu->ModRM);
    mod = MODRM_MOD(pCpu->ModRM);

    if(!pSibInc)
    {
        pSibInc = &sibinc;
    }

    *pSibInc = 0;

    if(pCpu->addrmode == CPUMODE_32BIT)
    {//32 bits addressing mode
        if(mod != 3 && rm == 4)
        {//SIB byte follows ModRM
            *pSibInc = ParseSIB_SizeOnly(lpszCodeBlock, pOp, pParam, pCpu);
            lpszCodeBlock += *pSibInc;
            size += *pSibInc;
        }

        switch(mod)
        {
        case 0: //effective address
            if(rm == 5) {//32 bits displacement
                size += sizeof(int32_t);
            }
            //else register address
            break;

        case 1: //effective address + 8 bits displacement
            size += sizeof(char);
            break;

        case 2: //effective address + 32 bits displacement
            size += sizeof(int32_t);
            break;

        case 3: //registers
            break;
        }
    }
    else
    {//16 bits addressing mode
        switch(mod)
        {
        case 0: //effective address
            if(rm == 6) {
                size += sizeof(uint16_t);
            }
            break;

        case 1: //effective address + 8 bits displacement
            size += sizeof(char);
            break;

        case 2: //effective address + 16 bits displacement
            size += sizeof(uint16_t);
            break;

        case 3: //registers
            break;
        }
    }
    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseIllegal(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    AssertFailed();
    return 0;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseModRM(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = sizeof(uint8_t);   //ModRM byte
    int sibinc;

    pCpu->ModRM    = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += sizeof(uint8_t);

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
    int sibinc;

    pCpu->ModRM    = DISReadByte(pCpu, lpszCodeBlock);
    lpszCodeBlock += sizeof(uint8_t);

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
    return 0;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByte(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    pParam->parval = DISReadByte(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE8;

    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%02Xh", (uint32_t)pParam->parval);
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByte_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByteSX(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    if(pCpu->opmode == CPUMODE_32BIT)
    {
        pParam->parval = (uint32_t)(int8_t)DISReadByte(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE32_SX8;
        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%08Xh", (uint32_t)pParam->parval);
    }
    else
    {
        pParam->parval = (uint16_t)(int8_t)DISReadByte(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16_SX8;
        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%04Xh", (uint16_t)pParam->parval);
    }
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmByteSX_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    return sizeof(uint8_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUshort(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE16;

    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%04Xh", (uint16_t)pParam->parval);
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUshort_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    return sizeof(uint16_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUlong(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE32;

    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%08Xh", (uint32_t)pParam->parval);
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmUlong_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    return sizeof(uint32_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmQword(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    pParam->parval = DISReadQWord(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE64;

    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%08X", (uint32_t)pParam->parval);
    disasmAddStringF(&pParam->szParam[9], sizeof(pParam->szParam)-9, "%08Xh", (uint32_t)(pParam->parval >> 32));
    return sizeof(uint64_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmQword_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    return sizeof(uint64_t);
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmV(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    if(pCpu->opmode == CPUMODE_32BIT)
    {
        pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE32;

        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%08Xh", (uint32_t)pParam->parval);
        return sizeof(uint32_t);
    }
    else
    {
        pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16;

        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%04Xh", (uint32_t)pParam->parval);
        return sizeof(uint16_t);
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmV_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    if(pCpu->opmode == CPUMODE_32BIT)
    {
        return sizeof(uint32_t);
    }
    else
    {
        return sizeof(uint16_t);
    }
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmBRel(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    pParam->parval = DISReadByte(pCpu, lpszCodeBlock);
    pParam->flags |= USE_IMMEDIATE8_REL;

    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), " (0%02Xh)", (uint32_t)pParam->parval);
    return sizeof(char);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmBRel_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    return sizeof(char);
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmVRel(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    if(pCpu->opmode == CPUMODE_32BIT)
    {
        pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE32_REL;

        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), " (0%08Xh)", (uint32_t)pParam->parval);
        return sizeof(int32_t);
    }
    else
    {
        pParam->parval = DISReadWord(pCpu, lpszCodeBlock);
        pParam->flags |= USE_IMMEDIATE16_REL;

        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), " (0%04Xh)", (uint32_t)pParam->parval);
        return sizeof(uint16_t);
    }
}
//*****************************************************************************
// Relative displacement for branches (rel. to next instruction)
//*****************************************************************************
unsigned ParseImmVRel_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    if(pCpu->opmode == CPUMODE_32BIT)
    {
        return sizeof(int32_t);
    }
    else
    {
        return sizeof(uint16_t);
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmAddr(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmGetPtrString(pCpu, pOp, pParam);
    if(pCpu->addrmode == CPUMODE_32BIT)
    {
        if(OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
        {// far 16:32 pointer
            pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
            *((uint32_t*)&pParam->parval+1) = DISReadWord(pCpu, lpszCodeBlock+sizeof(uint32_t));
            pParam->flags  |= USE_IMMEDIATE_ADDR_16_32;

            disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%04X:0%08Xh", (uint32_t)(pParam->parval>>32), (uint32_t)pParam->parval);
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

            disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "[0%08Xh]", pParam->disp32);
            return sizeof(uint32_t);
        }
    }
    else
    {
        if(OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
        {// far 16:16 pointer
            pParam->parval = DISReadDWord(pCpu, lpszCodeBlock);
            pParam->flags |= USE_IMMEDIATE_ADDR_16_16;

            disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "0%04X:0%04Xh", (uint32_t)(pParam->parval>>16), (uint16_t)pParam->parval );
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

            disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "[0%04Xh]", (uint32_t)pParam->disp16);
            return sizeof(uint16_t);
        }
    }
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseImmAddr_SizeOnly(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    if(pCpu->addrmode == CPUMODE_32BIT)
    {
        if(OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
        {// far 16:32 pointer
            return sizeof(uint32_t) + sizeof(uint16_t);
        }
        else
        {// near 32 bits pointer
            return sizeof(uint32_t);
        }
    }
    else
    {
        if(OP_PARM_VSUBTYPE(pParam->param) == OP_PARM_p)
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
unsigned ParseFixedReg(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    /*
     * Sets up flags for stored in OPC fixed registers.
     */

    if(pParam->param == OP_PARM_NONE)
    {
        /* No parameter at all. */
        return 0;
    }

    if(pParam->param < OP_PARM_REG_SEG_START)
    {
        /* 32-bit EAX..EDI registers. */

        if(pCpu->opmode == CPUMODE_32BIT)
        {
            /* Use 32-bit registers. */
            pParam->base.reg_gen32 = pParam->param - OP_PARM_REG_GEN32_START;
            pParam->flags |= USE_REG_GEN32;
            pParam->size   = 4;
        }
        else
        {
            /* Use 16-bit registers. */
            pParam->base.reg_gen16 = pParam->param - OP_PARM_REG_GEN32_START;
            pParam->flags |= USE_REG_GEN16;
            pParam->size   = 2;
            pParam->param = pParam->param - OP_PARM_REG_GEN32_START + OP_PARM_REG_GEN16_START;
        }
    }
    else
    if(pParam->param < OP_PARM_REG_GEN16_START)
    {
        /* Segment ES..GS registers. */
        pParam->base.reg_seg = pParam->param - OP_PARM_REG_SEG_START;
        pParam->flags |= USE_REG_SEG;
        pParam->size   = 2;
    }
    else
    if(pParam->param < OP_PARM_REG_GEN8_START)
    {
        /* 16-bit AX..DI registers. */
        pParam->base.reg_gen16 = pParam->param - OP_PARM_REG_GEN16_START;
        pParam->flags |= USE_REG_GEN16;
        pParam->size   = 2;
    }
    else
    if(pParam->param < OP_PARM_REG_FP_START)
    {
        /* 8-bit AL..DL, AH..DH registers. */
        pParam->base.reg_gen8 = pParam->param - OP_PARM_REG_GEN8_START;
        pParam->flags |= USE_REG_GEN8;
        pParam->size   = 1;
    }
    else
    if(pParam->param <= OP_PARM_REGFP_7)
    {
        /* FPU registers. */
        pParam->base.reg_fp = pParam->param - OP_PARM_REG_FP_START;
        pParam->flags |= USE_REG_FP;
        pParam->size   = 10;
    }
    /* else - not supported for now registers. */

    return 0;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseXv(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmGetPtrString(pCpu, pOp, pParam);
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), (pCpu->addrmode == CPUMODE_32BIT) ? "DS:ESI" : "DS:SI");

    pParam->flags |= USE_POINTER_DS_BASED;
    if(pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen32 = USE_REG_ESI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    {
        pParam->base.reg_gen16 = USE_REG_SI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseXb(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), (pCpu->addrmode == CPUMODE_32BIT) ? "DS:ESI" : "DS:SI");

    pParam->flags |= USE_POINTER_DS_BASED;
    if(pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen32 = USE_REG_ESI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    {
        pParam->base.reg_gen16 = USE_REG_SI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseYv(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmGetPtrString(pCpu, pOp, pParam);
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), (pCpu->addrmode == CPUMODE_32BIT) ? "ES:EDI" : "ES:DI");

    pParam->flags |= USE_POINTER_ES_BASED;
    if(pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen32 = USE_REG_EDI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    {
        pParam->base.reg_gen16 = USE_REG_DI;
        pParam->flags |= USE_REG_GEN16;
    }
    return 0;   //no additional opcode bytes
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseYb(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), (pCpu->addrmode == CPUMODE_32BIT) ? "ES:EDI" : "ES:DI");

    pParam->flags |= USE_POINTER_ES_BASED;
    if(pCpu->addrmode == CPUMODE_32BIT)
    {
        pParam->base.reg_gen32 = USE_REG_EDI;
        pParam->flags |= USE_REG_GEN32;
    }
    else
    {
        pParam->base.reg_gen16 = USE_REG_DI;
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

    //2nd byte
    pCpu->opcode = DISReadByte(pCpu, lpszCodeBlock);
    pOpcode      = &g_aTwoByteMapX86[pCpu->opcode];

    /* Handle opcode table extensions that rely on the address, repe or repne prefix byte.  */
    /** @todo Should we take the first or last prefix byte in case of multiple prefix bytes??? */
    if (pCpu->lastprefix)
    {
        switch(pCpu->lastprefix)
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
unsigned ParseNopPause(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0;

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

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group1[idx+reg];
    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseShiftGrp2(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int idx;
    unsigned size = 0, modrm, reg;

    switch(pCpu->opcode)
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
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp3(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    int idx = (pCpu->opcode - 0xF6) * 8;
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group3[idx+reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp4(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group4[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp5(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group5[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

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

    pCpu->ModRM    = DISReadByte(pCpu, lpszCodeBlock);

    modrmsize = QueryModRM(lpszCodeBlock+sizeof(uint8_t), pOp, pParam, pCpu);

    uint8_t opcode = DISReadByte(pCpu, lpszCodeBlock+sizeof(uint8_t)+modrmsize);

    pOp = (PCOPCODE)&g_aTwoByteMapX86_3DNow[opcode];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
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

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group6[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp7(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg, rm, mod;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    mod   = MODRM_MOD(modrm);
    reg   = MODRM_REG(modrm);
    rm    = MODRM_RM(modrm);

    if (mod == 3 && rm == 0)
    {
        pOp = (PCOPCODE)&g_aMapX86_Group7_mod11_rm000[reg];
    }
    else
    if (mod == 3 && rm == 1)
    {
        pOp = (PCOPCODE)&g_aMapX86_Group7_mod11_rm001[reg];
    }
    else
        pOp = (PCOPCODE)&g_aMapX86_Group7_mem[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp8(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group8[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp9(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group9[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp10(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group10[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp12(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    if(pCpu->prefix & PREFIX_OPSIZE)
    {
        reg += 8;   //2nd table
    }

    pOp = (PCOPCODE)&g_aMapX86_Group12[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp13(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);
    if(pCpu->prefix & PREFIX_OPSIZE)
    {
        reg += 8;   //2nd table
    }

    pOp = (PCOPCODE)&g_aMapX86_Group13[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp14(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);
    if(pCpu->prefix & PREFIX_OPSIZE)
    {
        reg += 8;   //2nd table
    }

    pOp = (PCOPCODE)&g_aMapX86_Group14[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp15(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg, mod, rm;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    mod   = MODRM_MOD(modrm);
    reg   = MODRM_REG(modrm);
    rm    = MODRM_RM(modrm);

    if (mod == 3 && rm == 0)
        pOp = (PCOPCODE)&g_aMapX86_Group15_mod11_rm000[reg];
    else
        pOp = (PCOPCODE)&g_aMapX86_Group15_mem[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
//*****************************************************************************
unsigned ParseGrp16(RTUINTPTR lpszCodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu)
{
    unsigned size = 0, modrm, reg;

    modrm = DISReadByte(pCpu, lpszCodeBlock);
    reg   = MODRM_REG(modrm);

    pOp = (PCOPCODE)&g_aMapX86_Group16[reg];

    //little hack to make sure the ModRM byte is included in the returned size
    if(pOp->idxParse1 != IDX_ParseModRM && pOp->idxParse2 != IDX_ParseModRM)
    {
        size = sizeof(uint8_t); //ModRM byte
    }

    size += ParseInstruction(lpszCodeBlock, pOp, pCpu);

    return size;
}
//*****************************************************************************
const char *szModRMReg8[]   = {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"};
const char *szModRMReg16[]  = {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"};
const char *szModRMReg32[]  = {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"};
//*****************************************************************************
void disasmModRMReg(PDISCPUSTATE pCpu, PCOPCODE pOp, int idx, POP_PARAMETER pParam, int fRegAddr)
{
    int subtype, type, mod;

    mod     = MODRM_MOD(pCpu->ModRM);

    type    = OP_PARM_VTYPE(pParam->param);
    subtype = OP_PARM_VSUBTYPE(pParam->param);
    if (fRegAddr)
    {
        subtype = OP_PARM_d;
    }
    else
    if(subtype == OP_PARM_v || subtype == OP_PARM_NONE)
    {
        subtype = (pCpu->opmode == CPUMODE_32BIT) ? OP_PARM_d : OP_PARM_w;
    }

    switch(subtype)
    {
    case OP_PARM_b:
        disasmAddString(pParam->szParam, szModRMReg8[idx]);
        pParam->flags |= USE_REG_GEN8;
        pParam->base.reg_gen8 = idx;
        break;

    case OP_PARM_w:
        disasmAddString(pParam->szParam, szModRMReg16[idx]);
        pParam->flags |= USE_REG_GEN16;
        pParam->base.reg_gen16 = idx;
        break;

    case OP_PARM_d:
        disasmAddString(pParam->szParam, szModRMReg32[idx]);
        pParam->flags |= USE_REG_GEN32;
        pParam->base.reg_gen32 = idx;
        break;

    default:
#ifdef IN_RING3
        Log(("disasmModRMReg %x:%x failed!!\n", type, subtype));
        DIS_THROW(ExceptionInvalidModRM);
#else
        AssertMsgFailed(("Oops!\n"));
#endif
        break;
    }
}
//*****************************************************************************
const char *szModRMReg1616[8]  = {"BX+SI", "BX+DI", "BP+SI", "BP+DI", "SI", "DI", "BP", "BX"};
int   BaseModRMReg16[8]  = { USE_REG_BX, USE_REG_BX, USE_REG_BP, USE_REG_BP, USE_REG_SI, USE_REG_DI, USE_REG_BP, USE_REG_BX};
int   IndexModRMReg16[4] = { USE_REG_SI, USE_REG_DI, USE_REG_SI, USE_REG_DI};
//*****************************************************************************
void disasmModRMReg16(PDISCPUSTATE pCpu, PCOPCODE pOp, int idx, POP_PARAMETER pParam)
{
    disasmAddString(pParam->szParam, szModRMReg1616[idx]);
    pParam->flags |= USE_REG_GEN16;
    pParam->base.reg_gen16 = BaseModRMReg16[idx];
    if(idx < 4)
    {
        pParam->flags |= USE_INDEX;
        pParam->index.reg_gen = IndexModRMReg16[idx];
    }
}
//*****************************************************************************
const char *szModRMSegReg[6] = {"ES", "CS", "SS", "DS", "FS", "GS"};
//*****************************************************************************
void disasmModRMSReg(PDISCPUSTATE pCpu, PCOPCODE pOp, int idx, POP_PARAMETER pParam)
{
#if 0 //def DEBUG_Sander
    AssertMsg(idx < (int)ELEMENTS(szModRMSegReg), ("idx=%d\n", idx));
#endif
#ifdef IN_RING3
    if(idx >= (int)ELEMENTS(szModRMSegReg))
    {
        Log(("disasmModRMSReg %d failed!!\n", idx));
        DIS_THROW(ExceptionInvalidParameter);
    }
#endif

    idx = RT_MIN(idx, (int)ELEMENTS(szModRMSegReg)-1);
    disasmAddString(pParam->szParam, szModRMSegReg[idx]);
    pParam->flags |= USE_REG_SEG;
    pParam->base.reg_seg = idx;
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintAbs32(POP_PARAMETER pParam)
{
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "%08Xh", pParam->disp32);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp32(POP_PARAMETER pParam)
{
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "%08Xh", pParam->disp32);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp8(POP_PARAMETER pParam)
{
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "%d", pParam->disp8);
}
//*****************************************************************************
//*****************************************************************************
void disasmPrintDisp16(POP_PARAMETER pParam)
{
    disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "%04Xh", pParam->disp16);
}
//*****************************************************************************
//*****************************************************************************
void disasmGetPtrString(PDISCPUSTATE pCpu, PCOPCODE pOp, POP_PARAMETER pParam)
{
    int subtype = OP_PARM_VSUBTYPE(pParam->param);

    if(subtype == OP_PARM_v)
    {
        subtype = (pCpu->opmode == CPUMODE_32BIT) ? OP_PARM_d : OP_PARM_w;
    }

    switch(subtype)
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
        disasmAddStringF(pParam->szParam, sizeof(pParam->szParam), "%s:", szModRMSegReg[pCpu->prefix_seg]);
}
#ifndef IN_GC
//*****************************************************************************
/* Read functions for getting the opcode bytes */
//*****************************************************************************
uint8_t DISReadByte(PDISCPUSTATE pCpu, RTUINTPTR pAddress)
{
    if(pCpu->pfnReadBytes)
    {
         uint8_t temp = 0;
         int     rc;

         rc = pCpu->pfnReadBytes(pAddress, &temp, sizeof(temp), pCpu);
         if(VBOX_FAILURE(rc))
         {
             Log(("DISReadByte failed!!\n"));
             DIS_THROW(ExceptionMemRead);
         }
         return temp;
    }
#ifdef IN_RING0
    AssertMsgFailed(("DISReadByte with no read callback in ring 0!!\n"));
    return 0;
#else
    else return *(uint8_t *)pAddress;
#endif
}
//*****************************************************************************
//*****************************************************************************
uint16_t DISReadWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress)
{
    if(pCpu->pfnReadBytes)
    {
         uint16_t temp = 0;
         int     rc;

         rc = pCpu->pfnReadBytes(pAddress, (uint8_t*)&temp, sizeof(temp), pCpu);
         if(VBOX_FAILURE(rc))
         {
             Log(("DISReadWord failed!!\n"));
             DIS_THROW(ExceptionMemRead);
         }
         return temp;
    }
#ifdef IN_RING0
    AssertMsgFailed(("DISReadWord with no read callback in ring 0!!\n"));
    return 0;
#else
    else return *(uint16_t *)pAddress;
#endif
}
//*****************************************************************************
//*****************************************************************************
uint32_t DISReadDWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress)
{
    if(pCpu->pfnReadBytes)
    {
         uint32_t temp = 0;
         int     rc;

         rc = pCpu->pfnReadBytes(pAddress, (uint8_t*)&temp, sizeof(temp), pCpu);
         if(VBOX_FAILURE(rc))
         {
             Log(("DISReadDWord failed!!\n"));
             DIS_THROW(ExceptionMemRead);
         }
         return temp;
    }
#ifdef IN_RING0
    AssertMsgFailed(("DISReadDWord with no read callback in ring 0!!\n"));
    return 0;
#else
    else return *(uint32_t *)pAddress;
#endif
}
//*****************************************************************************
//*****************************************************************************
uint64_t DISReadQWord(PDISCPUSTATE pCpu, RTUINTPTR pAddress)
{
    if(pCpu->pfnReadBytes)
    {
         uint64_t temp = 0;
         int     rc;

         rc = pCpu->pfnReadBytes(pAddress, (uint8_t*)&temp, sizeof(temp), pCpu);
         if(VBOX_FAILURE(rc))
         {
             Log(("DISReadQWord %x failed!!\n", pAddress));
             DIS_THROW(ExceptionMemRead);
         }

         return temp;
    }
#ifdef IN_RING0
    AssertMsgFailed(("DISReadQWord with no read callback in ring 0!!\n"));
    return 0;
#else
    else return *(uint64_t *)pAddress;
#endif
}
#endif /* IN_GC */

#if !defined(DIS_CORE_ONLY) && defined(LOG_ENABLED)
//*****************************************************************************
//*****************************************************************************
void disasmAddString(char *psz, const char *pszAdd)
{
    strcat(psz, pszAdd);
}
//*****************************************************************************
//*****************************************************************************
void disasmAddStringF(char *psz, uint32_t size, const char *pszFormat, ...)
{
    va_list args;
    va_start(args, pszFormat);
    RTStrPrintfV(psz + strlen(psz), size, pszFormat, args);
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
