/** @file
 *
 * VBox disassembler:
 * Core components
 */

/*
 * Copyright (C) 2006-2007 Sun Microsystems, Inc.
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
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
*   Global Variables                                                           *
*******************************************************************************/

/**
 * Array for accessing 64-bit general registers in VMMREGFRAME structure
 * by register's index from disasm.
 */
static const unsigned g_aReg64Index[] =
{
    RT_OFFSETOF(CPUMCTXCORE, rax),        /* USE_REG_RAX */
    RT_OFFSETOF(CPUMCTXCORE, rcx),        /* USE_REG_RCX */
    RT_OFFSETOF(CPUMCTXCORE, rdx),        /* USE_REG_RDX */
    RT_OFFSETOF(CPUMCTXCORE, rbx),        /* USE_REG_RBX */
    RT_OFFSETOF(CPUMCTXCORE, rsp),        /* USE_REG_RSP */
    RT_OFFSETOF(CPUMCTXCORE, rbp),        /* USE_REG_RBP */
    RT_OFFSETOF(CPUMCTXCORE, rsi),        /* USE_REG_RSI */
    RT_OFFSETOF(CPUMCTXCORE, rdi),        /* USE_REG_RDI */
    RT_OFFSETOF(CPUMCTXCORE, r8),         /* USE_REG_R8  */
    RT_OFFSETOF(CPUMCTXCORE, r9),         /* USE_REG_R9  */
    RT_OFFSETOF(CPUMCTXCORE, r10),        /* USE_REG_R10 */
    RT_OFFSETOF(CPUMCTXCORE, r11),        /* USE_REG_R11 */
    RT_OFFSETOF(CPUMCTXCORE, r12),        /* USE_REG_R12 */
    RT_OFFSETOF(CPUMCTXCORE, r13),        /* USE_REG_R13 */
    RT_OFFSETOF(CPUMCTXCORE, r14),        /* USE_REG_R14 */
    RT_OFFSETOF(CPUMCTXCORE, r15)         /* USE_REG_R15 */
};

/**
 * Macro for accessing 64-bit general purpose registers in CPUMCTXCORE structure.
 */
#define DIS_READ_REG64(p, idx)       (*(uint64_t *)((char *)(p) + g_aReg64Index[idx]))
#define DIS_WRITE_REG64(p, idx, val) (*(uint64_t *)((char *)(p) + g_aReg64Index[idx]) = val)
#define DIS_PTR_REG64(p, idx)        ( (uint64_t *)((char *)(p) + g_aReg64Index[idx]))

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

//*****************************************************************************
//*****************************************************************************
DISDECL(int) DISGetParamSize(PDISCPUSTATE pCpu, POP_PARAMETER pParam)
{
    int subtype = OP_PARM_VSUBTYPE(pParam->param);

    if (subtype == OP_PARM_v)
        subtype = (pCpu->opmode == CPUMODE_32BIT) ? OP_PARM_d : OP_PARM_w;

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

    case OP_PARM_p: /* far pointer */
        if (pCpu->addrmode == CPUMODE_32BIT)
            return 6;   /* 16:32 */
        else
        if (pCpu->addrmode == CPUMODE_64BIT)
            return 12;  /* 16:64 */
        else
            return 4;   /* 16:16 */

    default:
        if (pParam->size)
            return pParam->size;
        else //@todo dangerous!!!
            return 4;
    }
}
//*****************************************************************************
//*****************************************************************************
DISDECL(int) DISDetectSegReg(PDISCPUSTATE pCpu, POP_PARAMETER pParam)
{
    if (pCpu->prefix & PREFIX_SEG)
    {
        /* Use specified SEG: prefix. */
        return pCpu->prefix_seg;
    }
    else
    {
        /* Guess segment register by parameter type. */
        if (pParam->flags & (USE_REG_GEN32|USE_REG_GEN64|USE_REG_GEN16))
        {
            AssertCompile(USE_REG_ESP == USE_REG_RSP);
            AssertCompile(USE_REG_EBP == USE_REG_RBP);
            AssertCompile(USE_REG_ESP == USE_REG_SP);
            AssertCompile(USE_REG_EBP == USE_REG_BP);
            if (pParam->base.reg_gen == USE_REG_ESP || pParam->base.reg_gen == USE_REG_EBP)
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
DISDECL(int) DISFetchReg8(PCPUMCTXCORE pCtx, unsigned reg8, uint8_t *pVal)
{
    AssertReturn(reg8 < ELEMENTS(g_aReg8Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG8(pCtx, reg8);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified 16 bits general purpose register
 *
 */
DISDECL(int) DISFetchReg16(PCPUMCTXCORE pCtx, unsigned reg16, uint16_t *pVal)
{
    AssertReturn(reg16 < ELEMENTS(g_aReg16Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG16(pCtx, reg16);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified 32 bits general purpose register
 *
 */
DISDECL(int) DISFetchReg32(PCPUMCTXCORE pCtx, unsigned reg32, uint32_t *pVal)
{
    AssertReturn(reg32 < ELEMENTS(g_aReg32Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG32(pCtx, reg32);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified 64 bits general purpose register
 *
 */
DISDECL(int) DISFetchReg64(PCPUMCTXCORE pCtx, unsigned reg64, uint64_t *pVal)
{
    AssertReturn(reg64 < ELEMENTS(g_aReg64Index), VERR_INVALID_PARAMETER);

    *pVal = DIS_READ_REG64(pCtx, reg64);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 8 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg8(PCPUMCTXCORE pCtx, unsigned reg8, uint8_t **ppReg)
{
    AssertReturn(reg8 < ELEMENTS(g_aReg8Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG8(pCtx, reg8);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 16 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg16(PCPUMCTXCORE pCtx, unsigned reg16, uint16_t **ppReg)
{
    AssertReturn(reg16 < ELEMENTS(g_aReg16Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG16(pCtx, reg16);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 32 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg32(PCPUMCTXCORE pCtx, unsigned reg32, uint32_t **ppReg)
{
    AssertReturn(reg32 < ELEMENTS(g_aReg32Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG32(pCtx, reg32);
    return VINF_SUCCESS;
}

/**
 * Returns the pointer to the specified 64 bits general purpose register
 *
 */
DISDECL(int) DISPtrReg64(PCPUMCTXCORE pCtx, unsigned reg64, uint64_t **ppReg)
{
    AssertReturn(reg64 < ELEMENTS(g_aReg64Index), VERR_INVALID_PARAMETER);

    *ppReg = DIS_PTR_REG64(pCtx, reg64);
    return VINF_SUCCESS;
}

/**
 * Returns the value of the specified segment register
 *
 */
DISDECL(int) DISFetchRegSeg(PCPUMCTXCORE pCtx, unsigned sel, RTSEL *pVal)
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
DISDECL(int) DISFetchRegSegEx(PCPUMCTXCORE pCtx, unsigned sel, RTSEL *pVal, CPUMSELREGHID **ppSelHidReg)
{
    AssertReturn(sel < ELEMENTS(g_aRegSegIndex), VERR_INVALID_PARAMETER);

    AssertCompile(sizeof(uint16_t) == sizeof(RTSEL));
    *pVal = DIS_READ_REGSEG(pCtx, sel);
    *ppSelHidReg = (CPUMSELREGHID *)((char *)pCtx + g_aRegHidSegIndex[sel]);
    return VINF_SUCCESS;
}

/**
 * Updates the value of the specified 64 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg64(PCPUMCTXCORE pRegFrame, unsigned reg64, uint64_t val64)
{
    AssertReturn(reg64 < ELEMENTS(g_aReg64Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG64(pRegFrame, reg64, val64);
    return VINF_SUCCESS;
}

/**
 * Updates the value of the specified 32 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg32(PCPUMCTXCORE pRegFrame, unsigned reg32, uint32_t val32)
{
    AssertReturn(reg32 < ELEMENTS(g_aReg32Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG32(pRegFrame, reg32, val32);
    return VINF_SUCCESS;
}

/**
 * Updates the value of the specified 16 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg16(PCPUMCTXCORE pRegFrame, unsigned reg16, uint16_t val16)
{
    AssertReturn(reg16 < ELEMENTS(g_aReg16Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG16(pRegFrame, reg16, val16);
    return VINF_SUCCESS;
}

/**
 * Updates the specified 8 bits general purpose register
 *
 */
DISDECL(int) DISWriteReg8(PCPUMCTXCORE pRegFrame, unsigned reg8, uint8_t val8)
{
    AssertReturn(reg8 < ELEMENTS(g_aReg8Index), VERR_INVALID_PARAMETER);

    DIS_WRITE_REG8(pRegFrame, reg8, val8);
    return VINF_SUCCESS;
}

/**
 * Updates the specified segment register
 *
 */
DISDECL(int) DISWriteRegSeg(PCPUMCTXCORE pCtx, unsigned sel, RTSEL val)
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

    if (pParam->flags & (USE_BASE|USE_INDEX|USE_DISPLACEMENT32|USE_DISPLACEMENT16|USE_DISPLACEMENT8|USE_RIPDISPLACEMENT32))
    {
        // Effective address
        pParamVal->type = PARMTYPE_ADDRESS;
        pParamVal->size = pParam->size;

        if (pParam->flags & USE_BASE)
        {
            if (pParam->flags & USE_REG_GEN8)
            {
                pParamVal->flags |= PARAM_VAL8;
                if (VBOX_FAILURE(DISFetchReg8(pCtx, pParam->base.reg_gen, &pParamVal->val.val8))) return VERR_INVALID_PARAMETER;
            }
            else
            if (pParam->flags & USE_REG_GEN16)
            {
                pParamVal->flags |= PARAM_VAL16;
                if (VBOX_FAILURE(DISFetchReg16(pCtx, pParam->base.reg_gen, &pParamVal->val.val16))) return VERR_INVALID_PARAMETER;
            }
            else
            if (pParam->flags & USE_REG_GEN32)
            {
                pParamVal->flags |= PARAM_VAL32;
                if (VBOX_FAILURE(DISFetchReg32(pCtx, pParam->base.reg_gen, &pParamVal->val.val32))) return VERR_INVALID_PARAMETER;
            }
            else
            if (pParam->flags & USE_REG_GEN64)
            {
                pParamVal->flags |= PARAM_VAL64;
                if (VBOX_FAILURE(DISFetchReg64(pCtx, pParam->base.reg_gen, &pParamVal->val.val64))) return VERR_INVALID_PARAMETER;
            }
            else {
                AssertFailed();
                return VERR_INVALID_PARAMETER;
            }
        }
        // Note that scale implies index (SIB byte)
        if (pParam->flags & USE_INDEX)
        {
            uint32_t val32;

            pParamVal->flags |= PARAM_VAL32;
            if (VBOX_FAILURE(DISFetchReg32(pCtx, pParam->index.reg_gen, &val32))) return VERR_INVALID_PARAMETER;

            if (pParam->flags & USE_SCALE)
                val32 *= pParam->scale;

            pParamVal->val.val32 += val32;
        }

        if (pParam->flags & USE_DISPLACEMENT8)
        {
            if (pCpu->mode == CPUMODE_32BIT)
                pParamVal->val.val32 += (int32_t)pParam->disp8;
            else
                pParamVal->val.val16 += (int16_t)pParam->disp8;
        }
        else
        if (pParam->flags & USE_DISPLACEMENT16)
        {
            if (pCpu->mode == CPUMODE_32BIT)
                pParamVal->val.val32 += (int32_t)pParam->disp16;
            else
                pParamVal->val.val16 += pParam->disp16;
        }
        else
        if (pParam->flags & USE_DISPLACEMENT32)
        {
            if (pCpu->mode == CPUMODE_32BIT)
                pParamVal->val.val32 += pParam->disp32;
            else
                AssertFailed();
        }
        else
        if (pParam->flags & USE_RIPDISPLACEMENT32)
        {
            if (pCpu->mode == CPUMODE_64BIT)
                pParamVal->val.val64 += pParam->disp32 + pCtx->rip;
            else
                AssertFailed();
        }
        return VINF_SUCCESS;
    }

    if (pParam->flags & (USE_REG_GEN8|USE_REG_GEN16|USE_REG_GEN32|USE_REG_GEN64|USE_REG_FP|USE_REG_MMX|USE_REG_XMM|USE_REG_CR|USE_REG_DBG|USE_REG_SEG|USE_REG_TEST))
    {
        if (parmtype == PARAM_DEST)
        {
            // Caller needs to interpret the register according to the instruction (source/target, special value etc)
            pParamVal->type = PARMTYPE_REGISTER;
            pParamVal->size = pParam->size;
            return VINF_SUCCESS;
        }
        //else PARAM_SOURCE

        pParamVal->type = PARMTYPE_IMMEDIATE;

        if (pParam->flags & USE_REG_GEN8)
        {
            pParamVal->flags |= PARAM_VAL8;
            pParamVal->size   = sizeof(uint8_t);
            if (VBOX_FAILURE(DISFetchReg8(pCtx, pParam->base.reg_gen, &pParamVal->val.val8))) return VERR_INVALID_PARAMETER;
        }
        else
        if (pParam->flags & USE_REG_GEN16)
        {
            pParamVal->flags |= PARAM_VAL16;
            pParamVal->size   = sizeof(uint16_t);
            if (VBOX_FAILURE(DISFetchReg16(pCtx, pParam->base.reg_gen, &pParamVal->val.val16))) return VERR_INVALID_PARAMETER;
        }
        else
        if (pParam->flags & USE_REG_GEN32)
        {
            pParamVal->flags |= PARAM_VAL32;
            pParamVal->size   = sizeof(uint32_t);
            if (VBOX_FAILURE(DISFetchReg32(pCtx, pParam->base.reg_gen, &pParamVal->val.val32))) return VERR_INVALID_PARAMETER;
        }
        else
        if (pParam->flags & USE_REG_GEN64)
        {
            pParamVal->flags |= PARAM_VAL64;
            pParamVal->size   = sizeof(uint64_t);
            if (VBOX_FAILURE(DISFetchReg64(pCtx, pParam->base.reg_gen, &pParamVal->val.val64))) return VERR_INVALID_PARAMETER;
        }
        else
        {
            // Caller needs to interpret the register according to the instruction (source/target, special value etc)
            pParamVal->type = PARMTYPE_REGISTER;
        }
        Assert(!(pParam->flags & USE_IMMEDIATE));
        return VINF_SUCCESS;
    }

    if (pParam->flags & USE_IMMEDIATE)
    {
        pParamVal->type = PARMTYPE_IMMEDIATE;
        if (pParam->flags & (USE_IMMEDIATE8|USE_IMMEDIATE8_REL))
        {
            pParamVal->flags |= PARAM_VAL8;
            if (pParam->size == 2)
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
        if (pParam->flags & (USE_IMMEDIATE16|USE_IMMEDIATE16_REL|USE_IMMEDIATE_ADDR_0_16|USE_IMMEDIATE16_SX8))
        {
            pParamVal->flags |= PARAM_VAL16;
            pParamVal->size   = sizeof(uint16_t);
            pParamVal->val.val16 = (uint16_t)pParam->parval;
            Assert(pParamVal->size == pParam->size || ((pParam->size == 1) && (pParam->flags & USE_IMMEDIATE16_SX8)) );
        }
        else
        if (pParam->flags & (USE_IMMEDIATE32|USE_IMMEDIATE32_REL|USE_IMMEDIATE_ADDR_0_32|USE_IMMEDIATE32_SX8))
        {
            pParamVal->flags |= PARAM_VAL32;
            pParamVal->size   = sizeof(uint32_t);
            pParamVal->val.val32 = (uint32_t)pParam->parval;
            Assert(pParamVal->size == pParam->size || ((pParam->size == 1) && (pParam->flags & USE_IMMEDIATE32_SX8)) );
        }
        else
        if (pParam->flags & (USE_IMMEDIATE64))
        {
            pParamVal->flags |= PARAM_VAL64;
            pParamVal->size   = sizeof(uint64_t);
            pParamVal->val.val64 = pParam->parval;
            Assert(pParamVal->size == pParam->size);
        }
        else
        if (pParam->flags & (USE_IMMEDIATE_ADDR_16_16))
        {
            pParamVal->flags |= PARAM_VALFARPTR16;
            pParamVal->size   = sizeof(uint16_t)*2;
            pParamVal->val.farptr.sel    = (uint16_t)RT_LOWORD(pParam->parval >> 16);
            pParamVal->val.farptr.offset = (uint32_t)RT_LOWORD(pParam->parval);
            Assert(pParamVal->size == pParam->size);
        }
        else
        if (pParam->flags & (USE_IMMEDIATE_ADDR_16_32))
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
DISDECL(int) DISQueryParamRegPtr(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, POP_PARAMETER pParam, void **ppReg, size_t *pcbSize)
{
    if (pParam->flags & (USE_REG_GEN8|USE_REG_GEN16|USE_REG_GEN32|USE_REG_FP|USE_REG_MMX|USE_REG_XMM|USE_REG_CR|USE_REG_DBG|USE_REG_SEG|USE_REG_TEST))
    {
        if (pParam->flags & USE_REG_GEN8)
        {
            uint8_t *pu8Reg;
            if (VBOX_SUCCESS(DISPtrReg8(pCtx, pParam->base.reg_gen, &pu8Reg)))
            {
                *pcbSize = sizeof(uint8_t);
                *ppReg = (void *)pu8Reg;
                return VINF_SUCCESS;
            }
        }
        else
        if (pParam->flags & USE_REG_GEN16)
        {
            uint16_t *pu16Reg;
            if (VBOX_SUCCESS(DISPtrReg16(pCtx, pParam->base.reg_gen, &pu16Reg)))
            {
                *pcbSize = sizeof(uint16_t);
                *ppReg = (void *)pu16Reg;
                return VINF_SUCCESS;
            }
        }
        else
        if (pParam->flags & USE_REG_GEN32)
        {
            uint32_t *pu32Reg;
            if (VBOX_SUCCESS(DISPtrReg32(pCtx, pParam->base.reg_gen, &pu32Reg)))
            {
                *pcbSize = sizeof(uint32_t);
                *ppReg = (void *)pu32Reg;
                return VINF_SUCCESS;
            }
        }
        else
        if (pParam->flags & USE_REG_GEN64)
        {
            uint64_t *pu64Reg;
            if (VBOX_SUCCESS(DISPtrReg64(pCtx, pParam->base.reg_gen, &pu64Reg)))
            {
                *pcbSize = sizeof(uint64_t);
                *ppReg = (void *)pu64Reg;
                return VINF_SUCCESS;
            }
        }
    }
    return VERR_INVALID_PARAMETER;
}
//*****************************************************************************
//*****************************************************************************
