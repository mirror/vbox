/* $Id$ */
/** @file
 * BS3Kit - bs3-cpu-instr-3 - MMX, SSE and AVX instructions, C code template.
 */

/*
 * Copyright (C) 2007-2022 Oracle Corporation
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


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>


/*********************************************************************************************************************************
*   Structures and Typedefs                                                                                                      *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
/** Instruction set type and operand width. */
typedef enum
{
    T_INVALID,
    T_MMX,
    T_MMX_SSE,  /**< MMX instruction, but require the SSE CPUID to work. */
    T_AXMMX,
    T_AXMMX_OR_SSE,
    T_SSE,
    T_128BITS = T_SSE,
    T_SSE2,
    T_SSE3,
    T_SSSE3,
    T_SSE4_1,
    T_SSE4_2,
    T_SSE4A,
    T_AVX_128,
    T_AVX2_128,
    T_AVX_256,
    T_256BITS = T_AVX_256,
    T_AVX2_256,
    T_MAX
} INPUT_TYPE_T;

/** Memory or register rm variant. */
enum { RM_REG, RM_MEM };

/**
 * Execution environment configuration.
 */
typedef struct BS3CPUINSTR3_CONFIG_T
{
    uint16_t    fCr0Mp          : 1;
    uint16_t    fCr0Em          : 1;
    uint16_t    fCr0Ts          : 1;
    uint16_t    fCr4OsFxSR      : 1;
    uint16_t    fCr4OsXSave     : 1;
    uint16_t    fXcr0Sse        : 1;
    uint16_t    fXcr0Avx        : 1;
    /** x87 exception pending (IE + something unmasked). */
    uint16_t    fX87XcptPending : 1;
    /** Aligned memory operands. If zero, they will be misaligned and tests w/o memory ops skipped. */
    uint16_t    fAligned        : 1;
    uint16_t    fAlignCheck     : 1;
    uint16_t    fMxCsrMM        : 1; /**< AMD only */
    uint8_t     bXcptMmx;
    uint8_t     bXcptSse;
    uint8_t     bXcptAvx;
} BS3CPUINSTR3_CONFIG_T;
/** Pointer to an execution environment configuration. */
typedef BS3CPUINSTR3_CONFIG_T const BS3_FAR *PCBS3CPUINSTR3_CONFIG_T;

/** State saved by bs3CpuInstr3ConfigReconfigure. */
typedef struct BS3CPUINSTR3_CONFIG_SAVED_T
{
    uint32_t uCr0;
    uint32_t uCr4;
    uint32_t uEfl;
    uint16_t uFcw;
    uint16_t uFsw;
    uint32_t uMxCsr;
} BS3CPUINSTR3_CONFIG_SAVED_T;
typedef BS3CPUINSTR3_CONFIG_SAVED_T BS3_FAR *PBS3CPUINSTR3_CONFIG_SAVED_T;
typedef BS3CPUINSTR3_CONFIG_SAVED_T const BS3_FAR *PCBS3CPUINSTR3_CONFIG_SAVED_T;

#endif


/*********************************************************************************************************************************
*   External Symbols                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN

# define BS3_FNBS3FAR_PROTOTYPES_CMN(a_BaseNm) \
    extern FNBS3FAR RT_CONCAT(a_BaseNm, _c16); \
    extern FNBS3FAR RT_CONCAT(a_BaseNm, _c32); \
    extern FNBS3FAR RT_CONCAT(a_BaseNm, _c64)

/* AND */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pand_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pand_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pand_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pand_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpand_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpand_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpand_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpand_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andps_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andps_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandps_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandps_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandps_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandps_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andpd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andpd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandpd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandpd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandpd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandpd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vandpd_YMM10_YMM8_YMM15_icebp_c64;

/* ANDN */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pandn_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pandn_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pandn_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pandn_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpandn_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpandn_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpandn_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpandn_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andnps_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andnps_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnps_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnps_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnps_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnps_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andnpd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_andnpd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnpd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnpd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnpd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vandnpd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vandnpd_YMM10_YMM8_YMM15_icebp_c64;

/* OR */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_por_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_por_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_por_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_por_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpor_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpor_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpor_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpor_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_orps_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_orps_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorps_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorps_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorps_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorps_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_orpd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_orpd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorpd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorpd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorpd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vorpd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vorpd_YMM10_YMM8_YMM15_icebp_c64;

/* XOR */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pxor_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pxor_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pxor_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pxor_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpxor_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpxor_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpxor_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpxor_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_xorps_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_xorps_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_xorpd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_xorpd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorpd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorpd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorpd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vxorpd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vxorpd_YMM10_YMM8_YMM15_icebp_c64;

/* [V]PCMPGT[BWD] */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtb_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtb_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtb_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtb_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtw_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtw_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtw_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtw_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtd_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtd_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpcmpgtd_YMM10_YMM8_YMM15_icebp_c64;

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtq_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpgtq_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpcmpgtq_YMM10_YMM8_YMM15_icebp_c64;

/* [V]PCMPEQ[BWD] */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqb_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqb_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqb_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqb_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqw_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqw_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqw_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqw_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqd_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqd_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpcmpeqd_YMM10_YMM8_YMM15_icebp_c64;

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqq_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pcmpeqq_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpcmpeqq_YMM10_YMM8_YMM15_icebp_c64;

/* [V]ADD[BWDQ] */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddb_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddb_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddb_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddb_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddb_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddb_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddb_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddb_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddw_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddw_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddw_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddw_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddw_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddw_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddw_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddw_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddd_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddd_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpaddd_YMM10_YMM8_YMM15_icebp_c64;

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddq_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddq_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddq_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_paddq_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddq_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddq_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddq_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpaddq_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpaddq_YMM10_YMM8_YMM15_icebp_c64;

/* [V]SUB[BWDQ] */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubb_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubb_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubb_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubb_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubb_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubb_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubb_YMM7_YMM2_YMM3_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubb_YMM7_YMM2_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubw_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubw_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubw_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubw_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubw_XMM1_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubw_XMM1_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubw_YMM1_YMM1_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubw_YMM1_YMM1_FSxBX_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubd_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubd_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubd_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubd_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubd_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubd_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubd_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubd_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpsubd_YMM10_YMM8_YMM15_icebp_c64;

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubq_MM1_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubq_MM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubq_XMM1_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_psubq_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubq_XMM2_XMM1_XMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubq_XMM2_XMM1_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubq_YMM2_YMM1_YMM0_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpsubq_YMM2_YMM1_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpsubq_YMM10_YMM8_YMM15_icebp_c64;

/* [V]PMOVMSKB */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pmovmskb_EAX_MM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pmovmskb_EAX_qword_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pmovmskb_EAX_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pmovmskb_EAX_dqword_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpmovmskb_EAX_XMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpmovmskb_EAX_dqword_FSxBX_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpmovmskb_EAX_YMM2_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpmovmskb_EAX_qqword_FSxBX_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpmovmskb_RAX_YMM9_icebp_c64;


/* PSHUFW */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufw_MM1_MM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufw_MM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufw_MM1_MM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufw_MM1_FSxBX_01Bh_icebp);

/* [V]PSHUFHW */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufhw_XMM1_XMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufhw_XMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufhw_XMM1_XMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufhw_XMM1_FSxBX_01Bh_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_XMM1_XMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_XMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_XMM1_XMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_XMM1_FSxBX_01Bh_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_YMM1_YMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_YMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_YMM1_YMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufhw_YMM1_FSxBX_01Bh_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpshufhw_YMM12_YMM7_0FFh_icebp_c64;
extern FNBS3FAR             bs3CpuInstr3_vpshufhw_YMM9_YMM12_01Bh_icebp_c64;

/* [V]PSHUFLW */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshuflw_XMM1_XMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshuflw_XMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshuflw_XMM1_XMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshuflw_XMM1_FSxBX_01Bh_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_XMM1_XMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_XMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_XMM1_XMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_XMM1_FSxBX_01Bh_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_YMM1_YMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_YMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_YMM1_YMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshuflw_YMM1_FSxBX_01Bh_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpshuflw_YMM12_YMM7_0FFh_icebp_c64;
extern FNBS3FAR             bs3CpuInstr3_vpshuflw_YMM9_YMM12_01Bh_icebp_c64;

/* [V]PSHUFD */
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufd_XMM1_XMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufd_XMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufd_XMM1_XMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_pshufd_XMM1_FSxBX_01Bh_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_XMM1_XMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_XMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_XMM1_XMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_XMM1_FSxBX_01Bh_icebp);

BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_YMM1_YMM2_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_YMM1_FSxBX_0FFh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_YMM1_YMM2_01Bh_icebp);
BS3_FNBS3FAR_PROTOTYPES_CMN(bs3CpuInstr3_vpshufd_YMM1_FSxBX_01Bh_icebp);
extern FNBS3FAR             bs3CpuInstr3_vpshufd_YMM12_YMM7_0FFh_icebp_c64;
extern FNBS3FAR             bs3CpuInstr3_vpshufd_YMM9_YMM12_01Bh_icebp_c64;

#endif


/*********************************************************************************************************************************
*   Global Variables                                                                                                             *
*********************************************************************************************************************************/
#ifdef BS3_INSTANTIATING_CMN
static bool g_fGlobalInitialized    = false;
static bool g_fAmdMisalignedSse     = false;
static bool g_afTypeSupports[T_MAX] = { false, false, false, false, false, false, false, false, false };

/** Exception type #4 test configurations. */
static const BS3CPUINSTR3_CONFIG_T g_aXcptConfig4[] =
{
/*
 *   X87 SSE SSE SSE     AVX      AVX  AVX  MMX  MMX+SSE   MMX+AVX  AMD/SSE   <-- applies to
 *                                               +AVX      +AMD/SSE
 *   CR0 CR0 CR0 CR4     CR4      XCR0 XCR0 FCW                      MXCSR
 *   MP, EM, TS, OSFXSR, OSXSAVE, SSE, AVX, ES+, fAligned, AC/AM,   MM,   bXcptMmx,    bXcptSse,    bXcptAvx */
    { 0, 0,  0,  1,      1,       1,   1,   0,   1,        0,       0,    X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_DB }, /* #0 */
    { 1, 0,  0,  1,      1,       1,   1,   0,   1,        0,       0,    X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_DB }, /* #1 */
    { 0, 1,  0,  1,      1,       1,   1,   0,   1,        0,       0,    X86_XCPT_UD, X86_XCPT_UD, X86_XCPT_DB }, /* #2 */
    { 0, 0,  1,  1,      1,       1,   1,   0,   1,        0,       0,    X86_XCPT_NM, X86_XCPT_NM, X86_XCPT_NM }, /* #3 */
    { 0, 1,  1,  1,      1,       1,   1,   0,   1,        0,       0,    X86_XCPT_UD, X86_XCPT_UD, X86_XCPT_NM }, /* #4 */
    { 0, 0,  0,  0,      1,       1,   1,   0,   1,        0,       0,    X86_XCPT_DB, X86_XCPT_UD, X86_XCPT_DB }, /* #5 */
    { 0, 0,  0,  1,      0,       1,   1,   0,   1,        0,       0,    X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_UD }, /* #6 */
    { 0, 0,  0,  1,      1,       1,   0,   0,   1,        0,       0,    X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_UD }, /* #7 */
    { 0, 0,  0,  1,      1,       0,   0,   0,   1,        0,       0,    X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_UD }, /* #8 */
    { 0, 0,  0,  1,      1,       1,   1,   1,   1,        0,       0,    X86_XCPT_MF, X86_XCPT_DB, X86_XCPT_DB }, /* #9 - pending x87 exception */
    /* Memory misalignment: */
    { 0, 0,  0,  1,      1,       1,   1,   0,   0,        0,       0,    X86_XCPT_DB, X86_XCPT_GP, X86_XCPT_DB }, /* #10 */
    { 0, 0,  0,  1,      1,       1,   1,   0,   0,        1,       0,    X86_XCPT_AC, X86_XCPT_GP, X86_XCPT_AC }, /* #11 */
    /* AMD only: */
    { 0, 0,  0,  1,      1,       1,   1,   0,   0,        0,       1,    X86_XCPT_DB, X86_XCPT_DB, X86_XCPT_DB }, /* #12 */
    { 0, 0,  0,  1,      1,       1,   1,   0,   0,        1,       1,    X86_XCPT_AC, X86_XCPT_AC, X86_XCPT_AC }, /* #13 */
};
#endif


/*
 * Common code.
 * Common code.
 * Common code.
 */
#ifdef BS3_INSTANTIATING_CMN

/** Initializes global variables. */
static void bs3CpuInstr3InitGlobals(void)
{
    if (!g_fGlobalInitialized)
    {
        if (g_uBs3CpuDetected & BS3CPU_F_CPUID)
        {
            uint32_t fEbx, fEcx, fEdx;
            ASMCpuIdExSlow(1, 0, 0, 0, NULL, NULL, &fEcx, &fEdx);
            g_afTypeSupports[T_MMX]         = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_MMX);
            g_afTypeSupports[T_MMX_SSE]     = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_SSE);
            g_afTypeSupports[T_SSE]         = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_SSE);
            g_afTypeSupports[T_SSE2]        = RT_BOOL(fEdx & X86_CPUID_FEATURE_EDX_SSE2);
            g_afTypeSupports[T_SSE3]        = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSE3);
            g_afTypeSupports[T_SSSE3]       = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSSE3);
            g_afTypeSupports[T_SSE4_1]      = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSE4_1);
            g_afTypeSupports[T_SSE4_2]      = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_SSE4_2);
            g_afTypeSupports[T_AVX_128]     = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_AVX);
            g_afTypeSupports[T_AVX_256]     = RT_BOOL(fEcx & X86_CPUID_FEATURE_ECX_AVX);

            if (ASMCpuId_EAX(0) >= 7)
            {
                ASMCpuIdExSlow(7, 0, 0, 0, NULL, &fEbx, NULL, NULL);
                g_afTypeSupports[T_AVX2_128] = RT_BOOL(fEbx & X86_CPUID_STEXT_FEATURE_EBX_AVX2);
                g_afTypeSupports[T_AVX2_256] = RT_BOOL(fEbx & X86_CPUID_STEXT_FEATURE_EBX_AVX2);
            }

            if (g_uBs3CpuDetected & BS3CPU_F_CPUID_EXT_LEAVES)
            {
                ASMCpuIdExSlow(UINT32_C(0x80000001), 0, 0, 0, NULL, NULL, &fEcx, &fEdx);
                g_afTypeSupports[T_AXMMX]   = RT_BOOL(fEcx & X86_CPUID_AMD_FEATURE_EDX_AXMMX);
                g_afTypeSupports[T_SSE4A]   = RT_BOOL(fEcx & X86_CPUID_AMD_FEATURE_ECX_SSE4A);
                g_fAmdMisalignedSse         = RT_BOOL(fEcx & X86_CPUID_AMD_FEATURE_ECX_MISALNSSE);
            }
            g_afTypeSupports[T_AXMMX_OR_SSE] = g_afTypeSupports[T_AXMMX] || g_afTypeSupports[T_SSE];
        }

        g_fGlobalInitialized = true;
    }
}


/**
 * Reconfigures the execution environment according to @a pConfig.
 *
 * Call bs3CpuInstr3ConfigRestore to undo the changes.
 *
 * @returns true on success, false if the configuration cannot be applied. In
 *          the latter case, no context changes are made.
 * @param   pSavedCfg   Where to save state we modify.
 * @param   pCtx        The register context to modify.
 * @param   pExtCtx     The extended register context to modify.
 * @param   pConfig     The configuration to apply.
 * @param   bMode       The target mode.
 */
static bool bs3CpuInstr3ConfigReconfigure(PBS3CPUINSTR3_CONFIG_SAVED_T pSavedCfg, PBS3REGCTX pCtx, PBS3EXTCTX pExtCtx,
                                          PCBS3CPUINSTR3_CONFIG_T pConfig, uint8_t bMode)
{
    /*
     * Save context bits we may change here
     */
    pSavedCfg->uCr0   = pCtx->cr0.u32;
    pSavedCfg->uCr4   = pCtx->cr4.u32;
    pSavedCfg->uEfl   = pCtx->rflags.u32;
    pSavedCfg->uFcw   = Bs3ExtCtxGetFcw(pExtCtx);
    pSavedCfg->uFsw   = Bs3ExtCtxGetFsw(pExtCtx);
    pSavedCfg->uMxCsr = Bs3ExtCtxGetMxCsr(pExtCtx);

    /*
     * Can we make these changes?
     */
    if (pConfig->fMxCsrMM && !g_fAmdMisalignedSse)
        return false;

    /* Currently we skip pending x87 exceptions in real mode as they cannot be
       caught, given that we preserve the bios int10h. */
    if (pConfig->fX87XcptPending && BS3_MODE_IS_RM_OR_V86(bMode))
        return false;

    /*
     * Modify the test context.
     */
    if (pConfig->fCr0Mp)
        pCtx->cr0.u32 |= X86_CR0_MP;
    else
        pCtx->cr0.u32 &= ~X86_CR0_MP;
    if (pConfig->fCr0Em)
        pCtx->cr0.u32 |= X86_CR0_EM;
    else
        pCtx->cr0.u32 &= ~X86_CR0_EM;
    if (pConfig->fCr0Ts)
        pCtx->cr0.u32 |= X86_CR0_TS;
    else
        pCtx->cr0.u32 &= ~X86_CR0_TS;

    if (pConfig->fCr4OsFxSR)
        pCtx->cr4.u32 |= X86_CR4_OSFXSR;
    else
        pCtx->cr4.u32 &= ~X86_CR4_OSFXSR;
    /** @todo X86_CR4_OSXMMEEXCPT? */
    if (pConfig->fCr4OsXSave)
        pCtx->cr4.u32 |= X86_CR4_OSXSAVE;
    else
        pCtx->cr4.u32 &= ~X86_CR4_OSXSAVE;

    if (pConfig->fXcr0Sse)
        pExtCtx->fXcr0Saved |= XSAVE_C_SSE;
    else
        pExtCtx->fXcr0Saved &= ~XSAVE_C_SSE;
    if (pConfig->fXcr0Avx)
        pExtCtx->fXcr0Saved |= XSAVE_C_YMM;
    else
        pExtCtx->fXcr0Saved &= ~XSAVE_C_YMM;

    if (pConfig->fAlignCheck)
    {
        pCtx->rflags.u32 |= X86_EFL_AC;
        pCtx->cr0.u32    |= X86_CR0_AM;
    }
    else
    {
        pCtx->rflags.u32 &= ~X86_EFL_AC;
        pCtx->cr0.u32    &= ~X86_CR0_AM;
    }

    if (!pConfig->fX87XcptPending)
        Bs3ExtCtxSetFsw(pExtCtx, pSavedCfg->uFsw & ~(X86_FSW_ES | X86_FSW_B));
    else
    {
        Bs3ExtCtxSetFcw(pExtCtx, pSavedCfg->uFcw & ~X86_FCW_ZM);
        Bs3ExtCtxSetFsw(pExtCtx, pSavedCfg->uFsw | X86_FSW_ZE | X86_FSW_ES | X86_FSW_B);
        pCtx->cr0.u32 |= X86_CR0_NE;
    }

    if (pConfig->fMxCsrMM)
        Bs3ExtCtxSetMxCsr(pExtCtx, pSavedCfg->uMxCsr | X86_MXCSR_MM);
    else
        Bs3ExtCtxSetMxCsr(pExtCtx, pSavedCfg->uMxCsr & ~X86_MXCSR_MM);
    return true;
}


/**
 * Undoes changes made by bs3CpuInstr3ConfigReconfigure.
 */
static void bs3CpuInstr3ConfigRestore(PCBS3CPUINSTR3_CONFIG_SAVED_T pSavedCfg, PBS3REGCTX pCtx, PBS3EXTCTX pExtCtx)
{
    pCtx->cr0.u32       = pSavedCfg->uCr0;
    pCtx->cr4.u32       = pSavedCfg->uCr4;
    pCtx->rflags.u32    = pSavedCfg->uEfl;
    pExtCtx->fXcr0Saved = pExtCtx->fXcr0Nominal;
    Bs3ExtCtxSetFcw(pExtCtx, pSavedCfg->uFcw);
    Bs3ExtCtxSetFsw(pExtCtx, pSavedCfg->uFsw);
    Bs3ExtCtxSetMxCsr(pExtCtx, pSavedCfg->uMxCsr);
}


/**
 * Allocates two extended CPU contexts and initializes the first one
 * with random data.
 * @returns First extended context, initialized with randomish data. NULL on
 *          failure (complained).
 * @param   ppExtCtx2   Where to return the 2nd context.
 */
static PBS3EXTCTX bs3CpuInstr3AllocExtCtxs(PBS3EXTCTX BS3_FAR *ppExtCtx2)
{
    /* Allocate extended context structures. */
    uint64_t   fFlags;
    uint16_t   cb       = Bs3ExtCtxGetSize(&fFlags);
    PBS3EXTCTX pExtCtx1 = Bs3MemAlloc(BS3MEMKIND_TILED, cb * 2);
    PBS3EXTCTX pExtCtx2 = (PBS3EXTCTX)((uint8_t BS3_FAR *)pExtCtx1 + cb);
    if (pExtCtx1)
    {
        Bs3ExtCtxInit(pExtCtx1, cb, fFlags);
        /** @todo populate with semi-random stuff. */

        Bs3ExtCtxInit(pExtCtx2, cb, fFlags);
        *ppExtCtx2 = pExtCtx2;
        return pExtCtx1;
    }
    Bs3TestFailedF("Bs3MemAlloc(tiled,%#x)", cb * 2);
    *ppExtCtx2 = NULL;
    return NULL;
}

static void bs3CpuInstr3FreeExtCtxs(PBS3EXTCTX pExtCtx1, PBS3EXTCTX BS3_FAR pExtCtx2)
{
    RT_NOREF_PV(pExtCtx2);
    Bs3MemFree(pExtCtx1, pExtCtx1->cb * 2);
}

/**
 * Sets up SSE and maybe AVX.
 */
static void bs3CpuInstr3SetupSseAndAvx(PBS3REGCTX pCtx, PCBS3EXTCTX pExtCtx)
{
    uint32_t cr0 =  Bs3RegGetCr0();
    cr0 &= ~(X86_CR0_TS | X86_CR0_MP | X86_CR0_EM);
    cr0 |= X86_CR0_NE;
    pCtx->cr0.u32 = cr0;
    Bs3RegSetCr0(cr0);

    if (pExtCtx->enmMethod != BS3EXTCTXMETHOD_ANCIENT)
    {
        uint32_t cr4 = Bs3RegGetCr4();
        if (pExtCtx->enmMethod == BS3EXTCTXMETHOD_XSAVE)
        {
            cr4 |= X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT | X86_CR4_OSXSAVE;
            Bs3RegSetCr4(cr4);
            Bs3RegSetXcr0(pExtCtx->fXcr0Nominal);
        }
        else if (pExtCtx->enmMethod == BS3EXTCTXMETHOD_FXSAVE)
        {
            cr4 |= X86_CR4_OSFXSR | X86_CR4_OSXMMEEXCPT;
            Bs3RegSetCr4(cr4);
        }
        pCtx->cr4.u32 = cr4;
    }
}


/*
 * Test type #1.
 */

typedef struct BS3CPUINSTR3_TEST1_VALUES_T
{
    RTUINT256U      uSrc2;
    RTUINT256U      uSrc1; /**< uDstIn for MMX & SSE */
    RTUINT256U      uDstOut;
} BS3CPUINSTR3_TEST1_VALUES_T;

typedef struct BS3CPUINSTR3_TEST1_T
{
    FPFNBS3FAR      pfnWorker;
    uint8_t         bAvxMisalignXcpt;
    uint8_t         enmRm;
    uint8_t         enmType;
    uint8_t         iRegDst;
    uint8_t         iRegSrc1;
    uint8_t         iRegSrc2;
    uint8_t         cValues;
    BS3CPUINSTR3_TEST1_VALUES_T const BS3_FAR *paValues;
} BS3CPUINSTR3_TEST1_T;

typedef struct BS3CPUINSTR3_TEST1_MODE_T
{
    BS3CPUINSTR3_TEST1_T const BS3_FAR *paTests;
    unsigned                            cTests;
} BS3CPUINSTR3_TEST1_MODE_T;

/** Initializer for a BS3CPUINSTR3_TEST1_MODE_T array (three entries). */
#if ARCH_BITS == 16
# define BS3CPUINSTR3_TEST1_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { NULL, 0 }, { NULL, 0 } }
#elif ARCH_BITS == 32
# define BS3CPUINSTR3_TEST1_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { NULL, 0 } }
#else
# define BS3CPUINSTR3_TEST1_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { a_aTests64, RT_ELEMENTS(a_aTests64) } }
#endif

/** Converts an execution mode (BS3_MODE_XXX) into an index into an array
 *  initialized by BS3CPUINSTR3_TEST1_MODES_INIT. */
#define BS3CPUINSTR3_TEST1_MODES_INDEX(a_bMode) \
    (BS3_MODE_IS_16BIT_CODE(bMode) ? 0 : BS3_MODE_IS_32BIT_CODE(bMode) ? 1 : 2)


/**
 * Test type #1 worker.
 */
static uint8_t bs3CpuInstr3_WorkerTestType1(uint8_t bMode, BS3CPUINSTR3_TEST1_T const BS3_FAR *paTests, unsigned cTests,
                                            PCBS3CPUINSTR3_CONFIG_T paConfigs, unsigned cConfigs)
{
    const char BS3_FAR * const  pszMode = Bs3GetModeName(bMode);
    BS3REGCTX                   Ctx;
    BS3TRAPFRAME                TrapFrame;
    uint8_t                     bRing = BS3_MODE_IS_V86(bMode) ? 3 : 0;
    PBS3EXTCTX                  pExtCtxOut;
    PBS3EXTCTX                  pExtCtx = bs3CpuInstr3AllocExtCtxs(&pExtCtxOut);
    if (!pExtCtx)
        return 0;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /* Ensure that the globals we use here have been initialized. */
    bs3CpuInstr3InitGlobals();

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 1024);
    bs3CpuInstr3SetupSseAndAvx(&Ctx, pExtCtx);
    //Bs3TestPrintf("FTW=%#x mm1/st1=%.16Rhxs\n",  pExtCtx->Ctx.x87.FTW, &pExtCtx->Ctx.x87.aRegs[1]);

    /*
     * Run the tests in all rings since alignment issues may behave
     * differently in ring-3 compared to ring-0.
     */
    for (;;)
    {
        unsigned iCfg;
        for (iCfg = 0; iCfg < cConfigs; iCfg++)
        {
            unsigned                    iTest;
            BS3CPUINSTR3_CONFIG_SAVED_T SavedCfg;
            if (!bs3CpuInstr3ConfigReconfigure(&SavedCfg, &Ctx, pExtCtx, &paConfigs[iCfg], bMode))
                continue; /* unsupported config */

            /*
             * Iterate the tests.
             */
            for (iTest = 0; iTest < cTests; iTest++)
            {
                BS3CPUINSTR3_TEST1_VALUES_T const BS3_FAR *paValues = paTests[iTest].paValues;
                uint8_t const   cbInstr     = ((uint8_t const BS3_FAR *)(uintptr_t)paTests[iTest].pfnWorker)[-1];
                unsigned const  cValues     = paTests[iTest].cValues;
                bool const      fMmxInstr   = paTests[iTest].enmType < T_SSE;
                bool const      fSseInstr   = paTests[iTest].enmType >= T_SSE && paTests[iTest].enmType < T_AVX_128;
                bool const      fAvxInstr   = paTests[iTest].enmType >= T_AVX_128;
                uint8_t const   cbOperand   = paTests[iTest].enmType < T_128BITS ? 64/8
                                            : paTests[iTest].enmType < T_256BITS ? 128/8 : 256/8;
                uint8_t const   cbAlign     = RT_MIN(cbOperand, 16);
                uint8_t         bXcptExpect = !g_afTypeSupports[paTests[iTest].enmType] ? X86_XCPT_UD
                                            : fMmxInstr ? paConfigs[iCfg].bXcptMmx
                                            : fSseInstr ? paConfigs[iCfg].bXcptSse
                                            : BS3_MODE_IS_RM_OR_V86(bMode) ? X86_XCPT_UD : paConfigs[iCfg].bXcptAvx;
                uint16_t        idTestStep  = bRing * 10000 + iCfg * 100 + iTest * 10;
                unsigned        iVal;
                uint8_t         abPadding[sizeof(RTUINT256U) * 2];
                unsigned const  offPadding  = (BS3_FP_OFF(&abPadding[sizeof(RTUINT256U)]) & ~(size_t)(cbAlign - 1))
                                            - BS3_FP_OFF(&abPadding[0]);
                PRTUINT256U     puMemOp     = (PRTUINT256U)&abPadding[offPadding - !paConfigs[iCfg].fAligned];
                BS3_ASSERT((uint8_t BS3_FAR *)puMemOp - &abPadding[0] <= sizeof(RTUINT256U));

                /* If testing unaligned memory accesses, skip register-only tests.  This allows
                   setting bXcptMmx, bXcptSse and bXcptAvx to reflect the misaligned exceptions.  */
                if (!paConfigs[iCfg].fAligned && paTests[iTest].enmRm != RM_MEM)
                    continue;

                /* #AC is only raised in ring-3.: */
                if (bXcptExpect == X86_XCPT_AC)
                {
                    if (bRing != 3)
                        bXcptExpect = X86_XCPT_DB;
                    else if (fAvxInstr)
                        bXcptExpect = paTests[iTest].bAvxMisalignXcpt; /* they generally don't raise #AC */
                }

                Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paTests[iTest].pfnWorker);

                /*
                 * Iterate the test values and do the actual testing.
                 */
                for (iVal = 0; iVal < cValues; iVal++, idTestStep++)
                {
                    uint16_t   cErrors;
                    uint16_t   uSavedFtw = 0xff;
                    RTUINT256U uMemOpExpect;

                    /*
                     * Set up the context and some expectations.
                     */
                    /* dest */
                    if (paTests[iTest].iRegDst == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        Bs3MemSet(puMemOp, sizeof(*puMemOp), 0xcc);
                        if (bXcptExpect == X86_XCPT_DB)
                            uMemOpExpect = paValues[iVal].uDstOut;
                        else
                            uMemOpExpect = *puMemOp;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc2, ~paValues[iVal].uDstOut.QWords.qw0, BS3EXTCTXTOPMM_ZERO);

                    /* source #1 (/ destination for MMX and SSE) */
                    if (paTests[iTest].iRegSrc1 == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        *puMemOp = paValues[iVal].uSrc1;
                        if (paTests[iTest].iRegDst == UINT8_MAX)
                            BS3_ASSERT(fSseInstr);
                        else
                            uMemOpExpect = paValues[iVal].uSrc1;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc1, paValues[iVal].uSrc1.QWords.qw0, BS3EXTCTXTOPMM_ZERO);
                    else if (fSseInstr)
                        Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegSrc1, &paValues[iVal].uSrc1.DQWords.dqw0);
                    else
                        Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegSrc1, &paValues[iVal].uSrc1, 32);

                    /* source #2 */
                    if (paTests[iTest].iRegSrc2 == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        BS3_ASSERT(paTests[iTest].iRegDst != UINT8_MAX && paTests[iTest].iRegSrc1 != UINT8_MAX);
                        *puMemOp = uMemOpExpect = paValues[iVal].uSrc2;
                        uMemOpExpect = paValues[iVal].uSrc2;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc2, paValues[iVal].uSrc2.QWords.qw0, BS3EXTCTXTOPMM_ZERO);
                    else if (fSseInstr)
                        Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegSrc2, &paValues[iVal].uSrc2.DQWords.dqw0);
                    else
                        Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegSrc2, &paValues[iVal].uSrc2, 32);

                    /* Memory pointer. */
                    if (paTests[iTest].enmRm == RM_MEM)
                    {
                        BS3_ASSERT(   paTests[iTest].iRegDst == UINT8_MAX
                                   || paTests[iTest].iRegSrc1 == UINT8_MAX
                                   || paTests[iTest].iRegSrc2 == UINT8_MAX);
                        Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, &Ctx.fs, puMemOp);
                    }

                    /*
                     * Execute.
                     */
                    Bs3ExtCtxRestore(pExtCtx);
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                    Bs3ExtCtxSave(pExtCtxOut);

                    /*
                     * Check the result:
                     */
                    cErrors = Bs3TestSubErrorCount();

                    if (fMmxInstr && bXcptExpect == X86_XCPT_DB)
                    {
                        uSavedFtw = Bs3ExtCtxGetAbridgedFtw(pExtCtx);
                        Bs3ExtCtxSetAbridgedFtw(pExtCtx, 0xff); /* Observed on 10980xe after pxor mm1, mm2. */
                    }
                    if (bXcptExpect == X86_XCPT_DB && paTests[iTest].iRegDst != UINT8_MAX)
                    {
                        if (fMmxInstr)
                            Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegDst, paValues[iVal].uDstOut.QWords.qw0, BS3EXTCTXTOPMM_SET);
                        else if (fSseInstr)
                            Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegDst, &paValues[iVal].uDstOut.DQWords.dqw0);
                        else
                            Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegDst, &paValues[iVal].uDstOut, cbOperand);
                    }
                    Bs3TestCheckExtCtx(pExtCtxOut, pExtCtx, 0 /*fFlags*/, pszMode, idTestStep);

                    if (TrapFrame.bXcpt != bXcptExpect)
                        Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bXcptExpect, TrapFrame.bXcpt);

                    /* Kludge! Looks like EFLAGS.AC is cleared when raising #GP in real mode on the 10980XE. WEIRD! */
                    if (bMode == BS3_MODE_RM && (Ctx.rflags.u32 & X86_EFL_AC))
                    {
                        if (TrapFrame.Ctx.rflags.u32 & X86_EFL_AC)
                            Bs3TestFailedF("Expected EFLAGS.AC to be cleared (bXcpt=%d)", TrapFrame.bXcpt);
                        TrapFrame.Ctx.rflags.u32 |= X86_EFL_AC;
                    }
                    Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &Ctx, bXcptExpect == X86_XCPT_DB ? cbInstr + 1 : 0, 0,
                                         bXcptExpect == X86_XCPT_DB || BS3_MODE_IS_16BIT_SYS(bMode) ? 0 : X86_EFL_RF,
                                         pszMode, idTestStep);

                    if (   paTests[iTest].enmRm == RM_MEM
                        && Bs3MemCmp(puMemOp, &uMemOpExpect, cbOperand) != 0)
                        Bs3TestFailedF("Expected uMemOp %*.Rhxs, got %*.Rhxs", cbOperand, &uMemOpExpect, cbOperand, puMemOp);

                    if (cErrors != Bs3TestSubErrorCount())
                    {
                        if (paConfigs[iCfg].fAligned)
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect);
                        else
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x, puMemOp=%p, EFLAGS=%#RX32, CR0=%#RX32)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect, puMemOp, TrapFrame.Ctx.rflags.u32, TrapFrame.Ctx.cr0);
                        Bs3TestPrintf("\n");
                    }

                    if (uSavedFtw != 0xff)
                        Bs3ExtCtxSetAbridgedFtw(pExtCtx, uSavedFtw);
                }
            }

            bs3CpuInstr3ConfigRestore(&SavedCfg, &Ctx, pExtCtx);
        }

        /*
         * Next ring.
         */
        bRing++;
        if (bRing > 3 || bMode == BS3_MODE_RM)
            break;
        Bs3RegCtxConvertToRingX(&Ctx, bRing);
    }

    /*
     * Cleanup.
     */
    bs3CpuInstr3FreeExtCtxs(pExtCtx, pExtCtxOut);
    return 0;
}


/*
 * PAND, VPAND, ANDPS, VANDPS, ANDPD, VANDPD.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_andps_andpd_pand)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValues[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ^ */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ^ */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x5555666677770000, 0x1111222233334444, 0x1111222233334444, 0x5555666677770000) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ^ */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x0c09d02808403294, 0x385406c840621622, 0x8000290816080282, 0x0050c020030090b9) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pand_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_YMM7_YMM2_YMM3_icebp_c16,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_YMM7_YMM2_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andps_XMM1_XMM2_icebp_c16,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andps_XMM1_FSxBX_icebp_c16,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_XMM1_XMM1_XMM2_icebp_c16,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_XMM1_XMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_YMM1_YMM1_YMM2_icebp_c16,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_YMM1_YMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andpd_XMM1_XMM2_icebp_c16,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andpd_XMM1_FSxBX_icebp_c16,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_XMM2_XMM1_XMM0_icebp_c16,    255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_XMM2_XMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM2_YMM1_YMM0_icebp_c16,    255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM2_YMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pand_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_YMM7_YMM2_YMM3_icebp_c32,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_YMM7_YMM2_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andps_XMM1_XMM2_icebp_c32,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andps_XMM1_FSxBX_icebp_c32,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_XMM1_XMM1_XMM2_icebp_c32,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_XMM1_XMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_YMM1_YMM1_YMM2_icebp_c32,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_YMM1_YMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andpd_XMM1_XMM2_icebp_c32,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andpd_XMM1_FSxBX_icebp_c32,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_XMM2_XMM1_XMM0_icebp_c32,    255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_XMM2_XMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM2_YMM1_YMM0_icebp_c32,    255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM2_YMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pand_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pand_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_YMM7_YMM2_YMM3_icebp_c64,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpand_YMM7_YMM2_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andps_XMM1_XMM2_icebp_c64,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andps_XMM1_FSxBX_icebp_c64,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_XMM1_XMM1_XMM2_icebp_c64,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_XMM1_XMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_YMM1_YMM1_YMM2_icebp_c64,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandps_YMM1_YMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andpd_XMM1_XMM2_icebp_c64,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andpd_XMM1_FSxBX_icebp_c64,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_XMM2_XMM1_XMM0_icebp_c64,    255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_XMM2_XMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM2_YMM1_YMM0_icebp_c64,    255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM2_YMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandpd_YMM10_YMM8_YMM15_icebp_c64,  255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PANDN, VPANDN, ANDNPS, VANDNPS, ANDNPD, VANDNPD.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_andnps_andnpd_pandn)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValues[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ^ */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ^ */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000008888, 0x0000000000000000, 0x0000000000000000, 0x0000000000008888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ^ */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x41002002649c4141, 0x06a01100260929c4, 0x342106a040449920, 0x9c0c205390090602) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pandn_MM1_MM2_icebp_c16,            255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_MM1_FSxBX_icebp_c16,          255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_XMM1_XMM2_icebp_c16,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_XMM1_FSxBX_icebp_c16,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_XMM1_XMM1_XMM2_icebp_c16,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_XMM1_XMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_YMM7_YMM2_YMM3_icebp_c16,    255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_YMM7_YMM2_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andnps_XMM1_XMM2_icebp_c16,         255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andnps_XMM1_FSxBX_icebp_c16,        255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_XMM1_XMM1_XMM2_icebp_c16,   255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_XMM1_XMM1_FSxBX_icebp_c16,  X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_YMM1_YMM1_YMM2_icebp_c16,   255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_YMM1_YMM1_FSxBX_icebp_c16,  X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andnpd_XMM1_XMM2_icebp_c16,         255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andnpd_XMM1_FSxBX_icebp_c16,        255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_XMM2_XMM1_XMM0_icebp_c16,   255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_XMM2_XMM1_FSxBX_icebp_c16,  X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM2_YMM1_YMM0_icebp_c16,   255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM2_YMM1_FSxBX_icebp_c16,  X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pandn_MM1_MM2_icebp_c32,            255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_MM1_FSxBX_icebp_c32,          255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_XMM1_XMM2_icebp_c32,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_XMM1_FSxBX_icebp_c32,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_XMM1_XMM1_XMM2_icebp_c32,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_XMM1_XMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_YMM7_YMM2_YMM3_icebp_c32,    255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_YMM7_YMM2_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andnps_XMM1_XMM2_icebp_c32,         255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andnps_XMM1_FSxBX_icebp_c32,        255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_XMM1_XMM1_XMM2_icebp_c32,   255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_XMM1_XMM1_FSxBX_icebp_c32,  X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_YMM1_YMM1_YMM2_icebp_c32,   255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_YMM1_YMM1_FSxBX_icebp_c32,  X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andnpd_XMM1_XMM2_icebp_c32,         255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andnpd_XMM1_FSxBX_icebp_c32,        255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_XMM2_XMM1_XMM0_icebp_c32,   255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_XMM2_XMM1_FSxBX_icebp_c32,  X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM2_YMM1_YMM0_icebp_c32,   255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM2_YMM1_FSxBX_icebp_c32,  X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pandn_MM1_MM2_icebp_c64,            255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_MM1_FSxBX_icebp_c64,          255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_XMM1_XMM2_icebp_c64,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pandn_XMM1_FSxBX_icebp_c64,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_XMM1_XMM1_XMM2_icebp_c64,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_XMM1_XMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_YMM7_YMM2_YMM3_icebp_c64,    255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpandn_YMM7_YMM2_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andnps_XMM1_XMM2_icebp_c64,         255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andnps_XMM1_FSxBX_icebp_c64,        255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_XMM1_XMM1_XMM2_icebp_c64,   255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_XMM1_XMM1_FSxBX_icebp_c64,  X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_YMM1_YMM1_YMM2_icebp_c64,   255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnps_YMM1_YMM1_FSxBX_icebp_c64,  X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_andnpd_XMM1_XMM2_icebp_c64,         255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_andnpd_XMM1_FSxBX_icebp_c64,        255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_XMM2_XMM1_XMM0_icebp_c64,   255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_XMM2_XMM1_FSxBX_icebp_c64,  X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM2_YMM1_YMM0_icebp_c64,   255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM2_YMM1_FSxBX_icebp_c64,  X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vandnpd_YMM10_YMM8_YMM15_icebp_c64, 255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}



/*
 * POR, VPOR, PORPS, VORPS, PORPD, VPORPD.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_orps_orpd_por)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValues[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ^ */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ^ */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0xddddeeeeffff8888, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff8888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ^ */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x5fddfdae6dff73d5, 0xfffc9fec667b7ff7, 0xbc21effbffddfbe3, 0xdfdfedf3b38d9fff) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_por_MM1_MM2_icebp_c16,              255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_MM1_FSxBX_icebp_c16,            255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_XMM1_XMM2_icebp_c16,            255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_XMM1_FSxBX_icebp_c16,           255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_XMM1_XMM1_XMM2_icebp_c16,      255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_XMM1_XMM1_FSxBX_icebp_c16,     X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_YMM7_YMM2_YMM3_icebp_c16,      255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_YMM7_YMM2_FSxBX_icebp_c16,     X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_orps_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_orps_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_YMM1_YMM1_YMM2_icebp_c16,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_YMM1_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_orpd_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_orpd_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_por_MM1_MM2_icebp_c32,              255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_MM1_FSxBX_icebp_c32,            255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_XMM1_XMM2_icebp_c32,            255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_XMM1_FSxBX_icebp_c32,           255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_XMM1_XMM1_XMM2_icebp_c32,      255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_XMM1_XMM1_FSxBX_icebp_c32,     X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_YMM7_YMM2_YMM3_icebp_c32,      255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_YMM7_YMM2_FSxBX_icebp_c32,     X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_orps_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_orps_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_YMM1_YMM1_YMM2_icebp_c32,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_YMM1_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_orpd_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_orpd_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_por_MM1_MM2_icebp_c64,              255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_MM1_FSxBX_icebp_c64,            255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_XMM1_XMM2_icebp_c64,            255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_por_XMM1_FSxBX_icebp_c64,           255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_XMM1_XMM1_XMM2_icebp_c64,      255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_XMM1_XMM1_FSxBX_icebp_c64,     X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_YMM7_YMM2_YMM3_icebp_c64,      255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpor_YMM7_YMM2_FSxBX_icebp_c64,     X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_orps_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_orps_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_YMM1_YMM1_YMM2_icebp_c64,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorps_YMM1_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_orpd_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_orpd_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vorpd_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PXOR, VPXOR, XORPS, VXORPS, XORPD, VXORPD.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_xorps_xorpd_pxor)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValues[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ^ */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ^ */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x8888888888888888, 0x8888888888888888, 0x8888888888888888, 0x8888888888888888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ^ */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x53d42d8665bf4141, 0xc7a89924261969d5, 0x3c21c6f3e9d5f961, 0xdf8f2dd3b08d0f46) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pxor_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_YMM7_YMM2_YMM3_icebp_c16,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_YMM7_YMM2_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_xorps_XMM1_XMM2_icebp_c16,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorps_XMM1_FSxBX_icebp_c16,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp_c16,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp_c16,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_xorpd_XMM1_XMM2_icebp_c16,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorpd_XMM1_FSxBX_icebp_c16,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_XMM2_XMM1_XMM0_icebp_c16,    255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_XMM2_XMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM2_YMM1_YMM0_icebp_c16,    255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM2_YMM1_FSxBX_icebp_c16,   X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pxor_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_YMM7_YMM2_YMM3_icebp_c32,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_YMM7_YMM2_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_xorps_XMM1_XMM2_icebp_c32,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorps_XMM1_FSxBX_icebp_c32,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp_c32,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp_c32,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_xorpd_XMM1_XMM2_icebp_c32,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorpd_XMM1_FSxBX_icebp_c32,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_XMM2_XMM1_XMM0_icebp_c32,    255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_XMM2_XMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM2_YMM1_YMM0_icebp_c32,    255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM2_YMM1_FSxBX_icebp_c32,   X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pxor_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pxor_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_YMM7_YMM2_YMM3_icebp_c64,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpxor_YMM7_YMM2_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_xorps_XMM1_XMM2_icebp_c64,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorps_XMM1_FSxBX_icebp_c64,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_XMM2_icebp_c64,    255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_XMM1_XMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_YMM2_icebp_c64,    255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorps_YMM1_YMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },

        {  bs3CpuInstr3_xorpd_XMM1_XMM2_icebp_c64,          255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_xorpd_XMM1_FSxBX_icebp_c64,         255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_XMM2_XMM1_XMM0_icebp_c64,    255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_XMM2_XMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM2_YMM1_YMM0_icebp_c64,    255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM2_YMM1_FSxBX_icebp_c64,   X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vxorpd_YMM10_YMM8_YMM15_icebp_c64,  255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}



/*
 * PCMPGTB, VPCMPGTB, PCMPGTW, VPCMPGTW, PCMPGTD, VPCMPGTD, PCMPGTQ, VPCMPGTQ.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_pcmpgtb_pcmpgtw_pcmpgtd_pcmpgtq)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesB[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* < */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* < */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x000000000000ffff, 0x0000000000000000, 0x0000000000000000, 0x000000000000ffff) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* < */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x0000000000ff0000, 0x00ff00ff00ffffff, 0x000000ff0000ffff, 0xff000000ff00ffff) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesW[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* < */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* < */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x000000000000ffff, 0x0000000000000000, 0x0000000000000000, 0x000000000000ffff) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ^ */ RTUINT256_INIT_C(0x1eddddac77733294, 0xf95c8eec40725633, 0x3333e95bbf9962c3, 0x43d3cda0238499fd), /* modified 1st and 3rd value */
            /* = */ RTUINT256_INIT_C(0x00000000ffff0000, 0x000000000000ffff, 0xffff00000000ffff, 0xffff0000ffffffff) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesD[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* < */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* < */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* < */ RTUINT256_INIT_C(0x555dddac09633294, 0xf95c8eec77725633, 0x7770e95bbf9962c3, 0x43d3cda0238499fd), /* modified 1st, 2nd and 3rd value */
            /* = */ RTUINT256_INIT_C(0xffffffff00000000, 0x00000000ffffffff, 0xffffffff00000000, 0xffffffffffffffff) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesQ[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* < */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* < */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* < */ RTUINT256_INIT_C(0x77ddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),  /* modified 1st value */
            /* = */ RTUINT256_INIT_C(0xffffffffffffffff, 0x0000000000000000, 0x0000000000000000, 0xffffffffffffffff) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pcmpgtb_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_YMM3_icebp_c16,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_pcmpgtw_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_YMM2_icebp_c16,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_pcmpgtd_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_pcmpgtq_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE4_2,   1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_pcmpgtq_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE4_2,   1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pcmpgtb_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_YMM3_icebp_c32,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_pcmpgtw_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_YMM2_icebp_c32,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_pcmpgtd_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_pcmpgtq_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE4_2,   1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_pcmpgtq_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE4_2,   1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pcmpgtb_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpgtb_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_YMM3_icebp_c64,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpgtb_YMM7_YMM2_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_pcmpgtw_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpgtw_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_YMM2_icebp_c64,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpgtw_YMM1_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_pcmpgtd_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpgtd_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpgtd_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_pcmpgtq_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE4_2,   1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
        {  bs3CpuInstr3_pcmpgtq_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE4_2,   1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpgtq_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesD), s_aValuesQ },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PCMPEQB, VPCMPEQB, PCMPEQW, VPCMPEQW, PCMPEQD, VPCMPEQD, PCMPEQQ, VPCMPEQQ.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_pcmpeqb_pcmpeqw_pcmpeqd_pcmpeqq)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesB[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ==*/ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ==*/ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000) },
        {           RTUINT256_INIT_C(0x4dddf02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ==*/ RTUINT256_INIT_C(0x1eddddac09dc3294, 0xf95c17ec667256e6, 0xb400e95bbf999bc3, 0x9cd3cda0230999fd), /* modified all to get some matches */
            /* = */ RTUINT256_INIT_C(0x00ff000000ff0000, 0x0000ff00ff0000ff, 0xff0000000000ff00, 0xff00000000ff0000) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesW[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ==*/ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ==*/ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ==*/ RTUINT256_INIT_C(0x1eddf02a6cdc3294, 0x3ef48eec666b5633, 0x88002fa8bf999ba2, 0x9c5ccda0238496bb), /* modified all to get some matches */
            /* = */ RTUINT256_INIT_C(0x0000ffffffff0000, 0xffff0000ffff0000, 0x0000ffff0000ffff, 0xffff00000000ffff) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesD[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ==*/ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ==*/ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ==*/ RTUINT256_INIT_C(0x4d09f02a09633294, 0x3ef417c8666b3fe6, 0x8800e95b564c9ba2, 0x9c5ce073238499fd), /* modified all to get some matches */
            /* = */ RTUINT256_INIT_C(0xffffffff00000000, 0xffffffffffffffff, 0x00000000ffffffff, 0xffffffff00000000) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesQ[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* ==*/ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* ==*/ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0x0000000000000000, 0x0000000000000000, 0x0000000000000000) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* ==*/ RTUINT256_INIT_C(0x1eddddac09633294, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x43d3cda0238499fd), /* modified 2nd and 3rd to get some matches */
            /* = */ RTUINT256_INIT_C(0x0000000000000000, 0xffffffffffffffff, 0xffffffffffffffff, 0x0000000000000000) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pcmpeqb_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_YMM3_icebp_c16,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_pcmpeqw_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_YMM2_icebp_c16,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_pcmpeqd_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_pcmpeqq_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE4_1,   1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_pcmpeqq_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE4_1,   1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pcmpeqb_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_YMM3_icebp_c32,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_pcmpeqw_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_YMM2_icebp_c32,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_pcmpeqd_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_pcmpeqq_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE4_2,   1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_pcmpeqq_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE4_2,   1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pcmpeqb_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_pcmpeqb_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_YMM3_icebp_c64,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpcmpeqb_YMM7_YMM2_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_pcmpeqw_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_pcmpeqw_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_YMM2_icebp_c64,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpcmpeqw_YMM1_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_pcmpeqd_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_pcmpeqd_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpcmpeqd_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_pcmpeqq_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE4_2,   1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_pcmpeqq_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE4_2,   1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpcmpeqq_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PADDB, VPADDB, PADDW, VPADDW, PADDD, VPADDD, PADDQ, VPADDQ.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_paddb_paddw_paddd_paddq)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesB[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x3232545476768888, 0xaaaacccceeee1010, 0xaaaacccceeee1010, 0x3232545476768888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x6be6cdd6753fa569, 0x3750a5b4a6dd9519, 0x3c21180315e5fd65, 0xdf2fad13b68d2fb8) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesW[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x3332555477768888, 0xaaaacccceeee1110, 0xaaaacccceeee1110, 0x3332555477768888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x6be6cdd6763fA669, 0x3850A6B4A6DD9619, 0x3C21190315E5FE65, 0xE02FAE13B68D30B8) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesD[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x3333555477768888, 0xAAAACCCCEEEF1110, 0xAAAACCCCEEEF1110, 0x3333555477768888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x6BE7CDD6763FA669, 0x3850A6B4A6DD9619, 0x3C22190315E5FE65, 0xE030AE13B68E30B8) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesQ[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x3333555577768888, 0xAAAACCCCEEEF1110, 0xAAAACCCCEEEF1110, 0x3333555577768888) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0x6BE7CDD6763FA669, 0x3850A6B4A6DD9619, 0x3C22190415E5FE65, 0xE030AE13B68E30B8) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_paddb_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_YMM7_YMM2_YMM3_icebp_c16,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_YMM7_YMM2_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_paddw_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_YMM1_YMM1_YMM2_icebp_c16,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_YMM1_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_paddd_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_paddq_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_paddb_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_YMM7_YMM2_YMM3_icebp_c32,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_YMM7_YMM2_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_paddw_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_YMM1_YMM1_YMM2_icebp_c32,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_YMM1_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_paddd_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_paddq_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_paddb_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_paddb_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_YMM7_YMM2_YMM3_icebp_c64,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpaddb_YMM7_YMM2_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_paddw_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_paddw_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_YMM1_YMM1_YMM2_icebp_c64,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpaddw_YMM1_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_paddd_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_paddd_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpaddd_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_paddq_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_paddq_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpaddq_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PSUBB, VPSUBB, PSUBW, VPSUBW, PSUBD, VPSUBD, PSUBQ, VPSUBQ.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_psubb_psubw_psubd_psubq)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesB[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x8888888888887878, 0x8888888888888888, 0x8888888888888888, 0x8888888888887878) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0xd1d4ed829d87bfbf, 0xbb687724da07174d, 0xd4dfbab3694dc721, 0xa777ed2d907b0342) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesW[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x8888888888887778, 0x8888888888888888, 0x8888888888888888, 0x8888888888887778) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0xd1d4ed829c87bebf, 0xba687724da07164d, 0xd3dfb9b3694dc721, 0xa777ed2d907b0342) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesD[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x8888888888877778, 0x8888888888888888, 0x8888888888888888, 0x8888888888877778) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0xd1d3ed829c86bebf, 0xba687724da07164d, 0xd3dfb9b3694cc721, 0xa776ed2d907b0342) },
    };

    static BS3CPUINSTR3_TEST1_VALUES_T const s_aValuesQ[] =
    {
        {           RTUINT256_INIT_C(0, 0, 0, 0),
            /* + */ RTUINT256_INIT_C(0, 0, 0, 0),
            /* = */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {           RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* + */ RTUINT256_INIT_C(0xddddeeeeffff0000, 0x9999aaaabbbbcccc, 0x9999aaaabbbbcccc, 0xddddeeeeffff0000),
            /* = */ RTUINT256_INIT_C(0x8888888888877778, 0x8888888888888888, 0x8888888888888888, 0x8888888888877778) },
        {           RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* + */ RTUINT256_INIT_C(0x1eddddac09633294, 0xf95c8eec40725633, 0x8800e95bbf9962c3, 0x43d3cda0238499fd),
            /* = */ RTUINT256_INIT_C(0xd1d3ed819c86bebf, 0xba687723da07164d, 0xd3dfb9b3694cc721, 0xa776ed2c907b0342) },
    };

    static BS3CPUINSTR3_TEST1_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_psubb_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_YMM7_YMM2_YMM3_icebp_c16,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_YMM7_YMM2_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_psubw_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_XMM1_XMM1_XMM2_icebp_c16,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_XMM1_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_YMM1_YMM1_YMM2_icebp_c16,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_YMM1_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_psubd_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_psubq_MM1_MM2_icebp_c16,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_MM1_FSxBX_icebp_c16,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_XMM1_XMM2_icebp_c16,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_XMM1_FSxBX_icebp_c16,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_XMM2_XMM1_XMM0_icebp_c16,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_XMM2_XMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM2_YMM1_YMM0_icebp_c16,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM2_YMM1_FSxBX_icebp_c16,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST1_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_psubb_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_YMM7_YMM2_YMM3_icebp_c32,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_YMM7_YMM2_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_psubw_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_XMM1_XMM1_XMM2_icebp_c32,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_XMM1_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_YMM1_YMM1_YMM2_icebp_c32,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_YMM1_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_psubd_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_psubq_MM1_MM2_icebp_c32,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_MM1_FSxBX_icebp_c32,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_XMM1_XMM2_icebp_c32,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_XMM1_FSxBX_icebp_c32,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_XMM2_XMM1_XMM0_icebp_c32,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_XMM2_XMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM2_YMM1_YMM0_icebp_c32,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM2_YMM1_FSxBX_icebp_c32,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST1_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_psubb_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_psubb_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_YMM7_YMM2_YMM3_icebp_c64,     255,         RM_REG, T_AVX_256,  7, 2,   3, RT_ELEMENTS(s_aValuesB), s_aValuesB },
        {  bs3CpuInstr3_vpsubb_YMM7_YMM2_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  7, 2, 255, RT_ELEMENTS(s_aValuesB), s_aValuesB },

        {  bs3CpuInstr3_psubw_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_psubw_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_XMM1_XMM1_XMM2_icebp_c64,     255,         RM_REG, T_AVX_128,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_XMM1_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_YMM1_YMM1_YMM2_icebp_c64,     255,         RM_REG, T_AVX_256,  1, 1,   2, RT_ELEMENTS(s_aValuesW), s_aValuesW },
        {  bs3CpuInstr3_vpsubw_YMM1_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  1, 1, 255, RT_ELEMENTS(s_aValuesW), s_aValuesW },

        {  bs3CpuInstr3_psubd_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_psubd_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesD), s_aValuesD },
        {  bs3CpuInstr3_vpsubd_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesD), s_aValuesD },

        {  bs3CpuInstr3_psubq_MM1_MM2_icebp_c64,             255,         RM_REG, T_MMX,      1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_MM1_FSxBX_icebp_c64,           255,         RM_MEM, T_MMX,      1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_XMM1_XMM2_icebp_c64,           255,         RM_REG, T_SSE2,     1, 1,   2, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_psubq_XMM1_FSxBX_icebp_c64,          255,         RM_MEM, T_SSE2,     1, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_XMM2_XMM1_XMM0_icebp_c64,     255,         RM_REG, T_AVX_128,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_XMM2_XMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_128,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM2_YMM1_YMM0_icebp_c64,     255,         RM_REG, T_AVX_256,  2, 1,   0, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM2_YMM1_FSxBX_icebp_c64,    X86_XCPT_DB, RM_MEM, T_AVX_256,  2, 1, 255, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
        {  bs3CpuInstr3_vpsubq_YMM10_YMM8_YMM15_icebp_c64,   255,         RM_REG, T_AVX_256, 10, 8,  15, RT_ELEMENTS(s_aValuesQ), s_aValuesQ },
    };
# endif

    static BS3CPUINSTR3_TEST1_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST1_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST1_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType1(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}



/*
 * Test type #2 - GPR <- MM/XMM/YMM, no VVVV.
 */

typedef struct BS3CPUINSTR3_TEST2_VALUES_T
{
    RTUINT256U      uSrc;
    uint64_t        uDstOut;
} BS3CPUINSTR3_TEST2_VALUES_T;

typedef struct BS3CPUINSTR3_TEST2_T
{
    FPFNBS3FAR      pfnWorker;
    uint8_t         bAvxMisalignXcpt;
    uint8_t         enmRm;
    uint8_t         enmType;
    uint8_t         cbDst;
    uint8_t         cBitsDstValMask;
    bool            fInvalidEncoding;
    uint8_t         iRegDst;
    uint8_t         iRegSrc;
    uint8_t         cValues;
    BS3CPUINSTR3_TEST2_VALUES_T const BS3_FAR *paValues;
} BS3CPUINSTR3_TEST2_T;

typedef struct BS3CPUINSTR3_TEST2_MODE_T
{
    BS3CPUINSTR3_TEST2_T const BS3_FAR *paTests;
    unsigned                            cTests;
} BS3CPUINSTR3_TEST2_MODE_T;

/** Initializer for a BS3CPUINSTR3_TEST2_MODE_T array (three entries). */
#if ARCH_BITS == 16
# define BS3CPUINSTR3_TEST2_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { NULL, 0 }, { NULL, 0 } }
#elif ARCH_BITS == 32
# define BS3CPUINSTR3_TEST2_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { NULL, 0 } }
#else
# define BS3CPUINSTR3_TEST2_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { a_aTests64, RT_ELEMENTS(a_aTests64) } }
#endif

/** Converts an execution mode (BS3_MODE_XXX) into an index into an array
 *  initialized by BS3CPUINSTR3_TEST2_MODES_INIT. */
#define BS3CPUINSTR3_TEST2_MODES_INDEX(a_bMode) \
    (BS3_MODE_IS_16BIT_CODE(bMode) ? 0 : BS3_MODE_IS_32BIT_CODE(bMode) ? 1 : 2)


/**
 * Test type #2 worker.
 */
static uint8_t bs3CpuInstr3_WorkerTestType2(uint8_t bMode, BS3CPUINSTR3_TEST2_T const BS3_FAR *paTests, unsigned cTests,
                                            PCBS3CPUINSTR3_CONFIG_T paConfigs, unsigned cConfigs)
{
    const char BS3_FAR * const  pszMode = Bs3GetModeName(bMode);
    BS3REGCTX                   Ctx;
    BS3TRAPFRAME                TrapFrame;
    uint8_t                     bRing = BS3_MODE_IS_V86(bMode) ? 3 : 0;
    PBS3EXTCTX                  pExtCtxOut;
    PBS3EXTCTX                  pExtCtx = bs3CpuInstr3AllocExtCtxs(&pExtCtxOut);
    if (!pExtCtx)
        return 0;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /* Ensure that the globals we use here have been initialized. */
    bs3CpuInstr3InitGlobals();

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 1024);
    bs3CpuInstr3SetupSseAndAvx(&Ctx, pExtCtx);
    //Bs3TestPrintf("FTW=%#x mm1/st1=%.16Rhxs\n",  pExtCtx->Ctx.x87.FTW, &pExtCtx->Ctx.x87.aRegs[1]);

    /*
     * Run the tests in all rings since alignment issues may behave
     * differently in ring-3 compared to ring-0.
     */
    for (;;)
    {
        unsigned iCfg;
        for (iCfg = 0; iCfg < cConfigs; iCfg++)
        {
            unsigned                    iTest;
            BS3CPUINSTR3_CONFIG_SAVED_T SavedCfg;
            if (!bs3CpuInstr3ConfigReconfigure(&SavedCfg, &Ctx, pExtCtx, &paConfigs[iCfg], bMode))
                continue; /* unsupported config */

            /*
             * Iterate the tests.
             */
            for (iTest = 0; iTest < cTests; iTest++)
            {
                BS3CPUINSTR3_TEST2_VALUES_T const BS3_FAR *paValues = paTests[iTest].paValues;
                uint8_t const   cbInstr     = ((uint8_t const BS3_FAR *)(uintptr_t)paTests[iTest].pfnWorker)[-1];
                unsigned const  cValues     = paTests[iTest].cValues;
                bool const      fMmxInstr   = paTests[iTest].enmType < T_SSE;
                bool const      fSseInstr   = paTests[iTest].enmType >= T_SSE && paTests[iTest].enmType < T_AVX_128;
                bool const      fAvxInstr   = paTests[iTest].enmType >= T_AVX_128;
                uint8_t const   cbOperand   = paTests[iTest].enmType < T_128BITS ? 64/8
                                            : paTests[iTest].enmType < T_256BITS ? 128/8 : 256/8;
                uint8_t const   cbAlign     = RT_MIN(cbOperand, 16);
                uint8_t         bXcptExpect =    !g_afTypeSupports[paTests[iTest].enmType]
                                              || paTests[iTest].fInvalidEncoding ? X86_XCPT_UD
                                            : fMmxInstr ? paConfigs[iCfg].bXcptMmx
                                            : fSseInstr ? paConfigs[iCfg].bXcptSse
                                            : BS3_MODE_IS_RM_OR_V86(bMode) ? X86_XCPT_UD : paConfigs[iCfg].bXcptAvx;
                uint64_t const  fDstValMask = paTests[iTest].cBitsDstValMask == 64 ? UINT64_MAX
                                            : RT_BIT_64(paTests[iTest].cBitsDstValMask) - 1;
                uint16_t        idTestStep  = bRing * 10000 + iCfg * 100 + iTest * 10;
                unsigned        iVal;
                uint8_t         abPadding[sizeof(RTUINT256U) * 2];
                unsigned const  offPadding  = (BS3_FP_OFF(&abPadding[sizeof(RTUINT256U)]) & ~(size_t)(cbAlign - 1))
                                            - BS3_FP_OFF(&abPadding[0]);
                PRTUINT256U     puMemOp     = (PRTUINT256U)&abPadding[offPadding - !paConfigs[iCfg].fAligned];
                BS3_ASSERT((uint8_t BS3_FAR *)puMemOp - &abPadding[0] <= sizeof(RTUINT256U));

                /* If testing unaligned memory accesses, skip register-only tests.  This allows
                   setting bXcptMmx, bXcptSse and bXcptAvx to reflect the misaligned exceptions.  */
                if (!paConfigs[iCfg].fAligned && paTests[iTest].enmRm != RM_MEM)
                    continue;

                /* #AC is only raised in ring-3.: */
                if (bXcptExpect == X86_XCPT_AC)
                {
                    if (bRing != 3)
                        bXcptExpect = X86_XCPT_DB;
                    else if (fAvxInstr)
                        bXcptExpect = paTests[iTest].bAvxMisalignXcpt; /* they generally don't raise #AC */
                }

                Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paTests[iTest].pfnWorker);

                /*
                 * Iterate the test values and do the actual testing.
                 */
                for (iVal = 0; iVal < cValues; iVal++, idTestStep++)
                {
                    uint16_t   cErrors;
                    uint16_t   uSavedFtw = 0xff;
                    RTUINT256U uMemOpExpect;

                    /*
                     * Set up the context and some expectations.
                     */
                    /* dest */
                    if (paTests[iTest].iRegDst == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        Bs3MemSet(puMemOp, sizeof(*puMemOp), 0xcc);
                        uMemOpExpect = *puMemOp;
                        if (bXcptExpect == X86_XCPT_DB)
                            switch (paTests[iTest].cbDst)
                            {
                                case 1: uMemOpExpect.au8[0]  = (uint8_t) (paValues[iVal].uDstOut & fDstValMask); break;
                                case 2: uMemOpExpect.au16[0] = (uint16_t)(paValues[iVal].uDstOut & fDstValMask); break;
                                case 4: uMemOpExpect.au32[0] = (uint32_t)(paValues[iVal].uDstOut & fDstValMask); break;
                                case 8: uMemOpExpect.au64[0] =           (paValues[iVal].uDstOut & fDstValMask); break;
                                default: BS3_ASSERT(0);
                            }
                    }

                    /* source */
                    if (paTests[iTest].iRegSrc == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        BS3_ASSERT(paTests[iTest].iRegDst != UINT8_MAX);
                        *puMemOp = uMemOpExpect = paValues[iVal].uSrc;
                        uMemOpExpect = paValues[iVal].uSrc;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc, paValues[iVal].uSrc.QWords.qw0, BS3EXTCTXTOPMM_ZERO);
                    else if (fSseInstr)
                        Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegSrc, &paValues[iVal].uSrc.DQWords.dqw0);
                    else
                        Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegSrc, &paValues[iVal].uSrc, 32);

                    /* Memory pointer. */
                    if (paTests[iTest].enmRm == RM_MEM)
                    {
                        BS3_ASSERT(paTests[iTest].iRegDst == UINT8_MAX || paTests[iTest].iRegSrc == UINT8_MAX);
                        Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, &Ctx.fs, puMemOp);
                    }

                    /*
                     * Execute.
                     */
                    Bs3ExtCtxRestore(pExtCtx);
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                    Bs3ExtCtxSave(pExtCtxOut);

                    /*
                     * Check the result:
                     */
                    cErrors = Bs3TestSubErrorCount();

                    if (fMmxInstr && bXcptExpect == X86_XCPT_DB)
                    {
                        uSavedFtw = Bs3ExtCtxGetAbridgedFtw(pExtCtx);
                        Bs3ExtCtxSetAbridgedFtw(pExtCtx, 0xff);
                    }
                    Bs3TestCheckExtCtx(pExtCtxOut, pExtCtx, 0 /*fFlags*/, pszMode, idTestStep);

                    if (TrapFrame.bXcpt != bXcptExpect)
                        Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bXcptExpect, TrapFrame.bXcpt);

                    if (bXcptExpect == X86_XCPT_DB && paTests[iTest].iRegDst != UINT8_MAX)
                        Bs3RegCtxSetGpr(&Ctx, paTests[iTest].iRegDst, paValues[iVal].uDstOut & fDstValMask, paTests[iTest].cbDst);
                    /* Kludge! Looks like EFLAGS.AC is cleared when raising #GP in real mode on the 10980XE. WEIRD! */
                    if (bMode == BS3_MODE_RM && (Ctx.rflags.u32 & X86_EFL_AC))
                    {
                        if (TrapFrame.Ctx.rflags.u32 & X86_EFL_AC)
                            Bs3TestFailedF("Expected EFLAGS.AC to be cleared (bXcpt=%d)", TrapFrame.bXcpt);
                        TrapFrame.Ctx.rflags.u32 |= X86_EFL_AC;
                    }
                    Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &Ctx, bXcptExpect == X86_XCPT_DB ? cbInstr + 1 : 0, 0,
                                         bXcptExpect == X86_XCPT_DB || BS3_MODE_IS_16BIT_SYS(bMode) ? 0 : X86_EFL_RF,
                                         pszMode, idTestStep);

                    if (   paTests[iTest].enmRm == RM_MEM
                        && Bs3MemCmp(puMemOp, &uMemOpExpect, cbOperand) != 0)
                        Bs3TestFailedF("Expected uMemOp %*.Rhxs, got %*.Rhxs", cbOperand, &uMemOpExpect, cbOperand, puMemOp);

                    if (cErrors != Bs3TestSubErrorCount())
                    {
                        if (paConfigs[iCfg].fAligned)
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect);
                        else
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x, puMemOp=%p, EFLAGS=%#RX32, CR0=%#RX32)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect, puMemOp, TrapFrame.Ctx.rflags.u32, TrapFrame.Ctx.cr0);
                        Bs3TestPrintf("\n");
                    }

                    if (uSavedFtw != 0xff)
                        Bs3ExtCtxSetAbridgedFtw(pExtCtx, uSavedFtw);
                }
            }

            bs3CpuInstr3ConfigRestore(&SavedCfg, &Ctx, pExtCtx);
        }

        /*
         * Next ring.
         */
        bRing++;
        if (bRing > 3 || bMode == BS3_MODE_RM)
            break;
        Bs3RegCtxConvertToRingX(&Ctx, bRing);
    }

    /*
     * Cleanup.
     */
    bs3CpuInstr3FreeExtCtxs(pExtCtx, pExtCtxOut);
    return 0;
}


/*
 * PMOVMSKB, VPMOVMSKB.
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_pmovmskb)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST2_VALUES_T const s_aValues[] =
    {
        { RTUINT256_INIT_C(0, 0, 0, 0), /*->*/ UINT64_C(0) },
        { RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff), UINT64_C(0xffffffff) },
        { RTUINT256_INIT_C(0x7f7f7f7f7f7f7f7f, 0x7f7f7f7f7f7f7f7f, 0x7f7f7f7f7f7f7f7f, 0x7f7f7f7f7f7f7f7f), UINT64_C(0x00000000) },
        { RTUINT256_INIT_C(0x8080808080808080, 0x8080808080808080, 0x8080808080808080, 0x8080808080808080), UINT64_C(0xffffffff) },
        { RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888), UINT64_C(0x03000003) },
        { RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb), UINT64_C(0x255193ab) },
    };

    static BS3CPUINSTR3_TEST2_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pmovmskb_EAX_MM2_icebp_c16,           255, RM_REG, T_AXMMX_OR_SSE, 4,  8, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_qword_FSxBX_icebp_c16,   255, RM_MEM, T_AXMMX_OR_SSE, 4,  8, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_XMM2_icebp_c16,          255, RM_REG, T_SSE2,         4, 16, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_dqword_FSxBX_icebp_c16,  255, RM_MEM, T_SSE2,         4, 16, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_XMM2_icebp_c16,         255, RM_REG, T_AVX_128,      4, 16, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_dqword_FSxBX_icebp_c16, 255, RM_MEM, T_AVX_128,      4, 16, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_YMM2_icebp_c16,         255, RM_REG, T_AVX2_256,     4, 32, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_qqword_FSxBX_icebp_c16, 255, RM_MEM, T_AVX2_256,     4, 32, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };

# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST2_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pmovmskb_EAX_MM2_icebp_c32,           255, RM_REG, T_AXMMX_OR_SSE, 4,  8, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_qword_FSxBX_icebp_c32,   255, RM_MEM, T_AXMMX_OR_SSE, 4,  8, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_XMM2_icebp_c32,          255, RM_REG, T_SSE2,         4, 16, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_dqword_FSxBX_icebp_c32,  255, RM_MEM, T_SSE2,         4, 16, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_XMM2_icebp_c32,         255, RM_REG, T_AVX_128,      4, 16, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_dqword_FSxBX_icebp_c32, 255, RM_MEM, T_AVX_128,      4, 16, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_YMM2_icebp_c32,         255, RM_REG, T_AVX2_256,     4, 32, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_qqword_FSxBX_icebp_c32, 255, RM_MEM, T_AVX2_256,     4, 32, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST2_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pmovmskb_EAX_MM2_icebp_c64,           255, RM_REG, T_AXMMX_OR_SSE, 8,  8, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_qword_FSxBX_icebp_c64,   255, RM_MEM, T_AXMMX_OR_SSE, 8,  8, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_XMM2_icebp_c64,          255, RM_REG, T_SSE2,         8, 16, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_pmovmskb_EAX_dqword_FSxBX_icebp_c64,  255, RM_MEM, T_SSE2,         8, 16, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_XMM2_icebp_c64,         255, RM_REG, T_AVX_128,      8, 16, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_dqword_FSxBX_icebp_c64, 255, RM_MEM, T_AVX_128,      8, 16, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_YMM2_icebp_c64,         255, RM_REG, T_AVX2_256,     8, 32, false, 0,   2, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_EAX_qqword_FSxBX_icebp_c64, 255, RM_MEM, T_AVX2_256,     8, 32, true,  1, 255, RT_ELEMENTS(s_aValues), s_aValues },
        {  bs3CpuInstr3_vpmovmskb_RAX_YMM9_icebp_c64,         255, RM_REG, T_AVX2_256,     8, 32, false, 0,   9, RT_ELEMENTS(s_aValues), s_aValues },
    };
# endif

    static BS3CPUINSTR3_TEST2_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST2_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST2_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType2(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * Test type #3.
 */

typedef struct BS3CPUINSTR3_TEST3_VALUES_T
{
    RTUINT256U      uSrc;
    RTUINT256U      uDstOut;
} BS3CPUINSTR3_TEST3_VALUES_T;

typedef struct BS3CPUINSTR3_TEST3_T
{
    FPFNBS3FAR      pfnWorker;
    uint8_t         bAvxMisalignXcpt;
    uint8_t         enmRm;
    uint8_t         enmType;
    uint8_t         iRegDst;
    uint8_t         iRegSrc;
    uint8_t         cValues;
    BS3CPUINSTR3_TEST3_VALUES_T const BS3_FAR *paValues;
} BS3CPUINSTR3_TEST3_T;

typedef struct BS3CPUINSTR3_TEST3_MODE_T
{
    BS3CPUINSTR3_TEST3_T const BS3_FAR *paTests;
    unsigned                            cTests;
} BS3CPUINSTR3_TEST3_MODE_T;

/** Initializer for a BS3CPUINSTR3_TEST3_MODE_T array (three entries). */
#if ARCH_BITS == 16
# define BS3CPUINSTR3_TEST3_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { NULL, 0 }, { NULL, 0 } }
#elif ARCH_BITS == 32
# define BS3CPUINSTR3_TEST3_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { NULL, 0 } }
#else
# define BS3CPUINSTR3_TEST3_MODES_INIT(a_aTests16, a_aTests32, a_aTests64) \
    { { a_aTests16, RT_ELEMENTS(a_aTests16) }, { a_aTests32, RT_ELEMENTS(a_aTests32) }, { a_aTests64, RT_ELEMENTS(a_aTests64) } }
#endif

/** Converts an execution mode (BS3_MODE_XXX) into an index into an array
 *  initialized by BS3CPUINSTR3_TEST3_MODES_INIT. */
#define BS3CPUINSTR3_TEST3_MODES_INDEX(a_bMode) \
    (BS3_MODE_IS_16BIT_CODE(bMode) ? 0 : BS3_MODE_IS_32BIT_CODE(bMode) ? 1 : 2)


/**
 * Test type #1 worker.
 */
static uint8_t bs3CpuInstr3_WorkerTestType3(uint8_t bMode, BS3CPUINSTR3_TEST3_T const BS3_FAR *paTests, unsigned cTests,
                                            PCBS3CPUINSTR3_CONFIG_T paConfigs, unsigned cConfigs)
{
    const char BS3_FAR * const  pszMode = Bs3GetModeName(bMode);
    BS3REGCTX                   Ctx;
    BS3TRAPFRAME                TrapFrame;
    uint8_t                     bRing   = BS3_MODE_IS_V86(bMode) ? 3 : 0;
    PBS3EXTCTX                  pExtCtxOut;
    PBS3EXTCTX                  pExtCtx = bs3CpuInstr3AllocExtCtxs(&pExtCtxOut);
    if (!pExtCtx)
        return 0;

    /* Ensure the structures are allocated before we sample the stack pointer. */
    Bs3MemSet(&Ctx, 0, sizeof(Ctx));
    Bs3MemSet(&TrapFrame, 0, sizeof(TrapFrame));

    /* Ensure that the globals we use here have been initialized. */
    bs3CpuInstr3InitGlobals();

    /*
     * Create test context.
     */
    Bs3RegCtxSaveEx(&Ctx, bMode, 1024);
    bs3CpuInstr3SetupSseAndAvx(&Ctx, pExtCtx);

    /*
     * Run the tests in all rings since alignment issues may behave
     * differently in ring-3 compared to ring-0.
     */
    for (;;)
    {
        unsigned iCfg;
        for (iCfg = 0; iCfg < cConfigs; iCfg++)
        {
            unsigned                    iTest;
            BS3CPUINSTR3_CONFIG_SAVED_T SavedCfg;
            if (!bs3CpuInstr3ConfigReconfigure(&SavedCfg, &Ctx, pExtCtx, &paConfigs[iCfg], bMode))
                continue; /* unsupported config */

            /*
             * Iterate the tests.
             */
            for (iTest = 0; iTest < cTests; iTest++)
            {
                BS3CPUINSTR3_TEST3_VALUES_T const BS3_FAR *paValues = paTests[iTest].paValues;
                uint8_t const   cbInstr     = ((uint8_t const BS3_FAR *)(uintptr_t)paTests[iTest].pfnWorker)[-1];
                unsigned const  cValues     = paTests[iTest].cValues;
                bool const      fMmxInstr   = paTests[iTest].enmType < T_SSE;
                bool const      fSseInstr   = paTests[iTest].enmType >= T_SSE && paTests[iTest].enmType < T_AVX_128;
                bool const      fAvxInstr   = paTests[iTest].enmType >= T_AVX_128;
                uint8_t const   cbOperand   = paTests[iTest].enmType < T_128BITS ? 64/8
                                            : paTests[iTest].enmType < T_256BITS ? 128/8 : 256/8;
                uint8_t const   cbAlign     = RT_MIN(cbOperand, 16);
                uint8_t         bXcptExpect = !g_afTypeSupports[paTests[iTest].enmType] ? X86_XCPT_UD
                                            : fMmxInstr ? paConfigs[iCfg].bXcptMmx
                                            : fSseInstr ? paConfigs[iCfg].bXcptSse
                                            : BS3_MODE_IS_RM_OR_V86(bMode) ? X86_XCPT_UD : paConfigs[iCfg].bXcptAvx;
                uint16_t        idTestStep  = bRing * 10000 + iCfg * 100 + iTest * 10;
                unsigned        iVal;
                uint8_t         abPadding[sizeof(RTUINT256U) * 2];
                unsigned const  offPadding  = (BS3_FP_OFF(&abPadding[sizeof(RTUINT256U)]) & ~(size_t)(cbAlign - 1))
                                            - BS3_FP_OFF(&abPadding[0]);
                PRTUINT256U     puMemOp     = (PRTUINT256U)&abPadding[offPadding - !paConfigs[iCfg].fAligned];
                BS3_ASSERT((uint8_t BS3_FAR *)puMemOp - &abPadding[0] <= sizeof(RTUINT256U));

                /* If testing unaligned memory accesses, skip register-only tests.  This allows
                   setting bXcptMmx, bXcptSse and bXcptAvx to reflect the misaligned exceptions.  */
                if (!paConfigs[iCfg].fAligned && paTests[iTest].enmRm != RM_MEM)
                    continue;

                /* #AC is only raised in ring-3.: */
                if (bXcptExpect == X86_XCPT_AC)
                {
                    if (bRing != 3)
                        bXcptExpect = X86_XCPT_DB;
                    else if (fAvxInstr)
                        bXcptExpect = paTests[iTest].bAvxMisalignXcpt; /* they generally don't raise #AC */
                }

                Bs3RegCtxSetRipCsFromCurPtr(&Ctx, paTests[iTest].pfnWorker);

                /*
                 * Iterate the test values and do the actual testing.
                 */
                for (iVal = 0; iVal < cValues; iVal++, idTestStep++)
                {
                    uint16_t   cErrors;
                    uint16_t   uSavedFtw = 0xff;
                    RTUINT256U uMemOpExpect;

                    /*
                     * Set up the context and some expectations.
                     */
                    /* dest */
                    if (paTests[iTest].iRegDst == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        Bs3MemSet(puMemOp, sizeof(*puMemOp), 0xcc);
                        if (bXcptExpect == X86_XCPT_DB)
                            uMemOpExpect = paValues[iVal].uDstOut;
                        else
                            uMemOpExpect = *puMemOp;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc, ~paValues[iVal].uDstOut.QWords.qw0, BS3EXTCTXTOPMM_ZERO);

                    /* source */
                    if (paTests[iTest].iRegSrc == UINT8_MAX)
                    {
                        BS3_ASSERT(paTests[iTest].enmRm == RM_MEM);
                        BS3_ASSERT(paTests[iTest].iRegDst != UINT8_MAX);
                        *puMemOp = uMemOpExpect = paValues[iVal].uSrc;
                        uMemOpExpect = paValues[iVal].uSrc;
                    }
                    else if (fMmxInstr)
                        Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegSrc, paValues[iVal].uSrc.QWords.qw0, BS3EXTCTXTOPMM_ZERO);
                    else if (fSseInstr)
                        Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegSrc, &paValues[iVal].uSrc.DQWords.dqw0);
                    else
                        Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegSrc, &paValues[iVal].uSrc, 32);

                    /* Memory pointer. */
                    if (paTests[iTest].enmRm == RM_MEM)
                    {
                        BS3_ASSERT(   paTests[iTest].iRegDst == UINT8_MAX
                                   || paTests[iTest].iRegSrc == UINT8_MAX);
                        Bs3RegCtxSetGrpSegFromCurPtr(&Ctx, &Ctx.rbx, &Ctx.fs, puMemOp);
                    }

                    /*
                     * Execute.
                     */
                    Bs3ExtCtxRestore(pExtCtx);
                    Bs3TrapSetJmpAndRestore(&Ctx, &TrapFrame);
                    Bs3ExtCtxSave(pExtCtxOut);

                    /*
                     * Check the result:
                     */
                    cErrors = Bs3TestSubErrorCount();

                    if (bXcptExpect == X86_XCPT_DB && fMmxInstr)
                    {
                        uSavedFtw = Bs3ExtCtxGetAbridgedFtw(pExtCtx);
                        Bs3ExtCtxSetAbridgedFtw(pExtCtx, 0xff);
                    }
                    if (bXcptExpect == X86_XCPT_DB && paTests[iTest].iRegDst != UINT8_MAX)
                    {
                        if (fMmxInstr)
                            Bs3ExtCtxSetMm(pExtCtx, paTests[iTest].iRegDst, paValues[iVal].uDstOut.QWords.qw0, BS3EXTCTXTOPMM_SET);
                        else if (fSseInstr)
                            Bs3ExtCtxSetXmm(pExtCtx, paTests[iTest].iRegDst, &paValues[iVal].uDstOut.DQWords.dqw0);
                        else
                            Bs3ExtCtxSetYmm(pExtCtx, paTests[iTest].iRegDst, &paValues[iVal].uDstOut, cbOperand);
                    }
                    Bs3TestCheckExtCtx(pExtCtxOut, pExtCtx, 0 /*fFlags*/, pszMode, idTestStep);

                    if (TrapFrame.bXcpt != bXcptExpect)
                        Bs3TestFailedF("Expected bXcpt = %#x, got %#x", bXcptExpect, TrapFrame.bXcpt);

                    /* Kludge! Looks like EFLAGS.AC is cleared when raising #GP in real mode on the 10980XE. WEIRD! */
                    if (bMode == BS3_MODE_RM && (Ctx.rflags.u32 & X86_EFL_AC))
                    {
                        if (TrapFrame.Ctx.rflags.u32 & X86_EFL_AC)
                            Bs3TestFailedF("Expected EFLAGS.AC to be cleared (bXcpt=%d)", TrapFrame.bXcpt);
                        TrapFrame.Ctx.rflags.u32 |= X86_EFL_AC;
                    }
                    Bs3TestCheckRegCtxEx(&TrapFrame.Ctx, &Ctx, bXcptExpect == X86_XCPT_DB ? cbInstr + 1 : 0, 0,
                                         bXcptExpect == X86_XCPT_DB || BS3_MODE_IS_16BIT_SYS(bMode) ? 0 : X86_EFL_RF,
                                         pszMode, idTestStep);

                    if (   paTests[iTest].enmRm == RM_MEM
                        && Bs3MemCmp(puMemOp, &uMemOpExpect, cbOperand) != 0)
                        Bs3TestFailedF("Expected uMemOp %*.Rhxs, got %*.Rhxs", cbOperand, &uMemOpExpect, cbOperand, puMemOp);

                    if (cErrors != Bs3TestSubErrorCount())
                    {
                        if (paConfigs[iCfg].fAligned)
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect);
                        else
                            Bs3TestFailedF("ring-%d/cfg#%u/test#%u/value#%u failed (bXcptExpect=%#x, puMemOp=%p, EFLAGS=%#RX32, CR0=%#RX32)",
                                           bRing, iCfg, iTest, iVal, bXcptExpect, puMemOp, TrapFrame.Ctx.rflags.u32, TrapFrame.Ctx.cr0);
                        Bs3TestPrintf("\n");
                    }

                    if (uSavedFtw != 0xff)
                        Bs3ExtCtxSetAbridgedFtw(pExtCtx, uSavedFtw);
                }
            }

            bs3CpuInstr3ConfigRestore(&SavedCfg, &Ctx, pExtCtx);
        }

        /*
         * Next ring.
         */
        bRing++;
        if (bRing > 3 || bMode == BS3_MODE_RM)
            break;
        Bs3RegCtxConvertToRingX(&Ctx, bRing);
    }

    /*
     * Cleanup.
     */
    bs3CpuInstr3FreeExtCtxs(pExtCtx, pExtCtxOut);
    return 0;
}


/*
 * PSHUFW
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_pshufw)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValuesFF[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0, 0, 0, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0, 0, 0, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0x5555555555555555) },
        {            RTUINT256_INIT_C(0, 0, 0, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0x9c5c9c5c9c5c9c5c) },
    };

    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValues1B[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0, 0, 0, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0, 0, 0, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0x8888777766665555) },
        {            RTUINT256_INIT_C(0, 0, 0, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0x96bb9309e0739c5c) },
    };

    static BS3CPUINSTR3_TEST3_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pshufw_MM1_MM2_0FFh_icebp_c16,   255, RM_REG, T_AXMMX_OR_SSE, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufw_MM1_FSxBX_0FFh_icebp_c16, 255, RM_MEM, T_AXMMX_OR_SSE, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufw_MM1_MM2_01Bh_icebp_c16,   255, RM_REG, T_AXMMX_OR_SSE, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufw_MM1_FSxBX_01Bh_icebp_c16, 255, RM_MEM, T_AXMMX_OR_SSE, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST3_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pshufw_MM1_MM2_0FFh_icebp_c32,   255, RM_REG, T_AXMMX_OR_SSE, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufw_MM1_FSxBX_0FFh_icebp_c32, 255, RM_MEM, T_AXMMX_OR_SSE, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufw_MM1_MM2_01Bh_icebp_c32,   255, RM_REG, T_AXMMX_OR_SSE, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufw_MM1_FSxBX_01Bh_icebp_c32, 255, RM_MEM, T_AXMMX_OR_SSE, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST3_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pshufw_MM1_MM2_0FFh_icebp_c64,   255, RM_REG, T_AXMMX_OR_SSE, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufw_MM1_FSxBX_0FFh_icebp_c64, 255, RM_MEM, T_AXMMX_OR_SSE, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufw_MM1_MM2_01Bh_icebp_c64,   255, RM_REG, T_AXMMX_OR_SSE, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufw_MM1_FSxBX_01Bh_icebp_c64, 255, RM_MEM, T_AXMMX_OR_SSE, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif

    static BS3CPUINSTR3_TEST3_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST3_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST3_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType3(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PSHUFHW
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_pshufhw)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValuesFF[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0x5555555555555555, 0x1111222233334444, 0x1111111111111111, 0x5555666677778888) },
        {            RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0x4d094d094d094d09, 0x3ef417c8666b3fe6, 0xb421b421b421b421, 0x9c5ce073930996bb) },
    };

    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValues1B[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0x8888777766665555, 0x1111222233334444, 0x4444333322221111, 0x5555666677778888) },
        {            RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0x73d56cdcf02a4d09, 0x3ef417c8666b3fe6, 0x9ba2564c2fa8b421, 0x9c5ce073930996bb) },
    };

    static BS3CPUINSTR3_TEST3_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pshufhw_XMM1_XMM2_0FFh_icebp_c16,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufhw_XMM1_FSxBX_0FFh_icebp_c16,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufhw_XMM1_XMM2_01Bh_icebp_c16,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufhw_XMM1_FSxBX_01Bh_icebp_c16,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufhw_XMM1_XMM2_0FFh_icebp_c16,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_XMM1_FSxBX_0FFh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_XMM1_XMM2_01Bh_icebp_c16,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_XMM1_FSxBX_01Bh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufhw_YMM1_YMM2_0FFh_icebp_c16,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM1_FSxBX_0FFh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM1_YMM2_01Bh_icebp_c16,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_YMM1_FSxBX_01Bh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST3_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pshufhw_XMM1_XMM2_0FFh_icebp_c32,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufhw_XMM1_FSxBX_0FFh_icebp_c32,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufhw_XMM1_XMM2_01Bh_icebp_c32,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufhw_XMM1_FSxBX_01Bh_icebp_c32,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufhw_XMM1_XMM2_0FFh_icebp_c32,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_XMM1_FSxBX_0FFh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_XMM1_XMM2_01Bh_icebp_c32,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_XMM1_FSxBX_01Bh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufhw_YMM1_YMM2_0FFh_icebp_c32,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM1_FSxBX_0FFh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM1_YMM2_01Bh_icebp_c32,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_YMM1_FSxBX_01Bh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST3_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pshufhw_XMM1_XMM2_0FFh_icebp_c64,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufhw_XMM1_FSxBX_0FFh_icebp_c64,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufhw_XMM1_XMM2_01Bh_icebp_c64,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufhw_XMM1_FSxBX_01Bh_icebp_c64,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufhw_XMM1_XMM2_0FFh_icebp_c64,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_XMM1_FSxBX_0FFh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_XMM1_XMM2_01Bh_icebp_c64,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_XMM1_FSxBX_01Bh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufhw_YMM1_YMM2_0FFh_icebp_c64,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM1_FSxBX_0FFh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM1_YMM2_01Bh_icebp_c64,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_YMM1_FSxBX_01Bh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufhw_YMM12_YMM7_0FFh_icebp_c64, 255,         RM_REG, T_AVX2_256, 12,  7, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufhw_YMM9_YMM12_01Bh_icebp_c64, 255,         RM_REG, T_AVX2_256, 9,  12, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif

    static BS3CPUINSTR3_TEST3_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST3_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST3_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType3(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PSHUFLW
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_pshuflw)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValuesFF[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0x5555666677778888, 0x1111111111111111, 0x1111222233334444, 0x5555555555555555) },
        {            RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef43ef43ef43ef4, 0xb4212fa8564c9ba2, 0x9c5c9c5c9c5c9c5c) },
    };

    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValues1B[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0x5555666677778888, 0x4444333322221111, 0x1111222233334444, 0x8888777766665555) },
        {            RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3fe6666b17c83ef4, 0xb4212fa8564c9ba2, 0x96bb9309e0739c5c) },
    };

    static BS3CPUINSTR3_TEST3_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pshuflw_XMM1_XMM2_0FFh_icebp_c16,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshuflw_XMM1_FSxBX_0FFh_icebp_c16,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshuflw_XMM1_XMM2_01Bh_icebp_c16,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshuflw_XMM1_FSxBX_01Bh_icebp_c16,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshuflw_XMM1_XMM2_0FFh_icebp_c16,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_XMM1_FSxBX_0FFh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_XMM1_XMM2_01Bh_icebp_c16,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_XMM1_FSxBX_01Bh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshuflw_YMM1_YMM2_0FFh_icebp_c16,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM1_FSxBX_0FFh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM1_YMM2_01Bh_icebp_c16,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_YMM1_FSxBX_01Bh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST3_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pshuflw_XMM1_XMM2_0FFh_icebp_c32,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshuflw_XMM1_FSxBX_0FFh_icebp_c32,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshuflw_XMM1_XMM2_01Bh_icebp_c32,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshuflw_XMM1_FSxBX_01Bh_icebp_c32,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshuflw_XMM1_XMM2_0FFh_icebp_c32,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_XMM1_FSxBX_0FFh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_XMM1_XMM2_01Bh_icebp_c32,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_XMM1_FSxBX_01Bh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshuflw_YMM1_YMM2_0FFh_icebp_c32,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM1_FSxBX_0FFh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM1_YMM2_01Bh_icebp_c32,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_YMM1_FSxBX_01Bh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST3_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pshuflw_XMM1_XMM2_0FFh_icebp_c64,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshuflw_XMM1_FSxBX_0FFh_icebp_c64,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshuflw_XMM1_XMM2_01Bh_icebp_c64,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshuflw_XMM1_FSxBX_01Bh_icebp_c64,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshuflw_XMM1_XMM2_0FFh_icebp_c64,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_XMM1_FSxBX_0FFh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_XMM1_XMM2_01Bh_icebp_c64,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_XMM1_FSxBX_01Bh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshuflw_YMM1_YMM2_0FFh_icebp_c64,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM1_FSxBX_0FFh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM1_YMM2_01Bh_icebp_c64,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_YMM1_FSxBX_01Bh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshuflw_YMM12_YMM7_0FFh_icebp_c64, 255,         RM_REG, T_AVX2_256, 12,  7, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshuflw_YMM9_YMM12_01Bh_icebp_c64, 255,         RM_REG, T_AVX2_256, 9,  12, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif

    static BS3CPUINSTR3_TEST3_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST3_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST3_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType3(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}


/*
 * PSHUFHD
 */
BS3_DECL_FAR(uint8_t) BS3_CMN_NM(bs3CpuInstr3_v_pshufd)(uint8_t bMode)
{
    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValuesFF[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0x5555666655556666, 0x5555666655556666, 0x1111222211112222, 0x1111222211112222) },
        {            RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0x4d09f02a4d09f02a, 0x4d09f02a4d09f02a, 0xb4212fa8b4212fa8, 0xb4212fa8b4212fa8) },
    };

    static BS3CPUINSTR3_TEST3_VALUES_T const s_aValues1B[] =
    {
        {            RTUINT256_INIT_C(0, 0, 0, 0),
            /* => */ RTUINT256_INIT_C(0, 0, 0, 0) },
        {            RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff),
            /* => */ RTUINT256_INIT_C(0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff, 0xffffffffffffffff) },
        {            RTUINT256_INIT_C(0x5555666677778888, 0x1111222233334444, 0x1111222233334444, 0x5555666677778888),
            /* => */ RTUINT256_INIT_C(0x3333444411112222, 0x7777888855556666, 0x7777888855556666, 0x3333444411112222) },
        {            RTUINT256_INIT_C(0x4d09f02a6cdc73d5, 0x3ef417c8666b3fe6, 0xb4212fa8564c9ba2, 0x9c5ce073930996bb),
            /* => */ RTUINT256_INIT_C(0x666b3fe63ef417c8, 0x6cdc73d54d09f02a, 0x930996bb9c5ce073, 0x564c9ba2b4212fa8) },
    };

    static BS3CPUINSTR3_TEST3_T const s_aTests16[] =
    {
        {  bs3CpuInstr3_pshufd_XMM1_XMM2_0FFh_icebp_c16,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufd_XMM1_FSxBX_0FFh_icebp_c16,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufd_XMM1_XMM2_01Bh_icebp_c16,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufd_XMM1_FSxBX_01Bh_icebp_c16,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufd_XMM1_XMM2_0FFh_icebp_c16,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_XMM1_FSxBX_0FFh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_XMM1_XMM2_01Bh_icebp_c16,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_XMM1_FSxBX_01Bh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufd_YMM1_YMM2_0FFh_icebp_c16,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM1_FSxBX_0FFh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM1_YMM2_01Bh_icebp_c16,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_YMM1_FSxBX_01Bh_icebp_c16, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# if ARCH_BITS >= 32
    static BS3CPUINSTR3_TEST3_T const s_aTests32[] =
    {
        {  bs3CpuInstr3_pshufd_XMM1_XMM2_0FFh_icebp_c32,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufd_XMM1_FSxBX_0FFh_icebp_c32,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufd_XMM1_XMM2_01Bh_icebp_c32,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufd_XMM1_FSxBX_01Bh_icebp_c32,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufd_XMM1_XMM2_0FFh_icebp_c32,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_XMM1_FSxBX_0FFh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_XMM1_XMM2_01Bh_icebp_c32,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_XMM1_FSxBX_01Bh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufd_YMM1_YMM2_0FFh_icebp_c32,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM1_FSxBX_0FFh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM1_YMM2_01Bh_icebp_c32,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_YMM1_FSxBX_01Bh_icebp_c32, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif
# if ARCH_BITS >= 64
    static BS3CPUINSTR3_TEST3_T const s_aTests64[] =
    {
        {  bs3CpuInstr3_pshufd_XMM1_XMM2_0FFh_icebp_c64,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufd_XMM1_FSxBX_0FFh_icebp_c64,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_pshufd_XMM1_XMM2_01Bh_icebp_c64,   255,         RM_REG, T_SSE2,     1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_pshufd_XMM1_FSxBX_01Bh_icebp_c64,  255,         RM_MEM, T_SSE2,     1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufd_XMM1_XMM2_0FFh_icebp_c64,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_XMM1_FSxBX_0FFh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_XMM1_XMM2_01Bh_icebp_c64,  255,         RM_REG, T_AVX_128,  1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_XMM1_FSxBX_01Bh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX_128,  1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },

        {  bs3CpuInstr3_vpshufd_YMM1_YMM2_0FFh_icebp_c64,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM1_FSxBX_0FFh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM1_YMM2_01Bh_icebp_c64,  255,         RM_REG, T_AVX2_256, 1,   2, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_YMM1_FSxBX_01Bh_icebp_c64, X86_XCPT_DB, RM_MEM, T_AVX2_256, 1, 255, RT_ELEMENTS(s_aValues1B), s_aValues1B },
        {  bs3CpuInstr3_vpshufd_YMM12_YMM7_0FFh_icebp_c64, 255,         RM_REG, T_AVX2_256, 12,  7, RT_ELEMENTS(s_aValuesFF), s_aValuesFF },
        {  bs3CpuInstr3_vpshufd_YMM9_YMM12_01Bh_icebp_c64, 255,         RM_REG, T_AVX2_256, 9,  12, RT_ELEMENTS(s_aValues1B), s_aValues1B },
    };
# endif

    static BS3CPUINSTR3_TEST3_MODE_T const s_aTests[3] = BS3CPUINSTR3_TEST3_MODES_INIT(s_aTests16, s_aTests32, s_aTests64);
    unsigned const                         iTest       = BS3CPUINSTR3_TEST3_MODES_INDEX(bMode);
    return bs3CpuInstr3_WorkerTestType3(bMode, s_aTests[iTest].paTests, s_aTests[iTest].cTests,
                                        g_aXcptConfig4, RT_ELEMENTS(g_aXcptConfig4));
}



#endif /* BS3_INSTANTIATING_CMN */



/*
 * Mode specific code.
 * Mode specific code.
 * Mode specific code.
 */
#ifdef BS3_INSTANTIATING_MODE


#endif /* BS3_INSTANTIATING_MODE */

