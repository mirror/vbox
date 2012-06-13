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

/** @name Prefix byte flags (DISCPUSTATE::prefix_rex).
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

/** @name 64 bits prefix byte flags (DISCPUSTATE::prefix_rex).
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

/** index in {"RAX", "RCX", "RDX", "RBX", "RSP", "RBP", "RSI", "RDI", "R8", "R9", "R10", "R11", "R12", "R13", "R14", "R15"}
 * @{
 */
#define USE_REG_RAX                     0
#define USE_REG_RCX                     1
#define USE_REG_RDX                     2
#define USE_REG_RBX                     3
#define USE_REG_RSP                     4
#define USE_REG_RBP                     5
#define USE_REG_RSI                     6
#define USE_REG_RDI                     7
#define USE_REG_R8                      8
#define USE_REG_R9                      9
#define USE_REG_R10                     10
#define USE_REG_R11                     11
#define USE_REG_R12                     12
#define USE_REG_R13                     13
#define USE_REG_R14                     14
#define USE_REG_R15                     15
/** @} */

/** index in {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI", "R8D", "R9D", "R10D", "R11D", "R12D", "R13D", "R14D", "R15D"}
 * @{
 */
#define USE_REG_EAX                     0
#define USE_REG_ECX                     1
#define USE_REG_EDX                     2
#define USE_REG_EBX                     3
#define USE_REG_ESP                     4
#define USE_REG_EBP                     5
#define USE_REG_ESI                     6
#define USE_REG_EDI                     7
#define USE_REG_R8D                     8
#define USE_REG_R9D                     9
#define USE_REG_R10D                    10
#define USE_REG_R11D                    11
#define USE_REG_R12D                    12
#define USE_REG_R13D                    13
#define USE_REG_R14D                    14
#define USE_REG_R15D                    15
/** @} */

/** index in {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI", "R8W", "R9W", "R10W", "R11W", "R12W", "R13W", "R14W", "R15W"}
 * @{
 */
#define USE_REG_AX                      0
#define USE_REG_CX                      1
#define USE_REG_DX                      2
#define USE_REG_BX                      3
#define USE_REG_SP                      4
#define USE_REG_BP                      5
#define USE_REG_SI                      6
#define USE_REG_DI                      7
#define USE_REG_R8W                     8
#define USE_REG_R9W                     9
#define USE_REG_R10W                    10
#define USE_REG_R11W                    11
#define USE_REG_R12W                    12
#define USE_REG_R13W                    13
#define USE_REG_R14W                    14
#define USE_REG_R15W                    15
/** @} */

/** index in {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH", "R8B", "R9B", "R10B", "R11B", "R12B", "R13B", "R14B", "R15B", "SPL", "BPL", "SIL", "DIL"}
 * @{
 */
#define USE_REG_AL                      0
#define USE_REG_CL                      1
#define USE_REG_DL                      2
#define USE_REG_BL                      3
#define USE_REG_AH                      4
#define USE_REG_CH                      5
#define USE_REG_DH                      6
#define USE_REG_BH                      7
#define USE_REG_R8B                     8
#define USE_REG_R9B                     9
#define USE_REG_R10B                    10
#define USE_REG_R11B                    11
#define USE_REG_R12B                    12
#define USE_REG_R13B                    13
#define USE_REG_R14B                    14
#define USE_REG_R15B                    15
#define USE_REG_SPL                     16
#define USE_REG_BPL                     17
#define USE_REG_SIL                     18
#define USE_REG_DIL                     19

/** @} */

/** index in {ES, CS, SS, DS, FS, GS}
 * @{
 */
typedef enum
{
    DIS_SELREG_ES = 0,
    DIS_SELREG_CS = 1,
    DIS_SELREG_SS = 2,
    DIS_SELREG_DS = 3,
    DIS_SELREG_FS = 4,
    DIS_SELREG_GS = 5,
    /** The usual 32-bit paranoia. */
    DIS_SEGREG_32BIT_HACK = 0x7fffffff
} DIS_SELREG;
/** @} */

#define USE_REG_FP0                     0
#define USE_REG_FP1                     1
#define USE_REG_FP2                     2
#define USE_REG_FP3                     3
#define USE_REG_FP4                     4
#define USE_REG_FP5                     5
#define USE_REG_FP6                     6
#define USE_REG_FP7                     7

#define USE_REG_CR0                     0
#define USE_REG_CR1                     1
#define USE_REG_CR2                     2
#define USE_REG_CR3                     3
#define USE_REG_CR4                     4
#define USE_REG_CR8                     8

#define USE_REG_DR0                     0
#define USE_REG_DR1                     1
#define USE_REG_DR2                     2
#define USE_REG_DR3                     3
#define USE_REG_DR4                     4
#define USE_REG_DR5                     5
#define USE_REG_DR6                     6
#define USE_REG_DR7                     7

#define USE_REG_MMX0                    0
#define USE_REG_MMX1                    1
#define USE_REG_MMX2                    2
#define USE_REG_MMX3                    3
#define USE_REG_MMX4                    4
#define USE_REG_MMX5                    5
#define USE_REG_MMX6                    6
#define USE_REG_MMX7                    7

#define USE_REG_XMM0                    0
#define USE_REG_XMM1                    1
#define USE_REG_XMM2                    2
#define USE_REG_XMM3                    3
#define USE_REG_XMM4                    4
#define USE_REG_XMM5                    5
#define USE_REG_XMM6                    6
#define USE_REG_XMM7                    7
/** @todo missing XMM8-XMM15 */

/** Used by DISQueryParamVal & EMIQueryParamVal
 * @{
 */
#define PARAM_VAL8             RT_BIT(0)
#define PARAM_VAL16            RT_BIT(1)
#define PARAM_VAL32            RT_BIT(2)
#define PARAM_VAL64            RT_BIT(3)
#define PARAM_VALFARPTR16      RT_BIT(4)
#define PARAM_VALFARPTR32      RT_BIT(5)

#define PARMTYPE_REGISTER      1
#define PARMTYPE_ADDRESS       2
#define PARMTYPE_IMMEDIATE     3

typedef struct
{
    uint32_t        type;
    uint32_t        size;
    uint64_t        flags;

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

} OP_PARAMVAL;
/** Pointer to opcode parameter value. */
typedef OP_PARAMVAL *POP_PARAMVAL;

typedef enum
{
    PARAM_DEST,
    PARAM_SOURCE
} PARAM_TYPE;

/** @} */

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
        uint8_t     reg_gen;
        /** ST(0) - ST(7) */
        uint8_t     reg_fp;
        /** MMX0 - MMX7 */
        uint8_t     reg_mmx;
        /** XMM0 - XMM7 */
        uint8_t     reg_xmm;
        /** {ES, CS, SS, DS, FS, GS} (DIS_SELREG). */
        uint8_t     reg_seg;
        /** TR0-TR7 (?) */
        uint8_t     reg_test;
        /** CR0-CR4 */
        uint8_t     reg_ctrl;
        /** DR0-DR7 */
        uint8_t     reg_dbg;
    } base;
    union
    {
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
typedef const DISOPPARAM *PCOP_PARAMETER;


/** Pointer to const opcode. */
typedef const struct DISOPCODE *PCDISOPCODE;

/**
 * Callback for reading opcode bytes.
 *
 * @param   pDisState       Pointer to the CPU state.  The primary user argument
 *                          can be retrived from DISCPUSTATE::apvUserData[0]. If
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
typedef FNDISPARSE *PFNDISPARSE;
typedef PFNDISPARSE const *PCPFNDISPARSE;

typedef struct DISCPUSTATE
{
    /* Because of apvUserData[1] and apvUserData[2], put the less frequently
       used bits at the top for now.  (Might be better off in the middle?) */
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
    uint8_t         addrmode;
    /** The operand mode (DISCPUMODE). */
    uint8_t         opmode;
    /** Per instruction prefix settings. */
    uint8_t         prefix;  
    /* off: 0x070 (112) */
    /** REX prefix value (64 bits only). */
    uint8_t         prefix_rex;         
    /** Segment prefix value (DIS_SELREG). */
    uint8_t         idxSegPrefix;
    /** Last prefix byte (for SSE2 extension tables). */
    uint8_t         lastprefix;
    /** First opcode byte of instruction. */
    uint8_t         opcode;
    /* off: 0x074 (116) */
    /** The size of the prefix bytes. */
    uint8_t         cbPrefix;           
    /** The instruction size. */
    uint8_t         opsize;
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
    /** User data slots for the read callback.  The first entry is used for the
     *  pvUser argument, the rest are up for grabs.
     * @remarks This must come last so that we can memset everything before this. */
    void           *apvUserData[3];
#if ARCH_BITS == 32
    uint32_t        auPadding4[3];
#endif
} DISCPUSTATE;

/** The storage padding sufficient to hold the largest DISCPUSTATE in all
 * contexts (R3, R0 and RC). Used various places in the VMM internals.   */
#define DISCPUSTATE_PADDING_SIZE    (HC_ARCH_BITS == 64 ? 0x1a0 : 0x180)

/** Opcode. */
#pragma pack(4)
typedef struct DISOPCODE
{
#ifndef DIS_CORE_ONLY
    const char  *pszOpcode;
#endif
    uint8_t     idxParse1;
    uint8_t     idxParse2;
    uint8_t     idxParse3;
    uint16_t    opcode;
    uint16_t    param1;
    uint16_t    param2;
    uint16_t    param3;
    uint32_t    optype;
} DISOPCODE;
#pragma pack()


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
DISDECL(DIS_SELREG) DISDetectSegReg(PDISCPUSTATE pCpu, PDISOPPARAM pParam);
DISDECL(uint8_t)    DISQuerySegPrefixByte(PDISCPUSTATE pCpu);

DISDECL(int) DISQueryParamVal(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, PDISOPPARAM pParam, POP_PARAMVAL pParamVal, PARAM_TYPE parmtype);
DISDECL(int) DISQueryParamRegPtr(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, PDISOPPARAM pParam, void **ppReg, size_t *pcbSize);

DISDECL(int) DISFetchReg8(PCCPUMCTXCORE pCtx, unsigned reg8, uint8_t *pVal);
DISDECL(int) DISFetchReg16(PCCPUMCTXCORE pCtx, unsigned reg16, uint16_t *pVal);
DISDECL(int) DISFetchReg32(PCCPUMCTXCORE pCtx, unsigned reg32, uint32_t *pVal);
DISDECL(int) DISFetchReg64(PCCPUMCTXCORE pCtx, unsigned reg64, uint64_t *pVal);
DISDECL(int) DISFetchRegSeg(PCCPUMCTXCORE pCtx, DIS_SELREG sel, RTSEL *pVal);
DISDECL(int) DISFetchRegSegEx(PCCPUMCTXCORE pCtx, DIS_SELREG sel, RTSEL *pVal, PCPUMSELREGHID *ppSelHidReg);
DISDECL(int) DISWriteReg8(PCPUMCTXCORE pRegFrame, unsigned reg8, uint8_t val8);
DISDECL(int) DISWriteReg16(PCPUMCTXCORE pRegFrame, unsigned reg32, uint16_t val16);
DISDECL(int) DISWriteReg32(PCPUMCTXCORE pRegFrame, unsigned reg32, uint32_t val32);
DISDECL(int) DISWriteReg64(PCPUMCTXCORE pRegFrame, unsigned reg64, uint64_t val64);
DISDECL(int) DISWriteRegSeg(PCPUMCTXCORE pCtx, DIS_SELREG sel, RTSEL val);
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

