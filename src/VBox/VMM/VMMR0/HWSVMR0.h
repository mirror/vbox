/* $Id$ */
/** @file
 * HWACCM SVM - Internal header file.
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
 * Enable SVM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0Enable(PVM pVM);

/**
 * Disable SVM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0Disable(PVM pVM);

/**
 * Sets up and activates SVM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) SVMR0Setup(PVM pVM);


/**
 * Runs guest code in a SVM VM.
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

