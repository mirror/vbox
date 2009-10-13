/* $Revision$ */
/** @file
 * VBoxDrv - The VirtualBox Support Driver - Common OS agnostic.
 */

/*
 * Copyright (C) 2006-2009 Sun Microsystems, Inc.
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_SUP_DRV
#define SUPDRV_AGNOSTIC
#include "SUPDrvInternal.h"

/** @todo trim this down. */
#include <iprt/param.h>
#include <iprt/alloc.h>
#include <iprt/cpuset.h>
#include <iprt/handletable.h>
#include <iprt/mp.h>
#include <iprt/power.h>
#include <iprt/process.h>
#include <iprt/semaphore.h>
#include <iprt/spinlock.h>
#include <iprt/thread.h>
#include <iprt/uuid.h>

#include <VBox/param.h>
#include <VBox/log.h>
#include <VBox/err.h>
#include <VBox/hwacc_svm.h>
#include <VBox/hwacc_vmx.h>
#include <VBox/x86.h>


/**
 * Gets the paging mode of the current CPU.
 *
 * @returns Paging mode, SUPPAGEINGMODE_INVALID on error.
 */
SUPR0DECL(SUPPAGINGMODE) SUPR0GetPagingMode(void)
{
    SUPPAGINGMODE enmMode;

    RTR0UINTREG cr0 = ASMGetCR0();
    if ((cr0 & (X86_CR0_PG | X86_CR0_PE)) != (X86_CR0_PG | X86_CR0_PE))
        enmMode = SUPPAGINGMODE_INVALID;
    else
    {
        RTR0UINTREG cr4 = ASMGetCR4();
        uint32_t fNXEPlusLMA = 0;
        if (cr4 & X86_CR4_PAE)
        {
            uint32_t fAmdFeatures = ASMCpuId_EDX(0x80000001);
            if (fAmdFeatures & (X86_CPUID_AMD_FEATURE_EDX_NX | X86_CPUID_AMD_FEATURE_EDX_LONG_MODE))
            {
                uint64_t efer = ASMRdMsr(MSR_K6_EFER);
                if ((fAmdFeatures & X86_CPUID_AMD_FEATURE_EDX_NX)        && (efer & MSR_K6_EFER_NXE))
                    fNXEPlusLMA |= RT_BIT(0);
                if ((fAmdFeatures & X86_CPUID_AMD_FEATURE_EDX_LONG_MODE) && (efer & MSR_K6_EFER_LMA))
                    fNXEPlusLMA |= RT_BIT(1);
            }
        }

        switch ((cr4 & (X86_CR4_PAE | X86_CR4_PGE)) | fNXEPlusLMA)
        {
            case 0:
                enmMode = SUPPAGINGMODE_32_BIT;
                break;

            case X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_32_BIT_GLOBAL;
                break;

            case X86_CR4_PAE:
                enmMode = SUPPAGINGMODE_PAE;
                break;

            case X86_CR4_PAE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_PAE_NX;
                break;

            case X86_CR4_PAE | X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case X86_CR4_PAE | X86_CR4_PGE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_PAE_GLOBAL;
                break;

            case RT_BIT(1) | X86_CR4_PAE:
                enmMode = SUPPAGINGMODE_AMD64;
                break;

            case RT_BIT(1) | X86_CR4_PAE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_AMD64_NX;
                break;

            case RT_BIT(1) | X86_CR4_PAE | X86_CR4_PGE:
                enmMode = SUPPAGINGMODE_AMD64_GLOBAL;
                break;

            case RT_BIT(1) | X86_CR4_PAE | X86_CR4_PGE | RT_BIT(0):
                enmMode = SUPPAGINGMODE_AMD64_GLOBAL_NX;
                break;

            default:
                AssertMsgFailed(("Cannot happen! cr4=%#x fNXEPlusLMA=%d\n", cr4, fNXEPlusLMA));
                enmMode = SUPPAGINGMODE_INVALID;
                break;
        }
    }
    return enmMode;
}


/**
 * Enables or disabled hardware virtualization extensions using native OS APIs.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS on success.
 * @retval  VERR_NOT_SUPPORTED if not supported by the native OS.
 *
 * @param   fEnable         Whether to enable or disable.
 */
SUPR0DECL(int) SUPR0EnableVTx(bool fEnable)
{
#ifdef RT_OS_DARWIN
    return supdrvOSEnableVTx(fEnable);
#else
    return VERR_NOT_SUPPORTED;
#endif
}



SUPR0DECL(int) SUPR0QueryVTCaps(PSUPDRVSESSION pSession, uint32_t *pfCaps)
{
    /*
     * Input validation.
     */
    AssertReturn(SUP_IS_SESSION_VALID(pSession), VERR_INVALID_PARAMETER);
    AssertPtrReturn(pfCaps, VERR_INVALID_POINTER);

    *pfCaps = 0;

    if (ASMHasCpuId())
    {
        uint32_t u32FeaturesECX;
        uint32_t u32Dummy;
        uint32_t u32FeaturesEDX;
        uint32_t u32VendorEBX, u32VendorECX, u32VendorEDX, u32AMDFeatureEDX, u32AMDFeatureECX;
        uint64_t val;

        ASMCpuId(0, &u32Dummy, &u32VendorEBX, &u32VendorECX, &u32VendorEDX);
        ASMCpuId(1, &u32Dummy, &u32Dummy, &u32FeaturesECX, &u32FeaturesEDX);
        /* Query AMD features. */
        ASMCpuId(0x80000001, &u32Dummy, &u32Dummy, &u32AMDFeatureECX, &u32AMDFeatureEDX);

        if (    u32VendorEBX == X86_CPUID_VENDOR_INTEL_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_INTEL_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_INTEL_EDX
           )
        {
            if (    (u32FeaturesECX & X86_CPUID_FEATURE_ECX_VMX)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                 && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                val = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
                /*
                 * Both the LOCK and VMXON bit must be set; otherwise VMXON will generate a #GP.
                 * Once the lock bit is set, this MSR can no longer be modified.
                 */
                if (       (val & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                        ==        (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK) /* enabled and locked */
                    ||  !(val & MSR_IA32_FEATURE_CONTROL_LOCK) /* not enabled, but not locked either */
                   )
                {
                    VMX_CAPABILITY vtCaps;

                    *pfCaps |= SUPVTCAPS_VT_X;

                    vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                    if (vtCaps.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                    {
                        vtCaps.u = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS2);
                        if (vtCaps.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                            *pfCaps |= SUPVTCAPS_NESTED_PAGING;
                    }
                    return VINF_SUCCESS;
                }
                return VERR_VMX_MSR_LOCKED_OR_DISABLED;
            }
            return VERR_VMX_NO_VMX;
        }

        if (    u32VendorEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  u32VendorECX == X86_CPUID_VENDOR_AMD_ECX
            &&  u32VendorEDX == X86_CPUID_VENDOR_AMD_EDX
           )
        {
            if (   (u32AMDFeatureECX & X86_CPUID_AMD_FEATURE_ECX_SVM)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                /* Check if SVM is disabled */
                val = ASMRdMsr(MSR_K8_VM_CR);
                if (!(val & MSR_K8_VM_CR_SVM_DISABLE))
                {
                    *pfCaps |= SUPVTCAPS_AMD_V;

                    /* Query AMD features. */
                    ASMCpuId(0x8000000A, &u32Dummy, &u32Dummy, &u32Dummy, &u32FeaturesEDX);

                    if (u32FeaturesEDX & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                        *pfCaps |= SUPVTCAPS_NESTED_PAGING;

                    return VINF_SUCCESS;
                }
                return VERR_SVM_DISABLED;
            }
            return VERR_SVM_NO_SVM;
        }
    }

    return VERR_UNSUPPORTED_CPU;
}


/**
 * Determin the GIP TSC mode.
 *
 * @returns The most suitable TSC mode.
 * @param   pDevExt     Pointer to the device instance data.
 */
SUPGIPMODE VBOXCALL supdrvGipDeterminTscMode(PSUPDRVDEVEXT pDevExt)
{
    /*
     * On SMP we're faced with two problems:
     *      (1) There might be a skew between the CPU, so that cpu0
     *          returns a TSC that is sligtly different from cpu1.
     *      (2) Power management (and other things) may cause the TSC
     *          to run at a non-constant speed, and cause the speed
     *          to be different on the cpus. This will result in (1).
     *
     * So, on SMP systems we'll have to select the ASYNC update method
     * if there are symphoms of these problems.
     */
    if (RTMpGetCount() > 1)
    {
        uint32_t uEAX, uEBX, uECX, uEDX;
        uint64_t u64DiffCoresIgnored;

        /* Permit the user and/or the OS specfic bits to force async mode. */
        if (supdrvOSGetForcedAsyncTscMode(pDevExt))
            return SUPGIPMODE_ASYNC_TSC;

        /* Try check for current differences between the cpus. */
        if (supdrvDetermineAsyncTsc(&u64DiffCoresIgnored))
            return SUPGIPMODE_ASYNC_TSC;

        /*
         * If the CPU supports power management and is an AMD one we
         * won't trust it unless it has the TscInvariant bit is set.
         */
        /* Check for "AuthenticAMD" */
        ASMCpuId(0, &uEAX, &uEBX, &uECX, &uEDX);
        if (    uEAX >= 1
            &&  uEBX == X86_CPUID_VENDOR_AMD_EBX
            &&  uECX == X86_CPUID_VENDOR_AMD_ECX
            &&  uEDX == X86_CPUID_VENDOR_AMD_EDX)
        {
            /* Check for APM support and that TscInvariant is cleared. */
            ASMCpuId(0x80000000, &uEAX, &uEBX, &uECX, &uEDX);
            if (uEAX >= 0x80000007)
            {
                ASMCpuId(0x80000007, &uEAX, &uEBX, &uECX, &uEDX);
                if (    !(uEDX & RT_BIT(8))/* TscInvariant */
                    &&  (uEDX & 0x3e))  /* STC|TM|THERMTRIP|VID|FID. Ignore TS. */
                    return SUPGIPMODE_ASYNC_TSC;
            }
        }
    }
    return SUPGIPMODE_SYNC_TSC;
}

