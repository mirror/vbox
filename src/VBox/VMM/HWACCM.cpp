/** @file
 *
 * HWACCM - Intel/AMD VM Hardware Support Manager
 */

/*
 * Copyright (C) 2006 InnoTek Systemberatung GmbH
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License as published by the Free Software Foundation,
 * in version 2 as it comes in the "COPYING" file of the VirtualBox OSE
 * distribution. VirtualBox OSE is distributed in the hope that it will
 * be useful, but WITHOUT ANY WARRANTY of any kind.
 *
 * If you received this file as part of a commercial VirtualBox
 * distribution, then only the terms of your commercial VirtualBox
 * license agreement apply instead of the previous paragraph.
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
#include "x86context.h"

#include <string.h>


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

    /** @todo Make sure both pages are either not accessible or readonly! */
    /* Allocate one page for VMXON. */
    pVM->hwaccm.s.vmx.pVMXON = SUPContAlloc(PAGE_SIZE, &pVM->hwaccm.s.vmx.pVMXONPhys);
    if (pVM->hwaccm.s.vmx.pVMXON == 0)
    {
        AssertMsgFailed(("SUPContAlloc failed!!\n"));
        return VERR_NO_MEMORY;
    }
    memset(pVM->hwaccm.s.vmx.pVMXON, 0, PAGE_SIZE);

    /* Allocate one page for the VM control structure (VMCS). */
    pVM->hwaccm.s.vmx.pVMCS = SUPContAlloc(PAGE_SIZE, &pVM->hwaccm.s.vmx.pVMCSPhys);
    if (pVM->hwaccm.s.vmx.pVMCS == 0)
    {
        AssertMsgFailed(("SUPContAlloc failed!!\n"));
        return VERR_NO_MEMORY;
    }
    memset(pVM->hwaccm.s.vmx.pVMCS, 0, PAGE_SIZE);

    /* Reuse those two pages for AMD SVM. (one is active; never both) */
    pVM->hwaccm.s.svm.pHState     = pVM->hwaccm.s.vmx.pVMXON;
    pVM->hwaccm.s.svm.pHStatePhys = pVM->hwaccm.s.vmx.pVMXONPhys;
    pVM->hwaccm.s.svm.pVMCB       = pVM->hwaccm.s.vmx.pVMCS;
    pVM->hwaccm.s.svm.pVMCBPhys   = pVM->hwaccm.s.vmx.pVMCSPhys;

    /* Allocate one page for the SVM host control structure (used for vmsave/vmload). */
    pVM->hwaccm.s.svm.pVMCBHost = SUPContAlloc(PAGE_SIZE, &pVM->hwaccm.s.svm.pVMCBHostPhys);
    if (pVM->hwaccm.s.svm.pVMCBHost == 0)
    {
        AssertMsgFailed(("SUPContAlloc failed!!\n"));
        return VERR_NO_MEMORY;
    }
    memset(pVM->hwaccm.s.svm.pVMCBHost, 0, PAGE_SIZE);

    /* Allocate 12 KB for the IO bitmap (doesn't seem to be a way to convince SVM not to use it) */
    pVM->hwaccm.s.svm.pIOBitmap = SUPContAlloc(PAGE_SIZE*3, &pVM->hwaccm.s.svm.pIOBitmapPhys);
    if (pVM->hwaccm.s.svm.pIOBitmap == 0)
    {
        AssertMsgFailed(("SUPContAlloc failed!!\n"));
        return VERR_NO_MEMORY;
    }
    /* Set all bits to intercept all IO accesses. */
    memset(pVM->hwaccm.s.svm.pIOBitmap, 0xff, PAGE_SIZE*3);

    /* Allocate 8 KB for the MSR bitmap (doesn't seem to be a way to convince SVM not to use it) */
    pVM->hwaccm.s.svm.pMSRBitmap = SUPContAlloc(PAGE_SIZE*2, &pVM->hwaccm.s.svm.pMSRBitmapPhys);
    if (pVM->hwaccm.s.svm.pMSRBitmap == 0)
    {
        AssertMsgFailed(("SUPContAlloc failed!!\n"));
        return VERR_NO_MEMORY;
    }
    /* Set all bits to intercept all MSR accesses. */
    memset(pVM->hwaccm.s.svm.pMSRBitmap, 0xff, PAGE_SIZE*2);

    /* Misc initialisation. */
    pVM->hwaccm.s.vmx.fSupported = false;
    pVM->hwaccm.s.svm.fSupported = false;
    pVM->hwaccm.s.vmx.fEnabled   = false;
    pVM->hwaccm.s.svm.fEnabled   = false;

    pVM->hwaccm.s.fActive        = false;

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
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitInvpg,     STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Invlpg",      STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitInvd,      STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Invd",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCpuid,     STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/Cpuid",       STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCRxWrite,  STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/CRx/Write",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCRxRead,   STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/CRx/Read",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitDRxWrite,  STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/DRx/Write",   STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitDRxRead,   STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/DRx/Read",    STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitCLTS,      STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/CLTS",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitLMSW,      STAMTYPE_COUNTER, "/HWACCM/Exit/Instr/LMSW",        STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIOWrite,   STAMTYPE_COUNTER, "/HWACCM/Exit/IO/Write",          STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIORead,    STAMTYPE_COUNTER, "/HWACCM/Exit/IO/Read",           STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatExitIrqWindow, STAMTYPE_COUNTER, "/HWACCM/Exit/GuestIrq/Pending",  STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatSwitchGuestIrq,STAMTYPE_COUNTER, "/HWACCM/Switch/IrqPending",      STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatSwitchToR3,    STAMTYPE_COUNTER, "/HWACCM/Switch/ToR3",            STAMUNIT_OCCURENCES,    "Nr of occurances");

    STAM_REG(pVM, &pVM->hwaccm.s.StatIntInject,     STAMTYPE_COUNTER, "/HWACCM/Irq/Inject",             STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatIntReinject,   STAMTYPE_COUNTER, "/HWACCM/Irq/Reinject",           STAMUNIT_OCCURENCES,    "Nr of occurances");
    STAM_REG(pVM, &pVM->hwaccm.s.StatPendingHostIrq,STAMTYPE_COUNTER, "/HWACCM/Irq/PendingOnHost",      STAMUNIT_OCCURENCES,    "Nr of occurances");

    pVM->hwaccm.s.pStatExitReason = 0;

#ifdef VBOX_WITH_STATISTICS
    rc = MMHyperAlloc(pVM, MAX_EXITREASON_STAT*sizeof(*pVM->hwaccm.s.pStatExitReason), 0, MM_TAG_HWACCM, (void **)&pVM->hwaccm.s.pStatExitReason);
    AssertRC(rc);
    if (VBOX_SUCCESS(rc))
    {
        for (int i=0;i<MAX_EXITREASON_STAT;i++)
        {
            char szName[64];
            RTStrPrintf(szName, sizeof(szName), "/HWACCM/Exit/Reason/%02x", i);
            int rc = STAMR3Register(pVM, &pVM->hwaccm.s.pStatExitReason[i], STAMTYPE_COUNTER, STAMVISIBILITY_USED, szName, STAMUNIT_OCCURENCES, "Exit reason");
            AssertRC(rc);
        }
    }
#endif

    /* Disabled by default. */
    pVM->fHWACCMEnabled = false;

    /* HWACCM support must be explicitely enabled in the configuration file. */
    pVM->hwaccm.s.fAllowed = false;
    CFGMR3QueryBool(CFGMR3GetChild(CFGMR3GetRoot(pVM), "HWVirtExt/"), "Enabled", &pVM->hwaccm.s.fAllowed);

    return VINF_SUCCESS;
}


/**
 * Turns off normal raw mode features
 *
 * @param   pVM         The VM to operate on.
 */
static void hwaccmr3DisableRawMode(PVM pVM)
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
    PGMR3RemoveMappingsFromShwPD(pVM);
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
#ifdef LOG_ENABLED
    Log(("HWACCMR3Relocate to %VGv\n", MMHyperGetArea(pVM, 0)));
#endif

    if (pVM->hwaccm.s.fAllowed == false)
        return ;

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        Log(("pVM->hwaccm.s.vmx.fSupported = %d\n", pVM->hwaccm.s.vmx.fSupported));
        LogRel(("HWACCM: Host CR4=%08X\n", pVM->hwaccm.s.vmx.hostCR4));
        LogRel(("HWACCM: MSR_IA32_FEATURE_CONTROL      = %VX64\n", pVM->hwaccm.s.vmx.msr.feature_ctrl));
        LogRel(("HWACCM: MSR_IA32_VMX_BASIC_INFO       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_basic_info));
        LogRel(("HWACCM: MSR_IA32_VMX_PINBASED_CTLS    = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_pin_ctls));
        LogRel(("HWACCM: MSR_IA32_VMX_PROCBASED_CTLS   = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_proc_ctls));
        LogRel(("HWACCM: MSR_IA32_VMX_EXIT_CTLS        = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_exit));
        LogRel(("HWACCM: MSR_IA32_VMX_ENTRY_CTLS       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_entry));
        LogRel(("HWACCM: MSR_IA32_VMX_MISC             = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_misc));
        LogRel(("HWACCM: MSR_IA32_VMX_CR0_FIXED0       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0));
        LogRel(("HWACCM: MSR_IA32_VMX_CR0_FIXED1       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1));
        LogRel(("HWACCM: MSR_IA32_VMX_CR4_FIXED0       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0));
        LogRel(("HWACCM: MSR_IA32_VMX_CR4_FIXED1       = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1));
        LogRel(("HWACCM: MSR_IA32_VMX_VMCS_ENUM        = %VX64\n", pVM->hwaccm.s.vmx.msr.vmx_vmcs_enum));

        if (pVM->hwaccm.s.fInitialized == false && pVM->hwaccm.s.vmx.msr.feature_ctrl != 0)
        {
            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            int rc = SUPCallVMMR0(pVM, VMMR0_HWACC_SETUP_VM, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                hwaccmr3DisableRawMode(pVM);

                pVM->fHWACCMEnabled = true;
                pVM->hwaccm.s.vmx.fEnabled = true;
                LogRel(("HWACCM: VMX enabled!\n"));
            }
            else
            {
                LogRel(("HWACCM: VMX setup failed with rc=%Vrc!\n", rc));
                pVM->fHWACCMEnabled = false;
            }
        }
    }
    else
    if (pVM->hwaccm.s.svm.fSupported)
    {
        Log(("pVM->hwaccm.s.svm.fSupported = %d\n", pVM->hwaccm.s.svm.fSupported));
        LogRel(("HWACMM: cpuid 0x80000001.u32AMDFeatureECX = %VX32\n", pVM->hwaccm.s.cpuid.u32AMDFeatureECX));
        LogRel(("HWACMM: cpuid 0x80000001.u32AMDFeatureEDX = %VX32\n", pVM->hwaccm.s.cpuid.u32AMDFeatureEDX));
        LogRel(("HWACCM: SVM revision                      = %X\n", pVM->hwaccm.s.svm.u32Rev));
        LogRel(("HWACCM: SVM max ASID                      = %d\n", pVM->hwaccm.s.svm.u32MaxASID));

        if (pVM->hwaccm.s.fInitialized == false)
        {
            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            int rc = SUPCallVMMR0(pVM, VMMR0_HWACC_SETUP_VM, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
#if 1
                LogRel(("HWACCM: SVM supported; disabled currently\n"));
#else
                hwaccmr3DisableRawMode(pVM);

                pVM->fHWACCMEnabled = true;
                pVM->hwaccm.s.svm.fEnabled = true;
#endif
            }
            else
            {
                pVM->fHWACCMEnabled = false;
            }
        }
    }

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
    if (pVM->hwaccm.s.pStatExitReason)
    {
        MMHyperFree(pVM, pVM->hwaccm.s.pStatExitReason);
        pVM->hwaccm.s.pStatExitReason = 0;
    }

    if (pVM->hwaccm.s.vmx.pVMXON)
    {
        SUPContFree(pVM->hwaccm.s.vmx.pVMXON);
        pVM->hwaccm.s.vmx.pVMXON = 0;
    }
    if (pVM->hwaccm.s.vmx.pVMCS)
    {
        SUPContFree(pVM->hwaccm.s.vmx.pVMCS);
        pVM->hwaccm.s.vmx.pVMCS = 0;
    }
    if (pVM->hwaccm.s.svm.pVMCBHost)
    {
        SUPContFree(pVM->hwaccm.s.svm.pVMCBHost);
        pVM->hwaccm.s.svm.pVMCBHost = 0;
    }
    if (pVM->hwaccm.s.svm.pIOBitmap)
    {
        SUPContFree(pVM->hwaccm.s.svm.pIOBitmap);
        pVM->hwaccm.s.svm.pIOBitmap = 0;
    }
    if (pVM->hwaccm.s.svm.pMSRBitmap)
    {
        SUPContFree(pVM->hwaccm.s.svm.pMSRBitmap);
        pVM->hwaccm.s.svm.pMSRBitmap = 0;
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
        hwaccmr3DisableRawMode(pVM);

    /* On first entry we'll sync everything. */
    pVM->hwaccm.s.fContextUseFlags = HWACCM_CHANGED_ALL;

    pVM->hwaccm.s.vmx.cr0_mask = 0;
    pVM->hwaccm.s.vmx.cr4_mask = 0;
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
    uint32_t mask;

    Assert(pVM->fHWACCMEnabled);

    /* @todo we can support real-mode by using v86 and protected mode without paging with identity mapped pages.
     * (but do we really care?)
     */

    pVM->hwaccm.s.fActive = false;

    /** @note The context supplied by REM is partial. If we add more checks here, be sure to verify that REM provides this info! */

    /* Too early for VMX and SVN (?). */
    if (pCtx->idtr.pIdt == 0 || pCtx->idtr.cbIdt == 0 || pCtx->tr == 0)
        return false;

    /* The guest is about to complete the switch to protected mode. Wait a bit longer. */
    if (pCtx->csHid.Attr.n.u1Present == 0)
        return false;
    if (pCtx->ssHid.Attr.n.u1Present == 0)
        return false;

    /** @todo if we remove this check, then Windows XP install fails during the textmode phase */
    if (!(pCtx->cr0 & X86_CR0_WRITE_PROTECT))
        return false;

    if (pVM->hwaccm.s.vmx.fEnabled)
    {
        /* if bit N is set in cr0_fixed0, then it must be set in the guest's cr0. */
        mask = (uint32_t)pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0;
        /** @note We ignore the NE bit here on purpose; see vmmr0\hwaccmr0.cpp for details. */
        mask &= ~X86_CR0_NE;

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
    else
    {
        Assert(pVM->hwaccm.s.svm.fEnabled);

        /* Let's start with protected mode with paging enabled first. */
        if ((pCtx->cr0 & (X86_CR0_PE|X86_CR0_PG)) == (X86_CR0_PE|X86_CR0_PG))
        {
            pVM->hwaccm.s.fActive = true;
            return true;
        }
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
        Log(("hwaccmR3Load: Invalid version u32Version=%d!\n", u32Version));
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




