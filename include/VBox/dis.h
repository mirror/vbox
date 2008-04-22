/** @file
 * DIS - The VirtualBox Disassembler.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

#ifndef ___VBox_disasm_h
#define ___VBox_disasm_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/disopcode.h>

#if defined(__L4ENV__)
#include <setjmp.h>
#endif

__BEGIN_DECLS


/** CPU mode flags (DISCPUSTATE::mode).
 * @{
 */
typedef enum
{
    CPUMODE_16BIT = 1,
    CPUMODE_32BIT = 2,
    CPUMODE_64BIT = 3,
    /** hack forcing the size of the enum to 32-bits. */
    CPUMODE_MAKE_32BIT_HACK = 0x7fffffff
} DISCPUMODE;
/** @} */

/** Prefix byte flags
 * @{
 */
#define PREFIX_NONE                     0
/** non-default address size. */
#define PREFIX_ADDRSIZE                 RT_BIT(0)
/** non-default operand size. */
#define PREFIX_OPSIZE                   RT_BIT(1)
/** lock prefix. */
#define PREFIX_LOCK                     RT_BIT(2)
/** segment prefix. */
#define PREFIX_SEG                      RT_BIT(3)
/** rep(e) prefix (not a prefix, but we'll treat is as one). */
#define PREFIX_REP                      RT_BIT(4)
/** rep(e) prefix (not a prefix, but we'll treat is as one). */
#define PREFIX_REPNE                    RT_BIT(5)
/** REX prefix (64 bits) */
#define PREFIX_REX                      RT_BIT(6)
/** @} */

/** 64 bits prefix byte flags
 * @{
 */
#define PREFIX_REX_OP_2_FLAGS(a)        (a - OP_PARM_REX_START)
#define PREFIX_REX_FLAGS                PREFIX_REX_OP_2_FLAGS(OP_PARM_REX)
#define PREFIX_REX_FLAGS_B              PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_B)
#define PREFIX_REX_FLAGS_X              PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_X)
#define PREFIX_REX_FLAGS_XB             PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_XB)
#define PREFIX_REX_FLAGS_R              PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_R)
#define PREFIX_REX_FLAGS_RB             PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_RB)
#define PREFIX_REX_FLAGS_RX             PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_RX)
#define PREFIX_REX_FLAGS_RXB            PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_RXB)
#define PREFIX_REX_FLAGS_W              PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_W)
#define PREFIX_REX_FLAGS_WB             PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WB)
#define PREFIX_REX_FLAGS_WX             PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WX)
#define PREFIX_REX_FLAGS_WXB            PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WXB)
#define PREFIX_REX_FLAGS_WR             PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WR)
#define PREFIX_REX_FLAGS_WRB            PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WRB)
#define PREFIX_REX_FLAGS_WRX            PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WRX)
#define PREFIX_REX_FLAGS_WRXB           PREFIX_REX_OP_2_FLAGS(OP_PARM_REX_WRXB)
/** @} */

/**
 * Operand type.
 */
#define OPTYPE_INVALID                  RT_BIT(0)
#define OPTYPE_HARMLESS                 RT_BIT(1)
#define OPTYPE_CONTROLFLOW              RT_BIT(2)
#define OPTYPE_POTENTIALLY_DANGEROUS    RT_BIT(3)
#define OPTYPE_DANGEROUS                RT_BIT(4)
#define OPTYPE_PORTIO                   RT_BIT(5)
#define OPTYPE_PRIVILEGED               RT_BIT(6)
#define OPTYPE_PRIVILEGED_NOTRAP        RT_BIT(7)
#define OPTYPE_UNCOND_CONTROLFLOW       RT_BIT(8)
#define OPTYPE_RELATIVE_CONTROLFLOW     RT_BIT(9)
#define OPTYPE_COND_CONTROLFLOW         RT_BIT(10)
#define OPTYPE_INTERRUPT                RT_BIT(11)
#define OPTYPE_ILLEGAL                  RT_BIT(12)
#define OPTYPE_RRM_DANGEROUS            RT_BIT(14)  /**< Some additional dangerouse ones when recompiling raw r0. */
#define OPTYPE_RRM_DANGEROUS_16         RT_BIT(15)  /**< Some additional dangerouse ones when recompiling 16-bit raw r0. */
#define OPTYPE_RRM_MASK                 (OPTYPE_RRM_DANGEROUS | OPTYPE_RRM_DANGEROUS_16)
#define OPTYPE_INHIBIT_IRQS             RT_BIT(16)  /**< Will or can inhibit irqs (sti, pop ss, mov ss) */
#define OPTYPE_PORTIO_READ              RT_BIT(17)
#define OPTYPE_PORTIO_WRITE             RT_BIT(18)
#define OPTYPE_INVALID_64               RT_BIT(19)  /**< Invalid in 64 bits mode */
#define OPTYPE_ONLY_64                  RT_BIT(20)  /**< Only valid in 64 bits mode */
#define OPTYPE_DEFAULT_64_OP_SIZE       RT_BIT(21)  /**< Default 64 bits operand size */
#define OPTYPE_FORCED_64_OP_SIZE        RT_BIT(22)  /**< Forced 64 bits operand size; regardless of prefix bytes */
#define OPTYPE_ALL                      (0xffffffff)

/** Parameter usage flags.
 * @{
 */
#define USE_BASE                        RT_BIT_64(0)
#define USE_INDEX                       RT_BIT_64(1)
#define USE_SCALE                       RT_BIT_64(2)
#define USE_REG_GEN8                    RT_BIT_64(3)
#define USE_REG_GEN16                   RT_BIT_64(4)
#define USE_REG_GEN32                   RT_BIT_64(5)
#define USE_REG_GEN64                   RT_BIT_64(6)
#define USE_REG_FP                      RT_BIT_64(7)
#define USE_REG_MMX                     RT_BIT_64(8)
#define USE_REG_XMM                     RT_BIT_64(9)
#define USE_REG_CR                      RT_BIT_64(10)
#define USE_REG_DBG                     RT_BIT_64(11)
#define USE_REG_SEG                     RT_BIT_64(12)
#define USE_REG_TEST                    RT_BIT_64(13)
#define USE_DISPLACEMENT8               RT_BIT_64(14)
#define USE_DISPLACEMENT16              RT_BIT_64(15)
#define USE_DISPLACEMENT32              RT_BIT_64(16)
#define USE_IMMEDIATE8                  RT_BIT_64(17)
#define USE_IMMEDIATE8_REL              RT_BIT_64(18)
#define USE_IMMEDIATE16                 RT_BIT_64(19)
#define USE_IMMEDIATE16_REL             RT_BIT_64(20)
#define USE_IMMEDIATE32                 RT_BIT_64(21)
#define USE_IMMEDIATE32_REL             RT_BIT_64(22)
#define USE_IMMEDIATE64                 RT_BIT_64(23)
#define USE_IMMEDIATE_ADDR_0_32         RT_BIT_64(24)
#define USE_IMMEDIATE_ADDR_16_32        RT_BIT_64(25)
#define USE_IMMEDIATE_ADDR_0_16         RT_BIT_64(26)
#define USE_IMMEDIATE_ADDR_16_16        RT_BIT_64(27)
/** DS:ESI */
#define USE_POINTER_DS_BASED            RT_BIT_64(28)
/** ES:EDI */
#define USE_POINTER_ES_BASED            RT_BIT_64(29)
#define USE_IMMEDIATE16_SX8             RT_BIT_64(30)
#define USE_IMMEDIATE32_SX8             RT_BIT_64(31)

#define USE_IMMEDIATE                   (USE_IMMEDIATE8|USE_IMMEDIATE16|USE_IMMEDIATE32|USE_IMMEDIATE64|USE_IMMEDIATE8_REL|USE_IMMEDIATE16_REL|USE_IMMEDIATE32_REL|USE_IMMEDIATE_ADDR_0_32|USE_IMMEDIATE_ADDR_16_32|USE_IMMEDIATE_ADDR_0_16|USE_IMMEDIATE_ADDR_16_16|USE_IMMEDIATE16_SX8|USE_IMMEDIATE32_SX8)

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

/** index in {"EAX", "ECX", "EDX", "EBX", "ESP", "EBP", "ESI", "EDI"}
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
/** @} */
/** index in {"AX", "CX", "DX", "BX", "SP", "BP", "SI", "DI"}
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
/** @} */

/** index in {"AL", "CL", "DL", "BL", "AH", "CH", "DH", "BH"}
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
/** @} */

/** index in {ES, CS, SS, DS, FS, GS}
 * @{
 */
#define USE_REG_ES                      0
#define USE_REG_CS                      1
#define USE_REG_SS                      2
#define USE_REG_DS                      3
#define USE_REG_FS                      4
#define USE_REG_GS                      5
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
typedef struct _OP_PARAMETER
{
    int             param;
    uint64_t        parval;
    char            szParam[32];

    int32_t         disp8, disp16, disp32;

    uint32_t        size;

    uint64_t        flags;

    union
    {
        uint32_t    reg_gen;
        /** ST(0) - ST(7) */
        uint32_t    reg_fp;
        /** MMX0 - MMX7 */
        uint32_t    reg_mmx;
        /** XMM0 - XMM7 */
        uint32_t    reg_xmm;
        /** {ES, CS, SS, DS, FS, GS} */
        uint32_t    reg_seg;
        /** TR0-TR7 (?) */
        uint32_t    reg_test;
        /** CR0-CR4 */
        uint32_t    reg_ctrl;
        /** DR0-DR7 */
        uint32_t    reg_dbg;
    } base;
    union
    {
        uint32_t    reg_gen;
    } index;

    /** 2, 4 or 8. */
    uint32_t scale;

} OP_PARAMETER;
/** Pointer to opcode parameter. */
typedef OP_PARAMETER *POP_PARAMETER;
/** Pointer to opcode parameter. */
typedef const OP_PARAMETER *PCOP_PARAMETER;


struct _OPCODE;
/** Pointer to opcode. */
typedef struct _OPCODE *POPCODE;
/** Pointer to const opcode. */
typedef const struct _OPCODE *PCOPCODE;

typedef DECLCALLBACK(int) FN_DIS_READBYTES(RTUINTPTR pSrc, uint8_t *pDest, uint32_t size, void *pvUserdata);
typedef FN_DIS_READBYTES *PFN_DIS_READBYTES;

/* forward decl */
struct _DISCPUSTATE;
/** Pointer to the disassembler CPU state. */
typedef struct _DISCPUSTATE *PDISCPUSTATE;

/** Parser callback.
 * @remark no DECLCALLBACK() here because it's considered to be internal (really, I'm too lazy to update all the functions). */
typedef unsigned FNDISPARSE(RTUINTPTR pu8CodeBlock, PCOPCODE pOp, POP_PARAMETER pParam, PDISCPUSTATE pCpu);
typedef FNDISPARSE *PFNDISPARSE;

typedef struct _DISCPUSTATE
{
    /* Global setting */
    DISCPUMODE      mode;

    /* Per instruction prefix settings */
    uint32_t        prefix;
    /** segment prefix value. */
    uint32_t        prefix_seg;
    /** rex prefix value (64 bits only */
    uint32_t        prefix_rex;
    /** addressing mode (16 or 32 bits). (CPUMODE_*) */
    DISCPUMODE      addrmode;
    /** operand mode (16 or 32 bits). (CPUMODE_*) */
    DISCPUMODE      opmode;

    OP_PARAMETER    param1;
    OP_PARAMETER    param2;
    OP_PARAMETER    param3;

    /** ModRM byte. */
    uint32_t        ModRM;
    /** scalar, index, base byte. */
    uint32_t        SIB;

    int32_t         disp;

    /** First opcode byte of instruction. */
    uint8_t         opcode;
    /** Last prefix byte (for SSE2 extension tables) */
    uint8_t         lastprefix;
    RTUINTPTR       opaddr;
    uint32_t        opsize;
#ifndef DIS_CORE_ONLY
    /** Opcode format string for current instruction. */
    const char      *pszOpcode;
#endif

    /** Internal: pointer to disassembly function table */
    PFNDISPARSE    *pfnDisasmFnTable;
    /** Internal: instruction filter */
    uint32_t        uFilter;

    /** Pointer to the current instruction. */
    PCOPCODE        pCurInstr;

    void           *apvUserData[3];

    /** Optional read function */
    PFN_DIS_READBYTES pfnReadBytes;
#ifdef __L4ENV__
    jmp_buf *pJumpBuffer;
#endif /* __L4ENV__ */
} DISCPUSTATE;

/** Opcode. */
#pragma pack(4)
typedef struct _OPCODE
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
} OPCODE;
#pragma pack()


/**
 * Disassembles a code block.
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pvCodeBlock     Pointer to the strunction to disassemble.
 * @param   cbMax           Maximum number of bytes to disassemble.
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 *
 *
 * @todo    Define output callback.
 * @todo    Using signed integers as sizes is a bit odd. There are still
 *          some GCC warnings about mixing signed and unsigend integers.
 * @todo    Need to extend this interface to include a code address so we
 *          can dissassemble GC code. Perhaps a new function is better...
 * @remark  cbMax isn't respected as a boundry. DISInstr() will read beyond cbMax.
 *          This means *pcbSize >= cbMax sometimes.
 */
DISDECL(int) DISBlock(PDISCPUSTATE pCpu, RTUINTPTR pvCodeBlock, unsigned cbMax, unsigned *pSize);

/**
 * Disassembles one instruction
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pu8Instruction  Pointer to the instrunction to disassemble.
 * @param   u32EipOffset    Offset to add to instruction address to get the real virtual address
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstr(PDISCPUSTATE pCpu, RTUINTPTR pu8Instruction, unsigned u32EipOffset, unsigned *pcbSize, char *pszOutput);

/**
 * Disassembles one instruction
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pu8Instruction  Pointer to the strunction to disassemble.
 * @param   u32EipOffset    Offset to add to instruction address to get the real virtual address
 * @param   pcbSize         Where to store the size of the instruction.
 *                          NULL is allowed.
 * @param   pszOutput       Storage for disassembled instruction
 * @param   uFilter         Instruction type filter
 *
 * @todo    Define output callback.
 */
DISDECL(int) DISInstrEx(PDISCPUSTATE pCpu, RTUINTPTR pu8Instruction, uint32_t u32EipOffset, uint32_t *pcbSize,
                         char *pszOutput, unsigned uFilter);

/**
 * Parses one instruction.
 * The result is found in pCpu.
 *
 * @returns VBox error code
 * @param   pCpu            Pointer to cpu structure which has DISCPUSTATE::mode set correctly.
 * @param   InstructionAddr Pointer to the instruction to parse.
 * @param   pcbInstruction  Where to store the size of the instruction.
 *                          NULL is allowed.
 */
DISDECL(int) DISCoreOne(PDISCPUSTATE pCpu, RTUINTPTR InstructionAddr, unsigned *pcbInstruction);

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
                          PDISCPUSTATE pCpu, unsigned *pcbInstruction);

DISDECL(int) DISGetParamSize(PDISCPUSTATE pCpu, POP_PARAMETER pParam);
DISDECL(int) DISDetectSegReg(PDISCPUSTATE pCpu, POP_PARAMETER pParam);
DISDECL(uint8_t) DISQuerySegPrefixByte(PDISCPUSTATE pCpu);

/**
 * Returns the value of the parameter in pParam
 *
 * @returns VBox error code
 * @param   pCtx            Exception structure pointer
 * @param   pCpu            Pointer to cpu structure which have DISCPUSTATE::mode
 *                          set correctly.
 * @param   pParam          Pointer to the parameter to parse
 * @param   pParamVal       Pointer to parameter value (OUT)
 * @param   parmtype        Parameter type
 *
 * @note    Currently doesn't handle FPU/XMM/MMX/3DNow! parameters correctly!!
 *
 */
DISDECL(int) DISQueryParamVal(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, POP_PARAMETER pParam, POP_PARAMVAL pParamVal, PARAM_TYPE parmtype);
DISDECL(int) DISQueryParamRegPtr(PCPUMCTXCORE pCtx, PDISCPUSTATE pCpu, POP_PARAMETER pParam, void **ppReg, size_t *pcbSize);

DISDECL(int) DISFetchReg8(PCPUMCTXCORE pCtx, unsigned reg8, uint8_t *pVal);
DISDECL(int) DISFetchReg16(PCPUMCTXCORE pCtx, unsigned reg16, uint16_t *pVal);
DISDECL(int) DISFetchReg32(PCPUMCTXCORE pCtx, unsigned reg32, uint32_t *pVal);
DISDECL(int) DISFetchReg64(PCPUMCTXCORE pCtx, unsigned reg64, uint64_t *pVal);
DISDECL(int) DISFetchRegSeg(PCPUMCTXCORE pCtx, unsigned sel, RTSEL *pVal);
DISDECL(int) DISFetchRegSegEx(PCPUMCTXCORE pCtx, unsigned sel, RTSEL *pVal, PCPUMSELREGHID *ppSelHidReg);
DISDECL(int) DISWriteReg8(PCPUMCTXCORE pRegFrame, unsigned reg8, uint8_t val8);
DISDECL(int) DISWriteReg16(PCPUMCTXCORE pRegFrame, unsigned reg32, uint16_t val16);
DISDECL(int) DISWriteReg32(PCPUMCTXCORE pRegFrame, unsigned reg32, uint32_t val32);
DISDECL(int) DISWriteReg64(PCPUMCTXCORE pRegFrame, unsigned reg64, uint64_t val64);
DISDECL(int) DISWriteRegSeg(PCPUMCTXCORE pCtx, unsigned sel, RTSEL val);
DISDECL(int) DISPtrReg8(PCPUMCTXCORE pCtx, unsigned reg8, uint8_t **ppReg);
DISDECL(int) DISPtrReg16(PCPUMCTXCORE pCtx, unsigned reg16, uint16_t **ppReg);
DISDECL(int) DISPtrReg32(PCPUMCTXCORE pCtx, unsigned reg32, uint32_t **ppReg);
DISDECL(int) DISPtrReg64(PCPUMCTXCORE pCtx, unsigned reg64, uint64_t **ppReg);

__END_DECLS

#endif

