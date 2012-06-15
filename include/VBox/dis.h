/** @file
 * DIS - The VirtualBox Disassembler.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_dis_h
#define ___VBox_dis_h

#include <VBox/types.h>
#include <VBox/disopcode.h>
#include <iprt/assert.h>


RT_C_DECLS_BEGIN


/**
 * CPU mode flags (DISCPUSTATE::mode).
 */
typedef enum DISCPUMODE
{
    DISCPUMODE_INVALID = 0,
    DISCPUMODE_16BIT,
    DISCPUMODE_32BIT,
    DISCPUMODE_64BIT,
    /** hack forcing the size of the enum to 32-bits. */
    DISCPUMODE_MAKE_32BIT_HACK = 0x7fffffff
} DISCPUMODE;

/** @name Prefix byte flags (DISCPUSTATE::fPrefix).
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

/** @name 64 bits prefix byte flags (DISCPUSTATE::fRexPrefix).
 * Requires VBox/disopcode.h.
 * @{
 */
#define DISPREFIX_REX_OP_2_FLAGS(a)     (a - OP_PARM_REX_START)
#define DISPREFIX_REX_FLAGS             DISPREFIX_REX_OP_2_FLAGS(OP_PARM_REX)
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

/** @name Operand type.
 * @{
 */
#define DISOPTYPE_INVALID                  RT_BIT_32(0)
#define DISOPTYPE_HARMLESS                 RT_BIT_32(1)
#define DISOPTYPE_CONTROLFLOW              RT_BIT_32(2)
#define DISOPTYPE_POTENTIALLY_DANGEROUS    RT_BIT_32(3)
#define DISOPTYPE_DANGEROUS                RT_BIT_32(4)
#define DISOPTYPE_PORTIO                   RT_BIT_32(5)
#define DISOPTYPE_PRIVILEGED               RT_BIT_32(6)
#define DISOPTYPE_PRIVILEGED_NOTRAP        RT_BIT_32(7)
#define DISOPTYPE_UNCOND_CONTROLFLOW       RT_BIT_32(8)
#define DISOPTYPE_RELATIVE_CONTROLFLOW     RT_BIT_32(9)
#define DISOPTYPE_COND_CONTROLFLOW         RT_BIT_32(10)
#define DISOPTYPE_INTERRUPT                RT_BIT_32(11)
#define DISOPTYPE_ILLEGAL                  RT_BIT_32(12)
#define DISOPTYPE_RRM_DANGEROUS            RT_BIT_32(14)  /**< Some additional dangerous ones when recompiling raw r0. */
#define DISOPTYPE_RRM_DANGEROUS_16         RT_BIT_32(15)  /**< Some additional dangerous ones when recompiling 16-bit raw r0. */
#define DISOPTYPE_RRM_MASK                 (DISOPTYPE_RRM_DANGEROUS | DISOPTYPE_RRM_DANGEROUS_16)
#define DISOPTYPE_INHIBIT_IRQS             RT_BIT_32(16)  /**< Will or can inhibit irqs (sti, pop ss, mov ss) */
#define DISOPTYPE_PORTIO_READ              RT_BIT_32(17)
#define DISOPTYPE_PORTIO_WRITE             RT_BIT_32(18)
#define DISOPTYPE_INVALID_64               RT_BIT_32(19)  /**< Invalid in 64 bits mode */
#define DISOPTYPE_ONLY_64                  RT_BIT_32(20)  /**< Only valid in 64 bits mode */
#define DISOPTYPE_DEFAULT_64_OP_SIZE       RT_BIT_32(21)  /**< Default 64 bits operand size */
#define DISOPTYPE_FORCED_64_OP_SIZE        RT_BIT_32(22)  /**< Forced 64 bits operand size; regardless of prefix bytes */
#define DISOPTYPE_REXB_EXTENDS_OPREG       RT_BIT_32(23)  /**< REX.B extends the register field in the opcode byte */
#define DISOPTYPE_MOD_FIXED_11             RT_BIT_32(24)  /**< modrm.mod is always 11b */
#define DISOPTYPE_FORCED_32_OP_SIZE_X86    RT_BIT_32(25)  /**< Forced 32 bits operand size; regardless of prefix bytes (only in 16 & 32 bits mode!) */
#define DISOPTYPE_ALL                      UINT32_C(0xffffffff)
/** @}  */

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
#define DISUSE_REG_CR                      RT_BIT_64(10)
#define DISUSE_REG_DBG                     RT_BIT_64(11)
#define DISUSE_REG_SEG                     RT_BIT_64(12)
#define DISUSE_REG_TEST                    RT_BIT_64(13)
#define DISUSE_DISPLACEMENT8               RT_BIT_64(14)
#define DISUSE_DISPLACEMENT16              RT_BIT_64(15)
#define DISUSE_DISPLACEMENT32              RT_BIT_64(16)
#define DISUSE_DISPLACEMENT64              RT_BIT_64(17)
#define DISUSE_RIPDISPLACEMENT32           RT_BIT_64(18)
#define DISUSE_IMMEDIATE8                  RT_BIT_64(19)
#define DISUSE_IMMEDIATE8_REL              RT_BIT_64(20)
#define DISUSE_IMMEDIATE16                 RT_BIT_64(21)
#define DISUSE_IMMEDIATE16_REL             RT_BIT_64(22)
#define DISUSE_IMMEDIATE32                 RT_BIT_64(23)
#define DISUSE_IMMEDIATE32_REL             RT_BIT_64(24)
#define DISUSE_IMMEDIATE64                 RT_BIT_64(25)
#define DISUSE_IMMEDIATE64_REL             RT_BIT_64(26)
#define DISUSE_IMMEDIATE_ADDR_0_32         RT_BIT_64(27)
#define DISUSE_IMMEDIATE_ADDR_16_32        RT_BIT_64(28)
#define DISUSE_IMMEDIATE_ADDR_0_16         RT_BIT_64(29)
#define DISUSE_IMMEDIATE_ADDR_16_16        RT_BIT_64(30)
/** DS:ESI */
#define DISUSE_POINTER_DS_BASED            RT_BIT_64(31)
/** ES:EDI */
#define DISUSE_POINTER_ES_BASED            RT_BIT_64(32)
#define DISUSE_IMMEDIATE16_SX8             RT_BIT_64(33)
#define DISUSE_IMMEDIATE32_SX8             RT_BIT_64(34)
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

/** @name 64-bit general register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::base.reg_gen and DISOPPARAM::index.reg_gen.
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
 * DISOPPARAM::base.reg_gen and DISOPPARAM::index.reg_gen.
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
 * DISOPPARAM::base.reg_gen and DISOPPARAM::index.reg_gen.
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
 * DISOPPARAM::base.reg_gen and DISOPPARAM::index.reg_gen.
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
 * DISOPPARAM::base.reg_seg.
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
    /** The usual 32-bit paranoia. */
    DIS_SEGREG_32BIT_HACK = 0x7fffffff
} DISSELREG;
/** @} */

/** @name FPU register indexes.
 * This matches the AMD64 register encoding.  It is found used in
 * DISOPPARAM::base.reg_fp.
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
 * DISOPPARAM::base.reg_ctrl.
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
 * DISOPPARAM::base.reg_dbg.
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
 * DISOPPARAM::base.reg_mmx.
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
 * DISOPPARAM::base.reg_xmm.
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
/** @}  */


/**
 * Operand Parameter.
 */
typedef struct DISOPPARAM
{
    uint64_t        parval;
    /** A combination of DISUSE_XXX. */
    uint64_t        fUse;
    union
    {
        int64_t     i64;
        int32_t     i32;
        int32_t     i16;
        int32_t     i8;
        uint64_t    u64;
        uint32_t    u32;
        uint32_t    u16;
        uint32_t    u8;
    } uDisp;
    int32_t         param;

    union
    {
        /** DISGREG_XXX. */
        uint8_t     reg_gen;
        /** DISFPREG_XXX */
        uint8_t     reg_fp;
        /** DISMREG_XXX. */
        uint8_t     reg_mmx;
        /** DISXREG_XXX. */
        uint8_t     reg_xmm;
        /** DISSELREG_XXX. */
        uint8_t     reg_seg;
        /** TR0-TR7  (no defines for these). */
        uint8_t     reg_test;
        /** DISCREG_XXX */
        uint8_t     reg_ctrl;
        /** DISDREG_XXX */
        uint8_t     reg_dbg;
    } base;
    union
    {
        /** DISGREG_XXX. */
        uint8_t     reg_gen;
    } index;

    /** 2, 4 or 8. */
    uint8_t         scale;
    /** Parameter size. */
    uint8_t         cb;
} DISOPPARAM;
AssertCompileSize(DISOPPARAM, 32);
/** Pointer to opcode parameter. */
typedef DISOPPARAM *PDISOPPARAM;
/** Pointer to opcode parameter. */
typedef const DISOPPARAM *PCDISOPPARAM;


/**
 * Opcode descriptor.
 */
typedef struct DISOPCODE
{
#ifndef DIS_CORE_ONLY
    const char  *pszOpcode;
#endif
    uint8_t     idxParse1;
    uint8_t     idxParse2;
    uint8_t     idxParse3;
    uint8_t     uUnused;
    uint16_t    opcode;
    uint16_t    param1;
    uint16_t    param2;
    uint16_t    param3;
    uint32_t    optype;
} DISOPCODE;
/** Pointer to const opcode. */
typedef const struct DISOPCODE *PCDISOPCODE;


/**
 * Callback for reading opcode bytes.
 *
 * @param   pDisState       Pointer to the CPU state.  The primary user argument
 *                          can be retrived from DISCPUSTATE::pvUser. If
 *                          more is required these can be passed in the
 *                          subsequent slots.
 * @param   pbDst           Pointer to output buffer.
 * @param   uSrcAddr        The address to start reading at.
 * @param   cbToRead        The number of bytes to read.
 */
typedef DECLCALLBACK(int) FNDISREADBYTES(PDISCPUSTATE pDisState, uint8_t *pbDst, RTUINTPTR uSrcAddr, uint32_t cbToRead);
/** Pointer to a opcode byte reader. */
typedef FNDISREADBYTES *PFNDISREADBYTES;

/** Parser callback.
 * @remark no DECLCALLBACK() here because it's considered to be internal (really, I'm too lazy to update all the functions). */
typedef unsigned FNDISPARSE(RTUINTPTR pu8CodeBlock, PCDISOPCODE pOp, PDISOPPARAM pParam, PDISCPUSTATE pCpu);
/** Pointer to a disassembler parser function. */
typedef FNDISPARSE *PFNDISPARSE;
/** Pointer to a const disassembler parser function pointer. */
typedef PFNDISPARSE const *PCPFNDISPARSE;

/**
 * The diassembler state and result.
 */
typedef struct DISCPUSTATE
{
    /* Because of pvUser2, put the less frequently used bits at the top for
       now. (Might be better off in the middle?) */
    DISOPPARAM      param3;
    DISOPPARAM      param2;
    DISOPPARAM      param1;

    /* off: 0x060 (96) */
    /** ModRM fields. */
    union
    {
        /** Bitfield view */
        struct
        {
            unsigned        Rm  : 4;
            unsigned        Reg : 4;
            unsigned        Mod : 2;
        } Bits;
        /** unsigned view */
        unsigned            u;
    } ModRM;
    /** SIB fields. */
    union
    {
        /** Bitfield view */
        struct
        {
            unsigned        Base  : 4;
            unsigned        Index : 4;
            unsigned        Scale : 2;
        } Bits;
        /** unsigned view */
        unsigned            u;
    } SIB;
    int32_t         i32SibDisp;

    /* off: 0x06c (108) */
    /** The CPU mode (DISCPUMODE). */
    uint8_t         mode;
    /** The addressing mode (DISCPUMODE). */
    uint8_t         uAddrMode;
    /** The operand mode (DISCPUMODE). */
    uint8_t         uOpMode;
    /** Per instruction prefix settings. */
    uint8_t         fPrefix;
    /* off: 0x070 (112) */
    /** REX prefix value (64 bits only). */
    uint8_t         fRexPrefix;
    /** Segment prefix value (DISSELREG). */
    uint8_t         idxSegPrefix;
    /** Last prefix byte (for SSE2 extension tables). */
    uint8_t         bLastPrefix;
    /** First opcode byte of instruction. */
    uint8_t         bOpCode;
    /* off: 0x074 (116) */
    /** The size of the prefix bytes. */
    uint8_t         cbPrefix;
    /** The instruction size. */
    uint8_t         cbInstr;
    uint8_t         abUnused[2];
    /* off: 0x078 (120) */
    /** Return code set by a worker function like the opcode bytes readers. */
    int32_t         rc;
    /** Internal: instruction filter */
    uint32_t        fFilter;
    /* off: 0x080 (128) */
    /** Internal: pointer to disassembly function table */
    PCPFNDISPARSE   pfnDisasmFnTable;
#if ARCH_BITS == 32
    uint32_t        uPtrPadding1;
#endif
    /** Pointer to the current instruction. */
    PCDISOPCODE     pCurInstr;
#if ARCH_BITS == 32
    uint32_t        uPtrPadding2;
#endif
    /* off: 0x090 (144) */
    /** The address of the instruction. */
    RTUINTPTR       uInstrAddr;
    /* off: 0x098 (152) */
    /** Optional read function */
    PFNDISREADBYTES pfnReadBytes;
#if ARCH_BITS == 32
    uint32_t        uPadding3;
#endif
    /* off: 0x0a0 (160) */
    /** The instruction bytes. */
    uint8_t         abInstr[16];
    /* off: 0x0b0 (176) */
    /** User data supplied as an argument to the APIs. */
    void           *pvUser;
#if ARCH_BITS == 32
    uint32_t        uPadding4;
#endif
    /** User data that can be set prior to calling the API.
     * @deprecated Please don't use this any more. */
    void           *pvUser2;
#if ARCH_BITS == 32
    uint32_t        uPadding5;
#endif
} DISCPUSTATE;



DISDECL(int) DISInstrToStr(void const *pvInstr, DISCPUMODE enmCpuMode,
                           PDISCPUSTATE pCpu, uint32_t *pcbInstr, char *pszOutput, size_t cbOutput);
DISDECL(int) DISInstrToStrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                     PDISCPUSTATE pCpu, uint32_t *pcbInstr, char *pszOutput, size_t cbOutput);
DISDECL(int) DISInstrToStrEx(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode,
                             PFNDISREADBYTES pfnReadBytes, void *pvUser, uint32_t uFilter,
                             PDISCPUSTATE pCpu, uint32_t *pcbInstr, char *pszOutput, size_t cbOutput);

DISDECL(int) DISInstr(void const *pvInstr, DISCPUMODE enmCpuMode, PDISCPUSTATE pCpu, uint32_t *pcbInstr);
DISDECL(int) DISInstrWithReader(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, PFNDISREADBYTES pfnReadBytes, void *pvUser,
                                PDISCPUSTATE pCpu, uint32_t *pcbInstr);
DISDECL(int) DISInstEx(RTUINTPTR uInstrAddr, DISCPUMODE enmCpuMode, uint32_t uFilter,
                       PFNDISREADBYTES pfnReadBytes, void *pvUser,
                       PDISCPUSTATE pCpu, uint32_t *pcbInstr);

DISDECL(int)        DISGetParamSize(PDISCPUSTATE pCpu, PDISOPPARAM pParam);
DISDECL(DISSELREG)  DISDetectSegReg(PDISCPUSTATE pCpu, PDISOPPARAM pParam);
DISDECL(uint8_t)    DISQuerySegPrefixByte(PDISCPUSTATE pCpu);



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
DISDECL(int) DISQueryParamVal(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, PDISOPPARAM pParam, PDISQPVPARAMVAL pParamVal, DISQPVWHICH parmtype);

DISDECL(int) DISQueryParamRegPtr(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, PDISOPPARAM pParam, void **ppReg, size_t *pcbSize);
DISDECL(int) DISFetchReg8(PCCPUMCTXCORE pCtx, unsigned reg8, uint8_t *pVal);
DISDECL(int) DISFetchReg16(PCCPUMCTXCORE pCtx, unsigned reg16, uint16_t *pVal);
DISDECL(int) DISFetchReg32(PCCPUMCTXCORE pCtx, unsigned reg32, uint32_t *pVal);
DISDECL(int) DISFetchReg64(PCCPUMCTXCORE pCtx, unsigned reg64, uint64_t *pVal);
DISDECL(int) DISFetchRegSeg(PCCPUMCTXCORE pCtx, DISSELREG sel, RTSEL *pVal);
DISDECL(int) DISFetchRegSegEx(PCCPUMCTXCORE pCtx, DISSELREG sel, RTSEL *pVal, PCPUMSELREGHID *ppSelHidReg);
DISDECL(int) DISWriteReg8(PCPUMCTXCORE pRegFrame, unsigned reg8, uint8_t val8);
DISDECL(int) DISWriteReg16(PCPUMCTXCORE pRegFrame, unsigned reg32, uint16_t val16);
DISDECL(int) DISWriteReg32(PCPUMCTXCORE pRegFrame, unsigned reg32, uint32_t val32);
DISDECL(int) DISWriteReg64(PCPUMCTXCORE pRegFrame, unsigned reg64, uint64_t val64);
DISDECL(int) DISWriteRegSeg(PCPUMCTXCORE pCtx, DISSELREG sel, RTSEL val);
DISDECL(int) DISPtrReg8(PCPUMCTXCORE pCtx, unsigned reg8, uint8_t **ppReg);
DISDECL(int) DISPtrReg16(PCPUMCTXCORE pCtx, unsigned reg16, uint16_t **ppReg);
DISDECL(int) DISPtrReg32(PCPUMCTXCORE pCtx, unsigned reg32, uint32_t **ppReg);
DISDECL(int) DISPtrReg64(PCPUMCTXCORE pCtx, unsigned reg64, uint64_t **ppReg);


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
 * @param   pCpu        Pointer to the disassembler CPU state.
 * @param   u32Sel      The selector value. Use DIS_FMT_SEL_IS_REG, DIS_FMT_SEL_GET_VALUE,
 *                      DIS_FMT_SEL_GET_REG to access this.
 * @param   uAddress    The segment address.
 * @param   pszBuf      Where to store the symbol name
 * @param   cchBuf      The size of the buffer.
 * @param   poff        If not a perfect match, then this is where the offset from the return
 *                      symbol to the specified address is returned.
 * @param   pvUser      The user argument.
 */
typedef DECLCALLBACK(int) FNDISGETSYMBOL(PCDISCPUSTATE pCpu, uint32_t u32Sel, RTUINTPTR uAddress, char *pszBuf, size_t cchBuf, RTINTPTR *poff, void *pvUser);
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

DISDECL(size_t) DISFormatYasm(  PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf);
DISDECL(size_t) DISFormatYasmEx(PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf, uint32_t fFlags, PFNDISGETSYMBOL pfnGetSymbol, void *pvUser);
DISDECL(size_t) DISFormatMasm(  PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf);
DISDECL(size_t) DISFormatMasmEx(PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf, uint32_t fFlags, PFNDISGETSYMBOL pfnGetSymbol, void *pvUser);
DISDECL(size_t) DISFormatGas(   PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf);
DISDECL(size_t) DISFormatGasEx( PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf, uint32_t fFlags, PFNDISGETSYMBOL pfnGetSymbol, void *pvUser);

/** @todo DISAnnotate(PCDISCPUSTATE pCpu, char *pszBuf, size_t cchBuf, register reader, memory reader); */

DISDECL(bool)   DISFormatYasmIsOddEncoding(PDISCPUSTATE pCpu);


RT_C_DECLS_END

#endif

