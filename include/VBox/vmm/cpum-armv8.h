/** @file
 * CPUM - CPU Monitor(/ Manager).
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

#ifndef VBOX_INCLUDED_vmm_cpum_armv8_h
#define VBOX_INCLUDED_vmm_cpum_armv8_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/types.h>
#include <iprt/armv8.h>


RT_C_DECLS_BEGIN

/** @defgroup grp_cpum_armv8      The CPU Monitor / Manager API
 * @ingroup grp_vmm
 * @{
 */


/**
 * System register read functions.
 */
typedef enum CPUMSYSREGRDFN
{
    /** Invalid zero value. */
    kCpumSysRegRdFn_Invalid = 0,
    /** Return the CPUMMSRRANGE::uValue. */
    kCpumSysRegRdFn_FixedValue,
    /** Alias to the system register range starting at the system register given by
     * CPUMSYSREGRANGE::uValue.  Must be used in pair with
     * kCpumSysRegWrFn_Alias. */
    kCpumSysRegRdFn_Alias,
    /** Write only register, all read attempts cause an exception. */
    kCpumSysRegRdFn_WriteOnly,

    /** Read from a GICv3 PE ICC system register. */
    kCpumSysRegRdFn_GicV3Icc,
    /** Read from the OSLSR_EL1 syste register. */
    kCpumSysRegRdFn_OslsrEl1,

    /** End of valid system register read function indexes. */
    kCpumSysRegRdFn_End
} CPUMSYSREGRDFN;


/**
 * System register write functions.
 */
typedef enum CPUMSYSREGWRFN
{
    /** Invalid zero value. */
    kCpumSysRegWrFn_Invalid = 0,
    /** Writes are ignored. */
    kCpumSysRegWrFn_IgnoreWrite,
    /** Writes cause an exception. */
    kCpumSysRegWrFn_ReadOnly,
    /** Alias to the system register range starting at the system register given by
     * CPUMSYSREGRANGE::uValue.  Must be used in pair with
     * kCpumSysRegRdFn_Alias. */
    kCpumSysRegWrFn_Alias,

    /** Write to a GICv3 PE ICC system register. */
    kCpumSysRegWrFn_GicV3Icc,
    /** Write to the OSLAR_EL1 syste register. */
    kCpumSysRegWrFn_OslarEl1,

    /** End of valid system register write function indexes. */
    kCpumSysRegWrFn_End
} CPUMSYSREGWRFN;


/**
 * System register range.
 *
 * @note This is very similar to how x86/amd64 MSRs are handled.
 */
typedef struct CPUMSYSREGRANGE
{
    /** The first system register. [0] */
    uint16_t    uFirst;
    /** The last system register. [2] */
    uint16_t    uLast;
    /** The read function (CPUMMSRRDFN). [4] */
    uint16_t    enmRdFn;
    /** The write function (CPUMMSRWRFN). [6] */
    uint16_t    enmWrFn;
    /** The offset of the 64-bit system register value relative to the start of CPUMCPU.
     * UINT16_MAX if not used by the read and write functions.  [8] */
    uint32_t    offCpumCpu : 24;
    /** Reserved for future hacks. [11] */
    uint32_t    fReserved : 8;
    /** Padding/Reserved. [12] */
    uint32_t    u32Padding;
    /** The init/read value. [16]
     * When enmRdFn is kCpumMsrRdFn_INIT_VALUE, this is the value returned on RDMSR.
     * offCpumCpu must be UINT16_MAX in that case, otherwise it must be a valid
     * offset into CPUM. */
    uint64_t    uValue;
    /** The bits to ignore when writing. [24]   */
    uint64_t    fWrIgnMask;
    /** The bits that will cause an exception when writing. [32]
     * This is always checked prior to calling the write function.  Using
     * UINT64_MAX effectively marks the MSR as read-only. */
    uint64_t    fWrExcpMask;
    /** The register name, if applicable. [32] */
    char        szName[56];

    /** The number of reads. */
    STAMCOUNTER cReads;
    /** The number of writes. */
    STAMCOUNTER cWrites;
    /** The number of times ignored bits were written. */
    STAMCOUNTER cIgnoredBits;
    /** The number of exceptions generated. */
    STAMCOUNTER cExcp;
} CPUMSYSREGRANGE;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMSYSREGRANGE, 128);
#endif
/** Pointer to an system register range. */
typedef CPUMSYSREGRANGE *PCPUMSYSREGRANGE;
/** Pointer to a const system register range. */
typedef CPUMSYSREGRANGE const *PCCPUMSYSREGRANGE;


/**
 * CPU features and quirks.
 * This is mostly exploded CPUID info.
 */
typedef struct CPUMFEATURES
{
    /** The CPU vendor (CPUMCPUVENDOR). */
    uint8_t         enmCpuVendor;
    /** The CPU family. */
    uint8_t         uFamily;
    /** The CPU model. */
    uint8_t         uModel;
    /** The CPU stepping. */
    uint8_t         uStepping;
    /** The microarchitecture. */
#ifndef VBOX_FOR_DTRACE_LIB
    CPUMMICROARCH   enmMicroarch;
#else
    uint32_t        enmMicroarch;
#endif
    /** The maximum physical address width of the CPU. */
    uint8_t         cMaxPhysAddrWidth;
    /** The maximum linear address width of the CPU. */
    uint8_t         cMaxLinearAddrWidth;

    /** @name Granule sizes supported.
     * @{ */
    /** 4KiB translation granule size supported. */
    uint32_t        fTGran4K : 1;
    /** 16KiB translation granule size supported. */
    uint32_t        fTGran16K : 1;
    /** 64KiB translation granule size supported. */
    uint32_t        fTGran64K : 1;
    /** @} */

    /** @name pre-2020 Architecture Extensions.
     * @{ */
    /** Supports Advanced SIMD Extension (FEAT_AdvSIMD). */
    uint32_t        fAdvSimd : 1;
    /** Supports Advanced SIMD AES instructions (FEAT_AES). */
    uint32_t        fAes : 1;
    /** Supports Advanced SIMD PMULL instructions (FEAT_PMULL). */
    uint32_t        fPmull : 1;
    /** Supports CP15Disable2 (FEAT_CP15DISABLE2). */
    uint32_t        fCp15Disable2 : 1;
    /** Supports Cache Speculation Variant 2 (FEAT_CSV2). */
    uint32_t        fCsv2 : 1;
    /** Supports Cache Speculation Variant 2, version 1.1 (FEAT_CSV2_1p1). */
    uint32_t        fCsv21p1 : 1;
    /** Supports Cache Speculation Variant 2, version 1.2 (FEAT_CSV2_1p2). */
    uint32_t        fCsv21p2 : 1;
    /** Supports Cache Speculation Variant 3 (FEAT_CSV3). */
    uint32_t        fCsv3 : 1;
    /** Supports Data Gahtering Hint (FEAT_DGH). */
    uint32_t        fDgh : 1;
    /** Supports Double Lock (FEAT_DoubleLock). */
    uint32_t        fDoubleLock : 1;
    /** Supports Enhanced Translation Synchronization (FEAT_ETS2). */
    uint32_t        fEts2 : 1;
    /** Supports Floating Point Extensions (FEAT_FP). */
    uint32_t        fFp : 1;
    /** Supports IVIPT Extensions (FEAT_IVIPT). */
    uint32_t        fIvipt : 1;
    /** Supports PC Sample-based Profiling Extension (FEAT_PCSRv8). */
    uint32_t        fPcsrV8 : 1;
    /** Supports Speculation Restrictions instructions (FEAT_SPECRES). */
    uint32_t        fSpecres : 1;
    /** Supports Reliability, Availability, and Serviceability (RAS) Extension (FEAT_RAS). */
    uint32_t        fRas : 1;
    /** Supports Speculation Barrier (FEAT_SB). */
    uint32_t        fSb : 1;
    /** Supports Advanced SIMD SHA1 instructions (FEAT_SHA1). */
    uint32_t        fSha1 : 1;
    /** Supports Advanced SIMD SHA256 instructions (FEAT_SHA256). */
    uint32_t        fSha256 : 1;
    /** Supports Speculation Store Bypass Safe (FEAT_SSBS). */
    uint32_t        fSsbs : 1;
    /** Supports MRS and MSR instructions for Speculation Store Bypass Safe version 2 (FEAT_SSBS2). */
    uint32_t        fSsbs2 : 1;
    /** Supports CRC32 instructions (FEAT_CRC32). */
    uint32_t        fCrc32 : 1;
    /** Supports Intermediate chacing of trnslation table walks (FEAT_nTLBPA). */
    uint32_t        fNTlbpa : 1;
    /** Supports debug with VHE (FEAT_Debugv8p1). */
    uint32_t        fDebugV8p1 : 1;
    /** Supports Hierarchical permission disables in translation tables (FEAT_HPDS). */
    uint32_t        fHpds : 1;
    /** Supports Limited ordering regions (FEAT_LOR). */
    uint32_t        fLor : 1;
    /** Supports Lare Systems Extensons (FEAT_LSE). */
    uint32_t        fLse : 1;
    /** Supports Privileged access never (FEAT_PAN). */
    uint32_t        fPan : 1;
    /** Supports Armv8.1 PMU extensions (FEAT_PMUv3p1). */
    uint32_t        fPmuV3p1 : 1;
    /** Supports Advanced SIMD rouding double multiply accumulate instructions (FEAT_RDM). */
    uint32_t        fRdm : 1;
    /** Supports hardware management of the Access flag and dirty state (FEAT_HAFDBS). */
    uint32_t        fHafdbs : 1;
    /** Supports Virtualization Host Extensions (FEAT_VHE). */
    uint32_t        fVhe : 1;
    /** Supports 16-bit VMID (FEAT_VMID16). */
    uint32_t        fVmid16 : 1;
    /** Supports AArch32 BFloat16 instructions (FEAT_AA32BF16). */
    uint32_t        fAa32Bf16 : 1;
    /** Supports AArch32 Hierarchical permission disables (FEAT_AA32HPD). */
    uint32_t        fAa32Hpd : 1;
    /** Supports AArch32 Int8 matrix multiplication instructions (FEAT_AA32I8MM). */
    uint32_t        fAa32I8mm : 1;
    /** Supports AT S1E1R and AT S1E1W instruction variants affected by PSTATE.PAN (FEAT_PAN2). */
    uint32_t        fPan2 : 1;
    /** Supports AArch64 BFloat16 instructions (FEAT_BF16). */
    uint32_t        fBf16 : 1;
    /** Supports DC CVADP instruction (FEAT_DPB2). */
    uint32_t        fDpb2 : 1;
    /** Supports DC VAP instruction (FEAT_DPB). */
    uint32_t        fDpb : 1;
    /** Supports Debug v8.2 (FEAT_Debugv8p2). */
    uint32_t        fDebugV8p2 : 1;
    /** Supports Advanced SIMD dot product instructions (FEAT_DotProd). */
    uint32_t        fDotProd : 1;
    /** Supports Enhanced Virtualization Traps (FEAT_EVT). */
    uint32_t        fEvt : 1;
    /** Supports Single precision Matrix Multiplication (FEAT_F32MM). */
    uint32_t        fF32mm : 1;
    /** Supports Double precision Matrix Multiplication (FEAT_F64MM). */
    uint32_t        fF64mm : 1;
    /** Supports Floating-point half precision multiplication instructions (FEAT_FHM). */
    uint32_t        fFhm : 1;
    /** Supports Half-precision floating point data processing (FEAT_FP16). */
    uint32_t        fFp16 : 1;
    /** Supports AArch64 Int8 matrix multiplication instructions (FEAT_I8MM). */
    uint32_t        fI8mm : 1;
    /** Supports Implicit Error Synchronization event (FEAT_IESB). */
    uint32_t        fIesb : 1;
    /** Supports Large PA and IPA support (FEAT_LPA). */
    uint32_t        fLpa : 1;
    /** Supports AArch32 Load/Store Multiple instructions atomicity and ordering controls (FEAT_LSMAOC). */
    uint32_t        fLsmaoc : 1;
    /** Supports Large VA support (FEAT_LVA). */
    uint32_t        fLva : 1;
    /** Supports Memory Partitioning and Monitoring Extension (FEAT_MPAM). */
    uint32_t        fMpam : 1;
    /** Supports PC Sample-based Profiling Extension, version 8.2 (FEAT_PCSRv8p2). */
    uint32_t        fPcsrV8p2 : 1;
    /** Supports Advanced SIMD SHA3 instructions (FEAT_SHA3). */
    uint32_t        fSha3 : 1;
    /** Supports Advanced SIMD SHA512 instructions (FEAT_SHA512). */
    uint32_t        fSha512 : 1;
    /** Supports Advanced SIMD SM3 instructions (FEAT_SM3). */
    uint32_t        fSm3 : 1;
    /** Supports Advanced SIMD SM4 instructions (FEAT_SM4). */
    uint32_t        fSm4 : 1;
    /** Supports Statistical Profiling Extension (FEAT_SPE). */
    uint32_t        fSpe : 1;
    /** Supports Scalable Vector Extension (FEAT_SVE). */
    uint32_t        fSve : 1;
    /** Supports Translation Table Common not private translations (FEAT_TTCNP). */
    uint32_t        fTtcnp : 1;
    /** Supports Hierarchical permission disables, version 2 (FEAT_HPDS2). */
    uint32_t        fHpds2 : 1;
    /** Supports Translation table stage 2 Unprivileged Execute-never (FEAT_XNX). */
    uint32_t        fXnx : 1;
    /** Supports Unprivileged Access Override control (FEAT_UAO). */
    uint32_t        fUao : 1;
    /** Supports VMID-aware PIPT instruction cache (FEAT_VPIPT). */
    uint32_t        fVpipt : 1;
    /** Supports Extended cache index (FEAT_CCIDX). */
    uint32_t        fCcidx : 1;
    /** Supports Floating-point complex number instructions (FEAT_FCMA). */
    uint32_t        fFcma : 1;
    /** Supports Debug over Powerdown (FEAT_DoPD). */
    uint32_t        fDopd : 1;
    /** Supports Enhanced pointer authentication (FEAT_EPAC). */
    uint32_t        fEpac : 1;
    /** Supports Faulting on AUT* instructions (FEAT_FPAC). */
    uint32_t        fFpac : 1;
    /** Supports Faulting on combined pointer euthentication instructions (FEAT_FPACCOMBINE). */
    uint32_t        fFpacCombine : 1;
    /** Supports JavaScript conversion instructions (FEAT_JSCVT). */
    uint32_t        fJscvt : 1;
    /** Supports Load-Acquire RCpc instructions (FEAT_LRCPC). */
    uint32_t        fLrcpc : 1;
    /** Supports Nexted Virtualization (FEAT_NV). */
    uint32_t        fNv : 1;
    /** Supports QARMA5 pointer authentication algorithm (FEAT_PACQARMA5). */
    uint32_t        fPacQarma5 : 1;
    /** Supports implementation defined pointer authentication algorithm (FEAT_PACIMP). */
    uint32_t        fPacImp : 1;
    /** Supports Pointer authentication (FEAT_PAuth). */
    uint32_t        fPAuth : 1;
    /** Supports Enhancements to pointer authentication (FEAT_PAuth2). */
    uint32_t        fPAuth2 : 1;
    /** Supports Statistical Profiling Extensions version 1.1 (FEAT_SPEv1p1). */
    uint32_t        fSpeV1p1 : 1;
    /** Supports Activity Monitor Extension, version 1 (FEAT_AMUv1). */
    uint32_t        fAmuV1 : 1;
    /** Supports Generic Counter Scaling (FEAT_CNTSC). */
    uint32_t        fCntsc : 1;
    /** Supports Debug v8.4 (FEAT_Debugv8p4). */
    uint32_t        fDebugV8p4 : 1;
    /** Supports Double Fault Extension (FEAT_DoubleFault). */
    uint32_t        fDoubleFault : 1;
    /** Supports Data Independent Timing instructions (FEAT_DIT). */
    uint32_t        fDit : 1;
    /** Supports Condition flag manipulation isntructions (FEAT_FlagM). */
    uint32_t        fFlagM : 1;
    /** Supports ID space trap handling (FEAT_IDST). */
    uint32_t        fIdst : 1;
    /** Supports Load-Acquire RCpc instructions version 2 (FEAT_LRCPC2). */
    uint32_t        fLrcpc2 : 1;
    /** Supports Large Sytem Extensions version 2 (FEAT_LSE2). */
    uint32_t        fLse2 : 1;
    /** Supports Enhanced nested virtualization support (FEAT_NV2). */
    uint32_t        fNv2 : 1;
    /** Supports Armv8.4 PMU Extensions (FEAT_PMUv3p4). */
    uint32_t        fPmuV3p4 : 1;
    /** Supports RAS Extension v1.1 (FEAT_RASv1p1). */
    uint32_t        fRasV1p1 : 1;
    /** Supports RAS Extension v1.1 System Architecture (FEAT_RASSAv1p1). */
    uint32_t        fRassaV1p1 : 1;
    /** Supports Stage 2 forced Write-Back (FEAT_S2FWB). */
    uint32_t        fS2Fwb : 1;
    /** Supports Secure El2 (FEAT_SEL2). */
    uint32_t        fSecEl2 : 1;
    /** Supports TLB invalidate instructions on Outer Shareable domain (FEAT_TLBIOS). */
    uint32_t        fTlbios : 1;
    /** Supports TLB invalidate range instructions (FEAT_TLBIRANGE). */
    uint32_t        fTlbirange : 1;
    /** Supports Self-hosted Trace Extensions (FEAT_TRF). */
    uint32_t        fTrf : 1;
    /** Supports Translation Table Level (FEAT_TTL). */
    uint32_t        fTtl : 1;
    /** Supports Translation table break-before-make levels (FEAT_BBM). */
    uint32_t        fBbm : 1;
    /** Supports Small translation tables (FEAT_TTST). */
    uint32_t        fTtst : 1;
    /** Supports Branch Target Identification (FEAT_BTI). */
    uint32_t        fBti : 1;
    /** Supports Enhancements to flag manipulation instructions (FEAT_FlagM2). */
    uint32_t        fFlagM2 : 1;
    /** Supports Context synchronization and exception handling (FEAT_ExS). */
    uint32_t        fExs : 1;
    /** Supports Preenting EL0 access to halves of address maps (FEAT_E0PD). */
    uint32_t        fE0Pd : 1;
    /** Supports Floating-point to integer instructions (FEAT_FRINTTS). */
    uint32_t        fFrintts : 1;
    /** Supports Guest translation granule size (FEAT_GTG). */
    uint32_t        fGtg : 1;
    /** Supports Instruction-only Memory Tagging Extension (FEAT_MTE). */
    uint32_t        fMte : 1;
    /** Supports memory Tagging Extension version 2 (FEAT_MTE2). */
    uint32_t        fMte2 : 1;
    /** Supports Armv8.5 PMU Extensions (FEAT_PMUv3p5). */
    uint32_t        fPmuV3p5 : 1;
    /** Supports Random number generator (FEAT_RNG). */
    uint32_t        fRng : 1;
    /** Supports AMU Extensions version 1.1 (FEAT_AMUv1p1). */
    uint32_t        fAmuV1p1 : 1;
    /** Supports Enhanced Counter Virtualization (FEAT_ECV). */
    uint32_t        fEcv : 1;
    /** Supports Fine Grain Traps (FEAT_FGT). */
    uint32_t        fFgt : 1;
    /** Supports Memory Partitioning and Monitoring version 0.1 (FEAT_MPAMv0p1). */
    uint32_t        fMpamV0p1 : 1;
    /** Supports Memory Partitioning and Monitoring version 1.1 (FEAT_MPAMv1p1). */
    uint32_t        fMpamV1p1 : 1;
    /** Supports Multi-threaded PMU Extensions (FEAT_MTPMU). */
    uint32_t        fMtPmu : 1;
    /** Supports Delayed Trapping of WFE (FEAT_TWED). */
    uint32_t        fTwed : 1;
    /** Supports Embbedded Trace Macrocell version 4 (FEAT_ETMv4). */
    uint32_t        fEtmV4 : 1;
    /** Supports Embbedded Trace Macrocell version 4.1 (FEAT_ETMv4p1). */
    uint32_t        fEtmV4p1 : 1;
    /** Supports Embbedded Trace Macrocell version 4.2 (FEAT_ETMv4p2). */
    uint32_t        fEtmV4p2 : 1;
    /** Supports Embbedded Trace Macrocell version 4.3 (FEAT_ETMv4p3). */
    uint32_t        fEtmV4p3 : 1;
    /** Supports Embbedded Trace Macrocell version 4.4 (FEAT_ETMv4p4). */
    uint32_t        fEtmV4p4 : 1;
    /** Supports Embbedded Trace Macrocell version 4.5 (FEAT_ETMv4p5). */
    uint32_t        fEtmV4p5 : 1;
    /** Supports Embbedded Trace Macrocell version 4.6 (FEAT_ETMv4p6). */
    uint32_t        fEtmV4p6 : 1;
    /** Supports Generic Interrupt Controller version 3 (FEAT_GICv3). */
    uint32_t        fGicV3 : 1;
    /** Supports Generic Interrupt Controller version 3.1 (FEAT_GICv3p1). */
    uint32_t        fGicV3p1 : 1;
    /** Supports Trapping Non-secure EL1 writes to ICV_DIR (FEAT_GICv3_TDIR). */
    uint32_t        fGicV3Tdir : 1;
    /** Supports Generic Interrupt Controller version 4 (FEAT_GICv4). */
    uint32_t        fGicV4 : 1;
    /** Supports Generic Interrupt Controller version 4.1 (FEAT_GICv4p1). */
    uint32_t        fGicV4p1 : 1;
    /** Supports PMU extension, version 3 (FEAT_PMUv3). */
    uint32_t        fPmuV3 : 1;
    /** Supports Embedded Trace Extension (FEAT_ETE). */
    uint32_t        fEte : 1;
    /** Supports Embedded Trace Extension, version 1.1 (FEAT_ETEv1p1). */
    uint32_t        fEteV1p1 : 1;
    /** Supports Embedded Trace Extension, version 1.2 (FEAT_ETEv1p2). */
    uint32_t        fEteV1p2 : 1;
    /** Supports Scalable Vector Extension version 2 (FEAT_SVE2). */
    uint32_t        fSve2 : 1;
    /** Supports Scalable Vector AES instructions (FEAT_SVE_AES). */
    uint32_t        fSveAes : 1;
    /** Supports Scalable Vector PMULL instructions (FEAT_SVE_PMULL128). */
    uint32_t        fSvePmull128 : 1;
    /** Supports Scalable Vector Bit Permutes instructions (FEAT_SVE_BitPerm). */
    uint32_t        fSveBitPerm : 1;
    /** Supports Scalable Vector SHA3 instructions (FEAT_SVE_SHA3). */
    uint32_t        fSveSha3 : 1;
    /** Supports Scalable Vector SM4 instructions (FEAT_SVE_SM4). */
    uint32_t        fSveSm4 : 1;
    /** Supports Transactional Memory Extension (FEAT_TME). */
    uint32_t        fTme : 1;
    /** Supports Trace Buffer Extension (FEAT_TRBE). */
    uint32_t        fTrbe : 1;
    /** Supports Scalable Matrix Extension (FEAT_SME). */
    uint32_t        fSme : 1;
    /** @} */

    /** @name 2020 Architecture Extensions.
     * @{ */
    /** Supports Alternate floating-point behavior (FEAT_AFP). */
    uint32_t        fAfp : 1;
    /** Supports HCRX_EL2 register (FEAT_HCX). */
    uint32_t        fHcx : 1;
    /** Supports Larger phsical address for 4KiB and 16KiB translation granules (FEAT_LPA2). */
    uint32_t        fLpa2 : 1;
    /** Supports 64 byte loads and stores without return (FEAT_LS64). */
    uint32_t        fLs64 : 1;
    /** Supports 64 byte stores with return (FEAT_LS64_V). */
    uint32_t        fLs64V : 1;
    /** Supports 64 byte EL0 stores with return (FEAT_LS64_ACCDATA). */
    uint32_t        fLs64Accdata : 1;
    /** Supports MTE Asymmetric Fault Handling (FEAT_MTE3). */
    uint32_t        fMte3 : 1;
    /** Supports SCTLR_ELx.EPAN (FEAT_PAN3). */
    uint32_t        fPan3 : 1;
    /** Supports Armv8.7 PMU extensions (FEAT_PMUv3p7). */
    uint32_t        fPmuV3p7 : 1;
    /** Supports Increased precision of Reciprocal Extimate and Reciprocal Square Root Estimate (FEAT_RPRES). */
    uint32_t        fRpres : 1;
    /** Supports Realm Management Extension (FEAT_RME). */
    uint32_t        fRme : 1;
    /** Supports Full A64 instruction set support in Streaming SVE mode (FEAT_SME_FA64). */
    uint32_t        fSmeFA64 : 1;
    /** Supports Double-precision floating-point outer product instructions (FEAT_SME_F64F64). */
    uint32_t        fSmeF64F64 : 1;
    /** Supports 16-bit to 64-bit integer widening outer product instructions (FEAT_SME_I16I64). */
    uint32_t        fSmeI16I64 : 1;
    /** Supports Statistical Profiling Extensions version 1.2 (FEAT_SPEv1p2). */
    uint32_t        fSpeV1p2 : 1;
    /** Supports AArch64 Extended BFloat16 instructions (FEAT_EBF16). */
    uint32_t        fEbf16 : 1;
    /** Supports WFE and WFI instructions with timeout (FEAT_WFxT). */
    uint32_t        fWfxt : 1;
    /** Supports XS attribute (FEAT_XS). */
    uint32_t        fXs : 1;
    /** Supports branch Record Buffer Extension (FEAT_BRBE). */
    uint32_t        fBrbe : 1;
    /** @} */

    /** @name 2021 Architecture Extensions.
     * @{ */
    /** Supports Control for cache maintenance permission (FEAT_CMOW). */
    uint32_t        fCmow : 1;
    /** Supports PAC algorithm enhancement (FEAT_CONSTPACFIELD). */
    uint32_t        fConstPacField : 1;
    /** Supports Debug v8.8 (FEAT_Debugv8p8). */
    uint32_t        fDebugV8p8 : 1;
    /** Supports Hinted conditional branches (FEAT_HBC). */
    uint32_t        fHbc : 1;
    /** Supports Setting of MDCR_EL2.HPMN to zero (FEAT_HPMN0). */
    uint32_t        fHpmn0 : 1;
    /** Supports Non-Maskable Interrupts (FEAT_NMI). */
    uint32_t        fNmi : 1;
    /** Supports GIC Non-Maskable Interrupts (FEAT_GICv3_NMI). */
    uint32_t        fGicV3Nmi : 1;
    /** Supports Standardization of memory operations (FEAT_MOPS). */
    uint32_t        fMops : 1;
    /** Supports Pointer authentication - QARMA3 algorithm (FEAT_PACQARMA3). */
    uint32_t        fPacQarma3 : 1;
    /** Supports Event counting threshold (FEAT_PMUv3_TH). */
    uint32_t        fPmuV3Th : 1;
    /** Supports Armv8.8 PMU extensions (FEAT_PMUv3p8). */
    uint32_t        fPmuV3p8 : 1;
    /** Supports 64-bit external interface to the Performance Monitors (FEAT_PMUv3_EXT64). */
    uint32_t        fPmuV3Ext64 : 1;
    /** Supports 32-bit external interface to the Performance Monitors (FEAT_PMUv3_EXT32). */
    uint32_t        fPmuV3Ext32 : 1;
    /** Supports External interface to the Performance Monitors (FEAT_PMUv3_EXT). */
    uint32_t        fPmuV3Ext : 1;
    /** Supports Trapping support for RNDR/RNDRRS (FEAT_RNG_TRAP). */
    uint32_t        fRngTrap : 1;
    /** Supports Statistical Profiling Extension version 1.3 (FEAT_SPEv1p3). */
    uint32_t        fSpeV1p3 : 1;
    /** Supports EL0 use of IMPLEMENTATION DEFINEd functionality (FEAT_TIDCP1). */
    uint32_t        fTidcp1 : 1;
    /** Supports Branch Record Buffer Extension version 1.1 (FEAT_BRBEv1p1). */
    uint32_t        fBrbeV1p1 : 1;
    /** @} */

    /** @name 2022 Architecture Extensions.
     * @{ */
    /** Supports Address Breakpoint Linking Extenions (FEAT_ABLE). */
    uint32_t        fAble : 1;
    /** Supports Asynchronous Device error exceptions (FEAT_ADERR). */
    uint32_t        fAderr : 1;
    /** Supports Memory Attribute Index Enhancement (FEAT_AIE). */
    uint32_t        fAie : 1;
    /** Supports Asynchronous Normal error exception (FEAT_ANERR). */
    uint32_t        fAnerr : 1;
    /** Supports Breakpoint Mismatch and Range Extension (FEAT_BWE). */
    uint32_t        fBwe : 1;
    /** Supports Clear Branch History instruction (FEAT_CLRBHB). */
    uint32_t        fClrBhb : 1;
    /** Supports Check Feature Status (FEAT_CHK). */
    uint32_t        fChk : 1;
    /** Supports Common Short Sequence Compression instructions (FEAT_CSSC). */
    uint32_t        fCssc : 1;
    /** Supports Cache Speculation Variant 2 version 3 (FEAT_CSV2_3). */
    uint32_t        fCsv2v3 : 1;
    /** Supports 128-bit Translation Tables, 56 bit PA (FEAT_D128). */
    uint32_t        fD128 : 1;
    /** Supports Debug v8.9 (FEAT_Debugv8p9). */
    uint32_t        fDebugV8p9 : 1;
    /** Supports Enhancements to the Double Fault Extension (FEAT_DoubleFault2). */
    uint32_t        fDoubleFault2 : 1;
    /** Supports Exception based Event Profiling (FEAT_EBEP). */
    uint32_t        fEbep : 1;
    /** Supports Exploitative control using branch history information (FEAT_ECBHB). */
    uint32_t        fEcBhb : 1;
    /** Supports for EDHSR (FEAT_EDHSR). */
    uint32_t        fEdhsr : 1;
    /** Supports Embedded Trace Extension version 1.3 (FEAT_ETEv1p3). */
    uint32_t        fEteV1p3 : 1;
    /** Supports Fine-grained traps 2 (FEAT_FGT2). */
    uint32_t        fFgt2 : 1;
    /** Supports Guarded Control Stack Extension (FEAT_GCS). */
    uint32_t        fGcs : 1;
    /** Supports Hardware managed Access Flag for Table descriptors (FEAT_HAFT). */
    uint32_t        fHaft : 1;
    /** Supports Instrumentation Extension (FEAT_ITE). */
    uint32_t        fIte : 1;
    /** Supports Load-Acquire RCpc instructions version 3 (FEAT_LRCPC3). */
    uint32_t        fLrcpc3 : 1;
    /** Supports 128-bit atomics (FEAT_LSE128). */
    uint32_t        fLse128 : 1;
    /** Supports 56-bit VA (FEAT_LVA3). */
    uint32_t        fLva3 : 1;
    /** Supports Memory Encryption Contexts (FEAT_MEC). */
    uint32_t        fMec : 1;
    /** Supports Enhanced Memory Tagging Extension (FEAT_MTE4). */
    uint32_t        fMte4 : 1;
    /** Supports Canoncial Tag checking for untagged memory (FEAT_MTE_CANONCIAL_TAGS). */
    uint32_t        fMteCanonicalTags : 1;
    /** Supports FAR_ELx on a Tag Check Fault (FEAT_MTE_TAGGED_FAR). */
    uint32_t        fMteTaggedFar : 1;
    /** Supports Store only Tag checking (FEAT_MTE_STORE_ONLY). */
    uint32_t        fMteStoreOnly : 1;
    /** Supports Memory tagging with Address tagging disabled (FEAT_MTE_NO_ADDRESS_TAGS). */
    uint32_t        fMteNoAddressTags : 1;
    /** Supports Memory tagging asymmetric faults (FEAT_MTE_ASYM_FAULT). */
    uint32_t        fMteAsymFault : 1;
    /** Supports Memory Tagging asynchronous faulting (FEAT_MTE_ASYNC). */
    uint32_t        fMteAsync : 1;
    /** Supports Allocation tag access permission (FEAT_MTE_PERM_S1). */
    uint32_t        fMtePermS1 : 1;
    /** Supports Armv8.9 PC Sample-based Profiling Extension (FEAT_PCSRv8p9). */
    uint32_t        fPcsrV8p9 : 1;
    /** Supports Permission model enhancements (FEAT_S1PIE). */
    uint32_t        fS1Pie : 1;
    /** Supports Permission model enhancements (FEAT_S2PIE). */
    uint32_t        fS2Pie : 1;
    /** Supports Permission model enhancements (FEAT_S1POE). */
    uint32_t        fS1Poe : 1;
    /** Supports Permission model enhancements (FEAT_S2POE). */
    uint32_t        fS2Poe : 1;
    /** Supports Physical Fault Address Registers (FEAT_PFAR). */
    uint32_t        fPfar : 1;
    /** Supports Armv8.9 PMU extensions (FEAT_PMUv3p9). */
    uint32_t        fPmuV3p9 : 1;
    /** Supports PMU event edge detection (FEAT_PMUv3_EDGE). */
    uint32_t        fPmuV3Edge : 1;
    /** Supports Fixed-function instruction counter (FEAT_PMUv3_ICNTR). */
    uint32_t        fPmuV3Icntr : 1;
    /** Supports PMU Snapshot Extension (FEAT_PMUv3_SS). */
    uint32_t        fPmuV3Ss : 1;
    /** Supports SLC traget for PRFM instructions (FEAT_PRFMSLC). */
    uint32_t        fPrfmSlc : 1;
    /** Supports RAS version 2 (FEAT_RASv2). */
    uint32_t        fRasV2 : 1;
    /** Supports RAS version 2 System Architecture (FEAT_RASSAv2). */
    uint32_t        fRasSaV2 : 1;
    /** Supports for Range Prefetch Memory instruction (FEAT_RPRFM). */
    uint32_t        fRprfm : 1;
    /** Supports extensions to SCTLR_ELx (FEAT_SCTLR2). */
    uint32_t        fSctlr2 : 1;
    /** Supports Synchronous Exception-based Event Profiling (FEAT_SEBEP). */
    uint32_t        fSebep : 1;
    /** Supports non-widening half-precision FP16 to FP16 arithmetic for SME2.1 (FEAT_SME_F16F16). */
    uint32_t        fSmeF16F16 : 1;
    /** Supports Scalable Matrix Extension version 2 (FEAT_SME2). */
    uint32_t        fSme2 : 1;
    /** Supports Scalable Matrix Extension version 2.1 (FEAT_SME2p1). */
    uint32_t        fSme2p1 : 1;
    /** Supports Enhanced speculation restriction instructions (FEAT_SPECRES2). */
    uint32_t        fSpecres2 : 1;
    /** Supports System Performance Monitors Extension (FEAT_SPMU). */
    uint32_t        fSpmu : 1;
    /** Supports Statistical profiling Extension version 1.4 (FEAT_SPEv1p4). */
    uint32_t        fSpeV1p4 : 1;
    /** Supports Call Return Branch Records (FEAT_SPE_CRR). */
    uint32_t        fSpeCrr : 1;
    /** Supports Data Source Filtering (FEAT_SPE_FDS). */
    uint32_t        fSpeFds : 1;
    /** Supports Scalable Vector Extension version SVE2.1 (FEAT_SVE2p1). */
    uint32_t        fSve2p1 : 1;
    /** Supports Non-widening BFloat16 to BFloat16 arithmetic for SVE (FEAT_SVE_B16B16). */
    uint32_t        fSveB16B16 : 1;
    /** Supports 128-bit System instructions (FEAT_SYSINSTR128). */
    uint32_t        fSysInstr128 : 1;
    /** Supports 128-bit System registers (FEAT_SYSREG128). */
    uint32_t        fSysReg128 : 1;
    /** Supports Extension to TCR_ELx (FEAT_TCR2). */
    uint32_t        fTcr2 : 1;
    /** Supports Translation Hardening Extension (FEAT_THE). */
    uint32_t        fThe : 1;
    /** Supports Trace Buffer external mode (FEAT_TRBE_EXT). */
    uint32_t        fTrbeExt : 1;
    /** Supports Trace Buffer MPAM extension (FEAT_TRBE_MPAM). */
    uint32_t        fTrbeMpam : 1;
    /** @} */

    /** Padding to the required size to match CPUMFEATURES for x86/amd64. */
    uint8_t         abPadding[6];
} CPUMFEATURES;
#ifndef VBOX_FOR_DTRACE_LIB
AssertCompileSize(CPUMFEATURES, 48);
#endif
/** Pointer to a CPU feature structure. */
typedef CPUMFEATURES *PCPUMFEATURES;
/** Pointer to a const CPU feature structure. */
typedef CPUMFEATURES const *PCCPUMFEATURES;

/**
 * Chameleon wrapper structure for the host CPU features.
 *
 * This is used for the globally readable g_CpumHostFeatures variable, which is
 * initialized once during VMMR0 load for ring-0 and during CPUMR3Init in
 * ring-3.  To reflect this immutability after load/init, we use this wrapper
 * structure to switch it between const and non-const depending on the context.
 * Only two files sees it as non-const (CPUMR0.cpp and CPUM.cpp).
 */
typedef struct CPUHOSTFEATURES
{
    CPUMFEATURES
#ifndef CPUM_WITH_NONCONST_HOST_FEATURES
    const
#endif
                    s;
} CPUHOSTFEATURES;
/** Pointer to a const host CPU feature structure. */
typedef CPUHOSTFEATURES const *PCCPUHOSTFEATURES;

/** Host CPU features.
 * @note In ring-3, only valid after CPUMR3Init.  In ring-0, valid after
 *       module init. */
extern CPUHOSTFEATURES g_CpumHostFeatures;


/**
 * CPU database entry.
 */
typedef struct CPUMDBENTRY
{
    /** The CPU name. */
    const char     *pszName;
    /** The full CPU name. */
    const char     *pszFullName;
    /** The CPU vendor (CPUMCPUVENDOR). */
    uint8_t         enmVendor;
    /** The CPU family. */
    uint8_t         uFamily;
    /** The CPU model. */
    uint8_t         uModel;
    /** The CPU stepping. */
    uint8_t         uStepping;
    /** The microarchitecture. */
    CPUMMICROARCH   enmMicroarch;
    /** Scalable bus frequency used for reporting other frequencies. */
    uint64_t        uScalableBusFreq;
    /** Flags - CPUMDB_F_XXX. */
    uint32_t        fFlags;
    /** The maximum physical address with of the CPU.  This should correspond to
     * the value in CPUID leaf 0x80000008 when present. */
    uint8_t         cMaxPhysAddrWidth;
} CPUMDBENTRY;
/** Pointer to a const CPU database entry. */
typedef CPUMDBENTRY const *PCCPUMDBENTRY;


/**
 * CPU ID registers.
 */
typedef struct CPUMIDREGS
{
    /** Content of the ID_AA64PFR0_EL1 register. */
    uint64_t        u64RegIdAa64Pfr0El1;
    /** Content of the ID_AA64PFR1_EL1 register. */
    uint64_t        u64RegIdAa64Pfr1El1;
    /** Content of the ID_AA64DFR0_EL1 register. */
    uint64_t        u64RegIdAa64Dfr0El1;
    /** Content of the ID_AA64DFR1_EL1 register. */
    uint64_t        u64RegIdAa64Dfr1El1;
    /** Content of the ID_AA64AFR0_EL1 register. */
    uint64_t        u64RegIdAa64Afr0El1;
    /** Content of the ID_AA64AFR1_EL1 register. */
    uint64_t        u64RegIdAa64Afr1El1;
    /** Content of the ID_AA64ISAR0_EL1 register. */
    uint64_t        u64RegIdAa64Isar0El1;
    /** Content of the ID_AA64ISAR1_EL1 register. */
    uint64_t        u64RegIdAa64Isar1El1;
    /** Content of the ID_AA64ISAR2_EL1 register. */
    uint64_t        u64RegIdAa64Isar2El1;
    /** Content of the ID_AA64MMFR0_EL1 register. */
    uint64_t        u64RegIdAa64Mmfr0El1;
    /** Content of the ID_AA64MMFR1_EL1 register. */
    uint64_t        u64RegIdAa64Mmfr1El1;
    /** Content of the ID_AA64MMFR2_EL1 register. */
    uint64_t        u64RegIdAa64Mmfr2El1;
    /** Content of the CLIDR_EL1 register. */
    uint64_t        u64RegClidrEl1;
    /** Content of the CTR_EL0 register. */
    uint64_t        u64RegCtrEl0;
    /** Content of the DCZID_EL0 register. */
    uint64_t        u64RegDczidEl0;
} CPUMIDREGS;
/** Pointer to CPU ID registers. */
typedef CPUMIDREGS *PCPUMIDREGS;
/** Pointer to a const CPU ID registers structure. */
typedef CPUMIDREGS const *PCCPUMIDREGS;


/** @name Changed flags.
 * These flags are used to keep track of which important register that
 * have been changed since last they were reset. The only one allowed
 * to clear them is REM!
 *
 * @todo This is obsolete, but remains as it will be refactored for coordinating
 *       IEM and NEM/HM later. Probably.
 * @{
 */
#define CPUM_CHANGED_GLOBAL_TLB_FLUSH           RT_BIT(0)
#define CPUM_CHANGED_ALL                        ( CPUM_CHANGED_GLOBAL_TLB_FLUSH )
/** @} */


#if !defined(IPRT_WITHOUT_NAMED_UNIONS_AND_STRUCTS) || defined(DOXYGEN_RUNNING)
/** @name Inlined Guest Getters and predicates Functions.
 * @{ */

/**
 * Tests if the guest is running in 64 bits mode or not.
 *
 * @returns true if in 64 bits mode, otherwise false.
 * @param   pCtx    Current CPU context.
 */
DECLINLINE(bool) CPUMIsGuestIn64BitCodeEx(PCCPUMCTX pCtx)
{
    return !RT_BOOL(pCtx->fPState & ARMV8_SPSR_EL2_AARCH64_M4);
}

/** @} */
#endif /* !IPRT_WITHOUT_NAMED_UNIONS_AND_STRUCTS || DOXYGEN_RUNNING */


#ifndef VBOX_FOR_DTRACE_LIB

#ifdef IN_RING3
/** @defgroup grp_cpum_armv8_r3    The CPUM ARMv8 ring-3 API
 * @{
 */

VMMR3DECL(int)          CPUMR3SysRegRangesInsert(PVM pVM, PCCPUMSYSREGRANGE pNewRange);
VMMR3DECL(int)          CPUMR3PopulateFeaturesByIdRegisters(PVM pVM, PCCPUMIDREGS pIdRegs);

VMMR3_INT_DECL(int)     CPUMR3QueryGuestIdRegs(PVM pVM, PCCPUMIDREGS *ppIdRegs);

/** @} */
#endif /* IN_RING3 */


/** @name Guest Register Getters.
 * @{ */
VMMDECL(bool)           CPUMGetGuestIrqMasked(PVMCPUCC pVCpu);
VMMDECL(bool)           CPUMGetGuestFiqMasked(PVMCPUCC pVCpu);
VMMDECL(VBOXSTRICTRC)   CPUMQueryGuestSysReg(PVMCPUCC pVCpu, uint32_t idSysReg, uint64_t *puValue);
/** @} */


/** @name Guest Register Setters.
 * @{ */
VMMDECL(VBOXSTRICTRC)   CPUMSetGuestSysReg(PVMCPUCC pVCpu, uint32_t idSysReg, uint64_t uValue);
/** @} */

#endif

/** @} */
RT_C_DECLS_END


#endif /* !VBOX_INCLUDED_vmm_cpum_armv8_h */

