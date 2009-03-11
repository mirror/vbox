/** @file
 * X86 (and AMD64) Structures and Definitions.
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
 * The contents of this file may alternatively be used under the terms
 * of the Common Development and Distribution License Version 1.0
 * (CDDL) only, as it comes in the "COPYING.CDDL" file of the
 * VirtualBox OSE distribution, in which case the provisions of the
 * CDDL are applicable instead of those of the GPL.
 *
 * You may elect to license modified versions of this file under the
 * terms and conditions of either the GPL or the CDDL or both.
 *
 * Please contact Sun Microsystems, Inc., 4150 Network Circle, Santa
 * Clara, CA 95054 USA or visit http://www.sun.com if you need
 * additional information or have any questions.
 */

/*
 * x86.mac is generated from this file using:
 *      sed -e '/__VBox_x86_h__/d' -e '/#define/!d' -e 's/#define/%define/' include/VBox/x86.h
 */

#ifndef ___VBox_x86_h
#define ___VBox_x86_h

#include <VBox/types.h>
#include <iprt/assert.h>

/* Workaround for Solaris sys/regset.h defining CS, DS */
#if defined(RT_OS_SOLARIS)
# undef CS
# undef DS
#endif

/** @defgroup grp_x86   x86 Types and Definitions
 * @{
 */

/**
 * EFLAGS Bits.
 */
typedef struct X86EFLAGSBITS
{
    /** Bit 0 - CF - Carry flag - Status flag. */
    unsigned    u1CF : 1;
    /** Bit 1 -  1 - Reserved flag. */
    unsigned    u1Reserved0 : 1;
    /** Bit 2 - PF - Parity flag - Status flag. */
    unsigned    u1PF : 1;
    /** Bit 3 -  0 - Reserved flag. */
    unsigned    u1Reserved1 : 1;
    /** Bit 4 - AF - Auxiliary carry flag - Status flag. */
    unsigned    u1AF : 1;
    /** Bit 5 -  0 - Reserved flag. */
    unsigned    u1Reserved2 : 1;
    /** Bit 6 - ZF - Zero flag - Status flag. */
    unsigned    u1ZF : 1;
    /** Bit 7 - SF - Signed flag - Status flag. */
    unsigned    u1SF : 1;
    /** Bit 8 - TF - Trap flag - System flag. */
    unsigned    u1TF : 1;
    /** Bit 9 - IF - Interrupt flag - System flag. */
    unsigned    u1IF : 1;
    /** Bit 10 - DF - Direction flag - Control flag. */
    unsigned    u1DF : 1;
    /** Bit 11 - OF - Overflow flag - Status flag. */
    unsigned    u1OF : 1;
    /** Bit 12-13 - IOPL - I/O prvilege level flag - System flag. */
    unsigned    u2IOPL : 2;
    /** Bit 14 - NT - Nested task flag - System flag. */
    unsigned    u1NT : 1;
    /** Bit 15 -  0 - Reserved flag. */
    unsigned    u1Reserved3 : 1;
    /** Bit 16 - RF - Resume flag - System flag. */
    unsigned    u1RF : 1;
    /** Bit 17 - VM - Virtual 8086 mode - System flag. */
    unsigned    u1VM : 1;
    /** Bit 18 - AC - Alignment check flag - System flag. Works with CR0.AM. */
    unsigned    u1AC : 1;
    /** Bit 19 - VIF - Virtual interupt flag - System flag. */
    unsigned    u1VIF : 1;
    /** Bit 20 - VIP - Virtual interupt pending flag - System flag. */
    unsigned    u1VIP : 1;
    /** Bit 21 - ID - CPUID flag - System flag. If this responds to flipping CPUID is supported. */
    unsigned    u1ID : 1;
    /** Bit 22-31 - 0 - Reserved flag. */
    unsigned    u10Reserved4 : 10;
} X86EFLAGSBITS;
/** Pointer to EFLAGS bits. */
typedef X86EFLAGSBITS *PX86EFLAGSBITS;
/** Pointer to const EFLAGS bits. */
typedef const X86EFLAGSBITS *PCX86EFLAGSBITS;

/**
 * EFLAGS.
 */
typedef union X86EFLAGS
{
    /** The plain unsigned view. */
    uint32_t        u;
    /** The bitfield view. */
    X86EFLAGSBITS   Bits;
    /** The 8-bit view. */
    uint8_t         au8[4];
    /** The 16-bit view. */
    uint16_t        au16[2];
    /** The 32-bit view. */
    uint32_t        au32[1];
    /** The 32-bit view. */
    uint32_t        u32;
} X86EFLAGS;
/** Pointer to EFLAGS. */
typedef X86EFLAGS *PX86EFLAGS;
/** Pointer to const EFLAGS. */
typedef const X86EFLAGS *PCX86EFLAGS;

/**
 * RFLAGS (32 upper bits are reserved).
 */
typedef union X86RFLAGS
{
    /** The plain unsigned view. */
    uint64_t        u;
    /** The bitfield view. */
    X86EFLAGSBITS   Bits;
    /** The 8-bit view. */
    uint8_t         au8[8];
    /** The 16-bit view. */
    uint16_t        au16[4];
    /** The 32-bit view. */
    uint32_t        au32[2];
    /** The 64-bit view. */
    uint64_t        au64[1];
    /** The 64-bit view. */
    uint64_t        u64;
} X86RFLAGS;
/** Pointer to RFLAGS. */
typedef X86RFLAGS *PX86RFLAGS;
/** Pointer to const RFLAGS. */
typedef const X86RFLAGS *PCX86RFLAGS;


/** @name EFLAGS
 * @{
 */
/** Bit 0 - CF - Carry flag - Status flag. */
#define X86_EFL_CF          RT_BIT(0)
/** Bit 2 - PF - Parity flag - Status flag. */
#define X86_EFL_PF          RT_BIT(2)
/** Bit 4 - AF - Auxiliary carry flag - Status flag. */
#define X86_EFL_AF          RT_BIT(4)
/** Bit 6 - ZF - Zero flag - Status flag. */
#define X86_EFL_ZF          RT_BIT(6)
/** Bit 7 - SF - Signed flag - Status flag. */
#define X86_EFL_SF          RT_BIT(7)
/** Bit 8 - TF - Trap flag - System flag. */
#define X86_EFL_TF          RT_BIT(8)
/** Bit 9 - IF - Interrupt flag - System flag. */
#define X86_EFL_IF          RT_BIT(9)
/** Bit 10 - DF - Direction flag - Control flag. */
#define X86_EFL_DF          RT_BIT(10)
/** Bit 11 - OF - Overflow flag - Status flag. */
#define X86_EFL_OF          RT_BIT(11)
/** Bit 12-13 - IOPL - I/O prvilege level flag - System flag. */
#define X86_EFL_IOPL        (RT_BIT(12) | RT_BIT(13))
/** Bit 14 - NT - Nested task flag - System flag. */
#define X86_EFL_NT          RT_BIT(14)
/** Bit 16 - RF - Resume flag - System flag. */
#define X86_EFL_RF          RT_BIT(16)
/** Bit 17 - VM - Virtual 8086 mode - System flag. */
#define X86_EFL_VM          RT_BIT(17)
/** Bit 18 - AC - Alignment check flag - System flag. Works with CR0.AM. */
#define X86_EFL_AC          RT_BIT(18)
/** Bit 19 - VIF - Virtual interupt flag - System flag. */
#define X86_EFL_VIF         RT_BIT(19)
/** Bit 20 - VIP - Virtual interupt pending flag - System flag. */
#define X86_EFL_VIP         RT_BIT(20)
/** Bit 21 - ID - CPUID flag - System flag. If this responds to flipping CPUID is supported. */
#define X86_EFL_ID          RT_BIT(21)
/** IOPL shift. */
#define X86_EFL_IOPL_SHIFT  12
/** The the IOPL level from the flags. */
#define X86_EFL_GET_IOPL(efl)   (((efl) >> X86_EFL_IOPL_SHIFT) & 3)
/** Bits restored by popf */
#define X86_EFL_POPF_BITS       (X86_EFL_CF | X86_EFL_PF | X86_EFL_AF | X86_EFL_ZF | X86_EFL_SF | X86_EFL_TF | X86_EFL_IF | X86_EFL_DF | X86_EFL_OF | X86_EFL_IOPL | X86_EFL_NT | X86_EFL_AC | X86_EFL_ID)
/** @} */


/** CPUID Feature information - ECX.
 * CPUID query with EAX=1.
 */
typedef struct X86CPUIDFEATECX
{
    /** Bit 0 - SSE3 - Supports SSE3 or not. */
    unsigned    u1SSE3 : 1;
    /** Reserved. */
    unsigned    u2Reserved1 : 2;
    /** Bit 3 - MONITOR - Supports MONITOR/MWAIT. */
    unsigned    u1Monitor : 1;
    /** Bit 4 - CPL-DS - CPL Qualified Debug Store. */
    unsigned    u1CPLDS : 1;
    /** Bit 5 - VMX - Virtual Machine Technology. */
    unsigned    u1VMX : 1;
    /** Reserved. */
    unsigned    u1Reserved2 : 1;
    /** Bit 7 - EST - Enh. SpeedStep Tech. */
    unsigned    u1EST : 1;
    /** Bit 8 - TM2 - Terminal Monitor 2. */
    unsigned    u1TM2 : 1;
    /** Bit 9 - SSSE3 - Supplemental Streaming SIMD Extensions 3. */
    unsigned    u1SSSE3 : 1;
    /** Bit 10 - CNTX-ID - L1 Context ID. */
    unsigned    u1CNTXID : 1;
    /** Reserved. */
    unsigned    u2Reserved4 : 2;
    /** Bit 13 - CX16 - CMPXCHG16B. */
    unsigned    u1CX16 : 1;
    /** Bit 14 - xTPR Update Control. Processor supports changing IA32_MISC_ENABLES[bit 23]. */
    unsigned    u1TPRUpdate : 1;
    /** Reserved. */
    unsigned    u17Reserved5 : 17;

} X86CPUIDFEATECX;
/** Pointer to CPUID Feature Information - ECX. */
typedef X86CPUIDFEATECX *PX86CPUIDFEATECX;
/** Pointer to const CPUID Feature Information - ECX. */
typedef const X86CPUIDFEATECX *PCX86CPUIDFEATECX;


/** CPUID Feature Information - EDX.
 * CPUID query with EAX=1.
 */
typedef struct X86CPUIDFEATEDX
{
    /** Bit 0 - FPU - x87 FPU on Chip. */
    unsigned    u1FPU : 1;
    /** Bit 1 - VME - Virtual 8086 Mode Enhancements. */
    unsigned    u1VME : 1;
    /** Bit 2 - DE - Debugging extensions. */
    unsigned    u1DE : 1;
    /** Bit 3 - PSE - Page Size Extension. */
    unsigned    u1PSE : 1;
    /** Bit 4 - TSC - Time Stamp Counter. */
    unsigned    u1TSC : 1;
    /** Bit 5 - MSR - Model Specific Registers RDMSR and WRMSR Instructions. */
    unsigned    u1MSR : 1;
    /** Bit 6 - PAE - Physical Address Extension. */
    unsigned    u1PAE : 1;
    /** Bit 7 - MCE - Machine Check Exception. */
    unsigned    u1MCE : 1;
    /** Bit 8 - CX8 - CMPXCHG8B instruction. */
    unsigned    u1CX8 : 1;
    /** Bit 9 - APIC - APIC On-Chip. */
    unsigned    u1APIC : 1;
    /** Bit 10 - Reserved. */
    unsigned    u1Reserved1 : 1;
    /** Bit 11 - SEP - SYSENTER and SYSEXIT. */
    unsigned    u1SEP : 1;
    /** Bit 12 - MTRR - Memory Type Range Registers. */
    unsigned    u1MTRR : 1;
    /** Bit 13 - PGE - PTE Global Bit. */
    unsigned    u1PGE : 1;
    /** Bit 14 - MCA - Machine Check Architecture. */
    unsigned    u1MCA : 1;
    /** Bit 15 - CMOV - Conditional Move Instructions. */
    unsigned    u1CMOV : 1;
    /** Bit 16 - PAT - Page Attribute Table. */
    unsigned    u1PAT : 1;
    /** Bit 17 - PSE-36 - 36-bit Page Size Extention. */
    unsigned    u1PSE36 : 1;
    /** Bit 18 - PSN - Processor Serial Number. */
    unsigned    u1PSN : 1;
    /** Bit 19 - CLFSH - CLFLUSH Instruction. */
    unsigned    u1CLFSH : 1;
    /** Bit 20 - Reserved. */
    unsigned    u1Reserved2 : 1;
    /** Bit 21 - DS - Debug Store. */
    unsigned    u1DS : 1;
    /** Bit 22 - ACPI - Thermal Monitor and Software Controlled Clock Facilities. */
    unsigned    u1ACPI : 1;
    /** Bit 23 - MMX - Intel MMX 'Technology'. */
    unsigned    u1MMX : 1;
    /** Bit 24 - FXSR - FXSAVE and FXRSTOR Instructions. */
    unsigned    u1FXSR : 1;
    /** Bit 25 - SSE - SSE Support. */
    unsigned    u1SSE : 1;
    /** Bit 26 - SSE2 - SSE2 Support. */
    unsigned    u1SSE2 : 1;
    /** Bit 27 - SS - Self Snoop. */
    unsigned    u1SS : 1;
    /** Bit 28 - HTT - Hyper-Threading Technology. */
    unsigned    u1HTT : 1;
    /** Bit 29 - TM - Thermal Monitor. */
    unsigned    u1TM : 1;
    /** Bit 30 - Reserved - . */
    unsigned    u1Reserved3 : 1;
    /** Bit 31 - PBE - Pending Break Enabled. */
    unsigned    u1PBE : 1;
} X86CPUIDFEATEDX;
/** Pointer to CPUID Feature Information - EDX. */
typedef X86CPUIDFEATEDX *PX86CPUIDFEATEDX;
/** Pointer to const CPUID Feature Information - EDX. */
typedef const X86CPUIDFEATEDX *PCX86CPUIDFEATEDX;

/** @name CPUID Vendor information.
 * CPUID query with EAX=0.
 * @{
 */
#define X86_CPUID_VENDOR_INTEL_EBX      0x756e6547      /* Genu */
#define X86_CPUID_VENDOR_INTEL_ECX      0x6c65746e      /* ntel */
#define X86_CPUID_VENDOR_INTEL_EDX      0x49656e69      /* ineI */

#define X86_CPUID_VENDOR_AMD_EBX        0x68747541      /* Auth */
#define X86_CPUID_VENDOR_AMD_ECX        0x444d4163      /* cAMD */
#define X86_CPUID_VENDOR_AMD_EDX        0x69746e65      /* enti */
/** @} */


/** @name CPUID Feature information.
 * CPUID query with EAX=1.
 * @{
 */
/** ECX Bit 0 - SSE3 - Supports SSE3 or not. */
#define X86_CPUID_FEATURE_ECX_SSE3      RT_BIT(0)
/** ECX Bit 3 - MONITOR - Supports MONITOR/MWAIT. */
#define X86_CPUID_FEATURE_ECX_MONITOR   RT_BIT(3)
/** ECX Bit 4 - CPL-DS - CPL Qualified Debug Store. */
#define X86_CPUID_FEATURE_ECX_CPLDS     RT_BIT(4)
/** ECX Bit 5 - VMX - Virtual Machine Technology. */
#define X86_CPUID_FEATURE_ECX_VMX       RT_BIT(5)
/** ECX Bit 7 - EST - Enh. SpeedStep Tech. */
#define X86_CPUID_FEATURE_ECX_EST       RT_BIT(7)
/** ECX Bit 8 - TM2 - Terminal Monitor 2. */
#define X86_CPUID_FEATURE_ECX_TM2       RT_BIT(8)
/** ECX Bit 9 - SSSE3 - Supplemental Streaming SIMD Extensions 3. */
#define X86_CPUID_FEATURE_ECX_SSSE3     RT_BIT(9)
/** ECX Bit 10 - CNTX-ID - L1 Context ID. */
#define X86_CPUID_FEATURE_ECX_CNTXID    RT_BIT(10)
/** ECX Bit 13 - CX16 - CMPXCHG16B. */
#define X86_CPUID_FEATURE_ECX_CX16      RT_BIT(13)
/** ECX Bit 14 - xTPR Update Control. Processor supports changing IA32_MISC_ENABLES[bit 23]. */
#define X86_CPUID_FEATURE_ECX_TPRUPDATE RT_BIT(14)
/** ECX Bit 21 - x2APIC support. */
#define X86_CPUID_FEATURE_ECX_X2APIC    RT_BIT(21)
/** ECX Bit 23 - POPCOUNT instruction. */
#define X86_CPUID_FEATURE_ECX_POPCOUNT  RT_BIT(23)


/** Bit 0 - FPU - x87 FPU on Chip. */
#define X86_CPUID_FEATURE_EDX_FPU       RT_BIT(0)
/** Bit 1 - VME - Virtual 8086 Mode Enhancements. */
#define X86_CPUID_FEATURE_EDX_VME       RT_BIT(1)
/** Bit 2 - DE - Debugging extensions. */
#define X86_CPUID_FEATURE_EDX_DE        RT_BIT(2)
/** Bit 3 - PSE - Page Size Extension. */
#define X86_CPUID_FEATURE_EDX_PSE       RT_BIT(3)
/** Bit 4 - TSC - Time Stamp Counter. */
#define X86_CPUID_FEATURE_EDX_TSC       RT_BIT(4)
/** Bit 5 - MSR - Model Specific Registers RDMSR and WRMSR Instructions. */
#define X86_CPUID_FEATURE_EDX_MSR       RT_BIT(5)
/** Bit 6 - PAE - Physical Address Extension. */
#define X86_CPUID_FEATURE_EDX_PAE       RT_BIT(6)
/** Bit 7 - MCE - Machine Check Exception. */
#define X86_CPUID_FEATURE_EDX_MCE       RT_BIT(7)
/** Bit 8 - CX8 - CMPXCHG8B instruction. */
#define X86_CPUID_FEATURE_EDX_CX8       RT_BIT(8)
/** Bit 9 - APIC - APIC On-Chip. */
#define X86_CPUID_FEATURE_EDX_APIC      RT_BIT(9)
/** Bit 11 - SEP - SYSENTER and SYSEXIT. */
#define X86_CPUID_FEATURE_EDX_SEP       RT_BIT(11)
/** Bit 12 - MTRR - Memory Type Range Registers. */
#define X86_CPUID_FEATURE_EDX_MTRR      RT_BIT(12)
/** Bit 13 - PGE - PTE Global Bit. */
#define X86_CPUID_FEATURE_EDX_PGE       RT_BIT(13)
/** Bit 14 - MCA - Machine Check Architecture. */
#define X86_CPUID_FEATURE_EDX_MCA       RT_BIT(14)
/** Bit 15 - CMOV - Conditional Move Instructions. */
#define X86_CPUID_FEATURE_EDX_CMOV      RT_BIT(15)
/** Bit 16 - PAT - Page Attribute Table. */
#define X86_CPUID_FEATURE_EDX_PAT       RT_BIT(16)
/** Bit 17 - PSE-36 - 36-bit Page Size Extention. */
#define X86_CPUID_FEATURE_EDX_PSE36     RT_BIT(17)
/** Bit 18 - PSN - Processor Serial Number. */
#define X86_CPUID_FEATURE_EDX_PSN       RT_BIT(18)
/** Bit 19 - CLFSH - CLFLUSH Instruction. */
#define X86_CPUID_FEATURE_EDX_CLFSH     RT_BIT(19)
/** Bit 21 - DS - Debug Store. */
#define X86_CPUID_FEATURE_EDX_DS        RT_BIT(21)
/** Bit 22 - ACPI - Termal Monitor and Software Controlled Clock Facilities. */
#define X86_CPUID_FEATURE_EDX_ACPI      RT_BIT(22)
/** Bit 23 - MMX - Intel MMX Technology. */
#define X86_CPUID_FEATURE_EDX_MMX       RT_BIT(23)
/** Bit 24 - FXSR - FXSAVE and FXRSTOR Instructions. */
#define X86_CPUID_FEATURE_EDX_FXSR      RT_BIT(24)
/** Bit 25 - SSE - SSE Support. */
#define X86_CPUID_FEATURE_EDX_SSE       RT_BIT(25)
/** Bit 26 - SSE2 - SSE2 Support. */
#define X86_CPUID_FEATURE_EDX_SSE2      RT_BIT(26)
/** Bit 27 - SS - Self Snoop. */
#define X86_CPUID_FEATURE_EDX_SS        RT_BIT(27)
/** Bit 28 - HTT - Hyper-Threading Technology. */
#define X86_CPUID_FEATURE_EDX_HTT       RT_BIT(28)
/** Bit 29 - TM - Therm. Monitor. */
#define X86_CPUID_FEATURE_EDX_TM        RT_BIT(29)
/** Bit 31 - PBE - Pending Break Enabled. */
#define X86_CPUID_FEATURE_EDX_PBE       RT_BIT(31)
/** @} */


/** @name CPUID AMD Feature information.
 * CPUID query with EAX=0x80000001.
 * @{
 */
/** Bit 0 - FPU - x87 FPU on Chip. */
#define X86_CPUID_AMD_FEATURE_EDX_FPU   RT_BIT(0)
/** Bit 1 - VME - Virtual 8086 Mode Enhancements. */
#define X86_CPUID_AMD_FEATURE_EDX_VME    RT_BIT(1)
/** Bit 2 - DE - Debugging extensions. */
#define X86_CPUID_AMD_FEATURE_EDX_DE        RT_BIT(2)
/** Bit 3 - PSE - Page Size Extension. */
#define X86_CPUID_AMD_FEATURE_EDX_PSE       RT_BIT(3)
/** Bit 4 - TSC - Time Stamp Counter. */
#define X86_CPUID_AMD_FEATURE_EDX_TSC       RT_BIT(4)
/** Bit 5 - MSR - K86 Model Specific Registers RDMSR and WRMSR Instructions. */
#define X86_CPUID_AMD_FEATURE_EDX_MSR       RT_BIT(5)
/** Bit 6 - PAE - Physical Address Extension. */
#define X86_CPUID_AMD_FEATURE_EDX_PAE       RT_BIT(6)
/** Bit 7 - MCE - Machine Check Exception. */
#define X86_CPUID_AMD_FEATURE_EDX_MCE       RT_BIT(7)
/** Bit 8 - CX8 - CMPXCHG8B instruction. */
#define X86_CPUID_AMD_FEATURE_EDX_CX8       RT_BIT(8)
/** Bit 9 - APIC - APIC On-Chip. */
#define X86_CPUID_AMD_FEATURE_EDX_APIC      RT_BIT(9)
/** Bit 11 - SEP - AMD SYSCALL and SYSRET. */
#define X86_CPUID_AMD_FEATURE_EDX_SEP       RT_BIT(11)
/** Bit 12 - MTRR - Memory Type Range Registers. */
#define X86_CPUID_AMD_FEATURE_EDX_MTRR      RT_BIT(12)
/** Bit 13 - PGE - PTE Global Bit. */
#define X86_CPUID_AMD_FEATURE_EDX_PGE       RT_BIT(13)
/** Bit 14 - MCA - Machine Check Architecture. */
#define X86_CPUID_AMD_FEATURE_EDX_MCA       RT_BIT(14)
/** Bit 15 - CMOV - Conditional Move Instructions. */
#define X86_CPUID_AMD_FEATURE_EDX_CMOV      RT_BIT(15)
/** Bit 16 - PAT - Page Attribute Table. */
#define X86_CPUID_AMD_FEATURE_EDX_PAT       RT_BIT(16)
/** Bit 17 - PSE-36 - 36-bit Page Size Extention. */
#define X86_CPUID_AMD_FEATURE_EDX_PSE36     RT_BIT(17)
/** Bit 20 - NX - AMD No-Execute Page Protection. */
#define X86_CPUID_AMD_FEATURE_EDX_NX        RT_BIT(20)
/** Bit 22 - AXMMX - AMD Extensions to MMX Instructions. */
#define X86_CPUID_AMD_FEATURE_EDX_AXMMX     RT_BIT(22)
/** Bit 23 - MMX - Intel MMX Technology. */
#define X86_CPUID_AMD_FEATURE_EDX_MMX       RT_BIT(23)
/** Bit 24 - FXSR - FXSAVE and FXRSTOR Instructions. */
#define X86_CPUID_AMD_FEATURE_EDX_FXSR      RT_BIT(24)
/** Bit 25 - FFXSR - AMD fast FXSAVE and FXRSTOR Instructions. */
#define X86_CPUID_AMD_FEATURE_EDX_FFXSR     RT_BIT(25)
/** Bit 26 - PAGE1GB - AMD 1GB large page support. */
#define X86_CPUID_AMD_FEATURE_EDX_PAGE1GB   RT_BIT(26)
/** Bit 27 - RDTSCP - AMD RDTSCP instruction. */
#define X86_CPUID_AMD_FEATURE_EDX_RDTSCP    RT_BIT(27)
/** Bit 29 - LM - AMD Long Mode. */
#define X86_CPUID_AMD_FEATURE_EDX_LONG_MODE RT_BIT(29)
/** Bit 30 - 3DNOWEXT - AMD Extensions to 3DNow. */
#define X86_CPUID_AMD_FEATURE_EDX_3DNOW_EX  RT_BIT(30)
/** Bit 31 - 3DNOW - AMD 3DNow. */
#define X86_CPUID_AMD_FEATURE_EDX_3DNOW     RT_BIT(31)

/** Bit 0 - LAHF/SAHF - AMD LAHF and SAHF in 64-bit mode. */
#define X86_CPUID_AMD_FEATURE_ECX_LAHF_SAHF RT_BIT(0)
/** Bit 1 - CMPL - Core multi-processing legacy mode. */
#define X86_CPUID_AMD_FEATURE_ECX_CMPL      RT_BIT(1)
/** Bit 2 - SVM - AMD VM extensions. */
#define X86_CPUID_AMD_FEATURE_ECX_SVM       RT_BIT(2)
/** Bit 3 - EXTAPIC - AMD extended APIC registers starting at 0x400. */
#define X86_CPUID_AMD_FEATURE_ECX_EXT_APIC  RT_BIT(3)
/** Bit 4 - CR8L - AMD LOCK MOV CR0 means MOV CR8. */
#define X86_CPUID_AMD_FEATURE_ECX_CR8L      RT_BIT(4)
/** Bit 5 - ABM - AMD Advanced bit manipulation. LZCNT instruction support. */
#define X86_CPUID_AMD_FEATURE_ECX_ABM       RT_BIT(5)
/** Bit 6 - SSE4A - AMD EXTRQ, INSERTQ, MOVNTSS, and MOVNTSD instruction support. */
#define X86_CPUID_AMD_FEATURE_ECX_SSE4A     RT_BIT(6)
/** Bit 7 - MISALIGNSSE - AMD Misaligned SSE mode. */
#define X86_CPUID_AMD_FEATURE_ECX_MISALNSSE RT_BIT(7)
/** Bit 8 - 3DNOWPRF - AMD PREFETCH and PREFETCHW instruction support. */
#define X86_CPUID_AMD_FEATURE_ECX_3DNOWPRF  RT_BIT(8)
/** Bit 9 - OSVW - AMD OS visible workaround. */
#define X86_CPUID_AMD_FEATURE_ECX_OSVW      RT_BIT(9)
/** Bit 12 - SKINIT - AMD SKINIT: SKINIT, STGI, and DEV support. */
#define X86_CPUID_AMD_FEATURE_ECX_SKINIT    RT_BIT(12)
/** Bit 13 - WDT - AMD Watchdog timer support. */
#define X86_CPUID_AMD_FEATURE_ECX_WDT       RT_BIT(13)

/** @} */


/** @name CPUID AMD Feature information.
 * CPUID query with EAX=0x80000007.
 * @{
 */
/** Bit 0 - TS - Temperature Sensor. */
#define X86_CPUID_AMD_ADVPOWER_EDX_TS        RT_BIT(0)
/** Bit 1 - FID - Frequency ID Control. */
#define X86_CPUID_AMD_ADVPOWER_EDX_FID       RT_BIT(1)
/** Bit 2 - VID - Voltage ID Control. */
#define X86_CPUID_AMD_ADVPOWER_EDX_VID       RT_BIT(2)
/** Bit 3 - TTP - THERMTRIP. */
#define X86_CPUID_AMD_ADVPOWER_EDX_TTP       RT_BIT(3)
/** Bit 4 - TM - Hardware Thermal Control. */
#define X86_CPUID_AMD_ADVPOWER_EDX_TM        RT_BIT(4)
/** Bit 5 - STC - Software Thermal Control. */
#define X86_CPUID_AMD_ADVPOWER_EDX_STC       RT_BIT(5)
/** Bit 6 - MC - 100 Mhz Multiplier Control. */
#define X86_CPUID_AMD_ADVPOWER_EDX_MC        RT_BIT(6)
/** Bit 7 - HWPSTATE - Hardware P-State Control. */
#define X86_CPUID_AMD_ADVPOWER_EDX_HWPSTATE  RT_BIT(7)
/** Bit 8 - TSCINVAR - TSC Invariant. */
#define X86_CPUID_AMD_ADVPOWER_EDX_TSCINVAR  RT_BIT(8)
/** @} */


/** @name CR0
 * @{ */
/** Bit 0 - PE - Protection Enabled */
#define X86_CR0_PE                          RT_BIT(0)
#define X86_CR0_PROTECTION_ENABLE           RT_BIT(0)
/** Bit 1 - MP - Monitor Coprocessor */
#define X86_CR0_MP                          RT_BIT(1)
#define X86_CR0_MONITOR_COPROCESSOR         RT_BIT(1)
/** Bit 2 - EM - Emulation. */
#define X86_CR0_EM                          RT_BIT(2)
#define X86_CR0_EMULATE_FPU                 RT_BIT(2)
/** Bit 3 - TS - Task Switch. */
#define X86_CR0_TS                          RT_BIT(3)
#define X86_CR0_TASK_SWITCH                 RT_BIT(3)
/** Bit 4 - ET - Extension flag. ('hardcoded' to 1) */
#define X86_CR0_ET                          RT_BIT(4)
#define X86_CR0_EXTENSION_TYPE              RT_BIT(4)
/** Bit 5 - NE - Numeric error. */
#define X86_CR0_NE                          RT_BIT(5)
#define X86_CR0_NUMERIC_ERROR               RT_BIT(5)
/** Bit 16 - WP - Write Protect. */
#define X86_CR0_WP                          RT_BIT(16)
#define X86_CR0_WRITE_PROTECT               RT_BIT(16)
/** Bit 18 - AM - Alignment Mask. */
#define X86_CR0_AM                          RT_BIT(18)
#define X86_CR0_ALIGMENT_MASK               RT_BIT(18)
/** Bit 29 - NW - Not Write-though. */
#define X86_CR0_NW                          RT_BIT(29)
#define X86_CR0_NOT_WRITE_THROUGH           RT_BIT(29)
/** Bit 30 - WP - Cache Disable. */
#define X86_CR0_CD                          RT_BIT(30)
#define X86_CR0_CACHE_DISABLE               RT_BIT(30)
/** Bit 31 - PG - Paging. */
#define X86_CR0_PG                          RT_BIT(31)
#define X86_CR0_PAGING                      RT_BIT(31)
/** @} */


/** @name CR3
 * @{ */
/** Bit 3 - PWT - Page-level Writes Transparent. */
#define X86_CR3_PWT                         RT_BIT(3)
/** Bit 4 - PCD - Page-level Cache Disable. */
#define X86_CR3_PCD                         RT_BIT(4)
/** Bits 12-31 - - Page directory page number. */
#define X86_CR3_PAGE_MASK                   (0xfffff000)
/** Bits  5-31 - - PAE Page directory page number. */
#define X86_CR3_PAE_PAGE_MASK               (0xffffffe0)
/** Bits 12-51 - - AMD64 Page directory page number. */
#define X86_CR3_AMD64_PAGE_MASK             UINT64_C(0x000ffffffffff000)
/** @} */


/** @name CR4
 * @{ */
/** Bit 0 - VME - Virtual-8086 Mode Extensions. */
#define X86_CR4_VME                         RT_BIT(0)
/** Bit 1 - PVI - Protected-Mode Virtual Interrupts. */
#define X86_CR4_PVI                         RT_BIT(1)
/** Bit 2 - TSD - Time Stamp Disable. */
#define X86_CR4_TSD                         RT_BIT(2)
/** Bit 3 - DE - Debugging Extensions. */
#define X86_CR4_DE                          RT_BIT(3)
/** Bit 4 - PSE - Page Size Extension. */
#define X86_CR4_PSE                         RT_BIT(4)
/** Bit 5 - PAE - Physical Address Extension. */
#define X86_CR4_PAE                         RT_BIT(5)
/** Bit 6 - MCE - Machine-Check Enable. */
#define X86_CR4_MCE                         RT_BIT(6)
/** Bit 7 - PGE - Page Global Enable. */
#define X86_CR4_PGE                         RT_BIT(7)
/** Bit 8 - PCE - Performance-Monitoring Counter Enable. */
#define X86_CR4_PCE                         RT_BIT(8)
/** Bit 9 - OSFSXR - Operating System Support for FXSAVE and FXRSTORE instruction. */
#define X86_CR4_OSFSXR                      RT_BIT(9)
/** Bit 10 - OSXMMEEXCPT - Operating System Support for Unmasked SIMD Floating-Point Exceptions. */
#define X86_CR4_OSXMMEEXCPT                 RT_BIT(10)
/** Bit 13 - VMXE - VMX mode is enabled. */
#define X86_CR4_VMXE                        RT_BIT(13)
/** @} */


/** @name DR6
 * @{ */
/** Bit 0 - B0 - Breakpoint 0 condition detected. */
#define X86_DR6_B0                          RT_BIT(0)
/** Bit 1 - B1 - Breakpoint 1 condition detected. */
#define X86_DR6_B1                          RT_BIT(1)
/** Bit 2 - B2 - Breakpoint 2 condition detected. */
#define X86_DR6_B2                          RT_BIT(2)
/** Bit 3 - B3 - Breakpoint 3 condition detected. */
#define X86_DR6_B3                          RT_BIT(3)
/** Bit 13 - BD - Debug register access detected. Corresponds to the X86_DR7_GD bit. */
#define X86_DR6_BD                          RT_BIT(13)
/** Bit 14 - BS - Single step */
#define X86_DR6_BS                          RT_BIT(14)
/** Bit 15 - BT - Task switch. (TSS T bit.) */
#define X86_DR6_BT                          RT_BIT(15)
/** Value of DR6 after powerup/reset. */
#define X86_DR6_INIT_VAL                    UINT64_C(0xFFFF0FF0)
/** @} */


/** @name DR7
 * @{ */
/** Bit 0 - L0 - Local breakpoint enable. Cleared on task switch. */
#define X86_DR7_L0                          RT_BIT(0)
/** Bit 1 - G0 - Global breakpoint enable. Not cleared on task switch. */
#define X86_DR7_G0                          RT_BIT(1)
/** Bit 2 - L1 - Local breakpoint enable. Cleared on task switch. */
#define X86_DR7_L1                          RT_BIT(2)
/** Bit 3 - G1 - Global breakpoint enable. Not cleared on task switch. */
#define X86_DR7_G1                          RT_BIT(3)
/** Bit 4 - L2 - Local breakpoint enable. Cleared on task switch. */
#define X86_DR7_L2                          RT_BIT(4)
/** Bit 5 - G2 - Global breakpoint enable. Not cleared on task switch. */
#define X86_DR7_G2                          RT_BIT(5)
/** Bit 6 - L3 - Local breakpoint enable. Cleared on task switch. */
#define X86_DR7_L3                          RT_BIT(6)
/** Bit 7 - G3 - Global breakpoint enable. Not cleared on task switch. */
#define X86_DR7_G3                          RT_BIT(7)
/** Bit 8 - LE - Local breakpoint exact. (Not supported (read ignored) by P6 and later.) */
#define X86_DR7_LE                          RT_BIT(8)
/** Bit 9 - GE - Local breakpoint exact. (Not supported (read ignored) by P6 and later.) */
#define X86_DR7_GE                          RT_BIT(9)

/** Bit 13 - GD - General detect enable. Enables emulators to get exceptions when
 * any DR register is accessed. */
#define X86_DR7_GD                          RT_BIT(13)
/** Bit 16 & 17 - R/W0 - Read write field 0. Values X86_DR7_RW_*. */
#define X86_DR7_RW0_MASK                    (3 << 16)
/** Bit 18 & 19 - LEN0 - Length field 0. Values X86_DR7_LEN_*. */
#define X86_DR7_LEN0_MASK                   (3 << 18)
/** Bit 20 & 21 - R/W1 - Read write field 0. Values X86_DR7_RW_*. */
#define X86_DR7_RW1_MASK                    (3 << 20)
/** Bit 22 & 23 - LEN1 - Length field 0. Values X86_DR7_LEN_*. */
#define X86_DR7_LEN1_MASK                   (3 << 22)
/** Bit 24 & 25 - R/W2 - Read write field 0. Values X86_DR7_RW_*. */
#define X86_DR7_RW2_MASK                    (3 << 24)
/** Bit 26 & 27 - LEN2 - Length field 0. Values X86_DR7_LEN_*. */
#define X86_DR7_LEN2_MASK                   (3 << 26)
/** Bit 28 & 29 - R/W3 - Read write field 0. Values X86_DR7_RW_*. */
#define X86_DR7_RW3_MASK                    (3 << 28)
/** Bit 30 & 31 - LEN3 - Length field 0. Values X86_DR7_LEN_*. */
#define X86_DR7_LEN3_MASK                   (3 << 30)

/** Bits which must be 1s. */
#define X86_DR7_MB1_MASK                    (RT_BIT(10))

/** Calcs the L bit of Nth breakpoint.
 * @param   iBp     The breakpoint number [0..3].
 */
#define X86_DR7_L(iBp)                      ( UINT32_C(1) << (iBp * 2) )

/** Calcs the G bit of Nth breakpoint.
 * @param   iBp     The breakpoint number [0..3].
 */
#define X86_DR7_G(iBp)                      ( UINT32_C(1) << (iBp * 2 + 1) )

/** @name Read/Write values.
 * @{ */
/** Break on instruction fetch only. */
#define X86_DR7_RW_EO                       0U
/** Break on write only. */
#define X86_DR7_RW_WO                       1U
/** Break on I/O read/write. This is only defined if CR4.DE is set. */
#define X86_DR7_RW_IO                       2U
/** Break on read or write (but not instruction fetches). */
#define X86_DR7_RW_RW                       3U
/** @} */

/** Shifts a X86_DR7_RW_* value to its right place.
 * @param   iBp     The breakpoint number [0..3].
 * @param   fRw     One of the X86_DR7_RW_* value.
 */
#define X86_DR7_RW(iBp, fRw)                ( (fRw) << ((iBp) * 4 + 16) )

/** @name Length values.
 * @{ */
#define X86_DR7_LEN_BYTE                    0U
#define X86_DR7_LEN_WORD                    1U
#define X86_DR7_LEN_QWORD                   2U /**< AMD64 long mode only. */
#define X86_DR7_LEN_DWORD                   3U
/** @} */

/** Shifts a X86_DR7_LEN_* value to its right place.
 * @param   iBp     The breakpoint number [0..3].
 * @param   cb      One of the X86_DR7_LEN_* values.
 */
#define X86_DR7_LEN(iBp, cb)                ( (cb) << ((iBp) * 4 + 18) )

/** Fetch the breakpoint length bits from the DR7 value.
 * @param   uDR7    DR7 value
 * @param   iBp     The breakpoint number [0..3].
 */
#define X86_DR7_GET_LEN(uDR7, iBp)          ( ( (uDR7) >> ((iBp) * 4 + 18) ) & 0x3U)

/** Mask used to check if any breakpoints are enabled. */
#define X86_DR7_ENABLED_MASK                (RT_BIT(0) | RT_BIT(1) | RT_BIT(2) | RT_BIT(3) | RT_BIT(4) | RT_BIT(5) | RT_BIT(6) | RT_BIT(7))

/** Mask used to check if any io breakpoints are set. */
#define X86_DR7_IO_ENABLED_MASK             (X86_DR7_RW(0, X86_DR7_RW_IO) | X86_DR7_RW(1, X86_DR7_RW_IO) | X86_DR7_RW(2, X86_DR7_RW_IO) | X86_DR7_RW(3, X86_DR7_RW_IO))

/** Value of DR7 after powerup/reset. */
#define X86_DR7_INIT_VAL                    0x400
/** @} */


/** @name Machine Specific Registers
 * @{
 */

/** Time Stamp Counter. */
#define MSR_IA32_TSC                        0x10

#define MSR_IA32_PLATFORM_ID                0x17

#ifndef MSR_IA32_APICBASE /* qemu cpu.h klugde */
#define MSR_IA32_APICBASE                   0x1b
#endif

/** CPU Feature control. */
#define MSR_IA32_FEATURE_CONTROL            0x3A
#define MSR_IA32_FEATURE_CONTROL_LOCK       RT_BIT(0)
#define MSR_IA32_FEATURE_CONTROL_VMXON      RT_BIT(2)

/** BIOS update trigger (microcode update). */
#define MSR_IA32_BIOS_UPDT_TRIG             0x79

/** BIOS update signature (microcode). */
#define MSR_IA32_BIOS_SIGN_ID               0x8B

/** MTRR Capabilities. */
#define MSR_IA32_MTRR_CAP                   0xFE


#ifndef MSR_IA32_SYSENTER_CS /* qemu cpu.h klugde */
/** SYSENTER_CS - the R0 CS, indirectly giving R0 SS, R3 CS and R3 DS.
 * R0 SS == CS + 8
 * R3 CS == CS + 16
 * R3 SS == CS + 24
 */
#define MSR_IA32_SYSENTER_CS                0x174
/** SYSENTER_ESP - the R0 ESP. */
#define MSR_IA32_SYSENTER_ESP               0x175
/** SYSENTER_EIP - the R0 EIP. */
#define MSR_IA32_SYSENTER_EIP               0x176
#endif

/** Machine Check Global Capabilities Register. */
#define MSR_IA32_MCP_CAP                    0x179
/** Machine Check Global Status Register. */
#define MSR_IA32_MCP_STATUS                 0x17A
/** Machine Check Global Control Register. */
#define MSR_IA32_MCP_CTRL                   0x17B

/* Page Attribute Table. */
#define MSR_IA32_CR_PAT                     0x277

/** MTRR Default Range. */
#define MSR_IA32_MTRR_DEF_TYPE              0x2FF

#define MSR_IA32_MC0_CTL                    0x400
#define MSR_IA32_MC0_STATUS                 0x401

/** Basic VMX information. */
#define MSR_IA32_VMX_BASIC_INFO             0x480
/** Allowed settings for pin-based VM execution controls */
#define MSR_IA32_VMX_PINBASED_CTLS          0x481
/** Allowed settings for proc-based VM execution controls */
#define MSR_IA32_VMX_PROCBASED_CTLS         0x482
/** Allowed settings for the VMX exit controls. */
#define MSR_IA32_VMX_EXIT_CTLS              0x483
/** Allowed settings for the VMX entry controls. */
#define MSR_IA32_VMX_ENTRY_CTLS             0x484
/** Misc VMX info. */
#define MSR_IA32_VMX_MISC                   0x485
/** Fixed cleared bits in CR0. */
#define MSR_IA32_VMX_CR0_FIXED0             0x486
/** Fixed set bits in CR0. */
#define MSR_IA32_VMX_CR0_FIXED1             0x487
/** Fixed cleared bits in CR4. */
#define MSR_IA32_VMX_CR4_FIXED0             0x488
/** Fixed set bits in CR4. */
#define MSR_IA32_VMX_CR4_FIXED1             0x489
/** Information for enumerating fields in the VMCS. */
#define MSR_IA32_VMX_VMCS_ENUM              0x48A
/** Allowed settings for secondary proc-based VM execution controls */
#define MSR_IA32_VMX_PROCBASED_CTLS2        0x48B
/** EPT capabilities. */
#define MSR_IA32_VMX_EPT_CAPS               0x48C
/** X2APIC MSR ranges. */
#define MSR_IA32_APIC_START                 0x800
#define MSR_IA32_APIC_END                   0x900

/** K6 EFER - Extended Feature Enable Register. */
#define MSR_K6_EFER                         0xc0000080
/** @todo document EFER */
/** Bit 0 - SCE - System call extensions (SYSCALL / SYSRET). (R/W) */
#define  MSR_K6_EFER_SCE                     RT_BIT(0)
/** Bit 8 - LME - Long mode enabled. (R/W) */
#define  MSR_K6_EFER_LME                     RT_BIT(8)
/** Bit 10 - LMA - Long mode active. (R) */
#define  MSR_K6_EFER_LMA                     RT_BIT(10)
/** Bit 11 - NXE - No-Execute Page Protection Enabled. (R/W) */
#define  MSR_K6_EFER_NXE                     RT_BIT(11)
/** Bit 12 - SVME - Secure VM Extension Enabled. (R/W) */
#define  MSR_K6_EFER_SVME                    RT_BIT(12)
/** Bit 13 - LMSLE - Long Mode Segment Limit Enable. (R/W?) */
#define  MSR_K6_EFER_LMSLE                   RT_BIT(13)
/** Bit 14 - FFXSR - Fast FXSAVE / FXRSTOR (skip XMM*). (R/W) */
#define  MSR_K6_EFER_FFXSR                   RT_BIT(14)
/** K6 STAR - SYSCALL/RET targets. */
#define MSR_K6_STAR                         0xc0000081
/** Shift value for getting the SYSRET CS and SS value. */
#define  MSR_K6_STAR_SYSRET_CS_SS_SHIFT     48
/** Shift value for getting the SYSCALL CS and SS value. */
#define  MSR_K6_STAR_SYSCALL_CS_SS_SHIFT    32
/** Selector mask for use after shifting. */
#define  MSR_K6_STAR_SEL_MASK               0xffff
/** The mask which give the SYSCALL EIP. */
#define  MSR_K6_STAR_SYSCALL_EIP_MASK       0xffffffff
/** K6 WHCR - Write Handling Control Register. */
#define MSR_K6_WHCR                         0xc0000082
/** K6 UWCCR - UC/WC Cacheability Control Register. */
#define MSR_K6_UWCCR                        0xc0000085
/** K6 PSOR - Processor State Observability Register. */
#define MSR_K6_PSOR                         0xc0000087
/** K6 PFIR - Page Flush/Invalidate Register. */
#define MSR_K6_PFIR                         0xc0000088

#define MSR_K7_EVNTSEL0                     0xc0010000
#define MSR_K7_EVNTSEL1                     0xc0010001
#define MSR_K7_EVNTSEL2                     0xc0010002
#define MSR_K7_EVNTSEL3                     0xc0010003
#define MSR_K7_PERFCTR0                     0xc0010004
#define MSR_K7_PERFCTR1                     0xc0010005
#define MSR_K7_PERFCTR2                     0xc0010006
#define MSR_K7_PERFCTR3                     0xc0010007

/** K8 LSTAR - Long mode SYSCALL target (RIP). */
#define MSR_K8_LSTAR                        0xc0000082
/** K8 CSTAR - Compatibility mode SYSCALL target (RIP). */
#define MSR_K8_CSTAR                        0xc0000083
/** K8 SF_MASK - SYSCALL flag mask. (aka SFMASK) */
#define MSR_K8_SF_MASK                      0xc0000084
/** K8 FS.base - The 64-bit base FS register. */
#define MSR_K8_FS_BASE                      0xc0000100
/** K8 GS.base - The 64-bit base GS register. */
#define MSR_K8_GS_BASE                      0xc0000101
/** K8 KernelGSbase - Used with SWAPGS. */
#define MSR_K8_KERNEL_GS_BASE               0xc0000102
#define MSR_K8_TSC_AUX                      0xc0000103
#define MSR_K8_SYSCFG                       0xc0010010
#define MSR_K8_HWCR                         0xc0010015
#define MSR_K8_IORRBASE0                    0xc0010016
#define MSR_K8_IORRMASK0                    0xc0010017
#define MSR_K8_IORRBASE1                    0xc0010018
#define MSR_K8_IORRMASK1                    0xc0010019
#define MSR_K8_TOP_MEM1                     0xc001001a
#define MSR_K8_TOP_MEM2                     0xc001001d
#define MSR_K8_VM_CR                        0xc0010114
#define MSR_K8_VM_CR_SVM_DISABLE            RT_BIT(4)

#define MSR_K8_IGNNE                        0xc0010115
#define MSR_K8_SMM_CTL                      0xc0010116
/** SVM - VM_HSAVE_PA - Physical address for saving and restoring
 *                      host state during world switch.
 */
#define MSR_K8_VM_HSAVE_PA                  0xc0010117

/** @} */


/** @name Page Table / Directory / Directory Pointers / L4.
 * @{
 */

/** Page table/directory  entry as an unsigned integer. */
typedef uint32_t X86PGUINT;
/** Pointer to a page table/directory table entry as an unsigned integer. */
typedef X86PGUINT *PX86PGUINT;
/** Pointer to an const page table/directory table entry as an unsigned integer. */
typedef X86PGUINT const *PCX86PGUINT;

/** Number of entries in a 32-bit PT/PD. */
#define X86_PG_ENTRIES                      1024


/** PAE page table/page directory/pdpt/l4/l5 entry as an unsigned integer. */
typedef uint64_t X86PGPAEUINT;
/** Pointer to a PAE page table/page directory/pdpt/l4/l5 entry as an unsigned integer. */
typedef X86PGPAEUINT *PX86PGPAEUINT;
/** Pointer to an const PAE page table/page directory/pdpt/l4/l5 entry as an unsigned integer. */
typedef X86PGPAEUINT const *PCX86PGPAEUINT;

/** Number of entries in a PAE PT/PD. */
#define X86_PG_PAE_ENTRIES                  512
/** Number of entries in a PAE PDPT. */
#define X86_PG_PAE_PDPE_ENTRIES             4

/** Number of entries in an AMD64 PT/PD/PDPT/L4/L5. */
#define X86_PG_AMD64_ENTRIES                X86_PG_PAE_ENTRIES
/** Number of entries in an AMD64 PDPT.
 * Just for complementing X86_PG_PAE_PDPE_ENTRIES, using X86_PG_AMD64_ENTRIES for this is fine too. */
#define X86_PG_AMD64_PDPE_ENTRIES           X86_PG_AMD64_ENTRIES

/** The size of a 4KB page. */
#define X86_PAGE_4K_SIZE                    _4K
/** The page shift of a 4KB page. */
#define X86_PAGE_4K_SHIFT                   12
/** The 4KB page offset mask. */
#define X86_PAGE_4K_OFFSET_MASK             0xfff
/** The 4KB page base mask for virtual addresses. */
#define X86_PAGE_4K_BASE_MASK               0xfffffffffffff000ULL
/** The 4KB page base mask for virtual addresses - 32bit version. */
#define X86_PAGE_4K_BASE_MASK_32            0xfffff000U

/** The size of a 2MB page. */
#define X86_PAGE_2M_SIZE                    _2M
/** The page shift of a 2MB page. */
#define X86_PAGE_2M_SHIFT                   21
/** The 2MB page offset mask. */
#define X86_PAGE_2M_OFFSET_MASK             0x001fffff
/** The 2MB page base mask for virtual addresses. */
#define X86_PAGE_2M_BASE_MASK               0xffffffffffe00000ULL
/** The 2MB page base mask for virtual addresses - 32bit version. */
#define X86_PAGE_2M_BASE_MASK_32            0xffe00000U

/** The size of a 4MB page. */
#define X86_PAGE_4M_SIZE                    _4M
/** The page shift of a 4MB page. */
#define X86_PAGE_4M_SHIFT                   22
/** The 4MB page offset mask. */
#define X86_PAGE_4M_OFFSET_MASK             0x003fffff
/** The 4MB page base mask for virtual addresses. */
#define X86_PAGE_4M_BASE_MASK               0xffffffffffc00000ULL
/** The 4MB page base mask for virtual addresses - 32bit version. */
#define X86_PAGE_4M_BASE_MASK_32            0xffc00000U



/** @name Page Table Entry
 * @{
 */
/** Bit 0 -  P  - Present bit. */
#define X86_PTE_BIT_P                       0
/** Bit 1 - R/W - Read (clear) / Write (set) bit. */
#define X86_PTE_BIT_RW                      1)
/** Bit 2 - U/S - User (set) / Supervisor (clear) bit. */
#define X86_PTE_BIT_US                      2
/** Bit 3 - PWT - Page level write thru bit. */
#define X86_PTE_BIT_PWT                     3
/** Bit 4 - PCD - Page level cache disable bit. */
#define X86_PTE_BIT_PCD                     4
/** Bit 5 -  A  - Access bit. */
#define X86_PTE_BIT_A                       5
/** Bit 6 -  D  - Dirty bit. */
#define X86_PTE_BIT_D                       6
/** Bit 7 - PAT - Page Attribute Table index bit. Reserved and 0 if not supported. */
#define X86_PTE_BIT_PAT                     7
/** Bit 8 -  G  - Global flag. */
#define X86_PTE_BIT_G                       8

/** Bit 0 -  P  - Present bit mask. */
#define X86_PTE_P                           RT_BIT(0)
/** Bit 1 - R/W - Read (clear) / Write (set) bit mask. */
#define X86_PTE_RW                          RT_BIT(1)
/** Bit 2 - U/S - User (set) / Supervisor (clear) bit mask. */
#define X86_PTE_US                          RT_BIT(2)
/** Bit 3 - PWT - Page level write thru bit mask. */
#define X86_PTE_PWT                         RT_BIT(3)
/** Bit 4 - PCD - Page level cache disable bit mask. */
#define X86_PTE_PCD                         RT_BIT(4)
/** Bit 5 -  A  - Access bit mask. */
#define X86_PTE_A                           RT_BIT(5)
/** Bit 6 -  D  - Dirty bit mask. */
#define X86_PTE_D                           RT_BIT(6)
/** Bit 7 - PAT - Page Attribute Table index bit mask. Reserved and 0 if not supported. */
#define X86_PTE_PAT                         RT_BIT(7)
/** Bit 8 -  G  - Global bit mask. */
#define X86_PTE_G                           RT_BIT(8)

/** Bits 9-11 - - Available for use to system software. */
#define X86_PTE_AVL_MASK                    (RT_BIT(9) | RT_BIT(10) | RT_BIT(11))
/** Bits 12-31 - - Physical Page number of the next level. */
#define X86_PTE_PG_MASK                     ( 0xfffff000 )

/** Bits 12-51 - - PAE - Physical Page number of the next level. */
#if 1 /* we're using this internally and have to mask of the top 16-bit. */
#define X86_PTE_PAE_PG_MASK                 ( 0x0000fffffffff000ULL )
/** @todo Get rid of the above hack; makes code unreadable. */
#define X86_PTE_PAE_PG_MASK_FULL            ( 0x000ffffffffff000ULL )
#else
#define X86_PTE_PAE_PG_MASK                 ( 0x000ffffffffff000ULL )
#endif
/** Bits 63 - NX - PAE - No execution flag. */
#define X86_PTE_PAE_NX                      RT_BIT_64(63)

/**
 * Page table entry.
 */
typedef struct X86PTEBITS
{
    /** Flags whether(=1) or not the page is present. */
    unsigned    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    unsigned    u1Write : 1;
    /** User(=1) / Supervisor (=0) flag. */
    unsigned    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    unsigned    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    unsigned    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    unsigned    u1Accessed : 1;
    /** Dirty flag.
     * Indicates that the page have been written to. */
    unsigned    u1Dirty : 1;
    /** Reserved / If PAT enabled, bit 2 of the index.  */
    unsigned    u1PAT : 1;
    /** Global flag. (Ignored in all but final level.) */
    unsigned    u1Global : 1;
    /** Available for use to system software. */
    unsigned    u3Available : 3;
    /** Physical Page number of the next level. */
    unsigned    u20PageNo : 20;
} X86PTEBITS;
/** Pointer to a page table entry. */
typedef X86PTEBITS *PX86PTEBITS;
/** Pointer to a const page table entry. */
typedef const X86PTEBITS *PCX86PTEBITS;

/**
 * Page table entry.
 */
typedef union X86PTE
{
    /** Unsigned integer view */
    X86PGUINT       u;
    /** Bit field view. */
    X86PTEBITS      n;
    /** 32-bit view. */
    uint32_t        au32[1];
    /** 16-bit view. */
    uint16_t        au16[2];
    /** 8-bit view. */
    uint8_t         au8[4];
} X86PTE;
/** Pointer to a page table entry. */
typedef X86PTE *PX86PTE;
/** Pointer to a const page table entry. */
typedef const X86PTE *PCX86PTE;


/**
 * PAE page table entry.
 */
typedef struct X86PTEPAEBITS
{
    /** Flags whether(=1) or not the page is present. */
    uint32_t    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    uint32_t    u1Write : 1;
    /** User(=1) / Supervisor(=0) flag. */
    uint32_t    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint32_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint32_t    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    uint32_t    u1Accessed : 1;
    /** Dirty flag.
     * Indicates that the page have been written to. */
    uint32_t    u1Dirty : 1;
    /** Reserved / If PAT enabled, bit 2 of the index.  */
    uint32_t    u1PAT : 1;
    /** Global flag. (Ignored in all but final level.) */
    uint32_t    u1Global : 1;
    /** Available for use to system software. */
    uint32_t    u3Available : 3;
    /** Physical Page number of the next level - Low Part. Don't use this. */
    uint32_t    u20PageNoLow : 20;
    /** Physical Page number of the next level - High Part. Don't use this. */
    uint32_t    u20PageNoHigh : 20;
    /** MBZ bits */
    uint32_t    u11Reserved : 11;
    /** No Execute flag. */
    uint32_t    u1NoExecute : 1;
} X86PTEPAEBITS;
/** Pointer to a page table entry. */
typedef X86PTEPAEBITS *PX86PTEPAEBITS;
/** Pointer to a page table entry. */
typedef const X86PTEPAEBITS *PCX86PTEPAEBITS;

/**
 * PAE Page table entry.
 */
typedef union X86PTEPAE
{
    /** Unsigned integer view */
    X86PGPAEUINT    u;
    /** Bit field view. */
    X86PTEPAEBITS   n;
    /** 32-bit view. */
    uint32_t        au32[2];
    /** 16-bit view. */
    uint16_t        au16[4];
    /** 8-bit view. */
    uint8_t         au8[8];
} X86PTEPAE;
/** Pointer to a PAE page table entry. */
typedef X86PTEPAE *PX86PTEPAE;
/** Pointer to a const PAE page table entry. */
typedef const X86PTEPAE *PCX86PTEPAE;
/** @} */

/**
 * Page table.
 */
typedef struct X86PT
{
    /** PTE Array. */
    X86PTE     a[X86_PG_ENTRIES];
} X86PT;
/** Pointer to a page table. */
typedef X86PT *PX86PT;
/** Pointer to a const page table. */
typedef const X86PT *PCX86PT;

/** The page shift to get the PT index. */
#define X86_PT_SHIFT                        12
/** The PT index mask (apply to a shifted page address). */
#define X86_PT_MASK                         0x3ff


/**
 * Page directory.
 */
typedef struct X86PTPAE
{
    /** PTE Array. */
    X86PTEPAE  a[X86_PG_PAE_ENTRIES];
} X86PTPAE;
/** Pointer to a page table. */
typedef X86PTPAE *PX86PTPAE;
/** Pointer to a const page table. */
typedef const X86PTPAE *PCX86PTPAE;

/** The page shift to get the PA PTE index. */
#define X86_PT_PAE_SHIFT                    12
/** The PAE PT index mask (apply to a shifted page address). */
#define X86_PT_PAE_MASK                     0x1ff


/** @name 4KB Page Directory Entry
 * @{
 */
/** Bit 0 -  P  - Present bit. */
#define X86_PDE_P                           RT_BIT(0)
/** Bit 1 - R/W - Read (clear) / Write (set) bit. */
#define X86_PDE_RW                          RT_BIT(1)
/** Bit 2 - U/S - User (set) / Supervisor (clear) bit. */
#define X86_PDE_US                          RT_BIT(2)
/** Bit 3 - PWT - Page level write thru bit. */
#define X86_PDE_PWT                         RT_BIT(3)
/** Bit 4 - PCD - Page level cache disable bit. */
#define X86_PDE_PCD                         RT_BIT(4)
/** Bit 5 -  A  - Access bit. */
#define X86_PDE_A                           RT_BIT(5)
/** Bit 7 - PS  - Page size attribute.
 * Clear mean 4KB pages, set means large pages (2/4MB). */
#define X86_PDE_PS                          RT_BIT(7)
/** Bits 9-11 - - Available for use to system software. */
#define X86_PDE_AVL_MASK                    (RT_BIT(9) | RT_BIT(10) | RT_BIT(11))
/** Bits 12-31 -  - Physical Page number of the next level. */
#define X86_PDE_PG_MASK                     ( 0xfffff000 )

/** Bits 12-51 - - PAE - Physical Page number of the next level. */
#if 1 /* we're using this internally and have to mask of the top 16-bit. */
/* Note: This is kind of dangerous if the guest uses these bits (legally or illegally);
 *       we partly or that part into shadow page table entries. Will be corrected
 *       soon.
 */
#define X86_PDE_PAE_PG_MASK                 ( 0x0000fffffffff000ULL )
#define X86_PDE_PAE_PG_MASK_FULL            ( 0x000ffffffffff000ULL )
#else
#define X86_PDE_PAE_PG_MASK                 ( 0x000ffffffffff000ULL )
#endif
/** Bits 63 - NX - PAE - No execution flag. */
#define X86_PDE_PAE_NX                      RT_BIT_64(63)

/**
 * Page directory entry.
 */
typedef struct X86PDEBITS
{
    /** Flags whether(=1) or not the page is present. */
    unsigned    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    unsigned    u1Write : 1;
    /** User(=1) / Supervisor (=0) flag. */
    unsigned    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    unsigned    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    unsigned    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    unsigned    u1Accessed : 1;
    /** Reserved / Ignored (dirty bit). */
    unsigned    u1Reserved0 : 1;
    /** Size bit if PSE is enabled - in any event it's 0. */
    unsigned    u1Size : 1;
    /** Reserved / Ignored (global bit). */
    unsigned    u1Reserved1 : 1;
    /** Available for use to system software. */
    unsigned    u3Available : 3;
    /** Physical Page number of the next level. */
    unsigned    u20PageNo : 20;
} X86PDEBITS;
/** Pointer to a page directory entry. */
typedef X86PDEBITS *PX86PDEBITS;
/** Pointer to a const page directory entry. */
typedef const X86PDEBITS *PCX86PDEBITS;


/**
 * PAE page directory entry.
 */
typedef struct X86PDEPAEBITS
{
    /** Flags whether(=1) or not the page is present. */
    uint32_t    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    uint32_t    u1Write : 1;
    /** User(=1) / Supervisor (=0) flag. */
    uint32_t    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint32_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint32_t    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    uint32_t    u1Accessed : 1;
    /** Reserved / Ignored (dirty bit). */
    uint32_t    u1Reserved0 : 1;
    /** Size bit if PSE is enabled - in any event it's 0. */
    uint32_t    u1Size : 1;
    /** Reserved / Ignored (global bit). /  */
    uint32_t    u1Reserved1 : 1;
    /** Available for use to system software. */
    uint32_t    u3Available : 3;
    /** Physical Page number of the next level - Low Part. Don't use! */
    uint32_t    u20PageNoLow : 20;
    /** Physical Page number of the next level - High Part. Don't use! */
    uint32_t    u20PageNoHigh : 20;
    /** MBZ bits */
    uint32_t    u11Reserved : 11;
    /** No Execute flag. */
    uint32_t    u1NoExecute : 1;
} X86PDEPAEBITS;
/** Pointer to a page directory entry. */
typedef X86PDEPAEBITS *PX86PDEPAEBITS;
/** Pointer to a const page directory entry. */
typedef const X86PDEPAEBITS *PCX86PDEPAEBITS;

/** @} */


/** @name 2/4MB Page Directory Entry
 * @{
 */
/** Bit 0 -  P  - Present bit. */
#define X86_PDE4M_P                         RT_BIT(0)
/** Bit 1 - R/W - Read (clear) / Write (set) bit. */
#define X86_PDE4M_RW                        RT_BIT(1)
/** Bit 2 - U/S - User (set) / Supervisor (clear) bit. */
#define X86_PDE4M_US                        RT_BIT(2)
/** Bit 3 - PWT - Page level write thru bit. */
#define X86_PDE4M_PWT                       RT_BIT(3)
/** Bit 4 - PCD - Page level cache disable bit. */
#define X86_PDE4M_PCD                       RT_BIT(4)
/** Bit 5 -  A  - Access bit. */
#define X86_PDE4M_A                         RT_BIT(5)
/** Bit 6 -  D  - Dirty bit. */
#define X86_PDE4M_D                         RT_BIT(6)
/** Bit 7 - PS  - Page size attribute. Clear mean 4KB pages, set means large pages (2/4MB). */
#define X86_PDE4M_PS                        RT_BIT(7)
/** Bit 8 -  G  - Global flag. */
#define X86_PDE4M_G                         RT_BIT(8)
/** Bits 9-11 - AVL - Available for use to system software. */
#define X86_PDE4M_AVL                       (RT_BIT(9) | RT_BIT(10) | RT_BIT(11))
/** Bit 12 - PAT - Page Attribute Table index bit. Reserved and 0 if not supported. */
#define X86_PDE4M_PAT                       RT_BIT(12)
/** Shift to get from X86_PTE_PAT to X86_PDE4M_PAT. */
#define X86_PDE4M_PAT_SHIFT                 (12 - 7)
/** Bits 22-31 - - Physical Page number. */
#define X86_PDE4M_PG_MASK                   ( 0xffc00000 )
/** Bits 13-20 - - Physical Page number high part (32-39 bits). AMD64 hack. */
#define X86_PDE4M_PG_HIGH_MASK              ( 0x001fe000 )
/** The number of bits to the high part of the page number. */
#define X86_PDE4M_PG_HIGH_SHIFT             19

/** Bits 21-51 - - PAE & AMD64 - Physical Page number.
 * (Bits 40-51 (long mode) & bits 36-51 (pae legacy) are reserved according to the Intel docs; AMD allows for more.) */
#define X86_PDE2M_PAE_PG_MASK               ( 0x000fffffffe00000ULL )
/** Bits 63 - NX - PAE & AMD64 - No execution flag. */
#define X86_PDE2M_PAE_NX                    X86_PDE2M_PAE_NX

/**
 * 4MB page directory entry.
 */
typedef struct X86PDE4MBITS
{
    /** Flags whether(=1) or not the page is present. */
    unsigned    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    unsigned    u1Write : 1;
    /** User(=1) / Supervisor (=0) flag. */
    unsigned    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    unsigned    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    unsigned    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    unsigned    u1Accessed : 1;
    /** Dirty flag.
     * Indicates that the page have been written to. */
    unsigned    u1Dirty : 1;
    /** Page size flag - always 1 for 4MB entries. */
    unsigned    u1Size : 1;
    /** Global flag.  */
    unsigned    u1Global : 1;
    /** Available for use to system software. */
    unsigned    u3Available : 3;
    /** Reserved / If PAT enabled, bit 2 of the index.  */
    unsigned    u1PAT : 1;
    /** Bits 32-39 of the page number on AMD64.
     * This AMD64 hack allows accessing 40bits of physical memory without PAE. */
    unsigned    u8PageNoHigh : 8;
    /** Reserved. */
    unsigned    u1Reserved : 1;
    /** Physical Page number of the page. */
    unsigned    u10PageNo : 10;
} X86PDE4MBITS;
/** Pointer to a page table entry. */
typedef X86PDE4MBITS *PX86PDE4MBITS;
/** Pointer to a const page table entry. */
typedef const X86PDE4MBITS *PCX86PDE4MBITS;


/**
 * 2MB PAE page directory entry.
 */
typedef struct X86PDE2MPAEBITS
{
    /** Flags whether(=1) or not the page is present. */
    uint32_t    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    uint32_t    u1Write : 1;
    /** User(=1) / Supervisor(=0) flag. */
    uint32_t    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint32_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint32_t    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    uint32_t    u1Accessed : 1;
    /** Dirty flag.
     * Indicates that the page have been written to. */
    uint32_t    u1Dirty : 1;
    /** Page size flag - always 1 for 2MB entries. */
    uint32_t    u1Size : 1;
    /** Global flag.  */
    uint32_t    u1Global : 1;
    /** Available for use to system software. */
    uint32_t    u3Available : 3;
    /** Reserved / If PAT enabled, bit 2 of the index.  */
    uint32_t    u1PAT : 1;
    /** Reserved. */
    uint32_t    u9Reserved : 9;
    /** Physical Page number of the next level - Low part. Don't use! */
    uint32_t    u10PageNoLow : 10;
    /** Physical Page number of the next level - High part. Don't use! */
    uint32_t    u20PageNoHigh : 20;
    /** MBZ bits */
    uint32_t    u11Reserved : 11;
    /** No Execute flag. */
    uint32_t    u1NoExecute : 1;
} X86PDE2MPAEBITS;
/** Pointer to a 4MB PAE page table entry. */
typedef X86PDE2MPAEBITS *PX86PDE2MPAEBITS;
/** Pointer to a 4MB PAE page table entry. */
typedef const X86PDE2MPAEBITS *PCX86PDE2MPAEBITS;

/** @} */

/**
 * Page directory entry.
 */
typedef union X86PDE
{
    /** Unsigned integer view. */
    X86PGUINT       u;
    /** Normal view. */
    X86PDEBITS      n;
    /** 4MB view (big). */
    X86PDE4MBITS    b;
    /** 8 bit unsigned integer view. */
    uint8_t         au8[4];
    /** 16 bit unsigned integer view. */
    uint16_t        au16[2];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[1];
} X86PDE;
/** Pointer to a page directory entry. */
typedef X86PDE *PX86PDE;
/** Pointer to a const page directory entry. */
typedef const X86PDE *PCX86PDE;

/**
 * PAE page directory entry.
 */
typedef union X86PDEPAE
{
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** Normal view. */
    X86PDEPAEBITS   n;
    /** 2MB page view (big). */
    X86PDE2MPAEBITS b;
    /** 8 bit unsigned integer view. */
    uint8_t         au8[8];
    /** 16 bit unsigned integer view. */
    uint16_t        au16[4];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} X86PDEPAE;
/** Pointer to a page directory entry. */
typedef X86PDEPAE *PX86PDEPAE;
/** Pointer to a const page directory entry. */
typedef const X86PDEPAE *PCX86PDEPAE;

/**
 * Page directory.
 */
typedef struct X86PD
{
    /** PDE Array. */
    X86PDE      a[X86_PG_ENTRIES];
} X86PD;
/** Pointer to a page directory. */
typedef X86PD *PX86PD;
/** Pointer to a const page directory. */
typedef const X86PD *PCX86PD;

/** The page shift to get the PD index. */
#define X86_PD_SHIFT                        22
/** The PD index mask (apply to a shifted page address). */
#define X86_PD_MASK                         0x3ff


/**
 * PAE page directory.
 */
typedef struct X86PDPAE
{
    /** PDE Array. */
    X86PDEPAE   a[X86_PG_PAE_ENTRIES];
} X86PDPAE;
/** Pointer to a PAE page directory. */
typedef X86PDPAE *PX86PDPAE;
/** Pointer to a const PAE page directory. */
typedef const X86PDPAE *PCX86PDPAE;

/** The page shift to get the PAE PD index. */
#define X86_PD_PAE_SHIFT                    21
/** The PAE PD index mask (apply to a shifted page address). */
#define X86_PD_PAE_MASK                     0x1ff


/** @name Page Directory Pointer Table Entry (PAE)
 * @{
 */
/** Bit 0 -  P  - Present bit. */
#define X86_PDPE_P                          RT_BIT(0)
/** Bit 1 - R/W - Read (clear) / Write (set) bit. Long Mode only. */
#define X86_PDPE_RW                         RT_BIT(1)
/** Bit 2 - U/S - User (set) / Supervisor (clear) bit. Long Mode only. */
#define X86_PDPE_US                         RT_BIT(2)
/** Bit 3 - PWT - Page level write thru bit. */
#define X86_PDPE_PWT                        RT_BIT(3)
/** Bit 4 - PCD - Page level cache disable bit. */
#define X86_PDPE_PCD                        RT_BIT(4)
/** Bit 5 -  A  - Access bit. Long Mode only. */
#define X86_PDPE_A                          RT_BIT(5)
/** Bits 9-11 - - Available for use to system software. */
#define X86_PDPE_AVL_MASK                   (RT_BIT(9) | RT_BIT(10) | RT_BIT(11))
/** Bits 12-51 - - PAE - Physical Page number of the next level. */
#if 1 /* we're using this internally and have to mask of the top 16-bit. */
#define X86_PDPE_PG_MASK                    ( 0x0000fffffffff000ULL )
/** @todo Get rid of the above hack; makes code unreadable. */
#define X86_PDPE_PG_MASK_FULL               ( 0x000ffffffffff000ULL )
#else
#define X86_PDPE_PG_MASK                    ( 0x000ffffffffff000ULL )
#endif
/** Bits 63 - NX - PAE - No execution flag. Long Mode only. */
#define X86_PDPE_NX                         RT_BIT_64(63)

/**
 * Page directory pointer table entry.
 */
typedef struct X86PDPEBITS
{
    /** Flags whether(=1) or not the page is present. */
    uint32_t    u1Present : 1;
    /** Chunk of reserved bits. */
    uint32_t    u2Reserved : 2;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint32_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint32_t    u1CacheDisable : 1;
    /** Chunk of reserved bits. */
    uint32_t    u4Reserved : 4;
    /** Available for use to system software. */
    uint32_t    u3Available : 3;
    /** Physical Page number of the next level - Low Part. Don't use! */
    uint32_t    u20PageNoLow : 20;
    /** Physical Page number of the next level - High Part. Don't use! */
    uint32_t    u20PageNoHigh : 20;
    /** MBZ bits */
    uint32_t    u12Reserved : 12;
} X86PDPEBITS;
/** Pointer to a page directory pointer table entry. */
typedef X86PDPEBITS *PX86PTPEBITS;
/** Pointer to a const page directory pointer table entry. */
typedef const X86PDPEBITS *PCX86PTPEBITS;

/**
 * Page directory pointer table entry. AMD64 version
 */
typedef struct X86PDPEAMD64BITS
{
    /** Flags whether(=1) or not the page is present. */
    uint32_t    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    uint32_t    u1Write : 1;
    /** User(=1) / Supervisor (=0) flag. */
    uint32_t    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint32_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint32_t    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    uint32_t    u1Accessed : 1;
    /** Chunk of reserved bits. */
    uint32_t    u3Reserved : 3;
    /** Available for use to system software. */
    uint32_t    u3Available : 3;
    /** Physical Page number of the next level - Low Part. Don't use! */
    uint32_t    u20PageNoLow : 20;
    /** Physical Page number of the next level - High Part. Don't use! */
    uint32_t    u20PageNoHigh : 20;
    /** MBZ bits */
    uint32_t    u11Reserved : 11;
    /** No Execute flag. */
    uint32_t    u1NoExecute : 1;
} X86PDPEAMD64BITS;
/** Pointer to a page directory pointer table entry. */
typedef X86PDPEAMD64BITS *PX86PDPEAMD64BITS;
/** Pointer to a const page directory pointer table entry. */
typedef const X86PDPEBITS *PCX86PDPEAMD64BITS;

/**
 * Page directory pointer table entry.
 */
typedef union X86PDPE
{
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** Normal view. */
    X86PDPEBITS     n;
    /** AMD64 view. */
    X86PDPEAMD64BITS lm;
    /** 8 bit unsigned integer view. */
    uint8_t         au8[8];
    /** 16 bit unsigned integer view. */
    uint16_t        au16[4];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} X86PDPE;
/** Pointer to a page directory pointer table entry. */
typedef X86PDPE *PX86PDPE;
/** Pointer to a const page directory pointer table entry. */
typedef const X86PDPE *PCX86PDPE;


/**
 * Page directory pointer table.
 */
typedef struct X86PDPT
{
    /** PDE Array. */
    X86PDPE         a[X86_PG_AMD64_PDPE_ENTRIES];
} X86PDPT;
/** Pointer to a page directory pointer table. */
typedef X86PDPT *PX86PDPT;
/** Pointer to a const page directory pointer table. */
typedef const X86PDPT *PCX86PDPT;

/** The page shift to get the PDPT index. */
#define X86_PDPT_SHIFT             30
/** The PDPT index mask (apply to a shifted page address). (32 bits PAE) */
#define X86_PDPT_MASK_PAE          0x3
/** The PDPT index mask (apply to a shifted page address). (64 bits PAE)*/
#define X86_PDPT_MASK_AMD64        0x1ff

/** @} */


/** @name Page Map Level-4 Entry (Long Mode PAE)
 * @{
 */
/** Bit 0 -  P  - Present bit. */
#define X86_PML4E_P                         RT_BIT(0)
/** Bit 1 - R/W - Read (clear) / Write (set) bit. */
#define X86_PML4E_RW                        RT_BIT(1)
/** Bit 2 - U/S - User (set) / Supervisor (clear) bit. */
#define X86_PML4E_US                        RT_BIT(2)
/** Bit 3 - PWT - Page level write thru bit. */
#define X86_PML4E_PWT                       RT_BIT(3)
/** Bit 4 - PCD - Page level cache disable bit. */
#define X86_PML4E_PCD                       RT_BIT(4)
/** Bit 5 -  A  - Access bit. */
#define X86_PML4E_A                         RT_BIT(5)
/** Bits 9-11 - - Available for use to system software. */
#define X86_PML4E_AVL_MASK                  (RT_BIT(9) | RT_BIT(10) | RT_BIT(11))
/** Bits 12-51 - - PAE - Physical Page number of the next level. */
#if 1 /* we're using this internally and have to mask of the top 16-bit. */
#define X86_PML4E_PG_MASK                   ( 0x0000fffffffff000ULL )
#define X86_PML4E_PG_MASK_FULL              ( 0x000ffffffffff000ULL )
#else
#define X86_PML4E_PG_MASK                   ( 0x000ffffffffff000ULL )
#endif
/** Bits 63 - NX - PAE - No execution flag. */
#define X86_PML4E_NX                        RT_BIT_64(63)

/**
 * Page Map Level-4 Entry
 */
typedef struct X86PML4EBITS
{
    /** Flags whether(=1) or not the page is present. */
    uint32_t    u1Present : 1;
    /** Read(=0) / Write(=1) flag. */
    uint32_t    u1Write : 1;
    /** User(=1) / Supervisor (=0) flag. */
    uint32_t    u1User : 1;
    /** Write Thru flag. If PAT enabled, bit 0 of the index. */
    uint32_t    u1WriteThru : 1;
    /** Cache disabled flag. If PAT enabled, bit 1 of the index. */
    uint32_t    u1CacheDisable : 1;
    /** Accessed flag.
     * Indicates that the page have been read or written to. */
    uint32_t    u1Accessed : 1;
    /** Chunk of reserved bits. */
    uint32_t    u3Reserved : 3;
    /** Available for use to system software. */
    uint32_t    u3Available : 3;
    /** Physical Page number of the next level - Low Part. Don't use! */
    uint32_t    u20PageNoLow : 20;
    /** Physical Page number of the next level - High Part. Don't use! */
    uint32_t    u20PageNoHigh : 20;
    /** MBZ bits */
    uint32_t    u11Reserved : 11;
    /** No Execute flag. */
    uint32_t    u1NoExecute : 1;
} X86PML4EBITS;
/** Pointer to a page map level-4 entry. */
typedef X86PML4EBITS *PX86PML4EBITS;
/** Pointer to a const page map level-4 entry. */
typedef const X86PML4EBITS *PCX86PML4EBITS;

/**
 * Page Map Level-4 Entry.
 */
typedef union X86PML4E
{
    /** Unsigned integer view. */
    X86PGPAEUINT    u;
    /** Normal view. */
    X86PML4EBITS    n;
    /** 8 bit unsigned integer view. */
    uint8_t         au8[8];
    /** 16 bit unsigned integer view. */
    uint16_t        au16[4];
    /** 32 bit unsigned integer view. */
    uint32_t        au32[2];
} X86PML4E;
/** Pointer to a page map level-4 entry. */
typedef X86PML4E *PX86PML4E;
/** Pointer to a const page map level-4 entry. */
typedef const X86PML4E *PCX86PML4E;


/**
 * Page Map Level-4.
 */
typedef struct X86PML4
{
    /** PDE Array. */
    X86PML4E        a[X86_PG_PAE_ENTRIES];
} X86PML4;
/** Pointer to a page map level-4. */
typedef X86PML4 *PX86PML4;
/** Pointer to a const page map level-4. */
typedef const X86PML4 *PCX86PML4;

/** The page shift to get the PML4 index. */
#define X86_PML4_SHIFT              39
/** The PML4 index mask (apply to a shifted page address). */
#define X86_PML4_MASK               0x1ff

/** @} */

/** @} */


/**
 * 80-bit MMX/FPU register type.
 */
typedef struct X86FPUMMX
{
    uint8_t reg[10];
} X86FPUMMX;
/** Pointer to a 80-bit MMX/FPU register type. */
typedef X86FPUMMX *PX86FPUMMX;
/** Pointer to a const 80-bit MMX/FPU register type. */
typedef const X86FPUMMX *PCX86FPUMMX;

/**
 * FPU state (aka FSAVE/FRSTOR Memory Region).
 */
#pragma pack(1)
typedef struct X86FPUSTATE
{
    /** Control word. */
    uint16_t    FCW;
    /** Alignment word */
    uint16_t    Dummy1;
    /** Status word. */
    uint16_t    FSW;
    /** Alignment word */
    uint16_t    Dummy2;
    /** Tag word */
    uint16_t    FTW;
    /** Alignment word */
    uint16_t    Dummy3;

    /** Instruction pointer. */
    uint32_t    FPUIP;
    /** Code selector. */
    uint16_t    CS;
    /** Opcode. */
    uint16_t    FOP;
    /** FOO. */
    uint32_t    FPUOO;
    /** FOS. */
    uint32_t    FPUOS;
    /** FPU view - todo. */
    X86FPUMMX   regs[8];
} X86FPUSTATE;
#pragma pack()
/** Pointer to a FPU state. */
typedef X86FPUSTATE  *PX86FPUSTATE;
/** Pointer to a const FPU state. */
typedef const X86FPUSTATE  *PCX86FPUSTATE;

/**
 * FPU Extended state (aka FXSAVE/FXRSTORE Memory Region).
 */
#pragma pack(1)
typedef struct X86FXSTATE
{
    /** Control word. */
    uint16_t    FCW;
    /** Status word. */
    uint16_t    FSW;
    /** Tag word (it's a byte actually). */
    uint8_t     FTW;
    uint8_t     huh1;
    /** Opcode. */
    uint16_t    FOP;
    /** Instruction pointer. */
    uint32_t    FPUIP;
    /** Code selector. */
    uint16_t    CS;
    uint16_t    Rsvrd1;
    /* - offset 16 - */
    /** Data pointer. */
    uint32_t    FPUDP;
    /** Data segment */
    uint16_t    DS;
    uint16_t    Rsrvd2;
    uint32_t    MXCSR;
    uint32_t    MXCSR_MASK;
    /* - offset 32 - */
    union
    {
        /** MMX view. */
        uint64_t    mmx;
        /** FPU view - todo. */
        X86FPUMMX   fpu;
        /** 8-bit view. */
        uint8_t     au8[16];
        /** 16-bit view. */
        uint16_t    au16[8];
        /** 32-bit view. */
        uint32_t    au32[4];
        /** 64-bit view. */
        uint64_t    au64[2];
        /** 128-bit view. (yeah, very helpful) */
        uint128_t   au128[1];
    } aRegs[8];
    /* - offset 160 - */
    union
    {
        /** XMM Register view *. */
        uint128_t   xmm;
        /** 8-bit view. */
        uint8_t     au8[16];
        /** 16-bit view. */
        uint16_t    au16[8];
        /** 32-bit view. */
        uint32_t    au32[4];
        /** 64-bit view. */
        uint64_t    au64[2];
        /** 128-bit view. (yeah, very helpful) */
        uint128_t   au128[1];
    } aXMM[16]; /* 8 registers in 32 bits mode; 16 in long mode */
    /* - offset 416 - */
    uint32_t    au32RsrvdRest[(512 - 416) / sizeof(uint32_t)];
} X86FXSTATE;
#pragma pack()
/** Pointer to a FPU Extended state. */
typedef X86FXSTATE *PX86FXSTATE;
/** Pointer to a const FPU Extended state. */
typedef const X86FXSTATE *PCX86FXSTATE;


/** @name Selector Descriptor
 * @{
 */

/**
 * Generic descriptor table entry
 */
#pragma pack(1)
typedef struct X86DESCGENERIC
{
    /** Limit - Low word. */
    unsigned    u16LimitLow : 16;
    /** Base address - lowe word.
     * Don't try set this to 24 because MSC is doing studing things then. */
    unsigned    u16BaseLow : 16;
    /** Base address - first 8 bits of high word. */
    unsigned    u8BaseHigh1 : 8;
    /** Segment Type. */
    unsigned    u4Type : 4;
    /** Descriptor Type. System(=0) or code/data selector */
    unsigned    u1DescType : 1;
    /** Descriptor Privelege level. */
    unsigned    u2Dpl : 2;
    /** Flags selector present(=1) or not. */
    unsigned    u1Present : 1;
    /** Segment limit 16-19. */
    unsigned    u4LimitHigh : 4;
    /** Available for system software. */
    unsigned    u1Available : 1;
    /** 32 bits mode: Reserved - 0, long mode: Long Attribute Bit. */
    unsigned    u1Long : 1;
    /** This flags meaning depends on the segment type. Try make sense out
     * of the intel manual yourself.  */
    unsigned    u1DefBig : 1;
    /** Granularity of the limit. If set 4KB granularity is used, if
     * clear byte. */
    unsigned    u1Granularity : 1;
    /** Base address - highest 8 bits. */
    unsigned    u8BaseHigh2 : 8;
} X86DESCGENERIC;
#pragma pack()
/** Pointer to a generic descriptor entry. */
typedef X86DESCGENERIC *PX86DESCGENERIC;
/** Pointer to a const generic descriptor entry. */
typedef const X86DESCGENERIC *PCX86DESCGENERIC;


/**
 * Descriptor attributes.
 */
typedef struct X86DESCATTRBITS
{
    /** Segment Type. */
    unsigned    u4Type : 4;
    /** Descriptor Type. System(=0) or code/data selector */
    unsigned    u1DescType : 1;
    /** Descriptor Privelege level. */
    unsigned    u2Dpl : 2;
    /** Flags selector present(=1) or not. */
    unsigned    u1Present : 1;
    /** Segment limit 16-19. */
    unsigned    u4LimitHigh : 4;
    /** Available for system software. */
    unsigned    u1Available : 1;
    /** 32 bits mode: Reserved - 0, long mode: Long Attribute Bit. */
    unsigned    u1Long : 1;
    /** This flags meaning depends on the segment type. Try make sense out
     * of the intel manual yourself.  */
    unsigned    u1DefBig : 1;
    /** Granularity of the limit. If set 4KB granularity is used, if
     * clear byte. */
    unsigned    u1Granularity : 1;
} X86DESCATTRBITS;


#pragma pack(1)
typedef union X86DESCATTR
{
    /** Unsigned integer view. */
    uint32_t           u;
    /** Normal view. */
    X86DESCATTRBITS    n;
} X86DESCATTR;
#pragma pack()

/** Pointer to descriptor attributes. */
typedef X86DESCATTR *PX86DESCATTR;
/** Pointer to const descriptor attributes. */
typedef const X86DESCATTR *PCX86DESCATTR;


/**
 * Descriptor table entry.
 */
#pragma pack(1)
typedef union X86DESC
{
    /** Generic descriptor view. */
    X86DESCGENERIC  Gen;
#if 0
    /** IDT view. */
    VBOXIDTE        Idt;
#endif

    /** 8 bit unsigned interger view. */
    uint8_t         au8[8];
    /** 16 bit unsigned interger view. */
    uint16_t        au16[4];
    /** 32 bit unsigned interger view. */
    uint32_t        au32[2];
} X86DESC;
#pragma pack()
/** Pointer to descriptor table entry. */
typedef X86DESC *PX86DESC;
/** Pointer to const descriptor table entry. */
typedef const X86DESC *PCX86DESC;


/** @def X86DESC_BASE
 * Return the base address of a descriptor.
 */
#define X86DESC_BASE(desc) \
        (  ((uint32_t)((desc).Gen.u8BaseHigh2) << 24) \
         | (           (desc).Gen.u8BaseHigh1  << 16) \
         | (           (desc).Gen.u16BaseLow        ) )

/** @def X86DESC_LIMIT
 * Return the limit of a descriptor.
 */
#define X86DESC_LIMIT(desc) \
        (  ((uint32_t)((desc).Gen.u4LimitHigh) << 16) \
         | (           (desc).Gen.u16LimitLow       ) )

/**
 * 64 bits generic descriptor table entry
 * Note: most of these bits have no meaning in long mode.
 */
#pragma pack(1)
typedef struct X86DESC64GENERIC
{
    /** Limit - Low word - *IGNORED*. */
    unsigned    u16LimitLow : 16;
    /** Base address - lowe word. - *IGNORED*
     * Don't try set this to 24 because MSC is doing studing things then. */
    unsigned    u16BaseLow : 16;
    /** Base address - first 8 bits of high word. - *IGNORED* */
    unsigned    u8BaseHigh1 : 8;
    /** Segment Type. */
    unsigned    u4Type : 4;
    /** Descriptor Type. System(=0) or code/data selector */
    unsigned    u1DescType : 1;
    /** Descriptor Privelege level. */
    unsigned    u2Dpl : 2;
    /** Flags selector present(=1) or not. */
    unsigned    u1Present : 1;
    /** Segment limit 16-19. - *IGNORED* */
    unsigned    u4LimitHigh : 4;
    /** Available for system software. - *IGNORED* */
    unsigned    u1Available : 1;
    /** Long mode flag. */
    unsigned    u1Long : 1;
    /** This flags meaning depends on the segment type. Try make sense out
     * of the intel manual yourself.  */
    unsigned    u1DefBig : 1;
    /** Granularity of the limit. If set 4KB granularity is used, if
     * clear byte. - *IGNORED* */
    unsigned    u1Granularity : 1;
    /** Base address - highest 8 bits. - *IGNORED* */
    unsigned    u8BaseHigh2 : 8;
    /** Base address - bits 63-32. */
    unsigned    u32BaseHigh3    : 32;
    unsigned    u8Reserved      : 8;
    unsigned    u5Zeros         : 5;
    unsigned    u19Reserved     : 19;
} X86DESC64GENERIC;
#pragma pack()
/** Pointer to a generic descriptor entry. */
typedef X86DESC64GENERIC *PX86DESC64GENERIC;
/** Pointer to a const generic descriptor entry. */
typedef const X86DESC64GENERIC *PCX86DESC64GENERIC;

/**
 * System descriptor table entry (64 bits)
 */
#pragma pack(1)
typedef struct X86DESC64SYSTEM
{
    /** Limit - Low word. */
    unsigned    u16LimitLow     : 16;
    /** Base address - lowe word.
     * Don't try set this to 24 because MSC is doing studing things then. */
    unsigned    u16BaseLow      : 16;
    /** Base address - first 8 bits of high word. */
    unsigned    u8BaseHigh1     : 8;
    /** Segment Type. */
    unsigned    u4Type          : 4;
    /** Descriptor Type. System(=0) or code/data selector */
    unsigned    u1DescType      : 1;
    /** Descriptor Privelege level. */
    unsigned    u2Dpl           : 2;
    /** Flags selector present(=1) or not. */
    unsigned    u1Present       : 1;
    /** Segment limit 16-19. */
    unsigned    u4LimitHigh     : 4;
    /** Available for system software. */
    unsigned    u1Available     : 1;
    /** Reserved - 0. */
    unsigned    u1Reserved      : 1;
    /** This flags meaning depends on the segment type. Try make sense out
     * of the intel manual yourself.  */
    unsigned    u1DefBig        : 1;
    /** Granularity of the limit. If set 4KB granularity is used, if
     * clear byte. */
    unsigned    u1Granularity   : 1;
    /** Base address - bits 31-24. */
    unsigned    u8BaseHigh2     : 8;
    /** Base address - bits 63-32. */
    unsigned    u32BaseHigh3    : 32;
    unsigned    u8Reserved      : 8;
    unsigned    u5Zeros         : 5;
    unsigned    u19Reserved     : 19;
} X86DESC64SYSTEM;
#pragma pack()
/** Pointer to a generic descriptor entry. */
typedef X86DESC64SYSTEM *PX86DESC64SYSTEM;
/** Pointer to a const generic descriptor entry. */
typedef const X86DESC64SYSTEM *PCX86DESC64SYSTEM;


/**
 * Descriptor table entry.
 */
#pragma pack(1)
typedef union X86DESC64
{
    /** Generic descriptor view. */
    X86DESC64GENERIC    Gen;
    /** System descriptor view. */
    X86DESC64SYSTEM     System;
#if 0
    X86DESC64GATE       Gate;
#endif

    /** 8 bit unsigned interger view. */
    uint8_t             au8[16];
    /** 16 bit unsigned interger view. */
    uint16_t            au16[8];
    /** 32 bit unsigned interger view. */
    uint32_t            au32[4];
    /** 64 bit unsigned interger view. */
    uint64_t            au64[2];
} X86DESC64;
#pragma pack()
/** Pointer to descriptor table entry. */
typedef X86DESC64 *PX86DESC64;
/** Pointer to const descriptor table entry. */
typedef const X86DESC64 *PCX86DESC64;

#if HC_ARCH_BITS == 64
typedef X86DESC64   X86DESCHC;
typedef X86DESC64   *PX86DESCHC;
#else
typedef X86DESC     X86DESCHC;
typedef X86DESC     *PX86DESCHC;
#endif

/** @def X86DESC_LIMIT
 * Return the base of a 64-bit descriptor.
 */
#define X86DESC64_BASE(desc) \
        (  ((uint64_t)((desc).Gen.u32BaseHigh3) << 32) \
         | ((uint32_t)((desc).Gen.u8BaseHigh2)  << 24) \
         | (           (desc).Gen.u8BaseHigh1   << 16) \
         | (           (desc).Gen.u16BaseLow         ) )


/** @name Selector Descriptor Types.
 * @{
 */

/** @name Non-System Selector Types.
 * @{ */
/** Code(=set)/Data(=clear) bit. */
#define X86_SEL_TYPE_CODE                   8
/** Memory(=set)/System(=clear) bit. */
#define X86_SEL_TYPE_MEMORY                 RT_BIT(4)
/** Accessed bit. */
#define X86_SEL_TYPE_ACCESSED               1
/** Expand down bit (for data selectors only). */
#define X86_SEL_TYPE_DOWN                   4
/** Conforming bit (for code selectors only). */
#define X86_SEL_TYPE_CONF                   4
/** Write bit (for data selectors only). */
#define X86_SEL_TYPE_WRITE                  2
/** Read bit (for code selectors only). */
#define X86_SEL_TYPE_READ                   2

/** Read only selector type. */
#define X86_SEL_TYPE_RO                     0
/** Accessed read only selector type. */
#define X86_SEL_TYPE_RO_ACC                (0 | X86_SEL_TYPE_ACCESSED)
/** Read write selector type. */
#define X86_SEL_TYPE_RW                     2
/** Accessed read write selector type. */
#define X86_SEL_TYPE_RW_ACC                (2 | X86_SEL_TYPE_ACCESSED)
/** Expand down read only selector type. */
#define X86_SEL_TYPE_RO_DOWN                4
/** Accessed expand down read only selector type. */
#define X86_SEL_TYPE_RO_DOWN_ACC           (4 | X86_SEL_TYPE_ACCESSED)
/** Expand down read write selector type. */
#define X86_SEL_TYPE_RW_DOWN                6
/** Accessed expand down read write selector type. */
#define X86_SEL_TYPE_RW_DOWN_ACC           (6 | X86_SEL_TYPE_ACCESSED)
/** Execute only selector type. */
#define X86_SEL_TYPE_EO                    (0 | X86_SEL_TYPE_CODE)
/** Accessed execute only selector type. */
#define X86_SEL_TYPE_EO_ACC                (0 | X86_SEL_TYPE_CODE | X86_SEL_TYPE_ACCESSED)
/** Execute and read selector type. */
#define X86_SEL_TYPE_ER                    (2 | X86_SEL_TYPE_CODE)
/** Accessed execute and read selector type. */
#define X86_SEL_TYPE_ER_ACC                (2 | X86_SEL_TYPE_CODE | X86_SEL_TYPE_ACCESSED)
/** Conforming execute only selector type. */
#define X86_SEL_TYPE_EO_CONF               (4 | X86_SEL_TYPE_CODE)
/** Accessed Conforming execute only selector type. */
#define X86_SEL_TYPE_EO_CONF_ACC           (4 | X86_SEL_TYPE_CODE | X86_SEL_TYPE_ACCESSED)
/** Conforming execute and write selector type. */
#define X86_SEL_TYPE_ER_CONF               (6 | X86_SEL_TYPE_CODE)
/** Accessed Conforming execute and write selector type. */
#define X86_SEL_TYPE_ER_CONF_ACC           (6 | X86_SEL_TYPE_CODE | X86_SEL_TYPE_ACCESSED)
/** @} */


/** @name System Selector Types.
 * @{ */
/** Undefined system selector type. */
#define X86_SEL_TYPE_SYS_UNDEFINED          0
/** 286 TSS selector. */
#define X86_SEL_TYPE_SYS_286_TSS_AVAIL      1
/** LDT selector. */
#define X86_SEL_TYPE_SYS_LDT                2
/** 286 TSS selector - Busy. */
#define X86_SEL_TYPE_SYS_286_TSS_BUSY       3
/** 286 Callgate selector. */
#define X86_SEL_TYPE_SYS_286_CALL_GATE      4
/** Taskgate selector. */
#define X86_SEL_TYPE_SYS_TASK_GATE          5
/** 286 Interrupt gate selector. */
#define X86_SEL_TYPE_SYS_286_INT_GATE       6
/** 286 Trapgate selector. */
#define X86_SEL_TYPE_SYS_286_TRAP_GATE      7
/** Undefined system selector. */
#define X86_SEL_TYPE_SYS_UNDEFINED2         8
/** 386 TSS selector. */
#define X86_SEL_TYPE_SYS_386_TSS_AVAIL      9
/** Undefined system selector. */
#define X86_SEL_TYPE_SYS_UNDEFINED3         0xA
/** 386 TSS selector - Busy. */
#define X86_SEL_TYPE_SYS_386_TSS_BUSY       0xB
/** 386 Callgate selector. */
#define X86_SEL_TYPE_SYS_386_CALL_GATE      0xC
/** Undefined system selector. */
#define X86_SEL_TYPE_SYS_UNDEFINED4         0xD
/** 386 Interruptgate selector. */
#define X86_SEL_TYPE_SYS_386_INT_GATE       0xE
/** 386 Trapgate selector. */
#define X86_SEL_TYPE_SYS_386_TRAP_GATE      0xF
/** @} */

/** @name AMD64 System Selector Types.
 * @{ */
#define AMD64_SEL_TYPE_SYS_LDT              2
/** 286 TSS selector - Busy. */
#define AMD64_SEL_TYPE_SYS_TSS_AVAIL        9
/** 386 TSS selector - Busy. */
#define AMD64_SEL_TYPE_SYS_TSS_BUSY         0xB
/** 386 Callgate selector. */
#define AMD64_SEL_TYPE_SYS_CALL_GATE        0xC
/** 386 Interruptgate selector. */
#define AMD64_SEL_TYPE_SYS_INT_GATE         0xE
/** 386 Trapgate selector. */
#define AMD64_SEL_TYPE_SYS_TRAP_GATE        0xF
/** @} */

/** @} */


/** @name Descriptor Table Entry Flag Masks.
 * These are for the 2nd 32-bit word of a descriptor.
 * @{ */
/** Bits 8-11 - TYPE - Descriptor type mask. */
#define X86_DESC_TYPE_MASK                  (RT_BIT(8) | RT_BIT(9) | RT_BIT(10) | RT_BIT(11))
/** Bit 12 - S - System (=0) or Code/Data (=1). */
#define X86_DESC_S                          RT_BIT(12)
/** Bits 13-14 - DPL - Descriptor Privilege Level. */
#define X86_DESC_DPL                       (RT_BIT(13) | RT_BIT(14))
/** Bit 15 - P - Present. */
#define X86_DESC_P                          RT_BIT(15)
/** Bit 20 - AVL - Available for system software. */
#define X86_DESC_AVL                        RT_BIT(20)
/** Bit 22 - DB - Default operation size. 0 = 16 bit, 1 = 32 bit. */
#define X86_DESC_DB                         RT_BIT(22)
/** Bit 23 - G - Granularity of the limit. If set 4KB granularity is
 * used, if clear byte. */
#define X86_DESC_G                          RT_BIT(23)
/** @} */

/** @} */

/** @name Task segment.
 * @{
 */
#pragma pack(1)
typedef struct X86TSS32
{
    /** Back link to previous task. (static) */
    RTSEL       selPrev;
    uint16_t    padding1;
    /** Ring-0 stack pointer. (static) */
    uint32_t    esp0;
    /** Ring-0 stack segment. (static) */
    RTSEL       ss0;
    uint16_t    padding_ss0;
    /** Ring-1 stack pointer. (static) */
    uint32_t    esp1;
    /** Ring-1 stack segment. (static) */
    RTSEL       ss1;
    uint16_t    padding_ss1;
    /** Ring-2 stack pointer. (static) */
    uint32_t    esp2;
    /** Ring-2 stack segment. (static) */
    RTSEL       ss2;
    uint16_t    padding_ss2;
    /** Page directory for the task. (static) */
    uint32_t    cr3;
    /** EIP before task switch. */
    uint32_t    eip;
    /** EFLAGS before task switch. */
    uint32_t    eflags;
    /** EAX before task switch. */
    uint32_t    eax;
    /** ECX before task switch. */
    uint32_t    ecx;
    /** EDX before task switch. */
    uint32_t    edx;
    /** EBX before task switch. */
    uint32_t    ebx;
    /** ESP before task switch. */
    uint32_t    esp;
    /** EBP before task switch. */
    uint32_t    ebp;
    /** ESI before task switch. */
    uint32_t    esi;
    /** EDI before task switch. */
    uint32_t    edi;
    /** ES before task switch. */
    RTSEL       es;
    uint16_t    padding_es;
    /** CS before task switch. */
    RTSEL       cs;
    uint16_t    padding_cs;
    /** SS before task switch. */
    RTSEL       ss;
    uint16_t    padding_ss;
    /** DS before task switch. */
    RTSEL       ds;
    uint16_t    padding_ds;
    /** FS before task switch. */
    RTSEL       fs;
    uint16_t    padding_fs;
    /** GS before task switch. */
    RTSEL       gs;
    uint16_t    padding_gs;
    /** LDTR before task switch. */
    RTSEL       selLdt;
    uint16_t    padding_ldt;
    /** Debug trap flag */
    uint16_t    fDebugTrap;
    /** Offset relative to the TSS of the start of the I/O Bitmap
     * and the end of the interrupt redirection bitmap. */
    uint16_t    offIoBitmap;
    /** 32 bytes for the virtual interrupt redirection bitmap. (VME) */
    uint8_t     IntRedirBitmap[32];
} X86TSS32;
#pragma pack()
/** Pointer to task segment. */
typedef X86TSS32 *PX86TSS32;
/** Pointer to const task segment. */
typedef const X86TSS32 *PCX86TSS32;
/** @} */


/** @name 64 bits Task segment.
 * @{
 */
#pragma pack(1)
typedef struct X86TSS64
{
    /** Reserved. */
    uint32_t    u32Reserved;
    /** Ring-0 stack pointer. (static) */
    uint64_t    rsp0;
    /** Ring-1 stack pointer. (static) */
    uint64_t    rsp1;
    /** Ring-2 stack pointer. (static) */
    uint64_t    rsp2;
    /** Reserved. */
    uint32_t    u32Reserved2[2];
    /* IST */
    uint64_t    ist1;
    uint64_t    ist2;
    uint64_t    ist3;
    uint64_t    ist4;
    uint64_t    ist5;
    uint64_t    ist6;
    uint64_t    ist7;
    /* Reserved. */
    uint16_t    u16Reserved[5];
    /** Offset relative to the TSS of the start of the I/O Bitmap
     * and the end of the interrupt redirection bitmap. */
    uint16_t    offIoBitmap;
    /** 32 bytes for the virtual interrupt redirection bitmap. (VME) */
    uint8_t     IntRedirBitmap[32];
} X86TSS64;
#pragma pack()
/** Pointer to task segment. */
typedef X86TSS64 *PX86TSS64;
/** Pointer to const task segment. */
typedef const X86TSS64 *PCX86TSS64;
AssertCompileSize(X86TSS64, 136);

/** @} */


/** @name Selectors.
 * @{
 */

/**
 * The shift used to convert a selector from and to index an index (C).
 */
#define X86_SEL_SHIFT       3

/**
 * The shift used to convert a selector from and to index an index (C).
 */
#define AMD64_SEL_SHIFT     4

/** @def X86_SEL_SHIFT_HC
 * This is for use with X86DESCHC. */
#if HC_ARCH_BITS == 64
#define X86_SEL_SHIFT_HC    AMD64_SEL_SHIFT
#else
#define X86_SEL_SHIFT_HC    X86_SEL_SHIFT
#endif

/**
 * The mask used to mask off the table indicator and CPL of an selector.
 */
#define X86_SEL_MASK        0xfff8

/**
 * The bit indicating that a selector is in the LDT and not in the GDT.
 */
#define X86_SEL_LDT         0x0004
/**
 * The bit mask for getting the RPL of a selector.
 */
#define X86_SEL_RPL         0x0003

/** @} */


/**
 * x86 Exceptions/Faults/Traps.
 */
typedef enum X86XCPT
{
    /** \#DE - Divide error. */
    X86_XCPT_DE = 0x00,
    /** \#DB - Debug event (single step, DRx, ..) */
    X86_XCPT_DB = 0x01,
    /** NMI - Non-Maskable Interrupt */
    X86_XCPT_NMI = 0x02,
    /** \#BP - Breakpoint (INT3). */
    X86_XCPT_BP = 0x03,
    /** \#OF - Overflow (INTO). */
    X86_XCPT_OF = 0x04,
    /** \#BR - Bound range exceeded (BOUND). */
    X86_XCPT_BR = 0x05,
    /** \#UD - Undefined opcode. */
    X86_XCPT_UD = 0x06,
    /** \#NM - Device not available (math coprocessor device). */
    X86_XCPT_NM = 0x07,
    /** \#DF - Double fault. */
    X86_XCPT_DF = 0x08,
    /** ??? - Coprocessor segment overrun (obsolete). */
    X86_XCPT_CO_SEG_OVERRUN = 0x09,
    /** \#TS - Taskswitch (TSS). */
    X86_XCPT_TS = 0x0a,
    /** \#NP - Segment no present. */
    X86_XCPT_NP = 0x0b,
    /** \#SS - Stack segment fault. */
    X86_XCPT_SS = 0x0c,
    /** \#GP - General protection fault. */
    X86_XCPT_GP = 0x0d,
    /** \#PF - Page fault. */
    X86_XCPT_PF = 0x0e,
    /* 0x0f is reserved. */
    /** \#MF - Math fault (FPU). */
    X86_XCPT_MF = 0x10,
    /** \#AC - Alignment check. */
    X86_XCPT_AC = 0x11,
    /** \#MC - Machine check. */
    X86_XCPT_MC = 0x12,
    /** \#XF - SIMD Floating-Pointer Exception. */
    X86_XCPT_XF = 0x13
} X86XCPT;
/** Pointer to a x86 exception code. */
typedef X86XCPT *PX86XCPT;
/** Pointer to a const x86 exception code. */
typedef const X86XCPT *PCX86XCPT;


/** @name Trap Error Codes
 * @{
 */
/** External indicator. */
#define X86_TRAP_ERR_EXTERNAL       1
/** IDT indicator. */
#define X86_TRAP_ERR_IDT            2
/** Descriptor table indicator - If set LDT, if clear GDT. */
#define X86_TRAP_ERR_TI             4
/** Mask for getting the selector. */
#define X86_TRAP_ERR_SEL_MASK       0xfff8
/** Shift for getting the selector table index (C type index). */
#define X86_TRAP_ERR_SEL_SHIFT      3
/** @} */


/** @name \#PF Trap Error Codes
 * @{
 */
/** Bit 0 -   P - Not present (clear) or page level protection (set) fault. */
#define X86_TRAP_PF_P               RT_BIT(0)
/** Bit 1 - R/W - Read (clear) or write (set) access. */
#define X86_TRAP_PF_RW              RT_BIT(1)
/** Bit 2 - U/S - CPU executing in user mode (set) or supervisor mode (clear). */
#define X86_TRAP_PF_US              RT_BIT(2)
/** Bit 3 - RSVD- Reserved bit violation (set), i.e. reserved bit was set to 1. */
#define X86_TRAP_PF_RSVD            RT_BIT(3)
/** Bit 4 - I/D - Instruction fetch (set) / Data access (clear) - PAE + NXE. */
#define X86_TRAP_PF_ID              RT_BIT(4)
/** @} */

#pragma pack(1)
/**
 * 32-bit IDTR/GDTR.
 */
typedef struct X86XDTR32
{
    /** Size of the descriptor table. */
    uint16_t    cb;
    /** Address of the descriptor table. */
    uint32_t    uAddr;
} X86XDTR32, *PX86XDTR32;
#pragma pack()

#pragma pack(1)
/**
 * 64-bit IDTR/GDTR.
 */
typedef struct X86XDTR64
{
    /** Size of the descriptor table. */
    uint16_t    cb;
    /** Address of the descriptor table. */
    uint64_t    uAddr;
} X86XDTR64, *PX86XDTR64;
#pragma pack()

/** @} */

#endif

