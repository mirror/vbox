/** @file
 * DIS - The VirtualBox Disassembler.
 */

/*
 * Copyright (C) 2006-2023 Oracle and/or its affiliates.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL), a copy of it is provided in the "COPYING.CDDL" file included
 * in the VirtualBox distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * SPDX-License-Identifier: GPL-3.0-only OR CDDL-1.0
 */

#ifndef VBOX_INCLUDED_dis_x86_amd64_h
#define VBOX_INCLUDED_dis_x86_amd64_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/disopcode-x86-amd64.h>
#include <iprt/assert.h>


RT_C_DECLS_BEGIN

/** @addtogroup grp_dis   VBox Disassembler
 * @{ */

/** @name Prefix byte flags (DISSTATE::fPrefix).
 * @{
 */
#define DISPREFIX_NONE                  UINT8_C(0x00)
/** non-default address size. */
#define DISPREFIX_ADDRSIZE              UINT8_C(0x01)
/** non-default operand size. */
#define DISPREFIX_OPSIZE                UINT8_C(0x02)
/** lock prefix. */
#define DISPREFIX_LOCK                  UINT8_C(0x04)
/** segment prefix. */
#define DISPREFIX_SEG                   UINT8_C(0x08)
/** rep(e) prefix (not a prefix, but we'll treat is as one). */
#define DISPREFIX_REP                   UINT8_C(0x10)
/** rep(e) prefix (not a prefix, but we'll treat is as one). */
#define DISPREFIX_REPNE                 UINT8_C(0x20)
/** REX prefix (64 bits) */
#define DISPREFIX_REX                   UINT8_C(0x40)
/** @} */

/** @name VEX.Lvvvv prefix destination register flag.
 *  @{
 */
#define VEX_LEN256                      UINT8_C(0x01)
#define VEXREG_IS256B(x)                   ((x) & VEX_LEN256)
/* Convert second byte of VEX prefix to internal format */
#define VEX_2B2INT(x)                   ((((x) >> 2) & 0x1f))
#define VEX_HAS_REX_R(x)                  (!((x) & 0x80))

#define DISPREFIX_VEX_FLAG_W            UINT8_C(0x01)
 /** @} */

/** @name 64 bits prefix byte flags (DISSTATE::fRexPrefix).
 * Requires VBox/disopcode.h.
 * @{
 */
#define DISPREFIX_REX_OP_2_FLAGS(a)     (a - OP_PARM_REX_START)
/*#define DISPREFIX_REX_FLAGS             DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX) - 0, which is no flag */
#define DISPREFIX_REX_FLAGS_B           DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_B)
#define DISPREFIX_REX_FLAGS_X           DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_X)
#define DISPREFIX_REX_FLAGS_XB          DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_XB)
#define DISPREFIX_REX_FLAGS_R           DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_R)
#define DISPREFIX_REX_FLAGS_RB          DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_RB)
#define DISPREFIX_REX_FLAGS_RX          DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_RX)
#define DISPREFIX_REX_FLAGS_RXB         DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_RXB)
#define DISPREFIX_REX_FLAGS_W           DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_W)
#define DISPREFIX_REX_FLAGS_WB          DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WB)
#define DISPREFIX_REX_FLAGS_WX          DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WX)
#define DISPREFIX_REX_FLAGS_WXB         DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WXB)
#define DISPREFIX_REX_FLAGS_WR          DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WR)
#define DISPREFIX_REX_FLAGS_WRB         DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WRB)
#define DISPREFIX_REX_FLAGS_WRX         DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WRX)
#define DISPREFIX_REX_FLAGS_WRXB        DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WRXB)
/** @} */
AssertCompile(RT_IS_POWER_OF_TWO(DISPREFIX_REX_FLAGS_B));
AssertCompile(RT_IS_POWER_OF_TWO(DISPREFIX_REX_FLAGS_X));
AssertCompile(RT_IS_POWER_OF_TWO(DISPREFIX_REX_FLAGS_W));
AssertCompile(RT_IS_POWER_OF_TWO(DISPREFIX_REX_FLAGS_R));


/** @name 64-bit general register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxGenReg and DISOPPARAM::Index.idxGenReg.
 * @note  Safe to assume same values as the 16-bit and 32-bit general registers.
 * @{
 */
#define DISGREG_RAX                     UINT8_C(0)
#define DISGREG_RCX                     UINT8_C(1)
#define DISGREG_RDX                     UINT8_C(2)
#define DISGREG_RBX                     UINT8_C(3)
#define DISGREG_RSP                     UINT8_C(4)
#define DISGREG_RBP                     UINT8_C(5)
#define DISGREG_RSI                     UINT8_C(6)
#define DISGREG_RDI                     UINT8_C(7)
#define DISGREG_R8                      UINT8_C(8)
#define DISGREG_R9                      UINT8_C(9)
#define DISGREG_R10                     UINT8_C(10)
#define DISGREG_R11                     UINT8_C(11)
#define DISGREG_R12                     UINT8_C(12)
#define DISGREG_R13                     UINT8_C(13)
#define DISGREG_R14                     UINT8_C(14)
#define DISGREG_R15                     UINT8_C(15)
/** @} */

/** @name 32-bit general register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxGenReg and DISOPPARAM::Index.idxGenReg.
 * @note  Safe to assume same values as the 16-bit and 64-bit general registers.
 * @{
 */
#define DISGREG_EAX                     UINT8_C(0)
#define DISGREG_ECX                     UINT8_C(1)
#define DISGREG_EDX                     UINT8_C(2)
#define DISGREG_EBX                     UINT8_C(3)
#define DISGREG_ESP                     UINT8_C(4)
#define DISGREG_EBP                     UINT8_C(5)
#define DISGREG_ESI                     UINT8_C(6)
#define DISGREG_EDI                     UINT8_C(7)
#define DISGREG_R8D                     UINT8_C(8)
#define DISGREG_R9D                     UINT8_C(9)
#define DISGREG_R10D                    UINT8_C(10)
#define DISGREG_R11D                    UINT8_C(11)
#define DISGREG_R12D                    UINT8_C(12)
#define DISGREG_R13D                    UINT8_C(13)
#define DISGREG_R14D                    UINT8_C(14)
#define DISGREG_R15D                    UINT8_C(15)
/** @} */

/** @name 16-bit general register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxGenReg and DISOPPARAM::Index.idxGenReg.
 * @note  Safe to assume same values as the 32-bit and 64-bit general registers.
 * @{
 */
#define DISGREG_AX                      UINT8_C(0)
#define DISGREG_CX                      UINT8_C(1)
#define DISGREG_DX                      UINT8_C(2)
#define DISGREG_BX                      UINT8_C(3)
#define DISGREG_SP                      UINT8_C(4)
#define DISGREG_BP                      UINT8_C(5)
#define DISGREG_SI                      UINT8_C(6)
#define DISGREG_DI                      UINT8_C(7)
#define DISGREG_R8W                     UINT8_C(8)
#define DISGREG_R9W                     UINT8_C(9)
#define DISGREG_R10W                    UINT8_C(10)
#define DISGREG_R11W                    UINT8_C(11)
#define DISGREG_R12W                    UINT8_C(12)
#define DISGREG_R13W                    UINT8_C(13)
#define DISGREG_R14W                    UINT8_C(14)
#define DISGREG_R15W                    UINT8_C(15)
/** @} */

/** @name 8-bit general register indexes.
 * This mostly (?) matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxGenReg and DISOPPARAM::Index.idxGenReg.
 * @{
 */
#define DISGREG_AL                      UINT8_C(0)
#define DISGREG_CL                      UINT8_C(1)
#define DISGREG_DL                      UINT8_C(2)
#define DISGREG_BL                      UINT8_C(3)
#define DISGREG_AH                      UINT8_C(4)
#define DISGREG_CH                      UINT8_C(5)
#define DISGREG_DH                      UINT8_C(6)
#define DISGREG_BH                      UINT8_C(7)
#define DISGREG_R8B                     UINT8_C(8)
#define DISGREG_R9B                     UINT8_C(9)
#define DISGREG_R10B                    UINT8_C(10)
#define DISGREG_R11B                    UINT8_C(11)
#define DISGREG_R12B                    UINT8_C(12)
#define DISGREG_R13B                    UINT8_C(13)
#define DISGREG_R14B                    UINT8_C(14)
#define DISGREG_R15B                    UINT8_C(15)
#define DISGREG_SPL                     UINT8_C(16)
#define DISGREG_BPL                     UINT8_C(17)
#define DISGREG_SIL                     UINT8_C(18)
#define DISGREG_DIL                     UINT8_C(19)
/** @} */

/** @name Segment registerindexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxSegReg.
 * @{
 */
typedef enum
{
    DISSELREG_ES = 0,
    DISSELREG_CS = 1,
    DISSELREG_SS = 2,
    DISSELREG_DS = 3,
    DISSELREG_FS = 4,
    DISSELREG_GS = 5,
    /** End of the valid register index values. */
    DISSELREG_END,
    /** The usual 32-bit paranoia. */
    DIS_SEGREG_32BIT_HACK = 0x7fffffff
} DISSELREG;
/** @} */

/** @name FPU register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxFpuReg.
 * @{
 */
#define DISFPREG_ST0                    UINT8_C(0)
#define DISFPREG_ST1                    UINT8_C(1)
#define DISFPREG_ST2                    UINT8_C(2)
#define DISFPREG_ST3                    UINT8_C(3)
#define DISFPREG_ST4                    UINT8_C(4)
#define DISFPREG_ST5                    UINT8_C(5)
#define DISFPREG_ST6                    UINT8_C(6)
#define DISFPREG_ST7                    UINT8_C(7)
/** @}  */

/** @name Control register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxCtrlReg.
 * @{
 */
#define DISCREG_CR0                     UINT8_C(0)
#define DISCREG_CR1                     UINT8_C(1)
#define DISCREG_CR2                     UINT8_C(2)
#define DISCREG_CR3                     UINT8_C(3)
#define DISCREG_CR4                     UINT8_C(4)
#define DISCREG_CR8                     UINT8_C(8)
/** @}  */

/** @name Debug register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxDbgReg.
 * @{
 */
#define DISDREG_DR0                     UINT8_C(0)
#define DISDREG_DR1                     UINT8_C(1)
#define DISDREG_DR2                     UINT8_C(2)
#define DISDREG_DR3                     UINT8_C(3)
#define DISDREG_DR4                     UINT8_C(4)
#define DISDREG_DR5                     UINT8_C(5)
#define DISDREG_DR6                     UINT8_C(6)
#define DISDREG_DR7                     UINT8_C(7)
/** @}  */

/** @name MMX register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxMmxReg.
 * @{
 */
#define DISMREG_MMX0                    UINT8_C(0)
#define DISMREG_MMX1                    UINT8_C(1)
#define DISMREG_MMX2                    UINT8_C(2)
#define DISMREG_MMX3                    UINT8_C(3)
#define DISMREG_MMX4                    UINT8_C(4)
#define DISMREG_MMX5                    UINT8_C(5)
#define DISMREG_MMX6                    UINT8_C(6)
#define DISMREG_MMX7                    UINT8_C(7)
/** @}  */

/** @name SSE register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::Base.idxXmmReg.
 * @{
 */
#define DISXREG_XMM0                    UINT8_C(0)
#define DISXREG_XMM1                    UINT8_C(1)
#define DISXREG_XMM2                    UINT8_C(2)
#define DISXREG_XMM3                    UINT8_C(3)
#define DISXREG_XMM4                    UINT8_C(4)
#define DISXREG_XMM5                    UINT8_C(5)
#define DISXREG_XMM6                    UINT8_C(6)
#define DISXREG_XMM7                    UINT8_C(7)
/** @} */


/**
 * Opcode parameter (operand) details.
 */
typedef struct DISOPPARAMX86
{
    /** Disposition.  */
    union
    {
        /** 64-bit displacement, applicable if DISUSE_DISPLACEMENT64 is set in fUse.  */
        int64_t     i64;
        uint64_t    u64;
        /** 32-bit displacement, applicable if DISUSE_DISPLACEMENT32 or
         * DISUSE_RIPDISPLACEMENT32  is set in fUse. */
        int32_t     i32;
        uint32_t    u32;
        /** 16-bit displacement, applicable if DISUSE_DISPLACEMENT16 is set in fUse.  */
        int32_t     i16;
        uint32_t    u16;
        /** 8-bit displacement, applicable if DISUSE_DISPLACEMENT8 is set in fUse.  */
        int32_t     i8;
        uint32_t    u8;
    } uDisp;
    /** The base register from ModR/M or SIB, applicable if DISUSE_BASE is
     * set in fUse. */
    union
    {
        /** General register index (DISGREG_XXX), applicable if DISUSE_REG_GEN8,
         * DISUSE_REG_GEN16, DISUSE_REG_GEN32 or DISUSE_REG_GEN64 is set in fUse. */
        uint8_t     idxGenReg;
        /** FPU stack register index (DISFPREG_XXX), applicable if DISUSE_REG_FP is
         * set in fUse.  1:1 indexes. */
        uint8_t     idxFpuReg;
        /** MMX register index (DISMREG_XXX), applicable if DISUSE_REG_MMX is
         * set in fUse.  1:1 indexes. */
        uint8_t     idxMmxReg;
        /** SSE register index (DISXREG_XXX), applicable if DISUSE_REG_XMM is
         * set in fUse.  1:1 indexes. */
        uint8_t     idxXmmReg;
        /** SSE2 register index (DISYREG_XXX), applicable if DISUSE_REG_YMM is
         * set in fUse.  1:1 indexes. */
        uint8_t     idxYmmReg;
        /** Segment register index (DISSELREG_XXX), applicable if DISUSE_REG_SEG is
         * set in fUse. */
        uint8_t     idxSegReg;
        /** Test register, TR0-TR7, present on early IA32 CPUs, applicable if
         * DISUSE_REG_TEST is set in fUse.  No index defines for these. */
        uint8_t     idxTestReg;
        /** Control register index (DISCREG_XXX), applicable if DISUSE_REG_CR is
         * set in fUse.  1:1 indexes. */
        uint8_t     idxCtrlReg;
        /** Debug register index (DISDREG_XXX), applicable if DISUSE_REG_DBG is
         * set in fUse.  1:1 indexes. */
        uint8_t     idxDbgReg;
    } Base;
    /** The SIB index register meaning, applicable if DISUSE_INDEX is
     * set in fUse. */
    union
    {
        /** General register index (DISGREG_XXX), applicable if DISUSE_REG_GEN8,
         * DISUSE_REG_GEN16, DISUSE_REG_GEN32 or DISUSE_REG_GEN64 is set in fUse. */
        uint8_t     idxGenReg;
        /** XMM register index (DISXREG_XXX), applicable if DISUSE_REG_XMM
         *  is set in fUse. */
        uint8_t     idxXmmReg;
        /** YMM register index (DISXREG_XXX), applicable if DISUSE_REG_YMM
         *  is set in fUse. */
        uint8_t     idxYmmReg;
    } Index;
    /** 2, 4 or 8, if DISUSE_SCALE is set in fUse. */
    uint8_t           uScale;
    /** Parameter size. */
    uint8_t           cb;
    /** Copy of the corresponding DISOPCODE::fParam1 / DISOPCODE::fParam2 /
     * DISOPCODE::fParam3. */
    uint32_t          fParam;
} DISOPPARAMX86;
AssertCompileSize(DISOPPARAMX86, 16);
/** Pointer to opcode parameter. */
typedef DISOPPARAMX86 *PDISOPPARAMX86;
/** Pointer to opcode parameter. */
typedef const DISOPPARAMX86 *PCDISOPPARAMX86;


/** Parser callback.
 * @remark no DECLCALLBACK() here because it's considered to be internal and
 *         there is no point in enforcing CDECL. */
typedef size_t FNDISPARSEX86(size_t offInstr, PCDISOPCODE pOp, PDISSTATE pDis, PDISOPPARAM pParam);
/** Pointer to a disassembler parser function. */
typedef FNDISPARSEX86 *PFNDISPARSEX86;
/** Pointer to a const disassembler parser function pointer. */
typedef PFNDISPARSEX86 const *PCPFNDISPARSEX86;

/**
 * The x86/amd64 specific disassembler state and result.
 */
typedef struct DISSTATEX86
{
    /** SIB fields. */
    union
    {
        /** Bitfield view */
        struct
        {
            uint8_t     Base;
            uint8_t     Index;
            uint8_t     Scale;
        } Bits;
    } SIB;
    /** ModRM fields. */
    union
    {
        /** Bitfield view */
        struct
        {
            uint8_t     Rm;
            uint8_t     Reg;
            uint8_t     Mod;
        } Bits;
    } ModRM;
    /** The addressing mode (DISCPUMODE). */
    uint8_t         uAddrMode;
    /** The operand mode (DISCPUMODE). */
    uint8_t         uOpMode;
    /** Per instruction prefix settings. */
    uint8_t         fPrefix;
    /** REX prefix value (64 bits only). */
    uint8_t         fRexPrefix;
    /** Segment prefix value (DISSELREG). */
    uint8_t         idxSegPrefix;
    /** Last prefix byte (for SSE2 extension tables). */
    uint8_t         bLastPrefix;
    /** Last significant opcode byte of instruction. */
    uint8_t         bOpCode;
    /** The size of the prefix bytes. */
    uint8_t         cbPrefix;
    /** VEX presence flag, destination register and size
     * @todo r=bird: There is no VEX presence flage here, just ~vvvv and L.  */
    uint8_t         bVexDestReg;
    /** VEX.W flag */
    uint8_t         bVexWFlag;
    /** Internal: instruction filter */
    uint32_t        fFilter;
    /** SIB displacment. */
    int32_t         i32SibDisp;
    /** Internal: pointer to disassembly function table */
    PCPFNDISPARSEX86 pfnDisasmFnTable;
#if ARCH_BITS == 32
    uint32_t        uPtrPadding1;
#endif

} DISSTATEX86;
AssertCompileSize(DISSTATEX86, 32);



DISDECL(bool)   DISFormatYasmIsOddEncoding(PDISSTATE pDis);

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_dis_x86_amd64_h */

