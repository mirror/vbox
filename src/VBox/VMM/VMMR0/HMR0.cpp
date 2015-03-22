/* $Id$ */
/** @file
 * Hardware Assisted Virtualization Manager (HM) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2006-2014 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pgm.h>
#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/hm_svm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/cpuset.h>
#include <iprt/mem.h>
#include <iprt/memobj.h>
#include <iprt/once.h>
#include <iprt/param.h>
#include <iprt/power.h>
#include <iprt/string.h>
#include <iprt/thread.h>
#include <iprt/x86.h>
#include "HMVMXR0.h"
#include "HMSVMR0.h"


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void) hmR0EnableCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) hmR0DisableCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) hmR0InitIntelCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) hmR0InitAmdCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) hmR0PowerCallback(RTPOWEREVENT enmEvent, void *pvUser);
static DECLCALLBACK(void) hmR0MpEventCallback(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvData);


/*******************************************************************************
*   Structures and Typedefs                                                    *
*******************************************************************************/
/**
 * This is used to manage the status code of a RTMpOnAll in HM.
 */
typedef struct HMR0FIRSTRC
{
    /** The status code. */
    int32_t volatile    rc;
    /** The ID of the CPU reporting the first failure. */
    RTCPUID volatile    idCpu;
} HMR0FIRSTRC;
/** Pointer to a first return code structure. */
typedef HMR0FIRSTRC *PHMR0FIRSTRC;


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/**
 * Global data.
 */
static struct
{
    /** Per CPU globals. */
    HMGLOBALCPUINFO                 aCpuInfo[RTCPUSET_MAX_CPUS];

    /** @name Ring-0 method table for AMD-V and VT-x specific operations.
     * @{ */
    DECLR0CALLBACKMEMBER(int,  pfnEnterSession, (PVM pVM, PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu));
    DECLR0CALLBACKMEMBER(void, pfnThreadCtxCallback, (RTTHREADCTXEVENT enmEvent, PVMCPU pVCpu, bool fGlobalInit));
    DECLR0CALLBACKMEMBER(int,  pfnSaveHostState, (PVM pVM, PVMCPU pVCpu));
    DECLR0CALLBACKMEMBER(int,  pfnRunGuestCode, (PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx));
    DECLR0CALLBACKMEMBER(int,  pfnEnableCpu, (PHMGLOBALCPUINFO pCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage,
                                              bool fEnabledByHost, void *pvArg));
    DECLR0CALLBACKMEMBER(int,  pfnDisableCpu, (PHMGLOBALCPUINFO pCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage));
    DECLR0CALLBACKMEMBER(int,  pfnInitVM, (PVM pVM));
    DECLR0CALLBACKMEMBER(int,  pfnTermVM, (PVM pVM));
    DECLR0CALLBACKMEMBER(int,  pfnSetupVM ,(PVM pVM));
    /** @} */

    /** Maximum ASID allowed. */
    uint32_t                        uMaxAsid;

    /** VT-x data. */
    struct
    {
        /** Set to by us to indicate VMX is supported by the CPU. */
        bool                        fSupported;
        /** Whether we're using SUPR0EnableVTx or not. */
        bool                        fUsingSUPR0EnableVTx;
        /** Whether we're using the preemption timer or not. */
        bool                        fUsePreemptTimer;
        /** The shift mask employed by the VMX-Preemption timer. */
        uint8_t                     cPreemptTimerShift;

        /** Host CR4 value (set by ring-0 VMX init) */
        /** @todo This isn't used for anything relevant. Remove later? */
        uint64_t                    u64HostCr4;

        /** Host EFER value (set by ring-0 VMX init) */
        uint64_t                    u64HostEfer;

        /** VMX MSR values */
        VMXMSRS                     Msrs;

        /* Last instruction error */
        uint32_t                    ulLastInstrError;
    } vmx;

    /** AMD-V information. */
    struct
    {
        /* HWCR MSR (for diagnostics) */
        uint64_t                    u64MsrHwcr;

        /** SVM revision. */
        uint32_t                    u32Rev;

        /** SVM feature bits from cpuid 0x8000000a */
        uint32_t                    u32Features;

        /** Set by us to indicate SVM is supported by the CPU. */
        bool                        fSupported;
    } svm;
    /** Saved error from detection */
    int32_t                         lLastError;

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    /** If set, VT-x/AMD-V is enabled globally at init time, otherwise it's
     * enabled and disabled each time it's used to execute guest code. */
    bool                            fGlobalInit;
    /** Indicates whether the host is suspending or not.  We'll refuse a few
     *  actions when the host is being suspended to speed up the suspending and
     *  avoid trouble. */
    volatile bool                   fSuspended;

    /** Whether we've already initialized all CPUs.
     * @remarks We could check the EnableAllCpusOnce state, but this is
     *          simpler and hopefully easier to understand. */
    bool                            fEnabled;
    /** Serialize initialization in HMR0EnableAllCpus. */
    RTONCE                          EnableAllCpusOnce;
} g_HvmR0;



/**
 * Initializes a first return code structure.
 *
 * @param   pFirstRc            The structure to init.
 */
static void hmR0FirstRcInit(PHMR0FIRSTRC pFirstRc)
{
    pFirstRc->rc    = VINF_SUCCESS;
    pFirstRc->idCpu = NIL_RTCPUID;
}


/**
 * Try set the status code (success ignored).
 *
 * @param   pFirstRc            The first return code structure.
 * @param   rc                  The status code.
 */
static void hmR0FirstRcSetStatus(PHMR0FIRSTRC pFirstRc, int rc)
{
    if (   RT_FAILURE(rc)
        && ASMAtomicCmpXchgS32(&pFirstRc->rc, rc, VINF_SUCCESS))
        pFirstRc->idCpu = RTMpCpuId();
}


/**
 * Get the status code of a first return code structure.
 *
 * @returns The status code; VINF_SUCCESS or error status, no informational or
 *          warning errors.
 * @param   pFirstRc            The first return code structure.
 */
static int hmR0FirstRcGetStatus(PHMR0FIRSTRC pFirstRc)
{
    return pFirstRc->rc;
}


#ifdef VBOX_STRICT
/**
 * Get the CPU ID on which the failure status code was reported.
 *
 * @returns The CPU ID, NIL_RTCPUID if no failure was reported.
 * @param   pFirstRc            The first return code structure.
 */
static RTCPUID hmR0FirstRcGetCpuId(PHMR0FIRSTRC pFirstRc)
{
    return pFirstRc->idCpu;
}
#endif /* VBOX_STRICT */


/** @name Dummy callback handlers.
 * @{ */

static DECLCALLBACK(int) hmR0DummyEnter(PVM pVM, PVMCPU pVCpu, PHMGLOBALCPUINFO pCpu)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCpu);
    return VINF_SUCCESS;
}

static DECLCALLBACK(void) hmR0DummyThreadCtxCallback(RTTHREADCTXEVENT enmEvent, PVMCPU pVCpu, bool fGlobalInit)
{
    NOREF(enmEvent); NOREF(pVCpu); NOREF(fGlobalInit);
}

static DECLCALLBACK(int) hmR0DummyEnableCpu(PHMGLOBALCPUINFO pCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage,
                                            bool fEnabledBySystem, void *pvArg)
{
    NOREF(pCpu); NOREF(pVM); NOREF(pvCpuPage); NOREF(HCPhysCpuPage); NOREF(fEnabledBySystem); NOREF(pvArg);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) hmR0DummyDisableCpu(PHMGLOBALCPUINFO pCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    NOREF(pCpu);  NOREF(pvCpuPage); NOREF(HCPhysCpuPage);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) hmR0DummyInitVM(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) hmR0DummyTermVM(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) hmR0DummySetupVM(PVM pVM)
{
    NOREF(pVM);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) hmR0DummyRunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx);
    return VINF_SUCCESS;
}

static DECLCALLBACK(int) hmR0DummySaveHostState(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM); NOREF(pVCpu);
    return VINF_SUCCESS;
}

/** @} */


/**
 * Checks if the CPU is subject to the "VMX-Preemption Timer Does Not Count
 * Down at the Rate Specified" erratum.
 *
 * Errata names and related steppings:
 *      - BA86   - D0.
 *      - AAX65  - C2.
 *      - AAU65  - C2, K0.
 *      - AAO95  - B1.
 *      - AAT59  - C2.
 *      - AAK139 - D0.
 *      - AAM126 - C0, C1, D0.
 *      - AAN92  - B1.
 *      - AAJ124 - C0, D0.
 *
 *      - AAP86  - B1.
 *
 * Steppings: B1, C0, C1, C2, D0, K0.
 *
 * @returns true if subject to it, false if not.
 */
static bool hmR0InitIntelIsSubjectToVmxPreemptionTimerErratum(void)
{
    uint32_t u = ASMCpuId_EAX(1);
    u &= ~(RT_BIT_32(14) | RT_BIT_32(15) | RT_BIT_32(28) | RT_BIT_32(29) | RT_BIT_32(30) | RT_BIT_32(31));
    if (   u == UINT32_C(0x000206E6) /* 323344.pdf - BA86   - D0 - Intel Xeon Processor 7500 Series */
        || u == UINT32_C(0x00020652) /* 323056.pdf - AAX65  - C2 - Intel Xeon Processor L3406 */
        || u == UINT32_C(0x00020652) /* 322814.pdf - AAT59  - C2 - Intel CoreTM i7-600, i5-500, i5-400 and i3-300 Mobile Processor Series */
        || u == UINT32_C(0x00020652) /* 322911.pdf - AAU65  - C2 - Intel CoreTM i5-600, i3-500 Desktop Processor Series and Intel Pentium Processor G6950 */
        || u == UINT32_C(0x00020655) /* 322911.pdf - AAU65  - K0 - Intel CoreTM i5-600, i3-500 Desktop Processor Series and Intel Pentium Processor G6950 */
        || u == UINT32_C(0x000106E5) /* 322373.pdf - AAO95  - B1 - Intel Xeon Processor 3400 Series */
        || u == UINT32_C(0x000106E5) /* 322166.pdf - AAN92  - B1 - Intel CoreTM i7-800 and i5-700 Desktop Processor Series */
        || u == UINT32_C(0x000106E5) /* 320767.pdf - AAP86  - B1 - Intel Core i7-900 Mobile Processor Extreme Edition Series, Intel Core i7-800 and i7-700 Mobile Processor Series */
        || u == UINT32_C(0x000106A0) /* 321333.pdf - AAM126 - C0 - Intel Xeon Processor 3500 Series Specification */
        || u == UINT32_C(0x000106A1) /* 321333.pdf - AAM126 - C1 - Intel Xeon Processor 3500 Series Specification */
        || u == UINT32_C(0x000106A4) /* 320836.pdf - AAJ124 - C0 - Intel Core i7-900 Desktop Processor Extreme Edition Series and Intel Core i7-900 Desktop Processor Series */
        || u == UINT32_C(0x000106A5) /* 321333.pdf - AAM126 - D0 - Intel Xeon Processor 3500 Series Specification */
        || u == UINT32_C(0x000106A5) /* 321324.pdf - AAK139 - D0 - Intel Xeon Processor 5500 Series Specification */
        || u == UINT32_C(0x000106A5) /* 320836.pdf - AAJ124 - D0 - Intel Core i7-900 Desktop Processor Extreme Edition Series and Intel Core i7-900 Desktop Processor Series */
        )
        return true;
    return false;
}


/**
 * Intel specific initialization code.
 *
 * @returns VBox status code (will only fail if out of memory).
 */
static int hmR0InitIntel(uint32_t u32FeaturesECX, uint32_t u32FeaturesEDX)
{
    /*
     * Check that all the required VT-x features are present.
     * We also assume all VT-x-enabled CPUs support fxsave/fxrstor.
     */
    if (    (u32FeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
         && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
         && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
       )
    {
        /** @todo move this into a separate function. */
        g_HvmR0.vmx.Msrs.u64FeatureCtrl = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);

        /*
         * First try use native kernel API for controlling VT-x.
         * (This is only supported by some Mac OS X kernels atm.)
         */
        int rc = g_HvmR0.lLastError = SUPR0EnableVTx(true /* fEnable */);
        g_HvmR0.vmx.fUsingSUPR0EnableVTx = rc != VERR_NOT_SUPPORTED;
        if (g_HvmR0.vmx.fUsingSUPR0EnableVTx)
        {
            AssertLogRelMsg(rc == VINF_SUCCESS || rc == VERR_VMX_IN_VMX_ROOT_MODE || rc == VERR_VMX_NO_VMX, ("%Rrc\n", rc));
            if (RT_SUCCESS(rc))
            {
                g_HvmR0.vmx.fSupported = true;
                rc = SUPR0EnableVTx(false /* fEnable */);
                AssertLogRelRC(rc);
            }
        }
        else
        {
            /* We need to check if VT-x has been properly initialized on all
               CPUs. Some BIOSes do a lousy job. */
            HMR0FIRSTRC FirstRc;
            hmR0FirstRcInit(&FirstRc);
            g_HvmR0.lLastError = RTMpOnAll(hmR0InitIntelCpu, &FirstRc, NULL);
            if (RT_SUCCESS(g_HvmR0.lLastError))
                g_HvmR0.lLastError = hmR0FirstRcGetStatus(&FirstRc);
        }
        if (RT_SUCCESS(g_HvmR0.lLastError))
        {
            /* Reread in case it was changed by hmR0InitIntelCpu(). */
            g_HvmR0.vmx.Msrs.u64FeatureCtrl = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);

            /*
             * Read all relevant registers and MSRs.
             */
            g_HvmR0.vmx.u64HostCr4          = ASMGetCR4();
            g_HvmR0.vmx.u64HostEfer         = ASMRdMsr(MSR_K6_EFER);
            g_HvmR0.vmx.Msrs.u64BasicInfo   = ASMRdMsr(MSR_IA32_VMX_BASIC_INFO);
            g_HvmR0.vmx.Msrs.VmxPinCtls.u   = ASMRdMsr(MSR_IA32_VMX_PINBASED_CTLS);
            g_HvmR0.vmx.Msrs.VmxProcCtls.u  = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
            g_HvmR0.vmx.Msrs.VmxExit.u      = ASMRdMsr(MSR_IA32_VMX_EXIT_CTLS);
            g_HvmR0.vmx.Msrs.VmxEntry.u     = ASMRdMsr(MSR_IA32_VMX_ENTRY_CTLS);
            g_HvmR0.vmx.Msrs.u64Misc        = ASMRdMsr(MSR_IA32_VMX_MISC);
            g_HvmR0.vmx.Msrs.u64Cr0Fixed0   = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED0);
            g_HvmR0.vmx.Msrs.u64Cr0Fixed1   = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED1);
            g_HvmR0.vmx.Msrs.u64Cr4Fixed0   = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED0);
            g_HvmR0.vmx.Msrs.u64Cr4Fixed1   = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED1);
            g_HvmR0.vmx.Msrs.u64VmcsEnum    = ASMRdMsr(MSR_IA32_VMX_VMCS_ENUM);
            /* VPID 16 bits ASID. */
            g_HvmR0.uMaxAsid                = 0x10000; /* exclusive */

            if (g_HvmR0.vmx.Msrs.VmxProcCtls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
            {
                g_HvmR0.vmx.Msrs.VmxProcCtls2.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS2);
                if (g_HvmR0.vmx.Msrs.VmxProcCtls2.n.allowed1 & (VMX_VMCS_CTRL_PROC_EXEC2_EPT | VMX_VMCS_CTRL_PROC_EXEC2_VPID))
                    g_HvmR0.vmx.Msrs.u64EptVpidCaps = ASMRdMsr(MSR_IA32_VMX_EPT_VPID_CAP);

                if (g_HvmR0.vmx.Msrs.VmxProcCtls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VMFUNC)
                    g_HvmR0.vmx.Msrs.u64Vmfunc = ASMRdMsr(MSR_IA32_VMX_VMFUNC);
            }

            if (!g_HvmR0.vmx.fUsingSUPR0EnableVTx)
            {
                /*
                 * Enter root mode
                 */
                RTR0MEMOBJ hScatchMemObj;
                rc = RTR0MemObjAllocCont(&hScatchMemObj, PAGE_SIZE, false /* fExecutable */);
                if (RT_FAILURE(rc))
                {
                    LogRel(("hmR0InitIntel: RTR0MemObjAllocCont(,PAGE_SIZE,false) -> %Rrc\n", rc));
                    return rc;
                }

                void      *pvScatchPage      = RTR0MemObjAddress(hScatchMemObj);
                RTHCPHYS   HCPhysScratchPage = RTR0MemObjGetPagePhysAddr(hScatchMemObj, 0);
                ASMMemZeroPage(pvScatchPage);

                /* Set revision dword at the beginning of the structure. */
                *(uint32_t *)pvScatchPage = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(g_HvmR0.vmx.Msrs.u64BasicInfo);

                /* Make sure we don't get rescheduled to another cpu during this probe. */
                RTCCUINTREG fFlags = ASMIntDisableFlags();

                /*
                 * Check CR4.VMXE
                 */
                g_HvmR0.vmx.u64HostCr4 = ASMGetCR4();
                if (!(g_HvmR0.vmx.u64HostCr4 & X86_CR4_VMXE))
                {
                    /* In theory this bit could be cleared behind our back.  Which would cause
                       #UD faults when we try to execute the VMX instructions... */
                    ASMSetCR4(g_HvmR0.vmx.u64HostCr4 | X86_CR4_VMXE);
                }

                /*
                 * The only way of checking if we're in VMX root mode or not is to try and enter it.
                 * There is no instruction or control bit that tells us if we're in VMX root mode.
                 * Therefore, try and enter VMX root mode here.
                 */
                rc = VMXEnable(HCPhysScratchPage);
                if (RT_SUCCESS(rc))
                {
                    g_HvmR0.vmx.fSupported = true;
                    VMXDisable();
                }
                else
                {
                    /*
                     * KVM leaves the CPU in VMX root mode. Not only is  this not allowed,
                     * it will crash the host when we enter raw mode, because:
                     *
                     *   (a) clearing X86_CR4_VMXE in CR4 causes a #GP (we no longer modify
                     *       this bit), and
                     *   (b) turning off paging causes a #GP  (unavoidable when switching
                     *       from long to 32 bits mode or 32 bits to PAE).
                     *
                     * They should fix their code, but until they do we simply refuse to run.
                     */
                    g_HvmR0.lLastError = VERR_VMX_IN_VMX_ROOT_MODE;
                    Assert(g_HvmR0.vmx.fSupported == false);
                }

                /* Restore CR4 again; don't leave the X86_CR4_VMXE flag set
                   if it wasn't so before (some software could incorrectly
                   think it's in VMX mode). */
                ASMSetCR4(g_HvmR0.vmx.u64HostCr4);
                ASMSetFlags(fFlags);

                RTR0MemObjFree(hScatchMemObj, false);
            }

            if (g_HvmR0.vmx.fSupported)
            {
                rc = VMXR0GlobalInit();
                if (RT_FAILURE(rc))
                    g_HvmR0.lLastError = rc;

                /*
                 * Install the VT-x methods.
                 */
                g_HvmR0.pfnEnterSession      = VMXR0Enter;
                g_HvmR0.pfnThreadCtxCallback = VMXR0ThreadCtxCallback;
                g_HvmR0.pfnSaveHostState     = VMXR0SaveHostState;
                g_HvmR0.pfnRunGuestCode      = VMXR0RunGuestCode;
                g_HvmR0.pfnEnableCpu         = VMXR0EnableCpu;
                g_HvmR0.pfnDisableCpu        = VMXR0DisableCpu;
                g_HvmR0.pfnInitVM            = VMXR0InitVM;
                g_HvmR0.pfnTermVM            = VMXR0TermVM;
                g_HvmR0.pfnSetupVM           = VMXR0SetupVM;

                /*
                 * Check for the VMX-Preemption Timer and adjust for the "VMX-Preemption
                 * Timer Does Not Count Down at the Rate Specified" erratum.
                 */
                if (g_HvmR0.vmx.Msrs.VmxPinCtls.n.allowed1 & VMX_VMCS_CTRL_PIN_EXEC_PREEMPT_TIMER)
                {
                    g_HvmR0.vmx.fUsePreemptTimer   = true;
                    g_HvmR0.vmx.cPreemptTimerShift = MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(g_HvmR0.vmx.Msrs.u64Misc);
                    if (hmR0InitIntelIsSubjectToVmxPreemptionTimerErratum())
                        g_HvmR0.vmx.cPreemptTimerShift = 0; /* This is about right most of the time here. */
                }
            }
        }
#ifdef LOG_ENABLED
        else
            SUPR0Printf("hmR0InitIntelCpu failed with rc=%d\n", g_HvmR0.lLastError);
#endif
    }
    else
        g_HvmR0.lLastError = VERR_VMX_NO_VMX;
    return VINF_SUCCESS;
}


/**
 * AMD-specific initialization code.
 *
 * @returns VBox status code.
 */
static int hmR0InitAmd(uint32_t u32FeaturesEDX, uint32_t uMaxExtLeaf)
{
    /*
     * Read all SVM MSRs if SVM is available. (same goes for RDMSR/WRMSR)
     * We also assume all SVM-enabled CPUs support fxsave/fxrstor.
     */
    int rc;
    if (   (g_HvmR0.cpuid.u32AMDFeatureECX & X86_CPUID_AMD_FEATURE_ECX_SVM)
        && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
        && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
        && ASMIsValidExtRange(uMaxExtLeaf)
        && uMaxExtLeaf >= 0x8000000a
       )
    {
        /* Call the global AMD-V initialization routine. */
        rc = SVMR0GlobalInit();
        if (RT_FAILURE(rc))
        {
            g_HvmR0.lLastError = rc;
            return rc;
        }

        /*
         * Install the AMD-V methods.
         */
        g_HvmR0.pfnEnterSession      = SVMR0Enter;
        g_HvmR0.pfnThreadCtxCallback = SVMR0ThreadCtxCallback;
        g_HvmR0.pfnSaveHostState     = SVMR0SaveHostState;
        g_HvmR0.pfnRunGuestCode      = SVMR0RunGuestCode;
        g_HvmR0.pfnEnableCpu         = SVMR0EnableCpu;
        g_HvmR0.pfnDisableCpu        = SVMR0DisableCpu;
        g_HvmR0.pfnInitVM            = SVMR0InitVM;
        g_HvmR0.pfnTermVM            = SVMR0TermVM;
        g_HvmR0.pfnSetupVM           = SVMR0SetupVM;

        /* Query AMD features. */
        uint32_t u32Dummy;
        ASMCpuId(0x8000000a, &g_HvmR0.svm.u32Rev, &g_HvmR0.uMaxAsid, &u32Dummy, &g_HvmR0.svm.u32Features);

        /*
         * We need to check if AMD-V has been properly initialized on all CPUs.
         * Some BIOSes might do a poor job.
         */
        HMR0FIRSTRC FirstRc;
        hmR0FirstRcInit(&FirstRc);
        rc = RTMpOnAll(hmR0InitAmdCpu, &FirstRc, NULL);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
            rc = hmR0FirstRcGetStatus(&FirstRc);
#ifndef DEBUG_bird
        AssertMsg(rc == VINF_SUCCESS || rc == VERR_SVM_IN_USE,
                  ("hmR0InitAmdCpu failed for cpu %d with rc=%Rrc\n", hmR0FirstRcGetCpuId(&FirstRc), rc));
#endif
        if (RT_SUCCESS(rc))
        {
            /* Read the HWCR MSR for diagnostics. */
            g_HvmR0.svm.u64MsrHwcr = ASMRdMsr(MSR_K8_HWCR);
            g_HvmR0.svm.fSupported = true;
        }
        else
        {
            g_HvmR0.lLastError = rc;
            if (rc == VERR_SVM_DISABLED || rc == VERR_SVM_IN_USE)
                rc = VINF_SUCCESS; /* Don't fail if AMD-V is disabled or in use. */
        }
    }
    else
    {
        rc = VINF_SUCCESS;              /* Don't fail if AMD-V is not supported. See @bugref{6785}. */
        g_HvmR0.lLastError = VERR_SVM_NO_SVM;
    }
    return rc;
}


/**
 * Does global Ring-0 HM initialization (at module init).
 *
 * @returns VBox status code.
 */
VMMR0_INT_DECL(int) HMR0Init(void)
{
    /*
     * Initialize the globals.
     */
    g_HvmR0.fEnabled = false;
    static RTONCE s_OnceInit = RTONCE_INITIALIZER;
    g_HvmR0.EnableAllCpusOnce = s_OnceInit;
    for (unsigned i = 0; i < RT_ELEMENTS(g_HvmR0.aCpuInfo); i++)
    {
        g_HvmR0.aCpuInfo[i].hMemObj = NIL_RTR0MEMOBJ;
        g_HvmR0.aCpuInfo[i].idCpu   = NIL_RTCPUID;
    }

    /* Fill in all callbacks with placeholders. */
    g_HvmR0.pfnEnterSession      = hmR0DummyEnter;
    g_HvmR0.pfnThreadCtxCallback = hmR0DummyThreadCtxCallback;
    g_HvmR0.pfnSaveHostState     = hmR0DummySaveHostState;
    g_HvmR0.pfnRunGuestCode      = hmR0DummyRunGuestCode;
    g_HvmR0.pfnEnableCpu         = hmR0DummyEnableCpu;
    g_HvmR0.pfnDisableCpu        = hmR0DummyDisableCpu;
    g_HvmR0.pfnInitVM            = hmR0DummyInitVM;
    g_HvmR0.pfnTermVM            = hmR0DummyTermVM;
    g_HvmR0.pfnSetupVM           = hmR0DummySetupVM;

    /* Default is global VT-x/AMD-V init. */
    g_HvmR0.fGlobalInit         = true;

    /*
     * Make sure aCpuInfo is big enough for all the CPUs on this system.
     */
    if (RTMpGetArraySize() > RT_ELEMENTS(g_HvmR0.aCpuInfo))
    {
        LogRel(("HM: Too many real CPUs/cores/threads - %u, max %u\n", RTMpGetArraySize(), RT_ELEMENTS(g_HvmR0.aCpuInfo)));
        return VERR_TOO_MANY_CPUS;
    }

    /*
     * Check for VT-x and AMD-V capabilities.
     */
    int rc;
    if (ASMHasCpuId())
    {
        /* Standard features. */
        uint32_t uMaxLeaf, u32VendorEBX, u32VendorECX, u32VendorEDX;
        ASMCpuId(0, &uMaxLeaf, &u32VendorEBX, &u32VendorECX, &u32VendorEDX);
        if (ASMIsValidStdRange(uMaxLeaf))
        {
            uint32_t u32FeaturesECX, u32FeaturesEDX, u32Dummy;
            ASMCpuId(1, &u32Dummy, &u32Dummy,   &u32FeaturesECX, &u32FeaturesEDX);

            /* Query AMD features. */
            uint32_t uMaxExtLeaf = ASMCpuId_EAX(0x80000000);
            if (ASMIsValidExtRange(uMaxExtLeaf))
                ASMCpuId(0x80000001, &u32Dummy, &u32Dummy,
                         &g_HvmR0.cpuid.u32AMDFeatureECX,
                         &g_HvmR0.cpuid.u32AMDFeatureEDX);
            else
                g_HvmR0.cpuid.u32AMDFeatureECX = g_HvmR0.cpuid.u32AMDFeatureEDX = 0;

            /* Go to CPU specific initialization code. */
            if (   ASMIsIntelCpuEx(u32VendorEBX, u32VendorECX, u32VendorEDX)
                || ASMIsViaCentaurCpuEx(u32VendorEBX, u32VendorECX, u32VendorEDX))
            {
                rc = hmR0InitIntel(u32FeaturesECX, u32FeaturesEDX);
                if (RT_FAILURE(rc))
                    return rc;
            }
            else if (ASMIsAmdCpuEx(u32VendorEBX, u32VendorECX, u32VendorEDX))
            {
                rc = hmR0InitAmd(u32FeaturesEDX, uMaxExtLeaf);
                if (RT_FAILURE(rc))
                    return rc;
            }
            else
                g_HvmR0.lLastError = VERR_HM_UNKNOWN_CPU;
        }
        else
            g_HvmR0.lLastError = VERR_HM_UNKNOWN_CPU;
    }
    else
        g_HvmR0.lLastError = VERR_HM_NO_CPUID;

    /*
     * Register notification callbacks that we can use to disable/enable CPUs
     * when brought offline/online or suspending/resuming.
     */
    if (!g_HvmR0.vmx.fUsingSUPR0EnableVTx)
    {
        rc = RTMpNotificationRegister(hmR0MpEventCallback, NULL);
        AssertRC(rc);

        rc = RTPowerNotificationRegister(hmR0PowerCallback, NULL);
        AssertRC(rc);
    }

    /* We return success here because module init shall not fail if HM
       fails to initialize. */
    return VINF_SUCCESS;
}


/**
 * Does global Ring-0 HM termination (at module termination).
 *
 * @returns VBox status code.
 */
VMMR0_INT_DECL(int) HMR0Term(void)
{
    int rc;
    if (   g_HvmR0.vmx.fSupported
        && g_HvmR0.vmx.fUsingSUPR0EnableVTx)
    {
        /*
         * Simple if the host OS manages VT-x.
         */
        Assert(g_HvmR0.fGlobalInit);
        rc = SUPR0EnableVTx(false /* fEnable */);

        for (unsigned iCpu = 0; iCpu < RT_ELEMENTS(g_HvmR0.aCpuInfo); iCpu++)
        {
            g_HvmR0.aCpuInfo[iCpu].fConfigured = false;
            Assert(g_HvmR0.aCpuInfo[iCpu].hMemObj == NIL_RTR0MEMOBJ);
        }
    }
    else
    {
        Assert(!g_HvmR0.vmx.fUsingSUPR0EnableVTx);
        if (!g_HvmR0.vmx.fUsingSUPR0EnableVTx)
        {
            /* Doesn't really matter if this fails. */
            rc = RTMpNotificationDeregister(hmR0MpEventCallback, NULL);  AssertRC(rc);
            rc = RTPowerNotificationDeregister(hmR0PowerCallback, NULL); AssertRC(rc);
        }
        else
            rc = VINF_SUCCESS;

        /*
         * Disable VT-x/AMD-V on all CPUs if we enabled it before.
         */
        if (g_HvmR0.fGlobalInit)
        {
            HMR0FIRSTRC FirstRc;
            hmR0FirstRcInit(&FirstRc);
            rc = RTMpOnAll(hmR0DisableCpuCallback, NULL /* pvUser 1 */, &FirstRc);
            Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
            if (RT_SUCCESS(rc))
            {
                rc = hmR0FirstRcGetStatus(&FirstRc);
                AssertMsgRC(rc, ("%u: %Rrc\n", hmR0FirstRcGetCpuId(&FirstRc), rc));
            }
        }

        /*
         * Free the per-cpu pages used for VT-x and AMD-V.
         */
        for (unsigned i = 0; i < RT_ELEMENTS(g_HvmR0.aCpuInfo); i++)
        {
            if (g_HvmR0.aCpuInfo[i].hMemObj != NIL_RTR0MEMOBJ)
            {
                RTR0MemObjFree(g_HvmR0.aCpuInfo[i].hMemObj, false);
                g_HvmR0.aCpuInfo[i].hMemObj = NIL_RTR0MEMOBJ;
            }
        }
    }

    /** @todo This needs cleaning up. There's no matching
     *        hmR0TermIntel()/hmR0TermAmd() and all the VT-x/AMD-V specific bits
     *        should move into their respective modules. */
    /* Finally, call global VT-x/AMD-V termination. */
    if (g_HvmR0.vmx.fSupported)
        VMXR0GlobalTerm();
    else if (g_HvmR0.svm.fSupported)
        SVMR0GlobalTerm();

    return rc;
}


/**
 * Worker function used by hmR0PowerCallback() and HMR0Init() to initalize VT-x
 * on a CPU.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     Pointer to the first RC structure.
 * @param   pvUser2     Ignored.
 */
static DECLCALLBACK(void) hmR0InitIntelCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    /** @todo Unify code with SUPR0QueryVTCaps(). */
    PHMR0FIRSTRC pFirstRc = (PHMR0FIRSTRC)pvUser1;
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /** @todo fix idCpu == index assumption (rainy day) */
    NOREF(idCpu); NOREF(pvUser2);

    int rc = SUPR0GetVmxUsability(NULL /* pfIsSmxModeAmbiguous */);
    hmR0FirstRcSetStatus(pFirstRc, rc);
}


/**
 * Worker function used by hmR0PowerCallback() and HMR0Init() to initalize AMD-V
 * on a CPU.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     Pointer to the first RC structure.
 * @param   pvUser2     Ignored.
 */
static DECLCALLBACK(void) hmR0InitAmdCpu(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PHMR0FIRSTRC pFirstRc = (PHMR0FIRSTRC)pvUser1;
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /** @todo fix idCpu == index assumption (rainy day) */
    NOREF(idCpu); NOREF(pvUser2);

    int rc = SUPR0GetSvmUsability(true /* fInitSvm */);
    hmR0FirstRcSetStatus(pFirstRc, rc);
}


/**
 * Enable VT-x or AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM (can be NULL).
 * @param   idCpu       The identifier for the CPU the function is called on.
 *
 * @remarks Maybe called with interrupts disabled!
 */
static int hmR0EnableCpu(PVM pVM, RTCPUID idCpu)
{
    PHMGLOBALCPUINFO pCpu = &g_HvmR0.aCpuInfo[idCpu];

    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /** @todo fix idCpu == index assumption (rainy day) */
    Assert(idCpu < RT_ELEMENTS(g_HvmR0.aCpuInfo));
    Assert(!pCpu->fConfigured);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    pCpu->idCpu = idCpu;
    /* Do NOT reset cTlbFlushes here, see @bugref{6255}. */

    int rc;
    if (g_HvmR0.vmx.fSupported && g_HvmR0.vmx.fUsingSUPR0EnableVTx)
        rc = g_HvmR0.pfnEnableCpu(pCpu, pVM, NULL /* pvCpuPage */, NIL_RTHCPHYS, true, &g_HvmR0.vmx.Msrs);
    else
    {
        AssertLogRelMsgReturn(pCpu->hMemObj != NIL_RTR0MEMOBJ, ("hmR0EnableCpu failed idCpu=%u.\n", idCpu), VERR_HM_IPE_1);
        void    *pvCpuPage     = RTR0MemObjAddress(pCpu->hMemObj);
        RTHCPHYS HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0 /* iPage */);

        if (g_HvmR0.vmx.fSupported)
            rc = g_HvmR0.pfnEnableCpu(pCpu, pVM, pvCpuPage, HCPhysCpuPage, false, &g_HvmR0.vmx.Msrs);
        else
            rc = g_HvmR0.pfnEnableCpu(pCpu, pVM, pvCpuPage, HCPhysCpuPage, false, NULL /* pvArg */);
    }
    AssertRC(rc);
    if (RT_SUCCESS(rc))
        pCpu->fConfigured = true;

    return rc;
}


/**
 * Worker function passed to RTMpOnAll() that is to be called on all CPUs.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     Opaque pointer to the VM (can be NULL!).
 * @param   pvUser2     The 2nd user argument.
 */
static DECLCALLBACK(void) hmR0EnableCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PVM             pVM      = (PVM)pvUser1;     /* can be NULL! */
    PHMR0FIRSTRC    pFirstRc = (PHMR0FIRSTRC)pvUser2;
    AssertReturnVoid(g_HvmR0.fGlobalInit);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    hmR0FirstRcSetStatus(pFirstRc, hmR0EnableCpu(pVM, idCpu));
}


/**
 * RTOnce callback employed by HMR0EnableAllCpus.
 *
 * @returns VBox status code.
 * @param   pvUser          Pointer to the VM.
 * @param   pvUserIgnore    NULL, ignored.
 */
static DECLCALLBACK(int32_t) hmR0EnableAllCpuOnce(void *pvUser)
{
    PVM pVM = (PVM)pvUser;

    /*
     * Indicate that we've initialized.
     *
     * Note! There is a potential race between this function and the suspend
     *       notification.  Kind of unlikely though, so ignored for now.
     */
    AssertReturn(!g_HvmR0.fEnabled, VERR_HM_ALREADY_ENABLED_IPE);
    ASMAtomicWriteBool(&g_HvmR0.fEnabled, true);

    /*
     * The global init variable is set by the first VM.
     */
    g_HvmR0.fGlobalInit = pVM->hm.s.fGlobalInit;

#ifdef VBOX_STRICT
    for (unsigned i = 0; i < RT_ELEMENTS(g_HvmR0.aCpuInfo); i++)
    {
        Assert(g_HvmR0.aCpuInfo[i].hMemObj == NIL_RTR0MEMOBJ);
        Assert(!g_HvmR0.aCpuInfo[i].fConfigured);
        Assert(!g_HvmR0.aCpuInfo[i].cTlbFlushes);
        Assert(!g_HvmR0.aCpuInfo[i].uCurrentAsid);
    }
#endif

    int rc;
    if (   g_HvmR0.vmx.fSupported
        && g_HvmR0.vmx.fUsingSUPR0EnableVTx)
    {
        /*
         * Global VT-x initialization API (only darwin for now).
         */
        rc = SUPR0EnableVTx(true /* fEnable */);
        if (RT_SUCCESS(rc))
        {
            /* If the host provides a VT-x init API, then we'll rely on that for global init. */
            g_HvmR0.fGlobalInit = pVM->hm.s.fGlobalInit = true;
        }
        else
            AssertMsgFailed(("hmR0EnableAllCpuOnce/SUPR0EnableVTx: rc=%Rrc\n", rc));
    }
    else
    {
        /*
         * We're doing the job ourselves.
         */
        /* Allocate one page per cpu for the global VT-x and AMD-V pages */
        for (unsigned i = 0; i < RT_ELEMENTS(g_HvmR0.aCpuInfo); i++)
        {
            Assert(g_HvmR0.aCpuInfo[i].hMemObj == NIL_RTR0MEMOBJ);

            if (RTMpIsCpuPossible(RTMpCpuIdFromSetIndex(i)))
            {
                rc = RTR0MemObjAllocCont(&g_HvmR0.aCpuInfo[i].hMemObj, PAGE_SIZE, false /* executable R0 mapping */);
                AssertLogRelRCReturn(rc, rc);

                void *pvR0 = RTR0MemObjAddress(g_HvmR0.aCpuInfo[i].hMemObj); Assert(pvR0);
                ASMMemZeroPage(pvR0);
            }
        }

        rc = VINF_SUCCESS;
    }

    if (   RT_SUCCESS(rc)
        && g_HvmR0.fGlobalInit)
    {
        /* First time, so initialize each cpu/core. */
        HMR0FIRSTRC FirstRc;
        hmR0FirstRcInit(&FirstRc);
        rc = RTMpOnAll(hmR0EnableCpuCallback, (void *)pVM, &FirstRc);
        if (RT_SUCCESS(rc))
            rc = hmR0FirstRcGetStatus(&FirstRc);
        AssertMsgRC(rc, ("hmR0EnableAllCpuOnce failed for cpu %d with rc=%d\n", hmR0FirstRcGetCpuId(&FirstRc), rc));
    }

    return rc;
}


/**
 * Sets up HM on all cpus.
 *
 * @returns VBox status code.
 * @param   pVM                 Pointer to the VM.
 */
VMMR0_INT_DECL(int) HMR0EnableAllCpus(PVM pVM)
{
    /* Make sure we don't touch HM after we've disabled HM in preparation of a suspend. */
    if (ASMAtomicReadBool(&g_HvmR0.fSuspended))
        return VERR_HM_SUSPEND_PENDING;

    return RTOnce(&g_HvmR0.EnableAllCpusOnce, hmR0EnableAllCpuOnce, pVM);
}


/**
 * Disable VT-x or AMD-V on the current CPU.
 *
 * @returns VBox status code.
 * @param   idCpu       The identifier for the CPU this function is called on.
 *
 * @remarks Must be called with preemption disabled.
 */
static int hmR0DisableCpu(RTCPUID idCpu)
{
    PHMGLOBALCPUINFO pCpu = &g_HvmR0.aCpuInfo[idCpu];

    Assert(!g_HvmR0.vmx.fSupported || !g_HvmR0.vmx.fUsingSUPR0EnableVTx);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /** @todo fix idCpu == index assumption (rainy day) */
    Assert(idCpu < RT_ELEMENTS(g_HvmR0.aCpuInfo));
    Assert(!pCpu->fConfigured || pCpu->hMemObj != NIL_RTR0MEMOBJ);
    AssertRelease(idCpu == RTMpCpuId());

    if (pCpu->hMemObj == NIL_RTR0MEMOBJ)
        return pCpu->fConfigured ? VERR_NO_MEMORY : VINF_SUCCESS /* not initialized. */;

    int rc;
    if (pCpu->fConfigured)
    {
        void    *pvCpuPage     = RTR0MemObjAddress(pCpu->hMemObj);
        RTHCPHYS HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);

        rc = g_HvmR0.pfnDisableCpu(pCpu, pvCpuPage, HCPhysCpuPage);
        AssertRCReturn(rc, rc);

        pCpu->fConfigured = false;
        pCpu->idCpu = NIL_RTCPUID;
    }
    else
        rc = VINF_SUCCESS; /* nothing to do */
    return rc;
}


/**
 * Worker function passed to RTMpOnAll() that is to be called on the target
 * CPUs.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     The 1st user argument.
 * @param   pvUser2     Opaque pointer to the FirstRc.
 */
static DECLCALLBACK(void) hmR0DisableCpuCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PHMR0FIRSTRC pFirstRc = (PHMR0FIRSTRC)pvUser2; NOREF(pvUser1);
    AssertReturnVoid(g_HvmR0.fGlobalInit);
    hmR0FirstRcSetStatus(pFirstRc, hmR0DisableCpu(idCpu));
}


/**
 * Worker function passed to RTMpOnSpecific() that is to be called on the target
 * CPU.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     Null, not used.
 * @param   pvUser2     Null, not used.
 */
static DECLCALLBACK(void) hmR0DisableCpuOnSpecificCallback(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    NOREF(pvUser1);
    NOREF(pvUser2);
    hmR0DisableCpu(idCpu);
}


/**
 * Callback function invoked when a cpu goes online or offline.
 *
 * @param   enmEvent            The Mp event.
 * @param   idCpu               The identifier for the CPU the function is called on.
 * @param   pvData              Opaque data (PVM pointer).
 */
static DECLCALLBACK(void) hmR0MpEventCallback(RTMPEVENT enmEvent, RTCPUID idCpu, void *pvData)
{
    NOREF(pvData);

    /*
     * We only care about uninitializing a CPU that is going offline. When a
     * CPU comes online, the initialization is done lazily in HMR0Enter().
     */
    switch (enmEvent)
    {
        case RTMPEVENT_OFFLINE:
        {
            RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
            RTThreadPreemptDisable(&PreemptState);
            if (idCpu == RTMpCpuId())
            {
                int rc = hmR0DisableCpu(idCpu);
                AssertRC(rc);
                RTThreadPreemptRestore(&PreemptState);
            }
            else
            {
                RTThreadPreemptRestore(&PreemptState);
                RTMpOnSpecific(idCpu, hmR0DisableCpuOnSpecificCallback, NULL /* pvUser1 */, NULL /* pvUser2 */);
            }
            break;
        }

        default:
            break;
    }
}


/**
 * Called whenever a system power state change occurs.
 *
 * @param   enmEvent        The Power event.
 * @param   pvUser          User argument.
 */
static DECLCALLBACK(void) hmR0PowerCallback(RTPOWEREVENT enmEvent, void *pvUser)
{
    NOREF(pvUser);
    Assert(!g_HvmR0.vmx.fSupported || !g_HvmR0.vmx.fUsingSUPR0EnableVTx);

#ifdef LOG_ENABLED
    if (enmEvent == RTPOWEREVENT_SUSPEND)
        SUPR0Printf("hmR0PowerCallback RTPOWEREVENT_SUSPEND\n");
    else
        SUPR0Printf("hmR0PowerCallback RTPOWEREVENT_RESUME\n");
#endif

    if (enmEvent == RTPOWEREVENT_SUSPEND)
        ASMAtomicWriteBool(&g_HvmR0.fSuspended, true);

    if (g_HvmR0.fEnabled)
    {
        int         rc;
        HMR0FIRSTRC FirstRc;
        hmR0FirstRcInit(&FirstRc);

        if (enmEvent == RTPOWEREVENT_SUSPEND)
        {
            if (g_HvmR0.fGlobalInit)
            {
                /* Turn off VT-x or AMD-V on all CPUs. */
                rc = RTMpOnAll(hmR0DisableCpuCallback, NULL /* pvUser 1 */, &FirstRc);
                Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
            }
            /* else nothing to do here for the local init case */
        }
        else
        {
            /* Reinit the CPUs from scratch as the suspend state might have
               messed with the MSRs. (lousy BIOSes as usual) */
            if (g_HvmR0.vmx.fSupported)
                rc = RTMpOnAll(hmR0InitIntelCpu, &FirstRc, NULL);
            else
                rc = RTMpOnAll(hmR0InitAmdCpu, &FirstRc, NULL);
            Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
            if (RT_SUCCESS(rc))
                rc = hmR0FirstRcGetStatus(&FirstRc);
#ifdef LOG_ENABLED
            if (RT_FAILURE(rc))
                SUPR0Printf("hmR0PowerCallback hmR0InitXxxCpu failed with %Rc\n", rc);
#endif
            if (g_HvmR0.fGlobalInit)
            {
                /* Turn VT-x or AMD-V back on on all CPUs. */
                rc = RTMpOnAll(hmR0EnableCpuCallback, NULL /* pVM */, &FirstRc /* output ignored */);
                Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
            }
            /* else nothing to do here for the local init case */
        }
    }

    if (enmEvent == RTPOWEREVENT_RESUME)
        ASMAtomicWriteBool(&g_HvmR0.fSuspended, false);
}


/**
 * Does ring-0 per-VM HM initialization.
 *
 * This will copy HM global into the VM structure and call the CPU specific
 * init routine which will allocate resources for each virtual CPU and such.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 *
 * @remarks This is called after HMR3Init(), see vmR3CreateU() and
 *          vmR3InitRing3().
 */
VMMR0_INT_DECL(int) HMR0InitVM(PVM pVM)
{
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("HMR0InitVM: %p\n", pVM);
#endif

    /* Make sure we don't touch HM after we've disabled HM in preparation of a suspend. */
    if (ASMAtomicReadBool(&g_HvmR0.fSuspended))
        return VERR_HM_SUSPEND_PENDING;

    /*
     * Copy globals to the VM structure.
     */
    pVM->hm.s.vmx.fSupported            = g_HvmR0.vmx.fSupported;
    pVM->hm.s.svm.fSupported            = g_HvmR0.svm.fSupported;

    pVM->hm.s.vmx.fUsePreemptTimer     &= g_HvmR0.vmx.fUsePreemptTimer;     /* Can be overridden by CFGM. See HMR3Init(). */
    pVM->hm.s.vmx.cPreemptTimerShift    = g_HvmR0.vmx.cPreemptTimerShift;
    pVM->hm.s.vmx.u64HostCr4            = g_HvmR0.vmx.u64HostCr4;
    pVM->hm.s.vmx.u64HostEfer           = g_HvmR0.vmx.u64HostEfer;
    pVM->hm.s.vmx.Msrs                  = g_HvmR0.vmx.Msrs;
    pVM->hm.s.svm.u64MsrHwcr            = g_HvmR0.svm.u64MsrHwcr;
    pVM->hm.s.svm.u32Rev                = g_HvmR0.svm.u32Rev;
    pVM->hm.s.svm.u32Features           = g_HvmR0.svm.u32Features;
    pVM->hm.s.cpuid.u32AMDFeatureECX    = g_HvmR0.cpuid.u32AMDFeatureECX;
    pVM->hm.s.cpuid.u32AMDFeatureEDX    = g_HvmR0.cpuid.u32AMDFeatureEDX;
    pVM->hm.s.lLastError                = g_HvmR0.lLastError;

    pVM->hm.s.uMaxAsid                  = g_HvmR0.uMaxAsid;


    if (!pVM->hm.s.cMaxResumeLoops) /* allow ring-3 overrides */
    {
        pVM->hm.s.cMaxResumeLoops       = 1024;
        if (RTThreadPreemptIsPendingTrusty())
            pVM->hm.s.cMaxResumeLoops   = 8192;
    }

    /*
     * Initialize some per CPU fields.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->hm.s.idEnteredCpu = NIL_RTCPUID;
        pVCpu->hm.s.idLastCpu    = NIL_RTCPUID;

        /* We'll aways increment this the first time (host uses ASID 0). */
        AssertReturn(!pVCpu->hm.s.uCurrentAsid, VERR_HM_IPE_3);
    }

    pVM->hm.s.uHostKernelFeatures = SUPR0GetKernelFeatures();

    /*
     * Call the hardware specific initialization method.
     */
    return g_HvmR0.pfnInitVM(pVM);
}


/**
 * Does ring-0 per VM HM termination.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0_INT_DECL(int) HMR0TermVM(PVM pVM)
{
    Log(("HMR0TermVM: %p\n", pVM));
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /*
     * Call the hardware specific method.
     *
     * Note! We might be preparing for a suspend, so the pfnTermVM() functions should probably not
     * mess with VT-x/AMD-V features on the CPU, currently all they do is free memory so this is safe.
     */
    return g_HvmR0.pfnTermVM(pVM);
}


/**
 * Sets up a VT-x or AMD-V session.
 *
 * This is mostly about setting up the hardware VM state.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0_INT_DECL(int) HMR0SetupVM(PVM pVM)
{
    Log(("HMR0SetupVM: %p\n", pVM));
    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* Make sure we don't touch HM after we've disabled HM in preparation of a suspend. */
    AssertReturn(!ASMAtomicReadBool(&g_HvmR0.fSuspended), VERR_HM_SUSPEND_PENDING);

    /* On first entry we'll sync everything. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
        HMCPU_CF_RESET_TO(&pVM->aCpus[i], HM_CHANGED_HOST_CONTEXT | HM_CHANGED_ALL_GUEST);

    /*
     * Call the hardware specific setup VM method. This requires the CPU to be
     * enabled for AMD-V/VT-x and preemption to be prevented.
     */
    RTTHREADPREEMPTSTATE PreemptState = RTTHREADPREEMPTSTATE_INITIALIZER;
    RTThreadPreemptDisable(&PreemptState);
    RTCPUID          idCpu  = RTMpCpuId();

    /* Enable VT-x or AMD-V if local init is required. */
    int rc;
    if (!g_HvmR0.fGlobalInit)
    {
        rc = hmR0EnableCpu(pVM, idCpu);
        AssertRCReturnStmt(rc, RTThreadPreemptRestore(&PreemptState), rc);
    }

    /* Setup VT-x or AMD-V. */
    rc = g_HvmR0.pfnSetupVM(pVM);

    /* Disable VT-x or AMD-V if local init was done before. */
    if (!g_HvmR0.fGlobalInit)
    {
        int rc2 = hmR0DisableCpu(idCpu);
        AssertRC(rc2);
    }

    RTThreadPreemptRestore(&PreemptState);
    return rc;
}


/**
 * Turns on HM on the CPU if necessary and initializes the bare minimum state
 * required for entering HM context.
 *
 * @returns VBox status code.
 * @param   pvCpu       Pointer to the VMCPU.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0_INT_DECL(int) HMR0EnterCpu(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    int              rc    = VINF_SUCCESS;
    RTCPUID          idCpu = RTMpCpuId();
    PHMGLOBALCPUINFO pCpu = &g_HvmR0.aCpuInfo[idCpu];
    AssertPtr(pCpu);

    /* Enable VT-x or AMD-V if local init is required, or enable if it's a freshly onlined CPU. */
    if (!pCpu->fConfigured)
        rc = hmR0EnableCpu(pVCpu->CTX_SUFF(pVM), idCpu);

    /* Reload host-state (back from ring-3/migrated CPUs) and shared guest/host bits. */
    HMCPU_CF_SET(pVCpu, HM_CHANGED_HOST_CONTEXT | HM_CHANGED_HOST_GUEST_SHARED_STATE);

    Assert(pCpu->idCpu == idCpu && pCpu->idCpu != NIL_RTCPUID);
    pVCpu->hm.s.idEnteredCpu = idCpu;
    return rc;
}


/**
 * Enters the VT-x or AMD-V session.
 *
 * @returns VBox status code.
 * @param   pVM        Pointer to the VM.
 * @param   pVCpu      Pointer to the VMCPU.
 *
 * @remarks This is called with preemption disabled.
 */
VMMR0_INT_DECL(int) HMR0Enter(PVM pVM, PVMCPU pVCpu)
{
    /* Make sure we can't enter a session after we've disabled HM in preparation of a suspend. */
    AssertReturn(!ASMAtomicReadBool(&g_HvmR0.fSuspended), VERR_HM_SUSPEND_PENDING);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    /* Load the bare minimum state required for entering HM. */
    int rc = HMR0EnterCpu(pVCpu);
    AssertRCReturn(rc, rc);

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    AssertReturn(!VMMR0ThreadCtxHooksAreRegistered(pVCpu), VERR_HM_IPE_5);
    bool fStartedSet = PGMR0DynMapStartOrMigrateAutoSet(pVCpu);
#endif

    RTCPUID          idCpu = RTMpCpuId();
    PHMGLOBALCPUINFO pCpu  = &g_HvmR0.aCpuInfo[idCpu];
    Assert(pCpu);
    Assert(HMCPU_CF_IS_SET(pVCpu, HM_CHANGED_HOST_CONTEXT | HM_CHANGED_HOST_GUEST_SHARED_STATE));

    rc = g_HvmR0.pfnEnterSession(pVM, pVCpu, pCpu);
    AssertMsgRCReturn(rc, ("pfnEnterSession failed. rc=%Rrc pVCpu=%p HostCpuId=%u\n", rc, pVCpu, idCpu), rc);

    /* Load the host-state as we may be resuming code after a longjmp and quite
       possibly now be scheduled on a different CPU. */
    rc = g_HvmR0.pfnSaveHostState(pVM, pVCpu);
    AssertMsgRCReturn(rc, ("pfnSaveHostState failed. rc=%Rrc pVCpu=%p HostCpuId=%u\n", rc, pVCpu, idCpu), rc);

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    if (fStartedSet)
        PGMRZDynMapReleaseAutoSet(pVCpu);
#endif

    /* Keep track of the CPU owning the VMCS for debugging scheduling weirdness and ring-3 calls. */
    if (RT_FAILURE(rc))
        pVCpu->hm.s.idEnteredCpu = NIL_RTCPUID;
    return rc;
}


/**
 * Deinitializes the bare minimum state used for HM context and if necessary
 * disable HM on the CPU.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0_INT_DECL(int) HMR0LeaveCpu(PVMCPU pVCpu)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    VMCPU_ASSERT_EMT_RETURN(pVCpu, VERR_HM_WRONG_CPU);

    RTCPUID          idCpu = RTMpCpuId();
    PHMGLOBALCPUINFO pCpu  = &g_HvmR0.aCpuInfo[idCpu];

    if (   !g_HvmR0.fGlobalInit
        && pCpu->fConfigured)
    {
        int rc = hmR0DisableCpu(idCpu);
        AssertRCReturn(rc, rc);
        Assert(!pCpu->fConfigured);
        Assert(pCpu->idCpu == NIL_RTCPUID);

        /* For obtaining a non-zero ASID/VPID on next re-entry. */
        pVCpu->hm.s.idLastCpu = NIL_RTCPUID;
    }

    /* Clear it while leaving HM context, hmPokeCpuForTlbFlush() relies on this. */
    pVCpu->hm.s.idEnteredCpu = NIL_RTCPUID;

    return VINF_SUCCESS;
}


/**
 * Thread-context hook for HM.
 *
 * @param   enmEvent        The thread-context event.
 * @param   pvUser          Opaque pointer to the VMCPU.
 */
VMMR0_INT_DECL(void) HMR0ThreadCtxCallback(RTTHREADCTXEVENT enmEvent, void *pvUser)
{
    PVMCPU pVCpu = (PVMCPU)pvUser;
    Assert(pVCpu);
    Assert(g_HvmR0.pfnThreadCtxCallback);

    g_HvmR0.pfnThreadCtxCallback(enmEvent, pVCpu, g_HvmR0.fGlobalInit);
}


/**
 * Runs guest code in a hardware accelerated VM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks Can be called with preemption enabled if thread-context hooks are
 *          used!!!
 */
VMMR0_INT_DECL(int) HMR0RunGuestCode(PVM pVM, PVMCPU pVCpu)
{
#ifdef VBOX_STRICT
    /* With thread-context hooks we would be running this code with preemption enabled. */
    if (!RTThreadPreemptIsEnabled(NIL_RTTHREAD))
    {
        PHMGLOBALCPUINFO pCpu = &g_HvmR0.aCpuInfo[RTMpCpuId()];
        Assert(!VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_PGM_SYNC_CR3 | VMCPU_FF_PGM_SYNC_CR3_NON_GLOBAL));
        Assert(pCpu->fConfigured);
        AssertReturn(!ASMAtomicReadBool(&g_HvmR0.fSuspended), VERR_HM_SUSPEND_PENDING);
    }
#endif

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    AssertReturn(!VMMR0ThreadCtxHooksAreRegistered(pVCpu), VERR_HM_IPE_4);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    PGMRZDynMapStartAutoSet(pVCpu);
#endif

    int rc = g_HvmR0.pfnRunGuestCode(pVM, pVCpu, CPUMQueryGuestCtxPtr(pVCpu));

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    PGMRZDynMapReleaseAutoSet(pVCpu);
#endif
    return rc;
}

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)

/**
 * Save guest FPU/XMM state (64 bits guest mode & 32 bits host only)
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0_INT_DECL(int)   HMR0SaveFPUState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    STAM_COUNTER_INC(&pVCpu->hm.s.StatFpu64SwitchBack);
    if (pVM->hm.s.vmx.fSupported)
        return VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_HMRCSaveGuestFPU64, 0, NULL);
    return SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_HMRCSaveGuestFPU64, 0, NULL);
}


/**
 * Save guest debug state (64 bits guest mode & 32 bits host only)
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0_INT_DECL(int)   HMR0SaveDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    STAM_COUNTER_INC(&pVCpu->hm.s.StatDebug64SwitchBack);
    if (pVM->hm.s.vmx.fSupported)
        return VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_HMRCSaveGuestDebug64, 0, NULL);
    return SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_HMRCSaveGuestDebug64, 0, NULL);
}


/**
 * Test the 32->64 bits switcher.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0_INT_DECL(int)   HMR0TestSwitcher3264(PVM pVM)
{
    PVMCPU   pVCpu     = &pVM->aCpus[0];
    PCPUMCTX pCtx      = CPUMQueryGuestCtxPtr(pVCpu);
    uint32_t aParam[5] = {0, 1, 2, 3, 4};
    int      rc;

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatWorldSwitch3264, z);
    if (pVM->hm.s.vmx.fSupported)
        rc = VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_HMRCTestSwitcher64, 5, &aParam[0]);
    else
        rc = SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_HMRCTestSwitcher64, 5, &aParam[0]);
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatWorldSwitch3264, z);

    return rc;
}

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */

/**
 * Returns suspend status of the host.
 *
 * @returns Suspend pending or not.
 */
VMMR0_INT_DECL(bool) HMR0SuspendPending(void)
{
    return ASMAtomicReadBool(&g_HvmR0.fSuspended);
}


/**
 * Returns the cpu structure for the current cpu.
 * Keep in mind that there is no guarantee it will stay the same (long jumps to ring 3!!!).
 *
 * @returns The cpu structure pointer.
 */
VMMR0DECL(PHMGLOBALCPUINFO) HMR0GetCurrentCpu(void)
{
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));
    RTCPUID idCpu = RTMpCpuId();
    Assert(idCpu < RT_ELEMENTS(g_HvmR0.aCpuInfo));
    return &g_HvmR0.aCpuInfo[idCpu];
}


/**
 * Returns the cpu structure for the current cpu.
 * Keep in mind that there is no guarantee it will stay the same (long jumps to ring 3!!!).
 *
 * @returns The cpu structure pointer.
 * @param   idCpu       id of the VCPU.
 */
VMMR0DECL(PHMGLOBALCPUINFO) HMR0GetCurrentCpuEx(RTCPUID idCpu)
{
    Assert(idCpu < RT_ELEMENTS(g_HvmR0.aCpuInfo));
    return &g_HvmR0.aCpuInfo[idCpu];
}


/**
 * Save a pending IO read.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   GCPtrRip        Address of IO instruction.
 * @param   GCPtrRipNext    Address of the next instruction.
 * @param   uPort           Port address.
 * @param   uAndVal         AND mask for saving the result in eax.
 * @param   cbSize          Read size.
 */
VMMR0_INT_DECL(void) HMR0SavePendingIOPortRead(PVMCPU pVCpu, RTGCPTR GCPtrRip, RTGCPTR GCPtrRipNext,
                                               unsigned uPort, unsigned uAndVal, unsigned cbSize)
{
    pVCpu->hm.s.PendingIO.enmType         = HMPENDINGIO_PORT_READ;
    pVCpu->hm.s.PendingIO.GCPtrRip        = GCPtrRip;
    pVCpu->hm.s.PendingIO.GCPtrRipNext    = GCPtrRipNext;
    pVCpu->hm.s.PendingIO.s.Port.uPort    = uPort;
    pVCpu->hm.s.PendingIO.s.Port.uAndVal  = uAndVal;
    pVCpu->hm.s.PendingIO.s.Port.cbSize   = cbSize;
    return;
}


/**
 * Save a pending IO write.
 *
 * @param   pVCpu           Pointer to the VMCPU.
 * @param   GCPtrRIP        Address of IO instruction.
 * @param   uPort           Port address.
 * @param   uAndVal         AND mask for fetching the result from eax.
 * @param   cbSize          Read size.
 */
VMMR0_INT_DECL(void) HMR0SavePendingIOPortWrite(PVMCPU pVCpu, RTGCPTR GCPtrRip, RTGCPTR GCPtrRipNext,
                                                unsigned uPort, unsigned uAndVal, unsigned cbSize)
{
    pVCpu->hm.s.PendingIO.enmType         = HMPENDINGIO_PORT_WRITE;
    pVCpu->hm.s.PendingIO.GCPtrRip        = GCPtrRip;
    pVCpu->hm.s.PendingIO.GCPtrRipNext    = GCPtrRipNext;
    pVCpu->hm.s.PendingIO.s.Port.uPort    = uPort;
    pVCpu->hm.s.PendingIO.s.Port.uAndVal  = uAndVal;
    pVCpu->hm.s.PendingIO.s.Port.cbSize   = cbSize;
    return;
}


/**
 * Raw-mode switcher hook - disable VT-x if it's active *and* the current
 * switcher turns off paging.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   enmSwitcher     The switcher we're about to use.
 * @param   pfVTxDisabled   Where to store whether VT-x was disabled or not.
 */
VMMR0_INT_DECL(int) HMR0EnterSwitcher(PVM pVM, VMMSWITCHER enmSwitcher, bool *pfVTxDisabled)
{
    NOREF(pVM);

    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    *pfVTxDisabled = false;

    /* No such issues with AMD-V */
    if (!g_HvmR0.vmx.fSupported)
        return VINF_SUCCESS;

    /* Check if the swithcing we're up to is safe. */
    switch (enmSwitcher)
    {
        case VMMSWITCHER_32_TO_32:
        case VMMSWITCHER_PAE_TO_PAE:
            return VINF_SUCCESS;    /* safe switchers as they don't turn off paging */

        case VMMSWITCHER_32_TO_PAE:
        case VMMSWITCHER_PAE_TO_32: /* is this one actually used?? */
        case VMMSWITCHER_AMD64_TO_32:
        case VMMSWITCHER_AMD64_TO_PAE:
            break;                  /* unsafe switchers */

        default:
            AssertFailedReturn(VERR_HM_WRONG_SWITCHER);
    }

    /* When using SUPR0EnableVTx we must let the host suspend and resume VT-x,
       regardless of whether we're currently using VT-x or not. */
    if (g_HvmR0.vmx.fUsingSUPR0EnableVTx)
    {
        *pfVTxDisabled = SUPR0SuspendVTxOnCpu();
        return VINF_SUCCESS;
    }

    /** @todo Check if this code is presumtive wrt other VT-x users on the
    *        system... */

    /* Nothing to do if we haven't enabled VT-x. */
    if (!g_HvmR0.fEnabled)
        return VINF_SUCCESS;

    /* Local init implies the CPU is currently not in VMX root mode. */
    if (!g_HvmR0.fGlobalInit)
        return VINF_SUCCESS;

    /* Ok, disable VT-x. */
    PHMGLOBALCPUINFO pCpu = HMR0GetCurrentCpu();
    AssertReturn(pCpu && pCpu->hMemObj != NIL_RTR0MEMOBJ, VERR_HM_IPE_2);

    *pfVTxDisabled = true;
    void    *pvCpuPage     = RTR0MemObjAddress(pCpu->hMemObj);
    RTHCPHYS HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);
    return VMXR0DisableCpu(pCpu, pvCpuPage, HCPhysCpuPage);
}


/**
 * Raw-mode switcher hook - re-enable VT-x if was active *and* the current
 * switcher turned off paging.
 *
 * @param   pVM             Pointer to the VM.
 * @param   fVTxDisabled    Whether VT-x was disabled or not.
 */
VMMR0_INT_DECL(void) HMR0LeaveSwitcher(PVM pVM, bool fVTxDisabled)
{
    Assert(!ASMIntAreEnabled());

    if (!fVTxDisabled)
        return;         /* nothing to do */

    Assert(g_HvmR0.vmx.fSupported);
    if (g_HvmR0.vmx.fUsingSUPR0EnableVTx)
        SUPR0ResumeVTxOnCpu(fVTxDisabled);
    else
    {
        Assert(g_HvmR0.fEnabled);
        Assert(g_HvmR0.fGlobalInit);

        PHMGLOBALCPUINFO pCpu = HMR0GetCurrentCpu();
        AssertReturnVoid(pCpu && pCpu->hMemObj != NIL_RTR0MEMOBJ);

        void           *pvCpuPage     = RTR0MemObjAddress(pCpu->hMemObj);
        RTHCPHYS        HCPhysCpuPage = RTR0MemObjGetPagePhysAddr(pCpu->hMemObj, 0);
        VMXR0EnableCpu(pCpu, pVM, pvCpuPage, HCPhysCpuPage, false, &g_HvmR0.vmx.Msrs);
    }
}

#ifdef VBOX_STRICT

/**
 * Dumps a descriptor.
 *
 * @param   pDesc    Descriptor to dump.
 * @param   Sel      Selector number.
 * @param   pszMsg   Message to prepend the log entry with.
 */
VMMR0DECL(void) HMR0DumpDescriptor(PCX86DESCHC pDesc, RTSEL Sel, const char *pszMsg)
{
    /*
     * Make variable description string.
     */
    static struct
    {
        unsigned    cch;
        const char *psz;
    } const s_aTypes[32] =
    {
# define STRENTRY(str) { sizeof(str) - 1, str }

        /* system */
# if HC_ARCH_BITS == 64
        STRENTRY("Reserved0 "),                  /* 0x00 */
        STRENTRY("Reserved1 "),                  /* 0x01 */
        STRENTRY("LDT "),                        /* 0x02 */
        STRENTRY("Reserved3 "),                  /* 0x03 */
        STRENTRY("Reserved4 "),                  /* 0x04 */
        STRENTRY("Reserved5 "),                  /* 0x05 */
        STRENTRY("Reserved6 "),                  /* 0x06 */
        STRENTRY("Reserved7 "),                  /* 0x07 */
        STRENTRY("Reserved8 "),                  /* 0x08 */
        STRENTRY("TSS64Avail "),                 /* 0x09 */
        STRENTRY("ReservedA "),                  /* 0x0a */
        STRENTRY("TSS64Busy "),                  /* 0x0b */
        STRENTRY("Call64 "),                     /* 0x0c */
        STRENTRY("ReservedD "),                  /* 0x0d */
        STRENTRY("Int64 "),                      /* 0x0e */
        STRENTRY("Trap64 "),                     /* 0x0f */
# else
        STRENTRY("Reserved0 "),                  /* 0x00 */
        STRENTRY("TSS16Avail "),                 /* 0x01 */
        STRENTRY("LDT "),                        /* 0x02 */
        STRENTRY("TSS16Busy "),                  /* 0x03 */
        STRENTRY("Call16 "),                     /* 0x04 */
        STRENTRY("Task "),                       /* 0x05 */
        STRENTRY("Int16 "),                      /* 0x06 */
        STRENTRY("Trap16 "),                     /* 0x07 */
        STRENTRY("Reserved8 "),                  /* 0x08 */
        STRENTRY("TSS32Avail "),                 /* 0x09 */
        STRENTRY("ReservedA "),                  /* 0x0a */
        STRENTRY("TSS32Busy "),                  /* 0x0b */
        STRENTRY("Call32 "),                     /* 0x0c */
        STRENTRY("ReservedD "),                  /* 0x0d */
        STRENTRY("Int32 "),                      /* 0x0e */
        STRENTRY("Trap32 "),                     /* 0x0f */
# endif
        /* non system */
        STRENTRY("DataRO "),                     /* 0x10 */
        STRENTRY("DataRO Accessed "),            /* 0x11 */
        STRENTRY("DataRW "),                     /* 0x12 */
        STRENTRY("DataRW Accessed "),            /* 0x13 */
        STRENTRY("DataDownRO "),                 /* 0x14 */
        STRENTRY("DataDownRO Accessed "),        /* 0x15 */
        STRENTRY("DataDownRW "),                 /* 0x16 */
        STRENTRY("DataDownRW Accessed "),        /* 0x17 */
        STRENTRY("CodeEO "),                     /* 0x18 */
        STRENTRY("CodeEO Accessed "),            /* 0x19 */
        STRENTRY("CodeER "),                     /* 0x1a */
        STRENTRY("CodeER Accessed "),            /* 0x1b */
        STRENTRY("CodeConfEO "),                 /* 0x1c */
        STRENTRY("CodeConfEO Accessed "),        /* 0x1d */
        STRENTRY("CodeConfER "),                 /* 0x1e */
        STRENTRY("CodeConfER Accessed ")         /* 0x1f */
# undef SYSENTRY
    };
# define ADD_STR(psz, pszAdd) do { strcpy(psz, pszAdd); psz += strlen(pszAdd); } while (0)
    char        szMsg[128];
    char       *psz = &szMsg[0];
    unsigned    i = pDesc->Gen.u1DescType << 4 | pDesc->Gen.u4Type;
    memcpy(psz, s_aTypes[i].psz, s_aTypes[i].cch);
    psz += s_aTypes[i].cch;

    if (pDesc->Gen.u1Present)
        ADD_STR(psz, "Present ");
    else
        ADD_STR(psz, "Not-Present ");
# if HC_ARCH_BITS == 64
    if (pDesc->Gen.u1Long)
        ADD_STR(psz, "64-bit ");
    else
        ADD_STR(psz, "Comp   ");
# else
    if (pDesc->Gen.u1Granularity)
        ADD_STR(psz, "Page ");
    if (pDesc->Gen.u1DefBig)
        ADD_STR(psz, "32-bit ");
    else
        ADD_STR(psz, "16-bit ");
# endif
# undef ADD_STR
    *psz = '\0';

    /*
     * Limit and Base and format the output.
     */
    uint32_t    u32Limit = X86DESC_LIMIT_G(pDesc);

# if HC_ARCH_BITS == 64
    uint64_t    u32Base  = X86DESC64_BASE(pDesc);
    Log(("%s %04x - %RX64 %RX64 - base=%RX64 limit=%08x dpl=%d %s\n", pszMsg,
         Sel, pDesc->au64[0], pDesc->au64[1], u32Base, u32Limit, pDesc->Gen.u2Dpl, szMsg));
# else
    uint32_t    u32Base  = X86DESC_BASE(pDesc);
    Log(("%s %04x - %08x %08x - base=%08x limit=%08x dpl=%d %s\n", pszMsg,
         Sel, pDesc->au32[0], pDesc->au32[1], u32Base, u32Limit, pDesc->Gen.u2Dpl, szMsg));
# endif
}


/**
 * Formats a full register dump.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the CPU context.
 */
VMMR0DECL(void) HMDumpRegs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pVM);

    /*
     * Format the flags.
     */
    static struct
    {
        const char *pszSet; const char *pszClear; uint32_t fFlag;
    } const s_aFlags[] =
    {
        { "vip", NULL, X86_EFL_VIP },
        { "vif", NULL, X86_EFL_VIF },
        { "ac",  NULL, X86_EFL_AC },
        { "vm",  NULL, X86_EFL_VM },
        { "rf",  NULL, X86_EFL_RF },
        { "nt",  NULL, X86_EFL_NT },
        { "ov",  "nv", X86_EFL_OF },
        { "dn",  "up", X86_EFL_DF },
        { "ei",  "di", X86_EFL_IF },
        { "tf",  NULL, X86_EFL_TF },
        { "nt",  "pl", X86_EFL_SF },
        { "nz",  "zr", X86_EFL_ZF },
        { "ac",  "na", X86_EFL_AF },
        { "po",  "pe", X86_EFL_PF },
        { "cy",  "nc", X86_EFL_CF },
    };
    char szEFlags[80];
    char *psz = szEFlags;
    uint32_t uEFlags = pCtx->eflags.u32;
    for (unsigned i = 0; i < RT_ELEMENTS(s_aFlags); i++)
    {
        const char *pszAdd = s_aFlags[i].fFlag & uEFlags ? s_aFlags[i].pszSet : s_aFlags[i].pszClear;
        if (pszAdd)
        {
            strcpy(psz, pszAdd);
            psz += strlen(pszAdd);
            *psz++ = ' ';
        }
    }
    psz[-1] = '\0';


    /*
     * Format the registers.
     */
    if (CPUMIsGuestIn64BitCode(pVCpu))
    {
        Log(("rax=%016RX64 rbx=%016RX64 rcx=%016RX64 rdx=%016RX64\n"
             "rsi=%016RX64 rdi=%016RX64 r8 =%016RX64 r9 =%016RX64\n"
             "r10=%016RX64 r11=%016RX64 r12=%016RX64 r13=%016RX64\n"
             "r14=%016RX64 r15=%016RX64\n"
             "rip=%016RX64 rsp=%016RX64 rbp=%016RX64 iopl=%d %*s\n"
             "cs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
             "ds={%04x base=%016RX64 limit=%08x flags=%08x}\n"
             "es={%04x base=%016RX64 limit=%08x flags=%08x}\n"
             "fs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
             "gs={%04x base=%016RX64 limit=%08x flags=%08x}\n"
             "ss={%04x base=%016RX64 limit=%08x flags=%08x}\n"
             "cr0=%016RX64 cr2=%016RX64 cr3=%016RX64 cr4=%016RX64\n"
             "dr0=%016RX64 dr1=%016RX64 dr2=%016RX64 dr3=%016RX64\n"
             "dr4=%016RX64 dr5=%016RX64 dr6=%016RX64 dr7=%016RX64\n"
             "gdtr=%016RX64:%04x  idtr=%016RX64:%04x  eflags=%08x\n"
             "ldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
             "tr  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
             "SysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
             ,
             pCtx->rax, pCtx->rbx, pCtx->rcx, pCtx->rdx, pCtx->rsi, pCtx->rdi,
             pCtx->r8, pCtx->r9, pCtx->r10, pCtx->r11, pCtx->r12, pCtx->r13,
             pCtx->r14, pCtx->r15,
             pCtx->rip, pCtx->rsp, pCtx->rbp, X86_EFL_GET_IOPL(uEFlags), 31, szEFlags,
             pCtx->cs.Sel, pCtx->cs.u64Base, pCtx->cs.u32Limit, pCtx->cs.Attr.u,
             pCtx->ds.Sel, pCtx->ds.u64Base, pCtx->ds.u32Limit, pCtx->ds.Attr.u,
             pCtx->es.Sel, pCtx->es.u64Base, pCtx->es.u32Limit, pCtx->es.Attr.u,
             pCtx->fs.Sel, pCtx->fs.u64Base, pCtx->fs.u32Limit, pCtx->fs.Attr.u,
             pCtx->gs.Sel, pCtx->gs.u64Base, pCtx->gs.u32Limit, pCtx->gs.Attr.u,
             pCtx->ss.Sel, pCtx->ss.u64Base, pCtx->ss.u32Limit, pCtx->ss.Attr.u,
             pCtx->cr0,  pCtx->cr2, pCtx->cr3,  pCtx->cr4,
             pCtx->dr[0],  pCtx->dr[1], pCtx->dr[2],  pCtx->dr[3],
             pCtx->dr[4],  pCtx->dr[5], pCtx->dr[6],  pCtx->dr[7],
             pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, uEFlags,
             pCtx->ldtr.Sel, pCtx->ldtr.u64Base, pCtx->ldtr.u32Limit, pCtx->ldtr.Attr.u,
             pCtx->tr.Sel, pCtx->tr.u64Base, pCtx->tr.u32Limit, pCtx->tr.Attr.u,
             pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp));
    }
    else
        Log(("eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
             "eip=%08x esp=%08x ebp=%08x iopl=%d %*s\n"
             "cs={%04x base=%016RX64 limit=%08x flags=%08x} dr0=%08RX64 dr1=%08RX64\n"
             "ds={%04x base=%016RX64 limit=%08x flags=%08x} dr2=%08RX64 dr3=%08RX64\n"
             "es={%04x base=%016RX64 limit=%08x flags=%08x} dr4=%08RX64 dr5=%08RX64\n"
             "fs={%04x base=%016RX64 limit=%08x flags=%08x} dr6=%08RX64 dr7=%08RX64\n"
             "gs={%04x base=%016RX64 limit=%08x flags=%08x} cr0=%08RX64 cr2=%08RX64\n"
             "ss={%04x base=%016RX64 limit=%08x flags=%08x} cr3=%08RX64 cr4=%08RX64\n"
             "gdtr=%016RX64:%04x  idtr=%016RX64:%04x  eflags=%08x\n"
             "ldtr={%04x base=%08RX64 limit=%08x flags=%08x}\n"
             "tr  ={%04x base=%08RX64 limit=%08x flags=%08x}\n"
             "SysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
             ,
             pCtx->eax, pCtx->ebx, pCtx->ecx, pCtx->edx, pCtx->esi, pCtx->edi,
             pCtx->eip, pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(uEFlags), 31, szEFlags,
             pCtx->cs.Sel, pCtx->cs.u64Base, pCtx->cs.u32Limit, pCtx->cs.Attr.u, pCtx->dr[0],  pCtx->dr[1],
             pCtx->ds.Sel, pCtx->ds.u64Base, pCtx->ds.u32Limit, pCtx->ds.Attr.u, pCtx->dr[2],  pCtx->dr[3],
             pCtx->es.Sel, pCtx->es.u64Base, pCtx->es.u32Limit, pCtx->es.Attr.u, pCtx->dr[4],  pCtx->dr[5],
             pCtx->fs.Sel, pCtx->fs.u64Base, pCtx->fs.u32Limit, pCtx->fs.Attr.u, pCtx->dr[6],  pCtx->dr[7],
             pCtx->gs.Sel, pCtx->gs.u64Base, pCtx->gs.u32Limit, pCtx->gs.Attr.u, pCtx->cr0,  pCtx->cr2,
             pCtx->ss.Sel, pCtx->ss.u64Base, pCtx->ss.u32Limit, pCtx->ss.Attr.u, pCtx->cr3,  pCtx->cr4,
             pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, uEFlags,
             pCtx->ldtr.Sel, pCtx->ldtr.u64Base, pCtx->ldtr.u32Limit, pCtx->ldtr.Attr.u,
             pCtx->tr.Sel, pCtx->tr.u64Base, pCtx->tr.u32Limit, pCtx->tr.Attr.u,
             pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp));

    Log(("FPU:\n"
        "FCW=%04x FSW=%04x FTW=%02x\n"
        "FOP=%04x FPUIP=%08x CS=%04x Rsrvd1=%04x\n"
        "FPUDP=%04x DS=%04x Rsvrd2=%04x MXCSR=%08x MXCSR_MASK=%08x\n"
        ,
        pCtx->XState.x87.FCW, pCtx->XState.x87.FSW, pCtx->XState.x87.FTW,
        pCtx->XState.x87.FOP, pCtx->XState.x87.FPUIP, pCtx->XState.x87.CS, pCtx->XState.x87.Rsrvd1,
        pCtx->XState.x87.FPUDP, pCtx->XState.x87.DS, pCtx->XState.x87.Rsrvd2,
        pCtx->XState.x87.MXCSR, pCtx->XState.x87.MXCSR_MASK));

    Log(("MSR:\n"
        "EFER         =%016RX64\n"
        "PAT          =%016RX64\n"
        "STAR         =%016RX64\n"
        "CSTAR        =%016RX64\n"
        "LSTAR        =%016RX64\n"
        "SFMASK       =%016RX64\n"
        "KERNELGSBASE =%016RX64\n",
        pCtx->msrEFER,
        pCtx->msrPAT,
        pCtx->msrSTAR,
        pCtx->msrCSTAR,
        pCtx->msrLSTAR,
        pCtx->msrSFMASK,
        pCtx->msrKERNELGSBASE));
}

#endif /* VBOX_STRICT */

