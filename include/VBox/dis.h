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

#ifndef VBOX_INCLUDED_dis_h
#define VBOX_INCLUDED_dis_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <VBox/dis-x86-amd64.h>
#if defined(VBOX_DIS_WITH_ARMV8)
# include <VBox/dis-armv8.h>
#endif
#include <iprt/assert.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_dis   VBox Disassembler
 * @{ */

/** @name Operand type (DISOPCODEX86::fOpType).
 * @{
 */
#define DISOPTYPE_INVALID                       RT_BIT_32(0)
#define DISOPTYPE_HARMLESS                      RT_BIT_32(1)
#define DISOPTYPE_CONTROLFLOW                   RT_BIT_32(2)
#define DISOPTYPE_POTENTIALLY_DANGEROUS         RT_BIT_32(3)
#define DISOPTYPE_DANGEROUS                     RT_BIT_32(4)
#define DISOPTYPE_PORTIO                        RT_BIT_32(5)
#define DISOPTYPE_PRIVILEGED                    RT_BIT_32(6)
#define DISOPTYPE_PRIVILEGED_NOTRAP             RT_BIT_32(7)
#define DISOPTYPE_UNCOND_CONTROLFLOW            RT_BIT_32(8)
#define DISOPTYPE_RELATIVE_CONTROLFLOW          RT_BIT_32(9)
#define DISOPTYPE_COND_CONTROLFLOW              RT_BIT_32(10)
#define DISOPTYPE_INTERRUPT                     RT_BIT_32(11)
#define DISOPTYPE_ILLEGAL                       RT_BIT_32(12)
#define DISOPTYPE_RRM_DANGEROUS                 RT_BIT_32(14)  /**< Some additional dangerous ones when recompiling raw r0. */
#define DISOPTYPE_RRM_DANGEROUS_16              RT_BIT_32(15)  /**< Some additional dangerous ones when recompiling 16-bit raw r0. */
#define DISOPTYPE_RRM_MASK                      (DISOPTYPE_RRM_DANGEROUS | DISOPTYPE_RRM_DANGEROUS_16)
#define DISOPTYPE_INHIBIT_IRQS                  RT_BIT_32(16)  /**< Will or can inhibit irqs (sti, pop ss, mov ss) */

#define DISOPTYPE_X86_PORTIO_READ               RT_BIT_32(17)
#define DISOPTYPE_X86_PORTIO_WRITE              RT_BIT_32(18)
#define DISOPTYPE_X86_INVALID_64                RT_BIT_32(19)  /**< Invalid in 64 bits mode */
#define DISOPTYPE_X86_ONLY_64                   RT_BIT_32(20)  /**< Only valid in 64 bits mode */
#define DISOPTYPE_X86_DEFAULT_64_OP_SIZE        RT_BIT_32(21)  /**< Default 64 bits operand size */
#define DISOPTYPE_X86_FORCED_64_OP_SIZE         RT_BIT_32(22)  /**< Forced 64 bits operand size; regardless of prefix bytes */
#define DISOPTYPE_X86_REXB_EXTENDS_OPREG        RT_BIT_32(23)  /**< REX.B extends the register field in the opcode byte */
#define DISOPTYPE_X86_MOD_FIXED_11              RT_BIT_32(24)  /**< modrm.mod is always 11b */
#define DISOPTYPE_X86_FORCED_32_OP_SIZE_X86     RT_BIT_32(25)  /**< Forced 32 bits operand size; regardless of prefix bytes (only in 16 & 32 bits mode!) */
#define DISOPTYPE_X86_AVX                       RT_BIT_32(28)  /**< AVX,AVX2,++ instruction. Not implemented yet! */
#define DISOPTYPE_X86_SSE                       RT_BIT_32(29)  /**< SSE,SSE2,SSE3,SSE4,++ instruction. Not implemented yet! */
#define DISOPTYPE_X86_MMX                       RT_BIT_32(30)  /**< MMX,MMXExt,3DNow,++ instruction. Not implemented yet! */
#define DISOPTYPE_X86_FPU                       RT_BIT_32(31)  /**< FPU instruction. Not implemented yet! */
#define DISOPTYPE_ALL                           UINT32_C(0xffffffff)
/** @}  */


/**
 * Opcode descriptor.
 */
#if !defined(DIS_CORE_ONLY) || defined(DOXYGEN_RUNNING)
typedef struct DISOPCODE
{
# define DISOPCODE_FORMAT  0
    /** Mnemonic and operand formatting. */
    const char  *pszOpcode;
    /** Parameter \#1 parser index. */
    uint8_t     idxParse1;
    /** Parameter \#2 parser index. */
    uint8_t     idxParse2;
    /** Parameter \#3 parser index. */
    uint8_t     idxParse3;
    /** Parameter \#4 parser index.  */
    uint8_t     idxParse4;
    /** The opcode identifier. This DIS specific, @see grp_dis_opcodes and
     * VBox/disopcode-x86-amd64.h. */
    uint16_t    uOpcode;
    /** Parameter \#1 info, @see grp_dis_opparam. */
    uint16_t    fParam1;
    /** Parameter \#2 info, @see grp_dis_opparam. */
    uint16_t    fParam2;
    /** Parameter \#3 info, @see grp_dis_opparam. */
    uint16_t    fParam3;
    /** Parameter \#4 info, @see grp_dis_opparam. */
    uint16_t    fParam4;
    /** padding unused */
    uint16_t    uPadding;
    /** Operand type flags, DISOPTYPE_XXX. */
    uint32_t    fOpType;
} DISOPCODE;
#else
# pragma pack(1)
typedef struct DISOPCODE
{
#if 1 /*!defined(RT_ARCH_X86) && !defined(RT_ARCH_AMD64) - probably not worth it for ~4K, costs 2-3% speed. */
    /* 16 bytes (trick is to make sure the bitfields doesn't cross dwords): */
# define DISOPCODE_FORMAT  16
    uint32_t    fOpType;
    uint16_t    uOpcode;
    uint8_t     idxParse1;
    uint8_t     idxParse2;
    uint32_t    fParam1   : 12; /* 1st dword: 12+12+8 = 0x20 (32) */
    uint32_t    fParam2   : 12;
    uint32_t    idxParse3 : 8;
    uint32_t    fParam3   : 12; /* 2nd dword: 12+12+8 = 0x20 (32) */
    uint32_t    fParam4   : 12;
    uint32_t    idxParse4 : 8;
#else /* 15 bytes: */
# define DISOPCODE_FORMAT  15
    uint64_t    uOpcode   : 10; /* 1st qword: 10+12+12+12+6+6+6 = 0x40 (64) */
    uint64_t    idxParse1 : 6;
    uint64_t    idxParse2 : 6;
    uint64_t    idxParse3 : 6;
    uint64_t    fParam1   : 12;
    uint64_t    fParam2   : 12;
    uint64_t    fParam3   : 12;
    uint32_t    fOpType;
    uint16_t    fParam4;
    uint8_t     idxParse4;
#endif
} DISOPCODE;
# pragma pack()
AssertCompile(sizeof(DISOPCODE) == DISOPCODE_FORMAT);
#endif
AssertCompile(DISOPCODE_FORMAT != 15); /* Needs fixing before use as disopcode.h now has more than 1024 opcode values. */


/** @name Parameter usage flags.
 * @{
 */
#define DISUSE_BASE                        RT_BIT_64(0)
#define DISUSE_INDEX                       RT_BIT_64(1)
#define DISUSE_SCALE                       RT_BIT_64(2)
#define DISUSE_REG_GEN8                    RT_BIT_64(3)
#define DISUSE_REG_GEN16                   RT_BIT_64(4)
#define DISUSE_REG_GEN32                   RT_BIT_64(5)
#define DISUSE_REG_GEN64                   RT_BIT_64(6)
#define DISUSE_REG_FP                      RT_BIT_64(7)
#define DISUSE_REG_MMX                     RT_BIT_64(8)
#define DISUSE_REG_XMM                     RT_BIT_64(9)
#define DISUSE_REG_YMM                     RT_BIT_64(10)
#define DISUSE_REG_CR                      RT_BIT_64(11)
#define DISUSE_REG_DBG                     RT_BIT_64(12)
#define DISUSE_REG_SEG                     RT_BIT_64(13)
#define DISUSE_REG_TEST                    RT_BIT_64(14)
#define DISUSE_DISPLACEMENT8               RT_BIT_64(15)
#define DISUSE_DISPLACEMENT16              RT_BIT_64(16)
#define DISUSE_DISPLACEMENT32              RT_BIT_64(17)
#define DISUSE_DISPLACEMENT64              RT_BIT_64(18)
#define DISUSE_RIPDISPLACEMENT32           RT_BIT_64(19)
#define DISUSE_IMMEDIATE8                  RT_BIT_64(20)
#define DISUSE_IMMEDIATE8_REL              RT_BIT_64(21)
#define DISUSE_IMMEDIATE16                 RT_BIT_64(22)
#define DISUSE_IMMEDIATE16_REL             RT_BIT_64(23)
#define DISUSE_IMMEDIATE32                 RT_BIT_64(24)
#define DISUSE_IMMEDIATE32_REL             RT_BIT_64(25)
#define DISUSE_IMMEDIATE64                 RT_BIT_64(26)
#define DISUSE_IMMEDIATE64_REL             RT_BIT_64(27)
#define DISUSE_IMMEDIATE_ADDR_0_32         RT_BIT_64(28)
#define DISUSE_IMMEDIATE_ADDR_16_32        RT_BIT_64(29)
#define DISUSE_IMMEDIATE_ADDR_0_16         RT_BIT_64(30)
#define DISUSE_IMMEDIATE_ADDR_16_16        RT_BIT_64(31)
/** DS:ESI */
#define DISUSE_POINTER_DS_BASED            RT_BIT_64(32)
/** ES:EDI */
#define DISUSE_POINTER_ES_BASED            RT_BIT_64(33)
#define DISUSE_IMMEDIATE16_SX8             RT_BIT_64(34)
#define DISUSE_IMMEDIATE32_SX8             RT_BIT_64(35)
#define DISUSE_IMMEDIATE64_SX8             RT_BIT_64(36)

/** Mask of immediate use flags. */
#define DISUSE_IMMEDIATE                   ( DISUSE_IMMEDIATE8 \
                                           | DISUSE_IMMEDIATE16 \
                                           | DISUSE_IMMEDIATE32 \
                                           | DISUSE_IMMEDIATE64 \
                                           | DISUSE_IMMEDIATE8_REL \
                                           | DISUSE_IMMEDIATE16_REL \
                                           | DISUSE_IMMEDIATE32_REL \
                                           | DISUSE_IMMEDIATE64_REL \
                                           | DISUSE_IMMEDIATE_ADDR_0_32 \
                                           | DISUSE_IMMEDIATE_ADDR_16_32 \
                                           | DISUSE_IMMEDIATE_ADDR_0_16 \
                                           | DISUSE_IMMEDIATE_ADDR_16_16 \
                                           | DISUSE_IMMEDIATE16_SX8 \
                                           | DISUSE_IMMEDIATE32_SX8 \
                                           | DISUSE_IMMEDIATE64_SX8)
/** Check if the use flags indicates an effective address. */
#define DISUSE_IS_EFFECTIVE_ADDR(a_fUseFlags) (!!(  (a_fUseFlags) \
                                                  & (  DISUSE_BASE  \
                                                     | DISUSE_INDEX \
                                                     | DISUSE_DISPLACEMENT32 \
                                                     | DISUSE_DISPLACEMENT64 \
                                                     | DISUSE_DISPLACEMENT16 \
                                                     | DISUSE_DISPLACEMENT8 \
                                                     | DISUSE_RIPDISPLACEMENT32) ))
/** @} */


/**
 * Opcode parameter (operand) details.
 */
typedef struct DISOPPARAM
{
    /** A combination of DISUSE_XXX. */
    uint64_t            fUse;
    /** Immediate value or address, applicable if any of the flags included in
    * DISUSE_IMMEDIATE are set in fUse. */
    uint64_t            uValue;

    /** Architecture specific parameter state. */
    union
    {
        /** x86/amd64 specific state. */
        DISOPPARAMX86   x86;
#if defined(VBOX_DIS_WITH_ARMV8)
        /** ARMv8 specific state. */
        DISOPPARAMARMV8 armv8;
#endif
    } arch;

} DISOPPARAM;
AssertCompileSize(DISOPPARAM, 32);
/** Pointer to opcode parameter. */
typedef const DISOPPARAM *PCDISOPPARAM;


/**
 * Callback for reading instruction bytes.
 *
 * @returns VBox status code, bytes in DISSTATE::u::abInstr and byte count in
 *          DISSTATE::cbCachedInstr.
 * @param   pDis            Pointer to the disassembler state.  The user
 *                          argument can be found in DISSTATE::pvUser if needed.
 * @param   offInstr        The offset relative to the start of the instruction.
 *
 *                          To get the source address, add this to
 *                          DISSTATE::uInstrAddr.
 *
 *                          To calculate the destination buffer address, use it
 *                          as an index into DISSTATE::abInstr.
 *
 * @param   cbMinRead       The minimum number of bytes to read.
 * @param   cbMaxRead       The maximum number of bytes that may be read.
 */
typedef DECLCALLBACKTYPE(int, FNDISREADBYTES,(PDISSTATE pDis, uint8_t offInstr, uint8_t cbMinRead, uint8_t cbMaxRead));
/** Pointer to a opcode byte reader. */
typedef FNDISREADBYTES *PFNDISREADBYTES;


/**
 * The diassembler state and result.
 */
typedef struct DISSTATE
{
    /** The instruction as different views. */
    union
    {
        /** The instruction bytes. */
        uint8_t         abInstr[16];
        /** Single 16-bit view. */
        uint16_t        u16;
        /** Single 32-bit view. */
        uint32_t        u32;
        /** 16-bit view. */
        uint16_t        au16Instr[8];
        /** 32-bit view. */
        uint32_t        au32Instr[4];
        /** 64-bit view. */
        uint64_t        au64Instr[2];
    } u;

    /** Pointer to the current instruction. */
    PCDISOPCODE         pCurInstr;
#if ARCH_BITS == 32
    uint32_t            uPtrPadding2;
#endif

    DISOPPARAM          Param1;
    DISOPPARAM          Param2;
    DISOPPARAM          Param3;
    DISOPPARAM          Param4;

    /** The number of valid bytes in abInstr. */
    uint8_t             cbCachedInstr;
    /** The CPU mode (DISCPUMODE). */
    uint8_t             uCpuMode;
    /** The instruction size. */
    uint8_t             cbInstr;
    /** Unused bytes. */
    uint8_t             abUnused[1];

    /** Return code set by a worker function like the opcode bytes readers. */
    int32_t             rc;
    /** The address of the instruction. */
    RTUINTPTR           uInstrAddr;
    /** Optional read function */
    PFNDISREADBYTES     pfnReadBytes;
#if ARCH_BITS == 32
    uint32_t            uPadding3;
#endif
    /** User data supplied as an argument to the APIs. */
    void                *pvUser;
#if ARCH_BITS == 32
    uint32_t            uPadding4;
#endif

    /** Architecture specific state. */
    union
    {
        /** x86/amd64 specific state. */
        DISSTATEX86     x86;
#if defined(VBOX_DIS_WITH_ARMV8)
        /** ARMv8 specific state. */
        DISSTATEARMV8   armv8;
#endif
    } arch;

} DISSTATE;
AssertCompileMemberAlignment(DISSTATE, arch, 8);
AssertCompileSize(DISSTATE, 0xd8);



DISDECL(int) DISInstrToStr(void const *pvInstr, DISCPUMODE enmCpuMode,
                           PDISSTATE pDis, uint32_t *pcbInstr, char *pszOutput, size_t cbOutput);
DISDECL(int) DISInstrToStrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                     PDISSTATE pDis, uint32_t *pcbInstr, char *pszOutput, size_t cbOutput);
DISDECL(int) DISInstrToStrEx(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode,
                             PFNDISREADBYTES pfnReadBytes, void *pvUser, uint32_t uFilter,
                             PDISSTATE pDis, uint32_t *pcbInstr, char *pszOutput, size_t cbOutput);

DISDECL(int) DISInstr(void const *pvInstr, DISCPUMODE enmCpuMode, PDISSTATE pDis, uint32_t *pcbInstr);
DISDECL(int) DISInstrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                PDISSTATE pDis, uint32_t *pcbInstr);
DISDECL(int) DISInstrEx(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, uint32_t uFilter,
                        PFNDISREADBYTES pfnReadBytes, void *pvUser,
                        PDISSTATE pDis, uint32_t *pcbInstr);
DISDECL(int) DISInstrWithPrefetchedBytes(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, uint32_t fFilter,
                                         void const *pvPrefetched, size_t cbPretched,
                                         PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                         PDISSTATE pDis, uint32_t *pcbInstr);

DISDECL(uint8_t)    DISGetParamSize(PCDISSTATE pDis, PCDISOPPARAM pParam);
#if 0 /* unused */
DISDECL(DISSELREG)  DISDetectSegReg(PCDISSTATE pDis, PCDISOPPARAM pParam);
DISDECL(uint8_t)    DISQuerySegPrefixByte(PCDISSTATE pDis);
#endif

#if 0 /* Needs refactoring if we want to use this again, CPUMCTXCORE is history.  */
/** @name Flags returned by DISQueryParamVal (DISQPVPARAMVAL::flags).
 * @{
 */
#define DISQPV_FLAG_8                   UINT8_C(0x01)
#define DISQPV_FLAG_16                  UINT8_C(0x02)
#define DISQPV_FLAG_32                  UINT8_C(0x04)
#define DISQPV_FLAG_64                  UINT8_C(0x08)
#define DISQPV_FLAG_FARPTR16            UINT8_C(0x10)
#define DISQPV_FLAG_FARPTR32            UINT8_C(0x20)
/** @}  */

/** @name Types returned by DISQueryParamVal (DISQPVPARAMVAL::flags).
 * @{ */
#define DISQPV_TYPE_REGISTER            UINT8_C(1)
#define DISQPV_TYPE_ADDRESS             UINT8_C(2)
#define DISQPV_TYPE_IMMEDIATE           UINT8_C(3)
/** @}  */

typedef struct
{
    union
    {
        uint8_t     val8;
        uint16_t    val16;
        uint32_t    val32;
        uint64_t    val64;

        int8_t      i8;
        int16_t     i16;
        int32_t     i32;
        int64_t     i64;

        struct
        {
            uint16_t sel;
            uint32_t offset;
        } farptr;
    } val;

    uint8_t         type;
    uint8_t         size;
    uint8_t         flags;
} DISQPVPARAMVAL;
/** Pointer to opcode parameter value. */
typedef DISQPVPARAMVAL *PDISQPVPARAMVAL;

/** Indicates which parameter DISQueryParamVal should operate on. */
typedef enum DISQPVWHICH
{
    DISQPVWHICH_DST = 1,
    DISQPVWHICH_SRC,
    DISQPVWHAT_32_BIT_HACK = 0x7fffffff
} DISQPVWHICH;
DISDECL(int) DISQueryParamVal(PCPUMCTXCORE pCtx, PCDISSTATE pDis, PCDISOPPARAM pParam, PDISQPVPARAMVAL pParamVal, DISQPVWHICH parmtype);
DISDECL(int) DISQueryParamRegPtr(PCPUMCTXCORE pCtx, PCDISSTATE pDis, PCDISOPPARAM pParam, void **ppReg, size_t *pcbSize);

DISDECL(int) DISFetchReg8(PCCPUMCTXCORE pCtx, unsigned reg8, uint8_t *pVal);
DISDECL(int) DISFetchReg16(PCCPUMCTXCORE pCtx, unsigned reg16, uint16_t *pVal);
DISDECL(int) DISFetchReg32(PCCPUMCTXCORE pCtx, unsigned reg32, uint32_t *pVal);
DISDECL(int) DISFetchReg64(PCCPUMCTXCORE pCtx, unsigned reg64, uint64_t *pVal);
DISDECL(int) DISFetchRegSeg(PCCPUMCTXCORE pCtx, DISSELREG sel, RTSEL *pVal);
DISDECL(int) DISWriteReg8(PCPUMCTXCORE pRegFrame, unsigned reg8, uint8_t val8);
DISDECL(int) DISWriteReg16(PCPUMCTXCORE pRegFrame, unsigned reg32, uint16_t val16);
DISDECL(int) DISWriteReg32(PCPUMCTXCORE pRegFrame, unsigned reg32, uint32_t val32);
DISDECL(int) DISWriteReg64(PCPUMCTXCORE pRegFrame, unsigned reg64, uint64_t val64);
DISDECL(int) DISWriteRegSeg(PCPUMCTXCORE pCtx, DISSELREG sel, RTSEL val);
DISDECL(int) DISPtrReg8(PCPUMCTXCORE pCtx, unsigned reg8, uint8_t **ppReg);
DISDECL(int) DISPtrReg16(PCPUMCTXCORE pCtx, unsigned reg16, uint16_t **ppReg);
DISDECL(int) DISPtrReg32(PCPUMCTXCORE pCtx, unsigned reg32, uint32_t **ppReg);
DISDECL(int) DISPtrReg64(PCPUMCTXCORE pCtx, unsigned reg64, uint64_t **ppReg);
#endif /* obsolete */


/**
 * Try resolve an address into a symbol name.
 *
 * For use with DISFormatYasmEx(), DISFormatMasmEx() and DISFormatGasEx().
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success, pszBuf contains the full symbol name.
 * @retval  VINF_BUFFER_OVERFLOW if pszBuf is too small the symbol name. The
 *          content of pszBuf is truncated and zero terminated.
 * @retval  VERR_SYMBOL_NOT_FOUND if no matching symbol was found for the address.
 *
 * @param   pDis        Pointer to the disassembler CPU state.
 * @param   u32Sel      The selector value. Use DIS_FMT_SEL_IS_REG, DIS_FMT_SEL_GET_VALUE,
 *                      DIS_FMT_SEL_GET_REG to access this.
 * @param   uAddress    The segment address.
 * @param   pszBuf      Where to store the symbol name
 * @param   cchBuf      The size of the buffer.
 * @param   poff        If not a perfect match, then this is where the offset from the return
 *                      symbol to the specified address is returned.
 * @param   pvUser      The user argument.
 */
typedef DECLCALLBACKTYPE(int, FNDISGETSYMBOL,(PCDISSTATE pDis, uint32_t u32Sel, RTUINTPTR uAddress, char *pszBuf, size_t cchBuf,
                                              RTINTPTR *poff, void *pvUser));
/** Pointer to a FNDISGETSYMBOL(). */
typedef FNDISGETSYMBOL *PFNDISGETSYMBOL;

/**
 * Checks if the FNDISGETSYMBOL argument u32Sel is a register or not.
 */
#define DIS_FMT_SEL_IS_REG(u32Sel)          ( !!((u32Sel) & RT_BIT(31)) )

/**
 * Extracts the selector value from the FNDISGETSYMBOL argument u32Sel.
 * @returns Selector value.
 */
#define DIS_FMT_SEL_GET_VALUE(u32Sel)       ( (RTSEL)(u32Sel) )

/**
 * Extracts the register number from the FNDISGETSYMBOL argument u32Sel.
 * @returns USE_REG_CS, USE_REG_SS, USE_REG_DS, USE_REG_ES, USE_REG_FS or USE_REG_FS.
 */
#define DIS_FMT_SEL_GET_REG(u32Sel)         ( ((u32Sel) >> 16) & 0xf )

/** @internal */
#define DIS_FMT_SEL_FROM_REG(uReg)          ( ((uReg) << 16) | RT_BIT(31) | 0xffff )
/** @internal */
#define DIS_FMT_SEL_FROM_VALUE(Sel)         ( (Sel) & 0xffff )


/** @name Flags for use with DISFormatYasmEx(), DISFormatMasmEx() and DISFormatGasEx().
 * @{
 */
/** Put the address to the right. */
#define DIS_FMT_FLAGS_ADDR_RIGHT            RT_BIT_32(0)
/** Put the address to the left. */
#define DIS_FMT_FLAGS_ADDR_LEFT             RT_BIT_32(1)
/** Put the address in comments.
 * For some assemblers this implies placing it to the right. */
#define DIS_FMT_FLAGS_ADDR_COMMENT          RT_BIT_32(2)
/** Put the instruction bytes to the right of the disassembly. */
#define DIS_FMT_FLAGS_BYTES_RIGHT           RT_BIT_32(3)
/** Put the instruction bytes to the left of the disassembly. */
#define DIS_FMT_FLAGS_BYTES_LEFT            RT_BIT_32(4)
/** Put the instruction bytes in comments.
 * For some assemblers this implies placing the bytes to the right. */
#define DIS_FMT_FLAGS_BYTES_COMMENT         RT_BIT_32(5)
/** Put the bytes in square brackets. */
#define DIS_FMT_FLAGS_BYTES_BRACKETS        RT_BIT_32(6)
/** Put spaces between the bytes. */
#define DIS_FMT_FLAGS_BYTES_SPACED          RT_BIT_32(7)
/** Display the relative +/- offset of branch instructions that uses relative addresses,
 * and put the target address in parenthesis. */
#define DIS_FMT_FLAGS_RELATIVE_BRANCH       RT_BIT_32(8)
/** Strict assembly. The assembly should, when ever possible, make the
 * assembler reproduce the exact same binary. (Refers to the yasm
 * strict keyword.) */
#define DIS_FMT_FLAGS_STRICT                RT_BIT_32(9)
/** Checks if the given flags are a valid combination. */
#define DIS_FMT_FLAGS_IS_VALID(fFlags) \
    (   !((fFlags) & ~UINT32_C(0x000003ff)) \
     && ((fFlags) & (DIS_FMT_FLAGS_ADDR_RIGHT  | DIS_FMT_FLAGS_ADDR_LEFT))  != (DIS_FMT_FLAGS_ADDR_RIGHT  | DIS_FMT_FLAGS_ADDR_LEFT) \
     && (   !((fFlags) & DIS_FMT_FLAGS_ADDR_COMMENT) \
         || (fFlags & (DIS_FMT_FLAGS_ADDR_RIGHT | DIS_FMT_FLAGS_ADDR_LEFT)) ) \
     && ((fFlags) & (DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_BYTES_LEFT)) != (DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_BYTES_LEFT) \
     && (   !((fFlags) & (DIS_FMT_FLAGS_BYTES_COMMENT | DIS_FMT_FLAGS_BYTES_BRACKETS)) \
         || (fFlags & (DIS_FMT_FLAGS_BYTES_RIGHT | DIS_FMT_FLAGS_BYTES_LEFT)) ) \
    )
/** @} */

DISDECL(size_t) DISFormatYasm(  PCDISSTATE pDis, char *pszBuf, size_t cchBuf);
DISDECL(size_t) DISFormatYasmEx(PCDISSTATE pDis, char *pszBuf, size_t cchBuf, uint32_t fFlags, PFNDISGETSYMBOL pfnGetSymbol, void *pvUser);
DISDECL(size_t) DISFormatMasm(  PCDISSTATE pDis, char *pszBuf, size_t cchBuf);
DISDECL(size_t) DISFormatMasmEx(PCDISSTATE pDis, char *pszBuf, size_t cchBuf, uint32_t fFlags, PFNDISGETSYMBOL pfnGetSymbol, void *pvUser);
DISDECL(size_t) DISFormatGas(   PCDISSTATE pDis, char *pszBuf, size_t cchBuf);
DISDECL(size_t) DISFormatGasEx( PCDISSTATE pDis, char *pszBuf, size_t cchBuf, uint32_t fFlags, PFNDISGETSYMBOL pfnGetSymbol, void *pvUser);

/** @todo DISAnnotate(PCDISSTATE pDis, char *pszBuf, size_t cchBuf, register
 *        reader, memory reader); */

/** @} */

RT_C_DECLS_END

#endif /* !VBOX_INCLUDED_dis_h */

