/** @file
 * HWACC/VMX - VMX Structures and Definitions.
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
 *
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 */

#ifndef ___VBox_vmx_h
#define ___VBox_vmx_h

#include <VBox/types.h>
#include <VBox/err.h>
#include <iprt/assert.h>
#include <iprt/asm.h>

/** @defgroup grp_vmx   vmx Types and Definitions
 * @ingroup grp_hwaccm
 * @{
 */

/** VMX Basic Exit Reasons.
 * @{
 */
/* And-mask for setting reserved bits to zero */
#define VMX_EFLAGS_RESERVED_0           (~0xffc08028)
/* Or-mask for setting reserved bits to 1 */
#define VMX_EFLAGS_RESERVED_1           0x00000002
/** @} */

/** VMX Basic Exit Reasons.
 * @{
 */
/** 0 Exception or non-maskable interrupt (NMI). */
#define VMX_EXIT_EXCEPTION          0
/** 1 External interrupt. */
#define VMX_EXIT_EXTERNAL_IRQ       1
/** 2 Triple fault. */
#define VMX_EXIT_TRIPLE_FAULT       2
/** 3 INIT signal. */
#define VMX_EXIT_INIT_SIGNAL        3
/** 4 Start-up IPI (SIPI). */
#define VMX_EXIT_SIPI               4
/** 5 I/O system-management interrupt (SMI). */
#define VMX_EXIT_IO_SMI_IRQ         5
/** 6 Other SMI. */
#define VMX_EXIT_SMI_IRQ            6
/** 7 Interrupt window. */
#define VMX_EXIT_IRQ_WINDOW         7
/** 9 Task switch. */
#define VMX_EXIT_TASK_SWITCH        9
/** 10 Guest software attempted to execute CPUID. */
#define VMX_EXIT_CPUID              10
/** 12 Guest software attempted to execute HLT. */
#define VMX_EXIT_HLT                12
/** 13 Guest software attempted to execute INVD. */
#define VMX_EXIT_INVD               13
/** 14 Guest software attempted to execute INVPG. */
#define VMX_EXIT_INVPG              14
/** 15 Guest software attempted to execute RDPMC. */
#define VMX_EXIT_RDPMC              15
/** 16 Guest software attempted to execute RDTSC. */
#define VMX_EXIT_RDTSC              16
/** 17 Guest software attempted to execute RSM in SMM. */
#define VMX_EXIT_RSM                17
/** 18 Guest software executed VMCALL. */
#define VMX_EXIT_VMCALL             18
/** 19 Guest software executed VMCLEAR. */
#define VMX_EXIT_VMCLEAR            19
/** 20 Guest software executed VMLAUNCH. */
#define VMX_EXIT_VMLAUNCH           20
/** 21 Guest software executed VMPTRLD. */
#define VMX_EXIT_VMPTRLD            21
/** 22 Guest software executed VMPTRST. */
#define VMX_EXIT_VMPTRST            22
/** 23 Guest software executed VMREAD. */
#define VMX_EXIT_VMREAD             23
/** 24 Guest software executed VMRESUME. */
#define VMX_EXIT_VMRESUME           24
/** 25 Guest software executed VMWRITE. */
#define VMX_EXIT_VMWRITE            25
/** 26 Guest software executed VMXOFF. */
#define VMX_EXIT_VMXOFF             26
/** 27 Guest software executed VMXON. */
#define VMX_EXIT_VMXON              27
/** 28 Control-register accesses. */
#define VMX_EXIT_CRX_MOVE           28
/** 29 Debug-register accesses. */
#define VMX_EXIT_DRX_MOVE           29
/** 30 I/O instruction. */
#define VMX_EXIT_PORT_IO            30
/** 31 RDMSR. Guest software attempted to execute RDMSR. */
#define VMX_EXIT_RDMSR              31
/** 32 WRMSR. Guest software attempted to execute WRMSR. */
#define VMX_EXIT_WRMSR              32
/** 33 VM-entry failure due to invalid guest state. */
#define VMX_EXIT_ERR_INVALID_GUEST_STATE    33
/** 34 VM-entry failure due to MSR loading. */
#define VMX_EXIT_ERR_MSR_LOAD       34
/** 36 Guest software executed MWAIT. */
#define VMX_EXIT_MWAIT              36
/** 39 Guest software attempted to execute MONITOR. */
#define VMX_EXIT_MONITOR            39
/** 40 Guest software attempted to execute PAUSE. */
#define VMX_EXIT_PAUSE              40
/** 41 VM-entry failure due to machine-check. */
#define VMX_EXIT_ERR_MACHINE_CHECK  41
/** 43 TPR below threshold. Guest software executed MOV to CR8. */
#define VMX_EXIT_TPR                43

/** @} */


/** VM Instruction Errors
 * @{
 */
/** 1 VMCALL executed in VMX root operation. */
#define VMX_ERROR_VMCALL                            1
/** 2 VMCLEAR with invalid physical address. */
#define VMX_ERROR_VMCLEAR_INVALID_PHYS_ADDR         2
/** 3 VMCLEAR with VMXON pointer. */
#define VMX_ERROR_VMCLEAR_INVALID_VMXON_PTR         3
/** 4 VMLAUNCH with non-clear VMCS. */
#define VMX_ERROR_VMLAUCH_NON_CLEAR_VMCS            4
/** 5 VMRESUME with non-launched VMCS. */
#define VMX_ERROR_VMRESUME_NON_LAUNCHED_VMCS        5
/** 6 VMRESUME with a corrupted VMCS (indicates corruption of the current VMCS). */
#define VMX_ERROR_VMRESUME_CORRUPTED_VMCS           6
/** 7 VM entry with invalid control field(s). */
#define VMX_ERROR_VMENTRY_INVALID_CONTROL_FIELDS    7
/** 8 VM entry with invalid host-state field(s). */
#define VMX_ERROR_VMENTRY_INVALID_HOST_STATE        8
/** 9 VMPTRLD with invalid physical address. */
#define VMX_ERROR_VMPTRLD_INVALID_PHYS_ADDR         9
/** 10 VMPTRLD with VMXON pointer. */
#define VMX_ERROR_VMPTRLD_VMXON_PTR                 10
/** 11 VMPTRLD with incorrect VMCS revision identifier. */
#define VMX_ERROR_VMPTRLD_WRONG_VMCS_REVISION       11
/** 12 VMREAD/VMWRITE from/to unsupported VMCS component. */
#define VMX_ERROR_VMREAD_INVALID_COMPONENT          12
#define VMX_ERROR_VMWRITE_INVALID_COMPONENT         VMX_ERROR_VMREAD_INVALID_COMPONENT
/** 13 VMWRITE to read-only VMCS component. */
#define VMX_ERROR_VMWRITE_READONLY_COMPONENT        13
/** 15 VMXON executed in VMX root operation. */
#define VMX_ERROR_VMXON_IN_VMX_ROOT_OP              15
/** 16 VM entry with invalid executive-VMCS pointer. */
#define VMX_ERROR_VMENTRY_INVALID_VMCS_EXEC_PTR     16
/** 17 VM entry with non-launched executive VMCS. */
#define VMX_ERROR_VMENTRY_NON_LAUNCHED_EXEC_VMCS    17
/** 18 VM entry with executive-VMCS pointer not VMXON pointer. */
#define VMX_ERROR_VMENTRY_EXEC_VMCS_PTR             18
/** 19 VMCALL with non-clear VMCS. */
#define VMX_ERROR_VMCALL_NON_CLEAR_VMCS             19
/** 20 VMCALL with invalid VM-exit control fields. */
#define VMX_ERROR_VMCALL_INVALID_VMEXIT_FIELDS      20
/** 22 VMCALL with incorrect MSEG revision identifier. */
#define VMX_ERROR_VMCALL_INVALID_MSEG_REVISION      22
/** 23 VMXOFF under dual-monitor treatment of SMIs and SMM. */
#define VMX_ERROR_VMXOFF_DUAL_MONITOR               23
/** 24 VMCALL with invalid SMM-monitor features. */
#define VMX_ERROR_VMCALL_INVALID_SMM_MONITOR        24
/** 25 VM entry with invalid VM-execution control fields in executive VMCS. */
#define VMX_ERROR_VMENTRY_INVALID_VM_EXEC_CTRL      25
/** 26 VM entry with events blocked by MOV SS. */
#define VMX_ERROR_VMENTRY_MOV_SS                    26

/** @} */


/** VMX MSR bit definitions
 * @{
 */

/** Basic VMX information.
 * @{
 */
/** VMCS revision identifier used by the processor. */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_ID(a)                      (a & 0x7FFFFFFF)
/** Size of the VMCS. */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(a)                    ((a >> 32ULL) & 0xFFF)
/** Width of physical address used for the VMCS.
 *  0 -> limited to the available amount of physical ram
 *  1 -> within the first 4 GB
 */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_PHYS_WIDTH(a)              ((a >> 48ULL) & 1)
/** Whether the processor supports the dual-monitor treatment of system-management interrupts and system-management code. (always 1) */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_DUAL_MON(a)                ((a >> 49ULL) & 1)
/** Memory type that must be used for the VMCS. */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_MEM_TYPE(a)                ((a >> 50ULL) & 0xF)
/** @} */


/** Misc VMX info.
 * @{
 */
/** Activity states supported by the implementation. */
#define MSR_IA32_VMX_MISC_ACTIVITY_STATES(a)                    ((a >> 6ULL) & 0x7)
/** Number of CR3 target values supported by the processor. (0-256) */
#define MSR_IA32_VMX_MISC_CR3_TARGET(a)                         ((a >> 16ULL) & 0x1FF)
/** Maximum nr of MSRs in the VMCS. (N+1)*512. */
#define MSR_IA32_VMX_MISC_MAX_MSR(a)                            ((((a >> 25ULL) & 0x7) + 1) * 512)
/** MSEG revision identifier used by the processor. */
#define MSR_IA32_VMX_MISC_MSEG_ID(a)                            (a >> 32ULL)
/** @} */


/** VMCS enumeration field info
 * @{
 */
/** Highest field index. */
#define MSR_IA32_VMX_VMCS_ENUM_HIGHEST_INDEX(a)                 ((a >> 1ULL) & 0x1FF)

/** @} */

/** @} */


/** VMCS field encoding
 * @{
 */

/* 16 bits guest fields
 * @{
 */
#define VMX_VMCS_GUEST_FIELD_ES                                 0x800
#define VMX_VMCS_GUEST_FIELD_CS                                 0x802
#define VMX_VMCS_GUEST_FIELD_SS                                 0x804
#define VMX_VMCS_GUEST_FIELD_DS                                 0x806
#define VMX_VMCS_GUEST_FIELD_FS                                 0x808
#define VMX_VMCS_GUEST_FIELD_GS                                 0x80A
#define VMX_VMCS_GUEST_FIELD_LDTR                               0x80C
#define VMX_VMCS_GUEST_FIELD_TR                                 0x80E
/** @} */

/** 16 bits host fields
 * @{
 */
#define VMX_VMCS_HOST_FIELD_ES                                  0xC00
#define VMX_VMCS_HOST_FIELD_CS                                  0xC02
#define VMX_VMCS_HOST_FIELD_SS                                  0xC04
#define VMX_VMCS_HOST_FIELD_DS                                  0xC06
#define VMX_VMCS_HOST_FIELD_FS                                  0xC08
#define VMX_VMCS_HOST_FIELD_GS                                  0xC0A
#define VMX_VMCS_HOST_FIELD_TR                                  0xC0C
/** @}          */

/** 64 Bits control fields
 * @{
 */
#define VMX_VMCS_CTRL_IO_BITMAP_A_FULL                          0x2000
#define VMX_VMCS_CTRL_IO_BITMAP_A_HIGH                          0x2001
#define VMX_VMCS_CTRL_IO_BITMAP_B_FULL                          0x2002
#define VMX_VMCS_CTRL_IO_BITMAP_B_HIGH                          0x2003

/* Optional */
#define VMX_VMCS_CTRL_MSR_BITMAP_FULL                           0x2004
#define VMX_VMCS_CTRL_MSR_BITMAP_HIGH                           0x2005

#define VMX_VMCS_CTRL_VMEXIT_MSR_STORE_FULL                     0x2006
#define VMX_VMCS_CTRL_VMEXIT_MSR_STORE_HIGH                     0x2007
#define VMX_VMCS_CTRL_VMEXIT_MSR_LOAD_FULL                      0x2008
#define VMX_VMCS_CTRL_VMEXIT_MSR_LOAD_HIGH                      0x2009

#define VMX_VMCS_CTRL_VMENTRY_MSR_LOAD_FULL                     0x200A
#define VMX_VMCS_CTRL_VMENTRY_MSR_LOAD_HIGH                     0x200B

#define VMX_VMCS_CTRL_EXEC_VMCS_PTR_FULL                        0x200C
#define VMX_VMCS_CTRL_EXEC_VMCS_PTR_HIGH                        0x200D

#define VMX_VMCS_CTRL_TSC_OFFSET_FULL                           0x2010
#define VMX_VMCS_CTRL_TSC_OFFSET_HIGH                           0x2011

/* Optional */
#define VMX_VMCS_CTRL_VAPIC_PAGEADDR_FULL                       0x2012
#define VMX_VMCS_CTRL_VAPIC_PAGEADDR_HIGH                       0x2013
/** @} */


/** 64 Bits guest fields
 * @{
 */
#define VMX_VMCS_GUEST_LINK_PTR_FULL                            0x2800
#define VMX_VMCS_GUEST_LINK_PTR_HIGH                            0x2801
#define VMX_VMCS_GUEST_DEBUGCTL_FULL                            0x2802   /* MSR IA32_DEBUGCTL */
#define VMX_VMCS_GUEST_DEBUGCTL_HIGH                            0x2803   /* MSR IA32_DEBUGCTL */
/** @} */


/** 32 Bits control fields
 * @{
 */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS                         0x4000
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS                        0x4002
#define VMX_VMCS_CTRL_EXCEPTION_BITMAP                          0x4004
#define VMX_VMCS_CTRL_PAGEFAULT_ERROR_MASK                      0x4006
#define VMX_VMCS_CTRL_PAGEFAULT_ERROR_MATCH                     0x4008
#define VMX_VMCS_CTRL_CR3_TARGET_COUNT                          0x400A
#define VMX_VMCS_CTRL_EXIT_CONTROLS                             0x400C
#define VMX_VMCS_CTRL_EXIT_MSR_STORE_COUNT                      0x400E
#define VMX_VMCS_CTRL_EXIT_MSR_LOAD_COUNT                       0x4010
#define VMX_VMCS_CTRL_ENTRY_CONTROLS                            0x4012
#define VMX_VMCS_CTRL_ENTRY_MSR_LOAD_COUNT                      0x4014
#define VMX_VMCS_CTRL_ENTRY_IRQ_INFO                            0x4016
#define VMX_VMCS_CTRL_ENTRY_EXCEPTION_ERRCODE                   0x4018
#define VMX_VMCS_CTRL_ENTRY_INSTR_LENGTH                        0x401A
/* Optional */
#define VMX_VMCS_CTRL_TPR_TRESHOLD                              0x401C
/** @} */


/** VMX_VMCS_CTRL_PIN_EXEC_CONTROLS
 * @{
 */
/* External interrupts cause VM exits if set; otherwise dispatched through the guest's IDT. */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT            RT_BIT(0)
/* Non-maskable interrupts cause VM exits if set; otherwise dispatched through the guest's IDT. */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT                RT_BIT(3)
/* All other bits are reserved and must be set according to MSR IA32_VMX_PROCBASED_CTLS. */
/** @} */


/** VMX_VMCS_CTRL_PROC_EXEC_CONTROLS
 * @{
 */
/* VM Exit as soon as RFLAGS.IF=1 and no blocking is active. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT        RT_BIT(2)
/* Use timestamp counter offset. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET             RT_BIT(3)
/* VM Exit when executing the HLT instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT               RT_BIT(7)
/* VM Exit when executing the INVLPG instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT            RT_BIT(9)
/* VM Exit when executing the MWAIT instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT             RT_BIT(10)
/* VM Exit when executing the RDPMC instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT             RT_BIT(11)
/* VM Exit when executing the RDTSC instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT             RT_BIT(12)
/* VM Exit on CR8 loads. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT          RT_BIT(19)
/* VM Exit on CR8 stores. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT         RT_BIT(20)
/* Use TPR shadow. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW         RT_BIT(21)
/* VM Exit when executing a MOV DRx instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT            RT_BIT(23)
/* VM Exit when executing IO instructions. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT         RT_BIT(24)
/* Use IO bitmaps. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS         RT_BIT(25)
/* Use MSR bitmaps. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS        RT_BIT(28)
/* VM Exit when executing the MONITOR instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT           RT_BIT(29)
/* VM Exit when executing the PAUSE instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT             RT_BIT(30)
/** @} */


/** VMX_VMCS_CTRL_ENTRY_CONTROLS
 * @{
 */
/** 64 bits guest mode. Must be 0 for CPUs that don't support AMD64. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_IA64_MODE                  RT_BIT(9)
/** In SMM mode after VM-entry. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM                  RT_BIT(10)
/** Disable dual treatment of SMI and SMM; must be zero for VM-entry outside of SMM. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON         RT_BIT(11)
/** @} */


/** VMX_VMCS_CTRL_EXIT_CONTROLS
 * @{
 */
/** Return to long mode after a VM-exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_AMD64                  RT_BIT(9)
/** Acknowledge external interrupts with the irq controller if one caused a VM-exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ            RT_BIT(15)
/** @} */

/** 32 Bits read-only fields
 * @{
 */
#define VMX_VMCS_RO_VM_INSTR_ERROR                              0x4400
#define VMX_VMCS_RO_EXIT_REASON                                 0x4402
#define VMX_VMCS_RO_EXIT_INTERRUPTION_INFO                      0x4404
#define VMX_VMCS_RO_EXIT_INTERRUPTION_ERRCODE                   0x4406
#define VMX_VMCS_RO_IDT_INFO                                    0x4408
#define VMX_VMCS_RO_IDT_ERRCODE                                 0x440A
#define VMX_VMCS_RO_EXIT_INSTR_LENGTH                           0x440C
#define VMX_VMCS_RO_EXIT_INSTR_INFO                             0x440E
/** @} */

/** VMX_VMCS_RO_EXIT_INTERRUPTION_INFO
 * @{
 */
#define VMX_EXIT_INTERRUPTION_INFO_VECTOR(a)            (a & 0xff)
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT           8
#define VMX_EXIT_INTERRUPTION_INFO_TYPE(a)              ((a >> VMX_EXIT_INTERRUPTION_INFO_TYPE_SHIFT) & 7)
#define VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID       RT_BIT(11)
#define VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_IS_VALID(a) (a & VMX_EXIT_INTERRUPTION_INFO_ERROR_CODE_VALID)
#define VMX_EXIT_INTERRUPTION_INFO_NMI_UNBLOCK(a)       (a & RT_BIT(12))
#define VMX_EXIT_INTERRUPTION_INFO_VALID_SHIFT          31
#define VMX_EXIT_INTERRUPTION_INFO_VALID(a)             (a & RT_BIT(31))
/* Construct an irq event injection value from the exit interruption info value (same except that bit 12 is reserved). */
#define VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(a)      (a & ~RT_BIT(12))
/** @} */

/** VMX_VMCS_RO_EXIT_INTERRUPTION_INFO_TYPE
 * @{
 */
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT             0
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI             2
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT         3
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_SW              4 /* int xx */
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT         6
/** @} */


/** 32 Bits guest state fields
 * @{
 */
#define VMX_VMCS_GUEST_ES_LIMIT                                 0x4800
#define VMX_VMCS_GUEST_CS_LIMIT                                 0x4802
#define VMX_VMCS_GUEST_SS_LIMIT                                 0x4804
#define VMX_VMCS_GUEST_DS_LIMIT                                 0x4806
#define VMX_VMCS_GUEST_FS_LIMIT                                 0x4808
#define VMX_VMCS_GUEST_GS_LIMIT                                 0x480A
#define VMX_VMCS_GUEST_LDTR_LIMIT                               0x480C
#define VMX_VMCS_GUEST_TR_LIMIT                                 0x480E
#define VMX_VMCS_GUEST_GDTR_LIMIT                               0x4810
#define VMX_VMCS_GUEST_IDTR_LIMIT                               0x4812
#define VMX_VMCS_GUEST_ES_ACCESS_RIGHTS                         0x4814
#define VMX_VMCS_GUEST_CS_ACCESS_RIGHTS                         0x4816
#define VMX_VMCS_GUEST_SS_ACCESS_RIGHTS                         0x4818
#define VMX_VMCS_GUEST_DS_ACCESS_RIGHTS                         0x481A
#define VMX_VMCS_GUEST_FS_ACCESS_RIGHTS                         0x481C
#define VMX_VMCS_GUEST_GS_ACCESS_RIGHTS                         0x481E
#define VMX_VMCS_GUEST_LDTR_ACCESS_RIGHTS                       0x4820
#define VMX_VMCS_GUEST_TR_ACCESS_RIGHTS                         0x4822
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE                   0x4824
#define VMX_VMCS_GUEST_ACTIVITY_STATE                           0x4826
#define VMX_VMCS_GUEST_SYSENTER_CS                              0x482A  /* MSR IA32_SYSENTER_CS */
/** @} */


/** VMX_VMCS_GUEST_ACTIVITY_STATE
 * @{
 */
/* The logical processor is active. */
#define VMX_CMS_GUEST_ACTIVITY_ACTIVE                           0x0
/* The logical processor is inactive, because executed a HLT instruction. */
#define VMX_CMS_GUEST_ACTIVITY_HLT                              0x1
/* The logical processor is inactive, because of a triple fault or other serious error. */
#define VMX_CMS_GUEST_ACTIVITY_SHUTDOWN                         0x2
/* The logical processor is inactive, because it's waiting for a startup-IPI */
#define VMX_CMS_GUEST_ACTIVITY_SIPI_WAIT                        0x3
/** @} */


/** VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE
 * @{
 */
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI         RT_BIT(0)
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS       RT_BIT(1)
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI         RT_BIT(2)
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI         RT_BIT(3)
/** @} */


/** 32 Bits host state fields
 * @{
 */
#define VMX_VMCS_HOST_SYSENTER_CS                               0x4C00
/** @} */

/** Natural width control fields
 * @{
 */
#define VMX_VMCS_CTRL_CR0_MASK                                  0x6000
#define VMX_VMCS_CTRL_CR4_MASK                                  0x6002
#define VMX_VMCS_CTRL_CR0_READ_SHADOW                           0x6004
#define VMX_VMCS_CTRL_CR4_READ_SHADOW                           0x6006
#define VMX_VMCS_CTRL_CR3_TARGET_VAL0                           0x6008
#define VMX_VMCS_CTRL_CR3_TARGET_VAL1                           0x600A
#define VMX_VMCS_CTRL_CR3_TARGET_VAL2                           0x600C
#define VMX_VMCS_CTRL_CR3_TARGET_VAL31                          0x600E
/** @} */


/** Natural width read-only data fields
 * @{
 */
#define VMX_VMCS_RO_EXIT_QUALIFICATION                          0x6400
#define VMX_VMCS_RO_IO_RCX                                      0x6402
#define VMX_VMCS_RO_IO_RSX                                      0x6404
#define VMX_VMCS_RO_IO_RDI                                      0x6406
#define VMX_VMCS_RO_IO_RIP                                      0x6408
#define VMX_VMCS_GUEST_LINEAR_ADDR                              0x640A
/** @} */


/** VMX_VMCS_RO_EXIT_QUALIFICATION
 * @{
 */

/** DRx moves
 * @{
 */
/** 0-2:  Debug register number */
#define VMX_EXIT_QUALIFICATION_DRX_REGISTER(a)         (a & 7)
/** 3:    Reserved; cleared to 0. */
#define VMX_EXIT_QUALIFICATION_DRX_RES1(a)             ((a >> 3) & 1)
/** 4:    Direction of move (0 = write, 1 = read) */
#define VMX_EXIT_QUALIFICATION_DRX_DIRECTION(a)        ((a >> 4) & 1)
/** 5-7:  Reserved; cleared to 0. */
#define VMX_EXIT_QUALIFICATION_DRX_RES2(a)             ((a >> 5) & 7)
/** 8-11: General purpose register number. */
#define VMX_EXIT_QUALIFICATION_DRX_GENREG(a)           ((a >> 8) & 0xF)
/** Rest: reserved. */

/** VMX_EXIT_QUALIFICATION_DRX_DIRECTION
 * @{
 */
#define VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE     0
#define VMX_EXIT_QUALIFICATION_DRX_DIRECTION_READ      1
/** @} */

/** @} */


/** CRx accesses
 * @{
 */
/** 0-3:   Control register number (0 for CLTS & LMSW) */
#define VMX_EXIT_QUALIFICATION_CRX_REGISTER(a)         (a & 0xF)
/** 4-5:   Access type. */
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS(a)           ((a >> 4) & 3)
/** 6:     LMSW operand type */
#define VMX_EXIT_QUALIFICATION_CRX_LMSW_OP(a)          ((a >> 6) & 1)
/** 7:     Reserved; cleared to 0. */
#define VMX_EXIT_QUALIFICATION_CRX_RES1(a)             ((a >> 7) & 1)
/** 8-11:  General purpose register number (0 for CLTS & LMSW). */
#define VMX_EXIT_QUALIFICATION_CRX_GENREG(a)           ((a >> 8) & 0xF)
/** 12-15: Reserved; cleared to 0. */
#define VMX_EXIT_QUALIFICATION_CRX_RES2(a)             ((a >> 12) & 0xF)
/** 16-31: LMSW source data (else 0). */
#define VMX_EXIT_QUALIFICATION_CRX_LMSW_DATA(a)        ((a >> 16) & 0xFFFF)
/** Rest: reserved. */


/** VMX_EXIT_QUALIFICATION_CRX_ACCESS
 * @{
 */
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_WRITE        0
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_READ         1
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_CLTS         2
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_LMSW         3
/** @} */

/** @} */


/** VMX_EXIT_PORT_IO
 * @{
 */
/** 0-2:   IO operation width. */
#define VMX_EXIT_QUALIFICATION_IO_WIDTH(a)             (a & 7)
/** 3:     IO operation direction. */
#define VMX_EXIT_QUALIFICATION_IO_DIRECTION(a)         ((a >> 3) & 1)
/** 4:     String IO operation. */
#define VMX_EXIT_QUALIFICATION_IO_STRING(a)            ((a >> 4) & 1)
/** 5:     Repeated IO operation. */
#define VMX_EXIT_QUALIFICATION_IO_REP(a)               ((a >> 5) & 1)
/** 6:     Operand encoding. */
#define VMX_EXIT_QUALIFICATION_IO_ENCODING(a)          ((a >> 6) & 1)
/** 16-31: IO Port (0-0xffff). */
#define VMX_EXIT_QUALIFICATION_IO_PORT(a)              ((a >> 16) & 0xffff)
/* Rest reserved. */
/** @} */

/** VMX_EXIT_QUALIFICATION_IO_DIRECTION
 * @{
 */
#define VMX_EXIT_QUALIFICATION_IO_DIRECTION_OUT        0
#define VMX_EXIT_QUALIFICATION_IO_DIRECTION_IN         1
/** @} */


/** VMX_EXIT_QUALIFICATION_IO_ENCODING
 * @{
 */
#define VMX_EXIT_QUALIFICATION_IO_ENCODING_DX          0
#define VMX_EXIT_QUALIFICATION_IO_ENCODING_IMM         1
/** @} */

/** @} */

/** Natural width guest state fields
 * @{
 */
#define VMX_VMCS_GUEST_CR0                                      0x6800
#define VMX_VMCS_GUEST_CR3                                      0x6802
#define VMX_VMCS_GUEST_CR4                                      0x6804
#define VMX_VMCS_GUEST_ES_BASE                                  0x6806
#define VMX_VMCS_GUEST_CS_BASE                                  0x6808
#define VMX_VMCS_GUEST_SS_BASE                                  0x680A
#define VMX_VMCS_GUEST_DS_BASE                                  0x680C
#define VMX_VMCS_GUEST_FS_BASE                                  0x680E
#define VMX_VMCS_GUEST_GS_BASE                                  0x6810
#define VMX_VMCS_GUEST_LDTR_BASE                                0x6812
#define VMX_VMCS_GUEST_TR_BASE                                  0x6814
#define VMX_VMCS_GUEST_GDTR_BASE                                0x6816
#define VMX_VMCS_GUEST_IDTR_BASE                                0x6818
#define VMX_VMCS_GUEST_DR7                                      0x681A
#define VMX_VMCS_GUEST_RSP                                      0x681C
#define VMX_VMCS_GUEST_RIP                                      0x681E
#define VMX_VMCS_GUEST_RFLAGS                                   0x6820
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS                         0x6822
#define VMX_VMCS_GUEST_SYSENTER_ESP                             0x6824  /* MSR IA32_SYSENTER_ESP */
#define VMX_VMCS_GUEST_SYSENTER_EIP                             0x6826  /* MSR IA32_SYSENTER_EIP */
/** @} */


/** VMX_VMCS_GUEST_DEBUG_EXCEPTIONS
 * @{
 */
/* Hardware breakpoint 0 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B0                      RT_BIT(0)
/* Hardware breakpoint 1 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B1                      RT_BIT(1)
/* Hardware breakpoint 2 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B2                      RT_BIT(2)
/* Hardware breakpoint 3 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B3                      RT_BIT(3)
/* At least one data or IO breakpoint was hit. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_BREAKPOINT_ENABLED      RT_BIT(12)
/* A debug exception would have been triggered by single-step execution mode. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_BS                      RT_BIT(14)
/* Bits 4-11, 13 and 15-63 are reserved. */




/** @} */

/** Natural width host state fields
 * @{
 */
#define VMX_VMCS_HOST_CR0                                       0x6C00
#define VMX_VMCS_HOST_CR3                                       0x6C02
#define VMX_VMCS_HOST_CR4                                       0x6C04
#define VMX_VMCS_HOST_FS_BASE                                   0x6C06
#define VMX_VMCS_HOST_GS_BASE                                   0x6C08
#define VMX_VMCS_HOST_TR_BASE                                   0x6C0A
#define VMX_VMCS_HOST_GDTR_BASE                                 0x6C0C
#define VMX_VMCS_HOST_IDTR_BASE                                 0x6C0E
#define VMX_VMCS_HOST_SYSENTER_ESP                              0x6C10
#define VMX_VMCS_HOST_SYSENTER_EIP                              0x6C12
#define VMX_VMCS_HOST_RSP                                       0x6C14
#define VMX_VMCS_HOST_RIP                                       0x6C16
/** @} */

/** @} */


#if RT_INLINE_ASM_GNU_STYLE
#  define __STR(x) #x
#  define STR(x) __STR(x)
#endif


/** @} */

/** @defgroup grp_vmx_asm   vmx assembly helpers
 * @ingroup grp_vmx
 * @{
 */

/**
 * Executes VMXON
 *
 * @returns VBox status code
 * @param   pVMXOn      Physical address of VMXON structure
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64
DECLASM(int) VMXEnable(RTHCPHYS pVMXOn);
#else
DECLINLINE(int) VMXEnable(RTHCPHYS pVMXOn)
{
    int rc = VINF_SUCCESS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       "push     %3                                             \n\t"
       "push     %2                                             \n\t"
       ".byte    0xF3, 0x0F, 0xC7, 0x34, 0x24  # VMXON [esp]    \n\t"
       "ja       2f                                             \n\t"
       "je       1f                                             \n\t"
       "movl     $"STR(VERR_VMX_INVALID_VMXON_PTR)", %0         \n\t"
       "jmp      2f                                             \n\t"
       "1:                                                      \n\t"
       "movl     $"STR(VERR_VMX_GENERIC)", %0                   \n\t"
       "2:                                                      \n\t"
       "add      $8, %%esp                                      \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)pVMXOn),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(pVMXOn >> 32)) /* this would not work with -fomit-frame-pointer */
       :"memory"
       );
# else
    __asm
    {
        push    dword ptr [pVMXOn+4]
        push    dword ptr [pVMXOn]
        _emit   0xF3
        _emit   0x0F
        _emit   0xC7
        _emit   0x34
        _emit   0x24     /* VMXON [esp] */
        jnc     vmxon_good
        mov     dword ptr [rc], VERR_VMX_INVALID_VMXON_PTR
        jmp     the_end

vmxon_good:
        jnz     the_end
        mov     dword ptr [rc], VERR_VMX_GENERIC
the_end:
        add     esp, 8
    }
# endif
    return rc;
}
#endif


/**
 * Executes VMXOFF
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64
DECLASM(void) VMXDisable(void);
#else
DECLINLINE(void) VMXDisable(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       ".byte 0x0F, 0x01, 0xC4  # VMXOFF                        \n\t"
       );
# else
    __asm
    {
        _emit   0x0F
        _emit   0x01
        _emit   0xC4   /* VMXOFF */
    }
# endif
}
#endif


/**
 * Executes VMCLEAR
 *
 * @returns VBox status code
 * @param   pVMCS       Physical address of VM control structure
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64
DECLASM(int) VMXClearVMCS(RTHCPHYS pVMCS);
#else
DECLINLINE(int) VMXClearVMCS(RTHCPHYS pVMCS)
{
    int rc = VINF_SUCCESS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       "push    %3                                              \n\t"
       "push    %2                                              \n\t"
       ".byte   0x66, 0x0F, 0xC7, 0x34, 0x24  # VMCLEAR [esp]   \n\t"
       "jnc     1f                                              \n\t"
       "movl    $"STR(VERR_VMX_INVALID_VMCS_PTR)", %0           \n\t"
       "1:                                                      \n\t"
       "add     $8, %%esp                                       \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)pVMCS),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(pVMCS >> 32)) /* this would not work with -fomit-frame-pointer */
       :"memory"
       );
# else
    __asm
    {
        push    dword ptr [pVMCS+4]
        push    dword ptr [pVMCS]
        _emit   0x66
        _emit   0x0F
        _emit   0xC7
        _emit   0x34
        _emit   0x24     /* VMCLEAR [esp] */
        jnc     success
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
success:
        add     esp, 8
    }
# endif
    return rc;
}
#endif


/**
 * Executes VMPTRLD
 *
 * @returns VBox status code
 * @param   pVMCS       Physical address of VMCS structure
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64
DECLASM(int) VMXActivateVMCS(RTHCPHYS pVMCS);
#else
DECLINLINE(int) VMXActivateVMCS(RTHCPHYS pVMCS)
{
    int rc = VINF_SUCCESS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       "push    %3                                              \n\t"
       "push    %2                                              \n\t"
       ".byte   0x0F, 0xC7, 0x34, 0x24  # VMPTRLD [esp]         \n\t"
       "jnc     1f                                              \n\t"
       "movl    $"STR(VERR_VMX_INVALID_VMCS_PTR)", %0           \n\t"
       "1:                                                      \n\t"
       "add     $8, %%esp                                       \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)pVMCS),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(pVMCS >> 32)) /* this will not work with -fomit-frame-pointer */
       );
# else
    __asm
    {
        push    dword ptr [pVMCS+4]
        push    dword ptr [pVMCS]
        _emit   0x0F
        _emit   0xC7
        _emit   0x34
        _emit   0x24     /* VMPTRLD [esp] */
        jnc     success
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR

success:
        add     esp, 8
    }
# endif
    return rc;
}
#endif


/**
 * Executes VMWRITE
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   u64Val          16, 32 or 64 bits value
 */
DECLASM(int) VMXWriteVMCS64(uint32_t idxField, uint64_t u64Val);

/**
 * Executes VMWRITE
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   u32Val          32 bits value
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64
DECLASM(int) VMXWriteVMCS32(uint32_t idxField, uint32_t u32Val);
#else
DECLINLINE(int) VMXWriteVMCS32(uint32_t idxField, uint32_t u32Val)
{
    int rc = VINF_SUCCESS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       ".byte  0x0F, 0x79, 0xC2        # VMWRITE eax, edx       \n\t"
       "ja     2f                                               \n\t"
       "je     1f                                               \n\t"
       "movl   $"STR(VERR_VMX_INVALID_VMCS_PTR)", %0            \n\t"
       "jmp    2f                                               \n\t"
       "1:                                                      \n\t"
       "movl   $"STR(VERR_VMX_INVALID_VMCS_FIELD)", %0          \n\t"
       "2:                                                      \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "a"(idxField),
        "d"(u32Val)
       );
# else
    __asm
    {
        push   dword ptr [u32Val]
        mov    eax, [idxField]
        _emit  0x0F
        _emit  0x79
        _emit  0x04
        _emit  0x24     /* VMWRITE eax, [esp] */
        jnc    valid_vmcs
        mov    dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
        jmp    the_end

valid_vmcs:
        jnz    the_end
        mov    dword ptr [rc], VERR_VMX_INVALID_VMCS_FIELD
the_end:
        add    esp, 4
    }
# endif
    return rc;
}
#endif

#if HC_ARCH_BITS == 64
#define VMXWriteVMCS VMXWriteVMCS64
#else
#define VMXWriteVMCS VMXWriteVMCS32
#endif /* HC_ARCH_BITS == 64 */


/**
 * Executes VMREAD
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   pData           Ptr to store VM field value
 */
DECLASM(int) VMXReadVMCS64(uint32_t idxField, uint64_t *pData);

/**
 * Executes VMREAD
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   pData           Ptr to store VM field value
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64
DECLASM(int) VMXReadVMCS32(uint32_t idxField, uint32_t *pData);
#else
DECLINLINE(int) VMXReadVMCS32(uint32_t idxField, uint32_t *pData)
{
    int rc = VINF_SUCCESS;
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       "movl   $"STR(VINF_SUCCESS)", %0                          \n\t"
       ".byte  0x0F, 0x78, 0xc2        # VMREAD eax, edx         \n\t"
       "ja     2f                                                \n\t"
       "je     1f                                                \n\t"
       "movl   $"STR(VERR_VMX_INVALID_VMCS_PTR)", %0             \n\t"
       "jmp    2f                                                \n\t"
       "1:                                                       \n\t"
       "movl   $"STR(VERR_VMX_INVALID_VMCS_FIELD)", %0           \n\t"
       "2:                                                       \n\t"
       :"=&r"(rc),
        "=d"(*pData)
       :"a"(idxField),
        "d"(0)
       );
# else
    __asm
    {
        sub     esp, 4
        mov     dword ptr [esp], 0
        mov     eax, [idxField]
        _emit   0x0F
        _emit   0x78
        _emit   0x04
        _emit   0x24     /* VMREAD eax, [esp] */
        mov     edx, pData
        pop     dword ptr [edx]
        jnc     valid_vmcs
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
        jmp     the_end

valid_vmcs:
        jnz     the_end
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_FIELD
the_end:
    }
# endif
    return rc;
}
#endif

#if HC_ARCH_BITS == 64
#define VMXReadVMCS VMXReadVMCS64
#else
#define VMXReadVMCS VMXReadVMCS32
#endif /* HC_ARCH_BITS == 64 */

/**
 * Prepares for and executes VMLAUNCH
 *
 * @returns VBox status code
 * @param   pCtx        Guest context
 */
DECLASM(int) VMXStartVM(PCPUMCTX pCtx);

/**
 * Prepares for and executes VMRESUME
 *
 * @returns VBox status code
 * @param   pCtx        Guest context
 */
DECLASM(int) VMXResumeVM(PCPUMCTX pCtx);

/**
 * Gets the last instruction error value from the current VMCS
 *
 * @returns error value
 */
DECLINLINE(uint32_t) VMXGetLastError(void)
{
#if HC_ARCH_BITS == 64
    uint64_t uLastError = 0;
    int rc = VMXReadVMCS(VMX_VMCS_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return (uint32_t)uLastError;

#else /* 32-bit host: */
    uint32_t lasterr = 0;
    int      rc;

    rc = VMXReadVMCS32(VMX_VMCS_RO_VM_INSTR_ERROR, &lasterr);
    AssertRC(rc);
    return lasterr;
#endif
}

/** @} */

#endif

