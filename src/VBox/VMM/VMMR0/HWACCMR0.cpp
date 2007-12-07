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
#include "HWVMXR0.h"
#include "HWSVMR0.h"

/**
 * Does Ring-0 HWACCM initialization.
 *
 * This is mainly to check that the Host CPU mode is compatible
 * with VMX.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Init(PVM pVM)
{
    LogComFlow(("HWACCMR0Init: %p\n", pVM));

    pVM->hwaccm.s.vmx.fSupported = false;;
    pVM->hwaccm.s.svm.fSupported = false;;

#ifndef VBOX_WITH_HYBIRD_32BIT_KERNEL /* paranoia */

    pVM->hwaccm.s.fHWACCMR0Init = true;
    pVM->hwaccm.s.lLastError    = VINF_SUCCESS;

    /*
     * Check for VMX capabilities
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
        ASMCpuId(0x80000001, &u32Dummy, &u32Dummy, &pVM->hwaccm.s.cpuid.u32AMDFeatureECX, &pVM->hwaccm.s.cpuid.u32AMDFeatureEDX);

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
                pVM->hwaccm.s.vmx.msr.feature_ctrl    = ASMRdMsr(MSR_IA32_FEATURE_CONTROL);
                /*
                 * Both the LOCK and VMXON bit must be set; otherwise VMXON will generate a #GP.
                 * Once the lock bit is set, this MSR can no longer be modified.
                 */
                /** @todo need to check this for each cpu/core in the system!!!) */
                if (!(pVM->hwaccm.s.vmx.msr.feature_ctrl & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK)))
                {
                    /* MSR is not yet locked; we can change it ourselves here */
                    pVM->hwaccm.s.vmx.msr.feature_ctrl |= (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK);
                    ASMWrMsr(MSR_IA32_FEATURE_CONTROL, pVM->hwaccm.s.vmx.msr.feature_ctrl);
                }

                if (   (pVM->hwaccm.s.vmx.msr.feature_ctrl & (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                                                          == (MSR_IA32_FEATURE_CONTROL_VMXON|MSR_IA32_FEATURE_CONTROL_LOCK))
                {
                    pVM->hwaccm.s.vmx.fSupported          = true;
                    pVM->hwaccm.s.vmx.msr.vmx_basic_info  = ASMRdMsr(MSR_IA32_VMX_BASIC_INFO);
                    pVM->hwaccm.s.vmx.msr.vmx_pin_ctls    = ASMRdMsr(MSR_IA32_VMX_PINBASED_CTLS);
                    pVM->hwaccm.s.vmx.msr.vmx_proc_ctls   = ASMRdMsr(MSR_IA32_VMX_PROCBASED_CTLS);
                    pVM->hwaccm.s.vmx.msr.vmx_exit        = ASMRdMsr(MSR_IA32_VMX_EXIT_CTLS);
                    pVM->hwaccm.s.vmx.msr.vmx_entry       = ASMRdMsr(MSR_IA32_VMX_ENTRY_CTLS);
                    pVM->hwaccm.s.vmx.msr.vmx_misc        = ASMRdMsr(MSR_IA32_VMX_MISC);
                    pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0  = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED0);
                    pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1  = ASMRdMsr(MSR_IA32_VMX_CR0_FIXED1);
                    pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0  = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED0);
                    pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1  = ASMRdMsr(MSR_IA32_VMX_CR4_FIXED1);
                    pVM->hwaccm.s.vmx.msr.vmx_vmcs_enum   = ASMRdMsr(MSR_IA32_VMX_VMCS_ENUM);

                    /*
                     * Check CR4.VMXE
                     */
                    pVM->hwaccm.s.vmx.hostCR4 = ASMGetCR4();
                    if (!(pVM->hwaccm.s.vmx.hostCR4 & X86_CR4_VMXE))
                    {
                        /* In theory this bit could be cleared behind our back. Which would cause #UD faults when we
                         * try to execute the VMX instructions...
                         */
                        ASMSetCR4(pVM->hwaccm.s.vmx.hostCR4 | X86_CR4_VMXE);
                    }

                    if (    pVM->hwaccm.s.vmx.pVMXONPhys 
                        &&  pVM->hwaccm.s.vmx.pVMXON)
                    {
                        /* Set revision dword at the beginning of the structure. */
                        *(uint32_t *)pVM->hwaccm.s.vmx.pVMXON = MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info);

#if HC_ARCH_BITS == 64
                        /* Enter VMX Root Mode */
                        int rc = VMXEnable(pVM->hwaccm.s.vmx.pVMXONPhys);
                        if (VBOX_FAILURE(rc))
                        {
                            /* KVM leaves the CPU in VMX root mode. Not only is this not allowed, it will crash the host when we enter raw mode, because
                             * (a) clearing X86_CR4_VMXE in CR4 causes a #GP    (we no longer modify this bit)
                             * (b) turning off paging causes a #GP              (unavoidable when switching from long to 32 bits mode)
                             *
                             * They should fix their code, but until they do we simply refuse to run.
                             */
                            return VERR_VMX_IN_VMX_ROOT_MODE;
                        }
                        VMXDisable();
#endif
                    }
                    /* Restore CR4 again; don't leave the X86_CR4_VMXE flag set if it wasn't so before (some software could incorrectly think it's in VMX mode) */
                    ASMSetCR4(pVM->hwaccm.s.vmx.hostCR4);
                }
                else
                    pVM->hwaccm.s.lLastError = VERR_VMX_ILLEGAL_FEATURE_CONTROL_MSR;
            }
            else
                pVM->hwaccm.s.lLastError = VERR_VMX_NO_VMX;
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
            if (   (pVM->hwaccm.s.cpuid.u32AMDFeatureECX & X86_CPUID_AMD_FEATURE_ECX_SVM)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_MSR)
                && (u32FeaturesEDX & X86_CPUID_FEATURE_EDX_FXSR)
               )
            {
                uint64_t val;

                /* Check if SVM is disabled */
                val = ASMRdMsr(MSR_K8_VM_CR);
                if (!(val & MSR_K8_VM_CR_SVM_DISABLE))
                {
                    /* Turn on SVM in the EFER MSR. */
                    val = ASMRdMsr(MSR_K6_EFER);
                    if (!(val & MSR_K6_EFER_SVME))
                    {
                        ASMWrMsr(MSR_K6_EFER, val | MSR_K6_EFER_SVME);
                    }
                    /* Paranoia. */
                    val = ASMRdMsr(MSR_K6_EFER);
                    if (val & MSR_K6_EFER_SVME)
                    {
                        /* Query AMD features. */
                        ASMCpuId(0x8000000A, &pVM->hwaccm.s.svm.u32Rev, &pVM->hwaccm.s.svm.u32MaxASID, &u32Dummy, &u32Dummy);

                        pVM->hwaccm.s.svm.fSupported = true;
                    }
                    else
                    {
                        pVM->hwaccm.s.lLastError = VERR_SVM_ILLEGAL_EFER_MSR;
                        AssertFailed();
                    }
                }
                else
                    pVM->hwaccm.s.lLastError = VERR_SVM_DISABLED;
            }
            else
                pVM->hwaccm.s.lLastError = VERR_SVM_NO_SVM;
        }
        else
            pVM->hwaccm.s.lLastError = VERR_HWACCM_UNKNOWN_CPU;
    }
    else
        pVM->hwaccm.s.lLastError = VERR_HWACCM_NO_CPUID;

#endif /* !VBOX_WITH_HYBIRD_32BIT_KERNEL */

    return VINF_SUCCESS;
}


/**
 * Sets up and activates VMX
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0SetupVMX(PVM pVM)
{
    int rc = VINF_SUCCESS;

    if (pVM == NULL)
        return VERR_INVALID_PARAMETER;

    /* Setup Intel VMX. */
    if (pVM->hwaccm.s.vmx.fSupported)
        rc = VMXR0Setup(pVM);
    else
        rc = SVMR0Setup(pVM);

    return rc;
}


/**
 * Enable VMX or SVN
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Enable(PVM pVM)
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
        rc  = VMXR0Enable(pVM);
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
        rc  = SVMR0Enable(pVM);
        AssertRC(rc);
        rc |= SVMR0LoadGuestState(pVM, pCtx);
        AssertRC(rc);
        if (rc != VINF_SUCCESS)
            return rc;

    }
    return VINF_SUCCESS;
}


/**
 * Disable VMX or SVN
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) HWACCMR0Disable(PVM pVM)
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
        return VMXR0Disable(pVM);
    }
    else
    {
        Assert(pVM->hwaccm.s.svm.fSupported);
        return SVMR0Disable(pVM);
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
         "cs={%04x base=%08x limit=%08x flags=%08x} dr0=%08x dr1=%08x\n"
         "ds={%04x base=%08x limit=%08x flags=%08x} dr2=%08x dr3=%08x\n"
         "es={%04x base=%08x limit=%08x flags=%08x} dr4=%08x dr5=%08x\n"
         "fs={%04x base=%08x limit=%08x flags=%08x} dr6=%08x dr7=%08x\n"
         ,
         pCtx->eax, pCtx->ebx, pCtx->ecx, pCtx->edx, pCtx->esi, pCtx->edi,
         pCtx->eip, pCtx->esp, pCtx->ebp, X86_EFL_GET_IOPL(efl), 31, szEFlags,
         (RTSEL)pCtx->cs, pCtx->csHid.u32Base, pCtx->csHid.u32Limit, pCtx->csHid.Attr.u, pCtx->dr0,  pCtx->dr1,
         (RTSEL)pCtx->ds, pCtx->dsHid.u32Base, pCtx->dsHid.u32Limit, pCtx->dsHid.Attr.u, pCtx->dr2,  pCtx->dr3,
         (RTSEL)pCtx->es, pCtx->esHid.u32Base, pCtx->esHid.u32Limit, pCtx->esHid.Attr.u, pCtx->dr4,  pCtx->dr5,
         (RTSEL)pCtx->fs, pCtx->fsHid.u32Base, pCtx->fsHid.u32Limit, pCtx->fsHid.Attr.u, pCtx->dr6,  pCtx->dr7));

    Log(("gs={%04x base=%08x limit=%08x flags=%08x} cr0=%08x cr2=%08x\n"
         "ss={%04x base=%08x limit=%08x flags=%08x} cr3=%08x cr4=%08x\n"
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
