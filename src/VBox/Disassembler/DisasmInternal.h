/* $Id$ */
/** @file
 * VBox disassembler - Internal header.
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

#ifndef ___DisasmInternal_h___
#define ___DisasmInternal_h___

#include <VBox/types.h>
#include <VBox/dis.h>


/** @defgroup grp_dis_int Internals.
 * @ingroup grp_dis
 * @{
 */

/** @name Index into g_apfnCalcSize and g_apfnFullDisasm.
 * @{ */
#define IDX_ParseNop                0
#define IDX_ParseModRM              1
#define IDX_UseModRM                2
#define IDX_ParseImmByte            3
#define IDX_ParseImmBRel            4
#define IDX_ParseImmUshort          5
#define IDX_ParseImmV               6
#define IDX_ParseImmVRel            7
#define IDX_ParseImmAddr            8
#define IDX_ParseFixedReg           9
#define IDX_ParseImmUlong           10
#define IDX_ParseImmQword           11
#define IDX_ParseTwoByteEsc         12
#define IDX_ParseImmGrpl            13
#define IDX_ParseShiftGrp2          14
#define IDX_ParseGrp3               15
#define IDX_ParseGrp4               16
#define IDX_ParseGrp5               17
#define IDX_Parse3DNow              18
#define IDX_ParseGrp6               19
#define IDX_ParseGrp7               20
#define IDX_ParseGrp8               21
#define IDX_ParseGrp9               22
#define IDX_ParseGrp10              23
#define IDX_ParseGrp12              24
#define IDX_ParseGrp13              25
#define IDX_ParseGrp14              26
#define IDX_ParseGrp15              27
#define IDX_ParseGrp16              28
#define IDX_ParseModFence           29
#define IDX_ParseYv                 30
#define IDX_ParseYb                 31
#define IDX_ParseXv                 32
#define IDX_ParseXb                 33
#define IDX_ParseEscFP              34
#define IDX_ParseNopPause           35
#define IDX_ParseImmByteSX          36
#define IDX_ParseImmZ               37
#define IDX_ParseThreeByteEsc4      38
#define IDX_ParseThreeByteEsc5      39
#define IDX_ParseImmAddrF           40
#define IDX_ParseMax                (IDX_ParseImmAddrF+1)
/** @}  */


/** @name Opcode maps.
 * @{ */
extern const DISOPCODE g_InvalidOpcode[1];

extern const DISOPCODE g_aOneByteMapX86[256];
extern const DISOPCODE g_aOneByteMapX64[256];
extern const DISOPCODE g_aTwoByteMapX86[256];

/** Two byte opcode map with prefix 0x66 */
extern const DISOPCODE g_aTwoByteMapX86_PF66[256];

/** Two byte opcode map with prefix 0xF2 */
extern const DISOPCODE g_aTwoByteMapX86_PFF2[256];

/** Two byte opcode map with prefix 0xF3 */
extern const DISOPCODE g_aTwoByteMapX86_PFF3[256];

/** Three byte opcode map (0xF 0x38) */
extern PCDISOPCODE const g_apThreeByteMapX86_0F38[16];

/** Three byte opcode map with prefix 0x66 (0xF 0x38) */
extern PCDISOPCODE const g_apThreeByteMapX86_660F38[16];

/** Three byte opcode map with prefix 0xF2 (0xF 0x38) */
extern PCDISOPCODE const g_apThreeByteMapX86_F20F38[16];

/** Three byte opcode map with prefix 0x66 (0xF 0x3A) */
extern PCDISOPCODE const g_apThreeByteMapX86_660F3A[16];
/** @} */

/** @name Opcode extensions (Group tables)
 * @{ */
extern const DISOPCODE g_aMapX86_Group1[8*4];
extern const DISOPCODE g_aMapX86_Group2[8*6];
extern const DISOPCODE g_aMapX86_Group3[8*2];
extern const DISOPCODE g_aMapX86_Group4[8];
extern const DISOPCODE g_aMapX86_Group5[8];
extern const DISOPCODE g_aMapX86_Group6[8];
extern const DISOPCODE g_aMapX86_Group7_mem[8];
extern const DISOPCODE g_aMapX86_Group7_mod11_rm000[8];
extern const DISOPCODE g_aMapX86_Group7_mod11_rm001[8];
extern const DISOPCODE g_aMapX86_Group8[8];
extern const DISOPCODE g_aMapX86_Group9[8];
extern const DISOPCODE g_aMapX86_Group10[8];
extern const DISOPCODE g_aMapX86_Group11[8*2];
extern const DISOPCODE g_aMapX86_Group12[8*2];
extern const DISOPCODE g_aMapX86_Group13[8*2];
extern const DISOPCODE g_aMapX86_Group14[8*2];
extern const DISOPCODE g_aMapX86_Group15_mem[8];
extern const DISOPCODE g_aMapX86_Group15_mod11_rm000[8];
extern const DISOPCODE g_aMapX86_Group16[8];
extern const DISOPCODE g_aMapX86_NopPause[2];
/** @} */

/** 3DNow! map (0x0F 0x0F prefix) */
extern const DISOPCODE g_aTwoByteMapX86_3DNow[256];

/** Floating point opcodes starting with escape byte 0xDF
 * @{ */
extern const DISOPCODE g_aMapX86_EscF0_Low[8];
extern const DISOPCODE g_aMapX86_EscF0_High[16*4];
extern const DISOPCODE g_aMapX86_EscF1_Low[8];
extern const DISOPCODE g_aMapX86_EscF1_High[16*4];
extern const DISOPCODE g_aMapX86_EscF2_Low[8];
extern const DISOPCODE g_aMapX86_EscF2_High[16*4];
extern const DISOPCODE g_aMapX86_EscF3_Low[8];
extern const DISOPCODE g_aMapX86_EscF3_High[16*4];
extern const DISOPCODE g_aMapX86_EscF4_Low[8];
extern const DISOPCODE g_aMapX86_EscF4_High[16*4];
extern const DISOPCODE g_aMapX86_EscF5_Low[8];
extern const DISOPCODE g_aMapX86_EscF5_High[16*4];
extern const DISOPCODE g_aMapX86_EscF6_Low[8];
extern const DISOPCODE g_aMapX86_EscF6_High[16*4];
extern const DISOPCODE g_aMapX86_EscF7_Low[8];
extern const DISOPCODE g_aMapX86_EscF7_High[16*4];

extern const PCDISOPCODE g_apMapX86_FP_Low[8];
extern const PCDISOPCODE g_apMapX86_FP_High[8];
/** @} */

/** @def OP
 * Wrapper which initializes an OPCODE.
 * We must use this so that we can exclude unused fields in order
 * to save precious bytes in the GC version.
 *
 * @internal
 */
#ifndef DIS_CORE_ONLY
# define OP(pszOpcode, idxParse1, idxParse2, idxParse3, opcode, param1, param2, param3, optype) \
    { pszOpcode, idxParse1, idxParse2, idxParse3, 0, opcode, param1, param2, param3, optype }
#else
# define OP(pszOpcode, idxParse1, idxParse2, idxParse3, opcode, param1, param2, param3, optype) \
    { idxParse1, idxParse2, idxParse3, 0, opcode, param1, param2, param3, optype }
#endif


size_t disFormatBytes(PCDISCPUSTATE pDis, char *pszDst, size_t cchDst, uint32_t fFlags);

/** @} */
#endif

