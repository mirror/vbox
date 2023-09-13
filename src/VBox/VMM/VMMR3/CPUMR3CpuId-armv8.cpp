/* $Id$ */
/** @file
 * CPUM - CPU ID part for ARMv8 hypervisor.
 */

/*
 * Copyright (C) 2023 Oracle and/or its affiliates.
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
 * SPDX-License-Identifier: GPL-3.0-only
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#define CPUM_WITH_NONCONST_HOST_FEATURES /* required for initializing parts of the g_CpumHostFeatures structure here. */
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/nem.h>
#include <VBox/vmm/ssm.h>
#include "CPUMInternal-armv8.h"
#include <VBox/vmm/vmcc.h>

#include <VBox/err.h>
#include <iprt/ctype.h>
#include <iprt/mem.h>
#include <iprt/string.h>


/*********************************************************************************************************************************
*   Defined Constants And Macros                                                                                                 *
*********************************************************************************************************************************/

/*
 *
 * Init related code.
 * Init related code.
 * Init related code.
 *
 *
 */

/** @name Instruction Set Extension Options
 * @{  */
/** Configuration option type (extended boolean, really). */
typedef uint8_t CPUMISAEXTCFG;
/** Always disable the extension. */
#define CPUMISAEXTCFG_DISABLED              false
/** Enable the extension if it's supported by the host CPU. */
#define CPUMISAEXTCFG_ENABLED_SUPPORTED     true
/** Enable the extension if it's supported by the host CPU, but don't let
 * the portable CPUID feature disable it. */
#define CPUMISAEXTCFG_ENABLED_PORTABLE      UINT8_C(127)
/** Always enable the extension. */
#define CPUMISAEXTCFG_ENABLED_ALWAYS        UINT8_C(255)
/** @} */

/**
 * CPUID Configuration (from CFGM).
 *
 * @remarks  The members aren't document since we would only be duplicating the
 *           \@cfgm entries in cpumR3CpuIdReadConfig.
 */
typedef struct CPUMCPUIDCONFIG
{
    CPUMISAEXTCFG   enmAes;
    CPUMISAEXTCFG   enmPmull;
    CPUMISAEXTCFG   enmSha1;
    CPUMISAEXTCFG   enmSha256;
    CPUMISAEXTCFG   enmSha512;
    CPUMISAEXTCFG   enmCrc32;
    CPUMISAEXTCFG   enmSha3;

    char            szCpuName[128];
} CPUMCPUIDCONFIG;
/** Pointer to CPUID config (from CFGM). */
typedef CPUMCPUIDCONFIG *PCPUMCPUIDCONFIG;


/**
 * Explode the CPU features from the given ID registers.
 *
 * @returns VBox status code.
 * @param   pIdRegs             The ID registers to explode the features from.
 * @param   pFeatures           Where to store the features to.
 */
static int cpumCpuIdExplodeFeatures(PCCPUMIDREGS pIdRegs, PCPUMFEATURES pFeatures)
{
    uint64_t u64IdReg = pIdRegs->u64RegIdAa64Mmfr0El1;

    static uint8_t s_aPaRange[] = { 32, 36, 40, 42, 44, 48, 52 };
    AssertLogRelMsgReturn(RT_BF_GET(u64IdReg, ARMV8_ID_AA64MMFR0_EL1_PARANGE) < RT_ELEMENTS(s_aPaRange),
                          ("CPUM: Invalid/Unsupported PARange value in ID_AA64MMFR0_EL1 register: %u\n",
                          RT_BF_GET(u64IdReg, ARMV8_ID_AA64MMFR0_EL1_PARANGE)),
                          VERR_CPUM_IPE_1);

    pFeatures->cMaxPhysAddrWidth = s_aPaRange[RT_BF_GET(u64IdReg, ARMV8_ID_AA64MMFR0_EL1_PARANGE)];
    pFeatures->fTGran4K          = RT_BF_GET(u64IdReg, ARMV8_ID_AA64MMFR0_EL1_TGRAN4)  != ARMV8_ID_AA64MMFR0_EL1_TGRAN4_NOT_IMPL;
    pFeatures->fTGran16K         = RT_BF_GET(u64IdReg, ARMV8_ID_AA64MMFR0_EL1_TGRAN16) != ARMV8_ID_AA64MMFR0_EL1_TGRAN16_NOT_IMPL;
    pFeatures->fTGran64K         = RT_BF_GET(u64IdReg, ARMV8_ID_AA64MMFR0_EL1_TGRAN64) != ARMV8_ID_AA64MMFR0_EL1_TGRAN64_NOT_IMPL;

    /* ID_AA64ISAR0_EL1 features. */
    u64IdReg = pIdRegs->u64RegIdAa64Isar0El1;
    pFeatures->fAes              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_AES)     >= ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED;
    pFeatures->fPmull            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_AES)     >= ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED_PMULL;
    pFeatures->fSha1             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA1)    >= ARMV8_ID_AA64ISAR0_EL1_SHA1_SUPPORTED;
    pFeatures->fSha256           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA2)    >= ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256;
    pFeatures->fSha512           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA2)    >= ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256_SHA512;
    pFeatures->fCrc32            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_CRC32)   >= ARMV8_ID_AA64ISAR0_EL1_CRC32_SUPPORTED;
    pFeatures->fLse              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_ATOMIC)  >= ARMV8_ID_AA64ISAR0_EL1_ATOMIC_SUPPORTED;
    pFeatures->fTme              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_TME)     >= ARMV8_ID_AA64ISAR0_EL1_TME_SUPPORTED;
    pFeatures->fRdm              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_RDM)     >= ARMV8_ID_AA64ISAR0_EL1_RDM_SUPPORTED;
    pFeatures->fSha3             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA3)    >= ARMV8_ID_AA64ISAR0_EL1_SHA3_SUPPORTED;
    pFeatures->fSm3              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SM3)     >= ARMV8_ID_AA64ISAR0_EL1_SM3_SUPPORTED;
    pFeatures->fSm4              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SM4)     >= ARMV8_ID_AA64ISAR0_EL1_SM4_SUPPORTED;
    pFeatures->fDotProd          = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_DP)      >= ARMV8_ID_AA64ISAR0_EL1_DP_SUPPORTED;
    pFeatures->fFhm              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_FHM)     >= ARMV8_ID_AA64ISAR0_EL1_FHM_SUPPORTED;
    pFeatures->fFlagM            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_TS)      >= ARMV8_ID_AA64ISAR0_EL1_TS_SUPPORTED;
    pFeatures->fFlagM2           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_TS)      >= ARMV8_ID_AA64ISAR0_EL1_TS_SUPPORTED_2;
    pFeatures->fTlbios           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_TLB)     >= ARMV8_ID_AA64ISAR0_EL1_TLB_SUPPORTED;
    pFeatures->fTlbirange        = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_TLB)     >= ARMV8_ID_AA64ISAR0_EL1_TLB_SUPPORTED_RANGE;
    pFeatures->fRng              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_RNDR)    >= ARMV8_ID_AA64ISAR0_EL1_RNDR_SUPPORTED;

    /* ID_AA64ISAR1_EL1 features. */
    u64IdReg = pIdRegs->u64RegIdAa64Isar1El1;
    pFeatures->fDpb              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_DPB)     >= ARMV8_ID_AA64ISAR1_EL1_DPB_SUPPORTED;
    pFeatures->fDpb2             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_DPB)     >= ARMV8_ID_AA64ISAR1_EL1_DPB_SUPPORTED_2;

    /* PAuth using QARMA5. */
    pFeatures->fPacQarma5        = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_APA)     != ARMV8_ID_AA64ISAR1_EL1_APA_NOT_IMPL;
    pFeatures->fPAuth            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_APA)     >= ARMV8_ID_AA64ISAR1_EL1_APA_SUPPORTED_PAUTH;
    pFeatures->fEpac             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_APA)     >= ARMV8_ID_AA64ISAR1_EL1_APA_SUPPORTED_EPAC;
    pFeatures->fPAuth2           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_APA)     >= ARMV8_ID_AA64ISAR1_EL1_APA_SUPPORTED_PAUTH2;
    pFeatures->fFpac             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_APA)     >= ARMV8_ID_AA64ISAR1_EL1_APA_SUPPORTED_FPAC;
    pFeatures->fFpacCombine      = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_APA)     >= ARMV8_ID_AA64ISAR1_EL1_APA_SUPPORTED_FPACCOMBINE;

    /* PAuth using implementation defined algorithm. */
    pFeatures->fPacImp           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_API)     != ARMV8_ID_AA64ISAR1_EL1_API_NOT_IMPL;
    pFeatures->fPAuth            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_API)     >= ARMV8_ID_AA64ISAR1_EL1_API_SUPPORTED_PAUTH;
    pFeatures->fEpac             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_API)     >= ARMV8_ID_AA64ISAR1_EL1_API_SUPPORTED_EPAC;
    pFeatures->fPAuth2           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_API)     >= ARMV8_ID_AA64ISAR1_EL1_API_SUPPORTED_PAUTH2;
    pFeatures->fFpac             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_API)     >= ARMV8_ID_AA64ISAR1_EL1_API_SUPPORTED_FPAC;
    pFeatures->fFpacCombine      = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_API)     >= ARMV8_ID_AA64ISAR1_EL1_API_SUPPORTED_FPACCOMBINE;

    pFeatures->fJscvt            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_FJCVTZS) >= ARMV8_ID_AA64ISAR1_EL1_FJCVTZS_SUPPORTED;
    pFeatures->fFcma             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_FCMA)    >= ARMV8_ID_AA64ISAR1_EL1_FCMA_SUPPORTED;
    pFeatures->fLrcpc            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_LRCPC)   >= ARMV8_ID_AA64ISAR1_EL1_LRCPC_SUPPORTED;
    pFeatures->fLrcpc2           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_LRCPC)   >= ARMV8_ID_AA64ISAR1_EL1_LRCPC_SUPPORTED_2;
    pFeatures->fFrintts          = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_FRINTTS) >= ARMV8_ID_AA64ISAR1_EL1_FRINTTS_SUPPORTED;
    pFeatures->fSb               = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_SB)      >= ARMV8_ID_AA64ISAR1_EL1_SB_SUPPORTED;
    pFeatures->fSpecres          = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_SPECRES) >= ARMV8_ID_AA64ISAR1_EL1_SPECRES_SUPPORTED;
    pFeatures->fBf16             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_BF16)    >= ARMV8_ID_AA64ISAR1_EL1_BF16_SUPPORTED_BF16;
    pFeatures->fEbf16            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_BF16)    >= ARMV8_ID_AA64ISAR1_EL1_BF16_SUPPORTED_EBF16;
    pFeatures->fDgh              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_DGH)     >= ARMV8_ID_AA64ISAR1_EL1_DGH_SUPPORTED;
    pFeatures->fI8mm             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_I8MM)    >= ARMV8_ID_AA64ISAR1_EL1_I8MM_SUPPORTED;
    pFeatures->fXs               = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_XS)      >= ARMV8_ID_AA64ISAR1_EL1_XS_SUPPORTED;
    pFeatures->fLs64             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_LS64)    >= ARMV8_ID_AA64ISAR1_EL1_LS64_SUPPORTED;
    pFeatures->fLs64V            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_LS64)    >= ARMV8_ID_AA64ISAR1_EL1_LS64_SUPPORTED_V;
    pFeatures->fLs64Accdata      = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR1_EL1_LS64)    >= ARMV8_ID_AA64ISAR1_EL1_LS64_SUPPORTED_ACCDATA;

    /* ID_AA64ISAR2_EL1 features. */
    u64IdReg = pIdRegs->u64RegIdAa64Isar2El1;
    pFeatures->fWfxt             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_WFXT)    >= ARMV8_ID_AA64ISAR2_EL1_WFXT_SUPPORTED;
    pFeatures->fRpres            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_RPRES)   >= ARMV8_ID_AA64ISAR2_EL1_RPRES_SUPPORTED;

    /* PAuth using QARMA3. */
    pFeatures->fPacQarma3        = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_GPA3)    >= ARMV8_ID_AA64ISAR2_EL1_GPA3_SUPPORTED;
    pFeatures->fPacQarma3        = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_APA3)    != ARMV8_ID_AA64ISAR2_EL1_APA3_NOT_IMPL;
    pFeatures->fPAuth            = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_APA3)    >= ARMV8_ID_AA64ISAR2_EL1_APA3_SUPPORTED_PAUTH;
    pFeatures->fEpac             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_APA3)    >= ARMV8_ID_AA64ISAR2_EL1_APA3_SUPPORTED_EPAC;
    pFeatures->fPAuth2           = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_APA3)    >= ARMV8_ID_AA64ISAR2_EL1_APA3_SUPPORTED_PAUTH2;
    pFeatures->fFpac             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_APA3)    >= ARMV8_ID_AA64ISAR2_EL1_APA3_SUPPORTED_FPAC;
    pFeatures->fFpacCombine      = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_APA3)    >= ARMV8_ID_AA64ISAR2_EL1_APA3_SUPPORTED_FPACCOMBINE;

    pFeatures->fMops             = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_MOPS)    >= ARMV8_ID_AA64ISAR2_EL1_MOPS_SUPPORTED;
    pFeatures->fHbc              = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_BC)      >= ARMV8_ID_AA64ISAR2_EL1_BC_SUPPORTED;
    pFeatures->fConstPacField    = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR2_EL1_PACFRAC) >= ARMV8_ID_AA64ISAR2_EL1_PACFRAC_TRUE;

    return VINF_SUCCESS;
}


/**
 * Sanitizes and adjust the CPUID leaves.
 *
 * Drop features that aren't virtualized (or virtualizable).  Adjust information
 * and capabilities to fit the virtualized hardware.  Remove information the
 * guest shouldn't have (because it's wrong in the virtual world or because it
 * gives away host details) or that we don't have documentation for and no idea
 * what means.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure (for cCpus).
 * @param   pCpum       The CPUM instance data.
 * @param   pConfig     The CPUID configuration we've read from CFGM.
 */
static int cpumR3CpuIdSanitize(PVM pVM, PCPUM pCpum, PCPUMCPUIDCONFIG pConfig)
{
#define PORTABLE_CLEAR_BITS_WHEN(Lvl, a_pLeafReg, FeatNm, fMask, uValue) \
    if ( pCpum->u8PortableCpuIdLevel >= (Lvl) && ((a_pLeafReg) & (fMask)) == (uValue) ) \
    { \
        LogRel(("PortableCpuId: " #a_pLeafReg "[" #FeatNm "]: %#x -> 0\n", (a_pLeafReg) & (fMask))); \
        (a_pLeafReg) &= ~(uint32_t)(fMask); \
    }
#define PORTABLE_DISABLE_FEATURE_BIT(Lvl, a_pLeafReg, FeatNm, fBitMask) \
    if ( pCpum->u8PortableCpuIdLevel >= (Lvl) && ((a_pLeafReg) & (fBitMask)) ) \
    { \
        LogRel(("PortableCpuId: " #a_pLeafReg "[" #FeatNm "]: 1 -> 0\n")); \
        (a_pLeafReg) &= ~(uint32_t)(fBitMask); \
    }
#define PORTABLE_DISABLE_FEATURE_BIT_CFG(Lvl, a_IdReg, a_FeatNm, a_IdRegValCheck, enmConfig, a_IdRegValNotSup) \
    if (   pCpum->u8PortableCpuIdLevel >= (Lvl) \
        && (RT_BF_GET(a_IdReg, a_FeatNm) >= a_IdRegValCheck) \
        && (enmConfig) != CPUMISAEXTCFG_ENABLED_PORTABLE ) \
    { \
        LogRel(("PortableCpuId: [" #a_FeatNm "]: 1 -> 0\n")); \
        (a_IdReg) = RT_BF_SET(a_IdReg, a_FeatNm, a_IdRegValNotSup); \
    }
    //Assert(pCpum->GuestFeatures.enmCpuVendor != CPUMCPUVENDOR_INVALID);

    /* The CPUID entries we start with here isn't necessarily the ones of the host, so we
       must consult HostFeatures when processing CPUMISAEXTCFG variables. */
    PCCPUMFEATURES pHstFeat = &pCpum->HostFeatures;
#define PASSTHRU_FEATURE(a_IdReg, enmConfig, fHostFeature, a_IdRegNm, a_IdRegValSup, a_IdRegValNotSup) \
    (a_IdReg) =   ((enmConfig) && ((enmConfig) == CPUMISAEXTCFG_ENABLED_ALWAYS || (fHostFeature)) \
                ? RT_BF_SET(a_IdReg, a_IdRegNm, a_IdRegValSup) \
                : RT_BF_SET(a_IdReg, a_IdRegNm, a_IdRegValNotSup))

    /* ID_AA64ISAR0_EL1 */
    uint64_t u64ValTmp = 0;
    uint64_t u64IdReg = pVM->cpum.s.GuestIdRegs.u64RegIdAa64Isar0El1;

    PASSTHRU_FEATURE(u64IdReg, pConfig->enmAes,     pHstFeat->fAes,     ARMV8_ID_AA64ISAR0_EL1_AES,     ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED,                   ARMV8_ID_AA64ISAR0_EL1_AES_NOT_IMPL);
    u64ValTmp = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_AES) == ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED ? ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED : ARMV8_ID_AA64ISAR0_EL1_AES_NOT_IMPL;
    PASSTHRU_FEATURE(u64IdReg, pConfig->enmPmull,   pHstFeat->fPmull,   ARMV8_ID_AA64ISAR0_EL1_AES,     ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED_PMULL,             u64ValTmp);
    PASSTHRU_FEATURE(u64IdReg, pConfig->enmSha1,    pHstFeat->fSha1,    ARMV8_ID_AA64ISAR0_EL1_SHA1,    ARMV8_ID_AA64ISAR0_EL1_SHA1_SUPPORTED,                  ARMV8_ID_AA64ISAR0_EL1_SHA1_NOT_IMPL);
    PASSTHRU_FEATURE(u64IdReg, pConfig->enmSha256,  pHstFeat->fSha256,  ARMV8_ID_AA64ISAR0_EL1_SHA2,    ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256,           ARMV8_ID_AA64ISAR0_EL1_SHA2_NOT_IMPL);
    u64ValTmp = RT_BF_GET(u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA2) == ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256 ? ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256 : ARMV8_ID_AA64ISAR0_EL1_SHA2_NOT_IMPL;
    PASSTHRU_FEATURE(u64IdReg, pConfig->enmSha512,  pHstFeat->fSha512,  ARMV8_ID_AA64ISAR0_EL1_SHA2,    ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256_SHA512,    u64ValTmp);
    PASSTHRU_FEATURE(u64IdReg, pConfig->enmCrc32,   pHstFeat->fCrc32,   ARMV8_ID_AA64ISAR0_EL1_CRC32,   ARMV8_ID_AA64ISAR0_EL1_CRC32_SUPPORTED,                 ARMV8_ID_AA64ISAR0_EL1_CRC32_NOT_IMPL);
    PASSTHRU_FEATURE(u64IdReg, pConfig->enmSha3,    pHstFeat->fSha3,    ARMV8_ID_AA64ISAR0_EL1_SHA3,    ARMV8_ID_AA64ISAR0_EL1_SHA3_SUPPORTED,                  ARMV8_ID_AA64ISAR0_EL1_SHA3_NOT_IMPL);

    if (pCpum->u8PortableCpuIdLevel > 0)
    {
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_AES,   ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED,                   pConfig->enmAes,    ARMV8_ID_AA64ISAR0_EL1_AES_NOT_IMPL);
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_AES,   ARMV8_ID_AA64ISAR0_EL1_AES_SUPPORTED_PMULL,             pConfig->enmPmull,  ARMV8_ID_AA64ISAR0_EL1_AES_NOT_IMPL);
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA1,  ARMV8_ID_AA64ISAR0_EL1_SHA1_SUPPORTED,                  pConfig->enmSha1,   ARMV8_ID_AA64ISAR0_EL1_SHA1_NOT_IMPL);
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA2,  ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256,           pConfig->enmSha256, ARMV8_ID_AA64ISAR0_EL1_SHA2_NOT_IMPL);
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA2,  ARMV8_ID_AA64ISAR0_EL1_SHA2_SUPPORTED_SHA256_SHA512,    pConfig->enmSha512, ARMV8_ID_AA64ISAR0_EL1_SHA2_NOT_IMPL);
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_CRC32, ARMV8_ID_AA64ISAR0_EL1_CRC32_SUPPORTED,                 pConfig->enmCrc32,  ARMV8_ID_AA64ISAR0_EL1_CRC32_NOT_IMPL);
        PORTABLE_DISABLE_FEATURE_BIT_CFG(1, u64IdReg, ARMV8_ID_AA64ISAR0_EL1_SHA3,  ARMV8_ID_AA64ISAR0_EL1_SHA3_SUPPORTED,                  pConfig->enmSha3,   ARMV8_ID_AA64ISAR0_EL1_SHA3_NOT_IMPL);
    }

    /* Write ID_AA64ISAR0_EL1 register back. */
    pVM->cpum.s.GuestIdRegs.u64RegIdAa64Isar0El1 = u64IdReg;

    /** @todo Other ID and feature registers. */

    return VINF_SUCCESS;
#undef PORTABLE_DISABLE_FEATURE_BIT
#undef PORTABLE_CLEAR_BITS_WHEN
}


/**
 * Reads a value in /CPUM/IsaExts/ node.
 *
 * @returns VBox status code (error message raised).
 * @param   pVM             The cross context VM structure. (For errors.)
 * @param   pIsaExts        The /CPUM/IsaExts node (can be NULL).
 * @param   pszValueName    The value / extension name.
 * @param   penmValue       Where to return the choice.
 * @param   enmDefault      The default choice.
 */
static int cpumR3CpuIdReadIsaExtCfg(PVM pVM, PCFGMNODE pIsaExts, const char *pszValueName,
                                    CPUMISAEXTCFG *penmValue, CPUMISAEXTCFG enmDefault)
{
    /*
     * Try integer encoding first.
     */
    uint64_t uValue;
    int rc = CFGMR3QueryInteger(pIsaExts, pszValueName, &uValue);
    if (RT_SUCCESS(rc))
        switch (uValue)
        {
            case 0: *penmValue = CPUMISAEXTCFG_DISABLED; break;
            case 1: *penmValue = CPUMISAEXTCFG_ENABLED_SUPPORTED; break;
            case 2: *penmValue = CPUMISAEXTCFG_ENABLED_ALWAYS; break;
            case 9: *penmValue = CPUMISAEXTCFG_ENABLED_PORTABLE; break;
            default:
                return VMSetError(pVM, VERR_CPUM_INVALID_CONFIG_VALUE, RT_SRC_POS,
                                  "Invalid config value for '/CPUM/IsaExts/%s': %llu (expected 0/'disabled', 1/'enabled', 2/'portable', or 9/'forced')",
                                  pszValueName, uValue);
        }
    /*
     * If missing, use default.
     */
    else if (rc == VERR_CFGM_VALUE_NOT_FOUND || rc == VERR_CFGM_NO_PARENT)
        *penmValue = enmDefault;
    else
    {
        if (rc == VERR_CFGM_NOT_INTEGER)
        {
            /*
             * Not an integer, try read it as a string.
             */
            char szValue[32];
            rc = CFGMR3QueryString(pIsaExts, pszValueName, szValue, sizeof(szValue));
            if (RT_SUCCESS(rc))
            {
                RTStrToLower(szValue);
                size_t cchValue = strlen(szValue);
#define EQ(a_str) (cchValue == sizeof(a_str) - 1U && memcmp(szValue, a_str, sizeof(a_str) - 1))
                if (     EQ("disabled") || EQ("disable") || EQ("off") || EQ("no"))
                    *penmValue = CPUMISAEXTCFG_DISABLED;
                else if (EQ("enabled")  || EQ("enable")  || EQ("on")  || EQ("yes"))
                    *penmValue = CPUMISAEXTCFG_ENABLED_SUPPORTED;
                else if (EQ("forced")   || EQ("force")   || EQ("always"))
                    *penmValue = CPUMISAEXTCFG_ENABLED_ALWAYS;
                else if (EQ("portable"))
                    *penmValue = CPUMISAEXTCFG_ENABLED_PORTABLE;
                else if (EQ("default")  || EQ("def"))
                    *penmValue = enmDefault;
                else
                    return VMSetError(pVM, VERR_CPUM_INVALID_CONFIG_VALUE, RT_SRC_POS,
                                      "Invalid config value for '/CPUM/IsaExts/%s': '%s' (expected 0/'disabled', 1/'enabled', 2/'portable', or 9/'forced')",
                                      pszValueName, uValue);
#undef EQ
            }
        }
        if (RT_FAILURE(rc))
            return VMSetError(pVM, rc, RT_SRC_POS, "Error reading config value '/CPUM/IsaExts/%s': %Rrc", pszValueName, rc);
    }
    return VINF_SUCCESS;
}


#if 0
/**
 * Reads a value in /CPUM/IsaExts/ node, forcing it to DISABLED if wanted.
 *
 * @returns VBox status code (error message raised).
 * @param   pVM             The cross context VM structure. (For errors.)
 * @param   pIsaExts        The /CPUM/IsaExts node (can be NULL).
 * @param   pszValueName    The value / extension name.
 * @param   penmValue       Where to return the choice.
 * @param   enmDefault      The default choice.
 * @param   fAllowed        Allowed choice.  Applied both to the result and to
 *                          the default value.
 */
static int cpumR3CpuIdReadIsaExtCfgEx(PVM pVM, PCFGMNODE pIsaExts, const char *pszValueName,
                                      CPUMISAEXTCFG *penmValue, CPUMISAEXTCFG enmDefault, bool fAllowed)
{
    int rc;
    if (fAllowed)
        rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, pszValueName, penmValue, enmDefault);
    else
    {
        rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, pszValueName, penmValue, false /*enmDefault*/);
        if (RT_SUCCESS(rc) && *penmValue == CPUMISAEXTCFG_ENABLED_ALWAYS)
            LogRel(("CPUM: Ignoring forced '%s'\n", pszValueName));
        *penmValue = CPUMISAEXTCFG_DISABLED;
    }
    return rc;
}
#endif

static int cpumR3CpuIdReadConfig(PVM pVM, PCPUMCPUIDCONFIG pConfig, PCFGMNODE pCpumCfg)
{
    /** @cfgm{/CPUM/PortableCpuIdLevel, 8-bit, 0, 3, 0}
     * When non-zero CPUID features that could cause portability issues will be
     * stripped.  The higher the value the more features gets stripped.  Higher
     * values should only be used when older CPUs are involved since it may
     * harm performance and maybe also cause problems with specific guests. */
    int rc = CFGMR3QueryU8Def(pCpumCfg, "PortableCpuIdLevel", &pVM->cpum.s.u8PortableCpuIdLevel, 0);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/GuestCpuName, string}
     * The name of the CPU we're to emulate.  The default is the host CPU.
     * Note! CPUs other than "host" one is currently unsupported. */
    rc = CFGMR3QueryStringDef(pCpumCfg, "GuestCpuName", pConfig->szCpuName, sizeof(pConfig->szCpuName), "host");
    AssertLogRelRCReturn(rc, rc);

    /*
     * Instruction Set Architecture (ISA) Extensions.
     */
    PCFGMNODE pIsaExts = CFGMR3GetChild(pCpumCfg, "IsaExts");
    if (pIsaExts)
    {
        rc = CFGMR3ValidateConfig(pIsaExts, "/CPUM/IsaExts/",
                                   "AES"
                                   "|PMULL"
                                   "|SHA1"
                                   "|SHA256"
                                   "|SHA512"
                                   "|CRC32"
                                   "|SHA3"
                                  , "" /*pszValidNodes*/, "CPUM" /*pszWho*/, 0 /*uInstance*/);
        if (RT_FAILURE(rc))
            return rc;
    }

    /** @cfgm{/CPUM/IsaExts/AES, boolean, depends}
     * Expose FEAT_AES instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "AES", &pConfig->enmAes, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/IsaExts/AES, boolean, depends}
     * Expose FEAT_AES and FEAT_PMULL instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "PMULL", &pConfig->enmPmull, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/IsaExts/SHA1, boolean, depends}
     * Expose FEAT_SHA1 instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "SHA1", &pConfig->enmSha1, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/IsaExts/SHA256, boolean, depends}
     * Expose FEAT_SHA256 instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "SHA256", &pConfig->enmSha256, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/IsaExts/SHA512, boolean, depends}
     * Expose FEAT_SHA256 and FEAT_SHA512 instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "SHA512", &pConfig->enmSha512, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/IsaExts/CRC32, boolean, depends}
     * Expose FEAT_CRC32 instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "CRC32", &pConfig->enmCrc32, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    /** @cfgm{/CPUM/IsaExts/SHA3, boolean, depends}
     * Expose FEAT_SHA3 instruction set extension to the guest.
     */
    rc = cpumR3CpuIdReadIsaExtCfg(pVM, pIsaExts, "SHA3", &pConfig->enmSha3, true /*enmDefault*/);
    AssertLogRelRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * Populates the host and guest features by the given ID registers.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   pIdRegs             Pointer to the ID register struct.
 *
 * @note Unlike on x86 there is no cross platform usermode accessible way to get at the CPU features.
 *       On ARM there are some ID_AA64*_EL1 system registers accessible by EL1 and higher only so we have to
 *       rely on the host/NEM backend to query those and hand them to CPUM where they will be parsed and modified based
 *       on the VM config.
 */
VMMR3DECL(int) CPUMR3PopulateFeaturesByIdRegisters(PVM pVM, PCCPUMIDREGS pIdRegs)
{
    /* Set the host features from the given ID registers. */
    int rc = cpumCpuIdExplodeFeatures(pIdRegs, &g_CpumHostFeatures.s);
    AssertRCReturn(rc, rc);

    pVM->cpum.s.HostFeatures               = g_CpumHostFeatures.s;
    pVM->cpum.s.GuestFeatures.enmCpuVendor = pVM->cpum.s.HostFeatures.enmCpuVendor;
    pVM->cpum.s.HostIdRegs                 = *pIdRegs;
    pVM->cpum.s.GuestIdRegs                = *pIdRegs;

    PCPUM       pCpum    = &pVM->cpum.s;
    PCFGMNODE   pCpumCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM");

    /*
     * Read the configuration.
     */
    CPUMCPUIDCONFIG Config;
    RT_ZERO(Config);

    rc = cpumR3CpuIdReadConfig(pVM, &Config, pCpumCfg);
    AssertRCReturn(rc, rc);

#if 0
    /*
     * Get the guest CPU data from the database and/or the host.
     *
     * The CPUID and MSRs are currently living on the regular heap to avoid
     * fragmenting the hyper heap (and because there isn't/wasn't any realloc
     * API for the hyper heap).  This means special cleanup considerations.
     */
    rc = cpumR3DbGetCpuInfo(Config.szCpuName, &pCpum->GuestInfo);
    if (RT_FAILURE(rc))
        return rc == VERR_CPUM_DB_CPU_NOT_FOUND
             ? VMSetError(pVM, rc, RT_SRC_POS,
                          "Info on guest CPU '%s' could not be found. Please, select a different CPU.", Config.szCpuName)
             : rc;
#endif

    /*
     * Pre-explode the CPUID info.
     */
    if (RT_SUCCESS(rc))
        rc = cpumCpuIdExplodeFeatures(pIdRegs, &pCpum->GuestFeatures);

    /*
     * Sanitize the cpuid information passed on to the guest.
     */
    if (RT_SUCCESS(rc))
        rc = cpumR3CpuIdSanitize(pVM, pCpum, &Config);

    return rc;
}


/**
 * Queries the pointer to the VM wide ID registers exposing configured features to the guest.
 *
 * @returns VBox status code.
 * @param   pVM                 The cross context VM structure.
 * @param   ppIdRegs            Where to store the pointer to the guest ID register struct.
 */
VMMR3_INT_DECL(int) CPUMR3QueryGuestIdRegs(PVM pVM, PCCPUMIDREGS *ppIdRegs)
{
    AssertPtrReturn(ppIdRegs, VERR_INVALID_POINTER);

    *ppIdRegs = &pVM->cpum.s.GuestIdRegs;
    return VINF_SUCCESS;
}


/*
 *
 *
 * CPUID Info Handler.
 * CPUID Info Handler.
 * CPUID Info Handler.
 *
 *
 */


/** ID_AA64ISAR0_EL1 field descriptions.   */
static DBGFREGSUBFIELD const g_aIdAa64IsaR0Fields[] =
{
    DBGFREGSUBFIELD_RO("AES\0"       "AES instruction support in AArch64",                           4, 4, 0),
    DBGFREGSUBFIELD_RO("SHA1\0"      "SHA1 instruction support in AArch64",                          8, 4, 0),
    DBGFREGSUBFIELD_RO("SHA2\0"      "SHA256/512 instruction support in AArch64",                   12, 4, 0),
    DBGFREGSUBFIELD_RO("CRC32\0"     "CRC32 instruction support in AArch64",                        16, 4, 0),
    DBGFREGSUBFIELD_RO("ATOMIC\0"    "Atomic instruction support in AArch64",                       20, 4, 0),
    DBGFREGSUBFIELD_RO("TME\0"       "TME instruction support in AArch64",                          24, 4, 0),
    DBGFREGSUBFIELD_RO("RDM\0"       "SQRDMLAH/SQRDMLSH instruction support in AArch64",            28, 4, 0),
    DBGFREGSUBFIELD_RO("SHA3\0"      "SHA3 instruction support in AArch64",                         32, 4, 0),
    DBGFREGSUBFIELD_RO("SM3\0"       "SM3 instruction support in AArch64",                          36, 4, 0),
    DBGFREGSUBFIELD_RO("SM4\0"       "SM4 instruction support in AArch64",                          40, 4, 0),
    DBGFREGSUBFIELD_RO("DP\0"        "Dot Product instruction support in AArch64",                  44, 4, 0),
    DBGFREGSUBFIELD_RO("FHM\0"       "FMLAL/FMLSL instruction support in AArch64",                  48, 4, 0),
    DBGFREGSUBFIELD_RO("TS\0"        "Flag manipulation instruction support in AArch64",            52, 4, 0),
    DBGFREGSUBFIELD_RO("TLB\0"       "TLB maintenance instruction support in AArch64",              56, 4, 0),
    DBGFREGSUBFIELD_RO("RNDR\0"      "Random number instruction support in AArch64",                60, 4, 0),
    DBGFREGSUBFIELD_TERMINATOR()
};


/** ID_AA64ISAR1_EL1 field descriptions.   */
static DBGFREGSUBFIELD const g_aIdAa64IsaR1Fields[] =
{
    DBGFREGSUBFIELD_RO("DPB\0"       "Data Persistance writeback support in AArch64",                0, 4, 0),
    DBGFREGSUBFIELD_RO("APA\0"       "QARMA5 PAuth support in AArch64",                              4, 4, 0),
    DBGFREGSUBFIELD_RO("API\0"       "Impl defined PAuth support in AArch64",                        8, 4, 0),
    DBGFREGSUBFIELD_RO("JSCVT\0"     "FJCVTZS instruction support in AArch64",                      12, 4, 0),
    DBGFREGSUBFIELD_RO("FCMA\0"      "FCMLA/FCADD instruction support in AArch64",                  16, 4, 0),
    DBGFREGSUBFIELD_RO("LRCPC\0"     "RCpc instruction support in AArch64",                         20, 4, 0),
    DBGFREGSUBFIELD_RO("GPA\0"       "QARMA5 code authentication support in AArch64",               24, 4, 0),
    DBGFREGSUBFIELD_RO("GPI\0"       "Impl defined code authentication support in AArch64",         28, 4, 0),
    DBGFREGSUBFIELD_RO("FRINTTS\0"   "FRINT{32,64}{Z,X} instruction support in AArch64",            32, 4, 0),
    DBGFREGSUBFIELD_RO("SB\0"        "SB instruction support in AArch64",                           36, 4, 0),
    DBGFREGSUBFIELD_RO("SPECRES\0"   "Prediction invalidation support in AArch64",                  40, 4, 0),
    DBGFREGSUBFIELD_RO("BF16\0"      "BFloat16 support in AArch64",                                 44, 4, 0),
    DBGFREGSUBFIELD_RO("DGH\0"       "Data Gathering Hint support in AArch64",                      48, 4, 0),
    DBGFREGSUBFIELD_RO("I8MM\0"      "Int8 matrix mul instruction support in AArch64",              52, 4, 0),
    DBGFREGSUBFIELD_RO("XS\0"        "XS attribute support in AArch64",                             56, 4, 0),
    DBGFREGSUBFIELD_RO("LS64\0"      "LD64B and ST64B* instruction support in AArch64",             60, 4, 0),
    DBGFREGSUBFIELD_TERMINATOR()
};


/** ID_AA64ISAR2_EL1 field descriptions.   */
static DBGFREGSUBFIELD const g_aIdAa64IsaR2Fields[] =
{
    DBGFREGSUBFIELD_RO("WFxT\0"      "WFET/WFIT intruction support in AArch64",                      0, 4, 0),
    DBGFREGSUBFIELD_RO("RPRES\0"     "Reciprocal 12 bit mantissa support in AArch64",                4, 4, 0),
    DBGFREGSUBFIELD_RO("GPA3\0"      "QARMA3 code authentication support in AArch64",                8, 4, 0),
    DBGFREGSUBFIELD_RO("APA3\0"      "QARMA3 PAuth support in AArch64",                             12, 4, 0),
    DBGFREGSUBFIELD_RO("MOPS\0"      "Memory Copy and Set instruction support in AArch64",          16, 4, 0),
    DBGFREGSUBFIELD_RO("BC\0"        "BC instruction support in AArch64",                           20, 4, 0),
    DBGFREGSUBFIELD_RO("PAC_frac\0"  "ConstPACField() returns TRUE",                                24, 4, 0),
    DBGFREGSUBFIELD_TERMINATOR()
};


/** ID_AA64MMFR0_EL1 field descriptions.   */
static DBGFREGSUBFIELD const g_aIdAa64MmfR0Fields[] =
{
    DBGFREGSUBFIELD_RO("PARange\0"   "Physical address width",                                       0, 4, 0),
    DBGFREGSUBFIELD_RO("ASIDBits\0"  "Number of ASID bits",                                          4, 4, 0),
    DBGFREGSUBFIELD_RO("BigEnd\0"    "Mixed-endian configuration support",                           8, 4, 0),
    DBGFREGSUBFIELD_RO("SNSMem\0"    "Secure and Non-secure memory distinction",                    12, 4, 0),
    DBGFREGSUBFIELD_RO("BigEndEL0\0" "Mixed-endian support in EL0 only",                            16, 4, 0),
    DBGFREGSUBFIELD_RO("TGran16\0"   "16KiB memory granule size",                                   20, 4, 0),
    DBGFREGSUBFIELD_RO("TGran64\0"   "64KiB memory granule size",                                   24, 4, 0),
    DBGFREGSUBFIELD_RO("TGran4\0"    "4KiB memory granule size",                                    28, 4, 0),
    DBGFREGSUBFIELD_RO("TGran16_2\0" "16KiB memory granule size at stage 2",                        32, 4, 0),
    DBGFREGSUBFIELD_RO("TGran64_2\0" "64KiB memory granule size at stage 2",                        36, 4, 0),
    DBGFREGSUBFIELD_RO("TGran4_2\0"  "4KiB memory granule size at stage 2",                         40, 4, 0),
    DBGFREGSUBFIELD_RO("ExS\0"       "Disabling context synchronizing exception",                   44, 4, 0),
    DBGFREGSUBFIELD_RO("Res0\0"      "Reserved",                                                    48, 4, 0),
    DBGFREGSUBFIELD_RO("Res0\0"      "Reserved",                                                    52, 4, 0),
    DBGFREGSUBFIELD_RO("FGT\0"       "Fine-grained trap controls support",                          56, 4, 0),
    DBGFREGSUBFIELD_RO("ECV\0"       "Enhanced Counter Virtualization support",                      60, 4, 0),
    DBGFREGSUBFIELD_TERMINATOR()
};


/** ID_AA64MMFR1_EL1 field descriptions.   */
static DBGFREGSUBFIELD const g_aIdAa64MmfR1Fields[] =
{
    DBGFREGSUBFIELD_RO("HAFDBS\0"    "Hardware updates to Access/Dirty state",                       0, 4, 0),
    DBGFREGSUBFIELD_RO("VMIDBit\0"   "Number of VMID bits",                                          4, 4, 0),
    DBGFREGSUBFIELD_RO("VH\0"        "Virtualization Host Extensions",                               8, 4, 0),
    DBGFREGSUBFIELD_RO("HPDS\0"      "Hierarchical Permission Disables",                            12, 4, 0),
    DBGFREGSUBFIELD_RO("LO\0"        "LORegions support",                                           16, 4, 0),
    DBGFREGSUBFIELD_RO("PAN\0"       "Privileged Access Never",                                     20, 4, 0),
    DBGFREGSUBFIELD_RO("SpecSEI\0"   "SError interrupt exception for speculative reads",            24, 4, 0),
    DBGFREGSUBFIELD_RO("XNX\0"       "Execute-never control support",                               28, 4, 0),
    DBGFREGSUBFIELD_RO("TWED\0"      "Configurable delayed WFE trapping",                           32, 4, 0),
    DBGFREGSUBFIELD_RO("ETS\0"       "Enhanced Translation Synchronization support",                36, 4, 0),
    DBGFREGSUBFIELD_RO("HCX\0"       "HCRX_EL2 support",                                            40, 4, 0),
    DBGFREGSUBFIELD_RO("AFP\0"       "FPCR.{AH,FIZ,NEP} support",                                   44, 4, 0),
    DBGFREGSUBFIELD_RO("nTLBPA\0"    "Caching of translation table walks",                          48, 4, 0),
    DBGFREGSUBFIELD_RO("TIDCP1\0"    "FEAT_TIDCP1 support",                                         52, 4, 0),
    DBGFREGSUBFIELD_RO("CMOW\0"      "Cache maintenance instruction permission",                    56, 4, 0),
    DBGFREGSUBFIELD_RO("Res0\0"      "Reserved",                                                    60, 4, 0),
    DBGFREGSUBFIELD_TERMINATOR()
};


/** ID_AA64MMFR2_EL1 field descriptions.   */
static DBGFREGSUBFIELD const g_aIdAa64MmfR2Fields[] =
{
    DBGFREGSUBFIELD_RO("CnP\0"       "Common not Private translation support",                       0, 4, 0),
    DBGFREGSUBFIELD_RO("UAO\0"       "User Access Override",                                         4, 4, 0),
    DBGFREGSUBFIELD_RO("LSM\0"       "LSMAOE/nTLSMD bit support",                                    8, 4, 0),
    DBGFREGSUBFIELD_RO("IESB\0"      "IESB bit support in SCTLR_ELx",                               12, 4, 0),
    DBGFREGSUBFIELD_RO("VARange\0"   "Large virtual address space support",                         16, 4, 0),
    DBGFREGSUBFIELD_RO("CCIDX\0"     "64-bit CCSIDR_EL1 format",                                    20, 4, 0),
    DBGFREGSUBFIELD_RO("NV\0"        "Nested Virtualization support",                               24, 4, 0),
    DBGFREGSUBFIELD_RO("ST\0"        "Small translation table support",                             28, 4, 0),
    DBGFREGSUBFIELD_RO("AT\0"        "Unaligned single-copy atomicity support",                     32, 4, 0),
    DBGFREGSUBFIELD_RO("IDS\0"       "FEAT_IDST support",                                           36, 4, 0),
    DBGFREGSUBFIELD_RO("FWB\0"       "HCR_EL2.FWB support",                                         40, 4, 0),
    DBGFREGSUBFIELD_RO("Res0\0"      "Reserved",                                                    44, 4, 0),
    DBGFREGSUBFIELD_RO("TTL\0"       "TTL field support in address operations",                     48, 4, 0),
    DBGFREGSUBFIELD_RO("BBM\0"       "FEAT_BBM support",                                            52, 4, 0),
    DBGFREGSUBFIELD_RO("EVT\0"       "Enhanced Virtualization Traps support",                       56, 4, 0),
    DBGFREGSUBFIELD_RO("E0PD\0"      "E0PD mechanism support",                                      60, 4, 0),
    DBGFREGSUBFIELD_TERMINATOR()
};


static void cpumR3CpuIdInfoMnemonicListU64(PCDBGFINFOHLP pHlp, uint64_t uVal, PCDBGFREGSUBFIELD pDesc,
                                           const char *pszLeadIn, uint32_t cchWidth)
{
    if (pszLeadIn)
        pHlp->pfnPrintf(pHlp, "%*s", cchWidth, pszLeadIn);

    for (uint32_t iBit = 0; iBit < 64; iBit++)
        if (RT_BIT_64(iBit) & uVal)
        {
            while (   pDesc->pszName != NULL
                   && iBit >= (uint32_t)pDesc->iFirstBit + pDesc->cBits)
                pDesc++;
            if (   pDesc->pszName != NULL
                && iBit - (uint32_t)pDesc->iFirstBit < (uint32_t)pDesc->cBits)
            {
                if (pDesc->cBits == 1)
                    pHlp->pfnPrintf(pHlp, " %s", pDesc->pszName);
                else
                {
                    uint64_t uFieldValue = uVal >> pDesc->iFirstBit;
                    if (pDesc->cBits < 64)
                        uFieldValue &= RT_BIT_64(pDesc->cBits) - UINT64_C(1);
                    pHlp->pfnPrintf(pHlp, pDesc->cBits < 4 ? " %s=%llu" : " %s=%#llx", pDesc->pszName, uFieldValue);
                    iBit = pDesc->iFirstBit + pDesc->cBits - 1;
                }
            }
            else
                pHlp->pfnPrintf(pHlp, " %u", iBit);
        }
    if (pszLeadIn)
        pHlp->pfnPrintf(pHlp, "\n");
}


static void cpumR3CpuIdInfoVerboseCompareListU64(PCDBGFINFOHLP pHlp, uint64_t uVal1, uint64_t uVal2, PCDBGFREGSUBFIELD pDesc,
                                                 uint32_t cchWidth)
{
    uint32_t uCombined = uVal1 | uVal2;
    for (uint32_t iBit = 0; iBit < 64; iBit++)
        if (   (RT_BIT_64(iBit) & uCombined)
            || (iBit == pDesc->iFirstBit && pDesc->pszName) )
        {
            while (   pDesc->pszName != NULL
                   && iBit >= (uint32_t)pDesc->iFirstBit + pDesc->cBits)
                pDesc++;

            if (   pDesc->pszName != NULL
                && iBit - (uint32_t)pDesc->iFirstBit < (uint32_t)pDesc->cBits)
            {
                size_t      cchMnemonic  = strlen(pDesc->pszName);
                const char *pszDesc      = pDesc->pszName + cchMnemonic + 1;
                size_t      cchDesc      = strlen(pszDesc);
                uint32_t    uFieldValue1 = uVal1 >> pDesc->iFirstBit;
                uint32_t    uFieldValue2 = uVal2 >> pDesc->iFirstBit;
                if (pDesc->cBits < 64)
                {
                    uFieldValue1 &= RT_BIT_64(pDesc->cBits) - UINT64_C(1);
                    uFieldValue2 &= RT_BIT_64(pDesc->cBits) - UINT64_C(1);
                }

                pHlp->pfnPrintf(pHlp,  pDesc->cBits < 5 ? "  %s - %s%*s= %u (%u)\n" : "  %s - %s%*s= %#x (%#x)\n",
                                pDesc->pszName, pszDesc,
                                cchMnemonic + 3 + cchDesc < cchWidth ? cchWidth - (cchMnemonic + 3 + cchDesc) : 1, "",
                                uFieldValue1, uFieldValue2);

                iBit = pDesc->iFirstBit + pDesc->cBits - 1U;
                pDesc++;
            }
            else
                pHlp->pfnPrintf(pHlp, "  %2u - Reserved%*s= %u (%u)\n", iBit, 13 < cchWidth ? cchWidth - 13 : 1, "",
                                RT_BOOL(uVal1 & RT_BIT_64(iBit)), RT_BOOL(uVal2 & RT_BIT_64(iBit)));
        }
}


/**
 * Produces a detailed summary of standard leaf 0x00000007.
 *
 * @param   pHlp            The info helper functions.
 * @param   pszIdReg        The ID register name.
 * @param   u64IdRegHost    The host value of the ID register.
 * @param   u64IdRegGuest   The guest value of the ID register.
 * @param   pDesc           The field descriptor.
 * @param   fVerbose        Whether to be very verbose or not.
 */
static void cpumR3CpuIdInfoIdRegDetails(PCDBGFINFOHLP pHlp, const char *pszIdReg, uint64_t u64IdRegHost, uint64_t u64IdRegGuest,
                                        PCDBGFREGSUBFIELD pDesc, bool fVerbose)
{
    pHlp->pfnPrintf(pHlp, "ID register %s: Host %#RX64 Guest %#RX64\n", pszIdReg, u64IdRegHost, u64IdRegGuest);
    if (fVerbose)
    {
        pHlp->pfnPrintf(pHlp, "  Mnemonic - Description                                  = guest (host)\n");
        cpumR3CpuIdInfoVerboseCompareListU64(pHlp, u64IdRegGuest, u64IdRegHost, pDesc, 56);
    }
    else
        cpumR3CpuIdInfoMnemonicListU64(pHlp, u64IdRegGuest, pDesc, "Features:", 36);
}


/**
 * Display the guest CpuId leaves.
 *
 * @param   pVM         The cross context VM structure.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     "terse", "default" or "verbose".
 */
DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    /*
     * Parse the argument.
     */
    unsigned iVerbosity = 1;
    if (pszArgs)
    {
        pszArgs = RTStrStripL(pszArgs);
        if (!strcmp(pszArgs, "terse"))
            iVerbosity--;
        else if (!strcmp(pszArgs, "verbose"))
            iVerbosity++;
    }

    cpumR3CpuIdInfoIdRegDetails(pHlp, "ID_AA64ISAR0_EL1",
                                pVM->cpum.s.HostIdRegs.u64RegIdAa64Isar0El1,
                                pVM->cpum.s.GuestIdRegs.u64RegIdAa64Isar0El1,
                                g_aIdAa64IsaR0Fields, iVerbosity > 1);

    cpumR3CpuIdInfoIdRegDetails(pHlp, "ID_AA64ISAR1_EL1",
                                pVM->cpum.s.HostIdRegs.u64RegIdAa64Isar1El1,
                                pVM->cpum.s.GuestIdRegs.u64RegIdAa64Isar1El1,
                                g_aIdAa64IsaR1Fields, iVerbosity > 1);

    cpumR3CpuIdInfoIdRegDetails(pHlp, "ID_AA64ISAR2_EL1",
                                pVM->cpum.s.HostIdRegs.u64RegIdAa64Isar2El1,
                                pVM->cpum.s.GuestIdRegs.u64RegIdAa64Isar2El1,
                                g_aIdAa64IsaR2Fields, iVerbosity > 1);

    cpumR3CpuIdInfoIdRegDetails(pHlp, "ID_AA64MMFR0_EL1",
                                pVM->cpum.s.HostIdRegs.u64RegIdAa64Mmfr0El1,
                                pVM->cpum.s.GuestIdRegs.u64RegIdAa64Mmfr0El1,
                                g_aIdAa64MmfR0Fields, iVerbosity > 1);

    cpumR3CpuIdInfoIdRegDetails(pHlp, "ID_AA64MMFR1_EL1",
                                pVM->cpum.s.HostIdRegs.u64RegIdAa64Mmfr1El1,
                                pVM->cpum.s.GuestIdRegs.u64RegIdAa64Mmfr1El1,
                                g_aIdAa64MmfR1Fields, iVerbosity > 1);

    cpumR3CpuIdInfoIdRegDetails(pHlp, "ID_AA64MMFR2_EL1",
                                pVM->cpum.s.HostIdRegs.u64RegIdAa64Mmfr2El1,
                                pVM->cpum.s.GuestIdRegs.u64RegIdAa64Mmfr2El1,
                                g_aIdAa64MmfR2Fields, iVerbosity > 1);

    /** @todo Other ID and feature registers. */
}
