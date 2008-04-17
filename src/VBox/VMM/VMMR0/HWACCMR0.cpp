/* $Id$ */
/** @file
 * HWACCM - Host Context Ring 0.
 */

/*
 * Copyright (C) 2006-2007 innotek GmbH
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
#define LOG_GROUP LOG_GROUP_HWACCM
#include <VBox/hwaccm.h>
#include "HWACCMInternal.h"
#include <VBox/vm.h>
#include <VBox/x86.h>
#include <VBox/hwacc_vmx.h>
#include <VBox/hwacc_svm.h>
#include <VBox/pgm.h>
#include <VBox/pdm.h>
#include <VBox/err.h>
#include <VBox/log.h>
#include <VBox/selm.h>
#include <VBox/iom.h>
#include <iprt/param.h>
#include <iprt/assert.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/memobj.h>
#include <iprt/cpuset.h>
#include "HWVMXR0.h"
#include "HWSVMR0.h"

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void) HWACCMR0EnableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) HWACCMR0DisableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) HWACCMR0InitCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static              int   hwaccmr0CheckCpuRcArray(int *paRc, unsigned cErrorCodes, RTCPUID *pidCpu);

/*******************************************************************************
*   Local Variables                                                            *
*******************************************************************************/
static struct
{
    struct
    {
        RTR0MEMOBJ  pMemObj;
        bool        fVMXConfigured;
        bool        fSVMConfigured;
    } aCpuInfo[RTCPUSET_MAX_CPUS];

    struct
    {
        /** Set by the ring-0 driver to indicate VMX is supported by the CPU. */
        bool                        fSupported;

        /** Host CR4 value (set by ring-0 VMX init) */
        uint64_t                    hostCR4;

        /** VMX MSR values */
        struct
        {
            uint64_t                feature_ctrl;
            uint64_t                vmx_basic_info;
            uint64_t                vmx_pin_ctls;
            uint64_t                vmx_proc_ctls;
            uint64_t                vmx_exit;
            uint64_t                vmx_entry;
            uint64_t                vmx_misc;
            uint64_t                vmx_cr0_fixed0;
            uint64_t                vmx_cr0_fixed1;
            uint64_t                vmx_cr4_fixed0;
            uint64_t                vmx_cr4_fixed1;
            uint64_t                vmx_vmcs_enum;
        } msr;
        /* Last instruction error */
        uint32_t                    ulLastInstrError;
    } vmx;
    struct
    {
        /** Set by the ring-0 driver to indicate SVM is supported by the CPU. */
        bool                        fSupported;

        /** SVM revision. */
        uint32_t                    u32Rev;

        /** Maximum ASID allowed. */
        uint32_t                    u32MaxASID;
    } svm;
    /** Saved error from detection */
    int32_t         lLastError;

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    HWACCMSTATE     enmHwAccmState;
} HWACCMR0Globals;



/**
 * Does global Ring-0 HWACCM initialization.
 *
 * @returns VBox status code.
 */
HWACCMR0DECL(int) HWACCMR0Init()
{
    int        rc;

    memset(&HWACCMR0Globals, 0, sizeof(HWACCMR0Globals));
    HWACCMR0Globals.enmHwAccmState = HWACCMSTATE_UNINITIALIZED;

#ifndef VBOX_WITH_HYBIRD_32BIT_KERNEL /* paranoia */

    /*
     * Check for VT-x and AMD-V capabilities
     */
    if (ASMHasCpuId())
    {
        uint32_t u32FeaturesECX;
        uint32_t u32Dummy;
        uint32_t u32FeaturesEDX;
        uint32_t u32VendorEBX, u32VendorECX, u32VendorEDX;

        ASMCpuId(0, &u32Dummy, &u32VendorEBX, &u32VendorECX, &u32VendorEDX);
        ASMCpuId(1, &u32Dummy, &u32Dummy, &u32FeaturesECX, &u32FeaturesEDX);
        /* Query AMD features. */
        ASMCpuId(0x80000001, &u32Dummy, &u32Dummy, &HWACCMR0Globals.cpuid.u32AMDFeatureECX, &HWACCMR0Globals.cpuid.u32AMDFeatureEDX);

        if (    u32VendorEBX == X86_CPUID_VENDOR_INTEL_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_INTEL_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_INTEL_EDX
           )
        {
            /*
             * Read all VMX MSRs if VMX is available. (same goes for RDMSR/WRMSR)
             * We also assume all VMX-enabled CPUs support fxsave/fxrstor.
             */
            if (    (u32FeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                int     aRc[RTCPUSET_MAX_CPUS];
                RTCPUID idCpu = 0;

                HWACCMR0Globals.vmx.msr.feature_ctrl = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);

                /* We need to check if VT-x has been properly initialized on all CPUs. Some BIOSes do a lousy job. */
                memset(aRc, 0, sizeof(aRc));
                HWACCMR0Globals.lLastError = RTMpOnAll(HWACCMR0InitCPU, (void *)u32VendorEBX, aRc);

                /* Check the return code of all invocations. */
                if (VBOX_SUCCESS(HWACCMR0Globals.lLastError))
                    HWACCMR0Globals.lLastError = hwaccmr0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);

                if (VBOX_SUCCESS(HWACCMR0Globals.lLastError))
                {
                    /* Reread in case we've changed it. */
                    HWACCMR0Globals.vmx.msr.feature_ctrl = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);

                    if (   (HWACCMR0Globals.vmx.msr.feature_ctrl & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                                                                == (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                    {
                        HWACCMR0Globals.vmx.fSupported          = true;
                        HWACCMR0Globals.vmx.msr.vmx_basic_info  = ASMRdMsr(MSR_IA32_VMX_BASIC_INFO);
                        HWACCMR0Globals.vmx.msr.vmx_pin_ctls    = ASMRdMsr(MSR_IA32_VMX_PINBASED_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_proc_ctls   = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_exit        = ASMRdMsr(MSR_IA32_VMX_EXIT_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_entry       = ASMRdMsr(MSR_IA32_VMX_ENTRY_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_misc        = ASMRdMsr(MSR_IA32_VMX_MISC);
                        HWACCMR0Globals.vmx.msr.vmx_cr0_fixed0  = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED0);
                        HWACCMR0Globals.vmx.msr.vmx_cr0_fixed1  = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED1);
                        HWACCMR0Globals.vmx.msr.vmx_cr4_fixed0  = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED0);
                        HWACCMR0Globals.vmx.msr.vmx_cr4_fixed1  = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED1);
                        HWACCMR0Globals.vmx.msr.vmx_vmcs_enum   = ASMRdMsr(MSR_IA32_VMX_VMCS_ENUM);
                        HWACCMR0Globals.vmx.hostCR4             = ASMGetCR4();

#if HC_ARCH_BITS == 64
                        RTR0MEMOBJ pScatchMemObj;
                        void      *pvScatchPage;
                        RTHCPHYS   pScatchPagePhys;

                        rc = RTR0MemObjAllocCont(&pScatchMemObj, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
                        if (RT_FAILURE(rc))
                            return rc;

                        pvScatchPage    = RTR0MemObjAddress(pScatchMemObj);
                        pScatchPagePhys = RTR0MemObjGetPagePhysAddr(pScatchMemObj, 0);
                        memset(pvScatchPage, 0, PAGE_SIZE);

                        /* Set revision dword at the beginning of the structure. */
                        *(uint32_t *)pvScatchPage = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(HWACCMR0Globals.vmx.msr.vmx_basic_info);

                        /* Make sure we don't get rescheduled to another cpu during this probe. */
                        RTCCUINTREG fFlags = ASMIntDisableFlags();

                        /*
                         * Check CR4.VMXE
                         */
                        if (!(HWACCMR0Globals.vmx.hostCR4 & X86_CR4_VMXE))
                        {
                            /* In theory this bit could be cleared behind our back. Which would cause #UD faults when we
                             * try to execute the VMX instructions...
                             */
                            ASMSetCR4(HWACCMR0Globals.vmx.hostCR4 | X86_CR4_VMXE);
                        }

                        /* Enter VMX Root Mode */
                        rc = VMXEnable(pScatchPagePhys);
                        if (VBOX_FAILURE(rc))
                        {
                            /* KVM leaves the CPU in VMX root mode. Not only is this not allowed, it will crash the host when we enter raw mode, because
                             * (a) clearing X86_CR4_VMXE in CR4 causes a #GP    (we no longer modify this bit)
                             * (b) turning off paging causes a #GP              (unavoidable when switching from long to 32 bits mode)
                             *
                             * They should fix their code, but until they do we simply refuse to run.
                             */
                            HWACCMR0Globals.lLastError = VERR_VMX_IN_VMX_ROOT_MODE;
                            HWACCMR0Globals.vmx.fSupported = false;
                        }
                        else
                            VMXDisable();

                        /* Restore CR4 again; don't leave the X86_CR4_VMXE flag set if it wasn't so before (some software could incorrectly think it's in VMX mode) */
                        ASMSetCR4(HWACCMR0Globals.vmx.hostCR4);
                        ASMSetFlags(fFlags);

                        RTR0MemObjFree(pScatchMemObj, false);
#endif
                    }
                    else
                    {
                        AssertFailed(); /* can't hit this case anymore */
                        HWACCMR0Globals.lLastError = VERR_VMX_ILLEGAL_FEATURE_CONTROL_MSR;
                    }
                }
#ifdef LOG_ENABLED
                else
                    SUPR0Printf("HWACCMR0InitCPU failed with rc=%d\n", HWACCMR0Globals.lLastError);
#endif
            }
            else
                HWACCMR0Globals.lLastError = VERR_VMX_NO_VMX;
        }
        else
        if (    u32VendorEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_AMD_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_AMD_EDX
           )
        {
            /*
             * Read all SVM MSRs if SVM is available. (same goes for RDMSR/WRMSR)
             * We also assume all SVM-enabled CPUs support fxsave/fxrstor.
             */
            if (   (HWACCMR0Globals.cpuid.u32AMDFeatureECX & X86_CPUID_AMD_FEATURE_ECX_SVM)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                int     aRc[RTCPUSET_MAX_CPUS];
                RTCPUID idCpu = 0;

                /* We need to check if AMD-V has been properly initialized on all CPUs. Some BIOSes might do a poor job. */
                memset(aRc, 0, sizeof(aRc));
                rc = RTMpOnAll(HWACCMR0InitCPU, (void *)u32VendorEBX, aRc);
                AssertRC(rc);

                /* Check the return code of all invocations. */
                if (VBOX_SUCCESS(rc))
                    rc = hwaccmr0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);

                AssertMsg(VBOX_SUCCESS(rc), ("HWACCMR0InitCPU failed for cpu %d with rc=%d\n", idCpu, rc));

                if (VBOX_SUCCESS(rc))
                {
                    /* Query AMD features. */
                    ASMCpuId(0x8000000A, &HWACCMR0Globals.svm.u32Rev, &HWACCMR0Globals.svm.u32MaxASID, &u32Dummy, &u32Dummy);

                    HWACCMR0Globals.svm.fSupported = true;
                }
                else
                    HWACCMR0Globals.lLastError = rc;
            }
            else
                HWACCMR0Globals.lLastError = VERR_SVM_NO_SVM;
        }
        else
            HWACCMR0Globals.lLastError = VERR_HWACCM_UNKNOWN_CPU;
    }
    else
        HWACCMR0Globals.lLastError = VERR_HWACCM_NO_CPUID;

#endif /* !VBOX_WITH_HYBIRD_32BIT_KERNEL */

    return VINF_SUCCESS;
}


/**
 * Checks the error code array filled in for each cpu in the system.
 *
 * @returns VBox status code.
 * @param   paRc        Error code array
 * @param   cErrorCodes Array size
 * @param   pidCpu      Value of the first cpu that set an error (out)
 */
static int hwaccmr0CheckCpuRcArray(int *paRc, unsigned cErrorCodes, RTCPUID *pidCpu)
{
    int rc = VINF_SUCCESS;

    Assert(cErrorCodes == RTCPUSET_MAX_CPUS);

    for (unsigned i=0;i<cErrorCodes;i++)
    {
        if (RTMpIsCpuOnline(i))
        {
            if (VBOX_FAILURE(paRc[i]))
            {
                rc      = paRc[i];
                *pidCpu = i;
                break;
            }
        }
    }
    return rc;
}

/**
 * Does global Ring-0 HWACCM termination.
 *
 * @returns VBox status code.
 */
HWACCMR0DECL(int) HWACCMR0Term()
{
    int aRc[RTCPUSET_MAX_CPUS];

    memset(aRc, 0, sizeof(aRc));
    int rc = RTMpOnAll(HWACCMR0DisableCPU, aRc, NULL);
    Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);

    /* Free the per-cpu pages used for VT-x and AMD-V */
    for (unsigned i=0;i<RT_ELEMENTS(HWACCMR0Globals.aCpuInfo);i++)
    {
        AssertMsg(VBOX_SUCCESS(aRc[i]), ("HWACCMR0DisableCPU failed for cpu %d with rc=%d\n", i, aRc[i]));
        if (HWACCMR0Globals.aCpuInfo[i].pMemObj)
        {
            RTR0MemObjFree(HWACCMR0Globals.aCpuInfo[i].pMemObj, false);
            HWACCMR0Globals.aCpuInfo[i].pMemObj = NULL;
        }
    }
    return rc;
}


/**
 * Worker function passed to RTMpOnAll, RTMpOnOthers and RTMpOnSpecific that
 * is to be called on the target cpus.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     The 1st user argument.
 * @param   pvUser2     The 2nd user argument.
 */
static DECLCALLBACK(void) HWACCMR0InitCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    unsigned u32VendorEBX = (uintptr_t)pvUser1;
    int     *paRc         = (int *)pvUser2;
    uint64_t val;

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0InitCPU cpu %d\n", idCpu);
#endif
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /// @todo fix idCpu == index assumption (rainy day)

    if (u32VendorEBX == X86_CPUID_VENDOR_INTEL_EBX)
    {
        val = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);

        /*
        * Both the LOCK and VMXON bit must be set; otherwise VMXON will generate a #GP.
        * Once the lock bit is set, this MSR can no longer be modified.
        */
        if (!(val & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK)))
        {
            /* MSR is not yet locked; we can change it ourselves here */
            ASMWrMsr(MSR_IA32_FEATURE_CONTROL, HWACCMR0Globals.vmx.msr.feature_ctrl | MSR_IA32_FEATURE_CONTROL_VMXON | MSR_IA32_FEATURE_CONTROL_LOCK);
            val = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
        }
        if (   (val & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                   == (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
            paRc[idCpu] = VINF_SUCCESS;
        else
            paRc[idCpu] = VERR_VMX_MSR_LOCKED_OR_DISABLED;
    }
    else
    if (u32VendorEBX == X86_CPUID_VENDOR_AMD_EBX)
    {
        /* Check if SVM is disabled */
        val = ASMRdMsr(MSR_K8_VM_CR);
        if (!(val & MSR_K8_VM_CR_SVM_DISABLE))
        {
            /* Turn on SVM in the EFER MSR. */
            val = ASMRdMsr(MSR_K6_EFER);
            if (!(val & MSR_K6_EFER_SVME))
                ASMWrMsr(MSR_K6_EFER, val | MSR_K6_EFER_SVME);

            /* Paranoia. */
            val = ASMRdMsr(MSR_K6_EFER);
            if (val & MSR_K6_EFER_SVME)
                paRc[idCpu] = VINF_SUCCESS;
            else
                paRc[idCpu] = VERR_SVM_ILLEGAL_EFER_MSR;
        }
        else
            paRc[idCpu] = HWACCMR0Globals.lLastError = VERR_SVM_DISABLED;
    }
    else
        AssertFailed(); /* can't happen */
    return;
}


/**
 * Sets up HWACCM on all cpus.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM to operate on.
 * @param   enmNewHwAccmState   New hwaccm state
 *
 */
HWACCMR0DECL(int) HWACCMR0EnableAllCpus(PVM pVM, HWACCMSTATE enmNewHwAccmState)
{
    Assert(sizeof(HWACCMR0Globals.enmHwAccmState) == sizeof(uint32_t));
    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&HWACCMR0Globals.enmHwAccmState, enmNewHwAccmState, HWACCMSTATE_UNINITIALIZED))
    {
        int     aRc[RTCPUSET_MAX_CPUS];
        RTCPUID idCpu = 0;

        /* Don't setup hwaccm as that might not work (vt-x & 64 bits raw mode) */
        if (enmNewHwAccmState == HWACCMSTATE_DISABLED)
            return VINF_SUCCESS;

        memset(aRc, 0, sizeof(aRc));

        /* Allocate one page per cpu for the global vt-x and amd-v pages */
        for (unsigned i=0;i<RT_ELEMENTS(HWACCMR0Globals.aCpuInfo);i++)
        {
            Assert(!HWACCMR0Globals.aCpuInfo[i].pMemObj);

            /** @todo this is rather dangerous if cpus can be taken offline; we don't care for now */
            if (RTMpIsCpuOnline(i))
            {
                int rc = RTR0MemObjAllocCont(&HWACCMR0Globals.aCpuInfo[i].pMemObj, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
                AssertRC(rc);
                if (RT_FAILURE(rc))
                    return rc;

                void *pvR0 = RTR0MemObjAddress(HWACCMR0Globals.aCpuInfo[i].pMemObj);
                Assert(pvR0);
                memset(pvR0, 0, PAGE_SIZE);

#ifdef LOG_ENABLED
                SUPR0Printf("address %x phys %x\n", pvR0, (uint32_t)RTR0MemObjGetPagePhysAddr(HWACCMR0Globals.aCpuInfo[i].pMemObj, 0));
#endif
            }
        }
        /* First time, so initialize each cpu/core */
        int rc = RTMpOnAll(HWACCMR0EnableCPU, (void *)pVM, aRc);

        /* Check the return code of all invocations. */
        if (VBOX_SUCCESS(rc))
            rc = hwaccmr0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);

        AssertMsg(VBOX_SUCCESS(rc), ("HWACCMR0EnableAllCpus failed for cpu %d with rc=%d\n", idCpu, rc));
        return rc;
    }

    if (HWACCMR0Globals.enmHwAccmState == enmNewHwAccmState)
        return VINF_SUCCESS;

    /* Request to change the mode is not allowed */
    return VERR_ACCESS_DENIED;
}

/**
 * Worker function passed to RTMpOnAll, RTMpOnOthers and RTMpOnSpecific that
 * is to be called on the target cpus.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     The 1st user argument.
 * @param   pvUser2     The 2nd user argument.
 */
static DECLCALLBACK(void) HWACCMR0EnableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PVM      pVM = (PVM)pvUser1;
    int     *paRc = (int *)pvUser2;
    void    *pvPageCpu;
    RTHCPHYS pPageCpuPhys;

    Assert(pVM);
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /// @todo fix idCpu == index assumption (rainy day)
    Assert(idCpu < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo));

    /* Should never happen */
    if (!HWACCMR0Globals.aCpuInfo[idCpu].pMemObj)
    {
        AssertFailed();
        return;
    }

    pvPageCpu    = RTR0MemObjAddress(HWACCMR0Globals.aCpuInfo[idCpu].pMemObj);
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(HWACCMR0Globals.aCpuInfo[idCpu].pMemObj, 0);

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        paRc[idCpu] = VMXR0EnableCpu(idCpu, pVM, pvPageCpu, pPageCpuPhys);
        AssertRC(paRc[idCpu]);
        if (VBOX_SUCCESS(paRc[idCpu]))
            HWACCMR0Globals.aCpuInfo[idCpu].fVMXConfigured = true;
    }
    else
    if (pVM->hwaccm.s.svm.fSupported)
    {
        paRc[idCpu] = SVMR0EnableCpu(idCpu, pVM, pvPageCpu, pPageCpuPhys);
        AssertRC(paRc[idCpu]);
        if (VBOX_SUCCESS(paRc[idCpu]))
            HWACCMR0Globals.aCpuInfo[idCpu].fSVMConfigured = true;
    }
    return;
}

/**
 * Worker function passed to RTMpOnAll, RTMpOnOthers and RTMpOnSpecific that
 * is to be called on the target cpus.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     The 1st user argument.
 * @param   pvUser2     The 2nd user argument.
 */
static DECLCALLBACK(void) HWACCMR0DisableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    void    *pvPageCpu;
    RTHCPHYS pPageCpuPhys;
    int     *paRc = (int *)pvUser1;

    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /// @todo fix idCpu == index assumption (rainy day)
    Assert(idCpu < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo));

    if (!HWACCMR0Globals.aCpuInfo[idCpu].pMemObj)
        return;

    pvPageCpu    = RTR0MemObjAddress(HWACCMR0Globals.aCpuInfo[idCpu].pMemObj);
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(HWACCMR0Globals.aCpuInfo[idCpu].pMemObj, 0);

    if (HWACCMR0Globals.aCpuInfo[idCpu].fVMXConfigured)
    {
        paRc[idCpu] = VMXR0DisableCpu(idCpu, pvPageCpu, pPageCpuPhys);
        AssertRC(paRc[idCpu]);
        HWACCMR0Globals.aCpuInfo[idCpu].fVMXConfigured = false;
    }
    else
    if (HWACCMR0Globals.aCpuInfo[idCpu].fSVMConfigured)
    {
        paRc[idCpu] = SVMR0DisableCpu(idCpu, pvPageCpu, pPageCpuPhys);
        AssertRC(paRc[idCpu]);
        HWACCMR0Globals.aCpuInfo[idCpu].fSVMConfigured = false;
    }
    return;
}


/**
 * Does Ring-0 per VM HWACCM initialization.
 *
 * This is mainly to check that the Host CPU mode is compatible
 * with VMX.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0InitVM(PVM pVM)
{
    int rc = VINF_SUCCESS;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0InitVM: %p\n", pVM);
#endif

    pVM->hwaccm.s.vmx.fSupported            = HWACCMR0Globals.vmx.fSupported;
    pVM->hwaccm.s.svm.fSupported            = HWACCMR0Globals.svm.fSupported;

    pVM->hwaccm.s.vmx.msr.feature_ctrl      = HWACCMR0Globals.vmx.msr.feature_ctrl;
    pVM->hwaccm.s.vmx.hostCR4               = HWACCMR0Globals.vmx.hostCR4;
    pVM->hwaccm.s.vmx.msr.vmx_basic_info    = HWACCMR0Globals.vmx.msr.vmx_basic_info;
    pVM->hwaccm.s.vmx.msr.vmx_pin_ctls      = HWACCMR0Globals.vmx.msr.vmx_pin_ctls;
    pVM->hwaccm.s.vmx.msr.vmx_proc_ctls     = HWACCMR0Globals.vmx.msr.vmx_proc_ctls;
    pVM->hwaccm.s.vmx.msr.vmx_exit          = HWACCMR0Globals.vmx.msr.vmx_exit;
    pVM->hwaccm.s.vmx.msr.vmx_entry         = HWACCMR0Globals.vmx.msr.vmx_entry;
    pVM->hwaccm.s.vmx.msr.vmx_misc          = HWACCMR0Globals.vmx.msr.vmx_misc;
    pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0    = HWACCMR0Globals.vmx.msr.vmx_cr0_fixed0;
    pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1    = HWACCMR0Globals.vmx.msr.vmx_cr0_fixed1;
    pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0    = HWACCMR0Globals.vmx.msr.vmx_cr4_fixed0;
    pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1    = HWACCMR0Globals.vmx.msr.vmx_cr4_fixed1;
    pVM->hwaccm.s.vmx.msr.vmx_vmcs_enum     = HWACCMR0Globals.vmx.msr.vmx_vmcs_enum;
    pVM->hwaccm.s.svm.u32Rev                = HWACCMR0Globals.svm.u32Rev;
    pVM->hwaccm.s.svm.u32MaxASID            = HWACCMR0Globals.svm.u32MaxASID;
    pVM->hwaccm.s.cpuid.u32AMDFeatureECX    = HWACCMR0Globals.cpuid.u32AMDFeatureECX;
    pVM->hwaccm.s.cpuid.u32AMDFeatureEDX    = HWACCMR0Globals.cpuid.u32AMDFeatureEDX;
    pVM->hwaccm.s.lLastError                = HWACCMR0Globals.lLastError;

    /* Init a VT-x or AMD-V VM. */
    if (pVM->hwaccm.s.vmx.fSupported)
        rc = VMXR0InitVM(pVM);
    else
    if (pVM->hwaccm.s.svm.fSupported)
        rc = SVMR0InitVM(pVM);

    return rc;
}


/**
 * Does Ring-0 per VM HWACCM termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0TermVM(PVM pVM)
{
    int rc = VINF_SUCCESS;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0TermVM: %p\n", pVM);
#endif

    /* Terminate a VT-x or AMD-V VM. */
    if (pVM->hwaccm.s.vmx.fSupported)
        rc = VMXR0TermVM(pVM);
    else
    if (pVM->hwaccm.s.svm.fSupported)
        rc = SVMR0TermVM(pVM);

    return rc;
}


/**
 * Sets up a VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0SetupVM(PVM pVM)
{
    int rc = VINF_SUCCESS;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0SetupVM: %p\n", pVM);
#endif

    /* Setup VT-x or AMD-V. */
    if (pVM->hwaccm.s.vmx.fSupported)
        rc = VMXR0SetupVM(pVM);
    else
    if (pVM->hwaccm.s.svm.fSupported)
        rc = SVMR0SetupVM(pVM);

    return rc;
}


/**
 * Enters the VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Enter(PVM pVM)
{
    CPUMCTX *pCtx;
    int      rc;

    rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
    if (VBOX_FAILURE(rc))
        return rc;

    /* Always load the guest's FPU/XMM state on-demand. */
    CPUMDeactivateGuestFPUState(pVM);

    /* Always reload the host context and the guest's CR0 register. (!!!!) */
    pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0 | HWACCM_CHANGED_HOST_CONTEXT;

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        rc  = VMXR0Enter(pVM);
        AssertRC(rc);
        rc |= VMXR0SaveHostState(pVM);
        AssertRC(rc);
        rc |= VMXR0LoadGuestState(pVM, pCtx);
        AssertRC(rc);
        if (rc != VINF_SUCCESS)
            return rc;
    }
    else
    {
        Assert(pVM->hwaccm.s.svm.fSupported);
        rc  = SVMR0Enter(pVM);
        AssertRC(rc);
        rc |= SVMR0LoadGuestState(pVM, pCtx);
        AssertRC(rc);
        if (rc != VINF_SUCCESS)
            return rc;

    }
    return VINF_SUCCESS;
}


/**
 * Leaves the VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Leave(PVM pVM)
{
    CPUMCTX *pCtx;
    int      rc;

    rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
    if (VBOX_FAILURE(rc))
        return rc;

    /** @note It's rather tricky with longjmps done by e.g. Log statements or the page fault handler. */
    /*        We must restore the host FPU here to make absolutely sure we don't leave the guest FPU state active
     *        or trash somebody else's FPU state.
     */

    /* Restore host FPU and XMM state if necessary. */
    if (CPUMIsGuestFPUStateActive(pVM))
    {
        Log2(("CPUMRestoreHostFPUState\n"));
        /** @note CPUMRestoreHostFPUState keeps the current CR0 intact. */
        CPUMRestoreHostFPUState(pVM);

        pVM->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
    }

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        return VMXR0Leave(pVM);
    }
    else
    {
        Assert(pVM->hwaccm.s.svm.fSupported);
        return SVMR0Leave(pVM);
    }
}

/**
 * Runs guest code in a hardware accelerated VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0RunGuestCode(PVM pVM)
{
    CPUMCTX *pCtx;
    int      rc;

    rc = CPUMQueryGuestCtxPtr(pVM, &pCtx);
    if (VBOX_FAILURE(rc))
        return rc;

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        return VMXR0RunGuestCode(pVM, pCtx);
    }
    else
    {
        Assert(pVM->hwaccm.s.svm.fSupported);
        return SVMR0RunGuestCode(pVM, pCtx);
    }
}


#ifdef VBOX_STRICT
#include <iprt/string.h>
/**
 * Dumps a descriptor.
 *
 * @param   Desc    Descriptor to dump.
 * @param   Sel     Selector number.
 * @param   pszMsg  Message to prepend the log entry with.
 */
HWACCMR0DECL(void) HWACCMR0DumpDescriptor(PX86DESCHC Desc, RTSEL Sel, const char *pszMsg)
{
    /*
     * Make variable description string.
     */
    static struct
    {
        unsigned    cch;
        const char *psz;
    } const aTypes[32] =
    {
        #define STRENTRY(str) { sizeof(str) - 1, str }

        /* system */
#if HC_ARCH_BITS == 64
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
#else
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
#endif
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
        #undef SYSENTRY
    };
    #define ADD_STR(psz, pszAdd) do { strcpy(psz, pszAdd); psz += strlen(pszAdd); } while (0)
    char        szMsg[128];
    char       *psz = &szMsg[0];
    unsigned    i = Desc->Gen.u1DescType << 4 | Desc->Gen.u4Type;
    memcpy(psz, aTypes[i].psz, aTypes[i].cch);
    psz += aTypes[i].cch;

    if (Desc->Gen.u1Present)
        ADD_STR(psz, "Present ");
    else
        ADD_STR(psz, "Not-Present ");
#if HC_ARCH_BITS == 64
    if (Desc->Gen.u1Long)
        ADD_STR(psz, "64-bit ");
    else
        ADD_STR(psz, "Comp   ");
#else
    if (Desc->Gen.u1Granularity)
        ADD_STR(psz, "Page ");
    if (Desc->Gen.u1DefBig)
        ADD_STR(psz, "32-bit ");
    else
        ADD_STR(psz, "16-bit ");
#endif
    #undef ADD_STR
    *psz = '\0';

    /*
     * Limit and Base and format the output.
     */
    uint32_t    u32Limit = Desc->Gen.u4LimitHigh << 16 | Desc->Gen.u16LimitLow;
    if (Desc->Gen.u1Granularity)
        u32Limit = u32Limit << PAGE_SHIFT | PAGE_OFFSET_MASK;

#if HC_ARCH_BITS == 64
    uint64_t    u32Base =  ((uintptr_t)Desc->Gen.u32BaseHigh3 << 32ULL) | Desc->Gen.u8BaseHigh2 << 24ULL | Desc->Gen.u8BaseHigh1 << 16ULL | Desc->Gen.u16BaseLow;

    Log(("%s %04x - %VX64 %VX64 - base=%VX64 limit=%08x dpl=%d %s\n", pszMsg,
         Sel, Desc->au64[0], Desc->au64[1], u32Base, u32Limit, Desc->Gen.u2Dpl, szMsg));
#else
    uint32_t    u32Base =  Desc->Gen.u8BaseHigh2 << 24 | Desc->Gen.u8BaseHigh1 << 16 | Desc->Gen.u16BaseLow;

    Log(("%s %04x - %08x %08x - base=%08x limit=%08x dpl=%d %s\n", pszMsg,
         Sel, Desc->au32[0], Desc->au32[1], u32Base, u32Limit, Desc->Gen.u2Dpl, szMsg));
#endif
}

/**
 * Formats a full register dump.
 *
 * @param   pCtx        The context to format.
 */
HWACCMR0DECL(void) HWACCMDumpRegs(PCPUMCTX pCtx)
{
    /*
     * Format the flags.
     */
    static struct
    {
        const char *pszSet; const char *pszClear; uint32_t fFlag;
    }   aFlags[] =
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
    char szEFlags[80];
    char *psz = szEFlags;
    uint32_t efl = pCtx->eflags.u32;
    for (unsigned i = 0; i < ELEMENTS(aFlags); i++)
    {
        const char *pszAdd = aFlags[i].fFlag & efl ? aFlags[i].pszSet : aFlags[i].pszClear;
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
    Log(("eax=%08x ebx=%08x ecx=%08x edx=%08x esi=%08x edi=%08x\n"
         "eip=%08x esp=%08x ebp=%08x iopl=%d %*s\n"
         "cs={%04x base=%08x limit=%08x flags=%08x} dr0=%08RX64 dr1=%08RX64\n"
         "ds={%04x base=%08x limit=%08x flags=%08x} dr2=%08RX64 dr3=%08RX64\n"
         "es={%04x base=%08x limit=%08x flags=%08x} dr4=%08RX64 dr5=%08RX64\n"
         "fs={%04x base=%08x limit=%08x flags=%08x} dr6=%08RX64 dr7=%08RX64\n"
         ,
         pCtx->eax, pCtx->ebx, pCtx->ecx, pCtx->edx, pCtx->esi, pCtx->edi,
         pCtx->eip, pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(efl), 31, szEFlags,
         (RTSEL)pCtx->cs, pCtx->csHid.u32Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pCtx->dr0,  pCtx->dr1,
         (RTSEL)pCtx->ds, pCtx->dsHid.u32Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pCtx->dr2,  pCtx->dr3,
         (RTSEL)pCtx->es, pCtx->esHid.u32Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pCtx->dr4,  pCtx->dr5,
         (RTSEL)pCtx->fs, pCtx->fsHid.u32Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pCtx->dr6,  pCtx->dr7));

    Log(("gs={%04x base=%08x limit=%08x flags=%08x} cr0=%08RX64 cr2=%08RX64\n"
         "ss={%04x base=%08x limit=%08x flags=%08x} cr3=%08RX64 cr4=%08RX64\n"
         "gdtr=%08x:%04x  idtr=%08x:%04x  eflags=%08x\n"
         "ldtr={%04x base=%08x limit=%08x flags=%08x}\n"
         "tr  ={%04x base=%08x limit=%08x flags=%08x}\n"
         "SysEnter={cs=%04llx eip=%08llx esp=%08llx}\n"
         "FCW=%04x FSW=%04x FTW=%04x\n",
         (RTSEL)pCtx->gs, pCtx->gsHid.u32Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, pCtx->cr0,  pCtx->cr2,
         (RTSEL)pCtx->ss, pCtx->ssHid.u32Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, pCtx->cr3,  pCtx->cr4,
         pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, efl,
         (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u32Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
         (RTSEL)pCtx->tr, pCtx->trHid.u32Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
         pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp,
         pCtx->fpu.FCW, pCtx->fpu.FSW, pCtx->fpu.FTW));


}
#endif
