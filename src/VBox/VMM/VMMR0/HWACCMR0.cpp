/* $Id$ */
/** @file
 * HWACCM - Host Context Ring 0.
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
#include <iprt/power.h>
#include "HWVMXR0.h"
#include "HWSVMR0.h"

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(void) hwaccmR0EnableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) hwaccmR0DisableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static DECLCALLBACK(void) HWACCMR0InitCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2);
static              int   hwaccmR0CheckCpuRcArray(int *paRc, unsigned cErrorCodes, RTCPUID *pidCpu);
static DECLCALLBACK(void) hwaccmR0PowerCallback(RTPOWEREVENT enmEvent, void *pvUser);

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/

static struct
{
    HWACCM_CPUINFO aCpuInfo[RTCPUSET_MAX_CPUS];

    /** Ring 0 handlers for VT-x and AMD-V. */
    DECLR0CALLBACKMEMBER(int, pfnEnterSession,(PVM pVM, PVMCPU pVCpu, PHWACCM_CPUINFO pCpu));
    DECLR0CALLBACKMEMBER(int, pfnLeaveSession,(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx));
    DECLR0CALLBACKMEMBER(int, pfnSaveHostState,(PVM pVM, PVMCPU pVCpu));
    DECLR0CALLBACKMEMBER(int, pfnLoadGuestState,(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx));
    DECLR0CALLBACKMEMBER(int, pfnRunGuestCode,(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx));
    DECLR0CALLBACKMEMBER(int, pfnEnableCpu, (PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys));
    DECLR0CALLBACKMEMBER(int, pfnDisableCpu, (PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys));
    DECLR0CALLBACKMEMBER(int, pfnInitVM, (PVM pVM));
    DECLR0CALLBACKMEMBER(int, pfnTermVM, (PVM pVM));
    DECLR0CALLBACKMEMBER(int, pfnSetupVM, (PVM pVM));

    /** Maximum ASID allowed. */
    uint32_t                        uMaxASID;

    struct
    {
        /** Set by the ring-0 driver to indicate VMX is supported by the CPU. */
        bool                        fSupported;
        /** Whether we're using SUPR0EnableVTx or not. */
        bool                        fUsingSUPR0EnableVTx;

        /** Host CR4 value (set by ring-0 VMX init) */
        uint64_t                    hostCR4;

        /** VMX MSR values */
        struct
        {
            uint64_t                feature_ctrl;
            uint64_t                vmx_basic_info;
            VMX_CAPABILITY          vmx_pin_ctls;
            VMX_CAPABILITY          vmx_proc_ctls;
            VMX_CAPABILITY          vmx_proc_ctls2;
            VMX_CAPABILITY          vmx_exit;
            VMX_CAPABILITY          vmx_entry;
            uint64_t                vmx_misc;
            uint64_t                vmx_cr0_fixed0;
            uint64_t                vmx_cr0_fixed1;
            uint64_t                vmx_cr4_fixed0;
            uint64_t                vmx_cr4_fixed1;
            uint64_t                vmx_vmcs_enum;
            uint64_t                vmx_eptcaps;
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

        /** SVM feature bits from cpuid 0x8000000a */
        uint32_t                    u32Features;
    } svm;
    /** Saved error from detection */
    int32_t         lLastError;

    struct
    {
        uint32_t                    u32AMDFeatureECX;
        uint32_t                    u32AMDFeatureEDX;
    } cpuid;

    HWACCMSTATE                     enmHwAccmState;

    volatile        bool            fSuspended;
} HWACCMR0Globals;



/**
 * Does global Ring-0 HWACCM initialization.
 *
 * @returns VBox status code.
 */
VMMR0DECL(int) HWACCMR0Init(void)
{
    int        rc;

    memset(&HWACCMR0Globals, 0, sizeof(HWACCMR0Globals));
    HWACCMR0Globals.enmHwAccmState = HWACCMSTATE_UNINITIALIZED;
    for (unsigned i = 0; i < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo); i++)
        HWACCMR0Globals.aCpuInfo[i].pMemObj = NIL_RTR0MEMOBJ;

    /* Fill in all callbacks with placeholders. */
    HWACCMR0Globals.pfnEnterSession     = HWACCMR0DummyEnter;
    HWACCMR0Globals.pfnLeaveSession     = HWACCMR0DummyLeave;
    HWACCMR0Globals.pfnSaveHostState    = HWACCMR0DummySaveHostState;
    HWACCMR0Globals.pfnLoadGuestState   = HWACCMR0DummyLoadGuestState;
    HWACCMR0Globals.pfnRunGuestCode     = HWACCMR0DummyRunGuestCode;
    HWACCMR0Globals.pfnEnableCpu        = HWACCMR0DummyEnableCpu;
    HWACCMR0Globals.pfnDisableCpu       = HWACCMR0DummyDisableCpu;
    HWACCMR0Globals.pfnInitVM           = HWACCMR0DummyInitVM;
    HWACCMR0Globals.pfnTermVM           = HWACCMR0DummyTermVM;
    HWACCMR0Globals.pfnSetupVM          = HWACCMR0DummySetupVM;

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

                /*
                 * First try use native kernel API for controlling VT-x.
                 * (This is only supported by some Mac OS X kernels atm.)
                 */
                HWACCMR0Globals.lLastError = rc = SUPR0EnableVTx(true /* fEnable */);
                if (rc != VERR_NOT_SUPPORTED)
                {
                    AssertMsg(rc == VINF_SUCCESS || rc == VERR_VMX_IN_VMX_ROOT_MODE || rc == VERR_VMX_NO_VMX, ("%Rrc\n", rc));
                    HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx = true;
                    if (RT_SUCCESS(rc))
                    {
                        HWACCMR0Globals.vmx.fSupported = true;
                        rc = SUPR0EnableVTx(false /* fEnable */);
                        AssertRC(rc);
                    }
                }
                else
                {
                    HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx = false;

                    /* We need to check if VT-x has been properly initialized on all CPUs. Some BIOSes do a lousy job. */
                    memset(aRc, 0, sizeof(aRc));
                    HWACCMR0Globals.lLastError = RTMpOnAll(HWACCMR0InitCPU, (void *)u32VendorEBX, aRc);

                    /* Check the return code of all invocations. */
                    if (RT_SUCCESS(HWACCMR0Globals.lLastError))
                        HWACCMR0Globals.lLastError = hwaccmR0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);
                }
                if (RT_SUCCESS(HWACCMR0Globals.lLastError))
                {
                    /* Reread in case we've changed it. */
                    HWACCMR0Globals.vmx.msr.feature_ctrl = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);

                    if (   (HWACCMR0Globals.vmx.msr.feature_ctrl & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                        ==                                         (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                    {
                        RTR0MEMOBJ pScatchMemObj;
                        void      *pvScatchPage;
                        RTHCPHYS   pScatchPagePhys;

                        HWACCMR0Globals.vmx.msr.vmx_basic_info  = ASMRdMsr(MSR_IA32_VMX_BASIC_INFO);
                        HWACCMR0Globals.vmx.msr.vmx_pin_ctls.u  = ASMRdMsr(MSR_IA32_VMX_PINBASED_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_proc_ctls.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_exit.u      = ASMRdMsr(MSR_IA32_VMX_EXIT_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_entry.u     = ASMRdMsr(MSR_IA32_VMX_ENTRY_CTLS);
                        HWACCMR0Globals.vmx.msr.vmx_misc        = ASMRdMsr(MSR_IA32_VMX_MISC);
                        HWACCMR0Globals.vmx.msr.vmx_cr0_fixed0  = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED0);
                        HWACCMR0Globals.vmx.msr.vmx_cr0_fixed1  = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED1);
                        HWACCMR0Globals.vmx.msr.vmx_cr4_fixed0  = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED0);
                        HWACCMR0Globals.vmx.msr.vmx_cr4_fixed1  = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED1);
                        HWACCMR0Globals.vmx.msr.vmx_vmcs_enum   = ASMRdMsr(MSR_IA32_VMX_VMCS_ENUM);
                        /* VPID 16 bits ASID. */
                        HWACCMR0Globals.uMaxASID                = 0x10000; /* exclusive */

                        if (HWACCMR0Globals.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                        {
                            HWACCMR0Globals.vmx.msr.vmx_proc_ctls2.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS2);
                            if (HWACCMR0Globals.vmx.msr.vmx_proc_ctls2.n.allowed1 & (VMX_VMCS_CTRL_PROC_EXEC2_EPT|VMX_VMCS_CTRL_PROC_EXEC2_VPID))
                                HWACCMR0Globals.vmx.msr.vmx_eptcaps = ASMRdMsr(MSR_IA32_VMX_EPT_CAPS);
                        }

                        if (!HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx)
                        {
                            HWACCMR0Globals.vmx.hostCR4             = ASMGetCR4();

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
                            if (RT_FAILURE(rc))
                            {
                                /* KVM leaves the CPU in VMX root mode. Not only is this not allowed, it will crash the host when we enter raw mode, because
                                 * (a) clearing X86_CR4_VMXE in CR4 causes a #GP    (we no longer modify this bit)
                                 * (b) turning off paging causes a #GP              (unavoidable when switching from long to 32 bits mode or 32 bits to PAE)
                                 *
                                 * They should fix their code, but until they do we simply refuse to run.
                                 */
                                HWACCMR0Globals.lLastError = VERR_VMX_IN_VMX_ROOT_MODE;
                            }
                            else
                            {
                                HWACCMR0Globals.vmx.fSupported = true;
                                VMXDisable();
                            }

                            /* Restore CR4 again; don't leave the X86_CR4_VMXE flag set if it wasn't so before (some software could incorrectly think it's in VMX mode) */
                            ASMSetCR4(HWACCMR0Globals.vmx.hostCR4);
                            ASMSetFlags(fFlags);

                            RTR0MemObjFree(pScatchMemObj, false);
                            if (RT_FAILURE(HWACCMR0Globals.lLastError))
                                return HWACCMR0Globals.lLastError;
                        }
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
                if (RT_SUCCESS(rc))
                    rc = hwaccmR0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);

                AssertMsgRC(rc, ("HWACCMR0InitCPU failed for cpu %d with rc=%d\n", idCpu, rc));

                if (RT_SUCCESS(rc))
                {
                    /* Query AMD features. */
                    ASMCpuId(0x8000000A, &HWACCMR0Globals.svm.u32Rev, &HWACCMR0Globals.uMaxASID, &u32Dummy, &HWACCMR0Globals.svm.u32Features);

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

    if (HWACCMR0Globals.vmx.fSupported)
    {
        HWACCMR0Globals.pfnEnterSession     = VMXR0Enter;
        HWACCMR0Globals.pfnLeaveSession     = VMXR0Leave;
        HWACCMR0Globals.pfnSaveHostState    = VMXR0SaveHostState;
        HWACCMR0Globals.pfnLoadGuestState   = VMXR0LoadGuestState;
        HWACCMR0Globals.pfnRunGuestCode     = VMXR0RunGuestCode;
        HWACCMR0Globals.pfnEnableCpu        = VMXR0EnableCpu;
        HWACCMR0Globals.pfnDisableCpu       = VMXR0DisableCpu;
        HWACCMR0Globals.pfnInitVM           = VMXR0InitVM;
        HWACCMR0Globals.pfnTermVM           = VMXR0TermVM;
        HWACCMR0Globals.pfnSetupVM          = VMXR0SetupVM;
    }
    else
    if (HWACCMR0Globals.svm.fSupported)
    {
        HWACCMR0Globals.pfnEnterSession     = SVMR0Enter;
        HWACCMR0Globals.pfnLeaveSession     = SVMR0Leave;
        HWACCMR0Globals.pfnSaveHostState    = SVMR0SaveHostState;
        HWACCMR0Globals.pfnLoadGuestState   = SVMR0LoadGuestState;
        HWACCMR0Globals.pfnRunGuestCode     = SVMR0RunGuestCode;
        HWACCMR0Globals.pfnEnableCpu        = SVMR0EnableCpu;
        HWACCMR0Globals.pfnDisableCpu       = SVMR0DisableCpu;
        HWACCMR0Globals.pfnInitVM           = SVMR0InitVM;
        HWACCMR0Globals.pfnTermVM           = SVMR0TermVM;
        HWACCMR0Globals.pfnSetupVM          = SVMR0SetupVM;
    }

    if (!HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx)
    {
        rc = RTPowerNotificationRegister(hwaccmR0PowerCallback, 0);
        AssertRC(rc);
    }

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
static int hwaccmR0CheckCpuRcArray(int *paRc, unsigned cErrorCodes, RTCPUID *pidCpu)
{
    int rc = VINF_SUCCESS;

    Assert(cErrorCodes == RTCPUSET_MAX_CPUS);

    for (unsigned i=0;i<cErrorCodes;i++)
    {
        if (RTMpIsCpuOnline(i))
        {
            if (RT_FAILURE(paRc[i]))
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
VMMR0DECL(int) HWACCMR0Term(void)
{
    int rc;
    if (   HWACCMR0Globals.vmx.fSupported
        && HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx)
    {
        rc = SUPR0EnableVTx(false /* fEnable */);
        for (unsigned iCpu = 0; iCpu < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo); iCpu++)
        {
            HWACCMR0Globals.aCpuInfo[iCpu].fConfigured = false;
            Assert(HWACCMR0Globals.aCpuInfo[iCpu].pMemObj == NIL_RTR0MEMOBJ);
        }
    }
    else
    {
        int aRc[RTCPUSET_MAX_CPUS];

        Assert(!HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx);
        if (!HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx)
        {
            rc = RTPowerNotificationDeregister(hwaccmR0PowerCallback, 0);
            Assert(RT_SUCCESS(rc));
        }

        memset(aRc, 0, sizeof(aRc));
        rc = RTMpOnAll(hwaccmR0DisableCPU, aRc, NULL);
        Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);

        /* Free the per-cpu pages used for VT-x and AMD-V */
        for (unsigned i=0;i<RT_ELEMENTS(HWACCMR0Globals.aCpuInfo);i++)
        {
            AssertMsgRC(aRc[i], ("hwaccmR0DisableCPU failed for cpu %d with rc=%d\n", i, aRc[i]));
            if (HWACCMR0Globals.aCpuInfo[i].pMemObj != NIL_RTR0MEMOBJ)
            {
                RTR0MemObjFree(HWACCMR0Globals.aCpuInfo[i].pMemObj, false);
                HWACCMR0Globals.aCpuInfo[i].pMemObj = NIL_RTR0MEMOBJ;
            }
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
        if (    !(val & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
            ||  ((val & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK)) == MSR_IA32_FEATURE_CONTROL_VMXON) /* Some BIOSes forget to set the locked bit. */
           )
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
            {
                /* Restore previous value. */
                ASMWrMsr(MSR_K6_EFER, val & ~MSR_K6_EFER_SVME);
                paRc[idCpu] = VINF_SUCCESS;
            }
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
 *
 */
VMMR0DECL(int) HWACCMR0EnableAllCpus(PVM pVM)
{
    AssertCompile(sizeof(HWACCMR0Globals.enmHwAccmState) == sizeof(uint32_t));

    /* Make sure we don't touch hwaccm after we've disabled hwaccm in preparation of a suspend. */
    if (ASMAtomicReadBool(&HWACCMR0Globals.fSuspended))
        return VERR_HWACCM_SUSPEND_PENDING;

    if (ASMAtomicCmpXchgU32((volatile uint32_t *)&HWACCMR0Globals.enmHwAccmState, HWACCMSTATE_ENABLED, HWACCMSTATE_UNINITIALIZED))
    {
        int rc;

        if (   HWACCMR0Globals.vmx.fSupported
            && HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx)
        {
            rc = SUPR0EnableVTx(true /* fEnable */);
            if (RT_SUCCESS(rc))
            {
                for (unsigned iCpu = 0; iCpu < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo); iCpu++)
                {
                    HWACCMR0Globals.aCpuInfo[iCpu].fConfigured = true;
                    Assert(HWACCMR0Globals.aCpuInfo[iCpu].pMemObj == NIL_RTR0MEMOBJ);
                }
            }
            else
                AssertMsgFailed(("HWACCMR0EnableAllCpus/SUPR0EnableVTx: rc=%Rrc\n", rc));
        }
        else
        {
            int     aRc[RTCPUSET_MAX_CPUS];
            RTCPUID idCpu = 0;

            memset(aRc, 0, sizeof(aRc));

            /* Allocate one page per cpu for the global vt-x and amd-v pages */
            for (unsigned i=0;i<RT_ELEMENTS(HWACCMR0Globals.aCpuInfo);i++)
            {
                Assert(!HWACCMR0Globals.aCpuInfo[i].pMemObj);

                /** @todo this is rather dangerous if cpus can be taken offline; we don't care for now */
                if (RTMpIsCpuOnline(i))
                {
                    rc = RTR0MemObjAllocCont(&HWACCMR0Globals.aCpuInfo[i].pMemObj, 1 << PAGE_SHIFT, true /* executable R0 mapping */);
                    AssertRC(rc);
                    if (RT_FAILURE(rc))
                        return rc;

                    void *pvR0 = RTR0MemObjAddress(HWACCMR0Globals.aCpuInfo[i].pMemObj);
                    Assert(pvR0);
                    ASMMemZeroPage(pvR0);

#ifdef LOG_ENABLED
                    SUPR0Printf("address %x phys %x\n", pvR0, (uint32_t)RTR0MemObjGetPagePhysAddr(HWACCMR0Globals.aCpuInfo[i].pMemObj, 0));
#endif
                }
            }
            /* First time, so initialize each cpu/core */
            rc = RTMpOnAll(hwaccmR0EnableCPU, (void *)pVM, aRc);

            /* Check the return code of all invocations. */
            if (RT_SUCCESS(rc))
                rc = hwaccmR0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);
            AssertMsgRC(rc, ("HWACCMR0EnableAllCpus failed for cpu %d with rc=%d\n", idCpu, rc));
        }

        return rc;
    }
    return VINF_SUCCESS;
}

/**
 * Worker function passed to RTMpOnAll, RTMpOnOthers and RTMpOnSpecific that
 * is to be called on the target cpus.
 *
 * @param   idCpu       The identifier for the CPU the function is called on.
 * @param   pvUser1     The 1st user argument.
 * @param   pvUser2     The 2nd user argument.
 */
static DECLCALLBACK(void) hwaccmR0EnableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    PVM             pVM = (PVM)pvUser1;     /* can be NULL! */
    int            *paRc = (int *)pvUser2;
    void           *pvPageCpu;
    RTHCPHYS        pPageCpuPhys;
    PHWACCM_CPUINFO pCpu = &HWACCMR0Globals.aCpuInfo[idCpu];

    Assert(!HWACCMR0Globals.vmx.fSupported || !HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx);
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /// @todo fix idCpu == index assumption (rainy day)
    Assert(idCpu < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo));
    Assert(!pCpu->fConfigured);
    Assert(ASMAtomicReadBool(&pCpu->fInUse) == false);

    pCpu->idCpu     = idCpu;

    /* Make sure we start with a clean TLB. */
    pCpu->fFlushTLB = true;

    pCpu->uCurrentASID = 0;   /* we'll aways increment this the first time (host uses ASID 0) */
    pCpu->cTLBFlushes  = 0;

    /* Should never happen */
    if (!pCpu->pMemObj)
    {
        AssertFailed();
        paRc[idCpu] = VERR_INTERNAL_ERROR;
        return;
    }

    pvPageCpu    = RTR0MemObjAddress(pCpu->pMemObj);
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(pCpu->pMemObj, 0);

    paRc[idCpu]  = HWACCMR0Globals.pfnEnableCpu(pCpu, pVM, pvPageCpu, pPageCpuPhys);
    AssertRC(paRc[idCpu]);
    if (RT_SUCCESS(paRc[idCpu]))
        pCpu->fConfigured = true;

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
static DECLCALLBACK(void) hwaccmR0DisableCPU(RTCPUID idCpu, void *pvUser1, void *pvUser2)
{
    void           *pvPageCpu;
    RTHCPHYS        pPageCpuPhys;
    int            *paRc = (int *)pvUser1;
    PHWACCM_CPUINFO pCpu = &HWACCMR0Globals.aCpuInfo[idCpu];

    Assert(!HWACCMR0Globals.vmx.fSupported || !HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx);
    Assert(idCpu == (RTCPUID)RTMpCpuIdToSetIndex(idCpu)); /// @todo fix idCpu == index assumption (rainy day)
    Assert(idCpu < RT_ELEMENTS(HWACCMR0Globals.aCpuInfo));
    Assert(ASMAtomicReadBool(&pCpu->fInUse) == false);
    Assert(!pCpu->fConfigured || pCpu->pMemObj);

    if (!pCpu->pMemObj)
        return;

    pvPageCpu    = RTR0MemObjAddress(pCpu->pMemObj);
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(pCpu->pMemObj, 0);

    if (pCpu->fConfigured)
    {
        paRc[idCpu] = HWACCMR0Globals.pfnDisableCpu(pCpu, pvPageCpu, pPageCpuPhys);
        AssertRC(paRc[idCpu]);
        pCpu->fConfigured = false;
    }
    else
        paRc[idCpu] = VINF_SUCCESS; /* nothing to do */

    pCpu->uCurrentASID = 0;
    return;
}

/**
 * Called whenever a system power state change occurs.
 *
 * @param   enmEvent        Power event
 * @param   pvUser          User argument
 */
static DECLCALLBACK(void) hwaccmR0PowerCallback(RTPOWEREVENT enmEvent, void *pvUser)
{
    NOREF(pvUser);
    Assert(!HWACCMR0Globals.vmx.fSupported || !HWACCMR0Globals.vmx.fUsingSUPR0EnableVTx);

#ifdef LOG_ENABLED
    if (enmEvent == RTPOWEREVENT_SUSPEND)
        SUPR0Printf("hwaccmR0PowerCallback RTPOWEREVENT_SUSPEND\n");
    else
        SUPR0Printf("hwaccmR0PowerCallback RTPOWEREVENT_RESUME\n");
#endif

    if (enmEvent == RTPOWEREVENT_SUSPEND)
        ASMAtomicWriteBool(&HWACCMR0Globals.fSuspended, true);

    if (HWACCMR0Globals.enmHwAccmState == HWACCMSTATE_ENABLED)
    {
        int     aRc[RTCPUSET_MAX_CPUS];
        int     rc;
        RTCPUID idCpu;

        memset(aRc, 0, sizeof(aRc));
        if (enmEvent == RTPOWEREVENT_SUSPEND)
        {
            /* Turn off VT-x or AMD-V on all CPUs. */
            rc = RTMpOnAll(hwaccmR0DisableCPU, aRc, NULL);
            Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
        }
        else
        {
            /* Reinit the CPUs from scratch as the suspend state has messed with the MSRs. */
            rc = RTMpOnAll(HWACCMR0InitCPU, (void *)((HWACCMR0Globals.vmx.fSupported) ? X86_CPUID_VENDOR_INTEL_EBX : X86_CPUID_VENDOR_AMD_EBX), aRc);
            Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);

            if (RT_SUCCESS(rc))
                rc = hwaccmR0CheckCpuRcArray(aRc, RT_ELEMENTS(aRc), &idCpu);
#ifdef LOG_ENABLED
            if (RT_FAILURE(rc))
                SUPR0Printf("hwaccmR0PowerCallback HWACCMR0InitCPU failed with %d\n", rc);
#endif

            /* Turn VT-x or AMD-V back on on all CPUs. */
            rc = RTMpOnAll(hwaccmR0EnableCPU, NULL, aRc);
            Assert(RT_SUCCESS(rc) || rc == VERR_NOT_SUPPORTED);
        }
    }
    if (enmEvent == RTPOWEREVENT_RESUME)
        ASMAtomicWriteBool(&HWACCMR0Globals.fSuspended, false);
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
VMMR0DECL(int) HWACCMR0InitVM(PVM pVM)
{
    int             rc;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0InitVM: %p\n", pVM);
#endif

    /* Make sure we don't touch hwaccm after we've disabled hwaccm in preparation of a suspend. */
    if (ASMAtomicReadBool(&HWACCMR0Globals.fSuspended))
        return VERR_HWACCM_SUSPEND_PENDING;

    pVM->hwaccm.s.vmx.fSupported            = HWACCMR0Globals.vmx.fSupported;
    pVM->hwaccm.s.svm.fSupported            = HWACCMR0Globals.svm.fSupported;

    pVM->hwaccm.s.vmx.msr.feature_ctrl      = HWACCMR0Globals.vmx.msr.feature_ctrl;
    pVM->hwaccm.s.vmx.hostCR4               = HWACCMR0Globals.vmx.hostCR4;
    pVM->hwaccm.s.vmx.msr.vmx_basic_info    = HWACCMR0Globals.vmx.msr.vmx_basic_info;
    pVM->hwaccm.s.vmx.msr.vmx_pin_ctls      = HWACCMR0Globals.vmx.msr.vmx_pin_ctls;
    pVM->hwaccm.s.vmx.msr.vmx_proc_ctls     = HWACCMR0Globals.vmx.msr.vmx_proc_ctls;
    pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2    = HWACCMR0Globals.vmx.msr.vmx_proc_ctls2;
    pVM->hwaccm.s.vmx.msr.vmx_exit          = HWACCMR0Globals.vmx.msr.vmx_exit;
    pVM->hwaccm.s.vmx.msr.vmx_entry         = HWACCMR0Globals.vmx.msr.vmx_entry;
    pVM->hwaccm.s.vmx.msr.vmx_misc          = HWACCMR0Globals.vmx.msr.vmx_misc;
    pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0    = HWACCMR0Globals.vmx.msr.vmx_cr0_fixed0;
    pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1    = HWACCMR0Globals.vmx.msr.vmx_cr0_fixed1;
    pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0    = HWACCMR0Globals.vmx.msr.vmx_cr4_fixed0;
    pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1    = HWACCMR0Globals.vmx.msr.vmx_cr4_fixed1;
    pVM->hwaccm.s.vmx.msr.vmx_vmcs_enum     = HWACCMR0Globals.vmx.msr.vmx_vmcs_enum;
    pVM->hwaccm.s.vmx.msr.vmx_eptcaps       = HWACCMR0Globals.vmx.msr.vmx_eptcaps;
    pVM->hwaccm.s.svm.u32Rev                = HWACCMR0Globals.svm.u32Rev;
    pVM->hwaccm.s.svm.u32Features           = HWACCMR0Globals.svm.u32Features;
    pVM->hwaccm.s.cpuid.u32AMDFeatureECX    = HWACCMR0Globals.cpuid.u32AMDFeatureECX;
    pVM->hwaccm.s.cpuid.u32AMDFeatureEDX    = HWACCMR0Globals.cpuid.u32AMDFeatureEDX;
    pVM->hwaccm.s.lLastError                = HWACCMR0Globals.lLastError;

    pVM->hwaccm.s.uMaxASID                  = HWACCMR0Globals.uMaxASID;

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->hwaccm.s.idEnteredCpu              = NIL_RTCPUID;

        /* Invalidate the last cpu we were running on. */
        pVCpu->hwaccm.s.idLastCpu                 = NIL_RTCPUID;

        /* we'll aways increment this the first time (host uses ASID 0) */
        pVCpu->hwaccm.s.uCurrentASID              = 0;
    }

    RTCCUINTREG     fFlags = ASMIntDisableFlags();
    PHWACCM_CPUINFO pCpu = HWACCMR0GetCurrentCpu();

    /* @note Not correct as we can be rescheduled to a different cpu, but the fInUse case is mostly for debugging. */
    ASMAtomicWriteBool(&pCpu->fInUse, true);
    ASMSetFlags(fFlags);

    /* Init a VT-x or AMD-V VM. */
    rc = HWACCMR0Globals.pfnInitVM(pVM);

    ASMAtomicWriteBool(&pCpu->fInUse, false);
    return rc;
}


/**
 * Does Ring-0 per VM HWACCM termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) HWACCMR0TermVM(PVM pVM)
{
    int             rc;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0TermVM: %p\n", pVM);
#endif

    /* Make sure we don't touch hwaccm after we've disabled hwaccm in preparation of a suspend. */
    AssertReturn(!ASMAtomicReadBool(&HWACCMR0Globals.fSuspended), VERR_HWACCM_SUSPEND_PENDING);

    /* @note Not correct as we can be rescheduled to a different cpu, but the fInUse case is mostly for debugging. */
    RTCCUINTREG     fFlags = ASMIntDisableFlags();
    PHWACCM_CPUINFO pCpu = HWACCMR0GetCurrentCpu();

    ASMAtomicWriteBool(&pCpu->fInUse, true);
    ASMSetFlags(fFlags);

    /* Terminate a VT-x or AMD-V VM. */
    rc = HWACCMR0Globals.pfnTermVM(pVM);

    ASMAtomicWriteBool(&pCpu->fInUse, false);
    return rc;
}


/**
 * Sets up a VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) HWACCMR0SetupVM(PVM pVM)
{
    int             rc;
    RTCPUID         idCpu = RTMpCpuId();
    PHWACCM_CPUINFO pCpu = &HWACCMR0Globals.aCpuInfo[idCpu];

    AssertReturn(pVM, VERR_INVALID_PARAMETER);

    /* Make sure we don't touch hwaccm after we've disabled hwaccm in preparation of a suspend. */
    AssertReturn(!ASMAtomicReadBool(&HWACCMR0Globals.fSuspended), VERR_HWACCM_SUSPEND_PENDING);

#ifdef LOG_ENABLED
    SUPR0Printf("HWACCMR0SetupVM: %p\n", pVM);
#endif

    ASMAtomicWriteBool(&pCpu->fInUse, true);

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        /* On first entry we'll sync everything. */
        pVM->aCpus[i].hwaccm.s.fContextUseFlags = HWACCM_CHANGED_ALL;
    }

    /* Setup VT-x or AMD-V. */
    rc = HWACCMR0Globals.pfnSetupVM(pVM);

    ASMAtomicWriteBool(&pCpu->fInUse, false);

    return rc;
}


/**
 * Enters the VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu      VMCPUD id.
 */
VMMR0DECL(int) HWACCMR0Enter(PVM pVM, PVMCPU pVCpu)
{
    PCPUMCTX        pCtx;
    int             rc;
    RTCPUID         idCpu = RTMpCpuId();
    PHWACCM_CPUINFO pCpu = &HWACCMR0Globals.aCpuInfo[idCpu];

    /* Make sure we can't enter a session after we've disabled hwaccm in preparation of a suspend. */
    AssertReturn(!ASMAtomicReadBool(&HWACCMR0Globals.fSuspended), VERR_HWACCM_SUSPEND_PENDING);
    ASMAtomicWriteBool(&pCpu->fInUse, true);

    pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    /* Always load the guest's FPU/XMM state on-demand. */
    CPUMDeactivateGuestFPUState(pVCpu);

    /* Always load the guest's debug state on-demand. */
    CPUMDeactivateGuestDebugState(pVCpu);

    /* Always reload the host context and the guest's CR0 register. (!!!!) */
    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0 | HWACCM_CHANGED_HOST_CONTEXT;

    /* Setup the register and mask according to the current execution mode. */
    if (pCtx->msrEFER & MSR_K6_EFER_LMA)
        pVM->hwaccm.s.u64RegisterMask = UINT64_C(0xFFFFFFFFFFFFFFFF);
    else
        pVM->hwaccm.s.u64RegisterMask = UINT64_C(0xFFFFFFFF);

    rc  = HWACCMR0Globals.pfnEnterSession(pVM, pVCpu, pCpu);
    AssertRC(rc);
    /* We must save the host context here (VT-x) as we might be rescheduled on a different cpu after a long jump back to ring 3. */
    rc |= HWACCMR0Globals.pfnSaveHostState(pVM, pVCpu);
    AssertRC(rc);
    rc |= HWACCMR0Globals.pfnLoadGuestState(pVM, pVCpu, pCtx);
    AssertRC(rc);

    /* keep track of the CPU owning the VMCS for debugging scheduling weirdness and ring-3 calls. */
    if (RT_SUCCESS(rc))
    {
        AssertMsg(pVCpu->hwaccm.s.idEnteredCpu == NIL_RTCPUID, ("%d", (int)pVCpu->hwaccm.s.idEnteredCpu));
        pVCpu->hwaccm.s.idEnteredCpu = idCpu;

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        PGMDynMapMigrateAutoSet(pVM);
#endif
    }
    return rc;
}


/**
 * Leaves the VT-x or AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu      VMCPUD id.
 */
VMMR0DECL(int) HWACCMR0Leave(PVM pVM, PVMCPU pVCpu)
{
    PCPUMCTX        pCtx;
    int             rc;
    RTCPUID         idCpu = RTMpCpuId();
    PHWACCM_CPUINFO pCpu = &HWACCMR0Globals.aCpuInfo[idCpu];

    AssertReturn(!ASMAtomicReadBool(&HWACCMR0Globals.fSuspended), VERR_HWACCM_SUSPEND_PENDING);

    pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    /* Note:  It's rather tricky with longjmps done by e.g. Log statements or the page fault handler.
     *        We must restore the host FPU here to make absolutely sure we don't leave the guest FPU state active
     *        or trash somebody else's FPU state.
     */
    /* Save the guest FPU and XMM state if necessary. */
    if (CPUMIsGuestFPUStateActive(pVCpu))
    {
        Log2(("CPUMR0SaveGuestFPU\n"));
        CPUMR0SaveGuestFPU(pVM, pVCpu, pCtx);

        pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_GUEST_CR0;
        Assert(!CPUMIsGuestFPUStateActive(pVCpu));
    }

    rc = HWACCMR0Globals.pfnLeaveSession(pVM, pVCpu, pCtx);

    /* keep track of the CPU owning the VMCS for debugging scheduling weirdness and ring-3 calls. */
#ifdef RT_STRICT
    if (RT_UNLIKELY(    pVCpu->hwaccm.s.idEnteredCpu != idCpu
                    &&  RT_FAILURE(rc)))
    {
        AssertMsgFailed(("Owner is %d, I'm %d", (int)pVCpu->hwaccm.s.idEnteredCpu, (int)idCpu));
        rc = VERR_INTERNAL_ERROR;
    }
#endif
    pVCpu->hwaccm.s.idEnteredCpu = NIL_RTCPUID;

    ASMAtomicWriteBool(&pCpu->fInUse, false);
    return rc;
}

/**
 * Runs guest code in a hardware accelerated VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu      VMCPUD id.
 */
VMMR0DECL(int) HWACCMR0RunGuestCode(PVM pVM, PVMCPU pVCpu)
{
    CPUMCTX *pCtx;
    RTCPUID  idCpu = RTMpCpuId(); NOREF(idCpu);
    int      rc;
#ifdef VBOX_STRICT
    PHWACCM_CPUINFO pCpu = &HWACCMR0Globals.aCpuInfo[idCpu];
#endif

    Assert(!VM_FF_ISPENDING(pVM, VM_FF_PGM_SYNC_CR3 | VM_FF_PGM_SYNC_CR3_NON_GLOBAL));
    Assert(HWACCMR0Globals.aCpuInfo[idCpu].fConfigured);
    AssertReturn(!ASMAtomicReadBool(&HWACCMR0Globals.fSuspended), VERR_HWACCM_SUSPEND_PENDING);
    Assert(ASMAtomicReadBool(&pCpu->fInUse) == true);

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    PGMDynMapStartAutoSet(pVM);
#endif

    pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    rc = HWACCMR0Globals.pfnRunGuestCode(pVM, pVCpu, pCtx);

#ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
    PGMDynMapReleaseAutoSet(pVM);
#endif
    return rc;
}


#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
/**
 * Save guest FPU/XMM state (64 bits guest mode & 32 bits host only)
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int)   HWACCMR0SaveFPUState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (pVM->hwaccm.s.vmx.fSupported)
        return VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnSaveGuestFPU64, 0, NULL);

    return SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnSaveGuestFPU64, 0, NULL);
}

/**
 * Save guest debug state (64 bits guest mode & 32 bits host only)
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 * @param   pVCpu       VMCPU handle.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int)   HWACCMR0SaveDebugState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (pVM->hwaccm.s.vmx.fSupported)
        return VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnSaveGuestDebug64, 0, NULL);

    return SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnSaveGuestDebug64, 0, NULL);
}

/**
 * Test the 32->64 bits switcher
 *
 * @returns VBox status code.
 * @param   pVM         VM handle.
 */
VMMR0DECL(int)   HWACCMR0TestSwitcher3264(PVM pVM)
{
    PVMCPU   pVCpu = &pVM->aCpus[0];
    CPUMCTX *pCtx;
    uint32_t aParam[5] = {0, 1, 2, 3, 4};
    int      rc;

    pCtx = CPUMQueryGuestCtxPtr(pVCpu);

    STAM_PROFILE_ADV_START(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);   
    if (pVM->hwaccm.s.vmx.fSupported)
        rc = VMXR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnTest64, 5, &aParam[0]);
    else
        rc = SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, pVM->hwaccm.s.pfnTest64, 5, &aParam[0]);
    STAM_PROFILE_ADV_STOP(&pVCpu->hwaccm.s.StatWorldSwitch3264, z);
    return rc;
}

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */

/**
 * Returns suspend status of the host
 *
 * @returns Suspend pending or not
 */
VMMR0DECL(bool) HWACCMR0SuspendPending()
{
    return ASMAtomicReadBool(&HWACCMR0Globals.fSuspended);
}

/**
 * Returns the cpu structure for the current cpu.
 * Keep in mind that there is no guarantee it will stay the same (long jumps to ring 3!!!).
 *
 * @returns cpu structure pointer
 */
VMMR0DECL(PHWACCM_CPUINFO) HWACCMR0GetCurrentCpu()
{
    RTCPUID  idCpu = RTMpCpuId();

    return &HWACCMR0Globals.aCpuInfo[idCpu];
}

/**
 * Returns the cpu structure for the current cpu.
 * Keep in mind that there is no guarantee it will stay the same (long jumps to ring 3!!!).
 *
 * @returns cpu structure pointer
 * @param   idCpu       id of the VCPU
 */
VMMR0DECL(PHWACCM_CPUINFO) HWACCMR0GetCurrentCpuEx(RTCPUID idCpu)
{
    return &HWACCMR0Globals.aCpuInfo[idCpu];
}

/**
 * Disable VT-x if it's active *and* the current switcher turns off paging
 *
 * @returns VBox status code.
 * @param   pVM             VM handle.
 * @param   pfVTxDisabled   VT-x was disabled or not (out)
 */
VMMR0DECL(int) HWACCMR0EnterSwitcher(PVM pVM, bool *pfVTxDisabled)
{
    Assert(!(ASMGetFlags() & X86_EFL_IF));

    *pfVTxDisabled = false;

    if (    HWACCMR0Globals.enmHwAccmState != HWACCMSTATE_ENABLED
        ||  !HWACCMR0Globals.vmx.fSupported /* no such issues with AMD-V */)
        return VINF_SUCCESS;    /* nothing to do */

    switch(VMMGetSwitcher(pVM))
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
        AssertFailed();
        return VERR_INTERNAL_ERROR;
    }

    PHWACCM_CPUINFO pCpu = HWACCMR0GetCurrentCpu();
    void           *pvPageCpu;
    RTHCPHYS        pPageCpuPhys;

    AssertReturn(pCpu && pCpu->pMemObj, VERR_INTERNAL_ERROR);
    pvPageCpu    = RTR0MemObjAddress(pCpu->pMemObj);
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(pCpu->pMemObj, 0);

    *pfVTxDisabled = true;
    return VMXR0DisableCpu(pCpu, pvPageCpu, pPageCpuPhys);
}

/**
 * Reeable VT-x if was active *and* the current switcher turned off paging
 *
 * @returns VBox status code.
 * @param   pVM          VM handle.
 * @param   fVTxDisabled VT-x was disabled or not
 */
VMMR0DECL(int) HWACCMR0LeaveSwitcher(PVM pVM, bool fVTxDisabled)
{
    Assert(!(ASMGetFlags() & X86_EFL_IF));

    if (!fVTxDisabled)
        return VINF_SUCCESS;    /* nothing to do */

    Assert(   HWACCMR0Globals.enmHwAccmState == HWACCMSTATE_ENABLED
           && HWACCMR0Globals.vmx.fSupported);

    PHWACCM_CPUINFO pCpu = HWACCMR0GetCurrentCpu();
    void           *pvPageCpu;
    RTHCPHYS        pPageCpuPhys;

    AssertReturn(pCpu && pCpu->pMemObj, VERR_INTERNAL_ERROR);
    pvPageCpu    = RTR0MemObjAddress(pCpu->pMemObj);
    pPageCpuPhys = RTR0MemObjGetPagePhysAddr(pCpu->pMemObj, 0);

    return VMXR0EnableCpu(pCpu, pVM, pvPageCpu, pPageCpuPhys);
}

#ifdef VBOX_STRICT
# include <iprt/string.h>
/**
 * Dumps a descriptor.
 *
 * @param   pDesc    Descriptor to dump.
 * @param   Sel     Selector number.
 * @param   pszMsg  Message to prepend the log entry with.
 */
VMMR0DECL(void) HWACCMR0DumpDescriptor(PX86DESCHC pDesc, RTSEL Sel, const char *pszMsg)
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
    memcpy(psz, aTypes[i].psz, aTypes[i].cch);
    psz += aTypes[i].cch;

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
    uint32_t    u32Limit = X86DESC_LIMIT(*pDesc);
    if (pDesc->Gen.u1Granularity)
        u32Limit = u32Limit << PAGE_SHIFT | PAGE_OFFSET_MASK;

# if HC_ARCH_BITS == 64
    uint64_t    u32Base =  X86DESC64_BASE(*pDesc);

    Log(("%s %04x - %RX64 %RX64 - base=%RX64 limit=%08x dpl=%d %s\n", pszMsg,
         Sel, pDesc->au64[0], pDesc->au64[1], u32Base, u32Limit, pDesc->Gen.u2Dpl, szMsg));
# else
    uint32_t    u32Base =  X86DESC_BASE(*pDesc);

    Log(("%s %04x - %08x %08x - base=%08x limit=%08x dpl=%d %s\n", pszMsg,
         Sel, pDesc->au32[0], pDesc->au32[1], u32Base, u32Limit, pDesc->Gen.u2Dpl, szMsg));
# endif
}

/**
 * Formats a full register dump.
 *
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        The context to format.
 */
VMMR0DECL(void) HWACCMDumpRegs(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
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
    for (unsigned i = 0; i < RT_ELEMENTS(aFlags); i++)
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
    if (CPUMIsGuestIn64BitCode(pVCpu, CPUMCTX2CORE(pCtx)))
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
            pCtx->rip, pCtx->rsp, pCtx->rbp, X86_EFL_GET_IOPL(efl), 31, szEFlags,
            (RTSEL)pCtx->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u,
            (RTSEL)pCtx->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u,
            (RTSEL)pCtx->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u,
            (RTSEL)pCtx->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u,
            (RTSEL)pCtx->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u,
            (RTSEL)pCtx->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u,
            pCtx->cr0,  pCtx->cr2, pCtx->cr3,  pCtx->cr4,
            pCtx->dr[0],  pCtx->dr[1], pCtx->dr[2],  pCtx->dr[3],
            pCtx->dr[4],  pCtx->dr[5], pCtx->dr[6],  pCtx->dr[7],
            pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, efl,
            (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u64Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
            (RTSEL)pCtx->tr, pCtx->trHid.u64Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
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
            pCtx->eip, pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(efl), 31, szEFlags,
            (RTSEL)pCtx->cs, pCtx->csHid.u64Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pCtx->dr[0],  pCtx->dr[1],
            (RTSEL)pCtx->ds, pCtx->dsHid.u64Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pCtx->dr[2],  pCtx->dr[3],
            (RTSEL)pCtx->es, pCtx->esHid.u64Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pCtx->dr[4],  pCtx->dr[5],
            (RTSEL)pCtx->fs, pCtx->fsHid.u64Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pCtx->dr[6],  pCtx->dr[7],
            (RTSEL)pCtx->gs, pCtx->gsHid.u64Base, pCtx->gsHid.u32Limit, pCtx->gsHid.Attr.u, pCtx->cr0,  pCtx->cr2,
            (RTSEL)pCtx->ss, pCtx->ssHid.u64Base, pCtx->ssHid.u32Limit, pCtx->ssHid.Attr.u, pCtx->cr3,  pCtx->cr4,
            pCtx->gdtr.pGdt, pCtx->gdtr.cbGdt, pCtx->idtr.pIdt, pCtx->idtr.cbIdt, efl,
            (RTSEL)pCtx->ldtr, pCtx->ldtrHid.u64Base, pCtx->ldtrHid.u32Limit, pCtx->ldtrHid.Attr.u,
            (RTSEL)pCtx->tr, pCtx->trHid.u64Base, pCtx->trHid.u32Limit, pCtx->trHid.Attr.u,
            pCtx->SysEnter.cs, pCtx->SysEnter.eip, pCtx->SysEnter.esp));

    Log(("FPU:\n"
        "FCW=%04x FSW=%04x FTW=%02x\n"
        "res1=%02x FOP=%04x FPUIP=%08x CS=%04x Rsvrd1=%04x\n"
        "FPUDP=%04x DS=%04x Rsvrd2=%04x MXCSR=%08x MXCSR_MASK=%08x\n"
        ,
        pCtx->fpu.FCW, pCtx->fpu.FSW, pCtx->fpu.FTW,
        pCtx->fpu.huh1, pCtx->fpu.FOP, pCtx->fpu.FPUIP, pCtx->fpu.CS, pCtx->fpu.Rsvrd1,
        pCtx->fpu.FPUDP, pCtx->fpu.DS, pCtx->fpu.Rsrvd2,
        pCtx->fpu.MXCSR, pCtx->fpu.MXCSR_MASK));


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

/* Dummy callback handlers. */
VMMR0DECL(int) HWACCMR0DummyEnter(PVM pVM, PVMCPU pVCpu, PHWACCM_CPUINFO pCpu)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyLeave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyEnableCpu(PHWACCM_CPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyDisableCpu(PHWACCM_CPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyInitVM(PVM pVM)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyTermVM(PVM pVM)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummySetupVM(PVM pVM)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyRunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummySaveHostState(PVM pVM, PVMCPU pVCpu)
{
    return VINF_SUCCESS;
}

VMMR0DECL(int) HWACCMR0DummyLoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    return VINF_SUCCESS;
}
