/* $Id$ */
/** @file
 * HWACCM AMD-V - Internal header file.
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

#ifndef ___HWSVMR0_h
#define ___HWSVMR0_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/dis.h>
#include <VBox/hwaccm.h>
#include <VBox/pgm.h>
#include <VBox/hwacc_svm.h>

__BEGIN_DECLS

/** @defgroup grp_svm       Internal
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
 */
HWACCMR0DECL(int) SVMR0Enter(PVM pVM);

/**
 * Leaves the AMD-V session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0Leave(PVM pVM);

/**
 * Sets up and activates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   idCpu           The identifier for the CPU the function is called on.
 * @param   pVM             The VM to operate on.
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
HWACCMR0DECL(int) SVMR0EnableCpu(RTCPUID idCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Deactivates AMD-V on the current CPU
 *
 * @returns VBox status code.
 * @param   idCpu           The identifier for the CPU the function is called on.
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
HWACCMR0DECL(int) SVMR0DisableCpu(RTCPUID idCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Does Ring-0 per VM AMD-V init.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0InitVM(PVM pVM);

/**
 * Does Ring-0 per VM AMD-V termination.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0TermVM(PVM pVM);

/**
 * Sets up AMD-V for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0SetupVM(PVM pVM);


/**
 * Runs guest code in an AMD-V VM.
 *
 * @note NEVER EVER turn on interrupts here. Due to our illegal entry into the kernel, it might mess things up. (XP kernel traps have been frequently observed)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 */
HWACCMR0DECL(int) SVMR0RunGuestCode(PVM pVM, CPUMCTX *pCtx);


/**
 * Loads the guest state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 */
HWACCMR0DECL(int) SVMR0LoadGuestState(PVM pVM, CPUMCTX *pCtx);

/**
 * Invalidates a guest page
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   GCVirt      Page to invalidate
 */
HWACCMR0DECL(int) SVMR0InvalidatePage(PVM pVM, RTGCPTR GCVirt);

/**
 * Flushes the guest TLB
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0FlushTLB(PVM pVM);


/* Convert hidden selector attribute word between VMX and SVM formats. */
#define SVM_HIDSEGATTR_VMX2SVM(a)     (a & 0xFF) | ((a & 0xF000) >> 4)
#define SVM_HIDSEGATTR_SVM2VMX(a)     (a & 0xFF) | ((a & 0x0F00) << 4)

#define SVM_WRITE_SELREG(REG, reg)                                      \
        pVMCB->guest.REG.u16Sel   = pCtx->reg;                          \
        pVMCB->guest.REG.u32Limit = pCtx->reg##Hid.u32Limit;            \
        pVMCB->guest.REG.u64Base  = pCtx->reg##Hid.u32Base;             \
        pVMCB->guest.REG.u16Attr  = SVM_HIDSEGATTR_VMX2SVM(pCtx->reg##Hid.Attr.u);

#define SVM_READ_SELREG(REG, reg)                                       \
        pCtx->reg                = pVMCB->guest.REG.u16Sel;             \
        pCtx->reg##Hid.u32Limit  = pVMCB->guest.REG.u32Limit;           \
        pCtx->reg##Hid.u32Base   = pVMCB->guest.REG.u64Base;            \
        pCtx->reg##Hid.Attr.u    = SVM_HIDSEGATTR_SVM2VMX(pVMCB->guest.REG.u16Attr);

#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

