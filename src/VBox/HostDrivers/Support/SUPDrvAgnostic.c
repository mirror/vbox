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
 * Internal worker for SUPR0QueryVTCaps.
 *
 * @returns See QUPR0QueryVTCaps.
 * @param   pfCaps              See QUPR0QueryVTCaps
 */
int VBOXCALL supR0QueryVTCaps(uint32_t *pfCaps)
{
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

