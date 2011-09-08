/* $Id$ */
/** @file
 * HM SVM (AMD-V) - Internal header file.
 */

/*
 * Copyright (C) 2006-2007 Oracle Corporation
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
#include <VBox/vmm/hwaccm.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/hwacc_svm.h>

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
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCpu        CPU info struct
 */
VMMR0DECL(int) SVMR0Enter(PVM pVM, PVMCPU pVCpu, PHMGLOBLCPUINFO pCpu);

/**
 * Leaves the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        CPU context
 */
VMMR0DECL(int) SVMR0Leave(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);

/**
 * Sets up and activates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pVM             The VM to operate on. (can be NULL after a resume)
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) SVMR0EnableCpu(PHMGLOBLCPUINFO pCpu, PVM pVM, void *pvPageCpu, RTHCPHYS HCPhysCpuPage);

/**
 * Deactivates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   pCpu            CPU info struct
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
VMMR0DECL(int) SVMR0DisableCpu(PHMGLOBLCPUINFO pCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Does Ring-0 per VM AMD-V init.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) SVMR0InitVM(PVM pVM);

/**
 * Does Ring-0 per VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) SVMR0TermVM(PVM pVM);

/**
 * Sets up AMD-V for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR0DECL(int) SVMR0SetupVM(PVM pVM);


/**
 * Runs guest code in an AMD-V VM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) SVMR0RunGuestCode(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);


/**
 * Save the host state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 */
VMMR0DECL(int) SVMR0SaveHostState(PVM pVM, PVMCPU pVCpu);

/**
 * Loads the guest state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 */
VMMR0DECL(int) SVMR0LoadGuestState(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx);

#if HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)

/**
 * Prepares for and executes VMRUN (64 bits guests from a 32 bits hosts).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Guest context.
 * @param   pVM             The VM to operate on.
 * @param   pVCpu           The VMCPU to operate on. (not used)
 */
DECLASM(int) SVMR0VMSwitcherRun64(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);

/**
 * Executes the specified handler in 64 mode
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pVCpu       The VMCPU to operate on.
 * @param   pCtx        Guest context
 * @param   pfnHandler  RC handler
 * @param   cbParam     Number of parameters
 * @param   paParam     Array of 32 bits parameters
 */
VMMR0DECL(int) SVMR0Execute64BitsHandler(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx, RTRCPTR pfnHandler, uint32_t cbParam, uint32_t *paParam);

#endif /* HC_ARCH_BITS == 32 && defined(VBOX_WITH_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL) */

/**
 * Prepares for and executes VMRUN (32 bits guests).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Guest context.
 * @param   pVM             The VM to operate on. (not used)
 * @param   pVCpu           The VMCPU to operate on. (not used)
 */
DECLASM(int) SVMR0VMRun(RTHCPHYS pVMCBHostPhys, RTHCPHYS pVMCBPhys, PCPUMCTX pCtx, PVM pVM, PVMCPU pVCpu);


/**
 * Prepares for and executes VMRUN (64 bits guests).
 *
 * @returns VBox status code.
 * @param   pVMCBHostPhys   Physical address of host VMCB.
 * @param   pVMCBPhys       Physical address of the VMCB.
 * @param   pCtx            Guest context.
 * @param   pVM             The VM to operate on. (not used)
 * @param   pVCpu           The VMCPU to operate on. (not used)
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

#define SVM_WRITE_SELREG(REG, reg)                                      \
{                                                                       \
        pVMCB->guest.REG.u16Sel   = pCtx->reg;                          \
        pVMCB->guest.REG.u32Limit = pCtx->reg##Hid.u32Limit;            \
        pVMCB->guest.REG.u64Base  = pCtx->reg##Hid.u64Base;             \
        pVMCB->guest.REG.u16Attr  = SVM_HIDSEGATTR_VMX2SVM(pCtx->reg##Hid.Attr.u); \
}

#define SVM_READ_SELREG(REG, reg)                                       \
{                                                                       \
        pCtx->reg                = pVMCB->guest.REG.u16Sel;             \
        pCtx->reg##Hid.u32Limit  = pVMCB->guest.REG.u32Limit;           \
        pCtx->reg##Hid.u64Base   = pVMCB->guest.REG.u64Base;            \
        pCtx->reg##Hid.Attr.u    = SVM_HIDSEGATTR_SVM2VMX(pVMCB->guest.REG.u16Attr); \
}

#endif /* IN_RING0 */

/** @} */

RT_C_DECLS_END

#endif

