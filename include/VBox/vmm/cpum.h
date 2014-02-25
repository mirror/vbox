/** @file
 * CPUM - CPU Monitor(/ Manager).
 */

/*
 * Copyright (C) 2006-2013 Oracle Corporation
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

#ifndef ___VBox_vmm_cpum_h
#define ___VBox_vmm_cpum_h

#include <iprt/x86.h>
#include <VBox/types.h>
#include <VBox/vmm/cpumctx.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_cpum      The CPU Monitor / Manager API
 * @{
 */

/**
 * CPUID feature to set or clear.
 */
typedef enum CPUMCPUIDFEATURE
{
    CPUMCPUIDFEATURE_INVALID = 0,
    /** The APIC feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_APIC,
    /** The sysenter/sysexit feature bit. (Std) */
    CPUMCPUIDFEATURE_SEP,
    /** The SYSCALL/SYSEXIT feature bit (64 bits mode only for Intel CPUs). (Ext) */
    CPUMCPUIDFEATURE_SYSCALL,
    /** The PAE feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_PAE,
    /** The NX feature bit. (Ext) */
    CPUMCPUIDFEATURE_NX,
    /** The LAHF/SAHF feature bit (64 bits mode only). (Ext) */
    CPUMCPUIDFEATURE_LAHF,
    /** The LONG MODE feature bit. (Ext) */
    CPUMCPUIDFEATURE_LONG_MODE,
    /** The PAT feature bit. (Std+Ext) */
    CPUMCPUIDFEATURE_PAT,
    /** The x2APIC  feature bit. (Std) */
    CPUMCPUIDFEATURE_X2APIC,
    /** The RDTSCP feature bit. (Ext) */
    CPUMCPUIDFEATURE_RDTSCP,
    /** The Hypervisor Present bit. (Std) */
    CPUMCPUIDFEATURE_HVP,
    /** 32bit hackishness. */
    CPUMCPUIDFEATURE_32BIT_HACK = 0x7fffffff
} CPUMCPUIDFEATURE;

/**
 * CPU Vendor.
 */
typedef enum CPUMCPUVENDOR
{
    CPUMCPUVENDOR_INVALID = 0,
    CPUMCPUVENDOR_INTEL,
    CPUMCPUVENDOR_AMD,
    CPUMCPUVENDOR_VIA,
    CPUMCPUVENDOR_CYRIX,
    CPUMCPUVENDOR_UNKNOWN,
    /** 32bit hackishness. */
    CPUMCPUVENDOR_32BIT_HACK = 0x7fffffff
} CPUMCPUVENDOR;


/**
 * X86 and AMD64 CPU microarchitectures and in processor generations.
 *
 * @remarks The separation here is sometimes a little bit too finely grained,
 *          and the differences is more like processor generation than micro
 *          arch.  This can be useful, so we'll provide functions for getting at
 *          more coarse grained info.
 */
typedef enum CPUMMICROARCH
{
    kCpumMicroarch_Invalid = 0,

    kCpumMicroarch_Intel_First,

    kCpumMicroarch_Intel_8086 = kCpumMicroarch_Intel_First,
    kCpumMicroarch_Intel_80186,
    kCpumMicroarch_Intel_80286,
    kCpumMicroarch_Intel_80386,
    kCpumMicroarch_Intel_80486,
    kCpumMicroarch_Intel_P5,

    kCpumMicroarch_Intel_P6_Core_Atom_First,
    kCpumMicroarch_Intel_P6 = kCpumMicroarch_Intel_P6_Core_Atom_First,
    kCpumMicroarch_Intel_P6_II,
    kCpumMicroarch_Intel_P6_III,

    kCpumMicroarch_Intel_P6_M_Banias,
    kCpumMicroarch_Intel_P6_M_Dothan,
    kCpumMicroarch_Intel_Core_Yonah,        /**< Core, also known as Enhanced Pentium M. */

    kCpumMicroarch_Intel_Core2_First,
    kCpumMicroarch_Intel_Core2_Merom = kCpumMicroarch_Intel_Core2_First,
    kCpumMicroarch_Intel_Core2_Penryn,

    kCpumMicroarch_Intel_Core7_First,
    kCpumMicroarch_Intel_Core7_Nehalem = kCpumMicroarch_Intel_Core7_First,
    kCpumMicroarch_Intel_Core7_Westmere,
    kCpumMicroarch_Intel_Core7_SandyBridge,
    kCpumMicroarch_Intel_Core7_IvyBridge,
    kCpumMicroarch_Intel_Core7_Haswell,
    kCpumMicroarch_Intel_Core7_Broadwell,
    kCpumMicroarch_Intel_Core7_Skylake,
    kCpumMicroarch_Intel_Core7_Cannonlake,
    kCpumMicroarch_Intel_Core7_End,

    kCpumMicroarch_Intel_Atom_First,
    kCpumMicroarch_Intel_Atom_Bonnell = kCpumMicroarch_Intel_Atom_First,
    kCpumMicroarch_Intel_Atom_Lincroft,     /**< Second generation bonnell (44nm). */
    kCpumMicroarch_Intel_Atom_Saltwell,     /**< 32nm shrink of Bonnell. */
    kCpumMicroarch_Intel_Atom_Silvermont,   /**< 22nm */
    kCpumMicroarch_Intel_Atom_Airmount,     /**< 14nm */
    kCpumMicroarch_Intel_Atom_Goldmont,     /**< 14nm */
    kCpumMicroarch_Intel_Atom_Unknown,
    kCpumMicroarch_Intel_Atom_End,

    kCpumMicroarch_Intel_P6_Core_Atom_End,

    kCpumMicroarch_Intel_NB_First,
    kCpumMicroarch_Intel_NB_Willamette = kCpumMicroarch_Intel_NB_First, /**< 180nm */
    kCpumMicroarch_Intel_NB_Northwood,      /**< 130nm */
    kCpumMicroarch_Intel_NB_Prescott,       /**< 90nm */
    kCpumMicroarch_Intel_NB_Prescott2M,     /**< 90nm */
    kCpumMicroarch_Intel_NB_CedarMill,      /**< 65nm */
    kCpumMicroarch_Intel_NB_Gallatin,       /**< 90nm Xeon, Pentium 4 Extreme Edition ("Emergency Edition"). */
    kCpumMicroarch_Intel_NB_Unknown,
    kCpumMicroarch_Intel_NB_End,

    kCpumMicroarch_Intel_Unknown,
    kCpumMicroarch_Intel_End,

    kCpumMicroarch_AMD_First,
    kCpumMicroarch_AMD_Am286 = kCpumMicroarch_AMD_First,
    kCpumMicroarch_AMD_Am386,
    kCpumMicroarch_AMD_Am486,
    kCpumMicroarch_AMD_Am486Enh,            /**< Covers Am5x86 as well. */
    kCpumMicroarch_AMD_K5,
    kCpumMicroarch_AMD_K6,

    kCpumMicroarch_AMD_K7_First,
    kCpumMicroarch_AMD_K7_Palomino = kCpumMicroarch_AMD_K7_First,
    kCpumMicroarch_AMD_K7_Spitfire,
    kCpumMicroarch_AMD_K7_Thunderbird,
    kCpumMicroarch_AMD_K7_Morgan,
    kCpumMicroarch_AMD_K7_Thoroughbred,
    kCpumMicroarch_AMD_K7_Barton,
    kCpumMicroarch_AMD_K7_Unknown,
    kCpumMicroarch_AMD_K7_End,

    kCpumMicroarch_AMD_K8_First,
    kCpumMicroarch_AMD_K8_130nm = kCpumMicroarch_AMD_K8_First, /**< 130nm Clawhammer, Sledgehammer, Newcastle, Paris, Odessa, Dublin */
    kCpumMicroarch_AMD_K8_90nm,             /**< 90nm shrink */
    kCpumMicroarch_AMD_K8_90nm_DualCore,    /**< 90nm with two cores. */
    kCpumMicroarch_AMD_K8_90nm_AMDV,        /**< 90nm with AMD-V (usually) and two cores (usually). */
    kCpumMicroarch_AMD_K8_65nm,             /**< 65nm shrink. */
    kCpumMicroarch_AMD_K8_End,

    kCpumMicroarch_AMD_K10,
    kCpumMicroarch_AMD_K10_Lion,
    kCpumMicroarch_AMD_K10_Llano,
    kCpumMicroarch_AMD_Bobcat,
    kCpumMicroarch_AMD_Jaguar,

    kCpumMicroarch_AMD_15h_First,
    kCpumMicroarch_AMD_15h_Bulldozer = kCpumMicroarch_AMD_15h_First,
    kCpumMicroarch_AMD_15h_Piledriver,
    kCpumMicroarch_AMD_15h_Steamroller,     /**< Yet to be released, might have different family.  */
    kCpumMicroarch_AMD_15h_Excavator,       /**< Yet to be released, might have different family.  */
    kCpumMicroarch_AMD_15h_Unknown,
    kCpumMicroarch_AMD_15h_End,

    kCpumMicroarch_AMD_16h_First,
    kCpumMicroarch_AMD_16h_End,

    kCpumMicroarch_AMD_Unknown,
    kCpumMicroarch_AMD_End,

    kCpumMicroarch_VIA_First,
    kCpumMicroarch_Centaur_C6 = kCpumMicroarch_VIA_First,
    kCpumMicroarch_Centaur_C2,
    kCpumMicroarch_Centaur_C3,
    kCpumMicroarch_VIA_C3_M2,
    kCpumMicroarch_VIA_C3_C5A,          /**< 180nm Samuel - Cyrix III, C3, 1GigaPro. */
    kCpumMicroarch_VIA_C3_C5B,          /**< 150nm Samuel 2 - Cyrix III, C3, 1GigaPro, Eden ESP, XP 2000+. */
    kCpumMicroarch_VIA_C3_C5C,          /**< 130nm Ezra - C3, Eden ESP. */
    kCpumMicroarch_VIA_C3_C5N,          /**< 130nm Ezra-T - C3. */
    kCpumMicroarch_VIA_C3_C5XL,         /**< 130nm Nehemiah - C3, Eden ESP, Eden-N. */
    kCpumMicroarch_VIA_C3_C5P,          /**< 130nm Nehemiah+ - C3. */
    kCpumMicroarch_VIA_C7_C5J,          /**< 90nm Esther - C7, C7-D, C7-M, Eden, Eden ULV. */
    kCpumMicroarch_VIA_Isaiah,
    kCpumMicroarch_VIA_Unknown,
    kCpumMicroarch_VIA_End,

    kCpumMicroarch_Cyrix_First,
    kCpumMicroarch_Cyrix_5x86 = kCpumMicroarch_Cyrix_First,
    kCpumMicroarch_Cyrix_M1,
    kCpumMicroarch_Cyrix_MediaGX,
    kCpumMicroarch_Cyrix_MediaGXm,
    kCpumMicroarch_Cyrix_M2,
    kCpumMicroarch_Cyrix_Unknown,
    kCpumMicroarch_Cyrix_End,

    kCpumMicroarch_Unknown,

    kCpumMicroarch_32BitHack = 0x7fffffff
} CPUMMICROARCH;


/** Predicate macro for catching netburst CPUs. */
#define CPUMMICROARCH_IS_INTEL_NETBURST(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_Intel_NB_First && (a_enmMicroarch) <= kCpumMicroarch_Intel_NB_End)

/** Predicate macro for catching Core7 CPUs. */
#define CPUMMICROARCH_IS_INTEL_CORE7(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_Intel_Core7_First && (a_enmMicroarch) <= kCpumMicroarch_Intel_Core7_End)

/** Predicate macro for catching AMD Family OFh CPUs (aka K8).    */
#define CPUMMICROARCH_IS_AMD_FAM_0FH(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_AMD_K8_First && (a_enmMicroarch) <= kCpumMicroarch_AMD_K8_End)

/** Predicate macro for catching AMD Family 10H CPUs (aka K10).    */
#define CPUMMICROARCH_IS_AMD_FAM_10H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_K10)

/** Predicate macro for catching AMD Family 11H CPUs (aka Lion).    */
#define CPUMMICROARCH_IS_AMD_FAM_11H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_K10_Lion)

/** Predicate macro for catching AMD Family 12H CPUs (aka Llano).    */
#define CPUMMICROARCH_IS_AMD_FAM_12H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_K10_Llano)

/** Predicate macro for catching AMD Family 14H CPUs (aka Bobcat).    */
#define CPUMMICROARCH_IS_AMD_FAM_14H(a_enmMicroarch) ((a_enmMicroarch) == kCpumMicroarch_AMD_Bobcat)

/** Predicate macro for catching AMD Family 15H CPUs (bulldozer and it's
 * decendants). */
#define CPUMMICROARCH_IS_AMD_FAM_15H(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_AMD_15h_First && (a_enmMicroarch) <= kCpumMicroarch_AMD_15h_End)

/** Predicate macro for catching AMD Family 16H CPUs. */
#define CPUMMICROARCH_IS_AMD_FAM_16H(a_enmMicroarch) \
    ((a_enmMicroarch) >= kCpumMicroarch_AMD_16h_First && (a_enmMicroarch) <= kCpumMicroarch_AMD_16h_End)



/**
 * CPUID leaf.
 */
typedef struct CPUMCPUIDLEAF
{
    /** The leaf number. */
    uint32_t    uLeaf;
    /** The sub-leaf number. */
    uint32_t    uSubLeaf;
    /** Sub-leaf mask.  This is 0 when sub-leaves aren't used. */
    uint32_t    fSubLeafMask;

    /** The EAX value. */
    uint32_t    uEax;
    /** The EBX value. */
    uint32_t    uEbx;
    /** The ECX value. */
    uint32_t    uEcx;
    /** The EDX value. */
    uint32_t    uEdx;

    /** Flags. */
    uint32_t    fFlags;
} CPUMCPUIDLEAF;
/** Pointer to a CPUID leaf. */
typedef CPUMCPUIDLEAF *PCPUMCPUIDLEAF;
/** Pointer to a const CPUID leaf. */
typedef CPUMCPUIDLEAF const *PCCPUMCPUIDLEAF;

/** @name CPUMCPUIDLEAF::fFlags
 * @{ */
/** Indicates that ECX (the sub-leaf indicator) doesn't change when
 * requesting the final leaf and all undefined leaves that follows it.
 * Observed for 0x0000000b on Intel. */
#define CPUMCPUIDLEAF_F_SUBLEAVES_ECX_UNCHANGED RT_BIT_32(0)
/** @} */

/**
 * Method used to deal with unknown CPUID leafs.
 */
typedef enum CPUMUKNOWNCPUID
{
    /** Invalid zero value. */
    CPUMUKNOWNCPUID_INVALID = 0,
    /** Use given default values (DefCpuId). */
    CPUMUKNOWNCPUID_DEFAULTS,
    /** Return the last standard leaf.
     * Intel Sandy Bridge has been observed doing this. */
    CPUMUKNOWNCPUID_LAST_STD_LEAF,
    /** Return the last standard leaf, with ecx observed.
     * Intel Sandy Bridge has been observed doing this. */
    CPUMUKNOWNCPUID_LAST_STD_LEAF_WITH_ECX,
    /** The register values are passed thru unmodified. */
    CPUMUKNOWNCPUID_PASSTHRU,
    /** End of valid value. */
    CPUMUKNOWNCPUID_END,
    /** Ensure 32-bit type. */
    CPUMUKNOWNCPUID_32BIT_HACK = 0x7fffffff
} CPUMUKNOWNCPUID;
/** Pointer to unknown CPUID leaf method. */
typedef CPUMUKNOWNCPUID *PCPUMUKNOWNCPUID;



/** @name Guest Register Getters.
 * @{ */
VMMDECL(void)       CPUMGetGuestGDTR(PVMCPU pVCpu, PVBOXGDTR pGDTR);
VMMDECL(RTGCPTR)    CPUMGetGuestIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)      CPUMGetGuestTR(PVMCPU pVCpu, PCPUMSELREGHID pHidden);
VMMDECL(RTSEL)      CPUMGetGuestLDTR(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestLdtrEx(PVMCPU pVCpu, uint64_t *pGCPtrBase, uint32_t *pcbLimit);
VMMDECL(uint64_t)   CPUMGetGuestCR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR4(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestCR8(PVMCPU pVCpu);
VMMDECL(int)        CPUMGetGuestCRx(PVMCPU pVCpu, unsigned iReg, uint64_t *pValue);
VMMDECL(uint32_t)   CPUMGetGuestEFlags(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEIP(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestRIP(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEAX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEBX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestECX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEDX(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestESI(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEDI(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestESP(PVMCPU pVCpu);
VMMDECL(uint32_t)   CPUMGetGuestEBP(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestCS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestDS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestES(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestFS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestGS(PVMCPU pVCpu);
VMMDECL(RTSEL)      CPUMGetGuestSS(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR0(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR1(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR2(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR3(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR6(PVMCPU pVCpu);
VMMDECL(uint64_t)   CPUMGetGuestDR7(PVMCPU pVCpu);
VMMDECL(int)        CPUMGetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t *pValue);
VMMDECL(void)       CPUMGetGuestCpuId(PVMCPU pVCpu, uint32_t iLeaf, uint32_t *pEax, uint32_t *pEbx, uint32_t *pEcx, uint32_t *pEdx);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdStdMax(PVM pVM);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdExtMax(PVM pVM);
VMMDECL(uint32_t)   CPUMGetGuestCpuIdCentaurMax(PVM pVM);
VMMDECL(uint64_t)   CPUMGetGuestEFER(PVMCPU pVCpu);
VMMDECL(int)        CPUMQueryGuestMsr(PVMCPU pVCpu, uint32_t idMsr, uint64_t *puValue);
VMMDECL(int)        CPUMSetGuestMsr(PVMCPU pVCpu, uint32_t idMsr, uint64_t uValue);
VMMDECL(CPUMCPUVENDOR)  CPUMGetGuestCpuVendor(PVM pVM);
VMMDECL(CPUMCPUVENDOR)  CPUMGetHostCpuVendor(PVM pVM);
/** @} */

/** @name Guest Register Setters.
 * @{ */
VMMDECL(int)        CPUMSetGuestGDTR(PVMCPU pVCpu, uint64_t GCPtrBase, uint16_t cbLimit);
VMMDECL(int)        CPUMSetGuestIDTR(PVMCPU pVCpu, uint64_t GCPtrBase, uint16_t cbLimit);
VMMDECL(int)        CPUMSetGuestTR(PVMCPU pVCpu, uint16_t tr);
VMMDECL(int)        CPUMSetGuestLDTR(PVMCPU pVCpu, uint16_t ldtr);
VMMDECL(int)        CPUMSetGuestCR0(PVMCPU pVCpu, uint64_t cr0);
VMMDECL(int)        CPUMSetGuestCR2(PVMCPU pVCpu, uint64_t cr2);
VMMDECL(int)        CPUMSetGuestCR3(PVMCPU pVCpu, uint64_t cr3);
VMMDECL(int)        CPUMSetGuestCR4(PVMCPU pVCpu, uint64_t cr4);
VMMDECL(int)        CPUMSetGuestDR0(PVMCPU pVCpu, uint64_t uDr0);
VMMDECL(int)        CPUMSetGuestDR1(PVMCPU pVCpu, uint64_t uDr1);
VMMDECL(int)        CPUMSetGuestDR2(PVMCPU pVCpu, uint64_t uDr2);
VMMDECL(int)        CPUMSetGuestDR3(PVMCPU pVCpu, uint64_t uDr3);
VMMDECL(int)        CPUMSetGuestDR6(PVMCPU pVCpu, uint64_t uDr6);
VMMDECL(int)        CPUMSetGuestDR7(PVMCPU pVCpu, uint64_t uDr7);
VMMDECL(int)        CPUMSetGuestDRx(PVMCPU pVCpu, uint32_t iReg, uint64_t Value);
VMMDECL(int)        CPUMSetGuestEFlags(PVMCPU pVCpu, uint32_t eflags);
VMMDECL(int)        CPUMSetGuestEIP(PVMCPU pVCpu, uint32_t eip);
VMMDECL(int)        CPUMSetGuestEAX(PVMCPU pVCpu, uint32_t eax);
VMMDECL(int)        CPUMSetGuestEBX(PVMCPU pVCpu, uint32_t ebx);
VMMDECL(int)        CPUMSetGuestECX(PVMCPU pVCpu, uint32_t ecx);
VMMDECL(int)        CPUMSetGuestEDX(PVMCPU pVCpu, uint32_t edx);
VMMDECL(int)        CPUMSetGuestESI(PVMCPU pVCpu, uint32_t esi);
VMMDECL(int)        CPUMSetGuestEDI(PVMCPU pVCpu, uint32_t edi);
VMMDECL(int)        CPUMSetGuestESP(PVMCPU pVCpu, uint32_t esp);
VMMDECL(int)        CPUMSetGuestEBP(PVMCPU pVCpu, uint32_t ebp);
VMMDECL(int)        CPUMSetGuestCS(PVMCPU pVCpu, uint16_t cs);
VMMDECL(int)        CPUMSetGuestDS(PVMCPU pVCpu, uint16_t ds);
VMMDECL(int)        CPUMSetGuestES(PVMCPU pVCpu, uint16_t es);
VMMDECL(int)        CPUMSetGuestFS(PVMCPU pVCpu, uint16_t fs);
VMMDECL(int)        CPUMSetGuestGS(PVMCPU pVCpu, uint16_t gs);
VMMDECL(int)        CPUMSetGuestSS(PVMCPU pVCpu, uint16_t ss);
VMMDECL(void)       CPUMSetGuestEFER(PVMCPU pVCpu, uint64_t val);
VMMDECL(void)       CPUMSetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(void)       CPUMClearGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(bool)       CPUMGetGuestCpuIdFeature(PVM pVM, CPUMCPUIDFEATURE enmFeature);
VMMDECL(void)       CPUMSetGuestCtx(PVMCPU pVCpu, const PCPUMCTX pCtx);
VMM_INT_DECL(void)  CPUMGuestLazyLoadHiddenCsAndSs(PVMCPU pVCpu);
VMM_INT_DECL(void)  CPUMGuestLazyLoadHiddenSelectorReg(PVMCPU pVCpu, PCPUMSELREG pSReg);
VMMR0_INT_DECL(void)        CPUMR0SetGuestTscAux(PVMCPU pVCpu, uint64_t uValue);
VMMR0_INT_DECL(uint64_t)    CPUMR0GetGuestTscAux(PVMCPU pVCpu);
/** @} */


/** @name Misc Guest Predicate Functions.
 * @{  */

VMMDECL(bool)       CPUMIsGuestIn16BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestIn32BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestIn64BitCode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestNXEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestPageSizeExtEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestPagingEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestR0WriteProtEnabled(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInRealMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInRealOrV86Mode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInProtectedMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInPagedProtectedMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInLongMode(PVMCPU pVCpu);
VMMDECL(bool)       CPUMIsGuestInPAEMode(PVMCPU pVCpu);
VMM_INT_DECL(bool)  CPUMIsGuestInRawMode(PVMCPU pVCpu);

#ifndef VBOX_WITHOUT_UNNAMED_UNIONS

/**
 * Tests if the guest is running in real mode or not.
 *
 * @returns true if in real mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestInRealModeEx(PCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE);
}

/**
 * Tests if the guest is running in real or virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestInRealOrV86ModeEx(PCPUMCTX pCtx)
{
    return !(pCtx->cr0 & X86_CR0_PE)
        || pCtx->eflags.Bits.u1VM;  /* Cannot be set in long mode. Intel spec 2.3.1 "System Flags and Fields in IA-32e Mode". */
}

/**
 * Tests if the guest is running in virtual 8086 mode.
 *
 * @returns @c true if it is, @c false if not.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool) CPUMIsGuestInV86ModeEx(PCPUMCTX pCtx)
{
    return (pCtx->eflags.Bits.u1VM == 1);
}

/**
 * Tests if the guest is running in paged protected or not.
 *
 * @returns true if in paged protected mode, otherwise false.
 * @param   pVM     The VM handle.
 */
DECLINLINE(bool)    CPUMIsGuestInPagedProtectedModeEx(PCPUMCTX pCtx)
{
    return (pCtx->cr0 & (X86_CR0_PE | X86_CR0_PG)) == (X86_CR0_PE | X86_CR0_PG);
}

/**
 * Tests if the guest is running in long mode or not.
 *
 * @returns true if in long mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestInLongModeEx(PCPUMCTX pCtx)
{
    return (pCtx->msrEFER & MSR_K6_EFER_LMA) == MSR_K6_EFER_LMA;
}

VMM_INT_DECL(bool) CPUMIsGuestIn64BitCodeSlow(PCPUMCTX pCtx);

/**
 * Tests if the guest is running in 64 bits mode or not.
 *
 * @returns true if in 64 bits protected mode, otherwise false.
 * @param   pVCpu   The current virtual CPU.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestIn64BitCodeEx(PCPUMCTX pCtx)
{
    if (!(pCtx->msrEFER & MSR_K6_EFER_LMA))
        return false;
    if (!CPUMSELREG_ARE_HIDDEN_PARTS_VALID(NULL, &pCtx->cs))
        return CPUMIsGuestIn64BitCodeSlow(pCtx);
    return pCtx->cs.Attr.n.u1Long;
}

/**
 * Tests if the guest has paging enabled or not.
 *
 * @returns true if paging is enabled, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestPagingEnabledEx(PCPUMCTX pCtx)
{
    return !!(pCtx->cr0 & X86_CR0_PG);
}

/**
 * Tests if the guest is running in PAE mode or not.
 *
 * @returns true if in PAE mode, otherwise false.
 * @param   pCtx    Current CPU context
 */
DECLINLINE(bool)    CPUMIsGuestInPAEModeEx(PCPUMCTX pCtx)
{
    /* Intel mentions EFER.LMA and EFER.LME in different parts of their spec. We shall use EFER.LMA rather
       than EFER.LME as it reflects if the CPU has entered paging with EFER.LME set.  */
    return (   (pCtx->cr4 & X86_CR4_PAE)
            && CPUMIsGuestPagingEnabledEx(pCtx)
            && !(pCtx->msrEFER & MSR_K6_EFER_LMA));
}

#endif /* VBOX_WITHOUT_UNNAMED_UNIONS */

/** @} */


/** @name Hypervisor Register Getters.
 * @{ */
VMMDECL(RTSEL)          CPUMGetHyperCS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperDS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperES(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperFS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperGS(PVMCPU pVCpu);
VMMDECL(RTSEL)          CPUMGetHyperSS(PVMCPU pVCpu);
#if 0 /* these are not correct. */
VMMDECL(uint32_t)       CPUMGetHyperCR0(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR2(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR3(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperCR4(PVMCPU pVCpu);
#endif
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperEAX(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEBX(PVMCPU pVCpu);
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperECX(PVMCPU pVCpu);
/** This register is only saved on fatal traps. */
VMMDECL(uint32_t)       CPUMGetHyperEDX(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperESI(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEDI(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEBP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperESP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEFlags(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperEIP(PVMCPU pVCpu);
VMMDECL(uint64_t)       CPUMGetHyperRIP(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetHyperIDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(uint32_t)       CPUMGetHyperGDTR(PVMCPU pVCpu, uint16_t *pcbLimit);
VMMDECL(RTSEL)          CPUMGetHyperLDTR(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR0(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR1(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR2(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR3(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR6(PVMCPU pVCpu);
VMMDECL(RTGCUINTREG)    CPUMGetHyperDR7(PVMCPU pVCpu);
VMMDECL(void)           CPUMGetHyperCtx(PVMCPU pVCpu, PCPUMCTX pCtx);
VMMDECL(uint32_t)       CPUMGetHyperCR3(PVMCPU pVCpu);
/** @} */

/** @name Hypervisor Register Setters.
 * @{ */
VMMDECL(void)           CPUMSetHyperGDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(void)           CPUMSetHyperLDTR(PVMCPU pVCpu, RTSEL SelLDTR);
VMMDECL(void)           CPUMSetHyperIDTR(PVMCPU pVCpu, uint32_t addr, uint16_t limit);
VMMDECL(void)           CPUMSetHyperCR3(PVMCPU pVCpu, uint32_t cr3);
VMMDECL(void)           CPUMSetHyperTR(PVMCPU pVCpu, RTSEL SelTR);
VMMDECL(void)           CPUMSetHyperCS(PVMCPU pVCpu, RTSEL SelCS);
VMMDECL(void)           CPUMSetHyperDS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperES(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperFS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperGS(PVMCPU pVCpu, RTSEL SelDS);
VMMDECL(void)           CPUMSetHyperSS(PVMCPU pVCpu, RTSEL SelSS);
VMMDECL(void)           CPUMSetHyperESP(PVMCPU pVCpu, uint32_t u32ESP);
VMMDECL(int)            CPUMSetHyperEFlags(PVMCPU pVCpu, uint32_t Efl);
VMMDECL(void)           CPUMSetHyperEIP(PVMCPU pVCpu, uint32_t u32EIP);
VMM_INT_DECL(void)      CPUMSetHyperState(PVMCPU pVCpu, uint32_t u32EIP, uint32_t u32ESP, uint32_t u32EAX, uint32_t u32EDX);
VMMDECL(void)           CPUMSetHyperDR0(PVMCPU pVCpu, RTGCUINTREG uDr0);
VMMDECL(void)           CPUMSetHyperDR1(PVMCPU pVCpu, RTGCUINTREG uDr1);
VMMDECL(void)           CPUMSetHyperDR2(PVMCPU pVCpu, RTGCUINTREG uDr2);
VMMDECL(void)           CPUMSetHyperDR3(PVMCPU pVCpu, RTGCUINTREG uDr3);
VMMDECL(void)           CPUMSetHyperDR6(PVMCPU pVCpu, RTGCUINTREG uDr6);
VMMDECL(void)           CPUMSetHyperDR7(PVMCPU pVCpu, RTGCUINTREG uDr7);
VMMDECL(void)           CPUMSetHyperCtx(PVMCPU pVCpu, const PCPUMCTX pCtx);
VMMDECL(int)            CPUMRecalcHyperDRx(PVMCPU pVCpu, uint8_t iGstReg, bool fForceHyper);
/** @} */

VMMDECL(void)           CPUMPushHyper(PVMCPU pVCpu, uint32_t u32);
VMMDECL(int)            CPUMQueryHyperCtxPtr(PVMCPU pVCpu, PCPUMCTX *ppCtx);
VMMDECL(PCPUMCTX)       CPUMGetHyperCtxPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetHyperCtxCore(PVMCPU pVCpu);
VMMDECL(PCPUMCTX)       CPUMQueryGuestCtxPtr(PVMCPU pVCpu);
VMMDECL(PCCPUMCTXCORE)  CPUMGetGuestCtxCore(PVMCPU pVCpu);
VMM_INT_DECL(int)       CPUMRawEnter(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
VMM_INT_DECL(int)       CPUMRawLeave(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore, int rc);
VMMDECL(uint32_t)       CPUMRawGetEFlags(PVMCPU pVCpu);
VMMDECL(void)           CPUMRawSetEFlags(PVMCPU pVCpu, uint32_t fEfl);

/** @name Changed flags.
 * These flags are used to keep track of which important register that
 * have been changed since last they were reset. The only one allowed
 * to clear them is REM!
 * @{
 */
#define CPUM_CHANGED_FPU_REM                    RT_BIT(0)
#define CPUM_CHANGED_CR0                        RT_BIT(1)
#define CPUM_CHANGED_CR4                        RT_BIT(2)
#define CPUM_CHANGED_GLOBAL_TLB_FLUSH           RT_BIT(3)
#define CPUM_CHANGED_CR3                        RT_BIT(4)
#define CPUM_CHANGED_GDTR                       RT_BIT(5)
#define CPUM_CHANGED_IDTR                       RT_BIT(6)
#define CPUM_CHANGED_LDTR                       RT_BIT(7)
#define CPUM_CHANGED_TR                         RT_BIT(8)  /**@< Currently unused. */
#define CPUM_CHANGED_SYSENTER_MSR               RT_BIT(9)
#define CPUM_CHANGED_HIDDEN_SEL_REGS            RT_BIT(10) /**@< Currently unused. */
#define CPUM_CHANGED_CPUID                      RT_BIT(11)
#define CPUM_CHANGED_ALL                        (  CPUM_CHANGED_FPU_REM \
                                                 | CPUM_CHANGED_CR0 \
                                                 | CPUM_CHANGED_CR4 \
                                                 | CPUM_CHANGED_GLOBAL_TLB_FLUSH \
                                                 | CPUM_CHANGED_CR3 \
                                                 | CPUM_CHANGED_GDTR \
                                                 | CPUM_CHANGED_IDTR \
                                                 | CPUM_CHANGED_LDTR \
                                                 | CPUM_CHANGED_TR \
                                                 | CPUM_CHANGED_SYSENTER_MSR \
                                                 | CPUM_CHANGED_HIDDEN_SEL_REGS \
                                                 | CPUM_CHANGED_CPUID )
/** @} */

VMMDECL(void)           CPUMSetChangedFlags(PVMCPU pVCpu, uint32_t fChangedFlags);
VMMR3DECL(uint32_t)     CPUMR3RemEnter(PVMCPU pVCpu, uint32_t *puCpl);
VMMR3DECL(void)         CPUMR3RemLeave(PVMCPU pVCpu, bool fNoOutOfSyncSels);
VMMDECL(bool)           CPUMSupportsFXSR(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysEnter(PVM pVM);
VMMDECL(bool)           CPUMIsHostUsingSysCall(PVM pVM);
VMMDECL(bool)           CPUMIsGuestFPUStateActive(PVMCPU pVCpu);
VMMDECL(void)           CPUMDeactivateGuestFPUState(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestDebugStateActive(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsGuestDebugStateActivePending(PVMCPU pVCpu);
VMMDECL(void)           CPUMDeactivateGuestDebugState(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsHyperDebugStateActive(PVMCPU pVCpu);
VMMDECL(bool)           CPUMIsHyperDebugStateActivePending(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestCPL(PVMCPU pVCpu);
VMMDECL(CPUMMODE)       CPUMGetGuestMode(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMGetGuestCodeBits(PVMCPU pVCpu);
VMMDECL(DISCPUMODE)     CPUMGetGuestDisMode(PVMCPU pVCpu);
VMMDECL(uint64_t)       CPUMGetGuestScalableBusFrequency(PVM pVM);

/** @name Typical scalable bus frequency values.
 * @{ */
/** Special internal value indicating that we don't know the frequency.
 *  @internal  */
#define CPUM_SBUSFREQ_UNKNOWN       UINT64_C(1)
#define CPUM_SBUSFREQ_100MHZ        UINT64_C(100000000)
#define CPUM_SBUSFREQ_133MHZ        UINT64_C(133333333)
#define CPUM_SBUSFREQ_167MHZ        UINT64_C(166666666)
#define CPUM_SBUSFREQ_200MHZ        UINT64_C(200000000)
#define CPUM_SBUSFREQ_267MHZ        UINT64_C(266666666)
#define CPUM_SBUSFREQ_333MHZ        UINT64_C(333333333)
#define CPUM_SBUSFREQ_400MHZ        UINT64_C(400000000)
/** @} */


#ifdef IN_RING3
/** @defgroup grp_cpum_r3    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

VMMR3DECL(int)          CPUMR3Init(PVM pVM);
VMMR3DECL(int)          CPUMR3InitCompleted(PVM pVM);
VMMR3DECL(void)         CPUMR3LogCpuIds(PVM pVM);
VMMR3DECL(void)         CPUMR3Relocate(PVM pVM);
VMMR3DECL(int)          CPUMR3Term(PVM pVM);
VMMR3DECL(void)         CPUMR3Reset(PVM pVM);
VMMR3DECL(void)         CPUMR3ResetCpu(PVM pVM, PVMCPU pVCpu);
VMMDECL(bool)           CPUMR3IsStateRestorePending(PVM pVM);
VMMR3DECL(void)         CPUMR3SetHWVirtEx(PVM pVM, bool fHWVirtExEnabled);
VMMR3DECL(int)          CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdStdRCPtr(PVM pVM);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdExtRCPtr(PVM pVM);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdCentaurRCPtr(PVM pVM);
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdDefRCPtr(PVM pVM);

VMMR3DECL(CPUMMICROARCH)    CPUMR3CpuIdDetermineMicroarchEx(CPUMCPUVENDOR enmVendor, uint8_t bFamily,
                                                            uint8_t bModel, uint8_t bStepping);
VMMR3DECL(const char *)     CPUMR3MicroarchName(CPUMMICROARCH enmMicroarch);
VMMR3DECL(int)              CPUMR3CpuIdCollectLeaves(PCPUMCPUIDLEAF *ppaLeaves, uint32_t *pcLeaves);
VMMR3DECL(int)              CPUMR3CpuIdDetectUnknownLeafMethod(PCPUMUKNOWNCPUID penmUnknownMethod, PCPUMCPUID pDefUnknown);
VMMR3DECL(const char *)     CPUMR3CpuIdUnknownLeafMethodName(CPUMUKNOWNCPUID enmUnknownMethod);
VMMR3DECL(CPUMCPUVENDOR)    CPUMR3CpuIdDetectVendorEx(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX);
VMMR3DECL(const char *)     CPUMR3CpuVendorName(CPUMCPUVENDOR enmVendor);

/** @} */
#endif /* IN_RING3 */

#ifdef IN_RC
/** @defgroup grp_cpum_gc    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */

/**
 * Calls a guest trap/interrupt handler directly
 *
 * Assumes a trap stack frame has already been setup on the guest's stack!
 * This function does not return!
 *
 * @param   pRegFrame   Original trap/interrupt context
 * @param   selCS       Code selector of handler
 * @param   pHandler    GC virtual address of handler
 * @param   eflags      Callee's EFLAGS
 * @param   selSS       Stack selector for handler
 * @param   pEsp        Stack address for handler
 */
DECLASM(void)           CPUMGCCallGuestTrapHandler(PCPUMCTXCORE pRegFrame, uint32_t selCS, RTRCPTR pHandler,
                                                   uint32_t eflags, uint32_t selSS, RTRCPTR pEsp);

/**
 * Call guest V86 code directly.
 *
 * This function does not return!
 *
 * @param   pRegFrame   Original trap/interrupt context
 */
DECLASM(void)           CPUMGCCallV86Code(PCPUMCTXCORE pRegFrame);

VMMDECL(int)            CPUMHandleLazyFPU(PVMCPU pVCpu);
VMMDECL(uint32_t)       CPUMRCGetGuestCPL(PVMCPU pVCpu, PCPUMCTXCORE pRegFrame);
#ifdef VBOX_WITH_RAW_RING1
VMMDECL(void)           CPUMRCRecheckRawState(PVMCPU pVCpu, PCPUMCTXCORE pCtxCore);
#endif

/** @} */
#endif /* IN_RC */

#ifdef IN_RING0
/** @defgroup grp_cpum_r0    The CPU Monitor(/Manager) API
 * @ingroup grp_cpum
 * @{
 */
VMMR0_INT_DECL(int)     CPUMR0ModuleInit(void);
VMMR0_INT_DECL(int)     CPUMR0ModuleTerm(void);
VMMR0_INT_DECL(int)     CPUMR0InitVM(PVM pVM);
VMMR0_INT_DECL(int)     CPUMR0Trap07Handler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0_INT_DECL(int)     CPUMR0LoadGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0_INT_DECL(int)     CPUMR0SaveGuestFPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);
VMMR0_INT_DECL(int)     CPUMR0SaveHostDebugState(PVM pVM, PVMCPU pVCpu);
VMMR0_INT_DECL(bool)    CPUMR0DebugStateMaybeSaveGuestAndRestoreHost(PVMCPU pVCpu, bool fDr6);
VMMR0_INT_DECL(bool)    CPUMR0DebugStateMaybeSaveGuest(PVMCPU pVCpu, bool fDr6);

VMMR0_INT_DECL(void)    CPUMR0LoadGuestDebugState(PVMCPU pVCpu, bool fDr6);
VMMR0_INT_DECL(void)    CPUMR0LoadHyperDebugState(PVMCPU pVCpu, bool fDr6);
#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
VMMR0_INT_DECL(void)    CPUMR0SetLApic(PVMCPU pVCpu, RTCPUID idHostCpu);
#endif

/** @} */
#endif /* IN_RING0 */

/** @} */
RT_C_DECLS_END


#endif

