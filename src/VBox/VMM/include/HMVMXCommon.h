/* $Id$ */
/** @file
 * HM/VMX - Internal header file for sharing common bits between the
 *          VMX template code (which is also used with NEM on darwin) and HM.
 */

/*
 * Copyright (C) 2006-2021 Oracle Corporation
 *
 * This file is part of VirtualBox Open Source Edition (OSE), as
 * available from http://www.virtualbox.org. This file is free software;
 * you can redistribute it and/or modify it under the terms of the GNU
 * General Public License (GPL) as published by the Free Software
 * Foundation, in version 2 as it comes in the "COPYING" file of the
 * VirtualBox OSE distribution. VirtualBox OSE is distributed in the
 * hope that it will be useful, but WITHOUT ANY WARRANTY of any kind.
 */

#ifndef VMM_INCLUDED_SRC_include_HMVMXCommon_h
#define VMM_INCLUDED_SRC_include_HMVMXCommon_h
#ifndef RT_WITHOUT_PRAGMA_ONCE
# pragma once
#endif

#include <VBox/cdefs.h>
#include <VBox/types.h>

RT_C_DECLS_BEGIN


/** @defgroup grp_hm_int       Internal
 * @ingroup grp_hm
 * @internal
 * @{
 */

/** @name HM_CHANGED_XXX
 * HM CPU-context changed flags.
 *
 * These flags are used to keep track of which registers and state has been
 * modified since they were imported back into the guest-CPU context.
 *
 * @{
 */
#define HM_CHANGED_HOST_CONTEXT                  UINT64_C(0x0000000000000001)
#define HM_CHANGED_GUEST_RIP                     UINT64_C(0x0000000000000004)
#define HM_CHANGED_GUEST_RFLAGS                  UINT64_C(0x0000000000000008)

#define HM_CHANGED_GUEST_RAX                     UINT64_C(0x0000000000000010)
#define HM_CHANGED_GUEST_RCX                     UINT64_C(0x0000000000000020)
#define HM_CHANGED_GUEST_RDX                     UINT64_C(0x0000000000000040)
#define HM_CHANGED_GUEST_RBX                     UINT64_C(0x0000000000000080)
#define HM_CHANGED_GUEST_RSP                     UINT64_C(0x0000000000000100)
#define HM_CHANGED_GUEST_RBP                     UINT64_C(0x0000000000000200)
#define HM_CHANGED_GUEST_RSI                     UINT64_C(0x0000000000000400)
#define HM_CHANGED_GUEST_RDI                     UINT64_C(0x0000000000000800)
#define HM_CHANGED_GUEST_R8_R15                  UINT64_C(0x0000000000001000)
#define HM_CHANGED_GUEST_GPRS_MASK               UINT64_C(0x0000000000001ff0)

#define HM_CHANGED_GUEST_ES                      UINT64_C(0x0000000000002000)
#define HM_CHANGED_GUEST_CS                      UINT64_C(0x0000000000004000)
#define HM_CHANGED_GUEST_SS                      UINT64_C(0x0000000000008000)
#define HM_CHANGED_GUEST_DS                      UINT64_C(0x0000000000010000)
#define HM_CHANGED_GUEST_FS                      UINT64_C(0x0000000000020000)
#define HM_CHANGED_GUEST_GS                      UINT64_C(0x0000000000040000)
#define HM_CHANGED_GUEST_SREG_MASK               UINT64_C(0x000000000007e000)

#define HM_CHANGED_GUEST_GDTR                    UINT64_C(0x0000000000080000)
#define HM_CHANGED_GUEST_IDTR                    UINT64_C(0x0000000000100000)
#define HM_CHANGED_GUEST_LDTR                    UINT64_C(0x0000000000200000)
#define HM_CHANGED_GUEST_TR                      UINT64_C(0x0000000000400000)
#define HM_CHANGED_GUEST_TABLE_MASK              UINT64_C(0x0000000000780000)

#define HM_CHANGED_GUEST_CR0                     UINT64_C(0x0000000000800000)
#define HM_CHANGED_GUEST_CR2                     UINT64_C(0x0000000001000000)
#define HM_CHANGED_GUEST_CR3                     UINT64_C(0x0000000002000000)
#define HM_CHANGED_GUEST_CR4                     UINT64_C(0x0000000004000000)
#define HM_CHANGED_GUEST_CR_MASK                 UINT64_C(0x0000000007800000)

#define HM_CHANGED_GUEST_APIC_TPR                UINT64_C(0x0000000008000000)
#define HM_CHANGED_GUEST_EFER_MSR                UINT64_C(0x0000000010000000)

#define HM_CHANGED_GUEST_DR0_DR3                 UINT64_C(0x0000000020000000)
#define HM_CHANGED_GUEST_DR6                     UINT64_C(0x0000000040000000)
#define HM_CHANGED_GUEST_DR7                     UINT64_C(0x0000000080000000)
#define HM_CHANGED_GUEST_DR_MASK                 UINT64_C(0x00000000e0000000)

#define HM_CHANGED_GUEST_X87                     UINT64_C(0x0000000100000000)
#define HM_CHANGED_GUEST_SSE_AVX                 UINT64_C(0x0000000200000000)
#define HM_CHANGED_GUEST_OTHER_XSAVE             UINT64_C(0x0000000400000000)
#define HM_CHANGED_GUEST_XCRx                    UINT64_C(0x0000000800000000)

#define HM_CHANGED_GUEST_KERNEL_GS_BASE          UINT64_C(0x0000001000000000)
#define HM_CHANGED_GUEST_SYSCALL_MSRS            UINT64_C(0x0000002000000000)
#define HM_CHANGED_GUEST_SYSENTER_CS_MSR         UINT64_C(0x0000004000000000)
#define HM_CHANGED_GUEST_SYSENTER_EIP_MSR        UINT64_C(0x0000008000000000)
#define HM_CHANGED_GUEST_SYSENTER_ESP_MSR        UINT64_C(0x0000010000000000)
#define HM_CHANGED_GUEST_SYSENTER_MSR_MASK       UINT64_C(0x000001c000000000)
#define HM_CHANGED_GUEST_TSC_AUX                 UINT64_C(0x0000020000000000)
#define HM_CHANGED_GUEST_OTHER_MSRS              UINT64_C(0x0000040000000000)
#define HM_CHANGED_GUEST_ALL_MSRS                (  HM_CHANGED_GUEST_EFER              \
                                                  | HM_CHANGED_GUEST_KERNEL_GS_BASE    \
                                                  | HM_CHANGED_GUEST_SYSCALL_MSRS      \
                                                  | HM_CHANGED_GUEST_SYSENTER_MSR_MASK \
                                                  | HM_CHANGED_GUEST_TSC_AUX           \
                                                  | HM_CHANGED_GUEST_OTHER_MSRS)

#define HM_CHANGED_GUEST_HWVIRT                  UINT64_C(0x0000080000000000)
#define HM_CHANGED_GUEST_MASK                    UINT64_C(0x00000ffffffffffc)

#define HM_CHANGED_KEEPER_STATE_MASK             UINT64_C(0xffff000000000000)

#define HM_CHANGED_VMX_XCPT_INTERCEPTS           UINT64_C(0x0001000000000000)
#define HM_CHANGED_VMX_GUEST_AUTO_MSRS           UINT64_C(0x0002000000000000)
#define HM_CHANGED_VMX_GUEST_LAZY_MSRS           UINT64_C(0x0004000000000000)
#define HM_CHANGED_VMX_ENTRY_EXIT_CTLS           UINT64_C(0x0008000000000000)
#define HM_CHANGED_VMX_MASK                      UINT64_C(0x000f000000000000)
#define HM_CHANGED_VMX_HOST_GUEST_SHARED_STATE   (  HM_CHANGED_GUEST_DR_MASK \
                                                  | HM_CHANGED_VMX_GUEST_LAZY_MSRS)

#define HM_CHANGED_SVM_XCPT_INTERCEPTS           UINT64_C(0x0001000000000000)
#define HM_CHANGED_SVM_MASK                      UINT64_C(0x0001000000000000)
#define HM_CHANGED_SVM_HOST_GUEST_SHARED_STATE   HM_CHANGED_GUEST_DR_MASK

#define HM_CHANGED_ALL_GUEST                     (  HM_CHANGED_GUEST_MASK \
                                                  | HM_CHANGED_KEEPER_STATE_MASK)

/** Mask of what state might have changed when IEM raised an exception.
 *  This is a based on IEM_CPUMCTX_EXTRN_XCPT_MASK. */
#define HM_CHANGED_RAISED_XCPT_MASK              (  HM_CHANGED_GUEST_GPRS_MASK  \
                                                  | HM_CHANGED_GUEST_RIP        \
                                                  | HM_CHANGED_GUEST_RFLAGS     \
                                                  | HM_CHANGED_GUEST_SS         \
                                                  | HM_CHANGED_GUEST_CS         \
                                                  | HM_CHANGED_GUEST_CR0        \
                                                  | HM_CHANGED_GUEST_CR3        \
                                                  | HM_CHANGED_GUEST_CR4        \
                                                  | HM_CHANGED_GUEST_APIC_TPR   \
                                                  | HM_CHANGED_GUEST_EFER_MSR   \
                                                  | HM_CHANGED_GUEST_DR7        \
                                                  | HM_CHANGED_GUEST_CR2        \
                                                  | HM_CHANGED_GUEST_SREG_MASK  \
                                                  | HM_CHANGED_GUEST_TABLE_MASK)

#ifdef VBOX_WITH_NESTED_HWVIRT_SVM
/** Mask of what state might have changed when \#VMEXIT is emulated. */
# define HM_CHANGED_SVM_VMEXIT_MASK              (  HM_CHANGED_GUEST_RSP         \
                                                  | HM_CHANGED_GUEST_RAX         \
                                                  | HM_CHANGED_GUEST_RIP         \
                                                  | HM_CHANGED_GUEST_RFLAGS      \
                                                  | HM_CHANGED_GUEST_CS          \
                                                  | HM_CHANGED_GUEST_SS          \
                                                  | HM_CHANGED_GUEST_DS          \
                                                  | HM_CHANGED_GUEST_ES          \
                                                  | HM_CHANGED_GUEST_GDTR        \
                                                  | HM_CHANGED_GUEST_IDTR        \
                                                  | HM_CHANGED_GUEST_CR_MASK     \
                                                  | HM_CHANGED_GUEST_EFER_MSR    \
                                                  | HM_CHANGED_GUEST_DR6         \
                                                  | HM_CHANGED_GUEST_DR7         \
                                                  | HM_CHANGED_GUEST_OTHER_MSRS  \
                                                  | HM_CHANGED_GUEST_HWVIRT      \
                                                  | HM_CHANGED_SVM_MASK          \
                                                  | HM_CHANGED_GUEST_APIC_TPR)

/** Mask of what state might have changed when VMRUN is emulated. */
# define HM_CHANGED_SVM_VMRUN_MASK               HM_CHANGED_SVM_VMEXIT_MASK
#endif
#ifdef VBOX_WITH_NESTED_HWVIRT_VMX
/** Mask of what state might have changed when VM-exit is emulated.
 *
 *  This is currently unused, but keeping it here in case we can get away a bit more
 *  fine-grained state handling.
 *
 *  @note Update IEM_CPUMCTX_EXTRN_VMX_VMEXIT_MASK when this changes. */
# define HM_CHANGED_VMX_VMEXIT_MASK             (  HM_CHANGED_GUEST_CR0 | HM_CHANGED_GUEST_CR3 | HM_CHANGED_GUEST_CR4 \
                                                 | HM_CHANGED_GUEST_DR7 | HM_CHANGED_GUEST_DR6 \
                                                 | HM_CHANGED_GUEST_EFER_MSR \
                                                 | HM_CHANGED_GUEST_SYSENTER_MSR_MASK \
                                                 | HM_CHANGED_GUEST_OTHER_MSRS    /* for PAT MSR */ \
                                                 | HM_CHANGED_GUEST_RIP | HM_CHANGED_GUEST_RSP | HM_CHANGED_GUEST_RFLAGS \
                                                 | HM_CHANGED_GUEST_SREG_MASK \
                                                 | HM_CHANGED_GUEST_TR \
                                                 | HM_CHANGED_GUEST_LDTR | HM_CHANGED_GUEST_GDTR | HM_CHANGED_GUEST_IDTR \
                                                 | HM_CHANGED_GUEST_HWVIRT )
#endif
/** @} */


/** Maximum number of exit reason statistics counters. */
#define MAX_EXITREASON_STAT                        0x100
#define MASK_EXITREASON_STAT                       0xff
#define MASK_INJECT_IRQ_STAT                       0xff


/**
 * HM event.
 *
 * VT-x and AMD-V common event injection structure.
 */
typedef struct HMEVENT
{
    /** Whether the event is pending. */
    uint32_t        fPending;
    /** The error-code associated with the event. */
    uint32_t        u32ErrCode;
    /** The length of the instruction in bytes (only relevant for software
     *  interrupts or software exceptions). */
    uint32_t        cbInstr;
    /** Alignment. */
    uint32_t        u32Padding;
    /** The encoded event (VM-entry interruption-information for VT-x or EVENTINJ
     *  for SVM). */
    uint64_t        u64IntInfo;
    /** Guest virtual address if this is a page-fault event. */
    RTGCUINTPTR     GCPtrFaultAddress;
} HMEVENT;
/** Pointer to a HMEVENT struct. */
typedef HMEVENT *PHMEVENT;
/** Pointer to a const HMEVENT struct. */
typedef const HMEVENT *PCHMEVENT;
AssertCompileSizeAlignment(HMEVENT, 8);

/** @} */

/** @} */

RT_C_DECLS_END

#endif /* !VMM_INCLUDED_SRC_include_HMVMXCommon_h */

