/* $Id$ */
/** @file
 * NEM - Native execution manager.
 */

/*
 * Copyright (C) 2018 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/** @page pg_nem NEM - Native Execution Manager.
 *
 * Later.
 *
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NEM
#include <VBox/vmm/nem.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>

#include <iprt/asm.h>



/**
 * Basic init and configuration reading.
 *
 * Always call NEMR3Term after calling this.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) NEMR3InitConfig(PVM pVM)
{
    LogFlow(("NEMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, nem.s, 64);
    AssertCompile(sizeof(pVM->nem.s) <= sizeof(pVM->nem.padding));

    /*
     * Initialize state info so NEMR3Term will always be happy.
     * No returning prior to setting magics!
     */
    pVM->nem.s.u32Magic = NEM_MAGIC;
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
        pVM->aCpus[iCpu].nem.s.u32Magic = NEMCPU_MAGIC;

    /*
     * Read configuration.
     */
    PCFGMNODE pCfgNem = CFGMR3GetChild(CFGMR3GetRoot(pVM), "NEM/");

    /*
     * Validate the NEM settings.
     */
    int rc = CFGMR3ValidateConfig(pCfgNem,
                                  "/NEM/",
                                  "Enabled",
                                  "" /* pszValidNodes */, "NEM" /* pszWho */, 0 /* uInstance */);
    if (RT_FAILURE(rc))
        return rc;

    /** @cfgm{/NEM/NEMEnabled, bool, true}
     * Whether NEM is enabled. */
    rc = CFGMR3QueryBoolDef(pCfgNem, "Enabled", &pVM->nem.s.fEnabled, true);
    AssertLogRelRCReturn(rc, rc);

    return VINF_SUCCESS;
}


/**
 * This is called by HMR3Init() when HM cannot be used.
 *
 * Sets VM::bMainExecutionEngine to VM_EXEC_ENGINE_NATIVE_API if we can use a
 * native hypervisor API to execute the VM.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   fFallback   Whether this is a fallback call.  Cleared if the VM is
 *                      configured to use NEM instead of HM.
 * @param   fForced     Whether /HM/HMForced was set.  If set and we fail to
 *                      enable NEM, we'll return a failure status code.
 *                      Otherwise we'll assume HMR3Init falls back on raw-mode.
 */
VMMR3_INT_DECL(int) NEMR3Init(PVM pVM, bool fFallback, bool fForced)
{
    Assert(pVM->bMainExecutionEngine != VM_EXEC_ENGINE_NATIVE_API);
    int rc;
    if (pVM->nem.s.fEnabled)
    {
#ifdef VBOX_WITH_NATIVE_NEM
        rc = nemR3NativeInit(pVM, fFallback, fForced);
        ASMCompilerBarrier(); /* May have changed bMainExecutionEngine. */
#else
        RT_NOREF(fFallback);
        rc = VINF_SUCCESS;
#endif
        if (RT_SUCCESS(rc))
        {
            if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
                LogRel(("NEM: NEMR3Init: Active.\n"));
            else
            {
                LogRel(("NEM: NEMR3Init: Not available.\n"));
                if (fForced)
                    rc = VERR_NEM_NOT_AVAILABLE;
            }
        }
        else
            LogRel(("NEM: NEMR3Init: Native init failed: %Rrc.\n", rc));
    }
    else
    {
        LogRel(("NEM: NEMR3Init: Disabled.\n"));
        rc = fForced ? VERR_NEM_NOT_ENABLED : VINF_SUCCESS;
    }
    return rc;
}


/**
 * Perform initialization that depends on CPUM working.
 *
 * This is a noop if NEM wasn't activated by a previous NEMR3Init() call.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) NEMR3InitAfterCPUM(PVM pVM)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeInitAfterCPUM(pVM);
#else
    RT_NOREF(pVM);
#endif
    return rc;
}


/**
 * Called when a init phase has completed.
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 * @param   enmWhat     The phase that completed.
 */
VMMR3_INT_DECL(int) NEMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeInitCompleted(pVM, enmWhat);
#else
    RT_NOREF(pVM, enmWhat);
#endif
    return rc;
}


/**
 *
 * @returns VBox status code.
 * @param   pVM         The cross context VM structure.
 */
VMMR3_INT_DECL(int) NEMR3Term(PVM pVM)
{
    AssertReturn(pVM->nem.s.u32Magic == NEM_MAGIC, VERR_WRONG_ORDER);
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
        AssertReturn(pVM->aCpus[iCpu].nem.s.u32Magic == NEMCPU_MAGIC, VERR_WRONG_ORDER);

    /* Do native termination. */
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeTerm(pVM);
#endif

    /* Mark it as terminated. */
    for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
        pVM->aCpus[iCpu].nem.s.u32Magic = NEMCPU_MAGIC_DEAD;
    pVM->nem.s.u32Magic = NEM_MAGIC_DEAD;
    return rc;
}


/**
 * The VM is being reset.
 *
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(void) NEMR3Reset(PVM pVM)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        nemR3NativeReset(pVM);
#else
    RT_NOREF(pVM);
#endif
}


/**
 * Resets a virtual CPU.
 *
 * Used to bring up secondary CPUs on SMP as well as CPU hot plugging.
 *
 * @param   pVCpu   The cross context virtual CPU structure to reset.
 */
VMMR3_INT_DECL(void) NEMR3ResetCpu(PVMCPU pVCpu)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVCpu->pVMR3->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        nemR3NativeResetCpu(pVCpu);
#else
    RT_NOREF(pVCpu);
#endif
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRamRegister(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeNotifyPhysRamRegister(pVM, GCPhys, cb);
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb);
#endif
    return rc;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeNotifyPhysMmioExMap(pVM, GCPhys, cb, fFlags);
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
#endif
    return rc;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysMmioExUnmap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeNotifyPhysMmioExUnmap(pVM, GCPhys, cb, fFlags);
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
#endif
    return rc;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeNotifyPhysRomRegisterEarly(pVM, GCPhys, cb, fFlags);
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
#endif
    return rc;
}


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTUINT cb, uint32_t fFlags)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeNotifyPhysRomRegisterLate(pVM, GCPhys, cb, fFlags);
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags);
#endif
    return rc;
}


VMMR3_INT_DECL(void) NEMR3NotifySetA20(PVMCPU pVCpu, bool fEnabled)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVCpu->pVMR3->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        nemR3NativeNotifySetA20(pVCpu, fEnabled);
#else
    NOREF(pVCpu); NOREF(fEnabled);
#endif
}

