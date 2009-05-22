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
#include <VBox/patm.h>
#include <VBox/csam.h>
#include <VBox/selm.h>
#include <VBox/rem.h>
#include <VBox/hwacc_vmx.h>
#include <VBox/hwacc_svm.h>
#include "HWACCMInternal.h"
#include <VBox/vm.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/string.h>
#include <iprt/thread.h>

/*******************************************************************************
*   Global Variables                                                           *
*******************************************************************************/
#ifdef VBOX_WITH_STATISTICS
# define EXIT_REASON(def, val, str) #def " - " #val " - " str
# define EXIT_REASON_NIL() NULL
/** Exit reason descriptions for VT-x, used to describe statistics. */
static const char * const g_apszVTxExitReasons[MAX_EXITREASON_STAT] =
{
    EXIT_REASON(VMX_EXIT_EXCEPTION          ,  0, "Exception or non-maskable interrupt (NMI)."),
    EXIT_REASON(VMX_EXIT_EXTERNAL_IRQ       ,  1, "External interrupt."),
    EXIT_REASON(VMX_EXIT_TRIPLE_FAULT       ,  2, "Triple fault."),
    EXIT_REASON(VMX_EXIT_INIT_SIGNAL        ,  3, "INIT signal."),
    EXIT_REASON(VMX_EXIT_SIPI               ,  4, "Start-up IPI (SIPI)."),
    EXIT_REASON(VMX_EXIT_IO_SMI_IRQ         ,  5, "I/O system-management interrupt (SMI)."),
    EXIT_REASON(VMX_EXIT_SMI_IRQ            ,  6, "Other SMI."),
    EXIT_REASON(VMX_EXIT_IRQ_WINDOW         ,  7, "Interrupt window."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_TASK_SWITCH        ,  9, "Task switch."),
    EXIT_REASON(VMX_EXIT_CPUID              , 10, "Guest software attempted to execute CPUID."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_HLT                , 12, "Guest software attempted to execute HLT."),
    EXIT_REASON(VMX_EXIT_INVD               , 13, "Guest software attempted to execute INVD."),
    EXIT_REASON(VMX_EXIT_INVPG              , 14, "Guest software attempted to execute INVPG."),
    EXIT_REASON(VMX_EXIT_RDPMC              , 15, "Guest software attempted to execute RDPMC."),
    EXIT_REASON(VMX_EXIT_RDTSC              , 16, "Guest software attempted to execute RDTSC."),
    EXIT_REASON(VMX_EXIT_RSM                , 17, "Guest software attempted to execute RSM in SMM."),
    EXIT_REASON(VMX_EXIT_VMCALL             , 18, "Guest software executed VMCALL."),
    EXIT_REASON(VMX_EXIT_VMCLEAR            , 19, "Guest software executed VMCLEAR."),
    EXIT_REASON(VMX_EXIT_VMLAUNCH           , 20, "Guest software executed VMLAUNCH."),
    EXIT_REASON(VMX_EXIT_VMPTRLD            , 21, "Guest software executed VMPTRLD."),
    EXIT_REASON(VMX_EXIT_VMPTRST            , 22, "Guest software executed VMPTRST."),
    EXIT_REASON(VMX_EXIT_VMREAD             , 23, "Guest software executed VMREAD."),
    EXIT_REASON(VMX_EXIT_VMRESUME           , 24, "Guest software executed VMRESUME."),
    EXIT_REASON(VMX_EXIT_VMWRITE            , 25, "Guest software executed VMWRITE."),
    EXIT_REASON(VMX_EXIT_VMXOFF             , 26, "Guest software executed VMXOFF."),
    EXIT_REASON(VMX_EXIT_VMXON              , 27, "Guest software executed VMXON."),
    EXIT_REASON(VMX_EXIT_CRX_MOVE           , 28, "Control-register accesses."),
    EXIT_REASON(VMX_EXIT_DRX_MOVE           , 29, "Debug-register accesses."),
    EXIT_REASON(VMX_EXIT_PORT_IO            , 30, "I/O instruction."),
    EXIT_REASON(VMX_EXIT_RDMSR              , 31, "RDMSR. Guest software attempted to execute RDMSR."),
    EXIT_REASON(VMX_EXIT_WRMSR              , 32, "WRMSR. Guest software attempted to execute WRMSR."),
    EXIT_REASON(VMX_EXIT_ERR_INVALID_GUEST_STATE,  33, "VM-entry failure due to invalid guest state."),
    EXIT_REASON(VMX_EXIT_ERR_MSR_LOAD       , 34, "VM-entry failure due to MSR loading."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_MWAIT              , 36, "Guest software executed MWAIT."),
    EXIT_REASON_NIL(),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_MONITOR            , 39, "Guest software attempted to execute MONITOR."),
    EXIT_REASON(VMX_EXIT_PAUSE              , 40, "Guest software attempted to execute PAUSE."),
    EXIT_REASON(VMX_EXIT_ERR_MACHINE_CHECK  , 41, "VM-entry failure due to machine-check."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_TPR                , 43, "TPR below threshold. Guest software executed MOV to CR8."),
    EXIT_REASON(VMX_EXIT_APIC_ACCESS        , 44, "APIC access. Guest software attempted to access memory at a physical address on the APIC-access page."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_XDTR_ACCESS        , 46, "Access to GDTR or IDTR. Guest software attempted to execute LGDT, LIDT, SGDT, or SIDT."),
    EXIT_REASON(VMX_EXIT_TR_ACCESS          , 47, "Access to LDTR or TR. Guest software attempted to execute LLDT, LTR, SLDT, or STR."),
    EXIT_REASON(VMX_EXIT_EPT_VIOLATION      , 48, "EPT violation. An attempt to access memory with a guest-physical address was disallowed by the configuration of the EPT paging structures."),
    EXIT_REASON(VMX_EXIT_EPT_MISCONFIG      , 49, "EPT misconfiguration. An attempt to access memory with a guest-physical address encountered a misconfigured EPT paging-structure entry."),
    EXIT_REASON(VMX_EXIT_INVEPT             , 50, "INVEPT. Guest software attempted to execute INVEPT."),
    EXIT_REASON_NIL(),
    EXIT_REASON(VMX_EXIT_PREEMPTION_TIMER   , 52, "VMX-preemption timer expired. The preemption timer counted down to zero."),
    EXIT_REASON(VMX_EXIT_INVVPID            , 53, "INVVPID. Guest software attempted to execute INVVPID."),
    EXIT_REASON(VMX_EXIT_WBINVD             , 54, "WBINVD. Guest software attempted to execute WBINVD."),
    EXIT_REASON(VMX_EXIT_XSETBV             , 55, "XSETBV. Guest software attempted to execute XSETBV."),
    EXIT_REASON_NIL()
};
/** Exit reason descriptions for AMD-V, used to describe statistics. */
static const char * const g_apszAmdVExitReasons[MAX_EXITREASON_STAT] =
{
    EXIT_REASON(SVM_EXIT_READ_CR0                   ,  0, "Read CR0."),
    EXIT_REASON(SVM_EXIT_READ_CR1                   ,  1, "Read CR1."),
    EXIT_REASON(SVM_EXIT_READ_CR2                   ,  2, "Read CR2."),
    EXIT_REASON(SVM_EXIT_READ_CR3                   ,  3, "Read CR3."),
    EXIT_REASON(SVM_EXIT_READ_CR4                   ,  4, "Read CR4."),
    EXIT_REASON(SVM_EXIT_READ_CR5                   ,  5, "Read CR5."),
    EXIT_REASON(SVM_EXIT_READ_CR6                   ,  6, "Read CR6."),
    EXIT_REASON(SVM_EXIT_READ_CR7                   ,  7, "Read CR7."),
    EXIT_REASON(SVM_EXIT_READ_CR8                   ,  8, "Read CR8."),
    EXIT_REASON(SVM_EXIT_READ_CR9                   ,  9, "Read CR9."),
    EXIT_REASON(SVM_EXIT_READ_CR10                  , 10, "Read CR10."),
    EXIT_REASON(SVM_EXIT_READ_CR11                  , 11, "Read CR11."),
    EXIT_REASON(SVM_EXIT_READ_CR12                  , 12, "Read CR12."),
    EXIT_REASON(SVM_EXIT_READ_CR13                  , 13, "Read CR13."),
    EXIT_REASON(SVM_EXIT_READ_CR14                  , 14, "Read CR14."),
    EXIT_REASON(SVM_EXIT_READ_CR15                  , 15, "Read CR15."),
    EXIT_REASON(SVM_EXIT_WRITE_CR0                  , 16, "Write CR0."),
    EXIT_REASON(SVM_EXIT_WRITE_CR1                  , 17, "Write CR1."),
    EXIT_REASON(SVM_EXIT_WRITE_CR2                  , 18, "Write CR2."),
    EXIT_REASON(SVM_EXIT_WRITE_CR3                  , 19, "Write CR3."),
    EXIT_REASON(SVM_EXIT_WRITE_CR4                  , 20, "Write CR4."),
    EXIT_REASON(SVM_EXIT_WRITE_CR5                  , 21, "Write CR5."),
    EXIT_REASON(SVM_EXIT_WRITE_CR6                  , 22, "Write CR6."),
    EXIT_REASON(SVM_EXIT_WRITE_CR7                  , 23, "Write CR7."),
    EXIT_REASON(SVM_EXIT_WRITE_CR8                  , 24, "Write CR8."),
    EXIT_REASON(SVM_EXIT_WRITE_CR9                  , 25, "Write CR9."),
    EXIT_REASON(SVM_EXIT_WRITE_CR10                 , 26, "Write CR10."),
    EXIT_REASON(SVM_EXIT_WRITE_CR11                 , 27, "Write CR11."),
    EXIT_REASON(SVM_EXIT_WRITE_CR12                 , 28, "Write CR12."),
    EXIT_REASON(SVM_EXIT_WRITE_CR13                 , 29, "Write CR13."),
    EXIT_REASON(SVM_EXIT_WRITE_CR14                 , 30, "Write CR14."),
    EXIT_REASON(SVM_EXIT_WRITE_CR15                 , 31, "Write CR15."),
    EXIT_REASON(SVM_EXIT_READ_DR0                   , 32, "Read DR0."),
    EXIT_REASON(SVM_EXIT_READ_DR1                   , 33, "Read DR1."),
    EXIT_REASON(SVM_EXIT_READ_DR2                   , 34, "Read DR2."),
    EXIT_REASON(SVM_EXIT_READ_DR3                   , 35, "Read DR3."),
    EXIT_REASON(SVM_EXIT_READ_DR4                   , 36, "Read DR4."),
    EXIT_REASON(SVM_EXIT_READ_DR5                   , 37, "Read DR5."),
    EXIT_REASON(SVM_EXIT_READ_DR6                   , 38, "Read DR6."),
    EXIT_REASON(SVM_EXIT_READ_DR7                   , 39, "Read DR7."),
    EXIT_REASON(SVM_EXIT_READ_DR8                   , 40, "Read DR8."),
    EXIT_REASON(SVM_EXIT_READ_DR9                   , 41, "Read DR9."),
    EXIT_REASON(SVM_EXIT_READ_DR10                  , 42, "Read DR10."),
    EXIT_REASON(SVM_EXIT_READ_DR11                  , 43, "Read DR11"),
    EXIT_REASON(SVM_EXIT_READ_DR12                  , 44, "Read DR12."),
    EXIT_REASON(SVM_EXIT_READ_DR13                  , 45, "Read DR13."),
    EXIT_REASON(SVM_EXIT_READ_DR14                  , 46, "Read DR14."),
    EXIT_REASON(SVM_EXIT_READ_DR15                  , 47, "Read DR15."),
    EXIT_REASON(SVM_EXIT_WRITE_DR0                  , 48, "Write DR0."),
    EXIT_REASON(SVM_EXIT_WRITE_DR1                  , 49, "Write DR1."),
    EXIT_REASON(SVM_EXIT_WRITE_DR2                  , 50, "Write DR2."),
    EXIT_REASON(SVM_EXIT_WRITE_DR3                  , 51, "Write DR3."),
    EXIT_REASON(SVM_EXIT_WRITE_DR4                  , 52, "Write DR4."),
    EXIT_REASON(SVM_EXIT_WRITE_DR5                  , 53, "Write DR5."),
    EXIT_REASON(SVM_EXIT_WRITE_DR6                  , 54, "Write DR6."),
    EXIT_REASON(SVM_EXIT_WRITE_DR7                  , 55, "Write DR7."),
    EXIT_REASON(SVM_EXIT_WRITE_DR8                  , 56, "Write DR8."),
    EXIT_REASON(SVM_EXIT_WRITE_DR9                  , 57, "Write DR9."),
    EXIT_REASON(SVM_EXIT_WRITE_DR10                 , 58, "Write DR10."),
    EXIT_REASON(SVM_EXIT_WRITE_DR11                 , 59, "Write DR11."),
    EXIT_REASON(SVM_EXIT_WRITE_DR12                 , 60, "Write DR12."),
    EXIT_REASON(SVM_EXIT_WRITE_DR13                 , 61, "Write DR13."),
    EXIT_REASON(SVM_EXIT_WRITE_DR14                 , 62, "Write DR14."),
    EXIT_REASON(SVM_EXIT_WRITE_DR15                 , 63, "Write DR15."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_0                , 64, "Exception Vector 0  (0x0)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1                , 65, "Exception Vector 1  (0x1)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_2                , 66, "Exception Vector 2  (0x2)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_3                , 67, "Exception Vector 3  (0x3)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_4                , 68, "Exception Vector 4  (0x4)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_5                , 69, "Exception Vector 5  (0x5)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_6                , 70, "Exception Vector 6  (0x6)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_7                , 71, "Exception Vector 7  (0x7)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_8                , 72, "Exception Vector 8  (0x8)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_9                , 73, "Exception Vector 9  (0x9)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_A                , 74, "Exception Vector 10 (0xA)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_B                , 75, "Exception Vector 11 (0xB)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_C                , 76, "Exception Vector 12 (0xC)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_D                , 77, "Exception Vector 13 (0xD)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_E                , 78, "Exception Vector 14 (0xE)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_F                , 79, "Exception Vector 15 (0xF)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_10               , 80, "Exception Vector 16 (0x10)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_11               , 81, "Exception Vector 17 (0x11)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_12               , 82, "Exception Vector 18 (0x12)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_13               , 83, "Exception Vector 19 (0x13)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_14               , 84, "Exception Vector 20 (0x14)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_15               , 85, "Exception Vector 22 (0x15)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_16               , 86, "Exception Vector 22 (0x16)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_17               , 87, "Exception Vector 23 (0x17)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_18               , 88, "Exception Vector 24 (0x18)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_19               , 89, "Exception Vector 25 (0x19)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1A               , 90, "Exception Vector 26 (0x1A)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1B               , 91, "Exception Vector 27 (0x1B)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1C               , 92, "Exception Vector 28 (0x1C)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1D               , 93, "Exception Vector 29 (0x1D)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1E               , 94, "Exception Vector 30 (0x1E)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1F               , 95, "Exception Vector 31 (0x1F)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_INTR             , 96, "Physical maskable interrupt."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_NMI              , 97, "Physical non-maskable interrupt."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_SMI              , 98, "System management interrupt."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_INIT             , 99, "Physical INIT signal."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_VINTR            ,100, "Visual interrupt."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_CR0_SEL_WRITE    ,101, "Write to CR0 that changed any bits other than CR0.TS or CR0.MP."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_IDTR_READ        ,102, "Read IDTR"),
    EXIT_REASON(SVM_EXIT_EXCEPTION_GDTR_READ        ,103, "Read GDTR"),
    EXIT_REASON(SVM_EXIT_EXCEPTION_LDTR_READ        ,104, "Read LDTR."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_TR_READ          ,105, "Read TR."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_TR_READ          ,106, "Write IDTR."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_TR_READ          ,107, "Write GDTR."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_TR_READ          ,108, "Write LDTR."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_TR_READ          ,109, "Write TR."),
    EXIT_REASON(SVM_EXIT_RDTSC                      ,110, "RDTSC instruction."),
    EXIT_REASON(SVM_EXIT_RDPMC                      ,111, "RDPMC instruction."),
    EXIT_REASON(SVM_EXIT_PUSHF                      ,112, "PUSHF instruction."),
    EXIT_REASON(SVM_EXIT_POPF                       ,113, "POPF instruction."),
    EXIT_REASON(SVM_EXIT_CPUID                      ,114, "CPUID instruction."),
    EXIT_REASON(SVM_EXIT_RSM                        ,115, "RSM instruction."),
    EXIT_REASON(SVM_EXIT_IRET                       ,116, "IRET instruction."),
    EXIT_REASON(SVM_EXIT_SWINT                      ,117, "Software interrupt (INTn instructions)."),
    EXIT_REASON(SVM_EXIT_INVD                       ,118, "INVD instruction."),
    EXIT_REASON(SVM_EXIT_PAUSE                      ,119, "PAUSE instruction."),
    EXIT_REASON(SVM_EXIT_HLT                        ,120, "HLT instruction."),
    EXIT_REASON(SVM_EXIT_INVLPG                     ,121, "INVLPG instruction."),
    EXIT_REASON(SVM_EXIT_INVLPGA                    ,122, "INVLPGA instruction."),
    EXIT_REASON(SVM_EXIT_IOIO                       ,123, "IN/OUT accessing protected port (EXITINFO1 field provides more information)."),
    EXIT_REASON(SVM_EXIT_MSR                        ,124, "RDMSR or WRMSR access to protected MSR."),
    EXIT_REASON(SVM_EXIT_TASK_SWITCH                ,125, "Task switch."),
    EXIT_REASON(SVM_EXIT_FERR_FREEZE                ,126, "FP legacy handling enabled, and processor is frozen in an x87/mmx instruction waiting for an interrupt"),
    EXIT_REASON(SVM_EXIT_TASK_SHUTDOWN              ,127, "Shutdown."),
    EXIT_REASON(SVM_EXIT_TASK_VMRUN                 ,128, "VMRUN instruction."),
    EXIT_REASON(SVM_EXIT_TASK_VMCALL                ,129, "VMCALL instruction."),
    EXIT_REASON(SVM_EXIT_TASK_VMLOAD                ,130, "VMLOAD instruction."),
    EXIT_REASON(SVM_EXIT_TASK_VMSAVE                ,131, "VMSAVE instruction."),
    EXIT_REASON(SVM_EXIT_TASK_STGI                  ,132, "STGI instruction."),
    EXIT_REASON(SVM_EXIT_TASK_CLGI                  ,133, "CLGI instruction."),
    EXIT_REASON(SVM_EXIT_TASK_SKINIT                ,134, "SKINIT instruction."),
    EXIT_REASON(SVM_EXIT_TASK_RDTSCP                ,135, "RDTSCP instruction."),
    EXIT_REASON(SVM_EXIT_TASK_ICEBP                 ,136, "ICEBP instruction."),
    EXIT_REASON(SVM_EXIT_TASK_WBINVD                ,137, "WBINVD instruction."),
    EXIT_REASON(SVM_EXIT_TASK_MONITOR               ,138, "MONITOR instruction."),
    EXIT_REASON(SVM_EXIT_MWAIT_UNCOND               ,139, "MWAIT instruction unconditional."),
    EXIT_REASON(SVM_EXIT_MWAIT_ARMED                ,140, "MWAIT instruction when armed."),
    EXIT_REASON(SVM_EXIT_MWAIT_NPF                  ,1024, "Nested paging: host-level page fault occurred (EXITINFO1 contains fault errorcode; EXITINFO2 contains the guest physical address causing the fault)."),
    EXIT_REASON_NIL()
};
# undef EXIT_REASON
# undef EXIT_REASON_NIL
#endif /* VBOX_WITH_STATISTICS */

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
VMMR3DECL(int) HWACCMR3Init(PVM pVM)
{
    LogFlow(("HWACCMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, hwaccm.s, 32);
    AssertCompile(sizeof(pVM->hwaccm.s) <= sizeof(pVM->hwaccm.padding));

    /* Some structure checks. */
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, u8Reserved3) == 0xC0, ("u8Reserved3 offset = %x\n", RT_OFFSETOF(SVM_VMCB, u8Reserved3)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, ctrl.EventInject) == 0xA8, ("ctrl.EventInject offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.EventInject)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, ctrl.ExitIntInfo) == 0x88, ("ctrl.ExitIntInfo offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.ExitIntInfo)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, ctrl.TLBCtrl) == 0x58, ("ctrl.TLBCtrl offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.TLBCtrl)));

    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest) == 0x400, ("guest offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved4) == 0x4A0, ("guest.u8Reserved4 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved4)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved6) == 0x4D8, ("guest.u8Reserved6 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved6)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved7) == 0x580, ("guest.u8Reserved7 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved7)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u8Reserved9) == 0x648, ("guest.u8Reserved9 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8Reserved9)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, u8Reserved10) == 0x698, ("u8Reserved3 offset = %x\n", RT_OFFSETOF(SVM_VMCB, u8Reserved10)));
    AssertReleaseMsg(sizeof(SVM_VMCB) == 0x1000, ("SVM_VMCB size = %x\n", sizeof(SVM_VMCB)));


    /*
     * Register the saved state data unit.
     */
    int rc = SSMR3RegisterInternal(pVM, "HWACCM", 0, HWACCM_SSM_VERSION, sizeof(HWACCM),
                                   NULL, hwaccmR3Save, NULL,
                                   NULL, hwaccmR3Load, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /* Misc initialisation. */
    pVM->hwaccm.s.vmx.fSupported = false;
    pVM->hwaccm.s.svm.fSupported = false;
    pVM->hwaccm.s.vmx.fEnabled   = false;
    pVM->hwaccm.s.svm.fEnabled   = false;

    pVM->hwaccm.s.fNestedPaging  = false;

    /* Disabled by default. */
    pVM->fHWACCMEnabled = false;

    /*
     * Check CFGM options.
     */
    PCFGMNODE pRoot      = CFGMR3GetRoot(pVM);
    PCFGMNODE pHWVirtExt = CFGMR3GetChild(pRoot, "HWVirtExt/");
    /* Nested paging: disabled by default. */
    rc = CFGMR3QueryBoolDef(pRoot, "EnableNestedPaging", &pVM->hwaccm.s.fAllowNestedPaging, false);
    AssertRC(rc);

    /* VT-x VPID: disabled by default. */
    rc = CFGMR3QueryBoolDef(pRoot, "EnableVPID", &pVM->hwaccm.s.vmx.fAllowVPID, false);
    AssertRC(rc);

    /* HWACCM support must be explicitely enabled in the configuration file. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "Enabled", &pVM->hwaccm.s.fAllowed, false);
    AssertRC(rc);

#ifdef RT_OS_DARWIN
    if (VMMIsHwVirtExtForced(pVM) != pVM->hwaccm.s.fAllowed)
#else
    if (VMMIsHwVirtExtForced(pVM) && !pVM->hwaccm.s.fAllowed)
#endif
    {
        AssertLogRelMsgFailed(("VMMIsHwVirtExtForced=%RTbool fAllowed=%RTbool\n",
                               VMMIsHwVirtExtForced(pVM), pVM->hwaccm.s.fAllowed));
        return VERR_HWACCM_CONFIG_MISMATCH;
    }

    if (VMMIsHwVirtExtForced(pVM))
        pVM->fHWACCMEnabled = true;

#if HC_ARCH_BITS == 32
    /* 64-bit mode is configurable and it depends on both the kernel mode and VT-x.
     * (To use the default, don't set 64bitEnabled in CFGM.) */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "64bitEnabled", &pVM->hwaccm.s.fAllow64BitGuests, false);
    AssertLogRelRCReturn(rc, rc);
    if (pVM->hwaccm.s.fAllow64BitGuests)
    {
# ifdef RT_OS_DARWIN
        if (!VMMIsHwVirtExtForced(pVM))
# else
        if (!pVM->hwaccm.s.fAllowed)
# endif
            return VM_SET_ERROR(pVM, VERR_INVALID_PARAMETER, "64-bit guest support was requested without also enabling HWVirtEx (VT-x/AMD-V).");
    }
#else
    /* On 64-bit hosts 64-bit guest support is enabled by default, but allow this to be overridden
     * via VBoxInternal/HWVirtExt/64bitEnabled=0. (ConsoleImpl2.cpp doesn't set this to false for 64-bit.) */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "64bitEnabled", &pVM->hwaccm.s.fAllow64BitGuests, true);
    AssertLogRelRCReturn(rc, rc);
#endif

    return VINF_SUCCESS;
}

/**
 * Initializes the per-VCPU HWACCM.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) HWACCMR3InitCPU(PVM pVM)
{
    LogFlow(("HWACCMR3InitCPU\n"));

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->hwaccm.s.fActive = false;
    }

#ifdef VBOX_WITH_STATISTICS
    /*
     * Statistics.
     */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        int    rc;

        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatEntry, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of VMXR0RunGuestCode entry",
                             "/PROF/HWACCM/CPU%d/SwitchToGC", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExit1, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of VMXR0RunGuestCode exit part 1",
                             "/PROF/HWACCM/CPU%d/SwitchFromGC_1", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExit2, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of VMXR0RunGuestCode exit part 2",
                             "/PROF/HWACCM/CPU%d/SwitchFromGC_2", i);
        AssertRC(rc);
# if 1 /* temporary for tracking down darwin holdup. */
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExit2Sub1, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Temporary - I/O",
                             "/PROF/HWACCM/CPU%d/SwitchFromGC_2/Sub1", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExit2Sub2, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Temporary - CRx RWs",
                             "/PROF/HWACCM/CPU%d/SwitchFromGC_2/Sub2", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExit2Sub3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Temporary - Exceptions",
                             "/PROF/HWACCM/CPU%d/SwitchFromGC_2/Sub3", i);
        AssertRC(rc);
# endif
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatInGC, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of vmlaunch",
                             "/PROF/HWACCM/CPU%d/InGC", i);
        AssertRC(rc);

# if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatWorldSwitch3264, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of the 32/64 switcher",
                             "/PROF/HWACCM/CPU%d/Switcher3264", i);
        AssertRC(rc);
# endif

# define HWACCM_REG_COUNTER(a, b) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Profiling of vmlaunch", b, i); \
        AssertRC(rc);

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitShadowNM,           "/HWACCM/CPU%d/Exit/Trap/Shw/#NM");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestNM,            "/HWACCM/CPU%d/Exit/Trap/Gst/#NM");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitShadowPF,           "/HWACCM/CPU%d/Exit/Trap/Shw/#PF");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestPF,            "/HWACCM/CPU%d/Exit/Trap/Gst/#PF");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestUD,            "/HWACCM/CPU%d/Exit/Trap/Gst/#UD");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestSS,            "/HWACCM/CPU%d/Exit/Trap/Gst/#SS");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestNP,            "/HWACCM/CPU%d/Exit/Trap/Gst/#NP");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestGP,            "/HWACCM/CPU%d/Exit/Trap/Gst/#GP");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestMF,            "/HWACCM/CPU%d/Exit/Trap/Gst/#MF");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestDE,            "/HWACCM/CPU%d/Exit/Trap/Gst/#DE");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestDB,            "/HWACCM/CPU%d/Exit/Trap/Gst/#DB");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitInvpg,              "/HWACCM/CPU%d/Exit/Instr/Invlpg");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitInvd,               "/HWACCM/CPU%d/Exit/Instr/Invd");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitCpuid,              "/HWACCM/CPU%d/Exit/Instr/Cpuid");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdtsc,              "/HWACCM/CPU%d/Exit/Instr/Rdtsc");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdpmc,              "/HWACCM/CPU%d/Exit/Instr/Rdpmc");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdmsr,              "/HWACCM/CPU%d/Exit/Instr/Rdmsr");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitWrmsr,              "/HWACCM/CPU%d/Exit/Instr/Wrmsr");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitMwait,              "/HWACCM/CPU%d/Exit/Instr/Mwait");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitDRxWrite,           "/HWACCM/CPU%d/Exit/Instr/DR/Write");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitDRxRead,            "/HWACCM/CPU%d/Exit/Instr/DR/Read");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitCLTS,               "/HWACCM/CPU%d/Exit/Instr/CLTS");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitLMSW,               "/HWACCM/CPU%d/Exit/Instr/LMSW");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitCli,                "/HWACCM/CPU%d/Exit/Instr/Cli");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitSti,                "/HWACCM/CPU%d/Exit/Instr/Sti");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitPushf,              "/HWACCM/CPU%d/Exit/Instr/Pushf");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitPopf,               "/HWACCM/CPU%d/Exit/Instr/Popf");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitIret,               "/HWACCM/CPU%d/Exit/Instr/Iret");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitInt,                "/HWACCM/CPU%d/Exit/Instr/Int");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitHlt,                "/HWACCM/CPU%d/Exit/Instr/Hlt");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitIOWrite,            "/HWACCM/CPU%d/Exit/IO/Write");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitIORead,             "/HWACCM/CPU%d/Exit/IO/Read");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitIOStringWrite,      "/HWACCM/CPU%d/Exit/IO/WriteString");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitIOStringRead,       "/HWACCM/CPU%d/Exit/IO/ReadString");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitIrqWindow,          "/HWACCM/CPU%d/Exit/IrqWindow");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitMaxResume,          "/HWACCM/CPU%d/Exit/MaxResume");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatSwitchGuestIrq,         "/HWACCM/CPU%d/Switch/IrqPending");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatSwitchToR3,             "/HWACCM/CPU%d/Switch/ToR3");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatIntInject,              "/HWACCM/CPU%d/Irq/Inject");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatIntReinject,            "/HWACCM/CPU%d/Irq/Reinject");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatPendingHostIrq,         "/HWACCM/CPU%d/Irq/PendingOnHost");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushPageManual,        "/HWACCM/CPU%d/Flush/Page/Virt");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushPhysPageManual,    "/HWACCM/CPU%d/Flush/Page/Phys");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushTLBManual,         "/HWACCM/CPU%d/Flush/TLB/Manual");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushTLBCRxChange,      "/HWACCM/CPU%d/Flush/TLB/CRx");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushPageInvlpg,        "/HWACCM/CPU%d/Flush/Page/Invlpg");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushTLBWorldSwitch,    "/HWACCM/CPU%d/Flush/TLB/Switch");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatNoFlushTLBWorldSwitch,  "/HWACCM/CPU%d/Flush/TLB/Skipped");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushASID,              "/HWACCM/CPU%d/Flush/TLB/ASID");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushTLBInvlpga,        "/HWACCM/CPU%d/Flush/TLB/PhysInvl");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatTlbShootdown,           "/HWACCM/CPU%d/Flush/Shootdown/Page");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatTlbShootdownFlush,      "/HWACCM/CPU%d/Flush/Shootdown/TLB");
        
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatTSCOffset,              "/HWACCM/CPU%d/TSC/Offset");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatTSCIntercept,           "/HWACCM/CPU%d/TSC/Intercept");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDRxArmed,               "/HWACCM/CPU%d/Debug/Armed");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDRxContextSwitch,       "/HWACCM/CPU%d/Debug/ContextSwitch");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDRxIOCheck,             "/HWACCM/CPU%d/Debug/IOCheck");

        for (unsigned j=0;j<RT_ELEMENTS(pVCpu->hwaccm.s.StatExitCRxWrite);j++)
        {
            rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExitCRxWrite[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, "Profiling of CRx writes",
                                "/HWACCM/CPU%d/Exit/Instr/CR/Write/%x", i, j);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExitCRxRead[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, "Profiling of CRx reads",
                                "/HWACCM/CPU%d/Exit/Instr/CR/Read/%x", i, j);
            AssertRC(rc);
        }

#undef HWACCM_REG_COUNTER

        pVCpu->hwaccm.s.paStatExitReason = NULL;

        rc = MMHyperAlloc(pVM, MAX_EXITREASON_STAT*sizeof(*pVCpu->hwaccm.s.paStatExitReason), 0, MM_TAG_HWACCM, (void **)&pVCpu->hwaccm.s.paStatExitReason);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            const char * const *papszDesc = ASMIsIntelCpu() ? &g_apszVTxExitReasons[0] : &g_apszAmdVExitReasons[0];
            for (int j=0;j<MAX_EXITREASON_STAT;j++)
            {
                rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.paStatExitReason[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                     papszDesc[j] ? papszDesc[j] : "Exit reason",
                                     "/HWACCM/CPU%d/Exit/Reason/%02x", i, j);
                AssertRC(rc);
            }
            rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatExitReasonNPF, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, "Nested page fault", "/HWACCM/CPU%d/Exit/Reason/#NPF", i);
            AssertRC(rc);
        }
        pVCpu->hwaccm.s.paStatExitReasonR0 = MMHyperR3ToR0(pVM, pVCpu->hwaccm.s.paStatExitReason);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hwaccm.s.paStatExitReasonR0 != NIL_RTR0PTR || !VMMIsHwVirtExtForced(pVM));
# else
        Assert(pVCpu->hwaccm.s.paStatExitReasonR0 != NIL_RTR0PTR);
# endif
    }
#endif /* VBOX_WITH_STATISTICS */

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /* Magic marker for searching in crash dumps. */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;
        strcpy((char *)pCache->aMagic, "VMCSCACHE Magic");
        pCache->uMagic = UINT64_C(0xDEADBEEFDEADBEEF);
    }
#endif
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

    /* Disable the switcher code (safety precaution). */
    VMMR3DisableSwitcher(pVM);

    /* Disable mapping of the hypervisor into the shadow page table. */
    PGMR3MappingsDisable(pVM);

    /* Disable the switcher */
    VMMR3DisableSwitcher(pVM);

    /* Reinit the paging mode to force the new shadow mode. */
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        PGMR3ChangeMode(pVM, pVCpu, PGMMODE_REAL);
    }
}

/**
 * Initialize VT-x or AMD-V.
 *
 * @returns VBox status code.
 * @param   pVM         The VM handle.
 */
VMMR3DECL(int) HWACCMR3InitFinalizeR0(PVM pVM)
{
    int rc;

    if (    !pVM->hwaccm.s.vmx.fSupported
        &&  !pVM->hwaccm.s.svm.fSupported)
    {
        LogRel(("HWACCM: No VT-x or AMD-V CPU extension found. Reason %Rrc\n", pVM->hwaccm.s.lLastError));
        LogRel(("HWACCM: VMX MSR_IA32_FEATURE_CONTROL=%RX64\n", pVM->hwaccm.s.vmx.msr.feature_ctrl));
        if (VMMIsHwVirtExtForced(pVM))
            return VM_SET_ERROR(pVM, VERR_VMX_NO_VMX, "VT-x is not available.");
        return VINF_SUCCESS;
    }

    if (!pVM->hwaccm.s.fAllowed)
        return VINF_SUCCESS;    /* nothing to do */

    /* Enable VT-x or AMD-V on all host CPUs. */
    rc = SUPCallVMMR0Ex(pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_HWACC_ENABLE, 0, NULL);
    if (RT_FAILURE(rc))
    {
        LogRel(("HWACCMR3InitFinalize: SUPCallVMMR0Ex VMMR0_DO_HWACC_ENABLE failed with %Rrc\n", rc));
        return rc;
    }
    Assert(!pVM->fHWACCMEnabled || VMMIsHwVirtExtForced(pVM));

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        Log(("pVM->hwaccm.s.vmx.fSupported = %d\n", pVM->hwaccm.s.vmx.fSupported));

        if (    pVM->hwaccm.s.fInitialized == false
            &&  pVM->hwaccm.s.vmx.msr.feature_ctrl != 0)
        {
            uint64_t val;
            RTGCPHYS GCPhys = 0;

            LogRel(("HWACCM: Host CR4=%08X\n", pVM->hwaccm.s.vmx.hostCR4));
            LogRel(("HWACCM: MSR_IA32_FEATURE_CONTROL      = %RX64\n", pVM->hwaccm.s.vmx.msr.feature_ctrl));
            LogRel(("HWACCM: MSR_IA32_VMX_BASIC_INFO       = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_basic_info));
            LogRel(("HWACCM: VMCS id                       = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HWACCM: VMCS size                     = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HWACCM: VMCS physical address limit   = %s\n", MSR_IA32_VMX_BASIC_INFO_VMCS_PHYS_WIDTH(pVM->hwaccm.s.vmx.msr.vmx_basic_info) ? "< 4 GB" : "None"));
            LogRel(("HWACCM: VMCS memory type              = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_MEM_TYPE(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HWACCM: Dual monitor treatment        = %d\n", MSR_IA32_VMX_BASIC_INFO_VMCS_DUAL_MON(pVM->hwaccm.s.vmx.msr.vmx_basic_info)));

            LogRel(("HWACCM: MSR_IA32_VMX_PINBASED_CTLS    = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.allowed1;
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER\n"));
            val = pVM->hwaccm.s.vmx.msr.vmx_pin_ctls.n.disallowed0;
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER *must* be set\n"));

            LogRel(("HWACCM: MSR_IA32_VMX_PROCBASED_CTLS   = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.u));
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
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG\n"));
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
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG *must* be set\n"));
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
                LogRel(("HWACCM: MSR_IA32_VMX_PROCBASED_CTLS2  = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.u));
                val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_X2APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT\n"));

                val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_X2APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT *must* be set\n"));
            }

            LogRel(("HWACCM: MSR_IA32_VMX_ENTRY_CTLS       = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_entry.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1;
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR\n"));
            val = pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0;
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR *must* be set\n"));

            LogRel(("HWACCM: MSR_IA32_VMX_EXIT_CTLS        = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_exit.u));
            val = pVM->hwaccm.s.vmx.msr.vmx_exit.n.allowed1;
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER\n"));
            val = pVM->hwaccm.s.vmx.msr.vmx_exit.n.disallowed0;
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64 *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER)
                LogRel(("HWACCM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER *must* be set\n"));

            if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps)
            {
                LogRel(("HWACCM: MSR_IA32_VMX_EPT_VPID_CAPS    = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_eptcaps));

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

            LogRel(("HWACCM: MSR_IA32_VMX_MISC             = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_misc));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT %x\n", MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_ACTIVITY_STATES %x\n", MSR_IA32_VMX_MISC_ACTIVITY_STATES(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_CR3_TARGET      %x\n", MSR_IA32_VMX_MISC_CR3_TARGET(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_MAX_MSR         %x\n", MSR_IA32_VMX_MISC_MAX_MSR(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            LogRel(("HWACCM:    MSR_IA32_VMX_MISC_MSEG_ID         %x\n", MSR_IA32_VMX_MISC_MSEG_ID(pVM->hwaccm.s.vmx.msr.vmx_misc)));

            LogRel(("HWACCM: MSR_IA32_VMX_CR0_FIXED0       = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0));
            LogRel(("HWACCM: MSR_IA32_VMX_CR0_FIXED1       = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed1));
            LogRel(("HWACCM: MSR_IA32_VMX_CR4_FIXED0       = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed0));
            LogRel(("HWACCM: MSR_IA32_VMX_CR4_FIXED1       = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_cr4_fixed1));
            LogRel(("HWACCM: MSR_IA32_VMX_VMCS_ENUM        = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_vmcs_enum));

            LogRel(("HWACCM: TPR shadow physaddr           = %RHp\n", pVM->hwaccm.s.vmx.pAPICPhys));
            LogRel(("HWACCM: MSR bitmap physaddr           = %RHp\n", pVM->hwaccm.s.vmx.pMSRBitmapPhys));

            for (unsigned i=0;i<pVM->cCPUs;i++)
                LogRel(("HWACCM: VMCS physaddr VCPU%d           = %RHp\n", i, pVM->aCpus[i].hwaccm.s.vmx.pVMCSPhys));

#ifdef HWACCM_VTX_WITH_EPT
            if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                pVM->hwaccm.s.fNestedPaging = pVM->hwaccm.s.fAllowNestedPaging;
#endif /* HWACCM_VTX_WITH_EPT */
#ifdef HWACCM_VTX_WITH_VPID
            if (    (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                &&  !pVM->hwaccm.s.fNestedPaging)    /* VPID and EPT are mutually exclusive. */
                pVM->hwaccm.s.vmx.fVPID = pVM->hwaccm.s.vmx.fAllowVPID;
#endif /* HWACCM_VTX_WITH_VPID */

            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            /* Allocate three pages for the TSS we need for real mode emulation. (2 page for the IO bitmap) */
#if 1
            rc = PDMR3VMMDevHeapAlloc(pVM, HWACCM_VTX_TOTAL_DEVHEAP_MEM, (RTR3PTR *)&pVM->hwaccm.s.vmx.pRealModeTSS);
#else
            rc = VERR_NO_MEMORY; /* simulation of no VMMDev Heap. */
#endif
            if (RT_SUCCESS(rc))
            {
                /* The I/O bitmap starts right after the virtual interrupt redirection bitmap. */
                ASMMemZero32(pVM->hwaccm.s.vmx.pRealModeTSS, sizeof(*pVM->hwaccm.s.vmx.pRealModeTSS));
                pVM->hwaccm.s.vmx.pRealModeTSS->offIoBitmap = sizeof(*pVM->hwaccm.s.vmx.pRealModeTSS);
                /* Bit set to 0 means redirection enabled. */
                memset(pVM->hwaccm.s.vmx.pRealModeTSS->IntRedirBitmap, 0x0, sizeof(pVM->hwaccm.s.vmx.pRealModeTSS->IntRedirBitmap));
                /* Allow all port IO, so the VT-x IO intercepts do their job. */
                memset(pVM->hwaccm.s.vmx.pRealModeTSS + 1, 0, PAGE_SIZE*2);
                *((unsigned char *)pVM->hwaccm.s.vmx.pRealModeTSS + HWACCM_VTX_TSS_SIZE - 2) = 0xff;

                /* Construct a 1024 element page directory with 4 MB pages for the identity mapped page table used in
                 * real and protected mode without paging with EPT.
                 */
                pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable = (PX86PD)((char *)pVM->hwaccm.s.vmx.pRealModeTSS + PAGE_SIZE * 3);
                for (unsigned i=0;i<X86_PG_ENTRIES;i++)
                {
                    pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable->a[i].u  = _4M * i;
                    pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable->a[i].u |= X86_PDE4M_P | X86_PDE4M_RW | X86_PDE4M_US | X86_PDE4M_A | X86_PDE4M_D | X86_PDE4M_PS | X86_PDE4M_G;
                }

                /* We convert it here every time as pci regions could be reconfigured. */
                rc = PDMVMMDevHeapR3ToGCPhys(pVM, pVM->hwaccm.s.vmx.pRealModeTSS, &GCPhys);
                AssertRC(rc);
                LogRel(("HWACCM: Real Mode TSS guest physaddr  = %RGp\n", GCPhys));

                rc = PDMVMMDevHeapR3ToGCPhys(pVM, pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                AssertRC(rc);
                LogRel(("HWACCM: Non-Paging Mode EPT CR3       = %RGp\n", GCPhys));
            }
            else
            {
                LogRel(("HWACCM: No real mode VT-x support (PDMR3VMMDevHeapAlloc returned %Rrc)\n", rc));
                pVM->hwaccm.s.vmx.pRealModeTSS = NULL;
                pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable = NULL;
            }

            rc = SUPCallVMMR0Ex(pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                pVM->fHWACCMEnabled = true;
                pVM->hwaccm.s.vmx.fEnabled = true;
                hwaccmR3DisableRawMode(pVM);

                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                if (pVM->hwaccm.s.fAllow64BitGuests)
                {
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);            /* 64 bits only on Intel CPUs */
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NXE);
                }
                LogRel((pVM->hwaccm.s.fAllow64BitGuests
                        ? "HWACCM: 32-bit and 64-bit guests supported.\n"
                        : "HWACCM: 32-bit guests supported.\n"));
#else
                LogRel(("HWACCM: 32-bit guests supported.\n"));
#endif
                LogRel(("HWACCM: VMX enabled!\n"));
                if (pVM->hwaccm.s.fNestedPaging)
                {
                    LogRel(("HWACCM: Enabled nested paging\n"));
                    LogRel(("HWACCM: EPT root page                 = %RHp\n", PGMGetHyperCR3(VMMGetCpu(pVM))));
                }
                if (pVM->hwaccm.s.vmx.fVPID)
                    LogRel(("HWACCM: Enabled VPID\n"));

                if (   pVM->hwaccm.s.fNestedPaging
                    || pVM->hwaccm.s.vmx.fVPID)
                {
                    LogRel(("HWACCM: enmFlushPage    %d\n", pVM->hwaccm.s.vmx.enmFlushPage));
                    LogRel(("HWACCM: enmFlushContext %d\n", pVM->hwaccm.s.vmx.enmFlushContext));
                }
            }
            else
            {
                LogRel(("HWACCM: VMX setup failed with rc=%Rrc!\n", rc));
                LogRel(("HWACCM: Last instruction error %x\n", pVM->aCpus[0].hwaccm.s.vmx.lasterror.ulInstrError));
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

            LogRel(("HWACMM: cpuid 0x80000001.u32AMDFeatureECX = %RX32\n", pVM->hwaccm.s.cpuid.u32AMDFeatureECX));
            LogRel(("HWACMM: cpuid 0x80000001.u32AMDFeatureEDX = %RX32\n", pVM->hwaccm.s.cpuid.u32AMDFeatureEDX));
            LogRel(("HWACCM: AMD-V revision                    = %X\n", pVM->hwaccm.s.svm.u32Rev));
            LogRel(("HWACCM: AMD-V max ASID                    = %d\n", pVM->hwaccm.s.uMaxASID));
            LogRel(("HWACCM: AMD-V features                    = %X\n", pVM->hwaccm.s.svm.u32Features));

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

            rc = SUPCallVMMR0Ex(pVM->pVMR0, 0 /* VCPU 0 */, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
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
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                if (pVM->hwaccm.s.fAllow64BitGuests)
                {
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NXE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
                }
#endif
                LogRel((pVM->hwaccm.s.fAllow64BitGuests
                        ? "HWACCM:    32-bit and 64-bit guest supported.\n"
                        : "HWACCM:    32-bit guest supported.\n"));
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
VMMR3DECL(void) HWACCMR3Relocate(PVM pVM)
{
    Log(("HWACCMR3Relocate to %RGv\n", MMHyperGetArea(pVM, 0)));

    /* Fetch the current paging mode during the relocate callback during state loading. */
    if (VMR3GetState(pVM) == VMSTATE_LOADING)
    {
        for (unsigned i=0;i<pVM->cCPUs;i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];
            /* @todo SMP */
            pVCpu->hwaccm.s.enmShadowMode            = PGMGetShadowMode(pVCpu);
            pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = PGMGetGuestMode(pVCpu);
        }
    }
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (pVM->fHWACCMEnabled)
    {
        int rc;

        switch(PGMGetHostMode(pVM))
        {
        case PGMMODE_32_BIT:
            pVM->hwaccm.s.pfnHost32ToGuest64R0 = VMMR3GetHostToGuestSwitcher(pVM, VMMSWITCHER_32_TO_AMD64);
            break;

        case PGMMODE_PAE:
        case PGMMODE_PAE_NX:
            pVM->hwaccm.s.pfnHost32ToGuest64R0 = VMMR3GetHostToGuestSwitcher(pVM, VMMSWITCHER_PAE_TO_AMD64);
            break;

        default:
            AssertFailed();
            break;
        }
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "VMXGCStartVM64", &pVM->hwaccm.s.pfnVMXGCStartVM64);
        AssertReleaseMsgRC(rc, ("VMXGCStartVM64 -> rc=%Rrc\n", rc));

        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "SVMGCVMRun64",   &pVM->hwaccm.s.pfnSVMGCVMRun64);
        AssertReleaseMsgRC(rc, ("SVMGCVMRun64 -> rc=%Rrc\n", rc));

        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "HWACCMSaveGuestFPU64",   &pVM->hwaccm.s.pfnSaveGuestFPU64);
        AssertReleaseMsgRC(rc, ("HWACCMSetupFPU64 -> rc=%Rrc\n", rc));

        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "HWACCMSaveGuestDebug64",   &pVM->hwaccm.s.pfnSaveGuestDebug64);
        AssertReleaseMsgRC(rc, ("HWACCMSetupDebug64 -> rc=%Rrc\n", rc));

# ifdef DEBUG
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "HWACCMTestSwitcher64",   &pVM->hwaccm.s.pfnTest64);
        AssertReleaseMsgRC(rc, ("HWACCMTestSwitcher64 -> rc=%Rrc\n", rc));
# endif
    }
#endif
    return;
}

/**
 * Checks hardware accelerated raw mode is allowed.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(bool) HWACCMR3IsAllowed(PVM pVM)
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
 * @param   pVCpu          The VMCPU to operate on.
 * @param   enmShadowMode  New shadow paging mode.
 * @param   enmGuestMode   New guest paging mode.
 */
VMMR3DECL(void) HWACCMR3PagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode)
{
    /* Ignore page mode changes during state loading. */
    if (VMR3GetState(pVCpu->pVMR3) == VMSTATE_LOADING)
        return;

    pVCpu->hwaccm.s.enmShadowMode = enmShadowMode;

    if (   pVM->hwaccm.s.vmx.fEnabled
        && pVM->fHWACCMEnabled)
    {
        if (    pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
            &&  enmGuestMode >= PGMMODE_PROTECTED)
        {
            PCPUMCTX pCtx;

            pCtx = CPUMQueryGuestCtxPtr(pVCpu);

            /* After a real mode switch to protected mode we must force
             * CPL to 0. Our real mode emulation had to set it to 3.
             */
            pCtx->ssHid.Attr.n.u2Dpl  = 0;
        }
    }

    if (pVCpu->hwaccm.s.vmx.enmCurrGuestMode != enmGuestMode)
    {
        /* Keep track of paging mode changes. */
        pVCpu->hwaccm.s.vmx.enmPrevGuestMode = pVCpu->hwaccm.s.vmx.enmCurrGuestMode;
        pVCpu->hwaccm.s.vmx.enmCurrGuestMode = enmGuestMode;

        /* Did we miss a change, because all code was executed in the recompiler? */
        if (pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode == enmGuestMode)
        {
            Log(("HWACCMR3PagingModeChanged missed %s->%s transition (prev %s)\n", PGMGetModeName(pVCpu->hwaccm.s.vmx.enmPrevGuestMode), PGMGetModeName(pVCpu->hwaccm.s.vmx.enmCurrGuestMode), PGMGetModeName(pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode)));
            pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = pVCpu->hwaccm.s.vmx.enmPrevGuestMode;
        }
    }

    /* Reset the contents of the read cache. */
    PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;
    for (unsigned j=0;j<pCache->Read.cValidEntries;j++)
        pCache->Read.aFieldVal[j] = 0;
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
VMMR3DECL(int) HWACCMR3Term(PVM pVM)
{
    if (pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        PDMR3VMMDevHeapFree(pVM, pVM->hwaccm.s.vmx.pRealModeTSS);
        pVM->hwaccm.s.vmx.pRealModeTSS       = 0;
    }
    HWACCMR3TermCPU(pVM);
    return 0;
}

/**
 * Terminates the per-VCPU HWACCM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM it self is at this point powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int) HWACCMR3TermCPU(PVM pVM)
{
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        if (pVCpu->hwaccm.s.paStatExitReason)
        {
            MMHyperFree(pVM, pVCpu->hwaccm.s.paStatExitReason);
            pVCpu->hwaccm.s.paStatExitReason   = NULL;
            pVCpu->hwaccm.s.paStatExitReasonR0 = NIL_RTR0PTR;
        }
#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        memset(pVCpu->hwaccm.s.vmx.VMCSCache.aMagic, 0, sizeof(pVCpu->hwaccm.s.vmx.VMCSCache.aMagic));
        pVCpu->hwaccm.s.vmx.VMCSCache.uMagic = 0;
        pVCpu->hwaccm.s.vmx.VMCSCache.uPos = 0xffffffff;
#endif
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
VMMR3DECL(void) HWACCMR3Reset(PVM pVM)
{
    LogFlow(("HWACCMR3Reset:\n"));

    if (pVM->fHWACCMEnabled)
        hwaccmR3DisableRawMode(pVM);

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        /* On first entry we'll sync everything. */
        pVCpu->hwaccm.s.fContextUseFlags = HWACCM_CHANGED_ALL;

        pVCpu->hwaccm.s.vmx.cr0_mask = 0;
        pVCpu->hwaccm.s.vmx.cr4_mask = 0;

        pVCpu->hwaccm.s.fActive        = false;
        pVCpu->hwaccm.s.Event.fPending = false;

        /* Reset state information for real-mode emulation in VT-x. */
        pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode = PGMMODE_REAL;
        pVCpu->hwaccm.s.vmx.enmPrevGuestMode     = PGMMODE_REAL;
        pVCpu->hwaccm.s.vmx.enmCurrGuestMode     = PGMMODE_REAL;

        /* Reset the contents of the read cache. */
        PVMCSCACHE pCache = &pVCpu->hwaccm.s.vmx.VMCSCache;
        for (unsigned j=0;j<pCache->Read.cValidEntries;j++)
            pCache->Read.aFieldVal[j] = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        /* Magic marker for searching in crash dumps. */
        strcpy((char *)pCache->aMagic, "VMCSCACHE Magic");
        pCache->uMagic = UINT64_C(0xDEADBEEFDEADBEEF);
#endif
    }
}

/**
 * Force execution of the current IO code in the recompiler
 *
 * @returns VBox status code.
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Partial VM execution context
 */
VMMR3DECL(int) HWACCMR3EmulateIoBlock(PVM pVM, PCPUMCTX pCtx)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    Assert(pVM->fHWACCMEnabled);
    Log(("HWACCMR3EmulateIoBlock\n"));

    /* This is primarily intended to speed up Grub, so we don't care about paged protected mode. */
    if (HWACCMCanEmulateIoBlockEx(pCtx))
    {
        Log(("HWACCMR3EmulateIoBlock -> enabled\n"));
        pVCpu->hwaccm.s.EmulateIoBlock.fEnabled         = true;
        pVCpu->hwaccm.s.EmulateIoBlock.GCPtrFunctionEip = pCtx->rip;
        pVCpu->hwaccm.s.EmulateIoBlock.cr0              = pCtx->cr0;
        return VINF_EM_RESCHEDULE_REM;
    }
    return VINF_SUCCESS;
}

/**
 * Checks if we can currently use hardware accelerated raw mode.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 * @param   pCtx        Partial VM execution context
 */
VMMR3DECL(bool) HWACCMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    Assert(pVM->fHWACCMEnabled);

    /* If we're still executing the IO code, then return false. */
    if (    RT_UNLIKELY(pVCpu->hwaccm.s.EmulateIoBlock.fEnabled)
        &&  pCtx->rip <  pVCpu->hwaccm.s.EmulateIoBlock.GCPtrFunctionEip + 0x200
        &&  pCtx->rip >  pVCpu->hwaccm.s.EmulateIoBlock.GCPtrFunctionEip - 0x200
        &&  pCtx->cr0 == pVCpu->hwaccm.s.EmulateIoBlock.cr0)
        return false;

    pVCpu->hwaccm.s.EmulateIoBlock.fEnabled = false;

    /* AMD-V supports real & protected mode with or without paging. */
    if (pVM->hwaccm.s.svm.fEnabled)
    {
        pVCpu->hwaccm.s.fActive = true;
        return true;
    }

    pVCpu->hwaccm.s.fActive = false;

    /* Note! The context supplied by REM is partial. If we add more checks here, be sure to verify that REM provides this info! */
#ifdef HWACCM_VMX_EMULATE_REALMODE
    if (pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        if (CPUMIsGuestInRealModeEx(pCtx))
        {
            /* VT-x will not allow high selector bases in v86 mode; fall back to the recompiler in that case.
             * The base must also be equal to (sel << 4).
             */
            if (   (   pCtx->cs != (pCtx->csHid.u64Base >> 4)
                    && pCtx->csHid.u64Base != 0xffff0000 /* we can deal with the BIOS code as it's also mapped into the lower region. */)
                || pCtx->ds != (pCtx->dsHid.u64Base >> 4)
                || pCtx->es != (pCtx->esHid.u64Base >> 4)
                || pCtx->fs != (pCtx->fsHid.u64Base >> 4)
                || pCtx->gs != (pCtx->gsHid.u64Base >> 4)
                || pCtx->ss != (pCtx->ssHid.u64Base >> 4))
            {
                return false;
            }
        }
        else
        {
            PGMMODE enmGuestMode = PGMGetGuestMode(pVCpu);
            /* Verify the requirements for executing code in protected mode. VT-x can't handle the CPU state right after a switch
             * from real to protected mode. (all sorts of RPL & DPL assumptions)
             */
            if (    pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
                &&  enmGuestMode >= PGMMODE_PROTECTED)
            {
                if (   (pCtx->cs & X86_SEL_RPL)
                    || (pCtx->ds & X86_SEL_RPL)
                    || (pCtx->es & X86_SEL_RPL)
                    || (pCtx->fs & X86_SEL_RPL)
                    || (pCtx->gs & X86_SEL_RPL)
                    || (pCtx->ss & X86_SEL_RPL))
                {
                    return false;
                }
            }
        }
    }
    else
#endif /* HWACCM_VMX_EMULATE_REALMODE */
    {
        if (!CPUMIsGuestInLongModeEx(pCtx))
        {
            /** @todo   This should (probably) be set on every excursion to the REM,
             *          however it's too risky right now. So, only apply it when we go
             *          back to REM for real mode execution. (The XP hack below doesn't
             *          work reliably without this.)
             *  Update: Implemented in EM.cpp, see #ifdef EM_NOTIFY_HWACCM.  */
            pVM->aCpus[0].hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL_GUEST;

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

            /* Windows XP: possible same as above, but new recompiler requires new heuristics?
               VT-x doesn't seem to like something about the guest state and this stuff avoids it. */
            /** @todo This check is actually wrong, it doesn't take the direction of the
             *        stack segment into account. But, it does the job for now. */
            if (pCtx->rsp >= pCtx->ssHid.u32Limit)
                return false;
#if 0
            if (    pCtx->cs >= pCtx->gdtr.cbGdt
                ||  pCtx->ss >= pCtx->gdtr.cbGdt
                ||  pCtx->ds >= pCtx->gdtr.cbGdt
                ||  pCtx->es >= pCtx->gdtr.cbGdt
                ||  pCtx->fs >= pCtx->gdtr.cbGdt
                ||  pCtx->gs >= pCtx->gdtr.cbGdt)
                return false;
#endif
        }
    }

    if (pVM->hwaccm.s.vmx.fEnabled)
    {
        uint32_t mask;

        /* if bit N is set in cr0_fixed0, then it must be set in the guest's cr0. */
        mask = (uint32_t)pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0;
        /* Note: We ignore the NE bit here on purpose; see vmmr0\hwaccmr0.cpp for details. */
        mask &= ~X86_CR0_NE;

#ifdef HWACCM_VMX_EMULATE_REALMODE
        if (pVM->hwaccm.s.vmx.pRealModeTSS)
        {
            /* Note: We ignore the PE & PG bits here on purpose; we emulate real and protected mode without paging. */
            mask &= ~(X86_CR0_PG|X86_CR0_PE);
        }
        else
#endif
        {
            /* We support protected mode without paging using identity mapping. */
            mask &= ~X86_CR0_PG;
        }
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

        pVCpu->hwaccm.s.fActive = true;
        return true;
    }

    return false;
}

/**
 * Notifcation from EM about a rescheduling into hardware assisted execution
 * mode.
 *
 * @param   pVCpu       Pointer to the current virtual cpu structure.
 */
VMMR3DECL(void) HWACCMR3NotifyScheduled(PVMCPU pVCpu)
{
    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL_GUEST;
}

/**
 * Notifcation from EM about returning from instruction emulation (REM / EM).
 *
 * @param   pVCpu       Pointer to the current virtual cpu structure.
 */
VMMR3DECL(void) HWACCMR3NotifyEmulated(PVMCPU pVCpu)
{
    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL_GUEST;
}

/**
 * Checks if we are currently using hardware accelerated raw mode.
 *
 * @returns boolean
 * @param   pVCpu        The VMCPU to operate on.
 */
VMMR3DECL(bool) HWACCMR3IsActive(PVMCPU pVCpu)
{
    return pVCpu->hwaccm.s.fActive;
}

/**
 * Checks if we are currently using nested paging.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(bool) HWACCMR3IsNestedPagingActive(PVM pVM)
{
    return pVM->hwaccm.s.fNestedPaging;
}

/**
 * Checks if we are currently using VPID in VT-x mode.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(bool) HWACCMR3IsVPIDActive(PVM pVM)
{
    return pVM->hwaccm.s.vmx.fVPID;
}


/**
 * Checks if internal events are pending. In that case we are not allowed to dispatch interrupts.
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(bool) HWACCMR3IsEventPending(PVM pVM)
{
    /* @todo SMP */
    return HWACCMIsEnabled(pVM) && pVM->aCpus[0].hwaccm.s.Event.fPending;
}


/**
 * Inject an NMI into a running VM
 *
 * @returns boolean
 * @param   pVM         The VM to operate on.
 */
VMMR3DECL(int)  HWACCMR3InjectNMI(PVM pVM)
{
    pVM->hwaccm.s.fInjectNMI = true;
    return VINF_SUCCESS;
}

/**
 * Check fatal VT-x/AMD-V error and produce some meaningful
 * log release message.
 *
 * @param   pVM         The VM to operate on.
 * @param   iStatusCode VBox status code
 */
VMMR3DECL(void) HWACCMR3CheckError(PVM pVM, int iStatusCode)
{
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        switch(iStatusCode)
        {
        case VERR_VMX_INVALID_VMCS_FIELD:
            break;

        case VERR_VMX_INVALID_VMCS_PTR:
            LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current pointer %RGp vs %RGp\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.u64VMCSPhys, pVM->aCpus[i].hwaccm.s.vmx.pVMCSPhys));
            LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current VMCS version %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulVMCSRevision));
            LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Entered Cpu %d\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.idEnteredCpu));
            LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current Cpu %d\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.idCurrentCpu));
            break;

        case VERR_VMX_UNABLE_TO_START_VM:
            LogRel(("VERR_VMX_UNABLE_TO_START_VM: CPU%d instruction error %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulInstrError));
            LogRel(("VERR_VMX_UNABLE_TO_START_VM: CPU%d exit reason       %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulExitReason));
#if 0 /* @todo dump the current control fields to the release log */
            if (pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulInstrError == VMX_ERROR_VMENTRY_INVALID_CONTROL_FIELDS)
            {

            }
#endif
            break;

        case VERR_VMX_UNABLE_TO_RESUME_VM:
            LogRel(("VERR_VMX_UNABLE_TO_RESUME_VM: CPU%d instruction error %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulInstrError));
            LogRel(("VERR_VMX_UNABLE_TO_RESUME_VM: CPU%d exit reason       %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulExitReason));
            break;

        case VERR_VMX_INVALID_VMXON_PTR:
            break;
        }
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

    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        /*
         * Save the basic bits - fortunately all the other things can be resynced on load.
         */
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hwaccm.s.Event.fPending);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hwaccm.s.Event.errCode);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU64(pSSM, pVM->aCpus[i].hwaccm.s.Event.intInfo);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hwaccm.s.vmx.enmLastSeenGuestMode);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hwaccm.s.vmx.enmCurrGuestMode);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hwaccm.s.vmx.enmPrevGuestMode);
        AssertRCReturn(rc, rc);
    }

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
    if (   u32Version != HWACCM_SSM_VERSION
        && u32Version != HWACCM_SSM_VERSION_2_0_X)
    {
        AssertMsgFailed(("hwaccmR3Load: Invalid version u32Version=%d!\n", u32Version));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    for (unsigned i=0;i<pVM->cCPUs;i++)
    {
        rc = SSMR3GetU32(pSSM, &pVM->aCpus[i].hwaccm.s.Event.fPending);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &pVM->aCpus[i].hwaccm.s.Event.errCode);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU64(pSSM, &pVM->aCpus[i].hwaccm.s.Event.intInfo);
        AssertRCReturn(rc, rc);

        if (u32Version >= HWACCM_SSM_VERSION)
        {
            uint32_t val;

            rc = SSMR3GetU32(pSSM, &val);
            AssertRCReturn(rc, rc);
            pVM->aCpus[i].hwaccm.s.vmx.enmLastSeenGuestMode = (PGMMODE)val;

            rc = SSMR3GetU32(pSSM, &val);
            AssertRCReturn(rc, rc);
            pVM->aCpus[i].hwaccm.s.vmx.enmCurrGuestMode = (PGMMODE)val;

            rc = SSMR3GetU32(pSSM, &val);
            AssertRCReturn(rc, rc);
            pVM->aCpus[i].hwaccm.s.vmx.enmPrevGuestMode = (PGMMODE)val;
        }
    }
    return VINF_SUCCESS;
}

