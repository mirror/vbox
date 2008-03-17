/* $Id$ */
/** @file
 * HWACCM VT-x - Internal header file.
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

#ifndef ___HWVMXR0_h
#define ___HWVMXR0_h

#include <VBox/cdefs.h>
#include <VBox/types.h>
#include <VBox/em.h>
#include <VBox/stam.h>
#include <VBox/dis.h>
#include <VBox/hwaccm.h>
#include <VBox/pgm.h>
#include <VBox/hwacc_vmx.h>

__BEGIN_DECLS

/** @defgroup grp_vmx       Internal
 * @ingroup grp_vmx
 * @internal
 * @{
 */

#ifdef IN_RING0

/**
 * Enters the VT-x session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) VMXR0Enter(PVM pVM);

/**
 * Leaves the VT-x session
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) VMXR0Leave(PVM pVM);


/**
 * Sets up and activates VT-x on the current CPU
 *
 * @returns VBox status code.
 * @param   idCpu           The identifier for the CPU the function is called on.
 * @param   pVM             The VM to operate on.
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
HWACCMR0DECL(int) VMXR0EnableCpu(RTCPUID idCpu, PVM pVM, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Deactivates VT-x on the current CPU
 *
 * @returns VBox status code.
 * @param   idCpu           The identifier for the CPU the function is called on.
 * @param   pvPageCpu       Pointer to the global cpu page
 * @param   pPageCpuPhys    Physical address of the global cpu page
 */
HWACCMR0DECL(int) VMXR0DisableCpu(RTCPUID idCpu, void *pvPageCpu, RTHCPHYS pPageCpuPhys);

/**
 * Sets up VT-x for the specified VM
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) VMXR0SetupVM(PVM pVM);


/**
 * Save the host state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR0DECL(int) VMXR0SaveHostState(PVM pVM);

/**
 * Loads the guest state
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 */
HWACCMR0DECL(int) VMXR0LoadGuestState(PVM pVM, CPUMCTX *pCtx);


/**
 * Runs guest code in a VT-x VM.
 *
 * @note NEVER EVER turn on interrupts here. Due to our illegal entry into the kernel, it might mess things up. (XP kernel traps have been frequently observed)
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Guest context
 */
HWACCMR0DECL(int) VMXR0RunGuestCode(PVM pVM, CPUMCTX *pCtx);


#define VMX_WRITE_SELREG(REG, reg) \
        rc  = VMXWriteVMCS(VMX_VMCS_GUEST_FIELD_##REG,      pCtx->reg);                         \
        rc |= VMXWriteVMCS(VMX_VMCS_GUEST_##REG##_LIMIT,    pCtx->reg##Hid.u32Limit);           \
        rc |= VMXWriteVMCS(VMX_VMCS_GUEST_##REG##_BASE,     pCtx->reg##Hid.u32Base);            \
        if (pCtx->eflags.u32 & X86_EFL_VM)                                                      \
            val = pCtx->reg##Hid.Attr.u;                                                        \
        else                                                                                    \
        if (pCtx->reg && pCtx->reg##Hid.Attr.n.u1Present == 1)                                  \
            val = pCtx->reg##Hid.Attr.u | X86_SEL_TYPE_ACCESSED;                                \
        else                                                                                    \
            val = 0x10000;  /* Invalid guest state error otherwise. (BIT(16) = Unusable) */     \
                                                                                                \
        rc |= VMXWriteVMCS(VMX_VMCS_GUEST_##REG##_ACCESS_RIGHTS, val);

#define VMX_READ_SELREG(REG, reg) \
        VMXReadVMCS(VMX_VMCS_GUEST_FIELD_##REG,           &val);     \
        pCtx->reg                = val;                              \
        VMXReadVMCS(VMX_VMCS_GUEST_##REG##_LIMIT,         &val);     \
        pCtx->reg##Hid.u32Limit    = val;                            \
        VMXReadVMCS(VMX_VMCS_GUEST_##REG##_BASE,          &val);     \
        pCtx->reg##Hid.u32Base     = val;                            \
        VMXReadVMCS(VMX_VMCS_GUEST_##REG##_ACCESS_RIGHTS, &val);     \
        pCtx->reg##Hid.Attr.u    = val;


#endif /* IN_RING0 */

/** @} */

__END_DECLS

#endif

