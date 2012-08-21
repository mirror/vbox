/* $Id$ */
/** @file
 * HWACCM - Intel/AMD VM Hardware Support Manager.
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
#define LOG_GROUP LOG_GROUP_HWACCM
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
#include <VBox/vmm/hwacc_vmx.h>
#include <VBox/vmm/hwacc_svm.h>
#include "HWACCMInternal.h"
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
    EXIT_REASON(SVM_EXIT_INTR                       , 96, "Physical maskable interrupt."),
    EXIT_REASON(SVM_EXIT_NMI                        , 97, "Physical non-maskable interrupt."),
    EXIT_REASON(SVM_EXIT_SMI                        , 98, "System management interrupt."),
    EXIT_REASON(SVM_EXIT_INIT                       , 99, "Physical INIT signal."),
    EXIT_REASON(SVM_EXIT_VINTR                      ,100, "Virtual interrupt."),
    EXIT_REASON(SVM_EXIT_CR0_SEL_WRITE              ,101, "Write to CR0 that changed any bits other than CR0.TS or CR0.MP."),
    EXIT_REASON(SVM_EXIT_IDTR_READ                  ,102, "Read IDTR"),
    EXIT_REASON(SVM_EXIT_GDTR_READ                  ,103, "Read GDTR"),
    EXIT_REASON(SVM_EXIT_LDTR_READ                  ,104, "Read LDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ                    ,105, "Read TR."),
    EXIT_REASON(SVM_EXIT_TR_READ                    ,106, "Write IDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ                    ,107, "Write GDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ                    ,108, "Write LDTR."),
    EXIT_REASON(SVM_EXIT_TR_READ                    ,109, "Write TR."),
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
    EXIT_REASON(SVM_EXIT_SHUTDOWN                   ,127, "Shutdown."),
    EXIT_REASON(SVM_EXIT_VMRUN                      ,128, "VMRUN instruction."),
    EXIT_REASON(SVM_EXIT_VMMCALL                    ,129, "VMCALL instruction."),
    EXIT_REASON(SVM_EXIT_VMLOAD                     ,130, "VMLOAD instruction."),
    EXIT_REASON(SVM_EXIT_VMSAVE                     ,131, "VMSAVE instruction."),
    EXIT_REASON(SVM_EXIT_STGI                       ,132, "STGI instruction."),
    EXIT_REASON(SVM_EXIT_CLGI                       ,133, "CLGI instruction."),
    EXIT_REASON(SVM_EXIT_SKINIT                     ,134, "SKINIT instruction."),
    EXIT_REASON(SVM_EXIT_RDTSCP                     ,135, "RDTSCP instruction."),
    EXIT_REASON(SVM_EXIT_ICEBP                      ,136, "ICEBP instruction."),
    EXIT_REASON(SVM_EXIT_WBINVD                     ,137, "WBINVD instruction."),
    EXIT_REASON(SVM_EXIT_MONITOR                    ,138, "MONITOR instruction."),
    EXIT_REASON(SVM_EXIT_MWAIT_UNCOND               ,139, "MWAIT instruction unconditional."),
    EXIT_REASON(SVM_EXIT_MWAIT_ARMED                ,140, "MWAIT instruction when armed."),
    EXIT_REASON(SVM_EXIT_NPF                        ,1024, "Nested paging: host-level page fault occurred (EXITINFO1 contains fault errorcode; EXITINFO2 contains the guest physical address causing the fault)."),
    EXIT_REASON_NIL()
};
# undef EXIT_REASON
# undef EXIT_REASON_NIL
#endif /* VBOX_WITH_STATISTICS */

/*******************************************************************************
*   Internal Functions                                                         *
*******************************************************************************/
static DECLCALLBACK(int) hwaccmR3Save(PVM pVM, PSSMHANDLE pSSM);
static DECLCALLBACK(int) hwaccmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass);
static int hwaccmR3InitCPU(PVM pVM);
static int hwaccmR3InitFinalizeR0(PVM pVM);
static int hwaccmR3TermCPU(PVM pVM);


/**
 * Initializes the HWACCM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
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
    int rc = SSMR3RegisterInternal(pVM, "HWACCM", 0, HWACCM_SSM_VERSION, sizeof(HWACCM),
                                   NULL, NULL, NULL,
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
    pVM->hwaccm.s.fLargePages    = false;

    /* Disabled by default. */
    pVM->fHWACCMEnabled = false;

    /*
     * Check CFGM options.
     */
    PCFGMNODE pRoot      = CFGMR3GetRoot(pVM);
    PCFGMNODE pHWVirtExt = CFGMR3GetChild(pRoot, "HWVirtExt/");
    /* Nested paging: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "EnableNestedPaging", &pVM->hwaccm.s.fAllowNestedPaging, false);
    AssertRC(rc);

    /* Large pages: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "EnableLargePages", &pVM->hwaccm.s.fLargePages, false);
    AssertRC(rc);

    /* VT-x VPID: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "EnableVPID", &pVM->hwaccm.s.vmx.fAllowVPID, false);
    AssertRC(rc);

    /* HWACCM support must be explicitely enabled in the configuration file. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "Enabled", &pVM->hwaccm.s.fAllowed, false);
    AssertRC(rc);

    /* TPR patching for 32 bits (Windows) guests with IO-APIC: disabled by default. */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "TPRPatchingEnabled", &pVM->hwaccm.s.fTRPPatchingAllowed, false);
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
    /*
     * 64-bit mode is configurable and it depends on both the kernel mode and VT-x.
     * (To use the default, don't set 64bitEnabled in CFGM.)
     */
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
    /*
     * On 64-bit hosts 64-bit guest support is enabled by default, but allow this to be overridden
     * via VBoxInternal/HWVirtExt/64bitEnabled=0. (ConsoleImpl2.cpp doesn't set this to false for 64-bit.)*
     */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "64bitEnabled", &pVM->hwaccm.s.fAllow64BitGuests, true);
    AssertLogRelRCReturn(rc, rc);
#endif


    /*
     * Determine the init method for AMD-V and VT-x; either one global init for each host CPU
     *  or local init each time we wish to execute guest code.
     *
     *  Default false for Mac OS X and Windows due to the higher risk of conflicts with other hypervisors.
     */
    rc = CFGMR3QueryBoolDef(pHWVirtExt, "Exclusive", &pVM->hwaccm.s.fGlobalInit,
#if defined(RT_OS_DARWIN) || defined(RT_OS_WINDOWS)
                            false
#else
                            true
#endif
                           );

    /* Max number of resume loops. */
    rc = CFGMR3QueryU32Def(pHWVirtExt, "MaxResumeLoops", &pVM->hwaccm.s.cMaxResumeLoops, 0 /* set by R0 later */);
    AssertRC(rc);

    return rc;
}


/**
 * Initializes the per-VCPU HWACCM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
static int hwaccmR3InitCPU(PVM pVM)
{
    LogFlow(("HWACCMR3InitCPU\n"));

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        pVCpu->hwaccm.s.fActive = false;
    }

#ifdef VBOX_WITH_STATISTICS
    STAM_REG(pVM, &pVM->hwaccm.s.StatTPRPatchSuccess,   STAMTYPE_COUNTER, "/HWACCM/TPR/Patch/Success",  STAMUNIT_OCCURENCES,    "Number of times an instruction was successfully patched.");
    STAM_REG(pVM, &pVM->hwaccm.s.StatTPRPatchFailure,   STAMTYPE_COUNTER, "/HWACCM/TPR/Patch/Failed",   STAMUNIT_OCCURENCES,    "Number of unsuccessful patch attempts.");
    STAM_REG(pVM, &pVM->hwaccm.s.StatTPRReplaceSuccess, STAMTYPE_COUNTER, "/HWACCM/TPR/Replace/Success",STAMUNIT_OCCURENCES,    "Number of times an instruction was successfully patched.");
    STAM_REG(pVM, &pVM->hwaccm.s.StatTPRReplaceFailure, STAMTYPE_COUNTER, "/HWACCM/TPR/Replace/Failed", STAMUNIT_OCCURENCES,    "Number of unsuccessful patch attempts.");

    /*
     * Statistics.
     */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];
        int    rc;

        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatPoke, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of RTMpPokeCpu",
                             "/PROF/HWACCM/CPU%d/Poke", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatSpinPoke, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of poke wait",
                             "/PROF/HWACCM/CPU%d/PokeWait", i);
        AssertRC(rc);
        rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.StatSpinPokeFailed, STAMTYPE_PROFILE, STAMVISIBILITY_USED, STAMUNIT_TICKS_PER_CALL, "Profiling of poke wait when RTMpPokeCpu fails",
                             "/PROF/HWACCM/CPU%d/PokeWaitFailed", i);
        AssertRC(rc);
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
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitShadowPFEM,         "/HWACCM/CPU%d/Exit/Trap/Shw/#PF-EM");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestPF,            "/HWACCM/CPU%d/Exit/Trap/Gst/#PF");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestUD,            "/HWACCM/CPU%d/Exit/Trap/Gst/#UD");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestSS,            "/HWACCM/CPU%d/Exit/Trap/Gst/#SS");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestNP,            "/HWACCM/CPU%d/Exit/Trap/Gst/#NP");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestGP,            "/HWACCM/CPU%d/Exit/Trap/Gst/#GP");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestMF,            "/HWACCM/CPU%d/Exit/Trap/Gst/#MF");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestDE,            "/HWACCM/CPU%d/Exit/Trap/Gst/#DE");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestDB,            "/HWACCM/CPU%d/Exit/Trap/Gst/#DB");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestBP,            "/HWACCM/CPU%d/Exit/Trap/Gst/#BP");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestXF,            "/HWACCM/CPU%d/Exit/Trap/Gst/#XF");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitGuestXcpUnk,        "/HWACCM/CPU%d/Exit/Trap/Gst/Other");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitInvlpg,             "/HWACCM/CPU%d/Exit/Instr/Invlpg");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitInvd,               "/HWACCM/CPU%d/Exit/Instr/Invd");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitCpuid,              "/HWACCM/CPU%d/Exit/Instr/Cpuid");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdtsc,              "/HWACCM/CPU%d/Exit/Instr/Rdtsc");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdtscp,             "/HWACCM/CPU%d/Exit/Instr/Rdtscp");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdpmc,              "/HWACCM/CPU%d/Exit/Instr/Rdpmc");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitRdmsr,              "/HWACCM/CPU%d/Exit/Instr/Rdmsr");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitWrmsr,              "/HWACCM/CPU%d/Exit/Instr/Wrmsr");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitMwait,              "/HWACCM/CPU%d/Exit/Instr/Mwait");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitMonitor,            "/HWACCM/CPU%d/Exit/Instr/Monitor");
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
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitPreemptPending,     "/HWACCM/CPU%d/Exit/PreemptPending");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatExitMTF,                "/HWACCM/CPU%d/Exit/MonitorTrapFlag");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatSwitchGuestIrq,         "/HWACCM/CPU%d/Switch/IrqPending");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatSwitchToR3,             "/HWACCM/CPU%d/Switch/ToR3");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatIntInject,              "/HWACCM/CPU%d/Irq/Inject");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatIntReinject,            "/HWACCM/CPU%d/Irq/Reinject");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatPendingHostIrq,         "/HWACCM/CPU%d/Irq/PendingOnHost");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushPage,              "/HWACCM/CPU%d/Flush/Page");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushPageManual,        "/HWACCM/CPU%d/Flush/Page/Virt");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushPhysPageManual,    "/HWACCM/CPU%d/Flush/Page/Phys");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFlushTLB,               "/HWACCM/CPU%d/Flush/TLB");
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
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatTSCInterceptOverFlow,   "/HWACCM/CPU%d/TSC/InterceptOverflow");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDRxArmed,               "/HWACCM/CPU%d/Debug/Armed");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDRxContextSwitch,       "/HWACCM/CPU%d/Debug/ContextSwitch");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDRxIOCheck,             "/HWACCM/CPU%d/Debug/IOCheck");

        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatLoadMinimal,            "/HWACCM/CPU%d/Load/Minimal");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatLoadFull,               "/HWACCM/CPU%d/Load/Full");

#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatFpu64SwitchBack,        "/HWACCM/CPU%d/Switch64/Fpu");
        HWACCM_REG_COUNTER(&pVCpu->hwaccm.s.StatDebug64SwitchBack,      "/HWACCM/CPU%d/Switch64/Debug");
#endif

        for (unsigned j = 0; j < RT_ELEMENTS(pVCpu->hwaccm.s.StatExitCRxWrite); j++)
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
            for (int j = 0; j < MAX_EXITREASON_STAT; j++)
            {
                if (papszDesc[j])
                {
                    rc = STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.paStatExitReason[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES,
                                        papszDesc[j], "/HWACCM/CPU%d/Exit/Reason/%02x", i, j);
                    AssertRC(rc);
                }
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

        rc = MMHyperAlloc(pVM, sizeof(STAMCOUNTER) * 256, 8, MM_TAG_HWACCM, (void **)&pVCpu->hwaccm.s.paStatInjectedIrqs);
        AssertRCReturn(rc, rc);
        pVCpu->hwaccm.s.paStatInjectedIrqsR0 = MMHyperR3ToR0(pVM, pVCpu->hwaccm.s.paStatInjectedIrqs);
# ifdef VBOX_WITH_2X_4GB_ADDR_SPACE
        Assert(pVCpu->hwaccm.s.paStatInjectedIrqsR0 != NIL_RTR0PTR || !VMMIsHwVirtExtForced(pVM));
# else
        Assert(pVCpu->hwaccm.s.paStatInjectedIrqsR0 != NIL_RTR0PTR);
# endif
        for (unsigned j = 0; j < 255; j++)
            STAMR3RegisterF(pVM, &pVCpu->hwaccm.s.paStatInjectedIrqs[j], STAMTYPE_COUNTER, STAMVISIBILITY_USED, STAMUNIT_OCCURENCES, "Forwarded interrupts.",
                            (j < 0x20) ? "/HWACCM/CPU%d/Interrupt/Trap/%02X" : "/HWACCM/CPU%d/Interrupt/IRQ/%02X", i, j);

    }
#endif /* VBOX_WITH_STATISTICS */

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
    /* Magic marker for searching in crash dumps. */
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
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
 * Called when a init phase has completed.
 *
 * @returns VBox status code.
 * @param   pVM                 The VM.
 * @param   enmWhat             The phase that completed.
 */
VMMR3_INT_DECL(int) HWACCMR3InitCompleted(PVM pVM, VMINITCOMPLETED enmWhat)
{
    switch (enmWhat)
    {
        case VMINITCOMPLETED_RING3:
            return hwaccmR3InitCPU(pVM);
        case VMINITCOMPLETED_RING0:
            return hwaccmR3InitFinalizeR0(pVM);
        default:
            return VINF_SUCCESS;
    }
}


/**
 * Turns off normal raw mode features.
 *
 * @param   pVM         Pointer to the VM.
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
static int hwaccmR3InitFinalizeR0(PVM pVM)
{
    int rc;

    /*
     * Hack to allow users to work around broken BIOSes that incorrectly set EFER.SVME, which makes us believe somebody else
     * is already using AMD-V.
     */
    if (    !pVM->hwaccm.s.vmx.fSupported
        &&  !pVM->hwaccm.s.svm.fSupported
        &&  pVM->hwaccm.s.lLastError == VERR_SVM_IN_USE /* implies functional AMD-V */
        &&  RTEnvExist("VBOX_HWVIRTEX_IGNORE_SVM_IN_USE"))
    {
        LogRel(("HWACCM: VBOX_HWVIRTEX_IGNORE_SVM_IN_USE active!\n"));
        pVM->hwaccm.s.svm.fSupported        = true;
        pVM->hwaccm.s.svm.fIgnoreInUseError = true;
    }
    else
    if (    !pVM->hwaccm.s.vmx.fSupported
        &&  !pVM->hwaccm.s.svm.fSupported)
    {
        LogRel(("HWACCM: No VT-x or AMD-V CPU extension found. Reason %Rrc\n", pVM->hwaccm.s.lLastError));
        LogRel(("HWACCM: VMX MSR_IA32_FEATURE_CONTROL=%RX64\n", pVM->hwaccm.s.vmx.msr.feature_ctrl));

        if (VMMIsHwVirtExtForced(pVM))
        {
            switch (pVM->hwaccm.s.lLastError)
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
                return pVM->hwaccm.s.lLastError;
            }
        }
        return VINF_SUCCESS;
    }

    if (pVM->hwaccm.s.vmx.fSupported)
    {
        rc = SUPR3QueryVTxSupported();
        if (RT_FAILURE(rc))
        {
#ifdef RT_OS_LINUX
            LogRel(("HWACCM: The host kernel does not support VT-x -- Linux 2.6.13 or newer required!\n"));
#else
            LogRel(("HWACCM: The host kernel does not support VT-x!\n"));
#endif
            if (   pVM->cCpus > 1
                || VMMIsHwVirtExtForced(pVM))
                return rc;

            /* silently fall back to raw mode */
            return VINF_SUCCESS;
        }
    }

    if (!pVM->hwaccm.s.fAllowed)
        return VINF_SUCCESS;    /* nothing to do */

    /* Enable VT-x or AMD-V on all host CPUs. */
    rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HWACC_ENABLE, 0, NULL);
    if (RT_FAILURE(rc))
    {
        LogRel(("HWACCMR3InitFinalize: SUPR3CallVMMR0Ex VMMR0_DO_HWACC_ENABLE failed with %Rrc\n", rc));
        return rc;
    }
    Assert(!pVM->fHWACCMEnabled || VMMIsHwVirtExtForced(pVM));

    pVM->hwaccm.s.fHasIoApic = PDMHasIoApic(pVM);
    /* No TPR patching is required when the IO-APIC is not enabled for this VM. (Main should have taken care of this already) */
    if (!pVM->hwaccm.s.fHasIoApic)
    {
        Assert(!pVM->hwaccm.s.fTRPPatchingAllowed); /* paranoia */
        pVM->hwaccm.s.fTRPPatchingAllowed = false;
    }

    bool fOldBuffered = RTLogRelSetBuffering(true /*fBuffered*/);
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
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_X2APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT\n"));

                val = pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.disallowed0;
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_X2APIC)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_X2APIC *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_EPT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_VPID *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE *must* be set\n"));
                if (val & VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT)
                    LogRel(("HWACCM:    VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT *must* be set\n"));
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
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_SINGLE_CONTEXT)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_SINGLE_CONTEXT\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_ALL_CONTEXTS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVEPT_CAPS_ALL_CONTEXTS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV_ADDR)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_INDIV_ADDR\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_ALL_CONTEXTS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_ALL_CONTEXTS\n"));
                if (pVM->hwaccm.s.vmx.msr.vmx_eptcaps & MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT_RETAIN_GLOBALS)
                    LogRel(("HWACCM:    MSR_IA32_VMX_EPT_CAPS_INVVPID_CAPS_SINGLE_CONTEXT_RETAIN_GLOBALS\n"));
            }

            LogRel(("HWACCM: MSR_IA32_VMX_MISC             = %RX64\n", pVM->hwaccm.s.vmx.msr.vmx_misc));
            if (MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hwaccm.s.vmx.msr.vmx_misc) == pVM->hwaccm.s.vmx.cPreemptTimerShift)
                LogRel(("HWACCM:    MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT %x\n", MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hwaccm.s.vmx.msr.vmx_misc)));
            else
            {
                LogRel(("HWACCM:    MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT %x - erratum detected, using %x instead\n",
                        MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(pVM->hwaccm.s.vmx.msr.vmx_misc), pVM->hwaccm.s.vmx.cPreemptTimerShift));
            }
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

            /* Paranoia */
            AssertRelease(MSR_IA32_VMX_MISC_MAX_MSR(pVM->hwaccm.s.vmx.msr.vmx_misc) >= 512);

            for (VMCPUID i = 0; i < pVM->cCpus; i++)
            {
                LogRel(("HWACCM: VCPU%d: MSR bitmap physaddr    = %RHp\n", i, pVM->aCpus[i].hwaccm.s.vmx.pMSRBitmapPhys));
                LogRel(("HWACCM: VCPU%d: VMCS physaddr          = %RHp\n", i, pVM->aCpus[i].hwaccm.s.vmx.HCPhysVMCS));
            }

            if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_EPT)
                pVM->hwaccm.s.fNestedPaging = pVM->hwaccm.s.fAllowNestedPaging;

            if (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VPID)
                pVM->hwaccm.s.vmx.fVPID = pVM->hwaccm.s.vmx.fAllowVPID;

            /*
             * Disallow RDTSCP in the guest if there is no secondary process-based VM execution controls as otherwise
             * RDTSCP would cause a #UD. There might be no CPUs out there where this happens, as RDTSCP was introduced
             * in Nehalems and secondary VM exec. controls should be supported in all of them, but nonetheless it's Intel...
             */
            if (!(pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                && CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP))
            {
                CPUMClearGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_RDTSCP);
            }

            /* Unrestricted guest execution relies on EPT. */
            if (    pVM->hwaccm.s.fNestedPaging
                &&  (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE))
            {
                pVM->hwaccm.s.vmx.fUnrestrictedGuest = true;
            }

            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            if (!pVM->hwaccm.s.vmx.fUnrestrictedGuest)
            {
                /* Allocate three pages for the TSS we need for real mode emulation. (2 pages for the IO bitmap) */
                rc = PDMR3VMMDevHeapAlloc(pVM, HWACCM_VTX_TOTAL_DEVHEAP_MEM, (RTR3PTR *)&pVM->hwaccm.s.vmx.pRealModeTSS);
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

                    /*
                     * Construct a 1024 element page directory with 4 MB pages for the identity mapped page table used in
                     * real and protected mode without paging with EPT.
                     */
                    pVM->hwaccm.s.vmx.pNonPagingModeEPTPageTable = (PX86PD)((char *)pVM->hwaccm.s.vmx.pRealModeTSS + PAGE_SIZE * 3);
                    for (unsigned i = 0; i < X86_PG_ENTRIES; i++)
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
            }

            rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
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
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);
                }
                else
                /* Turn on NXE if PAE has been enabled *and* the host has turned on NXE (we reuse the host EFER in the switcher) */
                /* Todo: this needs to be fixed properly!! */
                if (    CPUMGetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_PAE)
                    &&  (pVM->hwaccm.s.vmx.hostEFER & MSR_K6_EFER_NXE))
                    CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_NX);

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
                    if (pVM->hwaccm.s.vmx.enmFlushEPT == VMX_FLUSH_EPT_SINGLE_CONTEXT)
                        LogRel(("HWACCM: enmFlushEPT                   = VMX_FLUSH_EPT_SINGLE_CONTEXT\n"));
                    else if (pVM->hwaccm.s.vmx.enmFlushEPT == VMX_FLUSH_EPT_ALL_CONTEXTS)
                        LogRel(("HWACCM: enmFlushEPT                   = VMX_FLUSH_EPT_ALL_CONTEXTS\n"));
                    else if (pVM->hwaccm.s.vmx.enmFlushEPT == VMX_FLUSH_EPT_NOT_SUPPORTED)
                        LogRel(("HWACCM: enmFlushEPT                   = VMX_FLUSH_EPT_NOT_SUPPORTED\n"));
                    else
                        LogRel(("HWACCM: enmFlushEPT                   = %d\n", pVM->hwaccm.s.vmx.enmFlushEPT));

                    if (pVM->hwaccm.s.vmx.fUnrestrictedGuest)
                        LogRel(("HWACCM: Unrestricted guest execution enabled!\n"));

#if HC_ARCH_BITS == 64
                    if (pVM->hwaccm.s.fLargePages)
                    {
                        /* Use large (2 MB) pages for our EPT PDEs where possible. */
                        PGMSetLargePageUsage(pVM, true);
                        LogRel(("HWACCM: Large page support enabled!\n"));
                    }
#endif
                }
                else
                    Assert(!pVM->hwaccm.s.vmx.fUnrestrictedGuest);

                if (pVM->hwaccm.s.vmx.fVPID)
                {
                    LogRel(("HWACCM: Enabled VPID\n"));
                    if (pVM->hwaccm.s.vmx.enmFlushVPID == VMX_FLUSH_VPID_INDIV_ADDR)
                        LogRel(("HWACCM: enmFlushVPID                  = VMX_FLUSH_VPID_INDIV_ADDR\n"));
                    else if (pVM->hwaccm.s.vmx.enmFlushVPID == VMX_FLUSH_VPID_SINGLE_CONTEXT)
                        LogRel(("HWACCM: enmFlushVPID                  = VMX_FLUSH_VPID_SINGLE_CONTEXT\n"));
                    else if (pVM->hwaccm.s.vmx.enmFlushVPID == VMX_FLUSH_VPID_ALL_CONTEXTS)
                        LogRel(("HWACCM: enmFlushVPID                  = VMX_FLUSH_VPID_ALL_CONTEXTS\n"));
                    else if (pVM->hwaccm.s.vmx.enmFlushVPID == VMX_FLUSH_VPID_SINGLE_CONTEXT_RETAIN_GLOBALS)
                        LogRel(("HWACCM: enmFlushVPID                  = VMX_FLUSH_VPID_SINGLE_CONTEXT_RETAIN_GLOBALS\n"));
                    else
                        LogRel(("HWACCM: enmFlushVPID                  = %d\n", pVM->hwaccm.s.vmx.enmFlushVPID));
                }
                else if (pVM->hwaccm.s.vmx.enmFlushVPID == VMX_FLUSH_VPID_NOT_SUPPORTED)
                    LogRel(("HWACCM: Ignoring VPID capabilities of CPU.\n"));

                /* TPR patching status logging. */
                if (pVM->hwaccm.s.fTRPPatchingAllowed)
                {
                    if (    (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL)
                        &&  (pVM->hwaccm.s.vmx.msr.vmx_proc_ctls2.n.allowed1 & VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC))
                    {
                        pVM->hwaccm.s.fTRPPatchingAllowed = false;  /* not necessary as we have a hardware solution. */
                        LogRel(("HWACCM: TPR Patching not required (VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC).\n"));
                    }
                    else
                    {
                        uint32_t u32Eax, u32Dummy;

                        /* TPR patching needs access to the MSR_K8_LSTAR msr. */
                        ASMCpuId(0x80000000, &u32Eax, &u32Dummy, &u32Dummy, &u32Dummy);
                        if (    u32Eax < 0x80000001
                            ||  !(ASMCpuId_EDX(0x80000001) & X86_CPUID_EXT_FEATURE_EDX_LONG_MODE))
                        {
                            pVM->hwaccm.s.fTRPPatchingAllowed = false;
                            LogRel(("HWACCM: TPR patching disabled (long mode not supported).\n"));
                        }
                    }
                }
                LogRel(("HWACCM: TPR Patching %s.\n", (pVM->hwaccm.s.fTRPPatchingAllowed) ? "enabled" : "disabled"));

                /*
                 * Check for preemption timer config override and log the state of it.
                 */
                if (pVM->hwaccm.s.vmx.fUsePreemptTimer)
                {
                    PCFGMNODE pCfgHwAccM = CFGMR3GetChild(CFGMR3GetRoot(pVM), "HWACCM");
                    int rc2 = CFGMR3QueryBoolDef(pCfgHwAccM, "UsePreemptTimer", &pVM->hwaccm.s.vmx.fUsePreemptTimer, true);
                    AssertLogRelRC(rc2);
                }
                if (pVM->hwaccm.s.vmx.fUsePreemptTimer)
                    LogRel(("HWACCM: Using the VMX-preemption timer (cPreemptTimerShift=%u)\n", pVM->hwaccm.s.vmx.cPreemptTimerShift));
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
            LogRel(("HWACCM: AMD HWCR MSR                      = %RX64\n", pVM->hwaccm.s.svm.msrHWCR));
            LogRel(("HWACCM: AMD-V revision                    = %X\n", pVM->hwaccm.s.svm.u32Rev));
            LogRel(("HWACCM: AMD-V max ASID                    = %d\n", pVM->hwaccm.s.uMaxASID));
            LogRel(("HWACCM: AMD-V features                    = %X\n", pVM->hwaccm.s.svm.u32Features));
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
            uint32_t fSvmFeatures = pVM->hwaccm.s.svm.u32Features;
            for (unsigned i = 0; i < RT_ELEMENTS(s_aSvmFeatures); i++)
                if (fSvmFeatures & s_aSvmFeatures[i].fFlag)
                {
                    LogRel(("HWACCM:    %s\n", s_aSvmFeatures[i].pszName));
                    fSvmFeatures &= ~s_aSvmFeatures[i].fFlag;
                }
            if (fSvmFeatures)
                for (unsigned iBit = 0; iBit < 32; iBit++)
                    if (RT_BIT_32(iBit) & fSvmFeatures)
                        LogRel(("HWACCM:    Reserved bit %u\n", iBit));

            /* Only try once. */
            pVM->hwaccm.s.fInitialized = true;

            if (pVM->hwaccm.s.svm.u32Features & AMD_CPUID_SVM_FEATURE_EDX_NESTED_PAGING)
                pVM->hwaccm.s.fNestedPaging = pVM->hwaccm.s.fAllowNestedPaging;

            rc = SUPR3CallVMMR0Ex(pVM->pVMR0, 0 /*idCpu*/, VMMR0_DO_HWACC_SETUP_VM, 0, NULL);
            AssertRC(rc);
            if (rc == VINF_SUCCESS)
            {
                pVM->fHWACCMEnabled = true;
                pVM->hwaccm.s.svm.fEnabled = true;

                if (pVM->hwaccm.s.fNestedPaging)
                {
                    LogRel(("HWACCM:    Enabled nested paging\n"));
#if HC_ARCH_BITS == 64
                    if (pVM->hwaccm.s.fLargePages)
                    {
                        /* Use large (2 MB) pages for our nested paging PDEs where possible. */
                        PGMSetLargePageUsage(pVM, true);
                        LogRel(("HWACCM:    Large page support enabled!\n"));
                    }
#endif
                }

                hwaccmR3DisableRawMode(pVM);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SEP);
                CPUMSetGuestCpuIdFeature(pVM, CPUMCPUIDFEATURE_SYSCALL);
#ifdef VBOX_ENABLE_64_BITS_GUESTS
                if (pVM->hwaccm.s.fAllow64BitGuests)
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

                LogRel((pVM->hwaccm.s.fAllow64BitGuests
                        ? "HWACCM:    32-bit and 64-bit guest supported.\n"
                        : "HWACCM:    32-bit guest supported.\n"));

                LogRel(("HWACCM:    TPR Patching %s.\n", (pVM->hwaccm.s.fTRPPatchingAllowed) ? "enabled" : "disabled"));
            }
            else
            {
                pVM->fHWACCMEnabled = false;
            }
        }
    }
    if (pVM->fHWACCMEnabled)
        LogRel(("HWACCM:    VT-x/AMD-V init method: %s\n", (pVM->hwaccm.s.fGlobalInit) ? "GLOBAL" : "LOCAL"));
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
VMMR3DECL(void) HWACCMR3Relocate(PVM pVM)
{
    Log(("HWACCMR3Relocate to %RGv\n", MMHyperGetArea(pVM, 0)));

    /* Fetch the current paging mode during the relocate callback during state loading. */
    if (VMR3GetState(pVM) == VMSTATE_LOADING)
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            pVCpu->hwaccm.s.enmShadowMode            = PGMGetShadowMode(pVCpu);
            Assert(pVCpu->hwaccm.s.vmx.enmCurrGuestMode == PGMGetGuestMode(pVCpu));
            pVCpu->hwaccm.s.vmx.enmCurrGuestMode     = PGMGetGuestMode(pVCpu);
        }
    }
#if HC_ARCH_BITS == 32 && defined(VBOX_ENABLE_64_BITS_GUESTS) && !defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
    if (pVM->fHWACCMEnabled)
    {
        int rc;
        switch (PGMGetHostMode(pVM))
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
 * Checks if hardware accelerated raw mode is allowed.
 *
 * @returns true if hardware acceleration is allowed, otherwise false.
 * @param   pVM         Pointer to the VM.
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
 * @param   pVM            Pointer to the VM.
 * @param   pVCpu          Pointer to the VMCPU.
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
               CPL to 0. Our real mode emulation had to set it to 3. */
            pCtx->ss.Attr.n.u2Dpl  = 0;
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
    for (unsigned j = 0; j < pCache->Read.cValidEntries; j++)
        pCache->Read.aFieldVal[j] = 0;
}


/**
 * Terminates the HWACCM.
 *
 * Termination means cleaning up and freeing all resources,
 * the VM itself is, at this point, powered off or suspended.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int) HWACCMR3Term(PVM pVM)
{
    if (pVM->hwaccm.s.vmx.pRealModeTSS)
    {
        PDMR3VMMDevHeapFree(pVM, pVM->hwaccm.s.vmx.pRealModeTSS);
        pVM->hwaccm.s.vmx.pRealModeTSS       = 0;
    }
    hwaccmR3TermCPU(pVM);
    return 0;
}


/**
 * Terminates the per-VCPU HWACCM.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 */
static int hwaccmR3TermCPU(PVM pVM)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i]; NOREF(pVCpu);

#ifdef VBOX_WITH_STATISTICS
        if (pVCpu->hwaccm.s.paStatExitReason)
        {
            MMHyperFree(pVM, pVCpu->hwaccm.s.paStatExitReason);
            pVCpu->hwaccm.s.paStatExitReason   = NULL;
            pVCpu->hwaccm.s.paStatExitReasonR0 = NIL_RTR0PTR;
        }
        if (pVCpu->hwaccm.s.paStatInjectedIrqs)
        {
            MMHyperFree(pVM, pVCpu->hwaccm.s.paStatInjectedIrqs);
            pVCpu->hwaccm.s.paStatInjectedIrqs   = NULL;
            pVCpu->hwaccm.s.paStatInjectedIrqsR0 = NIL_RTR0PTR;
        }
#endif

#ifdef VBOX_WITH_CRASHDUMP_MAGIC
        memset(pVCpu->hwaccm.s.vmx.VMCSCache.aMagic, 0, sizeof(pVCpu->hwaccm.s.vmx.VMCSCache.aMagic));
        pVCpu->hwaccm.s.vmx.VMCSCache.uMagic = 0;
        pVCpu->hwaccm.s.vmx.VMCSCache.uPos = 0xffffffff;
#endif
    }
    return 0;
}


/**
 * Resets a virtual CPU.
 *
 * Used by HWACCMR3Reset and CPU hot plugging.
 *
 * @param   pVCpu   The CPU to reset.
 */
VMMR3DECL(void) HWACCMR3ResetCpu(PVMCPU pVCpu)
{
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
 * For the HWACCM component this means that any GDT/LDT/TSS monitors
 * needs to be removed.
 *
 * @param   pVM     Pointer to the VM.
 */
VMMR3DECL(void) HWACCMR3Reset(PVM pVM)
{
    LogFlow(("HWACCMR3Reset:\n"));

    if (pVM->fHWACCMEnabled)
        hwaccmR3DisableRawMode(pVM);

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        PVMCPU pVCpu = &pVM->aCpus[i];

        HWACCMR3ResetCpu(pVCpu);
    }

    /* Clear all patch information. */
    pVM->hwaccm.s.pGuestPatchMem         = 0;
    pVM->hwaccm.s.pFreeGuestPatchMem     = 0;
    pVM->hwaccm.s.cbGuestPatchMem        = 0;
    pVM->hwaccm.s.cPatches           = 0;
    pVM->hwaccm.s.PatchTree          = 0;
    pVM->hwaccm.s.fTPRPatchingActive = false;
    ASMMemZero32(pVM->hwaccm.s.aPatches, sizeof(pVM->hwaccm.s.aPatches));
}


/**
 * Callback to patch a TPR instruction (vmmcall or mov cr8).
 *
 * @returns VBox strict status code.
 * @param   pVM     Pointer to the VM.
 * @param   pVCpu   The VMCPU for the EMT we're being called on.
 * @param   pvUser  Unused.
 */
DECLCALLBACK(VBOXSTRICTRC) hwaccmR3RemovePatches(PVM pVM, PVMCPU pVCpu, void *pvUser)
{
    VMCPUID idCpu = (VMCPUID)(uintptr_t)pvUser;

    /* Only execute the handler on the VCPU the original patch request was issued. */
    if (pVCpu->idCpu != idCpu)
        return VINF_SUCCESS;

    Log(("hwaccmR3RemovePatches\n"));
    for (unsigned i = 0; i < pVM->hwaccm.s.cPatches; i++)
    {
        uint8_t         abInstr[15];
        PHWACCMTPRPATCH pPatch = &pVM->hwaccm.s.aPatches[i];
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
    pVM->hwaccm.s.cPatches           = 0;
    pVM->hwaccm.s.PatchTree          = 0;
    pVM->hwaccm.s.pFreeGuestPatchMem = pVM->hwaccm.s.pGuestPatchMem;
    pVM->hwaccm.s.fTPRPatchingActive = false;
    return VINF_SUCCESS;
}


/**
 * Worker for enabling patching in a VT-x/AMD-V guest.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   idCpu       VCPU to execute hwaccmR3RemovePatches on.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
static int hwaccmR3EnablePatching(PVM pVM, VMCPUID idCpu, RTRCPTR pPatchMem, unsigned cbPatchMem)
{
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, hwaccmR3RemovePatches, (void *)(uintptr_t)idCpu);
    AssertRC(rc);

    pVM->hwaccm.s.pGuestPatchMem      = pPatchMem;
    pVM->hwaccm.s.pFreeGuestPatchMem  = pPatchMem;
    pVM->hwaccm.s.cbGuestPatchMem     = cbPatchMem;
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
VMMR3DECL(int)  HWACMMR3EnablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem)
{
    VM_ASSERT_EMT(pVM);
    Log(("HWACMMR3EnablePatching %RGv size %x\n", pPatchMem, cbPatchMem));
    if (pVM->cCpus > 1)
    {
        /* We own the IOM lock here and could cause a deadlock by waiting for a VCPU that is blocking on the IOM lock. */
        int rc = VMR3ReqCallNoWait(pVM, VMCPUID_ANY_QUEUE,
                                   (PFNRT)hwaccmR3EnablePatching, 4, pVM, VMMGetCpuId(pVM), (RTRCPTR)pPatchMem, cbPatchMem);
        AssertRC(rc);
        return rc;
    }
    return hwaccmR3EnablePatching(pVM, VMMGetCpuId(pVM), (RTRCPTR)pPatchMem, cbPatchMem);
}


/**
 * Disable patching in a VT-x/AMD-V guest.
 *
 * @returns VBox status code.
 * @param   pVM         Pointer to the VM.
 * @param   pPatchMem   Patch memory range.
 * @param   cbPatchMem  Size of the memory range.
 */
VMMR3DECL(int)  HWACMMR3DisablePatching(PVM pVM, RTGCPTR pPatchMem, unsigned cbPatchMem)
{
    Log(("HWACMMR3DisablePatching %RGv size %x\n", pPatchMem, cbPatchMem));

    Assert(pVM->hwaccm.s.pGuestPatchMem == pPatchMem);
    Assert(pVM->hwaccm.s.cbGuestPatchMem == cbPatchMem);

    /* @todo Potential deadlock when other VCPUs are waiting on the IOM lock (we own it)!! */
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE, hwaccmR3RemovePatches, (void *)(uintptr_t)VMMGetCpuId(pVM));
    AssertRC(rc);

    pVM->hwaccm.s.pGuestPatchMem      = 0;
    pVM->hwaccm.s.pFreeGuestPatchMem  = 0;
    pVM->hwaccm.s.cbGuestPatchMem     = 0;
    pVM->hwaccm.s.fTPRPatchingActive = false;
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
DECLCALLBACK(VBOXSTRICTRC) hwaccmR3ReplaceTprInstr(PVM pVM, PVMCPU pVCpu, void *pvUser)
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
    PHWACCMTPRPATCH pPatch = (PHWACCMTPRPATCH)RTAvloU32Get(&pVM->hwaccm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
    if (pPatch)
    {
        Log(("hwaccmR3ReplaceTprInstr: already patched %RGv\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    uint32_t const  idx = pVM->hwaccm.s.cPatches;
    if (idx >= RT_ELEMENTS(pVM->hwaccm.s.aPatches))
    {
        Log(("hwaccmR3ReplaceTprInstr: no available patch slots (%RGv)\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    pPatch = &pVM->hwaccm.s.aPatches[idx];

    Log(("hwaccmR3ReplaceTprInstr: rip=%RGv idxPatch=%u\n", pCtx->rip, idx));

    /*
     * Disassembler the instruction and get cracking.
     */
    DBGFR3DisasInstrCurrentLog(pVCpu, "hwaccmR3ReplaceTprInstr");
    PDISCPUSTATE    pDis = &pVCpu->hwaccm.s.DisState;
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
                pPatch->enmType     = HWACCMTPRINSTR_WRITE_REG;
                pPatch->uSrcOperand = pDis->Param2.Base.idxGenReg;
                Log(("hwaccmR3ReplaceTprInstr: HWACCMTPRINSTR_WRITE_REG %u\n", pDis->Param2.Base.idxGenReg));
            }
            else
            {
                Assert(pDis->Param2.fUse == DISUSE_IMMEDIATE32);
                pPatch->enmType     = HWACCMTPRINSTR_WRITE_IMM;
                pPatch->uSrcOperand = pDis->Param2.uValue;
                Log(("hwaccmR3ReplaceTprInstr: HWACCMTPRINSTR_WRITE_IMM %#llx\n", pDis->Param2.uValue));
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
                &&  cbOpMmio + cbOp < sizeof(pVM->hwaccm.s.aPatches[idx].aOpcode))
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
                pPatch->enmType = HWACCMTPRINSTR_READ_SHR4;
            }
            else
            {
                pPatch->enmType     = HWACCMTPRINSTR_READ;
                pPatch->uDstOperand = idxMmioReg;

                rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->rip, s_abVMMCall, sizeof(s_abVMMCall));
                AssertRC(rc);

                memcpy(pPatch->aNewOpcode, s_abVMMCall, sizeof(s_abVMMCall));
                pPatch->cbNewOp = sizeof(s_abVMMCall);
                Log(("hwaccmR3ReplaceTprInstr: HWACCMTPRINSTR_READ %u\n", pPatch->uDstOperand));
            }
        }

        pPatch->Core.Key = pCtx->eip;
        rc = RTAvloU32Insert(&pVM->hwaccm.s.PatchTree, &pPatch->Core);
        AssertRC(rc);

        pVM->hwaccm.s.cPatches++;
        STAM_COUNTER_INC(&pVM->hwaccm.s.StatTPRReplaceSuccess);
        return VINF_SUCCESS;
    }

    /*
     * Save invalid patch, so we will not try again.
     */
    Log(("hwaccmR3ReplaceTprInstr: Failed to patch instr!\n"));
    pPatch->Core.Key = pCtx->eip;
    pPatch->enmType  = HWACCMTPRINSTR_INVALID;
    rc = RTAvloU32Insert(&pVM->hwaccm.s.PatchTree, &pPatch->Core);
    AssertRC(rc);
    pVM->hwaccm.s.cPatches++;
    STAM_COUNTER_INC(&pVM->hwaccm.s.StatTPRReplaceFailure);
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
DECLCALLBACK(VBOXSTRICTRC) hwaccmR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, void *pvUser)
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
    PHWACCMTPRPATCH pPatch = (PHWACCMTPRPATCH)RTAvloU32Get(&pVM->hwaccm.s.PatchTree, (AVLOU32KEY)pCtx->eip);
    if (pPatch)
    {
        Log(("hwaccmR3PatchTprInstr: already patched %RGv\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    uint32_t const  idx = pVM->hwaccm.s.cPatches;
    if (idx >= RT_ELEMENTS(pVM->hwaccm.s.aPatches))
    {
        Log(("hwaccmR3PatchTprInstr: no available patch slots (%RGv)\n", pCtx->rip));
        return VINF_SUCCESS;
    }
    pPatch = &pVM->hwaccm.s.aPatches[idx];

    Log(("hwaccmR3PatchTprInstr: rip=%RGv idxPatch=%u\n", pCtx->rip, idx));
    DBGFR3DisasInstrCurrentLog(pVCpu, "hwaccmR3PatchTprInstr");

    /*
     * Disassemble the instruction and get cracking.
     */
    PDISCPUSTATE    pDis   = &pVCpu->hwaccm.s.DisState;
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
        pPatch->enmType = HWACCMTPRINSTR_JUMP_REPLACEMENT;

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
        *(RTRCUINTPTR *)&aPatch[off] = ((RTRCUINTPTR)pCtx->eip + cbOp) - ((RTRCUINTPTR)pVM->hwaccm.s.pFreeGuestPatchMem + off + 4);
        off += sizeof(RTRCUINTPTR);

        if (pVM->hwaccm.s.pFreeGuestPatchMem + off <= pVM->hwaccm.s.pGuestPatchMem + pVM->hwaccm.s.cbGuestPatchMem)
        {
            /* Write new code to the patch buffer. */
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pVM->hwaccm.s.pFreeGuestPatchMem, aPatch, off);
            AssertRC(rc);

#ifdef LOG_ENABLED
            uint32_t cbCurInstr;
            for (RTGCPTR GCPtrInstr = pVM->hwaccm.s.pFreeGuestPatchMem;
                 GCPtrInstr < pVM->hwaccm.s.pFreeGuestPatchMem + off;
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
            *(RTRCUINTPTR *)&pPatch->aNewOpcode[1] = ((RTRCUINTPTR)pVM->hwaccm.s.pFreeGuestPatchMem) - ((RTRCUINTPTR)pCtx->eip + 5);

            /* Overwrite the TPR instruction with a jump. */
            rc = PGMPhysSimpleWriteGCPtr(pVCpu, pCtx->eip, pPatch->aNewOpcode, 5);
            AssertRC(rc);

            DBGFR3DisasInstrCurrentLog(pVCpu, "Jump");

            pVM->hwaccm.s.pFreeGuestPatchMem += off;
            pPatch->cbNewOp = 5;

            pPatch->Core.Key = pCtx->eip;
            rc = RTAvloU32Insert(&pVM->hwaccm.s.PatchTree, &pPatch->Core);
            AssertRC(rc);

            pVM->hwaccm.s.cPatches++;
            pVM->hwaccm.s.fTPRPatchingActive = true;
            STAM_COUNTER_INC(&pVM->hwaccm.s.StatTPRPatchSuccess);
            return VINF_SUCCESS;
        }

        Log(("Ran out of space in our patch buffer!\n"));
    }
    else
        Log(("hwaccmR3PatchTprInstr: Failed to patch instr!\n"));


    /*
     * Save invalid patch, so we will not try again.
     */
    pPatch = &pVM->hwaccm.s.aPatches[idx];
    pPatch->Core.Key = pCtx->eip;
    pPatch->enmType  = HWACCMTPRINSTR_INVALID;
    rc = RTAvloU32Insert(&pVM->hwaccm.s.PatchTree, &pPatch->Core);
    AssertRC(rc);
    pVM->hwaccm.s.cPatches++;
    STAM_COUNTER_INC(&pVM->hwaccm.s.StatTPRPatchFailure);
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
VMMR3DECL(int) HWACCMR3PatchTprInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    NOREF(pCtx);
    int rc = VMMR3EmtRendezvous(pVM, VMMEMTRENDEZVOUS_FLAGS_TYPE_ONE_BY_ONE,
                                pVM->hwaccm.s.pGuestPatchMem ? hwaccmR3PatchTprInstr : hwaccmR3ReplaceTprInstr,
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
 * @returns true if we can currently use hardware acceleration, otherwise false.
 * @param   pVM         Pointer to the VM.
 * @param   pCtx        Partial VM execution context.
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
    Assert((pVM->hwaccm.s.vmx.fUnrestrictedGuest && !pVM->hwaccm.s.vmx.pRealModeTSS) || (!pVM->hwaccm.s.vmx.fUnrestrictedGuest && pVM->hwaccm.s.vmx.pRealModeTSS));

    bool fSupportsRealMode = pVM->hwaccm.s.vmx.fUnrestrictedGuest || PDMVMMDevHeapIsEnabled(pVM);
    if (!pVM->hwaccm.s.vmx.fUnrestrictedGuest)
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
                if (    pVCpu->hwaccm.s.vmx.enmLastSeenGuestMode == PGMMODE_REAL
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
                &&  !pVM->hwaccm.s.vmx.fUnrestrictedGuest)
            {
                /** @todo   This should (probably) be set on every excursion to the REM,
                 *          however it's too risky right now. So, only apply it when we go
                 *          back to REM for real mode execution. (The XP hack below doesn't
                 *          work reliably without this.)
                 *  Update: Implemented in EM.cpp, see #ifdef EM_NOTIFY_HWACCM.  */
                pVM->aCpus[0].hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL_GUEST;

                if (    !pVM->hwaccm.s.fNestedPaging        /* requires a fake PD for real *and* protected mode without paging - stored in the VMM device heap*/
                    ||  CPUMIsGuestInRealModeEx(pCtx))      /* requires a fake TSS for real mode - stored in the VMM device heap */
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

    if (pVM->hwaccm.s.vmx.fEnabled)
    {
        uint32_t mask;

        /* if bit N is set in cr0_fixed0, then it must be set in the guest's cr0. */
        mask = (uint32_t)pVM->hwaccm.s.vmx.msr.vmx_cr0_fixed0;
        /* Note: We ignore the NE bit here on purpose; see vmmr0\hwaccmr0.cpp for details. */
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
 * Checks if we need to reschedule due to VMM device heap changes.
 *
 * @returns true if a reschedule is required, otherwise false.
 * @param   pVM         Pointer to the VM.
 * @param   pCtx        VM execution context.
 */
VMMR3DECL(bool) HWACCMR3IsRescheduleRequired(PVM pVM, PCPUMCTX pCtx)
{
    /*
     * The VMM device heap is a requirement for emulating real mode or protected mode without paging
     * when the unrestricted guest execution feature is missing (VT-x only).
     */
    if (    pVM->hwaccm.s.vmx.fEnabled
        &&  !pVM->hwaccm.s.vmx.fUnrestrictedGuest
        &&  !CPUMIsGuestInPagedProtectedModeEx(pCtx)
        &&  !PDMVMMDevHeapIsEnabled(pVM)
        &&  (pVM->hwaccm.s.fNestedPaging || CPUMIsGuestInRealModeEx(pCtx)))
        return true;

    return false;
}


/**
 * Notification from EM about a rescheduling into hardware assisted execution
 * mode.
 *
 * @param   pVCpu       Pointer to the current VMCPU.
 */
VMMR3DECL(void) HWACCMR3NotifyScheduled(PVMCPU pVCpu)
{
    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL_GUEST;
}


/**
 * Notification from EM about returning from instruction emulation (REM / EM).
 *
 * @param   pVCpu       Pointer to the VMCPU.
 */
VMMR3DECL(void) HWACCMR3NotifyEmulated(PVMCPU pVCpu)
{
    pVCpu->hwaccm.s.fContextUseFlags |= HWACCM_CHANGED_ALL_GUEST;
}


/**
 * Checks if we are currently using hardware accelerated raw mode.
 *
 * @returns true if hardware acceleration is being used, otherwise false.
 * @param   pVCpu        Pointer to the VMCPU.
 */
VMMR3DECL(bool) HWACCMR3IsActive(PVMCPU pVCpu)
{
    return pVCpu->hwaccm.s.fActive;
}


/**
 * Checks if we are currently using nested paging.
 *
 * @returns true if nested paging is being used, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HWACCMR3IsNestedPagingActive(PVM pVM)
{
    return pVM->hwaccm.s.fNestedPaging;
}


/**
 * Checks if we are currently using VPID in VT-x mode.
 *
 * @returns true if VPID is being used, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HWACCMR3IsVPIDActive(PVM pVM)
{
    return pVM->hwaccm.s.vmx.fVPID;
}


/**
 * Checks if internal events are pending. In that case we are not allowed to dispatch interrupts.
 *
 * @returns true if an internal event is pending, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HWACCMR3IsEventPending(PVMCPU pVCpu)
{
    return HWACCMIsEnabled(pVCpu->pVMR3) && pVCpu->hwaccm.s.Event.fPending;
}


/**
 * Checks if the VMX-preemption timer is being used.
 *
 * @returns true if the VMX-preemption timer is being used, otherwise false.
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(bool) HWACCMR3IsVmxPreemptionTimerUsed(PVM pVM)
{
    return HWACCMIsEnabled(pVM)
        && pVM->hwaccm.s.vmx.fEnabled
        && pVM->hwaccm.s.vmx.fUsePreemptTimer;
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
VMMR3DECL(VBOXSTRICTRC) HWACCMR3RestartPendingIOInstr(PVM pVM, PVMCPU pVCpu, PCPUMCTX pCtx)
{
    HWACCMPENDINGIO enmType = pVCpu->hwaccm.s.PendingIO.enmType;

    pVCpu->hwaccm.s.PendingIO.enmType = HWACCMPENDINGIO_INVALID;

    if (    pVCpu->hwaccm.s.PendingIO.GCPtrRip != pCtx->rip
        ||  enmType  == HWACCMPENDINGIO_INVALID)
        return VERR_NOT_FOUND;

    VBOXSTRICTRC rcStrict;
    switch (enmType)
    {
        case HWACCMPENDINGIO_PORT_READ:
        {
            uint32_t uAndVal = pVCpu->hwaccm.s.PendingIO.s.Port.uAndVal;
            uint32_t u32Val  = 0;

            rcStrict = IOMIOPortRead(pVM, pVCpu->hwaccm.s.PendingIO.s.Port.uPort,
                                     &u32Val,
                                     pVCpu->hwaccm.s.PendingIO.s.Port.cbSize);
            if (IOM_SUCCESS(rcStrict))
            {
                /* Write back to the EAX register. */
                pCtx->eax = (pCtx->eax & ~uAndVal) | (u32Val & uAndVal);
                pCtx->rip = pVCpu->hwaccm.s.PendingIO.GCPtrRipNext;
            }
            break;
        }

        case HWACCMPENDINGIO_PORT_WRITE:
            rcStrict = IOMIOPortWrite(pVM, pVCpu->hwaccm.s.PendingIO.s.Port.uPort,
                                      pCtx->eax & pVCpu->hwaccm.s.PendingIO.s.Port.uAndVal,
                                      pVCpu->hwaccm.s.PendingIO.s.Port.cbSize);
            if (IOM_SUCCESS(rcStrict))
                pCtx->rip = pVCpu->hwaccm.s.PendingIO.GCPtrRipNext;
            break;

        default:
            AssertLogRelFailedReturn(VERR_HM_UNKNOWN_IO_INSTRUCTION);
    }

    return rcStrict;
}


/**
 * Inject an NMI into a running VM (only VCPU 0!)
 *
 * @returns boolean
 * @param   pVM         Pointer to the VM.
 */
VMMR3DECL(int)  HWACCMR3InjectNMI(PVM pVM)
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
VMMR3DECL(void) HWACCMR3CheckError(PVM pVM, int iStatusCode)
{
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        switch (iStatusCode)
        {
            case VERR_VMX_INVALID_VMCS_FIELD:
                break;

            case VERR_VMX_INVALID_VMCS_PTR:
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current pointer %RGp vs %RGp\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.u64VMCSPhys, pVM->aCpus[i].hwaccm.s.vmx.HCPhysVMCS));
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current VMCS version %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulVMCSRevision));
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Entered Cpu %d\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.idEnteredCpu));
                LogRel(("VERR_VMX_INVALID_VMCS_PTR: CPU%d Current Cpu %d\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.idCurrentCpu));
                break;

            case VERR_VMX_UNABLE_TO_START_VM:
                LogRel(("VERR_VMX_UNABLE_TO_START_VM: CPU%d instruction error %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulInstrError));
                LogRel(("VERR_VMX_UNABLE_TO_START_VM: CPU%d exit reason       %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulExitReason));
                if (pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulInstrError == VMX_ERROR_VMENTRY_INVALID_CONTROL_FIELDS)
                {
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d MSRBitmapPhys %RHp\n", i, pVM->aCpus[i].hwaccm.s.vmx.pMSRBitmapPhys));
#ifdef VBOX_WITH_AUTO_MSR_LOAD_RESTORE
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d GuestMSRPhys  %RHp\n", i, pVM->aCpus[i].hwaccm.s.vmx.pGuestMSRPhys));
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d HostMsrPhys   %RHp\n", i, pVM->aCpus[i].hwaccm.s.vmx.pHostMSRPhys));
                    LogRel(("VERR_VMX_UNABLE_TO_START_VM: Cpu%d HostMsrPhys   %x\n",   i, pVM->aCpus[i].hwaccm.s.vmx.cCachedMSRs));
#endif
                }
                /** @todo Log VM-entry event injection control fields
                 *        VMX_VMCS_CTRL_ENTRY_IRQ_INFO, VMX_VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE
                 *        and VMX_VMCS_CTRL_ENTRY_INSTR_LENGTH from the VMCS. */
                break;

            case VERR_VMX_UNABLE_TO_RESUME_VM:
                LogRel(("VERR_VMX_UNABLE_TO_RESUME_VM: CPU%d instruction error %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulInstrError));
                LogRel(("VERR_VMX_UNABLE_TO_RESUME_VM: CPU%d exit reason       %x\n", i, pVM->aCpus[i].hwaccm.s.vmx.lasterror.ulExitReason));
                break;

            case VERR_VMX_INVALID_VMXON_PTR:
                break;
        }
    }

    if (iStatusCode == VERR_VMX_UNABLE_TO_START_VM)
    {
        LogRel(("VERR_VMX_UNABLE_TO_START_VM: VM-entry allowed    %x\n", pVM->hwaccm.s.vmx.msr.vmx_entry.n.allowed1));
        LogRel(("VERR_VMX_UNABLE_TO_START_VM: VM-entry disallowed %x\n", pVM->hwaccm.s.vmx.msr.vmx_entry.n.disallowed0));
    }
}


/**
 * Execute state save operation.
 *
 * @returns VBox status code.
 * @param   pVM             Pointer to the VM.
 * @param   pSSM            SSM operation handle.
 */
static DECLCALLBACK(int) hwaccmR3Save(PVM pVM, PSSMHANDLE pSSM)
{
    int rc;

    Log(("hwaccmR3Save:\n"));

    for (VMCPUID i = 0; i < pVM->cCpus; i++)
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
#ifdef VBOX_HWACCM_WITH_GUEST_PATCHING
    rc = SSMR3PutGCPtr(pSSM, pVM->hwaccm.s.pGuestPatchMem);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutGCPtr(pSSM, pVM->hwaccm.s.pFreeGuestPatchMem);
    AssertRCReturn(rc, rc);
    rc = SSMR3PutU32(pSSM, pVM->hwaccm.s.cbGuestPatchMem);
    AssertRCReturn(rc, rc);

    /* Store all the guest patch records too. */
    rc = SSMR3PutU32(pSSM, pVM->hwaccm.s.cPatches);
    AssertRCReturn(rc, rc);

    for (unsigned i = 0; i < pVM->hwaccm.s.cPatches; i++)
    {
        PHWACCMTPRPATCH pPatch = &pVM->hwaccm.s.aPatches[i];

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

        AssertCompileSize(HWACCMTPRINSTR, 4);
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
static DECLCALLBACK(int) hwaccmR3Load(PVM pVM, PSSMHANDLE pSSM, uint32_t uVersion, uint32_t uPass)
{
    int rc;

    Log(("hwaccmR3Load:\n"));
    Assert(uPass == SSM_PASS_FINAL); NOREF(uPass);

    /*
     * Validate version.
     */
    if (   uVersion != HWACCM_SSM_VERSION
        && uVersion != HWACCM_SSM_VERSION_NO_PATCHING
        && uVersion != HWACCM_SSM_VERSION_2_0_X)
    {
        AssertMsgFailed(("hwaccmR3Load: Invalid version uVersion=%d!\n", uVersion));
        return VERR_SSM_UNSUPPORTED_DATA_UNIT_VERSION;
    }
    for (VMCPUID i = 0; i < pVM->cCpus; i++)
    {
        rc = SSMR3GetU32(pSSM, &pVM->aCpus[i].hwaccm.s.Event.fPending);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &pVM->aCpus[i].hwaccm.s.Event.errCode);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU64(pSSM, &pVM->aCpus[i].hwaccm.s.Event.intInfo);
        AssertRCReturn(rc, rc);

        if (uVersion >= HWACCM_SSM_VERSION_NO_PATCHING)
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
#ifdef VBOX_HWACCM_WITH_GUEST_PATCHING
    if (uVersion > HWACCM_SSM_VERSION_NO_PATCHING)
    {
        rc = SSMR3GetGCPtr(pSSM, &pVM->hwaccm.s.pGuestPatchMem);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetGCPtr(pSSM, &pVM->hwaccm.s.pFreeGuestPatchMem);
        AssertRCReturn(rc, rc);
        rc = SSMR3GetU32(pSSM, &pVM->hwaccm.s.cbGuestPatchMem);
        AssertRCReturn(rc, rc);

        /* Fetch all TPR patch records. */
        rc = SSMR3GetU32(pSSM, &pVM->hwaccm.s.cPatches);
        AssertRCReturn(rc, rc);

        for (unsigned i = 0; i < pVM->hwaccm.s.cPatches; i++)
        {
            PHWACCMTPRPATCH pPatch = &pVM->hwaccm.s.aPatches[i];

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

            if (pPatch->enmType == HWACCMTPRINSTR_JUMP_REPLACEMENT)
                pVM->hwaccm.s.fTPRPatchingActive = true;

            Assert(pPatch->enmType == HWACCMTPRINSTR_JUMP_REPLACEMENT || pVM->hwaccm.s.fTPRPatchingActive == false);

            rc = SSMR3GetU32(pSSM, &pPatch->uSrcOperand);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->uDstOperand);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->cFaults);
            AssertRCReturn(rc, rc);

            rc = SSMR3GetU32(pSSM, &pPatch->pJumpTarget);
            AssertRCReturn(rc, rc);

            Log(("hwaccmR3Load: patch %d\n", i));
            Log(("Key       = %x\n", pPatch->Core.Key));
            Log(("cbOp      = %d\n", pPatch->cbOp));
            Log(("cbNewOp   = %d\n", pPatch->cbNewOp));
            Log(("type      = %d\n", pPatch->enmType));
            Log(("srcop     = %d\n", pPatch->uSrcOperand));
            Log(("dstop     = %d\n", pPatch->uDstOperand));
            Log(("cFaults   = %d\n", pPatch->cFaults));
            Log(("target    = %x\n", pPatch->pJumpTarget));
            rc = RTAvloU32Insert(&pVM->hwaccm.s.PatchTree, &pPatch->Core);
            AssertRC(rc);
        }
    }
#endif

    /* Recheck all VCPUs if we can go straight into hwaccm execution mode. */
    if (HWACCMIsEnabled(pVM))
    {
        for (VMCPUID i = 0; i < pVM->cCpus; i++)
        {
            PVMCPU pVCpu = &pVM->aCpus[i];

            HWACCMR3CanExecuteGuest(pVM, CPUMQueryGuestCtxPtr(pVCpu));
        }
    }
    return VINF_SUCCESS;
}

