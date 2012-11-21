/** @file
 * HM - VMX Structures and Definitions. (VMM)
 */

/*
 * Copyright (C) 2006-2010 Oracle Corporation
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

/** @defgroup grp_vmx   vmx Types and Definitions
 * @ingroup grp_hm
 * @{
 */

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
#pragma pack(1)
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
    /** Availabe for software. */
    uint64_t    u12Available    : 12;
} EPTPML4EBITS;
#pragma pack()
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
#pragma pack(1)
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
#pragma pack()
/** Pointer to a PML4 table entry. */
typedef EPTPML4E *PEPTPML4E;
/** Pointer to a const PML4 table entry. */
typedef const EPTPML4E *PCEPTPML4E;
AssertCompileSize(EPTPML4E, 8);

/**
 * EPT PML4 Table.
 */
#pragma pack(1)
typedef struct EPTPML4
{
    EPTPML4E    a[EPT_PG_ENTRIES];
} EPTPML4;
#pragma pack()
/** Pointer to an EPT PML4 Table. */
typedef EPTPML4 *PEPTPML4;
/** Pointer to a const EPT PML4 Table. */
typedef const EPTPML4 *PCEPTPML4;

/**
 * EPT Page Directory Pointer Entry. Bit view.
 */
#pragma pack(1)
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
    /** Availabe for software. */
    uint64_t    u12Available    : 12;
} EPTPDPTEBITS;
#pragma pack()
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
#pragma pack(1)
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
#pragma pack()
/** Pointer to an EPT Page Directory Pointer Entry. */
typedef EPTPDPTE *PEPTPDPTE;
/** Pointer to a const EPT Page Directory Pointer Entry. */
typedef const EPTPDPTE *PCEPTPDPTE;
AssertCompileSize(EPTPDPTE, 8);

/**
 * EPT Page Directory Pointer Table.
 */
#pragma pack(1)
typedef struct EPTPDPT
{
    EPTPDPTE    a[EPT_PG_ENTRIES];
} EPTPDPT;
#pragma pack()
/** Pointer to an EPT Page Directory Pointer Table. */
typedef EPTPDPT *PEPTPDPT;
/** Pointer to a const EPT Page Directory Pointer Table. */
typedef const EPTPDPT *PCEPTPDPT;


/**
 * EPT Page Directory Table Entry. Bit view.
 */
#pragma pack(1)
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
    /** Availabe for software. */
    uint64_t    u12Available    : 12;
} EPTPDEBITS;
#pragma pack()
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
#pragma pack(1)
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
    /** Availabe for software. */
    uint64_t    u12Available    : 12;
} EPTPDE2MBITS;
#pragma pack()
AssertCompileSize(EPTPDE2MBITS, 8);

/** Bits 21-51 - - EPT - Physical Page number of the next level. */
#define EPT_PDE2M_PG_MASK       X86_PDE2M_PAE_PG_MASK

/**
 * EPT Page Directory Table Entry.
 */
#pragma pack(1)
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
#pragma pack()
/** Pointer to an EPT Page Directory Table Entry. */
typedef EPTPDE *PEPTPDE;
/** Pointer to a const EPT Page Directory Table Entry. */
typedef const EPTPDE *PCEPTPDE;
AssertCompileSize(EPTPDE, 8);

/**
 * EPT Page Directory Table.
 */
#pragma pack(1)
typedef struct EPTPD
{
    EPTPDE      a[EPT_PG_ENTRIES];
} EPTPD;
#pragma pack()
/** Pointer to an EPT Page Directory Table. */
typedef EPTPD *PEPTPD;
/** Pointer to a const EPT Page Directory Table. */
typedef const EPTPD *PCEPTPD;


/**
 * EPT Page Table Entry. Bit view.
 */
#pragma pack(1)
typedef struct EPTPTEBITS
{
    /** 0 - Present bit.
     * @remark This is a convenience "misnomer".  The bit actually indicates
     *         read access and the CPU will consider an entry with any of the
     *         first three bits set as present.  Since all our valid entries
     *         will have this bit set, it can be used as a present indicator
     *         and allow some code sharing. */
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
#pragma pack()
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
#pragma pack(1)
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
#pragma pack()
/** Pointer to an EPT Page Directory Table Entry. */
typedef EPTPTE *PEPTPTE;
/** Pointer to a const EPT Page Directory Table Entry. */
typedef const EPTPTE *PCEPTPTE;
AssertCompileSize(EPTPTE, 8);

/**
 * EPT Page Table.
 */
#pragma pack(1)
typedef struct EPTPT
{
    EPTPTE      a[EPT_PG_ENTRIES];
} EPTPT;
#pragma pack()
/** Pointer to an extended page table. */
typedef EPTPT *PEPTPT;
/** Pointer to a const extended table. */
typedef const EPTPT *PCEPTPT;

/**
 * VPID flush types.
 */
typedef enum
{
    /** Invalidate a specific page. */
    VMX_FLUSH_VPID_INDIV_ADDR                    = 0,
    /** Invalidate one context (specific VPID). */
    VMX_FLUSH_VPID_SINGLE_CONTEXT                = 1,
    /** Invalidate all contexts (all VPIDs). */
    VMX_FLUSH_VPID_ALL_CONTEXTS                  = 2,
    /** Invalidate a single VPID context retaining global mappings. */
    VMX_FLUSH_VPID_SINGLE_CONTEXT_RETAIN_GLOBALS = 3,
    /** Unsupported by VirtualBox. */
    VMX_FLUSH_VPID_NOT_SUPPORTED                 = 0xbad,
    /** Unsupported by CPU. */
    VMX_FLUSH_VPID_NONE                          = 0xb00,
    /** 32bit hackishness. */
    VMX_FLUSH_VPID_32BIT_HACK                    = 0x7fffffff
} VMX_FLUSH_VPID;

/**
 * EPT flush types.
 */
typedef enum
{
    /** Invalidate one context (specific EPT). */
    VMX_FLUSH_EPT_SINGLE_CONTEXT                = 1,
    /* Invalidate all contexts (all EPTs) */
    VMX_FLUSH_EPT_ALL_CONTEXTS                  = 2,
    /** Unsupported by VirtualBox.   */
    VMX_FLUSH_EPT_NOT_SUPPORTED                 = 0xbad,
    /** Unsupported by CPU. */
    VMX_FLUSH_EPT_NONE                          = 0xb00,
    /** 32bit hackishness. */
    VMX_FLUSH_EPT_32BIT_HACK                    = 0x7fffffff
} VMX_FLUSH_EPT;
/** @} */

/** @name MSR load/store elements
 * @{
 */
#pragma pack(1)
typedef struct
{
    uint32_t    u32IndexMSR;
    uint32_t    u32Reserved;
    uint64_t    u64Value;
} VMXMSR;
#pragma pack()
/** Pointer to an MSR load/store element. */
typedef VMXMSR *PVMXMSR;
/** Pointer to a const MSR load/store element. */
typedef const VMXMSR *PCVMXMSR;

/** @} */


/** @name VT-x capability qword
 * @{
 */
#pragma pack(1)
typedef union
{
    struct
    {
        /** Bits set here -must- be set in the correpsonding VM-execution controls. */
        uint32_t        disallowed0;
        /** Bits cleared here -must- be cleared in the corresponding VM-execution
         *  controls. */
        uint32_t        allowed1;
    } n;
    uint64_t            u;
} VMX_CAPABILITY;
#pragma pack()
/** @} */

/** @name VMX Basic Exit Reasons.
 * @{
 */
/** And-mask for setting reserved bits to zero */
#define VMX_EFLAGS_RESERVED_0           (~0xffc08028)
/** Or-mask for setting reserved bits to 1 */
#define VMX_EFLAGS_RESERVED_1           0x00000002
/** @} */

/** @name VMX Basic Exit Reasons.
 * @{
 */
/** -1 Invalid exit code */
#define VMX_EXIT_INVALID            -1
/** 0 Exception or non-maskable interrupt (NMI). */
#define VMX_EXIT_EXCEPTION_NMI      0
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
/** 14 Guest software attempted to execute INVLPG. */
#define VMX_EXIT_INVLPG             14
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
/** 37 VM exit due to monitor trap flag. */
#define VMX_EXIT_MTF                37
/** 39 Guest software attempted to execute MONITOR. */
#define VMX_EXIT_MONITOR            39
/** 40 Guest software attempted to execute PAUSE. */
#define VMX_EXIT_PAUSE              40
/** 41 VM-entry failure due to machine-check. */
#define VMX_EXIT_ERR_MACHINE_CHECK  41
/** 43 TPR below threshold. Guest software executed MOV to CR8. */
#define VMX_EXIT_TPR                43
/** 44 APIC access. Guest software attempted to access memory at a physical address on the APIC-access page. */
#define VMX_EXIT_APIC_ACCESS        44
/** 46 Access to GDTR or IDTR. Guest software attempted to execute LGDT, LIDT, SGDT, or SIDT. */
#define VMX_EXIT_XDTR_ACCESS        46
/** 47 Access to LDTR or TR. Guest software attempted to execute LLDT, LTR, SLDT, or STR. */
#define VMX_EXIT_TR_ACCESS          47
/** 48 EPT violation. An attempt to access memory with a guest-physical address was disallowed by the configuration of the EPT paging structures. */
#define VMX_EXIT_EPT_VIOLATION      48
/** 49 EPT misconfiguration. An attempt to access memory with a guest-physical address encountered a misconfigured EPT paging-structure entry. */
#define VMX_EXIT_EPT_MISCONFIG      49
/** 50 INVEPT. Guest software attempted to execute INVEPT. */
#define VMX_EXIT_INVEPT             50
/** 51 RDTSCP. Guest software attempted to execute RDTSCP. */
#define VMX_EXIT_RDTSCP             51
/** 52 VMX-preemption timer expired. The preemption timer counted down to zero. */
#define VMX_EXIT_PREEMPTION_TIMER   52
/** 53 INVVPID. Guest software attempted to execute INVVPID. */
#define VMX_EXIT_INVVPID            53
/** 54 WBINVD. Guest software attempted to execute WBINVD. */
#define VMX_EXIT_WBINVD             54
/** 55 XSETBV. Guest software attempted to execute XSETBV. */
#define VMX_EXIT_XSETBV             55
/** 57 RDRAND. Guest software attempted to execute RDRAND. */
#define VMX_EXIT_RDRAND             57
/** 58 INVPCID. Guest software attempted to execute INVPCID. */
#define VMX_EXIT_INVPCID            58
/** 59 VMFUNC. Guest software attempted to execute VMFUNC. */
#define VMX_EXIT_VMFUNC             59
/** The maximum exit value (inclusive). */
#define VMX_EXIT_MAX                (VMX_EXIT_VMFUNC)
/** @} */


/** @name VM Instruction Errors
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
/** 26 Invalid operand to INVEPT/INVVPID. */
#define VMX_ERROR_INVEPTVPID_INVALID_OPERAND        28

/** @} */


/** @name VMX MSRs - Basic VMX information.
 * @{
 */
/** VMCS revision identifier used by the processor. */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_ID(a)                      (a & 0x7FFFFFFF)
/** Size of the VMCS. */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_SIZE(a)                    (((a) >> 32) & 0xFFF)
/** Width of physical address used for the VMCS.
 *  0 -> limited to the available amount of physical ram
 *  1 -> within the first 4 GB
 */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_PHYS_WIDTH(a)              (((a) >> 48) & 1)
/** Whether the processor supports the dual-monitor treatment of system-management interrupts and system-management code. (always 1) */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_DUAL_MON(a)                (((a) >> 49) & 1)
/** Memory type that must be used for the VMCS. */
#define MSR_IA32_VMX_BASIC_INFO_VMCS_MEM_TYPE(a)                (((a) >> 50) & 0xF)
/** @} */


/** @name VMX MSRs - Misc VMX info.
 * @{
 */
/** Relationship between the preemption timer and tsc; count down every time bit x of the tsc changes. */
#define MSR_IA32_VMX_MISC_PREEMPT_TSC_BIT(a)                    ((a) & 0x1f)
/** Activity states supported by the implementation. */
#define MSR_IA32_VMX_MISC_ACTIVITY_STATES(a)                    (((a) >> 6) & 0x7)
/** Number of CR3 target values supported by the processor. (0-256) */
#define MSR_IA32_VMX_MISC_CR3_TARGET(a)                         (((a) >> 16) & 0x1FF)
/** Maximum nr of MSRs in the VMCS. (N+1)*512. */
#define MSR_IA32_VMX_MISC_MAX_MSR(a)                            (((((a) >> 25) & 0x7) + 1) * 512)
/** MSEG revision identifier used by the processor. */
#define MSR_IA32_VMX_MISC_MSEG_ID(a)                            ((a) >> 32)
/** @} */


/** @name VMX MSRs - VMCS enumeration field info
 * @{
 */
/** Highest field index. */
#define MSR_IA32_VMX_VMCS_ENUM_HIGHEST_INDEX(a)                 (((a) >> 1) & 0x1FF)

/** @} */


/** @name MSR_IA32_VMX_EPT_VPID_CAPS; EPT capabilities MSR
 * @{
 */
#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_X_ONLY                                RT_BIT_64(0)
#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_W_ONLY                                RT_BIT_64(1)
#define MSR_IA32_VMX_EPT_VPID_CAP_RWX_WX_ONLY                               RT_BIT_64(2)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_21_BITS                               RT_BIT_64(3)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_30_BITS                               RT_BIT_64(4)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_39_BITS                               RT_BIT_64(5)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_48_BITS                               RT_BIT_64(6)
#define MSR_IA32_VMX_EPT_VPID_CAP_GAW_57_BITS                               RT_BIT_64(7)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_UC                                    RT_BIT_64(8)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WC                                    RT_BIT_64(9)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WT                                    RT_BIT_64(12)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WP                                    RT_BIT_64(13)
#define MSR_IA32_VMX_EPT_VPID_CAP_EMT_WB                                    RT_BIT_64(14)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_21_BITS                                RT_BIT_64(16)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_30_BITS                                RT_BIT_64(17)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_39_BITS                                RT_BIT_64(18)
#define MSR_IA32_VMX_EPT_VPID_CAP_SP_48_BITS                                RT_BIT_64(19)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT                                    RT_BIT_64(20)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_SINGLE_CONTEXT                     RT_BIT_64(25)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVEPT_ALL_CONTEXTS                       RT_BIT_64(26)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID                                   RT_BIT_64(32)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_INDIV_ADDR                        RT_BIT_64(40)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT                    RT_BIT_64(41)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_ALL_CONTEXTS                      RT_BIT_64(42)
#define MSR_IA32_VMX_EPT_VPID_CAP_INVVPID_SINGLE_CONTEXT_RETAIN_GLOBALS     RT_BIT_64(43)

/** @} */

/** @name Extended Page Table Pointer (EPTP)
 * @{
 */
/** Uncachable EPT paging structure memory type. */
#define VMX_EPT_MEMTYPE_UC                                  0
/** Write-back EPT paging structure memory type. */
#define VMX_EPT_MEMTYPE_WB                                  6
/** Shift value to get the EPT page walk length (bits 5-3) */
#define VMX_EPT_PAGE_WALK_LENGTH_SHIFT                      3
/** Mask value to get the EPT page walk length (bits 5-3) */
#define VMX_EPT_PAGE_WALK_LENGTH_MASK                       7
/** Default EPT page-walk length (1 less than the actual EPT page-walk
 *  length) */
#define VMX_EPT_PAGE_WALK_LENGTH_DEFAULT                    3
/** @} */


/** @name VMCS field encoding - 16 bits guest fields
 * @{
 */
#define VMX_VMCS16_GUEST_FIELD_VPID                               0x0
#define VMX_VMCS16_GUEST_FIELD_ES                                 0x800
#define VMX_VMCS16_GUEST_FIELD_CS                                 0x802
#define VMX_VMCS16_GUEST_FIELD_SS                                 0x804
#define VMX_VMCS16_GUEST_FIELD_DS                                 0x806
#define VMX_VMCS16_GUEST_FIELD_FS                                 0x808
#define VMX_VMCS16_GUEST_FIELD_GS                                 0x80A
#define VMX_VMCS16_GUEST_FIELD_LDTR                               0x80C
#define VMX_VMCS16_GUEST_FIELD_TR                                 0x80E
/** @} */

/** @name VMCS field encoding - 16 bits host fields
 * @{
 */
#define VMX_VMCS16_HOST_FIELD_ES                                  0xC00
#define VMX_VMCS16_HOST_FIELD_CS                                  0xC02
#define VMX_VMCS16_HOST_FIELD_SS                                  0xC04
#define VMX_VMCS16_HOST_FIELD_DS                                  0xC06
#define VMX_VMCS16_HOST_FIELD_FS                                  0xC08
#define VMX_VMCS16_HOST_FIELD_GS                                  0xC0A
#define VMX_VMCS16_HOST_FIELD_TR                                  0xC0C
/** @}          */

/** @name VMCS field encoding - 64 bits host fields
 * @{
 */
#define VMX_VMCS64_HOST_FIELD_PAT_FULL                            0x2C00
#define VMX_VMCS64_HOST_FIELD_PAT_HIGH                            0x2C01
#define VMX_VMCS64_HOST_FIELD_EFER_FULL                           0x2C02
#define VMX_VMCS64_HOST_FIELD_EFER_HIGH                           0x2C03
#define VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_FULL                     0x2C04      /**< MSR IA32_PERF_GLOBAL_CTRL */
#define VMX_VMCS64_HOST_PERF_GLOBAL_CTRL_HIGH                     0x2C05      /**< MSR IA32_PERF_GLOBAL_CTRL */
/** @}          */


/** @name VMCS field encoding - 64 Bits control fields
 * @{
 */
#define VMX_VMCS64_CTRL_IO_BITMAP_A_FULL                          0x2000
#define VMX_VMCS64_CTRL_IO_BITMAP_A_HIGH                          0x2001
#define VMX_VMCS64_CTRL_IO_BITMAP_B_FULL                          0x2002
#define VMX_VMCS64_CTRL_IO_BITMAP_B_HIGH                          0x2003

/* Optional */
#define VMX_VMCS64_CTRL_MSR_BITMAP_FULL                           0x2004
#define VMX_VMCS64_CTRL_MSR_BITMAP_HIGH                           0x2005

#define VMX_VMCS64_CTRL_EXIT_MSR_STORE_FULL                       0x2006
#define VMX_VMCS64_CTRL_EXIT_MSR_STORE_HIGH                       0x2007
#define VMX_VMCS64_CTRL_EXIT_MSR_LOAD_FULL                        0x2008
#define VMX_VMCS64_CTRL_EXIT_MSR_LOAD_HIGH                        0x2009

#define VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_FULL                       0x200A
#define VMX_VMCS64_CTRL_ENTRY_MSR_LOAD_HIGH                       0x200B

#define VMX_VMCS64_CTRL_EXEC_VMCS_PTR_FULL                        0x200C
#define VMX_VMCS64_CTRL_EXEC_VMCS_PTR_HIGH                        0x200D

#define VMX_VMCS64_CTRL_TSC_OFFSET_FULL                           0x2010
#define VMX_VMCS64_CTRL_TSC_OFFSET_HIGH                           0x2011

/** Optional (VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW) */
#define VMX_VMCS64_CTRL_VAPIC_PAGEADDR_FULL                       0x2012
#define VMX_VMCS64_CTRL_VAPIC_PAGEADDR_HIGH                       0x2013

/** Optional (VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC) */
#define VMX_VMCS64_CTRL_APIC_ACCESSADDR_FULL                      0x2014
#define VMX_VMCS64_CTRL_APIC_ACCESSADDR_HIGH                      0x2015

/** Optional (VMX_VMCS_CTRL_PROC_EXEC2_VMFUNC) */
#define VMX_VMCS64_CTRL_VMFUNC_CTRLS_FULL                         0x2018
#define VMX_VMCS64_CTRL_VMFUNC_CTRLS_HIGH                         0x2019

/** Extended page table pointer. */
#define VMX_VMCS64_CTRL_EPTP_FULL                                 0x201a
#define VMX_VMCS64_CTRL_EPTP_HIGH                                 0x201b

/** Extended page table pointer lists. */
#define VMX_VMCS64_CTRL_EPTP_LIST_FULL                            0x2024
#define VMX_VMCS64_CTRL_EPTP_LIST_HIGH                            0x2025

/** VM-exit guest phyiscal address. */
#define VMX_VMCS64_EXIT_GUEST_PHYS_ADDR_FULL                      0x2400
#define VMX_VMCS64_EXIT_GUEST_PHYS_ADDR_HIGH                      0x2401
/** @} */


/** @name VMCS field encoding - 64 Bits guest fields
 * @{
 */
#define VMX_VMCS64_GUEST_VMCS_LINK_PTR_FULL                     0x2800
#define VMX_VMCS64_GUEST_VMCS_LINK_PTR_HIGH                     0x2801
#define VMX_VMCS64_GUEST_DEBUGCTL_FULL                          0x2802      /**< MSR IA32_DEBUGCTL */
#define VMX_VMCS64_GUEST_DEBUGCTL_HIGH                          0x2803      /**< MSR IA32_DEBUGCTL */
#define VMX_VMCS64_GUEST_PAT_FULL                               0x2804
#define VMX_VMCS64_GUEST_PAT_HIGH                               0x2805
#define VMX_VMCS64_GUEST_EFER_FULL                              0x2806
#define VMX_VMCS64_GUEST_EFER_HIGH                              0x2807
#define VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_FULL                  0x2808      /**< MSR IA32_PERF_GLOBAL_CTRL */
#define VMX_VMCS64_GUEST_PERF_GLOBAL_CTRL_HIGH                  0x2809      /**< MSR IA32_PERF_GLOBAL_CTRL */
#define VMX_VMCS64_GUEST_PDPTE0_FULL                            0x280A
#define VMX_VMCS64_GUEST_PDPTE0_HIGH                            0x280B
#define VMX_VMCS64_GUEST_PDPTE1_FULL                            0x280C
#define VMX_VMCS64_GUEST_PDPTE1_HIGH                            0x280D
#define VMX_VMCS64_GUEST_PDPTE2_FULL                            0x280E
#define VMX_VMCS64_GUEST_PDPTE2_HIGH                            0x280F
#define VMX_VMCS64_GUEST_PDPTE3_FULL                            0x2810
#define VMX_VMCS64_GUEST_PDPTE3_HIGH                            0x2811
/** @} */


/** @name VMCS field encoding - 32 Bits control fields
 * @{
 */
#define VMX_VMCS32_CTRL_PIN_EXEC_CONTROLS                       0x4000
#define VMX_VMCS32_CTRL_PROC_EXEC_CONTROLS                      0x4002
#define VMX_VMCS32_CTRL_EXCEPTION_BITMAP                        0x4004
#define VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MASK                    0x4006
#define VMX_VMCS32_CTRL_PAGEFAULT_ERROR_MATCH                   0x4008
#define VMX_VMCS32_CTRL_CR3_TARGET_COUNT                        0x400A
#define VMX_VMCS32_CTRL_EXIT_CONTROLS                           0x400C
#define VMX_VMCS32_CTRL_EXIT_MSR_STORE_COUNT                    0x400E
#define VMX_VMCS32_CTRL_EXIT_MSR_LOAD_COUNT                     0x4010
#define VMX_VMCS32_CTRL_ENTRY_CONTROLS                          0x4012
#define VMX_VMCS32_CTRL_ENTRY_MSR_LOAD_COUNT                    0x4014
#define VMX_VMCS32_CTRL_ENTRY_IRQ_INFO                          0x4016
#define VMX_VMCS32_CTRL_ENTRY_EXCEPTION_ERRCODE                 0x4018
#define VMX_VMCS32_CTRL_ENTRY_INSTR_LENGTH                      0x401A
#define VMX_VMCS32_CTRL_TPR_THRESHOLD                           0x401C
#define VMX_VMCS32_CTRL_PROC_EXEC_CONTROLS2                     0x401E
/** @} */


/** @name VMX_VMCS_CTRL_PIN_EXEC_CONTROLS
 * @{
 */
/** External interrupts cause VM exits if set; otherwise dispatched through the guest's IDT. */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_EXT_INT_EXIT            RT_BIT(0)
/** Non-maskable interrupts cause VM exits if set; otherwise dispatched through the guest's IDT. */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_NMI_EXIT                RT_BIT(3)
/** Virtual NMIs. */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_VIRTUAL_NMI             RT_BIT(5)
/** Activate VMX preemption timer. */
#define VMX_VMCS_CTRL_PIN_EXEC_CONTROLS_PREEMPT_TIMER           RT_BIT(6)
/* All other bits are reserved and must be set according to MSR IA32_VMX_PROCBASED_CTLS. */
/** @} */

/** @name VMX_VMCS_CTRL_PROC_EXEC_CONTROLS
 * @{
 */
/** VM Exit as soon as RFLAGS.IF=1 and no blocking is active. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_IRQ_WINDOW_EXIT        RT_BIT(2)
/** Use timestamp counter offset. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_TSC_OFFSET             RT_BIT(3)
/** VM Exit when executing the HLT instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_HLT_EXIT               RT_BIT(7)
/** VM Exit when executing the INVLPG instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_INVLPG_EXIT            RT_BIT(9)
/** VM Exit when executing the MWAIT instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MWAIT_EXIT             RT_BIT(10)
/** VM Exit when executing the RDPMC instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDPMC_EXIT             RT_BIT(11)
/** VM Exit when executing the RDTSC/RDTSCP instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_RDTSC_EXIT             RT_BIT(12)
/** VM Exit when executing the MOV to CR3 instruction. (forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_LOAD_EXIT          RT_BIT(15)
/** VM Exit when executing the MOV from CR3 instruction. (forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR3_STORE_EXIT         RT_BIT(16)
/** VM Exit on CR8 loads. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_LOAD_EXIT          RT_BIT(19)
/** VM Exit on CR8 stores. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_CR8_STORE_EXIT         RT_BIT(20)
/** Use TPR shadow. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_TPR_SHADOW         RT_BIT(21)
/** VM Exit when virtual nmi blocking is disabled. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_NMI_WINDOW_EXIT        RT_BIT(22)
/** VM Exit when executing a MOV DRx instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MOV_DR_EXIT            RT_BIT(23)
/** VM Exit when executing IO instructions. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_UNCOND_IO_EXIT         RT_BIT(24)
/** Use IO bitmaps. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_IO_BITMAPS         RT_BIT(25)
/** Monitor trap flag. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_TRAP_FLAG      RT_BIT(27)
/** Use MSR bitmaps. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_USE_MSR_BITMAPS        RT_BIT(28)
/** VM Exit when executing the MONITOR instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_MONITOR_EXIT           RT_BIT(29)
/** VM Exit when executing the PAUSE instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC_CONTROLS_PAUSE_EXIT             RT_BIT(30)
/** Determines whether the secondary processor based VM-execution controls are used. */
#define VMX_VMCS_CTRL_PROC_EXEC_USE_SECONDARY_EXEC_CTRL         RT_BIT(31)
/** @} */

/** @name VMX_VMCS_CTRL_PROC_EXEC_CONTROLS2
 * @{
 */
/** Virtualize APIC access. */
#define VMX_VMCS_CTRL_PROC_EXEC2_VIRT_APIC                      RT_BIT(0)
/** EPT supported/enabled. */
#define VMX_VMCS_CTRL_PROC_EXEC2_EPT                            RT_BIT(1)
/** Descriptor table instructions cause VM-exits. */
#define VMX_VMCS_CTRL_PROC_EXEC2_DESCRIPTOR_INSTR_EXIT          RT_BIT(2)
/** RDTSCP supported/enabled. */
#define VMX_VMCS_CTRL_PROC_EXEC2_RDTSCP                         RT_BIT(3)
/** Virtualize x2APIC mode. */
#define VMX_VMCS_CTRL_PROC_EXEC2_X2APIC                         RT_BIT(4)
/** VPID supported/enabled. */
#define VMX_VMCS_CTRL_PROC_EXEC2_VPID                           RT_BIT(5)
/** VM Exit when executing the WBINVD instruction. */
#define VMX_VMCS_CTRL_PROC_EXEC2_WBINVD_EXIT                    RT_BIT(6)
/** Unrestricted guest execution. */
#define VMX_VMCS_CTRL_PROC_EXEC2_REAL_MODE                      RT_BIT(7)
/** A specified nr of pause loops cause a VM-exit. */
#define VMX_VMCS_CTRL_PROC_EXEC2_PAUSE_LOOP_EXIT                RT_BIT(10)
/** VM Exit when executing RDRAND instructions. */
#define VMX_VMCS_CTRL_PROC_EXEC2_RDRAND_EXIT                    RT_BIT(11)
/** Enables INVPCID instructions. */
#define VMX_VMCS_CTRL_PROC_EXEC2_INVPCID                        RT_BIT(12)
/** Enables VMFUNC instructions. */
#define VMX_VMCS_CTRL_PROC_EXEC2_VMFUNC                         RT_BIT(13)
/** @} */


/** @name VMX_VMCS_CTRL_ENTRY_CONTROLS
 * @{
 */
/** Load guest debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_DEBUG                 RT_BIT(2)
/** 64 bits guest mode. Must be 0 for CPUs that don't support AMD64. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_IA32E_MODE_GUEST           RT_BIT(9)
/** In SMM mode after VM-entry. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_ENTRY_SMM                  RT_BIT(10)
/** Disable dual treatment of SMI and SMM; must be zero for VM-entry outside of SMM. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_DEACTIVATE_DUALMON         RT_BIT(11)
/** This control determines whether the guest IA32_PERF_GLOBAL_CTRL MSR is loaded on VM entry. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PERF_MSR        RT_BIT(13)
/** This control determines whether the guest IA32_PAT MSR is loaded on VM entry. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_PAT_MSR         RT_BIT(14)
/** This control determines whether the guest IA32_EFER MSR is loaded on VM entry. */
#define VMX_VMCS_CTRL_ENTRY_CONTROLS_LOAD_GUEST_EFER_MSR        RT_BIT(15)
/** @} */


/** @name VMX_VMCS_CTRL_EXIT_CONTROLS
 * @{
 */
/** Save guest debug controls (dr7 & IA32_DEBUGCTL_MSR) (forced to 1 on the 'first' VT-x capable CPUs; this actually includes the newest Nehalem CPUs) */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_DEBUG                  RT_BIT(2)
/** Return to long mode after a VM-exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_HOST_ADDR_SPACE_SIZE        RT_BIT(9)
/** This control determines whether the IA32_PERF_GLOBAL_CTRL MSR is loaded on VM exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_PERF_MSR               RT_BIT(12)
/** Acknowledge external interrupts with the irq controller if one caused a VM-exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_ACK_EXTERNAL_IRQ            RT_BIT(15)
/** This control determines whether the guest IA32_PAT MSR is saved on VM exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_PAT_MSR          RT_BIT(18)
/** This control determines whether the host IA32_PAT MSR is loaded on VM exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_PAT_MSR           RT_BIT(19)
/** This control determines whether the guest IA32_EFER MSR is saved on VM exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_GUEST_EFER_MSR         RT_BIT(20)
/** This control determines whether the host IA32_EFER MSR is loaded on VM exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_LOAD_HOST_EFER_MSR          RT_BIT(21)
/** This control determines whether the value of the VMX preemption timer is saved on VM exit. */
#define VMX_VMCS_CTRL_EXIT_CONTROLS_SAVE_VMX_PREEMPT_TIMER      RT_BIT(22)
/** @} */

/**  @name VMCS field encoding - 32 Bits read-only fields
 * @{
 */
#define VMX_VMCS32_RO_VM_INSTR_ERROR                              0x4400
#define VMX_VMCS32_RO_EXIT_REASON                                 0x4402
#define VMX_VMCS32_RO_EXIT_INTERRUPTION_INFO                      0x4404
#define VMX_VMCS32_RO_EXIT_INTERRUPTION_ERRCODE                   0x4406
#define VMX_VMCS32_RO_IDT_INFO                                    0x4408
#define VMX_VMCS32_RO_IDT_ERRCODE                                 0x440A
#define VMX_VMCS32_RO_EXIT_INSTR_LENGTH                           0x440C
#define VMX_VMCS32_RO_EXIT_INSTR_INFO                             0x440E
/** @} */

/** @name VMX_VMCS_RO_EXIT_INTERRUPTION_INFO
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
/** Construct an irq event injection value from the exit interruption info value (same except that bit 12 is reserved). */
#define VMX_VMCS_CTRL_ENTRY_IRQ_INFO_FROM_EXIT_INT_INFO(a)      (a & ~RT_BIT(12))
/** @} */

/** @name VMX_VMCS_RO_EXIT_INTERRUPTION_INFO_TYPE
 * @{
 */
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_EXT             0
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_NMI             2
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_HWEXCPT         3
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_SW              4 /**< int xx */
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_DBEXCPT         5 /**< Why are we getting this one?? */
#define VMX_EXIT_INTERRUPTION_INFO_TYPE_SWEXCPT         6
/** @} */


/**  @name VMCS field encoding - 32 Bits guest state fields
 * @{
 */
#define VMX_VMCS32_GUEST_ES_LIMIT                                 0x4800
#define VMX_VMCS32_GUEST_CS_LIMIT                                 0x4802
#define VMX_VMCS32_GUEST_SS_LIMIT                                 0x4804
#define VMX_VMCS32_GUEST_DS_LIMIT                                 0x4806
#define VMX_VMCS32_GUEST_FS_LIMIT                                 0x4808
#define VMX_VMCS32_GUEST_GS_LIMIT                                 0x480A
#define VMX_VMCS32_GUEST_LDTR_LIMIT                               0x480C
#define VMX_VMCS32_GUEST_TR_LIMIT                                 0x480E
#define VMX_VMCS32_GUEST_GDTR_LIMIT                               0x4810
#define VMX_VMCS32_GUEST_IDTR_LIMIT                               0x4812
#define VMX_VMCS32_GUEST_ES_ACCESS_RIGHTS                         0x4814
#define VMX_VMCS32_GUEST_CS_ACCESS_RIGHTS                         0x4816
#define VMX_VMCS32_GUEST_SS_ACCESS_RIGHTS                         0x4818
#define VMX_VMCS32_GUEST_DS_ACCESS_RIGHTS                         0x481A
#define VMX_VMCS32_GUEST_FS_ACCESS_RIGHTS                         0x481C
#define VMX_VMCS32_GUEST_GS_ACCESS_RIGHTS                         0x481E
#define VMX_VMCS32_GUEST_LDTR_ACCESS_RIGHTS                       0x4820
#define VMX_VMCS32_GUEST_TR_ACCESS_RIGHTS                         0x4822
#define VMX_VMCS32_GUEST_INTERRUPTIBILITY_STATE                   0x4824
#define VMX_VMCS32_GUEST_ACTIVITY_STATE                           0x4826
#define VMX_VMCS32_GUEST_SYSENTER_CS                              0x482A  /**< MSR IA32_SYSENTER_CS */
#define VMX_VMCS32_GUEST_PREEMPTION_TIMER_VALUE                   0x482E
/** @} */


/** @name VMX_VMCS_GUEST_ACTIVITY_STATE
 * @{
 */
/** The logical processor is active. */
#define VMX_CMS_GUEST_ACTIVITY_ACTIVE                           0x0
/** The logical processor is inactive, because executed a HLT instruction. */
#define VMX_CMS_GUEST_ACTIVITY_HLT                              0x1
/** The logical processor is inactive, because of a triple fault or other serious error. */
#define VMX_CMS_GUEST_ACTIVITY_SHUTDOWN                         0x2
/** The logical processor is inactive, because it's waiting for a startup-IPI */
#define VMX_CMS_GUEST_ACTIVITY_SIPI_WAIT                        0x3
/** @} */


/** @name VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE
 * @{
 */
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_STI         RT_BIT(0)
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_MOVSS       RT_BIT(1)
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_SMI         RT_BIT(2)
#define VMX_VMCS_GUEST_INTERRUPTIBILITY_STATE_BLOCK_NMI         RT_BIT(3)
/** @} */


/** @name VMCS field encoding - 32 Bits host state fields
 * @{
 */
#define VMX_VMCS32_HOST_SYSENTER_CS                             0x4C00
/** @} */

/** @name Natural width control fields
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


/** @name Natural width read-only data fields
 * @{
 */
#define VMX_VMCS_RO_EXIT_QUALIFICATION                          0x6400
#define VMX_VMCS_RO_IO_RCX                                      0x6402
#define VMX_VMCS_RO_IO_RSX                                      0x6404
#define VMX_VMCS_RO_IO_RDI                                      0x6406
#define VMX_VMCS_RO_IO_RIP                                      0x6408
#define VMX_VMCS_EXIT_GUEST_LINEAR_ADDR                         0x640A
/** @} */


/** @name VMX_VMCS_RO_EXIT_QUALIFICATION
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
/** @} */

/** @name VMX_EXIT_QUALIFICATION_DRX_DIRECTION values
 * @{
 */
#define VMX_EXIT_QUALIFICATION_DRX_DIRECTION_WRITE     0
#define VMX_EXIT_QUALIFICATION_DRX_DIRECTION_READ      1
/** @} */



/** @name CRx accesses
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
/** @} */

/** @name VMX_EXIT_QUALIFICATION_CRX_ACCESS
 * @{
 */
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_WRITE        0
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_READ         1
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_CLTS         2
#define VMX_EXIT_QUALIFICATION_CRX_ACCESS_LMSW         3
/** @} */

/** @name VMX_EXIT_QUALIFICATION_TASK_SWITCH
 * @{
 */
#define VMX_EXIT_QUALIFICATION_TASK_SWITCH_SELECTOR(a)  (a & 0xffff)
#define VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE(a)      ((a >> 30)& 0x3)
/** Task switch caused by a call instruction. */
#define VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE_CALL    0
/** Task switch caused by an iret instruction. */
#define VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE_IRET    1
/** Task switch caused by a jmp instruction. */
#define VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE_JMP     2
/** Task switch caused by an interrupt gate. */
#define VMX_EXIT_QUALIFICATION_TASK_SWITCH_TYPE_IDT     3

/** @} */


/** @name VMX_EXIT_EPT_VIOLATION
 * @{
 */
/** Set if the violation was caused by a data read. */
#define VMX_EXIT_QUALIFICATION_EPT_DATA_READ            RT_BIT(0)
/** Set if the violation was caused by a data write. */
#define VMX_EXIT_QUALIFICATION_EPT_DATA_WRITE           RT_BIT(1)
/** Set if the violation was caused by an insruction fetch. */
#define VMX_EXIT_QUALIFICATION_EPT_INSTR_FETCH          RT_BIT(2)
/** AND of the present bit of all EPT structures. */
#define VMX_EXIT_QUALIFICATION_EPT_ENTRY_PRESENT        RT_BIT(3)
/** AND of the write bit of all EPT structures. */
#define VMX_EXIT_QUALIFICATION_EPT_ENTRY_WRITE          RT_BIT(4)
/** AND of the execute bit of all EPT structures. */
#define VMX_EXIT_QUALIFICATION_EPT_ENTRY_EXECUTE        RT_BIT(5)
/** Set if the guest linear address field contains the faulting address. */
#define VMX_EXIT_QUALIFICATION_EPT_GUEST_ADDR_VALID     RT_BIT(7)
/** If bit 7 is one: (reserved otherwise)
 *  1 - violation due to physical address access.
 *  0 - violation caused by page walk or access/dirty bit updates
 */
#define VMX_EXIT_QUALIFICATION_EPT_TRANSLATED_ACCESS    RT_BIT(8)
/** @} */


/** @name VMX_EXIT_PORT_IO
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

/** @name VMX_EXIT_QUALIFICATION_IO_DIRECTION
 * @{
 */
#define VMX_EXIT_QUALIFICATION_IO_DIRECTION_OUT        0
#define VMX_EXIT_QUALIFICATION_IO_DIRECTION_IN         1
/** @} */


/** @name VMX_EXIT_QUALIFICATION_IO_ENCODING
 * @{
 */
#define VMX_EXIT_QUALIFICATION_IO_ENCODING_DX          0
#define VMX_EXIT_QUALIFICATION_IO_ENCODING_IMM         1
/** @} */

/** @name VMX_EXIT_APIC_ACCESS
 * @{
 */
/** 0-11:   If the APIC-access VM exit is due to a linear access, the offset of access within the APIC page. */
#define VMX_EXIT_QUALIFICATION_APIC_ACCESS_OFFSET(a)    (a & 0xfff)
/** 12-15:  Access type. */
#define VMX_EXIT_QUALIFICATION_APIC_ACCESS_TYPE(a)      ((a >> 12) & 0xf)
/* Rest reserved. */
/** @} */


/** @name VMX_EXIT_QUALIFICATION_APIC_ACCESS_TYPE; access types
 * @{
 */
/** Linear read access. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_READ                0
/** Linear write access. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_WRITE               1
/** Linear instruction fetch access. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_INSTR_FETCH         2
/** Linear read/write access during event delivery. */
#define VMX_APIC_ACCESS_TYPE_LINEAR_EVENT_DELIVERY      3
/** Physical read/write access during event delivery. */
#define VMX_APIC_ACCESS_TYPE_PHYSICAL_EVENT_DELIVERY    10
/** Physical access for an instruction fetch or during instruction execution. */
#define VMX_APIC_ACCESS_TYPE_PHYSICAL_INSTR             15
/** @} */

/** @} */

/** @name VMCS field encoding - Natural width guest state fields
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
#define VMX_VMCS_GUEST_SYSENTER_ESP                             0x6824  /**< MSR IA32_SYSENTER_ESP */
#define VMX_VMCS_GUEST_SYSENTER_EIP                             0x6826  /**< MSR IA32_SYSENTER_EIP */
/** @} */


/** @name VMX_VMCS_GUEST_DEBUG_EXCEPTIONS
 * @{
 */
/** Hardware breakpoint 0 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B0                      RT_BIT(0)
/** Hardware breakpoint 1 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B1                      RT_BIT(1)
/** Hardware breakpoint 2 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B2                      RT_BIT(2)
/** Hardware breakpoint 3 was met. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_B3                      RT_BIT(3)
/** At least one data or IO breakpoint was hit. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_BREAKPOINT_ENABLED      RT_BIT(12)
/** A debug exception would have been triggered by single-step execution mode. */
#define VMX_VMCS_GUEST_DEBUG_EXCEPTIONS_BS                      RT_BIT(14)
/** Bits 4-11, 13 and 15-63 are reserved. */

/** @} */

/** @name VMCS field encoding - Natural width host state fields
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
# define __STR(x)   #x
# define STR(x)     __STR(x)
#endif


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
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
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
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
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
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
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
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
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
 * Executes VMPTRST
 *
 * @returns VBox status code
 * @param pVMCS    Address that will receive the current pointer
 */
DECLASM(int) VMXGetActivateVMCS(RTHCPHYS *pVMCS);

/**
 * Executes VMWRITE
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   u32Val          32 bits value
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
DECLASM(int) VMXWriteVmcs32(uint32_t idxField, uint32_t u32Val);
#else
DECLINLINE(int) VMXWriteVmcs32(uint32_t idxField, uint32_t u32Val)
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

/**
 * Executes VMWRITE
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   u64Val          16, 32 or 64 bits value
 */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
DECLASM(int) VMXWriteVmcs64(uint32_t idxField, uint64_t u64Val);
#else
VMMR0DECL(int) VMXWriteVmcs64Ex(PVMCPU pVCpu, uint32_t idxField, uint64_t u64Val);

#define VMXWriteVmcs64(idxField, u64Val)    VMXWriteVmcs64Ex(pVCpu, idxField, u64Val)
#endif

#if HC_ARCH_BITS == 64
#define VMXWriteVmcs VMXWriteVmcs64
#else
#define VMXWriteVmcs VMXWriteVmcs32
#endif /* HC_ARCH_BITS == 64 */


/**
 * Invalidate a page using invept
 * @returns VBox status code
 * @param   enmFlush    Type of flush
 * @param   pDescriptor Descriptor
 */
DECLASM(int) VMXR0InvEPT(VMX_FLUSH_EPT enmFlush, uint64_t *pDescriptor);

/**
 * Invalidate a page using invvpid
 * @returns VBox status code
 * @param   enmFlush    Type of flush
 * @param   pDescriptor Descriptor
 */
DECLASM(int) VMXR0InvVPID(VMX_FLUSH_VPID enmFlush, uint64_t *pDescriptor);

/**
 * Executes VMREAD
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   pData           Ptr to store VM field value
 */
#if RT_INLINE_ASM_EXTERNAL || HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
DECLASM(int) VMXReadVmcs32(uint32_t idxField, uint32_t *pData);
#else
DECLINLINE(int) VMXReadVmcsS32(uint32_t idxField, uint32_t *pData)
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

/**
 * Executes VMREAD
 *
 * @returns VBox status code
 * @param   idxField        VMCS index
 * @param   pData           Ptr to store VM field value
 */
#if HC_ARCH_BITS == 64 || defined(VBOX_WITH_HYBRID_32BIT_KERNEL)
DECLASM(int) VMXReadVmcs64(uint32_t idxField, uint64_t *pData);
#else
DECLINLINE(int) VMXReadVmcs64(uint32_t idxField, uint64_t *pData)
{
    int rc;

    uint32_t val_hi, val;
    rc  = VMXReadVmcs32(idxField, &val);
    rc |= VMXReadVmcs32(idxField + 1, &val_hi);
    AssertRC(rc);
    *pData = RT_MAKE_U64(val, val_hi);
    return rc;
}
#endif

#if HC_ARCH_BITS == 64
# define VMXReadVmcs VMXReadVmcs64
#else
# define VMXReadVmcs VMXReadVmcs32
#endif /* HC_ARCH_BITS == 64 */

/**
 * Gets the last instruction error value from the current VMCS
 *
 * @returns error value
 */
DECLINLINE(uint32_t) VMXGetLastError(void)
{
#if HC_ARCH_BITS == 64
    uint64_t uLastError = 0;
    int rc = VMXReadVmcs(VMX_VMCS32_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return (uint32_t)uLastError;

#else /* 32-bit host: */
    uint32_t uLastError = 0;
    int rc = VMXReadVmcs32(VMX_VMCS32_RO_VM_INSTR_ERROR, &uLastError);
    AssertRC(rc);
    return uLastError;
#endif
}

#ifdef IN_RING0
VMMR0DECL(int) VMXR0InvalidatePage(PVM pVM, PVMCPU pVCpu, RTGCPTR GCVirt);
VMMR0DECL(int) VMXR0InvalidatePhysPage(PVM pVM, PVMCPU pVCpu, RTGCPHYS GCPhys);
#endif /* IN_RING0 */

/** @} */

#endif

