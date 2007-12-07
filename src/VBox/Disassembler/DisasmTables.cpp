/** @file
 *
 * VBox disassembler:
 * Tables
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
#ifdef USING_VISUAL_STUDIO
# include <stdafx.h>
#endif
#include <VBox/dis.h>
#include <VBox/disopcode.h>
#include "DisasmTables.h"
#include "DisasmInternal.h"


/** @def O
 * Wrapper which initializes an OPCODE.
 * We must use this so that we can exclude unused fields in order
 * to save precious bytes in the GC version.
 *
 * @internal
 */
#ifndef DIS_CORE_ONLY
# define OP(pszOpcode, idxParse1, idxParse2, idxParse3, opcode, param1, param2, param3, optype) \
    { pszOpcode, idxParse1, idxParse2, idxParse3, opcode, param1, param2, param3, optype }
#else
# define OP(pszOpcode, idxParse1, idxParse2, idxParse3, opcode, param1, param2, param3, optype) \
    { idxParse1, idxParse2, idxParse3, opcode, param1, param2, param3, optype }
#endif

//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
//TODO: Verify tables for correctness
//TODO: opcode type (harmless, potentially dangerous, dangerous)
//!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

char SZINVALID_OPCODE[] = "Invalid Opcode";

#define INVALID_OPCODE  \
    OP(SZINVALID_OPCODE,     0,              0,          0,          OP_INVALID, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_INVALID)

#define INVALID_OPCODE_BLOCK \
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,\
    INVALID_OPCODE,

/* Tables for the elegant Intel X86 instruction set */

const OPCODE g_aOneByteMapX86[256] =
{
    /* 0 */
    OP("add %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADD,     OP_PARM_Eb,         OP_PARM_Gb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("add %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADD,     OP_PARM_Ev,         OP_PARM_Gv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("add %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADD,     OP_PARM_Gb,         OP_PARM_Eb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("add %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADD,     OP_PARM_Gv,         OP_PARM_Ev ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("add AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_ADD,     OP_PARM_REG_AL,     OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("add %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_ADD,     OP_PARM_REG_EAX,    OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push ES",            IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_ES,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop ES",             IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_ES,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Eb,%Gb",         IDX_ParseModRM,     IDX_UseModRM,   0,          OP_OR,      OP_PARM_Eb,         OP_PARM_Gb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Ev,%Gv",         IDX_ParseModRM,     IDX_UseModRM,   0,          OP_OR,      OP_PARM_Ev,         OP_PARM_Gv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Gb,%Eb",         IDX_ParseModRM,     IDX_UseModRM,   0,          OP_OR,      OP_PARM_Gb,         OP_PARM_Eb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Gv,%Ev",         IDX_ParseModRM,     IDX_UseModRM,   0,          OP_OR,      OP_PARM_Gv,         OP_PARM_Ev ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or AL,%Ib",          IDX_ParseFixedReg,  IDX_ParseImmByte, 0,        OP_OR,      OP_PARM_REG_AL,     OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %eAX,%Iv",        IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_OR,      OP_PARM_REG_EAX,    OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push CS",            IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_CS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_POTENTIALLY_DANGEROUS),
    OP("2-BYTE ESCAPE",      IDX_ParseTwoByteEsc,0,          0,          OP_2B_ESC,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 1 */
    OP("adc %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADC,     OP_PARM_Eb,         OP_PARM_Gb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADC,     OP_PARM_Ev,         OP_PARM_Gv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADC,     OP_PARM_Gb,         OP_PARM_Eb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADC,     OP_PARM_Gv,         OP_PARM_Ev ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_ADC,     OP_PARM_REG_AL,     OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_ADC,     OP_PARM_REG_EAX,    OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push SS",            IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_SS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_RRM_DANGEROUS),
    OP("pop SS",             IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_SS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_RRM_DANGEROUS | OPTYPE_INHIBIT_IRQS),
    OP("sbb %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SBB,     OP_PARM_Eb,         OP_PARM_Gb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SBB,     OP_PARM_Ev,         OP_PARM_Gv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SBB,     OP_PARM_Gb,         OP_PARM_Eb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SBB,     OP_PARM_Gv,         OP_PARM_Ev ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_SBB,     OP_PARM_REG_AL,     OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_SBB,     OP_PARM_REG_EAX,    OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push DS",            IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_DS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop DS",             IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_DS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_RRM_DANGEROUS),

    /* 2 */
    OP("and %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_AND,     OP_PARM_Eb,         OP_PARM_Gb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_AND,     OP_PARM_Ev,         OP_PARM_Gv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_AND,     OP_PARM_Gb,         OP_PARM_Eb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_AND,     OP_PARM_Gv,         OP_PARM_Ev ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_AND,     OP_PARM_REG_AL,     OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_AND,     OP_PARM_REG_EAX,    OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("SEG ES",             0,              0,          0,          OP_SEG,     OP_PARM_REG_ES,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("daa",                0,              0,          0,          OP_DAA,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUB,     OP_PARM_Eb,         OP_PARM_Gb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUB,     OP_PARM_Ev,         OP_PARM_Gv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUB,     OP_PARM_Gb,         OP_PARM_Eb ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUB,     OP_PARM_Gv,         OP_PARM_Ev ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_SUB,     OP_PARM_REG_AL,     OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_SUB,     OP_PARM_REG_EAX,    OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    /* Branch not taken hint prefix for branches on a Pentium 4 or Xeon CPU (or higher)! */
    OP("SEG CS",             0,              0,          0,          OP_SEG,     OP_PARM_REG_CS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("das",                0,              0,          0,          OP_DAS,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 3 */
    OP("xor %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XOR,     OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XOR,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XOR,     OP_PARM_Gb,         OP_PARM_Eb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XOR,     OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_XOR,     OP_PARM_REG_AL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_XOR,     OP_PARM_REG_EAX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("SEG SS",             0,              0,          0,          OP_SEG,     OP_PARM_REG_SS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("aaa",                0,              0,          0,          OP_AAA,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMP,     OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMP,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMP,     OP_PARM_Gb,         OP_PARM_Eb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMP,     OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_CMP,     OP_PARM_REG_AL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_CMP,     OP_PARM_REG_EAX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    /* Branch not taken hint prefix for branches on a Pentium 4 or Xeon CPU (or higher)! */
    OP("SEG DS",             0,              0,          0,          OP_SEG,     OP_PARM_REG_DS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("aas",                0,              0,          0,          OP_AAS,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 4 */
    OP("inc %eAX",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_EAX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eCX",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_ECX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eDX",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_EDX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eBX",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_EBX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eSP",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_ESP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eBP",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_EBP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eSI",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_ESI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc %eDI",           IDX_ParseFixedReg,  0,          0,          OP_INC,     OP_PARM_REG_EDI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eAX",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_EAX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eCX",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_ECX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eDX",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_EDX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eBX",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_EBX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eSP",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_ESP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eBP",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_EBP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eSI",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_ESI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %eDI",           IDX_ParseFixedReg,  0,          0,          OP_DEC,     OP_PARM_REG_EDI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 5 */
    OP("push %eAX",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_EAX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eCX",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_ECX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eDX",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_EDX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eBX",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_EBX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eSP",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_ESP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eBP",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_EBP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eSI",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_ESI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %eDI",          IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_EDI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eAX",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_EAX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eCX",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_ECX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eDX",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_EDX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eBX",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_EBX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eSP",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_ESP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eBP",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_EBP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eSI",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_ESI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop %eDI",           IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_EDI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 6 */
    OP("pusha",              0,              0,          0,          OP_PUSHA,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("popa",               0,              0,          0,          OP_POPA,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bound %Gv,%Ma",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BOUND,   OP_PARM_Gv,         OP_PARM_Ma,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("arpl %Ew,%Rw",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ARPL,    OP_PARM_Ew,         OP_PARM_Rw,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("SEG FS",             0,              0,          0,          OP_SEG,     OP_PARM_REG_FS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("SEG GS",             0,              0,          0,          OP_SEG,     OP_PARM_REG_GS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("OP SIZE",            0,              0,          0,          OP_OPSIZE,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ADR SIZE",           0,              0,          0,          OP_ADRSIZE, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("push %Iv",           IDX_ParseImmV,      0,          0,          OP_PUSH,    OP_PARM_Iv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("imul %Gv,%Ev,%Iv",   IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmV,  OP_IMUL,    OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_Iv,     OPTYPE_HARMLESS),
    OP("push %Ib",           IDX_ParseImmByteSX, 0,          0,          OP_PUSH,    OP_PARM_Ib,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("imul %Gv,%Ev,%Ib",   IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByteSX,OP_IMUL,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("insb %Yb,DX",        IDX_ParseYb,        IDX_ParseFixedReg,       0,          OP_INSB,    OP_PARM_Yb,         OP_PARM_REG_DX, OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_READ),
    OP("insw/d %Yv,DX",      IDX_ParseYv,        IDX_ParseFixedReg,       0,          OP_INSWD,   OP_PARM_Yv,         OP_PARM_REG_DX, OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_READ),
    OP("outsb DX,%Xb",       IDX_ParseFixedReg,  IDX_ParseXb,    0,          OP_OUTSB,   OP_PARM_REG_DX,     OP_PARM_Yb,     OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_WRITE),
    OP("outsw/d DX,%Xv",     IDX_ParseFixedReg,  IDX_ParseXv,    0,          OP_OUTSWD,  OP_PARM_REG_DX,     OP_PARM_Yv,     OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_WRITE),


    /* 7 */
    OP("jo %Jb",             IDX_ParseImmBRel,   0,          0,          OP_JO,      OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jno %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JNO,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jc %Jb",             IDX_ParseImmBRel,   0,          0,          OP_JC,      OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnc %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JNC,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("je %Jb",             IDX_ParseImmBRel,   0,          0,          OP_JE,      OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jne %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JNE,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jbe %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JBE,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnbe %Jb",           IDX_ParseImmBRel,   0,          0,          OP_JNBE,    OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("js %Jb",             IDX_ParseImmBRel,   0,          0,          OP_JS,      OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jns %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JNS,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jp %Jb",             IDX_ParseImmBRel,   0,          0,          OP_JP,      OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnp %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JNP,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jl %Jb",             IDX_ParseImmBRel,   0,          0,          OP_JL,      OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnl %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JNL,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jle %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JLE,     OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnle %Jb",           IDX_ParseImmBRel,   0,          0,          OP_JNLE,    OP_PARM_Jb  ,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),

    /* 8 */
    OP("Imm Grp1 %Eb,%Ib",   IDX_ParseImmGrpl,   0,          0,          OP_IMM_GRP1,OP_PARM_Eb,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Imm Grp1 %Ev,%Iv",   IDX_ParseImmGrpl,   0,          0,          OP_IMM_GRP1,OP_PARM_Ev,         OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Imm Grp1 %Eb,%Ib",   IDX_ParseImmGrpl,   0,          0,          OP_IMM_GRP1,OP_PARM_Eb,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Imm Grp1 %Ev,%Ib",   IDX_ParseImmGrpl,   0,          0,          OP_IMM_GRP1,OP_PARM_Ev,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("test %Eb,%Gb",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_TEST,    OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("test %Ev,%Gv",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_TEST,    OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %Eb,%Gb",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XCHG,    OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %Ev,%Gv",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XCHG,    OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Eb,%Gb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV,     OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Gb,%Eb",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV,     OP_PARM_Gb,         OP_PARM_Eb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Gv,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV,     OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Ev,%Sw",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV,     OP_PARM_Ev,         OP_PARM_Sw,     OP_PARM_NONE,   OPTYPE_POTENTIALLY_DANGEROUS),
    OP("lea %Gv,%M",         IDX_ParseModRM,     IDX_UseModRM,   0,          OP_LEA,     OP_PARM_Gv,         OP_PARM_M,      OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Sw,%Ev",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV,     OP_PARM_Sw,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_POTENTIALLY_DANGEROUS | OPTYPE_INHIBIT_IRQS),
    OP("pop %Ev",            IDX_ParseModRM,     0,          0,          OP_POP,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 9 */
    OP("nop/pause/xchg %eAX,%eAX",  IDX_ParseNopPause,  0,          0,       OP_NOP,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eCX,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_ECX,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eDX,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_EDX,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eBX,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_EBX,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eSP,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_ESP,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eBP,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_EBP,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eSI,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_ESI,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xchg %eDI,%eAX",     IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_XCHG,    OP_PARM_REG_EDI,    OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cbw",                0,              0,          0,          OP_CBW,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cwd",                0,              0,          0,          OP_CWD,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("call %Ap",           IDX_ParseImmAddr,   0,          0,          OP_CALL,    OP_PARM_Ap,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW),
    OP("wait",               0,              0,          0,          OP_WAIT,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pushf %Fv",          0,              0,          0,          OP_PUSHF,   OP_PARM_Fv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_POTENTIALLY_DANGEROUS),
    OP("popf %Fv",           0,              0,          0,          OP_POPF,    OP_PARM_Fv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_POTENTIALLY_DANGEROUS),
    OP("sahf",               0,              0,          0,          OP_SAHF,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lahf",               0,              0,          0,          OP_LAHF,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),


    /* A */
    OP("mov AL,%Ob",         IDX_ParseFixedReg,  IDX_ParseImmAddr,0,         OP_MOV,     OP_PARM_REG_AL,     OP_PARM_Ob,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eAX,%Ov",       IDX_ParseFixedReg,  IDX_ParseImmAddr,0,         OP_MOV,     OP_PARM_REG_EAX,    OP_PARM_Ov,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Ob,AL",         IDX_ParseImmAddr,   IDX_ParseFixedReg, 0,       OP_MOV,     OP_PARM_Ob,         OP_PARM_REG_AL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Ov,%eAX",       IDX_ParseImmAddr,   IDX_ParseFixedReg, 0,       OP_MOV,     OP_PARM_Ov,         OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movsb %Xb,%Yb",      IDX_ParseXb,        IDX_ParseYb,    0,          OP_MOVSB,   OP_PARM_Xb,         OP_PARM_Yb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movsw/d %Xv,%Yv",    IDX_ParseXv,        IDX_ParseYv,    0,          OP_MOVSWD,  OP_PARM_Xv,         OP_PARM_Yv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmpsb %Xb,%Yb",      IDX_ParseXb,        IDX_ParseYb,    0,          OP_CMPSB,   OP_PARM_Xb,         OP_PARM_Yb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmpsw/d %Xv,%Yv",    IDX_ParseXv,        IDX_ParseYv,    0,          OP_CMPWD,   OP_PARM_Xv,         OP_PARM_Yv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("test AL,%Ib",        IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_TEST,    OP_PARM_REG_AL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("test %eAX,%Iv",      IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_TEST,    OP_PARM_REG_EAX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("stosb %Yb,AL",       IDX_ParseYb,        IDX_ParseFixedReg, 0,       OP_STOSB,   OP_PARM_Yb,         OP_PARM_REG_AL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("stosw/d %Yv,%eAX",   IDX_ParseYv,        IDX_ParseFixedReg, 0,       OP_STOSWD,  OP_PARM_Yv,         OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lodsb AL,%Xb",       IDX_ParseFixedReg,  IDX_ParseXb,    0,          OP_LODSB,   OP_PARM_REG_AL,     OP_PARM_Xb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lodsw/d %eAX,%Xv",   IDX_ParseFixedReg,  IDX_ParseXv,    0,          OP_LODSWD,  OP_PARM_REG_EAX,    OP_PARM_Xv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("scasb AL,%Xb",       IDX_ParseFixedReg,  IDX_ParseXb,    0,          OP_SCASB,   OP_PARM_REG_AL,     OP_PARM_Xb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("scasw/d %eAX,%Xv",   IDX_ParseFixedReg,  IDX_ParseXv,    0,          OP_SCASWD,  OP_PARM_REG_EAX,    OP_PARM_Xv,     OP_PARM_NONE,   OPTYPE_HARMLESS),


    /* B */
    OP("mov AL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_AL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov CL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_CL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov DL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_DL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov BL,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_BL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov AH,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_AH,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov CH,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_CH,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov DH,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_DH,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov BH,%Ib",         IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_MOV,     OP_PARM_REG_BH,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eAX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_EAX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eCX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_ECX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eDX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_EDX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eBX,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_EBX,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eSP,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_ESP,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eBP,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_EBP,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eSI,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_ESI,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %eDI,%Iv",       IDX_ParseFixedReg,  IDX_ParseImmV,  0,          OP_MOV,     OP_PARM_REG_EDI,    OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* C */
    OP("Shift Grp2 %Eb,%Ib", IDX_ParseShiftGrp2, 0,          0,             OP_SHIFT_GRP2, OP_PARM_Eb,      OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Shift Grp2 %Ev,%Ib", IDX_ParseShiftGrp2, 0,          0,             OP_SHIFT_GRP2, OP_PARM_Ev,      OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("retn %Iw",           IDX_ParseImmUshort, 0,          0,             OP_RETN,     OP_PARM_Iw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("retn",               0,                  0,          0,             OP_RETN,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("les %Gv,%Mp",        IDX_ParseModRM,     IDX_ParseImmAddr,          0,           OP_LES,     OP_PARM_Gv,         OP_PARM_Mp,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lds %Gv,%Mp",        IDX_ParseModRM,     IDX_ParseImmAddr,          0,           OP_LDS,     OP_PARM_Gv,         OP_PARM_Mp,     OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_RRM_DANGEROUS),
    OP("mov %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,          0,           OP_MOV,     OP_PARM_Eb,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mov %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,             0,           OP_MOV,     OP_PARM_Ev,         OP_PARM_Iv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("enter %Iw,%Ib",      IDX_ParseImmUshort, IDX_ParseImmByte,          0,           OP_ENTER,   OP_PARM_Iw,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("leave",              0,                  0,                         0,           OP_LEAVE,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("retf %Iw",           IDX_ParseImmUshort, 0,                         0,           OP_RETF,    OP_PARM_Iw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("retf",               0,                  0,                         0,           OP_RETF,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("int 3",              0,                  0,                         0,           OP_INT3,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_INTERRUPT),
    OP("int %Ib",            IDX_ParseImmByte,   0,                         0,           OP_INT,     OP_PARM_Ib,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_INTERRUPT),
    OP("into",               0,                  0,                         0,           OP_INTO,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_INTERRUPT),
    OP("iret",               0,                  0,                         0,           OP_IRET,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),


    /* D */
    OP("Shift Grp2 %Eb,1",   IDX_ParseShiftGrp2, 0,          0,          OP_SHIFT_GRP2, OP_PARM_Eb,      OP_PARM_1,      OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Shift Grp2 %Ev,1",   IDX_ParseShiftGrp2, 0,          0,          OP_SHIFT_GRP2, OP_PARM_Ev,      OP_PARM_1,      OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Shift Grp2 %Eb,CL",  IDX_ParseShiftGrp2, IDX_ParseFixedReg, 0,       OP_SHIFT_GRP2, OP_PARM_Eb,      OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Shift Grp2 %Ev,CL",  IDX_ParseShiftGrp2, IDX_ParseFixedReg, 0,       OP_SHIFT_GRP2, OP_PARM_Ev,      OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("aam %Ib",            IDX_ParseImmByte,   0,          0,          OP_AAM,     OP_PARM_Ib,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("aad %Ib",            IDX_ParseImmByte,   0,          0,          OP_AAD,     OP_PARM_Ib,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    /* setalc?? */
    INVALID_OPCODE,
    OP("xlat",               0,              0,          0,          OP_XLAT,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf0",           IDX_ParseEscFP,     0,          0,          OP_ESCF0,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf1",           IDX_ParseEscFP,     0,          0,          OP_ESCF1,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf2",           IDX_ParseEscFP,     0,          0,          OP_ESCF2,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf3",           IDX_ParseEscFP,     0,          0,          OP_ESCF3,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf4",           IDX_ParseEscFP,     0,          0,          OP_ESCF4,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf5",           IDX_ParseEscFP,     0,          0,          OP_ESCF5,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf6",           IDX_ParseEscFP,     0,          0,          OP_ESCF6,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("esc 0xf7",           IDX_ParseEscFP,     0,          0,          OP_ESCF7,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),


    /* E */
    OP("loopne %Jb",         IDX_ParseImmBRel,   0,          0,          OP_LOOPNE,  OP_PARM_Jb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("loope %Jb",          IDX_ParseImmBRel,   0,          0,          OP_LOOPE,   OP_PARM_Jb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("loop %Jb",           IDX_ParseImmBRel,   0,          0,          OP_LOOP,    OP_PARM_Jb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("j(e)cxz %Jb",        IDX_ParseImmBRel,   0,          0,          OP_JECXZ,   OP_PARM_Jb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("in AL,%Ib",          IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_IN,      OP_PARM_REG_AL,     OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_READ),
    OP("in %eAX,%Ib",        IDX_ParseFixedReg,  IDX_ParseImmByte,0,         OP_IN,      OP_PARM_REG_EAX,    OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_READ),
    OP("out %Ib,AL",         IDX_ParseImmByte,   IDX_ParseFixedReg,0,        OP_OUT,     OP_PARM_Ib,         OP_PARM_REG_AL, OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_WRITE),
    OP("out %Ib,%eAX",       IDX_ParseImmByte,   IDX_ParseFixedReg,0,        OP_OUT,     OP_PARM_Ib,         OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_WRITE),
    OP("call %Jv",           IDX_ParseImmVRel,   0,          0,          OP_CALL,    OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_RELATIVE_CONTROLFLOW),
    OP("jmp %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JMP,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("jmp %Ap",            IDX_ParseImmAddr,   0,          0,          OP_JMP,     OP_PARM_Ap,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("jmp %Jb",            IDX_ParseImmBRel,   0,          0,          OP_JMP,     OP_PARM_Jb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW | OPTYPE_RELATIVE_CONTROLFLOW),
    OP("in AL,DX",           IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_IN,      OP_PARM_REG_AL,     OP_PARM_REG_DX, OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_READ),
    OP("in %eAX,DX",         IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_IN,      OP_PARM_REG_EAX,    OP_PARM_REG_DX, OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_READ),
    OP("out DX,AL",          IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_OUT,     OP_PARM_REG_DX,     OP_PARM_REG_AL, OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_WRITE),
    OP("out DX,%eAX",        IDX_ParseFixedReg,  IDX_ParseFixedReg, 0,       OP_OUT,     OP_PARM_REG_DX,     OP_PARM_REG_EAX,OP_PARM_NONE,   OPTYPE_PORTIO | OPTYPE_PRIVILEGED | OPTYPE_PORTIO_WRITE),


    /* F */
    OP("lock",               0,              0,          0,          OP_LOCK,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    /* softice bp */
    INVALID_OPCODE,
    OP("repne",              0,              0,          0,          OP_REPNE,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rep(e)",             0,              0,          0,          OP_REPE,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("hlt",                0,              0,          0,          OP_HLT,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_PRIVILEGED),
    OP("cmc",                0,              0,          0,          OP_CMC,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Unary Grp3 %Eb",     IDX_ParseGrp3,      0,          0,          OP_UNARY_GRP3,  OP_PARM_Eb,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Unary Grp3 %Ev",     IDX_ParseGrp3,      0,          0,          OP_UNARY_GRP3,  OP_PARM_Ev,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("clc",                0,              0,          0,          OP_CLC,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("stc",                0,              0,          0,          OP_STC,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cli",                0,              0,          0,          OP_CLI,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("sti",                0,              0,          0,          OP_STI,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED | OPTYPE_INHIBIT_IRQS),
    OP("cld",                0,              0,          0,          OP_CLD,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("std",                0,              0,          0,          OP_STD,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("inc/dec Grp4",       IDX_ParseGrp4,      0,          0,          OP_INC_GRP4, OP_PARM_NONE,      OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Indirect Grp5",      IDX_ParseGrp5,      0,          0,          OP_IND_GRP5, OP_PARM_NONE,      OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


const OPCODE g_aTwoByteMapX86[256] =
{
    /* 0 */
    OP("Grp6",               IDX_ParseGrp6,      0,          0,          OP_GRP6,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Grp7",               IDX_ParseGrp7,      0,          0,          OP_GRP7,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lar %Gv,%Ew",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_LAR,     OP_PARM_Gv,         OP_PARM_Ew,     OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    OP("lsl %Gv,%Ew",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_LSL,     OP_PARM_Gv,         OP_PARM_Ew,     OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    INVALID_OPCODE,
    OP("syscall",            0,              0,          0,          OP_SYSCALL, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW),
    OP("clts",               0,              0,          0,          OP_CLTS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    OP("sysret",             0,              0,          0,          OP_SYSRET,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("invd",               0,              0,          0,          OP_INVD,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    OP("wbinvd",             0,              0,          0,          OP_WBINVD,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    INVALID_OPCODE,
    OP("Two Byte Illegal Opcodes UD2", 0,    0,          0,          OP_ILLUD2,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_ILLEGAL),
    INVALID_OPCODE,
    /* NOP Ev or prefetch (Intel vs AMD) */
    OP("nop %Ev/prefetch",   IDX_ParseModRM, 0,          0,          OP_NOP,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("femms",              0,              0,          0,          OP_FEMMS,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("3DNow! Esc",         IDX_Parse3DNow,     0,          0,          OP_3DNOW,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 1 */
    OP("movups %Vps,%Wps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVUPS,  OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movups %Wps,%Vps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVUPS,  OP_PARM_Wps,        OP_PARM_Vps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    /* can also be movhlps when reg->reg */
    OP("movlps %Wq,%Vq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVLPS,  OP_PARM_Wq,         OP_PARM_Vq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movlps %Vq,%Wq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVLPS,  OP_PARM_Vq,         OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("unpcklps %Vps,%Wq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_UNPCKLPS,OP_PARM_Vps,        OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("unpckhps %Vps,%Wq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_UNPCKHPS,OP_PARM_Vps,        OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    /* can also be movlhps when reg->reg */
    OP("movhps %Wq,%Vq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVHPS,  OP_PARM_Wq,         OP_PARM_Vq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movhps %Vq,%Wq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVHPS,  OP_PARM_Vq,         OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("prefetch Grp16",     IDX_ParseGrp16,     0,          0,          OP_PREFETCH_GRP16,OP_PARM_NONE, OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("nop %Ev",            IDX_ParseModRM, 0,          0,          OP_NOP,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 2 */
    OP("mov %Rd,%Cd",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV_CR,  OP_PARM_Rd,         OP_PARM_Cd,     OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("mov %Rd,%Dd",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV_DR,  OP_PARM_Rd,         OP_PARM_Dd,     OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("mov %Cd,%Rd",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV_CR,  OP_PARM_Cd,         OP_PARM_Rd,     OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("mov %Dd,%Rd",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV_DR,  OP_PARM_Dd,         OP_PARM_Rd,     OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    /* only valid for Pentium Pro & Pentium II */
    OP("mov %Rd,%Td",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV_TR,  OP_PARM_Rd,         OP_PARM_Td,     OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    INVALID_OPCODE,
    /* only valid for Pentium Pro & Pentium II */
    OP("mov %Td,%Rd",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOV_TR,  OP_PARM_Td,         OP_PARM_Rd,     OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    INVALID_OPCODE,

    OP("movaps %Vps,%Wps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVAPS,  OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movaps %Wps,%Vps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVAPS,  OP_PARM_Wps,        OP_PARM_Vps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtpi2ps %Vps,%Qq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPI2PS,OP_PARM_Vps,        OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movntps %Wps,%Vps",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVNTPS, OP_PARM_Wps,        OP_PARM_Vps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvttps2pi %Qq,%Wps", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTTPS2PI,OP_PARM_Qq,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtps2pi %Qq,%Wps",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPS2PI,OP_PARM_Qq,         OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ucomiss %Vss,%Wss",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_UCOMISS, OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("comiss %Vps,%Wps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_COMISS,  OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 3 */
    OP("wrmsr",              0,              0,          0,          OP_WRMSR,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("rdtsc",              0,              0,          0,          OP_RDTSC,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_POTENTIALLY_DANGEROUS),
    OP("rdmsr",              0,              0,          0,          OP_RDMSR,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    OP("rdpmc",              0,              0,          0,          OP_RPPMC,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_PRIVILEGED),
    OP("sysenter",           0,              0,          0,          OP_SYSENTER,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW),
    OP("sysexit",            0,              0,          0,          OP_SYSEXIT, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    /* SSE2 */
    OP("movnti %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVNTI,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 4 */
    OP("cmovo %Gv,%Ev",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVO,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovno %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNO,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovc %Gv,%Ev",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVC,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovnc %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNC,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovz %Gv,%Ev",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVZ,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovnz %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNZ,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovbe %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVBE,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovnbe %Gv,%Ev",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNBE, OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovs %Gv,%Ev",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVS,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovns %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNS,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovp %Gv,%Ev",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVP,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovnp %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNP,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovl %Gv,%Ev",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVL,   OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovnl %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNL,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovle %Gv,%Ev",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVLE,  OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmovnle %Gv,%Ev",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMOVNLE, OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 5 */
    OP("movmskps %Ed,%Vps",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVMSKPS,OP_PARM_Ed,         OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sqrtps %Vps,%Wps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SQRTPS,  OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rsqrtps %Vps,%Wps",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_RSQRTPS, OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcpps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_RCPPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("andps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ANDPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("andnps %Vps,%Wps",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ANDNPS,  OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("orps %Vps,%Wps",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ORPS,    OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xorps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XORPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("addps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADDPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mulps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MULPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtps2pd %Vpd,%Wps", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPS2PD,OP_PARM_Vpd,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtdq2ps %Vps,%Wdq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTDQ2PS,OP_PARM_Vps,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("subps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUBPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("minps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MINPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("divps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_DIVPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("maxps %Vps,%Wps",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MAXPS,   OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 6 */
    OP("punpcklbw %Pq,%Qd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKLBW, OP_PARM_Pq,       OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpcklwd %Pq,%Qd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKLWD, OP_PARM_Pq,       OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckldq %Pq,%Qd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKLDQ, OP_PARM_Pq,       OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("packsswb %Pq,%Qq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PACKSSWB,OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpgtb %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPGTB, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpgtw %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPGTW, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpgtd %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPGTD, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("packuswb %Pq,%Qq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PACKUSWB,OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhbw %Pq,%Qd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHBW, OP_PARM_Pq,       OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhwd %Pq,%Qd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHWD, OP_PARM_Pq,       OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhdq %Pq,%Qd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHDQ, OP_PARM_Pq,       OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("packssdw %Pq,%Qd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PACKSSDW,OP_PARM_Pq,         OP_PARM_Qd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movd %Pd,%Ed",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVD,    OP_PARM_Pd,         OP_PARM_Ed,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movq %Pq,%Qq",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVQ,    OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 7 */
    OP("pshufw %Pq,%Qq,%Ib", IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_PSHUFW, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("Grp12",              IDX_ParseGrp12,     0,          0,          OP_GRP12,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Grp13",              IDX_ParseGrp13,     0,          0,          OP_GRP13,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Grp14",              IDX_ParseGrp14,     0,          0,          OP_GRP14,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpeqb %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPEQB, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpeqw %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPEQW, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpeqd %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPEQD, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("emms",               0,              0,          0,          OP_EMMS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("MMX UD 0x78",        0,              0,          0,          OP_MMX_UD78,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("MMX UD 0x79",        0,              0,          0,          OP_MMX_UD79,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("MMX UD 0x7A",        0,              0,          0,          OP_MMX_UD7A,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("MMX UD 0x7B",        0,              0,          0,          OP_MMX_UD7B,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("MMX UD 0x7C",        0,              0,          0,          OP_MMX_UD7C,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("MMX UD 0x7D",        0,              0,          0,          OP_MMX_UD7D,OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movd %Ed,%Pd",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVD,    OP_PARM_Ed,         OP_PARM_Pd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movq %Qq,%Pq",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVQ,    OP_PARM_Qq,         OP_PARM_Pq,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 8 */
    OP("jo %Jv",             IDX_ParseImmVRel,   0,          0,          OP_JO,      OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jno %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JNO,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jc %Jv",             IDX_ParseImmVRel,   0,          0,          OP_JC,      OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnc %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JNC,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("je %Jv",             IDX_ParseImmVRel,   0,          0,          OP_JE,      OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jne %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JNE,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jbe %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JBE,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnbe %Jv",           IDX_ParseImmVRel,   0,          0,          OP_JNBE,    OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("js %Jv",             IDX_ParseImmVRel,   0,          0,          OP_JS,      OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jns %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JNS,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jp %Jv",             IDX_ParseImmVRel,   0,          0,          OP_JP,      OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnp %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JNP,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jl %Jv",             IDX_ParseImmVRel,   0,          0,          OP_JL,      OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnl %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JNL,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jle %Jv",            IDX_ParseImmVRel,   0,          0,          OP_JLE,     OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),
    OP("jnle %Jv",           IDX_ParseImmVRel,   0,          0,          OP_JNLE,    OP_PARM_Jv,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW|OPTYPE_RELATIVE_CONTROLFLOW|OPTYPE_COND_CONTROLFLOW),

    /* 9 */
    OP("seto %Eb",           IDX_ParseModRM,     0,          0,          OP_SETO,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setno %Eb",          IDX_ParseModRM,     0,          0,          OP_SETNO,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setc %Eb",           IDX_ParseModRM,     0,          0,          OP_SETC,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setnc %Eb",          IDX_ParseModRM,     0,          0,          OP_SETNC,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sete %Eb",           IDX_ParseModRM,     0,          0,          OP_SETE,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setne %Eb",          IDX_ParseModRM,     0,          0,          OP_SETNE,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setbe %Eb",          IDX_ParseModRM,     0,          0,          OP_SETBE,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setnbe %Eb",         IDX_ParseModRM,     0,          0,          OP_SETNBE,  OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sets %Eb",           IDX_ParseModRM,     0,          0,          OP_SETS,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setns %Eb",          IDX_ParseModRM,     0,          0,          OP_SETNS,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setp %Eb",           IDX_ParseModRM,     0,          0,          OP_SETP,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setnp %Eb",          IDX_ParseModRM,     0,          0,          OP_SETNP,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setl %Eb",           IDX_ParseModRM,     0,          0,          OP_SETL,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setnl %Eb",          IDX_ParseModRM,     0,          0,          OP_SETNL,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setle %Eb",          IDX_ParseModRM,     0,          0,          OP_SETLE,   OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("setnle %Eb",         IDX_ParseModRM,     0,          0,          OP_SETNLE,  OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* a */
    OP("push fs",            IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_FS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop fs",             IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_FS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cpuid",              0,                  0,          0,          OP_CPUID,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_POTENTIALLY_DANGEROUS),
    OP("bt %Ev,%Gv",         IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BT,      OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shld %Ev,%Gv,%Ib",   IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte, OP_SHLD,  OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("shld %Ev,%Gv,CL",    IDX_ParseModRM,     IDX_UseModRM,   0,      OP_SHLD,  OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("push gs",            IDX_ParseFixedReg,  0,          0,          OP_PUSH,    OP_PARM_REG_GS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pop gs",             IDX_ParseFixedReg,  0,          0,          OP_POP,     OP_PARM_REG_GS,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rsm",                0,              0,          0,          OP_RSM,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bts %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BTS,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shrd %Ev,%Gv,%Ib",   IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_SHRD,   OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("shrd %Ev,%Gv,CL",    IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseFixedReg,OP_SHRD,  OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_REG_CL, OPTYPE_HARMLESS),
    OP("Grp15",              IDX_ParseGrp15,     0,          0,          OP_GRP15,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("imul %Gv,%Ev",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_IMUL,    OP_PARM_Gv,         OP_PARM_Ev,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* b */
    OP("cmpxchg %Eb,%Gb",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMPXCHG, OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmpxchg %Ev,%Gv",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CMPXCHG, OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lss %Gv,%Mp",        IDX_ParseModRM,     IDX_ParseImmAddr,   0,          OP_LSS,     OP_PARM_Gv,         OP_PARM_Mp,     OP_PARM_NONE,   OPTYPE_HARMLESS | OPTYPE_RRM_DANGEROUS),
    OP("btr %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BTR,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lfs %Gv,%Mp",        IDX_ParseModRM,     IDX_ParseImmAddr,   0,          OP_LFS,     OP_PARM_Gv,         OP_PARM_Mp,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("lgs %Gv,%Mp",        IDX_ParseModRM,     IDX_ParseImmAddr,   0,          OP_LGS,     OP_PARM_Gv,         OP_PARM_Mp,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movzx %Gv,%Eb",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVZX,   OP_PARM_Gv,         OP_PARM_Eb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movzx %Gv,%Ew",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVZX,   OP_PARM_Gv,         OP_PARM_Ew,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("Grp10 Invalid Op",   IDX_ParseGrp10,     0,          0,          OP_GRP10_INV,OP_PARM_NONE,      OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("Grp8",               IDX_ParseGrp8,      0,          0,          OP_GRP8,    OP_PARM_Ev,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("btc %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BTC,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bsf %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BSF,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bsr %Ev,%Gv",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_BSR,     OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movsx %Gv,%Eb",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVSX,   OP_PARM_Gv,         OP_PARM_Eb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movsx %Gv,%Ew",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVSX,   OP_PARM_Gv,         OP_PARM_Ew,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* c */
    OP("xadd %Eb,%Gb",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XADD,    OP_PARM_Eb,         OP_PARM_Gb,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xadd %Ev,%Gv",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XADD,    OP_PARM_Ev,         OP_PARM_Gv,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmpps %Vps,%Wps,%Ib",IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte, OP_CMPPS, OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_Ib,     OPTYPE_HARMLESS),
    /* SSE2 */
    OP("movnti %Ed,%Gd",     IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_MOVNTI, OP_PARM_Ed,         OP_PARM_Gd,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pinsrw %Pq,%Ed,%Ib", IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_PINSRW, OP_PARM_Pq,         OP_PARM_Ed,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("pextrw %Gd,%Pq,%Ib", IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_PEXTRW, OP_PARM_Gd,         OP_PARM_Pq,     OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("shufps %Vps,%Wps,%Ib",IDX_ParseModRM,    IDX_UseModRM,   IDX_ParseImmByte,OP_SHUFPS, OP_PARM_Vps,        OP_PARM_Wps,    OP_PARM_Ib,     OPTYPE_HARMLESS),
    OP("Grp9",               IDX_ParseGrp9,      0,          0,          OP_GRP9,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap EAX",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_EAX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap ECX",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_ECX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap EDX",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_EDX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap EBX",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_EBX,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap ESP",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_ESP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap EBP",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_EBP,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap ESI",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_ESI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bswap EDI",          IDX_ParseFixedReg,  0,          0,          OP_BSWAP,   OP_PARM_REG_EDI,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    INVALID_OPCODE,
    OP("psrlw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRLW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrld %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRLD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrlq %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRLQ,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddq %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDQ,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmullw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULLW,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pmovskb %Gd,%Pq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMOVSKB, OP_PARM_Gd,         OP_PARM_Pq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubusb %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBUSB, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubusw %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBUSW, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pminub %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMINUB,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pand %Pq,%Qq",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAND,    OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddusb %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDUSB, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddusw %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDUSW, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmaxub %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMAXUB,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pandn %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PANDN,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* e */
    OP("pavgn %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAVGN,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psraw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRAW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrad %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRAD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pavgw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAVGW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmulhuw %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULHUW, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmulhw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULHW,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("movntq %Wq,%Vq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVNTQ,  OP_PARM_Wq,         OP_PARM_Vq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubsb %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBSB,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubsw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBSW,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pminsw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMINSW,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("por %Pq,%Qq",        IDX_ParseModRM,     IDX_UseModRM,   0,          OP_POR,     OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddsb %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDSB,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddsw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDSW,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmaxsw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMAXSW,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pxor %Pq,%Qq",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PXOR,    OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    INVALID_OPCODE,
    OP("psllw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSLLW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pslld %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSLLD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psllq %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSSQ,    OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmuludq %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULUDQ, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddwd %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDWD,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psadbw %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADBW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("maskmovq %Ppi,%Qpi", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMASKMOVQ, OP_PARM_Ppi,      OP_PARM_Qpi,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubb %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBB,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubd %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubq %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddb %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDB,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddd %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
};

/* Two byte opcode map with prefix 0x66 */
const OPCODE g_aTwoByteMapX86_PF66[256] =
{
    /* 0 */
    INVALID_OPCODE_BLOCK

    /* 1 */
    OP("movupd %Vpd,%Wpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVUPD,  OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movupd %Wpd,%Vpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVUPD,  OP_PARM_Wpd,        OP_PARM_Vpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movlpd %Vq,%Ws",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVLPD,  OP_PARM_Vq,         OP_PARM_Ws,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movlpd %Vq,%Wq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVLPD,  OP_PARM_Vq,         OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("unpcklpd %Vpd,%Wq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_UNPCKLPD,OP_PARM_Vpd,        OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("unpckhpd %Vpd,%Wq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_UNPCKHPD,OP_PARM_Vpd,        OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movhpd %Vq,%Wq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVHPD,  OP_PARM_Vq,         OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movhpd %Wq,%Vq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVHPD,  OP_PARM_Wq,         OP_PARM_Vq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 2 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movapd %Vpd,%Wpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVAPD,  OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movapd %Wpd,%Vpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVAPD,  OP_PARM_Wpd,        OP_PARM_Vpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtpi2pd %Vpd,%Qdq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPI2PD,OP_PARM_Vpd,        OP_PARM_Qdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movntpd %Wpd,%Vpd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVNTPD, OP_PARM_Wpd,        OP_PARM_Vpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvttpd2pi %Qdq,%Wpd",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTTPD2PI,OP_PARM_Qdq,       OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtpd2pi %Qdq,%Wpd", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPD2PI,OP_PARM_Qdq,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ucomisd %Vsd,%Wsd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_UCOMISD, OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("comisd %Vpd,%Wpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_COMISD,  OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 3 */
    INVALID_OPCODE_BLOCK

    /* 4 */
    INVALID_OPCODE_BLOCK

    /* 5 */
    OP("movmskpd %Ed,%Vpd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVMSKPD,OP_PARM_Ed,         OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sqrtpd %Vpd,%Wpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SQRTPD,  OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("andpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ANDPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("andnpd %Vps,%Wpd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ANDNPD,  OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("orpd %Vpd,%Wpd",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ORPD,    OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xorpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_XORPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("addpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADDPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mulpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MULPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtpd2ps %Vps,%Wpd", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPD2PS,OP_PARM_Vps,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtps2dq %Vpq,%Wps", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPS2DQ,OP_PARM_Vpq,        OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("subpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUBPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("minpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MINPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("divpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_DIVPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("maxpd %Vpd,%Wpd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MAXPD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 6 */
    OP("punpcklbw %Vdq,%Wdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKLBW, OP_PARM_Vdq,      OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpcklwd %Vdq,%Wdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKLWD, OP_PARM_Vdq,      OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckldq %Vdq,%Wdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKLDQ, OP_PARM_Vdq,      OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("packsswb %Vdq,%Wdq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PACKSSWB,OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpgtb %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPGTB, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpgtw %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPGTW, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpgtd %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPGTD, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("packuswb %Vdq,%Wdq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PACKUSWB,OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhbw %Pdq,%Qdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHBW, OP_PARM_Pdq,      OP_PARM_Qdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhwd %Pdq,%Qdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHWD, OP_PARM_Pdq,      OP_PARM_Qdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhdq %Pdq,%Qdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHDQ, OP_PARM_Pdq,      OP_PARM_Qdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("packssdw %Pdq,%Qdq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PACKSSDW,OP_PARM_Pdq,        OP_PARM_Qdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpcklqdq %Vdq,%Wdq",IDX_ParseModRM,    IDX_UseModRM,   0,          OP_PUNPCKLQDQ,OP_PARM_Vdq,      OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("punpckhqd %Vdq,%Wdq",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PUNPCKHQD, OP_PARM_Vdq,      OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movd %Vdq,%Ed",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVD,    OP_PARM_Vdq,        OP_PARM_Ed,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movdqa %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVDQA,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 7 */
    OP("pshufd %Vdq,%Wdq,%Ib",IDX_ParseModRM,    IDX_UseModRM,   IDX_ParseImmByte,OP_PSHUFD, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_Ib,     OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pcmpeqb %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPEQB, OP_PARM_Vdq,        OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpeqw %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPEQW, OP_PARM_Vdq,        OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pcmpeqd %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PCMPEQD, OP_PARM_Vdq,        OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movd %Ed,%Vdq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVD,    OP_PARM_Ed,         OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movdqa %Qq,%Pq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVDQA,  OP_PARM_Wdq,        OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 8 */
    INVALID_OPCODE_BLOCK

    /* 9 */
    INVALID_OPCODE_BLOCK

    /* a */
    INVALID_OPCODE_BLOCK

    /* b */
    INVALID_OPCODE_BLOCK

    /* c */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cmppd %Vpd,%Wpd,%Ib",IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte, OP_CMPPD, OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_Ib, OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pinsrw %Vdq,%Ed,%Ib",IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_PINSRW, OP_PARM_Vdq,        OP_PARM_Ed,     OP_PARM_Ib, OPTYPE_HARMLESS),
    OP("pextrw %Gd,%Vdq,%Ib",IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_PEXTRW, OP_PARM_Gd,         OP_PARM_Vdq,    OP_PARM_Ib, OPTYPE_HARMLESS),
    OP("shufpd %Vpd,%Wpd,%Ib",IDX_ParseModRM,    IDX_UseModRM,   IDX_ParseImmByte,OP_SHUFPD, OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_Ib, OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* d */
    INVALID_OPCODE,
    OP("psrlw %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRLW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrld %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRLD,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrlq %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRLQ,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddq %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDQ,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmullw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULLW,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movq %Wq,%Vq",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVQ,    OP_PARM_Wq,         OP_PARM_Vq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmovskb %Gd,%Vdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMOVSKB, OP_PARM_Gd,         OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubusb %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBUSB, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubusw %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBUSW, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pminub %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMINUB,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pand %Vdq,%Wdq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAND,    OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddusb %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDUSB, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddusw %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDUSW, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmaxub %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMAXUB,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pandn %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PANDN,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* e */
    OP("pavgn %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAVGN,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psraw %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRAW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrad %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSRAD,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pavgw %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAVGW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmulhuw %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULHUW, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmulhw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULHW,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvttpd2dq %Vdq,%Wpd",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTTPD2DQ,OP_PARM_Vdq,       OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movntdq %Wdq,%Vdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVNTDQ, OP_PARM_Wdq,        OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubsb %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBSB,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubsw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBSW,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pminsw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMINSW,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("por %Vdq,%Wdq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_POR,     OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddsb %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDSB,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddsw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDSW,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmaxsw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMAXSW,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pxor %Vdq,%Wdq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PXOR,    OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    INVALID_OPCODE,
    OP("psllw %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSLLW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pslld %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSLLD,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psllq %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSSQ,    OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pmuludq %Vdq,%Wdq",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PMULUDQ, OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddwd %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDWD,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psadbw %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADBW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("maskmovdqu %Vdq,%Wdq",IDX_ParseModRM,    IDX_UseModRM,   0,          OP_PMASKMOVDQU, OP_PARM_Vdq,    OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubb %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBB,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubw %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubd %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBD,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psubq %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PSUBD,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddb %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDB,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddw %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDW,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("paddd %Vdq,%Wdq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PADDD,   OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
};

/* Two byte opcode map with prefix 0xF2 */
const OPCODE g_aTwoByteMapX86_PFF2[256] =
{
    /* 0 */
    INVALID_OPCODE_BLOCK

    /* 1 */
    OP("movsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVSD,   OP_PARM_Vpd,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movsd %Wsd,%Vsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVSD,   OP_PARM_Wpd,        OP_PARM_Vpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 2 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cvtsi2sd %Vsd,%Ed",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTSI2SD,OP_PARM_Vsd,        OP_PARM_Ed,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("cvttsd2si %Gd,%Wsd", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTTSD2SI,OP_PARM_Gd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtsd2si %Gd,%Wsd",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTSD2SI,OP_PARM_Gd,         OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 3 */
    INVALID_OPCODE_BLOCK

    /* 4 */
    INVALID_OPCODE_BLOCK

    /* 5 */
    INVALID_OPCODE,
    OP("sqrtsd %Vsd,%Wsd",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SQRTSD,  OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("addsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADDSD,   OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mulsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MULSD,   OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtsd2ss %Vss,%Wsd", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTSD2SS,OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("subsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUBSD,   OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("minsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MINSD,   OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("divsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_DIVSD,   OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("maxsd %Vsd,%Wsd",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MAXSD,   OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 6 */
    INVALID_OPCODE_BLOCK

    /* 7 */
    OP("pshuflw %Vdq,%Wdq,%Ib",IDX_ParseModRM,   IDX_UseModRM,   IDX_ParseImmByte,OP_PSHUFLW,    OP_PARM_Vdq,    OP_PARM_Wdq,    OP_PARM_Ib,     OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 8 */
    INVALID_OPCODE_BLOCK

    /* 9 */
    INVALID_OPCODE_BLOCK

    /* a */
    INVALID_OPCODE_BLOCK

    /* b */
    INVALID_OPCODE_BLOCK

    /* c */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cmpsd %Vsd,%Wsd,%Ib",IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,  OP_CMPSD,    OP_PARM_Vsd,        OP_PARM_Wsd,    OP_PARM_Ib, OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* d */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movdq2q %Pq,%Wq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVDQ2Q, OP_PARM_Pq,         OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* e */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cvtpd2dq %Vdq,%Wpd", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTPD2DQ,OP_PARM_Vdq,        OP_PARM_Wpd,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* f */
    INVALID_OPCODE_BLOCK
};


/* Two byte opcode map with prefix 0xF3 */
const OPCODE g_aTwoByteMapX86_PFF3[256] =
{
    /* 0 */
    INVALID_OPCODE_BLOCK

    /* 1 */
    OP("movss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movss %Wss,%Vss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVSS,   OP_PARM_Wss,        OP_PARM_Vss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 2 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cvtsi2ss %Vss,%Ed",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTSI2SS,OP_PARM_Vss,        OP_PARM_Ed,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("cvttss2si %Gd,%Wss", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTTSS2SI,OP_PARM_Gd,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvtss2si %Gd,%Wss",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTSS2SI,OP_PARM_Gd,         OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 3 */
    INVALID_OPCODE_BLOCK

    /* 4 */
    INVALID_OPCODE_BLOCK

    /* 5 */
    INVALID_OPCODE,
    OP("sqrtss %Vss,%Wss",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SQRTSS,  OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rsqrtss %Vss,%Wss",  IDX_ParseModRM,     IDX_UseModRM,   0,          OP_RSQRTSS, OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("addss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_ADDSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mulss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MULSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    //??
    OP("cvtss2sd %Vss,%Wss", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTSD2SS,OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cvttps2dq %Vdq,%Wps",IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTTPS2DQ,OP_PARM_Vdq,       OP_PARM_Wps,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("subss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_SUBSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("minss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MINSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("divss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_DIVSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("maxss %Vss,%Wss",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MAXSS,   OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 6 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movdqu %Vdq,%Wdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVDQU,  OP_PARM_Vdq,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 7 */
    OP("pshufhw %Vdq,%Wdq,%Ib",IDX_ParseModRM,   IDX_UseModRM,   IDX_ParseImmByte,OP_PSHUFHW,    OP_PARM_Vdq,    OP_PARM_Wdq,    OP_PARM_Ib,     OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movq %Vq,%Wq",       IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVQ,    OP_PARM_Vq,         OP_PARM_Wq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("movdqu %Wdq,%Vdq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVDQU,  OP_PARM_Wdq,        OP_PARM_Vdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 8 */
    INVALID_OPCODE_BLOCK

    /* 9 */
    INVALID_OPCODE_BLOCK

    /* a */
    INVALID_OPCODE_BLOCK

    /* b */
    INVALID_OPCODE_BLOCK

    /* c */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cmpss %Vss,%Wss,%Ib",IDX_ParseModRM,     IDX_UseModRM,   IDX_ParseImmByte,OP_CMPSS,  OP_PARM_Vss,        OP_PARM_Wss,    OP_PARM_Ib,     OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* d */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("movq2dq %Vdq,%Qq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_MOVQ2DQ, OP_PARM_Vdq,        OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* e */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("cvtdq2pd %Vpd,%Wdq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_CVTDQ2PD,OP_PARM_Vpd,        OP_PARM_Wdq,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* f */
    INVALID_OPCODE_BLOCK
};

/* 3DNow! map (0x0F 0x0F prefix) */
const OPCODE g_aTwoByteMapX86_3DNow[256] =
{
    /* 0 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pi2fw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PI2FW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pi2fd %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PI2FD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 1 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pf2iw %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PF2IW,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pf2id %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PF2ID,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* 2 */
    INVALID_OPCODE_BLOCK

    /* 3 */
    INVALID_OPCODE_BLOCK

    /* 4 */
    INVALID_OPCODE_BLOCK

    /* 5 */
    INVALID_OPCODE_BLOCK

    /* 6 */
    INVALID_OPCODE_BLOCK

    /* 7 */
    INVALID_OPCODE_BLOCK

    /* 8 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfnacc %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFNACC,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfpnacc %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFPNACC, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* 9 */
    OP("pfcmpge %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFCMPGE, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfmin %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFMIN,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pfrcp %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFRCP,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pfrsqrt %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFRSQRT, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfsub %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFSUB,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfadd %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFADD,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* a */
    OP("pfcmpgt %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFCMPGT, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfmax %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFMAX,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pfrcpit1 %Pq,%Qq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFRCPIT1,OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pfrsqrtit1 %Pq,%Qq", IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFRSQRTIT1,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfsubr %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFSUBR,  OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfacc %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFACC,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* b */
    OP("pfcmpeq %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFCMPEQ, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pfmul %Pq,%Qq",      IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFMUL,   OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pfrcpit2 %Pq,%Qq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFRCPIT2,OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pfmulhrw %Pq,%Qq",   IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFMULHRW,OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pswapd %Pq,%Qq",     IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PFSWAPD, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("pavgusb %Pq,%Qq",    IDX_ParseModRM,     IDX_UseModRM,   0,          OP_PAVGUSB, OP_PARM_Pq,         OP_PARM_Qq,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* c */
    INVALID_OPCODE_BLOCK

    /* d */
    INVALID_OPCODE_BLOCK

    /* e */
    INVALID_OPCODE_BLOCK

    /* f */
    INVALID_OPCODE_BLOCK
};



/* Floating point opcode starting with escape byte 0xD8 (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF0_Low[8] =
{
    /* 0 */
    OP("fadd %Md",           IDX_ParseModRM,     0,          0,          OP_FADD,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul %Md",           IDX_ParseModRM,     0,          0,          OP_FMUL,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom %Md",           IDX_ParseModRM,     0,          0,          OP_FCOM,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp %Md",          IDX_ParseModRM,     0,          0,          OP_FCOMP,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub %Md",           IDX_ParseModRM,     0,          0,          OP_FSUB,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr %Md",          IDX_ParseModRM,     0,          0,          OP_FSUBR,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv %Md",           IDX_ParseModRM,     0,          0,          OP_FDIV,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr %Md",          IDX_ParseModRM,     0,          0,          OP_FDIVR,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};

/* Floating point opcode starting with escape byte 0xD8 (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF0_High[16*4] =
{
    /* c */
    OP("fadd ST(0),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(1)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(2)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(3)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(4)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(5)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(6)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(0),ST(7)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(1)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(2)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(3)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(4)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(5)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(6)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(7)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    OP("fcom ST(0),ST(0)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(1)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(2)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(3)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(4)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(5)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(6)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom ST(0),ST(7)",   0,              0,          0,          OP_FCOM,    OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(0)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(1)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(2)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(3)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(4)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(5)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(6)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp ST(0),ST(7)",  0,              0,          0,          OP_FCOMP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),


    /* e */
    OP("fsub ST(0),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(1)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(2)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(3)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(4)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(5)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(6)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(7)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(1)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(2)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(3)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(4)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(5)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(6)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(0),ST(7)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    OP("fdiv ST(0),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(1)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(2)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(3)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(4)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(5)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(6)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(7)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(1)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(2)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(3)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(4)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(5)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(6)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(0),ST(7)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
};

/* Floating point opcode starting with escape byte 0xD9 (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF1_Low[8] =
{
    /* 0 */
    OP("fld %Md",            IDX_ParseModRM,     0,          0,          OP_FLD,     OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("fst %Md",            IDX_ParseModRM,     0,          0,          OP_FST,     OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp %Md",           IDX_ParseModRM,     0,          0,          OP_FSTP,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    //TODO:??
    OP("fldenv %M",          IDX_ParseModRM,     0,          0,          OP_FLDENV,  OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldcw %Ew",          IDX_ParseModRM,     0,          0,          OP_FSUBR,   OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    //TODO:??
    OP("fstenv %M",          IDX_ParseModRM,     0,          0,          OP_FSTENV,  OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstcw %Ew",          IDX_ParseModRM,     0,          0,          OP_FSTCW,   OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xD9 (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF1_High[16*4] =
{
    /* c */
    OP("fld ST(0),ST(0)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(1)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(2)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(3)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(4)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(5)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(6)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fld ST(0),ST(7)",    0,              0,          0,          OP_FLD,     OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(0)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(1)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(2)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(3)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(4)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(5)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(6)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxch ST(0),ST(7)",   0,              0,          0,          OP_FXCH,    OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    OP("fnop",               0,              0,          0,          OP_FNOP,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,


    /* e */
    OP("fchs",               0,              0,          0,          OP_FCHS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fabs",               0,              0,          0,          OP_FABS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("ftst",               0,              0,          0,          OP_FCHS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxam",               0,              0,          0,          OP_FCHS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("fld1",               0,              0,          0,          OP_FLD1,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldl2t",             0,              0,          0,          OP_FLDL2T,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldl2e",             0,              0,          0,          OP_FLDL2E,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldpi",              0,              0,          0,          OP_FLDPI,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldlg2",             0,              0,          0,          OP_FLDLG2,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldln2",             0,              0,          0,          OP_FLDLN2,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fldz",               0,              0,          0,          OP_FLDZ,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* f */
    OP("f2xm1",              0,              0,          0,          OP_F2XM1,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fyl2x",              0,              0,          0,          OP_FYL2X,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fptan",              0,              0,          0,          OP_FPTAN,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fpatan",             0,              0,          0,          OP_FPATAN,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxtract",            0,              0,          0,          OP_FXTRACT, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("frem1",              0,              0,          0,          OP_FREM1,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdecstp",            0,              0,          0,          OP_FDECSTP, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fincstp",            0,              0,          0,          OP_FINCSTP, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fprem",              0,              0,          0,          OP_FPREM,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fyl2xp1",            0,              0,          0,          OP_FYL2XP1, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsqrt",              0,              0,          0,          OP_FSQRT,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsincos",            0,              0,          0,          OP_FSINCOS, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("frndint",            0,              0,          0,          OP_FRNDINT, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fscale",             0,              0,          0,          OP_FSCALE,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsin",               0,              0,          0,          OP_FSIN,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcos",               0,              0,          0,          OP_FCOS,    OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xDA (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF2_Low[8] =
{
    /* 0 */
    OP("fiadd %Md",          IDX_ParseModRM,     0,          0,          OP_FIADD,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fimul %Md",          IDX_ParseModRM,     0,          0,          OP_FIMUL,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ficom %Md",          IDX_ParseModRM,     0,          0,          OP_FICOM,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ficomp %Md",         IDX_ParseModRM,     0,          0,          OP_FICOMP,  OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fisub %Md",          IDX_ParseModRM,     0,          0,          OP_FISUB,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fisubr %Md",         IDX_ParseModRM,     0,          0,          OP_FISUBR,  OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fidiv %Md",          IDX_ParseModRM,     0,          0,          OP_FIDIV,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fidivr %Md",         IDX_ParseModRM,     0,          0,          OP_FIDIVR,  OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xD9 (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF2_High[16*4] =
{
    /* c */
    OP("fcmovb ST(0),ST(0)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(1)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(2)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(3)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(4)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(5)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(6)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovb ST(0),ST(7)", 0,              0,          0,          OP_FCMOVB,  OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(0)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(1)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(2)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(3)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(4)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(5)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(6)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmove ST(0),ST(7)", 0,              0,          0,          OP_FCMOVE,  OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    OP("fcmovbe ST(0),ST(0)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(1)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(2)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(3)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(4)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(5)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(6)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovbe ST(0),ST(7)",0,              0,          0,          OP_FCMOVBE, OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(0)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(1)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(2)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(3)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(4)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(5)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(6)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovu ST(0),ST(7)", 0,              0,          0,          OP_FCMOVU,  OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* e */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("fucompp",            0,              0,          0,          OP_FUCOMPP, OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* f */
    INVALID_OPCODE_BLOCK
};


/* Floating point opcode starting with escape byte 0xDB (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF3_Low[8] =
{
    /* 0 */
    OP("fild %Md",           IDX_ParseModRM,     0,          0,          OP_FILD,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("fist %Md",           IDX_ParseModRM,     0,          0,          OP_FIST,    OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fistp %Md",          IDX_ParseModRM,     0,          0,          OP_FISTP,   OP_PARM_Md,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("fld %Mq",            IDX_ParseModRM,     0,          0,          OP_FLD,     OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("fstp %Mq",           IDX_ParseModRM,     0,          0,          OP_FSTP,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xDB (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF3_High[16*4] =
{
    /* c */
    OP("fcmovnb ST(0),ST(0)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(1)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(2)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(3)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(4)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(5)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(6)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnb ST(0),ST(7)",0,              0,          0,          OP_FCMOVNB, OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(0)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(1)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(2)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(3)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(4)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(5)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(6)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovne ST(0),ST(7)",0,              0,          0,          OP_FCMOVNE, OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    OP("fcmovnbe ST(0),ST(0)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(1)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(2)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(3)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(4)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(5)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(6)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnbe ST(0),ST(7)",0,             0,          0,          OP_FCMOVNBE,OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(0)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(1)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(2)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(3)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(4)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(5)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(6)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcmovnu ST(0),ST(7)",0,              0,          0,          OP_FCMOVNU, OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* e */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("fclex",              0,              0,          0,          OP_FCLEX,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("finit",              0,              0,          0,          OP_FINIT,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("fucomi ST(0),ST(0)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(1)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(2)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(3)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(4)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(5)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(6)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomi ST(0),ST(7)",0,               0,          0,          OP_FUCOMI,  OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),


    /* e */
    OP("fcomi ST(0),ST(0)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(1)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(2)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(3)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(4)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(5)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(6)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomi ST(0),ST(7)",0,                0,          0,          OP_FCOMI,   OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};


/* Floating point opcode starting with escape byte 0xDC (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF4_Low[8] =
{
    /* 0 */
    OP("fadd %Mq",           IDX_ParseModRM,     0,          0,          OP_FADD,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul %Mq",           IDX_ParseModRM,     0,          0,          OP_FMUL,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcom %Mq",           IDX_ParseModRM,     0,          0,          OP_FCOM,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomp %Mq",          IDX_ParseModRM,     0,          0,          OP_FCOMP,   OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub %Mq",           IDX_ParseModRM,     0,          0,          OP_FSUB,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr %Mq",          IDX_ParseModRM,     0,          0,          OP_FSUBR,   OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv %Mq",           IDX_ParseModRM,     0,          0,          OP_FDIV,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr %Mq",          IDX_ParseModRM,     0,          0,          OP_FDIVR,   OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xDC (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF4_High[16*4] =
{
    /* c */
    OP("fadd ST(0),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(1),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(2),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(3),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(4),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(5),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(6),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fadd ST(7),ST(0)",   0,              0,          0,          OP_FADD,    OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(0),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(1),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(2),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(3),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(4),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(5),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(6),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmul ST(7),ST(0)",   0,              0,          0,          OP_FMUL,    OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    INVALID_OPCODE_BLOCK


    /* e */
    OP("fsubr ST(0),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(1),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(2),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(3),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(4),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(5),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(6),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubr ST(7),ST(0)",  0,              0,          0,          OP_FSUBR,   OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(0),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(1),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(2),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(3),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(4),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(5),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(6),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsub ST(7),ST(0)",   0,              0,          0,          OP_FSUB,    OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    OP("fdivr ST(0),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(1),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(2),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(3),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(4),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(5),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(6),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivr ST(7),ST(0)",  0,              0,          0,          OP_FDIVR,   OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(0),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(1),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(2),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(3),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(4),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(5),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(6),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdiv ST(7),ST(0)",   0,              0,          0,          OP_FDIV,    OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xDD (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF5_Low[8] =
{
    /* 0 */
    OP("fld %Mq",            IDX_ParseModRM,     0,          0,          OP_FLD,     OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE, OPTYPE_HARMLESS /* fixme: wasn't initialized! */),
    INVALID_OPCODE,
    OP("fst %Mq",            IDX_ParseModRM,     0,          0,          OP_FST,     OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE, OPTYPE_HARMLESS /* fixme: wasn't initialized! */),
    OP("fstp %Mq",           IDX_ParseModRM,     0,          0,          OP_FSTP,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE, OPTYPE_HARMLESS /* fixme: wasn't initialized! */),
    OP("frstor %M",          IDX_ParseModRM,     0,          0,          OP_FRSTOR,  OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE, OPTYPE_HARMLESS /* fixme: wasn't initialized! */),
    INVALID_OPCODE,
    OP("fsave %M",           IDX_ParseModRM,     0,          0,          OP_FSAVE,   OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE, OPTYPE_HARMLESS /* fixme: wasn't initialized! */),
    OP("fstsw %Mw",          IDX_ParseModRM,     0,          0,          OP_FSTSW,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE, OPTYPE_HARMLESS /* fixme: wasn't initialized! */),
};


/* Floating point opcode starting with escape byte 0xDD (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF5_High[16*4] =
{
    /* c */
    OP("ffree ST(0)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_0,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(1)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_1,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(2)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_2,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(3)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_3,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(4)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_4,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(5)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_5,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(6)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_6,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ffree ST(7)",        0,              0,          0,          OP_FFREE,   OP_PARM_REGFP_7,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,

    /* d */
    OP("fst ST(0)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_0,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(1)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_1,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(2)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_2,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(3)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_3,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(4)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_4,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(5)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_5,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(6)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_6,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fst ST(7)",      0,                  0,          0,          OP_FST,     OP_PARM_REGFP_7,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(0)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_0,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(1)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_1,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(2)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_2,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(3)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_3,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(4)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_4,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(5)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_5,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(6)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_6,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fstp ST(7)",     0,                  0,          0,          OP_FSTP,    OP_PARM_REGFP_7,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),


    /* e */
    OP("fucom ST(0)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_0,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(1)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_1,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(2)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_2,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(3)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_3,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(4)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_4,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(5)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_5,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(6)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_6,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucom ST(7)",        0,              0,          0,          OP_FUCOM,   OP_PARM_REGFP_7,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(0)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_0,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(1)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_1,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(2)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_2,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(3)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_3,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(4)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_4,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(5)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_5,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(6)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_6,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomp ST(7)",       0,              0,          0,          OP_FUCOMP,  OP_PARM_REGFP_7,    OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    INVALID_OPCODE_BLOCK
};



/* Floating point opcode starting with escape byte 0xDE (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF6_Low[8] =
{
    /* 0 */
    OP("fiadd %Mw",          IDX_ParseModRM,     0,          0,          OP_FIADD,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fimul %Mw",          IDX_ParseModRM,     0,          0,          OP_FIMUL,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ficom %Mw",          IDX_ParseModRM,     0,          0,          OP_FICOM,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ficomp %Mw",         IDX_ParseModRM,     0,          0,          OP_FICOMP,  OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fisub %Mw",          IDX_ParseModRM,     0,          0,          OP_FISUB,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fisubr %Mw",         IDX_ParseModRM,     0,          0,          OP_FISUBR,  OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fidiv %Mw",          IDX_ParseModRM,     0,          0,          OP_FIDIV,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fidivr %Mw",         IDX_ParseModRM,     0,          0,          OP_FIDIVR,  OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xDE (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF6_High[16*4] =
{
    /* c */
    OP("faddp ST(0),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(1),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(2),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(3),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(4),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(5),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(6),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("faddp ST(7),ST(0)",  0,              0,          0,          OP_FADDP,   OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(0),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(1),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(2),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(3),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(4),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(5),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(6),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fmulp ST(7),ST(0)",  0,              0,          0,          OP_FMULP,   OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* d */
    INVALID_OPCODE,
    OP("fcompp",             0,              0,          0,          OP_FCOMPP,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,


    /* e */
    OP("fsubrp ST(0),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(1),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(2),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(3),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(4),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(5),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(6),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubrp ST(7),ST(0)", 0,              0,          0,          OP_FSUBRP,  OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(0),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(1),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(2),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(3),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(4),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(5),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(6),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fsubp ST(7),ST(0)",  0,              0,          0,          OP_FSUBP,   OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    OP("fdivrp ST(0),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(1),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(2),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(3),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(4),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(5),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(6),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivrp ST(7),ST(0)", 0,              0,          0,          OP_FDIVRP,  OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(0),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(1),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_1,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(2),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_2,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(3),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_3,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(4),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_4,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(5),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_5,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(6),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_6,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fdivp ST(7),ST(0)",  0,              0,          0,          OP_FDIVP,   OP_PARM_REGFP_7,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
};

/* Floating point opcode starting with escape byte 0xDF (values 0-0xBF)*/
const OPCODE g_aMapX86_EscF7_Low[8] =
{
    /* 0 */
    OP("fild %Mw",           IDX_ParseModRM,     0,          0,          OP_FILD,    OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("fist %Mw",           IDX_ParseModRM,     0,          0,          OP_FIST,    OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fistp %Mw",          IDX_ParseModRM,     0,          0,          OP_FISTP,   OP_PARM_Mw,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fbld %M",            IDX_ParseModRM,     0,          0,          OP_FBLD,    OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fild %Mq",           IDX_ParseModRM,     0,          0,          OP_FILD,    OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fbstp %M",           IDX_ParseModRM,     0,          0,          OP_FBSTP,   OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fistp %Mq",          IDX_ParseModRM,     0,          0,          OP_FISTP,   OP_PARM_Mq,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* Floating point opcode starting with escape byte 0xDF (outside 0-0xBF)*/
const OPCODE g_aMapX86_EscF7_High[16*4] =
{
    /* c */
    INVALID_OPCODE_BLOCK

    /* d */
    INVALID_OPCODE_BLOCK

    /* e */
    OP("fstsw ax",           IDX_ParseFixedReg,  0,          0,          OP_FSTSW,   OP_PARM_REG_AX,     OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("fucomip ST(0),ST(0)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(1)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(2)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(3)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(4)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(5)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(6)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fucomip ST(0),ST(7)",0,              0,          0,          OP_FUCOMIP, OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* f */
    OP("fcomip ST(0),ST(0)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_0,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(1)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_1,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(2)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_2,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(3)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_3,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(4)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_4,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(5)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_5,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(6)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_6,OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fcomip ST(0),ST(7)", 0,              0,          0,          OP_FCOMIP,  OP_PARM_REGFP_0,    OP_PARM_REGFP_7,OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};


PCOPCODE g_paMapX86_FP_Low[8] =
{
    g_aMapX86_EscF0_Low,
    g_aMapX86_EscF1_Low,
    g_aMapX86_EscF2_Low,
    g_aMapX86_EscF3_Low,
    g_aMapX86_EscF4_Low,
    g_aMapX86_EscF5_Low,
    g_aMapX86_EscF6_Low,
    g_aMapX86_EscF7_Low
};

PCOPCODE g_paMapX86_FP_High[8] =
{
    g_aMapX86_EscF0_High,
    g_aMapX86_EscF1_High,
    g_aMapX86_EscF2_High,
    g_aMapX86_EscF3_High,
    g_aMapX86_EscF4_High,
    g_aMapX86_EscF5_High,
    g_aMapX86_EscF6_High,
    g_aMapX86_EscF7_High
};

/* Opcode extensions (Group tables) */
const OPCODE g_aMapX86_Group1[8*4] =
{
    /* 80 */
    OP("add %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ADD, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Eb,%Ib",         IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_OR,  OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ADC, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SBB, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_AND, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SUB, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_XOR, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_CMP, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 81 */
    OP("add %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_ADD, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Ev,%Iv",         IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_OR,  OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_ADC, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_SBB, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_AND, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_SUB, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_XOR, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_CMP, OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 82 */
    OP("add %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ADD, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Eb,%Ib",         IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_OR,  OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ADC, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SBB, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_AND, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SUB, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_XOR, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_CMP, OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* 83 */
    OP("add %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_ADD, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("or %Ev,%Ib",         IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_OR,  OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("adc %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_ADC, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sbb %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_SBB, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("and %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_AND, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sub %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_SUB, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("xor %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_XOR, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("cmp %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByteSX,0,         OP_CMP, OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
};

const OPCODE g_aMapX86_Group2[8*6] =
{
    /* C0 */
    OP("rol %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ROL,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ror %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ROR,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcl %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_RCL,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcr %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_RCR,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Eb,%Ib",    IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SHL,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shr %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SHR,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Eb,%Ib",    IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SHL,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sar %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SAR,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* C1 */
    OP("rol %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ROL,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ror %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_ROR,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcl %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_RCL,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcr %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_RCR,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Ev,%Ib",    IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SHL,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shr %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SHR,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Ev,%Ib",    IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SHL,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sar %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,0,         OP_SAR,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* D0 */
    OP("rol %Eb,1",          IDX_ParseModRM,     0,          0,          OP_ROL,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ror %Eb,1",          IDX_ParseModRM,     0,          0,          OP_ROR,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcl %Eb,1",          IDX_ParseModRM,     0,          0,          OP_RCL,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcr %Eb,1",          IDX_ParseModRM,     0,          0,          OP_RCR,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Eb,1",      IDX_ParseModRM,     0,          0,          OP_SHL,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shr %Eb,1",          IDX_ParseModRM,     0,          0,          OP_SHR,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Eb,1",      IDX_ParseModRM,     0,          0,          OP_SHL,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sar %Eb,1",          IDX_ParseModRM,     0,          0,          OP_SAR,     OP_PARM_Eb,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* D1 */
    OP("rol %Ev,1",          IDX_ParseModRM,     0,          0,          OP_ROL,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ror %Ev,1",          IDX_ParseModRM,     0,          0,          OP_ROR,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcl %Ev,1",          IDX_ParseModRM,     0,          0,          OP_RCL,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcr %Ev,1",          IDX_ParseModRM,     0,          0,          OP_RCR,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Ev,1",      IDX_ParseModRM,     0,          0,          OP_SHL,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shr %Ev,1",          IDX_ParseModRM,     0,          0,          OP_SHR,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Ev,1",      IDX_ParseModRM,     0,          0,          OP_SHL,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sar %Ev,1",          IDX_ParseModRM,     0,          0,          OP_SAR,     OP_PARM_Ev,         OP_PARM_1 ,     OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* D2 */
    OP("rol %Eb,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_ROL,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ror %Eb,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_ROR,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcl %Eb,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_RCL,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcr %Eb,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_RCR,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Eb,CL",     IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SHL,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shr %Eb,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SHR,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Eb,CL",     IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SHL,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sar %Eb,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SAR,     OP_PARM_Eb,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* D3 */
    OP("rol %Ev,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_ROL,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ror %Ev,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_ROR,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcl %Ev,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_RCL,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("rcr %Ev,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_RCR,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Ev,CL",     IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SHL,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shr %Ev,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SHR,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("shl/sal %Ev,CL",     IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SHL,     OP_PARM_Ev,         OP_PARM_REG_CL, OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sar %Ev,CL",         IDX_ParseModRM,     IDX_ParseFixedReg, 0,       OP_SAR,     OP_PARM_Ev,         OP_PARM_REG_CL ,OP_PARM_NONE,   OPTYPE_HARMLESS),

};


const OPCODE g_aMapX86_Group3[8*2] =
{
    /* F6 */
    OP("test %Eb,%Ib",       IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_TEST,   OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    //AMD manual claims test??
    INVALID_OPCODE,
    OP("not %Eb",            IDX_ParseModRM,     0,          0,          OP_NOT,     OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("neg %Eb",            IDX_ParseModRM,     0,          0,          OP_NEG,     OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mul %Eb",            IDX_ParseModRM,     0,          0,          OP_MUL,     OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("imul %Eb",           IDX_ParseModRM,     0,          0,          OP_IMUL,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("div %Eb",            IDX_ParseModRM,     0,          0,          OP_DIV,     OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("idiv %Eb",           IDX_ParseModRM,     0,          0,          OP_IDIV,    OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),

    /* F7 */
    OP("test %Ev,%Iv",       IDX_ParseModRM,     IDX_ParseImmV,  0,          OP_TEST,    OP_PARM_Ev,         OP_PARM_Iv ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    //AMD manual claims test??
    INVALID_OPCODE,
    OP("not %Ev",            IDX_ParseModRM,     0,          0,          OP_NOT,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("neg %Ev",            IDX_ParseModRM,     0,          0,          OP_NEG,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mul %Ev",            IDX_ParseModRM,     0,          0,          OP_MUL,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("imul %Ev",           IDX_ParseModRM,     0,          0,          OP_IMUL,    OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("div %Ev",            IDX_ParseModRM,     0,          0,          OP_DIV,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("idiv %Ev",           IDX_ParseModRM,     0,          0,          OP_IDIV,    OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};

const OPCODE g_aMapX86_Group4[8] =
{
    /* FE */
    OP("inc %Eb",            IDX_ParseModRM,     0,          0,          OP_INC,     OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %Eb",            IDX_ParseModRM,     0,          0,          OP_DEC,     OP_PARM_Eb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};

const OPCODE g_aMapX86_Group5[8] =
{
    /* FF */
    OP("inc %Ev",            IDX_ParseModRM,     0,          0,          OP_INC,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("dec %Ev",            IDX_ParseModRM,     0,          0,          OP_DEC,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("call %Ev",           IDX_ParseModRM,     0,          0,          OP_CALL,    OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW),
    OP("call %Ep",           IDX_ParseModRM,     0,          0,          OP_CALL,    OP_PARM_Ep,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW),
    OP("jmp %Ev",            IDX_ParseModRM,     0,          0,          OP_JMP,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("jmp %Ep",            IDX_ParseModRM,     0,          0,          OP_JMP,     OP_PARM_Ep,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_CONTROLFLOW | OPTYPE_UNCOND_CONTROLFLOW),
    OP("push %Ev",           IDX_ParseModRM,     0,          0,          OP_PUSH,    OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
};


const OPCODE g_aMapX86_Group6[8] =
{
    /* 0F 00 */
    OP("sldt %Ew",           IDX_ParseModRM,     0,          0,          OP_SLDT,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    OP("str %Ev",            IDX_ParseModRM,     0,          0,          OP_STR,     OP_PARM_Ev,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    OP("lldt %Ew",           IDX_ParseModRM,     0,          0,          OP_LLDT,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("ltr %Ew",            IDX_ParseModRM,     0,          0,          OP_LTR,     OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("verr %Ew",           IDX_ParseModRM,     0,          0,          OP_VERR,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    OP("verw %Ew",           IDX_ParseModRM,     0,          0,          OP_VERW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    INVALID_OPCODE,
    INVALID_OPCODE,
};

const OPCODE g_aMapX86_Group7_mem[8] =
{
    /* 0F 01 */
    OP("sgdt %Ms",           IDX_ParseModRM,     0,          0,          OP_SGDT,    OP_PARM_Ms,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    OP("sidt %Ms",           IDX_ParseModRM,     0,          0,          OP_SIDT,    OP_PARM_Ms,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    OP("lgdt %Ms",           IDX_ParseModRM,     0,          0,          OP_LGDT,    OP_PARM_Ms,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("lidt %Ms",           IDX_ParseModRM,     0,          0,          OP_LIDT,    OP_PARM_Ms,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("smsw %Ew",           IDX_ParseModRM,     0,          0,          OP_SMSW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    INVALID_OPCODE,
    OP("lmsw %Ew",           IDX_ParseModRM,     0,          0,          OP_LMSW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    OP("invlpg %Mb",         IDX_ParseModRM,     0,          0,          OP_INVLPG,  OP_PARM_Mb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
};

const OPCODE g_aMapX86_Group7_mod11_rm000[8] =
{
    /* 0F 01 MOD=11b */
    INVALID_OPCODE,
    OP("monitor %eAX,%eCX,%eDX", 0,          0,          0,          OP_MONITOR, OP_PARM_REG_EAX,        OP_PARM_REG_ECX,    OP_PARM_REG_EDX,    OPTYPE_HARMLESS ),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("smsw %Ew",           IDX_ParseModRM,     0,          0,          OP_SMSW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    INVALID_OPCODE,
    OP("lmsw %Ew",           IDX_ParseModRM,     0,          0,          OP_LMSW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    INVALID_OPCODE,
};

const OPCODE g_aMapX86_Group7_mod11_rm001[8] =
{
    /* 0F 01 MOD=11b */
    INVALID_OPCODE,
    OP("mwait %eAX,%eCX",        0,          0,          0,          OP_MWAIT,   OP_PARM_REG_EAX,        OP_PARM_REG_ECX,    OP_PARM_NONE,   OPTYPE_HARMLESS ),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("smsw %Ew",           IDX_ParseModRM,     0,          0,          OP_SMSW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED_NOTRAP),
    INVALID_OPCODE,
    OP("lmsw %Ew",           IDX_ParseModRM,     0,          0,          OP_LMSW,    OP_PARM_Ew,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_DANGEROUS | OPTYPE_PRIVILEGED),
    INVALID_OPCODE,
};

const OPCODE g_aMapX86_Group8[8] =
{
    /* 0F BA */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("bt %Ev,%Ib",         IDX_ParseModRM,     IDX_ParseImmByte,       0,          OP_BT,      OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("bts %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,       0,          OP_BTS,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("btr %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,       0,          OP_BTR,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("btc %Ev,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,       0,          OP_BTC,     OP_PARM_Ev,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
};

const OPCODE g_aMapX86_Group9[8] =
{
    /* 0F C7 */
    INVALID_OPCODE,
    OP("cmpxchg8b %Mq",      IDX_ParseModRM,     0,          0,          OP_CMPXCHG8B, OP_PARM_Mq,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};

const OPCODE g_aMapX86_Group10[8] =
{
    /* 0F B9 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};


const OPCODE g_aMapX86_Group11[8*2] =
{
    /* 0F C6 */
    OP("mov %Eb,%Ib",        IDX_ParseModRM,     IDX_ParseImmByte,       0,          OP_MOV,     OP_PARM_Eb,         OP_PARM_Ib ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    /* 0F C7 */
    OP("mov %Ev,%Iv",        IDX_ParseModRM,     IDX_ParseImmV,      0,          OP_MOV,     OP_PARM_Ev,         OP_PARM_Iz ,    OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};


/* 0xF 0x71 */
const OPCODE g_aMapX86_Group12[8*2] =
{
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psrlw %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLW,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("psraw %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRAW,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("psllw %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLW,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* Group 12 with prefix 0x66 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psrlw %Pdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLW,  OP_PARM_Pdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("psraw %Pdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRAW,  OP_PARM_Pdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("psllw %Pdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLW,  OP_PARM_Pdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
};

/* 0xF 0x72 */
const OPCODE g_aMapX86_Group13[8*2] =
{
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psrld %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLD,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("psrad %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRAD,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pslld %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLD,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* Group 13 with prefix 0x66 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psrld %Wdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLD,  OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("psrad %Wdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRAD,  OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    OP("pslld %Wdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLD,  OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
};

/* 0xF 0x73 */
const OPCODE g_aMapX86_Group14[8*2] =
{
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psrlq %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLQ,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psllq %Pq,%Ib",      IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLQ,  OP_PARM_Pq,         OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,

    /* Group 14 with prefix 0x66 */
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psrlq %Wdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLD,  OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("psrldq %Wdq,%Ib",    IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSRLDQ, OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("psllq %Wdq,%Ib",     IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLD,  OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pslldq %Wdq,%Ib",    IDX_ParseModRM,     IDX_ParseImmByte,0,          OP_PSLLDQ, OP_PARM_Wdq,        OP_PARM_Ib,     OP_PARM_NONE,   OPTYPE_HARMLESS),
};


/* 0xF 0xAE */
const OPCODE g_aMapX86_Group15_mem[8] =
{
    OP("fxsave %M",          IDX_ParseModRM,     0,          0,          OP_FXSAVE,  OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("fxrstor %M",         IDX_ParseModRM,     0,          0,          OP_FXRSTOR, OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("ldmxcsr %M",         IDX_ParseModRM,     0,          0,          OP_LDMXCSR, OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("stmxcsr %M",         IDX_ParseModRM,     0,          0,          OP_STMXCSR, OP_PARM_M,          OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("clflush %Mb",        IDX_ParseModRM,     0,          0,          OP_CLFLUSH, OP_PARM_Mb,         OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};

/* 0xF 0xAE */
const OPCODE g_aMapX86_Group15_mod11_rm000[8] =
{
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    OP("lfence",             IDX_ParseModFence,  0,          0,          OP_LFENCE,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("mfence",             IDX_ParseModFence,  0,          0,          OP_MFENCE,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("sfence",             IDX_ParseModFence,  0,          0,          OP_SFENCE,  OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};

/* 0xF 0x18 */
const OPCODE g_aMapX86_Group16[8] =
{
    OP("prefetchnta %Mb",  IDX_ParseModRM, 0,          0,          OP_PREFETCH,OP_PARM_Mb,        OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("prefetcht0 %Mb",   IDX_ParseModRM, 0,          0,          OP_PREFETCH,OP_PARM_Mb,        OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("prefetcht1 %Mb",   IDX_ParseModRM, 0,          0,          OP_PREFETCH,OP_PARM_Mb,        OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("prefetcht2 %Mb",   IDX_ParseModRM, 0,          0,          OP_PREFETCH,OP_PARM_Mb,        OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
    INVALID_OPCODE,
};

/* 0x90 or 0xF3 0x90 */
const OPCODE g_aMapX86_NopPause[2] =
{
    OP("nop",                0,              0,          0,       OP_NOP,     OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
    OP("pause",              0,              0,          0,       OP_PAUSE,   OP_PARM_NONE,       OP_PARM_NONE,   OP_PARM_NONE,   OPTYPE_HARMLESS),
};

