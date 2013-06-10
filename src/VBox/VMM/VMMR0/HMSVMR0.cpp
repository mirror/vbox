/* $Id$ */
/** @file
 * HM SVM (AMD-V) - Host Context Ring-0.
 */

/*
 * Copyright (C) 2013 Oracle Corporation
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

#ifdef DEBUG_ramshankar
# define HMSVM_ALWAYS_TRAP_ALL_XCPTS
# define HMSVM_ALWAYS_TRAP_PF
#endif


/*******************************************************************************
*   Defined Constants And Macros                                               *
*******************************************************************************/
/** @name Segment attribute conversion between CPU and AMD-V VMCB format.
 *
 * The CPU format of the segment attribute is described in X86DESCATTRBITS
 * which is 16-bits (i.e. includes 4 bits of the segment limit).
 *
 * The AMD-V VMCB format the segment attribute is compact 12-bits (strictly
 * only the attribute bits and nothing else). Upper 4-bits are unused.
 *
 * @{ */
#define HMSVM_CPU_2_VMCB_SEG_ATTR(a)       (a & 0xff) | ((a & 0xf000) >> 4)
#define HMSVM_VMCB_2_CPU_SEG_ATTR(a)       (a & 0xff) | ((a & 0x0f00) << 4)
/** @} */

/** @name Macros for loading, storing segment registers to/from the VMCB.
 *  @{ */
#define HMSVM_LOAD_SEG_REG(REG, reg) \
    do \
    { \
        Assert(pCtx->reg.fFlags & CPUMSELREG_FLAGS_VALID); \
        Assert(pCtx->reg.ValidSel == pCtx->reg.Sel); \
        pVmcb->guest.REG.u16Sel     = pCtx->reg.Sel; \
        pVmcb->guest.REG.u32Limit   = pCtx->reg.u32Limit; \
        pVmcb->guest.REG.u64Base    = pCtx->reg.u64Base; \
        pVmcb->guest.REG.u16Attr    = HMSVM_CPU_2_VMCB_SEG_ATTR(pCtx->reg.Attr.u); \
    } while (0)

#define HMSVM_SAVE_SEG_REG(REG, reg) \
    do \
    { \
        pCtx->reg.Sel       = pVmcb->guest.REG.u16Sel; \
        pCtx->reg.ValidSel  = pVmcb->guest.REG.u16Sel; \
        pCtx->reg.fFlags    = CPUMSELREG_FLAGS_VALID; \
        pCtx->reg.u32Limit  = pVmcb->guest.REG.u32Limit; \
        pCtx->reg.u64Base   = pVmcb->guest.REG.u64Base; \
        pCtx->reg.Attr.u    = HMSVM_VMCB_2_CPU_SEG_ATTR(pVmcb->guest.REG.u16Attr); \
    } while (0)
/** @} */

/** @name VMCB Clean Bits used for VMCB-state caching. */
/** All intercepts vectors, TSC offset, PAUSE filter counter. */
#define HMSVM_VMCB_CLEAN_INTERCEPTS             RT_BIT(0)
/** I/O permission bitmap, MSR permission bitmap. */
#define HMSVM_VMCB_CLEAN_IOPM_MSRPM             RT_BIT(1)
/** ASID.  */
#define HMSVM_VMCB_CLEAN_ASID                   RT_BIT(2)
/** TRP: V_TPR, V_IRQ, V_INTR_PRIO, V_IGN_TPR, V_INTR_MASKING,
V_INTR_VECTOR. */
#define HMSVM_VMCB_CLEAN_TPR                    RT_BIT(3)
/** Nested Paging: Nested CR3 (nCR3), PAT. */
#define HMSVM_VMCB_CLEAN_NP                     RT_BIT(4)
/** Control registers (CR0, CR3, CR4, EFER). */
#define HMSVM_VMCB_CLEAN_CRX                    RT_BIT(5)
/** Debug registers (DR6, DR7). */
#define HMSVM_VMCB_CLEAN_DRX                    RT_BIT(6)
/** GDT, IDT limit and base. */
#define HMSVM_VMCB_CLEAN_DT                     RT_BIT(7)
/** Segment register: CS, SS, DS, ES limit and base. */
#define HMSVM_VMCB_CLEAN_SEG                    RT_BIT(8)
/** CR2.*/
#define HMSVM_VMCB_CLEAN_CR2                    RT_BIT(9)
/** Last-branch record (DbgCtlMsr, br_from, br_to, lastint_from, lastint_to) */
#define HMSVM_VMCB_CLEAN_LBR                    RT_BIT(10)
/** AVIC (AVIC APIC_BAR; AVIC APIC_BACKING_PAGE, AVIC
PHYSICAL_TABLE and AVIC LOGICAL_TABLE Pointers). */
#define HMSVM_VMCB_CLEAN_AVIC                   RT_BIT(11)
/** @} */

/**
 * MSRPM (MSR permission bitmap) read permissions (for guest RDMSR).
 */
typedef enum SVMMSREXITREAD
{
    /** Reading this MSR causes a VM-exit. */
    SVMMSREXIT_INTERCEPT_READ = 0xb,
    /** Reading this MSR does not cause a VM-exit. */
    SVMMSREXIT_PASSTHRU_READ
} VMXMSREXITREAD;

/**
 * MSRPM (MSR permission bitmap) write permissions (for guest WRMSR).
 */
typedef enum SVMMSREXITWRITE
{
    /** Writing to this MSR causes a VM-exit. */
    SVMMSREXIT_INTERCEPT_WRITE = 0xd,
    /** Writing to this MSR does not cause a VM-exit. */
    SVMMSREXIT_PASSTHRU_WRITE
} VMXMSREXITWRITE;


/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static void hmR0SvmSetMsrPermission(PVMCPU pVCpu, unsigned uMsr, SVMMSREXITREAD enmRead, SVMMSREXITWRITE enmWrite);


/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
/** Ring-0 memory object for the IO bitmap. */
RTR0MEMOBJ                  g_hMemObjIOBitmap = NIL_RTR0MEMOBJ;
/** Physical address of the IO bitmap. */
RTHCPHYS                    g_HCPhysIOBitmap  = 0;
/** Virtual address of the IO bitmap. */
R0PTRTYPE(void *)           g_pvIOBitmap      = NULL;


/**
 * Sets up and activates AMD-V on the current CPU.
 *
 * @returns VBox status code.
 * @param   pCpu            Pointer to the CPU info struct.
 * @param   pVM             Pointer to the VM (can be NULL after a resume!).
 * @param   pvCpuPage       Pointer to the global CPU page.
 * @param   HCPhysCpuPage   Physical address of the global CPU page.
 */
VMMR0DECL(int) SVMR0EnableCpu(PHMGLOBLCPUINFO pCpu, PVM pVM, void *pvCpuPage, RTHCPHYS HCPhysCpuPage, bool fEnabledByHost)
{
    AssertReturn(!fEnabledByHost, VERR_INVALID_PARAMETER);
    AssertReturn(   HCPhysCpuPage
                 && HCPhysCpuPage != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(pvCpuPage, VERR_INVALID_PARAMETER);

    /*
     * We must turn on AMD-V and setup the host state physical address, as those MSRs are per CPU.
     */
    uint64_t u64HostEfer = ASMRdMsr(MSR_K6_EFER);
    if (u64HostEfer & MSR_K6_EFER_SVME)
    {
        /* If the VBOX_HWVIRTEX_IGNORE_SVM_IN_USE is active, then we blindly use AMD-V. */
        if (   pVM
            && pVM->hm.s.svm.fIgnoreInUseError)
        {
            pCpu->fIgnoreAMDVInUseError = true;
        }

        if (!pCpu->fIgnoreAMDVInUseError)
            return VERR_SVM_IN_USE;
    }

    /* Turn on AMD-V in the EFER MSR. */
    ASMWrMsr(MSR_K6_EFER, u64HostEfer | MSR_K6_EFER_SVME);

    /* Write the physical page address where the CPU will store the host state while executing the VM. */
    ASMWrMsr(MSR_K8_VM_HSAVE_PA, HCPhysCpuPage);

    /*
     * Theoretically, other hypervisors may have used ASIDs, ideally we should flush all non-zero ASIDs
     * when enabling SVM. AMD doesn't have an SVM instruction to flush all ASIDs (flushing is done
     * upon VMRUN). Therefore, just set the fFlushAsidBeforeUse flag which instructs hmR0SvmSetupTLB()
     * to flush the TLB with before using a new ASID.
     */
    pCpu->fFlushAsidBeforeUse = true;

    /*
     * Ensure each VCPU scheduled on this CPU gets a new VPID on resume. See @bugref{6255}.
     */
    ++pCpu->cTlbFlushes;

    return VINF_SUCCESS;
}


/**
 * Deactivates AMD-V on the current CPU.
 *
 * @returns VBox status code.
 * @param   pCpu            Pointer to the CPU info struct.
 * @param   pvCpuPage       Pointer to the global CPU page.
 * @param   HCPhysCpuPage   Physical address of the global CPU page.
 */
VMMR0DECL(int) SVMR0DisableCpu(PHMGLOBLCPUINFO pCpu, void *pvCpuPage, RTHCPHYS HCPhysCpuPage)
{
    AssertReturn(   HCPhysCpuPage
                 && HCPhysCpuPage != NIL_RTHCPHYS, VERR_INVALID_PARAMETER);
    AssertReturn(pvCpuPage, VERR_INVALID_PARAMETER);
    NOREF(pCpu);

    /* Turn off AMD-V in the EFER MSR if AMD-V is active. */
    uint64_t u64HostEfer = ASMRdMsr(MSR_K6_EFER);
    if (u64HostEfer & MSR_K6_EFER_SVME)
    {
        ASMWrMsr(MSR_K6_EFER, u64HostEfer & ~MSR_K6_EFER_SVME);

        /* Invalidate host state physical address. */
        ASMWrMsr(MSR_K8_VM_HSAVE_PA, 0);
    }

    return VINF_SUCCESS;
}


/**
 * Does global AMD-V initialization (called during module initialization).
 *
 * @returns VBox status code.
 */
VMMR0DECL(int) SVMR0GlobalInit(void)
{
    /*
     * Allocate 12 KB for the IO bitmap. Since this is non-optional and we always intercept all IO accesses, it's done
     * once globally here instead of per-VM.
     */
    int rc = RTR0MemObjAllocCont(&g_hMemObjIOBitmap, 3 << PAGE_SHIFT, false /* fExecutable */);
    if (RT_FAILURE(rc))
        return rc;

    g_pvIOBitmap     = RTR0MemObjAddress(g_hMemObjIOBitmap);
    g_HCPhysIOBitmap = RTR0MemObjGetPagePhysAddr(g_hMemObjIOBitmap, 0 /* iPage */);

    /* Set all bits to intercept all IO accesses. */
    ASMMemFill32(pVM->hm.s.svm.pvIOBitmap, 3 << PAGE_SHIFT, UINT32_C(0xffffffff));
}


/**
 * Does global VT-x termination (called during module termination).
 */
VMMR0DECL(void) SVMR0GlobalTerm(void)
{
    if (g_hMemObjIOBitmap != NIL_RTR0MEMOBJ)
    {
        RTR0MemObjFree(pVM->hm.s.svm.hMemObjIOBitmap, false /* fFreeMappings */);
        g_pvIOBitmap      = NULL;
        g_HCPhysIOBitmap  = 0;
        g_hMemObjIOBitmap = NIL_RTR0MEMOBJ;
    }
}


/**
 * Frees any allocated per-VCPU structures for a VM.
 *
 * @param   pVM     Pointer to the VM.
 */
DECLINLINE(void) hmR0SvmFreeStructs(PVM pVM)
{
    for (uint32_t i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        AssertPtr(pVCpu);

        if (pVCpu->hm.s.svm.hMemObjVmcbHost != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hm.s.svm.hMemObjVmcbHost, false);
            pVCpu->hm.s.svm.pvVmcbHost       = 0;
            pVCpu->hm.s.svm.HCPhysVmcbHost   = 0;
            pVCpu->hm.s.svm.hMemObjVmcbHost  = NIL_RTR0MEMOBJ;
        }

        if (pVCpu->hm.s.svm.hMemObjVmcb != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hm.s.svm.hMemObjVmcb, false);
            pVCpu->hm.s.svm.pvVmcb           = 0;
            pVCpu->hm.s.svm.HCPhysVmcb       = 0;
            pVCpu->hm.s.svm.hMemObjVmcb      = NIL_RTR0MEMOBJ;
        }

        if (pVCpu->hm.s.svm.hMemObjMsrBitmap != NIL_RTR0MEMOBJ)
        {
            RTR0MemObjFree(pVCpu->hm.s.svm.hMemObjMsrBitmap, false);
            pVCpu->hm.s.svm.pvMsrBitmap      = 0;
            pVCpu->hm.s.svm.HCPhysMsrBitmap  = 0;
            pVCpu->hm.s.svm.hMemObjMsrBitmap = NIL_RTR0MEMOBJ;
        }
    }
}


/**
 * Does per-VM AMD-V initialization.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) SVMR0InitVM(PVM pVM)
{
    int rc = VERR_INTERNAL_ERROR_5;

    /*
     * Check for an AMD CPU erratum which requires us to flush the TLB before every world-switch.
     */
    uint32_t u32Family;
    uint32_t u32Model;
    uint32_t u32Stepping;
    if (HMAmdIsSubjectToErratum170(&u32Family, &u32Model, &u32Stepping))
    {
        Log4(("SVMR0InitVM: AMD cpu with erratum 170 family %#x model %#x stepping %#x\n", u32Family, u32Model, u32Stepping));
        pVM->hm.s.svm.fAlwaysFlushTLB = true;
    }

    /*
     * Initialize the R0 memory objects up-front so we can properly cleanup on allocation failures.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        pVCpu->hm.s.svm.hMemObjVmcbHost  = NIL_RTR0MEMOBJ;
        pVCpu->hm.s.svm.hMemObjVmcb      = NIL_RTR0MEMOBJ;
        pVCpu->hm.s.svm.hMemObjMsrBitmap = NIL_RTR0MEMOBJ;
    }

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        /*
         * Allocate one page for the host-context VM control block (VMCB). This is used for additional host-state (such as
         * FS, GS, Kernel GS Base, etc.) apart from the host-state save area specified in MSR_K8_VM_HSAVE_PA.
         */
        rc = RTR0MemObjAllocCont(&pVCpu->hm.s.svm.hMemObjVmcbHost, 1 << PAGE_SHIFT, false /* fExecutable */);
        if (RT_FAILURE(rc))
            goto failure_cleanup;

        pVCpu->hm.s.svm.pvVmcbHost     = RTR0MemObjAddress(pVCpu->hm.s.svm.hMemObjVmcbHost);
        pVCpu->hm.s.svm.HCPhysVmcbHost = RTR0MemObjGetPagePhysAddr(pVCpu->hm.s.svm.hMemObjVmcbHost, 0 /* iPage */);
        Assert(pVCpu->hm.s.svm.HCPhysVmcbHost < _4G);
        ASMMemZeroPage(pVCpu->hm.s.svm.pvVmcbHost);

        /*
         * Allocate one page for the guest-state VMCB.
         */
        rc = RTR0MemObjAllocCont(&pVCpu->hm.s.svm.hMemObjVmcb, 1 << PAGE_SHIFT, false /* fExecutable */);
        if (RT_FAILURE(rc))
            goto failure_cleanup;

        pVCpu->hm.s.svm.pvVmcb          = RTR0MemObjAddress(pVCpu->hm.s.svm.hMemObjVmcb);
        pVCpu->hm.s.svm.HCPhysVmcb      = RTR0MemObjGetPagePhysAddr(pVCpu->hm.s.svm.hMemObjVmcb, 0 /* iPage */);
        Assert(pVCpu->hm.s.svm.HCPhysVmcb < _4G);
        ASMMemZeroPage(pVCpu->hm.s.svm.pvVmcb);

        /*
         * Allocate two pages (8 KB) for the MSR permission bitmap. There doesn't seem to be a way to convince
         * SVM to not require one.
         */
        rc = RTR0MemObjAllocCont(&pVCpu->hm.s.svm.hMemObjMsrBitmap, 2 << PAGE_SHIFT, false /* fExecutable */);
        if (RT_FAILURE(rc))
            failure_cleanup;

        pVCpu->hm.s.svm.pvMsrBitmap     = RTR0MemObjAddress(pVCpu->hm.s.svm.hMemObjMsrBitmap);
        pVCpu->hm.s.svm.HCPhysMsrBitmap = RTR0MemObjGetPagePhysAddr(pVCpu->hm.s.svm.hMemObjMsrBitmap, 0 /* iPage */);
        /* Set all bits to intercept all MSR accesses (changed later on). */
        ASMMemFill32(pVCpu->hm.s.svm.pvMsrBitmap, 2 << PAGE_SHIFT, 0xffffffff);
    }

    return VINF_SUCCESS;

failure_cleanup:
    hmR0SvmFreeVMStructs(pVM);
    return rc;
}


/**
 * Does per-VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) SVMR0TermVM(PVM pVM)
{
    hmR0SvmFreeVMStructs(pVM);
    return VINF_SUCCESS;
}


/**
 * Sets the permission bits for the specified MSR in the MSRPM.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   uMsr       The MSR.
 * @param   fRead       Whether reading is allowed.
 * @param   fWrite      Whether writing is allowed.
 */
static void hmR0SvmSetMsrPermission(PVMCPU pVCpu, uint32_t uMsr, SVMMSREXITREAD enmRead, SVMMSREXITWRITE enmWrite)
{
    unsigned ulBit;
    uint8_t *pbMsrBitmap = (uint8_t *)pVCpu->hm.s.svm.pvMsrBitmap;

    /*
     * Layout:
     * Byte offset       MSR range
     * 0x000  - 0x7ff    0x00000000 - 0x00001fff
     * 0x800  - 0xfff    0xc0000000 - 0xc0001fff
     * 0x1000 - 0x17ff   0xc0010000 - 0xc0011fff
     * 0x1800 - 0x1fff           Reserved
     */
    if (uMsr <= 0x00001FFF)
    {
        /* Pentium-compatible MSRs */
        ulBit    = uMsr * 2;
    }
    else if (   uMsr >= 0xC0000000
             && uMsr <= 0xC0001FFF)
    {
        /* AMD Sixth Generation x86 Processor MSRs and SYSCALL */
        ulBit = (uMsr - 0xC0000000) * 2;
        pbMsrBitmap += 0x800;
    }
    else if (   uMsr >= 0xC0010000
             && uMsr <= 0xC0011FFF)
    {
        /* AMD Seventh and Eighth Generation Processor MSRs */
        ulBit = (uMsr - 0xC0001000) * 2;
        pbMsrBitmap += 0x1000;
    }
    else
    {
        AssertFailed();
        return;
    }

    Assert(ulBit < 0x3fff /* 16 * 1024 - 1 */);
    if (enmRead == SVMMSREXIT_INTERCEPT_READ)
        ASMBitSet(pbMsrBitmap, ulBit);
    else
        ASMBitClear(pbMsrBitmap, ulBit);

    if (enmWrite == SVMMSREXIT_INTERCEPT_WRITE)
        ASMBitSet(pbMsrBitmap, ulBit + 1);
    else
        ASMBitClear(pbMsrBitmap, ulBit + 1);

    pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_IOPM_MSRPM;
}


/**
 * Sets up AMD-V for the specified VM.
 * This function is only called once per-VM during initalization.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) SVMR0SetupVM(PVM pVM)
{
    int rc = VINF_SUCCESS;

    AssertReturn(pVM, VERR_INVALID_PARAMETER);
    Assert(pVM->hm.s.svm.fSupported);

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU   pVCpu = &pVM->aCpus[i];
        PSVMVMCB pVmcb = (PSVMVMCB)pVM->aCpus[i].hm.s.svm.pvVmcb;

        AssertMsgReturn(pVmcb, ("Invalid pVmcb\n"), VERR_SVM_INVALID_PVMCB);

        /* Trap exceptions unconditionally (debug purposes). */
#ifdef HMSVM_ALWAYS_TRAP_PF
        pVmcb->ctrl.u32InterceptException |=   RT_BIT(X86_XCPT_PF);
#endif
#ifdef HMSVM_ALWAYS_TRAP_ALL_XCPTS
        pVmcb->ctrl.u32InterceptException |=   RT_BIT(X86_XCPT_BP)
                                             | RT_BIT(X86_XCPT_DB)
                                             | RT_BIT(X86_XCPT_DE)
                                             | RT_BIT(X86_XCPT_NM)
                                             | RT_BIT(X86_XCPT_UD)
                                             | RT_BIT(X86_XCPT_NP)
                                             | RT_BIT(X86_XCPT_SS)
                                             | RT_BIT(X86_XCPT_GP)
                                             | RT_BIT(X86_XCPT_PF)
                                             | RT_BIT(X86_XCPT_MF);
#endif

        /* Set up unconditional intercepts and conditions. */
        pVmcb->ctrl.u32InterceptCtrl1 =   SVM_CTRL1_INTERCEPT_INTR          /* External interrupt causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_VINTR         /* When guest enabled interrupts cause a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_NMI           /* Non-Maskable Interrupts causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_SMI           /* System Management Interrupt cause a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_INIT          /* INIT signal causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_RDPMC         /* RDPMC causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_CPUID         /* CPUID causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_RSM           /* RSM causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_HLT           /* HLT causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_INOUT_BITMAP  /* Use the IOPM to cause IOIO VM-exits. */
                                        | SVM_CTRL1_INTERCEPT_MSR_SHADOW    /* MSR access not covered by MSRPM causes a VM-exit.*/
                                        | SVM_CTRL1_INTERCEPT_INVLPGA       /* INVLPGA causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_SHUTDOWN      /* Shutdown events causes a VM-exit. */
                                        | SVM_CTRL1_INTERCEPT_FERR_FREEZE;  /* Intercept "freezing" during legacy FPU handling. */

        pVmcb->ctrl.u32InterceptCtrl2 =   SVM_CTRL2_INTERCEPT_VMRUN         /* VMRUN causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_VMMCALL       /* VMMCALL causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_VMLOAD        /* VMLOAD causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_VMSAVE        /* VMSAVE causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_STGI          /* STGI causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_CLGI          /* CLGI causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_SKINIT        /* SKINIT causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_WBINVD        /* WBINVD causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_MONITOR       /* MONITOR causes a VM-exit. */
                                        | SVM_CTRL2_INTERCEPT_MWAIT_UNCOND; /* MWAIT causes a VM-exit. */

        /* CR0, CR4 reads must be intercepted, our shadow values are not necessarily the same as the guest's. */
        pVmcb->ctrl.u16InterceptRdCRx = RT_BIT(0) | RT_BIT(4);

        /* CR0, CR4 writes must be intercepted for obvious reasons. */
        pVmcb->ctrl.u16InterceptWrCRx = RT_BIT(0) | RT_BIT(4);

        /* Intercept all DRx reads and writes by default. Changed later on. */
        pVmcb->ctrl.u16InterceptRdDRx = 0xffff;
        pVmcb->ctrl.u16InterceptWrDRx = 0xffff;

        /* Virtualize masking of INTR interrupts. (reads/writes from/to CR8 go to the V_TPR register) */
        pVmcb->ctrl.IntCtrl.n.u1VIrqMasking = 1;

        /* Ignore the priority in the TPR; just deliver it to the guest when we tell it to. */
        pVmcb->ctrl.IntCtrl.n.u1IgnoreTPR   = 1;

        /* Set IO and MSR bitmap permission bitmap physical addresses. */
        pVmcb->ctrl.u64IOPMPhysAddr  = g_HCPhysIOBitmap;
        pVmcb->ctrl.u64MSRPMPhysAddr = pVCpu->hm.s.svm.HCPhysMsrBitmap;

        /* No LBR virtualization. */
        pVmcb->ctrl.u64LBRVirt = 0;

        /* Initially set all VMCB clean bits to 0 indicating that everything should be loaded from memory. */
        pVmcb->u64VmcbCleanBits = 0;

        /* The ASID must start at 1; the host uses 0. */
        pVmcb->ctrl.TLBCtrl.n.u32ASID = 1;

        /*
         * Setup the PAT MSR (applicable for Nested Paging only).
         * The default value should be 0x0007040600070406ULL, but we want to treat all guest memory as WB,
         * so choose type 6 for all PAT slots.
         */
        pVmcb->guest.u64GPAT = UINT64_C(0x0006060606060606);

        /* Without Nested Paging, we need additionally intercepts. */
        if (!pVM->hm.s.fNestedPaging)
        {
            /* CR3 reads/writes must be intercepted; our shadow values differ from the guest values. */
            pVmcb->ctrl.u16InterceptRdCRx |= RT_BIT(3);
            pVmcb->ctrl.u16InterceptWrCRx |= RT_BIT(3);

            /* Intercept INVLPG and task switches (may change CR3, EFLAGS, LDT). */
            pVmcb->ctrl.u32InterceptCtrl1 |=   SVM_CTRL1_INTERCEPT_INVLPG
                                             | SVM_CTRL1_INTERCEPT_TASK_SWITCH;

            /* Page faults must be intercepted to implement shadow paging. */
            pVmcb->ctrl.u32InterceptException |= RT_BIT(X86_XCPT_PF);
        }

        /*
         * The following MSRs are saved/restored automatically during the world-switch.
         * Don't intercept guest read/write accesses to these MSRs.
         */
        hmR0SvmSetMsrPermission(pVCpu, MSR_K8_LSTAR, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_K8_CSTAR, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_K6_STAR, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_K8_SF_MASK, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_K8_FS_BASE, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_K8_GS_BASE, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_K8_KERNEL_GS_BASE, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_CS, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_ESP, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
        hmR0SvmSetMsrPermission(pVCpu, MSR_IA32_SYSENTER_EIP, SVMMSREXIT_PASSTHRU_READ, SVMMSREXIT_PASSTHRU_WRITE);
    }

    return rc;
}


/**
 * Flushes the appropriate tagged-TLB entries.
 *
 * @param    pVM        Pointer to the VM.
 * @param    pVCpu      Pointer to the VMCPU.
 */
static void hmR0SvmFlushTaggedTlb(PVMCPU pVCpu)
{
    PVM pVM              = pVCpu->CTX_SUFF(pVM);
    PSVMVMCB pVmcb       = (PSVMVMCB)pVCpu->hm.s.svm.pvVmcb;
    PHMGLOBLCPUINFO pCpu = HMR0GetCurrentCpu();

    /*
     * Force a TLB flush for the first world switch if the current CPU differs from the one we ran on last.
     * This can happen both for start & resume due to long jumps back to ring-3.
     * If the TLB flush count changed, another VM (VCPU rather) has hit the ASID limit while flushing the TLB,
     * so we cannot reuse the ASIDs without flushing.
     */
    bool fNewAsid = false;
    if (   pVCpu->hm.s.idLastCpu   != pCpu->idCpu
        || pVCpu->hm.s.cTlbFlushes != pCpu->cTlbFlushes)
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlbWorldSwitch);
        pVCpu->hm.s.fForceTLBFlush = true;
        fNewAsid = true;
    }

    /* Set TLB flush state as checked until we return from the world switch. */
    ASMAtomicWriteBool(&pVCpu->hm.s.fCheckedTLBFlush, true);

    /* Check for explicit TLB shootdowns. */
    if (VMCPU_FF_TEST_AND_CLEAR(pVCpu, VMCPU_FF_TLB_FLUSH))
    {
        pVCpu->hm.s.fForceTLBFlush = true;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushTlb);
    }

    pVCpu->hm.s.idLastCpu = pCpu->idCpu;
    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_NOTHING;

    if (pVM->hm.s.svm.fAlwaysFlushTLB)
    {
        /*
         * This is the AMD erratum 170. We need to flush the entire TLB for each world switch. Sad.
         */
        pCpu->uCurrentAsid               = 1;
        pVCpu->hm.s.uCurrentAsid         = 1;
        pVCpu->hm.s.cTlbFlushes          = pCpu->cTlbFlushes;
        pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;
    }
    else if (pVCpu->hm.s.fForceTLBFlush)
    {
        if (fNewAsid)
        {
            ++pCpu->uCurrentAsid;
            bool fHitASIDLimit = false;
            if (pCpu->uCurrentAsid >= pVM->hm.s.uMaxAsid)
            {
                pCpu->uCurrentAsid        = 1;      /* Wraparound at 1; host uses 0 */
                pCpu->cTlbFlushes++;                /* All VCPUs that run on this host CPU must use a new VPID. */
                fHitASIDLimit             = true;

                if (pVM->hm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_FLUSH_BY_ASID)
                {
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_SINGLE_CONTEXT;
                    pCpu->fFlushAsidBeforeUse = true;
                }
                else
                {
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;
                    pCpu->fFlushAsidBeforeUse = false;
                }
            }

            if (   !fHitASIDLimit
                && pCpu->fFlushAsidBeforeUse)
            {
                if (pVM->hm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_FLUSH_BY_ASID)
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_SINGLE_CONTEXT;
                else
                {
                    pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;
                    pCpu->fFlushAsidBeforeUse = false;
                }
            }

            pVCpu->hm.s.uCurrentAsid = pCpu->uCurrentAsid;
            pVCpu->hm.s.cTlbFlushes  = pCpu->cTlbFlushes;
        }
        else
        {
            if (pVM->hm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_FLUSH_BY_ASID)
                pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_SINGLE_CONTEXT;
            else
                pVmcb->ctrl.TLBCtrl.n.u8TLBFlush = SVM_TLB_FLUSH_ENTIRE;
        }

        pVCpu->hm.s.fForceTLBFlush = false;
    }
    else
    {
        /** @todo We never set VMCPU_FF_TLB_SHOOTDOWN anywhere so this path should
         *        not be executed. See hmQueueInvlPage() where it is commented
         *        out. Support individual entry flushing someday. */
        if (VMCPU_FF_IS_PENDING(pVCpu, VMCPU_FF_TLB_SHOOTDOWN))
        {
            /* Deal with pending TLB shootdown actions which were queued when we were not executing code. */
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTlbShootdown);
            for (uint32_t i = 0; i < pVCpu->hm.s.TlbShootdown.cPages; i++)
                SVMR0InvlpgA(pVCpu->hm.s.TlbShootdown.aPages[i], pVmcb->ctrl.TLBCtrl.n.u32ASID);
        }
    }

    pVCpu->hm.s.TlbShootdown.cPages = 0;
    VMCPU_FF_CLEAR(pVCpu, VMCPU_FF_TLB_SHOOTDOWN);

    /* Update VMCB with the ASID. */
    if (pVmcb->ctrl.TLBCtrl.n.u32ASID != pVCpu->hm.s.uCurrentAsid)
    {
        pVmcb->ctrl.TLBCtrl.n.u32ASID = pVCpu->hm.s.uCurrentAsid;
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_ASID;
    }

    AssertMsg(pVCpu->hm.s.cTlbFlushes == pCpu->cTlbFlushes,
              ("Flush count mismatch for cpu %d (%x vs %x)\n", pCpu->idCpu, pVCpu->hm.s.cTlbFlushes, pCpu->cTlbFlushes));
    AssertMsg(pCpu->uCurrentAsid >= 1 && pCpu->uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d uCurrentAsid = %x\n", pCpu->idCpu, pCpu->uCurrentAsid));
    AssertMsg(pVCpu->hm.s.uCurrentAsid >= 1 && pVCpu->hm.s.uCurrentAsid < pVM->hm.s.uMaxAsid,
              ("cpu%d VM uCurrentAsid = %x\n", pCpu->idCpu, pVCpu->hm.s.uCurrentAsid));

#ifdef VBOX_WITH_STATISTICS
    if (pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_NOTHING)
        STAM_COUNTER_INC(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch);
    else if (   pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_SINGLE_CONTEXT
             || pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_SINGLE_CONTEXT_RETAIN_GLOBALS)
    {
        STAM_COUNTER_INC(&pVCpu->hm.s.StatFlushAsid);
    }
    else
        Assert(pVmcb->ctrl.TLBCtrl.n.u8TLBFlush == SVM_TLB_FLUSH_ENTIRE)
#endif
}


/** @name 64-bit guest on 32-bit host OS helper functions.
 *
 * The host CPU is still 64-bit capable but the host OS is running in 32-bit
 * mode (code segment, paging). These wrappers/helpers perform the necessary
 * bits for the 32->64 switcher.
 *
 * @{ */
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
/**
 * Prepares for and executes VMRUN (64-bit guests on a 32-bit host).
 *
 * @returns VBox status code.
 * @param   HCPhysVmcbHost  Physical address of host VMCB.
 * @param   HCPhysVmcb      Physical address of the VMCB.
 * @param   pCtx            Pointer to the guest-CPU context.
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU.
 */
DECLASM(int) SVMR0VMSwitcherRun64(RTHCPHYS HCPhysVmcbHost, RTHCPHYS HCPhysVmcb, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu)
{
    uint32_t aParam[4];
    aParam[0] = (uint32_t)(HCPhysVmcbHost);             /* Param 1: HCPhysVmcbHost - Lo. */
    aParam[1] = (uint32_t)(HCPhysVmcbHost >> 32);       /* Param 1: HCPhysVmcbHost - Hi. */
    aParam[2] = (uint32_t)(HCPhysVmcb);                 /* Param 2: HCPhysVmcb - Lo. */
    aParam[3] = (uint32_t)(HCPhysVmcb >> 32);           /* Param 2: HCPhysVmcb - Hi. */

    return SVMR0Execute64BitsHandler(pVM, pVCpu, pCtx, HM64ON32OP_SVMRCVMRun64, 4, &aParam[0]);
}


/**
 * Executes the specified VMRUN handler in 64-bit mode.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 * @param   enmOp       The operation to perform.
 * @param   cbParam     Number of parameters.
 * @param   paParam     Array of 32-bit parameters.
 */
VMMR0DECL(int) SVMR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, HM64ON32OP enmOp, uint32_t cbParam,
                                         uint32_t *paParam)
{
    AssertReturn(pVM->hm.s.pfnHost32ToGuest64R0, VERR_HM_NO_32_TO_64_SWITCHER);
    Assert(enmOp > HM64ON32OP_INVALID && enmOp < HM64ON32OP_END);

    /* Disable interrupts. */
    RTHCUINTREG uOldEFlags = ASMIntDisableFlags();

#ifdef VBOX_WITH_VMMR0_DISABLE_LAPIC_NMI
    RTCPUID idHostCpu = RTMpCpuId();
    CPUMR0SetLApic(pVM, idHostCpu);
#endif

    CPUMSetHyperESP(pVCpu, VMMGetStackRC(pVCpu));
    CPUMSetHyperEIP(pVCpu, enmOp);
    for (int i = (int)cbParam - 1; i >= 0; i--)
        CPUMPushHyper(pVCpu, paParam[i]);

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatWorldSwitch3264, z);
    /* Call the switcher. */
    int rc = pVM->hm.s.pfnHost32ToGuest64R0(pVM, RT_OFFSETOF(VM, aCpus[pVCpu->idCpu].cpum) - RT_OFFSETOF(VM, cpum));
    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatWorldSwitch3264, z);

    /* Restore interrupts. */
    ASMSetFlags(uOldEFlags);
    return rc;
}

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) */
/** @} */


/**
 * Saves the host state.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(int) SVMR0SaveHostState(PVM pVM, PVMCPU pVCpu)
{
    NOREF(pVM);
    NOREF(pVCpu);
    /* Nothing to do here. AMD-V does this for us automatically during the world-switch. */
    return VINF_SUCCESS;
}


DECLINLINE(void) hmR0SvmAddXcptIntercept(uint32_t u32Xcpt)
{
    if (!(pVmcb->ctrl.u32InterceptException & RT_BIT(u32Xcpt))
    {
        pVmcb->ctrl.u32InterceptException |= RT_BIT(u32Xcpt);
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
    }
}

DECLINLINE(void) hmR0SvmRemoveXcptIntercept(uint32_t u32Xcpt)
{
#ifndef HMVMX_ALWAYS_TRAP_ALL_XCPTS
    if (pVmcb->ctrl.u32InterceptException & RT_BIT(u32Xcpt))
    {
        pVmcb->ctrl.u32InterceptException &= ~RT_BIT(u32Xcpt);
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
    }
#endif
}


/**
 * Loads the guest control registers (CR0, CR2, CR3, CR4) into the VMCB.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmLoadGuestControlRegs(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /*
     * Guest CR0.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR0)
    {
        uint64_t u64GuestCR0 = pCtx->cr0;

        /* Always enable caching. */
        u64GuestCR0 &= ~(X86_CR0_CD | X86_CR0_NW);

        /*
         * When Nested Paging is not available use shadow page tables and intercept #PFs (latter done in SVMR0SetupVM()).
         */
        if (!pVM->hm.s.fNestedPaging)
        {
            u64GuestCR0 |= X86_CR0_PG;  /* When Nested Paging is not available, use shadow page tables. */
            u64GuestCR0 |= X86_CR0_WP;  /* Guest CPL 0 writes to its read-only pages should cause a #PF VM-exit. */
        }

        /*
         * Guest FPU bits.
         */
        bool fInterceptNM = false;
        bool fInterceptMF = false;
        u64GuestCR0 |= X86_CR0_NE;         /* Use internal x87 FPU exceptions handling rather than external interrupts. */
        if (CPUMIsGuestFPUStateActive(pVCpu))
        {
            /* Catch floating point exceptions if we need to report them to the guest in a different way. */
            if (!(u64GuestCR0 & X86_CR0_NE))
            {
                Log4(("hmR0SvmLoadGuestControlRegs: Intercepting Guest CR0.MP Old-style FPU handling!!!\n"));
                pVmcb->ctrl.u32InterceptException |= RT_BIT(X86_XCPT_MF);
                fInterceptMF = true;
            }
        }
        else
        {
            fInterceptNM = true;           /* Guest FPU inactive, VM-exit on #NM for lazy FPU loading. */
            u32GuestCR0 |=  X86_CR0_TS     /* Guest can task switch quickly and do lazy FPU syncing. */
                          | X86_CR0_MP;    /* FWAIT/WAIT should not ignore CR0.TS and should generate #NM. */
        }

        /*
         * Update the exception intercept bitmap.
         */
        if (fInterceptNM)
            hmR0SvmAddXcptIntercept(X86_XCPT_NM);
        else
            hmR0SvmRemoveXcptIntercept(X86_XCPT_NM);

        if (fInterceptMF)
            hmR0SvmAddXcptIntercept(X86_XCPT_MF);
        else
            hmR0SvmRemoveXcptIntercept(X86_XCPT_MF);

        pVmcb->guest.u64CR0 = u64GuestCR0;
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CR2;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR0;
    }

    /*
     * Guest CR2.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR2)
    {
        pVmcb->guest.u64CR2 = pCtx->cr2;
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CR2;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR2;
    }

    /*
     * Guest CR3.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR3)
    {
        if (pVM->hm.s.fNestedPaging)
        {
            PGMMODE enmShwPagingMode;
#if HC_ARCH_BITS == 32
            if (CPUMIsGuestInLongModeEx(pCtx))
                enmShwPagingMode = PGMMODE_AMD64_NX;
            else
#endif
                enmShwPagingMode = PGMGetHostMode(pVM);

            pVmcb->ctrl.u64NestedPagingCR3  = PGMGetNestedCR3(pVCpu, enmShwPagingMode);
            pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_NP;
            Assert(pVmcb->ctrl.u64NestedPagingCR3);
            pVmcb->guest.u64CR3 = pCtx->cr3;
        }
        else
            pVmcb->guest.u64CR3 = PGMGetHyperCR3(pVCpu);

        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CR2;
        pVCpu->hm.s.fContextUseFlags &= HM_CHANGED_GUEST_CR3;
    }

    /*
     * Guest CR4.
     */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_CR4)
    {
        uint64_t u64GuestCR4 = pCtx->cr4;
        if (!pVM->hm.s.fNestedPaging)
        {
            switch (pVCpu->hm.s.enmShadowMode)
            {
                case PGMMODE_REAL:
                case PGMMODE_PROTECTED:     /* Protected mode, no paging. */
                    AssertFailed();
                    return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;

                case PGMMODE_32_BIT:        /* 32-bit paging. */
                    u64GuestCR4 &= ~X86_CR4_PAE;
                    break;

                case PGMMODE_PAE:           /* PAE paging. */
                case PGMMODE_PAE_NX:        /* PAE paging with NX enabled. */
                    /** Must use PAE paging as we could use physical memory > 4 GB */
                    u64GuestCR4 |= X86_CR4_PAE;
                    break;

                case PGMMODE_AMD64:         /* 64-bit AMD paging (long mode). */
                case PGMMODE_AMD64_NX:      /* 64-bit AMD paging (long mode) with NX enabled. */
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                    break;
#else
                    AssertFailed();
                    return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif

                default:                    /* shut up gcc */
                    AssertFailed();
                    return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
            }
        }

        pVmcb->guest.u64CR4 = u64GuestCR4;
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_CR2;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_CR4;
    }

    return VINF_SUCCESS;
}

/**
 * Loads the guest segment registers into the VMCB.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmLoadGuestSegmentRegs(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /* Guest Segment registers: CS, SS, DS, ES, FS, GS. */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_SEGMENT_REGS)
    {
        HMSVM_LOAD_SEG_REG(CS, cs);
        HMSVM_LOAD_SEG_REG(SS, cs);
        HMSVM_LOAD_SEG_REG(DS, cs);
        HMSVM_LOAD_SEG_REG(ES, cs);
        HMSVM_LOAD_SEG_REG(FS, cs);
        HMSVM_LOAD_SEG_REG(GS, cs);

        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_SEG;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_SEGMENT_REGS;
    }

    /* Guest TR. */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_TR)
    {
        HMSVM_LOAD_SEG_REG(TR, tr);
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_TR;
    }

    /* Guest LDTR. */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_LDTR)
    {
        HMSVM_LOAD_SEG_REG(LDTR, ldtr);
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_LDTR;
    }

    /* Guest GDTR. */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_GDTR)
    {
        pVmcb->guest.GDTR.u32Limit = pCtx->gdtr.cbGdt;
        pVmcb->guest.GDTR.u64Base  = pCtx->gdtr.pGdt;
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DT;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_GDTR;
    }

    /* Guest IDTR. */
    if (pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_IDTR)
    {
        pVmcb->guest.IDTR.u32Limit = pCtx->idtr.cbIdt;
        pVmcb->guest.IDTR.u64Base  = pCtx->idtr.pIdt;
        pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DT;
        pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_IDTR;
    }
}


/**
 * Loads the guest MSRs into the VMCB.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmLoadGuestMsrs(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    /* Guest Sysenter MSRs. */
    pVmcb->guest.u64SysEnterCS  = pCtx->SysEnter.cs;
    pVmcb->guest.u64SysEnterEIP = pCtx->SysEnter.eip;
    pVmcb->guest.u64SysEnterESP = pCtx->SysEnter.esp;

    /* Guest EFER MSR. */
    /* AMD-V requires guest EFER.SVME to be set. Weird.
       See AMD spec. 15.5.1 "Basic Operation" | "Canonicalization and Consistency Checks". */
    pVmcb->guest.u64EFER = pCtx->msrEFER | MSR_K6_EFER_SVME;

    /* 64-bit MSRs. */
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
        pVmcb->guest.FS.u64Base      = pCtx->fs.u64Base;
        pVmcb->guest.GS.u64Base      = pCtx->gs.u64Base;
    }
    else
    {
        /* If the guest isn't in 64-bit mode, clear MSR_K6_LME bit from guest EFER otherwise AMD-V expects amd64 shadow paging. */
        pVmcb->guest.u64EFER &= ~MSR_K6_EFER_LME;
    }

    /** @todo The following are used in 64-bit only (SYSCALL/SYSRET) but they might
     *        be writable in 32-bit mode. Clarify with AMD spec. */
    pVmcb->guest.u64STAR         = pCtx->msrSTAR;
    pVmcb->guest.u64LSTAR        = pCtx->msrLSTAR;
    pVmcb->guest.u64CSTAR        = pCtx->msrCSTAR;
    pVmcb->guest.u64SFMASK       = pCtx->msrSFMASK;
    pVmcb->guest.u64KernelGSBase = pCtx->msrKERNELGSBASE;
}

/**
 * Loads the guest debug registers into the VMCB.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmLoadGuestDebugRegs(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (!(pVCpu->hm.s.fContextUseFlags & HM_CHANGED_GUEST_DEBUG))
        return;

    /** @todo Turn these into assertions if possible. */
    pCtx->dr[6] |= X86_DR6_INIT_VAL;                                          /* Set reserved bits to 1. */
    pCtx->dr[6] &= ~RT_BIT(12);                                               /* MBZ. */

    pCtx->dr[7] &= 0xffffffff;                                                /* Upper 32 bits MBZ. */
    pCtx->dr[7] &= ~(RT_BIT(11) | RT_BIT(12) | RT_BIT(14) | RT_BIT(15));      /* MBZ. */
    pCtx->dr[7] |= 0x400;                                                     /* MB1. */

    /* Update DR6, DR7 with the guest values. */
    pVmcb->guest.u64DR7 = pCtx->dr[7];
    pVmcb->guest.u64DR6 = pCtx->dr[6];
    pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DRX;

    bool fInterceptDB     = false;
    bool fInterceptMovDRx = false;
    if (DBGFIsStepping(pVCpu))
    {
        /* AMD-V doesn't have any monitor-trap flag equivalent. Instead, enable tracing in the guest and trap #DB. */
        pVmcb->guest.u64RFlags |= X86_EFL_TF;
        fInterceptDB = true;
    }

    if (CPUMGetHyperDR7(pVCpu) & (X86_DR7_ENABLED_MASK | X86_DR7_GD))
    {
        if (!CPUMIsHyperDebugStateActive(pVCpu))
        {
            rc = CPUMR0LoadHyperDebugState(pVM, pVCpu, pMixedCtx, true /* include DR6 */);
            AssertRC(rc);

            /* Update DR6, DR7 with the hypervisor values. */
            pVmcb->guest.u64DR7 = CPUMGetHyperDR7(pVCpu);
            pVmcb->guest.u64DR6 = CPUMGetHyperDR6(pVCpu);
            pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_DRX;
        }
        Assert(CPUMIsHyperDebugStateActive(pVCpu));
        fInterceptMovDRx = true;
    }
    else if (pMixedCtx->dr[7] & (X86_DR7_ENABLED_MASK | X86_DR7_GD))
    {
        if (!CPUMIsGuestDebugStateActive(pVCpu))
        {
            rc = CPUMR0LoadGuestDebugState(pVM, pVCpu, pMixedCtx, true /* include DR6 */);
            AssertRC(rc);
            STAM_COUNTER_INC(&pVCpu->hm.s.StatDRxArmed);
        }
        Assert(CPUMIsGuestDebugStateActive(pVCpu));
        Assert(fInterceptMovDRx == false);
    }
    else if (!CPUMIsGuestDebugStateActive(pVCpu))
    {
        /* For the first time we would need to intercept MOV DRx accesses even when the guest debug registers aren't loaded. */
        fInterceptMovDRx = true;
    }

    if (fInterceptDB)
        hmR0SvmAddXcptIntercept(X86_XCPT_DB);
    else
        hmR0SvmRemoveXcptIntercept(X86_XCPT_DB);

    if (fInterceptMovDRx)
    {
        if (   pVmcb->ctrl.u16InterceptRdDRx != 0xffff
            || pVmcb->ctrl.u16InterceptWrDRx != 0xffff)
        {
            pVmcb->ctrl.u16InterceptRdDRx = 0xffff;
            pVmcb->ctrl.u16InterceptWrDRx = 0xffff;
            pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
    }
    else
    {
        if (   pVmcb->ctrl.u16InterceptRdDRx
            || pVmcb->ctrl.u16InterceptWrDRx)
        {
            pVmcb->ctrl.u16InterceptRdDRx = 0;
            pVmcb->ctrl.u16InterceptWrDRx = 0;
            pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
        }
    }

    pVCpu->hm.s.fContextUseFlags &= ~HM_CHANGED_GUEST_DEBUG;
}

/**
 * Sets up the appropriate function to run guest code.
 *
 * @returns VBox status code.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pMixedCtx   Pointer to the guest-CPU context. The data may be
 *                      out-of-sync. Make sure to update the required fields
 *                      before using them.
 *
 * @remarks No-long-jump zone!!!
 */
static int hmR0SvmSetupVMRunHandler(PVMCPU pVCpu, PCPUMCTX pCtx)
{
    if (CPUMIsGuestInLongModeEx(pCtx))
    {
#ifndef VBOX_ENABLE_64_BITS_GUESTS
        return VERR_PGM_UNSUPPORTED_SHADOW_PAGING_MODE;
#endif
        Assert(pVCpu->CTX_SUFF(pVM)->hm.s.fAllow64BitGuests);    /* Guaranteed by hmR3InitFinalizeR0(). */
#if HC_ARCH_BITS == 32 && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        /* 32-bit host. We need to switch to 64-bit before running the 64-bit guest. */
        pVCpu->hm.s.svm.pfnVMRun = SVMR0VMSwitcherRun64;
#else
        /* 64-bit host or hybrid host. */
        pVCpu->hm.s.svm.pfnVMRun = SVMR0VMRun64;
#endif
    }
    else
    {
        /* Guest is not in long mode, use the 32-bit handler. */
        pVCpu->hm.s.svm.pfnVMRun = SVMR0VMRun;
    }
    return VINF_SUCCESS;
}


/**
 * Loads the guest state.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest-CPU context.
 *
 * @remarks No-long-jump zone!!!
 */
VMMR0DECL(int) SVMR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    AssertPtr(pVM);
    AssertPtr(pVCpu);
    AssertPtr(pMixedCtx);
    Assert(!RTThreadPreemptIsEnabled(NIL_RTTHREAD));

    PSVMVMCB pVmcb = (PSVMVMCB)pVCpu->hm.s.svm.pvVmcb;
    AssertMsgReturn(pVmcb, ("Invalid pVmcb\n"), VERR_SVM_INVALID_PVMCB);

    STAM_PROFILE_ADV_START(&pVCpu->hm.s.StatLoadGuestState, x);

    int rc = hmR0SvmLoadGuestControlRegs(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0SvmLoadGuestControlRegs! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    hmR0SvmLoadGuestSegmentRegs(pVCpu, pCtx);
    hmR0SvmLoadGuestMsrs(pVCpu, pCtx);

    pVmcb->guest.u64RIP    = pCtx->rip;
    pVmcb->guest.u64RSP    = pCtx->rsp;
    pVmcb->guest.u64RFlags = pCtx->eflags.u32;
    pVmcb->guest.u8CPL     = pCtx->ss.Attr.n.u2Dpl;
    pVmcb->guest.u64RAX    = pCtx->rax;

    /* hmR0SvmLoadGuestDebugRegs() must be called -after- updating guest RFLAGS as the RFLAGS may need to be changed. */
    hmR0SvmLoadGuestDebugRegs(pVCpu, pCtx);

    rc = hmR0SvmSetupVMRunHandler(pVCpu, pMixedCtx);
    AssertLogRelMsgRCReturn(rc, ("hmR0SvmSetupVMRunHandler! rc=%Rrc (pVM=%p pVCpu=%p)\n", rc, pVM, pVCpu), rc);

    /* Clear any unused and reserved bits. */
    pVCpu->hm.s.fContextUseFlags &= ~(  HM_CHANGED_GUEST_SYSENTER_CS_MSR
                                      | HM_CHANGED_GUEST_SYSENTER_EIP_MSR
                                      | HM_CHANGED_GUEST_SYSENTER_ESP_MSR);

    AssertMsg(!pVCpu->hm.s.fContextUseFlags,
             ("Missed updating flags while loading guest state. pVM=%p pVCpu=%p fContextUseFlags=%#RX32\n",
              pVM, pVCpu, pVCpu->hm.s.fContextUseFlags));

    STAM_PROFILE_ADV_STOP(&pVCpu->hm.s.StatLoadGuestState, x);

    return rc;
}


/**
 * Sets up the usage of TSC offsetting for the VCPU.
 *
 * @param   pVCpu       Pointer to the VMCPU.
 *
 * @remarks No-long-jump zone!!!
 */
static void hmR0SvmSetupTscOffsetting(PVMCPU pVCpu)
{
    PSVMVMCB pVmcb = pVCpu->hm.s.svm.pvVmcb;
    if (TMCpuTickCanUseRealTSC(pVCpu, &pVmcb->ctrl.u64TSCOffset))
    {
        uint64_t u64CurTSC = ASMReadTSC();
        if (u64CurTSC + pVmcb->ctrl.u64TSCOffset > TMCpuTickGetLastSeen(pVCpu))
        {
            pVmcb->ctrl.u32InterceptCtrl1 &= ~SVM_CTRL1_INTERCEPT_RDTSC;
            pVmcb->ctrl.u32InterceptCtrl2 &= ~SVM_CTRL2_INTERCEPT_RDTSCP;
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTscOffset);
        }
        else
        {
            pVmcb->ctrl.u32InterceptCtrl1 |= SVM_CTRL1_INTERCEPT_RDTSC;
            pVmcb->ctrl.u32InterceptCtrl2 |= SVM_CTRL2_INTERCEPT_RDTSCP;
            STAM_COUNTER_INC(&pVCpu->hm.s.StatTscInterceptOverFlow);
        }
    }
    else
    {
        pVmcb->ctrl.u32InterceptCtrl1 |= SVM_CTRL1_INTERCEPT_RDTSC;
        pVmcb->ctrl.u32InterceptCtrl2 |= SVM_CTRL2_INTERCEPT_RDTSCP;
        STAM_COUNTER_INC(&pVCpu->hm.s.StatTscIntercept);
    }

    pVmcb->u64VmcbCleanBits &= ~HMSVM_VMCB_CLEAN_INTERCEPTS;
}

