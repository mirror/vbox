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
 *
 * @section sec_nem_win     Windows
 *
 * On Windows the Hyper-V root partition (dom0 in zen terminology) does not have
 * nested VT-x or AMD-V capabilities.  For a while raw-mode worked in it,
 * however now we \#GP when modifying CR4.  So, when Hyper-V is active on
 * Windows we have little choice but to use Hyper-V to run our VMs.
 *
 * @subsection subsec_nem_win_whv   The WinHvPlatform API
 *
 * Since Windows 10 build 17083 there is a documented API for managing Hyper-V
 * VMs, header file WinHvPlatform.h and implementation in WinHvPlatform.dll.
 * This interface is a wrapper around the undocumented Virtualization
 * Infrastructure Driver (VID) API - VID.DLL and VID.SYS.  The wrapper is
 * written in C++, namespaced and early version (at least) was using standard
 * container templates in several places.
 *
 * When creating a VM using WHvCreatePartition, it will only create the
 * WinHvPlatform structures for it, to which you get an abstract pointer.  The
 * VID API that actually creates the partition is first engaged when you call
 * WHvSetupPartition after first setting a lot of properties using
 * WHvSetPartitionProperty.  Since the VID API is just a very thin wrapper
 * around CreateFile and NtDeviceIoControl, it returns an actual HANDLE for the
 * partition WinHvPlatform.  We fish this HANDLE out of the WinHvPlatform
 * partition structures because we need to talk directly to VID for reasons
 * we'll get to in a bit.  (Btw. we could also intercept the CreateFileW or
 * NtDeviceIoControl calls from VID.DLL to get the HANDLE should fishing in the
 * partition structures become difficult.)
 *
 * The WinHvPlatform API requires us to both set the number of guest CPUs before
 * setting up the partition and call WHvCreateVirtualProcessor for each of them.
 * The CPU creation function boils down to a VidMessageSlotMap call that sets up
 * and maps a message buffer into ring-3 for async communication with hyper-V
 * and/or the VID.SYS thread actually running the CPU.  When for instance a
 * VMEXIT is encountered, hyper-V sends a message that the
 * WHvRunVirtualProcessor API retrieves (and later acknowledges) via
 * VidMessageSlotHandleAndGetNext.  It should be noteded that
 * WHvDeleteVirtualProcessor doesn't do much as there seems to be no partner
 * function VidMessagesSlotMap that reverses what it did.
 *
 * Memory is managed thru calls to WHvMapGpaRange and WHvUnmapGpaRange (GPA does
 * not mean grade point average here, but rather guest physical addressspace),
 * which corresponds to VidCreateVaGpaRangeSpecifyUserVa and VidDestroyGpaRange
 * respectively.  As 'UserVa' indicates, the functions works on user process
 * memory.  The mappings are also subject to quota restrictions, so the number
 * of ranges are limited and probably their total size as well.  Obviously
 * VID.SYS keeps track of the ranges, but so does WinHvPlatform, which means
 * there is a bit of overhead involved and quota restrctions makes sense.  For
 * some reason though, regions are lazily mapped on VMEXIT/memory by
 * WHvRunVirtualProcessor.
 *
 * Running guest code is done thru the WHvRunVirtualProcessor function.  It
 * asynchronously starts or resumes hyper-V CPU execution and then waits for an
 * VMEXIT message.   Other threads can interrupt the execution by using
 * WHvCancelVirtualProcessor, which which case the thread in
 * WHvRunVirtualProcessor is woken up via a dummy QueueUserAPC and will call
 * VidStopVirtualProcessor to asynchronously end execution.  The stop CPU call
 * not immediately succeed if the CPU encountered a VMEXIT before the stop was
 * processed, in which case the VMEXIT needs to be processed first, and the
 * pending stop will be processed in a subsequent call to
 * WHvRunVirtualProcessor.
 *
 * {something about registers}
 *
 * @subsubsection subsubsec_nem_win_whv_cons    Issues / Disadvantages
 *
 * Here are some observations:
 *
 * - The WHvCancelVirtualProcessor API schedules a dummy usermode APC callback
 *   in order to cancel any current or future alertable wait in VID.SYS during
 *   the VidMessageSlotHandleAndGetNext call.
 *
 *   IIRC this will make the kernel schedule the callback thru
 *   NTDLL!KiUserApcDispatcher by modifying the thread context and quite
 *   possibly the userland thread stack.  When the APC callback returns to
 *   KiUserApcDispatcher, it will call NtContinue to restore the old thread
 *   context and resume execution from there.  Upshot this is a bit expensive.
 *
 *   Using NtAltertThread call could do the same without the thread context
 *   modifications and the extra kernel call.
 *
 *
 * - Not sure if this is a thing, but WHvCancelVirtualProcessor seems to cause
 *   cause a lot more spurious WHvRunVirtualProcessor returns that what we get
 *   with the replacement code.  By spurious returns we mean that the
 *   subsequent call to WHvRunVirtualProcessor would return immediately.
 *
 *
 * - When WHvRunVirtualProcessor returns without a message, or on a terse
 *   VID message like HLT, it will make a kernel call to get some registers.
 *   This is potentially inefficient if the caller decides he needs more
 *   register state.
 *
 *   It would be better to just return what's available and let the caller fetch
 *   what is missing from his point of view in a single kernel call.
 *
 *
 * - The WHvRunVirtualProcessor implementation does lazy GPA range mappings when
 *   a unmapped GPA message is received from hyper-V.
 *
 *   Since MMIO is currently realized as unmapped GPA, this will slow down all
 *   MMIO accesses a tiny little bit as WHvRunVirtualProcessor looks up the
 *   guest physical address the checks if it's a pending lazy mapping.
 *
 *
 * - There is no API for modifying protection of a page within a GPA range.
 *
 *   We're left with having to unmap the range and then remap it with the new
 *   protection.  For instance we're actively using this to track dirty VRAM
 *   pages, which means there are occational readonly->writable transitions at
 *   run time followed by bulk reversal to readonly when the display is
 *   refreshed.
 *
 *   Now to work around the issue, we do page sized GPA ranges.  In addition to
 *   add a lot of tracking overhead to WinHvPlatform and VID.SYS, it also causes
 *   us to exceed our quota before we've even mapped a default sized VRAM
 *   page-by-page.  So, to work around this quota issue we have to lazily map
 *   pages and actively restrict the number of mappings.
 *
 *   Out best workaround thus far is bypassing WinHvPlatform and VID when in
 *   comes to memory and instead us the hypercalls to do it (HvCallMapGpaPages,
 *   HvCallUnmapGpaPages).  (This also maps a whole lot better into our own
 *   guest page management infrastructure.)
 *
 *
 * - Observed problems doing WHvUnmapGpaRange followed by WHvMapGpaRange.
 *
 *   As mentioned above, we've been forced to use this sequence when modifying
 *   page protection.   However, when upgrading from readonly to writable, we've
 *   ended up looping forever with the same write to readonly memory exit.
 *
 *   Workaround: Insert a WHvRunVirtualProcessor call and make sure to get a GPA
 *   unmapped exit between the two calls.  Terrible for performance and code
 *   sanity.
 *
 *
 * - WHVRunVirtualProcessor wastes time converting VID/Hyper-V messages to it's
 *   own defined format.
 *
 *   We understand this might be because Microsoft wishes to remain free to
 *   modify the VID/Hyper-V messages, but it's still rather silly and does slow
 *   things down.
 *
 *
 * - WHVRunVirtualProcessor would've benefited from using a callback interface:
 *      - The potential size changes of the exit context structure wouldn't be
 *        an issue, since the function could manage that itself.
 *      - State handling could be optimized simplified (esp. cancellation).
 *
 *
 * - WHvGetVirtualProcessorRegisters and WHvSetVirtualProcessorRegisters
 *   internally converts register names, probably using temporary heap buffers.
 *
 *   From the looks of things, it's converting from WHV_REGISTER_NAME to
 *   HV_REGISTER_NAME that's documented in the "Virtual Processor Register
 *   Names" section of "Hypervisor Top-Level Functional Specification".  This
 *   feels like an awful waste of time.  We simply cannot understand why it
 *   wouldn't have sufficed to use HV_REGISTER_NAME here and simply checked the
 *   input values if restrictions were desired.
 *
 *   To avoid the heap + conversion overhead, we're currently using the
 *   HvCallGetVpRegisters and HvCallSetVpRegisters calls directly.
 *
 *
 * - Why does WINHVR.SYS (or VID.SYS) only query/set 32 registers at the time
 *   thru the HvCallGetVpRegisters and HvCallSetVpRegisters hypercalls?
 *
 *   We've not trouble getting/setting all the registers defined by
 *   WHV_REGISTER_NAME in one hypercall...
 *
 *
 * - .
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

