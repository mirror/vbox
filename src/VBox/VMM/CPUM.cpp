/* $Id$ */
/** @file
 * CPUM - CPU Monitor / Manager.
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
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/** @page pg_cpum CPUM - CPU Monitor / Manager
 *
 * The CPU Monitor / Manager keeps track of all the CPU registers. It is
 * also responsible for lazy FPU handling and some of the context loading
 * in raw mode.
 *
 * There are three CPU contexts, the most important one is the guest one (GC).
 * When running in raw-mode (RC) there is a special hyper context for the VMM
 * part that floats around inside the guest address space. When running in
 * raw-mode, CPUM also maintains a host context for saving and restoring
 * registers accross world switches. This latter is done in cooperation with the
 * world switcher (@see pg_vmm).
 *
 * @see grp_cpum
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_CPUM
#include <VBox/cpum.h>
#include <VBox/cpumdis.h>
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/mm.h>
#include <VBox/selm.h>
#include <VBox/dbgf.h>
#include <VBox/patm.h>
#include <VBox/hwaccm.h>
#include <VBox/ssm.h>
#include "CPUMInternal.h"
#include <VBox/vm.h>

#include <VBox/param.h>
#include <VBox/dis.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/mp.h>
#include <iprt/cpuset.h>


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** The current saved state version. */
#ifdef VBOX_WITH_LIVE_MIGRATION
#define CPUM_SAVED_STATE_VERSION                11
#else
#define CPUM_SAVED_STATE_VERSION                10
#endif
/** The saved state version of 3.0 and 3.1 trunk before the teleportation
 * changes. */
#define CPUM_SAVED_STATE_VERSION_VER3_0         10
/** The saved state version for the 2.1 trunk before the MSR changes. */
#define CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR   9
/** The saved state version of 2.0, used for backwards compatibility. */
#define CPUM_SAVED_STATE_VERSION_VER2_0         8
/** The saved state version of 1.6, used for backwards compatability. */
#define CPUM_SAVED_STATE_VERSION_VER1_6         6


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/

/**
 * What kind of cpu info dump to perform.
 */
typedef enum CPUMDUMPTYPE
{
    CPUMDUMPTYPE_TERSE,
    CPUMDUMPTYPE_DEFAULT,
    CPUMDUMPTYPE_VERBOSE
} CPUMDUMPTYPE;
/** Pointer to a cpu info dump type. */
typedef CPUMDUMPTYPE *PCPUMDUMPTYPE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static CPUMCPUVENDOR cpumR3DetectVendor(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX);
static int cpumR3CpuIdInit(PVM pVM);
#ifdef VBOX_WITH_LIVE_MIGRATION
static DECLCALLBACK(int)  cpumR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass);
#endif
static DECLCALLBACK(int)  cpumR3SaveExec(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int)  cpumR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoGuestInstr(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoHyper(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3InfoHost(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);
static DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs);


/**
 * Initializes the CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CPUMR3Init(PVM pVM)
{
    LogFlow(("CPUMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, cpum.s, 32);
    AssertCompile(sizeof(pVM->cpum.s) <= sizeof(pVM->cpum.padding));
    AssertCompileSizeAlignment(CPUMCTX, 64);
    AssertCompileSizeAlignment(CPUMCTXMSR, 64);
    AssertCompileSizeAlignment(CPUMHOSTCTX, 64);
    AssertCompileMemberAlignment(VM, cpum, 64);
    AssertCompileMemberAlignment(VM, aCpus, 64);
    AssertCompileMemberAlignment(VMCPU, cpum.s, 64);
    AssertCompileMemberSizeAlignment(VM, aCpus[0].cpum.s, 64);

    /* Calculate the offset from CPUM to CPUMCPU for the first CPU. */
    pVM->cpum.s.ulOffCPUMCPU = RT_OFFSETOF(VM, aCpus[0].cpum) - RT_OFFSETOF(VM, cpum);
    Assert((uintptr_t)&pVM->cpum + pVM->cpum.s.ulOffCPUMCPU == (uintptr_t)&pVM->aCpus[0].cpum);

    /* Calculate the offset from CPUMCPU to CPUM. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        /*
         * Setup any fixed pointers and offsets.
         */
        pVCpu->cpum.s.pHyperCoreR3 = CPUMCTX2CORE(&pVCpu->cpum.s.Hyper);
        pVCpu->cpum.s.pHyperCoreR0 = VM_R0_ADDR(pVM, CPUMCTX2CORE(&pVCpu->cpum.s.Hyper));

        pVCpu->cpum.s.ulOffCPUM   = RT_OFFSETOF(VM, aCpus[i].cpum) - RT_OFFSETOF(VM, cpum);
        Assert((uintptr_t)&pVCpu->cpum - pVCpu->cpum.s.ulOffCPUM == (uintptr_t)&pVM->cpum);
    }

    /*
     * Check that the CPU supports the minimum features we require.
     */
    if (!ASMHasCpuId())
    {
        Log(("The CPU doesn't support CPUID!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    ASMCpuId_ECX_EDX(1, &pVM->cpum.s.CPUFeatures.ecx, &pVM->cpum.s.CPUFeatures.edx);
    ASMCpuId_ECX_EDX(0x80000001, &pVM->cpum.s.CPUFeaturesExt.ecx, &pVM->cpum.s.CPUFeaturesExt.edx);

    /* Setup the CR4 AND and OR masks used in the switcher */
    /* Depends on the presence of FXSAVE(SSE) support on the host CPU */
    if (!pVM->cpum.s.CPUFeatures.edx.u1FXSR)
    {
        Log(("The CPU doesn't support FXSAVE/FXRSTOR!\n"));
        /* No FXSAVE implies no SSE */
        pVM->cpum.s.CR4.AndMask = X86_CR4_PVI | X86_CR4_VME;
        pVM->cpum.s.CR4.OrMask  = 0;
    }
    else
    {
        pVM->cpum.s.CR4.AndMask = X86_CR4_OSXMMEEXCPT | X86_CR4_PVI | X86_CR4_VME;
        pVM->cpum.s.CR4.OrMask  = X86_CR4_OSFSXR;
    }

    if (!pVM->cpum.s.CPUFeatures.edx.u1MMX)
    {
        Log(("The CPU doesn't support MMX!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    if (!pVM->cpum.s.CPUFeatures.edx.u1TSC)
    {
        Log(("The CPU doesn't support TSC!\n"));
        return VERR_UNSUPPORTED_CPU;
    }
    /* Bogus on AMD? */
    if (!pVM->cpum.s.CPUFeatures.edx.u1SEP)
        Log(("The CPU doesn't support SYSENTER/SYSEXIT!\n"));

    /*
     * Detech the host CPU vendor.
     * (The guest CPU vendor is re-detected later on.)
     */
    uint32_t uEAX, uEBX, uECX, uEDX;
    ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
    pVM->cpum.s.enmHostCpuVendor = cpumR3DetectVendor(uEAX, uEBX, uECX, uEDX);
    pVM->cpum.s.enmGuestCpuVendor = pVM->cpum.s.enmHostCpuVendor;

    /*
     * Setup hypervisor startup values.
     */

    /*
     * Register saved state data item.
     */
#ifdef VBOX_WITH_LIVE_MIGRATION
    int rc = SSMR3RegisterInternal(pVM, "cpum", 1, CPUM_SAVED_STATE_VERSION, sizeof(CPUM),
                                   NULL, cpumR3LiveExec, NULL,
                                   NULL, cpumR3SaveExec, NULL,
                                   NULL, cpumR3LoadExec, NULL);
#else
    int rc = SSMR3RegisterInternal(pVM, "cpum", 1, CPUM_SAVED_STATE_VERSION, sizeof(CPUM),
                                   NULL, NULL, NULL,
                                   NULL, cpumR3SaveExec, NULL,
                                   NULL, cpumR3LoadExec, NULL);
#endif
    if (RT_FAILURE(rc))
        return rc;

    /*
     * Register info handlers.
     */
    DBGFR3InfoRegisterInternal(pVM, "cpum",             "Displays the all the cpu states.",         &cpumR3InfoAll);
    DBGFR3InfoRegisterInternal(pVM, "cpumguest",        "Displays the guest cpu state.",            &cpumR3InfoGuest);
    DBGFR3InfoRegisterInternal(pVM, "cpumhyper",        "Displays the hypervisor cpu state.",       &cpumR3InfoHyper);
    DBGFR3InfoRegisterInternal(pVM, "cpumhost",         "Displays the host cpu state.",             &cpumR3InfoHost);
    DBGFR3InfoRegisterInternal(pVM, "cpuid",            "Displays the guest cpuid leaves.",         &cpumR3CpuIdInfo);
    DBGFR3InfoRegisterInternal(pVM, "cpumguestinstr",   "Displays the current guest instruction.",  &cpumR3InfoGuestInstr);

    /*
     * Initialize the Guest CPUID state.
     */
    rc = cpumR3CpuIdInit(pVM);
    if (RT_FAILURE(rc))
        return rc;
    CPUMR3Reset(pVM);
    return VINF_SUCCESS;
}


/**
 * Initializes the per-VCPU CPUM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CPUMR3InitCPU(PVM pVM)
{
    LogFlow(("CPUMR3InitCPU\n"));
    return VINF_SUCCESS;
}


/**
 * Detect the CPU vendor give n the
 *
 * @returns The vendor.
 * @param   uEAX                EAX from CPUID(0).
 * @param   uEBX                EBX from CPUID(0).
 * @param   uECX                ECX from CPUID(0).
 * @param   uEDX                EDX from CPUID(0).
 */
static CPUMCPUVENDOR cpumR3DetectVendor(uint32_t uEAX, uint32_t uEBX, uint32_t uECX, uint32_t uEDX)
{
    if (    uEAX >= 1
        &&  uEBX == X86_CPUID_VENDOR_AMD_EBX
        &&  uECX == X86_CPUID_VENDOR_AMD_ECX
        &&  uEDX == X86_CPUID_VENDOR_AMD_EDX)
        return CPUMCPUVENDOR_AMD;

    if (    uEAX >= 1
        &&  uEBX == X86_CPUID_VENDOR_INTEL_EBX
        &&  uECX == X86_CPUID_VENDOR_INTEL_ECX
        &&  uEDX == X86_CPUID_VENDOR_INTEL_EDX)
        return CPUMCPUVENDOR_INTEL;

    /** @todo detect the other buggers... */
    return CPUMCPUVENDOR_UNKNOWN;
}


/**
 * Fetches overrides for a CPUID leaf.
 *
 * @returns VBox status code.
 * @param   pLeaf               The leaf to load the overrides into.
 * @param   pCfgNode            The CFGM node containing the overrides
 *                              (/CPUM/HostCPUID/ or /CPUM/CPUID/).
 * @param   iLeaf               The CPUID leaf number.
 */
static int cpumR3CpuIdFetchLeafOverride(PCPUMCPUID pLeaf, PCFGMNODE pCfgNode, uint32_t iLeaf)
{
    PCFGMNODE pLeafNode = CFGMR3GetChildF(pCfgNode, "%RX32", iLeaf);
    if (pLeafNode)
    {
        uint32_t u32;
        int rc = CFGMR3QueryU32(pLeafNode, "eax", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->eax = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "ebx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->ebx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "ecx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->ecx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

        rc = CFGMR3QueryU32(pLeafNode, "edx", &u32);
        if (RT_SUCCESS(rc))
            pLeaf->edx = u32;
        else
            AssertReturn(rc == VERR_CFGM_VALUE_NOT_FOUND, rc);

    }
    return VINF_SUCCESS;
}


/**
 * Load the overrides for a set of CPUID leafs.
 *
 * @returns VBox status code.
 * @param   paLeafs             The leaf array.
 * @param   cLeafs              The number of leafs.
 * @param   uStart              The start leaf number.
 * @param   pCfgNode            The CFGM node containing the overrides
 *                              (/CPUM/HostCPUID/ or /CPUM/CPUID/).
 */
static int cpumR3CpuIdInitLoadOverrideSet(uint32_t uStart, PCPUMCPUID paLeafs, uint32_t cLeafs, PCFGMNODE pCfgNode)
{
    for (uint32_t i = 0; i < cLeafs; i++)
    {
        int rc = cpumR3CpuIdFetchLeafOverride(&paLeafs[i], pCfgNode, uStart + i);
        if (RT_FAILURE(rc))
            return rc;
    }

    return VINF_SUCCESS;
}

/**
 * Init a set of host CPUID leafs.
 *
 * @returns VBox status code.
 * @param   paLeafs             The leaf array.
 * @param   cLeafs              The number of leafs.
 * @param   uStart              The start leaf number.
 * @param   pCfgNode            The /CPUM/HostCPUID/ node.
 */
static int cpumR3CpuIdInitHostSet(uint32_t uStart, PCPUMCPUID paLeafs, uint32_t cLeafs, PCFGMNODE pCfgNode)
{
    /* Using the ECX variant for all of them can't hurt... */
    for (uint32_t i = 0; i < cLeafs; i++)
        ASMCpuId_Idx_ECX(uStart + i, 0, &paLeafs[i].eax, &paLeafs[i].ebx, &paLeafs[i].ecx, &paLeafs[i].edx);

    /* Load CPUID leaf override; we currently don't care if the caller
       specifies features the host CPU doesn't support. */
    return cpumR3CpuIdInitLoadOverrideSet(uStart, paLeafs, cLeafs, pCfgNode);
}


/**
 * Initializes the emulated CPU's cpuid information.
 *
 * @returns VBox status code.
 * @param   pVM          The VM to operate on.
 */
static int cpumR3CpuIdInit(PVM pVM)
{
    PCPUM       pCPUM    = &pVM->cpum.s;
    PCFGMNODE   pCpumCfg = CFGMR3GetChild(CFGMR3GetRoot(pVM), "CPUM");
    uint32_t    i;
    int         rc;

    /*
     * Get the host CPUIDs and redetect the guest CPU vendor (could've been overridden).
     */
    /** @cfgm{CPUM/HostCPUID/[000000xx|800000xx|c000000x]/[eax|ebx|ecx|edx],32-bit}
     * Overrides the host CPUID leaf values used for calculating the guest CPUID
     * leafs.  This can be used to preserve the CPUID values when moving a VM to
     * a different machine.  Another use is restricting (or extending) the
     * feature set exposed to the guest. */
    PCFGMNODE pHostOverrideCfg = CFGMR3GetChild(pCpumCfg, "HostCPUID");
    rc = cpumR3CpuIdInitHostSet(UINT32_C(0x00000000), &pCPUM->aGuestCpuIdStd[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdStd),     pHostOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitHostSet(UINT32_C(0x80000000), &pCPUM->aGuestCpuIdExt[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdExt),     pHostOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitHostSet(UINT32_C(0xc0000000), &pCPUM->aGuestCpuIdCentaur[0], RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur), pHostOverrideCfg);
    AssertRCReturn(rc, rc);

    pCPUM->enmGuestCpuVendor = cpumR3DetectVendor(pCPUM->aGuestCpuIdStd[0].eax, pCPUM->aGuestCpuIdStd[0].ebx,
                                                  pCPUM->aGuestCpuIdStd[0].ecx, pCPUM->aGuestCpuIdStd[0].edx);

    /*
     * Only report features we can support.
     */
    pCPUM->aGuestCpuIdStd[1].edx      &= X86_CPUID_FEATURE_EDX_FPU
                                       | X86_CPUID_FEATURE_EDX_VME
                                       | X86_CPUID_FEATURE_EDX_DE
                                       | X86_CPUID_FEATURE_EDX_PSE
                                       | X86_CPUID_FEATURE_EDX_TSC
                                       | X86_CPUID_FEATURE_EDX_MSR
                                       //| X86_CPUID_FEATURE_EDX_PAE   - not implemented yet.
                                       | X86_CPUID_FEATURE_EDX_MCE
                                       | X86_CPUID_FEATURE_EDX_CX8
                                       //| X86_CPUID_FEATURE_EDX_APIC  - set by the APIC device if present.
                                       /* Note! we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see #1757) */
                                       //| X86_CPUID_FEATURE_EDX_SEP
                                       | X86_CPUID_FEATURE_EDX_MTRR
                                       | X86_CPUID_FEATURE_EDX_PGE
                                       | X86_CPUID_FEATURE_EDX_MCA
                                       | X86_CPUID_FEATURE_EDX_CMOV
                                       | X86_CPUID_FEATURE_EDX_PAT
                                       | X86_CPUID_FEATURE_EDX_PSE36
                                       //| X86_CPUID_FEATURE_EDX_PSN   - no serial number.
                                       | X86_CPUID_FEATURE_EDX_CLFSH
                                       //| X86_CPUID_FEATURE_EDX_DS    - no debug store.
                                       //| X86_CPUID_FEATURE_EDX_ACPI  - not virtualized yet.
                                       | X86_CPUID_FEATURE_EDX_MMX
                                       | X86_CPUID_FEATURE_EDX_FXSR
                                       | X86_CPUID_FEATURE_EDX_SSE
                                       | X86_CPUID_FEATURE_EDX_SSE2
                                       //| X86_CPUID_FEATURE_EDX_SS    - no self snoop.
                                       //| X86_CPUID_FEATURE_EDX_HTT   - no hyperthreading.
                                       //| X86_CPUID_FEATURE_EDX_TM    - no thermal monitor.
                                       //| X86_CPUID_FEATURE_EDX_PBE   - no pending break enabled.
                                       | 0;
    pCPUM->aGuestCpuIdStd[1].ecx      &= 0
                                       | X86_CPUID_FEATURE_ECX_SSE3
                                       /* Can't properly emulate monitor & mwait with guest SMP; force the guest to use hlt for idling VCPUs. */
                                       | ((pVM->cCpus == 1) ? X86_CPUID_FEATURE_ECX_MONITOR : 0)
                                       //| X86_CPUID_FEATURE_ECX_CPLDS - no CPL qualified debug store.
                                       //| X86_CPUID_FEATURE_ECX_VMX   - not virtualized.
                                       //| X86_CPUID_FEATURE_ECX_EST   - no extended speed step.
                                       //| X86_CPUID_FEATURE_ECX_TM2   - no thermal monitor 2.
                                       //| X86_CPUID_FEATURE_ECX_SSSE3 - no SSSE3 support
                                       //| X86_CPUID_FEATURE_ECX_CNTXID - no L1 context id (MSR++).
                                       //| X86_CPUID_FEATURE_ECX_CX16  - no cmpxchg16b
                                       /* ECX Bit 14 - xTPR Update Control. Processor supports changing IA32_MISC_ENABLES[bit 23]. */
                                       //| X86_CPUID_FEATURE_ECX_TPRUPDATE
                                       /* ECX Bit 21 - x2APIC support - not yet. */
                                       // | X86_CPUID_FEATURE_ECX_X2APIC
                                       /* ECX Bit 23 - POPCOUNT instruction. */
                                       //| X86_CPUID_FEATURE_ECX_POPCOUNT
                                       | 0;

    /* ASSUMES that this is ALWAYS the AMD define feature set if present. */
    pCPUM->aGuestCpuIdExt[1].edx      &= X86_CPUID_AMD_FEATURE_EDX_FPU
                                       | X86_CPUID_AMD_FEATURE_EDX_VME
                                       | X86_CPUID_AMD_FEATURE_EDX_DE
                                       | X86_CPUID_AMD_FEATURE_EDX_PSE
                                       | X86_CPUID_AMD_FEATURE_EDX_TSC
                                       | X86_CPUID_AMD_FEATURE_EDX_MSR //?? this means AMD MSRs..
                                       //| X86_CPUID_AMD_FEATURE_EDX_PAE    - not implemented yet.
                                       //| X86_CPUID_AMD_FEATURE_EDX_MCE    - not virtualized yet.
                                       | X86_CPUID_AMD_FEATURE_EDX_CX8
                                       //| X86_CPUID_AMD_FEATURE_EDX_APIC   - set by the APIC device if present.
                                       /* Note! we don't report sysenter/sysexit support due to our inability to keep the IOPL part of eflags in sync while in ring 1 (see #1757) */
                                       //| X86_CPUID_AMD_FEATURE_EDX_SEP
                                       | X86_CPUID_AMD_FEATURE_EDX_MTRR
                                       | X86_CPUID_AMD_FEATURE_EDX_PGE
                                       | X86_CPUID_AMD_FEATURE_EDX_MCA
                                       | X86_CPUID_AMD_FEATURE_EDX_CMOV
                                       | X86_CPUID_AMD_FEATURE_EDX_PAT
                                       | X86_CPUID_AMD_FEATURE_EDX_PSE36
                                       //| X86_CPUID_AMD_FEATURE_EDX_NX     - not virtualized, requires PAE.
                                       //| X86_CPUID_AMD_FEATURE_EDX_AXMMX
                                       | X86_CPUID_AMD_FEATURE_EDX_MMX
                                       | X86_CPUID_AMD_FEATURE_EDX_FXSR
                                       | X86_CPUID_AMD_FEATURE_EDX_FFXSR
                                       //| X86_CPUID_AMD_FEATURE_EDX_PAGE1GB
                                       //| X86_CPUID_AMD_FEATURE_EDX_RDTSCP - AMD only; turned on when necessary
                                       //| X86_CPUID_AMD_FEATURE_EDX_LONG_MODE - turned on when necessary
                                       | X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX
                                       | X86_CPUID_AMD_FEATURE_EDX_3DNOW
                                       | 0;
    pCPUM->aGuestCpuIdExt[1].ecx      &= 0
                                       //| X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF
                                       //| X86_CPUID_AMD_FEATURE_ECX_CMPL
                                       //| X86_CPUID_AMD_FEATURE_ECX_SVM    - not virtualized.
                                       //| X86_CPUID_AMD_FEATURE_ECX_EXT_APIC
                                       /* Note: This could prevent teleporting from AMD to Intel CPUs! */
                                       | X86_CPUID_AMD_FEATURE_ECX_CR8L         /* expose lock mov cr0 = mov cr8 hack for guests that can use this feature to access the TPR. */
                                       //| X86_CPUID_AMD_FEATURE_ECX_ABM
                                       //| X86_CPUID_AMD_FEATURE_ECX_SSE4A
                                       //| X86_CPUID_AMD_FEATURE_ECX_MISALNSSE
                                       //| X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF
                                       //| X86_CPUID_AMD_FEATURE_ECX_OSVW
                                       //| X86_CPUID_AMD_FEATURE_ECX_SKINIT
                                       //| X86_CPUID_AMD_FEATURE_ECX_WDT
                                       | 0;

    rc = CFGMR3QueryBoolDef(pCpumCfg, "SyntheticCpu", &pCPUM->fSyntheticCpu, false); AssertRCReturn(rc, rc);
    if (pCPUM->fSyntheticCpu)
    {
        const char szVendor[13]    = "VirtualBox  ";
        const char szProcessor[48] = "VirtualBox SPARCx86 Processor v1000            "; /* includes null terminator */

        pCPUM->enmGuestCpuVendor = CPUMCPUVENDOR_SYNTHETIC;

        /* Limit the nr of standard leaves; 5 for monitor/mwait */
        pCPUM->aGuestCpuIdStd[0].eax = RT_MIN(pCPUM->aGuestCpuIdStd[0].eax, 5);

        /* 0: Vendor */
        pCPUM->aGuestCpuIdStd[0].ebx = pCPUM->aGuestCpuIdExt[0].ebx = ((uint32_t *)szVendor)[0];
        pCPUM->aGuestCpuIdStd[0].ecx = pCPUM->aGuestCpuIdExt[0].ecx = ((uint32_t *)szVendor)[2];
        pCPUM->aGuestCpuIdStd[0].edx = pCPUM->aGuestCpuIdExt[0].edx = ((uint32_t *)szVendor)[1];

        /* 1.eax: Version information.  family : model : stepping */
        pCPUM->aGuestCpuIdStd[1].eax = (0xf << 8) + (0x1 << 4) + 1;

        /* Leaves 2 - 4 are Intel only - zero them out */
        memset(&pCPUM->aGuestCpuIdStd[2], 0, sizeof(pCPUM->aGuestCpuIdStd[2]));
        memset(&pCPUM->aGuestCpuIdStd[3], 0, sizeof(pCPUM->aGuestCpuIdStd[3]));
        memset(&pCPUM->aGuestCpuIdStd[4], 0, sizeof(pCPUM->aGuestCpuIdStd[4]));

        /* Leaf 5 = monitor/mwait */

        /* Limit the nr of extended leaves: 0x80000008 to include the max virtual and physical address size (64 bits guests). */
        pCPUM->aGuestCpuIdExt[0].eax = RT_MIN(pCPUM->aGuestCpuIdExt[0].eax, 0x80000008);
        /* AMD only - set to zero. */
        pCPUM->aGuestCpuIdExt[0].ebx = pCPUM->aGuestCpuIdExt[0].ecx = pCPUM->aGuestCpuIdExt[0].edx = 0;

        /* 0x800000001: AMD only; shared feature bits are set dynamically. */
        memset(&pCPUM->aGuestCpuIdExt[1], 0, sizeof(pCPUM->aGuestCpuIdExt[1]));

        /* 0x800000002-4: Processor Name String Identifier. */
        pCPUM->aGuestCpuIdExt[2].eax = ((uint32_t *)szProcessor)[0];
        pCPUM->aGuestCpuIdExt[2].ebx = ((uint32_t *)szProcessor)[1];
        pCPUM->aGuestCpuIdExt[2].ecx = ((uint32_t *)szProcessor)[2];
        pCPUM->aGuestCpuIdExt[2].edx = ((uint32_t *)szProcessor)[3];
        pCPUM->aGuestCpuIdExt[3].eax = ((uint32_t *)szProcessor)[4];
        pCPUM->aGuestCpuIdExt[3].ebx = ((uint32_t *)szProcessor)[5];
        pCPUM->aGuestCpuIdExt[3].ecx = ((uint32_t *)szProcessor)[6];
        pCPUM->aGuestCpuIdExt[3].edx = ((uint32_t *)szProcessor)[7];
        pCPUM->aGuestCpuIdExt[4].eax = ((uint32_t *)szProcessor)[8];
        pCPUM->aGuestCpuIdExt[4].ebx = ((uint32_t *)szProcessor)[9];
        pCPUM->aGuestCpuIdExt[4].ecx = ((uint32_t *)szProcessor)[10];
        pCPUM->aGuestCpuIdExt[4].edx = ((uint32_t *)szProcessor)[11];

        /* 0x800000005-7 - reserved -> zero */
        memset(&pCPUM->aGuestCpuIdExt[5], 0, sizeof(pCPUM->aGuestCpuIdExt[5]));
        memset(&pCPUM->aGuestCpuIdExt[6], 0, sizeof(pCPUM->aGuestCpuIdExt[6]));
        memset(&pCPUM->aGuestCpuIdExt[7], 0, sizeof(pCPUM->aGuestCpuIdExt[7]));

        /* 0x800000008: only the max virtual and physical address size. */
        pCPUM->aGuestCpuIdExt[8].ecx = pCPUM->aGuestCpuIdExt[8].ebx = pCPUM->aGuestCpuIdExt[8].edx = 0;  /* reserved */
    }

    /*
     * Hide HTT, multicode, SMP, whatever.
     * (APIC-ID := 0 and #LogCpus := 0)
     */
    pCPUM->aGuestCpuIdStd[1].ebx &= 0x0000ffff;
#ifdef VBOX_WITH_MULTI_CORE
    if (    pVM->cCpus > 1
        &&  pCPUM->enmGuestCpuVendor != CPUMCPUVENDOR_SYNTHETIC)
    {
        /* If CPUID Fn0000_0001_EDX[HTT] = 1 then LogicalProcessorCount is the number of threads per CPU core times the number of CPU cores per processor */
        pCPUM->aGuestCpuIdStd[1].ebx |= (pVM->cCpus << 16);
        pCPUM->aGuestCpuIdStd[1].edx |= X86_CPUID_FEATURE_EDX_HTT;  /* necessary for hyper-threading *or* multi-core CPUs */
    }
#endif

    /* Cpuid 2:
     * Intel: Cache and TLB information
     * AMD:   Reserved
     * Safe to expose
     */

    /* Cpuid 3:
     * Intel: EAX, EBX - reserved
     *        ECX, EDX - Processor Serial Number if available, otherwise reserved
     * AMD:   Reserved
     * Safe to expose
     */
    if (!(pCPUM->aGuestCpuIdStd[1].edx & X86_CPUID_FEATURE_EDX_PSN))
        pCPUM->aGuestCpuIdStd[3].ecx = pCPUM->aGuestCpuIdStd[3].edx = 0;

    /* Cpuid 4:
     * Intel: Deterministic Cache Parameters Leaf
     *        Note: Depends on the ECX input! -> Feeling rather lazy now, so we just return 0
     * AMD:   Reserved
     * Safe to expose, except for EAX:
     *      Bits 25-14: Maximum number of addressable IDs for logical processors sharing this cache (see note)**
     *      Bits 31-26: Maximum number of processor cores in this physical package**
     * Note: These SMP values are constant regardless of ECX
     */
    pCPUM->aGuestCpuIdStd[4].ecx = pCPUM->aGuestCpuIdStd[4].edx = 0;
    pCPUM->aGuestCpuIdStd[4].eax = pCPUM->aGuestCpuIdStd[4].ebx = 0;
#ifdef VBOX_WITH_MULTI_CORE
    if (    pVM->cCpus > 1
        &&  pVM->cpum.s.enmGuestCpuVendor == CPUMCPUVENDOR_INTEL)
    {
        AssertReturn(pVM->cCpus <= 64, VERR_TOO_MANY_CPUS);
        /* One logical processor with possibly multiple cores. */
        /* See  http://www.intel.com/Assets/PDF/appnote/241618.pdf p. 29 */
        pCPUM->aGuestCpuIdStd[4].eax |= ((pVM->cCpus - 1) << 26);   /* 6 bits only -> 64 cores! */
    }
#endif

    /* Cpuid 5:     Monitor/mwait Leaf
     * Intel: ECX, EDX - reserved
     *        EAX, EBX - Smallest and largest monitor line size
     * AMD:   EDX - reserved
     *        EAX, EBX - Smallest and largest monitor line size
     *        ECX - extensions (ignored for now)
     * Safe to expose
     */
    if (!(pCPUM->aGuestCpuIdStd[1].ecx & X86_CPUID_FEATURE_ECX_MONITOR))
        pCPUM->aGuestCpuIdStd[5].eax = pCPUM->aGuestCpuIdStd[5].ebx = 0;

    pCPUM->aGuestCpuIdStd[5].ecx = pCPUM->aGuestCpuIdStd[5].edx = 0;

    /*
     * Determine the default.
     *
     * Intel returns values of the highest standard function, while AMD
     * returns zeros. VIA on the other hand seems to returning nothing or
     * perhaps some random garbage, we don't try to duplicate this behavior.
     */
    ASMCpuId(pCPUM->aGuestCpuIdStd[0].eax + 10,
             &pCPUM->GuestCpuIdDef.eax, &pCPUM->GuestCpuIdDef.ebx,
             &pCPUM->GuestCpuIdDef.ecx, &pCPUM->GuestCpuIdDef.edx);

    /* Cpuid 0x800000005 & 0x800000006 contain information about L1, L2 & L3 cache and TLB identifiers.
     * Safe to pass on to the guest.
     *
     * Intel: 0x800000005 reserved
     *        0x800000006 L2 cache information
     * AMD:   0x800000005 L1 cache information
     *        0x800000006 L2/L3 cache information
     */

    /* Cpuid 0x800000007:
     * AMD:               EAX, EBX, ECX - reserved
     *                    EDX: Advanced Power Management Information
     * Intel:             Reserved
     */
    if (pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000007))
    {
        Assert(pVM->cpum.s.enmGuestCpuVendor != CPUMCPUVENDOR_INVALID);

        pCPUM->aGuestCpuIdExt[7].eax = pCPUM->aGuestCpuIdExt[7].ebx = pCPUM->aGuestCpuIdExt[7].ecx = 0;

        if (pVM->cpum.s.enmGuestCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* Only expose the TSC invariant capability bit to the guest. */
            pCPUM->aGuestCpuIdExt[7].edx    &= 0
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TS
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_FID
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_VID
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TTP
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_TM
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_STC
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_MC
                                            //| X86_CPUID_AMD_ADVPOWER_EDX_HWPSTATE
#if 1
            /* We don't expose X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR, because newer Linux kernels blindly assume
             * that the AMD performance counters work if this is set for 64 bits guests. (can't really find a CPUID feature bit for them though)
             */
#else
                                            | X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR
#endif
                                            | 0;
        }
        else
            pCPUM->aGuestCpuIdExt[7].edx    = 0;
    }

    /* Cpuid 0x800000008:
     * AMD:               EBX, EDX - reserved
     *                    EAX: Virtual/Physical address Size
     *                    ECX: Number of cores + APICIdCoreIdSize
     * Intel:             EAX: Virtual/Physical address Size
     *                    EBX, ECX, EDX - reserved
     */
    if (pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000008))
    {
        /* Only expose the virtual and physical address sizes to the guest. (EAX completely) */
        pCPUM->aGuestCpuIdExt[8].ebx = pCPUM->aGuestCpuIdExt[8].edx = 0;  /* reserved */
        /* Set APICIdCoreIdSize to zero (use legacy method to determine the number of cores per cpu)
         * NC (0-7) Number of cores; 0 equals 1 core */
        pCPUM->aGuestCpuIdExt[8].ecx = 0;
#ifdef VBOX_WITH_MULTI_CORE
        if (    pVM->cCpus > 1
            &&  pVM->cpum.s.enmGuestCpuVendor == CPUMCPUVENDOR_AMD)
        {
            /* Legacy method to determine the number of cores. */
            pCPUM->aGuestCpuIdExt[1].ecx |= X86_CPUID_AMD_FEATURE_ECX_CMPL;
            pCPUM->aGuestCpuIdExt[8].ecx |= (pVM->cCpus - 1); /* NC: Number of CPU cores - 1; 8 bits */

        }
#endif
    }

    /** @cfgm{/CPUM/NT4LeafLimit, boolean, false}
     * Limit the number of standard CPUID leaves to 0..3 to prevent NT4 from
     * bugchecking with MULTIPROCESSOR_CONFIGURATION_NOT_SUPPORTED (0x3e).
     * This option corrsponds somewhat to IA32_MISC_ENABLES.BOOT_NT4[bit 22].
     */
    bool fNt4LeafLimit;
    rc = CFGMR3QueryBoolDef(pCpumCfg, "NT4LeafLimit", &fNt4LeafLimit, false); AssertRCReturn(rc, rc);
    if (fNt4LeafLimit)
        pCPUM->aGuestCpuIdStd[0].eax = 3;

    /*
     * Limit it the number of entries and fill the remaining with the defaults.
     *
     * The limits are masking off stuff about power saving and similar, this
     * is perhaps a bit crudely done as there is probably some relatively harmless
     * info too in these leaves (like words about having a constant TSC).
     */
    if (pCPUM->aGuestCpuIdStd[0].eax > 5)
        pCPUM->aGuestCpuIdStd[0].eax = 5;

    for (i = pCPUM->aGuestCpuIdStd[0].eax + 1; i < RT_ELEMENTS(pCPUM->aGuestCpuIdStd); i++)
        pCPUM->aGuestCpuIdStd[i] = pCPUM->GuestCpuIdDef;

    if (pCPUM->aGuestCpuIdExt[0].eax > UINT32_C(0x80000008))
        pCPUM->aGuestCpuIdExt[0].eax = UINT32_C(0x80000008);
    for (i = pCPUM->aGuestCpuIdExt[0].eax >= UINT32_C(0x80000000)
           ? pCPUM->aGuestCpuIdExt[0].eax - UINT32_C(0x80000000) + 1
           : 0;
         i < RT_ELEMENTS(pCPUM->aGuestCpuIdExt); i++)
        pCPUM->aGuestCpuIdExt[i] = pCPUM->GuestCpuIdDef;

    /*
     * Workaround for missing cpuid(0) patches when leaf 4 returns GuestCpuIdDef:
     * If we miss to patch a cpuid(0).eax then Linux tries to determine the number
     * of processors from (cpuid(4).eax >> 26) + 1.
     */
    if (pVM->cCpus == 1)
        pCPUM->aGuestCpuIdStd[4].eax = 0;

    /*
     * Centaur stuff (VIA).
     *
     * The important part here (we think) is to make sure the 0xc0000000
     * function returns 0xc0000001. As for the features, we don't currently
     * let on about any of those... 0xc0000002 seems to be some
     * temperature/hz/++ stuff, include it as well (static).
     */
    if (    pCPUM->aGuestCpuIdCentaur[0].eax >= UINT32_C(0xc0000000)
        &&  pCPUM->aGuestCpuIdCentaur[0].eax <= UINT32_C(0xc0000004))
    {
        pCPUM->aGuestCpuIdCentaur[0].eax = RT_MIN(pCPUM->aGuestCpuIdCentaur[0].eax, UINT32_C(0xc0000002));
        pCPUM->aGuestCpuIdCentaur[1].edx = 0; /* all features hidden */
        for (i = pCPUM->aGuestCpuIdCentaur[0].eax - UINT32_C(0xc0000000);
             i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur);
             i++)
            pCPUM->aGuestCpuIdCentaur[i] = pCPUM->GuestCpuIdDef;
    }
    else
        for (i = 0; i < RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur); i++)
            pCPUM->aGuestCpuIdCentaur[i] = pCPUM->GuestCpuIdDef;


    /*
     * Load CPUID overrides from configuration.
     * Note: Kind of redundant now, but allows unchanged overrides
     */
    /** @cfgm{CPUM/CPUID/[000000xx|800000xx|c000000x]/[eax|ebx|ecx|edx],32-bit}
     * Overrides the CPUID leaf values. */
    PCFGMNODE pOverrideCfg = CFGMR3GetChild(pCpumCfg, "CPUID");
    rc = cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x00000000), &pCPUM->aGuestCpuIdStd[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdStd),     pOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0x80000000), &pCPUM->aGuestCpuIdExt[0],     RT_ELEMENTS(pCPUM->aGuestCpuIdExt),     pOverrideCfg);
    AssertRCReturn(rc, rc);
    rc = cpumR3CpuIdInitLoadOverrideSet(UINT32_C(0xc0000000), &pCPUM->aGuestCpuIdCentaur[0], RT_ELEMENTS(pCPUM->aGuestCpuIdCentaur), pOverrideCfg);
    AssertRCReturn(rc, rc);

    /*
     * Check if PAE was explicitely enabled by the user.
     */
    bool fEnable;
    rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "EnablePAE", &fEnable, false);    AssertRCReturn(rc, rc);
    if (fEnable)
        CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);

    /*
     * Log the cpuid and we're good.
     */
    RTCPUSET OnlineSet;
    LogRel(("Logical host processors: %d, processor active mask: %016RX64\n",
            (int)RTMpGetCount(), RTCpuSetToU64(RTMpGetOnlineSet(&OnlineSet)) ));
    LogRel(("************************* CPUID dump ************************\n"));
    DBGFR3Info(pVM, "cpuid", "verbose", DBGFR3InfoLogRelHlp());
    LogRel(("\n"));
    DBGFR3InfoLog(pVM, "cpuid", "verbose"); /* macro */
    LogRel(("******************** End of CPUID dump **********************\n"));
    return VINF_SUCCESS;
}




/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * The CPUM will update the addresses used by the switcher.
 *
 * @param   pVM     The VM.
 */
VMMR3DECL(void) CPUMR3Relocate(PVM pVM)
{
    LogFlow(("CPUMR3Relocate\n"));
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        /*
         * Switcher pointers.
         */
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->cpum.s.pHyperCoreRC = MMHyperCCToRC(pVM, pVCpu->cpum.s.pHyperCoreR3);
        Assert(pVCpu->cpum.s.pHyperCoreRC != NIL_RTRCPTR);
    }
}


/**
 * Terminates the CPUM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CPUMR3Term(PVM pVM)
{
    CPUMR3TermCPU(pVM);
    return 0;
}


/**
 * Terminates the per-VCPU CPUM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) CPUMR3TermCPU(PVM pVM)
{
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU   pVCpu = &pVM->aCpus[i];
        PCPUMCTX pCtx  = CPUMQueryGuestCtxPtr(pVCpu);

        memset(pVCpu->cpum.s.aMagic, 0, sizeof(pVCpu->cpum.s.aMagic));
        pVCpu->cpum.s.uMagic     = 0;
        pCtx->dr[5]              = 0;
    }
#endif
    return 0;
}

VMMR3DECL(void) CPUMR3ResetCpu(PVMCPU pVCpu)
{
    /* @todo anything different for VCPU > 0? */
    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    /*
     * Initialize everything to ZERO first.
     */
    uint32_t    fUseFlags =  pVCpu->cpum.s.fUseFlags & ~CPUM_USED_FPU_SINCE_REM;
    memset(pCtx, 0, sizeof(*pCtx));
    pVCpu->cpum.s.fUseFlags  = fUseFlags;

    pCtx->cr0                       = X86_CR0_CD | X86_CR0_NW | X86_CR0_ET;  //0x60000010
    pCtx->eip                       = 0x0000fff0;
    pCtx->edx                       = 0x00000600;   /* P6 processor */
    pCtx->eflags.Bits.u1Reserved0   = 1;

    pCtx->cs                        = 0xf000;
    pCtx->csHid.u64Base             = UINT64_C(0xffff0000);
    pCtx->csHid.u32Limit            = 0x0000ffff;
    pCtx->csHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->csHid.Attr.n.u1Present    = 1;
    pCtx->csHid.Attr.n.u4Type       = X86_SEL_TYPE_READ | X86_SEL_TYPE_CODE;

    pCtx->dsHid.u32Limit            = 0x0000ffff;
    pCtx->dsHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->dsHid.Attr.n.u1Present    = 1;
    pCtx->dsHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->esHid.u32Limit            = 0x0000ffff;
    pCtx->esHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->esHid.Attr.n.u1Present    = 1;
    pCtx->esHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->fsHid.u32Limit            = 0x0000ffff;
    pCtx->fsHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->fsHid.Attr.n.u1Present    = 1;
    pCtx->fsHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->gsHid.u32Limit            = 0x0000ffff;
    pCtx->gsHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->gsHid.Attr.n.u1Present    = 1;
    pCtx->gsHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->ssHid.u32Limit            = 0x0000ffff;
    pCtx->ssHid.Attr.n.u1Present    = 1;
    pCtx->ssHid.Attr.n.u1DescType   = 1; /* code/data segment */
    pCtx->ssHid.Attr.n.u4Type       = X86_SEL_TYPE_RW;

    pCtx->idtr.cbIdt                = 0xffff;
    pCtx->gdtr.cbGdt                = 0xffff;

    pCtx->ldtrHid.u32Limit          = 0xffff;
    pCtx->ldtrHid.Attr.n.u1Present  = 1;
    pCtx->ldtrHid.Attr.n.u4Type     = X86_SEL_TYPE_SYS_LDT;

    pCtx->trHid.u32Limit            = 0xffff;
    pCtx->trHid.Attr.n.u1Present    = 1;
    pCtx->trHid.Attr.n.u4Type       = X86_SEL_TYPE_SYS_286_TSS_BUSY;

    pCtx->dr[6]                     = X86_DR6_INIT_VAL;
    pCtx->dr[7]                     = X86_DR7_INIT_VAL;

    pCtx->fpu.FTW                   = 0xff;         /* All tags are set, i.e. the regs are empty. */
    pCtx->fpu.FCW                   = 0x37f;

    /* Intel 64 and IA-32 Architectures Software Developer's Manual Volume 3A, Table 8-1. IA-32 Processor States Following Power-up, Reset, or INIT */
    pCtx->fpu.MXCSR                 = 0x1F80;

    /* Init PAT MSR */
    pCtx->msrPAT                    = UINT64_C(0x0007040600070406); /** @todo correct? */

    /* Reset EFER; see AMD64 Architecture Programmer's Manual Volume 2: Table 14-1. Initial Processor State
    * The Intel docs don't mention it.
    */
    pCtx->msrEFER                   = 0;
}

/**
 * Resets the CPU.
 *
 * @returns VINF_SUCCESS.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(void) CPUMR3Reset(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        CPUMR3ResetCpu(&pVM->aCpus[i]);

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(&pVM->aCpus[i]);

        /* Magic marker for searching in crash dumps. */
        strcpy((char *)pVM->aCpus[i].cpum.s.aMagic, "CPUMCPU Magic");
        pVM->aCpus[i].cpum.s.uMagic     = UINT64_C(0xDEADBEEFDEADBEEF);
        pCtx->dr[5]                     = UINT64_C(0xDEADBEEFDEADBEEF);
#endif
    }
}

#ifdef VBOX_WITH_LIVE_MIGRATION

/**
 * Called both in pass 0 and the final pass.
 *
 * @param   pVM                 The VM handle.
 * @param   pSSM                The saved state handle.
 */
static void cpumR3SaveCpuId(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save all the CPU ID leaves here so we can check them for compatability
     * upon loading.
     */
    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], sizeof(pVM->cpum.s.aGuestCpuIdStd));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3PutMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /*
     * Save a good portion of the raw CPU IDs as well as they may come in
     * handy when validating features for raw mode.
     */
    CPUMCPUID   aRawStd[8];
    for (unsigned i = 0; i < RT_ELEMENTS(aRawStd); i++)
        ASMCpuId(i, &aRawStd[i].eax, &aRawStd[i].ebx, &aRawStd[i].ecx, &aRawStd[i].edx);
    SSMR3PutU32(pSSM, RT_ELEMENTS(aRawStd));
    SSMR3PutMem(pSSM, &aRawStd[0], sizeof(aRawStd));

    CPUMCPUID   aRawExt[16];
    for (unsigned i = 0; i < RT_ELEMENTS(aRawExt); i++)
        ASMCpuId(i | UINT32_C(0x80000000), &aRawExt[i].eax, &aRawExt[i].ebx, &aRawExt[i].ecx, &aRawExt[i].edx);
    SSMR3PutU32(pSSM, RT_ELEMENTS(aRawExt));
    SSMR3PutMem(pSSM, &aRawExt[0], sizeof(aRawExt));
}


/**
 * Loads the CPU ID leaves saved by pass 0.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM handle.
 * @param   pSSM                The saved state handle.
 * @param   uVersion            The format version.
 */
static int cpumR3LoadCpuId(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion)
{
    AssertMsgReturn(uVersion >= CPUM_SAVED_STATE_VERSION, ("%u\n", uVersion), VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION);

    /*
     * Load them into stack buffers first.
     */
    CPUMCPUID   aGuestCpuIdStd[RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd)];
    uint32_t    cGuestCpuIdStd;
    int rc = SSMR3GetU32(pSSM, &cGuestCpuIdStd); AssertRCReturn(rc, rc);
    if (cGuestCpuIdStd > RT_ELEMENTS(aGuestCpuIdStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aGuestCpuIdStd[0], cGuestCpuIdStd * sizeof(aGuestCpuIdStd[0]));

    CPUMCPUID   aGuestCpuIdExt[RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt)];
    uint32_t    cGuestCpuIdExt;
    rc = SSMR3GetU32(pSSM, &cGuestCpuIdExt); AssertRCReturn(rc, rc);
    if (cGuestCpuIdExt > RT_ELEMENTS(aGuestCpuIdExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aGuestCpuIdExt[0], cGuestCpuIdExt * sizeof(aGuestCpuIdExt[0]));

    CPUMCPUID   aGuestCpuIdCentaur[RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur)];
    uint32_t    cGuestCpuIdCentaur;
    rc = SSMR3GetU32(pSSM, &cGuestCpuIdCentaur); AssertRCReturn(rc, rc);
    if (cGuestCpuIdCentaur > RT_ELEMENTS(aGuestCpuIdCentaur))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aGuestCpuIdCentaur[0], cGuestCpuIdCentaur * sizeof(aGuestCpuIdCentaur[0]));

    CPUMCPUID   GuestCpuIdDef;
    rc = SSMR3GetMem(pSSM, &GuestCpuIdDef, sizeof(GuestCpuIdDef));
    AssertRCReturn(rc, rc);

    CPUMCPUID   aRawStd[8];
    uint32_t    cRawStd;
    rc = SSMR3GetU32(pSSM, &cRawStd); AssertRCReturn(rc, rc);
    if (cRawStd > RT_ELEMENTS(aRawStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &aRawStd[0], cRawStd * sizeof(aRawStd[0]));

    CPUMCPUID   aRawExt[16];
    uint32_t    cRawExt;
    rc = SSMR3GetU32(pSSM, &cRawExt); AssertRCReturn(rc, rc);
    if (cRawExt > RT_ELEMENTS(aRawExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    rc = SSMR3GetMem(pSSM, &aRawExt[0], cRawExt * sizeof(aRawExt[0]));
    AssertRCReturn(rc, rc);

    /*
     * Note that we support restoring less than the current amount of standard
     * leaves because we've been allowed more is newer version of VBox.
     *
     * So, pad new entries with the default.
     */
    for (uint32_t i = cGuestCpuIdStd; i < RT_ELEMENTS(aGuestCpuIdStd); i++)
        aGuestCpuIdStd[i] = GuestCpuIdDef;

    for (uint32_t i = cGuestCpuIdExt; i < RT_ELEMENTS(aGuestCpuIdExt); i++)
        aGuestCpuIdExt[i] = GuestCpuIdDef;

    for (uint32_t i = cGuestCpuIdCentaur; i < RT_ELEMENTS(aGuestCpuIdCentaur); i++)
        aGuestCpuIdCentaur[i] = GuestCpuIdDef;

    for (uint32_t i = cRawStd; i < RT_ELEMENTS(aRawStd); i++)
        ASMCpuId(i, &aRawStd[i].eax, &aRawStd[i].ebx, &aRawStd[i].ecx, &aRawStd[i].edx);

    for (uint32_t i = cRawExt; i < RT_ELEMENTS(aRawExt); i++)
        ASMCpuId(i | UINT32_C(0x80000000), &aRawExt[i].eax, &aRawExt[i].ebx, &aRawExt[i].ecx, &aRawExt[i].edx);

    /*
     * Get the raw CPU IDs for the current host.
     */
    CPUMCPUID   aHostRawStd[8];
    for (unsigned i = 0; i < RT_ELEMENTS(aHostRawStd); i++)
        ASMCpuId(i, &aHostRawStd[i].eax, &aHostRawStd[i].ebx, &aHostRawStd[i].ecx, &aHostRawStd[i].edx);

    CPUMCPUID   aHostRawExt[16];
    for (unsigned i = 0; i < RT_ELEMENTS(aHostRawExt); i++)
        ASMCpuId(i | UINT32_C(0x80000000), &aHostRawExt[i].eax, &aHostRawExt[i].ebx, &aHostRawExt[i].ecx, &aHostRawExt[i].edx);

    /*
     * Now for the fun part...
     */


    /*
     * We're good, commit the CPU ID leaves.
     */
    memcmp(&pVM->cpum.s.aGuestCpuIdStd[0],     &aGuestCpuIdStd[0],     sizeof(aGuestCpuIdStd));
    memcmp(&pVM->cpum.s.aGuestCpuIdExt[0],     &aGuestCpuIdExt[0],     sizeof(aGuestCpuIdExt));
    memcmp(&pVM->cpum.s.aGuestCpuIdCentaur[0], &aGuestCpuIdCentaur[0], sizeof(aGuestCpuIdCentaur));
    pVM->cpum.s.GuestCpuIdDef = GuestCpuIdDef;

    return VINF_SUCCESS;
}


/**
 * Pass 0 live exec callback.
 *
 * @returns VINF_SSM_DONT_CALL_AGAIN.
 * @param   pVM                 The VM handle.
 * @param   pSSM                The saved state handle.
 * @param   uPass               The pass (0).
 */
static DECLCALLBACK(int) cpumR3LiveExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uPass)
{
    AssertReturn(uPass == 0, VERR_INTERNAL_ERROR_4);
    cpumR3SaveCpuId(pVM, pSSM);
    return VINF_SSM_DONT_CALL_AGAIN;
}

#endif /* VBOX_WITH_LIVE_MIGRATION */

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) cpumR3SaveExec(PVM pVM, PSSMHANDLE pSSM)
{
    /*
     * Save.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        SSMR3PutMem(pSSM, &pVCpu->cpum.s.Hyper, sizeof(pVCpu->cpum.s.Hyper));
    }

    SSMR3PutU32(pSSM, pVM->cCpus);
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        SSMR3PutMem(pSSM, &pVCpu->cpum.s.Guest, sizeof(pVCpu->cpum.s.Guest));
        SSMR3PutU32(pSSM, pVCpu->cpum.s.fUseFlags);
        SSMR3PutU32(pSSM, pVCpu->cpum.s.fChanged);
        SSMR3PutMem(pSSM, &pVCpu->cpum.s.GuestMsr, sizeof(pVCpu->cpum.s.GuestMsr));
    }

#ifdef VBOX_WITH_LIVE_MIGRATION
    cpumR3SaveCpuId(pVM, pSSM);
    return VINF_SUCCESS;
#else

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], sizeof(pVM->cpum.s.aGuestCpuIdStd));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    SSMR3PutU32(pSSM, RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur));
    SSMR3PutMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3PutMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /* Add the cpuid for checking that the cpu is unchanged. */
    uint32_t au32CpuId[8] = {0};
    ASMCpuId(0, &au32CpuId[0], &au32CpuId[1], &au32CpuId[2], &au32CpuId[3]);
    ASMCpuId(1, &au32CpuId[4], &au32CpuId[5], &au32CpuId[6], &au32CpuId[7]);
    return SSMR3PutMem(pSSM, &au32CpuId[0], sizeof(au32CpuId));
#endif
}


/**
 * Load a version 1.6 CPUMCTX structure.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pCpumctx16      Version 1.6 CPUMCTX
 */
static void cpumR3LoadCPUM1_6(PVM pVM, CPUMCTX_VER1_6 *pCpumctx16)
{
#define CPUMCTX16_LOADREG(RegName) \
        pVM->aCpus[0].cpum.s.Guest.RegName = pCpumctx16->RegName;

#define CPUMCTX16_LOADDRXREG(RegName) \
        pVM->aCpus[0].cpum.s.Guest.dr[RegName] = pCpumctx16->dr##RegName;

#define CPUMCTX16_LOADHIDREG(RegName) \
        pVM->aCpus[0].cpum.s.Guest.RegName##Hid.u64Base  = pCpumctx16->RegName##Hid.u32Base; \
        pVM->aCpus[0].cpum.s.Guest.RegName##Hid.u32Limit = pCpumctx16->RegName##Hid.u32Limit; \
        pVM->aCpus[0].cpum.s.Guest.RegName##Hid.Attr     = pCpumctx16->RegName##Hid.Attr;

#define CPUMCTX16_LOADSEGREG(RegName) \
        pVM->aCpus[0].cpum.s.Guest.RegName = pCpumctx16->RegName; \
        CPUMCTX16_LOADHIDREG(RegName);

    pVM->aCpus[0].cpum.s.Guest.fpu = pCpumctx16->fpu;

    CPUMCTX16_LOADREG(rax);
    CPUMCTX16_LOADREG(rbx);
    CPUMCTX16_LOADREG(rcx);
    CPUMCTX16_LOADREG(rdx);
    CPUMCTX16_LOADREG(rdi);
    CPUMCTX16_LOADREG(rsi);
    CPUMCTX16_LOADREG(rbp);
    CPUMCTX16_LOADREG(esp);
    CPUMCTX16_LOADREG(rip);
    CPUMCTX16_LOADREG(rflags);

    CPUMCTX16_LOADSEGREG(cs);
    CPUMCTX16_LOADSEGREG(ds);
    CPUMCTX16_LOADSEGREG(es);
    CPUMCTX16_LOADSEGREG(fs);
    CPUMCTX16_LOADSEGREG(gs);
    CPUMCTX16_LOADSEGREG(ss);

    CPUMCTX16_LOADREG(r8);
    CPUMCTX16_LOADREG(r9);
    CPUMCTX16_LOADREG(r10);
    CPUMCTX16_LOADREG(r11);
    CPUMCTX16_LOADREG(r12);
    CPUMCTX16_LOADREG(r13);
    CPUMCTX16_LOADREG(r14);
    CPUMCTX16_LOADREG(r15);

    CPUMCTX16_LOADREG(cr0);
    CPUMCTX16_LOADREG(cr2);
    CPUMCTX16_LOADREG(cr3);
    CPUMCTX16_LOADREG(cr4);

    CPUMCTX16_LOADDRXREG(0);
    CPUMCTX16_LOADDRXREG(1);
    CPUMCTX16_LOADDRXREG(2);
    CPUMCTX16_LOADDRXREG(3);
    CPUMCTX16_LOADDRXREG(4);
    CPUMCTX16_LOADDRXREG(5);
    CPUMCTX16_LOADDRXREG(6);
    CPUMCTX16_LOADDRXREG(7);

    pVM->aCpus[0].cpum.s.Guest.gdtr.cbGdt   = pCpumctx16->gdtr.cbGdt;
    pVM->aCpus[0].cpum.s.Guest.gdtr.pGdt    = pCpumctx16->gdtr.pGdt;
    pVM->aCpus[0].cpum.s.Guest.idtr.cbIdt   = pCpumctx16->idtr.cbIdt;
    pVM->aCpus[0].cpum.s.Guest.idtr.pIdt    = pCpumctx16->idtr.pIdt;

    CPUMCTX16_LOADREG(ldtr);
    CPUMCTX16_LOADREG(tr);

    pVM->aCpus[0].cpum.s.Guest.SysEnter     = pCpumctx16->SysEnter;

    CPUMCTX16_LOADREG(msrEFER);
    CPUMCTX16_LOADREG(msrSTAR);
    CPUMCTX16_LOADREG(msrPAT);
    CPUMCTX16_LOADREG(msrLSTAR);
    CPUMCTX16_LOADREG(msrCSTAR);
    CPUMCTX16_LOADREG(msrSFMASK);
    CPUMCTX16_LOADREG(msrKERNELGSBASE);

    CPUMCTX16_LOADHIDREG(ldtr);
    CPUMCTX16_LOADHIDREG(tr);

#undef CPUMCTX16_LOADSEGREG
#undef CPUMCTX16_LOADHIDREG
#undef CPUMCTX16_LOADDRXREG
#undef CPUMCTX16_LOADREG
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) cpumR3LoadExec(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    /*
     * Validate version.
     */
    if (    uVersion != CPUM_SAVED_STATE_VERSION
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER3_0
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER2_0
        &&  uVersion != CPUM_SAVED_STATE_VERSION_VER1_6)
    {
        AssertMsgFailed(("cpumR3LoadExec: Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }

    if (uPass == SSM_PASS_FINAL)
    {
        /*
         * Set the size of RTGCPTR for SSMR3GetGCPtr. (Only necessary for
         * really old SSM file versions.)
         */
        if (uVersion == CPUM_SAVED_STATE_VERSION_VER1_6)
            SSMR3SetGCPtrSize(pSSM, sizeof(RTGCPTR32));
        else if (uVersion <= CPUM_SAVED_STATE_VERSION_VER3_0)
            SSMR3SetGCPtrSize(pSSM, HC_ARCH_BITS == 32 ? sizeof(RTGCPTR32) : sizeof(RTGCPTR));

        /*
         * Restore.
         */
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU   pVCpu = &pVM->aCpus[i];
            uint32_t uCR3  = pVCpu->cpum.s.Hyper.cr3;
            uint32_t uESP  = pVCpu->cpum.s.Hyper.esp; /* see VMMR3Relocate(). */

            SSMR3GetMem(pSSM, &pVCpu->cpum.s.Hyper, sizeof(pVCpu->cpum.s.Hyper));
            pVCpu->cpum.s.Hyper.cr3 = uCR3;
            pVCpu->cpum.s.Hyper.esp = uESP;
        }

        if (uVersion == CPUM_SAVED_STATE_VERSION_VER1_6)
        {
            CPUMCTX_VER1_6 cpumctx16;
            memset(&pVM->aCpus[0].cpum.s.Guest, 0, sizeof(pVM->aCpus[0].cpum.s.Guest));
            SSMR3GetMem(pSSM, &cpumctx16, sizeof(cpumctx16));

            /* Save the old cpumctx state into the new one. */
            cpumR3LoadCPUM1_6(pVM, &cpumctx16);

            SSMR3GetU32(pSSM, &pVM->aCpus[0].cpum.s.fUseFlags);
            SSMR3GetU32(pSSM, &pVM->aCpus[0].cpum.s.fChanged);
        }
        else
        {
            if (uVersion >= CPUM_SAVED_STATE_VERSION_VER2_1_NOMSR)
            {
                uint32_t cCpus;
                int rc = SSMR3GetU32(pSSM, &cCpus); AssertRCReturn(rc, rc);
                AssertLogRelMsgReturn(cCpus == pVM->cCpus, ("Mismatching CPU counts: saved: %u; configured: %u \n", cCpus, pVM->cCpus),
                                      VERR_SSM_UNEXPECTED_DATA);
            }
            AssertLogRelMsgReturn(   uVersion != CPUM_SAVED_STATE_VERSION_VER2_0
                                  || pVM->cCpus == 1,
                                  ("cCpus=%u\n", pVM->cCpus),
                                  VERR_SSM_UNEXPECTED_DATA);

            for (VMCPUID i = 0; i < pVM->cCpus; i++)
            {
                SSMR3GetMem(pSSM, &pVM->aCpus[i].cpum.s.Guest, sizeof(pVM->aCpus[i].cpum.s.Guest));
                SSMR3GetU32(pSSM, &pVM->aCpus[i].cpum.s.fUseFlags);
                SSMR3GetU32(pSSM, &pVM->aCpus[i].cpum.s.fChanged);
                if (uVersion >= CPUM_SAVED_STATE_VERSION_VER3_0)
                    SSMR3GetMem(pSSM, &pVM->aCpus[i].cpum.s.GuestMsr, sizeof(pVM->aCpus[i].cpum.s.GuestMsr));
            }
        }
    }

#ifdef VBOX_WITH_LIVE_MIGRATION
    /*
     * Guest CPU config and CPUID.
     */
    /** @todo config. */

    if (uVersion > CPUM_SAVED_STATE_VERSION_VER3_0)
        return cpumR3LoadCpuId(pVM, pSSM, uVersion);

    /** @todo Merge the code below into cpumR3LoadCpuId when we've found out what is
     *        actually required. */
#endif

    /*
     * Restore the CPUID leaves.
     *
     * Note that we support restoring less than the current amount of standard
     * leaves because we've been allowed more is newer version of VBox.
     */
    uint32_t cElements;
    int rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements > RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdStd[0], cElements*sizeof(pVM->cpum.s.aGuestCpuIdStd[0]));

    rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdExt[0], sizeof(pVM->cpum.s.aGuestCpuIdExt));

    rc = SSMR3GetU32(pSSM, &cElements); AssertRCReturn(rc, rc);
    if (cElements != RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur))
        return VERR_SSM_DATA_UNIT_FORMAT_CHANGED;
    SSMR3GetMem(pSSM, &pVM->cpum.s.aGuestCpuIdCentaur[0], sizeof(pVM->cpum.s.aGuestCpuIdCentaur));

    SSMR3GetMem(pSSM, &pVM->cpum.s.GuestCpuIdDef, sizeof(pVM->cpum.s.GuestCpuIdDef));

    /*
     * Check that the basic cpuid id information is unchanged.
     */
    /** @todo we should check the 64 bits capabilities too! */
    uint32_t au32CpuId[8] = {0};
    ASMCpuId(0, &au32CpuId[0], &au32CpuId[1], &au32CpuId[2], &au32CpuId[3]);
    ASMCpuId(1, &au32CpuId[4], &au32CpuId[5], &au32CpuId[6], &au32CpuId[7]);
    uint32_t au32CpuIdSaved[8];
    rc = SSMR3GetMem(pSSM, &au32CpuIdSaved[0], sizeof(au32CpuIdSaved));
    if (RT_SUCCESS(rc))
    {
        /* Ignore CPU stepping. */
        au32CpuId[4]      &=  0xfffffff0;
        au32CpuIdSaved[4] &=  0xfffffff0;

        /* Ignore APIC ID (AMD specs). */
        au32CpuId[5]      &= ~0xff000000;
        au32CpuIdSaved[5] &= ~0xff000000;

        /* Ignore the number of Logical CPUs (AMD specs). */
        au32CpuId[5]      &= ~0x00ff0000;
        au32CpuIdSaved[5] &= ~0x00ff0000;

        /* Ignore some advanced capability bits, that we don't expose to the guest. */
        au32CpuId[6]      &= ~(   X86_CPUID_FEATURE_ECX_DTES64
                               |  X86_CPUID_FEATURE_ECX_VMX
                               |  X86_CPUID_FEATURE_ECX_SMX
                               |  X86_CPUID_FEATURE_ECX_EST
                               |  X86_CPUID_FEATURE_ECX_TM2
                               |  X86_CPUID_FEATURE_ECX_CNTXID
                               |  X86_CPUID_FEATURE_ECX_TPRUPDATE
                               |  X86_CPUID_FEATURE_ECX_PDCM
                               |  X86_CPUID_FEATURE_ECX_DCA
                               |  X86_CPUID_FEATURE_ECX_X2APIC
                              );
        au32CpuIdSaved[6] &= ~(   X86_CPUID_FEATURE_ECX_DTES64
                               |  X86_CPUID_FEATURE_ECX_VMX
                               |  X86_CPUID_FEATURE_ECX_SMX
                               |  X86_CPUID_FEATURE_ECX_EST
                               |  X86_CPUID_FEATURE_ECX_TM2
                               |  X86_CPUID_FEATURE_ECX_CNTXID
                               |  X86_CPUID_FEATURE_ECX_TPRUPDATE
                               |  X86_CPUID_FEATURE_ECX_PDCM
                               |  X86_CPUID_FEATURE_ECX_DCA
                               |  X86_CPUID_FEATURE_ECX_X2APIC
                              );

        /* Make sure we don't forget to update the masks when enabling
         * features in the future.
         */
        AssertRelease(!(pVM->cpum.s.aGuestCpuIdStd[1].ecx &
                              (   X86_CPUID_FEATURE_ECX_DTES64
                               |  X86_CPUID_FEATURE_ECX_VMX
                               |  X86_CPUID_FEATURE_ECX_SMX
                               |  X86_CPUID_FEATURE_ECX_EST
                               |  X86_CPUID_FEATURE_ECX_TM2
                               |  X86_CPUID_FEATURE_ECX_CNTXID
                               |  X86_CPUID_FEATURE_ECX_TPRUPDATE
                               |  X86_CPUID_FEATURE_ECX_PDCM
                               |  X86_CPUID_FEATURE_ECX_DCA
                               |  X86_CPUID_FEATURE_ECX_X2APIC
                              )));
        /* do the compare */
        if (memcmp(au32CpuIdSaved, au32CpuId, sizeof(au32CpuIdSaved)))
        {
            if (SSMR3HandleGetAfter(pSSM) == SSMAFTER_DEBUG_IT)
                LogRel(("cpumR3LoadExec: CpuId mismatch! (ignored due to SSMAFTER_DEBUG_IT)\n"
                        "Saved=%.*Rhxs\n"
                        "Real =%.*Rhxs\n",
                        sizeof(au32CpuIdSaved), au32CpuIdSaved,
                        sizeof(au32CpuId), au32CpuId));
            else
            {
                LogRel(("cpumR3LoadExec: CpuId mismatch!\n"
                        "Saved=%.*Rhxs\n"
                        "Real =%.*Rhxs\n",
                        sizeof(au32CpuIdSaved), au32CpuIdSaved,
                        sizeof(au32CpuId), au32CpuId));
                rc = VERR_SSM_LOAD_CPUID_MISMATCH;
            }
        }
    }

    return rc;
}


/**
 * Formats the EFLAGS value into mnemonics.
 *
 * @param   pszEFlags   Where to write the mnemonics. (Assumes sufficient buffer space.)
 * @param   efl         The EFLAGS value.
 */
static void cpumR3InfoFormatFlags(char *pszEFlags, uint32_t efl)
{
    /*
     * Format the flags.
     */
    static const struct
    {
        const char *pszSet; const char *pszClear; uint32_t fFlag;
    }   s_aFlags[] =
    {
        { "vip",NULL, X86_EFL_VIP },
        { "vif",NULL, X86_EFL_VIF },
        { "ac", NULL, X86_EFL_AC },
        { "vm", NULL, X86_EFL_VM },
        { "rf", NULL, X86_EFL_RF },
        { "nt", NULL, X86_EFL_NT },
        { "ov", "nv", X86_EFL_OF },
        { "dn", "up", X86_EFL_DF },
        { "ei", "di", X86_EFL_IF },
        { "tf", NULL, X86_EFL_TF },
        { "nt", "pl", X86_EFL_SF },
        { "nz", "zr", X86_EFL_ZF },
        { "ac", "na", X86_EFL_AF },
        { "po", "pe", X86_EFL_PF },
        { "cy", "nc", X86_EFL_CF },
    };
    char *psz = pszEFlags;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFlags); i++)
    {
        const char *pszAdd = s_aFlags[i].fFlag & efl ? s_aFlags[i].pszSet : s_aFlags[i].pszClear;
        if (pszAdd)
        {
            strcpy(psz, pszAdd);
            psz += strlen(pszAdd);
            *psz++ = ' ';
        }
    }
    psz[-1] = '\0';
}


/**
 * Formats a full register dump.
 *
 * @param   pVM         VM Handle.
 * @param   pCtx        The context to format.
 * @param   pCtxCore    The context core to format.
 * @param   pHlp        Output functions.
 * @param   enmType     The dump type.
 * @param   pszPrefix   Register name prefix.
 */
static void cpumR3InfoOne(PVM pVM, PCPUMCTX pCtx, PCCPUMCTXCORE pCtxCore, PCDBGFINFOHLP pHlp, CPUMDUMPTYPE enmType, const char *pszPrefix)
{
    /*
     * Format the EFLAGS.
     */
    uint32_t efl = pCtxCore->eflags.u32;
    char szEFlags[80];
    cpumR3InfoFormatFlags(&szEFlags[0], efl);

    /*
     * Format the registers.
     */
    switch (enmType)
    {
        case CPUMDUMPTYPE_TERSE:
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
                pHlp->pfnPrintf(pHlp,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x                %seflags=%08x\n",
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                    pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, efl);
            else
                pHlp->pfnPrintf(pHlp,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x                %seflags=%08x\n",
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                    pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, efl);
            break;

        case CPUMDUMPTYPE_DEFAULT:
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
                pHlp->pfnPrintf(pHlp,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %str=%04x      %seflags=%08x\n"
                    "%scr0=%08RX64 %scr2=%08RX64 %scr3=%08RX64 %scr4=%08RX64 %sgdtr=%016RX64:%04x %sldtr=%04x\n"
                    ,
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                    pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, (RTSEL)pCtx->tr, pszPrefix, efl,
                    pszPrefix, pCtx->cr0, pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3, pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, (RTSEL)pCtx->ldtr);
            else
                pHlp->pfnPrintf(pHlp,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs=%04x %sss=%04x %sds=%04x %ses=%04x %sfs=%04x %sgs=%04x %str=%04x      %seflags=%08x\n"
                    "%scr0=%08RX64 %scr2=%08RX64 %scr3=%08RX64 %scr4=%08RX64 %sgdtr=%08RX64:%04x %sldtr=%04x\n"
                    ,
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pszPrefix, (RTSEL)pCtxCore->ss, pszPrefix, (RTSEL)pCtxCore->ds, pszPrefix, (RTSEL)pCtxCore->es,
                    pszPrefix, (RTSEL)pCtxCore->fs, pszPrefix, (RTSEL)pCtxCore->gs, pszPrefix, (RTSEL)pCtx->tr, pszPrefix, efl,
                    pszPrefix, pCtx->cr0, pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3, pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, (RTSEL)pCtx->ldtr);
            break;

        case CPUMDUMPTYPE_VERBOSE:
            if (CPUMIsGuestIn64BitCodeEx(pCtx))
                pHlp->pfnPrintf(pHlp,
                    "%srax=%016RX64 %srbx=%016RX64 %srcx=%016RX64 %srdx=%016RX64\n"
                    "%srsi=%016RX64 %srdi=%016RX64 %sr8 =%016RX64 %sr9 =%016RX64\n"
                    "%sr10=%016RX64 %sr11=%016RX64 %sr12=%016RX64 %sr13=%016RX64\n"
                    "%sr14=%016RX64 %sr15=%016RX64\n"
                    "%srip=%016RX64 %srsp=%016RX64 %srbp=%016RX64 %siopl=%d %*s\n"
                    "%scs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sds={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%ses={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sfs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sgs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%sss={%04x base=%016RX64 limit=%08x flags=%08x}\n"
                    "%scr0=%016RX64 %scr2=%016RX64 %scr3=%016RX64 %scr4=%016RX64\n"
                    "%sdr0=%016RX64 %sdr1=%016RX64 %sdr2=%016RX64 %sdr3=%016RX64\n"
                    "%sdr4=%016RX64 %sdr5=%016RX64 %sdr6=%016RX64 %sdr7=%016RX64\n"
                    "%sgdtr=%016RX64:%04x  %sidtr=%016RX64:%04x  %seflags=%08x\n"
                    "%sldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%str  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%sSysEnter={cs=%04llx eip=%016RX64 esp=%016RX64}\n"
                    ,
                    pszPrefix, pCtxCore->rax, pszPrefix, pCtxCore->rbx, pszPrefix, pCtxCore->rcx, pszPrefix, pCtxCore->rdx, pszPrefix, pCtxCore->rsi, pszPrefix, pCtxCore->rdi,
                    pszPrefix, pCtxCore->r8, pszPrefix, pCtxCore->r9, pszPrefix, pCtxCore->r10, pszPrefix, pCtxCore->r11, pszPrefix, pCtxCore->r12, pszPrefix, pCtxCore->r13,
                    pszPrefix, pCtxCore->r14, pszPrefix, pCtxCore->r15,
                    pszPrefix, pCtxCore->rip, pszPrefix, pCtxCore->rsp, pszPrefix, pCtxCore->rbp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u,
                    pszPrefix, (RTSEL)pCtxCore->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u,
                    pszPrefix, (RTSEL)pCtxCore->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u,
                    pszPrefix, (RTSEL)pCtxCore->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u,
                    pszPrefix, (RTSEL)pCtxCore->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u,
                    pszPrefix, (RTSEL)pCtxCore->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u,
                    pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->dr[0],  pszPrefix, pCtx->dr[1], pszPrefix, pCtx->dr[2],  pszPrefix, pCtx->dr[3],
                    pszPrefix, pCtx->dr[4],  pszPrefix, pCtx->dr[5], pszPrefix, pCtx->dr[6],  pszPrefix, pCtx->dr[7],
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, efl,
                    pszPrefix, (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u64Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
                    pszPrefix, (RTSEL)pCtx->tr, pCtx->trHid.u64Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
                    pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);
            else
                pHlp->pfnPrintf(pHlp,
                    "%seax=%08x %sebx=%08x %secx=%08x %sedx=%08x %sesi=%08x %sedi=%08x\n"
                    "%seip=%08x %sesp=%08x %sebp=%08x %siopl=%d %*s\n"
                    "%scs={%04x base=%016RX64 limit=%08x flags=%08x} %sdr0=%08RX64 %sdr1=%08RX64\n"
                    "%sds={%04x base=%016RX64 limit=%08x flags=%08x} %sdr2=%08RX64 %sdr3=%08RX64\n"
                    "%ses={%04x base=%016RX64 limit=%08x flags=%08x} %sdr4=%08RX64 %sdr5=%08RX64\n"
                    "%sfs={%04x base=%016RX64 limit=%08x flags=%08x} %sdr6=%08RX64 %sdr7=%08RX64\n"
                    "%sgs={%04x base=%016RX64 limit=%08x flags=%08x} %scr0=%08RX64 %scr2=%08RX64\n"
                    "%sss={%04x base=%016RX64 limit=%08x flags=%08x} %scr3=%08RX64 %scr4=%08RX64\n"
                    "%sgdtr=%016RX64:%04x  %sidtr=%016RX64:%04x  %seflags=%08x\n"
                    "%sldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%str  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
                    "%sSysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
                    ,
                    pszPrefix, pCtxCore->eax, pszPrefix, pCtxCore->ebx, pszPrefix, pCtxCore->ecx, pszPrefix, pCtxCore->edx, pszPrefix, pCtxCore->esi, pszPrefix, pCtxCore->edi,
                    pszPrefix, pCtxCore->eip, pszPrefix, pCtxCore->esp, pszPrefix, pCtxCore->ebp, pszPrefix, X86_EFL_GET_IOPL(efl), *pszPrefix ? 33 : 31, szEFlags,
                    pszPrefix, (RTSEL)pCtxCore->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pszPrefix, pCtx->dr[0],  pszPrefix, pCtx->dr[1],
                    pszPrefix, (RTSEL)pCtxCore->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pszPrefix, pCtx->dr[2],  pszPrefix, pCtx->dr[3],
                    pszPrefix, (RTSEL)pCtxCore->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pszPrefix, pCtx->dr[4],  pszPrefix, pCtx->dr[5],
                    pszPrefix, (RTSEL)pCtxCore->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pszPrefix, pCtx->dr[6],  pszPrefix, pCtx->dr[7],
                    pszPrefix, (RTSEL)pCtxCore->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, pszPrefix, pCtx->cr0,  pszPrefix, pCtx->cr2,
                    pszPrefix, (RTSEL)pCtxCore->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, pszPrefix, pCtx->cr3,  pszPrefix, pCtx->cr4,
                    pszPrefix, pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pszPrefix, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, pszPrefix, efl,
                    pszPrefix, (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u64Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
                    pszPrefix, (RTSEL)pCtx->tr, pCtx->trHid.u64Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
                    pszPrefix, pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);

            pHlp->pfnPrintf(pHlp,
                "FPU:\n"
                "%sFCW=%04x %sFSW=%04x %sFTW=%02x\n"
                "%sres1=%02x %sFOP=%04x %sFPUIP=%08x %sCS=%04x %sRsvrd1=%04x\n"
                "%sFPUDP=%04x %sDS=%04x %sRsvrd2=%04x %sMXCSR=%08x %sMXCSR_MASK=%08x\n"
                ,
                pszPrefix, pCtx->fpu.FCW, pszPrefix, pCtx->fpu.FSW, pszPrefix, pCtx->fpu.FTW,
                pszPrefix, pCtx->fpu.huh1, pszPrefix, pCtx->fpu.FOP, pszPrefix, pCtx->fpu.FPUIP, pszPrefix, pCtx->fpu.CS, pszPrefix, pCtx->fpu.Rsvrd1,
                pszPrefix, pCtx->fpu.FPUDP, pszPrefix, pCtx->fpu.DS, pszPrefix, pCtx->fpu.Rsrvd2,
                pszPrefix, pCtx->fpu.MXCSR, pszPrefix, pCtx->fpu.MXCSR_MASK);

            pHlp->pfnPrintf(pHlp,
                "MSR:\n"
                "%sEFER         =%016RX64\n"
                "%sPAT          =%016RX64\n"
                "%sSTAR         =%016RX64\n"
                "%sCSTAR        =%016RX64\n"
                "%sLSTAR        =%016RX64\n"
                "%sSFMASK       =%016RX64\n"
                "%sKERNELGSBASE =%016RX64\n",
                pszPrefix, pCtx->msrEFER,
                pszPrefix, pCtx->msrPAT,
                pszPrefix, pCtx->msrSTAR,
                pszPrefix, pCtx->msrCSTAR,
                pszPrefix, pCtx->msrLSTAR,
                pszPrefix, pCtx->msrSFMASK,
                pszPrefix, pCtx->msrKERNELGSBASE);
            break;
    }
}


/**
 * Display all cpu states and any other cpum info.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoAll(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    cpumR3InfoGuest(pVM, pHlp, pszArgs);
    cpumR3InfoGuestInstr(pVM, pHlp, pszArgs);
    cpumR3InfoHyper(pVM, pHlp, pszArgs);
    cpumR3InfoHost(pVM, pHlp, pszArgs);
}


/**
 * Parses the info argument.
 *
 * The argument starts with 'verbose', 'terse' or 'default' and then
 * continues with the comment string.
 *
 * @param   pszArgs         The pointer to the argument string.
 * @param   penmType        Where to store the dump type request.
 * @param   ppszComment     Where to store the pointer to the comment string.
 */
static void cpumR3InfoParseArg(const char *pszArgs, CPUMDUMPTYPE *penmType, const char **ppszComment)
{
    if (!pszArgs)
    {
        *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = "";
    }
    else
    {
        if (!strncmp(pszArgs, "verbose", sizeof("verbose") - 1))
        {
            pszArgs += 5;
            *penmType = CPUMDUMPTYPE_VERBOSE;
        }
        else if (!strncmp(pszArgs, "terse", sizeof("terse") - 1))
        {
            pszArgs += 5;
            *penmType = CPUMDUMPTYPE_TERSE;
        }
        else if (!strncmp(pszArgs, "default", sizeof("default") - 1))
        {
            pszArgs += 7;
            *penmType = CPUMDUMPTYPE_DEFAULT;
        }
        else
            *penmType = CPUMDUMPTYPE_DEFAULT;
        *ppszComment = RTStrStripL(pszArgs);
    }
}


/**
 * Display the guest cpu state.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoGuest(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);

    /* @todo SMP support! */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    pHlp->pfnPrintf(pHlp, "Guest CPUM (VCPU %d) state: %s\n", pVCpu->idCpu, pszComment);

    PCPUMCTX pCtx = CPUMQueryGuestCtxPtr(pVCpu);
    cpumR3InfoOne(pVM, pCtx, CPUMCTX2CORE(pCtx), pHlp, enmType, "");
}


/**
 * Display the current guest instruction
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoGuestInstr(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    char szInstruction[256];
    /* @todo SMP support! */
    PVMCPU pVCpu = VMMGetCpu(pVM);
    if (!pVCpu)
        pVCpu = &pVM->aCpus[0];

    int rc = DBGFR3DisasInstrCurrent(pVCpu, szInstruction, sizeof(szInstruction));
    if (RT_SUCCESS(rc))
        pHlp->pfnPrintf(pHlp, "\nCPUM: %s\n\n", szInstruction);
}


/**
 * Display the hypervisor cpu state.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoHyper(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    /* @todo SMP */
    PVMCPU pVCpu = &pVM->aCpus[0];

    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Hypervisor CPUM state: %s\n", pszComment);
    cpumR3InfoOne(pVM, &pVCpu->cpum.s.Hyper, pVCpu->cpum.s.pHyperCoreR3, pHlp, enmType, ".");
    pHlp->pfnPrintf(pHlp, "CR4OrMask=%#x CR4AndMask=%#x\n", pVM->cpum.s.CR4.OrMask, pVM->cpum.s.CR4.AndMask);
}


/**
 * Display the host cpu state.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     Arguments, ignored.
 */
static DECLCALLBACK(void) cpumR3InfoHost(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
{
    CPUMDUMPTYPE enmType;
    const char *pszComment;
    cpumR3InfoParseArg(pszArgs, &enmType, &pszComment);
    pHlp->pfnPrintf(pHlp, "Host CPUM state: %s\n", pszComment);

    /*
     * Format the EFLAGS.
     */
    /* @todo SMP */
    PCPUMHOSTCTX pCtx = &pVM->aCpus[0].cpum.s.Host;
#if HC_ARCH_BITS == 32
    uint32_t efl = pCtx->eflags.u32;
#else
    uint64_t efl = pCtx->rflags;
#endif
    char szEFlags[80];
    cpumR3InfoFormatFlags(&szEFlags[0], efl);

    /*
     * Format the registers.
     */
#if HC_ARCH_BITS == 32
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    if (!(pCtx->efer & MSR_K6_EFER_LMA))
# endif
    {
        pHlp->pfnPrintf(pHlp,
            "eax=xxxxxxxx ebx=%08x ecx=xxxxxxxx edx=xxxxxxxx esi=%08x edi=%08x\n"
            "eip=xxxxxxxx esp=%08x ebp=%08x iopl=%d %31s\n"
            "cs=%04x ds=%04x es=%04x fs=%04x gs=%04x                       eflags=%08x\n"
            "cr0=%08RX64 cr2=xxxxxxxx cr3=%08RX64 cr4=%08RX64 gdtr=%08x:%04x ldtr=%04x\n"
            "dr[0]=%08RX64 dr[1]=%08RX64x dr[2]=%08RX64 dr[3]=%08RX64x dr[6]=%08RX64 dr[7]=%08RX64\n"
            "SysEnter={cs=%04x eip=%08x esp=%08x}\n"
            ,
            /*pCtx->eax,*/ pCtx->ebx, /*pCtx->ecx, pCtx->edx,*/ pCtx->esi, pCtx->edi,
            /*pCtx->eip,*/ pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(efl), szEFlags,
            (RTSEL)pCtx->cs, (RTSEL)pCtx->ds, (RTSEL)pCtx->es, (RTSEL)pCtx->fs, (RTSEL)pCtx->gs, efl,
            pCtx->cr0, /*pCtx->cr2,*/ pCtx->cr3, pCtx->cr4,
            pCtx->dr0, pCtx->dr1, pCtx->dr2, pCtx->dr3, pCtx->dr6, pCtx->dr7,
            (uint32_t)pCtx->gdtr.uAddr, pCtx->gdtr.cb, (RTSEL)pCtx->ldtr,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp);
    }
# ifdef VBOX_WITH_HYBRID_32BIT_KERNEL
    else
# endif
#endif
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    {
        pHlp->pfnPrintf(pHlp,
            "rax=xxxxxxxxxxxxxxxx rbx=%016RX64 rcx=xxxxxxxxxxxxxxxx\n"
            "rdx=xxxxxxxxxxxxxxxx rsi=%016RX64 rdi=%016RX64\n"
            "rip=xxxxxxxxxxxxxxxx rsp=%016RX64 rbp=%016RX64\n"
            " r8=xxxxxxxxxxxxxxxx  r9=xxxxxxxxxxxxxxxx r10=%016RX64\n"
            "r11=%016RX64 r12=%016RX64 r13=%016RX64\n"
            "r14=%016RX64 r15=%016RX64\n"
            "iopl=%d  %31s\n"
            "cs=%04x  ds=%04x  es=%04x  fs=%04x  gs=%04x                   eflags=%08RX64\n"
            "cr0=%016RX64 cr2=xxxxxxxxxxxxxxxx cr3=%016RX64\n"
            "cr4=%016RX64 ldtr=%04x tr=%04x\n"
            "dr[0]=%016RX64 dr[1]=%016RX64 dr[2]=%016RX64\n"
            "dr[3]=%016RX64 dr[6]=%016RX64 dr[7]=%016RX64\n"
            "gdtr=%016RX64:%04x  idtr=%016RX64:%04x\n"
            "SysEnter={cs=%04x eip=%08x esp=%08x}\n"
            "FSbase=%016RX64 GSbase=%016RX64 efer=%08RX64\n"
            ,
            /*pCtx->rax,*/ pCtx->rbx, /*pCtx->rcx,
            pCtx->rdx,*/ pCtx->rsi, pCtx->rdi,
            /*pCtx->rip,*/ pCtx->rsp, pCtx->rbp,
            /*pCtx->r8,  pCtx->r9,*/  pCtx->r10,
            pCtx->r11, pCtx->r12, pCtx->r13,
            pCtx->r14, pCtx->r15,
            X86_EFL_GET_IOPL(efl), szEFlags,
            (RTSEL)pCtx->cs, (RTSEL)pCtx->ds, (RTSEL)pCtx->es, (RTSEL)pCtx->fs, (RTSEL)pCtx->gs, efl,
            pCtx->cr0, /*pCtx->cr2,*/ pCtx->cr3,
            pCtx->cr4, pCtx->ldtr, pCtx->tr,
            pCtx->dr0, pCtx->dr1, pCtx->dr2,
            pCtx->dr3, pCtx->dr6, pCtx->dr7,
            pCtx->gdtr.uAddr, pCtx->gdtr.cb, pCtx->idtr.uAddr, pCtx->idtr.cb,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp,
            pCtx->FSbase, pCtx->GSbase, pCtx->efer);
    }
#endif
}


/**
 * Get L1 cache / TLS associativity.
 */
static const char *getCacheAss(unsigned u, char *pszBuf)
{
    if (u == 0)
        return "res0  ";
    if (u == 1)
        return "direct";
    if (u >= 256)
        return "???";

    RTStrPrintf(pszBuf, 16, "%d way", u);
    return pszBuf;
}


/**
 * Get L2 cache soociativity.
 */
const char *getL2CacheAss(unsigned u)
{
    switch (u)
    {
        case 0:  return "off   ";
        case 1:  return "direct";
        case 2:  return "2 way ";
        case 3:  return "res3  ";
        case 4:  return "4 way ";
        case 5:  return "res5  ";
        case 6:  return "8 way ";                                    case 7:  return "res7  ";
        case 8:  return "16 way";
        case 9:  return "res9  ";
        case 10: return "res10 ";
        case 11: return "res11 ";
        case 12: return "res12 ";
        case 13: return "res13 ";
        case 14: return "res14 ";
        case 15: return "fully ";
        default:
            return "????";
    }
}


/**
 * Display the guest CpuId leaves.
 *
 * @param   pVM         VM Handle.
 * @param   pHlp        The info helper functions.
 * @param   pszArgs     "terse", "default" or "verbose".
 */
static DECLCALLBACK(void) cpumR3CpuIdInfo(PVM pVM, PCDBGFINFOHLP pHlp, const char *pszArgs)
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

    /*
     * Start cracking.
     */
    CPUMCPUID   Host;
    CPUMCPUID   Guest;
    unsigned    cStdMax = pVM->cpum.s.aGuestCpuIdStd[0].eax;

    pHlp->pfnPrintf(pHlp,
                    "         RAW Standard CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdStd); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[i];
        ASMCpuId_Idx_ECX(i, 0, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cStdMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * If verbose, decode it.
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[0];
        pHlp->pfnPrintf(pHlp,
                        "Name:                            %.04s%.04s%.04s\n"
                        "Supports:                        0-%x\n",
                        &Guest.ebx, &Guest.edx, &Guest.ecx, Guest.eax);
    }

    /*
     * Get Features.
     */
    bool const fIntel = ASMIsIntelCpuEx(pVM->cpum.s.aGuestCpuIdStd[0].ebx,
                                        pVM->cpum.s.aGuestCpuIdStd[0].ecx,
                                        pVM->cpum.s.aGuestCpuIdStd[0].edx);
    if (cStdMax >= 1 && iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdStd[1];
        uint32_t uEAX = Guest.eax;

        pHlp->pfnPrintf(pHlp,
                        "Family:                          %d  \tExtended: %d \tEffective: %d\n"
                        "Model:                           %d  \tExtended: %d \tEffective: %d\n"
                        "Stepping:                        %d\n"
                        "APIC ID:                         %#04x\n"
                        "Logical CPUs:                    %d\n"
                        "CLFLUSH Size:                    %d\n"
                        "Brand ID:                        %#04x\n",
                        (uEAX >> 8) & 0xf, (uEAX >> 20) & 0x7f, ASMGetCpuFamily(uEAX),
                        (uEAX >> 4) & 0xf, (uEAX >> 16) & 0x0f, ASMGetCpuModel(uEAX, fIntel),
                        ASMGetCpuStepping(uEAX),
                        (Guest.ebx >> 24) & 0xff,
                        (Guest.ebx >> 16) & 0xff,
                        (Guest.ebx >>  8) & 0xff,
                        (Guest.ebx >>  0) & 0xff);
        if (iVerbosity == 1)
        {
            uint32_t uEDX = Guest.edx;
            pHlp->pfnPrintf(pHlp, "Features EDX:                   ");
            if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " FPU");
            if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " VME");
            if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DE");
            if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " PSE");
            if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TSC");
            if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " MSR");
            if (uEDX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " PAE");
            if (uEDX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MCE");
            if (uEDX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " CX8");
            if (uEDX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " APIC");
            if (uEDX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " 10");
            if (uEDX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SEP");
            if (uEDX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " MTRR");
            if (uEDX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PGE");
            if (uEDX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " MCA");
            if (uEDX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " CMOV");
            if (uEDX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " PAT");
            if (uEDX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PSE36");
            if (uEDX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " PSN");
            if (uEDX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " CLFSH");
            if (uEDX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " 20");
            if (uEDX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " DS");
            if (uEDX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " ACPI");
            if (uEDX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " MMX");
            if (uEDX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " FXSR");
            if (uEDX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " SSE");
            if (uEDX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " SSE2");
            if (uEDX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " SS");
            if (uEDX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " HTT");
            if (uEDX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " TM");
            if (uEDX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " 30");
            if (uEDX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " PBE");
            pHlp->pfnPrintf(pHlp, "\n");

            uint32_t uECX = Guest.ecx;
            pHlp->pfnPrintf(pHlp, "Features ECX:                   ");
            if (uECX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " SSE3");
            if (uECX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " 1");
            if (uECX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " 2");
            if (uECX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " MONITOR");
            if (uECX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " DS-CPL");
            if (uECX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " VMX");
            if (uECX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " 6");
            if (uECX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " EST");
            if (uECX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " TM2");
            if (uECX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " 9");
            if (uECX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " CNXT-ID");
            if (uECX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " 11");
            if (uECX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " 12");
            if (uECX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " CX16");
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if (uECX & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            ASMCpuId(1, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

            X86CPUIDFEATEDX EdxHost  = *(PX86CPUIDFEATEDX)&Host.edx;
            X86CPUIDFEATECX EcxHost  = *(PX86CPUIDFEATECX)&Host.ecx;
            X86CPUIDFEATEDX EdxGuest = *(PX86CPUIDFEATEDX)&Guest.edx;
            X86CPUIDFEATECX EcxGuest = *(PX86CPUIDFEATECX)&Guest.ecx;

            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "FPU - x87 FPU on Chip                  = %d (%d)\n",  EdxGuest.u1FPU,        EdxHost.u1FPU);
            pHlp->pfnPrintf(pHlp, "VME - Virtual 8086 Mode Enhancements   = %d (%d)\n",  EdxGuest.u1VME,        EdxHost.u1VME);
            pHlp->pfnPrintf(pHlp, "DE - Debugging extensions              = %d (%d)\n",  EdxGuest.u1DE,         EdxHost.u1DE);
            pHlp->pfnPrintf(pHlp, "PSE - Page Size Extension              = %d (%d)\n",  EdxGuest.u1PSE,        EdxHost.u1PSE);
            pHlp->pfnPrintf(pHlp, "TSC - Time Stamp Counter               = %d (%d)\n",  EdxGuest.u1TSC,        EdxHost.u1TSC);
            pHlp->pfnPrintf(pHlp, "MSR - Model Specific Registers         = %d (%d)\n",  EdxGuest.u1MSR,        EdxHost.u1MSR);
            pHlp->pfnPrintf(pHlp, "PAE - Physical Address Extension       = %d (%d)\n",  EdxGuest.u1PAE,        EdxHost.u1PAE);
            pHlp->pfnPrintf(pHlp, "MCE - Machine Check Exception          = %d (%d)\n",  EdxGuest.u1MCE,        EdxHost.u1MCE);
            pHlp->pfnPrintf(pHlp, "CX8 - CMPXCHG8B instruction            = %d (%d)\n",  EdxGuest.u1CX8,        EdxHost.u1CX8);
            pHlp->pfnPrintf(pHlp, "APIC - APIC On-Chip                    = %d (%d)\n",  EdxGuest.u1APIC,       EdxHost.u1APIC);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EdxGuest.u1Reserved1,  EdxHost.u1Reserved1);
            pHlp->pfnPrintf(pHlp, "SEP - SYSENTER and SYSEXIT             = %d (%d)\n",  EdxGuest.u1SEP,        EdxHost.u1SEP);
            pHlp->pfnPrintf(pHlp, "MTRR - Memory Type Range Registers     = %d (%d)\n",  EdxGuest.u1MTRR,       EdxHost.u1MTRR);
            pHlp->pfnPrintf(pHlp, "PGE - PTE Global Bit                   = %d (%d)\n",  EdxGuest.u1PGE,        EdxHost.u1PGE);
            pHlp->pfnPrintf(pHlp, "MCA - Machine Check Architecture       = %d (%d)\n",  EdxGuest.u1MCA,        EdxHost.u1MCA);
            pHlp->pfnPrintf(pHlp, "CMOV - Conditional Move Instructions   = %d (%d)\n",  EdxGuest.u1CMOV,       EdxHost.u1CMOV);
            pHlp->pfnPrintf(pHlp, "PAT - Page Attribute Table             = %d (%d)\n",  EdxGuest.u1PAT,        EdxHost.u1PAT);
            pHlp->pfnPrintf(pHlp, "PSE-36 - 36-bit Page Size Extention    = %d (%d)\n",  EdxGuest.u1PSE36,      EdxHost.u1PSE36);
            pHlp->pfnPrintf(pHlp, "PSN - Processor Serial Number          = %d (%d)\n",  EdxGuest.u1PSN,        EdxHost.u1PSN);
            pHlp->pfnPrintf(pHlp, "CLFSH - CLFLUSH Instruction.           = %d (%d)\n",  EdxGuest.u1CLFSH,      EdxHost.u1CLFSH);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EdxGuest.u1Reserved2,  EdxHost.u1Reserved2);
            pHlp->pfnPrintf(pHlp, "DS - Debug Store                       = %d (%d)\n",  EdxGuest.u1DS,         EdxHost.u1DS);
            pHlp->pfnPrintf(pHlp, "ACPI - Thermal Mon. & Soft. Clock Ctrl.= %d (%d)\n",  EdxGuest.u1ACPI,       EdxHost.u1ACPI);
            pHlp->pfnPrintf(pHlp, "MMX - Intel MMX Technology             = %d (%d)\n",  EdxGuest.u1MMX,        EdxHost.u1MMX);
            pHlp->pfnPrintf(pHlp, "FXSR - FXSAVE and FXRSTOR Instructions = %d (%d)\n",  EdxGuest.u1FXSR,       EdxHost.u1FXSR);
            pHlp->pfnPrintf(pHlp, "SSE - SSE Support                      = %d (%d)\n",  EdxGuest.u1SSE,        EdxHost.u1SSE);
            pHlp->pfnPrintf(pHlp, "SSE2 - SSE2 Support                    = %d (%d)\n",  EdxGuest.u1SSE2,       EdxHost.u1SSE2);
            pHlp->pfnPrintf(pHlp, "SS - Self Snoop                        = %d (%d)\n",  EdxGuest.u1SS,         EdxHost.u1SS);
            pHlp->pfnPrintf(pHlp, "HTT - Hyper-Threading Technolog        = %d (%d)\n",  EdxGuest.u1HTT,        EdxHost.u1HTT);
            pHlp->pfnPrintf(pHlp, "TM - Thermal Monitor                   = %d (%d)\n",  EdxGuest.u1TM,         EdxHost.u1TM);
            pHlp->pfnPrintf(pHlp, "30 - Reserved                          = %d (%d)\n",  EdxGuest.u1Reserved3,  EdxHost.u1Reserved3);
            pHlp->pfnPrintf(pHlp, "PBE - Pending Break Enable             = %d (%d)\n",  EdxGuest.u1PBE,        EdxHost.u1PBE);

            pHlp->pfnPrintf(pHlp, "Supports SSE3 or not                   = %d (%d)\n",  EcxGuest.u1SSE3,       EcxHost.u1SSE3);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %d (%d)\n",  EcxGuest.u1Reserved1,  EcxHost.u1Reserved1);
            pHlp->pfnPrintf(pHlp, "DS Area 64-bit layout                  = %d (%d)\n",  EcxGuest.u1DTE64,      EcxHost.u1DTE64);
            pHlp->pfnPrintf(pHlp, "Supports MONITOR/MWAIT                 = %d (%d)\n",  EcxGuest.u1Monitor,    EcxHost.u1Monitor);
            pHlp->pfnPrintf(pHlp, "CPL-DS - CPL Qualified Debug Store     = %d (%d)\n",  EcxGuest.u1CPLDS,      EcxHost.u1CPLDS);
            pHlp->pfnPrintf(pHlp, "VMX - Virtual Machine Technology       = %d (%d)\n",  EcxGuest.u1VMX,        EcxHost.u1VMX);
            pHlp->pfnPrintf(pHlp, "SMX - Safer Mode Extensions            = %d (%d)\n",  EcxGuest.u1SMX,        EcxHost.u1SMX);
            pHlp->pfnPrintf(pHlp, "Enhanced SpeedStep Technology          = %d (%d)\n",  EcxGuest.u1EST,        EcxHost.u1EST);
            pHlp->pfnPrintf(pHlp, "Terminal Monitor 2                     = %d (%d)\n",  EcxGuest.u1TM2,        EcxHost.u1TM2);
            pHlp->pfnPrintf(pHlp, "Supports Supplemental SSE3 or not      = %d (%d)\n",  EcxGuest.u1SSSE3,      EcxHost.u1SSSE3);
            pHlp->pfnPrintf(pHlp, "L1 Context ID                          = %d (%d)\n",  EcxGuest.u1CNTXID,     EcxHost.u1CNTXID);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %#x (%#x)\n",EcxGuest.u2Reserved2,  EcxHost.u2Reserved2);
            pHlp->pfnPrintf(pHlp, "CMPXCHG16B                             = %d (%d)\n",  EcxGuest.u1CX16,       EcxHost.u1CX16);
            pHlp->pfnPrintf(pHlp, "xTPR Update Control                    = %d (%d)\n",  EcxGuest.u1TPRUpdate,  EcxHost.u1TPRUpdate);
            pHlp->pfnPrintf(pHlp, "Perf/Debug Capability MSR              = %d (%d)\n",  EcxGuest.u1PDCM,       EcxHost.u1PDCM);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %#x (%#x)\n",EcxGuest.u2Reserved3,  EcxHost.u2Reserved3);
            pHlp->pfnPrintf(pHlp, "Direct Cache Access                    = %d (%d)\n",  EcxGuest.u1DCA,        EcxHost.u1DCA);
            pHlp->pfnPrintf(pHlp, "Supports SSE4_1 or not                 = %d (%d)\n",  EcxGuest.u1SSE4_1,     EcxHost.u1SSE4_1);
            pHlp->pfnPrintf(pHlp, "Supports SSE4_2 or not                 = %d (%d)\n",  EcxGuest.u1SSE4_2,     EcxHost.u1SSE4_2);
            pHlp->pfnPrintf(pHlp, "Supports the x2APIC extensions         = %d (%d)\n",  EcxGuest.u1x2APIC,     EcxHost.u1x2APIC);
            pHlp->pfnPrintf(pHlp, "Supports MOVBE                         = %d (%d)\n",  EcxGuest.u1MOVBE,      EcxHost.u1MOVBE);
            pHlp->pfnPrintf(pHlp, "Supports POPCNT                        = %d (%d)\n",  EcxGuest.u1POPCNT,     EcxHost.u1POPCNT);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %#x (%#x)\n",EcxGuest.u2Reserved4,  EcxHost.u2Reserved4);
            pHlp->pfnPrintf(pHlp, "Supports XSAVE                         = %d (%d)\n",  EcxGuest.u1XSAVE,      EcxHost.u1XSAVE);
            pHlp->pfnPrintf(pHlp, "Supports OSXSAVE                       = %d (%d)\n",  EcxGuest.u1OSXSAVE,    EcxHost.u1OSXSAVE);
            pHlp->pfnPrintf(pHlp, "Reserved                               = %#x (%#x)\n",EcxGuest.u4Reserved5,  EcxHost.u4Reserved5);
        }
    }
    if (cStdMax >= 2 && iVerbosity)
    {
        /** @todo */
    }

    /*
     * Extended.
     * Implemented after AMD specs.
     */
    unsigned    cExtMax = pVM->cpum.s.aGuestCpuIdExt[0].eax & 0xffff;

    pHlp->pfnPrintf(pHlp,
                    "\n"
                    "         RAW Extended CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdExt); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[i];
        ASMCpuId(0x80000000 | i, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        0x80000000 | i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cExtMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * Understandable output
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[0];
        pHlp->pfnPrintf(pHlp,
                        "Ext Name:                        %.4s%.4s%.4s\n"
                        "Ext Supports:                    0x80000000-%#010x\n",
                        &Guest.ebx, &Guest.edx, &Guest.ecx, Guest.eax);
    }

    if (iVerbosity && cExtMax >= 1)
    {
        Guest = pVM->cpum.s.aGuestCpuIdExt[1];
        uint32_t uEAX = Guest.eax;
        pHlp->pfnPrintf(pHlp,
                        "Family:                          %d  \tExtended: %d \tEffective: %d\n"
                        "Model:                           %d  \tExtended: %d \tEffective: %d\n"
                        "Stepping:                        %d\n"
                        "Brand ID:                        %#05x\n",
                        (uEAX >> 8) & 0xf, (uEAX >> 20) & 0x7f, ASMGetCpuFamily(uEAX),
                        (uEAX >> 4) & 0xf, (uEAX >> 16) & 0x0f, ASMGetCpuModel(uEAX, fIntel),
                        ASMGetCpuStepping(uEAX),
                        Guest.ebx & 0xfff);

        if (iVerbosity == 1)
        {
            uint32_t uEDX = Guest.edx;
            pHlp->pfnPrintf(pHlp, "Features EDX:                   ");
            if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " FPU");
            if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " VME");
            if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " DE");
            if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " PSE");
            if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TSC");
            if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " MSR");
            if (uEDX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " PAE");
            if (uEDX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MCE");
            if (uEDX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " CX8");
            if (uEDX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " APIC");
            if (uEDX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " 10");
            if (uEDX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SCR");
            if (uEDX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " MTRR");
            if (uEDX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PGE");
            if (uEDX & RT_BIT(14))  pHlp->pfnPrintf(pHlp, " MCA");
            if (uEDX & RT_BIT(15))  pHlp->pfnPrintf(pHlp, " CMOV");
            if (uEDX & RT_BIT(16))  pHlp->pfnPrintf(pHlp, " PAT");
            if (uEDX & RT_BIT(17))  pHlp->pfnPrintf(pHlp, " PSE36");
            if (uEDX & RT_BIT(18))  pHlp->pfnPrintf(pHlp, " 18");
            if (uEDX & RT_BIT(19))  pHlp->pfnPrintf(pHlp, " 19");
            if (uEDX & RT_BIT(20))  pHlp->pfnPrintf(pHlp, " NX");
            if (uEDX & RT_BIT(21))  pHlp->pfnPrintf(pHlp, " 21");
            if (uEDX & RT_BIT(22))  pHlp->pfnPrintf(pHlp, " ExtMMX");
            if (uEDX & RT_BIT(23))  pHlp->pfnPrintf(pHlp, " MMX");
            if (uEDX & RT_BIT(24))  pHlp->pfnPrintf(pHlp, " FXSR");
            if (uEDX & RT_BIT(25))  pHlp->pfnPrintf(pHlp, " FastFXSR");
            if (uEDX & RT_BIT(26))  pHlp->pfnPrintf(pHlp, " Page1GB");
            if (uEDX & RT_BIT(27))  pHlp->pfnPrintf(pHlp, " RDTSCP");
            if (uEDX & RT_BIT(28))  pHlp->pfnPrintf(pHlp, " 28");
            if (uEDX & RT_BIT(29))  pHlp->pfnPrintf(pHlp, " LongMode");
            if (uEDX & RT_BIT(30))  pHlp->pfnPrintf(pHlp, " Ext3DNow");
            if (uEDX & RT_BIT(31))  pHlp->pfnPrintf(pHlp, " 3DNow");
            pHlp->pfnPrintf(pHlp, "\n");

            uint32_t uECX = Guest.ecx;
            pHlp->pfnPrintf(pHlp, "Features ECX:                   ");
            if (uECX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " LAHF/SAHF");
            if (uECX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " CMPL");
            if (uECX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " SVM");
            if (uECX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " ExtAPIC");
            if (uECX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " CR8L");
            if (uECX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " ABM");
            if (uECX & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " SSE4A");
            if (uECX & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " MISALNSSE");
            if (uECX & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " 3DNOWPRF");
            if (uECX & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " OSVW");
            if (uECX & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " IBS");
            if (uECX & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " SSE5");
            if (uECX & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " SKINIT");
            if (uECX & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " WDT");
            for (unsigned iBit = 5; iBit < 32; iBit++)
                if (uECX & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            ASMCpuId(0x80000001, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

            uint32_t uEdxGst = Guest.edx;
            uint32_t uEdxHst = Host.edx;
            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "FPU - x87 FPU on Chip                  = %d (%d)\n",  !!(uEdxGst & RT_BIT( 0)),  !!(uEdxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "VME - Virtual 8086 Mode Enhancements   = %d (%d)\n",  !!(uEdxGst & RT_BIT( 1)),  !!(uEdxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "DE - Debugging extensions              = %d (%d)\n",  !!(uEdxGst & RT_BIT( 2)),  !!(uEdxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "PSE - Page Size Extension              = %d (%d)\n",  !!(uEdxGst & RT_BIT( 3)),  !!(uEdxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "TSC - Time Stamp Counter               = %d (%d)\n",  !!(uEdxGst & RT_BIT( 4)),  !!(uEdxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "MSR - K86 Model Specific Registers     = %d (%d)\n",  !!(uEdxGst & RT_BIT( 5)),  !!(uEdxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "PAE - Physical Address Extension       = %d (%d)\n",  !!(uEdxGst & RT_BIT( 6)),  !!(uEdxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "MCE - Machine Check Exception          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 7)),  !!(uEdxHst & RT_BIT( 7)));
            pHlp->pfnPrintf(pHlp, "CX8 - CMPXCHG8B instruction            = %d (%d)\n",  !!(uEdxGst & RT_BIT( 8)),  !!(uEdxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "APIC - APIC On-Chip                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 9)),  !!(uEdxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "10 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(10)),  !!(uEdxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "SEP - SYSCALL and SYSRET               = %d (%d)\n",  !!(uEdxGst & RT_BIT(11)),  !!(uEdxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "MTRR - Memory Type Range Registers     = %d (%d)\n",  !!(uEdxGst & RT_BIT(12)),  !!(uEdxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "PGE - PTE Global Bit                   = %d (%d)\n",  !!(uEdxGst & RT_BIT(13)),  !!(uEdxHst & RT_BIT(13)));
            pHlp->pfnPrintf(pHlp, "MCA - Machine Check Architecture       = %d (%d)\n",  !!(uEdxGst & RT_BIT(14)),  !!(uEdxHst & RT_BIT(14)));
            pHlp->pfnPrintf(pHlp, "CMOV - Conditional Move Instructions   = %d (%d)\n",  !!(uEdxGst & RT_BIT(15)),  !!(uEdxHst & RT_BIT(15)));
            pHlp->pfnPrintf(pHlp, "PAT - Page Attribute Table             = %d (%d)\n",  !!(uEdxGst & RT_BIT(16)),  !!(uEdxHst & RT_BIT(16)));
            pHlp->pfnPrintf(pHlp, "PSE-36 - 36-bit Page Size Extention    = %d (%d)\n",  !!(uEdxGst & RT_BIT(17)),  !!(uEdxHst & RT_BIT(17)));
            pHlp->pfnPrintf(pHlp, "18 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(18)),  !!(uEdxHst & RT_BIT(18)));
            pHlp->pfnPrintf(pHlp, "19 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(19)),  !!(uEdxHst & RT_BIT(19)));
            pHlp->pfnPrintf(pHlp, "NX - No-Execute Page Protection        = %d (%d)\n",  !!(uEdxGst & RT_BIT(20)),  !!(uEdxHst & RT_BIT(20)));
            pHlp->pfnPrintf(pHlp, "DS - Debug Store                       = %d (%d)\n",  !!(uEdxGst & RT_BIT(21)),  !!(uEdxHst & RT_BIT(21)));
            pHlp->pfnPrintf(pHlp, "AXMMX - AMD Extensions to MMX Instr.   = %d (%d)\n",  !!(uEdxGst & RT_BIT(22)),  !!(uEdxHst & RT_BIT(22)));
            pHlp->pfnPrintf(pHlp, "MMX - Intel MMX Technology             = %d (%d)\n",  !!(uEdxGst & RT_BIT(23)),  !!(uEdxHst & RT_BIT(23)));
            pHlp->pfnPrintf(pHlp, "FXSR - FXSAVE and FXRSTOR Instructions = %d (%d)\n",  !!(uEdxGst & RT_BIT(24)),  !!(uEdxHst & RT_BIT(24)));
            pHlp->pfnPrintf(pHlp, "25 - AMD fast FXSAVE and FXRSTOR Instr.= %d (%d)\n",  !!(uEdxGst & RT_BIT(25)),  !!(uEdxHst & RT_BIT(25)));
            pHlp->pfnPrintf(pHlp, "26 - 1 GB large page support           = %d (%d)\n",  !!(uEdxGst & RT_BIT(26)),  !!(uEdxHst & RT_BIT(26)));
            pHlp->pfnPrintf(pHlp, "27 - RDTSCP instruction                = %d (%d)\n",  !!(uEdxGst & RT_BIT(27)),  !!(uEdxHst & RT_BIT(27)));
            pHlp->pfnPrintf(pHlp, "28 - Reserved                          = %d (%d)\n",  !!(uEdxGst & RT_BIT(28)),  !!(uEdxHst & RT_BIT(28)));
            pHlp->pfnPrintf(pHlp, "29 - AMD Long Mode                     = %d (%d)\n",  !!(uEdxGst & RT_BIT(29)),  !!(uEdxHst & RT_BIT(29)));
            pHlp->pfnPrintf(pHlp, "30 - AMD Extensions to 3DNow           = %d (%d)\n",  !!(uEdxGst & RT_BIT(30)),  !!(uEdxHst & RT_BIT(30)));
            pHlp->pfnPrintf(pHlp, "31 - AMD 3DNow                         = %d (%d)\n",  !!(uEdxGst & RT_BIT(31)),  !!(uEdxHst & RT_BIT(31)));

            uint32_t uEcxGst = Guest.ecx;
            uint32_t uEcxHst = Host.ecx;
            pHlp->pfnPrintf(pHlp, "LahfSahf - LAHF/SAHF in 64-bit mode    = %d (%d)\n",  !!(uEcxGst & RT_BIT( 0)),  !!(uEcxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "CmpLegacy - Core MP legacy mode (depr) = %d (%d)\n",  !!(uEcxGst & RT_BIT( 1)),  !!(uEcxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "SVM - AMD VM Extensions                = %d (%d)\n",  !!(uEcxGst & RT_BIT( 2)),  !!(uEcxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "APIC registers starting at 0x400       = %d (%d)\n",  !!(uEcxGst & RT_BIT( 3)),  !!(uEcxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "AltMovCR8 - LOCK MOV CR0 means MOV CR8 = %d (%d)\n",  !!(uEcxGst & RT_BIT( 4)),  !!(uEcxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "Advanced bit manipulation              = %d (%d)\n",  !!(uEcxGst & RT_BIT( 5)),  !!(uEcxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "SSE4A instruction support              = %d (%d)\n",  !!(uEcxGst & RT_BIT( 6)),  !!(uEcxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "Misaligned SSE mode                    = %d (%d)\n",  !!(uEcxGst & RT_BIT( 7)),  !!(uEcxHst & RT_BIT( 7)));
            pHlp->pfnPrintf(pHlp, "PREFETCH and PREFETCHW instruction     = %d (%d)\n",  !!(uEcxGst & RT_BIT( 8)),  !!(uEcxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "OS visible workaround                  = %d (%d)\n",  !!(uEcxGst & RT_BIT( 9)),  !!(uEcxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "Instruction based sampling             = %d (%d)\n",  !!(uEcxGst & RT_BIT(10)),  !!(uEcxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "SSE5 support                           = %d (%d)\n",  !!(uEcxGst & RT_BIT(11)),  !!(uEcxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "SKINIT, STGI, and DEV support          = %d (%d)\n",  !!(uEcxGst & RT_BIT(12)),  !!(uEcxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "Watchdog timer support.                = %d (%d)\n",  !!(uEcxGst & RT_BIT(13)),  !!(uEcxHst & RT_BIT(13)));
            pHlp->pfnPrintf(pHlp, "31:14 - Reserved                       = %#x (%#x)\n",   uEcxGst >> 14,          uEcxHst >> 14);
        }
    }

    if (iVerbosity && cExtMax >= 2)
    {
        char szString[4*4*3+1] = {0};
        uint32_t *pu32 = (uint32_t *)szString;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].eax;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].ebx;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].ecx;
        *pu32++ = pVM->cpum.s.aGuestCpuIdExt[2].edx;
        if (cExtMax >= 3)
        {
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].eax;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].ebx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].ecx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[3].edx;
        }
        if (cExtMax >= 4)
        {
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].eax;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].ebx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].ecx;
            *pu32++ = pVM->cpum.s.aGuestCpuIdExt[4].edx;
        }
        pHlp->pfnPrintf(pHlp, "Full Name:                       %s\n", szString);
    }

    if (iVerbosity && cExtMax >= 5)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[5].eax;
        uint32_t uEBX = pVM->cpum.s.aGuestCpuIdExt[5].ebx;
        uint32_t uECX = pVM->cpum.s.aGuestCpuIdExt[5].ecx;
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[5].edx;
        char sz1[32];
        char sz2[32];

        pHlp->pfnPrintf(pHlp,
                        "TLB 2/4M Instr/Uni:              %s %3d entries\n"
                        "TLB 2/4M Data:                   %s %3d entries\n",
                        getCacheAss((uEAX >>  8) & 0xff, sz1), (uEAX >>  0) & 0xff,
                        getCacheAss((uEAX >> 24) & 0xff, sz2), (uEAX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "TLB 4K Instr/Uni:                %s %3d entries\n"
                        "TLB 4K Data:                     %s %3d entries\n",
                        getCacheAss((uEBX >>  8) & 0xff, sz1), (uEBX >>  0) & 0xff,
                        getCacheAss((uEBX >> 24) & 0xff, sz2), (uEBX >> 16) & 0xff);
        pHlp->pfnPrintf(pHlp, "L1 Instr Cache Line Size:        %d bytes\n"
                        "L1 Instr Cache Lines Per Tag:    %d\n"
                        "L1 Instr Cache Associativity:    %s\n"
                        "L1 Instr Cache Size:             %d KB\n",
                        (uEDX >> 0) & 0xff,
                        (uEDX >> 8) & 0xff,
                        getCacheAss((uEDX >> 16) & 0xff, sz1),
                        (uEDX >> 24) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "L1 Data Cache Line Size:         %d bytes\n"
                        "L1 Data Cache Lines Per Tag:     %d\n"
                        "L1 Data Cache Associativity:     %s\n"
                        "L1 Data Cache Size:              %d KB\n",
                        (uECX >> 0) & 0xff,
                        (uECX >> 8) & 0xff,
                        getCacheAss((uECX >> 16) & 0xff, sz1),
                        (uECX >> 24) & 0xff);
    }

    if (iVerbosity && cExtMax >= 6)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[6].eax;
        uint32_t uEBX = pVM->cpum.s.aGuestCpuIdExt[6].ebx;
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[6].edx;

        pHlp->pfnPrintf(pHlp,
                        "L2 TLB 2/4M Instr/Uni:           %s %4d entries\n"
                        "L2 TLB 2/4M Data:                %s %4d entries\n",
                        getL2CacheAss((uEAX >> 12) & 0xf),  (uEAX >>  0) & 0xfff,
                        getL2CacheAss((uEAX >> 28) & 0xf),  (uEAX >> 16) & 0xfff);
        pHlp->pfnPrintf(pHlp,
                        "L2 TLB 4K Instr/Uni:             %s %4d entries\n"
                        "L2 TLB 4K Data:                  %s %4d entries\n",
                        getL2CacheAss((uEBX >> 12) & 0xf),  (uEBX >>  0) & 0xfff,
                        getL2CacheAss((uEBX >> 28) & 0xf),  (uEBX >> 16) & 0xfff);
        pHlp->pfnPrintf(pHlp,
                        "L2 Cache Line Size:              %d bytes\n"
                        "L2 Cache Lines Per Tag:          %d\n"
                        "L2 Cache Associativity:          %s\n"
                        "L2 Cache Size:                   %d KB\n",
                        (uEDX >> 0) & 0xff,
                        (uEDX >> 8) & 0xf,
                        getL2CacheAss((uEDX >> 12) & 0xf),
                        (uEDX >> 16) & 0xffff);
    }

    if (iVerbosity && cExtMax >= 7)
    {
        uint32_t uEDX = pVM->cpum.s.aGuestCpuIdExt[7].edx;

        pHlp->pfnPrintf(pHlp, "APM Features:                   ");
        if (uEDX & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " TS");
        if (uEDX & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " FID");
        if (uEDX & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " VID");
        if (uEDX & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " TTP");
        if (uEDX & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " TM");
        if (uEDX & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " STC");
        for (unsigned iBit = 6; iBit < 32; iBit++)
            if (uEDX & RT_BIT(iBit))
                pHlp->pfnPrintf(pHlp, " %d", iBit);
        pHlp->pfnPrintf(pHlp, "\n");
    }

    if (iVerbosity && cExtMax >= 8)
    {
        uint32_t uEAX = pVM->cpum.s.aGuestCpuIdExt[8].eax;
        uint32_t uECX = pVM->cpum.s.aGuestCpuIdExt[8].ecx;

        pHlp->pfnPrintf(pHlp,
                        "Physical Address Width:          %d bits\n"
                        "Virtual Address Width:           %d bits\n",
                        (uEAX >> 0) & 0xff,
                        (uEAX >> 8) & 0xff);
        pHlp->pfnPrintf(pHlp,
                        "Physical Core Count:             %d\n",
                        (uECX >> 0) & 0xff);
    }


    /*
     * Centaur.
     */
    unsigned cCentaurMax = pVM->cpum.s.aGuestCpuIdCentaur[0].eax & 0xffff;

    pHlp->pfnPrintf(pHlp,
                    "\n"
                    "         RAW Centaur CPUIDs\n"
                    "     Function  eax      ebx      ecx      edx\n");
    for (unsigned i = 0; i < RT_ELEMENTS(pVM->cpum.s.aGuestCpuIdCentaur); i++)
    {
        Guest = pVM->cpum.s.aGuestCpuIdCentaur[i];
        ASMCpuId(0xc0000000 | i, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);

        pHlp->pfnPrintf(pHlp,
                        "Gst: %08x  %08x %08x %08x %08x%s\n"
                        "Hst:           %08x %08x %08x %08x\n",
                        0xc0000000 | i, Guest.eax, Guest.ebx, Guest.ecx, Guest.edx,
                        i <= cCentaurMax ? "" : "*",
                        Host.eax, Host.ebx, Host.ecx, Host.edx);
    }

    /*
     * Understandable output
     */
    if (iVerbosity)
    {
        Guest = pVM->cpum.s.aGuestCpuIdCentaur[0];
        pHlp->pfnPrintf(pHlp,
                        "Centaur Supports:                0xc0000000-%#010x\n",
                        Guest.eax);
    }

    if (iVerbosity && cCentaurMax >= 1)
    {
        ASMCpuId(0xc0000001, &Host.eax, &Host.ebx, &Host.ecx, &Host.edx);
        uint32_t uEdxGst = pVM->cpum.s.aGuestCpuIdExt[1].edx;
        uint32_t uEdxHst = Host.edx;

        if (iVerbosity == 1)
        {
            pHlp->pfnPrintf(pHlp, "Centaur Features EDX:           ");
            if (uEdxGst & RT_BIT(0))   pHlp->pfnPrintf(pHlp, " AIS");
            if (uEdxGst & RT_BIT(1))   pHlp->pfnPrintf(pHlp, " AIS-E");
            if (uEdxGst & RT_BIT(2))   pHlp->pfnPrintf(pHlp, " RNG");
            if (uEdxGst & RT_BIT(3))   pHlp->pfnPrintf(pHlp, " RNG-E");
            if (uEdxGst & RT_BIT(4))   pHlp->pfnPrintf(pHlp, " LH");
            if (uEdxGst & RT_BIT(5))   pHlp->pfnPrintf(pHlp, " FEMMS");
            if (uEdxGst & RT_BIT(6))   pHlp->pfnPrintf(pHlp, " ACE");
            if (uEdxGst & RT_BIT(7))   pHlp->pfnPrintf(pHlp, " ACE-E");
            /* possibly indicating MM/HE and MM/HE-E on older chips... */
            if (uEdxGst & RT_BIT(8))   pHlp->pfnPrintf(pHlp, " ACE2");
            if (uEdxGst & RT_BIT(9))   pHlp->pfnPrintf(pHlp, " ACE2-E");
            if (uEdxGst & RT_BIT(10))  pHlp->pfnPrintf(pHlp, " PHE");
            if (uEdxGst & RT_BIT(11))  pHlp->pfnPrintf(pHlp, " PHE-E");
            if (uEdxGst & RT_BIT(12))  pHlp->pfnPrintf(pHlp, " PMM");
            if (uEdxGst & RT_BIT(13))  pHlp->pfnPrintf(pHlp, " PMM-E");
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if (uEdxGst & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, " %d", iBit);
            pHlp->pfnPrintf(pHlp, "\n");
        }
        else
        {
            pHlp->pfnPrintf(pHlp, "Mnemonic - Description                 = guest (host)\n");
            pHlp->pfnPrintf(pHlp, "AIS - Alternate Instruction Set        = %d (%d)\n",  !!(uEdxGst & RT_BIT( 0)),  !!(uEdxHst & RT_BIT( 0)));
            pHlp->pfnPrintf(pHlp, "AIS-E - AIS enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 1)),  !!(uEdxHst & RT_BIT( 1)));
            pHlp->pfnPrintf(pHlp, "RNG - Random Number Generator          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 2)),  !!(uEdxHst & RT_BIT( 2)));
            pHlp->pfnPrintf(pHlp, "RNG-E - RNG enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 3)),  !!(uEdxHst & RT_BIT( 3)));
            pHlp->pfnPrintf(pHlp, "LH - LongHaul MSR 0000_110Ah           = %d (%d)\n",  !!(uEdxGst & RT_BIT( 4)),  !!(uEdxHst & RT_BIT( 4)));
            pHlp->pfnPrintf(pHlp, "FEMMS - FEMMS                          = %d (%d)\n",  !!(uEdxGst & RT_BIT( 5)),  !!(uEdxHst & RT_BIT( 5)));
            pHlp->pfnPrintf(pHlp, "ACE - Advanced Cryptography Engine     = %d (%d)\n",  !!(uEdxGst & RT_BIT( 6)),  !!(uEdxHst & RT_BIT( 6)));
            pHlp->pfnPrintf(pHlp, "ACE-E - ACE enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT( 7)),  !!(uEdxHst & RT_BIT( 7)));
            /* possibly indicating MM/HE and MM/HE-E on older chips... */
            pHlp->pfnPrintf(pHlp, "ACE2 - Advanced Cryptography Engine 2  = %d (%d)\n",  !!(uEdxGst & RT_BIT( 8)),  !!(uEdxHst & RT_BIT( 8)));
            pHlp->pfnPrintf(pHlp, "ACE2-E - ACE enabled                   = %d (%d)\n",  !!(uEdxGst & RT_BIT( 9)),  !!(uEdxHst & RT_BIT( 9)));
            pHlp->pfnPrintf(pHlp, "PHE - Hash Engine                      = %d (%d)\n",  !!(uEdxGst & RT_BIT(10)),  !!(uEdxHst & RT_BIT(10)));
            pHlp->pfnPrintf(pHlp, "PHE-E - PHE enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT(11)),  !!(uEdxHst & RT_BIT(11)));
            pHlp->pfnPrintf(pHlp, "PMM - Montgomery Multiplier            = %d (%d)\n",  !!(uEdxGst & RT_BIT(12)),  !!(uEdxHst & RT_BIT(12)));
            pHlp->pfnPrintf(pHlp, "PMM-E - PMM enabled                    = %d (%d)\n",  !!(uEdxGst & RT_BIT(13)),  !!(uEdxHst & RT_BIT(13)));
            for (unsigned iBit = 14; iBit < 32; iBit++)
                if ((uEdxGst | uEdxHst) & RT_BIT(iBit))
                    pHlp->pfnPrintf(pHlp, "Bit %d                                 = %d (%d)\n",  !!(uEdxGst & RT_BIT(iBit)),  !!(uEdxHst & RT_BIT(iBit)));
            pHlp->pfnPrintf(pHlp, "\n");
        }
    }
}


/**
 * Structure used when disassembling and instructions in DBGF.
 * This is used so the reader function can get the stuff it needs.
 */
typedef struct CPUMDISASSTATE
{
    /** Pointer to the CPU structure. */
    PDISCPUSTATE    pCpu;
    /** The VM handle. */
    PVM             pVM;
    /** The VMCPU handle. */
    PVMCPU          pVCpu;
    /** Pointer to the first byte in the segemnt. */
    RTGCUINTPTR     GCPtrSegBase;
    /** Pointer to the byte after the end of the segment. (might have wrapped!) */
    RTGCUINTPTR     GCPtrSegEnd;
    /** The size of the segment minus 1. */
    RTGCUINTPTR     cbSegLimit;
    /** Pointer to the current page - R3 Ptr. */
    void const     *pvPageR3;
    /** Pointer to the current page - GC Ptr. */
    RTGCPTR         pvPageGC;
    /** The lock information that PGMPhysReleasePageMappingLock needs. */
    PGMPAGEMAPLOCK  PageMapLock;
    /** Whether the PageMapLock is valid or not. */
    bool            fLocked;
    /** 64 bits mode or not. */
    bool            f64Bits;
} CPUMDISASSTATE, *PCPUMDISASSTATE;


/**
 * Instruction reader.
 *
 * @returns VBox status code.
 * @param   PtrSrc      Address to read from.
 *                      In our case this is relative to the selector pointed to by the 2nd user argument of uDisCpu.
 * @param   pu8Dst      Where to store the bytes.
 * @param   cbRead      Number of bytes to read.
 * @param   uDisCpu     Pointer to the disassembler cpu state.
 *                      In this context it's always pointer to the Core of a DBGFDISASSTATE.
 */
static DECLCALLBACK(int) cpumR3DisasInstrRead(RTUINTPTR PtrSrc, uint8_t *pu8Dst, unsigned cbRead, void *uDisCpu)
{
    PDISCPUSTATE pCpu = (PDISCPUSTATE)uDisCpu;
    PCPUMDISASSTATE pState = (PCPUMDISASSTATE)pCpu->apvUserData[0];
    Assert(cbRead > 0);
    for (;;)
    {
        RTGCUINTPTR GCPtr = PtrSrc + pState->GCPtrSegBase;

        /* Need to update the page translation? */
        if (    !pState->pvPageR3
            ||  (GCPtr >> PAGE_SHIFT) != (pState->pvPageGC >> PAGE_SHIFT))
        {
            int rc = VINF_SUCCESS;

            /* translate the address */
            pState->pvPageGC = GCPtr & PAGE_BASE_GC_MASK;
            if (    MMHyperIsInsideArea(pState->pVM, pState->pvPageGC)
                &&  !HWACCMIsEnabled(pState->pVM))
            {
                pState->pvPageR3 = MMHyperRCToR3(pState->pVM, (RTRCPTR)pState->pvPageGC);
                if (!pState->pvPageR3)
                    rc = VERR_INVALID_POINTER;
            }
            else
            {
                /* Release mapping lock previously acquired. */
                if (pState->fLocked)
                    PGMPhysReleasePageMappingLock(pState->pVM, &pState->PageMapLock);
                rc = PGMPhysGCPtr2CCPtrReadOnly(pState->pVCpu, pState->pvPageGC, &pState->pvPageR3, &pState->PageMapLock);
                pState->fLocked = RT_SUCCESS_NP(rc);
            }
            if (RT_FAILURE(rc))
            {
                pState->pvPageR3 = NULL;
                return rc;
            }
        }

        /* check the segemnt limit */
        if (!pState->f64Bits && PtrSrc > pState->cbSegLimit)
            return VERR_OUT_OF_SELECTOR_BOUNDS;

        /* calc how much we can read */
        uint32_t cb = PAGE_SIZE - (GCPtr & PAGE_OFFSET_MASK);
        if (!pState->f64Bits)
        {
            RTGCUINTPTR cbSeg = pState->GCPtrSegEnd - GCPtr;
            if (cb > cbSeg && cbSeg)
                cb = cbSeg;
        }
        if (cb > cbRead)
            cb = cbRead;

        /* read and advance */
        memcpy(pu8Dst, (char *)pState->pvPageR3 + (GCPtr & PAGE_OFFSET_MASK), cb);
        cbRead -= cb;
        if (!cbRead)
            return VINF_SUCCESS;
        pu8Dst += cb;
        PtrSrc += cb;
    }
}


/**
 * Disassemble an instruction and return the information in the provided structure.
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pVCpu       VMCPU Handle
 * @param   pCtx        CPU context
 * @param   GCPtrPC     Program counter (relative to CS) to disassemble from.
 * @param   pCpu        Disassembly state
 * @param   pszPrefix   String prefix for logging (debug only)
 *
 */
VMMR3DECL(int) CPUMR3DisasmInstrCPU(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTGCPTR GCPtrPC, PDISCPUSTATE pCpu, const char *pszPrefix)
{
    CPUMDISASSTATE  State;
    int             rc;

    const PGMMODE enmMode = PGMGetGuestMode(pVCpu);
    State.pCpu            = pCpu;
    State.pvPageGC        = 0;
    State.pvPageR3        = NULL;
    State.pVM             = pVM;
    State.pVCpu           = pVCpu;
    State.fLocked         = false;
    State.f64Bits         = false;

    /*
     * Get selector information.
     */
    if (    (pCtx->cr0 & X86_CR0_PE)
        &&   pCtx->eflags.Bits.u1VM == 0)
    {
        if (CPUMAreHiddenSelRegsValid(pVM))
        {
            State.f64Bits         = enmMode >= PGMMODE_AMD64 && pCtx->csHid.Attr.n.u1Long;
            State.GCPtrSegBase    = pCtx->csHid.u64Base;
            State.GCPtrSegEnd     = pCtx->csHid.u32Limit + 1 + (RTGCUINTPTR)pCtx->csHid.u64Base;
            State.cbSegLimit      = pCtx->csHid.u32Limit;
            pCpu->mode            = (State.f64Bits)
                                    ? CPUMODE_64BIT
                                    : pCtx->csHid.Attr.n.u1DefBig
                                    ? CPUMODE_32BIT
                                    : CPUMODE_16BIT;
        }
        else
        {
            DBGFSELINFO SelInfo;

            rc = SELMR3GetShadowSelectorInfo(pVM, pCtx->cs, &SelInfo);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("SELMR3GetShadowSelectorInfo failed for %04X:%RGv rc=%d\n", pCtx->cs, GCPtrPC, rc));
                return rc;
            }

            /*
             * Validate the selector.
             */
            rc = DBGFR3SelInfoValidateCS(&SelInfo, pCtx->ss);
            if (RT_FAILURE(rc))
            {
                AssertMsgFailed(("SELMSelInfoValidateCS failed for %04X:%RGv rc=%d\n", pCtx->cs, GCPtrPC, rc));
                return rc;
            }
            State.GCPtrSegBase    = SelInfo.GCPtrBase;
            State.GCPtrSegEnd     = SelInfo.cbLimit + 1 + (RTGCUINTPTR)SelInfo.GCPtrBase;
            State.cbSegLimit      = SelInfo.cbLimit;
            pCpu->mode            = SelInfo.u.Raw.Gen.u1DefBig ? CPUMODE_32BIT : CPUMODE_16BIT;
        }
    }
    else
    {
        /* real or V86 mode */
        pCpu->mode            = CPUMODE_16BIT;
        State.GCPtrSegBase    = pCtx->cs * 16;
        State.GCPtrSegEnd     = 0xFFFFFFFF;
        State.cbSegLimit      = 0xFFFFFFFF;
    }

    /*
     * Disassemble the instruction.
     */
    pCpu->pfnReadBytes    = cpumR3DisasInstrRead;
    pCpu->apvUserData[0]  = &State;

    uint32_t cbInstr;
#ifndef LOG_ENABLED
    rc = DISInstr(pCpu, GCPtrPC, 0, &cbInstr, NULL);
    if (RT_SUCCESS(rc))
    {
#else
    char szOutput[160];
    rc = DISInstr(pCpu, GCPtrPC, 0, &cbInstr, &szOutput[0]);
    if (RT_SUCCESS(rc))
    {
        /* log it */
        if (pszPrefix)
            Log(("%s-CPU%d: %s", pszPrefix, pVCpu->idCpu, szOutput));
        else
            Log(("%s", szOutput));
#endif
        rc = VINF_SUCCESS;
    }
    else
        Log(("CPUMR3DisasmInstrCPU: DISInstr failed for %04X:%RGv rc=%Rrc\n", pCtx->cs, GCPtrPC, rc));

    /* Release mapping lock acquired in cpumR3DisasInstrRead. */
    if (State.fLocked)
        PGMPhysReleasePageMappingLock(pVM, &State.PageMapLock);

    return rc;
}

#ifdef DEBUG

/**
 * Disassemble an instruction and dump it to the log
 *
 * @returns VBox status code.
 * @param   pVM         VM Handle
 * @param   pVCpu       VMCPU Handle
 * @param   pCtx        CPU context
 * @param   pc          GC instruction pointer
 * @param   pszPrefix   String prefix for logging
 *
 * @deprecated  Use DBGFR3DisasInstrCurrentLog().
 */
VMMR3DECL(void) CPUMR3DisasmInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTGCPTR pc, const char *pszPrefix)
{
    DISCPUSTATE Cpu;
    CPUMR3DisasmInstrCPU(pVM, pVCpu, pCtx, pc, &Cpu, pszPrefix);
}


/**
 * Debug helper - Saves guest context on raw mode entry (for fatal dump)
 *
 * @internal
 */
VMMR3DECL(void) CPUMR3SaveEntryCtx(PVM pVM)
{
    /* @todo SMP support!! */
    pVM->cpum.s.GuestEntry = *CPUMQueryGuestCtxPtr(VMMGetCpu(pVM));
}

#endif /* DEBUG */

/**
 * API for controlling a few of the CPU features found in CR4.
 *
 * Currently only X86_CR4_TSD is accepted as input.
 *
 * @returns VBox status code.
 *
 * @param   pVM     The VM handle.
 * @param   fOr     The CR4 OR mask.
 * @param   fAnd    The CR4 AND mask.
 */
VMMR3DECL(int) CPUMR3SetCR4Feature(PVM pVM, RTHCUINTREG fOr, RTHCUINTREG fAnd)
{
    AssertMsgReturn(!(fOr & ~(X86_CR4_TSD)), ("%#x\n", fOr), VERR_INVALID_PARAMETER);
    AssertMsgReturn((fAnd & ~(X86_CR4_TSD)) == ~(X86_CR4_TSD), ("%#x\n", fAnd), VERR_INVALID_PARAMETER);

    pVM->cpum.s.CR4.OrMask &= fAnd;
    pVM->cpum.s.CR4.OrMask |= fOr;

    return VINF_SUCCESS;
}


/**
 * Gets a pointer to the array of standard CPUID leaves.
 *
 * CPUMR3GetGuestCpuIdStdMax() give the size of the array.
 *
 * @returns Pointer to the standard CPUID leaves (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdStdRCPtr(PVM pVM)
{
    return RCPTRTYPE(PCCPUMCPUID)VM_RC_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdStd[0]);
}


/**
 * Gets a pointer to the array of extended CPUID leaves.
 *
 * CPUMGetGuestCpuIdExtMax() give the size of the array.
 *
 * @returns Pointer to the extended CPUID leaves (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdExtRCPtr(PVM pVM)
{
    return (RCPTRTYPE(PCCPUMCPUID))VM_RC_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdExt[0]);
}


/**
 * Gets a pointer to the array of centaur CPUID leaves.
 *
 * CPUMGetGuestCpuIdCentaurMax() give the size of the array.
 *
 * @returns Pointer to the centaur CPUID leaves (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdCentaurRCPtr(PVM pVM)
{
    return (RCPTRTYPE(PCCPUMCPUID))VM_RC_ADDR(pVM, &pVM->cpum.s.aGuestCpuIdCentaur[0]);
}


/**
 * Gets a pointer to the default CPUID leaf.
 *
 * @returns Pointer to the default CPUID leaf (read-only).
 * @param   pVM         The VM handle.
 * @remark  Intended for PATM.
 */
VMMR3DECL(RCPTRTYPE(PCCPUMCPUID)) CPUMR3GetGuestCpuIdDefRCPtr(PVM pVM)
{
    return (RCPTRTYPE(PCCPUMCPUID))VM_RC_ADDR(pVM, &pVM->cpum.s.GuestCpuIdDef);
}
