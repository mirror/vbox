/* $Id$ */
/** @file
 * HM - Intel/AMD VM Hardware Support Manager.
 */

/*
 * Copyright (C) 2006-2012 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

/*******************************************************************************
*   Header Files                                                               *
*******************************************************************************/
#define LOG_GROUP LOG_GROUP_HM
#include <VBox/vmm/cpum.h>
#include <VBox/vmm/stam.h>
#include <VBox/vmm/mm.h>
#include <VBox/vmm/pdmapi.h>
#include <VBox/vmm/pgm.h>
#include <VBox/vmm/ssm.h>
#include <VBox/vmm/trpm.h>
#include <VBox/vmm/dbgf.h>
#include <VBox/vmm/iom.h>
#include <VBox/vmm/patm.h>
#include <VBox/vmm/csam.h>
#include <VBox/vmm/selm.h>
#ifdef VBOX_WITH_REM
# include <VBox/vmm/rem.h>
#endif
#include <VBox/vmm/hm_vmx.h>
#include <VBox/vmm/hm_svm.h>
#include "HMInternal.h"
#include <VBox/vmm/vm.h>
#include <VBox/err.h>
#include <VBox/param.h>

#include <iprt/assert.h>
#include <VBox/log.h>
#include <iprt/asm.h>
#include <iprt/asm-amd64-x86.h>
#include <iprt/string.h>
#include <iprt/env.h>
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
    EXIT_REASON(VMX_EXIT_XCPT_NMI           ,  0, "Exception or non-maskable interrupt (NMI)."),
    EXIT_REASON(VMX_EXIT_EXT_INT            ,  1, "External interrupt."),
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
    EXIT_REASON(VMX_EXIT_INVLPG             , 14, "Guest software attempted to execute INVLPG."),
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
    EXIT_REASON(VMX_EXIT_MTF                , 37, "Monitor Trap Flag."),
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
    EXIT_REASON(VMX_EXIT_RDTSCP             , 51, "Guest software attempted to execute RDTSCP."),
    EXIT_REASON(VMX_EXIT_PREEMPTION_TIMER   , 52, "VMX-preemption timer expired. The preemption timer counted down to zero."),
    EXIT_REASON(VMX_EXIT_INVVPID            , 53, "INVVPID. Guest software attempted to execute INVVPID."),
    EXIT_REASON(VMX_EXIT_WBINVD             , 54, "WBINVD. Guest software attempted to execute WBINVD."),
    EXIT_REASON(VMX_EXIT_XSETBV             , 55, "XSETBV. Guest software attempted to execute XSETBV."),
    EXIT_REASON_NIL()
};
/** Exit reason descriptions for AMD-V, used to describe statistics. */
static const char * const g_apszAmdVExitReasons[MAX_EXITREASON_STAT] =
{
    EXIT_REASON(SVM_EXIT_READ_CR0           ,  0, "Read CR0."),
    EXIT_REASON(SVM_EXIT_READ_CR1           ,  1, "Read CR1."),
    EXIT_REASON(SVM_EXIT_READ_CR2           ,  2, "Read CR2."),
    EXIT_REASON(SVM_EXIT_READ_CR3           ,  3, "Read CR3."),
    EXIT_REASON(SVM_EXIT_READ_CR4           ,  4, "Read CR4."),
    EXIT_REASON(SVM_EXIT_READ_CR5           ,  5, "Read CR5."),
    EXIT_REASON(SVM_EXIT_READ_CR6           ,  6, "Read CR6."),
    EXIT_REASON(SVM_EXIT_READ_CR7           ,  7, "Read CR7."),
    EXIT_REASON(SVM_EXIT_READ_CR8           ,  8, "Read CR8."),
    EXIT_REASON(SVM_EXIT_READ_CR9           ,  9, "Read CR9."),
    EXIT_REASON(SVM_EXIT_READ_CR10          , 10, "Read CR10."),
    EXIT_REASON(SVM_EXIT_READ_CR11          , 11, "Read CR11."),
    EXIT_REASON(SVM_EXIT_READ_CR12          , 12, "Read CR12."),
    EXIT_REASON(SVM_EXIT_READ_CR13          , 13, "Read CR13."),
    EXIT_REASON(SVM_EXIT_READ_CR14          , 14, "Read CR14."),
    EXIT_REASON(SVM_EXIT_READ_CR15          , 15, "Read CR15."),
    EXIT_REASON(SVM_EXIT_WRITE_CR0          , 16, "Write CR0."),
    EXIT_REASON(SVM_EXIT_WRITE_CR1          , 17, "Write CR1."),
    EXIT_REASON(SVM_EXIT_WRITE_CR2          , 18, "Write CR2."),
    EXIT_REASON(SVM_EXIT_WRITE_CR3          , 19, "Write CR3."),
    EXIT_REASON(SVM_EXIT_WRITE_CR4          , 20, "Write CR4."),
    EXIT_REASON(SVM_EXIT_WRITE_CR5          , 21, "Write CR5."),
    EXIT_REASON(SVM_EXIT_WRITE_CR6          , 22, "Write CR6."),
    EXIT_REASON(SVM_EXIT_WRITE_CR7          , 23, "Write CR7."),
    EXIT_REASON(SVM_EXIT_WRITE_CR8          , 24, "Write CR8."),
    EXIT_REASON(SVM_EXIT_WRITE_CR9          , 25, "Write CR9."),
    EXIT_REASON(SVM_EXIT_WRITE_CR10         , 26, "Write CR10."),
    EXIT_REASON(SVM_EXIT_WRITE_CR11         , 27, "Write CR11."),
    EXIT_REASON(SVM_EXIT_WRITE_CR12         , 28, "Write CR12."),
    EXIT_REASON(SVM_EXIT_WRITE_CR13         , 29, "Write CR13."),
    EXIT_REASON(SVM_EXIT_WRITE_CR14         , 30, "Write CR14."),
    EXIT_REASON(SVM_EXIT_WRITE_CR15         , 31, "Write CR15."),
    EXIT_REASON(SVM_EXIT_READ_DR0           , 32, "Read DR0."),
    EXIT_REASON(SVM_EXIT_READ_DR1           , 33, "Read DR1."),
    EXIT_REASON(SVM_EXIT_READ_DR2           , 34, "Read DR2."),
    EXIT_REASON(SVM_EXIT_READ_DR3           , 35, "Read DR3."),
    EXIT_REASON(SVM_EXIT_READ_DR4           , 36, "Read DR4."),
    EXIT_REASON(SVM_EXIT_READ_DR5           , 37, "Read DR5."),
    EXIT_REASON(SVM_EXIT_READ_DR6           , 38, "Read DR6."),
    EXIT_REASON(SVM_EXIT_READ_DR7           , 39, "Read DR7."),
    EXIT_REASON(SVM_EXIT_READ_DR8           , 40, "Read DR8."),
    EXIT_REASON(SVM_EXIT_READ_DR9           , 41, "Read DR9."),
    EXIT_REASON(SVM_EXIT_READ_DR10          , 42, "Read DR10."),
    EXIT_REASON(SVM_EXIT_READ_DR11          , 43, "Read DR11"),
    EXIT_REASON(SVM_EXIT_READ_DR12          , 44, "Read DR12."),
    EXIT_REASON(SVM_EXIT_READ_DR13          , 45, "Read DR13."),
    EXIT_REASON(SVM_EXIT_READ_DR14          , 46, "Read DR14."),
    EXIT_REASON(SVM_EXIT_READ_DR15          , 47, "Read DR15."),
    EXIT_REASON(SVM_EXIT_WRITE_DR0          , 48, "Write DR0."),
    EXIT_REASON(SVM_EXIT_WRITE_DR1          , 49, "Write DR1."),
    EXIT_REASON(SVM_EXIT_WRITE_DR2          , 50, "Write DR2."),
    EXIT_REASON(SVM_EXIT_WRITE_DR3          , 51, "Write DR3."),
    EXIT_REASON(SVM_EXIT_WRITE_DR4          , 52, "Write DR4."),
    EXIT_REASON(SVM_EXIT_WRITE_DR5          , 53, "Write DR5."),
    EXIT_REASON(SVM_EXIT_WRITE_DR6          , 54, "Write DR6."),
    EXIT_REASON(SVM_EXIT_WRITE_DR7          , 55, "Write DR7."),
    EXIT_REASON(SVM_EXIT_WRITE_DR8          , 56, "Write DR8."),
    EXIT_REASON(SVM_EXIT_WRITE_DR9          , 57, "Write DR9."),
    EXIT_REASON(SVM_EXIT_WRITE_DR10         , 58, "Write DR10."),
    EXIT_REASON(SVM_EXIT_WRITE_DR11         , 59, "Write DR11."),
    EXIT_REASON(SVM_EXIT_WRITE_DR12         , 60, "Write DR12."),
    EXIT_REASON(SVM_EXIT_WRITE_DR13         , 61, "Write DR13."),
    EXIT_REASON(SVM_EXIT_WRITE_DR14         , 62, "Write DR14."),
    EXIT_REASON(SVM_EXIT_WRITE_DR15         , 63, "Write DR15."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_0        , 64, "Exception Vector 0  (0x0)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1        , 65, "Exception Vector 1  (0x1)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_2        , 66, "Exception Vector 2  (0x2)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_3        , 67, "Exception Vector 3  (0x3)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_4        , 68, "Exception Vector 4  (0x4)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_5        , 69, "Exception Vector 5  (0x5)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_6        , 70, "Exception Vector 6  (0x6)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_7        , 71, "Exception Vector 7  (0x7)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_8        , 72, "Exception Vector 8  (0x8)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_9        , 73, "Exception Vector 9  (0x9)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_A        , 74, "Exception Vector 10 (0xA)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_B        , 75, "Exception Vector 11 (0xB)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_C        , 76, "Exception Vector 12 (0xC)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_D        , 77, "Exception Vector 13 (0xD)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_E        , 78, "Exception Vector 14 (0xE)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_F        , 79, "Exception Vector 15 (0xF)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_10       , 80, "Exception Vector 16 (0x10)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_11       , 81, "Exception Vector 17 (0x11)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_12       , 82, "Exception Vector 18 (0x12)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_13       , 83, "Exception Vector 19 (0x13)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_14       , 84, "Exception Vector 20 (0x14)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_15       , 85, "Exception Vector 22 (0x15)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_16       , 86, "Exception Vector 22 (0x16)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_17       , 87, "Exception Vector 23 (0x17)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_18       , 88, "Exception Vector 24 (0x18)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_19       , 89, "Exception Vector 25 (0x19)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1A       , 90, "Exception Vector 26 (0x1A)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1B       , 91, "Exception Vector 27 (0x1B)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1C       , 92, "Exception Vector 28 (0x1C)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1D       , 93, "Exception Vector 29 (0x1D)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1E       , 94, "Exception Vector 30 (0x1E)."),
    EXIT_REASON(SVM_EXIT_EXCEPTION_1F       , 95, "Exception Vector 31 (0x1F)."),
    EXIT_REASON(SVM_EXIT_INTR               , 96, "Physical maskable interrupt."),
    EXIT_REASON(SVM_EXIT_NMI                , 97, "Physical non-maskable interrupt."),
    EXIT_REASON(SVM_EXIT_SMI                , 98, "System management interrupt."),
    EXIT_REASON(SVM_EXIT_INIT               , 99, "Physical INIT signal."),
    EXIT_REASON(SVM_EXIT_VINTR              ,100, "Virtual interrupt."),
    EXIT_REASON(SVM_EXIT_CR0_SEL_WRITE      ,101, "Write to CR0 that changed any bits other than CR0.TS or CR0.MP."),
    EXIT_REASON(SVM_EXIT_IDTR_READ          ,102, "Read IDTR"),
    EXIT_REASON(SVM_EXIT_GDTR_READ          ,103, "Read GDTR"),
    EXIT_REASON(SVM_EXIT_LDTR_READ          ,104, "Read LDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ            ,105, "Read TR."),
    EXIT_REASON(SVM_EXIT_TR_READ            ,106, "Write IDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ            ,107, "Write GDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ            ,108, "Write LDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ            ,109, "Write TR."),
    EXIT_REASON(SVM_EXIT_RDTSC              ,110, "RDTSC instruction."),
    EXIT_REASON(SVM_EXIT_RDPMC              ,111, "RDPMC instruction."),
    EXIT_REASON(SVM_EXIT_PUSHF              ,112, "PUSHF instruction."),
    EXIT_REASON(SVM_EXIT_POPF               ,113, "POPF instruction."),
    EXIT_REASON(SVM_EXIT_CPUID              ,114, "CPUID instruction."),
    EXIT_REASON(SVM_EXIT_RSM                ,115, "RSM instruction."),
    EXIT_REASON(SVM_EXIT_IRET               ,116, "IRET instruction."),
    EXIT_REASON(SVM_EXIT_SWINT              ,117, "Software interrupt (INTn instructions)."),
    EXIT_REASON(SVM_EXIT_INVD               ,118, "INVD instruction."),
    EXIT_REASON(SVM_EXIT_PAUSE              ,119, "PAUSE instruction."),
    EXIT_REASON(SVM_EXIT_HLT                ,120, "HLT instruction."),
    EXIT_REASON(SVM_EXIT_INVLPG             ,121, "INVLPG instruction."),
    EXIT_REASON(SVM_EXIT_INVLPGA            ,122, "INVLPGA instruction."),
    EXIT_REASON(SVM_EXIT_IOIO               ,123, "IN/OUT accessing protected port (EXITINFO1 field provides more information)."),
    EXIT_REASON(SVM_EXIT_MSR                ,124, "RDMSR or WRMSR access to protected MSR."),
    EXIT_REASON(SVM_EXIT_TASK_SWITCH        ,125, "Task switch."),
    EXIT_REASON(SVM_EXIT_FERR_FREEZE        ,126, "FP legacy handling enabled, and processor is frozen in an x87/mmx instruction waiting for an interrupt"),
    EXIT_REASON(SVM_EXIT_SHUTDOWN           ,127, "Shutdown."),
    EXIT_REASON(SVM_EXIT_VMRUN              ,128, "VMRUN instruction."),
    EXIT_REASON(SVM_EXIT_VMMCALL            ,129, "VMCALL instruction."),
    EXIT_REASON(SVM_EXIT_VMLOAD             ,130, "VMLOAD instruction."),
    EXIT_REASON(SVM_EXIT_VMSAVE             ,131, "VMSAVE instruction."),
    EXIT_REASON(SVM_EXIT_STGI               ,132, "STGI instruction."),
    EXIT_REASON(SVM_EXIT_CLGI               ,133, "CLGI instruction."),
    EXIT_REASON(SVM_EXIT_SKINIT             ,134, "SKINIT instruction."),
    EXIT_REASON(SVM_EXIT_RDTSCP             ,135, "RDTSCP instruction."),
    EXIT_REASON(SVM_EXIT_ICEBP              ,136, "ICEBP instruction."),
    EXIT_REASON(SVM_EXIT_WBINVD             ,137, "WBINVD instruction."),
    EXIT_REASON(SVM_EXIT_MONITOR            ,138, "MONITOR instruction."),
    EXIT_REASON(SVM_EXIT_MWAIT_UNCOND       ,139, "MWAIT instruction unconditional."),
    EXIT_REASON(SVM_EXIT_MWAIT_ARMED        ,140, "MWAIT instruction when armed."),
    EXIT_REASON(SVM_EXIT_NPF                ,1024, "Nested paging: host-level page fault occurred (EXITINFO1 contains fault errorcode; EXITINFO2 contains the guest physical address causing the fault)."),
    EXIT_REASON_NIL()
};
# undef EXIT_REASON
# undef EXIT_REASON_NIL
#endif /* VBOX_WITH_STATISTICS */

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) hmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) hmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static int hmR3InitCPU(PVM pVM);
static int hmR3InitFinalizeR0(PVM pVM);
static int hmR3TermCPU(PVM pVM);


/**
 * Initializes the HM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int) HMR3Init(PVM pVM)
{
    LogFlow(("HMR3Init\n"));

    /*
     * Assert alignment and sizes.
     */
    AssertCompileMemberAlignment(VM, hm.s, 32);
    AssertCompile(sizeof(pVM->hm.s) <= sizeof(pVM->hm.padding));

    /* Some structure checks. */
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, ctrl.EventInject) == 0xA8, ("ctrl.EventInject offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.EventInject)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, ctrl.ExitIntInfo) == 0x88, ("ctrl.ExitIntInfo offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.ExitIntInfo)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, ctrl.TLBCtrl) == 0x58, ("ctrl.TLBCtrl offset = %x\n", RT_OFFSETOF(SVM_VMCB, ctrl.TLBCtrl)));

    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest) == 0x400, ("guest offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.TR) == 0x490, ("guest.TR offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.TR)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u8CPL) == 0x4CB, ("guest.u8CPL offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u8CPL)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64EFER) == 0x4D0, ("guest.u64EFER offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64EFER)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64CR4) == 0x548, ("guest.u64CR4 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64CR4)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64RIP) == 0x578, ("guest.u64RIP offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64RIP)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64RSP) == 0x5D8, ("guest.u64RSP offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64RSP)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64CR2) == 0x640, ("guest.u64CR2 offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64CR2)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64GPAT) == 0x668, ("guest.u64GPAT offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64GPAT)));
    AssertReleaseMsg(RT_OFFSETOF(SVM_VMCB, guest.u64LASTEXCPTO) == 0x690, ("guest.u64LASTEXCPTO offset = %x\n", RT_OFFSETOF(SVM_VMCB, guest.u64LASTEXCPTO)));
    AssertReleaseMsg(sizeof(SVM_VMCB) == 0x1000, ("SVM_VMCB size = %x\n", sizeof(SVM_VMCB)));

    /*
     * Register the saved state data unit.
     */
    int rc = SSMR3RegisterInternal(pVM, "HWACCM", 0, HM_SSM_VERSION, sizeof(HM),
                                   NULL, NULL, NULL,
                                   NULL, hmR3Save, NULL,
                                   NULL, hmR3Load, NULL);
    if (RT_FAILURE(rc))
        return rc;

    /* Misc initialisation. */
    pVM->hm.s.vmx.fSupported = false;
    pVM->hm.s.svm.fSupported = false;
    pVM->hm.s.vmx.fEnabled   = false;
    pVM->hm.s.svm.fEnabled   = false;

    pVM->hm.s.fNestedPaging  = false;
    pVM->hm.s.fLargePages    = false;

    /* Disabled by default. */
    pVM->fHMEnabled = false;

    /*
     * Check CFGM options.
     */
    PCFGMNODE pRoot      = CFGMR3GetRoot(pVM);
    PCFGMNODE pHWVirtExt = CFGMR3GetChild(pRoot, "HWVirtExt/");
    /* Nested paging: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "EnableNestedPaging", &pVM->hm.s.fAllowNestedPaging, false);
    AssertRC(rc);

    /* Large pages: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "EnableLargePages", &pVM->hm.s.fLargePages, false);
    AssertRC(rc);

    /* VT-x VPID: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "EnableVPID", &pVM->hm.s.vmx.fAllowVpid, false);
    AssertRC(rc);

    /* HM support must be explicitely enabled in the configuration file. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "Enabled", &pVM->hm.s.fAllowed, false);
    AssertRC(rc);

    /* TPR patching for 32 bits (Windows) guests with IO-APIC: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "TPRPatchingEnabled", &pVM->hm.s.fTRPPatchingAllowed, false);
    AssertRC(rc);

#ifdef RT_OS_DARWIN
    if (VMMIsHwVirtExtForced(pVM) != pVM->hm.s.fAllowed)
#else
    if (VMMIsHwVirtExtForced(pVM) && !pVM->hm.s.fAllowed)
#endif
    {
        AssertLogRelMsgFailed(("VMMIsHwVirtExtForced=%RTbool fAllowed=%RTbool\n",
                               VMMIsHwVirtExtForced(pVM), pVM->hm.s.fAllowed));
        return VERR_HM_CONFIG_MISMATCH;
    }

    if (VMMIsHwVirtExtForced(pVM))
        pVM->fHMEnabled = true;

#if HC_ARCH_BITS == 32
    /*
     * 64-bit mode is configurable and it depends on both the kernel mode and VT-x.
     * (To use the default, don't set 64bitEnabled in CFGM.)
     */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "64bitEnabled", &pVM->hm.s.fAllow64BitGuests, false);
    AssertLogRelRCReturn(rc, rc);
    if (pVM->hm.s.fAllow64BitGuests)
    {
# ifdef RT_OS_DARWIN
        if (!VMMIsHwVirtExtForced(pVM))
# else
        if (!pVM->hm.s.fAllowed)
# endif
            return VM_SET_ERROR(pVM, VERR_INVALID_PARAMETER, "64-bit guest support was requested without also enabling HWVirtEx (VT-x/AMD-V).");
    }
#else
    /*
     * On 64-bit hosts 64-bit guest support is enabled by default, but allow this to be overridden
     * via VBoxInternal/HWVirtExt/64bitEnabled=0. (ConsoleImpl2.cpp doesn't set this to false for 64-bit.)*
     */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "64bitEnabled", &pVM->hm.s.fAllow64BitGuests, true);
    AssertLogRelRCReturn(rc, rc);
#endif


    /*
     * Determine the init method for AMD-V and VT-x; either one global init for each host CPU
     *  or local init each time we wish to execute guest code.
     *
     *  Default false for Mac OS X and Windows due to the higher risk of conflicts with other hypervisors.
     */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "Exclusive", &pVM->hm.s.fGlobalInit,
#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
                            false
#else
                            true
#endif
                           );

    /* Max number of resume loops. */
    rc = CFGMR3QueryU32Def(pHWVirtExt, "MaxResumeLoops", &pVM->hm.s.cMaxResumeLoops, 0 /* set by R0 later */);
    AssertRC(rc);

    return rc;
}


/**
 * Initializes the per-VCPU HM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
static int hmR3InitCPU(PVM pVM)
{
    LogFlow(("HMR3InitCPU\n"));

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->hm.s.fActive = false;
    }

#ifdef VBOX_WITH_STATISTICS
    STAM_REG(pVM, &pVM->hm.s.StatTprPatchSuccess,   STAMTYPE_COUNTER, "/HM/TPR/Patch/Success",  STAMUNIT_OCCURENCES, "Number of times an instruction was successfully patched.");
    STAM_REG(pVM, &pVM->hm.s.StatTprPatchFailure,   STAMTYPE_COUNTER, "/HM/TPR/Patch/Failed",   STAMUNIT_OCCURENCES, "Number of unsuccessful patch attempts.");
    STAM_REG(pVM, &pVM->hm.s.StatTprReplaceSuccess, STAMTYPE_COUNTER, "/HM/TPR/Replace/Success",STAMUNIT_OCCURENCES, "Number of times an instruction was successfully patched.");
    STAM_REG(pVM, &pVM->hm.s.StatTprReplaceFailure, STAMTYPE_COUNTER, "/HM/TPR/Replace/Failed", STAMUNIT_OCCURENCES, "Number of unsuccessful patch attempts.");

    /*
     * Statistics.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        int    rc;

        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatPoke, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of RTMpPokeCpu",
                             "/PROF/HM/CPU%d/Poke", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatSpinPoke, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of poke wait",
                             "/PROF/HM/CPU%d/PokeWait", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatSpinPokeFailed, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of poke wait when RTMpPokeCpu fails",
                             "/PROF/HM/CPU%d/PokeWaitFailed", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatEntry, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of VMXR0RunGuestCode entry",
                             "/PROF/HM/CPU%d/SwitchToGC", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExit1, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of VMXR0RunGuestCode exit part 1",
                             "/PROF/HM/CPU%d/SwitchFromGC_1", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExit2, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of VMXR0RunGuestCode exit part 2",
                             "/PROF/HM/CPU%d/SwitchFromGC_2", i);
        AssertRC(rc);
# if 1 /* temporary for tracking down darwin holdup. */
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExit2Sub1, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Temporary - I/O",
                             "/PROF/HM/CPU%d/SwitchFromGC_2/Sub1", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExit2Sub2, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Temporary - CRx RWs",
                             "/PROF/HM/CPU%d/SwitchFromGC_2/Sub2", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExit2Sub3, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Temporary - Exceptions",
                             "/PROF/HM/CPU%d/SwitchFromGC_2/Sub3", i);
        AssertRC(rc);
# endif
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatInGC, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL,
                             "Profiling of vmlaunch",
                             "/PROF/HM/CPU%d/InGC", i);
        AssertRC(rc);

# if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatWorldSwitch3264, STAMTYPE_PROFILE, STAMVISIBILITY_USED,
                             STAMUNIT_TICKS_PER_CALL, "Profiling of the 32/64 switcher",
                             "/PROF/HM/CPU%d/Switcher3264", i);
        AssertRC(rc);
# endif

# define HM_REG_COUNTER(a, b) \
        rc = STAMR3RegisterF(pVM, a, STAMTYPE_COUNTER, STAMVISIBILITY_ALWAYS, STAMUNIT_OCCURENCES, "Profiling of vmlaunch", b, i); \
        AssertRC(rc);

        HM_REG_COUNTER(&pVCpu->hm.s.StatExitShadowNM,           "/HM/CPU%d/Exit/Trap/Shw/#NM");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestNM,            "/HM/CPU%d/Exit/Trap/Gst/#NM");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitShadowPF,           "/HM/CPU%d/Exit/Trap/Shw/#PF");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitShadowPFEM,         "/HM/CPU%d/Exit/Trap/Shw/#PF-EM");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestPF,            "/HM/CPU%d/Exit/Trap/Gst/#PF");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestUD,            "/HM/CPU%d/Exit/Trap/Gst/#UD");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestSS,            "/HM/CPU%d/Exit/Trap/Gst/#SS");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestNP,            "/HM/CPU%d/Exit/Trap/Gst/#NP");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestGP,            "/HM/CPU%d/Exit/Trap/Gst/#GP");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestMF,            "/HM/CPU%d/Exit/Trap/Gst/#MF");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestDE,            "/HM/CPU%d/Exit/Trap/Gst/#DE");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestDB,            "/HM/CPU%d/Exit/Trap/Gst/#DB");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestBP,            "/HM/CPU%d/Exit/Trap/Gst/#BP");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestXF,            "/HM/CPU%d/Exit/Trap/Gst/#XF");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitGuestXcpUnk,        "/HM/CPU%d/Exit/Trap/Gst/Other");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitInvlpg,             "/HM/CPU%d/Exit/Instr/Invlpg");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitInvd,               "/HM/CPU%d/Exit/Instr/Invd");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitWbinvd,             "/HM/CPU%d/Exit/Instr/Wbinvd");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPause,              "/HM/CPU%d/Exit/Instr/Pause");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCpuid,              "/HM/CPU%d/Exit/Instr/Cpuid");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitRdtsc,              "/HM/CPU%d/Exit/Instr/Rdtsc");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitRdtscp,             "/HM/CPU%d/Exit/Instr/Rdtscp");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitRdpmc,              "/HM/CPU%d/Exit/Instr/Rdpmc");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitRdrand,             "/HM/CPU%d/Exit/Instr/Rdrand");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitRdmsr,              "/HM/CPU%d/Exit/Instr/Rdmsr");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitWrmsr,              "/HM/CPU%d/Exit/Instr/Wrmsr");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMwait,              "/HM/CPU%d/Exit/Instr/Mwait");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMonitor,            "/HM/CPU%d/Exit/Instr/Monitor");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitDRxWrite,           "/HM/CPU%d/Exit/Instr/DR/Write");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitDRxRead,            "/HM/CPU%d/Exit/Instr/DR/Read");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitClts,               "/HM/CPU%d/Exit/Instr/CLTS");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitLmsw,               "/HM/CPU%d/Exit/Instr/LMSW");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitCli,                "/HM/CPU%d/Exit/Instr/Cli");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitSti,                "/HM/CPU%d/Exit/Instr/Sti");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPushf,              "/HM/CPU%d/Exit/Instr/Pushf");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPopf,               "/HM/CPU%d/Exit/Instr/Popf");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIret,               "/HM/CPU%d/Exit/Instr/Iret");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitInt,                "/HM/CPU%d/Exit/Instr/Int");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitHlt,                "/HM/CPU%d/Exit/Instr/Hlt");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitXdtrAccess,         "/HM/CPU%d/Exit/Instr/XdtrAccess");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIOWrite,            "/HM/CPU%d/Exit/IO/Write");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIORead,             "/HM/CPU%d/Exit/IO/Read");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIOStringWrite,      "/HM/CPU%d/Exit/IO/WriteString");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIOStringRead,       "/HM/CPU%d/Exit/IO/ReadString");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitIntWindow,          "/HM/CPU%d/Exit/IntWindow");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMaxResume,          "/HM/CPU%d/Exit/MaxResume");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPreemptPending,     "/HM/CPU%d/Exit/PreemptPending");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitPreemptTimer,       "/HM/CPU%d/Exit/PreemptTimer");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitTprBelowThreshold,  "/HM/CPU%d/Exit/TprBelowThreshold");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitTaskSwitch,         "/HM/CPU%d/Exit/TaskSwitch");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitMtf,                "/HM/CPU%d/Exit/MonitorTrapFlag");
        HM_REG_COUNTER(&pVCpu->hm.s.StatExitApicAccess,         "/HM/CPU%d/Exit/ApicAccess");

        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchGuestIrq,         "/HM/CPU%d/Switch/IrqPending");
        HM_REG_COUNTER(&pVCpu->hm.s.StatSwitchToR3,             "/HM/CPU%d/Switch/ToR3");

        HM_REG_COUNTER(&pVCpu->hm.s.StatIntInject,              "/HM/CPU%d/Irq/Inject");
        HM_REG_COUNTER(&pVCpu->hm.s.StatIntReinject,            "/HM/CPU%d/Irq/Reinject");
        HM_REG_COUNTER(&pVCpu->hm.s.StatPendingHostIrq,         "/HM/CPU%d/Irq/PendingOnHost");

        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPage,              "/HM/CPU%d/Flush/Page");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPageManual,        "/HM/CPU%d/Flush/Page/Virt");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPhysPageManual,    "/HM/CPU%d/Flush/Page/Phys");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlb,               "/HM/CPU%d/Flush/TLB");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbManual,         "/HM/CPU%d/Flush/TLB/Manual");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbCRxChange,      "/HM/CPU%d/Flush/TLB/CRx");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushPageInvlpg,        "/HM/CPU%d/Flush/Page/Invlpg");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbWorldSwitch,    "/HM/CPU%d/Flush/TLB/Switch");
        HM_REG_COUNTER(&pVCpu->hm.s.StatNoFlushTlbWorldSwitch,  "/HM/CPU%d/Flush/TLB/Skipped");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushAsid,              "/HM/CPU%d/Flush/TLB/ASID");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushNestedPaging,      "/HM/CPU%d/Flush/TLB/NestedPaging");
        HM_REG_COUNTER(&pVCpu->hm.s.StatFlushTlbInvlpga,        "/HM/CPU%d/Flush/TLB/PhysInvl");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTlbShootdown,           "/HM/CPU%d/Flush/Shootdown/Page");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTlbShootdownFlush,      "/HM/CPU%d/Flush/Shootdown/TLB");

        HM_REG_COUNTER(&pVCpu->hm.s.StatTscOffset,              "/HM/CPU%d/TSC/Offset");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTscIntercept,           "/HM/CPU%d/TSC/Intercept");
        HM_REG_COUNTER(&pVCpu->hm.s.StatTscInterceptOverFlow,   "/HM/CPU%d/TSC/InterceptOverflow");

        HM_REG_COUNTER(&pVCpu->hm.s.StatDRxArmed,               "/HM/CPU%d/Debug/Armed");
        HM_REG_COUNTER(&pVCpu->hm.s.StatDRxContextSwitch,       "/HM/CPU%d/Debug/ContextSwitch");
        HM_REG_COUNTER(&pVCpu->hm.s.StatDRxIoCheck,             "/HM/CPU%d/Debug/IOCheck");

        HM_REG_COUNTER(&pVCpu->hm.s.StatLoadMinimal,            "/HM/CPU%d/Load/Minimal");
        HM_REG_COUNTER(&pVCpu->hm.s.StatLoadFull,               "/HM/CPU%d/Load/Full");

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        HM_REG_COUNTER(&pVCpu->hm.s.StatFpu64SwitchBack,        "/HM/CPU%d/Switch64/Fpu");
        HM_REG_COUNTER(&pVCpu->hm.s.StatDebug64SwitchBack,      "/HM/CPU%d/Switch64/Debug");
#endif

        for (unsigned j = 0; j < RT_ELEMENTS(pVCpu->hm.s.StatExitCRxWrite); j++)
        {
            rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitCRxWrite[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED,
                                 STAMUNIT_OCCURENCES, "Profiling of CRx writes",
                                 "/HM/CPU%d/Exit/Instr/CR/Write/%x", i, j);
            AssertRC(rc);
            rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitCRxRead[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED,
                                 STAMUNIT_OCCURENCES, "Profiling of CRx reads",
                                 "/HM/CPU%d/Exit/Instr/CR/Read/%x", i, j);
            AssertRC(rc);
        }

#undef HM_REG_COUNTER

        pVCpu->hm.s.paStatExitReason = NULL;

        rc = MMHyperAlloc(pVM, MAX_EXITREASON_STAT*sizeof(*pVCpu->hm.s.paStatExitReason), 0, MM_TAG_HM,
                          (void **)&pVCpu->hm.s.paStatExitReason);
        AssertRC(rc);
        if (RT_SUCCESS(rc))
        {
            const char * const *papszDesc = ASMIsIntelCpu() ? &g_apszVTxExitReasons[0] : &g_apszAmdVExitReasons[0];
            for (int j = 0; j < MAX_EXITREASON_STAT; j++)
            {
                if (papszDesc[j])
                {
                    rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.paStatExitReason[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED,
                                         STAMUNIT_OCCURENCES, papszDesc[j], "/HM/CPU%d/Exit/Reason/%02x", i, j);
                    AssertRC(rc);
                }
            }
            rc = STAMR3RegisterF(pVM, &pVCpu->hm.s.StatExitReasonNpf, STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                 "Nested page fault", "/HM/CPU%d/Exit/Reason/#NPF", i);
            AssertRC(rc);
        }
        pVCpu->hm.s.paStatExitReasonR0 = MMHyperR3ToR0(pVM, pVCpu->hm.s.paStatExitReason);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hm.s.paStatExitReasonR0 != NIL_RTR0PTR || !VMMIsHwVirtExtForced(pVM));
# else
        Assert(pVCpu->hm.s.paStatExitReasonR0 != NIL_RTR0PTR);
# endif

        rc = MMHyperAlloc(pVM, sizeof(STAMCOUNTER) * 256, 8, MM_TAG_HM, (void **)&pVCpu->hm.s.paStatInjectedIrqs);
        AssertRCReturn(rc, rc);
        pVCpu->hm.s.paStatInjectedIrqsR0 = MMHyperR3ToR0(pVM, pVCpu->hm.s.paStatInjectedIrqs);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hm.s.paStatInjectedIrqsR0 != NIL_RTR0PTR || !VMMIsHwVirtExtForced(pVM));
# else
        Assert(pVCpu->hm.s.paStatInjectedIrqsR0 != NIL_RTR0PTR);
# endif
        for (unsigned j = 0; j < 255; j++)
        {
            STAMR3RegisterF(pVM, &pVCpu->hm.s.paStatInjectedIrqs[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                            "Forwarded interrupts.",
                            (j < 0x20) ? "/HM/CPU%d/Interrupt/Trap/%02X" : "/HM/CPU%d/Interrupt/IRQ/%02X", i, j);
        }

    }
#endif /* VBOX_WITH_STATISTICS */

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /* Magic marker for searching in crash dumps. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;
        strcpy((char *)pCache->aMagic, "VMCSCACHE Magic");
        pCache->uMagic = UINT64_C(0xDEADBEEFDEADBEEF);
    }
#endif
    return VINF_SUCCESS;
}


/**
 * Called when a init phase has completed.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM.
 * @param   enmWhat             The phase that completed.
 */
VMMR3_INT_DECL(int) HMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    switch (enmWhat)
    {
        case VMINITCOMPLETED_RING3:
            return hmR3InitCPU(pVM);
        case VMINITCOMPLETED_RING0:
            return hmR3InitFinalizeR0(pVM);
        default:
            return VINF_SUCCESS;
    }
}


/**
 * Turns off normal raw mode features.
 *
 * @param   pVM         Pointer to the VM.
 */
static void hmR3DisableRawMode(PVM pVM)
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
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        PGMR3ChangeMode(pVM, pVCpu, PGMMODE_REAL);
    }
}


/**
 * Initialize VT-x or AMD-V.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
static int hmR3InitFinalizeR0(PVM pVM)
{
    int rc;

    /*
     * Hack to allow users to work around broken BIOSes that incorrectly set EFER.SVME, which makes us believe somebody else
     * is already using AMD-V.
     */
    if (    !pVM->hm.s.vmx.fSupported
        &&  !pVM->hm.s.svm.fSupported
        &&  pVM->hm.s.lLastError == VERR_SVM_IN_USE /* implies functional AMD-V */
        &&  RTEnvExist("VBOX_HWVIRTEX_IGNORE_SVM_IN_USE"))
    {
        LogRel(("HM: VBOX_HWVIRTEX_IGNORE_SVM_IN_USE active!\n"));
        pVM->hm.s.svm.fSupported        = true;
        pVM->hm.s.svm.fIgnoreInUseError = true;
    }
    else
    if (    !pVM->hm.s.vmx.fSupported
        &&  !pVM->hm.s.svm.fSupported)
    {
        LogRel(("HM: No VT-x or AMD-V CPU extension found. Reason %Rrc\n", pVM->hm.s.lLastError));
        LogRel(("HM: VMX MSR_IA32_FEATURE_CONTROL=%RX64\n", pVM->hm.s.vmx.msr.feature_ctrl));

        if (VMMIsHwVirtExtForced(pVM))
        {
            switch (pVM->hm.s.lLastError)
            {
            case VERR_VMX_NO_VMX:
                return VM_SET_ERROR(pVM, VERR_VMX_NO_VMX, "VT-x is not available.");
            case VERR_VMX_IN_VMX_ROOT_MODE:
                return VM_SET_ERROR(pVM, VERR_VMX_IN_VMX_ROOT_MODE, "VT-x is being used by another hypervisor.");
            case VERR_SVM_IN_USE:
                return VM_SET_ERROR(pVM, VERR_SVM_IN_USE, "AMD-V is being used by another hypervisor.");
            case VERR_SVM_NO_SVM:
                return VM_SET_ERROR(pVM, VERR_SVM_NO_SVM, "AMD-V is not available.");
            case VERR_SVM_DISABLED:
                return VM_SET_ERROR(pVM, VERR_SVM_DISABLED, "AMD-V is disabled in the BIOS.");
            default:
                return pVM->hm.s.lLastError;
            }
        }
        return VINF_SUCCESS;
    }

    if (pVM->hm.s.vmx.fSupported)
    {
        rc = SUPR3QueryVTxSupported();
        if (RT_FAILURE(rc))
        {
#ifdef RT_OS_LINUX
            LogRel(("HM: The host kernel does not support VT-x -- Linux 2.6.13 or newer required!\n"));
#else
            LogRel(("HM: The host kernel does not support VT-x!\n"));
#endif
            if (   pVM->cCpus > 1
                || VMMIsHwVirtExtForced(pVM))
                return rc;

            /* silently fall back to raw mode */
            return VINF_SUCCESS;
        }
    }

    if (!pVM->hm.s.fAllowed)
        return VINF_SUCCESS;    /* nothing to do */

    /* Enable VT-x or AMD-V on all host CPUs. */
    rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HM_ENABLE, 0, NULL);
    if (RT_FAILURE(rc))
    {
        LogRel(("HMR3InitFinalize: SUPR3CallVMMR0Ex VMMR0_DO_HM_ENABLE failed with %Rrc\n", rc));
        return rc;
    }
    Assert(!pVM->fHMEnabled || VMMIsHwVirtExtForced(pVM));

    pVM->hm.s.fHasIoApic = PDMHasIoApic(pVM);
    /* No TPR patching is required when the IO-APIC is not enabled for this VM. (Main should have taken care of this already) */
    if (!pVM->hm.s.fHasIoApic)
    {
        Assert(!pVM->hm.s.fTRPPatchingAllowed); /* paranoia */
        pVM->hm.s.fTRPPatchingAllowed = false;
    }

    bool fOldBuffered = RTLogRelSetBuffering(true /*fBuffered*/);
    if (pVM->hm.s.vmx.fSupported)
    {
        Log(("pVM->hm.s.vmx.fSupported = %d\n", pVM->hm.s.vmx.fSupported));

        if (    pVM->hm.s.fInitialized == false
            &&  pVM->hm.s.vmx.msr.feature_ctrl != 0)
        {
            uint64_t val;
            RTGCPHYS GCPhys = 0;

            LogRel(("HM: Host CR4=%08X\n", pVM->hm.s.vmx.hostCR4));
            LogRel(("HM: MSR_IA32_FEATURE_CONTROL      = %RX64\n", pVM->hm.s.vmx.msr.feature_ctrl));
            LogRel(("HM: MSR_IA32_VMX_BASIC_INFO       = %RX64\n", pVM->hm.s.vmx.msr.vmx_basic_info));
            LogRel(("HM: VMCS id                       = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_ID(pVM->hm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HM: VMCS size                     = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(pVM->hm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HM: VMCS physical address limit   = %s\n", MSR_IA32_VMX_BASIC_INFO_VMCS_PHYS_WIDTH(pVM->hm.s.vmx.msr.vmx_basic_info) ? "< 4 GB" : "None"));
            LogRel(("HM: VMCS memory type              = %x\n", MSR_IA32_VMX_BASIC_INFO_VMCS_MEM_TYPE(pVM->hm.s.vmx.msr.vmx_basic_info)));
            LogRel(("HM: Dual monitor treatment        = %d\n", MSR_IA32_VMX_BASIC_INFO_VMCS_DUAL_MON(pVM->hm.s.vmx.msr.vmx_basic_info)));

            LogRel(("HM: MSR_IA32_VMX_PINBASED_CTLS    = %RX64\n", pVM->hm.s.vmx.msr.vmx_pin_ctls.u));
            val = pVM->hm.s.vmx.msr.vmx_pin_ctls.n.allowed1;
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER\n"));
            val = pVM->hm.s.vmx.msr.vmx_pin_ctls.n.disallowed0;
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER)
                LogRel(("HM:    VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER *must* be set\n"));

            LogRel(("HM: MSR_IA32_VMX_PROCBASED_CTLS   = %RX64\n", pVM->hm.s.vmx.msr.vmx_proc_ctls.u));
            val = pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1;
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INT_WINDOW_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INT_WINDOW_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL\n"));

            val = pVM->hm.s.vmx.msr.vmx_proc_ctls.n.disallowed0;
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INT_WINDOW_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INT_WINDOW_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL *must* be set\n"));

            if (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
            {
                LogRel(("HM: MSR_IA32_VMX_PROCBASED_CTLS2  = %RX64\n", pVM->hm.s.vmx.msr.vmx_proc_ctls2.u));
                val = pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_X2APIC)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT\n"));

                val = pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_TABLE_EXIT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_X2APIC)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT)
                    LogRel(("HM:    VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT *must* be set\n"));
            }

            LogRel(("HM: MSR_IA32_VMX_ENTRY_CTLS       = %RX64\n", pVM->hm.s.vmx.msr.vmx_entry.u));
            val = pVM->hm.s.vmx.msr.vmx_entry.n.allowed1;
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_IA32E_MODE_GUEST)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_IA32E_MODE_GUEST\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR\n"));
            val = pVM->hm.s.vmx.msr.vmx_entry.n.disallowed0;
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_IA32E_MODE_GUEST)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_IA32E_MODE_GUEST *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR *must* be set\n"));

            LogRel(("HM: MSR_IA32_VMX_EXIT_CTLS        = %RX64\n", pVM->hm.s.vmx.msr.vmx_exit.u));
            val = pVM->hm.s.vmx.msr.vmx_exit.n.allowed1;
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_ADDR_SPACE_SIZE)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_ADDR_SPACE_SIZE\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXT_INT)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXT_INT\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER\n"));
            val = pVM->hm.s.vmx.msr.vmx_exit.n.disallowed0;
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_ADDR_SPACE_SIZE)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_ADDR_SPACE_SIZE *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXT_INT)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXT_INT *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR *must* be set\n"));
            if (val & VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER)
                LogRel(("HM:    VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER *must* be set\n"));

            if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps)
            {
                LogRel(("HM: MSR_IA32_VMX_EPT_VPID_CAP     = %RX64\n", pVM->hm.s.vmx.msr.vmx_ept_vpid_caps));

                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_RWX_X_ONLY)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_RWX_X_ONLY\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_RWX_W_ONLY)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_RWX_W_ONLY\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_RWX_WX_ONLY)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_RWX_WX_ONLY\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_GAW_21_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_GAW_21_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_GAW_30_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_GAW_30_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_GAW_39_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_GAW_39_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_GAW_48_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_GAW_48_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_GAW_57_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_GAW_57_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_UC)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_EMT_UC\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_WC)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_EMT_WC\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_WT)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_EMT_WT\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_WP)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_EMT_WP\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_SP_21_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_SP_21_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_SP_30_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_SP_30_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_SP_39_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_SP_39_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_SP_48_BITS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_SP_48_BITS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVEPT\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVVPID\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS\n"));
                if (pVM->hm.s.vmx.msr.vmx_ept_vpid_caps & MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS)
                    LogRel(("HM:    MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS\n"));
            }

            LogRel(("HM: MSR_IA32_VMX_MISC             = %RX64\n", pVM->hm.s.vmx.msr.vmx_misc));
            if (MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hm.s.vmx.msr.vmx_misc) == pVM->hm.s.vmx.cPreemptTimerShift)
                LogRel(("HM:    MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT %x\n", MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hm.s.vmx.msr.vmx_misc)));
            else
            {
                LogRel(("HM:    MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT %x - erratum detected, using %x instead\n",
                        MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hm.s.vmx.msr.vmx_misc), pVM->hm.s.vmx.cPreemptTimerShift));
            }
            LogRel(("HM:    MSR_IA32_VMX_MISC_ACTIVITY_STATES %x\n", MSR_IA32_VMX_MISC_ACTIVITY_STATES(pVM->hm.s.vmx.msr.vmx_misc)));
            LogRel(("HM:    MSR_IA32_VMX_MISC_CR3_TARGET      %x\n", MSR_IA32_VMX_MISC_CR3_TARGET(pVM->hm.s.vmx.msr.vmx_misc)));
            LogRel(("HM:    MSR_IA32_VMX_MISC_MAX_MSR         %x\n", MSR_IA32_VMX_MISC_MAX_MSR(pVM->hm.s.vmx.msr.vmx_misc)));
            LogRel(("HM:    MSR_IA32_VMX_MISC_MSEG_ID         %x\n", MSR_IA32_VMX_MISC_MSEG_ID(pVM->hm.s.vmx.msr.vmx_misc)));

            LogRel(("HM: MSR_IA32_VMX_CR0_FIXED0       = %RX64\n", pVM->hm.s.vmx.msr.vmx_cr0_fixed0));
            LogRel(("HM: MSR_IA32_VMX_CR0_FIXED1       = %RX64\n", pVM->hm.s.vmx.msr.vmx_cr0_fixed1));
            LogRel(("HM: MSR_IA32_VMX_CR4_FIXED0       = %RX64\n", pVM->hm.s.vmx.msr.vmx_cr4_fixed0));
            LogRel(("HM: MSR_IA32_VMX_CR4_FIXED1       = %RX64\n", pVM->hm.s.vmx.msr.vmx_cr4_fixed1));
            LogRel(("HM: MSR_IA32_VMX_VMCS_ENUM        = %RX64\n", pVM->hm.s.vmx.msr.vmx_vmcs_enum));

            LogRel(("HM: APIC-access page physaddr     = %RHp\n", pVM->hm.s.vmx.HCPhysApicAccess));

            /* Paranoia */
            AssertRelease(MSR_IA32_VMX_MISC_MAX_MSR(pVM->hm.s.vmx.msr.vmx_misc) >= 512);

            for (VMCPUID i = 0; i < pVM->cCpus; i++)
            {
                LogRel(("HM: VCPU%d: MSR bitmap physaddr    = %RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysMsrBitmap));
                LogRel(("HM: VCPU%d: VMCS physaddr          = %RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysVMCS));
            }

            if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                pVM->hm.s.fNestedPaging = pVM->hm.s.fAllowNestedPaging;

            if (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                pVM->hm.s.vmx.fVpid = pVM->hm.s.vmx.fAllowVpid;

            /*
             * Disallow RDTSCP in the guest if there is no secondary process-based VM execution controls as otherwise
             * RDTSCP would cause a #UD. There might be no CPUs out there where this happens, as RDTSCP was introduced
             * in Nehalems and secondary VM exec. controls should be supported in all of them, but nonetheless it's Intel...
             */
            if (!(pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                && CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP))
            {
                CPUMClearGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP);
            }

            /* Unrestricted guest execution relies on EPT. */
            if (    pVM->hm.s.fNestedPaging
                &&  (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE))
            {
                pVM->hm.s.vmx.fUnrestrictedGuest = true;
            }

            /* Only try once. */
            pVM->hm.s.fInitialized = true;

            if (!pVM->hm.s.vmx.fUnrestrictedGuest)
            {
                /* Allocate three pages for the TSS we need for real mode emulation. (2 pages for the IO bitmap) */
                rc = PDMR3VmmDevHeapAlloc(pVM, HM_VTX_TOTAL_DEVHEAP_MEM, (RTR3PTR *)&pVM->hm.s.vmx.pRealModeTSS);
                if (RT_SUCCESS(rc))
                {
                    /* The I/O bitmap starts right after the virtual interrupt redirection bitmap. */
                    /* Refer Intel spec. 20.3.3 "Software Interrupt Handling in Virtual-8086 mode" esp. Figure 20-5.*/
                    ASMMemZero32(pVM->hm.s.vmx.pRealModeTSS, sizeof(*pVM->hm.s.vmx.pRealModeTSS));
                    pVM->hm.s.vmx.pRealModeTSS->offIoBitmap = sizeof(*pVM->hm.s.vmx.pRealModeTSS);
                    /* Bit set to 0 means software interrupts are redirected to the 8086 program interrupt handler rather than
                       switching to protected-mode handler. */
                    memset(pVM->hm.s.vmx.pRealModeTSS->IntRedirBitmap, 0, sizeof(pVM->hm.s.vmx.pRealModeTSS->IntRedirBitmap));
                    /* Allow all port IO, so that port IO instructions do not cause exceptions and would instead
                       cause a VM-exit (based on VT-x's IO bitmap which we currently configure to always cause an exit). */
                    memset(pVM->hm.s.vmx.pRealModeTSS + 1, 0, PAGE_SIZE * 2);
                    *((unsigned char *)pVM->hm.s.vmx.pRealModeTSS + HM_VTX_TSS_SIZE - 2) = 0xff;

                    /*
                     * Construct a 1024 element page directory with 4 MB pages for the identity mapped page table used in
                     * real and protected mode without paging with EPT.
                     */
                    pVM->hm.s.vmx.pNonPagingModeEPTPageTable = (PX86PD)((char *)pVM->hm.s.vmx.pRealModeTSS + PAGE_SIZE * 3);
                    for (unsigned i = 0; i < X86_PG_ENTRIES; i++)
                    {
                        pVM->hm.s.vmx.pNonPagingModeEPTPageTable->a[i].u  = _4M * i;
                        pVM->hm.s.vmx.pNonPagingModeEPTPageTable->a[i].u |= X86_PDE4M_P | X86_PDE4M_RW | X86_PDE4M_US
                                                                            | X86_PDE4M_A | X86_PDE4M_D | X86_PDE4M_PS
                                                                            | X86_PDE4M_G;
                    }

                    /* We convert it here every time as pci regions could be reconfigured. */
                    rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pRealModeTSS, &GCPhys);
                    AssertRC(rc);
                    LogRel(("HM: Real Mode TSS guest physaddr  = %RGp\n", GCPhys));

                    rc = PDMVmmDevHeapR3ToGCPhys(pVM, pVM->hm.s.vmx.pNonPagingModeEPTPageTable, &GCPhys);
                    AssertRC(rc);
                    LogRel(("HM: Non-Paging Mode EPT CR3       = %RGp\n", GCPhys));
                }
                else
                {
                    /** @todo This cannot possibly work, there are other places which assumes
                     *        this allocation cannot fail (see HMR3CanExecuteGuest()). Make this
                     *        a failure case. */
                    LogRel(("HM: No real mode VT-x support (PDMR3VMMDevHeapAlloc returned %Rrc)\n", rc));
                    pVM->hm.s.vmx.pRealModeTSS = NULL;
                    pVM->hm.s.vmx.pNonPagingModeEPTPageTable = NULL;
                }
            }

            rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HM_SETUP_VM, 0, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                pVM->fHMEnabled = true;
                pVM->hm.s.vmx.fEnabled = true;
                hmR3DisableRawMode(pVM);

                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                if (pVM->hm.s.fAllow64BitGuests)
                {
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);            /* 64 bits only on Intel CPUs */
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
                }
                else
                /* Turn on NXE if PAE has been enabled *and* the host has turned on NXE (we reuse the host EFER in the switcher) */
                /* Todo: this needs to be fixed properly!! */
                if (    CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE)
                    &&  (pVM->hm.s.vmx.hostEFER & MSR_K6_EFER_NXE))
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);

                LogRel((pVM->hm.s.fAllow64BitGuests
                        ? "HM: 32-bit and 64-bit guests supported.\n"
                        : "HM: 32-bit guests supported.\n"));
#else
                LogRel(("HM: 32-bit guests supported.\n"));
#endif
                LogRel(("HM: VMX enabled!\n"));
                if (pVM->hm.s.fNestedPaging)
                {
                    LogRel(("HM: Enabled nested paging\n"));
                    LogRel(("HM: EPT root page                 = %RHp\n", PGMGetHyperCR3(VMMGetCpu(pVM))));
                    if (pVM->hm.s.vmx.enmFlushEpt == VMX_FLUSH_EPT_SINGLE_CONTEXT)
                        LogRel(("HM: enmFlushEpt                   = VMX_FLUSH_EPT_SINGLE_CONTEXT\n"));
                    else if (pVM->hm.s.vmx.enmFlushEpt == VMX_FLUSH_EPT_ALL_CONTEXTS)
                        LogRel(("HM: enmFlushEpt                   = VMX_FLUSH_EPT_ALL_CONTEXTS\n"));
                    else if (pVM->hm.s.vmx.enmFlushEpt == VMX_FLUSH_EPT_NOT_SUPPORTED)
                        LogRel(("HM: enmFlushEpt                   = VMX_FLUSH_EPT_NOT_SUPPORTED\n"));
                    else
                        LogRel(("HM: enmFlushEpt                   = %d\n", pVM->hm.s.vmx.enmFlushEpt));

                    if (pVM->hm.s.vmx.fUnrestrictedGuest)
                        LogRel(("HM: Unrestricted guest execution enabled!\n"));

#if HC_ARCH_BITS == 64
                    if (pVM->hm.s.fLargePages)
                    {
                        /* Use large (2 MB) pages for our EPT PDEs where possible. */
                        PGMSetLargePageUsage(pVM, true);
                        LogRel(("HM: Large page support enabled!\n"));
                    }
#endif
                }
                else
                    Assert(!pVM->hm.s.vmx.fUnrestrictedGuest);

                if (pVM->hm.s.vmx.fVpid)
                {
                    LogRel(("HM: Enabled VPID\n"));
                    if (pVM->hm.s.vmx.enmFlushVpid == VMX_FLUSH_VPID_INDIV_ADDR)
                        LogRel(("HM: enmFlushVpid                  = VMX_FLUSH_VPID_INDIV_ADDR\n"));
                    else if (pVM->hm.s.vmx.enmFlushVpid == VMX_FLUSH_VPID_SINGLE_CONTEXT)
                        LogRel(("HM: enmFlushVpid                  = VMX_FLUSH_VPID_SINGLE_CONTEXT\n"));
                    else if (pVM->hm.s.vmx.enmFlushVpid == VMX_FLUSH_VPID_ALL_CONTEXTS)
                        LogRel(("HM: enmFlushVpid                  = VMX_FLUSH_VPID_ALL_CONTEXTS\n"));
                    else if (pVM->hm.s.vmx.enmFlushVpid == VMX_FLUSH_VPID_SINGLE_CONTEXT_RETAIN_GLOBALS)
                        LogRel(("HM: enmFlushVpid                  = VMX_FLUSH_VPID_SINGLE_CONTEXT_RETAIN_GLOBALS\n"));
                    else
                        LogRel(("HM: enmFlushVpid                  = %d\n", pVM->hm.s.vmx.enmFlushVpid));
                }
                else if (pVM->hm.s.vmx.enmFlushVpid == VMX_FLUSH_VPID_NOT_SUPPORTED)
                    LogRel(("HM: Ignoring VPID capabilities of CPU.\n"));

                /* TPR patching status logging. */
                if (pVM->hm.s.fTRPPatchingAllowed)
                {
                    if (    (pVM->hm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                        &&  (pVM->hm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC))
                    {
                        pVM->hm.s.fTRPPatchingAllowed = false;  /* not necessary as we have a hardware solution. */
                        LogRel(("HM: TPR Patching not required (VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC).\n"));
                    }
                    else
                    {
                        uint32_t u32Eax, u32Dummy;

                        /* TPR patching needs access to the MSR_K8_LSTAR msr. */
                        ASMCpuId(0x80000000, &u32Eax, &u32Dummy, &u32Dummy, &u32Dummy);
                        if (    u32Eax < 0x80000001
                            ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE))
                        {
                            pVM->hm.s.fTRPPatchingAllowed = false;
                            LogRel(("HM: TPR patching disabled (long mode not supported).\n"));
                        }
                    }
                }
                LogRel(("HM: TPR Patching %s.\n", (pVM->hm.s.fTRPPatchingAllowed) ? "enabled" : "disabled"));

                /*
                 * Check for preemption timer config override and log the state of it.
                 */
                if (pVM->hm.s.vmx.fUsePreemptTimer)
                {
                    PCFGMNODE pCfgHm = CFGMR3GetChild(CFGMR3GetRoot(pVM), "HM");
                    int rc2 = CFGMR3QueryBoolDef(pCfgHm, "UsePreemptTimer", &pVM->hm.s.vmx.fUsePreemptTimer, true);
                    AssertLogRelRC(rc2);
                }
                if (pVM->hm.s.vmx.fUsePreemptTimer)
                    LogRel(("HM: Using the VMX-preemption timer (cPreemptTimerShift=%u)\n", pVM->hm.s.vmx.cPreemptTimerShift));
            }
            else
            {
                LogRel(("HM: VMX setup failed with rc=%Rrc!\n", rc));
                for (VMCPUID i = 0; i < pVM->cCpus; i++)
                    LogRel(("HM: CPU[%ld] Last instruction error %x\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u32InstrError));
                pVM->fHMEnabled = false;
            }
        }
    }
    else
    if (pVM->hm.s.svm.fSupported)
    {
        Log(("pVM->hm.s.svm.fSupported = %d\n", pVM->hm.s.svm.fSupported));

        if (pVM->hm.s.fInitialized == false)
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
                LogRel(("HM: AMD cpu with erratum 170 family %x model %x stepping %x\n", u32Family, u32Model, u32Stepping));
            }

            LogRel(("HM: cpuid 0x80000001.u32AMDFeatureECX = %RX32\n", pVM->hm.s.cpuid.u32AMDFeatureECX));
            LogRel(("HM: cpuid 0x80000001.u32AMDFeatureEDX = %RX32\n", pVM->hm.s.cpuid.u32AMDFeatureEDX));
            LogRel(("HM: AMD HWCR MSR                      = %RX64\n", pVM->hm.s.svm.msrHwcr));
            LogRel(("HM: AMD-V revision                    = %X\n", pVM->hm.s.svm.u32Rev));
            LogRel(("HM: AMD-V max ASID                    = %d\n", pVM->hm.s.uMaxAsid));
            LogRel(("HM: AMD-V features                    = %X\n", pVM->hm.s.svm.u32Features));
            static const struct { uint32_t fFlag; const char *pszName; } s_aSvmFeatures[] =
            {
#define FLAG_NAME(a_Define) { a_Define, #a_Define }
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_LBR_VIRT),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_SVM_LOCK),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_NRIP_SAVE),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_TSC_RATE_MSR),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_VMCB_CLEAN),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_FLUSH_BY_ASID),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_DECODE_ASSIST),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_SSE_3_5_DISABLE),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_PAUSE_FILTER),
                FLAG_NAME(AMD_CPUID_SVM_FEATURE_EDX_PAUSE_FILTER),
#undef FLAG_NAME
            };
            uint32_t fSvmFeatures = pVM->hm.s.svm.u32Features;
            for (unsigned i = 0; i < RT_ELEMENTS(s_aSvmFeatures); i++)
                if (fSvmFeatures & s_aSvmFeatures[i].fFlag)
                {
                    LogRel(("HM:    %s\n", s_aSvmFeatures[i].pszName));
                    fSvmFeatures &= ~s_aSvmFeatures[i].fFlag;
                }
            if (fSvmFeatures)
                for (unsigned iBit = 0; iBit < 32; iBit++)
                    if (RT_BIT_32(iBit) & fSvmFeatures)
                        LogRel(("HM:    Reserved bit %u\n", iBit));

            /* Only try once. */
            pVM->hm.s.fInitialized = true;

            if (pVM->hm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                pVM->hm.s.fNestedPaging = pVM->hm.s.fAllowNestedPaging;

            rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HM_SETUP_VM, 0, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                pVM->fHMEnabled = true;
                pVM->hm.s.svm.fEnabled = true;

                if (pVM->hm.s.fNestedPaging)
                {
                    LogRel(("HM:    Enabled nested paging\n"));
#if HC_ARCH_BITS == 64
                    if (pVM->hm.s.fLargePages)
                    {
                        /* Use large (2 MB) pages for our nested paging PDEs where possible. */
                        PGMSetLargePageUsage(pVM, true);
                        LogRel(("HM:    Large page support enabled!\n"));
                    }
#endif
                }

                hmR3DisableRawMode(pVM);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                if (pVM->hm.s.fAllow64BitGuests)
                {
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LONG_MODE);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_LAHF);
                }
                else
                /* Turn on NXE if PAE has been enabled. */
                if (CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE))
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
#endif

                LogRel((pVM->hm.s.fAllow64BitGuests
                        ? "HM:    32-bit and 64-bit guest supported.\n"
                        : "HM:    32-bit guest supported.\n"));

                LogRel(("HM:    TPR Patching %s.\n", (pVM->hm.s.fTRPPatchingAllowed) ? "enabled" : "disabled"));
            }
            else
            {
                pVM->fHMEnabled = false;
            }
        }
    }
    if (pVM->fHMEnabled)
        LogRel(("HM:    VT-x/AMD-V init method: %s\n", (pVM->hm.s.fGlobalInit) ? "GLOBAL" : "LOCAL"));
    RTLogRelSetBuffering(fOldBuffered);
    return VINF_SUCCESS;
}


/**
 * Applies relocations to data and code managed by this
 * component. This function will be called at init and
 * whenever the VMM need to relocate it self inside the GC.
 *
 * @param   pVM     The VM.
 */
VMMR3DECL(void) HMR3Relocate(PVM pVM)
{
    Log(("HMR3Relocate to %RGv\n", MMHyperGetArea(pVM, 0)));

    /* Fetch the current paging mode during the relocate callback during state loading. */
    if (VMR3GetState(pVM) == VMSTATE_LOADING)
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            pVCpu->hm.s.enmShadowMode            = PGMGetShadowMode(pVCpu);
            Assert(pVCpu->hm.s.vmx.enmCurrGuestMode == PGMGetGuestMode(pVCpu));
            pVCpu->hm.s.vmx.enmCurrGuestMode     = PGMGetGuestMode(pVCpu);
        }
    }
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (pVM->fHMEnabled)
    {
        int rc;
        switch (PGMGetHostMode(pVM))
        {
            case PGMMODE_32_BIT:
                pVM->hm.s.pfnHost32ToGuest64R0 = VMMR3GetHostToGuestSwitcher(pVM, VMMSWITCHER_32_TO_AMD64);
                break;

            case PGMMODE_PAE:
            case PGMMODE_PAE_NX:
                pVM->hm.s.pfnHost32ToGuest64R0 = VMMR3GetHostToGuestSwitcher(pVM, VMMSWITCHER_PAE_TO_AMD64);
                break;

            default:
                AssertFailed();
                break;
        }
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "VMXGCStartVM64", &pVM->hm.s.pfnVMXGCStartVM64);
        AssertReleaseMsgRC(rc, ("VMXGCStartVM64 -> rc=%Rrc\n", rc));

        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "SVMGCVMRun64",   &pVM->hm.s.pfnSVMGCVMRun64);
        AssertReleaseMsgRC(rc, ("SVMGCVMRun64 -> rc=%Rrc\n", rc));

        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "HMSaveGuestFPU64",   &pVM->hm.s.pfnSaveGuestFPU64);
        AssertReleaseMsgRC(rc, ("HMSetupFPU64 -> rc=%Rrc\n", rc));

        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "HMSaveGuestDebug64",   &pVM->hm.s.pfnSaveGuestDebug64);
        AssertReleaseMsgRC(rc, ("HMSetupDebug64 -> rc=%Rrc\n", rc));

# ifdef DEBUG
        rc = PDMR3LdrGetSymbolRC(pVM, NULL,       "HMTestSwitcher64",   &pVM->hm.s.pfnTest64);
        AssertReleaseMsgRC(rc, ("HMTestSwitcher64 -> rc=%Rrc\n", rc));
# endif
    }
#endif
    return;
}


/**
 * Checks if hardware accelerated raw mode is allowed.
 *
 * @returns true if hardware acceleration is allowed, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HMR3IsAllowed(PVM pVM)
{
    return pVM->hm.s.fAllowed;
}


/**
 * Notification callback which is called whenever there is a chance that a CR3
 * value might have changed.
 *
 * This is called by PGM.
 *
 * @param   pVM            Pointer to the VM.
 * @param   pVCpu          Pointer to the VMCPU.
 * @param   enmShadowMode  New shadow paging mode.
 * @param   enmGuestMode   New guest paging mode.
 */
VMMR3DECL(void) HMR3PagingModeChanged(PVM pVM, PVMCPU pVCpu, PGMMODE enmShadowMode, PGMMODE enmGuestMode)
{
    /* Ignore page mode changes during state loading. */
    if (VMR3GetState(pVCpu->pVMR3) == VMSTATE_LOADING)
        return;

    pVCpu->hm.s.enmShadowMode = enmShadowMode;

    if (   pVM->hm.s.vmx.fEnabled
        && pVM->fHMEnabled)
    {
        if (    pVCpu->hm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
            &&  enmGuestMode >= PGMMODE_PROTECTED)
        {
            PCPUMCTX pCtx;

            pCtx = CPUMQueryGuestCtxPtr(pVCpu);

            /* After a real mode switch to protected mode we must force
               CPL to 0. Our real mode emulation had to set it to 3. */
            pCtx->ss.Attr.n.u2Dpl  = 0;
        }
    }

    if (pVCpu->hm.s.vmx.enmCurrGuestMode != enmGuestMode)
    {
        /* Keep track of paging mode changes. */
        pVCpu->hm.s.vmx.enmPrevGuestMode = pVCpu->hm.s.vmx.enmCurrGuestMode;
        pVCpu->hm.s.vmx.enmCurrGuestMode = enmGuestMode;

        /* Did we miss a change, because all code was executed in the recompiler? */
        if (pVCpu->hm.s.vmx.enmLastSeenGuestMode == enmGuestMode)
        {
            Log(("HMR3PagingModeChanged missed %s->%s transition (prev %s)\n", PGMGetModeName(pVCpu->hm.s.vmx.enmPrevGuestMode),
                 PGMGetModeName(pVCpu->hm.s.vmx.enmCurrGuestMode), PGMGetModeName(pVCpu->hm.s.vmx.enmLastSeenGuestMode)));
            pVCpu->hm.s.vmx.enmLastSeenGuestMode = pVCpu->hm.s.vmx.enmPrevGuestMode;
        }
    }

    /* Reset the contents of the read cache. */
    PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;
    for (unsigned j = 0; j < pCache->Read.cValidEntries; j++)
        pCache->Read.aFieldVal[j] = 0;
}


/**
 * Terminates the HM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM itself is, at this point, powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int) HMR3Term(PVM pVM)
{
    if (pVM->hm.s.vmx.pRealModeTSS)
    {
        PDMR3VmmDevHeapFree(pVM, pVM->hm.s.vmx.pRealModeTSS);
        pVM->hm.s.vmx.pRealModeTSS       = 0;
    }
    hmR3TermCPU(pVM);
    return 0;
}


/**
 * Terminates the per-VCPU HM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
static int hmR3TermCPU(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i]; NOREF(pVCpu);

#ifdef VBOX_WITH_STATISTICS
        if (pVCpu->hm.s.paStatExitReason)
        {
            MMHyperFree(pVM, pVCpu->hm.s.paStatExitReason);
            pVCpu->hm.s.paStatExitReason   = NULL;
            pVCpu->hm.s.paStatExitReasonR0 = NIL_RTR0PTR;
        }
        if (pVCpu->hm.s.paStatInjectedIrqs)
        {
            MMHyperFree(pVM, pVCpu->hm.s.paStatInjectedIrqs);
            pVCpu->hm.s.paStatInjectedIrqs   = NULL;
            pVCpu->hm.s.paStatInjectedIrqsR0 = NIL_RTR0PTR;
        }
#endif

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        memset(pVCpu->hm.s.vmx.VMCSCache.aMagic, 0, sizeof(pVCpu->hm.s.vmx.VMCSCache.aMagic));
        pVCpu->hm.s.vmx.VMCSCache.uMagic = 0;
        pVCpu->hm.s.vmx.VMCSCache.uPos = 0xffffffff;
#endif
    }
    return 0;
}


/**
 * Resets a virtual CPU.
 *
 * Used by HMR3Reset and CPU hot plugging.
 *
 * @param   pVCpu   The CPU to reset.
 */
VMMR3DECL(void) HMR3ResetCpu(PVMCPU pVCpu)
{
    /* On first entry we'll sync everything. */
    pVCpu->hm.s.fContextUseFlags = HM_CHANGED_ALL;

    pVCpu->hm.s.vmx.cr0_mask = 0;
    pVCpu->hm.s.vmx.cr4_mask = 0;

    pVCpu->hm.s.fActive        = false;
    pVCpu->hm.s.Event.fPending = false;

    /* Reset state information for real-mode emulation in VT-x. */
    pVCpu->hm.s.vmx.enmLastSeenGuestMode = PGMMODE_REAL;
    pVCpu->hm.s.vmx.enmPrevGuestMode     = PGMMODE_REAL;
    pVCpu->hm.s.vmx.enmCurrGuestMode     = PGMMODE_REAL;

    /* Reset the contents of the read cache. */
    PVMCSCACHE pCache = &pVCpu->hm.s.vmx.VMCSCache;
    for (unsigned j = 0; j < pCache->Read.cValidEntries; j++)
        pCache->Read.aFieldVal[j] = 0;

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /* Magic marker for searching in crash dumps. */
    strcpy((char *)pCache->aMagic, "VMCSCACHE Magic");
    pCache->uMagic = UINT64_C(0xDEADBEEFDEADBEEF);
#endif
}


/**
 * The VM is being reset.
 *
 * For the HM component this means that any GDT/LDT/TSS monitors
 * needs to be removed.
 *
 * @param   pVM     Pointer to the VM.
 */
VMMR3DECL(void) HMR3Reset(PVM pVM)
{
    LogFlow(("HMR3Reset:\n"));

    if (pVM->fHMEnabled)
        hmR3DisableRawMode(pVM);

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        HMR3ResetCpu(pVCpu);
    }

    /* Clear all patch information. */
    pVM->hm.s.pGuestPatchMem         = 0;
    pVM->hm.s.pFreeGuestPatchMem     = 0;
    pVM->hm.s.cbGuestPatchMem        = 0;
    pVM->hm.s.cPatches           = 0;
    pVM->hm.s.PatchTree          = 0;
    pVM->hm.s.fTPRPatchingActive = false;
    ASMMemZero32(pVM->hm.s.aPatches, sizeof(pVM->hm.s.aPatches));
}


/**
 * Callback to patch a TPR instruction (vmmcall or mov cr8).
 *
 * @returns VBox strict status code.
 * @param   pVM     Pointer to the VM.
 * @param   pVCpu   The VMCPU for the EMT we're being called on.
 * @param   pvUser  Unused.
 */
DECLCALLBACK(VBOXSTRICTRC) hmR3RemovePatches(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    VMCPUID idCpu = (VMCPUID)(uintptr_t)pvUser;

    /* Only execute the handler on the VCPU the original patch request was issued. */
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    Log(("hmR3RemovePatches\n"));
    for (unsigned i = 0; i < pVM->hm.s.cPatches; i++)
    {
        uint8_t         abInstr[15];
        PHMTPRPATCH pPatch = &pVM->hm.s.aPatches[i];
        RTGCPTR         pInstrGC = (RTGCPTR)pPatch->Core.Key;
        int             rc;

#ifdef LOG_ENABLED
        char            szOutput[256];

        rc = DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, CPUMGetGuestCS(pVCpu), pInstrGC, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                szOutput, sizeof(szOutput), NULL);
        if (RT_SUCCESS(rc))
            Log(("Patched instr: %s\n", szOutput));
#endif

        /* Check if the instruction is still the same. */
        rc = PGMPhysSimpleReadGCPtr(pVCpu, abInstr, pInstrGC, pPatch->cbNewOp);
        if (rc != VINF_SUCCESS)
        {
            Log(("Patched code removed? (rc=%Rrc0\n", rc));
            continue;   /* swapped out or otherwise removed; skip it. */
        }

        if (memcmp(abInstr, pPatch->aNewOpcode, pPatch->cbNewOp))
        {
            Log(("Patched instruction was changed! (rc=%Rrc0\n", rc));
            continue;   /* skip it. */
        }

        rc = PGMPhysSimpleWriteGCPtr(pVCpu, pInstrGC, pPatch->aOpcode, pPatch->cbOp);
        AssertRC(rc);

#ifdef LOG_ENABLED
        rc = DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, CPUMGetGuestCS(pVCpu), pInstrGC, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                szOutput, sizeof(szOutput), NULL);
        if (RT_SUCCESS(rc))
            Log(("Original instr: %s\n", szOutput));
#endif
    }
    pVM->hm.s.cPatches           = 0;
    pVM->hm.s.PatchTree          = 0;
    pVM->hm.s.pFreeGuestPatchMem = pVM->hm.s.pGuestPatchMem;
    pVM->hm.s.fTPRPatchingActive = false;
    return VINF_SUCCESS;
}


/**
 * Worker for enabling patching in a VT-x/AMD-V guest.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   idCpu       VCPU to execute hmR3RemovePatches on.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
static int hmR3EnablePatching(PVM pVM, VMCPUID idCpu, RTRCPTR pPatchMem, unsigned cbPatchMem)
{
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, hmR3RemovePatches, (void *)(uintptr_t)idCpu);
    AssertRC(rc);

    pVM->hm.s.pGuestPatchMem      = pPatchMem;
    pVM->hm.s.pFreeGuestPatchMem  = pPatchMem;
    pVM->hm.s.cbGuestPatchMem     = cbPatchMem;
    return VINF_SUCCESS;
}


/**
 * Enable patching in a VT-x/AMD-V guest
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
VMMR3DECL(int)  HMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem)
{
    VM_ASSERT_EMT(pVM);
    Log(("HMR3EnablePatching %RGv size %x\n", pPatchMem, cbPatchMem));
    if (pVM->cCpus > 1)
    {
        /* We own the IOM lock here and could cause a deadlock by waiting for a VCPU that is blocking on the IOM lock. */
        int rc = VMR3ReqCallNoWait(pVM, VMCPUID_ANY_QUEUE,
                                   (PFNRT)hmR3EnablePatching, 4, pVM, VMMGetCpuId(pVM), (RTRCPTR)pPatchMem, cbPatchMem);
        AssertRC(rc);
        return rc;
    }
    return hmR3EnablePatching(pVM, VMMGetCpuId(pVM), (RTRCPTR)pPatchMem, cbPatchMem);
}


/**
 * Disable patching in a VT-x/AMD-V guest.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
VMMR3DECL(int)  HMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem)
{
    Log(("HMR3DisablePatching %RGv size %x\n", pPatchMem, cbPatchMem));

    Assert(pVM->hm.s.pGuestPatchMem == pPatchMem);
    Assert(pVM->hm.s.cbGuestPatchMem == cbPatchMem);

    /* @todo Potential deadlock when other VCPUs are waiting on the IOM lock (we own it)!! */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, hmR3RemovePatches,
                                (void *)(uintptr_t)VMMGetCpuId(pVM));
    AssertRC(rc);

    pVM->hm.s.pGuestPatchMem      = 0;
    pVM->hm.s.pFreeGuestPatchMem  = 0;
    pVM->hm.s.cbGuestPatchMem     = 0;
    pVM->hm.s.fTPRPatchingActive = false;
    return VINF_SUCCESS;
}


/**
 * Callback to patch a TPR instruction (vmmcall or mov cr8).
 *
 * @returns VBox strict status code.
 * @param   pVM     Pointer to the VM.
 * @param   pVCpu   The VMCPU for the EMT we're being called on.
 * @param   pvUser  User specified CPU context.
 *
 */
DECLCALLBACK(VBOXSTRICTRC) hmR3ReplaceTprInstr(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    /*
     * Only execute the handler on the VCPU the original patch request was
     * issued. (The other CPU(s) might not yet have switched to protected
     * mode, nor have the correct memory context.)
     */
    VMCPUID         idCpu  = (VMCPUID)(uintptr_t)pvUser;
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    /*
     * We're racing other VCPUs here, so don't try patch the instruction twice
     * and make sure there is still room for our patch record.
     */
    PCPUMCTX        pCtx   = CPUMQueryGuestCtxPtr(pVCpu);
    PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
    if (pPatch)
    {
        Log(("hmR3ReplaceTprInstr: already patched %RGv\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    uint32_t const  idx = pVM->hm.s.cPatches;
    if (idx >= RT_ELEMENTS(pVM->hm.s.aPatches))
    {
        Log(("hmR3ReplaceTprInstr: no available patch slots (%RGv)\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    pPatch = &pVM->hm.s.aPatches[idx];

    Log(("hmR3ReplaceTprInstr: rip=%RGv idxPatch=%u\n", pCtx->rip, idx));

    /*
     * Disassembler the instruction and get cracking.
     */
    DBGFR3DisasInstrCurrentLog(pVCpu, "hmR3ReplaceTprInstr");
    PDISCPUSTATE    pDis = &pVCpu->hm.s.DisState;
    uint32_t        cbOp;
    int rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
    AssertRC(rc);
    if (    rc == VINF_SUCCESS
        &&  pDis->pCurInstr->uOpcode == OP_MOV
        &&  cbOp >= 3)
    {
        static uint8_t const s_abVMMCall[3] = { 0x0f, 0x01, 0xd9 };

        rc = PGMPhysSimpleReadGCPtr(pVCpu, pPatch->aOpcode, pCtx->rip, cbOp);
        AssertRC(rc);

        pPatch->cbOp = cbOp;

        if (pDis->Param1.fUse == DISUSE_DISPLACEMENT32)
        {
            /* write. */
            if (pDis->Param2.fUse == DISUSE_REG_GEN32)
            {
                pPatch->enmType     = HMTPRINSTR_WRITE_REG;
                pPatch->uSrcOperand = pDis->Param2.Base.idxGenReg;
                Log(("hmR3ReplaceTprInstr: HMTPRINSTR_WRITE_REG %u\n", pDis->Param2.Base.idxGenReg));
            }
            else
            {
                Assert(pDis->Param2.fUse == DISUSE_IMMEDIATE32);
                pPatch->enmType     = HMTPRINSTR_WRITE_IMM;
                pPatch->uSrcOperand = pDis->Param2.uValue;
                Log(("hmR3ReplaceTprInstr: HMTPRINSTR_WRITE_IMM %#llx\n", pDis->Param2.uValue));
            }
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, s_abVMMCall, sizeof(s_abVMMCall));
            AssertRC(rc);

            memcpy(pPatch->aNewOpcode, s_abVMMCall, sizeof(s_abVMMCall));
            pPatch->cbNewOp = sizeof(s_abVMMCall);
        }
        else
        {
            /*
             * TPR Read.
             *
             * Found:
             *   mov eax, dword [fffe0080]        (5 bytes)
             * Check if next instruction is:
             *   shr eax, 4
             */
            Assert(pDis->Param1.fUse == DISUSE_REG_GEN32);

            uint8_t  const idxMmioReg = pDis->Param1.Base.idxGenReg;
            uint8_t  const cbOpMmio   = cbOp;
            uint64_t const uSavedRip  = pCtx->rip;

            pCtx->rip += cbOp;
            rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
            DBGFR3DisasInstrCurrentLog(pVCpu, "Following read");
            pCtx->rip = uSavedRip;

            if (    rc == VINF_SUCCESS
                &&  pDis->pCurInstr->uOpcode == OP_SHR
                &&  pDis->Param1.fUse == DISUSE_REG_GEN32
                &&  pDis->Param1.Base.idxGenReg == idxMmioReg
                &&  pDis->Param2.fUse == DISUSE_IMMEDIATE8
                &&  pDis->Param2.uValue == 4
                &&  cbOpMmio + cbOp < sizeof(pVM->hm.s.aPatches[idx].aOpcode))
            {
                uint8_t abInstr[15];

                /* Replacing two instructions now. */
                rc = PGMPhysSimpleReadGCPtr(pVCpu, &pPatch->aOpcode, pCtx->rip, cbOpMmio + cbOp);
                AssertRC(rc);

                pPatch->cbOp = cbOpMmio + cbOp;

                /* 0xF0, 0x0F, 0x20, 0xC0 = mov eax, cr8 */
                abInstr[0] = 0xF0;
                abInstr[1] = 0x0F;
                abInstr[2] = 0x20;
                abInstr[3] = 0xC0 | pDis->Param1.Base.idxGenReg;
                for (unsigned i = 4; i < pPatch->cbOp; i++)
                    abInstr[i] = 0x90;  /* nop */

                rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, abInstr, pPatch->cbOp);
                AssertRC(rc);

                memcpy(pPatch->aNewOpcode, abInstr, pPatch->cbOp);
                pPatch->cbNewOp = pPatch->cbOp;

                Log(("Acceptable read/shr candidate!\n"));
                pPatch->enmType = HMTPRINSTR_READ_SHR4;
            }
            else
            {
                pPatch->enmType     = HMTPRINSTR_READ;
                pPatch->uDstOperand = idxMmioReg;

                rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, s_abVMMCall, sizeof(s_abVMMCall));
                AssertRC(rc);

                memcpy(pPatch->aNewOpcode, s_abVMMCall, sizeof(s_abVMMCall));
                pPatch->cbNewOp = sizeof(s_abVMMCall);
                Log(("hmR3ReplaceTprInstr: HMTPRINSTR_READ %u\n", pPatch->uDstOperand));
            }
        }

        pPatch->Core.Key = pCtx->eip;
        rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
        AssertRC(rc);

        pVM->hm.s.cPatches++;
        STAM_COUNTER_INC(&pVM->hm.s.StatTprReplaceSuccess);
        return VINF_SUCCESS;
    }

    /*
     * Save invalid patch, so we will not try again.
     */
    Log(("hmR3ReplaceTprInstr: Failed to patch instr!\n"));
    pPatch->Core.Key = pCtx->eip;
    pPatch->enmType  = HMTPRINSTR_INVALID;
    rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
    AssertRC(rc);
    pVM->hm.s.cPatches++;
    STAM_COUNTER_INC(&pVM->hm.s.StatTprReplaceFailure);
    return VINF_SUCCESS;
}


/**
 * Callback to patch a TPR instruction (jump to generated code).
 *
 * @returns VBox strict status code.
 * @param   pVM     Pointer to the VM.
 * @param   pVCpu   The VMCPU for the EMT we're being called on.
 * @param   pvUser  User specified CPU context.
 *
 */
DECLCALLBACK(VBOXSTRICTRC) hmR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    /*
     * Only execute the handler on the VCPU the original patch request was
     * issued. (The other CPU(s) might not yet have switched to protected
     * mode, nor have the correct memory context.)
     */
    VMCPUID         idCpu  = (VMCPUID)(uintptr_t)pvUser;
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    /*
     * We're racing other VCPUs here, so don't try patch the instruction twice
     * and make sure there is still room for our patch record.
     */
    PCPUMCTX        pCtx   = CPUMQueryGuestCtxPtr(pVCpu);
    PHMTPRPATCH pPatch = (PHMTPRPATCH)RTAvloU32Get(&pVM->hm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
    if (pPatch)
    {
        Log(("hmR3PatchTprInstr: already patched %RGv\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    uint32_t const  idx = pVM->hm.s.cPatches;
    if (idx >= RT_ELEMENTS(pVM->hm.s.aPatches))
    {
        Log(("hmR3PatchTprInstr: no available patch slots (%RGv)\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    pPatch = &pVM->hm.s.aPatches[idx];

    Log(("hmR3PatchTprInstr: rip=%RGv idxPatch=%u\n", pCtx->rip, idx));
    DBGFR3DisasInstrCurrentLog(pVCpu, "hmR3PatchTprInstr");

    /*
     * Disassemble the instruction and get cracking.
     */
    PDISCPUSTATE    pDis   = &pVCpu->hm.s.DisState;
    uint32_t        cbOp;
    int rc = EMInterpretDisasCurrent(pVM, pVCpu, pDis, &cbOp);
    AssertRC(rc);
    if (    rc == VINF_SUCCESS
        &&  pDis->pCurInstr->uOpcode == OP_MOV
        &&  cbOp >= 5)
    {
        uint8_t         aPatch[64];
        uint32_t        off = 0;

        rc = PGMPhysSimpleReadGCPtr(pVCpu, pPatch->aOpcode, pCtx->rip, cbOp);
        AssertRC(rc);

        pPatch->cbOp    = cbOp;
        pPatch->enmType = HMTPRINSTR_JUMP_REPLACEMENT;

        if (pDis->Param1.fUse == DISUSE_DISPLACEMENT32)
        {
            /*
                * TPR write:
                *
                * push ECX                      [51]
                * push EDX                      [52]
                * push EAX                      [50]
                * xor EDX,EDX                   [31 D2]
                * mov EAX,EAX                   [89 C0]
                *  or
                * mov EAX,0000000CCh            [B8 CC 00 00 00]
                * mov ECX,0C0000082h            [B9 82 00 00 C0]
                * wrmsr                         [0F 30]
                * pop EAX                       [58]
                * pop EDX                       [5A]
                * pop ECX                       [59]
                * jmp return_address            [E9 return_address]
                *
                */
            bool fUsesEax = (pDis->Param2.fUse == DISUSE_REG_GEN32 && pDis->Param2.Base.idxGenReg == DISGREG_EAX);

            aPatch[off++] = 0x51;    /* push ecx */
            aPatch[off++] = 0x52;    /* push edx */
            if (!fUsesEax)
                aPatch[off++] = 0x50;    /* push eax */
            aPatch[off++] = 0x31;    /* xor edx, edx */
            aPatch[off++] = 0xD2;
            if (pDis->Param2.fUse == DISUSE_REG_GEN32)
            {
                if (!fUsesEax)
                {
                    aPatch[off++] = 0x89;    /* mov eax, src_reg */
                    aPatch[off++] = MAKE_MODRM(3, pDis->Param2.Base.idxGenReg, DISGREG_EAX);
                }
            }
            else
            {
                Assert(pDis->Param2.fUse == DISUSE_IMMEDIATE32);
                aPatch[off++] = 0xB8;    /* mov eax, immediate */
                *(uint32_t *)&aPatch[off] = pDis->Param2.uValue;
                off += sizeof(uint32_t);
            }
            aPatch[off++] = 0xB9;    /* mov ecx, 0xc0000082 */
            *(uint32_t *)&aPatch[off] = MSR_K8_LSTAR;
            off += sizeof(uint32_t);

            aPatch[off++] = 0x0F;    /* wrmsr */
            aPatch[off++] = 0x30;
            if (!fUsesEax)
                aPatch[off++] = 0x58;    /* pop eax */
            aPatch[off++] = 0x5A;    /* pop edx */
            aPatch[off++] = 0x59;    /* pop ecx */
        }
        else
        {
            /*
                * TPR read:
                *
                * push ECX                      [51]
                * push EDX                      [52]
                * push EAX                      [50]
                * mov ECX,0C0000082h            [B9 82 00 00 C0]
                * rdmsr                         [0F 32]
                * mov EAX,EAX                   [89 C0]
                * pop EAX                       [58]
                * pop EDX                       [5A]
                * pop ECX                       [59]
                * jmp return_address            [E9 return_address]
                *
                */
            Assert(pDis->Param1.fUse == DISUSE_REG_GEN32);

            if (pDis->Param1.Base.idxGenReg != DISGREG_ECX)
                aPatch[off++] = 0x51;    /* push ecx */
            if (pDis->Param1.Base.idxGenReg != DISGREG_EDX )
                aPatch[off++] = 0x52;    /* push edx */
            if (pDis->Param1.Base.idxGenReg != DISGREG_EAX)
                aPatch[off++] = 0x50;    /* push eax */

            aPatch[off++] = 0x31;    /* xor edx, edx */
            aPatch[off++] = 0xD2;

            aPatch[off++] = 0xB9;    /* mov ecx, 0xc0000082 */
            *(uint32_t *)&aPatch[off] = MSR_K8_LSTAR;
            off += sizeof(uint32_t);

            aPatch[off++] = 0x0F;    /* rdmsr */
            aPatch[off++] = 0x32;

            if (pDis->Param1.Base.idxGenReg != DISGREG_EAX)
            {
                aPatch[off++] = 0x89;    /* mov dst_reg, eax */
                aPatch[off++] = MAKE_MODRM(3, DISGREG_EAX, pDis->Param1.Base.idxGenReg);
            }

            if (pDis->Param1.Base.idxGenReg != DISGREG_EAX)
                aPatch[off++] = 0x58;    /* pop eax */
            if (pDis->Param1.Base.idxGenReg != DISGREG_EDX )
                aPatch[off++] = 0x5A;    /* pop edx */
            if (pDis->Param1.Base.idxGenReg != DISGREG_ECX)
                aPatch[off++] = 0x59;    /* pop ecx */
        }
        aPatch[off++] = 0xE9;    /* jmp return_address */
        *(RTRCUINTPTR *)&aPatch[off] = ((RTRCUINTPTR)pCtx->eip + cbOp) - ((RTRCUINTPTR)pVM->hm.s.pFreeGuestPatchMem + off + 4);
        off += sizeof(RTRCUINTPTR);

        if (pVM->hm.s.pFreeGuestPatchMem + off <= pVM->hm.s.pGuestPatchMem + pVM->hm.s.cbGuestPatchMem)
        {
            /* Write new code to the patch buffer. */
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pVM->hm.s.pFreeGuestPatchMem, aPatch, off);
            AssertRC(rc);

#ifdef LOG_ENABLED
            uint32_t cbCurInstr;
            for (RTGCPTR GCPtrInstr = pVM->hm.s.pFreeGuestPatchMem;
                 GCPtrInstr < pVM->hm.s.pFreeGuestPatchMem + off;
                 GCPtrInstr += RT_MAX(cbCurInstr, 1))
            {
                char     szOutput[256];
                rc = DBGFR3DisasInstrEx(pVM, pVCpu->idCpu, pCtx->cs.Sel, GCPtrInstr, DBGF_DISAS_FLAGS_DEFAULT_MODE,
                                        szOutput, sizeof(szOutput), &cbCurInstr);
                if (RT_SUCCESS(rc))
                    Log(("Patch instr %s\n", szOutput));
                else
                    Log(("%RGv: rc=%Rrc\n", GCPtrInstr, rc));
            }
#endif

            pPatch->aNewOpcode[0] = 0xE9;
            *(RTRCUINTPTR *)&pPatch->aNewOpcode[1] = ((RTRCUINTPTR)pVM->hm.s.pFreeGuestPatchMem) - ((RTRCUINTPTR)pCtx->eip + 5);

            /* Overwrite the TPR instruction with a jump. */
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->eip, pPatch->aNewOpcode, 5);
            AssertRC(rc);

            DBGFR3DisasInstrCurrentLog(pVCpu, "Jump");

            pVM->hm.s.pFreeGuestPatchMem += off;
            pPatch->cbNewOp = 5;

            pPatch->Core.Key = pCtx->eip;
            rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
            AssertRC(rc);

            pVM->hm.s.cPatches++;
            pVM->hm.s.fTPRPatchingActive = true;
            STAM_COUNTER_INC(&pVM->hm.s.StatTprPatchSuccess);
            return VINF_SUCCESS;
        }

        Log(("Ran out of space in our patch buffer!\n"));
    }
    else
        Log(("hmR3PatchTprInstr: Failed to patch instr!\n"));


    /*
     * Save invalid patch, so we will not try again.
     */
    pPatch = &pVM->hm.s.aPatches[idx];
    pPatch->Core.Key = pCtx->eip;
    pPatch->enmType  = HMTPRINSTR_INVALID;
    rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
    AssertRC(rc);
    pVM->hm.s.cPatches++;
    STAM_COUNTER_INC(&pVM->hm.s.StatTprPatchFailure);
    return VINF_SUCCESS;
}


/**
 * Attempt to patch TPR mmio instructions.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR3DECL(int) HMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pCtx);
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE,
                                pVM->hm.s.pGuestPatchMem ? hmR3PatchTprInstr : hmR3ReplaceTprInstr,
                                (void *)(uintptr_t)pVCpu->idCpu);
    AssertRC(rc);
    return rc;
}


/**
 * Force execution of the current IO code in the recompiler.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pCtx        Partial VM execution context.
 */
VMMR3DECL(int) HMR3EmulateIoBlock(PVM pVM, PCPUMCTX pCtx)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    Assert(pVM->fHMEnabled);
    Log(("HMR3EmulateIoBlock\n"));

    /* This is primarily intended to speed up Grub, so we don't care about paged protected mode. */
    if (HMCanEmulateIoBlockEx(pCtx))
    {
        Log(("HMR3EmulateIoBlock -> enabled\n"));
        pVCpu->hm.s.EmulateIoBlock.fEnabled         = true;
        pVCpu->hm.s.EmulateIoBlock.GCPtrFunctionEip = pCtx->rip;
        pVCpu->hm.s.EmulateIoBlock.cr0              = pCtx->cr0;
        return VINF_EM_RESCHEDULE_REM;
    }
    return VINF_SUCCESS;
}


/**
 * Checks if we can currently use hardware accelerated raw mode.
 *
 * @returns true if we can currently use hardware acceleration, otherwise false.
 * @param   pVM         Pointer to the VM.
 * @param   pCtx        Partial VM execution context.
 */
VMMR3DECL(bool) HMR3CanExecuteGuest(PVM pVM, PCPUMCTX pCtx)
{
    PVMCPU pVCpu = VMMGetCpu(pVM);

    Assert(pVM->fHMEnabled);

    /* If we're still executing the IO code, then return false. */
    if (    RT_UNLIKELY(pVCpu->hm.s.EmulateIoBlock.fEnabled)
        &&  pCtx->rip <  pVCpu->hm.s.EmulateIoBlock.GCPtrFunctionEip + 0x200
        &&  pCtx->rip >  pVCpu->hm.s.EmulateIoBlock.GCPtrFunctionEip - 0x200
        &&  pCtx->cr0 == pVCpu->hm.s.EmulateIoBlock.cr0)
        return false;

    pVCpu->hm.s.EmulateIoBlock.fEnabled = false;

    /* AMD-V supports real & protected mode with or without paging. */
    if (pVM->hm.s.svm.fEnabled)
    {
        pVCpu->hm.s.fActive = true;
        return true;
    }

    pVCpu->hm.s.fActive = false;

    /* Note! The context supplied by REM is partial. If we add more checks here, be sure to verify that REM provides this info! */
    Assert(   (pVM->hm.s.vmx.fUnrestrictedGuest && !pVM->hm.s.vmx.pRealModeTSS)
           || (!pVM->hm.s.vmx.fUnrestrictedGuest && pVM->hm.s.vmx.pRealModeTSS));

    bool fSupportsRealMode = pVM->hm.s.vmx.fUnrestrictedGuest || PDMVmmDevHeapIsEnabled(pVM);
    if (!pVM->hm.s.vmx.fUnrestrictedGuest)
    {
        /*
         * The VMM device heap is a requirement for emulating real mode or protected mode without paging with the unrestricted
         * guest execution feature i missing (VT-x only).
         */
        if (fSupportsRealMode)
        {
            if (CPUMIsGuestInRealModeEx(pCtx))
            {
                /* In V86 mode (VT-x or not), the CPU enforces real-mode compatible selector
                 * bases and limits, i.e. limit must be 64K and base must be selector * 16.
                 * If this is not true, we cannot execute real mode as V86 and have to fall
                 * back to emulation.
                 */
                if (   pCtx->cs.Sel != (pCtx->cs.u64Base >> 4)
                    || pCtx->ds.Sel != (pCtx->ds.u64Base >> 4)
                    || pCtx->es.Sel != (pCtx->es.u64Base >> 4)
                    || pCtx->ss.Sel != (pCtx->ss.u64Base >> 4)
                    || pCtx->fs.Sel != (pCtx->fs.u64Base >> 4)
                    || pCtx->gs.Sel != (pCtx->gs.u64Base >> 4)
                    || (pCtx->cs.u32Limit != 0xffff)
                    || (pCtx->ds.u32Limit != 0xffff)
                    || (pCtx->es.u32Limit != 0xffff)
                    || (pCtx->ss.u32Limit != 0xffff)
                    || (pCtx->fs.u32Limit != 0xffff)
                    || (pCtx->gs.u32Limit != 0xffff))
                {
                    return false;
                }
            }
            else
            {
                PGMMODE enmGuestMode = PGMGetGuestMode(pVCpu);
                /* Verify the requirements for executing code in protected
                   mode. VT-x can't handle the CPU state right after a switch
                   from real to protected mode. (all sorts of RPL & DPL assumptions) */
                if (    pVCpu->hm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
                    &&  enmGuestMode >= PGMMODE_PROTECTED)
                {
                    if (   (pCtx->cs.Sel & X86_SEL_RPL)
                        || (pCtx->ds.Sel & X86_SEL_RPL)
                        || (pCtx->es.Sel & X86_SEL_RPL)
                        || (pCtx->fs.Sel & X86_SEL_RPL)
                        || (pCtx->gs.Sel & X86_SEL_RPL)
                        || (pCtx->ss.Sel & X86_SEL_RPL))
                    {
                        return false;
                    }
                }
                /* VT-x also chokes on invalid tr or ldtr selectors (minix) */
                if (    pCtx->gdtr.cbGdt
                    &&  (   pCtx->tr.Sel > pCtx->gdtr.cbGdt
                         || pCtx->ldtr.Sel > pCtx->gdtr.cbGdt))
                {
                        return false;
                }
            }
        }
        else
        {
            if (    !CPUMIsGuestInLongModeEx(pCtx)
                &&  !pVM->hm.s.vmx.fUnrestrictedGuest)
            {
                /** @todo   This should (probably) be set on every excursion to the REM,
                 *          however it's too risky right now. So, only apply it when we go
                 *          back to REM for real mode execution. (The XP hack below doesn't
                 *          work reliably without this.)
                 *  Update: Implemented in EM.cpp, see #ifdef EM_NOTIFY_HM.  */
                for (uint32_t i = 0; i < pVM->cCpus; i++)
                    pVM->aCpus[i].hm.s.fContextUseFlags |= HM_CHANGED_ALL_GUEST;

                if (    !pVM->hm.s.fNestedPaging        /* requires a fake PD for real *and* protected mode without paging - stored in the VMM device heap */
                    ||  CPUMIsGuestInRealModeEx(pCtx))  /* requires a fake TSS for real mode - stored in the VMM device heap */
                    return false;

                /* Too early for VT-x; Solaris guests will fail with a guru meditation otherwise; same for XP. */
                if (pCtx->idtr.pIdt == 0 || pCtx->idtr.cbIdt == 0 || pCtx->tr.Sel == 0)
                    return false;

                /* The guest is about to complete the switch to protected mode. Wait a bit longer. */
                /* Windows XP; switch to protected mode; all selectors are marked not present in the
                 * hidden registers (possible recompiler bug; see load_seg_vm) */
                if (pCtx->cs.Attr.n.u1Present == 0)
                    return false;
                if (pCtx->ss.Attr.n.u1Present == 0)
                    return false;

                /* Windows XP: possible same as above, but new recompiler requires new heuristics?
                   VT-x doesn't seem to like something about the guest state and this stuff avoids it. */
                /** @todo This check is actually wrong, it doesn't take the direction of the
                 *        stack segment into account. But, it does the job for now. */
                if (pCtx->rsp >= pCtx->ss.u32Limit)
                    return false;
#if 0
                if (    pCtx->cs.Sel >= pCtx->gdtr.cbGdt
                    ||  pCtx->ss.Sel >= pCtx->gdtr.cbGdt
                    ||  pCtx->ds.Sel >= pCtx->gdtr.cbGdt
                    ||  pCtx->es.Sel >= pCtx->gdtr.cbGdt
                    ||  pCtx->fs.Sel >= pCtx->gdtr.cbGdt
                    ||  pCtx->gs.Sel >= pCtx->gdtr.cbGdt)
                    return false;
#endif
            }
        }
    }

    if (pVM->hm.s.vmx.fEnabled)
    {
        uint32_t mask;

        /* if bit N is set in cr0_fixed0, then it must be set in the guest's cr0. */
        mask = (uint32_t)pVM->hm.s.vmx.msr.vmx_cr0_fixed0;
        /* Note: We ignore the NE bit here on purpose; see vmmr0\hmr0.cpp for details. */
        mask &= ~X86_CR0_NE;

        if (fSupportsRealMode)
        {
            /* Note: We ignore the PE & PG bits here on purpose; we emulate real and protected mode without paging. */
            mask &= ~(X86_CR0_PG|X86_CR0_PE);
        }
        else
        {
            /* We support protected mode without paging using identity mapping. */
            mask &= ~X86_CR0_PG;
        }
        if ((pCtx->cr0 & mask) != mask)
            return false;

        /* if bit N is cleared in cr0_fixed1, then it must be zero in the guest's cr0. */
        mask = (uint32_t)~pVM->hm.s.vmx.msr.vmx_cr0_fixed1;
        if ((pCtx->cr0 & mask) != 0)
            return false;

        /* if bit N is set in cr4_fixed0, then it must be set in the guest's cr4. */
        mask  = (uint32_t)pVM->hm.s.vmx.msr.vmx_cr4_fixed0;
        mask &= ~X86_CR4_VMXE;
        if ((pCtx->cr4 & mask) != mask)
            return false;

        /* if bit N is cleared in cr4_fixed1, then it must be zero in the guest's cr4. */
        mask = (uint32_t)~pVM->hm.s.vmx.msr.vmx_cr4_fixed1;
        if ((pCtx->cr4 & mask) != 0)
            return false;

        pVCpu->hm.s.fActive = true;
        return true;
    }

    return false;
}


/**
 * Checks if we need to reschedule due to VMM device heap changes.
 *
 * @returns true if a reschedule is required, otherwise false.
 * @param   pVM         Pointer to the VM.
 * @param   pCtx        VM execution context.
 */
VMMR3DECL(bool) HMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx)
{
    /*
     * The VMM device heap is a requirement for emulating real mode or protected mode without paging
     * when the unrestricted guest execution feature is missing (VT-x only).
     */
    if (    pVM->hm.s.vmx.fEnabled
        &&  !pVM->hm.s.vmx.fUnrestrictedGuest
        &&  !CPUMIsGuestInPagedProtectedModeEx(pCtx)
        &&  !PDMVmmDevHeapIsEnabled(pVM)
        &&  (pVM->hm.s.fNestedPaging || CPUMIsGuestInRealModeEx(pCtx)))
        return true;

    return false;
}


/**
 * Notification from EM about a rescheduling into hardware assisted execution
 * mode.
 *
 * @param   pVCpu       Pointer to the current VMCPU.
 */
VMMR3DECL(void) HMR3NotifyScheduled(PVMCPU pVCpu)
{
    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_ALL_GUEST;
}


/**
 * Notification from EM about returning from instruction emulation (REM / EM).
 *
 * @param   pVCpu       Pointer to the VMCPU.
 */
VMMR3DECL(void) HMR3NotifyEmulated(PVMCPU pVCpu)
{
    pVCpu->hm.s.fContextUseFlags |= HM_CHANGED_ALL_GUEST;
}


/**
 * Checks if we are currently using hardware accelerated raw mode.
 *
 * @returns true if hardware acceleration is being used, otherwise false.
 * @param   pVCpu        Pointer to the VMCPU.
 */
VMMR3DECL(bool) HMR3IsActive(PVMCPU pVCpu)
{
    return pVCpu->hm.s.fActive;
}


/**
 * Checks if we are currently using nested paging.
 *
 * @returns true if nested paging is being used, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HMR3IsNestedPagingActive(PVM pVM)
{
    return pVM->hm.s.fNestedPaging;
}


/**
 * Checks if we are currently using VPID in VT-x mode.
 *
 * @returns true if VPID is being used, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HMR3IsVPIDActive(PVM pVM)
{
    return pVM->hm.s.vmx.fVpid;
}


/**
 * Checks if internal events are pending. In that case we are not allowed to dispatch interrupts.
 *
 * @returns true if an internal event is pending, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HMR3IsEventPending(PVMCPU pVCpu)
{
    return HMIsEnabled(pVCpu->pVMR3) && pVCpu->hm.s.Event.fPending;
}


/**
 * Checks if the VMX-preemption timer is being used.
 *
 * @returns true if the VMX-preemption timer is being used, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HMR3IsVmxPreemptionTimerUsed(PVM pVM)
{
    return HMIsEnabled(pVM)
        && pVM->hm.s.vmx.fEnabled
        && pVM->hm.s.vmx.fUsePreemptTimer;
}


/**
 * Restart an I/O instruction that was refused in ring-0
 *
 * @returns Strict VBox status code. Informational status codes other than the one documented
 *          here are to be treated as internal failure. Use IOM_SUCCESS() to check for success.
 * @retval  VINF_SUCCESS                Success.
 * @retval  VINF_EM_FIRST-VINF_EM_LAST  Success with some exceptions (see IOM_SUCCESS()), the
 *                                      status code must be passed on to EM.
 * @retval  VERR_NOT_FOUND if no pending I/O instruction.
 *
 * @param   pVM         Pointer to the VM.
 * @param   pVCpu       Pointer to the VMCPU.
 * @param   pCtx        Pointer to the guest CPU context.
 */
VMMR3DECL(VBOXSTRICTRC) HMR3RestartPendingIOInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    HMPENDINGIO enmType = pVCpu->hm.s.PendingIO.enmType;

    pVCpu->hm.s.PendingIO.enmType = HMPENDINGIO_INVALID;

    if (    pVCpu->hm.s.PendingIO.GCPtrRip != pCtx->rip
        ||  enmType  == HMPENDINGIO_INVALID)
        return VERR_NOT_FOUND;

    VBOXSTRICTRC rcStrict;
    switch (enmType)
    {
        case HMPENDINGIO_PORT_READ:
        {
            uint32_t uAndVal = pVCpu->hm.s.PendingIO.s.Port.uAndVal;
            uint32_t u32Val  = 0;

            rcStrict = IOMIOPortRead(pVM, pVCpu->hm.s.PendingIO.s.Port.uPort,
                                     &u32Val,
                                     pVCpu->hm.s.PendingIO.s.Port.cbSize);
            if (IOM_SUCCESS(rcStrict))
            {
                /* Write back to the EAX register. */
                pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Val & uAndVal);
                pCtx->rip = pVCpu->hm.s.PendingIO.GCPtrRipNext;
            }
            break;
        }

        case HMPENDINGIO_PORT_WRITE:
            rcStrict = IOMIOPortWrite(pVM, pVCpu->hm.s.PendingIO.s.Port.uPort,
                                      pCtx->eax & pVCpu->hm.s.PendingIO.s.Port.uAndVal,
                                      pVCpu->hm.s.PendingIO.s.Port.cbSize);
            if (IOM_SUCCESS(rcStrict))
                pCtx->rip = pVCpu->hm.s.PendingIO.GCPtrRipNext;
            break;

        default:
            AssertLogRelFailedReturn(VERR_HM_UNKNOWN_IO_INSTRUCTION);
    }

    return rcStrict;
}


/**
 * Inject an NMI into a running VM (only VCPU 0!)
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int)  HMR3InjectNMI(PVM pVM)
{
    VMCPU_FF_SET(&pVM->aCpus[0], VMCPU_FF_INTERRUPT_NMI);
    return VINF_SUCCESS;
}


/**
 * Check fatal VT-x/AMD-V error and produce some meaningful
 * log release message.
 *
 * @param   pVM         Pointer to the VM.
 * @param   iStatusCode VBox status code.
 */
VMMR3DECL(void) HMR3CheckError(PVM pVM, int iStatusCode)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        switch (iStatusCode)
        {
            case VERR_VMX_INVALID_VMCS_FIELD:
                break;

            case VERR_VMX_INVALID_VMCS_PTR:
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current pointer %RGp vs %RGp\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u64VMCSPhys, pVM->aCpus[i].hm.s.vmx.HCPhysVMCS));
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current VMCS version %x\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u32VMCSRevision));
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Entered Cpu %d\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.idEnteredCpu));
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current Cpu %d\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.idCurrentCpu));
                break;

            case VERR_VMX_UNABLE_TO_START_VM:
                LogRel(("VERR_VMX_UNABLE_TO_START_VM: CPU%d instruction error %x\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u32InstrError));
                LogRel(("VERR_VMX_UNABLE_TO_START_VM: CPU%d exit reason       %x\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u32ExitReason));
                if (pVM->aCpus[i].hm.s.vmx.lasterror.u32InstrError == VMX_ERROR_VMENTRY_INVALID_CONTROL_FIELDS)
                {
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d MSRBitmapPhys %RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysMsrBitmap));
#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d GuestMSRPhys  %RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysGuestMsr));
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d HostMsrPhys   %RHp\n", i, pVM->aCpus[i].hm.s.vmx.HCPhysHostMsr));
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d cGuestMSRs    %x\n",   i, pVM->aCpus[i].hm.s.vmx.cGuestMsrs));
#endif
                }
                /** @todo Log VM-entry event injection control fields
                 *        VMX_VMCS_CTRL_ENTRY_IRQ_INFO, VMX_VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE
                 *        and VMX_VMCS_CTRL_ENTRY_INSTR_LENGTH from the VMCS. */
                break;

            case VERR_VMX_UNABLE_TO_RESUME_VM:
                LogRel(("VERR_VMX_UNABLE_TO_RESUME_VM: CPU%d instruction error %x\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u32InstrError));
                LogRel(("VERR_VMX_UNABLE_TO_RESUME_VM: CPU%d exit reason       %x\n", i, pVM->aCpus[i].hm.s.vmx.lasterror.u32ExitReason));
                break;

            case VERR_VMX_INVALID_VMXON_PTR:
                break;
        }
    }

    if (iStatusCode == VERR_VMX_UNABLE_TO_START_VM)
    {
        LogRel(("VERR_VMX_UNABLE_TO_START_VM: VM-entry allowed    %x\n", pVM->hm.s.vmx.msr.vmx_entry.n.allowed1));
        LogRel(("VERR_VMX_UNABLE_TO_START_VM: VM-entry disallowed %x\n", pVM->hm.s.vmx.msr.vmx_entry.n.disallowed0));
    }
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) hmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    int rc;

    Log(("hmR3Save:\n"));

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        /*
         * Save the basic bits - fortunately all the other things can be resynced on load.
         */
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hm.s.Event.fPending);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hm.s.Event.u32ErrCode);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU64(pSSM, pVM->aCpus[i].hm.s.Event.u64IntrInfo);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hm.s.vmx.enmLastSeenGuestMode);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hm.s.vmx.enmCurrGuestMode);
        AssertRCReturn(rc, rc);
        rc = SSMR3PutU32(pSSM, pVM->aCpus[i].hm.s.vmx.enmPrevGuestMode);
        AssertRCReturn(rc, rc);
    }
#ifdef VBOX_HM_WITH_GUEST_PATCHING
    rc = SSMR3PutGCPtr(pSSM, pVM->hm.s.pGuestPatchMem);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutGCPtr(pSSM, pVM->hm.s.pFreeGuestPatchMem);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pVM->hm.s.cbGuestPatchMem);
    AssertRCReturn(rc, rc);

    /* Store all the guest patch records too. */
    rc = SSMR3PutU32(pSSM, pVM->hm.s.cPatches);
    AssertRCReturn(rc, rc);

    for (unsigned i = 0; i < pVM->hm.s.cPatches; i++)
    {
        PHMTPRPATCH pPatch = &pVM->hm.s.aPatches[i];

        rc = SSMR3PutU32(pSSM, pPatch->Core.Key);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutMem(pSSM, pPatch->aOpcode, sizeof(pPatch->aOpcode));
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pPatch->cbOp);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutMem(pSSM, pPatch->aNewOpcode, sizeof(pPatch->aNewOpcode));
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pPatch->cbNewOp);
        AssertRCReturn(rc, rc);

        AssertCompileSize(HMTPRINSTR, 4);
        rc = SSMR3PutU32(pSSM, (uint32_t)pPatch->enmType);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pPatch->uSrcOperand);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pPatch->uDstOperand);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pPatch->pJumpTarget);
        AssertRCReturn(rc, rc);

        rc = SSMR3PutU32(pSSM, pPatch->cFaults);
        AssertRCReturn(rc, rc);
    }
#endif
    return VINF_SUCCESS;
}


/**
 * Execute state load operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 * @param   uVersion        Data layout version.
 * @param   uPass           The data pass.
 */
static DECLCALLBACK(int) hmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    int rc;

    Log(("hmR3Load:\n"));
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Validate version.
     */
    if (   uVersion != HM_SSM_VERSION
        && uVersion != HM_SSM_VERSION_NO_PATCHING
        && uVersion != HM_SSM_VERSION_2_0_X)
    {
        AssertMsgFailed(("hmR3Load: Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        rc = SSMR3GetU32(pSSM, &pVM->aCpus[i].hm.s.Event.fPending);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &pVM->aCpus[i].hm.s.Event.u32ErrCode);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU64(pSSM, &pVM->aCpus[i].hm.s.Event.u64IntrInfo);
        AssertRCReturn(rc, rc);

        if (uVersion >= HM_SSM_VERSION_NO_PATCHING)
        {
            uint32_t val;

            rc = SSMR3GetU32(pSSM, &val);
            AssertRCReturn(rc, rc);
            pVM->aCpus[i].hm.s.vmx.enmLastSeenGuestMode = (PGMMODE)val;

            rc = SSMR3GetU32(pSSM, &val);
            AssertRCReturn(rc, rc);
            pVM->aCpus[i].hm.s.vmx.enmCurrGuestMode = (PGMMODE)val;

            rc = SSMR3GetU32(pSSM, &val);
            AssertRCReturn(rc, rc);
            pVM->aCpus[i].hm.s.vmx.enmPrevGuestMode = (PGMMODE)val;
        }
    }
#ifdef VBOX_HM_WITH_GUEST_PATCHING
    if (uVersion > HM_SSM_VERSION_NO_PATCHING)
    {
        rc = SSMR3GetGCPtr(pSSM, &pVM->hm.s.pGuestPatchMem);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetGCPtr(pSSM, &pVM->hm.s.pFreeGuestPatchMem);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &pVM->hm.s.cbGuestPatchMem);
        AssertRCReturn(rc, rc);

        /* Fetch all TPR patch records. */
        rc = SSMR3GetU32(pSSM, &pVM->hm.s.cPatches);
        AssertRCReturn(rc, rc);

        for (unsigned i = 0; i < pVM->hm.s.cPatches; i++)
        {
            PHMTPRPATCH pPatch = &pVM->hm.s.aPatches[i];

            rc = SSMR3GetU32(pSSM, &pPatch->Core.Key);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetMem(pSSM, pPatch->aOpcode, sizeof(pPatch->aOpcode));
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->cbOp);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetMem(pSSM, pPatch->aNewOpcode, sizeof(pPatch->aNewOpcode));
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->cbNewOp);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, (uint32_t *)&pPatch->enmType);
            AssertRCReturn(rc, rc);

            if (pPatch->enmType == HMTPRINSTR_JUMP_REPLACEMENT)
                pVM->hm.s.fTPRPatchingActive = true;

            Assert(pPatch->enmType == HMTPRINSTR_JUMP_REPLACEMENT || pVM->hm.s.fTPRPatchingActive == false);

            rc = SSMR3GetU32(pSSM, &pPatch->uSrcOperand);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->uDstOperand);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->cFaults);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->pJumpTarget);
            AssertRCReturn(rc, rc);

            Log(("hmR3Load: patch %d\n", i));
            Log(("Key       = %x\n", pPatch->Core.Key));
            Log(("cbOp      = %d\n", pPatch->cbOp));
            Log(("cbNewOp   = %d\n", pPatch->cbNewOp));
            Log(("type      = %d\n", pPatch->enmType));
            Log(("srcop     = %d\n", pPatch->uSrcOperand));
            Log(("dstop     = %d\n", pPatch->uDstOperand));
            Log(("cFaults   = %d\n", pPatch->cFaults));
            Log(("target    = %x\n", pPatch->pJumpTarget));
            rc = RTAvloU32Insert(&pVM->hm.s.PatchTree, &pPatch->Core);
            AssertRC(rc);
        }
    }
#endif

    /* Recheck all VCPUs if we can go straight into hm execution mode. */
    if (HMIsEnabled(pVM))
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            HMR3CanExecuteGuest(pVM, CPUMQueryGuestCtxPtr(pVCpu));
        }
    }
    return VINF_SUCCESS;
}

