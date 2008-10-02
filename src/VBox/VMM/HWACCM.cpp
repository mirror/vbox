/* $Id$ */
/** @file
 * HWACCM - Intel/AMD VM Hardware Support Manager
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

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HWACCM
#include <VBox/cpum.h>
#include <VBox/stam.h>
#include <VBox/mm.h>
#include <VBox/pdm.h>
#include <VBox/pgm.h>
#include <VBox/trpm.h>
#include <VBox/dbgf.h>
#include <VBox/hwacc_vmx.h>
#include <VBox/hwacc_svm.h>
#include "HWACCMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/param.h>
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/selm.h>

#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/thread.h>

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) hwaccmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) hwaccmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version);


/**
 * Initializes the HWACCM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(int) HWACCMR3Init(PVM pVM)
{
    LogFlow(("HWACCMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertRelease(!(RT_OFFSETOF(VM, hwaccm.s) & 31));
    AssertRelease(sizeof(pVM->hwaccm.s) <= sizeof(pVM->hwaccm.padding));

    /* Some structure checks. */
    AssertMsg(RT_OFFSETOF(SVM_VMCB, u8Reserved3) == 0xC0, ("u8Reserved3 offset = %x\n", RT_OFFSETOF(SVM_VMCB, u8Reserved3)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, ctrl.EventInject) == 0xA8, ("ctrl.EventInject offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.EventInject)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, ctrl.ExitIntInfo) == 0x88, ("ctrl.ExitIntInfo offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.ExitIntInfo)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, ctrl.TLBCtrl) == 0x58, ("ctrl.TLBCtrl offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.TLBCtrl)));

    AssertMsg(RT_OFFSETOF(SVM_VMCB, guest) == 0x400, ("guest offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved4) == 0x4A0, ("guest.u8Reserved4 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved4)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved6) == 0x4D8, ("guest.u8Reserved6 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved6)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved7) == 0x580, ("guest.u8Reserved7 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved7)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved9) == 0x648, ("guest.u8Reserved9 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved9)));
    AssertMsg(RT_OFFSETOF(SVM_VMCB, u8Reserved10) == 0x698, ("u8Reserved3 offset = %x\n", RT_OFFSETOF(SVM_VMCB, u8Reserved10)));
    AssertMsg(sizeof(SVM_VMCB) == 0x1000, ("SVM_VMCB size = %x\n", sizeof(SVM_VMCB)));


    /*
     * Register the saved state data unit.
     */
    int rc = SSMR3RegisterInternal(pVM, "HWACCM", 0, HWACCM_SSM_VERSION, sizeof(HWACCM),
                                   NULL, hwaccmR3Save, NULL,
                                   NULL, hwaccmR3Load, NULL);
    if (VBOX_FAILURE(rc))
        return rc;

    /* Misc initialisation. */
    pVM->hwaccm.s.vmx.fSupported = false;
    pVM->hwaccm.s.svm.fSupported = false;
    pVM->hwaccm.s.vmx.fEnabled   = false;
    pVM->hwaccm.s.svm.fEnabled   = false;

    pVM->hwaccm.s.fActive        = false;
    pVM->hwaccm.s.fNestedPaging  = false;

    /* On first entry we'll sync everything. */
    pVM->hwaccm.s.fContextUseFlags = HWACCM_CHANGED_ALL;

    pVM->hwaccm.s.vmx.cr0_mask = 0;
    pVM->hwaccm.s.vmx.cr4_mask = 0;

    /*
     * Statistics.
     */
    STAM_REG(pVM, &pVM->hwaccm.s.StatEntry,    STAMTYPE_PROFILE, "/PROF/HWACCM/SwitchToGC",     STAMUNIT_TICKS_PER_CALL, "Profiling of VMXR0RunGuestCode entry");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExit,     STAMTYPE_PROFILE, "/PROF/HWACCM/SwitchFromGC",   STAMUNIT_TICKS_PER_CALL, "Profiling of VMXR0RunGuestCode exit");
    STAM_REG(pVM, &pVM->hwaccm.s.StatInGC,     STAMTYPE_PROFILE, "/PROF/HWACCM/InGC",           STAMUNIT_TICKS_PER_CALL, "Profiling of vmlaunch");

    STAM_REG(pVM, &pVM->hwaccm.s.StatExitShadowNM,  STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Shadow/#NM",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestNM,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#NM",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitShadowPF,  STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Shadow/#PF",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestPF,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#PF",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestUD,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#UD",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestSS,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#SS",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestNP,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#NP",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestGP,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#GP",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestMF,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#MF",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestDE,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#DE",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitGuestDB,   STAMTYPE_COUNTER, "/HWACCM/Exit/Trap/Guest/#DB",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitInvpg,     STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Invlpg",      STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitInvd,      STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Invd",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCpuid,     STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Cpuid",       STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitRdtsc,     STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Rdtsc",       STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCRxWrite,  STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/CRx/Write",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCRxRead,   STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/CRx/Read",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitDRxWrite,  STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/DRx/Write",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitDRxRead,   STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/DRx/Read",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCLTS,      STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/CLTS",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitLMSW,      STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/LMSW",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIOWrite,   STAMTYPE_COUNTER, "/HWACCM/Exit/IO/Write",          STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIORead,    STAMTYPE_COUNTER, "/HWACCM/Exit/IO/Read",           STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIOStringWrite,   STAMTYPE_COUNTER, "/HWACCM/Exit/IO/WriteString",          STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIOStringRead,    STAMTYPE_COUNTER, "/HWACCM/Exit/IO/ReadString",           STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIrqWindow, STAMTYPE_COUNTER, "/HWACCM/Exit/GuestIrq/Pending",  STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitMaxResume, STAMTYPE_COUNTER, "/HWACCM/Exit/Safety/MaxResume",  STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatSwitchGuestIrq,STAMTYPE_COUNTER, "/HWACCM/Switch/IrqPending",      STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatSwitchToR3,    STAMTYPE_COUNTER, "/HWACCM/Switch/ToR3",            STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatIntInject,     STAMTYPE_COUNTER, "/HWACCM/Irq/Inject",             STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatIntReinject,   STAMTYPE_COUNTER, "/HWACCM/Irq/Reinject",           STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatPendingHostIrq,STAMTYPE_COUNTER, "/HWACCM/Irq/PendingOnHost",      STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushPageManual,       STAMTYPE_COUNTER, "/HWACCM/Flush/Page/Virt/Manual", STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushPhysPageManual,   STAMTYPE_COUNTER, "/HWACCM/Flush/Page/Phys/Manual", STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushTLBManual,        STAMTYPE_COUNTER, "/HWACCM/Flush/TLB/Manual",  STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushTLBCRxChange,     STAMTYPE_COUNTER, "/HWACCM/Flush/TLB/CRx",     STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushPageInvlpg,       STAMTYPE_COUNTER, "/HWACCM/Flush/Page/Invlpg", STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushTLBWorldSwitch,   STAMTYPE_COUNTER, "/HWACCM/Flush/TLB/Switch",  STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatNoFlushTLBWorldSwitch, STAMTYPE_COUNTER, "/HWACCM/Flush/TLB/Skipped", STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatFlushASID,             STAMTYPE_COUNTER, "/HWACCM/Flush/TLB/ASID",    STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatTSCOffset,             STAMTYPE_COUNTER, "/HWACCM/TSC/Offset",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatTSCIntercept,          STAMTYPE_COUNTER, "/HWACCM/TSC/Intercept",     STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatDRxArmed,              STAMTYPE_COUNTER, "/HWACCM/Debug/Armed",           STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatDRxContextSwitch,      STAMTYPE_COUNTER, "/HWACCM/Debug/ContextSwitch",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatDRxIOCheck,            STAMTYPE_COUNTER, "/HWACCM/Debug/IOCheck",         STAMUNIT_OCCURENCES,    "Nr of occurances");

    pVM->hwaccm.s.paStatExitReason = NULL;

#ifdef VBOX_WITH_STATISTICS
    rc = MMHyperAlloc(pVM, MAX_EXITREASON_STAT*sizeof(*pVM->hwaccm.s.paStatExitReason), 0, MM_TAG_HWACCM, (void **)&pVM->hwaccm.s.paStatExitReason);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        for (int i=0;i<MAX_EXITREASON_STAT;i++)
        {
            int rc = STAMR3RegisterF(pVM, &pVM->hwaccm.s.paStatExitReason[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, "Exit reason",
                                     "/HWACCM/Exit/Reason/%02x", i);
            AssertRC(rc);
        }
        int rc = STAMR3Register(pVM, &pVM->hwaccm.s.StatExitReasonNPF, STAMTYPE_COUNTER, STAMVISIBILITY_USED, "/HWACCM/Exit/Reason/#NPF", STAMUNIT_OCCURENCES, "Exit reason");
        AssertRC(rc);
    }
    pVM->hwaccm.s.paStatExitReasonR0 = MMHyperR3ToR0(pVM, pVM->hwaccm.s.paStatExitReason);
    Assert(pVM->hwaccm.s.paStatExitReasonR0);
#endif

    /* Disabled by default. */
    pVM->fHWACCMEnabled = false;

    /*
     * Check CFGM options.
     */
    /* Nested paging: disabled by default. */
    rc = CFGMR3QueryBoolDef(CFGMR3GetRoot(pVM), "EnableNestedPaging", &pVM->hwaccm.s.fAllowNestedPaging, false);
    AssertRC(rc);

    /* HWACCM support must be explicitely enabled in the configuration file. */
    rc = CFGMR3QueryBoolDef(CFGMR3GetChild(CFGMR3GetRoot(pVM), "HWVirtExt/"), "Enabled", &pVM->hwaccm.s.fAllowed, false);
    AssertRC(rc);

    return VINF_SUCCESS;
}

/**
 * Turns off normal raw mode features
 *
 * @param   pVM         The VM to operate on.
 */
static void hwaccmR3DisableRawMode(PVM pVM)
{
    /* Disable PATM & CSAM. */
    PATMR3AllowPatching(pVM, false);
    CSAMDisableScanning(pVM);

    /* Turn off IDT/LDT/GDT and TSS monitoring and sycing. */
    SELMR3DisableMonitoring(pVM);
    TRPMR3DisableMonitoring(pVM);

    /* The hidden selector registers are now valid. */
    CPUMSetHiddenSelRegsValid(pVM, true);

    /* Disable the switcher code (safety precaution). */
    VMMR3DisableSwitcher(pVM);

    /* Disable mapping of the hypervisor into the shadow page table. */
    PGMR3ChangeShwPDMappings(pVM, false);

    /* Disable the switcher */
    VMMR3DisableSwitcher(pVM);

    if (pVM->hwaccm.s.fNestedPaging)
    {
        /* Reinit the paging mode to force the new shadow mode. */
        PGMR3ChangeMode(pVM, PGMMODE_REAL);
    }
}

/**
 * Initialize VT-x or AMD-V.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
HWACCMR3DECL(int) HWACCMR3InitFinalizeR0(PVM pVM)
{
    int rc;

    if (    !pVM->hwaccm.s.vmx.fSupported
        &&  !pVM->hwaccm.s.svm.fSupported)
    {
        LogRel(("HWACCM: No VMX or SVM CPU extension found. Reason %Vrc\n", pVM->hwaccm.s.lLastError));
        LogRel(("HWACCM: VMX MSR_IA32_FEATURE_CONTROL=%VX64\n", pVM->hwaccm.s.vmx.msr.feature_ctrl));
        return VINF_SUCCESS;
    }

    /*
     * Note that we have a global setting for VT-x/AMD-V usage. VMX root mode changes the way the CPU operates. Our 64 bits switcher will trap
     * because it turns off paging, which is not allowed in VMX root mode.
     *
     * To simplify matters we'll just force all running VMs to either use raw or hwaccm mode. No mixing allowed.
     *
     */
    /* If we enabled or disabled hwaccm mode, then it can't be changed until all the VMs are shutdown. */
    rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_HWACC_ENABLE, (pVM->hwaccm.s.fAllowed) ? HWACCMSTATE_ENABLED : HWACCMSTATE_DISABLED, NULL);
    if (VBOX_FAILURE(rc))
    {
        LogRel(("HWACCMR3InitFinalize: SUPCallVMMR0Ex VMMR0_DO_HWACC_ENABLE failed with %Vrc\n", rc));
        LogRel(("HWACCMR3InitFinalize: disallowed %s of HWACCM\n", pVM->hwaccm.s.fAllowed ? "enabling" : "disabling"));
        /* Invert the selection */
        pVM->hwaccm.s.fAllowed ^= 1;
        LogRel(("HWACCMR3InitFinalize: new HWACCM status = %s\n", pVM->hwaccm.s.fAllowed ? "enabled" : "disabled"));

        if (pVM->hwaccm.s.fAllowed)
        {
            if (pVM->hwaccm.s.vmx.fSupported)
                VMSetRuntimeError(pVM, false, "HwAccmModeChangeDisallowed", "An active VM already uses Intel VT-x hardware acceleration. It is not allowed to simultaneously use software virtualization, therefore this VM will be run using VT-x as well.\n");
            else
                VMSetRuntimeError(pVM, false, "HwAccmModeChangeDisallowed", "An active VM already uses AMD-V hardware acceleration. It is not allowed to simultaneously use software virtualization, therefore this VM will be run using AMD-V as well.\n");
        }
        else
            VMSetRuntimeError(pVM, false, "HwAccmModeChangeDisallowed", "An active VM already uses software virtualization. It is not allowed to simultaneously use VT-x or AMD-V, therefore this VM will be run using software virtualization as well.\n");
    }

    if (pVM->hwaccm.s.fAllowed == false)
        return VINF_SUCCESS;    /* disabled */

    Assert(!pVM->fHWACCMEnabled);

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        Log(("pVM->hwaccm.s.vmx.fSupported = %d\n", pVM->hwaccm.s.vmx.fSupported));

        if (    pVM->hwaccm.s.fInitialized == false
            &&  pVM->hwaccm.s.vmx.msr.feature_ctrl != 0)
        {
            uint64_t val;

            LogRel(("HWACCM: Host CR4=%08X\n", pVM->hwaccm.s.vmx.hostCR4));
            LogRel(("HWACCM: MSR_IA32_FEATURE_CONTROL      = %VX64\n", pVM->hwaccm.s.vmx.msr.feature_ctrl));
            LogRel(("HWACCM: MSR_IA32_VMX_BASIC_INFO       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_basic_info));
            LogRel(("HWACCM: VMCS id                       = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HWACCM: VMCS size                     = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HWACCM: VMCS physical address limit   = %s\n", MSR_IA32_VMX_BASIC_INFO_VMCS_PHYS_WIDTH(pVM->hwaccm.s.vmx.msr.vmx_basic_info) ? "< 4 GB" : "None"));
            LogRel(("HWACCM: VMCS memory type              = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_MEM_TYPE(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HWACCM: Dual monitor treatment        = %d\n", MSR_IA32_VMX_BASIC_INFO_VMCS_DUAL_MON(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));

            LogRel(("HWACCM: MSR_IA32_VMX_PINBASED_CTLS    = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.allowed1;
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT\n"));
            val = pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0;
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT *must* be set\n"));

            LogRel(("HWACCM: MSR_IA32_VMX_PROCBASED_CTLS   = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1;
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL\n"));

            val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.disallowed0;
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL *must* be set\n"));

            if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
            {
                LogRel(("HWACCM: MSR_IA32_VMX_PROCBASED_CTLS2  = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.u));
                val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT\n"));

                val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT *must* be set\n"));
            }

            LogRel(("HWACCM: MSR_IA32_VMX_ENTRY_CTLS       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_entry.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1;
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON\n"));
            val = pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0;
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON *must* be set\n"));

            LogRel(("HWACCM: MSR_IA32_VMX_EXIT_CTLS        = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_exit.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1;
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ\n"));
            val = pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0;
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64 *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ *must* be set\n"));

            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps)
            {
                LogRel(("HWACCM: MSR_IA32_VMX_EPT_VPID_CAPS    = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_eptcaps));

                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_RWX_X_ONLY)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_RWX_X_ONLY\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_RWX_W_ONLY)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_RWX_W_ONLY\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_RWX_WX_ONLY)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_RWX_WX_ONLY\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_GAW_21_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_GAW_21_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_GAW_30_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_GAW_30_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_GAW_39_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_GAW_39_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_GAW_48_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_GAW_48_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_GAW_57_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_GAW_57_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_EMT_UC)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_EMT_UC\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_EMT_WC)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_EMT_WC\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_EMT_WT)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_EMT_WT\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_EMT_WP)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_EMT_WP\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_EMT_WB)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_EMT_WB\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_SP_21_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_SP_21_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_SP_30_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_SP_30_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_SP_39_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_SP_39_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_SP_48_BITS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_SP_48_BITS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVEPT\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_INDIV)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_INDIV\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_CONTEXT)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_CONTEXT\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_ALL)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_ALL\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_CONTEXT)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_CONTEXT\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_ALL)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_ALL\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_CONTEXT_GLOBAL)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_CONTEXT_GLOBAL\n"));
            }

            LogRel(("HWACCM: MSR_IA32_VMX_MISC             = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_misc));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_ACTIVITY_STATES %x\n", MSR_IA32_VMX_MISC_ACTIVITY_STATES(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_CR3_TARGET      %x\n", MSR_IA32_VMX_MISC_CR3_TARGET(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_MAX_MSR         %x\n", MSR_IA32_VMX_MISC_MAX_MSR(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_MSEG_ID         %x\n", MSR_IA32_VMX_MISC_MSEG_ID(pVM->hwaccm.s.vmx.msr.vmx_misc)));

            LogRel(("HWACCM: MSR_IA32_VMX_CR0_FIXED0       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0));
            LogRel(("HWACCM: MSR_IA32_VMX_CR0_FIXED1       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1));
            LogRel(("HWACCM: MSR_IA32_VMX_CR4_FIXED0       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0));
            LogRel(("HWACCM: MSR_IA32_VMX_CR4_FIXED1       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1));
            LogRel(("HWACCM: MSR_IA32_VMX_VMCS_ENUM        = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_vmcs_enum));

            LogRel(("HWACCM: VMCS physaddr                 = %VHp\n", pVM->hwaccm.s.vmx.pVMCSPhys));
            LogRel(("HWACCM: TPR shadow physaddr           = %VHp\n", pVM->hwaccm.s.vmx.pAPICPhys));
            LogRel(("HWACCM: MSR bitmap physaddr           = %VHp\n", pVM->hwaccm.s.vmx.pMSRBitmapPhys));

#ifdef HWACCM_VTX_WITH_EPT
            if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                pVM->hwaccm.s.fNestedPaging = pVM->hwaccm.s.fAllowNestedPaging;
#endif

            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            /* Allocate three pages for the TSS we need for real mode emulation. (2 page for the IO bitmap) */
            rc = PDMR3VMMDevHeapAlloc(pVM, HWACCM_VTX_TSS_SIZE, (RTR3PTR *)&pVM->hwaccm.s.vmx.pRealModeTSS);
            AssertRC(rc);
            if (RT_FAILURE(rc))
                return rc;

            /* The I/O bitmap starts right after the virtual interrupt redirection bitmap. */
            ASMMemZero32(pVM->hwaccm.s.vmx.pRealModeTSS, sizeof(*pVM->hwaccm.s.vmx.pRealModeTSS));
            pVM->hwaccm.s.vmx.pRealModeTSS->offIoBitmap = sizeof(*pVM->hwaccm.s.vmx.pRealModeTSS);
            /* Bit set to 0 means redirection enabled. */
            memset(pVM->hwaccm.s.vmx.pRealModeTSS->IntRedirBitmap, 0x0, sizeof(pVM->hwaccm.s.vmx.pRealModeTSS->IntRedirBitmap));
            /* Allow all port IO, so the VT-x IO intercepts do their job. */
            memset(pVM->hwaccm.s.vmx.pRealModeTSS + 1, 0, PAGE_SIZE*2);
            *((unsigned char *)pVM->hwaccm.s.vmx.pRealModeTSS + HWACCM_VTX_TSS_SIZE - 2) = 0xff;

            rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                pVM->fHWACCMEnabled = true;
                pVM->hwaccm.s.vmx.fEnabled = true;
                hwaccmR3DisableRawMode(pVM);

                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);            /* 64 bits only on Intel CPUs */
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NXE);
#endif
                LogRel(("HWACCM: VMX enabled!\n"));
                if (pVM->hwaccm.s.fNestedPaging)
                    LogRel(("HWACCM:    Enabled nested paging\n"));
            }
            else
            {
                LogRel(("HWACCM: VMX setup failed with rc=%Vrc!\n", rc));
                LogRel(("HWACCM: Last instruction error %x\n", pVM->hwaccm.s.vmx.ulLastInstrError));
                pVM->fHWACCMEnabled = false;
            }
        }
    }
    else
    if (pVM->hwaccm.s.svm.fSupported)
    {
        Log(("pVM->hwaccm.s.svm.fSupported = %d\n", pVM->hwaccm.s.svm.fSupported));

        if (pVM->hwaccm.s.fInitialized == false)
        {
            /* Erratum 170 which requires a forced TLB flush for each world switch:
             * See http://www.amd.com/us-en/assets/content_type/white_papers_and_tech_docs/33610.pdf
             *
             * All BH-G1/2 and DH-G1/2 models include a fix:
             * Athlon X2:   0x6b 1/2
             *              0x68 1/2
             * Athlon 64:   0x7f 1
             *              0x6f 2
             * Sempron:     0x7f 1/2
             *              0x6f 2
             *              0x6c 2
             *              0x7c 2
             * Turion 64:   0x68 2
             *
             */
            uint32_t u32Dummy;
            uint32_t u32Version, u32Family, u32Model, u32Stepping, u32BaseFamily;
            ASMCpuId(1, &u32Version, &u32Dummy, &u32Dummy, &u32Dummy);
            u32BaseFamily= (u32Version >> 8) & 0xf;
            u32Family    = u32BaseFamily + (u32BaseFamily == 0xf ? ((u32Version >> 20) & 0x7f) : 0);
            u32Model     = ((u32Version >> 4) & 0xf);
            u32Model     = u32Model | ((u32BaseFamily == 0xf ? (u32Version >> 16) & 0x0f : 0) << 4);
            u32Stepping  = u32Version & 0xf;
            if (    u32Family == 0xf
                &&  !((u32Model == 0x68 || u32Model == 0x6b || u32Model == 0x7f) &&  u32Stepping >= 1)
                &&  !((u32Model == 0x6f || u32Model == 0x6c || u32Model == 0x7c) &&  u32Stepping >= 2))
            {
                LogRel(("HWACMM: AMD cpu with erratum 170 family %x model %x stepping %x\n", u32Family, u32Model, u32Stepping));
            }

            LogRel(("HWACMM: cpuid 0x80000001.u32AMDFeatureECX = %VX32\n", pVM->hwaccm.s.cpuid.u32AMDFeatureECX));
            LogRel(("HWACMM: cpuid 0x80000001.u32AMDFeatureEDX = %VX32\n", pVM->hwaccm.s.cpuid.u32AMDFeatureEDX));
            LogRel(("HWACCM: SVM revision                      = %X\n", pVM->hwaccm.s.svm.u32Rev));
            LogRel(("HWACCM: SVM max ASID                      = %d\n", pVM->hwaccm.s.svm.u32MaxASID));
            LogRel(("HWACCM: SVM features                      = %X\n", pVM->hwaccm.s.svm.u32Features));

            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                LogRel(("HWACCM:    AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING\n"));
            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_LBR_VIRT)
                LogRel(("HWACCM:    AMD_CPUID_SVM_FEATURE_EDX_LBR_VIRT\n"));
            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_SVM_LOCK)
                LogRel(("HWACCM:    AMD_CPUID_SVM_FEATURE_EDX_SVM_LOCK\n"));
            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_NRIP_SAVE)
                LogRel(("HWACCM:    AMD_CPUID_SVM_FEATURE_EDX_NRIP_SAVE\n"));
            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_SSE_3_5_DISABLE)
                LogRel(("HWACCM:    AMD_CPUID_SVM_FEATURE_EDX_SSE_3_5_DISABLE\n"));

            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                pVM->hwaccm.s.fNestedPaging = pVM->hwaccm.s.fAllowNestedPaging;

            rc = SUPCallVMMR0Ex(pVM->pVMR0, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                pVM->fHWACCMEnabled = true;
                pVM->hwaccm.s.svm.fEnabled = true;

                if (pVM->hwaccm.s.fNestedPaging)
                    LogRel(("HWACCM:    Enabled nested paging\n"));

                hwaccmR3DisableRawMode(pVM);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NXE);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
#endif
            }
            else
            {
                pVM->fHWACCMEnabled = false;
            }
        }
    }
    return VINF_SUCCESS;
}

/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 */
HWACCMR3DECL(void) HWACCMR3Relocate(PVM pVM)
{
    Log(("HWACCMR3Relocate to %VGv\n", MMHyperGetArea(pVM, 0)));
    return;
}

/**
 * Checks hardware accelerated raw mode is allowed.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsAllowed(PVM pVM)
{
    return pVM->hwaccm.s.fAllowed;
}

/**
 * Notification callback which is called whenever there is a chance that a CR3
 * value might have changed.
 *
 * This is called by PGM.
 *
 * @param   pVM            The VM to operate on.
 * @param   enmShadowMode  New paging mode.
 */
HWACCMR3DECL(void) HWACCMR3PagingModeChanged(PVM pVM, PGMMODE enmShadowMode)
{
    pVM->hwaccm.s.enmShadowMode = enmShadowMode;
}

/**
 * Terminates the HWACCM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(int) HWACCMR3Term(PVM pVM)
{
    if (pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        PDMR3VMMDevHeapFree(pVM, pVM->hwaccm.s.vmx.pRealModeTSS);
        pVM->hwaccm.s.vmx.pRealModeTSS       = 0;
    }

    if (pVM->hwaccm.s.paStatExitReason)
    {
        MMHyperFree(pVM, pVM->hwaccm.s.paStatExitReason);
        pVM->hwaccm.s.paStatExitReason = NULL;
    }
    return 0;
}

/**
 * The VM is being reset.
 *
 * For the HWACCM component this means that any GDT/LDT/TSS monitors
 * needs to be removed.
 *
 * @param   pVM     VM handle.
 */
HWACCMR3DECL(void) HWACCMR3Reset(PVM pVM)
{
    LogFlow(("HWACCMR3Reset:\n"));

    if (pVM->fHWACCMEnabled)
        hwaccmR3DisableRawMode(pVM);

    /* On first entry we'll sync everything. */
    pVM->hwaccm.s.fContextUseFlags = HWACCM_CHANGED_ALL;

    pVM->hwaccm.s.vmx.cr0_mask = 0;
    pVM->hwaccm.s.vmx.cr4_mask = 0;

    pVM->hwaccm.s.Event.fPending = false;

    /* Reset state information for real-mode emulation in VT-x. */
    pVM->hwaccm.s.vmx.RealMode.Event.fPending = false;
    memset(&pVM->hwaccm.s.vmx.RealMode, 0, sizeof(pVM->hwaccm.s.vmx.RealMode));
    pVM->hwaccm.s.vmx.enmCurrGuestMode = PGMMODE_REAL;
}

/**
 * Checks if we can currently use hardware accelerated raw mode.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Partial VM execution context
 */
HWACCMR3DECL(bool) HWACCMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx)
{
    Assert(pVM->fHWACCMEnabled);

    /* AMD SVM supports real & protected mode with or without paging. */
    if (pVM->hwaccm.s.svm.fEnabled)
    {
        pVM->hwaccm.s.fActive = true;
        return true;
    }

    pVM->hwaccm.s.fActive = false;

    /* Note! The context supplied by REM is partial. If we add more checks here, be sure to verify that REM provides this info! */
#ifdef HWACCM_VMX_EMULATE_REALMODE
    if (CPUMIsGuestInRealModeEx(pCtx))
    {
        /* VT-x will not allow high selector bases in v86 mode; fall back to the recompiler in that case. */
        if (    pCtx->dsHid.u64Base > 0xfffff
            ||  pCtx->esHid.u64Base > 0xfffff
            ||  pCtx->fsHid.u64Base > 0xfffff
            ||  pCtx->gsHid.u64Base > 0xfffff)
            return false;
    }
#else
    if (!CPUMIsGuestInLongModeEx(pCtx))
    {
        /* Too early for VT-x; Solaris guests will fail with a guru meditation otherwise; same for XP. */
        if (pCtx->idtr.pIdt == 0 || pCtx->idtr.cbIdt == 0 || pCtx->tr == 0)
            return false;

        /* The guest is about to complete the switch to protected mode. Wait a bit longer. */
        /* Windows XP; switch to protected mode; all selectors are marked not present in the
         * hidden registers (possible recompiler bug; see load_seg_vm) */
        if (pCtx->csHid.Attr.n.u1Present == 0)
            return false;
        if (pCtx->ssHid.Attr.n.u1Present == 0)
            return false;
    }
#endif

    if (pVM->hwaccm.s.vmx.fEnabled)
    {
        uint32_t mask;

        /* if bit N is set in cr0_fixed0, then it must be set in the guest's cr0. */
        mask = (uint32_t)pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0;
        /* Note: We ignore the NE bit here on purpose; see vmmr0\hwaccmr0.cpp for details. */
        mask &= ~X86_CR0_NE;

#ifdef HWACCM_VMX_EMULATE_REALMODE
        /* Note: We ignore the PE & PG bits here on purpose; we emulate real and protected mode without paging. */
        mask &= ~(X86_CR0_PG|X86_CR0_PE);
#else
        /* We support protected mode without paging using identity mapping. */
        mask &= ~X86_CR0_PG;
#endif
        if ((pCtx->cr0 & mask) != mask)
            return false;

        /* if bit N is cleared in cr0_fixed1, then it must be zero in the guest's cr0. */
        mask = (uint32_t)~pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1;
        if ((pCtx->cr0 & mask) != 0)
            return false;

        /* if bit N is set in cr4_fixed0, then it must be set in the guest's cr4. */
        mask  = (uint32_t)pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0;
        mask &= ~X86_CR4_VMXE;
        if ((pCtx->cr4 & mask) != mask)
            return false;

        /* if bit N is cleared in cr4_fixed1, then it must be zero in the guest's cr4. */
        mask = (uint32_t)~pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1;
        if ((pCtx->cr4 & mask) != 0)
            return false;

        pVM->hwaccm.s.fActive = true;
        return true;
    }

    return false;
}

/**
 * Checks if we are currently using hardware accelerated raw mode.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsActive(PVM pVM)
{
    return pVM->hwaccm.s.fActive;
}

/**
 * Checks if we are currently using nested paging.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsNestedPagingActive(PVM pVM)
{
    return pVM->hwaccm.s.fNestedPaging;
}

/**
 * Checks if internal events are pending. In that case we are not allowed to dispatch interrupts.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
HWACCMR3DECL(bool) HWACCMR3IsEventPending(PVM pVM)
{
    return HWACCMIsEnabled(pVM) && pVM->hwaccm.s.Event.fPending;
}

/**
 * Check fatal VT-x/AMD-V error and produce some meaningful
 * log release message.
 *
 * @param   pVM         The VM to operate on.
 * @param   iStatusCode VBox status code
 */
HWACCMR3DECL(void) HWACCMR3CheckError(PVM pVM, int iStatusCode)
{
    switch(iStatusCode)
    {
    case VERR_VMX_INVALID_VMCS_FIELD:
        break;

    case VERR_VMX_INVALID_VMCS_PTR:
        LogRel(("VERR_VMX_INVALID_VMCS_PTR: Current pointer %VGp vs %VGp\n", pVM->hwaccm.s.vmx.lasterror.u64VMCSPhys, pVM->hwaccm.s.vmx.pVMCSPhys));
        LogRel(("VERR_VMX_INVALID_VMCS_PTR: Current VMCS version %x\n", pVM->hwaccm.s.vmx.lasterror.ulVMCSRevision));
        break;

    case VERR_VMX_INVALID_VMXON_PTR:
        break;
    }
}

/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) hwaccmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    int rc;

    Log(("hwaccmR3Save:\n"));

    /*
     * Save the basic bits - fortunately all the other things can be resynced on load.
     */
    rc = SSMR3PutU32(pSSM, pVM->hwaccm.s.Event.fPending);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pVM->hwaccm.s.Event.errCode);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU64(pSSM, pVM->hwaccm.s.Event.intInfo);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}

/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             VM Handle.
 * @param   pSSM            SSM operation handle.
 * @param   u32Version      Data layout version.
 */
static DECLCALLBACK(int) hwaccmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t u32Version)
{
    int rc;

    Log(("hwaccmR3Load:\n"));

    /*
     * Validate version.
     */
    if (u32Version != HWACCM_SSM_VERSION)
    {
        AssertMsgFailed(("hwaccmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    rc = SSMR3GetU32(pSSM, &pVM->hwaccm.s.Event.fPending);
    AssertRCReturn(rc, rc);
    rc = SSMR3GetU32(pSSM, &pVM->hwaccm.s.Event.errCode);
    AssertRCReturn(rc, rc);
    rc = SSMR3GetU64(pSSM, &pVM->hwaccm.s.Event.intInfo);
    AssertRCReturn(rc, rc);

    return VINF_SUCCESS;
}




