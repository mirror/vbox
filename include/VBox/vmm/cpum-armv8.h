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

/** @defgroup grp_cpum      The CPU Monitor / Manager API
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

    /** Padding to the required size to match CPUMFEATURES for x86/amd64. */
    uint8_t         abPadding[48 - 10];
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
DECLINLINE(bool) CPUMIsGuestIn64BitCodeEx(PCPUMCTX pCtx)
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

