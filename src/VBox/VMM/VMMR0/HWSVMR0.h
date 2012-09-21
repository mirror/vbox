/* $Id$ */
/** @file
 * HM SVM (AMD-V) - Internal header file.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef ___VMMR0_HWSVMR0_h
#define ___VMMR0_HWSVMR0_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/vmm/em.h>
#include <VBox/vmm/stam.h>
#include <VBox/dis.h>
#include <VBox/vmm/hm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/hm_svm.h>

RT_C_DECLS_BEGIN

/** @defgroup grp_svm_int  Internal
 * @ingroup grp_svm
 * @internal
 * @{
 */

#ifdef IN_RING0

/**
 * Enters the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCpu        Pointer to the CPU info struct.
 */
VMMR0DECL(int) SVMR0Enter(PVM pVM, PVMCPU pVCpu, PHMGLOBLCPUINFO pCpu);

/**
 * Leaves the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0DECL(int) SVMR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);

VMMR0DECL(int) SVMR0EnableCpu(PHMGLOBLCPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS HCPhysCpuPage, bool fEnabledBySystem);

/**
 * Deactivates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            Pointer to the CPU info struct.
 * @param   pvPageCpu       Pointer to the global CPU page.
 * @param   pPageCpuPhys    Physical address of the global CPU page.
 */
VMMR0DECL(int) SVMR0DisableCpu(PHMGLOBLCPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Does Ring-0 per VM AMD-V init.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) SVMR0InitVM(PVM pVM);

/**
 * Does Ring-0 per VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) SVMR0TermVM(PVM pVM);

/**
 * Sets up AMD-V for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR0DECL(int) SVMR0SetupVM(PVM pVM);


/**
 * Runs guest code in an AMD-V VM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0DECL(int) SVMR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


/**
 * Save the host state.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 */
VMMR0DECL(int) SVMR0SaveHostState(PVM pVM, PVMCPU pVCpu);

/**
 * Loads the guest state.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR0DECL(int) SVMR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
/**
 * Prepares for and executes VMRUN (64-bit guests from a 32-bit host).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Pointer to the guest CPU context.
 * @param   pVM             Pointer to the VM.
 * @param   pVCpu           Pointer to the VMCPU. (not used)
 */
DECLASM(int) SVMR0VMSwitcherRun64(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);

/**
 * Executes the specified handler in 64-bit mode.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 * @param   pfnHandler  Pointer to the RC handler function.
 * @param   cbParam     Number of parameters.
 * @param   paParam     Array of 32-bit parameters.
 */
VMMR0DECL(int) SVMR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTRCPTR pfnHandler, uint32_t cbParam,
                                         uint32_t *paParam);
#endif /* HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */

/**
 * Prepares for and executes VMRUN (32-bit guests).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Pointer to the guest CPU context.
 * @param   pVM             Pointer to the VM. (not used)
 * @param   pVCpu           Pointer to the VMCPU. (not used)
 */
DECLASM(int) SVMR0VMRun(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);


/**
 * Prepares for and executes VMRUN (64-bit guests).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Pointer to the guest CPU context.
 * @param   pVM             Pointer to the VM. (not used)
 * @param   pVCpu           Pointer to the VMCPU. (not used)
 */
DECLASM(int) SVMR0VMRun64(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);

/**
 * Executes INVLPGA.
 *
 * @param   pPageGC         Virtual page to invalidate.
 * @param   u32ASID         Tagged TLB id.
 */
DECLASM(void) SVMR0InvlpgA(RTGCPTR pPageGC, uint32_t u32ASID);

/** Convert hidden selector attribute word between VMX and SVM formats. */
#define SVM_HIDSEGATTR_VMX2SVM(a)     (a & 0xFF) | ((a & 0xF000) >> 4)
#define SVM_HIDSEGATTR_SVM2VMX(a)     (a & 0xFF) | ((a & 0x0F00) << 4)

#define SVM_WRITE_SELREG(REG, reg) \
    do \
    { \
        Assert(pCtx->reg.fFlags & CPUMSELREG_FLAGS_VALID); \
        Assert(pCtx->reg.ValidSel == pCtx->reg.Sel); \
        pVMCB->guest.REG.u16Sel     = pCtx->reg.Sel; \
        pVMCB->guest.REG.u32Limit   = pCtx->reg.u32Limit; \
        pVMCB->guest.REG.u64Base    = pCtx->reg.u64Base; \
        pVMCB->guest.REG.u16Attr    = SVM_HIDSEGATTR_VMX2SVM(pCtx->reg.Attr.u); \
    } while (0)

#define SVM_READ_SELREG(REG, reg) \
    do \
    { \
        pCtx->reg.Sel       = pVMCB->guest.REG.u16Sel; \
        pCtx->reg.ValidSel  = pVMCB->guest.REG.u16Sel; \
        pCtx->reg.fFlags    = CPUMSELREG_FLAGS_VALID; \
        pCtx->reg.u32Limit  = pVMCB->guest.REG.u32Limit; \
        pCtx->reg.u64Base   = pVMCB->guest.REG.u64Base; \
        pCtx->reg.Attr.u    = SVM_HIDSEGATTR_SVM2VMX(pVMCB->guest.REG.u16Attr); \
    } while (0)

#endif /* IN_RING0 */

/** @} */

RT_C_DECLS_END

#endif /* !___VMMR0_HWSVMR0_h */

