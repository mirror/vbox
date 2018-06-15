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
 * This is an alternative execution manage to HM and raw-mode.   On one host
 * (Windows) we're forced to use this, on the others we just do it because we
 * can.   Since this is host specific in nature, information about an
 * implementation is contained in the NEMR3Native-xxxx.cpp files.
 *
 * @ref pg_nem_win
 */


/*********************************************************************************************************************************
*   Header Files                                                                                                                 *
*********************************************************************************************************************************/
#define LOG_GROUP LOG_GROUP_NEM
#include <VBox/vmm/nem.h>
#include <VBox/vmm/gim.h>
#include "NEMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/vmm/uvm.h>

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
                                  "Enabled"
                                  "|Allow64BitGuests",
                                  "" /* pszValidNodes */, "NEM" /* pszWho */, 0 /* uInstance */);
    if (RT_FAILURE(rc))
        return rc;

    /** @cfgm{/NEM/NEMEnabled, bool, true}
     * Whether NEM is enabled. */
    rc = CFGMR3QueryBoolDef(pCfgNem, "Enabled", &pVM->nem.s.fEnabled, true);
    AssertLogRelRCReturn(rc, rc);


#ifdef VBOX_WITH_64_BITS_GUESTS
    /** @cfgm{/HM/Allow64BitGuests, bool, 32-bit:false, 64-bit:true}
     * Enables AMD64 CPU features.
     * On 32-bit hosts this isn't default and require host CPU support. 64-bit hosts
     * already have the support. */
    rc = CFGMR3QueryBoolDef(pCfgNem, "Allow64BitGuests", &pVM->nem.s.fAllow64BitGuests, HC_ARCH_BITS == 64);
    AssertLogRelRCReturn(rc, rc);
#else
    pVM->nem.s.fAllow64BitGuests = false;
#endif


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
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
    {
        /*
         * Enable CPU features making general ASSUMPTIONS (there are two similar
         * blocks of code in HM.cpp), to avoid duplicating this code.  The
         * native backend can make check capabilities and adjust as needed.
         */
        CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
        if (CPUMGetGuestCpuVendor(pVM) == CPUMCPUVENDOR_AMD)
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);            /* 64 bits only on Intel CPUs */
        if (pVM->nem.s.fAllow64BitGuests)
        {
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
        }
        /* Turn on NXE if PAE has been enabled. */
        else if (CPUMR3GetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE))
            CPUMR3SetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);

        /*
         * Do native after-CPUM init.
         */
#ifdef VBOX_WITH_NATIVE_NEM
        rc = nemR3NativeInitAfterCPUM(pVM);
#else
        RT_NOREF(pVM);
#endif
    }
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
    /*
     * Check if GIM needs #UD, since that applies to everyone.
     */
    if (enmWhat == VMINITCOMPLETED_RING3)
        for (VMCPUID iCpu = 0; iCpu < pVM->cCpus; iCpu++)
            pVM->aCpus[iCpu].nem.s.fGIMTrapXcptUD = GIMShouldTrapXcptUD(&pVM->aCpus[iCpu]);

    /*
     * Call native code.
     */
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
 * External interface for querying whether native execution API is used.
 *
 * @returns true if NEM is being used, otherwise false.
 * @param   pUVM        The user mode VM handle.
 * @sa      HMR3IsEnabled
 */
VMMR3DECL(bool) NEMR3IsEnabled(PUVM pUVM)
{
    UVM_ASSERT_VALID_EXT_RETURN(pUVM, false);
    PVM pVM = pUVM->pVM;
    VM_ASSERT_VALID_EXT_RETURN(pVM, false);
    return VM_IS_NEM_ENABLED(pVM);
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
 * @param   pVCpu       The cross context virtual CPU structure to reset.
 * @param   fInitIpi    Set if being reset due to INIT IPI.
 */
VMMR3_INT_DECL(void) NEMR3ResetCpu(PVMCPU pVCpu, bool fInitIpi)
{
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVCpu->pVMR3->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        nemR3NativeResetCpu(pVCpu, fInitIpi);
#else
    RT_NOREF(pVCpu, fInitIpi);
#endif
}


/**
 * Indicates to TM that TMTSCMODE_NATIVE_API should be used for TSC.
 *
 * @returns true if TMTSCMODE_NATIVE_API must be used, otherwise @c false.
 * @param   pVM     The cross context VM structure.
 */
VMMR3_INT_DECL(bool) NEMR3NeedSpecialTscMode(PVM pVM)
{
#ifdef VBOX_WITH_NATIVE_NEM
# ifdef RT_OS_WINDOWS
    if (VM_IS_NEM_ENABLED(pVM))
        return true;
# endif
#else
    RT_NOREF(pVM);
#endif
    return false;
}


/**
 * Gets the name of a generic NEM exit code.
 *
 * @returns Pointer to read only string if @a uExit is known, otherwise NULL.
 * @param   uExit               The NEM exit to name.
 */
VMMR3DECL(const char *) NEMR3GetExitName(uint32_t uExit)
{
    switch ((NEMEXITTYPE)uExit)
    {
        case NEMEXITTYPE_UNRECOVERABLE_EXCEPTION:       return "NEM unrecoverable exception";
        case NEMEXITTYPE_INVALID_VP_REGISTER_VALUE:     return "NEM invalid vp register value";
        case NEMEXITTYPE_INTTERRUPT_WINDOW:             return "NEM interrupt window";
        case NEMEXITTYPE_HALT:                          return "NEM halt";
        case NEMEXITTYPE_XCPT_UD:                       return "NEM #UD";
        case NEMEXITTYPE_XCPT_DB:                       return "NEM #DB";
        case NEMEXITTYPE_XCPT_BP:                       return "NEM #BP";
        case NEMEXITTYPE_CANCELED:                      return "NEM canceled";
        case NEMEXITTYPE_MEMORY_ACCESS:                 return "NEM memory access";
    }

    return NULL;
}


VMMR3_INT_DECL(VBOXSTRICTRC) NEMR3RunGC(PVM pVM, PVMCPU pVCpu)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    return nemR3NativeRunGC(pVM, pVCpu);
#else
    NOREF(pVM); NOREF(pVCpu);
    return VERR_INTERNAL_ERROR_3;
#endif
}


VMMR3_INT_DECL(bool) NEMR3CanExecuteGuest(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    return nemR3NativeCanExecuteGuest(pVM, pVCpu, pCtx);
#else
    NOREF(pVM); NOREF(pVCpu); NOREF(pCtx);
    return false;
#endif
}


VMMR3_INT_DECL(bool) NEMR3SetSingleInstruction(PVM pVM, PVMCPU pVCpu, bool fEnable)
{
    Assert(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    return nemR3NativeSetSingleInstruction(pVM, pVCpu, fEnable);
#else
    NOREF(pVM); NOREF(pVCpu); NOREF(fEnable);
    return false;
#endif
}


VMMR3_INT_DECL(void) NEMR3NotifyFF(PVM pVM, PVMCPU pVCpu, uint32_t fFlags)
{
    AssertLogRelReturnVoid(VM_IS_NEM_ENABLED(pVM));
#ifdef VBOX_WITH_NATIVE_NEM
    nemR3NativeNotifyFF(pVM, pVCpu, fFlags);
#else
    RT_NOREF(pVM, pVCpu, fFlags);
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


VMMR3_INT_DECL(int)  NEMR3NotifyPhysMmioExMap(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags, void *pvMmio2)
{
    int rc = VINF_SUCCESS;
#ifdef VBOX_WITH_NATIVE_NEM
    if (pVM->bMainExecutionEngine == VM_EXEC_ENGINE_NATIVE_API)
        rc = nemR3NativeNotifyPhysMmioExMap(pVM, GCPhys, cb, fFlags, pvMmio2);
#else
    NOREF(pVM); NOREF(GCPhys); NOREF(cb); NOREF(fFlags); NOREF(pvMmio2);
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


VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterEarly(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
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


/**
 * Called after the ROM range has been fully completed.
 *
 * This will be preceeded by a NEMR3NotifyPhysRomRegisterEarly() call as well a
 * number of NEMHCNotifyPhysPageProtChanged calls.
 *
 * @returns VBox status code
 * @param   pVM             The cross context VM structure.
 * @param   GCPhys          The ROM address (page aligned).
 * @param   cb              The size (page aligned).
 * @param   fFlags          NEM_NOTIFY_PHYS_ROM_F_XXX.
 */
VMMR3_INT_DECL(int)  NEMR3NotifyPhysRomRegisterLate(PVM pVM, RTGCPHYS GCPhys, RTGCPHYS cb, uint32_t fFlags)
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

