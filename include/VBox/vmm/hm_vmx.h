/** @file
 * HM - VMX Structures and Definitions. (VMM)
 */

/*
 * Copyright (C) 2006-2017 Oracle Corporation
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

#ifndef ___VBox_vmm_vmx_h
#define ___VBox_vmm_vmx_h

#include <VBox/types.h>
#include <VBox/err.h>
#include <iprt/x86.h>
#include <iprt/assert.h>

/* In Visual C++ versions prior to 2012, the vmx intrinsics are only available
   when targeting AMD64. */
#if RT_INLINE_ASM_USES_INTRIN >= 16 && defined(RT_ARCH_AMD64)
# pragma warning(push)
# pragma warning(disable:4668) /* Several incorrect __cplusplus uses. */
# pragma warning(disable:4255) /* Incorrect __slwpcb prototype. */
# include <intrin.h>
# pragma warning(pop)
/* We always want them as intrinsics, no functions. */
# pragma intrinsic(__vmx_on)
# pragma intrinsic(__vmx_off)
# pragma intrinsic(__vmx_vmclear)
# pragma intrinsic(__vmx_vmptrld)
# pragma intrinsic(__vmx_vmread)
# pragma intrinsic(__vmx_vmwrite)
# define VMX_USE_MSC_INTRINSICS 1
#else
# define VMX_USE_MSC_INTRINSICS 0
#endif


/** @defgroup grp_hm_vmx    VMX Types and Definitions
 * @ingroup grp_hm
 * @{
 */

/** @name Host-state restoration flags.
 * @note If you change these values don't forget to update the assembly
 *       defines as well!
 * @{
 */
#define VMX_RESTORE_HOST_SEL_DS                                 RT_BIT(0)
#define VMX_RESTORE_HOST_SEL_ES                                 RT_BIT(1)
#define VMX_RESTORE_HOST_SEL_FS                                 RT_BIT(2)
#define VMX_RESTORE_HOST_SEL_GS                                 RT_BIT(3)
#define VMX_RESTORE_HOST_SEL_TR                                 RT_BIT(4)
#define VMX_RESTORE_HOST_GDTR                                   RT_BIT(5)
#define VMX_RESTORE_HOST_IDTR                                   RT_BIT(6)
#define VMX_RESTORE_HOST_GDT_READ_ONLY                          RT_BIT(7)
#define VMX_RESTORE_HOST_REQUIRED                               RT_BIT(8)
#define VMX_RESTORE_HOST_GDT_NEED_WRITABLE                      RT_BIT(9)
/** @} */

/**
 * Host-state restoration structure.
 * This holds host-state fields that require manual restoration.
 * Assembly version found in hm_vmx.mac (should be automatically verified).
 */
typedef struct VMXRESTOREHOST
{
    RTSEL       uHostSelDS;     /* 0x00 */
    RTSEL       uHostSelES;     /* 0x02 */
    RTSEL       uHostSelFS;     /* 0x04 */
    RTSEL       uHostSelGS;     /* 0x06 */
    RTSEL       uHostSelTR;     /* 0x08 */
    uint8_t     abPadding0[4];
    X86XDTR64   HostGdtr;       /**< 0x0e - should be aligned by it's 64-bit member. */
    uint8_t     abPadding1[6];
    X86XDTR64   HostGdtrRw;     /**< 0x1e - should be aligned by it's 64-bit member. */
    uint8_t     abPadding2[6];
    X86XDTR64   HostIdtr;       /**< 0x2e - should be aligned by it's 64-bit member. */
    uint64_t    uHostFSBase;    /* 0x38 */
    uint64_t    uHostGSBase;    /* 0x40 */
} VMXRESTOREHOST;
/** Pointer to VMXRESTOREHOST. */
typedef VMXRESTOREHOST *PVMXRESTOREHOST;
AssertCompileSize(X86XDTR64, 10);
AssertCompileMemberOffset(VMXRESTOREHOST, HostGdtr.uAddr,   16);
AssertCompileMemberOffset(VMXRESTOREHOST, HostGdtrRw.uAddr, 32);
AssertCompileMemberOffset(VMXRESTOREHOST, HostIdtr.uAddr,   48);
AssertCompileMemberOffset(VMXRESTOREHOST, uHostFSBase,      56);
AssertCompileSize(VMXRESTOREHOST, 72);
AssertCompileSizeAlignment(VMXRESTOREHOST, 8);

/** @name Host-state MSR lazy-restoration flags.
 * @{
 */
/** The host MSRs have been saved. */
#define VMX_LAZY_MSRS_SAVED_HOST                                RT_BIT(0)
/** The guest MSRs are loaded and in effect. */
#define VMX_LAZY_MSRS_LOADED_GUEST                              RT_BIT(1)
/** @} */

/** @name VMX HM-error codes for VERR_HM_UNSUPPORTED_CPU_FEATURE_COMBO.
 *  UFC = Unsupported Feature Combination.
 * @{
 */
/** Unsupported pin-based VM-execution controls combo. */
#define VMX_UFC_CTRL_PIN_EXEC                                   1
/** Unsupported processor-based VM-execution controls combo. */
#define VMX_UFC_CTRL_PROC_EXEC                                  2
/** Unsupported move debug register VM-exit combo. */
#define VMX_UFC_CTRL_PROC_MOV_DRX_EXIT                          3
/** Unsupported VM-entry controls combo. */
#define VMX_UFC_CTRL_ENTRY                                      4
/** Unsupported VM-exit controls combo. */
#define VMX_UFC_CTRL_EXIT                                       5
/** MSR storage capacity of the VMCS autoload/store area is not sufficient
 *  for storing host MSRs. */
#define VMX_UFC_INSUFFICIENT_HOST_MSR_STORAGE                   6
/** MSR storage capacity of the VMCS autoload/store area is not sufficient
 *  for storing guest MSRs. */
#define VMX_UFC_INSUFFICIENT_GUEST_MSR_STORAGE                  7
/** Invalid VMCS size. */
#define VMX_UFC_INVALID_VMCS_SIZE                               8
/** Unsupported secondary processor-based VM-execution controls combo. */
#define VMX_UFC_CTRL_PROC_EXEC2                                 9
/** Invalid unrestricted-guest execution controls combo. */
#define VMX_UFC_INVALID_UX_COMBO                                10
/** EPT flush type not supported. */
#define VMX_UFC_EPT_FLUSH_TYPE_UNSUPPORTED                      11
/** EPT paging structure memory type is not write-back. */
#define VMX_UFC_EPT_MEM_TYPE_NOT_WB                             12
/** EPT requires INVEPT instr. support but it's not available. */
#define VMX_UFC_EPT_INVEPT_UNAVAILABLE                          13
/** EPT requires page-walk length of 4. */
#define VMX_UFC_EPT_PAGE_WALK_LENGTH_UNSUPPORTED                14
/** @} */

/** @name VMX HM-error codes for VERR_VMX_VMCS_FIELD_CACHE_INVALID.
 *  VCI = VMCS-field Cache Invalid.
 * @{
 */
/** Cache of VM-entry controls invalid. */
#define VMX_VCI_CTRL_ENTRY                                      300
/** Cache of VM-exit controls invalid. */
#define VMX_VCI_CTRL_EXIT                                       301
/** Cache of pin-based VM-execution controls invalid. */
#define VMX_VCI_CTRL_PIN_EXEC                                   302
/** Cache of processor-based VM-execution controls invalid. */
#define VMX_VCI_CTRL_PROC_EXEC                                  303
/** Cache of secondary processor-based VM-execution controls invalid. */
#define VMX_VCI_CTRL_PROC_EXEC2                                 304
/** Cache of exception bitmap invalid. */
#define VMX_VCI_CTRL_XCPT_BITMAP                                305
/** Cache of TSC offset invalid. */
#define VMX_VCI_CTRL_TSC_OFFSET                                 306
/** @} */

/** @name VMX HM-error codes for VERR_VMX_INVALID_GUEST_STATE.
 *  IGS = Invalid Guest State.
 * @{
 */
/** An error occurred while checking invalid-guest-state. */
#define VMX_IGS_ERROR                                           500
/** The invalid guest-state checks did not find any reason why. */
#define VMX_IGS_REASON_NOT_FOUND                                501
/** CR0 fixed1 bits invalid. */
#define VMX_IGS_CR0_FIXED1                                      502
/** CR0 fixed0 bits invalid. */
#define VMX_IGS_CR0_FIXED0                                      503
/** CR0.PE and CR0.PE invalid VT-x/host combination. */
#define VMX_IGS_CR0_PG_PE_COMBO                                 504
/** CR4 fixed1 bits invalid. */
#define VMX_IGS_CR4_FIXED1                                      505
/** CR4 fixed0 bits invalid. */
#define VMX_IGS_CR4_FIXED0                                      506
/** Reserved bits in VMCS' DEBUGCTL MSR field not set to 0 when
 *  VMX_VMCS_CTRL_ENTRY_LOAD_DEBUG is used. */
#define VMX_IGS_DEBUGCTL_MSR_RESERVED                           507
/** CR0.PG not set for long-mode when not using unrestricted guest. */
#define VMX_IGS_CR0_PG_LONGMODE                                 508
/** CR4.PAE not set for long-mode guest when not using unrestricted guest. */
#define VMX_IGS_CR4_PAE_LONGMODE                                509
/** CR4.PCIDE set for 32-bit guest. */
#define VMX_IGS_CR4_PCIDE                                       510
/** VMCS' DR7 reserved bits not set to 0. */
#define VMX_IGS_DR7_RESERVED                                    511
/** VMCS' PERF_GLOBAL MSR reserved bits not set to 0. */
#define VMX_IGS_PERF_GLOBAL_MSR_RESERVED                        512
/** VMCS' EFER MSR reserved bits not set to 0. */
#define VMX_IGS_EFER_MSR_RESERVED                               513
/** VMCS' EFER MSR.LMA does not match the IA32e mode guest control. */
#define VMX_IGS_EFER_LMA_GUEST_MODE_MISMATCH                    514
/** VMCS' EFER MSR.LMA does not match EFER.LME of the guest when using paging
 *  without unrestricted guest. */
#define VMX_IGS_EFER_LMA_LME_MISMATCH                           515
/** CS.Attr.P bit invalid. */
#define VMX_IGS_CS_ATTR_P_INVALID                               516
/** CS.Attr reserved bits not set to 0.  */
#define VMX_IGS_CS_ATTR_RESERVED                                517
/** CS.Attr.G bit invalid. */
#define VMX_IGS_CS_ATTR_G_INVALID                               518
/** CS is unusable. */
#define VMX_IGS_CS_ATTR_UNUSABLE                                519
/** CS and SS DPL unequal. */
#define VMX_IGS_CS_SS_ATTR_DPL_UNEQUAL                          520
/** CS and SS DPL mismatch. */
#define VMX_IGS_CS_SS_ATTR_DPL_MISMATCH                         521
/** CS Attr.Type invalid. */
#define VMX_IGS_CS_ATTR_TYPE_INVALID                            522
/** CS and SS RPL unequal. */
#define VMX_IGS_SS_CS_RPL_UNEQUAL                               523
/** SS.Attr.DPL and SS RPL unequal. */
#define VMX_IGS_SS_ATTR_DPL_RPL_UNEQUAL                         524
/** SS.Attr.DPL invalid for segment type. */
#define VMX_IGS_SS_ATTR_DPL_INVALID                             525
/** SS.Attr.Type invalid. */
#define VMX_IGS_SS_ATTR_TYPE_INVALID                            526
/** SS.Attr.P bit invalid. */
#define VMX_IGS_SS_ATTR_P_INVALID                               527
/** SS.Attr reserved bits not set to 0. */
#define VMX_IGS_SS_ATTR_RESERVED                                528
/** SS.Attr.G bit invalid. */
#define VMX_IGS_SS_ATTR_G_INVALID                               529
/** DS.Attr.A bit invalid. */
#define VMX_IGS_DS_ATTR_A_INVALID                               530
/** DS.Attr.P bit invalid. */
#define VMX_IGS_DS_ATTR_P_INVALID                               531
/** DS.Attr.DPL and DS RPL unequal. */
#define VMX_IGS_DS_ATTR_DPL_RPL_UNEQUAL                         532
/** DS.Attr reserved bits not set to 0. */
#define VMX_IGS_DS_ATTR_RESERVED                                533
/** DS.Attr.G bit invalid. */
#define VMX_IGS_DS_ATTR_G_INVALID                               534
/** DS.Attr.Type invalid. */
#define VMX_IGS_DS_ATTR_TYPE_INVALID                            535
/** ES.Attr.A bit invalid. */
#define VMX_IGS_ES_ATTR_A_INVALID                               536
/** ES.Attr.P bit invalid. */
#define VMX_IGS_ES_ATTR_P_INVALID                               537
/** ES.Attr.DPL and DS RPL unequal. */
#define VMX_IGS_ES_ATTR_DPL_RPL_UNEQUAL                         538
/** ES.Attr reserved bits not set to 0. */
#define VMX_IGS_ES_ATTR_RESERVED                                539
/** ES.Attr.G bit invalid. */
#define VMX_IGS_ES_ATTR_G_INVALID                               540
/** ES.Attr.Type invalid. */
#define VMX_IGS_ES_ATTR_TYPE_INVALID                            541
/** FS.Attr.A bit invalid. */
#define VMX_IGS_FS_ATTR_A_INVALID                               542
/** FS.Attr.P bit invalid. */
#define VMX_IGS_FS_ATTR_P_INVALID                               543
/** FS.Attr.DPL and DS RPL unequal. */
#define VMX_IGS_FS_ATTR_DPL_RPL_UNEQUAL                         544
/** FS.Attr reserved bits not set to 0. */
#define VMX_IGS_FS_ATTR_RESERVED                                545
/** FS.Attr.G bit invalid. */
#define VMX_IGS_FS_ATTR_G_INVALID                               546
/** FS.Attr.Type invalid. */
#define VMX_IGS_FS_ATTR_TYPE_INVALID                            547
/** GS.Attr.A bit invalid. */
#define VMX_IGS_GS_ATTR_A_INVALID                               548
/** GS.Attr.P bit invalid. */
#define VMX_IGS_GS_ATTR_P_INVALID                               549
/** GS.Attr.DPL and DS RPL unequal. */
#define VMX_IGS_GS_ATTR_DPL_RPL_UNEQUAL                         550
/** GS.Attr reserved bits not set to 0. */
#define VMX_IGS_GS_ATTR_RESERVED                                551
/** GS.Attr.G bit invalid. */
#define VMX_IGS_GS_ATTR_G_INVALID                               552
/** GS.Attr.Type invalid. */
#define VMX_IGS_GS_ATTR_TYPE_INVALID                            553
/** V86 mode CS.Base invalid. */
#define VMX_IGS_V86_CS_BASE_INVALID                             554
/** V86 mode CS.Limit invalid. */
#define VMX_IGS_V86_CS_LIMIT_INVALID                            555
/** V86 mode CS.Attr invalid. */
#define VMX_IGS_V86_CS_ATTR_INVALID                             556
/** V86 mode SS.Base invalid. */
#define VMX_IGS_V86_SS_BASE_INVALID                             557
/** V86 mode SS.Limit invalid. */
#define VMX_IGS_V86_SS_LIMIT_INVALID                            558
/** V86 mode SS.Attr invalid. */
#define VMX_IGS_V86_SS_ATTR_INVALID                             559
/** V86 mode DS.Base invalid. */
#define VMX_IGS_V86_DS_BASE_INVALID                             560
/** V86 mode DS.Limit invalid. */
#define VMX_IGS_V86_DS_LIMIT_INVALID                            561
/** V86 mode DS.Attr invalid. */
#define VMX_IGS_V86_DS_ATTR_INVALID                             562
/** V86 mode ES.Base invalid. */
#define VMX_IGS_V86_ES_BASE_INVALID                             563
/** V86 mode ES.Limit invalid. */
#define VMX_IGS_V86_ES_LIMIT_INVALID                            564
/** V86 mode ES.Attr invalid. */
#define VMX_IGS_V86_ES_ATTR_INVALID                             565
/** V86 mode FS.Base invalid. */
#define VMX_IGS_V86_FS_BASE_INVALID                             566
/** V86 mode FS.Limit invalid. */
#define VMX_IGS_V86_FS_LIMIT_INVALID                            567
/** V86 mode FS.Attr invalid. */
#define VMX_IGS_V86_FS_ATTR_INVALID                             568
/** V86 mode GS.Base invalid. */
#define VMX_IGS_V86_GS_BASE_INVALID                             569
/** V86 mode GS.Limit invalid. */
#define VMX_IGS_V86_GS_LIMIT_INVALID                            570
/** V86 mode GS.Attr invalid. */
#define VMX_IGS_V86_GS_ATTR_INVALID                             571
/** Longmode CS.Base invalid. */
#define VMX_IGS_LONGMODE_CS_BASE_INVALID                        572
/** Longmode SS.Base invalid. */
#define VMX_IGS_LONGMODE_SS_BASE_INVALID                        573
/** Longmode DS.Base invalid. */
#define VMX_IGS_LONGMODE_DS_BASE_INVALID                        574
/** Longmode ES.Base invalid. */
#define VMX_IGS_LONGMODE_ES_BASE_INVALID                        575
/** SYSENTER ESP is not canonical. */
#define VMX_IGS_SYSENTER_ESP_NOT_CANONICAL                      576
/** SYSENTER EIP is not canonical. */
#define VMX_IGS_SYSENTER_EIP_NOT_CANONICAL                      577
/** PAT MSR invalid. */
#define VMX_IGS_PAT_MSR_INVALID                                 578
/** PAT MSR reserved bits not set to 0. */
#define VMX_IGS_PAT_MSR_RESERVED                                579
/** GDTR.Base is not canonical. */
#define VMX_IGS_GDTR_BASE_NOT_CANONICAL                         580
/** IDTR.Base is not canonical. */
#define VMX_IGS_IDTR_BASE_NOT_CANONICAL                         581
/** GDTR.Limit invalid. */
#define VMX_IGS_GDTR_LIMIT_INVALID                              582
/** IDTR.Limit invalid. */
#define VMX_IGS_IDTR_LIMIT_INVALID                              583
/** Longmode RIP is invalid. */
#define VMX_IGS_LONGMODE_RIP_INVALID                            584
/** RFLAGS reserved bits not set to 0. */
#define VMX_IGS_RFLAGS_RESERVED                                 585
/** RFLAGS RA1 reserved bits not set to 1. */
#define VMX_IGS_RFLAGS_RESERVED1                                586
/** RFLAGS.VM (V86 mode) invalid. */
#define VMX_IGS_RFLAGS_VM_INVALID                               587
/** RFLAGS.IF invalid. */
#define VMX_IGS_RFLAGS_IF_INVALID                               588
/** Activity state invalid. */
#define VMX_IGS_ACTIVITY_STATE_INVALID                          589
/** Activity state HLT invalid when SS.Attr.DPL is not zero. */
#define VMX_IGS_ACTIVITY_STATE_HLT_INVALID                      590
/** Activity state ACTIVE invalid when block-by-STI or MOV SS. */
#define VMX_IGS_ACTIVITY_STATE_ACTIVE_INVALID                   591
/** Activity state SIPI WAIT invalid. */
#define VMX_IGS_ACTIVITY_STATE_SIPI_WAIT_INVALID                592
/** Interruptibility state reserved bits not set to 0. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_RESERVED                 593
/** Interruptibility state cannot be block-by-STI -and- MOV SS. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_STI_MOVSS_INVALID        594
/** Interruptibility state block-by-STI invalid for EFLAGS. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_STI_EFL_INVALID          595
/** Interruptibility state invalid while trying to deliver external
 *  interrupt. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_EXT_INT_INVALID          596
/** Interruptibility state block-by-MOVSS invalid while trying to deliver an
 *  NMI. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_MOVSS_INVALID            597
/** Interruptibility state block-by-SMI invalid when CPU is not in SMM. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_SMI_INVALID              598
/** Interruptibility state block-by-SMI invalid when trying to enter SMM. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_SMI_SMM_INVALID          599
/** Interruptibility state block-by-STI (maybe) invalid when trying to
 *  deliver an NMI. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_STI_INVALID              600
/** Interruptibility state block-by-NMI invalid when virtual-NMIs control is
 *  active. */
#define VMX_IGS_INTERRUPTIBILITY_STATE_NMI_INVALID              601
/** Pending debug exceptions reserved bits not set to 0. */
#define VMX_IGS_PENDING_DEBUG_RESERVED                          602
/** Longmode pending debug exceptions reserved bits not set to 0. */
#define VMX_IGS_LONGMODE_PENDING_DEBUG_RESERVED                 603
/** Pending debug exceptions.BS bit is not set when it should be. */
#define VMX_IGS_PENDING_DEBUG_XCPT_BS_NOT_SET                   604
/** Pending debug exceptions.BS bit is not clear when it should be. */
#define VMX_IGS_PENDING_DEBUG_XCPT_BS_NOT_CLEAR                 605
/** VMCS link pointer reserved bits not set to 0. */
#define VMX_IGS_VMCS_LINK_PTR_RESERVED                          606
/** TR cannot index into LDT, TI bit MBZ. */
#define VMX_IGS_TR_TI_INVALID                                   607
/** LDTR cannot index into LDT. TI bit MBZ. */
#define VMX_IGS_LDTR_TI_INVALID                                 608
/** TR.Base is not canonical. */
#define VMX_IGS_TR_BASE_NOT_CANONICAL                           609
/** FS.Base is not canonical. */
#define VMX_IGS_FS_BASE_NOT_CANONICAL                           610
/** GS.Base is not canonical. */
#define VMX_IGS_GS_BASE_NOT_CANONICAL                           611
/** LDTR.Base is not canonical. */
#define VMX_IGS_LDTR_BASE_NOT_CANONICAL                         612
/** TR is unusable. */
#define VMX_IGS_TR_ATTR_UNUSABLE                                613
/** TR.Attr.S bit invalid. */
#define VMX_IGS_TR_ATTR_S_INVALID                               614
/** TR is not present. */
#define VMX_IGS_TR_ATTR_P_INVALID                               615
/** TR.Attr reserved bits not set to 0. */
#define VMX_IGS_TR_ATTR_RESERVED                                616
/** TR.Attr.G bit invalid. */
#define VMX_IGS_TR_ATTR_G_INVALID                               617
/** Longmode TR.Attr.Type invalid. */
#define VMX_IGS_LONGMODE_TR_ATTR_TYPE_INVALID                   618
/** TR.Attr.Type invalid. */
#define VMX_IGS_TR_ATTR_TYPE_INVALID                            619
/** CS.Attr.S invalid. */
#define VMX_IGS_CS_ATTR_S_INVALID                               620
/** CS.Attr.DPL invalid. */
#define VMX_IGS_CS_ATTR_DPL_INVALID                             621
/** PAE PDPTE reserved bits not set to 0. */
#define VMX_IGS_PAE_PDPTE_RESERVED                              623
/** @} */

/** @name VMX VMCS-Read cache indices.
 * @{
 */
#define VMX_VMCS_GUEST_ES_BASE_CACHE_IDX                        0
#define VMX_VMCS_GUEST_CS_BASE_CACHE_IDX                        1
#define VMX_VMCS_GUEST_SS_BASE_CACHE_IDX                        2
#define VMX_VMCS_GUEST_DS_BASE_CACHE_IDX                        3
#define VMX_VMCS_GUEST_FS_BASE_CACHE_IDX                        4
#define VMX_VMCS_GUEST_GS_BASE_CACHE_IDX                        5
#define VMX_VMCS_GUEST_LDTR_BASE_CACHE_IDX                      6
#define VMX_VMCS_GUEST_TR_BASE_CACHE_IDX                        7
#define VMX_VMCS_GUEST_GDTR_BASE_CACHE_IDX                      8
#define VMX_VMCS_GUEST_IDTR_BASE_CACHE_IDX                      9
#define VMX_VMCS_GUEST_RSP_CACHE_IDX                            10
#define VMX_VMCS_GUEST_RIP_CACHE_IDX                            11
#define VMX_VMCS_GUEST_SYSENTER_ESP_CACHE_IDX                   12
#define VMX_VMCS_GUEST_SYSENTER_EIP_CACHE_IDX                   13
#define VMX_VMCS_RO_EXIT_QUALIFICATION_CACHE_IDX                14
#define VMX_VMCS_MAX_CACHE_IDX                                  (VMX_VMCS_RO_EXIT_QUALIFICATION_CACHE_IDX + 1)
#define VMX_VMCS_GUEST_CR3_CACHE_IDX                            15
#define VMX_VMCS_MAX_NESTED_PAGING_CACHE_IDX                    (VMX_VMCS_GUEST_CR3_CACHE_IDX + 1)
/** @} */

/** @name VMX EPT paging structures
 * @{
 */

/**
 * Number of page table entries in the EPT. (PDPTE/PDE/PTE)
 */
#define EPT_PG_ENTRIES          X86_PG_PAE_ENTRIES

/**
 * EPT Page Directory Pointer Entry. Bit view.
 * @todo uint64_t isn't safe for bitfields (gcc pedantic warnings, and IIRC,
 *       this did cause trouble with one compiler/version).
 */
typedef struct EPTPML4EBITS
{
    /** Present bit. */
    uint64_t    u1Present       : 1;
    /** Writable bit. */
    uint64_t    u1Write         : 1;
    /** Executable bit. */
    uint64_t    u1Execute       : 1;
    /** Reserved (must be 0). */
    uint64_t    u5Reserved      : 5;
    /** Available for software. */
    uint64_t    u4Available     : 4;
    /** Physical address of the next level (PD). Restricted by maximum physical address width of the cpu. */
    uint64_t    u40PhysAddr     : 40;
    /** Available for software. */
    uint64_t    u12Available    : 12;
} EPTPML4EBITS;
AssertCompileSize(EPTPML4EBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PML4E_PG_MASK       X86_PML4E_PG_MASK
/** The page shift to get the PML4 index. */
#define EPT_PML4_SHIFT          X86_PML4_SHIFT
/** The PML4 index mask (apply to a shifted page address). */
#define EPT_PML4_MASK           X86_PML4_MASK

/**
 * EPT PML4E.
 */
typedef union EPTPML4E
{
    /** Normal view. */
    EPTPML4EBITS    n;
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** 64 bit unsigned integer view. */
    uint64_t        au64[1];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} EPTPML4E;
AssertCompileSize(EPTPML4E, 8);
/** Pointer to a PML4 table entry. */
typedef EPTPML4E *PEPTPML4E;
/** Pointer to a const PML4 table entry. */
typedef const EPTPML4E *PCEPTPML4E;

/**
 * EPT PML4 Table.
 */
typedef struct EPTPML4
{
    EPTPML4E    a[EPT_PG_ENTRIES];
} EPTPML4;
AssertCompileSize(EPTPML4, 0x1000);
/** Pointer to an EPT PML4 Table. */
typedef EPTPML4 *PEPTPML4;
/** Pointer to a const EPT PML4 Table. */
typedef const EPTPML4 *PCEPTPML4;

/**
 * EPT Page Directory Pointer Entry. Bit view.
 */
typedef struct EPTPDPTEBITS
{
    /** Present bit. */
    uint64_t    u1Present       : 1;
    /** Writable bit. */
    uint64_t    u1Write         : 1;
    /** Executable bit. */
    uint64_t    u1Execute       : 1;
    /** Reserved (must be 0). */
    uint64_t    u5Reserved      : 5;
    /** Available for software. */
    uint64_t    u4Available     : 4;
    /** Physical address of the next level (PD). Restricted by maximum physical address width of the cpu. */
    uint64_t    u40PhysAddr     : 40;
    /** Available for software. */
    uint64_t    u12Available    : 12;
} EPTPDPTEBITS;
AssertCompileSize(EPTPDPTEBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDPTE_PG_MASK       X86_PDPE_PG_MASK
/** The page shift to get the PDPT index. */
#define EPT_PDPT_SHIFT          X86_PDPT_SHIFT
/** The PDPT index mask (apply to a shifted page address). */
#define EPT_PDPT_MASK           X86_PDPT_MASK_AMD64

/**
 * EPT Page Directory Pointer.
 */
typedef union EPTPDPTE
{
    /** Normal view. */
    EPTPDPTEBITS    n;
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** 64 bit unsigned integer view. */
    uint64_t        au64[1];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} EPTPDPTE;
AssertCompileSize(EPTPDPTE, 8);
/** Pointer to an EPT Page Directory Pointer Entry. */
typedef EPTPDPTE *PEPTPDPTE;
/** Pointer to a const EPT Page Directory Pointer Entry. */
typedef const EPTPDPTE *PCEPTPDPTE;

/**
 * EPT Page Directory Pointer Table.
 */
typedef struct EPTPDPT
{
    EPTPDPTE    a[EPT_PG_ENTRIES];
} EPTPDPT;
AssertCompileSize(EPTPDPT, 0x1000);
/** Pointer to an EPT Page Directory Pointer Table. */
typedef EPTPDPT *PEPTPDPT;
/** Pointer to a const EPT Page Directory Pointer Table. */
typedef const EPTPDPT *PCEPTPDPT;

/**
 * EPT Page Directory Table Entry. Bit view.
 */
typedef struct EPTPDEBITS
{
    /** Present bit. */
    uint64_t    u1Present       : 1;
    /** Writable bit. */
    uint64_t    u1Write         : 1;
    /** Executable bit. */
    uint64_t    u1Execute       : 1;
    /** Reserved (must be 0). */
    uint64_t    u4Reserved      : 4;
    /** Big page (must be 0 here). */
    uint64_t    u1Size          : 1;
    /** Available for software. */
    uint64_t    u4Available     : 4;
    /** Physical address of page table. Restricted by maximum physical address width of the cpu. */
    uint64_t    u40PhysAddr     : 40;
    /** Available for software. */
    uint64_t    u12Available    : 12;
} EPTPDEBITS;
AssertCompileSize(EPTPDEBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDE_PG_MASK         X86_PDE_PAE_PG_MASK
/** The page shift to get the PD index. */
#define EPT_PD_SHIFT            X86_PD_PAE_SHIFT
/** The PD index mask (apply to a shifted page address). */
#define EPT_PD_MASK             X86_PD_PAE_MASK

/**
 * EPT 2MB Page Directory Table Entry. Bit view.
 */
typedef struct EPTPDE2MBITS
{
    /** Present bit. */
    uint64_t    u1Present       : 1;
    /** Writable bit. */
    uint64_t    u1Write         : 1;
    /** Executable bit. */
    uint64_t    u1Execute       : 1;
    /** EPT Table Memory Type. MBZ for non-leaf nodes. */
    uint64_t    u3EMT           : 3;
    /** Ignore PAT memory type */
    uint64_t    u1IgnorePAT     : 1;
    /** Big page (must be 1 here). */
    uint64_t    u1Size          : 1;
    /** Available for software. */
    uint64_t    u4Available     : 4;
    /** Reserved (must be 0). */
    uint64_t    u9Reserved      : 9;
    /** Physical address of the 2MB page. Restricted by maximum physical address width of the cpu. */
    uint64_t    u31PhysAddr     : 31;
    /** Available for software. */
    uint64_t    u12Available    : 12;
} EPTPDE2MBITS;
AssertCompileSize(EPTPDE2MBITS, 8);

/** Bits 21-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDE2M_PG_MASK       X86_PDE2M_PAE_PG_MASK

/**
 * EPT Page Directory Table Entry.
 */
typedef union EPTPDE
{
    /** Normal view. */
    EPTPDEBITS      n;
    /** 2MB view (big). */
    EPTPDE2MBITS    b;
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** 64 bit unsigned integer view. */
    uint64_t        au64[1];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} EPTPDE;
AssertCompileSize(EPTPDE, 8);
/** Pointer to an EPT Page Directory Table Entry. */
typedef EPTPDE *PEPTPDE;
/** Pointer to a const EPT Page Directory Table Entry. */
typedef const EPTPDE *PCEPTPDE;

/**
 * EPT Page Directory Table.
 */
typedef struct EPTPD
{
    EPTPDE      a[EPT_PG_ENTRIES];
} EPTPD;
AssertCompileSize(EPTPD, 0x1000);
/** Pointer to an EPT Page Directory Table. */
typedef EPTPD *PEPTPD;
/** Pointer to a const EPT Page Directory Table. */
typedef const EPTPD *PCEPTPD;

/**
 * EPT Page Table Entry. Bit view.
 */
typedef struct EPTPTEBITS
{
    /** 0 - Present bit.
     * @remarks This is a convenience "misnomer". The bit actually indicates read access
     *          and the CPU will consider an entry with any of the first three bits set
     *          as present.  Since all our valid entries will have this bit set, it can
     *          be used as a present indicator and allow some code sharing. */
    uint64_t    u1Present       : 1;
    /** 1 - Writable bit. */
    uint64_t    u1Write         : 1;
    /** 2 - Executable bit. */
    uint64_t    u1Execute       : 1;
    /** 5:3 - EPT Memory Type. MBZ for non-leaf nodes. */
    uint64_t    u3EMT           : 3;
    /** 6 - Ignore PAT memory type */
    uint64_t    u1IgnorePAT     : 1;
    /** 11:7 - Available for software. */
    uint64_t    u5Available     : 5;
    /** 51:12 - Physical address of page. Restricted by maximum physical
     *  address width of the cpu. */
    uint64_t    u40PhysAddr     : 40;
    /** 63:52 - Available for software. */
    uint64_t    u12Available    : 12;
} EPTPTEBITS;
AssertCompileSize(EPTPTEBITS, 8);

/** Bits 12-51 - - EPT - Physical Page number of the next level. */
#define EPT_PTE_PG_MASK         X86_PTE_PAE_PG_MASK
/** The page shift to get the EPT PTE index. */
#define EPT_PT_SHIFT            X86_PT_PAE_SHIFT
/** The EPT PT index mask (apply to a shifted page address). */
#define EPT_PT_MASK             X86_PT_PAE_MASK

/**
 * EPT Page Table Entry.
 */
typedef union EPTPTE
{
    /** Normal view. */
    EPTPTEBITS      n;
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** 64 bit unsigned integer view. */
    uint64_t        au64[1];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} EPTPTE;
AssertCompileSize(EPTPTE, 8);
/** Pointer to an EPT Page Directory Table Entry. */
typedef EPTPTE *PEPTPTE;
/** Pointer to a const EPT Page Directory Table Entry. */
typedef const EPTPTE *PCEPTPTE;

/**
 * EPT Page Table.
 */
typedef struct EPTPT
{
    EPTPTE      a[EPT_PG_ENTRIES];
} EPTPT;
AssertCompileSize(EPTPT, 0x1000);
/** Pointer to an extended page table. */
typedef EPTPT *PEPTPT;
/** Pointer to a const extended table. */
typedef const EPTPT *PCEPTPT;

/** @} */

/**
 * VMX VPID flush types.
 * @note Valid enum members are in accordance to the VT-x spec.
 */
typedef enum
{
    /** Invalidate a specific page. */
    VMXTLBFLUSHVPID_INDIV_ADDR                 = 0,
    /** Invalidate one context (specific VPID). */
    VMXTLBFLUSHVPID_SINGLE_CONTEXT             = 1,
    /** Invalidate all contexts (all VPIDs). */
    VMXTLBFLUSHVPID_ALL_CONTEXTS               = 2,
    /** Invalidate a single VPID context retaining global mappings. */
    VMXTLBFLUSHVPID_SINGLE_CONTEXT_RETAIN_GLOBALS = 3,
    /** Unsupported by VirtualBox. */
    VMXTLBFLUSHVPID_NOT_SUPPORTED              = 0xbad0,
    /** Unsupported by CPU. */
    VMXTLBFLUSHVPID_NONE                       = 0xbad1
} VMXTLBFLUSHVPID;
AssertCompileSize(VMXTLBFLUSHVPID, 4);

/**
 * VMX EPT flush types.
 * @note Valid enums values are in accordance to the VT-x spec.
 */
typedef enum
{
    /** Invalidate one context (specific EPT). */
    VMXTLBFLUSHEPT_SINGLE_CONTEXT              = 1,
    /* Invalidate all contexts (all EPTs) */
    VMXTLBFLUSHEPT_ALL_CONTEXTS                = 2,
    /** Unsupported by VirtualBox.   */
    VMXTLBFLUSHEPT_NOT_SUPPORTED               = 0xbad0,
    /** Unsupported by CPU. */
    VMXTLBFLUSHEPT_NONE                        = 0xbad1
} VMXTLBFLUSHEPT;
AssertCompileSize(VMXTLBFLUSHEPT, 4);

/**
 * VMX Posted Interrupt Descriptor.
 * In accordance to the VT-x spec.
 */
typedef struct VMXPOSTEDINTRDESC
{
    uint32_t    aVectorBitmap[8];
    uint32_t    fOutstandingNotification : 1;
    uint32_t    uReserved0               : 31;
    uint8_t     au8Reserved0[28];
} VMXPOSTEDINTRDESC;
AssertCompileMemberSize(VMXPOSTEDINTRDESC, aVectorBitmap, 32);
AssertCompileSize(VMXPOSTEDINTRDESC, 64);
/** Pointer to a posted interrupt descriptor. */
typedef VMXPOSTEDINTRDESC *PVMXPOSTEDINTRDESC;
/** Pointer to a const posted interrupt descriptor. */
typedef const VMXPOSTEDINTRDESC *PCVMXPOSTEDINTRDESC;

/**
 * VMX VMCS revision identifier.
 */
typedef union
{
    struct
    {
        /** Revision identifier. */
        uint32_t    u31RevisionId : 31;
        /** Whether this is a shadow VMCS. */
        uint32_t    fIsShadowVmcs : 1;
    } n;
    /* The unsigned integer view. */
    uint32_t        u;
} VMXVMCSREVID;
AssertCompileSize(VMXVMCSREVID, 4);
/** Pointer to the VMXVMCSREVID union. */
typedef VMXVMCSREVID *PVMXVMCSREVID;
/** Pointer to a const VMXVVMCSREVID union. */
typedef const VMXVMCSREVID *PCVMXVMCSREVID;

/**
 * VMX VM-exit instruction information.
 */
typedef union
{
    /** Plain unsigned int representation. */
    uint32_t    u;

    /** INS and OUTS information. */
    struct
    {
        uint32_t    u7Reserved0 : 7;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize  : 3;
        uint32_t    u5Reserved1 : 5;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg     : 3;
        uint32_t    uReserved2  : 14;
    } StrIo;

    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Scaling       : 2;
        uint32_t    u5Undef0        : 5;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize      : 3;
        /** Cleared to 0. */
        uint32_t    u1Cleared0      : 1;
        uint32_t    u4Undef0        : 4;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg         : 3;
        /** The index register (X86_GREG_XXX). */
        uint32_t    iIdxReg         : 4;
        /** Set if index register is invalid. */
        uint32_t    fIdxRegInvalid  : 1;
        /** The base register (X86_GREG_XXX). */
        uint32_t    iBaseReg        : 4;
        /** Set if base register is invalid. */
        uint32_t    fBaseRegInvalid : 1;
        /** Register 2 (X86_GREG_XXX). */
        uint32_t    iReg2           : 4;
    } Inv;

    /** VMCLEAR, VMPTRLD, VMPTRST, VMXON, XRSTORS, XSAVES information. */
    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Scaling       : 2;
        uint32_t    u5Reserved0     : 5;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize      : 3;
        /** Cleared to 0. */
        uint32_t    u1Cleared0      : 1;
        uint32_t    u4Reserved0     : 4;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg         : 3;
        /** The index register (X86_GREG_XXX). */
        uint32_t    iIdxReg         : 4;
        /** Set if index register is invalid. */
        uint32_t    fIdxRegInvalid  : 1;
        /** The base register (X86_GREG_XXX). */
        uint32_t    iBaseReg        : 4;
        /** Set if base register is invalid. */
        uint32_t    fBaseRegInvalid : 1;
        /** Register 2 (X86_GREG_XXX). */
        uint32_t    iReg2           : 4;
    } VmxXsave;

    /** LIDT, LGDT, SIDT, SGDT information. */
    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Scaling       : 2;
        uint32_t    u5Undef0        : 5;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize      : 3;
        /** Always cleared to 0. */
        uint32_t    u1Cleared0      : 1;
        /** Operand size; 0=16-bit, 1=32-bit, undefined for 64-bit.  */
        uint32_t    uOperandSize    : 1;
        uint32_t    u3Undef0        : 3;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg         : 3;
        /** The index register (X86_GREG_XXX). */
        uint32_t    iIdxReg         : 4;
        /** Set if index register is invalid. */
        uint32_t    fIdxRegInvalid  : 1;
        /** The base register (X86_GREG_XXX). */
        uint32_t    iBaseReg        : 4;
        /** Set if base register is invalid. */
        uint32_t    fBaseRegInvalid : 1;
        /** Instruction identity (VMX_INSTR_ID_XXX). */
        uint32_t    u2InstrId       : 2;
        uint32_t    u2Undef0        : 2;
    } GdtIdt;

    /** LLDT, LTR, SLDT, STR information. */
    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Scaling       : 2;
        uint32_t    u1Undef0        : 1;
        /** Register 1 (X86_GREG_XXX). */
        uint32_t    iReg1           : 4;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize      : 3;
        /** Memory/Register - Always cleared to 0 to indicate memory operand. */
        uint32_t    fIsRegOperand   : 1;
        uint32_t    u4Undef0        : 4;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg         : 3;
        /** The index register (X86_GREG_XXX). */
        uint32_t    iIdxReg         : 4;
        /** Set if index register is invalid. */
        uint32_t    fIdxRegInvalid  : 1;
        /** The base register (X86_GREG_XXX). */
        uint32_t    iBaseReg        : 4;
        /** Set if base register is invalid. */
        uint32_t    fBaseRegInvalid : 1;
        /** Instruction identity (VMX_INSTR_ID_XXX). */
        uint32_t    u2InstrId       : 2;
        uint32_t    u2Undef0        : 2;
    } LdtTr;

    /** RDRAND, RDSEED information. */
    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Undef0        : 2;
        /** Destination register (X86_GREG_XXX). */
        uint32_t    iReg1           : 4;
        uint32_t    u4Undef0        : 4;
        /** Operand size; 0=16-bit, 1=32-bit, 2=64-bit, 3=unused.  */
        uint32_t    u2OperandSize   : 2;
        uint32_t    u19Def0         : 20;
    } RdrandRdseed;

    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Scaling       : 2;
        uint32_t    u1Undef0        : 1;
        /** Register 1 (X86_GREG_XXX). */
        uint32_t    iReg1           : 4;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize      : 3;
        /** Memory or register operand. */
        uint32_t    fIsRegOperand   : 1;
        /** Operand size; 0=16-bit, 1=32-bit, 2=64-bit, 3=unused.  */
        uint32_t    u4Undef0        : 4;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg         : 3;
        /** The index register (X86_GREG_XXX). */
        uint32_t    iIdxReg         : 4;
        /** Set if index register is invalid. */
        uint32_t    fIdxRegInvalid  : 1;
        /** The base register (X86_GREG_XXX). */
        uint32_t    iBaseReg        : 4;
        /** Set if base register is invalid. */
        uint32_t    fBaseRegInvalid : 1;
        /** Register 2 (X86_GREG_XXX). */
        uint32_t    iReg2           : 4;
    } VmreadVmwrite;

    /** This is a combination field of all instruction information. Note! Not all field
     *  combinations are valid (e.g., iReg1 is undefined for memory operands) and
     *  specialized fields are overwritten by their generic counterparts (e.g. no
     *  instruction identity field). */
    struct
    {
        /** Scaling; 0=no scaling, 1=scale-by-2, 2=scale-by-4, 3=scale-by-8. */
        uint32_t    u2Scaling       : 2;
        uint32_t    u1Undef0        : 1;
        /** Register 1 (X86_GREG_XXX). */
        uint32_t    iReg1           : 4;
        /** The address size; 0=16-bit, 1=32-bit, 2=64-bit, rest undefined. */
        uint32_t    u3AddrSize      : 3;
        /** Memory/Register - Always cleared to 0 to indicate memory operand. */
        uint32_t    fIsRegOperand   : 1;
        /** Operand size; 0=16-bit, 1=32-bit, 2=64-bit, 3=unused.  */
        uint32_t    uOperandSize    : 2;
        uint32_t    u2Undef0        : 2;
        /** The segment register (X86_SREG_XXX). */
        uint32_t    iSegReg         : 3;
        /** The index register (X86_GREG_XXX). */
        uint32_t    iIdxReg         : 4;
        /** Set if index register is invalid. */
        uint32_t    fIdxRegInvalid  : 1;
        /** The base register (X86_GREG_XXX). */
        uint32_t    iBaseReg        : 4;
        /** Set if base register is invalid. */
        uint32_t    fBaseRegInvalid : 1;
        /** Register 2 (X86_GREG_XXX) or instruction identity. */
        uint32_t    iReg2           : 4;
    } All;
} VMXEXITINSTRINFO;
AssertCompileSize(VMXEXITINSTRINFO, 4);
/** Pointer to a VMX VM-exit instruction info. struct. */
typedef VMXEXITINSTRINFO *PVMXEXITINSTRINFO;
/** Pointer to a const VMX VM-exit instruction info. struct. */
typedef const VMXEXITINSTRINFO *PCVMXEXITINSTRINFO;


/** @name VM-entry failure reported in VM-exit qualification.
 * See Intel spec. 26.7 "VM-entry failures during or after loading guest-state".
 */
/** No errors during VM-entry. */
#define VMX_ENTRY_FAIL_QUAL_NO_ERROR                            (0)
/** Not used. */
#define VMX_ENTRY_FAIL_QUAL_NOT_USED                            (1)
/** Error while loading PDPTEs. */
#define VMX_ENTRY_FAIL_QUAL_PDPTE                               (2)
/** NMI injection when blocking-by-STI is set. */
#define VMX_ENTRY_FAIL_QUAL_NMI_INJECT                          (3)
/** Invalid VMCS link pointer. */
#define VMX_ENTRY_FAIL_QUAL_VMCS_LINK_PTR                       (4)
/** @} */


/**
 * VMX MSR autoload/store element.
 * In accordance to the VT-x spec.
 */
typedef struct VMXAUTOMSR
{
    /** The MSR Id. */
    uint32_t    u32Msr;
    /** Reserved (MBZ). */
    uint32_t    u32Reserved;
    /** The MSR value. */
    uint64_t    u64Value;
} VMXAUTOMSR;
AssertCompileSize(VMXAUTOMSR, 16);
/** Pointer to an MSR load/store element. */
typedef VMXAUTOMSR *PVMXAUTOMSR;
/** Pointer to a const MSR load/store element. */
typedef const VMXAUTOMSR *PCVMXAUTOMSR;

/** VMX auto load-store MSR (VMXAUTOMSR) offset mask. */
#define VMX_AUTOMSR_OFFSET_MASK         0xf

/**
 * VMX tagged-TLB flush types.
 */
typedef enum
{
    VMXTLBFLUSHTYPE_EPT,
    VMXTLBFLUSHTYPE_VPID,
    VMXTLBFLUSHTYPE_EPT_VPID,
    VMXTLBFLUSHTYPE_NONE
} VMXTLBFLUSHTYPE;
/** Pointer to a VMXTLBFLUSHTYPE enum. */
typedef VMXTLBFLUSHTYPE *PVMXTLBFLUSHTYPE;
/** Pointer to a const VMXTLBFLUSHTYPE enum. */
typedef const VMXTLBFLUSHTYPE *PCVMXTLBFLUSHTYPE;

/**
 * VMX controls MSR.
 */
typedef union
{
    struct
    {
        /** Bits set here -must- be set in the corresponding VM-execution controls. */
        uint32_t        disallowed0;
        /** Bits cleared here -must- be cleared in the corresponding VM-execution
         *  controls. */
        uint32_t        allowed1;
    } n;
    uint64_t            u;
} VMXCTLSMSR;
AssertCompileSize(VMXCTLSMSR, 8);
/** Pointer to a VMXCTLSMSR union. */
typedef VMXCTLSMSR *PVMXCTLSMSR;
/** Pointer to a const VMXCTLSMSR union. */
typedef const VMXCTLSMSR *PCVMXCTLSMSR;

/**
 * VMX MSRs.
 * @remarks Although treated as a plain-old data (POD) in several places, please
 *          update HMVmxGetHostMsr() if new MSRs are added here.
 */
typedef struct VMXMSRS
{
    uint64_t        u64FeatCtrl;
    uint64_t        u64Basic;
    VMXCTLSMSR      PinCtls;
    VMXCTLSMSR      ProcCtls;
    VMXCTLSMSR      ProcCtls2;
    VMXCTLSMSR      ExitCtls;
    VMXCTLSMSR      EntryCtls;
    VMXCTLSMSR      TruePinCtls;
    VMXCTLSMSR      TrueProcCtls;
    VMXCTLSMSR      TrueEntryCtls;
    VMXCTLSMSR      TrueExitCtls;
    uint64_t        u64Misc;
    uint64_t        u64Cr0Fixed0;
    uint64_t        u64Cr0Fixed1;
    uint64_t        u64Cr4Fixed0;
    uint64_t        u64Cr4Fixed1;
    uint64_t        u64VmcsEnum;
    uint64_t        u64VmFunc;
    uint64_t        u64EptVpidCaps;
    uint64_t        a_u64Reserved[2];
} VMXMSRS;
AssertCompileSizeAlignment(VMXMSRS, 8);
AssertCompileSize(VMXMSRS, 168);
/** Pointer to a VMXMSRS struct. */
typedef VMXMSRS *PVMXMSRS;
/** Pointer to a const VMXMSRS struct. */
typedef const VMXMSRS *PCVMXMSRS;


/** @name VMX Basic Exit Reasons.
 * @{
 */
/** -1 Invalid exit code */
#define VMX_EXIT_INVALID                                      (-1)
/** 0 Exception or non-maskable interrupt (NMI). */
#define VMX_EXIT_XCPT_OR_NMI                                    0
/** 1 External interrupt. */
#define VMX_EXIT_EXT_INT                                        1
/** 2 Triple fault. */
#define VMX_EXIT_TRIPLE_FAULT                                   2
/** 3 INIT signal. */
#define VMX_EXIT_INIT_SIGNAL                                    3
/** 4 Start-up IPI (SIPI). */
#define VMX_EXIT_SIPI                                           4
/** 5 I/O system-management interrupt (SMI). */
#define VMX_EXIT_IO_SMI                                         5
/** 6 Other SMI. */
#define VMX_EXIT_SMI                                            6
/** 7 Interrupt window exiting. */
#define VMX_EXIT_INT_WINDOW                                     7
/** 8 NMI window exiting. */
#define VMX_EXIT_NMI_WINDOW                                     8
/** 9 Task switch. */
#define VMX_EXIT_TASK_SWITCH                                    9
/** 10 Guest software attempted to execute CPUID. */
#define VMX_EXIT_CPUID                                          10
/** 11 Guest software attempted to execute GETSEC. */
#define VMX_EXIT_GETSEC                                         11
/** 12 Guest software attempted to execute HLT. */
#define VMX_EXIT_HLT                                            12
/** 13 Guest software attempted to execute INVD. */
#define VMX_EXIT_INVD                                           13
/** 14 Guest software attempted to execute INVLPG. */
#define VMX_EXIT_INVLPG                                         14
/** 15 Guest software attempted to execute RDPMC. */
#define VMX_EXIT_RDPMC                                          15
/** 16 Guest software attempted to execute RDTSC. */
#define VMX_EXIT_RDTSC                                          16
/** 17 Guest software attempted to execute RSM in SMM. */
#define VMX_EXIT_RSM                                            17
/** 18 Guest software executed VMCALL. */
#define VMX_EXIT_VMCALL                                         18
/** 19 Guest software executed VMCLEAR. */
#define VMX_EXIT_VMCLEAR                                        19
/** 20 Guest software executed VMLAUNCH. */
#define VMX_EXIT_VMLAUNCH                                       20
/** 21 Guest software executed VMPTRLD. */
#define VMX_EXIT_VMPTRLD                                        21
/** 22 Guest software executed VMPTRST. */
#define VMX_EXIT_VMPTRST                                        22
/** 23 Guest software executed VMREAD. */
#define VMX_EXIT_VMREAD                                         23
/** 24 Guest software executed VMRESUME. */
#define VMX_EXIT_VMRESUME                                       24
/** 25 Guest software executed VMWRITE. */
#define VMX_EXIT_VMWRITE                                        25
/** 26 Guest software executed VMXOFF. */
#define VMX_EXIT_VMXOFF                                         26
/** 27 Guest software executed VMXON. */
#define VMX_EXIT_VMXON                                          27
/** 28 Control-register accesses. */
#define VMX_EXIT_MOV_CRX                                        28
/** 29 Debug-register accesses. */
#define VMX_EXIT_MOV_DRX                                        29
/** 30 I/O instruction. */
#define VMX_EXIT_IO_INSTR                                       30
/** 31 RDMSR. Guest software attempted to execute RDMSR. */
#define VMX_EXIT_RDMSR                                          31
/** 32 WRMSR. Guest software attempted to execute WRMSR. */
#define VMX_EXIT_WRMSR                                          32
/** 33 VM-entry failure due to invalid guest state. */
#define VMX_EXIT_ERR_INVALID_GUEST_STATE                        33
/** 34 VM-entry failure due to MSR loading. */
#define VMX_EXIT_ERR_MSR_LOAD                                   34
/** 36 Guest software executed MWAIT. */
#define VMX_EXIT_MWAIT                                          36
/** 37 VM-exit due to monitor trap flag. */
#define VMX_EXIT_MTF                                            37
/** 39 Guest software attempted to execute MONITOR. */
#define VMX_EXIT_MONITOR                                        39
/** 40 Guest software attempted to execute PAUSE. */
#define VMX_EXIT_PAUSE                                          40
/** 41 VM-entry failure due to machine-check. */
#define VMX_EXIT_ERR_MACHINE_CHECK                              41
/** 43 TPR below threshold. Guest software executed MOV to CR8. */
#define VMX_EXIT_TPR_BELOW_THRESHOLD                            43
/** 44 APIC access. Guest software attempted to access memory at a physical
 *  address on the APIC-access page. */
#define VMX_EXIT_APIC_ACCESS                                    44
/** 45 Virtualized EOI. EOI virtualization was performed for a virtual
 *  interrupt whose vector indexed a bit set in the EOI-exit bitmap. */
#define VMX_EXIT_VIRTUALIZED_EOI                                45
/** 46 Access to GDTR or IDTR. Guest software attempted to execute LGDT, LIDT,
 *  SGDT, or SIDT. */
#define VMX_EXIT_GDTR_IDTR_ACCESS                               46
/** 47 Access to LDTR or TR. Guest software attempted to execute LLDT, LTR,
 *  SLDT, or STR. */
#define VMX_EXIT_LDTR_TR_ACCESS                                 47
/** 48 EPT violation. An attempt to access memory with a guest-physical address
 *  was disallowed by the configuration of the EPT paging structures. */
#define VMX_EXIT_EPT_VIOLATION                                  48
/** 49 EPT misconfiguration. An attempt to access memory with a guest-physical
 *  address encountered a misconfigured EPT paging-structure entry. */
#define VMX_EXIT_EPT_MISCONFIG                                  49
/** 50 INVEPT. Guest software attempted to execute INVEPT. */
#define VMX_EXIT_INVEPT                                         50
/** 51 RDTSCP. Guest software attempted to execute RDTSCP. */
#define VMX_EXIT_RDTSCP                                         51
/** 52 VMX-preemption timer expired. The preemption timer counted down to zero. */
#define VMX_EXIT_PREEMPT_TIMER                                  52
/** 53 INVVPID. Guest software attempted to execute INVVPID. */
#define VMX_EXIT_INVVPID                                        53
/** 54 WBINVD. Guest software attempted to execute WBINVD. */
#define VMX_EXIT_WBINVD                                         54
/** 55 XSETBV. Guest software attempted to execute XSETBV. */
#define VMX_EXIT_XSETBV                                         55
/** 56 APIC write. Guest completed write to virtual-APIC. */
#define VMX_EXIT_APIC_WRITE                                     56
/** 57 RDRAND. Guest software attempted to execute RDRAND. */
#define VMX_EXIT_RDRAND                                         57
/** 58 INVPCID. Guest software attempted to execute INVPCID. */
#define VMX_EXIT_INVPCID                                        58
/** 59 VMFUNC. Guest software attempted to execute VMFUNC. */
#define VMX_EXIT_VMFUNC                                         59
/** 60 ENCLS. Guest software attempted to execute ENCLS. */
#define VMX_EXIT_ENCLS                                          60
/** 61 - RDSEED - Guest software attempted to executed RDSEED and exiting was
 * enabled. */
#define VMX_EXIT_RDSEED                                         61
/** 62 - Page-modification log full. */
#define VMX_EXIT_PML_FULL                                       62
/** 63 - XSAVES - Guest software attempted to executed XSAVES and exiting was
 * enabled (XSAVES/XRSTORS was enabled too, of course). */
#define VMX_EXIT_XSAVES                                         63
/** 63 - XRSTORS - Guest software attempted to executed XRSTORS and exiting
 * was enabled (XSAVES/XRSTORS was enabled too, of course). */
#define VMX_EXIT_XRSTORS                                        64
/** The maximum exit value (inclusive). */
#define VMX_EXIT_MAX                                            (VMX_EXIT_XRSTORS)
/** @} */


/** @name VM Instruction Errors.
 * See Intel spec. "30.4 VM Instruction Error Numbers"
 * @{
 */
typedef enum
{
    /** VMCALL executed in VMX root operation. */
    VMXINSTRERR_VMCALL_VMXROOTMODE             = 1,
    /** VMCLEAR with invalid physical address. */
    VMXINSTRERR_VMCLEAR_INVALID_PHYSADDR       = 2,
    /** VMCLEAR with VMXON pointer. */
    VMXINSTRERR_VMCLEAR_VMXON_PTR              = 3,
    /** VMLAUNCH with non-clear VMCS. */
    VMXINSTRERR_VMLAUNCH_NON_CLEAR_VMCS        = 4,
    /** VMRESUME with non-launched VMCS. */
    VMXINSTRERR_VMRESUME_NON_LAUNCHED_VMCS     = 5,
    /** VMRESUME after VMXOFF (VMXOFF and VMXON between VMLAUNCH and VMRESUME). */
    VMXINSTRERR_VMRESUME_AFTER_VMXOFF          = 6,
    /** VM-entry with invalid control field(s). */
    VMXINSTRERR_VMENTRY_INVALID_CTLS           = 7,
    /** VM-entry with invalid host-state field(s). */
    VMXINSTRERR_VMENTRY_INVALID_HOST_STATE     = 8,
    /** VMPTRLD with invalid physical address. */
    VMXINSTRERR_VMPTRLD_INVALID_PHYSADDR       = 9,
    /** VMPTRLD with VMXON pointer. */
    VMXINSTRERR_VMPTRLD_VMXON_PTR              = 10,
    /** VMPTRLD with incorrect VMCS revision identifier. */
    VMXINSTRERR_VMPTRLD_INCORRECT_VMCS_REV     = 11,
    /** VMREAD from unsupported VMCS component. */
    VMXINSTRERR_VMREAD_INVALID_COMPONENT       = 12,
    /** VMWRITE to unsupported VMCS component. */
    VMXINSTRERR_VMWRITE_INVALID_COMPONENT      = 12,
    /** VMWRITE to read-only VMCS component. */
    VMXINSTRERR_VMWRITE_RO_COMPONENT           = 13,
    /** VMXON executed in VMX root operation. */
    VMXINSTRERR_VMXON_IN_VMXROOTMODE           = 15,
    /** VM-entry with invalid executive-VMCS pointer. */
    VMXINSTRERR_VMENTRY_EXEC_VMCS_INVALID_PTR  = 16,
    /** VM-entry with non-launched executive VMCS. */
    VMXINSTRERR_VMENTRY_EXEC_VMCS_NON_LAUNCHED = 17,
    /** VM-entry with executive-VMCS pointer not VMXON pointer. */
    VMXINSTRERR_VMENTRY_EXEC_VMCS_PTR          = 18,
    /** VMCALL with non-clear VMCS. */
    VMXINSTRERR_VMCALL_NON_CLEAR_VMCS          = 19,
    /** VMCALL with invalid VM-exit control fields. */
    VMXINSTRERR_VMCALL_INVALID_EXITCTLS        = 20,
    /** VMCALL with incorrect MSEG revision identifier. */
    VMXINSTRERR_VMCALL_INVALID_MSEG_ID         = 22,
    /** VMXOFF under dual-monitor treatment of SMIs and SMM. */
    VMXINSTRERR_VMXOFF_DUAL_MON                = 23,
    /** VMCALL with invalid SMM-monitor features. */
    VMXINSTRERR_VMCALL_INVALID_SMMCTLS         = 24,
    /** VM-entry with invalid VM-execution control fields in executive VMCS. */
    VMXINSTRERR_VMENTRY_EXEC_VMCS_INVALID_CTLS = 25,
    /** VM-entry with events blocked by MOV SS. */
    VMXINSTRERR_VMENTRY_BLOCK_MOVSS            = 26,
    /** Invalid operand to INVEPT/INVVPID. */
    VMXINSTRERR_INVEPT_INVVPID_INVALID_OPERAND = 28
} VMXINSTRERR;
/** @} */


/** @name VMX abort reasons.
 * See Intel spec. "27.7 VMX Aborts".
 * Update HMVmxGetAbortDesc() if new reasons are added.
 * @{
 */
typedef enum
{
    /** None - don't use this / uninitialized value. */
    VMXABORT_NONE                  = 0,
    /** VMX abort caused during saving of guest MSRs. */
    VMXABORT_SAVE_GUEST_MSRS       = 1,
    /** VMX abort caused during host PDPTE checks. */
    VMXBOART_HOST_PDPTE            = 2,
    /** VMX abort caused due to current VMCS being corrupted. */
    VMXABORT_CURRENT_VMCS_CORRUPT  = 3,
    /** VMX abort caused during loading of host MSRs. */
    VMXABORT_LOAD_HOST_MSR         = 4,
    /** VMX abort caused due to a machine-check exception during VM-exit. */
    VMXABORT_MACHINE_CHECK_XCPT    = 5,
    /** VMX abort caused due to invalid return from long mode. */
    VMXABORT_HOST_NOT_IN_LONG_MODE = 6,
    /* Type size hack. */
    VMXABORT_32BIT_HACK            = 0x7fffffff
} VMXABORT;
AssertCompileSize(VMXABORT, 4);
/** @} */


/** @name VMX MSR - Basic VMX information.
 * @{
 */
/** VMCS (and related regions) memory type - Uncacheable. */
#define VMX_BASIC_MEM_TYPE_UC                                   0
/** VMCS (and related regions) memory type - Write back. */
#define VMX_BASIC_MEM_TYPE_WB                                   6

/** Bit fields for MSR_IA32_VMX_BASIC.  */
/** VMCS revision identifier used by the processor. */
#define VMX_BF_BASIC_VMCS_ID_SHIFT                              0
#define VMX_BF_BASIC_VMCS_ID_MASK                               UINT64_C(0x000000007fffffff)
/** Bit 31 is reserved and RAZ. */
#define VMX_BF_BASIC_RSVD_32_SHIFT                              31
#define VMX_BF_BASIC_RSVD_32_MASK                               UINT64_C(0x0000000080000000)
/** VMCS size in bytes. */
#define VMX_BF_BASIC_VMCS_SIZE_SHIFT                            32
#define VMX_BF_BASIC_VMCS_SIZE_MASK                             UINT64_C(0x00001fff00000000)
/** Bits 45:47 are reserved. */
#define VMX_BF_BASIC_RSVD_45_47_SHIFT                           45
#define VMX_BF_BASIC_RSVD_45_47_MASK                            UINT64_C(0x0000e00000000000)
/** Width of physical addresses used for the VMCS and associated memory regions
 *  (always 0 on CPUs that support Intel 64 architecture). */
#define VMX_BF_BASIC_PHYSADDR_WIDTH_SHIFT                       48
#define VMX_BF_BASIC_PHYSADDR_WIDTH_MASK                        UINT64_C(0x0001000000000000)
/** Dual-monitor treatment of SMI and SMM supported. */
#define VMX_BF_BASIC_DUAL_MON_SHIFT                             49
#define VMX_BF_BASIC_DUAL_MON_MASK                              UINT64_C(0x0002000000000000)
/** Memory type that must be used for the VMCS and associated memory regions. */
#define VMX_BF_BASIC_VMCS_MEM_TYPE_SHIFT                        50
#define VMX_BF_BASIC_VMCS_MEM_TYPE_MASK                         UINT64_C(0x003c000000000000)
/** VM-exit instruction information for INS/OUTS. */
#define VMX_BF_BASIC_VMCS_INS_OUTS_SHIFT                        54
#define VMX_BF_BASIC_VMCS_INS_OUTS_MASK                         UINT64_C(0x0040000000000000)
/** Whether 'true' VMX controls MSRs are supported for handling of default1 class
 *  bits in VMX control MSRs. */
#define VMX_BF_BASIC_TRUE_CTLS_SHIFT                            55
#define VMX_BF_BASIC_TRUE_CTLS_MASK                             UINT64_C(0x0080000000000000)
/** Bits 56:63 are reserved and RAZ. */
#define VMX_BF_BASIC_RSVD_56_63_SHIFT                           56
#define VMX_BF_BASIC_RSVD_56_63_MASK                            UINT64_C(0xff00000000000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_BASIC_, UINT64_C(0), UINT64_MAX,
                            (VMCS_ID, RSVD_32, VMCS_SIZE, RSVD_45_47, PHYSADDR_WIDTH, DUAL_MON, VMCS_MEM_TYPE,
                             VMCS_INS_OUTS, TRUE_CTLS, RSVD_56_63));
/** @} */


/** @name VMX MSR - Miscellaneous data.
 * Bit fields for MSR_IA32_VMX_MISC.
 * @{
 */
/** Whether VM-exit stores EFER.LMA into the "IA32e mode guest" field. */
#define VMX_MISC_EXIT_STORE_EFER_LMA                            RT_BIT(5)
/** Whether VMWRITE to any valid VMCS field incl. read-only fields, otherwise
 * VMWRITE cannot modify read-only VM-exit information fields. */
#define VMX_MISC_VMWRITE_ALL                                    RT_BIT(29)
/** Whether VM-entry can inject software interrupts, INT1 (ICEBP) with 0-length
 *  instructions. */
#define VMX_MISC_ENTRY_INJECT_SOFT_INT                          RT_BIT(30)
/** Maximum number of MSRs in the auto-load/store MSR areas, (n+1) * 512. */
#define VMX_MISC_MAX_MSRS(a_MiscMsr)                            (512 * (RT_BF_GET((a_MiscMsr), VMX_BF_MISC_MAX_MSRS) + 1))
/** Maximum CR3-target count supported by the CPU. */
#define VMX_MISC_CR3_TARGET_COUNT(a_MiscMsr)                    (((a) >> 16) & 0xff)
/** Relationship between the preemption timer and tsc. */
#define VMX_BF_MISC_PREEMPT_TIMER_TSC_SHIFT                     0
#define VMX_BF_MISC_PREEMPT_TIMER_TSC_MASK                      UINT64_C(0x000000000000001f)
/** Whether VM-exit stores EFER.LMA into the "IA32e mode guest" field. */
#define VMX_BF_MISC_EXIT_STORE_EFER_LMA_SHIFT                   5
#define VMX_BF_MISC_EXIT_STORE_EFER_LMA_MASK                    UINT64_C(0x0000000000000020)
/** Activity states supported by the implementation. */
#define VMX_BF_MISC_ACTIVITY_STATES_SHIFT                       6
#define VMX_BF_MISC_ACTIVITY_STATES_MASK                        UINT64_C(0x00000000000001c0)
/** Bits 9:13 is reserved and RAZ. */
#define VMX_BF_MISC_RSVD_9_13_SHIFT                             9
#define VMX_BF_MISC_RSVD_9_13_MASK                              UINT64_C(0x0000000000003e00)
/** Whether Intel PT (Processor Trace) can be used in VMX operation.  */
#define VMX_BF_MISC_PT_SHIFT                                    14
#define VMX_BF_MISC_PT_MASK                                     UINT64_C(0x0000000000004000)
/** Whether RDMSR can be used to read IA32_SMBASE MSR in SMM. */
#define VMX_BF_MISC_SMM_READ_SMBASE_MSR_SHIFT                   15
#define VMX_BF_MISC_SMM_READ_SMBASE_MSR_MASK                    UINT64_C(0x0000000000008000)
/** Number of CR3 target values supported by the processor. (0-256) */
#define VMX_BF_MISC_CR3_TARGET_SHIFT                            16
#define VMX_BF_MISC_CR3_TARGET_MASK                             UINT64_C(0x0000000001ff0000)
/** Maximum number of MSRs in the VMCS. */
#define VMX_BF_MISC_MAX_MSRS_SHIFT                              25
#define VMX_BF_MISC_MAX_MSRS_MASK                               UINT64_C(0x000000000e000000)
/** Whether IA32_SMM_MONITOR_CTL MSR can be modified to allow VMXOFF to block
 *  SMIs. */
#define VMX_BF_MISC_VMXOFF_BLOCK_SMI_SHIFT                      28
#define VMX_BF_MISC_VMXOFF_BLOCK_SMI_MASK                       UINT64_C(0x0000000010000000)
/** Whether VMWRITE to any valid VMCS field incl. read-only fields, otherwise
 * VMWRITE cannot modify read-only VM-exit information fields. */
#define VMX_BF_MISC_VMWRITE_ALL_SHIFT                           29
#define VMX_BF_MISC_VMWRITE_ALL_MASK                            UINT64_C(0x0000000020000000)
/** Whether VM-entry can inject software interrupts, INT1 (ICEBP) with 0-length
 *  instructions. */
#define VMX_BF_MISC_ENTRY_INJECT_SOFT_INT_SHIFT                 30
#define VMX_BF_MISC_ENTRY_INJECT_SOFT_INT_MASK                  UINT64_C(0x0000000040000000)
/** Bit 31 is reserved and RAZ. */
#define VMX_BF_MISC_RSVD_31_SHIFT                               31
#define VMX_BF_MISC_RSVD_31_MASK                                UINT64_C(0x0000000080000000)
/** 32-bit MSEG revision ID used by the processor. */
#define VMX_BF_MISC_MSEG_ID_SHIFT                               32
#define VMX_BF_MISC_MSEG_ID_MASK                                UINT64_C(0xffffffff00000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_MISC_, UINT64_C(0), UINT64_MAX,
                            (PREEMPT_TIMER_TSC, EXIT_STORE_EFER_LMA, ACTIVITY_STATES, RSVD_9_13, PT, SMM_READ_SMBASE_MSR,
                             CR3_TARGET, MAX_MSRS, VMXOFF_BLOCK_SMI, VMWRITE_ALL, ENTRY_INJECT_SOFT_INT, RSVD_31, MSEG_ID));
/** @} */

/** @name VMX MSR - VMCS enumeration.
 * Bit fields for MSR_IA32_VMX_VMCS_ENUM.
 * @{
 */
/** Bit 0 is reserved and RAZ.  */
#define VMX_BF_VMCS_ENUM_RSVD_0_SHIFT                           0
#define VMX_BF_VMCS_ENUM_RSVD_0_MASK                            UINT64_C(0x0000000000000001)
/** Highest index value used in VMCS field encoding. */
#define VMX_BF_VMCS_ENUM_HIGHEST_IDX_SHIFT                      1
#define VMX_BF_VMCS_ENUM_HIGHEST_IDX_MASK                       UINT64_C(0x00000000000003fe)
/** Bit 10:63 is reserved and RAZ.  */
#define VMX_BF_VMCS_ENUM_RSVD_10_63_SHIFT                       10
#define VMX_BF_VMCS_ENUM_RSVD_10_63_MASK                        UINT64_C(0xfffffffffffffc00)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_VMCS_ENUM_, UINT64_C(0), UINT64_MAX,
                            (RSVD_0, HIGHEST_IDX, RSVD_10_63));
/** @} */


/** @name VMX MSR - VM Functions.
 * Bit fields for MSR_IA32_VMX_VMFUNC.
 * @{
 */
/** EPTP-switching function changes the value of the EPTP to one chosen from the EPTP list. */
#define VMX_BF_VMFUNC_EPTP_SWITCHING_SHIFT                      0
#define VMX_BF_VMFUNC_EPTP_SWITCHING_MASK                       UINT64_C(0x0000000000000001)
/** Bits 1:63 are reserved and RAZ. */
#define VMX_BF_VMFUNC_RSVD_1_63_SHIFT                           1
#define VMX_BF_VMFUNC_RSVD_1_63_MASK                            UINT64_C(0xfffffffffffffffe)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_VMFUNC_, UINT64_C(0), UINT64_MAX,
                            (EPTP_SWITCHING, RSVD_1_63));
/** @} */


/** @name VMX MSR - EPT/VPID capabilities.
 * @{
 */
#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_X_ONLY                    RT_BIT_64(0)
#define MSR_IA32_VMX_EPT_VPID_CAP_PAGE_WALK_LENGTH_4            RT_BIT_64(6)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_UC                        RT_BIT_64(8)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB                        RT_BIT_64(14)
#define MSR_IA32_VMX_EPT_VPID_CAP_PDE_2M                        RT_BIT_64(16)
#define MSR_IA32_VMX_EPT_VPID_CAP_PDPTE_1G                      RT_BIT_64(17)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT                        RT_BIT_64(20)
#define MSR_IA32_VMX_EPT_VPID_CAP_EPT_ACCESS_DIRTY              RT_BIT_64(21)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT         RT_BIT_64(25)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS           RT_BIT_64(26)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID                       RT_BIT_64(32)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR            RT_BIT_64(40)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT        RT_BIT_64(41)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS          RT_BIT_64(42)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS  RT_BIT_64(43)
/** @} */


/** @name Extended Page Table Pointer (EPTP)
 * @{
 */
/** Uncachable EPT paging structure memory type. */
#define VMX_EPT_MEMTYPE_UC                                      0
/** Write-back EPT paging structure memory type. */
#define VMX_EPT_MEMTYPE_WB                                      6
/** Shift value to get the EPT page walk length (bits 5-3) */
#define VMX_EPT_PAGE_WALK_LENGTH_SHIFT                          3
/** Mask value to get the EPT page walk length (bits 5-3) */
#define VMX_EPT_PAGE_WALK_LENGTH_MASK                           7
/** Default EPT page-walk length (1 less than the actual EPT page-walk
 *  length) */
#define VMX_EPT_PAGE_WALK_LENGTH_DEFAULT                        3
/** @} */


/** @name VMCS field encoding: 16-bit guest fields.
 * @{
 */
#define VMX_VMCS16_VPID                                         0x0000
#define VMX_VMCS16_POSTED_INT_NOTIFY_VECTOR                     0x0002
#define VMX_VMCS16_EPTP_INDEX                                   0x0004
#define VMX_VMCS16_GUEST_ES_SEL                                 0x0800
#define VMX_VMCS16_GUEST_CS_SEL                                 0x0802
#define VMX_VMCS16_GUEST_SS_SEL                                 0x0804
#define VMX_VMCS16_GUEST_DS_SEL                                 0x0806
#define VMX_VMCS16_GUEST_FS_SEL                                 0x0808
#define VMX_VMCS16_GUEST_GS_SEL                                 0x080a
#define VMX_VMCS16_GUEST_LDTR_SEL                               0x080c
#define VMX_VMCS16_GUEST_TR_SEL                                 0x080e
#define VMX_VMCS16_GUEST_INTR_STATUS                            0x0810
#define VMX_VMCS16_GUEST_PML_INDEX                              0x0812
/** @} */


/** @name VMCS field encoding: 16-bits host fields.
 * @{
 */
#define VMX_VMCS16_HOST_ES_SEL                                  0x0c00
#define VMX_VMCS16_HOST_CS_SEL                                  0x0c02
#define VMX_VMCS16_HOST_SS_SEL                                  0x0c04
#define VMX_VMCS16_HOST_DS_SEL                                  0x0c06
#define VMX_VMCS16_HOST_FS_SEL                                  0x0c08
#define VMX_VMCS16_HOST_GS_SEL                                  0x0c0a
#define VMX_VMCS16_HOST_TR_SEL                                  0x0c0c
/** @} */


/** @name VMCS field encoding: 64-bit control fields.
 * @{
 */
#define VMX_VMCS64_CTRL_IO_BITMAP_A_FULL                        0x2000
#define VMX_VMCS64_CTRL_IO_BITMAP_A_HIGH                        0x2001
#define VMX_VMCS64_CTRL_IO_BITMAP_B_FULL                        0x2002
#define VMX_VMCS64_CTRL_IO_BITMAP_B_HIGH                        0x2003
#define VMX_VMCS64_CTRL_MSR_BITMAP_FULL                         0x2004
#define VMX_VMCS64_CTRL_MSR_BITMAP_HIGH                         0x2005
#define VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL                     0x2006
#define VMX_VMCS64_CTRL_EXIT_MSR_STORE_HIGH                     0x2007
#define VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL                      0x2008
#define VMX_VMCS64_CTRL_EXIT_MSR_LOAD_HIGH                      0x2009
#define VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL                     0x200a
#define VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_HIGH                     0x200b
#define VMX_VMCS64_CTRL_EXEC_VMCS_PTR_FULL                      0x200c
#define VMX_VMCS64_CTRL_EXEC_VMCS_PTR_HIGH                      0x200d
#define VMX_VMCS64_CTRL_EXEC_PML_ADDR_FULL                      0x200e
#define VMX_VMCS64_CTRL_EXEC_PML_ADDR_HIGH                      0x200f
#define VMX_VMCS64_CTRL_TSC_OFFSET_FULL                         0x2010
#define VMX_VMCS64_CTRL_TSC_OFFSET_HIGH                         0x2011
#define VMX_VMCS64_CTRL_VIRT_APIC_PAGEADDR_FULL                 0x2012
#define VMX_VMCS64_CTRL_VIRT_APIC_PAGEADDR_HIGH                 0x2013
#define VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL                    0x2014
#define VMX_VMCS64_CTRL_APIC_ACCESSADDR_HIGH                    0x2015
#define VMX_VMCS64_CTRL_POSTED_INTR_DESC_FULL                   0x2016
#define VMX_VMCS64_CTRL_POSTED_INTR_DESC_HIGH                   0x2017
#define VMX_VMCS64_CTRL_VMFUNC_CTRLS_FULL                       0x2018
#define VMX_VMCS64_CTRL_VMFUNC_CTRLS_HIGH                       0x2019
#define VMX_VMCS64_CTRL_EPTP_FULL                               0x201a
#define VMX_VMCS64_CTRL_EPTP_HIGH                               0x201b
#define VMX_VMCS64_CTRL_EOI_BITMAP_0_FULL                       0x201c
#define VMX_VMCS64_CTRL_EOI_BITMAP_0_HIGH                       0x201d
#define VMX_VMCS64_CTRL_EOI_BITMAP_1_FULL                       0x201e
#define VMX_VMCS64_CTRL_EOI_BITMAP_1_HIGH                       0x201f
#define VMX_VMCS64_CTRL_EOI_BITMAP_2_FULL                       0x2020
#define VMX_VMCS64_CTRL_EOI_BITMAP_2_HIGH                       0x2021
#define VMX_VMCS64_CTRL_EOI_BITMAP_3_FULL                       0x2022
#define VMX_VMCS64_CTRL_EOI_BITMAP_3_HIGH                       0x2023
#define VMX_VMCS64_CTRL_EPTP_LIST_FULL                          0x2024
#define VMX_VMCS64_CTRL_EPTP_LIST_HIGH                          0x2025
#define VMX_VMCS64_CTRL_VMREAD_BITMAP_FULL                      0x2026
#define VMX_VMCS64_CTRL_VMREAD_BITMAP_HIGH                      0x2027
#define VMX_VMCS64_CTRL_VMWRITE_BITMAP_FULL                     0x2028
#define VMX_VMCS64_CTRL_VMWRITE_BITMAP_HIGH                     0x2029
#define VMX_VMCS64_CTRL_VIRTXCPT_INFO_ADDR_FULL                 0x202a
#define VMX_VMCS64_CTRL_VIRTXCPT_INFO_ADDR_HIGH                 0x202b
#define VMX_VMCS64_CTRL_XSS_EXITING_BITMAP_FULL                 0x202c
#define VMX_VMCS64_CTRL_XSS_EXITING_BITMAP_HIGH                 0x202d
#define VMX_VMCS64_CTRL_ENCLS_EXITING_BITMAP_FULL               0x202e
#define VMX_VMCS64_CTRL_ENCLS_EXITING_BITMAP_HIGH               0x202f
#define VMX_VMCS64_CTRL_TSC_MULTIPLIER_FULL                     0x2032
#define VMX_VMCS64_CTRL_TSC_MULTIPLIER_HIGH                     0x2033
/** @} */


/** @name VMCS field encoding: 64-bit read-only data fields.
 * @{
 */
#define VMX_VMCS64_RO_GUEST_PHYS_ADDR_FULL                      0x2400
#define VMX_VMCS64_RO_GUEST_PHYS_ADDR_HIGH                      0x2401
/** @} */


/** @name VMCS field encoding: 64-bit guest fields.
 * @{
 */
#define VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL                     0x2800
#define VMX_VMCS64_GUEST_VMCS_LINK_PTR_HIGH                     0x2801
#define VMX_VMCS64_GUEST_DEBUGCTL_FULL                          0x2802
#define VMX_VMCS64_GUEST_DEBUGCTL_HIGH                          0x2803
#define VMX_VMCS64_GUEST_PAT_FULL                               0x2804
#define VMX_VMCS64_GUEST_PAT_HIGH                               0x2805
#define VMX_VMCS64_GUEST_EFER_FULL                              0x2806
#define VMX_VMCS64_GUEST_EFER_HIGH                              0x2807
#define VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL                  0x2808
#define VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_HIGH                  0x2809
#define VMX_VMCS64_GUEST_PDPTE0_FULL                            0x280a
#define VMX_VMCS64_GUEST_PDPTE0_HIGH                            0x280b
#define VMX_VMCS64_GUEST_PDPTE1_FULL                            0x280c
#define VMX_VMCS64_GUEST_PDPTE1_HIGH                            0x280d
#define VMX_VMCS64_GUEST_PDPTE2_FULL                            0x280e
#define VMX_VMCS64_GUEST_PDPTE2_HIGH                            0x280f
#define VMX_VMCS64_GUEST_PDPTE3_FULL                            0x2810
#define VMX_VMCS64_GUEST_PDPTE3_HIGH                            0x2811
#define VMX_VMCS64_GUEST_BNDCFGS_FULL                           0x2812
#define VMX_VMCS64_GUEST_BNDCFGS_HIGH                           0x2813
/** @} */


/** @name VMCS field encoding: 64-bit host fields.
 * @{
 */
#define VMX_VMCS64_HOST_PAT_FULL                                0x2c00
#define VMX_VMCS64_HOST_PAT_HIGH                                0x2c01
#define VMX_VMCS64_HOST_EFER_FULL                               0x2c02
#define VMX_VMCS64_HOST_EFER_HIGH                               0x2c03
#define VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_FULL                   0x2c04
#define VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_HIGH                   0x2c05
/** @} */


/** @name VMCS field encoding: 32-bit control fields.
 * @{
 */
#define VMX_VMCS32_CTRL_PIN_EXEC                                0x4000
#define VMX_VMCS32_CTRL_PROC_EXEC                               0x4002
#define VMX_VMCS32_CTRL_EXCEPTION_BITMAP                        0x4004
#define VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK                    0x4006
#define VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH                   0x4008
#define VMX_VMCS32_CTRL_CR3_TARGET_COUNT                        0x400a
#define VMX_VMCS32_CTRL_EXIT                                    0x400c
#define VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT                    0x400e
#define VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT                     0x4010
#define VMX_VMCS32_CTRL_ENTRY                                   0x4012
#define VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT                    0x4014
#define VMX_VMCS32_CTRL_ENTRY_INTERRUPTION_INFO                 0x4016
#define VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE                 0x4018
#define VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH                      0x401a
#define VMX_VMCS32_CTRL_TPR_THRESHOLD                           0x401c
#define VMX_VMCS32_CTRL_PROC_EXEC2                              0x401e
#define VMX_VMCS32_CTRL_PLE_GAP                                 0x4020
#define VMX_VMCS32_CTRL_PLE_WINDOW                              0x4022
/** @} */


/** @name VMCS field encoding: 32-bits read-only fields.
 * @{
 */
#define VMX_VMCS32_RO_VM_INSTR_ERROR                            0x4400
#define VMX_VMCS32_RO_EXIT_REASON                               0x4402
#define VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO                    0x4404
#define VMX_VMCS32_RO_EXIT_INTERRUPTION_ERROR_CODE              0x4406
#define VMX_VMCS32_RO_IDT_VECTORING_INFO                        0x4408
#define VMX_VMCS32_RO_IDT_VECTORING_ERROR_CODE                  0x440a
#define VMX_VMCS32_RO_EXIT_INSTR_LENGTH                         0x440c
#define VMX_VMCS32_RO_EXIT_INSTR_INFO                           0x440e
/** @} */


/** @name VMCS field encoding: 32-bit guest-state fields.
 * @{
 */
#define VMX_VMCS32_GUEST_ES_LIMIT                               0x4800
#define VMX_VMCS32_GUEST_CS_LIMIT                               0x4802
#define VMX_VMCS32_GUEST_SS_LIMIT                               0x4804
#define VMX_VMCS32_GUEST_DS_LIMIT                               0x4806
#define VMX_VMCS32_GUEST_FS_LIMIT                               0x4808
#define VMX_VMCS32_GUEST_GS_LIMIT                               0x480a
#define VMX_VMCS32_GUEST_LDTR_LIMIT                             0x480c
#define VMX_VMCS32_GUEST_TR_LIMIT                               0x480e
#define VMX_VMCS32_GUEST_GDTR_LIMIT                             0x4810
#define VMX_VMCS32_GUEST_IDTR_LIMIT                             0x4812
#define VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS                       0x4814
#define VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS                       0x4816
#define VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS                       0x4818
#define VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS                       0x481a
#define VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS                       0x481c
#define VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS                       0x481e
#define VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS                     0x4820
#define VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS                       0x4822
#define VMX_VMCS32_GUEST_INT_STATE                              0x4824
#define VMX_VMCS32_GUEST_ACTIVITY_STATE                         0x4826
#define VMX_VMCS32_GUEST_SMBASE                                 0x4828
#define VMX_VMCS32_GUEST_SYSENTER_CS                            0x482a
#define VMX_VMCS32_PREEMPT_TIMER_VALUE                          0x482e
/** @} */


/** @name VMCS field encoding: 32-bit host-state fields.
 * @{
 */
#define VMX_VMCS32_HOST_SYSENTER_CS                             0x4C00
/** @} */


/** @name Natural width control fields.
 * @{
 */
#define VMX_VMCS_CTRL_CR0_MASK                                  0x6000
#define VMX_VMCS_CTRL_CR4_MASK                                  0x6002
#define VMX_VMCS_CTRL_CR0_READ_SHADOW                           0x6004
#define VMX_VMCS_CTRL_CR4_READ_SHADOW                           0x6006
#define VMX_VMCS_CTRL_CR3_TARGET_VAL0                           0x6008
#define VMX_VMCS_CTRL_CR3_TARGET_VAL1                           0x600a
#define VMX_VMCS_CTRL_CR3_TARGET_VAL2                           0x600c
#define VMX_VMCS_CTRL_CR3_TARGET_VAL3                           0x600e
/** @} */


/** @name Natural width read-only data fields.
 * @{
 */
#define VMX_VMCS_RO_EXIT_QUALIFICATION                          0x6400
#define VMX_VMCS_RO_IO_RCX                                      0x6402
#define VMX_VMCS_RO_IO_RSX                                      0x6404
#define VMX_VMCS_RO_IO_RDI                                      0x6406
#define VMX_VMCS_RO_IO_RIP                                      0x6408
#define VMX_VMCS_RO_EXIT_GUEST_LINEAR_ADDR                      0x640a
/** @} */


/** @name VMCS field encoding: Natural width guest-state fields.
 * @{
 */
#define VMX_VMCS_GUEST_CR0                                      0x6800
#define VMX_VMCS_GUEST_CR3                                      0x6802
#define VMX_VMCS_GUEST_CR4                                      0x6804
#define VMX_VMCS_GUEST_ES_BASE                                  0x6806
#define VMX_VMCS_GUEST_CS_BASE                                  0x6808
#define VMX_VMCS_GUEST_SS_BASE                                  0x680a
#define VMX_VMCS_GUEST_DS_BASE                                  0x680c
#define VMX_VMCS_GUEST_FS_BASE                                  0x680e
#define VMX_VMCS_GUEST_GS_BASE                                  0x6810
#define VMX_VMCS_GUEST_LDTR_BASE                                0x6812
#define VMX_VMCS_GUEST_TR_BASE                                  0x6814
#define VMX_VMCS_GUEST_GDTR_BASE                                0x6816
#define VMX_VMCS_GUEST_IDTR_BASE                                0x6818
#define VMX_VMCS_GUEST_DR7                                      0x681a
#define VMX_VMCS_GUEST_RSP                                      0x681c
#define VMX_VMCS_GUEST_RIP                                      0x681e
#define VMX_VMCS_GUEST_RFLAGS                                   0x6820
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPTS                      0x6822
#define VMX_VMCS_GUEST_SYSENTER_ESP                             0x6824
#define VMX_VMCS_GUEST_SYSENTER_EIP                             0x6826
/** @} */


/** @name VMCS field encoding: Natural width host-state fields.
 * @{
 */
#define VMX_VMCS_HOST_CR0                                       0x6c00
#define VMX_VMCS_HOST_CR3                                       0x6c02
#define VMX_VMCS_HOST_CR4                                       0x6c04
#define VMX_VMCS_HOST_FS_BASE                                   0x6c06
#define VMX_VMCS_HOST_GS_BASE                                   0x6c08
#define VMX_VMCS_HOST_TR_BASE                                   0x6c0a
#define VMX_VMCS_HOST_GDTR_BASE                                 0x6c0c
#define VMX_VMCS_HOST_IDTR_BASE                                 0x6c0e
#define VMX_VMCS_HOST_SYSENTER_ESP                              0x6c10
#define VMX_VMCS_HOST_SYSENTER_EIP                              0x6c12
#define VMX_VMCS_HOST_RSP                                       0x6c14
#define VMX_VMCS_HOST_RIP                                       0x6c16
/** @} */


/** @name VMCS field encoding: Access.
 * @{ */
typedef enum
{
    VMXVMCSFIELDACCESS_FULL = 0,
    VMXVMCSFIELDACCESS_HIGH
} VMXVMCSFIELDACCESS;
AssertCompileSize(VMXVMCSFIELDACCESS, 4);
/** @} */


/** @name VMCS field encoding: Type.
 * @{ */
typedef enum
{
    VMXVMCSFIELDTYPE_CONTROL = 0,
    VMXVMCSFIELDTYPE_VMEXIT_INFO,
    VMXVMCSFIELDTYPE_GUEST_STATE,
    VMXVMCSFIELDTYPE_HOST_STATE
} VMXVMCSFIELDTYPE;
AssertCompileSize(VMXVMCSFIELDTYPE, 4);
/** @} */


/** @name VMCS field encoding: Width.
 * @{ */
typedef enum
{
    VMXVMCSFIELDWIDTH_16BIT = 0,
    VMXVMCSFIELDWIDTH_64BIT,
    VMXVMCSFIELDWIDTH_32BIT,
    VMXVMCSFIELDWIDTH_NATURAL
} VMXVMCSFIELDWIDTH;
AssertCompileSize(VMXVMCSFIELDWIDTH, 4);
/** @} */

/** @name VM-entry instruction length.
 * @{ */
/** The maximum valid value for VM-entry instruction length while injecting a
 *  software interrupt, software exception or privileged software exception. */
#define VMX_ENTRY_INSTR_LEN_MAX                                 15
/** @} */


/** @name VM-entry register masks.
 * @{ */
/** CR0 bits ignored on VM-entry (ET, NW, CD and reserved bits bits 6:15, bit 17,
 *  bits 19:28). */
#define VMX_ENTRY_CR0_IGNORE_MASK                               UINT64_C(0x7ffaffc0)
/** DR7 bits set here are always cleared on VM-entry (bit 12, bits 14:15). */
#define VMX_ENTRY_DR7_MBZ_MASK                                  UINT64_C(0xd000)
/** DR7 bits set here are always set on VM-entry (bit 10). */
#define VMX_ENTRY_DR7_MB1_MASK                                  UINT64_C(0x400)
/** @} */


/** @name Pin-based VM-execution controls.
 * @{
 */
/** External interrupt exiting. */
#define VMX_PIN_CTLS_EXT_INT_EXIT                               RT_BIT(0)
/** NMI exiting. */
#define VMX_PIN_CTLS_NMI_EXIT                                   RT_BIT(3)
/** Virtual NMIs. */
#define VMX_PIN_CTLS_VIRT_NMI                                   RT_BIT(5)
/** Activate VMX preemption timer. */
#define VMX_PIN_CTLS_PREEMPT_TIMER                              RT_BIT(6)
/** Process interrupts with the posted-interrupt notification vector. */
#define VMX_PIN_CTLS_POSTED_INT                                 RT_BIT(7)
/** Default1 class when true capability MSRs are not supported. */
#define VMX_PIN_CTLS_DEFAULT1                                   UINT32_C(0x00000016)

/** Bit fields for MSR_IA32_VMX_PINBASED_CTLS and Pin-based VM-execution
 *  controls field in the VMCS. */
#define VMX_BF_PIN_CTLS_EXT_INT_EXIT_SHIFT                      0
#define VMX_BF_PIN_CTLS_EXT_INT_EXIT_MASK                       UINT32_C(0x00000001)
#define VMX_BF_PIN_CTLS_UNDEF_1_2_SHIFT                         1
#define VMX_BF_PIN_CTLS_UNDEF_1_2_MASK                          UINT32_C(0x00000006)
#define VMX_BF_PIN_CTLS_NMI_EXIT_SHIFT                          3
#define VMX_BF_PIN_CTLS_NMI_EXIT_MASK                           UINT32_C(0x00000008)
#define VMX_BF_PIN_CTLS_UNDEF_4_SHIFT                           4
#define VMX_BF_PIN_CTLS_UNDEF_4_MASK                            UINT32_C(0x00000010)
#define VMX_BF_PIN_CTLS_VIRT_NMI_SHIFT                          5
#define VMX_BF_PIN_CTLS_VIRT_NMI_MASK                           UINT32_C(0x00000020)
#define VMX_BF_PIN_CTLS_PREEMPT_TIMER_SHIFT                     6
#define VMX_BF_PIN_CTLS_PREEMPT_TIMER_MASK                      UINT32_C(0x00000040)
#define VMX_BF_PIN_CTLS_POSTED_INT_SHIFT                        7
#define VMX_BF_PIN_CTLS_POSTED_INT_MASK                         UINT32_C(0x00000080)
#define VMX_BF_PIN_CTLS_UNDEF_8_31_SHIFT                        8
#define VMX_BF_PIN_CTLS_UNDEF_8_31_MASK                         UINT32_C(0xffffff00)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_PIN_CTLS_, UINT32_C(0), UINT32_MAX,
                            (EXT_INT_EXIT, UNDEF_1_2, NMI_EXIT, UNDEF_4, VIRT_NMI, PREEMPT_TIMER, POSTED_INT, UNDEF_8_31));
/** @} */


/** @name Processor-based VM-execution controls.
 * @{
 */
/** VM-exit as soon as RFLAGS.IF=1 and no blocking is active. */
#define VMX_PROC_CTLS_INT_WINDOW_EXIT                           RT_BIT(2)
/** Use timestamp counter offset. */
#define VMX_PROC_CTLS_USE_TSC_OFFSETTING                        RT_BIT(3)
/** VM-exit when executing the HLT instruction. */
#define VMX_PROC_CTLS_HLT_EXIT                                  RT_BIT(7)
/** VM-exit when executing the INVLPG instruction. */
#define VMX_PROC_CTLS_INVLPG_EXIT                               RT_BIT(9)
/** VM-exit when executing the MWAIT instruction. */
#define VMX_PROC_CTLS_MWAIT_EXIT                                RT_BIT(10)
/** VM-exit when executing the RDPMC instruction. */
#define VMX_PROC_CTLS_RDPMC_EXIT                                RT_BIT(11)
/** VM-exit when executing the RDTSC/RDTSCP instruction. */
#define VMX_PROC_CTLS_RDTSC_EXIT                                RT_BIT(12)
/** VM-exit when executing the MOV to CR3 instruction. (forced to 1 on the
 *  'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_PROC_CTLS_CR3_LOAD_EXIT                             RT_BIT(15)
/** VM-exit when executing the MOV from CR3 instruction. (forced to 1 on the
 *  'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_PROC_CTLS_CR3_STORE_EXIT                            RT_BIT(16)
/** VM-exit on CR8 loads. */
#define VMX_PROC_CTLS_CR8_LOAD_EXIT                             RT_BIT(19)
/** VM-exit on CR8 stores. */
#define VMX_PROC_CTLS_CR8_STORE_EXIT                            RT_BIT(20)
/** Use TPR shadow. */
#define VMX_PROC_CTLS_USE_TPR_SHADOW                            RT_BIT(21)
/** VM-exit when virtual NMI blocking is disabled. */
#define VMX_PROC_CTLS_NMI_WINDOW_EXIT                           RT_BIT(22)
/** VM-exit when executing a MOV DRx instruction. */
#define VMX_PROC_CTLS_MOV_DR_EXIT                               RT_BIT(23)
/** VM-exit when executing IO instructions. */
#define VMX_PROC_CTLS_UNCOND_IO_EXIT                            RT_BIT(24)
/** Use IO bitmaps. */
#define VMX_PROC_CTLS_USE_IO_BITMAPS                            RT_BIT(25)
/** Monitor trap flag. */
#define VMX_PROC_CTLS_MONITOR_TRAP_FLAG                         RT_BIT(27)
/** Use MSR bitmaps. */
#define VMX_PROC_CTLS_USE_MSR_BITMAPS                           RT_BIT(28)
/** VM-exit when executing the MONITOR instruction. */
#define VMX_PROC_CTLS_MONITOR_EXIT                              RT_BIT(29)
/** VM-exit when executing the PAUSE instruction. */
#define VMX_PROC_CTLS_PAUSE_EXIT                                RT_BIT(30)
/** Whether the secondary processor based VM-execution controls are used. */
#define VMX_PROC_CTLS_USE_SECONDARY_CTLS                        RT_BIT(31)
/** Default1 class when true-capability MSRs are not supported. */
#define VMX_PROC_CTLS_DEFAULT1                                  UINT32_C(0x0401e172)

/** Bit fields for MSR_IA32_VMX_PROCBASED_CTLS and Processor-based VM-execution
 *  controls field in the VMCS. */
#define VMX_BF_PROC_CTLS_UNDEF_0_1_SHIFT                        0
#define VMX_BF_PROC_CTLS_UNDEF_0_1_MASK                         UINT32_C(0x00000003)
#define VMX_BF_PROC_CTLS_INT_WINDOW_EXIT_SHIFT                  2
#define VMX_BF_PROC_CTLS_INT_WINDOW_EXIT_MASK                   UINT32_C(0x00000004)
#define VMX_BF_PROC_CTLS_USE_TSC_OFFSETTING_SHIFT               3
#define VMX_BF_PROC_CTLS_USE_TSC_OFFSETTING_MASK                UINT32_C(0x00000008)
#define VMX_BF_PROC_CTLS_UNDEF_4_6_SHIFT                        4
#define VMX_BF_PROC_CTLS_UNDEF_4_6_MASK                         UINT32_C(0x00000070)
#define VMX_BF_PROC_CTLS_HLT_EXIT_SHIFT                         7
#define VMX_BF_PROC_CTLS_HLT_EXIT_MASK                          UINT32_C(0x00000080)
#define VMX_BF_PROC_CTLS_UNDEF_8_SHIFT                          8
#define VMX_BF_PROC_CTLS_UNDEF_8_MASK                           UINT32_C(0x00000100)
#define VMX_BF_PROC_CTLS_INVLPG_EXIT_SHIFT                      9
#define VMX_BF_PROC_CTLS_INVLPG_EXIT_MASK                       UINT32_C(0x00000200)
#define VMX_BF_PROC_CTLS_MWAIT_EXIT_SHIFT                       10
#define VMX_BF_PROC_CTLS_MWAIT_EXIT_MASK                        UINT32_C(0x00000400)
#define VMX_BF_PROC_CTLS_RDPMC_EXIT_SHIFT                       11
#define VMX_BF_PROC_CTLS_RDPMC_EXIT_MASK                        UINT32_C(0x00000800)
#define VMX_BF_PROC_CTLS_RDTSC_EXIT_SHIFT                       12
#define VMX_BF_PROC_CTLS_RDTSC_EXIT_MASK                        UINT32_C(0x00001000)
#define VMX_BF_PROC_CTLS_UNDEF_13_14_SHIFT                      13
#define VMX_BF_PROC_CTLS_UNDEF_13_14_MASK                       UINT32_C(0x00006000)
#define VMX_BF_PROC_CTLS_CR3_LOAD_EXIT_SHIFT                    15
#define VMX_BF_PROC_CTLS_CR3_LOAD_EXIT_MASK                     UINT32_C(0x00008000)
#define VMX_BF_PROC_CTLS_CR3_STORE_EXIT_SHIFT                   16
#define VMX_BF_PROC_CTLS_CR3_STORE_EXIT_MASK                    UINT32_C(0x00010000)
#define VMX_BF_PROC_CTLS_UNDEF_17_18_SHIFT                      17
#define VMX_BF_PROC_CTLS_UNDEF_17_18_MASK                       UINT32_C(0x00060000)
#define VMX_BF_PROC_CTLS_CR8_LOAD_EXIT_SHIFT                    19
#define VMX_BF_PROC_CTLS_CR8_LOAD_EXIT_MASK                     UINT32_C(0x00080000)
#define VMX_BF_PROC_CTLS_CR8_STORE_EXIT_SHIFT                   20
#define VMX_BF_PROC_CTLS_CR8_STORE_EXIT_MASK                    UINT32_C(0x00100000)
#define VMX_BF_PROC_CTLS_USE_TPR_SHADOW_SHIFT                   21
#define VMX_BF_PROC_CTLS_USE_TPR_SHADOW_MASK                    UINT32_C(0x00200000)
#define VMX_BF_PROC_CTLS_NMI_WINDOW_EXIT_SHIFT                  22
#define VMX_BF_PROC_CTLS_NMI_WINDOW_EXIT_MASK                   UINT32_C(0x00400000)
#define VMX_BF_PROC_CTLS_MOV_DR_EXIT_SHIFT                      23
#define VMX_BF_PROC_CTLS_MOV_DR_EXIT_MASK                       UINT32_C(0x00800000)
#define VMX_BF_PROC_CTLS_UNCOND_IO_EXIT_SHIFT                   24
#define VMX_BF_PROC_CTLS_UNCOND_IO_EXIT_MASK                    UINT32_C(0x01000000)
#define VMX_BF_PROC_CTLS_USE_IO_BITMAPS_SHIFT                   25
#define VMX_BF_PROC_CTLS_USE_IO_BITMAPS_MASK                    UINT32_C(0x02000000)
#define VMX_BF_PROC_CTLS_UNDEF_26_SHIFT                         26
#define VMX_BF_PROC_CTLS_UNDEF_26_MASK                          UINT32_C(0x4000000)
#define VMX_BF_PROC_CTLS_MONITOR_TRAP_FLAG_SHIFT                27
#define VMX_BF_PROC_CTLS_MONITOR_TRAP_FLAG_MASK                 UINT32_C(0x08000000)
#define VMX_BF_PROC_CTLS_USE_MSR_BITMAPS_SHIFT                  28
#define VMX_BF_PROC_CTLS_USE_MSR_BITMAPS_MASK                   UINT32_C(0x10000000)
#define VMX_BF_PROC_CTLS_MONITOR_EXIT_SHIFT                     29
#define VMX_BF_PROC_CTLS_MONITOR_EXIT_MASK                      UINT32_C(0x20000000)
#define VMX_BF_PROC_CTLS_PAUSE_EXIT_SHIFT                       30
#define VMX_BF_PROC_CTLS_PAUSE_EXIT_MASK                        UINT32_C(0x40000000)
#define VMX_BF_PROC_CTLS_USE_SECONDARY_CTLS_SHIFT               31
#define VMX_BF_PROC_CTLS_USE_SECONDARY_CTLS_MASK                UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_PROC_CTLS_, UINT32_C(0), UINT32_MAX,
                            (UNDEF_0_1, INT_WINDOW_EXIT, USE_TSC_OFFSETTING, UNDEF_4_6, HLT_EXIT, UNDEF_8, INVLPG_EXIT,
                             MWAIT_EXIT, RDPMC_EXIT, RDTSC_EXIT, UNDEF_13_14, CR3_LOAD_EXIT, CR3_STORE_EXIT, UNDEF_17_18,
                             CR8_LOAD_EXIT, CR8_STORE_EXIT, USE_TPR_SHADOW, NMI_WINDOW_EXIT, MOV_DR_EXIT, UNCOND_IO_EXIT,
                             USE_IO_BITMAPS, UNDEF_26, MONITOR_TRAP_FLAG, USE_MSR_BITMAPS, MONITOR_EXIT, PAUSE_EXIT,
                             USE_SECONDARY_CTLS));
/** @} */


/** @name Secondary Processor-based VM-execution controls.
 * @{
 */
/** Virtualize APIC access. */
#define VMX_PROC_CTLS2_VIRT_APIC_ACCESS                         RT_BIT(0)
/** EPT supported/enabled. */
#define VMX_PROC_CTLS2_EPT                                      RT_BIT(1)
/** Descriptor table instructions cause VM-exits. */
#define VMX_PROC_CTLS2_DESC_TABLE_EXIT                          RT_BIT(2)
/** RDTSCP supported/enabled. */
#define VMX_PROC_CTLS2_RDTSCP                                   RT_BIT(3)
/** Virtualize x2APIC mode. */
#define VMX_PROC_CTLS2_VIRT_X2APIC_MODE                         RT_BIT(4)
/** VPID supported/enabled. */
#define VMX_PROC_CTLS2_VPID                                     RT_BIT(5)
/** VM-exit when executing the WBINVD instruction. */
#define VMX_PROC_CTLS2_WBINVD_EXIT                              RT_BIT(6)
/** Unrestricted guest execution. */
#define VMX_PROC_CTLS2_UNRESTRICTED_GUEST                       RT_BIT(7)
/** APIC register virtualization. */
#define VMX_PROC_CTLS2_APIC_REG_VIRT                            RT_BIT(8)
/** Virtual-interrupt delivery. */
#define VMX_PROC_CTLS2_VIRT_INT_DELIVERY                        RT_BIT(9)
/** A specified number of pause loops cause a VM-exit. */
#define VMX_PROC_CTLS2_PAUSE_LOOP_EXIT                          RT_BIT(10)
/** VM-exit when executing RDRAND instructions. */
#define VMX_PROC_CTLS2_RDRAND_EXIT                              RT_BIT(11)
/** Enables INVPCID instructions. */
#define VMX_PROC_CTLS2_INVPCID                                  RT_BIT(12)
/** Enables VMFUNC instructions. */
#define VMX_PROC_CTLS2_VMFUNC                                   RT_BIT(13)
/** Enables VMCS shadowing. */
#define VMX_PROC_CTLS2_VMCS_SHADOWING                           RT_BIT(14)
/** Enables ENCLS VM-exits. */
#define VMX_PROC_CTLS2_ENCLS_EXIT                               RT_BIT(15)
/** VM-exit when executing RDSEED. */
#define VMX_PROC_CTLS2_RDSEED_EXIT                              RT_BIT(16)
/** Enables page-modification logging. */
#define VMX_PROC_CTLS2_PML                                      RT_BIT(17)
/** Controls whether EPT-violations may cause \#VE instead of exits. */
#define VMX_PROC_CTLS2_EPT_VE                                   RT_BIT(18)
/** Conceal VMX non-root operation from Intel processor trace (PT). */
#define VMX_PROC_CTLS2_CONCEAL_FROM_PT                          RT_BIT(19)
/** Enables XSAVES/XRSTORS instructions. */
#define VMX_PROC_CTLS2_XSAVES_XRSTORS                           RT_BIT(20)
/** Use TSC scaling. */
#define VMX_PROC_CTLS2_TSC_SCALING                              RT_BIT(25)

/** Bit fields for MSR_IA32_VMX_PROCBASED_CTLS2 and Secondary processor-based
 *  VM-execution controls field in the VMCS. */
#define VMX_BF_PROC_CTLS2_VIRT_APIC_ACCESS_SHIFT                0
#define VMX_BF_PROC_CTLS2_VIRT_APIC_ACCESS_MASK                 UINT32_C(0x00000001)
#define VMX_BF_PROC_CTLS2_EPT_SHIFT                             1
#define VMX_BF_PROC_CTLS2_EPT_MASK                              UINT32_C(0x00000002)
#define VMX_BF_PROC_CTLS2_DESC_TABLE_EXIT_SHIFT                 2
#define VMX_BF_PROC_CTLS2_DESC_TABLE_EXIT_MASK                  UINT32_C(0x00000004)
#define VMX_BF_PROC_CTLS2_RDTSCP_SHIFT                          3
#define VMX_BF_PROC_CTLS2_RDTSCP_MASK                           UINT32_C(0x00000008)
#define VMX_BF_PROC_CTLS2_VIRT_X2APIC_MODE_SHIFT                4
#define VMX_BF_PROC_CTLS2_VIRT_X2APIC_MODE_MASK                 UINT32_C(0x00000010)
#define VMX_BF_PROC_CTLS2_VPID_SHIFT                            5
#define VMX_BF_PROC_CTLS2_VPID_MASK                             UINT32_C(0x00000020)
#define VMX_BF_PROC_CTLS2_WBINVD_EXIT_SHIFT                     6
#define VMX_BF_PROC_CTLS2_WBINVD_EXIT_MASK                      UINT32_C(0x00000040)
#define VMX_BF_PROC_CTLS2_UNRESTRICTED_GUEST_SHIFT              7
#define VMX_BF_PROC_CTLS2_UNRESTRICTED_GUEST_MASK               UINT32_C(0x00000080)
#define VMX_BF_PROC_CTLS2_APIC_REG_VIRT_SHIFT                   8
#define VMX_BF_PROC_CTLS2_APIC_REG_VIRT_MASK                    UINT32_C(0x00000100)
#define VMX_BF_PROC_CTLS2_VIRT_INT_DELIVERY_SHIFT               9
#define VMX_BF_PROC_CTLS2_VIRT_INT_DELIVERY_MASK                UINT32_C(0x00000200)
#define VMX_BF_PROC_CTLS2_PAUSE_LOOP_EXIT_SHIFT                 10
#define VMX_BF_PROC_CTLS2_PAUSE_LOOP_EXIT_MASK                  UINT32_C(0x00000400)
#define VMX_BF_PROC_CTLS2_RDRAND_EXIT_SHIFT                     11
#define VMX_BF_PROC_CTLS2_RDRAND_EXIT_MASK                      UINT32_C(0x00000800)
#define VMX_BF_PROC_CTLS2_INVPCID_SHIFT                         12
#define VMX_BF_PROC_CTLS2_INVPCID_MASK                          UINT32_C(0x00001000)
#define VMX_BF_PROC_CTLS2_VMFUNC_SHIFT                          13
#define VMX_BF_PROC_CTLS2_VMFUNC_MASK                           UINT32_C(0x00002000)
#define VMX_BF_PROC_CTLS2_VMCS_SHADOWING_SHIFT                  14
#define VMX_BF_PROC_CTLS2_VMCS_SHADOWING_MASK                   UINT32_C(0x00004000)
#define VMX_BF_PROC_CTLS2_ENCLS_EXIT_SHIFT                      15
#define VMX_BF_PROC_CTLS2_ENCLS_EXIT_MASK                       UINT32_C(0x00008000)
#define VMX_BF_PROC_CTLS2_RDSEED_EXIT_SHIFT                     16
#define VMX_BF_PROC_CTLS2_RDSEED_EXIT_MASK                      UINT32_C(0x00010000)
#define VMX_BF_PROC_CTLS2_PML_SHIFT                             17
#define VMX_BF_PROC_CTLS2_PML_MASK                              UINT32_C(0x00020000)
#define VMX_BF_PROC_CTLS2_EPT_VE_SHIFT                          18
#define VMX_BF_PROC_CTLS2_EPT_VE_MASK                           UINT32_C(0x00040000)
#define VMX_BF_PROC_CTLS2_CONCEAL_FROM_PT_SHIFT                 19
#define VMX_BF_PROC_CTLS2_CONCEAL_FROM_PT_MASK                  UINT32_C(0x00080000)
#define VMX_BF_PROC_CTLS2_XSAVES_XRSTORS_SHIFT                  20
#define VMX_BF_PROC_CTLS2_XSAVES_XRSTORS_MASK                   UINT32_C(0x00100000)
#define VMX_BF_PROC_CTLS2_UNDEF_21_24_SHIFT                     21
#define VMX_BF_PROC_CTLS2_UNDEF_21_24_MASK                      UINT32_C(0x01e00000)
#define VMX_BF_PROC_CTLS2_TSC_SCALING_SHIFT                     25
#define VMX_BF_PROC_CTLS2_TSC_SCALING_MASK                      UINT32_C(0x02000000)
#define VMX_BF_PROC_CTLS2_UNDEF_26_31_SHIFT                     26
#define VMX_BF_PROC_CTLS2_UNDEF_26_31_MASK                      UINT32_C(0xfc000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_PROC_CTLS2_, UINT32_C(0), UINT32_MAX,
                            (VIRT_APIC_ACCESS, EPT, DESC_TABLE_EXIT, RDTSCP, VIRT_X2APIC_MODE, VPID, WBINVD_EXIT,
                             UNRESTRICTED_GUEST, APIC_REG_VIRT, VIRT_INT_DELIVERY, PAUSE_LOOP_EXIT, RDRAND_EXIT, INVPCID, VMFUNC,
                             VMCS_SHADOWING, ENCLS_EXIT, RDSEED_EXIT, PML, EPT_VE, CONCEAL_FROM_PT, XSAVES_XRSTORS, UNDEF_21_24,
                             TSC_SCALING, UNDEF_26_31));
/** @} */


/** @name VM-entry controls.
 * @{
 */
/** Load guest debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the
 *  'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_ENTRY_CTLS_LOAD_DEBUG                               RT_BIT(2)
/** 64-bit guest mode. Must be 0 for CPUs that don't support AMD64. */
#define VMX_ENTRY_CTLS_IA32E_MODE_GUEST                         RT_BIT(9)
/** In SMM mode after VM-entry. */
#define VMX_ENTRY_CTLS_ENTRY_TO_SMM                             RT_BIT(10)
/** Disable dual treatment of SMI and SMM; must be zero for VM-entry outside of SMM. */
#define VMX_ENTRY_CTLS_DEACTIVATE_DUAL_MON                      RT_BIT(11)
/** Whether the guest IA32_PERF_GLOBAL_CTRL MSR is loaded on VM-entry. */
#define VMX_ENTRY_CTLS_LOAD_PERF_MSR                            RT_BIT(13)
/** Whether the guest IA32_PAT MSR is loaded on VM-entry. */
#define VMX_ENTRY_CTLS_LOAD_PAT_MSR                             RT_BIT(14)
/** Whether the guest IA32_EFER MSR is loaded on VM-entry. */
#define VMX_ENTRY_CTLS_LOAD_EFER_MSR                            RT_BIT(15)
/** Whether the guest IA32_BNDCFGS MSR is loaded on VM-entry. */
#define VMX_ENTRY_CTLS_LOAD_BNDCFGS_MSR                         RT_BIT(16)
/** Whether to conceal VMX from Intel PT (Processor Trace). */
#define VMX_ENTRY_CTLS_CONCEAL_VMX_PT                           RT_BIT(17)
/** Default1 class when true-capability MSRs are not supported. */
#define VMX_ENTRY_CTLS_DEFAULT1                                 UINT32_C(0x000011ff)

/** Bit fields for MSR_IA32_VMX_ENTRY_CTLS and VM-entry controls field in the
 *  VMCS. */
#define VMX_BF_ENTRY_CTLS_UNDEF_0_1_SHIFT                       0
#define VMX_BF_ENTRY_CTLS_UNDEF_0_1_MASK                        UINT32_C(0x00000003)
#define VMX_BF_ENTRY_CTLS_LOAD_DEBUG_SHIFT                      2
#define VMX_BF_ENTRY_CTLS_LOAD_DEBUG_MASK                       UINT32_C(0x00000004)
#define VMX_BF_ENTRY_CTLS_UNDEF_3_8_SHIFT                       3
#define VMX_BF_ENTRY_CTLS_UNDEF_3_8_MASK                        UINT32_C(0x000001f8)
#define VMX_BF_ENTRY_CTLS_IA32E_MODE_GUEST_SHIFT                9
#define VMX_BF_ENTRY_CTLS_IA32E_MODE_GUEST_MASK                 UINT32_C(0x00000200)
#define VMX_BF_ENTRY_CTLS_ENTRY_SMM_SHIFT                       10
#define VMX_BF_ENTRY_CTLS_ENTRY_SMM_MASK                        UINT32_C(0x00000400)
#define VMX_BF_ENTRY_CTLS_DEACTIVATE_DUAL_MON_SHIFT             11
#define VMX_BF_ENTRY_CTLS_DEACTIVATE_DUAL_MON_MASK              UINT32_C(0x00000800)
#define VMX_BF_ENTRY_CTLS_UNDEF_12_SHIFT                        12
#define VMX_BF_ENTRY_CTLS_UNDEF_12_MASK                         UINT32_C(0x00001000)
#define VMX_BF_ENTRY_CTLS_LOAD_PERF_MSR_SHIFT                   13
#define VMX_BF_ENTRY_CTLS_LOAD_PERF_MSR_MASK                    UINT32_C(0x00002000)
#define VMX_BF_ENTRY_CTLS_LOAD_PAT_MSR_SHIFT                    14
#define VMX_BF_ENTRY_CTLS_LOAD_PAT_MSR_MASK                     UINT32_C(0x00004000)
#define VMX_BF_ENTRY_CTLS_LOAD_EFER_MSR_SHIFT                   15
#define VMX_BF_ENTRY_CTLS_LOAD_EFER_MSR_MASK                    UINT32_C(0x00008000)
#define VMX_BF_ENTRY_CTLS_LOAD_BNDCFGS_MSR_SHIFT                16
#define VMX_BF_ENTRY_CTLS_LOAD_BNDCFGS_MSR_MASK                 UINT32_C(0x00010000)
#define VMX_BF_ENTRY_CTLS_CONCEAL_VMX_PT_SHIFT                  17
#define VMX_BF_ENTRY_CTLS_CONCEAL_VMX_PT_MASK                   UINT32_C(0x00020000)
#define VMX_BF_ENTRY_CTLS_UNDEF_18_31_SHIFT                     18
#define VMX_BF_ENTRY_CTLS_UNDEF_18_31_MASK                      UINT32_C(0xfffc0000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_ENTRY_CTLS_, UINT32_C(0), UINT32_MAX,
                            (UNDEF_0_1, LOAD_DEBUG, UNDEF_3_8, IA32E_MODE_GUEST, ENTRY_SMM, DEACTIVATE_DUAL_MON, UNDEF_12,
                             LOAD_PERF_MSR, LOAD_PAT_MSR, LOAD_EFER_MSR, LOAD_BNDCFGS_MSR, CONCEAL_VMX_PT, UNDEF_18_31));
/** @} */


/** @name VM-exit controls.
 * @{
 */
/** Save guest debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the
 *  'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_EXIT_CTLS_SAVE_DEBUG                                RT_BIT(2)
/** Return to long mode after a VM-exit. */
#define VMX_EXIT_CTLS_HOST_ADDR_SPACE_SIZE                      RT_BIT(9)
/** Whether the host IA32_PERF_GLOBAL_CTRL MSR is loaded on VM-exit. */
#define VMX_EXIT_CTLS_LOAD_PERF_MSR                             RT_BIT(12)
/** Acknowledge external interrupts with the irq controller if one caused a VM-exit. */
#define VMX_EXIT_CTLS_ACK_EXT_INT                               RT_BIT(15)
/** Whether the guest IA32_PAT MSR is saved on VM-exit. */
#define VMX_EXIT_CTLS_SAVE_PAT_MSR                              RT_BIT(18)
/** Whether the host IA32_PAT MSR is loaded on VM-exit. */
#define VMX_EXIT_CTLS_LOAD_PAT_MSR                              RT_BIT(19)
/** Whether the guest IA32_EFER MSR is saved on VM-exit. */
#define VMX_EXIT_CTLS_SAVE_EFER_MSR                             RT_BIT(20)
/** Whether the host IA32_EFER MSR is loaded on VM-exit. */
#define VMX_EXIT_CTLS_LOAD_EFER_MSR                             RT_BIT(21)
/** Whether the value of the VMX preemption timer is saved on every VM-exit. */
#define VMX_EXIT_CTLS_SAVE_PREEMPT_TIMER                        RT_BIT(22)
/** Whether IA32_BNDCFGS MSR is cleared on VM-exit. */
#define VMX_EXIT_CTLS_CLEAR_BNDCFGS_MSR                         RT_BIT(23)
/** Default1 class when true-capability MSRs are not supported.  */
#define VMX_EXIT_CTLS_DEFAULT1                                  UINT32_C(0x00036dff)

/** Bit fields for MSR_IA32_VMX_EXIT_CTLS and VM-exit controls field in the
 *  VMCS. */
#define VMX_BF_EXIT_CTLS_UNDEF_0_1_SHIFT                        0
#define VMX_BF_EXIT_CTLS_UNDEF_0_1_MASK                         UINT32_C(0x00000003)
#define VMX_BF_EXIT_CTLS_SAVE_DEBUG_SHIFT                       2
#define VMX_BF_EXIT_CTLS_SAVE_DEBUG_MASK                        UINT32_C(0x00000004)
#define VMX_BF_EXIT_CTLS_UNDEF_3_8_SHIFT                        3
#define VMX_BF_EXIT_CTLS_UNDEF_3_8_MASK                         UINT32_C(0x000001f8)
#define VMX_BF_EXIT_CTLS_HOST_ADDR_SPACE_SIZE_SHIFT             9
#define VMX_BF_EXIT_CTLS_HOST_ADDR_SPACE_SIZE_MASK              UINT32_C(0x00000200)
#define VMX_BF_EXIT_CTLS_UNDEF_10_11_SHIFT                      10
#define VMX_BF_EXIT_CTLS_UNDEF_10_11_MASK                       UINT32_C(0x00000c00)
#define VMX_BF_EXIT_CTLS_LOAD_PERF_MSR_SHIFT                    12
#define VMX_BF_EXIT_CTLS_LOAD_PERF_MSR_MASK                     UINT32_C(0x00001000)
#define VMX_BF_EXIT_CTLS_UNDEF_13_14_SHIFT                      13
#define VMX_BF_EXIT_CTLS_UNDEF_13_14_MASK                       UINT32_C(0x00006000)
#define VMX_BF_EXIT_CTLS_ACK_EXT_INT_SHIFT                      15
#define VMX_BF_EXIT_CTLS_ACK_EXT_INT_MASK                       UINT32_C(0x00008000)
#define VMX_BF_EXIT_CTLS_UNDEF_16_17_SHIFT                      16
#define VMX_BF_EXIT_CTLS_UNDEF_16_17_MASK                       UINT32_C(0x00030000)
#define VMX_BF_EXIT_CTLS_SAVE_PAT_MSR_SHIFT                     18
#define VMX_BF_EXIT_CTLS_SAVE_PAT_MSR_MASK                      UINT32_C(0x00040000)
#define VMX_BF_EXIT_CTLS_LOAD_PAT_MSR_SHIFT                     19
#define VMX_BF_EXIT_CTLS_LOAD_PAT_MSR_MASK                      UINT32_C(0x00080000)
#define VMX_BF_EXIT_CTLS_SAVE_EFER_MSR_SHIFT                    20
#define VMX_BF_EXIT_CTLS_SAVE_EFER_MSR_MASK                     UINT32_C(0x00100000)
#define VMX_BF_EXIT_CTLS_LOAD_EFER_MSR_SHIFT                    21
#define VMX_BF_EXIT_CTLS_LOAD_EFER_MSR_MASK                     UINT32_C(0x00200000)
#define VMX_BF_EXIT_CTLS_SAVE_PREEMPT_TIMER_SHIFT               22
#define VMX_BF_EXIT_CTLS_SAVE_PREEMPT_TIMER_MASK                UINT32_C(0x00400000)
#define VMX_BF_EXIT_CTLS_UNDEF_23_31_SHIFT                      23
#define VMX_BF_EXIT_CTLS_UNDEF_23_31_MASK                       UINT32_C(0xff800000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_EXIT_CTLS_, UINT32_C(0), UINT32_MAX,
                            (UNDEF_0_1, SAVE_DEBUG, UNDEF_3_8, HOST_ADDR_SPACE_SIZE, UNDEF_10_11, LOAD_PERF_MSR, UNDEF_13_14,
                             ACK_EXT_INT, UNDEF_16_17, SAVE_PAT_MSR, LOAD_PAT_MSR, SAVE_EFER_MSR, LOAD_EFER_MSR,
                             SAVE_PREEMPT_TIMER, UNDEF_23_31));
/** @} */


/** @name VM-exit reason.
 * @{
 */
#define VMX_EXIT_REASON_BASIC(a)                                ((a) & 0xffff)
#define VMX_EXIT_REASON_HAS_ENTRY_FAILED(a)                     (((a) >> 31) & 1)
#define VMX_EXIT_REASON_ENTRY_FAILED                            RT_BIT(31)

/** Bit fields for VM-exit reason. */
/** The exit reason. */
#define VMX_BF_EXIT_REASON_BASIC_SHIFT                          0
#define VMX_BF_EXIT_REASON_BASIC_MASK                           UINT32_C(0x0000ffff)
/** Bits 16:26 are reseved and MBZ. */
#define VMX_BF_EXIT_REASON_RSVD_16_26_SHIFT                     16
#define VMX_BF_EXIT_REASON_RSVD_16_26_MASK                      UINT32_C(0x07ff0000)
/** Whether the VM-exit was incident to enclave mode. */
#define VMX_BF_EXIT_REASON_ENCLAVE_MODE_SHIFT                   27
#define VMX_BF_EXIT_REASON_ENCLAVE_MODE_MASK                    UINT32_C(0x08000000)
/** Pending MTF (Monitor Trap Flag) during VM-exit (only applicable in SMM mode). */
#define VMX_BF_EXIT_REASON_SMM_PENDING_MTF_SHIFT                28
#define VMX_BF_EXIT_REASON_SMM_PENDING_MTF_MASK                 UINT32_C(0x10000000)
/** VM-exit from VMX root operation (only possible with SMM). */
#define VMX_BF_EXIT_REASON_VMX_ROOT_MODE_SHIFT                  29
#define VMX_BF_EXIT_REASON_VMX_ROOT_MODE_MASK                   UINT32_C(0x20000000)
/** Bit 30 is reserved and MBZ. */
#define VMX_BF_EXIT_REASON_RSVD_30_SHIFT                        30
#define VMX_BF_EXIT_REASON_RSVD_30_MASK                         UINT32_C(0x40000000)
/** Whether VM-entry failed (currently only happens during loading guest-state
 *  or MSRs or machine check exceptions). */
#define VMX_BF_EXIT_REASON_ENTRY_FAILED_SHIFT                   31
#define VMX_BF_EXIT_REASON_ENTRY_FAILED_MASK                    UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_EXIT_REASON_, UINT32_C(0), UINT32_MAX,
                            (BASIC, RSVD_16_26, ENCLAVE_MODE, SMM_PENDING_MTF, VMX_ROOT_MODE, RSVD_30, ENTRY_FAILED));
/** @} */


/** @name VM-entry interruption information.
 * @{
 */
#define VMX_ENTRY_INT_INFO_IS_VALID(a)                          (((a) >> 31) & 1)
#define VMX_ENTRY_INT_INFO_VECTOR(a)                            ((a) & 0xff)
#define VMX_ENTRY_INT_INFO_TYPE_SHIFT                           8
#define VMX_ENTRY_INT_INFO_TYPE(a)                              (((a) >> 8) & 7)
#define VMX_ENTRY_INT_INFO_ERROR_CODE_VALID                     RT_BIT(11)
#define VMX_ENTRY_INT_INFO_IS_ERROR_CODE_VALID(a)               (((a) >> 11) & 1)
#define VMX_ENTRY_INT_INFO_NMI_UNBLOCK_IRET                     12
#define VMX_ENTRY_INT_INFO_IS_NMI_UNBLOCK_IRET(a)               (((a) >> 12) & 1)
#define VMX_ENTRY_INT_INFO_VALID                                RT_BIT(31)
#define VMX_ENTRY_INT_INFO_IS_VALID(a)                          (((a) >> 31) & 1)
/** Construct an VM-entry interruption information field from a VM-exit interruption
 *  info value (same except that bit 12 is reserved). */
#define VMX_ENTRY_INT_INFO_FROM_EXIT_INT_INFO(a)                ((a) & ~RT_BIT(12))
/** Construct a VM-entry interruption information field from an IDT-vectoring
 *  information field (same except that bit 12 is reserved). */
#define VMX_ENTRY_INT_INFO_FROM_EXIT_IDT_INFO(a)                ((a) & ~RT_BIT(12))

/** Bit fields for VM-entry interruption information. */
/** The VM-entry interruption vector. */
#define VMX_BF_ENTRY_INT_INFO_VECTOR_SHIFT                      0
#define VMX_BF_ENTRY_INT_INFO_VECTOR_MASK                       UINT32_C(0x000000ff)
/** The VM-entry interruption type (see VMX_ENTRY_INT_INFO_TYPE_XXX). */
#define VMX_BF_ENTRY_INT_INFO_TYPE_SHIFT                        8
#define VMX_BF_ENTRY_INT_INFO_TYPE_MASK                         UINT32_C(0x00000700)
/** Whether this event has an error code.   */
#define VMX_BF_ENTRY_INT_INFO_ERR_CODE_VALID_SHIFT              11
#define VMX_BF_ENTRY_INT_INFO_ERR_CODE_VALID_MASK               UINT32_C(0x00000800)
/** Bits 12:30 are reserved and MBZ. */
#define VMX_BF_ENTRY_INT_INFO_RSVD_12_30_SHIFT                  12
#define VMX_BF_ENTRY_INT_INFO_RSVD_12_30_MASK                   UINT32_C(0x7ffff000)
/** Whether this VM-entry interruption info is valid.  */
#define VMX_BF_ENTRY_INT_INFO_VALID_SHIFT                       31
#define VMX_BF_ENTRY_INT_INFO_VALID_MASK                        UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_ENTRY_INT_INFO_, UINT32_C(0), UINT32_MAX,
                            (VECTOR, TYPE, ERR_CODE_VALID, RSVD_12_30, VALID));
/** @} */

/** @name VM-entry exception error code.
 * @{ */
/** Error code valid mask. */
/** @todo r=ramshankar: Intel spec. 26.2.1.3 "VM-Entry Control Fields" states that
 *        bits 31:15 MBZ. However, Intel spec. 6.13 "Error Code" states "To keep the
 *        stack aligned for doubleword pushes, the upper half of the error code is
 *        reserved" which implies bits 31:16 MBZ (and not 31:15) which is what we
 *        use below. */
#define VMX_ENTRY_INT_XCPT_ERR_CODE_VALID_MASK                  UINT32_C(0xffff)
/** @} */

/** @name VM-entry interruption information types.
 * @{
 */
#define VMX_ENTRY_INT_INFO_TYPE_EXT_INT                         0
#define VMX_ENTRY_INT_INFO_TYPE_RSVD                            1
#define VMX_ENTRY_INT_INFO_TYPE_NMI                             2
#define VMX_ENTRY_INT_INFO_TYPE_HW_XCPT                         3
#define VMX_ENTRY_INT_INFO_TYPE_SW_INT                          4
#define VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT                    5
#define VMX_ENTRY_INT_INFO_TYPE_SW_XCPT                         6
#define VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT                     7
/** @} */


/** @name VM-entry interruption information vector types for
 *        VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT.
 * @{ */
#define VMX_ENTRY_INT_INFO_VECTOR_MTF                           0
/** @} */


/** @name VM-exit interruption information.
 * @{
 */
#define VMX_EXIT_INT_INFO_VECTOR(a)                             ((a) & 0xff)
#define VMX_EXIT_INT_INFO_TYPE_SHIFT                            8
#define VMX_EXIT_INT_INFO_TYPE(a)                               (((a) >> 8) & 7)
#define VMX_EXIT_INT_INFO_ERROR_CODE_VALID                      RT_BIT(11)
#define VMX_EXIT_INT_INFO_IS_ERROR_CODE_VALID(a)                (((a) >> 11) & 1)
#define VMX_EXIT_INT_INFO_NMI_UNBLOCK_IRET                      12
#define VMX_EXIT_INT_INFO_IS_NMI_UNBLOCK_IRET(a)                (((a) >> 12) & 1)
#define VMX_EXIT_INT_INFO_VALID                                 RT_BIT(31)
#define VMX_EXIT_INT_INFO_IS_VALID(a)                           (((a) >> 31) & 1)

/** Bit fields for VM-exit interruption infomration. */
/** The VM-exit interruption vector. */
#define VMX_BF_EXIT_INT_INFO_VECTOR_SHIFT                       0
#define VMX_BF_EXIT_INT_INFO_VECTOR_MASK                        UINT32_C(0x000000ff)
/** The VM-exit interruption type (see VMX_EXIT_INT_INFO_TYPE_XXX). */
#define VMX_BF_EXIT_INT_INFO_TYPE_SHIFT                         8
#define VMX_BF_EXIT_INT_INFO_TYPE_MASK                          UINT32_C(0x00000700)
/** Whether this event has an error code. */
#define VMX_BF_EXIT_INT_INFO_ERR_CODE_VALID_SHIFT               11
#define VMX_BF_EXIT_INT_INFO_ERR_CODE_VALID_MASK                UINT32_C(0x00000800)
/** Whether NMI-unblocking due to IRET is active. */
#define VMX_BF_EXIT_INT_INFO_NMI_UNBLOCK_IRET_SHIFT             12
#define VMX_BF_EXIT_INT_INFO_NMI_UNBLOCK_IRET_MASK              UINT32_C(0x00001000)
/** Bits 13:30 is reserved (MBZ). */
#define VMX_BF_EXIT_INT_INFO_RSVD_13_30_SHIFT                   13
#define VMX_BF_EXIT_INT_INFO_RSVD_13_30_MASK                    UINT32_C(0x7fffe000)
/** Whether this VM-exit interruption info is valid. */
#define VMX_BF_EXIT_INT_INFO_VALID_SHIFT                        31
#define VMX_BF_EXIT_INT_INFO_VALID_MASK                         UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_EXIT_INT_INFO_, UINT32_C(0), UINT32_MAX,
                            (VECTOR, TYPE, ERR_CODE_VALID, NMI_UNBLOCK_IRET, RSVD_13_30, VALID));
/** @} */


/** @name VM-exit interruption information types.
 * @{
 */
#define VMX_EXIT_INT_INFO_TYPE_EXT_INT                          0
#define VMX_EXIT_INT_INFO_TYPE_NMI                              2
#define VMX_EXIT_INT_INFO_TYPE_HW_XCPT                          3
#define VMX_EXIT_INT_INFO_TYPE_SW_INT                           4
#define VMX_EXIT_INT_INFO_TYPE_PRIV_SW_XCPT                     5
#define VMX_EXIT_INT_INFO_TYPE_SW_XCPT                          6
#define VMX_EXIT_INT_INFO_TYPE_UNUSED                           7
/** @} */


/** @name VM-exit instruction identity.
 *
 * These are found in VM-exit instruction information fields for certain
 * instructions.
 * @{ */
typedef uint32_t VMXINSTRID;
/** Whether the instruction ID field is valid. */
#define VMXINSTRID_VALID                                        RT_BIT_32(31)
/** Whether the instruction's primary operand in the Mod R/M byte (bits 0:3) is a
 *  read or write. */
#define VMXINSTRID_MODRM_PRIMARY_OP_W                           RT_BIT_32(30)
/** Gets whether the instruction ID is valid or not.  */
#define VMXINSTRID_IS_VALID(a)                                  (((a) >> 31) & 1)
#define VMXINSTRID_IS_MODRM_PRIMARY_OP_W(a)                     (((a) >> 30) & 1)
/** Gets the instruction ID.  */
#define VMXINSTRID_GET_ID(a)                                    ((a) & ~(VMXINSTRID_VALID | VMXINSTRID_MODRM_PRIMARY_OP_W))
/** No instruction ID info. */
#define VMXINSTRID_NONE                                         0

/** The OR'd rvalues are from the VT-x spec (valid bit is VBox specific): */
#define VMXINSTRID_SGDT                                         (0x0 | VMXINSTRID_VALID | VMXINSTRID_MODRM_PRIMARY_OP_W)
#define VMXINSTRID_SIDT                                         (0x1 | VMXINSTRID_VALID | VMXINSTRID_MODRM_PRIMARY_OP_W)
#define VMXINSTRID_LGDT                                         (0x2 | VMXINSTRID_VALID)
#define VMXINSTRID_LIDT                                         (0x3 | VMXINSTRID_VALID)

#define VMXINSTRID_SLDT                                         (0x0 | VMXINSTRID_VALID | VMXINSTRID_MODRM_PRIMARY_OP_W)
#define VMXINSTRID_STR                                          (0x1 | VMXINSTRID_VALID | VMXINSTRID_MODRM_PRIMARY_OP_W)
#define VMXINSTRID_LLDT                                         (0x2 | VMXINSTRID_VALID)
#define VMXINSTRID_LTR                                          (0x3 | VMXINSTRID_VALID)

/** The following IDs are used internally (some for logging, others for conveying
 *  the ModR/M primary operand write bit): */
#define VMXINSTRID_VMLAUNCH                                     (0x10 | VMXINSTRID_VALID)
#define VMXINSTRID_VMRESUME                                     (0x11 | VMXINSTRID_VALID)
#define VMXINSTRID_VMREAD                                       (0x12 | VMXINSTRID_VALID)
#define VMXINSTRID_VMWRITE                                      (0x13 | VMXINSTRID_VALID | VMXINSTRID_MODRM_PRIMARY_OP_W)
/** @} */


/** @name IDT-vectoring information.
 * @{
 */
#define VMX_IDT_VECTORING_INFO_VECTOR(a)                        ((a) & 0xff)
#define VMX_IDT_VECTORING_INFO_TYPE(a)                          (((a) >> 8) & 7)
#define VMX_IDT_VECTORING_INFO_IS_ERROR_CODE_VALID(a)           (((a) >> 11) & 1)
#define VMX_IDT_VECTORING_INFO_IS_VALID(a)                      (((a) >> 31) & 1)

/** Construct an IDT-vectoring information field from an VM-entry interruption
 *  information field (same except that bit 12 is reserved). */
#define VMX_EXIT_IDT_INFO_FROM_ENTRY_INT_INFO(a)                ((a) & ~RT_BIT(12))

/** Bit fields for IDT-vectoring information. */
/** The IDT-vectoring info vector. */
#define VMX_BF_IDT_VECTORING_INFO_VECTOR_SHIFT                  0
#define VMX_BF_IDT_VECTORING_INFO_VECTOR_MASK                   UINT32_C(0x000000ff)
/** The IDT-vectoring info type (see VMX_IDT_VECTORING_INFO_TYPE_XXX). */
#define VMX_BF_IDT_VECTORING_INFO_TYPE_SHIFT                    8
#define VMX_BF_IDT_VECTORING_INFO_TYPE_MASK                     UINT32_C(0x00000700)
/** Whether the event has an error code. */
#define VMX_BF_IDT_VECTORING_INFO_ERR_CODE_VALID_SHIFT          11
#define VMX_BF_IDT_VECTORING_INFO_ERR_CODE_VALID_MASK           UINT32_C(0x00000800)
/** Bit 12 is undefined. */
#define VMX_BF_IDT_VECTORING_INFO_UNDEF_12_SHIFT                12
#define VMX_BF_IDT_VECTORING_INFO_UNDEF_12_MASK                 UINT32_C(0x00001000)
/** Bits 13:30 is reserved (MBZ). */
#define VMX_BF_IDT_VECTORING_INFO_RSVD_13_30_SHIFT              13
#define VMX_BF_IDT_VECTORING_INFO_RSVD_13_30_MASK               UINT32_C(0x7fffe000)
/** Whether this IDT-vectoring info is valid. */
#define VMX_BF_IDT_VECTORING_INFO_VALID_SHIFT                   31
#define VMX_BF_IDT_VECTORING_INFO_VALID_MASK                    UINT32_C(0x80000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_IDT_VECTORING_INFO_, UINT32_C(0), UINT32_MAX,
                            (VECTOR, TYPE, ERR_CODE_VALID, UNDEF_12, RSVD_13_30, VALID));
/** @} */


/** @name IDT-vectoring information vector types.
 * @{
 */
#define VMX_IDT_VECTORING_INFO_TYPE_EXT_INT                     0
#define VMX_IDT_VECTORING_INFO_TYPE_NMI                         2
#define VMX_IDT_VECTORING_INFO_TYPE_HW_XCPT                     3
#define VMX_IDT_VECTORING_INFO_TYPE_SW_INT                      4
#define VMX_IDT_VECTORING_INFO_TYPE_PRIV_SW_XCPT                5
#define VMX_IDT_VECTORING_INFO_TYPE_SW_XCPT                     6
#define VMX_IDT_VECTORING_INFO_TYPE_SW_UNUSED                   7
/** @} */


/** @name TPR threshold.
 * @{ */
/** Mask of the TPR threshold field (bits 31:4 MBZ). */
#define VMX_TPR_THRESHOLD_MASK                                   UINT32_C(0xf)

/** Bit fields for TPR threshold. */
#define VMX_BF_TPR_THRESHOLD_TPR_SHIFT                           0
#define VMX_BF_TPR_THRESHOLD_TPR_MASK                            UINT32_C(0x0000000f)
#define VMX_BF_TPR_THRESHOLD_RSVD_4_31_SHIFT                     4
#define VMX_BF_TPR_THRESHOLD_RSVD_4_31_MASK                      UINT32_C(0xfffffff0)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_TPR_THRESHOLD_, UINT32_C(0), UINT32_MAX,
                            (TPR, RSVD_4_31));
/** @} */


/** @name Guest-activity states.
 * @{
 */
/** The logical processor is active. */
#define VMX_VMCS_GUEST_ACTIVITY_ACTIVE                          0x0
/** The logical processor is inactive, because it executed a HLT instruction. */
#define VMX_VMCS_GUEST_ACTIVITY_HLT                             0x1
/** The logical processor is inactive, because of a triple fault or other serious error. */
#define VMX_VMCS_GUEST_ACTIVITY_SHUTDOWN                        0x2
/** The logical processor is inactive, because it's waiting for a startup-IPI */
#define VMX_VMCS_GUEST_ACTIVITY_SIPI_WAIT                       0x3
/** @} */


/** @name Guest-interruptibility states.
 * @{
 */
#define VMX_VMCS_GUEST_INT_STATE_BLOCK_STI                      RT_BIT(0)
#define VMX_VMCS_GUEST_INT_STATE_BLOCK_MOVSS                    RT_BIT(1)
#define VMX_VMCS_GUEST_INT_STATE_BLOCK_SMI                      RT_BIT(2)
#define VMX_VMCS_GUEST_INT_STATE_BLOCK_NMI                      RT_BIT(3)
#define VMX_VMCS_GUEST_INT_STATE_ENCLAVE                        RT_BIT(4)

/** Mask of the guest-interruptibility state field (bits 31:5 MBZ). */
#define VMX_VMCS_GUEST_INT_STATE_MASK                           UINT32_C(0x1f)
/** @} */


/** @name Exit qualification for Mov DRx.
 * @{
 */
/** 0-2:  Debug register number */
#define VMX_EXIT_QUAL_DRX_REGISTER(a)                           ((a) & 7)
/** 3:    Reserved; cleared to 0. */
#define VMX_EXIT_QUAL_DRX_RES1(a)                               (((a) >> 3) & 1)
/** 4:    Direction of move (0 = write, 1 = read) */
#define VMX_EXIT_QUAL_DRX_DIRECTION(a)                          (((a) >> 4) & 1)
/** 5-7:  Reserved; cleared to 0. */
#define VMX_EXIT_QUAL_DRX_RES2(a)                               (((a) >> 5) & 7)
/** 8-11: General purpose register number. */
#define VMX_EXIT_QUAL_DRX_GENREG(a)                             (((a) >> 8) & 0xf)
/** Rest: reserved. */
/** @} */


/** @name Exit qualification for debug exceptions types.
 * @{
 */
#define VMX_EXIT_QUAL_DRX_DIRECTION_WRITE                       0
#define VMX_EXIT_QUAL_DRX_DIRECTION_READ                        1
/** @} */


/** @name Exit qualification for control-register accesses.
 * @{
 */
/** 0-3:   Control register number (0 for CLTS & LMSW) */
#define VMX_EXIT_QUAL_CRX_REGISTER(a)                           ((a) & 0xf)
/** 4-5:   Access type. */
#define VMX_EXIT_QUAL_CRX_ACCESS(a)                             (((a) >> 4) & 3)
/** 6:     LMSW operand type */
#define VMX_EXIT_QUAL_CRX_LMSW_OP(a)                            (((a) >> 6) & 1)
/** 7:     Reserved; cleared to 0. */
#define VMX_EXIT_QUAL_CRX_RES1(a)                               (((a) >> 7) & 1)
/** 8-11:  General purpose register number (0 for CLTS & LMSW). */
#define VMX_EXIT_QUAL_CRX_GENREG(a)                             (((a) >> 8) & 0xf)
/** 12-15: Reserved; cleared to 0. */
#define VMX_EXIT_QUAL_CRX_RES2(a)                               (((a) >> 12) & 0xf)
/** 16-31: LMSW source data (else 0). */
#define VMX_EXIT_QUAL_CRX_LMSW_DATA(a)                          (((a) >> 16) & 0xffff)
/* Rest: reserved. */
/** @} */


/** @name Exit qualification for control-register access types.
 * @{
 */
#define VMX_EXIT_QUAL_CRX_ACCESS_WRITE                          0
#define VMX_EXIT_QUAL_CRX_ACCESS_READ                           1
#define VMX_EXIT_QUAL_CRX_ACCESS_CLTS                           2
#define VMX_EXIT_QUAL_CRX_ACCESS_LMSW                           3
/** @} */


/** @name Exit qualification for task switch.
 * @{
 */
#define VMX_EXIT_QUAL_TASK_SWITCH_SELECTOR(a)                   ((a) & 0xffff)
#define VMX_EXIT_QUAL_TASK_SWITCH_TYPE(a)                       (((a) >> 30) & 0x3)
/** Task switch caused by a call instruction. */
#define VMX_EXIT_QUAL_TASK_SWITCH_TYPE_CALL                     0
/** Task switch caused by an iret instruction. */
#define VMX_EXIT_QUAL_TASK_SWITCH_TYPE_IRET                     1
/** Task switch caused by a jmp instruction. */
#define VMX_EXIT_QUAL_TASK_SWITCH_TYPE_JMP                      2
/** Task switch caused by an interrupt gate. */
#define VMX_EXIT_QUAL_TASK_SWITCH_TYPE_IDT                      3
/** @} */


/** @name Exit qualification for EPT violations.
 * @{
 */
/** Set if the violation was caused by a data read. */
#define VMX_EXIT_QUAL_EPT_DATA_READ                             RT_BIT(0)
/** Set if the violation was caused by a data write. */
#define VMX_EXIT_QUAL_EPT_DATA_WRITE                            RT_BIT(1)
/** Set if the violation was caused by an instruction fetch. */
#define VMX_EXIT_QUAL_EPT_INSTR_FETCH                           RT_BIT(2)
/** AND of the present bit of all EPT structures. */
#define VMX_EXIT_QUAL_EPT_ENTRY_PRESENT                         RT_BIT(3)
/** AND of the write bit of all EPT structures. */
#define VMX_EXIT_QUAL_EPT_ENTRY_WRITE                           RT_BIT(4)
/** AND of the execute bit of all EPT structures. */
#define VMX_EXIT_QUAL_EPT_ENTRY_EXECUTE                         RT_BIT(5)
/** Set if the guest linear address field contains the faulting address. */
#define VMX_EXIT_QUAL_EPT_GUEST_ADDR_VALID                      RT_BIT(7)
/** If bit 7 is one: (reserved otherwise)
 *  1 - violation due to physical address access.
 *  0 - violation caused by page walk or access/dirty bit updates
 */
#define VMX_EXIT_QUAL_EPT_TRANSLATED_ACCESS                     RT_BIT(8)
/** @} */


/** @name Exit qualification for I/O instructions.
 * @{
 */
/** 0-2:   IO operation width. */
#define VMX_EXIT_QUAL_IO_WIDTH(a)                               ((a) & 7)
/** 3:     IO operation direction. */
#define VMX_EXIT_QUAL_IO_DIRECTION(a)                           (((a) >> 3) & 1)
/** 4:     String IO operation (INS / OUTS). */
#define VMX_EXIT_QUAL_IO_IS_STRING(a)                           (((a) >> 4) & 1)
/** 5:     Repeated IO operation. */
#define VMX_EXIT_QUAL_IO_IS_REP(a)                              (((a) >> 5) & 1)
/** 6:     Operand encoding. */
#define VMX_EXIT_QUAL_IO_ENCODING(a)                            (((a) >> 6) & 1)
/** 16-31: IO Port (0-0xffff). */
#define VMX_EXIT_QUAL_IO_PORT(a)                                (((a) >> 16) & 0xffff)
/* Rest reserved. */
/** @} */


/** @name Exit qualification for I/O instruction types.
 * @{
 */
#define VMX_EXIT_QUAL_IO_DIRECTION_OUT                          0
#define VMX_EXIT_QUAL_IO_DIRECTION_IN                           1
/** @} */


/** @name Exit qualification for I/O instruction encoding.
 * @{
 */
#define VMX_EXIT_QUAL_IO_ENCODING_DX                            0
#define VMX_EXIT_QUAL_IO_ENCODING_IMM                           1
/** @} */


/** @name Exit qualification for APIC-access VM-exits from linear and
 *        guest-physical accesses.
 * @{
 */
/** 0-11: If the APIC-access VM-exit is due to a linear access, the offset of
 *  access within the APIC page. */
#define VMX_EXIT_QUAL_APIC_ACCESS_OFFSET(a)                     ((a) & 0xfff)
/** 12-15: Access type. */
#define VMX_EXIT_QUAL_APIC_ACCESS_TYPE(a)                       (((a) & 0xf000) >> 12)
/* Rest reserved. */
/** @} */


/** @name Exit qualification for linear address APIC-access types.
 * @{
 */
/** Linear read access. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_READ                        0
/** Linear write access. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_WRITE                       1
/** Linear instruction fetch access. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_INSTR_FETCH                 2
/** Linear read/write access during event delivery. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_EVENT_DELIVERY              3
/** Physical read/write access during event delivery. */
#define VMX_APIC_ACCESS_TYPE_PHYSICAL_EVENT_DELIVERY            10
/** Physical access for an instruction fetch or during instruction execution. */
#define VMX_APIC_ACCESS_TYPE_PHYSICAL_INSTR                     15
/** @} */


/** @name VMX_BF_XXTR_INSINFO_XXX - VMX_EXIT_XDTR_ACCESS instruction information.
 * Found in VMX_VMCS32_RO_EXIT_INSTR_INFO.
 * @{
 */
/** Address calculation scaling field (powers of two). */
#define VMX_BF_XDTR_INSINFO_SCALE_SHIFT                         0
#define VMX_BF_XDTR_INSINFO_SCALE_MASK                          UINT32_C(0x00000003)
/** Bits 2 thru 6 are undefined. */
#define VMX_BF_XDTR_INSINFO_UNDEF_2_6_SHIFT                     2
#define VMX_BF_XDTR_INSINFO_UNDEF_2_6_MASK                      UINT32_C(0x0000007c)
/** Address size, only 0(=16), 1(=32) and 2(=64) are defined.
 * @remarks anyone's guess why this is a 3 bit field...  */
#define VMX_BF_XDTR_INSINFO_ADDR_SIZE_SHIFT                     7
#define VMX_BF_XDTR_INSINFO_ADDR_SIZE_MASK                      UINT32_C(0x00000380)
/** Bit 10 is defined as zero. */
#define VMX_BF_XDTR_INSINFO_ZERO_10_SHIFT                       10
#define VMX_BF_XDTR_INSINFO_ZERO_10_MASK                        UINT32_C(0x00000400)
/** Operand size, either (1=)32-bit or (0=)16-bit, but get this, it's undefined
 * for exits from 64-bit code as the operand size there is fixed. */
#define VMX_BF_XDTR_INSINFO_OP_SIZE_SHIFT                       11
#define VMX_BF_XDTR_INSINFO_OP_SIZE_MASK                        UINT32_C(0x00000800)
/** Bits 12 thru 14 are undefined. */
#define VMX_BF_XDTR_INSINFO_UNDEF_12_14_SHIFT                   12
#define VMX_BF_XDTR_INSINFO_UNDEF_12_14_MASK                    UINT32_C(0x00007000)
/** Applicable segment register (X86_SREG_XXX values). */
#define VMX_BF_XDTR_INSINFO_SREG_SHIFT                          15
#define VMX_BF_XDTR_INSINFO_SREG_MASK                           UINT32_C(0x00038000)
/** Index register (X86_GREG_XXX values). Undefined if HAS_INDEX_REG is clear. */
#define VMX_BF_XDTR_INSINFO_INDEX_REG_SHIFT                     18
#define VMX_BF_XDTR_INSINFO_INDEX_REG_MASK                      UINT32_C(0x003c0000)
/** Is VMX_BF_XDTR_INSINFO_INDEX_REG_XXX valid (=1) or not (=0). */
#define VMX_BF_XDTR_INSINFO_HAS_INDEX_REG_SHIFT                 22
#define VMX_BF_XDTR_INSINFO_HAS_INDEX_REG_MASK                  UINT32_C(0x00400000)
/** Base register (X86_GREG_XXX values). Undefined if HAS_BASE_REG is clear. */
#define VMX_BF_XDTR_INSINFO_BASE_REG_SHIFT                      23
#define VMX_BF_XDTR_INSINFO_BASE_REG_MASK                       UINT32_C(0x07800000)
/** Is VMX_XDTR_INSINFO_BASE_REG_XXX valid (=1) or not (=0). */
#define VMX_BF_XDTR_INSINFO_HAS_BASE_REG_SHIFT                  27
#define VMX_BF_XDTR_INSINFO_HAS_BASE_REG_MASK                   UINT32_C(0x08000000)
/** The instruction identity (VMX_XDTR_INSINFO_II_XXX values). */
#define VMX_BF_XDTR_INSINFO_INSTR_ID_SHIFT                      28
#define VMX_BF_XDTR_INSINFO_INSTR_ID_MASK                       UINT32_C(0x30000000)
#define VMX_XDTR_INSINFO_II_SGDT                                0 /**< Instruction ID: SGDT */
#define VMX_XDTR_INSINFO_II_SIDT                                1 /**< Instruction ID: SIDT */
#define VMX_XDTR_INSINFO_II_LGDT                                2 /**< Instruction ID: LGDT */
#define VMX_XDTR_INSINFO_II_LIDT                                3 /**< Instruction ID: LIDT */
/** Bits 30 & 31 are undefined. */
#define VMX_BF_XDTR_INSINFO_UNDEF_30_31_SHIFT                   30
#define VMX_BF_XDTR_INSINFO_UNDEF_30_31_MASK                    UINT32_C(0xc0000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_XDTR_INSINFO_, UINT32_C(0), UINT32_MAX,
                            (SCALE, UNDEF_2_6, ADDR_SIZE, ZERO_10, OP_SIZE, UNDEF_12_14, SREG, INDEX_REG, HAS_INDEX_REG,
                             BASE_REG, HAS_BASE_REG, INSTR_ID, UNDEF_30_31));
/** @} */


/** @name VMX_BF_YYTR_INSINFO_XXX - VMX_EXIT_TR_ACCESS instruction information.
 * Found in VMX_VMCS32_RO_EXIT_INSTR_INFO.
 * This is similar to VMX_BF_XDTR_INSINFO_XXX.
 * @{
 */
/** Address calculation scaling field (powers of two). */
#define VMX_BF_YYTR_INSINFO_SCALE_SHIFT                         0
#define VMX_BF_YYTR_INSINFO_SCALE_MASK                          UINT32_C(0x00000003)
/** Bit 2 is undefined. */
#define VMX_BF_YYTR_INSINFO_UNDEF_2_SHIFT                       2
#define VMX_BF_YYTR_INSINFO_UNDEF_2_MASK                        UINT32_C(0x00000004)
/** Register operand 1. Undefined if VMX_YYTR_INSINFO_HAS_REG1 is clear. */
#define VMX_BF_YYTR_INSINFO_REG1_SHIFT                          3
#define VMX_BF_YYTR_INSINFO_REG1_MASK                           UINT32_C(0x00000078)
/** Address size, only 0(=16), 1(=32) and 2(=64) are defined.
 * @remarks anyone's guess why this is a 3 bit field...  */
#define VMX_BF_YYTR_INSINFO_ADDR_SIZE_SHIFT                     7
#define VMX_BF_YYTR_INSINFO_ADDR_SIZE_MASK                      UINT32_C(0x00000380)
/** Is VMX_YYTR_INSINFO_REG1_XXX valid (=1) or not (=0). */
#define VMX_BF_YYTR_INSINFO_HAS_REG1_SHIFT                      10
#define VMX_BF_YYTR_INSINFO_HAS_REG1_MASK                       UINT32_C(0x00000400)
/** Bits 11 thru 14 are undefined. */
#define VMX_BF_YYTR_INSINFO_UNDEF_11_14_SHIFT                   11
#define VMX_BF_YYTR_INSINFO_UNDEF_11_14_MASK                    UINT32_C(0x00007800)
/** Applicable segment register (X86_SREG_XXX values). */
#define VMX_BF_YYTR_INSINFO_SREG_SHIFT                          15
#define VMX_BF_YYTR_INSINFO_SREG_MASK                           UINT32_C(0x00038000)
/** Index register (X86_GREG_XXX values). Undefined if HAS_INDEX_REG is clear. */
#define VMX_BF_YYTR_INSINFO_INDEX_REG_SHIFT                     18
#define VMX_BF_YYTR_INSINFO_INDEX_REG_MASK                      UINT32_C(0x003c0000)
/** Is VMX_YYTR_INSINFO_INDEX_REG_XXX valid (=1) or not (=0). */
#define VMX_BF_YYTR_INSINFO_HAS_INDEX_REG_SHIFT                 22
#define VMX_BF_YYTR_INSINFO_HAS_INDEX_REG_MASK                  UINT32_C(0x00400000)
/** Base register (X86_GREG_XXX values). Undefined if HAS_BASE_REG is clear. */
#define VMX_BF_YYTR_INSINFO_BASE_REG_SHIFT                      23
#define VMX_BF_YYTR_INSINFO_BASE_REG_MASK                       UINT32_C(0x07800000)
/** Is VMX_YYTR_INSINFO_BASE_REG_XXX valid (=1) or not (=0). */
#define VMX_BF_YYTR_INSINFO_HAS_BASE_REG_SHIFT                  27
#define VMX_BF_YYTR_INSINFO_HAS_BASE_REG_MASK                   UINT32_C(0x08000000)
/** The instruction identity (VMX_YYTR_INSINFO_II_XXX values) */
#define VMX_BF_YYTR_INSINFO_INSTR_ID_SHIFT                      28
#define VMX_BF_YYTR_INSINFO_INSTR_ID_MASK                       UINT32_C(0x30000000)
#define VMX_YYTR_INSINFO_II_SLDT                                0 /**< Instruction ID: SLDT */
#define VMX_YYTR_INSINFO_II_STR                                 1 /**< Instruction ID: STR */
#define VMX_YYTR_INSINFO_II_LLDT                                2 /**< Instruction ID: LLDT */
#define VMX_YYTR_INSINFO_II_LTR                                 3 /**< Instruction ID: LTR */
/** Bits 30 & 31 are undefined. */
#define VMX_BF_YYTR_INSINFO_UNDEF_30_31_SHIFT                   30
#define VMX_BF_YYTR_INSINFO_UNDEF_30_31_MASK                    UINT32_C(0xc0000000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_YYTR_INSINFO_, UINT32_C(0), UINT32_MAX,
                            (SCALE, UNDEF_2, REG1, ADDR_SIZE, HAS_REG1, UNDEF_11_14, SREG, INDEX_REG, HAS_INDEX_REG,
                             BASE_REG, HAS_BASE_REG, INSTR_ID, UNDEF_30_31));
/** @} */


/** @name Format of Pending-Debug-Exceptions.
 * Bits 4-11, 13, 15 and 17-63 are reserved.
 * Similar to DR6 except bit 12 (breakpoint enabled) and bit 16 (RTM) are both
 * possibly valid here but not in DR6.
 * @{
 */
/** Hardware breakpoint 0 was met. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP0                   RT_BIT_64(0)
/** Hardware breakpoint 1 was met. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP1                   RT_BIT_64(1)
/** Hardware breakpoint 2 was met. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP2                   RT_BIT_64(2)
/** Hardware breakpoint 3 was met. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP3                   RT_BIT_64(3)
/** At least one data or IO breakpoint was hit. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_EN_BP                 RT_BIT_64(12)
/** A debug exception would have been triggered by single-step execution mode. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BS                    RT_BIT_64(14)
/** A debug exception occurred inside an RTM region.   */
#define VMX_VMCS_GUEST_PENDING_DEBUG_RTM                        RT_BIT_64(16)
/** Mask of valid bits. */
#define VMX_VMCS_GUEST_PENDING_DEBUG_VALID_MASK                 (  VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP0 \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP1 \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP2 \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BP3 \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_EN_BP \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BS \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_RTM)
#define VMX_VMCS_GUEST_PENDING_DEBUG_RTM_MASK                   (  VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_EN_BP \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_XCPT_BS \
                                                                 | VMX_VMCS_GUEST_PENDING_DEBUG_RTM)
/** Bit fields for Pending debug exceptions. */
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP0_SHIFT                  0
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP0_MASK                   UINT64_C(0x0000000000000001)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP1_SHIFT                  1
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP1_MASK                   UINT64_C(0x0000000000000002)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP2_SHIFT                  2
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP2_MASK                   UINT64_C(0x0000000000000004)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP3_SHIFT                  3
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BP3_MASK                   UINT64_C(0x0000000000000008)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_4_11_SHIFT            4
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_4_11_MASK             UINT64_C(0x0000000000000ff0)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_EN_BP_SHIFT                12
#define VMX_BF_VMCS_PENDING_DBG_XCPT_EN_BP_MASK                 UINT64_C(0x0000000000001000)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_13_SHIFT              13
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_13_MASK               UINT64_C(0x0000000000002000)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BS_SHIFT                   14
#define VMX_BF_VMCS_PENDING_DBG_XCPT_BS_MASK                    UINT64_C(0x0000000000004000)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_15_SHIFT              15
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_15_MASK               UINT64_C(0x0000000000008000)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RTM_SHIFT                  16
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RTM_MASK                   UINT64_C(0x0000000000010000)
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_17_63_SHIFT           17
#define VMX_BF_VMCS_PENDING_DBG_XCPT_RSVD_17_63_MASK            UINT64_C(0xfffffffffffe0000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_VMCS_PENDING_DBG_XCPT_, UINT64_C(0), UINT64_MAX,
                            (BP0, BP1, BP2, BP3, RSVD_4_11, EN_BP, RSVD_13, BS, RSVD_15, RTM, RSVD_17_63));
/** @} */


/** @name VMCS field encoding.
 * @{ */
typedef union
{
    struct
    {
        /** The access type; 0=full, 1=high of 64-bit fields. */
        uint32_t    fAccessType  : 1;
        /** The index. */
        uint32_t    u8Index      : 8;
        /** The type; 0=control, 1=VM-exit info, 2=guest-state, 3=host-state.  */
        uint32_t    u2Type       : 2;
        /** Reserved (MBZ). */
        uint32_t    u1Reserved0  : 1;
        /** The width; 0=16-bit, 1=64-bit, 2=32-bit, 3=natural-width. */
        uint32_t    u2Width      : 2;
        /** Reserved (MBZ). */
        uint32_t    u18Reserved0 : 18;
    } n;
    /* The unsigned integer view. */
    uint32_t    u;
} VMXVMCSFIELDENC;
AssertCompileSize(VMXVMCSFIELDENC, 4);
/** Pointer to a VMCS field encoding. */
typedef VMXVMCSFIELDENC *PVMXVMCSFIELDENC;
/** Pointer to a const VMCS field encoding. */
typedef const VMXVMCSFIELDENC *PCVMXVMCSFIELDENC;

/** VMCS field encoding type: Full. */
#define VMX_VMCS_ENC_ACCESS_TYPE_FULL                           0
/** VMCS field encoding type: High. */
#define VMX_VMCS_ENC_ACCESS_TYPE_HIGH                           1

/** VMCS field encoding type: Control. */
#define VMX_VMCS_ENC_TYPE_CONTROL                               0
/** VMCS field encoding type: VM-exit information / read-only fields. */
#define VMX_VMCS_ENC_TYPE_VMEXIT_INFO                           1
/** VMCS field encoding type: Guest-state. */
#define VMX_VMCS_ENC_TYPE_GUEST_STATE                           2
/** VMCS field encoding type: Host-state. */
#define VMX_VMCS_ENC_TYPE_HOST_STATE                            3

/** VMCS field encoding width: 16-bit. */
#define VMX_VMCS_ENC_WIDTH_16BIT                                0
/** VMCS field encoding width: 64-bit. */
#define VMX_VMCS_ENC_WIDTH_64BIT                                1
/** VMCS field encoding width: 32-bit. */
#define VMX_VMCS_ENC_WIDTH_32BIT                                2
/** VMCS field encoding width: Natural width. */
#define VMX_VMCS_ENC_WIDTH_NATURAL                              3

/** VMCS field encoding: Mask of reserved bits (bits 63:15 MBZ), bit 12 is
 *  not included! */
#define VMX_VMCS_ENC_RSVD_MASK                                  UINT64_C(0xffffffffffff8000)

/** Bits fields for VMCS field encoding. */
#define VMX_BF_VMCS_ENC_ACCESS_TYPE_SHIFT                       0
#define VMX_BF_VMCS_ENC_ACCESS_TYPE_MASK                        UINT32_C(0x00000001)
#define VMX_BF_VMCS_ENC_INDEX_SHIFT                             1
#define VMX_BF_VMCS_ENC_INDEX_MASK                              UINT32_C(0x000003fe)
#define VMX_BF_VMCS_ENC_TYPE_SHIFT                              10
#define VMX_BF_VMCS_ENC_TYPE_MASK                               UINT32_C(0x00000c00)
#define VMX_BF_VMCS_ENC_RSVD_12_SHIFT                           12
#define VMX_BF_VMCS_ENC_RSVD_12_MASK                            UINT32_C(0x00001000)
#define VMX_BF_VMCS_ENC_WIDTH_SHIFT                             13
#define VMX_BF_VMCS_ENC_WIDTH_MASK                              UINT32_C(0x00006000)
#define VMX_BF_VMCS_ENC_RSVD_15_31_SHIFT                        15
#define VMX_BF_VMCS_ENC_RSVD_15_31_MASK                         UINT32_C(0xffff8000)
RT_BF_ASSERT_COMPILE_CHECKS(VMX_BF_VMCS_ENC_, UINT32_C(0), UINT32_MAX,
                            (ACCESS_TYPE, INDEX, TYPE, RSVD_12, WIDTH, RSVD_15_31));
/** @} */


/** @defgroup grp_hm_vmx_virt    VMX virtualization.
 * @{
 */

/** @name Virtual VMX MSR - Miscellaneous data.
 * @{ */
/** Number of CR3-target values supported. */
#define VMX_V_CR3_TARGET_COUNT                                  4
/** Activity states supported. */
#define VMX_V_GUEST_ACTIVITY_STATE_MASK                         (VMX_VMCS_GUEST_ACTIVITY_HLT | VMX_VMCS_GUEST_ACTIVITY_SHUTDOWN)
/** VMX preemption-timer shift (Core i7-2600 taken as reference). */
#define VMX_V_PREEMPT_TIMER_SHIFT                               5
/** Maximum number of MSRs in the auto-load/store MSR areas, (n+1) * 512. */
#define VMX_V_AUTOMSR_COUNT_MAX                                 0
/** SMM MSEG revision ID. */
#define VMX_V_MSEG_REV_ID                                       0
/** @} */

/** @name VMX_V_VMCS_STATE_XXX - Virtual VMCS state.
 * @{ */
/** VMCS state clear. */
#define VMX_V_VMCS_STATE_CLEAR          RT_BIT(1)
/** VMCS state launched. */
#define VMX_V_VMCS_STATE_LAUNCHED       RT_BIT(2)
/** @} */

/** CR0 bits set here must always be set when in VMX operation. */
#define VMX_V_CR0_FIXED0                                        (X86_CR0_PE | X86_CR0_NE | X86_CR0_PG)
/** VMX_V_CR0_FIXED0 when unrestricted-guest execution is supported for the guest. */
#define VMX_V_CR0_FIXED0_UX                                     (VMX_V_CR0_FIXED0 & ~(X86_CR0_PE | X86_CR0_PG))
/** CR4 bits set here must always be set when in VMX operation. */
#define VMX_V_CR4_FIXED0                                        (X86_CR4_VMXE)

/** Virtual VMCS revision ID. Bump this arbitarily chosen identifier if incompatible
 *  changes to the layout of VMXVVMCS is done.  Bit 31 MBZ.  */
#define VMX_V_VMCS_REVISION_ID                                  UINT32_C(0x1d000001)
AssertCompile(!(VMX_V_VMCS_REVISION_ID & RT_BIT(31)));

/** The size of the virtual VMCS region (we use the maximum allowed size to avoid
 *  complications when teleporation may be implemented). */
#define VMX_V_VMCS_SIZE                                         X86_PAGE_4K_SIZE
/** The size of the virtual VMCS region (in pages). */
#define VMX_V_VMCS_PAGES                                        1

/** The size of the Virtual-APIC page (in bytes).  */
#define VMX_V_VIRT_APIC_SIZE                                    X86_PAGE_4K_SIZE
/** The size of the Virtual-APIC page (in pages). */
#define VMX_V_VIRT_APIC_PAGES                                   1

/** The size of the VMREAD/VMWRITE bitmap (in bytes). */
#define VMX_V_VMREAD_VMWRITE_BITMAP_SIZE                        X86_PAGE_4K_SIZE
/** The size of the VMREAD/VMWRITE-bitmap (in pages). */
#define VMX_V_VMREAD_VMWRITE_BITMAP_PAGES                       1

/** The size of the auto-load/store MSR area (in bytes). */
#define VMX_V_AUTOMSR_AREA_SIZE                                 ((512 * (VMX_V_AUTOMSR_COUNT_MAX + 1)) * sizeof(VMXAUTOMSR))
/* Assert that the size is page aligned or adjust the VMX_V_AUTOMSR_AREA_PAGES macro below. */
AssertCompile(RT_ALIGN_Z(VMX_V_AUTOMSR_AREA_SIZE, X86_PAGE_4K_SIZE) == VMX_V_AUTOMSR_AREA_SIZE);
/** The size of the auto-load/store MSR area (in pages). */
#define VMX_V_AUTOMSR_AREA_PAGES                                ((VMX_V_AUTOMSR_AREA_SIZE) >> X86_PAGE_4K_SHIFT)

/** The highest index value used for supported virtual VMCS field encoding. */
#define VMX_V_VMCS_MAX_INDEX                                    RT_BF_GET(VMX_VMCS64_CTRL_TSC_MULTIPLIER_HIGH, VMX_BF_VMCS_ENC_INDEX)

/**
 * Virtual VM-Exit information.
 *
 * This is a convenience structure that bundles some VM-exit information related
 * fields together.
 */
typedef struct
{
    /** The VM-exit reason. */
    uint32_t                uReason;
    /** The VM-exit instruction length. */
    uint32_t                cbInstr;
    /** The VM-exit instruction information. */
    VMXEXITINSTRINFO        InstrInfo;
    /** The VM-exit instruction ID. */
    VMXINSTRID              uInstrId;

    /** The VM-exit qualification field. */
    uint64_t                u64Qual;
    /** The guest-linear address field. */
    uint64_t                u64GuestLinearAddr;
    /** The guest-physical address field. */
    uint64_t                u64GuestPhysAddr;
    /** The effective guest-linear address if @a InstrInfo indicates a memory-based
     *  instruction VM-exit. */
    RTGCPTR                 GCPtrEffAddr;
} VMXVEXITINFO;
/** Pointer to the VMXVEXITINFO struct. */
typedef VMXVEXITINFO *PVMXVEXITINFO;
/** Pointer to a const VMXVEXITINFO struct. */
typedef const VMXVEXITINFO *PCVMXVEXITINFO;
AssertCompileMemberAlignment(VMXVEXITINFO, u64Qual, 8);

/**
 * Virtual VMCS.
 * This is our custom format and merged into the actual VMCS (/shadow) when we
 * execute nested-guest code using hardware-assisted VMX.
 *
 * The first 8 bytes are as per Intel spec. 24.2 "Format of the VMCS Region".
 *
 * The offset and size of the VMCS state field (fVmcsState) is also fixed (not by
 * Intel but for our own requirements) as we use it to offset into guest memory.
 *
 * Although the guest is supposed to access the VMCS only through the execution of
 * VMX instructions (VMREAD, VMWRITE etc.), since the VMCS may reside in guest
 * memory (e.g, active but not current VMCS), for saved-states compatibility, and
 * for teleportation purposes, any newly added fields should be added to the
 * appropriate reserved sections or at the end of the structure.
 *
 * We always treat natural-width fields as 64-bit in our implementation since
 * it's easier, allows for teleporation in the future and does not affect guest
 * software.
 */
#pragma pack(1)
typedef struct
{
    /** 0x0 - VMX VMCS revision identifier.  */
    VMXVMCSREVID    u32VmcsRevId;
    /** 0x4 - VMX-abort indicator. */
    uint32_t        u32VmxAbortId;
    /** 0x8 - VMCS state, see VMX_V_VMCS_STATE_XXX. */
    uint8_t         fVmcsState;
    /** 0x9 - Reserved for future. */
    uint8_t         au8Padding0[3];
    /** 0xc - Reserved for future. */
    uint32_t        au32Reserved0[7];

    /** @name 16-bit control fields.
     * @{ */
    /** 0x28 - Virtual processor ID. */
    uint16_t        u16Vpid;
    /** 0x2a - Posted interrupt notify vector. */
    uint16_t        u16PostIntNotifyVector;
    /** 0x2c - EPTP index. */
    uint16_t        u16EptpIndex;
    /** 0x2e - Reserved for future. */
    uint16_t        au16Reserved0[8];
    /** @} */

    /** @name 16-bit Guest-state fields.
     * Order of [ES..GS] is important, must match X86_SREG_XXX.
     * @{ */
    /** 0x3e - Guest ES selector. */
    RTSEL           GuestEs;
    /** 0x40 - Guest ES selector. */
    RTSEL           GuestCs;
    /** 0x42 - Guest ES selector. */
    RTSEL           GuestSs;
    /** 0x44 - Guest ES selector. */
    RTSEL           GuestDs;
    /** 0x46 - Guest ES selector. */
    RTSEL           GuestFs;
    /** 0x48 - Guest ES selector. */
    RTSEL           GuestGs;
    /** 0x4a - Guest LDTR selector. */
    RTSEL           GuestLdtr;
    /** 0x4c - Guest TR selector. */
    RTSEL           GuestTr;
    /** 0x4e - Guest interrupt status (virtual-interrupt delivery). */
    uint16_t        u16GuestIntStatus;
    /** 0x50 - PML index. */
    uint16_t        u16PmlIndex;
    /** 0x52 - Reserved for future. */
    uint16_t        au16Reserved1[8];
    /** @} */

    /** @name 16-bit Host-state fields.
     * @{ */
    /** 0x62 - Host ES selector. */
    RTSEL           HostEs;
    /** 0x64 - Host CS selector. */
    RTSEL           HostCs;
    /** 0x66 - Host SS selector. */
    RTSEL           HostSs;
    /** 0x68 - Host DS selector. */
    RTSEL           HostDs;
    /** 0x6a - Host FS selector. */
    RTSEL           HostFs;
    /** 0x6c - Host GS selector. */
    RTSEL           HostGs;
    /** 0x6e - Host TR selector. */
    RTSEL           HostTr;
    /** 0x70 - Reserved for future. */
    uint16_t        au16Reserved2[10];
    /** @} */

    /** @name 32-bit Control fields.
     * @{ */
    /** 0x84 - Pin-based VM-execution controls. */
    uint32_t        u32PinCtls;
    /** 0x88 - Processor-based VM-execution controls. */
    uint32_t        u32ProcCtls;
    /** 0x8c - Exception bitmap. */
    uint32_t        u32XcptBitmap;
    /** 0x90 - Page-fault exception error mask. */
    uint32_t        u32XcptPFMask;
    /** 0x94 - Page-fault exception error match. */
    uint32_t        u32XcptPFMatch;
    /** 0x98 - CR3-target count. */
    uint32_t        u32Cr3TargetCount;
    /** 0x9c - VM-exit controls. */
    uint32_t        u32ExitCtls;
    /** 0xa0 - VM-exit MSR store count. */
    uint32_t        u32ExitMsrStoreCount;
    /** 0xa4 - VM-exit MSR load count. */
    uint32_t        u32ExitMsrLoadCount;
    /** 0xa8 - VM-entry controls. */
    uint32_t        u32EntryCtls;
    /** 0xac - VM-entry MSR load count. */
    uint32_t        u32EntryMsrLoadCount;
    /** 0xb0 - VM-entry interruption information. */
    uint32_t        u32EntryIntInfo;
    /** 0xb4 - VM-entry exception error code. */
    uint32_t        u32EntryXcptErrCode;
    /** 0xb8 - VM-entry instruction length. */
    uint32_t        u32EntryInstrLen;
    /** 0xbc - TPR-threshold. */
    uint32_t        u32TprThreshold;
    /** 0xc0 - Secondary-processor based VM-execution controls. */
    uint32_t        u32ProcCtls2;
    /** 0xc4 - Pause-loop exiting Gap. */
    uint32_t        u32PleGap;
    /** 0xc8 - Pause-loop exiting Window. */
    uint32_t        u32PleWindow;
    /** 0xcc - Reserved for future. */
    uint32_t        au32Reserved1[8];
    /** @} */

    /** @name 32-bit Read-only Data fields.
     * @{ */
    /** 0xec - VM-instruction error.  */
    uint32_t        u32RoVmInstrError;
    /** 0xf0 - VM-exit reason. */
    uint32_t        u32RoExitReason;
    /** 0xf4 - VM-exit interruption information. */
    uint32_t        u32RoExitIntInfo;
    /** 0xf8 - VM-exit interruption error code. */
    uint32_t        u32RoExitErrCode;
    /** 0xfc - IDT-vectoring information. */
    uint32_t        u32RoIdtVectoringInfo;
    /** 0x100 - IDT-vectoring error code. */
    uint32_t        u32RoIdtVectoringErrCode;
    /** 0x104 - VM-exit instruction length. */
    uint32_t        u32RoExitInstrLen;
    /** 0x108 - VM-exit instruction information. */
    uint32_t        u32RoExitInstrInfo;
    /** 0x10c - Reserved for future. */
    uint32_t        au32RoReserved2[8];
    /** @} */

    /** @name 32-bit Guest-state fields.
     * Order of [ES..GS] limit and attributes are important, must match X86_SREG_XXX.
     * @{ */
    /** 0x12c - Guest ES limit. */
    uint32_t        u32GuestEsLimit;
    /** 0x130 - Guest CS limit. */
    uint32_t        u32GuestCsLimit;
    /** 0x134 - Guest SS limit. */
    uint32_t        u32GuestSsLimit;
    /** 0x138 - Guest DS limit. */
    uint32_t        u32GuestDsLimit;
    /** 0x13c - Guest FS limit. */
    uint32_t        u32GuestFsLimit;
    /** 0x140 - Guest GS limit. */
    uint32_t        u32GuestGsLimit;
    /** 0x144 - Guest LDTR limit. */
    uint32_t        u32GuestLdtrLimit;
    /** 0x148 - Guest TR limit. */
    uint32_t        u32GuestTrLimit;
    /** 0x14c - Guest GDTR limit. */
    uint32_t        u32GuestGdtrLimit;
    /** 0x150 - Guest IDTR limit. */
    uint32_t        u32GuestIdtrLimit;
    /** 0x154 - Guest ES attributes. */
    uint32_t        u32GuestEsAttr;
    /** 0x158 - Guest CS attributes. */
    uint32_t        u32GuestCsAttr;
    /** 0x15c - Guest SS attributes. */
    uint32_t        u32GuestSsAttr;
    /** 0x160 - Guest DS attributes. */
    uint32_t        u32GuestDsAttr;
    /** 0x164 - Guest FS attributes. */
    uint32_t        u32GuestFsAttr;
    /** 0x168 - Guest GS attributes. */
    uint32_t        u32GuestGsAttr;
    /** 0x16c - Guest LDTR attributes. */
    uint32_t        u32GuestLdtrAttr;
    /** 0x170 - Guest TR attributes. */
    uint32_t        u32GuestTrAttr;
    /** 0x174 - Guest interruptibility state. */
    uint32_t        u32GuestIntrState;
    /** 0x178 - Guest activity state. */
    uint32_t        u32GuestActivityState;
    /** 0x17c - Guest SMBASE. */
    uint32_t        u32GuestSmBase;
    /** 0x180 - Guest SYSENTER CS. */
    uint32_t        u32GuestSysenterCS;
    /** 0x184 - Preemption timer value. */
    uint32_t        u32PreemptTimer;
    /** 0x188 - Reserved for future. */
    uint32_t        au32Reserved3[8];
    /** @} */

    /** @name 32-bit Host-state fields.
     * @{ */
    /** 0x1a8 - Host SYSENTER CS. */
    uint32_t        u32HostSysenterCs;
    /** 0x1ac - Reserved for future. */
    uint32_t        au32Reserved4[11];
    /** @} */

    /** @name 64-bit Control fields.
     * @{ */
    /** 0x1d8 - I/O bitmap A address. */
    RTUINT64U       u64AddrIoBitmapA;
    /** 0x1e0 - I/O bitmap B address. */
    RTUINT64U       u64AddrIoBitmapB;
    /** 0x1e8 - MSR bitmap address. */
    RTUINT64U       u64AddrMsrBitmap;
    /** 0x1f0 - VM-exit MSR-store area address. */
    RTUINT64U       u64AddrExitMsrStore;
    /** 0x1f8 - VM-exit MSR-load area address. */
    RTUINT64U       u64AddrExitMsrLoad;
    /** 0x200 - VM-entry MSR-load area address. */
    RTUINT64U       u64AddrEntryMsrLoad;
    /** 0x208 - Executive-VMCS pointer. */
    RTUINT64U       u64ExecVmcsPtr;
    /** 0x210 - PML address. */
    RTUINT64U       u64AddrPml;
    /** 0x218 - TSC offset. */
    RTUINT64U       u64TscOffset;
    /** 0x220 - Virtual-APIC address. */
    RTUINT64U       u64AddrVirtApic;
    /** 0x228 - APIC-access address. */
    RTUINT64U       u64AddrApicAccess;
    /** 0x230 - Posted-interrupt descriptor address.  */
    RTUINT64U       u64AddrPostedIntDesc;
    /** 0x238 - VM-functions control.  */
    RTUINT64U       u64VmFuncCtls;
    /** 0x240 - EPTP pointer.  */
    RTUINT64U       u64EptpPtr;
    /** 0x248 - EOI-exit bitmap 0.  */
    RTUINT64U       u64EoiExitBitmap0;
    /** 0x250 - EOI-exit bitmap 1.  */
    RTUINT64U       u64EoiExitBitmap1;
    /** 0x258 - EOI-exit bitmap 2.  */
    RTUINT64U       u64EoiExitBitmap2;
    /** 0x260 - EOI-exit bitmap 3.  */
    RTUINT64U       u64EoiExitBitmap3;
    /** 0x268 - EPTP-list address.  */
    RTUINT64U       u64AddrEptpList;
    /** 0x270 - VMREAD-bitmap address.  */
    RTUINT64U       u64AddrVmreadBitmap;
    /** 0x278 - VMWRITE-bitmap address.  */
    RTUINT64U       u64AddrVmwriteBitmap;
    /** 0x280 - Virtualization-exception information address.  */
    RTUINT64U       u64AddrXcptVeInfo;
    /** 0x288 - XSS-exiting bitmap address.  */
    RTUINT64U       u64AddrXssBitmap;
    /** 0x290 - ENCLS-exiting bitmap address.  */
    RTUINT64U       u64AddrEnclsBitmap;
    /** 0x298 - TSC multiplier.  */
    RTUINT64U       u64TscMultiplier;
    /** 0x2a0 - Reserved for future. */
    RTUINT64U       au64Reserved0[16];
    /** @} */

    /** @name 64-bit Read-only Data fields.
     * @{ */
    /** 0x320 - Guest-physical address. */
    RTUINT64U       u64RoGuestPhysAddr;
    /** 0x328 - Reserved for future. */
    RTUINT64U       au64Reserved1[8];
    /** @} */

    /** @name 64-bit Guest-state fields.
     * @{ */
    /** 0x368 - VMCS link pointer. */
    RTUINT64U       u64VmcsLinkPtr;
    /** 0x370 - Guest debug-control MSR. */
    RTUINT64U       u64GuestDebugCtlMsr;
    /** 0x378 - Guest PAT MSR. */
    RTUINT64U       u64GuestPatMsr;
    /** 0x380 - Guest EFER MSR. */
    RTUINT64U       u64GuestEferMsr;
    /** 0x388 - Guest global performance-control MSR. */
    RTUINT64U       u64GuestPerfGlobalCtlMsr;
    /** 0x390 - Guest PDPTE 0. */
    RTUINT64U       u64GuestPdpte0;
    /** 0x398 - Guest PDPTE 0. */
    RTUINT64U       u64GuestPdpte1;
    /** 0x3a0 - Guest PDPTE 1. */
    RTUINT64U       u64GuestPdpte2;
    /** 0x3a8 - Guest PDPTE 2. */
    RTUINT64U       u64GuestPdpte3;
    /** 0x3b0 - Guest Bounds-config MSR (Intel MPX - Memory Protection Extensions). */
    RTUINT64U       u64GuestBndcfgsMsr;
    /** 0x3b8 - Reserved for future. */
    RTUINT64U       au64Reserved2[16];
    /** @} */

    /** @name 64-bit Host-state Fields.
     * @{ */
    /** 0x438 - Host PAT MSR. */
    RTUINT64U       u64HostPatMsr;
    /** 0x440 - Host EFER MSR. */
    RTUINT64U       u64HostEferMsr;
    /** 0x448 - Host global performance-control MSR. */
    RTUINT64U       u64HostPerfGlobalCtlMsr;
    /** 0x450 - Reserved for future. */
    RTUINT64U       au64Reserved3[16];
    /** @} */

    /** @name Natural-width Control fields.
     * @{ */
    /** 0x4d0 - CR0 guest/host Mask. */
    RTUINT64U       u64Cr0Mask;
    /** 0x4d8 - CR4 guest/host Mask. */
    RTUINT64U       u64Cr4Mask;
    /** 0x4e0 - CR0 read shadow. */
    RTUINT64U       u64Cr0ReadShadow;
    /** 0x4e8 - CR4 read shadow. */
    RTUINT64U       u64Cr4ReadShadow;
    /** 0x4f0 - CR3-target value 0. */
    RTUINT64U       u64Cr3Target0;
    /** 0x4f8 - CR3-target value 1. */
    RTUINT64U       u64Cr3Target1;
    /** 0x500 - CR3-target value 2. */
    RTUINT64U       u64Cr3Target2;
    /** 0x508 - CR3-target value 3. */
    RTUINT64U       u64Cr3Target3;
    /** 0x510 - Reserved for future. */
    RTUINT64U       au64Reserved4[32];
    /** @} */

    /** @name Natural-width Read-only Data fields. */
    /** 0x610 - Exit qualification. */
    RTUINT64U       u64RoExitQual;
    /** 0x618 - I/O RCX. */
    RTUINT64U       u64RoIoRcx;
    /** 0x620 - I/O RSI. */
    RTUINT64U       u64RoIoRsi;
    /** 0x628 - I/O RDI. */
    RTUINT64U       u64RoIoRdi;
    /** 0x630 - I/O RIP. */
    RTUINT64U       u64RoIoRip;
    /** 0x638 - Guest-linear address. */
    RTUINT64U       u64RoGuestLinearAddr;
    /** 0x640 - Reserved for future. */
    RTUINT64U       au64Reserved5[16];
    /** @} */

    /** @name Natural-width Guest-state Fields.
     * Order of [ES..GS] base is important, must match X86_SREG_XXX.
     * @{ */
    /** 0x6c0 - Guest CR0. */
    RTUINT64U       u64GuestCr0;
    /** 0x6c8 - Guest CR3. */
    RTUINT64U       u64GuestCr3;
    /** 0x6d0 - Guest CR4. */
    RTUINT64U       u64GuestCr4;
    /** 0x6d8 - Guest ES base. */
    RTUINT64U       u64GuestEsBase;
    /** 0x6e0 - Guest CS base. */
    RTUINT64U       u64GuestCsBase;
    /** 0x6e8 - Guest SS base. */
    RTUINT64U       u64GuestSsBase;
    /** 0x6f0 - Guest DS base. */
    RTUINT64U       u64GuestDsBase;
    /** 0x6f8 - Guest FS base. */
    RTUINT64U       u64GuestFsBase;
    /** 0x700 - Guest GS base. */
    RTUINT64U       u64GuestGsBase;
    /** 0x708 - Guest LDTR base. */
    RTUINT64U       u64GuestLdtrBase;
    /** 0x710 - Guest TR base. */
    RTUINT64U       u64GuestTrBase;
    /** 0x718 - Guest GDTR base.  */
    RTUINT64U       u64GuestGdtrBase;
    /** 0x720 - Guest IDTR base.  */
    RTUINT64U       u64GuestIdtrBase;
    /** 0x728 - Guest DR7.  */
    RTUINT64U       u64GuestDr7;
    /** 0x730 - Guest RSP.  */
    RTUINT64U       u64GuestRsp;
    /** 0x738 - Guest RIP.  */
    RTUINT64U       u64GuestRip;
    /** 0x740 - Guest RFLAGS.  */
    RTUINT64U       u64GuestRFlags;
    /** 0x748 - Guest pending debug exception.  */
    RTUINT64U       u64GuestPendingDbgXcpt;
    /** 0x750 - Guest SYSENTER ESP.  */
    RTUINT64U       u64GuestSysenterEsp;
    /** 0x758 - Guest SYSENTER EIP.  */
    RTUINT64U       u64GuestSysenterEip;
    /** 0x760 - Reserved for future. */
    RTUINT64U       au64Reserved6[32];
    /** @} */

    /** @name Natural-width Host-state fields.
     * @{ */
    /** 0x860 - Host CR0. */
    RTUINT64U       u64HostCr0;
    /** 0x868 - Host CR3. */
    RTUINT64U       u64HostCr3;
    /** 0x870 - Host CR4. */
    RTUINT64U       u64HostCr4;
    /** 0x878 - Host FS base. */
    RTUINT64U       u64HostFsBase;
    /** 0x880 - Host GS base. */
    RTUINT64U       u64HostGsBase;
    /** 0x888 - Host TR base. */
    RTUINT64U       u64HostTrBase;
    /** 0x890 - Host GDTR base. */
    RTUINT64U       u64HostGdtrBase;
    /** 0x898 - Host IDTR base. */
    RTUINT64U       u64HostIdtrBase;
    /** 0x8a0 - Host SYSENTER ESP base. */
    RTUINT64U       u64HostSysenterEsp;
    /** 0x8a8 - Host SYSENTER ESP base. */
    RTUINT64U       u64HostSysenterEip;
    /** 0x8b0 - Host RSP. */
    RTUINT64U       u64HostRsp;
    /** 0x8b8 - Host RIP. */
    RTUINT64U       u64HostRip;
    /** 0x8c0 - Reserved for future. */
    RTUINT64U       au64Reserved7[32];
    /** @} */

    /** 0x9c0 - Padding. */
    uint8_t         abPadding[X86_PAGE_4K_SIZE - 0x9c0];
} VMXVVMCS;
#pragma pack()
/** Pointer to the VMXVVMCS struct. */
typedef VMXVVMCS *PVMXVVMCS;
/** Pointer to a const VMXVVMCS struct. */
typedef const VMXVVMCS *PCVMXVVMCS;
AssertCompileSize(VMXVVMCS, X86_PAGE_4K_SIZE);
AssertCompileMemberSize(VMXVVMCS, fVmcsState, sizeof(uint8_t));
AssertCompileMemberOffset(VMXVVMCS, u32VmxAbortId,      0x004);
AssertCompileMemberOffset(VMXVVMCS, fVmcsState,         0x008);
AssertCompileMemberOffset(VMXVVMCS, u16Vpid,            0x028);
AssertCompileMemberOffset(VMXVVMCS, GuestEs,            0x03e);
AssertCompileMemberOffset(VMXVVMCS, HostEs,             0x062);
AssertCompileMemberOffset(VMXVVMCS, u32PinCtls,         0x084);
AssertCompileMemberOffset(VMXVVMCS, u32RoVmInstrError,  0x0ec);
AssertCompileMemberOffset(VMXVVMCS, u32GuestEsLimit,    0x12c);
AssertCompileMemberOffset(VMXVVMCS, u32HostSysenterCs,  0x1a8);
AssertCompileMemberOffset(VMXVVMCS, u64AddrIoBitmapA,   0x1d8);
AssertCompileMemberOffset(VMXVVMCS, u64RoGuestPhysAddr, 0x320);
AssertCompileMemberOffset(VMXVVMCS, u64VmcsLinkPtr,     0x368);
AssertCompileMemberOffset(VMXVVMCS, u64HostPatMsr,      0x438);
AssertCompileMemberOffset(VMXVVMCS, u64Cr0Mask,         0x4d0);
AssertCompileMemberOffset(VMXVVMCS, u64RoExitQual,      0x610);
AssertCompileMemberOffset(VMXVVMCS, u64GuestCr0,        0x6c0);
AssertCompileMemberOffset(VMXVVMCS, u64HostCr0,         0x860);
/** @} */

/**
 * Virtual VMX-instruction and VM-exit diagnostics.
 *
 * These are not the same as VM instruction errors that are enumerated in the Intel
 * spec. These are purely internal, fine-grained definitions used for diagnostic
 * purposes and are not reported to guest software under the VM-instruction error
 * field in its VMCS.
 *
 * @note Members of this enum are used as array indices, so no gaps are allowed.
 *       Please update g_apszVmxInstrDiagDesc when you add new fields to this
 *       enum.
 */
typedef enum
{
    /* Internal processing errors. */
    kVmxVDiag_None = 0,
    kVmxVDiag_Ipe_1,
    kVmxVDiag_Ipe_2,
    kVmxVDiag_Ipe_3,
    kVmxVDiag_Ipe_4,
    kVmxVDiag_Ipe_5,
    kVmxVDiag_Ipe_6,
    kVmxVDiag_Ipe_7,
    kVmxVDiag_Ipe_8,
    kVmxVDiag_Ipe_9,
    kVmxVDiag_Ipe_10,
    kVmxVDiag_Ipe_11,
    kVmxVDiag_Ipe_12,
    kVmxVDiag_Ipe_13,
    kVmxVDiag_Ipe_14,
    kVmxVDiag_Ipe_15,
    kVmxVDiag_Ipe_16,
    /* VMXON. */
    kVmxVDiag_Vmxon_A20M,
    kVmxVDiag_Vmxon_Cpl,
    kVmxVDiag_Vmxon_Cr0Fixed0,
    kVmxVDiag_Vmxon_Cr0Fixed1,
    kVmxVDiag_Vmxon_Cr4Fixed0,
    kVmxVDiag_Vmxon_Cr4Fixed1,
    kVmxVDiag_Vmxon_Intercept,
    kVmxVDiag_Vmxon_LongModeCS,
    kVmxVDiag_Vmxon_MsrFeatCtl,
    kVmxVDiag_Vmxon_PtrAbnormal,
    kVmxVDiag_Vmxon_PtrAlign,
    kVmxVDiag_Vmxon_PtrMap,
    kVmxVDiag_Vmxon_PtrReadPhys,
    kVmxVDiag_Vmxon_PtrWidth,
    kVmxVDiag_Vmxon_RealOrV86Mode,
    kVmxVDiag_Vmxon_ShadowVmcs,
    kVmxVDiag_Vmxon_VmxAlreadyRoot,
    kVmxVDiag_Vmxon_Vmxe,
    kVmxVDiag_Vmxon_VmcsRevId,
    kVmxVDiag_Vmxon_VmxRootCpl,
    /* VMXOFF. */
    kVmxVDiag_Vmxoff_Cpl,
    kVmxVDiag_Vmxoff_Intercept,
    kVmxVDiag_Vmxoff_LongModeCS,
    kVmxVDiag_Vmxoff_RealOrV86Mode,
    kVmxVDiag_Vmxoff_Vmxe,
    kVmxVDiag_Vmxoff_VmxRoot,
    /* VMPTRLD. */
    kVmxVDiag_Vmptrld_Cpl,
    kVmxVDiag_Vmptrld_LongModeCS,
    kVmxVDiag_Vmptrld_PtrAbnormal,
    kVmxVDiag_Vmptrld_PtrAlign,
    kVmxVDiag_Vmptrld_PtrMap,
    kVmxVDiag_Vmptrld_PtrReadPhys,
    kVmxVDiag_Vmptrld_PtrVmxon,
    kVmxVDiag_Vmptrld_PtrWidth,
    kVmxVDiag_Vmptrld_RealOrV86Mode,
    kVmxVDiag_Vmptrld_ShadowVmcs,
    kVmxVDiag_Vmptrld_VmcsRevId,
    kVmxVDiag_Vmptrld_VmxRoot,
    /* VMPTRST. */
    kVmxVDiag_Vmptrst_Cpl,
    kVmxVDiag_Vmptrst_LongModeCS,
    kVmxVDiag_Vmptrst_PtrMap,
    kVmxVDiag_Vmptrst_RealOrV86Mode,
    kVmxVDiag_Vmptrst_VmxRoot,
    /* VMCLEAR. */
    kVmxVDiag_Vmclear_Cpl,
    kVmxVDiag_Vmclear_LongModeCS,
    kVmxVDiag_Vmclear_PtrAbnormal,
    kVmxVDiag_Vmclear_PtrAlign,
    kVmxVDiag_Vmclear_PtrMap,
    kVmxVDiag_Vmclear_PtrReadPhys,
    kVmxVDiag_Vmclear_PtrVmxon,
    kVmxVDiag_Vmclear_PtrWidth,
    kVmxVDiag_Vmclear_RealOrV86Mode,
    kVmxVDiag_Vmclear_VmxRoot,
    /* VMWRITE. */
    kVmxVDiag_Vmwrite_Cpl,
    kVmxVDiag_Vmwrite_FieldInvalid,
    kVmxVDiag_Vmwrite_FieldRo,
    kVmxVDiag_Vmwrite_LinkPtrInvalid,
    kVmxVDiag_Vmwrite_LongModeCS,
    kVmxVDiag_Vmwrite_PtrInvalid,
    kVmxVDiag_Vmwrite_PtrMap,
    kVmxVDiag_Vmwrite_RealOrV86Mode,
    kVmxVDiag_Vmwrite_VmxRoot,
    /* VMREAD. */
    kVmxVDiag_Vmread_Cpl,
    kVmxVDiag_Vmread_FieldInvalid,
    kVmxVDiag_Vmread_LinkPtrInvalid,
    kVmxVDiag_Vmread_LongModeCS,
    kVmxVDiag_Vmread_PtrInvalid,
    kVmxVDiag_Vmread_PtrMap,
    kVmxVDiag_Vmread_RealOrV86Mode,
    kVmxVDiag_Vmread_VmxRoot,
    /* VMLAUNCH/VMRESUME. */
    kVmxVDiag_Vmentry_AddrApicAccess,
    kVmxVDiag_Vmentry_AddrEntryMsrLoad,
    kVmxVDiag_Vmentry_AddrExitMsrLoad,
    kVmxVDiag_Vmentry_AddrExitMsrStore,
    kVmxVDiag_Vmentry_AddrIoBitmapA,
    kVmxVDiag_Vmentry_AddrIoBitmapB,
    kVmxVDiag_Vmentry_AddrMsrBitmap,
    kVmxVDiag_Vmentry_AddrVirtApicPage,
    kVmxVDiag_Vmentry_AddrVmcsLinkPtr,
    kVmxVDiag_Vmentry_AddrVmreadBitmap,
    kVmxVDiag_Vmentry_AddrVmwriteBitmap,
    kVmxVDiag_Vmentry_ApicRegVirt,
    kVmxVDiag_Vmentry_BlocKMovSS,
    kVmxVDiag_Vmentry_Cpl,
    kVmxVDiag_Vmentry_Cr3TargetCount,
    kVmxVDiag_Vmentry_EntryCtlsAllowed1,
    kVmxVDiag_Vmentry_EntryCtlsDisallowed0,
    kVmxVDiag_Vmentry_EntryInstrLen,
    kVmxVDiag_Vmentry_EntryInstrLenZero,
    kVmxVDiag_Vmentry_EntryIntInfoErrCodePe,
    kVmxVDiag_Vmentry_EntryIntInfoErrCodeVec,
    kVmxVDiag_Vmentry_EntryIntInfoTypeVecRsvd,
    kVmxVDiag_Vmentry_EntryXcptErrCodeRsvd,
    kVmxVDiag_Vmentry_ExitCtlsAllowed1,
    kVmxVDiag_Vmentry_ExitCtlsDisallowed0,
    kVmxVDiag_Vmentry_GuestActStateHlt,
    kVmxVDiag_Vmentry_GuestActStateRsvd,
    kVmxVDiag_Vmentry_GuestActStateShutdown,
    kVmxVDiag_Vmentry_GuestActStateSsDpl,
    kVmxVDiag_Vmentry_GuestActStateStiMovSs,
    kVmxVDiag_Vmentry_GuestCr0Fixed0,
    kVmxVDiag_Vmentry_GuestCr0Fixed1,
    kVmxVDiag_Vmentry_GuestCr0PgPe,
    kVmxVDiag_Vmentry_GuestCr3,
    kVmxVDiag_Vmentry_GuestCr4Fixed0,
    kVmxVDiag_Vmentry_GuestCr4Fixed1,
    kVmxVDiag_Vmentry_GuestDebugCtl,
    kVmxVDiag_Vmentry_GuestDr7,
    kVmxVDiag_Vmentry_GuestEferMsr,
    kVmxVDiag_Vmentry_GuestEferMsrRsvd,
    kVmxVDiag_Vmentry_GuestGdtrBase,
    kVmxVDiag_Vmentry_GuestGdtrLimit,
    kVmxVDiag_Vmentry_GuestIdtrBase,
    kVmxVDiag_Vmentry_GuestIdtrLimit,
    kVmxVDiag_Vmentry_GuestIntStateEnclave,
    kVmxVDiag_Vmentry_GuestIntStateExtInt,
    kVmxVDiag_Vmentry_GuestIntStateNmi,
    kVmxVDiag_Vmentry_GuestIntStateRFlagsSti,
    kVmxVDiag_Vmentry_GuestIntStateRsvd,
    kVmxVDiag_Vmentry_GuestIntStateSmi,
    kVmxVDiag_Vmentry_GuestIntStateStiMovSs,
    kVmxVDiag_Vmentry_GuestIntStateVirtNmi,
    kVmxVDiag_Vmentry_GuestPae,
    kVmxVDiag_Vmentry_GuestPatMsr,
    kVmxVDiag_Vmentry_GuestPcide,
    kVmxVDiag_Vmentry_GuestPdpteCr3ReadPhys,
    kVmxVDiag_Vmentry_GuestPdpte0Rsvd,
    kVmxVDiag_Vmentry_GuestPdpte1Rsvd,
    kVmxVDiag_Vmentry_GuestPdpte2Rsvd,
    kVmxVDiag_Vmentry_GuestPdpte3Rsvd,
    kVmxVDiag_Vmentry_GuestPndDbgXcptBsNoTf,
    kVmxVDiag_Vmentry_GuestPndDbgXcptBsTf,
    kVmxVDiag_Vmentry_GuestPndDbgXcptRsvd,
    kVmxVDiag_Vmentry_GuestPndDbgXcptRtm,
    kVmxVDiag_Vmentry_GuestRip,
    kVmxVDiag_Vmentry_GuestRipRsvd,
    kVmxVDiag_Vmentry_GuestRFlagsIf,
    kVmxVDiag_Vmentry_GuestRFlagsRsvd,
    kVmxVDiag_Vmentry_GuestRFlagsVm,
    kVmxVDiag_Vmentry_GuestSegAttrCsDefBig,
    kVmxVDiag_Vmentry_GuestSegAttrCsDplEqSs,
    kVmxVDiag_Vmentry_GuestSegAttrCsDplLtSs,
    kVmxVDiag_Vmentry_GuestSegAttrCsDplZero,
    kVmxVDiag_Vmentry_GuestSegAttrCsType,
    kVmxVDiag_Vmentry_GuestSegAttrCsTypeRead,
    kVmxVDiag_Vmentry_GuestSegAttrDescTypeCs,
    kVmxVDiag_Vmentry_GuestSegAttrDescTypeDs,
    kVmxVDiag_Vmentry_GuestSegAttrDescTypeEs,
    kVmxVDiag_Vmentry_GuestSegAttrDescTypeFs,
    kVmxVDiag_Vmentry_GuestSegAttrDescTypeGs,
    kVmxVDiag_Vmentry_GuestSegAttrDescTypeSs,
    kVmxVDiag_Vmentry_GuestSegAttrDplRplCs,
    kVmxVDiag_Vmentry_GuestSegAttrDplRplDs,
    kVmxVDiag_Vmentry_GuestSegAttrDplRplEs,
    kVmxVDiag_Vmentry_GuestSegAttrDplRplFs,
    kVmxVDiag_Vmentry_GuestSegAttrDplRplGs,
    kVmxVDiag_Vmentry_GuestSegAttrDplRplSs,
    kVmxVDiag_Vmentry_GuestSegAttrGranCs,
    kVmxVDiag_Vmentry_GuestSegAttrGranDs,
    kVmxVDiag_Vmentry_GuestSegAttrGranEs,
    kVmxVDiag_Vmentry_GuestSegAttrGranFs,
    kVmxVDiag_Vmentry_GuestSegAttrGranGs,
    kVmxVDiag_Vmentry_GuestSegAttrGranSs,
    kVmxVDiag_Vmentry_GuestSegAttrLdtrDescType,
    kVmxVDiag_Vmentry_GuestSegAttrLdtrGran,
    kVmxVDiag_Vmentry_GuestSegAttrLdtrPresent,
    kVmxVDiag_Vmentry_GuestSegAttrLdtrRsvd,
    kVmxVDiag_Vmentry_GuestSegAttrLdtrType,
    kVmxVDiag_Vmentry_GuestSegAttrPresentCs,
    kVmxVDiag_Vmentry_GuestSegAttrPresentDs,
    kVmxVDiag_Vmentry_GuestSegAttrPresentEs,
    kVmxVDiag_Vmentry_GuestSegAttrPresentFs,
    kVmxVDiag_Vmentry_GuestSegAttrPresentGs,
    kVmxVDiag_Vmentry_GuestSegAttrPresentSs,
    kVmxVDiag_Vmentry_GuestSegAttrRsvdCs,
    kVmxVDiag_Vmentry_GuestSegAttrRsvdDs,
    kVmxVDiag_Vmentry_GuestSegAttrRsvdEs,
    kVmxVDiag_Vmentry_GuestSegAttrRsvdFs,
    kVmxVDiag_Vmentry_GuestSegAttrRsvdGs,
    kVmxVDiag_Vmentry_GuestSegAttrRsvdSs,
    kVmxVDiag_Vmentry_GuestSegAttrSsDplEqRpl,
    kVmxVDiag_Vmentry_GuestSegAttrSsDplZero,
    kVmxVDiag_Vmentry_GuestSegAttrSsType,
    kVmxVDiag_Vmentry_GuestSegAttrTrDescType,
    kVmxVDiag_Vmentry_GuestSegAttrTrGran,
    kVmxVDiag_Vmentry_GuestSegAttrTrPresent,
    kVmxVDiag_Vmentry_GuestSegAttrTrRsvd,
    kVmxVDiag_Vmentry_GuestSegAttrTrType,
    kVmxVDiag_Vmentry_GuestSegAttrTrUnusable,
    kVmxVDiag_Vmentry_GuestSegAttrTypeAccCs,
    kVmxVDiag_Vmentry_GuestSegAttrTypeAccDs,
    kVmxVDiag_Vmentry_GuestSegAttrTypeAccEs,
    kVmxVDiag_Vmentry_GuestSegAttrTypeAccFs,
    kVmxVDiag_Vmentry_GuestSegAttrTypeAccGs,
    kVmxVDiag_Vmentry_GuestSegAttrTypeAccSs,
    kVmxVDiag_Vmentry_GuestSegAttrV86Cs,
    kVmxVDiag_Vmentry_GuestSegAttrV86Ds,
    kVmxVDiag_Vmentry_GuestSegAttrV86Es,
    kVmxVDiag_Vmentry_GuestSegAttrV86Fs,
    kVmxVDiag_Vmentry_GuestSegAttrV86Gs,
    kVmxVDiag_Vmentry_GuestSegAttrV86Ss,
    kVmxVDiag_Vmentry_GuestSegBaseCs,
    kVmxVDiag_Vmentry_GuestSegBaseDs,
    kVmxVDiag_Vmentry_GuestSegBaseEs,
    kVmxVDiag_Vmentry_GuestSegBaseFs,
    kVmxVDiag_Vmentry_GuestSegBaseGs,
    kVmxVDiag_Vmentry_GuestSegBaseLdtr,
    kVmxVDiag_Vmentry_GuestSegBaseSs,
    kVmxVDiag_Vmentry_GuestSegBaseTr,
    kVmxVDiag_Vmentry_GuestSegBaseV86Cs,
    kVmxVDiag_Vmentry_GuestSegBaseV86Ds,
    kVmxVDiag_Vmentry_GuestSegBaseV86Es,
    kVmxVDiag_Vmentry_GuestSegBaseV86Fs,
    kVmxVDiag_Vmentry_GuestSegBaseV86Gs,
    kVmxVDiag_Vmentry_GuestSegBaseV86Ss,
    kVmxVDiag_Vmentry_GuestSegLimitV86Cs,
    kVmxVDiag_Vmentry_GuestSegLimitV86Ds,
    kVmxVDiag_Vmentry_GuestSegLimitV86Es,
    kVmxVDiag_Vmentry_GuestSegLimitV86Fs,
    kVmxVDiag_Vmentry_GuestSegLimitV86Gs,
    kVmxVDiag_Vmentry_GuestSegLimitV86Ss,
    kVmxVDiag_Vmentry_GuestSegSelCsSsRpl,
    kVmxVDiag_Vmentry_GuestSegSelLdtr,
    kVmxVDiag_Vmentry_GuestSegSelTr,
    kVmxVDiag_Vmentry_GuestSysenterEspEip,
    kVmxVDiag_Vmentry_VmcsLinkPtrCurVmcs,
    kVmxVDiag_Vmentry_VmcsLinkPtrReadPhys,
    kVmxVDiag_Vmentry_VmcsLinkPtrRevId,
    kVmxVDiag_Vmentry_VmcsLinkPtrShadow,
    kVmxVDiag_Vmentry_HostCr0Fixed0,
    kVmxVDiag_Vmentry_HostCr0Fixed1,
    kVmxVDiag_Vmentry_HostCr3,
    kVmxVDiag_Vmentry_HostCr4Fixed0,
    kVmxVDiag_Vmentry_HostCr4Fixed1,
    kVmxVDiag_Vmentry_HostCr4Pae,
    kVmxVDiag_Vmentry_HostCr4Pcide,
    kVmxVDiag_Vmentry_HostCsTr,
    kVmxVDiag_Vmentry_HostEferMsr,
    kVmxVDiag_Vmentry_HostEferMsrRsvd,
    kVmxVDiag_Vmentry_HostGuestLongMode,
    kVmxVDiag_Vmentry_HostGuestLongModeNoCpu,
    kVmxVDiag_Vmentry_HostLongMode,
    kVmxVDiag_Vmentry_HostPatMsr,
    kVmxVDiag_Vmentry_HostRip,
    kVmxVDiag_Vmentry_HostRipRsvd,
    kVmxVDiag_Vmentry_HostSel,
    kVmxVDiag_Vmentry_HostSegBase,
    kVmxVDiag_Vmentry_HostSs,
    kVmxVDiag_Vmentry_HostSysenterEspEip,
    kVmxVDiag_Vmentry_LongModeCS,
    kVmxVDiag_Vmentry_MsrLoad,
    kVmxVDiag_Vmentry_MsrLoadCount,
    kVmxVDiag_Vmentry_MsrLoadPtrReadPhys,
    kVmxVDiag_Vmentry_MsrLoadRing3,
    kVmxVDiag_Vmentry_MsrLoadRsvd,
    kVmxVDiag_Vmentry_NmiWindowExit,
    kVmxVDiag_Vmentry_PinCtlsAllowed1,
    kVmxVDiag_Vmentry_PinCtlsDisallowed0,
    kVmxVDiag_Vmentry_ProcCtlsAllowed1,
    kVmxVDiag_Vmentry_ProcCtlsDisallowed0,
    kVmxVDiag_Vmentry_ProcCtls2Allowed1,
    kVmxVDiag_Vmentry_ProcCtls2Disallowed0,
    kVmxVDiag_Vmentry_PtrInvalid,
    kVmxVDiag_Vmentry_PtrReadPhys,
    kVmxVDiag_Vmentry_RealOrV86Mode,
    kVmxVDiag_Vmentry_SavePreemptTimer,
    kVmxVDiag_Vmentry_TprThresholdRsvd,
    kVmxVDiag_Vmentry_TprThresholdVTpr,
    kVmxVDiag_Vmentry_VirtApicPagePtrReadPhys,
    kVmxVDiag_Vmentry_VirtIntDelivery,
    kVmxVDiag_Vmentry_VirtNmi,
    kVmxVDiag_Vmentry_VirtX2ApicTprShadow,
    kVmxVDiag_Vmentry_VirtX2ApicVirtApic,
    kVmxVDiag_Vmentry_VmcsClear,
    kVmxVDiag_Vmentry_VmcsLaunch,
    kVmxVDiag_Vmentry_VmreadBitmapPtrReadPhys,
    kVmxVDiag_Vmentry_VmwriteBitmapPtrReadPhys,
    kVmxVDiag_Vmentry_VmxRoot,
    kVmxVDiag_Vmentry_Vpid,
    kVmxVDiag_Vmexit_HostPdpteCr3ReadPhys,
    kVmxVDiag_Vmexit_HostPdpte0Rsvd,
    kVmxVDiag_Vmexit_HostPdpte1Rsvd,
    kVmxVDiag_Vmexit_HostPdpte2Rsvd,
    kVmxVDiag_Vmexit_HostPdpte3Rsvd,
    kVmxVDiag_Vmexit_MsrLoad,
    kVmxVDiag_Vmexit_MsrLoadCount,
    kVmxVDiag_Vmexit_MsrLoadPtrReadPhys,
    kVmxVDiag_Vmexit_MsrLoadRing3,
    kVmxVDiag_Vmexit_MsrLoadRsvd,
    kVmxVDiag_Vmexit_MsrStore,
    kVmxVDiag_Vmexit_MsrStoreCount,
    kVmxVDiag_Vmexit_MsrStorePtrWritePhys,
    kVmxVDiag_Vmexit_MsrStoreRing3,
    kVmxVDiag_Vmexit_MsrStoreRsvd,
    /* Last member for determining array index limit. */
    kVmxVDiag_End
} VMXVDIAG;
AssertCompileSize(VMXVDIAG, 4);


/** @defgroup grp_hm_vmx_inline    VMX Inline Helpers
 * @{
 */
/**
 * Gets the effective width of a VMCS field given it's encoding adjusted for
 * HIGH/FULL access for 64-bit fields.
 *
 * @returns The effective VMCS field width.
 * @param   uFieldEnc   The VMCS field encoding.
 *
 * @remarks Warning! This function does not verify the encoding is for a valid and
 *          supported VMCS field.
 */
DECLINLINE(uint8_t) HMVmxGetVmcsFieldWidthEff(uint32_t uFieldEnc)
{
    /* Only the "HIGH" parts of all 64-bit fields have bit 0 set. */
    if (uFieldEnc & RT_BIT(0))
        return VMXVMCSFIELDWIDTH_32BIT;

    /* Bits 13:14 contains the width of the VMCS field, see VMXVMCSFIELDWIDTH_XXX. */
    return (uFieldEnc >> 13) & 0x3;
}

/**
 * Returns whether the given VMCS field is a read-only VMCS field or not.
 *
 * @returns @c true if it's a read-only field, @c false otherwise.
 * @param   uFieldEnc   The VMCS field encoding.
 *
 * @remarks Warning! This function does not verify the encoding is for a valid and
 *          supported VMCS field.
 */
DECLINLINE(bool) HMVmxIsVmcsFieldReadOnly(uint32_t uFieldEnc)
{
    /* See Intel spec. B.4.2 "Natural-Width Read-Only Data Fields". */
    return (RT_BF_GET(uFieldEnc, VMX_BF_VMCS_ENC_TYPE) == VMXVMCSFIELDTYPE_VMEXIT_INFO);
}

/**
 * Returns whether the given VM-entry interruption-information type is valid or not.
 *
 * @returns @c true if it's a valid type, @c false otherwise.
 * @param   fSupportsMTF    Whether the Monitor-Trap Flag CPU feature is supported.
 * @param   uType           The VM-entry interruption-information type.
 */
DECLINLINE(bool) HMVmxIsEntryIntInfoTypeValid(bool fSupportsMTF, uint8_t uType)
{
    /* See Intel spec. 26.2.1.3 "VM-Entry Control Fields". */
    switch (uType)
    {
        case VMX_ENTRY_INT_INFO_TYPE_EXT_INT:
        case VMX_ENTRY_INT_INFO_TYPE_NMI:
        case VMX_ENTRY_INT_INFO_TYPE_HW_XCPT:
        case VMX_ENTRY_INT_INFO_TYPE_SW_INT:
        case VMX_ENTRY_INT_INFO_TYPE_PRIV_SW_XCPT:
        case VMX_ENTRY_INT_INFO_TYPE_SW_XCPT:           return true;
        case VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT:       return fSupportsMTF;
        default:
            return false;
    }
}

/**
 * Returns whether the given VM-entry interruption-information vector and type
 * combination is valid or not.
 *
 * @returns @c true if it's a valid vector/type combination, @c false otherwise.
 * @param   uVector     The VM-entry interruption-information vector.
 * @param   uType       The VM-entry interruption-information type.
 *
 * @remarks Warning! This function does not validate the type field individually.
 *          Use it after verifying type is valid using HMVmxIsEntryIntInfoTypeValid.
 */
DECLINLINE(bool) HMVmxIsEntryIntInfoVectorValid(uint8_t uVector, uint8_t uType)
{
    /* See Intel spec. 26.2.1.3 "VM-Entry Control Fields". */
    if (   uType == VMX_ENTRY_INT_INFO_TYPE_NMI
        && uVector != X86_XCPT_NMI)
        return false;
    if (   uType == VMX_ENTRY_INT_INFO_TYPE_HW_XCPT
        && uVector > X86_XCPT_LAST)
        return false;
    if (   uType == VMX_ENTRY_INT_INFO_TYPE_OTHER_EVENT
        && uVector != VMX_ENTRY_INT_INFO_VECTOR_MTF)
        return false;
    return true;
}


/**
 * Returns whether or not the VM-exit is trap-like or fault-like.
 *
 * @returns @c true if it's a trap-like VM-exit, @c false otehrwise.
 * @param   uExitReason     The VM-exit reason.
 *
 * @remarks Warning! This does not validate the VM-exit reason.
 */
DECLINLINE(bool) HMVmxIsTrapLikeVmexit(uint32_t uExitReason)
{
    /*
     * Trap-like VM-exits - The instruction causing the VM-exit completes before the
     * VM-exit occurs.
     *
     * Fault-like VM-exits - The instruction causing the VM-exit is not completed before
     * the VM-exit occurs.
     *
     * See Intel spec. 25.5.2 "Monitor Trap Flag".
     * See Intel spec. 29.1.4 "EOI Virtualization".
     * See Intel spec. 29.4.3.3 "APIC-Write VM Exits".
     * See Intel spec. 29.1.2 "TPR Virtualization".
     */
    /** @todo NSTVMX: r=ramshankar: What about VM-exits due to debug traps (single-step,
     *        I/O breakpoints, data breakpoints), debug exceptions (data breakpoint)
     *        delayed by MovSS blocking, machine-check exceptions. */
    switch (uExitReason)
    {
        case VMX_EXIT_MTF:
        case VMX_EXIT_VIRTUALIZED_EOI:
        case VMX_EXIT_APIC_WRITE:
        case VMX_EXIT_TPR_BELOW_THRESHOLD:
            return true;
    }
    return false;
}
/** @} */


/** @defgroup grp_hm_vmx_asm    VMX Assembly Helpers
 * @{
 */

/**
 * Restores some host-state fields that need not be done on every VM-exit.
 *
 * @returns VBox status code.
 * @param   fRestoreHostFlags   Flags of which host registers needs to be
 *                              restored.
 * @param   pRestoreHost        Pointer to the host-restore structure.
 */
DECLASM(int) VMXRestoreHostState(uint32_t fRestoreHostFlags, PVMXRESTOREHOST pRestoreHost);


/**
 * Dispatches an NMI to the host.
 */
DECLASM(int) VMXDispatchHostNmi(void);


/**
 * Executes VMXON.
 *
 * @returns VBox status code.
 * @param   HCPhysVmxOn      Physical address of VMXON structure.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXEnable(RTHCPHYS HCPhysVmxOn);
#else
DECLINLINE(int) VMXEnable(RTHCPHYS HCPhysVmxOn)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "push     %3                                             \n\t"
       "push     %2                                             \n\t"
       ".byte    0xf3, 0x0f, 0xc7, 0x34, 0x24  # VMXON [esp]    \n\t"
       "ja       2f                                             \n\t"
       "je       1f                                             \n\t"
       "movl     $" RT_XSTR(VERR_VMX_INVALID_VMXON_PTR)", %0    \n\t"
       "jmp      2f                                             \n\t"
       "1:                                                      \n\t"
       "movl     $" RT_XSTR(VERR_VMX_VMXON_FAILED)", %0         \n\t"
       "2:                                                      \n\t"
       "add      $8, %%esp                                      \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)HCPhysVmxOn),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(HCPhysVmxOn >> 32)) /* this would not work with -fomit-frame-pointer */
       :"memory"
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc = __vmx_on(&HCPhysVmxOn);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMXON_PTR : VERR_VMX_VMXON_FAILED;

# else
    int rc = VINF_SUCCESS;
    __asm
    {
        push    dword ptr [HCPhysVmxOn + 4]
        push    dword ptr [HCPhysVmxOn]
        _emit   0xf3
        _emit   0x0f
        _emit   0xc7
        _emit   0x34
        _emit   0x24     /* VMXON [esp] */
        jnc     vmxon_good
        mov     dword ptr [rc], VERR_VMX_INVALID_VMXON_PTR
        jmp     the_end

vmxon_good:
        jnz     the_end
        mov     dword ptr [rc], VERR_VMX_VMXON_FAILED
the_end:
        add     esp, 8
    }
    return rc;
# endif
}
#endif


/**
 * Executes VMXOFF.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(void) VMXDisable(void);
#else
DECLINLINE(void) VMXDisable(void)
{
# if RT_INLINE_ASM_GNU_STYLE
    __asm__ __volatile__ (
       ".byte 0x0f, 0x01, 0xc4  # VMXOFF                        \n\t"
       );

# elif VMX_USE_MSC_INTRINSICS
    __vmx_off();

# else
    __asm
    {
        _emit   0x0f
        _emit   0x01
        _emit   0xc4   /* VMXOFF */
    }
# endif
}
#endif


/**
 * Executes VMCLEAR.
 *
 * @returns VBox status code.
 * @param   HCPhysVmcs       Physical address of VM control structure.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXClearVmcs(RTHCPHYS HCPhysVmcs);
#else
DECLINLINE(int) VMXClearVmcs(RTHCPHYS HCPhysVmcs)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "push    %3                                              \n\t"
       "push    %2                                              \n\t"
       ".byte   0x66, 0x0f, 0xc7, 0x34, 0x24  # VMCLEAR [esp]   \n\t"
       "jnc     1f                                              \n\t"
       "movl    $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0      \n\t"
       "1:                                                      \n\t"
       "add     $8, %%esp                                       \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)HCPhysVmcs),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(HCPhysVmcs >> 32)) /* this would not work with -fomit-frame-pointer */
       :"memory"
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc = __vmx_vmclear(&HCPhysVmcs);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return VERR_VMX_INVALID_VMCS_PTR;

# else
    int rc = VINF_SUCCESS;
    __asm
    {
        push    dword ptr [HCPhysVmcs + 4]
        push    dword ptr [HCPhysVmcs]
        _emit   0x66
        _emit   0x0f
        _emit   0xc7
        _emit   0x34
        _emit   0x24     /* VMCLEAR [esp] */
        jnc     success
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR
success:
        add     esp, 8
    }
    return rc;
# endif
}
#endif


/**
 * Executes VMPTRLD.
 *
 * @returns VBox status code.
 * @param   HCPhysVmcs       Physical address of VMCS structure.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXActivateVmcs(RTHCPHYS HCPhysVmcs);
#else
DECLINLINE(int) VMXActivateVmcs(RTHCPHYS HCPhysVmcs)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "push    %3                                              \n\t"
       "push    %2                                              \n\t"
       ".byte   0x0f, 0xc7, 0x34, 0x24  # VMPTRLD [esp]         \n\t"
       "jnc     1f                                              \n\t"
       "movl    $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0      \n\t"
       "1:                                                      \n\t"
       "add     $8, %%esp                                       \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "ir"((uint32_t)HCPhysVmcs),        /* don't allow direct memory reference here, */
        "ir"((uint32_t)(HCPhysVmcs >> 32)) /* this will not work with -fomit-frame-pointer */
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc = __vmx_vmptrld(&HCPhysVmcs);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return VERR_VMX_INVALID_VMCS_PTR;

# else
    int rc = VINF_SUCCESS;
    __asm
    {
        push    dword ptr [HCPhysVmcs + 4]
        push    dword ptr [HCPhysVmcs]
        _emit   0x0f
        _emit   0xc7
        _emit   0x34
        _emit   0x24     /* VMPTRLD [esp] */
        jnc     success
        mov     dword ptr [rc], VERR_VMX_INVALID_VMCS_PTR

success:
        add     esp, 8
    }
    return rc;
# endif
}
#endif


/**
 * Executes VMPTRST.
 *
 * @returns VBox status code.
 * @param   pHCPhysVmcs    Where to store the physical address of the current
 *                         VMCS.
 */
DECLASM(int) VMXGetActivatedVmcs(RTHCPHYS *pHCPhysVmcs);


/**
 * Executes VMWRITE.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       VMCS field encoding.
 * @param   u32Val          The 32-bit value to set.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXWriteVmcs32(uint32_t uFieldEnc, uint32_t u32Val);
#else
DECLINLINE(int) VMXWriteVmcs32(uint32_t uFieldEnc, uint32_t u32Val)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       ".byte  0x0f, 0x79, 0xc2        # VMWRITE eax, edx       \n\t"
       "ja     2f                                               \n\t"
       "je     1f                                               \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0       \n\t"
       "jmp    2f                                               \n\t"
       "1:                                                      \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_FIELD)", %0     \n\t"
       "2:                                                      \n\t"
       :"=rm"(rc)
       :"0"(VINF_SUCCESS),
        "a"(uFieldEnc),
        "d"(u32Val)
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
     unsigned char rcMsc = __vmx_vmwrite(uFieldEnc, u32Val);
     if (RT_LIKELY(rcMsc == 0))
         return VINF_SUCCESS;
     return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;

#else
    int rc = VINF_SUCCESS;
    __asm
    {
        push   dword ptr [u32Val]
        mov    eax, [uFieldEnc]
        _emit  0x0f
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
    return rc;
# endif
}
#endif

/**
 * Executes VMWRITE.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   u64Val          The 16, 32 or 64-bit value to set.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if !defined(RT_ARCH_X86)
# if !VMX_USE_MSC_INTRINSICS || ARCH_BITS != 64
DECLASM(int) VMXWriteVmcs64(uint32_t uFieldEnc, uint64_t u64Val);
# else  /* VMX_USE_MSC_INTRINSICS */
DECLINLINE(int) VMXWriteVmcs64(uint32_t uFieldEnc, uint64_t u64Val)
{
    unsigned char rcMsc = __vmx_vmwrite(uFieldEnc, u64Val);
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;
}
# endif /* VMX_USE_MSC_INTRINSICS */
#else
# define VMXWriteVmcs64(uFieldEnc, u64Val)    VMXWriteVmcs64Ex(pVCpu, uFieldEnc, u64Val) /** @todo dead ugly, picking up pVCpu like this */
VMMR0DECL(int) VMXWriteVmcs64Ex(PVMCPU pVCpu, uint32_t uFieldEnc, uint64_t u64Val);
#endif

#if ARCH_BITS == 32
# define VMXWriteVmcsHstN                       VMXWriteVmcs32
# define VMXWriteVmcsGstN(uFieldEnc, u64Val)     VMXWriteVmcs64Ex(pVCpu, uFieldEnc, u64Val)
#else  /* ARCH_BITS == 64 */
# define VMXWriteVmcsHstN                       VMXWriteVmcs64
# define VMXWriteVmcsGstN                       VMXWriteVmcs64
#endif


/**
 * Invalidate a page using INVEPT.
 *
 * @returns VBox status code.
 * @param   enmFlush        Type of flush.
 * @param   pDescriptor     Pointer to the descriptor.
 */
DECLASM(int) VMXR0InvEPT(VMXTLBFLUSHEPT enmFlush, uint64_t *pDescriptor);


/**
 * Invalidate a page using INVVPID.
 *
 * @returns VBox status code.
 * @param   enmFlush        Type of flush.
 * @param   pDescriptor     Pointer to the descriptor.
 */
DECLASM(int) VMXR0InvVPID(VMXTLBFLUSHVPID enmFlush, uint64_t *pDescriptor);


/**
 * Executes VMREAD for a 32-bit field.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   pData           Where to store VMCS field value.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if ((RT_INLINE_ASM_EXTERNAL || !defined(RT_ARCH_X86)) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXReadVmcs32(uint32_t uFieldEnc, uint32_t *pData);
#else
DECLINLINE(int) VMXReadVmcs32(uint32_t uFieldEnc, uint32_t *pData)
{
# if RT_INLINE_ASM_GNU_STYLE
    int rc = VINF_SUCCESS;
    __asm__ __volatile__ (
       "movl   $" RT_XSTR(VINF_SUCCESS)", %0                     \n\t"
       ".byte  0x0f, 0x78, 0xc2        # VMREAD eax, edx         \n\t"
       "ja     2f                                                \n\t"
       "je     1f                                                \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_PTR)", %0        \n\t"
       "jmp    2f                                                \n\t"
       "1:                                                       \n\t"
       "movl   $" RT_XSTR(VERR_VMX_INVALID_VMCS_FIELD)", %0      \n\t"
       "2:                                                       \n\t"
       :"=&r"(rc),
        "=d"(*pData)
       :"a"(uFieldEnc),
        "d"(0)
       );
    return rc;

# elif VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc;
#  if ARCH_BITS == 32
    rcMsc = __vmx_vmread(uFieldEnc, pData);
#  else
    uint64_t u64Tmp;
    rcMsc = __vmx_vmread(uFieldEnc, &u64Tmp);
    *pData = (uint32_t)u64Tmp;
#  endif
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;

#else
    int rc = VINF_SUCCESS;
    __asm
    {
        sub     esp, 4
        mov     dword ptr [esp], 0
        mov     eax, [uFieldEnc]
        _emit   0x0f
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
    return rc;
# endif
}
#endif

/**
 * Executes VMREAD for a 64-bit field.
 *
 * @returns VBox status code.
 * @retval  VINF_SUCCESS.
 * @retval  VERR_VMX_INVALID_VMCS_PTR.
 * @retval  VERR_VMX_INVALID_VMCS_FIELD.
 *
 * @param   uFieldEnc       The VMCS field encoding.
 * @param   pData           Where to store VMCS field value.
 *
 * @remarks The values of the two status codes can be OR'ed together, the result
 *          will be VERR_VMX_INVALID_VMCS_PTR.
 */
#if (!defined(RT_ARCH_X86) && !VMX_USE_MSC_INTRINSICS)
DECLASM(int) VMXReadVmcs64(uint32_t uFieldEnc, uint64_t *pData);
#else
DECLINLINE(int) VMXReadVmcs64(uint32_t uFieldEnc, uint64_t *pData)
{
# if VMX_USE_MSC_INTRINSICS
    unsigned char rcMsc;
#  if ARCH_BITS == 32
    size_t        uLow;
    size_t        uHigh;
    rcMsc  = __vmx_vmread(uFieldEnc, &uLow);
    rcMsc |= __vmx_vmread(uFieldEnc + 1, &uHigh);
    *pData = RT_MAKE_U64(uLow, uHigh);
# else
    rcMsc = __vmx_vmread(uFieldEnc, pData);
# endif
    if (RT_LIKELY(rcMsc == 0))
        return VINF_SUCCESS;
    return rcMsc == 2 ? VERR_VMX_INVALID_VMCS_PTR : VERR_VMX_INVALID_VMCS_FIELD;

# elif ARCH_BITS == 32
    int rc;
    uint32_t val_hi, val;
    rc  = VMXReadVmcs32(uFieldEnc, &val);
    rc |= VMXReadVmcs32(uFieldEnc + 1, &val_hi);
    AssertRC(rc);
    *pData = RT_MAKE_U64(val, val_hi);
    return rc;

# else
#  error "Shouldn't be here..."
# endif
}
#endif


/**
 * Gets the last instruction error value from the current VMCS.
 *
 * @returns VBox status code.
 */
DECLINLINE(uint32_t) VMXGetLastError(void)
{
#if ARCH_BITS == 64
    uint64_t uLastError = 0;
    int rc = VMXReadVmcs64(VMX_VMCS32_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return (uint32_t)uLastError;

#else /* 32-bit host: */
    uint32_t uLastError = 0;
    int rc = VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return uLastError;
#endif
}

/** @} */

/** @} */

#endif

